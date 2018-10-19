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




#include <cstring>
#include <cstdlib>
#include <cassert>




#include <QVariant>
#include <QDebug>
#include <QHeaderView>




#include "layer.h"
#include "window.h"
#include "tree_view.h"
#include "tree_view_internal.h"
#include "layers_panel.h"
#include "ui_builder.h"
#include "dialog.h"
#include "statusbar.h"




/*
  TODO_LATER: improve handling of 'editable' property.
  Non-editable items have e.g limited number of fields in context menu.

  The following properties of @tree_item are used to set properties of entry in tree:
  - TreeItem::editable
  - TreeItem::visible;
  - TreeItem::get_tooltip()
*/




using namespace SlavGPS;




#define SG_MODULE "Tree View"




extern Tree * g_tree;




typedef int GtkTreeDragSource;
typedef int GtkTreeDragDest;
typedef int GtkCellRenderer;
typedef int GtkCellRendererToggle;
typedef int GtkCellRendererText;
typedef int GtkCellEditable;
typedef int GtkSelectionData;
typedef int GtkTreePath;




static int vik_tree_view_drag_data_received(GtkTreeDragDest *drag_dest, GtkTreePath *dest, GtkSelectionData *selection_data);
static int vik_tree_view_drag_data_delete(GtkTreeDragSource *drag_source, GtkTreePath *path);




TreeItem * TreeView::get_tree_item(const TreeIndex & item_index) const
{
	if (item_index.row() == -1 || item_index.column() == -1) {
		qDebug() << SG_PREFIX_W << "Querying for item with -1 row or column";
		return NULL;
	}

	QStandardItem * parent_item = this->tree_model->itemFromIndex(item_index.parent());
	if (!parent_item) {
		/* "item_index" points at the top tree item. */
		qDebug() << SG_PREFIX_I << "Querying Top Tree Item for item" << item_index.row() << item_index.column();
		parent_item = this->tree_model->invisibleRootItem();
	}
	QStandardItem * ch = parent_item->child(item_index.row(), (int) TreeViewColumn::TreeItem);

	QVariant variant = ch->data(RoleLayerData);
	// http://www.qtforum.org/article/34069/store-user-data-void-with-qstandarditem-in-qstandarditemmodel.html
	return variant.value<TreeItem *>();
}




void TreeView::apply_tree_item_timestamp(const TreeItem * tree_item)
{

	return;

	QStandardItem * parent_item = this->tree_model->itemFromIndex(tree_item->index.parent());
	if (!parent_item) {
		/* "tree_item->index" points at the top tree item. */
		qDebug() << SG_PREFIX_I << "Querying Top Tree Item for item" << tree_item->index.row() << tree_item->index.column();
		parent_item = this->tree_model->invisibleRootItem();
	}
	QStandardItem * ch = parent_item->child(tree_item->index.row(), (int) TreeViewColumn::Timestamp);

	qDebug() << SG_PREFIX_I;

	QVariant variant = QVariant::fromValue((qlonglong) tree_item->get_timestamp());
	this->tree_model->setData(ch->index(), variant, RoleLayerData);

	qDebug() << SG_PREFIX_I;
}




void TreeView::apply_tree_item_tooltip(const TreeItem * tree_item)
{
	return;

	QStandardItem * parent_item = this->tree_model->itemFromIndex(tree_item->index.parent());
	if (!parent_item) {
		/* "tree_item->index" points at the top tree item. */
		qDebug() << SG_PREFIX_I << "Querying Top Tree Item for item" << tree_item->index.row() << tree_item->index.column();
		parent_item = this->tree_model->invisibleRootItem();
	}
	QStandardItem * ch = parent_item->child(tree_item->index.row(), (int) TreeViewColumn::Name);
	ch->setToolTip(tree_item->get_tooltip());
}




void TreeView::select_cb(void) /* Slot. */
{
	TreeItem * selected_item = this->get_selected_tree_item();
	if (!selected_item) {
		return;
	}

	Window * main_window = g_tree->tree_get_main_window();

	/* Clear statusbar. */
	main_window->get_statusbar()->set_message(StatusBarField::Info, "");

	qDebug() << SG_PREFIX_I << "Selected item is" << (selected_item->tree_item_type == TreeItemType::Layer ? "layer" : "sublayer");

	/* Either the selected layer itself, or an owner/parent of selected sublayer item. */
	const Layer * layer = selected_item->to_layer();

	/* This should activate toolbox relevant to selected layer's type. */
	main_window->handle_selection_of_layer(layer);

	const bool redraw_required = selected_item->handle_selection_in_tree();
	if (redraw_required) {
		qDebug() << SG_PREFIX_SIGNAL << "Will call 'emit_items_tree_updated_cb()' for" << selected_item->name;
		g_tree->tree_get_items_tree()->emit_items_tree_updated_cb(selected_item->name);
	}
}




bool TreeView::change_tree_item_position(TreeItem * tree_item, bool up)
{
	if (!tree_item || tree_item->tree_item_type != TreeItemType::Layer) {
		return false;
	}

	QModelIndex parent_index = tree_item->index.parent();
	if (!parent_index.isValid()) {
		qDebug() << SG_PREFIX_W << "Parent index is invalid. Function called for top level item?";
		return false;
	}

	QStandardItem * source_parent_item = this->tree_model->itemFromIndex(parent_index);
	QStandardItem * target_parent_item = source_parent_item;

	const int n_rows = source_parent_item->rowCount();

	const int source_row = tree_item->index.row();
	const int target_row = up ? source_row - 1 : source_row + 1;

	if (target_row < 0 || target_row > n_rows - 1) {
		qDebug() << SG_PREFIX_W << "Can't move item" << (up ? "up" : "down") << ": out of range";
		return false;
	}


	/* This is the actual move: cut from old position and paste into new position. */
	QList<QStandardItem *> items = source_parent_item->takeRow(source_row);
	target_parent_item->insertRow(target_row, items);


	tree_item->index = QPersistentModelIndex(items.at(0)->index());

	return true;
}




void TreeView::select_and_expose_tree_item(const TreeItem * tree_item)
{
	this->setCurrentIndex(tree_item->index);
}




TreeItem * TreeView::get_selected_tree_item(void) const
{
	TreeIndex selected = QPersistentModelIndex(this->currentIndex());
	if (!selected.isValid()) {
		qDebug() << SG_PREFIX_W << "No selected tree item";
		return NULL;
	}

	TreeItem * tree_item = this->get_tree_item(selected);
	if (!tree_item) {
		qDebug() << SG_PREFIX_E << "Can't get item for valid index";
	}

	return tree_item;
}




void TreeView::detach_tree_item(TreeItem * tree_item)
{
	this->tree_model->removeRow(tree_item->index.row(), tree_item->index.parent());
	tree_item->tree_view = NULL;
}




void TreeView::detach_children(TreeItem * parent_tree_item)
{
	QStandardItem * parent_item = this->tree_model->itemFromIndex(parent_tree_item->index);
	parent_item->removeRows(0, parent_item->rowCount());
}




void TreeView::apply_tree_item_icon(const TreeItem * tree_item)
{
	if (!tree_item->index.isValid()) {
		qDebug() << SG_PREFIX_E << "Invalid item index";
		return;
	}

	const QIcon & icon = tree_item->icon;

	if (icon.isNull()) {
		/* Not an error. Perhaps there is no resource defined for an icon. */
		return;
	}

	qDebug() << SG_PREFIX_I;
	/* Icon is a property of TreeViewColumn::Name column. */

	QStandardItem * parent_item = this->tree_model->itemFromIndex(tree_item->index.parent());
	if (!parent_item) {
		/* "tree_item->index" points at the top-level item. */
		qDebug() << SG_PREFIX_I << "Querying Top Level Item for item" << tree_item->index.row() << tree_item->index.column();
		parent_item = this->tree_model->invisibleRootItem();
	}
	qDebug() << SG_PREFIX_I;
	QStandardItem * ch = parent_item->child(tree_item->index.row(), (int) TreeViewColumn::Name);
	ch->setIcon(icon);

	qDebug() << SG_PREFIX_I;
}




void TreeView::apply_tree_item_name(const TreeItem * tree_item)
{
	if (!tree_item->index.isValid()) {
		qDebug() << SG_PREFIX_E << "Invalid item index";
		return;
	}
	this->tree_model->itemFromIndex(tree_item->index)->setText(tree_item->name);
}




bool TreeView::get_tree_item_visibility(const TreeItem * tree_item)
{
	const TreeIndex & index = tree_item->index;

	QStandardItem * parent_item = this->tree_model->itemFromIndex(index.parent());
	if (!parent_item) {
		/* "index" points at the top-level item. */
		//qDebug() << SG_PREFIX_I << "Querying Top Level Item for item" << index.row() << index.column();
		if (index.row() == -1 || index.column() == -1) {
			qDebug() << SG_PREFIX_E << "Invalid row or column";
		}
		parent_item = this->tree_model->invisibleRootItem();
	}
	QStandardItem * ch = parent_item->child(index.row(), (int) TreeViewColumn::Visible);

	QVariant variant = ch->data();
	return ch->checkState() != Qt::Unchecked; /* See if Item is either checked (Qt::Checked) or partially checked (Qt::PartiallyChecked). */
}




/**
   Get visibility of an item considering visibility of all parents
   i.e. if any parent is invisible then this item will also be considered
   invisible (even though it itself may be marked as visible).
*/
bool TreeView::get_tree_item_visibility_with_parents(const TreeItem * tree_item)
{
	const TreeIndex & index = tree_item->index;

	int loop_depth = 1;

	TreeIndex this_item_index = tree_item->index;
	const TreeItem * this_tree_item = tree_item;

	do {
#if 0           /* Debug. */
		qDebug() << SG_PREFIX_I << "Checking visibility of" << tree_item->name << "in tree, forever loop depth =" << loop_depth++;
#endif

		if (!this->get_tree_item_visibility(this_tree_item)) {
			/* Simple case: this item is not visible. */
			return false;
		}
		/* This item is visible. What about its parent? */

		TreeIndex parent_item_index = this_tree_item->index.parent();
		if (!parent_item_index.isValid()) {
			/* This item doesn't have valid parent, so it
			   must be a top-level item. The top-level item
			   was visible (we checked this few lines above),
			   so return true. */
			return true;
		}
		TreeItem * parent_tree_item = this->get_tree_item(parent_item_index);


		this_item_index = parent_item_index;
		this_tree_item = parent_tree_item;

	} while (1); /* Forever loop that will finish either on first invisible item it meets, or on visible top-level item. */
}




bool TreeView::apply_tree_item_visibility(const TreeItem * tree_item)
{
	if (!tree_item || !tree_item->index.isValid()) {
		qDebug() << SG_PREFIX_E << "Invalid tree item" << (tree_item ? "bad index" : "NULL pointer");
		return false;
	}

	/* kamilFIXME: this does not take into account third state. */
	QModelIndex visible_index = tree_item->index.sibling(tree_item->index.row(), (int) TreeViewColumn::Visible);
	this->tree_model->itemFromIndex(visible_index)->setCheckState(tree_item->visible ? Qt::Checked : Qt::Unchecked);

	return true;
}




#if 0
void TreeView::toggle_tree_item_visibility(TreeIndex const & item_index)
{
	if (!item_index.isValid()) {
		qDebug() << SG_PREFIX_E << "Invalid item index";
		return;
	}

	/* kamilFIXME: this does not take into account third state. */
	QModelIndex visible_index = item_index.sibling(item_index.row(), (int) TreeViewColumn::Visible);
	QStandardItem * item = this->tree_model->itemFromIndex(visible_index);
	bool visible = item->checkState() == Qt::Checked;
	item->setCheckState(!visible ? Qt::Checked : Qt::Unchecked);
}
#endif




void TreeView::expand_tree_item(const TreeItem * tree_item)
{
	TreeIndex const & index = tree_item->index;

	if (!index.isValid()) {
		qDebug() << SG_PREFIX_E << "Invalid index";
		return;
	}

	QStandardItem * item = this->tree_model->itemFromIndex(index);
	this->setExpanded(item->index(), true);
}




void TreeView::select_tree_item(const TreeItem * tree_item)
{
	TreeIndex const & index = tree_item->index;

	if (!index.isValid()) {
		qDebug() << SG_PREFIX_E << "Invalid index";
		return;
	}

	this->setCurrentIndex(index);
}




void TreeView::deselect_tree_item(const TreeItem * tree_item)
{
	this->selectionModel()->select(tree_item->index, QItemSelectionModel::Deselect);
}




QList<QStandardItem *> TreeView::create_new_row(TreeItem * tree_item, const QString & name)
{
	// http://www.qtforum.org/article/34069/store-user-data-void-with-qstandarditem-in-qstandarditemmodel.html

	QList<QStandardItem *> items;
	QStandardItem * item = NULL;
	QVariant variant;

	const QString tooltip = tree_item->get_tooltip();


	/* TreeViewColumn::Name */
	item = new QStandardItem(name);
	item->setToolTip(tooltip);
	item->setEditable(tree_item->editable);
	if (!tree_item->icon.isNull()) { /* Icon can be set with ::apply_tree_item_icon(). */
		item->setIcon(tree_item->icon);
	}
	//item->moveToThread(QApplication::instance()->thread())
	items << item;

	/* TreeViewColumn::Visible */
	item = new QStandardItem();
	item->setCheckable(true);
	item->setCheckState(tree_item->visible ? Qt::Checked : Qt::Unchecked);
	//item->moveToThread(QApplication::instance()->thread())
	items << item;

	/* TreeViewColumn::TreeItem */
	item = new QStandardItem();
	variant = QVariant::fromValue(tree_item);
	item->setData(variant, RoleLayerData);
	//item->moveToThread(QApplication::instance()->thread())
	items << item;

	/* TreeViewColumn::Editable */
	item = new QStandardItem();
	variant = QVariant::fromValue(tree_item->editable);
	item->setData(variant, RoleLayerData);
	//item->moveToThread(QApplication::instance()->thread())
	items << item;

	/* TreeViewColumn::Timestamp */
	/* Value in this column can be set with ::apply_tree_item_timestamp(). */
	qlonglong timestamp = 0;
	item = new QStandardItem((qlonglong) timestamp);
	//item->moveToThread(QApplication::instance()->thread())
	items << item;


	return items;
}




/**
   \brief Add given @tree_item under given parent tree item

   @param parent_tree_index - parent tree item under which to put new @tree_item
   @param tree_item - tree_item to be added

   @return sg_ret::ok on success
   @return error value on failure
*/
sg_ret TreeView::attach_to_tree(const TreeItem * parent_tree_item, TreeItem * tree_item, TreeView::AttachMode attach_mode, const TreeItem * sibling_tree_item)
{
	if (!parent_tree_item->index.isValid()) {
		/* Parent index must always be valid. The only
		   possibility would be when we would push top level
		   layer, but this has been already done in tree
		   view's constructor. */
		qDebug() << SG_PREFIX_E << "Trying to push tree item with invalid parent item";
		return sg_ret::err;
	}

	int row = -2;
	sg_ret result = sg_ret::err;

	switch (attach_mode) {
	case TreeView::AttachMode::Front:
		row = 0;
		qDebug() << SG_PREFIX_I << "Pushing front tree item named" << tree_item->name << "into row" << row;
		result = this->insert_tree_item_at_row(parent_tree_item, tree_item, row);
		qDebug() << SG_PREFIX_I;
		break;

	case TreeView::AttachMode::Back:
		row = this->tree_model->itemFromIndex(parent_tree_item->index)->rowCount();
		qDebug() << SG_PREFIX_I << "Pushing back tree item named" << tree_item->name << "into row" << row;
		result = this->insert_tree_item_at_row(parent_tree_item, tree_item, row);
		qDebug() << SG_PREFIX_I;
		break;

	case TreeView::AttachMode::Before:
	case TreeView::AttachMode::After:
		if (NULL == sibling_tree_item) {
			qDebug() << SG_PREFIX_E << "Failed to attach tree item" << tree_item->name << "next to sibling: NULL sibling";
			result = sg_ret::err;
			break;
		}

		if (!sibling_tree_item->index.isValid()) {
			qDebug() << SG_PREFIX_E << "Failed to attach tree item" << tree_item->name << "next to sibling: invalid sibling";
			result = sg_ret::err;
			break;
		}

		row = sibling_tree_item->index.row() + (attach_mode == TreeView::AttachMode::Before ? 0 : 1);
		qDebug() << SG_PREFIX_I << "Pushing tree item named" << tree_item->name << "next to sibling named" << sibling_tree_item->name << "into row" << row;
		result = this->insert_tree_item_at_row(parent_tree_item, tree_item, row);
		qDebug() << SG_PREFIX_I;
		break;

	default:
		qDebug() << SG_PREFIX_E << "Unexpected attach mode" << (int) attach_mode;
		result = sg_ret::err;
		break;
	}


	if (sg_ret::ok != result) {
		qDebug() << SG_PREFIX_E << "Failed to attach child" << tree_item->name << "under parent" << parent_tree_item->name << "with mode" << (int) attach_mode << "into row" << row;
		return sg_ret::err;
	}


	this->apply_tree_item_timestamp(tree_item);
	qDebug() << SG_PREFIX_I;
	this->apply_tree_item_icon(tree_item);

	qDebug() << SG_PREFIX_I;

	return sg_ret::ok;
}




/* Inspired by the internals of GtkTreeView sorting itself. */
typedef struct _SortTuple
{
	int offset;
	char *name;
	int64_t timestamp;
} SortTuple;

static int sort_tuple_compare(const void * a, const void * b, void * order)
{
	SortTuple *sa = (SortTuple *)a;
	SortTuple *sb = (SortTuple *)b;

	int answer = -1;
#ifdef K_FIXME_RESTORE
	const int sort_order = (int) (long) order;
	if (sort_order < TreeViewSortOrder::DateAscending) {
		/* Alphabetical comparison, default ascending order. */
		answer = g_strcmp0(sa->name, sb->name);
		/* Invert sort order for descending order. */
		if (sort_order == TreeViewSortOrder::AlphabeticalDescending) {
			answer = -answer;
		}
	} else {
		/* Date comparison. */
		bool ans = (sa->timestamp > sb->timestamp);
		if (ans) {
			answer = 1;
		}
		/* Invert sort order for descending order. */
		if (sort_order == TreeViewSortOrder::DateDescending) {
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
 * @parent: The level within the tree view to sort
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
void TreeView::sort_children(const TreeItem * parent_tree_item, TreeViewSortOrder sort_order)
{
	TreeIndex const & parent_index = parent_tree_item->index;

	if (sort_order == TreeViewSortOrder::None) {
		/* Nothing to do. */
		return;
	}

#ifdef K_FIXME_RESTORE
	GtkTreeIter child;
	if (!gtk_tree_model_iter_children(this->tree_model, &child, parent_index)) {
		return;
	}

	unsigned int length = gtk_tree_model_iter_n_children(this->tree_model, parent_index);

	/* Create an array to store the position offsets. */
	SortTuple *sort_array;
	sort_array = (SortTuple *) malloc(length * sizeof (SortTuple));

	unsigned int ii = 0;
	do {
		sort_array[ii].offset = ii;
		gtk_tree_model_get(this->tree_model, &child, COLUMN_NAME, &(sort_array[ii].name), -1);
		gtk_tree_model_get(this->tree_model, &child, COLUMN_TIMESTAMP, &(sort_array[ii].timestamp), -1);
		ii++;
	} while (gtk_tree_model_iter_next(this->tree_model, &child));

	/* Sort list... */
	g_qsort_with_data(sort_array,
			  length,
			  sizeof (SortTuple),
			  sort_tuple_compare,
			  sort_order);

	/* As the sorted list contains the reordered position offsets, extract this and then apply to the tree view. */
	int * positions = (int *) malloc(sizeof(int) * length);
	for (ii = 0; ii < length; ii++) {
		positions[ii] = sort_array[ii].offset;
		free(sort_array[ii].name);
	}
	free(sort_array);

	/* This is extremely fast compared to the old alphabetical insertion. */
	gtk_tree_store_reorder(GTK_TREE_STORE(this->tree_model), parent_index, positions);
	free(positions);
#endif
}




static int vik_tree_view_drag_data_received(GtkTreeDragDest *drag_dest, GtkTreePath *dest, GtkSelectionData *selection_data)
{
	bool retval = false;
#ifdef K_FIXME_RESTORE
	QStandardItemModel *tree_model;
	QStandardItemModel *src_model = NULL;
	GtkTreePath *src_path = NULL, *dest_cp = NULL;
	TreeIndex src_index;
	GtkTreeIter root_iter;
	TreeIndex dest_parent_index;
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
		if (!gtk_tree_model_get_iter(src_model, src_index, src_path)) {
			goto out;
		}
		if (!gtk_tree_path_compare(src_path, dest)) {
			goto out;
		}

		dest_cp = gtk_tree_path_copy(dest);

		gtk_tree_model_get_iter_first(tree_model, &root_iter);
		TREEVIEW_GET(tree_model, &root_iter, COLUMN_ITEM, &layer);

		if (gtk_tree_path_get_depth(dest_cp) > 1) { /* Can't be sibling of top layer. */

			TreeItem * tree_item = NULL;

			/* Find the first ancestor that is a full layer, and store in dest_parent_index. */
			do {
				gtk_tree_path_up(dest_cp);
				gtk_tree_model_get_iter(src_model, &dest_parent_index, dest_cp);

				tree_item = layer->tree_view->get_tree_item(dest_parent_index);
			} while (gtk_tree_path_get_depth(dest_cp) > 1 && tree_item->tree_item_type != TreeItemType::Layer);


			tree_item = layer->tree_view->get_tree_item(&src_item);
			Layer * layer_source  = (Layer *) tree_item->owning_layer;
			assert (layer_source);
			Layer * layer_dest = layer->tree_view->get_tree_item(dest_parent_index)->to_layer();

			/* TODO_LATER: might want to allow different types, and let the clients handle how they want. */
			layer_dest->drag_drop_request(layer_source, src_index, dest);
		}
	}

 out:
	if (dest_cp) {
		gtk_tree_path_free(dest_cp);
	}

	if (src_path) {
		gtk_tree_path_free(src_path);
	}
#endif
	return retval;
}




/*
 * This may not be necessary.
 */
static int vik_tree_view_drag_data_delete(GtkTreeDragSource *drag_source, GtkTreePath *path)
{
#ifdef K_FIXME_RESTORE
	char *s_dest = gtk_tree_path_to_string(path);
	fprintf(stdout, QObject::tr("delete data from %1\n").arg(s_dest));
	free(s_dest);
#endif
	return false;
}




sg_ret TreeView::insert_tree_item_at_row(const TreeItem * parent_tree_item, TreeItem * tree_item, int row)
{
	if (parent_tree_item) {
		qDebug() << SG_PREFIX_I << "Inserting tree item" << tree_item->name << "under parent tree item" << parent_tree_item->name;
	} else {
		qDebug() << SG_PREFIX_I << "Inserting tree item" << tree_item->name << "on top of tree";
	}

	QList<QStandardItem *> items = this->create_new_row(tree_item, tree_item->name);

	if (parent_tree_item && parent_tree_item->index.isValid()) {
		this->tree_model->itemFromIndex(parent_tree_item->index)->insertRow(row, items);
	} else {
		/* Adding tree item just right under top-level item. */
		this->tree_model->invisibleRootItem()->insertRow(row, items);
	}

	tree_item->index = QPersistentModelIndex(items.at(0)->index());
	tree_item->tree_view = this;

	/* Some tree items may have been created in other thread
	   (e.g. during acquire process). Signal connections for such
	   objects will not work. Fix this by moving the object to
	   main thread.
	   http://doc.qt.io/archives/qt-4.8/threads-qobject.html */
	tree_item->moveToThread(QApplication::instance()->thread());

	//connect(this->tree_model, SIGNAL(itemChanged(QStandardItem*)), item, SLOT(visibility_toggled_cb(QStandardItem *)));

	qDebug() << SG_PREFIX_I;

	return sg_ret::ok;
}




TreeView::TreeView(TreeItem * top_level_layer, QWidget * parent_widget) : QTreeView(parent_widget)
{
	this->tree_model = new TreeModel(this, NULL);



	QStandardItem * header_item = NULL;

	header_item = new QStandardItem("Item Name");
	this->tree_model->setHorizontalHeaderItem((int) TreeViewColumn::Name, header_item);

	header_item = new QStandardItem("Visible");
	this->tree_model->setHorizontalHeaderItem((int) TreeViewColumn::Visible, header_item);

	header_item = new QStandardItem("Item");
	this->tree_model->setHorizontalHeaderItem((int) TreeViewColumn::TreeItem, header_item);

	header_item = new QStandardItem("Editable");
	this->tree_model->setHorizontalHeaderItem((int) TreeViewColumn::Editable, header_item);

	header_item = new QStandardItem("Time stamp");
	this->tree_model->setHorizontalHeaderItem((int) TreeViewColumn::Timestamp, header_item);


	this->setModel(this->tree_model);
	this->expandAll();


	this->setSelectionMode(QAbstractItemView::SingleSelection);


	this->header()->setSectionResizeMode((int) TreeViewColumn::Visible, QHeaderView::ResizeToContents); /* This column holds only a checkbox, so let's limit its width to column label. */
	this->header()->setSectionHidden((int) TreeViewColumn::TreeItem, true);
	this->header()->setSectionHidden((int) TreeViewColumn::Editable, true);
	this->header()->setSectionHidden((int) TreeViewColumn::Timestamp, true);


	//connect(this, SIGNAL(activated(const QModelIndex &)), this, SLOT(select_cb(void)));
	connect(this, SIGNAL(clicked(const QModelIndex &)), this, SLOT(select_cb(void)));
	//connect(this, SIGNAL(pressed(const QModelIndex &)), this, SLOT(select_cb(void)));
	connect(this->tree_model, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)), this, SLOT(data_changed_cb(const QModelIndex&, const QModelIndex&)));



	/* Drag & Drop. */
	this->setDragEnabled(true);
	this->setDropIndicatorShown(true);
	this->setAcceptDrops(true);


#ifdef K_FIXME_RESTORE
	/* Can not specify 'auto' sort order with a 'GtkTreeSortable' on the name since we want to control the ordering of layers.
	   Thus need to create special sort to operate on a subsection of tree_view (i.e. from a specific child either a layer or sublayer).
	   See vik_tree_view_sort_children(). */

	gtk_tree_view_set_reorderable(this, true);
	QObject::connect(this->selectionModel(), SIGNAL("changed"), this, SLOT (select_cb));
#endif


	TreeItem * invalid_parent_tree_item = NULL; /* Top layer doesn't have any parent index. */
	const int row = 0;
	qDebug() << SG_PREFIX_I << "Inserting top level layer in row" << row;
	this->insert_tree_item_at_row(invalid_parent_tree_item, top_level_layer, row);
	qDebug() << SG_PREFIX_I;
}




TreeView::~TreeView()
{
}




/**
   Called when data in tree view has been changed

   Should execute column-specific code.

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
		qDebug() << SG_PREFIX_E << "Invalid TreeIndex from valid index" << top_left;
		return;
	}

	TreeItem * tree_item = this->get_tree_item(*index);
	if (!tree_item) {
		qDebug() << SG_PREFIX_E << "Failed to get tree item from valid index";
		return;
	}

	QStandardItem * item = this->tree_model->itemFromIndex(*index);
	if (!item) {
		qDebug() << SG_PREFIX_E << "Failed to get standard item from valid index";
		return;
	}

	const TreeViewColumn col = (TreeViewColumn) index->column();
	switch (col) {
	case TreeViewColumn::Name:
		if (item->text().isEmpty()) {
			qDebug() << SG_PREFIX_W << "Edited item in column Name: new name is empty, ignoring the change";
			/* We have to undo the action of setting empty text label. */
			item->setText(tree_item->name);
		} else {
			qDebug() << SG_PREFIX_I << "Edited item in column Name: new name is" << item->text();
			tree_item->name = item->text();
		}

		break;

	case TreeViewColumn::Visible:
		qDebug() << SG_PREFIX_I << "Edited item in column Visible: is checkable?" << item->isCheckable();

		tree_item->visible = (bool) item->checkState();
		qDebug() << SG_PREFIX_SIGNAL << "Eemitting tree_item_needs_redraw(), uid=", tree_item->get_uid();
		emit this->tree_item_needs_redraw(tree_item->get_uid());
		break;

	case TreeViewColumn::TreeItem:
		qDebug() << SG_PREFIX_W << "Edited item in column TreeItem";
		break;

	case TreeViewColumn::Editable:
		qDebug() << SG_PREFIX_W << "Edited item in column Editable";
		break;

	case TreeViewColumn::Timestamp:
		qDebug() << SG_PREFIX_W << "Edited item in column Timestamp";
		break;

	default:
		qDebug() << SG_PREFIX_E << "Edited item in unknown column" << (int) col;
		break;
	}

	return;
}




Qt::ItemFlags TreeModel::flags(const QModelIndex & idx) const
{
	Qt::ItemFlags defaultFlags = QStandardItemModel::flags(idx);

	if (idx.isValid()) {
		return Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | defaultFlags;
	} else {
		return Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | defaultFlags;
	}
}




bool TreeModel::canDropMimeData(const QMimeData * data_, Qt::DropAction action, int row, int column, const QModelIndex & parent_)
{
	Q_UNUSED(action);
	Q_UNUSED(row);
	Q_UNUSED(parent_);

#ifdef K_FIXME_RESTORE
	if (!data_->hasFormat("application/vnd.text.list")) {
		return false;
	}

	if (column > 0) {
		return false;
	}
#endif

	if (!parent_.isValid()) {
		/* Don't let dropping items on top level. */
		return false;
	}


	TreeItem * parent_item = this->view->get_tree_item(parent_);
	if (!parent_item) {
		qDebug() << SG_PREFIX_E << "Can't find parent item";
		return false;
	}

	if (parent_item->tree_item_type == TreeItemType::Layer) {
		qDebug() << SG_PREFIX_I << "Can drop on Layer";
		return true;
	} else if (parent_item->tree_item_type == TreeItemType::Sublayer) {
		qDebug() << SG_PREFIX_I << "Can drop on Sublayer";
		return true;
	} else {
		qDebug() << SG_PREFIX_E << "Wrong type of parent:" << (int) parent_item->tree_item_type;
		return false;
	}


	return true;
}




/*
  http://doc.qt.io/qt-5/qabstractitemmodel.html#dropMimeData
*/
bool TreeModel::dropMimeData(const QMimeData * data_, Qt::DropAction action, int row, int column, const QModelIndex & parent_)
{
	if (!canDropMimeData(data_, action, row, column, parent_)) {
		qDebug() << SG_PREFIX_D << "Can't drop mime data";
		return false;
	}

	if (action == Qt::IgnoreAction) {
		qDebug() << SG_PREFIX_D << "Ignore action";
		return true;
	}

	if (row == -1 && column == -1) {
		/* Drop onto an existing item. */
		if (parent_.isValid()) {
			TreeItem * parent_item = this->view->get_tree_item(parent_);
			if (!parent_item) {
				qDebug() << SG_PREFIX_I << "Can't find parent item";
				return false;
			} else {
				qDebug() << SG_PREFIX_I << "Dropping onto existing item, parent =" << parent_.row() << parent_.column() << parent_item->name << (int) parent_item->tree_item_type;
				return true;
			}
		} else {
			return false;
		}
	} else {
		qDebug() << SG_PREFIX_I << "Dropping as sibling, parent =" << parent_.row() << parent_.column() << row << column;
		if (parent_.isValid()) {
			return true;
		} else {
			qDebug() << SG_PREFIX_I << "Invalid parent";
			return false;
		}
	}
}




Qt::DropActions TreeModel::supportedDropActions() const
{
	return Qt::MoveAction;
}




static void vik_tree_view_edited_cb(GtkCellRendererText *cell, char *path_str, const char *new_name, TreeView * tree_view)
{
#ifdef K_FIXME_RESTORE
	tree_view->editing = false;

	/* Get type and data. */
	TreeIndex * index = tree_view->get_index_from_path_str(path_str);

	g_signal_emit(G_OBJECT(tree_view), tree_view_signals[VT_ITEM_EDITED_SIGNAL], 0, index, new_name);
#endif
}




static void vik_tree_view_edit_start_cb(GtkCellRenderer *cell, GtkCellEditable *editable, char *path, TreeView * tree_view)
{
#ifdef K_FIXME_RESTORE
	tree_view->editing = true;
#endif
}




static void vik_tree_view_edit_stop_cb(GtkCellRenderer *cell, TreeView * tree_view)
{
#ifdef K_FIXME_RESTORE
	tree_view->editing = false;
#endif
}




#ifdef K_OLD_IMPLEMENTATION
TreeIndex * TreeView::get_index_from_path_str(char const * path_str)
{
	TreeIndex * index = NULL;

	return gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL (this->tree_model), iter, path_str);

	return index;
}
#endif




bool TreeView::is_editing_in_progress(void) const
{
	/* Don't know how to get cell for the selected item. */
	//return ((int) (long) g_object_get_data(G_OBJECT(cell), "editing"));

	/* Instead maintain our own value applying to the whole tree. */
	return this->editing;
}




bool TreeView::tree_item_properties_cb(void) /* Slot. */
{
	TreeItem * selected_item = this->get_selected_tree_item();
	if (!selected_item) {
		return false;
	}

	if (!selected_item->has_properties_dialog) {
		Dialog::info(tr("This item has no configurable properties."), g_tree->tree_get_main_window());
		qDebug() << SG_PREFIX_I << "Selected item" << selected_item->type_id << "has no configurable properties";
		return true;
	}

	bool result = selected_item->properties_dialog();
	if (result) {
		if (selected_item->tree_item_type == TreeItemType::Layer) {
			selected_item->to_layer()->emit_layer_changed("Tree View - Item Properties");
		}
		return true;
	}

	return false;
}




void Tree::add_to_set_of_selected(TreeItem * tree_item)
{
	/* At this moment we support selection of only one item at a
	   time. So if anyone selects a new item, all other (old) selected
	   items must be forgotten. */
	this->selected_tree_items.clear();

	this->selected_tree_items.insert(std::pair<sg_uid_t, TreeItem *>(tree_item->uid, tree_item));
}




bool Tree::remove_from_set_of_selected(const TreeItem * tree_item)
{
	if (!tree_item) {
		return 0;
	}

	/* erase() returns number of removed items. */
	return 0 != this->selected_tree_items.erase(tree_item->uid);
}




bool Tree::is_in_set_of_selected(const TreeItem * tree_item) const
{
	if (!tree_item) {
		return false;
	}

	return this->selected_tree_items.end() != this->selected_tree_items.find(tree_item->uid);
}
