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

#ifndef _SG_TREE_ITEM_H_
#define _SG_TREE_ITEM_H_



#include <cstdint>




#include <QStandardItem>
#include <QPersistentModelIndex>
#include <QObject>
#include <QString>
#include <QMenu>




#include "globals.h"




namespace SlavGPS {




	class Layer;
	class Viewport;
	class TreeView;
	class Pickle;

	class TreeItemListFormat;




	typedef uint32_t sg_uid_t;
#define SG_UID_INITIAL  1
#define SG_UID_NONE     0




	enum class TreeItemType {
		Layer,
		Sublayer
	};




	enum TreeItemPropertyID {
		ParentLayer,     /* Name of parent layer containing given tree item. */
		TheItem,        /* Name of given tree item. */
		Timestamp,       /* Timestamp attribute of given tree item. */
		Icon,            /* Icon attribute of given tree item (pixmap). */
		Visibility,      /* Is the tree item visible in tree view (boolean)? */
		Comment,         /* Comment attribute of given tree item. */
		Elevation,       /* Elevation attribute of given tree item. */
		Coordinate,      /* Coordinate attribute of given tree item. */
	};




	typedef QPersistentModelIndex TreeIndex;




	class TreeItem : public QObject {
		Q_OBJECT

		friend class Tree;
	public:
		TreeItem();
		~TreeItem();


		/* Which standard operations shall be present in context menu for a tree item? */
		enum MenuOperation {
			None       = 0x0000,
			Properties = 0x0001,
			Cut        = 0x0002,
			Copy       = 0x0004,
			Paste      = 0x0008,
			Delete     = 0x0010,
			New        = 0x0020,
			All        = 0xffff,
		};


		TreeIndex const & get_index(void);
		void set_index(TreeIndex & i);

		sg_uid_t get_uid(void) const;

		static bool compare_name_ascending(const TreeItem * a, const TreeItem * b);   /* Ascending: AAA -> ZZZ */
		static bool compare_name_descending(const TreeItem * a, const TreeItem * b);  /* Descending: ZZZ -> AAA */


		virtual time_t get_timestamp(void) const { return this->has_timestamp ? this->timestamp : 0; };

		virtual QString get_tooltip(void) const;

		/* A TreeItem object needs to implement this method if it contains (is direct
		   parent of) any items/children that need to be added to application's tree
		   of items.

		   This method should call attach_to_tree() on any such
		   child that needs to be added to the tree. */
		virtual sg_ret attach_children_to_tree(void);

		virtual bool add_context_menu_items(QMenu & menu, bool tree_view_context_menu) { return false; };

		virtual sg_ret drag_drop_request(TreeItem * tree_item, int row, int col);
		virtual sg_ret dropped_item_is_acceptable(TreeItem * tree_item, bool * result) const;


		/* Change visibility of tree item.
		   Return visibility state after the toggle has been performed. */
		virtual bool toggle_visible(void);
		virtual void set_visible(bool new_state);

		virtual void marshall(Pickle & pickle) { };

		virtual QList<QStandardItem *> get_list_representation(const TreeItemListFormat & list_format);

		/**
		   \brief The item has been selected in items tree. Do something about it.

		   @return false if the event of being selected was not handled
		   @return true otherwise
		 */
		virtual bool handle_selection_in_tree(void) { return false; };

		virtual void draw_tree_item(Viewport * viewport, bool highlight_selected, bool parent_is_selected) { return; };

		virtual bool properties_dialog() { return false; };

		/* Is given tree item a member of a tree? */
		bool is_in_tree(void) const;

		/* Get layer associated with this tree item.
		   Either the tree item itself is a layer, or a sublayer has its parent/owning layer.
		   Return one of these. */
		Layer * to_layer(void) const;

		Layer * get_owning_layer(void) const;
		void set_owning_layer(Layer * layer);

		TreeItem::MenuOperation get_menu_operation_ids(void) const;
		void set_menu_operation_ids(TreeItem::MenuOperation new_value);

		/* See if two items are exactly the same object (i.e. whether pointers point to the same object).
		   Return true if this condition is true.
		   Return false otherwise.
		   Return false if any of the pointers is NULL. */
		static bool the_same_object(const TreeItem * item1, const TreeItem * item2);


		virtual void display_debug_info(const QString & reference) const;

	//protected:
		TreeItemType tree_item_type = TreeItemType::Layer;
		TreeIndex index;             /* Set in TreeView::attach_to_tree(). */
		TreeView * tree_view = NULL; /* Reference to application's main tree, set in TreeView::insert_tree_item_at_row(). */

		bool editable = true; /* Is this item is editable? TODO_LATER: be more specific: is the data editable, or is the reference visible in the tree editable? */
		bool visible = true;  /* Is this item is visible in a tree of data items? */

		bool has_properties_dialog = false; /* Does this tree item has dialog, in which you can view or change *configurable* properties? */


		QString name;
		QString type_id;
		QStringList accepted_child_type_ids;

		bool has_timestamp = false;
		time_t timestamp = 0;

		QIcon icon; /* .isNull() may return true for this field (if child class doesn't assign anything to the icon). */

		char debug_string[100] = { 0 };

	protected:
		Layer * owning_layer = NULL; /* Reference. */

		sg_uid_t uid = SG_UID_INITIAL;

		/* Menu items (actions) to be created and put into a
		   context menu for given tree item type. */
		TreeItem::MenuOperation menu_operation_ids = TreeItem::MenuOperation::All;
	};

	/* These silly names are a workaroud for clash of operator definitions.
	   https://stackoverflow.com/questions/10755058/qflags-enum-type-conversion-fails-all-of-a-sudden */
	TreeItem::MenuOperation operator_bit_and(const TreeItem::MenuOperation arg1, const TreeItem::MenuOperation arg2);
	TreeItem::MenuOperation operator_bit_or(const TreeItem::MenuOperation arg1, const TreeItem::MenuOperation arg2);
	TreeItem::MenuOperation operator_bit_not(const TreeItem::MenuOperation arg);



} /* namespace SlavGPS */




Q_DECLARE_METATYPE(SlavGPS::TreeItem*)




#endif /* #ifndef _SG_TREE_ITEM_H_ */
