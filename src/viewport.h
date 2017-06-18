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

#include "coord.h"
#include "bbox.h"
#include "globals.h"




#define VIK_VIEWPORT_MAX_ZOOM 32768.0
#define VIK_VIEWPORT_MIN_ZOOM (1 / 32.0)

/* Used for coord to screen etc, screen to coord. */
#define VIK_VIEWPORT_UTM_WRONG_ZONE -9999999
#define VIK_VIEWPORT_OFF_SCREEN_DOUBLE -9999999.9





namespace SlavGPS {




	/* Drawmode management. */
	enum class ViewportDrawMode {
		UTM = 0,
		EXPEDIA,
		MERCATOR,
		LATLON
	};




	enum {
		SG_TEXT_OFFSET_NONE = 0x00,
		SG_TEXT_OFFSET_LEFT = 0x01,
		SG_TEXT_OFFSET_UP   = 0x02
	};




	void viewport_init(void);



	class Viewport;
	void vik_viewport_add_copyright_cb(Viewport * viewport, QString const & copyright);




} /* namespace SlavGPS */




#endif /* _SG_VIEWPORT_H_ */
