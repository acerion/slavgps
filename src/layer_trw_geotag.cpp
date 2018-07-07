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




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
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




#define PREFIX ": Layer TRW GeoTag:" << __FUNCTION__ << __LINE__ << ">"




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




static void save_default_values(GeoTagValues values);
static GeoTagValues get_default_values(void);




/* Function taken from GPSCorrelate 1.6.1.
   ConvertToUnixTime Copyright 2005 Daniel Foote. GPL2+. */

#define EXIF_DATE_FORMAT "%d:%d:%d %d:%d:%d"

time_t ConvertToUnixTime(char * StringTime, char * Format, int TZOffsetHours, int TZOffsetMinutes)
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
	struct tm Time;
	Time.tm_wday = 0;
	Time.tm_yday = 0;
	Time.tm_isdst = -1;

	/* Read out the time from the string using our format. */
	sscanf(StringTime, Format, &Time.tm_year, &Time.tm_mon,
	       &Time.tm_mday, &Time.tm_hour,
	       &Time.tm_min, &Time.tm_sec);

	/* Adjust the years for the mktime function to work. */
	Time.tm_year -= 1900;
	Time.tm_mon  -= 1;

	/* Add our timezone offset to the time.  We don't check to see
	   if it overflowed anything; mktime does this and fixes it
	   for us. */
	/* Note also that we SUBTRACT these times. We want the result
	   to be in UTC. */

	Time.tm_hour -= TZOffsetHours;
	Time.tm_min  -= TZOffsetMinutes;

	/* Calculate and return the unix time. */
	return mktime(&Time);
}
/* GPSCorrelate END */




GeoTagDialog::~GeoTagDialog()
{
	/* TODO: Need to free SGFileList?? */
}




class GeotagJob : public BackgroundJob {
public:
	GeotagJob(GeoTagDialog * dialog);
	~GeotagJob();

	void run(void);


	void geotag(void);
	void geotag_waypoint(void);
	void geotag_track(Track * trk);

	QStringList selected_files;
	QString current_file;

	LayerTRW * trw = NULL;
	Track * trk = NULL;      /* Use specified track or all tracks if NULL. */
	Waypoint * wp = NULL;    /* Use specified waypoint or otherwise the track(s) if NULL. */

	/* User options... */
	GeoTagValues values;

	time_t PhotoTime = 0;
	/* Store answer from interpolation for an image. */
	bool found_match = false;
	Coord coord;
	double altitude = 0;
	/* If anything has changed. */
	bool redraw = false;
};




GeotagJob::GeotagJob(GeoTagDialog * dialog)
{
	this->trw = dialog->trw;
	this->wp = dialog->wp;
	this->trk = dialog->trk;

	/* Values extracted from the dialog: */
	this->values.create_waypoints = dialog->create_waypoints_cb->isChecked();
	this->values.overwrite_waypoints = dialog->overwrite_waypoints_cb->isChecked();
	this->values.write_exif = dialog->write_exif_cb->isChecked();
	this->values.overwrite_gps_exif = dialog->overwrite_gps_exif_cb->isChecked();
	this->values.no_change_mtime = dialog->no_change_mtime_cb->isChecked();
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
			qDebug() << "II" PREFIX << "String" << TZString << "parsed as" << this->values.TimeZoneHours << this->values.TimeZoneMins;
		} else {
			qDebug() << "EE" PREFIX << "String" << TZString << "can't be parsed";
		}
	} else {
		/* No colon. Just parse as int. */
		this->values.TimeZoneHours = TZString.toInt();
	}
	this->values.time_offset = dialog->time_offset_entry->text().toInt();

	this->redraw = false;

	/* Save settings for reuse. */
	save_default_values(this->values);

	this->selected_files.clear();
	this->selected_files = dialog->files_selection->get_list();

	this->n_items = this->selected_files.size();
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




static void save_default_values(GeoTagValues values)
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
void GeotagJob::geotag_track(Track * trk2)
{
	/* If already found match then don't need to check this track. */
	if (this->found_match) {
		return;
	}

	for (auto iter = trk2->begin(); iter != trk2->end(); iter++) {

		Trackpoint * tp = *iter;

		/* Is it exactly this point? */
		if (this->PhotoTime == tp->timestamp) {
			this->coord = tp->coord;
			this->altitude = tp->altitude;
			this->found_match = true;
			break;
		}

		/* Now need two trackpoints, hence check next is available. */
		if (std::next(iter) == trk2->end()) {
			break;
		}

		Trackpoint * tp_next = *std::next(iter);

		/* TODO need to use 'has_timestamp' property. */
		if (tp->timestamp == tp_next->timestamp) {
			continue;
		}
		if (tp->timestamp > tp_next->timestamp) {
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
		if (tp->timestamp > this->PhotoTime) {
			break;
		}

		/* Is is between this and the next point? */
		if ((this->PhotoTime > tp->timestamp) && (this->PhotoTime < tp_next->timestamp)) {
			this->found_match = true;
			/* Interpolate. */
			/* Calculate the "scale": a decimal giving the
			   relative distance in time between the two
			   points. Ie, a number between 0 and 1 - 0 is
			   the first point, 1 is the next point, and
			   0.5 would be half way. */
			double scale = (double)tp_next->timestamp - (double)tp->timestamp;
			scale = ((double)this->PhotoTime - (double)tp->timestamp) / scale;

			LatLon ll_result;

			const LatLon ll1 = tp->coord.get_latlon();
			const LatLon ll2 = tp_next->coord.get_latlon();

			ll_result.lat = ll1.lat + ((ll2.lat - ll1.lat) * scale);

			/* This won't cope with going over the 180 degrees longitude boundary. */
			ll_result.lon = ll1.lon + ((ll2.lon - ll1.lon) * scale);

			/* Set coord. */
			this->coord = Coord(ll_result, CoordMode::LATLON);

			/* Interpolate elevation. */
			this->altitude = tp->altitude + ((tp_next->altitude - tp->altitude) * scale);
			break;
		}
	}
}




/**
   Simply align the images the waypoint position
*/
void GeotagJob::geotag_waypoint(void)
{
	/* Write EXIF if specified - although a fairly useless process if you've turned it off! */
	if (this->values.write_exif) {
		bool has_gps_exif = false;

		const QString datetime = GeotagExif::get_exif_date_from_file(this->current_file, &has_gps_exif);
		/* If image already has gps info - don't attempt to change it unless forced. */
		if (this->values.overwrite_gps_exif || !has_gps_exif) {
			int ans = GeotagExif::write_exif_gps(this->current_file, this->wp->coord, this->wp->altitude, this->values.no_change_mtime);
			if (ans != 0) {
				this->trw->get_window()->statusbar_update(StatusBarField::INFO, QString("Failed updating EXIF on %1").arg(this->current_file));
			}
		}
	}
}




/**
   Correlate the image to any track within the TrackWaypoint layer
*/
void GeotagJob::geotag(void)
{
	if (!this->trw) {
		return;
	}

	if (this->current_file.isEmpty()) {
		return;
	}

	if (this->wp) {
		this->geotag_waypoint();
		return;
	}

	bool has_gps_exif = false;

	const QString datetime = GeotagExif::get_exif_date_from_file(this->current_file, &has_gps_exif);
	if (datetime.isEmpty()) {
		return;
	}

	/* If image already has gps info - don't attempt to change it. */
	if (!this->values.overwrite_gps_exif && has_gps_exif) {
		if (this->values.create_waypoints) {
			/* Create waypoint with file information. */
			Waypoint * new_wp = GeotagExif::create_waypoint_from_file(this->current_file, this->trw->get_coord_mode());
			if (!new_wp) {
				/* Couldn't create Waypoint. */
				return;
			}
			if (new_wp->name.isEmpty()) {
				/* GeotagExif method doesn't guarantee setting waypoints name. */
				new_wp->set_name(file_base_name(this->current_file));
			}

			QString new_wp_name = new_wp->name;

			bool updated_existing_waypoint = false;

			if (this->values.overwrite_waypoints) {
				Waypoint * current_wp = this->trw->get_waypoints_node().find_waypoint_by_name(new_wp_name);
				if (current_wp) {
					/* Existing wp found, so set new position, comment and image. */
					current_wp->coord = new_wp->coord;
					current_wp->altitude = new_wp->altitude;
					current_wp->set_image_full_path(this->current_file);

					new_wp_name = GeotagExif::waypoint_set_comment_get_name(this->current_file, current_wp);
					updated_existing_waypoint = true;
				}
			}

			if (!updated_existing_waypoint) {
				new_wp->set_name(new_wp_name);
				this->trw->add_waypoint_from_file(new_wp);
				/* TODO: do we delete the new_wp if we don't add it to layer? How was it done in Viking? */
			}

			/* Mark for redraw. */
			this->redraw = true;
		}
		return;
	}

	this->PhotoTime = ConvertToUnixTime(datetime.toUtf8().data(), (char *) EXIF_DATE_FORMAT, this->values.TimeZoneHours, this->values.TimeZoneMins);

	/* Apply any offset. */
	this->PhotoTime = this->PhotoTime + this->values.time_offset;

	this->found_match = false;

	if (this->trk) {
		/* Single specified track. */
		this->geotag_track(this->trk);
	} else {
		/* Try all tracks. */
		const std::list<Track *> & tracks = this->trw->get_tracks();
		for (auto iter = tracks.begin(); iter != tracks.end(); iter++) {
			this->geotag_track(*iter);
		}
	}

	/* Match found? */
	if (this->found_match) {

		if (this->values.create_waypoints) {

			bool updated_existing_waypoint = false;

			if (this->values.overwrite_waypoints) {

				/* Update existing WP. */
				/* Find a WP with current name. */
				QString wp_name = file_base_name(this->current_file);
				Waypoint * wp2 = this->trw->get_waypoints_node().find_waypoint_by_name(wp_name);
				if (wp2) {
					/* Found, so set new position, image and (below) a comment. */
					wp2->coord = this->coord;
					wp2->altitude = this->altitude;
					wp2->set_image_full_path(this->current_file);

					/* We ignore the returned wp name because the existing wp
					   already has a name. We only update waypoint's comment. */
					wp_name = GeotagExif::waypoint_set_comment_get_name(this->current_file, wp2);
					updated_existing_waypoint = true;
				}
			}

			if (!updated_existing_waypoint) {
				/* Create waypoint with found position. */

				Waypoint * wp2 = new Waypoint();
				wp2->visible = true;
				wp2->coord = this->coord;
				wp2->altitude = this->altitude;
				wp2->set_image_full_path(this->current_file);

				/* Brand new wp, so we don't ignore wp name returned by this function.
				   If the name is not empty, we will use it for the wp. */
				QString wp_name = GeotagExif::waypoint_set_comment_get_name(this->current_file, wp2);
				if (wp_name.isEmpty()) {
					wp_name = file_base_name(this->current_file);
				}
				wp2->set_name(wp_name);
				this->trw->add_waypoint_from_file(wp2);
			}

			/* Mark for redraw. */
			this->redraw = true;
		}

		/* Write EXIF if specified. */
		if (this->values.write_exif) {
			int ans = GeotagExif::write_exif_gps(this->current_file, this->coord, this->altitude, this->values.no_change_mtime);
			if (ans != 0) {
				this->trw->get_window()->statusbar_update(StatusBarField::INFO, QString("Failed updating EXIF on %1").arg(this->current_file));
			}
		}
	}
}




GeotagJob::~GeotagJob()
{
	/* kamilTODO: is that all that we need to clean up? */
}




/**
   Run geotagging process in a separate thread.
*/
void GeotagJob::run(void)
{
	const int n_files = this->selected_files.size();
	int done = 0;

	/* TODO decide how to report any issues to the user... */

	for (auto iter = this->selected_files.begin(); iter != this->selected_files.end(); iter++) {
		/* For each file attempt to geotag it. */
		this->current_file = *iter;
		this->geotag();

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
			this->trw->emit_layer_changed("TRW Geotag - run"); /* Update from background. */
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
	int len = geotag_job->selected_files.size();

	const QString job_description = QObject::tr("Geotagging %1 Images...").arg(len);
	geotag_job->set_description(job_description);

	/* Processing lots of files can take time - so run a background effort. */
	Background::run_in_background(geotag_job, ThreadPoolType::LOCAL);
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
	dialog->files_selection = new SGFileList(QObject::tr("Images"), file_list, dialog);
	// TODO: VIK_FILE_LIST(vik_file_list_new(, mime_type_filters));
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



	QStringList mime_type_filters;
	mime_type_filters << "image/jpeg";

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

	/* TODO: is it safe to delete dialog here? There is a
	   background job running, started in ::on_accept_cb(), that
	   may be using some data from the dialog. */
	delete dialog;
}
