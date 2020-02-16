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
#include "layer_trw_import.h"




namespace SlavGPS {




	class GPXMetaData;




	class DataSourceOSMMyTraces : public DataSource {
	public:
		DataSourceOSMMyTraces(GisViewport * gisview);
		~DataSourceOSMMyTraces() {};

		LoadStatus acquire_into_layer(AcquireContext & acquire_context, AcquireProgressDialog * progr_dialog) override;

		int run_config_dialog(AcquireContext & acquire_context) override;

		SGObjectTypeID get_source_id(void) const override;
		static SGObjectTypeID source_id(void);

	private:
		void update_tracks_metadata_property(std::list<GPXMetaData *> & tracks_metadata);

		GisViewport * m_gisview = nullptr;
	};




	/* Dialog used to specify (configure) parameters of accessing
	   "My OSM traces" feature of OSM. */
	class DataSourceOSMMyTracesConfigDialog : public DataSourceDialog {
	public:
		DataSourceOSMMyTracesConfigDialog(const QString & window_title);

		AcquireOptions * create_acquire_options(AcquireContext & acquire_context) override;

	private:
		/* Actual user and password values are stored in oms-traces.c. */
		QLineEdit m_user_name_entry;
		QLineEdit m_password_entry;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_DATASOURCE_OSM_MY_TRACES_H_ */
