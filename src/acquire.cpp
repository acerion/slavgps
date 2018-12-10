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
#include "widget_list_selection.h"




using namespace SlavGPS;




#define SG_MODULE "Acquire"




extern Babel babel;




static QMap<QString, DataSource *> g_bfilters;
static Track * bfilter_track = NULL;
static AcquireContext * g_acquire_context = NULL;




AcquireWorker::AcquireWorker()
{
}




AcquireWorker::~AcquireWorker()
{
	qDebug() << SG_PREFIX_I;
}




void AcquireWorker::configure_target_layer(DataSourceMode mode)
{

	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@  context" << (quintptr) this->acquire_context;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@    layer" << (quintptr) this->acquire_context->target_trw;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@ viewport" << (quintptr) this->acquire_context->viewport;

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

	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@  context" << (quintptr) this->acquire_context;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@    layer" << (quintptr) this->acquire_context->target_trw;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@ viewport" << (quintptr) this->acquire_context->viewport;
}





/* Call the function when acquire process has been completed without
   termination or errors. */
void AcquireWorker::finalize_after_completion(void)
{
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@  context" << (quintptr) this->acquire_context;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@    layer" << (quintptr) this->acquire_context->target_trw;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@ viewport" << (quintptr) this->acquire_context->viewport;

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

			if (this->acquire_context->target_trw->is_in_tree()) {
				qDebug() << SG_PREFIX_W << "Target TRW layer is attached to tree, perhaps it should be disconnected from the tree";
			}

			delete this->acquire_context->target_trw;
			this->acquire_context->target_trw = NULL;
			return;
		}


		qDebug() << SG_PREFIX_I << "New layer is non-empty, will now process the layer";
		//this->acquire_context->top_level_layer->add_layer(this->acquire_context->target_trw, true);
		//this->acquire_context->top_level_layer->attach_children_to_tree();
	}


	this->acquire_context->target_trw->attach_children_to_tree();
	//this->acquire_context->viewport = ThisApp::get_main_viewport();
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

#ifdef FIXME_RESTORE
			/* We can't call this method directly. For datasource wikipedia this causes crash because dialog doesn't exist anymore. */
			this->progress_dialog->accept();
#endif
		}
	}
}




/* Call the function when acquire process has been terminated - either
   because of errors or because user cancelled it. */
void AcquireWorker::finalize_after_termination(void)
{
	if (this->acquire_context->target_trw_allocated) {
		this->acquire_context->target_trw->unref_layer();
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
	const sg_ret acquire_result = this->data_source->acquire_into_layer(this->acquire_context->target_trw, this->acquire_context, this->progress_dialog);
	this->acquire_is_running = false;

	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@  context" << (quintptr) this->acquire_context;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@    layer" << (quintptr) this->acquire_context->target_trw;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@ viewport" << (quintptr) this->acquire_context->viewport;

	if (acquire_result == sg_ret::ok) {
		qDebug() << SG_PREFIX_I << "Acquire process ended with success";
		this->finalize_after_completion();
	} else {
		qDebug() << SG_PREFIX_W << "Acquire process ended with error";
		this->finalize_after_termination();
	}

	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@  context" << (quintptr) this->acquire_context;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@    layer" << (quintptr) this->acquire_context->target_trw;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@ viewport" << (quintptr) this->acquire_context->viewport;
}




void Acquire::acquire_from_source(DataSource * new_data_source, DataSourceMode mode, AcquireContext * new_acquire_context)
{
	if (QDialog::Accepted != new_data_source->run_config_dialog(new_acquire_context)) {
		qDebug() << SG_PREFIX_I << "Data source config dialog returned !Accepted";
		return;
	}

	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@  context" << (quintptr) new_acquire_context;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@    layer" << (quintptr) new_acquire_context->target_trw;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@ viewport" << (quintptr) new_acquire_context->viewport;


	AcquireProgressDialog * progress_dialog = new_data_source->create_progress_dialog(QObject::tr("Acquiring"));
	progress_dialog->set_headline(QObject::tr("Importing data..."));
	progress_dialog->setMinimumHeight(400);

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

	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@  context" << (quintptr) new_acquire_context;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@    layer" << (quintptr) new_acquire_context->target_trw;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@ viewport" << (quintptr) new_acquire_context->viewport;


	/* Start the acquire task in a background thread and then
	   block this foreground (UI) thread by showing a dialog. We
	   need to block this tread to prevent the UI focus from going
	   back to main window.

	   Until a background acquire thread is in progress, its
	   progress window must be in foreground. */

	QThreadPool::globalInstance()->start(worker);

	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@  context" << (quintptr) new_acquire_context;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@    layer" << (quintptr) new_acquire_context->target_trw;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@ viewport" << (quintptr) new_acquire_context->viewport;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@    layer" << (quintptr) new_acquire_context->target_trw;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@ viewport" << (quintptr) new_acquire_context->viewport;

	if (worker->progress_dialog) {
		worker->progress_dialog->exec();
	}

	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@  context" << (quintptr) new_acquire_context;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@    layer" << (quintptr) new_acquire_context->target_trw;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@ viewport" << (quintptr) new_acquire_context->viewport;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@    layer" << (quintptr) new_acquire_context->target_trw;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@ viewport" << (quintptr) new_acquire_context->viewport;

#ifdef K_FIXME_RESTORE
	/* We get here if user closed a dialog window.  The window was
	   either closed through "Cancel" button when the acquire
	   process was still in progress, or through "OK" button that
	   became active when acquire process has been completed. */
	if (worker->acquire_is_running) {
		/* Cancel and mark for thread to finish. */
		worker->acquire_is_running = false;
	} else {

		data_source->on_complete();
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
	const QString filter_id = qa->data().toString();

	auto iter = g_bfilters.find(filter_id);
	if (iter == g_bfilters.end()) {
		qDebug() << SG_PREFIX_E << "Can't find bfilter with id" << filter_id;
		return;
	}

	Acquire::acquire_from_source(iter.value(), iter.value()->mode, g_acquire_context);

	return;
}




QMenu * Acquire::create_bfilter_menu(const QString & menu_label, DataSourceInputType input_type, QWidget * parent)
{
	QMenu * menu = NULL;

	for (auto iter = g_bfilters.begin(); iter != g_bfilters.end(); iter++) {
		const QString filter_id = iter.key();
		DataSource * filter = iter.value();

		if (filter->input_type != input_type) {
			qDebug() << SG_PREFIX_I << "Not adding filter" << filter->window_title << "to menu" << menu_label << ", type not matched";
			continue;
		}
		qDebug() << SG_PREFIX_I << "Adding filter" << filter->window_title << "to menu" << menu_label;

		if (!menu) { /* Do this just once, but return NULL if no filters. */
			menu = new QMenu(menu_label, parent);
		}

		QAction * action = new QAction(filter->window_title, g_acquire_context);
		action->setData(filter_id);
		QObject::connect(action, SIGNAL (triggered(bool)), g_acquire_context, SLOT (filter_trwlayer_cb(void)));
		menu->addAction(action);
	}

	return menu;
}




/**
   @brief Create a "Filter" sub menu intended for rightclicking on a TRW layer

   @return NULL if no filters are available for a TRW layer
   @return new menu otherwise
*/
QMenu * Acquire::create_bfilter_layer_menu(QWidget * parent)
{
	return Acquire::create_bfilter_menu(QObject::tr("&Filter"), DataSourceInputType::TRWLayer, parent);
}




/**
   @brief Create a sub menu intended for rightclicking on a TRWLayer's menu called "Filter with Track "TRACKNAME"..."

   @return NULL if no filters or no filter track has been set
   @return menu otherwise
*/
QMenu * Acquire::create_bfilter_layer_track_menu(QWidget * parent)
{
	if (bfilter_track == NULL) {
		return NULL;
	} else {
		g_acquire_context->target_trk = bfilter_track;

		const QString menu_label = QObject::tr("Filter with %1").arg(bfilter_track->name);
		return Acquire::create_bfilter_menu(menu_label, DataSourceInputType::TRWLayerTrack, parent);
	}
}




/**
   @brief Create a "Filter" sub menu intended for rightclicking on a TRW track

   @return NULL if no filters are available for a TRW track
   @return new menu otherwise
*/
QMenu * Acquire::create_bfilter_track_menu(QWidget * parent)
{
	return Acquire::create_bfilter_menu(QObject::tr("&Filter"), DataSourceInputType::Track, parent);
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
	DataSource * filter = NULL;


	/*** Input is LayerTRW. ***/
	Acquire::register_bfilter(new BFilterSimplify());
	Acquire::register_bfilter(new BFilterCompress());
	Acquire::register_bfilter(new BFilterDuplicates());
	Acquire::register_bfilter(new BFilterManual());

	/*** Input is a Track and a LayerTRW. ***/
	Acquire::register_bfilter(new BFilterPolygon());
	Acquire::register_bfilter(new BFilterExcludePolygon());


	g_acquire_context = new AcquireContext();
}




void Acquire::uninit(void)
{
	delete g_acquire_context;

	for (auto iter = g_bfilters.begin(); iter != g_bfilters.end(); iter++) {
		delete iter.value();
	}
}




sg_ret Acquire::register_bfilter(DataSource * bfilter)
{
	if (bfilter->type_id == "") {
		qDebug() << SG_PREFIX_E << "bfilter with empty id";
		return sg_ret::err;
	}

	auto iter = g_bfilters.find(bfilter->type_id);
	if (iter != g_bfilters.end()) {
		qDebug() << SG_PREFIX_E << "Duplicate bfilter with id" << bfilter->type_id;
		return sg_ret::err;
	}

	g_bfilters.insert(bfilter->type_id, bfilter);

	return sg_ret::err;
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

	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@  context" << (quintptr) g_acquire_context;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@    layer" << (quintptr) g_acquire_context->target_trw;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@ viewport" << (quintptr) g_acquire_context->viewport;
}




AcquireOptions::AcquireOptions()
{
}




AcquireOptions::~AcquireOptions()
{
	if (this->babel_process) {
		delete this->babel_process;
		this->babel_process = NULL;
	}
}




/**
 * @trw: The #LayerTRW where to insert the collected data
 * @shell_command: the command to run
 * @input_file_type:
 * @cb:	Optional callback function.
 * @cb_data: Passed along to cb
 * @not_used: Must use NULL
 *
 * Runs the input command in a shell (bash) and optionally uses GPSBabel to convert from input_file_type.
 * If input_file_type is %NULL, doesn't use GPSBabel. Input must be GPX (or Geocaching *.loc)
 *
 * Uses Babel::convert_through_gpx() to actually run the command. This function
 * prepares the command and temporary file, and sets up the arguments for bash.
 */
sg_ret AcquireOptions::import_with_shell_command(LayerTRW * trw, AcquireContext * acquire_context, AcquireProgressDialog * progr_dialog)
{
	qDebug() << SG_PREFIX_I << "Initial form of shell command" << this->shell_command;

	QString full_shell_command;
	if (this->input_data_format.isEmpty()) {
		/* Output of command will be redirected to GPX importer through read_stdout_cb(). */
		full_shell_command = this->shell_command;
	} else {
		/* "-" indicates output to stdout; stdout will be redirected to GPX importer through read_stdout_cb(). */
		full_shell_command = QString("%1 | %2 -i %3 -f - -o gpx -F -").arg(this->shell_command).arg(babel.gpsbabel_path).arg(this->input_data_format);

	}
	qDebug() << SG_PREFIX_I << "Final form of shell command" << full_shell_command;

#ifdef FIXME_RESTORE
	const QString program(BASH_LOCATION);
	const QStringList args(QStringList() << "-c" << full_shell_command);

	this->babel_process = new BabelProcess();
	this->babel_process->set_args(program, args);
	this->babel_process->set_acquire_context(acquire_context);
	this->babel_process->set_progress_dialog(progr_dialog);
	const sg_ret rv = this->babel_process->convert_through_gpx(trw);
	delete this->babel_process;
	return rv;
#else
	return sg_ret::ok;
#endif
}




int AcquireOptions::kill_babel_process(const QString & status)
{
	if (this->babel_process && QProcess::NotRunning != this->babel_process->process->state()) {
		return this->babel_process->kill(status);
	} else {
		return -3;
	}
}




/**
 * @trw: The #LayerTRW where to insert the collected data.
 * @url: The URL to fetch.
 * @input_file_type: If input_file_type is empty, input must be GPX.
 * @dl_options: Download options. If %NULL then default download options will be used.
 *
 * Download the file pointed by the URL and optionally uses GPSBabel to convert from input_type.
 * If input_file_type and babel_filters are empty, gpsbabel is not used.
 *
 * Returns: %true on successful invocation of GPSBabel or read of the GPX.
 */
sg_ret AcquireOptions::import_from_url(LayerTRW * trw, DownloadOptions * dl_options, AcquireProgressDialog * progr_dialog)
{
	/* If no download options specified, use defaults: */
	DownloadOptions babel_dl_options(2);
	if (dl_options) {
		babel_dl_options = *dl_options;
	}

	sg_ret ret = sg_ret::err;

	qDebug() << SG_PREFIX_D << "Input data format =" << this->input_data_format << ", url =" << this->source_url;


	QTemporaryFile tmp_file;
	if (!SGUtils::create_temporary_file(tmp_file, "tmp-viking.XXXXXX")) {
		return sg_ret::err;
	}
	const QString target_file_full_path = tmp_file.fileName();
	qDebug() << SG_PREFIX_D << "Temporary file:" << target_file_full_path;
	tmp_file.remove(); /* Because we only needed to confirm that a path to temporary file is "available"? */

	DownloadHandle dl_handle(&babel_dl_options);
	const DownloadStatus download_status = dl_handle.perform_download(this->source_url, target_file_full_path);

	if (DownloadStatus::Success == download_status) {
		if (!this->input_data_format.isEmpty()) {

			BabelProcess * file_importer = new BabelProcess();

			file_importer->set_input(this->input_data_format, target_file_full_path);
			file_importer->set_output("gpx", "-");

			ret = file_importer->convert_through_gpx(trw);
			delete file_importer;
		} else {
			/* Process directly the retrieved file. */
			qDebug() << SG_PREFIX_D << "Directly read GPX file" << target_file_full_path;

			QFile file(target_file_full_path);
			if (file.open(QIODevice::ReadOnly)) {
				ret = GPX::read_layer_from_file(file, trw);
			} else {
				ret = sg_ret::err;
				qDebug() << SG_PREFIX_E << "Failed to open file" << target_file_full_path << "for reading:" << file.error();
			}
		}
	}
	Util::remove(target_file_full_path);


	return ret;
}




/**
 * @trw: The TRW layer to place data into. Duplicate items will be overwritten.
 * @process_options: The options to control the appropriate processing function. See #AcquireOptions for more detail.
 * @cb: Optional callback function.
 * @cb_data: Passed along to cb.
 * @dl_options: If downloading from a URL use these options (may be NULL).
 *
 * Loads data into a trw layer from a file, using gpsbabel.  This routine is synchronous;
 * that is, it will block the calling program until the conversion is done. To avoid blocking, call
 * this routine from a worker thread.
 *
 * Returns: %true on success.
 */
sg_ret AcquireOptions::universal_import_fn(LayerTRW * trw, DownloadOptions * dl_options, AcquireContext * acquire_context, AcquireProgressDialog * progr_dialog)
{
	if (this->babel_process) {

		qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@  context" << (quintptr) acquire_context;
		qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@    layer" << (quintptr) acquire_context->target_trw;
		qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@ viewport" << (quintptr) acquire_context->viewport;
		qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@    layer" << (quintptr) acquire_context->target_trw;
		qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@ viewport" << (quintptr) acquire_context->viewport;


#if 1
		if (!trw->is_in_tree()) {
			acquire_context->top_level_layer->add_layer(trw, true);
		}
#endif


		BabelProcess * importer = new BabelProcess();

		importer->program_name = this->babel_process->program_name;
		importer->options      = this->babel_process->options;
		importer->input_type   = this->babel_process->input_type;
		importer->input_file   = this->babel_process->input_file;
		importer->filters      = this->babel_process->filters;
		importer->output_type  = this->babel_process->output_type;
		importer->output_file  = this->babel_process->output_file;


		importer->set_output("gpx", "-"); /* Output data appearing on stdout of gpsbabel will be redirected to input of GPX importer. */
		importer->set_acquire_context(acquire_context);
		importer->set_progress_dialog(progr_dialog);
		const sg_ret result = importer->convert_through_gpx(trw);

		delete importer;

		qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@  context" << (quintptr) acquire_context;
		qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@    layer" << (quintptr) acquire_context->target_trw;
		qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@ viewport" << (quintptr) acquire_context->viewport;
		qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@    layer" << (quintptr) acquire_context->target_trw;
		qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@ viewport" << (quintptr) acquire_context->viewport;

		return result;
	}


	switch (this->mode) {
	case AcquireOptions::Mode::FromURL:
		return this->import_from_url(trw, dl_options, progr_dialog);

	case AcquireOptions::Mode::FromShellCommand:
		return this->import_with_shell_command(trw, acquire_context, progr_dialog);

	default:
		qDebug() << SG_PREFIX_E << "Unexpected babel options mode" << (int) this->mode;
		return sg_ret::err;
	}
}
