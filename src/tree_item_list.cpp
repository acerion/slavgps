/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013-2015, Rob Norris <rw_norris@hotmail.com>
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




#include <cmath>
#include <cassert>




#include <QMenu>
#include <QDebug>
#include <QHeaderView>
#include <QDateTime>
#include <QProcess>




#include "layer.h"
#include "tree_item_list.h"
#include "layer_aggregate.h"
#include "window.h"
#include "viewport_internal.h"
#include "util.h"
#include "application_state.h"
#include "preferences.h"
#include "tree_view_internal.h"
#include "dialog.h"





using namespace SlavGPS;




#define SG_MODULE "TreeItem List"




void show_context_menu(TreeItem * item, const QPoint & cursor_position)
{
	if (!item) {
		qDebug() << SG_PREFIX_E << "Show context menu for item: NULL item";
		return;
	}

	QMenu menu;

	if (item->tree_item_type == TreeItemType::Layer) {

		qDebug() << SG_PREFIX_I << "Menu for layer tree item" << item->type_id << item->name;

		/* We don't want a parent layer here. We want item
		   cast to layer if the item is layer, or item's
		   parent layer otherwise. */
		Layer * layer = item->to_layer();

		/* "New layer -> layer types" submenu. */
		TreeItem::MenuOperation operations = layer->get_menu_operation_ids();
		operations = operator_bit_or(operations, TreeItem::MenuOperation::New);
		//this->context_menu_add_standard_items(&menu, operations);

		/* Layer-type-specific menu items. */
		layer->add_menu_items(menu);
	} else {
		qDebug() << SG_PREFIX_I << "Menu for non-layer tree item" << item->type_id << item->name;


		if (!item->add_context_menu_items(menu, true)) {
			return;
		}
		/* TODO_LATER: specific things for different types. */
	}

	menu.exec(cursor_position);

	return;
}



void TreeItemListDialog::contextMenuEvent(QContextMenuEvent * ev)
{
	if (!this->view->geometry().contains(ev->pos())) {
		qDebug() << SG_PREFIX_W << "context menu event outside list view";
		/* We want to handle only events that happen inside of list view. */
		return;
	}
	qDebug() << SG_PREFIX_I << "context menu event inside list view";

	QPoint orig = ev->pos();
	QPoint v = this->view->pos();
	QPoint t = this->view->viewport()->pos();

	qDebug() << SG_PREFIX_D << "Event @" << orig.x() << orig.y();
	qDebug() << SG_PREFIX_D << "Viewport @" << v;
	qDebug() << SG_PREFIX_D << "Tree view @" << t;

	QPoint point;
	point.setX(orig.x() - v.x() - t.x());
	point.setY(orig.y() - v.y() - t.y());

	QModelIndex item_index = this->view->indexAt(point);

	if (!item_index.isValid()) {
		/* We have clicked on empty space, not on tree
		   item. Not an error, user simply missed a row. */
		qDebug() << SG_PREFIX_I << "INvalid index";
		return;
	}

	/* We have clicked on some valid tree item. */
	qDebug() << SG_PREFIX_I << "Item index row =" << item_index.row() << ", item index column =" << item_index.column();


	QStandardItem * root_item = this->model->invisibleRootItem();
	if (!root_item) {
		qDebug() << SG_PREFIX_E << "Failed to get root item";
		return;
	}

	const int column_idx = this->column_id_to_column_idx(TreeItemPropertyID::TheItem);
	if (column_idx < 0) {
		qDebug() << SG_PREFIX_E << "Calculated column index is out of range:" << column_idx;
		return;
	}


	QStandardItem * child_item = root_item->child(item_index.row(), column_idx);
	if (!child_item) {
		qDebug() << SG_PREFIX_E << "Failed to get child item from column no." << column_idx;
		return;
	}

	qDebug() << SG_PREFIX_I << "Selected cell" << child_item->text();

	TreeItem * tree_item = child_item->data(RoleLayerData).value<TreeItem *>();
	if (!tree_item) {
		qDebug() << SG_PREFIX_E << "Failed to get non-NULL TreeItem from item" << child_item->text() << "at column id" << TreeItemPropertyID::TheItem;
		return;
	}

	show_context_menu(tree_item, QCursor::pos());

	return;
}




int TreeItemListDialog::column_id_to_column_idx(TreeItemPropertyID column_id)
{
	for (unsigned int i = 0; i < this->list_format.columns.size(); i++) {
		if (this->list_format.columns[i].id == column_id) {
			return (int) i;
		}
	}
	return -1;
}




/**
   Create a table of tree_items with corresponding tree_item information.
   This table does not support being actively updated.
*/
void TreeItemListDialog::build_model(const TreeItemListFormat & new_list_format)
{
	if (this->tree_items.empty()) {
		return;
	}

	this->list_format = new_list_format;

	this->model = new QStandardItemModel();

	for (unsigned int i = 0; i < this->list_format.columns.size(); i++) {
		this->model->setHorizontalHeaderItem(i, new QStandardItem(this->list_format.columns[i].header_label));
	}


	this->view = new QTableView();
	this->view->horizontalHeader()->setStretchLastSection(false);
	this->view->verticalHeader()->setVisible(false);
	this->view->setWordWrap(false);
	this->view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	this->view->setTextElideMode(Qt::ElideRight);
	this->view->setSelectionMode(QAbstractItemView::ExtendedSelection);
	this->view->setSelectionBehavior(QAbstractItemView::SelectRows);
	this->view->setShowGrid(false);
	this->view->setModel(this->model);
	this->view->setSortingEnabled(true);


	for (unsigned int i = 0; i < this->list_format.columns.size(); i++) {
		this->model->setHorizontalHeaderItem(i, new QStandardItem(this->list_format.columns[i].header_label));

		this->view->horizontalHeader()->setSectionHidden(i, !this->list_format.columns[i].visible);
		this->view->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Interactive);
#ifdef TODO_LATER
		this->view->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
		this->view->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
#endif
	}


	this->vbox->addWidget(this->view);
	this->vbox->addWidget(this->button_box);

	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox);

	connect(this->button_box, SIGNAL(accepted()), this, SLOT(accept()));



	/* Set this member before adding rows to the table. */
	Qt::DateFormat dt_format;
	if (ApplicationState::get_integer(VIK_SETTINGS_SORTABLE_DATE_TIME_FORMAT, (int *) &dt_format)) {
		this->date_time_format = dt_format;
	}



	for (auto iter = tree_items.begin(); iter != tree_items.end(); iter++) {
		const QList<QStandardItem *> items = (*iter)->get_list_representation(this->list_format);
		this->model->invisibleRootItem()->appendRow(items);
	}
	this->view->sortByColumn(TreeItemPropertyID::ParentLayer, Qt::SortOrder::AscendingOrder);


	this->setMinimumSize(700, 400);


	this->view->show();

	this->view->setVisible(false);
	this->view->resizeRowsToContents();
	this->view->resizeColumnsToContents();
	this->view->setVisible(true);
}




/**
   @title: the title for the dialog
   @layer: the layer, from which a list of tree_items should be extracted

   Common method for showing a list of tree_items with extended information
*/
void TreeItemListDialog::show_dialog(QString const & title, const TreeItemListFormat & new_list_format, const std::list<TreeItem *> & items, QWidget * parent)
{
	TreeItemListDialog dialog(title, parent);

	dialog.tree_items = items;

	dialog.build_model(new_list_format);
	dialog.exec();
}




TreeItemListDialog::TreeItemListDialog(QString const & title, QWidget * parent_widget) : QDialog(parent_widget)
{
	this->setWindowTitle(title);

	this->button_box = new QDialogButtonBox();
	this->parent = parent_widget;
	this->button_box->addButton("&Close", QDialogButtonBox::AcceptRole);
	connect(this->button_box, &QDialogButtonBox::accepted, this, &TreeItemListDialog::accept_cb);
	this->vbox = new QVBoxLayout;
}




TreeItemListDialog::~TreeItemListDialog()
{
}




/* Here we save in track objects changes made in the dialog. */
void TreeItemListDialog::accept_cb(void) /* Slot. */
{
	/* FIXME: check and make sure the tree_item still exists before doing anything to it. */
#ifdef TODO_LATER
	if (this->selected_tree_item) {
		Layer * parent_layer = this->selected_tree_item->get_parent_layer();
		if (parent_layer) {
			this->selected_tree_item->update_tree_item_properties();
			parent_layer->emit_tree_item_changed("TRW - TreeItem List Dialog - Accept");
		}
	}
#endif

	this->accept();
}




void TreeItemListModel::sort(int column, Qt::SortOrder order)
{
	if (column == TreeItemPropertyID::Icon) {
		return;
	}

	QStandardItemModel::sort(column, order);
}




TreeItemListFormat & TreeItemListFormat::operator=(const TreeItemListFormat & other)
{
	if (&other == this) {
		return *this;
	}

	for (unsigned int i = 0; i < other.columns.size(); i++) {
		this->columns.push_back(TreeItemListColumn(other.columns[i].id,
							   other.columns[i].visible,
							   other.columns[i].header_label));
	}

	return *this;
}
