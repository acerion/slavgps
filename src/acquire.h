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




#include <cstdint>

#include <QObject>
#include <QMenu>
#include <QDialog>
#include <QLabel>
#include <QRunnable>

#include "babel.h"
#include "ui_builder.h"
#include "datasource.h"




namespace SlavGPS {




	class LayersPanel;
	class Viewport;
	class Window;
	class LayerTRW;
	class Track;
	class DataSource;
	class DataSourceWebToolDialog;
	class WebToolDatasource;




	typedef struct _DataSourceInterface DataSourceInterface;

	typedef struct {
		Window * window;
		LayersPanel * panel;
		Viewport * viewport;
		void * userdata;
	} acq_vik_t;




	/**
	   Frees any widgets created for the setup or progress dialogs, any allocated state, etc.
	*/
	typedef void (* DataSourceCleanupFunc)(void * user_data);


	/**
	   Global data structure used to expose the progress dialog to the worker thread.
	*/
	class AcquireProcess : public QObject {
		Q_OBJECT
	public:
		AcquireProcess() {};
		AcquireProcess(Window * new_window, LayersPanel * new_panel, Viewport * new_viewport) : window(new_window), panel(new_panel), viewport(new_viewport) {};

		//void acquire(DataSourceMode mode, DataSourceInterface * source_interface, WebToolDatasource * web_tool_data_source, DataSourceCleanupFunc cleanup_function);
		void acquire(DataSource * new_data_source, DataSourceMode mode, void * parent_data_source_dialog);
		QMenu * build_menu(const QString & submenu_label, DatasourceInputtype inputtype);

		QLabel * status = NULL;
		Window * window = NULL;
		LayersPanel * panel = NULL;
		Viewport * viewport = NULL;
		LayerTRW * trw = NULL;
		Track * trk = NULL;

		DataSourceDialog * dialog_ = NULL;
		bool running = false;
		DataSourceInterface * source_interface = NULL;
		DataSource * data_source = NULL;
		DataSourceDialog * parent_data_source_dialog = NULL;

	public slots:
		void acquire_trwlayer_cb(void);
	};




	/**
	   Returns: pointer to state if OK, otherwise %NULL.
	*/
	typedef DataSourceWebToolDialog * (* DataSourceInitFunc)(LayersPanel * panel, Viewport * viewport, WebToolDatasource * data_source);

	/**
	   Create a dialog for configuring/setting up access to data source
	*/
	typedef DataSourceDialog * (* DataSourceCreateSetupDialogFunc)(Viewport * viewport, void * user_data);

	/**
	  @trw:
	  @process_options: options to control the behaviour of this function (see #ProcessOptions)
	  @status_cb: the #DataSourceInterface.progress_func
	  @acquiring: the widgets and data used by #DataSourceInterface.progress_func
	  @download_options: Optional options used if downloads from URLs is used.

	  The actual function to do stuff - must report success/failure.
	*/
	typedef bool (* DataSourceProcessFunc)(void * trw, ProcessOptions * process_options, BabelCallback cb, AcquireProcess * acquiring, void * download_options);




	struct _DataSourceInterface {
		QString window_title;
		QString layer_title;
		DataSourceMode mode;
		DatasourceInputtype inputtype;
		bool autoview;
		bool keep_dialog_open; /* ... when done. */

		bool is_thread;

		DataSourceInitFunc init_func;
		DataSourceCreateSetupDialogFunc create_setup_dialog_func;
		DataSourceProcessFunc process_func;
		DataSourceCleanupFunc cleanup_func;
	};





	class DataSource {
	public:
		DataSource() {};
		~DataSource() {};

		virtual DataSourceDialog * create_setup_dialog(Viewport * viewport, void * user_data) { return NULL; };
		virtual ProcessOptions * get_process_options(void * user_data, DownloadOptions * download_options, const char * input_file_name, const char * input_track_file_name) { return NULL; };
		virtual bool process_func(LayerTRW * trw, ProcessOptions * process_options, BabelCallback cb, AcquireProcess * acquiring, DownloadOptions * download_options) { return false; };
		virtual void progress_func(BabelProgressCode c, void * data, AcquireProcess * acquiring) { return; };

		QString window_title;
		QString layer_title;
		DataSourceMode mode;
		DatasourceInputtype inputtype;
		bool autoview;
		bool keep_dialog_open; /* ... when done. */

		bool is_thread;
	};




	/* Parent class for data sources that have the same process
	   function: a_babel_convert_from(), called either directly or
	   indirectly. */
	class DataSourceBabel : public DataSource {
	public:
		DataSourceBabel() {};
		~DataSourceBabel() {};

		virtual DataSourceDialog * create_setup_dialog(Viewport * viewport, void * user_data) { return NULL; };
		virtual bool process_func(LayerTRW * trw, ProcessOptions * process_options, BabelCallback cb, AcquireProcess * acquiring, DownloadOptions * download_options);
	};




	/* Passed along to worker thread. */
	typedef struct {
		AcquireProcess * acquiring = NULL;
		ProcessOptions * po = NULL;
		bool creating_new_layer = false;
		LayerTRW * trw = NULL;
		DownloadOptions * dl_options = NULL;
	} AcquireGetterParams;



	/* Remember that by default QRunnable is auto-deleted on thread exit. */
	class AcquireGetter : public QRunnable {
	public:
	AcquireGetter(AcquireGetterParams & getter_params) : params(getter_params) {}
		void run(); /* Re-implementation of QRunnable::get(). */

		AcquireGetterParams params;
	};







	//ProcessOptions * acquire_create_process_options(AcquireProcess * acq, DataSourceDialog * setup_dialog, DownloadOptions * dl_options, DataSourceInterface * interface, void * pass_along_data);
	ProcessOptions * acquire_create_process_options(AcquireProcess * acq, DataSourceDialog * setup_dialog, DownloadOptions * dl_options, DataSource * data_source);
	void progress_func(BabelProgressCode c, void * data, AcquireProcess * acquiring);
	void on_complete_process(AcquireGetterParams & getter_params);


	class Acquire {
	public:
		static void init(void);
		static void uninit(void);

		//static void acquire_from_source(Window * window, LayersPanel * panel, Viewport * viewport, DataSourceMode mode, DataSourceInterface * source_interface, WebToolDatasource * web_tool_data_source, DataSourceCleanupFunc cleanup_function);


		static QMenu * create_trwlayer_menu(Window * window, LayersPanel * panel, Viewport * viewport, LayerTRW * trw);
		static QMenu * create_trwlayer_track_menu(Window * window, LayersPanel * panel, Viewport * viewport, LayerTRW * trw);
		static QMenu * create_track_menu(Window * window, LayersPanel * panel, Viewport * viewport, Track * trk);

		static void set_filter_track(Track * trk);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_ACQUIRE_H_ */
