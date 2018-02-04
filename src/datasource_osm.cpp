/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2011, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QDebug>

#include "datasource_osm.h"
#include "viewport_internal.h"
#include "babel.h"
#include "gpx.h"
#include "acquire.h"
#include "util.h"
#include "dialog.h"




using namespace SlavGPS;




/**
   See http://wiki.openstreetmap.org/wiki/API_v0.6#GPS_Traces
*/
#define DOWNLOAD_URL_FMT "api.openstreetmap.org/api/0.6/trackpoints?bbox=%1,%2,%3,%4&page=%5"




static int g_last_page_number = 0;




DataSourceOSMTraces::DataSourceOSMTraces(void)
{
	this->window_title = QObject::tr("OSM traces");
	this->layer_title = QObject::tr("OSM traces");
	this->mode = DataSourceMode::AUTO_LAYER_MANAGEMENT;
	this->inputtype = DatasourceInputtype::NONE;
	this->autoview = true;
	this->keep_dialog_open = true;  /* true = keep dialog open after success. */
	this->is_thread = true;         /* true = run as thread. */
}




DataSourceDialog * DataSourceOSMTraces::create_setup_dialog(Viewport * viewport, void * user_data)
{
	return new DataSourceOSMDialog(viewport);
}




ProcessOptions * DataSourceOSMDialog::get_process_options(DownloadOptions & dl_options)
{
	ProcessOptions * po = new ProcessOptions();

	const LatLonBBoxStrings bbox_strings = LatLonBBox::to_strings(this->viewport->get_bbox());

	/* Retrieve the specified page number. */
	const int page = this->spin_box.value();

	/* Download is of GPX type. */
	po->url = QString(DOWNLOAD_URL_FMT).arg(bbox_strings.min_lon).arg(bbox_strings.min_lat).arg(bbox_strings.max_lon).arg(bbox_strings.max_lat).arg(page);
	/* Don't modify dl_options, use the default download settings. */

	qDebug() << "DD: Datasource OSM: URL =" << po->url;

	return po;
}




DataSourceOSMDialog::DataSourceOSMDialog(Viewport * new_viewport)
{
	/* Page selector. */
	QLabel * label = new QLabel(tr("Page Number:"));

	this->spin_box.setMinimum(0);
	this->spin_box.setMaximum(100);
	this->spin_box.setSingleStep(1);
	this->spin_box.setValue(g_last_page_number);

	this->grid->addWidget(label, 0, 0);
	this->grid->addWidget(&this->spin_box, 0, 1);

	connect(this->button_box, &QDialogButtonBox::accepted, this, &DataSourceOSMDialog::accept_cb);

	this->viewport = new_viewport;
}




DataSourceOSMDialog::~DataSourceOSMDialog()
{
}




void DataSourceOSMDialog::accept_cb(void)
{
	g_last_page_number = this->spin_box.value();
	qDebug() << "II: Datasource OSM Traces: dialog result: accepted, page number =" << g_last_page_number;
}
