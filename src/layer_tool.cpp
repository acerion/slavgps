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




#include "layer.h"
#include "layer_tool.h"
#include "viewport_internal.h"




using namespace SlavGPS;




LayerTool::LayerTool(Window * window_, Viewport * viewport_, LayerType layer_type_)
{
	this->window = window_;
	this->viewport = viewport_;
	this->layer_type = layer_type_;
	if (layer_type == LayerType::NUM_TYPES) {
		strcpy(this->debug_string, "LayerType::generic");
	} else {
		strcpy(this->debug_string, "LayerType::");
		strcpy(this->debug_string + 11, Layer::get_type_string(layer_type).toUtf8().constData());
	}
}




LayerTool::~LayerTool()
{
#ifdef K
	if (radioActionEntry.stock_id) {
		free((void *) radioActionEntry.stock_id);
		radioActionEntry.stock_id = NULL;
	}
	if (radioActionEntry.label) {
		free((void *) radioActionEntry.label);
		radioActionEntry.label = NULL;
	}
	if (radioActionEntry.accelerator) {
		free((void *) radioActionEntry.accelerator);
		radioActionEntry.accelerator = NULL;
	}
	if (radioActionEntry.tooltip) {
		free((void *) radioActionEntry.tooltip);
		radioActionEntry.tooltip = NULL;
	}

	if (ruler) {
		free(ruler);
		ruler = NULL;
	}
	if (zoom) {
		if (zoom->pixmap) {
			g_object_unref(G_OBJECT (zoom->pixmap));
		}

		free(zoom);
		zoom = NULL;
	}
#endif

	delete this->cursor_click;
	delete this->cursor_release;

	delete this->sublayer_edit; /* This may not be the best place to do this delete (::sublayer_edit is alloced in subclasses)... */
}




/**
   @brief Return Pretty-print name of tool that can be used in UI
*/
QString LayerTool::get_description() const
{
	return this->action_tooltip;
}




/* Background drawing hook, to be passed the viewport. */
static bool tool_sync_done = true; /* TODO: get rid of this global variable. */




void LayerTool::sublayer_edit_click(int x, int y)
{
	assert (this->sublayer_edit);

	/* We have clicked on a point, and we are holding it.
	   We hold it during move, until we release it. */
	this->sublayer_edit->holding = true;

	//gdk_gc_set_function(this->sublayer_edit->pen, GDK_INVERT);
	this->viewport->draw_rectangle(this->sublayer_edit->pen, x - 3, y - 3, 6, 6);
	this->viewport->sync();
	this->sublayer_edit->oldx = x;
	this->sublayer_edit->oldy = y;
	this->sublayer_edit->moving = false;
}




void LayerTool::sublayer_edit_move(int x, int y)
{
	assert (this->sublayer_edit);

	this->viewport->draw_rectangle(this->sublayer_edit->pen, this->sublayer_edit->oldx - 3, this->sublayer_edit->oldy - 3, 6, 6);
	this->viewport->draw_rectangle(this->sublayer_edit->pen, x - 3, y - 3, 6, 6);
	this->sublayer_edit->oldx = x;
	this->sublayer_edit->oldy = y;
	this->sublayer_edit->moving = true;

	if (tool_sync_done) {
		this->viewport->sync();
		tool_sync_done = true;
	}
}




void LayerTool::sublayer_edit_release(void)
{
	assert (this->sublayer_edit);

	this->viewport->draw_rectangle(this->sublayer_edit->pen, this->sublayer_edit->oldx - 3, this->sublayer_edit->oldy - 3, 6, 6);
	this->sublayer_edit->holding = false;
	this->sublayer_edit->moving = false;
}
