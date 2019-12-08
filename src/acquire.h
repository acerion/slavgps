/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_ACQUIRE_H_
#define _SG_ACQUIRE_H_




#include <QObject>
#include <QMenu>
#include <QDialog>
#include <QLabel>
#include <QRunnable>




#include "ui_builder.h"
#include "datasource.h"




namespace SlavGPS {




	class LayersPanel;
	class GisViewport;
	class Window;
	class LayerTRW;
	class LayerAggregate;
	class Track;
	class DataSource;
	class AcquireWorker;
	class AcquireOptions;
	class BabelProcess;




	enum class AcquireProgressCode {
		DiagOutput, /* A line of data/diagnostic output is available. */
		Completed,  /* Acquire tool completed work. */
	};




	class AcquireTool : public QObject {
		Q_OBJECT
	public:
		AcquireTool() {};
		virtual int kill(const QString & status) { return -1; };
		virtual void import_progress_cb(AcquireProgressCode code, void * data) { return; };
		virtual void export_progress_cb(AcquireProgressCode code, void * data) { return; };
	};




	class AcquireContext : public QObject {
		Q_OBJECT
	public:
		AcquireContext();
		AcquireContext(Window * new_window, GisViewport * new_gisview, LayerAggregate * new_top_level_layer, Layer * new_selected_layer)
			: window(new_window), gisview(new_gisview), top_level_layer(new_top_level_layer), selected_layer(new_selected_layer) {};

		void print_debug(const char * function, int line) const;

		Window * window = NULL;
		GisViewport * gisview = NULL;
		LayerAggregate * top_level_layer = NULL;
		Layer * selected_layer = NULL;


		LayerTRW * target_trw = NULL;
		Track * target_trk = NULL;

		/* Whether a target trw layer has been freshly
		   created, or it already existed in tree view. */
		bool target_trw_allocated = false;

	public slots:
		void filter_trwlayer_cb(void);
	};




	/* Remember that by default QRunnable is auto-deleted on thread exit. */
	class AcquireWorker : public QObject, public QRunnable {
		Q_OBJECT
	public:
		AcquireWorker(DataSource * data_source, const AcquireContext & acquire_context);
		~AcquireWorker();
		void run(); /* Re-implementation of QRunnable::run(). */
		void configure_target_layer(DataSourceMode mode);

		void finalize_after_completion(void);
		void finalize_after_termination(void);

		sg_ret build_progress_dialog(void);

		AcquireContext acquire_context;

		bool acquire_is_running = false;
		DataSource * data_source = NULL;
		AcquireProgressDialog * progress_dialog = NULL;

	signals:
		void report_status(int status);

		void completed_with_success(void);
		void completed_with_failure(void);
	};




	class Acquire {
	public:
		static void init(void);
		static void uninit(void);

		enum {
			Success,
			Failure
		};


		static void acquire_from_source(DataSource * data_source, DataSourceMode mode, AcquireContext & acquire_context);
		static void set_context(Window * window, GisViewport * gisview, LayerAggregate * top_level_layer, Layer * selected_layer);
		static void set_target(LayerTRW * trw, Track * trk);

		static QMenu * create_bfilter_layer_menu(QWidget * parent);
		static QMenu * create_bfilter_layer_track_menu(QWidget * parent);
		static QMenu * create_bfilter_track_menu(QWidget * parent);

		static void set_bfilter_track(Track * trk);

	private:
		static QMenu * create_bfilter_menu(const QString & menu_label, DataSourceInputType input_type, QWidget * parent);
		static sg_ret register_bfilter(DataSource * bfilter);
	};




	class AcquireOptions {
	public:
		enum Mode {
			FromURL,
			FromShellCommand
		};

		AcquireOptions();
		AcquireOptions(AcquireOptions::Mode new_mode) : mode(new_mode) { };
		virtual ~AcquireOptions();

		LoadStatus universal_import_fn(LayerTRW * trw, DownloadOptions * dl_options, AcquireContext * acquire_context, AcquireProgressDialog * progr_dialog);
		LoadStatus import_from_url(LayerTRW * trw, const DownloadOptions * dl_options, AcquireProgressDialog * progr_dialog);
		LoadStatus import_with_shell_command(LayerTRW * trw, AcquireContext * acquire_context, AcquireProgressDialog * progr_dialog);

		int kill_babel_process(const QString & status);

		QString source_url;        /* If first step in acquiring is getting a data from URL, this is the field to save the source URL. */
		QString input_data_format; /* If empty, then uses internal file format handler (GPX only ATM), otherwise specify gpsbabel input type like "kml","tcx", etc... */
		QString shell_command;     /* Optional shell command to run instead of gpsbabel - but will be (Unix) platform specific. */
		AcquireOptions::Mode mode;

		BabelProcess * babel_process = NULL;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_ACQUIRE_H_ */
