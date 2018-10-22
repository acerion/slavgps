/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2011, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_WEBTOOL_BOUNDS_H_
#define _SG_WEBTOOL_BOUNDS_H_




#include "webtool.h"




namespace SlavGPS {




	class Viewport;




	class WebToolBounds : public WebTool {
		Q_OBJECT
	public:
		WebToolBounds(const QString & tool_label, const char * new_url_format);
		~WebToolBounds();

		QString get_url_for_viewport(Viewport * viewport);
		QString get_url_at_position(Viewport * viewport, const Coord * coord);

		QString get_url_for_bbox(const LatLonBBox & bbox);

	}; /* class WebToolBounds */




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WEBTOOL_BOUNDS_H_ */
