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

	if (this->url_format) {
		free(this->url_format);
		this->url_format = NULL;
	}
}




QString WebToolCenter::get_url_at_position(Window * a_window, const Coord * a_coord)
{
	uint8_t zoom_level = 17;
	struct LatLon ll;


	Viewport * viewport = a_window->get_viewport();
	/* Coords.
	   Use the provided position otherwise use center of the viewport. */
	if (a_coord) {
		ll = a_coord->get_latlon();
	} else {
		ll = viewport->get_center()->get_latlon();
	}

	/* Zoom - ideally x & y factors need to be the same otherwise use the default. */
	if (viewport->get_xmpp() == viewport->get_ympp()) {
		zoom_level = this->mpp_to_zoom_level(viewport->get_zoom());
	}

	QString string_lat;
	QString string_lon;
	CoordUtils::to_strings(string_lat, string_lon, ll);

	char * url = g_strdup_printf(this->url_format, string_lat.toUtf8().constData(), string_lon.toUtf8().constData(), zoom_level);
	QString result(url);
	free(url);

	qDebug() << "II: Web Tool Center: url at position is" << result;

	return result;
}




QString WebToolCenter::get_url_at_current_position(Window * a_window)
{
	return this->get_url_at_position(a_window, NULL);
}
