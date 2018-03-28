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
#include "globals.h"
#include "map_utils.h"




using namespace SlavGPS;




#define MAX_NUMBER_CODES 9




WebToolFormat::WebToolFormat(const QString & new_label, const QString & new_url_format, const QString & new_url_format_code) : WebTool(new_label)
{
	qDebug() << "II: Web Tool Format tool created with label" << new_label;

	this->label = new_label;
	this->url_format = new_url_format;
	this->url_format_code = new_url_format_code;
}




WebToolFormat::~WebToolFormat()
{
	qDebug() << "II: Web Tool Format: delete tool with label" << this->label;
}




int WebToolFormat::mpp_to_zoom_level(double mpp)
{
	return map_utils_mpp_to_zoom_level(mpp);
}




QString WebToolFormat::get_url_at_position(Viewport * a_viewport, const Coord * a_coord)
{
	/* Center values. */
	LatLon lat_lon = a_viewport->get_center()->get_latlon();

	QString center_lat;
	QString center_lon;
	lat_lon.to_strings_raw(center_lat, center_lon);

	LatLon llpt;
	llpt.lat = 0.0;
	llpt.lon = 0.0;
	if (a_coord) {
		lat_lon = a_coord->get_latlon(); /* kamilFIXME: shouldn't this be "llpt = "? */
	}
	QString point_lat;
	QString point_lon;
	llpt.to_strings_raw(point_lat, point_lon);

	int zoom_level = 17; // A zoomed in default
	// zoom - ideally x & y factors need to be the same otherwise use the default
	if (a_viewport->get_xmpp() == a_viewport->get_ympp()) {
		zoom_level = map_utils_mpp_to_zoom_level(a_viewport->get_zoom());
	}

	QString zoom((int) zoom_level);

	int len = this->url_format_code.size();
	if (len == 0) {
		qDebug() << "EE: Web Tool Format: url format code is empty";
		return QString("");
	} else if (len > MAX_NUMBER_CODES) {
		qDebug() << "WW: Web Tool Format: url format code too long:" << len << MAX_NUMBER_CODES << this->url_format_code;
		len = MAX_NUMBER_CODES;
	} else {
		;
	}

	std::vector<QString> values;

	const LatLonBBoxStrings bbox_strings = a_viewport->get_bbox().to_strings();

	for (int i = 0; i < len; i++) {
		switch (this->url_format_code[i].toUpper().toLatin1()) {
		case 'L': values[i] = bbox_strings.west;  break;
		case 'R': values[i] = bbox_strings.east;  break;
		case 'B': values[i] = bbox_strings.south; break;
		case 'T': values[i] = bbox_strings.north; break;
		case 'A': values[i] = center_lat; break;
		case 'O': values[i] = center_lon; break;
		case 'Z': values[i] = zoom; break;
		case 'P': values[i] = point_lat; break;
		case 'N': values[i] = point_lon; break;
		default:
			qDebug() << "EE: Web Tool Format: invalid URL format code" << this->url_format_code[i];
			return QString("");
		}
	}

	QString url = QString(this->url_format)
		.arg(values[0]).arg(values[1]).arg(values[2])
		.arg(values[3]).arg(values[4]).arg(values[5])
		.arg(values[6]).arg(values[7]).arg(values[8]);

	qDebug() << "DD: Web Tool Format: url at position is" << url;
	return url;
}




QString WebToolFormat::get_url_at_current_position(Viewport * a_viewport)
{
	return this->get_url_at_position(a_viewport, NULL);
}
