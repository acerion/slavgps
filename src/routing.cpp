/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2012-2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (c) 2016-2019 Kamil Ignacak <acerion@wp.pl>
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




#include <QDebug>




#include "babel.h"
#include "preferences.h"
#include "routing.h"
#include "routing_engine.h"
#include "util.h"
#include "ui_builder.h"




using namespace SlavGPS;




#define SG_MODULE "Routing"

/* Params will be routing.default */
/* We have to make sure these don't collide. */
#define PREFERENCES_NAMESPACE_ROUTING "routing."



/*
  Viking presents RoutingEngine::m_name in combo list, and saves
  RoutingEngine::m_id to config file.

  Therefore routing_engines_enum should contain engine names.
*/
static EnginesContainer registered_routing_engines;
static WidgetStringEnumerationData routing_engines_enum = {
	{
	},
	""
};




static ParameterSpecification prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_ROUTING "default", SGVariantType::String, PARAMETER_GROUP_GENERIC, QObject::tr("Default engine:"), WidgetType::StringEnumeration, &routing_engines_enum, NULL, "" },
};




void Routing::init(void)
{
	Preferences::register_parameter_group(PREFERENCES_NAMESPACE_ROUTING, QObject::tr("Routing"));
	Preferences::register_parameter_instance(prefs[0], SGVariant(routing_engines_enum.default_string, prefs[0].type_id));
}




/**
   @reviewed-on 2019-12-08
*/
void Routing::uninit(void)
{
	Routing::unregister_all_engines();
}




/**
   @reviewed-on 2019-12-07
*/
static EnginesContainer::iterator search_by_string_id(EnginesContainer & engines, const QString & string_id)
{
	for (auto iter = engines.begin(); iter != engines.end(); iter++) {
		RoutingEngine * engine = *iter;
		if (QString(engine->get_id()) == string_id) {
			return iter;
		}
	}
	return engines.end();
}




/**
   @reviewed-on 2019-12-08
*/
static EnginesContainer::iterator search_by_name(EnginesContainer & engines, const QString & name)
{
	for (auto iter = engines.begin(); iter != engines.end(); iter++) {
		RoutingEngine * engine = *iter;
		if (QString(engine->get_name()) == name) {
			return iter;
		}
	}
	return engines.end();
}




/**
   @reviewed-on 2019-12-07
*/
EnginesContainer::iterator Routing::get_default_engine_iter(void)
{
	const QString id = Preferences::get_param_value(PREFERENCES_NAMESPACE_ROUTING "default").val_string;
	EnginesContainer::iterator iter = search_by_string_id(registered_routing_engines, id);
	if (iter != registered_routing_engines.end()) {
		qDebug() << SG_PREFIX_E << "Can't find engine with id" << id;
		return iter;
	}

	/* Given ID was not found, but try to fall back to first engine on a list. */
	if (registered_routing_engines.size()) {
		return registered_routing_engines.begin();
	}

	return registered_routing_engines.end();
}




/**
   @reviewed-on 2019-12-07
*/
bool Routing::get_default_engine_name(QString & engine_name)
{
	auto iter = Routing::get_default_engine_iter();
	if (iter == registered_routing_engines.end()) {
		return false;
	} else {
		engine_name = (*iter)->get_name();
		return true;
	}
}




bool Routing::find_route_with_default_engine(LayerTRW * trw, const LatLon & start, const LatLon & end)
{
	/* The engine. */
	auto iter = Routing::get_default_engine_iter();
	if (iter == registered_routing_engines.end()) {
		qDebug() << SG_PREFIX_N << "No routing engine found";
		return false;
	} else {
		qDebug() << SG_PREFIX_N << "Will try to find route with routing engine" << (*iter)->get_name();
		return (*iter)->find_route(trw, start, end);
	}
}




/**
   @reviewed-on 2019-12-07
*/
void Routing::register_engine(RoutingEngine * engine)
{
	const QString new_string_id = engine->get_id();

	/* Check if new_string_id already exists in list. */
	auto engine_iter = search_by_string_id(registered_routing_engines, new_string_id);
	if (engine_iter != registered_routing_engines.end()) {
		qDebug() << SG_PREFIX_I << "Routing engine" << new_string_id << "already exists: will update the entry";
		qDebug() << SG_PREFIX_I << "Replacing engine" << (*engine_iter)->get_name() << "with" << engine->get_name();

		/* Replace old routing engine definition with new
		   one. This may be used to replace old hardcoded
		   definition (e.g. with old/invalid parameters) with
		   new, updated definition from external file. */
		delete *engine_iter;
		*engine_iter = engine;
	} else {
		qDebug() << SG_PREFIX_I << "Registering new engine" << new_string_id;
		registered_routing_engines.push_back(engine);
	}

	/*
	  Update (re-generate) data structure used to generate combo
	  in user interface.

	  TODO_MAYBE: The loop will be executed on each registration
	  of router, so there will be some unnecessary work, but no
	  too much. Maybe fix it in future: execute the loop only once
	  after all engines have been registered. But how to recognize
	  that moment?
	*/
	routing_engines_enum.values.clear();
	for (auto iter = registered_routing_engines.begin(); iter != registered_routing_engines.end(); iter++) {
		routing_engines_enum.values.push_back((*iter)->get_name());
	}
}




/**
   Unregister all registered routing engines.
*/
void Routing::unregister_all_engines(void)
{
	for (auto iter = registered_routing_engines.begin(); iter != registered_routing_engines.end(); iter++) {
		delete *iter;
	}
}




/**
   @reviewed-on 2019-12-07
*/
QComboBox * Routing::create_engines_combo(RoutingEnginePredicate predicate, const QString & default_engine_id)
{
	QComboBox * combo = new QComboBox();
	int current_index = -1;
	int i = 0;

	for (auto iter = registered_routing_engines.begin(); iter != registered_routing_engines.end(); iter++) {
		const RoutingEngine * engine = *iter;

		/* Only register engine fulfilling expected behavior.
		   No predicate means to put in combo all engines. */
		const bool add = predicate == nullptr || predicate(engine);
		if (add) {
			QVariant id(engine->get_id());
			combo->addItem(engine->get_name(), id);
			if (engine->get_id() == default_engine_id) {
				current_index = i;
			}
			i++;
		}
	}
	if (current_index != -1) {
		combo->setCurrentIndex(current_index);
	}

	return combo;
}




/**
   @reviewed-on 2019-12-08
*/
const RoutingEngine * Routing::get_engine_by_name(const QString & name)
{
	EnginesContainer::iterator iter = search_by_name(registered_routing_engines, name);
	if (iter != registered_routing_engines.end()) {
		return *iter;
	} else {
		return nullptr;
	}
}
