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




#include <cstring>
#include <cstdlib>




#include <QDebug>




#include "goto_tool.h"
#include "util.h"
#include "viewport_internal.h"




using namespace SlavGPS;




#define SG_MODULE "GoTo Tool"




GotoTool::GotoTool()
{
	this->id = 0;
}





GotoTool::GotoTool(const QString & new_label)
{
	this->label = new_label;
}





GotoTool::~GotoTool()
{
}





QString GotoTool::get_label(void) const
{
	return this->label;
}





const DownloadOptions * GotoTool::get_download_options(void) const
{
	// Default: return NULL
	return NULL;
}





/**
 * vik_goto_tool_get_coord:
 *
 * @viewport:  The #GisViewport
 * @srch_str:  The string to search with
 * @coord:     Returns the top match position for a successful search
 *
 * Returns: An integer value indicating:
 *  0  = search found something
 *  -1 = search place not found by the #GotoTool
 *  1  = search unavailable in the #GotoTool due to communication issue
 *
 */
GotoToolResult GotoTool::get_coord(GisViewport * gisview, const QString & name, Coord * coord)
{
	GotoToolResult ret = GotoToolResult::Error;
	LatLon lat_lon;

	qDebug() << SG_PREFIX_D << "Raw goto name:" << name;
	const QString escaped_name = Util::uri_escape(name);
	qDebug() << SG_PREFIX_D << "Escaped goto name:" << escaped_name;

	const QString uri = QString(this->get_url_format()).arg(escaped_name);
	DownloadHandle dl_handle(this->get_download_options());
	QTemporaryFile tmp_file;
	tmp_file.setAutoRemove(false);
	if (!dl_handle.download_to_tmp_file(tmp_file, uri)) {
		/* Some kind of download error, so no tmp file. */
		ret = GotoToolResult::Error;
		goto done_no_file;
	}

	qDebug() << SG_PREFIX_D << "Temporary file:" << tmp_file.fileName();

	if (this->parse_file_for_latlon(tmp_file, lat_lon)) {
		*coord = Coord(lat_lon, gisview->get_coord_mode());
		ret = GotoToolResult::Found;
	} else {
		ret = GotoToolResult::NotFound;
	}
	Util::remove(tmp_file);

 done_no_file:
	return ret;
}
