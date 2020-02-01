/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2005, Evan Battaglia <viking@greentorch.org>
 * Copyright (C) 2010, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (c) 2013, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2020, Kamil Ignacak <acerion@wp.pl>
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




#include "map_source_mbtiles.h"
#include "globals.h"
#include "dialog.h"
#include "map_utils.h"




#define SG_MODULE "MBTiles Map Source"




using namespace SlavGPS;




/*
  No cache needed for this type.

  This map source uses an SQL MBTiles File for the tileset. For now
  the MBTiles file is read locally (from disc) only.

  https://www.gdal.org/frmt_mbtiles.html
  https://wiki.openstreetmap.org/wiki/MBTiles
  http://github.com/mapbox/mbtiles-spec
*/




MapSourceMBTiles::MapSourceMBTiles() : MapSourceSlippy(MapTypeID::MBTiles, QObject::tr("MBTiles File"), nullptr, nullptr)
{
	/* TODO_LATER: can we read license from file? This license
	   string is invalid for user-generated and user-owned
	   tiles. */
	this->set_copyright("© OpenStreetMap contributors");
	this->is_direct_file_access_flag = true;
}




MapSourceMBTiles::~MapSourceMBTiles()
{
	this->close_map_source();
}




QPixmap MapSourceMBTiles::get_tile_pixmap(__attribute__((unused)) const MapCacheObj & map_cache_obj, const TileInfo & tile_info, const MapSourceArgs & args) const
{
	QPixmap result;

#ifdef HAVE_SQLITE3_H
	if (this->sqlite_handle) {

#if 0
		const QString statement = QString("SELECT name FROM sqlite_master WHERE type='table';");
		char *errMsg = nullptr;
		int ans = sqlite3_exec(this->sqlite_handle, statement.toUtf8().constData(), sql_select_tile_dump_cb, pixmap, &errMsg);
		if (ans != SQLITE_OK) {
			// Only to console for information purposes only
			qDebug() << SG_PREFIX_W << "SQL problem:" << ans << "for" << statement << "- error:" << errMsg;
			sqlite3_free(errMsg);
		}
#endif

		/* Reading BLOBS is a bit more involved and so can't use the simpler sqlite3_exec().
		   Hence this specific function. */
		result = create_pixmap_sql_exec(tile_info);
	}
#endif

	qDebug() << SG_PREFIX_I << "Creating pixmap from mbtiles:" << (result.isNull() ? "failure" : "success");

	return result;
}





#ifdef HAVE_SQLITE3_H
static int sql_select_tile_dump_cb(__attribute__((unused)) void * data, int cols, char ** fields, char ** col_names)
{
	qDebug() << SG_PREFIX_D << "Found" << cols << "columns";
	for (int i = 0; i < cols; i++) {
		qDebug() << SG_PREFIX_D << "SQL processing" << col_names[i] << "=" << fields[i];
	}
	return 0;
}




QPixmap MapSourceMBTiles::create_pixmap_sql_exec(const TileInfo & tile_info) const
{
	const int xx = tile_info.x;
	const int yy = tile_info.y;
	const int tile_zoom_level = tile_info.get_tile_zoom_level(); /* This is OSM MBTile, so use method that returns OSM-like zoom level. */

	QPixmap pixmap;

	/* MBTiles stored internally with the flipping y thingy (i.e. TMS scheme). */
	const int flip_y = (int) pow(2, tile_zoom_level) - 1 - yy;
	const QString statement = QString("SELECT tile_data FROM tiles WHERE zoom_level=%1 AND tile_column=%2 AND tile_row=%3;").arg(tile_zoom_level).arg(xx).arg(flip_y);
	qDebug() << SG_PREFIX_I << "Statement =" << statement;

	bool finished = false;

	sqlite3_stmt *sql_stmt = nullptr;
	int ans = sqlite3_prepare_v2(this->sqlite_handle, statement.toUtf8().constData(), -1, &sql_stmt, nullptr);
	if (ans != SQLITE_OK) {
		qDebug() << SG_PREFIX_W << "prepare() failure -" << ans << "-" << statement << sqlite3_errmsg(this->sqlite_handle);
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
	sqlite3_finalize(sql_stmt); /* TODO_LATER: check returned value? */

	return pixmap;
}
#endif




QStringList MapSourceMBTiles::get_tile_description(__attribute__((unused)) const MapCacheObj & map_cache_obj, const TileInfo & tile_info, const MapSourceArgs & args) const
{
#ifdef HAVE_SQLITE3_H

	QPixmap pixmap;
	if (this->sqlite_handle) {
		pixmap = this->create_pixmap_sql_exec(tile_info);
	}
	const QString exists = pixmap.isNull() ? QObject::tr("Doesn't exist") : QObject::tr("Exists");


	const int tile_zoom_level = tile_info.scale.get_tile_zoom_level();
	const int flip_y = (int) pow(2, tile_zoom_level) - 1 - tile_info.y; /* TODO_LATER: wrap this in function. This calculation appears twice (or more) in this file. */
	/* NB Also handles .jpg automatically due to pixmap_new_from() support - although just print png for now. */
	QString source = QObject::tr("Source: %1 (%2%3%4%5%6.%7 %8)")
		.arg(args.tile_file_full_path)
		.arg(tile_zoom_level)
		.arg(QDir::separator())
		.arg(tile_info.x)
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




sg_ret MapSourceMBTiles::open_map_source(const MapSourceArgs & args, QString & error_message)
{
	const int ans = sqlite3_open_v2(args.tile_file_full_path.toUtf8().constData(),
					&this->sqlite_handle,
					SQLITE_OPEN_READONLY,
					nullptr);
	if (ans == SQLITE_OK) {
		return sg_ret::ok;
	} else {
		const QString sqlite_error_string = sqlite3_errmsg(this->sqlite_handle);
		qDebug() << SG_PREFIX_E << "Can't open sqlite data source:" << sqlite_error_string;

		error_message = QObject::tr("Failed to open MBTiles file.\n"
					    "Path: %1\n"
					    "Error: %2").arg(args.tile_file_full_path).arg(sqlite_error_string);

		this->sqlite_handle = nullptr;
		return sg_ret::err;
	}
}




sg_ret MapSourceMBTiles::close_map_source(void)
{
	if (this->sqlite_handle) {
		const int ans = sqlite3_close(this->sqlite_handle); /* TODO_LATER: other sqlite functions in this file have v2 postfix, but not this one. Is this ok? */
		if (ans != SQLITE_OK) {
			/* Only to console for information purposes only. */
			qDebug() << SG_PREFIX_E << "Failed to properly close map source:" << ans << sqlite3_errmsg(this->sqlite_handle);
			return sg_ret::err;
		}
	}

	return sg_ret::ok;
}
