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

#ifndef _SG_ROUTING_H_
#define _SG_ROUTING_H_




#include <vector>
#include <utility>




#include <QComboBox>




#include "routing_engine.h"




namespace SlavGPS {




	class LayerTRW;
	class LatLon;




	typedef bool (* RoutingEnginePredicate)(const RoutingEngine * engine);
	typedef std::vector<std::pair<RoutingEngine *, int>> EnginesContainer; /* Engine + unique integer ID */



	class Routing {
	public:
		/**
		   @brief Register a new routing engine

		   @engine: new routing engine to register
		*/
		static void register_engine(RoutingEngine * engine);

		static void unregister_all_engines(void); /* TODO_LATER: this function is not called anywhere. */
		static void prefs_init(void);

		/**
		   @brief Get the default engine's name, based on user's preferences

		   The name is returned through @param engine_name.

		   @return true on success (@param engine_name is set)
		   @return false otherwise
		*/
		static bool get_default_engine_name(QString & engine_name);

		/**
		   @brief Route computation with default engine

		   @return true on success
		   @return false otherwise (either there is no routing engine or routing algo failed)
		*/
		static bool find_route_with_default_engine(LayerTRW * trw, const LatLon & start, const LatLon & end);

		/**
		   @brief Creates a combo box to allow selection of a routing engine

		   @param predicate: user function to decide if an engine has to be added or not
		   @param default_engine_id: ID string of engine that should be selected in combo by default

		   Returned pointer is owned by caller

		   @return newly allocated combo box
		*/
		static QComboBox * create_engines_combo(RoutingEnginePredicate predicate, const QString & default_engine_id);

		/**
		   @brief Get routing engine by its ID

		   @param string_id - id of engine to look up

		   @return RoutingEngine object with given ID on success
		   @return nullptr on failure
		*/
		static const RoutingEngine * get_engine_by_id(const QString & string_id);

	private:
		static EnginesContainer::iterator get_default_engine_iter(void);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_ROUTING_H_ */
