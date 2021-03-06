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




	class GisViewport;



	class Ruler {
	public:
		Ruler(GisViewport * gisview, DistanceType::Unit new_distance_unit);

		/* Arguments to the functions should indicate
		   coordinates of pixel in Qt's coordinate system,
		   where beginning (point 0,0) is in upper-left
		   corner. */
		sg_ret set_begin(const ScreenPos & pos);
		sg_ret set_end(const ScreenPos & pos);

		void set_total_distance(const Distance & new_total_distance) { this->total_distance = new_total_distance; }
		void set_line_pen(const QPen & pen) { this->line_pen = pen; }

		void paint_ruler(QPainter & painter, bool paint_tooltips);

		QString get_msg(void) const;
		Angle get_angle(void) const { return this->angle; }
		Distance get_line_distance(void) const { return this->line_distance; }

	private:
		void draw_line(QPainter & painter);

		/* These coordinates of beginning and end of ruler are
		   in Qt's coordinate system, where beginning (pixel
		   0,0) is in upper-left corner. */
		ScreenPos m_begin_pos;
		ScreenPos m_end_pos;

		double dx = 0;
		double dy = 0;

		ArrowSymbol begin_arrow;
		ArrowSymbol end_arrow;

		Angle angle;
		Angle base_angle;

		DistanceType::Unit distance_unit;
		Distance line_distance;
		Distance total_distance;

		GisViewport * gisview = NULL;

		QPen line_pen;
		QPen compass_pen;
		QPen arc_pen;

		Coord begin_coord;
		Coord end_coord;
	};




} /* namespace SlavGPS */




#endif /* #ifnder _H_SG_RULER_ */
