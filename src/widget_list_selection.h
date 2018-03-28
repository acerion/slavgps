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




#include <QList>
#include <QString>
#include <QStandardItemModel>
#include <QDialog>
#include <QVBoxLayout>
#include <QTableView>
#include <QDialogButtonBox>




#include "dialog.h"




namespace SlavGPS {




	class Geoname;
	class Track;
	class Waypoint;




	enum class ListSelectionMode {
		SingleItem,       /* List selection dialog allows selection of only one item. */
		MultipleItems     /* List selection dialog allows selection of one or more items. */
	};




	class SGListSelection : public QTableView {
		Q_OBJECT
	public:
		SGListSelection() {};
		SGListSelection(ListSelectionMode selection_mode, QWidget * a_parent = NULL);

		void set_headers(const QStringList & header_labels);

		QStandardItemModel model;
		QItemSelectionModel selection_model;

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
	std::list<T> a_dialog_select_from_list(const std::list<T> & elements, ListSelectionMode selection_mode, const QString & title, const QStringList & header_labels, QWidget * parent)
	{
		BasicDialog dialog(parent);
		SGListSelection list_widget(selection_mode, &dialog);
		list_widget.set_headers(header_labels);

		dialog.setWindowTitle(title);
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
			QModelIndexList selected = list_widget.selection_model.selectedIndexes();
			for (auto iter = selected.begin(); iter != selected.end(); iter++) {
				QVariant variant = list_widget.model.itemFromIndex(*iter)->data();
				result.push_back(variant.value<T>());
			}
		}

		return result;
	}




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WIDGET_LIST_SELECTION_H_ */
