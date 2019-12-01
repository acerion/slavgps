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
 */




#include <cassert>




#include <QDebug>




#include "datasource_osm.h"
#include "viewport_internal.h"
#include "babel.h"
#include "gpx.h"
#include "acquire.h"
#include "util.h"
#include "dialog.h"




using namespace SlavGPS;




#define SG_MODULE "DataSource OSM"




/**
   See http://wiki.openstreetmap.org/wiki/API_v0.6#GPS_Traces
*/
#define DOWNLOAD_URL_FMT "api.openstreetmap.org/api/0.6/trackpoints?bbox=%1,%2,%3,%4&page=%5"




static int g_last_page_number = 0;




DataSourceOSMTraces::DataSourceOSMTraces(GisViewport * new_gisview)
{
	this->gisview = new_gisview;

	this->window_title = QObject::tr("OSM traces");
	this->layer_title = QObject::tr("OSM traces");
	this->mode = DataSourceMode::AutoLayerManagement;
	this->input_type = DataSourceInputType::None;
	this->autoview = true;
	this->keep_dialog_open = true;  /* true = keep dialog open after success. */
}




SGObjectTypeID DataSourceOSMTraces::get_source_id(void) const
{
	return DataSourceOSMTraces::source_id();
}
SGObjectTypeID DataSourceOSMTraces::source_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.datasource.osm_traces");
	return id;
}




int DataSourceOSMTraces::run_config_dialog(AcquireContext * acquire_context)
{
	DataSourceOSMTracesDialog config_dialog(this->window_title, this->gisview);

	const int answer = config_dialog.exec();
	if (answer == QDialog::Accepted) {
		this->acquire_options = config_dialog.create_acquire_options(acquire_context);
		this->download_options = new DownloadOptions; /* With default values. */
	}

	return answer;
}




AcquireOptions * DataSourceOSMTracesDialog::create_acquire_options(AcquireContext * acquire_context)
{
	AcquireOptions * babel_options = new AcquireOptions(AcquireOptions::Mode::FromURL);

	const LatLonBBoxStrings bbox_strings = this->gisview->get_bbox().values_to_c_strings();

	/* Retrieve the specified page number. */
	const int page = this->spin_box.value();

	/* Download is of GPX type. */
	babel_options->source_url = QString(DOWNLOAD_URL_FMT).arg(bbox_strings.west).arg(bbox_strings.south).arg(bbox_strings.east).arg(bbox_strings.north).arg(page);
	/* Don't modify dl_options, use the default download settings. */

	qDebug() << SG_PREFIX_D << "Source URL =" << babel_options->source_url;

	return babel_options;
}




DataSourceOSMTracesDialog::DataSourceOSMTracesDialog(const QString & window_title, GisViewport * new_gisview) : DataSourceDialog(window_title)
{
	/* Page selector. */
	QLabel * label = new QLabel(tr("Page Number:"));

	this->spin_box.setMinimum(0);
	this->spin_box.setMaximum(100);
	this->spin_box.setSingleStep(1);
	this->spin_box.setValue(g_last_page_number);

	this->grid->addWidget(label, 0, 0);
	this->grid->addWidget(&this->spin_box, 0, 1);

	connect(this->button_box, &QDialogButtonBox::accepted, this, &DataSourceOSMTracesDialog::accept_cb);

	this->gisview = new_gisview;
}




DataSourceOSMTracesDialog::~DataSourceOSMTracesDialog()
{
}




void DataSourceOSMTracesDialog::accept_cb(void)
{
	g_last_page_number = this->spin_box.value();
	qDebug() << "II: Datasource OSM Traces: dialog result: accepted, page number =" << g_last_page_number;
}
