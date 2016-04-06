/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2011, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
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
#include <string.h>
#include <stdlib.h>

#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>

#include "viking.h"
#include "babel.h"
#include "gpx.h"
#include "babel_ui.h"
#include "acquire.h"

typedef struct {
  GtkWidget *file;
  GtkWidget *type;
} datasource_file_widgets_t;

extern GList * a_babel_file_list;

/* The last used directory */
static char *last_folder_uri = NULL;

/* The last used file filter */
/* Nb: we use a complex strategy for this because the UI is rebuild each
 time, so it is not possible to reuse directly the GtkFileFilter as they are
 differents. */
static BabelFile *last_file_filter = NULL;

/* The last file format selected */
static int last_type = 0;

static void * datasource_file_init ( acq_vik_t *avt );
static void datasource_file_add_setup_widgets ( GtkWidget *dialog, VikViewport *vvp, void * user_data );
static void datasource_file_get_process_options ( datasource_file_widgets_t *widgets, ProcessOptions *po, void * not_used, const char *not_used2, const char *not_used3 );
static void datasource_file_cleanup ( void * data );

VikDataSourceInterface vik_datasource_file_interface = {
  N_("Import file with GPSBabel"),
  N_("Imported file"),
  VIK_DATASOURCE_AUTO_LAYER_MANAGEMENT,
  VIK_DATASOURCE_INPUTTYPE_NONE,
  true,
  true,
  true,
  (VikDataSourceInitFunc)		datasource_file_init,
  (VikDataSourceCheckExistenceFunc)	NULL,
  (VikDataSourceAddSetupWidgetsFunc)	datasource_file_add_setup_widgets,
  (VikDataSourceGetProcessOptionsFunc)  datasource_file_get_process_options,
  (VikDataSourceProcessFunc)            a_babel_convert_from,
  (VikDataSourceProgressFunc)		NULL,
  (VikDataSourceAddProgressWidgetsFunc)	NULL,
  (VikDataSourceCleanupFunc)		datasource_file_cleanup,
  (VikDataSourceOffFunc)                NULL,

  NULL,
  0,
  NULL,
  NULL,
  0
};

/* See VikDataSourceInterface */
static void * datasource_file_init ( acq_vik_t *avt )
{
  datasource_file_widgets_t *widgets = (datasource_file_widgets_t *) malloc(sizeof (datasource_file_widgets_t));
  return widgets;
}

static void add_file_filter (void * data, void * user_data)
{
  GtkFileChooser *chooser = GTK_FILE_CHOOSER ( user_data );
  const char *label = ((BabelFile*) data)->label;
  const char *ext = ((BabelFile*) data)->ext;
  if ( ext == NULL || ext[0] == '\0' )
    /* No file extension => no filter */
	return;
  char *pattern = g_strdup_printf ( "*.%s", ext );

  GtkFileFilter *filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern ( filter, pattern );
  if ( strstr ( label, pattern+1 ) ) {
    gtk_file_filter_set_name ( filter, label );
  } else {
    /* Ensure displayed label contains file pattern */
	/* NB: we skip the '*' in the pattern */
	char *name = g_strdup_printf ( "%s (%s)", label, pattern+1 );
    gtk_file_filter_set_name ( filter, name );
	free( name );
  }
  g_object_set_data ( G_OBJECT(filter), "Babel", data );
  gtk_file_chooser_add_filter ( chooser, filter );
  if ( last_file_filter == data )
    /* Previous selection used this filter */
    gtk_file_chooser_set_filter ( chooser, filter );

  free( pattern );
}

/* See VikDataSourceInterface */
static void datasource_file_add_setup_widgets ( GtkWidget *dialog, VikViewport *vvp, void * user_data )
{
  datasource_file_widgets_t *widgets = (datasource_file_widgets_t *)user_data;
  GtkWidget *filename_label, *type_label;

  /* The file selector */
  filename_label = gtk_label_new (_("File:"));
  widgets->file = gtk_file_chooser_button_new (_("File to import"), GTK_FILE_CHOOSER_ACTION_OPEN);
  if (last_folder_uri)
    gtk_file_chooser_set_current_folder_uri ( GTK_FILE_CHOOSER(widgets->file), last_folder_uri);
  /* Add filters */
  g_list_foreach ( a_babel_file_list, add_file_filter, widgets->file );
  GtkFileFilter *all_filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern ( all_filter, "*" );
  gtk_file_filter_set_name ( all_filter, _("All files") );
  gtk_file_chooser_add_filter ( GTK_FILE_CHOOSER(widgets->file), all_filter );
  if ( last_file_filter == NULL )
    /* No previously selected filter or 'All files' selected */
    gtk_file_chooser_set_filter ( GTK_FILE_CHOOSER(widgets->file), all_filter );

  /* The file format selector */
  type_label = gtk_label_new (_("File type:"));
  /* Propose any readable file */
  BabelMode mode = { 1, 0, 1, 0, 1, 0 };
  widgets->type = a_babel_ui_file_type_selector_new ( mode );
  g_signal_connect ( G_OBJECT(widgets->type), "changed",
      G_CALLBACK(a_babel_ui_type_selector_dialog_sensitivity_cb), dialog );
  gtk_combo_box_set_active ( GTK_COMBO_BOX(widgets->type), last_type );
  /* Manually call the callback to fix the state */
  a_babel_ui_type_selector_dialog_sensitivity_cb ( GTK_COMBO_BOX(widgets->type), dialog );

  /* Packing all these widgets */
  GtkBox *box = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
  gtk_box_pack_start ( box, filename_label, false, false, 5 );
  gtk_box_pack_start ( box, widgets->file, false, false, 5 );
  gtk_box_pack_start ( box, type_label, false, false, 5 );
  gtk_box_pack_start ( box, widgets->type, false, false, 5 );
  gtk_widget_show_all(dialog);
}

/* See VikDataSourceInterface */
static void datasource_file_get_process_options ( datasource_file_widgets_t *widgets, ProcessOptions *po, void * not_used, const char *not_used2, const char *not_used3 )
{
  /* Retrieve the file selected */
  char *filename = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER(widgets->file) );

  /* Memorize the directory for later use */
  free(last_folder_uri);
  last_folder_uri = gtk_file_chooser_get_current_folder_uri ( GTK_FILE_CHOOSER(widgets->file) );

  /* Memorize the file filter for later use */
  GtkFileFilter *filter = gtk_file_chooser_get_filter ( GTK_FILE_CHOOSER(widgets->file) );
  last_file_filter = (BabelFile * ) g_object_get_data ( G_OBJECT(filter), "Babel" );

  /* Retrieve and memorize file format selected */
  char *type = NULL;
  last_type = gtk_combo_box_get_active ( GTK_COMBO_BOX (widgets->type) );
  type = (a_babel_ui_file_type_selector_get ( widgets->type ))->name;

  /* Generate the process options */
  po->babelargs = g_strdup_printf( "-i %s", type);
  po->filename = g_strdup(filename);

  /* Free memory */
  free(filename);

  fprintf(stderr, _("DEBUG: using babel args '%s' and file '%s'\n"), po->babelargs, po->filename);
}

/* See VikDataSourceInterface */
static void datasource_file_cleanup ( void * data )
{
  free( data );
}

