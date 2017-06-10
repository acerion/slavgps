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

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "babel.h"
#include "gpx.h"
#include "acquire.h"
#include "dialog.h"
#include "util.h"




using namespace SlavGPS;



extern VikDataSourceInterface vik_datasource_gps_interface;
extern VikDataSourceInterface vik_datasource_geojson_interface;
extern VikDataSourceInterface vik_datasource_routing_interface;
extern VikDataSourceInterface vik_datasource_osm_interface;
extern VikDataSourceInterface vik_datasource_osm_my_traces_interface;
extern VikDataSourceInterface vik_datasource_geotag_interface;
extern VikDataSourceInterface vik_datasource_wikipedia_interface;
extern VikDataSourceInterface vik_datasource_url_interface;



/************************ FILTER LIST *******************/
// extern VikDataSourceInterface vik_datasource_gps_interface;
#ifdef K
/*** Input is LayerTRW. ***/
extern VikDataSourceInterface vik_datasource_bfilter_simplify_interface;
extern VikDataSourceInterface vik_datasource_bfilter_compress_interface;
extern VikDataSourceInterface vik_datasource_bfilter_dup_interface;
extern VikDataSourceInterface vik_datasource_bfilter_manual_interface;

/*** Input is a track and a LayerTRW. ***/
extern VikDataSourceInterface vik_datasource_bfilter_polygon_interface;
extern VikDataSourceInterface vik_datasource_bfilter_exclude_polygon_interface;
#endif

/*** Input is a track. ***/
#ifdef K
const VikDataSourceInterface * filters[] = {

	&vik_datasource_bfilter_simplify_interface,
	&vik_datasource_bfilter_compress_interface,
	&vik_datasource_bfilter_dup_interface,
	&vik_datasource_bfilter_manual_interface,
	&vik_datasource_bfilter_polygon_interface,
	&vik_datasource_bfilter_exclude_polygon_interface,

};

const unsigned int N_FILTERS = sizeof(filters) / sizeof(filters[0]);
#endif

Track * filter_track = NULL;

/********************************************************/

/* Passed along to worker thread. */
typedef struct {
	acq_dialog_widgets_t * w;
	ProcessOptions * po;
	bool creating_new_layer;
	LayerTRW * trw;
	DownloadFileOptions * options;
} w_and_interface_t;




/*********************************************************
 * Definitions and routines for acquiring data from Data Sources in general.
 *********************************************************/




static void progress_func(BabelProgressCode c, void * data, acq_dialog_widgets_t * w)
{
#ifdef K
	if (w->source_interface->is_thread) {
		gdk_threads_enter();
		if (!w->running) {
			if (w->source_interface->cleanup_func) {
				w->source_interface->cleanup_func(w->user_data);
			}
			gdk_threads_leave();
			g_thread_exit(NULL);
		}
		gdk_threads_leave();
	}

	if (w->source_interface->progress_func) {
		w->source_interface->progress_func(c, data, w);
	}
#endif
}




/**
 * Some common things to do on completion of a datasource process
 *  . Update layer
 *  . Update dialog info
 *  . Update main dsisplay
 */
static void on_complete_process(w_and_interface_t * wi)
{
	if (wi->w->running) {
#ifdef K
		gtk_label_set_text(GTK_LABEL(wi->w->status), _("Done."));
#endif
		if (wi->creating_new_layer) {
			/* Only create the layer if it actually contains anything useful. */
			/* TODO: create function for this operation to hide detail: */
			if (! wi->trw->is_empty()) {
				wi->trw->post_read(wi->w->viewport, true);
				Layer * layer = wi->trw;
				wi->w->panel->get_top_layer()->add_layer(layer, true);
			} else {
#ifdef K
				gtk_label_set_text(GTK_LABEL(wi->w->status), _("No data."));
#endif
			}
		}
#ifdef K
		if (wi->w->source_interface->keep_dialog_open) {
			gtk_dialog_set_response_sensitive(GTK_DIALOG(wi->w->dialog), GTK_RESPONSE_ACCEPT, true);
			gtk_dialog_set_response_sensitive(GTK_DIALOG(wi->w->dialog), GTK_RESPONSE_REJECT, false);
		} else {
			gtk_dialog_response(GTK_DIALOG(wi->w->dialog), GTK_RESPONSE_ACCEPT);
		}
#endif
		/* Main display update. */
		if (wi->trw) {
			wi->trw->post_read(wi->w->viewport, true);
			/* View this data if desired - must be done after post read (so that the bounds are known). */
			if (wi->w->source_interface->autoview) {
				wi->trw->auto_set_view(wi->w->panel->get_viewport());
			}
			wi->w->panel->emit_update_cb();
		}
	} else {
		/* Cancelled. */
		if (wi->creating_new_layer) {
			wi->trw->unref();
		}
	}
}



ProcessOptions::ProcessOptions()
{
}


ProcessOptions::ProcessOptions(const char * args, const char * file_name, const char * file_type, const char * input_url)
{
	if (args) {
		this->babelargs = strdup(args);
	}
	if (filename) {
		this->filename = strdup(file_name);
	}
	if (file_type) {
		this->input_file_type = strdup(file_type);
	}
	if (input_url) {
		this->url = strdup(input_url);
	}
}


ProcessOptions::~ProcessOptions()
{
	free(this->babelargs);
	free(this->filename);
	free(this->input_file_type);
	free(this->babel_filters);
	free(this->url);
	free(this->shell_command);
}




/* This routine is the worker thread.  there is only one simultaneous download allowed. */
static void get_from_anything(w_and_interface_t * wi)
{
	bool result = true;

	VikDataSourceInterface * source_interface = wi->w->source_interface;

	if (source_interface->process_func) {
		result = source_interface->process_func(wi->trw, wi->po, (BabelStatusFunc)progress_func, wi->w, wi->options);
	}
	delete wi->po;
	free(wi->options);
#ifdef K
	if (wi->w->running && !result) {
		gdk_threads_enter();
		gtk_label_set_text(GTK_LABEL(wi->w->status), _("Error: acquisition failed."));
		if (wi->creating_new_layer) {
			wi->trw->unref();
		}
		gdk_threads_leave();
	} else {
		gdk_threads_enter();
		on_complete_process(wi);
		gdk_threads_leave();
	}
#endif

	if (source_interface->cleanup_func) {
		source_interface->cleanup_func(wi->w->user_data);
	}

	if (wi->w->running) {
		wi->w->running = false;
	} else {
		free(wi->w);
		free(wi);
		wi = NULL;
	}

	g_thread_exit(NULL);
}




/* Depending on type of filter, often only trw or track will be given.
 * The other can be NULL.
 */
static void acquire(Window * window,
		    LayersPanel * panel,
		    Viewport * viewport,
		    vik_datasource_mode_t mode,
		    VikDataSourceInterface *source_interface,
		    LayerTRW * trw,
		    Track * trk,
		    void * userdata,
		    VikDataSourceCleanupFunc cleanup_function)
{
	/* For manual dialogs. */
	GtkWidget * dialog = NULL;
	GtkWidget * status;
	char * args_off = NULL;
	char * fd_off = NULL;
	void * user_data;
	DownloadFileOptions * options = (DownloadFileOptions *) malloc(sizeof (DownloadFileOptions));
	memset(options, 0, sizeof (DownloadFileOptions));

	acq_vik_t avt;
	avt.panel = panel;
	avt.viewport = viewport;
	avt.window = window;
	avt.userdata = userdata;

	/* For UI builder. */
	void * pass_along_data;
	ParameterValue *paramdatas = NULL;

	/*** INIT AND CHECK EXISTENCE ***/
	if (source_interface->init_func) {
		user_data = source_interface->init_func(&avt);
	} else {
		user_data = NULL;
	}
	pass_along_data = user_data;

	if (source_interface->check_existence_func) {
		char *error_str = source_interface->check_existence_func();
		if (error_str) {
			dialog_error(error_str, window);
			free(error_str);
			return;
		}
	}

	/* BUILD UI & GET OPTIONS IF NECESSARY. */


	if (source_interface->add_setup_widgets_func) {
		source_interface->add_setup_widgets_func(dialog, viewport, user_data);
	}

#ifdef K
	/* POSSIBILITY 0: NO OPTIONS. DO NOTHING HERE. */
	/* POSSIBILITY 1: ADD_SETUP_WIDGETS_FUNC */
	if (source_interface->add_setup_widgets_func) {
		dialog = gtk_dialog_new_with_buttons("", window->get_toolkit_window(), (GtkDialogFlags) 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);

		gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
		GtkWidget *response_w = NULL;
#if GTK_CHECK_VERSION (2, 20, 0)
		response_w = gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
#endif

		source_interface->add_setup_widgets_func(dialog, viewport, user_data);
		gtk_window_set_title(GTK_WINDOW(dialog), _(source_interface->window_title));

		if (response_w) {
			gtk_widget_grab_focus(response_w);
		}

		if (gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_ACCEPT) {
			source_interface->cleanup_func(user_data);
			gtk_widget_destroy(dialog);
			return;
		}
	}
	/* POSSIBILITY 2: UI BUILDER */
	else if (source_interface->params) {
		paramdatas = a_uibuilder_run_dialog(source_interface->window_title, window->get_toolkit_window(),
						    source_interface->params, source_interface->params_count,
						    source_interface->params_groups, source_interface->params_groups_count,
						    source_interface->params_defaults);
		if (paramdatas) {
			pass_along_data = paramdatas;
		} else {
			return; /* TODO: do we have to free anything here? */
		}
	}
#endif

	/* CREATE INPUT DATA & GET OPTIONS */
	ProcessOptions * po = NULL;

	if (source_interface->inputtype == VIK_DATASOURCE_INPUTTYPE_TRWLAYER) {
		char * name_src = a_gpx_write_tmp_file(trw, NULL);

		po = source_interface->get_process_options(pass_along_data, NULL, name_src, NULL);

		util_add_to_deletion_list(name_src);

		free(name_src);
	} else if (source_interface->inputtype == VIK_DATASOURCE_INPUTTYPE_TRWLAYER_TRACK) {
		char * name_src = a_gpx_write_tmp_file(trw, NULL);
		char * name_src_track = a_gpx_write_track_tmp_file(trk, NULL);

		po = source_interface->get_process_options(pass_along_data, NULL, name_src, name_src_track);

		util_add_to_deletion_list(name_src);
		util_add_to_deletion_list(name_src_track);

		free(name_src);
		free(name_src_track);
	} else if (source_interface->inputtype == VIK_DATASOURCE_INPUTTYPE_TRACK) {
		char *name_src_track = a_gpx_write_track_tmp_file(trk, NULL);

		po = source_interface->get_process_options(pass_along_data, NULL, NULL, name_src_track);

		free(name_src_track);
	} else if (source_interface->get_process_options) {
		po = source_interface->get_process_options(pass_along_data, options, NULL, NULL);
	} else {
		/* kamil: what now? */
	}

	/* Get data for Off command. */
	if (source_interface->off_func) {
		source_interface->off_func(pass_along_data, &args_off, &fd_off);
	}
#ifdef K
	/* Cleanup for option dialogs. */
	if (source_interface->add_setup_widgets_func) {
		gtk_widget_destroy(dialog);
		dialog = NULL;
	} else if (source_interface->params) {
		a_uibuilder_free_paramdatas(paramdatas, source_interface->params, source_interface->params_count);
	}
#endif

	acq_dialog_widgets_t * w = (acq_dialog_widgets_t *) malloc(sizeof (acq_dialog_widgets_t));
	w_and_interface_t * wi = (w_and_interface_t *) malloc(sizeof (w_and_interface_t));
	wi->w = w;
	wi->w->source_interface = source_interface;
	wi->po = po;
	wi->options = options;
	wi->trw = trw;
	wi->creating_new_layer = (!trw); /* Default if Auto Layer Management is passed in. */

#ifdef K
	dialog = gtk_dialog_new_with_buttons("", window->get_toolkit_window(), (GtkDialogFlags) 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT, false);
	gtk_window_set_title(GTK_WINDOW(dialog), _(source_interface->window_title));

	w->dialog = dialog;
	w->running = true;
	status = gtk_label_new(_("Working..."));
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), status, false, false, 5);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	/* May not want to see the dialog at all. */
	if (source_interface->is_thread || source_interface->keep_dialog_open) {
		gtk_widget_show_all(dialog);
	}
	w->status = status;
#endif

	w->window = window;
	w->panel = panel;
	w->viewport = viewport;
	if (source_interface->add_progress_widgets_func) {
		source_interface->add_progress_widgets_func(dialog, user_data);
	}
	w->user_data = user_data;


	if (mode == VIK_DATASOURCE_ADDTOLAYER) {
		Layer * current_selected = w->panel->get_selected_layer();
		if (current_selected->type == LayerType::TRW) {
			wi->trw = (LayerTRW *) current_selected;
			wi->creating_new_layer = false;
		}
	} else if (mode == VIK_DATASOURCE_CREATENEWLAYER) {
		wi->creating_new_layer = true;
	} else if (mode == VIK_DATASOURCE_MANUAL_LAYER_MANAGEMENT) {
		/* Don't create in acquire - as datasource will perform the necessary actions. */
		wi->creating_new_layer = false;
		Layer * current_selected = w->panel->get_selected_layer();
		if (current_selected->type == LayerType::TRW) {
			wi->trw = (LayerTRW *) current_selected;
		}
	}
	if (wi->creating_new_layer) {
		wi->trw = new LayerTRW();
		wi->trw->set_coord_mode(w->viewport->get_coord_mode());
#ifdef K
		wi->trw->rename(_(source_interface->layer_title));
#endif
	}

#ifdef K
	if (source_interface->is_thread) {
		if (po->babelargs || po->url || po->shell_command) {
#if GLIB_CHECK_VERSION (2, 32, 0)
			g_thread_try_new("get_from_anything", (GThreadFunc)get_from_anything, wi, NULL);
#else
			g_thread_create((GThreadFunc)get_from_anything, wi, false, NULL);
#endif
			gtk_dialog_run(GTK_DIALOG(dialog));
			if (w->running) {
				/* Cancel and mark for thread to finish. */
				w->running = false;
				/* NB Thread will free memory. */
			} else {
				if (args_off) {
					/* Turn off. */
					ProcessOptions off_po(args_off, fd_off, NULL, NULL); /* kamil FIXME: memory leak through these pointers? */
					a_babel_convert_from(NULL, &off_po, NULL, NULL, NULL);
					free(args_off);
				}
				if (fd_off) {
					free(fd_off);
				}

				/* Thread finished by normal completion - free memory. */
				free(w);
				free(wi);
			}
		} else {
			/* This shouldn't happen... */
			gtk_label_set_text(GTK_LABEL(w->status), _("Unable to create command\nAcquire method failed."));
			gtk_dialog_run(GTK_DIALOG (dialog));
		}
	} else {
#endif
		/* Bypass thread method malarkly - you'll just have to wait... */
		if (source_interface->process_func) {
			bool result = source_interface->process_func(wi->trw, po, (BabelStatusFunc) progress_func, w, options);
			if (!result) {
				dialog_error(QString(_("Error: acquisition failed.")), window);
			}
		}
		delete po;
		free(options);

		on_complete_process(wi);
#ifdef K
		/* Actually show it if necessary. */
		if (wi->w->source_interface->keep_dialog_open) {
			gtk_dialog_run(GTK_DIALOG(dialog));
		}
#endif

		free(w);
		free(wi);
#ifdef K
	}
#endif


#ifdef K
	gtk_widget_destroy(dialog);

	if (cleanup_function) {
		cleanup_function(source_interface);
	}
#endif
}




/**
 * @window: The #Window to work with
 * @panel: The #LayersPanel in which a #LayerTRW layer may be created/appended
 * @viewport: The #Viewport defining the current view
 * @mode: How layers should be managed
 * @source_interface: The #VikDataSourceInterface determining how and what actions to take
 * @userdata: External data to be passed into the #VikDataSourceInterface
 * @cleanup_function: The function to dispose the #VikDataSourceInterface if necessary
 *
 * Process the given VikDataSourceInterface for sources with no input data.
 */
void SlavGPS::a_acquire(Window * window,
			LayersPanel * panel,
			Viewport * viewport,
			vik_datasource_mode_t mode,
			VikDataSourceInterface *source_interface,
			void * userdata,
			VikDataSourceCleanupFunc cleanup_function)
{
	acquire(window, panel, viewport, mode, source_interface, NULL, NULL, userdata, cleanup_function);
}




typedef struct {
	Window * window;
	LayersPanel * panel;
	Viewport * viewport;
	LayerTRW * trw;
	Track * trk;
} pass_along;




static void acquire_trwlayer_callback(GObject *menuitem, pass_along * data)
{
#ifdef K
	VikDataSourceInterface * iface = (VikDataSourceInterface *) g_object_get_data (menuitem, "vik_acq_iface");

	acquire(data->window, data->panel, data->viewport, iface->mode, iface, data->trw, data->trk, NULL, NULL);
#endif
}




static GtkWidget * acquire_build_menu(Window * window, LayersPanel * panel, Viewport * viewport,
				      LayerTRW * trw, Track * trk, /* Both passed to acquire, although for many filters only one is necessary. */
				      const char *menu_title, vik_datasource_inputtype_t inputtype)
{
	GtkWidget * menu_item = NULL;
	GtkWidget * menu = NULL;
	GtkWidget * item = NULL;

	static pass_along data = {
		window,
		panel,
		viewport,
		trw,
		trk
	};

#ifdef K
	for (unsigned int i = 0; i < N_FILTERS; i++) {
		if (filters[i]->inputtype == inputtype) {
			if (! menu_item) { /* Do this just once, but return NULL if no filters. */
				menu = gtk_menu_new();
				menu_item = gtk_menu_item_new_with_mnemonic(menu_title);
				gtk_menu_item_set_submenu(GTK_MENU_ITEM (menu_item), menu);
			}

			action = QAction(QString(filters[i]->window_title), this);
			g_object_set_data(G_OBJECT(item), "vik_acq_iface", (void *) filters[i]);
			QObject::connect(action, SIGNAL (triggered(bool)), &data, SLOT (acquire_trwlayer_callback));
			menu->addAction(action);
		}
	}
#endif

	return menu_item;
}




/**
 * Create a sub menu intended for rightclicking on a TRWLayer's menu called "Filter".
 *
 * Returns: %NULL if no filters.
 */
GtkWidget * SlavGPS::a_acquire_trwlayer_menu(Window * window, LayersPanel * panel, Viewport * viewport, LayerTRW * trw)
{
	return acquire_build_menu(window, panel, viewport, trw, NULL, _("_Filter"), VIK_DATASOURCE_INPUTTYPE_TRWLAYER);
}




/**
 * Create a sub menu intended for rightclicking on a TRWLayer's menu called "Filter with Track "TRACKNAME"...".
 *
 * Returns: %NULL if no filters or no filter track has been set.
 */
GtkWidget * SlavGPS::a_acquire_trwlayer_track_menu(Window * window, LayersPanel * panel, Viewport * viewport, LayerTRW * trw)
{
	if (filter_track == NULL) {
		return NULL;
	} else {
		char *menu_title = g_strdup_printf(_("Filter with %s"), filter_track->name);
		GtkWidget *rv = acquire_build_menu(window, panel, viewport, trw, filter_track,
						   menu_title, VIK_DATASOURCE_INPUTTYPE_TRWLAYER_TRACK);
		free(menu_title);
		return rv;
	}
}




/**
 * Create a sub menu intended for rightclicking on a track's menu called "Filter".
 *
 * Returns: %NULL if no applicable filters
 */
GtkWidget * SlavGPS::a_acquire_track_menu(Window * window, LayersPanel * panel, Viewport * viewport, Track * trk)
{
	return acquire_build_menu(window, panel, viewport, NULL, trk, _("Filter"), VIK_DATASOURCE_INPUTTYPE_TRACK);
}




/**
 * Sets application-wide track to use with filter. references the track.
 */
void SlavGPS::a_acquire_set_filter_track(Track * trk)
{
	if (filter_track) {
		filter_track->free();
	}

	filter_track = trk;
	trk->ref();
}
