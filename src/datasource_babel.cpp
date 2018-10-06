/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2006, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
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

/**
 * SECTION:babel
 * @short_description: running external programs and redirecting to TRWLayers.
 *
 * GPSBabel may not be necessary for everything,
 *  one can use shell_command option but this will be OS platform specific
 */




#include "datasource_babel.h"
#include "babel.h"




using namespace SlavGPS;




#define SG_MODULE "DataSource Babel"




bool DataSourceBabel::acquire_into_layer(LayerTRW * trw, AcquireContext & acquire_context, DataProgressDialog * progr_dialog)
{
	qDebug() << SG_PREFIX_I;
	return this->acquire_options->universal_import_fn(trw, this->download_options, acquire_context, progr_dialog);
}




int DataSourceBabel::kill(const QString & status)
{
	if (this->acquire_options) {
		return this->acquire_options->kill_babel_process(status);
	} else {
		return -4;
	}
}
