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
	class Viewport;
	class Window;
	class LayerTRW;
	class Track;
	class DataSource;




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




	class AcquireOptions {
	public:
		virtual bool is_valid(void) const = 0;
	};




	/**
	   Global data structure used to expose the progress dialog to the worker thread.
	*/
	class AcquireProcess : public AcquireTool {
		Q_OBJECT
	public:
		AcquireProcess() {};
		AcquireProcess(Window * new_window, LayersPanel * new_panel, Viewport * new_viewport) : window(new_window), panel(new_panel), viewport(new_viewport) {};

		void acquire_from_source(DataSource * data_source, DataSourceMode mode);
		void set_context(Window * window, LayersPanel * panel, Viewport * viewport, LayerTRW * trw, Track * trk);

		void acquire(DataSource * new_data_source, DataSourceMode mode, void * parent_data_source_dialog);

		void import_progress_cb(AcquireProgressCode code, void * data);

		void configure_target_layer(DataSourceMode mode);

		enum {
			Success,
			Failure
		};

		Window * window = NULL;
		LayersPanel * panel = NULL;
		Viewport * viewport = NULL;
		LayerTRW * trw = NULL;
		Track * trk = NULL;

		bool acquire_is_running = false;

		bool creating_new_layer = false;

		DataSource * data_source = NULL;
		DataSourceDialog * config_dialog = NULL;
		DataSourceDialog * parent_data_source_dialog = NULL;
		DataProgressDialog * progress_dialog = NULL;

	public slots:
		void filter_trwlayer_cb(void);
		void handle_getter_status_cb(int status);
	};




	class DataSource {
	public:
		DataSource() {};
		virtual ~DataSource();

		virtual bool acquire_into_layer(LayerTRW * trw, AcquireTool * babel_something) { return false; };
		virtual void progress_func(AcquireProgressCode code, void * data, AcquireProcess * acquiring) { return; };
		virtual void cleanup(void * data) { return; };
		virtual int kill(const QString & status) { return -1; };

		virtual int run_config_dialog(AcquireProcess * acquire_context) { return QDialog::Rejected; };

		virtual DataProgressDialog * create_progress_dialog(const QString & title);

		QString window_title;
		QString layer_title;


		DataSourceMode mode;
		DataSourceInputType input_type;

		bool autoview = false;
		bool keep_dialog_open = false; /* ... when done. */
		bool is_thread = false;

		AcquireOptions * acquire_options = NULL;
		DownloadOptions * download_options = NULL;
	};




	/* Remember that by default QRunnable is auto-deleted on thread exit. */
	class AcquireGetter : public QObject, public QRunnable {
		Q_OBJECT
	public:
		AcquireGetter() {};
		~AcquireGetter();
		void run(); /* Re-implementation of QRunnable::run(). */
		void on_complete_process(void);


		bool result = false;
		DataSource * data_source = NULL;
		AcquireProcess * acquiring = NULL;
		DataProgressDialog * progress_dialog = NULL;
	signals:
		void report_status(int status);
	};




	class Acquire {
	public:
		static void init(void);
		static void uninit(void);

		static void set_context(Window * window, LayersPanel * panel, Viewport * viewport, LayerTRW * trw, Track * trk);

		static QMenu * create_bfilter_layer_menu(void);
		static QMenu * create_bfilter_layer_track_menu(void);
		static QMenu * create_bfilter_track_menu(void);

		static void set_bfilter_track(Track * trk);

	private:
		static QMenu * create_bfilter_menu(const QString & menu_label, DataSourceInputType input_type);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_ACQUIRE_H_ */
