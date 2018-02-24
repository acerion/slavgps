/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2008, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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




#include <cstring>
#include <cstdlib>
#include <cstdint>

#include <QDebug>

#include <glib.h>

#include "window.h"
#include "viewport_internal.h"
#include "vikwebtoolcenter.h"
#include "util.h"
#include "globals.h"




using namespace SlavGPS;




WebToolCenter::WebToolCenter(const QString & new_label, char const * new_url_format) : WebTool(new_label)
{
	qDebug() << "II: Web Tool Center created with label" << new_label;

	this->label = new_label;
	this->url_format = strdup(new_url_format);
}




WebToolCenter::~WebToolCenter()
{
	qDebug() << "II: Web Tool Center: delete tool with" << this->label;
}




QString WebToolCenter::get_url_at_position(Viewport * a_viewport, const Coord * a_coord)
{
	uint8_t zoom_level = 17;
	LatLon lat_lon;

	/* Coords.
	   Use the provided position otherwise use center of the viewport. */
	if (a_coord) {
		lat_lon = a_coord->get_latlon();
	} else {
		lat_lon = a_viewport->get_center()->get_latlon();
	}

	/* Zoom - ideally x & y factors need to be the same otherwise use the default. */
	if (a_viewport->get_xmpp() == a_viewport->get_ympp()) {
		zoom_level = this->mpp_to_zoom_level(a_viewport->get_zoom());
	}

	QString string_lat;
	QString string_lon;
	lat_lon.to_strings_raw(string_lat, string_lon);

	const QString url = QString(this->url_format).arg(string_lat).arg(string_lon).arg(zoom_level);

	qDebug() << "II: Web Tool Center: url at position is" << url;

	return url;
}




QString WebToolCenter::get_url_at_current_position(Viewport * a_viewport)
{
	return this->get_url_at_position(a_viewport, NULL);
}
