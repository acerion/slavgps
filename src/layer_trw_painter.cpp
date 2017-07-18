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
#include "layer_trw_painter.h"
#include "settings.h"
#include "globals.h"
#include "vikutils.h"
#include "viewport.h"
#include "waypoint.h"
#include "track.h"
#include "layer_trw.h"
#include "window.h"




using namespace SlavGPS;




TRWPainter::TRWPainter(LayerTRW * a_trw, Viewport * a_viewport)
{
	this->trw = a_trw;
	this->viewport = a_viewport;
	this->window = this->trw->get_window();

	this->xmpp = this->viewport->get_xmpp();
	this->ympp = this->viewport->get_ympp();
	this->width = this->viewport->get_width();
	this->height = this->viewport->get_height();
	this->cc = this->trw->drawdirections_size * cos(DEG2RAD(45)); /* Calculate once per trw update - even if not used. */
	this->ss = this->trw->drawdirections_size * sin(DEG2RAD(45)); /* Calculate once per trw update - even if not used. */

	this->center = this->viewport->get_center();
	this->coord_mode = this->viewport->get_coord_mode();
	this->one_utm_zone = this->viewport->is_one_zone(); /* False if some other projection besides UTM. */

	if (this->coord_mode == CoordMode::UTM && this->one_utm_zone) {
		int w2 = this->xmpp * (this->width / 2) + 1600 / this->xmpp;
		int h2 = this->ympp * (this->height / 2) + 1600 / this->ympp;
		/* Leniency -- for tracks. Obviously for waypoints this SHOULD be a lot smaller. */

		this->ce1 = this->center->utm.easting - w2;
		this->ce2 = this->center->utm.easting + w2;
		this->cn1 = this->center->utm.northing - h2;
		this->cn2 = this->center->utm.northing + h2;

	} else if (this->coord_mode == CoordMode::LATLON) {

		/* Quick & dirty calculation; really want to check all corners due to lat/lon smaller at top in northern hemisphere. */
		/* This also DOESN'T WORK if you are crossing 180/-180 lon. I don't plan to in the near future... */
		Coord upperleft = this->viewport->screen_to_coord(-500, -500);
		Coord bottomright = this->viewport->screen_to_coord(this->width + 500, this->height + 500);
		this->ce1 = upperleft.ll.lon;
		this->ce2 = bottomright.ll.lon;
		this->cn1 = bottomright.ll.lat;
		this->cn2 = upperleft.ll.lat;
	} else {
		;
	}

	this->viewport->get_bbox(&this->bbox);
}




void TRWPainter::set_highlight(bool a_highlight)
{
	this->highlight = a_highlight;
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




void TRWPainter::draw_track_label(const QString & text, const QColor & fg_color, const QColor & bg_color, const Coord * coord)
{
#ifdef K
	char *label_markup = g_strdup_printf("<span foreground=\"%s\" background=\"%s\" size=\"%s\">%s</span>", fg_color, bg_color, this->trw->track_fsize_str, text);
	if (pango_parse_markup(label_markup, -1, 0, NULL, NULL, NULL, NULL)) {
		pango_layout_set_markup(this->trw->tracklabellayout, label_markup, -1);
	} else {
		/* Fallback if parse failure. */
		pango_layout_set_text(this->trw->tracklabellayout, text, -1);
	}

	free(label_markup);
#endif
	int label_x, label_y;
	//int width, height;
	//pango_layout_get_pixel_size(this->trw->tracklabellayout, &width, &height);

	this->viewport->coord_to_screen(coord, &label_x, &label_y);
	//this->viewport->draw_layout(this->trw->track_bg_gc, label_x-width/2, label_y-height/2, this->trw->tracklabellayout);

	QPen pen;
	pen.setColor(fg_color);
	this->viewport->draw_text(QFont("Helvetica", 10), pen, label_x, label_y, text);
}




/**
 * Draw a few labels along a track at nicely seperated distances.
 * This might slow things down if there's many tracks being displayed with this on.
 */
void TRWPainter::draw_dist_labels(Track * trk, bool drawing_highlight)
{

	static const double chunksd[] = {0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 15.0, 20.0,
					 25.0, 40.0, 50.0, 75.0, 100.0,
					 150.0, 200.0, 250.0, 500.0, 1000.0};

	double dist = trk->get_length_including_gaps() / (trk->max_number_dist_labels + 1);
	DistanceUnit distance_unit = Preferences::get_unit_distance();

	/* Convert to specified unit to find the friendly breakdown value. */
	dist = convert_distance_meters_to(distance_unit, dist);

	int index = 0;
	for (size_t i = 0; i < (sizeof chunksd) / (sizeof chunksd[0]); i++) {
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
			Coord coord(ll_new, this->trw->coord_mode);

			const QColor fg_color = this->get_fg_color(trk);
			const QColor bg_color = this->get_bg_color(drawing_highlight);

			this->draw_track_label(name, fg_color, bg_color, &coord);

			free(name);
		}
	}
}




QColor TRWPainter::get_fg_color(const Track * trk) const
{
	QColor fg_color;
	if (this->trw->drawmode == DRAWMODE_BY_TRACK) {
		fg_color = trk->color;
	} else {
		fg_color = this->trw->track_color;
	}
	return fg_color;
}




/* If highlight mode is on, then color of the background should be the
   same as the highlight colour. */
QColor TRWPainter::get_bg_color(bool drawing_highlight) const
{
	QColor bg_color;
	if (drawing_highlight) {
		bg_color = this->viewport->get_highlight_color();
	} else {
		bg_color = this->trw->track_bg_color;
	}
	return bg_color;
}




/**
 * Draw a label (or labels) for the track name somewhere depending on the track's properties.
 */
void TRWPainter::draw_track_name_labels(Track * trk, bool drawing_highlight)
{
	const QColor fg_color = this->get_fg_color(trk);
	const QColor bg_color = this->get_bg_color(drawing_highlight);

	char *ename = g_markup_escape_text(trk->name, -1);

	if (trk->draw_name_mode == TrackDrawNameMode::START_END_CENTRE ||
	    trk->draw_name_mode == TrackDrawNameMode::CENTRE) {
		struct LatLon average, maxmin[2] = { {0,0}, {0,0} };
		LayerTRW::find_maxmin_in_track(trk, maxmin);
		average.lat = (maxmin[0].lat+maxmin[1].lat)/2;
		average.lon = (maxmin[0].lon+maxmin[1].lon)/2;
		Coord coord(average, this->trw->coord_mode);

		this->draw_track_label(ename, fg_color, bg_color, &coord);
	}

	if (trk->draw_name_mode == TrackDrawNameMode::CENTRE) {
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

	if (trk->draw_name_mode == TrackDrawNameMode::START_END ||
	    trk->draw_name_mode == TrackDrawNameMode::START_END_CENTRE) {

		/* This number can be configured via the settings if you really want to change it. */
		double distance_diff;
		if (!a_settings_get_double("trackwaypoint_start_end_distance_diff", &distance_diff)) {
			distance_diff = 100.0; /* Metres. */
		}

		if (Coord::distance(begin_coord, end_coord) < distance_diff) {
			/* Start and end 'close' together so only draw one label at an average location. */
			int x1, x2, y1, y2;
			this->viewport->coord_to_screen(&begin_coord, &x1, &y1);
			this->viewport->coord_to_screen(&end_coord, &x2, &y2);
			Coord av_coord = this->viewport->screen_to_coord((x1 + x2) / 2, (y1 + y2) / 2);

			QString name = QObject::tr("%1: %2").arg(ename).arg(QObject::tr("start/end"));
			this->draw_track_label(name, fg_color, bg_color, &av_coord);

			done_start_end = true;
		}
	}

	if (!done_start_end) {
		if (trk->draw_name_mode == TrackDrawNameMode::START
		    || trk->draw_name_mode == TrackDrawNameMode::START_END
		    || trk->draw_name_mode == TrackDrawNameMode::START_END_CENTRE) {

			const QString name_start = QObject::tr("%1: %2").arg(ename).arg(QObject::tr("start"));
			this->draw_track_label(name_start, fg_color, bg_color, &begin_coord);
		}
		/* Don't draw end label if this is the one being created. */
		if (trk != this->trw->current_trk) {
			if (trk->draw_name_mode == TrackDrawNameMode::END
			    || trk->draw_name_mode == TrackDrawNameMode::START_END
			    || trk->draw_name_mode == TrackDrawNameMode::START_END_CENTRE) {

				const QString name_end = QObject::tr("%1: %2").arg(ename).arg(QObject::tr("end"));
				this->draw_track_label(name_end, fg_color, bg_color, &end_coord);
			}
		}
	}

	free(ename);
}




/**
 * Draw a point labels along a track.
 * This might slow things down if there's many tracks being displayed with this on.
 */
void TRWPainter::draw_point_names(Track * trk, bool drawing_highlight)
{
	if (trk->empty()) {
		return;
	}

	const QColor fg_color = this->get_fg_color(trk);
	const QColor bg_color = this->get_bg_color(drawing_highlight);

	for (auto iter = trk->trackpointsB->begin(); iter != trk->trackpointsB->end(); iter++) {
		if ((*iter)->name) {
			this->draw_track_label((*iter)->name, fg_color, bg_color, &(*iter)->coord);
		}
	}
}




void TRWPainter::draw_track_draw_midarrow(int x, int y, int oldx, int oldy, QPen & main_pen)
{
	int midx = (oldx + x) / 2;
	int midy = (oldy + y) / 2;

	double len = sqrt(((midx - oldx) * (midx - oldx)) + ((midy - oldy) * (midy - oldy)));
	/* Avoid divide by zero and ensure at least 1 pixel big. */
	if (len > 1) {
		double dx = (oldx - midx) / len;
		double dy = (oldy - midy) / len;
		this->viewport->draw_line(main_pen, midx, midy, midx + (dx * this->cc + dy * this->ss), midy + (dy * this->cc - dx * this->ss));
		this->viewport->draw_line(main_pen, midx, midy, midx + (dx * this->cc - dy * this->ss), midy + (dy * this->cc + dx * this->ss));
	}
}




void TRWPainter::draw_track_draw_something(int x, int y, int oldx, int oldy, QPen & main_pen, Trackpoint * tp, Trackpoint * tp_next, double min_alt, double alt_diff)
{
#define FIXALTITUDE(what) \
	((((Trackpoint *) (what))->altitude - min_alt) / alt_diff * DRAW_ELEVATION_FACTOR * this->trw->elevation_factor / this->xmpp)


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
		tmp_pen = gtk_widget_get_style(this->viewport)->light_gc[3];
	} else {
		tmp_pen = gtk_widget_get_style(this->viewport)->dark_gc[0];
	}
#endif
	this->viewport->draw_polygon(tmp_pen, points, 4, true);

	this->viewport->draw_line(main_pen, oldx, oldy - FIXALTITUDE (tp), x, y - FIXALTITUDE (tp_next));
}




void TRWPainter::draw_track(Track * trk, bool draw_track_outline)
{
	if (!trk->visible) {
		return;
	}

	/* TODO: this function is a mess, get rid of any redundancy. */

	double min_alt, max_alt, alt_diff = 0;
	if (this->trw->drawelevation) {

		/* Assume if it has elevation at the beginning, it has it throughout. not ness a true good assumption. */
		if (trk->get_minmax_alt(&min_alt, &max_alt)) {
			alt_diff = max_alt - min_alt;
		}
	}

	/* Admittedly this is not an efficient way to do it because we go through the whole GC thing all over... */
	if (this->trw->bg_line_thickness && !draw_track_outline) {
		this->draw_track(trk, true);
	}

	bool drawpoints;
	bool drawstops;
	if (draw_track_outline) {
		drawpoints = drawstops = false;
	} else {
		drawpoints = this->trw->drawpoints;
		drawstops = this->trw->drawstops;
	}

#if 1   /* Temporary test code. */
	this->draw_track_label("some label", QColor("green"), QColor("black"), this->viewport->get_center());
#endif


#if 1
	drawstops = true;
	this->trw->stop_length = 1;
	//this->trw->drawmode = DRAWMODE_BY_SPEED;
#endif

	QPen main_pen = QPen(QColor("black"));
	main_pen.setWidth(1);

	bool drawing_highlight = false;
	/* Current track - used for creation. */
	if (trk == this->trw->current_trk) {
		main_pen = this->trw->current_trk_pen;
	} else {
		if (this->highlight) {
			/* Draw all tracks of the layer in special colour.
			   NB this supercedes the drawmode. */
			main_pen = this->viewport->get_highlight_pen();
			drawing_highlight = true;
		}

		if (!drawing_highlight) {
			/* Still need to figure out the pen according to the drawing mode: */
			switch (this->trw->drawmode) {
			case DRAWMODE_BY_TRACK:
				this->trw->track_1color_pen.setColor(trk->color);
				this->trw->track_1color_pen.setWidth(this->trw->line_thickness);
				main_pen = this->trw->track_1color_pen;
				break;
			default:
				/* Mostly for DRAWMODE_ALL_SAME_COLOR
				   but includes DRAWMODE_BY_SPEED, main_pen is set later on as necessary. */
				main_pen = this->trw->track_pens[VIK_TRW_LAYER_TRACK_GC_SINGLE];
				break;
			}
		}
	}


	if (trk->empty()) {
		return;
	}

	const uint8_t tp_size_reg = this->trw->drawpoints_size;
	const uint8_t tp_size_cur = this->trw->drawpoints_size * 2;

	auto iter = trk->trackpointsB->begin();

	uint8_t tp_size = (this->trw->selected_tp.valid && *iter == *this->trw->selected_tp.iter) ? tp_size_cur : tp_size_reg;

	int x, y;
	this->viewport->coord_to_screen(&(*iter)->coord, &x, &y);

	/* Draw the first point as something a bit different from the normal points.
	   ATM it's slightly bigger and a triangle. */

	if (drawpoints) {
		QPoint trian[3] = { QPoint(x, y-(3*tp_size)), QPoint(x-(2*tp_size), y+(2*tp_size)), QPoint(x+(2*tp_size), y+(2*tp_size)) };
		this->viewport->draw_polygon(main_pen, trian, 3, true);
	}

	double average_speed = 0.0;
	double low_speed = 0.0;
	double high_speed = 0.0;
	/* If necessary calculate these values - which is done only once per track redraw. */
	if (this->trw->drawmode == DRAWMODE_BY_SPEED) {
		/* The percentage factor away from the average speed determines transistions between the levels. */
		average_speed = trk->get_average_speed_moving(this->trw->stop_length);
		low_speed = average_speed - (average_speed*(this->trw->track_draw_speed_factor/100.0));
		high_speed = average_speed + (average_speed*(this->trw->track_draw_speed_factor/100.0));
	}

	int prev_x = x;
	int prev_y = y;
	bool use_prev_xy = true; /* prev_x/prev_y contain valid coordinates of previous point. */

	iter++; /* Because first Trackpoint has been drawn above. */

	for (; iter != trk->trackpointsB->end(); iter++) {
		Trackpoint * tp = *iter;

		tp_size = (this->trw->selected_tp.valid && tp == *this->trw->selected_tp.iter) ? tp_size_cur : tp_size_reg;

		Trackpoint * prev_tp = (Trackpoint *) *std::prev(iter);
		/* See if in a different lat/lon 'quadrant' so don't draw massively long lines (presumably wrong way around the Earth).
		   Mainly to prevent wrong lines drawn when a track crosses the 180 degrees East-West longitude boundary
		   (since Viewport::draw_line() only copes with pixel value and has no concept of the globe). */
		if (this->coord_mode == CoordMode::LATLON
		    && ((prev_tp->coord.ll.lon < -90.0 && tp->coord.ll.lon > 90.0)
			|| (prev_tp->coord.ll.lon > 90.0 && tp->coord.ll.lon < -90.0))) {

			use_prev_xy = false;
			continue;
		}

		/* Check some stuff -- but only if we're in UTM and there's only ONE ZONE; or lat lon. */

		/* kamilTODO: compare this condition with condition in TRWPainter::draw_waypoint(). */
		bool first_condition = (this->coord_mode == CoordMode::UTM && !this->one_utm_zone); /* UTM coord mode & more than one UTM zone - do everything. */
		bool second_condition_A = ((!this->one_utm_zone) || tp->coord.utm.zone == this->center->utm.zone);  /* Only check zones if UTM & one_utm_zone. */
		bool second_condition_B = (tp->coord.ll.lon < this->ce2 && tp->coord.ll.lon > this->ce1) || (tp->coord.utm.easting < this->ce2 && tp->coord.utm.easting > this->ce1);
		bool second_condition_C = (tp->coord.ll.lat > this->cn1 && tp->coord.ll.lat < this->cn2) || (tp->coord.utm.northing > this->cn1 && tp->coord.utm.northing < this->cn2);
		bool second_condition = (second_condition_A && second_condition_B && second_condition_C);

#if 0
		if ((!this->one_utm_zone && !this->lat_lon) /* UTM & zones; do everything. */
		    || (((!this->one_utm_zone) || tp->coord.utm_zone == this->center->utm_zone) /* Only check zones if UTM & one_utm_zone. */
			&& tp->coord.east_west < this->ce2 && tp->coord.east_west > this->ce1 /* Both UTM and lat lon. */
			&& tp->coord.north_south > this->cn1 && tp->coord.north_south < this->cn2))

#endif

		//fprintf(stderr, "%d || (%d && %d && %d)\n", first_condition, second_condition_A, second_condition_B, second_condition_C);

		if (first_condition || second_condition) {

			//fprintf(stderr, "first branch ----\n");

			this->viewport->coord_to_screen(&(tp->coord), &x, &y);

			/* The concept of drawing stops is that if the next trackpoint has a
			   timestamp far into the future, we draw a circle of 6x trackpoint
			   size, instead of a rectangle of 2x trackpoint size. Stop is drawn
			   first so the trackpoint will be drawn on top. */
			if (drawstops
			    && drawpoints
			    && ! draw_track_outline
			    && std::next(iter) != trk->trackpointsB->end()
			    && (*std::next(iter))->timestamp - (*iter)->timestamp > this->trw->stop_length) {

				this->viewport->draw_arc(this->trw->track_pens[VIK_TRW_LAYER_TRACK_GC_STOP], x-(3*tp_size), y-(3*tp_size), 6*tp_size, 6*tp_size, 0, 360, true);
			}

			if (use_prev_xy && x == prev_x && y == prev_y) {
				/* Points are the same in display coordinates, don't
				   draw, skip drawing part. Notice that we do
				   this after drawing stops. */
				goto skip;
			}

			if (drawpoints || this->trw->drawlines) {
				/* Setup main_pen for both point and line drawing. */
				if (!drawing_highlight && (this->trw->drawmode == DRAWMODE_BY_SPEED)) {
					main_pen = this->trw->track_pens[track_section_colour_by_speed(tp, prev_tp, average_speed, low_speed, high_speed)];
				}
			}

			if (drawpoints && !draw_track_outline) {

				if (std::next(iter) != trk->trackpointsB->end()) {
					/* Regular point - draw 2x square. */
					this->viewport->fill_rectangle(main_pen.color(), x-tp_size, y-tp_size, 2*tp_size, 2*tp_size);
				} else {
					/* Final point - draw 4x circle. */
					this->viewport->draw_arc(main_pen, x-(2*tp_size), y-(2*tp_size), 4*tp_size, 4*tp_size, 0, 360, true);
				}
			}

			if ((!tp->newsegment) && (this->trw->drawlines)) {

				/* UTM only: zone check. */
				if (drawpoints && this->trw->coord_mode == CoordMode::UTM && tp->coord.utm.zone != this->center->utm.zone) {
					draw_utm_skip_insignia(this->viewport, main_pen, x, y);
				}

				if (!use_prev_xy) {
					this->viewport->coord_to_screen(&(prev_tp->coord), &prev_x, &prev_y);
				}

				if (draw_track_outline) {
					this->viewport->draw_line(this->trw->track_bg_pen, prev_x, prev_y, x, y);
				} else {
					this->viewport->draw_line(main_pen, prev_x, prev_y, x, y);

					if (this->trw->drawelevation
					    && std::next(iter) != trk->trackpointsB->end()
					    && (*std::next(iter))->altitude != VIK_DEFAULT_ALTITUDE) {

						this->draw_track_draw_something(x, y, prev_x, prev_y, main_pen, *iter, *std::next(iter), min_alt, alt_diff);
					}
				}
			}

			if ((!tp->newsegment) && this->trw->drawdirections) {
				/* Draw an arrow at the mid point to show the direction of the track.
				   Code is a rework from vikwindow::draw_ruler(). */
				this->draw_track_draw_midarrow(x, y, prev_x, prev_y, main_pen);
			}

		skip:
			prev_x = x;
			prev_y = y;
			use_prev_xy = true;
		} else {

			if (use_prev_xy && this->trw->drawlines && (!tp->newsegment)) {
				if (this->trw->coord_mode != CoordMode::UTM || tp->coord.utm.zone == this->center->utm.zone) {
					this->viewport->coord_to_screen(&(tp->coord), &x, &y);

					if (!drawing_highlight && (this->trw->drawmode == DRAWMODE_BY_SPEED)) {
						main_pen = this->trw->track_pens[track_section_colour_by_speed(tp, prev_tp, average_speed, low_speed, high_speed)];
					}

					/* Draw only if current point has different coordinates than the previous one. */
					if (x != prev_x || y != prev_y) {
						if (draw_track_outline) {
							this->viewport->draw_line(this->trw->track_bg_pen, prev_x, prev_y, x, y);
						} else {
							this->viewport->draw_line(main_pen, prev_x, prev_y, x, y);
						}
					}
				} else {
					/* Draw only if current point has different coordinates than the previous one. */
					if (x != prev_x && y != prev_y) { /* kamilFIXME: is && a correct condition? */
						this->viewport->coord_to_screen(&(prev_tp->coord), &x, &y);
						draw_utm_skip_insignia(this->viewport, main_pen, x, y);
					}
				}
			}
			use_prev_xy = false;
		}
	}

	/* Labels drawn after the trackpoints, so the labels are on top. */
	if (this->trw->track_draw_labels) {
		if (trk->max_number_dist_labels > 0) {
			this->draw_dist_labels(trk, drawing_highlight);
		}
		this->draw_point_names(trk, drawing_highlight);

		if (trk->draw_name_mode != TrackDrawNameMode::NONE) {
			this->draw_track_name_labels(trk, drawing_highlight);
		}
	}
}




void TRWPainter::draw_track_cb(const void * id, Track * trk)
{
	if (BBOX_INTERSECT (trk->bbox, this->bbox)) {
		this->draw_track(trk, false);
	}
}




void TRWPainter::draw_tracks_cb(Tracks & tracks)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		if (BBOX_INTERSECT (i->second->bbox, this->bbox)) {
			this->draw_track(i->second, false);
		}
	}
}




void TRWPainter::draw_waypoint(Waypoint * wp)
{
	if (!wp->visible) {
		return;
	}

	bool cond = (this->coord_mode == CoordMode::UTM && !this->one_utm_zone)
		|| ((this->coord_mode == CoordMode::LATLON || wp->coord.utm.zone == this->center->utm.zone) &&
		    (wp->coord.ll.lon < this->ce2 && wp->coord.ll.lon > this->ce1 && wp->coord.ll.lat > this->cn1 && wp->coord.ll.lat < this->cn2)
		    || (wp->coord.utm.easting < this->ce2 && wp->coord.utm.easting > this->ce1 && wp->coord.utm.northing > this->cn1 && wp->coord.utm.northing < this->cn2));


	if (!cond) {
		return;
	}

	int x, y;
	this->viewport->coord_to_screen(&(wp->coord), &x, &y);

	/* If in shrunken_cache, get that. If not, get and add to shrunken_cache. */
	if (wp->image && this->trw->drawimages)	{
		if (0 == this->draw_image(wp, x, y)) {
			return;
		}
	}

	/* Draw appropriate symbol - either symbol image or simple types. */
	this->draw_symbol(wp, x, y);

	if (this->trw->drawlabels) {
		this->draw_label(wp, x, y);
	}
}




int TRWPainter::draw_image(Waypoint * wp, int x, int y)
{
	if (this->trw->image_alpha == 0) {
		return 0;
	}

#ifdef K

	QPixmap * pixmap = NULL;
	GList * l = g_list_find_custom(this->trw->image_cache->head, wp->image, (GCompareFunc) cached_pixmap_cmp);
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
			if (this->trw->image_size == 128) {
				cp->pixmap = regularthumb;
			} else {
				cp->pixmap = a_thumbnails_scale_pixmap(regularthumb, this->trw->image_size, this->trw->image_size);
				assert (cp->pixmap);
				g_object_unref(G_OBJECT(regularthumb));
			}
			cp->image = g_strdup(image);

			/* Apply alpha setting to the image before the pixmap gets stored in the cache. */
			if (this->trw->image_alpha != 255) {
				cp->pixmap = ui_pixmap_set_alpha(cp->pixmap, this->trw->image_alpha);
			}

			/* Needed so 'click picture' tool knows how big the pic is; we don't
			   store it in cp because they may have been freed already. */
			wp->image_width = cp->pixmap->width();
			wp->image_height = cp->pixmap->heigth();

			g_queue_push_head(this->trw->image_cache, cp);
			if (this->trw->image_cache->length > this->trw->image_cache_size) {
				cached_pixbuf_free((CachedPixmap *) g_queue_pop_tail(this->trw->image_cache));
			}

			pixmap = cp->pixmap;
		} else {
			pixmap = a_thumbnails_get_default(); /* thumbnail not yet loaded */
		}
	}
	if (pixmap) {
		int w = pixmap->width();
		int h = pixmap->height();

		if (x + (w / 2) > 0 && y + (h / 2) > 0 && x - (w / 2) < this->width && y - (h / 2) < this->height) { /* always draw within boundaries */
			if (this->highlight) {
				/* Highlighted - so draw a little border around the chosen one
				   single line seems a little weak so draw 2 of them. */
				this->viewport->draw_rectangle(this->viewport->get_highlight_pen(),
							    x - (w / 2) - 1, y - (h / 2) - 1, w + 2, h + 2);
				this->viewport->draw_rectangle(this->viewport->get_highlight_pen(),
							    x - (w / 2) - 2, y - (h / 2) - 2, w + 4, h + 4);
			}
			this->viewport->draw_pixmap(pixmap, 0, 0, x - (w / 2), y - (h / 2), w, h);
		}
		return 0;
	}
#endif
	/* If failed to draw picture, default to drawing regular waypoint. */
	return 1;
}




void TRWPainter::draw_symbol(Waypoint * wp, int x, int y)
{
#ifndef K
	this->trw->waypoint_pen.setColor(QColor("orange"));
#endif

#ifdef K
	if (this->trw->wp_draw_symbols && wp->symbol && wp->symbol_pixmap) {
		this->viewport->draw_pixmap(wp->symbol_pixmap, 0, 0, x - wp->symbol_pixmap->width()/2, y - wp->symbol_pixmap->height()/2, -1, -1);
	} else
#endif
		if (wp == this->trw->current_wp) {
		switch (this->trw->wp_symbol) {
		case WP_SYMBOL_FILLED_SQUARE:
			this->viewport->fill_rectangle(this->trw->waypoint_pen.color(), x - (this->trw->wp_size), y - (this->trw->wp_size), this->trw->wp_size*2, this->trw->wp_size*2);
			break;
		case WP_SYMBOL_SQUARE:
			this->viewport->draw_rectangle(this->trw->waypoint_pen, x - (this->trw->wp_size), y - (this->trw->wp_size), this->trw->wp_size*2, this->trw->wp_size*2);
			break;
		case WP_SYMBOL_CIRCLE:
			this->viewport->draw_arc(this->trw->waypoint_pen, x - this->trw->wp_size, y - this->trw->wp_size, this->trw->wp_size, this->trw->wp_size, 0, 360, true);
			break;
		case WP_SYMBOL_X:
			this->viewport->draw_line(this->trw->waypoint_pen, x - this->trw->wp_size*2, y - this->trw->wp_size*2, x + this->trw->wp_size*2, y + this->trw->wp_size*2);
			this->viewport->draw_line(this->trw->waypoint_pen, x - this->trw->wp_size*2, y + this->trw->wp_size*2, x + this->trw->wp_size*2, y - this->trw->wp_size*2);
		default:
			break;
		}
	} else {
		switch (this->trw->wp_symbol) {
		case WP_SYMBOL_FILLED_SQUARE:
			this->viewport->fill_rectangle(this->trw->waypoint_pen.color(), x - this->trw->wp_size/2, y - this->trw->wp_size/2, this->trw->wp_size, this->trw->wp_size);
			break;
		case WP_SYMBOL_SQUARE:
			this->viewport->draw_rectangle(this->trw->waypoint_pen, x - this->trw->wp_size/2, y - this->trw->wp_size/2, this->trw->wp_size, this->trw->wp_size);
			break;
		case WP_SYMBOL_CIRCLE:
			this->viewport->draw_arc(this->trw->waypoint_pen, x-this->trw->wp_size/2, y-this->trw->wp_size/2, this->trw->wp_size, this->trw->wp_size, 0, 360, true);
			break;
		case WP_SYMBOL_X:
			this->viewport->draw_line(this->trw->waypoint_pen, x-this->trw->wp_size, y-this->trw->wp_size, x+this->trw->wp_size, y+this->trw->wp_size);
			this->viewport->draw_line(this->trw->waypoint_pen, x-this->trw->wp_size, y+this->trw->wp_size, x+this->trw->wp_size, y-this->trw->wp_size);
			break;
		default:
			break;
		}
	}

	return;
}




void TRWPainter::draw_label(Waypoint * wp, int x, int y)
{
#ifdef K
	/* Thanks to the GPSDrive people (Fritz Ganter et al.) for hints on this part ... yah, I'm too lazy to study documentation. */

	/* Hopefully name won't break the markup (may need to sanitize - g_markup_escape_text()). */

	/* Could this stored in the waypoint rather than recreating each pass? */
	char * wp_label_markup = g_strdup_printf("<span size=\"%s\">%s</span>", this->trw->wp_fsize_str, wp->name);
	if (pango_parse_markup(wp_label_markup, -1, 0, NULL, NULL, NULL, NULL)) {
		pango_layout_set_markup(this->trw->wplabellayout, wp_label_markup, -1);
	} else {
		/* Fallback if parse failure. */
		pango_layout_set_text(this->trw->wplabellayout, wp->name, -1);
	}
	free(wp_label_markup);
#endif

#ifdef K
	int label_x, label_y;
	int width, height;
	pango_layout_get_pixel_size(this->trw->wplabellayout, &width, &height);
	label_x = x - width/2;
	if (wp->symbol_pixmap) {
		label_y = y - height - 2 - wp->symbol_pixmap->height()/2;
	} else {
		label_y = y - this->trw->wp_size - height - 2;
	}
#else
	int label_x = x;
	int label_y = y;
	//int width = 0;
	//int height = 0;
	this->trw->waypoint_text_pen = QPen(Qt::blue);
#endif

	if (this->highlight) {
		this->viewport->fill_rectangle(this->viewport->get_highlight_pen().color(), label_x - 1, label_y-1,width+2,height+2);
	} else {
		this->viewport->fill_rectangle(this->trw->waypoint_bg_pen.color(), label_x - 1, label_y-1,width+2,height+2);
	}
	/* TODO: use correct font size: this->trw->wp_fsize_str. */
	this->viewport->draw_text(QFont("Arial", 10), this->trw->waypoint_text_pen, label_x, label_y, QString(wp->name));

	return;
}




void TRWPainter::draw_waypoint_cb(Waypoint * wp)
{
	if (BBOX_INTERSECT (this->trw->waypoints_bbox, this->bbox)) {
		this->draw_waypoint(wp);
	}
}




void TRWPainter::draw_waypoints_cb(Waypoints * waypoints)
{
	if (BBOX_INTERSECT (this->trw->waypoints_bbox, this->bbox)) {
		for (auto i = waypoints->begin(); i != waypoints->end(); i++) {
			this->draw_waypoint(i->second);
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