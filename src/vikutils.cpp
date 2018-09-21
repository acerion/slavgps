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




#include <cstdlib>
#include <unistd.h>




#include <QTimeZone>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QDesktopWidget>




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
#include "measurements.h"
#include "preferences.h"
#include "map_utils.h"
#include "viewport_internal.h"

#ifdef K_INCLUDES
#include "download.h"
#include "file.h"
#endif




using namespace SlavGPS;




#define SG_MODULE "VikUtils"
#define PREFIX ": VikUtils:" << __FUNCTION__ << __LINE__ << ">"




extern Tree * g_tree;




bool vik_debug = false;
bool vik_verbose = false;
bool vik_version = false;




static struct kdtree * kd_timezones = NULL;




#define FMT_MAX_NUMBER_CODES 9

/**
 * @format_code:  String describing the message to generate.
 * @tp:           The trackpoint for which the message is generated about.
 * @tp_prev:      A trackpoint (presumed previous) for interpolating values with the other trackpoint (such as speed).
 * @trk:          The track in which the trackpoints reside.
 * @climb:        Vertical speed (Out of band (i.e. not in a trackpoint) value for display currently only for GPSD usage).
 *
 * TODO_MAYBE: One day replace this cryptic format code with some kind of tokenizer parsing
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
						speed = Coord::distance(tp->coord, tp_prev->coord) / std::abs(tp->timestamp - tp_prev->timestamp);
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
						speed = (tp->altitude - tp_prev->altitude) / std::abs(tp->timestamp - tp_prev->timestamp);
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

		case 'A':
			values[i] = QObject::tr("%1Alt %2").arg(separator).arg(Altitude(tp->altitude, HeightUnit::Metres).convert_to_unit(Preferences::get_unit_height()).to_string());
			break;

		case 'C': {
			int heading = std::isnan(tp->course) ? 0 : (int)round(tp->course);
			values[i] = QObject::tr("%1Course %2%3").arg(separator).arg(heading, 3, 10, (QChar) '0').arg(DEGREE_SYMBOL);
			break;
		}

		case 'P': {
			if (tp_prev) {
				const Distance diff = Coord::distance_2(tp->coord, tp_prev->coord);
				values[i] = QObject::tr("%1Distance diff %2")
					.arg(separator)
					/* Supplementary unit (meters or yards) will be chosen based on selection of main distance units. */
					.arg(diff.convert_to_supplementary_unit(Preferences::get_unit_distance()).to_string());
			}
			break;
		}

		case 'T': {
			QString time_string;
			if (tp->has_timestamp) {
				/* Compact date time format. */
				time_string = SGUtils::get_time_string(tp->timestamp, Qt::TextDate, tp->coord, NULL);
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

		case 'F': { /* Distance from tp to the end 'Finish' (along the track). */
			if (trk) {
				Distance begin_to_tp = trk->get_length_to_trackpoint(tp);
				Distance total = trk->get_length_including_gaps();
				Distance tp_to_end = total - begin_to_tp;

				values[i] = QObject::tr("%1To End %2")
					.arg(separator)
					.arg(tp_to_end.convert_to_unit(Preferences::get_unit_distance()).to_nice_string());
			}
			break;
		}

		case 'D': { /* Distance from start (along the track). */
			if (trk) {
				const Distance begin_to_tp = trk->get_length_to_trackpoint(tp);

				values[i] = QObject::tr("%1Distance along %2")
					.arg(separator)
					.arg(begin_to_tp.convert_to_unit(Preferences::get_unit_distance()).to_nice_string());
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




double SlavGPS::convert_speed_mps_to(double speed, SpeedUnit speed_unit)
{
	switch (speed_unit) {
	case SpeedUnit::KilometresPerHour:
		speed = VIK_MPS_TO_KPH(speed);
		break;
	case SpeedUnit::MilesPerHour:
		speed = VIK_MPS_TO_MPH(speed);
		break;
	case SpeedUnit::MetresPerSecond:
		/* Already in m/s so nothing to do. */
		break;
	case SpeedUnit::Knots:
		speed = VIK_MPS_TO_KNOTS(speed);
		break;
	default:
		qDebug() << "EE:" PREFIX << "invalid speed unit" << (int) speed_unit;
		break;
	}

	return speed;
}




QString SlavGPS::get_speed_unit_string(SpeedUnit speed_unit)
{
	QString result;

	switch (speed_unit) {
	case SpeedUnit::KilometresPerHour:
		result = QObject::tr("km/h");
		break;
	case SpeedUnit::MilesPerHour:
		result = QObject::tr("mph");
		break;
	case SpeedUnit::MetresPerSecond:
		result = QObject::tr("m/s");
		break;
	case SpeedUnit::Knots:
		result = QObject::tr("knots");
		break;
	default:
		qDebug() << "EE:" PREFIX << "invalid speed unit" << (int) speed_unit;
		break;
	}

	return result;
}




QString SlavGPS::get_speed_string(double speed, SpeedUnit speed_unit)
{
	QString result;
	const int fract = 2; /* Number of digits after decimal point. */

	switch (speed_unit) {
	case SpeedUnit::KilometresPerHour:
		result = QObject::tr("%1 km/h").arg(VIK_MPS_TO_KPH (speed), 0, 'f', fract);
		break;
	case SpeedUnit::MilesPerHour:
		result = QObject::tr("%1 mph").arg(VIK_MPS_TO_MPH (speed), 0, 'f', fract);
		break;
	case SpeedUnit::MetresPerSecond:
		result = QObject::tr("%1 m/s").arg(speed, 0, 'f', fract);
		break;
	case SpeedUnit::Knots:
		result = QObject::tr("%1 knots").arg(VIK_MPS_TO_KNOTS (speed), 0, 'f', fract);
		break;
	default:
		result = "--";
		qDebug() << "EE:" PREFIX << "invalid speed unit" << (int) speed_unit;
		break;
	}

	return result;
}




double SlavGPS::convert_distance_meters_to(double distance, DistanceUnit distance_unit)
{
	switch (distance_unit) {
	case DistanceUnit::Kilometres:
		return distance / 1000.0;
	case DistanceUnit::Miles:
		return VIK_METERS_TO_MILES(distance);
	case DistanceUnit::NauticalMiles:
		return VIK_METERS_TO_NAUTICAL_MILES(distance);
	default:
		qDebug() << "EE" PREFIX << "invalid distance unit" << (int) distance_unit;
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
		LayerMap::set_cache_default(MapCacheLayout::OSM);
		set_defaults = true;
	}

	if (auto_features) {
		/* Set Maps to autodownload. */
		/* Ensure the default is true. */
		LayerMap::set_autodownload_default(true);
		set_defaults = true;


		/* Enable auto add map + Enable IP lookup. */
		Preferences::set_param_value(QString(PREFERENCES_NAMESPACE_STARTUP "add_default_map_layer"), SGVariant((bool) true));
		Preferences::set_param_value(QString(PREFERENCES_NAMESPACE_STARTUP "startup_method"),        SGVariant((int32_t) StartupMethod::AutoLocation));

		/* Only on Windows make checking for the latest version on by default. */
		/* For other systems it's expected a Package manager or similar controls the installation, so leave it off. */
#ifdef WINDOWS
		Preferences::set_param_value(QString(PREFERENCES_NAMESPACE_STARTUP "check_version"), SGVariant((bool) true));
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
QString SlavGPS::vu_get_canonical_filename(Layer * layer, const QString & path, const QString & reference_file_full_path)
{
	QString canonical;

	if (g_path_is_absolute(path.toUtf8().constData())) {
		canonical = path;
	} else {
		char * dirpath = NULL;
		if (reference_file_full_path.isEmpty()) {
			dirpath = g_get_current_dir(); // Fallback - if here then probably can't create the correct path
		} else {
			dirpath = g_path_get_dirname(reference_file_full_path.toUtf8().constData());
		}


		QString subpath = QString("%1%2%3").arg(dirpath).arg(QDir::separator()).arg(path);
		QString full_path;
		if (g_path_is_absolute(dirpath)) {
			full_path = subpath;
		} else {
			full_path = QString("%1%2%3").arg(g_get_current_dir()).arg(QDir::separator()).arg(subpath);
		}

		canonical = SGUtils::get_canonical_path(full_path);
		free(dirpath);
	}

	return canonical;
}




/**
   @dir: The directory from which to load the latlontz.txt file.

   @returns the number of elements within the latlontz.txt loaded.
*/
int TZLookup::load_from_dir(const QString & dir)
{
	int inserted = 0;
	const QString path = dir + QDir::separator() + "latlontz.txt";
	if (0 != access(path.toUtf8().constData(), R_OK)) {
		qDebug() << "WW" PREFIX << "Could not access time zones file" << path;
		return inserted;
	}

	long line_num = 0;
	FILE * file = fopen(path.toUtf8().constData(), "r");
	if (!file) {
		qDebug() << "WW" PREFIX << "Could not open time zones file" << path;
		return inserted;
	}

	char buffer[4096];
	while (fgets(buffer, sizeof (buffer), file)) {
		line_num++;
		const QStringList components = QString(buffer).split(" ", QString::SkipEmptyParts);
		if (components.size() == 3) {
			double pt[2] = { SGUtils::c_to_double(components.at(0)), SGUtils::c_to_double(components.at(1)) };
			QTimeZone * time_zone = new QTimeZone(QByteArray(components.at(2).trimmed().toUtf8().constData()));
			if (kd_insert(kd_timezones, pt, time_zone)) {
				qDebug() << "EE" PREFIX << "Insertion problem for tz" << time_zone <<  "created from line" << line_num << buffer;
			} else {
				inserted++;
			}
			/* Don't free time_zone as it's part of the kdtree data now. */
		} else {
			qDebug() << "WW" PREFIX << "Line" << line_num << "in time zones file does not have 3 parts:" << buffer;
		}
	}

	fclose(file);

	qDebug() << "II" PREFIX << "Loaded" << inserted << "time zones";
	return inserted;
}




/**
 * Can be called multiple times but only initializes the lookup once.
 */
void TZLookup::init()
{
	/* Only setup once. */
	if (kd_timezones) {
		return;
	}

	kd_timezones = kd_create(2);

	/* Look in the directories of data path. */
	const QStringList data_dirs = SlavGPSLocations::get_data_dirs();
	unsigned int loaded = 0;

	/* Process directories in reverse order for priority. */
	int n_data_dirs = data_dirs.size();
	for (int i = n_data_dirs - 1; i >= 0; i--) {
		loaded += TZLookup::load_from_dir(data_dirs.at(i));
	}

	qDebug() << "DD" PREFIX << "Loaded" << loaded << "elements";
	if (loaded == 0) {
		qDebug() << "EE" PREFIX << "No lat/lon/timezones loaded";
	}
}




static void tz_destructor(void * data)
{
	if (data) {
		QTimeZone * time_zone = (QTimeZone *) data;
		qDebug() << "DD" PREFIX << "Will delete time zone" << time_zone;
		delete time_zone;
	}
}




/**
   Clear memory used by the lookup.
   Only call on program exit.
*/
void TZLookup::uninit(void)
{
	if (kd_timezones) {
		kd_data_destructor(kd_timezones, tz_destructor);
		kd_free(kd_timezones);
	}
}




static double dist_sq(const double * a1, const double * a2, int dims)
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




static QString time_string_tz(time_t time, Qt::DateFormat format, const QTimeZone & tz)
{
	QDateTime utc = QDateTime::fromTime_t(time, Qt::OffsetFromUTC, 0);  /* TODO_MAYBE: use fromSecsSinceEpoch() after migrating to Qt 5.8 or later. */
	QDateTime local = utc.toTimeZone(tz);

	return local.toString(format);
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
const QTimeZone * TZLookup::get_tz_at_location(const Coord & coord)
{
	QTimeZone * tz = NULL;
	if (!kd_timezones) {
		return tz;
	}

	const LatLon ll = coord.get_latlon();
	const double pt[2] = { ll.lat, ll.lon };

	double nearest;
	if (!ApplicationState::get_double(VIK_SETTINGS_NEAREST_TZ_FACTOR, &nearest)) {
		nearest = 1.0;
	}

	struct kdres * presults = kd_nearest_range(kd_timezones, pt, nearest);
	while (!kd_res_end(presults)) {
		double pos[2];
		QTimeZone * ans = (QTimeZone *) kd_res_item(presults, pos);
		/* compute the distance of the current result from the pt. */
		double dist = sqrt(dist_sq(pt, pos, 2));
		if (dist < nearest) {
			//printf("NEARER node at (%.3f, %.3f, %.3f) is %.3f away is %s\n", pos[0], pos[1], pos[2], dist, ans);
			nearest = dist;
			tz = ans;
		}
		kd_res_next(presults);
	}

	if (tz) {
		qDebug() << "DD" PREFIX << "time zone lookup found" << kd_res_size(presults) << "results - picked" << tz->displayName(QDateTime::currentDateTime());
	} else {
		qDebug() << "WW" PREFIX << "time zone lookup NOT found";
	}
	kd_res_free(presults);

	return tz;
}




/**
   @timestamp - The time of which the string is wanted
   @format - The format of the time string
   @coord - Position of object for the time output (only applicable for format == SGTimeReference::World)
   @tz - TimeZone string - maybe NULL (only applicable for SGTimeReference::World). Useful to pass in the cached value from TZLookup::get_tz_at_location() to save looking it up again for the same position.

   @return a string of the time according to the time display property
*/
QString SGUtils::get_time_string(time_t timestamp, Qt::DateFormat format, const Coord & coord, const QTimeZone * tz)
{
	QString time_string;

	const SGTimeReference ref = Preferences::get_time_ref_frame();
	switch (ref) {
	case SGTimeReference::UTC:
		time_string = QDateTime::fromTime_t(timestamp, QTimeZone::utc()).toString(format); /* TODO_MAYBE: use fromSecsSinceEpoch() after migrating to Qt 5.8 or later. */
		qDebug() << SG_PREFIX_D << "UTC: timestamp =" << timestamp << "-> time string" << time_string;
		break;
	case SGTimeReference::World:
		if (tz) {
			/* Use specified timezone. */
			time_string = time_string_tz(timestamp, format, *tz);
			qDebug() << SG_PREFIX_D << "World (from timezone): timestamp =" << timestamp << "-> time string" << time_string;
		} else {
			/* No timezone specified so work it out. */
			QTimeZone const * tz_from_location = TZLookup::get_tz_at_location(coord);
			if (tz_from_location) {
				time_string = time_string_tz(timestamp, format, *tz_from_location);
				qDebug() << SG_PREFIX_D << "World (from location): timestamp =" << timestamp << "-> time string" << time_string;
			} else {
				/* No results (e.g. could be in the middle of a sea).
				   Fallback to simplistic method that doesn't take into account Timezones of countries. */
				const LatLon ll = coord.get_latlon();
				time_string = time_string_adjusted(&timestamp, round (ll.lon / 15.0) * 3600);
				qDebug() << SG_PREFIX_D << "World (fallback): timestamp =" << timestamp << "-> time string" << time_string;
			}
		}
		break;
	case SGTimeReference::Locale:
		time_string = QDateTime::fromTime_t(timestamp, QTimeZone::systemTimeZone()).toString(format); /* TODO_MAYBE: use fromSecsSinceEpoch() after migrating to Qt 5.8 or later. */
		qDebug() << SG_PREFIX_D << "Locale: timestamp =" << timestamp << "-> time string" << time_string;
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected SGTimeReference value" << (int) ref;
		break;
	}

	return time_string;
}




/**
   @timestamp - The time of which the string is wanted
   @format - The format of the time string
   @coord - Position of object for the time output (only applicable for format == SGTimeReference::World)

   @return a string of the time according to the time display property
*/
QString SGUtils::get_time_string(time_t timestamp, Qt::DateFormat format, const Coord & coord)
{
	QString time_string;
	const QTimeZone * tz_from_location = NULL;

	const SGTimeReference ref = Preferences::get_time_ref_frame();
	switch (ref) {
	case SGTimeReference::UTC:
		time_string = QDateTime::fromTime_t(timestamp, QTimeZone::utc()).toString(format); /* TODO_MAYBE: use fromSecsSinceEpoch() after migrating to Qt 5.8 or later. */
		qDebug() << SG_PREFIX_D << "UTC: timestamp =" << timestamp << "-> time string" << time_string;
		break;
	case SGTimeReference::World:
		/* No timezone specified so work it out. */
		tz_from_location = TZLookup::get_tz_at_location(coord);
		if (tz_from_location) {
			time_string = time_string_tz(timestamp, format, *tz_from_location);
			qDebug() << SG_PREFIX_D << "World (from location): timestamp =" << timestamp << "-> time string" << time_string;
		} else {
			/* No results (e.g. could be in the middle of a sea).
			   Fallback to simplistic method that doesn't take into account Timezones of countries. */
			const LatLon ll = coord.get_latlon();
			time_string = time_string_adjusted(&timestamp, round (ll.lon / 15.0) * 3600);
			qDebug() << SG_PREFIX_D << "World (fallback): timestamp =" << timestamp << "-> time string" << time_string;
		}
		break;
	case SGTimeReference::Locale:
		time_string = QDateTime::fromTime_t(timestamp, QTimeZone::systemTimeZone()).toString(format); /* TODO_MAYBE: use fromSecsSinceEpoch() after migrating to Qt 5.8 or later. */
		qDebug() << SG_PREFIX_D << "Locale: timestamp =" << timestamp << "-> time string" << time_string;
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected SGTimeReference value" << (int) ref;
		break;
	}

	return time_string;
}




void CommandLineOptions::apply(Window * window)
{
	if (!window) {
		return;
	}

	Viewport * viewport = window->get_viewport();

	if (this->lat_lon.is_valid()) {
		viewport->set_center_from_latlon(this->lat_lon, true);
	}

	if (this->zoom_level_osm >= 0) {
		/* Convert OSM zoom level into Viking zoom level. */
		double map_zoom = exp((MAGIC_SEVENTEEN - this->zoom_level_osm) * log(2));
		if (map_zoom > 1.0) {
			map_zoom = round(map_zoom);
		}
		viewport->set_viking_zoom_level(map_zoom);
	}

	if (this->map_type_id != MapTypeID::Initial) {
		/* Some value selected in command line. */

		MapTypeID the_type_id = this->map_type_id;
		if (the_type_id == MapTypeID::Default) {
			the_type_id = LayerMap::get_default_map_type_id();
		}

		/* Don't add map layer if one already exists. */
		const std::list<const Layer *> map_layers = g_tree->tree_get_items_tree()->get_all_layers_of_type(LayerType::Map, true);
		bool add_map = true;

		for (auto iter = map_layers.begin(); iter != map_layers.end(); iter++) {
			LayerMap * map = (LayerMap *) *iter;
			MapTypeID type_id = map->get_map_type_id();
			if (the_type_id == type_id) {
				add_map = false;
				break;
			}
		}

		add_map = add_map && MapSource::is_map_type_id_registered(the_type_id);
		if (add_map) {
			LayerMap * layer = new LayerMap();

			layer->set_map_type_id(the_type_id);
			layer->set_name(Layer::get_type_ui_label(layer->type));

			g_tree->tree_get_items_tree()->get_top_layer()->add_layer(layer, true);
			layer->emit_layer_changed("Command Line Options - Apply");
		}
	}
}




bool CommandLineOptions::parse(QCoreApplication & app)
{
	QCommandLineParser parser;

	const QCommandLineOption opt_debug(QStringList() << "d" << "debug", QObject::tr("Enable debug output"));
	parser.addOption(opt_debug);

	const QCommandLineOption opt_verbose(QStringList() << "V" << "verbose", QObject::tr("Enable verbose output"));
	parser.addOption(opt_verbose);

	const QCommandLineOption opt_version(QStringList() << "v" << "version", QObject::tr("Show program version"));
	parser.addOption(opt_version);

	const QCommandLineOption opt_latitude(QStringList() << "y" << "latitude", QObject::tr("Latitude in decimal degrees"), "latitude");
	parser.addOption(opt_latitude);

	const QCommandLineOption opt_longitude(QStringList() << "x" << "longitude", QObject::tr("Longitude in decimal degrees"), "longitude");
	parser.addOption(opt_longitude);

	const QCommandLineOption opt_zoom(QStringList() << "z" << "zoom", QObject::tr("Zoom Level (OSM). Value can be 0 - 22"), "zoom");
	parser.addOption(opt_zoom);

	const QCommandLineOption opt_map(QStringList() << "m" << "map", QObject::tr("Add a map layer by id value. Use 0 for the default map."), "map");
	parser.addOption(opt_map);


	parser.process(app);


	this->debug = parser.isSet(opt_debug);
	qDebug() << "DD" PREFIX << "debug is" << this->debug;
	if (this->debug) {
#ifdef K_FIXME_RESTORE
		g_log_set_handler(NULL, G_LOG_LEVEL_DEBUG, log_debug, NULL);
#endif
	}


	this->verbose = parser.isSet(opt_verbose);
	qDebug() << "DD" PREFIX << "verbose is" << this->verbose;

	this->version = parser.isSet(opt_version);
	qDebug() << "DD" PREFIX << "version is" << this->version;



	if (parser.isSet(opt_latitude) != parser.isSet(opt_longitude)) {
		qDebug() << "EE" PREFIX << "you need to specify both latitude and longitude";
		return false;
	}

	if (parser.isSet(opt_latitude) && parser.isSet(opt_longitude)) {
		QLocale locale; /* System's locale. */
		bool parse_ok = true;
		this->lat_lon.lat = locale.toDouble(parser.value(opt_latitude), &parse_ok);
		if (parse_ok) {
			this->lat_lon.lon = locale.toDouble(parser.value(opt_longitude), &parse_ok);
		}

		if (parse_ok && this->lat_lon.is_valid()) {
			qDebug() << "DD" PREFIX << "lat/lon is" << this->lat_lon.lat << this->lat_lon.lon;
		} else {
			this->lat_lon.invalidate();
			qDebug() << "EE" PREFIX << "Failed to parse lat/lon values from command line:" << parser.value(opt_latitude) << parser.value(opt_longitude);
			return false;
		}
	}



	if (parser.isSet(opt_zoom)) {
		this->zoom_level_osm = parser.value(opt_zoom).toInt();
		qDebug() << "DD" PREFIX << "zoom is" << this->zoom_level_osm;
	}

	if (parser.isSet(opt_map)) {
		this->map_type_id = (MapTypeID) parser.value(opt_map).toInt();
		qDebug() << "DD" PREFIX << "map type id is" << (int) this->map_type_id;
	}

	this->files = parser.positionalArguments(); /* Possibly .vik files passed in command line, to be opened by application. */
	qDebug() << "DD" PREFIX << "list of files is" << this->files;

	return true;
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
void SlavGPS::vu_zoom_to_show_bbox(Viewport * viewport, CoordMode mode, const LatLonBBox & bbox)
{
	vu_zoom_to_show_bbox_common(viewport, mode, bbox, 1.0, true);
	return;
}




/**
 * Work out the best zoom level for the LatLon area and set the viewport to that zoom level.
 */
void SlavGPS::vu_zoom_to_show_bbox_common(Viewport * viewport, CoordMode mode, const LatLonBBox & bbox, double zoom, bool save_position)
{
	/* First set the center [in case previously viewing from elsewhere]. */
	/* Then loop through zoom levels until provided positions are in view. */
	/* This method is not particularly fast - but should work well enough. */

	const Coord coord(bbox.get_center(), mode);
	viewport->set_center_from_coord(coord, save_position);

	/* Convert into definite 'smallest' and 'largest' positions. */
	double lowest_latitude = 0.0;
	if (bbox.north < bbox.south) {
		lowest_latitude = bbox.north;
	} else {
		lowest_latitude = bbox.south;
	}

	double maximal_longitude = 0.0;
	if (bbox.east > bbox.west) {
	        maximal_longitude = bbox.east;
	} else {
		maximal_longitude = bbox.west;
	}

	/* Never zoom in too far - generally not that useful, as too close! */
	/* Always recalculate the 'best' zoom level. */
	viewport->set_viking_zoom_level(zoom);


	/* Should only be a maximum of about 18 iterations from min to max zoom levels. */
	while (zoom <= SG_VIEWPORT_ZOOM_MAX) {
		const LatLonBBox current_bbox = viewport->get_bbox();
		/* NB I think the logic used in this test to determine if the bounds is within view
		   fails if track goes across 180 degrees longitude.
		   Hopefully that situation is not too common...
		   Mind you viking doesn't really do edge locations to well anyway. */

		if (current_bbox.south < lowest_latitude
		    && current_bbox.north > lowest_latitude
		    && current_bbox.west < maximal_longitude
		    && current_bbox.east > maximal_longitude) {

			/* Found within zoom level. */
			break;
		}

		/* Try next. */
		zoom = zoom * 2;
		viewport->set_viking_zoom_level(zoom);
	}
}




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
	static bool very_first_run_known = false;
	static bool very_first_run = false;

	/* Use cached result if available. */
	if (very_first_run_known) {
		return very_first_run;
	}

	very_first_run = !SlavGPSLocations::config_dir_exists();
	very_first_run_known = true;

	return very_first_run;
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
		qDebug() << SG_PREFIX_E << "Failed to create temporary file" << name_pattern << file.error();
		return false;
	}
	qDebug() << SG_PREFIX_I << "Successfully created temporary file" << file.fileName();
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
	pen.setCapStyle(Qt::RoundCap);
	pen.setJoinStyle(Qt::RoundJoin);

#if 0   /* This line style is used by default. */
	pen.setStyle(Qt::SolidLine);
#endif

	return pen;
}




/* Just a very simple wrapper. */
QString SGUtils::get_canonical_path(const QString & path)
{
	QDir dir(path);
	return dir.canonicalPath();
}




double SGUtils::c_to_double(const QString & string)
{
	static QLocale c_locale = QLocale::c();
	bool ok = false;
	double result = c_locale.toDouble(string, &ok);
	if (!ok) {
		result = NAN;
	}

	return result;
}




/**
   \brief Convert a double to a string in C locale

   Following GPX specifications, decimal values are xsd:decimal
   So, they must use the period separator, not the localized one.

   This function re-implements glib-based a_coords_dtostr() function
   from coords.cpp.
*/
QString SGUtils::double_to_c(double d, int precision)
{
	static QLocale c_locale = QLocale::c();
	QString result;

	if (d == NAN) {
		return result;
	} else {
		result = c_locale.toString(d, 'f', precision);
	}

	return result;
}




GlobalPoint SGUtils::coord_to_global_point(const Coord & coord, const Viewport * viewport)
{
	GlobalPoint global_point;
	const ScreenPos screen_pos = viewport->coord_to_screen_pos(coord); /* In viewport's x/y coordinate system. */
	global_point.point = viewport->mapToGlobal(QPoint(screen_pos.x, screen_pos.y)); /* In screen's x/y coordinate system. */

	if (1) { /* Debug. */
		const int primary_screen = QApplication::desktop()->primaryScreen();
		const QRect primary_screen_geo = QApplication::desktop()->availableGeometry(primary_screen);
		const QRect containing_screen_geo = QApplication::desktop()->availableGeometry(viewport);

		qDebug() << SG_PREFIX_D << "Available geometry of primary screen:" << primary_screen_geo;
		qDebug() << SG_PREFIX_D << "Available geometry of screen containing widget:" << containing_screen_geo;
	}

	return global_point;
}
