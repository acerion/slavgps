/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2019, Kamil Ignacak <acerion@wp.pl>
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

#ifndef _SG_TREE_ITEM_LIST_H_
#define _SG_TREE_ITEM_LIST_H_




#include <list>
#include <vector>




#include <QWidget>
#include <QDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QStandardItemModel>
#include <QTableView>
#include <QHeaderView>
#include <QContextMenuEvent>




#include "application_state.h"
#include "measurements.h"
#include "tree_item.h"




namespace SlavGPS {




	class TreeItem;
	class Viewport;
	class TreeItem;




	class TreeItemListModel : public QStandardItemModel {
		Q_OBJECT

	public:
		TreeItemListModel(QObject * parent = NULL) : QStandardItemModel(parent) {};
		void sort(int column, Qt::SortOrder order = Qt::AscendingOrder);
	};




	class TreeItemListDialog : public QDialog {
		Q_OBJECT
	public:
		TreeItemListDialog(QString const & title, QWidget * parent_widget);
		~TreeItemListDialog();

		TreeItemViewFormat view_format;



		void contextMenuEvent(QContextMenuEvent * event);

		int column_id_to_column_idx(TreeItemPropertyID column_id);

		QWidget * parent = NULL;
		QDialogButtonBox * button_box = NULL;
		QVBoxLayout * vbox = NULL;

		QStandardItemModel * model = NULL;
		QTableView * view = NULL;

		/* TreeItem selected in list. */
		TreeItem * selected_tree_item = NULL;


		Qt::DateFormat date_time_format = Qt::ISODate;

	private slots:
		void accept_cb(void);
	};




	/* Helper class is needed because Q_OBJECT and templates don't work well together. */
	template <class T>
	class TreeItemListDialogHelper {
	public:
		/**
		   Common method for showing a list of tree items with extended information
		*/
		void show_dialog(QString const & title, const TreeItemViewFormat & view_format, const std::list<T> & items, QWidget * parent = NULL);

	private:
		void build_model(void);
		void build_view(void);

		std::list<T> tree_items;

		TreeItemListDialog * dialog = NULL;
	};




	template <class T>
	void TreeItemListDialogHelper<T>::show_dialog(QString const & title, const TreeItemViewFormat & view_format, const std::list<T> & items, QWidget * parent)
	{
		if (items.empty()) {
			return;
		}
		this->tree_items = items;

		this->dialog = new TreeItemListDialog(title, parent);
		this->dialog->view_format = view_format;

		this->build_model();
		this->build_view();

		this->dialog->exec();

		delete this->dialog;
	}




	/**
	   Create a table of tree_items with corresponding tree_item information.
	   This table does not support being actively updated.
	*/
	template <class T>
	void TreeItemListDialogHelper<T>::build_model(void)
	{
		this->dialog->model = new QStandardItemModel();

		for (unsigned int i = 0; i < this->dialog->view_format.columns.size(); i++) {
			this->dialog->model->setHorizontalHeaderItem(i, new QStandardItem(this->dialog->view_format.columns[i].header_label));
		}
	}




	template <class T>
	void TreeItemListDialogHelper<T>::build_view(void)
	{
		this->dialog->view = new QTableView();
		this->dialog->view->horizontalHeader()->setStretchLastSection(false);
		this->dialog->view->verticalHeader()->setVisible(false);
		this->dialog->view->setWordWrap(false);
		this->dialog->view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
		this->dialog->view->setTextElideMode(Qt::ElideRight);
		this->dialog->view->setSelectionMode(QAbstractItemView::ExtendedSelection);
		this->dialog->view->setSelectionBehavior(QAbstractItemView::SelectRows);
		this->dialog->view->setShowGrid(false);
		this->dialog->view->setModel(this->dialog->model);
		this->dialog->view->setSortingEnabled(true);


		for (unsigned int i = 0; i < this->dialog->view_format.columns.size(); i++) {
			this->dialog->model->setHorizontalHeaderItem(i, new QStandardItem(this->dialog->view_format.columns[i].header_label));

			this->dialog->view->horizontalHeader()->setSectionHidden(i, !this->dialog->view_format.columns[i].visible);
			this->dialog->view->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Interactive);
#ifdef TODO_LATER
			this->dialog->view->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
			this->dialog->view->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
#endif
		}


		this->dialog->vbox->addWidget(this->dialog->view);
		this->dialog->vbox->addWidget(this->dialog->button_box);

		QLayout * old = this->dialog->layout();
		delete old;
		this->dialog->setLayout(this->dialog->vbox);

		QObject::connect(this->dialog->button_box, SIGNAL (accepted()), this->dialog, SLOT (accept()));



		/* Set this member before adding rows to the table. */
		Qt::DateFormat dt_format;
		if (ApplicationState::get_integer(VIK_SETTINGS_SORTABLE_DATE_TIME_FORMAT, (int *) &dt_format)) {
			this->dialog->date_time_format = dt_format;
		}



		for (auto iter = this->tree_items.begin(); iter != this->tree_items.end(); iter++) {
			const QList<QStandardItem *> items = (*iter)->get_list_representation(this->dialog->view_format);
			this->dialog->model->invisibleRootItem()->appendRow(items);
		}
		this->dialog->view->sortByColumn(TreeItemPropertyID::ParentLayer, Qt::SortOrder::AscendingOrder);


		this->dialog->setMinimumSize(700, 400);


		this->dialog->view->show();

		this->dialog->view->setVisible(false);
		this->dialog->view->resizeRowsToContents();
		this->dialog->view->resizeColumnsToContents();
		this->dialog->view->setVisible(true);
	}




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TREE_ITEM_LIST_H_ */
