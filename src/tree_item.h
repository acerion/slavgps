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
#include <vector>




#include <QStandardItem>
#include <QPersistentModelIndex>
#include <QObject>
#include <QString>
#include <QMenu>
#include <QDebug>




#include "globals.h"
#include "measurements.h"




#define VIK_SETTINGS_SORTABLE_DATE_TIME_FORMAT "sortable_date_time_format"




namespace SlavGPS {




	class Layer;
	class GisViewport;
	class TreeView;
	class Pickle;

	class TreeItemViewFormat;
	class TreeItemIdentityPredicate;


	enum class TreeViewAttachMode {
		Front,
		Back,
		Before,
		After
	};




	typedef uint32_t sg_uid_t;
#define SG_UID_INITIAL  1
#define SG_UID_NONE     0




	enum TreeItemPropertyID {
		ParentLayer,     /* Name of parent layer containing given tree item. */
		TheItem,         /* Name of given tree item. */
		Timestamp,       /* Timestamp attribute of given tree item. */
		Icon,            /* Icon attribute of given tree item (pixmap). */
		Visibility,      /* Is the tree item visible in tree view (boolean)? */
		Editable,
		Comment,         /* Comment attribute of given tree item. */
		Elevation,       /* Elevation attribute of given tree item. */
		Coordinate,      /* Coordinate attribute of given tree item. */

		/* Track/Route-specific properties. */
		Length,
		DurationProp,
		MinimumSpeed,
		AverageSpeed,
		MaximumSpeed,
		MinimumHeight,
		AverageHeight,
		MaximumHeight,
	};




	class TreeItemViewColumn {
	public:
		TreeItemViewColumn(enum TreeItemPropertyID new_id, bool new_visible, const QString & new_header_label) :
			id(new_id),
			visible(new_visible),
			header_label(new_header_label) {};
		const TreeItemPropertyID id;
		const bool visible;            /* Is the column visible? */
		const QString header_label;    /* If the column is visible, this is the label of column header. */
	};

	class TreeItemViewFormat {
	public:
		std::vector<TreeItemViewColumn> columns;
		TreeItemViewFormat & operator=(const TreeItemViewFormat & other);
	};




	typedef QPersistentModelIndex TreeIndex;




	/* Which standard operations shall be present in context menu
	   for a tree item? */
	enum class StandardMenuOperation {
		Properties,
		Cut,
		Copy,
		Paste,
		Delete,
		New,
	};

	class StandardMenuOperations : public std::list<StandardMenuOperation> {
	public:
		bool is_member(StandardMenuOperation op) const;
	};






	class TreeItem : public QObject {
		Q_OBJECT

		friend class Tree;
		friend class TreeItemIdentityPredicate;
		friend class TreeView;
	public:
		TreeItem();
		~TreeItem();


		TreeIndex const & index(void) const;
		void set_index(TreeIndex & index);

		sg_uid_t get_uid(void) const;

		static bool compare_name_ascending(const TreeItem * a, const TreeItem * b);   /* Ascending: AAA -> ZZZ */
		static bool compare_name_descending(const TreeItem * a, const TreeItem * b);  /* Descending: ZZZ -> AAA */

		void emit_tree_item_changed(const QString & where);
		void emit_tree_item_changed_although_invisible(const QString & where);

		virtual Time get_timestamp(void) const;
		virtual void set_timestamp(const Time & value);
		virtual void set_timestamp(time_t value);

		const QString & get_name(void) const;
		void set_name(const QString & name);

		virtual QString get_tooltip(void) const;

		/* A TreeItem object needs to implement this method if
		   it contains (is direct parent of) any
		   items/children that need to be added to
		   application's tree of items.

		   This method should call
		   attach_to_tree_under_parent() on any such child
		   that needs to be added to the tree.

		   TODO_LATER: this should be a protected method.
		*/
		virtual sg_ret post_read_2(void);


		virtual sg_ret menu_add_standard_operations(QMenu & menu, const StandardMenuOperations & ops, bool in_tree_view);
		virtual sg_ret menu_add_type_specific_operations(__attribute__((unused)) QMenu & menu, __attribute__((unused)) bool in_tree_view) { return sg_ret::ok; }

		/**
		   @param in_tree_view decides if context menu is
		   shown in response to event in Tree View widget, or
		   in other widget.
		*/
		sg_ret show_context_menu(const QPoint & position, bool in_tree_view, QWidget * parent = nullptr);

		virtual sg_ret drag_drop_request(TreeItem * tree_item, int row, int col);
		virtual bool dropped_item_is_acceptable(const TreeItem & tree_item) const;



		/* Change visibility of tree item.
		   Return visibility state after the toggle has been performed. */
		bool toggle_visible(void);

		void set_visible(bool new_state);

		/* See if given item is marked as visible. Don't look
		   at parents' visibility. */
		bool is_visible(void) const;

		/* See if given item is marked as visible, and all its
		   parents are also marked as visible. */
		bool is_visible_with_parents(void) const;



		virtual void marshall(__attribute__((unused)) Pickle & pickle) { };

		virtual QList<QStandardItem *> get_list_representation(const TreeItemViewFormat & view_format);

		/* Update visible properties of tree item in tree view. */
		virtual sg_ret update_tree_item_properties(void) { return sg_ret::ok; };

		/**
		   \brief Equivalent of selecting the tree item by clicking it with mouse cursor in tree
		*/
		virtual sg_ret click_in_tree(const QString & debug);

		/**
		   \brief The item has been selected in items tree. Do something about it.

		   @return false if the event of being selected was not handled
		   @return true otherwise
		 */
		virtual bool handle_selection_in_tree(void) { return false; };

		virtual void draw_tree_item(__attribute__((unused)) GisViewport * gisview, __attribute__((unused)) bool highlight_selected, __attribute__((unused)) bool parent_is_selected) { return; };

		virtual bool show_properties_dialog(void) { return false; };

		/* Is given tree item a member of a tree? */
		bool is_in_tree(void) const;

		/**
		   @brief Is given tree item a layer?
		*/
		virtual bool is_layer(void) const;




		/* Top-level functions for cut/copy/paste/delete operations. */
		/**
		   @brief "paste" operation

		   @param allow_reordering: should be set to true for GUI
		   interactions, whereas loading from a file needs strict ordering and
		   so should be false.
		*/
		virtual sg_ret add_child_item(TreeItem * item, bool allow_reordering);
		virtual sg_ret cut_child_item(TreeItem * item);
		virtual sg_ret copy_child_item(TreeItem * item);
		/**
		   @brief Delete a child item specified by @param item

		   This method also calls destructor of @param item.
		*/
		virtual sg_ret delete_child_item(TreeItem * item, bool confirm_deleting);




		/**
		   @brief Get layer associated with this tree item

		   Either the tree item itself is a layer, or a
		   sublayer has its parent/owning layer.  Return one
		   of these.
		*/
		virtual Layer * immediate_layer(void);

		/**
		   Get direct parent tree item. Direct parent tree
		   item and owning tree item may be two different tree
		   items.
		*/
		TreeItem * parent_tree_item(void) const;

		/**
		   Get tree item that is the owner of this tree
		   item. Direct parent tree item and owning tree item
		   may be two different tree items.
		*/
		TreeItem * owner_tree_item(void) const;

		Layer * parent_layer(void) const;

		/**
		   Set two fields of this tree item using specified
		   direct @param parent tree item: parent tree item
		   and owner tree item
		*/
		virtual sg_ret set_parent_and_owner_tree_item(TreeItem * parent);


		/* Get tree items (direct and indirect children of the
		   layer) of types given by @param wanted_types. */
		virtual sg_ret get_tree_items(std::list<TreeItem *> & list, const std::list<SGObjectTypeID> & wanted_types) const;

		virtual sg_ret attach_to_tree_under_parent(TreeItem * parent, TreeViewAttachMode attach_mode = TreeViewAttachMode::Back, const TreeItem * sibling = nullptr);


		const StandardMenuOperations & get_menu_operation_ids(void) const;
		void set_menu_operation_ids(const StandardMenuOperations & ops);

		/* See if two items are exactly the same object (i.e. whether pointers point to the same object).
		   Return true if this condition is true.
		   Return false otherwise.
		   Return false if any of the pointers is nullptr. */
		static bool the_same_object(const TreeItem * item1, const TreeItem * item2);


		virtual void display_debug_info(const QString & reference) const;

		virtual SGObjectTypeID get_type_id(void) const;

		/**
		   @brief Move child tree item up (closer to beginning of container) or down (closer to end of container)

		   @return true if move was successful
		   @return false otherwise (e.g. because child item is already at the beginning or end of container
		*/
		virtual bool move_child(TreeItem & child_tree_item, bool up);

		void update_tree_item_tooltip(void);


	//protected:

		TreeView * tree_view = nullptr; /* Reference to application's main tree, set in TreeView::insert_tree_item_at_row(). */

		bool editable = true; /* Is this item is editable? TODO_LATER: be more specific: is the data editable, or is the reference visible in the tree editable? */


		bool has_properties_dialog = false; /* Does this tree item has dialog, in which you can view or change *configurable* properties? */



		SGObjectTypeID m_type_id;
		std::vector<SGObjectTypeID> accepted_child_type_ids;

		QIcon icon; /* .isNull() may return true for this field (if child class doesn't assign anything to the icon). */

		char debug_string[100] = { 0 };

	protected:
		TreeIndex m_index; /* Set in TreeView::attach_to_tree(). */
		QString m_name;

		sg_uid_t uid = SG_UID_INITIAL;

		/* Menu items (actions) to be created and put into a
		   context menu for given tree item type. */
		StandardMenuOperations m_menu_operation_ids;

		Time timestamp; /* Invalid by default. */

		/* Is this item marked as visible in a tree of data
		   items? This does not include visibility of parent
		   items. */
		bool visible = true;

		/* Direct parent, may be different than owning layer. */
		TreeItem * m_parent_tree_item = nullptr;

		/* Some tree items may belong to a grandparent rather
		   than parent item. */
		TreeItem * m_owner_tree_item = nullptr;

	signals:
		void tree_item_changed(const QString & tree_item_name);

	public slots:
		virtual sg_ret cut_tree_item_cb(void);
		virtual sg_ret copy_tree_item_cb(void);
		virtual sg_ret delete_tree_item_cb(void);
		virtual sg_ret paste_child_tree_item_cb(void);
	};



	/* c++ UnaryPredicate. Can be passed to find_if() or remove_if().
	   Similar to TreeItem::the_same_object(). */
	class TreeItemIdentityPredicate {
	public:
		TreeItemIdentityPredicate(const TreeItem * item) : uid_(item->get_uid()) {};
		TreeItemIdentityPredicate(const TreeItem & item) : uid_(item.get_uid()) {};

		bool operator()(const TreeItem * x) const
		{
			return x->uid == this->uid_;
		}
	private:
		const sg_uid_t uid_;
	};




	template <typename T>
	bool move_tree_item_child_algo(std::list<T> & children, const T child_tree_item, bool up)
	{
		TreeItemIdentityPredicate pred(child_tree_item);
		auto child_iter = std::find_if(children.begin(), children.end(), pred);
		if (child_iter == children.end()) {
			qDebug() << "EE   Move TreeItem Child algo: failed to find iterator of child item" << child_tree_item->get_name();
			return false;
		}

		bool result = false;
		if (up) {
			if (child_iter == children.begin()) {
				qDebug() << "NN   Move TreeItem Child algo: not moving child" << child_tree_item->get_name() << "up, already at the beginning";
			} else {
				qDebug() << "II   Move TreeItem Child algo: moving child" << child_tree_item->get_name() << "up in list of children";
				std::swap(*child_iter, *std::prev(child_iter));
				result = true;
			}
		} else {
			if (std::next(child_iter) == children.end()) {
				qDebug() << "NN   Move TreeItem Child algo: not moving child" << child_tree_item->get_name() << "down, already at the end";
			} else {
				qDebug() << "II   Move TreeItem Child algo: moving child" << child_tree_item->get_name() << "down in list of children";
				std::swap(*child_iter, *std::next(child_iter));
				result = true;
			}
		}

		return result;
	}




} /* namespace SlavGPS */




Q_DECLARE_METATYPE(SlavGPS::TreeItem*)




#endif /* #ifndef _SG_TREE_ITEM_H_ */
