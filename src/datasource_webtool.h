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




#ifndef _SG_DATASOURCE_ONLINE_SERVICE_H_
#define _SG_DATASOURCE_ONLINE_SERVICE_H_




#include <QObject>
#include <QLineEdit>




#include "babel.h"
#include "datasource.h"
#include "webtool.h"
#include "datasource_babel.h"




namespace SlavGPS {




	class OnlineService_query;




	class DataSourceOnlineService : public DataSourceBabel {
	public:
		DataSourceOnlineService() {};
		DataSourceOnlineService(const QString & window_title, const QString & layer_title, GisViewport * gisview, OnlineService_query * online_service);
		int run_config_dialog(AcquireContext * acquire_context);

		void cleanup(void * data);

		SGObjectTypeID get_source_id(void) const override;
		static SGObjectTypeID source_id(void);

		GisViewport * gisview = NULL;
		OnlineService_query * online_service = NULL;
	};




	/* Only for those online services that require some search
	   term from user. I guesstimate that most online services
	   don't require the term and only need coordinates. */
	class DataSourceOnlineServiceDialog : public DataSourceDialog {
		Q_OBJECT
	public:
		/* Get data from online data source */
		DataSourceOnlineServiceDialog(const QString & window_title, GisViewport * gisview, OnlineService_query * new_online_service);

		AcquireOptions * create_acquire_options(AcquireContext * acquire_context);

		OnlineService_query * online_service = NULL;
		GisViewport * gisview = NULL;
		QLineEdit input_field;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_DATASOURCE_ONLINE_SERVICE_H_ */
