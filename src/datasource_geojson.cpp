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
 *
 */

#include <cstdlib>

#include <glib/gstdio.h>

#include "acquire.h"
#include "babel.h"
#include "geojson.h"
#include "window.h"
#include "util.h"
#include "widget_file_entry.h"




using namespace SlavGPS;




typedef struct {
	SGFileEntry * file_entry = NULL;
	QStringList filelist;  /* Files selected. */
} datasource_geojson_user_data_t;




/* The last used directory. */
static QUrl last_directory_url;




static void * datasource_geojson_init(acq_vik_t * avt);
static DataSourceDialog * datasource_geojson_create_setup_dialog(Viewport * viewport, void * user_data);
static ProcessOptions * datasource_geojson_get_process_options(datasource_geojson_user_data_t * user_data, void * not_used, char const * not_used2, char const * not_used3);
static bool datasource_geojson_process(LayerTRW * trw, ProcessOptions * process_options, BabelCallback status_cb, AcquireProcess * acquiring, DownloadOptions * unused);
static void datasource_geojson_cleanup(void * data);




DataSourceInterface datasource_geojson_interface = {
	N_("Acquire from GeoJSON"),
	N_("GeoJSON"),
	DataSourceMode::AUTO_LAYER_MANAGEMENT,
	DatasourceInputtype::NONE,
	true,
	false, /* false = don't keep dialog open after success. We should be able to see the data on the screen so no point in keeping the dialog open. */
	false, /* false = don't run as thread. Open each file in the main loop. */

	(DataSourceInitFunc)                  datasource_geojson_init,
	(DataSourceCheckExistenceFunc)        NULL,
	(DataSourceCreateSetupDialogFunc)     datasource_geojson_create_setup_dialog,
	(DataSourceGetProcessOptionsFunc)     datasource_geojson_get_process_options,
	(DataSourceProcessFunc)               datasource_geojson_process,
	(DataSourceProgressFunc)              NULL,
	(DataSourceCreateProgressDialogFunc)  NULL,
	(DataSourceCleanupFunc)               datasource_geojson_cleanup,
	(DataSourceTurnOffFunc)               NULL,
	NULL,
	0,
	NULL,
	NULL,
	0
};




static void * datasource_geojson_init(acq_vik_t * avt)
{
	datasource_geojson_user_data_t * user_data = (datasource_geojson_user_data_t *) malloc(sizeof (datasource_geojson_user_data_t));
	user_data->filelist.clear();
	return user_data;
}




static DataSourceDialog * datasource_geojson_create_setup_dialog(Viewport * viewport, void * user_data)
{
	DataSourceDialog * setup_dialog = NULL;
	GtkWidget * dialog;
	datasource_geojson_user_data_t * ud = (datasource_geojson_user_data_t *) user_data;

#ifdef K
	ud->file_entry = new SGFileEntry;

	/* Try to make it a nice size - otherwise seems to default to something impractically small. */
	gtk_window_set_default_size(GTK_WINDOW (dialog), 600, 300);
#endif

	if (last_directory_url.isValid()) {
		ud->file_entry->file_selector->setDirectoryUrl(last_directory_url);
	}
#ifdef K
	GtkFileChooser * chooser = GTK_FILE_CHOOSER (ud->file_entry);

	/* Add filters. */
	GtkFileFilter * filter;
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("All"));
	gtk_file_filter_add_pattern(filter, "*");
	gtk_file_chooser_add_filter(chooser, filter);

	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("GeoJSON"));
	gtk_file_filter_add_pattern(filter, "*.geojson");
	gtk_file_chooser_add_filter(chooser, filter);

	/* Default to geojson. */
	gtk_file_chooser_set_filter(chooser, filter);

	/* Allow selecting more than one. */
	gtk_file_chooser_set_select_multiple(chooser, true);

	/* Packing all widgets. */
	GtkBox * box = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
	box->addWidget(ud->file_entry);

	gtk_widget_show_all(dialog);
#endif

	return setup_dialog;
}




ProcessOptions * datasource_geojson_get_process_options(datasource_geojson_user_data_t * userdata, void * not_used, char const * not_used2, char const * not_used3)
{
	ProcessOptions * po = new ProcessOptions();
#ifdef K
	/* Retrieve the files selected. */
	userdata->filelist = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(userdata->file_entry)); /* Not reusable!! */
#endif
	/* Memorize the directory for later reuse. */
	last_directory_url = userdata->file_entry->file_selector->directoryUrl();

	/* TODO Memorize the file filter for later reuse? */
	//GtkFileFilter *filter = gtk_file_chooser_get_filter(GTK_FILE_CHOOSER(userdata->file_entry));

	/* Return some value so *thread* processing will continue. */
	po->babel_args = "fake command"; /* Not really used, thus no translations. */

	return po;
}




/**
 * Process selected files and try to generate waypoints storing them in the given trw.
 */
static bool datasource_geojson_process(LayerTRW * trw, ProcessOptions * process_options, BabelCallback status_cb, AcquireProcess * acquiring, DownloadOptions * unused)
{
	datasource_geojson_user_data_t * user_data = (datasource_geojson_user_data_t *) acquiring->user_data;

	/* Process selected files. */
	for (int i = 0; i < user_data->filelist.size(); i++) {
		const QString file_full_path = user_data->filelist.at(i);

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

	user_data->filelist.clear();

	/* No failure. */
	return true;
}




static void datasource_geojson_cleanup(void * data)
{
	free(data);
}
