/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2014-2015, Rob Norris <rw_norris@hotmail.com>
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
#include "babel.h"
#include "geojson.h"
#include "window.h"
#include "util.h"
#include "datasource_geojson.h"
#include "statusbar.h"




using namespace SlavGPS;




/* The last used directory. */
static QUrl g_last_directory_url;

/* The last used file filter. */
static QString g_last_filter;




#define DIALOG_MIN_WIDTH 400




DataSourceGeoJSON::DataSourceGeoJSON()
{
	this->m_window_title = QObject::tr("Acquire from GeoJSON");
	this->m_layer_title = QObject::tr("GeoJSON");
	this->m_layer_mode = TargetLayerMode::AutoLayerManagement;
	this->m_autoview = true;
	this->m_keep_dialog_open_after_success = false;
}




SGObjectTypeID DataSourceGeoJSON::get_source_id(void) const
{
	return DataSourceGeoJSON::source_id();
}
SGObjectTypeID DataSourceGeoJSON::source_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.datasource.geojson");
	return id;
}




int DataSourceGeoJSON::run_config_dialog(__attribute__((unused)) AcquireContext & acquire_context)
{
	DataSourceGeoJSONDialog config_dialog(this->m_window_title);

	const int answer = config_dialog.exec();
	if (answer == QDialog::Accepted) {
		this->selected_files = config_dialog.file_selector->get_selected_files_full_paths();
		g_last_directory_url = config_dialog.file_selector->get_directory_url();
		g_last_filter = config_dialog.file_selector->get_selected_name_filter();
	}

	return answer;
}




DataSourceGeoJSONDialog::DataSourceGeoJSONDialog(const QString & window_title) : DataSourceDialog(window_title)
{
	/* QFileDialog::ExistingFiles: allow selecting more than one.
	   By default the file selector is created with AcceptMode::AcceptOpen. */
	this->file_selector = new FileSelectorWidget(QFileDialog::Option(0), QFileDialog::ExistingFiles, tr("Select File to Import"), NULL);
	this->file_selector->set_file_type_filter(FileSelectorWidget::FileTypeFilter::GeoJSON);

	if (g_last_directory_url.isValid()) {
		this->file_selector->set_directory_url(g_last_directory_url);
	}

	if (!g_last_filter.isEmpty()) {
		this->file_selector->select_name_filter(g_last_filter);
	}

	this->setMinimumWidth(DIALOG_MIN_WIDTH);

	this->grid->addWidget(this->file_selector, 0, 0);

	this->file_selector->setFocus();
}




/**
   Process selected files and try to generate waypoints storing them in the given trw.
*/
LoadStatus DataSourceGeoJSON::acquire_into_layer(AcquireContext & acquire_context, __attribute__((unused)) AcquireProgressDialog * progr_dialog)
{
	/* Process selected files. */
	for (int i = 0; i < this->selected_files.size(); i++) {
		const QString file_full_path = this->selected_files.at(i);

		const QString gpx_filename = geojson_import_to_gpx(file_full_path);
		if (!gpx_filename.isEmpty()) {
			/* Important that this process is run in the main thread. */
			acquire_context.get_window()->open_file(gpx_filename, false);
			/* Delete the temporary file. */
			QDir::root().remove(gpx_filename);
		} else {
			acquire_context.get_window()->statusbar_update(StatusBarField::Info, QObject::tr("Unable to import from: %1").arg(file_full_path));
		}
	}

	this->selected_files.clear();

	/* No failure. */
	return LoadStatus::Code::Success;
}
