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

#include "layer.h"
#include "layer_interface.h"
#include "variant.h"




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




#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
	typedef struct {
		struct gps_data_t gpsd;
		LayerGPS * gps_layer;
	} VglGpsd;

	typedef struct {
		struct gps_fix_t fix;
		int satellites_used;
		bool dirty;   /* Needs to be saved. */
	} GpsFix;
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */




	enum {
		TRW_DOWNLOAD = 0,
		TRW_UPLOAD,
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
		TRW_REALTIME,
#endif
		NUM_TRW
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


		LayerTRW * trw_children[NUM_TRW] = { 0 };
		int cur_read_child = 0;   /* Used only for reading file. */

#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
		bool rt_gpsd_connect(bool ask_if_failed);
		void rt_gpsd_disconnect();

		VglGpsd * vgpsd = NULL;
		bool realtime_tracking;  /* Set/reset only by the callback. */
		bool first_realtime_trackpoint = false;
		GpsFix realtime_fix;
		GpsFix last_fix;

		Track * realtime_track = NULL;

		GIOChannel * realtime_io_channel = NULL;
		unsigned int realtime_io_watch_id = 0;
		unsigned int realtime_retry_timer = 0;

#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
		QPen realtime_track_pen;
		QPen realtime_track_bg_pen;
		QPen realtime_track_pt_pen;
		QPen realtime_track_pt1_pen;
		QPen realtime_track_pt2_pen;
#endif

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
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */

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

#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
		void gps_start_stop_tracking_cb(void);
		void gps_empty_realtime_cb(void);
#endif
	};




	void layer_gps_init(void);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_GPS_H_ */
