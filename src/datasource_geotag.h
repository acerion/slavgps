/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2012-2015, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_DATASOURCE_GEOTAG_H_
#define _SG_DATASOURCE_GEOTAG_H_




#include "acquire.h"
#include "datasource.h"
#include "widget_file_entry.h"




namespace SlavGPS {




	class DataSourceGeoTag : public DataSource {
	public:
		DataSourceGeoTag();
		~DataSourceGeoTag() {};

		bool acquire_into_layer(LayerTRW * trw, AcquireContext & acquire_context, DataProgressDialog * progr_dialog);

		int run_config_dialog(AcquireContext & acquire_context);

	private:
		QStringList selected_files;
	};




	class DataSourceGeoTagDialog : public DataSourceDialog {
	public:
		DataSourceGeoTagDialog(const QString & window_title);

		FileSelectorWidget * file_selector = NULL;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_DATASOURCE_GEOTAG_H_ */
