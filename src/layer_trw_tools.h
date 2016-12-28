/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (c) 2011-2013, Rob Norris <rw_norris@hotmail.com>
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
 *
 */

#ifndef _SG_LAYER_TRW_TOOLS_H_
#define _SG_LAYER_TRW_TOOLS_H_




#include "layer.h"
#include "window.h"
#include "viewport.h"




SlavGPS::LayerTool * tool_edit_trackpoint_create(SlavGPS::Window * window, SlavGPS::Viewport * viewport);
SlavGPS::LayerTool * tool_show_picture_create(SlavGPS::Window * window, SlavGPS::Viewport * viewport);
SlavGPS::LayerTool * tool_edit_waypoint_create(SlavGPS::Window * window, SlavGPS::Viewport * viewport);
SlavGPS::LayerTool * tool_new_route_create(SlavGPS::Window * window, SlavGPS::Viewport * viewport);
SlavGPS::LayerTool * tool_new_track_create(SlavGPS::Window * window, SlavGPS::Viewport * viewport);
SlavGPS::LayerTool * tool_new_waypoint_create(SlavGPS::Window * window, SlavGPS::Viewport * viewport);
SlavGPS::LayerTool * tool_extended_route_finder_create(SlavGPS::Window * window, SlavGPS::Viewport * viewport);



namespace SlavGPS {




	class LayerToolTRWNewWaypoint : public LayerTool {
	public:
		LayerToolTRWNewWaypoint(Window * window, Viewport * viewport);
	};

	class LayerToolTRWEditTrackpoint : public LayerTool {
	public:
		LayerToolTRWEditTrackpoint(Window * window, Viewport * viewport);
	};

	class LayerToolTRWExtendedRouteFinder : public LayerTool {
	public:
		LayerToolTRWExtendedRouteFinder(Window * window, Viewport * viewport);
	};

	class LayerToolTRWShowPicture : public LayerTool {
	public:
		LayerToolTRWShowPicture(Window * window, Viewport * viewport);
	};

	class LayerToolTRWEditWaypoint : public LayerTool {
	public:
		LayerToolTRWEditWaypoint(Window * window, Viewport * viewport);
	};

	class LayerToolTRWNewTrack : public LayerTool {
	public:
		LayerToolTRWNewTrack(Window * window, Viewport * viewport);
	};

	class LayerToolTRWNewRoute : public LayerTool {
	public:
		LayerToolTRWNewRoute(Window * window, Viewport * viewport);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_TRW_TOOLS_H_ */
