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




static Preferences preferences;




/* TODO: STRING_LIST. */
/* TODO: share code in file reading. */
/* TODO: remove hackaround in show_window. */




#define VIKING_PREFS_FILE "viking.prefs"



/* When doing lookup:
   1. use id to get a parameter from registered_parameters.
   2. use parameter->name to look up parameter's value in registered_parameters_values.

   When doing registration:
   1.
   2.
*/
static std::map<param_id_t, LayerParam *> registered_parameters; /* Parameter id -> Parameter. */
static std::map<std::string, LayerTypedParamData *> registered_parameter_values; /* Parameter name -> Typed parameter value. */
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
		LayerTypedParamData * newval;
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
				if (oldval->second->type == LayerParamType::STRING_LIST) {
					fprintf(stderr, "CRITICAL: Param strings not implemented in preferences\n"); /* Fake it. */
				}

				newval = vik_layer_data_typed_param_copy_from_string(oldval->second->type, val);
				registered_parameter_values.insert(std::pair<std::string, LayerTypedParamData *>(key, newval));

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




void Preferences::set_param_value(param_id_t id, LayerParamValue value, LayerParam * parameters)
{
	/* Don't change stored pointer values. */
	if (parameters[id].type == LayerParamType::PTR) {
		return;
	}
	if (parameters[id].type == LayerParamType::STRING_LIST) {
		fprintf(stderr, "CRITICAL: Param strings not implemented in preferences\n"); /* Fake it. */
	}
	registered_parameter_values.insert(std::pair<std::string, LayerTypedParamData *>(std::string((char *)(parameters[id].name)),
											 vik_layer_typed_param_data_copy_from_data(parameters[id].type, value)));
}




/* Allow preferences to be manipulated externally. */
void a_preferences_run_setparam(LayerParamValue value, LayerParam * parameters)
{
	preferences.set_param_value(0, value, parameters);
}




LayerParamValue Preferences::get_param_value(param_id_t id)
{
	auto val = registered_parameter_values.find(std::string(registered_parameters[id]->name));
	assert (val != registered_parameter_values.end());

	if (val->second->type == LayerParamType::STRING_LIST) {
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
			LayerParam * param = registered_parameters[i];
			auto val = registered_parameter_values.find(std::string(param->name));
			if (val != registered_parameter_values.end()) {
				if (val->second->type != LayerParamType::PTR) {
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




void a_preferences_show_window(QWindow * parent)
{
	//LayerParamValue *a_uibuilder_run_dialog (GtkWindow *parent, VikLayerParam \* registered_parameters, // uint16_t params_count, char **groups, uint8_t groups_count, // LayerParamValue *params_defaults)
	/* TODO: THIS IS A MAJOR HACKAROUND, but ok when we have only a couple preferences. */
#if 0
	int params_count = registered_parameters.size();
	LayerParam * contiguous_params = (LayerParam *) malloc(params_count * sizeof (LayerParam));
	for (unsigned int i = 0; i < registered_parameters.size(); i++) {
		contiguous_params[i] = *(registered_parameters[i]);
	}
	contiguous_params[params_count - 1].title = NULL;

	LayerPropertiesDialog dialog((QWidget *) parent);
	dialog.fill(&preferences, contiguous_params, params_count);
	dialog.exec();
#else

	LayerPropertiesDialog dialog((QWidget *) parent);
	dialog.fill(&preferences);
	dialog.exec();
#endif



#if 0
	loaded = true;
	preferences_load_from_file();
	if (a_uibuilder_properties_factory(_("Preferences"),
					   parent,
					   contiguous_params,
					   params_count,
					   (char **) groups_names->pdata,
					   groups_names->len, // groups, groups_count, // groups? what groups?!
					   (bool (*) (void *, uint16_t, LayerParamValue,void *, bool)) preferences_run_setparam,
					   NULL /* Not used. */,
					   contiguous_params,
					   preferences_run_getparam,
					   NULL,
					   NULL /* Not used, */)) {
		a_preferences_save_to_file();
	}
	free(contiguous_params);
#endif
}




void a_preferences_register(LayerParam * parameter, LayerParamValue default_value, const char * group_key)
{
	static param_id_t id = 0;
	/* All preferences should be registered before loading. */
	if (loaded) {
		fprintf(stderr, "CRITICAL: REGISTERING preference %s after LOADING from \n" VIKING_PREFS_FILE, parameter->name);
	}
	/* Copy value. */
	LayerParam * new_parameter = (LayerParam *) malloc(1 * sizeof (LayerParam));
	*new_parameter = *parameter;
	LayerTypedParamData * newval = vik_layer_typed_param_data_copy_from_data(parameter->type, default_value);
	if (group_key) {
		new_parameter->group = preferences_groups_key_to_index(group_key);
	}

	registered_parameters.insert(std::pair<param_id_t, LayerParam *>(id, new_parameter));
	registered_parameter_values.insert(std::pair<std::string, LayerTypedParamData *>(std::string(new_parameter->name), newval));
	id++;
}




void a_preferences_init()
{
	preferences_groups_init();

	/* Not copied. */
	//registered_parameters = g_ptr_array_new();

	/* Key not copied (same ptr as in pref), actual param data yes. */
	//registered_parameter_values = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, vik_layer_typed_param_data_free);

	loaded = false;
}




void a_preferences_uninit()
{
	preferences_groups_uninit();

	registered_parameters.clear();
	registered_parameter_values.clear();
}




LayerParamValue * a_preferences_get(const char * key)
{
	if (!loaded) {
		fprintf(stderr, "DEBUG: %s: First time: %s\n", __FUNCTION__, key);
		/* Since we can't load the file in a_preferences_init (no params registered yet),
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




std::map<param_id_t, LayerParam *>::iterator Preferences::begin()
{
	return registered_parameters.begin();
}




std::map<param_id_t, LayerParam *>::iterator Preferences::end()
{
	return registered_parameters.end();
}
