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

GType vik_gps_layer_get_type ();

typedef enum {
  GPS_DOWN=0,
  GPS_UP
} vik_gps_dir;

typedef enum {
  WPT=0,
  TRK=1,
  RTE=2
} vik_gps_xfer_type;

typedef struct _VikGpsLayer VikGpsLayer;

bool vik_gps_layer_is_empty ( VikGpsLayer *vgl );
const GList *vik_gps_layer_get_children ( VikGpsLayer *vgl );
VikTrwLayer * vik_gps_layer_get_a_child(VikGpsLayer *vgl);

// Non layer specific but expose communal method
int vik_gps_comm ( VikTrwLayer *vtl,
                    Track * trk,
                    vik_gps_dir dir,
                    char *protocol,
                    char *port,
                    bool tracking,
                    VikViewport *vvp,
                    VikLayersPanel *vlp,
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
		LayerGPS(VikLayer * vl) : Layer(vl) { };


		/* Layer interface methods. */
		void draw(Viewport * viewport);
		char const * tooltip();
		void marshall(uint8_t ** data, int * len);
	};





}





#endif
