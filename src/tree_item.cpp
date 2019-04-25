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




sg_uid_t TreeItem::get_uid(void) const
{
	return this->uid;
}




QString TreeItem::get_tooltip(void) const
{
	return tr("Tree Item");
}




Layer * TreeItem::to_layer(void) const
{
	switch (this->get_tree_item_type()) {
	case TreeItemType::Layer:
		return (Layer *) this;
	case TreeItemType::Sublayer:
		return this->owning_layer;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected value of tree item type:" << (int) this->get_tree_item_type() << this->name;
		/* We shouldn't be returning anything, but at least
		   try to return something that may be not NULL. */
		return this->owning_layer;
	}
}




Layer * TreeItem::get_owning_layer(void) const
{
	return this->owning_layer;
}




void TreeItem::set_owning_layer(Layer * layer)
{
	this->owning_layer = layer;
}




TreeItem * TreeItem::get_parent_tree_item(void) const
{
	return this->parent_tree_item;
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
	if (NULL == this->tree_view) {
		return false;
	}

	if (!this->index.isValid()) {
		return false;
	}

	return true;
}




bool TreeItem::compare_name_ascending(const TreeItem * a, const TreeItem * b)
{
	return (a->name < b->name);
}




bool TreeItem::compare_name_descending(const TreeItem * a, const TreeItem * b)
{
	return !TreeItem::compare_name_ascending(a, b);
}




TreeItem::MenuOperation SlavGPS::operator_bit_or(TreeItem::MenuOperation arg1, TreeItem::MenuOperation arg2)
{
	TreeItem::MenuOperation result = static_cast<TreeItem::MenuOperation>(static_cast<uint16_t>(arg1) | static_cast<uint16_t>(arg2));
	return result;
}




TreeItem::MenuOperation SlavGPS::operator_bit_and(TreeItem::MenuOperation arg1, TreeItem::MenuOperation arg2)
{
	TreeItem::MenuOperation result = static_cast<TreeItem::MenuOperation>(static_cast<uint16_t>(arg1) & static_cast<uint16_t>(arg2));
	return result;
}




TreeItem::MenuOperation SlavGPS::operator_bit_not(const TreeItem::MenuOperation arg)
{
	TreeItem::MenuOperation result = static_cast<TreeItem::MenuOperation>(~(static_cast<uint16_t>(arg)));
	return result;
}




TreeItem::MenuOperation TreeItem::get_menu_operation_ids(void) const
{
	return this->menu_operation_ids;
}




void TreeItem::set_menu_operation_ids(TreeItem::MenuOperation new_value)
{
	this->menu_operation_ids = new_value;
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
			item = new QStandardItem(this->name);
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
			item->setCheckState(this->visible ? Qt::Checked : Qt::Unchecked);
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
			item = new QStandardItem(this->timestamp.get_value());
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
	qDebug() << SG_PREFIX_E << "Can't drop tree item" << tree_item->name << "here";
	return sg_ret::err;
}




sg_ret TreeItem::dropped_item_is_acceptable(TreeItem * tree_item, bool * result) const
{
	if (NULL == result) {
		return sg_ret::err;
	}

	*result = false;
	return sg_ret::ok;
}




sg_ret TreeItem::attach_children_to_tree(void)
{
	return sg_ret::ok;
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
	this->timestamp = Time(value);
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
		qDebug() << SG_PREFIX_SIGNAL << "Tree item" << this->name << "emits 'layer changed' signal @" << where;
		emit this->tree_item_changed(this->name);
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
	qDebug() << SG_PREFIX_SIGNAL << "TreeItem" << this->name << "emits 'changed' signal @" << where;
	emit this->tree_item_changed(this->name);
}
