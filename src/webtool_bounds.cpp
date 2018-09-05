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




#include <QDebug>




#include "window.h"
#include "viewport_internal.h"
#include "webtool_bounds.h"




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
	const LatLonBBoxStrings bbox_strings = a_viewport->get_bbox().to_strings();

	const QString url = QString(this->url_format).arg(bbox_strings.west).arg(bbox_strings.east).arg(bbox_strings.south).arg(bbox_strings.north);

	qDebug() << "II: Web Tool Bounds: url at current position is" << url;

	return url;
}




QString WebToolBounds::get_url_at_position(Viewport * a_viewport, const Coord * a_coord)
{
	/* TODO_2_LATER: could use zoom level to generate an offset from center lat/lon to get the bounds.
	   For now simply use the existing function to use bounds from the viewport. */
	return this->get_url_at_current_position(a_viewport);
}
