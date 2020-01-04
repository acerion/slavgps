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




#include <cassert>




#include "datasource.h"
#include "layer_trw_import.h"
#include "geotag_exif.h"
#include "file_utils.h"
#include "util.h"
#include "vikutils.h"
#include "layer_trw.h"
#include "layer_aggregate.h"
#include "window.h"
#include "statusbar.h"
#include "viewport_internal.h"
#include "datasource_geotag.h"




using namespace SlavGPS;




#define SG_MODULE "DataSource Geotag"




/* The last used directory. */
static QUrl g_last_directory_url;

/* The last used file filter. */
static QString g_last_filter;




#define DIALOG_MIN_WIDTH 400




DataSourceGeoTag::DataSourceGeoTag()
{
	this->m_window_title = QObject::tr("Create Waypoints from Geotagged Images");
	this->m_layer_title = QObject::tr("Geotagged Images");
	this->m_layer_mode = TargetLayerMode::AutoLayerManagement;
	this->m_autoview = true;
	this->m_keep_dialog_open_after_success = false;
}




SGObjectTypeID DataSourceGeoTag::get_source_id(void) const
{
	return DataSourceGeoTag::source_id();
}
SGObjectTypeID DataSourceGeoTag::source_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.datasource.geotag");
	return id;
}




int DataSourceGeoTag::run_config_dialog(AcquireContext & acquire_context)
{
	DataSourceGeoTagDialog config_dialog(this->m_window_title);

	const int answer = config_dialog.exec();
	if (answer == QDialog::Accepted) {
		this->selected_files = config_dialog.file_selector->get_selected_files_full_paths();
		g_last_directory_url = config_dialog.file_selector->get_directory_url();
		g_last_filter = config_dialog.file_selector->get_selected_name_filter();
	}

	return answer;
}




DataSourceGeoTagDialog::DataSourceGeoTagDialog(const QString & window_title) : DataSourceDialog(window_title)
{
	/* QFileDialog::ExistingFiles: allow selecting more than one.
	   By default the file selector is created with AcceptMode::AcceptOpen. */
	this->file_selector = new FileSelectorWidget(QFileDialog::Option(0), QFileDialog::ExistingFiles, tr("Select File to Import"), NULL);
	this->file_selector->set_file_type_filter(FileSelectorWidget::FileTypeFilter::JPEG);

	if (g_last_directory_url.isValid()) {
		this->file_selector->set_directory_url(g_last_directory_url);
	}

	if (!g_last_filter.isEmpty()) {
		this->file_selector->select_name_filter(g_last_filter);
	}

	this->setMinimumWidth(DIALOG_MIN_WIDTH);
	this->grid->addWidget(this->file_selector, 0, 0);
}




/**
   Process selected files and try to generate waypoints storing them
   in the given trw

   In prinicple this loading should be quite fast and so don't need to
   have any progress monitoring.
*/
LoadStatus DataSourceGeoTag::acquire_into_layer(LayerTRW * trw, AcquireContext & acquire_context, AcquireProgressDialog * progr_dialog)
{
	for (int i = 0; i < this->selected_files.size(); i++) {
		const QString file_full_path = this->selected_files.at(0);

		qDebug() << SG_PREFIX_I << "Trying to acquire waypoints from" << file_full_path;

		Waypoint * wp = GeotagExif::create_waypoint_from_file(file_full_path, acquire_context.m_gisview->get_coord_mode());
		if (!wp) {
			qDebug() << SG_PREFIX_W << "Failed to create waypoint from file" << file_full_path;
			acquire_context.m_window->statusbar_update(StatusBarField::Info, QObject::tr("Unable to create waypoint from %1").arg(file_full_path));
			continue;
		}

		if (wp->get_name().isEmpty()) {
			/* GeotagExif method doesn't guarantee setting waypoints name. */
			wp->set_name(file_base_name(file_full_path));
		}

		qDebug() << SG_PREFIX_I << "Adding waypoint" << wp->get_name() << "to layer" << trw->get_name();
		trw->add_waypoint(wp);
	}

	this->selected_files.clear();

	/* No failure. */
	return LoadStatus::Code::Success;
}
