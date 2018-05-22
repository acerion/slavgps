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




#include <QComboBox>




#include "routing_engine.h"




namespace SlavGPS {




	class LayerTRW;
	class LatLon;




	typedef bool (* RoutingEnginePredicate)(RoutingEngine * engine);




	class Routing {
	public:
		static void register_engine(RoutingEngine * engine);
		static void unregister_all_engines(void); /* TODO: this function is not called anywhere. */
		static void prefs_init(void);

		static RoutingEngine * get_default_engine(void);
		static bool find_route_with_default_engine(LayerTRW * trw, const LatLon & start, const LatLon & end);

		static QComboBox * create_engines_combo(RoutingEnginePredicate predicate);
		static RoutingEngine * get_engine_by_index(QComboBox * combo, int index);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_ROUTING_H_ */
