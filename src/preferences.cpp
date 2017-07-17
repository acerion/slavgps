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

#include <QDebug>

#include <glib/gstdio.h>

#include "preferences.h"
#include "dir.h"
#include "file.h"
#include "util.h"
#include "globals.h"
#include "uibuilder_qt.h"
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
	{ 0, VIKING_PREFERENCES_NAMESPACE "degree_format",            SGVariantType::UINT,    VIK_LAYER_GROUP_NONE, N_("Degree format:"),            WidgetType::COMBOBOX,        params_degree_formats, NULL, NULL, NULL, NULL, NULL },
	{ 1, VIKING_PREFERENCES_NAMESPACE "units_distance",           SGVariantType::UINT,    VIK_LAYER_GROUP_NONE, N_("Distance units:"),           WidgetType::COMBOBOX,        params_units_distance, NULL, NULL, NULL, NULL, NULL },
	{ 2, VIKING_PREFERENCES_NAMESPACE "units_speed",              SGVariantType::UINT,    VIK_LAYER_GROUP_NONE, N_("Speed units:"),              WidgetType::COMBOBOX,        params_units_speed,    NULL, NULL, NULL, NULL, NULL },
	{ 3, VIKING_PREFERENCES_NAMESPACE "units_height",             SGVariantType::UINT,    VIK_LAYER_GROUP_NONE, N_("Height units:"),             WidgetType::COMBOBOX,        params_units_height,   NULL, NULL, NULL, NULL, NULL },
	{ 4, VIKING_PREFERENCES_NAMESPACE "use_large_waypoint_icons", SGVariantType::BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Use large waypoint icons:"), WidgetType::CHECKBUTTON,     NULL,                  NULL, NULL, NULL, NULL, NULL },
	{ 5, VIKING_PREFERENCES_NAMESPACE "default_latitude",         SGVariantType::DOUBLE,  VIK_LAYER_GROUP_NONE, N_("Default latitude:"),         WidgetType::SPINBOX_DOUBLE,  params_scales_lat,     NULL, NULL, NULL, NULL, NULL },
	{ 6, VIKING_PREFERENCES_NAMESPACE "default_longitude",        SGVariantType::DOUBLE,  VIK_LAYER_GROUP_NONE, N_("Default longitude:"),        WidgetType::SPINBOX_DOUBLE,  params_scales_long,    NULL, NULL, NULL, NULL, NULL },
	{ 7, VIKING_PREFERENCES_NAMESPACE "time_reference_frame",     SGVariantType::UINT,    VIK_LAYER_GROUP_NONE, N_("Time Display:"),             WidgetType::COMBOBOX,        params_time_ref_frame, NULL, N_("Display times according to the reference frame. Locale is the user's system setting. World is relative to the location of the object."), NULL, NULL, NULL },
	{ 8, NULL,                                                    SGVariantType::UINT,    VIK_LAYER_GROUP_NONE, N_("Time Display:"),             WidgetType::COMBOBOX,        params_time_ref_frame, NULL, N_("Display times according to the reference frame. Locale is the user's system setting. World is relative to the location of the object."), NULL, NULL, NULL },
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
	{ 0, VIKING_PREFERENCES_IO_NAMESPACE "kml_export_units",         SGVariantType::UINT, VIK_LAYER_GROUP_NONE, N_("KML File Export Units:"), WidgetType::COMBOBOX, params_kml_export_units,       NULL, NULL, NULL, NULL, NULL },
	{ 1, VIKING_PREFERENCES_IO_NAMESPACE "gpx_export_track_sort",    SGVariantType::UINT, VIK_LAYER_GROUP_NONE, N_("GPX Track Order:"),       WidgetType::COMBOBOX, params_gpx_export_trk_sort,    NULL, NULL, NULL, NULL, NULL },
	{ 2, VIKING_PREFERENCES_IO_NAMESPACE "gpx_export_wpt_sym_names", SGVariantType::UINT, VIK_LAYER_GROUP_NONE, N_("GPX Waypoint Symbols:"),  WidgetType::COMBOBOX, params_gpx_export_wpt_symbols, NULL, N_("Save GPX Waypoint Symbol names in the specified case. May be useful for compatibility with various devices"), NULL, NULL, NULL },
	{ 3, NULL,                                                       SGVariantType::UINT, VIK_LAYER_GROUP_NONE, N_("GPX Waypoint Symbols:"),  WidgetType::COMBOBOX, params_gpx_export_wpt_symbols, NULL, N_("Save GPX Waypoint Symbol names in the specified case. May be useful for compatibility with various devices"), NULL, NULL, NULL },
};

#ifndef WINDOWS
static Parameter io_prefs_non_windows[] = {
	{ 0, VIKING_PREFERENCES_IO_NAMESPACE "image_viewer", SGVariantType::STRING, VIK_LAYER_GROUP_NONE, N_("Image Viewer:"), WidgetType::FILEENTRY, NULL, NULL, NULL, NULL, NULL, NULL },
	{ 1, NULL,                                           SGVariantType::STRING, VIK_LAYER_GROUP_NONE, N_("Image Viewer:"), WidgetType::FILEENTRY, NULL, NULL, NULL, NULL, NULL, NULL },
};
#endif

static Parameter io_prefs_external_gpx[] = {
	{ 0, VIKING_PREFERENCES_IO_NAMESPACE "external_gpx_1", SGVariantType::STRING, VIK_LAYER_GROUP_NONE, N_("External GPX Program 1:"), WidgetType::FILEENTRY, NULL, NULL, NULL, NULL, NULL, NULL },
	{ 1, VIKING_PREFERENCES_IO_NAMESPACE "external_gpx_2", SGVariantType::STRING, VIK_LAYER_GROUP_NONE, N_("External GPX Program 2:"), WidgetType::FILEENTRY, NULL, NULL, NULL, NULL, NULL, NULL },
	{ 2, NULL,                                             SGVariantType::STRING, VIK_LAYER_GROUP_NONE, N_("External GPX Program 2:"), WidgetType::FILEENTRY, NULL, NULL, NULL, NULL, NULL, NULL },
};

static label_id_t params_vik_fileref[] = {
	{ "Absolute", 0 },
	{ "Relative", 1 },
	{ NULL,       2 } };

static ParameterScale params_recent_files[] = { {-1, 25, 1, 0} };

static Parameter prefs_advanced[] = {
	{ 0, VIKING_PREFERENCES_ADVANCED_NAMESPACE "save_file_reference_mode",  SGVariantType::UINT,    VIK_LAYER_GROUP_NONE, N_("Save File Reference Mode:"),           WidgetType::COMBOBOX,    params_vik_fileref,  NULL, N_("When saving a Viking .vik file, this determines how the directory paths of filenames are written."), NULL, NULL, NULL },
	{ 1, VIKING_PREFERENCES_ADVANCED_NAMESPACE "ask_for_create_track_name", SGVariantType::BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Ask for Name before Track Creation:"), WidgetType::CHECKBUTTON, NULL,                NULL, NULL, NULL, NULL, NULL },
	{ 2, VIKING_PREFERENCES_ADVANCED_NAMESPACE "create_track_tooltip",      SGVariantType::BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Show Tooltip during Track Creation:"), WidgetType::CHECKBUTTON, NULL,                NULL, NULL, NULL, NULL, NULL },
	{ 3, VIKING_PREFERENCES_ADVANCED_NAMESPACE "number_recent_files",       SGVariantType::INT,     VIK_LAYER_GROUP_NONE, N_("The number of recent files:"),         WidgetType::SPINBUTTON,  params_recent_files, NULL, N_("Only applies to new windows or on application restart. -1 means all available files."), NULL, NULL, NULL },
	{ 4, NULL,                                                              SGVariantType::INT,     VIK_LAYER_GROUP_NONE, N_("The number of recent files:"),         WidgetType::SPINBUTTON,  params_recent_files, NULL, N_("Only applies to new windows or on application restart. -1 means all available files."), NULL, NULL, NULL },
};

static label_id_t params_startup_methods[] = {
	{ "Home Location",  0 },
	{ "Last Location",  1 },
	{ "Specified File", 2 },
	{ "Auto Location",  3 },
	{ NULL,             4 } };

static Parameter startup_prefs[] = {
	{ 0, VIKING_PREFERENCES_STARTUP_NAMESPACE "restore_window_state",  SGVariantType::BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Restore Window Setup:"),    WidgetType::CHECKBUTTON, NULL,                   NULL, N_("Restore window size and layout"), NULL, NULL, NULL},
	{ 1, VIKING_PREFERENCES_STARTUP_NAMESPACE "add_default_map_layer", SGVariantType::BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Add a Default Map Layer:"), WidgetType::CHECKBUTTON, NULL,                   NULL, N_("The default map layer added is defined by the Layer Defaults. Use the menu Edit->Layer Defaults->Map... to change the map type and other values."), NULL, NULL, NULL},
	{ 2, VIKING_PREFERENCES_STARTUP_NAMESPACE "startup_method",        SGVariantType::UINT,    VIK_LAYER_GROUP_NONE, N_("Startup Method:"),          WidgetType::COMBOBOX,    params_startup_methods, NULL, NULL, NULL, NULL, NULL },
	{ 3, VIKING_PREFERENCES_STARTUP_NAMESPACE "startup_file",          SGVariantType::STRING,  VIK_LAYER_GROUP_NONE, N_("Startup File:"),            WidgetType::FILEENTRY,   NULL,                   NULL, N_("The default file to load on startup. Only applies when the startup method is set to 'Specified File'"), NULL, NULL, NULL },
	{ 4, VIKING_PREFERENCES_STARTUP_NAMESPACE "check_version",         SGVariantType::BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Check For New Version:"),   WidgetType::CHECKBUTTON, NULL,                   NULL, N_("Periodically check to see if a new version of Viking is available"), NULL, NULL, NULL },
	{ 5, NULL,                                                         SGVariantType::BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Check For New Version:"),   WidgetType::CHECKBUTTON, NULL,                   NULL, N_("Periodically check to see if a new version of Viking is available"), NULL, NULL, NULL },
};
/* End of Options static stuff. */





static Preferences preferences;




/* TODO: STRING_LIST. */
/* TODO: share code in file reading. */
/* TODO: remove hackaround in show_window. */




#define VIKING_PREFS_FILE "viking.prefs"



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
bool loaded;




/************ groups *********/




static GPtrArray *groups_names;
static GHashTable *groups_keys_to_indices; /* contains int, NULL (0) is not found, instead 1 is used for 0, 2 for 1, etc. */




static void preferences_groups_init()
{
	groups_names = g_ptr_array_new();
	groups_keys_to_indices = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
}




static void preferences_groups_uninit()
{
	g_ptr_array_foreach(groups_names, (GFunc)g_free, NULL);
	g_ptr_array_free(groups_names, true);
	g_hash_table_destroy(groups_keys_to_indices);
}




void a_preferences_register_group(const char * key, const char *name)
{
	if (g_hash_table_lookup(groups_keys_to_indices, key)) {
		fprintf(stderr, "CRITICAL: Duplicate preferences group keys\n");
	} else {
		g_ptr_array_add(groups_names, g_strdup(name));
		g_hash_table_insert(groups_keys_to_indices, g_strdup(key), KINT_TO_POINTER ((int) groups_names->len)); /* index + 1 */
	}
}




/* Returns -1 if not found. */
static int16_t preferences_groups_key_to_index(const char * key)
{
	int index = KPOINTER_TO_INT (g_hash_table_lookup (groups_keys_to_indices, key));
	if (!index) {
		return VIK_LAYER_GROUP_NONE; /* Which should be -1 anyway. */
	}
	return (int16_t) (index - 1);
}




/*****************************/




static bool preferences_load_from_file()
{
	char * fn = g_build_filename(get_viking_dir(), VIKING_PREFS_FILE, NULL);
	FILE * f = fopen(fn, "r");
	free(fn);

	if (f) {
		char buf[4096];
		char * key = NULL;
		char * val = NULL;
		while (!feof (f)) {
			if (fgets(buf,sizeof(buf), f) == NULL) {
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
		fclose(f);
		f = NULL;
		return true;
	}
	return false;
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
void a_preferences_run_setparam(SGVariant value, Parameter * parameters)
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
bool a_preferences_save_to_file()
{
	char * fn = g_build_filename(get_viking_dir(), VIKING_PREFS_FILE, NULL);

	FILE * f = fopen(fn, "w");
	/* Since preferences files saves OSM login credentials, it'll be better to store it in secret. */
	if (chmod(fn, 0600) != 0) {
		fprintf(stderr, "WARNING: %s: Failed to set permissions on %s\n", __FUNCTION__, fn);
	}
	free(fn);

	if (f) {
		for (unsigned int i = 0; i < registered_parameters.size(); i++) {
			Parameter * param = registered_parameters[i];
			auto val = registered_parameter_values.find(std::string(param->name));
			if (val != registered_parameter_values.end()) {
				if (val->second->type != SGVariantType::PTR) {
					if (val->second->type == SGVariantType::DOUBLE) {
						qDebug() << "II: Preferences: saving to file" << param->name << (double) val->second->data.d;
					}
					file_write_layer_param(f, param->name, val->second->type, val->second->data);
				}
			}
		}
		fclose(f);
		f = NULL;
		return true;
	}

	return false;
}




void preferences_show_window(QWidget * parent)
{
	//loaded = true;
	//preferences_load_from_file();

	PropertiesDialog dialog(QString("Preferences"), parent);
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


#if 0
	if (a_uibuilder_properties_factory(_("Preferences"),
					   parent,
					   contiguous_params,
					   params_count,
					   (char **) groups_names->pdata,
					   groups_names->len, // groups, groups_count, // groups? what groups?!
					   (bool (*) (void *, uint16_t, SGVariant,void *, bool)) preferences_run_setparam,
					   NULL /* Not used. */,
					   contiguous_params,
					   preferences_run_getparam,
					   NULL,
					   NULL /* Not used, */)) {

	}
	free(contiguous_params);
#endif
}




void a_preferences_register(Parameter * parameter, SGVariant default_value, const char * group_key)
{
	static param_id_t id = 0;
	/* All preferences should be registered before loading. */
	if (loaded) {
		fprintf(stderr, "CRITICAL: REGISTERING preference %s after LOADING from \n" VIKING_PREFS_FILE, parameter->name);
	}
	/* Copy value. */
        Parameter * new_parameter = (Parameter *) malloc(1 * sizeof (Parameter));
	*new_parameter = *parameter;
	ParameterValueTyped * newval = vik_layer_typed_param_data_copy_from_data(parameter->type, default_value);
	if (group_key) {
		new_parameter->group = preferences_groups_key_to_index(group_key);
	}

	registered_parameters.insert(std::pair<param_id_t, Parameter *>(id, new_parameter));
	registered_parameter_values.insert(std::pair<std::string, ParameterValueTyped *>(std::string(new_parameter->name), newval));
	id++;
}




void Preferences::init()
{
	preferences_groups_init();

	/* Not copied. */
	//registered_parameters = g_ptr_array_new();

	/* Key not copied (same ptr as in pref), actual param data yes. */
	//registered_parameter_values = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, vik_layer_typed_param_data_free);

	loaded = false;
}




void Preferences::uninit()
{
	preferences_groups_uninit();

	registered_parameters.clear();
	registered_parameter_values.clear();
}




SGVariant * a_preferences_get(const char * key)
{
	if (!loaded) {
		fprintf(stderr, "DEBUG: %s: First time: %s\n", __FUNCTION__, key);
		/* Since we can't load the file in Preferences::init() (no params registered yet),
		   do it once before we get the first key. */
		preferences_load_from_file();
		loaded = true;
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
	return a_preferences_get(VIKING_PREFERENCES_STARTUP_NAMESPACE "restore_window_state")->b;
}




void Preferences::register_default_values()
{
	fprintf(stderr, "DEBUG: VIKING VERSION as number: %d\n", viking_version_to_number(VIKING_VERSION));

	/* Defaults for the options are setup here. */
	a_preferences_register_group(VIKING_PREFERENCES_GROUP_KEY, _("General"));

	SGVariant tmp;
	tmp.u = (uint32_t) DegreeFormat::DMS;
	a_preferences_register(&general_prefs[0], tmp, VIKING_PREFERENCES_GROUP_KEY);

	tmp.u = (uint32_t) DistanceUnit::KILOMETRES;
	a_preferences_register(&general_prefs[1], tmp, VIKING_PREFERENCES_GROUP_KEY);

	tmp.u = (uint32_t) SpeedUnit::KILOMETRES_PER_HOUR;
	a_preferences_register(&general_prefs[2], tmp, VIKING_PREFERENCES_GROUP_KEY);

	tmp.u = (uint32_t) HeightUnit::METRES;
	a_preferences_register(&general_prefs[3], tmp, VIKING_PREFERENCES_GROUP_KEY);

	tmp.b = true;
	a_preferences_register(&general_prefs[4], tmp, VIKING_PREFERENCES_GROUP_KEY);

	/* Maintain the default location to New York. */
	tmp.d = 40.714490;
	a_preferences_register(&general_prefs[5], tmp, VIKING_PREFERENCES_GROUP_KEY);
	tmp.d = -74.007130;
	a_preferences_register(&general_prefs[6], tmp, VIKING_PREFERENCES_GROUP_KEY);

	tmp.u = VIK_TIME_REF_LOCALE;
	a_preferences_register(&general_prefs[7], tmp, VIKING_PREFERENCES_GROUP_KEY);

	/* New Tab. */
	a_preferences_register_group(VIKING_PREFERENCES_STARTUP_GROUP_KEY, _("Startup"));

	tmp.b = false;
	a_preferences_register(&startup_prefs[0], tmp, VIKING_PREFERENCES_STARTUP_GROUP_KEY);

	tmp.b = false;
	a_preferences_register(&startup_prefs[1], tmp, VIKING_PREFERENCES_STARTUP_GROUP_KEY);

	tmp.u = VIK_STARTUP_METHOD_HOME_LOCATION;
	a_preferences_register(&startup_prefs[2], tmp, VIKING_PREFERENCES_STARTUP_GROUP_KEY);

	tmp.s = "";
	a_preferences_register(&startup_prefs[3], tmp, VIKING_PREFERENCES_STARTUP_GROUP_KEY);

	tmp.b = false;
	a_preferences_register(&startup_prefs[4], tmp, VIKING_PREFERENCES_STARTUP_GROUP_KEY);

	/* New Tab. */
	a_preferences_register_group(VIKING_PREFERENCES_IO_GROUP_KEY, _("Export/External"));

	tmp.u = VIK_KML_EXPORT_UNITS_METRIC;
	a_preferences_register(&io_prefs[0], tmp, VIKING_PREFERENCES_IO_GROUP_KEY);

	tmp.u = VIK_GPX_EXPORT_TRK_SORT_TIME;
	a_preferences_register(&io_prefs[1], tmp, VIKING_PREFERENCES_IO_GROUP_KEY);

	tmp.b = VIK_GPX_EXPORT_WPT_SYM_NAME_TITLECASE;
	a_preferences_register(&io_prefs[2], tmp, VIKING_PREFERENCES_IO_GROUP_KEY);

#ifndef WINDOWS
	tmp.s = "xdg-open";
	a_preferences_register(&io_prefs_non_windows[0], tmp, VIKING_PREFERENCES_IO_GROUP_KEY);
#endif

	/* JOSM for OSM editing around a GPX track. */
	tmp.s = "josm";
	a_preferences_register(&io_prefs_external_gpx[0], tmp, VIKING_PREFERENCES_IO_GROUP_KEY);
	/* Add a second external program - another OSM editor by default. */
	tmp.s = "merkaartor";
	a_preferences_register(&io_prefs_external_gpx[1], tmp, VIKING_PREFERENCES_IO_GROUP_KEY);

	/* 'Advanced' Properties. */
	a_preferences_register_group(VIKING_PREFERENCES_ADVANCED_GROUP_KEY, _("Advanced"));

	tmp.u = VIK_FILE_REF_FORMAT_ABSOLUTE;
	a_preferences_register(&prefs_advanced[0], tmp, VIKING_PREFERENCES_ADVANCED_GROUP_KEY);

	tmp.b = true;
	a_preferences_register(&prefs_advanced[1], tmp, VIKING_PREFERENCES_ADVANCED_GROUP_KEY);

	tmp.b = true;
	a_preferences_register(&prefs_advanced[2], tmp, VIKING_PREFERENCES_ADVANCED_GROUP_KEY);

	tmp.i = 10; /* Seemingly GTK's default for the number of recent files. */
	a_preferences_register(&prefs_advanced[3], tmp, VIKING_PREFERENCES_ADVANCED_GROUP_KEY);
}




DegreeFormat Preferences::get_degree_format(void)
{
	return (DegreeFormat) a_preferences_get(VIKING_PREFERENCES_NAMESPACE "degree_format")->u;
}




DistanceUnit Preferences::get_unit_distance()
{
	return (DistanceUnit) a_preferences_get(VIKING_PREFERENCES_NAMESPACE "units_distance")->u;
}




SpeedUnit Preferences::get_unit_speed(void)
{
	return (SpeedUnit) a_preferences_get(VIKING_PREFERENCES_NAMESPACE "units_speed")->u;
}




HeightUnit Preferences::get_unit_height(void)
{
	return (HeightUnit) a_preferences_get(VIKING_PREFERENCES_NAMESPACE "units_height")->u;
}


//

bool Preferences::get_use_large_waypoint_icons()
{
	return a_preferences_get(VIKING_PREFERENCES_NAMESPACE "use_large_waypoint_icons")->b;
}




double Preferences::get_default_lat()
{
	double data = a_preferences_get(VIKING_PREFERENCES_NAMESPACE "default_latitude")->d;
	//return data;
	return 55.0;
}




double Preferences::get_default_lon()
{
	double data = a_preferences_get(VIKING_PREFERENCES_NAMESPACE "default_longitude")->d;
	//return data;
	return 16.0;
}




vik_time_ref_frame_t Preferences::get_time_ref_frame()
{
	return (vik_time_ref_frame_t) a_preferences_get(VIKING_PREFERENCES_NAMESPACE "time_reference_frame")->u;
}




/* External/Export Options. */

vik_kml_export_units_t Preferences::get_kml_export_units()
{
	return (vik_kml_export_units_t) a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "kml_export_units")->u;
}




vik_gpx_export_trk_sort_t Preferences::get_gpx_export_trk_sort()
{
	return (vik_gpx_export_trk_sort_t) a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "gpx_export_track_sort")->u;
}




vik_gpx_export_wpt_sym_name_t Preferences::get_gpx_export_wpt_sym_name()
{
	return (vik_gpx_export_wpt_sym_name_t) a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "gpx_export_wpt_sym_names")->b;
}




#ifndef WINDOWS
char const * Preferences::get_image_viewer()
{
	return a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "image_viewer")->s;
}
#endif




char const * Preferences::get_external_gpx_program_1()
{
	return a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "external_gpx_1")->s;
}

char const * Preferences::get_external_gpx_program_2()
{
	return a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "external_gpx_2")->s;
}




/* Advanced Options */

vik_file_ref_format_t Preferences::get_file_ref_format()
{
	return (vik_file_ref_format_t) a_preferences_get(VIKING_PREFERENCES_ADVANCED_NAMESPACE "save_file_reference_mode")->u;
}




bool Preferences::get_ask_for_create_track_name()
{
	return a_preferences_get(VIKING_PREFERENCES_ADVANCED_NAMESPACE "ask_for_create_track_name")->b;
}




bool Preferences::get_create_track_tooltip()
{
	return a_preferences_get(VIKING_PREFERENCES_ADVANCED_NAMESPACE "create_track_tooltip")->b;
}




int Preferences::get_recent_number_files()
{
	return a_preferences_get(VIKING_PREFERENCES_ADVANCED_NAMESPACE "number_recent_files")->i;
}




bool Preferences::get_add_default_map_layer()
{
	return a_preferences_get(VIKING_PREFERENCES_STARTUP_NAMESPACE "add_default_map_layer")->b;
}




vik_startup_method_t Preferences::get_startup_method()
{
	return (vik_startup_method_t) a_preferences_get(VIKING_PREFERENCES_STARTUP_NAMESPACE "startup_method")->u;
}




char const * Preferences::get_startup_file()
{
	return a_preferences_get(VIKING_PREFERENCES_STARTUP_NAMESPACE "startup_file")->s;
}




bool Preferences::get_check_version()
{
	return a_preferences_get(VIKING_PREFERENCES_STARTUP_NAMESPACE "check_version")->b;
}
