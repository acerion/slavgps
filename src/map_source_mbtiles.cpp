/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2005, Evan Battaglia <viking@greentorch.org>
 * Copyright (C) 2010, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (c) 2013, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2018, Kamil Ignacak <acerion@wp.pl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */




#include <QDebug>
#include <QDir>




#include <glib.h>
#include <glib/gstdio.h>




#include "map_source_mbtiles.h"
#include "globals.h"
#include "dialog.h"




#define SG_MODULE "MBTiles Map Source"




using namespace SlavGPS;




/*
  https://www.gdal.org/frmt_mbtiles.html
  https://wiki.openstreetmap.org/wiki/MBTiles
*/




/* No cache needed for this type. */
/* This map source uses an SQL MBTiles File for the tileset. For now the MBTiles file is read locally (from disc) only. */
/* http://github.com/mapbox/mbtiles-spec */
MapSourceMBTiles::MapSourceMBTiles() : MapSourceSlippy(MapTypeID::MBTiles, QObject::tr("MBTiles File"), NULL, NULL)
{
	/* For using your own generated data assumed you know the license already! */
	this->set_copyright("© OpenStreetMap contributors"); // probably
	this->is_direct_file_access_flag = true;
}




MapSourceMBTiles::~MapSourceMBTiles()
{
}




QPixmap MapSourceMBTiles::get_tile_pixmap(const MapSourceArgs & args)
{
	QPixmap result;

#ifdef HAVE_SQLITE3_H
	if (args.sqlite_handle) {
		/*
		  char *statement = g_strdup_printf("SELECT name FROM sqlite_master WHERE type='table';");
		  char *errMsg = NULL;
		  int ans = sqlite3_exec(this->sqlite_handle, statement, sql_select_tile_dump_cb, pixmap, &errMsg);
		  if (ans != SQLITE_OK) {
		  // Only to console for information purposes only
		  qDebug() << SG_PREFIX_W << "SQL problem:" << ans << "for" << statement << "- error:" << errMsg;
		  sqlite3_free(errMsg);
		  }
		  free(statement);
		*/

		/* Reading BLOBS is a bit more involved and so can't use the simpler sqlite3_exec().
		   Hence this specific function. */
		result = create_pixmap_sql_exec(*args.sqlite_handle, args.x, args.y, args.zoom);
	}
#endif

	qDebug() << SG_PREFIX_I << "Creating pixmap from mbtiles:" << (result.isNull() ? "failure" : "success");

	return result;
}





#ifdef HAVE_SQLITE3_H
static int sql_select_tile_dump_cb(void * data, int cols, char ** fields, char ** col_names)
{
	qDebug() << SG_PREFIX_D << "Found" << cols << "columns";
	for (int i = 0; i < cols; i++) {
		qDebug() << SG_PREFIX_D << "SQL processing" << col_names[i] << "=" << fields[i];
	}
	return 0;
}




QPixmap MapSourceMBTiles::create_pixmap_sql_exec(sqlite3 * sqlite_handle, int xx, int yy, int zoom) const
{
	QPixmap pixmap;

	/* MBTiles stored internally with the flipping y thingy (i.e. TMS scheme). */
	int flip_y = (int) pow(2, zoom)-1 - yy;
	char * statement = g_strdup_printf("SELECT tile_data FROM tiles WHERE zoom_level=%d AND tile_column=%d AND tile_row=%d;", zoom, xx, flip_y);

	bool finished = false;

	sqlite3_stmt *sql_stmt = NULL;
	int ans = sqlite3_prepare_v2(sqlite_handle, statement, -1, &sql_stmt, NULL);
	if (ans != SQLITE_OK) {
		qDebug() << SG_PREFIX_W << "prepare() failure -" << ans << "-" << statement;
		finished = true;
	}

	while (!finished) {
		ans = sqlite3_step(sql_stmt);
		switch (ans) {
		case SQLITE_ROW: {
			/* Get tile_data blob. */
			int count = sqlite3_column_count(sql_stmt);
			if (count != 1)  {
				qDebug() << SG_PREFIX_W << "Count not one -" << count;
				finished = true;
			} else {
				const void *data = sqlite3_column_blob(sql_stmt, 0);
				int bytes = sqlite3_column_bytes(sql_stmt, 0);
				if (bytes < 1)  {
					qDebug() << SG_PREFIX_W << "Not enough bytes:" << bytes;
					finished = true;
				} else {
					if (!pixmap.loadFromData((const unsigned char *) data, (unsigned int) bytes)) {
						qDebug() << SG_PREFIX_E << "Failed to load pixmap from sql";
					}
				}
			}
			break;
		}
		default:
			/* e.g. SQLITE_DONE | SQLITE_ERROR | SQLITE_MISUSE | etc...
			   Finished normally and give up on any errors. */
			if (ans != SQLITE_DONE) {
				qDebug() << SG_PREFIX_W << "Step issue" << ans;
			}
			finished = true;
			break;
		}
	}
	(void)sqlite3_finalize(sql_stmt);

	free(statement);

	return pixmap;
}
#endif




QStringList MapSourceMBTiles::get_tile_info(const MapSourceArgs & args) const
{
#ifdef HAVE_SQLITE3_H

	QPixmap pixmap;
	if (args.sqlite_handle) {
		pixmap = this->create_pixmap_sql_exec(*args.sqlite_handle, args.x, args.y, args.zoom);
	}
	QString exists = pixmap.isNull() ? QObject::tr("NO") : QObject::tr("YES");


	const int flip_y = (int) pow(2, args.zoom) - 1 - args.y;
	/* NB Also handles .jpg automatically due to pixmap_new_from() support - although just print png for now. */
	QString source = QObject::tr("Source: %1 (%2%3%4%5%6.%7 %8)")
		.arg(args.file_full_path)
		.arg(args.zoom)
		.arg(QDir::separator())
		.arg(args.x)
		.arg(QDir::separator())
		.arg(flip_y)
		.arg("png")
		.arg(exists);
#else
	QString source = QObject::tr("Source: Not available");
#endif

	QStringList result;
	result << source;

	return result;
}




void MapSourceMBTiles::close_map_source(MapSourceArgs & args)
{
	if (args.sqlite_handle) {
		const int ans = sqlite3_close(*args.sqlite_handle);
		if (ans != SQLITE_OK) {
			/* Only to console for information purposes only. */
			qDebug() << SG_PREFIX_W << "SQL Close problem:" << ans;
		}
	}
}




void MapSourceMBTiles::post_read(MapSourceArgs & args)
{
	const int ans = sqlite3_open_v2(args.file_full_path.toUtf8().constData(),
				  args.sqlite_handle,
				  SQLITE_OPEN_READONLY,
				  NULL);
	if (ans != SQLITE_OK) {
		/* That didn't work, so here's why: */
		qDebug() << SG_PREFIX_W << sqlite3_errmsg(*args.sqlite_handle);

		Dialog::error(QObject::tr("Failed to open MBTiles file: %1").arg(args.file_full_path), args.parent_window);
		*args.sqlite_handle = NULL;
	}
}
