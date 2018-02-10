/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstring>
#include <cstdlib>

#include <glib.h>
#include <glib/gstdio.h>




#include <QDebug>




#include "goto_tool.h"
#include "util.h"
#include "viewport_internal.h"




using namespace SlavGPS;




#define PREFIX " GoTo Tool:" << __FUNCTION__ << __LINE__ << ">"




GotoTool::GotoTool()
{
	this->id = 0;
	this->label = NULL;
}





GotoTool::GotoTool(char const * new_label)
{
	if (new_label) {
		this->label = strdup(new_label);
	} else {
		this->label = strdup("<no-set>");
	}
}





GotoTool::~GotoTool()
{
	if (this->label) {
		free(this->label);
		this->label = NULL;
	}
}





char * GotoTool::get_label()
{
	return this->label ? strdup(this->label) : NULL;
}





const DownloadOptions * GotoTool::get_download_options(void) const
{
	// Default: return NULL
	return NULL;
}





/**
 * vik_goto_tool_get_coord:
 *
 * @viewport:  The #Viewport
 * @srch_str:  The string to search with
 * @coord:     Returns the top match position for a successful search
 *
 * Returns: An integer value indicating:
 *  0  = search found something
 *  -1 = search place not found by the #GotoTool
 *  1  = search unavailable in the #GotoTool due to communication issue
 *
 */
GotoToolResult GotoTool::get_coord(Viewport * viewport, const QString & name, Coord * coord)
{
	GotoToolResult ret = GotoToolResult::Error;
	LatLon lat_lon;

	qDebug() << "DD" PREFIX << "raw goto name:" << name;
	char * escaped_srch_str = uri_escape(name.toUtf8().constData());
	qDebug() << "DD" PREFIX << "escaped goto name:" << escaped_srch_str;

	char * uri = g_strdup_printf(this->get_url_format(), escaped_srch_str);
	const QString tmp_file_full_path = Download::get_uri_to_tmp_file(QString(uri), this->get_download_options());
	if (tmp_file_full_path.isEmpty()) {
		/* Some kind of download error, so no tmp file. */
		ret = GotoToolResult::Error;
		goto done_no_file;
	}

	qDebug() << "DD" PREFIX << "temporary file:" << tmp_file_full_path;

	if (this->parse_file_for_latlon(tmp_file_full_path, lat_lon)) {
		*coord = Coord(lat_lon, viewport->get_coord_mode());
		ret = GotoToolResult::Found;
	} else {
		ret = GotoToolResult::NotFound;
	}
	util_remove(tmp_file_full_path);

 done_no_file:
	free(escaped_srch_str);
	free(uri);
	return ret;
}
