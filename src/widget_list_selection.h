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




	void a_dialog_select_from_list_prepare(QDialog & dialog, QStandardItemModel & model, QTableView & view, QVBoxLayout & vbox, QDialogButtonBox & button_box, bool multiple_selection_allowed, QString const & title, QString const & msg);

	template <typename T>
	std::list<T> a_dialog_select_from_list(std::list<T> const & elements, bool multiple_selection_allowed, QString const & title, QString const & msg, QWidget * parent)
	{
		QDialog dialog(parent);
		QDialogButtonBox button_box(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
		QStandardItemModel model;
		QTableView view;
		QVBoxLayout vbox;
		QItemSelectionModel selection_model(&model);

		a_dialog_select_from_list_prepare(dialog, model, view, vbox, button_box, multiple_selection_allowed, title, msg);
		view.setSelectionModel(&selection_model);

		for (auto iter = elements.begin(); iter != elements.end(); iter++) {
			SlavGPS::SGItem * item = new SlavGPS::SGItem(*iter);
			item->setEditable(false);
			model.invisibleRootItem()->appendRow(item);
		}
		view.setVisible(false);
		view.resizeRowsToContents();
		view.resizeColumnsToContents();
		view.setVisible(true);
		view.show();


		std::list<T> result;
		if (dialog.exec() == QDialog::Accepted) {
			QModelIndexList selected = selection_model.selectedIndexes();
			for (auto iter = selected.begin(); iter != selected.end(); iter++) {
				QVariant variant = model.itemFromIndex(*iter)->data();
				result.push_back(variant.value<T>());
			}
		}

		return result;
	}




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WIDGET_LIST_SELECTION_H_ */
