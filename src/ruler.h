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
#include <QString>




#include "measurements.h"
#include "coord.h"




namespace SlavGPS {




	class Viewport;



	class Ruler {
	public:
		Ruler(Viewport * new_viewport, DistanceUnit new_distance_unit);

		void set_begin(int begin_x, int begin_y);
		void set_end(int end_x, int end_y);
		void set_total_distance(double new_total_distance) { this->total_distance = new_total_distance; }
		void set_line_pen(const QPen & pen) { this->line_pen = pen; }

		void paint_ruler(QPainter & painter, bool paint_tooltips);

		QString get_msg(void) const;
		double get_angle(void) const { return this->angle; }
		double get_line_distance(void) const { return this->line_distance; }

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
		double base_angle = 0;

		DistanceUnit distance_unit;
		double line_distance = 0.0;
		double total_distance = 0.0;

		Viewport * viewport = NULL;

		QPen line_pen;
		QPen compass_pen;
		QPen arc_pen;

		Coord begin_coord;
		Coord end_coord;
	};




} /* namespace SlavGPS */




#endif /* #ifnder _H_SG_RULER_ */
