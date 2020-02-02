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




#include <cmath>
#include <cstdlib>
#include <cassert>




#include "layer_map_source_tms.h"
#include "map_utils.h"
#include "viewport_internal.h"




using namespace SlavGPS;




#define SG_MODULE "Map Source TMS"




MapSourceTms::MapSourceTms()
{
	fprintf(stderr, "MapSourceTms constructor start\n");
	this->drawmode = GisViewportDrawMode::LatLon;
	fprintf(stderr, "MapSourceTms constructor end\n");
}




MapSourceTms::~MapSourceTms()
{
	fprintf(stderr, "MapSourceTms destructor start\n");
	fprintf(stderr, "MapSourceTms destructor end\n");
}




MapSourceTms::MapSourceTms(MapTypeID map_type_id, const QString & ui_label, const QString & new_server_hostname, const QString & new_server_path_format)
{
	this->m_map_type_id = map_type_id;
	this->m_ui_label = ui_label;
	this->server_hostname = new_server_hostname;
	this->server_path_format = new_server_path_format;

	this->is_direct_file_access_flag = false;
	this->is_osm_meta_tiles_flag = false;
}




bool MapSourceTms::supports_download_only_new(void) const
{
	return this->dl_options.check_file_server_time;
}




bool MapSourceTms::coord_to_tile_info(const Coord & src_coord, const VikingScale & viking_scale, TileInfo & tile_info) const
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

	/* Note: VIK_GZ(MAGIC_SEVENTEEN) / xmpp / 2 = number of tile on Y axis */
	fprintf(stderr, "DEBUG: %s: xmpp=%f ympp=%f -> %f\n", __FUNCTION__, xmpp, ympp, VIK_GZ(MAGIC_SEVENTEEN) / xmpp / 2);
	tile_info.x = floor((src_coord.lat_lon.lon + 180) / 180 * VIK_GZ(MAGIC_SEVENTEEN) / xmpp / 2);
	/* We should restore logic of viking:
	   tile index on Y axis follow a screen logic (top -> down). */
	tile_info.y = floor((180 - (src_coord.lat_lon.lat + 90)) / 180 * VIK_GZ(MAGIC_SEVENTEEN) / xmpp / 2);
	tile_info.z = 0;
	fprintf(stderr, "DEBUG: %s: %f,%f -> %d,%d\n", __FUNCTION__, src_coord.lat_lon.lon, src_coord.lat_lon.lat, tile_info.x, tile_info.y);
	return true;
}




sg_ret MapSourceTms::tile_info_to_center_coord(const TileInfo & src, Coord & coord) const
{
	const double socalled_mpp = src.scale.to_so_called_mpp();

	coord.set_coord_mode(CoordMode::LatLon); /* This function decides what will be the coord mode of returned coordinate. */

	coord.lat_lon.lon = (src.x + 0.5) * 180 / VIK_GZ(MAGIC_SEVENTEEN) * socalled_mpp * 2 - 180;
	/* We should restore logic of viking:
	   tile index on Y axis follow a screen logic (top -> down). */
	coord.lat_lon.lat = -((src.y + 0.5) * 180 / VIK_GZ(MAGIC_SEVENTEEN) * socalled_mpp * 2 - 90);

	qDebug() << SG_PREFIX_D << "Converting:" << src.x << src.y << "->" << coord.lat_lon;

	return sg_ret::ok;
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
