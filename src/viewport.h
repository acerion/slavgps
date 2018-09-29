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
#define VIK_VIEWPORT_UTM_WRONG_ZONE -9999999
#define VIK_VIEWPORT_OFF_SCREEN_DOUBLE -9999999.9





namespace SlavGPS {




	class Viewport;




	class ViewportLogo {
	public:
		QPixmap logo_pixmap;
		QString logo_id; /* For easy comparison of logos. For Map Sources this will be map_type_string; */
	};




	/* Drawmode management. */
	enum class ViewportDrawMode {
		UTM = 0,
		Expedia,
		Mercator,
		LatLon
	};




	class ViewportDrawModes {
	public:
		static QString get_name(ViewportDrawMode mode);
		static QString get_id_string(ViewportDrawMode mode);
		static bool set_draw_mode_from_file(Viewport * viewport, const char * line);
	};




	class ScreenPos {
	public:
		ScreenPos() {};
		ScreenPos(int new_x, int new_y) : x(new_x), y(new_y) {};
		int x = 0;
		int y = 0;

		bool operator==(const ScreenPos & pos) const;

		static ScreenPos get_average(const ScreenPos & pos1, const ScreenPos & pos2);
		static bool is_close_enough(const ScreenPos & pos1, const ScreenPos & pos2, int limit);
	};




	enum {
		SG_TEXT_OFFSET_NONE = 0x00,
		SG_TEXT_OFFSET_LEFT = 0x01,
		SG_TEXT_OFFSET_UP   = 0x02
	};




} /* namespace SlavGPS */




#endif /* _SG_VIEWPORT_H_ */
