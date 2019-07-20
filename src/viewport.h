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

#ifndef _SG_VIEWPORT_H_
#define _SG_VIEWPORT_H_




#include <list>
#include <cstdint>




#include <QString>
#include <QPixmap>




#include "coord.h"




/* Used for coord to screen etc, screen to coord. */
#define VIK_VIEWPORT_OFF_SCREEN_DOUBLE -9999999.9





namespace SlavGPS {




	class GisViewport;
	class ViewportPixmap; /* Forward declarations for benefit of headers that include this file. */




	enum class GisViewportDomain {
		Time = 0,
		Elevation,
		Distance,
		Speed,
		Gradient,
		Max
	};




	class GisViewportLogo {
	public:
		QPixmap logo_pixmap;
		QString logo_id; /* For easy comparison of logos. For Map Sources this will be map_type_string; */
	};




	enum class GisViewportDrawMode {
		Invalid = -1,
		UTM = 0,
		Expedia,
		Mercator,
		LatLon
	};
	QDebug operator<<(QDebug debug, const GisViewportDrawMode mode);




	class GisViewportDrawModes {
	public:
		static QString get_label_with_accelerator(GisViewportDrawMode mode);
		static QString get_id_string(GisViewportDrawMode mode);
		static bool set_draw_mode_from_file(GisViewport * gisview, const char * line);
	};




	/* Enum created to avoid using specific pixel values. */
	enum class ScreenPosition {
		UpperLeft,
		UpperRight,
		BottomLeft,
		BottomRight
	};



	/*
	  For pixel coordinates that are better represented by
	  floating point variables. 'qreal' type matches
	  primitive data type used to construct QPointF.
	*/
	typedef qreal fpixel;




	class ScreenPos : public QPointF {
	public:
		ScreenPos() {};
		ScreenPos(fpixel x, fpixel y) : QPointF(x, y) {};

		void set(fpixel x, fpixel y);

		bool operator==(const ScreenPos & pos) const;

		static ScreenPos get_average(const ScreenPos & pos1, const ScreenPos & pos2);
		static bool are_closer_than(const ScreenPos & pos1, const ScreenPos & pos2, fpixel limit);
	};
	QDebug operator<<(QDebug debug, const ScreenPos & screen_pos);




} /* namespace SlavGPS */




#endif /* _SG_VIEWPORT_H_ */
