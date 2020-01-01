/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2010-2015, Rob Norris <rw_norris@hotmail.com>
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




#include <mutex>




#include <QDebug>




#include "tree_item.h"
#include "tree_view.h"
#include "tree_view_internal.h"
#include "layers_panel.h"
#include "window.h"




using namespace SlavGPS;




#define SG_MODULE "Tree Item"




extern SelectedTreeItems g_selected;




static sg_uid_t g_uid_counter = SG_UID_INITIAL;
static std::mutex g_uid_counter_mutex;




TreeItem::TreeItem()
{
	g_uid_counter_mutex.lock();
	++g_uid_counter;
	this->uid = g_uid_counter;
	g_uid_counter_mutex.unlock();
}




TreeItem::~TreeItem()
{
	g_selected.remove_from_set(this);
}




TreeIndex const & TreeItem::get_index(void)
{
	return this->index;
}




void TreeItem::set_index(TreeIndex & i)
{
	this->index = i;
}




bool TreeItem::toggle_visible(void)
{
	this->visible = !this->visible;
	return this->visible;
}




void TreeItem::set_visible(bool new_state)
{
	this->visible = new_state;
}




bool TreeItem::is_visible_with_parents(void) const
{
	return this->tree_view->get_tree_item_visibility_with_parents(this);
}




bool TreeItem::is_visible(void) const
{
	return this->visible;
}




sg_uid_t TreeItem::get_uid(void) const
{
	return this->uid;
}




QString TreeItem::get_tooltip(void) const
{
	return tr("Tree Item");
}




/**
   @reviewed-on 2019-12-31
*/
const QString & TreeItem::get_name(void) const
{
	return this->m_name;
}




/**
   @reviewed-on 2019-12-31
*/
void TreeItem::set_name(const QString & name)
{
	this->m_name = name;
}




/**
   @reviewed-on 2019-12-29
*/
Layer * TreeItem::get_immediate_layer(void)
{
	/* Default behaviour for Waypoints, Tracks, Routes, their
	   containers and other non-Layer items. This behaviour will
	   be overriden for Layer class and its derived classes. */
	return this->owning_layer;
}




Layer * TreeItem::get_owning_layer(void) const
{
	return this->owning_layer;
}




void TreeItem::set_owning_layer(Layer * layer)
{
	this->owning_layer = layer;
}




TreeItem * TreeItem::get_direct_parent_tree_item(void) const
{
	return this->m_direct_parent_tree_item;
}




bool TreeItem::the_same_object(const TreeItem * item1, const TreeItem * item2)
{
	if (NULL == item1 || NULL == item2) {
		return false;
	}

	return item1->uid == item2->uid;
}




bool TreeItem::is_in_tree(void) const
{
	if (nullptr == this->tree_view) {
		return false;
	}

	if (!this->index.isValid()) {
		return false;
	}

	return true;
}




bool TreeItem::is_layer(void) const
{
	return false;
}




bool TreeItem::compare_name_ascending(const TreeItem * a, const TreeItem * b)
{
	return (a->m_name < b->m_name);
}




bool TreeItem::compare_name_descending(const TreeItem * a, const TreeItem * b)
{
	return !TreeItem::compare_name_ascending(a, b);
}




/**
   @reviewed-on 2019-12-31
*/
const StandardMenuOperations & TreeItem::get_menu_operation_ids(void) const
{
	return this->m_menu_operation_ids;
}




/**
   @reviewed-on 2019-12-31
*/
void TreeItem::set_menu_operation_ids(const StandardMenuOperations & ops)
{
	this->m_menu_operation_ids = ops;
}




sg_ret TreeItem::menu_add_standard_operations(QMenu & menu, const StandardMenuOperations & ops, bool in_tree_view)
{
	LayersPanel * layers_panel = ThisApp::get_layers_panel();
	return layers_panel->context_menu_add_standard_operations(menu, ops);
}




sg_ret TreeItem::show_context_menu(const QPoint & position, bool in_tree_view, QWidget * parent)
{
	qDebug() << SG_PREFIX_I << "Context menu for" << this->m_type_id << this->get_name();
	QMenu menu(parent);


	/* First add standard operations. */
	StandardMenuOperations ops = this->get_menu_operation_ids();
	if (in_tree_view) {
		ops.push_back(StandardMenuOperation::New);
	}
	if (ops.size() > 0) {
		if (sg_ret::ok != this->menu_add_standard_operations(menu, ops, in_tree_view)) {
			return sg_ret::err;
		}
		menu.addSeparator();
	}


	/* Now add type-specific operations below */
	if (sg_ret::ok != this->menu_add_type_specific_operations(menu, in_tree_view)) {
		return sg_ret::err;
	}


	menu.exec(position);
	return sg_ret::ok;
}




QList<QStandardItem *> TreeItem::get_list_representation(const TreeItemViewFormat & view_format)
{
	// http://www.qtforum.org/article/34069/store-user-data-void-with-qstandarditem-in-qstandarditemmodel.html

	QList<QStandardItem *> items;
	QVariant variant;

	const QString tooltip = this->get_tooltip();

	for (const TreeItemViewColumn & col : view_format.columns) {

		QStandardItem * item = NULL;

		switch (col.id) {
		case TreeItemPropertyID::TheItem:
			item = new QStandardItem(this->get_name());
			item->setToolTip(tooltip);
			item->setEditable(this->editable);
			variant = QVariant::fromValue(this);
			item->setData(variant, RoleLayerData);
			if (!this->icon.isNull()) { /* Icon can be set with ::apply_tree_item_icon(). */
				item->setIcon(this->icon);
			}
			//item->moveToThread(QApplication::instance()->thread())
			items << item;
			break;

		case TreeItemPropertyID::Visibility:
			item = new QStandardItem();
			item->setCheckable(true);
			item->setCheckState(this->is_visible() ? Qt::Checked : Qt::Unchecked);
			//item->moveToThread(QApplication::instance()->thread())
			items << item;
			break;


		case TreeItemPropertyID::Editable:
			item = new QStandardItem();
			variant = QVariant::fromValue(this->editable);
			item->setData(variant, RoleLayerData);
			//item->moveToThread(QApplication::instance()->thread())
			items << item;
			break;

		case TreeItemPropertyID::Timestamp:
			/* Value in this column can be set with ::apply_tree_item_timestamp(). */
			/* Don't remove the check for
			   validity. Invalid value passed to
			   QStandardItem() may crash the program. */
			if (this->timestamp.is_valid()) {
				item = new QStandardItem(this->timestamp.get_ll_value());
			} else {
				item = new QStandardItem(0);
			}
			//item->moveToThread(QApplication::instance()->thread())
			items << item;
			break;
		default:
			qDebug() << SG_PREFIX_N << "Unexpected tree item column id" << (int) col.id;
			break;
		}
	}

	return items;
}




void TreeItem::display_debug_info(const QString & reference) const
{
	return;
}




sg_ret TreeItem::drag_drop_request(TreeItem * tree_item, int row, int col)
{
	qDebug() << SG_PREFIX_E << "Can't drop tree item" << tree_item->get_name() << "here";
	return sg_ret::err;
}




/**
   @reviewed-on 2019-12-22
*/
bool TreeItem::dropped_item_is_acceptable(const TreeItem & tree_item) const
{
	auto iter = std::find(this->accepted_child_type_ids.begin(), this->accepted_child_type_ids.end(), tree_item.get_type_id());
	if (iter != this->accepted_child_type_ids.end()) {
		qDebug() << SG_PREFIX_I << "Accepted child type ids =" << this->accepted_child_type_ids << ", dropped item type id =" << tree_item.m_type_id;
		return true;
	} else {
		return false;
	}
}




Time TreeItem::get_timestamp(void) const
{
	return this->timestamp; /* Returned value may be invalid. */
}




void TreeItem::set_timestamp(const Time & value)
{
	this->timestamp = value;
}




void TreeItem::set_timestamp(time_t value)
{
	this->timestamp = Time(value, Time::get_internal_unit());
}




bool TreeItem::move_child(TreeItem & child_tree_item, bool up)
{
	return false;
}




/**
   Indicate to receiver that specified tree item has changed (if the item is visible)
*/
void TreeItem::emit_tree_item_changed(const QString & where)
{
	if (this->visible && this->tree_view) {
		ThisApp::get_main_window()->set_redraw_trigger(this);
		qDebug() << SG_PREFIX_SIGNAL << "Tree item" << this->m_name << "emits 'layer changed' signal @" << where;
		emit this->tree_item_changed(this->m_name);
	}
}




/**
   Indicate to receiver that specified tree item has changed (even if the item is not)

   Should only be done by LayersPanel (hence never used from the background)
   need to redraw and record trigger when we make a layer invisible.
*/
void TreeItem::emit_tree_item_changed_although_invisible(const QString & where)
{
	ThisApp::get_main_window()->set_redraw_trigger(this);
	qDebug() << SG_PREFIX_SIGNAL << "TreeItem" << this->m_name << "emits 'changed' signal @" << where;
	emit this->tree_item_changed(this->m_name);
}




sg_ret TreeItem::click_in_tree(const QString & debug)
{
	QStandardItem * item = this->tree_view->get_tree_model()->itemFromIndex(this->index);
	if (NULL == item) {
		qDebug() << SG_PREFIX_E << "Failed to get qstandarditem for" << this->m_name;
		return sg_ret::err;
	}

	this->tree_view->select_and_expose_tree_item(this);

#if 0
	this->tree_view->selectionModel()->select(item->index(), QItemSelectionModel::ClearAndSelect);
#endif

#if 0
	qDebug() << SG_PREFIX_SIGNAL << "Will emit 'clicked' signal for tree item" << this->name << ":" << debug;
	emit this->tree_view->clicked(item->index());
#endif

	return sg_ret::ok;
}




sg_ret TreeItem::get_tree_items(std::list<TreeItem *> & list, const std::list<SGObjectTypeID> & wanted_types) const
{
	return sg_ret::ok;
}




sg_ret TreeItem::attach_to_tree_under_parent(TreeItem * parent, TreeViewAttachMode attach_mode, const TreeItem * sibling)
{
	if (!parent->is_in_tree()) {
		qDebug() << SG_PREFIX_E << "Parent tree item" << parent->get_name() << "is not attached to tree";
		return sg_ret::err;
	}

	/* Attach yourself to tree first. */
	if (sg_ret::ok != parent->tree_view->attach_to_tree(parent, this, attach_mode, sibling)) {
		qDebug() << SG_PREFIX_E << "Failed to attach tree item" << this->get_name() << "to tree";
		return sg_ret::err;
	}
	if (!this->is_in_tree()) { /* After calling tree_view->attach_to_tree(), this condition should be true. */
		qDebug() << SG_PREFIX_E << "Failed to attach tree item" << this->get_name() << "to tree";
		return sg_ret::err;
	}

	/* Attach child items. */
	return this->attach_children_to_tree();
}




sg_ret TreeItem::attach_children_to_tree(void)
{
	return sg_ret::ok;
}




SGObjectTypeID TreeItem::get_type_id(void) const
{
	qDebug() << SG_PREFIX_W << "Returning empty object type id for object" << this->m_name;
	return SGObjectTypeID(); /* Empty value for all tree item types that don't need specific value. */
}




void TreeItem::update_tree_item_tooltip(void)
{
	if (this->tree_view) {
		this->tree_view->update_tree_item_tooltip(*this);
	} else {
		qDebug() << SG_PREFIX_E << "Trying to update tooltip of tree item" << this->m_name << "that is not connected to tree";
	}
	return;
}




bool StandardMenuOperations::is_member(StandardMenuOperation op) const
{
	auto iter = std::find(this->begin(), this->end(), op);
	return iter != this->end();
}




/**
   @reviewed-on 2019-12-30
*/
sg_ret TreeItem::cut_tree_item_cb(void)
{
	qDebug() << SG_PREFIX_E << "Called the method for base class";
	return sg_ret::err;
}




/**
   @reviewed-on 2019-12-30
*/
sg_ret TreeItem::copy_tree_item_cb(void)
{
	qDebug() << SG_PREFIX_E << "Called the method for base class";
	return sg_ret::err;
}




/**
   @reviewed-on 2019-12-30
*/
sg_ret TreeItem::delete_tree_item_cb(void)
{
	qDebug() << SG_PREFIX_E << "Called the method for base class";
	return sg_ret::err;
}




/**
   @reviewed-on 2019-12-30
*/
sg_ret TreeItem::paste_child_tree_item_cb(void)
{
	qDebug() << SG_PREFIX_E << "Called the method for base class";
	return sg_ret::err;
}
