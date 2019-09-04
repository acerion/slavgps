/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2019, Kamil Ignacak <acerion@wp.pl>
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




#include <QDebug>




#include "layer_trw.h"
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
void TpPropertiesDialog::update_timestamp_widget(const Trackpoint * tp)
{
	if (tp->timestamp.is_valid()) {
		this->timestamp_widget->set_timestamp(tp->timestamp, tp->coord);
	} else {
		this->timestamp_widget->reset_timestamp();
	}
}




void TpPropertiesDialog::sync_coord_entry_to_current_tp_cb(void) /* Slot. */
{
	if (!this->current_tp) {
		return;
	}
	if (this->sync_to_current_tp_block) {
		return;
	}

	//// this->current_tp->coord.get_coord_mode()

	const Coord new_coord = this->coord_entry->get_value();
	const bool redraw_track = Coord::distance(this->current_tp->coord, new_coord) > 0.05; /* May not be exact due to rounding. */
	this->current_tp->coord = new_coord;


	this->timestamp_widget->set_coord(new_coord);


	/* Don't redraw unless we really have to. */
	if (redraw_track) {
		/* Tell parent code that a track has changed its
		   coordinates (one of track's trackpoints has changed
		   its coordinates). */
		emit this->trackpoint_coordinates_changed();
	}
}




void TpPropertiesDialog::sync_altitude_entry_to_current_tp_cb(void) /* Slot. */
{
	if (!this->current_tp) {
		return;
	}
	if (this->sync_to_current_tp_block) {
		return;
	}

	/* Always store internally in metres. */
	this->current_tp->altitude = this->altitude_entry->get_value_iu();
}




/* Set timestamp of current trackpoint. */
void TpPropertiesDialog::sync_timestamp_entry_to_current_tp_cb(const Time & timestamp)
{
	qDebug() << SG_PREFIX_SLOT << "Slot received new timestamp" << timestamp;

	this->set_timestamp_of_current_tp(timestamp);
}




/* Clear timestamp of current trackpoint. */
void TpPropertiesDialog::sync_empty_timestamp_entry_to_current_tp_cb(void)
{
	qDebug() << SG_PREFIX_SLOT << "Slot received zero timestamp";

	this->set_timestamp_of_current_tp(Time()); /* Invalid value - this should indicate that timestamp is cleared from the tp. */
}




bool TpPropertiesDialog::set_timestamp_of_current_tp(const Time & timestamp)
{
	if (!this->current_tp) {
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

	this->current_tp->set_timestamp(timestamp);

	return true;
}




bool TpPropertiesDialog::sync_name_entry_to_current_tp_cb(const QString & new_name) /* Slot. */
{
	if (!this->current_tp) {
		return false;
	}
	if (this->sync_to_current_tp_block) {
		return false;
	}

	this->current_tp->set_name(new_name);

	return true;
}




void TpPropertiesDialog::reset_dialog_data(void)
{
	this->current_tp = NULL;
	this->current_track = NULL;

	this->trkpt_name->insert("");
	this->trkpt_name->setEnabled(false);

	this->timestamp_widget->reset_timestamp();

	this->course->setText("");

	this->coord_entry->setEnabled(false);
	this->altitude_entry->me_widget->setEnabled(false);
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
void TpPropertiesDialog::set_dialog_data(Track * track, const TrackPoints::iterator & current_tp_iter, bool is_route)
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


	this->coord_entry->setEnabled(true);
	this->altitude_entry->me_widget->setEnabled(true);
	this->timestamp_widget->setEnabled(tp->timestamp.is_valid());

	this->set_dialog_title(track->name);

	this->timestamp_widget->setEnabled(!is_route);
	if (is_route) {
		/* Remove any data that may have been previously displayed. */
		this->timestamp_widget->clear();
	}

	this->sync_to_current_tp_block = true; /* Don't update while setting data. */

	this->coord_entry->set_value(tp->coord, true); /* true: block signals when setting initial value of widget. */
	this->altitude_entry->set_value_iu(tp->altitude);


	this->update_timestamp_widget(tp);

	this->sync_to_current_tp_block = false; /* Can now update after setting data. */


	if (this->current_tp) {
		const Distance diff = Coord::distance_2(tp->coord, this->current_tp->coord);
		this->diff_dist->setText(diff.convert_to_unit(Preferences::get_unit_distance()).to_nice_string());

		if (tp->timestamp.is_valid() && this->current_tp->timestamp.is_valid()) {
			const Time timestamp_diff = tp->timestamp - this->current_tp->timestamp;
			this->diff_time->setText(tr("%1 s").arg((long) timestamp_diff.get_ll_value()));
			if (tp->timestamp == this->current_tp->timestamp) {
				this->diff_speed->setText("--");
			} else {
				const Distance dist = Coord::distance_2(tp->coord, this->current_tp->coord);
				const Time duration = Time::get_abs_diff(tp->timestamp, this->current_tp->timestamp);
				Speed speed_diff;
				speed_diff.make_speed(dist, duration);
				this->diff_speed->setText(speed_diff.to_string());
			}
		} else {
			this->diff_time->setText("");
			this->diff_speed->setText("");
		}
	}


	this->course->setText(tp->course.to_string());
	this->speed->setText(Speed(tp->gps_speed, SpeedUnit::MetresPerSecond).convert_to_unit(speed_unit).to_string());
	this->hdop->setText(Distance(tp->hdop, DistanceUnit::Meters).convert_to_unit(distance_unit).to_nice_string());
	this->pdop->setText(Distance(tp->pdop, DistanceUnit::Meters).convert_to_unit(distance_unit).to_nice_string());
	this->vdop->setText(Altitude(tp->vdop, HeightUnit::Metres).convert_to_unit(height_unit).to_nice_string());
	this->sat->setText(tr("%1 / %2").arg(tp->nsats).arg((int) tp->fix_mode));


	this->current_tp = tp;
	this->current_track = track;
}




void TpPropertiesDialog::set_dialog_title(const QString & track_name)
{
	this->setWindowTitle(QObject::tr("%1: Trackpoint").arg(track_name));
}




TpPropertiesDialog::TpPropertiesDialog(CoordMode coord_mode, QWidget * parent_widget) : QDialog(parent_widget)
{
	this->setWindowTitle(tr("Trackpoint"));

	this->button_box_upper = new QDialogButtonBox();
	this->button_box_lower = new QDialogButtonBox();

	this->button_insert_tp_after = this->button_box_upper->addButton(tr("&Insert After"), QDialogButtonBox::ActionRole);
	this->button_insert_tp_after->setIcon(QIcon::fromTheme("list-add"));
	this->button_delete_current_tp = this->button_box_upper->addButton(tr("&Delete"), QDialogButtonBox::ActionRole);
	this->button_delete_current_tp->setIcon(QIcon::fromTheme("list-delete"));
	this->button_split_track = this->button_box_upper->addButton(tr("Split Here"), QDialogButtonBox::ActionRole);

	this->button_go_back = this->button_box_lower->addButton(tr("&Back"), QDialogButtonBox::ActionRole);
	this->button_go_back->setIcon(QIcon::fromTheme("go-previous"));
	this->button_go_forward = this->button_box_lower->addButton(tr("&Forward"), QDialogButtonBox::ActionRole);
	this->button_go_forward->setIcon(QIcon::fromTheme("go-next"));
	this->button_close_dialog = this->button_box_lower->addButton(tr("&Close"), QDialogButtonBox::AcceptRole);


	 /* Without this connection the dialog wouldn't close.  Button
	    box is sending accepted() signal thanks to AcceptRole of
	    "Close" button, configured above. */
	connect(this->button_box_lower, SIGNAL (accepted()), this, SLOT (accept()));


	/* Use signal mapper only for buttons that do some action
	   related to track/trackpoint.  Previously I've used the
	   signal mapper also for "Close" button, but that led to some
	   crash of app (probably signal going back and forth between
	   the dialog and trw layer), so I removed it. */
	this->signal_mapper = new QSignalMapper(this);
	connect(this->button_insert_tp_after,   SIGNAL (released()), signal_mapper, SLOT (map()));
	connect(this->button_delete_current_tp, SIGNAL (released()), signal_mapper, SLOT (map()));
	connect(this->button_split_track,       SIGNAL (released()), signal_mapper, SLOT (map()));
	connect(this->button_go_back,           SIGNAL (released()), signal_mapper, SLOT (map()));
	connect(this->button_go_forward,        SIGNAL (released()), signal_mapper, SLOT (map()));

	this->signal_mapper->setMapping(this->button_insert_tp_after,   (int) TpPropertiesDialog::Action::InsertTpAfter);
	this->signal_mapper->setMapping(this->button_delete_current_tp, (int) TpPropertiesDialog::Action::DeleteSelectedTp);
	this->signal_mapper->setMapping(this->button_split_track,       (int) TpPropertiesDialog::Action::SplitAtSelectedTp);
	this->signal_mapper->setMapping(this->button_go_back,           (int) TpPropertiesDialog::Action::GoBack);
	this->signal_mapper->setMapping(this->button_go_forward,        (int) TpPropertiesDialog::Action::GoForward);

	connect(this->signal_mapper, SIGNAL (mapped(int)), this, SLOT (clicked_cb(int)));


	this->vbox = new QVBoxLayout;
	this->grid = new QGridLayout();
	this->vbox->addLayout(this->grid);
	this->vbox->addWidget(this->button_box_upper);
	this->vbox->addWidget(this->button_box_lower);


	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox);

	int row = 0;
	const int left_col = 0;
	const int right_col = 1;

	/* Properties of text labels that display non-editable
	   trackpoint properties. */
	const Qt::TextInteractionFlags value_display_label_flags = Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard;


	this->trkpt_name = new QLineEdit("", this);
	this->grid->addWidget(new QLabel(tr("Name:")), row, left_col);
	this->grid->addWidget(this->trkpt_name, row, right_col);
	connect(this->trkpt_name, SIGNAL (textEdited(const QString &)), this, SLOT (sync_name_entry_to_current_tp_cb(const QString &)));

	row++;

	this->coord_entry = new CoordEntryWidget(coord_mode);
	this->grid->addWidget(this->coord_entry, row, left_col, 1, 2);
	connect(this->coord_entry, SIGNAL (value_changed(void)), this, SLOT (sync_coord_entry_to_current_tp_cb(void)));

	row++;

	const HeightUnit height_unit = Altitude::get_internal_unit();
	MeasurementScale<Altitude> scale_alti(Altitude(SG_ALTITUDE_RANGE_MIN, height_unit),
					      Altitude(SG_ALTITUDE_RANGE_MAX, height_unit),
					      Altitude(0.0, height_unit),
					      Altitude(1, height_unit),
					      SG_ALTITUDE_PRECISION);
	this->altitude_entry = new MeasurementEntry_2<Altitude, HeightUnit>(Altitude(0, height_unit), &scale_alti, this);
	this->grid->addWidget(new QLabel(tr("Altitude:")), row, left_col);
	this->grid->addWidget(this->altitude_entry->me_widget, row, right_col);
	connect(this->altitude_entry->me_widget->spin, SIGNAL (valueChanged(double)), this, SLOT (sync_altitude_entry_to_current_tp_cb(void)));

	row++;

	this->course = new QLabel("", this);
	this->course->setTextInteractionFlags(value_display_label_flags);
	this->grid->addWidget(new QLabel(tr("Course:")), row, left_col);
	this->grid->addWidget(this->course, row, right_col);

	row++;

	this->timestamp_widget = new TimestampWidget();
	this->grid->addWidget(this->timestamp_widget, row, left_col, 1, 2);
	connect(this->timestamp_widget, SIGNAL (value_is_set(const Time &)), this, SLOT (sync_timestamp_entry_to_current_tp_cb(const Time &)));
	connect(this->timestamp_widget, SIGNAL (value_is_reset()), this, SLOT (sync_empty_timestamp_entry_to_current_tp_cb(void)));

	row++;

	this->diff_dist = new QLabel("", this);
	this->diff_dist->setTextInteractionFlags(value_display_label_flags);
	this->grid->addWidget(new QLabel(tr("Distance Difference:")), row, left_col);
	this->grid->addWidget(this->diff_dist, row, right_col);

	row++;

	this->diff_time = new QLabel("", this);
	this->diff_time->setTextInteractionFlags(value_display_label_flags);
	this->grid->addWidget(new QLabel(tr("Time Difference:")), row, left_col);
	this->grid->addWidget(this->diff_time, row, right_col);

	row++;

	this->diff_speed = new QLabel("", this);
	this->diff_speed->setTextInteractionFlags(value_display_label_flags);
	this->grid->addWidget(new QLabel(tr("\"Speed\" Between:")), row, left_col);
	this->grid->addWidget(this->diff_speed, row, right_col);

	row++;

	this->speed = new QLabel("", this);
	this->speed->setTextInteractionFlags(value_display_label_flags);
	this->grid->addWidget(new QLabel(tr("Speed:")), row, left_col);
	this->grid->addWidget(this->speed, row, right_col);

	row++;

	this->vdop = new QLabel("", this);
	this->vdop->setTextInteractionFlags(value_display_label_flags);
	this->grid->addWidget(new QLabel(tr("VDOP:")), row, left_col);
	this->grid->addWidget(this->vdop, row, right_col);

	row++;

	this->hdop = new QLabel("", this);
	this->hdop->setTextInteractionFlags(value_display_label_flags);
	this->grid->addWidget(new QLabel(tr("HDOP:")), row, left_col);
	this->grid->addWidget(this->hdop, row, right_col);

	row++;

	this->pdop = new QLabel("", this);
	this->pdop->setTextInteractionFlags(value_display_label_flags);
	this->grid->addWidget(new QLabel(tr("PDOP:")), row, left_col);
	this->grid->addWidget(this->pdop, row, right_col);

	row++;

	this->sat = new QLabel("", this);
	this->sat->setTextInteractionFlags(value_display_label_flags);
	this->grid->addWidget(new QLabel(tr("SAT/FIX:")), row, left_col);
	this->grid->addWidget(this->sat, row, right_col);
}




TpPropertiesDialog::~TpPropertiesDialog()
{
}




void TpPropertiesDialog::clicked_cb(int action) /* Slot. */
{
	qDebug() << SG_PREFIX_I << "Handling dialog action" << action;

	Track * track = this->current_track;
	if (!track) {
		qDebug() << SG_PREFIX_N << "Not handling action, no current track";
		return;
	}
	LayerTRW * trw = (LayerTRW *) track->get_owning_layer();
	if (!trw) {
		qDebug() << SG_PREFIX_N << "Not handling action, no current trw layer";
		return;
	}


	switch ((TpPropertiesDialog::Action) action) {
	case TpPropertiesDialog::Action::SplitAtSelectedTp:
		if (sg_ret::ok != track->split_at_selected_trackpoint_cb()) {
			break;
		}
		this->set_dialog_data(track, track->iterators[SELECTED].iter, track->is_route());
		break;

	case TpPropertiesDialog::Action::DeleteSelectedTp:
		if (!track->has_selected_tp()) {
			return;
		}
		trw->delete_selected_tp(track);

		if (track->has_selected_tp()) {
			/* Update Trackpoint Properties with the available adjacent trackpoint. */
			this->set_dialog_data(track, track->iterators[SELECTED].iter, track->is_route());
		}

		track->emit_tree_item_changed("Indicating deletion of trackpoint");
		break;

	case TpPropertiesDialog::Action::GoForward:
		if (sg_ret::ok != track->move_selected_tp_forward()) {
			break;
		}

		this->set_dialog_data(track, track->iterators[SELECTED].iter, track->is_route());
		track->emit_tree_item_changed("Indicating going forward in track");
		break;

	case TpPropertiesDialog::Action::GoBack:
		if (sg_ret::ok != track->move_selected_tp_back()) {
			break;
		}

		this->set_dialog_data(track, track->iterators[SELECTED].iter, track->is_route());
		track->emit_tree_item_changed("TRW - trackpoint properties - go back");
		break;

	case TpPropertiesDialog::Action::InsertTpAfter:
		/* This method makes necessary checks of selected trackpoint.
		   It also triggers redraw of layer. */
		track->create_tp_next_to_selected_tp(false);
		break;

	default:
		qDebug() << SG_PREFIX_E << "Unexpected dialog action" << action;
		break;
	}
}
