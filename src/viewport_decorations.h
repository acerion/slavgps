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
#include "globals.h"




namespace SlavGPS {




	class Viewport;
	class ViewportLogo;




	class ViewportDecorations {
	public:
		ViewportDecorations();

		sg_ret add_attribution(QString const & copyright);
		sg_ret add_logo(const ViewportLogo & logo);

		void draw(Viewport * viewport) const;
		void clear(void);

	private:
		void draw_attributions(Viewport * viewport) const;
		void draw_logos(Viewport * viewport) const;

		void draw_scale(Viewport * viewport) const;
		void draw_center_mark(Viewport * viewport) const;

		void draw_scale_helper_scale(Viewport * viewport, const QPen & pen, int scale_len, int h) const;
		QString draw_scale_helper_value(Viewport * viewport, DistanceUnit distance_unit, double scale_unit) const;

		/* For scale and center mark. */
		QPen pen_marks_bg;
		QPen pen_marks_fg;

		QStringList attributions; /* Attributions/copyrights of stuff displayed in viewport. */
		std::list<ViewportLogo> logos;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_VIEWPORT_DECORATIONS_H_ */
