/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <stdlib.h>

#include "viking.h"
#include "acquire.h"
#include "babel.h"

// Initially was just going to be a URL and always in GPX format
// But might as well specify the file type as per datasource_file.c
// However in this version we'll cope with no GPSBabel available and in this case just try GPX

typedef struct {
	GtkWidget *url;
	GtkWidget *type;
} datasource_url_widgets_t;

extern GList * a_babel_device_list;
extern GList * a_babel_file_list;

/* The last file format selected */
static int last_type = -1;

static void * datasource_url_init ( acq_vik_t *avt );
static void datasource_url_add_setup_widgets ( GtkWidget *dialog, VikViewport *vvp, void * user_data );
static void datasource_url_get_process_options ( datasource_url_widgets_t *widgets, ProcessOptions *po, DownloadFileOptions *download_options, const char *not_used2, const char *not_used3 );
static void datasource_url_cleanup ( void * data );

VikDataSourceInterface vik_datasource_url_interface = {
	N_("Acquire from URL"),
	N_("URL"),
	VIK_DATASOURCE_AUTO_LAYER_MANAGEMENT,
	VIK_DATASOURCE_INPUTTYPE_NONE,
	true,
	true,
	true,
	(VikDataSourceInitFunc)               datasource_url_init,
	(VikDataSourceCheckExistenceFunc)     NULL,
	(VikDataSourceAddSetupWidgetsFunc)    datasource_url_add_setup_widgets,
	(VikDataSourceGetProcessOptionsFunc)  datasource_url_get_process_options,
	(VikDataSourceProcessFunc)            a_babel_convert_from,
	(VikDataSourceProgressFunc)           NULL,
	(VikDataSourceAddProgressWidgetsFunc) NULL,
	(VikDataSourceCleanupFunc)            datasource_url_cleanup,
	(VikDataSourceOffFunc)                NULL,
	NULL,
	0,
	NULL,
	NULL,
	0
};

static void * datasource_url_init ( acq_vik_t *avt )
{
	datasource_url_widgets_t *widgets = (datasource_url_widgets_t *) malloc(sizeof (datasource_url_widgets_t));
	return widgets;
}

static void fill_combo_box (void * data, void * user_data)
{
	const char *label = ((BabelFile*) data)->label;
	vik_combo_box_text_append (GTK_WIDGET(user_data), label);
}

static int find_entry = -1;
static int wanted_entry = -1;

static void find_type (void * elem, void * user_data)
{
	const char *name = ((BabelFile*)elem)->name;
	const char *type_name = (const char *) user_data;
	find_entry++;
	if (!g_strcmp0(name, type_name)) {
		wanted_entry = find_entry;
	}
}

#define VIK_SETTINGS_URL_FILE_DL_TYPE "url_file_download_type"

static void datasource_url_add_setup_widgets ( GtkWidget *dialog, VikViewport *vvp, void * user_data )
{
	datasource_url_widgets_t *widgets = (datasource_url_widgets_t *)user_data;
	GtkWidget *label = gtk_label_new (_("URL:"));
	widgets->url = gtk_entry_new ( );

	GtkWidget *type_label = gtk_label_new (_("File type:"));

	if ( last_type < 0 ) {
		find_entry = -1;
		wanted_entry = -1;
		char *type = NULL;
		if ( a_settings_get_string ( VIK_SETTINGS_URL_FILE_DL_TYPE, &type ) ) {
			// Use setting
			if ( type )
				g_list_foreach (a_babel_file_list, find_type, type);
		}
		else {
			// Default to GPX if possible
			g_list_foreach (a_babel_file_list, find_type, (void *) "gpx");
		}
		// If not found set it to the first entry, otherwise use the entry
		last_type = ( wanted_entry < 0 ) ? 0 : wanted_entry;
	}

	if ( a_babel_available() ) {
		widgets->type = vik_combo_box_text_new ();
		g_list_foreach (a_babel_file_list, fill_combo_box, widgets->type);
		gtk_combo_box_set_active (GTK_COMBO_BOX (widgets->type), last_type);
	}
	else {
		// Only GPX (not using GPSbabel)
		widgets->type = gtk_label_new (_("GPX"));
	}

	/* Packing all widgets */
	GtkBox *box = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
	gtk_box_pack_start ( box, label, false, false, 5 );
	gtk_box_pack_start ( box, widgets->url, false, false, 5 );
	gtk_box_pack_start ( box, type_label, false, false, 5 );
	gtk_box_pack_start ( box, widgets->type, false, false, 5 );

	gtk_widget_show_all(dialog);
}

static void datasource_url_get_process_options ( datasource_url_widgets_t *widgets, ProcessOptions *po, DownloadFileOptions *download_options, const char *not_used2, const char *not_used3 )
{
	// Retrieve the user entered value
	const char *value = gtk_entry_get_text ( GTK_ENTRY(widgets->url) );

	if (GTK_IS_COMBO_BOX (widgets->type) )
		last_type = gtk_combo_box_get_active ( GTK_COMBO_BOX (widgets->type) );

	po->input_file_type = NULL; // Default to gpx
	if ( a_babel_file_list )
		po->input_file_type = g_strdup( ((BabelFile*)g_list_nth_data (a_babel_file_list, last_type))->name );

	po->url = g_strdup( value );

	// Support .zip + bzip2 files directly
	download_options->convert_file = a_try_decompress_file;
	download_options->follow_location = 5;
}

static void datasource_url_cleanup ( void * data )
{
	free( data );
}
