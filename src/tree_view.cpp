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
#include <QMimeData>
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
#define MY_MIME_TYPE "application/vnd.text.list"




typedef int GtkCellRenderer;
typedef int GtkCellRendererToggle;
typedef int GtkCellRendererText;
typedef int GtkCellEditable;




extern SelectedTreeItems g_selected;




QDataStream & operator<<(QDataStream & stream, const TreeItem * tree_item)
{
	qulonglong pointer(*reinterpret_cast<qulonglong *>(&tree_item));
	return stream << pointer;
}




QDataStream & operator>>(QDataStream & stream, TreeItem *& tree_item)
{
	qulonglong ptrval;
	stream >> ptrval;
	tree_item = *reinterpret_cast<TreeItem **>(&ptrval);
	return stream;
}




TreeItem * TreeView::get_tree_item(const TreeIndex & item_index) const
{
	if (item_index.row() == -1 || item_index.column() == -1) {
		qDebug() << SG_PREFIX_W << "Querying for item with -1 row or column";
		return NULL;
	}

	QStandardItem * parent_item = this->tree_model->itemFromIndex(item_index.parent());
	if (!parent_item) {
		/* "item_index" points at the top tree item. */
		//qDebug() << SG_PREFIX_I << "Querying Top Tree Item for item" << item_index.row() << item_index.column();
		parent_item = this->tree_model->invisibleRootItem();
	}
	QStandardItem * ch = parent_item->child(item_index.row(), this->property_id_to_column_idx(TreeItemPropertyID::TheItem));

	QVariant variant = ch->data(RoleLayerData);
	// http://www.qtforum.org/article/34069/store-user-data-void-with-qstandarditem-in-qstandarditemmodel.html
	return variant.value<TreeItem *>();
}




void TreeView::apply_tree_item_timestamp(const TreeItem * tree_item)
{
	QStandardItem * parent_item = this->tree_model->itemFromIndex(tree_item->index.parent());
	if (!parent_item) {
		/* "tree_item->index" points at the top tree item. */
		qDebug() << SG_PREFIX_I << "Querying Top Tree Item for item" << tree_item->index.row() << tree_item->index.column();
		parent_item = this->tree_model->invisibleRootItem();
	}
	QStandardItem * ch = parent_item->child(tree_item->index.row(), this->property_id_to_column_idx(TreeItemPropertyID::Timestamp));

	qDebug() << SG_PREFIX_I;

	QVariant variant = QVariant::fromValue((qlonglong) tree_item->get_timestamp().ll_value());
	this->tree_model->setData(ch->index(), variant, RoleLayerData);

	qDebug() << SG_PREFIX_I;
}




void TreeView::update_tree_item_tooltip(const TreeItem & tree_item)
{
	qDebug() << SG_PREFIX_I << "Called for tree item" << tree_item.get_name();
	QStandardItem * parent_item = this->tree_model->itemFromIndex(tree_item.index.parent());
	if (!parent_item) {
		/* "tree_item.index" points at the top tree item. */
		qDebug() << SG_PREFIX_I << "Querying Top Tree Item for item" << tree_item.index.row() << tree_item.index.column();
		parent_item = this->tree_model->invisibleRootItem();
	}

	/* Apply the tooltip only to main column with item's name.

	   Perhaps in future other columns will get their own
	   dedicated tooltips, but not now. */
	QStandardItem * ch = parent_item->child(tree_item.index.row(), this->property_id_to_column_idx(TreeItemPropertyID::TheItem));
	const QString tooltip = tree_item.get_tooltip();
	qDebug() << SG_PREFIX_I << "Generated tooltip" << tooltip << "for tree item" << tree_item.get_name();
	ch->setToolTip(tooltip);
}




void TreeView::tree_item_selected_cb(void) /* Slot. */
{
	qDebug() << SG_PREFIX_SLOT << "Handling signal";

	TreeItem * selected_item = this->get_selected_tree_item();
	if (!selected_item) {
		return;
	}
	qDebug() << SG_PREFIX_I << "Selected tree item" << selected_item->get_name();

	Window * main_window = ThisApp::get_main_window();

	/* Clear statusbar. */
	main_window->get_statusbar()->set_message(StatusBarField::Info, "");

	/* Activate set of tools relevant to selected item's type. */
	main_window->handle_selection_of_tree_item(*selected_item);

	qDebug() << SG_PREFIX_SIGNAL << "Will now emit signal TreeView::tree_item_selected()";
	emit this->tree_item_selected();

	const bool redraw_required = selected_item->handle_selection_in_tree();
	if (redraw_required) {
		qDebug() << SG_PREFIX_SIGNAL << "Will call 'emit_items_tree_updated_cb()' for" << selected_item->get_name();
		ThisApp::get_layers_panel()->emit_items_tree_updated_cb(selected_item->get_name());
	}
}




bool TreeView::change_tree_item_position(TreeItem * tree_item, bool up)
{
	if (!tree_item) {
		qDebug() << SG_PREFIX_E << "Trying to move NULL tree item";
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
		return NULL;
	}

	return tree_item;
}




void TreeView::detach_tree_item(TreeItem * tree_item)
{
	this->tree_model->removeRow(tree_item->index.row(), tree_item->index.parent());
	tree_item->tree_view = nullptr;
	tree_item->m_direct_parent_tree_item = nullptr;
}




void TreeView::detach_children(TreeItem * parent_tree_item)
{
	QStandardItem * parent_item = this->tree_model->itemFromIndex(parent_tree_item->index);
	parent_item->removeRows(0, parent_item->rowCount());
}




/**
   @reviewed-on: 2019-10-10
*/
void TreeView::apply_tree_item_icon(const TreeItem * tree_item)
{
	if (!tree_item->index.isValid()) {
		qDebug() << SG_PREFIX_E << "Invalid item index";
		return;
	}
	qDebug() << SG_PREFIX_I << "Setting icon for tree item" << tree_item->get_name();

	QStandardItem * parent_item = this->tree_model->itemFromIndex(tree_item->index.parent());
	if (!parent_item) {
		/* "tree_item->index" points at the top-level item. */
		qDebug() << SG_PREFIX_I << "Querying Top Level Item for item" << tree_item->index.row() << tree_item->index.column();
		parent_item = this->tree_model->invisibleRootItem();
	}

	/* Icon is a property of TreeItemPropertyID::TheItem column. */
	QStandardItem * child_item = parent_item->child(tree_item->index.row(), this->property_id_to_column_idx(TreeItemPropertyID::TheItem));
	/* Sometimes the icon may be null (QIcon::isNull()) - this can
	   happen e.g. when user selects "none" icon for waypoint. */
	child_item->setIcon(tree_item->icon);
}




void TreeView::apply_tree_item_name(const TreeItem * tree_item)
{
	if (!tree_item->index.isValid()) {
		qDebug() << SG_PREFIX_E << "Invalid item index";
		return;
	}
	this->tree_model->itemFromIndex(tree_item->index)->setText(tree_item->get_name());
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
	QStandardItem * ch = parent_item->child(index.row(), this->property_id_to_column_idx(TreeItemPropertyID::Visibility));

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
	__attribute__((unused)) int loop_depth = 1;

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

	QModelIndex visible_index = tree_item->index.sibling(tree_item->index.row(), this->property_id_to_column_idx(TreeItemPropertyID::Visibility));
	this->tree_model->itemFromIndex(visible_index)->setCheckState(tree_item->is_visible() ? Qt::Checked : Qt::Unchecked);

	return true;
}




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




/**
   \brief Add given @tree_item under given parent tree item

   @param parent_tree_index - parent tree item under which to put new @tree_item
   @param tree_item - tree_item to be added

   @return sg_ret::ok on success
   @return error value on failure
*/
sg_ret TreeView::attach_to_tree(TreeItem * parent_tree_item, TreeItem * tree_item, TreeViewAttachMode attach_mode, const TreeItem * sibling_tree_item)
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
	case TreeViewAttachMode::Front:
		row = 0;
		qDebug() << SG_PREFIX_I << "Pushing front tree item named" << tree_item->get_name() << "into row" << row;
		result = this->insert_tree_item_at_row(parent_tree_item, tree_item, row);
		qDebug() << SG_PREFIX_I;
		break;

	case TreeViewAttachMode::Back:
		row = this->tree_model->itemFromIndex(parent_tree_item->index)->rowCount();
		qDebug() << SG_PREFIX_I << "Pushing back tree item named" << tree_item->get_name() << "into row" << row;
		result = this->insert_tree_item_at_row(parent_tree_item, tree_item, row);
		qDebug() << SG_PREFIX_I;
		break;

	case TreeViewAttachMode::Before:
	case TreeViewAttachMode::After:
		if (NULL == sibling_tree_item) {
			qDebug() << SG_PREFIX_E << "Failed to attach tree item" << tree_item->get_name() << "next to sibling: NULL sibling";
			result = sg_ret::err;
			break;
		}

		if (!sibling_tree_item->index.isValid()) {
			qDebug() << SG_PREFIX_E << "Failed to attach tree item" << tree_item->get_name() << "next to sibling: invalid sibling";
			result = sg_ret::err;
			break;
		}

		row = sibling_tree_item->index.row() + (attach_mode == TreeViewAttachMode::Before ? 0 : 1);
		qDebug() << SG_PREFIX_I << "Pushing tree item named" << tree_item->get_name() << "next to sibling named" << sibling_tree_item->get_name() << "into row" << row;
		result = this->insert_tree_item_at_row(parent_tree_item, tree_item, row);
		qDebug() << SG_PREFIX_I;
		break;

	default:
		qDebug() << SG_PREFIX_E << "Unexpected attach mode" << (int) attach_mode;
		result = sg_ret::err;
		break;
	}


	if (sg_ret::ok != result) {
		qDebug() << SG_PREFIX_E << "Failed to attach child" << tree_item->get_name() << "under parent" << parent_tree_item->get_name() << "with mode" << (int) attach_mode << "into row" << row;
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

static int sort_tuple_compare(__attribute__((unused)) const void * a, __attribute__((unused)) const void * b, __attribute__((unused)) void * order)
{
	int answer = -1;

#ifdef K_FIXME_RESTORE
	SortTuple *sa = (SortTuple *)a;
	SortTuple *sb = (SortTuple *)b;
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
	__attribute__((unused)) TreeIndex const & parent_index = parent_tree_item->index;

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




sg_ret TreeView::insert_tree_item_at_row(TreeItem * new_parent_tree_item, TreeItem * tree_item, int row)
{
	if (new_parent_tree_item) {
		qDebug() << SG_PREFIX_I << "Inserting tree item" << tree_item->get_name() << "under parent tree item" << new_parent_tree_item->get_name();
	} else {
		qDebug() << SG_PREFIX_I << "Inserting tree item" << tree_item->get_name() << "on top of tree";
	}

	QList<QStandardItem *> items = tree_item->get_list_representation(this->view_format);

	if (new_parent_tree_item && new_parent_tree_item->index.isValid()) {
		this->tree_model->itemFromIndex(new_parent_tree_item->index)->insertRow(row, items);
	} else {
		/* Adding tree item just right under top-level item. */
		this->tree_model->invisibleRootItem()->insertRow(row, items);
	}

	tree_item->index = QPersistentModelIndex(items.at(0)->index());
	tree_item->tree_view = this;
	tree_item->m_direct_parent_tree_item = new_parent_tree_item;

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

	header_item = new QStandardItem("Item");
	this->tree_model->setHorizontalHeaderItem(this->property_id_to_column_idx(TreeItemPropertyID::TheItem), header_item);
	this->view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::TheItem, true, QObject::tr("Item")));

	header_item = new QStandardItem("Visible");
	this->tree_model->setHorizontalHeaderItem(this->property_id_to_column_idx(TreeItemPropertyID::Visibility), header_item);
	this->view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::Visibility, true, QObject::tr("Visible")));

	header_item = new QStandardItem("Editable");
	this->tree_model->setHorizontalHeaderItem(this->property_id_to_column_idx(TreeItemPropertyID::Editable), header_item);
	this->view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::Editable, false, QObject::tr("Editable")));

	header_item = new QStandardItem("Time stamp");
	this->tree_model->setHorizontalHeaderItem(this->property_id_to_column_idx(TreeItemPropertyID::Timestamp), header_item);
	this->view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::Timestamp, false, QObject::tr("Timestamp")));


	this->setModel(this->tree_model);
	this->expandAll();


	this->setSelectionMode(QAbstractItemView::SingleSelection);


	this->header()->setSectionResizeMode(this->property_id_to_column_idx(TreeItemPropertyID::Visibility), QHeaderView::ResizeToContents); /* This column holds only a checkbox, so let's limit its width to column label. */
	this->header()->setSectionHidden(this->property_id_to_column_idx(TreeItemPropertyID::TheItem), false);
	this->header()->setSectionHidden(this->property_id_to_column_idx(TreeItemPropertyID::Editable), true);
	this->header()->setSectionHidden(this->property_id_to_column_idx(TreeItemPropertyID::Timestamp), true);


	connect(this, SIGNAL (clicked(const QModelIndex &)), this, SLOT (tree_item_selected_cb(void)));
	connect(this->tree_model, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)), this, SLOT(data_changed_cb(const QModelIndex&, const QModelIndex&)));



	/* Drag & Drop. */
	this->setDragEnabled(true);
	this->setDropIndicatorShown(true);
	this->setAcceptDrops(true);
	this->setDragDropMode(QAbstractItemView::InternalMove);


#ifdef K_FIXME_RESTORE
	/* Can not specify 'auto' sort order with a 'GtkTreeSortable' on the name since we want to control the ordering of layers.
	   Thus need to create special sort to operate on a subsection of tree_view (i.e. from a specific child either a layer or sublayer).
	   See vik_tree_view_sort_children(). */

	gtk_tree_view_set_reorderable(this, true);
	QObject::connect(this->selectionModel(), SIGNAL("changed"), this, SLOT (tree_item_selected_cb(void)));
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
void TreeView::data_changed_cb(const QModelIndex & top_left, __attribute__((unused)) const QModelIndex & bottom_right)
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

	const int col = index->column();
	const TreeItemPropertyID property_id = this->column_idx_to_property_id(col);
	switch (property_id) {
	case TreeItemPropertyID::TheItem:
		if (item->text().isEmpty()) {
			qDebug() << SG_PREFIX_W << "Edited item in column Name: new name is empty, ignoring the change";
			/* We have to undo the action of setting empty text label. */
			item->setText(tree_item->get_name());
		} else {
			qDebug() << SG_PREFIX_I << "Edited item in column Name: new name is" << item->text();
			tree_item->set_name(item->text());
		}

		break;

	case TreeItemPropertyID::Visibility:
		qDebug() << SG_PREFIX_I << "Edited item in column Visible: is checkable?" << item->isCheckable();

		tree_item->set_visible(item->checkState());
		qDebug() << SG_PREFIX_SIGNAL << "Emitting tree_item_needs_redraw(), uid=", tree_item->get_uid();
		emit this->tree_item_needs_redraw(tree_item->get_uid());
		break;

	case TreeItemPropertyID::Editable:
		qDebug() << SG_PREFIX_W << "Edited item in column Editable";
		break;

	case TreeItemPropertyID::Timestamp:
		qDebug() << SG_PREFIX_W << "Edited item in column Timestamp";
		break;

	default:
		qDebug() << SG_PREFIX_E << "Edited item in unknown column" << (int) col;
		break;
	}

	return;
}




sg_ret TreeView::get_item_position(const TreeItem & item, bool & is_first, bool & is_last)
{
	QModelIndex parent_index = item.index.parent();
	if (!parent_index.isValid()) {
		qDebug() << SG_PREFIX_W << "Parent index is invalid. Function called for top level item?";
		return sg_ret::err;
	}

	QStandardItem * parent_item = this->tree_model->itemFromIndex(parent_index);

	const int n_rows = parent_item->rowCount();
	const int row = item.index.row();


	is_first = false;
	is_last = false;
	if (row == 0) {
		is_first = true;
	}
	if (row == n_rows - 1) {
		is_last = true;
	}

	/* Item may be first and last at the same time if it doesn't have any siblings. */

	qDebug() << SG_PREFIX_I << item.get_name() << "row =" << row << ", n_rows =" << n_rows << ", is_first =" << is_first << ", is_last =" << is_last;

	return sg_ret::ok;
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




QList<TreeItem *> get_tree_items(const QMimeData * mime_data)
{
	QList<TreeItem *> result;

	QByteArray mime_bytes = mime_data->data(MY_MIME_TYPE);
        QDataStream data_stream(&mime_bytes, QIODevice::ReadOnly);
	quint32 n_items;

        data_stream >> n_items;


	qDebug() << SG_PREFIX_I << "Mime formats" << mime_data->formats();
	qDebug() << SG_PREFIX_I << "Number of drag'n'drop items =" << n_items;
	for (quint32 i = 0; i < n_items; i++) {
		TreeItem * tree_item = NULL;
		data_stream >> tree_item;
		qDebug() << SG_PREFIX_I << "Dragged item's name =" << tree_item->get_name();
		result.push_back(tree_item);
	}

	return result;
}




bool TreeModel::canDropMimeData(const QMimeData * mime_data, Qt::DropAction action, int row, int column, const QModelIndex & parent_index)
{
	Q_UNUSED(action);

	/*
	  When dropping an item onto an existing TreeItem,
	  @parent_index will be the TreeItem's index.
	  Example: dropping a Waypoint on another Waypoint.
	  Example: dropping a Waypoint on Waypoints node.
	  Example: dropping a Waypoint on LayerTRW node.
	  Example: dropping a Waypoints node on LayerTRW node.
	  In this case @row == -1, @col == -1.



	  When dropping an item between two equally-nested siblings,
	  @parent_index will be the siblings' parent's index.  Call
	  parent_index.child(row, 0); to get index of sibling before
	  which the item will be dropped.

	  In this case @row is a zero-based index of target row, and
	  column indicates on which of view's column the item was
	  dropped. Most of the time the @column shouldn't matter and
	  zero can be used instead.

	  parent node
	      sibling1

	      sibling2
	                <--- dropping here will result in
	                     @parent_index pointing to 'parent node'
	                     and @parent_->child(row, 0) will return
	                     index of sibling3. @row == 2.
	      sibling3
	*/



	if (!mime_data->hasFormat(MY_MIME_TYPE)) {
		return false;
	}
	if (!parent_index.isValid()) {
		/* Don't let dropping items on top level. */
		return false;
	}


	TreeItem * parent_item = this->view->get_tree_item(parent_index);
	if (!parent_item) {
		qDebug() << SG_PREFIX_E << "Can't find parent item";
		return false;
	}



	qDebug() << SG_PREFIX_I << "Row =" << row << "col =" << column << "parent's name =" << parent_item->get_name();
	QList<TreeItem *> list = get_tree_items(mime_data);

	bool accepts_all = false; /* It's important to start from 'false'. */
	for (int i = 0; i < list.size(); i++) {
		TreeItem * tree_item = list.at(i);
		if (nullptr == tree_item) {
			qDebug() << SG_PREFIX_E << "Item" << i << "is NULL";
			return false;
		}
		if (!parent_item->dropped_item_is_acceptable(*tree_item)) {
			qDebug() << SG_PREFIX_I << "Can't drop MIME data: tree item doesn't accept child no." << i << "(type id mismatch)";
			accepts_all = false;
			break;
		}

		qDebug() << SG_PREFIX_I << "Can drop" << tree_item->m_type_id << "onto" << parent_item->m_type_id;
		accepts_all = true;
	}

	if (!accepts_all) {
		qDebug() << SG_PREFIX_I << "Can't drop MIME data: not all items accepted";
		return false;
	}

	return true;
}




/*
  http://doc.qt.io/qt-5/qabstractitemmodel.html#dropMimeData
*/
bool TreeModel::dropMimeData(const QMimeData * mime_data, Qt::DropAction action, int row, int column, const QModelIndex & parent_index)
{
	if (!canDropMimeData(mime_data, action, row, column, parent_index)) {
		qDebug() << SG_PREFIX_D << "Dropping this item on given target is not supported";
		return false;
	}

	if (action == Qt::IgnoreAction) {
		qDebug() << SG_PREFIX_D << "Ignore action";
		return true;
	}

	if (!parent_index.isValid()) {
		qDebug() << SG_PREFIX_E << "Invalid parent index";
		return false;
	}

	TreeItem * parent_item = this->view->get_tree_item(parent_index);
	if (NULL == parent_item) {
		qDebug() << SG_PREFIX_E << "Can't find parent item";
		return false;
	}

	QList<TreeItem *> list = get_tree_items(mime_data);

	if (row == -1 && column == -1) {
		/* Drop onto an item: push back to end of item's list of children. */
		qDebug() << SG_PREFIX_I << "Dropping items at the end of parent item" << parent_item->get_name();

		for (int i = 0; i < list.size(); i++) {
			TreeItem * tree_item = list.at(i);
			qDebug() << SG_PREFIX_I << "Dropping item" << tree_item->get_name() << "at the end of parent item" << parent_item->get_name();
			parent_item->drag_drop_request(tree_item, row, column);
		}

		return true;
	} else {
		/* Drop between some items: insert at position specified by row. */
		qDebug() << SG_PREFIX_I << "Dropping items as siblings with parent item" << parent_item->get_name();

		for (int i = 0; i < list.size(); i++) {
			TreeItem * tree_item = list.at(i);
			qDebug() << SG_PREFIX_I << "Dropping item" << tree_item->get_name() << "as sibling with parent item" << parent_item->get_name();
			parent_item->drag_drop_request(tree_item, row, column);
		}
		return true;
	}

#if 0
	QModelIndex drop_target_index = parent_index.child(row, 0);
	if (!drop_target_index.isValid()) {
		qDebug() << SG_PREFIX_W << "Can't get valid drop target index";
		return false;
	}

	TreeItem * target_item = this->view->get_tree_item(drop_target_index);
	if (target_item == NULL) {
		qDebug() << SG_PREFIX_W << "Can't get valid drop target item";
		return false;
	}
	qDebug() << SG_PREFIX_I << "Target item's name =" << target_item->name;


	if (parent_item->is_layer()) {
		qDebug() << SG_PREFIX_I << "Can drop on Layer";
		return true;
	} else {
		qDebug() << SG_PREFIX_I << "Can drop on Sublayer";
		return true;
	}

#endif
}




Qt::DropActions TreeModel::supportedDropActions() const
{
	return Qt::MoveAction;
}




QMimeData * TreeModel::mimeData(const QModelIndexList & indexes) const
{
	QList<TreeItem *> list;

	foreach (const QModelIndex &index, indexes) {
		if (!index.isValid()) {
			continue;
		}

		TreeItem * tree_item = this->view->get_tree_item(index);
		if (list.contains(tree_item)) {
			/* TODO_LATER: verify why when dragging a single
			   item the @indexes contains two copies of
			   index of dragged item.. */
			continue;
		}

		qDebug() << SG_PREFIX_I << "Pushing to list item with name =" << tree_item->get_name();
		list.push_back(tree_item);
	}


	QMimeData * mime_data = new QMimeData();
	QByteArray encoded_data;
	QDataStream stream(&encoded_data, QIODevice::WriteOnly);
	stream << list;

	qDebug() << SG_PREFIX_I << "Preparing mime data";
	mime_data->setData(MY_MIME_TYPE, encoded_data);
	return mime_data;
}




QStringList TreeModel::mimeTypes(void) const
{
	QStringList types;
	types << MY_MIME_TYPE;
	return types;
}




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
		Dialog::info(tr("This item has no configurable properties."), ThisApp::get_main_window());
		qDebug() << SG_PREFIX_I << "Selected item" << selected_item->m_type_id << "has no configurable properties";
		return true;
	}

	bool result = selected_item->show_properties_dialog();
	if (result) {
		selected_item->emit_tree_item_changed("Tree View - Item Properties");
		return true;
	}

	return false;
}




void SelectedTreeItems::add_to_set(TreeItem * tree_item)
{
	/* At this moment we support selection of only one item at a
	   time. So if anyone selects a new item, all other (old) selected
	   items must be forgotten. */
	this->selected_tree_items.clear();

	this->selected_tree_items.insert(std::pair<sg_uid_t, TreeItem *>(tree_item->get_uid(), tree_item));
}




bool SelectedTreeItems::remove_from_set(const TreeItem * tree_item)
{
	if (!tree_item) {
		return 0;
	}

	/* erase() returns number of removed items. */
	return 0 != this->selected_tree_items.erase(tree_item->get_uid());
}




bool SelectedTreeItems::is_in_set(const TreeItem * tree_item) const
{
	if (nullptr == tree_item) {
		return false;
	}

	return this->selected_tree_items.end() != this->selected_tree_items.find(tree_item->get_uid());
}




void SelectedTreeItems::clear(void)
{
	this->selected_tree_items.clear();
}




int SelectedTreeItems::size(void) const
{
	return this->selected_tree_items.size();
}




int TreeView::property_id_to_column_idx(TreeItemPropertyID property_id) const
{
	int col = 0;

	switch (property_id) {
	case TreeItemPropertyID::TheItem:
		col = 0;
		break;
	case TreeItemPropertyID::Visibility:
		col = 1;
		break;
	case TreeItemPropertyID::Editable:
		col = 2;
		break;
	case TreeItemPropertyID::Timestamp:
		col = 3;
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected property id" << (int) property_id;
		break;
	}

	return col;
}




TreeItemPropertyID TreeView::column_idx_to_property_id(int col) const
{
	TreeItemPropertyID property_id;
	switch (col) {
	case 0:
		property_id = TreeItemPropertyID::TheItem;
		break;
	case 1:
		property_id = TreeItemPropertyID::Visibility;
		break;
	case 2:
		property_id = TreeItemPropertyID::Editable;
		break;
	case 3:
		property_id = TreeItemPropertyID::Timestamp;
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected column" << col;
		break;
	}

	return property_id;
}




void SelectedTreeItems::print_draw_mode(const TreeItem & tree_item, bool parent_is_selected)
{
	if (1) {
		if (g_selected.is_in_set(&tree_item)) {
			qDebug() << SG_PREFIX_I << "Drawing tree item" << tree_item.get_name() << "as selected (selected directly)";
		} else if (parent_is_selected) {
			qDebug() << SG_PREFIX_I << "Drawing tree item" << tree_item.get_name() << "as selected (selected through parent)";
		} else {
			qDebug() << SG_PREFIX_I << "Drawing tree item" << tree_item.get_name() << "as non-selected";
		}
	}
}
