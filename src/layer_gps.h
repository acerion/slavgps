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




#include <glib.h>

#ifdef VIK_CONFIG_REALTIME_GPS_TRACKING
#include <gps.h>
#endif




#include "layer.h"
#include "layer_interface.h"
#include "variant.h"
#include "dialog.h"
#include "acquire.h"





#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
#define REALTIME_GPS_TRACKING_ENABLED 1
#else
#define REALTIME_GPS_TRACKING_ENABLED 0
#endif




namespace SlavGPS {




	class Viewport;
	class LayerGPS;
	class LayerTRW;
	class Window;
	class Track;
	class Trackpoint;




	enum class GPSDirection {
		Down = 0,
		Up
	};

	enum class GPSTransferType {
		WPT = 0,
		TRK = 1,
		RTE = 2
	};




	class GPSTransfer {
	public:
		GPSTransfer(GPSDirection dir) : direction(dir) {};

		/* Non layer specific but exposes common method. */
		int run_transfer(LayerTRW * trw_layer, Track * trk, Viewport * viewport, LayersPanel * panel, bool tracking);

		GPSDirection direction;    /* The direction of the transfer. */

		bool do_tracks = false;    /* Whether tracks shoud be processed. */
		bool do_routes = false;    /* Whether routes shoud be processed. */
		bool do_waypoints = false; /* Whether waypoints shoud be processed. */

		bool turn_off = false; /* Whether we should attempt to turn off the GPS device after the transfer (only some devices support this). Most of the time will be false. */

		QString gps_protocol;  /* The GPS device communication protocol. */
		QString serial_port;   /* The GPS serial port. */
	};






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
		Layer * unmarshall(Pickle & pickle, Viewport * viewport);
	};




	class LayerGPS : public Layer {
		Q_OBJECT
	public:
		LayerGPS();
		~LayerGPS();

		/* Module initialization. */
		static void init(void);


		/* Layer interface methods. */
		void draw_tree_item(Viewport * viewport, bool highlight_selected, bool parent_is_selected);
		QString get_tooltip(void) const;
		void marshall(Pickle & pickle);
		void change_coord_mode(CoordMode mode);
		void add_menu_items(QMenu & menu);
		sg_ret attach_children_to_tree(void);
		bool set_param_value(param_id_t param_id, const SGVariant & param_value, bool is_file_operation);
		SGVariant get_param_value(param_id_t param_id, bool is_file_operation) const;


		int get_child_layers_count(void) const;
		std::list<Layer const * > get_child_layers(void) const;
		LayerTRW * get_a_child();

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

		GPSTransfer download{GPSDirection::Down};
		GPSTransfer upload{GPSDirection::Up};

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

		/* GPS Layer can contain other layers and should be notified about changes in them. */
		void child_tree_item_changed_cb(const QString & child_tree_item_name);
	};




	class GPSSession : public QRunnable, public AcquireTool {
	public:
		GPSSession(GPSTransfer & new_transfer, LayerTRW * trw, Track * track, Viewport * viewport, bool in_progress);

		void set_current_count(int cnt);
		void set_total_count(int cnt);
		void set_gps_device_info(const QString & info);
		void process_line_for_gps_info(const char * line);

		void import_progress_cb(AcquireProgressCode code, void * data);
		void export_progress_cb(AcquireProgressCode code, void * data);

		void run();

		std::mutex mutex;

		GPSTransfer transfer{GPSDirection::Up};

		QString babel_opts;

		bool in_progress = false;
		int total_count = 0;
		int count = 0;
		LayerTRW * trw = NULL;
		Track * trk = NULL;
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
