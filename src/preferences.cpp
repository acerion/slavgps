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
#include <map>
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
#include "globals.h"
#include "ui_builder.h"
#include "vikutils.h"




using namespace SlavGPS;




static std::vector<SGLabelID> params_degree_formats = {
	SGLabelID("DDD", (int) DegreeFormat::DDD),
	SGLabelID("DMM", (int) DegreeFormat::DMM),
	SGLabelID("DMS", (int) DegreeFormat::DMS),
	SGLabelID("Raw", (int) DegreeFormat::RAW),
};

static std::vector<SGLabelID> params_units_distance = {
	SGLabelID("Kilometres",     (int) DistanceUnit::KILOMETRES),
	SGLabelID("Miles",          (int) DistanceUnit::MILES),
	SGLabelID("Nautical Miles", (int) DistanceUnit::NAUTICAL_MILES),
};

static std::vector<SGLabelID> params_units_speed = {
	SGLabelID("km/h",  (int) SpeedUnit::KILOMETRES_PER_HOUR),
	SGLabelID("mph",   (int) SpeedUnit::MILES_PER_HOUR),
	SGLabelID("m/s",   (int) SpeedUnit::METRES_PER_SECOND),
	SGLabelID("knots", (int) SpeedUnit::KNOTS)
};

static std::vector<SGLabelID> params_units_height = {
	SGLabelID("Metres", (int) HeightUnit::METRES),
	SGLabelID("Feet",   (int) HeightUnit::FEET),
};


/* Hardwired default location is New York. */
static ParameterScale scale_lat = {  -90.0,  90.0, SGVariant(40.714490),  0.05, 2 };
static ParameterScale scale_lon = { -180.0, 180.0, SGVariant(-74.007130), 0.05, 2 };

static std::vector<SGLabelID> params_time_ref_frame = {
	SGLabelID("Locale", 0),
	SGLabelID("World",  1),
	SGLabelID("UTC",    2),
};

static ParameterSpecification general_prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_GENERAL "degree_format",            SGVariantType::INT,     PARAMETER_GROUP_GENERIC, N_("Degree format:"),            WidgetType::COMBOBOX,        &params_degree_formats, NULL, NULL, NULL },
	{ 1, PREFERENCES_NAMESPACE_GENERAL "units_distance",           SGVariantType::INT,     PARAMETER_GROUP_GENERIC, N_("Distance units:"),           WidgetType::COMBOBOX,        &params_units_distance, NULL, NULL, NULL },
	{ 2, PREFERENCES_NAMESPACE_GENERAL "units_speed",              SGVariantType::INT,     PARAMETER_GROUP_GENERIC, N_("Speed units:"),              WidgetType::COMBOBOX,        &params_units_speed,    NULL, NULL, NULL },
	{ 3, PREFERENCES_NAMESPACE_GENERAL "units_height",             SGVariantType::INT,     PARAMETER_GROUP_GENERIC, N_("Height units:"),             WidgetType::COMBOBOX,        &params_units_height,   NULL, NULL, NULL },
	{ 4, PREFERENCES_NAMESPACE_GENERAL "use_large_waypoint_icons", SGVariantType::BOOLEAN, PARAMETER_GROUP_GENERIC, N_("Use large waypoint icons:"), WidgetType::CHECKBUTTON,     NULL,                   NULL, NULL, NULL },
	{ 5, PREFERENCES_NAMESPACE_GENERAL "default_latitude",         SGVariantType::DOUBLE,  PARAMETER_GROUP_GENERIC, N_("Default latitude:"),         WidgetType::SPINBOX_DOUBLE,  &scale_lat,             NULL, NULL, NULL },
	{ 6, PREFERENCES_NAMESPACE_GENERAL "default_longitude",        SGVariantType::DOUBLE,  PARAMETER_GROUP_GENERIC, N_("Default longitude:"),        WidgetType::SPINBOX_DOUBLE,  &scale_lon,             NULL, NULL, NULL },
	{ 7, PREFERENCES_NAMESPACE_GENERAL "time_reference_frame",     SGVariantType::INT,     PARAMETER_GROUP_GENERIC, N_("Time Display:"),             WidgetType::COMBOBOX,        &params_time_ref_frame, NULL, NULL, N_("Display times according to the reference frame. Locale is the user's system setting. World is relative to the location of the object.") },

	{ 8, NULL,                                                     SGVariantType::EMPTY,   PARAMETER_GROUP_GENERIC, NULL,                            WidgetType::NONE,            NULL,                   NULL, NULL, NULL },
};

/* External/Export Options */

static std::vector<SGLabelID> params_kml_export_units = {
	SGLabelID("Metric",   0),
	SGLabelID("Statute",  1),
	SGLabelID("Nautical", 2),
};

static std::vector<SGLabelID> params_gpx_export_trk_sort = {
	SGLabelID("Alphabetical", 0),
	SGLabelID("Time",         1),
	SGLabelID("Creation",     2),
};

static std::vector<SGLabelID> params_gpx_export_wpt_symbols = {
	SGLabelID("Title Case", 0),
	SGLabelID("Lowercase",  1),
};

static ParameterSpecification io_prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_IO "kml_export_units",         SGVariantType::INT, PARAMETER_GROUP_GENERIC, N_("KML File Export Units:"), WidgetType::COMBOBOX, &params_kml_export_units,       NULL, NULL, NULL },
	{ 1, PREFERENCES_NAMESPACE_IO "gpx_export_track_sort",    SGVariantType::INT, PARAMETER_GROUP_GENERIC, N_("GPX Track Order:"),       WidgetType::COMBOBOX, &params_gpx_export_trk_sort,    NULL, NULL, NULL },
	{ 2, PREFERENCES_NAMESPACE_IO "gpx_export_wpt_sym_names", SGVariantType::INT, PARAMETER_GROUP_GENERIC, N_("GPX Waypoint Symbols:"),  WidgetType::COMBOBOX, &params_gpx_export_wpt_symbols, NULL, NULL, N_("Save GPX Waypoint Symbol names in the specified case. May be useful for compatibility with various devices") },
	{ 3, NULL,                                                SGVariantType::EMPTY, PARAMETER_GROUP_GENERIC, NULL,                         WidgetType::NONE,     NULL,                           NULL, NULL, NULL }, /* Guard. */
};

#ifndef WINDOWS
static ParameterSpecification io_prefs_non_windows[] = {
	{ 0, PREFERENCES_NAMESPACE_IO "image_viewer", SGVariantType::STRING, PARAMETER_GROUP_GENERIC, N_("Image Viewer:"), WidgetType::FILEENTRY, NULL, NULL, NULL, NULL },
	{ 1, NULL,                                    SGVariantType::EMPTY,  PARAMETER_GROUP_GENERIC, NULL,                WidgetType::NONE,      NULL, NULL, NULL, NULL }, /* Guard. */
};
#endif

static ParameterSpecification io_prefs_external_gpx[] = {
	{ 0, PREFERENCES_NAMESPACE_IO "external_gpx_1", SGVariantType::STRING, PARAMETER_GROUP_GENERIC, N_("External GPX Program 1:"), WidgetType::FILEENTRY, NULL, NULL, NULL, NULL },
	{ 1, PREFERENCES_NAMESPACE_IO "external_gpx_2", SGVariantType::STRING, PARAMETER_GROUP_GENERIC, N_("External GPX Program 2:"), WidgetType::FILEENTRY, NULL, NULL, NULL, NULL },
	{ 2, NULL,                                      SGVariantType::EMPTY,  PARAMETER_GROUP_GENERIC, NULL,                          WidgetType::NONE,      NULL, NULL, NULL, NULL }, /* Guard. */
};

static std::vector<SGLabelID> params_vik_fileref = {
	SGLabelID("Absolute", 0),
	SGLabelID("Relative", 1),
};

static ParameterScale scale_recent_files = { -1, 25, SGVariant((int32_t) 10), 1, 0 }; /* Viking's comment about value of hardwired default: "Seemingly GTK's default for the number of recent files.". */

static ParameterSpecification prefs_advanced[] = {
	{ 0, PREFERENCES_NAMESPACE_ADVANCED "save_file_reference_mode",  SGVariantType::INT,     PARAMETER_GROUP_GENERIC, N_("Save File Reference Mode:"),           WidgetType::COMBOBOX,    &params_vik_fileref,  NULL, NULL, N_("When saving a Viking .vik file, this determines how the directory paths of filenames are written.") },
	{ 1, PREFERENCES_NAMESPACE_ADVANCED "ask_for_create_track_name", SGVariantType::BOOLEAN, PARAMETER_GROUP_GENERIC, N_("Ask for Name before Track Creation:"), WidgetType::CHECKBUTTON, NULL,                 NULL, NULL, NULL },
	{ 2, PREFERENCES_NAMESPACE_ADVANCED "create_track_tooltip",      SGVariantType::BOOLEAN, PARAMETER_GROUP_GENERIC, N_("Show Tooltip during Track Creation:"), WidgetType::CHECKBUTTON, NULL,                 NULL, NULL, NULL },
	{ 3, PREFERENCES_NAMESPACE_ADVANCED "number_recent_files",       SGVariantType::INT,     PARAMETER_GROUP_GENERIC, N_("The number of recent files:"),         WidgetType::SPINBOX_INT, &scale_recent_files,  NULL, NULL, N_("Only applies to new windows or on application restart. -1 means all available files.") },
	{ 4, NULL,                                                       SGVariantType::EMPTY,   PARAMETER_GROUP_GENERIC, NULL,                                      WidgetType::NONE,        NULL,                 NULL, NULL, NULL },  /* Guard. */
};

static std::vector<SGLabelID> params_startup_methods = {
	SGLabelID("Home Location",  0),
	SGLabelID("Last Location",  1),
	SGLabelID("Specified File", 2),
	SGLabelID("Auto Location",  3),
};

static ParameterSpecification startup_prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_STARTUP "restore_window_state",  SGVariantType::BOOLEAN, PARAMETER_GROUP_GENERIC, N_("Restore Window Setup:"),    WidgetType::CHECKBUTTON, NULL,                    NULL, NULL, N_("Restore window size and layout") },
	{ 1, PREFERENCES_NAMESPACE_STARTUP "add_default_map_layer", SGVariantType::BOOLEAN, PARAMETER_GROUP_GENERIC, N_("Add a Default Map Layer:"), WidgetType::CHECKBUTTON, NULL,                    NULL, NULL, N_("The default map layer added is defined by the Layer Defaults. Use the menu Edit->Layer Defaults->Map... to change the map type and other values.") },
	{ 2, PREFERENCES_NAMESPACE_STARTUP "startup_method",        SGVariantType::INT,     PARAMETER_GROUP_GENERIC, N_("Startup Method:"),          WidgetType::COMBOBOX,    &params_startup_methods, NULL, NULL, NULL },
	{ 3, PREFERENCES_NAMESPACE_STARTUP "startup_file",          SGVariantType::STRING,  PARAMETER_GROUP_GENERIC, N_("Startup File:"),            WidgetType::FILEENTRY,   NULL,                    NULL, NULL, N_("The default file to load on startup. Only applies when the startup method is set to 'Specified File'") },
	{ 4, PREFERENCES_NAMESPACE_STARTUP "check_version",         SGVariantType::BOOLEAN, PARAMETER_GROUP_GENERIC, N_("Check For New Version:"),   WidgetType::CHECKBUTTON, NULL,                    NULL, NULL, N_("Periodically check to see if a new version of Viking is available") },
	{ 5, NULL,                                                  SGVariantType::EMPTY,   PARAMETER_GROUP_GENERIC, NULL,                           WidgetType::NONE,        NULL,                    NULL, NULL, NULL }, /* Guard. */
};
/* End of Options static stuff. */





static Preferences preferences;




/* TODO: STRING_LIST. */
/* TODO: share code in file reading. */
/* TODO: remove hackaround in show_window. */




#define VIKING_PREFERENCES_FILE "viking.prefs"



/* How to get a value of parameter:
   1. use id to get a parameter from registered_parameter_specifications, then
   2. use parameter->name to look up parameter's value in registered_parameter_values.
   You probably can skip right to point 2 if you know full name of parameter.

   How to set a value of parameter:
   1.
   2.
*/
static std::map<param_id_t, ParameterSpecification *> registered_parameter_specifications; /* Parameter id -> Parameter Specification. */
static std::map<std::string, SGVariant *> registered_parameter_values; /* Parameter name -> Value of parameter. */




/************ groups *********/




static QHash<QString, param_id_t> groups_keys_to_indices;




void Preferences::register_group(const QString & group_key, const QString & group_name)
{
	static param_id_t group_id = 1;

	if (groups_keys_to_indices.contains(group_key)) {
		qDebug() << "EE: Preferences: duplicate preferences group key" << group_key;
	} else {
		preferences.group_names.insert(group_id, group_name);
		groups_keys_to_indices.insert(group_key, group_id);
		group_id++;
	}
}




/* Returns -1 if not found. */
static param_id_t preferences_group_key_to_group_id(const QString & key)
{
	int index = groups_keys_to_indices[key];
	if (0 == index) {
		/* "default-constructed value" of QT container for situations when there is no key-value pair in the container. */
		return PARAMETER_GROUP_GENERIC; /* Which should be -1 anyway. */
	}
	return (param_id_t) index;
}




/*****************************/




static bool preferences_load_from_file()
{
	const QString full_path = get_viking_dir() + QDir::separator() + VIKING_PREFERENCES_FILE;
	FILE * file = fopen(full_path.toUtf8().constData(), "r");

	if (!file) {
		return false;
	}

	char buf[4096];
	char * key = NULL;
	char * val = NULL;
	while (!feof(file)) {
		if (fgets(buf,sizeof (buf), file) == NULL) {
			break;
		}
		if (split_string_from_file_on_equals(buf, &key, &val)) {
			/* If it's not in there, ignore it. */
			auto old_val_iter = registered_parameter_values.find(key);
			if (old_val_iter == registered_parameter_values.end()) {
				qDebug() << "II: Preferences: load from file: ignoring key/val" << key << val;
				free(key);
				free(val);
				continue;
			} else {
				qDebug() << "II: Preferences: load from file: modifying key/val" << key << val;
			}


			/* Otherwise change it (you know the type!).
			   If it's a string list do some funky stuff ... yuck... not yet. */
			if (old_val_iter->second->type_id == SGVariantType::STRING_LIST) {
				qDebug() << "EE: Preferences: Load from File: 'string list' not implemented";
			}

			SGVariant * new_val = new SGVariant(val, old_val_iter->second->type_id);
			registered_parameter_values.at(std::string(key)) = new_val;

			free(key);
			free(val);
			/* Change value. */
		}
	}
	fclose(file);
	file = NULL;

	return true;
}




bool Preferences::set_param_value(param_id_t id, const SGVariant & value)
{
	/* Don't change stored pointer values. */
	if (registered_parameter_specifications[id]->type == SGVariantType::PTR) {
		return false;
	}
	if (registered_parameter_specifications[id]->type == SGVariantType::STRING_LIST) {
		qDebug() << "EE: Preferences: Set Parameter Value: 'string list' not implemented";
		return false;
	}
	if (value.type_id != registered_parameter_specifications[id]->type) {
		qDebug() << "EE: Preferences: mismatch of variant type for parameter" << registered_parameter_specifications[id]->name;
		return false;
	}

	SGVariant * new_val = new SGVariant(value); /* New value to save under an existing name. */
	/* FIXME: delete old value first? */
	registered_parameter_values.at(std::string(registered_parameter_specifications[id]->name)) = new_val;

	if (registered_parameter_specifications[id]->type == SGVariantType::DOUBLE) {
		qDebug() << "II: Preferences: saved parameter #" << id << registered_parameter_specifications[id]->name << new_val->d;
	}

	return true;
}




bool Preferences::set_param_value(const char * name, const SGVariant & value)
{
	param_id_t id = 0;
	for (auto iter = registered_parameter_specifications.begin(); iter != registered_parameter_specifications.end(); iter++) {
		if (0 == strcmp(iter->second->name, name)) {
			qDebug() << "II: Preferences: Set Param Value: setting value of param" << name;
			return preferences.set_param_value(iter->first, value);
		}
	}

	return false;
}




SGVariant Preferences::get_param_value(param_id_t id)
{
	auto val = registered_parameter_values.find(std::string(registered_parameter_specifications[id]->name));
	assert (val != registered_parameter_values.end());

	if (val->second->type_id == SGVariantType::STRING_LIST) {
		qDebug() << "EE: Preferences: Get Param Value: 'string list' not implemented in preferences";
	}
	return *val->second;
}




/**
 * Returns: true on success.
 */
bool Preferences::save_to_file(void)
{
	const QString full_path = get_viking_dir() + QDir::separator() + VIKING_PREFERENCES_FILE;

	FILE * file = fopen(full_path.toUtf8().constData(), "w");
	/* Since preferences files saves OSM login credentials, it'll be better to store it in secret. */
	if (chmod(full_path.toUtf8().constData(), 0600) != 0) {
		qDebug() << "WW: Preferences: failed to set permissions on" << full_path; /* TODO: shouldn't we abort saving to file? */
	}

	if (!file) {
		return false;
	}

	for (unsigned int i = 0; i < registered_parameter_specifications.size(); i++) {
		ParameterSpecification * param = registered_parameter_specifications[i];
		auto val_iter = registered_parameter_values.find(std::string(param->name));
		if (val_iter != registered_parameter_values.end()) {
			if (val_iter->second->type_id != SGVariantType::PTR) {
				if (val_iter->second->type_id == SGVariantType::DOUBLE) {
					qDebug() << "II: Preferences: saving to file" << param->name << (double) val_iter->second->d;
				}
				file_write_layer_param(file, param->name, val_iter->second->type_id, val_iter->second);
			}
		}
	}
	fclose(file);
	file = NULL;

	return true;
}




void Preferences::show_window(QWidget * parent)
{
	// preferences.loaded = true;
	// preferences_load_from_file();

	PropertiesDialog dialog(QObject::tr("Preferences"), parent);
	dialog.fill(&preferences);

	if (QDialog::Accepted == dialog.exec()) {
		for (auto iter = registered_parameter_specifications.begin(); iter != registered_parameter_specifications.end(); iter++) {
			const SGVariant param_value = dialog.get_param_value(iter->first, iter->second);
			if (iter->second->type == SGVariantType::DOUBLE) {
				qDebug() << "II: Preferences: extracted from dialog parameter #" << iter->first << iter->second->name << param_value.d;
			}
			qDebug() << "II: Preferences: extracted from dialog parameter #" << iter->first << iter->second->name;
			preferences.set_param_value(iter->first, param_value);
		}
		Preferences::save_to_file();
	}
}




void SlavGPS::Preferences::register_parameter(ParameterSpecification * parameter, const SGVariant & default_value, const char * group_key)
{
	static param_id_t param_id = 0;

	/* All preferences should be registered before loading. */
	if (preferences.loaded) {
		qDebug() << "EE: Preferences: registering preference" << parameter->name << "after loading preferences from" << VIKING_PREFERENCES_FILE;
	}

	/* Copy value. */
        ParameterSpecification * new_parameter = new ParameterSpecification;
	*new_parameter = *parameter;

	if (default_value.type_id != parameter->type) {
		qDebug() << "EE: Preferences: Register Parameter: mismatch of type id for parameter" << parameter->name;
	}
	SGVariant * new_val = new SGVariant(default_value);
	if (group_key) {
		new_parameter->group_id = preferences_group_key_to_group_id(QString(group_key));
	}

	registered_parameter_specifications.insert(std::pair<param_id_t, ParameterSpecification *>(param_id, new_parameter));
	registered_parameter_values.insert(std::pair<std::string, SGVariant *>(std::string(new_parameter->name), new_val));
	param_id++;
}




#ifdef K
void Preferences::Preferences()
{
}
#endif




void Preferences::uninit()
{
	registered_parameter_specifications.clear();
	registered_parameter_values.clear(); /* TODO: delete objects stored in the container as well? */
}




SGVariant * SlavGPS::a_preferences_get(const char * key)
{
	if (!preferences.loaded) {
		qDebug() << "DD: Preferences: calling _get() for the first time (key is" << key << ")";

		/* Since we can't load the file in Preferences::init() (no params registered yet),
		   do it once before we get the first key. */
		preferences_load_from_file();
		preferences.loaded = true;
	}

	auto val_iter = registered_parameter_values.find(std::string(key));
	if (val_iter == registered_parameter_values.end()) {
		return NULL;
	} else {
		return val_iter->second;
	}
}




std::map<param_id_t, ParameterSpecification *>::iterator Preferences::begin()
{
	return registered_parameter_specifications.begin();
}




std::map<param_id_t, ParameterSpecification *>::iterator Preferences::end()
{
	return registered_parameter_specifications.end();
}




bool Preferences::get_restore_window_state(void)
{
	return a_preferences_get(PREFERENCES_NAMESPACE_STARTUP "restore_window_state")->b;
}




void Preferences::register_default_values()
{
	qDebug() << "DD: Preferences: VIKING VERSION as number:" << viking_version_to_number(VIKING_VERSION);

	/* Defaults for the options are setup here. */
	Preferences::register_group(PREFERENCES_GROUP_KEY_GENERAL, QObject::tr("General"));

	SGVariant tmp;
	tmp = SGVariant((int32_t) DegreeFormat::DMS);
	Preferences::register_parameter(&general_prefs[0], tmp, PREFERENCES_GROUP_KEY_GENERAL);

	tmp = SGVariant((int32_t) DistanceUnit::KILOMETRES);
	Preferences::register_parameter(&general_prefs[1], tmp, PREFERENCES_GROUP_KEY_GENERAL);

	tmp = SGVariant((int32_t) SpeedUnit::KILOMETRES_PER_HOUR);
	Preferences::register_parameter(&general_prefs[2], tmp, PREFERENCES_GROUP_KEY_GENERAL);

	tmp = SGVariant((int32_t) HeightUnit::METRES);
	Preferences::register_parameter(&general_prefs[3], tmp, PREFERENCES_GROUP_KEY_GENERAL);

	tmp = SGVariant(true);
	Preferences::register_parameter(&general_prefs[4], tmp, PREFERENCES_GROUP_KEY_GENERAL);

	Preferences::register_parameter(&general_prefs[5], scale_lat.initial, PREFERENCES_GROUP_KEY_GENERAL);
	Preferences::register_parameter(&general_prefs[6], scale_lon.initial, PREFERENCES_GROUP_KEY_GENERAL);

	tmp = SGVariant((int32_t) VIK_TIME_REF_LOCALE);
	Preferences::register_parameter(&general_prefs[7], tmp, PREFERENCES_GROUP_KEY_GENERAL);

	/* New Tab. */
	Preferences::register_group(PREFERENCES_GROUP_KEY_STARTUP, QObject::tr("Startup"));

	tmp = SGVariant(false);
	Preferences::register_parameter(&startup_prefs[0], tmp, PREFERENCES_GROUP_KEY_STARTUP);

	tmp = SGVariant(false);
	Preferences::register_parameter(&startup_prefs[1], tmp, PREFERENCES_GROUP_KEY_STARTUP);

	tmp = SGVariant((int32_t) VIK_STARTUP_METHOD_HOME_LOCATION);
	Preferences::register_parameter(&startup_prefs[2], tmp, PREFERENCES_GROUP_KEY_STARTUP);

	tmp = SGVariant("");
	Preferences::register_parameter(&startup_prefs[3], tmp, PREFERENCES_GROUP_KEY_STARTUP);

	tmp = SGVariant(false);
	Preferences::register_parameter(&startup_prefs[4], tmp, PREFERENCES_GROUP_KEY_STARTUP);

	/* New Tab. */
	Preferences::register_group(PREFERENCES_GROUP_KEY_IO, QObject::tr("Export/External"));

	tmp = SGVariant((int32_t) VIK_KML_EXPORT_UNITS_METRIC);
	Preferences::register_parameter(&io_prefs[0], tmp, PREFERENCES_GROUP_KEY_IO);

	tmp = SGVariant((int32_t) VIK_GPX_EXPORT_TRK_SORT_TIME);
	Preferences::register_parameter(&io_prefs[1], tmp, PREFERENCES_GROUP_KEY_IO);

	tmp = SGVariant((int32_t) VIK_GPX_EXPORT_WPT_SYM_NAME_TITLECASE);
	Preferences::register_parameter(&io_prefs[2], tmp, PREFERENCES_GROUP_KEY_IO);

#ifndef WINDOWS
	tmp = SGVariant("xdg-open");
	Preferences::register_parameter(&io_prefs_non_windows[0], tmp, PREFERENCES_GROUP_KEY_IO);
#endif

	/* JOSM for OSM editing around a GPX track. */
	tmp = SGVariant("josm");
	Preferences::register_parameter(&io_prefs_external_gpx[0], tmp, PREFERENCES_GROUP_KEY_IO);
	/* Add a second external program - another OSM editor by default. */
	tmp = SGVariant("merkaartor");
	Preferences::register_parameter(&io_prefs_external_gpx[1], tmp, PREFERENCES_GROUP_KEY_IO);

	/* 'Advanced' Properties. */
	Preferences::register_group(PREFERENCES_GROUP_KEY_ADVANCED, QObject::tr("Advanced"));

	tmp = SGVariant((int32_t) VIK_FILE_REF_FORMAT_ABSOLUTE);
	Preferences::register_parameter(&prefs_advanced[0], tmp, PREFERENCES_GROUP_KEY_ADVANCED);

	tmp = SGVariant(true);
	Preferences::register_parameter(&prefs_advanced[1], tmp, PREFERENCES_GROUP_KEY_ADVANCED);

	tmp = SGVariant(true);
	Preferences::register_parameter(&prefs_advanced[2], tmp, PREFERENCES_GROUP_KEY_ADVANCED);

	Preferences::register_parameter(&prefs_advanced[3], scale_recent_files.initial, PREFERENCES_GROUP_KEY_ADVANCED);
}




DegreeFormat Preferences::get_degree_format(void)
{
	return (DegreeFormat) a_preferences_get(PREFERENCES_NAMESPACE_GENERAL "degree_format")->i;
}




DistanceUnit Preferences::get_unit_distance()
{
	return (DistanceUnit) a_preferences_get(PREFERENCES_NAMESPACE_GENERAL "units_distance")->i;
}




SpeedUnit Preferences::get_unit_speed(void)
{
	return (SpeedUnit) a_preferences_get(PREFERENCES_NAMESPACE_GENERAL "units_speed")->i;
}




HeightUnit Preferences::get_unit_height(void)
{
	return (HeightUnit) a_preferences_get(PREFERENCES_NAMESPACE_GENERAL "units_height")->i;
}


//

bool Preferences::get_use_large_waypoint_icons()
{
	return a_preferences_get(PREFERENCES_NAMESPACE_GENERAL "use_large_waypoint_icons")->b;
}




double Preferences::get_default_lat()
{
	double data = a_preferences_get(PREFERENCES_NAMESPACE_GENERAL "default_latitude")->d;
	//return data;
	return 55.0;
}




double Preferences::get_default_lon()
{
	double data = a_preferences_get(PREFERENCES_NAMESPACE_GENERAL "default_longitude")->d;
	//return data;
	return 16.0;
}




vik_time_ref_frame_t Preferences::get_time_ref_frame()
{
	return (vik_time_ref_frame_t) a_preferences_get(PREFERENCES_NAMESPACE_GENERAL "time_reference_frame")->i;
}




/* External/Export Options. */

vik_kml_export_units_t Preferences::get_kml_export_units()
{
	return (vik_kml_export_units_t) a_preferences_get(PREFERENCES_NAMESPACE_IO "kml_export_units")->i;
}




vik_gpx_export_trk_sort_t Preferences::get_gpx_export_trk_sort()
{
	return (vik_gpx_export_trk_sort_t) a_preferences_get(PREFERENCES_NAMESPACE_IO "gpx_export_track_sort")->i;
}




vik_gpx_export_wpt_sym_name_t Preferences::get_gpx_export_wpt_sym_name()
{
	return (vik_gpx_export_wpt_sym_name_t) a_preferences_get(PREFERENCES_NAMESPACE_IO "gpx_export_wpt_sym_names")->i;
}




#ifndef WINDOWS
const QString Preferences::get_image_viewer(void)
{
	return a_preferences_get(PREFERENCES_NAMESPACE_IO "image_viewer")->s;
}
#endif




const QString Preferences::get_external_gpx_program_1(void)
{
	return a_preferences_get(PREFERENCES_NAMESPACE_IO "external_gpx_1")->s;
}

const QString Preferences::get_external_gpx_program_2(void)
{
	return a_preferences_get(PREFERENCES_NAMESPACE_IO "external_gpx_2")->s;
}




/* Advanced Options */

vik_file_ref_format_t Preferences::get_file_ref_format()
{
	return (vik_file_ref_format_t) a_preferences_get(PREFERENCES_NAMESPACE_ADVANCED "save_file_reference_mode")->u;
}




bool Preferences::get_ask_for_create_track_name()
{
	return a_preferences_get(PREFERENCES_NAMESPACE_ADVANCED "ask_for_create_track_name")->b;
}




bool Preferences::get_create_track_tooltip()
{
	return a_preferences_get(PREFERENCES_NAMESPACE_ADVANCED "create_track_tooltip")->b;
}




int Preferences::get_recent_number_files()
{
	return a_preferences_get(PREFERENCES_NAMESPACE_ADVANCED "number_recent_files")->i;
}




bool Preferences::get_add_default_map_layer()
{
	return a_preferences_get(PREFERENCES_NAMESPACE_STARTUP "add_default_map_layer")->b;
}




vik_startup_method_t Preferences::get_startup_method()
{
	return (vik_startup_method_t) a_preferences_get(PREFERENCES_NAMESPACE_STARTUP "startup_method")->u;
}




const QString Preferences::get_startup_file(void)
{
	return a_preferences_get(PREFERENCES_NAMESPACE_STARTUP "startup_file")->s;
}




bool Preferences::get_check_version()
{
	return a_preferences_get(PREFERENCES_NAMESPACE_STARTUP "check_version")->b;
}
