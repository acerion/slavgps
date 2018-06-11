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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cmath>
#include <cstdlib>
#include <cassert>

#include <glib.h>

#include "map_source_wmsc.h"
#include "map_utils.h"
#include "viewport_internal.h"




using namespace SlavGPS;




MapSourceWmsc::MapSourceWmsc()
{
	fprintf(stderr, "MapSourceWmsc constructor start\n");
	drawmode = ViewportDrawMode::LATLON;
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
}




bool MapSourceWmsc::is_direct_file_access(void) const
{
	return false;
}




bool MapSourceWmsc::is_mbtiles(void) const
{
	return false;
}




bool MapSourceWmsc::is_osm_meta_tiles(void) const
{
	return false;
}




bool MapSourceWmsc::supports_download_only_new(void) const
{
	return this->dl_options.check_file_server_time;
}




bool MapSourceWmsc::coord_to_tile(const Coord & src_coord, double xzoom, double yzoom, TileInfo * dest) const
{
	assert (src_coord.mode == CoordMode::LATLON);

	if (xzoom != yzoom) {
		return false;
	}

	dest->scale = map_utils_mpp_to_scale(xzoom);
	if (dest->scale == 255) {
		return false;
	}

	/* Note: VIK_GZ(MAGIC_SEVENTEEN) / xzoom / 2 = number of tile on Y axis. */
	fprintf(stderr, "DEBUG: %s: xzoom=%f yzoom=%f -> %f\n", __FUNCTION__,
		xzoom, yzoom, VIK_GZ(MAGIC_SEVENTEEN) / xzoom / 2);
	dest->x = floor((src_coord.ll.lon + 180) / 180 * VIK_GZ(MAGIC_SEVENTEEN) / xzoom / 2);
	/* We should restore logic of viking:
	   tile index on Y axis follow a screen logic (top -> down). */
	dest->y = floor((180 - (src_coord.ll.lat + 90)) / 180 * VIK_GZ(MAGIC_SEVENTEEN) / xzoom / 2);
	dest->z = 0;
	fprintf(stderr, "DEBUG: %s: %f,%f -> %d,%d\n", __FUNCTION__,
		src_coord.ll.lon, src_coord.ll.lat, dest->x, dest->y);
	return true;
}




void MapSourceWmsc::tile_to_center_coord(TileInfo * src, Coord & dest_coord) const
{
	double socalled_mpp;
	if (src->scale >= 0) {
		socalled_mpp = VIK_GZ(src->scale);
	} else {
		socalled_mpp = 1.0/VIK_GZ(-src->scale);
	}
	dest_coord.mode = CoordMode::LATLON;
	dest_coord.ll.lon = (src->x+0.5) * 180 / VIK_GZ(MAGIC_SEVENTEEN) * socalled_mpp * 2 - 180;
	/* We should restore logic of viking:
	   tile index on Y axis follow a screen logic (top -> down). */
	dest_coord.ll.lat = -((src->y+0.5) * 180 / VIK_GZ(MAGIC_SEVENTEEN) * socalled_mpp * 2 - 90);
	fprintf(stderr, "DEBUG: %s: %d,%d -> %f,%f\n", __FUNCTION__,
		src->x, src->y, dest_coord.ll.lon, dest_coord.ll.lat);
}




const QString MapSourceWmsc::get_server_path(TileInfo * src) const
{
	double socalled_mpp;
	if (src->scale >= 0) {
		socalled_mpp = VIK_GZ(src->scale);
	} else {
		socalled_mpp = 1.0/VIK_GZ(-src->scale);
	}
	double minx = (double)src->x * 180 / VIK_GZ(MAGIC_SEVENTEEN) * socalled_mpp * 2 - 180;
	double maxx = (double)(src->x + 1) * 180 / VIK_GZ(MAGIC_SEVENTEEN) * socalled_mpp * 2 - 180;
	/* We should restore logic of viking:
	   tile index on Y axis follow a screen logic (top -> down). */
	double miny = -((double)(src->y + 1) * 180 / VIK_GZ(MAGIC_SEVENTEEN) * socalled_mpp * 2 - 90);
	double maxy = -((double)(src->y) * 180 / VIK_GZ(MAGIC_SEVENTEEN) * socalled_mpp * 2 - 90);

	char sminx[G_ASCII_DTOSTR_BUF_SIZE];
	char smaxx[G_ASCII_DTOSTR_BUF_SIZE];
	char sminy[G_ASCII_DTOSTR_BUF_SIZE];
	char smaxy[G_ASCII_DTOSTR_BUF_SIZE];

	g_ascii_dtostr(sminx, G_ASCII_DTOSTR_BUF_SIZE, minx);
	g_ascii_dtostr(smaxx, G_ASCII_DTOSTR_BUF_SIZE, maxx);
	g_ascii_dtostr(sminy, G_ASCII_DTOSTR_BUF_SIZE, miny);
	g_ascii_dtostr(smaxy, G_ASCII_DTOSTR_BUF_SIZE, maxy);

	const QString uri = QString(this->server_path_format).arg(sminx).arg(sminy).arg(smaxx).arg(smaxy);
	return uri;
}
