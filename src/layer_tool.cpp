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




using namespace SlavGPS;




LayerTool::LayerTool(Window * new_window, Viewport * new_viewport, LayerType new_layer_type)
{
	this->window = new_window;
	this->viewport = new_viewport;
	this->layer_type = new_layer_type;

	if (layer_type == LayerType::NUM_TYPES) {
		strcpy(this->debug_string, "LayerType::generic");
	} else {
		strcpy(this->debug_string, "LayerType::");
		strcpy(this->debug_string + 11, Layer::get_type_id_string(layer_type).toUtf8().constData());
	}
}




LayerTool::~LayerTool()
{
	delete this->cursor_click;
	delete this->cursor_release;

	delete this->layer_edit_info; /* This may not be the best place to do this delete (::layer_edit_info is alloced in subclasses)... */
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




void LayerTool::perform_selection(const ScreenPos & screen_pos)
{
	assert (this->layer_edit_info);

	/* We have clicked on an item, and we are holding it.
	   We will hold it during move, until we release it. */
	qDebug() << "----------------------------" << __FUNCTION__ << "holding = true";
	this->layer_edit_info->holding = true;

	/* We have just clicked the item, we aren't moving the cursor yet. */
	this->layer_edit_info->moving = false;

	//#ifdef K_FIXME_RESTORE
	/* What was this supposed to do? */
	//gdk_gc_set_function(this->layer_edit_info->pen, GDK_INVERT);
	this->viewport->draw_rectangle(this->layer_edit_info->pen, screen_pos.x - 3, screen_pos.y - 3, 6, 6);
	this->viewport->sync();
	//#endif

	this->layer_edit_info->old_screen_pos = screen_pos;
}




void LayerTool::perform_move(const ScreenPos & new_pos)
{
	assert (this->layer_edit_info);

	/* We are in the process of moving the pressed cursor. */
	this->layer_edit_info->moving = true;

#ifdef K_FIXME_RESTORE
	/* What was this supposed to do? */
	this->viewport->draw_rectangle(this->layer_edit_info->pen, this->layer_edit_info->old_screen_pos.x - 3, this->layer_edit_info->old_screen_pos.y - 3, 6, 6);
	this->viewport->draw_rectangle(this->layer_edit_info->pen, new_pos.x - 3, new_pos.y - 3, 6, 6);
#endif

	this->layer_edit_info->old_screen_pos = new_pos;

	if (tool_sync_done) {
		this->viewport->sync();
		tool_sync_done = true;
	}
}




void LayerTool::perform_release(void)
{
	assert (this->layer_edit_info);

#ifdef K_FIXME_RESTORE
	/* What was this supposed to do? */
	this->viewport->draw_rectangle(this->layer_edit_info->pen, this->layer_edit_info->old_screen_pos.x - 3, this->layer_edit_info->old_screen_pos.y - 3, 6, 6);
#endif

	qDebug() << "----------------------------" << __FUNCTION__ << "holding = false";
	this->layer_edit_info->holding = false;
	this->layer_edit_info->moving = false;
}




LayerEditInfo::LayerEditInfo()
{
	this->pen.setColor(QColor("black"));
	this->pen.setWidth(2);
}
