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




TimeThresholdDialog::TimeThresholdDialog(const QString & title, const QString & label, uint32_t custom_threshold, QWidget * a_parent)
{
	this->setWindowTitle(title);


	this->vbox = new QVBoxLayout();


	QLabel main_label(label);


	std::vector<SGLabelID> items;
	items.push_back(SGLabelID(QObject::tr("1 min"), 0));
	items.push_back(SGLabelID(QObject::tr("1 hour"), 1));
	items.push_back(SGLabelID(QObject::tr("1 day"), 2));
	items.push_back(SGLabelID(QObject::tr("Custom (in minutes):"), 3));
	this->radio_group = new RadioGroupWidget("", &items, NULL); /* This widget will be deleted by its parent Qt layout. */


	this->custom_spin.setMinimum(1); /* [minutes] */
	this->custom_spin.setMaximum(60 * 24 * 366); /* [minutes] */
	this->custom_spin.setValue(custom_threshold);
	this->custom_spin.setSingleStep(1);


	this->vbox->addWidget(&main_label);
	this->vbox->addWidget(this->radio_group);
	this->vbox->addWidget(&this->custom_spin);
	this->vbox->addWidget(&this->button_box);

	QObject::connect(&this->custom_spin, SIGNAL (valueChanged(int)), this, SLOT (spin_changed_cb(int)));

	this->button_box.addButton(QDialogButtonBox::Ok);
	this->button_box.addButton(QDialogButtonBox::Cancel);
	QObject::connect(&this->button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
	QObject::connect(&this->button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);


	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox); /* setLayout takes ownership of vbox. */
}



void TimeThresholdDialog::get_value(uint32_t * custom_threshold)
{
	const int selection = this->radio_group->get_id_of_selected();
	switch (selection) {
	case 0:
		*custom_threshold = 1;
		break;
	case 1:
		*custom_threshold = 60;
		break;
	case 2:
		*custom_threshold = 60 * 24;
		break;
	case 3:
		*custom_threshold = (uint32_t) this->custom_spin.value();
		break;
	default:
		qDebug() << "EE: Dialog: Time Threshold Dialog: invalid selection value" << selection;
		break;
	}
}



void TimeThresholdDialog::spin_changed_cb(__attribute__((unused)) int new_value)
{
	/* Enable "custom value" checkbox. */
	this->radio_group->set_id_of_selected(3);
}




bool SlavGPS::a_dialog_time_threshold(const QString & title, const QString & label, uint32_t * thr, QWidget * parent)
{
	TimeThresholdDialog dialog(title, label, *thr, parent);

	if (QDialog::Accepted == dialog.exec()) {
		dialog.get_value(thr);
		/* There is something strange about argument to qSetRealNumberPrecision().  The precision for
		   fractional part is not enough, I had to add few places for leading digits and decimal dot. */
		qDebug() << "DD: Dialog: Time Threshold Dialog: Saving time threshold as" << *thr;
		return true;
	} else {
		return false;
	}
}






MapAndZoomDialog::MapAndZoomDialog(const QString & title, const QStringList & map_labels, const std::vector<VikingZoomLevel> & viking_zoom_levels, QWidget * parent) : BasicDialog(parent)
{
	this->setWindowTitle(title);



	QLabel * map_label = new QLabel(tr("Map type:"));
	this->map_combo = new QComboBox();
	for (int i = 0; i < map_labels.size(); i++) {
		this->map_combo->addItem(map_labels.at(i));
	}



	QLabel * zoom_label = new QLabel(tr("Zoom level:"));
	this->zoom_combo = new QComboBox();
	for (unsigned int i = 0; i < viking_zoom_levels.size(); i++) {
		this->zoom_combo->addItem(viking_zoom_levels[i].to_string());
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
