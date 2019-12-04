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




#include <list>




#include <QDebug>




#include "babel.h"
#include "preferences.h"
#include "routing.h"
#include "routing_engine.h"
#include "util.h"
#include "ui_builder.h"




using namespace SlavGPS;




/* Params will be routing.default */
/* We have to make sure these don't collide. */
#define PREFERENCES_NAMESPACE_ROUTING "routing."




static std::vector<RoutingEngine *> routing_engines;   /* List to register all routing engines. */
static std::vector<SGLabelID> routing_engine_combo_items;
static std::vector<QString> routing_engine_ids; /* These are string IDs. */



static ParameterSpecification prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_ROUTING "default", SGVariantType::String, PARAMETER_GROUP_GENERIC, QObject::tr("Default engine:"), WidgetType::ComboBox, &routing_engine_combo_items, NULL, "" },
};




/**
   Initialize the preferences of the routing feature.
*/
void Routing::prefs_init(void)
{
	Preferences::register_parameter_group(PREFERENCES_NAMESPACE_ROUTING, QObject::tr("Routing"));
	Preferences::register_parameter_instance(prefs[0], SGVariant("", prefs[0].type_id));
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
   @id: the id of the engine we are looking for.

   Returns: the found engine or %NULL.
*/
RoutingEngine * routing_ui_find_engine(const QString & id)
{
	return search_by_string_id(routing_engines, id);
}




/**
   Retrieve the default engine, based on user's preferences.

   Returns: the default engine.
*/
RoutingEngine * Routing::get_default_engine(void)
{
	const QString id = Preferences::get_param_value(PREFERENCES_NAMESPACE_ROUTING "default").val_string;
	RoutingEngine * engine = routing_ui_find_engine(id);
	if (engine == NULL && routing_engines.size()) {
		/* Fallback to first element */
		engine = routing_engines[0];
	}

	return engine;
}




/**
   @brief Route computation with default engine

   @return value indicating success or failure
*/
bool Routing::find_route_with_default_engine(LayerTRW * trw, const LatLon & start, const LatLon & end)
{
	/* The engine. */
	RoutingEngine * engine = Routing::get_default_engine();
	/* The route computation. */
	return engine->find(trw, start, end);
}




/**
   @brief Register a new routing engine

   @engine: new routing engine to register
*/
void Routing::register_engine(RoutingEngine * engine)
{
	const QString label = engine->get_label();
	const QString string_id = engine->get_id();

	/* Check if string_id already exists in list. */
	RoutingEngine * found_engine = search_by_string_id(routing_engines, string_id);
	if (found_engine) {
		qDebug() << "DD: Routing: register:" << string_id << "already exists: update";
#ifdef K_FIXME_RESTORE
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

		/* TODO_LATER: verify that updated list of routers is displayed correctly a combo list in dialog. */
	} else {
		qDebug() << "DD: Routing: register:" << string_id << "is new: append";
		routing_engines.push_back(engine);

		const size_t len = routing_engine_combo_items.size();

		/* Add the label. */
		routing_engine_combo_items.push_back(SGLabelID(label, (int) len)); /* We use old length of vector as numeric ID. */

		/* Add the string id. */
		routing_engine_ids.push_back(string_id);

		/* TODO_LATER: verify that constructed list of routers is visible as a combo list in dialog. */

#ifdef K_FIXME_RESTORE
		/* TODO_LATER: previously the string IDs of routing engines
		   were passed to UI builder like below. Verify
		   whether this is still necessary. */
		prefs[0].extra_widget_data = routing_engine_ids;
#endif
	}
}




/**
   Unregister all registered routing engines.
*/
void Routing::unregister_all_engines(void)
{
	for (auto iter = routing_engines.begin(); iter != routing_engines.end(); iter++) {
		delete *iter;
	}
}





/**
   @brief Creates a combo box to allow selection of a routing engine

   @predicate: user function to decide if an engine has to be added or not

   We use void data hastable to store and retrieve the RoutingEngine
   associated to the selection.

   @return newly allocated combo box
*/
QComboBox * Routing::create_engines_combo(RoutingEnginePredicate predicate)
{
	QComboBox * combo = new QComboBox();

	/* Filter all engines with given user function. */

	for (auto iter = routing_engines.begin(); iter != routing_engines.end(); iter++) {
		RoutingEngine * engine = *iter;

		/* Only register engine fulfilling expected behavior.
		   No predicate means to register all engines. */
		const bool add = predicate == NULL || predicate(engine);
		if (add) {
			combo->addItem(engine->get_label(), engine->id);
		}
	}

	return combo;
}




/**
   @combo: routing engines combo
   @index: index of item selected in the combo

   @return RoutingEngine object from position @index in @combo.
*/
RoutingEngine * Routing::get_engine_by_index(QComboBox * combo, int index)
{
	const QString engine_id = combo->itemData(index).toString();
	RoutingEngine * engine = routing_ui_find_engine(engine_id);
	return engine;
}
