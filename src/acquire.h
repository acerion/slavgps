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

#include "babel.h"
#include "ui_builder.h"
#include "datasource.h"




namespace SlavGPS {




	class LayersPanel;
	class Viewport;
	class Window;
	class LayerTRW;
	class Track;




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
		void acquire(DataSourceMode mode, DataSourceInterface * source_interface, void * userdata, DataSourceCleanupFunc cleanup_function);
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
		void * user_data = NULL;

	public slots:
		void acquire_trwlayer_cb(void);
	};




	/**
	   Returns: pointer to state if OK, otherwise %NULL.
	*/
	typedef void * (* DataSourceInitFunc)(acq_vik_t * avt);

	/**
	   Returns: %NULL if OK, otherwise returns an error message.
	*/
	typedef char * (* DataSourceCheckExistenceFunc)();

	/**
	   Create a dialog for configuring/setting up access to data source
	*/
	typedef DataSourceDialog * (* DataSourceCreateSetupDialogFunc)(Viewport * viewport, void * user_data);

	/**
	   @user_data: provided by #DataSourceInterface.init_func or dialog with params
	   @download_options: optional options for downloads from URLs for #DataSourceInterface.process_func
	   @input_file_name:
	   @input_track_file_name:

	   Set both to %NULL to signal refusal (ie already downloading).
	   @return process options: main options controlling the behaviour of #DataSourceInterface.process_func
	*/
	typedef ProcessOptions * (* DataSourceGetProcessOptionsFunc)(void * user_data, void * download_options, const char * input_file_name, const char * input_track_file_name);

	/**
	  @trw:
	  @process_options: options to control the behaviour of this function (see #ProcessOptions)
	  @status_cb: the #DataSourceInterface.progress_func
	  @acquiring: the widgets and data used by #DataSourceInterface.progress_func
	  @download_options: Optional options used if downloads from URLs is used.

	  The actual function to do stuff - must report success/failure.
	*/
	typedef bool (* DataSourceProcessFunc)(void * trw, ProcessOptions * process_options, BabelCallback cb, AcquireProcess * acquiring, void * download_options);

	/* Same as BabelCallback. */
	typedef void  (* DataSourceProgressFunc)(BabelProgressCode c, void * data, AcquireProcess * acquiring);

	/**
	   Create a dialog for showing progress of accessing a data source
	*/
	typedef DataSourceDialog * (* DataSourceCreateProgressDialogFunc)(void * user_data);



	typedef void (* DataSourceTurnOffFunc) (void * user_data, QString & babel_args, QString & file_path);

	struct _DataSourceInterface {
		QString window_title;
		QString layer_title;
		DataSourceMode mode;
		DatasourceInputtype inputtype;
		bool autoview;
		bool keep_dialog_open; /* ... when done. */

		bool is_thread;

		DataSourceInitFunc init_func;
		DataSourceCheckExistenceFunc check_existence_func;
		DataSourceCreateSetupDialogFunc create_setup_dialog_func;
		DataSourceGetProcessOptionsFunc get_process_options;
		DataSourceProcessFunc process_func;
		DataSourceProgressFunc progress_func;
		DataSourceCreateProgressDialogFunc create_progress_dialog_func;
		DataSourceCleanupFunc cleanup_func;
		DataSourceTurnOffFunc off_func;

		ParameterSpecification * param_specs;
		uint16_t                 param_specs_count;
		SGVariant * params_defaults;
		char ** params_groups;
		uint8_t params_groups_count;
	};




	class Acquire {
	public:
		static void init(void);
		static void uninit(void);

		static void acquire_from_source(Window * window, LayersPanel * panel, Viewport * viewport, DataSourceMode mode, DataSourceInterface * source_interface, void * userdata, DataSourceCleanupFunc cleanup_function);


		static QMenu * create_trwlayer_menu(Window * window, LayersPanel * panel, Viewport * viewport, LayerTRW * trw);
		static QMenu * create_trwlayer_track_menu(Window * window, LayersPanel * panel, Viewport * viewport, LayerTRW * trw);
		static QMenu * create_track_menu(Window * window, LayersPanel * panel, Viewport * viewport, Track * trk);

		static void set_filter_track(Track * trk);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_ACQUIRE_H_ */
