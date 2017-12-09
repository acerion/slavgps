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
 *
 */

#include <cstdlib>

#include <QDebug>

#include "viewport_internal.h"
#include "acquire.h"
#include "geonamessearch.h"
#include "util.h"




using namespace SlavGPS;




static bool datasource_wikipedia_process(LayerTRW * trw, ProcessOptions * po, BabelCallback status_cb, AcquireProcess * acquiring, DownloadOptions * unused);




DataSourceInterface datasource_wikipedia_interface = {
	N_("Create Waypoints from Wikipedia Articles"),
	N_("Wikipedia Waypoints"),
	DataSourceMode::AUTO_LAYER_MANAGEMENT,
	DatasourceInputtype::NONE,
	false,
	false, /* false = don't keep dialog open after success. Not even using the dialog. */
	false, /* false = don't run as thread. Own method for getting data - does not fit encapsulation with current thread logic. */

	(DataSourceInitFunc)                  NULL,
	(DataSourceCheckExistenceFunc)        NULL,
	(DataSourceCreateSetupDialogFunc)     NULL,
	(DataSourceGetProcessOptionsFunc)     NULL,
	(DataSourceProcessFunc)               datasource_wikipedia_process,
	(DataSourceProgressFunc)              NULL,
	(DataSourceCreateProgressDialogFunc)  NULL,
	(DataSourceCleanupFunc)               NULL,
	(DataSourceTurnOffFunc)               NULL,

	NULL,
	0,
	NULL,
	NULL,
	0
};




/**
 * Process selected files and try to generate waypoints storing them in the given trw.
 */
static bool datasource_wikipedia_process(LayerTRW * trw, ProcessOptions * po, BabelCallback status_cb, AcquireProcess * acquiring, DownloadOptions * unused)
{
	if (!trw) {
		qDebug() << "EE: Datasource Wikipedia: missing TRW layer";
		return false;
	}

	a_geonames_wikipedia_box(acquiring->window, trw, acquiring->viewport->get_min_max_lat_lon());

	return true;
}
