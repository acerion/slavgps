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




#include "ui_util.h"
#include "globals.h"
#include "webtool.h"




using namespace SlavGPS;




#define SG_MODULE "WebTool"




WebTool::~WebTool()
{
	qDebug() << SG_PREFIX_I << "Deleted tool with label" << this->label;
}




void WebTool::run_at_current_position(Viewport * a_viewport)
{
	const QString url = this->get_url_for_viewport(a_viewport);
	open_url(url);
}




void WebTool::run_at_position(Viewport * a_viewport, const Coord * a_coord)
{
	QString url = this->get_url_at_position(a_viewport, a_coord);
	if (url.size()) {
		open_url(url);
	}
}




void WebTool::set_url_format(const QString & new_url_format)
{
	this->url_format = new_url_format;
}
