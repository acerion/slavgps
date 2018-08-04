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




using namespace SlavGPS;




QString SlavGPS::a_dialog_new_track(QString const & default_name, bool is_route, QWidget * parent)
{
	QString text;
	bool ok;
	do {
		text = QInputDialog::getText(parent,
					     is_route ? QString("Add Route") : QString("Add Track"),
					     is_route ? QString("Route Name:") : QString("Track Name:"),
					     QLineEdit::Normal,
					     QString(default_name), &ok);

		if (ok && text.isEmpty()) {
			QMessageBox::information(parent,
						 is_route ? QString("Route Name") : QString("Track Name"),
						 is_route ? QString("Please enter a name for the route.") : QString("Please enter a name for the track."));
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
	this->radio_group = new SGRadioGroup("", &items, NULL); /* kamilTODO: delete this widget in destructor? */


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




bool SlavGPS::a_dialog_map_and_zoom(const QStringList & map_labels, unsigned int default_map_idx, const QStringList & zoom_labels, unsigned int default_zoom_idx, unsigned int * selected_map_idx, unsigned int * selected_zoom_idx, QWidget * parent)
{
	BasicDialog dialog(parent);
	dialog.setWindowTitle(QObject::tr("Download along track"));


	QLabel map_label(QObject::tr("Map type:"));
	QComboBox map_combo;
	for (int i = 0; i < map_labels.size(); i++) {
		map_combo.addItem(map_labels.at(i));
	}
	map_combo.setCurrentIndex(default_map_idx);

	QLabel zoom_label(QObject::tr("Zoom level:"));
	QComboBox zoom_combo;
	for (int i = 0; i < zoom_labels.size(); i++) {
		zoom_combo.addItem(zoom_labels.at(i));
	}
	zoom_combo.setCurrentIndex(default_zoom_idx);


	dialog.grid->addWidget(&map_label, 0, 0);
	dialog.grid->addWidget(&map_combo, 0, 1);
	dialog.grid->addWidget(&zoom_label, 1, 0);
	dialog.grid->addWidget(&zoom_combo, 1, 1);


	if (QDialog::Accepted == dialog.exec()) {
		*selected_map_idx = map_combo.currentIndex();
		*selected_zoom_idx = zoom_combo.currentIndex();
		/* There is something strange about argument to qSetRealNumberPrecision().  The precision for
		   fractional part is not enough, I had to add few places for leading digits and decimal dot. */
		qDebug() << "DD: Dialog: Map and Zoom: map index:" << *selected_map_idx << "zoom index:" << *selected_zoom_idx;
		return true;
	} else {
		return false;
	}
}
