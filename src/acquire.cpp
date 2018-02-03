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
 */




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif




#include <cassert>




#include <QRunnable>




#include "acquire.h"
#include "window.h"
#include "viewport_internal.h"
#include "layers_panel.h"
#include "babel.h"
#include "gpx.h"
#include "dialog.h"
#include "util.h"
#include "layer_aggregate.h"
#include "layer_trw.h"
#include "layer_trw_track_internal.h"




using namespace SlavGPS;




#define PREFIX " Acquire:" << __FUNCTION__ << __LINE__ << ">"




extern DataSourceInterface datasource_gps_interface;
extern DataSourceInterface datasource_geojson_interface;
extern DataSourceInterface datasource_routing_interface;
extern DataSourceInterface datasource_geotag_interface;




/************************ FILTER LIST *******************/
// extern DataSourceInterface datasource_gps_interface;
/*** Input is LayerTRW. ***/
extern DataSourceInterface datasource_bfilter_simplify_interface;
extern DataSourceInterface datasource_bfilter_compress_interface;
extern DataSourceInterface datasource_bfilter_dup_interface;
extern DataSourceInterface datasource_bfilter_manual_interface;

/*** Input is a track and a LayerTRW. ***/
extern DataSourceInterface datasource_bfilter_polygon_interface;
extern DataSourceInterface datasource_bfilter_exclude_polygon_interface;

/*** Input is a track. ***/
const DataSourceInterface * filters[] = {
	&datasource_bfilter_simplify_interface,
	&datasource_bfilter_compress_interface,
	&datasource_bfilter_dup_interface,
	&datasource_bfilter_manual_interface,
	&datasource_bfilter_polygon_interface,
	&datasource_bfilter_exclude_polygon_interface,

};

const int N_FILTERS = sizeof(filters) / sizeof(filters[0]);

Track * filter_track = NULL;




/********************************************************/


static AcquireProcess * g_acquiring = NULL;


/*********************************************************
 * Definitions and routines for acquiring data from Data Sources in general.
 *********************************************************/




void SlavGPS::progress_func(BabelProgressCode c, void * data, AcquireProcess * acquiring)
{
	if (acquiring->source_interface && acquiring->source_interface->is_thread) {
		if (!acquiring->running) {
			if (acquiring->source_interface->cleanup_func) {
				acquiring->source_interface->cleanup_func(acquiring->user_data);
			}
		}
	}

	if (acquiring->source_interface && acquiring->source_interface->progress_func) {
		acquiring->source_interface->progress_func(c, data, acquiring);

	} else if (acquiring->data_source) {
		acquiring->data_source->progress_func(c, data, acquiring);
	}
}




/**
 * Some common things to do on completion of a datasource process
 *  . Update layer
 *  . Update dialog info
 *  . Update main dsisplay
 */
void SlavGPS::on_complete_process(AcquireGetterParams & getter_params)
{
	if (
#ifdef K
	    getter_params.acquiring->running
#else
	    true
#endif
	    ) {
		getter_params.acquiring->status->setText(QObject::tr("Done."));
		if (getter_params.creating_new_layer) {
			/* Only create the layer if it actually contains anything useful. */
			/* TODO: create function for this operation to hide detail: */
			if (!getter_params.trw->is_empty()) {
				getter_params.trw->post_read(getter_params.acquiring->viewport, true);
				Layer * layer = getter_params.trw;
				getter_params.acquiring->panel->get_top_layer()->add_layer(layer, true);
			} else {
				getter_params.acquiring->status->setText(QObject::tr("No data.")); /* TODO: where do we display thins message? */
			}
		}

		if (getter_params.acquiring->dialog_) {
			if (getter_params.acquiring->source_interface->keep_dialog_open) {
				/* Only allow dialog's validation when format selection is done. */
				getter_params.acquiring->dialog_->button_box->button(QDialogButtonBox::Ok)->setEnabled(true);
				getter_params.acquiring->dialog_->button_box->button(QDialogButtonBox::Cancel)->setEnabled(false);
			} else {
				/* Call 'accept()' slot to close the dialog. */
				getter_params.acquiring->dialog_->accept();
			}
		}

		/* Main display update. */
		if (getter_params.trw) {
			getter_params.trw->post_read(getter_params.acquiring->viewport, true);
			/* View this data if desired - must be done after post read (so that the bounds are known). */
			if (getter_params.acquiring->source_interface && getter_params.acquiring->source_interface->autoview) {
				getter_params.trw->auto_set_view(getter_params.acquiring->viewport);
			} else if (getter_params.acquiring->data_source && getter_params.acquiring->data_source->autoview) {
				getter_params.trw->auto_set_view(getter_params.acquiring->viewport);
			}
			getter_params.acquiring->panel->emit_update_window_cb("acquire completed");
		}
	} else {
		/* Cancelled. */
		if (getter_params.creating_new_layer) {
			getter_params.trw->unref();
		}
	}
}




ProcessOptions::ProcessOptions()
{
}




ProcessOptions::ProcessOptions(const QString & babel_args_, const QString & input_file_name_, const QString & input_file_type_, const QString & url_)
{
	if (!babel_args_.isEmpty()) {
		this->babel_args = babel_args_;
	}
	if (!input_file_name_.isEmpty()) {
		this->input_file_name = input_file_name_;
	}
	if (!input_file_type_.isEmpty()) {
		this->input_file_type = input_file_type_;
	}
	if (!url_.isEmpty()) {
		this->url = url_;
	}
}




ProcessOptions::~ProcessOptions()
{
}




/* This routine is the worker thread. There is only one simultaneous download allowed. */
/* Re-implementation of QRunnable::run() */
void AcquireGetter::run(void)
{
	bool result = true;

	DataSourceInterface * source_interface = this->params.acquiring->source_interface;
	DataSource * data_source = this->params.acquiring->data_source;

	if (source_interface && source_interface->process_func) {
		result = source_interface->process_func(this->params.trw, this->params.po, (BabelCallback) progress_func, this->params.acquiring, this->params.dl_options);
	} else if (data_source) {
		result = data_source->process_func(this->params.trw, this->params.po, (BabelCallback) progress_func, this->params.acquiring, this->params.dl_options);
	}

	delete this->params.po;
	delete this->params.dl_options;

	if (this->params.acquiring->running && !result) {
		this->params.acquiring->status->setText(QObject::tr("Error: acquisition failed."));
		if (this->params.creating_new_layer) {
			this->params.trw->unref();
		}
	} else {
		on_complete_process(this->params);
	}

	if (source_interface && source_interface->cleanup_func) {
		source_interface->cleanup_func(this->params.acquiring->user_data);
	}

	if (this->params.acquiring->running) {
		this->params.acquiring->running = false;
	}
}




/* Depending on type of filter, often only trw or track will be given.
 * The other can be NULL.
 */
void AcquireProcess::acquire(DataSourceMode mode, DataSourceInterface * source_interface_, void * userdata, DataSourceCleanupFunc cleanup_function)
{
	/* For manual dialogs. */
	DataSourceDialog * setup_dialog = NULL;
	DownloadOptions * dl_options = new DownloadOptions;

	this->source_interface = source_interface_;
	DataSourceInterface * interface = source_interface;

	acq_vik_t avt;
	avt.panel = this->panel;
	avt.viewport = this->viewport;
	avt.window = this->window;
	avt.userdata = userdata;

	/* For UI builder. */
	SGVariant * param_table = NULL;

	/*** INIT AND CHECK EXISTENCE ***/
	if (interface->init_func) {
		this->user_data = interface->init_func(&avt);
	} else {
		this->user_data = NULL;
	}
	void * pass_along_data = this->user_data;


	/* BUILD UI & GET OPTIONS IF NECESSARY. */


	/* POSSIBILITY 0: NO OPTIONS. DO NOTHING HERE. */
	/* POSSIBILITY 1: create "setup" dialog. */
	if (interface->create_setup_dialog_func) {

		/*
		  Data interfaces that have "create_setup_dialog_func":
		  datasource_gps_interface;
		  datasource_routing_interface;
		  datasource_geojson_interface;
		  datasource_geotag_interface;

		  Data interfaces that don't "use" this branch of code (yet?):
		  filters (bfilters)?
		*/

		setup_dialog = interface->create_setup_dialog_func(this->viewport, this->user_data);
		setup_dialog->setWindowTitle(QObject::tr(interface->window_title.toUtf8().constData()));
		/* TODO: set focus on "OK/Accept" button. */

		if (setup_dialog->exec() != QDialog::Accepted) {
			if (interface->cleanup_func) {
				interface->cleanup_func(this->user_data);
			}
			delete setup_dialog;
			return;
		}
	}
	/* POSSIBILITY 2: UI BUILDER */
	else if (interface->param_specs) {
#ifdef K
		param_table = a_uibuilder_run_dialog(interface->window_title, this->window,
						     interface->param_specs, interface->param_specs_count,
						     interface->parameter_groups, interface->params_groups_count,
						     interface->params_defaults);
#endif
		if (param_table) {
			pass_along_data = param_table;
		} else {
			return; /* TODO: do we have to free anything here? */
		}
	}

	/* CREATE INPUT DATA & GET OPTIONS */
	ProcessOptions * po = acquire_create_process_options(this, setup_dialog, dl_options, interface, pass_along_data);



	/* Cleanup for setup dialog. */
	if (interface->create_setup_dialog_func) {
		delete setup_dialog;
		setup_dialog = NULL;
	} else if (interface->param_specs) {
		a_uibuilder_free_paramdatas(param_table, interface->param_specs, interface->param_specs_count);
	}

	AcquireGetterParams getter_params;
	getter_params.acquiring = this;
	getter_params.po = po;
	getter_params.dl_options = dl_options;
	getter_params.trw = this->trw;
	getter_params.creating_new_layer = (!this->trw); /* Default if Auto Layer Management is passed in. */

#ifdef K
	setup_dialog = new BasicDialog(this->window);
	setup_dialog->button_box->button(QDialogButtonBox::Ok)->setEnabled(false);
	setup_dialog->setWindowTitle(QObject::tr(interface->window_title));
	setup_dialog->button_box->button(QDialogButtonBox::Ok)->setDefault(true);
#endif

	this->dialog_ = setup_dialog; /* TODO: setup or progress dialog? */
	this->running = true;
	this->status = new QLabel(tr("Working..."));

#ifdef K
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(setup_dialog))), status, false, false, 5);
#endif



	/* May not want to see the dialog at all. */
	if (interface->is_thread || interface->keep_dialog_open) {
#ifdef K
		gtk_widget_show_all(setup_dialog);
#endif
	}


	DataSourceDialog * progress_dialog = NULL;
	if (interface->create_progress_dialog_func) {
		progress_dialog = interface->create_progress_dialog_func(this->user_data);
	}


	switch (mode) {
	case DataSourceMode::CREATE_NEW_LAYER:
		getter_params.creating_new_layer = true;
		break;

	case DataSourceMode::ADD_TO_LAYER: {
		Layer * selected_layer = this->panel->get_selected_layer();
		if (selected_layer->type == LayerType::TRW) {
			getter_params.trw = (LayerTRW *) selected_layer;
			getter_params.creating_new_layer = false;
		}
		}
		break;

	case DataSourceMode::AUTO_LAYER_MANAGEMENT:
		/* NOOP */
		break;

	case DataSourceMode::MANUAL_LAYER_MANAGEMENT: {
		/* Don't create in acquire - as datasource will perform the necessary actions. */
		getter_params.creating_new_layer = false;
		Layer * selected_layer = this->panel->get_selected_layer();
		if (selected_layer->type == LayerType::TRW) {
			getter_params.trw = (LayerTRW *) selected_layer;
		}
		}
		break;
	default:
		qDebug() << "EE: Acquire: unexpected DataSourceMode" << (int) mode;
		break;
	};


	if (getter_params.creating_new_layer) {
		getter_params.trw = new LayerTRW();
		getter_params.trw->set_coord_mode(this->viewport->get_coord_mode());
		getter_params.trw->set_name(QObject::tr(interface->layer_title.toUtf8().constData()));

		/* We need to have that layer when completing acquisition
		   process: we need to do few operations on it. */
		this->trw = getter_params.trw;
	}

	if (interface->is_thread) {
		if (!po->babel_args.isEmpty() || !po->url.isEmpty() || !po->shell_command.isEmpty()) {

			AcquireGetter getter(getter_params);
			getter.run();

			if (progress_dialog) {
				progress_dialog->exec();
			}

			if (this->running) {
				/* Cancel and mark for thread to finish. */
				this->running = false;
				/* NB Thread will free memory. */
			} else {
				/* Get data for Off command. */
				if (interface->off_func) {
					QString babel_args_off;
					QString file_path_off;
					interface->off_func(pass_along_data, babel_args_off, file_path_off);

					if (!babel_args_off.isEmpty()) {
						/* Turn off. */
						ProcessOptions off_po(babel_args_off, file_path_off, NULL, NULL);
						a_babel_convert_from(NULL, &off_po, NULL, NULL, NULL);
					}
				}
			}
		} else {
			/* This shouldn't happen... */
			this->status->setText(QObject::tr("Unable to create command\nAcquire method failed."));
			if (progress_dialog) {
				progress_dialog->exec();
			}
		}
	} else {
		/* Bypass thread method malarkly - you'll just have to wait... */
		if (interface->process_func) {
			bool success = interface->process_func(getter_params.trw, po, (BabelCallback) progress_func, this, dl_options);
			if (!success) {
				Dialog::error(tr("Error: acquisition failed."), this->window);
			}
		}
		delete po;
		delete dl_options;

		on_complete_process(getter_params);

		/* Actually show it if necessary. */
		if (interface->keep_dialog_open) {
			if (progress_dialog) {
				progress_dialog->exec();
			}
		}
	}


	delete progress_dialog;
	delete setup_dialog;

	if (cleanup_function) {
		cleanup_function(interface);
	}
}




void AcquireProcess::acquire(DataSource * new_data_source)
{
	/* For manual dialogs. */
	DownloadOptions * dl_options = new DownloadOptions;

	this->data_source = new_data_source;

	/* For UI builder. */
	SGVariant * param_table = NULL;


	DataSourceDialog * setup_dialog = new_data_source->create_setup_dialog(this->viewport, this->user_data);
	if (setup_dialog) {
		setup_dialog->setWindowTitle(new_data_source->window_title); /* TODO: move this to dialog class. */
		/* TODO: set focus on "OK/Accept" button. */

		if (setup_dialog->exec() != QDialog::Accepted) {
			delete setup_dialog;
			return;
		}
	}

	/* CREATE INPUT DATA & GET OPTIONS */
	ProcessOptions * po = acquire_create_process_options(this, setup_dialog, dl_options, new_data_source);


	if (setup_dialog) {
		delete setup_dialog;
		setup_dialog = NULL;
	}

	AcquireGetterParams getter_params;
	getter_params.acquiring = this;
	getter_params.po = po;
	getter_params.dl_options = dl_options;
	getter_params.trw = this->trw;
	getter_params.creating_new_layer = (!this->trw); /* Default if Auto Layer Management is passed in. */


	this->dialog_ = setup_dialog; /* TODO: setup or progress dialog? */
	this->running = true;
	this->status = new QLabel(QObject::tr("Working..."));

	DataSourceDialog * progress_dialog = NULL;
#if 0
	if (new_data_source->create_progress_dialog) {
		progress_dialog = new_data_source->create_progress_dialog(this->user_data);
	}
#endif


	switch (new_data_source->mode) {
	case DataSourceMode::CREATE_NEW_LAYER:
		getter_params.creating_new_layer = true;
		break;

	case DataSourceMode::ADD_TO_LAYER: {
		Layer * selected_layer = this->panel->get_selected_layer();
		if (selected_layer->type == LayerType::TRW) {
			getter_params.trw = (LayerTRW *) selected_layer;
			getter_params.creating_new_layer = false;
		}
		}
		break;

	case DataSourceMode::AUTO_LAYER_MANAGEMENT:
		/* NOOP */
		break;

	case DataSourceMode::MANUAL_LAYER_MANAGEMENT: {
		/* Don't create in acquire - as datasource will perform the necessary actions. */
		getter_params.creating_new_layer = false;
		Layer * selected_layer = this->panel->get_selected_layer();
		if (selected_layer->type == LayerType::TRW) {
			getter_params.trw = (LayerTRW *) selected_layer;
		}
		}
		break;
	default:
		qDebug() << "EE: Acquire: unexpected DataSourceMode" << (int) new_data_source->mode;
		break;
	};


	if (getter_params.creating_new_layer) {
		getter_params.trw = new LayerTRW();
		getter_params.trw->set_coord_mode(this->viewport->get_coord_mode());
		getter_params.trw->set_name(new_data_source->layer_title);

		/* We need to have that layer when completing acquisition
		   process: we need to do few operations on it. */
		this->trw = getter_params.trw;
	}

	if (new_data_source->is_thread) {
		if (!po->babel_args.isEmpty() || !po->url.isEmpty() || !po->shell_command.isEmpty()) {

			AcquireGetter getter(getter_params);
			getter.run();

			if (progress_dialog) {
				progress_dialog->exec();
			}

			if (this->running) {
				/* Cancel and mark for thread to finish. */
				this->running = false;
				/* NB Thread will free memory. */
			} else {
#ifdef K
				/* Get data for Off command. */
				if (new_data_source->off_func) {
					QString babel_args_off;
					QString file_path_off;
					interface->off_func(pass_along_data, babel_args_off, file_path_off);

					if (!babel_args_off.isEmpty()) {
						/* Turn off. */
						ProcessOptions off_po(babel_args_off, file_path_off, NULL, NULL);
						a_babel_convert_from(NULL, &off_po, NULL, NULL, NULL);
					}
				}
#endif
			}
		} else {
			/* This shouldn't happen... */
			this->status->setText(QObject::tr("Unable to create command\nAcquire method failed."));
			if (progress_dialog) {
				progress_dialog->exec();
			}
		}
	} else {
		/* Bypass thread method malarkly - you'll just have to wait... */

		bool success = new_data_source->process_func(getter_params.trw, po, (BabelCallback) progress_func, this, dl_options);
		if (!success) {
			Dialog::error(QObject::tr("Error: acquisition failed."), this->window);
		}

		delete po;
		delete dl_options;

		on_complete_process(getter_params);

		/* Actually show it if necessary. */
		if (new_data_source->keep_dialog_open) {
			if (progress_dialog) {
				progress_dialog->exec();
			}
		}
	}


	delete progress_dialog;
	delete setup_dialog;
}




ProcessOptions * SlavGPS::acquire_create_process_options(AcquireProcess * acq, DataSourceDialog * setup_dialog, DownloadOptions * dl_options, DataSourceInterface * interface, void * pass_along_data)
{
	ProcessOptions * po = NULL;

	switch (interface->inputtype) {

	case DatasourceInputtype::TRWLAYER: {
		char * name_src = a_gpx_write_tmp_file(acq->trw, NULL);
		po = interface->get_process_options(pass_along_data, NULL, name_src, NULL);
		util_add_to_deletion_list(name_src);
		free(name_src);
		}
		break;

	case DatasourceInputtype::TRWLAYER_TRACK: {
		char * name_src = a_gpx_write_tmp_file(acq->trw, NULL);
		char * name_src_track = a_gpx_write_track_tmp_file(acq->trk, NULL);

		po = interface->get_process_options(pass_along_data, NULL, name_src, name_src_track);

		util_add_to_deletion_list(name_src);
		util_add_to_deletion_list(name_src_track);

		free(name_src);
		free(name_src_track);
		}
		break;

	case DatasourceInputtype::TRACK: {
		char * name_src_track = a_gpx_write_track_tmp_file(acq->trk, NULL);
		po = interface->get_process_options(pass_along_data, NULL, NULL, name_src_track);
		free(name_src_track);
		}
		break;

	case DatasourceInputtype::NONE:
		if (interface == &datasource_routing_interface
		    || interface == &datasource_gps_interface
		    || interface == &datasource_geojson_interface
		    || interface == &datasource_geotag_interface) {

			po = setup_dialog->get_process_options(*dl_options);
		} else {
			if (interface->get_process_options) {
				po = interface->get_process_options(pass_along_data, dl_options, NULL, NULL);
			}
		}
		break;

	default:
		qDebug() << "EE: Acquire: unsupported Datasource Input Type" << (int) interface->inputtype;
		break;
	};

	return po;
}




ProcessOptions * SlavGPS::acquire_create_process_options(AcquireProcess * acq, DataSourceDialog * setup_dialog, DownloadOptions * dl_options, DataSource * data_source)
{
	ProcessOptions * po = NULL;
	void * pass_along_data = NULL;

	switch (data_source->inputtype) {

	case DatasourceInputtype::TRWLAYER: {
		qDebug() << "II:" PREFIX << "input type: TRWLayer";

		char * name_src = a_gpx_write_tmp_file(acq->trw, NULL);
		po = data_source->get_process_options(pass_along_data, NULL, name_src, NULL);
		util_add_to_deletion_list(name_src);
		free(name_src);
		}
		break;

	case DatasourceInputtype::TRWLAYER_TRACK: {
		qDebug() << "II:" PREFIX << "input type: TRWLayerTrack";

		char * name_src = a_gpx_write_tmp_file(acq->trw, NULL);
		char * name_src_track = a_gpx_write_track_tmp_file(acq->trk, NULL);

		po = data_source->get_process_options(pass_along_data, NULL, name_src, name_src_track);

		util_add_to_deletion_list(name_src);
		util_add_to_deletion_list(name_src_track);

		free(name_src);
		free(name_src_track);
		}
		break;

	case DatasourceInputtype::TRACK: {
		qDebug() << "II:" PREFIX << "input type: Track";

		char * name_src_track = a_gpx_write_track_tmp_file(acq->trk, NULL);
		po = data_source->get_process_options(pass_along_data, NULL, NULL, name_src_track);
		free(name_src_track);
		}
		break;

	case DatasourceInputtype::NONE:
		/*
		  DataSourceFile
		  DataSourceOSMTraces
  		  DataSourceOSMMyTraces
		  DataSourceURL
		  DataSourceGeoCache

		  DataSourceWikipedia is also of type None, but it
		  doesn't provide setup dialog and doesn't implement
		  this method, so we will call empty method in base
		  class.
		*/
		qDebug() << "II:" PREFIX << "input type: None";

		assert (setup_dialog);
		po = setup_dialog->get_process_options(*dl_options);
		break;

	default:
		qDebug() << "EE:" PREFIX << "unsupported Datasource Input Type" << (int) data_source->inputtype;
		break;
	};

	return po;
}




/**
 * @window: The #Window to work with
 * @panel: The #LayersPanel in which a #LayerTRW layer may be created/appended
 * @viewport: The #Viewport defining the current view
 * @mode: How layers should be managed
 * @source_interface: The #DataSourceInterface determining how and what actions to take
 * @userdata: External data to be passed into the #DataSourceInterface
 * @cleanup_function: The function to dispose the #DataSourceInterface if necessary
 *
 * Process the given DataSourceInterface for sources with no input data.
 */
void Acquire::acquire_from_source(Window * new_window, LayersPanel * new_panel, Viewport * new_viewport, DataSourceMode mode, DataSourceInterface * source_interface, void * userdata, DataSourceCleanupFunc cleanup_function)
{
	g_acquiring->window = new_window;
	g_acquiring->panel = new_panel;
	g_acquiring->viewport = new_viewport;
	g_acquiring->trw = NULL;
	g_acquiring->trk = NULL;

	g_acquiring->acquire(mode, source_interface, userdata, cleanup_function);

	if (g_acquiring->trw) {
		g_acquiring->trw->add_children_to_tree();
	}
}




void AcquireProcess::acquire_trwlayer_cb(void)
{
	QAction * qa = (QAction *) QObject::sender();
	int idx = qa->data().toInt();

	DataSourceInterface * iface = (DataSourceInterface *) filters[idx];

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

			QAction * action = new QAction(filters[i]->window_title, this);
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
QMenu * Acquire::create_trwlayer_menu(Window * new_window, LayersPanel * new_panel, Viewport * new_viewport, LayerTRW * trw)
{
	g_acquiring->window = new_window;
	g_acquiring->panel = new_panel;
	g_acquiring->viewport = new_viewport;
	g_acquiring->trw = trw;
	g_acquiring->trk = NULL;

	return g_acquiring->build_menu(QObject::tr("&Filter"), DatasourceInputtype::TRWLAYER);
}




/**
 * Create a sub menu intended for rightclicking on a TRWLayer's menu called "Filter with Track "TRACKNAME"...".
 *
 * Returns: %NULL if no filters or no filter track has been set.
 */
QMenu * Acquire::create_trwlayer_track_menu(Window * new_window, LayersPanel * new_panel, Viewport * new_viewport, LayerTRW * new_trw)
{
	if (filter_track == NULL) {
		return NULL;
	} else {
		g_acquiring->window = new_window;
		g_acquiring->panel = new_panel;
		g_acquiring->viewport = new_viewport;
		g_acquiring->trw = new_trw;
		g_acquiring->trk = filter_track;

		const QString submenu_label = QObject::tr("Filter with %1").arg(filter_track->name);
		return g_acquiring->build_menu(submenu_label, DatasourceInputtype::TRWLAYER_TRACK);
	}
}




/**
 * Create a sub menu intended for rightclicking on a track's menu called "Filter".
 *
 * Returns: %NULL if no applicable filters
 */
QMenu * Acquire::create_track_menu(Window * new_window, LayersPanel * new_panel, Viewport * new_viewport, Track * new_trk)
{
	g_acquiring->window = new_window;
	g_acquiring->panel = new_panel;
	g_acquiring->viewport = new_viewport;
	g_acquiring->trw = NULL;
	g_acquiring->trk = new_trk;

	return g_acquiring->build_menu(QObject::tr("&Filter"), DatasourceInputtype::TRACK);
}




/**
 * Sets application-wide track to use with filter. references the track.
 */
void Acquire::set_filter_track(Track * trk)
{
	if (filter_track) {
		filter_track->free();
	}

	filter_track = trk;
	trk->ref();
}




void Acquire::init(void)
{
	g_acquiring = new AcquireProcess();
}




void Acquire::uninit(void)
{
	delete g_acquiring;
}
