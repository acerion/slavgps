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




#include <QObject>
#include <QDebug>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QLabel>
#include <QComboBox>




#include "layer_trw_dialogs.h"
#include "dialog.h"
#include "viewport_zoom.h"




using namespace SlavGPS;




QString SlavGPS::a_dialog_new_track(QString const & default_name, bool is_route, QWidget * parent)
{
	QString text;
	bool ok;
	do {
		text = QInputDialog::getText(parent,
					     is_route ? QObject::tr("Add Route") : QObject::tr("Add Track"),
					     is_route ? QObject::tr("Route Name:") : QObject::tr("Track Name:"),
					     QLineEdit::Normal,
					     QString(default_name), &ok);

		if (ok && text.isEmpty()) {
			QMessageBox::information(parent,
						 is_route ? QObject::tr("Route Name") : QObject::tr("Track Name"),
						 is_route ? QObject::tr("Please enter a name for the route.") : QObject::tr("Please enter a name for the track."));
		}

	} while (ok && text.isEmpty());

	return text;
}




MapAndZoomDialog::MapAndZoomDialog(const QString & title, const QStringList & map_labels, const std::vector<VikingScale> & viking_scales, QWidget * parent) : BasicDialog(parent)
{
	this->setWindowTitle(title);



	QLabel * map_label = new QLabel(tr("Map type:"));
	this->map_combo = new QComboBox();
	for (int i = 0; i < map_labels.size(); i++) {
		this->map_combo->addItem(map_labels.at(i));
	}



	QLabel * zoom_label = new QLabel(tr("Zoom level:"));
	this->zoom_combo = new QComboBox();
	for (unsigned int i = 0; i < viking_scales.size(); i++) {
		this->zoom_combo->addItem(viking_scales[i].to_string());
	}



	this->grid->addWidget(map_label, 0, 0);
	this->grid->addWidget(this->map_combo, 0, 1);
	this->grid->addWidget(zoom_label, 1, 0);
	this->grid->addWidget(this->zoom_combo, 1, 1);
}




void MapAndZoomDialog::preselect(int map_idx, int zoom_idx)
{
	this->map_combo->setCurrentIndex(map_idx);
	this->zoom_combo->setCurrentIndex(zoom_idx);
}




int MapAndZoomDialog::get_map_idx(void) const
{
	return this->map_combo->currentIndex();
}




int MapAndZoomDialog::get_zoom_idx(void) const
{
	return this->zoom_combo->currentIndex();
}
