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

#include "tree_item.h"




using namespace SlavGPS;




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
	if (this->tree_item_type == TreeItemType::LAYER) {
		return (Layer *) this;
	} else {
		return this->owning_layer;
	}
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




bool TreeItem::compare_name(const TreeItem * a, const TreeItem * b)
{
	return a->name < b->name;
}




bool TreeItem::compare_name_descending(const TreeItem * a, const TreeItem * b)
{
	/* TODO: is it '>' or '<'? */
	return (a->name > b->name);
}




bool TreeItem::compare_name_ascending(const TreeItem * a, const TreeItem * b)
{
	return !TreeItem::compare_name_descending(a, b);
}




TreeItemOperation operator&(TreeItemOperation& arg1, TreeItemOperation& arg2)
{
	TreeItemOperation result = static_cast<TreeItemOperation>(static_cast<uint16_t>(arg1) | static_cast<uint16_t>(arg2));
	return result;
}


TreeItemOperation operator|(TreeItemOperation& arg1, TreeItemOperation& arg2)
{
	TreeItemOperation result = static_cast<TreeItemOperation>(static_cast<uint16_t>(arg1) & static_cast<uint16_t>(arg2));
	return result;
}


TreeItemOperation operator~(const TreeItemOperation& arg)
{
	TreeItemOperation result = static_cast<TreeItemOperation>(~(static_cast<uint16_t>(arg)));
	return result;
}
