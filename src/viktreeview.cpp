/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2010-2015, Rob Norris <rw_norris@hotmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstring>
#include <cstdlib>
#include <cassert>

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

//#include "viking.h"
#include "config.h"
#include "viklayer.h"
#include "vikwindow.h"
#include "viktreeview.h"
#include "viklayerspanel.h"
#include "globals.h"
#include "uibuilder.h"




using namespace SlavGPS;




#define VIK_TREEVIEW_TYPE            (vik_treeview_get_type ())
#define VIK_TREEVIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_TREEVIEW_TYPE, VikTreeview))
#define VIK_TREEVIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_TREEVIEW_TYPE, VikTreeviewClass))
#define IS_VIK_TREEVIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_TREEVIEW_TYPE))
#define IS_VIK_TREEVIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_TREEVIEW_TYPE))

typedef struct _VikTreeviewClass VikTreeviewClass;

struct _VikTreeviewClass
{
	GtkTreeViewClass vbox_class;
	void (* item_edited) (VikTreeview *vt, GtkTreeIter *iter, const char *new_name);
	void (* item_toggled) (VikTreeview *vt,GtkTreeIter *iter);
};

GType vik_treeview_get_type();

enum {
	VT_ITEM_EDITED_SIGNAL,
	VT_ITEM_TOGGLED_SIGNAL,
	VT_LAST_SIGNAL
};

static unsigned int treeview_signals[VT_LAST_SIGNAL] = { 0, 0 };

static GObjectClass *parent_class;




#define TREEVIEW_GET(model,iter,what,dest) gtk_tree_model_get(GTK_TREE_MODEL(model),(iter),(what),(dest),-1)




enum {
	COLUMN_NAME = 0,
	COLUMN_VISIBLE,
	COLUMN_ICON,


	/* Invisible. */
	COLUMN_TYPE,
	COLUMN_PARENT,
	COLUMN_ITEM,
	COLUMN_DATA,
	COLUMN_UID,
	COLUMN_EDITABLE,
	COLUMN_TIMESTAMP, /* Date timestamp stored in tree model to enable sorting on this value. */
	NUM_COLUMNS
};




/* TODO: find, make "static" and put up here all non-"a_" functions. */
static void vik_treeview_finalize(GObject *gob);

static int vik_treeview_drag_data_received(GtkTreeDragDest *drag_dest, GtkTreePath *dest, GtkSelectionData *selection_data);
static int vik_treeview_drag_data_delete(GtkTreeDragSource *drag_source, GtkTreePath *path);

G_DEFINE_TYPE (VikTreeview, vik_treeview, GTK_TYPE_TREE_VIEW)

static void vik_cclosure_marshal_VOID__POINTER_POINTER(GClosure     * closure,
						       GValue       * return_value,
						       unsigned int   n_param_vals,
						       const GValue * param_values,
						       void         * invocation_hint,
						       void         * marshal_data)
{
	typedef bool (*VikMarshalFunc_VOID__POINTER_POINTER) (void          * data1,
							      gconstpointer   arg_1,
							      gconstpointer   arg_2,
							      void          * data2);

	register VikMarshalFunc_VOID__POINTER_POINTER callback;
	register GCClosure* cc = (GCClosure*) closure;
	register void * data1, * data2;

	if (n_param_vals != 3) {
		return;
	}

	if (G_CCLOSURE_SWAP_DATA(closure)) {
		data1 = closure->data;
		data2 = g_value_peek_pointer(param_values + 0);
	} else {
		data1 = g_value_peek_pointer(param_values + 0);
		data2 = closure->data;
	}
	callback = (VikMarshalFunc_VOID__POINTER_POINTER) (marshal_data ? marshal_data : cc->callback);
	callback(data1,
		  g_value_get_pointer(param_values + 1),
		  g_value_get_pointer(param_values + 2),
		  data2);
}




static void vik_treeview_class_init(VikTreeviewClass *klass)
{
	/* Destructor. */
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = vik_treeview_finalize;

	parent_class = (GObjectClass *) g_type_class_peek_parent(klass);

	treeview_signals[VT_ITEM_EDITED_SIGNAL] = g_signal_new("item_edited", G_TYPE_FROM_CLASS (klass), (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION), G_STRUCT_OFFSET (VikTreeviewClass, item_edited), NULL, NULL,
								vik_cclosure_marshal_VOID__POINTER_POINTER, G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);

	treeview_signals[VT_ITEM_TOGGLED_SIGNAL] = g_signal_new("item_toggled", G_TYPE_FROM_CLASS (klass), (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION), G_STRUCT_OFFSET (VikTreeviewClass, item_toggled), NULL, NULL,
								 g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);
}




static void vik_treeview_edited_cb(GtkCellRendererText *cell, char *path_str, const char *new_name, TreeView * tree_view)
{
	tree_view->editing = false;
	GtkTreeIter iter;

	/* Get type and data. */
	tree_view->get_iter_from_path_str(&iter, path_str);

	g_signal_emit(G_OBJECT(tree_view->vt), treeview_signals[VT_ITEM_EDITED_SIGNAL], 0, &iter, new_name);
}




static void vik_treeview_edit_start_cb(GtkCellRenderer *cell, GtkCellEditable *editable, char *path, TreeView * tree_view)
{
	tree_view->editing = true;
}




static void vik_treeview_edit_stop_cb(GtkCellRenderer *cell, TreeView * tree_view)
{
	tree_view->editing = false;
}




static void vik_treeview_toggled_cb(GtkCellRendererToggle *cell, char *path_str, TreeView * tree_view)
{
	GtkTreeIter iter_toggle;
	GtkTreeIter iter_selected;

	/* Get type and data. */
	tree_view->get_iter_from_path_str(&iter_toggle, path_str);

	GtkTreePath *tp_toggle = gtk_tree_model_get_path(tree_view->model, &iter_toggle);

	if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW (tree_view->vt)), NULL, &iter_selected)) {
		GtkTreePath *tp_selected = gtk_tree_model_get_path(tree_view->model, &iter_selected);
		if (gtk_tree_path_compare(tp_toggle, tp_selected)) {
			/* Toggle set on different path
			   therefore prevent subsequent auto selection (otherwise no action needed). */
			tree_view->was_a_toggle = true;
		}
		gtk_tree_path_free(tp_selected);
	} else {
		/* Toggle set on new path
		   therefore prevent subsequent auto selection. */
		tree_view->was_a_toggle = true;
	}

	gtk_tree_path_free(tp_toggle);

	g_signal_emit(G_OBJECT (tree_view->vt), treeview_signals[VT_ITEM_TOGGLED_SIGNAL], 0, &iter_toggle);
}




/* Inspired by GTK+ test
 * http://git.gnome.org/browse/gtk+/tree/tests/testtooltips.c
 */
static bool vik_treeview_tooltip_cb(GtkWidget  * widget,
				    int          x,
				    int          y,
				    bool         keyboard_tip,
				    GtkTooltip * tooltip,
				    void       * data)
{
	GtkTreeIter iter;
	GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
	GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
	GtkTreePath *path = NULL;

	char buffer[256];

	if (!gtk_tree_view_get_tooltip_context(tree_view, &x, &y,
						keyboard_tip,
						&model, &path, &iter)) {
		return false;
	}

	/* ATM normally treeview doesn't call into layers - maybe another level of redirection required? */
	TreeItemType tree_item_type;
	gtk_tree_model_get(model, &iter, COLUMN_TYPE, &tree_item_type, -1);
	if (tree_item_type == TreeItemType::SUBLAYER) {

		SublayerType sublayer_type = SublayerType::NONE;
		gtk_tree_model_get(model, &iter, COLUMN_DATA, &sublayer_type, -1);

		sg_uid_t * sublayer_uid = NULL;
		gtk_tree_model_get(model, &iter, COLUMN_UID, &sublayer_uid, -1);

		Layer * parent_layer = NULL;
		gtk_tree_model_get(model, &iter, COLUMN_PARENT, &parent_layer, -1);

		snprintf(buffer, sizeof(buffer), "%s", parent_layer->sublayer_tooltip(sublayer_type, (sg_uid_t) KPOINTER_TO_UINT (sublayer_uid)));

	} else if (tree_item_type == TreeItemType::LAYER) {
		Layer * layer;
		gtk_tree_model_get(model, &iter, COLUMN_ITEM, &layer, -1);
		snprintf(buffer, sizeof(buffer), "%s", layer->tooltip());
	} else {
		gtk_tree_path_free(path);
		return false;
	}

	/* Don't display null strings :) */
	if (strncmp(buffer, "(null)", 6) == 0) {
		gtk_tree_path_free(path);
		return false;
	} else {
		/* No point in using (Pango) markup verson - gtk_tooltip_set_markup()
		   especially as waypoint comments may well contain HTML markup which confuses the pango markup parser.
		   This plain text is probably faster too. */
		gtk_tooltip_set_text(tooltip, buffer);
	}

	gtk_tree_view_set_tooltip_row(tree_view, tooltip, path);

	gtk_tree_path_free(path);

	return true;
}




TreeItemType TreeView::get_item_type(GtkTreeIter *iter)
{
	TreeItemType rv;
	TREEVIEW_GET (this->model, iter, COLUMN_TYPE, &rv);
	return rv;
}




char * TreeView::get_name(GtkTreeIter * iter)
{
	char *rv;
	TREEVIEW_GET (this->model, iter, COLUMN_NAME, &rv);
	return rv;
}




SublayerType TreeView::get_sublayer_type(GtkTreeIter * iter)
{
	SublayerType rv;
	TREEVIEW_GET (this->model, iter, COLUMN_DATA, &rv);
	return rv;
}




void * TreeView::get_sublayer_uid_pointer(GtkTreeIter * iter)
{
	sg_uid_t * uid;
	TREEVIEW_GET (this->model, iter, COLUMN_UID, &uid);
	return uid;
}




sg_uid_t TreeView::get_sublayer_uid(GtkTreeIter * iter)
{
	void * uid = this->get_sublayer_uid_pointer(iter);
	return (sg_uid_t) KPOINTER_TO_UINT (uid);
}




Layer * TreeView::get_layer(GtkTreeIter * iter)
{
	Layer * layer;
	TREEVIEW_GET (this->model, iter, COLUMN_ITEM, &layer);
	return layer;
}




void TreeView::set_timestamp(GtkTreeIter *iter, time_t timestamp)
{
	gtk_tree_store_set (GTK_TREE_STORE(this->model), iter, COLUMN_TIMESTAMP, (int64_t) timestamp, -1);
}




Layer * TreeView::get_parent_layer(GtkTreeIter *iter)
{
	Layer * parent_layer;
	TREEVIEW_GET (this->model, iter, COLUMN_PARENT, &parent_layer);
	return parent_layer;
}




bool TreeView::get_iter_from_path_str(GtkTreeIter * iter, char const * path_str)
{
	return gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL (this->model), iter, path_str);
}




/**
 * Get visibility of an item considering visibility of all parents
 * i.e. if any parent is off then this item will also be considered
 * off (even though itself may be marked as on).
 */
bool TreeView::is_visible_in_tree(GtkTreeIter * iter)
{
	bool ans;
	TREEVIEW_GET (this->model, iter, COLUMN_VISIBLE, &ans);

	if (!ans) {
		return ans;
	}

	GtkTreeIter parent;
	GtkTreeIter child = * iter;
	while (gtk_tree_model_iter_parent(this->model, &parent, &child)) {
		/* Visibility of this parent. */
		TREEVIEW_GET (this->model, &parent, COLUMN_VISIBLE, &ans);
		/* If not visible, no need to check further ancestors. */
		if (!ans) {
			break;
		}
		child = parent;
	}
	return ans;
}




void TreeView::add_columns()
{
	int col_offset;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* Layer column. */
	renderer = gtk_cell_renderer_text_new();
	g_signal_connect(renderer, "edited", G_CALLBACK (vik_treeview_edited_cb), this);

	g_signal_connect(renderer, "editing-started", G_CALLBACK (vik_treeview_edit_start_cb), this);
	g_signal_connect(renderer, "editing-canceled", G_CALLBACK (vik_treeview_edit_stop_cb), this);

	g_object_set (G_OBJECT (renderer), "xalign", 0.0, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	col_offset = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (this->vt),
								  -1, _("Layer Name"),
								  renderer, "text",
								  COLUMN_NAME,
								  "editable", COLUMN_EDITABLE,
								  NULL);

	/* ATM the minimum overall width (and starting default) of the treeview size is determined
	   by the buttons added to the bottom of the layerspanel. */
	column = gtk_tree_view_get_column(GTK_TREE_VIEW (this->vt), col_offset - 1);
	gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN (column),
					GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_expand(GTK_TREE_VIEW_COLUMN (column), true);

	/* Layer type. */
	renderer = gtk_cell_renderer_pixbuf_new();

	g_object_set(G_OBJECT (renderer), "xalign", 0.5, NULL);

	col_offset = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (this->vt),
								 -1, "",
								 renderer, "pixbuf",
								 COLUMN_ICON,
								 NULL);

	column = gtk_tree_view_get_column(GTK_TREE_VIEW (this->vt), col_offset - 1);
	gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN (column),
					GTK_TREE_VIEW_COLUMN_AUTOSIZE);

	/* Layer visible. */
	renderer = gtk_cell_renderer_toggle_new();
	g_object_set(G_OBJECT (renderer), "xalign", 0.5, NULL);

	g_signal_connect(renderer, "toggled", G_CALLBACK (vik_treeview_toggled_cb), this);

	col_offset = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (this->vt),
								 -1, "",
								 renderer,
								 "active",
								 COLUMN_VISIBLE,
								 NULL);

	column = gtk_tree_view_get_column(GTK_TREE_VIEW (this->vt), col_offset - 1);
	gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN (column),
					GTK_TREE_VIEW_COLUMN_AUTOSIZE);


	g_object_set(GTK_TREE_VIEW (this->vt), "has-tooltip", true, NULL);
	g_signal_connect(GTK_TREE_VIEW (this->vt), "query-tooltip", G_CALLBACK (vik_treeview_tooltip_cb), this);
}




static void select_cb(GtkTreeSelection * selection, void * data)
{
	GtkTreeIter iter;
	if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		return;
	}

	TreeView * tree_view = (TreeView *) data;
	SublayerType sublayer_type = tree_view->get_sublayer_type(&iter);
	TreeItemType tree_item_type = tree_view->get_item_type(&iter);
	sg_uid_t sublayer_uid = tree_view->get_sublayer_uid(&iter);

	if (tree_item_type == TreeItemType::SUBLAYER) {
		if (!tree_view->go_up_to_layer(&iter)) {
			return;
		}
	}

	Layer * layer = tree_view->get_layer(&iter);
	Window * window = layer->get_window();
	window->selected_layer(layer);

	/* Apply settings now we have the all details. */
	if (vik_layer_selected(layer,
			       sublayer_type,
			       sublayer_uid,
			       tree_item_type,
			       window->get_layers_panel())) {

		/* Redraw required. */
		window->get_layers_panel()->emit_update();
	}
}




/* Go up the tree to find a Layer. */
bool TreeView::go_up_to_layer(GtkTreeIter * iter)
{
	GtkTreeIter this_iter = *iter;
	GtkTreeIter parent_iter;

	while (TreeItemType::LAYER != this->get_item_type(&this_iter)) {
		if (!this->get_parent_iter(&this_iter, &parent_iter)) {
			return false;
		}
		this_iter = parent_iter;
	}

	*iter = this_iter; /* Don't use parent_iter. If while() loop executes zero times, parent_iter is invalid. */
	return true;
}




static int vik_treeview_selection_filter(GtkTreeSelection *selection, GtkTreeModel *model, GtkTreePath *path, int path_currently_selected, void * data)
{
	TreeView * tree_view = (TreeView *) data;

	if (tree_view->was_a_toggle) {
		tree_view->was_a_toggle = false;
		return false;
	}

	return true;
}




void vik_treeview_init(VikTreeview *vt)
{
}




bool TreeView::get_parent_iter(GtkTreeIter * iter, GtkTreeIter * parent)
{
	return gtk_tree_model_iter_parent(GTK_TREE_MODEL(this->model), parent, iter);
}




bool TreeView::move(GtkTreeIter * iter, bool up)
{
	TreeItemType t = this->get_item_type(iter);
	if (t == TreeItemType::LAYER) {
		GtkTreeIter switch_iter;
		if (up) {
			/* Iter to path to iter. */
			GtkTreePath *path = gtk_tree_model_get_path(this->model, iter);
			if (!gtk_tree_path_prev(path) || !gtk_tree_model_get_iter(this->model, &switch_iter, path)) {
				gtk_tree_path_free(path);
				return false;
			}
			gtk_tree_path_free(path);
		} else {
			switch_iter = *iter;
			if (!gtk_tree_model_iter_next(this->model, &switch_iter)) {
				return false;
			}
		}
		gtk_tree_store_swap(GTK_TREE_STORE(this->model), iter, &switch_iter);
		return true;
		/* Now, the easy part. actually switching them, not the GUI. */
	} /* If item is map. */
	return false;
}




bool TreeView::get_iter_at_pos(GtkTreeIter * iter, int x, int y)
{
	GtkTreePath * path;
	(void) gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW (this->vt), x, y, &path, NULL, NULL, NULL);
	if (!path) {
		return false;
	}

	gtk_tree_model_get_iter(GTK_TREE_MODEL (this->model), iter, path);
	gtk_tree_path_free(path);
	return true;
}




void TreeView::select_and_expose(GtkTreeIter * iter)
{
	GtkTreeView * tree_view = GTK_TREE_VIEW (this->vt);
	GtkTreePath * path = gtk_tree_model_get_path(gtk_tree_view_get_model(tree_view), iter);

	gtk_tree_view_expand_to_path(tree_view, path);
	this->select(iter);
	gtk_tree_view_scroll_to_cell(tree_view, path, gtk_tree_view_get_expander_column(tree_view), false, 0.0, 0.0);
	gtk_tree_path_free(path);
}




bool TreeView::get_selected_iter(GtkTreeIter * iter)
{
	return gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW (this->vt)), NULL, iter);
}




bool TreeView::get_editing()
{
	/* Don't know how to get cell for the selected item. */
	//return KPOINTER_TO_INT(g_object_get_data(G_OBJECT(cell), "editing"));

	/* Instead maintain our own value applying to the whole tree. */
	return this->editing;
}




void TreeView::erase(GtkTreeIter *iter)
{
	gtk_tree_store_remove(GTK_TREE_STORE (this->model), iter);
}




void TreeView::set_icon(GtkTreeIter * iter, const GdkPixbuf * icon)
{
	if (!iter) {
		return;
	}
	gtk_tree_store_set(GTK_TREE_STORE(this->model), iter, COLUMN_ICON, icon, -1);
}




void TreeView::set_name(GtkTreeIter *iter, char const * name)
{
	if (!iter || !name) {
		return;
	}
	gtk_tree_store_set(GTK_TREE_STORE(this->model), iter, COLUMN_NAME, name, -1);
}




void TreeView::set_visibility(GtkTreeIter *iter, bool visible)
{
	if (!iter) {
		return;
	}
	gtk_tree_store_set(GTK_TREE_STORE(this->model), iter, COLUMN_VISIBLE, visible, -1);
}




void TreeView::toggle_visibility(GtkTreeIter *iter)
{
	if (!iter) {
		return;
	}
	bool visibility;
	TREEVIEW_GET(this->model, iter, COLUMN_VISIBLE, &visibility);
	gtk_tree_store_set(GTK_TREE_STORE(this->model), iter, COLUMN_VISIBLE, !visibility, -1);
}




void TreeView::expand(GtkTreeIter * iter)
{
	GtkTreePath * path = gtk_tree_model_get_path(this->model, iter);
	gtk_tree_view_expand_row(GTK_TREE_VIEW(this->vt), path, false);
	gtk_tree_path_free(path);
}




void TreeView::select(GtkTreeIter *iter)
{
	gtk_tree_selection_select_iter(gtk_tree_view_get_selection (GTK_TREE_VIEW (this->vt)), iter);
}




void TreeView::unselect(GtkTreeIter *iter)
{
	gtk_tree_selection_unselect_iter(gtk_tree_view_get_selection(GTK_TREE_VIEW (this->vt)), iter);
}




void TreeView::add_layer(GtkTreeIter * parent_iter,
			 GtkTreeIter *iter,
			 const char * name,
			 Layer * parent_layer,
			 bool above,
			 Layer * layer,
			 int data,
			 LayerType layer_type,
			 time_t timestamp)
{
	assert (iter);
	if (above) {
		gtk_tree_store_prepend(GTK_TREE_STORE (this->model), iter, parent_iter);
	} else {
		gtk_tree_store_append(GTK_TREE_STORE (this->model), iter, parent_iter);
	}
	gtk_tree_store_set(GTK_TREE_STORE (this->model), iter,
			   COLUMN_NAME, name,
			   COLUMN_VISIBLE, true,
			   COLUMN_TYPE, TreeItemType::LAYER,
			   COLUMN_PARENT, parent_layer,
			   COLUMN_ITEM, layer,
			   COLUMN_DATA, data,
			   COLUMN_EDITABLE, parent_layer == NULL ? false : true,
			   COLUMN_ICON, layer_type == LayerType::NUM_TYPES ? 0 : this->layer_type_icons[(int) layer_type],
			   COLUMN_TIMESTAMP, (int64_t) timestamp,
			   -1);
}




void TreeView::insert_layer(GtkTreeIter * parent_iter,
			    GtkTreeIter * iter,
			    const char * name,
			    Layer * parent_layer,
			    bool above,
			    Layer * layer,
			    int data,
			    LayerType layer_type,
			    GtkTreeIter * sibling,
			    time_t timestamp)
{
	assert (iter);
	if (sibling) {
		if (above) {
			gtk_tree_store_insert_before(GTK_TREE_STORE (this->model), iter, parent_iter, sibling);
		} else {
			gtk_tree_store_insert_after(GTK_TREE_STORE (this->model), iter, parent_iter, sibling);
		}
	} else {
		if (above) {
			gtk_tree_store_append(GTK_TREE_STORE (this->model), iter, parent_iter);
		} else {
			gtk_tree_store_prepend(GTK_TREE_STORE (this->model), iter, parent_iter);
		}
	}

	gtk_tree_store_set(GTK_TREE_STORE (this->model), iter,
			   COLUMN_NAME, name,
			   COLUMN_VISIBLE, true,
			   COLUMN_TYPE, TreeItemType::LAYER,
			   COLUMN_PARENT, parent_layer,
			   COLUMN_ITEM, layer,
			   COLUMN_DATA, data,
			   COLUMN_EDITABLE, true,
			   COLUMN_ICON, layer_type == LayerType::NUM_TYPES ? NULL : this->layer_type_icons[(int) layer_type],
			   COLUMN_TIMESTAMP, (int64_t) timestamp,
			   -1);
}




void TreeView::add_sublayer(GtkTreeIter * parent_iter,
			    GtkTreeIter * iter,
			    const char * name,
			    Layer * parent_layer,
			    sg_uid_t sublayer_uid,
			    SublayerType sublayer_type,
			    GdkPixbuf * icon,
			    bool editable,
			    time_t timestamp)
{
	assert (iter != NULL);

	gtk_tree_store_append(GTK_TREE_STORE(this->model), iter, parent_iter);
	gtk_tree_store_set(GTK_TREE_STORE(this->model), iter,
			   COLUMN_NAME, name,
			   COLUMN_VISIBLE, true,
			   COLUMN_TYPE, TreeItemType::SUBLAYER,
			   COLUMN_PARENT, parent_layer,
			   COLUMN_UID, KUINT_TO_POINTER (sublayer_uid),
			   COLUMN_DATA, sublayer_type,
			   COLUMN_EDITABLE, editable,
			   COLUMN_ICON, icon,
			   COLUMN_TIMESTAMP, (int64_t)timestamp,
			   -1);
}




/* Inspired by the internals of GtkTreeView sorting itself. */
typedef struct _SortTuple
{
	int offset;
	char *name;
	int64_t timestamp;
} SortTuple;

static int sort_tuple_compare(gconstpointer a, gconstpointer b, void * order)
{
	SortTuple *sa = (SortTuple *)a;
	SortTuple *sb = (SortTuple *)b;

	int answer = -1;
	if (KPOINTER_TO_INT(order) < VL_SO_DATE_ASCENDING) {
		/* Alphabetical comparison, default ascending order. */
		answer = g_strcmp0(sa->name, sb->name);
		/* Invert sort order for descending order. */
		if (KPOINTER_TO_INT(order) == VL_SO_ALPHABETICAL_DESCENDING) {
			answer = -answer;
		}
	} else {
		/* Date comparison. */
		bool ans = (sa->timestamp > sb->timestamp);
		if (ans) {
			answer = 1;
		}
		/* Invert sort order for descending order. */
		if (KPOINTER_TO_INT(order) == VL_SO_DATE_DESCENDING) {
			answer = -answer;
		}
	}
	return answer;
}




/**
 * Note: I don't believe we can sensibility use built in model sort gtk_tree_model_sort_new_with_model() on the name,
 * since that would also sort the layers - but that needs to be user controlled for ordering, such as which maps get drawn on top.
 *
 * vik_treeview_sort_children:
 * @vt:     The treeview to operate on
 * @parent: The level within the treeview to sort
 * @order:  How the items should be sorted
 *
 * Use the gtk_tree_store_reorder method as it very quick.
 *
 * This ordering can be performed on demand and works for any parent iterator (i.e. both sublayer and layer levels).
 *
 * It should be called whenever an individual sublayer item is added or renamed (or after a group of sublayer items have been added).
 *
 * Previously with insertion sort on every sublayer addition: adding 10,000 items would take over 30 seconds!
 * Now sorting after simply adding all tracks takes 1 second.
 * For a KML file with over 10,000 tracks (3Mb zipped) - See 'UK Hampshire Rights of Way'
 * http://www3.hants.gov.uk/row/row-maps.htm
 */
void TreeView::sort_children(GtkTreeIter * parent, vik_layer_sort_order_t order)
{
	if (order == VL_SO_NONE) {
		/* Nothing to do. */
		return;
	}

	GtkTreeModel * model = this->model;
	GtkTreeIter child;
	if (!gtk_tree_model_iter_children(model, &child, parent)) {
		return;
	}

	unsigned int length = gtk_tree_model_iter_n_children(model, parent);

	/* Create an array to store the position offsets. */
	SortTuple *sort_array;
	sort_array = (SortTuple *) malloc(length * sizeof (SortTuple));

	unsigned int ii = 0;
	do {
		sort_array[ii].offset = ii;
		gtk_tree_model_get(model, &child, COLUMN_NAME, &(sort_array[ii].name), -1);
		gtk_tree_model_get(model, &child, COLUMN_TIMESTAMP, &(sort_array[ii].timestamp), -1);
		ii++;
	} while (gtk_tree_model_iter_next(model, &child));

	/* Sort list... */
	g_qsort_with_data(sort_array,
			  length,
			  sizeof (SortTuple),
			  sort_tuple_compare,
			  KINT_TO_POINTER(order));

	/* As the sorted list contains the reordered position offsets, extract this and then apply to the treeview. */
	int * positions = (int *) malloc(sizeof(int) * length);
	for (ii = 0; ii < length; ii++) {
		positions[ii] = sort_array[ii].offset;
		free(sort_array[ii].name);
	}
	free(sort_array);

	/* This is extremely fast compared to the old alphabetical insertion. */
	gtk_tree_store_reorder(GTK_TREE_STORE(model), parent, positions);
	free(positions);
}




static void vik_treeview_finalize(GObject *gob)
{
}




static int vik_treeview_drag_data_received(GtkTreeDragDest *drag_dest, GtkTreePath *dest, GtkSelectionData *selection_data)
{
	GtkTreeModel *tree_model;
	GtkTreeModel *src_model = NULL;
	GtkTreePath *src_path = NULL, *dest_cp = NULL;
	bool retval = false;
	GtkTreeIter src_iter, root_iter, dest_parent;
	Layer * layer = NULL;

	if (!GTK_IS_TREE_STORE (drag_dest)) {
		return false;
	}

	tree_model = GTK_TREE_MODEL(drag_dest);

	if (gtk_tree_get_row_drag_data(selection_data, &src_model, &src_path) && src_model == tree_model) {
		/*
		 * Copy src_path to dest.  There are two subcases here, depending on what
		 * is being dragged.
		 *
		 * 1. src_path is a layer. In this case, interpret the drop
		 *    as a request to move the layer to a different aggregate layer.
		 *    If the destination is not an aggregate layer, use the first
		 *    ancestor that is.
		 *
		 * 2. src_path is a sublayer.  In this case, find ancestors of
		 *    both source and destination nodes who are full layers,
		 *    and call the move method of that layer type.
		 *
		 */
		if (!gtk_tree_model_get_iter(src_model, &src_iter, src_path)) {
			goto out;
		}
		if (!gtk_tree_path_compare(src_path, dest)) {
			goto out;
		}

		dest_cp = gtk_tree_path_copy(dest);

		gtk_tree_model_get_iter_first(tree_model, &root_iter);
		TREEVIEW_GET(tree_model, &root_iter, COLUMN_ITEM, &layer);

		if (gtk_tree_path_get_depth(dest_cp) > 1) { /* Can't be sibling of top layer. */

			/* Find the first ancestor that is a full layer, and store in dest_parent. */
			do {
				gtk_tree_path_up(dest_cp);
				gtk_tree_model_get_iter(src_model, &dest_parent, dest_cp);
			} while (gtk_tree_path_get_depth(dest_cp) > 1
				 && layer->tree_view->get_item_type(&dest_parent) != TreeItemType::LAYER);


			Layer * layer_source  = layer->tree_view->get_parent_layer(&src_iter);
			assert (layer_source);
			Layer * layer_dest = layer->tree_view->get_layer(&dest_parent);

			/* TODO: might want to allow different types, and let the clients handle how they want. */
			layer_dest->drag_drop_request(layer_source, &src_iter, dest);
		}
	}

 out:
	if (dest_cp) {
		gtk_tree_path_free(dest_cp);
	}

	if (src_path) {
		gtk_tree_path_free(src_path);
	}

	return retval;
}




/*
 * This may not be necessary.
 */
static int vik_treeview_drag_data_delete(GtkTreeDragSource *drag_source, GtkTreePath *path)
{
	char *s_dest = gtk_tree_path_to_string(path);
	fprintf(stdout, _("delete data from %s\n"), s_dest);
	free(s_dest);
	return false;
}




TreeView::TreeView()
{
	this->vt = (VikTreeview *) g_object_new(VIK_TREEVIEW_TYPE, NULL);

	memset(this->layer_type_icons, 0, sizeof (this->layer_type_icons));

	this->was_a_toggle = false;
	this->editing = false;

	/* ATM The dates are stored on initial creation and updated when items are deleted.
	   This should be good enough for most purposes, although it may get inaccurate if items are edited in a particular manner.
	   NB implicit conversion of time_t to int64_t. */
	this->model = GTK_TREE_MODEL(gtk_tree_store_new(NUM_COLUMNS,
							G_TYPE_STRING,  /* Name. */
							G_TYPE_BOOLEAN, /* Visibility. */
							GDK_TYPE_PIXBUF,/* The Icon. */
							G_TYPE_INT,     /* Layer Type. */
							G_TYPE_POINTER, /* pointer to TV parent. */
							G_TYPE_POINTER, /* pointer to the layer or sublayer. */
							G_TYPE_INT,     /* type of the sublayer. */
							G_TYPE_POINTER, /* sg_uid_t *. */
							G_TYPE_BOOLEAN, /* Editable. */
							G_TYPE_INT64)); /* Timestamp. */

	/* Create tree view. */
	gtk_tree_selection_set_select_function(gtk_tree_view_get_selection(GTK_TREE_VIEW(this->vt)), vik_treeview_selection_filter, this, NULL);

	gtk_tree_view_set_model(GTK_TREE_VIEW(this->vt), this->model);
	this->add_columns();

	/* Can not specify 'auto' sort order with a 'GtkTreeSortable' on the name since we want to control the ordering of layers.
	   Thus need to create special sort to operate on a subsection of treeview (i.e. from a specific child either a layer or sublayer).
	   See vik_treeview_sort_children(). */

	g_object_unref(this->model);

	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(this->vt), true);
	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW (this->vt)),
				     GTK_SELECTION_SINGLE);

	/* Override treestore's dnd methods only; this is easier than deriving from GtkTreeStore.
	 * The downside is that all treestores will have this behavior, so this needs to be
	 * changed if we add more treeviews in the future.  //Alex
	 */
	if (1) {
		GtkTreeDragSourceIface *isrc;
		GtkTreeDragDestIface *idest;

		isrc = (GtkTreeDragSourceIface *) g_type_interface_peek(g_type_class_peek(G_OBJECT_TYPE((GtkTreeDragSourceIface *)this->model)), GTK_TYPE_TREE_DRAG_SOURCE);
		isrc->drag_data_delete = vik_treeview_drag_data_delete;

		idest = (GtkTreeDragDestIface *) g_type_interface_peek(g_type_class_peek(G_OBJECT_TYPE(this->model)), GTK_TYPE_TREE_DRAG_DEST);
		idest->drag_data_received = vik_treeview_drag_data_received;
	}

	for (LayerType i = LayerType::AGGREGATE; i < LayerType::NUM_TYPES; ++i) {
		this->layer_type_icons[(int) i] = vik_layer_load_icon(i); /* If icon can't be loaded, it will be null and simply not be shown. */
	}

	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(this->vt), true);
	g_signal_connect(gtk_tree_view_get_selection(GTK_TREE_VIEW (this->vt)), "changed",
			 G_CALLBACK(select_cb), this);

}




TreeView::~TreeView()
{
	for (LayerType i = LayerType::AGGREGATE; i < LayerType::NUM_TYPES; ++i) {
		if (this->layer_type_icons[(int) i] != NULL) {
			g_object_unref(G_OBJECT(this->layer_type_icons[(int) i]));
		}
	}

	G_OBJECT_CLASS(parent_class)->finalize((GObject *) this->vt);
}
