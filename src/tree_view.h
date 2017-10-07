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
		INVALID = 0,
		LAYER,
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
		TREE_ITEM      = 3, /* Tree item to be stored in the tree. Layer, Sublayers Node, or Sublayer. */
		EDITABLE       = 4,
		TIMESTAMP      = 5, /* Item's timestamp. Sortable column. */
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

		/* A TreeItem object needs to implement this method if it contains (is direct
		   parent of) any items/children that need to be added to application's tree
		   of items.

		   This method should call add_tree_item() on any such
		   child that needs to be added to the tree. */
		virtual void add_children_to_tree(void) {};

		virtual bool add_context_menu_items(QMenu & menu, bool tree_view_context_menu) { return false; };


		/* Change visibility of tree item.
		   Return visibility state after the toggle has been performed. */
		virtual bool toggle_visible(void);
		virtual void set_visible(bool new_state);

		virtual void marshall(uint8_t ** data, size_t * data_len) { };

		/**
		   \brief The item has been selected in items tree. Do something about it.

		   @return false if the event of being selected was not handled
		   @return true otherwise
		 */
		virtual bool handle_selection_in_tree(void) { return false; };

		virtual void draw_tree_item(Viewport * viewport, bool hl_is_allowed, bool hl_is_required) { return; };

		/* Get layer associated with this tree item.
		   Either the tree item itself is a layer, or a sublayer has its parent/owning layer.
		   Return one of these. */
		Layer * to_layer(void) const;

	//protected:
		TreeItemType tree_item_type = TreeItemType::LAYER;
		TreeIndex index;             /* Set in TreeView::add_tree_item(). */
		TreeView * tree_view = NULL; /* Reference to application's main tree, set in TreeView::add_tree_item(). */

		bool editable = true; /* Is this item is editable? TODO: be more specific: is the data editable, or is the reference visible in the tree editable? */
		bool visible = true;  /* Is this item is visible in a tree of data items? */

		sg_uid_t uid = SG_UID_INITIAL;

		QString name;
		QString type_id;
		QStringList accepted_child_type_ids;

		Layer * owning_layer = NULL; /* Reference. */

		char debug_string[100] = { 0 };
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

		/* Set in TreeItem::handle_selection_in_tree(). Used to draw selected tree items with highlight in viewport. */
		TreeItem * selected_tree_item = NULL;

		void emit_update_window(void) { emit this->update_window(); };

	signals:
		void update_window(void);
	};




} /* namespace SlavGPS */




Q_DECLARE_METATYPE(SlavGPS::TreeItem*)




#endif /* #ifndef _SG_TREE_VIEW_H_ */
