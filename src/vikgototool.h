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
#ifndef _SG_GOTO_TOOL_H
#define _SG_GOTO_TOOL_H





#include <stdint.h>

#include "coords.h"
#include "viewport.h"
#include "download.h"





namespace SlavGPS {





	class GotoTool {

	public:
		GotoTool();
		GotoTool(char const * label);
		~GotoTool();

		virtual char * get_label();
		virtual char * get_url_format() = 0;
		virtual DownloadFileOptions * get_download_options();
		virtual bool parse_file_for_latlon(char * filename, struct LatLon * ll) = 0;

		int get_coord(Viewport * viewport, char * srch_str, VikCoord * coord);

	protected:

		int id;
		char * label;

	}; /* class GotoTool */





} /* namespace SlavGPS */





#endif /* #ifndef _SG_GOTO_TOOL_H */
