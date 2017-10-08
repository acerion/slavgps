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
 */

#ifndef _SG_LAYER_TRW_TOOLS_H_
#define _SG_LAYER_TRW_TOOLS_H_




#include "layer.h"
#include "window.h"
#include "viewport.h"




namespace SlavGPS {




/* Globally unique tool IDS. */
#define LAYER_TRW_TOOL_CREATE_WAYPOINT "sg.tool.layer_trw.create_waypoint"
#define LAYER_TRW_TOOL_CREATE_TRACK    "sg.tool.layer_trw.create_track"
#define LAYER_TRW_TOOL_CREATE_ROUTE    "sg.tool.layer_trw.create_route"
#define LAYER_TRW_TOOL_ROUTE_FINDER    "sg.tool.layer_trw.route_finder"
#define LAYER_TRW_TOOL_EDIT_WAYPOINT   "sg.tool.layer_trw.edit_waypoint"
#define LAYER_TRW_TOOL_EDIT_TRACKPOINT "sg.tool.layer_trw.edit_trackpoint"
#define LAYER_TRW_TOOL_SHOW_PICTURE    "sg.tool.layer_trw.show_picture"




	class LayerToolTRWNewWaypoint : public LayerTool {
	public:
		LayerToolTRWNewWaypoint(Window * window, Viewport * viewport);

		ToolStatus handle_mouse_click(Layer * layer, QMouseEvent * event);
	};

	class LayerToolTRWEditTrackpoint : public LayerTool {
	public:
		LayerToolTRWEditTrackpoint(Window * window, Viewport * viewport);

		ToolStatus handle_mouse_click(Layer * layer, QMouseEvent * event);
		ToolStatus handle_mouse_move(Layer * layer, QMouseEvent * event);
		ToolStatus handle_mouse_release(Layer * layer, QMouseEvent * event);
	};

	class LayerToolTRWExtendedRouteFinder : public LayerTool {
	public:
		LayerToolTRWExtendedRouteFinder(Window * window, Viewport * viewport);

		ToolStatus handle_mouse_click(Layer * layer, QMouseEvent * event);
		ToolStatus handle_mouse_move(Layer * layer, QMouseEvent * event);
		ToolStatus handle_mouse_release(Layer * layer, QMouseEvent * event);
		ToolStatus handle_key_press(Layer * layer, QKeyEvent * event);

		void undo(LayerTRW * trw, Track * track);
	};

	class LayerToolTRWShowPicture : public LayerTool {
	public:
		LayerToolTRWShowPicture(Window * window, Viewport * viewport);

		ToolStatus handle_mouse_click(Layer * layer, QMouseEvent * event);
	};

	class LayerToolTRWEditWaypoint : public LayerTool {
	public:
		LayerToolTRWEditWaypoint(Window * window, Viewport * viewport);

		ToolStatus handle_mouse_click(Layer * layer, QMouseEvent * event);
		ToolStatus handle_mouse_move(Layer * layer, QMouseEvent * event);
		ToolStatus handle_mouse_release(Layer * layer, QMouseEvent * event);
	};

	class LayerToolTRWNewTrack : public LayerTool {
	public:
		LayerToolTRWNewTrack(Window * window, Viewport * viewport);

		ToolStatus handle_mouse_click(Layer * layer, QMouseEvent * event);
		ToolStatus handle_mouse_double_click(Layer * layer, QMouseEvent * event);
		ToolStatus handle_mouse_move(Layer * layer, QMouseEvent * event);
		ToolStatus handle_mouse_release(Layer * layer, QMouseEvent * event);
		ToolStatus handle_key_press(Layer * layer, QKeyEvent * event);
	};

	class LayerToolTRWNewRoute : public LayerTool {
	public:
		LayerToolTRWNewRoute(Window * window, Viewport * viewport);

		ToolStatus handle_mouse_click(Layer * layer, QMouseEvent * event);
		ToolStatus handle_mouse_move(Layer * layer, QMouseEvent * event);
		ToolStatus handle_mouse_release(Layer * layer, QMouseEvent * event);
		ToolStatus handle_key_press(Layer * layer, QKeyEvent * event);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_TRW_TOOLS_H_ */
