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
		TREE_ITEM      = 4, /* Tree item to be stored in the tree. Layer, Sublayers Node, or Sublayer. */
		EDITABLE       = 5,
		TIMESTAMP      = 6, /* Item's timestamp. Sortable column. */
	};


	typedef QPersistentModelIndex TreeIndex;




	class TreeView;
	class Layer;
	class Window;
	class LayersPanel;
	class Viewport;




	class TreeItem : public QObject {
		Q_OBJECT
	public:
		TreeItem() {};
		~TreeItem() {};

		TreeIndex const & get_index(void);
		void set_index(TreeIndex & i);

		sg_uid_t get_uid(void) const;

		virtual QString get_tooltip(void);

		virtual bool add_context_menu_items(QMenu & menu) { return false; };


		/* Change visibility of tree item.
		   Return visibility state after the toggle has been performed. */
		virtual bool toggle_visible(void);
		virtual void set_visible(bool new_state);


		char debug_string[100] = { 0 };

	//protected:
		TreeItemType tree_item_type = TreeItemType::LAYER;
		TreeIndex index;
		TreeView * tree_view = NULL; /* Reference. */

		bool editable = true; /* Is this item is editable? TODO: be more specific: is the data editable, or is the reference visible in the tree editable? */
		bool visible = true;  /* Is this item is visible in a tree of data items? */

		sg_uid_t uid = SG_UID_INITIAL;

		QString type_id;
		QStringList accepted_child_type_ids;

		Window * window = NULL;
		Layer * owning_layer = NULL;
	};




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

		void emit_update_window(void) { emit this->update_window(); };

	signals:
		void update_window(void);
	};




} /* namespace SlavGPS */




Q_DECLARE_METATYPE(SlavGPS::TreeItem*)




#endif /* #ifndef _SG_TREE_VIEW_H_ */
