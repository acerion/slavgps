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
 *
 */
/*
 *  Similar to the track and trackpoint properties dialogs,
 *   this is made a separate file for ease of grouping related stuff together
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <cstdint>
#include <cmath>
#include <time.h>
#include <cstring>
#include <cstdlib>

#include <glib.h>

#include <QCheckBox>
#include <QLineEdit>

#include "widget_file_list.h"
#include "geotag_exif.h"
#include "thumbnails.h"
#include "background.h"
#include "layer_trw_geotag.h"
#include "file_utils.h"
#include "application_state.h"
#include "globals.h"
#include "layer_trw.h"
#include "layer_trw_track_internal.h"
#include "window.h"
#include "statusbar.h"




using namespace SlavGPS;




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
} option_values_t;




static int trw_layer_geotag_thread(BackgroundJob * job);
static void save_default_values(option_values_t default_values);




/* Function taken from GPSCorrelate 1.6.1.
   ConvertToUnixTime Copyright 2005 Daniel Foote. GPL2+. */

#define EXIF_DATE_FORMAT "%d:%d:%d %d:%d:%d"

time_t ConvertToUnixTime(char* StringTime, char* Format, int TZOffsetHours, int TZOffsetMinutes)
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




typedef struct {
	BasicDialog * dialog = NULL;
	SGFileList *files = NULL;
	LayerTRW * trw = NULL;      /* To pass on. */
	Waypoint * wp = NULL;       /* Use specified waypoint or otherwise the track(s) if NULL. */
	Track * trk = NULL;         /* Use specified track or all tracks if NULL. */
	QCheckBox *create_waypoints_b = NULL;
	QLabel *overwrite_waypoints_l = NULL;    /* Referenced so the sensitivity can be changed. */
	QCheckBox *overwrite_waypoints_b = NULL;
	QCheckBox *write_exif_b = NULL;
	QLabel *overwrite_gps_exif_l = NULL;   /* Referenced so the sensitivity can be changed. */
	QCheckBox *overwrite_gps_exif_b = NULL;
	QLabel *no_change_mtime_l = NULL;    /* Referenced so the sensitivity can be changed. */
	QCheckBox *no_change_mtime_b = NULL;
	QCheckBox *interpolate_segments_b = NULL;
	QLineEdit time_zone_b;    /* TODO consider a more user friendly tz widget eg libtimezonemap or similar. */
	QLineEdit time_offset_b;
} GeoTagWidgets;




static GeoTagWidgets *geotag_widgets_new()
{
	GeoTagWidgets * widgets = (GeoTagWidgets *) malloc(sizeof (GeoTagWidgets));
	memset(widgets, 0, sizeof (GeoTagWidgets));

	return widgets;
}




static void geotag_widgets_free(GeoTagWidgets *widgets)
{
	/* Need to free SGFileList?? */
	free(widgets);
}




class GeotagJob : public BackgroundJob {
public:
	GeotagJob(GeoTagWidgets * widgets);
	~GeotagJob();

	LayerTRW * trw = NULL;
	char * image = NULL;
	Waypoint * wp = NULL;    /* Use specified waypoint or otherwise the track(s) if NULL. */
	Track * trk = NULL;      /* Use specified track or all tracks if NULL. */
	/* User options... */
	option_values_t ov;
	std::list<char *> * files = NULL;
	time_t PhotoTime = 0;
	/* Store answer from interpolation for an image. */
	bool found_match = false;
	Coord coord;
	double altitude = 0;
	/* If anything has changed. */
	bool redraw = false;
};



GeotagJob::GeotagJob(GeoTagWidgets * widgets)
{
	this->thread_fn = trw_layer_geotag_thread;

	this->trw = widgets->trw;
	this->wp = widgets->wp;
	this->trk = widgets->trk;

	/* Values extracted from the widgets: */
	this->ov.create_waypoints = widgets->create_waypoints_b->isChecked();
	this->ov.overwrite_waypoints = widgets->overwrite_waypoints_b->isChecked();
	this->ov.write_exif = widgets->write_exif_b->isChecked();
	this->ov.overwrite_gps_exif = widgets->overwrite_gps_exif_b->isChecked();
	this->ov.no_change_mtime = widgets->no_change_mtime_b->isChecked();
	this->ov.interpolate_segments = widgets->interpolate_segments_b->isChecked();

	this->ov.TimeZoneHours = 0;
	this->ov.TimeZoneMins = 0;
#ifdef K
	const char * TZString = widgets->time_zone_b.text();
	/* Check the string. If there is a colon, then (hopefully) it's a time in xx:xx format.
	 * If not, it's probably just a +/-xx format. In all other cases,
	 * it will be interpreted as +/-xx, which, if given a string, returns 0. */
	if (strstr(TZString, ":")) {
		/* Found colon. Split into two. */
		sscanf(TZString, "%d:%d", &this->ov.TimeZoneHours, &this->ov.TimeZoneMins);
		if (this->ov.TimeZoneHours < 0) {
			this->ov.TimeZoneMins *= -1;
		}
	} else {
		/* No colon. Just parse. */
		this->ov.TimeZoneHours = atoi(TZString);
	}
#endif
	this->ov.time_offset = atoi(widgets->time_offset_b.text().toUtf8().constData());

	this->redraw = false;

	/* Save settings for reuse. */
	save_default_values(this->ov);

	this->files->clear();
	const QStringList a_list = widgets->files->get_list();

#ifdef K
	this->files->insert(this->files->begin(), a_list->begin(), a_list->end());
#endif

	this->n_items = this->files->size();

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




static void save_default_values(option_values_t default_values)
{
	ApplicationState::set_boolean(VIK_SETTINGS_GEOTAG_CREATE_WAYPOINT,      default_values.create_waypoints);
	ApplicationState::set_boolean(VIK_SETTINGS_GEOTAG_OVERWRITE_WAYPOINTS,  default_values.overwrite_waypoints);
	ApplicationState::set_boolean(VIK_SETTINGS_GEOTAG_WRITE_EXIF,           default_values.write_exif);
	ApplicationState::set_boolean(VIK_SETTINGS_GEOTAG_OVERWRITE_GPS_EXIF,   default_values.overwrite_gps_exif);
	ApplicationState::set_boolean(VIK_SETTINGS_GEOTAG_NO_CHANGE_MTIME,      default_values.no_change_mtime);
	ApplicationState::set_boolean(VIK_SETTINGS_GEOTAG_INTERPOLATE_SEGMENTS, default_values.interpolate_segments);
	ApplicationState::set_integer(VIK_SETTINGS_GEOTAG_TIME_OFFSET,          default_values.time_offset);
	ApplicationState::set_integer(VIK_SETTINGS_GEOTAG_TIME_OFFSET_HOURS,    default_values.TimeZoneHours);
	ApplicationState::set_integer(VIK_SETTINGS_GEOTAG_TIME_OFFSET_MINS,     default_values.TimeZoneMins);
}




static option_values_t get_default_values()
{
	option_values_t default_values;
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
 * Correlate the image against the specified track.
 */
static void trw_layer_geotag_track(Track * trk, GeotagJob * options)
{
	/* If already found match then don't need to check this track. */
	if (options->found_match) {
		return;
	}

	for (auto iter = trk->begin(); iter != trk->end(); iter++) {

		Trackpoint * tp = *iter;

		/* Is it exactly this point? */
		if (options->PhotoTime == tp->timestamp) {
			options->coord = tp->coord;
			options->altitude = tp->altitude;
			options->found_match = true;
			break;
		}

		/* Now need two trackpoints, hence check next is available. */
		if (std::next(iter) == trk->end()) {
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
		if (!options->ov.interpolate_segments) {
			/* Don't check between segments. */
			if (tp_next->newsegment) {
				/* Simply move on to consider next point. */
				continue;
			}
		}

		/* Too far. */
		if (tp->timestamp > options->PhotoTime) {
			break;
		}

		/* Is is between this and the next point? */
		if ((options->PhotoTime > tp->timestamp) && (options->PhotoTime < tp_next->timestamp)) {
			options->found_match = true;
			/* Interpolate. */
			/* Calculate the "scale": a decimal giving the relative distance
			 * in time between the two points. Ie, a number between 0 and 1 -
			 * 0 is the first point, 1 is the next point, and 0.5 would be
			 * half way. */
			double scale = (double)tp_next->timestamp - (double)tp->timestamp;
			scale = ((double)options->PhotoTime - (double)tp->timestamp) / scale;

			LatLon ll_result;

			const LatLon ll1 = tp->coord.get_latlon();
			const LatLon ll2 = tp_next->coord.get_latlon();

			ll_result.lat = ll1.lat + ((ll2.lat - ll1.lat) * scale);

			/* NB This won't cope with going over the 180 degrees longitude boundary. */
			ll_result.lon = ll1.lon + ((ll2.lon - ll1.lon) * scale);

			/* Set coord. */
			options->coord = Coord(ll_result, CoordMode::LATLON);

			/* Interpolate elevation. */
			options->altitude = tp->altitude + ((tp_next->altitude - tp->altitude) * scale);
			break;
		}
	}
}




static void trw_layer_geotag_tracks(Tracks & tracks, GeotagJob * options)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		trw_layer_geotag_track(i->second, options);
	}
}




/**
 * Simply align the images the waypoint position.
 */
static void trw_layer_geotag_waypoint(GeotagJob * options)
{
	/* Write EXIF if specified - although a fairly useless process if you've turned it off! */
	if (options->ov.write_exif) {
		bool has_gps_exif = false;

		char * datetime = a_geotag_get_exif_date_from_file(options->image, &has_gps_exif);
		/* If image already has gps info - don't attempt to change it unless forced. */
		if (options->ov.overwrite_gps_exif || !has_gps_exif) {
			int ans = a_geotag_write_exif_gps(options->image, options->wp->coord, options->wp->altitude, options->ov.no_change_mtime);
			if (ans != 0) {
				options->trw->get_window()->statusbar_update(StatusBarField::INFO, QString("Failed updating EXIF on %1").arg(options->image));
			}
		}
		free(datetime);
	}
}




/**
 * Correlate the image to any track within the TrackWaypoint layer.
 */
static void trw_layer_geotag_process(GeotagJob * options)
{
	if (!options->trw) {
		return;
	}

	if (!options->image) {
		return;
	}

	if (options->wp) {
		trw_layer_geotag_waypoint(options);
		return;
	}

	bool has_gps_exif = false;

	char * datetime = a_geotag_get_exif_date_from_file(options->image, &has_gps_exif);
	if (datetime) {

		/* If image already has gps info - don't attempt to change it. */
		if (!options->ov.overwrite_gps_exif && has_gps_exif) {
			if (options->ov.create_waypoints) {
				/* Create waypoint with file information. */
				QString file_name;
				Waypoint * wp = a_geotag_create_waypoint_from_file(options->image,
										   options->trw->get_coord_mode(),
										   file_name);
				if (!wp) {
					/* Couldn't create Waypoint. */
					free(datetime);
					return;
				}
				if (!file_name.size()) {
					file_name = file_base_name(options->image);
				}

				bool updated_waypoint = false;

				if (options->ov.overwrite_waypoints) {
					Waypoint * current_wp = options->trw->get_waypoints_node().find_waypoint_by_name(file_name);
					if (current_wp) {
						/* Existing wp found, so set new position, comment and image. */
						(void) a_geotag_waypoint_positioned(options->image, wp->coord, wp->altitude, file_name, current_wp);
						updated_waypoint = true;
					}
				}

				if (!updated_waypoint) {
					options->trw->add_waypoint_from_file(wp, file_name);
				}

				/* Mark for redraw. */
				options->redraw = true;
			}
			free(datetime);
			return;
		}

		options->PhotoTime = ConvertToUnixTime(datetime, (char *) EXIF_DATE_FORMAT, options->ov.TimeZoneHours, options->ov.TimeZoneMins);
		free(datetime);

		/* Apply any offset. */
		options->PhotoTime = options->PhotoTime + options->ov.time_offset;

		options->found_match = false;

		if (options->trk) {
			/* Single specified track. */
			trw_layer_geotag_track(options->trk, options);
		} else {
			/* Try all tracks. */
			std::unordered_map<unsigned int, SlavGPS::Track*> & tracks = options->trw->get_track_items();
			if (tracks.size() > 0) {
				trw_layer_geotag_tracks(tracks, options);
			}
		}

		/* Match found? */
		if (options->found_match) {

			if (options->ov.create_waypoints) {

				bool updated_waypoint = false;

				if (options->ov.overwrite_waypoints) {

					/* Update existing WP. */
					/* Find a WP with current name. */
					QString file_name = file_base_name(options->image);
					Waypoint * wp = options->trw->get_waypoints_node().find_waypoint_by_name(file_name);
					if (wp) {
						/* Found, so set new position, comment and image. */
						/* TODO: how do we use file_name modified by the function below? */
						(void)a_geotag_waypoint_positioned(options->image, options->coord, options->altitude, file_name, wp);
						updated_waypoint = true;
					}
				}

				if (!updated_waypoint) {
					/* Create waypoint with found position. */
					QString file_name;
					/* TODO: how do we use file_name modified by the function below? */
					Waypoint * wp = a_geotag_waypoint_positioned(options->image, options->coord, options->altitude, file_name, NULL);
					if (!file_name.size()) {
						file_name = file_base_name(options->image);
					}
					options->trw->add_waypoint_from_file(wp, file_name);
				}

				/* Mark for redraw. */
				options->redraw = true;
			}

			/* Write EXIF if specified. */
			if (options->ov.write_exif) {
				int ans = a_geotag_write_exif_gps(options->image, options->coord, options->altitude, options->ov.no_change_mtime);
				if (ans != 0) {
					options->trw->get_window()->statusbar_update(StatusBarField::INFO, QString("Failed updating EXIF on %1").arg(options->image));
				}
			}
		}
	}
}




GeotagJob::~GeotagJob()
{
	if (!this->files->empty()) {
		/* kamilFIXME: is that all that we need to clean up? */
		delete this->files;
	}
}




/**
 * Run geotagging process in a separate thread.
 */
static int trw_layer_geotag_thread(BackgroundJob * job)
{
	GeotagJob * geotag = (GeotagJob *) job;

	unsigned int total = geotag->files->size();
	unsigned int done = 0;

	/* TODO decide how to report any issues to the user... */

	for (auto iter = geotag->files->begin(); iter != geotag->files->end(); iter++) {
		/* Foreach file attempt to geotag it. */
		geotag->image = *iter;
		trw_layer_geotag_process(geotag);

		/* Update thread progress and detect stop requests. */
		int result = a_background_thread_progress(job, ((double) ++done) / total);
		if (result != 0) {
			return -1; /* Abort thread */
		}
	}

	if (geotag->redraw) {
		if (geotag->trw) {
			geotag->trw->get_waypoints_node().calculate_bounds();
			/* Ensure any new images get show. */
			geotag->trw->verify_thumbnails();
			/* Force redraw as verify only redraws if there are new thumbnails (they may already exist). */
			geotag->trw->emit_layer_changed(); /* NB Update from background. */
		}
	}

	return 0;
}




/**
 * Parse user input from dialog response.
 */
static void trw_layer_geotag_response_cb(QDialog * dialog, int resp, GeoTagWidgets *widgets)
{
#ifdef K
	switch (resp) {
	case GTK_RESPONSE_DELETE_EVENT: /* Received delete event (not from buttons). */
	case GTK_RESPONSE_REJECT:
		break;
	default: {
		/* GTK_RESPONSE_ACCEPT: */
		/* Get options. */
		GeotagJob * options = new GeotagJob(GeoTagWidgets * widgets);
		int len = options->files->size();

		const QString job_description = QObject::tr("Geotagging %1 Images...").arg(len);

		/* Processing lots of files can take time - so run a background effort. */
		a_background_thread(options, ThreadPoolType::LOCAL, job_description);
		break;
	}
	}
	geotag_widgets_free(widgets);
	gtk_widget_destroy(GTK_WIDGET(dialog));
#endif
}




/**
 * Handle widget sensitivities.
 */
static void write_exif_b_cb(GtkWidget *gw, GeoTagWidgets *gtw)
{
	/* Overwriting & file modification times are irrelevant if not going to write EXIF! */
	if (gtw->write_exif_b->isChecked()) {
		gtw->overwrite_gps_exif_b->setEnabled(true);
		gtw->overwrite_gps_exif_l->setEnabled(true);
		gtw->no_change_mtime_b->setEnabled(true);
		gtw->no_change_mtime_l->setEnabled(true);
	} else {
		gtw->overwrite_gps_exif_b->setEnabled(false);
		gtw->overwrite_gps_exif_l->setEnabled(false);
		gtw->no_change_mtime_b->setEnabled(false);
		gtw->no_change_mtime_l->setEnabled(false);
	}
}




static void create_waypoints_b_cb(GtkWidget *gw, GeoTagWidgets *gtw)
{
	/* Overwriting waypoints are irrelevant if not going to create them! */
	if (gtw->create_waypoints_b->isChecked()) {
		gtw->overwrite_waypoints_b->setEnabled(true);
		gtw->overwrite_waypoints_l->setEnabled(true);
	} else {
		gtw->overwrite_waypoints_b->setEnabled(false);
		gtw->overwrite_waypoints_l->setEnabled(false);
	}
}




/**
 * @parent: The Window of the calling process
 * @layer: The LayerTrw to use for correlating images to tracks
 * @track: Optional - The particular track to use (if specified) for correlating images
 * @track_name: Optional - The name of specified track to use
 */
void SlavGPS::trw_layer_geotag_dialog(Window * parent, LayerTRW * trw, Waypoint * wp, Track * trk)
{
	GeoTagWidgets * widgets = geotag_widgets_new();

	widgets->dialog = new BasicDialog(parent);
	widgets->dialog->setWindowTitle(QObject::tr("Geotag Images"));

#ifdef K

	GtkFileFilter *filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("JPG"));
	gtk_file_filter_add_mime_type(filter, "image/jpeg");

	widgets->files = VIK_FILE_LIST(vik_file_list_new(_("Images"), filter));
	widgets->trw = trw;
	widgets->wp = wp;
	widgets->trk = trk;
	widgets->create_waypoints_b = new QCheckBox();
	widgets->overwrite_waypoints_l = new QLabel(QObject::tr("Overwrite Existing Waypoints:"));
	widgets->overwrite_waypoints_b = new QCheckBox();
	widgets->write_exif_b = new QCheckBox();
	widgets->overwrite_gps_exif_l = new QLabel(QObject::tr("Overwrite Existing GPS Information:"));
	widgets->overwrite_gps_exif_b = new QCheckBox();
	widgets->no_change_mtime_l = new QLabel(QObject::tr("Keep File Modification Timestamp:"));
	widgets->no_change_mtime_b = new QCheckBox();
	widgets->interpolate_segments_b = new QCheckBox();

	gtk_entry_set_width_chars(widgets->time_zone_b, 7);
	gtk_entry_set_width_chars(widgets->time_offset_b, 7);

	/* Defaults. */
	option_values_t default_values = get_default_values();

	widgets->create_waypoints_b->setChecked(default_values.create_waypoints);
	widgets->overwrite_waypoints_b->setChecked(default_values.overwrite_waypoints);
	widgets->write_exif_b->setChecked(default_values.write_exif);
	widgets->overwrite_gps_exif_b->setChecked(default_values.overwrite_gps_exif);
	widgets->no_change_mtime_b->setChecked(default_values.no_change_mtime);
	widgets->interpolate_segments_b->setChecked(default_values.interpolate_segments);

	char tmp_string[7];
	snprintf(tmp_string, 7, "%+02d:%02d", default_values.TimeZoneHours, abs(default_values.TimeZoneMins));
	widgets->time_zone_b.seText(tmp_string);
	snprintf(tmp_string, 7, "%d", default_values.time_offset);
	widgets->time_offset_b.setText(tmp_string);

	/* Ensure sensitivities setup. */
	write_exif_b_cb(GTK_WIDGET(widgets->write_exif_b), widgets);
	QObject::connect(widgets->write_exif_b, SIGNAL("toggled"), widgets, SLOT (write_exif_b_cb));

	create_waypoints_b_cb(GTK_WIDGET(widgets->create_waypoints_b), widgets);
	QObject::connect(widgets->create_waypoints_b, SIGNAL("toggled"), widgets, SLOT (create_waypoints_b_cb));

	GtkWidget *cw_hbox = gtk_hbox_new(false, 0);
	QLabel * create_waypoints_l = new QLabel(QObject::tr("Create Waypoints:"));
	cw_hbox->addWidget(create_waypoints_l);
	cw_hbox->addWidget(widgets->create_waypoints_b);

	GtkWidget *ow_hbox = gtk_hbox_new(false, 0);
	ow_hbox->addWidget(widgets->overwrite_waypoints_l);
	ow_hbox->addWidget(widgets->overwrite_waypoints_b);

	GtkWidget *we_hbox = gtk_hbox_new(false, 0);
	we_hbox->addWidget(new QLabel(QObject::tr("Write EXIF:")));
	we_hbox->addWidget(widgets->write_exif_b);

	GtkWidget *og_hbox = gtk_hbox_new(false, 0);
	og_hbox->addWidget(widgets->overwrite_gps_exif_l);
	og_hbox->addWidget(widgets->overwrite_gps_exif_b);

	GtkWidget *fm_hbox = gtk_hbox_new(false, 0);
	fm_hbox->addWidget(widgets->no_change_mtime_l);
	fm_hbox->addWidget(widgets->no_change_mtime_b);

	GtkWidget *is_hbox = gtk_hbox_new(false, 0);
	QLabel * interpolate_segments_l = new QLabel(QObject::tr("Interpolate Between Track Segments:"));
	is_hbox->addWidget(widgets->interpolate_segments_l);
	is_hbox->addWidget(widgets->interpolate_segments_b);

	GtkWidget *to_hbox = gtk_hbox_new(false, 0);
	QLabel * time_offset_l = new QLabel(QObject::tr("Image Time Offset (Seconds):"));
	to_hbox->addWidget(time_offset_l);
	to_hbox->addWidget(&widgets->time_offset_b);
	widgets->time_offset_b.setToolTip(QObject::tr("The number of seconds to ADD to the photos time to make it match the GPS data. Calculate this with (GPS - Photo). Can be negative or positive. Useful to adjust times when a camera's timestamp was incorrect."));

	GtkWidget *tz_hbox = gtk_hbox_new(false, 0);
	QLabel * time_zone_l = new QLabel(QObject::tr("Image Timezone:"));
	tz_hbox->addWidget(time_zone_l);
	tz_hbox->addWidget(&widgets->time_zone_b);
	widgets->time_zone_b.setToolTip(QObject::tr("The timezone that was used when the images were created. For example, if a camera is set to AWST or +8:00 hours. Enter +8:00 here so that the correct adjustment to the images' time can be made. GPS data is always in UTC."));

	QString track_string;
	if (widgets->wp) {
		track_string = tr("Using waypoint: %1").arg(wp->name);
		/* Control sensitivities. */
		widgets->create_waypoints_b->setEnabled(false);
		create_waypoints_l->setEnabled(false);
		widgets->overwrite_waypoints_b->setEnabled(false);
		widgets->overwrite_waypoints_l->setEnabled(false);
		widgets->interpolate_segments_b->setEnabled(false);
		interpolate_segments_l->setEnabled(false);
		widgets->time_offset_b->setEnabled(false);
		time_offset_l->setEnabled(false);
		widgets->time_zone_b->setEnabled(false);
		time_zone_l->setEnabled(false);
	} else if (widgets->trk) {
		track_string = tr("Using track: %1").arg(trk->name);
	} else {
		track_string = tr("Using all tracks in: %1").arg(trw->name);
	}

	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(widgets->dialog))), new QLabel(track_string), false, false, 5);

	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(widgets->dialog))), GTK_WIDGET(widgets->files), true, true, 0);

	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(widgets->dialog))), cw_hbox,  false, false, 0);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(widgets->dialog))), ow_hbox,  false, false, 0);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(widgets->dialog))), we_hbox,  false, false, 0);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(widgets->dialog))), og_hbox,  false, false, 0);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(widgets->dialog))), fm_hbox,  false, false, 0);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(widgets->dialog))), is_hbox,  false, false, 0);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(widgets->dialog))), to_hbox,  false, false, 0);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(widgets->dialog))), tz_hbox,  false, false, 0);

	QObject::connect(widgets->dialog, SIGNAL("response"), widgets, SLOT (trw_layer_geotag_response_cb));

	widgets->dialog->button_box->button(QDialogButtonBox::Discard)->setDefault(true);

	gtk_widget_show_all(widgets->dialog);
#endif
}
