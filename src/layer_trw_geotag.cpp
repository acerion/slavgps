/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2011, Rob Norris <rw_norris@hotmail.com>
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




#include <cstdint>
#include <cmath>
#include <time.h>
#include <cstring>
#include <cstdlib>




#include <QCheckBox>
#include <QLineEdit>




#include "widget_file_list.h"
#include "geotag_exif.h"
#include "thumbnails.h"
#include "background.h"
#include "layer_trw_geotag.h"
#include "file_utils.h"
#include "application_state.h"
#include "layer_trw.h"
#include "layer_trw_track_internal.h"
#include "window.h"
#include "statusbar.h"




using namespace SlavGPS;




#define SG_MODULE "Layer TRW GeoTag"




typedef struct {
	bool create_waypoints;
	bool overwrite_waypoints;
	bool write_exif;
	bool overwrite_gps_exif;
	bool no_change_mtime;
	bool interpolate_segments;
	int time_offset;
	int TimeZoneHours;
	int TimeZoneMins;
} GeoTagValues;




static void save_default_values(const GeoTagValues & values);
static GeoTagValues get_default_values(void);




/* Function taken from GPSCorrelate 1.6.1.
   ConvertToUnixTime Copyright 2005 Daniel Foote. GPL2+. */

#define EXIF_DATE_FORMAT "%d:%d:%d %d:%d:%d"

time_t ConvertToUnixTime(const char * StringTime, const char * Format, int TZOffsetHours, int TZOffsetMinutes)
{
	/* Read the time using the specified format.  The format and
	   string being read from must have the most significant time
	   on the left, and the least significant on the right: ie,
	   Year on the left, seconds on the right. */

	/* Sanity check... */
	if (StringTime == NULL || Format == NULL) {
		return 0;
	}

	/* Define and set up our structure. */
	struct tm result;
	result.tm_wday = 0;
	result.tm_yday = 0;
	result.tm_isdst = -1;

	/* Read out the time from the string using our format. */
	sscanf(StringTime, Format, &result.tm_year, &result.tm_mon,
	       &result.tm_mday, &result.tm_hour,
	       &result.tm_min, &result.tm_sec);

	/* Adjust the years for the mktime function to work. */
	result.tm_year -= 1900;
	result.tm_mon  -= 1;

	/* Add our timezone offset to the time.  We don't check to see
	   if it overflowed anything; mktime does this and fixes it
	   for us. */
	/* Note also that we SUBTRACT these times. We want the result
	   to be in UTC. */

	result.tm_hour -= TZOffsetHours;
	result.tm_min  -= TZOffsetMinutes;

	/* Calculate and return the unix time. */
	return mktime(&result);
}
/* GPSCorrelate END */




GeoTagDialog::~GeoTagDialog()
{
	/* We don't have to delete GeoTagDialog::files_selection
	   because it is been automatically when dialog is deleted.
	   Either because the dialog is parent object of
	   GeoTagDialog::files_selection object (and QObject's magic
	   has worked), or because the dialog has been put into Qt
	   layout (and Qt layout magic has worked). */
}




class GeotagJob : public BackgroundJob {
public:
	GeotagJob(GeoTagDialog * dialog);
	~GeotagJob();

	void run(void);


	void geotag_image(const QString & file_full_path);
	sg_ret geotag_image_from_waypoint(const QString & file_full_path, const Coord & coord, const Altitude & altitude);
	sg_ret geotag_image_from_track(Track * trk);

	QStringList selected_images;

	LayerTRW * trw = NULL;
	Track * trk = NULL;      /* Use specified track or all tracks if NULL. */
	Waypoint * wp = NULL;    /* Use specified waypoint or otherwise the track(s) if NULL. */

	/* User options... */
	GeoTagValues values;

	Time photo_time;
	/* Store answer from interpolation for an image. */
	bool found_match = false;
	Coord coord;
	Altitude altitude;
	/* If anything has changed. */
	bool redraw = false;
};




GeotagJob::GeotagJob(GeoTagDialog * dialog)
{
	this->trw = dialog->trw;
	this->wp = dialog->wp;
	this->trk = dialog->trk;

	/* Values extracted from the dialog: */
	this->values.create_waypoints     = dialog->create_waypoints_cb->isChecked();
	this->values.overwrite_waypoints  = dialog->overwrite_waypoints_cb->isChecked();
	this->values.write_exif           = dialog->write_exif_cb->isChecked();
	this->values.overwrite_gps_exif   = dialog->overwrite_gps_exif_cb->isChecked();
	this->values.no_change_mtime      = dialog->no_change_mtime_cb->isChecked();
	this->values.interpolate_segments = dialog->interpolate_segments_cb->isChecked();

	this->values.TimeZoneHours = 0;
	this->values.TimeZoneMins = 0;
	const QString TZString = dialog->time_zone_entry->text();
	/* Check the string. If there is a colon, then (hopefully) it's a time in xx:xx format.
	   If not, it's probably just a +/-xx format. In all other cases,
	   it will be interpreted as +/-xx, which, if given a string, returns 0. */
	if (TZString.contains(":")) {
		/* Found colon. Split into two. */
		const QStringList elems = TZString.split(":");
		if (elems.size() == 2) {
			this->values.TimeZoneHours = elems.at(0).toInt();
			this->values.TimeZoneMins = elems.at(1).toInt();

			if (this->values.TimeZoneHours < 0) {
				this->values.TimeZoneMins *= -1;
			}
			qDebug() << SG_PREFIX_I << "String" << TZString << "parsed as" << this->values.TimeZoneHours << this->values.TimeZoneMins;
		} else {
			qDebug() << SG_PREFIX_E << "String" << TZString << "can't be parsed";
		}
	} else {
		/* No colon. Just parse as int. */
		this->values.TimeZoneHours = TZString.toInt();
	}
	this->values.time_offset = dialog->time_offset_entry->text().toInt();

	this->redraw = false;

	/* Save settings for reuse. */
	save_default_values(this->values);

	this->selected_images.clear();
	this->selected_images = dialog->files_selection->get_list();

	this->n_items = this->selected_images.size();

	this->photo_time = Time(0); /* This will set timestamp as valid. */
}




#define VIK_SETTINGS_GEOTAG_CREATE_WAYPOINT      "geotag_create_waypoints"
#define VIK_SETTINGS_GEOTAG_OVERWRITE_WAYPOINTS  "geotag_overwrite_waypoints"
#define VIK_SETTINGS_GEOTAG_WRITE_EXIF           "geotag_write_exif"
#define VIK_SETTINGS_GEOTAG_OVERWRITE_GPS_EXIF   "geotag_overwrite_gps"
#define VIK_SETTINGS_GEOTAG_NO_CHANGE_MTIME      "geotag_no_change_mtime"
#define VIK_SETTINGS_GEOTAG_INTERPOLATE_SEGMENTS "geotag_interpolate_segments"
#define VIK_SETTINGS_GEOTAG_TIME_OFFSET          "geotag_time_offset"
#define VIK_SETTINGS_GEOTAG_TIME_OFFSET_HOURS    "geotag_time_offset_hours"
#define VIK_SETTINGS_GEOTAG_TIME_OFFSET_MINS     "geotag_time_offset_mins"




static void save_default_values(const GeoTagValues & values)
{
	ApplicationState::set_boolean(VIK_SETTINGS_GEOTAG_CREATE_WAYPOINT,      values.create_waypoints);
	ApplicationState::set_boolean(VIK_SETTINGS_GEOTAG_OVERWRITE_WAYPOINTS,  values.overwrite_waypoints);
	ApplicationState::set_boolean(VIK_SETTINGS_GEOTAG_WRITE_EXIF,           values.write_exif);
	ApplicationState::set_boolean(VIK_SETTINGS_GEOTAG_OVERWRITE_GPS_EXIF,   values.overwrite_gps_exif);
	ApplicationState::set_boolean(VIK_SETTINGS_GEOTAG_NO_CHANGE_MTIME,      values.no_change_mtime);
	ApplicationState::set_boolean(VIK_SETTINGS_GEOTAG_INTERPOLATE_SEGMENTS, values.interpolate_segments);
	ApplicationState::set_integer(VIK_SETTINGS_GEOTAG_TIME_OFFSET,          values.time_offset);
	ApplicationState::set_integer(VIK_SETTINGS_GEOTAG_TIME_OFFSET_HOURS,    values.TimeZoneHours);
	ApplicationState::set_integer(VIK_SETTINGS_GEOTAG_TIME_OFFSET_MINS,     values.TimeZoneMins);
}




static GeoTagValues get_default_values(void)
{
	GeoTagValues default_values;
	if (!ApplicationState::get_boolean(VIK_SETTINGS_GEOTAG_CREATE_WAYPOINT, &default_values.create_waypoints)) {
		default_values.create_waypoints = true;
	}

	if (!ApplicationState::get_boolean(VIK_SETTINGS_GEOTAG_OVERWRITE_WAYPOINTS, &default_values.overwrite_waypoints)) {
		default_values.overwrite_waypoints = true;
	}

	if (!ApplicationState::get_boolean(VIK_SETTINGS_GEOTAG_WRITE_EXIF, &default_values.write_exif)) {
		default_values.write_exif = true;
	}

	if (!ApplicationState::get_boolean(VIK_SETTINGS_GEOTAG_OVERWRITE_GPS_EXIF, &default_values.overwrite_gps_exif)) {
		default_values.overwrite_gps_exif = false;
	}

	if (!ApplicationState::get_boolean(VIK_SETTINGS_GEOTAG_NO_CHANGE_MTIME, &default_values.no_change_mtime)) {
		default_values.no_change_mtime = true;
	}

	if (!ApplicationState::get_boolean(VIK_SETTINGS_GEOTAG_INTERPOLATE_SEGMENTS, &default_values.interpolate_segments)) {
		default_values.interpolate_segments = true;
	}

	if (!ApplicationState::get_integer(VIK_SETTINGS_GEOTAG_TIME_OFFSET, &default_values.time_offset)) {
		default_values.time_offset = 0;
	}

	if (!ApplicationState::get_integer(VIK_SETTINGS_GEOTAG_TIME_OFFSET_HOURS, &default_values.TimeZoneHours)) {
		default_values.TimeZoneHours = 0;
	}

	if (!ApplicationState::get_integer(VIK_SETTINGS_GEOTAG_TIME_OFFSET_MINS, &default_values.TimeZoneMins)) {
		default_values.TimeZoneMins = 0;
	}

	return default_values;
}




/**
   Correlate the image against the specified track.
*/
sg_ret GeotagJob::geotag_image_from_track(Track * trk2)
{
	/* If already found match then don't need to check this track. */
	if (this->found_match) {
		return sg_ret::ok;
	}

	const HeightUnit height_unit = HeightUnit::Metres;

	for (auto iter = trk2->begin(); iter != trk2->end(); iter++) {

		Trackpoint * tp = *iter;
		if (!tp->timestamp.is_valid()) {
			continue;
		}

		/* Is it exactly this point? */
		if (this->photo_time == tp->timestamp) {
			this->coord = tp->coord;
			this->altitude = tp->altitude;
			this->found_match = true;
			break;
		}

		/* Now need two trackpoints, hence check if next tp is available. */
		if (std::next(iter) == trk2->end()) {
			break;
		}

		Trackpoint * tp_next = *std::next(iter);
		if (!tp_next->timestamp.is_valid()) {
			continue;
		}

		if (tp->timestamp == tp_next->timestamp) {
			/* Skip this timestamp, we have already
			   compared against this value. */
			continue;
		}
		if (tp->timestamp > tp_next->timestamp) {
			/* Skip this out-of-order timestamp. */
			continue;
		}

		/* When interpolating between segments, no need for any special segment handling. */
		if (!this->values.interpolate_segments) {
			/* Don't check between segments. */
			if (tp_next->newsegment) {
				/* Simply move on to consider next point. */
				continue;
			}
		}

		/* Too far. */
		if (tp->timestamp > this->photo_time) {
			break;
		}

		/* Is is between this and the next point? */
		if ((this->photo_time > tp->timestamp) && (this->photo_time < tp_next->timestamp)) {
			this->found_match = true;
			/* Interpolate. */
			/* Calculate the "scale": a decimal giving the
			   relative distance in time between the two
			   points. Ie, a number between 0 and 1 - 0 is
			   the first point, 1 is the next point, and
			   0.5 would be half way. */
			const Time up = this->photo_time - tp->timestamp;
			const Time down = tp_next->timestamp - tp->timestamp;
			const double scale = up.get_value() / (1.0 * down.get_value());

			/* Interpolate coordinate. */
			const LatLon interpolated = LatLon::get_interpolated(tp->coord.get_lat_lon(), tp_next->coord.get_lat_lon(), scale);
			this->coord = Coord(interpolated, CoordMode::LatLon);

			/* Interpolate elevation. */
			this->altitude = tp->altitude + ((tp_next->altitude - tp->altitude) * scale);
			break;
		}
	}

	return sg_ret::ok;
}




/**
   Simply align the images the waypoint position
*/
sg_ret GeotagJob::geotag_image_from_waypoint(const QString & file_full_path, const Coord & wp_coord, const Altitude & wp_altitude)
{
	if (!this->values.write_exif) {
		return sg_ret::ok;
	}

	sg_ret retv = sg_ret::ok;

	const bool has_gps_exif = GeotagExif::object_has_gps_info(file_full_path);
	const QString datetime = GeotagExif::get_object_datetime(file_full_path);

	/* If image already has gps info - don't attempt to change it unless forced. */
	if (this->values.overwrite_gps_exif || !has_gps_exif) {
		const sg_ret ans = GeotagExif::write_exif_gps(file_full_path, wp_coord, wp_altitude, this->values.no_change_mtime);
		if (ans != sg_ret::ok) {
			this->trw->get_window()->statusbar_update(StatusBarField::Info, tr("Failed updating EXIF on %1").arg(file_full_path));
		}
		retv = ans;
	}

	return retv;
}




/**
   Correlate the image to any track within the TrackWaypoint layer
*/
void GeotagJob::geotag_image(const QString & file_full_path)
{
	if (!this->trw) {
		return;
	}

	if (file_full_path.isEmpty()) {
		return;
	}

	if (this->wp) {
		this->geotag_image_from_waypoint(file_full_path, this->wp->coord, this->wp->altitude);
		return;
	}

	const bool has_gps_exif = GeotagExif::object_has_gps_info(file_full_path);
	const QString datetime = GeotagExif::get_object_datetime(file_full_path);
	if (datetime.isEmpty()) {
		return;
	}

	/* If image already has gps info - don't attempt to change it. */
	if (!this->values.overwrite_gps_exif && has_gps_exif) {
		if (this->values.create_waypoints) {
			/* Create waypoint with file information. */
			Waypoint * new_wp = GeotagExif::create_waypoint_from_file(file_full_path, this->trw->get_coord_mode());
			if (!new_wp) {
				/* Couldn't create Waypoint. */
				return;
			}
			if (new_wp->name.isEmpty()) {
				/* GeotagExif method doesn't guarantee setting waypoints name. */
				new_wp->set_name(file_base_name(file_full_path));
			}

			QString new_wp_name = new_wp->name;

			bool updated_existing_waypoint = false;

			if (this->values.overwrite_waypoints) {
				Waypoint * current_wp = this->trw->get_waypoints_node().find_waypoint_by_name(new_wp_name);
				if (current_wp) {
					/* Existing wp found, so set new position, comment and image. */
					current_wp->coord = new_wp->coord;
					current_wp->altitude = new_wp->altitude;
					current_wp->set_image_full_path(file_full_path);
					current_wp->comment = GeotagExif::get_object_comment(file_full_path);

					new_wp_name = GeotagExif::get_object_name(file_full_path);
					updated_existing_waypoint = true;
				}
			}

			if (!updated_existing_waypoint) {
				new_wp->set_name(new_wp_name);
				this->trw->add_waypoint_from_file(new_wp);
				/* TODO_2_LATER: do we delete the new_wp if we don't add it to layer? How was it done in Viking? */
			}

			/* Mark for redraw. */
			this->redraw = true;
		}
		return;
	}

	this->photo_time.value = ConvertToUnixTime(datetime.toUtf8().data(), (char *) EXIF_DATE_FORMAT, this->values.TimeZoneHours, this->values.TimeZoneMins);

	/* Apply any offset. */
	this->photo_time.value += this->values.time_offset;

	this->found_match = false;

	if (this->trk) {
		/* Single specified track. */
		this->geotag_image_from_track(this->trk);
	} else {
		/* Try all tracks. */
		const std::list<Track *> & tracks = this->trw->get_tracks();
		for (auto iter = tracks.begin(); iter != tracks.end(); iter++) {
			this->geotag_image_from_track(*iter);
		}
	}

	if (this->found_match) {

		if (this->values.create_waypoints) {

			bool updated_existing_waypoint = false;

			if (this->values.overwrite_waypoints) {

				/* Update existing WP. */
				/* Find a WP with current name. */
				QString wp_name = file_base_name(file_full_path);
				Waypoint * wp2 = this->trw->get_waypoints_node().find_waypoint_by_name(wp_name);
				if (wp2) {
					/* Found, so set new position, image and (below) a comment. */
					wp2->coord = this->coord;
					wp2->altitude = this->altitude;
					wp2->set_image_full_path(file_full_path);
					wp2->comment = GeotagExif::get_object_comment(file_full_path);

					/* We ignore the returned wp name because the existing wp
					   already has a name. We only update waypoint's comment. */
					wp_name = GeotagExif::get_object_name(file_full_path);
					updated_existing_waypoint = true;
				}
			}

			if (!updated_existing_waypoint) {
				/* Create waypoint with found position. */

				Waypoint * wp2 = new Waypoint();
				wp2->coord = this->coord;
				wp2->altitude = this->altitude;
				wp2->set_image_full_path(file_full_path);
				wp2->comment = GeotagExif::get_object_name(file_full_path);

				/* Brand new wp, so we don't ignore wp name returned by this function.
				   If the name is not empty, we will use it for the wp. */
				QString wp_name = GeotagExif::get_object_name(file_full_path);
				if (wp_name.isEmpty()) {
					wp_name = file_base_name(file_full_path);
				}
				wp2->set_name(wp_name);
				this->trw->add_waypoint_from_file(wp2);
			}

			/* Mark for redraw. */
			this->redraw = true;
		}

		/* Write EXIF if specified. */
		if (this->values.write_exif) {
			const sg_ret ans = GeotagExif::write_exif_gps(file_full_path, this->coord, this->altitude, this->values.no_change_mtime);
			if (ans != sg_ret::ok) {
				this->trw->get_window()->statusbar_update(StatusBarField::Info, tr("Failed updating EXIF on %1").arg(file_full_path));
			}
		}
	}
}




GeotagJob::~GeotagJob()
{

}




/**
   Run geotagging process in a separate thread.
*/
void GeotagJob::run(void)
{
	const int n_files = this->selected_images.size();
	int done = 0;

	/* TODO_LATER decide how to report any issues to the user... */

	for (auto iter = this->selected_images.begin(); iter != this->selected_images.end(); iter++) {
		/* For each file attempt to geotag it. */
		this->geotag_image(*iter);

		/* Update thread progress and detect stop requests. */
		const bool end_job = this->set_progress_state(((double) ++done) / n_files);
		if (end_job) {
			return; /* Abort thread */
		}
	}

	if (this->redraw) {
		if (this->trw) {
			this->trw->get_waypoints_node().recalculate_bbox();
			/* Ensure any new images get show. */
			this->trw->generate_missing_thumbnails();
			/* Force redraw as verify only redraws if there are new thumbnails (they may already exist). */
			this->trw->emit_tree_item_changed("TRW Geotag - run"); /* Update from background. */
		}
	}

	return;
}




/**
   Parse user input from dialog response
*/
void GeoTagDialog::on_accept_cb(void)
{
	GeotagJob * geotag_job = new GeotagJob(this);
	int len = geotag_job->selected_images.size();

	const QString job_description = QObject::tr("Geotagging %1 Images...").arg(len);
	geotag_job->set_description(job_description);

	/* Processing lots of files can take time - so run a background effort. */
	geotag_job->run_in_background(ThreadPoolType::Local);
}




/**
   Handle widget sensitivities
*/
void GeoTagDialog::write_exif_cb_cb(void)
{
	/* Overwriting & file modification times are irrelevant if not going to write EXIF! */
	if (this->write_exif_cb->isChecked()) {
		this->overwrite_gps_exif_l->setEnabled(true);
		this->overwrite_gps_exif_cb->setEnabled(true);

		this->no_change_mtime_l->setEnabled(true);
		this->no_change_mtime_cb->setEnabled(true);
	} else {
		this->overwrite_gps_exif_l->setEnabled(false);
		this->overwrite_gps_exif_cb->setEnabled(false);

		this->no_change_mtime_l->setEnabled(false);
		this->no_change_mtime_cb->setEnabled(false);
	}
}




void GeoTagDialog::create_waypoints_cb_cb(void)
{
	/* Overwriting waypoints are irrelevant if not going to create them! */
	if (this->create_waypoints_cb->isChecked()) {
		this->overwrite_waypoints_cb->setEnabled(true);
		this->overwrite_waypoints_l->setEnabled(true);
	} else {
		this->overwrite_waypoints_cb->setEnabled(false);
		this->overwrite_waypoints_l->setEnabled(false);
	}
}




/**
   @parent: The Window of the calling process
   @trw: The LayerTrw to use for correlating images to tracks
   @trk: Optional - The particular track to use (if specified) for correlating images
 */
void SlavGPS::trw_layer_geotag_dialog(Window * parent, LayerTRW * trw, Waypoint * wp, Track * trk)
{
	GeoTagDialog * dialog = new GeoTagDialog(parent);
	dialog->setWindowTitle(QObject::tr("Geotag Images"));

	dialog->trw = trw;
	dialog->wp = wp;
	dialog->trk = trk;

	int row = 1;

	const QStringList file_list;
	dialog->files_selection = new FileListWidget(QObject::tr("Images"), file_list, dialog);
	dialog->files_selection->set_file_type_filter(FileSelectorWidget::FileTypeFilter::JPEG);
	dialog->grid->addWidget(dialog->files_selection, 1, 0, 1, 2);
	row++;


	dialog->create_waypoints_l = new QLabel(QObject::tr("Create Waypoints:"));
	dialog->create_waypoints_cb = new QCheckBox();
	dialog->grid->addWidget(dialog->create_waypoints_l, row, 0);
	dialog->grid->addWidget(dialog->create_waypoints_cb, row, 1);
	row++;


	dialog->overwrite_waypoints_l = new QLabel(QObject::tr("Overwrite Existing Waypoints:"));
	dialog->overwrite_waypoints_cb = new QCheckBox();
	dialog->grid->addWidget(dialog->overwrite_waypoints_l, row, 0);
	dialog->grid->addWidget(dialog->overwrite_waypoints_cb, row, 1);
	row++;


	dialog->write_exif_cb = new QCheckBox();
	dialog->grid->addWidget(new QLabel(QObject::tr("Write EXIF:")), row, 0);
	dialog->grid->addWidget(dialog->write_exif_cb, row, 1);
	row++;


	dialog->overwrite_gps_exif_l = new QLabel(QObject::tr("Overwrite Existing GPS Information:"));
	dialog->overwrite_gps_exif_cb = new QCheckBox();
	dialog->grid->addWidget(dialog->overwrite_gps_exif_l, row, 0);
	dialog->grid->addWidget(dialog->overwrite_gps_exif_cb, row, 1);
	row++;


	dialog->no_change_mtime_l = new QLabel(QObject::tr("Keep File Modification Timestamp:"));
	dialog->no_change_mtime_cb = new QCheckBox();
	dialog->grid->addWidget(dialog->no_change_mtime_l, row, 0);
	dialog->grid->addWidget(dialog->no_change_mtime_cb, row, 1);
	row++;


	QLabel * interpolate_segments_l = new QLabel(QObject::tr("Interpolate Between Track Segments:"));
	dialog->interpolate_segments_cb = new QCheckBox();
	dialog->grid->addWidget(interpolate_segments_l, row, 0);
	dialog->grid->addWidget(dialog->interpolate_segments_cb, row, 1);
	row++;


	QLabel * time_offset_l = new QLabel(QObject::tr("Image Time Offset (Seconds):"));
	dialog->time_offset_entry = new QLineEdit();
	dialog->grid->addWidget(time_offset_l, row, 0);
	dialog->grid->addWidget(dialog->time_offset_entry, row, 1);
	dialog->time_offset_entry->setToolTip(QObject::tr("The number of seconds to ADD to the photos time to make it match the GPS data. Calculate this with (GPS - Photo). Can be negative or positive. Useful to adjust times when a camera's timestamp was incorrect."));
	row++;


	QLabel * time_zone_l = new QLabel(QObject::tr("Image Timezone:"));
	dialog->time_zone_entry = new QLineEdit();
	dialog->grid->addWidget(time_zone_l, row, 0);
	dialog->grid->addWidget(dialog->time_zone_entry, row, 1);
	dialog->time_zone_entry->setToolTip(QObject::tr("The timezone that was used when the images were created. For example, if a camera is set to AWST or +8:00 hours. Enter +8:00 here so that the correct adjustment to the images' time can be made. GPS data is always in UTC."));
	dialog->time_zone_entry->setMaxLength(7);
	row++;





	/* Set default values of ui controls. */
	GeoTagValues default_values = get_default_values();

	dialog->create_waypoints_cb->setChecked(default_values.create_waypoints);
	dialog->overwrite_waypoints_cb->setChecked(default_values.overwrite_waypoints);
	dialog->write_exif_cb->setChecked(default_values.write_exif);
	dialog->overwrite_gps_exif_cb->setChecked(default_values.overwrite_gps_exif);
	dialog->no_change_mtime_cb->setChecked(default_values.no_change_mtime);
	dialog->interpolate_segments_cb->setChecked(default_values.interpolate_segments);

	char tmp_string[10];
	snprintf(tmp_string, sizeof (tmp_string), "%+02d:%02d", default_values.TimeZoneHours, abs(default_values.TimeZoneMins));
	dialog->time_zone_entry->setText(tmp_string);
	snprintf(tmp_string, sizeof (tmp_string), "%d", default_values.time_offset);
	dialog->time_offset_entry->setText(tmp_string);

	/* Ensure sensitivities setup. */
	dialog->write_exif_cb_cb();
	dialog->create_waypoints_cb_cb();

	QObject::connect(dialog->write_exif_cb, SIGNAL (toggled(bool)), dialog, SLOT (write_exif_cb_cb(void)));
	QObject::connect(dialog->create_waypoints_cb, SIGNAL (toggled(bool)), dialog, SLOT (create_waypoints_cb_cb(void)));


	QString track_string;
	if (dialog->wp) {
		track_string = QObject::tr("Using waypoint: %1").arg(wp->name);

		/* Control sensitivities. */
		dialog->create_waypoints_l->setEnabled(false);
		dialog->create_waypoints_cb->setEnabled(false);


		dialog->overwrite_waypoints_l->setEnabled(false);
		dialog->overwrite_waypoints_cb->setEnabled(false);

		interpolate_segments_l->setEnabled(false);
		dialog->interpolate_segments_cb->setEnabled(false);

		time_offset_l->setEnabled(false);
		dialog->time_offset_entry->setEnabled(false);

		time_zone_l->setEnabled(false);
		dialog->time_zone_entry->setEnabled(false);
	} else if (dialog->trk) {
		track_string = QObject::tr("Using track: %1").arg(trk->name);
	} else {
		track_string = QObject::tr("Using all tracks in: %1").arg(trw->name);
	}

	row = 0;
	dialog->create_waypoints_cb = new QCheckBox();
	dialog->grid->addWidget(new QLabel(track_string), row, 0, 1, 2);


	QObject::connect(dialog->button_box, SIGNAL (accepted(void)), dialog, SLOT (on_accept_cb(void)));
	dialog->button_box->button(QDialogButtonBox::Cancel)->setDefault(true);

	dialog->exec();


	/* TODO_LATER: is it safe to delete dialog here? There is a
	   background job running, started in ::on_accept_cb(), that
	   may be using some data from the dialog. */
	delete dialog;
}
