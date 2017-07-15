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
 */




#include <cstring>
#include <cstdlib>

#include <QDebug>

#include "window.h"
#include "viewport_internal.h"
#include "vikwebtoolbounds.h"
#include "globals.h"




using namespace SlavGPS;




WebToolBounds::WebToolBounds(const QString & new_label, const char * new_url_format) : WebTool(new_label)
{
	qDebug() << "II: Web Tool Bounds created with label" << new_label;

	this->label = new_label;
	this->url_format = strdup(new_url_format);
}




WebToolBounds::~WebToolBounds()
{
	qDebug() << "II: Web Tool Bounds: delete tool with label" << this->label;
}




QString WebToolBounds::get_url_at_current_position(Viewport * a_viewport)
{
	LatLonBBoxStrings bbox_strings;
	a_viewport->get_bbox_strings(bbox_strings);

	char * url = g_strdup_printf(this->url_format, bbox_strings.min_lon.toUtf8().constData(), bbox_strings.max_lon.toUtf8().constData(), bbox_strings.min_lat.toUtf8().constData(), bbox_strings.max_lat.toUtf8().constData());

	QString result(url);
	free(url);

	qDebug() << "II: Web Tool Bounds: url at current position is" << result;

	return result;
}




QString WebToolBounds::get_url_at_position(Viewport * a_viewport, const Coord * a_coord)
{
	/* TODO: could use zoom level to generate an offset from center lat/lon to get the bounds.
	   For now simply use the existing function to use bounds from the viewport. */
	return this->get_url_at_current_position(a_viewport);
}
