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




using namespace SlavGPS;




/* World Scale:
   VIK_GZ(MAGIC_SEVENTEEN) down to submeter scale: 1/VIK_GZ(5)

   No map provider is going to have tiles at the highest zoom in level - but we can interpolate to that. */


static const double scale_mpps[] = { VIK_GZ(0), VIK_GZ(1), VIK_GZ(2), VIK_GZ(3), VIK_GZ(4), VIK_GZ(5),
				     VIK_GZ(6), VIK_GZ(7), VIK_GZ(8), VIK_GZ(9), VIK_GZ(10), VIK_GZ(11),
				     VIK_GZ(12), VIK_GZ(13), VIK_GZ(14), VIK_GZ(15), VIK_GZ(16), VIK_GZ(17) };
static const int num_scales = (sizeof(scale_mpps) / sizeof(scale_mpps[0]));

static const double scale_neg_mpps[] = { 1.0/VIK_GZ(0), 1.0/VIK_GZ(1), 1.0/VIK_GZ(2),
					 1.0/VIK_GZ(3), 1.0/VIK_GZ(4), 1.0/VIK_GZ(5) };
static const int num_scales_neg = (sizeof(scale_neg_mpps) / sizeof(scale_neg_mpps[0]));




#define ERROR_MARGIN 0.01




/**
 * @mpp: The so called 'mpp'
 *
 * Returns: the zoom scale value which may be negative.
 */
TileScale SlavGPS::map_utils_mpp_to_tile_scale(double mpp)
{
	TileScale tile_scale;

	for (int i = 0; i < num_scales; i++) {
		if (std::abs(scale_mpps[i] - mpp) < ERROR_MARGIN) {
			tile_scale.value = i;
			tile_scale.valid = true;
			return tile_scale;
		}
	}
	for (int i = 0; i < num_scales_neg; i++) {
		if (std::abs(scale_neg_mpps[i] - mpp) < 0.000001) {
			tile_scale.value = -i;
			tile_scale.valid = true;
			return tile_scale;
		}
	}

	/* In original implementation of the function '255' was the
	   value returned when the loops didn't find any valid
	   value. */
	tile_scale.value = 255;
	tile_scale.valid = false;
	return tile_scale;
}




/**
 * @mpp: The so called 'mpp'
 *
 * Returns: a Map Source Zoom Level.
 * See: http://wiki.openstreetmap.org/wiki/Zoom_levels
 */
MapSourceZoomLevel SlavGPS::map_utils_mpp_to_zoom_level(double mpp)
{
	const TileScale tile_scale = map_utils_mpp_to_tile_scale(mpp);
	int osm_zoom_level = tile_scale.value.get_osm_scale();
	if (osm_zoom_level < 0) {
		osm_zoom_level = MAGIC_SEVENTEEN;
	}
	return MapSourceZoomLevel(osm_zoom_level);
}




/**
 * SECTION:map_utils
 * @short_description: Notes about TMS / Spherical Mercator conversion
 *
 * Coords are in Spherical Mercator projection (#CoordMode::LATLON)
 * TileInfo are in Inverse TMS
 *
 * See: http://docs.openlayers.org/library/spherical_mercator.html
 * See: http://wiki.osgeo.org/wiki/Tile_Map_Service_Specification
 * NB: the Y axis is inverted, ie the origin is at top-left corner.
 */




/**
 * @src:   Original #Coord in #CoordMode::LATLON format
 * @xzoom: Viking zoom level in x direction
 * @yzoom: Viking zoom level in y direction (actually needs to be same as xzoom)
 * @dest:  The resulting Inverse TMS coordinates in #TileInfo
 *
 * Convert a #Coord in CoordMode::LATLON format into Inverse TMS coordinates.
 *
 * Returns: whether the conversion was performed
 */
bool SlavGPS::map_utils_coord_to_iTMS(const Coord & src_coord, double xzoom, double yzoom, TileInfo & dest)
{
	if (src_coord.mode != CoordMode::LATLON) {
		return false;
	}

	if (xzoom != yzoom) {
		return false;
	}

	dest.scale = map_utils_mpp_to_tile_scale(xzoom);
	if (!dest.scale.is_valid()) {
		return false;
	}

	dest.x = (src_coord.ll.lon + 180) / 360 * VIK_GZ(MAGIC_SEVENTEEN) / xzoom;
	dest.y = (180 - MERCLAT(src_coord.ll.lat)) / 360 * VIK_GZ(MAGIC_SEVENTEEN) / xzoom;
	dest.z = 0;

	return true;
}




/* Internal convenience function. */
static Coord _to_vikcoord_with_offset(const TileInfo & src, double offset)
{
	Coord dest_coord;

	double socalled_mpp;
	if (src.scale.value >= 0) {
		socalled_mpp = VIK_GZ(src.scale.value);
	} else {
		socalled_mpp = 1.0/VIK_GZ(-src.scale.value);
	}
	dest_coord.mode = CoordMode::LATLON;
	dest_coord.ll.lon = ((src.x + offset) / VIK_GZ(MAGIC_SEVENTEEN) * socalled_mpp * 360) - 180;
	dest_coord.ll.lat = DEMERCLAT(180 - ((src.y + offset) / VIK_GZ(MAGIC_SEVENTEEN) * socalled_mpp * 360));

	return dest_coord;
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
Coord SlavGPS::map_utils_iTMS_to_center_coord(const TileInfo & src)
{
	return _to_vikcoord_with_offset(src, 0.5);
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
Coord SlavGPS::map_utils_iTMS_to_coord(const TileInfo & src)
{
	return _to_vikcoord_with_offset(src, 0.0);
}
