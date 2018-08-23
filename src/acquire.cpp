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
#include <unistd.h>




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
static Track * bfilter_track = NULL;
static AcquireProcess * g_acquiring = NULL;




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
	    this->acquiring->running
#else
	    true
#endif
	    ) {
		emit report_status(3);

		if (this->acquiring->creating_new_layer) {

			/* Only create the layer if it actually contains anything useful. */
			/* TODO_LATER: create function for this operation to hide detail: */
			if (!this->acquiring->trw->is_empty()) {
				this->acquiring->trw->post_read(this->acquiring->viewport, true);
				this->acquiring->panel->get_top_layer()->add_layer(this->acquiring->trw, true);

				/* Add any acquired tracks/routes/waypoints to tree view
				   so that they can be displayed on "tree updated" event. */
				this->acquiring->trw->add_children_to_tree();
			} else {
				emit report_status(4);
			}
		}

		if (this->data_source->config_dialog) {
			if (this->data_source->keep_dialog_open) {
				/* Only allow dialog's validation when format selection is done. */
				this->data_source->config_dialog->button_box->button(QDialogButtonBox::Ok)->setEnabled(true);
				this->data_source->config_dialog->button_box->button(QDialogButtonBox::Cancel)->setEnabled(false);
			} else {
				/* Call 'accept()' slot to close the dialog. */
				this->data_source->config_dialog->accept();
			}
		}

		/* Main display update. */
		if (this->acquiring->trw) {
			this->acquiring->trw->post_read(this->acquiring->viewport, true);
			/* View this data if desired - must be done after post read (so that the bounds are known). */
			if (this->data_source && this->data_source->autoview) {
				this->acquiring->trw->move_viewport_to_show_all(this->acquiring->viewport);
			}
			/// this->acquiring->panel->emit_items_tree_updated_cb("acquire completed");
		}
	} else {
		/* Cancelled. */
		if (this->acquiring->creating_new_layer) {
			this->acquiring->trw->unref();
		}
	}
}




void AcquireProcess::configure_target_layer(DataSourceMode mode)
{
	switch (mode) {
	case DataSourceMode::CreateNewLayer:
		this->creating_new_layer = true;
		break;

	case DataSourceMode::AddToLayer: {
		Layer * selected_layer = this->panel->get_selected_layer();
		if (selected_layer->type == LayerType::TRW) {
			this->trw = (LayerTRW *) selected_layer;
			this->creating_new_layer = false;
		}
		}
		break;

	case DataSourceMode::AutoLayerManagement:
		/* NOOP */
		break;

	case DataSourceMode::ManualLayerManagement: {
		/* Don't create in acquire - as datasource will perform the necessary actions. */
		this->creating_new_layer = false;
		Layer * selected_layer = this->panel->get_selected_layer();
		if (selected_layer->type == LayerType::TRW) {
			this->trw = (LayerTRW *) selected_layer;
		}
		}
		break;
	default:
		qDebug() << "EE" PREFIX << "unexpected DataSourceMode" << (int) mode;
		break;
	};


	if (this->creating_new_layer) {
		this->trw = new LayerTRW();
		this->trw->set_coord_mode(this->viewport->get_coord_mode());
		this->trw->set_name(this->data_source->layer_title);
	}
}




/* This routine is the worker thread. There is only one simultaneous download allowed. */
/* Re-implementation of QRunnable::run() */
void AcquireGetter::run(void)
{
	assert (this->data_source);

	this->result = this->data_source->acquire_into_layer(this->acquiring->trw, this->acquiring);

	if (this->acquiring->acquire_is_running && !this->result) {
		this->acquiring->progress_dialog->set_status(QObject::tr("Error: acquisition failed."));
		if (this->acquiring->creating_new_layer) {
			this->acquiring->trw->unref();
		}
	} else {
		this->on_complete_process();
	}

	this->data_source->cleanup(this->acquiring->parent_data_source_dialog);

	this->acquiring->acquire_is_running = false;
}




void AcquireProcess::acquire(DataSource * new_data_source, DataSourceMode mode, void * new_parent_data_source_dialog)
{
	this->parent_data_source_dialog = (DataSourceDialog *) new_parent_data_source_dialog;
	this->data_source = new_data_source;

	if (QDialog::Accepted != new_data_source->run_config_dialog(this)) {
		qDebug() << "II" PREFIX << "Data source config dialog returned !Accepted";
		return;
	}

	AcquireGetter getter;
	getter.acquiring = this;
	getter.data_source = new_data_source;
	getter.acquiring->creating_new_layer = (!this->trw); /* Default if Auto Layer Management is passed in. */




	this->creating_new_layer = (!this->trw); /* Default if Auto Layer Management is passed in. */
	this->configure_target_layer(mode);
	this->acquire_is_running = true;
	this->progress_dialog = new_data_source->create_progress_dialog(QObject::tr("Acquiring"));
	this->progress_dialog->set_status(QObject::tr("Importing data..."));


	if (!new_data_source->process_options->is_valid()) {
		/* This shouldn't happen... */
		this->progress_dialog->set_status(QObject::tr("Unable to create command\nAcquire method failed."));
		this->progress_dialog->exec(); /* TODO_LATER: improve handling of invalid process options. */
		delete this->progress_dialog;
		return;
	}

	if (new_data_source->is_thread) {
		getter.run();

		if (progress_dialog) {
			progress_dialog->exec();
		}

		if (this->acquire_is_running) {
			/* Cancel and mark for thread to finish. */
				this->acquire_is_running = false;
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

		bool success = new_data_source->acquire_into_layer(getter.acquiring->trw, this);
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
}




void AcquireProcess::import_progress_cb(AcquireProgressCode code, void * data)
{
	assert (this->data_source);

	if (this->data_source->is_thread) {
		if (!this->acquire_is_running) {
			this->data_source->cleanup(this->parent_data_source_dialog);
		}
	}

#ifdef K_FIXME_RESTORE
	/* See DataSourceGPS::progress_func(). */
	this->data_source->progress_func(code, data, this);
#endif
}




DataSource::~DataSource()
{
	delete this->process_options;
	delete this->download_options;
	delete this->config_dialog;
}




DataProgressDialog * DataSource::create_progress_dialog(const QString & title)
{
	DataProgressDialog * dialog = new DataProgressDialog(title);

	return dialog;
}




BabelOptions * DataSourceDialog::create_process_options_layer(LayerTRW * trw)
{
	qDebug() << "II" PREFIX << "input type: TRWLayer";

	BabelOptions * process_options = NULL;

	const QString layer_file_full_path = GPX::write_tmp_file(trw, NULL);
	process_options = this->get_process_options_layer(layer_file_full_path);
	Util::add_to_deletion_list(layer_file_full_path);

	return process_options;
}




BabelOptions * DataSourceDialog::create_process_options_layer_track(LayerTRW * trw, Track * trk)
{
	qDebug() << "II" PREFIX << "input type: TRWLayerTrack";

	const QString layer_file_full_path = GPX::write_tmp_file(trw, NULL);
	const QString track_file_full_path = GPX::write_track_tmp_file(trk, NULL);

	BabelOptions * process_options = this->get_process_options_layer_track(layer_file_full_path, track_file_full_path);

	Util::add_to_deletion_list(layer_file_full_path);
	Util::add_to_deletion_list(track_file_full_path);

	return process_options;
}




BabelOptions * DataSourceDialog::create_process_options_track(Track * trk)
{
	qDebug() << "II" PREFIX << "input type: Track";

	const QString track_file_full_path = GPX::write_track_tmp_file(trk, NULL);
	BabelOptions * process_options = this->get_process_options_layer_track("", track_file_full_path);

	return process_options;
}




BabelOptions * DataSourceDialog::create_process_options_none(void)
{
	qDebug() << "II" PREFIX << "input type: None";

	BabelOptions * process_options = this->get_process_options_none();

	return process_options;
}




void AcquireProcess::acquire_from_source(DataSource * new_data_source, DataSourceMode mode)
{
	this->acquire(new_data_source, mode, NULL);
}




void AcquireProcess::filter_trwlayer_cb(void)
{
	QAction * qa = (QAction *) QObject::sender();
	const int filter_idx = qa->data().toInt();

	this->acquire(g_bfilters[filter_idx], g_bfilters[filter_idx]->mode, NULL);
}




void AcquireProcess::handle_getter_status_cb(int status)
{
	switch (status) {
	case AcquireProcess::Success:
		qDebug() << "II" PREFIX << "Success";
		if (this->progress_dialog) {
			this->progress_dialog->set_status(QObject::tr("Import completed successfully"));
		}
		break;
	case AcquireProcess::Failure:
		qDebug() << "II" PREFIX << "Failure";
		if (this->progress_dialog) {
			this->progress_dialog->set_status(QObject::tr("Failed to import data"));
		}
	case 3:
		qDebug() << "II" PREFIX << "Done";
		if (this->progress_dialog) {
			this->progress_dialog->set_status(QObject::tr("Done"));
		}
		break;
	case 4:
		qDebug() << "II" PREFIX << "No Data";
		if (this->progress_dialog) {
			this->progress_dialog->set_status(QObject::tr("No Data"));
		}
		break;
	default:
		qDebug() << "EE" PREFIX << "Unhandled getter status" << status;
		break;
	};
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

			QAction * action = new QAction(filter->window_title, g_acquiring);
			action->setData(i);
			QObject::connect(action, SIGNAL (triggered(bool)), g_acquiring, SLOT (filter_trwlayer_cb(void)));
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
		g_acquiring->trk = bfilter_track;

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

	g_acquiring = new AcquireProcess();
}




void Acquire::uninit(void)
{
	delete g_acquiring;

	for (auto iter = g_bfilters.begin(); iter != g_bfilters.end(); iter++) {
		delete *iter;
	}
}




bool DataSourceBabel::acquire_into_layer(LayerTRW * trw, AcquireTool * progress_indicator)
{
	return this->process_options->universal_import_fn(trw, this->download_options, progress_indicator);
}




int DataSourceBabel::kill(const QString & status)
{
	if (this->process_options) {
		return this->process_options->kill(status);
	} else {
		return -4;
	}
}




void Acquire::set_context(Window * new_window, LayersPanel * new_panel, Viewport * new_viewport, LayerTRW * new_trw, Track * new_trk)
{
	g_acquiring->window = new_window;
	g_acquiring->panel = new_panel;
	g_acquiring->viewport = new_viewport;
	g_acquiring->trw = new_trw;
	g_acquiring->trk = new_trk;
}




void AcquireProcess::set_context(Window * new_window, LayersPanel * new_panel, Viewport * new_viewport, LayerTRW * new_trw, Track * new_trk)
{
	this->window = new_window;
	this->panel = new_panel;
	this->viewport = new_viewport;
	this->trw = new_trw;
	this->trk = new_trk;
}
