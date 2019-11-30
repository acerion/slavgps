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
#include <QDebug>




#include "window.h"
#include "toolbox.h"
#include "layer_tool.h"
#include "layers_panel.h"
#include "viewport_internal.h"
#include "globals.h"




using namespace SlavGPS;




#define SG_MODULE "ToolBox"




Toolbox::~Toolbox()
{
	for (auto iter = this->tools.begin(); iter != this->tools.end(); iter++) {
		delete iter->second;
	}
}




QActionGroup * Toolbox::add_tools(LayerToolContainer * new_tools)
{
	if (!new_tools) {
		qDebug() << SG_PREFIX_E << "NULL tools container";
		return NULL;
	}

	QActionGroup * group = new QActionGroup(this->window);

	for (auto iter = new_tools->begin(); iter != new_tools->end(); iter++) {

		LayerTool * tool = iter->second;

		QAction * qa = new QAction(tool->action_label, this->window);
		qa->setObjectName(tool->get_tool_id().m_val);
		qa->setIcon(QIcon(tool->action_icon_path));
		qa->setCheckable(true);

		tool->qa = qa;

		group->addAction(qa);
	}

	/* We should now get our own copies of Tools - we will become their owner.
	   The container will be deleted by caller of this function. */
	this->tools.insert(new_tools->begin(), new_tools->end());

	this->action_groups.push_back(group);

	return group;
}




LayerTool * Toolbox::get_tool(const SGObjectTypeID & tool_id) const
{
	auto iter = this->tools.find(tool_id);
	if (iter == this->tools.end()) {
		qDebug() << SG_PREFIX_E << "Failed to find tool with id =" << tool_id;
		return NULL;
	} else {
		return iter->second;
	}
}




void Toolbox::activate_tool_by_id(const SGObjectTypeID & tool_id)
{
	LayerTool * tool = this->get_tool(tool_id);
	if (NULL == tool) {
		qDebug() << SG_PREFIX_E << "Trying to activate a non-existent tool with id =" << tool_id;
		return;
	}

	if (this->active_tool) {
		if (this->active_tool == tool) {
			/* Don't re-activate the same tool. */
			return;
		}
		this->active_tool->deactivate_tool();
	}

	qDebug() << SG_PREFIX_I << "activating tool" << tool_id;

	if (tool->activate_tool()) {
		this->active_tool = tool;
		this->active_tool_qa = tool->qa;
		this->active_tool->qa->setChecked(true);
	}

	return;
}





void Toolbox::deactivate_tool_by_id(const SGObjectTypeID & tool_id)
{
	LayerTool * tool = this->get_tool(tool_id);
	if (NULL == tool) {
		qDebug() << SG_PREFIX_E << "Can't find tool with id =" << tool_id;
		return;
	}

	if (this->active_tool != tool) {
		qDebug() << SG_PREFIX_W << "Trying to deactivate inactive tool with id =" << tool_id;
		return;
	}

	tool->deactivate_tool();
	tool->qa->setChecked(false);
	this->active_tool = NULL;
	this->active_tool_qa = NULL;

	return;
}



void Toolbox::deactivate_current_tool(void)
{
	if (this->active_tool) {
		this->deactivate_tool_by_id(this->active_tool->get_tool_id());
	}
	this->active_tool = NULL;
	this->active_tool_qa = NULL;
}




/* May return NULL */
const LayerTool * Toolbox::get_current_tool(void) const
{
	return this->active_tool;
}




/* Called when user selects a tree item in tree view.  A new tree item
   is selected. Update state of tool groups in tool box accordingly so
   that tools relevant to this tree item are active, and all other
   tree items are inactive. */
void Toolbox::activate_tools_group(QString const & group_name)
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
				qDebug() << SG_PREFIX_I << "enabling tool group '" << name << "'";
				(*group)->setEnabled(true);
			}
		} else if ("generic" == name) {
			/* This group is always enabled, and should never be disabled. */
			continue;
		} else {
			/* Group other than "group_name". Disable. */
			if ((*group)->isEnabled()) {
				qDebug() << SG_PREFIX_I << "disabling tool group '" << name << "'";
				(*group)->setEnabled(false);
			}
		}
	}
}




/**
   Enable all buttons in given actions group

   If group is non-empty, return first action in that group.
*/
QAction * Toolbox::set_group_enabled(QString const & group_name)
{
	QActionGroup * group = this->get_group(group_name);
	if (!group) {
		/* This may a valid situation for layers without tools, e.g. Aggregate. */
		qDebug() << SG_PREFIX_W << "can't find group" << group_name << "to enable";
		return NULL;
	}

	qDebug() << SG_PREFIX_I << "enabling tools group" << group_name;
	group->setEnabled(true);

	/* Return currently selected tool (if any is selected). */
	QAction * returned = group->checkedAction();
	if (returned) {
		qDebug() << SG_PREFIX_I << "returning selected action" << returned->objectName() << "from group" << group_name;
		return returned;
	}

	/* Return first tool from toolbox (even if not selected. */
	if (!group->actions().empty()) {
		qDebug() << SG_PREFIX_I << "returning first action" << (*group->actions().constBegin())->objectName() << "from group" << group_name;
		return *group->actions().constBegin();
	}

	qDebug() << SG_PREFIX_W << "returning NULL action";
	return NULL;
}




/**
   Find group by object name
*/
QActionGroup * Toolbox::get_group(QString const & group_name)
{
	for (auto group = this->action_groups.begin(); group != this->action_groups.end(); ++group) {
		if ((*group)->objectName() == group_name) {
			return (*group);
		}
	}

	return NULL;
}




QAction * Toolbox::get_active_tool_action(void)
{
	return this->active_tool_qa;
}




LayerTool * Toolbox::get_active_tool(void)
{
	return this->active_tool;
}




/* A common set of boring tests done before passing a layer to a
   toolbox is possible/valid. */
Layer * Toolbox::handle_mouse_event_common(void)
{
	if (!this->active_tool) {
		qDebug() << SG_PREFIX_E << "No active tool";
		return NULL;
	}

	Layer * layer = this->window->items_tree->get_selected_layer();
	if (!layer) {
		qDebug() << SG_PREFIX_E << "No layer";
		return NULL;
	}


	if (this->active_tool->m_layer_kind != layer->m_kind           /* Click received for layer kind other than current layer's kind. */
	    && this->active_tool->m_layer_kind != LayerKind::Max) {    /* Click received for something other than generic tool. */

		qDebug() << SG_PREFIX_E << "Layer kind" << this->active_tool->m_layer_kind << "does not match";
		return NULL;
	}

	return layer;
}




void Toolbox::handle_mouse_click(QMouseEvent * event)
{
	Layer * layer = this->handle_mouse_event_common();
	if (NULL == layer) {
		return;
	}

	qDebug() << SG_PREFIX_I << "Passing layer" << layer->debug_string << "to tool" << this->active_tool->get_tool_id();
	this->active_tool->handle_mouse_click(layer, event);

	return;
}




void Toolbox::handle_mouse_double_click(QMouseEvent * event)
{
	Layer * layer = this->handle_mouse_event_common();
	if (NULL == layer) {
		return;
	}

	qDebug() << SG_PREFIX_I << "Passing layer" << layer->debug_string << "to tool" << this->active_tool->get_tool_id();
	this->active_tool->handle_mouse_double_click(layer, event);

	return;
}




void Toolbox::handle_mouse_move(QMouseEvent * event)
{
	Layer * layer = this->handle_mouse_event_common();
	if (NULL == layer) {
		return;
	}

	if (ToolStatus::AckGrabFocus == this->active_tool->handle_mouse_move(layer, event)) {
		this->window->get_main_gis_view()->setFocus();
	}

	return;
}




void Toolbox::handle_mouse_release(QMouseEvent * event)
{
	Layer * layer = this->handle_mouse_event_common();
	if (NULL == layer) {
		return;
	}

	qDebug() << SG_PREFIX_I << "Passing layer" << layer->debug_string << "to tool" << this->active_tool->get_tool_id();
	this->active_tool->handle_mouse_release(layer, event);

	return;
}
