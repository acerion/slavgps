/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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


/*
 * Dependencies in this file can be on anything.
 * For functions with simple system dependencies put it in util.c
 */


#include <cmath>
#include <cstdlib>
#include <unistd.h>

#include <glib.h>

#include "window.h"
#include "viewport_zoom.h"
#include "vikutils.h"
#include "util.h"
#include "application_state.h"
#include "misc/kdtree.h"
#include "dir.h"
#include "layer_map.h"
#include "layer_aggregate.h"
#include "layer_defaults.h"
#include "layers_panel.h"
#include "layer_trw_track_internal.h"
#include "clipboard.h"
#include "dialog.h"
#include "ui_util.h"
#if 0
#include "globals.h"
#include "download.h"
#include "preferences.h"
#include "file.h"
#endif




using namespace SlavGPS;




extern Tree * g_tree;




static struct kdtree * kd = NULL;




#define FMT_MAX_NUMBER_CODES 9

/**
 * @format_code:  String describing the message to generate.
 * @tp:           The trackpoint for which the message is generated about.
 * @tp_prev:      A trackpoint (presumed previous) for interpolating values with the other trackpoint (such as speed).
 * @trk:          The track in which the trackpoints reside.
 * @climb:        Vertical speed (Out of band (i.e. not in a trackpoint) value for display currently only for GPSD usage).
 *
 * TODO: One day replace this cryptic format code with some kind of tokenizer parsing
 * thus would make it more user friendly and maybe even GUI controlable.
 * However for now at least there is some semblance of user control.
 */
QString SlavGPS::vu_trackpoint_formatted_message(const char * format_code, Trackpoint * tp, Trackpoint * tp_prev, Track * trk, double climb)
{
	QString msg = "";
	if (!tp) {
		return msg;
	}

	int len = 0;
	if (format_code) {
		len = strlen(format_code);
	}

	if (len > FMT_MAX_NUMBER_CODES) {
		len = FMT_MAX_NUMBER_CODES;
	}

	std::vector<QString> values(FMT_MAX_NUMBER_CODES);

	SpeedUnit speed_units = Preferences::get_unit_speed();
	const QString speed_units_str = get_speed_unit_string(speed_units);

	const QString separator = " | ";

	for (int i = 0; i < len; i++) {
		switch (g_ascii_toupper(format_code[i])) {
		case 'G': /* GPS Preamble. */
			values[i] = QObject::tr("GPSD");
			break;
		case 'K': /* Trkpt Preamble. */
			values[i] = QObject::tr("Trkpt");
			break;

		case 'S': {
			double speed = 0.0;
			char * speedtype = NULL;
			if (std::isnan(tp->speed) && tp_prev) {
				if (tp->has_timestamp && tp_prev->has_timestamp) {
					if (tp->timestamp != tp_prev->timestamp) {

						/* Work out from previous trackpoint location and time difference. */
						speed = Coord::distance(tp->coord, tp_prev->coord) / ABS(tp->timestamp - tp_prev->timestamp);
						speedtype = strdup("*"); // Interpolated
					} else {
						speedtype = strdup("**");
					}
				} else {
					speedtype = strdup("**");
				}
			} else {
				speed = tp->speed;
				speedtype = strdup("");
			}
			speed = convert_speed_mps_to(speed, speed_units);

			values[i] = QObject::tr("%1Speed%2 %3%4").arg(separator).arg(speedtype).arg(speed, 0, 'f', 1).arg(speed_units_str);
			free(speedtype);
			break;
		}

		case 'B': {
			double speed = 0.0;
			char * speedtype = NULL;
			if (std::isnan(climb) && tp_prev) {
				if (tp->has_timestamp && tp_prev->has_timestamp) {
					if (tp->timestamp != tp_prev->timestamp) {
						/* Work out from previous trackpoint altitudes and time difference.
						   'speed' can be negative if going downhill. */
						speed = (tp->altitude - tp_prev->altitude) / ABS(tp->timestamp - tp_prev->timestamp);
						speedtype = strdup("*"); // Interpolated
					} else {
						speedtype = strdup("**"); // Unavailable
					}
				} else {
					speedtype = strdup("**");
				}
			} else {
				speed = climb;
				speedtype = strdup("");
			}
			speed = convert_speed_mps_to(speed, speed_units);

			/* Go for 2dp as expect low values for vertical speeds. */
			values[i] = QObject::tr("%1Climb%2 %3%4").arg(separator).arg(speedtype).arg(speed, 0, 'f', 2).arg(speed_units_str);
			free(speedtype);
			break;
		}

		case 'A': {
			HeightUnit height_units = Preferences::get_unit_height();
			switch (height_units) {
			case HeightUnit::FEET:
				values[i] = QObject::tr("%1Alt %2feet").arg(separator).arg((int)round(VIK_METERS_TO_FEET(tp->altitude)));
				break;
			default:
				/* HeightUnit::METRES: */
				values[i] = QObject::tr("%1Alt %2m").arg(separator).arg((int)round(tp->altitude));
				break;
			}
			break;
		}

		case 'C': {
			int heading = std::isnan(tp->course) ? 0 : (int)round(tp->course);
			values[i] = QObject::tr("%1Course %2\302\260").arg(separator).arg(heading, 3, 10, (QChar) '0'); /* TODO: what does \302\260 mean? */
			break;
		}

		case 'P': {
			if (tp_prev) {
				int diff = (int) round(Coord::distance(tp->coord, tp_prev->coord));

				QString dist_units_str;
				DistanceUnit distance_unit = Preferences::get_unit_distance();
				/* Expect the difference between track points to be small hence use metres or yards. */
				switch (distance_unit) {
				case DistanceUnit::MILES:
				case DistanceUnit::NAUTICAL_MILES:
					dist_units_str = QObject::tr("yards");
					break;
				default:
					/* DistanceUnit::KILOMETRES: */
					dist_units_str = QObject::tr("m");
					break;
				}

				values[i] = QObject::tr("%1Distance diff %2%3").arg(separator).arg(diff).arg(dist_units_str);
			}
			break;
		}

		case 'T': {
			QString time_string;
			if (tp->has_timestamp) {
				/* Compact date time format. */
				time_string = SGUtils::get_time_string(tp->timestamp, "%x %X", &tp->coord, NULL);
			} else {
				time_string = "--";
			}
			values[i] = QObject::tr("%1Time %2").arg(separator).arg(time_string);
			break;
		}

		case 'M': {
			if (tp_prev) {
				if (tp->has_timestamp && tp_prev->has_timestamp) {
					time_t t_diff = tp->timestamp - tp_prev->timestamp;
					values[i] = QObject::tr("%1Time diff %2s").arg(separator).arg((long) t_diff);
				}
			}
			break;
		}

		case 'X':
			values[i] = QObject::tr("%1No. of Sats %2").arg(separator).arg(tp->nsats);
			break;

		case 'F': {
			if (trk) {
				/* Distance to the end 'Finish' (along the track). */
				double distd = trk->get_length_to_trackpoint(tp);
				double diste = trk->get_length_including_gaps();
				double dist = diste - distd;

				const DistanceUnit distance_unit = Preferences::get_unit_distance();
				const QString distance_unit_string = get_distance_unit_string(distance_unit);
				dist = convert_distance_meters_to(dist, distance_unit);
				values[i] = QObject::tr("%1To End %2%3").arg(separator).arg(dist, 0, 'f', 2).arg(distance_unit_string);
			}
			break;
		}

		case 'D': {
			if (trk) {
				/* Distance from start (along the track). */
				double distd = trk->get_length_to_trackpoint(tp);

				const DistanceUnit distance_unit = Preferences::get_unit_distance();
				const QString distance_unit_string = get_distance_unit_string(distance_unit);
				distd = convert_distance_meters_to(distd, distance_unit);
				values[i] = QObject::tr("%1Distance along %2%3").arg(separator).arg(distd, 0, 'f', 2).arg(distance_unit_string);
			}
			break;
		}

		case 'L': {
			/* Location (Latitude/Longitude). */
			const LatLon lat_lon = tp->coord.get_latlon();
			QString lat;
			QString lon;
			LatLon::to_strings(lat_lon, lat, lon);

			values[i] = QObject::tr("%1%2 %3").arg(separator).arg(lat).arg(lon);
			break;
		}

		case 'N': /* Name of track. */
			if (trk) {
				values[i] = QObject::tr("%1Track: %2").arg(separator).arg(trk->name);
			}
			break;

		case 'E': /* Name of trackpoint if available. */
			if (!tp->name.isEmpty()) {
				values[i] = QObject::tr("%1%2").arg(separator).arg(tp->name);
			} else {
				values[i] = "";
			}
			break;

		default:
			break;
		}
	}

	msg = values[0] + values[1] + values[2] + values[3] + values[4] + values[5] + values[6] + values[7] + values[8];
	return msg;
}



double SlavGPS::convert_speed_mps_to(double speed, SpeedUnit speed_units)
{
	switch (speed_units) {
	case SpeedUnit::KILOMETRES_PER_HOUR:
		speed = VIK_MPS_TO_KPH(speed);
		break;
	case SpeedUnit::MILES_PER_HOUR:
		speed = VIK_MPS_TO_MPH(speed);
		break;
	case SpeedUnit::KNOTS:
		speed = VIK_MPS_TO_KNOTS(speed);
		break;
	default:
		/* SpeedUnit::METRES_PER_SECOND: */
		/* Already in m/s so nothing to do. */
		break;
	}

	return speed;
}




QString SlavGPS::get_speed_unit_string(SpeedUnit speed_unit)
{
	QString result;

	switch (speed_unit) {
	case SpeedUnit::MILES_PER_HOUR:
		result = QObject::tr("mph");
		break;
	case SpeedUnit::METRES_PER_SECOND:
		result = QObject::tr("m/s");
		break;
	case SpeedUnit::KNOTS:
		result = QObject::tr("knots");
		break;
	default:
		/* SpeedUnit::KILOMETRES_PER_HOUR */
		result = QObject::tr("km/h");
		break;
	}

	return result;
}




QString SlavGPS::get_speed_string(double speed, SpeedUnit speed_unit)
{
	QString result;
	const int fract = 2; /* Number of digits after decimal point. */

	switch (speed_unit) {
	case SpeedUnit::KILOMETRES_PER_HOUR:
		result = QObject::tr("%.2f km/h").arg(VIK_MPS_TO_KPH (speed), 0, 'f', fract);
		break;
	case SpeedUnit::MILES_PER_HOUR:
		result = QObject::tr("%.2f mph").arg(VIK_MPS_TO_MPH (speed), 0, 'f', fract);
		break;
	case SpeedUnit::KNOTS:
		result = QObject::tr("%.2f knots").arg(VIK_MPS_TO_KNOTS (speed), 0, 'f', fract);
		break;
	case SpeedUnit::METRES_PER_SECOND:
		result = QObject::tr("%.2f m/s").arg(speed, 0, 'f', fract);
		break;
	default:
		result = "--";
		qDebug() << "EE: Utils: get speed string: invalid speed unit" << (int) speed_unit;
	}

	return result;
}




QString SlavGPS::get_distance_unit_string(DistanceUnit distance_unit)
{
	QString result;

	switch (distance_unit) {
	case DistanceUnit::KILOMETRES:
		result = QObject::tr("km");
	case DistanceUnit::MILES:
		result = QObject::tr("miles");
	case DistanceUnit::NAUTICAL_MILES:
		result = QObject::tr("NM");
	default:
		qDebug() << "EE: Utils: get distance unit string: invalid distance unit" << (int) distance_unit;
		result = "";
	}
	return result;
}




QString SlavGPS::get_distance_string(double distance, DistanceUnit distance_unit)
{
	QString result;
	const int fract = 2; /* Number of digits after decimal point. */

	switch (distance_unit) {
	case DistanceUnit::KILOMETRES:
		result = QObject::tr("%1 km").arg(distance / 1000.0, 0, 'f', fract);
		break;
	case DistanceUnit::MILES:
		result = QObject::tr("%1 miles").arg(VIK_METERS_TO_MILES (distance), 0, 'f', fract);
		break;
	case DistanceUnit::NAUTICAL_MILES:
		result = QObject::tr("%1 NM").arg(VIK_METERS_TO_NAUTICAL_MILES (distance), 0, 'f', fract);
		break;
	default:
		qDebug() << "EE: Utils: get distance string: invalid distance unit" << (int) distance_unit;
	}

	return result;
}




double SlavGPS::convert_distance_meters_to(double distance, DistanceUnit distance_unit)
{
	switch (distance_unit) {
	case DistanceUnit::MILES:
		return VIK_METERS_TO_MILES(distance);

	case DistanceUnit::NAUTICAL_MILES:
		return VIK_METERS_TO_NAUTICAL_MILES(distance);

	case DistanceUnit::KILOMETRES:
		return distance / 1000.0;

	default:
		fprintf(stderr, "CRITICAL: invalid distance unit %d\n", (int) distance_unit);
		return distance;
	}
}




/**
 * Ask the user's opinion to set some of Viking's default behaviour.
 */
void SGUtils::set_auto_features_on_first_run(void)
{
	bool auto_features = false;
	bool set_defaults = false;

	if (SGUtils::is_very_first_run()) {
		auto_features = Dialog::yes_or_no(QObject::tr("This appears to be Viking's very first run.\n\nDo you wish to enable automatic internet features?\n\nIndividual settings can be controlled in the Preferences."), NULL);

		/* Default to more standard cache layout for new users (well new installs at least). */
		maps_layer_set_cache_default(MapsCacheLayout::OSM);
		set_defaults = true;
	}

	if (auto_features) {
		/* Set Maps to autodownload. */
		/* Ensure the default is true. */
		maps_layer_set_autodownload_default(true);
		set_defaults = true;


		/* Enable auto add map + Enable IP lookup. */
		Preferences::set_param_value(PREFERENCES_NAMESPACE_STARTUP "add_default_map_layer", SGVariant((bool) true));
		Preferences::set_param_value(PREFERENCES_NAMESPACE_STARTUP "startup_method",        SGVariant((int32_t) VIK_STARTUP_METHOD_AUTO_LOCATION));

		/* Only on Windows make checking for the latest version on by default. */
		/* For other systems it's expected a Package manager or similar controls the installation, so leave it off. */
#ifdef WINDOWS
		Preferences::set_param_value(PREFERENCES_NAMESPACE_STARTUP "check_version", SGVariant((bool) true));
#endif

		/* Ensure settings are saved for next time. */
		Preferences::save_to_file();
	}

	/* Ensure defaults are saved if changed. */
	if (set_defaults) {
		LayerDefaults::save();
	}
}





/**
 * Returns: Canonical absolute filename
 *
 * Any time a path may contain a relative component, so need to prepend that directory it is relative to.
 * Then resolve the full path to get the normal canonical filename.
 */
char * SlavGPS::vu_get_canonical_filename(Layer * layer, const char * filename, const char * reference_file_full_path)
{
	gchar *canonical = NULL;
	if (!filename) {
		return NULL;
	}

	if (g_path_is_absolute(filename)) {
		canonical = g_strdup(filename);
	} else {
		const char * vw_filename = reference_file_full_path;
		char * dirpath = NULL;
		if (vw_filename) {
			dirpath = g_path_get_dirname(vw_filename);
		} else {
			dirpath = g_get_current_dir(); // Fallback - if here then probably can't create the correct path
		}

		char * full = NULL;
		if (g_path_is_absolute(dirpath)) {
			full = g_strconcat(dirpath, G_DIR_SEPARATOR_S, filename, NULL);
		} else {
			full = g_strconcat(g_get_current_dir(), G_DIR_SEPARATOR_S, dirpath, G_DIR_SEPARATOR_S, filename, NULL);
		}

		canonical = file_realpath_dup(full); // resolved
		free(full);
		free(dirpath);
	}

	return canonical;
}




/**
 * @dir: The directory from which to load the latlontz.txt file.
 *
 * Returns: The number of elements within the latlontz.txt loaded.
 */
static int load_ll_tz_dir(const char * dir)
{
	int inserted = 0;
	const QString path = QString(dir) + QDir::separator() + "latlontz.txt";
	if (0 != access(path.toUtf8().constData(), R_OK)) {
		return inserted;
	}

	char buffer[4096];
	long line_num = 0;
	FILE * file = fopen(path.toUtf8().constData(), "r");
	if (!file) {
		qDebug() << "WW: LL/TZ: Could not open file" << path;
		return inserted;
	}

	while (fgets(buffer, 4096, file)) {
		line_num++;
		char **components = g_strsplit(buffer, " ", 3);
		unsigned int nn = g_strv_length(components);
		if (nn == 3) {
			double pt[2] = { g_ascii_strtod(components[0], NULL), g_ascii_strtod(components[1], NULL) };
			char * time_zone = g_strchomp(components[2]);
			if (kd_insert(kd, pt, time_zone)) {
				fprintf(stderr, "CRITICAL: Insertion problem of %s for line %ld of latlontz.txt\n", time_zone, line_num);
			} else {
				inserted++;
			}
			/* NB Don't free time_zone as it's part of the kdtree data now. */
			free(components[0]);
			free(components[1]);
		} else {
			fprintf(stderr, "WARNING: Line %ld of latlontz.txt does not have 3 parts\n", line_num);
		}
		free(components);
	}

	fclose(file);

	return inserted;
}




/**
 * Can be called multiple times but only initializes the lookup once.
 */
void SlavGPS::vu_setup_lat_lon_tz_lookup()
{
	/* Only setup once. */
	if (kd) {
		return;
	}

	kd = kd_create(2);

	/* Look in the directories of data path. */
	char **data_dirs = get_viking_data_path();
	unsigned int loaded = 0;
	/* Process directories in reverse order for priority. */
	unsigned int n_data_dirs = g_strv_length(data_dirs);
	for (; n_data_dirs > 0; n_data_dirs--) {
		loaded += load_ll_tz_dir(data_dirs[n_data_dirs-1]);
	}
	g_strfreev(data_dirs);

	fprintf(stderr, "DEBUG: %s: Loaded %d elements\n", __FUNCTION__, loaded);
	if (loaded == 0) {
		fprintf(stderr, "CRITICAL: %s: No lat/lon/timezones loaded\n", __FUNCTION__);
	}
}




/**
 * Clear memory used by the lookup.
 * Only call on program exit.
 */
void SlavGPS::vu_finalize_lat_lon_tz_lookup()
{
	if (kd) {
		kd_data_destructor(kd, g_free);
		kd_free(kd);
	}
}




static double dist_sq(double * a1, double * a2, int dims)
{
	double dist_sq = 0, diff;
	while (--dims >= 0) {
		diff = (a1[dims] - a2[dims]);
		dist_sq += diff*diff;
	}
	return dist_sq;
}




static char * time_string_adjusted(time_t * time, int offset_s)
{
	time_t * mytime = time;
	*mytime = *mytime + offset_s;
	char * str = (char *) malloc(64);
	/* Append asterisks to indicate use of simplistic model (i.e. no TZ). */
	strftime(str, 64, "%a %X %x **", gmtime(mytime));
	return str;
}




static char * time_string_tz(time_t * time, const char * format, GTimeZone * tz)
{
	GDateTime * utc = g_date_time_new_from_unix_utc(*time);
	if (!utc) {
		fprintf(stderr, "WARNING: %s: result from g_date_time_new_from_unix_utc() is NULL\n", __FUNCTION__);
		return NULL;
	}
	GDateTime * local = g_date_time_to_timezone(utc, tz);
	if (!local) {
		g_date_time_unref(utc);
		fprintf(stderr, "WARNING: %s: result from g_date_time_to_timezone() is NULL\n", __FUNCTION__);
		return NULL;
	}
	char * str = g_date_time_format(local, format);

	g_date_time_unref(local);
	g_date_time_unref(utc);
	return str;
}




#define VIK_SETTINGS_NEAREST_TZ_FACTOR "utils_nearest_tz_factor"




/**
 * @coord:     Position for which the time zone is desired
 *
 * Returns: TimeZone string of the nearest known location. String may be NULL.
 *
 * Use the k-d tree method (http://en.wikipedia.org/wiki/Kd-tree) to quickly retrieve
 * the nearest location to the given position.
 */
char * SlavGPS::vu_get_tz_at_location(const Coord * coord)
{
	char * tz = NULL;
	if (!coord || !kd) {
		return tz;
	}

	LatLon ll = coord->get_latlon();
	double pt[2] = { ll.lat, ll.lon };

	double nearest;
	if (!ApplicationState::get_double(VIK_SETTINGS_NEAREST_TZ_FACTOR, &nearest)) {
		nearest = 1.0;
	}

	struct kdres * presults = kd_nearest_range(kd, pt, nearest);
	while (!kd_res_end(presults)) {
		double pos[2];
		char *ans = (char*) kd_res_item(presults, pos);
		/* compute the distance of the current result from the pt. */
		double dist = sqrt(dist_sq(pt, pos, 2));
		if (dist < nearest) {
			//printf("NEARER node at (%.3f, %.3f, %.3f) is %.3f away is %s\n", pos[0], pos[1], pos[2], dist, ans);
			nearest = dist;
			tz = ans;
		}
		kd_res_next(presults);
	}
	fprintf(stderr, "DEBUG: TZ lookup found %d results - picked %s\n", kd_res_size(presults), tz);
	kd_res_free(presults);

	return tz;
}




/**
 * @time_t: The time of which the string is wanted
 * @format  The format of the time string - such as "%c"
 * @coord:  Position of object for the time output - maybe NULL
 *          (only applicable for VIK_TIME_REF_WORLD)
 * @tz:     TimeZone string - maybe NULL.
 *          (only applicable for VIK_TIME_REF_WORLD)
 *          Useful to pass in the cached value from vu_get_tz_at_location() to save looking it up again for the same position
 *
 * Returns: A string of the time according to the time display property.
 */
QString SGUtils::get_time_string(time_t time, const char * format, const Coord * coord, const char * tz)
{
	if (!format) {
		return NULL;
	}

	qDebug() << "DD: Get Time String for timestamp" << time;


	char * str = NULL;
	switch (Preferences::get_time_ref_frame()) {
		case VIK_TIME_REF_UTC:
			str = (char *) malloc(64);
			strftime(str, 64, format, gmtime(&time)); /* Always 'GMT'. */
			break;
		case VIK_TIME_REF_WORLD:
			if (coord && !tz) {
				/* No timezone specified so work it out. */
				char * mytz = vu_get_tz_at_location(coord);
				if (mytz) {
					GTimeZone * gtz = g_time_zone_new(mytz);
					str = time_string_tz(&time, format, gtz);
					g_time_zone_unref(gtz);
				} else {
					/* No results (e.g. could be in the middle of a sea).
					   Fallback to simplistic method that doesn't take into account Timezones of countries. */
					const LatLon ll = coord->get_latlon();
					str = time_string_adjusted(&time, round (ll.lon / 15.0) * 3600);
				}
			} else {
				/* Use specified timezone. */
				GTimeZone *gtz = g_time_zone_new(tz);
				str = time_string_tz(&time, format, gtz);
				g_time_zone_unref(gtz);
			}
			break;
		default: /* VIK_TIME_REF_LOCALE */
			str = (char *) malloc(64);
			strftime(str, 64, format, localtime(&time));
			break;
	}

	const QString result(str);
	free(str);
	return result;
}





/**
 * Apply any startup values that have been specified from the command line.
 * Values are defaulted in such a manner not to be applied when they haven't been specified.
 */
void SGUtils::command_line(Window * window, double latitude, double longitude, int zoom_osm_level, MapTypeID cmdline_type_id)
{
	if (!window) {
		return;
	}

	Viewport * viewport = window->get_viewport();

	if (latitude != 0.0 || longitude != 0.0) { /* TODO: is this condition correct? Isn't 0.0/0.0 a correct coordinate? */
		viewport->set_center_from_latlon(LatLon(latitude, longitude), true);
	}

	if (zoom_osm_level >= 0) {
		/* Convert OSM zoom level into Viking zoom level. */
		double mpp = exp((17-zoom_osm_level) * log(2));
		if (mpp > 1.0) {
			mpp = round(mpp);
		}
		viewport->set_zoom(mpp);
	}

	if (cmdline_type_id != MAP_TYPE_ID_INITIAL) {
		/* Some value selected in command line. */

		MapTypeID the_type_id = cmdline_type_id;
		if (the_type_id == MAP_TYPE_ID_DEFAULT) {
			the_type_id = LayerMap::get_default_map_type();
		}

		/* Don't add map layer if one already exists. */
		std::list<Layer const *> * maps = g_tree->tree_get_items_tree()->get_all_layers_of_type(LayerType::MAP, true);
		bool add_map = true;

		for (auto iter = maps->begin(); iter != maps->end(); iter++) {
			LayerMap * map = (LayerMap *) *iter;
			MapTypeID type_id = map->get_map_type();
			if (the_type_id == type_id) {
				add_map = false;
				break;
			}
		}

		if (add_map) {
			LayerMap * layer = new LayerMap();

			layer->set_map_type(the_type_id);
			layer->set_name(Layer::get_type_ui_label(layer->type));

			g_tree->tree_get_items_tree()->get_top_layer()->add_layer(layer, true);
			layer->emit_layer_changed();
		}
	}
}






/**
   Generate a single entry menu to allow copying the displayed text of a button.
*/
void SGUtils::copy_label_menu(QAbstractButton * button)
{
	QMenu menu;
	QAction * action = new QAction(QObject::tr("&Copy"), &menu);
	QObject::connect(action, SIGNAL (triggered(bool)), button, SLOT (vu_copy_label));
	menu.addAction(action);
	menu.exec(QCursor::pos());
}




/**
 * Work out the best zoom level for the LatLon area and set the viewport to that zoom level.
 */
void SlavGPS::vu_zoom_to_show_latlons(CoordMode mode, Viewport * viewport, const LatLonMinMax & min_max)
{
	vu_zoom_to_show_latlons_common(mode, viewport, min_max, 1.0, true);
	return;
}




/**
 * Work out the best zoom level for the LatLon area and set the viewport to that zoom level.
 */
void SlavGPS::vu_zoom_to_show_latlons_common(CoordMode mode, Viewport * viewport, const LatLonMinMax & min_max, double zoom, bool save_position)
{
	/* First set the center [in case previously viewing from elsewhere]. */
	/* Then loop through zoom levels until provided positions are in view. */
	/* This method is not particularly fast - but should work well enough. */

	const Coord coord(LatLonMinMax::get_average(min_max), mode);
	viewport->set_center_from_coord(coord, save_position);

	/* Convert into definite 'smallest' and 'largest' positions. */
	LatLon minmin;
	if (min_max.max.lat < min_max.min.lat) {
		minmin.lat = min_max.max.lat;
	} else {
		minmin.lat = min_max.min.lat;
	}

	LatLon maxmax;
	if (min_max.max.lon > min_max.min.lon) {
		maxmax.lon = min_max.max.lon;
	} else {
		maxmax.lon = min_max.min.lon;
	}

	/* Never zoom in too far - generally not that useful, as too close! */
	/* Always recalculate the 'best' zoom level. */
	viewport->set_zoom(zoom);

	double min_lat, max_lat, min_lon, max_lon;
	/* Should only be a maximum of about 18 iterations from min to max zoom levels. */
	while (zoom <= SG_VIEWPORT_ZOOM_MAX) {
		viewport->get_min_max_lat_lon(&min_lat, &max_lat, &min_lon, &max_lon);
		/* NB I think the logic used in this test to determine if the bounds is within view
		   fails if track goes across 180 degrees longitude.
		   Hopefully that situation is not too common...
		   Mind you viking doesn't really do edge locations to well anyway. */
		if (min_lat < minmin.lat
		    && max_lat > minmin.lat
		    && min_lon < maxmax.lon
		    && max_lon > maxmax.lon) {

			/* Found within zoom level. */
			break;
		}

		/* Try next. */
		zoom = zoom * 2;
		viewport->set_zoom(zoom);
	}
}




bool vik_debug = false;
bool vik_verbose = false;
bool vik_version = false;

/**
 * @version:  The string of the Viking version.
 *            This should be in the form of N.N.N.N, where the 3rd + 4th numbers are optional
 *            Often you'll want to pass in VIKING_VERSION
 *
 * Returns: a single number useful for comparison.
 */
int SlavGPS::viking_version_to_number(char const * version)
{
	/* Basic method, probably can be improved. */
	int version_number = 0;
	char** parts = g_strsplit(version, ".", 0);
	int part_num = 0;
	char *part = parts[part_num];
	/* Allow upto 4 parts to the version number. */
	while (part && part_num < 4) {
		/* Allow each part to have upto 100. */
		version_number = version_number + (atol(part) * pow(100, 3-part_num));
		part_num++;
		part = parts[part_num];
	}
	g_strfreev(parts);
	return version_number;
}




/**
 * Detect when Viking is run for the very first time.
 * Call this very early in the startup sequence to ensure subsequent correct results.
 * The return value is cached, since later on the test will no longer be true.
 */
bool SGUtils::is_very_first_run(void)
{
	static bool vik_very_first_run_known = false;
	static bool vik_very_first_run = false;

	/* Use cached result if available. */
	if (vik_very_first_run_known) {
		return vik_very_first_run;
	}

	char * dir = get_viking_dir_no_create();
	/* NB: will need extra logic if default dir gets changed e.g. from ~/.viking to ~/.config/viking. */
	if (dir) {
		/* If directory exists - Viking has been run before. */
		vik_very_first_run = (0 != access(dir, F_OK));
		free(dir);
	} else {
		vik_very_first_run = true;
	}
	vik_very_first_run_known = true;

	return vik_very_first_run;
}




/**
   \brief Get name of a file, without directories, but with full extension/suffix
*/
QString SlavGPS::file_base_name(const QString & full_path)
{
	QFileInfo info = QFileInfo(full_path);
	return info.fileName();
}




bool SGUtils::create_temporary_file(QTemporaryFile & file, const QString & name_pattern)
{
	file.setFileTemplate(name_pattern);
	if (!file.open()) {
		qDebug().nospace() << "EE: Utils: create temporary file: failed to create temporary file" << name_pattern << file.error();
		return false;
	}
	qDebug()<< "II: Utils: Utils: create temporary file: file path:" << file.fileName();
	file.close(); /* We have to close it here, otherwise client won't be able to write to it. */

	return true;
}




void SGUtils::color_to_string(char * buffer, size_t buffer_size, const QColor & color)
{
	snprintf(buffer, buffer_size, "#%.2x%.2x%.2x", (int) (color.red() / 256), (int) (color.green() / 256), (int) (color.blue() / 256));

}



QPen SGUtils::new_pen(const QColor & color, int width)
{
	QPen pen(color);
	pen.setWidth(width);

#ifdef K
	/* TODO: apply these attributes to a pen. */
	//GDK_LINE_SOLID
	//GDK_CAP_ROUND
	//GDK_JOIN_ROUND
#endif

	return pen;
}
