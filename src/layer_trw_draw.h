/*
 * SlavGPS -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Project started in 2016 by forking viking project.
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (c) 2011-2013, Rob Norris <rw_norris@hotmail.com>
 * Copyright (c) 2016 Kamil Ignacak <acerion@wp.pl>
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

#ifndef _SG_LAYER_TRW_DRAW_H
#define _SG_LAYER_TRW_DRAW_H





#include <stdint.h>

#include <unordered_map>

#include "vikviewport.h"
#include "vikwaypoint.h"
#include "viktrack.h"
#include "viktrwlayer.h"
#include "vikwindow.h"





#define VIK_TRW_LAYER_TRACK_GC 6
#define VIK_TRW_LAYER_TRACK_GCS 10
#define VIK_TRW_LAYER_TRACK_GC_BLACK 0
#define VIK_TRW_LAYER_TRACK_GC_SLOW 1
#define VIK_TRW_LAYER_TRACK_GC_AVER 2
#define VIK_TRW_LAYER_TRACK_GC_FAST 3
#define VIK_TRW_LAYER_TRACK_GC_STOP 4
#define VIK_TRW_LAYER_TRACK_GC_SINGLE 5

#define DRAWMODE_BY_TRACK 0
#define DRAWMODE_BY_SPEED 1
#define DRAWMODE_ALL_SAME_COLOR 2
// Note using DRAWMODE_BY_SPEED may be slow especially for vast numbers of trackpoints
//  as we are (re)calculating the colour for every point



#define DRAW_ELEVATION_FACTOR 30 /* height of elevation plotting, sort of relative to zoom level ("mpp" that isn't mpp necessarily) */
                                 /* this is multiplied by user-inputted value from 1-100. */


enum { WP_SYMBOL_FILLED_SQUARE, WP_SYMBOL_SQUARE, WP_SYMBOL_CIRCLE, WP_SYMBOL_X, WP_NUM_SYMBOLS };





typedef struct {
	SlavGPS::Viewport * viewport;
	SlavGPS::LayerTRW * trw;
	SlavGPS::Window * window;
	double xmpp, ympp;
	uint16_t width, height;
	double cc; // Cosine factor in track directions
	double ss; // Sine factor in track directions
	const VikCoord * center;
	VikCoordMode coord_mode; /* UTM or Lat/Lon. */
	bool one_utm_zone;       /* Viewport shows only one UTM zone. */
	double ce1, ce2, cn1, cn2;
	LatLonBBox bbox;
	bool highlight;
} DrawingParams;





void init_drawing_params(DrawingParams * dp, SlavGPS::LayerTRW * trw, SlavGPS::Viewport * viewport, bool highlight);
void trw_layer_draw_waypoints_cb(std::unordered_map<sg_uid_t, SlavGPS::Waypoint *> * waypoints, DrawingParams * dp);
void trw_layer_draw_waypoint_cb(SlavGPS::Waypoint * wp, DrawingParams * dp);
void trw_layer_draw_track_cb(const void * id, SlavGPS::Track * trk, DrawingParams * dp);
void trw_layer_draw_track_cb(std::unordered_map<sg_uid_t, SlavGPS::Track *> & tracks, DrawingParams * dp);





/* A cached waypoint image. */
/* This data structure probably should be put somewhere else. */
typedef struct {
	GdkPixbuf * pixbuf;
	char * image; /* filename */
} CachedPixbuf;

/* These two functions probably should be put somewhere else. */
void cached_pixbuf_free(CachedPixbuf * cp);
int cached_pixbuf_cmp(CachedPixbuf * cp, const char * name);





#endif /* #ifndef _SG_LAYER_TRW_DRAW_H */
