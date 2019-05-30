/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2005, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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




#include <cassert>




#include <QDebug>




#include "layer.h"
#include "layer_tool.h"
#include "viewport_internal.h"
#include "window.h"
#include "layers_panel.h"




using namespace SlavGPS;




LayerTool::LayerTool(Window * new_window, Viewport * new_viewport, LayerType new_layer_type)
{
	this->window = new_window;
	this->viewport = new_viewport;
	this->layer_type = new_layer_type;

	if (layer_type == LayerType::Max) {
		strcpy(this->debug_string, "LayerType::generic");
	} else {
		strcpy(this->debug_string, "LayerType::");
		strcpy(this->debug_string + 11, Layer::get_type_id_string(layer_type).toUtf8().constData());
	}
	this->cursor_click = QCursor(Qt::ArrowCursor);
	this->cursor_release = QCursor(Qt::ArrowCursor);
}




LayerTool::~LayerTool()
{
}




/**
   @brief Return Pretty-print name of tool that can be used in UI
*/
QString LayerTool::get_description() const
{
	return this->action_tooltip;
}




/* Background drawing hook, to be passed the viewport. */
static bool tool_sync_done = true; /* TODO_LATER: get rid of this global variable. */




void LayerTool::remember_selection(const ScreenPos & screen_pos)
{
	/* We have clicked on an item, and we are holding it.
	   We will hold it during move, until we release it. */
	this->layer_edit_holding = true;

	/* We have just clicked the item, we aren't moving the cursor yet. */
	this->layer_edit_moving = false;
}




void LayerTool::perform_move(const ScreenPos & new_pos)
{
	/* We are in the process of moving the pressed cursor. */
	this->layer_edit_moving = true;

	if (tool_sync_done) {
		this->viewport->sync();
		tool_sync_done = true;
	}
}




bool LayerTool::perform_release(void)
{
	/* Did we perform release of cursor after holding cursor down or moving pressed down cursor? */
	const bool something_released = this->layer_edit_holding || this->layer_edit_moving;

	this->layer_edit_holding = false;
	this->layer_edit_moving = false;

	return something_released;
}




bool LayerTool::activate_tool(void)
{
	if (this->layer_type == LayerType::Max) {
		/* Generic tool, does not depend on any layer being selected. */
		return true;
	}
	/* Else: layer specific tool. */

	Layer * layer = this->window->items_tree->get_selected_layer();
	if (layer) {
		/* Layer specific tool has a layer that it can operate on. */
		return true;
	}

	return false;
}




bool LayerTool::deactivate_tool(void)
{
	return true;
}




ToolStatus LayerTool::handle_mouse_click(Layer * layer, QMouseEvent * event)
{
	this->viewport->setCursor(this->cursor_click);
	return this->internal_handle_mouse_click(layer, event);
}




ToolStatus LayerTool::handle_mouse_double_click(Layer * layer, QMouseEvent * event)
{
	this->viewport->setCursor(this->cursor_click);
	return this->internal_handle_mouse_double_click(layer, event);
}




ToolStatus LayerTool::handle_mouse_move(Layer * layer, QMouseEvent * event)
{
	return this->internal_handle_mouse_move(layer, event);
}




ToolStatus LayerTool::handle_mouse_release(Layer * layer, QMouseEvent * event)
{
	this->viewport->setCursor(this->cursor_release);
	return this->internal_handle_mouse_release(layer, event);
}




ToolStatus LayerTool::handle_key_press(Layer * layer, QKeyEvent * event)
{
	return this->internal_handle_key_press(layer, event);
}
