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
		qa->setObjectName(tool->id_string);
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




LayerTool * Toolbox::get_tool(QString const & tool_id)
{
	auto iter = this->tools.find(tool_id);
	if (iter == this->tools.end()) {
		return NULL;
	} else {
		return iter->second;
	}
}




void Toolbox::activate_tool_by_id(const QString & tool_id)
{
	LayerTool * new_tool = this->get_tool(tool_id);
	if (!new_tool) {
		qDebug() << SG_PREFIX_E << "Trying to activate a non-existent tool" << tool_id;
		return;
	}

	if (this->active_tool) {
		if (this->active_tool == new_tool) {
			/* Don't re-activate the same tool. */
			return;
		}
		this->active_tool->deactivate_tool();
	}

	qDebug() << SG_PREFIX_I << "activating tool" << tool_id;

	if (new_tool->activate_tool()) {
		this->active_tool = new_tool;
		this->active_tool_qa = new_tool->qa;
		this->active_tool->qa->setChecked(true);
	}

	return;
}





void Toolbox::deactivate_tool_by_id(const QString & tool_id)
{
	LayerTool * tool = this->get_tool(tool_id);
	if (!tool) {
		qDebug() << SG_PREFIX_E << "can't find tool" << tool_id;
		return;
	}

	if (this->active_tool != tool) {
		qDebug() << SG_PREFIX_W << "trying to deactivate inactive tool" << tool_id;
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
		this->deactivate_tool_by_id(this->active_tool->id_string);
	}
	this->active_tool = NULL;
	this->active_tool_qa = NULL;
}




/* May return NULL */
const LayerTool * Toolbox::get_current_tool(void) const
{
	return this->active_tool;
}




/* Called when user selects a layer in tree view.
   A new layer is selected. Update state of tool
   groups in tool box accordingly. */
void Toolbox::handle_selection_of_layer(QString const & group_name)
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




QCursor const * Toolbox::get_cursor_click(QString const & tool_id)
{
	return this->get_tool(tool_id)->cursor_release;
}




QCursor const * Toolbox::get_cursor_release(QString const & tool_id)
{
	return this->get_tool(tool_id)->cursor_release;
}



/* A common set of boring tests done before passing a layer to a
   toolbox is possible/valid. */
#define handle_mouse_event_common()					\
	if (!this->active_tool) {					\
		qDebug() << SG_PREFIX_E << "no active tool";		\
		return;							\
	}								\
									\
	Layer * layer = this->window->items_tree->get_selected_layer();	\
	if (!layer) {							\
		qDebug() << SG_PREFIX_E << "no layer";			\
		return;							\
	}								\
									\
									\
	LayerType layer_tool_type = this->active_tool->layer_type;	\
	if (layer_tool_type != layer->type             /* Click received for layer other than current layer. */	\
	    && layer_tool_type != LayerType::Max) {    /* Click received for something other than generic tool. */ \
									\
		qDebug() << SG_PREFIX_E << "layer type does not match";	\
		return;							\
	}								\






void Toolbox::handle_mouse_click(QMouseEvent * event)
{
	handle_mouse_event_common();

	qDebug() << SG_PREFIX_I << "Passing layer" << layer->debug_string << "to tool" << this->active_tool->id_string;

	this->active_tool->viewport->setCursor(*this->active_tool->cursor_click); /* TODO_2_LATER: move this into click() method. */
	this->active_tool->handle_mouse_click(layer, event);

	return;
}




void Toolbox::handle_mouse_double_click(QMouseEvent * event)
{
	handle_mouse_event_common();

	this->active_tool->viewport->setCursor(*this->active_tool->cursor_click); /* TODO_2_LATER: move this into click() method. */
	this->active_tool->handle_mouse_double_click(layer, event);

	return;
}




void Toolbox::handle_mouse_move(QMouseEvent * event)
{
	handle_mouse_event_common();

	if (ToolStatus::AckGrabFocus == this->active_tool->handle_mouse_move(layer, event)) {
#ifdef K_FIXME_RESTORE
		this->window->viewport->setFocus();
#endif
	}

	return;
}




void Toolbox::handle_mouse_release(QMouseEvent * event)
{
	handle_mouse_event_common();

	qDebug() << SG_PREFIX_I << "Passing layer" << layer->debug_string << "to tool" << this->active_tool->id_string;

	this->active_tool->viewport->setCursor(*this->active_tool->cursor_release); /* TODO_2_LATER: move this into release() method. */
	this->active_tool->handle_mouse_release(layer, event);

	return;
}
