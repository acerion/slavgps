/*
 * SlavGPS -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Project started in 2016 by forking viking project.
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (c) 2011-2013, Rob Norris <rw_norris@hotmail.com>
 * Copyright (c) 2016-2018 Kamil Ignacak <acerion@wp.pl>
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




#include <vector>



#include <QColor>
#include <QPixmap>
#include <QPen>




#include "coord.h"
#include "layer_trw_definitions.h"
#include "mem_cache.h"




namespace SlavGPS {




	/* To be removed. */
	typedef int GdkFunction;




	class ScreenPos;
	class Track;
	class Trackpoint;
	class LayerTRW;
	class GisViewport;
	class Waypoint;
	class Window;




	class LayerTRWPainter {

	public:
		LayerTRWPainter(LayerTRW * trw);

		/* Call this every time the viewport changes (e.g. on zoom). */
		void set_viewport(GisViewport * gisview);

		void make_track_pens(void);
		void make_wp_pens(void);

		void draw_waypoint(Waypoint * wp, GisViewport * gisview, bool do_highlight);
		void draw_track(Track * trk, GisViewport * gisview, bool do_highlight);

	private:
		QColor get_fg_color(const Track * trk) const;
		QColor get_bg_color(bool do_highlight) const;

		inline bool coord_fits_in_viewport(const Coord & coord) const;

		/* Generate new cache object for given image path, but don't add the object to cache. */
		CachedPixmap generate_wp_cached_pixmap(const QString & image_full_path) const;

		void draw_waypoint_sub(Waypoint * wp, bool do_hightlight);
		void draw_waypoint_symbol(Waypoint * wp, const ScreenPos & wp_pos, bool do_highlight);
		bool draw_waypoint_image(Waypoint * wp, const ScreenPos & wp_pos, bool do_highlight);
		void draw_waypoint_label(Waypoint * wp, const ScreenPos & wp_pos, bool do_highlight);

		void draw_track_fg_sub(Track * trk, bool do_highlight);
		void draw_track_bg_sub(Track * trk, bool do_highlight);
		void draw_track_label(const QString & text, const QColor & fg_color, const QColor & bg_color, const Coord & coord);
		void draw_track_dist_labels(Track * trk, bool do_highlight);
		void draw_track_point_names(Track * trk, bool do_highlight);
		void draw_track_name_labels(Track * trk, bool do_highlight);
		void draw_track_draw_something(const ScreenPos & begin, const ScreenPos & end, QPen & pen, Trackpoint * tp, Trackpoint * tp_next, const Altitude & min_alt, double alt_diff);
		void draw_track_draw_midarrow(const ScreenPos & begin, const ScreenPos & end, QPen & pen);

		QPen get_track_fg_pen(Track * trk, bool do_highlight);

		GisViewport * gisview = NULL;
		LayerTRW * trw = NULL;
		Window * window = NULL;

		/* Properties of viewport (copied from viewport). */
		QRect vp_rect;
		double vp_xmpp = 0.0;
		double vp_ympp = 0.0;
		Coord vp_center;
		CoordMode vp_coord_mode;    /* UTM or Lat/Lon. */
		bool vp_is_one_utm_zone = false;  /* GisViewport shows only one UTM zone. */

		ArrowSymbol track_arrow;

		/* Variables used to decide whether an element that should be drawn is inside of visible area. */
		double coord_leftmost = 0.0;   /* Longitude or easting of leftmost point visible. */
		double coord_rightmost = 0.0;  /* Longitude or easting of rightmost point visible. */
		double coord_bottommost = 0.0; /* Latitude or northing of lowest point visible. */
		double coord_topmost = 0.0;    /* Latitude or northing of highest point visible. */



		std::vector<QPen> track_pens;


	public:

		double track_draw_speed_factor;
		QColor track_color_common; /* Used when layer's properties indicate that all tracks are drawn with the same color. */
		QPen current_track_pen;
		/* Separate pen for a track's potential new point as drawn via separate method
		   (compared to the actual track points drawn in the main trw_layer_draw_track function). */
		QPen current_track_new_point_pen;

		QPen track_bg_pen;
		QColor track_bg_color;

		QPen wp_marker_pen;
		QColor wp_marker_color;

		QPen wp_label_fg_pen;
		QColor wp_label_fg_color;

		QPen wp_label_bg_pen;
		QColor wp_label_bg_color;

		bool draw_track_labels;
		font_size_t track_label_font_size; /* Font size of track's label, in Pango's "absolute size" units. */

		LayerTRWTrackDrawingMode track_drawing_mode; /* Mostly about how a color(s) for track is/are selected, but in future perhaps other attributes will be variable as well. */

	        bool draw_trackpoints;
		int trackpoint_size;

		bool draw_track_lines;

		bool draw_track_stops;
		int track_min_stop_length; /* Keeping this as int, not as time_t, because it's used in parameters table with type SGVariantType::Int. */

		int track_thickness;
		int track_bg_thickness; /* Thickness of a line drawn in background of main line representing track. */

		bool draw_track_elevation;
		int track_elevation_factor;

		bool draw_track_directions;
		int draw_track_directions_size;

		bool draw_wp_symbols; /* Draw Garmin symbols of waypoints. */

		GraphicMarker wp_marker_type;
		int wp_marker_size; /* In Variant data type this field is stored as uint8_t. */

		bool draw_wp_labels;
		font_size_t wp_label_font_size; /* Font size of waypoint's label, in Pango's "absolute size" units. */

		bool draw_wp_images;
		int wp_image_alpha;
		int wp_image_size;

		bool /* GdkFunction */ wpbgand;

	}; /* class LayerTRWPainter */




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_TRW_PAINTER_H_ */
