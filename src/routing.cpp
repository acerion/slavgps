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
 * This module handles the list of #RoutingEngine.
 * It also handles the "default" functions.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdlib>
#include <list>

#include <QDebug>

#include <glib.h>
#include <glib/gstdio.h>

#include "curl_download.h"
#include "babel.h"
#include "preferences.h"
#include "routing.h"
#include "routing_engine.h"
#include "util.h"
#include "ui_builder.h"




using namespace SlavGPS;




/* Params will be routing.default */
/* We have to make sure these don't collide. */
#define PREFERENCES_GROUP_KEY_ROUTING "routing"
#define PREFERENCES_NAMESPACE_ROUTING "routing."




static std::vector<RoutingEngine *> routing_engines;   /* List to register all routing engines. */
static std::vector<SGLabelID> routing_engine_combo_items;
static std::vector<QString> routing_engine_ids; /* These are string IDs. */



static Parameter prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_ROUTING "default", SGVariantType::STRING, PARAMETER_GROUP_GENERIC, N_("Default engine:"), WidgetType::COMBOBOX, &routing_engine_combo_items, NULL, NULL, NULL },
};




/**
 * Initialize the preferences of the routing feature.
 */
void SlavGPS::routing_prefs_init()
{
	Preferences::register_group(PREFERENCES_GROUP_KEY_ROUTING, QObject::tr("Routing"));

	SGVariant tmp((char *) NULL);
	Preferences::register_parameter(prefs, tmp, PREFERENCES_GROUP_KEY_ROUTING);
}




static RoutingEngine * search_by_string_id(std::vector<RoutingEngine *> & engines, const QString & string_id)
{
	for (auto iter = engines.begin(); iter != engines.end(); iter++) {
		RoutingEngine * engine = *iter;
		if (QString(engine->get_id()) == string_id) {
			return engine;
		}
	}
	return NULL;
}




/**
 * @id: the id of the engine we are looking for.
 *
 * Returns: the found engine or %NULL.
 */
RoutingEngine * vik_routing_find_engine(const char * id)
{
	return search_by_string_id(routing_engines, QString(id));
}




/**
 * Retrieve the default engine, based on user's preferences.
 *
 * Returns: the default engine.
 */
RoutingEngine * SlavGPS::routing_default_engine(void)
{
	const char * id = a_preferences_get(PREFERENCES_NAMESPACE_ROUTING "default")->s;
	RoutingEngine * engine = vik_routing_find_engine(id);
	if (engine == NULL && routing_engines.size()) {
		/* Fallback to first element */
		engine = routing_engines[0];
	}

	return engine;
}




/**
 * Route computation with default engine.
 *
 * Return indicates success or not.
 */
bool SlavGPS::routing_default_find(LayerTRW * trw, struct LatLon start, struct LatLon end)
{
	/* The engine. */
	RoutingEngine * engine = routing_default_engine();
	/* The route computation. */
	return engine->find(trw, start, end);
}




/**
 * @engine: new routing engine to register.
 *
 * Register a new routing engine.
 */
void SlavGPS::routing_register(RoutingEngine * engine)
{
	const QString label = QString(engine->get_label());
	const QString string_id = QString(engine->get_id());

	/* Check if string_id already exists in list. */
	RoutingEngine * found_engine = search_by_string_id(routing_engines, string_id);
	if (found_engine) {
		qDebug() << "DD: Routing: register:" << string_id << "already exists: update";
#ifdef K
		/* Update main list. */
		g_object_unref(found_engine);
		found_engine = g_object_ref(engine);
#endif

		/* Update GUI arrays. */
		size_t len = routing_engine_combo_items.size();
		for (; len > 0 ; len--) {
			if (routing_engine_ids[len - 1] == string_id) {
				break;
			}
		}

		/* Update the label (possibly different). */
		routing_engine_combo_items[len - 1].label = label;

		/* TODO: verify that updated list of routers is displayed correctly a combo list in dialog. */
	} else {
		qDebug() << "DD: Routing: register:" << string_id << "is new: append";
		routing_engines.push_back(engine);

		const size_t len = routing_engine_combo_items.size();

		/* Add the label. */
		routing_engine_combo_items.push_back(SGLabelID(label, (int) len)); /* We use old length of vector as numeric ID. */

		/* Add the string id. */
		routing_engine_ids.push_back(string_id);

		/* TODO: verify that constructed list of routers is visible as a combo list in dialog. */

#ifdef K
		/* TODO: previously the string IDs of routing engines
		   were passed to UI builder like below. Verify
		   whether this is still necessary. */
		prefs[0].extra_widget_data = routing_engine_ids;
#endif
	}
}




/**
 * Unregister all registered routing engines.
 */
void SlavGPS::routing_unregister_all()
{
#ifdef K
	g_list_foreach(routing_engines, (GFunc) g_object_unref, NULL);
#endif
}




/**
 * @func: the function to run on each element.
 * @user_data: user's data to give to each call of @func.
 *
 * Loop over all registered routing engines.
 */
void SlavGPS::routing_foreach_engine(GFunc func, QComboBox * combo)
{
#ifdef K
	g_list_foreach(routing_engines, func, user_data);
#endif
}




/*
 * This function is called for all routing engine registered.
 * Following result of the predicate function, the current engine
 * is added to the combobox. In order to retrieve the RoutingEngine
 * object, we store a list of added engine in a void's data "engines".
 *
 * @see g_list_foreach()
 */
static void fill_engine_box(void * data, QComboBox * user_data)
{
	RoutingEngine * engine = (RoutingEngine *) data;
	/* Retrieve combo. */
	QComboBox * combo = (QComboBox *) user_data;
#ifdef K
	/* Only register engine fulfilling expected behavior. */
	Predicate predicate = (Predicate) g_object_get_data(G_OBJECT (combo), "func");
	void * predicate_data = g_object_get_data(G_OBJECT (combo), "user_data");
	/* No predicate means to register all engines. */
	bool ok = predicate == NULL || predicate(engine, predicate_data);

	if (ok) {
		/* Add item in widget. */
		const char *label = engine->get_label();
		vik_combo_box_text_append(combo, label);
		/* Save engine in internal list. */
		GList *engines = (GList*) g_object_get_data(combo , "engines");
		engines = g_list_append(engines, engine);
		g_object_set_data(combo, "engines", engines);
	}
#endif
}




/**
 * @func: user function to decide if an engine has to be added or not.
 * @user_data: user data for previous function.
 *
 * Creates a combo box to allow selection of a routing engine.
 *
 * We use void data hastable to store and retrieve the RoutingEngine
 * associated to the selection.
 *
 * Returns: the combo box
 */
QComboBox * SlavGPS::routing_ui_selector_new(Predicate func, void * user_data)
{
	/* Create the combo */
	QComboBox * combo = new QComboBox();
#ifdef K
	/* Save data for foreach function. */
	g_object_set_data(G_OBJECT (combo), "func", (void *) func);
	g_object_set_data(G_OBJECT (combo), "user_data", user_data);

	/* Filter all engines with given user function. */
	routing_foreach_engine(fill_engine_box, combo);
#endif
	return combo;
}




/**
 * @combo: the GtkWidget combobox
 * @pos: the selected position
 *
 * Retrieve the RoutingEngine stored in a list attached to @combo
 * via the "engines" property.
 *
 * Returns: the RoutingEngine object associated to @pos.
 */
RoutingEngine * SlavGPS::routing_ui_selector_get_nth(QComboBox * combo, int pos)
{
	RoutingEngine * engine = NULL;
#ifdef K
	/* Retrieve engine. */
	GList *engines = (GList*) g_object_get_data (G_OBJECT (combo) , "engines");
	RoutingEngine * engine = (RoutingEngine *) g_list_nth_data(engines, pos);
#endif
	return engine;
}
