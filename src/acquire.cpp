/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013-2015, Rob Norris <rw_norris@hotmail.com>
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "viking.h"
#include "babel.h"
#include "gpx.h"
#include "acquire.h"

/************************ FILTER LIST *******************/
// extern VikDataSourceInterface vik_datasource_gps_interface;

/*** Input is TRWLayer ***/
extern VikDataSourceInterface vik_datasource_bfilter_simplify_interface;
extern VikDataSourceInterface vik_datasource_bfilter_compress_interface;
extern VikDataSourceInterface vik_datasource_bfilter_dup_interface;
extern VikDataSourceInterface vik_datasource_bfilter_manual_interface;

/*** Input is a track and a TRWLayer ***/
extern VikDataSourceInterface vik_datasource_bfilter_polygon_interface;
extern VikDataSourceInterface vik_datasource_bfilter_exclude_polygon_interface;

/*** Input is a track ***/

const VikDataSourceInterface *filters[] = {
  &vik_datasource_bfilter_simplify_interface,
  &vik_datasource_bfilter_compress_interface,
  &vik_datasource_bfilter_dup_interface,
  &vik_datasource_bfilter_manual_interface,
  &vik_datasource_bfilter_polygon_interface,
  &vik_datasource_bfilter_exclude_polygon_interface,
};

const unsigned int N_FILTERS = sizeof(filters) / sizeof(filters[0]);

Track * filter_track = NULL;

/********************************************************/

/* passed along to worker thread */
typedef struct {
  acq_dialog_widgets_t *w;
  ProcessOptions *po;
  bool creating_new_layer;
  VikTrwLayer *vtl;
  DownloadFileOptions *options;
} w_and_interface_t;


/*********************************************************
 * Definitions and routines for acquiring data from Data Sources in general
 *********************************************************/

static void progress_func ( BabelProgressCode c, void * data, acq_dialog_widgets_t *w )
{
  if ( w->source_interface->is_thread ) {
    gdk_threads_enter ();
    if ( !w->running ) {
      if ( w->source_interface->cleanup_func )
        w->source_interface->cleanup_func ( w->user_data );
      gdk_threads_leave ();
      g_thread_exit ( NULL );
    }
    gdk_threads_leave ();
  }

  if ( w->source_interface->progress_func )
    w->source_interface->progress_func ( c, data, w );
}

/**
 * Some common things to do on completion of a datasource process
 *  . Update layer
 *  . Update dialog info
 *  . Update main dsisplay
 */
static void on_complete_process (w_and_interface_t *wi)
{
  if (wi->w->running) {
    gtk_label_set_text ( GTK_LABEL(wi->w->status), _("Done.") );
    if ( wi->creating_new_layer ) {
      /* Only create the layer if it actually contains anything useful */
      // TODO: create function for this operation to hide detail:
      if ( ! wi->vtl->trw.is_empty() ) {
        vik_layer_post_read ( VIK_LAYER(wi->vtl), wi->w->vvp, true );
        vik_aggregate_layer_add_layer ( vik_layers_panel_get_top_layer(wi->w->vlp), VIK_LAYER(wi->vtl), true );
      }
      else
        gtk_label_set_text ( GTK_LABEL(wi->w->status), _("No data.") );
    }
    if ( wi->w->source_interface->keep_dialog_open ) {
      gtk_dialog_set_response_sensitive ( GTK_DIALOG(wi->w->dialog), GTK_RESPONSE_ACCEPT, true );
      gtk_dialog_set_response_sensitive ( GTK_DIALOG(wi->w->dialog), GTK_RESPONSE_REJECT, false );
    } else {
      gtk_dialog_response ( GTK_DIALOG(wi->w->dialog), GTK_RESPONSE_ACCEPT );
    }
    // Main display update
    if ( wi->vtl ) {
      vik_layer_post_read ( VIK_LAYER(wi->vtl), wi->w->vvp, true );
      // View this data if desired - must be done after post read (so that the bounds are known)
      if ( wi->w->source_interface->autoview ) {
        wi->vtl->trw.auto_set_view(vik_layers_panel_get_viewport(wi->w->vlp));
      }
      vik_layers_panel_emit_update ( wi->w->vlp );
    }
  } else {
    /* cancelled */
    if ( wi->creating_new_layer )
      g_object_unref(wi->vtl);
  }
}

static void free_process_options ( ProcessOptions *po )
{
  if ( po ) {
    free( po->babelargs );
    free( po->filename );
    free( po->input_file_type );
    free( po->babel_filters );
    free( po->url );
    free( po->shell_command );
    free( po );
  }
}

/* this routine is the worker thread.  there is only one simultaneous download allowed */
static void get_from_anything ( w_and_interface_t *wi )
{
  bool result = true;

  VikDataSourceInterface *source_interface = wi->w->source_interface;

  if ( source_interface->process_func ) {
    result = source_interface->process_func ( wi->vtl, wi->po, (BabelStatusFunc)progress_func, wi->w, wi->options );
  }
  free_process_options ( wi->po );
  free( wi->options );

  if (wi->w->running && !result) {
    gdk_threads_enter();
    gtk_label_set_text ( GTK_LABEL(wi->w->status), _("Error: acquisition failed.") );
    if ( wi->creating_new_layer )
      g_object_unref ( G_OBJECT ( wi->vtl ) );
    gdk_threads_leave();
  }
  else {
    gdk_threads_enter();
    on_complete_process ( wi );
    gdk_threads_leave();
  }

  if ( source_interface->cleanup_func )
    source_interface->cleanup_func ( wi->w->user_data );

  if ( wi->w->running ) {
    wi->w->running = false;
  }
  else {
    free( wi->w );
    free( wi );
    wi = NULL;
  }

  g_thread_exit ( NULL );
}

/* depending on type of filter, often only vtl or track will be given.
 * the other can be NULL.
 */
static void acquire ( VikWindow *vw,
                      VikLayersPanel *vlp,
                      VikViewport *vvp,
                      vik_datasource_mode_t mode,
                      VikDataSourceInterface *source_interface,
                      VikTrwLayer *vtl,
                      Track * trk,
                      void * userdata,
                      VikDataSourceCleanupFunc cleanup_function )
{
  /* for manual dialogs */
  GtkWidget *dialog = NULL;
  GtkWidget *status;
  char *args_off = NULL;
  char *fd_off = NULL;
  void * user_data;
  DownloadFileOptions * options = (DownloadFileOptions *) malloc(sizeof (DownloadFileOptions));
  memset(options, 0, sizeof (DownloadFileOptions));

  acq_vik_t avt;
  avt.vlp = vlp;
  avt.vvp = vvp;
  avt.vw = vw;
  avt.userdata = userdata;

  /* for UI builder */
  void * pass_along_data;
  VikLayerParamData *paramdatas = NULL;

    /*** INIT AND CHECK EXISTENCE ***/
  if ( source_interface->init_func )
    user_data = source_interface->init_func(&avt);
  else
    user_data = NULL;
  pass_along_data = user_data;

  if ( source_interface->check_existence_func ) {
    char *error_str = source_interface->check_existence_func();
    if ( error_str ) {
      a_dialog_error_msg ( GTK_WINDOW(vw), error_str );
      free( error_str );
      return;
    }
  }

  /* BUILD UI & GET OPTIONS IF NECESSARY. */

  /* POSSIBILITY 0: NO OPTIONS. DO NOTHING HERE. */
  /* POSSIBILITY 1: ADD_SETUP_WIDGETS_FUNC */
  if ( source_interface->add_setup_widgets_func ) {
    dialog = gtk_dialog_new_with_buttons ( "", GTK_WINDOW(vw), (GtkDialogFlags) 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL );

    gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );
    GtkWidget *response_w = NULL;
#if GTK_CHECK_VERSION (2, 20, 0)
    response_w = gtk_dialog_get_widget_for_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );
#endif

    source_interface->add_setup_widgets_func(dialog, vvp, user_data);
    gtk_window_set_title ( GTK_WINDOW(dialog), _(source_interface->window_title) );

    if ( response_w )
      gtk_widget_grab_focus ( response_w );

    if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) != GTK_RESPONSE_ACCEPT ) {
      source_interface->cleanup_func(user_data);
      gtk_widget_destroy(dialog);
      return;
    }
  }
  /* POSSIBILITY 2: UI BUILDER */
  else if ( source_interface->params ) {
    paramdatas = a_uibuilder_run_dialog ( source_interface->window_title, GTK_WINDOW(vw),
			source_interface->params, source_interface->params_count,
			source_interface->params_groups, source_interface->params_groups_count,
			source_interface->params_defaults );
    if ( paramdatas )
      pass_along_data = paramdatas;
    else
      return; /* TODO: do we have to free anything here? */
  }

  /* CREATE INPUT DATA & GET OPTIONS */
  ProcessOptions *po = (ProcessOptions *) malloc(sizeof (ProcessOptions));
  memset(po, 0, sizeof (ProcessOptions));

  if ( source_interface->inputtype == VIK_DATASOURCE_INPUTTYPE_TRWLAYER ) {
    char *name_src = a_gpx_write_tmp_file ( vtl, NULL );

    source_interface->get_process_options_func ( pass_along_data, po, NULL, name_src, NULL );

    util_add_to_deletion_list ( name_src );

    free( name_src );
  } else if ( source_interface->inputtype == VIK_DATASOURCE_INPUTTYPE_TRWLAYER_TRACK ) {
    char *name_src = a_gpx_write_tmp_file ( vtl, NULL );
    char *name_src_track = a_gpx_write_track_tmp_file (trk, NULL );

    source_interface->get_process_options_func ( pass_along_data, po, NULL, name_src, name_src_track );

    util_add_to_deletion_list ( name_src );
    util_add_to_deletion_list ( name_src_track );

    free( name_src );
    free( name_src_track );
  } else if ( source_interface->inputtype == VIK_DATASOURCE_INPUTTYPE_TRACK ) {
    char *name_src_track = a_gpx_write_track_tmp_file (trk, NULL );

    source_interface->get_process_options_func ( pass_along_data, po, NULL, NULL, name_src_track );

    free( name_src_track );
  } else if ( source_interface->get_process_options_func )
    source_interface->get_process_options_func ( pass_along_data, po, options, NULL, NULL );

  /* Get data for Off command */
  if ( source_interface->off_func ) {
    source_interface->off_func ( pass_along_data, &args_off, &fd_off );
  }

  /* cleanup for option dialogs */
  if ( source_interface->add_setup_widgets_func ) {
    gtk_widget_destroy(dialog);
    dialog = NULL;
  } else if ( source_interface->params ) {
    a_uibuilder_free_paramdatas ( paramdatas, source_interface->params, source_interface->params_count );
  }

  acq_dialog_widgets_t * w = (acq_dialog_widgets_t *) malloc(sizeof (acq_dialog_widgets_t));
  w_and_interface_t *wi = (w_and_interface_t *) malloc(sizeof (w_and_interface_t));
  wi->w = w;
  wi->w->source_interface = source_interface;
  wi->po = po;
  wi->options = options;
  wi->vtl = vtl;
  wi->creating_new_layer = (!vtl); // Default if Auto Layer Management is passed in

  dialog = gtk_dialog_new_with_buttons ( "", GTK_WINDOW(vw), (GtkDialogFlags) 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL );
  gtk_dialog_set_response_sensitive ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT, false );
  gtk_window_set_title ( GTK_WINDOW(dialog), _(source_interface->window_title) );

  w->dialog = dialog;
  w->running = true;
  status = gtk_label_new (_("Working..."));
  gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), status, false, false, 5 );
  gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );
  // May not want to see the dialog at all
  if ( source_interface->is_thread || source_interface->keep_dialog_open )
    gtk_widget_show_all(dialog);
  w->status = status;

  w->vw = vw;
  w->vlp = vlp;
  w->vvp = vvp;
  if ( source_interface->add_progress_widgets_func ) {
    source_interface->add_progress_widgets_func ( dialog, user_data );
  }
  w->user_data = user_data;

  if ( mode == VIK_DATASOURCE_ADDTOLAYER ) {
    VikLayer *current_selected = vik_layers_panel_get_selected ( w->vlp );
    if ( IS_VIK_TRW_LAYER(current_selected) ) {
      wi->vtl = VIK_TRW_LAYER(current_selected);
      wi->creating_new_layer = false;
    }
  }
  else if ( mode == VIK_DATASOURCE_CREATENEWLAYER ) {
    wi->creating_new_layer = true;
  }
  else if ( mode == VIK_DATASOURCE_MANUAL_LAYER_MANAGEMENT ) {
    // Don't create in acquire - as datasource will perform the necessary actions
    wi->creating_new_layer = false;
    VikLayer *current_selected = vik_layers_panel_get_selected ( w->vlp );
    if ( IS_VIK_TRW_LAYER(current_selected) )
      wi->vtl = VIK_TRW_LAYER(current_selected);
  }
  if ( wi->creating_new_layer ) {
    wi->vtl = VIK_TRW_LAYER ( vik_layer_create ( VIK_LAYER_TRW, w->vvp, false ) );
    vik_layer_rename ( VIK_LAYER ( wi->vtl ), _(source_interface->layer_title) );
  }

  if ( source_interface->is_thread ) {
    if ( po->babelargs || po->url || po->shell_command ) {
#if GLIB_CHECK_VERSION (2, 32, 0)
      g_thread_try_new ( "get_from_anything", (GThreadFunc)get_from_anything, wi, NULL );
#else
      g_thread_create ( (GThreadFunc)get_from_anything, wi, false, NULL );
#endif
      gtk_dialog_run ( GTK_DIALOG(dialog) );
      if (w->running) {
        // Cancel and mark for thread to finish
        w->running = false;
        // NB Thread will free memory
      } else {
        if ( args_off ) {
          /* Turn off */
          ProcessOptions off_po = { args_off, fd_off, NULL, NULL, NULL };
          a_babel_convert_from (NULL, &off_po, NULL, NULL, NULL);
          free( args_off );
        }
        if ( fd_off )
          free( fd_off );

        // Thread finished by normal completion - free memory
        free( w );
        free( wi );
      }
    }
    else {
      // This shouldn't happen...
      gtk_label_set_text ( GTK_LABEL(w->status), _("Unable to create command\nAcquire method failed.") );
      gtk_dialog_run (GTK_DIALOG (dialog));
    }
  }
  else {
    // bypass thread method malarkly - you'll just have to wait...
    if ( source_interface->process_func ) {
      bool result = source_interface->process_func ( wi->vtl, po, (BabelStatusFunc) progress_func, w, options );
      if ( !result )
        a_dialog_msg ( GTK_WINDOW(vw), GTK_MESSAGE_ERROR, _("Error: acquisition failed."), NULL );
    }
    free_process_options ( po );
    free( options );

    on_complete_process ( wi );
    // Actually show it if necessary
    if ( wi->w->source_interface->keep_dialog_open )
      gtk_dialog_run ( GTK_DIALOG(dialog) );

    free( w );
    free( wi );
  }

  gtk_widget_destroy ( dialog );

  if ( cleanup_function )
    cleanup_function ( source_interface );
}

/**
 * a_acquire:
 * @vw: The #VikWindow to work with
 * @vlp: The #VikLayersPanel in which a #VikTrwLayer layer may be created/appended
 * @vvp: The #VikViewport defining the current view
 * @mode: How layers should be managed
 * @source_interface: The #VikDataSourceInterface determining how and what actions to take
 * @userdata: External data to be passed into the #VikDataSourceInterface
 * @cleanup_function: The function to dispose the #VikDataSourceInterface if necessary
 *
 * Process the given VikDataSourceInterface for sources with no input data.
 */
void a_acquire ( VikWindow *vw,
                 VikLayersPanel *vlp,
                 VikViewport *vvp,
                 vik_datasource_mode_t mode,
                 VikDataSourceInterface *source_interface,
                 void * userdata,
                 VikDataSourceCleanupFunc cleanup_function )
{
  acquire ( vw, vlp, vvp, mode, source_interface, NULL, NULL, userdata, cleanup_function );
}

static void acquire_trwlayer_callback ( GObject *menuitem, void * *pass_along )
{
  VikDataSourceInterface * iface = (VikDataSourceInterface *) g_object_get_data ( menuitem, "vik_acq_iface" );
  VikWindow *vw = (VikWindow *) pass_along[0];
  VikLayersPanel *vlp =	(VikLayersPanel *) pass_along[1];
  VikViewport *vvp = (VikViewport *) pass_along[2];
  VikTrwLayer *vtl = (VikTrwLayer *) pass_along[3];
  Track * trk = (Track *) pass_along[4];

  acquire ( vw, vlp, vvp, iface->mode, iface, vtl, trk, NULL, NULL );
}

static GtkWidget *acquire_build_menu ( VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp,
				VikTrwLayer *vtl, Track * trk, /* both passed to acquire, although for many filters only one ness */
				const char *menu_title, vik_datasource_inputtype_t inputtype )
{
  static void * pass_along[5];
  GtkWidget *menu_item=NULL, *menu=NULL;
  GtkWidget *item=NULL;
  int i;

  pass_along[0] = vw;
  pass_along[1] = vlp;
  pass_along[2] = vvp;
  pass_along[3] = vtl;
  pass_along[4] = trk;

  for ( i = 0; i < N_FILTERS; i++ ) {
    if ( filters[i]->inputtype == inputtype ) {
      if ( ! menu_item ) { /* do this just once, but return NULL if no filters */
        menu = gtk_menu_new();
        menu_item = gtk_menu_item_new_with_mnemonic ( menu_title );
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), menu );
      }

      item = gtk_menu_item_new_with_label ( filters[i]->window_title );
      g_object_set_data ( G_OBJECT(item), "vik_acq_iface", (void *) filters[i] );
      g_signal_connect ( G_OBJECT(item), "activate", G_CALLBACK(acquire_trwlayer_callback), pass_along );
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show ( item );
    }
  }

  return menu_item;
}

/**
 * a_acquire_trwlayer_menu:
 *
 * Create a sub menu intended for rightclicking on a TRWLayer's menu called "Filter".
 *
 * Returns: %NULL if no filters.
 */
GtkWidget *a_acquire_trwlayer_menu (VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp, VikTrwLayer *vtl)
{
  return acquire_build_menu ( vw, vlp, vvp, vtl, NULL, _("_Filter"), VIK_DATASOURCE_INPUTTYPE_TRWLAYER );
}

/**
 * a_acquire_trwlayer_track_menu:
 *
 * Create a sub menu intended for rightclicking on a TRWLayer's menu called "Filter with Track "TRACKNAME"...".
 *
 * Returns: %NULL if no filters or no filter track has been set.
 */
GtkWidget *a_acquire_trwlayer_track_menu (VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp, VikTrwLayer *vtl)
{
  if ( filter_track == NULL )
    return NULL;
  else {
    char *menu_title = g_strdup_printf ( _("Filter with %s"), filter_track->name );
    GtkWidget *rv = acquire_build_menu ( vw, vlp, vvp, vtl, filter_track,
			menu_title, VIK_DATASOURCE_INPUTTYPE_TRWLAYER_TRACK );
    free( menu_title );
    return rv;
  }
}

/**
 * a_acquire_track_menu:
 *
 * Create a sub menu intended for rightclicking on a track's menu called "Filter".
 *
 * Returns: %NULL if no applicable filters
 */
GtkWidget *a_acquire_track_menu (VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp, Track * trk)
{
  return acquire_build_menu ( vw, vlp, vvp, NULL, trk, _("Filter"), VIK_DATASOURCE_INPUTTYPE_TRACK );
}

/**
 * a_acquire_set_filter_track:
 *
 * Sets application-wide track to use with filter. references the track.
 */
void a_acquire_set_filter_track(Track * trk)
{
	if (filter_track) {
		filter_track->free();
	}

	filter_track = trk;
	trk->ref();
}
