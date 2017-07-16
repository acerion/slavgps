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
 * SECTION:vikroutingengine
 * @short_description: the base class to describe routing engine
 *
 * The #RoutingEngine class is both the interface and the base class
 * for the hierarchie of routing engines.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdlib>

#include <glib.h>
#include <glib/gstdio.h>

#include "layer_trw.h"
#include "babel.h"
#include "routing_engine.h"




using namespace SlavGPS;




#ifdef K
static void vik_routing_engine_set_property(GObject      * object,
					    unsigned int   property_id,
					    const GValue * value,
					    GParamSpec   * pspec)
{
	VikRoutingPrivate * priv = VIK_ROUTING_ENGINE_PRIVATE (object);

	switch (property_id) {
	case PROP_ID:
		free(priv->id);
		priv->id = g_strdup(g_value_get_string(value));
		break;

	case PROP_LABEL:
		free(priv->label);
		priv->label = g_strdup(g_value_get_string(value));
		break;

	case PROP_FORMAT:
		free(priv->format);
		priv->format = g_strdup(g_value_get_string(value));
		break;

	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}



static void vik_routing_engine_get_property(GObject      * object,
					    unsigned int   property_id,
					    GValue       * value,
					    GParamSpec   * pspec)
{
	VikRoutingPrivate * priv = VIK_ROUTING_ENGINE_PRIVATE (object);

	switch (property_id) {
	case PROP_ID:
		g_value_set_string(value, priv->id);
		break;

	case PROP_LABEL:
		g_value_set_string(value, priv->label);
		break;

	case PROP_FORMAT:
		g_value_set_string(value, priv->format);
		break;

	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}
#endif




RoutingEngine::RoutingEngine()
{
}




RoutingEngine::~RoutingEngine()
{
	free(this->id);
	this->id = NULL;

	free(this->label);
	this->label = NULL;

	free(this->format);
	this->format = NULL;
}




/**
 * @trw:
 * @start: starting point
 * @end: ending point
 *
 * Retrieve a route between two coordinates.
 *
 * Returns: indicates success or not.
 */
bool RoutingEngine::find(LayerTRW * trw, struct LatLon start, struct LatLon end)
{
	return false;
}




/**
 * Returns: %true if this engine supports the route finding based on directions.
 */
bool RoutingEngine::supports_direction(void)
{
	return false;
}




/**
 * @start: the start direction
 * @end: the end direction
 *
 * Compute the URL used with the acquire framework.
 *
 * Returns: the computed URL.
 */
char * RoutingEngine::get_url_from_directions(const char * start, const char * end)
{
	return NULL;
}




/**
 * @trw: layer where to create new track
 * @vt: the simple route to refine
 *
 * Retrieve a route refining the @vt track/route.
 *
 * A refined route is computed from @vt.
 * The route is computed from first trackpoint to last trackpoint,
 * and going via all intermediate trackpoints.
 *
 * Returns: indicates success or not.
 */
bool RoutingEngine::refine(LayerTRW * trw, Track * trk)
{
	return false;
}




/**
 * Returns: %true if this engine supports the refine of track.
 */
bool RoutingEngine::supports_refine(void)
{
	return false;
}
