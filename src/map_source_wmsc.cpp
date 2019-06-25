/*
 * viking
 * Copyright (C) 2010, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 *
 * viking is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * viking is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

 /**
  * SECTION:map_source_wmsc
  * @short_description: the class for WMS/WMS-C oriented map sources
  *
  * The #VikWmscMapSource class handles WMS/WMS-C oriented map sources.
  *
  * http://wiki.osgeo.org/wiki/WMS_Tile_Caching
  */




#include <cmath>
#include <cstdlib>
#include <cassert>




#include "map_source_wmsc.h"
#include "map_utils.h"
#include "viewport_internal.h"
#include "viewport_zoom.h"




using namespace SlavGPS;




#define SG_MODULE "Map Source WMSC"




MapSourceWmsc::MapSourceWmsc()
{
	fprintf(stderr, "MapSourceWmsc constructor start\n");
	drawmode = ViewportDrawMode::LatLon;
	fprintf(stderr, "MapSourceWmsc constructor end\n");
}




MapSourceWmsc::~MapSourceWmsc()
{
	fprintf(stderr, "MapSourceWmsc destructor start\n");
	fprintf(stderr, "MapSourceWmsc destructor end\n");
}




MapSourceWmsc::MapSourceWmsc(MapTypeID new_map_type_id, const QString & new_label, const QString & new_server_hostname, const QString & new_server_path_format)
{
	this->map_type_id = new_map_type_id;
	this->label = new_label;
	this->server_hostname = new_server_hostname;
	this->server_path_format = new_server_path_format;

	this->is_direct_file_access_flag = false;
	this->is_osm_meta_tiles_flag = false;
}




bool MapSourceWmsc::supports_download_only_new(void) const
{
	return this->dl_options.check_file_server_time;
}




bool MapSourceWmsc::coord_to_tile_info(const Coord & src_coord, const VikingScale & viking_scale, TileInfo & tile_info) const
{
	assert (src_coord.get_coord_mode() == CoordMode::LatLon);

	if (!viking_scale.x_y_is_equal()) {
		return false;
	}

	tile_info.scale = viking_scale.to_tile_scale();
	if (!tile_info.scale.is_valid()) {
		return false;
	}

	/* Convenience variables. */
	const double xmpp = viking_scale.get_x();
	const double ympp = viking_scale.get_y();

	/* Note: VIK_GZ(MAGIC_SEVENTEEN) / xmpp / 2 = number of tile on Y axis. */
	fprintf(stderr, "DEBUG: %s: xmpp=%f ympp=%f -> %f\n", __FUNCTION__, xmpp, ympp, VIK_GZ(MAGIC_SEVENTEEN) / xmpp / 2);
	tile_info.x = floor((src_coord.ll.lon + 180) / 180 * VIK_GZ(MAGIC_SEVENTEEN) / xmpp / 2);
	/* We should restore logic of viking:
	   tile index on Y axis follow a screen logic (top -> down). */
	tile_info.y = floor((180 - (src_coord.ll.lat + 90)) / 180 * VIK_GZ(MAGIC_SEVENTEEN) / xmpp / 2);
	tile_info.z = 0;
	fprintf(stderr, "DEBUG: %s: %f,%f -> %d,%d\n", __FUNCTION__, src_coord.ll.lon, src_coord.ll.lat, tile_info.x, tile_info.y);
	return true;
}




sg_ret MapSourceWmsc::tile_info_to_center_coord(const TileInfo & src, Coord & coord) const
{
	const double socalled_mpp = src.scale.to_so_called_mpp();

	coord.set_coord_mode(CoordMode::LatLon); /* This function decides what will be the coord mode of returned coordinate. */

	coord.ll.lon = (src.x + 0.5) * 180 / VIK_GZ(MAGIC_SEVENTEEN) * socalled_mpp * 2 - 180;
	/* We should restore logic of viking:
	   tile index on Y axis follow a screen logic (top -> down). */
	coord.ll.lat = -((src.y + 0.5) * 180 / VIK_GZ(MAGIC_SEVENTEEN) * socalled_mpp * 2 - 90);

	qDebug() << SG_PREFIX_D << "Converting:" << src.x << src.y << "->" << coord.ll;

	return sg_ret::ok;
}




const QString MapSourceWmsc::get_server_path(const TileInfo & src) const
{
	const double socalled_mpp = src.scale.to_so_called_mpp();
	const double minx = (double)src.x * 180 / VIK_GZ(MAGIC_SEVENTEEN) * socalled_mpp * 2 - 180;
	const double maxx = (double)(src.x + 1) * 180 / VIK_GZ(MAGIC_SEVENTEEN) * socalled_mpp * 2 - 180;
	/* We should restore logic of viking:
	   tile index on Y axis follow a screen logic (top -> down). */
	const double miny = -((double)(src.y + 1) * 180 / VIK_GZ(MAGIC_SEVENTEEN) * socalled_mpp * 2 - 90);
	const double maxy = -((double)(src.y) * 180 / VIK_GZ(MAGIC_SEVENTEEN) * socalled_mpp * 2 - 90);

	/* This is very similar to how LatLonBBoxStrings variable is created in bbox.cpp. */
	QLocale c_locale = QLocale::c();
	const QString uri = QString(this->server_path_format)
		.arg(c_locale.toString(minx))
		.arg(c_locale.toString(miny))
		.arg(c_locale.toString(maxx))
		.arg(c_locale.toString(maxy));
	return uri;
}
