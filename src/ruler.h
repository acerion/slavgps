/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2016-2018, Kamil Ignacak <acerion@wp.pl>
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

#ifndef _H_SG_RULER_
#define _H_SG_RULER_




#include <QPainter>




#include "measurements.h"




namespace SlavGPS {




	class Viewport;



	class Ruler {
	public:
		Ruler(Viewport * new_viewport, int begin_x, int begin_y, DistanceUnit new_distance_unit);
		void end_moved_to(int new_x2, int new_y2, QPainter & painter, double distance1 = -1.0, double distance2 = -1.0);

	private:
		void draw_line(QPainter & painter);

		int x1 = 0;
		int y1 = 0;
		int x2 = 0;
		int y2 = 0;

		double len = 0;
		double dx = 0;
		double dy = 0;
		double c = 0;
		double s = 0;
		double angle = 0;
		double baseangle = 0;

		DistanceUnit distance_unit;

		Viewport * viewport = NULL;
		double distance1 = 0.0;
		double distance2 = 0.0;

		QPen line_pen;
		QPen arc_pen;
	};




} /* namespace SlavGPS */




#endif /* #ifnder _H_SG_RULER_ */
