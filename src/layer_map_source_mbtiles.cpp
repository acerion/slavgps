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




#include "dialog.h"
#include "globals.h"
#include "layer_map_source_mbtiles.h"
#include "map_utils.h"




#define SG_MODULE "MBTiles Map Source"




using namespace SlavGPS;




static void get_mbtiles_z_x_y(const TileInfo & tile_info, int & z, int & x, int & y);




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




QPixmap MapSourceMBTiles::create_tile_pixmap(__attribute__((unused)) const MapCacheObj & map_cache_obj, const TileInfo & tile_info) const
{
	QPixmap result;

#ifdef HAVE_SQLITE3_H
	if (this->sqlite_handle) {
		result = this->create_pixmap_sql_exec(tile_info);
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
	/* Reading BLOBS is a bit more involved and so can't use the
	   simpler sqlite3_exec().  Hence this specific function. */

	QPixmap pixmap;

	if (nullptr == this->sqlite_handle) {
		qDebug() << SG_PREFIX_E << "Called the function for NULL handle";
		return pixmap;
	}

	int z, x, y;
	get_mbtiles_z_x_y(tile_info, z, x, y);

	const QString statement = QString("SELECT tile_data FROM tiles WHERE zoom_level=%1 AND tile_column=%2 AND tile_row=%3;").arg(z).arg(x).arg(y);
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
	sqlite3_finalize(sql_stmt);

	return pixmap;
}
#endif




QStringList MapSourceMBTiles::get_tile_description(__attribute__((unused)) const MapCacheObj & map_cache_obj, const TileInfo & tile_info) const
{
#ifdef HAVE_SQLITE3_H

	QPixmap pixmap;
	if (this->sqlite_handle) {
		pixmap = this->create_pixmap_sql_exec(tile_info);
	}
	const QString exists = pixmap.isNull() ? QObject::tr("Doesn't exist") : QObject::tr("Exists");

	int z, x, y;
	get_mbtiles_z_x_y(tile_info, z, x, y);

	QString source = QObject::tr("Source: %1 (%2%3%4%5%6.%7 %8)")
		.arg(this->mbtiles_file_full_path)
		.arg(z)
		.arg(QDir::separator())
		.arg(x)
		.arg(QDir::separator())
		.arg(y)
		.arg("png") /* TODO_LATER: hardcoded image extension! */
		.arg(exists);
#else
	QString source = QObject::tr("Source: Not available");
#endif

	QStringList result;
	result << source;

	return result;
}




sg_ret MapSourceMBTiles::open_map_source(const MapSourceParameters & source_params, QString & error_message)
{
	const int ans = sqlite3_open_v2(source_params.full_path.toUtf8().constData(),
					&this->sqlite_handle,
					SQLITE_OPEN_READONLY,
					nullptr);
	if (ans == SQLITE_OK) {
		this->mbtiles_file_full_path = source_params.full_path;
		return sg_ret::ok;
	} else {
		const QString sqlite_error_string = sqlite3_errmsg(this->sqlite_handle);
		qDebug() << SG_PREFIX_E << "Can't open sqlite data source:" << sqlite_error_string;

		error_message = QObject::tr("Failed to open MBTiles file.\n"
					    "Path: %1\n"
					    "Error: %2").arg(source_params.full_path).arg(sqlite_error_string);

		this->sqlite_handle = nullptr;
		return sg_ret::err;
	}
}




sg_ret MapSourceMBTiles::close_map_source(void)
{
	if (this->sqlite_handle) {
		/* Notice that we don't call here sqlite3_close_v2()
		   as it is (according to documentation in header)
		   intended for use in garbage-collected languages. */
		const int ans = sqlite3_close(this->sqlite_handle);
		if (ans != SQLITE_OK) {
			/* Only to console for information purposes only. */
			qDebug() << SG_PREFIX_E << "Failed to properly close map source:" << ans << sqlite3_errmsg(this->sqlite_handle);
			return sg_ret::err;
		}
	}

	return sg_ret::ok;
}




void get_mbtiles_z_x_y(const TileInfo & tile_info, int & z, int & x, int & y)
{
	z = tile_info.osm_tile_zoom_level().value(); /* This is OSM MBTile, so use method that returns OSM-like zoom level. */
	x = tile_info.x;
	y = (int) pow(2, z) - 1 - tile_info.y; /* MBTiles stored internally with the flipping y thingy (i.e. TMS scheme). */

	return;
}
