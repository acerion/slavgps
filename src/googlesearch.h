/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Quy Tonthat <qtonthat@gmail.com>
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

#ifndef _SG_GOTO_TOOL_GOOGLE_H_
#define _SG_GOTO_TOOL_GOOGLE_H_




#include "goto_tool.h"




namespace SlavGPS {




	class GotoToolGoogle : public GotoTool {

	public:
		GotoToolGoogle();
		~GotoToolGoogle();

		bool parse_file_for_latlon(char * file_name, struct LatLon * ll);
		char * get_url_format();
		DownloadOptions * get_download_options();

	}; /* class GotoToolGoogle */




} /* namespace SlavGPS */




#endif /* #ifndef _SG_GOTO_TOOL_GOOGLE_H_ */
