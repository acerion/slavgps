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

#include <cstring>
#include <cstdlib>

#include <QDebug>

#include "ui_util.h"
#include "vikwebtool.h"
#include "map_utils.h"
#include "window.h"




using namespace SlavGPS;



#if 0
WebTool::WebTool()
{
	qDebug() << "II: Web Tool created";
}
#endif



WebTool::~WebTool()
{
	qDebug() << "II: Web Tool deleted with label" << this->label;
	if (this->url_format) {
		free(this->url_format);
		this->url_format = NULL;
	}
}




void WebTool::run_at_current_position(Window * window)
{
	const QString url = this->get_url_at_current_position(window);
	open_url(url.toUtf8().constData());
}




void WebTool::run_at_position(Window * window, const Coord * coord)
{
	QString url = this->get_url_at_position(window, coord);
	if (url.size()) {
		open_url(url.toUtf8().constData());
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




uint8_t WebTool::mpp_to_zoom_level(double mpp)
{
	return map_utils_mpp_to_zoom_level(mpp);
}




void WebTool::run_at_current_position_cb(void)
{
	QAction * qa = (QAction *) QObject::sender();
	Window * window = (Window *) qa->data().toULongLong();
	this->run_at_current_position(window);
}
