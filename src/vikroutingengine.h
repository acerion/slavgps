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

#include <glib.h>

#include "layer_trw.h"
#include "coords.h"
#include "download.h"
#include "window.h"




#define VIK_ROUTING_ENGINE_TYPE            (vik_routing_engine_get_type ())
#define VIK_ROUTING_ENGINE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_ROUTING_ENGINE_TYPE, VikRoutingEngine))
#define VIK_ROUTING_ENGINE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_ROUTING_ENGINE_TYPE, VikRoutingEngineClass))
#define VIK_IS_ROUTING_ENGINE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_ROUTING_ENGINE_TYPE))
#define VIK_IS_ROUTING_ENGINE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_ROUTING_ENGINE_TYPE))
#define VIK_ROUTING_ENGINE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), VIK_ROUTING_ENGINE_TYPE, VikRoutingEngineClass))




typedef struct _VikRoutingEngine VikRoutingEngine;
typedef struct _VikRoutingEngineClass VikRoutingEngineClass;

struct _VikRoutingEngineClass {
	GObjectClass object_class;
	bool (* find)(VikRoutingEngine * self, SlavGPS::LayerTRW * trw, struct LatLon start, struct LatLon end);
	char * (* get_url_from_directions)(VikRoutingEngine * self, const char * start, const char * end);
	bool (* supports_direction)(VikRoutingEngine *self);
	bool (* refine)(VikRoutingEngine * self, SlavGPS::LayerTRW * trw, SlavGPS::Track * trk);
	bool (* supports_refine)(VikRoutingEngine * self);
};

GType vik_routing_engine_get_type();

struct _VikRoutingEngine {
	GObject obj;
};

bool vik_routing_engine_find(VikRoutingEngine * self, SlavGPS::LayerTRW * trw, struct LatLon start, struct LatLon end);
bool vik_routing_engine_refine(VikRoutingEngine * self, SlavGPS::LayerTRW * trw, SlavGPS::Track * trk);
char *vik_routing_engine_get_url_from_directions(VikRoutingEngine * self, const char * start, const char * end);

/* Accessors. */
char * vik_routing_engine_get_id(VikRoutingEngine * self);
char * vik_routing_engine_get_label(VikRoutingEngine * self);
char * vik_routing_engine_get_format(VikRoutingEngine * self);

bool vik_routing_engine_supports_direction(VikRoutingEngine * self);
bool vik_routing_engine_supports_refine(VikRoutingEngine * self);




#endif /* #ifndef _SG_ROUTING_ENGINE_H_ */
