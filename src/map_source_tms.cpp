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
  * SECTION:map_source_tms
  * @short_description: the class for TMS oriented map sources
  *
  * The #VikTmsMapSource class handles TMS oriented map sources.
  *
  * The tiles are in 'equirectangular'.
  * http://en.wikipedia.org/wiki/Equirectangular_projection
  *
  * Such service is also a type of TMS (Tile Map Service) as defined in
  * OSGeo's wiki.
  * http://wiki.osgeo.org/wiki/Tile_Map_Service_Specification
  * Following this specification, the protocol handled by this class
  * follows the global-geodetic profile.
  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_MATH_H
#include <cmath>
#endif

#include <cstdlib>
#include <cassert>

#include "globals.h"
#include "map_source_tms.h"
#include "map_utils.h"




using namespace SlavGPS;




MapSourceTms::MapSourceTms()
{
	fprintf(stderr, "MapSourceTms constructor start\n");
	drawmode = ViewportDrawMode::LATLON;
	fprintf(stderr, "MapSourceTms constructor end\n");
}




MapSourceTms::~MapSourceTms()
{
	fprintf(stderr, "MapSourceTms destructor start\n");
	fprintf(stderr, "MapSourceTms destructor end\n");
}




MapSourceTms::MapSourceTms(MapTypeID map_type_, char const * label_, char const * hostname, char const * url_)
{
	map_type = map_type_;
	label = g_strdup(label_);
	this->server_hostname = QString(hostname);
	server_path_format = g_strdup(url_);
}




bool MapSourceTms::is_direct_file_access()
{
	return false;
}




bool MapSourceTms::is_mbtiles()
{
	return false;
}




bool MapSourceTms::is_osm_meta_tiles()
{
	return false;
}




bool MapSourceTms::supports_download_only_new()
{
	return this->dl_options.check_file_server_time;
}




bool MapSourceTms::coord_to_tile(const Coord * src, double xzoom, double yzoom, TileInfo * dest)
{
	assert (src->mode == CoordMode::LATLON);

	if (xzoom != yzoom) {
		return false;
	}

	dest->scale = map_utils_mpp_to_scale(xzoom);
	if (dest->scale == 255) {
		return false;
	}

	/* Note : VIK_GZ(17) / xzoom / 2 = number of tile on Y axis */
	fprintf(stderr, "DEBUG: %s: xzoom=%f yzoom=%f -> %f\n", __FUNCTION__,
		xzoom, yzoom, VIK_GZ(17) / xzoom / 2);
	dest->x = floor((src->ll.lon + 180) / 180 * VIK_GZ(17) / xzoom / 2);
	/* We should restore logic of viking:
	 * tile index on Y axis follow a screen logic (top -> down)
	 */
	dest->y = floor((180 - (src->ll.lat + 90)) / 180 * VIK_GZ(17) / xzoom / 2);
	dest->z = 0;
	fprintf(stderr, "DEBUG: %s: %f,%f -> %d,%d\n", __FUNCTION__,
		src->ll.lon, src->ll.lat, dest->x, dest->y);
	return true;
}




void MapSourceTms::tile_to_center_coord(TileInfo * src, Coord * dest)
{
	double socalled_mpp;
	if (src->scale >= 0) {
		socalled_mpp = VIK_GZ(src->scale);
	} else {
		socalled_mpp = 1.0/VIK_GZ(-src->scale);
	}
	dest->mode = CoordMode::LATLON;
	dest->ll.lon = (src->x + 0.5) * 180 / VIK_GZ(17) * socalled_mpp * 2 - 180;
	/* We should restore logic of viking:
	 * tile index on Y axis follow a screen logic (top -> down)
	 */
	dest->ll.lat = -((src->y + 0.5) * 180 / VIK_GZ(17) * socalled_mpp * 2 - 90);
	fprintf(stderr, "DEBUG: %s: %d,%d -> %f,%f\n", __FUNCTION__,
		src->x, src->y, dest->ll.lon, dest->ll.lat);
}




const QString MapSourceTms::get_server_path(TileInfo * src) const
{
	/* We should restore logic of viking:
	 * tile index on Y axis follow a screen logic (top -> down)
	 */

	/* Note : nb tiles on Y axis */
	int nb_tiles = VIK_GZ(17 - src->scale - 1);

	char * uri = g_strdup_printf(server_path_format, 17 - src->scale - 1, src->x, nb_tiles - src->y - 1); /* kamilFIXME: memory leak. */

	return QString(uri);
}
