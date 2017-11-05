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
#include <cstring>
#include <cstdlib>

#include <glib/gprintf.h>

#include "acquire.h"
#include "geotag_exif.h"
#include "file_utils.h"
#include "util.h"
#include "vikutils.h"
#include "layer_trw.h"
#include "window.h"
#include "statusbar.h"
#include "viewport_internal.h"




using namespace SlavGPS;




typedef struct {
	GtkWidget * files;
	GSList * filelist;  // Files selected
} datasource_geotag_user_data_t;




/* The last used directory. */
static char * last_folder_uri = NULL;




static void * datasource_geotag_init(acq_vik_t * avt);
static void datasource_geotag_add_setup_widgets(GtkWidget * dialog, Viewport * viewport, void * user_data);
static ProcessOptions * datasource_geotag_get_process_options(void * user_data, void * not_used, char const * not_used2, char const * not_used3);
static bool datasource_geotag_process(LayerTRW * trw, ProcessOptions * po, BabelCallback status_cb, AcquireProcess * acquiring, void * not_used);
static void datasource_geotag_cleanup(void * user_data);




VikDataSourceInterface vik_datasource_geotag_interface = {
	N_("Create Waypoints from Geotagged Images"),
	N_("Geotagged Images"),
	DatasourceMode::AUTO_LAYER_MANAGEMENT,
	DatasourceInputtype::NONE,
	true,
	false, /* We should be able to see the data on the screen so no point in keeping the dialog open. */
	true,

	(DataSourceInternalDialog)                NULL,
	(VikDataSourceInitFunc)		          datasource_geotag_init,
	(VikDataSourceCheckExistenceFunc)	  NULL,
	(VikDataSourceAddSetupWidgetsFunc)        datasource_geotag_add_setup_widgets,
	(VikDataSourceGetProcessOptionsFunc)      datasource_geotag_get_process_options,
	(VikDataSourceProcessFunc)                datasource_geotag_process,
	(VikDataSourceProgressFunc)               NULL,
	(VikDataSourceAddProgressWidgetsFunc)     NULL,
	(VikDataSourceCleanupFunc)                datasource_geotag_cleanup,
	(VikDataSourceOffFunc)                    NULL,

	NULL,
	0,
	NULL,
	NULL,
	0
};




/* See VikDataSourceInterface. */
static void * datasource_geotag_init(acq_vik_t * avt)
{
	datasource_geotag_user_data_t * user_data = (datasource_geotag_user_data_t *) malloc(sizeof (datasource_geotag_user_data_t));
	user_data->filelist = NULL;
	return user_data;
}




/* See VikDataSourceInterface. */
static void datasource_geotag_add_setup_widgets(GtkWidget * dialog, Viewport * viewport, void * user_data)
{
	datasource_geotag_user_data_t * userdata = (datasource_geotag_user_data_t *) user_data;
#ifdef K
	/* The files selector. */
	userdata->files = gtk_file_chooser_widget_new(GTK_FILE_CHOOSER_ACTION_OPEN);

	/* Try to make it a nice size - otherwise seems to default to something impractically small. */
	gtk_window_set_default_size(GTK_WINDOW (dialog) , 600, 300);

	if (last_folder_uri) {
		gtk_file_chooser_set_current_folder_uri(GTK_FILE_CHOOSER(userdata->files), last_folder_uri);
	}

	GtkFileChooser * chooser = GTK_FILE_CHOOSER (userdata->files);

	/* Add filters. */
	GtkFileFilter *filter;
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("All"));
	gtk_file_filter_add_pattern(filter, "*");
	gtk_file_chooser_add_filter(chooser, filter);

	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("JPG"));
	gtk_file_filter_add_mime_type(filter, "image/jpeg");
	gtk_file_chooser_add_filter(chooser, filter);

	/* Default to jpgs. */
	gtk_file_chooser_set_filter(chooser, filter);

	/* Allow selecting more than one. */
	gtk_file_chooser_set_select_multiple(chooser, true);

	/* Could add code to setup a default symbol (see dialog.c for symbol usage).
	   Store in user_data type and then apply when creating the waypoints.
	   However not much point since these will have images associated with them! */

	/* Packing all widgets. */
	GtkBox * box = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
	box->addWidget(userdata->files);

	gtk_widget_show_all(dialog);
#endif
}




static ProcessOptions * datasource_geotag_get_process_options(void * user_data, void * not_used, char const * not_used2, char const * not_used3)
{
	ProcessOptions * po = new ProcessOptions();

	datasource_geotag_user_data_t * userdata = (datasource_geotag_user_data_t *)user_data;
#ifdef K
	/* Retrieve the files selected. */
	userdata->filelist = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(userdata->files)); /* Not reusable!! */

	/* Memorize the directory for later use. */
	free(last_folder_uri);
	last_folder_uri = gtk_file_chooser_get_current_folder_uri(GTK_FILE_CHOOSER(userdata->files));
	last_folder_uri = g_strdup(last_folder_uri);

	/* TODO Memorize the file filter for later use... */
	//GtkFileFilter *filter = gtk_file_chooser_get_filter (GTK_FILE_CHOOSER(userdata->files));
#endif
	/* Return some value so *thread* processing will continue */
	po->babel_args = "fake command"; /* Not really used, thus no translations. */

	return po;
}




/**
 * Process selected files and try to generate waypoints storing them in the given trw.
 */
static bool datasource_geotag_process(LayerTRW * trw, ProcessOptions * po, BabelCallback status_cb, AcquireProcess * acquiring, void * not_used)
{
	datasource_geotag_user_data_t * user_data = (datasource_geotag_user_data_t *) acquiring->user_data;

	/* Process selected files.
	   In prinicple this loading should be quite fast and so don't need to have any progress monitoring. */
	GSList * cur_file = user_data->filelist;
	while (cur_file) {
		char *filename = (char *) cur_file->data;
		QString name;

		Waypoint * wp = a_geotag_create_waypoint_from_file(filename, acquiring->viewport->get_coord_mode(), name);
		if (wp) {
			/* Create name if geotag method didn't return one. */
			if (!name.size()) {
				name = file_base_name(filename);
			}
			trw->add_waypoint_from_file(wp, name);
		} else {
			acquiring->window->statusbar_update(StatusBarField::INFO, QString("Unable to create waypoint from %1").arg(filename));
		}

		free(filename);
		cur_file = g_slist_next(cur_file);
	}

	/* Free memory. */
	g_slist_free(user_data->filelist);

	/* No failure. */
	return true;
}




/* See VikDataSourceInterface. */
static void datasource_geotag_cleanup(void * user_data)
{
	free(user_data);
}
