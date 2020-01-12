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




#include <QAction>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QCursor>
#include <QString>




#include "viewport_internal.h"




namespace SlavGPS {




	class Window;
	class GisViewport;
	class Layer;
	enum class LayerKind;



	class LayerTool {

	public:
		LayerTool(Window * window, GisViewport * gisview, LayerKind layer_kind);
		virtual ~LayerTool();

		/*
		  I think most of these are ignored, returning GRAB_FOCUS grabs the
		  focus for mouse move, mouse click, release always grabs
		  focus. Focus allows key presses to be handled.

		  It used to be that, if ignored, Viking could look for other layers.
		  this was useful for clicking a way/trackpoint in any layer, if no
		  layer was selected (find way/trackpoint).
		*/
		enum class Status {
			Ignored = 0,
			Handled,
			Error,
			HandledGrabFocus, /* Only for move. */
		};

		QString get_description(void) const;

		LayerTool::Status handle_mouse_click_wrapper(Layer * layer, QMouseEvent * event);
		LayerTool::Status handle_mouse_double_click_wrapper(Layer * layer, QMouseEvent * event);
		LayerTool::Status handle_mouse_move_wrapper(Layer * layer, QMouseEvent * event);
		LayerTool::Status handle_mouse_release_wrapper(Layer * layer, QMouseEvent * event);
		LayerTool::Status handle_key_press_wrapper(Layer * layer, QKeyEvent * event); /* TODO_LATER: where do we call this function? */

		/* Return true if tool has been successfully activated.
		   Return false otherwise. */
		virtual bool activate_tool(void);

		virtual bool deactivate_tool(void);

		/* Is the tool activated? / Is the button related to the tool pressed? */
		bool is_activated(void) const;

		virtual SGObjectTypeID get_tool_id(void) const = 0;


		QString action_icon_path;
		QString action_label;
		QString action_tooltip;
		QKeySequence action_accelerator;
		QAction * qa = NULL;

		bool pan_handler = false; /* Call click & release funtions even when 'Pan Mode' is on. */

		/* Default cursors will be provided by LayerTool base
		   class. */
		QCursor cursor_click;
		QCursor cursor_release;

		Window * window = NULL;
		GisViewport * gisview = NULL;


		LayerKind m_layer_kind; /* Can be set to LayerKind::Max to indicate "generic" (non-layer-specific) tool (zoom, select, etc.). */

		char debug_string[100]; /* For debug purposes only. */

	protected:
		virtual LayerTool::Status handle_mouse_click(__attribute__((unused)) Layer * layer, __attribute__((unused)) QMouseEvent * event)        { return LayerTool::Status::Ignored; }
		virtual LayerTool::Status handle_mouse_double_click(__attribute__((unused)) Layer * layer, __attribute__((unused)) QMouseEvent * event) { return LayerTool::Status::Ignored; }
		virtual LayerTool::Status handle_mouse_move(__attribute__((unused)) Layer * layer, __attribute__((unused)) QMouseEvent * event)         { return LayerTool::Status::Ignored; }
		virtual LayerTool::Status handle_mouse_release(__attribute__((unused)) Layer * layer, __attribute__((unused)) QMouseEvent * event)      { return LayerTool::Status::Ignored; }
		virtual LayerTool::Status handle_key_press(__attribute__((unused)) Layer * layer, __attribute__((unused)) QKeyEvent * event)            { return LayerTool::Status::Ignored; }
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_TOOL_H_ */
