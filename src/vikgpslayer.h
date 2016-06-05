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

#ifndef _VIKING_GPSLAYER_H
#define _VIKING_GPSLAYER_H

#include <stdbool.h>
#include <stdint.h>

#include "viklayer.h"
#include "viktrack.h"
#include "viktrwlayer.h"

#ifdef __cplusplus
extern "C" {
#endif


#define VIK_GPS_LAYER_TYPE            (vik_gps_layer_get_type ())
#define VIK_GPS_LAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_GPS_LAYER_TYPE, VikGpsLayer))
#define VIK_GPS_LAYER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_GPS_LAYER_TYPE, VikGpsLayerClass))
#define IS_VIK_GPS_LAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_GPS_LAYER_TYPE))
#define IS_VIK_GPS_LAYER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_GPS_LAYER_TYPE))

typedef struct _VikGpsLayerClass VikGpsLayerClass;
struct _VikGpsLayerClass
{
	VikLayerClass vik_layer_class;
};

struct _VikGpsLayer {
	VikLayer vl;
};

typedef struct _VikGpsLayer VikGpsLayer;


GType vik_gps_layer_get_type();

enum {
	TRW_DOWNLOAD = 0,
	TRW_UPLOAD,
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
	TRW_REALTIME,
#endif
	NUM_TRW
};


typedef enum {
	GPS_DOWN = 0,
	GPS_UP
} vik_gps_dir;

typedef enum {
	WPT = 0,
	TRK = 1,
	RTE = 2
} vik_gps_xfer_type;

#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
typedef struct {
	struct gps_data_t gpsd;
	VikGpsLayer *vgl;
} VglGpsd;

typedef struct {
	struct gps_fix_t fix;
	int satellites_used;
	bool dirty;   /* needs to be saved */
} GpsFix;
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */



// Non layer specific but expose communal method
int vik_gps_comm(LayerTRW * layer,
		 Track * trk,
		 vik_gps_dir dir,
		 char *protocol,
		 char *port,
		 bool tracking,
		 Viewport * viewport,
		 LayersPanel * panel,
		 bool do_tracks,
		 bool do_routes,
		 bool do_waypoints,
		 bool turn_off);

#ifdef __cplusplus
}
#endif





namespace SlavGPS {





	class LayerGPS : public Layer {
	public:
		LayerGPS();
		LayerGPS(VikLayer * vl);
		LayerGPS(Viewport * viewport);

		/* Layer interface methods. */
		void draw(Viewport * viewport);
		char const * tooltip();
		void marshall(uint8_t ** data, int * len);
		void change_coord_mode(VikCoordMode mode);
		void add_menu_items(GtkMenu * menu, void * panel);
		void realize(VikTreeview *vt, GtkTreeIter *layer_iter);
		void free_();


		void disconnect_layer_signal(VikLayer * vl);
		const GList * get_children();
		LayerTRW * get_a_child();
		bool is_empty();
		void realtime_tracking_draw(Viewport * viewport);
		Trackpoint * create_realtime_trackpoint(bool forced);
		void update_statusbar(VikWindow * vw);
		bool rt_ask_retry();



#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
		void rt_gpsd_disconnect();
		bool rt_gpsd_connect(bool ask_if_failed);
#endif




		LayerTRW * trw_children[NUM_TRW];
		GList * children;	/* used only for writing file */
		int cur_read_child;   /* used only for reading file */

#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
		VglGpsd * vgpsd;
		bool realtime_tracking;  /* set/reset only by the callback */
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

		/* params */
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





}





#endif
