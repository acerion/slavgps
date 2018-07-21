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
#include "config.h"
#include "ui_builder.h"




namespace SlavGPS {




	class TreeItem;
	class TreeView;




	class TreeModel : public QStandardItemModel {
	public:
		TreeModel(TreeView * view_, QObject * parent_) : QStandardItemModel(parent_) { view = view_; };
		Qt::ItemFlags flags(const QModelIndex & index) const;
		bool canDropMimeData(const QMimeData * data, Qt::DropAction action, int row, int column, const QModelIndex & parent);
		bool dropMimeData(const QMimeData * data, Qt::DropAction action, int row, int column, const QModelIndex & parent);
		Qt::DropActions supportedDropActions() const;

	private:
		TreeView * view = NULL;
	};




	class TreeView : public QTreeView {
		Q_OBJECT
	public:
		TreeView(TreeItem * top_level_layer, QWidget * parent_widget);
		TreeView();

		~TreeView();

		bool push_tree_item_front(const TreeItem * parent_tree_item, TreeItem * tree_item);
		bool push_tree_item_back(const TreeItem * parent_tree_item, TreeItem * tree_item);
		bool insert_tree_item(const TreeItem * parent_tree_index, TreeIndex const & sibling_index, TreeItem * tree_item, bool above);
		TreeItem * get_tree_item(TreeIndex const & item_index);

		TreeIndex * get_index_from_path_str(char const * path_str);
		TreeItem * get_selected_tree_item(void);


		void set_tree_item_name(const TreeItem * tree_item);
		void set_tree_item_icon(const TreeItem * tree_item);
		void set_tree_item_timestamp(const TreeItem * tree_item, time_t timestamp);
		void set_tree_item_tooltip(const TreeItem * tree_item);

		bool get_tree_item_visibility(TreeIndex const & item_index);
		bool get_tree_item_visibility_with_parents(TreeIndex const & item_index);
		void set_tree_item_visibility(TreeIndex const &  item_index, bool visible);
		void toggle_tree_item_visibility(TreeIndex const & item_index);

		void select(TreeIndex const & index);
		void select_and_expose(TreeIndex  const & index);
		void deselect(TreeIndex const & index);

		/* Move tree item up or down in list of its siblings. */
		bool change_tree_item_position(TreeItem * tree_item, bool up);

		void detach_tree_item(TreeItem * tree_item);
		void detach_children(TreeItem * parent_tree_item);

		bool is_editing_in_progress();
		void expand(TreeIndex const & index);
		void sort_children(TreeIndex const & parent_index, TreeViewSortOrder sort_order);

		bool editing = false;

		TreeModel * tree_model = NULL;

	private slots:
		void select_cb(void);
		void data_changed_cb(const QModelIndex & top_left, const QModelIndex & bottom_right);
		bool tree_item_properties_cb(void);

	signals:
		void tree_item_needs_redraw(sg_uid_t uid);

	private:
		bool insert_tree_item_at_row(const TreeItem * parent_tree_item, TreeItem * tree_item, int row);
		QList<QStandardItem *> create_new_row(TreeItem * tree_item, const QString & name);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TREE_VIEW_INTERNAL_H_ */
