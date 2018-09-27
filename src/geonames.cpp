/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#include "geonames.h"

#include "goto_tool_xml.h"
#include "goto.h"




using namespace SlavGPS;




void Geonames::init(void)
{
#ifdef VIK_CONFIG_GEONAMES

	/* Goto */
	GotoToolXML * geonames = new GotoToolXML(QObject::tr("Geonames"),
						 "http://api.geonames.org/search?q=%1&maxRows=1&lang=en&style=short&username=viking", /* TODO_2_LATER: update username value. */
						 "geonames/geoname/lat",
						 "geonames/geoname/lng");
	GoTo::register_tool(geonames);
#endif
}
