/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Format Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Format Public License for more details.
 *
 * You should have received a copy of the GNU Format Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */




#include <vector>




#include <QDebug>




#include "window.h"
#include "viewport_internal.h"
#include "webtool_format.h"
#include "util.h"
#include "map_utils.h"




using namespace SlavGPS;




#define MAX_NUMBER_CODES 9




#define SG_MODULE "Online Service with Format"




OnlineService_format::OnlineService_format(const QString & new_label, const QString & new_url_format, const QString & new_url_format_code) : OnlineService(new_label)
{
	qDebug() << SG_PREFIX_I << "Created tool with label" << new_label;

	this->label = new_label;
	this->url_format = new_url_format;
	this->url_format_code = new_url_format_code;
}




OnlineService_format::~OnlineService_format()
{
	qDebug() << SG_PREFIX_I << "Delete tool with label" << this->label;
}




/* TODO_LATER: compare with QString OnlineService_query::get_url_for_viewport(Viewport * viewport) */
QString OnlineService_format::get_url_at_position(GisViewport * a_gisview, const Coord * a_coord)
{
	/* Center values. */
	LatLon center_lat_lon = a_gisview->get_center().get_lat_lon();
	QString center_lat;
	QString center_lon;
	center_lat_lon.to_strings_raw(center_lat, center_lon);


	LatLon position_lat_lon;
	if (a_coord) {
		position_lat_lon = a_coord->get_lat_lon();
	} else {
		/* Coordinate not provided to function, so fall back to center of viewport. */
		position_lat_lon = center_lat_lon;
	}
	QString position_lat;
	QString position_lon;
	position_lat_lon.to_strings_raw(position_lat, position_lon);


	/* Zoom - ideally x & y factors need to be the same otherwise use the default. */
	TileZoomLevel tile_zoom_level(TileZoomLevels::Default); /* Zoomed in by default. */
	if (a_gisview->get_viking_scale().x_y_is_equal()) {
		tile_zoom_level = a_gisview->get_viking_scale().to_tile_zoom_level();
	}

	int len = this->url_format_code.size();
	if (len == 0) {
		qDebug() << SG_PREFIX_E << "url format code is empty";
		return QString("");
	} else if (len > MAX_NUMBER_CODES) {
		qDebug() << SG_PREFIX_E << "url format code too long:" << len << MAX_NUMBER_CODES << this->url_format_code;
		return QString("");
	} else {
		;
	}

	const LatLonBBoxStrings bbox_strings = a_gisview->get_bbox().values_to_c_strings();

	QString url = this->url_format;

	/* Evaluate+replace each consecutive format specifier %1, %2,
	   %3 etc. in url=='format string' with a value. */
	for (int i = 0; i < len; i++) {
		switch (this->url_format_code[i].toUpper().toLatin1()) {
		case 'L': url = url.arg(bbox_strings.west);  break;
		case 'R': url = url.arg(bbox_strings.east);  break;
		case 'B': url = url.arg(bbox_strings.south); break;
		case 'T': url = url.arg(bbox_strings.north); break;
		case 'A': url = url.arg(center_lat); break;
		case 'O': url = url.arg(center_lon); break;
		case 'Z': url = url.arg(tile_zoom_level.to_string()); break;
		case 'P': url = url.arg(position_lat); break;
		case 'N': url = url.arg(position_lon); break;
		default:
			qDebug() << SG_PREFIX_E << "Invalid URL format code" << this->url_format_code[i] << "at position" << i;
			return QString("");
		}
	}

	qDebug() << SG_PREFIX_D << "URL at position is" << url;

	return url;
}




QString OnlineService_format::get_url_for_viewport(GisViewport * a_gisview)
{
	return this->get_url_at_position(a_gisview, NULL);
}
