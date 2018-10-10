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
#include "acquire.h"
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
	this->window_title = QObject::tr("Acquire from GeoJSON");
	this->layer_title = QObject::tr("GeoJSON");
	this->mode = DataSourceMode::AutoLayerManagement;
	this->input_type = DataSourceInputType::None;
	this->autoview = true;
	this->keep_dialog_open = false; /* false = don't keep dialog open after success. We should be able to see the data on the screen so no point in keeping the dialog open. */
	this->is_thread = false; /* false = don't run as thread. Open each file in the main loop. */
}




int DataSourceGeoJSON::run_config_dialog(AcquireContext & acquire_context)
{
	DataSourceGeoJSONDialog config_dialog(this->window_title);

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
sg_ret DataSourceGeoJSON::acquire_into_layer(LayerTRW * trw, AcquireContext & acquire_context, AcquireProgressDialog * progr_dialog)
{
	/* Process selected files. */
	for (int i = 0; i < this->selected_files.size(); i++) {
		const QString file_full_path = this->selected_files.at(i);

		const QString gpx_filename = geojson_import_to_gpx(file_full_path);
		if (!gpx_filename.isEmpty()) {
			/* Important that this process is run in the main thread. */
			acquire_context.window->open_file(gpx_filename, false);
			/* Delete the temporary file. */
			QDir::root().remove(gpx_filename);
		} else {
			acquire_context.window->statusbar_update(StatusBarField::Info, QString("Unable to import from: %1").arg(file_full_path));
		}
	}

	this->selected_files.clear();

	/* No failure. */
	return sg_ret::ok;
}
