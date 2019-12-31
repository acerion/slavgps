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
 */




#include <cassert>
#include <cmath>




#include <QDebug>




#include <glib.h>




#include "thumbnails.h"
#include "ui_util.h"
#include "application_state.h"
#include "vikutils.h"
#include "viewport.h"
#include "viewport_internal.h"
#include "layer_trw_waypoint.h"
#include "layer_trw_painter.h"
#include "layer_trw_track_internal.h"
#include "layer_trw.h"
#include "window.h"
#include "preferences.h"
#include "graph_intervals.h"




using namespace SlavGPS;




#define SG_MODULE "Layer TRW Painter"




/* Height of elevation plotting, sort of relative to zoom level ("mpp" that isn't mpp necessarily). */
/* This is multiplied by user-inputted value from 1-100. */
#define DRAW_ELEVATION_FACTOR 30




static int pango_font_size_to_point_font_size(font_size_t font_size);




enum class LayerTRWTrackGraphics {
	Speed1,
	Speed2,
	Speed3,

	StopPen,
	SinglePen,
	NeutralPen,

	Max
};




LayerTRWPainter::LayerTRWPainter(LayerTRW * new_trw)
{
	this->trw = new_trw;
	this->window = this->trw->get_window();
}




void LayerTRWPainter::set_viewport(GisViewport * new_gisview)
{
	this->gisview = new_gisview;

	this->vp_xmpp = this->gisview->get_viking_scale().get_x();
	this->vp_ympp = this->gisview->get_viking_scale().get_y();

	this->vp_rect = this->gisview->central_get_rect();
	this->vp_center = this->gisview->get_center_coord();
	this->vp_coord_mode = this->gisview->get_coord_mode();
	this->vp_is_one_utm_zone = this->gisview->get_is_one_utm_zone(); /* False if some other projection besides UTM. */

	this->track_arrow = ArrowSymbol(45, this->draw_track_directions_size); /* Calculate once per trw update - even if not used. */

	if (this->vp_coord_mode == CoordMode::UTM && this->vp_is_one_utm_zone) {

		/* Leniency -- for tracks. Obviously for waypoints
		   this SHOULD be a lot smaller. */
		const int outside_margin = 1600; /* TODO_LATER: magic number. */

		const int width = this->vp_xmpp * (this->vp_rect.width() / 2) + outside_margin / this->vp_xmpp;
		const int height = this->vp_ympp * (this->vp_rect.height() / 2) + outside_margin / this->vp_ympp;


		this->coord_leftmost = this->vp_center.utm.get_easting() - width;
		this->coord_rightmost = this->vp_center.utm.get_easting() + width;
		this->coord_bottommost = this->vp_center.utm.get_northing() - height;
		this->coord_topmost = this->vp_center.utm.get_northing() + height;

	} else if (this->vp_coord_mode == CoordMode::LatLon) {

		/* Quick & dirty calculation; really want to check all corners due to lat/lon smaller at top in northern hemisphere. */
		/* This also DOESN'T WORK if you are crossing 180/-180 lon. I don't plan to in the near future... */

		/* Leniency -- for tracks. Obviously for waypoints
		   this SHOULD be a lot smaller. */
		const int outside_margin = 500; /* TODO_LATER: magic number. */

		const Coord upperleft = this->gisview->screen_pos_to_coord(-outside_margin, -outside_margin);
		const Coord bottomright = this->gisview->screen_pos_to_coord(this->vp_rect.width() + outside_margin, this->vp_rect.height() + outside_margin);

		this->coord_leftmost = upperleft.lat_lon.lon;
		this->coord_rightmost = bottomright.lat_lon.lon;
		this->coord_bottommost = bottomright.lat_lon.lat;
		this->coord_topmost = upperleft.lat_lon.lat;
	} else {
		;
	}
}




/*
  \brief Determine the color of the trackpoint (and/or trackline) relative to the average speed of whole track
  Here a simple traffic like light color system is used:
  . slow points are red
  . average is yellow
  . fast points are green
*/
class SpeedColoring {
public:
	SpeedColoring(const Speed & low, const Speed & average, const Speed & high) : low_speed(low), average_speed(average), high_speed(high) {};
	SpeedColoring() {};
	void set(const Speed & low, const Speed & average, const Speed & high) { low_speed = low; average_speed = average; high_speed = high; };
	LayerTRWTrackGraphics get(const Trackpoint * tp1, const Trackpoint * tp2);
private:
	Speed low_speed;
	Speed average_speed;
	Speed high_speed;
};




LayerTRWTrackGraphics SpeedColoring::get(const Trackpoint * tp1, const Trackpoint * tp2)
{
	if (!tp1->timestamp.is_valid() || !tp2->timestamp.is_valid()) {
		return LayerTRWTrackGraphics::NeutralPen;
	}
	if (!average_speed.is_positive()) {
		return LayerTRWTrackGraphics::NeutralPen;
	}

	const Distance distance = Coord::distance_2(tp1->coord, tp2->coord);
	const Duration duration = Time::get_abs_duration(tp1->timestamp, tp2->timestamp);
	Speed speed;
	speed.make_speed(distance, duration);
	if (speed < low_speed) {
		return LayerTRWTrackGraphics::Speed1;
	} else if (speed > high_speed) {
		return LayerTRWTrackGraphics::Speed3;
	} else {
		return LayerTRWTrackGraphics::Speed2;
	}
}




static void draw_utm_skip_insignia(GisViewport * gisview, QPen & pen, int x, int y)
{
	/* First draw '+'. */
	gisview->draw_line(pen, x+5, y,   x-5, y );
	gisview->draw_line(pen, x,   y+5, x,   y-5);

	/* And now draw 'x' on top of it. */
	gisview->draw_line(pen, x+5, y+5, x-5, y-5);
	gisview->draw_line(pen, x+5, y-5, x-5, y+5);
}




void LayerTRWPainter::draw_track_label(const QString & text, const QColor & fg_color, const QColor & bg_color, const Coord & coord)
{
	ScreenPos label_pos;
	this->gisview->coord_to_screen_pos(coord, label_pos);

	QPen pen;
	pen.setColor(fg_color);
	this->gisview->draw_text(QFont("Helvetica", pango_font_size_to_point_font_size(this->track_label_font_size)),
				 pen,
				 label_pos.x(),
				 label_pos.y(),
				 text);
}




/**
 * Draw a few labels along a track at nicely seperated distances.
 * This might slow things down if there's many tracks being displayed with this on.
 */
void LayerTRWPainter::draw_track_dist_labels(Track * trk, bool do_highlight)
{
	const DistanceUnit user_distance_unit = Preferences::get_unit_distance();
	const Distance start_distance(0.0, user_distance_unit);
	const Distance track_length = trk->get_length_including_gaps().convert_to_unit(user_distance_unit);

	const int n_intervals_max = trk->max_number_dist_labels;
	GraphIntervals<Distance> intervals;
	const Distance interval = intervals.get_interval(start_distance, track_length, n_intervals_max);

	for (int i = 1; i <= n_intervals_max; i++) {
		const Distance axis_mark_uu = interval * i;

		/* Convert distance into metres for use in finding a trackpoint. */
		const Distance axis_mark_iu = axis_mark_uu.convert_to_unit(DistanceUnit::Meters);
		if (!axis_mark_iu.is_valid()) {
			qDebug() << SG_PREFIX_E << "Conversion to meters failed";
			break;
		}

		double dist_current = 0.0;
		Trackpoint * tp_current = trk->get_tp_by_dist(axis_mark_iu.m_ll_value, false, &dist_current);
		double dist_next = 0.0;
		Trackpoint * tp_next = trk->get_tp_by_dist(axis_mark_iu.m_ll_value, true, &dist_next);

		double dist_between_tps = std::fabs(dist_next - dist_current);
		double ratio = 0.0;
		/* Prevent division by 0 errors. */
		if (dist_between_tps > 0.0) {
			ratio = std::fabs(axis_mark_iu.m_ll_value - dist_current) / dist_between_tps;
		}

		if (tp_current && tp_next) {

			const LatLon ll_current = tp_current->coord.get_lat_lon();
			const LatLon ll_next = tp_next->coord.get_lat_lon();

			/* Positional interpolation.
			   Using a simple ratio - may not be perfectly correct due to lat/lon projections
			   but should be good enough over the small scale that I anticipate usage on. */
			const LatLon ll_middle(ll_current.lat + (ll_next.lat - ll_current.lat) * ratio,
					       ll_current.lon + (ll_next.lon - ll_current.lon) * ratio);
			const Coord coord_middle(ll_middle, this->trw->coord_mode);

			const QColor fg_color = this->get_fg_color(trk);
			const QColor bg_color = this->get_bg_color(do_highlight);

			this->draw_track_label(axis_mark_uu.to_nice_string(), fg_color, bg_color, coord_middle);
		}
	}
}




QColor LayerTRWPainter::get_fg_color(const Track * trk) const
{
	return this->track_drawing_mode == LayerTRWTrackDrawingMode::ByTrack ? trk->color : this->track_color_common;
}




/* If highlight mode is on, then color of the background should be the
   same as the highlight color. */
QColor LayerTRWPainter::get_bg_color(bool do_highlight) const
{
	return do_highlight ? this->gisview->get_highlight_color() : this->track_bg_color;
}




/**
 * Draw a label (or labels) for the track name somewhere depending on the track's properties.
 */
void LayerTRWPainter::draw_track_name_labels(Track * trk, bool do_highlight)
{
	const QColor fg_color = this->get_fg_color(trk);
	const QColor bg_color = this->get_bg_color(do_highlight);

	char *ename = g_markup_escape_text(trk->get_name().toUtf8().constData(), -1);

	if (trk->draw_name_mode == TrackDrawNameMode::StartCentreEnd ||
	    trk->draw_name_mode == TrackDrawNameMode::Centre) {

		const Coord coord(trk->get_bbox().get_center_lat_lon(), this->trw->coord_mode);
		this->draw_track_label(ename, fg_color, bg_color, coord);
	}

	if (trk->draw_name_mode == TrackDrawNameMode::Centre) {
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
	const Coord begin_coord = tp_begin->coord;
	const Coord end_coord = tp_end->coord;

	bool done_start_end = false;

	if (trk->draw_name_mode == TrackDrawNameMode::StartEnd ||
	    trk->draw_name_mode == TrackDrawNameMode::StartCentreEnd) {

		/* This number can be configured via the settings if you really want to change it. */
		double distance_diff;
		if (!ApplicationState::get_double("trackwaypoint_start_end_distance_diff", &distance_diff)) {
			distance_diff = 100.0; /* Metres. */
		}

		if (Coord::distance(begin_coord, end_coord) < distance_diff) {
			/* Start and end 'close' together so only draw one label at an average location. */
			ScreenPos begin_pos;
			this->gisview->coord_to_screen_pos(begin_coord, begin_pos);

			ScreenPos end_pos;
			this->gisview->coord_to_screen_pos(end_coord, end_pos);

			const Coord av_coord = this->gisview->screen_pos_to_coord(ScreenPos::get_average(begin_pos, end_pos));

			QString name = QObject::tr("%1: start/end").arg(ename);
			this->draw_track_label(name, fg_color, bg_color, av_coord);

			done_start_end = true;
		}
	}

	if (!done_start_end) {
		if (trk->draw_name_mode == TrackDrawNameMode::Start
		    || trk->draw_name_mode == TrackDrawNameMode::StartEnd
		    || trk->draw_name_mode == TrackDrawNameMode::StartCentreEnd) {

			const QString name_start = QObject::tr("%1: start").arg(ename);
			this->draw_track_label(name_start, fg_color, bg_color, begin_coord);
		}
		/* Don't draw end label if this is the one being created. */
		if (trk != this->trw->selected_track_get()) {
			if (trk->draw_name_mode == TrackDrawNameMode::End
			    || trk->draw_name_mode == TrackDrawNameMode::StartEnd
			    || trk->draw_name_mode == TrackDrawNameMode::StartCentreEnd) {

				const QString name_end = QObject::tr("%1: end").arg(ename);
				this->draw_track_label(name_end, fg_color, bg_color, end_coord);
			}
		}
	}

	free(ename);
}




/**
 * Draw a point labels along a track.
 * This might slow things down if there's many tracks being displayed with this on.
 */
void LayerTRWPainter::draw_track_point_names(Track * trk, bool do_highlight)
{
	if (trk->empty()) {
		return;
	}

	const QColor fg_color = this->get_fg_color(trk);
	const QColor bg_color = this->get_bg_color(do_highlight);

	for (auto iter = trk->trackpoints.begin(); iter != trk->trackpoints.end(); iter++) {
		if (!(*iter)->name.isEmpty()) {
			this->draw_track_label((*iter)->name, fg_color, bg_color, (*iter)->coord);
		}
	}
}




void LayerTRWPainter::draw_track_draw_midarrow(const ScreenPos & begin, const ScreenPos & end, QPen & pen)
{
	const fpixel midx = (begin.x() + end.x()) / 2.0;
	const fpixel midy = (begin.y() + end.y()) / 2.0;

	const double len = sqrt(((midx - begin.x()) * (midx - begin.x())) + ((midy - begin.y()) * (midy - begin.y())));
	/* Avoid divide by zero and ensure at least 1 pixel big. */
	if (len > 1) {
		const double dx = (begin.x() - midx) / len;
		const double dy = (begin.y() - midy) / len;

		QPainter & painter = this->gisview->get_painter();
		painter.setPen(pen);

		this->track_arrow.set_arrow_tip(midx, midy);
		this->track_arrow.paint(painter, dx, dy);
	}
}




void LayerTRWPainter::draw_track_draw_something(const ScreenPos & begin, const ScreenPos & end, QPen & pen, Trackpoint * tp, Trackpoint * tp_next, const Altitude & min_alt, const Altitude & alt_diff)
{
#define FIXALTITUDE(m_tp) \
	((m_tp->altitude - min_alt) / alt_diff * DRAW_ELEVATION_FACTOR * this->track_elevation_factor / this->vp_xmpp)


	ScreenPos points[4];

	points[0] = begin;
	points[1] = ScreenPos(begin.x(), begin.y() - FIXALTITUDE (tp));
	points[2] = ScreenPos(end.x(), end.y() - FIXALTITUDE (tp_next));
	points[3] = end;

	QPen tmp_pen;
#ifdef K_FIXME_RESTORE
	if (((begin.x() - x) > 0 && (begin.y() - y) > 0) || ((begin.x() - x) < 0 && (begin.y() - y) < 0)) {
		tmp_pen = gtk_widget_get_style(this->viewport)->light_gc[3];
	} else {
		tmp_pen = gtk_widget_get_style(this->viewport)->dark_gc[0];
	}
#else
	tmp_pen.setColor("green");
	tmp_pen.setWidth(1);
#endif
	this->gisview->draw_polygon(tmp_pen, points, 4, true);

	this->gisview->draw_line(pen, begin.x(), begin.y() - FIXALTITUDE (tp), end.x(), end.y() - FIXALTITUDE (tp_next));
}




QPen LayerTRWPainter::get_track_fg_pen(Track * trk, bool do_highlight)
{
	QPen result;

	if (trk->is_selected()) {
		/* The track is highlighted/selected user, it gets a special pen. */
		result = this->m_selected_track_pen;
	} else if (do_highlight) {
		/* Draw all tracks of the layer in 'highlight' color.
		   This supersedes the this->track_drawing_mode. */
		result = this->gisview->get_highlight_pen();
	} else {
		/* Figure out the pen according to the drawing mode. */
		switch (this->track_drawing_mode) {
		case LayerTRWTrackDrawingMode::ByTrack:
			result.setColor(trk->color);
			result.setWidth(this->track_thickness);
			break;
		default:
			/* Mostly for LayerTRWTrackDrawingMode::AllSameColor
			   but includes LayerTRWTrackDrawingMode::BySpeed, the pen is set later on as necessary. */
			result = this->track_pens[(int) LayerTRWTrackGraphics::SinglePen];
			break;
		}
	}

	return result;
}




void LayerTRWPainter::draw_track_fg_sub(Track * trk, bool do_highlight)
{
	Altitude min_alt;
	Altitude max_alt;
	Altitude alt_diff;
	if (this->draw_track_elevation) {
		/* Assume if it has elevation at the beginning, it has it throughout. not ness a true good assumption. */
		if (trk->get_minmax_alt(min_alt, max_alt)) {
			alt_diff = max_alt - min_alt;
		}
	}

	const bool do_draw_trackpoints = this->draw_trackpoints;
	const bool do_draw_track_stops = do_highlight ? false : this->draw_track_stops;


#if 1   /* Temporary test code. */
	this->draw_track_label("test track label", QColor("green"), QColor("black"), this->gisview->get_center_coord());
#endif


	QPen main_pen = this->get_track_fg_pen(trk, do_highlight);
	/* If track_drawing_mode == LayerTRWTrackDrawingMode::BySpeed, main_pen may be overwritten below. */



	const int tp_size_reg = this->trackpoint_size;
	const int tp_size_cur = this->trackpoint_size * 2;

	auto iter = trk->trackpoints.begin();
	Trackpoint * tp = *iter;

	const bool trk_is_selected = trk->is_selected();
	int tp_size = (trk_is_selected && trk->get_selected_children().is_member(tp)) ? tp_size_cur : tp_size_reg;

	ScreenPos curr_pos;
	this->gisview->coord_to_screen_pos((*iter)->coord, curr_pos);

	/* Draw the first point as something a bit different from the normal points.
	   ATM it's slightly bigger and a triangle. */

	if (do_draw_trackpoints) {
		ScreenPos trian[3] = { ScreenPos(curr_pos.x(), curr_pos.y() - (3*tp_size)),
				       ScreenPos(curr_pos.x() - (2*tp_size), curr_pos.y() + (2*tp_size)),
				       ScreenPos(curr_pos.x() + (2*tp_size), curr_pos.y() + (2*tp_size)) };
		this->gisview->draw_polygon(main_pen, trian, 3, true);
	}


	SpeedColoring speed_coloring;
	/* If necessary calculate these values - which is done only once per track redraw. */
	if (this->track_drawing_mode == LayerTRWTrackDrawingMode::BySpeed) {
		/* The percentage factor away from the average speed
		   determines transistions between the levels. */
		Speed average_speed = trk->get_average_speed_moving(this->track_min_stop_duration);
		if (average_speed.is_valid()) {
			const Speed low_speed = average_speed - (average_speed * (this->track_draw_speed_factor / 100.0));
			const Speed high_speed = average_speed + (average_speed * (this->track_draw_speed_factor / 100.0));
			speed_coloring.set(low_speed, average_speed, high_speed);
		}
	}

	ScreenPos prev_pos = curr_pos;
	bool use_prev_pos = true; /* prev_pos contains valid coordinates of previous point. */

	iter++; /* Because first Trackpoint has been drawn above. */

	for (; iter != trk->trackpoints.end(); iter++) {
		tp = *iter;
		Trackpoint * prev_tp = (Trackpoint *) *std::prev(iter);

		tp_size = (trk_is_selected && trk->get_selected_children().is_member(tp)) ? tp_size_cur : tp_size_reg;


		/* See if in a different lat/lon 'quadrant' so don't draw massively long lines (presumably wrong way around the Earth).
		   Mainly to prevent wrong lines drawn when a track crosses the 180 degrees East-West longitude boundary
		   (since ViewportPixmap::draw_line() only copes with pixel value and has no concept of the globe). */
		if (this->vp_coord_mode == CoordMode::LatLon
		    && ((prev_tp->coord.lat_lon.lon < -90.0 && tp->coord.lat_lon.lon > 90.0)
			|| (prev_tp->coord.lat_lon.lon > 90.0 && tp->coord.lat_lon.lon < -90.0))) {

			use_prev_pos = false;
			continue;
		}

		/* Check some stuff -- but only if we're in UTM and there's only ONE ZONE; or lat lon. */

		/* TODO_LATER: compare this condition with condition in LayerTRWPainter::draw_waypoint_sub(). */
		bool first_condition = (this->vp_coord_mode == CoordMode::UTM && !this->vp_is_one_utm_zone); /* UTM coord mode & more than one UTM zone - do everything. */

		bool second_condition_A = ((!this->vp_is_one_utm_zone) || UTM::is_the_same_zone(tp->coord.utm, this->vp_center.utm));  /* Only check zones if UTM & one_utm_zone. */

		const bool fits_into_viewport = this->coord_fits_in_viewport(tp->coord);


		bool second_condition = (second_condition_A && fits_into_viewport);
#ifdef K_OLD_IMPLEMENTATION
		if ((!this->vp_is_one_utm_zone && !this->lat_lon) /* UTM & zones; do everything. */
		    || (((!this->vp_is_one_utm_zone) || tp->coord.utm_zone == this->center->utm_zone) /* Only check zones if UTM & one_utm_zone. */
			&& tp->coord.east_west < this->coord_rightmost && tp->coord.east_west > this->coord_leftmost /* Both UTM and lat lon. */
			&& tp->coord.north_south > this->coord_bottommost && tp->coord.north_south < this->coord_topmost))
#endif

		//fprintf(stderr, "%d || (%d && %d && %d)\n", first_condition, second_condition_A, fits_horizontally, fits_vertically);

		if (first_condition || second_condition) {

			//fprintf(stderr, "first branch ----\n");

			this->gisview->coord_to_screen_pos(tp->coord, curr_pos);

			/* The concept of drawing stops is that if the next trackpoint has a
			   timestamp far into the future, we draw a circle of 6x trackpoint
			   size, instead of a rectangle of 2x trackpoint size. Stop is drawn
			   first so the trackpoint will be drawn on top. */
			if (do_draw_track_stops
			    && do_draw_trackpoints
			    && !do_highlight
			    && std::next(iter) != trk->trackpoints.end()) {

				const Duration timestamp_diff = Time::get_abs_duration((*std::next(iter))->timestamp, (*iter)->timestamp);
				if (timestamp_diff > this->track_min_stop_duration) {
					const int stop_radius = (6 * tp_size) / 2;
					this->gisview->fill_ellipse(this->track_pens[(int) LayerTRWTrackGraphics::StopPen].color(),
								    curr_pos,
								    stop_radius, stop_radius);
				}
			}

			if (use_prev_pos && curr_pos == prev_pos) {
				/* Points are the same in display coordinates, don't
				   draw, skip drawing part. Notice that we do
				   this after drawing stops. */
				goto skip;
			}

			if (do_draw_trackpoints || this->draw_track_lines) {
				/* Setup main_pen for both point and line drawing. */
				if (!do_highlight && (this->track_drawing_mode == LayerTRWTrackDrawingMode::BySpeed)) {
					main_pen = this->track_pens[(int) speed_coloring.get(tp, prev_tp)];
				}
			}

			if (do_draw_trackpoints) {
				if (std::next(iter) != trk->trackpoints.end()) {
					/* Regular point - draw 2x square. */
					this->gisview->fill_rectangle(main_pen.color(), curr_pos.x() - tp_size, curr_pos.y() - tp_size, 2*tp_size, 2*tp_size);
				} else {
					/* Final point - draw 4x circle. */
					const int tp_radius = (4 * tp_size) / 2;
					this->gisview->fill_ellipse(main_pen.color(), curr_pos, tp_radius, tp_radius);
				}
			}

			if (!tp->newsegment && this->draw_track_lines) {

				/* UTM only: zone check. */
				if (do_draw_trackpoints && this->trw->coord_mode == CoordMode::UTM && !UTM::is_the_same_zone(tp->coord.utm, this->vp_center.utm)) {
					draw_utm_skip_insignia(this->gisview, main_pen, curr_pos.x(), curr_pos.y());
				}

				if (!use_prev_pos) {
					this->gisview->coord_to_screen_pos(prev_tp->coord, prev_pos);
				}

				this->gisview->draw_line(main_pen, prev_pos, curr_pos);

				if (this->draw_track_elevation
				    && std::next(iter) != trk->trackpoints.end()
				    && (*std::next(iter))->altitude.is_valid()
				    && alt_diff.is_valid()) {

					this->draw_track_draw_something(prev_pos, curr_pos, main_pen, *iter, *std::next(iter), min_alt, alt_diff);
				}
			}

			if (!tp->newsegment && this->draw_track_directions) {
				/* Draw an arrow at the mid point to show the direction of the track.
				   Code is a rework from vikwindow::draw_ruler(). */
				this->draw_track_draw_midarrow(prev_pos, curr_pos, main_pen);
			}

		skip:
			prev_pos = curr_pos;
			use_prev_pos = true;

		} else {

			if (use_prev_pos && this->draw_track_lines && (!tp->newsegment)) {
				if (this->trw->coord_mode != CoordMode::UTM || UTM::is_the_same_zone(tp->coord.utm, this->vp_center.utm)) {
					this->gisview->coord_to_screen_pos(tp->coord, curr_pos);

					if (!do_highlight && (this->track_drawing_mode == LayerTRWTrackDrawingMode::BySpeed)) {
						main_pen = this->track_pens[(int) speed_coloring.get(tp, prev_tp)];
					}

					/* Draw only if current point has different coordinates than the previous one. */
					if (curr_pos.x() != prev_pos.x() || curr_pos.y() != prev_pos.y()) {
						this->gisview->draw_line(main_pen, prev_pos, curr_pos);
					}
				} else {
					/* Draw only if current point has different coordinates than the previous one. */
					if (curr_pos.x() != prev_pos.x() || curr_pos.y() != prev_pos.y()) {
						this->gisview->coord_to_screen_pos(prev_tp->coord, curr_pos);
						draw_utm_skip_insignia(this->gisview, main_pen, curr_pos.x(), curr_pos.y());
					}
				}
			}
			use_prev_pos = false;
		}
	}
}





void LayerTRWPainter::draw_track_bg_sub(Track * trk, bool do_highlight)
{
	QPen main_pen = this->track_bg_pen;

	if (do_highlight) {
		/* Let's keep constant color of background, but indicate
		   selection of track by making the background thicker. */

		int w = main_pen.width();
		if (w < 3) {
			main_pen.setWidth(w * 2);
		} else if (w < 6) {
			main_pen.setWidth(w * 1.5);
		} else {
			main_pen.setWidth(w * 1.2);
		}
	}

	auto iter = trk->trackpoints.begin();

	ScreenPos curr_pos;
	this->gisview->coord_to_screen_pos((*iter)->coord, curr_pos);

	ScreenPos prev_pos = curr_pos;
	bool use_prev_pos = true; /* prev_pos contains valid coordinates of previous point. */

	iter++; /* Because first Trackpoint has been drawn above. */

	for (; iter != trk->trackpoints.end(); iter++) {
		Trackpoint * tp = *iter;
		Trackpoint * prev_tp = (Trackpoint *) *std::prev(iter);

		/* See if in a different lat/lon 'quadrant' so don't draw massively long lines (presumably wrong way around the Earth).
		   Mainly to prevent wrong lines drawn when a track crosses the 180 degrees East-West longitude boundary
		   (since ViewportPixmap::draw_line() only copes with pixel value and has no concept of the globe). */
		if (this->vp_coord_mode == CoordMode::LatLon
		    && ((prev_tp->coord.lat_lon.lon < -90.0 && tp->coord.lat_lon.lon > 90.0)
			|| (prev_tp->coord.lat_lon.lon > 90.0 && tp->coord.lat_lon.lon < -90.0))) {

			use_prev_pos = false;
			continue;
		}


		/* TODO_LATER: compare this condition with condition in LayerTRWPainter::draw_waypoint_sub(). */

#ifdef K_OLD_IMPLEMENTATION
		if ((!this->vp_is_one_utm_zone && !this->lat_lon) /* UTM & zones; do everything. */
		    || (((!this->vp_is_one_utm_zone) || tp->coord.utm_zone == this->center->utm_zone) /* Only check zones if UTM & one_utm_zone. */
			&& tp->coord.east_west < this->coord_rightmost && tp->coord.east_west > this->coord_leftmost /* Both UTM and lat lon. */
			&& tp->coord.north_south > this->coord_bottommost && tp->coord.north_south < this->coord_topmost))
#endif


		const bool fits_into_viewport = this->coord_fits_in_viewport(tp->coord);


		if (fits_into_viewport) {
			this->gisview->coord_to_screen_pos(tp->coord, curr_pos);

			if (use_prev_pos && curr_pos == prev_pos) {
				/* Points are the same in display coordinates, don't
				   draw, skip drawing part. Notice that we do
				   this after drawing stops. */
				goto skip;
			}

			if (!tp->newsegment && this->draw_track_lines) {
				if (!use_prev_pos) {
					this->gisview->coord_to_screen_pos(prev_tp->coord, prev_pos);
				}
				this->gisview->draw_line(this->track_bg_pen, prev_pos, curr_pos);
			}
		skip:
			prev_pos = curr_pos;
			use_prev_pos = true;

		} else {
			if (use_prev_pos && this->draw_track_lines && !tp->newsegment) {
				if (this->trw->coord_mode != CoordMode::UTM || UTM::is_the_same_zone(tp->coord.utm, this->vp_center.utm)) {
					this->gisview->coord_to_screen_pos(tp->coord, curr_pos);

					/* Draw only if current point has different coordinates than the previous one. */
					if (curr_pos.x() != prev_pos.x() || curr_pos.y() != prev_pos.y()) {
						this->gisview->draw_line(main_pen, prev_pos, curr_pos);
					}
				} else {
					/* Draw only if current point has different coordinates than the previous one. */
					if (curr_pos.x() != prev_pos.x() || curr_pos.y() != prev_pos.y()) {
						this->gisview->coord_to_screen_pos(prev_tp->coord, curr_pos);
						draw_utm_skip_insignia(this->gisview, main_pen, curr_pos.x(), curr_pos.y());
					}
				}
			}
			use_prev_pos = false;
		}
	}
}




void LayerTRWPainter::draw_track(Track * trk, GisViewport * a_gisview, bool do_highlight)
{
	if (!trk->bbox.intersects_with(a_gisview->get_bbox())) {
#if 0
		qDebug() << SG_PREFIX_D << "Track bbox:" << trk->bbox;
		qDebug() << SG_PREFIX_D << "GisViewport bbox:" << a_gisview->get_bbox();
#endif
		return;
	}

	if (!trk->is_visible()) {
		return;
	}

	if (trk->empty()) {
		return;
	}

	if (trk != this->trw->selected_track_get()) { /* Don't draw background of a track that is currently being created. */
		this->draw_track_bg_sub(trk, do_highlight);
	}
	this->draw_track_fg_sub(trk, do_highlight);

	/* Labels drawn at the end, so the labels are on top. */
	if (this->draw_track_labels) {
		if (trk->max_number_dist_labels > 0) {
			this->draw_track_dist_labels(trk, do_highlight);
		}
		this->draw_track_point_names(trk, do_highlight);

		if (trk->draw_name_mode != TrackDrawNameMode::None) {
			this->draw_track_name_labels(trk, do_highlight);
		}
	}
}




void LayerTRWPainter::draw_waypoint_sub(Waypoint * wp, bool do_highlight)
{
	const bool cond = (this->vp_coord_mode == CoordMode::UTM && !this->vp_is_one_utm_zone)
		|| ((this->vp_coord_mode == CoordMode::LatLon || UTM::is_the_same_zone(wp->get_coord().utm, this->vp_center.utm)) &&
		    this->coord_fits_in_viewport(wp->get_coord()));


	if (!cond) {
		return;
	}

	ScreenPos wp_screen_pos;
	this->gisview->coord_to_screen_pos(wp->get_coord(), wp_screen_pos);

	if (this->draw_wp_images && !wp->image_full_path.isEmpty()) {
		if (this->draw_waypoint_image(wp, wp_screen_pos, do_highlight)) {
			return;
		}
		/* Fall back to drawing a symbol (icon or geometric shape). */
	}

	/* Draw appropriate symbol - either icon or graphical mark. */
	this->draw_waypoint_symbol(wp, wp_screen_pos, do_highlight);

	if (this->draw_wp_labels) {
		this->draw_waypoint_label(wp, wp_screen_pos, do_highlight);
	}
}




CachedPixmap LayerTRWPainter::generate_wp_cached_pixmap(const QString & image_full_path) const
{
	CachedPixmap cache_object;

	if (this->wp_image_size == PIXMAP_THUMB_SIZE) {
		/* What a coincidence! Perhaps the image has already been "thumbnailed"
		   and we can read it from thumbnails dir. */
		cache_object = CachedPixmap(Thumbnails::get_thumbnail(image_full_path), image_full_path);
	}

	if (!cache_object.is_valid()) {
		/* We didn't manage to read the file from thumbnails file.
		   Either because the expected pixmap size (painter->wp_image_size)
		   is not equal to thumbnail size, or because there was no thumbnail on disc. */

		QPixmap original_image;
		if (original_image.load(image_full_path)) {
			cache_object = CachedPixmap(Thumbnails::scale_pixmap(original_image, this->wp_image_size, this->wp_image_size),
						    image_full_path);
		}
	}

	if (!cache_object.is_valid()) {
		/* Last resort. TODO_MAYBE: default thumbnail should be
		   somehow shared object. We don't want too many
		   copies of the default thumbnail in memory. */
		cache_object = CachedPixmap(Thumbnails::get_default_thumbnail(), "");
	}

	return cache_object;
}




/**
   @brief Draw waypoint's image

   Draw waypoint's image specified by wp->image.

   @wp_pos - position of waypoint on screen

   @return true if image has been drawn
   @return false if image has not been drawn for whatever reason (due to error or due to non-error situation)
*/
bool LayerTRWPainter::draw_waypoint_image(Waypoint * wp, const ScreenPos & wp_pos, bool do_highlight)
{
	if (this->wp_image_alpha == 0) {
		return false;
	}

	QPixmap pixmap;

	auto iter = std::find_if(this->trw->wp_image_cache.begin(), this->trw->wp_image_cache.end(), CachedPixmapCompareByPath(wp->image_full_path));
	if (iter != this->trw->wp_image_cache.end()) {
		/* Found a matching pixmap in cache. */
		pixmap = (*iter).pixmap;
	} else {
		/* WP Image Cache miss. */
		qDebug() << SG_PREFIX_I << "Waypoint image" << wp->image_full_path << "not found in cache, generating new cached image";

		CachedPixmap cache_object = this->generate_wp_cached_pixmap(wp->image_full_path);

		if (!cache_object.pixmap.isNull()) {
			/* Apply alpha setting to the image before the pixmap gets stored in the cache. */
			if (this->wp_image_alpha <= 255) {
				ui_pixmap_set_alpha(cache_object.pixmap, this->wp_image_alpha);
			}
			this->trw->wp_image_cache_add(cache_object);

			pixmap = cache_object.pixmap;
		}
	}

	if (pixmap.isNull()) {
		qDebug() << SG_PREFIX_E << "Failed to get wp pixmap from any source";
		return false;
	}

	const int w = pixmap.width();
	const int h = pixmap.height();
	const fpixel x = wp_pos.x();
	const fpixel y = wp_pos.y();

	bool centered = true;
	QRect target_rect;
	if (centered) {
		target_rect = QRect(x - (w / 2), y - (h / 2), w, h);
	} else {
		target_rect = QRect(x, y, w, h);
	}

	/* Draw only those waypoints that are visible in viewport. */
	if (!this->vp_rect.intersects(target_rect)) {
		return false;
	}


	wp->drawn_image_rect = target_rect;
	if (do_highlight) {
		/* Highlighted - so draw a little border around selected waypoint's image.
		   Single width seems a little weak so draw 2 times wider. */
		QPen pen = this->gisview->get_highlight_pen();
		const int pen_width = pen.width() * 2;
		const int delta = pen_width / 2;
		pen.setWidth(pen_width);

		this->gisview->draw_rectangle(pen, wp->drawn_image_rect.adjusted(-delta, -delta, delta, delta));
	}
	this->gisview->draw_pixmap(pixmap, wp->drawn_image_rect, QRect(0, 0, w, h));

	return true;
}




/**
   @brief Draw waypoint's symbol

   @wp_pos - position of waypoint on screen
*/
void LayerTRWPainter::draw_waypoint_symbol(Waypoint * wp, const ScreenPos & wp_pos, bool do_highlight)
{
	const fpixel x = wp_pos.x();
	const fpixel y = wp_pos.y();

	if (this->draw_wp_symbols && !wp->symbol_name.isEmpty() && wp->symbol_pixmap) {
		const fpixel viewport_x = x - wp->symbol_pixmap->width() / 2.0;
		const fpixel viewport_y = y - wp->symbol_pixmap->height() / 2.0;
		this->gisview->draw_pixmap(*wp->symbol_pixmap, viewport_x, viewport_y);

	} else {
		/* size - square's width (height), circle's diameter. */
		int size = do_highlight ? this->wp_marker_size * 2 : this->wp_marker_size;
		const QPen & pen = this->wp_marker_pen;

		switch (this->wp_marker_type) {
		case GraphicMarker::FilledSquare:
			this->gisview->fill_rectangle(pen.color(), x - size / 2, y - size / 2, size, size);
			break;
		case GraphicMarker::Square:
			this->gisview->draw_rectangle(pen, x - size / 2, y - size / 2, size, size);
			break;
		case GraphicMarker::Circle:
			size = 50;
			this->gisview->fill_ellipse(pen.color(), wp_pos, size / 2, size / 2);
			break;
		case GraphicMarker::X:
			/* x-markers need additional division of size by two. */
			size /= 2;
			this->gisview->draw_line(pen, x - size, y - size, x + size, y + size);
			this->gisview->draw_line(pen, x - size, y + size, x + size, y - size);
		default:
			break;
		}
	}

	return;
}




/**
   @brief Draw waypoint's label

   @wp_pos - position of waypoint on screen
*/
void LayerTRWPainter::draw_waypoint_label(Waypoint * wp, const ScreenPos & wp_pos, bool do_highlight)
{
	/* Could this be stored in the waypoint rather than recreating each pass? */

	const fpixel label_x = wp_pos.x();
	const fpixel label_y = wp_pos.y();
	int label_width = 100;
	int label_height = 50;

	if (do_highlight) {

		/* Draw waypoint's label with highlight background color. */

		/* +3/-3: we don't want the background of text overlap too much with symbol of waypoint. */
		QRectF bounding_rect(label_x + 3, label_y - 3, 300, -30);
		this->gisview->draw_text(QFont("Arial", pango_font_size_to_point_font_size(this->wp_label_font_size)),
					 this->wp_label_fg_pen,
					 this->gisview->get_highlight_pen().color(),
					 bounding_rect,
					 Qt::AlignBottom | Qt::AlignLeft,
					 wp->get_name());
	} else {
		/* Draw waypoint's label with regular background color. */
		this->gisview->draw_text(QFont("Arial", pango_font_size_to_point_font_size(this->wp_label_font_size)),
					 this->wp_label_fg_pen,
					 label_x,
					 label_y,
					 wp->get_name());
	}

	return;
}




void LayerTRWPainter::draw_waypoint(Waypoint * wp, GisViewport * a_gisview, bool do_highlight)
{
	wp->drawn_image_rect = QRect(); /* Null-ify/invalidate. */

	if (!wp->is_visible()) {
		return;
	}

	if (this->trw->get_waypoints_node().get_bbox().intersects_with(a_gisview->get_bbox())) {
		this->draw_waypoint_sub(wp, do_highlight);
	}
}




CachedPixmap::~CachedPixmap()
{

}




int pango_font_size_to_point_font_size(font_size_t font_size)
{
	int result;

	switch (font_size) {
	case FS_XX_SMALL:
		result = 5;
		break;
	case FS_X_SMALL:
		result = 6;
		break;
	case FS_SMALL:
		result = 8;
		break;
	case FS_LARGE:
		result = 12;
		break;
	case FS_X_LARGE:
		result = 14;
		break;
	case FS_XX_LARGE:
		result = 16;
		break;
	default:
		result = 10;
		break;
	}

	return result;
}




void LayerTRWPainter::make_track_pens(void)
{
	const int width = this->track_thickness;


	this->track_bg_pen = QPen(this->track_bg_color);
	this->track_bg_pen.setWidth(width + this->track_bg_thickness);


	/* Ensure new track drawing heeds line thickness setting,
	   however always have a minium of 2, as 1 pixel is really narrow. */
	int new_track_width = (this->track_thickness < 2) ? 2 : this->track_thickness;
	this->m_selected_track_pen = QPen(QColor("#FF0000"));
	this->m_selected_track_pen.setWidth(new_track_width);
	this->m_selected_track_pen.setCapStyle(Qt::RoundCap);
	this->m_selected_track_pen.setJoinStyle(Qt::RoundJoin);
	this->m_selected_track_pen.setStyle(Qt::DashLine);

	/* 'new_point' pen is exactly the same as the current track pen. */
	this->m_selected_track_new_point_pen = QPen(QColor("#FF0000"));
	this->m_selected_track_new_point_pen.setWidth(new_track_width);
	this->m_selected_track_new_point_pen.setCapStyle(Qt::RoundCap);
	this->m_selected_track_new_point_pen.setJoinStyle(Qt::RoundJoin);
	this->m_selected_track_new_point_pen.setStyle(Qt::DashLine);

	this->track_pens.clear();
	this->track_pens.resize((int) LayerTRWTrackGraphics::Max);

	this->track_pens[(int) LayerTRWTrackGraphics::Speed1] = QPen(QColor("#E6202E")); /* Red-ish. */
	this->track_pens[(int) LayerTRWTrackGraphics::Speed1].setWidth(width);

	this->track_pens[(int) LayerTRWTrackGraphics::Speed2] = QPen(QColor("#D2CD26")); /* Yellow-ish. */
	this->track_pens[(int) LayerTRWTrackGraphics::Speed2].setWidth(width);

	this->track_pens[(int) LayerTRWTrackGraphics::Speed3] = QPen(QColor("#2B8700")); /* Green-ish. */
	this->track_pens[(int) LayerTRWTrackGraphics::Speed3].setWidth(width);

	this->track_pens[(int) LayerTRWTrackGraphics::StopPen] = QPen(QColor("#874200"));
	this->track_pens[(int) LayerTRWTrackGraphics::StopPen].setWidth(width);

	this->track_pens[(int) LayerTRWTrackGraphics::SinglePen] = QPen(QColor(this->track_color_common));
	this->track_pens[(int) LayerTRWTrackGraphics::SinglePen].setWidth(width);

	this->track_pens[(int) LayerTRWTrackGraphics::NeutralPen] = QPen(QColor("#000000")); /* Black. */
	this->track_pens[(int) LayerTRWTrackGraphics::NeutralPen].setWidth(width);
}




void LayerTRWPainter::make_wp_pens(void)
{
	this->wp_marker_pen = QPen(this->wp_marker_color);
	this->wp_marker_pen.setWidth(2);

	this->wp_label_fg_pen = QPen(this->wp_label_fg_color);
	this->wp_label_fg_pen.setWidth(1);

	this->wp_label_bg_pen = QPen(this->wp_label_bg_color);
	this->wp_label_bg_pen.setWidth(1);

#ifdef K_FIXME_RESTORE
	gdk_gc_set_function(this->waypoint_bg_gc, this->wpbgand);
#endif
	return;
}




inline bool LayerTRWPainter::coord_fits_in_viewport(const Coord & coord) const
{
	bool fits_horizontally = false;
	bool fits_vertically = false;

	switch (this->vp_coord_mode) {
	case CoordMode::UTM:
		fits_horizontally = coord.utm.get_easting() < this->coord_rightmost && coord.utm.get_easting() > this->coord_leftmost;
		fits_vertically = coord.utm.get_northing() > this->coord_bottommost && coord.utm.get_northing() < this->coord_topmost;
		break;
	case CoordMode::LatLon:
		fits_horizontally = coord.lat_lon.lon < this->coord_rightmost && coord.lat_lon.lon > this->coord_leftmost;
		fits_vertically = coord.lat_lon.lat > this->coord_bottommost && coord.lat_lon.lat < this->coord_topmost;
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected viewport coordinate mode" << this->vp_coord_mode;
		break;
	}

	return fits_horizontally && fits_vertically;
}
