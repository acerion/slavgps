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




#include "coords.h"




namespace SlavGPS {




	/* Possible more modes to come? xy? We'll leave that as an option. */
	enum class CoordMode {
		UTM     = 0,
		LATLON  = 1
	};




	class VikCoord {
	public:
		VikCoord() {};
		VikCoord(const struct LatLon & ll, CoordMode mode);
		VikCoord(const struct UTM & utm, CoordMode mode);

		struct LatLon get_latlon(void) const;
		struct UTM get_utm(void) const;

		void set_area(const struct LatLon * wh, VikCoord * coord_tl, VikCoord * coord_br) const;
		bool is_inside(const VikCoord * coord_tl, const VikCoord * coord_br) const;

		void change_mode(CoordMode new_mode);
		VikCoord copy_change_mode(CoordMode new_mode) const;

		static double distance(const VikCoord & coord1, const VikCoord & coord2);

		bool operator==(const VikCoord & coord) const;
		bool operator!=(const VikCoord & coord) const;


		struct LatLon ll;
		struct UTM utm;
		CoordMode mode;
	};




	typedef VikCoord Coord;

	typedef struct _Rect {
		VikCoord tl;
		VikCoord br;
		VikCoord center;
	} Rect;




} /* namespace SlavGPS */




#endif /* #ifndef _SG_COORD_H_ */
