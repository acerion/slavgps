/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_WEBTOOL_DATASOURCE_H_
#define _SG_WEBTOOL_DATASOURCE_H_




#include <QObject>

#include "vikwebtool.h"




namespace SlavGPS {




	class WebToolDatasource : public WebTool {
		Q_OBJECT
	public:
		WebToolDatasource(const QString & label,
				  const QString & url_format,
				  const QString & url_format_code,
				  const char * file_type,
				  const char * babel_filter_args,
				  const char * input_label);
		~WebToolDatasource();

		void run_at_current_position(Window * a_window);

		QString get_url_at_current_position(Viewport * a_viewport);
		QString get_url_at_position(Viewport * a_viewport, const Coord * a_coord);




		/* This should be private. */

		bool webtool_needs_user_string();

		QString q_url_format;
		QString url_format_code;

		char * file_type = NULL;         /* Default value NULL equates to internal GPX reading. */
		char * babel_filter_args = NULL; /* Command line filter options for gpsbabel. */
		char * input_label = NULL;       /* Label to be shown next to the user input box if an input term is required. */
		char * user_string = NULL;

	}; /* class WebToolDatasource */




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WEBTOOL_DATASOURCE_H_ */
