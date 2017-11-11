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




static bool datasource_wikipedia_process(LayerTRW * trw, ProcessOptions * po, BabelCallback status_cb, AcquireProcess * acquiring);




VikDataSourceInterface vik_datasource_wikipedia_interface = {
	N_("Create Waypoints from Wikipedia Articles"),
	N_("Wikipedia Waypoints"),
	DatasourceMode::AUTO_LAYER_MANAGEMENT,
	DatasourceInputtype::NONE,
	false,
	false, /* Not even using the dialog. */
	false, /* Own method for getting data - does not fit encapsulation with current thread logic. */

	(DataSourceInternalDialog)            NULL,
	(VikDataSourceInitFunc)               NULL,
	(VikDataSourceCheckExistenceFunc)     NULL,
	(DataSourceCreateSetupDialogFunc)     NULL,
	(VikDataSourceGetProcessOptionsFunc)  NULL,
	(VikDataSourceProcessFunc)            datasource_wikipedia_process,
	(VikDataSourceProgressFunc)           NULL,
	(DataSourceCreateProgressDialogFunc)  NULL,
	(VikDataSourceCleanupFunc)            NULL,
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
static bool datasource_wikipedia_process(LayerTRW * trw, ProcessOptions * po, BabelCallback status_cb, AcquireProcess * acquiring)
{
	if (!trw) {
		qDebug() << "EE: Datasource Wikipedia: missing TRW layer";
		return false;
	}

	struct LatLon maxmin[2] = { {0.0,0.0}, {0.0,0.0} };

	/* Note the order is max part first then min part - thus reverse order of use in min_max function:. */
	acquiring->viewport->get_min_max_lat_lon(&maxmin[1].lat, &maxmin[0].lat, &maxmin[1].lon, &maxmin[0].lon);

	a_geonames_wikipedia_box(acquiring->window, trw, maxmin);

	return true;
}
