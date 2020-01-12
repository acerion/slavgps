/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013-2015, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2020, Kamil Ignacak <acerion@wp.pl>
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




#include <QRunnable>
#include <QThreadPool>




#include "babel.h"
#include "datasources.h"
#include "dialog.h"
#include "gpx.h"
#include "geonames_search.h"
#include "layer_aggregate.h"
#include "layer_trw.h"
#include "layer_trw_import.h"
#include "layer_trw_track_internal.h"
#include "layers_panel.h"
#include "util.h"
#include "viewport_internal.h"
#include "widget_list_selection.h"
#include "window.h"




using namespace SlavGPS;




#define SG_MODULE "LayerTRW Import"




extern Babel babel;




AcquireWorker::AcquireWorker(DataSource * data_source, const AcquireContext & acquire_context)
{
	this->m_data_source = data_source;
	this->m_acquire_context = acquire_context;
}




AcquireWorker::~AcquireWorker()
{
	qDebug() << SG_PREFIX_I;
}




sg_ret AcquireWorker::configure_target_layer(TargetLayerMode mode)
{
	this->m_acquire_context.print_debug(__FUNCTION__, __LINE__);

	switch (mode) {
	case TargetLayerMode::CreateNewLayer:
		this->m_acquire_context.m_trw_is_allocated = true;
		break;

	case TargetLayerMode::AddToLayer:
		if (nullptr == this->m_acquire_context.m_trw) {
			qDebug() << SG_PREFIX_E << "Mode is 'AddToLayer' but existing layer is NULL";
			return sg_ret::err;
		}
		/* Don't create new layer, acquire data into existing
		   TRW layer. */
		this->m_acquire_context.m_trw_is_allocated = false;
		break;

	case TargetLayerMode::AutoLayerManagement:
		/* NOOP */
		break;

	case TargetLayerMode::ManualLayerManagement:
		/* Don't create in acquire - as datasource will
		   perform the necessary actions. */
		if (nullptr == this->m_acquire_context.m_trw) {
			qDebug() << SG_PREFIX_E << "Mode is 'ManualLayerManagement' but existing layer is NULL";
			return sg_ret::err;
		}
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected TargetLayerMode" << (int) mode;
		break;
	};


	if (this->m_acquire_context.m_trw_is_allocated) {
		this->m_acquire_context.m_trw = new LayerTRW();
		this->m_acquire_context.m_trw->set_coord_mode(this->m_acquire_context.m_gisview->get_coord_mode());
		this->m_acquire_context.m_trw->set_name(this->m_data_source->m_layer_title);
	}

	this->m_acquire_context.print_debug(__FUNCTION__, __LINE__);

	return sg_ret::ok;
}





/* Call the function when acquire process has been completed without
   termination or errors. */
void AcquireWorker::finalize_after_success(void)
{
	this->m_acquire_context.print_debug(__FUNCTION__, __LINE__);

	if (this->m_acquire_context.m_trw_is_allocated) {
		qDebug() << SG_PREFIX_I << "Layer has been freshly allocated";

		if (nullptr == this->m_acquire_context.m_trw) {
			qDebug() << SG_PREFIX_E << "Layer marked as allocated, but is NULL";
			return;
		}

		if (this->m_acquire_context.m_trw->is_empty()) {
			/* Acquire process ended without errors, but
			   zero new items were acquired. */
			qDebug() << SG_PREFIX_I << "Layer is empty, delete the layer";

			if (this->m_acquire_context.m_trw->is_in_tree()) {
				qDebug() << SG_PREFIX_W << "Target TRW layer is attached to tree, perhaps it should be disconnected from the tree";
			}

			qDebug() << SG_PREFIX_I << "Will now delete target trw";
			delete this->m_acquire_context.m_trw;
			this->m_acquire_context.m_trw = nullptr;
			return;
		}


		qDebug() << SG_PREFIX_I << "New layer is non-empty, will now process the layer";
	}


	this->m_acquire_context.m_trw->attach_children_to_tree();
	this->m_acquire_context.m_trw->post_read(this->m_acquire_context.m_gisview, true);
	/* View this data if desired - must be done after post read (so that the bounds are known). */
	if (this->m_data_source && this->m_data_source->m_autoview) {
		this->m_acquire_context.m_trw->move_viewport_to_show_all(this->m_acquire_context.m_gisview);
		// this->m_acquire_context.panel->emit_items_tree_updated_cb("acquire completed");
	}
}




/* Call the function when acquire process has been terminated - either
   because of errors or because user cancelled it. */
void AcquireWorker::finalize_after_failure(void)
{
	qDebug() << SG_PREFIX_I;

	if (this->m_acquire_context.m_trw_is_allocated) {
		delete this->m_acquire_context.m_trw;
	}

	return;
}




/* This routine is the worker thread. There is only one simultaneous acquire allowed. */
/* Re-implementation of QRunnable::run() */
void AcquireWorker::run(void)
{
	this->m_acquire_context.print_debug(__FUNCTION__, __LINE__);
	sleep(1); /* Time for progress dialog to open and block main UI thread. */
	this->m_acquire_context.print_debug(__FUNCTION__, __LINE__);


	this->m_acquire_is_running = true;
	const LoadStatus acquire_result = this->m_data_source->acquire_into_layer(this->m_acquire_context, this->m_progress_dialog);
	this->m_acquire_is_running = false;


	this->m_acquire_context.print_debug(__FUNCTION__, __LINE__);

	if (LoadStatus::Code::Success == acquire_result) {
		qDebug() << SG_PREFIX_I << "Acquire process ended with success";
		this->finalize_after_success();

		qDebug() << SG_PREFIX_SIGNAL << "Will now signal successful completion of acquire";
		emit this->completed_with_success();
	} else {
		qDebug() << SG_PREFIX_W << "Acquire process ended with error" << acquire_result;
		this->finalize_after_failure();

		qDebug() << SG_PREFIX_SIGNAL << "Will now signal unsuccessful completion of acquire";
		emit this->completed_with_failure();
	}

	this->m_acquire_context.print_debug(__FUNCTION__, __LINE__);

	this->m_data_source->on_complete();
}




sg_ret AcquireWorker::build_progress_dialog(void)
{
	/* The dialog has Qt::WA_DeleteOnClose flag set. */
	this->m_progress_dialog = this->m_data_source->create_progress_dialog(QObject::tr("Acquiring"));

	if (nullptr == this->m_data_source->m_acquire_options) {
		/* This shouldn't happen... */
		qDebug() << SG_PREFIX_E << "Acquire options are NULL";

		this->m_progress_dialog->set_headline(QObject::tr("Unable to create command\nAcquire method failed."));
		this->m_progress_dialog->exec(); /* FIXME: improve handling of invalid process options. */
		delete this->m_progress_dialog;

		return sg_ret::err;
	}

	this->m_acquire_context.print_debug(__FUNCTION__, __LINE__);

	connect(this, SIGNAL (completed_with_success(void)), this->m_progress_dialog, SLOT (handle_acquire_completed_with_success_cb(void)));
	connect(this, SIGNAL (completed_with_failure(void)), this->m_progress_dialog, SLOT (handle_acquire_completed_with_failure_cb(void)));

	return sg_ret::ok;
}




sg_ret Acquire::acquire_from_source(DataSource * data_source, TargetLayerMode mode, AcquireContext & acquire_context)
{
	if (QDialog::Accepted != data_source->run_config_dialog(acquire_context)) {
		qDebug() << SG_PREFIX_I << "Data source config dialog returned !Accepted";
		return sg_ret::ok;
	}

	acquire_context.print_debug(__FUNCTION__, __LINE__);



	AcquireWorker * worker = new AcquireWorker(data_source, acquire_context); /* FIXME: worker needs to be deleted. */
	if (sg_ret::ok != worker->build_progress_dialog()) {
		return sg_ret::err;
	}
	if (sg_ret::ok != worker->configure_target_layer(mode)) {
		Dialog::error(QObject::tr("Failed to prepare importing of data"), nullptr);
		return sg_ret::err;
	}
	sleep(1);

	worker->m_acquire_context.print_debug(__FUNCTION__, __LINE__);


	/* Start the acquire task in a background thread and then
	   block this foreground (UI) thread by showing a dialog. We
	   need to block this tread to prevent the UI focus from going
	   back to main window.

	   Until a background acquire thread is in progress, its
	   progress window must be in foreground. */

	worker->m_acquire_context.print_debug(__FUNCTION__, __LINE__);

	if (worker->m_progress_dialog) {
		worker->m_progress_dialog->setModal(true);
		/* Return immediately, go to starting a worker thread. */
		worker->m_progress_dialog->show();
	}

	worker->m_acquire_context.print_debug(__FUNCTION__, __LINE__);
	QThreadPool::globalInstance()->start(worker); /* Worker will auto-delete itself. */
	worker->m_acquire_context.print_debug(__FUNCTION__, __LINE__);

	return sg_ret::ok;
}




AcquireContext::AcquireContext()
{
}




AcquireContext & AcquireContext::operator=(const AcquireContext & rhs)
{
	if (this == &rhs) {
		return *this;
	}

	this->m_window           = rhs.m_window;
	this->m_gisview          = rhs.m_gisview;
	this->m_parent_layer     = rhs.m_parent_layer;
	this->m_trw              = rhs.m_trw;
	this->m_trk              = rhs.m_trk;
	this->m_trw_is_allocated = rhs.m_trw_is_allocated;

	return *this;
}




AcquireOptions::~AcquireOptions()
{
	delete this->babel_process;
	this->babel_process = nullptr;
}




/**
 * @trw: The #LayerTRW where to insert the collected data
 * @shell_command: the command to run
 * @input_file_type:
 * @cb:	Optional callback function.
 * @cb_data: Passed along to cb
 *
 * Runs the input command in a shell (bash) and optionally uses GPSBabel to convert from input_file_type.
 * If input_file_type is %NULL, doesn't use GPSBabel. Input must be GPX (or Geocaching *.loc)
 *
 * Uses Babel::convert_through_gpx() to actually run the command. This function
 * prepares the command and temporary file, and sets up the arguments for bash.
 */
LoadStatus AcquireOptions::import_with_shell_command(__attribute__((unused)) AcquireContext & acquire_context, __attribute__((unused)) AcquireProgressDialog * progr_dialog)
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
	return LoadStatus::Code::Success;
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
LoadStatus AcquireOptions::import_from_url(AcquireContext & acquire_context, const DownloadOptions * dl_options, __attribute__((unused)) AcquireProgressDialog * progr_dialog)
{
	/* If no download options specified, use defaults: */
	DownloadOptions babel_dl_options(2);
	if (dl_options) {
		babel_dl_options = *dl_options;
	}

	LoadStatus load_status = LoadStatus::Code::Error;

	qDebug() << SG_PREFIX_D << "Input data format =" << this->input_data_format << ", url =" << this->source_url;


	QTemporaryFile tmp_file;
	if (!SGUtils::create_temporary_file(tmp_file, "tmp-viking.XXXXXX")) {
		return LoadStatus::Code::IntermediateFileAccess;
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

			load_status = file_importer->convert_through_gpx(acquire_context.m_trw);
			delete file_importer;
		} else {
			/* Process directly the retrieved file. */
			qDebug() << SG_PREFIX_D << "Directly read GPX file" << target_file_full_path;

			QFile file(target_file_full_path);
			if (file.open(QIODevice::ReadOnly)) {
				load_status = GPX::read_layer_from_file(file, acquire_context.m_trw);
			} else {
				load_status = LoadStatus::Code::FileAccess;
				qDebug() << SG_PREFIX_E << "Failed to open file" << target_file_full_path << "for reading:" << file.error();
			}
		}
	}
	Util::remove(target_file_full_path);


	return load_status;
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
LoadStatus AcquireOptions::universal_import_fn(AcquireContext & acquire_context, DownloadOptions * dl_options, AcquireProgressDialog * progr_dialog)
{
	if (this->babel_process) {

		if (!acquire_context.m_trw->is_in_tree()) {
			acquire_context.m_parent_layer->add_child_item(acquire_context.m_trw, true);
		}


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
		const LoadStatus result = importer->convert_through_gpx(acquire_context.m_trw);

		delete importer;

		return result;
	}


	switch (this->mode) {
	case AcquireOptions::Mode::FromURL:
		return this->import_from_url(acquire_context, dl_options, progr_dialog);

	case AcquireOptions::Mode::FromShellCommand:
		return this->import_with_shell_command(acquire_context, progr_dialog);

	default:
		qDebug() << SG_PREFIX_E << "Unexpected babel options mode" << (int) this->mode;
		return LoadStatus::Code::InternalError;
	}
}




void AcquireContext::print_debug(const char * function, int line) const
{
	qDebug() << SG_PREFIX_I << "@@@@@@";
	qDebug() << SG_PREFIX_I << "@@@@@@   layer" << (quintptr) this->m_trw << function << line;
	qDebug() << SG_PREFIX_I << "@@@@@@ gisview" << (quintptr) this->m_gisview << function << line;
	qDebug() << SG_PREFIX_I << "@@@@@@";
}




LayerTRWImporter::LayerTRWImporter(Window * window, GisViewport * gisview, Layer * parent_layer)
{
	/* Some tests to detect mixing of function arguments. */
	if (LayerKind::Aggregate != parent_layer->m_kind && LayerKind::GPS != parent_layer->m_kind) {
		qDebug() << SG_PREFIX_E << "Parent layer has wrong kind" << parent_layer->m_kind;
	}

	this->ctx.m_window = window;
	this->ctx.m_gisview = gisview;
	this->ctx.m_parent_layer = parent_layer;
}




LayerTRWImporter::LayerTRWImporter(Window * window, GisViewport * gisview, Layer * parent_layer, LayerTRW * existing_trw)
{
	/* Some tests to detect mixing of function arguments. */
	if (LayerKind::Aggregate != parent_layer->m_kind && LayerKind::GPS != parent_layer->m_kind) {
		qDebug() << SG_PREFIX_E << "Parent layer has wrong kind" << parent_layer->m_kind;
	}
	if (LayerKind::TRW != existing_trw->m_kind) {
		qDebug() << SG_PREFIX_E << "'existing trw' layer has wrong kind" << existing_trw->m_kind;
	}

	this->ctx.m_window = window;
	this->ctx.m_gisview = gisview;
	this->ctx.m_parent_layer = parent_layer;
	this->ctx.m_trw = existing_trw;
}




sg_ret LayerTRWImporter::import_into_existing_layer(DataSource * data_source)
{
	if (nullptr == this->ctx.m_trw) {
		qDebug() << SG_PREFIX_E << "Trying to import into existing layer, but existing TRW is not set";
		return sg_ret::err;
	}
	if (nullptr == this->ctx.m_parent_layer) {
		qDebug() << SG_PREFIX_E << "Trying to import into existing layer, but parent layer is not set";
		return sg_ret::err;
	}

	return Acquire::acquire_from_source(data_source, TargetLayerMode::AddToLayer, this->ctx);
}




sg_ret LayerTRWImporter::import_into_new_layer(DataSource * data_source)
{
	if (nullptr == this->ctx.m_parent_layer) {
		qDebug() << SG_PREFIX_E << "Trying to import into existing layer, but parent layer is not set";
		return sg_ret::err;
	}

	return Acquire::acquire_from_source(data_source, TargetLayerMode::CreateNewLayer, this->ctx);
}




void LayerTRWImporter::import_into_new_layer_from_gps_cb(void)
{
	this->import_into_new_layer(new DataSourceGPS());
}




void LayerTRWImporter::import_into_new_layer_from_file_cb(void)
{
	this->import_into_new_layer(new DataSourceFile());
}




void LayerTRWImporter::import_into_new_layer_from_geojson_cb(void)
{
	this->import_into_new_layer(new DataSourceGeoJSON());
}




void LayerTRWImporter::import_into_new_layer_from_routing_cb(void)
{
	this->import_into_new_layer(new DataSourceRouting());
}




void LayerTRWImporter::import_into_new_layer_from_osm_cb(void)
{
	this->import_into_new_layer(new DataSourceOSMTraces());
}




void LayerTRWImporter::import_into_new_layer_from_my_osm_cb(void)
{
	this->import_into_new_layer(new DataSourceOSMMyTraces());
}




#ifdef VIK_CONFIG_GEOCACHES
void LayerTRWImporter::import_into_new_layer_from_gc_cb(void)
{
	if (!DataSourceGeoCache::have_programs()) {
		return;
	}

	this->import_into_new_layer(new DataSourceGeoCache(ThisApp::get_main_gis_view()));
}
#endif




#ifdef VIK_CONFIG_GEOTAG
void LayerTRWImporter::import_into_new_layer_from_geotag_cb(void)
{
	this->import_into_new_layer(new DataSourceGeoTag());
}
#endif




#ifdef VIK_CONFIG_GEONAMES
void LayerTRWImporter::import_into_new_layer_from_wikipedia_cb(void)
{
	this->import_into_new_layer(new DataSourceWikipedia());
}
#endif




void LayerTRWImporter::import_into_new_layer_from_url_cb(void)
{
	this->import_into_new_layer(new DataSourceURL());
}




/*
 * Import into existing TRW Layer straight from GPS Device.
 */
void LayerTRWImporter::import_into_existing_layer_from_gps_cb(void)
{
	this->import_into_existing_layer(new DataSourceGPS());
}




/*
 * Import into existing TRW Layer from Directions.
 */
void LayerTRWImporter::import_into_existing_layer_from_routing_cb(void) /* Slot. */
{
	this->import_into_existing_layer(new DataSourceRouting());
}




/*
 * Import into existing TRW Layer from an entered URL.
 */
void LayerTRWImporter::import_into_existing_layer_from_url_cb(void) /* Slot. */
{
	this->import_into_existing_layer(new DataSourceURL());
}




/*
 * Import into existing TRW Layer from OSM.
 */
void LayerTRWImporter::import_into_existing_layer_from_osm_cb(void) /* Slot. */
{
	this->import_into_existing_layer(new DataSourceOSMTraces());
}




/**
 * Import into existing TRW Layer from OSM for 'My' Traces.
 */
void LayerTRWImporter::import_into_existing_layer_from_osm_my_traces_cb(void) /* Slot. */
{
	this->import_into_existing_layer(new DataSourceOSMMyTraces());
}




#ifdef VIK_CONFIG_GEOCACHES
/*
 * Import into existing TRW Layer from Geocaching.com
 */
void LayerTRWImporter::import_into_existing_layer_from_geocache_cb(void) /* Slot. */
{
	this->import_into_existing_layer(new DataSourceGeoCache(ThisApp::get_main_gis_view()));
}
#endif




#ifdef VIK_CONFIG_GEOTAG
/*
 * Import into existing TRW Layer from images.
 */
void LayerTRWImporter::import_into_existing_layer_from_geotagged_images_cb(void) /* Slot. */
{
	this->import_into_existing_layer(new DataSourceGeoTag());

#ifdef K_TODO_LATER /* Re-implement/re-enable. */
	/* Re-generate thumbnails as they may have changed.
	   TODO_MAYBE: move this somewhere else, where we are sure that the acquisition has been completed? */
	this->has_missing_thumbnails = true;
	this->generate_missing_thumbnails();
#endif
}
#endif




/*
 * Import into existing TRW Layer from any GPS Babel supported file.
 */
void LayerTRWImporter::import_into_existing_layer_from_file_cb(void) /* Slot. */
{
	this->import_into_existing_layer(new DataSourceFile());
}




void LayerTRWImporter::import_into_existing_layer_from_wikipedia_waypoints_viewport_cb(void) /* Slot. */
{
	Geonames::create_wikipedia_waypoints(this->ctx.m_trw, this->ctx.m_gisview->get_bbox(), this->ctx.m_window);

	this->ctx.m_trw->waypoints.recalculate_bbox();
	this->ctx.m_trw->emit_tree_item_changed("Redrawing items after adding wikipedia waypoints");
}




void LayerTRWImporter::import_into_existing_layer_from_wikipedia_waypoints_layer_cb(void) /* Slot. */
{
	Geonames::create_wikipedia_waypoints(this->ctx.m_trw, this->ctx.m_trw->get_bbox(), this->ctx.m_window);

	this->ctx.m_trw->waypoints.recalculate_bbox();
	this->ctx.m_trw->emit_tree_item_changed("Redrawing items after adding wikipedia waypoints");
}
