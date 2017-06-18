/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2011-2015, Rob Norris <rw_norris@hotmail.com>
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

#include <string.h>
#include <stdlib.h>

#include "window.h"
#include "viewport_internal.h"
#include "vikwebtoolbounds.h"
#include "globals.h"





using namespace SlavGPS;





WebToolBounds::WebToolBounds()
{
	fprintf(stderr, "%s:%d\n", __PRETTY_FUNCTION__, __LINE__);
}





WebToolBounds::WebToolBounds(const char * new_label, const char * new_url_format)
{
	fprintf(stderr, "%s:%d, label = %s\n", __PRETTY_FUNCTION__, __LINE__, new_label);

	this->label = strdup(new_label);
	this->url_format = strdup(new_url_format);
}





WebToolBounds::~WebToolBounds()
{
	fprintf(stderr, "%s:%d, label = %s\n", __PRETTY_FUNCTION__, __LINE__, this->label);
}





char * WebToolBounds::get_url(Window * window)
{
	fprintf(stderr, "%s:%d: called\n", __PRETTY_FUNCTION__, __LINE__);

	Viewport * viewport = window->get_viewport();
	LatLonBBoxStrings bbox_strings;
	viewport->get_bbox_strings(&bbox_strings);

	return g_strdup_printf(this->url_format, bbox_strings.sminlon, bbox_strings.smaxlon, bbox_strings.sminlat, bbox_strings.smaxlat);
}





char * WebToolBounds::get_url_at_position(Window * window, VikCoord * vc)
{
	// TODO: could use zoom level to generate an offset from center lat/lon to get the bounds
	// For now simply use the existing function to use bounds from the viewport
	return this->get_url(window);
}
