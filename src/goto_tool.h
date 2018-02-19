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

#ifndef _SG_GOTO_TOOL_H
#define _SG_GOTO_TOOL_H




#include <QString>

#include "coord.h"
#include "coords.h"
#include "download.h"




namespace SlavGPS {




	class Viewport;




	enum class GotoToolResult {
		Found,
		NotFound,
		Error
	};




	class GotoTool {

	public:
		GotoTool();
		GotoTool(const QString & new_label);
		~GotoTool();

		virtual QString get_label(void) const;
		virtual char * get_url_format(void) const = 0;
		virtual const DownloadOptions * get_download_options(void) const;
		virtual bool parse_file_for_latlon(const QString & file_full_path, LatLon & lat_lon) = 0;

		GotoToolResult get_coord(Viewport * viewport, const QString & name, Coord * coord);

	protected:

		int id;
		QString label;

		DownloadOptions dl_options;

	}; /* class GotoTool */





} /* namespace SlavGPS */





#endif /* #ifndef _SG_GOTO_TOOL_H */
