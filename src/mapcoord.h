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




	/* https://wiki.openstreetmap.org/wiki/Zoom_levels */
	class TileZoomLevel {
	public:
		enum class Level {
			Min      =  0,     /* Maximal zoom out, one tile showing whole world. */
			Default  = 17,     /* Zoomed in quite a bit. MAGIC_SEVENTEEN. */
			Max      = 20,     /* Maximal zoom in. */
		};

		explicit TileZoomLevel(int value);
		TileZoomLevel(TileZoomLevel::Level value) : m_value((int) value) {};

		void set_value(int value) { this->m_value = value; };
		int value(void) const { return this->m_value; };

		QString to_string(void) const;

		static bool unit_tests(void);

		/* These operators are members of the class to avoid
		   ambiguity during compilation - compiler indicates
		   that similar operators from Measurement template
		   class are potential candidates. */
		bool operator<(const TileZoomLevel & rhs) const;
		bool operator>(const TileZoomLevel & rhs) const;
		bool operator<=(const TileZoomLevel & rhs) const;
		bool operator>=(const TileZoomLevel & rhs) const;
	private:
		int m_value = 0;
	};





	class TilesRange {
	public:
		int get_tiles_count(void) const;

		int horiz_first_idx = 0;
		int horiz_last_idx  = 0;
		int vert_first_idx = 0;
		int vert_last_idx  = 0;

		/* How the tile index values change? Do they increase
		   (+1) or decrease (-1)? */
		int horiz_delta = 1;
		int vert_delta = 1;
	};




	class TileScale {
	public:
		TileZoomLevel osm_tile_zoom_level(void) const;
		int get_non_osm_scale(void) const;
		bool is_valid(void) const;
		double to_so_called_mpp(void) const;

		void set_scale_value(int new_value);
		int get_scale_value(void) const;

		void set_scale_valid(bool new_value);
		bool get_scale_valid(void) const;

		bool from_viking_scale = false;

	private:
		int value = 0;
		bool valid = false;
	};




	/* Common struct to all map types and map layer, to hold info
	   about a particular map tile. */
	class TileInfo {
	public:
		static TilesRange get_tiles_range(const TileInfo & ulm, const TileInfo & brm);

		void resize_up(int scale_dec, int scale_factor);
		void resize_down(int scale_inc, int scale_factor);

		/*
		  Get Lat/Lon coordinates of two points of iTMS tile:
		  of upper-left corner and of bottom-right corner.
		*/
		sg_ret get_itms_lat_lon_ul_br(LatLon & lat_lon_ul, LatLon & lat_lon_br) const;

		/* https://wiki.openstreetmap.org/wiki/Slippy_map_tilenames#X_and_Y */
		int x = 0;
		int y = 0;

		/* For use in OSM-like context only (0 = max zoomed out; ~18 = max zoomed in). */
		TileZoomLevel osm_tile_zoom_level(void) const { return this->scale.osm_tile_zoom_level(); }

		int z;      /* Zone or anything else. */
		TileScale scale;
	};
	QDebug operator<<(QDebug debug, const TileInfo & tile_info);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TILE_INFO_H_ */
