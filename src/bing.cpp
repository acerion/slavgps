/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2011, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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




#include "variant.h"
#include "bing.h"
#include "layer_map.h"
#include "map_source_bing.h"
#include "webtool_center.h"
#include "external_tools.h"
#include "util.h"




using namespace SlavGPS;




/** API key registered by Guilhem Bonnefille */
#define API_KEY "AqsTAipaBBpKLXhcaGgP8kceYukatmtDLS1x0CXEhRZnpl1RELF9hlI8j4mNIkrE"




/* Initialization. */
void Bing::init(void)
{
#ifdef VIK_CONFIG_BING
	MapSources::register_map_source(new MapSourceBing(MapTypeID::BingAerial, QObject::tr("Bing Aerial"), API_KEY));

	/* Allow opening web location. */
	ExternalTools::register_tool(new OnlineService_center(QObject::tr("Bing"), "http://www.bing.com/maps/?v=2&cp=%1~%2&lvl=%3"));
#endif
}
