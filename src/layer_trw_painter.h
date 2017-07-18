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
 */

#ifndef _SG_LAYER_TRW_PAINTER_H_
#define _SG_LAYER_TRW_PAINTER_H_




#include <cstdint>
#include <unordered_map>

#define VIK_TRW_LAYER_TRACK_GC 6
#define TRW_LAYER_TRACK_COLORS_MAX 10
#define VIK_TRW_LAYER_TRACK_GC_BLACK 0
#define VIK_TRW_LAYER_TRACK_GC_SLOW 1
#define VIK_TRW_LAYER_TRACK_GC_AVER 2
#define VIK_TRW_LAYER_TRACK_GC_FAST 3
#define VIK_TRW_LAYER_TRACK_GC_STOP 4
#define VIK_TRW_LAYER_TRACK_GC_SINGLE 5

#define DRAWMODE_BY_TRACK 0
#define DRAWMODE_BY_SPEED 1
#define DRAWMODE_ALL_SAME_COLOR 2
/* Note using DRAWMODE_BY_SPEED may be slow especially for vast numbers of trackpoints
   as we are (re)calculating the colour for every point. */




#define DRAW_ELEVATION_FACTOR 30 /* Height of elevation plotting, sort of relative to zoom level ("mpp" that isn't mpp necessarily). */
                                 /* This is multiplied by user-inputted value from 1-100. */




enum {
	WP_SYMBOL_FILLED_SQUARE,
	WP_SYMBOL_SQUARE,
	WP_SYMBOL_CIRCLE,
	WP_SYMBOL_X,
	WP_NUM_SYMBOLS
};




/* A cached waypoint image. */
/* This data structure probably should be put somewhere else. */
typedef struct {
	QPixmap * pixmap;
	char * image; /* Filename. */
} CachedPixmap;

/* These two functions probably should be put somewhere else. */
void cached_pixmap_free(CachedPixmap * cp);
int cached_pixmap_cmp(CachedPixmap * cp, const char * name);




namespace SlavGPS {




	class Track;
	class Trackpoint;
	class LayerTRW;
	class Viewport;
	class Waypoint;
	class Window;




	class TRWPainter {
	public:
		TRWPainter(LayerTRW * trw, Viewport * viewport);

		void set_highlight(bool highlight);

		void draw_waypoint_cb(Waypoint * wp);
		void draw_waypoints_cb(std::unordered_map<sg_uid_t, Waypoint *> * waypoints);
		void draw_track_cb(const void * id, Track * trk);
		void draw_tracks_cb(std::unordered_map<sg_uid_t, Track *> & tracks);

		void draw_track_label(const QString & text, const QColor & fg_color, const QColor & bg_color, const Coord * coord);
		void draw_dist_labels(Track * trk, bool drawing_highlight);
		void draw_point_names(Track * trk, bool drawing_highlight);
		void draw_track_name_labels(Track * trk, bool drawing_highlight);
		void draw_track(Track * trk, bool draw_track_outline);
		void draw_track_draw_something(int x, int y, int oldx, int oldy, QPen & main_pen, Trackpoint * tp, Trackpoint * tp_next, double min_alt, double alt_diff);
		void draw_track_draw_midarrow(int x, int y, int oldx, int oldy, QPen & main_pen);
		void draw_waypoint(Waypoint * wp);
		int draw_image(Waypoint * wp, int x, int y);
		void draw_symbol(Waypoint * wp, int x, int y);
		void draw_label(Waypoint * wp, int x, int y);


		Viewport * viewport = NULL;
		LayerTRW * trw = NULL;
		Window * window = NULL;

		double xmpp = 0;
		double ympp = 0;
		uint16_t width = 0;
		uint16_t height = 0;
		double cc = 0;      /* Cosine factor in track directions. */
		double ss = 0;      /* Sine factor in track directions. */
		const Coord * center = NULL;
		CoordMode coord_mode; /* UTM or Lat/Lon. */
		bool one_utm_zone = false;       /* Viewport shows only one UTM zone. */

		double ce1 = 0;
		double ce2 = 0;
		double cn1 = 0;
		double cn2 = 0;

		LatLonBBox bbox;
		bool highlight = false;

	private:
		QColor get_fg_color(const Track * trk) const;
		QColor get_bg_color(bool drawing_highlight) const;



	}; /* class TRWPainter */




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_TRW_PAINTER_H_ */