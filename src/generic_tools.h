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

#ifndef _H_GENERIC_TOOLS_H_
#define _H_GENERIC_TOOLS_H_




#include "layer.h"
#include "layer_tool.h"
#include "layer_interface.h"




namespace SlavGPS {




	class Window;
	class GisViewport;
	class Ruler;




	class GenericTools {
	public:
		static LayerToolContainer create_tools(Window * window, GisViewport * gisview);
	};


	class ZoomToRectangle {
	public:
		ZoomToRectangle();

		/*
		  Draw in viewport a rectangle that is starting at
		  position registered when ZoomToRectangle::begin()
		  was called, and that is ending in position @param
		  cursor_pos.

		  This method should be called each time the tool is
		  active and a cursor has been moved.
		*/
		sg_ret draw_rectangle(GisViewport * gisview, const ScreenPos & cursor_pos);

		/* To be called when zoom-to-rectangle operation is started. */
		sg_ret begin(GisViewport * gisview, const ScreenPos & cursor_pos);

		/*
		  To be called when zoom-to-rectangle operation is
		  completed (user released left mouse button while
		  Shift key was still pressed).

		  This method doesn't trigger update of viewport by
		  itself. It relies on consecutive call to a function
		  that will redraw the items tree in zoomed-in/out
		  viewport.
		*/
		sg_ret end(void);

		/*
		  To be called when zoom-to-rectangle operation is
		  interrupted (user released Shift key while left
		  mouse was still pressed).

		  This method may call GisViewport::update() to clean
		  up any artifacts that remained after interrupted
		  zoom.
		*/
		sg_ret abort(GisViewport * gisview);

		bool is_active(void) const { return this->m_is_active; }

		/* These event coordinates indicate pixel in Qt's
		   coordinate system, where beginning is in top-left
		   corner of screen. */
		ScreenPos m_start_pos;

	private:
		QPen m_pen;
		bool m_is_active = false;
		QPixmap m_orig_viewport_pixmap; /* Pixmap with saved viewport's state without "zoom to rectangle" mark. */
	};



	class GenericToolZoom : public LayerTool {
	public:
		GenericToolZoom(Window * window, GisViewport * gisview);
		~GenericToolZoom();

		SGObjectTypeID get_tool_id(void) const override;
		static SGObjectTypeID tool_id(void);

	private:
		LayerTool::Status handle_mouse_click(Layer * layer, QMouseEvent * event) override;
		LayerTool::Status handle_mouse_move(Layer * layer, QMouseEvent * event) override;
		LayerTool::Status handle_mouse_release(Layer * layer, QMouseEvent * event) override;

	private:
		ZoomToRectangle ztr;
	};



	class GenericToolRuler : public LayerTool {
	public:
		GenericToolRuler(Window * window, GisViewport * gisview);
		~GenericToolRuler();

		bool deactivate_tool(void);

		SGObjectTypeID get_tool_id(void) const override;
		static SGObjectTypeID tool_id(void);

	private:
		LayerTool::Status handle_mouse_release(Layer * layer, QMouseEvent * event) override;
		LayerTool::Status handle_mouse_move(Layer * layer, QMouseEvent * event) override;
		LayerTool::Status handle_mouse_click(Layer * layer, QMouseEvent * event) override;
		LayerTool::Status handle_key_press(Layer * layer, QKeyEvent * event) override;

		void reset_ruler(void);

		QPixmap orig_viewport_pixmap; /* Pixmap with saved viewport's state without ruler drawn on top of it. */

		Ruler * ruler = NULL;
	};


	class LayerToolPan : public LayerTool {
	public:
		LayerToolPan(Window * window, GisViewport * gisview);
		~LayerToolPan();

		SGObjectTypeID get_tool_id(void) const override;
		static SGObjectTypeID tool_id(void);

	private:
		LayerTool::Status handle_mouse_release(Layer * layer, QMouseEvent * event) override;
		LayerTool::Status handle_mouse_move(Layer * layer, QMouseEvent * event) override;
		LayerTool::Status handle_mouse_click(Layer * layer, QMouseEvent * event) override;
		LayerTool::Status handle_mouse_double_click(Layer * layer, QMouseEvent * event) override;
	};


	class LayerToolSelect : public LayerTool {
	public:
		LayerToolSelect(Window * window, GisViewport * gisview);

		/* Just for passing arguments from derived class
		   constructor to (grand)parent class constructor. */
		LayerToolSelect(Window * window, GisViewport * gisview, LayerKind layer_kind);

		~LayerToolSelect();


		enum class ObjectState {
			/* Object is not selected. Tool can't interact
			   with it.

			   The object probably is displayed in the
			   same way as any other object of its class,
			   but that is something that the tool doesn't
			   care about. */
			NotSelected,

			/* Object is selected by the tool. Tool can
			   interact with the object, but doesn't do it
			   just yet.

			   The object probably is displayed
			   differently than other (non-selected)
			   objects of its class. */
			IsSelected,

			/* Object is held. Tool is interacting with
			   the object right now, most probably tool is
			   dragging the object during mouse move.

			   The object probably is displayed
			   differently than other (non-selected
			   objects of its class, and additionally it
			   is drawn in new positions as it is moved
			   around. */
			IsHeld,
		};

		/**
		   Does the object state, remembered by the tool,
		   indicate that the object can be moved?

		   The tool surely can't move an object that is
		   NotSelected, and it surely can't move object that
		   only IsSelected. The object must be in state IsHeld
		   to be moved.
		*/
		bool can_tool_move_object(void);

		SGObjectTypeID get_tool_id(void) const override;
		static SGObjectTypeID tool_id(void);

		SGObjectTypeID selected_tree_item_type_id;
		ObjectState edited_object_state = ObjectState::NotSelected;

	private:
		LayerTool::Status handle_mouse_click(Layer * layer, QMouseEvent * event) override;
		LayerTool::Status handle_mouse_double_click(Layer * layer, QMouseEvent * event) override;
		LayerTool::Status handle_mouse_move(Layer * layer, QMouseEvent * event) override;
		LayerTool::Status handle_mouse_release(Layer * layer, QMouseEvent * event) override;

		void handle_mouse_click_common(Layer * layer, QMouseEvent * event);

		/* When a mouse click happens and some layer handles the click,
		   it's possible to start to move the selected item belonging to the layer. */
		bool select_and_move_activated = false;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _H_GENERIC_TOOLS_H_ */
