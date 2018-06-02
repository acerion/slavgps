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
#include <vector>




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
#include "datasources.h"




using namespace SlavGPS;




#define PREFIX ": Acquire:" << __FUNCTION__ << __LINE__ << ">"




static std::vector<DataSource *> g_bfilters;
static Track * filter_track = NULL;
static AcquireProcess * g_acquiring = NULL;




static ProcessOptions * acquire_create_process_options(AcquireProcess * acq, DataSourceDialog * setup_dialog, DataSource * data_source);
static DownloadOptions * acquire_create_download_options(DataSourceDialog * setup_dialog);




AcquireGetter::~AcquireGetter()
{
	delete this->po;
	delete this->dl_options;
}




/**
 * Some common things to do on completion of a datasource process
 *  . Update layer
 *  . Update dialog info
 *  . Update main dsisplay
 */
void AcquireGetter::on_complete_process(void)
{
	if (
#ifdef K_FIXME_RESTORE
	    this->acquiring->running
#else
	    true
#endif
	    ) {
		this->acquiring->status->setText(QObject::tr("Done."));
		if (this->creating_new_layer) {
			/* Only create the layer if it actually contains anything useful. */
			/* TODO: create function for this operation to hide detail: */
			if (!this->acquiring->trw->is_empty()) {
				this->acquiring->trw->post_read(this->acquiring->viewport, true);
				this->acquiring->panel->get_top_layer()->add_layer(this->acquiring->trw, true);

				/* Add any acquired tracks/routes/waypoints to tree view
				   so that they can be displayed on "tree updated" event. */
				this->acquiring->trw->add_children_to_tree();
			} else {
				this->acquiring->status->setText(QObject::tr("No data.")); /* TODO: where do we display this message? */
			}
		}

		if (this->acquiring->data_source_dialog) {
			if (this->acquiring->data_source->keep_dialog_open) {
				/* Only allow dialog's validation when format selection is done. */
				this->acquiring->data_source_dialog->button_box->button(QDialogButtonBox::Ok)->setEnabled(true);
				this->acquiring->data_source_dialog->button_box->button(QDialogButtonBox::Cancel)->setEnabled(false);
			} else {
				/* Call 'accept()' slot to close the dialog. */
				this->acquiring->data_source_dialog->accept();
			}
		}

		/* Main display update. */
		if (this->acquiring->trw) {
			this->acquiring->trw->post_read(this->acquiring->viewport, true);
			/* View this data if desired - must be done after post read (so that the bounds are known). */
			if (this->acquiring->data_source && this->acquiring->data_source->autoview) {
				this->acquiring->trw->move_viewport_to_show_all(this->acquiring->viewport);
			}
			this->acquiring->panel->emit_items_tree_updated_cb("acquire completed");
		}
	} else {
		/* Cancelled. */
		if (this->creating_new_layer) {
			this->acquiring->trw->unref();
		}
	}
}




ProcessOptions::ProcessOptions(const QString & new_babel_args, const QString & new_input_file_name, const QString & new_input_file_type, const QString & new_url)
{
	if (!new_babel_args.isEmpty()) {
		this->babel_args = new_babel_args;
	}
	if (!new_input_file_name.isEmpty()) {
		this->input_file_name = new_input_file_name;
	}
	if (!new_input_file_type.isEmpty()) {
		this->input_file_type = new_input_file_type;
	}
	if (!new_url.isEmpty()) {
		this->url = new_url;
	}
}




/* This routine is the worker thread. There is only one simultaneous download allowed. */
/* Re-implementation of QRunnable::run() */
void AcquireGetter::run(void)
{
	DataSource * data_source = this->acquiring->data_source;
	assert (data_source);

	bool result = data_source->process_func(this->acquiring->trw, this->po, this->dl_options, this->acquiring);

	if (this->acquiring->running && !result) {
		this->acquiring->status->setText(QObject::tr("Error: acquisition failed."));
		if (this->creating_new_layer) {
			this->acquiring->trw->unref();
		}
	} else {
		this->on_complete_process();
	}

	data_source->cleanup(this->acquiring->parent_data_source_dialog);

	this->acquiring->running = false;
}




void AcquireProcess::acquire(DataSource * new_data_source, DataSourceMode mode, void * new_parent_data_source_dialog)
{
	this->parent_data_source_dialog = (DataSourceDialog *) new_parent_data_source_dialog;
	this->data_source = new_data_source;


	DataSourceDialog * setup_dialog = new_data_source->create_setup_dialog(this->viewport, this->parent_data_source_dialog);
	if (setup_dialog) {
		setup_dialog->setWindowTitle(new_data_source->window_title);

		if (setup_dialog->exec() != QDialog::Accepted) {
			delete setup_dialog;
			return;
		}
	}


	AcquireGetter getter;
	getter.acquiring = this;
	getter.po = acquire_create_process_options(this, setup_dialog, new_data_source);
	getter.dl_options = acquire_create_download_options(setup_dialog);
	getter.creating_new_layer = (!this->trw); /* Default if Auto Layer Management is passed in. */

	if (setup_dialog) {
		delete setup_dialog;
		setup_dialog = NULL;
	}

	this->data_source_dialog = setup_dialog; /* FIXME: setup or progress dialog? */
	this->running = true;
	this->status = new QLabel(QObject::tr("Working..."));

	DataSourceDialog * progress_dialog = NULL;
#ifdef K_FIXME_RESTORE
	if (new_data_source->create_progress_dialog) {
		progress_dialog = new_data_source->create_progress_dialog(this->user_data);
	}
#endif


	switch (mode) {
	case DataSourceMode::CreateNewLayer:
		getter.creating_new_layer = true;
		break;

	case DataSourceMode::AddToLayer: {
		Layer * selected_layer = this->panel->get_selected_layer();
		if (selected_layer->type == LayerType::TRW) {
			getter.acquiring->trw = (LayerTRW *) selected_layer;
			getter.creating_new_layer = false;
		}
		}
		break;

	case DataSourceMode::AutoLayerManagement:
		/* NOOP */
		break;

	case DataSourceMode::ManualLayerManagement: {
		/* Don't create in acquire - as datasource will perform the necessary actions. */
		getter.creating_new_layer = false;
		Layer * selected_layer = this->panel->get_selected_layer();
		if (selected_layer->type == LayerType::TRW) {
			getter.acquiring->trw = (LayerTRW *) selected_layer;
		}
		}
		break;
	default:
		qDebug() << "EE: Acquire: unexpected DataSourceMode" << (int) mode;
		break;
	};


	if (getter.creating_new_layer) {
		getter.acquiring->trw = new LayerTRW();
		getter.acquiring->trw->set_coord_mode(this->viewport->get_coord_mode());
		getter.acquiring->trw->set_name(new_data_source->layer_title);
	}


	if (new_data_source->is_thread) {
		if (!getter.po->babel_args.isEmpty() || !getter.po->url.isEmpty() || !getter.po->shell_command.isEmpty()) {

			getter.run();

			if (progress_dialog) {
				progress_dialog->exec();
			}

			if (this->running) {
				/* Cancel and mark for thread to finish. */
				this->running = false;
				/* NB Thread will free memory. */
			} else {
#ifdef K_FIXME_RESTORE
				/* Get data for Off command. */
				if (new_data_source->off_func) {
					QString babel_args_off;
					QString file_path_off;
					interface->off_func(pass_along_data, babel_args_off, file_path_off);

					if (!babel_args_off.isEmpty()) {
						/* Turn off. */
						ProcessOptions off_options(babel_args_off, file_path_off, NULL, NULL);
						off_options.turn_off_device();
					}
				}
#endif
			}
		} else {
			/* This shouldn't happen... */
			this->status->setText(QObject::tr("Unable to create command\nAcquire method failed.")); /* TODO: this should go to dialog box. */
			if (progress_dialog) {
				progress_dialog->exec();
			}
		}
	} else {
		/* Bypass thread method malarkly - you'll just have to wait... */

		bool success = new_data_source->process_func(getter.acquiring->trw, getter.po, getter.dl_options, this);
		if (!success) {
			Dialog::error(QObject::tr("Error: acquisition failed."), this->window);
		}

		getter.on_complete_process();

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




void AcquireProcess::import_progress_cb(AcquireProgressCode code, void * data)
{
	assert (this->data_source);

	if (this->data_source->is_thread) {
		if (!this->running) {
			this->data_source->cleanup(this->parent_data_source_dialog);
		}
	}

#ifdef K_FIXME_RESTORE
	/* See DataSourceGPS::progress_func(). */
	this->data_source->progress_func(code, data, this);
#endif
}




ProcessOptions * acquire_create_process_options(AcquireProcess * acq, DataSourceDialog * setup_dialog, DataSource * data_source)
{
	ProcessOptions * po = NULL;

	switch (data_source->input_type) {

	case DataSourceInputType::TRWLayer: {
		/*
		  BFilterSimplify
		  BFilterCompress
		  BFilterDuplicates
		  BFilterManual
		*/
		qDebug() << "II" PREFIX << "input type: TRWLayer";

		const QString layer_file_full_path = GPX::write_tmp_file(acq->trw, NULL);
		po = setup_dialog->get_process_options_layer(layer_file_full_path);
		Util::add_to_deletion_list(layer_file_full_path);
		}
		break;

	case DataSourceInputType::TRWLayerTrack: {
		/*
		  BFilterPolygon
		  BFilterExcludePolygon
		*/
		qDebug() << "II" PREFIX << "input type: TRWLayerTrack";

		const QString layer_file_full_path = GPX::write_tmp_file(acq->trw, NULL);
		const QString track_file_full_path = GPX::write_track_tmp_file(acq->trk, NULL);

		po = setup_dialog->get_process_options_layer_track(layer_file_full_path, track_file_full_path);

		Util::add_to_deletion_list(layer_file_full_path);
		Util::add_to_deletion_list(track_file_full_path);
		}
		break;

	case DataSourceInputType::Track: {
		qDebug() << "II" PREFIX << "input type: Track";

		const QString track_file_full_path = GPX::write_track_tmp_file(acq->trk, NULL);
		po = setup_dialog->get_process_options_layer_track("", track_file_full_path);
		}
		break;

	case DataSourceInputType::None:
		/*
		  DataSourceFile
		  DataSourceOSMTraces
  		  DataSourceOSMMyTraces
		  DataSourceURL
		  DataSourceGeoCache
		  DataSourceGeoTag
		  DataSourceGeoJSON
		  DataSourceRouting
		  DataSourceGPS
		  DataSourceWebTool

		  DataSourceWikipedia is also of type None, but it
		  doesn't provide setup dialog and doesn't implement
		  this method, so we will call empty method in base
		  class.
		*/
		qDebug() << "II" PREFIX << "input type: None";

		assert (setup_dialog);
		po = setup_dialog->get_process_options_none();
		break;

	default:
		qDebug() << "EE" PREFIX << "unsupported Datasource Input Type" << (int) data_source->input_type;
		break;
	};

	return po;
}




DownloadOptions * acquire_create_download_options(DataSourceDialog * setup_dialog)
{
	DownloadOptions * dl_options = new DownloadOptions; /* With default values. */
	setup_dialog->adjust_download_options(*dl_options);
	return dl_options;
}




void Acquire::acquire_from_source(DataSource * new_data_source, DataSourceMode new_mode, Window * new_window, LayersPanel * new_panel, Viewport * new_viewport, DataSourceDialog * new_parent_data_source_dialog)
{
	AcquireProcess acquiring(new_window, new_panel, new_viewport);
	acquiring.acquire(new_data_source, new_mode, new_parent_data_source_dialog);
}




void AcquireProcess::acquire_trwlayer_cb(void)
{
	QAction * qa = (QAction *) QObject::sender();
	int idx = qa->data().toInt();

	this->acquire(g_bfilters[idx], g_bfilters[idx]->mode, NULL);
}




QMenu * AcquireProcess::build_menu(const QString & submenu_label, DataSourceInputType input_type)
{
	QMenu * menu = NULL;
	int i = 0;

	for (auto iter = g_bfilters.begin(); iter != g_bfilters.end(); iter++) {
		DataSource * filter = *iter;

		if (filter->input_type == input_type) {
			if (!menu) { /* Do this just once, but return NULL if no filters. */
				menu = new QMenu(submenu_label);
			}

			QAction * action = new QAction(filter->window_title, this);
			action->setData(i);
			QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (acquire_trwlayer_cb(void)));
			menu->addAction(action);
		}
		i++;
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

	return g_acquiring->build_menu(QObject::tr("&Filter"), DataSourceInputType::TRWLayer);
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
		return g_acquiring->build_menu(submenu_label, DataSourceInputType::TRWLayerTrack);
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

	return g_acquiring->build_menu(QObject::tr("&Filter"), DataSourceInputType::Track);
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
	/*** Input is LayerTRW. ***/
	g_bfilters.push_back(new BFilterSimplify());
	g_bfilters.push_back(new BFilterCompress());
	g_bfilters.push_back(new BFilterDuplicates());
	g_bfilters.push_back(new BFilterManual());
	/*** Input is a track and a LayerTRW. ***/
	g_bfilters.push_back(new BFilterPolygon());
	g_bfilters.push_back(new BFilterExcludePolygon());

	g_acquiring = new AcquireProcess();
}




void Acquire::uninit(void)
{
	delete g_acquiring;

	for (auto iter = g_bfilters.begin(); iter != g_bfilters.end(); iter++) {
		delete *iter;
	}
}




bool DataSourceBabel::process_func(LayerTRW * trw, ProcessOptions * babel_action, DownloadOptions * download_options, AcquireTool * progress_indicator)
{
	return babel_action->universal_import_fn(trw, download_options, progress_indicator);
}
