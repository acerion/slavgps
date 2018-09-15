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




#include <QDebug>




#include "window.h"
#include "viewport_internal.h"
#include "webtool_center.h"
#include "util.h"
#include "map_utils.h"




using namespace SlavGPS;




WebToolCenter::WebToolCenter(const QString & new_label, const QString & new_url_format) : WebTool(new_label)
{
	qDebug() << "II: Web Tool Center created with label" << new_label;

	this->label = new_label;
	this->url_format = new_url_format;
}




WebToolCenter::~WebToolCenter()
{
	qDebug() << "II: Web Tool Center: delete tool with" << this->label;
}




QString WebToolCenter::get_url_at_position(Viewport * a_viewport, const Coord * a_coord)
{

	LatLon lat_lon;

	/* Coords.
	   Use the provided position otherwise use center of the viewport. */
	if (a_coord) {
		lat_lon = a_coord->get_latlon();
	} else {
		lat_lon = a_viewport->get_center()->get_latlon();
	}

	/* Zoom - ideally x & y factors need to be the same otherwise use the default. */
	TileZoomLevel tile_zoom_level(TileZoomLevels::Default);
	if (a_viewport->get_viking_zoom_level().x_y_is_equal()) {
		tile_zoom_level = a_viewport->get_viking_zoom_level().to_tile_zoom_level();
	}

	QString string_lat;
	QString string_lon;
	lat_lon.to_strings_raw(string_lat, string_lon);

	const QString url = QString(this->url_format).arg(string_lat).arg(string_lon).arg(tile_zoom_level.get_value());

	qDebug() << "II: Web Tool Center: url at position is" << url;

	return url;
}




QString WebToolCenter::get_url_at_current_position(Viewport * viewport)
{
	return this->get_url_at_position(viewport, NULL);
}
