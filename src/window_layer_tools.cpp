/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2006, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2012-2015, Rob Norris <rw_norris@hotmail.com>
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




#include <cstring>

#include "window.h"
#include "window_layer_tools.h"




using namespace SlavGPS;




LayerToolsBox::~LayerToolsBox()
{
	for (int tt = 0; tt < this->n_tools; tt++) {
		delete this->tools[tt];
	}
}




int LayerToolsBox::add_tool(LayerTool * layer_tool)
{
	toolbar_action_tool_entry_register(this->window->viking_vtb, &layer_tool->radioActionEntry);

	this->tools.push_back(layer_tool);
	this->n_tools++;

	return this->n_tools;
}




LayerTool * LayerToolsBox::get_tool(char const *tool_name)
{
	for (int i = 0; i < this->n_tools; i++) {
		if (0 == strcmp(tool_name, this->tools[i]->radioActionEntry.name)) {
			return this->tools[i];
		}
	}
	return NULL;
}




void LayerToolsBox::activate(char const *tool_name)
{
	LayerTool * tool = this->get_tool(tool_name);
	Layer * layer = this->window->layers_panel->get_selected();
#if 0
	if (!layer) {
		return;
	}
#endif

	if (!tool) {
		fprintf(stderr, "CRITICAL: trying to activate a non-existent tool...\n");
		return;
	}
	/* is the tool already active? */
	if (this->active_tool == tool) {
		return;
	}

	if (this->active_tool) {
		if (this->active_tool->deactivate) {
			this->active_tool->deactivate(NULL, this->active_tool);
		}
	}
	if (tool->activate) {
		tool->activate(layer, tool);
	}
	this->active_tool = tool;
}




const GdkCursor * LayerToolsBox::get_cursor(char const *tool_name)
{
	LayerTool * tool = this->get_tool(tool_name);
	if (tool->cursor == NULL) {
		if (tool->cursor_type == GDK_CURSOR_IS_PIXMAP && tool->cursor_data != NULL) {
			GdkPixbuf *cursor_pixbuf = gdk_pixbuf_from_pixdata(tool->cursor_data, false, NULL);
			/* TODO: settable offeset */
			tool->cursor = gdk_cursor_new_from_pixbuf(gdk_display_get_default(), cursor_pixbuf, 3, 3);
			g_object_unref(G_OBJECT(cursor_pixbuf));
		} else {
			tool->cursor = gdk_cursor_new(tool->cursor_type);
		}
	}
	return tool->cursor;
}




void LayerToolsBox::click(GdkEventButton * event)
{
	Layer * layer = this->window->layers_panel->get_selected();
#if 1
	if (!layer) {
		return;
	}
#endif

	if (this->active_tool && this->active_tool->click) {
		LayerType ltype = this->active_tool->layer_type;
		if (ltype == LayerType::NUM_TYPES || (layer && ltype == layer->type)) {
			this->active_tool->click(layer, event, this->active_tool);
		}
	}
}




void LayerToolsBox::move(GdkEventMotion * event)
{
	Layer * layer = this->window->layers_panel->get_selected();
#if 1
	if (!layer) {
		return;
	}
#endif

	if (this->active_tool && this->active_tool->move) {
		LayerType ltype = this->active_tool->layer_type;
		if (ltype == LayerType::NUM_TYPES || (layer && ltype == layer->type)) {
			if (VIK_LAYER_TOOL_ACK_GRAB_FOCUS == this->active_tool->move(layer, event, this->active_tool)) {
				gtk_widget_grab_focus(this->window->viewport->get_toolkit_widget());
			}
		}
	}
}




void LayerToolsBox::release(GdkEventButton * event)
{
	Layer * layer = this->window->layers_panel->get_selected();
#if 1
	if (!layer) {
		return;
	}
#endif

	if (this->active_tool && this->active_tool->release) {
		LayerType ltype = this->active_tool->layer_type;
		if (ltype == LayerType::NUM_TYPES || (layer && ltype == layer->type)) {
			this->active_tool->release(layer, event, this->active_tool);
		}
	}
}
