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

#include "window.h"
#include "viewport_internal.h"
#include "layers_panel.h"
#include "babel.h"
#include "gpx.h"
#include "acquire.h"
#include "dialog.h"
#include "util.h"
#include "layer_aggregate.h"




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
/*** Input is LayerTRW. ***/
extern VikDataSourceInterface vik_datasource_bfilter_simplify_interface;
extern VikDataSourceInterface vik_datasource_bfilter_compress_interface;
extern VikDataSourceInterface vik_datasource_bfilter_dup_interface;
extern VikDataSourceInterface vik_datasource_bfilter_manual_interface;

/*** Input is a track and a LayerTRW. ***/
extern VikDataSourceInterface vik_datasource_bfilter_polygon_interface;
extern VikDataSourceInterface vik_datasource_bfilter_exclude_polygon_interface;

/*** Input is a track. ***/
const VikDataSourceInterface * filters[] = {
	&vik_datasource_bfilter_simplify_interface,
	&vik_datasource_bfilter_compress_interface,
	&vik_datasource_bfilter_dup_interface,
	&vik_datasource_bfilter_manual_interface,
	&vik_datasource_bfilter_polygon_interface,
	&vik_datasource_bfilter_exclude_polygon_interface,

};

const int N_FILTERS = sizeof(filters) / sizeof(filters[0]);

Track * filter_track = NULL;

/********************************************************/

/* Passed along to worker thread. */
typedef struct {
	AcquireProcess * acquiring;
	ProcessOptions * po;
	bool creating_new_layer;
	LayerTRW * trw;
	DownloadOptions * dl_options;
} w_and_interface_t;



static AcquireProcess * g_acquiring = NULL;


/*********************************************************
 * Definitions and routines for acquiring data from Data Sources in general.
 *********************************************************/




static void progress_func(BabelProgressCode c, void * data, AcquireProcess * acquiring)
{
	if (acquiring->source_interface->is_thread) {
#ifdef K
		gdk_threads_enter();
		if (!acquiring->running) {
			if (acquiring->source_interface->cleanup_func) {
				acquiring->source_interface->cleanup_func(acquiring->user_data);
			}
			gdk_threads_leave();
			g_thread_exit(NULL);
		}
		gdk_threads_leave();
#endif
	}

	if (acquiring->source_interface->progress_func) {
		acquiring->source_interface->progress_func(c, data, acquiring);
	}
}




/**
 * Some common things to do on completion of a datasource process
 *  . Update layer
 *  . Update dialog info
 *  . Update main dsisplay
 */
static void on_complete_process(w_and_interface_t * wi)
{
	if (
#ifdef K
	    wi->acquiring->running
#else
	    true
#endif
	    ) {
		wi->acquiring->status.setText(QObject::tr("Done."));
		if (wi->creating_new_layer) {
			/* Only create the layer if it actually contains anything useful. */
			/* TODO: create function for this operation to hide detail: */
			if (! wi->trw->is_empty()) {
				wi->trw->post_read(wi->acquiring->viewport, true);
				Layer * layer = wi->trw;
				wi->acquiring->panel->get_top_layer()->add_layer(layer, true);
			} else {
				wi->acquiring->status.setText(QObject::tr("No data.")); /* TODO: where do we display thins message? */
			}
		}
		if (wi->acquiring->source_interface->keep_dialog_open) {
#ifdef K
			gtk_dialog_set_response_sensitive(GTK_DIALOG(wi->acquiring->dialog), GTK_RESPONSE_ACCEPT, true);
			gtk_dialog_set_response_sensitive(GTK_DIALOG(wi->acquiring->dialog), GTK_RESPONSE_REJECT, false);
#endif
		} else {
#ifdef K
			gtk_dialog_response(GTK_DIALOG(wi->acquiring->dialog), GTK_RESPONSE_ACCEPT);
#endif
		}

		/* Main display update. */
		if (wi->trw) {
			wi->trw->post_read(wi->acquiring->viewport, true);
			/* View this data if desired - must be done after post read (so that the bounds are known). */
			if (wi->acquiring->source_interface->autoview) {
				wi->trw->auto_set_view(wi->acquiring->panel->get_viewport());
			}
			wi->acquiring->panel->emit_update_cb();
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

	VikDataSourceInterface * source_interface = wi->acquiring->source_interface;

	if (source_interface->process_func) {
		result = source_interface->process_func(wi->trw, wi->po, (BabelStatusFunc)progress_func, wi->acquiring, wi->dl_options);
	}
	delete wi->po;
	delete wi->dl_options;

	if (wi->acquiring->running && !result) {
#ifdef K
		gdk_threads_enter();
		wi->acquiring->status.setText(QObject::tr("Error: acquisition failed."));
		if (wi->creating_new_layer) {
			wi->trw->unref();
		}
		gdk_threads_leave();
#endif
	} else {
#ifdef K
		gdk_threads_enter();
		on_complete_process(wi);
		gdk_threads_leave();
#endif
	}

	if (source_interface->cleanup_func) {
		source_interface->cleanup_func(wi->acquiring->user_data);
	}

	if (wi->acquiring->running) {
		wi->acquiring->running = false;
	} else {
		free(wi);
		wi = NULL;
	}

	g_thread_exit(NULL);
}




/* Depending on type of filter, often only trw or track will be given.
 * The other can be NULL.
 */
void AcquireProcess::acquire(DatasourceMode mode, VikDataSourceInterface * source_interface_, void * userdata, VikDataSourceCleanupFunc cleanup_function)
{
	/* For manual dialogs. */
	GtkWidget * setup_dialog = NULL;
	char * args_off = NULL;
	char * fd_off = NULL;
	DownloadOptions * dl_options = new DownloadOptions;

	acq_vik_t avt;
	avt.panel = this->panel;
	avt.viewport = this->viewport;
	avt.window = this->window;
	avt.userdata = userdata;

	/* For UI builder. */
	void * pass_along_data;
	SGVariant *paramdatas = NULL;

	/*** INIT AND CHECK EXISTENCE ***/
	if (source_interface_->init_func) {
		this->user_data = source_interface_->init_func(&avt);
	} else {
		this->user_data = NULL;
	}
	pass_along_data = this->user_data;

	if (source_interface_->check_existence_func) {
		char *error_str = source_interface_->check_existence_func();
		if (error_str) {
			Dialog::error(error_str, this->window);
			free(error_str);
			return;
		}
	}

	/* BUILD UI & GET OPTIONS IF NECESSARY. */


	if (source_interface_->internal_dialog) {

		/*
		  Data interfaces that "use" this branch of code:
		  vik_datasource_osm_interface;
		  vik_datasource_wikipedia_interface;


		  Data interfaces that don't "use" this branch of code (yet?):
		  vik_datasource_gps_interface;
		  vik_datasource_geojson_interface;
		  vik_datasource_routing_interface;
		  vik_datasource_osm_my_traces_interface;
		  vik_datasource_geotag_interface;
		  vik_datasource_url_interface;
		*/

		int rv = source_interface_->internal_dialog(avt.window);
		if (rv != QDialog::Accepted) {
			if (source_interface_->cleanup_func) {
				source_interface_->cleanup_func(this->user_data);
			}
			return;
		}
	} else

	/* POSSIBILITY 0: NO OPTIONS. DO NOTHING HERE. */
	/* POSSIBILITY 1: ADD_SETUP_WIDGETS_FUNC */
	if (source_interface_->add_setup_widgets_func) {
#ifdef K
		setup_dialog = gtk_dialog_new_with_buttons("", this->window, (GtkDialogFlags) 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);

		gtk_dialog_set_default_response(GTK_DIALOG(setup_dialog), GTK_RESPONSE_ACCEPT);
		GtkWidget *response_w = NULL;
#if GTK_CHECK_VERSION (2, 20, 0)
		response_w = gtk_dialog_get_widget_for_response(GTK_DIALOG(setup_dialog), GTK_RESPONSE_ACCEPT);
#endif

		source_interface_->add_setup_widgets_func(setup_dialog, this->viewport, this->user_data);
		gtk_window_set_title(GTK_WINDOW(setup_dialog), _(source_interface_->window_title));

		if (response_w) {
			gtk_widget_grab_focus(response_w);
		}

		if (gtk_dialog_run(GTK_DIALOG(setup_dialog)) != GTK_RESPONSE_ACCEPT) {
			source_interface_->cleanup_func(this->user_data);
			gtk_widget_destroy(setup_dialog);
			return;
		}
#endif
	}
	/* POSSIBILITY 2: UI BUILDER */
	else if (source_interface_->params) {
#ifdef K
		paramdatas = a_uibuilder_run_dialog(source_interface_->window_title, this->window,
						    source_interface_->params, source_interface_->params_count,
						    source_interface_->params_groups, source_interface_->params_groups_count,
						    source_interface_->params_defaults);
#endif
		if (paramdatas) {
			pass_along_data = paramdatas;
		} else {
			return; /* TODO: do we have to free anything here? */
		}
	}

	/* CREATE INPUT DATA & GET OPTIONS */
	ProcessOptions * po = NULL;

	if (source_interface_->inputtype == DatasourceInputtype::TRWLAYER) {
		char * name_src = a_gpx_write_tmp_file(this->trw, NULL);

		po = source_interface_->get_process_options(pass_along_data, NULL, name_src, NULL);

		util_add_to_deletion_list(name_src);

		free(name_src);
	} else if (source_interface_->inputtype == DatasourceInputtype::TRWLAYER_TRACK) {
		char * name_src = a_gpx_write_tmp_file(this->trw, NULL);
		char * name_src_track = a_gpx_write_track_tmp_file(this->trk, NULL);

		po = source_interface_->get_process_options(pass_along_data, NULL, name_src, name_src_track);

		util_add_to_deletion_list(name_src);
		util_add_to_deletion_list(name_src_track);

		free(name_src);
		free(name_src_track);
	} else if (source_interface_->inputtype == DatasourceInputtype::TRACK) {
		char *name_src_track = a_gpx_write_track_tmp_file(this->trk, NULL);

		po = source_interface_->get_process_options(pass_along_data, NULL, NULL, name_src_track);

		free(name_src_track);
	} else if (source_interface_->get_process_options) {
		po = source_interface_->get_process_options(pass_along_data, dl_options, NULL, NULL);
	} else {
		/* kamil: what now? */
	}

	/* Get data for Off command. */
	if (source_interface_->off_func) {
		source_interface_->off_func(pass_along_data, &args_off, &fd_off);
	}

	/* Cleanup for option dialogs. */
	if (source_interface_->add_setup_widgets_func) {
#ifdef K
		gtk_widget_destroy(setup_dialog);
		setup_dialog = NULL;
#endif
	} else if (source_interface_->params) {
#ifdef K
		a_uibuilder_free_paramdatas(paramdatas, source_interface_->params, source_interface_->params_count);
#endif
	}

	w_and_interface_t * wi = (w_and_interface_t *) malloc(sizeof (w_and_interface_t));
	wi->acquiring = this;
	wi->acquiring->source_interface = source_interface_;
	wi->po = po;
	wi->dl_options = dl_options;
	wi->trw = this->trw;
	wi->creating_new_layer = (!this->trw); /* Default if Auto Layer Management is passed in. */

#ifdef K
	setup_dialog = gtk_dialog_new_with_buttons("", this->window, (GtkDialogFlags) 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(setup_dialog), GTK_RESPONSE_ACCEPT, false);
	gtk_window_set_title(GTK_WINDOW(setup_dialog), _(source_interface_->window_title));

	this->dialog = setup_dialog;
	this->running = true;
	QLabel * status = new QLabel(QObject::tr("Working..."));
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(setup_dialog))), status, false, false, 5);
	gtk_dialog_set_default_response(GTK_DIALOG(setup_dialog), GTK_RESPONSE_ACCEPT);
	/* May not want to see the dialog at all. */
	if (source_interface_->is_thread || source_interface_->keep_dialog_open) {
		gtk_widget_show_all(setup_dialog);
	}
	this->status = status;
#endif

	if (source_interface_->add_progress_widgets_func) {
		source_interface_->add_progress_widgets_func(setup_dialog, this->user_data);
	}


	if (mode == DatasourceMode::ADDTOLAYER) {
		Layer * current_selected = this->panel->get_selected_layer();
		if (current_selected->type == LayerType::TRW) {
			wi->trw = (LayerTRW *) current_selected;
			wi->creating_new_layer = false;
		}
	} else if (mode == DatasourceMode::CREATENEWLAYER) {
		wi->creating_new_layer = true;
	} else if (mode == DatasourceMode::MANUAL_LAYER_MANAGEMENT) {
		/* Don't create in acquire - as datasource will perform the necessary actions. */
		wi->creating_new_layer = false;
		Layer * current_selected = this->panel->get_selected_layer();
		if (current_selected->type == LayerType::TRW) {
			wi->trw = (LayerTRW *) current_selected;
		}
	}
	if (wi->creating_new_layer) {
		wi->trw = new LayerTRW();
		wi->trw->set_coord_mode(this->viewport->get_coord_mode());
		wi->trw->rename(_(source_interface_->layer_title));
	}

#ifdef K
	if (source_interface_->is_thread) {
		if (po->babelargs || po->url || po->shell_command) {
#if GLIB_CHECK_VERSION (2, 32, 0)
			g_thread_try_new("get_from_anything", (GThreadFunc)get_from_anything, wi, NULL);
#else
			g_thread_create((GThreadFunc)get_from_anything, wi, false, NULL);
#endif
			gtk_dialog_run(GTK_DIALOG(setup_dialog));
			if (this->running) {
				/* Cancel and mark for thread to finish. */
				this->running = false;
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
			this->status.setText(QObject::tr("Unable to create command\nAcquire method failed."));
			gtk_dialog_run(GTK_DIALOG (setup_dialog));
		}
	} else {
#endif
		/* Bypass thread method malarkly - you'll just have to wait... */
		if (source_interface_->process_func) {
			bool success = source_interface_->process_func(wi->trw, po, (BabelStatusFunc) progress_func, this, dl_options);
			if (!success) {
				Dialog::error(tr("Error: acquisition failed."), this->window);
			}
		}
		delete po;
		delete dl_options;

		on_complete_process(wi);
#ifdef K
		/* Actually show it if necessary. */
		if (wi->this->source_interface_->keep_dialog_open) {
			gtk_dialog_run(GTK_DIALOG(setup_dialog));
		}
#endif

		free(wi);
#ifdef K
	}
#endif


#ifdef K
	gtk_widget_destroy(setup_dialog);

	if (cleanup_function) {
		cleanup_function(source_interface_);
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
void SlavGPS::a_acquire(Window * window, LayersPanel * panel, Viewport * viewport, DatasourceMode mode, VikDataSourceInterface *source_interface, void * userdata, VikDataSourceCleanupFunc cleanup_function)
{
	g_acquiring->window = window;
	g_acquiring->panel = panel;
	g_acquiring->viewport = viewport;
	g_acquiring->trw = NULL;
	g_acquiring->trk = NULL;

	g_acquiring->acquire(mode, source_interface, userdata, cleanup_function);
}




void AcquireProcess::acquire_trwlayer_cb(void)
{
	QAction * qa = (QAction *) QObject::sender();
	int idx = qa->data().toInt();

	VikDataSourceInterface * iface = (VikDataSourceInterface *) filters[idx];

	this->acquire(iface->mode, iface, NULL, NULL);
}




QMenu * AcquireProcess::build_menu(const QString & submenu_label, DatasourceInputtype inputtype)
{
	QMenu * menu = NULL;

	for (int i = 0; i < N_FILTERS; i++) {
		if (filters[i]->inputtype == inputtype) {
			if (!menu) { /* Do this just once, but return NULL if no filters. */
				menu = new QMenu(submenu_label);
			}

			QAction * action = new QAction(QString(filters[i]->window_title), this);
			action->setData(i);
			QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (acquire_trwlayer_cb(void)));
			menu->addAction(action);
		}
	}

	return menu;
}




/**
 * Create a sub menu intended for rightclicking on a TRWLayer's menu called "Filter".
 *
 * Returns: %NULL if no filters.
 */
QMenu * SlavGPS::a_acquire_trwlayer_menu(Window * window, LayersPanel * panel, Viewport * viewport, LayerTRW * trw)
{
	g_acquiring->window = window;
	g_acquiring->panel = panel;
	g_acquiring->viewport = viewport;
	g_acquiring->trw = trw;
	g_acquiring->trk = NULL;

	return g_acquiring->build_menu(QObject::tr("&Filter"), DatasourceInputtype::TRWLAYER);
}




/**
 * Create a sub menu intended for rightclicking on a TRWLayer's menu called "Filter with Track "TRACKNAME"...".
 *
 * Returns: %NULL if no filters or no filter track has been set.
 */
QMenu * SlavGPS::a_acquire_trwlayer_track_menu(Window * window, LayersPanel * panel, Viewport * viewport, LayerTRW * trw)
{
	if (filter_track == NULL) {
		return NULL;
	} else {
		g_acquiring->window = window;
		g_acquiring->panel = panel;
		g_acquiring->viewport = viewport;
		g_acquiring->trw = trw;
		g_acquiring->trk = filter_track;

		const QString submenu_label = QString(QObject::tr("Filter with %1")).arg(filter_track->name);
		return g_acquiring->build_menu(submenu_label, DatasourceInputtype::TRWLAYER_TRACK);
	}
}




/**
 * Create a sub menu intended for rightclicking on a track's menu called "Filter".
 *
 * Returns: %NULL if no applicable filters
 */
QMenu * SlavGPS::a_acquire_track_menu(Window * window, LayersPanel * panel, Viewport * viewport, Track * trk)
{
	g_acquiring->window = window;
	g_acquiring->panel = panel;
	g_acquiring->viewport = viewport;
	g_acquiring->trw = NULL;
	g_acquiring->trk = trk;

	return g_acquiring->build_menu(QObject::tr("&Filter"), DatasourceInputtype::TRACK);
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




void SlavGPS::acquire_init(void)
{
	g_acquiring = new AcquireProcess();
}




void SlavGPS::acquire_uninit(void)
{
	delete g_acquiring;
}
