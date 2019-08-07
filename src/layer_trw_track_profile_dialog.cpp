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
static void real_time_label_update(ProfileView * view, const Trackpoint * tp);

static QString get_time_grid_label(const Time & interval_value, const Time & value);




TrackProfileDialog::~TrackProfileDialog()
{
	for (auto iter = this->views.begin(); iter != this->views.end(); iter++) {
		delete *iter;
	}
}




sg_ret ProfileView::regenerate_track_data_to_draw(Track * trk)
{
	this->track_data_to_draw.invalidate();

	/*
	  ::initial_track_data has been generated once, when the
	  dialog has been opened. Now compress it to limit number of
	  drawn points.
	*/
	const int compressed_n_points = this->get_central_n_columns();
	this->track_data_to_draw = this->initial_track_data.compress(compressed_n_points);
	this->track_data_to_draw.apply_unit_conversions(this->graph_2d->speed_unit, this->graph_2d->distance_unit, this->graph_2d->height_unit);

	if (!this->track_data_to_draw.valid) {
		qDebug() << SG_PREFIX_E << "Failed to regenerate valid compressed track data for" << this->get_title();
		return sg_ret::err;
	}
	qDebug() << SG_PREFIX_I << "Regenerated valid compressed track data for" << this->get_title();


	/* Do necessary adjustments to y values. */



	qDebug() << SG_PREFIX_I << "Initial track data" << this->get_title() << this->initial_track_data;
	qDebug() << SG_PREFIX_I << "Track data to draw" << this->get_title() << this->track_data_to_draw;


	/* Do necessary adjustments to x values. */
	sg_ret result = sg_ret::err;
	switch (this->graph_2d->x_domain) {
	case GisViewportDomain::Distance:
		result = this->set_initial_visible_range_x_distance();
		break;
	case GisViewportDomain::Time:
		result = this->set_initial_visible_range_x_time();
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled x domain" << (int) this->graph_2d->x_domain;
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

	if (!this->track_data_to_draw.valid || NULL == this->track_data_to_draw.x || NULL == this->track_data_to_draw.y) {
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
	const Distance x_min = Distance(this->track_data_to_draw.x_min);
	const Distance x_max = Distance(this->track_data_to_draw.x_max);
	this->x_min_visible_d = x_min.convert_to_unit(this->graph_2d->distance_unit);
	this->x_max_visible_d = x_max.convert_to_unit(this->graph_2d->distance_unit);

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
	this->x_min_visible_t = this->track_data_to_draw.x_min;
	this->x_max_visible_t = this->track_data_to_draw.x_max;
#else
	/* Instead of x_min/x_max use first and last timestamp.

	   FIXME: this is still not perfect solution: the glitch in
	   timestamp may occur in first or last trackpoint. Find a
	   good way to find a correct first/last timestamp. */
	this->x_min_visible_t = this->track_data_to_draw.x[0];
	this->x_max_visible_t = this->track_data_to_draw.x[this->track_data_to_draw.n_points - 1];
#endif

	const Time visible_range = this->x_max_visible_t - this->x_min_visible_t;
	if (visible_range.is_zero()) {
		qDebug() << SG_PREFIX_E << "Zero time span: min/max x = " << this->x_min_visible_t << this->x_max_visible_t << this->get_title();
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
	const double y_margin = 0.05; /* There is no deep reasoning behind this particular value. */

	/* This is not exactly the same range as this->y_visible_range_uu
	   calculated below. */
	const double y_data_range = std::fabs(this->track_data_to_draw.y_max - this->track_data_to_draw.y_min);

	switch (this->graph_2d->y_domain) {
	case GisViewportDomain::Speed:
	case GisViewportDomain::Distance:
		/* Some graphs better start at zero, e.g. speed graph
		   or distance graph. Showing negative speed values on
		   a graph wouldn't make sense. */
		this->y_visible_min = this->track_data_to_draw.y_min;
		break;
	case GisViewportDomain::Elevation:
	case GisViewportDomain::Gradient:
		this->y_visible_min = this->track_data_to_draw.y_min - y_data_range * y_margin;
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled y domain" << (int) this->graph_2d->y_domain;
		return sg_ret::err;
	}
	this->y_visible_max = this->track_data_to_draw.y_max + y_data_range * y_margin;
	this->y_visible_range_uu = std::fabs(this->y_visible_max - this->y_visible_min);


	this->x_visible_range_uu = std::fabs(this->track_data_to_draw.x_max - this->track_data_to_draw.x_min);


	/* Now, given the n_intervals value, find a suitable interval
	   index and value that will nicely cover visible range of
	   data. */
	const int n_intervals = GRAPH_Y_INTERVALS;
	int interval_index = 0;

	switch (this->graph_2d->y_domain) {
	case GisViewportDomain::Speed:
		interval_index = speed_intervals.intervals.get_interval_index(this->y_visible_min, this->y_visible_max, n_intervals);
		this->y_interval = speed_intervals.intervals.values[interval_index];
		break;
	case GisViewportDomain::Elevation:
		interval_index = altitude_intervals.intervals.get_interval_index(this->y_visible_min, this->y_visible_max, n_intervals);
		this->y_interval = altitude_intervals.intervals.values[interval_index];
		break;
	case GisViewportDomain::Distance:
		interval_index = distance_intervals.intervals.get_interval_index(this->y_visible_min, this->y_visible_max, n_intervals);
		this->y_interval = distance_intervals.intervals.values[interval_index].value;
		break;
	case GisViewportDomain::Gradient:
		interval_index = gradient_intervals.intervals.get_interval_index(this->y_visible_min, this->y_visible_max, n_intervals);
		this->y_interval = gradient_intervals.intervals.values[interval_index];
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled y domain" << (int) this->graph_2d->y_domain;
		return sg_ret::err;
	};


	return sg_ret::ok;
}




/**
   Change what is displayed in main GIS viewport in reaction to click
   event in one of Profile Dialog graphs.
*/
sg_ret TrackProfileDialog::set_center_at_selected_tp(QMouseEvent * ev, const Graph2D * graph_2d, int graph_2d_central_n_columns)
{
	const int graph_2d_leftmost_pixel = graph_2d->central_get_leftmost_pixel();
	const int graph_2d_rightmost_pixel = graph_2d->central_get_rightmost_pixel();


	const int event_x = ev->x();
	if (event_x < graph_2d_leftmost_pixel) {
		qDebug() << SG_PREFIX_E << "Event x =" << event_x << "is beyond left border =" << graph_2d_leftmost_pixel;
		return sg_ret::err;
	}
	if (event_x > graph_2d_rightmost_pixel) {
		qDebug() << SG_PREFIX_E << "Event x =" << event_x << "is beyond right border =" << graph_2d_rightmost_pixel;
		return sg_ret::err;
	}


	/*
	  Coordinate 'x' of event in central area of graph, where
	  bottom-left corner of the center area is a beginning of
	  coordinate system.
	*/
	const int center_cbl_x = event_x - graph_2d->central_get_leftmost_pixel();

	bool found = false;
	switch (graph_2d->x_domain) {
	case GisViewportDomain::Time:
		found = this->trk->select_tp_by_percentage_time((double) center_cbl_x / graph_2d_central_n_columns, SELECTED);
		break;
	case GisViewportDomain::Distance:
		found = this->trk->select_tp_by_percentage_dist((double) center_cbl_x / graph_2d_central_n_columns, NULL, SELECTED);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled x domain" << (int) graph_2d->x_domain;
		return sg_ret::err;
	}


	if (found) {
		Trackpoint * tp = this->trk->get_selected_tp();
		this->main_gisview->set_center_coord(tp->coord);
		this->trw->emit_tree_item_changed("TRW - Track Profile Dialog - set center");
		return sg_ret::ok;
	} else {
		return sg_ret::err;
	}
}




/**
   Draw two pairs of horizontal and vertical lines intersecting at given position.

   One pair is for position of selected trackpoint.
   The other pair is for current position of cursor.

   Both "pos" arguments should indicate position in graph's coordinate system.
*/
sg_ret ProfileView::draw_crosshairs(const Crosshair2D & selected_tp, const Crosshair2D & cursor_pos)
{
	if (!selected_tp.valid && !cursor_pos.valid) {
		/* Perhaps this should be an error, maybe we shouldn't
		   call the function when both positions are
		   invalid. */
		return sg_ret::ok;
	}


	/* Restore previously saved image that has no crosshairs on
	   it, just the graph, grids, borders and margins. */
	if (this->graph_2d->saved_pixmap_valid) {
		/* Debug code. */
		// qDebug() << SG_PREFIX_I << "Restoring saved image";
		this->graph_2d->set_pixmap(this->graph_2d->saved_pixmap);
	} else {
		qDebug() << SG_PREFIX_W << "NOT restoring saved image";
	}



	/*
	  Now draw crosshairs on this fresh (restored from saved) image.
	*/
	if (selected_tp.valid) {
		this->graph_2d->central_draw_simple_crosshair(selected_tp);
	}
	if (cursor_pos.valid) {
		this->graph_2d->central_draw_simple_crosshair(cursor_pos);
	}



	/*
	  From the test made on top of the function we know that at
	  least one crosshair has been painted. This call will call
	  GisViewport::paintEvent(), triggering final render to
	  screen.
	*/
	this->graph_2d->update();

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
   Draw crosshair for this trackpoint.
*/
void TrackProfileDialog::handle_mouse_button_release_cb(ViewportPixmap * vpixmap, QMouseEvent * ev)
{
	Graph2D * graph_2d = (Graph2D *) vpixmap;
	ProfileView * view = this->get_current_view();
	assert (view->graph_2d == graph_2d);



	qDebug() << SG_PREFIX_I << "Crosshair. Mouse event at" << ev->x() << ev->y() << "(cbl ="
		 << ev->x() - graph_2d->central_get_leftmost_pixel()
		 << graph_2d->central_get_bottommost_pixel() - ev->y() << ")";



	const sg_ret tp_lookup = this->set_center_at_selected_tp(ev,
								 graph_2d,
								 view->get_central_n_columns());
	if (sg_ret::ok != tp_lookup) {
		/* Unable to get the point so give up. */
		this->button_split_at_marker->setEnabled(false);
		return;
	}

	this->button_split_at_marker->setEnabled(true);

	bool is_selected_tp_crosshair = false; /* TODO: calculate value of this flag based on 'selected_tp' being valid below. */

	/* Attempt to redraw crosshair on all graphs. Notice that this
	   does not redraw full graphs, just crosshairs.

	   This is done on all graphs because we want to have 'mouse
	   release' event reflected in all graphs. */
	Crosshair2D cursor_pos; /* Noninitialized == invalid. Don't draw cursor position on clicks. */
	for (auto iter = this->views.begin(); iter != this->views.end(); iter++) {
		ProfileView * view_iter = *iter;
		if (!view_iter->graph_2d) {
			continue;
		}

		if (!view_iter->track_data_to_draw.valid) {
			/* We didn't visit that tab yet, so track data
			   hasn't been generated for current graph
			   width. */
			/* FIXME: generate the track data so that we
			   can set a crosshair over there. */
			continue;
		}


	        Crosshair2D selected_tp = view_iter->get_position_of_tp(this->trk, SELECTED);
		selected_tp.debug = "selected tp 3";
		if (!selected_tp.valid) {
			continue;
		}

		/*
		  Positions passed to draw_crosshairs() are in 2D
		  graph's coordinate system (beginning in bottom left
		  corner), not Qt's coordinate system (beginning in
		  upper left corner).
		*/
		view_iter->draw_crosshairs(selected_tp, cursor_pos);
	}


	this->button_split_at_marker->setEnabled(is_selected_tp_crosshair);

	return;
}




sg_ret ProfileView::cbl_find_y_on_graph_line(const int cbl_x, int & cbl_y)
{
	const int n_rows = this->graph_2d->central_get_n_rows();
	const int n_cols = this->graph_2d->central_get_n_columns();

	/*
	  Ensure x is inside of graph.

	  Here we call functions that return pixels in "beginning in
	  upper-left corner" coordinate system.  Since here we verify
	  position of 'x' that is also inn "beginning in lower-left
	  corner" coordinate system, this should be safe, because in
	  both coordinate systems beginning is on the left.
	*/
	const int leftmost_pixel = this->graph_2d->central_get_leftmost_pixel();
	const int rightmost_pixel = this->graph_2d->central_get_rightmost_pixel();
	if (cbl_x < leftmost_pixel) {
		qDebug() << SG_PREFIX_E << "x too far to left:" << cbl_x << leftmost_pixel;
		return sg_ret::err;
	}
	if (cbl_x > rightmost_pixel) {
		qDebug() << SG_PREFIX_E << "x too far to right:" << cbl_x << rightmost_pixel;
		return sg_ret::err;
	}


	if (cbl_x > n_cols) {
		qDebug() << SG_PREFIX_E << "cbl x too large:" << cbl_x << n_cols;
		assert (cbl_x <= n_cols);
	}


	cbl_y = n_rows * (this->track_data_to_draw.y[cbl_x] - this->y_visible_min) / this->y_visible_range_uu;

	return sg_ret::ok;
}




Crosshair2D ProfileView::get_cursor_pos_on_line(QMouseEvent * ev)
{
	Crosshair2D crosshair;

	const int leftmost_px = this->graph_2d->central_get_leftmost_pixel();

	crosshair.central_cbl_x = ev->x() - leftmost_px;

	/* Find 'y' position that lays on a graph line. */
	this->cbl_find_y_on_graph_line(crosshair.central_cbl_x, crosshair.central_cbl_y);

	/*
	  Use coordinates of point that is
	  a) limited to central area of 2d graph (so as if margins outside of the central area didn't exist),
	  b) is in 'beginning in bottom-left' coordinate system (cbl)
	  to calculate global, 'beginning in top-left' coordinates.
	*/
	crosshair.x = crosshair.central_cbl_x + this->graph_2d->central_get_leftmost_pixel();
	crosshair.y = this->graph_2d->central_get_bottommost_pixel() - crosshair.central_cbl_y;

	crosshair.valid = true;

	return crosshair;
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




void real_time_label_update(ProfileView * view, Track * trk)
{
	if (NULL == view->labels.t_value) {
		/* This function shouldn't be called for views that don't have the T label. */
		qDebug() << SG_PREFIX_W << "Called the function, but label is NULL";
		return;
	}

	const Trackpoint * tp = trk->get_hovered_tp();
	if (NULL == tp) {
		return;
	}

	view->labels.t_value->setText(tp->timestamp.to_timestamp_string(Qt::LocalTime));

	return;
}




void TrackProfileDialog::handle_cursor_move_cb(ViewportPixmap * vpixmap, QMouseEvent * ev)
{
	Graph2D * graph_2d = (Graph2D *) vpixmap;
	ProfileView * view = this->get_current_view();
	assert (view->graph_2d == graph_2d);

	if (!view->track_data_to_draw.valid) {
		qDebug() << SG_PREFIX_E << "Not handling cursor move, track data invalid";
		return;
	}

	double meters_from_start = 0.0;


	/* Crosshair laying on a graph, with 'x' position matching current 'x' position of cursor. */
	Crosshair2D cursor_pos = view->get_cursor_pos_on_line(ev);
	if (!cursor_pos.valid) {
		qDebug() << SG_PREFIX_E << "Failed to get cursor position on line";
		return;
	}


	/*
	  Crosshair laying on a graph where currently selected tp is
	  located.

	  At the beginning there will be no selected tp, so this
	  function will return !ok. This is fine, ignore the !ok
	  status and continue to drawing current position.

	  TODO_OPTIMIZATION: there is no need to find a selected
	  trackpoint every time a cursor moves. Only current
	  trackpoint (trackpoint under moving cursor mouse) is
	  changing. The trackpoint selected by previous cursor click
	  is does not change.
	*/
	Crosshair2D selected_tp = view->get_position_of_tp(this->trk, SELECTED);
	selected_tp.debug = "selected tp 1";


	switch (view->graph_2d->x_domain) {
	case GisViewportDomain::Distance:
		this->trk->select_tp_by_percentage_dist((double) cursor_pos.central_cbl_x / view->get_central_n_columns(), &meters_from_start, HOVERED);
		view->draw_crosshairs(selected_tp, cursor_pos);

		if (view->labels.x_value) {
			const Distance distance(meters_from_start, SupplementaryDistanceUnit::Meters);
			view->labels.x_value->setText(distance.convert_to_unit(Preferences::get_unit_distance()).to_string());
		}
		break;

	case GisViewportDomain::Time:
		this->trk->select_tp_by_percentage_time((double) cursor_pos.central_cbl_x / view->get_central_n_columns(), HOVERED);
		view->draw_crosshairs(selected_tp, cursor_pos);

		if (view->labels.x_value) {
			time_t seconds_from_start = 0;
			this->trk->get_tp_relative_timestamp(seconds_from_start, HOVERED);
			time_label_update(view->labels.x_value, seconds_from_start);
		}

		real_time_label_update(view, this->trk);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled x domain" << (int) view->graph_2d->x_domain;
		break;
	};


	double y = view->track_data_to_draw.y[cursor_pos.central_cbl_x]; /* FIXME: use proper value for array index. */
	switch (view->graph_2d->y_domain) {
	case GisViewportDomain::Speed:
		if (view->labels.y_value) {
			/* Even if GPS speed available (tp->speed), the text will correspond to the speed map shown.
			   No conversions needed as already in appropriate units. */
			view->labels.y_value->setText(Speed::to_string(y));
		}
		break;
	case GisViewportDomain::Elevation:
		if (view->labels.y_value && NULL != this->trk->get_hovered_tp()) {
			/* Recalculate value into target unit. */
			view->labels.y_value->setText(this->trk->get_hovered_tp()->altitude
						      .convert_to_unit(Preferences::get_unit_height())
						      .to_string());
		}
		break;
	case GisViewportDomain::Distance:
		if (view->labels.y_value) {
			const Distance distance_uu(y, Preferences::get_unit_distance()); /* 'y' is already recalculated to user unit, so this constructor must use user unit as well. */
			view->labels.y_value->setText(distance_uu.to_string());
		}
		break;
	case GisViewportDomain::Gradient:
		if (view->labels.y_value) {
			const Gradient gradient_uu(y);
			view->labels.y_value->setText(gradient_uu.to_string());
		}
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled y domain" << (int) view->graph_2d->y_domain;
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
	const double max_function_value_dem = this->y_visible_max;

	const QColor dem_color = this->dem_alt_pen.color();
	const QColor speed_color = this->gps_speed_pen.color();

	const int n_columns = this->graph_2d->central_get_n_columns();
	const int n_rows = this->graph_2d->central_get_n_rows();

	for (auto iter = std::next(trk->trackpoints.begin()); iter != trk->trackpoints.end(); iter++) {

		current_function_arg += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);
		if (do_dem) {
			const Altitude elev = DEMCache::get_elev_by_coord((*iter)->coord, DemInterpolation::Best);
			if (!elev.is_valid()) {
				continue;
			}

			const double elev_value_uu = elev.convert_to_unit(this->graph_2d->height_unit).get_value();

			/* offset is in current height units. */
			const double current_function_value_uu = elev_value_uu - this->y_visible_min;

			const int x = n_columns * current_function_arg / max_function_arg;
			const int y = 0 - n_rows * current_function_value_uu / max_function_value_dem;
			this->graph_2d->fill_rectangle(dem_color, x - 2, y - 2, 4, 4);
		}

		if (do_speed) {
			if (std::isnan((*iter)->speed)) {
				continue;
			}

			const double current_function_value = (*iter)->speed;

			/* This is just a speed indicator - no actual values can be inferred by user. */
			const int x = n_columns * current_function_arg / max_function_arg;
			const int y = 0 - n_rows * current_function_value / max_function_value_speed;
			this->graph_2d->fill_rectangle(speed_color, x - 2, y - 2, 4, 4);
		}
	}
}




/**
   @reviewed-on tbd
*/
sg_ret ProfileView::draw_function_values(Track * trk)
{
	const TrackData & trk_data = this->track_data_to_draw;
	const size_t n_values = trk_data.n_points;
	if (0 == n_values) {
		qDebug() << SG_PREFIX_N << "There were zero values in" << graph_2d->debug;
		return sg_ret::err;
	}
	/*
	  It would be tempting to add "assert (n_columns ==
	  n_values)", but this assertion would fail most of the
	  time. We may have uncompressed track data set, we may have
	  zoomed in to the point where there will be only few track
	  data points (trackpoints) visible.
	*/


	const int n_columns = this->graph_2d->central_get_n_columns();
	const int n_rows = this->graph_2d->central_get_n_rows();

	const int leftmost_px = this->graph_2d->central_get_leftmost_pixel();
	const int rightmost_px = this->graph_2d->central_get_rightmost_pixel();
	const int bottommost_px = this->graph_2d->central_get_bottommost_pixel();


	QPen valid_pen;
	valid_pen.setColor(trk->has_color ? trk->color : "blue");
	valid_pen.setWidth(1);

	QPen invalid_pen;
	invalid_pen.setColor(trk->color == "red" ? "black" : "red");
	invalid_pen.setWidth(1);


	qDebug() << SG_PREFIX_I
		 << "Will draw graph" << this->graph_2d->debug
		 << "with n values =" << n_values
		 << "into n columns =" << n_columns;

	const double y_pixels_per_unit = n_rows / this->y_visible_range_uu;
	const double x_pixels_per_unit = n_columns / this->x_visible_range_uu;

	ScreenPos cur_valid_pos;
	ScreenPos last_valid_pos(leftmost_px, bottommost_px);

	for (size_t i = 0; i < n_values; i++) {

		const double x_current_value_uu = trk_data.x[i];
		/*
		  This line creates x coordinate that is
		  proportionally as far from left border, as
		  trk_data.x[i] is from trk_data.x_min.

		  It works equally well for x vectors with constant
		  intervals (e.g. the same time interval == 1s between
		  consecutive measurements of y), as for x vector with
		  varying intervals (e.g. different values of
		  distances between consecutive measurements of y).
		*/
		const int x = leftmost_px + x_pixels_per_unit * (x_current_value_uu - trk_data.x_min);

		const bool y_value_valid = !std::isnan(trk_data.y[i]);

		if (y_value_valid) {
			const double y_current_value_uu = trk_data.y[i];

			cur_valid_pos.rx() = x;
			cur_valid_pos.ry() = bottommost_px - y_pixels_per_unit * (y_current_value_uu - trk_data.y_min);

			graph_2d->draw_line(valid_pen, last_valid_pos, cur_valid_pos);

			last_valid_pos = cur_valid_pos;
		} else {
			/*
			  Draw vertical line from bottom of graph to
			  top to indicate invalid y value.
			*/

			const int begin_y = bottommost_px;
			const int end_y = bottommost_px - n_rows;

			graph_2d->draw_line(invalid_pen, x, begin_y, x, end_y);
		}
	}

	return sg_ret::ok;
}




void ProfileViewET::draw_additional_indicators(Track * trk)
{
	const int n_columns = this->graph_2d->central_get_n_columns();
	const int n_rows = this->graph_2d->central_get_n_rows();

	if (this->show_dem_cb && this->show_dem_cb->checkState())  {
		const double max_function_value = this->y_visible_max;

		const QColor color = this->dem_alt_pen.color();

		for (int i = 0; i < n_columns; i++) {
			/* This could be slow doing this each time... */
			const bool found_tp = trk->select_tp_by_percentage_time(((double) i / (double) n_columns), HOVERED);
			if (!found_tp) {
				continue;
			}

			const Trackpoint * tp = trk->get_hovered_tp();
			const Altitude elev = DEMCache::get_elev_by_coord(tp->coord, DemInterpolation::Simple);
			if (!elev.is_valid()) {
				continue;
			}

			const double elev_value_uu = elev.convert_to_unit(Preferences::get_unit_height()).get_value();

			/* offset is in current height units. */
			const double current_function_value_uu = elev_value_uu - this->y_visible_min;

			const int x = i;
			const int y = 0 - n_rows * current_function_value_uu / max_function_value;
			this->graph_2d->fill_rectangle(color, x - 2, y - 2, 4, 4);
		}
	}


	/* Show speeds. */
	if (this->show_speed_cb && this->show_speed_cb->checkState()) {
		/* This is just an indicator - no actual values can be inferred by user. */

		const double max_function_value = trk->get_max_speed().get_value() * 110 / 100;

		const QColor color = this->gps_speed_pen.color();
		for (int i = 0; i < n_columns; i++) {

			const double current_function_value = this->track_data_to_draw.y[i];

			const int x = i;
			const int y = 0 - n_rows * current_function_value / max_function_value;
			this->graph_2d->fill_rectangle(color, x - 2, y - 2, 4, 4);
		}
	}

	return;
}




void ProfileViewSD::draw_additional_indicators(Track * trk)
{
	if (this->show_gps_speed_cb && this->show_gps_speed_cb->checkState()) {

		const int n_columns = this->graph_2d->central_get_n_columns();
		const int n_rows = this->graph_2d->central_get_n_rows();

		const double max_function_arg = trk->get_length_value_including_gaps();
		const double max_function_value = this->y_visible_max;
		double current_function_arg = 0.0;
		double current_function_value = 0.0;

		const QColor color = this->gps_speed_pen.color();
		for (auto iter = std::next(trk->trackpoints.begin()); iter != trk->trackpoints.end(); iter++) {
			double gps_speed = (*iter)->speed;
			if (std::isnan(gps_speed)) {
				continue;
			}

			gps_speed = Speed::convert_mps_to(gps_speed, this->graph_2d->speed_unit);

			current_function_arg += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);
			current_function_value = gps_speed - this->y_visible_min;

			const int x = n_columns * current_function_arg / max_function_arg;
			const int y = 0 - n_rows * current_function_value / max_function_value;
			this->graph_2d->fill_rectangle(color, x - 2, y - 2, 4, 4);
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

		const int n_columns = this->graph_2d->central_get_n_columns();
		const int n_rows = this->graph_2d->central_get_n_rows();

		Time ts_begin;
		Time ts_end;
		if (sg_ret::ok == trk->get_timestamps(ts_begin, ts_end)) {

			const time_t time_begin = ts_begin.get_value();
			const time_t time_end = ts_end.get_value();

			const time_t max_function_arg = time_end - time_begin;
			const double max_function_value = this->y_visible_max;

			const QColor color = this->gps_speed_pen.color();

			for (auto iter = trk->trackpoints.begin(); iter != trk->trackpoints.end(); iter++) {
				double gps_speed = (*iter)->speed;
				if (std::isnan(gps_speed)) {
					continue;
				}

				gps_speed = Speed::convert_mps_to(gps_speed, this->graph_2d->speed_unit);

				const time_t current_function_arg = (*iter)->timestamp.get_value() - time_begin;
				const double current_function_value = gps_speed - this->y_visible_min;

				const int x = n_columns * current_function_arg / max_function_arg;
				const int y = 0 - n_rows * current_function_value / max_function_value;
				this->graph_2d->fill_rectangle(color, x - 2, y - 2, 4, 4);
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

		const int n_columns = this->graph_2d->central_get_n_columns();
		const int n_rows = this->graph_2d->central_get_n_rows();

		const QColor color = this->gps_speed_pen.color();
		/* This is just an indicator - no actual values can be inferred by user. */
		for (int i = 0; i < n_columns; i++) {

			const double current_function_value = this->track_data_to_draw.y[i];

			const int x = i;
			const int y = 0 - n_rows * current_function_value / max_function_value;
			this->graph_2d->fill_rectangle(color, x - 2, y - 2, 4, 4);
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
sg_ret ProfileView::draw_graph_without_crosshairs(Track * trk)
{
	qDebug() << SG_PREFIX_I;

	if (this->graph_2d->x_domain == GisViewportDomain::Time) {
		const Time duration = trk->get_duration(true);
		if (!duration.is_valid() || duration.get_value() <= 0) {
			qDebug() << SG_PREFIX_E << "return 1";
			return sg_ret::err;
		}
	}

	if (sg_ret::ok != this->regenerate_track_data_to_draw(trk)) {
		qDebug() << SG_PREFIX_E << "return 2";
		return sg_ret::err;
	}

	/* Clear before redrawing. */
	this->graph_2d->clear();

	this->draw_function_values(trk);

	/* Draw grid on top of graph of values. */
	this->draw_x_grid(trk);
	this->draw_y_grid();

	this->graph_2d->central_draw_outside_boundary_rect();

	this->draw_additional_indicators(trk);

	/* This will call ::paintEvent(), triggering final render to screen. */
	this->graph_2d->update();

	/* The pixmap = margin + graph area. */
	qDebug() << SG_PREFIX_I << "Saving graph" << this->graph_2d->debug;
	this->graph_2d->saved_pixmap = this->graph_2d->get_pixmap();
	this->graph_2d->saved_pixmap_valid = true;

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

	const int n_columns = this->get_central_n_columns();
	const int n_rows = this->get_central_n_rows();

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
		const int x = n_columns * current_function_arg / max_function_arg;
		const int y = 0 - n_rows * current_function_value / max_function_value;
		this->graph_2d->fill_rectangle(color, x - 2, y - 2, 4, 4);
	}
}




/**
   Look up view
*/
ProfileView * TrackProfileDialog::find_view(Graph2D * graph_2d) const
{
	for (auto iter = this->views.begin(); iter != this->views.end(); iter++) {
		ProfileView * view = *iter;
		if (view->graph_2d == graph_2d) {
			return view;
		}
	}
	return NULL;
}




/**
   Draw all graphs
*/
void TrackProfileDialog::draw_all_views(bool resized)
{
	for (auto iter = this->views.begin(); iter != this->views.end(); iter++) {
		ProfileView * view = *iter;
		if (!view->graph_2d) {
			continue;
		}

		/* If dialog window is resized then saved image is no longer valid. */
		view->graph_2d->saved_pixmap_valid = !resized;
		view->draw_track_and_crosshairs(this->trk);
	}
}




Crosshair2D ProfileView::get_position_of_tp(Track * trk, tp_idx tp_idx)
{
	double pc = NAN;
	Crosshair2D crosshair; /* Initially invalid. */

	Trackpoint * tp = trk->get_tp(tp_idx);
	if (NULL == tp) {
		return crosshair;
	}

	switch (this->graph_2d->x_domain) {
	case GisViewportDomain::Time:
		pc = trk->get_tp_time_percent(tp_idx);
		break;
	case GisViewportDomain::Distance:
		pc = trk->get_tp_distance_percent(tp_idx);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled x domain" << (int) this->graph_2d->x_domain;
		/* pc = NAN */
		break;
	}

	if (!std::isnan(pc)) {
		crosshair.central_cbl_x = pc * this->get_central_n_columns();             /* Find x. */
		this->cbl_find_y_on_graph_line(crosshair.central_cbl_x, crosshair.central_cbl_y); /* Find y. */

		/*
		  Use coordinates of point that is
		  a) limited to central area of 2d graph (so as if margins outside of the central area didn't exist),
		  b) is in 'beginning in bottom-left' coordinate system (cbl)
		  to calculate global, 'beginning in top-left' coordinates.
		*/
		crosshair.x = crosshair.central_cbl_x + this->graph_2d->central_get_leftmost_pixel();
		crosshair.y = this->graph_2d->central_get_bottommost_pixel() - crosshair.central_cbl_y;

		crosshair.valid = true;
	}

	// qDebug() << SG_PREFIX_D << "returning pos of tp idx" << tp_idx;

	return crosshair;
}




sg_ret ProfileView::draw_track_and_crosshairs(Track * trk)
{
	sg_ret ret_trk;
	sg_ret ret_marks;

	sg_ret ret = this->draw_graph_without_crosshairs(trk);
	if (sg_ret::ok != ret) {
		qDebug() << SG_PREFIX_E << "Failed to draw graph without crosshairs";
		return ret;
	}


	/* Draw crosshairs. */
	if (1) {
		Crosshair2D cursor_pos = this->get_position_of_tp(trk, HOVERED);
		cursor_pos.debug = "cursor pos";
		Crosshair2D selected_tp = this->get_position_of_tp(trk, SELECTED);
		selected_tp.debug = "selected tp 2";

		ret = this->draw_crosshairs(selected_tp, cursor_pos);
		if (sg_ret::ok != ret) {
			qDebug() << SG_PREFIX_E << "Failed to draw crosshairs";
		}
	}


	return ret;
}




sg_ret TrackProfileDialog::handle_graph_resize_cb(ViewportPixmap * pixmap)
{
	Graph2D * graph_2d = (Graph2D *) pixmap;
	qDebug() << SG_PREFIX_SLOT << "Reacting to signal from graph" << graph_2d->debug;

	ProfileView * view = this->find_view(graph_2d);
	if (!view) {
		qDebug() << SG_PREFIX_E << "Can't find view";
		return sg_ret::err;
	}


	view->graph_2d->cached_central_n_columns = view->graph_2d->central_get_n_columns();
	view->graph_2d->cached_central_n_rows = view->graph_2d->central_get_n_rows();

	view->graph_2d->saved_pixmap_valid = true;
	view->draw_track_and_crosshairs(this->trk);


	return sg_ret::ok;
}




void ProfileView::create_graph_2d(void)
{
	this->graph_2d = new Graph2D(GRAPH_MARGIN_LEFT, GRAPH_MARGIN_RIGHT, GRAPH_MARGIN_TOP, GRAPH_MARGIN_BOTTOM, NULL);
	snprintf(this->graph_2d->debug, sizeof (this->graph_2d->debug), "%s", this->get_title().toUtf8().constData());

#if 0   /* This seems to be unnecessary. */
	qDebug() << SG_PREFIX_I << "Before applying total sizes for graph" << this->graph_2d->debug;
	const int initial_width = GRAPH_INITIAL_WIDTH;
	const int initial_height = GRAPH_INITIAL_HEIGHT;
	this->graph_2d->apply_total_sizes(initial_width, initial_height);
	qDebug() << SG_PREFIX_I << "After applying total sizes for graph" << this->graph_2d->debug;
#endif

	this->graph_2d->x_domain = this->x_domain;
	this->graph_2d->y_domain = this->y_domain;

	return;
}




void TrackProfileDialog::save_values(void)
{
	/* Session settings. */
	ApplicationState::set_integer(VIK_SETTINGS_TRACK_PROFILE_WIDTH, this->profile_width);
	ApplicationState::set_integer(VIK_SETTINGS_TRACK_PROFILE_HEIGHT, this->profile_height);

	/* Just for this session. */
	for (auto iter = this->views.begin(); iter != this->views.end(); iter++) {
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
	sg_ret ret = sg_ret::err;

	/* Note: destroying dialog (eg, parent window exit) won't give "response". */
	switch (resp) {
	case SG_TRACK_PROFILE_CANCEL:
		this->reject();
		break;

	case SG_TRACK_PROFILE_OK:
		this->trk->update_tree_item_properties();
		this->trw->emit_tree_item_changed("TRW - Track Profile Dialog - Profile OK");
		this->accept();
		break;

	case SG_TRACK_PROFILE_SPLIT_AT_MARKER:
		ret = this->trk->split_at_selected_trackpoint_cb();
		if (sg_ret::ok != ret) {
			Dialog::error(tr("Failed to split track. Track unchanged."), this->trw->get_window());
			keep_dialog = true;
		} else {
			this->trw->emit_tree_item_changed("A TRW Track has been split into several tracks (at marker)");
		}
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
	this->draw_all_views(true);
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

	this->main_vbox->addWidget(this->graph_2d);
	this->main_vbox->addLayout(this->labels_grid);
	this->main_vbox->addLayout(this->controls_vbox);

	return;
}




void SlavGPS::track_profile_dialog(Track * trk, GisViewport * main_gisview, QWidget * parent)
{
	TrackProfileDialog dialog(QObject::tr("Track Profile"), trk, main_gisview, parent);
	trk->set_profile_dialog(&dialog);
	dialog.exec();
	trk->clear_profile_dialog();
}




const QString & ProfileView::get_title(void) const
{
	return this->title;
}




void ProfileView::configure_title(void)
{
	if (this->y_domain == GisViewportDomain::Elevation && this->x_domain == GisViewportDomain::Distance) {
		this->title = QObject::tr("Elevation over distance");

	} else if (this->y_domain == GisViewportDomain::Gradient && this->x_domain == GisViewportDomain::Distance) {
		this->title = QObject::tr("Gradient over distance");

	} else if (this->y_domain == GisViewportDomain::Speed && this->x_domain == GisViewportDomain::Time) {
		this->title = QObject::tr("Speed over time");

	} else if (this->y_domain == GisViewportDomain::Distance && this->x_domain == GisViewportDomain::Time) {
		this->title = QObject::tr("Distance over time");

	} else if (this->y_domain == GisViewportDomain::Elevation && this->x_domain == GisViewportDomain::Time) {
		this->title = QObject::tr("Elevation over time");

	} else if (this->y_domain == GisViewportDomain::Speed && this->x_domain == GisViewportDomain::Distance) {
		this->title = QObject::tr("Speed over distance");

	} else {
		qDebug() << SG_PREFIX_E << "Unhandled x/y domain" << (int) this->x_domain << (int) this->y_domain;
	}

	return;
}




TrackProfileDialog::TrackProfileDialog(QString const & title, Track * new_trk, GisViewport * new_main_gisview, QWidget * parent) : QDialog(parent)
{
	this->setWindowTitle(tr("%1 - Track Profile").arg(new_trk->name));

	this->trw = (LayerTRW *) new_trk->get_owning_layer();
	this->trk = new_trk;
	this->main_gisview = new_main_gisview;


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

	this->views.push_back(new ProfileViewED(this));
	this->views.push_back(new ProfileViewGD(this));
	this->views.push_back(new ProfileViewST(this));
	this->views.push_back(new ProfileViewDT(this));
	this->views.push_back(new ProfileViewET(this));
	this->views.push_back(new ProfileViewSD(this));

	this->tabs = new QTabWidget();
	this->tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	this->trk->prepare_for_profile();

	for (auto iter = this->views.begin(); iter != this->views.end(); iter++) {
		ProfileView * view = *iter;
		if (!view) {
			continue;
		}

		view->configure_title();
		view->create_graph_2d();
		view->create_widgets_layout();
		view->configure_labels();
		view->configure_controls();

		this->tabs->addTab(view, view->get_title());

		qDebug() << SG_PREFIX_I << "Configuring signals for graph" << view->graph_2d->debug << "in view" << view->get_title();
		connect(view->graph_2d, SIGNAL (cursor_moved(ViewportPixmap *, QMouseEvent *)),    this, SLOT (handle_cursor_move_cb(ViewportPixmap *, QMouseEvent *)));
		connect(view->graph_2d, SIGNAL (button_released(ViewportPixmap *, QMouseEvent *)), this, SLOT (handle_mouse_button_release_cb(ViewportPixmap *, QMouseEvent *)));
		connect(view->graph_2d, SIGNAL (size_changed(ViewportPixmap *)), this, SLOT (handle_graph_resize_cb(ViewportPixmap *)));
	}


	this->button_box = new QDialogButtonBox();


	this->button_cancel = this->button_box->addButton(tr("&Cancel"), QDialogButtonBox::RejectRole);
	this->button_split_at_marker = this->button_box->addButton(tr("Split at &Marker"), QDialogButtonBox::ActionRole);
	this->button_ok = this->button_box->addButton(tr("&OK"), QDialogButtonBox::AcceptRole);

	this->button_split_at_marker->setEnabled(this->trk->has_selected_tp()); /* Initially no trackpoint is selected. */

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

	int i = 0;
	for (auto iter = this->views.begin(); iter != this->views.end(); iter++) {
		i++;
		ProfileView * view = *iter;
		if (!view) {
			qDebug() << SG_PREFIX_E << "Can't find profile" << i << "in loop";
			continue;
		}
		view->generate_initial_track_data(trk);
	}
}




void ProfileView::configure_labels(void)
{
	switch (this->graph_2d->x_domain) {
	case GisViewportDomain::Distance:
		this->labels.x_label = new QLabel(QObject::tr("Track Distance:"));
		this->labels.x_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);

		break;

	case GisViewportDomain::Time:
		this->labels.x_label = new QLabel(QObject::tr("Time From Start:"));
		this->labels.x_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);

		/* Additional timestamp to provide more information in UI. */
		this->labels.t_label = new QLabel(QObject::tr("Time/Date:"));
		this->labels.t_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);

		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled x domain" << (int) this->graph_2d->x_domain;
		break;
	}


	switch (this->graph_2d->y_domain) {
	case GisViewportDomain::Elevation:
		this->labels.y_label = new QLabel(QObject::tr("Track Height:"));
		this->labels.y_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);

		break;

	case GisViewportDomain::Gradient:
		this->labels.y_label = new QLabel(QObject::tr("Track Gradient:"));
		this->labels.y_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);

		break;

	case GisViewportDomain::Speed:
		this->labels.y_label = new QLabel(QObject::tr("Track Speed:"));
		this->labels.y_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);

		break;

	case GisViewportDomain::Distance:
		this->labels.y_label = new QLabel(QObject::tr("Track Distance:"));
		this->labels.y_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);

		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled y domain" << (int) this->graph_2d->y_domain;
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




ProfileView::ProfileView(GisViewportDomain new_x_domain, GisViewportDomain new_y_domain, TrackProfileDialog * new_dialog, QWidget * parent) : QWidget(parent)
{
	this->dialog = new_dialog;

	if (!ProfileView::supported_domains(new_x_domain, new_y_domain)) {
		qDebug() << SG_PREFIX_E << "Unhandled combination of x/y domains:" << (int) new_x_domain << (int) new_y_domain;
	}

	this->main_pen.setColor("lightsteelblue");
	this->main_pen.setWidth(1);

	this->gps_speed_pen.setColor("red");
	this->dem_alt_pen.setColor("green");
	this->no_alt_info_pen.setColor("yellow");

	this->x_domain = new_x_domain;
	this->y_domain = new_y_domain;


}




ProfileViewET::ProfileViewET(TrackProfileDialog * new_dialog) : ProfileView(GisViewportDomain::Time,     GisViewportDomain::Elevation, new_dialog) {}
ProfileViewSD::ProfileViewSD(TrackProfileDialog * new_dialog) : ProfileView(GisViewportDomain::Distance, GisViewportDomain::Speed,     new_dialog) {}
ProfileViewED::ProfileViewED(TrackProfileDialog * new_dialog) : ProfileView(GisViewportDomain::Distance, GisViewportDomain::Elevation, new_dialog) {}
ProfileViewGD::ProfileViewGD(TrackProfileDialog * new_dialog) : ProfileView(GisViewportDomain::Distance, GisViewportDomain::Gradient,  new_dialog) {}
ProfileViewST::ProfileViewST(TrackProfileDialog * new_dialog) : ProfileView(GisViewportDomain::Time,     GisViewportDomain::Speed,     new_dialog) {}
ProfileViewDT::ProfileViewDT(TrackProfileDialog * new_dialog) : ProfileView(GisViewportDomain::Time,     GisViewportDomain::Distance,  new_dialog) {}



ProfileView::~ProfileView()
{
	delete this->graph_2d;
}




/* Create initial track data using appropriate Track method. */
sg_ret ProfileView::generate_initial_track_data(Track * trk)
{
	this->initial_track_data.invalidate();

	if (this->graph_2d->y_domain == GisViewportDomain::Elevation && this->graph_2d->x_domain == GisViewportDomain::Distance) {
		this->initial_track_data.make_track_data_altitude_over_distance(trk, trk->get_tp_count());

	} else if (this->graph_2d->y_domain == GisViewportDomain::Gradient && this->graph_2d->x_domain == GisViewportDomain::Distance) {
		this->initial_track_data.make_track_data_gradient_over_distance(trk, trk->get_tp_count());

	} else if (this->graph_2d->y_domain == GisViewportDomain::Speed && this->graph_2d->x_domain == GisViewportDomain::Time) {
		this->initial_track_data.make_track_data_speed_over_time(trk);

	} else if (this->graph_2d->y_domain == GisViewportDomain::Distance && this->graph_2d->x_domain == GisViewportDomain::Time) {
		this->initial_track_data.make_track_data_distance_over_time(trk);

	} else if (this->graph_2d->y_domain == GisViewportDomain::Elevation && this->graph_2d->x_domain == GisViewportDomain::Time) {
		this->initial_track_data.make_track_data_altitude_over_time(trk);

	} else if (this->graph_2d->y_domain == GisViewportDomain::Speed && this->graph_2d->x_domain == GisViewportDomain::Distance) {
		this->initial_track_data.make_track_data_speed_over_distance(trk);

	} else {
		qDebug() << SG_PREFIX_E << "Unhandled x/y domain" << (int) this->graph_2d->x_domain << (int) this->graph_2d->y_domain;
	}


	if (!this->initial_track_data.valid) {
		qDebug() << SG_PREFIX_E << "Failed to generate valid initial track data for" << this->get_title();
		return sg_ret::err;
	}

	qDebug() << SG_PREFIX_I << "Generated valid initial track data for" << this->get_title();
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
	/* 'first_mark * y_interval' will be below y_visible_min. */
	if (min_visible <= 0) {
		while (interval * first_mark > min_visible) {
			first_mark--;
		}
	} else {
		while (interval * first_mark + interval < min_visible) {
			first_mark++;
		}
	}

	/* 'last_mark * y_interval' will be above y_visible_max. */
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




void ProfileView::draw_y_grid(void)
{
	if (this->y_visible_range_uu < 0.000001) {
		qDebug() << SG_PREFIX_E << "Zero visible range:" << this->y_visible_range_uu;
		return;
	}

	const int central_n_columns = this->get_central_n_columns();
	const int central_n_rows = this->get_central_n_rows();
	const int left_width = this->graph_2d->left_get_width();
	const int left_height = this->graph_2d->left_get_height();
	const int leftmost_px    = this->graph_2d->central_get_leftmost_pixel();
	const int rightmost_px   = this->graph_2d->central_get_rightmost_pixel();
	const int topmost_px     = this->graph_2d->central_get_topmost_pixel();
	const int bottommost_px  = this->graph_2d->central_get_bottommost_pixel();

	int first_mark = 0;
	int last_mark = 0;
	find_grid_line_indices(this->y_visible_min, this->y_visible_max, this->y_interval, first_mark, last_mark);

#if 0   /* Debug. */
	qDebug() << "===== drawing y grid for graph" << this->get_title() << ", central height =" << central_height;
	qDebug() << "      min/max y visible:" << this->y_visible_min << this->y_visible_max;
	qDebug() << "      interval =" << this->y_interval << ", first_mark/last_mark =" << first_mark << last_mark;
#endif

	for (int i = first_mark; i <= last_mark; i++) {
		const double axis_mark_uu = this->y_interval * i;

		/* Value in 'coordinate system with beginning in
		   bottom-left corner'. */
		/* Purposefully use "1.0 *" to enforce conversion to
		   float, to avoid losing data during integer division. */
		const int cbl_x_value = (axis_mark_uu - this->y_visible_min) * central_n_rows / this->y_visible_range_uu;
		/* Conversion to viewport pixmap's 'coordinate system
		   with beginning in top-left corner' */
		const int row = bottommost_px - cbl_x_value;


		/* Graph line. From bottom of central area to top of central area. */
		if (row >= topmost_px && row <= bottommost_px) {
			/* Graph line. From left edge of central area to right edge of central area. */
			this->graph_2d->central_draw_line(this->graph_2d->grid_pen,
							  leftmost_px,                     row,
							  leftmost_px + central_n_columns, row);

			/* Text label in left margin. */
			//qDebug() << SG_PREFIX_D << "      value (inside) =" << axis_mark_uu << ", left_row =" << left_row;
			const QRectF bounding_rect = QRectF(2, row, left_width - 4, left_height);
			const QString label = this->get_y_grid_label(axis_mark_uu);
			this->graph_2d->draw_text(this->graph_2d->labels_font, this->graph_2d->labels_pen, bounding_rect, Qt::AlignRight | Qt::AlignTop, label, TextOffset::Up);
		} else {
			//qDebug() << SG_PREFIX_D << "      value (outside) =" << axis_mark_uu << ", row =" << row;
		}
	}
}




void ProfileView::draw_x_grid_d_domain(void)
{
	const int central_n_columns  = this->get_central_n_columns();
	const int central_n_rows = this->get_central_n_rows();
	const int bottom_width   = this->graph_2d->bottom_get_width();
	const int bottom_height  = this->graph_2d->bottom_get_height();
	const int leftmost_px    = this->graph_2d->central_get_leftmost_pixel();
	const int rightmost_px   = this->graph_2d->central_get_rightmost_pixel();
	const int topmost_px     = this->graph_2d->central_get_topmost_pixel();
	const int bottommost_px  = this->graph_2d->central_get_bottommost_pixel();

	const Distance visible_range = this->x_max_visible_d - this->x_min_visible_d;
	if (visible_range.is_zero()) {
		qDebug() << SG_PREFIX_E << "Zero visible range:" << this->x_min_visible_d << this->x_max_visible_d;
		return;
	}

	int first_mark = 0;
	int last_mark = 0;
	find_grid_line_indices(this->x_min_visible_d, this->x_max_visible_d, this->x_interval_d, first_mark, last_mark);

#if 1   /* Debug. */
	qDebug() << "===== drawing x grid for graph" << this->get_title() << ", central n cols =" << central_n_columns;
	qDebug() << "===== drawing x grid for graph" << this->get_title() << ", bottom width =" << bottom_width;
	qDebug() << "      min/max d on x axis visible:" << this->x_min_visible_d << this->x_max_visible_d;
	qDebug() << "      interval =" << this->x_interval_d << ", first_mark/last_mark =" << first_mark << last_mark;
#endif

	for (int i = first_mark; i <= last_mark; i++) {
		const Distance axis_mark_uu = this->x_interval_d * i;


		/* Value in 'coordinate system with beginning in
		   bottom-left corner'. */
		/* Purposefully use "1.0 *" to enforce conversion to
		   float, to avoid losing data during integer division. */
		const int cbl_x_value = (axis_mark_uu - this->x_min_visible_d) * central_n_columns / (visible_range * 1.0);
		/* Conversion to viewport pixmap's 'coordinate system
		   with beginning in top-left corner' */
		const int col = leftmost_px + cbl_x_value;

		if (col >= leftmost_px && col <= rightmost_px) {
			//qDebug() << SG_PREFIX_D << "      value (inside) =" << axis_mark_uu << ", col =" << col;

			/* Graph line. From bottom of central area to top of central area. */
			this->graph_2d->central_draw_line(this->graph_2d->grid_pen,
							  col, topmost_px,
							  col, topmost_px + central_n_rows);

			/* Text label in bottom margin. */
			const QRectF bounding_rect = QRectF(col, bottommost_px + 1, bottom_width - 3, bottom_height - 3);
			const QString label = axis_mark_uu.to_nice_string();
			this->graph_2d->draw_text(this->graph_2d->labels_font, this->graph_2d->labels_pen, bounding_rect, Qt::AlignLeft | Qt::AlignTop, label, TextOffset::Left);

		} else {
			//qDebug() << SG_PREFIX_D << "      value (outside) =" << axis_mark_uu << ", col =" << col;
		}
	}
}




void ProfileView::draw_x_grid_t_domain(void)
{
	const int central_n_columns  = this->get_central_n_columns();
	const int central_n_rows = this->get_central_n_rows();
	const int bottom_width   = this->graph_2d->bottom_get_width();
	const int bottom_height  = this->graph_2d->bottom_get_height();
	const int leftmost_px    = this->graph_2d->central_get_leftmost_pixel();
	const int rightmost_px   = this->graph_2d->central_get_rightmost_pixel();
	const int topmost_px     = this->graph_2d->central_get_topmost_pixel();
	const int bottommost_px  = this->graph_2d->central_get_bottommost_pixel();

	const Time visible_range = this->x_max_visible_t - this->x_min_visible_t;
	if (visible_range.is_zero()) {
		qDebug() << SG_PREFIX_E << "Zero visible range:" << this->x_min_visible_t << this->x_max_visible_t;
		return;
	}

	int first_mark = 0;
	int last_mark = 0;
	find_grid_line_indices(this->x_min_visible_t, this->x_max_visible_t, this->x_interval_t, first_mark, last_mark);

#if 1
	qDebug() << "===== drawing x grid for graph" << this->get_title() << ", central width =" << central_n_columns;
	qDebug() << "===== drawing x grid for graph" << this->get_title() << ", bottom width =" << bottom_width;
	qDebug() << "      min/max t on x axis visible:" << this->x_min_visible_t << this->x_max_visible_t;
	qDebug() << "      interval =" << this->x_interval_t.get_value() << ", first_mark/last_mark =" << first_mark << last_mark;
#endif

	for (int i = first_mark; i <= last_mark; i++) {
		const Time axis_mark_uu = this->x_interval_t * i;

		/* 'col' is in "beginning in bottom-left corner" coordinate system. */
		/* Purposefully use "1.0 *" to enforce conversion to
		   float, to avoid losing data during integer division. */
		const int col = leftmost_px
			+ (axis_mark_uu - this->x_min_visible_t) * 1.0 * central_n_columns / (visible_range * 1.0);

		if (col >= leftmost_px && col <= rightmost_px) {
			//qDebug() << SG_PREFIX_D << "      value (inside) =" << axis_mark_uu << ", col =" << col;

			/* Graph line. From bottom of central area to top of central area. */
			this->graph_2d->central_draw_line(this->graph_2d->grid_pen,
							  col, topmost_px,
							  col, topmost_px + central_n_rows);


			/* Text label in bottom margin. */
			const QRectF bounding_rect = QRectF(col, bottommost_px + 1, bottom_width - 3, bottom_height - 3);
			const QString label = get_time_grid_label(this->x_interval_t, axis_mark_uu);
			this->graph_2d->draw_text(this->graph_2d->labels_font, this->graph_2d->labels_pen, bounding_rect, Qt::AlignLeft | Qt::AlignTop, label, TextOffset::Left);

		} else {
			//qDebug() << SG_PREFIX_D << "      value (outside) =" << axis_mark_uu << ", col =" << col;
		}
	}
}




QString ProfileView::get_y_grid_label(float value_uu)
{
	switch (this->graph_2d->y_domain) {
	case GisViewportDomain::Elevation:
		return Altitude(value_uu, this->graph_2d->height_unit).to_string();

	case GisViewportDomain::Distance:
		return Distance(value_uu, this->graph_2d->distance_unit).to_string();

	case GisViewportDomain::Speed:
		return Speed(value_uu, this->graph_2d->speed_unit).to_string();

	case GisViewportDomain::Gradient:
		return Gradient(value_uu).to_string();

	default:
		qDebug() << SG_PREFIX_E << "Unhandled y domain" << (int) this->graph_2d->y_domain;
		return "";
	}
}




void ProfileView::draw_x_grid(const Track * trk)
{
	switch (this->graph_2d->x_domain) {
	case GisViewportDomain::Time:
		this->draw_x_grid_t_domain();
		break;

	case GisViewportDomain::Distance:
		this->draw_x_grid_d_domain();
		break;

	default:
		qDebug() << SG_PREFIX_E << "Unhandled x domain" << (int) this->graph_2d->x_domain;
		break;
	}
}




bool ProfileView::supported_domains(GisViewportDomain x_domain, GisViewportDomain y_domain)
{
	switch (x_domain) {
	case GisViewportDomain::Distance:
		switch (y_domain) {
		case GisViewportDomain::Elevation:
		case GisViewportDomain::Gradient:
		case GisViewportDomain::Speed:
			return true;
		default:
			return false;
		}
	case GisViewportDomain::Time:
		switch (y_domain) {
		case GisViewportDomain::Speed:
		case GisViewportDomain::Distance:
		case GisViewportDomain::Elevation:
			return true;

		default:
			return false;
		}
	default:
		return false;
	}
}




int ProfileView::get_central_n_columns(void) const
{
	return this->graph_2d->cached_central_n_columns;
}




int ProfileView::get_central_n_rows(void) const
{
	return this->graph_2d->cached_central_n_rows;
}




Graph2D::Graph2D(int left, int right, int top, int bottom, QWidget * parent) : ViewportPixmap(left, right, top, bottom, parent)
{
	this->height_unit = Preferences::get_unit_height();
	this->distance_unit = Preferences::get_unit_distance();
	this->speed_unit = Preferences::get_unit_speed();
}




/**
   @reviewed-on tbd
*/
sg_ret Graph2D::cbl_get_cursor_pos(QMouseEvent * ev, ScreenPos & screen_pos) const
{
	const int leftmost   = this->central_get_leftmost_pixel();
	const int rightmost  = this->central_get_rightmost_pixel();
	const int topmost    = this->central_get_topmost_pixel();
	const int bottommost = this->central_get_bottommost_pixel();

	const QPoint position = this->mapFromGlobal(QCursor::pos());

#if 0   /* Verbose debug. */
	qDebug() << SG_PREFIX_I << "Difference in cursor position: dx = " << position.x() - ev->x() << ", dy = " << position.y() - ev->y();
#endif

#if 0
	const int x = position.x();
	const int y = position.y();
#else
	const int x = ev->x();
	const int y = ev->y();
#endif

	/* Cursor outside of chart area. */
	if (x > rightmost) {
		return sg_ret::err;
	}
	if (y > bottommost) {
		return sg_ret::err;
	}
	if (x < leftmost) {
		return sg_ret::err;
	}
	if (y < topmost) {
		return sg_ret::err;
	}

	/* Converting from Qt's "beginning is in upper-left" into "beginning is in bottom-left" coordinate system. */
	screen_pos.rx() = x;
	screen_pos.ry() = bottommost - y;

	return sg_ret::ok;
}





/**
   @reviewed-on tbd
*/
void Graph2D::mousePressEvent(QMouseEvent * ev)
{
	qDebug() << SG_PREFIX_I << "Mouse CLICK event, button" << (int) ev->button();
	ev->accept();
}



/**
   @reviewed-on tbd
*/
void Graph2D::mouseMoveEvent(QMouseEvent * ev)
{
	//this->draw_mouse_motion_cb(ev);
	emit this->cursor_moved(this, ev);
	ev->accept();
}




/**
   @reviewed-on tbd
*/
void Graph2D::mouseReleaseEvent(QMouseEvent * ev)
{
	qDebug() << SG_PREFIX_I << "called with button" << (int) ev->button();
	emit this->button_released(this, ev);
	ev->accept();
}




int Graph2D::central_get_n_columns(void) const
{
	return this->central_get_rightmost_pixel() - this->central_get_leftmost_pixel() + 1;
}




int Graph2D::central_get_n_rows(void) const
{
	return this->central_get_bottommost_pixel() - this->central_get_topmost_pixel() + 1;
}




void Graph2D::central_draw_simple_crosshair(const Crosshair2D & crosshair)
{
	const int leftmost_pixel = this->central_get_leftmost_pixel();
	const int rigthmost_pixel = this->central_get_rightmost_pixel();
	const int topmost_pixel = this->central_get_topmost_pixel();
	const int bottommost_pixel = this->central_get_bottommost_pixel();

	if (!crosshair.valid) {
		qDebug() << SG_PREFIX_E << "Crosshair" << crosshair.debug << "is invalid";
		/* Position outside of graph area. */
		return;
	}

	qDebug() << SG_PREFIX_I << "Crosshair" << crosshair.debug << "at coord" << crosshair.x << crosshair.y << "(central cbl =" << crosshair.central_cbl_x << crosshair.central_cbl_y << ")";

	if (crosshair.x > rigthmost_pixel || crosshair.x < leftmost_pixel) {
		qDebug() << SG_PREFIX_E << "Crosshair has bad x";
		/* Position outside of graph area. */
		return;
	}
	if (crosshair.y > bottommost_pixel || crosshair.y < topmost_pixel) {
		qDebug() << SG_PREFIX_E << "Crosshair has bad y";
		/* Position outside of graph area. */
		return;
	}


	this->painter.setPen(this->marker_pen);

	/* Small optimization: use QT's drawing primitives directly.
	   Remember that (0,0) screen position is in upper-left corner of viewport. */

	this->painter.drawLine(leftmost_pixel, crosshair.y, rigthmost_pixel, crosshair.y); /* Horizontal line. */
	this->painter.drawLine(crosshair.x, topmost_pixel, crosshair.x, bottommost_pixel); /* Vertical line. */
}
