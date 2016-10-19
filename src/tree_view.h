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
 *
 */

#ifndef _SG_TREEVIEW_H_
#define _SG_TREEVIEW_H_



#include <cstdint>

#include <QStandardItem>
#include <QPersistentModelIndex>
#include <QTreeView>
#include <QString>
#include <QObject>

#include "config.h"
#include "uibuilder.h"
#include "globals.h"
#include "slav_qt.h"




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
		NAME           =  0, /* From layer->name. */
		VISIBLE        =  1, /* From layer->visible. */
		ICON           =  2, /* Provided separately. */

		/* Invisible. */
		TREE_ITEM_TYPE =  3, /* Implicit, based on function adding an item. */
		PARENT_LAYER   =  4, /* Function's argument. */
		ITEM           =  5, /* Function's argument. */
		DATA           =  6, /* Function's argument. */
		UID            =  7, /* */
		EDITABLE       =  8, /* */
		TIMESTAMP      =  9, /* Date timestamp stored in tree model to enable sorting on this value. */
		NUM_COLUMNS    = 10
	};




	class Layer;




	class TreeView : public QTreeView {
		Q_OBJECT
	public:
		TreeView(QWidget * parent);
		TreeView();

		~TreeView();

		void add_layer(GtkTreeIter *parent_iter, GtkTreeIter *iter, const char *name, Layer * parent_layer, bool above, Layer * layer, int data, LayerType layer_type, time_t timestamp);
		QStandardItem * add_layer(Layer * layer, Layer * parent_layer, QStandardItem * parent_item, bool above, int data, time_t timestamp);
		QStandardItem * insert_layer(Layer * layer, Layer * parent_layer, QStandardItem * parent_item, bool above, int data, time_t timestamp, QStandardItem * sibling);
		void insert_layer(GtkTreeIter *parent_iter, GtkTreeIter *iter, const char *name, Layer * parent_layer, bool above, Layer * layer, int data, LayerType layer_type, GtkTreeIter *sibling, time_t timestamp);
		//void add_sublayer(GtkTreeIter *parent_iter, GtkTreeIter *iter, const char *name, Layer * parent_layer, sg_uid_t sublayer_uid, SublayerType sublayer_type, GdkPixbuf *icon, bool editable, time_t timestamp);
		QPersistentModelIndex * add_sublayer(sg_uid_t sublayer_uid, SublayerType sublayer_type, Layer * parent_layer, QPersistentModelIndex * parent_index, char const * name, QIcon * icon, bool editable, time_t timestamp);


		TreeItemType get_item_type(QStandardItem * item);

		Layer * get_parent_layer(QStandardItem * item);
		Layer * get_layer(QStandardItem * item);

		SublayerType get_sublayer_type(QStandardItem * item);
		sg_uid_t     get_sublayer_uid(QStandardItem * item);
		void       * get_sublayer_uid_pointer(QStandardItem * item);

		QString get_name(QStandardItem * item);

		QStandardItem * get_selected_item();
		bool get_iter_at_pos(GtkTreeIter * iter, int x, int y);
		bool get_iter_from_path_str(GtkTreeIter * iter, char const * path_str);
		QStandardItem * get_parent_item(QStandardItem * item);


		void set_icon(QPersistentModelIndex * index, GdkPixbuf const * icon);
		void set_name(QStandardItem * item, QString const & name);
		void set_visibility(QStandardItem * item, bool visible);
		void set_visibility(QPersistentModelIndex * index, bool visible);
		void toggle_visibility(QStandardItem * item);
		void set_timestamp(QStandardItem * item, time_t timestamp);


		void select(QStandardItem * item);
		void select_and_expose(QPersistentModelIndex * index);
		void unselect(GtkTreeIter * iter);
		void erase(QPersistentModelIndex * index);
		bool move(GtkTreeIter * iter, bool up);
		bool is_visible(QStandardItem * item);
		bool is_visible_in_tree(QStandardItem * item);
		bool get_editing();
		void expand(QStandardItem * item);
		void sort_children(QPersistentModelIndex * parent_index, vik_layer_sort_order_t order);

		QStandardItem * go_up_to_layer(QStandardItem * item);

		GtkWindow * get_toolkit_window(void);
		GtkWidget * get_toolkit_widget(void);




		bool editing = false;
		bool was_a_toggle = false;
		GdkPixbuf * layer_type_icons[(int) LayerType::NUM_TYPES];

		QStandardItemModel * model = NULL;

	private slots:
		void select_cb(void);
		void data_changed_cb(const QModelIndex & top_left, const QModelIndex & bottom_right);

	private:
		void add_columns();

	signals:
		void layer_needs_redraw(sg_uid_t uid);
	};




	void treeview_init(void);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TREEVIEW_H_ */
