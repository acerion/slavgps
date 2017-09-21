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



#include <cstdint>

#include <QStandardItem>
#include <QPersistentModelIndex>
#include <QTreeView>
#include <QString>
#include <QObject>

#include "tree_view.h"
#include "config.h"
#include "ui_builder.h"
#include "globals.h"




namespace SlavGPS {




	class Layer;
	class TreeItem;
	class LayersPanel;
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
		TreeView(LayersPanel * panel);
		TreeView();

		~TreeView();

		TreeIndex const & add_layer(Layer * layer, Layer * parent_layer, TreeIndex const & parent_index, bool above, time_t timestamp);
		TreeIndex const & insert_layer(Layer * layer, Layer * parent_layer, TreeIndex const & parent_index, bool above, time_t timestamp, TreeIndex const & sibling_index);
		TreeIndex const & add_sublayer(TreeItem * sublayer, Layer * parent_layer, TreeIndex const & parent_index, const QString & name, QIcon * icon, bool editable, time_t timestamp);

		TreeItemType get_item_type(TreeIndex const & index);

		Layer * get_parent_layer(TreeIndex const & index);
		Layer * get_layer(TreeIndex const & index);
		TreeItem * get_sublayer(TreeIndex const & index);

		QString get_name(TreeIndex const & index);

		TreeIndex const & get_selected_item();
		TreeIndex * get_index_at_pos(int x, int y);
		TreeIndex * get_index_from_path_str(char const * path_str);


		void set_icon(TreeIndex const & index, QIcon const * icon);
		void set_name(TreeIndex const &  index, QString const & name);
		void set_visibility(TreeIndex const &  index, bool visible);
		void toggle_visibility(TreeIndex const & index);
		void set_timestamp(TreeIndex const & index, time_t timestamp);


		void select(TreeIndex const & index);
		void select_and_expose(TreeIndex  const & index);
		void unselect(TreeIndex const & index);
		void erase(TreeIndex const & index);
		bool move(TreeIndex const & index, bool up);
		bool is_visible(TreeIndex const & index);
		bool is_visible_in_tree(TreeIndex const & index);
		bool get_editing();
		void expand(TreeIndex const & index);
		void sort_children(TreeIndex const & parent_index, sort_order_t order);

		LayersPanel * get_layers_panel(void);

		TreeIndex const go_up_to_layer(TreeIndex const & index);
		TreeIndex const go_up_to_layer(TreeIndex const & index, LayerType layer_type);

		bool editing = false;
		bool was_a_toggle = false;

		/* TODO: rename or remove this field. There is already QAbstractItemView::model(). */
		TreeModel * model = NULL;

	private slots:
		void select_cb(void);
		void data_changed_cb(const QModelIndex & top_left, const QModelIndex & bottom_right);

	signals:
		void layer_needs_redraw(sg_uid_t uid);

	private:
		LayersPanel * layers_panel = NULL; /* Just a reference to panel, in which the tree is embedded. */
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TREE_VIEW_INTERNAL_H_ */
