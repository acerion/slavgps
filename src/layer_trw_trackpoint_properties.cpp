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
#include "preferences.h"




using namespace SlavGPS;




#define SG_MODULE "Trackpoint Properties"




/**
   Update contents of timestamp widget
*/
void PropertiesDialogTP::update_timestamp_widget(Trackpoint * tp)
{
	if (tp->timestamp.is_valid()) {
		this->timestamp_widget->set_timestamp(tp->timestamp, tp->coord);
	} else {
		this->timestamp_widget->reset_timestamp();
	}
}




void PropertiesDialogTP::sync_latlon_entry_to_current_tp_cb(void) /* Slot. */
{
	if (!this->cur_tp) {
		return;
	}
	if (this->sync_to_current_tp_block) {
		return;
	}


	const Coord new_coord(LatLon(this->lat_entry->value(), this->lon_entry->value()), this->cur_tp->coord.mode);


	const bool redraw_track = Coord::distance(this->cur_tp->coord, new_coord) > 0.05; /* May not be exact due to rounding. */
	this->cur_tp->coord = new_coord;


	this->timestamp_widget->set_coord(new_coord);


	/* Don't redraw unless we really have to. */
	if (redraw_track) {
		/* Tell parent code that a track has changed its
		   coordinates (one of track's trackpoints has changed
		   its coordinates). */
		emit this->trackpoint_coordinates_changed(SG_TRACK_CHANGED);
	}
}




void PropertiesDialogTP::sync_altitude_entry_to_current_tp_cb(void) /* Slot. */
{
	if (!this->cur_tp) {
		return;
	}
	if (this->sync_to_current_tp_block) {
		return;
	}

	/* Always store internally in metres. */
	this->cur_tp->altitude = this->alt->get_value_iu().get_altitude();
}




/* Set timestamp of current trackpoint. */
void PropertiesDialogTP::sync_timestamp_entry_to_current_tp_cb(time_t timestamp_value)
{
	qDebug() << SG_PREFIX_SLOT << "Slot received new timestamp" << timestamp_value;

	this->set_timestamp_of_current_tp(Time(timestamp_value));
}




/* Clear timestamp of current trackpoint. */
void PropertiesDialogTP::sync_empty_timestamp_entry_to_current_tp_cb(void)
{
	qDebug() << SG_PREFIX_SLOT << "Slot received zero timestamp";

	this->set_timestamp_of_current_tp(Time()); /* Invalid value - this should indicate that timestamp is cleared from the tp. */
}




bool PropertiesDialogTP::set_timestamp_of_current_tp(const Time & timestamp)
{
	if (!this->cur_tp) {
		return false;
	}
	if (this->sync_to_current_tp_block) {
		/* TODO_LATER: indicate to user that operation has failed. */
		return false;
	}

	/* TODO_LATER: we are changing a timestamp of tp somewhere in
	   the middle of a track, so the timestamps may now not have
	   consecutive values.  Should we now warn user about unsorted
	   timestamps in consecutive trackpoints? */

	this->cur_tp->set_timestamp(timestamp);

	return true;
}




bool PropertiesDialogTP::sync_name_entry_to_current_tp_cb(const QString & new_name) /* Slot. */
{
	if (!this->cur_tp) {
		return false;
	}
	if (this->sync_to_current_tp_block) {
		return false;
	}

	this->cur_tp->set_name(new_name);

	return true;
}




void PropertiesDialogTP::reset_dialog_data(void)
{
	this->cur_tp = NULL;

	this->trkpt_name->insert("");
	this->trkpt_name->setEnabled(false);

	this->timestamp_widget->reset_timestamp();

	this->course->setText("");

	this->lat_entry->setEnabled(false);
	this->lon_entry->setEnabled(false);
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
	const HeightUnit height_unit = Preferences::get_unit_height();
	const DistanceUnit distance_unit = Preferences::get_unit_distance();
	const SpeedUnit speed_unit = Preferences::get_unit_speed();

	Trackpoint * tp = *current_tp_iter;

	this->trkpt_name->setEnabled(true);
	this->trkpt_name->setText(tp->name); /* The name may be empty, but we have to do this anyway (e.g. to overwrite non-empty name of previous trackpoint). */

	/* User can insert only if not at the end of track (otherwise use extend track). */
	this->button_insert_tp_after->setEnabled(std::next(current_tp_iter) != track->end());
	this->button_delete_current_tp->setEnabled(true);

	/* We can only split up a track if it's not an endpoint. */
	this->button_split_track->setEnabled(std::next(current_tp_iter) != track->end() && current_tp_iter != track->begin());

	this->button_go_forward->setEnabled(std::next(current_tp_iter) != track->end());
	this->button_go_back->setEnabled(current_tp_iter != track->begin());


	this->lat_entry->setEnabled(true);
	this->lon_entry->setEnabled(true);
	this->alt->setEnabled(true);
	this->timestamp_widget->setEnabled(tp->timestamp.is_valid());

	this->set_dialog_title(track->name);

	this->timestamp_widget->setEnabled(!is_route);
	if (is_route) {
		/* Remove any data that may have been previously displayed. */
		this->timestamp_widget->clear();
	}

	this->sync_to_current_tp_block = true; /* Don't update while setting data. */

	const LatLon lat_lon = tp->coord.get_latlon();
	this->lat_entry->setValue(lat_lon.lat);
	this->lon_entry->setValue(lat_lon.lon);


	this->alt->set_value_iu(tp->altitude);


	this->update_timestamp_widget(tp);

	this->sync_to_current_tp_block = false; /* Can now update after setting data. */


	if (this->cur_tp) {
		const Distance diff = Coord::distance_2(tp->coord, this->cur_tp->coord);
		this->diff_dist->setText(diff.convert_to_unit(Preferences::get_unit_distance()).to_nice_string());

		if (tp->timestamp.is_valid() && this->cur_tp->timestamp.is_valid()) {
			this->diff_time->setText(tr("%1 s").arg((long) (tp->timestamp.get_value() - this->cur_tp->timestamp.get_value())));
			if (tp->timestamp == this->cur_tp->timestamp) {
				this->diff_speed->setText("--");
			} else {
				const double dist = Coord::distance(tp->coord, this->cur_tp->coord);
				const Time duration = Time::get_abs_diff(tp->timestamp, this->cur_tp->timestamp);
				const Speed tmp_speed(dist / duration.get_value(), SpeedUnit::MetresPerSecond);
				this->diff_speed->setText(tmp_speed.to_string());
			}
		} else {
			this->diff_time->setText("");
			this->diff_speed->setText("");
		}
	}


	this->course->setText(Angle::get_course_string(tp->course));
	this->speed->setText(Speed(tp->speed, SpeedUnit::MetresPerSecond).convert_to_unit(speed_unit).to_string());
	this->hdop->setText(Distance(tp->hdop, SupplementaryDistanceUnit::Meters).convert_to_unit(distance_unit).to_nice_string());
	this->pdop->setText(Distance(tp->pdop, SupplementaryDistanceUnit::Meters).convert_to_unit(distance_unit).to_nice_string());
	this->vdop->setText(Altitude(tp->vdop, HeightUnit::Metres).convert_to_unit(height_unit).to_nice_string());
	this->sat->setText(tr("%1 / %2").arg(tp->nsats).arg((int) tp->fix_mode));


	this->cur_tp = tp;
}




void PropertiesDialogTP::set_dialog_title(const QString & track_name)
{
	this->setWindowTitle(QObject::tr("%1: Trackpoint").arg(track_name));
}




PropertiesDialogTP::PropertiesDialogTP()
{
}




PropertiesDialogTP::PropertiesDialogTP(QWidget * parent_widget) : QDialog(parent_widget)
{
	this->setWindowTitle(tr("Trackpoint"));

	this->button_box = new QDialogButtonBox();

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
	this->signal_mapper->setMapping(this->button_delete_current_tp, SG_TRACK_DELETE_SELECTED_TP);
	this->signal_mapper->setMapping(this->button_split_track,       SG_TRACK_SPLIT_TRACK_AT_SELECTED_TP);
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
	connect(this->trkpt_name, SIGNAL (textEdited(const QString &)), this, SLOT (sync_name_entry_to_current_tp_cb(const QString &)));



	this->lat_entry = new LatEntryWidget(SGVariant(0.0, SGVariantType::Latitude));
	this->grid->addWidget(new QLabel(tr("Latitude:")), 1, 0);
	this->grid->addWidget(this->lat_entry, 1, 1);
	connect(this->lat_entry, SIGNAL (valueChanged(double)), this, SLOT (sync_latlon_entry_to_current_tp_cb(void)));


	this->lon_entry = new LonEntryWidget(SGVariant(0.0, SGVariantType::Longitude));
	this->grid->addWidget(new QLabel(tr("Longitude:")), 2, 0);
	this->grid->addWidget(this->lon_entry, 2, 1);
	connect(this->lon_entry, SIGNAL (valueChanged(double)), this, SLOT (sync_llatlon_entry_to_current_tp_cb(void)));


	ParameterScale<double> scale_alti(SG_ALTITUDE_RANGE_MIN, SG_ALTITUDE_RANGE_MAX, SGVariant(0.0), 1, SG_ALTITUDE_PRECISION);
	this->alt = new MeasurementEntryWidget(this->alt, &scale_alti, this);
	this->grid->addWidget(new QLabel(tr("Altitude:")), 3, 0);
	this->grid->addWidget(this->alt, 3, 1);
	connect(this->alt, SIGNAL (valueChanged(double)), this, SLOT (sync_altitude_entry_to_current_tp_cb(void)));


	this->course = new QLabel("", this);
	this->course->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	this->grid->addWidget(new QLabel(tr("Course:")), 4, 0);
	this->grid->addWidget(this->course, 4, 1);


	this->timestamp_widget = new TimestampWidget();
	this->grid->addWidget(this->timestamp_widget, 5, 0, 2, 2);
	connect(this->timestamp_widget, SIGNAL (value_is_set(time_t)), this, SLOT (sync_timestamp_entry_to_current_tp_cb(time_t)));
	connect(this->timestamp_widget, SIGNAL (value_is_reset()), this, SLOT (sync_empty_timestamp_entry_to_current_tp_cb(void)));





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
