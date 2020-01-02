/*
 * SlavGPS -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_LAYER_TRW_IMPORT_H_
#define _SG_LAYER_TRW_IMPORT_H_




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
		AcquireContext(Window * window, GisViewport * gisview, Layer * parent_layer, LayerTRW * trw, Track * trk)
			: m_window(window), m_gisview(gisview), m_parent_layer(parent_layer), m_trw(trw), m_trk(trk) {};

		void print_debug(const char * function, int line) const;

		AcquireContext & operator=(const AcquireContext & rhs);

		Window * m_window = nullptr;
		GisViewport * m_gisview = nullptr;
		Layer * m_parent_layer = nullptr; /* Parent layer of TRW layer. It may be Aggregate layer or GPS layer. */
		LayerTRW * m_trw = nullptr;
		Track * m_trk = nullptr;

		/* Whether a target trw layer has been freshly
		   created, or it already existed in tree view. */
		bool m_trw_allocated = false;
	};





	/* Remember that by default QRunnable is auto-deleted on thread exit. */
	class AcquireWorker : public QObject, public QRunnable {
		Q_OBJECT
	public:
		AcquireWorker(DataSource * data_source, const AcquireContext & acquire_context);
		~AcquireWorker();
		void run(); /* Re-implementation of QRunnable::run(). */
		sg_ret configure_target_layer(DataSourceMode mode);

		void finalize_after_success(void);
		void finalize_after_failure(void);

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


		static sg_ret acquire_from_source(DataSource * data_source, DataSourceMode mode, AcquireContext & acquire_context);
		static void set_context(Window * window, GisViewport * gisview, Layer * parent_layer, LayerTRW * existing_trw_layer);
		static void set_target(LayerTRW * trw, Track * trk);
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




	class LayerTRWImporter : public QObject {
		Q_OBJECT
	public:
		/* For importing into new TRW layer. The new TRW layer
		   will be created under given @param parent_layer. */
		LayerTRWImporter(Window * window, GisViewport * gisview, Layer * parent_layer);

		/* For importing into existing TRW layer. Parent layer
		   of the existing TRW layer is specified with @param
		   parent_layer. */
		LayerTRWImporter(Window * window, GisViewport * gisview, Layer * parent_layer, LayerTRW * trw);

		sg_ret import_into_new_layer(DataSource * data_source, Layer * parent_layer);
		sg_ret import_into_existing_layer(DataSource * data_source);

		sg_ret add_import_into_new_layer_submenu(QMenu & submenu);
		sg_ret add_import_into_existing_layer_submenu(QMenu & submenu);

		AcquireContext ctx;

	public slots:
		void import_into_new_layer_from_gps_cb(void);
		void import_into_new_layer_from_file_cb(void);
		void import_into_new_layer_from_geojson_cb(void);
		void import_into_new_layer_from_routing_cb(void);
		void import_into_new_layer_from_osm_cb(void);
		void import_into_new_layer_from_my_osm_cb(void);
#ifdef VIK_CONFIG_GEOCACHES
		void import_into_new_layer_from_gc_cb(void);
#endif
#ifdef VIK_CONFIG_GEOTAG
		void import_into_new_layer_from_geotag_cb(void);
#endif
#ifdef VIK_CONFIG_GEONAMES
		void import_into_new_layer_from_wikipedia_cb(void);
#endif
		void import_into_new_layer_from_url_cb(void);



		void import_into_existing_layer_from_gps_cb(void);
		void import_into_existing_layer_from_routing_cb(void);
		void import_into_existing_layer_from_osm_cb(void);
		void import_into_existing_layer_from_osm_my_traces_cb(void);
		void import_into_existing_layer_from_url_cb(void);

		void import_into_existing_layer_from_wikipedia_waypoints_viewport_cb(void);
		void import_into_existing_layer_from_wikipedia_waypoints_layer_cb(void);

#ifdef VIK_CONFIG_GEOCACHES
		void import_into_existing_layer_from_geocache_cb(void);
#endif
#ifdef VIK_CONFIG_GEOTAG
		void import_into_existing_layer_from_geotagged_images_cb(void);
#endif
		void import_into_existing_layer_from_file_cb();
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_TRW_IMPORT_H_ */
