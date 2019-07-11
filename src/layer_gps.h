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
#include <QTimer>




#include <glib.h>

#ifdef VIK_CONFIG_REALTIME_GPS_TRACKING
#include <gps.h>

#if GPSD_API_MAJOR_VERSION == 5 || GPSD_API_MAJOR_VERSION == 6
#else
#error "Unsupported major version of GPS API"
#endif

#endif




#include "layer.h"
#include "layer_interface.h"
#include "variant.h"
#include "dialog.h"
#include "acquire.h"





#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
#define REALTIME_GPS_TRACKING_ENABLED 1
#define SG_GPSD_PORT 2947
#else
#define REALTIME_GPS_TRACKING_ENABLED 0
#endif




namespace SlavGPS {




	class GisViewport;
	class LayerGPS;
	class LayerTRW;
	class Window;
	class Track;
	class Trackpoint;




	enum class GPSDirection {
		Download = 0,
		Upload
	};

	enum class GPSTransferType {
		WPT = 0,
		TRK = 1,
		RTE = 2
	};




	class GPSTransfer {
	public:
		GPSTransfer(GPSDirection dir) : direction(dir) {};

		int run_transfer(LayerTRW * trw_layer, Track * trk, GisViewport * gisview, bool tracking);

		GPSDirection direction;    /* The direction of the transfer. */

		bool do_tracks = false;    /* Whether tracks shoud be processed. */
		bool do_routes = false;    /* Whether routes shoud be processed. */
		bool do_waypoints = false; /* Whether waypoints shoud be processed. */

		bool turn_off = false; /* Whether we should attempt to turn off the GPS device after the transfer (only some devices support this). Most of the time will be false. */

		QString gps_protocol;  /* The GPS device communication protocol. */
		QString serial_port;   /* The GPS serial port. */
	};






#if REALTIME_GPS_TRACKING_ENABLED
	class RTData {
	public:
		sg_ret set(struct gps_data_t & gpsdata, CoordMode coord_mode);
		void reset(void);

		struct gps_fix_t fix;
		int satellites_used;

		/* Whether this current data has been used to create a
		   trackpoint and that trackpoint has been pushed to a
		   real-time tracking track. */
		bool saved_to_track;

		LatLon lat_lon;
		Coord coord; /* Always in coordinate system of Real-time tracking TRW layer. */
	};




	enum class VehiclePosition {
		Centered = 0,
		OnScreen,
		None
	};
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
		Layer * unmarshall(Pickle & pickle, GisViewport * gisview);
	};




	class LayerGPS : public Layer {
		Q_OBJECT
	public:
		LayerGPS();
		~LayerGPS();


		/* Module initialization. */
		static void init(void);


		/* Layer interface methods. */
		void draw_tree_item(GisViewport * gisview, bool highlight_selected, bool parent_is_selected);
		QString get_tooltip(void) const;
		void marshall(Pickle & pickle);
		void change_coord_mode(CoordMode mode);
		void add_menu_items(QMenu & menu);
		sg_ret attach_children_to_tree(void);
		bool set_param_value(param_id_t param_id, const SGVariant & param_value, bool is_file_operation);
		SGVariant get_param_value(param_id_t param_id, bool is_file_operation) const override;


		int get_child_layers_count(void) const;
		std::list<Layer const * > get_child_layers(void) const;
		LayerTRW * get_a_child(void);

		void set_coord_mode(CoordMode mode);

		GPSTransfer download{GPSDirection::Download};
		GPSTransfer upload{GPSDirection::Upload};

		LayerTRW * trw_children[GPS_CHILD_LAYER_MAX] = { 0 };
		int cur_read_child = 0;   /* Used only for reading file. */

#if REALTIME_GPS_TRACKING_ENABLED
		void rt_gpsd_disconnect(void);
		void rt_gpsd_raw_hook(void);

		struct gps_data_t gpsdata;
		bool gpsdata_opened = false;
#endif

	public slots:
		void gps_upload_cb(void);
		void gps_download_cb(void);
		void gps_empty_download_cb(void);
		void gps_empty_all_cb(void);
		void gps_empty_upload_cb(void);

		/* GPS Layer can contain other layers and should be notified about changes in them. */
		void child_tree_item_changed_cb(const QString & child_tree_item_name);

#if REALTIME_GPS_TRACKING_ENABLED
		void rt_start_stop_tracking_cb(void);
		void rt_empty_realtime_cb(void);

		/**
		   @return true if connection attempt succeeded
		   @return false if connection attempt failed
		*/
		bool rt_gpsd_connect_periodic_retry_cb(void);
#endif

	private:

#if REALTIME_GPS_TRACKING_ENABLED
		void rt_tracking_draw(GisViewport * gisview, const RTData & rt_data);
		Trackpoint * rt_create_trackpoint(bool record_every_tp);
		void rt_update_statusbar(Window * window);

		bool rt_ask_retry(void);

		/* Top-level function for deinitializing and stopping
		   everything related to tracking. */
		void rt_stop_tracking(void);

		/* Clean up layer's child items (tracks, waypoints)
		   - remove them if necessary. */
		void rt_cleanup_layer_children(void);

		/* Close connection to gpsd. */
		void rt_gpsd_disconnect_inner(void);

		bool rt_gpsd_connect_try_once(void);


		Track * realtime_track = NULL;

		GIOChannel * realtime_io_channel = NULL;
		unsigned int realtime_io_watch_id = 0;
		QTimer realtime_retry_timer;

		QPen realtime_track_pen;
		QPen realtime_track_bg_pen;
		QPen realtime_track_pt_pen;
		QPen realtime_track_pt1_pen;
		QPen realtime_track_pt2_pen;

		/* Params. */
		QString gpsd_host;
		int gpsd_port = SG_GPSD_PORT;
		int gpsd_retry_interval = 10; /* The same value as in gpsd_retry_interval_scale. */
		bool realtime_record = false;
		bool realtime_jump_to_start = false;
		VehiclePosition vehicle_position = VehiclePosition::OnScreen; /* Default value is the same as in vehicle_position_enum. */
		bool realtime_update_statusbar = false;
		QString statusbar_format_code;
		Trackpoint * tp = NULL;
		Trackpoint * tp_prev = NULL;

		bool realtime_tracking_in_progress = false;  /* Set/reset only by the callback. */
		bool first_realtime_trackpoint = false;
		RTData current_rt_data;
		RTData previous_rt_data;
#endif
	};




	class GPSSession : public QRunnable, public AcquireTool {
	public:
		GPSSession(GPSTransfer & new_transfer, LayerTRW * trw, Track * track, GisViewport * gisview, bool in_progress);

		void set_current_count(int cnt);
		void set_total_count(int cnt);
		void set_gps_device_info(const QString & info);
		void process_line_for_gps_info(const char * line);

		void import_progress_cb(AcquireProgressCode code, void * data);
		void export_progress_cb(AcquireProgressCode code, void * data);

		void run();

		std::mutex mutex;

		GPSTransfer transfer{GPSDirection::Upload};

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
		GisViewport * gisview = NULL;
#if REALTIME_GPS_TRACKING_ENABLED
		bool realtime_tracking_in_progress = false;
#endif
	};




	bool get_unit_gps_info(char * info, size_t info_size, const char * str);
	bool get_prddat_gps_info(char * info, size_t info_size, const char * str);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_GPS_H_ */
