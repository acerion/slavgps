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




#include "layer_trw_definitions.h"
#include "layer_trw_waypoints.h"
#include "layer_trw_tracks.h"




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

		void draw_waypoint(Waypoint * wp, bool do_highlight);
		void draw_track(Track * trk, bool do_highlight);

	private:
		QColor get_fg_color(const Track * trk) const;
		QColor get_bg_color(bool do_highlight) const;

		QPixmap * update_pixmap_cache(const QString & image_full_path, Waypoint & wp);

		void draw_waypoint_sub(Waypoint * wp, bool do_hightlight);
		void draw_waypoint_symbol(Waypoint * wp, const ScreenPos & pos);
		bool draw_waypoint_image(Waypoint * wp, const ScreenPos & pos, bool do_highlight);
		void draw_waypoint_label(Waypoint * wp, const ScreenPos & pos, bool do_highlight);

		void draw_track_fg_sub(Track * trk, bool do_highlight);
		void draw_track_bg_sub(Track * trk, bool do_highlight);
		void draw_track_label(const QString & text, const QColor & fg_color, const QColor & bg_color, const Coord & coord);
		void draw_track_dist_labels(Track * trk, bool do_highlight);
		void draw_track_point_names(Track * trk, bool do_highlight);
		void draw_track_name_labels(Track * trk, bool do_highlight);
		void draw_track_draw_something(const ScreenPos & begin, const ScreenPos & end, QPen & pen, Trackpoint * tp, Trackpoint * tp_next, double min_alt, double alt_diff);
		void draw_track_draw_midarrow(const ScreenPos & begin, const ScreenPos & end, QPen & pen);

		Viewport * viewport = NULL;
		LayerTRW * trw = NULL;
		Window * window = NULL;

		/* Properties of viewport (copied from viewport). */
		int vp_width = 0;
		int vp_height = 0;
		double vp_xmpp = 0.0;
		double vp_ympp = 0.0;
		Coord vp_center;
		CoordMode vp_coord_mode;    /* UTM or Lat/Lon. */
		bool vp_is_one_utm_zone = false;  /* Viewport shows only one UTM zone. */
		LatLonBBox vp_bbox;

		double cosine_factor = 0.0;    /* Cosine factor in track directions. */
		double sine_factor = 0.0;      /* Sine factor in track directions. */


		/* Variables used to decide whether an element that should be drawn is inside of visible area. */
		double coord_leftmost = 0.0;   /* Longitude or easting of leftmost point visible. */
		double coord_rightmost = 0.0;  /* Longitude or easting of rightmost point visible. */
		double coord_bottommost = 0.0; /* Latitude or northing of lowest point visible. */
		double coord_topmost = 0.0;    /* Latitude or northing of highest point visible. */

	}; /* class TRWPainter */




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_TRW_PAINTER_H_ */
