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
		TreeView(QWidget * parent_widget);
		TreeView();

		~TreeView();

		TreeIndex const & append_tree_item(TreeIndex const & parent_index, TreeItem * item, const QString & name);
		TreeIndex const & insert_tree_item(TreeIndex const & parent_index, TreeIndex const & sibling_index, TreeItem * item, bool above, const QString & name);
		TreeItem * get_tree_item(TreeIndex const & item_index);

		TreeIndex * get_index_at_pos(int x, int y);
		TreeIndex * get_index_from_path_str(char const * path_str);
		TreeItem * get_selected_tree_item(void);


		void set_tree_item_name(TreeIndex const & item_index, QString const & name);
		void set_tree_item_icon(TreeIndex const & item_index, const QIcon & icon);
		void set_tree_item_timestamp(TreeIndex const & item_index, time_t timestamp);

		bool get_tree_item_visibility(TreeIndex const & item_index);
		bool get_tree_item_visibility_with_parents(TreeIndex const & item_index);
		void set_tree_item_visibility(TreeIndex const &  item_index, bool visible);
		void toggle_tree_item_visibility(TreeIndex const & item_index);

		void select(TreeIndex const & index);
		void select_and_expose(TreeIndex  const & index);
		void unselect(TreeIndex const & index);
		bool move(TreeIndex const & index, bool up);

		void detach_item(TreeItem * item);

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
		const TreeIndex & append_row(const TreeIndex & parent_index, TreeItem * item, const QString & name);
		const TreeIndex & insert_row(const TreeIndex & parent_index, TreeItem * item, const QString & name, int row);
		QList<QStandardItem *> create_new_row(TreeItem * tree_item, const QString & name);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TREE_VIEW_INTERNAL_H_ */
