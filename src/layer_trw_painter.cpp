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




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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
#define PREFIX " Layer TRW Painter:" << __FUNCTION__ << __LINE__ << ">"




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




void LayerTRWPainter::set_viewport(Viewport * new_viewport)
{
	this->viewport = new_viewport;

	this->vp_xmpp = this->viewport->get_viking_zoom_level().get_x();
	this->vp_ympp = this->viewport->get_viking_zoom_level().get_y();

	this->vp_width = this->viewport->get_width();
	this->vp_height = this->viewport->get_height();
	this->vp_center = this->viewport->get_center2();
	this->vp_coord_mode = this->viewport->get_coord_mode();
	this->vp_is_one_utm_zone = this->viewport->get_is_one_utm_zone(); /* False if some other projection besides UTM. */

	this->cosine_factor = this->draw_track_directions_size * cos(DEG2RAD(45)); /* Calculate once per trw update - even if not used. */
	this->sine_factor = this->draw_track_directions_size * sin(DEG2RAD(45)); /* Calculate once per trw update - even if not used. */

	if (this->vp_coord_mode == CoordMode::UTM && this->vp_is_one_utm_zone) {

		const int outside_margin = 1600; /* TODO_LATER: magic number. */

		const int width = this->vp_xmpp * (this->vp_width / 2) + outside_margin / this->vp_xmpp;
		const int height = this->vp_ympp * (this->vp_height / 2) + outside_margin / this->vp_ympp;
		/* Leniency -- for tracks. Obviously for waypoints this SHOULD be a lot smaller. */

		this->coord_leftmost = this->vp_center.utm.easting - width;
		this->coord_rightmost = this->vp_center.utm.easting + width;
		this->coord_bottommost = this->vp_center.utm.northing - height;
		this->coord_topmost = this->vp_center.utm.northing + height;

	} else if (this->vp_coord_mode == CoordMode::LATLON) {

		/* Quick & dirty calculation; really want to check all corners due to lat/lon smaller at top in northern hemisphere. */
		/* This also DOESN'T WORK if you are crossing 180/-180 lon. I don't plan to in the near future... */

		const int outside_margin = 500; /* TODO_LATER: magic number. */

		const Coord upperleft = this->viewport->screen_pos_to_coord(-outside_margin, -outside_margin);
		const Coord bottomright = this->viewport->screen_pos_to_coord(this->vp_width + outside_margin, this->vp_height + outside_margin);

		this->coord_leftmost = upperleft.ll.lon;
		this->coord_rightmost = bottomright.ll.lon;
		this->coord_bottommost = bottomright.ll.lat;
		this->coord_topmost = upperleft.ll.lat;
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
	SpeedColoring(double low, double average, double high) : low_speed(low), average_speed(average), high_speed(high) {};
	SpeedColoring() {};
	void set(double low, double average, double high) { low_speed = low; average_speed = average; high_speed = high; };
	LayerTRWTrackGraphics get(const Trackpoint * tp1, const Trackpoint * tp2);
private:
	double low_speed = 0.0;
	double average_speed = 0.0;
	double high_speed = 0.0;
};




LayerTRWTrackGraphics SpeedColoring::get(const Trackpoint * tp1, const Trackpoint * tp2)
{
	if (!tp1->has_timestamp || !tp2->has_timestamp) {
		return LayerTRWTrackGraphics::NeutralPen;
	}
	if (average_speed <= 0) {
		return LayerTRWTrackGraphics::NeutralPen;
	}

	const double speed = (Coord::distance(tp1->coord, tp2->coord) / (tp1->timestamp - tp2->timestamp));
	if (speed < low_speed) {
		return LayerTRWTrackGraphics::Speed1;
	} else if (speed > high_speed) {
		return LayerTRWTrackGraphics::Speed3;
	} else {
		return LayerTRWTrackGraphics::Speed2;
	}
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




void LayerTRWPainter::draw_track_label(const QString & text, const QColor & fg_color, const QColor & bg_color, const Coord & coord)
{
	const ScreenPos label_pos = this->viewport->coord_to_screen_pos(coord);

	QPen pen;
	pen.setColor(fg_color);
	this->viewport->draw_text(QFont("Helvetica", pango_font_size_to_point_font_size(this->track_label_font_size)),
				  pen,
				  label_pos.x,
				  label_pos.y,
				  text);
}




/**
 * Draw a few labels along a track at nicely seperated distances.
 * This might slow things down if there's many tracks being displayed with this on.
 */
void LayerTRWPainter::draw_track_dist_labels(Track * trk, bool do_highlight)
{
	const DistanceUnit user_distance_unit = Preferences::get_unit_distance();
	const Distance track_length = trk->get_length_including_gaps().convert_to_unit(user_distance_unit);

	const int n_intervals_max = trk->max_number_dist_labels;
	GraphIntervalsDistance intervals;
	const int interval_idx = intervals.intervals.get_interval_index(0, track_length.value, n_intervals_max);

	const double interval = intervals.intervals.get_interval_value(interval_idx);

	for (int i = 1; i <= n_intervals_max; i++) {
		const Distance dist_in_user_units(interval * i, user_distance_unit);

		/* Convert distance back into metres for use in finding a trackpoint. */
		const Distance dist_in_meters = dist_in_user_units.convert_to_unit(SupplementaryDistanceUnit::Meters);
		if (!dist_in_meters.is_valid()) {
			qDebug() << SG_PREFIX_E << "Conversion to meters failed";
			break;
		}

		double dist_current = 0.0;
		Trackpoint * tp_current = trk->get_tp_by_dist(dist_in_meters.value, false, &dist_current);
		double dist_next = 0.0;
		Trackpoint * tp_next = trk->get_tp_by_dist(dist_in_meters.value, true, &dist_next);

		double dist_between_tps = fabs(dist_next - dist_current);
		double ratio = 0.0;
		/* Prevent division by 0 errors. */
		if (dist_between_tps > 0.0) {
			ratio = fabs(dist_in_meters.value - dist_current) / dist_between_tps;
		}

		if (tp_current && tp_next) {

			const LatLon ll_current = tp_current->coord.get_latlon();
			const LatLon ll_next = tp_next->coord.get_latlon();

			/* Positional interpolation.
			   Using a simple ratio - may not be perfectly correct due to lat/lon projections
			   but should be good enough over the small scale that I anticipate usage on. */
			const LatLon ll_middle(ll_current.lat + (ll_next.lat - ll_current.lat) * ratio,
					       ll_current.lon + (ll_next.lon - ll_current.lon) * ratio);
			const Coord coord_middle(ll_middle, this->trw->coord_mode);

			const QColor fg_color = this->get_fg_color(trk);
			const QColor bg_color = this->get_bg_color(do_highlight);

			this->draw_track_label(dist_in_user_units.to_nice_string(), fg_color, bg_color, coord_middle);
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
	return do_highlight ? this->viewport->get_highlight_color() : this->track_bg_color;
}




/**
 * Draw a label (or labels) for the track name somewhere depending on the track's properties.
 */
void LayerTRWPainter::draw_track_name_labels(Track * trk, bool do_highlight)
{
	const QColor fg_color = this->get_fg_color(trk);
	const QColor bg_color = this->get_bg_color(do_highlight);

	char *ename = g_markup_escape_text(trk->name.toUtf8().constData(), -1);

	if (trk->draw_name_mode == TrackDrawNameMode::StartCentreEnd ||
	    trk->draw_name_mode == TrackDrawNameMode::Centre) {

		const Coord coord(trk->get_bbox().get_center(), this->trw->coord_mode);
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
			const ScreenPos begin_pos = this->viewport->coord_to_screen_pos(begin_coord);
			const ScreenPos end_pos = this->viewport->coord_to_screen_pos(end_coord);
			const Coord av_coord = this->viewport->screen_pos_to_coord(ScreenPos::get_average(begin_pos, end_pos));

			QString name = QObject::tr("%1: %2").arg(ename).arg(QObject::tr("start/end"));
			this->draw_track_label(name, fg_color, bg_color, av_coord);

			done_start_end = true;
		}
	}

	if (!done_start_end) {
		if (trk->draw_name_mode == TrackDrawNameMode::Start
		    || trk->draw_name_mode == TrackDrawNameMode::StartEnd
		    || trk->draw_name_mode == TrackDrawNameMode::StartCentreEnd) {

			const QString name_start = QObject::tr("%1: %2").arg(ename).arg(QObject::tr("start"));
			this->draw_track_label(name_start, fg_color, bg_color, begin_coord);
		}
		/* Don't draw end label if this is the one being created. */
		if (trk != this->trw->get_edited_track()) {
			if (trk->draw_name_mode == TrackDrawNameMode::End
			    || trk->draw_name_mode == TrackDrawNameMode::StartEnd
			    || trk->draw_name_mode == TrackDrawNameMode::StartCentreEnd) {

				const QString name_end = QObject::tr("%1: %2").arg(ename).arg(QObject::tr("end"));
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
	const int midx = (begin.x + end.x) / 2;
	const int midy = (begin.y + end.y) / 2;

	const double len = sqrt(((midx - begin.x) * (midx - begin.x)) + ((midy - begin.y) * (midy - begin.y)));
	/* Avoid divide by zero and ensure at least 1 pixel big. */
	if (len > 1) {
		const double dx = (begin.x - midx) / len;
		const double dy = (begin.y - midy) / len;
		this->viewport->draw_line(pen, midx, midy, midx + (dx * this->cosine_factor + dy * this->sine_factor), midy + (dy * this->cosine_factor - dx * this->sine_factor));
		this->viewport->draw_line(pen, midx, midy, midx + (dx * this->cosine_factor - dy * this->sine_factor), midy + (dy * this->cosine_factor + dx * this->sine_factor));
	}
}




void LayerTRWPainter::draw_track_draw_something(const ScreenPos & begin, const ScreenPos & end, QPen & pen, Trackpoint * tp, Trackpoint * tp_next, double min_alt, double alt_diff)
{
#define FIXALTITUDE(what) \
	((((Trackpoint *) (what))->altitude - min_alt) / alt_diff * DRAW_ELEVATION_FACTOR * this->track_elevation_factor / this->vp_xmpp)


	QPoint points[4];

	points[0] = QPoint(begin.x, begin.y);
	points[1] = QPoint(begin.x, begin.y - FIXALTITUDE (tp));
	points[2] = QPoint(end.x, end.y - FIXALTITUDE (tp_next));
	points[3] = QPoint(end.x, end.y);

	QPen tmp_pen;
#ifdef K_FIXME_RESTORE
	if (((begin.x - x) > 0 && (begin.y - y) > 0) || ((begin.x - x) < 0 && (begin.y - y) < 0)) {
		tmp_pen = gtk_widget_get_style(this->viewport)->light_gc[3];
	} else {
		tmp_pen = gtk_widget_get_style(this->viewport)->dark_gc[0];
	}
#else
	tmp_pen.setColor("green");
	tmp_pen.setWidth(1);
#endif
	this->viewport->draw_polygon(tmp_pen, points, 4, true);

	this->viewport->draw_line(pen, begin.x, begin.y - FIXALTITUDE (tp), end.x, end.y - FIXALTITUDE (tp_next));
}




QPen LayerTRWPainter::get_track_fg_pen(Track * trk, bool do_highlight)
{
	QPen result;

	if (trk == this->trw->get_edited_track()) {
		/* The track is being created by user, it gets a special pen. */
		result = this->current_track_pen;
	} else if (do_highlight) {
		/* Draw all tracks of the layer in 'highlight' color.
		   This supersedes the this->track_drawing_mode. */
		result = this->viewport->get_highlight_pen();
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
	double alt_diff = 0;
	if (this->draw_track_elevation) {
		/* Assume if it has elevation at the beginning, it has it throughout. not ness a true good assumption. */
		if (trk->get_minmax_alt(min_alt, max_alt)) {
			alt_diff = max_alt.get_value() - min_alt.get_value();
		}
	}

	const bool do_draw_trackpoints = this->draw_trackpoints;
	const bool do_draw_track_stops = do_highlight ? false : this->draw_track_stops;


#if 1   /* Temporary test code. */
	this->draw_track_label("test track label", QColor("green"), QColor("black"), this->viewport->get_center2());
#endif


	QPen main_pen = this->get_track_fg_pen(trk, do_highlight);
	/* If track_drawing_mode == LayerTRWTrackDrawingMode::BySpeed, main_pen may be overwritten below. */



	const int tp_size_reg = this->trackpoint_size;
	const int tp_size_cur = this->trackpoint_size * 2;

	auto iter = trk->trackpoints.begin();

	Track * selected_track = this->trw->get_edited_track();
	int tp_size = (selected_track && selected_track->selected_tp_iter.valid && *iter == *selected_track->selected_tp_iter.iter) ? tp_size_cur : tp_size_reg;

	ScreenPos curr_pos = this->viewport->coord_to_screen_pos((*iter)->coord);

	/* Draw the first point as something a bit different from the normal points.
	   ATM it's slightly bigger and a triangle. */

	if (do_draw_trackpoints) {
		QPoint trian[3] = { QPoint(curr_pos.x, curr_pos.y-(3*tp_size)), QPoint(curr_pos.x-(2*tp_size), curr_pos.y+(2*tp_size)), QPoint(curr_pos.x+(2*tp_size), curr_pos.y+(2*tp_size)) };
		this->viewport->draw_polygon(main_pen, trian, 3, true);
	}


	SpeedColoring speed_coloring;
	/* If necessary calculate these values - which is done only once per track redraw. */
	if (this->track_drawing_mode == LayerTRWTrackDrawingMode::BySpeed) {
		/* The percentage factor away from the average speed determines transistions between the levels.
		   TODO_LATER: what if average speed is invalid? */
		const double average_speed = trk->get_average_speed_moving(this->track_min_stop_length).get_value();
		const double low_speed = average_speed - (average_speed * (this->track_draw_speed_factor/100.0));
		const double high_speed = average_speed + (average_speed * (this->track_draw_speed_factor/100.0));
		speed_coloring.set(low_speed, average_speed, high_speed);
	}

	ScreenPos prev_pos = curr_pos;
	bool use_prev_pos = true; /* prev_pos contains valid coordinates of previous point. */

	iter++; /* Because first Trackpoint has been drawn above. */

	for (; iter != trk->trackpoints.end(); iter++) {
		Trackpoint * tp = *iter;

		tp_size = (selected_track && selected_track->selected_tp_iter.valid && tp == *selected_track->selected_tp_iter.iter) ? tp_size_cur : tp_size_reg;

		Trackpoint * prev_tp = (Trackpoint *) *std::prev(iter);

		/* See if in a different lat/lon 'quadrant' so don't draw massively long lines (presumably wrong way around the Earth).
		   Mainly to prevent wrong lines drawn when a track crosses the 180 degrees East-West longitude boundary
		   (since Viewport::draw_line() only copes with pixel value and has no concept of the globe). */
		if (this->vp_coord_mode == CoordMode::LATLON
		    && ((prev_tp->coord.ll.lon < -90.0 && tp->coord.ll.lon > 90.0)
			|| (prev_tp->coord.ll.lon > 90.0 && tp->coord.ll.lon < -90.0))) {

			use_prev_pos = false;
			continue;
		}

		/* Check some stuff -- but only if we're in UTM and there's only ONE ZONE; or lat lon. */

		/* TODO_LATER: compare this condition with condition in LayerTRWPainter::draw_waypoint_sub(). */
		bool first_condition = (this->vp_coord_mode == CoordMode::UTM && !this->vp_is_one_utm_zone); /* UTM coord mode & more than one UTM zone - do everything. */

		bool second_condition_A = ((!this->vp_is_one_utm_zone) || tp->coord.utm.zone == this->vp_center.utm.zone);  /* Only check zones if UTM & one_utm_zone. */

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

			curr_pos = this->viewport->coord_to_screen_pos(tp->coord);

			/* The concept of drawing stops is that if the next trackpoint has a
			   timestamp far into the future, we draw a circle of 6x trackpoint
			   size, instead of a rectangle of 2x trackpoint size. Stop is drawn
			   first so the trackpoint will be drawn on top. */
			if (do_draw_track_stops
			    && do_draw_trackpoints
			    && !do_highlight
			    && std::next(iter) != trk->trackpoints.end()
			    && (*std::next(iter))->timestamp - (*iter)->timestamp > this->track_min_stop_length) {

				const int stop_radius = (6 * tp_size) / 2;
				this->viewport->draw_ellipse(this->track_pens[(int) LayerTRWTrackGraphics::StopPen],
							     QPoint(curr_pos.x, curr_pos.y),
							     stop_radius, stop_radius, true);
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
					this->viewport->fill_rectangle(main_pen.color(), curr_pos.x-tp_size, curr_pos.y-tp_size, 2*tp_size, 2*tp_size);
				} else {
					/* Final point - draw 4x circle. */
					const int tp_radius = (4 * tp_size) / 2;
					this->viewport->draw_ellipse(main_pen, QPoint(curr_pos.x, curr_pos.y), tp_radius, tp_radius, true);
				}
			}

			if (!tp->newsegment && this->draw_track_lines) {

				/* UTM only: zone check. */
				if (do_draw_trackpoints && this->trw->coord_mode == CoordMode::UTM && tp->coord.utm.zone != this->vp_center.utm.zone) {
					draw_utm_skip_insignia(this->viewport, main_pen, curr_pos.x, curr_pos.y);
				}

				if (!use_prev_pos) {
					prev_pos = this->viewport->coord_to_screen_pos(prev_tp->coord);
				}

				this->viewport->draw_line(main_pen, prev_pos.x, prev_pos.y, curr_pos.x, curr_pos.y);

				if (this->draw_track_elevation
				    && std::next(iter) != trk->trackpoints.end()
				    && (*std::next(iter))->altitude != VIK_DEFAULT_ALTITUDE) {

					this->draw_track_draw_something(prev_pos, curr_pos, main_pen, *iter, *std::next(iter), min_alt.get_value(), alt_diff);
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
				if (this->trw->coord_mode != CoordMode::UTM || tp->coord.utm.zone == this->vp_center.utm.zone) {
					curr_pos = this->viewport->coord_to_screen_pos(tp->coord);

					if (!do_highlight && (this->track_drawing_mode == LayerTRWTrackDrawingMode::BySpeed)) {
						main_pen = this->track_pens[(int) speed_coloring.get(tp, prev_tp)];
					}

					/* Draw only if current point has different coordinates than the previous one. */
					if (curr_pos.x != prev_pos.x || curr_pos.y != prev_pos.y) {
						this->viewport->draw_line(main_pen, prev_pos.x, prev_pos.y, curr_pos.x, curr_pos.y);
					}
				} else {
					/* Draw only if current point has different coordinates than the previous one. */
					if (curr_pos.x != prev_pos.x && curr_pos.y != prev_pos.y) { /* kamilFIXME: is && a correct condition? */
						curr_pos = this->viewport->coord_to_screen_pos(prev_tp->coord);
						draw_utm_skip_insignia(this->viewport, main_pen, curr_pos.x, curr_pos.y);
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

	ScreenPos curr_pos = this->viewport->coord_to_screen_pos((*iter)->coord);
	ScreenPos prev_pos = curr_pos;
	bool use_prev_pos = true; /* prev_pos contains valid coordinates of previous point. */

	iter++; /* Because first Trackpoint has been drawn above. */

	for (; iter != trk->trackpoints.end(); iter++) {
		Trackpoint * tp = *iter;
		Trackpoint * prev_tp = (Trackpoint *) *std::prev(iter);

		/* See if in a different lat/lon 'quadrant' so don't draw massively long lines (presumably wrong way around the Earth).
		   Mainly to prevent wrong lines drawn when a track crosses the 180 degrees East-West longitude boundary
		   (since Viewport::draw_line() only copes with pixel value and has no concept of the globe). */
		if (this->vp_coord_mode == CoordMode::LATLON
		    && ((prev_tp->coord.ll.lon < -90.0 && tp->coord.ll.lon > 90.0)
			|| (prev_tp->coord.ll.lon > 90.0 && tp->coord.ll.lon < -90.0))) {

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
			curr_pos = this->viewport->coord_to_screen_pos(tp->coord);

			if (use_prev_pos && curr_pos == prev_pos) {
				/* Points are the same in display coordinates, don't
				   draw, skip drawing part. Notice that we do
				   this after drawing stops. */
				goto skip;
			}

			if (!tp->newsegment && this->draw_track_lines) {
				if (!use_prev_pos) {
					prev_pos = this->viewport->coord_to_screen_pos(prev_tp->coord);
				}
				this->viewport->draw_line(this->track_bg_pen, prev_pos.x, prev_pos.y, curr_pos.x, curr_pos.y);
			}
		skip:
			prev_pos = curr_pos;
			use_prev_pos = true;

		} else {
			if (use_prev_pos && this->draw_track_lines && !tp->newsegment) {
				if (this->trw->coord_mode != CoordMode::UTM || tp->coord.utm.zone == this->vp_center.utm.zone) {
					curr_pos = this->viewport->coord_to_screen_pos(tp->coord);

					/* Draw only if current point has different coordinates than the previous one. */
					if (curr_pos.x != prev_pos.x || curr_pos.y != prev_pos.y) {
						this->viewport->draw_line(main_pen, prev_pos.x, prev_pos.y, curr_pos.x, curr_pos.y);
					}
				} else {
					/* Draw only if current point has different coordinates than the previous one. */
					if (curr_pos.x != prev_pos.x && curr_pos.y != prev_pos.y) { /* kamilFIXME: is && a correct condition? */
						curr_pos = this->viewport->coord_to_screen_pos(prev_tp->coord);
						draw_utm_skip_insignia(this->viewport, main_pen, curr_pos.x, curr_pos.y);
					}
				}
			}
			use_prev_pos = false;
		}
	}
}




void LayerTRWPainter::draw_track(Track * trk, Viewport * a_viewport, bool do_highlight)
{
	if (!BBOX_INTERSECT (trk->bbox, a_viewport->get_bbox())) {
#if 0
		qDebug() << "DD" PREFIX << "track bbox:" << trk->bbox;
		qDebug() << "DD" PREFIX << "viewport bbox:" << a_viewport->get_bbox();
#endif
		return;
	}

	if (!trk->visible) {
		return;
	}

	if (trk->empty()) {
		return;
	}

	if (trk != this->trw->get_edited_track()) { /* Don't draw background of a track that is currently being created. */
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
		|| ((this->vp_coord_mode == CoordMode::LATLON || wp->coord.utm.zone == this->vp_center.utm.zone) &&
		    this->coord_fits_in_viewport(wp->coord));


	if (!cond) {
		return;
	}

	const ScreenPos wp_screen_pos = this->viewport->coord_to_screen_pos(wp->coord);

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

   @return true on success
   @return false on failure
*/
bool LayerTRWPainter::draw_waypoint_image(Waypoint * wp, const ScreenPos & pos, bool do_highlight)
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
		qDebug() << "II" PREFIX << "Waypoint image" << wp->image_full_path << "not found in cache, generating new cached image";

		CachedPixmap cache_object = this->generate_wp_cached_pixmap(wp->image_full_path);

		if (!cache_object.pixmap.isNull()) {
			/* Needed so 'click picture' tool knows how
			   big the pic is; we don't store it in
			   cache_object because they may have been
			   freed already. */
			wp->image_width = cache_object.pixmap.width();
			wp->image_height = cache_object.pixmap.height();

			/* Apply alpha setting to the image before the pixmap gets stored in the cache. */
			if (this->wp_image_alpha <= 255) {
				ui_pixmap_set_alpha(cache_object.pixmap, this->wp_image_alpha);
			}
			this->trw->wp_image_cache_add(cache_object);

			pixmap = cache_object.pixmap;
		}
	}

	if (pixmap.isNull()) {
		qDebug() << "EE" PREFIX << "failed to get wp pixmap from any source";
		return false;
	}

	const int w = pixmap.width();
	const int h = pixmap.height();
	const int x = pos.x;
	const int y = pos.y;

	const QRect target_rect(x - (w / 2), y - (h / 2), w, h);

	/* Always draw within boundaries. TODO_MAYBE: Replace this condition with "rectangles intersect" condition? */
	if (x + (w / 2) > 0 && y + (h / 2) > 0 && x - (w / 2) < this->vp_width && y - (h / 2) < this->vp_height) {
		if (do_highlight) {
			/* Highlighted - so draw a little border around selected waypoint's image.
			   Single width seems a little weak so draw 2 times wider. */
			QPen pen = this->viewport->get_highlight_pen();
			const int pen_width = pen.width() * 2;
			const int delta = pen_width / 2;
			pen.setWidth(pen_width);

			this->viewport->draw_rectangle(pen, target_rect.adjusted(-delta, -delta, delta, delta));
		}
		this->viewport->draw_pixmap(pixmap, target_rect, QRect(0, 0, w, h));
	}
	return true;
}




void LayerTRWPainter::draw_waypoint_symbol(Waypoint * wp, const ScreenPos & pos, bool do_highlight)
{
	const int x = pos.x;
	const int y = pos.y;

	if (this->draw_wp_symbols && !wp->symbol_name.isEmpty() && wp->symbol_pixmap) {
		const int viewport_x = x - wp->symbol_pixmap->width() / 2;
		const int viewport_y = y - wp->symbol_pixmap->height() / 2;
		this->viewport->draw_pixmap(*wp->symbol_pixmap, viewport_x, viewport_y);

	} else {
		/* size - square's width (height), circle's diameter. */
		int size = do_highlight ? this->wp_marker_size * 2 : this->wp_marker_size;
		const QPen & pen = this->wp_marker_pen;

		switch (this->wp_marker_type) {
		case GraphicMarker::FilledSquare:
			this->viewport->fill_rectangle(pen.color(), x - size / 2, y - size / 2, size, size);
			break;
		case GraphicMarker::Square:
			this->viewport->draw_rectangle(pen, x - size / 2, y - size / 2, size, size);
			break;
		case GraphicMarker::Circle:
			size = 50;
			this->viewport->draw_ellipse(pen, QPoint(pos.x, pos.y), size / 2, size / 2, true);
			break;
		case GraphicMarker::X:
			/* x-markers need additional division of size by two. */
			size /= 2;
			this->viewport->draw_line(pen, x - size, y - size, x + size, y + size);
			this->viewport->draw_line(pen, x - size, y + size, x + size, y - size);
		default:
			break;
		}
	}

	return;
}




void LayerTRWPainter::draw_waypoint_label(Waypoint * wp, const ScreenPos & pos, bool do_highlight)
{
	/* Could this be stored in the waypoint rather than recreating each pass? */

	const int label_x = pos.x;
	const int label_y = pos.y;
	int label_width = 100;
	int label_height = 50;
	this->wp_label_fg_pen = QPen(this->wp_label_fg_color);

	if (do_highlight) {

		/* Draw waypoint's label with highlight background color. */

		/* +3/-3: we don't want the background of text overlap too much with symbol of waypoint. */
		QRectF bounding_rect(label_x + 3, label_y - 3, 300, -30);
		this->viewport->draw_text(QFont("Arial", pango_font_size_to_point_font_size(this->wp_label_font_size)),
					  this->wp_label_fg_pen,
					  this->viewport->get_highlight_pen().color(),
					  bounding_rect,
					  Qt::AlignBottom | Qt::AlignLeft,
					  wp->name,
					  0);
	} else {
		/* Draw waypoint's label with regular background color. */
		this->viewport->draw_text(QFont("Arial", pango_font_size_to_point_font_size(this->wp_label_font_size)),
					  this->wp_label_fg_pen,
					  label_x,
					  label_y,
					  wp->name);
	}

	return;
}




void LayerTRWPainter::draw_waypoint(Waypoint * wp, Viewport * a_viewport, bool do_highlight)
{
	if (!wp->visible) {
		return;
	}

	if (BBOX_INTERSECT (this->trw->get_waypoints_node().get_bbox(), a_viewport->get_bbox())) {
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
	this->current_track_pen = QPen(QColor("#FF0000"));
	this->current_track_pen.setWidth(new_track_width);
	this->current_track_pen.setCapStyle(Qt::RoundCap);
	this->current_track_pen.setJoinStyle(Qt::RoundJoin);
	this->current_track_pen.setStyle(Qt::DashLine);

	/* 'new_point' pen is exactly the same as the current track pen. */
	this->current_track_new_point_pen = QPen(QColor("#FF0000"));
	this->current_track_new_point_pen.setWidth(new_track_width);
	this->current_track_new_point_pen.setCapStyle(Qt::RoundCap);
	this->current_track_new_point_pen.setJoinStyle(Qt::RoundJoin);
	this->current_track_new_point_pen.setStyle(Qt::DashLine);

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
		fits_horizontally = coord.utm.easting < this->coord_rightmost && coord.utm.easting > this->coord_leftmost;
		fits_vertically = coord.utm.northing > this->coord_bottommost && coord.utm.northing < this->coord_topmost;
		break;
	case CoordMode::LATLON:
		fits_horizontally = coord.ll.lon < this->coord_rightmost && coord.ll.lon > this->coord_leftmost;
		fits_vertically = coord.ll.lat > this->coord_bottommost && coord.ll.lat < this->coord_topmost;
		break;
	default:
		qDebug() << "EE:" PREFIX << "unexpected viewport coordinate mode" << (int) this->vp_coord_mode;
		break;
	}

	return fits_horizontally && fits_vertically;
}
