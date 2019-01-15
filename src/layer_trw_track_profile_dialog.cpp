/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2007, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2007-2008, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2012-2014, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2019, Kamil Ignacak <acerion@wp.pl>
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




#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cassert>




#include <QDebug>
#include <QPushButton>




#include "window.h"
#include "layer_trw.h"
#include "layer_trw_track_internal.h"
#include "layer_trw_track_profile_dialog.h"
#include "viewport_internal.h"
#include "dem_cache.h"
#include "vikutils.h"
#include "util.h"
#include "ui_util.h"
#include "dialog.h"
#include "application_state.h"
#include "preferences.h"
#include "measurements.h"
#include "graph_intervals.h"
#include "tree_view_internal.h"




using namespace SlavGPS;




#define SG_MODULE "Track Profile Dialog"




#define VIK_SETTINGS_TRACK_PROFILE_WIDTH "track_profile_display_width"
#define VIK_SETTINGS_TRACK_PROFILE_HEIGHT "track_profile_display_height"

#define VIK_SETTINGS_TRACK_PROFILE_ET_SHOW_DEM        "track_profile_et_show_dem"
#define VIK_SETTINGS_TRACK_PROFILE_ET_SHOW_SPEED      "track_profile_et_show_speed"
#define VIK_SETTINGS_TRACK_PROFILE_SD_SHOW_GPS_SPEED  "track_profile_sd_show_gps_speed"
#define VIK_SETTINGS_TRACK_PROFILE_ED_SHOW_DEM        "track_profile_ed_show_dem"
#define VIK_SETTINGS_TRACK_PROFILE_ED_SHOW_GPS_SPEED  "track_profile_ed_show_gps_speed"
#define VIK_SETTINGS_TRACK_PROFILE_GD_SHOW_GPS_SPEED  "track_profile_gd_show_gps_speed"
#define VIK_SETTINGS_TRACK_PROFILE_ST_SHOW_GPS_SPEED  "track_profile_st_show_gps_speed"
#define VIK_SETTINGS_TRACK_PROFILE_DT_SHOW_SPEED      "track_profile_dt_show_speed"




#define GRAPH_INITIAL_WIDTH 400
#define GRAPH_INITIAL_HEIGHT 300

#define GRAPH_MARGIN_LEFT 80 // 70
#define GRAPH_MARGIN_RIGHT 40 // 1
#define GRAPH_MARGIN_TOP 20
#define GRAPH_MARGIN_BOTTOM 30 // 1
#define GRAPH_X_INTERVALS 5
#define GRAPH_Y_INTERVALS 5




enum {
	SG_TRACK_PROFILE_CANCEL,
	SG_TRACK_PROFILE_SPLIT_AT_MARKER,
	SG_TRACK_PROFILE_OK,
};




static GraphIntervalsTime     time_intervals;
static GraphIntervalsDistance distance_intervals;
static GraphIntervalsAltitude altitude_intervals;
static GraphIntervalsGradient gradient_intervals;
static GraphIntervalsSpeed    speed_intervals;




static void time_label_update(QLabel * label, time_t seconds_from_start);
static void real_time_label_update(ProfileView * graph, const Trackpoint * tp);

static QString get_time_grid_label(const Time & interval_value, const Time & value);

static QString get_graph_title(void);




TrackProfileDialog::~TrackProfileDialog()
{
	for (auto iter = this->graphs.begin(); iter != this->graphs.end(); iter++) {
		delete *iter;
	}
}




sg_ret ProfileView::regenerate_data_from_scratch(Track * trk)
{
	/* First create track data using appropriate Track method. */

	const int w = this->viewport2d->central_get_width();

	if (this->viewport2d->y_domain == ViewportDomain::Elevation && this->viewport2d->x_domain == ViewportDomain::Distance) {
		this->track_data = trk->make_track_data_altitude_over_distance(w);

	} else if (this->viewport2d->y_domain == ViewportDomain::Gradient && this->viewport2d->x_domain == ViewportDomain::Distance) {
		this->track_data = trk->make_track_data_gradient_over_distance(w);

	} else if (this->viewport2d->y_domain == ViewportDomain::Speed && this->viewport2d->x_domain == ViewportDomain::Time) {
		this->track_data_raw = trk->make_track_data_speed_over_time();
		this->track_data = this->track_data_raw.compress(w);

	} else if (this->viewport2d->y_domain == ViewportDomain::Distance && this->viewport2d->x_domain == ViewportDomain::Time) {
		this->track_data_raw = trk->make_track_data_distance_over_time();
		this->track_data = this->track_data_raw.compress(w);

	} else if (this->viewport2d->y_domain == ViewportDomain::Elevation && this->viewport2d->x_domain == ViewportDomain::Time) {
		this->track_data_raw = trk->make_track_data_altitude_over_time();
		this->track_data = this->track_data_raw.compress(w);

	} else if (this->viewport2d->y_domain == ViewportDomain::Speed && this->viewport2d->x_domain == ViewportDomain::Distance) {
		this->track_data_raw = trk->make_track_data_speed_over_distance();
		this->track_data = this->track_data_raw.compress(w);
	} else {
		qDebug() << SG_PREFIX_E << "Unhandled x/y domain" << (int) this->viewport2d->x_domain << (int) this->viewport2d->y_domain;
	}

	if (!this->track_data.valid) {
		qDebug() << SG_PREFIX_E << "Value vector for" << this->get_graph_title() << "is invalid";
		return sg_ret::err;
	}
	qDebug() << SG_PREFIX_I << "Generated value vector for" << this->get_graph_title() << ", will now adjust y values";


	/* Do necessary adjustments to y values. */

	switch (this->viewport2d->y_domain) {
	case ViewportDomain::Speed:
		/* Convert into appropriate units. */
		for (int i = 0; i < w; i++) {
			this->track_data.y[i] = Speed::convert_mps_to(this->track_data.y[i], this->viewport2d->speed_unit);
		}

		qDebug() << SG_PREFIX_D << "Calculating min/max y speed for" << this->get_graph_title();
		this->track_data.calculate_min_max();
		if (this->track_data.y_min < 0.0) {
			this->track_data.y_min = 0; /* Splines sometimes give negative speeds. */
		}
		break;
	case ViewportDomain::Elevation:
		/* Convert into appropriate units. */
		if (this->viewport2d->height_unit == HeightUnit::Feet) {
			/* Convert altitudes into feet units. */
			for (int i = 0; i < w; i++) {
				this->track_data.y[i] = VIK_METERS_TO_FEET(this->track_data.y[i]);
			}
		}
		/* Otherwise leave in metres. */

		this->track_data.calculate_min_max();
		break;
	case ViewportDomain::Distance:
		/* Convert into appropriate units. */
		for (int i = 0; i < w; i++) {
			this->track_data.y[i] = Distance::convert_meters_to(this->track_data.y[i], this->viewport2d->distance_unit);
		}

#ifdef K_FIXME_RESTORE
		this->track_data.y_min = 0;
		this->track_data.y_max = Distance::convert_meters_to(trk->get_length_value_including_gaps(), this->viewport2d->distance_unit);
#endif

		this->track_data.calculate_min_max();
		break;
	case ViewportDomain::Gradient:
		/* No unit conversion needed. */
		this->track_data.calculate_min_max();
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled y domain" << (int) this->viewport2d->y_domain;
		return sg_ret::err;
	};



	qDebug() << SG_PREFIX_I << "After calling calculate_min_max; x_min/x_max =" << this->track_data.x_min << this->track_data.x_max << this->get_graph_title();



	/* Do necessary adjustments to x values. */
	sg_ret result = sg_ret::err;
	switch (this->viewport2d->x_domain) {
	case ViewportDomain::Distance:
		result = this->set_initial_visible_range_x_distance();
		break;
	case ViewportDomain::Time:
		result = this->set_initial_visible_range_x_time();
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled x domain" << (int) this->viewport2d->x_domain;
		result = sg_ret::err;
		break;
	}
	if (sg_ret::ok != result) {
		qDebug() << SG_PREFIX_E << "Failed to set initial visible x range";
		return result;
	}


	result = this->set_initial_visible_range_y();
	if (sg_ret::ok != result) {
		qDebug() << SG_PREFIX_E << "Failed to set initial visible y range";
		return result;
	}

	if (!this->track_data.valid || NULL == this->track_data.x || NULL == this->track_data.y) {
		qDebug() << SG_PREFIX_E << "Final test of track data: failure";
		return sg_ret::err;
	} else {
		qDebug() << SG_PREFIX_I << "Final test of track data: success";
		return sg_ret::ok;
	}
}




sg_ret ProfileView::set_initial_visible_range_x_distance(void)
{
	/* We won't display any x values outside of
	   track_data.x_min/max. We will never be able to zoom out to
	   show e.g. negative distances. */
	const Distance x_min = Distance(this->track_data.x_min);
	const Distance x_max = Distance(this->track_data.x_max);
	this->x_min_visible_d = x_min.convert_to_unit(this->viewport2d->distance_unit);
	this->x_max_visible_d = x_max.convert_to_unit(this->viewport2d->distance_unit);

	const Distance visible_range = this->x_max_visible_d - this->x_min_visible_d;
	if (visible_range.is_zero()) {
		qDebug() << SG_PREFIX_E << "Zero distance span: min/max = " << this->x_min_visible_d << this->x_max_visible_d;
		return sg_ret::err;
	}

	/* Now, given the n_intervals value, find a suitable interval
	   index and value that will nicely cover visible range of
	   data. */

	const int n_intervals = GRAPH_X_INTERVALS;

	int interval_index = distance_intervals.intervals.get_interval_index(this->x_min_visible_d, this->x_max_visible_d, n_intervals);
	this->x_interval_d = distance_intervals.intervals.values[interval_index];

	return sg_ret::ok;
}




sg_ret ProfileView::set_initial_visible_range_x_time(void)
{
	/* We won't display any x values outside of
	   track_data.x_min/max. We will never be able to zoom out to
	   show e.g. negative times. */
#if 0
	/* It's not a good idea to use track's min/max values as
	   min/max visible range.  I've seen track data with glitches
	   in timestamps, where in the middle of a track, in a row of
	   correctly incrementing timestamps there was suddenly one
	   smaller timestamp from a long time ago. */
	this->x_min_visible_t = this->track_data.x_min;
	this->x_max_visible_t = this->track_data.x_max;
#else
	/* Instead of x_min/x_max use first and last timestamp.

	   FIXME: this is still not perfect solution: the glitch in
	   timestamp may occur in first or last trackpoint. Find a
	   good way to find a correct first/last timestamp. */
	this->x_min_visible_t = this->track_data.x[0];
	this->x_max_visible_t = this->track_data.x[this->track_data.n_points - 1];
#endif

	const Time visible_range = this->x_max_visible_t - this->x_min_visible_t;
	if (visible_range.is_zero()) {
		qDebug() << SG_PREFIX_E << "Zero time span: min/max x = " << this->x_min_visible_t << this->x_max_visible_t << this->get_graph_title();
		return sg_ret::err;
	}

	/* Now, given the n_intervals value, find a suitable interval
	   index and value that will nicely cover visible range of
	   data. */

	const int n_intervals = GRAPH_X_INTERVALS;

	int interval_index = time_intervals.intervals.get_interval_index(this->x_min_visible_t, this->x_max_visible_t, n_intervals);
	this->x_interval_t = time_intervals.intervals.values[interval_index];

	return sg_ret::ok;
}




sg_ret ProfileView::set_initial_visible_range_y(void)
{
	/* When user will be zooming in and out, and (in particular)
	   moving graph up and down, the y_min/max_visible values will
	   be non-rounded (i.e. random).  Make them non-rounded from
	   the start, and be prepared to handle non-rounded
	   y_min/max_visible from the start. */
	const double over = 0.05; /* There is no deep reasoning behind this particular value. */
	const double range = abs(this->track_data.y_max - this->track_data.y_min);

	switch (this->viewport2d->y_domain) {
	case ViewportDomain::Speed:
	case ViewportDomain::Distance:
		/* Some graphs better start at zero, e.g. speed graph
		   or distance graph. Showing negative speed values on
		   a graph wouldn't make sense. */
		this->y_min_visible = this->track_data.y_min;
		break;
	case ViewportDomain::Elevation:
	case ViewportDomain::Gradient:
		this->y_min_visible = this->track_data.y_min - range * over;
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled y domain" << (int) this->viewport2d->y_domain;
		return sg_ret::err;
	}
	this->y_max_visible = this->track_data.y_max + range * over;



	/* Now, given the n_intervals value, find a suitable interval
	   index and value that will nicely cover visible range of
	   data. */
	const int n_intervals = GRAPH_Y_INTERVALS;
	int interval_index = 0;

	switch (this->viewport2d->y_domain) {
	case ViewportDomain::Speed:
		interval_index = speed_intervals.intervals.get_interval_index(this->y_min_visible, this->y_max_visible, n_intervals);
		this->y_interval = speed_intervals.intervals.values[interval_index];
		break;
	case ViewportDomain::Elevation:
		interval_index = altitude_intervals.intervals.get_interval_index(this->y_min_visible, this->y_max_visible, n_intervals);
		this->y_interval = altitude_intervals.intervals.values[interval_index];
		break;
	case ViewportDomain::Distance:
		interval_index = distance_intervals.intervals.get_interval_index(this->y_min_visible, this->y_max_visible, n_intervals);
		this->y_interval = distance_intervals.intervals.values[interval_index].value;
		break;
	case ViewportDomain::Gradient:
		interval_index = gradient_intervals.intervals.get_interval_index(this->y_min_visible, this->y_max_visible, n_intervals);
		this->y_interval = gradient_intervals.intervals.values[interval_index];
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled y domain" << (int) this->viewport2d->y_domain;
		return sg_ret::err;
	};


	return sg_ret::ok;
}




/* Change what is displayed in main viewport in reaction to click event in one of Profile Dialog viewports. */
static bool set_center_at_graph_position(int event_x,
					 LayerTRW * trw,
					 Viewport * main_viewport,
					 Track * trk,
					 ViewportDomain x_domain,
					 int graph_width)
{
	int x = event_x;
	if (x >= graph_width) {
		qDebug() << SG_PREFIX_E << "Condition 1 error:" << x << graph_width;
		x = graph_width; /* Notice that it's not 'x = graph_width - 1'. Current assignment will put mark at the border of graph. */
	}
	if (x < 0) {
		qDebug() << SG_PREFIX_E << "Condition 2 error:" << x;
		x = 0;
	}

	bool found = false;
	switch (x_domain) {
	case ViewportDomain::Time:
		found = trk->select_tp_by_percentage_time((double) x / graph_width, SELECTED);
		break;
	case ViewportDomain::Distance:
		found = trk->select_tp_by_percentage_dist((double) x / graph_width, NULL, SELECTED);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled x domain" << (int) x_domain;
		break;
	}

	if (found) {
		Trackpoint * tp = trk->get_tp(SELECTED);
		main_viewport->set_center_from_coord(tp->coord);
		trw->emit_layer_changed("TRW - Track Profile Dialog - set center");
	}
	return found;
}




/**
   Draw two pairs of horizontal and vertical lines intersecting at given position.

   One pair is for position of selected trackpoint.
   The other pair is for current position of cursor.

   Both "pos" arguments should indicate position in graph's coordinate system.
*/
sg_ret ProfileView::draw_marks(const ScreenPos & selected_pos, const ScreenPos & current_pos, bool & is_selected_drawn, bool & is_current_drawn)
{
	/* Restore previously saved image that has no marks on it, just the graph, grids, borders and margins. */
	if (this->viewport2d->central->canvas.saved_pixmap_valid) {
		/* Debug code. */
		// qDebug() << SG_PREFIX_I << "Restoring saved image";
		this->viewport2d->central->set_pixmap(this->viewport2d->central->canvas.saved_pixmap);
	} else {
		qDebug() << SG_PREFIX_W << "NOT restoring saved image";
	}



	/* Now draw marks on this fresh (restored from saved) image. */

	if (current_pos.x > 0 && current_pos.y > 0) {
		this->viewport2d->central_draw_simple_crosshair(current_pos);
		is_current_drawn = true;
	} else {
		is_current_drawn = false;
	}

	if (selected_pos.x > 0 && selected_pos.y > 0) {
		this->viewport2d->central_draw_simple_crosshair(selected_pos);
		is_selected_drawn = true;
	} else {
		is_selected_drawn = false;
	}



	if (is_selected_drawn || is_current_drawn) {
		/* This will call Viewport::paintEvent(), triggering final render to screen. */
		this->viewport2d->central->update();
	}

	return sg_ret::ok;
}




ProfileView * TrackProfileDialog::get_current_view(void) const
{
	const int tab_idx = this->tabs->currentIndex();
	ProfileView * profile_view = (ProfileView *) this->tabs->widget(tab_idx);
	return profile_view;
}




/**
   React to mouse button release

   Find a trackpoint corresponding to cursor position when button was released.
   Draw marking for this trackpoint.
*/
void TrackProfileDialog::handle_mouse_button_release_cb(Viewport * viewport, QMouseEvent * ev)
{
	ProfileView * graph = this->get_current_view();
	assert (graph->viewport2d->central == viewport);


	ScreenPos current_pos;
	viewport->get_cursor_pos(ev, current_pos);
	const bool found_tp = set_center_at_graph_position(current_pos.x, this->trw, this->main_viewport, this->trk, graph->viewport2d->x_domain, graph->viewport2d->central_get_width());
	if (!found_tp) {
		/* Unable to get the point so give up. */
		this->button_split_at_marker->setEnabled(false);
		return;
	}

	this->button_split_at_marker->setEnabled(true);


	/* Attempt to redraw marker on all graphs. Notice that this
	   does not redraw full graphs, just marks.

	   This is done on all graphs because we want to have 'mouse
	   release' event reflected in all graphs. */
	current_pos.set(-1.0, -1.0); /* Don't draw current position on clicks. */
	for (auto iter = this->graphs.begin(); iter != this->graphs.end(); iter++) {
		ProfileView * a_graph = *iter;
		if (!a_graph->viewport2d->central) {
			continue;
		}

		if (!a_graph->track_data.valid) {
			/* We didn't visit that tab yet, so track data
			   hasn't been generated for current graph
			   width. */
			/* FIXME: generate the track data so that we
			   can set a mark over there. */
			continue;
		}


	        ScreenPos selected_pos;
		if (sg_ret::ok != a_graph->get_position_of_tp(this->trk, SELECTED, selected_pos)) {
			continue;
		}

		/* Positions passed to draw_marks() are in graph's coordinate system, not viewport's coordinate system. */
		a_graph->draw_marks(selected_pos, current_pos, this->is_selected_drawn, this->is_current_drawn);
	}

	this->button_split_at_marker->setEnabled(this->is_selected_drawn);

	return;
}




/**
 * Calculate y position for mark on y=f(x) graph.
 */
sg_ret ProfileView::set_pos_y(ScreenPos & screen_pos)
{
	/* Using saved width/saved height for performance reasons. */
	const int w = this->viewport2d->central->saved_width;
	const int h = this->viewport2d->central->saved_height;

	int ix = (int) screen_pos.x;
	/* Ensure ix is inside of graph. */
	if (ix == w) {
		ix--;
	}

	const int y = h * (this->track_data.y[ix] - this->y_min_visible) / (this->y_max_visible - this->y_min_visible);
	screen_pos.set(screen_pos.x, y);

	return sg_ret::ok;
}




sg_ret ProfileView::get_cursor_pos_on_line(QMouseEvent * ev, ScreenPos & screen_pos)
{
	/* Get exact cursor position. 'y' may not be on a graph line. */
	if (sg_ret::ok != this->viewport2d->central->get_cursor_pos(ev, screen_pos)) {
		/* Not an error? */
		return sg_ret::ok;
	}
	/* Adjust 'y' position so that its on a graph line. */
	this->set_pos_y(screen_pos);

	return sg_ret::ok;
}



void time_label_update(QLabel * label, time_t seconds_from_start)
{
	unsigned int h = seconds_from_start/3600;
	unsigned int m = (seconds_from_start - h*3600)/60;
	unsigned int s = seconds_from_start - (3600*h) - (60*m);

	const QString tmp_buf = QObject::tr("%1:%2:%3").arg(h, 2, 10, (QChar) '0').arg(m, 2, 10, (QChar) '0').arg(s, 2, 10, (QChar) '0');
	label->setText(tmp_buf);

	return;
}




void real_time_label_update(ProfileView * graph, Track * trk)
{
	if (NULL == graph->labels.t_value) {
		/* This function shouldn't be called for graphs that don't have the T label. */
		qDebug() << SG_PREFIX_W << "Called the function, but label is NULL";
		return;
	}

	const Trackpoint * tp = trk->get_tp(CURRENT);
	if (NULL == tp) {
		return;
	}

	graph->labels.t_value->setText(tp->timestamp.to_timestamp_string(Qt::LocalTime));

	return;
}




void TrackProfileDialog::handle_cursor_move_cb(Viewport * viewport, QMouseEvent * ev)
{
	ProfileView * graph = this->get_current_view();
	assert (graph->viewport2d->central == viewport);

	if (!graph->track_data.valid) {
		qDebug() << SG_PREFIX_E << "Not handling cursor move, track data invalid";
		return;
	}

	double meters_from_start = 0.0;
	ScreenPos selected_pos; /* Crosshair laying on a graph where currently selected tp is located. */
	ScreenPos current_pos;  /* Crosshair laying on a graph, with 'x' position matching current 'x' position of cursor. */

	if (sg_ret::ok != graph->get_cursor_pos_on_line(ev, current_pos)) {
		qDebug() << SG_PREFIX_E << "Failed to get cursor position on line";
		return;
	}
	if (true || this->is_selected_drawn) {
		/* At the beginning there will be no selected tp, so
		   this function will return !ok. This is fine, ignore
		   the !ok status and continue to drawing current
		   position.

		   TODO_OPTIMIZATION: there is no need to find a
		   selected trackpoint every time a cursor moves. Only
		   current trackpoint (trackpoint under moving cursor
		   mouse) is changing. The trackpoint selected by
		   previous cursor click is does not change. */
		graph->get_position_of_tp(this->trk, SELECTED, selected_pos);
	}

	switch (graph->viewport2d->x_domain) {
	case ViewportDomain::Distance:
		this->trk->select_tp_by_percentage_dist((double) current_pos.x / graph->viewport2d->central->saved_width, &meters_from_start, CURRENT);
		graph->draw_marks(selected_pos, current_pos, this->is_selected_drawn, this->is_current_drawn);

		if (graph->labels.x_value) {
			const Distance distance(meters_from_start, SupplementaryDistanceUnit::Meters);
			graph->labels.x_value->setText(distance.convert_to_unit(Preferences::get_unit_distance()).to_string());
		}
		break;

	case ViewportDomain::Time:
		this->trk->select_tp_by_percentage_time((double) current_pos.x / graph->viewport2d->central->saved_width, CURRENT);
		graph->draw_marks(selected_pos, current_pos, this->is_selected_drawn, this->is_current_drawn);

		if (graph->labels.x_value) {
			time_t seconds_from_start = 0;
			this->trk->get_tp_relative_timestamp(seconds_from_start, CURRENT);
			time_label_update(graph->labels.x_value, seconds_from_start);
		}

		real_time_label_update(graph, this->trk);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled x domain" << (int) graph->viewport2d->x_domain;
		break;
	};


	double y = graph->track_data.y[current_pos.x];
	switch (graph->viewport2d->y_domain) {
	case ViewportDomain::Speed:
		if (graph->labels.y_value) {
			/* Even if GPS speed available (tp->speed), the text will correspond to the speed map shown.
			   No conversions needed as already in appropriate units. */
			graph->labels.y_value->setText(Speed::to_string(y));
		}
		break;
	case ViewportDomain::Elevation:
		if (graph->labels.y_value && NULL != this->trk->get_tp(CURRENT)) {
			/* Recalculate value into target unit. */
			graph->labels.y_value->setText(this->trk->get_tp(CURRENT)->altitude
						       .convert_to_unit(Preferences::get_unit_height())
						       .to_string());
		}
		break;
	case ViewportDomain::Distance:
		if (graph->labels.y_value) {
			const Distance distance_uu(y, Preferences::get_unit_distance()); /* 'y' is already recalculated to user unit, so this constructor must use user unit as well. */
			graph->labels.y_value->setText(distance_uu.to_string());
		}
		break;
	case ViewportDomain::Gradient:
		if (graph->labels.y_value) {
			const Gradient gradient_uu(y);
			graph->labels.y_value->setText(gradient_uu.to_string());
		}
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled y domain" << (int) graph->viewport2d->y_domain;
		break;
	};

	return;
}




/**
 * Draws DEM points and a respresentative speed on the supplied pixmap
 * (which is the elevations graph).
 */
void ProfileView::draw_dem_alt_speed_dist(Track * trk, bool do_dem, bool do_speed)
{
	double max_function_arg = trk->get_length_value_including_gaps();
	double max_function_value_speed = 0;

	/* Calculate the max speed factor. */
	if (do_speed) {
		max_function_value_speed = trk->get_max_speed().get_value() * 110 / 100;
	}

	double current_function_arg = 0.0;
	const double max_function_value_dem = this->y_max_visible;

	const QColor dem_color = this->dem_alt_pen.color();
	const QColor speed_color = this->gps_speed_pen.color();

	const int w = this->viewport2d->central_get_width();
	const int h = this->viewport2d->central_get_height();

	for (auto iter = std::next(trk->trackpoints.begin()); iter != trk->trackpoints.end(); iter++) {

		current_function_arg += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);
		if (do_dem) {
			const Altitude elev = DEMCache::get_elev_by_coord((*iter)->coord, DemInterpolation::Best);
			if (!elev.is_valid()) {
				continue;
			}

			const double elev_value_uu = elev.convert_to_unit(this->viewport2d->height_unit).get_value();

			/* offset is in current height units. */
			const double current_function_value_uu = elev_value_uu - this->y_min_visible;

			const int x = w * current_function_arg / max_function_arg;
			const int y = 0 - h * current_function_value_uu / max_function_value_dem;
			this->viewport2d->central->fill_rectangle(dem_color, x - 2, y - 2, 4, 4);
		}

		if (do_speed) {
			if (std::isnan((*iter)->speed)) {
				continue;
			}

			const double current_function_value = (*iter)->speed;

			/* This is just a speed indicator - no actual values can be inferred by user. */
			const int x = w * current_function_arg / max_function_arg;
			const int y = 0 - h * current_function_value / max_function_value_speed;
			this->viewport2d->central->fill_rectangle(speed_color, x - 2, y - 2, 4, 4);
		}
	}
}




void ProfileView::draw_function_values(void)
{
	const double visible_range = this->y_max_visible - this->y_min_visible;
	const int w = this->viewport2d->central_get_width();
	const int h = this->viewport2d->central_get_height();

	if (this->viewport2d->y_domain == ViewportDomain::Elevation) {
		for (int i = 0; i < w; i++) {
			if (this->track_data.y[i] == VIK_DEFAULT_ALTITUDE) {
				this->viewport2d->central_draw_line(this->no_alt_info_pen,
								    i, 0,
								    i, 0 + h);
			} else {
				this->viewport2d->central_draw_line(this->main_pen,
								    i, 0,
								    i, 0 + h * (this->track_data.y[i] - this->y_min_visible) / visible_range);
			}
		}
	} else {
		for (int i = 0; i < w; i++) {
			this->viewport2d->central_draw_line(this->main_pen,
							    i, 0,
							    i, 0 + h * (this->track_data.y[i] - this->y_min_visible) / visible_range);
		}
	}
}



void ProfileViewET::draw_additional_indicators(Track * trk)
{
	const int w = this->viewport2d->central_get_width();
	const int h = this->viewport2d->central_get_height();

	if (this->show_dem_cb && this->show_dem_cb->checkState())  {
		const double max_function_value = this->y_max_visible;

		const QColor color = this->dem_alt_pen.color();

		for (int i = 0; i < w; i++) {
			/* This could be slow doing this each time... */
			const bool found_tp = trk->select_tp_by_percentage_time(((double) i / (double) w), CURRENT);
			if (!found_tp) {
				continue;
			}

			const Trackpoint * tp = trk->get_tp(CURRENT);
			const Altitude elev = DEMCache::get_elev_by_coord(tp->coord, DemInterpolation::Simple);
			if (!elev.is_valid()) {
				continue;
			}

			const double elev_value_uu = elev.convert_to_unit(Preferences::get_unit_height()).get_value();

			/* offset is in current height units. */
			const double current_function_value_uu = elev_value_uu - this->y_min_visible;

			const int x = i;
			const int y = 0 - h * current_function_value_uu / max_function_value;
			this->viewport2d->central->fill_rectangle(color, x - 2, y - 2, 4, 4);
		}
	}


	/* Show speeds. */
	if (this->show_speed_cb && this->show_speed_cb->checkState()) {
		/* This is just an indicator - no actual values can be inferred by user. */

		const double max_function_value = trk->get_max_speed().get_value() * 110 / 100;

		const QColor color = this->gps_speed_pen.color();
		for (int i = 0; i < w; i++) {

			const double current_function_value = this->track_data.y[i];

			const int x = i;
			const int y = 0 - h * current_function_value / max_function_value;
			this->viewport2d->central->fill_rectangle(color, x - 2, y - 2, 4, 4);
		}
	}

	return;
}




void ProfileViewSD::draw_additional_indicators(Track * trk)
{
	if (this->show_gps_speed_cb && this->show_gps_speed_cb->checkState()) {

		const int w = this->viewport2d->central_get_width();
		const int h = this->viewport2d->central_get_height();

		const double max_function_arg = trk->get_length_value_including_gaps();
		const double max_function_value = this->y_max_visible;
		double current_function_arg = 0.0;
		double current_function_value = 0.0;

		const QColor color = this->gps_speed_pen.color();
		for (auto iter = std::next(trk->trackpoints.begin()); iter != trk->trackpoints.end(); iter++) {
			double gps_speed = (*iter)->speed;
			if (std::isnan(gps_speed)) {
				continue;
			}

			gps_speed = Speed::convert_mps_to(gps_speed, this->viewport2d->speed_unit);

			current_function_arg += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);
			current_function_value = gps_speed - this->y_min_visible;

			const int x = w * current_function_arg / max_function_arg;
			const int y = 0 - h * current_function_value / max_function_value;
			this->viewport2d->central->fill_rectangle(color, x - 2, y - 2, 4, 4);
		}
	}
}




void ProfileViewED::draw_additional_indicators(Track * trk)
{
	const bool do_show_dem = this->show_dem_cb && this->show_dem_cb->checkState();
	const bool do_show_gps_speed = this->show_gps_speed_cb && this->show_gps_speed_cb->checkState();

	if (do_show_dem || do_show_gps_speed) {

		/* Ensure somekind of max speed when not set. */
		if (!trk->get_max_speed().is_valid() || trk->get_max_speed().get_value() < 0.01) {
			trk->calculate_max_speed();
		}

		this->draw_dem_alt_speed_dist(trk, do_show_dem, do_show_gps_speed);
	}
}




void ProfileViewGD::draw_additional_indicators(Track * trk)
{
	const bool do_show_gps_speed = this->show_gps_speed_cb && this->show_gps_speed_cb->checkState();
	if (do_show_gps_speed) {
		/* Ensure some kind of max speed when not set. */
		if (!trk->get_max_speed().is_valid() || trk->get_max_speed().get_value() < 0.01) {
			trk->calculate_max_speed();
		}

		this->draw_speed_dist(trk);
	}
}




void ProfileViewST::draw_additional_indicators(Track * trk)
{
	if (this->show_gps_speed_cb && this->show_gps_speed_cb->checkState()) {

		const int w = this->viewport2d->central_get_width();
		const int h = this->viewport2d->central_get_height();

		Time ts_begin;
		Time ts_end;
		if (sg_ret::ok == trk->get_timestamps(ts_begin, ts_end)) {

			const time_t time_begin = ts_begin.get_value();
			const time_t time_end = ts_end.get_value();

			const time_t max_function_arg = time_end - time_begin;
			const double max_function_value = this->y_max_visible;

			const QColor color = this->gps_speed_pen.color();

			for (auto iter = trk->trackpoints.begin(); iter != trk->trackpoints.end(); iter++) {
				double gps_speed = (*iter)->speed;
				if (std::isnan(gps_speed)) {
					continue;
				}

				gps_speed = Speed::convert_mps_to(gps_speed, this->viewport2d->speed_unit);

				const time_t current_function_arg = (*iter)->timestamp.get_value() - time_begin;
				const double current_function_value = gps_speed - this->y_min_visible;

				const int x = w * current_function_arg / max_function_arg;
				const int y = 0 - h * current_function_value / max_function_value;
				this->viewport2d->central->fill_rectangle(color, x - 2, y - 2, 4, 4);
			}
		} else {
			qDebug() << SG_PREFIX_W << "Not drawing additional indicators: can't get timestamps";
		}
	}
}




void ProfileViewDT::draw_additional_indicators(Track * trk)
{
	/* Show speed indicator. */
	if (this->show_speed_cb && this->show_speed_cb->checkState()) {

		const double max_function_value = trk->get_max_speed().get_value() * 110 / 100;
		const int w = this->viewport2d->central_get_width();
		const int h = this->viewport2d->central_get_height();

		const QColor color = this->gps_speed_pen.color();
		/* This is just an indicator - no actual values can be inferred by user. */
		for (int i = 0; i < w; i++) {

			const double current_function_value = this->track_data.y[i];

			const int x = i;
			const int y = 0 - h * current_function_value / max_function_value;
			this->viewport2d->central->fill_rectangle(color, x - 2, y - 2, 4, 4);
		}
	}
}




void ProfileViewET::save_values(void)
{
	ApplicationState::set_boolean(VIK_SETTINGS_TRACK_PROFILE_ET_SHOW_DEM, this->show_dem_cb->checkState());
	ApplicationState::set_boolean(VIK_SETTINGS_TRACK_PROFILE_ET_SHOW_SPEED, this->show_speed_cb->checkState());
}




void ProfileViewSD::save_values(void)
{
	ApplicationState::set_boolean(VIK_SETTINGS_TRACK_PROFILE_SD_SHOW_GPS_SPEED, this->show_gps_speed_cb->checkState());
}




void ProfileViewED::save_values(void)
{
	ApplicationState::set_boolean(VIK_SETTINGS_TRACK_PROFILE_ED_SHOW_DEM, this->show_dem_cb->checkState());
	ApplicationState::set_boolean(VIK_SETTINGS_TRACK_PROFILE_ED_SHOW_GPS_SPEED, this->show_gps_speed_cb->checkState());
}




void ProfileViewGD::save_values(void)
{
	ApplicationState::set_boolean(VIK_SETTINGS_TRACK_PROFILE_GD_SHOW_GPS_SPEED, this->show_gps_speed_cb->checkState());
}




void ProfileViewST::save_values(void)
{
	ApplicationState::set_boolean(VIK_SETTINGS_TRACK_PROFILE_ST_SHOW_GPS_SPEED, this->show_gps_speed_cb->checkState());
}




void ProfileViewDT::save_values(void)
{
	ApplicationState::set_boolean(VIK_SETTINGS_TRACK_PROFILE_DT_SHOW_SPEED, this->show_speed_cb->checkState());
}




/**
   \brief Draw the y = f(x) graph
*/
sg_ret ProfileView::draw_graph(Track * trk)
{
	if (this->viewport2d->x_domain == ViewportDomain::Time) {
		const Time duration = trk->get_duration(true);
		if (!duration.is_valid() || duration.get_value() <= 0) {
			return sg_ret::err;
		}
	}

	this->regenerate_sizes();

	if (sg_ret::ok != this->regenerate_data(trk)) {
		return sg_ret::err;
	}

	/* Clear before redrawing. */
	this->viewport2d->central->clear();



     	struct my_data data2;
	data2.width = this->viewport2d->central_get_width();
	data2.height = this->viewport2d->central_get_height();

	if (sg_ret::err == trk->draw_tree_item(this->viewport2d->central, &data2, this->viewport2d->x_domain, this->viewport2d->y_domain)) {
		this->draw_function_values();
	}

	/* Draw grid on top of graph of values. */
	this->draw_x_grid_inside(trk);
	this->draw_y_grid_inside();

	this->draw_additional_indicators(trk);

	/* This will call Viewport::paintEvent(), triggering final render to screen. */
	this->viewport2d->central->update();

	/* The pixmap = margin + graph area. */
	qDebug() << SG_PREFIX_I << "Saving viewport" << this->viewport2d->central->debug;
	this->viewport2d->central->canvas.saved_pixmap = this->viewport2d->central->get_pixmap();
	this->viewport2d->central->canvas.saved_pixmap_valid = true;

	return sg_ret::ok;
}




/**
   Draws representative speed on a graph
   (which is the gradients graph).
*/
void ProfileView::draw_speed_dist(Track * trk)
{
	const double max_function_value = trk->get_max_speed().get_value() * 110 / 100; /* Calculate the max speed factor. */
	const double max_function_arg = trk->get_length_value_including_gaps();

	const int w = this->viewport2d->central_get_width();
	const int h = this->viewport2d->central_get_height();

	const QColor color = this->gps_speed_pen.color();
	double current_function_arg = 0.0;
	double current_function_value = 0.0;
	for (auto iter = std::next(trk->trackpoints.begin()); iter != trk->trackpoints.end(); iter++) {
		if (std::isnan((*iter)->speed)) {
			continue;
		}

		current_function_arg += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);
		current_function_value = (*iter)->speed;

		/* This is just a speed indicator - no actual values can be inferred by user. */
		const int x = w * current_function_arg / max_function_arg;
		const int y = 0 - h * current_function_value / max_function_value;
		this->viewport2d->central->fill_rectangle(color, x - 2, y - 2, 4, 4);
	}
}




/**
   Look up view
*/
ProfileView * TrackProfileDialog::find_view(Viewport2D * viewport) const
{
	for (auto iter = this->graphs.begin(); iter != this->graphs.end(); iter++) {
		ProfileView * view = *iter;
		if (view->viewport2d == viewport) {
			return view;
		}
	}
	return NULL;
}




/**
   Draw all graphs
*/
void TrackProfileDialog::draw_all_graphs(bool resized)
{
	for (auto iter = this->graphs.begin(); iter != this->graphs.end(); iter++) {
		ProfileView * view = *iter;
		if (!view->viewport2d->central) {
			continue;
		}

		/* If dialog window is resized then saved image is no longer valid. */
		view->viewport2d->central->canvas.saved_pixmap_valid = !resized;
		this->draw_center(view);

		/* TODO: what about drawing margins? */
	}
}




sg_ret ProfileView::get_position_of_tp(Track * trk, int tp_idx, ScreenPos & screen_pos)
{
	double pc = NAN;

	Trackpoint * tp = trk->get_tp(tp_idx);
	if (NULL == tp) {
		return sg_ret::err;
	}

	switch (this->viewport2d->x_domain) {
	case ViewportDomain::Time:
		pc = trk->get_tp_time_percent(tp_idx);
		break;
	case ViewportDomain::Distance:
		pc = trk->get_tp_distance_percent(tp_idx);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled x domain" << (int) this->viewport2d->x_domain;
		/* pc = NAN */
		break;
	}

	if (!std::isnan(pc)) {
		screen_pos.set(pc * this->viewport2d->central_get_width(), 0.0); /* Set x.*/
		this->set_pos_y(screen_pos); /* Find y. */
	}

	// qDebug() << SG_PREFIX_D << "returning pos of tp idx" << tp_idx;

	return sg_ret::ok;
}




sg_ret TrackProfileDialog::draw_center(ProfileView * graph)
{
	sg_ret ret = graph->draw_graph(this->trk);
	if (sg_ret::ok != ret) {
		qDebug() << SG_PREFIX_E << "Failed to draw single graph";
		return ret;
	}

	/* Ensure markers are redrawn if necessary. */
	if (this->is_selected_drawn || this->is_current_drawn) {

		ScreenPos current_pos(-1.0, -1.0);
		if (this->is_current_drawn) {
			graph->get_position_of_tp(this->trk, CURRENT, current_pos);
		}

		ScreenPos selected_pos;
		graph->get_position_of_tp(this->trk, SELECTED, selected_pos);

		ret = graph->draw_marks(selected_pos, current_pos, this->is_selected_drawn, this->is_current_drawn);
		if (sg_ret::ok != ret) {
			qDebug() << SG_PREFIX_E << "Failed to draw marks";
			return ret;
		}
	}

	return sg_ret::ok;
}




sg_ret TrackProfileDialog::draw_left(ProfileView * graph)
{
	graph->draw_y_grid_outside();

	return sg_ret::ok;
}




sg_ret TrackProfileDialog::draw_bottom(ProfileView * graph)
{
	graph->draw_x_grid_outside(graph->dialog->trk);

	return sg_ret::ok;
}





sg_ret TrackProfileDialog::paint_center_cb(Viewport2D * viewport)
{
	qDebug() << SG_PREFIX_SLOT << "Reacting to signal from canvas" << viewport->central->canvas.debug;

	ProfileView * view = this->find_view(viewport);
	if (!view) {
		qDebug() << SG_PREFIX_E << "Can't find view";
		return sg_ret::err;
	}

	view->viewport2d->central->canvas.saved_pixmap_valid = true;
	this->draw_center(view);

	return sg_ret::ok;
}




sg_ret TrackProfileDialog::paint_left_cb(Viewport2D * viewport)
{
	qDebug() << SG_PREFIX_SLOT << "Reacting to signal from canvas" << viewport->left->canvas.debug;

	ProfileView * view = this->find_view(viewport);
	if (!view) {
		qDebug() << SG_PREFIX_E << "Can't find view";
		return sg_ret::err;
	}

	//view->viewport2d->left->canvas.saved_img_valid = true;
	this->draw_left(view);

	return sg_ret::ok;
}




sg_ret TrackProfileDialog::paint_bottom_cb(Viewport2D * viewport)
{
	qDebug() << SG_PREFIX_SLOT << "Reacting to signal from canvas" << viewport->bottom->canvas.debug;

	ProfileView * view = this->find_view(viewport);
	if (!view) {
		qDebug() << SG_PREFIX_E << "Can't find view";
		return sg_ret::err;
	}

	//view->viewport2d->bottom->ocanvas.saved_img_valid = true;
	this->draw_bottom(view);

	return sg_ret::ok;
}




void ProfileView::create_viewport(TrackProfileDialog * new_dialog, ViewportDomain x_domain, ViewportDomain y_domain)
{
	const int initial_width = GRAPH_INITIAL_WIDTH;
	const int initial_height = GRAPH_INITIAL_HEIGHT;

	this->viewport2d = new Viewport2D(GRAPH_MARGIN_LEFT, GRAPH_MARGIN_RIGHT, GRAPH_MARGIN_TOP, GRAPH_MARGIN_BOTTOM, dialog);
	snprintf(this->viewport2d->central->debug, sizeof (this->viewport2d->central->debug), "%s", this->get_graph_title().toUtf8().constData());
	this->viewport2d->central->resize(initial_width, initial_height);
	this->viewport2d->central->reconfigure_drawing_area(initial_width, initial_height);

	this->viewport2d->x_domain = x_domain;
	this->viewport2d->y_domain = y_domain;

	QObject::connect(this->viewport2d->central, SIGNAL (cursor_moved(Viewport *, QMouseEvent *)),    dialog, SLOT (handle_cursor_move_cb(Viewport *, QMouseEvent *)));
	QObject::connect(this->viewport2d->central, SIGNAL (button_released(Viewport *, QMouseEvent *)), dialog, SLOT (handle_mouse_button_release_cb(Viewport *, QMouseEvent *)));

	return;
}




void TrackProfileDialog::save_values(void)
{
	/* Session settings. */
	ApplicationState::set_integer(VIK_SETTINGS_TRACK_PROFILE_WIDTH, this->profile_width);
	ApplicationState::set_integer(VIK_SETTINGS_TRACK_PROFILE_HEIGHT, this->profile_height);

	/* Just for this session. */
	for (auto iter = this->graphs.begin(); iter != this->graphs.end(); iter++) {
		(*iter)->save_values();
	}
}




void TrackProfileDialog::destroy_cb(void) /* Slot. */
{
	this->save_values();
}




void TrackProfileDialog::dialog_response_cb(int resp) /* Slot. */
{
	bool keep_dialog = false;

	/* Note: destroying dialog (eg, parent window exit) won't give "response". */
	switch (resp) {
	case SG_TRACK_PROFILE_CANCEL:
		this->reject();
		break;

	case SG_TRACK_PROFILE_OK:
		this->trk->update_tree_item_properties();
		this->trw->emit_layer_changed("TRW - Track Profile Dialog - Profile OK");
		this->accept();
		break;

	case SG_TRACK_PROFILE_SPLIT_AT_MARKER:
		if (sg_ret::ok != this->trk->split_at_trackpoint(SELECTED)) {
			Dialog::error(tr("Failed to split track. Track unchanged"), this->trw->get_window());
			keep_dialog = true;
		}
		this->trw->emit_layer_changed("A TRW Track has been split into several tracks (at marker)");
		break;
	default:
		qDebug() << SG_PREFIX_E << "Dialog response slot: unknown response" << resp;
		return;
	}

	/* Keep same behaviour for now: destroy dialog if click on any button. */
	if (!keep_dialog) {
		this->destroy_cb();
		this->accept();
	}
}




/**
 * Force a redraw when checkbutton has been toggled to show/hide that information.
 */
void TrackProfileDialog::checkbutton_toggle_cb(void)
{
	/* Even though not resized, we'll pretend it is -
	   as this invalidates the saved images (since the image may have changed). */
	this->draw_all_graphs(true);
}




/**
   Create the widgets for the given graph tab
*/
void ProfileView::create_widgets_layout(void)
{
	this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	this->setMinimumSize(500, 300);

	this->main_vbox = new QVBoxLayout();
	this->labels_grid = new QGridLayout();
	this->controls_vbox = new QVBoxLayout();

	QLayout * old = this->layout();
	delete old;
	qDeleteAll(this->children());
	this->setLayout(this->main_vbox);

	this->main_vbox->addWidget(this->viewport2d);
	this->main_vbox->addLayout(this->labels_grid);
	this->main_vbox->addLayout(this->controls_vbox);

	return;
}




void SlavGPS::track_profile_dialog(Track * trk, Viewport * main_viewport, QWidget * parent)
{
	TrackProfileDialog dialog(QObject::tr("Track Profile"), trk, main_viewport, parent);
	trk->set_profile_dialog(&dialog);
	dialog.exec();
	trk->clear_profile_dialog();
}




QString ProfileView::get_graph_title(void) const
{
	QString result;

	if (this->viewport2d->y_domain == ViewportDomain::Elevation && this->viewport2d->x_domain == ViewportDomain::Distance) {
		result = QObject::tr("Elevation over distance");

	} else if (this->viewport2d->y_domain == ViewportDomain::Gradient && this->viewport2d->x_domain == ViewportDomain::Distance) {
		result = QObject::tr("Gradient over distance");

	} else if (this->viewport2d->y_domain == ViewportDomain::Speed && this->viewport2d->x_domain == ViewportDomain::Time) {
		result = QObject::tr("Speed over time");

	} else if (this->viewport2d->y_domain == ViewportDomain::Distance && this->viewport2d->x_domain == ViewportDomain::Time) {
		result = QObject::tr("Distance over time");

	} else if (this->viewport2d->y_domain == ViewportDomain::Elevation && this->viewport2d->x_domain == ViewportDomain::Time) {
		result = QObject::tr("Elevation over time");

	} else if (this->viewport2d->y_domain == ViewportDomain::Speed && this->viewport2d->x_domain == ViewportDomain::Distance) {
		result = QObject::tr("Speed over distance");

	} else {
		qDebug() << SG_PREFIX_E << "Unhandled x/y domain" << (int) this->viewport2d->x_domain << (int) this->viewport2d->y_domain;
	}

	return result;
}




TrackProfileDialog::TrackProfileDialog(QString const & title, Track * new_trk, Viewport * new_main_viewport, QWidget * parent) : QDialog(parent)
{
	this->setWindowTitle(tr("%1 - Track Profile").arg(new_trk->name));

	this->trw = (LayerTRW *) new_trk->get_owning_layer();
	this->trk = new_trk;
	this->main_viewport = new_main_viewport;


	QLayout * old = this->layout();
	delete old;
	qDeleteAll(this->children());
	QVBoxLayout * vbox = new QVBoxLayout;
	this->setLayout(vbox);


	int profile_size_value;
	/* Ensure minimum values. */
	this->profile_width = 600;
	if (ApplicationState::get_integer(VIK_SETTINGS_TRACK_PROFILE_WIDTH, &profile_size_value)) {
		if (profile_size_value > this->profile_width) {
			this->profile_width = profile_size_value;
		}
	}

	this->profile_height = 300;
	if (ApplicationState::get_integer(VIK_SETTINGS_TRACK_PROFILE_HEIGHT, &profile_size_value)) {
		if (profile_size_value > this->profile_height) {
			this->profile_height = profile_size_value;
		}
	}

	this->graphs.push_back(new ProfileViewED(this));
	this->graphs.push_back(new ProfileViewGD(this));
	this->graphs.push_back(new ProfileViewST(this));
	this->graphs.push_back(new ProfileViewDT(this));
	this->graphs.push_back(new ProfileViewET(this));
	this->graphs.push_back(new ProfileViewSD(this));

	this->tabs = new QTabWidget();
	this->tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);


	/* NB This value not shown yet - but is used by internal calculations. */
	/* TODO_IMPROVEMENT: move this calculation into Track class. */
	this->trk->track_length_including_gaps = this->trk->get_length_value_including_gaps();


	for (auto iter = this->graphs.begin(); iter != this->graphs.end(); iter++) {
		ProfileView * graph = *iter;
		if (!graph->viewport2d->central) {
			continue;
		}

		graph->create_widgets_layout();
		graph->configure_labels();
		graph->configure_controls();

		this->tabs->addTab(graph, graph->get_graph_title());

		connect(&graph->viewport2d->central->canvas, SIGNAL (reconfigured(Viewport2D *)), this, SLOT (paint_center_cb(Viewport2D *)));
		connect(&graph->viewport2d->left->canvas, SIGNAL (reconfigured(Viewport2D *)), this, SLOT (paint_left_cb(Viewport2D *)));
		connect(&graph->viewport2d->bottom->canvas, SIGNAL (reconfigured(Viewport2D *)), this, SLOT (paint_bottom_cb(Viewport2D *)));
	}


	this->button_box = new QDialogButtonBox();


	this->button_cancel = this->button_box->addButton(tr("&Cancel"), QDialogButtonBox::RejectRole);
	this->button_split_at_marker = this->button_box->addButton(tr("Split at &Marker"), QDialogButtonBox::ActionRole);
	this->button_ok = this->button_box->addButton(tr("&OK"), QDialogButtonBox::AcceptRole);

	this->button_split_at_marker->setEnabled(this->trk->get_tp(SELECTED) != NULL); /* Initially no trackpoint is selected. */

	this->signal_mapper = new QSignalMapper(this);
	connect(this->button_cancel,          SIGNAL (released()), signal_mapper, SLOT (map()));
	connect(this->button_split_at_marker, SIGNAL (released()), signal_mapper, SLOT (map()));
	connect(this->button_ok,              SIGNAL (released()), signal_mapper, SLOT (map()));

	this->signal_mapper->setMapping(this->button_cancel,          SG_TRACK_PROFILE_CANCEL);
	this->signal_mapper->setMapping(this->button_split_at_marker, SG_TRACK_PROFILE_SPLIT_AT_MARKER);
	this->signal_mapper->setMapping(this->button_ok,              SG_TRACK_PROFILE_OK);

	connect(this->signal_mapper, SIGNAL (mapped(int)), this, SLOT (dialog_response_cb(int)));


	vbox->addWidget(this->tabs);
	vbox->addWidget(this->button_box);
}




void ProfileView::configure_labels(void)
{
	switch (this->viewport2d->x_domain) {
	case ViewportDomain::Distance:
		this->labels.x_label = new QLabel(QObject::tr("Track Distance:"));
		this->labels.x_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);

		break;

	case ViewportDomain::Time:
		this->labels.x_label = new QLabel(QObject::tr("Time From Start:"));
		this->labels.x_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);

		/* Additional timestamp to provide more information in UI. */
		this->labels.t_label = new QLabel(QObject::tr("Time/Date:"));
		this->labels.t_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);

		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled x domain" << (int) this->viewport2d->x_domain;
		break;
	}


	switch (this->viewport2d->y_domain) {
	case ViewportDomain::Elevation:
		this->labels.y_label = new QLabel(QObject::tr("Track Height:"));
		this->labels.y_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);

		break;

	case ViewportDomain::Gradient:
		this->labels.y_label = new QLabel(QObject::tr("Track Gradient:"));
		this->labels.y_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);

		break;

	case ViewportDomain::Speed:
		this->labels.y_label = new QLabel(QObject::tr("Track Speed:"));
		this->labels.y_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);

		break;

	case ViewportDomain::Distance:
		this->labels.y_label = new QLabel(QObject::tr("Track Distance:"));
		this->labels.y_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);

		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled y domain" << (int) this->viewport2d->y_domain;
		break;
	}


	/* Use spacer item in last column to bring first two columns
	   (with parameter's name and parameter's value) close
	   together. */

	int row = 0;
	this->labels_grid->addWidget(this->labels.x_label, row, 0, Qt::AlignLeft);
	this->labels_grid->addWidget(this->labels.x_value, row, 1, Qt::AlignRight);
	this->labels_grid->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum), row, 3);
	row++;

	this->labels_grid->addWidget(this->labels.y_label, row, 0, Qt::AlignLeft);
	this->labels_grid->addWidget(this->labels.y_value, row, 1, Qt::AlignRight);
	this->labels_grid->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum), row, 3);
	row++;

	if (this->labels.t_value) {
		this->labels_grid->addWidget(this->labels.t_label, row, 0, Qt::AlignLeft);
		this->labels_grid->addWidget(this->labels.t_value, row, 1, Qt::AlignRight);
		this->labels_grid->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum), row, 3);
		row++;
	}

	return;
}




void ProfileViewET::configure_controls(void)
{
	bool value;

	this->show_dem_cb = new QCheckBox(QObject::tr("Show DEM"), this->dialog);
	if (ApplicationState::get_boolean(VIK_SETTINGS_TRACK_PROFILE_ET_SHOW_DEM, &value)) {
		this->show_dem_cb->setCheckState(value ? Qt::Checked : Qt::Unchecked);
	}
	this->controls_vbox->addWidget(this->show_dem_cb);
	QObject::connect(this->show_dem_cb, SIGNAL (stateChanged(int)), this->dialog, SLOT (checkbutton_toggle_cb()));

	this->show_speed_cb = new QCheckBox(QObject::tr("Show Speed"), dialog);
	if (ApplicationState::get_boolean(VIK_SETTINGS_TRACK_PROFILE_ET_SHOW_SPEED, &value)) {
		this->show_speed_cb->setCheckState(value ? Qt::Checked : Qt::Unchecked);
	}
	this->controls_vbox->addWidget(this->show_speed_cb);
	QObject::connect(this->show_speed_cb, SIGNAL (stateChanged(int)), this->dialog, SLOT (checkbutton_toggle_cb()));
}




void ProfileViewSD::configure_controls(void)
{
	bool value;

	this->show_gps_speed_cb = new QCheckBox(QObject::tr("Show GPS S&peed"), this->dialog);
	if (ApplicationState::get_boolean(VIK_SETTINGS_TRACK_PROFILE_SD_SHOW_GPS_SPEED, &value)) {
		this->show_gps_speed_cb->setCheckState(value ? Qt::Checked : Qt::Unchecked);
	}
	this->controls_vbox->addWidget(this->show_gps_speed_cb);
	QObject::connect(this->show_gps_speed_cb, SIGNAL (stateChanged(int)), this->dialog, SLOT (checkbutton_toggle_cb()));
}




void ProfileViewED::configure_controls(void)
{
	bool value;

	this->show_dem_cb = new QCheckBox(QObject::tr("Show DEM"), this->dialog);
	if (ApplicationState::get_boolean(VIK_SETTINGS_TRACK_PROFILE_ED_SHOW_DEM, &value)) {
		this->show_dem_cb->setCheckState(value ? Qt::Checked : Qt::Unchecked);
	}
	this->controls_vbox->addWidget(this->show_dem_cb);
	QObject::connect(this->show_dem_cb, SIGNAL (stateChanged(int)), this->dialog, SLOT (checkbutton_toggle_cb()));

	this->show_gps_speed_cb = new QCheckBox(QObject::tr("Show GPS S&peed"), this->dialog);
	if (ApplicationState::get_boolean(VIK_SETTINGS_TRACK_PROFILE_ED_SHOW_GPS_SPEED, &value)) {
		this->show_gps_speed_cb->setCheckState(value ? Qt::Checked : Qt::Unchecked);
	}
	this->controls_vbox->addWidget(this->show_gps_speed_cb);
	QObject::connect(this->show_gps_speed_cb, SIGNAL (stateChanged(int)), this->dialog, SLOT (checkbutton_toggle_cb()));
}




void ProfileViewGD::configure_controls(void)
{
	bool value;

	this->show_gps_speed_cb = new QCheckBox(QObject::tr("Show GPS S&peed"), this->dialog);
	if (ApplicationState::get_boolean(VIK_SETTINGS_TRACK_PROFILE_GD_SHOW_GPS_SPEED, &value)) {
		this->show_gps_speed_cb->setCheckState(value ? Qt::Checked : Qt::Unchecked);
	}
	this->controls_vbox->addWidget(this->show_gps_speed_cb);
	QObject::connect(this->show_gps_speed_cb, SIGNAL (stateChanged(int)), this->dialog, SLOT (checkbutton_toggle_cb()));
}




void ProfileViewST::configure_controls(void)
{
	bool value;

	this->show_gps_speed_cb = new QCheckBox(QObject::tr("Show GPS S&peed"), this->dialog);
	if (ApplicationState::get_boolean(VIK_SETTINGS_TRACK_PROFILE_ST_SHOW_GPS_SPEED, &value)) {
		this->show_gps_speed_cb->setCheckState(value ? Qt::Checked : Qt::Unchecked);
	}
	this->controls_vbox->addWidget(this->show_gps_speed_cb);
	QObject::connect(this->show_gps_speed_cb, SIGNAL (stateChanged(int)), this->dialog, SLOT (checkbutton_toggle_cb()));
}




void ProfileViewDT::configure_controls(void)
{
	bool value;

	this->show_speed_cb = new QCheckBox(QObject::tr("Show S&peed"), this->dialog);
	if (ApplicationState::get_boolean(VIK_SETTINGS_TRACK_PROFILE_DT_SHOW_SPEED, &value)) {
		this->show_speed_cb->setCheckState(value ? Qt::Checked : Qt::Unchecked);
	}
	this->controls_vbox->addWidget(this->show_speed_cb);
	QObject::connect(this->show_speed_cb, SIGNAL (stateChanged(int)), this->dialog, SLOT (checkbutton_toggle_cb()));
}




QString get_time_grid_label(const Time & interval_value, const Time & value)
{
	QString result;

	const time_t val = value.get_value();
	const time_t interval = interval_value.get_value();

	switch (interval) {
	case 60:
	case 120:
	case 300:
	case 900:
		/* Minutes. */
		result = QObject::tr("%1 m").arg((int) (val / 60));
		break;
	case 1800:
	case 3600:
	case 10800:
	case 21600:
		/* Hours. */
		result = QObject::tr("%1 h").arg(((double) (val / (60 * 60))), 0, 'f', 1);
		break;
	case 43200:
	case 86400:
	case 172800:
		/* Days. */
		result = QObject::tr("%1 d").arg(((double) val / (60 *60 * 24)), 0, 'f', 1);
		break;
	case 604800:
	case 1209600:
		/* Weeks. */
		result = QObject::tr("%1 w").arg(((double) val / (60 * 60 * 24 * 7)), 0, 'f', 1);
		break;
	case 2419200:
		/* Months. */
		result = QObject::tr("%1 M").arg(((double) val / (60 * 60 * 24 * 28)), 0, 'f', 1);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled time interval value" << val;
		break;
	}

	return result;
}




ProfileView::ProfileView(ViewportDomain x_domain, ViewportDomain y_domain, TrackProfileDialog * new_dialog, QWidget * parent) : QWidget(parent)
{
	this->dialog = new_dialog;

	if (!ProfileView::supported_domains(x_domain, y_domain)) {
		qDebug() << SG_PREFIX_E << "Unhandled combination of x/y domains:" << (int) x_domain << (int) y_domain;
	}

	this->main_pen.setColor("lightsteelblue");
	this->main_pen.setWidth(1);

	this->gps_speed_pen.setColor("red");
	this->dem_alt_pen.setColor("green");
	this->no_alt_info_pen.setColor("yellow");

	this->create_viewport(new_dialog, x_domain, y_domain);
}




ProfileViewET::ProfileViewET(TrackProfileDialog * new_dialog) : ProfileView(ViewportDomain::Time,     ViewportDomain::Elevation, new_dialog) {}
ProfileViewSD::ProfileViewSD(TrackProfileDialog * new_dialog) : ProfileView(ViewportDomain::Distance, ViewportDomain::Speed,     new_dialog) {}
ProfileViewED::ProfileViewED(TrackProfileDialog * new_dialog) : ProfileView(ViewportDomain::Distance, ViewportDomain::Elevation, new_dialog) {}
ProfileViewGD::ProfileViewGD(TrackProfileDialog * new_dialog) : ProfileView(ViewportDomain::Distance, ViewportDomain::Gradient,  new_dialog) {}
ProfileViewST::ProfileViewST(TrackProfileDialog * new_dialog) : ProfileView(ViewportDomain::Time,     ViewportDomain::Speed,     new_dialog) {}
ProfileViewDT::ProfileViewDT(TrackProfileDialog * new_dialog) : ProfileView(ViewportDomain::Time,     ViewportDomain::Distance,  new_dialog) {}



ProfileView::~ProfileView()
{
	delete this->viewport2d;
}




sg_ret ProfileView::regenerate_data(Track * trk)
{
	this->track_data.invalidate();

	/* Ask a track to generate a vector of values representing some parameter
	   of a track as a function of either time or distance. */
	return this->regenerate_data_from_scratch(trk);
}




sg_ret ProfileView::regenerate_sizes(void)
{
	this->viewport2d->central->saved_width = this->viewport2d->central_get_width();
	this->viewport2d->central->saved_height = this->viewport2d->central_get_height();

	return sg_ret::ok;
}




/**
   Find first grid line. It will be a multiple of interval
   just below min_visible.

   Find last grid line. It will be a multiple of interval
   just above max_visible.

   All grid lines will be drawn starting from the first to
   last (provided that they will fall within graph's main
   area).

   When looking for first and last line, start from zero value
   and go up or down: a grid line will be always drawn at zero
   and/or at multiples of interval (depending whether they
   will fall within graph's main area).
*/
template <typename T>
void find_grid_line_indices(const T & min_visible, const T & max_visible, const T & interval, int & first_mark, int & last_mark)
{
	/* 'first_mark * y_interval' will be below y_min_visible. */
	if (min_visible <= 0) {
		while (interval * first_mark > min_visible) {
			first_mark--;
		}
	} else {
		while (interval * first_mark + interval < min_visible) {
			first_mark++;
		}
	}

	/* 'last_mark * y_interval' will be above y_max_visible. */
	if (max_visible <= 0) {
		while (interval * last_mark - interval > max_visible) {
			last_mark--;
		}
	} else {
		while (interval * last_mark < max_visible) {
			last_mark++;
		}
	}
}




void ProfileView::draw_y_grid_inside(void)
{
	const int w = this->viewport2d->central_get_width();
	const int h = this->viewport2d->central_get_height();

	const double visible_range = this->y_max_visible - this->y_min_visible;
	if (visible_range < 0.000001) {
		qDebug() << SG_PREFIX_E << "Zero visible range:" << this->y_min_visible << this->y_max_visible;
		return;
	}

	int first_mark = 0;
	int last_mark = 0;
	find_grid_line_indices(this->y_min_visible, this->y_max_visible, this->y_interval, first_mark, last_mark);

#if 0   /* Debug. */
	qDebug() << "===== drawing y grid for graph" << this->get_graph_title() << ", height =" << h;
	qDebug() << "      min/max y visible:" << this->y_min_visible << this->y_max_visible;
	qDebug() << "      interval =" << this->y_interval << ", first_mark/last_mark =" << first_mark << last_mark;
#endif

	for (int i = first_mark; i <= last_mark; i++) {
		const double axis_mark_uu = this->y_interval * i;

		/* 'row' is in "beginning in bottom-left corner" coordinate system. */
		/* Purposefully use "1.0 *" to enforce conversion to
		   float, to avoid losing data during integer division. */
		const int row = (axis_mark_uu - this->y_min_visible) * 1.0 * h / (visible_range * 1.0);

		if (row >= 0 && row < h) {
			this->viewport2d->central_draw_line(this->viewport2d->central->grid_pen,
							    0, row,
							    w, row);
		} else {
			//qDebug() << SG_PREFIX_D << "      value (outside) =" << axis_mark_uu << ", row =" << row;
		}
	}
}




void ProfileView::draw_y_grid_outside(void)
{
	const int w = this->viewport2d->left_get_width();
	const int h = this->viewport2d->left_get_height();

	const double visible_range = this->y_max_visible - this->y_min_visible;
	if (visible_range < 0.000001) {
		qDebug() << SG_PREFIX_E << "Zero visible range:" << this->y_min_visible << this->y_max_visible;
		return;
	}

	int first_mark = 0;
	int last_mark = 0;
	find_grid_line_indices(this->y_min_visible, this->y_max_visible, this->y_interval, first_mark, last_mark);

#if 1
	qDebug() << "===== drawing y grid for graph" << this->get_graph_title() << ", height =" << h;
	qDebug() << "      min/max y visible:" << this->y_min_visible << this->y_max_visible;
	qDebug() << "      interval =" << this->y_interval << ", first_mark/last_mark =" << first_mark << last_mark;
#endif

	for (int i = first_mark; i <= last_mark; i++) {
		const double axis_mark_uu = this->y_interval * i;

		/* 'row' is in "beginning in bottom-left corner" coordinate system. */
		/* Purposefully use "1.0 *" to enforce conversion to
		   float, to avoid losing data during integer division. */
		const int row = (axis_mark_uu - this->y_min_visible) * 1.0 * h / (visible_range * 1.0);

		if (row >= 0 && row < h) {
			//qDebug() << SG_PREFIX_D << "      value (inside) =" << axis_mark_uu << ", row =" << row;
			const QRectF bounding_rect = QRectF(0, h - row, w - 3, h - 3);
			const QString label = this->get_y_grid_label(axis_mark_uu);
			this->viewport2d->margin_draw_text(ViewportMargin::Position::Left, this->viewport2d->central->labels_font, this->viewport2d->central->labels_pen, bounding_rect, Qt::AlignRight | Qt::AlignTop, label, SG_TEXT_OFFSET_UP);
		} else {
			//qDebug() << SG_PREFIX_D << "      value (outside) =" << axis_mark_uu << ", row =" << row;
		}
	}
}




void ProfileView::draw_x_grid_sub_d_inside(void)
{
	const int w = this->viewport2d->central_get_width();
	const int h = this->viewport2d->central_get_height();

	const Distance visible_range = this->x_max_visible_d - this->x_min_visible_d;
	if (visible_range.is_zero()) {
		qDebug() << SG_PREFIX_E << "Zero visible range:" << this->x_min_visible_d << this->x_max_visible_d;
		return;
	}

	int first_mark = 0;
	int last_mark = 0;
	find_grid_line_indices(this->x_min_visible_d, this->x_max_visible_d, this->x_interval_d, first_mark, last_mark);

#if 1   /* Debug. */
	qDebug() << "===== drawing x grid for graph" << this->get_graph_title() << ", width =" << w;
	qDebug() << "      min/max d on x axis visible:" << this->x_min_visible_d << this->x_max_visible_d;
	qDebug() << "      interval =" << this->x_interval_d << ", first_mark/last_mark =" << first_mark << last_mark;
#endif

	for (int i = first_mark; i <= last_mark; i++) {
		const Distance axis_mark_uu = this->x_interval_d * i;

		/* 'col' is in "beginning in bottom-left corner" coordinate system. */
		/* Purposefully use "1.0 *" to enforce conversion to
		   float, to avoid losing data during integer division. */
		const int col = (axis_mark_uu - this->x_min_visible_d) * 1.0 * w / (visible_range * 1.0);

		if (col >= 0 && col < w) {
			//qDebug() << SG_PREFIX_D << "      value (inside) =" << axis_mark_uu << ", col =" << col;
			this->viewport2d->central_draw_line(this->viewport2d->central->grid_pen,
							    col, 0,
							    col, 0 + h);

		} else {
			//qDebug() << SG_PREFIX_D << "      value (outside) =" << axis_mark_uu << ", col =" << col;
		}
	}
}




void ProfileView::draw_x_grid_sub_d_outside(void)
{
	const int w = this->viewport2d->bottom_get_width();
	const int h = this->viewport2d->bottom_get_height();

	const Distance visible_range = this->x_max_visible_d - this->x_min_visible_d;
	if (visible_range.is_zero()) {
		qDebug() << SG_PREFIX_E << "Zero visible range:" << this->x_min_visible_d << this->x_max_visible_d;
		return;
	}

	int first_mark = 0;
	int last_mark = 0;
	find_grid_line_indices(this->x_min_visible_d, this->x_max_visible_d, this->x_interval_d, first_mark, last_mark);

#if 1
	qDebug() << "===== drawing x grid for graph" << this->get_graph_title() << ", width =" << w;
	qDebug() << "      min/max d on x axis visible:" << this->x_min_visible_d << this->x_max_visible_d;
	qDebug() << "      interval =" << this->x_interval_d << ", first_mark/last_mark =" << first_mark << last_mark;
#endif

	for (int i = first_mark; i <= last_mark; i++) {
		const Distance axis_mark_uu = this->x_interval_d * i;

		/* 'col' is in "beginning in bottom-left corner" coordinate system. */
		/* Purposefully use "1.0 *" to enforce conversion to
		   float, to avoid losing data during integer division. */
		const int col = (axis_mark_uu - this->x_min_visible_d) * 1.0 * w / (visible_range * 1.0);

		if (col >= 0 && col < w) {
			//qDebug() << SG_PREFIX_D << "      value (inside) =" << axis_mark_uu << ", col =" << col;
			const QRectF bounding_rect = QRectF(col, 0, w - 3, h - 3);
			const QString label = axis_mark_uu.to_nice_string();
			this->viewport2d->margin_draw_text(ViewportMargin::Position::Bottom, this->viewport2d->central->labels_font, this->viewport2d->central->labels_pen, bounding_rect, Qt::AlignLeft | Qt::AlignTop, label, SG_TEXT_OFFSET_LEFT);
		} else {
			//qDebug() << SG_PREFIX_D << "      value (outside) =" << axis_mark_uu << ", col =" << col;
		}
	}
}




void ProfileView::draw_x_grid_sub_t_inside(void)
{
	const int w = this->viewport2d->central_get_width();
	const int h = this->viewport2d->central_get_height();

	const Time visible_range = this->x_max_visible_t - this->x_min_visible_t;
	if (visible_range.is_zero()) {
		qDebug() << SG_PREFIX_E << "Zero visible range:" << this->x_min_visible_t << this->x_max_visible_t;
		return;
	}

	int first_mark = 0;
	int last_mark = 0;
	find_grid_line_indices(this->x_min_visible_t, this->x_max_visible_t, this->x_interval_t, first_mark, last_mark);

#if 1
	qDebug() << "===== drawing x grid for graph" << this->get_graph_title() << ", width =" << w;
	qDebug() << "      min/max t on x axis visible:" << this->x_min_visible_t << this->x_max_visible_t;
	qDebug() << "      interval =" << this->x_interval_t.get_value() << ", first_mark/last_mark =" << first_mark << last_mark;
#endif

	for (int i = first_mark; i <= last_mark; i++) {
		const Time axis_mark_uu = this->x_interval_t * i;

		/* 'col' is in "beginning in bottom-left corner" coordinate system. */
		/* Purposefully use "1.0 *" to enforce conversion to
		   float, to avoid losing data during integer division. */
		const int col = (axis_mark_uu - this->x_min_visible_t) * 1.0 * w / (visible_range * 1.0);

		if (col >= 0 && col < w) {
			//qDebug() << SG_PREFIX_D << "      value (inside) =" << axis_mark_uu << ", col =" << col;
			this->viewport2d->central_draw_line(this->viewport2d->central->grid_pen,
							    col, 0,
							    col, 0 + h);

		} else {
			//qDebug() << SG_PREFIX_D << "      value (outside) =" << axis_mark_uu << ", col =" << col;
		}
	}
}




void ProfileView::draw_x_grid_sub_t_outside(void)
{
	const int w = this->viewport2d->bottom_get_width();
	const int h = this->viewport2d->bottom_get_height();

	const Time visible_range = this->x_max_visible_t - this->x_min_visible_t;
	if (visible_range.is_zero()) {
		qDebug() << SG_PREFIX_E << "Zero visible range:" << this->x_min_visible_t << this->x_max_visible_t;
		return;
	}

	int first_mark = 0;
	int last_mark = 0;
	find_grid_line_indices(this->x_min_visible_t, this->x_max_visible_t, this->x_interval_t, first_mark, last_mark);

#if 1
	qDebug() << "===== drawing x grid for graph" << this->get_graph_title() << ", width =" << w;
	qDebug() << "      min/max t on x axis visible:" << this->x_min_visible_t << this->x_max_visible_t;
	qDebug() << "      interval =" << this->x_interval_t.get_value() << ", first_mark/last_mark =" << first_mark << last_mark;
#endif

	for (int i = first_mark; i <= last_mark; i++) {
		const Time axis_mark_uu = this->x_interval_t * i;

		/* 'col' is in "beginning in bottom-left corner" coordinate system. */
		/* Purposefully use "1.0 *" to enforce conversion to
		   float, to avoid losing data during integer division. */
		const int col = (axis_mark_uu - this->x_min_visible_t) * 1.0 * w / (visible_range * 1.0);

		if (col >= 0 && col < w) {
			//qDebug() << SG_PREFIX_D << "      value (inside) =" << axis_mark_uu << ", col =" << col;
			const QRectF bounding_rect = QRectF(col, 0, w - 3, h - 3);
			const QString label = get_time_grid_label(this->x_interval_t, axis_mark_uu);
			this->viewport2d->margin_draw_text(ViewportMargin::Position::Bottom, this->viewport2d->central->labels_font, this->viewport2d->central->labels_pen, bounding_rect, Qt::AlignLeft | Qt::AlignTop, label, SG_TEXT_OFFSET_LEFT);
		} else {
			//qDebug() << SG_PREFIX_D << "      value (outside) =" << axis_mark_uu << ", col =" << col;
		}
	}
}




QString ProfileView::get_y_grid_label(float value_uu)
{
	switch (this->viewport2d->y_domain) {
	case ViewportDomain::Elevation:
		return Altitude(value_uu, this->viewport2d->height_unit).to_string();

	case ViewportDomain::Distance:
		return Distance(value_uu, this->viewport2d->distance_unit).to_string();

	case ViewportDomain::Speed:
		return Speed(value_uu, this->viewport2d->speed_unit).to_string();

	case ViewportDomain::Gradient:
		return Gradient(value_uu).to_string();

	default:
		qDebug() << SG_PREFIX_E << "Unhandled y domain" << (int) this->viewport2d->y_domain;
		return "";
	}
}




void ProfileView::draw_x_grid_inside(const Track * trk)
{
	switch (this->viewport2d->x_domain) {
	case ViewportDomain::Time:
		this->draw_x_grid_sub_t_inside();
		break;

	case ViewportDomain::Distance:
		this->draw_x_grid_sub_d_inside();
		break;

	default:
		qDebug() << SG_PREFIX_E << "Unhandled x domain" << (int) this->viewport2d->x_domain;
		break;
	}
}




void ProfileView::draw_x_grid_outside(const Track * trk)
{
	switch (this->viewport2d->x_domain) {
	case ViewportDomain::Time:
		this->draw_x_grid_sub_t_outside();
		break;

	case ViewportDomain::Distance:
		this->draw_x_grid_sub_d_outside();
		break;

	default:
		qDebug() << SG_PREFIX_E << "Unhandled x domain" << (int) this->viewport2d->x_domain;
		break;
	}
}




bool ProfileView::supported_domains(ViewportDomain x_domain, ViewportDomain y_domain)
{
	switch (x_domain) {
	case ViewportDomain::Distance:
		switch (y_domain) {
		case ViewportDomain::Elevation:
		case ViewportDomain::Gradient:
		case ViewportDomain::Speed:
			return true;
		default:
			return false;
		}
	case ViewportDomain::Time:
		switch (y_domain) {
		case ViewportDomain::Speed:
		case ViewportDomain::Distance:
		case ViewportDomain::Elevation:
			return true;

		default:
			return false;
		}
	default:
		return false;
	}
}
