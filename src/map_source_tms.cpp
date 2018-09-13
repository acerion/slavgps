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

#include <cmath>
#include <cstdlib>
#include <cassert>

#include "map_source_tms.h"
#include "map_utils.h"
#include "viewport_internal.h"




using namespace SlavGPS;




#define SG_MODULE "Map Source TMS"




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




MapSourceTms::MapSourceTms(MapTypeID new_map_type_id, const QString & new_label, const QString & new_server_hostname, const QString & new_server_path_format)
{
	this->map_type_id = new_map_type_id;
	this->label = new_label;
	this->server_hostname = new_server_hostname;
	this->server_path_format = new_server_path_format;
}




bool MapSourceTms::is_direct_file_access(void) const
{
	return false;
}




bool MapSourceTms::is_osm_meta_tiles(void) const
{
	return false;
}




bool MapSourceTms::supports_download_only_new(void) const
{
	return this->dl_options.check_file_server_time;
}




bool MapSourceTms::coord_to_tile(const Coord & src_coord, const VikingZoomLevel & viking_zoom_level, TileInfo & dest) const
{
	assert (src_coord.mode == CoordMode::LATLON);

	if (!viking_zoom_level.x_y_is_equal()) {
		return false;
	}

	/* Convenience variables. */
	const double xzoom = viking_zoom_level.get_x();
	const double yzoom = viking_zoom_level.get_y();

	dest.scale = map_utils_mpp_to_tile_scale(xzoom);
	if (!dest.scale.is_valid()) {
		return false;
	}

	/* Note: VIK_GZ(MAGIC_SEVENTEEN) / xzoom / 2 = number of tile on Y axis */
	fprintf(stderr, "DEBUG: %s: xzoom=%f yzoom=%f -> %f\n", __FUNCTION__,
		xzoom, yzoom, VIK_GZ(MAGIC_SEVENTEEN) / xzoom / 2);
	dest.x = floor((src_coord.ll.lon + 180) / 180 * VIK_GZ(MAGIC_SEVENTEEN) / xzoom / 2);
	/* We should restore logic of viking:
	   tile index on Y axis follow a screen logic (top -> down). */
	dest.y = floor((180 - (src_coord.ll.lat + 90)) / 180 * VIK_GZ(MAGIC_SEVENTEEN) / xzoom / 2);
	dest.z = 0;
	fprintf(stderr, "DEBUG: %s: %f,%f -> %d,%d\n", __FUNCTION__, src_coord.ll.lon, src_coord.ll.lat, dest.x, dest.y);
	return true;
}




void MapSourceTms::tile_to_center_coord(const TileInfo & src, Coord & dest_coord) const
{
	const double socalled_mpp = src.scale.to_so_called_mpp();
	dest_coord.mode = CoordMode::LATLON;
	dest_coord.ll.lon = (src.x + 0.5) * 180 / VIK_GZ(MAGIC_SEVENTEEN) * socalled_mpp * 2 - 180;
	/* We should restore logic of viking:
	   tile index on Y axis follow a screen logic (top -> down). */
	dest_coord.ll.lat = -((src.y + 0.5) * 180 / VIK_GZ(MAGIC_SEVENTEEN) * socalled_mpp * 2 - 90);
	qDebug() << SG_PREFIX_D << "Converting:" << src.x << src.y << "->" << dest_coord.ll.lon << dest_coord.ll.lat;
}




const QString MapSourceTms::get_server_path(const TileInfo & src) const
{
	/* We should restore logic of viking:
	 * tile index on Y axis follow a screen logic (top -> down)
	 */

	/* Note : nb tiles on Y axis */
	int nb_tiles = VIK_GZ(MAGIC_SEVENTEEN - src.scale.get_non_osm_scale() - 1);

	const QString uri = QString(this->server_path_format).arg(MAGIC_SEVENTEEN - src.scale.get_non_osm_scale() - 1).arg(src.x).arg(nb_tiles - src.y - 1);

	return QString(uri);
}
