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




	class GisViewport;
	class GisViewportLogo;




	class GisViewportDecorations {
	public:
		GisViewportDecorations();

		sg_ret add_attribution(QString const & copyright);
		sg_ret add_logo(const GisViewportLogo & logo);

		void draw(GisViewport & gisview) const;
		void clear(void);

	private:
		void draw_attributions(GisViewport & gisview) const;
		void draw_logos(GisViewport & gisview) const;
		void draw_scale(GisViewport & gisview) const;
		void draw_center_mark(GisViewport & gisview) const;

		/**
		   @brief Draw horizontal line with vertical bars representing a scale indicator

		   @param begin - this is where bottom-left corner of
		   scale should be
		*/
		void draw_scale_helper_draw_scale(GisViewport & gisview, const ScreenPos & begin, const QPen & pen, int scale_len_px, int bar_height_max_px) const;

		QString draw_scale_helper_get_value_string(GisViewport & gisview, DistanceUnit distance_unit, double scale_unit) const;

		/**
		   @brief Draw text with viewport's metadata

		   Draw info about viewport's size, viewport's bbox
		   and about geo-coordinates of viewport's corners.
		*/
		void draw_viewport_metadata(GisViewport & gisview) const;

		/**
		   @brief Draw markers of location of some key pixels in viewport
		*/
		void draw_viewport_pixels(GisViewport & gisview) const;

		/* For scale and center mark. */
		QPen pen_marks_bg;
		QPen pen_marks_fg;

		QStringList attributions; /* Attributions/copyrights of stuff displayed in viewport. */
		std::list<GisViewportLogo> logos;

		/* Padding between different decorations. */
		int base_padding = 10;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_VIEWPORT_DECORATIONS_H_ */
