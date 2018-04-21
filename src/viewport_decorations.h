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

#ifndef _SG_VIEWPORT_DECORATIONS_H_
#define _SG_VIEWPORT_DECORATIONS_H_




#include <list>




#include <QStringList>




#include "measurements.h"




namespace SlavGPS {




	class Viewport;




	class ViewportDecorations {
	public:
		ViewportDecorations();

		void draw(Viewport * viewport);
		void reset_data(void);

		QStringList copyrights;
		std::list<QPixmap const *> logos;

	private:
		void draw_scale(Viewport * viewport);
		void draw_copyrights(Viewport * viewport);
		void draw_center_mark(Viewport * viewport);
		void draw_logos(Viewport * viewport);

		void draw_scale_helper_scale(Viewport * viewport, const QPen & pen, int scale_len, int h);
		QString draw_scale_helper_value(Viewport * viewport, DistanceUnit distance_unit, double scale_unit);

		void reset_copyrights(void);
		void reset_logos(void);

		/* For scale and center mark. */
		QPen pen_marks_bg;
		QPen pen_marks_fg;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_VIEWPORT_DECORATIONS_H_ */
