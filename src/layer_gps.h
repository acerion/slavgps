/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2006-2008, Quy Tonthat <qtonthat@gmail.com>
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

#ifndef _SG_LAYER_GPS_H_
#define _SG_LAYER_GPS_H_




#include <list>
#include <mutex>




#include <QRunnable>
#include <QLabel>




#include "layer.h"
#include "layer_interface.h"
#include "variant.h"
#include "babel.h"
#include "dialog.h"





#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
#define REALTIME_GPS_TRACKING_ENABLED 1
#else
#define REALTIME_GPS_TRACKING_ENABLED 0
#endif

#ifdef REALTIME_GPS_TRACKING_ENABLED
#undef REALTIME_GPS_TRACKING_ENABLED
#endif
#define REALTIME_GPS_TRACKING_ENABLED 1


#ifdef VIK_CONFIG_REALTIME_GPS_TRACKING
#include <gps.h>
#endif




namespace SlavGPS {




	class Viewport;
	class LayerGPS;
	class LayerTRW;
	class Window;
	class Track;
	class Trackpoint;




	enum class GPSDirection {
		DOWN = 0,
		UP
	};

	enum class GPSTransferType {
		WPT = 0,
		TRK = 1,
		RTE = 2
	};




	/* Non layer specific but exposes common method. */
	int vik_gps_comm(LayerTRW * trw_layer,
			 Track * trk,
			 GPSDirection dir,
			 const QString & protocol,
			 const QString & port,
			 bool tracking,
			 Viewport * viewport,
			 LayersPanel * panel,
			 bool do_tracks,
			 bool do_routes,
			 bool do_waypoints,
			 bool turn_off);




#if REALTIME_GPS_TRACKING_ENABLED
	typedef struct {
		struct gps_data_t gpsd;
		LayerGPS * gps_layer;
	} VglGpsd;

	typedef struct {
		struct gps_fix_t fix;
		int satellites_used;
		bool dirty;   /* Needs to be saved. */
	} GPSFix;
#endif /* REALTIME_GPS_TRACKING_ENABLED */




	enum {
		GPS_CHILD_LAYER_TRW_DOWNLOAD = 0,
		GPS_CHILD_LAYER_TRW_UPLOAD,
#if REALTIME_GPS_TRACKING_ENABLED
		GPS_CHILD_LAYER_TRW_REALTIME,
#endif
		GPS_CHILD_LAYER_MAX
	};




	class LayerGPSInterface : public LayerInterface {
	public:
		LayerGPSInterface();
		Layer * unmarshall(uint8_t * data, size_t data_len, Viewport * viewport);
	};




	class LayerGPS : public Layer {
		Q_OBJECT
	public:
		LayerGPS();
		~LayerGPS();

		/* Layer interface methods. */
		void draw(Viewport * viewport);
		QString get_tooltip();
		void marshall(uint8_t ** data, size_t * data_len);
		void change_coord_mode(CoordMode mode);
		void add_menu_items(QMenu & menu);
		void add_children_to_tree(void);
		bool set_param_value(uint16_t id, const SGVariant & param_value, bool is_file_operation);
		SGVariant get_param_value(param_id_t id, bool is_file_operation) const;


		std::list<Layer const * > * get_children();
		LayerTRW * get_a_child();
		bool is_empty();
		void realtime_tracking_draw(Viewport * viewport);
		Trackpoint * create_realtime_trackpoint(bool forced);
		void update_statusbar(Window * window);
		bool rt_ask_retry();


		void set_coord_mode(CoordMode mode);


		LayerTRW * trw_children[GPS_CHILD_LAYER_MAX] = { 0 };
		int cur_read_child = 0;   /* Used only for reading file. */

#if REALTIME_GPS_TRACKING_ENABLED
		bool rt_gpsd_connect(bool ask_if_failed);
		void rt_gpsd_disconnect();

		VglGpsd * vgpsd = NULL;
		bool realtime_tracking_in_progress;  /* Set/reset only by the callback. */
		bool first_realtime_trackpoint = false;
		GPSFix realtime_fix;
		GPSFix last_fix;

		Track * realtime_track = NULL;

		GIOChannel * realtime_io_channel = NULL;
		unsigned int realtime_io_watch_id = 0;
		unsigned int realtime_retry_timer = 0;

		QPen realtime_track_pen;
		QPen realtime_track_bg_pen;
		QPen realtime_track_pt_pen;
		QPen realtime_track_pt1_pen;
		QPen realtime_track_pt2_pen;

		/* Params. */
		QString gpsd_host;
		QString gpsd_port;
		int gpsd_retry_interval;
		bool realtime_record;
		bool realtime_jump_to_start;
		int32_t vehicle_position; /* Signed int because this is a generic enum ID. */
		bool realtime_update_statusbar;
		Trackpoint * tp = NULL;
		Trackpoint * tp_prev = NULL;
#endif /* REALTIME_GPS_TRACKING_ENABLED */

		QString protocol;
		QString serial_port;
		bool download_tracks;
		bool download_routes;
		bool download_waypoints;
		bool upload_tracks;
		bool upload_routes;
		bool upload_waypoints;

	public slots:
		void gps_upload_cb(void);
		void gps_download_cb(void);
		void gps_empty_download_cb(void);
		void gps_empty_all_cb(void);
		void gps_empty_upload_cb(void);

#if REALTIME_GPS_TRACKING_ENABLED
		void gps_start_stop_tracking_cb(void);
		void gps_empty_realtime_cb(void);
#endif
	};




	void layer_gps_init(void);




	class GPSSession : public QRunnable, public BabelSomething {
	public:
		GPSSession(GPSDirection dir, LayerTRW * trw, Track * track, const QString & port, Viewport * viewport, bool in_progress);

		void set_current_count(int cnt);
		void set_total_count(int cnt);
		void set_gps_device_info(const QString & info);
		void process_line_for_gps_info(const char * line);

		void import_progress_cb(BabelProgressCode code, void * data);
		void export_progress_cb(BabelProgressCode code, void * data);

		void run();

		std::mutex mutex;
		GPSDirection direction;
		QString port;
		bool in_progress = false;
		int total_count = 0;
		int count = 0;
		LayerTRW * trw = NULL;
		Track * trk = NULL;
		QString babel_args;
		QString window_title;
		BasicDialog * dialog = NULL;
		QLabel * status_label = NULL;
		QLabel * gps_device_label = NULL;
		QLabel * ver_label = NULL;
		QLabel * id_label = NULL;
		QLabel * wp_label = NULL;
		QLabel * trk_label = NULL;
		QLabel * rte_label = NULL;
		QLabel * progress_label = NULL;
		GPSTransferType progress_type = GPSTransferType::WPT;
		Viewport * viewport = NULL;
#if REALTIME_GPS_TRACKING_ENABLED
		bool realtime_tracking_in_progress = false;
#endif
};









} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_GPS_H_ */
