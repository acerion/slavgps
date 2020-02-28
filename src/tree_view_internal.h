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

#ifndef _SG_TREE_VIEW_INTERNAL_H_
#define _SG_TREE_VIEW_INTERNAL_H_




#include <list>




#include <QStandardItemModel>
#include <QTreeView>
#include <QString>
#include <QObject>




#include "tree_view.h"
#include "ui_builder.h"




namespace SlavGPS {




	class TreeItem;
	class TreeView;




	class TreeModel : public QStandardItemModel {
	public:
		TreeModel(TreeView * view_, QObject * parent_) : QStandardItemModel(parent_) { view = view_; };
		Qt::ItemFlags flags(const QModelIndex & index) const;
		bool canDropMimeData(const QMimeData * mime_data, Qt::DropAction action, int row, int column, const QModelIndex & parent_index);
		bool dropMimeData(const QMimeData * mime_data, Qt::DropAction action, int row, int column, const QModelIndex & parent_index);
		Qt::DropActions supportedDropActions() const;
		QMimeData * mimeData(const QModelIndexList & indexes) const;

		/* Will return list of mime types supplied by the model. */
		QStringList mimeTypes(void) const;

	private:
		TreeView * view = NULL;
	};




	class TreeView : public QTreeView {
		Q_OBJECT
	public:
		TreeView(TreeItem * top_level_layer, QWidget * parent_widget);
		TreeView();

		~TreeView();


		sg_ret attach_to_tree(TreeItem * parent_tree_item, TreeItem * tree_item, TreeViewAttachMode attach_mode = TreeViewAttachMode::Back, const TreeItem * sibling = NULL);
		TreeItem * get_tree_item(TreeIndex const & item_index) const;
		TreeItem * get_selected_tree_item(void) const;

		void apply_tree_item_name(const TreeItem * tree_item);
		void apply_tree_item_icon(const TreeItem * tree_item);
		void apply_tree_item_timestamp(const TreeItem * tree_item);
		void update_tree_item_tooltip(const TreeItem & tree_item);

		bool get_tree_item_visibility(const TreeItem * tree_item);
		bool get_tree_item_visibility_with_parents(const TreeItem * tree_item);
		bool apply_tree_item_visibility(const TreeItem * tree_item);

		void select_tree_item(const TreeItem * tree_item);
		void select_and_expose_tree_item(const TreeItem * tree_item);
		void deselect_tree_item(const TreeItem * tree_item);

		sg_ret get_child_rows_count(const TreeIndex & parent, int & rows) const;
		sg_ret get_child_from_row(const TreeIndex & parent, int row, TreeItem ** child_tree_item);

		void expand_tree_item(const TreeItem * tree_item);

		/* Move tree item up or down in list of its siblings. */
		bool move_tree_item(TreeItem & tree_item, bool up);

		void detach_tree_item(TreeItem * tree_item);
		void detach_children(TreeItem * parent_tree_item);

		sg_ret sort_children(TreeItem * parent_tree_item, TreeViewSortOrder sort_order);

		bool is_editing_in_progress(void) const;

		/* Get position of given tree item among its siblings. */
		sg_ret get_item_position(const TreeItem & item, bool & is_first, bool & is_last);

		TreeModel * get_tree_model(void) const { return this->tree_model; }

		void debug_print_tree(void) const;
		void debug_print_item_rec(const QStandardItem * item, int item_row_in_parents_frame, int padding) const;

	private slots:
		void tree_item_selected_cb(void);
		void data_changed_cb(const QModelIndex & top_left, const QModelIndex & bottom_right);
		bool tree_item_properties_cb(void);

	signals:
		void tree_item_needs_redraw(sg_uid_t uid);
		void tree_item_selected(void);

	private:
		sg_ret insert_tree_item_at_row(TreeItem * parent_tree_item, TreeItem * tree_item, int row);

		int column_id_to_column_idx(TreeItemPropertyID column_id);

		bool editing = false;

		TreeModel * tree_model = NULL;
		TreeItemViewFormat view_format;

		int property_id_to_column_idx(TreeItemPropertyID property_id) const;
		TreeItemPropertyID column_idx_to_property_id(int col) const;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TREE_VIEW_INTERNAL_H_ */
