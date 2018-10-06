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




AcquireGetter::~AcquireGetter()
{
}




/**
 * Some common things to do on completion of a datasource process
 *  . Update layer
 *  . Update dialog info
 *  . Update main viewport
 */
void AcquireGetter::on_complete_process(void)
{
	if (
#ifdef K_FIXME_RESTORE
	    this->acquire_is_running
#else
	    true
#endif
	    ) {
		emit report_status(3);

		if (this->acquire_context.target_trw_allocated) {

			/* Only create the layer if it actually contains anything useful. */
			/* TODO_2_LATER: create function for this operation to hide detail: */
			if (!this->acquire_context.target_trw->is_empty()) {
				this->acquire_context.target_trw->post_read(this->acquire_context.viewport, true);
				this->acquire_context.top_level_layer->add_layer(this->acquire_context.target_trw, true);

				/* Add any acquired tracks/routes/waypoints to tree view
				   so that they can be displayed on "tree updated" event. */
				this->acquire_context.target_trw->add_children_to_tree();
			} else {
				emit report_status(4);
			}
		}

		if (this->acquire_getter_progress_dialog) {
			if (this->data_source->keep_dialog_open) {
				/* Only allow dialog's validation when format selection is done. */
				this->acquire_getter_progress_dialog->button_box->button(QDialogButtonBox::Ok)->setEnabled(true);
				this->acquire_getter_progress_dialog->button_box->button(QDialogButtonBox::Cancel)->setEnabled(false);
			} else {
				/* Call 'accept()' slot to close the dialog. */
				this->acquire_getter_progress_dialog->accept();
			}
		}

		/* Main display update. */
		if (this->acquire_context.target_trw) {
			this->acquire_context.target_trw->post_read(this->acquire_context.viewport, true);
			/* View this data if desired - must be done after post read (so that the bounds are known). */
			if (this->data_source && this->data_source->autoview) {
				this->acquire_context.target_trw->move_viewport_to_show_all(this->acquire_context.viewport);
			}
			/// this->acquire_context->panel->emit_items_tree_updated_cb("acquire completed");
		}
	} else {
		/* Cancelled. */
		if (this->acquire_context.target_trw_allocated) {
			this->acquire_context.target_trw->unref();
		}
	}
}




void AcquireGetter::configure_target_layer(DataSourceMode mode)
{
	switch (mode) {
	case DataSourceMode::CreateNewLayer:
		this->acquire_context.target_trw_allocated = true;
		break;

	case DataSourceMode::AddToLayer: {
		Layer * selected_layer = this->acquire_context.selected_layer;
		if (selected_layer && selected_layer->type == LayerType::TRW) {
			this->acquire_context.target_trw = (LayerTRW *) selected_layer;
			this->acquire_context.target_trw_allocated = false;
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
		this->acquire_context.target_trw_allocated = false;
		Layer * selected_layer = this->acquire_context.selected_layer;
		if (selected_layer && selected_layer->type == LayerType::TRW) {
			this->acquire_context.target_trw = (LayerTRW *) selected_layer;
		} else {
			/* TODO */
		}
		}
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected DataSourceMode" << (int) mode;
		break;
	};


	if (this->acquire_context.target_trw_allocated) {
		this->acquire_context.target_trw = new LayerTRW();
		this->acquire_context.target_trw->set_coord_mode(this->acquire_context.viewport->get_coord_mode());
		this->acquire_context.target_trw->set_name(this->data_source->layer_title);
	}
}




/* This routine is the worker thread. There is only one simultaneous download allowed. */
/* Re-implementation of QRunnable::run() */
void AcquireGetter::run(void)
{
	assert (this->data_source);


	this->result = this->data_source->acquire_into_layer(this->acquire_context.target_trw, this->acquire_context, this->acquire_getter_progress_dialog);

	if (this->acquire_is_running && !this->result) {
		this->acquire_getter_progress_dialog->set_headline(QObject::tr("Error: acquisition failed."));
		if (this->acquire_context.target_trw_allocated) {
			this->acquire_context.target_trw->unref();
		}
	} else {
		this->on_complete_process();
	}

	this->acquire_is_running = false;
}




void Acquire::acquire_from_source(DataSource * new_data_source, DataSourceMode mode, AcquireContext * new_acquire_context)
{
	AcquireGetter * getter = new AcquireGetter(); /* TODO_LATER: this needs to be deleted. */

	getter->data_source = new_data_source;
	if (new_acquire_context) {
		getter->acquire_context.window = new_acquire_context->window;
		getter->acquire_context.viewport = new_acquire_context->viewport;
		getter->acquire_context.top_level_layer = new_acquire_context->top_level_layer;
		getter->acquire_context.selected_layer = new_acquire_context->selected_layer;
		getter->acquire_context.target_trw = new_acquire_context->target_trw; /* TODO: call to configure_target_layer() may overwrite ::target_trw. */
		getter->acquire_context.target_trk = new_acquire_context->target_trk;
		getter->acquire_context.target_trw_allocated = new_acquire_context->target_trw_allocated;
	}

	if (QDialog::Accepted != new_data_source->run_config_dialog(getter->acquire_context)) {
		qDebug() << SG_PREFIX_I << "Data source config dialog returned !Accepted";
		delete getter;
		return;
	}

	DataProgressDialog * progress_dialog = new_data_source->create_progress_dialog(QObject::tr("Acquiring"));
	getter->acquire_getter_progress_dialog = progress_dialog;
	getter->acquire_getter_progress_dialog->set_headline(QObject::tr("Importing data..."));

	if (NULL == getter->data_source->acquire_options || !getter->data_source->acquire_options->is_valid()) {

		/* This shouldn't happen... */

		if (NULL == getter->data_source->acquire_options) {
			qDebug() << SG_PREFIX_E << "Acquire options are NULL";
		} else {
			if (!getter->data_source->acquire_options->is_valid()) {
				qDebug() << SG_PREFIX_E << "Acquire options are invalid";
			}
		}

		if (getter->acquire_getter_progress_dialog) {
			getter->acquire_getter_progress_dialog->set_headline(QObject::tr("Unable to create command\nAcquire method failed."));
			getter->acquire_getter_progress_dialog->exec(); /* TODO_2_LATER: improve handling of invalid process options. */
			delete getter->acquire_getter_progress_dialog; /* TODO: move this to destructor. */
		}

		delete getter;
		return;
	}


	getter->configure_target_layer(mode);
	getter->acquire_is_running = true;





	if (getter->data_source->is_thread) {

		/* Start the acquire task in a background thread and
		   then block this foreground (UI) thread by showing a
		   dialog. We need to block this tread to prevent the
		   UI focus from going back to main window.

		   Until a background acquire thread is in progress,
		   its progress window must be in foreground. */

		QThreadPool::globalInstance()->start(getter);

		if (getter->acquire_getter_progress_dialog) {
			getter->acquire_getter_progress_dialog->exec();
		}


		/* We get here if user closed a dialog window.  The
		   window was either closed through "Cancel" button
		   when the acquire process was still in progress, or
		   through "OK" button that became active when acquire
		   process has been completed. */
		if (getter->acquire_is_running) {
			/* Cancel and mark for thread to finish. */
			getter->acquire_is_running = false;
		} else {
#ifdef K_FIXME_RESTORE
			/* Get data for Off command. */
			if (getter->data_source->off_func) {
				QString babel_args_off;
				QString file_path_off;
				interface->off_func(pass_along_data, babel_args_off, file_path_off);

				if (!babel_args_off.isEmpty()) {
					/* Turn off. */
					BabelOptions off_options;
					off_options.input = file_path_off;
					off_options.babel_args = babel_args_off;
					off_options.turn_off_device();
				}
			}
#endif
		}
	} else {
		/* Bypass thread method malarkly - you'll just have to wait... */
		qDebug() << SG_PREFIX_I << "Ready to run acquire process, non-thread method";

		bool success = getter->data_source->acquire_into_layer(getter->acquire_context.target_trw, getter->acquire_context, getter->acquire_getter_progress_dialog);
		if (!success) {
			Dialog::error(QObject::tr("Error: acquisition failed."), getter->acquire_context.window);
		}

		getter->on_complete_process();

		/* Actually show it if necessary. */
		if (getter->data_source->keep_dialog_open) {
			if (getter->acquire_getter_progress_dialog) {
				getter->acquire_getter_progress_dialog->exec();
			}
		}
	}


	delete progress_dialog;
}




DataSource::~DataSource()
{
	delete this->acquire_options;
	delete this->download_options;
}




DataProgressDialog * DataSource::create_progress_dialog(const QString & title)
{
	DataProgressDialog * dialog = new DataProgressDialog(title);

	return dialog;
}




BabelOptions * DataSourceDialog::create_acquire_options_layer(LayerTRW * trw)
{
	qDebug() << "II" PREFIX << "input type: TRWLayer";

	BabelOptions * process_options = NULL;

	const QString layer_file_full_path = GPX::write_tmp_file(trw, NULL);
	process_options = this->get_acquire_options_layer(layer_file_full_path);
	Util::add_to_deletion_list(layer_file_full_path);

	return process_options;
}




BabelOptions * DataSourceDialog::create_acquire_options_layer_track(LayerTRW * trw, Track * trk)
{
	qDebug() << "II" PREFIX << "input type: TRWLayerTrack";

	const QString layer_file_full_path = GPX::write_tmp_file(trw, NULL);
	const QString track_file_full_path = GPX::write_track_tmp_file(trk, NULL);

	BabelOptions * process_options = this->get_acquire_options_layer_track(layer_file_full_path, track_file_full_path);

	Util::add_to_deletion_list(layer_file_full_path);
	Util::add_to_deletion_list(track_file_full_path);

	return process_options;
}




BabelOptions * DataSourceDialog::create_acquire_options_track(Track * trk)
{
	qDebug() << "II" PREFIX << "input type: Track";

	const QString track_file_full_path = GPX::write_track_tmp_file(trk, NULL);
	BabelOptions * process_options = this->get_acquire_options_layer_track("", track_file_full_path);

	return process_options;
}




BabelOptions * DataSourceDialog::create_acquire_options_none(void)
{
	qDebug() << "II" PREFIX << "input type: None";

	BabelOptions * process_options = this->get_acquire_options_none();

	return process_options;
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
