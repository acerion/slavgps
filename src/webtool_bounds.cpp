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




#define SG_MODULE "Online Service with Bounds"




OnlineService_bbox::OnlineService_bbox(const QString & new_label, const QString & new_url_format) : OnlineService(new_label)
{
	qDebug() << SG_PREFIX_I << "Created tool with label" << new_label;

	this->label = new_label;
	this->url_format = new_url_format;
}




OnlineService_bbox::~OnlineService_bbox()
{
	qDebug() << SG_PREFIX_I << "Delete tool with label" << this->label;
}




QString OnlineService_bbox::get_url_for_viewport(GisViewport * a_gisview)
{
	return this->get_url_for_bbox(a_gisview->get_bbox());
}




QString OnlineService_bbox::get_url_at_position(GisViewport * a_gisview, const Coord * a_coord)
{
	/* FIXME: online service expects a bbox, but the method
	   provides only a coord.  We could use current zoom of a
	   viewport to generate a bbox around the coord with correct
	   size of the bbox.

	   For now just use viewport's bbox as a simple workaround. */
	return this->get_url_for_bbox(a_gisview->get_bbox());
}




QString OnlineService_bbox::get_url_for_bbox(const LatLonBBox & bbox)
{
	const LatLonBBoxStrings bbox_strings = bbox.values_to_c_strings();

	const QString url = QString(this->url_format).arg(bbox_strings.west).arg(bbox_strings.east).arg(bbox_strings.south).arg(bbox_strings.north);

	qDebug() << SG_PREFIX_I << "URL for bbox is" << url;

	return url;
}
