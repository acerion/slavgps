/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _SG_TREE_VIEW_H_
#define _SG_TREE_VIEW_H_




#include <map>




#include <QStandardItem>
#include <QPersistentModelIndex>
#include <QObject>
#include <QMenu>




#include "tree_item.h"




namespace SlavGPS {




	enum LayersTreeRole {
		RoleLayerData = Qt::UserRole + 1,
		RoleSublayerData
	};




	enum class TreeViewSortOrder {
		None = 0,
		AlphabeticalAscending,
		AlphabeticalDescending,
		DateAscending,
		DateDescending,
		Last
	};




	class TreeItem;




	class SelectedTreeItems {
	public:
		void add_to_set(TreeItem * tree_item);
		bool remove_from_set(const TreeItem * tree_item);
		bool is_in_set(const TreeItem * tree_item) const;
		void clear(void);
		int size(void) const;

		/**
		   Print to console information about how given @param
		   tree_item will be drawn given its current selection
		   status.
		*/
		static void print_draw_mode(const TreeItem & tree_item, bool parent_is_selected);

	private:
		/* Set in TreeItem::handle_selection_in_tree(). Used to draw selected tree items with highlight in viewport. */
		std::map<sg_uid_t, TreeItem *> selected_tree_items;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TREE_VIEW_H_ */
