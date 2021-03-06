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




#define SG_MODULE "Online Service with Center"




OnlineService_center::OnlineService_center(const QString & new_label, const QString & new_url_format) : OnlineService(new_label)
{
	qDebug() << SG_PREFIX_I << "Created tool with label" << new_label;

	this->label = new_label;
	this->url_format = new_url_format;
}




OnlineService_center::~OnlineService_center()
{
	qDebug() << SG_PREFIX_I << "Delete tool" << this->label;
}




QString OnlineService_center::get_url_at_position(GisViewport * a_gisview, const Coord * a_coord)
{
	QString result;
	const VikingScale viking_scale = a_gisview->get_viking_scale();

	/* Use the provided position otherwise use center of the viewport. */
	if (a_coord) {
		qDebug() << SG_PREFIX_I << "Getting URL for specific coordinate";
		result = this->get_url_for_coord(*a_coord, viking_scale);
	} else {
		qDebug() << SG_PREFIX_I << "Getting URL for center of viewport";
		result = this->get_url_for_coord(a_gisview->get_center_coord(), viking_scale);
	}

	return result;
}




QString OnlineService_center::get_url_for_viewport(GisViewport * a_gisview)
{
	return this->get_url_for_coord(a_gisview->get_center_coord(), a_gisview->get_viking_scale());
}




QString OnlineService_center::get_url_for_coord(const Coord & a_coord, const VikingScale & viking_scale)
{
	const LatLon lat_lon = a_coord.get_lat_lon();

	/* Zoom - ideally x & y factors need to be the same otherwise use the default. */
	TileZoomLevel zoom(TileZoomLevel::Level::Default);
	if (viking_scale.x_y_is_equal()) {
		zoom = viking_scale.to_tile_zoom_level();
	}

	QString string_lat;
	QString string_lon;
	lat_lon.to_strings_raw(string_lat, string_lon);

	const QString url = QString(this->url_format).arg(string_lat).arg(string_lon).arg(zoom.value());

	qDebug() << SG_PREFIX_I << "Result URL is" << url;

	return url;
}
