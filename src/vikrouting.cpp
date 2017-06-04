/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2012-2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

/**
 * SECTION:vikrouting
 * @short_description: the routing framework
 *
 * This module handles the list of #VikRoutingEngine.
 * It also handles the "default" functions.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdlib>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "curl_download.h"
#include "babel.h"
#include "preferences.h"
#include "vikrouting.h"
#include "vikroutingengine.h"




using namespace SlavGPS;




/* Params will be routing.default */
/* We have to make sure these don't collide. */
#define VIKING_ROUTING_PARAMS_GROUP_KEY "routing"
#define VIKING_ROUTING_PARAMS_NAMESPACE "routing."

/* List to register all routing engines. */
static GList * routing_engine_list = NULL;




static Parameter prefs[] = {
	{ LayerType::NUM_TYPES, VIKING_ROUTING_PARAMS_NAMESPACE "default", ParameterType::STRING, VIK_LAYER_GROUP_NONE, N_("Default engine:"), WidgetType::COMBOBOX, NULL, NULL, NULL, NULL, NULL, NULL },
};




char ** routing_engine_labels = NULL;
char ** routing_engine_ids = NULL;




/**
 * Initialize the preferences of the routing feature.
 */
void vik_routing_prefs_init()
{
	a_preferences_register_group(VIKING_ROUTING_PARAMS_GROUP_KEY, _("Routing"));

	ParameterValue tmp;
	tmp.s = NULL;
	a_preferences_register(prefs, tmp, VIKING_ROUTING_PARAMS_GROUP_KEY);
}




/* @see g_list_find_custom */
static int search_by_id(gconstpointer a, gconstpointer b)
{
	const char * id = (const char *) b;
	VikRoutingEngine *engine = (VikRoutingEngine *)a;
	char * engineId = vik_routing_engine_get_id(engine);
	if (id && engine) {
		return strcmp(id, engineId);
	} else {
		return -1;
	}
}




/**
 * @id: the id of the engine we are looking for.
 *
 * Returns: the found engine or %NULL.
 */
VikRoutingEngine * vik_routing_find_engine(const char * id)
{
	VikRoutingEngine * engine = NULL;
	GList * elem = g_list_find_custom(routing_engine_list, id, search_by_id);
	if (elem) {
		engine = (VikRoutingEngine *) elem->data;
	}
	return engine;
}




/**
 * Retrieve the default engine, based on user's preferences.
 *
 * Returns: the default engine.
 */
VikRoutingEngine * vik_routing_default_engine(void)
{
	const char * id = a_preferences_get(VIKING_ROUTING_PARAMS_NAMESPACE "default")->s;
	VikRoutingEngine * engine = vik_routing_find_engine(id);
	if (engine == NULL && routing_engine_list != NULL && g_list_first(routing_engine_list) != NULL) {
		/* Fallback to first element */
		engine = (VikRoutingEngine *) g_list_first(routing_engine_list)->data;
	}

	return engine;
}




/**
 * Route computation with default engine.
 *
 * Return indicates success or not.
 */
bool vik_routing_default_find(LayerTRW * trw, struct LatLon start, struct LatLon end)
{
	/* The engine. */
	VikRoutingEngine * engine = vik_routing_default_engine();
	/* The route computation. */
	return vik_routing_engine_find(engine, trw, start, end);
}




/**
 * @engine: new routing engine to register.
 *
 * Register a new routing engine.
 */
void vik_routing_register(VikRoutingEngine * engine)
{
	char * label = vik_routing_engine_get_label(engine);
	char * id = vik_routing_engine_get_id(engine);
	size_t len = 0;

	/* Check if id already exists in list. */
	GList * elem = g_list_find_custom(routing_engine_list, id, search_by_id);
	if (elem != NULL) {
		fprintf(stderr, "DEBUG: %s: %s already exists: update\n", __FUNCTION__, id);

		/* Update main list. */
		g_object_unref(elem->data);
		elem->data = g_object_ref(engine);

		/* Update GUI arrays. */
		len = g_strv_length(routing_engine_labels);
		for (; len > 0 ; len--) {
			if (strcmp(routing_engine_ids[len-1], id) == 0) {
				break;
			}
		}
		/* Update the label (possibly different). */
		free(routing_engine_labels[len-1]);
		routing_engine_labels[len-1] = g_strdup(label);

	} else {
		fprintf(stderr, "DEBUG: %s: %s is new: append\n", __FUNCTION__, id);
		routing_engine_list = g_list_append(routing_engine_list, g_object_ref(engine));

		if (routing_engine_labels) {
			len = g_strv_length(routing_engine_labels);
		}

		/* Add the label. */
		routing_engine_labels = (char **) g_realloc(routing_engine_labels, (len+2)*sizeof(char*));
		routing_engine_labels[len] = g_strdup(label);
		routing_engine_labels[len+1] = NULL;

		/* Add the id. */
		routing_engine_ids = (char **) g_realloc(routing_engine_ids, (len+2)*sizeof(char*));
		routing_engine_ids[len] = g_strdup(id);
		routing_engine_ids[len+1] = NULL;

		/* Hack
		   We have to ensure the mode Parameter references the up-to-date
		   GLists.
		*/
		/*
		  memcpy(&maps_layer_params[0].widget_data, &params_maptypes, sizeof(void *));
		  memcpy(&maps_layer_params[0].extra_widget_data, &params_maptypes_ids, sizeof(void *));
		*/
		prefs[0].widget_data = routing_engine_labels;
		prefs[0].extra_widget_data = routing_engine_ids;
	}
}




/**
 * Unregister all registered routing engines.
 */
void vik_routing_unregister_all()
{
	g_list_foreach(routing_engine_list, (GFunc) g_object_unref, NULL);
	g_strfreev(routing_engine_labels);
	g_strfreev(routing_engine_ids);
}




/**
 * @func: the function to run on each element.
 * @user_data: user's data to give to each call of @func.
 *
 * Loop over all registered routing engines.
 */
void vik_routing_foreach_engine(GFunc func, void * user_data)
{
	g_list_foreach(routing_engine_list, func, user_data);
}




/*
 * This function is called for all routing engine registered.
 * Following result of the predicate function, the current engine
 * is added to the combobox. In order to retrieve the VikRoutingEngine
 * object, we store a list of added engine in a GObject's data "engines".
 *
 * @see g_list_foreach()
 */
static void fill_engine_box(void * data, void * user_data)
{
	VikRoutingEngine * engine = (VikRoutingEngine *) data;
	/* Retrieve combo. */
	GtkWidget * widget = (GtkWidget *) user_data;

	/* Only register engine fulfilling expected behavior. */
	Predicate predicate = (Predicate) g_object_get_data(G_OBJECT (widget), "func");
	void * predicate_data = g_object_get_data(G_OBJECT (widget), "user_data");
	/* No predicate means to register all engines. */
	bool ok = predicate == NULL || predicate(engine, predicate_data);

	if (ok) {
		/* Add item in widget. */
		const char *label = vik_routing_engine_get_label(engine);
		vik_combo_box_text_append(widget, label);
		/* Save engine in internal list. */
		GList *engines = (GList*) g_object_get_data(G_OBJECT (widget) , "engines");
		engines = g_list_append(engines, engine);
		g_object_set_data(G_OBJECT (widget), "engines", engines);
	}
}




/**
 * @func: user function to decide if an engine has to be added or not.
 * @user_data: user data for previous function.
 *
 * Creates a combo box to allow selection of a routing engine.
 *
 * We use GObject data hastable to store and retrieve the VikRoutingEngine
 * associated to the selection.
 *
 * Returns: the combo box
 */
GtkWidget * vik_routing_ui_selector_new(Predicate func, void * user_data)
{
	/* Create the combo */
	GtkWidget * combo = vik_combo_box_text_new();

	/* Save data for foreach function. */
	g_object_set_data(G_OBJECT (combo), "func", (void *) func);
	g_object_set_data(G_OBJECT (combo), "user_data", user_data);

	/* Filter all engines with given user function. */
	vik_routing_foreach_engine(fill_engine_box, combo);

	return combo;
}




/**
 * @combo: the GtkWidget combobox
 * @pos: the selected position
 *
 * Retrieve the VikRoutingEngine stored in a list attached to @combo
 * via the "engines" property.
 *
 * Returns: the VikRoutingEngine object associated to @pos.
 */
VikRoutingEngine * vik_routing_ui_selector_get_nth(GtkWidget * combo, int pos)
{
	/* Retrieve engine. */
	GList *engines = (GList*) g_object_get_data (G_OBJECT (combo) , "engines");
	VikRoutingEngine *engine = (VikRoutingEngine *) g_list_nth_data(engines, pos);

	return engine;
}
