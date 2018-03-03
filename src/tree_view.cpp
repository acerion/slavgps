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
#include <QDebug>
#include <QHeaderView>

//#include "layer.h"
#include "window.h"
#include "tree_view.h"
#include "tree_view_internal.h"
#include "layers_panel.h"
#include "globals.h"
#include "ui_builder.h"
#include "dialog.h"
//#include "layer_aggregate.h"
//#include "layer_coord.h"




using namespace SlavGPS;




#define PREFIX ": Tree View:" << __FUNCTION__ << __LINE__ << ">"




extern Tree * g_tree;


typedef int GtkTreeDragSource;
typedef int GtkTreeDragDest;
typedef int GtkCellRenderer;
typedef int GtkCellRendererToggle;
typedef int GtkCellRendererText;
typedef int GtkCellEditable;
typedef int GtkSelectionData;
typedef int GtkTreeSelection;
typedef int GtkTreePath;

static int vik_tree_view_drag_data_received(GtkTreeDragDest *drag_dest, GtkTreePath *dest, GtkSelectionData *selection_data);
static int vik_tree_view_drag_data_delete(GtkTreeDragSource *drag_source, GtkTreePath *path);




TreeItem * TreeView::get_tree_item(TreeIndex const & item_index)
{
	QStandardItem * parent_item = this->model->itemFromIndex(item_index.parent());
	if (!parent_item) {
		/* "item_index" points at the top tree item. */
		qDebug() << "II: Tree View: get tree item: querying Top Tree Item for item" << item_index.row() << item_index.column();
		parent_item = this->model->invisibleRootItem();
	}
	QStandardItem * ch = parent_item->child(item_index.row(), (int) TreeViewColumn::TREE_ITEM);

	QVariant variant = ch->data(RoleLayerData);
	// http://www.qtforum.org/article/34069/store-user-data-void-with-qstandarditem-in-qstandarditemmodel.html
	return variant.value<TreeItem *>();
}




void TreeView::set_tree_item_timestamp(TreeIndex const & item_index, time_t timestamp)
{
	QStandardItem * parent_item = this->model->itemFromIndex(item_index.parent());
	if (!parent_item) {
		/* "item_index" points at the top tree item. */
		qDebug() << "II: Tree View: set tree item timestamp: querying Top Tree Item for item" << item_index.row() << item_index.column();
		parent_item = this->model->invisibleRootItem();
	}
	QStandardItem * ch = parent_item->child(item_index.row(), (int) TreeViewColumn::TIMESTAMP);

	QVariant variant = QVariant::fromValue((qlonglong) timestamp);
	this->model->setData(ch->index(), variant, RoleLayerData);
}




void TreeView::select_cb(void) /* Slot. */
{
	TreeItem * selected_item = this->get_selected_tree_item();
	if (!selected_item) {
		return;
	}

	Window * main_window = g_tree->tree_get_main_window();

	/* Clear statusbar. */
	main_window->get_statusbar()->set_message(StatusBarField::INFO, "");

	qDebug() << "II" PREFIX << "selected item is" << (selected_item->tree_item_type == TreeItemType::LAYER ? "layer" : "sublayer");

	/* Either the selected layer itself, or an owner/parent of selected sublayer item. */
	const Layer * layer = selected_item->to_layer();

	/* This should activate toolbox relevant to selected layer's type. */
	main_window->handle_selection_of_layer(layer);

	const bool redraw_required = selected_item->handle_selection_in_tree();
	if (redraw_required) {
		qDebug() << "SIGNAL" PREFIX << "will call 'emit_items_tree_updated_cb()' for" << selected_item->name;
		g_tree->tree_get_items_tree()->emit_items_tree_updated_cb(selected_item->name);
	}
}




bool TreeView::move(TreeIndex const & item_index, bool up)
{
	TreeItem * item = this->get_tree_item(item_index);
	if (item->tree_item_type != TreeItemType::LAYER) {
		return false;
	}

#ifdef K_TODO
	TreeIndex switch_index;
	if (up) {
		/* Iter to path to iter. */
		GtkTreePath *path = gtk_tree_model_get_path(this->model, item_index);
		if (!gtk_tree_path_prev(path) || !gtk_tree_model_get_iter(this->model, &switch_index, path)) {
			gtk_tree_path_free(path);
			return false;
		}
		gtk_tree_path_free(path);
	} else {
		switch_index = *item_index;
		if (!gtk_tree_model_iter_next(this->model, &switch_index)) {
			return false;
		}
	}
	gtk_tree_store_swap(GTK_TREE_STORE(this->model), item_index, &switch_index);
#endif
	/* Now, the easy part. actually switching them, not the GUI. */
	/* If item is map... */

	return true;
}




void TreeView::select_and_expose(TreeIndex const & index)
{
	this->setCurrentIndex(index);
}




TreeItem * TreeView::get_selected_tree_item(void)
{
	TreeIndex selected = QPersistentModelIndex(this->currentIndex());
	if (!selected.isValid()) {
		return NULL;
	}

	TreeItem * item = this->get_tree_item(selected);
	if (!item) {
		qDebug() << "EE: TreeView: get selected tree item: can't get item for valid index";
	}

	return item;
}




void TreeView::erase(TreeIndex const & index)
{
	this->model->removeRow(index.row(), index.parent());
}




void TreeView::set_tree_item_icon(TreeIndex const & item_index, const QIcon & icon)
{
	if (!item_index.isValid()) {
		qDebug() << "EE: TreeView: invalid item index in" << __FUNCTION__;
		return;
	}

	if (icon.isNull()) {
		/* Not an error. Perhaps there is no resource defined for an icon. */
		return;
	}

	/* Item index may be pointing to first column. We want to update TreeViewColumn::ICON column. */

	QStandardItem * parent_item = this->model->itemFromIndex(item_index.parent());
	if (!parent_item) {
		/* "item_index" points at the top-level item. */
		qDebug() << "II: Tree View: set tree item icon: querying Top Level Item for item" << item_index.row() << item_index.column();
		parent_item = this->model->invisibleRootItem();
	}
	QStandardItem * ch = parent_item->child(item_index.row(), (int) TreeViewColumn::ICON);
	ch->setIcon(icon);
}




void TreeView::set_tree_item_name(TreeIndex const & item_index, QString const & name)
{
	if (!item_index.isValid()) {
		qDebug() << "EE: TreeView: invalid item index in" << __FUNCTION__;
		return;
	}
	this->model->itemFromIndex(item_index)->setText(name);
}




bool TreeView::get_tree_item_visibility(TreeIndex const & index)
{
	QStandardItem * parent_item = this->model->itemFromIndex(index.parent());
	if (!parent_item) {
		/* "index" points at the top-level item. */
		qDebug() << "II: Tree View: get tree item visibility: querying Top Level Item for item" << index.row() << index.column();
		parent_item = this->model->invisibleRootItem();
	}
	QStandardItem * ch = parent_item->child(index.row(), (int) TreeViewColumn::VISIBLE);

	QVariant variant = ch->data();
	return ch->checkState() != Qt::Unchecked; /* See if Item is either checked (Qt::Checked) or partially checked (Qt::PartiallyChecked). */
}




/**
   Get visibility of an item considering visibility of all parents
   i.e. if any parent is invisible then this item will also be considered
   invisible (even though it itself may be marked as visible).
*/
bool TreeView::get_tree_item_visibility_with_parents(TreeIndex const & item_index)
{
	int loop_depth = 1;

	TreeIndex this_item_index = item_index;

	do {
#if 1           /* Debug. */
		TreeItem * item = this->get_tree_item(this_item_index);
		qDebug() << "II: TreeView: Checking visibility of" << item->name << "in tree, forever loop depth =" << loop_depth++;
#endif

		if (!this->get_tree_item_visibility(this_item_index)) {
			/* Simple case: this item is not visible. */
			return false;
		}
		/* This item is visible. What about its parent? */

		TreeIndex parent_item_index = this_item_index.parent();
		if (!parent_item_index.isValid()) {
			/* This item doesn't have valid parent, so it
			   must be a top-level item. The top-level item
			   was visible (we checked this few lines above),
			   so return true. */
			return true;
		}

		this_item_index = parent_item_index;

	} while (1); /* Forever loop that will finish either on first invisible item it meets, or on visible top-level item. */
}




void TreeView::set_tree_item_visibility(TreeIndex const & item_index, bool visible)
{
	if (!!item_index.isValid()) {
		qDebug() << "EE: Tree View: invalid item index in" << __FUNCTION__;
		return;
	}
	/* kamilFIXME: this does not take into account third state. */
	QModelIndex visible_index = item_index.sibling(item_index.row(), (int) TreeViewColumn::VISIBLE);
	this->model->itemFromIndex(visible_index)->setCheckState(visible ? Qt::Checked : Qt::Unchecked);
}




void TreeView::toggle_tree_item_visibility(TreeIndex const & item_index)
{
	if (!item_index.isValid()) {
		qDebug() << "EE: Tree View: invalid item index in" << __FUNCTION__;
		return;
	}

	/* kamilFIXME: this does not take into account third state. */
	QModelIndex visible_index = item_index.sibling(item_index.row(), (int) TreeViewColumn::VISIBLE);
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
#ifdef K_TODO
	gtk_tree_selection_unselect_iter(gtk_tree_view_get_selection(this), iter);
#endif
}




/*
  TODO: improve handling of 'editable' property.
  Non-editable items have e.g limited number of fields in context menu.

  The following properties of @tree_item are used to set properties of entry in tree:
  - TreeItem::editable
  - TreeItem::visible;
  - TreeItem::get_tooltip()
*/
TreeIndex const & TreeView::add_tree_item(TreeIndex const & parent_index, TreeItem * tree_item, const QString & name)
{
	// http://www.qtforum.org/article/34069/store-user-data-void-with-qstandarditem-in-qstandarditemmodel.html

	qDebug() << "II" PREFIX << "adding tree item" << name;

	QList<QStandardItem *> items;
	QStandardItem * item = NULL;
	QStandardItem * first_item = NULL;
	QVariant variant;

	const QString tooltip = tree_item->get_tooltip();


	/* TreeViewColumn::NAME */
	item = new QStandardItem(name);
	item->setToolTip(tooltip);
	item->setEditable(tree_item->editable);
	first_item = item;
	items << item;

	/* TreeViewColumn::VISIBLE */
	item = new QStandardItem();
	item->setToolTip(tooltip);
	item->setCheckable(true);
	item->setCheckState(tree_item->visible ? Qt::Checked : Qt::Unchecked);
	items << item;

	/* TreeViewColumn::ICON */
	/* Value in this column can be set with ::set_tree_item_icon(). */
	item = new QStandardItem();
	item->setToolTip(tooltip);
#if 0
	item->setIcon();
#endif
	item->setEditable(false);
	items << item;

	/* TreeViewColumn::TREE_ITEM */
	item = new QStandardItem();
	variant = QVariant::fromValue(tree_item);
	item->setData(variant, RoleLayerData);
	items << item;

	/* TreeViewColumn::EDITABLE */
	item = new QStandardItem();
	variant = QVariant::fromValue(tree_item->editable);
	item->setData(variant, RoleLayerData);
	items << item;

	/* TreeViewColumn::TIMESTAMP */
	/* Value in this column can be set with ::set_tree_item_timestamp(). */
	qlonglong timestamp = 0;
	item = new QStandardItem((qlonglong) timestamp);
	items << item;


	if (parent_index.isValid()) {
		this->model->itemFromIndex(parent_index)->appendRow(items);
	} else {
		/* Adding tree item just right under top-level item. */
		this->model->invisibleRootItem()->appendRow(items);
	}
	//connect(this->model, SIGNAL(itemChanged(QStandardItem*)), item, SLOT(visibility_toggled_cb(QStandardItem *)));

	tree_item->index = QPersistentModelIndex(first_item->index());
	tree_item->tree_view = this;

	return tree_item->index;
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
#ifdef K_TODO
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
void TreeView::sort_children(TreeIndex const & parent_index, sort_order_t order)
{
	if (order == VL_SO_NONE) {
		/* Nothing to do. */
		return;
	}

#ifdef K_TODO
	GtkTreeIter child;
	if (!gtk_tree_model_iter_children(this->model, &child, parent_index)) {
		return;
	}

	unsigned int length = gtk_tree_model_iter_n_children(this->model, parent_index);

	/* Create an array to store the position offsets. */
	SortTuple *sort_array;
	sort_array = (SortTuple *) malloc(length * sizeof (SortTuple));

	unsigned int ii = 0;
	do {
		sort_array[ii].offset = ii;
		gtk_tree_model_get(this->model, &child, COLUMN_NAME, &(sort_array[ii].name), -1);
		gtk_tree_model_get(this->model, &child, COLUMN_TIMESTAMP, &(sort_array[ii].timestamp), -1);
		ii++;
	} while (gtk_tree_model_iter_next(this->model, &child));

	/* Sort list... */
	g_qsort_with_data(sort_array,
			  length,
			  sizeof (SortTuple),
			  sort_tuple_compare,
			  KINT_TO_POINTER(order));

	/* As the sorted list contains the reordered position offsets, extract this and then apply to the tree view. */
	int * positions = (int *) malloc(sizeof(int) * length);
	for (ii = 0; ii < length; ii++) {
		positions[ii] = sort_array[ii].offset;
		free(sort_array[ii].name);
	}
	free(sort_array);

	/* This is extremely fast compared to the old alphabetical insertion. */
	gtk_tree_store_reorder(GTK_TREE_STORE(this->model), parent_index, positions);
	free(positions);
#endif
}




static int vik_tree_view_drag_data_received(GtkTreeDragDest *drag_dest, GtkTreePath *dest, GtkSelectionData *selection_data)
{
	bool retval = false;
#ifdef K_TODO
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

			TreeItem * item = NULL;

			/* Find the first ancestor that is a full layer, and store in dest_parent_index. */
			do {
				gtk_tree_path_up(dest_cp);
				gtk_tree_model_get_iter(src_model, &dest_parent_index, dest_cp);

				item = layer->tree_view->get_tree_item(dest_parent_index);
			} while (gtk_tree_path_get_depth(dest_cp) > 1 && item->tree_item_type != TreeItemType::LAYER);


			TreeItem * item = layer->tree_view->get_tree_item(&src_item);
			Layer * layer_source  = (Layer *) item->owning_layer;
			assert (layer_source);
			Layer * layer_dest = layer->tree_view->get_tree_item(dest_parent_index)->to_layer();

			/* TODO: might want to allow different types, and let the clients handle how they want. */
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
#ifdef K_TODO
	char *s_dest = gtk_tree_path_to_string(path);
	fprintf(stdout, QObject::tr("delete data from %1\n").arg(s_dest);
	free(s_dest);
#endif
	return false;
}





TreeIndex const & TreeView::insert_tree_item(TreeIndex const & parent_index, TreeIndex const & sibling_index, TreeItem * item, bool above, const QString & name)
{
#ifdef K_TODO
	if (sibling_index.isValid()) {
		if (above) {
			gtk_tree_store_insert_before(this->model, iter, parent_iter, sibling_index);
		} else {
			gtk_tree_store_insert_after(this->model, iter, parent_iter, sibling_index);
		}
	} else
#endif
		{
			this->add_tree_item(parent_index, item, name);
			return item->index;
		}
}




TreeView::TreeView(QWidget * parent_widget) : QTreeView(parent_widget)
{
	this->model = new TreeModel(this, NULL);



	QStandardItem * header_item = NULL;

	header_item = new QStandardItem("Item Name");
	this->model->setHorizontalHeaderItem((int) TreeViewColumn::NAME, header_item);

	header_item = new QStandardItem("Visible");
	this->model->setHorizontalHeaderItem((int) TreeViewColumn::VISIBLE, header_item);

	header_item = new QStandardItem("Type");
	this->model->setHorizontalHeaderItem((int) TreeViewColumn::ICON, header_item);

	header_item = new QStandardItem("Item");
	this->model->setHorizontalHeaderItem((int) TreeViewColumn::TREE_ITEM, header_item);

	header_item = new QStandardItem("Editable");
	this->model->setHorizontalHeaderItem((int) TreeViewColumn::EDITABLE, header_item);

	header_item = new QStandardItem("Time stamp");
	this->model->setHorizontalHeaderItem((int) TreeViewColumn::TIMESTAMP, header_item);


	this->setModel(this->model);
	this->expandAll();


	this->setSelectionMode(QAbstractItemView::SingleSelection);


	this->header()->setSectionResizeMode((int) TreeViewColumn::VISIBLE, QHeaderView::ResizeToContents); /* This column holds only a checkbox, so let's limit its width to column label. */
	this->header()->setSectionHidden((int) TreeViewColumn::TREE_ITEM, true);
	this->header()->setSectionHidden((int) TreeViewColumn::EDITABLE, true);
	this->header()->setSectionHidden((int) TreeViewColumn::TIMESTAMP, true);


	//connect(this, SIGNAL(activated(const QModelIndex &)), this, SLOT(select_cb(void)));
	connect(this, SIGNAL(clicked(const QModelIndex &)), this, SLOT(select_cb(void)));
	//connect(this, SIGNAL(pressed(const QModelIndex &)), this, SLOT(select_cb(void)));
	connect(this->model, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)), this, SLOT(data_changed_cb(const QModelIndex&, const QModelIndex&)));



	/* Drag & Drop. */
	this->setDragEnabled(true);
	this->setDropIndicatorShown(true);
	this->setAcceptDrops(true);


#if 0
	/* Can not specify 'auto' sort order with a 'GtkTreeSortable' on the name since we want to control the ordering of layers.
	   Thus need to create special sort to operate on a subsection of tree_view (i.e. from a specific child either a layer or sublayer).
	   See vik_tree_view_sort_children(). */

	gtk_tree_view_set_reorderable(this, true);
	QObject::connect(gtk_tree_view_get_selection(this), SIGNAL("changed"), this, SLOT (select_cb));
#endif
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
		qDebug() << "EE: Tree View: invalid TreeIndex from valid index" << top_left;
		return;
	}

	TreeItem * tree_item = this->get_tree_item(*index);
	if (!tree_item) {
		qDebug() << "EE: Tree View: failed to get tree item from valid index";
		return;
	}

	QStandardItem * item = this->model->itemFromIndex(*index);
	if (!item) {
		qDebug() << "EE: Tree View: failed to get standard item from valid index";
		return;
	}


	switch (index->column()) {
	case (int) TreeViewColumn::NAME:
		qDebug() << "II: Tree View: edited item in column NAME: new name is" << item->text();

		/* TODO: reject empty new name. */
		tree_item->name = item->text();
		break;

	case (int) TreeViewColumn::VISIBLE:
		qDebug() << "II: Tree View: edited item in column VISIBLE: is checkable?" << item->isCheckable();

		tree_item->visible = (bool) item->checkState();
		qDebug() << "SIGNAL: Tree View tree_item_needs_redraw(), uid=", tree_item->uid;
		emit this->tree_item_needs_redraw(tree_item->uid);
		break;

	case (int) TreeViewColumn::ICON:
		qDebug() << "WW: Tree View: edited item in column ICON";
		break;

	case (int) TreeViewColumn::TREE_ITEM:
		qDebug() << "WW: Tree View: edited item in column TREE_ITEM";
		break;

	case (int) TreeViewColumn::EDITABLE:
		qDebug() << "WW: Tree View: edited item in column EDITABLE";
		break;

	case (int) TreeViewColumn::TIMESTAMP:
		qDebug() << "WW: Tree View: edited item in column EDITABLE";
		break;

	default:
		qDebug() << "EE: Tree View: edited item in unknown column" << index->column();
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

#if 0
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
	if (parent_item->tree_item_type == TreeItemType::LAYER) {
		qDebug() << "EE: Tree View: Drag&Drop: canDropMimeData: can drop on Layer";
		return true;
	} else if (parent_item->tree_item_type == TreeItemType::SUBLAYER) {
		qDebug() << "EE: Tree View: Drag&Drop: canDropMimeData: can drop on Sublayer";
		return true;
	} else {
		qDebug() << "EE: Tree View: Drag&Drop: canDropMimeData: wrong type of parent:" << (int) parent_item->tree_item_type;
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
		qDebug() << "DD: can't drop mime data";
		return false;
	}

	if (action == Qt::IgnoreAction) {
		qDebug() << "DD: ignore action";
		return true;
	}

	if (row == -1 && column == -1) {
		/* Drop onto an existing item. */
		if (parent_.isValid()) {
			TreeItem * parent_item = this->view->get_tree_item(parent_);
			qDebug() << "II: Tree View: Drop Mime Data: dropping onto existing item, parent =" << parent_.row() << parent_.column() << parent_item->name << (int) parent_item->tree_item_type;
			return true;
		} else {
			return false;
		}
	} else {
		qDebug() << "II: Tree View: Drop Mime Data: dropping as sibling, parent =" << parent_.row() << parent_.column() << row << column;
		if (parent_.isValid()) {
			return true;
		} else {
			qDebug() << "II: Tree View: Drop Mime Data: invalid parent";
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
	tree_view->editing = false;

	/* Get type and data. */
	TreeIndex * index = tree_view->get_index_from_path_str(path_str);

#ifdef K_TODO
	g_signal_emit(G_OBJECT(tree_view), tree_view_signals[VT_ITEM_EDITED_SIGNAL], 0, index, new_name);
#endif
}




static void vik_tree_view_edit_start_cb(GtkCellRenderer *cell, GtkCellEditable *editable, char *path, TreeView * tree_view)
{
	tree_view->editing = true;
}




static void vik_tree_view_edit_stop_cb(GtkCellRenderer *cell, TreeView * tree_view)
{
	tree_view->editing = false;
}




TreeIndex * TreeView::get_index_from_path_str(char const * path_str)
{
	TreeIndex * index = NULL;
#ifdef K_TODO
	return gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL (this->model), iter, path_str);
#endif
	return index;
}




#ifdef K_TODO
void TreeView::add_columns()
{
	QObject::connect(renderer, SIGNAL("edited"), this, SLOT (vik_tree_view_edited_cb));
	QObject::connect(renderer, SIGNAL("editing-started"), this, SLOT (vik_tree_view_edit_start_cb));
	QObject::connect(renderer, SIGNAL("editing-canceled"), this, SLOT (vik_tree_view_edit_stop_cb));
	QObject::connect(renderer, SIGNAL("toggled"), this, SLOT (vik_tree_view_toggled_cb));
}
#endif




TreeIndex * TreeView::get_index_at_pos(int pos_x, int pos_y)
{
	TreeIndex * index = NULL;
#ifdef K_TODO
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




bool TreeView::is_editing_in_progress()
{
	/* Don't know how to get cell for the selected item. */
	//return KPOINTER_TO_INT(g_object_get_data(G_OBJECT(cell), "editing"));

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
		qDebug() << "II: Tree View: selected item" << selected_item->type_id << "has no configurable properties";
		return true;
	}

	bool result = selected_item->properties_dialog();
	if (result) {
		if (selected_item->tree_item_type == TreeItemType::LAYER) {
			selected_item->to_layer()->emit_layer_changed();
		}
		return true;
	}

	return false;
}
