/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _SG_TILE_INFO_H_
#define _SG_TILE_INFO_H_




#include <QDebug>




#include "globals.h"




namespace SlavGPS {




	class LatLon;




	class TilesRange {
	public:
		int get_tiles_count(void) const;

		int x_begin = 0;
		int x_end   = 0;
		int y_begin = 0;
		int y_end   = 0;
	};




	class TileScale {
	public:
		int get_tile_zoom_level(void) const;
		int get_non_osm_scale(void) const;
		bool is_valid(void) const;
		double to_so_called_mpp(void) const;

		void set_scale_value(int new_value);
		int get_scale_value(void) const;

		void set_scale_valid(bool new_value);
		bool get_scale_valid(void) const;

		bool from_viking_zoom_level = false;

	private:
		int value = 0;
		bool valid = false;
	};




	/* Common struct to all map types and map layer, to hold info
	   about a particular map tile. */
	class TileInfo {
	public:
		static TilesRange get_tiles_range(const TileInfo & ulm, const TileInfo & brm);

		void scale_up(int scale_dec, int scale_factor);
		void scale_down(int scale_inc, int scale_factor);

		/*
		  Get Lat/Lon coordinates of two points of iTMS tile:
		  of upper-left corner and of bottom-right corner.
		*/
		sg_ret get_itms_lat_lon_ul_br(LatLon & lat_lon_ul, LatLon & lat_lon_br) const;

		/* https://wiki.openstreetmap.org/wiki/Slippy_map_tilenames#X_and_Y */
		int x = 0;
		int y = 0;

		/* For use in OSM-like context only (0 = max zoomed out; ~18 = max zoomed in). */
		int get_tile_zoom_level(void) const { return this->scale.get_tile_zoom_level(); }

		int z;      /* Zone or anything else. */
		TileScale scale;
	};
	QDebug operator<<(QDebug debug, const TileInfo & tile_info);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TILE_INFO_H_ */
