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

#include <QString>
#include <QStandardItemModel>
#include <QDialog>
#include <QVBoxLayout>
#include <QTableView>
#include <QDialogButtonBox>

#include "ui_util.h"




namespace SlavGPS {




	class SGListSelection : public QTableView {
		Q_OBJECT
	public:
		SGListSelection() {};
		SGListSelection(bool multiple_selection, QWidget * a_parent = NULL);

		void set_headers(const QStringList & header_labels);

		QStandardItemModel model;
		QItemSelectionModel selection_model;
	};


	template <typename T>
	std::list<T> a_dialog_select_from_list(std::list<T> const & elements, bool multiple_selection, const QString & title, const QStringList & header_labels, QWidget * parent)
	{
		QDialog dialog(parent);
		SGListSelection view(multiple_selection, &dialog);
		view.set_headers(header_labels);
		QDialogButtonBox button_box(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
		QVBoxLayout vbox;

		dialog.setWindowTitle(title);
		dialog.setMinimumHeight(400);

		QObject::connect(&button_box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
		QObject::connect(&button_box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

		vbox.addWidget(&view);
		vbox.addWidget(&button_box);

		QLayout * old = dialog.layout();
		delete old;
		dialog.setLayout(&vbox);

		for (auto iter = elements.begin(); iter != elements.end(); iter++) {
			SlavGPS::SGItem * item = new SlavGPS::SGItem(*iter);
			item->setEditable(false);
			view.model.invisibleRootItem()->appendRow(item);
		}
		view.setVisible(false);
		view.resizeRowsToContents();
		view.resizeColumnsToContents();
		view.setVisible(true);
		view.show();


		std::list<T> result;
		if (dialog.exec() == QDialog::Accepted) {
			QModelIndexList selected = view.selection_model.selectedIndexes();
			for (auto iter = selected.begin(); iter != selected.end(); iter++) {
				QVariant variant = view.model.itemFromIndex(*iter)->data();
				result.push_back(variant.value<T>());
			}
		}

		return result;
	}




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WIDGET_LIST_SELECTION_H_ */
