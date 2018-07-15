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
/* TODO: verify how this overlaps with babel_dialog.cpp:g_last_file_type_index. */
// static QString g_last_filter;




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




int DataSourceGeoJSON::run_config_dialog(AcquireProcess * acquire_context)
{
	assert (!this->config_dialog);

	DataSourceGeoJSONDialog * dialog = new DataSourceGeoJSONDialog(this->window_title);

	int answer = dialog->exec();
	if (answer == QDialog::Accepted) {
		this->selected_files = dialog->file_entry->get_selected_files_full_paths();
		g_last_directory_url = dialog->file_entry->get_directory_url();

#ifdef K_TODO
		/* TODO Memorize the file filter for later reuse? */
		g_last_filter = dialog->file_entry->get_selected_name_filter();
#endif
	}

	this->config_dialog = dialog;

	return answer;
}




DataSourceGeoJSONDialog::DataSourceGeoJSONDialog(const QString & window_title) : DataSourceDialog(window_title)
{
	/* QFileDialog::ExistingFiles: allow selecting more than one.
	   By default the file selector is created with AcceptMode::AcceptOpen. */
	this->file_entry = new SGFileEntry(QFileDialog::Option(0), QFileDialog::ExistingFiles, SGFileTypeFilter::ANY, tr("Select File to Import"), NULL);

	if (g_last_directory_url.isValid()) {
		this->file_entry->set_directory_url(g_last_directory_url);
	}
#ifdef K_TODO
	if (!g_last_filter.isEmpty()) {
		this->file_entry->select_name_filter(g_last_filter);
	}
#endif

	const QString filter1 = QObject::tr("GeoJSON (*.geojson)");
	const QString filter2 = QObject::tr("All (*)");
	const QStringList filters = { filter1, filter2 };
	this->file_entry->set_name_filters(filters);
	this->file_entry->select_name_filter(filter1); /* Default to geojson. */

	this->setMinimumWidth(DIALOG_MIN_WIDTH);

	this->grid->addWidget(this->file_entry, 0, 0);

	this->file_entry->setFocus();
}




/**
   Process selected files and try to generate waypoints storing them in the given trw.
*/
bool DataSourceGeoJSON::acquire_into_layer(LayerTRW * trw, AcquireTool * babel_something)
{
	AcquireProcess * acquiring_context = (AcquireProcess *) babel_something;

	/* Process selected files. */
	for (int i = 0; i < this->selected_files.size(); i++) {
		const QString file_full_path = this->selected_files.at(i);

		const QString gpx_filename = geojson_import_to_gpx(file_full_path);
		if (!gpx_filename.isEmpty()) {
			/* Important that this process is run in the main thread. */
			acquiring_context->window->open_file(gpx_filename, false);
			/* Delete the temporary file. */
			QDir::root().remove(gpx_filename);
		} else {
			acquiring_context->window->statusbar_update(StatusBarField::INFO, QString("Unable to import from: %1").arg(file_full_path));
		}
	}

	this->selected_files.clear();

	/* No failure. */
	return true;
}
