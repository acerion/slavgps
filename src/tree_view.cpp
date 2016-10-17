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

#include <QVariant>

#ifndef SLAVGPS_QT
#include <glib.h>
#endif
#include <glib/gi18n.h>

#include "layer.h"
#include "window.h"


#include "tree_view.h"
#include "layers_panel.h"
#include "globals.h"
#include "uibuilder.h"

#include "layer_aggregate.h"
#include "layer_coord.h"




using namespace SlavGPS;




enum {
	VT_ITEM_EDITED_SIGNAL,
	VT_ITEM_TOGGLED_SIGNAL,
	VT_LAST_SIGNAL
};

static unsigned int treeview_signals[VT_LAST_SIGNAL] = { 0, 0 };



#ifdef SLAVGPS_QT
#define TREEVIEW_GET(model,iter,what,dest)
#else
#define TREEVIEW_GET(model,iter,what,dest) gtk_tree_model_get(GTK_TREE_MODEL(model),(iter),(what),(dest),-1)
#endif




static int vik_treeview_drag_data_received(GtkTreeDragDest *drag_dest, GtkTreePath *dest, GtkSelectionData *selection_data);
static int vik_treeview_drag_data_delete(GtkTreeDragSource *drag_source, GtkTreePath *path);



#ifndef SLAVGPS_QT
static void vik_cclosure_marshal_VOID__POINTER_POINTER(GClosure     * closure,
						       GValue       * return_value,
						       unsigned int   n_param_vals,
						       const GValue * param_values,
						       void         * invocation_hint,
						       void         * marshal_data)
{
	typedef bool (* VikMarshalFunc_VOID__POINTER_POINTER) (void          * data1,
							       gconstpointer   arg_1,
							       gconstpointer   arg_2,
							       void          * data2);

	register VikMarshalFunc_VOID__POINTER_POINTER callback;
	register GCClosure * cc = (GCClosure *) closure;
	register void * data1;
	register void * data2;

	fprintf(stderr, "8888888----------------8888888888-------------88888888888\n");

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
#endif



void SlavGPS::treeview_init(void)
{
#ifndef SLAVGPS_QT
	treeview_signals[VT_ITEM_EDITED_SIGNAL] = g_signal_new("item_edited", G_TYPE_OBJECT, (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION), 0, NULL, NULL,
							       vik_cclosure_marshal_VOID__POINTER_POINTER, G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);

	treeview_signals[VT_ITEM_TOGGLED_SIGNAL] = g_signal_new("item_toggled", G_TYPE_OBJECT, (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION), 0, NULL, NULL,
								g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);
#endif
	return;
}




static void vik_treeview_edited_cb(GtkCellRendererText *cell, char *path_str, const char *new_name, TreeView * tree_view)
{
	tree_view->editing = false;
	GtkTreeIter iter;

	/* Get type and data. */
	tree_view->get_iter_from_path_str(&iter, path_str);

#ifndef SLAVGPS_QT
	g_signal_emit(G_OBJECT(tree_view->tv_), treeview_signals[VT_ITEM_EDITED_SIGNAL], 0, &iter, new_name);
#endif
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

#ifndef SLAVGPS_QT
	GtkTreePath *tp_toggle = gtk_tree_model_get_path(tree_view->model, &iter_toggle);

	if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(tree_view->tv_), NULL, &iter_selected)) {
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

	g_signal_emit(G_OBJECT (tree_view->tv_), treeview_signals[VT_ITEM_TOGGLED_SIGNAL], 0, &iter_toggle);
#endif
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
#ifndef SLAVGPS_QT
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
#endif

	return true;
}


QString TreeView::get_name(QStandardItem * item)
{
	QStandardItem * parent = item->parent();
	if (!parent) {
		/* "item" points at the "Top Layer" layer. */
		fprintf(stderr, "%s:%d querying Top Layer for item row=%d, col=%d\n", __FUNCTION__, __LINE__, item->row(), item->column());
		parent = this->model->invisibleRootItem();
	}
	QStandardItem * ch = parent->child(item->row(), (int) LayersTreeColumn::NAME);

	return ch->text();
}



bool TreeView::is_visible(QStandardItem * item)
{
	QStandardItem * parent = item->parent();
	if (!parent) {
		/* "item" points at the "Top Layer" layer. */
		fprintf(stderr, "%s:%d querying Top Layer for item row=%d, col=%d\n", __FUNCTION__, __LINE__, item->row(), item->column());
		parent = this->model->invisibleRootItem();
	}
	QStandardItem * ch = parent->child(item->row(), (int) LayersTreeColumn::VISIBLE);

	QVariant variant = ch->data();
	return variant.toBool();;
}




TreeItemType TreeView::get_item_type(QStandardItem * item)
{
	QStandardItem * parent = item->parent();
	if (!parent) {
		/* "item" points at the "Top Layer" layer. */
		fprintf(stderr, "%s:%d querying Top Layer for item row=%d, col=%d\n", __FUNCTION__, __LINE__, item->row(), item->column());
		parent = this->model->invisibleRootItem();
	}
	QStandardItem * ch = parent->child(item->row(), (int) LayersTreeColumn::TREE_ITEM_TYPE);

	QVariant variant = ch->data(RoleLayerData);
	return (TreeItemType) variant.toInt();
}




Layer * TreeView::get_parent_layer(QStandardItem * item)
{
	QStandardItem * parent = item->parent();
	if (!parent) {
		/* "item" points at the "Top Layer" layer. */
		fprintf(stderr, "%s:%d querying Top Layer for item row=%d, col=%d\n", __FUNCTION__, __LINE__, item->row(), item->column());
		parent = this->model->invisibleRootItem();
	}
	QStandardItem * ch = parent->child(item->row(), (int) LayersTreeColumn::PARENT_LAYER);

	QVariant variant = ch->data(RoleLayerData);
	// http://www.qtforum.org/article/34069/store-user-data-void-with-qstandarditem-in-qstandarditemmodel.html
	return variant.value<Layer *>();
}




Layer * TreeView::get_layer(QStandardItem * item)
{
	QStandardItem * parent = item->parent();
	if (!parent) {
		/* "item" points at the "Top Layer" layer. */
		fprintf(stderr, "%s:%d querying Top Layer for item row=%d, col=%d\n", __FUNCTION__, __LINE__, item->row(), item->column());
		parent = this->model->invisibleRootItem();
	}
	QStandardItem * ch = parent->child(item->row(), (int) LayersTreeColumn::ITEM);

	QVariant variant = ch->data(RoleLayerData);
	// http://www.qtforum.org/article/34069/store-user-data-void-with-qstandarditem-in-qstandarditemmodel.html
	return variant.value<Layer *>();
}




SublayerType TreeView::get_sublayer_type(QStandardItem * item)
{
	QStandardItem * parent = item->parent();
	if (!parent) {
		/* "item" points at the "Top Layer" layer. */
		fprintf(stderr, "%s:%d querying Top Layer for item row=%d, col=%d\n", __FUNCTION__, __LINE__, item->row(), item->column());
		parent = this->model->invisibleRootItem();
	}
	QStandardItem * ch = parent->child(item->row(), (int) LayersTreeColumn::DATA);

	QVariant variant = ch->data(RoleLayerData);
	return (SublayerType) variant.toInt();
}




void * TreeView::get_sublayer_uid_pointer(QStandardItem * item)
{
	sg_uid_t * uid = NULL;
#if 0
	TREEVIEW_GET (this->model, iter, COLUMN_UID, &uid);
#endif
	return uid;
}




sg_uid_t TreeView::get_sublayer_uid(QStandardItem * item)
{
	QStandardItem * parent = item->parent();
	if (!parent) {
		/* "item" points at the "Top Layer" layer. */
		fprintf(stderr, "%s:%d querying Top Layer for item row=%d, col=%d\n", __FUNCTION__, __LINE__, item->row(), item->column());
		parent = this->model->invisibleRootItem();
	}
	QStandardItem * ch = parent->child(item->row(), (int) LayersTreeColumn::ITEM);

	QVariant variant = ch->data(RoleLayerData);
	// http://www.qtforum.org/article/34069/store-user-data-void-with-qstandarditem-in-qstandarditemmodel.html
	return (sg_uid_t) variant.toULongLong();
}




void TreeView::set_timestamp(QStandardItem * item, time_t timestamp)
{
	QStandardItem * parent = item->parent();
	if (!parent) {
		/* "item" points at the "Top Layer" layer. */
		fprintf(stderr, "%s:%d querying Top Layer for item row=%d, col=%d\n", __FUNCTION__, __LINE__, item->row(), item->column());
		parent = this->model->invisibleRootItem();
	}
	QStandardItem * ch = parent->child(item->row(), (int) LayersTreeColumn::TIMESTAMP);

	QModelIndex index = ch->index();
	QVariant variant = QVariant::fromValue((qlonglong) timestamp);
	this->model->setData(index, variant, RoleLayerData);
}




bool TreeView::get_iter_from_path_str(GtkTreeIter * iter, char const * path_str)
{
#ifndef SLAVGPS_QT
	return gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL (this->model), iter, path_str);
#endif
}




/**
 * Get visibility of an item considering visibility of all parents
 * i.e. if any parent is off then this item will also be considered
 * off (even though itself may be marked as on).
 */
bool TreeView::is_visible_in_tree(QStandardItem * item)
{
	bool visible = this->is_visible(item);

	if (!visible) {
		return visible;
	}

	QStandardItem * parent;
	QStandardItem * child = item;

	while (NULL != (parent = this->get_parent_item(child))) {
		/* Visibility of this parent. */
		visible = this->is_visible(parent);
		/* If not visible, no need to check further ancestors. */
		if (!visible) {
			break;
		}
		child = parent;
	}
	return visible;
}




void TreeView::add_columns()
{
#ifndef SLAVGPS_QT
	int col_offset;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* Layer column. */
	renderer = gtk_cell_renderer_text_new();
	g_signal_connect(renderer, "edited", G_CALLBACK (vik_treeview_edited_cb), this);

	g_signal_connect(renderer, "editing-started", G_CALLBACK (vik_treeview_edit_start_cb), this);
	g_signal_connect(renderer, "editing-canceled", G_CALLBACK (vik_treeview_edit_stop_cb), this);

	g_object_set (G_OBJECT (renderer), "xalign", 0.0, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	col_offset = gtk_tree_view_insert_column_with_attributes(this->tv_,
								 -1, _("Layer Name"),
								 renderer, "text",
								 COLUMN_NAME,
								 "editable", COLUMN_EDITABLE,
								 NULL);

	/* ATM the minimum overall width (and starting default) of the treeview size is determined
	   by the buttons added to the bottom of the layerspanel. */
	column = gtk_tree_view_get_column(this->tv_, col_offset - 1);
	gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN (column),
					GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_expand(GTK_TREE_VIEW_COLUMN (column), true);

	/* Layer type. */
	renderer = gtk_cell_renderer_pixbuf_new();

	g_object_set(G_OBJECT (renderer), "xalign", 0.5, NULL);

	col_offset = gtk_tree_view_insert_column_with_attributes(this->tv_,
								 -1, "",
								 renderer, "pixbuf",
								 COLUMN_ICON,
								 NULL);

	column = gtk_tree_view_get_column(this->tv_, col_offset - 1);
	gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN (column),
					GTK_TREE_VIEW_COLUMN_AUTOSIZE);

	/* Layer visible. */
	renderer = gtk_cell_renderer_toggle_new();
	g_object_set(G_OBJECT (renderer), "xalign", 0.5, NULL);

	g_signal_connect(renderer, "toggled", G_CALLBACK (vik_treeview_toggled_cb), this);

	col_offset = gtk_tree_view_insert_column_with_attributes(this->tv_,
								 -1, "",
								 renderer,
								 "active",
								 COLUMN_VISIBLE,
								 NULL);

	column = gtk_tree_view_get_column(this->tv_, col_offset - 1);
	gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN (column),
					GTK_TREE_VIEW_COLUMN_AUTOSIZE);


	g_object_set(this->tv_, "has-tooltip", true, NULL);
	g_signal_connect(this->tv_, "query-tooltip", G_CALLBACK (vik_treeview_tooltip_cb), this);
#endif
}




void TreeView::select_cb(void) /* Slot. */
{
	QStandardItem * item = this->get_selected_item();
	if (!item) {
		return;
	}

	SublayerType sublayer_type = this->get_sublayer_type(item);
	TreeItemType tree_item_type = this->get_item_type(item);
	sg_uid_t sublayer_uid = this->get_sublayer_uid(item);

	if (tree_item_type == TreeItemType::SUBLAYER) {
		if (!this->go_up_to_layer(item)) {
			return;
		}
	}

	Layer * layer = this->get_layer(item);
	Window * window = layer->get_window();
	window->selected_layer(layer);

	/* Apply settings now we have the all details. */
	if (layer->layer_selected(sublayer_type,
				  sublayer_uid,
				  tree_item_type)) {

		/* Redraw required. */
		window->get_layers_panel()->emit_update_cb();
	}
}




/* Go up the tree to find a Layer. */
QStandardItem * TreeView::go_up_to_layer(QStandardItem * item)
{
	QStandardItem * this_item = item;
	QStandardItem * parent_item = NULL;;

	while (TreeItemType::LAYER != this->get_item_type(this_item)) {
		if (NULL == (parent_item = this->get_parent_item(this_item))) {
			return NULL;
		}
		this_item = parent_item;
	}

	return this_item; /* Don't use parent_iter. If while() loop executes zero times, parent_iter is invalid. */
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




QStandardItem * TreeView::get_parent_item(QStandardItem * item)
{
	return item->parent();
}




bool TreeView::move(GtkTreeIter * iter, bool up)
{
#ifndef SLAVGPS_QT
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
#endif
	return false;
}




bool TreeView::get_iter_at_pos(GtkTreeIter * iter, int x, int y)
{
#ifndef SLAVGPS_QT
	GtkTreePath * path;
	(void) gtk_tree_view_get_path_at_pos(this->tv_, x, y, &path, NULL, NULL, NULL);
	if (!path) {
		return false;
	}

	gtk_tree_model_get_iter(GTK_TREE_MODEL (this->model), iter, path);
	gtk_tree_path_free(path);
#endif
	return true;
}




void TreeView::select_and_expose(GtkTreeIter * iter)
{
#ifndef SLAVGPS_QT
	GtkTreeView * tree_view = this->tv_;
	GtkTreePath * path = gtk_tree_model_get_path(gtk_tree_view_get_model(tree_view), iter);

	gtk_tree_view_expand_to_path(tree_view, path);
	this->select(iter);
	gtk_tree_view_scroll_to_cell(tree_view, path, gtk_tree_view_get_expander_column(tree_view), false, 0.0, 0.0);
	gtk_tree_path_free(path);
#endif
}




QStandardItem * TreeView::get_selected_item(void)
{
	QModelIndex index = this->currentIndex();
	if (!index.isValid()) {
		return NULL;
	}

	QStandardItem * item = this->model->itemFromIndex(index);
	qDebug() << "get_selected_item gets item with column" << item->column();

	return item;
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
#ifndef SLAVGPS_QT
	gtk_tree_store_remove(GTK_TREE_STORE (this->model), iter);
#endif
}




void TreeView::set_icon(GtkTreeIter * iter, const GdkPixbuf * icon)
{
	if (!iter) {
		return;
	}
#ifndef SLAVGPS_QT
	gtk_tree_store_set(GTK_TREE_STORE(this->model), iter, COLUMN_ICON, icon, -1);
#endif
}




void TreeView::set_name(QStandardItem * item, QString const & name)
{
	if (!item) {
		return;
	}
	item->setText(name);
}




void TreeView::set_visibility(QStandardItem * item, bool visible)
{
	if (!item) {
		return;
	}
	/* kamilFIXME: this does not take into account third state. */
	item->setCheckState(visible ? Qt::Checked : Qt::Unchecked);
}




void TreeView::toggle_visibility(QStandardItem * item)
{
	if (!item) {
		return;
	}

	/* kamilFIXME: this does not take into account third state. */
	bool visible = item->checkState() == Qt::Checked;
	item->setCheckState(!visible ? Qt::Checked : Qt::Unchecked);
}




void TreeView::expand(QStandardItem * item)
{
	this->setExpanded(item->index(), true);
}




void TreeView::select(QStandardItem * item)
{
#ifndef SLAVGPS_QT
	gtk_tree_selection_select_iter(gtk_tree_view_get_selection(this->tv_), iter);
#endif
}




void TreeView::unselect(GtkTreeIter *iter)
{
#ifndef SLAVGPS_QT
	gtk_tree_selection_unselect_iter(gtk_tree_view_get_selection(this->tv_), iter);
#endif
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
#ifndef SLAVGPS_QT
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
#endif
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
#ifndef SLAVGPS_QT
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
#endif
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
#ifndef SLAVGPS_QT

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
#endif
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
#ifndef SLAVGPS_QT
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
#endif
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
#ifndef SLAVGPS_QT
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
#endif
}




static int vik_treeview_drag_data_received(GtkTreeDragDest *drag_dest, GtkTreePath *dest, GtkSelectionData *selection_data)
{
#ifndef SLAVGPS_QT
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


			Layer * layer_source  = layer->tree_view->get_parent_layer(&src_item);
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
#endif
}




/*
 * This may not be necessary.
 */
static int vik_treeview_drag_data_delete(GtkTreeDragSource *drag_source, GtkTreePath *path)
{
#ifndef SLAVGPS_QT
	char *s_dest = gtk_tree_path_to_string(path);
	fprintf(stdout, _("delete data from %s\n"), s_dest);
	free(s_dest);
#endif
	return false;
}




QStandardItem * TreeView::add_layer(Layer * layer, Layer * parent_layer, QStandardItem * parent_item, bool above, int data, time_t timestamp)
{
	// http://www.qtforum.org/article/34069/store-user-data-void-with-qstandarditem-in-qstandarditemmodel.html

	QList<QStandardItem *> items;
	QStandardItem * item = NULL;
	QStandardItem * ret = NULL;
	QVariant layer_variant = QVariant::fromValue(layer);
	QVariant variant;


	/* LayersTreeColumn::NAME */
	item = new QStandardItem(QString(layer->name));
	ret = item;
	items << item;

	/* LayersTreeColumn::VISIBLE */
	item = new QStandardItem();
	item->setCheckable(true);
	item->setData(layer_variant, RoleLayerData); /* I'm assigning layer to "visible" so that I don't have to look up ::ITEM column to find a layer. */
	item->setCheckState(layer->visible ? Qt::Checked : Qt::Unchecked);
	items << item;

	/* LayersTreeColumn::ICON */
	item = new QStandardItem(QString(layer->type_string));
	item->setIcon(*layer->get_interface()->icon);
	item->setEditable(false); /* Don't allow editing layer type string. */
	items << item;

	/* LayersTreeColumn::TREE_ITEM_TYPE */
	item = new QStandardItem();
	variant = QVariant((int) TreeItemType::LAYER);
	item->setData(variant, RoleLayerData);
	items << item;

	/* LayersTreeColumn::PARENT_LAYER */
	item = new QStandardItem();
	variant = QVariant::fromValue(parent_layer);
	item->setData(variant, RoleLayerData);
	items << item;

	/* LayersTreeColumn::ITEM */
	item = new QStandardItem();
	variant = QVariant::fromValue(layer);
	item->setData(variant, RoleLayerData);
	items << item;

	/* LayersTreeColumn::DATA */
	item = new QStandardItem();
	variant = QVariant::fromValue(data);
	item->setData(variant, RoleLayerData);
	items << item;

	/* LayersTreeColumn::UID */
	item = new QStandardItem((qulonglong) SG_UID_NONE);
	variant = QVariant::fromValue(data);
	item->setData(variant, RoleLayerData);
	items << item;

	/* LayersTreeColumn::EDITABLE */
	item = new QStandardItem(parent_layer == NULL ? false : true);
	variant = QVariant::fromValue(data);
	item->setData(variant, RoleLayerData);
	items << item;

	/* LayersTreeColumn::TIMESTAMP */
#ifdef K
	item = new QStandardItem((qlonglong) timestamp);
#else
	timestamp = 0;
	item = new QStandardItem((qlonglong) timestamp);
#endif
	variant = QVariant::fromValue(data);
	item->setData(variant, RoleLayerData);
	items << item;

	if (parent_item) {
		parent_item->appendRow(items);
	} else {
		this->model->invisibleRootItem()->appendRow(items);
	}
	connect(this->model, SIGNAL(itemChanged(QStandardItem*)), layer, SLOT(visibility_toggled(QStandardItem *)));

	return ret;
}



QStandardItem * TreeView::insert_layer(Layer * layer, Layer * parent_layer, QStandardItem * parent_item, bool above, int data, time_t timestamp, QStandardItem * sibling)
{
	QStandardItem * item = NULL;
	/* kamilTODO: handle "sibling" */
#ifndef SLAVGPS_QT
	if (sibling) {
		if (above) {
			gtk_tree_store_insert_before(GTK_TREE_STORE (this->model), iter, parent_iter, sibling);
		} else {
			gtk_tree_store_insert_after(GTK_TREE_STORE (this->model), iter, parent_iter, sibling);
		}
	} else
#endif
		{
		return this->add_layer(layer, parent_layer, parent_item, above, data, timestamp);
	}
}




TreeView::TreeView(QWidget * parent) : QTreeView(parent)
{
	this->model = new QStandardItemModel();



	QStandardItem * header_item = NULL;

	header_item = new QStandardItem("Layer Name");
	this->model->setHorizontalHeaderItem((int) LayersTreeColumn::NAME, header_item);

	header_item = new QStandardItem("Visible");
	this->model->setHorizontalHeaderItem((int) LayersTreeColumn::VISIBLE, header_item);

	header_item = new QStandardItem("Type");
	this->model->setHorizontalHeaderItem((int) LayersTreeColumn::ICON, header_item);

	header_item = new QStandardItem("Tree Item Type");
	this->model->setHorizontalHeaderItem((int) LayersTreeColumn::TREE_ITEM_TYPE, header_item);

	header_item = new QStandardItem("Parent Layer");
	this->model->setHorizontalHeaderItem((int) LayersTreeColumn::PARENT_LAYER, header_item);

	header_item = new QStandardItem("Item");
	this->model->setHorizontalHeaderItem((int) LayersTreeColumn::ITEM, header_item);

	header_item = new QStandardItem("Data");
	this->model->setHorizontalHeaderItem((int) LayersTreeColumn::DATA, header_item);

	header_item = new QStandardItem("UID");
	this->model->setHorizontalHeaderItem((int) LayersTreeColumn::UID, header_item);

	header_item = new QStandardItem("Editable");
	this->model->setHorizontalHeaderItem((int) LayersTreeColumn::EDITABLE, header_item);

	header_item = new QStandardItem("Time stamp");
	this->model->setHorizontalHeaderItem((int) LayersTreeColumn::TIMESTAMP, header_item);


	this->setModel(this->model);
	this->expandAll();



	this->header()->setSectionResizeMode((int) LayersTreeColumn::VISIBLE, QHeaderView::ResizeToContents); /* This column holds only a checkbox, so let's limit its width to column label. */
	this->header()->setSectionHidden((int) LayersTreeColumn::TREE_ITEM_TYPE, true);
	this->header()->setSectionHidden((int) LayersTreeColumn::PARENT_LAYER, true);
	this->header()->setSectionHidden((int) LayersTreeColumn::ITEM, true);
	this->header()->setSectionHidden((int) LayersTreeColumn::DATA, true);
	this->header()->setSectionHidden((int) LayersTreeColumn::UID, true);
	this->header()->setSectionHidden((int) LayersTreeColumn::EDITABLE, true);
	this->header()->setSectionHidden((int) LayersTreeColumn::TIMESTAMP, true);


	//connect(this, SIGNAL(activated(const QModelIndex &)), this, SLOT(select_cb(void)));
	connect(this, SIGNAL(clicked(const QModelIndex &)), this, SLOT(select_cb(void)));
	//connect(this, SIGNAL(pressed(const QModelIndex &)), this, SLOT(select_cb(void)));
	connect(this->model, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)), this, SLOT(data_changed_cb(const QModelIndex&, const QModelIndex&)));


#if 0
	memset(this->layer_type_icons, 0, sizeof (this->layer_type_icons));


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
	gtk_tree_selection_set_select_function(gtk_tree_view_get_selection(this->tv_), vik_treeview_selection_filter, this, NULL);

	///gtk_tree_view_set_model(this->tv_, this->model);
	this->add_columns();

	/* Can not specify 'auto' sort order with a 'GtkTreeSortable' on the name since we want to control the ordering of layers.
	   Thus need to create special sort to operate on a subsection of treeview (i.e. from a specific child either a layer or sublayer).
	   See vik_treeview_sort_children(). */

	g_object_unref(this->model);

	gtk_tree_view_set_rules_hint(this->tv_, true);
	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(this->tv_),
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

	gtk_tree_view_set_reorderable(this->tv_, true);
	g_signal_connect(gtk_tree_view_get_selection(this->tv_), "changed",
			 G_CALLBACK(select_cb), this);
#endif
}




TreeView::~TreeView()
{
#ifndef SLAVGPS_QT
	for (LayerType i = LayerType::AGGREGATE; i < LayerType::NUM_TYPES; ++i) {
		if (this->layer_type_icons[(int) i] != NULL) {
			g_object_unref(G_OBJECT(this->layer_type_icons[(int) i]));
		}
	}

	/* kamilTODO: free this pointer. */
	this->tv_ = NULL;
#endif
}




GtkWindow * TreeView::get_toolkit_window(void)
{
#ifndef SLAVGPS_QT
	return GTK_WINDOW (gtk_widget_get_toplevel(GTK_WIDGET (this->tv_)));
#endif
}




GtkWidget * TreeView::get_toolkit_widget(void)
{
#ifndef SLAVGPS_QT
	return GTK_WIDGET(this->tv_);
#endif
}



/**
   Called when data in tree view has been changed

   Should call column-specific handlers.

   Range of changed items is between @param top_left and @param
   bottom_right, but this method only handles @top_left item.
*/
void TreeView::data_changed_cb(const QModelIndex & top_left, const QModelIndex & bottom_right)
{
	if (!top_left.isValid()) {
		return;
	}

	QStandardItem * item = this->model->itemFromIndex(top_left);
	if (!item) {
		qDebug() << "EE: Tree View: invalid item from valid index" << top_left << __FUNCTION__ << __LINE__;
		return;
	}


	if (item->column() == (int) LayersTreeColumn::VISIBLE) {
		qDebug() << "II: Tree View: edited item in column VISIBLE: is checkable?" << item->isCheckable();
		this->get_layer(item)->visible = (bool) item->checkState();
		qDebug() << "SIGNAL: Tree View layer_needs_redraw(78)";
		emit this->layer_needs_redraw(78);

	} else if (item->column() == (int) LayersTreeColumn::NAME) {

		/* TODO: reject empty new name. */

		qDebug() << "II: Tree View: edited item in column NAME: new name is" << item->text();
		this->get_layer(item)->rename((char *) item->text().data());
	} else {
		qDebug() << "EE: Tree View: edited item in column" << item->column();
	}

}
