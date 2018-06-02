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
		virtual void import_progress_cb(AcquireProgressCode code, void * data) { return; };
		virtual void export_progress_cb(AcquireProgressCode code, void * data) { return; };
	};




	/**
	   Global data structure used to expose the progress dialog to the worker thread.
	*/
	class AcquireProcess : public AcquireTool {
		Q_OBJECT
	public:
		AcquireProcess() {};
		AcquireProcess(Window * new_window, LayersPanel * new_panel, Viewport * new_viewport) : window(new_window), panel(new_panel), viewport(new_viewport) {};

		void acquire(DataSource * new_data_source, DataSourceMode mode, void * parent_data_source_dialog);
		QMenu * build_menu(const QString & submenu_label, DataSourceInputType input_type);

		void import_progress_cb(AcquireProgressCode code, void * data);

		QLabel * status = NULL;
		Window * window = NULL;
		LayersPanel * panel = NULL;
		Viewport * viewport = NULL;
		LayerTRW * trw = NULL;
		Track * trk = NULL;

		bool running = false;

		DataSource * data_source = NULL;
		DataSourceDialog * data_source_dialog = NULL;
		DataSourceDialog * parent_data_source_dialog = NULL;

	public slots:
		void acquire_trwlayer_cb(void);
	};




	class DataSource {
	public:
		DataSource() {};
		virtual ~DataSource() {};

		virtual DataSourceDialog * create_setup_dialog(Viewport * viewport, void * user_data) { return NULL; };
		virtual bool process_func(LayerTRW * trw, ProcessOptions * process_options, DownloadOptions * download_options, AcquireTool * babel_something) { return false; };
		virtual void progress_func(AcquireProgressCode code, void * data, AcquireProcess * acquiring) { return; };
		virtual void cleanup(void * data) { return; };

		QString window_title;
		QString layer_title;

		DataSourceMode mode;
		DataSourceInputType input_type;

		bool autoview = false;
		bool keep_dialog_open = false; /* ... when done. */
		bool is_thread = false;
	};




	/* Remember that by default QRunnable is auto-deleted on thread exit. */
	class AcquireGetter : public QRunnable {
	public:
		AcquireGetter() {};
		~AcquireGetter();
		void run(); /* Re-implementation of QRunnable::run(). */
		void on_complete_process(void);


		AcquireProcess * acquiring = NULL;
		bool creating_new_layer = false;

		ProcessOptions * po = NULL;
		DownloadOptions * dl_options = NULL;
	};




	class Acquire {
	public:
		static void init(void);
		static void uninit(void);

		static void acquire_from_source(DataSource * new_data_source, DataSourceMode new_mode, Window * new_window, LayersPanel * new_panel, Viewport * new_viewport, DataSourceDialog * new_parent_data_source_dialog);

		static QMenu * create_trwlayer_menu(Window * window, LayersPanel * panel, Viewport * viewport, LayerTRW * trw);
		static QMenu * create_trwlayer_track_menu(Window * window, LayersPanel * panel, Viewport * viewport, LayerTRW * trw);
		static QMenu * create_track_menu(Window * window, LayersPanel * panel, Viewport * viewport, Track * trk);

		static void set_filter_track(Track * trk);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_ACQUIRE_H_ */
