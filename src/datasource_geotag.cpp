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
 *
 */
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
#include "widget_file_entry.h"




using namespace SlavGPS;




class DataSourceGeoTagDialog : public DataSourceDialog {
public:
	DataSourceGeoTagDialog();

	ProcessOptions * get_process_options(DownloadOptions & dl_options);

	SGFileEntry * file_entry = NULL;
	QStringList selected_files;
};




/* The last used directory. */
static QUrl g_last_directory_url;

/* The last used file filter. */
/* TODO: verify how this overlaps with babel_dialog.cpp:g_last_file_type_index. */
// static QString g_last_filter;





static DataSourceDialog * datasource_geotag_create_setup_dialog(Viewport * viewport, void * user_data);
static bool datasource_geotag_process(LayerTRW * trw, ProcessOptions * po, BabelCallback status_cb, AcquireProcess * acquiring, DownloadOptions * not_used);




DataSourceInterface datasource_geotag_interface = {
	N_("Create Waypoints from Geotagged Images"),
	N_("Geotagged Images"),
	DataSourceMode::AUTO_LAYER_MANAGEMENT,
	DatasourceInputtype::NONE,
	true,
	false, /* false = don't keep dialog open after success. We should be able to see the data on the screen so no point in keeping the dialog open. */
	true,  /* true = run as thread. */

	(DataSourceInitFunc)		      NULL,
	(DataSourceCreateSetupDialogFunc)     datasource_geotag_create_setup_dialog,
	(DataSourceGetProcessOptionsFunc)     NULL,
	(DataSourceProcessFunc)               datasource_geotag_process,
	(DataSourceProgressFunc)              NULL,
	(DataSourceCreateProgressDialogFunc)  NULL,
	(DataSourceCleanupFunc)               NULL,
	(DataSourceTurnOffFunc)               NULL,

	NULL,
	0,
	NULL,
	NULL,
	0
};




static DataSourceDialog * datasource_geotag_create_setup_dialog(Viewport * viewport, void * user_data)
{
	return new DataSourceGeoTagDialog;
}




DataSourceGeoTagDialog::DataSourceGeoTagDialog()
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


	const QString filter1 = QObject::tr("JPG (*.jpeg *.jpg)"); /* TODO: improve this filter: mime: "image/jpeg" */
	const QString filter2 = QObject::tr("All (*)");
	const QStringList filter = { filter1, filter2 };
	this->file_entry->file_selector->setNameFilters(filter);
	this->file_entry->file_selector->selectNameFilter(filter1); /* Default to jpeg. */

	this->setMinimumWidth(400); /* TODO: perhaps this value should be #defined somewhere. */

	/* TODO: Comment from Viking:
	   Could add code to setup a default symbol (see dialog.c for symbol usage).
	   Store in user_data type and then apply when creating the waypoints.
	   However not much point since these will have images associated with them! */

	this->grid->addWidget(this->file_entry, 0, 0);
}




ProcessOptions * DataSourceGeoTagDialog::get_process_options(DownloadOptions & dl_options)
{
	ProcessOptions * po = new ProcessOptions();

	this->selected_files = this->file_entry->file_selector->selectedFiles();

	g_last_directory_url = this->file_entry->file_selector->directoryUrl();

#ifdef K /* TODO Memorize the file filter for later reuse? */
	g_last_filter = this->file_entry->file_selector->selectedNameFilter();
#endif

	/* Return some value so *thread* processing will continue */
	po->babel_args = "fake command"; /* Not really used, thus no translations. */

	return po;
}




/**
   Process selected files and try to generate waypoints storing them in the given trw.
*/
static bool datasource_geotag_process(LayerTRW * trw, ProcessOptions * po, BabelCallback status_cb, AcquireProcess * acquiring, DownloadOptions * not_used)
{
	DataSourceGeoTagDialog * user_data = (DataSourceGeoTagDialog *) acquiring->user_data;

	/* Process selected files.
	   In prinicple this loading should be quite fast and so don't need to have any progress monitoring. */
	for (int i = 0; i < user_data->selected_files.size(); i++) {
		const QString file_full_path = user_data->selected_files.at(0);

		QString name;
		Waypoint * wp = a_geotag_create_waypoint_from_file(file_full_path, acquiring->viewport->get_coord_mode(), name);
		if (wp) {
			/* Create name if geotag method didn't return one. */
			if (!name.size()) {
				name = file_base_name(file_full_path);
			}
			trw->add_waypoint_from_file(wp, name);
		} else {
			acquiring->window->statusbar_update(StatusBarField::INFO, QString("Unable to create waypoint from %1").arg(file_full_path));
		}
	}

	user_data->selected_files.clear();

	/* No failure. */
	return true;
}
