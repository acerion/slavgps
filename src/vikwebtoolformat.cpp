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




#include <cstring>
#include <cstdlib>

#include <QDebug>

#include "window.h"
#include "viewport_internal.h"
#include "vikwebtoolformat.h"
#include "util.h"
#include "globals.h"
#include "map_utils.h"




using namespace SlavGPS;




#define MAX_NUMBER_CODES 9




WebToolFormat::WebToolFormat(const QString & new_label, const char * new_url_format, const char * new_url_format_code) : WebTool(new_label)
{
	qDebug() << "II: Web Tool Format tool created with label" << new_label;

	this->label = new_label;

	if (new_url_format) {
		this->url_format = strdup(new_url_format);
	}
	if (new_url_format_code) {
		this->url_format_code = strdup(new_url_format_code);
	}
}




WebToolFormat::~WebToolFormat()
{
	qDebug() << "II: Web Tool Format: delete tool with label" << this->label;

	if (this->url_format_code) {
		free(this->url_format_code);
		this->url_format_code = NULL;
	}
}




uint8_t WebToolFormat::mpp_to_zoom_level(double mpp)
{
	return map_utils_mpp_to_zoom_level(mpp);
}




QString WebToolFormat::get_url_at_position(Window * a_window, const Coord * a_coord)
{
	Viewport * viewport = a_window->get_viewport();

	// Center values
	struct LatLon ll = viewport->get_center()->get_latlon();

	QString center_lat;
	QString center_lon;
	CoordUtils::to_strings(center_lat, center_lon, ll);

	struct LatLon llpt;
	llpt.lat = 0.0;
	llpt.lon = 0.0;
	if (a_coord) {
		ll = a_coord->get_latlon(); /* kamilFIXME: shouldn't this be "llpt = "? */
	}
	QString point_lat;
	QString point_lon;
	CoordUtils::to_strings(point_lat, point_lon, llpt);

	uint8_t zoom_level = 17; // A zoomed in default
	// zoom - ideally x & y factors need to be the same otherwise use the default
	if (viewport->get_xmpp() == viewport->get_ympp()) {
		zoom_level = map_utils_mpp_to_zoom_level(viewport->get_zoom());
	}

	char szoom[G_ASCII_DTOSTR_BUF_SIZE];
	snprintf(szoom, G_ASCII_DTOSTR_BUF_SIZE, "%d", zoom_level);

	int len = 0;
	if (this->url_format_code) {
		len = strlen (this->url_format_code);
	}
	if (len > MAX_NUMBER_CODES) {
		len = MAX_NUMBER_CODES;
	}

	char * values[MAX_NUMBER_CODES];
	for (int i = 0; i < MAX_NUMBER_CODES; i++) {
		values[i] = '\0';
	}

	LatLonBBoxStrings bbox_strings;
	viewport->get_bbox_strings(bbox_strings);

	for (int i = 0; i < len; i++) {
		switch (g_ascii_toupper(this->url_format_code[i])) {
		case 'L': values[i] = g_strdup(bbox_strings.min_lon.toUtf8().constData()); break;
		case 'R': values[i] = g_strdup(bbox_strings.max_lon.toUtf8().constData()); break;
		case 'B': values[i] = g_strdup(bbox_strings.min_lat.toUtf8().constData()); break;
		case 'T': values[i] = g_strdup(bbox_strings.max_lat.toUtf8().constData()); break;
		case 'A': values[i] = g_strdup(center_lat.toUtf8().constData()); break;
		case 'O': values[i] = g_strdup(center_lon.toUtf8().constData()); break;
		case 'Z': values[i] = g_strdup(szoom); break;
		case 'P': values[i] = g_strdup(point_lat.toUtf8().constData()); break;
		case 'N': values[i] = g_strdup(point_lon.toUtf8().constData()); break;
		default: break;
		}
	}

	char * url = g_strdup_printf(this->url_format, values[0], values[1], values[2], values[3], values[4], values[5], values[6], values[7], values[8]);

	for (int i = 0; i < MAX_NUMBER_CODES; i++) {
		if (values[i] != '\0') {
			free(values[i]);
		}
	}

	QString result(url);
	free(url);

	qDebug() << "DD: Web Tool Format: url at position is" << result;
	return result;
}




QString WebToolFormat::get_url_at_current_position(Window * a_window)
{
	return this->get_url_at_position(a_window, NULL);
}
