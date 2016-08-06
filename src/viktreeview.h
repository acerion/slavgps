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

#ifndef _VIKING_TREEVIEW_H
#define _VIKING_TREEVIEW_H

#include <stdint.h>


#include "config.h"
#include "uibuilder.h"

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>





struct _VikTreeview {
	GtkTreeView treeview;
};
typedef struct _VikTreeview VikTreeview;




enum {
	VIK_TREEVIEW_TYPE_LAYER = 0,
	VIK_TREEVIEW_TYPE_SUBLAYER
};





namespace SlavGPS {





	class TreeView {
	public:
		TreeView();
		~TreeView();

		void add_layer(GtkTreeIter *parent_iter, GtkTreeIter *iter, const char *name, void * parent, bool above, void * item, int data, LayerType layer_type, time_t timestamp);
		void insert_layer(GtkTreeIter *parent_iter, GtkTreeIter *iter, const char *name, void * parent, bool above, void * item, int data, LayerType layer_type, GtkTreeIter *sibling, time_t timestamp);
		void add_sublayer(GtkTreeIter *parent_iter, GtkTreeIter *iter, const char *name, void * parent, void * item, int data, GdkPixbuf *icon, bool editable, time_t timestamp);


		void * get_layer(GtkTreeIter * iter);
		int    get_data(GtkTreeIter * iter);
		int    get_layer_type(GtkTreeIter * iter);
		int    get_sublayer_type(GtkTreeIter * iter);
		void * get_sublayer_uid(GtkTreeIter * iter);
		int    get_type(GtkTreeIter * iter);
		char * get_name(GtkTreeIter * iter);
		void * get_pointer(GtkTreeIter * iter);
		void * get_parent(GtkTreeIter * iter);



		void set_icon(GtkTreeIter * iter, GdkPixbuf const * icon);
		void set_name(GtkTreeIter * iter, char const * name);
		void set_visibility(GtkTreeIter * iter, bool visible);
		void toggle_visibility(GtkTreeIter * iter);
		void set_timestamp(GtkTreeIter * iter, time_t timestamp);

		void select(GtkTreeIter * iter);
		void unselect(GtkTreeIter * iter);

		void delete_(GtkTreeIter * iter);
		bool move(GtkTreeIter * iter, bool up);

		bool get_selected_iter(GtkTreeIter * iter);
		bool get_iter_at_pos(GtkTreeIter * iter, int x, int y);
		bool get_iter_from_path_str(GtkTreeIter * iter, char const * path_str);
		bool get_parent_iter(GtkTreeIter * iter, GtkTreeIter * parent);

		bool is_visible_in_tree(GtkTreeIter * iter);
		void select_iter(GtkTreeIter * iter, bool view_all);
		bool get_editing();
		//void vik_treeview_expand_toplevel(VikTreeview *vt);
		void expand(GtkTreeIter * iter);
		void sort_children(GtkTreeIter * parent, vik_layer_sort_order_t order);





		GtkTreeModel * model;
		bool editing;
		bool was_a_toggle;
		GdkPixbuf * layer_type_icons[(int) LayerType::NUM_TYPES];

		VikTreeview * vt;

	private:
		void add_columns();
	};





} /* namespace SlavGPS */





#endif /* #ifndef _VIKING_TREEVIEW_H */
