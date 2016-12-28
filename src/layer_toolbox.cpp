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




/*
  Toolbox for tools that can operate on layers.
  There are generic tools, not tied to any specific layer.
  There are also layer-specific tools.
*/




#include <cassert>

#include <QAction>

#include "window.h"
#include "layer_toolbox.h"




using namespace SlavGPS;




LayerToolbox::~LayerToolbox()
{
	for (unsigned int tt = 0; tt < this->n_tools; tt++) {
		delete this->tools[tt];
	}
}




QAction * LayerToolbox::add_tool(LayerTool * layer_tool)
{
#if 0
	toolbar_action_tool_entry_register(this->window->viking_vtb, &layer_tool->radioActionEntry);
#endif
	QAction * qa = new QAction(layer_tool->action_label, this->window);

	qa->setObjectName(layer_tool->id_string);
	qDebug() << "DD: Layer Tools: Created qaction with name" << qa->objectName() << qa;
	qa->setIcon(QIcon(layer_tool->action_icon_path));
	qa->setCheckable(true);

	this->tools.push_back(layer_tool);
	this->n_tools++;

	return qa;
}




LayerTool * LayerToolbox::get_tool(QString const & tool_id)
{
	for (unsigned i = 0; i < this->n_tools; i++) {
		if (tool_id == this->tools[i]->id_string) {
			return this->tools[i];
		}
	}
	return NULL;
}




void LayerToolbox::activate_tool(QAction * qa)
{
	QString tool_id = qa->objectName();
	LayerTool * tool = this->get_tool(tool_id);
	Layer * layer = this->window->layers_panel->get_selected_layer();
#if 0
	if (!layer) {
		return;
	}
#endif

	if (!tool) {
		qDebug() << "EE: Layer Tools: trying to activate a non-existent tool" << tool_id;
		return;
	}
	/* Is the tool already active? */
	if (this->active_tool == tool) {
		assert (this->active_tool_qa == qa);
		return;
	}

	if (this->active_tool) {
		this->active_tool->deactivate_(NULL);
	}
	qDebug() << "II: Layer Tools: activating tool" << tool_id;
	tool->activate_(layer);
	this->active_tool = tool;
	this->active_tool_qa = qa;
}





bool LayerToolbox::deactivate_tool(QAction * qa)
{
	QString tool_id = qa->objectName();
	LayerTool * tool = this->get_tool(tool_id);
	if (!tool) {
		qDebug() << "EE: Layer Tools: trying to deactivate a non-existent tool" << tool_id;
		return true;
	}

	qDebug() << "II: Layer Tools: deactivating tool" << tool_id;

	assert (this->active_tool);

	tool->deactivate_(NULL);
	qa->setChecked(false);

	this->active_tool = NULL;
	this->active_tool_qa = NULL;
	return false;
}




/* A new layer is selected. Update state of tool groups in tool box accordingly. */
void LayerToolbox::selected_layer(QString const & group_name)
{
	for (auto group = this->action_groups.begin(); group != this->action_groups.end(); ++group) {

		QString name((*group)->objectName());

		if (group_name == name) {
			/* This is a group for our newly selected layer. It should become enabled. */
			if ((*group)->isEnabled()) {
				/* The group is already enabled, other groups are already disabled.
				   Nothing more to do in this function. */
				break;
			} else {
				qDebug() << "II: Layer Tool Box: enabling tool group '" << name << "'";
				(*group)->setEnabled(true);
			}
		} else if ("generic" == name) {
			/* This group is always enabled, and should never be disabled. */
			continue;
		} else {
			/* Group other than "group_name". Disable. */
			if ((*group)->isEnabled()) {
				qDebug() << "II: Layer Tool Box: disabling tool group '" << name << "'";
				(*group)->setEnabled(false);
			}
		}
	}
}




/**
   Enable all buttons in given actions group

   If group is non-empty, return first action in that group.
*/
QAction * LayerToolbox::set_group_enabled(QString const & group_name)
{
	QActionGroup * group = this->get_group(group_name);
	if (!group) {
		/* This may a valid situation for layers without tools, e.g. Aggregate. */
		qDebug() << "WW: Layer Tools: can't find group" << group_name << "to enable";
		return NULL;
	}

	qDebug() << "II: Layer Tools: setting group" << group_name << "enabled";
	group->setEnabled(true);

	/* Return currently selected tool (if any is selected). */
	QAction * returned = group->checkedAction();
	if (returned) {
		qDebug() << "II: Layer Tools: returning selected action" << returned->objectName() << "from group" << group_name;
		return returned;
	}

	/* Return first tool from toolbox (even if not selected. */
	if (!group->actions().empty()) {
		qDebug() << "II: Layer Tools: returning first action" << (*group->actions().constBegin())->objectName() << "from group" << group_name;
		return *group->actions().constBegin();
	}

	qDebug() << "WW: Layer Tools: returning NULL action";
	return NULL;
}




/**
   Find group by object name
*/
QActionGroup * LayerToolbox::get_group(QString const & group_name)
{
	for (auto group = this->action_groups.begin(); group != this->action_groups.end(); ++group) {
		if ((*group)->objectName() == group_name) {
			return (*group);
		}
	}

	return NULL;
}




QAction * LayerToolbox::get_active_tool_action(void)
{
	return this->active_tool_qa;
}




LayerTool * LayerToolbox::get_active_tool(void)
{
	return this->active_tool;
}




void LayerToolbox::add_group(QActionGroup * group)
{
	this->action_groups.push_back(group);
}




QCursor const * LayerToolbox::get_cursor_click(QString const & tool_id)
{
	return this->get_tool(tool_id)->cursor_release;
}




QCursor const * LayerToolbox::get_cursor_release(QString const & tool_id)
{
	return this->get_tool(tool_id)->cursor_release;
}




void LayerToolbox::click(QMouseEvent * event)
{
	Layer * layer = this->window->layers_panel->get_selected_layer();
	if (!layer) {
		qDebug() << "EE: Layer Tools: click received, no layer";
		return;
	}
	qDebug() << "II: Layer Tools: click received, selected layer" << layer->debug_string;


	if (!this->active_tool) {
		qDebug() << "EE: Layer Tools: click received, no active tool";
		return;
	}


	LayerType layer_tool_type = this->active_tool->layer_type;
	if (layer_tool_type != layer->type                   /* Click received for layer other than current layer. */
	    && layer_tool_type != LayerType::NUM_TYPES) {    /* Click received for something other than generic tool. */

		qDebug() << "EE: Layer Tools: click received, invalid type";
		return;
	}

	qDebug() << "II: Layer Tools: click received, will pass it to tool" << this->active_tool->id_string << "for layer" << layer->debug_string;
	this->active_tool->viewport->setCursor(*this->active_tool->cursor_click); /* TODO: move this into click() method. */
	this->active_tool->click_(layer, event);

	return;
}




void LayerToolbox::double_click(QMouseEvent * event)
{
	Layer * layer = this->window->layers_panel->get_selected_layer();
	if (!layer) {
		qDebug() << "EE: Layer Tools: double click received, no layer";
		return;
	}
	qDebug() << "II: Layer Tools: double click received, selected layer" << layer->debug_string;


	if (!this->active_tool) {
		qDebug() << "EE: Layer Tools: click received, no active tool";
		return;
	}


	LayerType layer_tool_type = this->active_tool->layer_type;
	if (layer_tool_type != layer->type                   /* Click received for layer other than current layer. */
	    && layer_tool_type != LayerType::NUM_TYPES) {    /* Click received for something other than generic tool. */

		qDebug() << "EE: Layer Tools: double click received, invalid type";
		return;
	}


	qDebug() << "II: Layer Tools: double click received, will pass it to tool" << this->active_tool->id_string << "for layer" << layer->debug_string;
	this->active_tool->viewport->setCursor(*this->active_tool->cursor_click); /* TODO: move this into click() method. */
	this->active_tool->double_click_(layer, event);

	return;
}




void LayerToolbox::move(QMouseEvent * event)
{
	Layer * layer = this->window->layers_panel->get_selected_layer();
	if (!layer) {
		qDebug() << "EE: Layer Tools: click received, no layer";
		return;
	}
	qDebug() << "II: Layer Tools: move received, selected layer" << layer->debug_string;


	if (!this->active_tool) {
		qDebug() << "EE: Layer Tools: move received, no active tool";
		return;
	}


	LayerType layer_tool_type = this->active_tool->layer_type;
	if (layer_tool_type != layer->type                   /* Click received for layer other than current layer. */
	    && layer_tool_type != LayerType::NUM_TYPES) {    /* Click received for something other than generic tool. */

		qDebug() << "EE: Layer Tools: double click received, invalid type";
		return;
	}


	qDebug() << "II: Layer Tools: move received, passing to tool" << this->active_tool->get_description();

	if (LayerToolFuncStatus::ACK_GRAB_FOCUS == this->active_tool->move_(layer, event)) {
#if 0
		gtk_widget_grab_focus(this->window->viewport);
#endif
	}

	return;
}




void LayerToolbox::release(QMouseEvent * event)
{
	Layer * layer = this->window->layers_panel->get_selected_layer();
	if (!layer) {
		qDebug() << "EE: Layer Tools: release received, no layer";
		return;
	}
	qDebug() << "II: Layer Tools: release received, selected layer" << layer->debug_string;


	if (!this->active_tool) {
		qDebug() << "EE: Layer Tools: release received, no active tool";
		return;
	}


	LayerType layer_tool_type = this->active_tool->layer_type;
	if (layer_tool_type != layer->type                   /* Click received for layer other than current layer. */
	    && layer_tool_type != LayerType::NUM_TYPES) {    /* Click received for something other than generic tool. */

		qDebug() << "EE: Layer Tools: release received, invalid type";
		return;
	}


	qDebug() << "II: Layer Tools: release received, will pass it to tool" << this->active_tool->id_string << "for layer" << layer->debug_string;
	this->active_tool->viewport->setCursor(*this->active_tool->cursor_release); /* TODO: move this into release() method. */
	this->active_tool->release_(layer, event);

	return;
}
