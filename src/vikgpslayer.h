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
 *
 */

#ifndef _SG_LAYER_GPS_H_
#define _SG_LAYER_GPS_H_




#include <list>
#include <cstdint>

#include "viklayer.h"
#include "viktrack.h"
#include "viktrwlayer.h"
#include "vikwindow.h"




namespace SlavGPS {




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
			 char * protocol,
			 char * port,
			 bool tracking,
			 Viewport * viewport,
			 LayersPanel * panel,
			 bool do_tracks,
			 bool do_routes,
			 bool do_waypoints,
			 bool turn_off);




	class LayerGPS;




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




	class LayerGPS : public Layer {
	public:
		LayerGPS();
		LayerGPS(Viewport * viewport);
		~LayerGPS();

		/* Layer interface methods. */
		void draw(Viewport * viewport);
		char const * tooltip();
		void marshall(uint8_t ** data, int * len);
		void change_coord_mode(VikCoordMode mode);
		void add_menu_items(GtkMenu * menu, void * panel);
		void realize(TreeView * tree_view, GtkTreeIter * layer_iter);
		bool set_param(uint16_t id, VikLayerParamData data, Viewport * viewport, bool is_file_operation);
		VikLayerParamData get_param(uint16_t id, bool is_file_operation) const;


		void disconnect_layer_signal(VikLayer * vl);
		std::list<Layer const * > * get_children();
		LayerTRW * get_a_child();
		bool is_empty();
		void realtime_tracking_draw(Viewport * viewport);
		Trackpoint * create_realtime_trackpoint(bool forced);
		void update_statusbar(Window * window);
		bool rt_ask_retry();



#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
		bool rt_gpsd_connect(bool ask_if_failed);
		void rt_gpsd_disconnect();
#endif




		LayerTRW * trw_children[NUM_TRW];
		int cur_read_child;   /* Used only for reading file. */

#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
		VglGpsd * vgpsd;
		bool realtime_tracking;  /* Set/reset only by the callback. */
		bool first_realtime_trackpoint;
		GpsFix realtime_fix;
		GpsFix last_fix;

		Track * realtime_track;

		GIOChannel * realtime_io_channel;
		unsigned int realtime_io_watch_id;
		unsigned int realtime_retry_timer;
		GdkGC * realtime_track_gc;
		GdkGC * realtime_track_bg_gc;
		GdkGC * realtime_track_pt_gc;
		GdkGC * realtime_track_pt1_gc;
		GdkGC * realtime_track_pt2_gc;

		/* Params. */
		char * gpsd_host;
		char * gpsd_port;
		int gpsd_retry_interval;
		bool realtime_record;
		bool realtime_jump_to_start;
		unsigned int vehicle_position;
		bool realtime_update_statusbar;
		Trackpoint * tp;
		Trackpoint * tp_prev;
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */
		char * protocol;
		char * serial_port;
		bool download_tracks;
		bool download_routes;
		bool download_waypoints;
		bool upload_tracks;
		bool upload_routes;
		bool upload_waypoints;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_GPS_H_ */
