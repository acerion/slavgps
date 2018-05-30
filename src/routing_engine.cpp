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

#include "layer_trw.h"
#include "babel.h"
#include "routing_engine.h"




using namespace SlavGPS;




RoutingEngine::RoutingEngine()
{
}




RoutingEngine::~RoutingEngine()
{
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
bool RoutingEngine::find(LayerTRW * trw, const LatLon & start, const LatLon & end)
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
QString RoutingEngine::get_url_from_directions(const QString & start, const QString & end)
{
	return "";
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




bool SlavGPS::routing_engine_supports_refine(RoutingEngine * engine)
{
	return engine->supports_refine();
}
