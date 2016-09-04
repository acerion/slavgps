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

#include "config.h"
#include "uibuilder.h"
#include "globals.h"

#ifdef SLAVGPS_QT
#include <QStandardItem>
#include <QTreeView>
#include <QString>
#include "slav_qt.h"
#else
#include <gtk/gtk.h>
#endif



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
		NAME        =  0,
		VISIBLE     =  1,
		ICON        =  2,


		/* Invisible. */
		TYPE        =  3,
		PARENT      =  4,
		ITEM        =  5,
		DATA        =  6,
		UID         =  7,
		EDITABLE    =  8,
		TIMESTAMP   =  9, /* Date timestamp stored in tree model to enable sorting on this value. */
		NUM_COLUMNS = 10
	};




	class Layer;




#ifdef SLAVGPS_QT
	class TreeView : public QTreeView {
#else
	class TreeView {
#endif
	public:
#ifdef SLAVGPS_QT
		TreeView(QWidget * parent);
#else
		TreeView();
#endif

		~TreeView();

		void add_layer(GtkTreeIter *parent_iter, GtkTreeIter *iter, const char *name, Layer * parent_layer, bool above, Layer * layer, int data, LayerType layer_type, time_t timestamp);
		QStandardItem * add_layer(QStandardItem * parent_item, char const * name, Layer * layer, int data, LayerType layer_type, time_t timestamp);
		void insert_layer(GtkTreeIter *parent_iter, GtkTreeIter *iter, const char *name, Layer * parent_layer, bool above, Layer * layer, int data, LayerType layer_type, GtkTreeIter *sibling, time_t timestamp);
		void add_sublayer(GtkTreeIter *parent_iter, GtkTreeIter *iter, const char *name, Layer * parent_layer, sg_uid_t sublayer_uid, SublayerType sublayer_type, GdkPixbuf *icon, bool editable, time_t timestamp);


		TreeItemType get_item_type(GtkTreeIter * iter);

		Layer * get_parent_layer(GtkTreeIter * iter);
		Layer * get_layer(GtkTreeIter * iter);

		SublayerType get_sublayer_type(GtkTreeIter * iter);
		sg_uid_t     get_sublayer_uid(GtkTreeIter * iter);
		void       * get_sublayer_uid_pointer(GtkTreeIter * iter);

		QString get_name(QStandardItem * item);

		QStandardItem * get_selected_item();
		bool get_iter_at_pos(GtkTreeIter * iter, int x, int y);
		bool get_iter_from_path_str(GtkTreeIter * iter, char const * path_str);
		bool get_parent_iter(GtkTreeIter * iter, GtkTreeIter * parent);


		void set_icon(GtkTreeIter * iter, GdkPixbuf const * icon);
		void set_name(QStandardItem * item, QString const & name);
		void set_visibility(QStandardItem * item, bool visible);
		void toggle_visibility(QStandardItem * item);
		void set_timestamp(GtkTreeIter * iter, time_t timestamp);


		void select(GtkTreeIter * iter);
		void select_and_expose(GtkTreeIter * iter);
		void unselect(GtkTreeIter * iter);
		void erase(GtkTreeIter * iter);
		bool move(GtkTreeIter * iter, bool up);
		bool is_visible_in_tree(GtkTreeIter * iter);
		bool get_editing();
		void expand(QStandardItem * item);
		void sort_children(GtkTreeIter * parent, vik_layer_sort_order_t order);

		bool go_up_to_layer(GtkTreeIter * iter);

		GtkWindow * get_toolkit_window(void);
		GtkWidget * get_toolkit_widget(void);




		bool editing;
		bool was_a_toggle;
		GdkPixbuf * layer_type_icons[(int) LayerType::NUM_TYPES];

#ifdef SLAVGPS_QT
		QStandardItemModel * model;
#else
		GtkTreeView * tv_;
		GtkTreeModel * model;
#endif

	private:
		void add_columns();
	};




	void treeview_init(void);




} /* namespace SlavGPS */



	Q_DECLARE_METATYPE(SlavGPS::Layer*);


#endif /* #ifndef _SG_TREEVIEW_H_ */
