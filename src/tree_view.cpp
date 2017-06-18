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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstring>
#include <cstdlib>
#include <cassert>

#include <QVariant>

#include "layer.h"
#include "window.h"
#include "tree_view.h"
#include "tree_view_internal.h"
#include "layers_panel.h"
#include "globals.h"
#include "uibuilder.h"
#include "layer_aggregate.h"
#include "layer_coord.h"




using namespace SlavGPS;




static int vik_treeview_drag_data_received(GtkTreeDragDest *drag_dest, GtkTreePath *dest, GtkSelectionData *selection_data);
static int vik_treeview_drag_data_delete(GtkTreeDragSource *drag_source, GtkTreePath *path);




static void vik_treeview_edited_cb(GtkCellRendererText *cell, char *path_str, const char *new_name, TreeView * tree_view)
{
	tree_view->editing = false;

	/* Get type and data. */
	TreeIndex * index = tree_view->get_index_from_path_str(path_str);

#ifdef K
	g_signal_emit(G_OBJECT(tree_view), treeview_signals[VT_ITEM_EDITED_SIGNAL], 0, index, new_name);
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
	/* Get type and data. */
	TreeIndex * index_toggle = tree_view->get_index_from_path_str(path_str);

#ifdef K
	GtkTreePath * tp_toggle = gtk_tree_model_get_path(tree_view->model, index_toggle);

	GtkTreeIter iter_selected;
	if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(tree_view), NULL, &iter_selected)) {
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

	g_signal_emit(G_OBJECT (tree_view), treeview_signals[VT_ITEM_TOGGLED_SIGNAL], 0, index_toggle);
#endif
}




QString TreeView::get_name(TreeIndex const & index)
{
	QStandardItem * parent_item = this->model->itemFromIndex(index.parent());
	if (!parent_item) {
		/* "index" points at the "Top Layer" layer. */
		qDebug() << "II: Tree View: querying Top Layer for item" << index.row() << index.column();
		parent_item = this->model->invisibleRootItem();
	}
	QStandardItem * ch = parent_item->child(index.row(), (int) LayersTreeColumn::NAME);

	return ch->text();
}




bool TreeView::is_visible(TreeIndex const & index)
{
	QStandardItem * parent_item = this->model->itemFromIndex(index.parent());
	if (!parent_item) {
		/* "index" points at the "Top Layer" layer. */
		qDebug() << "II: Tree View: querying Top Layer for item" << index.row() << index.column();
		parent_item = this->model->invisibleRootItem();
	}
	QStandardItem * ch = parent_item->child(index.row(), (int) LayersTreeColumn::VISIBLE);

	QVariant variant = ch->data();
	return variant.toBool();;
}




TreeItemType TreeView::get_item_type(TreeIndex const & index)
{
	QStandardItem * parent_item = this->model->itemFromIndex(index.parent());
	if (!parent_item) {
		/* "index" points at the "Top Layer" layer. */
		qDebug() << "II: Tree View: querying Top Layer for item" << index.row() << index.column();
		parent_item = this->model->invisibleRootItem();
	}
	QStandardItem * ch = parent_item->child(index.row(), (int) LayersTreeColumn::TREE_ITEM_TYPE);

	QVariant variant = ch->data(RoleLayerData);
	return (TreeItemType) variant.toInt();
}




Layer * TreeView::get_parent_layer(TreeIndex const & index)
{
	QStandardItem * parent_item = this->model->itemFromIndex(index.parent());
	if (!parent_item) {
		/* "index" points at the "Top Layer" layer. */
		qDebug() << "II: Tree View: querying Top Layer for item" << index.row() << index.column();
		parent_item = this->model->invisibleRootItem();
	}
	QStandardItem * ch = parent_item->child(index.row(), (int) LayersTreeColumn::PARENT_LAYER);

	QVariant variant = ch->data(RoleLayerData);
	// http://www.qtforum.org/article/34069/store-user-data-void-with-qstandarditem-in-qstandarditemmodel.html
	return variant.value<Layer *>();
}




Layer * TreeView::get_layer(TreeIndex const & index)
{
	QStandardItem * parent_item = this->model->itemFromIndex(index.parent());
	if (!parent_item) {
		/* "index" points at the "Top Layer" layer. */
		qDebug() << "II: Tree View: querying Top Layer for item" << index.row() << index.column();
		parent_item = this->model->invisibleRootItem();
	}
	QStandardItem * ch = parent_item->child(index.row(), (int) LayersTreeColumn::TREE_ITEM);

	QVariant variant = ch->data(RoleLayerData);
	// http://www.qtforum.org/article/34069/store-user-data-void-with-qstandarditem-in-qstandarditemmodel.html
	return variant.value<Layer *>();
}




Sublayer * TreeView::get_sublayer(TreeIndex const & index)
{
	QStandardItem * parent_item = this->model->itemFromIndex(index.parent());
	if (!parent_item) {
		/* "index" points at the "Top Layer" layer. */
		qDebug() << "II: Tree View: querying Top Layer for item" << index.row() << index.column();
		parent_item = this->model->invisibleRootItem();
	}
	QStandardItem * ch = parent_item->child(index.row(), (int) LayersTreeColumn::TREE_ITEM);

	QVariant variant = ch->data(RoleLayerData);
	// http://www.qtforum.org/article/34069/store-user-data-void-with-qstandarditem-in-qstandarditemmodel.html
	return variant.value<Sublayer *>();
}




void TreeView::set_timestamp(TreeIndex const & index, time_t timestamp)
{
	QStandardItem * parent_item = this->model->itemFromIndex(index.parent());
	if (!parent_item) {
		/* "index" points at the "Top Layer" layer. */
		qDebug() << "II: Tree View: querying Top Layer for item" << index.row() << index.column();
		parent_item = this->model->invisibleRootItem();
	}
	QStandardItem * ch = parent_item->child(index.row(), (int) LayersTreeColumn::TIMESTAMP);

	QVariant variant = QVariant::fromValue((qlonglong) timestamp);
	this->model->setData(ch->index(), variant, RoleLayerData);
}




TreeIndex * TreeView::get_index_from_path_str(char const * path_str)
{
	TreeIndex * index = NULL;
#ifdef K
	return gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL (this->model), iter, path_str);
#endif
	return index;
}




/**
 * Get visibility of an item considering visibility of all parents
 * i.e. if any parent is off then this item will also be considered
 * off (even though itself may be marked as on).
 */
bool TreeView::is_visible_in_tree(TreeIndex const & index)
{
	bool visible = this->is_visible(index);

	if (!visible) {
		return visible;
	}

	TreeIndex child = index;

	while (child.parent().isValid()) {
		TreeIndex parent_item = child.parent();
		/* Visibility of this parent. */
		visible = this->is_visible(parent_item);
		/* If not visible, no need to check further ancestors. */
		if (!visible) {
			break;
		}
		child = parent_item;
	}
	return visible;
}



#ifdef K
void TreeView::add_columns()
{
	QObject::connect(renderer, SIGNAL("edited"), this, SLOT (vik_treeview_edited_cb));
	QObject::connect(renderer, SIGNAL("editing-started"), this, SLOT (vik_treeview_edit_start_cb));
	QObject::connect(renderer, SIGNAL("editing-canceled"), this, SLOT (vik_treeview_edit_stop_cb));
	QObject::connect(renderer, SIGNAL("toggled"), this, SLOT (vik_treeview_toggled_cb));
}
#endif




void TreeView::select_cb(void) /* Slot. */
{
	TreeIndex const & index = this->get_selected_item();
	if (!index.isValid()) {
		return;
	}

	TreeIndex const layer_index = this->go_up_to_layer(index);
	if (!layer_index.isValid()) {
		return;
	}

	Layer * layer = this->get_layer(layer_index);
	Window * main_window = this->layers_panel->get_window();
	TreeItemType tree_item_type = this->get_item_type(index);

	Sublayer * sublayer = NULL;
	if (tree_item_type == TreeItemType::SUBLAYER && layer->type == LayerType::TRW) {
		sublayer = this->get_sublayer(index);
	}

	main_window->selected_layer(layer);

	/* Apply settings now we have the all details. */
	if (layer->layer_selected(tree_item_type, sublayer)) {

		/* Redraw required. */
		main_window->get_layers_panel()->emit_update_cb();
	}
}




/*
  Go up the tree to find a Layer.

  If @param index already refers to a layer, the function doesn't
  really go up, it returns the same index.

  If you want to skip item pointed to by @param index and start from
  its parent, you have to calculate parent by yourself before passing
  an index to this function.
*/
TreeIndex const TreeView::go_up_to_layer(TreeIndex const & index)
{
        TreeIndex this_index = index;
	TreeIndex parent_index;

	while (TreeItemType::LAYER != this->get_item_type(this_index)) {
		parent_index = this_index.parent();
		if (!parent_index.isValid()) {
			return parent_index; /* Returning copy of invalid index. */
		}
		this_index = QPersistentModelIndex(parent_index);
	}

	return this_index; /* Even if while() loop executes zero times, this_index is still a valid index. */
}




/*
  Go up the tree to find a Layer of given type.

  If @param index already refers to layer of given type, the function
  doesn't really go up, it returns the same index.

  If you want to skip item pointed to by @param index and start from
  its parent, you have to calculate parent by yourself before passing
  an index to this function.
*/
TreeIndex const TreeView::go_up_to_layer(TreeIndex const & index, LayerType layer_type)
{
        TreeIndex this_index = index;
	TreeIndex parent_index;

	while (1) {
		if (!this_index.isValid()) {
			return this_index; /* Returning copy of invalid index. */
		}

		if (TreeItemType::LAYER == this->get_item_type(this_index)
		    && this->get_layer(this_index)->type == layer_type) {

			return this_index; /* Returning index of matching layer. */
		}

		/* Go one step up to parent. */
		parent_index = this_index.parent();

		/* Parent also may be invalid. */
		if (!parent_index.isValid()) {
			return parent_index; /* Returning copy of invalid index. */
		}

		this_index = QPersistentModelIndex(parent_index);
	}
}




static int vik_treeview_selection_filter(GtkTreeSelection *selection, QStandardItemModel * model, GtkTreePath *path, int path_currently_selected, void * data)
{
	TreeView * tree_view = (TreeView *) data;

	if (tree_view->was_a_toggle) {
		tree_view->was_a_toggle = false;
		return false;
	}

	return true;
}




bool TreeView::move(TreeIndex const & index, bool up)
{
	TreeItemType t = this->get_item_type(index);
	if (t != TreeItemType::LAYER) {
		return false;
	}

#ifdef K
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
#endif
	/* Now, the easy part. actually switching them, not the GUI. */
	/* If item is map... */

	return true;
}




TreeIndex * TreeView::get_index_at_pos(int pos_x, int pos_y)
{
	TreeIndex * index = NULL;
#ifdef K
	GtkTreePath * path;
	(void) gtk_tree_view_get_path_at_pos(this, pos_x, pos_y, &path, NULL, NULL, NULL);
	if (!path) {
		return NULL;
	}

	gtk_tree_model_get_iter(GTK_TREE_MODEL (this->model), iter, path);
	gtk_tree_path_free(path);
#endif
	return index;
}




void TreeView::select_and_expose(TreeIndex const & index)
{
	this->setCurrentIndex(index);
}




TreeIndex const & TreeView::get_selected_item(void)
{
	static TreeIndex invalid;
	TreeIndex selected = QPersistentModelIndex(this->currentIndex());
	if (!selected.isValid()) {
		return invalid;
	}

	TreeItemType tree_item_type = this->get_item_type(selected);
	if (tree_item_type == TreeItemType::LAYER) {
		qDebug() << "II: Tree View: get selected item: layer";
		return this->get_layer(selected)->index;
	} else if (tree_item_type == TreeItemType::SUBLAYER) {
		qDebug() << "II: Tree View: get selected item: sublayer";
		return this->get_sublayer(selected)->index;
	} else {
		qDebug() << "EE: Tree View: get selected item: unknown tree item type" << (int) tree_item_type;
		return invalid;
	}
}




bool TreeView::get_editing()
{
	/* Don't know how to get cell for the selected item. */
	//return KPOINTER_TO_INT(g_object_get_data(G_OBJECT(cell), "editing"));

	/* Instead maintain our own value applying to the whole tree. */
	return this->editing;
}




void TreeView::erase(TreeIndex const & index)
{
	this->model->removeRow(index.row(), index.parent());
}




void TreeView::set_icon(TreeIndex const & index, QIcon const * icon)
{
	if (!index.isValid()) {
		qDebug() << "EE: TreeView: invalid index in" << __FUNCTION__;
		return;
	}

	/* index may be pointing to first column. We want to update LayersTreeColumn::ICON column. */

	QStandardItem * parent_item = this->model->itemFromIndex(index.parent());
	if (!parent_item) {
		/* "index" points at the "Top Layer" layer. */
		qDebug() << "II: Tree View: querying Top Layer for item" << index.row() << index.column();
		parent_item = this->model->invisibleRootItem();
	}
	QStandardItem * ch = parent_item->child(index.row(), (int) LayersTreeColumn::ICON);
	ch->setIcon(*icon);
}




void TreeView::set_name(TreeIndex const & index, QString const & name)
{
	if (!index.isValid()) {
		qDebug() << "EE: TreeView: invalid index in" << __FUNCTION__;
		return;
	}
	this->model->itemFromIndex(index)->setText(name);
}




void TreeView::set_visibility(TreeIndex const & index, bool visible)
{
	if (!!index.isValid()) {
		qDebug() << "EE: Tree View: invalid index in" << __FUNCTION__;
		return;
	}
	/* kamilFIXME: this does not take into account third state. */
	QModelIndex visible_index = index.sibling(index.row(), (int) LayersTreeColumn::VISIBLE);
	this->model->itemFromIndex(visible_index)->setCheckState(visible ? Qt::Checked : Qt::Unchecked);
}




void TreeView::toggle_visibility(TreeIndex const & index)
{
	if (!index.isValid()) {
		qDebug() << "EE: Tree View: invalid index in" << __FUNCTION__;
		return;
	}

	/* kamilFIXME: this does not take into account third state. */
	QModelIndex visible_index = index.sibling(index.row(), (int) LayersTreeColumn::VISIBLE);
	QStandardItem * item = this->model->itemFromIndex(visible_index);
	bool visible = item->checkState() == Qt::Checked;
	item->setCheckState(!visible ? Qt::Checked : Qt::Unchecked);
}




void TreeView::expand(TreeIndex const & index)
{
	if (!index.isValid()) {
		qDebug() << "EE: Tree View: invalid index in" << __FUNCTION__;
		return;
	}

	QStandardItem * item = this->model->itemFromIndex(index);
	this->setExpanded(item->index(), true);
}




void TreeView::select(TreeIndex const & index)
{
	if (!index.isValid()) {
		qDebug() << "EE: Tree View: invalid index in" << __FUNCTION__;
		return;
	}

	this->setCurrentIndex(index);
}




void TreeView::unselect(TreeIndex const & index)
{
#ifdef K
	gtk_tree_selection_unselect_iter(gtk_tree_view_get_selection(this), iter);
#endif
}




/*
  TODO: improve handling of 'editable' argument.
  Non-editable items have e.g limited number of fields in context menu.
*/
TreeIndex const & TreeView::add_sublayer(Sublayer * sublayer, Layer * parent_layer, TreeIndex const & parent_index, char const * name, QIcon * icon, bool editable, time_t timestamp)
{
	// http://www.qtforum.org/article/34069/store-user-data-void-with-qstandarditem-in-qstandarditemmodel.html

	QList<QStandardItem *> items;
	QStandardItem * item = NULL;
	QStandardItem * first_item = NULL;
	QVariant variant;

	QString tooltip = parent_layer->sublayer_tooltip(sublayer);


	/* LayersTreeColumn::NAME */
	item = new QStandardItem(QString(name));
	item->setToolTip(tooltip);
	item->setEditable(editable);
	first_item = item;
	items << item;

	/* LayersTreeColumn::VISIBLE */
	item = new QStandardItem();
	item->setToolTip(tooltip);
	item->setCheckable(true);
	item->setCheckState(parent_layer->visible ? Qt::Checked : Qt::Unchecked);
	items << item;

	/* LayersTreeColumn::ICON */
	item = new QStandardItem();
	item->setToolTip(tooltip);
	item->setIcon(*icon);
	item->setEditable(false);
	items << item;

	/* LayersTreeColumn::TREE_ITEM_TYPE */
	item = new QStandardItem();
	variant = QVariant((int) TreeItemType::SUBLAYER);
	item->setData(variant, RoleLayerData);
	items << item;

	/* LayersTreeColumn::PARENT_LAYER */
	item = new QStandardItem();
	variant = QVariant::fromValue(parent_layer);
	item->setData(variant, RoleLayerData);
	items << item;

	/* LayersTreeColumn::TREE_ITEM */
	item = new QStandardItem();
	variant = QVariant::fromValue(sublayer);
	item->setData(variant, RoleLayerData);
	items << item;

	/* LayersTreeColumn::EDITABLE */
	item = new QStandardItem();
	variant = QVariant::fromValue(editable);
	item->setData(variant, RoleLayerData);
	items << item;

	/* LayersTreeColumn::TIMESTAMP */
#ifdef K
	item = new QStandardItem((qlonglong) timestamp);
#else
	timestamp = 0;
	item = new QStandardItem((qlonglong) timestamp);
#endif
	items << item;


	if (parent_index.isValid()) {
		this->model->itemFromIndex(parent_index)->appendRow(items);
	} else {
		/* TODO: this shouldn't happen, we can't add sublayers right on top. */
		qDebug() << "EE: Tree View: adding sublayer on top level";
		this->model->invisibleRootItem()->appendRow(items);
	}
	//connect(this->model, SIGNAL(itemChanged(QStandardItem*)), layer, SLOT(visibility_toggled_cb(QStandardItem *)));

	sublayer->index = QPersistentModelIndex(first_item->index());

	return sublayer->index;
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
#ifdef K
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
void TreeView::sort_children(TreeIndex const & parent_index, vik_layer_sort_order_t order)
{
	if (order == VL_SO_NONE) {
		/* Nothing to do. */
		return;
	}

	QStandardItemModel * model = this->model;
#ifdef K
	GtkTreeIter child;
	if (!gtk_tree_model_iter_children(model, &child, parent_index)) {
		return;
	}

	unsigned int length = gtk_tree_model_iter_n_children(model, parent_index);

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
	gtk_tree_store_reorder(GTK_TREE_STORE(model), parent_index, positions);
	free(positions);
#endif
}




static int vik_treeview_drag_data_received(GtkTreeDragDest *drag_dest, GtkTreePath *dest, GtkSelectionData *selection_data)
{
#ifdef K
	QStandardItemModel *tree_model;
	QStandardItemModel *src_model = NULL;
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
				 && layer->tree_view->get_item_type(dest_parent) != TreeItemType::LAYER);


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
#ifdef K
	char *s_dest = gtk_tree_path_to_string(path);
	fprintf(stdout, _("delete data from %s\n"), s_dest);
	free(s_dest);
#endif
	return false;
}




TreeIndex const & TreeView::add_layer(Layer * layer, Layer * parent_layer, TreeIndex const & parent_index, bool above, time_t timestamp)
{
	// http://www.qtforum.org/article/34069/store-user-data-void-with-qstandarditem-in-qstandarditemmodel.html

	QString tooltip = layer->tooltip();

	QList<QStandardItem *> items;
	QStandardItem * item = NULL;
	QStandardItem * first_item = NULL;
	QVariant layer_variant = QVariant::fromValue(layer);
	QVariant variant;


	/* LayersTreeColumn::NAME */
	item = new QStandardItem(QString(layer->name));
	item->setToolTip(tooltip);
	first_item = item;

	items << item;

	/* LayersTreeColumn::VISIBLE */
	item = new QStandardItem();
	item->setToolTip(tooltip);
	item->setCheckable(true);
	item->setData(layer_variant, RoleLayerData); /* I'm assigning layer to "visible" so that I don't have to look up ::ITEM column to find a layer. */
	item->setCheckState(layer->visible ? Qt::Checked : Qt::Unchecked);
	items << item;

	/* LayersTreeColumn::ICON */ /* Old code: layer_type == LayerType::NUM_TYPES ? 0 : this->layer_type_icons[(int) layer_type], */
	item = new QStandardItem(QString(layer->debug_string));
	item->setToolTip(tooltip);
	item->setIcon(layer->get_interface()->action_icon);
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

	/* LayersTreeColumn::TREE_ITEM */
	item = new QStandardItem();
	variant = QVariant::fromValue(layer);
	item->setData(variant, RoleLayerData);
	items << item;

	/* LayersTreeColumn::EDITABLE */
	item = new QStandardItem(parent_layer == NULL ? false : true);
	items << item;

	/* LayersTreeColumn::TIMESTAMP */
#ifdef K
	item = new QStandardItem((qlonglong) timestamp);
#else
	timestamp = 0;
	item = new QStandardItem((qlonglong) timestamp);
#endif
	items << item;

#ifdef K
	/* TODO */
	if (above) {
		gtk_tree_store_prepend(GTK_TREE_STORE (this->model), iter, parent_iter);
	} else {
		gtk_tree_store_append(GTK_TREE_STORE (this->model), iter, parent_iter);
	}
#endif

	if (parent_index.isValid()) {
		this->model->itemFromIndex(parent_index)->appendRow(items);
	} else {
		this->model->invisibleRootItem()->appendRow(items);
	}
	connect(this->model, SIGNAL(itemChanged(QStandardItem*)), layer, SLOT(visibility_toggled_cb(QStandardItem *)));

	layer->index = QPersistentModelIndex(first_item->index());

	return layer->index;
}



TreeIndex const & TreeView::insert_layer(Layer * layer, Layer * parent_layer, TreeIndex const & parent_index, bool above, time_t timestamp, TreeIndex const & sibling_index)
{
	/* kamilTODO: handle "sibling" */
#ifdef K
	if (sibling_index.isValid()) {
		if (above) {
			gtk_tree_store_insert_before(GTK_TREE_STORE (this->model), iter, parent_iter, sibling_index);
		} else {
			gtk_tree_store_insert_after(GTK_TREE_STORE (this->model), iter, parent_iter, sibling_index);
		}
	} else
#endif
		{
		return this->add_layer(layer, parent_layer, parent_index, above, timestamp);
	}
}




TreeView::TreeView(LayersPanel * panel) : QTreeView((QWidget *) panel)
{
	this->layers_panel = panel;


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
	this->model->setHorizontalHeaderItem((int) LayersTreeColumn::TREE_ITEM, header_item);

	header_item = new QStandardItem("Editable");
	this->model->setHorizontalHeaderItem((int) LayersTreeColumn::EDITABLE, header_item);

	header_item = new QStandardItem("Time stamp");
	this->model->setHorizontalHeaderItem((int) LayersTreeColumn::TIMESTAMP, header_item);


	this->setModel(this->model);
	this->expandAll();


	this->setSelectionMode(QAbstractItemView::SingleSelection);


	this->header()->setSectionResizeMode((int) LayersTreeColumn::VISIBLE, QHeaderView::ResizeToContents); /* This column holds only a checkbox, so let's limit its width to column label. */
	this->header()->setSectionHidden((int) LayersTreeColumn::TREE_ITEM_TYPE, true);
	this->header()->setSectionHidden((int) LayersTreeColumn::PARENT_LAYER, true);
	this->header()->setSectionHidden((int) LayersTreeColumn::TREE_ITEM, true);
	this->header()->setSectionHidden((int) LayersTreeColumn::EDITABLE, true);
	this->header()->setSectionHidden((int) LayersTreeColumn::TIMESTAMP, true);


	//connect(this, SIGNAL(activated(const QModelIndex &)), this, SLOT(select_cb(void)));
	connect(this, SIGNAL(clicked(const QModelIndex &)), this, SLOT(select_cb(void)));
	//connect(this, SIGNAL(pressed(const QModelIndex &)), this, SLOT(select_cb(void)));
	connect(this->model, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)), this, SLOT(data_changed_cb(const QModelIndex&, const QModelIndex&)));


#if 0
	/* Can not specify 'auto' sort order with a 'GtkTreeSortable' on the name since we want to control the ordering of layers.
	   Thus need to create special sort to operate on a subsection of treeview (i.e. from a specific child either a layer or sublayer).
	   See vik_treeview_sort_children(). */

	for (LayerType i = LayerType::AGGREGATE; i < LayerType::NUM_TYPES; ++i) {
		this->layer_type_icons[(int) i] = vik_layer_load_icon(i); /* If icon can't be loaded, it will be null and simply not be shown. */
	}

	gtk_tree_view_set_reorderable(this, true);
	QObject::connect(gtk_tree_view_get_selection(this), SIGNAL("changed"), this, SLOT (select_cb));
#endif
}




TreeView::~TreeView()
{
	for (LayerType i = LayerType::AGGREGATE; i < LayerType::NUM_TYPES; ++i) {
		delete this->layer_type_icons[(int) i];
	}
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

	TreeIndex * index = new QPersistentModelIndex(top_left);
	if (!index) {
		qDebug() << "EE: Tree View: invalid TreeIndex from valid index" << top_left;
		return;
	}


	if (index->column() == (int) LayersTreeColumn::VISIBLE) {
		if (this->get_layer(*index)) {
			QStandardItem * item = this->model->itemFromIndex(*index);
			qDebug() << "II: Tree View: edited item in column VISIBLE: is checkable?" << item->isCheckable();
			this->get_layer(*index)->visible = (bool) item->checkState();
			qDebug() << "SIGNAL: Tree View layer_needs_redraw(78)";
			emit this->layer_needs_redraw(78);
		} else {
			/* No layer probably means that we want to edit a sublayer. */
		}

	} else if (index->column() == (int) LayersTreeColumn::NAME) {

		/* TODO: reject empty new name. */

		if (this->get_layer(*index)) {
			QStandardItem * item = this->model->itemFromIndex(*index);
			qDebug() << "II: Tree View: edited item in column NAME: new name is" << item->text();
			this->get_layer(*index)->rename((char *) item->text().data());
		} else {
			/* No layer probably means that we want to edit a sublayer. */
		}
	} else {
		qDebug() << "EE: Tree View: edited item in column" << index->column();
	}
}




LayersPanel * TreeView::get_layers_panel(void)
{
	return this->layers_panel;
}




TreeIndex const & TreeItem::get_index(void)
{
	return this->index;
}




void TreeItem::set_index(TreeIndex & i)
{
	this->index = i;
}
