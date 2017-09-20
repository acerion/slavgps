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



#include <cstdint>

#include <QStandardItem>
#include <QPersistentModelIndex>
#include <QObject>




namespace SlavGPS {




	enum class TreeItemType {
		LAYER = 0,
		SUBLAYER
	};


	enum LayersTreeRole {
		RoleLayerData = Qt::UserRole + 1,
		RoleSublayerData
	};


	enum class LayersTreeColumn {
		NAME           = 0, /* From item's name. Sortable column. */
		VISIBLE        = 1, /* From item's (or item parent's) visibility. */
		ICON           = 2,

		/* These columns are not visible in tree view. */
		TREE_ITEM_TYPE = 3, /* Implicit, based on function adding an item. */
		PARENT_LAYER   = 4, /* Parent layer of tree item. */
		TREE_ITEM      = 5, /* Tree item to be stored in the tree. Layer, Sublayers Node, or Sublayer. */
		EDITABLE       = 6,
		TIMESTAMP      = 7, /* Item's timestamp. Sortable column. */
	};


	typedef QPersistentModelIndex TreeIndex;




	class TreeView;




	class TreeItem : public QObject {
		Q_OBJECT
	public:
		TreeItem() {};
		~TreeItem() {};

		TreeIndex const & get_index(void);
		void set_index(TreeIndex & i);

		char debug_string[100] = { 0 };

	//protected:
		TreeItemType tree_item_type;
		TreeIndex index;
		TreeView * tree_view = NULL; /* Reference. */

		QString type_id;
	};




} /* namespace SlavGPS */




Q_DECLARE_METATYPE(SlavGPS::TreeItem*)




#endif /* #ifndef _SG_TREE_VIEW_H_ */
