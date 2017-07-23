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

#include "preferences.h"
#include "dir.h"
#include "file.h"
#include "util.h"
#include "globals.h"
#include "ui_builder.h"
#include "vikutils.h"




using namespace SlavGPS;




static label_id_t params_degree_formats[] = {
	{ "DDD",  (int32_t) DegreeFormat::DDD },
	{ "DMM",  (int32_t) DegreeFormat::DMM },
	{ "DMS",  (int32_t) DegreeFormat::DMS },
	{ "Raw",  (int32_t) DegreeFormat::RAW },
	{ NULL,   9                           } };

static label_id_t params_units_distance[] = {
	{ "Kilometres",     (int32_t) DistanceUnit::KILOMETRES     },
	{ "Miles",          (int32_t) DistanceUnit::MILES          },
	{ "Nautical Miles", (int32_t) DistanceUnit::NAUTICAL_MILES },
	{ NULL,             9                                      } };

static label_id_t params_units_speed[] = {
	{ "km/h",  (int32_t) SpeedUnit::KILOMETRES_PER_HOUR },
	{ "mph",   (int32_t) SpeedUnit::MILES_PER_HOUR      },
	{ "m/s",   (int32_t) SpeedUnit::METRES_PER_SECOND   },
	{ "knots", (int32_t) SpeedUnit::KNOTS               },
	{ NULL,    9                                        } };

static label_id_t params_units_height[] = {
	{ "Metres", (int32_t) HeightUnit::METRES },
	{ "Feet",   (int32_t) HeightUnit::FEET   },
	{ NULL,     9                            } };

static ParameterScale params_scales_lat[] = { {-90.0, 90.0, 0.05, 2} };
static ParameterScale params_scales_long[] = { {-180.0, 180.0, 0.05, 2} };

static label_id_t params_time_ref_frame[] = {
	{ "Locale", 0 },
	{ "World",  1 },
	{ "UTC",    2 },
	{ NULL,     3 } };

static Parameter general_prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_GENERAL "degree_format",            SGVariantType::UINT,    PARAMETER_GROUP_GENERIC, N_("Degree format:"),            WidgetType::COMBOBOX,        params_degree_formats, NULL, NULL, NULL, NULL, NULL },
	{ 1, PREFERENCES_NAMESPACE_GENERAL "units_distance",           SGVariantType::UINT,    PARAMETER_GROUP_GENERIC, N_("Distance units:"),           WidgetType::COMBOBOX,        params_units_distance, NULL, NULL, NULL, NULL, NULL },
	{ 2, PREFERENCES_NAMESPACE_GENERAL "units_speed",              SGVariantType::UINT,    PARAMETER_GROUP_GENERIC, N_("Speed units:"),              WidgetType::COMBOBOX,        params_units_speed,    NULL, NULL, NULL, NULL, NULL },
	{ 3, PREFERENCES_NAMESPACE_GENERAL "units_height",             SGVariantType::UINT,    PARAMETER_GROUP_GENERIC, N_("Height units:"),             WidgetType::COMBOBOX,        params_units_height,   NULL, NULL, NULL, NULL, NULL },
	{ 4, PREFERENCES_NAMESPACE_GENERAL "use_large_waypoint_icons", SGVariantType::BOOLEAN, PARAMETER_GROUP_GENERIC, N_("Use large waypoint icons:"), WidgetType::CHECKBUTTON,     NULL,                  NULL, NULL, NULL, NULL, NULL },
	{ 5, PREFERENCES_NAMESPACE_GENERAL "default_latitude",         SGVariantType::DOUBLE,  PARAMETER_GROUP_GENERIC, N_("Default latitude:"),         WidgetType::SPINBOX_DOUBLE,  params_scales_lat,     NULL, NULL, NULL, NULL, NULL },
	{ 6, PREFERENCES_NAMESPACE_GENERAL "default_longitude",        SGVariantType::DOUBLE,  PARAMETER_GROUP_GENERIC, N_("Default longitude:"),        WidgetType::SPINBOX_DOUBLE,  params_scales_long,    NULL, NULL, NULL, NULL, NULL },
	{ 7, PREFERENCES_NAMESPACE_GENERAL "time_reference_frame",     SGVariantType::UINT,    PARAMETER_GROUP_GENERIC, N_("Time Display:"),             WidgetType::COMBOBOX,        params_time_ref_frame, NULL, N_("Display times according to the reference frame. Locale is the user's system setting. World is relative to the location of the object."), NULL, NULL, NULL },
	{ 8, NULL,                                                     SGVariantType::UINT,    PARAMETER_GROUP_GENERIC, N_("Time Display:"),             WidgetType::COMBOBOX,        params_time_ref_frame, NULL, N_("Display times according to the reference frame. Locale is the user's system setting. World is relative to the location of the object."), NULL, NULL, NULL },
};

/* External/Export Options */

static label_id_t params_kml_export_units[] = {
	{ "Metric",   0 },
	{ "Statute",  1 },
	{ "Nautical", 2 },
	{ NULL,       3 } };

static label_id_t params_gpx_export_trk_sort[] = {
	{ "Alphabetical",  0 },
	{ "Time",          1 },
	{ "Creation",      2 },
	{ NULL,            3 } };

static label_id_t params_gpx_export_wpt_symbols[] = {
	{ "Title Case", 0 },
	{ "Lowercase",  1 },
	{ NULL,         2 } };

static Parameter io_prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_IO "kml_export_units",         SGVariantType::UINT, PARAMETER_GROUP_GENERIC, N_("KML File Export Units:"), WidgetType::COMBOBOX, params_kml_export_units,       NULL, NULL, NULL, NULL, NULL },
	{ 1, PREFERENCES_NAMESPACE_IO "gpx_export_track_sort",    SGVariantType::UINT, PARAMETER_GROUP_GENERIC, N_("GPX Track Order:"),       WidgetType::COMBOBOX, params_gpx_export_trk_sort,    NULL, NULL, NULL, NULL, NULL },
	{ 2, PREFERENCES_NAMESPACE_IO "gpx_export_wpt_sym_names", SGVariantType::UINT, PARAMETER_GROUP_GENERIC, N_("GPX Waypoint Symbols:"),  WidgetType::COMBOBOX, params_gpx_export_wpt_symbols, NULL, N_("Save GPX Waypoint Symbol names in the specified case. May be useful for compatibility with various devices"), NULL, NULL, NULL },
	{ 3, NULL,                                                SGVariantType::UINT, PARAMETER_GROUP_GENERIC, N_("GPX Waypoint Symbols:"),  WidgetType::COMBOBOX, params_gpx_export_wpt_symbols, NULL, N_("Save GPX Waypoint Symbol names in the specified case. May be useful for compatibility with various devices"), NULL, NULL, NULL },
};

#ifndef WINDOWS
static Parameter io_prefs_non_windows[] = {
	{ 0, PREFERENCES_NAMESPACE_IO "image_viewer", SGVariantType::STRING, PARAMETER_GROUP_GENERIC, N_("Image Viewer:"), WidgetType::FILEENTRY, NULL, NULL, NULL, NULL, NULL, NULL },
	{ 1, NULL,                                    SGVariantType::STRING, PARAMETER_GROUP_GENERIC, N_("Image Viewer:"), WidgetType::FILEENTRY, NULL, NULL, NULL, NULL, NULL, NULL },
};
#endif

static Parameter io_prefs_external_gpx[] = {
	{ 0, PREFERENCES_NAMESPACE_IO "external_gpx_1", SGVariantType::STRING, PARAMETER_GROUP_GENERIC, N_("External GPX Program 1:"), WidgetType::FILEENTRY, NULL, NULL, NULL, NULL, NULL, NULL },
	{ 1, PREFERENCES_NAMESPACE_IO "external_gpx_2", SGVariantType::STRING, PARAMETER_GROUP_GENERIC, N_("External GPX Program 2:"), WidgetType::FILEENTRY, NULL, NULL, NULL, NULL, NULL, NULL },
	{ 2, NULL,                                      SGVariantType::STRING, PARAMETER_GROUP_GENERIC, N_("External GPX Program 2:"), WidgetType::FILEENTRY, NULL, NULL, NULL, NULL, NULL, NULL },
};

static label_id_t params_vik_fileref[] = {
	{ "Absolute", 0 },
	{ "Relative", 1 },
	{ NULL,       2 } };

static ParameterScale params_recent_files[] = { {-1, 25, 1, 0} };

static Parameter prefs_advanced[] = {
	{ 0, PREFERENCES_NAMESPACE_ADVANCED "save_file_reference_mode",  SGVariantType::UINT,    PARAMETER_GROUP_GENERIC, N_("Save File Reference Mode:"),           WidgetType::COMBOBOX,    params_vik_fileref,  NULL, N_("When saving a Viking .vik file, this determines how the directory paths of filenames are written."), NULL, NULL, NULL },
	{ 1, PREFERENCES_NAMESPACE_ADVANCED "ask_for_create_track_name", SGVariantType::BOOLEAN, PARAMETER_GROUP_GENERIC, N_("Ask for Name before Track Creation:"), WidgetType::CHECKBUTTON, NULL,                NULL, NULL, NULL, NULL, NULL },
	{ 2, PREFERENCES_NAMESPACE_ADVANCED "create_track_tooltip",      SGVariantType::BOOLEAN, PARAMETER_GROUP_GENERIC, N_("Show Tooltip during Track Creation:"), WidgetType::CHECKBUTTON, NULL,                NULL, NULL, NULL, NULL, NULL },
	{ 3, PREFERENCES_NAMESPACE_ADVANCED "number_recent_files",       SGVariantType::INT,     PARAMETER_GROUP_GENERIC, N_("The number of recent files:"),         WidgetType::SPINBUTTON,  params_recent_files, NULL, N_("Only applies to new windows or on application restart. -1 means all available files."), NULL, NULL, NULL },
	{ 4, NULL,                                                       SGVariantType::INT,     PARAMETER_GROUP_GENERIC, N_("The number of recent files:"),         WidgetType::SPINBUTTON,  params_recent_files, NULL, N_("Only applies to new windows or on application restart. -1 means all available files."), NULL, NULL, NULL },
};

static label_id_t params_startup_methods[] = {
	{ "Home Location",  0 },
	{ "Last Location",  1 },
	{ "Specified File", 2 },
	{ "Auto Location",  3 },
	{ NULL,             4 } };

static Parameter startup_prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_STARTUP "restore_window_state",  SGVariantType::BOOLEAN, PARAMETER_GROUP_GENERIC, N_("Restore Window Setup:"),    WidgetType::CHECKBUTTON, NULL,                   NULL, N_("Restore window size and layout"), NULL, NULL, NULL},
	{ 1, PREFERENCES_NAMESPACE_STARTUP "add_default_map_layer", SGVariantType::BOOLEAN, PARAMETER_GROUP_GENERIC, N_("Add a Default Map Layer:"), WidgetType::CHECKBUTTON, NULL,                   NULL, N_("The default map layer added is defined by the Layer Defaults. Use the menu Edit->Layer Defaults->Map... to change the map type and other values."), NULL, NULL, NULL},
	{ 2, PREFERENCES_NAMESPACE_STARTUP "startup_method",        SGVariantType::UINT,    PARAMETER_GROUP_GENERIC, N_("Startup Method:"),          WidgetType::COMBOBOX,    params_startup_methods, NULL, NULL, NULL, NULL, NULL },
	{ 3, PREFERENCES_NAMESPACE_STARTUP "startup_file",          SGVariantType::STRING,  PARAMETER_GROUP_GENERIC, N_("Startup File:"),            WidgetType::FILEENTRY,   NULL,                   NULL, N_("The default file to load on startup. Only applies when the startup method is set to 'Specified File'"), NULL, NULL, NULL },
	{ 4, PREFERENCES_NAMESPACE_STARTUP "check_version",         SGVariantType::BOOLEAN, PARAMETER_GROUP_GENERIC, N_("Check For New Version:"),   WidgetType::CHECKBUTTON, NULL,                   NULL, N_("Periodically check to see if a new version of Viking is available"), NULL, NULL, NULL },
	{ 5, NULL,                                                  SGVariantType::BOOLEAN, PARAMETER_GROUP_GENERIC, N_("Check For New Version:"),   WidgetType::CHECKBUTTON, NULL,                   NULL, N_("Periodically check to see if a new version of Viking is available"), NULL, NULL, NULL },
};
/* End of Options static stuff. */





static Preferences preferences;




/* TODO: STRING_LIST. */
/* TODO: share code in file reading. */
/* TODO: remove hackaround in show_window. */




#define VIKING_PREFERENCES_FILE "viking.prefs"



/* How to get a value of parameter:
   1. use id to get a parameter from registered_parameters, then
   2. use parameter->name to look up parameter's value in registered_parameters_values.
   You probably can skip right to point 2 if you know full name of parameter.

   How to set a value of parameter:
   1.
   2.
*/
static std::map<param_id_t, Parameter *> registered_parameters; /* Parameter id -> Parameter. */
static std::map<std::string, ParameterValueTyped *> registered_parameter_values; /* Parameter name -> Typed parameter value. */




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
			/* if it's not in there, ignore it. */
			auto oldval = registered_parameter_values.find(key);
			if (oldval == registered_parameter_values.end()) {
				qDebug() << "II: Preferences: load from file: ignoring key/val" << key << val;
				free(key);
				free(val);
				continue;
			} else {
				qDebug() << "II: Preferences: load from file: modifying key/val" << key << val;
			}


			/* Otherwise change it (you know the type!).
			   If it's a string list do some funky stuff ... yuck... not yet. */
			if (oldval->second->type == SGVariantType::STRING_LIST) {
				fprintf(stderr, "CRITICAL: Param strings not implemented in preferences\n"); /* Fake it. */
			}

			ParameterValueTyped * newval = vik_layer_data_typed_param_copy_from_string(oldval->second->type, val);
			registered_parameter_values.at(std::string(key)) = newval;

			free(key);
			free(val);
			/* Change value. */
		}
	}
	fclose(file);
	file = NULL;

	return true;
}





void Preferences::set_param_value(param_id_t id, SGVariant value)
{
	/* Don't change stored pointer values. */
	if (registered_parameters[id]->type == SGVariantType::PTR) {
		return;
	}
	if (registered_parameters[id]->type == SGVariantType::STRING_LIST) {
		fprintf(stderr, "CRITICAL: Param strings not implemented in preferences\n"); /* Fake it. */
	}

	ParameterValueTyped * new_value = vik_layer_typed_param_data_copy_from_data(registered_parameters[id]->type, value); /* New value to save under an existing name. */
	registered_parameter_values.at(std::string(registered_parameters[id]->name)) = new_value;

	if (registered_parameters[id]->type == SGVariantType::DOUBLE) {
		qDebug() << "II: Preferences: saved parameter #" << id << registered_parameters[id]->name << new_value->data.d;
	}
}




/* Allow preferences to be manipulated externally. */
void SlavGPS::a_preferences_run_setparam(SGVariant value, Parameter * parameters)
{
	//preferences.set_param_value(0, value, parameters);
}




SGVariant Preferences::get_param_value(param_id_t id)
{
	auto val = registered_parameter_values.find(std::string(registered_parameters[id]->name));
	assert (val != registered_parameter_values.end());

	if (val->second->type == SGVariantType::STRING_LIST) {
		fprintf(stderr, "CRITICAL: Param strings not implemented in preferences\n"); /* fake it. */
	}
	return val->second->data;
}




/**
 * Returns: true on success.
 */
bool SlavGPS::a_preferences_save_to_file()
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

	for (unsigned int i = 0; i < registered_parameters.size(); i++) {
		Parameter * param = registered_parameters[i];
		auto val = registered_parameter_values.find(std::string(param->name));
		if (val != registered_parameter_values.end()) {
			if (val->second->type != SGVariantType::PTR) {
				if (val->second->type == SGVariantType::DOUBLE) {
					qDebug() << "II: Preferences: saving to file" << param->name << (double) val->second->data.d;
				}
				file_write_layer_param(file, param->name, val->second->type, val->second->data);
			}
		}
	}
	fclose(file);
	file = NULL;

	return true;
}




void SlavGPS::preferences_show_window(QWidget * parent)
{
	// preferences.loaded = true;
	// preferences_load_from_file();

	PropertiesDialog dialog(QObject::tr("Preferences"), parent);
	dialog.fill(&preferences);
	int dialog_code = dialog.exec();

	if (dialog_code == QDialog::Accepted) {
		for (auto iter = registered_parameters.begin(); iter != registered_parameters.end(); iter++) {
			SGVariant param_value = dialog.get_param_value(iter->first, iter->second);
			if (iter->second->type == SGVariantType::DOUBLE) {
				qDebug() << "II: Preferences: extracted from dialog parameter #" << iter->first << iter->second->name << param_value.d;
			}
			preferences.set_param_value(iter->first, param_value);
		}
		a_preferences_save_to_file();
	}
}




void SlavGPS::Preferences::register_parameter(Parameter * parameter, SGVariant default_value, const char * group_key)
{
	static param_id_t param_id = 0;

	/* All preferences should be registered before loading. */
	if (preferences.loaded) {
		qDebug() << "EE: Preferences: registering preference" << parameter->name << "after loading preferences from" << VIKING_PREFERENCES_FILE;
	}

	/* Copy value. */
        Parameter * new_parameter = new Parameter;
	*new_parameter = *parameter;

	ParameterValueTyped * newval = vik_layer_typed_param_data_copy_from_data(parameter->type, default_value);
	if (group_key) {
		new_parameter->group_id = preferences_group_key_to_group_id(QString(group_key));
	}

	registered_parameters.insert(std::pair<param_id_t, Parameter *>(param_id, new_parameter));
	registered_parameter_values.insert(std::pair<std::string, ParameterValueTyped *>(std::string(new_parameter->name), newval));
	param_id++;
}




#if 0
void Preferences::Preferences()
{
	/* Not copied. */
	//registered_parameters = g_ptr_array_new();

	/* Key not copied (same ptr as in pref), actual param data yes. */
	//registered_parameter_values = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, vik_layer_typed_param_data_free);
}
#endif




void Preferences::uninit()
{
	registered_parameters.clear();
	registered_parameter_values.clear();
}




SGVariant * SlavGPS::a_preferences_get(const char * key)
{
	if (!preferences.loaded) {
		fprintf(stderr, "DEBUG: %s: First time: %s\n", __FUNCTION__, key);
		/* Since we can't load the file in Preferences::init() (no params registered yet),
		   do it once before we get the first key. */
		preferences_load_from_file();
		preferences.loaded = true;
	}
	auto val = registered_parameter_values.find(std::string(key));
	if (val == registered_parameter_values.end()) {
		return NULL;
	} else {
		return &val->second->data;
	}
}




std::map<param_id_t, Parameter *>::iterator Preferences::begin()
{
	return registered_parameters.begin();
}




std::map<param_id_t, Parameter *>::iterator Preferences::end()
{
	return registered_parameters.end();
}




bool Preferences::get_restore_window_state(void)
{
	return a_preferences_get(PREFERENCES_NAMESPACE_STARTUP "restore_window_state")->b;
}




void Preferences::register_default_values()
{
	fprintf(stderr, "DEBUG: VIKING VERSION as number: %d\n", viking_version_to_number(VIKING_VERSION));

	/* Defaults for the options are setup here. */
	Preferences::register_group(PREFERENCES_GROUP_KEY_GENERAL, QObject::tr("General"));

	SGVariant tmp;
	tmp.u = (uint32_t) DegreeFormat::DMS;
	Preferences::register_parameter(&general_prefs[0], tmp, PREFERENCES_GROUP_KEY_GENERAL);

	tmp.u = (uint32_t) DistanceUnit::KILOMETRES;
	Preferences::register_parameter(&general_prefs[1], tmp, PREFERENCES_GROUP_KEY_GENERAL);

	tmp.u = (uint32_t) SpeedUnit::KILOMETRES_PER_HOUR;
	Preferences::register_parameter(&general_prefs[2], tmp, PREFERENCES_GROUP_KEY_GENERAL);

	tmp.u = (uint32_t) HeightUnit::METRES;
	Preferences::register_parameter(&general_prefs[3], tmp, PREFERENCES_GROUP_KEY_GENERAL);

	tmp.b = true;
	Preferences::register_parameter(&general_prefs[4], tmp, PREFERENCES_GROUP_KEY_GENERAL);

	/* Maintain the default location to New York. */
	tmp.d = 40.714490;
	Preferences::register_parameter(&general_prefs[5], tmp, PREFERENCES_GROUP_KEY_GENERAL);
	tmp.d = -74.007130;
	Preferences::register_parameter(&general_prefs[6], tmp, PREFERENCES_GROUP_KEY_GENERAL);

	tmp.u = VIK_TIME_REF_LOCALE;
	Preferences::register_parameter(&general_prefs[7], tmp, PREFERENCES_GROUP_KEY_GENERAL);

	/* New Tab. */
	Preferences::register_group(PREFERENCES_GROUP_KEY_STARTUP, QObject::tr("Startup"));

	tmp.b = false;
	Preferences::register_parameter(&startup_prefs[0], tmp, PREFERENCES_GROUP_KEY_STARTUP);

	tmp.b = false;
	Preferences::register_parameter(&startup_prefs[1], tmp, PREFERENCES_GROUP_KEY_STARTUP);

	tmp.u = VIK_STARTUP_METHOD_HOME_LOCATION;
	Preferences::register_parameter(&startup_prefs[2], tmp, PREFERENCES_GROUP_KEY_STARTUP);

	tmp.s = "";
	Preferences::register_parameter(&startup_prefs[3], tmp, PREFERENCES_GROUP_KEY_STARTUP);

	tmp.b = false;
	Preferences::register_parameter(&startup_prefs[4], tmp, PREFERENCES_GROUP_KEY_STARTUP);

	/* New Tab. */
	Preferences::register_group(PREFERENCES_GROUP_KEY_IO, QObject::tr("Export/External"));

	tmp.u = VIK_KML_EXPORT_UNITS_METRIC;
	Preferences::register_parameter(&io_prefs[0], tmp, PREFERENCES_GROUP_KEY_IO);

	tmp.u = VIK_GPX_EXPORT_TRK_SORT_TIME;
	Preferences::register_parameter(&io_prefs[1], tmp, PREFERENCES_GROUP_KEY_IO);

	tmp.b = VIK_GPX_EXPORT_WPT_SYM_NAME_TITLECASE;
	Preferences::register_parameter(&io_prefs[2], tmp, PREFERENCES_GROUP_KEY_IO);

#ifndef WINDOWS
	tmp.s = "xdg-open";
	Preferences::register_parameter(&io_prefs_non_windows[0], tmp, PREFERENCES_GROUP_KEY_IO);
#endif

	/* JOSM for OSM editing around a GPX track. */
	tmp.s = "josm";
	Preferences::register_parameter(&io_prefs_external_gpx[0], tmp, PREFERENCES_GROUP_KEY_IO);
	/* Add a second external program - another OSM editor by default. */
	tmp.s = "merkaartor";
	Preferences::register_parameter(&io_prefs_external_gpx[1], tmp, PREFERENCES_GROUP_KEY_IO);

	/* 'Advanced' Properties. */
	Preferences::register_group(PREFERENCES_GROUP_KEY_ADVANCED, QObject::tr("Advanced"));

	tmp.u = VIK_FILE_REF_FORMAT_ABSOLUTE;
	Preferences::register_parameter(&prefs_advanced[0], tmp, PREFERENCES_GROUP_KEY_ADVANCED);

	tmp.b = true;
	Preferences::register_parameter(&prefs_advanced[1], tmp, PREFERENCES_GROUP_KEY_ADVANCED);

	tmp.b = true;
	Preferences::register_parameter(&prefs_advanced[2], tmp, PREFERENCES_GROUP_KEY_ADVANCED);

	tmp.i = 10; /* Seemingly GTK's default for the number of recent files. */
	Preferences::register_parameter(&prefs_advanced[3], tmp, PREFERENCES_GROUP_KEY_ADVANCED);
}




DegreeFormat Preferences::get_degree_format(void)
{
	return (DegreeFormat) a_preferences_get(PREFERENCES_NAMESPACE_GENERAL "degree_format")->u;
}




DistanceUnit Preferences::get_unit_distance()
{
	return (DistanceUnit) a_preferences_get(PREFERENCES_NAMESPACE_GENERAL "units_distance")->u;
}




SpeedUnit Preferences::get_unit_speed(void)
{
	return (SpeedUnit) a_preferences_get(PREFERENCES_NAMESPACE_GENERAL "units_speed")->u;
}




HeightUnit Preferences::get_unit_height(void)
{
	return (HeightUnit) a_preferences_get(PREFERENCES_NAMESPACE_GENERAL "units_height")->u;
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
	return (vik_time_ref_frame_t) a_preferences_get(PREFERENCES_NAMESPACE_GENERAL "time_reference_frame")->u;
}




/* External/Export Options. */

vik_kml_export_units_t Preferences::get_kml_export_units()
{
	return (vik_kml_export_units_t) a_preferences_get(PREFERENCES_NAMESPACE_IO "kml_export_units")->u;
}




vik_gpx_export_trk_sort_t Preferences::get_gpx_export_trk_sort()
{
	return (vik_gpx_export_trk_sort_t) a_preferences_get(PREFERENCES_NAMESPACE_IO "gpx_export_track_sort")->u;
}




vik_gpx_export_wpt_sym_name_t Preferences::get_gpx_export_wpt_sym_name()
{
	return (vik_gpx_export_wpt_sym_name_t) a_preferences_get(PREFERENCES_NAMESPACE_IO "gpx_export_wpt_sym_names")->b;
}




#ifndef WINDOWS
char const * Preferences::get_image_viewer()
{
	return a_preferences_get(PREFERENCES_NAMESPACE_IO "image_viewer")->s;
}
#endif




char const * Preferences::get_external_gpx_program_1()
{
	return a_preferences_get(PREFERENCES_NAMESPACE_IO "external_gpx_1")->s;
}

char const * Preferences::get_external_gpx_program_2()
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




char const * Preferences::get_startup_file()
{
	return a_preferences_get(PREFERENCES_NAMESPACE_STARTUP "startup_file")->s;
}




bool Preferences::get_check_version()
{
	return a_preferences_get(PREFERENCES_NAMESPACE_STARTUP "check_version")->b;
}
