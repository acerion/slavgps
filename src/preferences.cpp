/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2008, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2010-2013, Rob Norris <rw_norris@hotmail.com>
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




#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <sys/stat.h> /* chmod() */




#include <QDebug>




#include "layer.h"
#include "preferences.h"
#include "dir.h"
#include "file.h"
#include "util.h"
#include "ui_builder.h"
#include "vikutils.h"




/* TODO_LATER: Handle variants of type "string list". */




using namespace SlavGPS;




#define SG_MODULE "Preferences"
#define VIKING_PREFERENCES_FILE "viking.prefs"




static WidgetIntEnumerationData degree_format_enum = {
	{
		SGLabelID(QObject::tr("DDD"), (int) DegreeFormat::DDD),
		SGLabelID(QObject::tr("DMM"), (int) DegreeFormat::DMM),
		SGLabelID(QObject::tr("DMS"), (int) DegreeFormat::DMS),
		SGLabelID(QObject::tr("Raw"), (int) DegreeFormat::Raw),
	},
	(int) DegreeFormat::DDD,
};

static WidgetIntEnumerationData unit_distance_enum = {
	{
		SGLabelID(QObject::tr("Kilometres"),     (int) DistanceType::Unit::E::Kilometres),
		SGLabelID(QObject::tr("Miles"),          (int) DistanceType::Unit::E::Miles),
		SGLabelID(QObject::tr("Nautical Miles"), (int) DistanceType::Unit::E::NauticalMiles),
	},
	(int) DistanceType::Unit::E::Kilometres,
};

static WidgetIntEnumerationData unit_speed_enum = {
	{
		SGLabelID(QObject::tr("km/h"),  (int) SpeedType::Unit::E::KilometresPerHour),
		SGLabelID(QObject::tr("mph"),   (int) SpeedType::Unit::E::MilesPerHour),
		SGLabelID(QObject::tr("m/s"),   (int) SpeedType::Unit::E::MetresPerSecond),
		SGLabelID(QObject::tr("knots"), (int) SpeedType::Unit::E::Knots)
	},
	(int) SpeedType::Unit::E::KilometresPerHour,
};

static WidgetIntEnumerationData unit_height_enum = {
	{
		SGLabelID(QObject::tr("Metres"), (int) AltitudeType::Unit::E::Metres),
		SGLabelID(QObject::tr("Feet"),   (int) AltitudeType::Unit::E::Feet),
	},
	(int) AltitudeType::Unit::E::Metres,
};


static WidgetIntEnumerationData time_ref_frame_enum = {
	{
		SGLabelID(QObject::tr("Locale"), (int) SGTimeReference::Locale),
		SGLabelID(QObject::tr("World"),  (int) SGTimeReference::World),
		SGLabelID(QObject::tr("UTC"),    (int) SGTimeReference::UTC),
	},
	(int) SGTimeReference::Locale,
};





/* Hardcoded default location is New York. */
static SGVariant hardcoded_latitude_value(40.714490, SGVariantType::Latitude);
static SGVariant hardcoded_longitude_value(-74.007130, SGVariantType::Longitude);
static SGVariant hardcoded_latitude_fn(void) { return hardcoded_latitude_value; }
static SGVariant hardcoded_longitude_fn(void) { return hardcoded_longitude_value; }

static ParameterSpecification general_prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_GENERAL "degree_format",            SGVariantType::Enumeration,     PARAMETER_GROUP_GENERIC, QObject::tr("Degree format:"),            WidgetType::IntEnumeration,     &degree_format_enum,    NULL, "" },
	{ 1, PREFERENCES_NAMESPACE_GENERAL "units_distance",           SGVariantType::Enumeration,     PARAMETER_GROUP_GENERIC, QObject::tr("Distance units:"),           WidgetType::IntEnumeration,     &unit_distance_enum,    NULL, "" },
	{ 2, PREFERENCES_NAMESPACE_GENERAL "units_speed",              SGVariantType::Enumeration,     PARAMETER_GROUP_GENERIC, QObject::tr("Speed units:"),              WidgetType::IntEnumeration,     &unit_speed_enum,       NULL, "" },
	{ 3, PREFERENCES_NAMESPACE_GENERAL "units_height",             SGVariantType::Enumeration,     PARAMETER_GROUP_GENERIC, QObject::tr("Height units:"),             WidgetType::IntEnumeration,     &unit_height_enum,      NULL, "" },
	{ 4, PREFERENCES_NAMESPACE_GENERAL "use_large_waypoint_icons", SGVariantType::Boolean,         PARAMETER_GROUP_GENERIC, QObject::tr("Use large waypoint icons:"), WidgetType::CheckButton,     NULL,                   NULL, "" },
	{ 5, PREFERENCES_NAMESPACE_GENERAL "default_latitude",         SGVariantType::Latitude,        PARAMETER_GROUP_GENERIC, QObject::tr("Default latitude:"),         WidgetType::Latitude,        NULL,                   hardcoded_latitude_fn, "" },
	{ 6, PREFERENCES_NAMESPACE_GENERAL "default_longitude",        SGVariantType::Longitude,       PARAMETER_GROUP_GENERIC, QObject::tr("Default longitude:"),        WidgetType::Longitude,       NULL,                   hardcoded_longitude_fn, "" },
	{ 7, PREFERENCES_NAMESPACE_GENERAL "time_reference_frame",     SGVariantType::Enumeration,     PARAMETER_GROUP_GENERIC, QObject::tr("Time Display:"),             WidgetType::IntEnumeration,     &time_ref_frame_enum,   NULL, QObject::tr("Display times according to the reference frame. Locale is the user's system setting. World is relative to the location of the object.") },
	{ 8,                               "",                         SGVariantType::Empty,           PARAMETER_GROUP_GENERIC, "",                                       WidgetType::None,            NULL,                   NULL, "" },
};

/* External/Export Options */

static WidgetIntEnumerationData kml_export_unit_enum = {
	{
		SGLabelID(QObject::tr("Metric"),   0),
		SGLabelID(QObject::tr("Statute"),  1),
		SGLabelID(QObject::tr("Nautical"), 2),
	},
	0,
};

static WidgetIntEnumerationData gpx_export_trk_sort_enum = {
	{
		SGLabelID(QObject::tr("Alphabetical"), 0),
		SGLabelID(QObject::tr("Time"),         1),
		SGLabelID(QObject::tr("Creation"),     2),
	},
	0,
};

static WidgetIntEnumerationData gpx_export_wpt_symbols_enum = {
	{
		SGLabelID(QObject::tr("Title Case"), 0),
		SGLabelID(QObject::tr("Lowercase"),  1),
	},
	0,
};

static ParameterSpecification io_prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_IO "kml_export_units",         SGVariantType::Enumeration,   PARAMETER_GROUP_GENERIC,  QObject::tr("KML File Export Units:"),  WidgetType::IntEnumeration,   &kml_export_unit_enum,        NULL, "" },
	{ 1, PREFERENCES_NAMESPACE_IO "gpx_export_track_sort",    SGVariantType::Enumeration,   PARAMETER_GROUP_GENERIC,  QObject::tr("GPX Track Order:"),        WidgetType::IntEnumeration,   &gpx_export_trk_sort_enum,    NULL, "" },
	{ 2, PREFERENCES_NAMESPACE_IO "gpx_export_wpt_sym_names", SGVariantType::Enumeration,   PARAMETER_GROUP_GENERIC,  QObject::tr("GPX Waypoint Symbols:"),   WidgetType::IntEnumeration,   &gpx_export_wpt_symbols_enum, NULL, QObject::tr("Save GPX Waypoint Symbol names in the specified case. May be useful for compatibility with various devices") },
	{ 3,                          "",                         SGVariantType::Empty,         PARAMETER_GROUP_GENERIC,  "",                                     WidgetType::None,          NULL,                         NULL, "" }, /* Guard. */
};

#ifndef WINDOWS
static ParameterSpecification io_prefs_non_windows[] = {
	{ 0, PREFERENCES_NAMESPACE_IO "image_viewer", SGVariantType::String, PARAMETER_GROUP_GENERIC, QObject::tr("Image Viewer:"), WidgetType::FileSelector, NULL, NULL, "" },
	{ 1,                          "",             SGVariantType::Empty,  PARAMETER_GROUP_GENERIC, "",                           WidgetType::None,         NULL, NULL, "" }, /* Guard. */
};
#endif

static ParameterSpecification io_prefs_external_gpx[] = {
	{ 0, PREFERENCES_NAMESPACE_IO "external_gpx_1", SGVariantType::String, PARAMETER_GROUP_GENERIC, QObject::tr("External GPX Program 1:"), WidgetType::FileSelector, NULL, NULL, "" },
	{ 1, PREFERENCES_NAMESPACE_IO "external_gpx_2", SGVariantType::String, PARAMETER_GROUP_GENERIC, QObject::tr("External GPX Program 2:"), WidgetType::FileSelector, NULL, NULL, "" },
	{ 2,                          "",               SGVariantType::Empty,  PARAMETER_GROUP_GENERIC, "",                                     WidgetType::None,         NULL, NULL, "" }, /* Guard. */
};

static WidgetIntEnumerationData vik_fileref_enum = {
	{
		SGLabelID(QObject::tr("Absolute"), (int) FilePathFormat::Absolute),
		SGLabelID(QObject::tr("Relative"), (int) FilePathFormat::Relative),
	},
	(int) FilePathFormat::Absolute,
};

static ParameterScale<int> scale_recent_files(-1, 25, SGVariant(10, SGVariantType::Int), 1, 0); /* Viking's comment about value of hardcoded default: "Seemingly GTK's default for the number of recent files.". */

static ParameterSpecification prefs_advanced[] = {
	{ 0, PREFERENCES_NAMESPACE_ADVANCED "save_file_reference_mode",  SGVariantType::Enumeration,   PARAMETER_GROUP_GENERIC, QObject::tr("Save File Reference Mode:"),           WidgetType::IntEnumeration,  &vik_fileref_enum,    NULL, QObject::tr("When saving a Viking .vik file, this determines how the directory paths of filenames are written.") },
	{ 1, PREFERENCES_NAMESPACE_ADVANCED "ask_for_create_track_name", SGVariantType::Boolean,       PARAMETER_GROUP_GENERIC, QObject::tr("Ask for Name before Track Creation:"), WidgetType::CheckButton,  NULL,                 NULL, "" },
	{ 2, PREFERENCES_NAMESPACE_ADVANCED "create_track_tooltip",      SGVariantType::Boolean,       PARAMETER_GROUP_GENERIC, QObject::tr("Show Tooltip during Track Creation:"), WidgetType::CheckButton,  NULL,                 NULL, "" },
	{ 3, PREFERENCES_NAMESPACE_ADVANCED "number_recent_files",       SGVariantType::Int,           PARAMETER_GROUP_GENERIC, QObject::tr("The number of recent files:"),         WidgetType::SpinBoxInt,   &scale_recent_files,  NULL, QObject::tr("Only applies to new windows or on application restart. -1 means all available files.") },
	{ 4,                                "",                          SGVariantType::Empty,         PARAMETER_GROUP_GENERIC, "",                                                 WidgetType::None,         NULL,                 NULL, "" },  /* Guard. */
};

static WidgetIntEnumerationData startup_method_enum = {
	{
		SGLabelID(QObject::tr("Home Location"),  0),
		SGLabelID(QObject::tr("Last Location"),  1),
		SGLabelID(QObject::tr("Specified File"), 2),
		SGLabelID(QObject::tr("Auto Location"),  3),
	},
	0,
};

static ParameterSpecification startup_prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_STARTUP "restore_window_state",  SGVariantType::Boolean,      PARAMETER_GROUP_GENERIC, QObject::tr("Restore Window Setup:"),    WidgetType::CheckButton,   NULL,                    NULL, QObject::tr("Restore window size and layout") },
	{ 1, PREFERENCES_NAMESPACE_STARTUP "add_default_map_layer", SGVariantType::Boolean,      PARAMETER_GROUP_GENERIC, QObject::tr("Add a Default Map Layer:"), WidgetType::CheckButton,   NULL,                    NULL, QObject::tr("The default map layer added is defined by the Layer Defaults. Use the menu Edit->Layer Defaults->Map... to change the map type and other values.") },
	{ 2, PREFERENCES_NAMESPACE_STARTUP "startup_method",        SGVariantType::Enumeration,  PARAMETER_GROUP_GENERIC, QObject::tr("Startup Method:"),          WidgetType::IntEnumeration,   &startup_method_enum,    NULL, "" },
	{ 3, PREFERENCES_NAMESPACE_STARTUP "startup_file",          SGVariantType::String,       PARAMETER_GROUP_GENERIC, QObject::tr("Startup File:"),            WidgetType::FileSelector,  NULL,                    NULL, QObject::tr("The default file to load on startup. Only applies when the startup method is set to 'Specified File'") },
	{ 4, PREFERENCES_NAMESPACE_STARTUP "check_version",         SGVariantType::Boolean,      PARAMETER_GROUP_GENERIC, QObject::tr("Check For New Version:"),   WidgetType::CheckButton,   NULL,                    NULL, QObject::tr("Periodically check to see if a new version of Viking is available") },
	{ 5,                               "",                      SGVariantType::Empty,        PARAMETER_GROUP_GENERIC, "",                                      WidgetType::None,          NULL,                    NULL, "" }, /* Guard. */
};




static Preferences preferences;
static std::vector<PreferenceTuple> g_preferences_list;
static QHash<QString, param_id_t> groups_keys_to_indices;




/* Predicate for std::find_if() for looking up preferences in g_preferences_list by name. */
struct PreferencesCompareByName {
	PreferencesCompareByName(const QString & new_param_name) : param_name(new_param_name) { }
	bool operator()(PreferenceTuple & item) const { return item.param_name == this->param_name; }
private:
	const QString param_name;
};




void Preferences::register_parameter_group(const QString & group_key, const QString & group_ui_label)
{
	static param_id_t group_id = 1;

	if (groups_keys_to_indices.contains(group_key)) {
		qDebug() << SG_PREFIX_E << "Duplicate preferences group key" << group_key;
	} else {
		preferences.group_names.insert(group_id, group_ui_label);
		groups_keys_to_indices.insert(group_key, group_id);
		group_id++;
	}
}




/* Returns -1 if not found. */
static param_id_t preferences_param_key_to_group_id(const QString & key)
{
	const int last_dot = key.lastIndexOf(".");
	if (-1 == last_dot || 0 == last_dot) {
		return -1;
	}

	qDebug() << SG_PREFIX_D << key << "<-" << key.left(last_dot + 1);

	const int index = groups_keys_to_indices[key.left(last_dot + 1)];
	if (0 == index) {
		/* "default-constructed value" of QT container for situations when there is no key-value pair in the container. */
		return PARAMETER_GROUP_GENERIC; /* Which should be -1 anyway. */
	}
	return (param_id_t) index;
}




/*****************************/




static bool preferences_load_from_file()
{
	const QString full_path = SlavGPSLocations::get_file_full_path(VIKING_PREFERENCES_FILE);
	QFile file(full_path);
	if (!file.exists()) {
		qDebug() << SG_PREFIX_E << "Failed to create file" << full_path << file.error();
		return false;
	}
	if (!file.open(QIODevice::ReadOnly)) {
		qDebug() << SG_PREFIX_E << "Failed to open file" << full_path << file.error();
		return false;
	}

	char buf[4096];
	QString key;
	QString val;
	while (!file.atEnd()) {
		if (0 >= file.readLine(buf, sizeof (buf))) {
			break;
		}
		if (!Util::split_string_from_file_on_equals(QString(buf), key, val)) {
			qDebug() << SG_PREFIX_N << "Can't split line from config file:" << buf;
			continue;
		}

		/* If the parameter is not registered, ignore it. */
		auto pref = std::find_if(g_preferences_list.begin(), g_preferences_list.end(), PreferencesCompareByName(key));
		if (pref == g_preferences_list.end()) {
			qDebug() << SG_PREFIX_N << "Ignoring key/val (parameter not registered)" << key << val;
			continue;
		}


		if (pref->param_value.type_id == SGVariantType::StringList) {
			qDebug() << SG_PREFIX_E << "'string list' variant type not implemented, skipping parameter" << key;
			continue;
		}

		/* Finally change existing value of registered parameter. */
		qDebug() << SG_PREFIX_I << "Modifying key/val" << key << val;
		const SGVariant new_val = SGVariant(pref->param_value.type_id, val); /* Build variant from string representation. */
		pref->param_value = new_val;
	}

	/* ~QFile() closes the file if necessary. */

	return true;
}




bool Preferences::set_param_value(const QString & param_name, const SGVariant & new_param_value)
{
	auto pref = std::find_if(g_preferences_list.begin(), g_preferences_list.end(), PreferencesCompareByName(param_name));
	if (pref == g_preferences_list.end()) {
		qDebug() << SG_PREFIX_E << "Parameter" << param_name << "not found";
		return false;
	}

	/* Don't change stored pointer values. */
	if (pref->param_spec.type_id == SGVariantType::Pointer) {
		return false;
	}
	if (pref->param_spec.type_id == SGVariantType::StringList) {
		qDebug() << SG_PREFIX_E << "'string list' not implemented";
		return false;
	}
	if (new_param_value.type_id != pref->param_spec.type_id) {
		qDebug() << SG_PREFIX_E << "Mismatch of variant type for parameter" << param_name << new_param_value.type_id << pref->param_spec.type_id;
		return false;
	}

	/* [] operator will return modifiable reference.  */
	pref->param_value = new_param_value;

	qDebug() << SG_PREFIX_I << "Set new value of parameter" << param_name << "=" << new_param_value;

	return true;
}




/**
   Returns: true on success.

   TODO_LATER: report errors to end user.
*/
bool Preferences::save_to_file(void)
{
	const QString full_path = SlavGPSLocations::get_file_full_path(VIKING_PREFERENCES_FILE);

	FILE * file = fopen(full_path.toUtf8().constData(), "w");
	if (!file) {
		return false;
	}
	/* Since preferences files saves OSM login credentials, it'll be better to store it in secret. */
	if (chmod(full_path.toUtf8().constData(), 0600) != 0) {
		qDebug() << SG_PREFIX_E << "Failed to set permissions on" << full_path;
		fclose(file);
		return false;
	}


	qDebug() << SG_PREFIX_I << "Saving to file" << full_path;

	for (auto pref = g_preferences_list.begin(); pref != g_preferences_list.end(); pref++) {
		if (pref->param_value.type_id == SGVariantType::Pointer) {
			/* Internal preference that should not be saved to file. */
			continue;
		}
		qDebug() << SG_PREFIX_I << "Saving param" << pref->param_spec.name << "=" << pref->param_value;
		pref->param_value.write(file, pref->param_spec.name);
	}
	fclose(file);
	file = NULL;

	return true;
}




void Preferences::show_window(QWidget * parent)
{
	if (!preferences.loaded) {
		qDebug() << SG_PREFIX_E << "Preferences haven't been loaded until now, this is bad";
		preferences_load_from_file();
		preferences.loaded = true;
	}

	PropertiesDialog dialog(QObject::tr("Preferences"), parent);
	dialog.fill(&preferences);
	if (QDialog::Accepted != dialog.exec()) {
		return;
	}


	for (auto pref = g_preferences_list.begin(); pref != g_preferences_list.end(); pref++) {

		/* It is called "new_param_value", but it's
		   really new only if it was changed in
		   preferences window. */
		const SGVariant new_param_value = dialog.get_param_value(pref->param_spec);

		qDebug() << SG_PREFIX_I << "Parameter from preferences dialog:" << pref->param_name << "=" << new_param_value;

		Preferences::set_param_value(pref->param_name, new_param_value);
	}
	Preferences::save_to_file();
}




void SlavGPS::Preferences::register_parameter_instance(const ParameterSpecification & param_spec, const SGVariant & default_value)
{
	/* In the debug message print name and name space separately, to improve debugging. */
	qDebug() << SG_PREFIX_I << "Registering preference" << param_spec.name << "=" << default_value;

	const QString & key = param_spec.name;

	/* All preferences should be registered before loading. */
	if (preferences.loaded) {
		qDebug() << SG_PREFIX_E << "Registering preference" << key << "after loading preferences from" << VIKING_PREFERENCES_FILE;
	}

	/* Copy value. */
        ParameterSpecification new_param_spec = ParameterSpecification(param_spec);

	if (default_value.type_id != new_param_spec.type_id) {
		qDebug() << SG_PREFIX_E << "Mismatch of type id for parameter" << key << default_value.type_id << new_param_spec.type_id;
	}
	new_param_spec.group_id = preferences_param_key_to_group_id(new_param_spec.name);
	if (new_param_spec.group_id == -1) {
		qDebug() << SG_PREFIX_E << "Failed to find group id for param name" << new_param_spec.name;
	}


	PreferenceTuple pref(new_param_spec.name, new_param_spec, default_value);
	g_preferences_list.push_back(pref);
}




void Preferences::uninit()
{
	g_preferences_list.clear();
}




SGVariant Preferences::get_param_value(const QString & param_name)
{
	SGVariant empty; /* Will have type id == Empty */

	if (!preferences.loaded) {
		qDebug() << SG_PREFIX_D << "The function has been called for the first time (param name is" << param_name << ")";

		/* Since we can't load the file in Preferences::init() (no params registered yet),
		   do it once before we get the first param name. */
		preferences_load_from_file();
		preferences.loaded = true;
	}

	auto pref = std::find_if(g_preferences_list.begin(), g_preferences_list.end(), PreferencesCompareByName(param_name));
	if (pref == g_preferences_list.end()) {
		return empty;
	} else {
		return pref->param_value;
	}
}




std::vector<PreferenceTuple>::iterator Preferences::begin()
{
	return g_preferences_list.begin();
}




std::vector<PreferenceTuple>::iterator Preferences::end()
{
	return g_preferences_list.end();
}




bool Preferences::get_restore_window_state(void)
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_STARTUP "restore_window_state").u.val_bool;
}




/* Defaults for preferences are setup here. */
void Preferences::register_default_values()
{
	qDebug() << SG_PREFIX_D << "VIKING VERSION as number:" << SGUtils::version_to_number(PACKAGE_VERSION);

	int i = 0;

	/* New tab. */
	Preferences::register_parameter_group(PREFERENCES_NAMESPACE_GENERAL, QObject::tr("General"));

	i = 0;
	Preferences::register_parameter_instance(general_prefs[i], SGVariant((int32_t) DegreeFormat::DMS, general_prefs[i].type_id));
	i++;
	Preferences::register_parameter_instance(general_prefs[i], SGVariant((int32_t) DistanceType::Unit::E::Kilometres, general_prefs[i].type_id));
	i++;
	Preferences::register_parameter_instance(general_prefs[i], SGVariant((int32_t) SpeedType::Unit::E::KilometresPerHour, general_prefs[i].type_id));
	i++;
	Preferences::register_parameter_instance(general_prefs[i], SGVariant((int32_t) AltitudeType::Unit::E::Metres, general_prefs[i].type_id));
	i++;
	Preferences::register_parameter_instance(general_prefs[i], SGVariant(true, general_prefs[i].type_id));
	i++;
	Preferences::register_parameter_instance(general_prefs[i], hardcoded_latitude_value);
	i++;
	Preferences::register_parameter_instance(general_prefs[i], hardcoded_longitude_value);
	i++;
	Preferences::register_parameter_instance(general_prefs[i], SGVariant((int32_t) SGTimeReference::Locale, general_prefs[i].type_id));


	/* New Tab. */
	Preferences::register_parameter_group(PREFERENCES_NAMESPACE_STARTUP, QObject::tr("Startup"));

	i = 0;
	Preferences::register_parameter_instance(startup_prefs[i], SGVariant(false, startup_prefs[i].type_id));
	i++;
	Preferences::register_parameter_instance(startup_prefs[i], SGVariant(false, startup_prefs[i].type_id));
	i++;
	Preferences::register_parameter_instance(startup_prefs[i], SGVariant((int32_t) StartupMethod::HomeLocation, startup_prefs[i].type_id));
	i++;
	Preferences::register_parameter_instance(startup_prefs[i], SGVariant("", startup_prefs[i].type_id));
	i++;
	Preferences::register_parameter_instance(startup_prefs[i], SGVariant(false, startup_prefs[i].type_id));


	/* New Tab. */
	Preferences::register_parameter_group(PREFERENCES_NAMESPACE_IO, QObject::tr("Export/External"));

	i = 0;
	Preferences::register_parameter_instance(io_prefs[i], SGVariant((int32_t) KMLExportUnits::Metric, io_prefs[i].type_id));
	i++;
	Preferences::register_parameter_instance(io_prefs[i], SGVariant((int32_t) GPXExportTrackSort::ByTime, io_prefs[i].type_id));
	i++;
	Preferences::register_parameter_instance(io_prefs[i], SGVariant((int32_t) GPXExportWptSymName::Titlecase, io_prefs[i].type_id));
	i++;

#ifndef WINDOWS
	Preferences::register_parameter_instance(io_prefs_non_windows[0], SGVariant("xdg-open", io_prefs_non_windows[0].type_id));
#endif

	i = 0;
	/* JOSM for OSM editing around a GPX track. */
	Preferences::register_parameter_instance(io_prefs_external_gpx[i], SGVariant("josm", io_prefs_external_gpx[i].type_id));
	i++;
	/* Add a second external program - another OSM editor by default. */
	Preferences::register_parameter_instance(io_prefs_external_gpx[i], SGVariant("merkaartor", io_prefs_external_gpx[i].type_id));
	i++;


	/* New tab. */
	Preferences::register_parameter_group(PREFERENCES_NAMESPACE_ADVANCED, QObject::tr("Advanced"));

	i = 0;
	Preferences::register_parameter_instance(prefs_advanced[i], SGVariant((int32_t) FilePathFormat::Absolute, prefs_advanced[i].type_id));
	i++;
	Preferences::register_parameter_instance(prefs_advanced[i], SGVariant(true, prefs_advanced[i].type_id));
	i++;
	Preferences::register_parameter_instance(prefs_advanced[i], SGVariant(true, prefs_advanced[i].type_id));
	i++;
	Preferences::register_parameter_instance(prefs_advanced[i], scale_recent_files.initial);
	i++;
}




DegreeFormat Preferences::get_degree_format(void)
{
	return (DegreeFormat) Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL "degree_format").u.val_enumeration;
}




DistanceType::Unit Preferences::get_unit_distance()
{
	return DistanceType::Unit((DistanceType::Unit::E) Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL "units_distance").u.val_enumeration);
}




SpeedType::Unit Preferences::get_unit_speed(void)
{
	return SpeedType::Unit((SpeedType::Unit::E) Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL "units_speed").u.val_enumeration);
}




AltitudeType::Unit Preferences::get_unit_height(void)
{
	return AltitudeType::Unit((AltitudeType::Unit::E) Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL "units_height").u.val_enumeration);
}


//

bool Preferences::get_use_large_waypoint_icons()
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL "use_large_waypoint_icons").u.val_bool;
}




Latitude Preferences::get_default_lat(void)
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL "default_latitude").get_latitude();
}




Longitude Preferences::get_default_lon(void)
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL "default_longitude").get_longitude();
}




SGTimeReference Preferences::get_time_ref_frame()
{
	return (SGTimeReference) Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL "time_reference_frame").u.val_enumeration;
}




/* External/Export Options. */

KMLExportUnits Preferences::get_kml_export_units()
{
	return (KMLExportUnits) Preferences::get_param_value(PREFERENCES_NAMESPACE_IO "kml_export_units").u.val_enumeration;
}




GPXExportTrackSort Preferences::get_gpx_export_trk_sort()
{
	return (GPXExportTrackSort) Preferences::get_param_value(PREFERENCES_NAMESPACE_IO "gpx_export_track_sort").u.val_enumeration;
}




GPXExportWptSymName Preferences::get_gpx_export_wpt_sym_name()
{
	return (GPXExportWptSymName) Preferences::get_param_value(PREFERENCES_NAMESPACE_IO "gpx_export_wpt_sym_names").u.val_enumeration;
}




#ifndef WINDOWS
QString Preferences::get_image_viewer(void)
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_IO "image_viewer").val_string;
}
#endif




QString Preferences::get_external_gpx_program_1(void)
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_IO "external_gpx_1").val_string;
}

QString Preferences::get_external_gpx_program_2(void)
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_IO "external_gpx_2").val_string;
}




/* Advanced Options */

FilePathFormat Preferences::get_file_path_format()
{
	return (FilePathFormat) Preferences::get_param_value(PREFERENCES_NAMESPACE_ADVANCED "save_file_reference_mode").u.val_enumeration;
}




bool Preferences::get_ask_for_create_track_name()
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_ADVANCED "ask_for_create_track_name").u.val_bool;
}




bool Preferences::get_create_track_tooltip()
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_ADVANCED "create_track_tooltip").u.val_bool;
}




int Preferences::get_recent_number_files()
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_ADVANCED "number_recent_files").u.val_int;
}




bool Preferences::get_add_default_map_layer()
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_STARTUP "add_default_map_layer").u.val_bool;
}




StartupMethod Preferences::get_startup_method()
{
	return (StartupMethod) Preferences::get_param_value(PREFERENCES_NAMESPACE_STARTUP "startup_method").u.val_enumeration;
}




QString Preferences::get_startup_file(void)
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_STARTUP "startup_file").val_string;
}




bool Preferences::get_check_version()
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_STARTUP "check_version").u.val_bool;
}
