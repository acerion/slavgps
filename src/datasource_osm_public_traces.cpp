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




#include "datasource_osm_public_traces.h"
#include "viewport_internal.h"
#include "babel.h"
#include "gpx.h"
#include "layer_trw_import.h"
#include "util.h"
#include "dialog.h"




using namespace SlavGPS;




#define SG_MODULE "DataSource OSM Public Traces"




/**
   https://wiki.openstreetmap.org/wiki/API_v0.6#URL_.2B_authentication
   https://wiki.openstreetmap.org/wiki/API_v0.6#GPS_traces
*/
#define DOWNLOAD_URL_FMT "https://api.openstreetmap.org/api/0.6/trackpoints?bbox=%1,%2,%3,%4&page=%5"




/* "specifies which group of 5,000 points, or page, to return" */
static int g_last_page_number = 0;




DataSourceOSMPublicTraces::DataSourceOSMPublicTraces()
{
	this->m_window_title = QObject::tr("OSM Public Traces");
	this->m_layer_title = QObject::tr("OSM Public Traces");
	this->m_layer_mode = TargetLayerMode::AutoLayerManagement;
	this->m_autoview = true;
	this->m_keep_dialog_open_after_success = true;
}




SGObjectTypeID DataSourceOSMPublicTraces::get_source_id(void) const
{
	return DataSourceOSMPublicTraces::source_id();
}
SGObjectTypeID DataSourceOSMPublicTraces::source_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.datasource.osm_public_traces");
	return id;
}




int DataSourceOSMPublicTraces::run_config_dialog(AcquireContext & acquire_context)
{
	DataSourceOSMPublicTracesConfigDialog config_dialog(this->m_window_title);

	const int answer = config_dialog.exec();
	if (answer == QDialog::Accepted) {
		this->m_acquire_options = config_dialog.create_acquire_options(acquire_context);
		this->m_download_options = new DownloadOptions; /* With default values. */
	}

	return answer;
}




AcquireOptions * DataSourceOSMPublicTracesConfigDialog::create_acquire_options(AcquireContext & acquire_context)
{
	AcquireOptions * babel_options = new AcquireOptions(AcquireOptions::Mode::FromURL);

	const LatLonBBoxStrings bbox_strings = acquire_context.get_gisview()->get_bbox().values_to_c_strings();

	const int page_number = this->m_page_number.value();

	babel_options->source_url = QString(DOWNLOAD_URL_FMT)
		.arg(bbox_strings.west)
		.arg(bbox_strings.south)
		.arg(bbox_strings.east)
		.arg(bbox_strings.north)
		.arg(page_number);
	/* Don't modify dl_options, use the default download settings. */

	qDebug() << SG_PREFIX_D << "Source URL =" << babel_options->source_url;

	return babel_options;
}




DataSourceOSMPublicTracesConfigDialog::DataSourceOSMPublicTracesConfigDialog(const QString & window_title) : DataSourceDialog(window_title)
{
	QLabel * label = new QLabel(tr("Page Number:"));

	this->m_page_number.setMinimum(0);
	this->m_page_number.setMaximum(100);
	this->m_page_number.setSingleStep(1);
	this->m_page_number.setValue(g_last_page_number);
	this->m_page_number.setToolTip(tr("Specifies which group of 5000 points, or 'page', to download."));

	this->grid->addWidget(label, 0, 0);
	this->grid->addWidget(&this->m_page_number, 0, 1);

	connect(this->button_box, &QDialogButtonBox::accepted, this, &DataSourceOSMPublicTracesConfigDialog::accept_cb);
}




DataSourceOSMPublicTracesConfigDialog::~DataSourceOSMPublicTracesConfigDialog()
{
}




void DataSourceOSMPublicTracesConfigDialog::accept_cb(void)
{
	g_last_page_number = this->m_page_number.value();
	qDebug() << SG_PREFIX_I << "Dialog result: accepted, page number =" << g_last_page_number;
}
