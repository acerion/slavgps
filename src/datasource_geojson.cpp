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

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

//#include "viking.h"
#include "acquire.h"
#include "babel.h"
#include "geojson.h"




using namespace SlavGPS;




typedef struct {
	GtkWidget * files;
	GSList * filelist;  /* Files selected. */
} datasource_geojson_user_data_t;




/* The last used directory. */
static char * last_folder_uri = NULL;




static void * datasource_geojson_init(acq_vik_t * avt);
static void datasource_geojson_add_setup_widgets(GtkWidget * dialog, Viewport * viewport, void * user_data);
static void datasource_geojson_get_process_options(datasource_geojson_user_data_t * user_data, ProcessOptions * po, void * not_used, char const * not_used2, char const * not_used3);
static bool datasource_geojson_process(LayerTRW * trw, ProcessOptions * process_options, BabelStatusFunc status_cb, acq_dialog_widgets_t * adw, DownloadFileOptions * options_unused);
static void datasource_geojson_cleanup(void * data);




VikDataSourceInterface vik_datasource_geojson_interface = {
	N_("Acquire from GeoJSON"),
	N_("GeoJSON"),
	VIK_DATASOURCE_AUTO_LAYER_MANAGEMENT,
	VIK_DATASOURCE_INPUTTYPE_NONE,
	true,
	false, /* We should be able to see the data on the screen so no point in keeping the dialog open. */
	false, /* Not thread method - open each file in the main loop. */
	(VikDataSourceInitFunc)               datasource_geojson_init,
	(VikDataSourceCheckExistenceFunc)     NULL,
	(VikDataSourceAddSetupWidgetsFunc)    datasource_geojson_add_setup_widgets,
	(VikDataSourceGetProcessOptionsFunc)  datasource_geojson_get_process_options,
	(VikDataSourceProcessFunc)            datasource_geojson_process,
	(VikDataSourceProgressFunc)           NULL,
	(VikDataSourceAddProgressWidgetsFunc) NULL,
	(VikDataSourceCleanupFunc)            datasource_geojson_cleanup,
	(VikDataSourceOffFunc)                NULL,
	NULL,
	0,
	NULL,
	NULL,
	0
};




static void * datasource_geojson_init(acq_vik_t * avt)
{
	datasource_geojson_user_data_t * user_data = (datasource_geojson_user_data_t *) malloc(sizeof (datasource_geojson_user_data_t));
	user_data->filelist = NULL;
	return user_data;
}




static void datasource_geojson_add_setup_widgets(GtkWidget * dialog, Viewport * viewport, void * user_data)
{
	datasource_geojson_user_data_t * ud = (datasource_geojson_user_data_t *) user_data;

	ud->files = gtk_file_chooser_widget_new(GTK_FILE_CHOOSER_ACTION_OPEN);

	/* Try to make it a nice size - otherwise seems to default to something impractically small. */
	gtk_window_set_default_size(GTK_WINDOW (dialog) , 600, 300);

	if (last_folder_uri) {
		gtk_file_chooser_set_current_folder_uri(GTK_FILE_CHOOSER(ud->files), last_folder_uri);
	}

	GtkFileChooser * chooser = GTK_FILE_CHOOSER (ud->files);

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
	gtk_box_pack_start(box, ud->files, true, true, 0);

	gtk_widget_show_all(dialog);
}




static void datasource_geojson_get_process_options(datasource_geojson_user_data_t * userdata, ProcessOptions * po, void * not_used, char const * not_used2, char const * not_used3)
{
	/* Retrieve the files selected. */
	userdata->filelist = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(userdata->files)); /* Not reusable!! */

	/* Memorize the directory for later reuse. */
	free(last_folder_uri);
	last_folder_uri = gtk_file_chooser_get_current_folder_uri(GTK_FILE_CHOOSER(userdata->files));

	/* TODO Memorize the file filter for later reuse? */
	//GtkFileFilter *filter = gtk_file_chooser_get_filter(GTK_FILE_CHOOSER(userdata->files));

	/* Return some value so *thread* processing will continue. */
	po->babelargs = strdup("fake command"); /* Not really used, thus no translations. */
}




/**
 * Process selected files and try to generate waypoints storing them in the given trw.
 */
static bool datasource_geojson_process(LayerTRW * trw, ProcessOptions * process_options, BabelStatusFunc status_cb, acq_dialog_widgets_t * adw, DownloadFileOptions * options_unused)
{
	datasource_geojson_user_data_t * user_data = (datasource_geojson_user_data_t *) adw->user_data;

	/* Process selected files. */
	GSList * cur_file = user_data->filelist;
	while (cur_file) {
		char * filename = (char *) cur_file->data;

		char * gpx_filename = geojson_import_to_gpx(filename);
		if (gpx_filename) {
			/* Important that this process is run in the main thread. */
			adw->window->open_file(gpx_filename, false);
			/* Delete the temporary file. */
			(void) remove(gpx_filename);
			free(gpx_filename);
		} else {
			char * msg = g_strdup_printf(_("Unable to import from: %s"), filename);
			adw->window->statusbar_update(msg, VIK_STATUSBAR_INFO);
			free(msg);
		}

		free(filename);
		cur_file = g_slist_next(cur_file);
	}

	/* Free memory. */
	g_slist_free(user_data->filelist);

	/* No failure. */
	return true;
}




static void datasource_geojson_cleanup(void * data)
{
	free(data);
}
