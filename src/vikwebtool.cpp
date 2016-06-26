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
 *
 */

#include <string.h>
#include <stdlib.h>

#include "ui_util.h"
#include "vikwebtool.h"
#include "maputils.h"





using namespace SlavGPS;





WebTool::WebTool()
{
	fprintf(stderr, "%s:%d\n", __PRETTY_FUNCTION__, __LINE__);
	this->url_format = NULL;
}





WebTool::~WebTool()
{
	fprintf(stderr, "%s:%d\n", __PRETTY_FUNCTION__, __LINE__);
	if (this->url_format) {
		free(this->url_format);
		this->url_format = NULL;
	}
}





void WebTool::open(Window * window)
{
	char * url = this->get_url(window);
	open_url(GTK_WINDOW(window->vw), url);
	free(url);
}





void WebTool::open_at_position(Window * window, VikCoord * vc)
{
	char * url = this->get_url_at_position(window, vc);
	if (url) {
		open_url(GTK_WINDOW(window->vw), url);
		free(url);
	}
}





void WebTool::set_url_format(char const * new_url_format)
{
	if (new_url_format) {
		free(this->url_format);
		this->url_format = strdup(new_url_format);
	}
	return;
}





uint8_t WebTool::mpp_to_zoom(double mpp)
{
	return map_utils_mpp_to_zoom_level(mpp);
}
