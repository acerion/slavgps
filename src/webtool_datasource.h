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
#include <QLineEdit>




#include "babel.h"
#include "datasource.h"
#include "webtool.h"
#include "datasource_babel.h"




namespace SlavGPS {




	class WebToolDatasource;




	class DataSourceWebTool : public DataSourceBabel {
	public:
		DataSourceWebTool() {};
		DataSourceWebTool(bool search, const QString & window_title, const QString & layer_title, Viewport * viewport, WebToolDatasource * web_tool_data_source);
		int run_config_dialog(AcquireContext & acquire_context);

		void cleanup(void * data);

		bool search = false;
		Viewport * viewport = NULL;
		WebToolDatasource * web_tool_data_source = NULL;
	};




	class WebToolDatasource : public WebTool {
		Q_OBJECT
	public:
		WebToolDatasource(const QString & new_tool_name,
				  const QString & url_format,
				  const QString & url_format_code,
				  const QString & file_type,
				  const QString & new_input_field_label);
		~WebToolDatasource();

		void run_at_current_position(Window * window);

		QString get_url_at_current_position(Viewport * viewport);
		QString get_url_at_position(Viewport * viewport, const Coord * a_coord);




		/* This should be private. */

		bool webtool_needs_user_string();

		QString url_format_code;

		QString file_type;               /* Default value NULL equates to internal GPX reading. */
		QString input_field_label_text;  /* Label to be shown next to the user input field if an input term is required. */
		QString user_string;

	}; /* class WebToolDatasource */




	class DataSourceWebToolDialog : public DataSourceDialog {
		Q_OBJECT
	public:
		DataSourceWebToolDialog(const QString & window_title, Viewport * viewport, WebToolDatasource * new_web_tool_data_source);

		AcquireOptions * get_acquire_options_none(void);

		WebToolDatasource * web_tool_data_source = NULL;
		Viewport * viewport = NULL;
		QLineEdit input_field;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WEBTOOL_DATASOURCE_H_ */
