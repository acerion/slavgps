/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2010-2015, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2020, Kamil Ignacak <acerion@wp.pl>
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
	qDebug() << SG_PREFIX_I << "Destructor of" << this->get_name() << "called";
	g_selected.remove_from_set(this);
}




TreeIndex const & TreeItem::index(void) const
{
	return this->m_index;
}




void TreeItem::set_index(TreeIndex & index)
{
	this->m_index = index;
}




bool TreeItem::toggle_visible(void)
{
	this->m_visible = !this->m_visible;
	return this->m_visible;
}




void TreeItem::set_visible(bool visible)
{
	this->m_visible = visible;
}




bool TreeItem::is_visible_with_parents(void) const
{
	return this->tree_view->get_tree_item_visibility_with_parents(this);
}




bool TreeItem::is_visible(void) const
{
	return this->m_visible;
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
   @reviewed-on 2020-03-06
*/
Layer * TreeItem::immediate_layer(void)
{
	if (this->is_layer()) {
		return (Layer *) this;
	} else {
		return this->parent_layer();
	}
}




/**
   @reviewed-on 2020-03-06
*/
sg_ret TreeItem::set_parent_member(TreeItem * parent)
{
	this->m_parent = parent;
	return sg_ret::ok;
}




/**
   @reviewed-on 2020-03-06
*/
TreeItem * TreeItem::parent_member(void) const
{
#if K_TODO_LATER
	/* Check that information stored in TreeItem::m_parent and in Qt Model/Tree is consistent. */
	if (is in tree) {
		assert (this->m_parent == this->tree_view->get_parent(this)->m_parent);
	}
#endif
	return this->m_parent;
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

	if (!this->index().isValid()) {
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




sg_ret TreeItem::menu_add_standard_operations(QMenu & menu, const StandardMenuOperations & ops, __attribute__((unused)) bool in_tree_view)
{
	LayersPanel * layers_panel = ThisApp::layers_panel();
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




/* FIXME: make this function as simple as possible, only returning a
   vector of QStandardItem values.  Query for as few properties of the
   item as possible: the item may not be attached to tree view, so
   getting some of its properties may be invalid, time consuming, and
   trigger error logs. */
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
				item = new QStandardItem(this->timestamp.ll_value());
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




void TreeItem::display_debug_info(__attribute__((unused)) const QString & reference) const
{
	return;
}




sg_ret TreeItem::accept_dropped_child(TreeItem * tree_item, __attribute__((unused)) int row, __attribute__((unused)) int col)
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
	this->timestamp = Time(value, TimeType::Unit::internal_unit());
}




bool TreeItem::move_child(TreeItem & child_tree_item, bool up)
{
	return this->tree_view->move_tree_item(child_tree_item, up);
}




/**
   Indicate to receiver that specified tree item has changed (if the item is visible)
*/
void TreeItem::emit_tree_item_changed(const QString & where)
{
	if (this->m_visible && this->tree_view) {
		ThisApp::main_window()->set_redraw_trigger(this);
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
	ThisApp::main_window()->set_redraw_trigger(this);
	qDebug() << SG_PREFIX_SIGNAL << "TreeItem" << this->m_name << "emits 'changed' signal @" << where;
	emit this->tree_item_changed(this->m_name);
}




sg_ret TreeItem::click_in_tree(__attribute__((unused)) const QString & debug)
{
	QStandardItem * item = this->tree_view->get_tree_model()->itemFromIndex(this->index());
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




sg_ret TreeItem::get_tree_items(__attribute__((unused)) std::list<TreeItem *> & list, __attribute__((unused)) const std::list<SGObjectTypeID> & wanted_types) const
{
	return sg_ret::ok;
}




sg_ret TreeItem::attach_child_to_tree(TreeItem * child, int row)
{
	if (!this->is_in_tree()) {
		qDebug() << SG_PREFIX_E << "Parent tree item" << this->get_name() << "is not attached to tree";
		return sg_ret::err;
	}

	/* Attach child to tree under yourself. */
	if (!child->is_in_tree()) {
		if (sg_ret::ok != this->tree_view->attach_to_tree(this, child, row)) {
			qDebug() << SG_PREFIX_E << "Failed to attach tree item" << child->get_name() << "to tree";
			return sg_ret::err;
		}
		if (!child->is_in_tree()) { /* After calling tree_view->attach_to_tree(), this condition should be true. */
			qDebug() << SG_PREFIX_E << "Failed to attach tree item" << child->get_name() << "to tree";
			return sg_ret::err;
		}
	}

	/* Attach child items. */
	return child->post_read_2();
}




sg_ret TreeItem::post_read_2(void)
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




/**
   @reviewed-on 2020-03-05
*/
sg_ret TreeItem::add_child_item(TreeItem * child_tree_item)
{
	/* Classes derived from TreeItem should implement mechanisms
	   deciding if a class can accept children and of which
	   type. */

	if (this->is_in_tree()) {
		/* This container is attached to Qt Model, so it can
		   attach the new child to the Model too, directly
		   under itself. */
		qDebug() << SG_PREFIX_I << "Attaching item" << child_tree_item->get_name() << "to tree under" << this->get_name();
		if (sg_ret::ok != this->attach_as_tree_item_child(child_tree_item, -1)) {
			qDebug() << SG_PREFIX_E << "Failed to attach" << child_tree_item->get_name() << "as tree item child of" << this->get_name();
			return sg_ret::err;
		}

		/* Update our own tooltip in tree view. */
		this->update_tree_item_tooltip();
		return sg_ret::ok;
	} else {
		/* This container is not attached to Qt Model yet,
		   most probably because it is being read
		   from file and won't be attached to Qt Model until
		   whole file is read.

		   So the container has to put the child on list of
		   un-attached items, to be attached later, in
		   post_read() function. */
		qDebug() << SG_PREFIX_I << this->get_name() << "container is not attached to Model yet, adding" << child_tree_item->get_name() << "to list of unattached children of" << this->get_name();
		this->unattached_children.push_back(child_tree_item);
		return sg_ret::ok;
	}
}




/**
   @reviewed-on 2019-12-30
*/
sg_ret TreeItem::cut_child_item(__attribute__((unused)) TreeItem * child_tree_item)
{
	qDebug() << SG_PREFIX_E << "Called the method for base class";
	return sg_ret::err;
}




/**
   @reviewed-on 2019-12-30
*/
sg_ret TreeItem::copy_child_item(__attribute__((unused)) TreeItem * child_tree_item)
{
	qDebug() << SG_PREFIX_E << "Called the method for base class";
	return sg_ret::err;
}




/**
   @reviewed-on 2019-12-30
*/
sg_ret TreeItem::delete_child_item(__attribute__((unused)) TreeItem * child_tree_item, __attribute__((unused)) bool confirm_deleting)
{
	qDebug() << SG_PREFIX_E << "Called the method for base class";
	return sg_ret::err;
}




int TreeItem::child_rows_count(void) const
{
	if (!this->is_in_tree()) {
		/* Not necessarily an error. */
		return -1;
	}

	int rows = 0;
	if (sg_ret::ok != this->tree_view->get_child_rows_count(this->index(), rows)) {
		return -1;
	}

	return rows;
}




sg_ret TreeItem::child_from_row(int row, TreeItem ** child_tree_item) const
{
	return this->tree_view->get_child_from_row(this->index(), row, child_tree_item);
}




TreeItem * TreeItem::find_child_by_uid(sg_uid_t child_uid) const
{
	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {
		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to find valid tree item in row" << row << "/" << rows;
			continue;
		}

		if (child_uid == tree_item->get_uid()) {
			return tree_item;
		}
	}

	return nullptr;
}




/**
   @reviewed-on 2020-02-25
*/
TreeItem * TreeItem::find_child_by_name(const QString & name) const
{
	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {
		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to find valid tree item in row" << row << "/" << rows;
			continue;
		}
		if (tree_item->get_name().isEmpty()) {
			continue;
		}
		if (tree_item->get_name() == name) {
			return tree_item;
		}
	}
	return nullptr;

}




int TreeItem::set_direct_children_only_visibility_flag(bool visible)
{
	int changed = 0;

	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {
		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to get child from row" << row << "/" << rows;
			continue;
		}

		tree_item->set_visible(visible);
		this->tree_view->apply_tree_item_visibility(tree_item);
		changed++;
	}

	return changed;
}




int TreeItem::toggle_direct_children_only_visibility_flag(void)
{
	int changed = 0;

	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {
		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to get child from row" << row << "/" << rows;
			continue;
		}

		tree_item->toggle_visible();
		/* Also set checkbox on/off in tree view. */
		this->tree_view->apply_tree_item_visibility(tree_item);
		changed++;
	}

	return changed;
}




int TreeItem::list_child_uids(std::list<sg_uid_t> & list) const
{
	int count = 0;

	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {
		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to get child from row" << row << "/" << rows;
			continue;
		}
		list.push_back(tree_item->get_uid());
		count++;
	}

	return count;
}




int TreeItem::list_tree_items(std::list<TreeItem *> & list) const
{
	int count = 0;

	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {
		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to find valid tree item in row" << row << "/" << rows;
			continue;
		}

		list.push_back(tree_item);
		count++;
	}
	return count;
}




sg_ret TreeItem::attach_as_tree_item_child(TreeItem * child, int row)
{
	if (nullptr == this->tree_view) {
		qDebug() << SG_PREFIX_E << "The method has been called for unattached parent" << this->get_name();
		return sg_ret::err;
	}
	if (sg_ret::ok != this->tree_view->attach_to_tree(this, child, row)) {
		qDebug() << SG_PREFIX_E << "Failed to attach child item" << child->get_name() << "under" << this->get_name();
		return sg_ret::err;
	}

	this->tree_view->expand(this->index());

	QObject::connect(child, SIGNAL (tree_item_changed(const QString &)), this->m_parent, SLOT (child_tree_item_changed_cb(const QString &)));
	return sg_ret::ok;
}




/* Doesn't set the trigger. Should be done by aggregate layer when child emits 'changed' signal. */
sg_ret TreeItem::child_tree_item_changed_cb(const QString & child_tree_item_name) /* Slot. */
{
	qDebug() << SG_PREFIX_SLOT << "Parent" << this->get_name() << "received 'child tree item changed' signal from" << child_tree_item_name;
	if (this->is_visible()) {
		/* TODO_LATER: this can used from the background - e.g. in acquire
		   so will need to flow background update status through too. */
		qDebug() << SG_PREFIX_SIGNAL << "Layer" << this->get_name() << "emits 'changed' signal";
		emit this->tree_item_changed(this->get_name());
	}

	return sg_ret::ok;
}
