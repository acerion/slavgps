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
/* TODO: this is a duplicate definition. */
#define VIK_SETTINGS_SORTABLE_DATE_TIME_FORMAT "sortable_date_time_format"




extern Tree * g_tree;



#if 0
/* Instead of hooking automatically on table item selection,
   this is performed on demand via the specific context menu request. */
void TreeItemListDialog::tree_item_select(LayerTRW * layer)
{
	if (!this->selected_wp) {
		qDebug() << SG_PREFIX_E << "Encountered NULL TreeItem in callback" << __FUNCTION__;
		return;
	}

	TreeItem * wp = this->selected_wp;
	LayerTRW * trw = wp->get_parent_layer_trw();

	if (wp && trw) {
		trw->tree_view->select_and_expose_tree_item(wp);
	} else {
		qDebug() << SG_PREFIX_E << "Selecting either NULL layer or NULL wp:" << (qintptr) trw << (qintptr) wp;
	}
}
#endif



void TreeItemListDialog::tree_item_properties_cb(void) /* Slot. */
{
	if (!this->selected_wp) {
		qDebug() << SG_PREFIX_E << "Encountered NULL TreeItem in callback" << __FUNCTION__;
		return;
	}

#ifdef K_TODO
	TreeItem * wp = this->selected_wp;
	LayerTRW * trw = wp->get_parent_layer_trw();

	if (wp->name.isEmpty()) {
		return;
	}

	/* Close this dialog to allow interaction with properties window.
	   Since the properties also allows tree_item manipulations it won't cause conflicts here. */
	this->accept();

	const std::tuple<bool, bool> result = tree_item_properties_dialog(wp, wp->name, trw->get_coord_mode(), g_tree->tree_get_main_window());
	if (std::get<SG_WP_DIALOG_OK>(result)) { /* "OK" pressed in dialog, tree_item's parameters entered in the dialog are valid. */

		if (std::get<SG_WP_DIALOG_NAME>(result)) {
			/* TreeItem's name has been changed. */
			trw->get_tree_items_node().propagate_new_tree_item_name(wp);
		}

		trw->get_tree_items_node().set_new_tree_item_icon(wp);

		if (trw->visible) {
			trw->emit_layer_changed("TRW - TreeItem List Dialog - properties");
		}
	}
#endif
}




void TreeItemListDialog::tree_item_view_cb(void) /* Slot. */
{
	if (!this->selected_wp) {
		qDebug() << SG_PREFIX_E << "Encountered NULL TreeItem in callback" << __FUNCTION__;
		return;
	}
#ifdef K_TODO
	TreeItem * wp = this->selected_wp;
	LayerTRW * trw = wp->get_parent_layer_trw();
	Viewport * viewport = g_tree->tree_get_main_viewport();

	viewport->set_center_from_coord(wp->coord, true);
	///this->tree_item_select(trw);
	trw->emit_layer_changed("TRW - TreeItem List Dialog - View");
#endif
}




void TreeItemListDialog::show_picture_tree_item_cb(void) /* Slot. */
{
	if (!this->selected_wp) {
		qDebug() << SG_PREFIX_E << "Encountered NULL TreeItem in callback" << __FUNCTION__;
		return;
	}
#ifdef K_TODO
	TreeItem * wp = this->selected_wp;
	LayerTRW * trw = wp->get_parent_layer_trw();

	const QString viewer = Preferences::get_image_viewer();
	const QString quoted_path = Util::shell_quote(wp->image_full_path);
	const QString command = QString("%1 %2").arg(viewer).arg(quoted_path);

	if (!QProcess::startDetached(command)) {
		Dialog::error(QObject::QObject::tr("Could not launch viewer program '%1' to view file '%2'.").arg(viewer).arg(quoted_path), trw->get_window());
	}
#endif
}




void TreeItemListDialog::contextMenuEvent(QContextMenuEvent * ev)
{
	QPoint orig = ev->pos();
	QPoint v = this->view->pos();
	QPoint t = this->view->viewport()->pos();

	orig.setX(orig.x() - v.x() - t.x());
	orig.setY(orig.y() - v.y() - t.y());

	QPoint point = orig;
	QModelIndex index = this->view->indexAt(point);
	if (!index.isValid()) {
		qDebug() << SG_PREFIX_I << "INvalid index";
		return;
	} else {
		qDebug() << SG_PREFIX_I << "On index.row =" << index.row() << "index.column =" << index.column();
	}


	QStandardItem * parent_item = this->model->invisibleRootItem();


	QStandardItem * child = parent_item->child(index.row(), TreeItemListColumnID::TheItem);
	qDebug() << SG_PREFIX_I << "Selected tree_item" << child->text();

	child = parent_item->child(index.row(), TreeItemListColumnID::TheItem);
	TreeItem * wp = child->data(RoleLayerData).value<TreeItem *>();
	if (!wp) {
		qDebug() << SG_PREFIX_E << "Failed to get non-NULL TreeItem from table";
		return;
	}
#ifdef K_TODO
	/* If we were able to get list of TreeItems, all of them need to have associated parent layer. */
	LayerTRW * trw = wp->get_parent_layer_trw();
	if (!trw) {
		qDebug() << SG_PREFIX_E << "Failed to get non-NULL parent layer";
		return;
	}

	this->selected_wp = wp;



	QAction * qa = NULL;
	QMenu menu(this);
	/* When multiple rows are selected, the number of applicable operation is lower. */
	QItemSelectionModel * selection = this->view->selectionModel();
	if (selection->selectedRows(0).size() == 1) {

		qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), tr("&Zoom onto"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (tree_item_view_cb()));

		qa = menu.addAction(QIcon::fromTheme("document-properties"), tr("&Properties"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (tree_item_properties_cb()));

		qa = menu.addAction(QIcon::fromTheme("vik-icon-Show Picture"), tr("&Show Picture..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (show_picture_tree_item_cb()));
		qa->setEnabled(!wp->image_full_path.isEmpty());
	}

	qa = menu.addAction(QIcon::fromTheme("edit-copy"), tr("&Copy Data"));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (copy_selected_only_visible_columns_cb()));

	qa = menu.addAction(QIcon::fromTheme("edit-copy"), tr("Copy Data (with &positions)"));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (copy_selected_with_position_cb()));


	menu.exec(QCursor::pos());
#endif
	return;
}




/**
 * @hide_layer_names:  Don't show the layer names (first column of list) that each tree_item belongs to
 *
 * Create a table of tree_items with corresponding tree_item information.
 * This table does not support being actively updated.
 */
void TreeItemListDialog::build_model(const TreeItemListFormat & list_format, bool hide_layer_names)
{
	if (this->tree_items.empty()) {
		return;
	}



	this->model = new QStandardItemModel();

	for (unsigned int i = 0; i < list_format.columns.size(); i++) {
		this->model->setHorizontalHeaderItem(i, new QStandardItem(list_format.columns[i].header_label));
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


	for (unsigned int i = 0; i < list_format.columns.size(); i++) {
		this->model->setHorizontalHeaderItem(i, new QStandardItem(list_format.columns[i].header_label));

		this->view->horizontalHeader()->setSectionHidden(i, !list_format.columns[i].visible);
		this->view->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Interactive);
#ifdef K_TODO
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
		const QList<QStandardItem *> items = (*iter)->get_list_representation(list_format);
		this->model->invisibleRootItem()->appendRow(items);
	}

#ifdef K_TODO
	if (hide_layer_names) {
		this->view->sortByColumn(TreeItemListColumnID::TheItem, Qt::SortOrder::AscendingOrder);
	} else {
		this->view->sortByColumn(TreeItemListColumnID::ParentLayer, Qt::SortOrder::AscendingOrder);
	}
#endif


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
void TreeItemListDialog::show_dialog(QString const & title, const TreeItemListFormat & list_format, const std::list<TreeItem *> & items, Layer * layer)
{
	TreeItemListDialog dialog(title, layer->get_window());

	dialog.tree_items.clear();
#ifdef K_TODO
	if (layer->type == LayerType::TRW) {
		((LayerTRW *) layer)->get_tree_items_list(dialog.tree_items);
	} else if (layer->type == LayerType::Aggregate) {
		((LayerAggregate *) layer)->get_tree_items_list(dialog.tree_items);
	} else {
		assert (0);
	}
#else
	dialog.tree_items = items;
#endif

	dialog.build_model(list_format, layer->type != LayerType::Aggregate);
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
#ifdef K_TODO
	if (this->selected_wp) {
		LayerTRW * trw = this->selected_wp->get_parent_layer_trw();
		trw->tree_items.update_tree_view(this->selected_wp);
		trw->emit_layer_changed("TRW - TreeItem List Dialog - Accept");
	}
#endif

	this->accept();
}




void TreeItemListModel::sort(int column, Qt::SortOrder order)
{
	if (column == TreeItemListColumnID::Icon) {
		return;
	}

	QStandardItemModel::sort(column, order);
}
