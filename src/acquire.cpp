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




#include <cassert>
#include <vector>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif




#include <QRunnable>
#include <QThreadPool>




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




#define SG_MODULE "Acquire"
#define PREFIX ": Acquire:" << __FUNCTION__ << __LINE__ << ">"




static std::vector<DataSource *> g_bfilters;
static Track * bfilter_track = NULL;
static AcquireContext * g_acquire_context = NULL;




static void finalize(AcquireContext & acquire_context, DataSource * data_source, AcquireProgressDialog * progress_dialog);




AcquireWorker::AcquireWorker()
{
}




AcquireWorker::~AcquireWorker()
{
}




void AcquireWorker::configure_target_layer(DataSourceMode mode)
{
	switch (mode) {
	case DataSourceMode::CreateNewLayer:
		this->acquire_context->target_trw_allocated = true;
		break;

	case DataSourceMode::AddToLayer: {
		Layer * selected_layer = this->acquire_context->selected_layer;
		if (selected_layer && selected_layer->type == LayerType::TRW) {
			this->acquire_context->target_trw = (LayerTRW *) selected_layer;
			this->acquire_context->target_trw_allocated = false;
		} else {
			/* TODO */
		}
		}
		break;

	case DataSourceMode::AutoLayerManagement:
		/* NOOP */
		break;

	case DataSourceMode::ManualLayerManagement: {
		/* Don't create in acquire - as datasource will perform the necessary actions. */
		this->acquire_context->target_trw_allocated = false;
		Layer * selected_layer = this->acquire_context->selected_layer;
		if (selected_layer && selected_layer->type == LayerType::TRW) {
			this->acquire_context->target_trw = (LayerTRW *) selected_layer;
		} else {
			/* TODO */
		}
		}
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected DataSourceMode" << (int) mode;
		break;
	};


	if (this->acquire_context->target_trw_allocated) {
		this->acquire_context->target_trw = new LayerTRW();
		this->acquire_context->target_trw->set_coord_mode(this->acquire_context->viewport->get_coord_mode());
		this->acquire_context->target_trw->set_name(this->data_source->layer_title);
	}
}





/* Call the function when acquire process has been completed without
   termination or errors. */
void AcquireWorker::finalize_after_completion(void)
{
	if (this->acquire_context->target_trw_allocated) {
		qDebug() << SG_PREFIX_I << "Layer has been freshly allocated";

		if (NULL == this->acquire_context->target_trw) {
			qDebug() << SG_PREFIX_E << "Layer marked as allocated, but is NULL";
			return;
		}

		if (this->acquire_context->target_trw->is_empty()) {
			/* Acquire process ended without errors, but
			   zero new items were acquired. */
			qDebug() << SG_PREFIX_I << "Layer is empty, delete the layer";

			/* TODO: verify that the layer is not attached to tree yet. */

			delete this->acquire_context->target_trw;
			this->acquire_context->target_trw = NULL;
			return;
		}


		qDebug() << SG_PREFIX_I << "New layer is non-empty, will now process the layer";
		//this->acquire_context->top_level_layer->add_layer(this->acquire_context->target_trw, true);
		//this->acquire_context->top_level_layer->attach_children_to_tree();
	}


	this->acquire_context->target_trw->attach_children_to_tree();
	this->acquire_context->target_trw->post_read(this->acquire_context->viewport, true);
	/* View this data if desired - must be done after post read (so that the bounds are known). */
	if (this->data_source && this->data_source->autoview) {
		this->acquire_context->target_trw->move_viewport_to_show_all(this->acquire_context->viewport);
		// this->acquire_context->panel->emit_items_tree_updated_cb("acquire completed");
	}


	if (this->progress_dialog) {
		if (this->data_source->keep_dialog_open) {
			this->progress_dialog->button_box->button(QDialogButtonBox::Ok)->setEnabled(true);
			this->progress_dialog->button_box->button(QDialogButtonBox::Cancel)->setEnabled(false);
		} else {
			/* Call 'accept()' slot to close the dialog. */
			qDebug() << SG_PREFIX_I << "Will close the dialog by clicking OK";
			this->progress_dialog->accept();
		}
	}
}




/* Call the function when acquire process has been terminated - either
   because of errors or because user cancelled it. */
void AcquireWorker::finalize_after_termination(void)
{
	if (this->acquire_context->target_trw_allocated) {
		this->acquire_context->target_trw->unref();
	}

	this->progress_dialog->set_headline(QObject::tr("Error: acquisition failed."));

	return;
}




/* This routine is the worker thread. There is only one simultaneous acquire allowed. */
/* Re-implementation of QRunnable::run() */
void AcquireWorker::run(void)
{
	sleep(1); /* Time for progress dialog to open and block main UI thread. */

	this->acquire_is_running = true;
	const sg_ret acquire_result = this->data_source->acquire_into_layer(this->acquire_context->target_trw, *this->acquire_context, this->progress_dialog);
	this->acquire_is_running = false;

	if (acquire_result == sg_ret::ok) {
		qDebug() << SG_PREFIX_I << "Acquire process ended with success";
		this->finalize_after_completion();
	} else {
		qDebug() << SG_PREFIX_W << "Acquire process ended with error";
		this->finalize_after_termination();
	}
}




void Acquire::acquire_from_source(DataSource * new_data_source, DataSourceMode mode, AcquireContext * new_acquire_context)
{
	if (QDialog::Accepted != new_data_source->run_config_dialog(*new_acquire_context)) {
		qDebug() << SG_PREFIX_I << "Data source config dialog returned !Accepted";
		return;
	}


	AcquireProgressDialog * progress_dialog = new_data_source->create_progress_dialog(QObject::tr("Acquiring"));
	progress_dialog->set_headline(QObject::tr("Importing data..."));


	if (false && NULL == new_data_source->acquire_options) {
		/* This shouldn't happen... */
		qDebug() << SG_PREFIX_E << "Acquire options are NULL";

		if (progress_dialog) {
			progress_dialog->set_headline(QObject::tr("Unable to create command\nAcquire method failed."));
			progress_dialog->exec(); /* TODO_2_LATER: improve handling of invalid process options. */
			delete progress_dialog; /* TODO: move this to destructor. */
		}

		return;
	}


	AcquireWorker * worker = new AcquireWorker(); /* TODO_LATER: this needs to be deleted. */
	worker->data_source = new_data_source;
	worker->acquire_context = new_acquire_context;
	worker->configure_target_layer(mode);
	worker->progress_dialog = progress_dialog;


	/* Start the acquire task in a background thread and then
	   block this foreground (UI) thread by showing a dialog. We
	   need to block this tread to prevent the UI focus from going
	   back to main window.

	   Until a background acquire thread is in progress, its
	   progress window must be in foreground. */

	QThreadPool::globalInstance()->start(worker);

	if (worker->progress_dialog) {
		worker->progress_dialog->exec();
	}

#ifdef K_FIXME_RESTORE
	/* We get here if user closed a dialog window.  The window was
	   either closed through "Cancel" button when the acquire
	   process was still in progress, or through "OK" button that
	   became active when acquire process has been completed. */
	if (worker->acquire_is_running) {
		/* Cancel and mark for thread to finish. */
		worker->acquire_is_running = false;
	} else {

		/* Get data for Off command. */
		if (data_source->off_func) {
			QString babel_args_off;
			QString file_path_off;
			interface->off_func(pass_along_data, babel_args_off, file_path_off);

			if (!babel_args_off.isEmpty()) {

				/* Turn off. */
				BabelTurnOffDevice turn_off(file_path_off, babel_args_off);
				turn_off.run_process();
			}
		}
	}

#endif


	delete progress_dialog;
}




AcquireContext::AcquireContext()
{
}




void AcquireContext::filter_trwlayer_cb(void)
{
	QAction * qa = (QAction *) QObject::sender();
	const int filter_idx = qa->data().toInt();

	Acquire::acquire_from_source(g_bfilters[filter_idx], g_bfilters[filter_idx]->mode, NULL);
}




QMenu * Acquire::create_bfilter_menu(const QString & menu_label, DataSourceInputType input_type)
{
	QMenu * menu = NULL;
	int i = 0;

	for (auto iter = g_bfilters.begin(); iter != g_bfilters.end(); iter++) {
		DataSource * filter = *iter;

		if (filter->input_type == input_type) {
			if (!menu) { /* Do this just once, but return NULL if no filters. */
				menu = new QMenu(menu_label);
			}

			QAction * action = new QAction(filter->window_title, g_acquire_context);
			action->setData(i);
			QObject::connect(action, SIGNAL (triggered(bool)), g_acquire_context, SLOT (filter_trwlayer_cb(void)));
			menu->addAction(action);
		}
		i++;
	}

	return menu;
}




/**
   @brief Create a "Filter" sub menu intended for rightclicking on a TRW layer

   @return NULL if no filters are available for a TRW layer
   @return new menu otherwise
*/
QMenu * Acquire::create_bfilter_layer_menu(void)
{
	return Acquire::create_bfilter_menu(QObject::tr("&Filter"), DataSourceInputType::TRWLayer);
}




/**
   @brief Create a sub menu intended for rightclicking on a TRWLayer's menu called "Filter with Track "TRACKNAME"..."

   @return NULL if no filters or no filter track has been set
   @return menu otherwise
*/
QMenu * Acquire::create_bfilter_layer_track_menu(void)
{
	if (bfilter_track == NULL) {
		return NULL;
	} else {
		g_acquire_context->target_trk = bfilter_track;

		const QString menu_label = QObject::tr("Filter with %1").arg(bfilter_track->name);
		return Acquire::create_bfilter_menu(menu_label, DataSourceInputType::TRWLayerTrack);
	}
}




/**
   @brief Create a "Filter" sub menu intended for rightclicking on a TRW track

   @return NULL if no filters are available for a TRW track
   @return new menu otherwise
*/
QMenu * Acquire::create_bfilter_track_menu(void)
{
	return Acquire::create_bfilter_menu(QObject::tr("&Filter"), DataSourceInputType::Track);
}




/**
 * Sets application-wide track to use with filter. references the track.
 */
void Acquire::set_bfilter_track(Track * trk)
{
	if (bfilter_track) {
		bfilter_track->free();
	}

	bfilter_track = trk;
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

	g_acquire_context = new AcquireContext();
}




void Acquire::uninit(void)
{
	delete g_acquire_context;

	for (auto iter = g_bfilters.begin(); iter != g_bfilters.end(); iter++) {
		delete *iter;
	}
}




void Acquire::set_context(Window * new_window, Viewport * new_viewport, LayerAggregate * new_top_level_layer, Layer * new_selected_layer)
{
	g_acquire_context->window = new_window;
	g_acquire_context->viewport = new_viewport;
	g_acquire_context->top_level_layer = new_top_level_layer;
	g_acquire_context->selected_layer = new_selected_layer;
}




void Acquire::set_target(LayerTRW * trw, Track * trk)
{
	g_acquire_context->target_trw = trw;
	g_acquire_context->target_trk = trk;
}
