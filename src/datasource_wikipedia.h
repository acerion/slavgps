/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013-2015, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_DATASOURCE_WIKIPEDIA_H_
#define _SG_DATASOURCE_WIKIPEDIA_H_




#include "layer_trw_import.h"
#include "datasource.h"




namespace SlavGPS {




	class ListSelectionWidget;




	class DataSourceWikipedia : public DataSource {
	public:
		DataSourceWikipedia();
		~DataSourceWikipedia() {};

		/* This data source does not provide configuration dialog. */
		LoadStatus acquire_into_layer(LayerTRW * trw, AcquireContext & acquire_context, AcquireProgressDialog * progr_dialog) override;

		int run_config_dialog(AcquireContext & acquire_context) override;

		AcquireProgressDialog * create_progress_dialog(const QString & title);

		SGObjectTypeID get_source_id(void) const override;
		static SGObjectTypeID source_id(void);

		ListSelectionWidget * list_selection_widget = NULL;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_DATASOURCE_WIKIPEDIA_H_ */
