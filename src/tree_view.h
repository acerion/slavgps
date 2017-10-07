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
#include <QMenu>

#include "globals.h"
#include "tree_item.h"




namespace SlavGPS {




	enum LayersTreeRole {
		RoleLayerData = Qt::UserRole + 1,
		RoleSublayerData
	};


	enum class TreeViewColumn {
		NAME           = 0, /* From item's name. Sortable column. */
		VISIBLE        = 1, /* From item's visibility. */
		ICON           = 2,

		/* These columns are not visible in tree view. */
		TREE_ITEM      = 3, /* Tree item to be stored in the tree. */
		EDITABLE       = 4,
		TIMESTAMP      = 5, /* Item's timestamp. Sortable column. */
	};





	class TreeView;
	class Layer;
	class Window;
	class LayersPanel;
	class Viewport;
	class TreeItem;




	class Tree : public QObject {
		Q_OBJECT
	public:
		Tree() {};
		~Tree() {};

		TreeView * tree_get_tree_view() { return this->tree_view; };
		Window * tree_get_main_window() { return this->window; };
		LayersPanel * tree_get_layers_panel() { return this->layers_panel; };
		Viewport * tree_get_main_viewport() { return this->viewport; };

		TreeView * tree_view = NULL; /* Reference. */
		Window * window = NULL;
		LayersPanel * layers_panel = NULL;
		Viewport * viewport = NULL;

		/* Set in TreeItem::handle_selection_in_tree(). Used to draw selected tree items with highlight in viewport. */
		TreeItem * selected_tree_item = NULL;

		void emit_update_window(void) { emit this->update_window(); };

	signals:
		void update_window(void);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TREE_VIEW_H_ */
