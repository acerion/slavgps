/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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

		/*
		  A TreeItem object needs to implement this method if
		  it contains (is direct parent of) any items/children
		  that need to be added to application's tree of
		  items.

		  This method should call attach_child_to_tree() on
		  any such child that needs to be added to the tree.

		  This method is not protected because it needs to be
		  available to some 'importer' class that is not a
		  friend of TreeItem class.
		*/
		virtual sg_ret attach_unattached_children(void);


		virtual sg_ret menu_add_standard_operations(QMenu & menu, const StandardMenuOperations & ops, bool in_tree_view);
		virtual sg_ret menu_add_type_specific_operations(__attribute__((unused)) QMenu & menu, __attribute__((unused)) bool in_tree_view) { return sg_ret::ok; }

		/**
		   @param in_tree_view decides if context menu is
		   shown in response to event in Tree View widget, or
		   in other widget.
		*/
		sg_ret show_context_menu(const QPoint & position, bool in_tree_view, QWidget * parent = nullptr);

		/* Since we store our main items in tree (and not in a
		   matrix), we don't need 'col' argument to this
		   function. */
		virtual sg_ret accept_dropped_child(TreeItem * tree_item, int row);
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
		*/
		virtual sg_ret add_child_item(TreeItem * child_tree_item);
		virtual sg_ret cut_child_item(TreeItem * child_tree_item);
		virtual sg_ret copy_child_item(TreeItem * child_tree_item);
		/**
		   @brief Delete a child item specified by @param item

		   This method also calls destructor of @param item.
		*/
		virtual sg_ret delete_child_item(TreeItem * child_tree_item, bool confirm_deleting);


		/**
		   @brief Get count of child items in underlying Qt Model, to which this tree item is attached

		   I'm using "rows" in function's name to stress that
		   this is about number of rows in the Model, not
		   about e.g. number of Trackpoints (which aren't tree
		   items) in TRW Track.

		   Derived classes may choose to override the
		   implementation if they are sure that they don't
		   have any child rows. One tree item that may do this
		   is Coordinates Layer.

		   @return -1 on errors (e.g. tree item not attached to the model),
		   @return 0 if no children items are present,
		   @return positive number - number of child items
		*/
		virtual int child_rows_count(void) const;

		/**
		   @brief Get item's child from given row

		   If the row contains NULL pointer to the child, the
		   function returns sg_ret::err - caller of the
		   function doesn't have to do additional check of
		   NULL-ness of returned pointer.
		*/
		sg_ret child_from_row(int row, TreeItem ** child_tree_item) const;

		/**
		   @brief Find child tree item by its uid
		*/
		TreeItem * find_child_by_uid(sg_uid_t child_uid) const;

		/**
		   @brief Find first child tree item with given @param name

		   Uses a case sensitive find. If there are more than
		   one child with given @param name, the function
		   ignores it.
		*/
		TreeItem * find_child_by_name(const QString & name) const;

		/**
		   @brief Get layer associated with this tree item

		   Either the tree item itself is a layer, or a
		   sublayer has its parent/owning layer.  Return one
		   of these.
		*/
		Layer * immediate_layer(void);

		/**
		   @brief Get list of child item UIDs

		   @return count of items on the list - may be zero
		*/
		int list_child_uids(std::list<sg_uid_t> & list) const;

		/**
		   @brief Get list of child items

		   @return count of items on the list - may be zero
		*/
		int list_tree_items(std::list<TreeItem *> & list) const;


		/**
		   @brief Get a parent of this TreeItem - a parent that is a layer

		   For most of TreeItem types the parent tree item
		   already is a layer. But for some TreeItem types
		   (e.g. Waypoint or Track) we have to go up one more
		   step (to grand-parent) to find the layer that
		   contains/owns/manages the tree item.
		*/
		virtual Layer * parent_layer(void) const = 0;


		/**
		   @brief Set TreeItem::m_parent member

		   The member should be always valid, even for
		   TreeItems that aren't attached to Qt Model.
		*/
		sg_ret set_parent_member(TreeItem * parent);

		/**
		   @brief Get direct parent tree item - get it from
		   TreeItem::m_parent
		*/
		TreeItem * parent_member(void) const;

		/**
		   @brief Set 'visibility' flag of only direct children to @param visible

		   This only modifies the 'visibility' flag of child
		   tree items. Actual redrawing of the child tree
		   items must be triggered by caller separately,
		   e.g. by calling TreeItem::emit_tree_item_changed()
		   by tree item that toggled the 'visibility' flag of
		   its direct children.

		   @brief Return count of child items affected
		*/
		int set_direct_children_only_visibility_flag(bool visible);

		/**
		   @brief Toggle 'visibility' flag of only direct children

		   This only modifies the 'visibility' flag of child
		   tree items. Actual redrawing of the child tree
		   items must be triggered by caller separately,
		   e.g. by calling TreeItem::emit_tree_item_changed()
		   by tree item that toggled the 'visibility' flag of
		   its direct children.

		   @brief Return count of child items affected
		*/
		int toggle_direct_children_only_visibility_flag(void);


		/* Get tree items (direct and indirect children of the
		   layer) of types given by @param wanted_types. */
		virtual sg_ret get_tree_items(std::list<TreeItem *> & list, const std::list<SGObjectTypeID> & wanted_types) const;

		virtual sg_ret attach_child_to_tree(TreeItem * child, int row = -1);


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
		   @brief Move child tree item up or down

		   The function may return false when child item is
		   already at the beginning or end of list of child
		   items

		   The function may return false when parent doesn't
		   allow moving its child items up and down.

		   @return true if move was successful
		   @return false otherwise
		*/
		virtual bool move_child(TreeItem & child_tree_item, bool up);

		void update_tree_item_tooltip(void);


		/**
		   Similar to C++ container's ::clear() method: call
		   destructor for all elements of this container,
		   remove the elements, leaving zero elements in the
		   container.

		   Removes both attached and unattached immediate
		   children. May recursively call ::clear() on child
		   items.
		*/
		void clear(void);

		/**
		   Similar to C++ container's ::size() method.  Returns
		   count of immediate children that are attached to Tree.

		   Derived classes may want to re-implement this
		   method to always return zero.
		*/
		virtual int attached_size(void) const;

		/**
		   Similar to C++ container's ::size() method.
		   Returns count of immediate children that are NOT
		   attached to Tree.

		   Derived classes may want to re-implement this
		   method to always return zero.
		*/
		virtual int unattached_size(void) const;

		/**
		   Similar to C++ container's ::empty()
		   method. Returns information whether there are zero
		   immediate children attached to Tree.

		   Derived classes may want to re-implement this
		   method to always return true.
		*/
		virtual bool attached_empty(void) const;

		/**
		   Similar to C++ container's ::empty()
		   method. Returns information whether there are zero
		   immediate children NOT attached to Tree.
		*/
		virtual bool unattached_empty(void) const;

		/**
		   Update properties of given tree item that may have
		   changed after e.g. a new child item has been added
		   or removed, or when something else has been changed
		   about the tree item.

		   The updated properties may be a tooltip, an icon or
		   a coordinates bounding box.
		*/
		virtual sg_ret update_properties(void);



	//protected:

		TreeView * tree_view = nullptr; /* Reference to application's main tree, set in TreeView::insert_tree_item_at_row(). */

		bool editable = true; /* Is this item is editable? TODO_LATER: be more specific: is the data editable, or is the reference visible in the tree editable? */


		bool has_properties_dialog = false; /* Does this tree item has dialog, in which you can view or change *configurable* properties? */



		SGObjectTypeID m_type_id;
		std::vector<SGObjectTypeID> accepted_child_type_ids;

		QIcon icon; /* .isNull() may return true for this field (if child class doesn't assign anything to the icon). */

		char debug_string[100] = { 0 };

	protected:

		/**
		   @Attach given @param child as child tree item in Qt Model

		   The method can be called when a single @param child
		   is added being to already connected parent
		   container, or when (in post_read()) all unattached
		   children are connected to Qt Model.
		*/
		sg_ret attach_as_tree_item_child(TreeItem * child, int row);


		TreeIndex m_index; /* Set in TreeView::attach_to_tree(). */
		QString m_name;

		/* Child items that have been read from some source,
		   but aren't attached to Qt Model yet. */
		std::list<TreeItem *> unattached_children;

		sg_uid_t uid = SG_UID_INITIAL;

		/* Menu items (actions) to be created and put into a
		   context menu for given tree item type. */
		StandardMenuOperations m_menu_operation_ids;

		Time timestamp; /* Invalid by default. */

		/* Is this item marked as visible in a tree of data
		   items? This does not include visibility of parent
		   items. */
		bool m_visible = true;

	signals:
		void tree_item_changed(const QString & tree_item_name);

		/**
		   E.g. count of child items has changed or bbox has
		   changed. Used to notify of such changes in child
		   tree item to parent tree item.

		   To be connected to
		   TreeItem::properties_changed_cb(). To be used
		   instead of direct method call to avoid loops.
		*/
		void properties_changed(const QString & where);

	public slots:
		/* Tree Item can contain other Tree Items and should
		   be notified about changes in them. */
		virtual sg_ret child_tree_item_changed_cb(const QString & child_tree_item_name);

		virtual sg_ret cut_tree_item_cb(void);
		virtual sg_ret copy_tree_item_cb(void);
		virtual sg_ret delete_tree_item_cb(void);
		virtual sg_ret paste_child_tree_item_cb(void);

		void properties_changed_cb(const QString & where);

	private:
		/*
		  Direct parent. For some TreeItem types
		  (e.g. Waypoint) this is not a layer but some
		  intermediate container.

		  TODO: The parent item can also be obtained from Tree
		  (Model) relations between tree items, so extra care
		  must be taken to ensure that these places have
		  consistent data.
		*/
		TreeItem * m_parent = nullptr;
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




} /* namespace SlavGPS */




Q_DECLARE_METATYPE(SlavGPS::TreeItem*)




#endif /* #ifndef _SG_TREE_ITEM_H_ */
