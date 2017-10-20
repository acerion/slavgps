/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

//#include <cmath>
#include <cstdlib>
#include <ctime>

#include <QDebug>

#include "ui_builder.h"
#include "layer_trw_trackpoint_properties.h"
#include "layer_trw_track_internal.h"
#include "vikutils.h"
#include "date_time_dialog.h"
#include "util.h"
#include "measurements.h"
#if 0
#include "coords.h"
#include "coord.h"
#include "layer_trw_waypoint.h"
#include "dialog.h"
#include "globals.h"
#include "ui_util.h"
#endif




using namespace SlavGPS;




/**
 *  Update the display for time fields.
 */
void PropertiesDialogTP::update_times(Trackpoint * tp)
{
	if (tp->has_timestamp) {
		this->timestamp->setValue(tp->timestamp);
		const QString msg = vu_get_time_string(&tp->timestamp, "%c", &tp->coord, NULL);
		this->datetime->setText(msg);
	} else {
		this->timestamp->setValue(0);
		this->datetime->setText("");
	}
}




void PropertiesDialogTP::sync_ll_to_tp_cb(void) /* Slot. */
{
	if (!this->cur_tp) {
		return;
	}
	if (this->sync_to_tp_block) {
		return;
	}

	struct LatLon ll;
	ll.lat = this->lat->value();
	ll.lon = this->lon->value();
	Coord coord(ll, this->cur_tp->coord.mode);

	/* Don't redraw unless we really have to. */
	if (Coord::distance(this->cur_tp->coord, coord) > 0.05) { /* May not be exact due to rounding. */
		this->cur_tp->coord = coord;
#ifdef K
		gtk_dialog_response(GTK_DIALOG(tpwin), SG_TRACK_CHANGED);
#endif
	}
}




void PropertiesDialogTP::sync_alt_to_tp_cb(void) /* Slot. */
{
	if (!this->cur_tp) {
		return;
	}
	if (this->sync_to_tp_block) {
		return;
	}

	/* Always store internally in metres. */
	const HeightUnit height_unit = Preferences::get_unit_height();
	switch (height_unit) {
	case HeightUnit::METRES:
		this->cur_tp->altitude = this->alt->value();
		break;
	case HeightUnit::FEET:
		this->cur_tp->altitude = VIK_FEET_TO_METERS(this->alt->value());
		break;
	default:
		this->cur_tp->altitude = this->alt->value();
		qDebug() << "EE: TrackPoint Properties: invalid height unit" << (int) height_unit << "in" << __FUNCTION__ << __LINE__;
	}
}




void PropertiesDialogTP::sync_timestamp_to_tp_cb(void) /* Slot. */
{
	if (!this->cur_tp) {
		return;
	}
	if (this->sync_to_tp_block) {
		return;
	}

	this->cur_tp->timestamp = this->timestamp->value();
	this->update_times(this->cur_tp);
}




static time_t last_edit_time = 0;

void PropertiesDialogTP::datetime_clicked_cb(void)
{
	if (!this->cur_tp) {
		return;
	}
	if (this->sync_to_tp_block) {
		return;
	}

#ifdef K
	if (event->button() == Qt::RightButton) {
		/* On right click and when a time is available, allow a method to copy the displayed time as text. */
		if (this->datetime->icon().isNull()) {
			vu_copy_label_menu(widget, event->button());
		}
		return;
	} else if (event->button() == Qt::MiddleButton) {
		return;
	}
#endif

	if (this->cur_tp->has_timestamp) {
		last_edit_time = this->cur_tp->timestamp;
	} else if (last_edit_time == 0) {
		time(&last_edit_time);
	} else {
	        /* Use last_edit_time that was set previously. */
	}

	time_t new_timestamp = 0;
	if (!date_time_dialog(tr("Edit Date/Time"), last_edit_time, new_timestamp, this)) {
		/* The dialog was cancelled? */
		return;
	} else {
		last_edit_time = new_timestamp;
		this->cur_tp->timestamp = new_timestamp;
		this->cur_tp->has_timestamp = true;
	}

	/* TODO: consider warning about unsorted times? */

	/* Clear the previous 'Add' icon as now a time is set. */
	if (!this->datetime->icon().isNull()) {
		this->datetime->setIcon(QIcon());
	}

	this->update_times(this->cur_tp);
}




bool PropertiesDialogTP::set_name_cb(void) /* Slot. */
{
	if (!this->cur_tp) {
		return false;
	}
	if (this->sync_to_tp_block) {
		return false;
	}

	this->cur_tp->set_name(this->trkpt_name->text());

	return true;
}




void PropertiesDialogTP::reset_dialog_data(void)
{
	/* TODO: shouldn't we set ->cur_tp to NULL? */
	this->trkpt_name->insert("");
	this->trkpt_name->setEnabled(false);

	this->datetime->setText("");

	this->course->setText("");

	this->lat->setEnabled(false);
	this->lon->setEnabled(false);
	this->alt->setEnabled(false);
	this->timestamp->setEnabled(false);
	this->datetime->setEnabled(false);

	/* Only keep Close button enabled. */
	this->button_insert_tp_after->setEnabled(false);
	this->button_split_track->setEnabled(false);
	this->button_delete_current_tp->setEnabled(false);
	this->button_go_back->setEnabled(false);
	this->button_go_forward->setEnabled(false);


	this->diff_dist->setText("");
	this->diff_time->setText("");
	this->diff_speed->setText("");
	this->speed->setText("");
	this->vdop->setText("");
	this->hdop->setText("");
	this->pdop->setText("");
	this->sat->setText("");

	this->setWindowTitle(QString("Trackpoint"));
}




/**
 * @track:      A Track
 * @iter:       Iterator to given Track
 * @is_route:   Is the track of the trackpoint actually a route?
 *
 * Sets the Trackpoint Edit Window to the values of the current trackpoint given in @tpl.
 */
void PropertiesDialogTP::set_dialog_data(Track * track, const TrackPoints::iterator & current_tp_iter, bool is_route)
{
	static char tmp_str[64];
	static QString tmp_string;
	static struct LatLon ll;

	Trackpoint * tp = *current_tp_iter;

	this->trkpt_name->setEnabled(true);
	if (tp->name.isEmpty()) { /* TODO: do we need these two branches at all? */
		this->trkpt_name->insert("");
	} else {
		this->trkpt_name->insert(tp->name);
	}

	/* User can insert only if not at the end of track (otherwise use extend track). */
	this->button_insert_tp_after->setEnabled(std::next(current_tp_iter) != track->end());
	this->button_delete_current_tp->setEnabled(true);

	/* We can only split up a track if it's not an endpoint. */
	this->button_split_track->setEnabled(std::next(current_tp_iter) != track->end() && current_tp_iter != track->begin());

	this->button_go_forward->setEnabled(std::next(current_tp_iter) != track->end());
	this->button_go_back->setEnabled(current_tp_iter != track->begin());


	this->lat->setEnabled(true);
	this->lon->setEnabled(true);
	this->alt->setEnabled(true);
	this->timestamp->setEnabled(tp->has_timestamp);

	this->datetime->setEnabled(tp->has_timestamp);
	/* Enable adding timestamps - but not on routepoints. */
	if (!tp->has_timestamp && !is_route) {
		this->datetime->setEnabled(true);
		this->datetime->setIcon(QIcon::fromTheme("list-add"));
	} else {
		this->set_dialog_title(track->name);
		if (!this->datetime->icon().isNull()) {
			this->datetime->setIcon(QIcon());
		}
	}

	this->sync_to_tp_block = true; /* Don't update while setting data. */

	ll = tp->coord.get_latlon();
	this->lat->setValue(ll.lat);
	this->lon->setValue(ll.lon);


	const HeightUnit height_unit = Preferences::get_unit_height();
	switch (height_unit) {
	case HeightUnit::METRES:
		this->alt->setValue(tp->altitude);
		break;
	case HeightUnit::FEET:
		this->alt->setValue(VIK_METERS_TO_FEET(tp->altitude));
		break;
	default:
		this->alt->setValue(tp->altitude);
		qDebug() << "EE: TrackPoint Properties: invalid height unit" << (int) height_unit << "in" << __FUNCTION__ << __LINE__;
	}

	this->update_times(tp);

	this->sync_to_tp_block = false; /* Can now update after setting data. */


	if (this->cur_tp) {

		tmp_string = Measurements::get_distance_string_short(Coord::distance(tp->coord, this->cur_tp->coord));
		this->diff_dist->setText(tmp_string);


		if (tp->has_timestamp && this->cur_tp->has_timestamp) {
			tmp_string = tr("%1 s").arg((long) (tp->timestamp - this->cur_tp->timestamp));
			this->diff_time->setText(tmp_string);
			if (tp->timestamp == this->cur_tp->timestamp) {
				this->diff_speed->setText(QString("--"));
			} else {
				double tmp_speed = Coord::distance(tp->coord, this->cur_tp->coord) / (ABS(tp->timestamp - this->cur_tp->timestamp));
				tmp_string = Measurements::get_speed_string(tmp_speed);
				this->diff_speed->setText(tmp_string);
			}
		} else {
			this->diff_time->setText("");
			this->diff_speed->setText("");
		}
	}


	tmp_string = Measurements::get_course_string(tp->course);
	this->course->setText(QString(tmp_string));


	tmp_string = Measurements::get_speed_string(tp->speed);
	this->speed->setText(tmp_string);


	tmp_string = Measurements::get_distance_string(tp->hdop, 5);
	this->hdop->setText(tmp_string);


	tmp_string = Measurements::get_distance_string(tp->pdop * 1.0936133, 5);
	this->pdop->setText(tmp_string);


	tmp_string = Measurements::get_altitude_string(tp->vdop, 5);
	this->vdop->setText(tmp_string);


	tmp_string = tr("%1 / %2").arg(tp->nsats).arg((int) tp->fix_mode);
	this->sat->setText(QString(tmp_string));

	this->cur_tp = tp;
}




void PropertiesDialogTP::set_dialog_title(const QString & track_name)
{
	const QString title = QString("%1: %2").arg(track_name).arg(QString("Trackpoint"));
	this->setWindowTitle(title);
}




PropertiesDialogTP::PropertiesDialogTP()
{
}




PropertiesDialogTP::PropertiesDialogTP(QWidget * parent_widget) : QDialog(parent_widget)
{
	this->setWindowTitle(QString("Trackpoint"));

	this->button_box = new QDialogButtonBox();
	this->parent = parent_widget;

	this->button_close_dialog = this->button_box->addButton("&Close", QDialogButtonBox::ActionRole);
	this->button_insert_tp_after = this->button_box->addButton("&Insert After", QDialogButtonBox::ActionRole);
	this->button_insert_tp_after->setIcon(QIcon::fromTheme("list-add"));
	this->button_delete_current_tp = this->button_box->addButton("&Delete", QDialogButtonBox::ActionRole);
	this->button_delete_current_tp->setIcon(QIcon::fromTheme("list-delete"));
	this->button_split_track = this->button_box->addButton("Split Here", QDialogButtonBox::ActionRole);
	this->button_go_back = this->button_box->addButton("&Back", QDialogButtonBox::ActionRole);
	this->button_go_back->setIcon(QIcon::fromTheme("go-previous"));
	this->button_go_forward = this->button_box->addButton("&Forward", QDialogButtonBox::ActionRole);
	this->button_go_forward->setIcon(QIcon::fromTheme("go-next"));



	this->signal_mapper = new QSignalMapper(this);
	connect(this->button_close_dialog,      SIGNAL (released()), signal_mapper, SLOT (map()));
	connect(this->button_insert_tp_after,   SIGNAL (released()), signal_mapper, SLOT (map()));
	connect(this->button_delete_current_tp, SIGNAL (released()), signal_mapper, SLOT (map()));
	connect(this->button_split_track,       SIGNAL (released()), signal_mapper, SLOT (map()));
	connect(this->button_go_back,           SIGNAL (released()), signal_mapper, SLOT (map()));
	connect(this->button_go_forward,        SIGNAL (released()), signal_mapper, SLOT (map()));

	this->signal_mapper->setMapping(this->button_close_dialog,      SG_TRACK_CLOSE_DIALOG);
	this->signal_mapper->setMapping(this->button_insert_tp_after,   SG_TRACK_INSERT_TP_AFTER);
	this->signal_mapper->setMapping(this->button_delete_current_tp, SG_TRACK_DELETE_CURRENT_TP);
	this->signal_mapper->setMapping(this->button_split_track,       SG_TRACK_SPLIT_TRACK_AT_CURRENT_TP);
	this->signal_mapper->setMapping(this->button_go_back,           SG_TRACK_GO_BACK);
	this->signal_mapper->setMapping(this->button_go_forward,        SG_TRACK_GO_FORWARD);


	this->vbox = new QVBoxLayout; /* Main track info. */
	this->hbox = new QHBoxLayout; /* Diff info. */

	QFormLayout * left_form = NULL;
	QFormLayout * right_form = NULL;
	QLayout * old_layout = NULL;

	left_form = new QFormLayout();
	this->left_area = new QWidget();
	old_layout = this->left_area->layout();
	delete old_layout;
	this->left_area->setLayout(left_form);

	right_form = new QFormLayout();
	this->right_area = new QWidget();
	old_layout = this->right_area->layout();
	delete old_layout;
	this->right_area->setLayout(right_form);


	this->hbox->addWidget(this->left_area);
	this->hbox->addWidget(this->right_area);
	this->vbox->addLayout(this->hbox);
	this->vbox->addWidget(this->button_box);


	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox);


	this->trkpt_name = new QLineEdit("", this);
	left_form->addRow(QString("Name:"), this->trkpt_name);
#ifdef K
	connect(this->trkpt_name, "focus-out-event", this, SLOT (set_name_cb(void)));
#endif



	this->lat = new QDoubleSpinBox(this);
	this->lat->setDecimals(6);
	this->lat->setMinimum(-90);
	this->lat->setMaximum(90);
	this->lat->setSingleStep(0.00005);
	this->lat->setValue(0);
	left_form->addRow(QString("Latitude:"), this->lat);
	connect(this->lat, SIGNAL (valueChanged(double)), this, SLOT (sync_ll_to_tp_cb(void)));


	this->lon = new QDoubleSpinBox(this);
	this->lon->setDecimals(6);
	this->lon->setMinimum(-180);
	this->lon->setMaximum(180);
	this->lon->setSingleStep(0.00005);
	this->lon->setValue(0);
	left_form->addRow(QString("Longitude:"), this->lon);
	connect(this->lon, SIGNAL (valueChanged(double)), this, SLOT (sync_ll_to_tp_cb(void)));


	this->alt = new QDoubleSpinBox(this);
	this->alt->setDecimals(2);
	this->alt->setMinimum(VIK_VAL_MIN_ALT);
	this->alt->setMaximum(VIK_VAL_MAX_ALT);
	this->alt->setSingleStep(10);
	this->alt->setValue(0);
	left_form->addRow(QString("Altitude:"), this->alt);
	connect(this->alt, SIGNAL (valueChanged(double)), this, SLOT (sync_alt_to_tp_cb(void)));


	this->course = new QLabel("", this);
	this->course->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	left_form->addRow(QString("Course:"), this->course);


	this->timestamp = new QSpinBox(this);
	this->timestamp->setMinimum(0);
	this->timestamp->setMaximum(2147483647); /* pow(2,31)-1 limit input to ~2038 for now. */ /* TODO: improve this initialization. */
	this->timestamp->setSingleStep(1);
	left_form->addRow(QString("Timestamp:"), this->timestamp);
	connect(this->timestamp, SIGNAL (valueChanged(int)), this, SLOT (sync_timestamp_to_tp_cb(void)));


	this->datetime = new QPushButton(this);
	left_form->addRow(QString("Time:"), this->datetime);
#ifdef K
	gtk_button_set_relief (GTK_BUTTON(tpwin->time), GTK_RELIEF_NONE);
#endif
	connect(this->datetime, SIGNAL (released(void)), this, SLOT (datetime_clicked_cb(void)));


	this->diff_dist = new QLabel("", this);
	this->diff_dist->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	right_form->addRow(QString("Distance Difference:"), this->diff_dist);


	this->diff_time = new QLabel("", this);
	this->diff_time->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	right_form->addRow(QString("Time Difference:"), this->diff_time);


	this->diff_speed = new QLabel("", this);
	this->diff_speed->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	right_form->addRow(QString("\"Speed\" Between:"), this->diff_speed);


	this->speed = new QLabel("", this);
	this->speed->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	right_form->addRow(QString("Speed:"), this->speed);


	this->vdop = new QLabel("", this);
	this->vdop->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	right_form->addRow(QString("VDOP:"), this->vdop);


	this->hdop = new QLabel("", this);
	this->hdop->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	right_form->addRow(QString("HDOP:"), this->hdop);


	this->pdop = new QLabel("", this);
	this->pdop->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	right_form->addRow(QString("PDOP:"), this->pdop);


	this->sat = new QLabel("", this);
	this->sat->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	right_form->addRow(QString("SAT/FIX:"), this->sat);
}



PropertiesDialogTP::~PropertiesDialogTP()
{
}
