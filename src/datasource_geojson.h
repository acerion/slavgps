/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2014-2015, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_DATASOURCE_GEOJSON_H_
#define _SG_DATASOURCE_GEOJSON_H_




#include "acquire.h"
#include "datasource.h"
#include "widget_file_entry.h"




namespace SlavGPS {




	class DataSourceGeoJSON : public DataSource {
	public:
		DataSourceGeoJSON();
		~DataSourceGeoJSON() {};

		bool acquire_into_layer(LayerTRW * trw, AcquireTool * babel_something);

		int run_config_dialog(AcquireProcess * acquire_context);
	private:
		QStringList selected_files;
	};




	class DataSourceGeoJSONDialog : public DataSourceDialog {
	public:
		DataSourceGeoJSONDialog(const QString & window_title);

		FileSelector * file_entry = NULL;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_DATASOURCE_GEOJSON_H_ */
