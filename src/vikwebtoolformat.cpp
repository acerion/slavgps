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
 *
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





WebToolFormat::WebToolFormat()
{
	fprintf(stderr, "%s:%d\n", __PRETTY_FUNCTION__, __LINE__);

	this->url_format_code = NULL;
}





WebToolFormat::WebToolFormat(const char * new_label, const char * new_url_format, const char * new_url_format_code)
{
	fprintf(stderr, "%s:%d, label = %s\n", __PRETTY_FUNCTION__, __LINE__, new_label);

	if (new_label) {
		this->label = strdup(new_label);
	}
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





char * WebToolFormat::get_url_at_position(Window * window, const Coord * coord)
{
	Viewport * viewport = window->get_viewport();

	// Center values
	struct LatLon ll = viewport->get_center()->get_latlon();

	char scenterlat[G_ASCII_DTOSTR_BUF_SIZE];
	char scenterlon[G_ASCII_DTOSTR_BUF_SIZE];
	g_ascii_dtostr(scenterlat, G_ASCII_DTOSTR_BUF_SIZE, ll.lat);
	g_ascii_dtostr(scenterlon, G_ASCII_DTOSTR_BUF_SIZE, ll.lon);

	struct LatLon llpt;
	llpt.lat = 0.0;
	llpt.lon = 0.0;
	if (coord) {
		ll = coord->get_latlon();
	}
	char spointlat[G_ASCII_DTOSTR_BUF_SIZE];
	char spointlon[G_ASCII_DTOSTR_BUF_SIZE];
	g_ascii_dtostr(spointlat, G_ASCII_DTOSTR_BUF_SIZE, llpt.lat);
	g_ascii_dtostr(spointlon, G_ASCII_DTOSTR_BUF_SIZE, llpt.lon);

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
	viewport->get_bbox_strings(&bbox_strings);

	for (int i = 0; i < len; i++) {
		switch (g_ascii_toupper(this->url_format_code[i])) {
		case 'L': values[i] = g_strdup(bbox_strings.sminlon); break;
		case 'R': values[i] = g_strdup(bbox_strings.smaxlon); break;
		case 'B': values[i] = g_strdup(bbox_strings.sminlat); break;
		case 'T': values[i] = g_strdup(bbox_strings.smaxlat); break;
		case 'A': values[i] = g_strdup(scenterlat); break;
		case 'O': values[i] = g_strdup(scenterlon); break;
		case 'Z': values[i] = g_strdup(szoom); break;
		case 'P': values[i] = g_strdup(spointlat); break;
		case 'N': values[i] = g_strdup(spointlon); break;
		default: break;
		}
	}

	char * url = g_strdup_printf(this->url_format, values[0], values[1], values[2], values[3], values[4], values[5], values[6], values[7], values[8]);

	for (int i = 0; i < MAX_NUMBER_CODES; i++) {
		if (values[i] != '\0') {
			free(values[i]);
		}
	}

	fprintf(stderr, "DEBUG: %s %s\n", __PRETTY_FUNCTION__, url);
	return url;
}





char * WebToolFormat::get_url(Window * window)
{
	return this->get_url_at_position(window, NULL);
}
