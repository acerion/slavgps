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




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "datasource.h"
#include "acquire.h"
#include "geotag_exif.h"
#include "file_utils.h"
#include "util.h"
#include "vikutils.h"
#include "layer_trw.h"
#include "window.h"
#include "statusbar.h"
#include "viewport_internal.h"
#include "datasource_geotag.h"




using namespace SlavGPS;




/* The last used directory. */
static QUrl g_last_directory_url;

/* The last used file filter. */
static QString g_last_filter;




#define DIALOG_MIN_WIDTH 400




#ifdef VIK_CONFIG_GEOTAG
// DataSourceInterface datasource_geotag_interface;
#endif




DataSourceGeoTag::DataSourceGeoTag()
{
	this->window_title = QObject::tr("Create Waypoints from Geotagged Images");
	this->layer_title = QObject::tr("Geotagged Images");
	this->mode = DataSourceMode::AutoLayerManagement;
	this->input_type = DataSourceInputType::None;
	this->autoview = true;
	this->keep_dialog_open = false; /* false = don't keep dialog open after success. We should be able to see the data on the screen so no point in keeping the dialog open. */
	this->is_thread = true;
}




int DataSourceGeoTag::run_config_dialog(AcquireProcess * acquire_context)
{
	DataSourceGeoTagDialog config_dialog(this->window_title);

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

	/* TODO_LATER: Comment from Viking:
	   Could add code to setup a default symbol (see dialog.c for symbol usage).
	   Store in user_data type and then apply when creating the waypoints.
	   However not much point since these will have images associated with them! */

	this->grid->addWidget(this->file_selector, 0, 0);
}




/**
   Process selected files and try to generate waypoints storing them in the given trw.
*/
bool DataSourceGeoTag::acquire_into_layer(LayerTRW * trw, AcquireTool * babel_something)
{
	AcquireProcess * acquiring_context = (AcquireProcess *) babel_something;

	/* Process selected files.
	   In prinicple this loading should be quite fast and so don't need to have any progress monitoring. */
	for (int i = 0; i < this->selected_files.size(); i++) {
		const QString file_full_path = this->selected_files.at(0);

		Waypoint * wp = GeotagExif::create_waypoint_from_file(file_full_path, acquiring_context->viewport->get_coord_mode());
		if (wp) {
			if (wp->name.isEmpty()) {
				/* GeotagExif method doesn't guarantee setting waypoints name. */
				wp->set_name(file_base_name(file_full_path));
			}
			trw->add_waypoint_from_file(wp);
		} else {
			acquiring_context->window->statusbar_update(StatusBarField::INFO, QString("Unable to create waypoint from %1").arg(file_full_path));
		}
	}

	this->selected_files.clear();

	/* No failure. */
	return true;
}
