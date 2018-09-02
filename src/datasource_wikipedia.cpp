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




#include <QDebug>




#include "viewport_internal.h"
#include "acquire.h"
#include "geonames_search.h"
#include "util.h"
#include "datasource_wikipedia.h"
#include "globals.h"




using namespace SlavGPS;




#define SG_MODULE "DataSource Wikipedia"




class AcquireOptionsWikipedia : public AcquireOptions {
public:
	bool is_valid(void) const { return true; }
};




DataSourceWikipedia::DataSourceWikipedia()
{
	this->window_title = QObject::tr("Create Waypoints from Wikipedia Articles");
	this->layer_title = QObject::tr("Wikipedia Waypoints");
	this->mode = DataSourceMode::AutoLayerManagement;
	this->input_type = DataSourceInputType::None;
	this->autoview = false;
	this->keep_dialog_open = false; /* false = don't keep dialog open after success. Not even using the dialog. */
	this->is_thread = false; /* false = don't run as thread. Own method for getting data - does not fit encapsulation with current thread logic. */
}




/**
   Process selected files and try to generate waypoints storing them in the given trw.
*/
bool DataSourceWikipedia::acquire_into_layer(LayerTRW * trw, AcquireTool * babel_something)
{
	AcquireProcess * acquiring_context = (AcquireProcess *) babel_something;

	if (!trw) {
		qDebug() << SG_PREFIX_E << "Missing TRW layer";
		return false;
	}

	a_geonames_wikipedia_box(acquiring_context->window, trw, acquiring_context->viewport->get_min_max_lat_lon());

	return true;
}





int DataSourceWikipedia::run_config_dialog(AcquireProcess * acquire_context)
{
	/* Fake acquire options, needed by current implementation of acquire.cpp. */
	this->acquire_options = new AcquireOptionsWikipedia;

	return QDialog::Accepted;
}
