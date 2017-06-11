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
 *
 */

#ifndef _SG_ACQUIRE_H_
#define _SG_ACQUIRE_H_




#include <cstdint>

//#include <gtk/gtk.h>

#include "window.h"
#include "layers_panel.h"
#include "viewport.h"
#include "babel.h"




namespace SlavGPS {




	typedef struct _VikDataSourceInterface VikDataSourceInterface;

	typedef struct {
		Window * window;
		LayersPanel * panel;
		Viewport * viewport;
		void * userdata;
	} acq_vik_t;

	/**
	 * acq_dialog_widgets_t:
	 *
	 * global data structure used to expose the progress dialog to the worker thread.
	 */
	typedef struct {
		QLabel * status;
		Window * window;
		LayersPanel * panel;
		Viewport * viewport;
		GtkWidget * dialog;
		bool running;
		VikDataSourceInterface * source_interface;
		void * user_data;
	} acq_dialog_widgets_t;

	typedef enum {
		/* Generally Datasources shouldn't use these and let the HCI decide between the create or add to layer options. */
		VIK_DATASOURCE_CREATENEWLAYER,
		VIK_DATASOURCE_ADDTOLAYER,
		VIK_DATASOURCE_AUTO_LAYER_MANAGEMENT,
		VIK_DATASOURCE_MANUAL_LAYER_MANAGEMENT,
	} vik_datasource_mode_t;
	/* TODO: replace track/layer? */

	typedef enum {
		VIK_DATASOURCE_INPUTTYPE_NONE = 0,
		VIK_DATASOURCE_INPUTTYPE_TRWLAYER,
		VIK_DATASOURCE_INPUTTYPE_TRACK,
		VIK_DATASOURCE_INPUTTYPE_TRWLAYER_TRACK
	} vik_datasource_inputtype_t;

	/**
	 * VikDataSourceInitFunc:
	 *
	 * Returns: pointer to state if OK, otherwise %NULL.
	 */
	typedef void * (*VikDataSourceInitFunc) (acq_vik_t * avt);

	/**
	 * VikDataSourceCheckExistenceFunc:
	 *
	 * Returns: %NULL if OK, otherwise returns an error message.
	 */
	typedef char *(*VikDataSourceCheckExistenceFunc) ();

	/**
	 * VikDataSourceAddSetupWidgetsFunc:
	 *
	 * Create widgets to show in a setup dialog, set up state via user_data.
	 */
	typedef void (* VikDataSourceAddSetupWidgetsFunc) (GtkWidget * dialog, Viewport * viewport, void * user_data);

	/**
	 * VikDataSourceGetProcessOptionsFunc:
	 * @user_data: provided by #VikDataSourceInterface.init_func or dialog with params
	 * @download_options: optional options for downloads from URLs for #VikDataSourceInterface.process_func
	 * @input_file_name:
	 * @input_track_file_name:
	 *
	 * Set both to %NULL to signal refusal (ie already downloading).
	 @return process options: main options controlling the behaviour of #VikDataSourceInterface.process_func
	 */
	typedef ProcessOptions * (* VikDataSourceGetProcessOptionsFunc) (void * user_data, void * download_options, const char * input_file_name, const char * input_track_file_name);

	/**
	 * VikDataSourceProcessFunc:
	 * @vtl:
	 * @process_options: options to control the behaviour of this function (see #ProcessOptions)
	 * @status_cb: the #VikDataSourceInterface.progress_func
	 * @adw: the widgets and data used by #VikDataSourceInterface.progress_func
	 * @download_options: Optional options used if downloads from URLs is used.
	 *
	 * The actual function to do stuff - must report success/failure.
	 */
	typedef bool (* VikDataSourceProcessFunc) (void * trw, ProcessOptions * process_options, BabelStatusFunc, acq_dialog_widgets_t * adw, void * download_options);

	/* NB Same as BabelStatusFunc. */
	typedef void  (* VikDataSourceProgressFunc) (BabelProgressCode c, void * data, acq_dialog_widgets_t * w);

	/**
	 * VikDataSourceAddProgressWidgetsFunc:
	 *
	 * Creates widgets to show in a progress dialog, may set up state via user_data.
	 */
	typedef void  (*VikDataSourceAddProgressWidgetsFunc) ( GtkWidget *dialog, void * user_data );

	/**
	 * VikDataSourceCleanupFunc:
	 *
	 * Frees any widgets created for the setup or progress dialogs, any allocated state, etc.
	 */
	typedef void (* VikDataSourceCleanupFunc) (void * user_data);

	typedef void (* VikDataSourceOffFunc) (void * user_data, char ** babelargs, char ** file_descriptor);

	/**
	 * VikDataSourceInterface:
	 *
	 * Main interface.
	 */
	struct _VikDataSourceInterface {
		const char * window_title;
		const char * layer_title;
		vik_datasource_mode_t mode;
		vik_datasource_inputtype_t inputtype;
		bool autoview;
		bool keep_dialog_open; /* ... when done. */

		bool is_thread;

		/*** Manual UI Building. ***/
		VikDataSourceInitFunc init_func;
		VikDataSourceCheckExistenceFunc check_existence_func;
		VikDataSourceAddSetupWidgetsFunc add_setup_widgets_func;
		/***                    ***/

		VikDataSourceGetProcessOptionsFunc get_process_options;

		VikDataSourceProcessFunc process_func;

		VikDataSourceProgressFunc progress_func;
		VikDataSourceAddProgressWidgetsFunc add_progress_widgets_func;
		VikDataSourceCleanupFunc cleanup_func;
		VikDataSourceOffFunc off_func;

		/*** UI Building.        ***/
		Parameter *      params;
		uint16_t         params_count;
		ParameterValue * params_defaults;
		char **          params_groups;
		uint8_t          params_groups_count;

	};

	/**********************************/

	void a_acquire(Window * window,
		       LayersPanel * panel,
		       Viewport * viewport,
		       vik_datasource_mode_t mode,
		       VikDataSourceInterface *source_interface,
		       void * userdata,
		       VikDataSourceCleanupFunc cleanup_function);

	QMenu * a_acquire_trwlayer_menu(Window * window, LayersPanel * panel, Viewport * viewport, LayerTRW * trw);

	QMenu * a_acquire_trwlayer_track_menu(Window * window, LayersPanel * panel, Viewport * viewport, LayerTRW * trw);

	QMenu * a_acquire_track_menu(Window * window, LayersPanel * panel, Viewport * viewport, Track * trk);

	void a_acquire_set_filter_track(Track * trk);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_ACQUIRE_H_ */
