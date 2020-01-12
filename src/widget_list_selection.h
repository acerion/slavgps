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
 */

#ifndef _SG_WIDGET_LIST_SELECTION_H_
#define _SG_WIDGET_LIST_SELECTION_H_




#include <list>
#include <unistd.h>




#include <QList>
#include <QString>
#include <QStandardItemModel>
#include <QDialog>
#include <QVBoxLayout>
#include <QTableView>
#include <QDialogButtonBox>




#include "dialog.h"
#include "tree_view.h"




namespace SlavGPS {




	class Geoname;
	class Track;
	class Waypoint;




	enum class ListSelectionMode {
		SingleItem,       /* List selection dialog allows selection of only one item. */
		MultipleItems     /* List selection dialog allows selection of one or more items. */
	};




	class ListSelectionWidget : public QTableView {
		Q_OBJECT
	public:
		ListSelectionWidget() {};
		ListSelectionWidget(ListSelectionMode selection_mode, QWidget * a_parent = NULL);
		~ListSelectionWidget();

		void set_headers(const QStringList & header_labels);

		template <typename T>
		sg_ret add_elements(const std::list<T> & elements);

		template <typename T>
		std::list<T> get_selection(T dummy);

		QStandardItemModel model;

		static QStringList get_headers_for_track(void);
		static QStringList get_headers_for_waypoint(void);
		static QStringList get_headers_for_geoname(void);
		static QStringList get_headers_for_string(void);

	};




	/* The list selection row should present items in such a way,
	   as to be able to differentiate between items with the same
	   name.

	   E.g. two tracks with the same name can have different start
	   times or durations - this should be presented in the list
	   dialog to allow user recognize all tracks and decide which
	   ones to select. */
	class ListSelectionRow {
	public:
		ListSelectionRow();
		ListSelectionRow(Track * trk);
		ListSelectionRow(Waypoint * wp);
		ListSelectionRow(Geoname * geoname);
		ListSelectionRow(QString const & text);
		~ListSelectionRow() {};

		QList<QStandardItem *> items;
	};



	template <typename T>
	sg_ret ListSelectionWidget::add_elements(const std::list<T> & elements)
	{
		qDebug() << __FILE__ << __LINE__;

		for (auto iter = elements.begin(); iter != elements.end(); iter++) {
			SlavGPS::ListSelectionRow row(*iter);
			this->model.invisibleRootItem()->appendRow(row.items);
		}
#if 0
		qDebug() << __FILE__ << __LINE__;
		this->setVisible(false);
		qDebug() << __FILE__ << __LINE__;
		this->resizeRowsToContents();
		qDebug() << __FILE__ << __LINE__;
		this->resizeColumnsToContents();
		qDebug() << __FILE__ << __LINE__;
		this->setVisible(true);
		qDebug() << __FILE__ << __LINE__;
		this->show();
		qDebug() << __FILE__ << __LINE__;
#endif

		return sg_ret::ok;
	}



	template <typename T>
	std::list<T> ListSelectionWidget::get_selection(__attribute__((unused)) T dummy)
	{
		qDebug() << __FILE__ << __LINE__;

		std::list<T> result;


		/* Don't use selectedIndexes(), because this method
		   would return as many indexes per row as there are
		   columns. We only want one item in
		   'selected_indices' list per row. */
		QModelIndexList selected_indices = this->selectionModel()->selectedRows();

		qDebug() << __FILE__ << __LINE__;

		QStandardItem * root_item = this->model.invisibleRootItem();
		if (!root_item) {
			qDebug() << "EE   Widget List Selection  >  Failed to get root item";
			return result;
		}

		qDebug() << __FILE__ << __LINE__;

		for (auto iter = selected_indices.begin(); iter != selected_indices.end(); iter++) {

			qDebug() << __FILE__ << __LINE__ << "adding new item to list of selected items";

			/* Pointer to the object that we want to put
			   in result is in zero-th column. */

			QModelIndex item_index = *iter;
			QStandardItem * child_item = root_item->child(item_index.row(), 0);
			if (!child_item) {
				qDebug() << "EE   Widget List Selection  >  Failed to get child item from zero-th column";
				return result;
			}

			const QVariant variant = child_item->data(RoleLayerData);
			result.push_back(variant.value<T>());
		}


		qDebug() << __FILE__ << __LINE__;

		return result;
	}



	template <class T>
	class ListSelectionDialog : public BasicDialog {
	public:
		ListSelectionDialog(const QString & title, ListSelectionMode selection_mode, const QStringList & header_labels, QWidget * parent = NULL);

		void set_list(const std::list<T> & elements);
		std::list<T> get_selection(void) const;

		ListSelectionWidget * list_widget = NULL;
	};

	template <class T>
	ListSelectionDialog<T>::ListSelectionDialog(const QString & title, ListSelectionMode selection_mode, const QStringList & header_labels, QWidget * parent)
		: BasicDialog(title, parent)
	{
		this->list_widget = new ListSelectionWidget(selection_mode, this);
		this->list_widget->set_headers(header_labels);

		this->setMinimumHeight(400);
		this->grid->addWidget(this->list_widget, 0, 0);
	}


	template <class T>
	void ListSelectionDialog<T>::set_list(const std::list<T> & elements)
	{
		for (auto iter = elements.begin(); iter != elements.end(); iter++) {
			SlavGPS::ListSelectionRow row(*iter);
			this->list_widget->model.invisibleRootItem()->appendRow(row.items);
		}
		this->list_widget->setVisible(false);
		this->list_widget->resizeRowsToContents();
		this->list_widget->resizeColumnsToContents();
		this->list_widget->setVisible(true);
		this->list_widget->show();
	}


	template <class T>
	std::list<T> ListSelectionDialog<T>::get_selection(void) const
	{
		std::list<T> result;

		/* Don't use selectedIndexes(),
		   because this method would return as many
		   indexes per row as there are columns. We only
		   want one item in 'selected_indices' list per row. */
		QModelIndexList selected_indices = this->list_widget->selectionModel()->selectedRows();

		QStandardItem * root_item = this->list_widget->model.invisibleRootItem();
		if (!root_item) {
			qDebug() << "EE   Widget List Dialog  >  Failed to get root item";
			return result;
		}
		for (auto iter = selected_indices.begin(); iter != selected_indices.end(); iter++) {

			/* Pointer to the object that we want
			   to put in result is in zero-th column. */

			QModelIndex item_index = *iter;
			QStandardItem * child_item = root_item->child(item_index.row(), 0);
			if (!child_item) {
				qDebug() << "EE   Widget List Dialog  >  Failed to get child item from zero-th column";
				break;
			}

			const QVariant variant = child_item->data(RoleLayerData);
			result.push_back(variant.value<T>());
		}

		return result;
	}



	template <typename T>
	std::list<T> a_dialog_select_from_list(BasicDialog & dialog, const std::list<T> & elements, ListSelectionMode selection_mode, const QStringList & header_labels)
	{
		ListSelectionWidget list_widget(selection_mode, &dialog);
		list_widget.set_headers(header_labels);

		dialog.setMinimumHeight(400);
		dialog.grid->addWidget(&list_widget, 0, 0);

		for (auto iter = elements.begin(); iter != elements.end(); iter++) {
			SlavGPS::ListSelectionRow row(*iter);
			list_widget.model.invisibleRootItem()->appendRow(row.items);
		}
		list_widget.setVisible(false);
		list_widget.resizeRowsToContents();
		list_widget.resizeColumnsToContents();
		list_widget.setVisible(true);
		list_widget.show();


		std::list<T> result;
		if (dialog.exec() == QDialog::Accepted) {

			/* Don't use selectedIndexes(),
			   because this method would return as many
			   indexes per row as there are columns. We only
			   want one item in 'selected_indices' list per row. */
			QModelIndexList selected_indices = list_widget.selectionModel()->selectedRows();

			QStandardItem * root_item = list_widget.model.invisibleRootItem();
			if (!root_item) {
				qDebug() << "EE   Widget List Selection  >  Failed to get root item";
				return result;
			}
			for (auto iter = selected_indices.begin(); iter != selected_indices.end(); iter++) {

				/* Pointer to the object that we want
				   to put in result is in zero-th column. */

				QModelIndex item_index = *iter;
				QStandardItem * child_item = root_item->child(item_index.row(), 0);
				if (!child_item) {
					qDebug() << "EE   Widget List Selection  >  Failed to get child item from zero-th column";
					return result;
				}

				const QVariant variant = child_item->data(RoleLayerData);
				result.push_back(variant.value<T>());
			}
		}

		return result;
	}




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WIDGET_LIST_SELECTION_H_ */
