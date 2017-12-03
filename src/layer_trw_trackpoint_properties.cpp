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




#define PREFIX "Trackpoint Properties: "




/**
 *  Update the display for time fields.
 */
void PropertiesDialogTP::update_times(Trackpoint * tp)
{
	if (tp->has_timestamp) {
		this->timestamp_widget->set_timestamp(tp->timestamp, &tp->coord);
	} else {
		this->timestamp_widget->reset_timestamp();
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

	LatLon ll;
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

	this->cur_tp->timestamp = this->timestamp_widget->get_timestamp();
	this->update_times(this->cur_tp);
}




/* Set timestamp of current trackpoint. */
void PropertiesDialogTP::set_timestamp_cb(time_t timestamp_value)
{
	qDebug() << "SLOT: Layer TRW Trackpoint Properties: 'Set Timestamp' slot" << timestamp_value;

	if (!this->cur_tp) {
		return;
	}
	if (this->sync_to_tp_block) {
		/* TODO: indicate to user that operation has failed. */
		return;
	}

	/* TODO: consider warning about unsorted times? */

	/* TODO: consider saving given timestamp and somehow passing
	   it to date/time dialog next time a timestamp needs to be
	   set on non-initialized tp ("last edit time"). */

	this->cur_tp->timestamp = timestamp_value;
	this->cur_tp->has_timestamp = true;
}




/* Clear timestamp of current trackpoint. */
void PropertiesDialogTP::clear_timestamp_cb(void)
{
	qDebug() << "SLOT: Layer TRW Trackpoint Properties: 'Clear Timestamp' slot";

	if (!this->cur_tp) {
		return;
	}
	if (this->sync_to_tp_block) {
		/* TODO: indicate to user that operation has failed. */
		return;
	}

	this->cur_tp->timestamp = 0;
	this->cur_tp->has_timestamp = false;
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

	this->timestamp_widget->reset_timestamp();

	this->course->setText("");

	this->lat->setEnabled(false);
	this->lon->setEnabled(false);
	this->alt->setEnabled(false);
	this->timestamp_widget->setEnabled(false);

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

	this->setWindowTitle(tr("Trackpoint"));
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
	this->timestamp_widget->setEnabled(tp->has_timestamp);

	/* Enable adding timestamps - but not on routepoints. */
#ifdef K
	if (!tp->has_timestamp && !is_route) {
		this->timestamp_widget->timestamp_button->setEnabled(true);
	} else {
		this->set_dialog_title(track->name);
#if 0
		if (!this->timestamp_widget->timestamp_button->icon().isNull()) {
			this->timestamp_widget->timestamp_button->setIcon(QIcon());
		}
#endif
	}
#endif

	this->sync_to_tp_block = true; /* Don't update while setting data. */

	const LatLon lat_lon = tp->coord.get_latlon();
	this->lat->setValue(lat_lon.lat);
	this->lon->setValue(lat_lon.lon);



	const HeightUnit height_unit = Preferences::get_unit_height();
	switch (height_unit) {
	case HeightUnit::METRES:
		this->alt->setValue(tp->altitude);
		break;
	case HeightUnit::FEET:
		this->alt->setValue(VIK_METERS_TO_FEET(tp->altitude));
		break;
	default:
		qDebug() << "EE:" PREFIX "invalid height unit" << (int) height_unit << "in" << __FUNCTION__ << __LINE__;
		this->alt->setValue(tp->altitude);
		break;
	}


	this->update_times(tp);

	this->sync_to_tp_block = false; /* Can now update after setting data. */


	if (this->cur_tp) {
		this->diff_dist->setText(Measurements::get_distance_string_short(Coord::distance(tp->coord, this->cur_tp->coord)));

		if (tp->has_timestamp && this->cur_tp->has_timestamp) {
			this->diff_time->setText(tr("%1 s").arg((long) (tp->timestamp - this->cur_tp->timestamp)));
			if (tp->timestamp == this->cur_tp->timestamp) {
				this->diff_speed->setText("--");
			} else {
				const double tmp_speed = Coord::distance(tp->coord, this->cur_tp->coord) / (ABS(tp->timestamp - this->cur_tp->timestamp));
				this->diff_speed->setText(Measurements::get_speed_string(tmp_speed));
			}
		} else {
			this->diff_time->setText("");
			this->diff_speed->setText("");
		}
	}


	this->course->setText(Measurements::get_course_string(tp->course));
	this->speed->setText(Measurements::get_speed_string(tp->speed));
	this->hdop->setText(Measurements::get_distance_string(tp->hdop, 5));
	this->pdop->setText(Measurements::get_distance_string(tp->pdop * 1.0936133, 5));
	this->vdop->setText(Measurements::get_altitude_string(tp->vdop, 5));
	this->sat->setText(tr("%1 / %2").arg(tp->nsats).arg((int) tp->fix_mode));


	this->cur_tp = tp;
}




void PropertiesDialogTP::set_dialog_title(const QString & track_name)
{
	const QString title = tr("%1: %2").arg(track_name).arg(tr("Trackpoint"));
	this->setWindowTitle(title);
}




PropertiesDialogTP::PropertiesDialogTP()
{
}




PropertiesDialogTP::PropertiesDialogTP(QWidget * parent_widget) : QDialog(parent_widget)
{
	this->setWindowTitle(tr("Trackpoint"));

	this->button_box = new QDialogButtonBox();
	this->parent = parent_widget;

	this->button_close_dialog = this->button_box->addButton(tr("&Close"), QDialogButtonBox::ActionRole);
	this->button_insert_tp_after = this->button_box->addButton(tr("&Insert After"), QDialogButtonBox::ActionRole);
	this->button_insert_tp_after->setIcon(QIcon::fromTheme("list-add"));
	this->button_delete_current_tp = this->button_box->addButton(tr("&Delete"), QDialogButtonBox::ActionRole);
	this->button_delete_current_tp->setIcon(QIcon::fromTheme("list-delete"));
	this->button_split_track = this->button_box->addButton(tr("Split Here"), QDialogButtonBox::ActionRole);
	this->button_go_back = this->button_box->addButton(tr("&Back"), QDialogButtonBox::ActionRole);
	this->button_go_back->setIcon(QIcon::fromTheme("go-previous"));
	this->button_go_forward = this->button_box->addButton(tr("&Forward"), QDialogButtonBox::ActionRole);
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


	this->vbox = new QVBoxLayout;


	this->grid = new QGridLayout();
	this->vbox->addLayout(this->grid);
	this->vbox->addWidget(this->button_box);


	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox);


	this->trkpt_name = new QLineEdit("", this);
	this->grid->addWidget(new QLabel(tr("Name:")), 0, 0);
	this->grid->addWidget(this->trkpt_name, 0, 1);
#ifdef K
	connect(this->trkpt_name, "focus-out-event", this, SLOT (set_name_cb(void)));
#endif



	this->lat = new QDoubleSpinBox(this);
	this->lat->setDecimals(6);
	this->lat->setMinimum(-90);
	this->lat->setMaximum(90);
	this->lat->setSingleStep(0.00005);
	this->lat->setValue(0);
	this->grid->addWidget(new QLabel(tr("Latitude:")), 1, 0);
	this->grid->addWidget(this->lat, 1, 1);
	connect(this->lat, SIGNAL (valueChanged(double)), this, SLOT (sync_ll_to_tp_cb(void)));


	this->lon = new QDoubleSpinBox(this);
	this->lon->setDecimals(6);
	this->lon->setMinimum(-180);
	this->lon->setMaximum(180);
	this->lon->setSingleStep(0.00005);
	this->lon->setValue(0);
	this->grid->addWidget(new QLabel(tr("Longitude:")), 2, 0);
	this->grid->addWidget(this->lon, 2, 1);
	connect(this->lon, SIGNAL (valueChanged(double)), this, SLOT (sync_ll_to_tp_cb(void)));


	this->alt = new QDoubleSpinBox(this);
	this->alt->setDecimals(SG_ALTITUDE_PRECISION);
	this->alt->setMinimum(SG_ALTITUDE_RANGE_MIN);
	this->alt->setMaximum(SG_ALTITUDE_RANGE_MAX);
	this->alt->setSingleStep(1);
	this->alt->setValue(0);
	this->grid->addWidget(new QLabel(tr("Altitude:")), 3, 0);
	this->grid->addWidget(this->alt, 3, 1);
	connect(this->alt, SIGNAL (valueChanged(double)), this, SLOT (sync_alt_to_tp_cb(void)));


	this->course = new QLabel("", this);
	this->course->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	this->grid->addWidget(new QLabel(tr("Course:")), 4, 0);
	this->grid->addWidget(this->course, 4, 1);


	this->timestamp_widget = new SGTimestampWidget();
	this->grid->addWidget(this->timestamp_widget, 5, 0, 2, 2);
	// TODO: connect(this->timestamp_widget, SIGNAL (value_is_set(time_t)), this, SLOT (sync_timestamp_to_tp_cb(time_t)));
	connect(this->timestamp_widget, SIGNAL (value_is_set(time_t)), this, SLOT (set_timestamp_cb(time_t)));
	connect(this->timestamp_widget, SIGNAL (value_is_reset()), this, SLOT (clear_timestamp_cb(void)));





	this->diff_dist = new QLabel("", this);
	this->diff_dist->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	this->grid->addWidget(new QLabel(tr("Distance Difference:")), 0, 3);
	this->grid->addWidget(this->diff_dist, 0, 4);


	this->diff_time = new QLabel("", this);
	this->diff_time->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	this->grid->addWidget(new QLabel(tr("Time Difference:")), 1, 3);
	this->grid->addWidget(this->diff_time, 1, 4);


	this->diff_speed = new QLabel("", this);
	this->diff_speed->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	this->grid->addWidget(new QLabel(tr("\"Speed\" Between:")), 2, 3);
	this->grid->addWidget(this->diff_speed, 2, 4);


	this->speed = new QLabel("", this);
	this->speed->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	this->grid->addWidget(new QLabel(tr("Speed:")), 3, 3);
	this->grid->addWidget(this->speed, 3, 4);


	this->vdop = new QLabel("", this);
	this->vdop->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	this->grid->addWidget(new QLabel(tr("VDOP:")), 4, 3);
	this->grid->addWidget(this->vdop, 4, 4);


	this->hdop = new QLabel("", this);
	this->hdop->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	this->grid->addWidget(new QLabel(tr("HDOP:")), 5, 3);
	this->grid->addWidget(this->hdop, 5, 4);


	this->pdop = new QLabel("", this);
	this->pdop->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	this->grid->addWidget(new QLabel(tr("PDOP:")), 6, 3);
	this->grid->addWidget(this->pdop, 6, 4);


	this->sat = new QLabel("", this);
	this->sat->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	this->grid->addWidget(new QLabel(tr("SAT/FIX:")), 7, 3);
	this->grid->addWidget(this->sat, 7, 4);


	this->grid->addWidget(new QLabel("  "), 0, 2); /* Spacer item. */
}




PropertiesDialogTP::~PropertiesDialogTP()
{
}
