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




#define SG_MODULE "Layer Tool"




LayerTool::LayerTool(Window * new_window, GisViewport * new_gisview, LayerKind layer_kind)
{
	this->window = new_window;
	this->gisview = new_gisview;
	this->m_layer_kind = layer_kind;

	if (this->m_layer_kind == LayerKind::Max) {
		strcpy(this->debug_string, "LayerKind::generic");
	} else {
		strcpy(this->debug_string, "LayerKind::");
		strcpy(this->debug_string + 11, Layer::get_fixed_layer_kind_string(this->m_layer_kind).toUtf8().constData());
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




bool LayerTool::activate_tool(void)
{
	if (this->m_layer_kind == LayerKind::Max) {
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




LayerTool::Status LayerTool::handle_mouse_click_wrapper(Layer * layer, QMouseEvent * event)
{
	this->gisview->setCursor(this->cursor_click);
	return this->handle_mouse_click(layer, event);
}




LayerTool::Status LayerTool::handle_mouse_double_click_wrapper(Layer * layer, QMouseEvent * event)
{
	this->gisview->setCursor(this->cursor_click);
	return this->handle_mouse_double_click(layer, event);
}




LayerTool::Status LayerTool::handle_mouse_move_wrapper(Layer * layer, QMouseEvent * event)
{
	return this->handle_mouse_move(layer, event);
}




LayerTool::Status LayerTool::handle_mouse_release_wrapper(Layer * layer, QMouseEvent * event)
{
	this->gisview->setCursor(this->cursor_release);
	return this->handle_mouse_release(layer, event);
}




LayerTool::Status LayerTool::handle_key_press_wrapper(Layer * layer, QKeyEvent * event)
{
	return this->handle_key_press(layer, event);
}




bool LayerTool::is_activated(void) const
{
	if (!this->qa) {
		qDebug() << SG_PREFIX_E << "QAction for" << this->get_tool_id() << "tool is NULL";
		return false;
	}
	return this->qa->isChecked();
}
