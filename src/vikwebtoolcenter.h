/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2008, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#ifndef _SG_WEBTOOL_CENTER_H_
#define _SG_WEBTOOL_CENTER_H_




#include "vikwebtool.h"




namespace SlavGPS {




	class Window;




	class WebToolCenter : public WebTool {

	public:
		WebToolCenter();
		WebToolCenter(char const * label, char const * url_formt);
		~WebToolCenter();

		char * get_url(Window * window);
		char * get_url_at_position(Window * window, const Coord * coord);

	}; /* class WebToolCenter */




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WEBTOOL_CENTER_H_ */
