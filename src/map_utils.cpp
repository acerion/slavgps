/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013-2014, Rob Norris <rw_norris@hotmail.com>
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




#include <cmath>
#include <cstdlib>




#include "map_utils.h"
#include "measurements.h"
#include "viewport_zoom.h"




using namespace SlavGPS;




/**
 * SECTION:map_utils
 * @short_description: Notes about TMS / Spherical Mercator conversion
 *
 * Coords are in Spherical Mercator projection (#CoordMode::LatLon)
 * TileInfo are in Inverse TMS
 *
 * See: http://docs.openlayers.org/library/spherical_mercator.html
 * See: http://wiki.osgeo.org/wiki/Tile_Map_Service_Specification
 * NB: the Y axis is inverted, ie the origin is at top-left corner.
 */




/**
 * @src:   Original #Coord in #CoordMode::LatLon format
 * @xzoom: Viking scale in x direction
 * @yzoom: Viking scale in y direction (actually needs to be same as xzoom)
 * @dest:  The resulting Inverse TMS coordinates in #TileInfo
 *
 * Convert a #Coord in CoordMode::LatLon format into Inverse TMS coordinates.
 *
 * Returns: whether the conversion was performed
 */
sg_ret MapUtils::lat_lon_to_iTMS(const LatLon & lat_lon, const VikingScale & viking_scale, TileInfo & dest)
{
	if (!viking_scale.x_y_is_equal()) {
		return sg_ret::err;
	}

	dest.scale = viking_scale.to_tile_scale();
	if (!dest.scale.is_valid()) {
		return sg_ret::err;
	}

	/* Convenience variable. */
	const double xmpp = viking_scale.get_x();
	const double ympp = viking_scale.get_y();

	dest.x = (lat_lon.lon + 180) / 360 * VIK_GZ(MAGIC_SEVENTEEN) / xmpp;
	dest.y = (180 - MERCLAT(lat_lon.lat)) / 360 * VIK_GZ(MAGIC_SEVENTEEN) / ympp;
	dest.z = 0;

	return sg_ret::ok;
}




/* Internal convenience function. */
static LatLon _to_lat_lon_with_offset(const TileInfo & src, double offset)
{
	LatLon lat_lon;

	const double socalled_mpp = src.scale.to_so_called_mpp();
	lat_lon.lon = ((src.x + offset) / VIK_GZ(MAGIC_SEVENTEEN) * socalled_mpp * 360) - 180;
	lat_lon.lat = DEMERCLAT(180 - ((src.y + offset) / VIK_GZ(MAGIC_SEVENTEEN) * socalled_mpp * 360));

	return lat_lon;
}




/**
 * @src:   Original #TileInfo in Inverse TMS format
 * @dest:  The resulting Spherical Mercator coordinates in #Coord
 *
 * Convert a #TileInfo in Inverse TMS format into Spherical Mercator
 * coordinates for the center of the TMS area.
 *
 * Returns: whether the conversion was performed
 */
LatLon SlavGPS::MapUtils::iTMS_to_center_lat_lon(const TileInfo & src)
{
	return _to_lat_lon_with_offset(src, 0.5);
}




/**
 * @src:   Original #TileInfo in Inverse TMS format
 * @dest:  The resulting Spherical Mercator coordinates in #Coord
 *
 * Convert a #TileInfo in Inverse TMS format into Spherical Mercator
 * coordinates (for the top left corner of the Inverse TMS area).
 *
 * Returns: whether the conversion was performed
 */
LatLon SlavGPS::MapUtils::iTMS_to_lat_lon(const TileInfo & src)
{
	return _to_lat_lon_with_offset(src, 0.0);
}
