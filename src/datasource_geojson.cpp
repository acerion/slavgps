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

#include "datasource.h"
#include "acquire.h"
#include "babel.h"
#include "geojson.h"
#include "window.h"
#include "util.h"
#include "datasource_geojson.h"




using namespace SlavGPS;




/* The last used directory. */
static QUrl g_last_directory_url;

/* The last used file filter. */
/* TODO: verify how this overlaps with babel_dialog.cpp:g_last_file_type_index. */
// static QString g_last_filter;




// DataSourceInterface datasource_geojson_interface;




DataSourceGeoJSON::DataSourceGeoJSON()
{
	this->window_title = QObject::tr("Acquire from GeoJSON");
	this->layer_title = QObject::tr("GeoJSON");
	this->mode = DataSourceMode::AUTO_LAYER_MANAGEMENT;
	this->inputtype = DatasourceInputtype::NONE;
	this->autoview = true;
	this->keep_dialog_open = false; /* false = don't keep dialog open after success. We should be able to see the data on the screen so no point in keeping the dialog open. */
	this->is_thread = false; /* false = don't run as thread. Open each file in the main loop. */
}




DataSourceDialog * DataSourceGeoJSON::create_setup_dialog(Viewport * viewport, void * user_data)
{
	return new DataSourceGeoJSONDialog();
}




DataSourceGeoJSONDialog::DataSourceGeoJSONDialog()
{
	/* QFileDialog::ExistingFiles: allow selecting more than one.
	   By default the file selector is created with AcceptMode::AcceptOpen. */
	this->file_entry = new SGFileEntry(QFileDialog::Option(0), QFileDialog::ExistingFiles, SGFileTypeFilter::ANY, tr("Select File to Import"), NULL);

	if (g_last_directory_url.isValid()) {
		this->file_entry->file_selector->setDirectoryUrl(g_last_directory_url);
	}
#ifdef K
	if (!g_last_filter.isEmpty()) {
		this->file_entry->file_selector->selectNameFilter(g_last_filter);
	}
#endif

	const QString filter1 = QObject::tr("GeoJSON (*.geojson)");
	const QString filter2 = QObject::tr("All (*)");
	const QStringList filters = { filter1, filter2 };
	this->file_entry->file_selector->setNameFilters(filters);
	this->file_entry->file_selector->selectNameFilter(filter1); /* Default to geojson. */

	this->setMinimumWidth(400); /* TODO: perhaps this value should be #defined somewhere. */

	this->grid->addWidget(this->file_entry, 0, 0);

	this->file_entry->setFocus();
}




ProcessOptions * DataSourceGeoJSONDialog::get_process_options(DownloadOptions & dl_options)
{
	ProcessOptions * po = new ProcessOptions();

	this->selected_files = this->file_entry->file_selector->selectedFiles();

	g_last_directory_url = this->file_entry->file_selector->directoryUrl();

#ifdef K /* TODO Memorize the file filter for later reuse? */
	g_last_filter = this->file_entry->file_selector->selectedNameFilter();
#endif

	/* Return some value so *thread* processing will continue. */
	po->babel_args = "fake command"; /* Not really used, thus no translations. */

	return po;
}




/**
   Process selected files and try to generate waypoints storing them in the given trw.
*/
bool DataSourceGeoJSON::process_func(LayerTRW * trw, ProcessOptions * process_options, BabelCallback status_cb, AcquireProcess * acquiring, DownloadOptions * unused)
{
	DataSourceGeoJSONDialog * config_dialog = (DataSourceGeoJSONDialog *) acquiring->user_data;

	/* Process selected files. */
	for (int i = 0; i < config_dialog->selected_files.size(); i++) {
		const QString file_full_path = config_dialog->selected_files.at(i);

		char * gpx_filename = geojson_import_to_gpx(file_full_path);
		if (gpx_filename) {
			/* Important that this process is run in the main thread. */
			acquiring->window->open_file(gpx_filename, false);
			/* Delete the temporary file. */
			(void) remove(gpx_filename);
			free(gpx_filename);
		} else {
			acquiring->window->statusbar_update(StatusBarField::INFO, QString("Unable to import from: %1").arg(file_full_path));
		}
	}

	config_dialog->selected_files.clear();

	/* No failure. */
	return true;
}
