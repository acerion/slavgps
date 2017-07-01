/*
 * SlavGPS -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Project started in 2016 by forking viking project.
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2008, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2009, Hein Ragas <viking@ragas.nl>
 * Copyright (c) 2012-2015, Rob Norris <rw_norris@hotmail.com>
 * Copyright (c) 2012-2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cassert>
#include <cstdio>

#ifdef HAVE_MATH_H
#include <cmath>
#endif
#ifdef HAVE_STRING_H
#include <cstring>
#endif
#ifdef HAVE_STDLIB_H
#include <cstdlib>
#endif

#include <glib.h>

//#include "thumbnails.h"
#include "viewport_internal.h"
#include "ui_util.h"
#include "layer_trw_draw.h"
#include "settings.h"
#include "globals.h"
#include "vikutils.h"




using namespace SlavGPS;




static void trw_layer_draw_symbol(Waypoint * wp, int x, int y, DrawingParams * dp);
static void trw_layer_draw_label(Waypoint * wp, int x, int y, DrawingParams * dp);
static void trw_layer_draw_label(Waypoint * wp, int x, int y, DrawingParams * dp);
static int  trw_layer_draw_image(Waypoint * wp, int x, int y, DrawingParams * dp);




void init_drawing_params(DrawingParams * dp, LayerTRW * trw, Viewport * viewport, bool highlight)
{
	dp->trw = trw;
	dp->viewport = viewport;
	dp->highlight = highlight;
	dp->window = dp->trw->get_window();
	dp->xmpp = viewport->get_xmpp();
	dp->ympp = viewport->get_ympp();
	dp->width = viewport->get_width();
	dp->height = viewport->get_height();
	dp->cc = trw->drawdirections_size * cos(DEG2RAD(45)); /* Calculate once per trw update - even if not used. */
	dp->ss = trw->drawdirections_size * sin(DEG2RAD(45)); /* Calculate once per trw update - even if not used. */

	dp->center = viewport->get_center();
	dp->coord_mode = viewport->get_coord_mode();
	dp->one_utm_zone = viewport->is_one_zone(); /* False if some other projection besides UTM. */

	if (dp->coord_mode == CoordMode::UTM && dp->one_utm_zone) {
		int w2 = dp->xmpp * (dp->width / 2) + 1600 / dp->xmpp;
		int h2 = dp->ympp * (dp->height / 2) + 1600 / dp->ympp;
		/* Leniency -- for tracks. Obviously for waypoints this SHOULD be a lot smaller. */

		dp->ce1 = dp->center->utm.easting - w2;
		dp->ce2 = dp->center->utm.easting + w2;
		dp->cn1 = dp->center->utm.northing - h2;
		dp->cn2 = dp->center->utm.northing + h2;

	} else if (dp->coord_mode == CoordMode::LATLON) {

		/* Quick & dirty calculation; really want to check all corners due to lat/lon smaller at top in northern hemisphere. */
		/* This also DOESN'T WORK if you are crossing 180/-180 lon. I don't plan to in the near future... */
		Coord upperleft = viewport->screen_to_coord(-500, -500);
		Coord bottomright = viewport->screen_to_coord(dp->width + 500, dp->height + 500);
		dp->ce1 = upperleft.ll.lon;
		dp->ce2 = bottomright.ll.lon;
		dp->cn1 = bottomright.ll.lat;
		dp->cn2 = upperleft.ll.lat;
	} else {
		;
	}

	viewport->get_bbox(&dp->bbox);
}




/*
 * Determine the colour of the trackpoint (and/or trackline) relative to the average speed.
 * Here a simple traffic like light colour system is used:
 *  . slow points are red
 *  . average is yellow
 *  . fast points are green
 */
static int track_section_colour_by_speed(Trackpoint * tp1, Trackpoint * tp2, double average_speed, double low_speed, double high_speed)
{
	if (tp1->has_timestamp && tp2->has_timestamp) {
		if (average_speed > 0) {
			double rv = (Coord::distance(tp1->coord, tp2->coord) / (tp1->timestamp - tp2->timestamp));
			if (rv < low_speed) {
				return VIK_TRW_LAYER_TRACK_GC_SLOW;
			} else if (rv > high_speed) {
				return VIK_TRW_LAYER_TRACK_GC_FAST;
			} else {
				return VIK_TRW_LAYER_TRACK_GC_AVER;
			}
		}
	}
	return VIK_TRW_LAYER_TRACK_GC_BLACK;
}




static void draw_utm_skip_insignia(Viewport * viewport, QPen & pen, int x, int y)
{
	/* First draw '+'. */
	viewport->draw_line(pen, x+5, y,   x-5, y );
	viewport->draw_line(pen, x,   y+5, x,   y-5);

	/* And now draw 'x' on top of it. */
	viewport->draw_line(pen, x+5, y+5, x-5, y-5);
	viewport->draw_line(pen, x+5, y-5, x-5, y+5);
}




static void trw_layer_draw_track_label(char * name, char * fgcolour, char * bgcolour, DrawingParams * dp, Coord * coord)
{
	char *label_markup = g_strdup_printf("<span foreground=\"%s\" background=\"%s\" size=\"%s\">%s</span>", fgcolour, bgcolour, dp->trw->track_fsize_str, name);
#ifdef K
	if (pango_parse_markup(label_markup, -1, 0, NULL, NULL, NULL, NULL)) {
		pango_layout_set_markup(dp->trw->tracklabellayout, label_markup, -1);
	} else {
		/* Fallback if parse failure. */
		pango_layout_set_text(dp->trw->tracklabellayout, name, -1);
	}

	free(label_markup);

	int label_x, label_y;
	int width, height;
	pango_layout_get_pixel_size(dp->trw->tracklabellayout, &width, &height);

	dp->viewport->coord_to_screen(coord, &label_x, &label_y);
	dp->viewport->draw_layout(dp->trw->track_bg_gc, label_x-width/2, label_y-height/2, dp->trw->tracklabellayout);
#endif
}




/**
 * Draw a few labels along a track at nicely seperated distances.
 * This might slow things down if there's many tracks being displayed with this on.
 */
static void trw_layer_draw_dist_labels(DrawingParams * dp, Track * trk, bool drawing_highlight)
{
#ifdef K
	static const double chunksd[] = {0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 15.0, 20.0,
					 25.0, 40.0, 50.0, 75.0, 100.0,
					 150.0, 200.0, 250.0, 500.0, 1000.0};

	double dist = trk->get_length_including_gaps() / (trk->max_number_dist_labels + 1);
	DistanceUnit distance_unit = Preferences::get_unit_distance();

	/* Convert to specified unit to find the friendly breakdown value. */
	dist = convert_distance_meters_to(distance_unit, dist);

	int index = 0;
	for (int i = 0; i < G_N_ELEMENTS(chunksd); i++) {
		if (chunksd[i] > dist) {
			index = i;
			dist = chunksd[index];
			break;
		}
	}


	for (int i = 1; i < trk->max_number_dist_labels+1; i++) {
		double dist_i = dist * i;

		/* Convert distance back into metres for use in finding a trackpoint. */
		switch (distance_unit) {
		case DistanceUnit::MILES:
			dist_i = VIK_MILES_TO_METERS(dist_i);
			break;
		case DistanceUnit::NAUTICAL_MILES:
			dist_i = VIK_NAUTICAL_MILES_TO_METERS(dist_i);
			break;
		default:
			/* DistanceUnit::KILOMETRES. */
			dist_i = dist_i*1000.0;
			break;
		}

		double dist_current = 0.0;
		Trackpoint * tp_current = trk->get_tp_by_dist(dist_i, false, &dist_current);
		double dist_next = 0.0;
		Trackpoint * tp_next = trk->get_tp_by_dist(dist_i, true, &dist_next);

		double dist_between_tps = fabs(dist_next - dist_current);
		double ratio = 0.0;
		/* Prevent division by 0 errors. */
		if (dist_between_tps > 0.0) {
			ratio = fabs(dist_i-dist_current) / dist_between_tps;
		}

		if (tp_current && tp_next) {
			/* Construct the name based on the distance value. */


			char *name;
			char unit_string[16];
			get_distance_unit_string(unit_string, sizeof (unit_string), distance_unit);

			/* Convert for display. */
			dist_i = convert_distance_meters_to(distance_unit, dist_i);

			/* Make the precision of the output related to the unit size. */
			if (index == 0) {
				name = g_strdup_printf("%.2f %s", dist_i, unit_string);
			} else if (index == 1) {
				name = g_strdup_printf("%.1f %s", dist_i, unit_string);
			} else {
				name = g_strdup_printf("%d %s", (int) round(dist_i), unit_string); /* TODO single vs plurals. */
			}


			struct LatLon ll_current = tp_current->coord.get_latlon();
			struct LatLon ll_next = tp_next->coord.get_latlon();

			/* Positional interpolation.
			   Using a simple ratio - may not be perfectly correct due to lat/long projections
			   but should be good enough over the small scale that I anticipate usage on. */
			struct LatLon ll_new = { ll_current.lat + (ll_next.lat-ll_current.lat)*ratio,
						 ll_current.lon + (ll_next.lon-ll_current.lon)*ratio };
			Coord coord(ll_new, dp->trw->coord_mode);

			char *fgcolour;
			if (dp->trw->drawmode == DRAWMODE_BY_TRACK) {
				fgcolour = gdk_color_to_string(&(trk->color));
			} else {
				fgcolour = gdk_color_to_string(&(dp->trw->track_color));
			}

			/* If highlight mode on, then colour the background in the highlight colour. */
			char *bgcolour;
			if (drawing_highlight) {
				bgcolour = g_strdup(dp->viewport->get_highlight_color());
			} else {
				bgcolour = gdk_color_to_string(&(dp->trw->track_bg_color));
			}

			trw_layer_draw_track_label(name, fgcolour, bgcolour, dp, &coord);

			free(fgcolour);
			free(bgcolour);
			free(name);
		}
	}
#endif
}




/**
 * Draw a label (or labels) for the track name somewhere depending on the track's properties.
 */
static void trw_layer_draw_track_name_labels(DrawingParams * dp, Track * trk, bool drawing_highlight)
{
#ifdef K
	char *fgcolour;
	if (dp->trw->drawmode == DRAWMODE_BY_TRACK) {
		fgcolour = gdk_color_to_string(&(trk->color));
	} else {
		fgcolour = gdk_color_to_string(&(dp->trw->track_color));
	}

	/* If highlight mode on, then colour the background in the highlight colour. */
	char *bgcolour;
	if (drawing_highlight) {
		bgcolour = g_strdup(dp->viewport->get_highlight_color());
	} else {
		bgcolour = gdk_color_to_string(&(dp->trw->track_bg_color));
	}

	char *ename = g_markup_escape_text(trk->name, -1);

	if (trk->draw_name_mode == TRACK_DRAWNAME_START_END_CENTRE ||
	    trk->draw_name_mode == TRACK_DRAWNAME_CENTRE) {
		struct LatLon average, maxmin[2] = { {0,0}, {0,0} };
		LayerTRW::find_maxmin_in_track(trk, maxmin);
		average.lat = (maxmin[0].lat+maxmin[1].lat)/2;
		average.lon = (maxmin[0].lon+maxmin[1].lon)/2;
		Coord coord(average, dp->trw->coord_mode);

		trw_layer_draw_track_label(ename, fgcolour, bgcolour, dp, &coord);
	}

	if (trk->draw_name_mode == TRACK_DRAWNAME_CENTRE) {
		/* No other labels to draw. */
		return;
	}

	Trackpoint * tp_end = trk->get_tp_last();
	if (!tp_end) {
		return;
	}
	Trackpoint * tp_begin = trk->get_tp_first();
	if (!tp_begin) {
		return;
	}
	Coord begin_coord = tp_begin->coord;
	Coord end_coord = tp_end->coord;

	bool done_start_end = false;

	if (trk->draw_name_mode == TRACK_DRAWNAME_START_END ||
	    trk->draw_name_mode == TRACK_DRAWNAME_START_END_CENTRE) {

		/* This number can be configured via the settings if you really want to change it. */
		double distance_diff;
		if (! a_settings_get_double("trackwaypoint_start_end_distance_diff", &distance_diff)) {
			distance_diff = 100.0; // Metres
		}

		if (Coord::distance(begin_coord, end_coord) < distance_diff) {
			/* Start and end 'close' together so only draw one label at an average location. */
			int x1, x2, y1, y2;
			dp->viewport->coord_to_screen(&begin_coord, &x1, &y1);
			dp->viewport->coord_to_screen(&end_coord, &x2, &y2);
			Coord av_coord = dp->viewport->screen_to_coord((x1 + x2) / 2, (y1 + y2) / 2);

			char *name = g_strdup_printf("%s: %s", ename, _("start/end"));
			trw_layer_draw_track_label(name, fgcolour, bgcolour, dp, &av_coord);
			free(name);

			done_start_end = true;
		}
	}

	if (! done_start_end) {
		if (trk->draw_name_mode == TRACK_DRAWNAME_START
		    || trk->draw_name_mode == TRACK_DRAWNAME_START_END
		    || trk->draw_name_mode == TRACK_DRAWNAME_START_END_CENTRE) {

			char *name_start = g_strdup_printf("%s: %s", ename, _("start"));
			trw_layer_draw_track_label(name_start, fgcolour, bgcolour, dp, &begin_coord);
			free(name_start);
		}
		/* Don't draw end label if this is the one being created. */
		if (trk != dp->trw->current_trk) {
			if (trk->draw_name_mode == TRACK_DRAWNAME_END
			    || trk->draw_name_mode == TRACK_DRAWNAME_START_END
			    || trk->draw_name_mode == TRACK_DRAWNAME_START_END_CENTRE) {

				char *name_end = g_strdup_printf("%s: %s", ename, _("end"));
				trw_layer_draw_track_label(name_end, fgcolour, bgcolour, dp, &end_coord);
				free(name_end);
			}
		}
	}

	free(fgcolour);
	free(bgcolour);
	free(ename);
#endif
}




/**
 * Draw a point labels along a track.
 * This might slow things down if there's many tracks being displayed with this on.
 */
static void trw_layer_draw_point_names(DrawingParams * dp, Track * trk, bool drawing_highlight)
{
	if (trk->empty()) {
		return;
	}
#ifdef K
	char *fgcolour;
	if (dp->trw->drawmode == DRAWMODE_BY_TRACK) {
		fgcolour = gdk_color_to_string(&(trk->color));
	} else {
		fgcolour = gdk_color_to_string(&(dp->trw->track_color));
	}
	char *bgcolour;
	if (drawing_highlight) {
		bgcolour = g_strdup(dp->viewport->get_highlight_color());
	} else {
		bgcolour = gdk_color_to_string(&(dp->trw->track_bg_color));
	}

	for (auto iter = trk->trackpointsB->begin(); iter != trk->trackpointsB->end(); iter++) {
		if ((*iter)->name) {
			trw_layer_draw_track_label((*iter)->name, fgcolour, bgcolour, dp, &(*iter)->coord);
		}
	}

	free(fgcolour);
	free(bgcolour);
#endif
}




void trw_layer_draw_track_draw_midarrow(DrawingParams * dp, int x, int y, int oldx, int oldy, QPen & main_pen)
{
	int midx = (oldx + x) / 2;
	int midy = (oldy + y) / 2;

	double len = sqrt(((midx - oldx) * (midx - oldx)) + ((midy - oldy) * (midy - oldy)));
	/* Avoid divide by zero and ensure at least 1 pixel big. */
	if (len > 1) {
		double dx = (oldx - midx) / len;
		double dy = (oldy - midy) / len;
		dp->viewport->draw_line(main_pen, midx, midy, midx + (dx * dp->cc + dy * dp->ss), midy + (dy * dp->cc - dx * dp->ss));
		dp->viewport->draw_line(main_pen, midx, midy, midx + (dx * dp->cc - dy * dp->ss), midy + (dy * dp->cc + dx * dp->ss));
	}
}




void trw_layer_draw_track_draw_something(DrawingParams * dp, int x, int y, int oldx, int oldy, QPen & main_pen, Trackpoint * tp, Trackpoint * tp_next, double min_alt, double alt_diff)
{
#define FIXALTITUDE(what) \
	((((Trackpoint *) (what))->altitude - min_alt) / alt_diff * DRAW_ELEVATION_FACTOR * dp->trw->elevation_factor / dp->xmpp)


	QPoint points[4];

	points[0] = QPoint(oldx, oldy);
	points[1] = QPoint(oldx, oldy - FIXALTITUDE (tp));
	points[2] = QPoint(x, y - FIXALTITUDE (tp_next));
	points[3] = QPoint(x, y);

	QPen tmp_pen;
#ifndef K
	tmp_pen.setColor("green");
	tmp_pen.setWidth(1);
#else
	if (((oldx - x) > 0 && (oldy - y) > 0) || ((oldx - x) < 0 && (oldy - y) < 0)) {
		tmp_pen = gtk_widget_get_style(dp->viewport)->light_gc[3];
	} else {
		tmp_pen = gtk_widget_get_style(dp->viewport)->dark_gc[0];
	}
#endif
	dp->viewport->draw_polygon(tmp_pen, points, 4, true);

	dp->viewport->draw_line(main_pen, oldx, oldy - FIXALTITUDE (tp), x, y - FIXALTITUDE (tp_next));
}




static void trw_layer_draw_track(Track * trk, DrawingParams * dp, bool draw_track_outline)
{
	if (!trk->visible) {
		return;
	}

	/* TODO: this function is a mess, get rid of any redundancy. */

	double min_alt, max_alt, alt_diff = 0;
	if (dp->trw->drawelevation) {

		/* Assume if it has elevation at the beginning, it has it throughout. not ness a true good assumption. */
		if (trk->get_minmax_alt(&min_alt, &max_alt)) {
			alt_diff = max_alt - min_alt;
		}
	}

	/* Admittedly this is not an efficient way to do it because we go through the whole GC thing all over... */
	if (dp->trw->bg_line_thickness && !draw_track_outline) {
		trw_layer_draw_track(trk, dp, true);
	}

	bool drawpoints;
	bool drawstops;
	if (draw_track_outline) {
		drawpoints = drawstops = false;
	} else {
		drawpoints = dp->trw->drawpoints;
		drawstops = dp->trw->drawstops;
	}
#if 1
	drawstops = true;
	dp->trw->stop_length = 1;
	dp->trw->drawmode = DRAWMODE_BY_SPEED;
#endif

	QPen main_pen = QPen(QColor("black"));
	main_pen.setWidth(1);

	bool drawing_highlight = false;
	/* Current track - used for creation. */
	if (trk == dp->trw->current_trk) {
		main_pen = dp->trw->current_trk_pen;
	} else {
		if (dp->highlight) {
			/* Draw all tracks of the layer in special colour.
			   NB this supercedes the drawmode. */
			main_pen = dp->viewport->get_highlight_pen();
			drawing_highlight = true;
		}

		if (!drawing_highlight) {
			/* Still need to figure out the pen according to the drawing mode: */
			switch (dp->trw->drawmode) {
			case DRAWMODE_BY_TRACK:
				dp->trw->track_1color_pen.setColor(trk->color);
				dp->trw->track_1color_pen.setWidth(dp->trw->line_thickness);
				main_pen = dp->trw->track_1color_pen;
				break;
			default:
				/* Mostly for DRAWMODE_ALL_SAME_COLOR
				   but includes DRAWMODE_BY_SPEED, main_pen is set later on as necessary. */
				main_pen = dp->trw->track_pens[VIK_TRW_LAYER_TRACK_GC_SINGLE];
				break;
			}
		}
	}


	if (trk->empty()) {
		return;
	}

	const uint8_t tp_size_reg = dp->trw->drawpoints_size;
	const uint8_t tp_size_cur = dp->trw->drawpoints_size * 2;

	auto iter = trk->trackpointsB->begin();

	uint8_t tp_size = (dp->trw->selected_tp.valid && *iter == *dp->trw->selected_tp.iter) ? tp_size_cur : tp_size_reg;

	int x, y;
	dp->viewport->coord_to_screen(&(*iter)->coord, &x, &y);

	/* Draw the first point as something a bit different from the normal points.
	   ATM it's slightly bigger and a triangle. */

	if (drawpoints) {
		QPoint trian[3] = { QPoint(x, y-(3*tp_size)), QPoint(x-(2*tp_size), y+(2*tp_size)), QPoint(x+(2*tp_size), y+(2*tp_size)) };
		dp->viewport->draw_polygon(main_pen, trian, 3, true);
	}

	double average_speed = 0.0;
	double low_speed = 0.0;
	double high_speed = 0.0;
	/* If necessary calculate these values - which is done only once per track redraw. */
	if (dp->trw->drawmode == DRAWMODE_BY_SPEED) {
		/* The percentage factor away from the average speed determines transistions between the levels. */
		average_speed = trk->get_average_speed_moving(dp->trw->stop_length);
		low_speed = average_speed - (average_speed*(dp->trw->track_draw_speed_factor/100.0));
		high_speed = average_speed + (average_speed*(dp->trw->track_draw_speed_factor/100.0));
	}

	int prev_x = x;
	int prev_y = y;
	bool use_prev_xy = true; /* prev_x/prev_y contain valid coordinates of previous point. */

	iter++; /* Because first Trackpoint has been drawn above. */

	for (; iter != trk->trackpointsB->end(); iter++) {
		Trackpoint * tp = *iter;

		tp_size = (dp->trw->selected_tp.valid && tp == *dp->trw->selected_tp.iter) ? tp_size_cur : tp_size_reg;

		Trackpoint * prev_tp = (Trackpoint *) *std::prev(iter);
		/* See if in a different lat/lon 'quadrant' so don't draw massively long lines (presumably wrong way around the Earth).
		   Mainly to prevent wrong lines drawn when a track crosses the 180 degrees East-West longitude boundary
		   (since Viewport::draw_line() only copes with pixel value and has no concept of the globe). */
		if (dp->coord_mode == CoordMode::LATLON
		    && ((prev_tp->coord.ll.lon < -90.0 && tp->coord.ll.lon > 90.0)
			|| (prev_tp->coord.ll.lon > 90.0 && tp->coord.ll.lon < -90.0))) {

			use_prev_xy = false;
			continue;
		}

		/* Check some stuff -- but only if we're in UTM and there's only ONE ZONE; or lat lon. */

		/* kamilTODO: compare this condition with condition in trw_layer_draw_waypoint(). */
		bool first_condition = (dp->coord_mode == CoordMode::UTM && !dp->one_utm_zone); /* UTM coord mode & more than one UTM zone - do everything. */
		bool second_condition_A = ((!dp->one_utm_zone) || tp->coord.utm.zone == dp->center->utm.zone);  /* Only check zones if UTM & one_utm_zone. */
		bool second_condition_B = (tp->coord.ll.lon < dp->ce2 && tp->coord.ll.lon > dp->ce1) || (tp->coord.utm.easting < dp->ce2 && tp->coord.utm.easting > dp->ce1);
		bool second_condition_C = (tp->coord.ll.lat > dp->cn1 && tp->coord.ll.lat < dp->cn2) || (tp->coord.utm.northing > dp->cn1 && tp->coord.utm.northing < dp->cn2);
		bool second_condition = (second_condition_A && second_condition_B && second_condition_C);

#if 0
		if ((!dp->one_utm_zone && !dp->lat_lon) /* UTM & zones; do everything. */
		    || (((!dp->one_utm_zone) || tp->coord.utm_zone == dp->center->utm_zone) /* Only check zones if UTM & one_utm_zone. */
			&& tp->coord.east_west < dp->ce2 && tp->coord.east_west > dp->ce1 /* Both UTM and lat lon. */
			&& tp->coord.north_south > dp->cn1 && tp->coord.north_south < dp->cn2))

#endif

		//fprintf(stderr, "%d || (%d && %d && %d)\n", first_condition, second_condition_A, second_condition_B, second_condition_C);

		if (first_condition || second_condition) {

			//fprintf(stderr, "first branch ----\n");

			dp->viewport->coord_to_screen(&(tp->coord), &x, &y);

			/* The concept of drawing stops is that if the next trackpoint has a
			   timestamp far into the future, we draw a circle of 6x trackpoint
			   size, instead of a rectangle of 2x trackpoint size. Stop is drawn
			   first so the trackpoint will be drawn on top. */
			if (drawstops
			    && drawpoints
			    && ! draw_track_outline
			    && std::next(iter) != trk->trackpointsB->end()
			    && (*std::next(iter))->timestamp - (*iter)->timestamp > dp->trw->stop_length) {

				dp->viewport->draw_arc(dp->trw->track_pens[VIK_TRW_LAYER_TRACK_GC_STOP], x-(3*tp_size), y-(3*tp_size), 6*tp_size, 6*tp_size, 0, 360, true);
			}

			if (use_prev_xy && x == prev_x && y == prev_y) {
				/* Points are the same in display coordinates, don't
				   draw, skip drawing part. Notice that we do
				   this after drawing stops. */
				goto skip;
			}

			if (drawpoints || dp->trw->drawlines) {
				/* Setup main_pen for both point and line drawing. */
				if (!drawing_highlight && (dp->trw->drawmode == DRAWMODE_BY_SPEED)) {
					main_pen = dp->trw->track_pens[track_section_colour_by_speed(tp, prev_tp, average_speed, low_speed, high_speed)];
				}
			}

			if (drawpoints && !draw_track_outline) {

				if (std::next(iter) != trk->trackpointsB->end()) {
					/* Regular point - draw 2x square. */
					dp->viewport->fill_rectangle(main_pen.color(), x-tp_size, y-tp_size, 2*tp_size, 2*tp_size);
				} else {
					/* Final point - draw 4x circle. */
					dp->viewport->draw_arc(main_pen, x-(2*tp_size), y-(2*tp_size), 4*tp_size, 4*tp_size, 0, 360, true);
				}
			}

			if ((!tp->newsegment) && (dp->trw->drawlines)) {

				/* UTM only: zone check. */
				if (drawpoints && dp->trw->coord_mode == CoordMode::UTM && tp->coord.utm.zone != dp->center->utm.zone) {
					draw_utm_skip_insignia(dp->viewport, main_pen, x, y);
				}

				if (!use_prev_xy) {
					dp->viewport->coord_to_screen(&(prev_tp->coord), &prev_x, &prev_y);
				}

				if (draw_track_outline) {
					dp->viewport->draw_line(dp->trw->track_bg_pen, prev_x, prev_y, x, y);
				} else {
					dp->viewport->draw_line(main_pen, prev_x, prev_y, x, y);

					if (dp->trw->drawelevation
					    && std::next(iter) != trk->trackpointsB->end()
					    && (*std::next(iter))->altitude != VIK_DEFAULT_ALTITUDE) {

						trw_layer_draw_track_draw_something(dp, x, y, prev_x, prev_y, main_pen, *iter, *std::next(iter), min_alt, alt_diff);
					}
				}
			}

			if ((!tp->newsegment) && dp->trw->drawdirections) {
				/* Draw an arrow at the mid point to show the direction of the track.
				   Code is a rework from vikwindow::draw_ruler(). */
				trw_layer_draw_track_draw_midarrow(dp, x, y, prev_x, prev_y, main_pen);
			}

		skip:
			prev_x = x;
			prev_y = y;
			use_prev_xy = true;
		} else {

			if (use_prev_xy && dp->trw->drawlines && (!tp->newsegment)) {
				if (dp->trw->coord_mode != CoordMode::UTM || tp->coord.utm.zone == dp->center->utm.zone) {
					dp->viewport->coord_to_screen(&(tp->coord), &x, &y);

					if (!drawing_highlight && (dp->trw->drawmode == DRAWMODE_BY_SPEED)) {
						main_pen = dp->trw->track_pens[track_section_colour_by_speed(tp, prev_tp, average_speed, low_speed, high_speed)];
					}

					/* Draw only if current point has different coordinates than the previous one. */
					if (x != prev_x || y != prev_y) {
						if (draw_track_outline) {
							dp->viewport->draw_line(dp->trw->track_bg_pen, prev_x, prev_y, x, y);
						} else {
							dp->viewport->draw_line(main_pen, prev_x, prev_y, x, y);
						}
					}
				} else {
					/* Draw only if current point has different coordinates than the previous one. */
					if (x != prev_x && y != prev_y) { /* kamilFIXME: is && a correct condition? */
						dp->viewport->coord_to_screen(&(prev_tp->coord), &x, &y);
						draw_utm_skip_insignia(dp->viewport, main_pen, x, y);
					}
				}
			}
			use_prev_xy = false;
		}
	}

	/* Labels drawn after the trackpoints, so the labels are on top. */
	if (dp->trw->track_draw_labels) {
		if (trk->max_number_dist_labels > 0) {
			trw_layer_draw_dist_labels(dp, trk, drawing_highlight);
		}
		trw_layer_draw_point_names(dp, trk, drawing_highlight);

		if (trk->draw_name_mode != TrackDrawNameMode::NONE) {
			trw_layer_draw_track_name_labels(dp, trk, drawing_highlight);
		}
	}
}




void trw_layer_draw_track_cb(const void * id, Track * trk, DrawingParams * dp)
{
	if (BBOX_INTERSECT (trk->bbox, dp->bbox)) {
		trw_layer_draw_track(trk, dp, false);
	}
}




void trw_layer_draw_track_cb(Tracks & tracks, DrawingParams * dp)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		if (BBOX_INTERSECT (i->second->bbox, dp->bbox)) {
			trw_layer_draw_track(i->second, dp, false);
		}
	}
}




static void trw_layer_draw_waypoint(Waypoint * wp, DrawingParams * dp)
{
	if (!wp->visible) {
		return;
	}

	bool cond = (dp->coord_mode == CoordMode::UTM && !dp->one_utm_zone)
		|| ((dp->coord_mode == CoordMode::LATLON || wp->coord.utm.zone == dp->center->utm.zone) &&
		    (wp->coord.ll.lon < dp->ce2 && wp->coord.ll.lon > dp->ce1 && wp->coord.ll.lat > dp->cn1 && wp->coord.ll.lat < dp->cn2)
		    || (wp->coord.utm.easting < dp->ce2 && wp->coord.utm.easting > dp->ce1 && wp->coord.utm.northing > dp->cn1 && wp->coord.utm.northing < dp->cn2));


	if (!cond) {
		return;
	}

	int x, y;
	dp->viewport->coord_to_screen(&(wp->coord), &x, &y);

	/* If in shrunken_cache, get that. If not, get and add to shrunken_cache. */
	if (wp->image && dp->trw->drawimages)	{
		if (0 == trw_layer_draw_image(wp, x, y, dp)) {
			return;
		}
	}

	/* Draw appropriate symbol - either symbol image or simple types. */
	trw_layer_draw_symbol(wp, x, y, dp);

	if (dp->trw->drawlabels) {
		trw_layer_draw_label(wp, x, y, dp);
	}
}




int trw_layer_draw_image(Waypoint * wp, int x, int y, DrawingParams * dp)
{
	if (dp->trw->image_alpha == 0) {
		return 0;
	}

#ifdef K

	QPixmap * pixmap = NULL;
	GList * l = g_list_find_custom(dp->trw->image_cache->head, wp->image, (GCompareFunc) cached_pixmap_cmp);
	if (l) {
		pixmap = ((CachedPixmap *) l->data)->pixmap;
	} else {
		char * image = wp->image;
		QPixmap * regularthumb = a_thumbnails_get(wp->image);
		if (!regularthumb) {
			regularthumb = a_thumbnails_get_default(); /* cache one 'not yet loaded' for all thumbs not loaded */
			image = (char *) "\x12\x00"; /* this shouldn't occur naturally. */
		}
		if (regularthumb) {
			CachedPixmap * cp = (CachedPixmap *) malloc(sizeof (CachedPixmap));
			if (dp->trw->image_size == 128) {
				cp->pixmap = regularthumb;
			} else {
				cp->pixmap = a_thumbnails_scale_pixmap(regularthumb, dp->trw->image_size, dp->trw->image_size);
				assert (cp->pixmap);
				g_object_unref(G_OBJECT(regularthumb));
			}
			cp->image = g_strdup(image);

			/* Apply alpha setting to the image before the pixmap gets stored in the cache. */
			if (dp->trw->image_alpha != 255) {
				cp->pixmap = ui_pixmap_set_alpha(cp->pixmap, dp->trw->image_alpha);
			}

			/* Needed so 'click picture' tool knows how big the pic is; we don't
			   store it in cp because they may have been freed already. */
			wp->image_width = cp->pixmap->width();
			wp->image_height = cp->pixmap->heigth();

			g_queue_push_head(dp->trw->image_cache, cp);
			if (dp->trw->image_cache->length > dp->trw->image_cache_size) {
				cached_pixbuf_free((CachedPixmap *) g_queue_pop_tail(dp->trw->image_cache));
			}

			pixmap = cp->pixmap;
		} else {
			pixmap = a_thumbnails_get_default(); /* thumbnail not yet loaded */
		}
	}
	if (pixmap) {
		int w = pixmap->width();
		int h = pixmap->height();

		if (x + (w / 2) > 0 && y + (h / 2) > 0 && x - (w / 2) < dp->width && y - (h / 2) < dp->height) { /* always draw within boundaries */
			if (dp->highlight) {
				/* Highlighted - so draw a little border around the chosen one
				   single line seems a little weak so draw 2 of them. */
				dp->viewport->draw_rectangle(dp->viewport->get_highlight_pen(),
							    x - (w / 2) - 1, y - (h / 2) - 1, w + 2, h + 2);
				dp->viewport->draw_rectangle(dp->viewport->get_highlight_pen(),
							    x - (w / 2) - 2, y - (h / 2) - 2, w + 4, h + 4);
			}
			dp->viewport->draw_pixmap(pixmap, 0, 0, x - (w / 2), y - (h / 2), w, h);
		}
		return 0;
	}
#endif
	/* If failed to draw picture, default to drawing regular waypoint. */
	return 1;
}




void trw_layer_draw_symbol(Waypoint * wp, int x, int y, DrawingParams * dp)
{
#ifndef K
	dp->trw->waypoint_pen.setColor(QColor("orange"));
#endif

#ifdef K
	if (dp->trw->wp_draw_symbols && wp->symbol && wp->symbol_pixmap) {
		dp->viewport->draw_pixmap(wp->symbol_pixmap, 0, 0, x - wp->symbol_pixmap->width()/2, y - wp->symbol_pixmap->height()/2, -1, -1);
	} else
#endif
		if (wp == dp->trw->current_wp) {
		switch (dp->trw->wp_symbol) {
		case WP_SYMBOL_FILLED_SQUARE:
			dp->viewport->fill_rectangle(dp->trw->waypoint_pen.color(), x - (dp->trw->wp_size), y - (dp->trw->wp_size), dp->trw->wp_size*2, dp->trw->wp_size*2);
			break;
		case WP_SYMBOL_SQUARE:
			dp->viewport->draw_rectangle(dp->trw->waypoint_pen, x - (dp->trw->wp_size), y - (dp->trw->wp_size), dp->trw->wp_size*2, dp->trw->wp_size*2);
			break;
		case WP_SYMBOL_CIRCLE:
			dp->viewport->draw_arc(dp->trw->waypoint_pen, x - dp->trw->wp_size, y - dp->trw->wp_size, dp->trw->wp_size, dp->trw->wp_size, 0, 360, true);
			break;
		case WP_SYMBOL_X:
			dp->viewport->draw_line(dp->trw->waypoint_pen, x - dp->trw->wp_size*2, y - dp->trw->wp_size*2, x + dp->trw->wp_size*2, y + dp->trw->wp_size*2);
			dp->viewport->draw_line(dp->trw->waypoint_pen, x - dp->trw->wp_size*2, y + dp->trw->wp_size*2, x + dp->trw->wp_size*2, y - dp->trw->wp_size*2);
		default:
			break;
		}
	} else {
		switch (dp->trw->wp_symbol) {
		case WP_SYMBOL_FILLED_SQUARE:
			dp->viewport->fill_rectangle(dp->trw->waypoint_pen.color(), x - dp->trw->wp_size/2, y - dp->trw->wp_size/2, dp->trw->wp_size, dp->trw->wp_size);
			break;
		case WP_SYMBOL_SQUARE:
			dp->viewport->draw_rectangle(dp->trw->waypoint_pen, x - dp->trw->wp_size/2, y - dp->trw->wp_size/2, dp->trw->wp_size, dp->trw->wp_size);
			break;
		case WP_SYMBOL_CIRCLE:
			dp->viewport->draw_arc(dp->trw->waypoint_pen, x-dp->trw->wp_size/2, y-dp->trw->wp_size/2, dp->trw->wp_size, dp->trw->wp_size, 0, 360, true);
			break;
		case WP_SYMBOL_X:
			dp->viewport->draw_line(dp->trw->waypoint_pen, x-dp->trw->wp_size, y-dp->trw->wp_size, x+dp->trw->wp_size, y+dp->trw->wp_size);
			dp->viewport->draw_line(dp->trw->waypoint_pen, x-dp->trw->wp_size, y+dp->trw->wp_size, x+dp->trw->wp_size, y-dp->trw->wp_size);
			break;
		default:
			break;
		}
	}

	return;
}




void trw_layer_draw_label(Waypoint * wp, int x, int y, DrawingParams * dp)
{
#ifdef K
	/* Thanks to the GPSDrive people (Fritz Ganter et al.) for hints on this part ... yah, I'm too lazy to study documentation. */

	/* Hopefully name won't break the markup (may need to sanitize - g_markup_escape_text()). */

	/* Could this stored in the waypoint rather than recreating each pass? */
	char * wp_label_markup = g_strdup_printf("<span size=\"%s\">%s</span>", dp->trw->wp_fsize_str, wp->name);
	if (pango_parse_markup(wp_label_markup, -1, 0, NULL, NULL, NULL, NULL)) {
		pango_layout_set_markup(dp->trw->wplabellayout, wp_label_markup, -1);
	} else {
		/* Fallback if parse failure. */
		pango_layout_set_text(dp->trw->wplabellayout, wp->name, -1);
	}
	free(wp_label_markup);
#endif

#ifdef K
	int label_x, label_y;
	int width, height;
	pango_layout_get_pixel_size(dp->trw->wplabellayout, &width, &height);
	label_x = x - width/2;
	if (wp->symbol_pixmap) {
		label_y = y - height - 2 - wp->symbol_pixmap->height()/2;
	} else {
		label_y = y - dp->trw->wp_size - height - 2;
	}
#else
	int label_x = x;
	int label_y = y;
	int width = 0;
	int height = 0;
	dp->trw->waypoint_text_pen = QPen(Qt::blue);
#endif

	if (dp->highlight) {
		dp->viewport->fill_rectangle(dp->viewport->get_highlight_pen().color(), label_x - 1, label_y-1,width+2,height+2);
	} else {
		dp->viewport->fill_rectangle(dp->trw->waypoint_bg_pen.color(), label_x - 1, label_y-1,width+2,height+2);
	}
	/* TODO: use correct font size: dp->trw->wp_fsize_str. */
	dp->viewport->draw_text(QFont("Arial", 10), dp->trw->waypoint_text_pen, label_x, label_y, QString(wp->name));

	return;
}




void trw_layer_draw_waypoint_cb(Waypoint * wp, DrawingParams * dp)
{
	if (BBOX_INTERSECT (dp->trw->waypoints_bbox, dp->bbox)) {
		trw_layer_draw_waypoint(wp, dp);
	}
}




void trw_layer_draw_waypoints_cb(Waypoints * waypoints, DrawingParams * dp)
{
	if (BBOX_INTERSECT (dp->trw->waypoints_bbox, dp->bbox)) {
		for (auto i = waypoints->begin(); i != waypoints->end(); i++) {
			trw_layer_draw_waypoint(i->second, dp);
		}
	}
}




void cached_pixmap_free(CachedPixmap * cp)
{
#ifdef K
	g_object_unref(G_OBJECT(cp->pixmap));
	free(cp->image);
#endif
}




int cached_pixmap_cmp(CachedPixmap * cp, const char * name)
{
	return strcmp(cp->image, name);
}
