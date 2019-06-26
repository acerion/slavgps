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

#ifndef _SG_COORD_H_
#define _SG_COORD_H_




#include <QString>




#include "coords.h"
#include "measurements.h"




namespace SlavGPS {




	enum class CoordMode {
		Invalid = -1, /* Invalid mode. May be also used to indicate invalid coordinate. */
		UTM     =  0,
		LatLon  =  1
	};




	class Coord {
	public:
		Coord() {};
		Coord(const LatLon & new_lat_lon, CoordMode new_mode);
		Coord(const UTM & new_utm, CoordMode new_mode);
		Coord(const Coord& coord);

		LatLon get_lat_lon(void) const;
		UTM get_utm(void) const;
		CoordMode get_coord_mode(void) const;

		void set_coord_mode(CoordMode mode);

		/* Get top-left and bottom-right coordinates of rectangle that has
		   dimensions (width/height) specified by area_span, and is centered at this coord's center. */
		void get_area_coordinates(const LatLon & area_span, Coord * coord_tl, Coord * coord_br) const;

		bool is_inside(const Coord & coord_tl, const Coord & coord_br) const;

		sg_ret recalculate_to_mode(CoordMode new_mode);

		static double distance(const Coord & coord1, const Coord & coord2); /* Result is in meters. */
		static Distance distance_2(const Coord & coord1, const Coord & coord2); /* Result is in meters. */

		QString to_string(void) const;

		bool operator==(const Coord & coord) const;
		bool operator!=(const Coord & coord) const;
		Coord & operator=(const Coord & other);


		LatLon lat_lon;
		UTM utm;

	private:
		CoordMode mode = CoordMode::Invalid;
	};
	QDebug operator<<(QDebug debug, const Coord & coord);




	typedef struct _Rect {
		Coord tl;
		Coord br;
		Coord center;
	} Rect;




} /* namespace SlavGPS */




#endif /* #ifndef _SG_COORD_H_ */
