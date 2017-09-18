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

#ifndef _SG_LAYER_TOOL_H_
#define _SG_LAYER_TOOL_H_




//#include <cstdio>
//#include <cstdint>
//#include <map>

//#include <QObject>
//#include <QTreeWidgetItem>
//#include <QPersistentModelIndex>
//#include <QIcon>
#include <QAction>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QCursor>
//#include <QMenu>
#include <QString>
//#include <QPixmap>
//#include <QPen>

//#include "ui_builder.h"
//#include "tree_view.h"
//#include "viewport.h"
#include "globals.h"
//#include "vikutils.h"




namespace SlavGPS {




	class Window;
	class Viewport;
	class Layer;
	class SublayerEdit;
	enum class LayerType;




	/*
	  I think most of these are ignored, returning GRAB_FOCUS grabs the
	  focus for mouse move, mouse click, release always grabs
	  focus. Focus allows key presses to be handled.

	  It used to be that, if ignored, Viking could look for other layers.
	  this was useful for clicking a way/trackpoint in any layer, if no
	  layer was selected (find way/trackpoint).
	*/
	enum class ToolStatus {
		IGNORED = 0,
		ACK,
		ACK_REDRAW_ABOVE,
		ACK_REDRAW_ALL,
		ACK_REDRAW_IF_VISIBLE,
		ACK_GRAB_FOCUS, /* Only for move. */
	};




	class LayerTool {

	public:
		LayerTool(Window * window, Viewport * viewport, LayerType layer_type);
		virtual ~LayerTool();

		QString get_description(void) const;

		virtual ToolStatus handle_mouse_click(Layer * layer, QMouseEvent * event)        { return ToolStatus::IGNORED; }
		virtual ToolStatus handle_mouse_double_click(Layer * layer, QMouseEvent * event) { return ToolStatus::IGNORED; }
		virtual ToolStatus handle_mouse_move(Layer * layer, QMouseEvent * event)         { return ToolStatus::IGNORED; }
		virtual ToolStatus handle_mouse_release(Layer * layer, QMouseEvent * event)      { return ToolStatus::IGNORED; }
		virtual ToolStatus handle_key_press(Layer * layer, QKeyEvent * event)            { return ToolStatus::IGNORED; }; /* TODO: where do we call this function? */
		virtual void activate_tool(Layer * layer) { return; };
		virtual void deactivate_tool(Layer * layer) { return; };

		/* Start holding a TRW point. */
		void sublayer_edit_click(int x, int y);
		/* A TRW point that is being held is now also moved around. */
		void sublayer_edit_move(int x, int y);
		/* Stop holding (i.e. release) a TRW point. */
		void sublayer_edit_release(void);

		QString action_icon_path;
		QString action_label;
		QString action_tooltip;
		QKeySequence action_accelerator;
		QAction * qa = NULL;

		bool pan_handler = false; /* Call click & release funtions even when 'Pan Mode' is on. */

		QCursor const * cursor_click = NULL;
		QCursor const * cursor_release = NULL;

		Window * window = NULL;
		Viewport * viewport = NULL;

		/* This should be moved to class LayerToolTRW. */
		SublayerEdit * sublayer_edit = NULL;

		LayerType layer_type; /* Can be set to LayerType::NUM_TYPES to indicate "generic" (non-layer-specific) tool (zoom, select, etc.). */

		/* Globally unique tool ID, e.g. "sg.tool.generic.zoom", or "sg.tool.layer_dem.download".
		   For internal use, not visible to end user. */
		QString id_string;

		char debug_string[100]; /* For debug purposes only. */
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_TOOL_H_ */
