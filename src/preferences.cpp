/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
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

#include <string>
#include <map>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cassert>

#include <QDebug>

#if 0
#include <gtk/gtk.h>
#endif
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "preferences.h"
#include "dir.h"
#include "file.h"
#include "util.h"
#include "globals.h"
#include "uibuilder_qt.h"




using namespace SlavGPS;



extern Parameter io_prefs[];
extern Parameter io_prefs_non_windows[];
extern Parameter io_prefs_external_gpx[];
extern Parameter startup_prefs[];
extern Parameter prefs_advanced[];
extern Parameter general_prefs[];




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
				if (oldval->second->type == ParameterType::STRING_LIST) {
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




void Preferences::set_param_value(param_id_t id, ParameterValue value)
{
	/* Don't change stored pointer values. */
	if (registered_parameters[id]->type == ParameterType::PTR) {
		return;
	}
	if (registered_parameters[id]->type == ParameterType::STRING_LIST) {
		fprintf(stderr, "CRITICAL: Param strings not implemented in preferences\n"); /* Fake it. */
	}

	ParameterValueTyped * new_value = vik_layer_typed_param_data_copy_from_data(registered_parameters[id]->type, value); /* New value to save under an existing name. */
	registered_parameter_values.at(std::string(registered_parameters[id]->name)) = new_value;

	if (registered_parameters[id]->type == ParameterType::DOUBLE) {
		qDebug() << "II: Preferences: saved parameter #" << id << registered_parameters[id]->name << new_value->data.d;
	}
}




/* Allow preferences to be manipulated externally. */
void a_preferences_run_setparam(ParameterValue value, Parameter * parameters)
{
	//preferences.set_param_value(0, value, parameters);
}




ParameterValue Preferences::get_param_value(param_id_t id)
{
	auto val = registered_parameter_values.find(std::string(registered_parameters[id]->name));
	assert (val != registered_parameter_values.end());

	if (val->second->type == ParameterType::STRING_LIST) {
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
				if (val->second->type != ParameterType::PTR) {
					if (val->second->type == ParameterType::DOUBLE) {
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
			ParameterValue param_value = dialog.get_param_value(iter->first, iter->second);
			if (iter->second->type == ParameterType::DOUBLE) {
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
					   (bool (*) (void *, uint16_t, ParameterValue,void *, bool)) preferences_run_setparam,
					   NULL /* Not used. */,
					   contiguous_params,
					   preferences_run_getparam,
					   NULL,
					   NULL /* Not used, */)) {

	}
	free(contiguous_params);
#endif
}




void a_preferences_register(Parameter * parameter, ParameterValue default_value, const char * group_key)
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




ParameterValue * a_preferences_get(const char * key)
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
	fprintf(stderr, "DEBUG: VIKING VERSION as number: %d\n", viking_version_to_number((char *) VIKING_VERSION));

	/* Defaults for the options are setup here. */
	a_preferences_register_group(VIKING_PREFERENCES_GROUP_KEY, _("General"));

	ParameterValue tmp;
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
