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




#ifndef _SG_DATASOURCE_OSM_MY_TRACES_H_
#define _SG_DATASOURCE_OSM_MY_TRACES_H_




#include <list>




#include <QLineEdit>




#include "datasource.h"
#include "acquire.h"




namespace SlavGPS {




	class GPXMetaData;




	class DataSourceOSMMyTraces : public DataSource {
	public:
		DataSourceOSMMyTraces() {};
		DataSourceOSMMyTraces(Viewport * viewport);
		~DataSourceOSMMyTraces() {};

		sg_ret acquire_into_layer(LayerTRW * trw, AcquireContext * acquire_context, AcquireProgressDialog * progr_dialog);

		int run_config_dialog(AcquireContext * acquire_context);

		Viewport * viewport = NULL;
	};




	class DataSourceOSMMyTracesDialog : public DataSourceDialog {
	public:
		DataSourceOSMMyTracesDialog(const QString & window_title, Viewport * new_viewport) : DataSourceDialog(window_title) { this->viewport = new_viewport; };

		AcquireOptions * create_acquire_options(AcquireContext * acquire_context);

		void set_in_current_view_property(std::list<GPXMetaData *> & list);

		/* Actual user and password values are stored in oms-traces.c. */
		QLineEdit user_entry;
		QLineEdit password_entry;

		Viewport * viewport = NULL;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_DATASOURCE_OSM_MY_TRACES_H_ */
