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




void TpPropertiesDialog::sync_coord_widget_to_current_point_cb(void) /* Slot. */
{
	if (NULL == this->current_point) {
		return;
	}
	if (this->skip_syncing_to_current_point) {
		return;
	}


	const Coord old_coord = this->current_point->coord;
	const Coord new_coord = this->coord_widget->get_value();


	this->current_point->coord = new_coord;
	this->timestamp_widget->set_coord(new_coord);


	/* Don't redraw unless we really have to. */
	const Distance distance = Coord::distance_2(old_coord, new_coord); /* May not be exact due to rounding. */
	const bool redraw = distance.is_valid() && distance.get_ll_value() > 0.05;
	if (redraw) {
		/* One of track's trackpoints has changed
		   its coordinates. */
		emit this->point_coordinates_changed();
	}
}




void TpPropertiesDialog::sync_altitude_widget_to_current_point_cb(void) /* Slot. */
{
	if (NULL == this->current_point) {
		return;
	}
	if (this->skip_syncing_to_current_point) {
		return;
	}

	/* Always store internally in metres. */
	this->current_point->altitude = this->altitude_widget->get_value_iu();
}




/* Set timestamp of current trackpoint. */
bool TpPropertiesDialog::sync_timestamp_widget_to_current_point_cb(const Time & timestamp)
{
	qDebug() << SG_PREFIX_SLOT << "Slot received new timestamp" << timestamp;

	if (NULL == this->current_point) {
		return false;
	}
	if (this->skip_syncing_to_current_point) {
		return false;
	}

	/* TODO_LATER: we are changing a timestamp of tp somewhere in
	   the middle of a track, so the timestamps may now not have
	   consecutive values.  Should we now warn user about unsorted
	   timestamps in consecutive trackpoints? */
	this->current_point->set_timestamp(timestamp);

	return true;
}




/* Clear timestamp of current trackpoint. */
bool TpPropertiesDialog::sync_empty_timestamp_widget_to_current_point_cb(void)
{
	qDebug() << SG_PREFIX_SLOT << "Slot received zero timestamp";

	if (NULL == this->current_point) {
		return false;
	}
	if (this->skip_syncing_to_current_point) {
		return false;
	}

	/* TODO_LATER: we are changing a timestamp of tp somewhere in
	   the middle of a track, so the timestamps may now not have
	   consecutive values.  Should we now warn user about unsorted
	   timestamps in consecutive trackpoints? */
	this->current_point->set_timestamp(Time()); /* Invalid value - this should indicate that timestamp is cleared from the tp. */

	return true;
}




bool TpPropertiesDialog::sync_name_entry_to_current_point_cb(const QString & new_name) /* Slot. */
{
	if (NULL == this->current_point) {
		return false;
	}
	if (this->skip_syncing_to_current_point) {
		return false;
	}

	this->current_point->set_name(new_name);

	return true;
}




/**
 * @track:      A Track
 * @iter:       Iterator to given Track
 * @is_route:   Is the track of the trackpoint actually a route?
 *
 * Sets the Trackpoint Edit Window to the values of the current trackpoint given in @tpl.
 */
void TpPropertiesDialog::set_dialog_data(Track * track, Trackpoint * trackpoint)
{
	const HeightUnit height_unit = Preferences::get_unit_height();
	const DistanceUnit distance_unit = Preferences::get_unit_distance();
	const SpeedUnit speed_unit = Preferences::get_unit_speed();

	Trackpoint point;
	if (trackpoint) {
		point = *trackpoint;
	}
	const TrackPoints::iterator & current_point_iter = track->iterators[SELECTED].iter;
	const bool is_route = track ? track->is_route() : false;

	this->name_entry->setEnabled(true);
	this->name_entry->setText(point.name); /* The name may be empty, but we have to do this anyway (e.g. to overwrite non-empty name of previous trackpoint). */



	/* User can insert only if not at the end of track (otherwise use extend track). */
	this->button_insert_tp_after->setEnabled(std::next(current_point_iter) != track->end());

	/* We can only split up a track if it's not an endpoint. */
	this->button_split_track->setEnabled(std::next(current_point_iter) != track->end() && current_point_iter != track->begin());

	this->button_delete_current_point->setEnabled(true);

	this->button_next_point->setEnabled(std::next(current_point_iter) != track->end());
	this->button_previous_point->setEnabled(current_point_iter != track->begin());



	this->coord_widget->setEnabled(true);
	this->altitude_widget->me_widget->setEnabled(true);
	this->timestamp_widget->setEnabled(point.timestamp.is_valid());

	this->set_dialog_title(track->name);

	this->timestamp_widget->setEnabled(!is_route);
	if (is_route) {
		/* Remove any data that may have been previously displayed. */
		this->timestamp_widget->clear();
	}

	this->skip_syncing_to_current_point = true; /* Don't update while setting data. */

	this->coord_widget->set_value(point.coord, true); /* true: block signals when setting initial value of widget. */
	this->altitude_widget->set_value_iu(point.altitude);
	this->update_timestamp_widget(&point);

	this->skip_syncing_to_current_point = false; /* Can now update after setting data. */


	if (this->current_point) {
		const Distance diff = Coord::distance_2(point.coord, this->current_point->coord);
		this->diff_dist->setText(diff.convert_to_unit(Preferences::get_unit_distance()).to_nice_string());

		if (point.timestamp.is_valid() && this->current_point->timestamp.is_valid()) {
			const Time timestamp_diff = point.timestamp - this->current_point->timestamp;
			this->diff_time->setText(tr("%1 s").arg((long) timestamp_diff.get_ll_value()));
			if (point.timestamp == this->current_point->timestamp) {
				this->diff_speed->setText("--");
			} else {
				const Distance dist = Coord::distance_2(point.coord, this->current_point->coord);
				const Time duration = Time::get_abs_diff(point.timestamp, this->current_point->timestamp);
				Speed speed_diff;
				speed_diff.make_speed(dist, duration);
				this->diff_speed->setText(speed_diff.to_string());
			}
		} else {
			this->diff_time->setText("");
			this->diff_speed->setText("");
		}
	}


	this->course->setText(point.course.to_string());
	this->speed->setText(Speed(point.gps_speed, SpeedUnit::MetresPerSecond).convert_to_unit(speed_unit).to_string());
	this->hdop->setText(Distance(point.hdop, DistanceUnit::Meters).convert_to_unit(distance_unit).to_nice_string());
	this->pdop->setText(Distance(point.pdop, DistanceUnit::Meters).convert_to_unit(distance_unit).to_nice_string());
	this->vdop->setText(Altitude(point.vdop, HeightUnit::Metres).convert_to_unit(height_unit).to_nice_string());
	this->sat->setText(tr("%1 / %2").arg(point.nsats).arg((int) point.fix_mode));


	this->current_point = trackpoint;
	this->current_track = track;
}




void TpPropertiesDialog::reset_dialog_data(void)
{
	if (this->current_track) {
		Trackpoint tp; /* Invalid, empty object. */
		this->set_dialog_data(this->current_track, &tp);
		return;
	}

	this->current_point = NULL;
	this->current_track = NULL;

	this->disable_widgets();

	this->setWindowTitle(tr("Trackpoint"));
}







void TpPropertiesDialog::set_dialog_title(const QString & track_name)
{
	this->setWindowTitle(QObject::tr("%1: Trackpoint").arg(track_name));
}




TpPropertiesDialog::TpPropertiesDialog(CoordMode coord_mode, QWidget * parent_widget) : TpPropertiesWidget(parent_widget)
{
	this->setWindowTitle(tr("Trackpoint"));
	this->build_buttons(this);
	this->build_widgets(this);
}




TpPropertiesDialog::~TpPropertiesDialog()
{
}




void TpPropertiesDialog::clicked_cb(int action) /* Slot. */
{
	qDebug() << SG_PREFIX_I << "Handling dialog action" << action;

	if (!this->current_track) {
		qDebug() << SG_PREFIX_N << "Not handling action, no current track";
		return;
	}
	LayerTRW * trw = (LayerTRW *) this->current_track->get_owning_layer();
	if (!trw) {
		qDebug() << SG_PREFIX_N << "Not handling action, no current trw layer";
		return;
	}


	switch ((TpPropertiesDialog::Action) action) {
	case TpPropertiesDialog::Action::SplitAtSelectedTp:
		if (sg_ret::ok != this->current_track->split_at_selected_trackpoint_cb()) {
			break;
		}
		this->set_dialog_data(this->current_track, this->current_point);
		break;

	case TpPropertiesDialog::Action::DeleteSelectedPoint:
		if (!this->current_track->has_selected_tp()) {
			return;
		}
		trw->delete_selected_tp(this->current_track);

		if (this->current_track->has_selected_tp()) {
			/* Update Trackpoint Properties with the available adjacent trackpoint. */
			this->set_dialog_data(this->current_track, this->current_point);
		}

		this->current_track->emit_tree_item_changed("Indicating deletion of trackpoint");
		break;

	case TpPropertiesDialog::Action::NextPoint:
		if (sg_ret::ok != this->current_track->move_selected_tp_forward()) {
			break;
		}

		this->set_dialog_data(this->current_track, this->current_point);
		this->current_track->emit_tree_item_changed("Indicating selecting next trackpoint in track");
		break;

	case TpPropertiesDialog::Action::PreviousPoint:
		if (sg_ret::ok != this->current_track->move_selected_tp_back()) {
			break;
		}

		this->set_dialog_data(this->current_track, this->current_point);
		this->current_track->emit_tree_item_changed("Indicating selecting previous trackpoint in track");
		break;

	case TpPropertiesDialog::Action::InsertTpAfter:
		/* This method makes necessary checks of selected trackpoint.
		   It also triggers redraw of layer. */
		this->current_track->create_tp_next_to_selected_tp(false);
		break;

	default:
		qDebug() << SG_PREFIX_E << "Unexpected dialog action" << action;
		break;
	}
}




void TpPropertiesDialog::set_coord_mode(CoordMode coord_mode)
{
	/* TODO: implement. */
}




TpPropertiesWidget::TpPropertiesWidget(QWidget * parent_widget) : PointPropertiesWidget(parent_widget)
{
}




sg_ret TpPropertiesWidget::build_widgets(QWidget * parent_widget)
{
	/* Properties of text labels that display non-editable
	   trackpoint properties. */
	const Qt::TextInteractionFlags value_display_label_flags = Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard;

	const int left_col = 0;
	const int right_col = 1;

	this->widgets_row = 0;

	this->PointPropertiesWidget::build_widgets(parent_widget);

	this->course = new QLabel("");
	this->course->setTextInteractionFlags(value_display_label_flags);
	this->grid->addWidget(new QLabel(tr("Course:")), this->widgets_row, left_col);
	this->grid->addWidget(this->course, this->widgets_row, right_col);

	this->widgets_row++;

	this->diff_dist = new QLabel("");
	this->diff_dist->setTextInteractionFlags(value_display_label_flags);
	this->grid->addWidget(new QLabel(tr("Distance Difference:")), this->widgets_row, left_col);
	this->grid->addWidget(this->diff_dist, this->widgets_row, right_col);

	this->widgets_row++;

	this->diff_time = new QLabel("");
	this->diff_time->setTextInteractionFlags(value_display_label_flags);
	this->grid->addWidget(new QLabel(tr("Time Difference:")), this->widgets_row, left_col);
	this->grid->addWidget(this->diff_time, this->widgets_row, right_col);

	this->widgets_row++;

	this->diff_speed = new QLabel("");
	this->diff_speed->setTextInteractionFlags(value_display_label_flags);
	this->grid->addWidget(new QLabel(tr("\"Speed\" Between:")), this->widgets_row, left_col);
	this->grid->addWidget(this->diff_speed, this->widgets_row, right_col);

	this->widgets_row++;

	this->speed = new QLabel("");
	this->speed->setTextInteractionFlags(value_display_label_flags);
	this->grid->addWidget(new QLabel(tr("Speed:")), this->widgets_row, left_col);
	this->grid->addWidget(this->speed, this->widgets_row, right_col);

	this->widgets_row++;

	this->vdop = new QLabel("");
	this->vdop->setTextInteractionFlags(value_display_label_flags);
	this->grid->addWidget(new QLabel(tr("VDOP:")), this->widgets_row, left_col);
	this->grid->addWidget(this->vdop, this->widgets_row, right_col);

	this->widgets_row++;

	this->hdop = new QLabel("");
	this->hdop->setTextInteractionFlags(value_display_label_flags);
	this->grid->addWidget(new QLabel(tr("HDOP:")), this->widgets_row, left_col);
	this->grid->addWidget(this->hdop, this->widgets_row, right_col);

	this->widgets_row++;

	this->pdop = new QLabel("");
	this->pdop->setTextInteractionFlags(value_display_label_flags);
	this->grid->addWidget(new QLabel(tr("PDOP:")), this->widgets_row, left_col);
	this->grid->addWidget(this->pdop, this->widgets_row, right_col);

	this->widgets_row++;

	this->sat = new QLabel("");
	this->sat->setTextInteractionFlags(value_display_label_flags);
	this->grid->addWidget(new QLabel(tr("SAT/FIX:")), this->widgets_row, left_col);
	this->grid->addWidget(this->sat, this->widgets_row, right_col);

	this->widgets_row++;


	connect(this->name_entry, SIGNAL (textEdited(const QString &)),                this, SLOT (sync_name_entry_to_current_point_cb(const QString &)));
	connect(this->coord_widget, SIGNAL (value_changed(void)),                      this, SLOT (sync_coord_widget_to_current_point_cb(void)));
	connect(this->altitude_widget->me_widget->spin, SIGNAL (valueChanged(double)), this, SLOT (sync_altitude_widget_to_current_point_cb(void)));
	connect(this->timestamp_widget, SIGNAL (value_is_set(const Time &)),           this, SLOT (sync_timestamp_widget_to_current_point_cb(const Time &)));
	connect(this->timestamp_widget, SIGNAL (value_is_reset()),                     this, SLOT (sync_empty_timestamp_widget_to_current_point_cb(void)));


	return sg_ret::ok;
}




void TpPropertiesWidget::disable_widgets(void)
{
	this->PointPropertiesWidget::disable_widgets();

	/* Only keep Close button enabled. */
	this->button_insert_tp_after->setEnabled(false);
	this->button_split_track->setEnabled(false);
	this->button_delete_current_point->setEnabled(false);
	this->button_previous_point->setEnabled(false);
	this->button_next_point->setEnabled(false);
}




void TpPropertiesWidget::build_buttons(QWidget * parent_widget)
{
	this->button_insert_tp_after = this->button_box_upper->addButton(tr("&Insert After"), QDialogButtonBox::ActionRole);
	this->button_insert_tp_after->setIcon(QIcon::fromTheme("list-add"));
	this->button_split_track = this->button_box_upper->addButton(tr("Split &Here"), QDialogButtonBox::ActionRole);
	this->button_delete_current_point = this->button_box_upper->addButton(tr("&Delete"), QDialogButtonBox::ActionRole);
	this->button_delete_current_point->setIcon(QIcon::fromTheme("list-delete"));

	/*
	  Use "Previous" and "Next" labels because in Waypoint
	  Properties we would like to add similar buttons (for
	  selecting previous/next waypoint within current TRW
	  layer). In context of waypoints the "Back"/"Forward" labels
	  wouldn't make much sense.

	  So for consistency reasons we will use "Previous" and "Next"
	  in both dialogs.
	*/
	this->button_previous_point = this->button_box_lower->addButton(tr("&Previous"), QDialogButtonBox::ActionRole);
	this->button_previous_point->setIcon(QIcon::fromTheme("go-previous"));
	this->button_next_point = this->button_box_lower->addButton(tr("&Next"), QDialogButtonBox::ActionRole);
	this->button_next_point->setIcon(QIcon::fromTheme("go-next"));
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
	connect(this->button_insert_tp_after,      SIGNAL (released()), signal_mapper, SLOT (map()));
	connect(this->button_split_track,          SIGNAL (released()), signal_mapper, SLOT (map()));
	connect(this->button_delete_current_point, SIGNAL (released()), signal_mapper, SLOT (map()));
	connect(this->button_previous_point,       SIGNAL (released()), signal_mapper, SLOT (map()));
	connect(this->button_next_point,           SIGNAL (released()), signal_mapper, SLOT (map()));

	this->signal_mapper->setMapping(this->button_insert_tp_after,      (int) TpPropertiesDialog::Action::InsertTpAfter);
	this->signal_mapper->setMapping(this->button_split_track,          (int) TpPropertiesDialog::Action::SplitAtSelectedTp);
	this->signal_mapper->setMapping(this->button_delete_current_point, (int) TpPropertiesDialog::Action::DeleteSelectedPoint);
	this->signal_mapper->setMapping(this->button_previous_point,       (int) TpPropertiesDialog::Action::PreviousPoint);
	this->signal_mapper->setMapping(this->button_next_point,           (int) TpPropertiesDialog::Action::NextPoint);

	connect(this->signal_mapper, SIGNAL (mapped(int)), this, SLOT (clicked_cb(int)));
}
