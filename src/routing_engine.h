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
 *
 */
#ifndef _SG_ROUTING_ENGINE_H_
#define _SG_ROUTING_ENGINE_H_




#include <cstdint>

#include "layer_trw.h"
#include "coords.h"
#include "download.h"
#include "window.h"




namespace SlavGPS {

	class RoutingEngine {
	public:
		RoutingEngine();
		~RoutingEngine();

		virtual bool find(LayerTRW * trw, struct LatLon start, struct LatLon end);
		virtual char * get_url_from_directions(const char * start, const char * end);
		virtual bool supports_direction(void);
		virtual bool refine(LayerTRW * trw, SlavGPS::Track * trk);
		virtual bool supports_refine(void);

		char * get_id(void) { return this->id; }
		char * get_label(void) { return this->label; }
		char * get_format(void) { return this->format; }

		char * id = NULL;     /* The identifier of the routing engine. */
		char * label = NULL;  /* The label of the routing engine. */
		char * format = NULL; /* The format of the output (see gpsbabel). */
	};
}




// bool vik_routing_engine_supports_direction(RoutingEngine * self);
// bool vik_routing_engine_supports_refine(RoutingEngine * self);




#endif /* #ifndef _SG_ROUTING_ENGINE_H_ */
