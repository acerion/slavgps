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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "vikgototool.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>





using namespace SlavGPS;





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





DownloadFileOptions * GotoTool::get_download_options()
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
int GotoTool::get_coord(Window * window, Viewport * viewport, char * srch_str, VikCoord * coord)
{
	int ret = 0;  /* OK */

	fprintf(stderr, "DEBUG: %s: raw goto: %s\n", __FUNCTION__, srch_str);
	char * escaped_srch_str = uri_escape(srch_str);
	fprintf(stderr, "DEBUG: %s: escaped goto: %s\n", __FUNCTION__, escaped_srch_str);

	char * uri = g_strdup_printf(this->get_url_format(), escaped_srch_str);
	char * tmpname = a_download_uri_to_tmp_file(uri, this->get_download_options());
	if (!tmpname) {
		// Some kind of download error, so no tmp file
		ret = 1;
		goto done_no_file;
	}

	fprintf(stderr, "DEBUG: %s: %s\n", __FILE__, tmpname);
	struct LatLon ll;
	if (!this->parse_file_for_latlon(tmpname, &ll)) {
		ret = -1;
		goto done;
	}
	vik_coord_load_from_latlon(coord, viewport->get_coord_mode(), &ll);

 done:
	(void) util_remove(tmpname);
 done_no_file:
	free(tmpname);
	free(escaped_srch_str);
	free(uri);
	return ret;
}
