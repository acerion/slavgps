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




#ifndef _SG_ONLINE_SERVICE_QUERY_H_
#define _SG_ONLINE_SERVICE_QUERY_H_




#include <QObject>




#include "webtool.h"




namespace SlavGPS {




	class OnlineService_query : public OnlineService {
		Q_OBJECT
	public:
		OnlineService_query(const QString & new_tool_name,
				    const QString & url_format,
				    const QString & url_format_code,
				    const QString & file_type,
				    const QString & input_field_label);
		~OnlineService_query();

		void run_at_current_position(GisViewport * gisview);

		QString get_url_for_viewport(GisViewport * gisview) override;
		QString get_url_at_position(GisViewport * gisview, const Coord * coord) override;

		QString get_last_user_string(void) const;



		/* This should be private. */

		bool tool_needs_user_string(void) const;

		QString url_format_code;

		QString file_type;               /* Default value NULL equates to internal GPX reading. */
		QString input_field_label_text;  /* Label to be shown next to the user input field if an input term is required. */
		QString user_string;

	}; /* class OnlineService_query */




}




#endif /* #ifndef _SG_ONLINE_SERVICE_QUERY_H_ */
