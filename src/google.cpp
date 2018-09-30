/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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




#include <cstdlib>




#include "google.h"
#include "external_tools.h"
#include "webtool_center.h"
#include "goto.h"
#include "googlesearch.h"
#include "routing.h"
#include "routing_engine_web.h"
#include "babel.h"




using namespace SlavGPS;




void Google::init(void)
{
#ifdef VIK_CONFIG_GOOGLE

	ExternalTools::register_tool(new WebToolCenter(QObject::tr("Google"), "http://maps.google.com/maps/@%1,%2,%3z"));

	/* Goto. */
#ifdef K_OLD_IMPLEMENTATION
	/* Google no longer supports the API we used. */

	GoogleGotoTool * gototool = google_goto_tool_new();
	GoTo::register_tool(gototool);
	g_object_unref(gototool);
#endif
#endif
}




/**
   Delayed initialization part as the check for gpsbabel availability needs to have been performed
*/
void Google::post_init(void)
{
#ifdef VIK_CONFIG_GOOGLE

	// Routing
	/* Google Directions service as routing engine.
	 *
	 * Technical details are available here:
	 * https://developers.google.com/maps/documentation/directions/#DirectionsResponses
	 *
	 * gpsbabel supports this format.
	 */

#ifdef K_OLD_IMPLEMENTATION
	/* Google no longer supports the API we used. */

	if (Babel::is_available()) {
		 RoutingEngine * engine = g_object_new(VIK_ROUTING_WEB_ENGINE_TYPE,
							   "id", "google",
							   "label", "Google",
							   "format", "google",
							   "url-base", "http://maps.google.com/maps?output=js&q=",
							   "url-start-ll", "from:%s,%s",
							   "url-stop-ll", "+to:%s,%s",
							   "url-start-dir", "from:%s",
							   "url-stop-dir", "+to:%s",
							   "referer", "http://maps.google.com/",
							   NULL);
		 Routing::register_engine(engine);
	 }
#endif
#endif
}
