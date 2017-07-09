/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2008, Hein Ragas <viking@ragas.nl>
 * Copyright (C) 2010-2014, Rob Norris <rw_norris@hotmail.com>
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




#include <QHeaderView>

#include "widget_list_selection.h"




using namespace SlavGPS;




void SlavGPS::a_dialog_select_from_list_prepare(QDialog & dialog, QStandardItemModel & model, QTableView & view, QVBoxLayout & vbox, QDialogButtonBox & button_box, bool multiple_selection_allowed, QString const & title, QString const & msg)
{
	dialog.setWindowTitle(title);
	dialog.setMinimumHeight(400);

	QObject::connect(&button_box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	QObject::connect(&button_box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

	model.setHorizontalHeaderItem(0, new QStandardItem(msg));
	model.setItemPrototype(new SGItem());

	view.horizontalHeader()->setStretchLastSection(true);
	view.verticalHeader()->setVisible(false);
	view.setWordWrap(false);
	view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	view.setTextElideMode(Qt::ElideRight);
	if (multiple_selection_allowed) {
		view.setSelectionMode(QAbstractItemView::ExtendedSelection);
	} else {
		view.setSelectionMode(QAbstractItemView::SingleSelection);
	}
	view.setShowGrid(false);
	view.setModel(&model);

	view.horizontalHeader()->setSectionHidden(0, false);
	view.horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);

	vbox.addWidget(&view);
	vbox.addWidget(&button_box);

	QLayout * old = dialog.layout();
	delete old;
	dialog.setLayout(&vbox);
}
