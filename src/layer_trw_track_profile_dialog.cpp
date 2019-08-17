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

#define MY_WIDGET_PROPERTY "ProfileViewBase"


#define VIK_SETTINGS_TRACK_PROFILE_WIDTH  "track_profile_display_width"
#define VIK_SETTINGS_TRACK_PROFILE_HEIGHT "track_profile_display_height"

#define VIK_SETTINGS_TRACK_PROFILE_ET_SHOW_DEM_ELEVATION    "track_profile_et_show_dem_elevation"
#define VIK_SETTINGS_TRACK_PROFILE_ET_SHOW_GPS_SPEED        "track_profile_et_show_gps_speed"
#define VIK_SETTINGS_TRACK_PROFILE_SD_SHOW_GPS_SPEED        "track_profile_sd_show_gps_speed"
#define VIK_SETTINGS_TRACK_PROFILE_ED_SHOW_DEM_ELEVATION    "track_profile_ed_show_dem_elevation"
#define VIK_SETTINGS_TRACK_PROFILE_ED_SHOW_GPS_SPEED        "track_profile_ed_show_gps_speed"
#define VIK_SETTINGS_TRACK_PROFILE_GD_SHOW_GPS_SPEED        "track_profile_gd_show_gps_speed"
#define VIK_SETTINGS_TRACK_PROFILE_ST_SHOW_GPS_SPEED        "track_profile_st_show_gps_speed"
#define VIK_SETTINGS_TRACK_PROFILE_DT_SHOW_GPS_SPEED        "track_profile_dt_show_gps_speed"




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




static QString get_time_grid_label(const Time & interval_value, const Time & value);




TrackProfileDialog::~TrackProfileDialog()
{
	for (auto iter = this->views.begin(); iter != this->views.end(); iter++) {
		delete *iter;
	}
}




template <typename Tx, typename Tx_ll>
sg_ret ProfileView<Tx, Tx_ll>::regenerate_track_data_to_draw(Track * trk)
{
	this->track_data_to_draw.invalidate();

	/*
	  ::initial_track_data has been generated once, when the
	  dialog has been opened. Now compress it to limit number of
	  drawn points.
	*/
	const int compressed_n_points = this->get_central_n_columns();

	const bool precise_drawing = true;


	if (precise_drawing) {
		this->track_data_to_draw = this->initial_track_data;
	} else {
		this->initial_track_data.compress_into(track_data_to_draw, compressed_n_points);
	}
	if (!this->track_data_to_draw.valid) {
		qDebug() << SG_PREFIX_E << "Failed to regenerate valid compressed track data for" << this->get_title();
		return sg_ret::err;
	}
	qDebug() << SG_PREFIX_I << "Regenerated valid compressed track data for" << this->get_title();
	qDebug() << SG_PREFIX_I << "Initial track data" << this->get_title() << this->initial_track_data;
	qDebug() << SG_PREFIX_I << "Track data to draw" << this->get_title() << this->track_data_to_draw;


	/* Do necessary adjustments to x values. */
	sg_ret result = sg_ret::err;
	result = this->set_initial_visible_range_x();
	if (sg_ret::ok != result) {
		qDebug() << SG_PREFIX_E << "Failed to set initial visible x range";
		return result;
	}
	result = this->set_initial_visible_range_y(this->track_data_to_draw);
	if (sg_ret::ok != result) {
		qDebug() << SG_PREFIX_E << "Failed to set initial visible y range";
		return result;
	}
	if (sg_ret::ok != result) {
		qDebug() << SG_PREFIX_E << "Failed to set initial visible x range";
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


namespace SlavGPS {


template <>
sg_ret ProfileView<Distance, Distance_ll>::set_initial_visible_range_x(void)
{
	/* We won't display any x values outside of
	   track_data.x_min/max. We will never be able to zoom out to
	   show e.g. negative distances. */
	this->x_visible_min = this->track_data_to_draw.x_min;
	this->x_visible_max = this->track_data_to_draw.x_max;


	this->x_visible_range_uu = this->x_visible_max - this->x_visible_min;
	if (this->x_visible_range_uu.is_zero()) {
		qDebug() << SG_PREFIX_E << "Zero distance span: min/max = " << this->x_visible_min << this->x_visible_max;
		return sg_ret::err;
	}

	/* Now, given the n_intervals value, find a suitable interval
	   index and value that will nicely cover visible range of
	   data. */

	const int n_intervals = GRAPH_X_INTERVALS;

	int interval_index = distance_intervals.intervals.get_interval_index(this->x_visible_min, this->x_visible_max, n_intervals);
	this->x_interval = distance_intervals.intervals.values[interval_index];

	return sg_ret::ok;
}




template <>
sg_ret ProfileView<Time, Time_ll>::set_initial_visible_range_x(void)
{
	/* We won't display any x values outside of
	   track_data.x_min/max. We will never be able to zoom out to
	   show e.g. negative times. */
	this->x_visible_min = this->track_data_to_draw.x_min;
	this->x_visible_max = this->track_data_to_draw.x_max;


	this->x_visible_range_uu = this->x_visible_max - this->x_visible_min;
	if (this->x_visible_range_uu.is_zero()) {
		qDebug() << SG_PREFIX_E << "Zero time span: min/max x = " << this->x_visible_min << this->x_visible_max << this->get_title();
		return sg_ret::err;
	}

	/* Now, given the n_intervals value, find a suitable interval
	   index and value that will nicely cover visible range of
	   data. */

	const int n_intervals = GRAPH_X_INTERVALS;

	int interval_index = time_intervals.intervals.get_interval_index(this->x_visible_min, this->x_visible_max, n_intervals);
	this->x_interval = time_intervals.intervals.values[interval_index];

	return sg_ret::ok;
}




}




template <typename Tx, typename Tx_ll>
sg_ret ProfileView<Tx, Tx_ll>::set_initial_visible_range_y(const TrackDataBase & track_data)
{
	/* When user will be zooming in and out, and (in particular)
	   moving graph up and down, the y_min/max_visible values will
	   be non-rounded (i.e. random).  Make them non-rounded from
	   the start, and be prepared to handle non-rounded
	   y_min/max_visible from the start. */
	const double y_margin = 0.05; /* There is no deep reasoning behind this particular value. */

	/* This is not exactly the same range as this->y_visible_range_uu
	   calculated below. */
	const double y_data_range = std::fabs(track_data.y_max - track_data.y_min);

	switch (this->graph_2d->y_domain) {
	case GisViewportDomain::Speed:
	case GisViewportDomain::Distance:
		/* Some graphs better start at zero, e.g. speed graph
		   or distance graph. Showing negative speed values on
		   a graph wouldn't make sense. */
		this->y_visible_min = track_data.y_min;
		break;
	case GisViewportDomain::Elevation:
	case GisViewportDomain::Gradient:
		this->y_visible_min = track_data.y_min - y_data_range * y_margin;
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled y domain" << (int) this->graph_2d->y_domain;
		return sg_ret::err;
	}
	this->y_visible_max = track_data.y_max + y_data_range * y_margin;
	this->y_visible_range_uu = std::fabs(this->y_visible_max - this->y_visible_min);





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
sg_ret TrackProfileDialog::set_center_at_selected_tp(const ProfileViewBase * view, QMouseEvent * ev)
{
	this->button_split_at_marker->setEnabled(false);

	const TPInfo tp_info = view->get_tp_info_under_cursor(ev);
	if (!tp_info.valid) {
		qDebug() << SG_PREFIX_E << "No valid tp info found for view" << view->get_title();
		return sg_ret::err;
	}

	if (NULL == tp_info.found_tp) {
		qDebug() << SG_PREFIX_E << "NULL 'found tp' for view" << view->get_title();
		return sg_ret::err;
	}

	if (sg_ret::ok != this->trk->select_tp(tp_info.found_tp)) {
		qDebug() << SG_PREFIX_E << "Failed to select tp for view" << view->get_title();
		return sg_ret::err;
	}

	this->main_gisview->set_center_coord(tp_info.found_tp->coord);
	this->trw->emit_tree_item_changed("Clicking on trackpoint in profile view has brought this trackpoint to center of main GIS viewport");

	/* There is a selected trackpoint, on which we can split the track. */
	this->button_split_at_marker->setEnabled(true);

	return sg_ret::ok;
}




/**
   Draw two pairs of horizontal and vertical lines intersecting at given position.

   One pair is for position of selected trackpoint.
   The other pair is for current position of cursor.

   Both "pos" arguments should indicate position in graph's coordinate system.
*/
sg_ret ProfileViewBase::draw_crosshairs(const Crosshair2D & selection_ch, const Crosshair2D & hover_ch)
{
	if (!selection_ch.valid && !hover_ch.valid) {
		/* Perhaps this should be an error, maybe we shouldn't
		   call the function when both positions are
		   invalid. */
		qDebug() << SG_PREFIX_N << "Not drawing crosshairs: both crosshairs are invalid";
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
	if (selection_ch.valid) {
		// qDebug() << SG_PREFIX_I << "Will now draw 'selection' crosshair in" << this->get_title();
		this->graph_2d->central_draw_simple_crosshair(selection_ch);
	}
	if (hover_ch.valid) {
		// qDebug() << SG_PREFIX_D << "Will now draw 'hover' crosshair in" << this->get_title();
		this->graph_2d->central_draw_simple_crosshair(hover_ch);
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




ProfileViewBase * TrackProfileDialog::get_current_view(void) const
{
	const int tab_idx = this->tabs->currentIndex();
	ProfileViewBase * profile_view = (ProfileViewBase *) this->tabs->widget(tab_idx)->property(MY_WIDGET_PROPERTY).toULongLong();
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
	ProfileViewBase * view = this->get_current_view();
	assert (view->graph_2d == graph_2d);

	qDebug() << SG_PREFIX_I << "Crosshair. Mouse event at" << ev->x() << ev->y() << "(cbl ="
		 << ev->x() - graph_2d->central_get_leftmost_pixel()
		 << graph_2d->central_get_bottommost_pixel() - ev->y() << ")";

	/* Noninitialized == invalid. Don't draw hover crosshair on
	   clicks, it's not necessary: the two crosshairs (hover and
	   selection crosshair) would be drawn in the same
	   position. */
	const Crosshair2D hover_ch;

	for (auto iter = this->views.begin(); iter != this->views.end(); iter++) {
		ProfileViewBase * view_iter = *iter;
		if (!view_iter->graph_2d) {
			qDebug() << SG_PREFIX_N << "Not drawing crosshairs in" << view_iter->get_title() << ": no graph";
			continue;
		}

		if (!view_iter->track_data_is_valid()) {
			qDebug() << SG_PREFIX_N << "Not drawing crosshairs in" << view_iter->get_title() << ": track data invalid";
			/* We didn't visit that tab yet, so track data
			   hasn't been generated for current graph
			   width. */
			/* FIXME: generate the track data so that we
			   can set a crosshair over there. */
			continue;
		}

		view_iter->m_selection_ch = view_iter->get_crosshair_under_cursor(ev);
		view_iter->m_selection_ch.debug = "selection crosshair in " + view_iter->get_title();
		if (!view_iter->m_selection_ch.valid) {
			qDebug() << SG_PREFIX_N << "Not drawing crosshairs in" << view_iter->get_title() << ": failed to get selection crosshair";
			continue;
		}

		/*
		  Positions passed to draw_crosshairs() are in 2D
		  graph's coordinate system (beginning in bottom left
		  corner), not Qt's coordinate system (beginning in
		  upper left corner).
		*/
		qDebug() << SG_PREFIX_I << "Will now draw crosshairs in" << view_iter->get_title();
		view_iter->draw_crosshairs(view_iter->m_selection_ch, hover_ch);
	}

	this->button_split_at_marker->setEnabled(false);

	const TPInfo tp_info = view->get_tp_info_under_cursor(ev);
	if (!tp_info.valid) {
		qDebug() << SG_PREFIX_W << "No valid tp info found for view" << view->get_title();
		return;
	}

	if (NULL == tp_info.found_tp) {
		qDebug() << SG_PREFIX_E << "NULL 'found tp' for view" << view->get_title();
		return;
	}

	if (sg_ret::ok != this->trk->select_tp(tp_info.found_tp)) {
		qDebug() << SG_PREFIX_E << "Failed to select tp for view" << view->get_title();
		return;
	}

	this->main_gisview->set_center_coord(tp_info.found_tp->coord);
	this->trw->emit_tree_item_changed("Clicking on trackpoint in profile view has brought this trackpoint to center of main GIS viewport");

	/* There is a selected trackpoint, on which we can split the track. */
	this->button_split_at_marker->setEnabled(true);

	return;
}




template <typename Tx, typename Tx_ll>
TPInfo ProfileView<Tx, Tx_ll>::get_tp_info_under_cursor(QMouseEvent * ev) const
{
	TPInfo result;

	const size_t n_values = this->track_data_to_draw.n_points;
	if (0 == n_values) {
		qDebug() << SG_PREFIX_N << "There were zero values in" << graph_2d->debug;
		return result;
	}

	const int n_columns = this->graph_2d->central_get_n_columns();
	const int n_rows = this->graph_2d->central_get_n_rows();
	const int leftmost_px = this->graph_2d->central_get_leftmost_pixel();
	const int bottommost_px = this->graph_2d->central_get_bottommost_pixel();

	const int event_x = ev->x();

	int x_px_diff = n_columns; /* We will be minimizing this value and stop when x_px_diff is the smallest. TODO: type of the variable: int or double? */

	const double x_pixels_per_unit = n_columns / this->x_visible_range_uu;
	const double y_pixels_per_unit = n_rows / this->y_visible_range_uu;

	for (size_t i = 0; i < n_values; i++) {

		const double y_current_value_uu = this->track_data_to_draw.y[i];
		const bool y_value_valid = !std::isnan(y_current_value_uu);
		if (!y_value_valid) {
			continue;
		}

		const Tx_ll x_current_value_uu = this->track_data_to_draw.x[i];
		const int x_px = leftmost_px + x_pixels_per_unit * (x_current_value_uu - this->track_data_to_draw.x_min.value);

		/* See if x coordinate of this trackpoint on a pixmap
		   is closer to cursor than the previous x
		   coordinate. */
		int x_px_diff_current = std::abs(x_px - event_x);
		if (x_px_diff_current < x_px_diff) {
			/* Found a trackpoint painted at position 'x' that is closer to cursor event position on x axis. */
			x_px_diff = x_px_diff_current;

			const int y_px = bottommost_px - y_pixels_per_unit * (y_current_value_uu - this->track_data_to_draw.y_min);

			result.found_x_px = x_px;
			result.found_y_px = y_px;
			result.found_tp_idx = i;
			result.found_tp = this->track_data_to_draw.tps[i];
			result.valid = true;

			// qDebug() << SG_PREFIX_I << "Found new position closer to cursor event at index" << found_tp_idx << ":" << found_x_px << found_y_px;
		}
	}

	return result;
}




/*
  If x values in track data were always separated by the same amount,
  this function could have been a simple array indexing code based on
  'x' coordinate of mouse event.

  But since sometimes there can be gaps in graphs where x-domain is
  Time, and especially because distance values (in graphs where
  x-domain is Distance) can greatly vary, things are more
  complicated.

  The fact that there can be fewer values in track data than there are
  columns in pixmap (i.e. distance between each drawn trackpoints can
  be two or more pixels) also doesn't help.

  We have to use more sophisticated algorithm for
  conversion of 'x' coordinate of mouse event to position of crosshair.

  We have to try to find a trackpoint in track data arrays that is
  drawn the closest to 'x' coordinate of mouse event.
*/
template <typename Tx, typename Tx_ll>
Crosshair2D ProfileView<Tx, Tx_ll>::get_crosshair_under_cursor(QMouseEvent * ev) const
{
	Crosshair2D crosshair;

	const TPInfo tp_info = this->get_tp_info_under_cursor(ev);
	if (!tp_info.valid) {
		qDebug() << SG_PREFIX_N << "Could not find valid tp info in" << this->get_title();
		return crosshair;
	}

	crosshair = this->tpinfo_to_crosshair(tp_info);

	return crosshair;
}




Crosshair2D ProfileViewBase::tpinfo_to_crosshair(const TPInfo & tp_info) const
{
	Crosshair2D crosshair;

	const int leftmost_px = this->graph_2d->central_get_leftmost_pixel();
	const int bottommost_px = this->graph_2d->central_get_bottommost_pixel();

	crosshair.central_cbl_x_px = tp_info.found_x_px - leftmost_px;
	crosshair.central_cbl_y_px = bottommost_px - tp_info.found_y_px;

	/*
	  Use coordinates of point that is
	  a) limited to central area of 2d graph (so as if margins outside of the central area didn't exist),
	  b) is in 'beginning in bottom-left' coordinate system (cbl)
	  to calculate global, 'beginning in top-left' coordinates.
	*/
	crosshair.x_px = crosshair.central_cbl_x_px + leftmost_px;
	crosshair.y_px = bottommost_px - crosshair.central_cbl_y_px;

	crosshair.valid = true;

	return crosshair;
}





void TrackProfileDialog::handle_cursor_move_cb(ViewportPixmap * vpixmap, QMouseEvent * ev)
{
	Graph2D * graph_2d = (Graph2D *) vpixmap;
	ProfileViewBase * view = this->get_current_view();
	assert (view->graph_2d == graph_2d);

	if (!view->track_data_is_valid()) {
		qDebug() << SG_PREFIX_E << "Not handling cursor move, track data invalid";
		return;
	}

	view->on_cursor_move(trk, ev);

	return;
}




template <typename Tx, typename Tx_ll>
sg_ret ProfileView<Tx, Tx_ll>::on_cursor_move(Track * trk, QMouseEvent * ev)
{
	const TPInfo tp_info = this->get_tp_info_under_cursor(ev);
	if (!tp_info.valid || NULL == tp_info.found_tp) {
		qDebug() << SG_PREFIX_N << "Could not find valid tp info for" << this->get_title();
		return sg_ret::err;
	}

	Crosshair2D hover_ch = this->tpinfo_to_crosshair(tp_info);
	hover_ch.debug = "hover crosshair for " + this->get_title();
	if (!hover_ch.valid) {
		qDebug() << SG_PREFIX_E << "Failed to get hover crosshair for" << this->get_title();
		return sg_ret::err;
	}

	this->draw_crosshairs(this->m_selection_ch, hover_ch);

	if (sg_ret::ok != this->update_x_labels(tp_info)) {
		qDebug() << SG_PREFIX_E << "Failed to update x labels in view" << this->get_title();
		return sg_ret::err;
	}

	if (sg_ret::ok != this->update_y_labels(tp_info)) {
		qDebug() << SG_PREFIX_E << "Failed to update y labels in view" << this->get_title();
		return sg_ret::err;
	}

	return sg_ret::ok;
}




namespace SlavGPS {




template <>
sg_ret ProfileView<Distance, Distance_ll>::update_x_labels(const TPInfo & tp_info)
{
	/* This is a private method, so we assume that tp_info, and in
	   particular tp_info.found_tp are valid. */
	assert(tp_info.valid);
	assert(NULL != tp_info.found_tp);


	if (this->labels.x_value) {
		/* Values in x[] are already re-calculated to user units. */
		const Distance_ll x_ll_uu = this->track_data_to_draw.x[tp_info.found_tp_idx];
		const Distance x_uu = Distance(x_ll_uu, Preferences::get_unit_distance()); /* TODO: distance unit or supplementary distance unit? */
		this->labels.x_value->setText(x_uu.to_string());
	}

	/*
	  Absolute timestamp.

	  Thanks to TrackData::tps[] we now have access to trackpoints
	  and their timestamps. Having the timestamp even in
	  Distance-based graphs may be useful.
	*/
	this->labels.tp_timestamp_value->setText(tp_info.found_tp->timestamp.to_timestamp_string(Qt::LocalTime));

	return sg_ret::ok;
}




template <>
sg_ret ProfileView<Time, Time_ll>::update_x_labels(const TPInfo & tp_info)
{
	/* This is a private method, so we assume that tp_info, and in
	   particular tp_info.found_tp are valid. */
	assert(tp_info.valid);
	assert(NULL != tp_info.found_tp);

	/* Relative time from start of track. */
	if (this->labels.x_value) {
		/* Values in x[] are already re-calculated to user units. */
		const Time_ll x_ll_uu = this->track_data_to_draw.x[tp_info.found_tp_idx];
		const Time x_uu = Time(x_ll_uu);

		/* TODO: we should use timestamp of first valid
		   tp. Make sure that x_min is a timestamp of first
		   valid tp. */
		const Time x_uu_from_start = x_uu - this->track_data_to_draw.x_min;

		this->labels.x_value->setText(x_uu_from_start.to_duration_string());
	}

	/* Absolute time stamp. */
	this->labels.tp_timestamp_value->setText(tp_info.found_tp->timestamp.to_timestamp_string(Qt::LocalTime));

	return sg_ret::ok;
}




} /* namespace SlavGPS */




template <typename Tx, typename Tx_ll>
sg_ret ProfileView<Tx, Tx_ll>::update_y_labels(const TPInfo & tp_info)
{
	/* This is a private method, so we assume that tp_info, and in
	   particular tp_info.found_tp are valid. */
	assert(tp_info.valid);
	assert(NULL != tp_info.found_tp);

	/* Values in y[] are already re-calculated to user units. */
	const double y_uu = this->track_data_to_draw.y[tp_info.found_tp_idx];

	switch (this->graph_2d->y_domain) {
	case GisViewportDomain::Speed:
		if (this->labels.y_value) {
			this->labels.y_value->setText(Speed::to_string(y_uu));
		}
		break;
	case GisViewportDomain::Elevation:
		if (this->labels.y_value) {
			const Altitude alti = Altitude(y_uu, Preferences::get_unit_height());
			this->labels.y_value->setText(alti.to_string());
		}
		break;
	case GisViewportDomain::Distance:
		if (this->labels.y_value) {
			const Distance distance_uu(y_uu, Preferences::get_unit_distance());
			this->labels.y_value->setText(distance_uu.to_string());
		}
		break;
	case GisViewportDomain::Gradient:
		if (this->labels.y_value) {
			const Gradient gradient_uu(y_uu);
			this->labels.y_value->setText(gradient_uu.to_string());
		}
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled y domain" << (int) this->graph_2d->y_domain;
		return sg_ret::err;
	}

	return sg_ret::ok;
}




/**
 * Draws DEM points and a respresentative speed on the supplied pixmap
 * (which is the elevations graph).
 */
template <typename Tx, typename Tx_ll>
sg_ret ProfileView<Tx, Tx_ll>::draw_dem_elevation(Track * trk)
{
	double max_function_arg = trk->get_length_value_including_gaps();
	double max_function_value_speed = 0;

	const int leftmost_px = this->graph_2d->central_get_leftmost_pixel();
	const int bottommost_px = this->graph_2d->central_get_bottommost_pixel();


	double current_function_arg = 0.0;
	const double max_function_value_dem = this->y_visible_max;

	const QColor dem_color = this->dem_alt_pen.color();



	const int n_columns = this->graph_2d->central_get_n_columns();
	const int n_rows = this->graph_2d->central_get_n_rows();
	const size_t n_values = this->track_data_to_draw.n_points;

	for (auto iter = std::next(trk->trackpoints.begin()); iter != trk->trackpoints.end(); iter++) {

		current_function_arg += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);
		const Altitude elev = DEMCache::get_elev_by_coord((*iter)->coord, DemInterpolation::Best);
		if (!elev.is_valid()) {
			continue;
		}

		const double elev_value_uu = elev.convert_to_unit(this->graph_2d->height_unit).get_value();

		/* offset is in current height units. */
		const double current_function_value_uu = elev_value_uu - this->y_visible_min;

		const int x_px = n_columns * current_function_arg / max_function_arg;
		const int y_px = 0 - n_rows * current_function_value_uu / max_function_value_dem;
		this->graph_2d->fill_rectangle(dem_color, x_px - 2, y_px - 2, 4, 4);
	}

	return sg_ret::ok;
}




/**
   @reviewed-on tbd
*/
template <typename Tx, typename Tx_ll>
sg_ret ProfileView<Tx, Tx_ll>::draw_function_values(Track * trk)
{
	const size_t n_values = this->track_data_to_draw.n_points;
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

	const double x_pixels_per_unit = n_columns / this->x_visible_range_uu;
	const double y_pixels_per_unit = n_rows / this->y_visible_range_uu;


	ScreenPos cur_valid_pos;
	ScreenPos last_valid_pos(leftmost_px, bottommost_px);

	for (size_t i = 0; i < n_values; i++) {

		const Tx_ll x_current_value_uu = this->track_data_to_draw.x[i];
		/*
		  This line creates x coordinate that is
		  proportionally as far from left border, as
		  track_data_to_draw.x[i] is from track_data_to_draw.x_min.

		  It works equally well for x vectors with constant
		  intervals (e.g. the same time interval == 1s between
		  consecutive measurements of y), as for x vector with
		  varying intervals (e.g. different values of
		  distances between consecutive measurements of y).
		*/
		const int x_px = leftmost_px + x_pixels_per_unit * (x_current_value_uu - this->track_data_to_draw.x_min.value);

		const bool y_value_valid = !std::isnan(this->track_data_to_draw.y[i]);

		if (y_value_valid) {
			const double y_current_value_uu = this->track_data_to_draw.y[i];

			cur_valid_pos.rx() = x_px;
			cur_valid_pos.ry() = bottommost_px - y_pixels_per_unit * (y_current_value_uu - this->track_data_to_draw.y_min);

			graph_2d->draw_line(valid_pen, last_valid_pos, cur_valid_pos);

			last_valid_pos = cur_valid_pos;
		} else {
			/*
			  Draw vertical line from bottom of graph to
			  top to indicate invalid y value.
			*/

			const int begin_y_px = bottommost_px;
			const int end_y_px = bottommost_px - n_rows;

			graph_2d->draw_line(invalid_pen, x_px, begin_y_px, x_px, end_y_px);
		}
	}

	return sg_ret::ok;
}




template <typename Tx, typename Tx_ll>
bool ProfileView<Tx, Tx_ll>::track_data_is_valid(void) const
{
	return this->track_data_to_draw.valid;
}




template <typename Tx, typename Tx_ll>
sg_ret ProfileView<Tx, Tx_ll>::draw_additional_indicators(Track * trk)
{
	const int n_rows = this->graph_2d->central_get_n_rows();
	const size_t n_values = this->track_data_to_draw.n_points;

	if (this->show_dem_cb && this->show_dem_cb->checkState())  {
		const double max_function_value = this->y_visible_max;

		const QColor color = this->dem_alt_pen.color();

		for (size_t i = 0; i < n_values; i++) {
			/* TODO: This could be slow doing this each time... */
			const Trackpoint * tp = this->track_data_to_draw.tps[i];
			if (NULL == tp) {
				continue;
			}

			const Altitude elev = DEMCache::get_elev_by_coord(tp->coord, DemInterpolation::Simple);
			if (!elev.is_valid()) {
				continue;
			}

			const double elev_value_uu = elev.convert_to_unit(Preferences::get_unit_height()).get_value();

			/* offset is in current height units. */
			const double current_function_value_uu = elev_value_uu - this->y_visible_min;

			const int x_px = i;
			const int y_px = 0 - n_rows * current_function_value_uu / max_function_value;
			this->graph_2d->fill_rectangle(color, x_px - 2, y_px - 2, 4, 4);
		}
	}


	if (this->show_dem_cb && this->show_dem_cb->checkState()) {
		this->draw_dem_elevation(trk);
	}


	/* Show GPS speed indicators. */
	if (this->show_gps_speed_cb && this->show_gps_speed_cb->checkState()) {
		/* Ensure some kind of max speed when not set. */
		if (!trk->get_max_speed().is_valid() || trk->get_max_speed().get_value() < 0.01) {
			trk->calculate_max_speed();
		}

		this->draw_gps_speeds(trk);
	}

	return sg_ret::ok;
}




template <typename Tx, typename Tx_ll>
sg_ret ProfileView<Tx, Tx_ll>::draw_gps_speeds(Track * trk)
{
	const int leftmost_px = this->graph_2d->central_get_leftmost_pixel();
	const int bottommost_px = this->graph_2d->central_get_bottommost_pixel();
	const int n_columns = this->graph_2d->central_get_n_columns();
	const int n_rows = this->graph_2d->central_get_n_rows();

	const size_t n_values = this->track_data_to_draw.n_points;

	const QColor & speed_color = this->gps_speed_pen.color();

	const Speed max_speed = trk->get_max_speed();
	qDebug() << SG_PREFIX_I << "Max speed is" << max_speed.get_value();

	const double speed_max = 1.1 * max_speed.get_value();

	const double x_pixels_per_unit = n_columns / this->x_visible_range_uu;
	const double y_pixels_per_unit = n_rows / 110; /* "110" means 110%. Zero percent at the bottom of graph, 110% (since we used 1.1 above) on top of graph. */

	for (size_t i = 0; i < n_values; i++) {

		const Trackpoint * tp = this->track_data_to_draw.tps[i];
		if (NULL == tp) {
			qDebug() << SG_PREFIX_N << "NULL trackpoint when drawing GPS speed for" << this->get_title();
			continue;
		}

		const double gps_speed = tp->gps_speed;
		if (std::isnan(gps_speed)) {
			qDebug() << SG_PREFIX_N << "NAN GPS speed for trackpoint when drawing GPS speed for" << this->get_title();
			continue;
		}

		const double x_value = this->track_data_to_draw.x[i];
		const double y_value = 100 * gps_speed / speed_max; /* Percentage of maximum speed. */

		const int x_px = leftmost_px + x_pixels_per_unit * (x_value - this->track_data_to_draw.x_min.value);
		const int y_px = bottommost_px - y_pixels_per_unit * y_value;

		/* This is just a speed indicator - no actual values can be inferred by user. */
		this->graph_2d->fill_rectangle(speed_color, x_px - 1, y_px - 1, 2, 2);
	}

	return sg_ret::ok;
}




void ProfileViewET::save_settings(void)
{
	ApplicationState::set_boolean(VIK_SETTINGS_TRACK_PROFILE_ET_SHOW_DEM_ELEVATION, this->show_dem_cb->checkState());
	ApplicationState::set_boolean(VIK_SETTINGS_TRACK_PROFILE_ET_SHOW_GPS_SPEED, this->show_gps_speed_cb->checkState());
}




void ProfileViewSD::save_settings(void)
{
	ApplicationState::set_boolean(VIK_SETTINGS_TRACK_PROFILE_SD_SHOW_GPS_SPEED, this->show_gps_speed_cb->checkState());
}




void ProfileViewED::save_settings(void)
{
	ApplicationState::set_boolean(VIK_SETTINGS_TRACK_PROFILE_ED_SHOW_DEM_ELEVATION, this->show_dem_cb->checkState());
	ApplicationState::set_boolean(VIK_SETTINGS_TRACK_PROFILE_ED_SHOW_GPS_SPEED, this->show_gps_speed_cb->checkState());
}




void ProfileViewGD::save_settings(void)
{
	ApplicationState::set_boolean(VIK_SETTINGS_TRACK_PROFILE_GD_SHOW_GPS_SPEED, this->show_gps_speed_cb->checkState());
}




void ProfileViewST::save_settings(void)
{
	ApplicationState::set_boolean(VIK_SETTINGS_TRACK_PROFILE_ST_SHOW_GPS_SPEED, this->show_gps_speed_cb->checkState());
}




void ProfileViewDT::save_settings(void)
{
	ApplicationState::set_boolean(VIK_SETTINGS_TRACK_PROFILE_DT_SHOW_GPS_SPEED, this->show_gps_speed_cb->checkState());
}




/**
   \brief Draw the y = f(x) graph
*/
template <typename Tx, typename Tx_ll>
sg_ret ProfileView<Tx, Tx_ll>::draw_graph_without_crosshairs(Track * trk)
{
	qDebug() << SG_PREFIX_I;
	QTime draw_time;
	draw_time.start();

	if (this->graph_2d->x_domain == GisViewportDomain::Time) {
		const Time duration = trk->get_duration(true);
		if (!duration.is_valid() || duration.get_value() <= 0) {
			qDebug() << SG_PREFIX_E << "return 1";
			return sg_ret::err;
		}
	}

	/* Clear before redrawing. */
	this->graph_2d->clear();





	if (sg_ret::ok != this->regenerate_track_data_to_draw(trk)) {
		qDebug() << SG_PREFIX_E << "return 2";
		return sg_ret::err;
	}
	this->draw_function_values(trk);


	/* Draw grid on top of graph of values. */
	this->draw_x_grid();
	this->draw_y_grid();

	this->graph_2d->central_draw_outside_boundary_rect();

	this->draw_additional_indicators(trk);

	/* This will call ::paintEvent(), triggering final render to screen. */
	this->graph_2d->update();

	/* The pixmap = margin + graph area. */
	qDebug() << SG_PREFIX_I << "before saving pixmap:" << QTime::currentTime();
	qDebug() << SG_PREFIX_I << "Saving graph" << this->graph_2d->debug << "took" << draw_time.elapsed() << "ms";
	this->graph_2d->saved_pixmap = this->graph_2d->get_pixmap();
	this->graph_2d->saved_pixmap_valid = true;

	return sg_ret::ok;
}




/**
   Look up view
*/
ProfileViewBase * TrackProfileDialog::find_view(Graph2D * graph_2d) const
{
	for (auto iter = this->views.begin(); iter != this->views.end(); iter++) {
		ProfileViewBase * view = *iter;
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
		ProfileViewBase * view = *iter;
		if (!view->graph_2d) {
			continue;
		}

		/* If dialog window is resized then saved image is no longer valid. */
		view->graph_2d->saved_pixmap_valid = !resized;
		view->draw_track_and_crosshairs(this->trk);
	}
}




sg_ret ProfileViewBase::draw_track_and_crosshairs(Track * trk)
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
		Crosshair2D hover_ch; /* Invalid, don't draw hover crosshair right after a view has been created or resized. */
		hover_ch.debug = "cursor pos";

		ret = this->draw_crosshairs(this->m_selection_ch, hover_ch);
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

	ProfileViewBase * view = this->find_view(graph_2d);
	if (!view) {
		qDebug() << SG_PREFIX_E << "Can't find view";
		return sg_ret::err;
	}

	/*
	  Invalidate. Old crosshair would be invalid in graph with new
	  sizes.

	  TODO: selection of tp should survive resizing of
	  graphs. Maybe we should save not only crosshair position,
	  but also Distance or Time on x axis, and then, based on that
	  value we should re-calculate ::m_selection_ch.
	*/
	view->m_selection_ch = Crosshair2D();

	view->graph_2d->cached_central_n_columns = view->graph_2d->central_get_n_columns();
	view->graph_2d->cached_central_n_rows = view->graph_2d->central_get_n_rows();

	view->graph_2d->saved_pixmap_valid = true;
	view->draw_track_and_crosshairs(this->trk);


	return sg_ret::ok;
}




void ProfileViewBase::create_graph_2d(void)
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




void TrackProfileDialog::save_settings(void)
{
	/* Session settings. */
	ApplicationState::set_integer(VIK_SETTINGS_TRACK_PROFILE_WIDTH, this->profile_width);
	ApplicationState::set_integer(VIK_SETTINGS_TRACK_PROFILE_HEIGHT, this->profile_height);

	/* Just for this session. */
	for (auto iter = this->views.begin(); iter != this->views.end(); iter++) {
		(*iter)->save_settings();
	}
}




void TrackProfileDialog::destroy_cb(void) /* Slot. */
{
	this->save_settings();
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
void ProfileViewBase::create_widgets_layout(void)
{
	this->widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	this->widget->setMinimumSize(500, 300);

	this->main_vbox = new QVBoxLayout();
	this->labels_grid = new QGridLayout();
	this->controls_vbox = new QVBoxLayout();

	QLayout * old = this->widget->layout();
	delete old;
	qDeleteAll(this->widget->children());
	this->widget->setLayout(this->main_vbox);

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




const QString & ProfileViewBase::get_title(void) const
{
	return this->title;
}




void ProfileViewBase::configure_title(void)
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
		ProfileViewBase * view = *iter;
		if (!view) {
			continue;
		}

		view->configure_title();
		view->create_graph_2d();
		view->create_widgets_layout();
		view->configure_labels();
		view->configure_controls();

		this->tabs->addTab(view->widget, view->get_title());

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
		ProfileViewBase * view = *iter;
		if (!view) {
			qDebug() << SG_PREFIX_E << "Can't find profile" << i << "in loop";
			continue;
		}
		view->generate_initial_track_data(trk);
	}
}




void ProfileViewBase::configure_labels(void)
{
	switch (this->graph_2d->x_domain) {
	case GisViewportDomain::Distance:
		this->labels.x_label = new QLabel(QObject::tr("Distance From Start:"));
		this->labels.x_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);

		/* Additional absolute timestamp to provide more information in UI. */
		this->labels.tp_timestamp_label = new QLabel(QObject::tr("Trackpoint timestamp:"));
		this->labels.tp_timestamp_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);

		break;

	case GisViewportDomain::Time:
		this->labels.x_label = new QLabel(QObject::tr("Time From Start:"));
		this->labels.x_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);

		/* Additional absolute timestamp to provide more information in UI. */
		this->labels.tp_timestamp_label = new QLabel(QObject::tr("Trackpoint timestamp:"));
		this->labels.tp_timestamp_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);

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
		this->labels.y_label = new QLabel(QObject::tr("Distance From Start:"));
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

	this->labels_grid->addWidget(this->labels.tp_timestamp_label, row, 0, Qt::AlignLeft);
	this->labels_grid->addWidget(this->labels.tp_timestamp_value, row, 1, Qt::AlignRight);
	this->labels_grid->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum), row, 3);
	row++;

	return;
}




void ProfileViewET::configure_controls(void)
{
	bool value;


	this->show_dem_cb = new QCheckBox(QObject::tr("Show DEM"), this->dialog);
	if (ApplicationState::get_boolean(VIK_SETTINGS_TRACK_PROFILE_ET_SHOW_DEM_ELEVATION, &value)) {
		this->show_dem_cb->setCheckState(value ? Qt::Checked : Qt::Unchecked);
	}
	this->controls_vbox->addWidget(this->show_dem_cb);
	QObject::connect(this->show_dem_cb, SIGNAL (stateChanged(int)), this->dialog, SLOT (checkbutton_toggle_cb()));

	this->show_gps_speed_cb = new QCheckBox(QObject::tr("Show GPS Speed (relative)"), dialog);
	if (ApplicationState::get_boolean(VIK_SETTINGS_TRACK_PROFILE_ET_SHOW_GPS_SPEED, &value)) {
		this->show_gps_speed_cb->setCheckState(value ? Qt::Checked : Qt::Unchecked);
	}
	this->controls_vbox->addWidget(this->show_gps_speed_cb);
	QObject::connect(this->show_gps_speed_cb, SIGNAL (stateChanged(int)), this->dialog, SLOT (checkbutton_toggle_cb()));
}




void ProfileViewSD::configure_controls(void)
{
	bool value;

	this->show_gps_speed_cb = new QCheckBox(QObject::tr("Show GPS Speed (relative)"), this->dialog);
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
	if (ApplicationState::get_boolean(VIK_SETTINGS_TRACK_PROFILE_ED_SHOW_DEM_ELEVATION, &value)) {
		this->show_dem_cb->setCheckState(value ? Qt::Checked : Qt::Unchecked);
	}
	this->controls_vbox->addWidget(this->show_dem_cb);
	QObject::connect(this->show_dem_cb, SIGNAL (stateChanged(int)), this->dialog, SLOT (checkbutton_toggle_cb()));

	this->show_gps_speed_cb = new QCheckBox(QObject::tr("Show GPS Speed (relative)"), this->dialog);
	if (ApplicationState::get_boolean(VIK_SETTINGS_TRACK_PROFILE_ED_SHOW_GPS_SPEED, &value)) {
		this->show_gps_speed_cb->setCheckState(value ? Qt::Checked : Qt::Unchecked);
	}
	this->controls_vbox->addWidget(this->show_gps_speed_cb);
	QObject::connect(this->show_gps_speed_cb, SIGNAL (stateChanged(int)), this->dialog, SLOT (checkbutton_toggle_cb()));
}




void ProfileViewGD::configure_controls(void)
{
	bool value;

	this->show_gps_speed_cb = new QCheckBox(QObject::tr("Show GPS Speed (relative)"), this->dialog);
	if (ApplicationState::get_boolean(VIK_SETTINGS_TRACK_PROFILE_GD_SHOW_GPS_SPEED, &value)) {
		this->show_gps_speed_cb->setCheckState(value ? Qt::Checked : Qt::Unchecked);
	}
	this->controls_vbox->addWidget(this->show_gps_speed_cb);
	QObject::connect(this->show_gps_speed_cb, SIGNAL (stateChanged(int)), this->dialog, SLOT (checkbutton_toggle_cb()));
}




void ProfileViewST::configure_controls(void)
{
	bool value;

	this->show_gps_speed_cb = new QCheckBox(QObject::tr("Show GPS Speed (relative)"), this->dialog);
	if (ApplicationState::get_boolean(VIK_SETTINGS_TRACK_PROFILE_ST_SHOW_GPS_SPEED, &value)) {
		this->show_gps_speed_cb->setCheckState(value ? Qt::Checked : Qt::Unchecked);
	}
	this->controls_vbox->addWidget(this->show_gps_speed_cb);
	QObject::connect(this->show_gps_speed_cb, SIGNAL (stateChanged(int)), this->dialog, SLOT (checkbutton_toggle_cb()));
}




void ProfileViewDT::configure_controls(void)
{
	bool value;

	this->show_gps_speed_cb = new QCheckBox(QObject::tr("Show GPS Speed (relative)"), this->dialog);
	if (ApplicationState::get_boolean(VIK_SETTINGS_TRACK_PROFILE_DT_SHOW_GPS_SPEED, &value)) {
		this->show_gps_speed_cb->setCheckState(value ? Qt::Checked : Qt::Unchecked);
	}
	this->controls_vbox->addWidget(this->show_gps_speed_cb);
	QObject::connect(this->show_gps_speed_cb, SIGNAL (stateChanged(int)), this->dialog, SLOT (checkbutton_toggle_cb()));
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




ProfileViewBase::ProfileViewBase(GisViewportDomain new_x_domain, GisViewportDomain new_y_domain, TrackProfileDialog * new_dialog, QWidget * parent)
{
	this->widget = new QWidget(parent);
	this->widget->setProperty(MY_WIDGET_PROPERTY, QVariant::fromValue((qulonglong) this));
	this->dialog = new_dialog;

	if (!ProfileViewBase::domains_are_supported(new_x_domain, new_y_domain)) {
		qDebug() << SG_PREFIX_E << "Unhandled combination of x/y domains:" << (int) new_x_domain << (int) new_y_domain;
	}

	this->main_pen.setColor("lightsteelblue");
	this->main_pen.setWidth(1);

	this->gps_speed_pen.setColor("orange");
	this->dem_alt_pen.setColor("green");
	this->no_alt_info_pen.setColor("yellow");

	this->x_domain = new_x_domain;
	this->y_domain = new_y_domain;
}




ProfileViewBase::~ProfileViewBase()
{
	/* TODO: delete ::widget? */
}




ProfileViewET::ProfileViewET(TrackProfileDialog * new_dialog) : ProfileView<Time, Time_ll>(GisViewportDomain::Time,     GisViewportDomain::Elevation, new_dialog) {}
ProfileViewSD::ProfileViewSD(TrackProfileDialog * new_dialog) : ProfileView<Distance, Distance_ll>(GisViewportDomain::Distance, GisViewportDomain::Speed,     new_dialog) {}
ProfileViewED::ProfileViewED(TrackProfileDialog * new_dialog) : ProfileView<Distance, Distance_ll>(GisViewportDomain::Distance, GisViewportDomain::Elevation, new_dialog) {}
ProfileViewGD::ProfileViewGD(TrackProfileDialog * new_dialog) : ProfileView<Distance, Distance_ll>(GisViewportDomain::Distance, GisViewportDomain::Gradient,  new_dialog) {}
ProfileViewST::ProfileViewST(TrackProfileDialog * new_dialog) : ProfileView<Time, Time_ll>(GisViewportDomain::Time,     GisViewportDomain::Speed,     new_dialog) {}
ProfileViewDT::ProfileViewDT(TrackProfileDialog * new_dialog) : ProfileView<Time, Time_ll>(GisViewportDomain::Time,     GisViewportDomain::Distance,  new_dialog) {}



template <typename Tx, typename Tx_ll>
ProfileView<Tx, Tx_ll>::~ProfileView()
{
	delete this->graph_2d;
}



namespace SlavGPS {


/* Create initial track data using appropriate Track method. */
template <>
sg_ret ProfileView<Distance, Distance_ll>::generate_initial_track_data(Track * trk)
{
	this->initial_track_data.invalidate();

	/*
	  It may be time consuming to convert units on whole long,
	  uncompressed ::initial_track_data. We could decide to do
	  this on compressed (and thus much smaller)
	  ::track_data_to_draw.

	  But in this place we do it only once, when a dialog is being
	  opened. Once it is done, we don't have to re-do it on every
	  resizing of dialog window.
	*/

	if (this->graph_2d->y_domain == GisViewportDomain::Elevation) {
		this->initial_track_data.make_track_data_altitude_over_distance(trk, trk->get_tp_count());

	} else if (this->graph_2d->y_domain == GisViewportDomain::Gradient) {
		this->initial_track_data.make_track_data_gradient_over_distance(trk, trk->get_tp_count());

	} else if (this->graph_2d->y_domain == GisViewportDomain::Speed) {
		this->initial_track_data.make_track_data_speed_over_distance(trk);
	} else {
		qDebug() << SG_PREFIX_E << "Unhandled y domain" << (int) this->graph_2d->y_domain;
	}

	if (!this->initial_track_data.valid) {
		qDebug() << SG_PREFIX_E << "Failed to generate valid initial track data for" << this->get_title();
		return sg_ret::err;
	}

	this->initial_track_data.apply_unit_conversions(this->graph_2d->speed_unit, this->graph_2d->distance_unit, this->graph_2d->height_unit);

	qDebug() << SG_PREFIX_I << "Generated valid initial track data for" << this->get_title();
	return sg_ret::ok;
}




template <>
sg_ret ProfileView<Time, Time_ll>::generate_initial_track_data(Track * trk)
{
	this->initial_track_data.invalidate();

	/*
	  It may be time consuming to convert units on whole long,
	  uncompressed ::initial_track_data. We could decide to do
	  this on compressed (and thus much smaller)
	  ::track_data_to_draw.

	  But in this place we do it only once, when a dialog is being
	  opened. Once it is done, we don't have to re-do it on every
	  resizing of dialog window.
	*/

	if (this->graph_2d->y_domain == GisViewportDomain::Speed) {
		this->initial_track_data.make_track_data_speed_over_time(trk);

	} else if (this->graph_2d->y_domain == GisViewportDomain::Distance) {
		this->initial_track_data.make_track_data_distance_over_time(trk);

	} else if (this->graph_2d->y_domain == GisViewportDomain::Elevation) {
		this->initial_track_data.make_track_data_altitude_over_time(trk);
	} else {
		qDebug() << SG_PREFIX_E << "Unhandled y domain" << (int) this->graph_2d->y_domain;
	}

	if (!this->initial_track_data.valid) {
		qDebug() << SG_PREFIX_E << "Failed to generate valid initial track data for" << this->get_title();
		return sg_ret::err;
	}

	this->initial_track_data.apply_unit_conversions(this->graph_2d->speed_unit, this->graph_2d->distance_unit, this->graph_2d->height_unit);

	qDebug() << SG_PREFIX_I << "Generated valid initial track data for" << this->get_title();
	return sg_ret::ok;
}


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
void find_multiples_of_interval(const T & min_visible, const T & max_visible, const T & interval, T & first_multiple, T & last_multiple)
{
	int n = 0;

	n = floor(min_visible / interval);
	first_multiple = interval * (n - 1);

	n = ceil(max_visible / interval);
	last_multiple = interval * (n + 1);

	qDebug() << SG_PREFIX_I
		 << "min_visible =" << min_visible
		 << ", max_visible =" << max_visible
		 << ", interval =" << interval
		 << ", first multiple =" << first_multiple
		 << ", last multiple =" << last_multiple;

#if 0

	int k = 0;
	qDebug() << SG_PREFIX_I << "min_visible =" << min_visible << ", max_visible =" << max_visible << QTime::currentTime();
	/* 'first_multiple * y_interval' will be below y_visible_min. */
	if (min_visible <= 0) {

		qDebug() << SG_PREFIX_I << "Before first loop:" << QTime::currentTime();
		while (interval * first_multiple > min_visible) {
			first_multiple--;
			k++;
		}
		qDebug() << SG_PREFIX_I << "After first loop:" << QTime::currentTime() << "total iterations =" << k;
	} else {
		qDebug() << SG_PREFIX_I << "Before second loop:" << QTime::currentTime();
		while (interval * first_multiple + interval < min_visible) {
			first_multiple++;
			k++;
		}
		qDebug() << SG_PREFIX_I << "After second loop:" << QTime::currentTime() << "total iterations =" << k;
	}

	/* 'last_multiple * y_interval' will be above y_visible_max. */
	if (max_visible <= 0) {
		qDebug() << SG_PREFIX_I << "Before third loop:" << QTime::currentTime();
		while (interval * last_multiple - interval > max_visible) {
			last_multiple--;
			k++;
		}
		qDebug() << SG_PREFIX_I << "After third loop:" << QTime::currentTime() << "total iterations =" << k;
	} else {
		qDebug() << SG_PREFIX_I << "Before fourth loop:" << QTime::currentTime();
		while (interval * last_multiple < max_visible) {
			last_multiple++;
			k++;
		}
		qDebug() << SG_PREFIX_I << "After fourth loop:" << QTime::currentTime() << "total iterations =" << k;
	}

	if (k > 30) {
		/* I've encountered a problem where this function has
		   been executing for 3 seconds and one of while loops
		   was making 25034897 iterations. It should be only a
		   few of iterations. */
		qDebug() SG_PREFIX_E << "Problems with calculating grid line indices, k =" << k;
	}
#endif
}




template <typename Tx, typename Tx_ll>
void ProfileView<Tx, Tx_ll>::draw_y_grid(void)
{
	if (this->y_visible_range_uu < 0.000001) {
		qDebug() << SG_PREFIX_E << "Zero visible range:" << this->y_visible_range_uu;
		return;
	}

	const int n_columns      = this->get_central_n_columns();
	const int n_rows         = this->get_central_n_rows();
	const int left_width     = this->graph_2d->left_get_width();
	const int left_height    = this->graph_2d->left_get_height();
	const int leftmost_px    = this->graph_2d->central_get_leftmost_pixel();
	const int topmost_px     = this->graph_2d->central_get_topmost_pixel();
	const int bottommost_px  = this->graph_2d->central_get_bottommost_pixel();


	if (this->y_visible_range_uu < 0.0000001) {
		qDebug() << SG_PREFIX_E << "Zero visible range:" << this->x_visible_min << this->x_visible_max;
		return;
	}
	const double y_pixels_per_unit = 1.0 * n_rows / this->y_visible_range_uu;

	double first_multiple = 0;
	double last_multiple = 0;
	find_multiples_of_interval(this->y_visible_min, this->y_visible_max, this->y_interval, first_multiple, last_multiple);

#if 1   /* Debug. */
	qDebug() << SG_PREFIX_D << "      graph:" << this->get_title();
	qDebug() << SG_PREFIX_D << "      visible range =" << this->y_visible_range_uu;
	qDebug() << SG_PREFIX_D << "      n rows =" << n_rows << ", n cols =" << n_columns;
	qDebug() << SG_PREFIX_D << "      leftmost px =" << leftmost_px << ", bottommost px =" << bottommost_px;
	qDebug() << SG_PREFIX_D << "      y visible min =" << this->y_visible_min << ", y visible max =" << this->y_visible_max;
	qDebug() << SG_PREFIX_D << "      interval =" << this->y_interval << ", first_multiple =" << first_multiple << ", last_multiple = " << last_multiple;
	qDebug() << SG_PREFIX_D << "      y pixels per unit =" << y_pixels_per_unit;
#endif



	/* Be sure to keep type of value_uu as floating-point
	   compatible, otherwise for intervals smaller than 1.0 you
	   will get forever loop. */
	for (double value_uu = first_multiple; value_uu <= last_multiple; value_uu += this->y_interval) {
		const double value_from_edge_uu = value_uu - this->y_visible_min;
		/* 'y_px' is in "beginning in top-left corner" coordinate system. */
		const int y_px = bottommost_px - y_pixels_per_unit * value_from_edge_uu;

		if (y_px >= topmost_px && y_px <= bottommost_px) {
			qDebug() << SG_PREFIX_D << "      value (inside) =" << value_uu << ", y_px =" << y_px;

			/* Graph line. From left edge of central area to right edge of central area. */
			this->graph_2d->central_draw_line(this->graph_2d->grid_pen,
							  leftmost_px,             y_px,
							  leftmost_px + n_columns, y_px);

			/* Text label in left margin. */
			const QRectF bounding_rect = QRectF(2, y_px, left_width - 4, left_height);
			const QString label = this->get_y_grid_label(value_uu);
			this->graph_2d->draw_text(this->graph_2d->labels_font, this->graph_2d->labels_pen, bounding_rect, Qt::AlignRight | Qt::AlignTop, label, TextOffset::Up);
		} else {
			qDebug() << SG_PREFIX_N << "      value (outside) =" << value_uu << ", y_px =" << y_px;
		}
	}
}





namespace SlavGPS {


template <>
QString ProfileView<Time, Time_ll>::get_x_grid_label(const Time & value_uu)
{
	return get_time_grid_label(this->x_interval, value_uu);
}



template <>
QString ProfileView<Distance, Distance_ll>::get_x_grid_label(const Distance & value_uu)
{
	return value_uu.to_nice_string();
}


}





template <typename Tx, typename Tx_ll>
void ProfileView<Tx, Tx_ll>::draw_x_grid(void)
{
	const int n_columns      = this->get_central_n_columns();
	const int n_rows         = this->get_central_n_rows();
	const int bottom_width   = this->graph_2d->bottom_get_width();
	const int bottom_height  = this->graph_2d->bottom_get_height();
	const int leftmost_px    = this->graph_2d->central_get_leftmost_pixel();
	const int rightmost_px   = this->graph_2d->central_get_rightmost_pixel();
	const int topmost_px     = this->graph_2d->central_get_topmost_pixel();
	const int bottommost_px  = this->graph_2d->central_get_bottommost_pixel();

	if (this->x_visible_range_uu.is_zero()) {
		qDebug() << SG_PREFIX_E << "Zero visible range:" << this->x_visible_min << this->x_visible_max;
		return;
	}
	const double x_pixels_per_unit = (1.0 * n_columns) / this->x_visible_range_uu;

	Tx first_multiple = 0;
	Tx last_multiple = 0;
	find_multiples_of_interval(this->x_visible_min, this->x_visible_max, this->x_interval, first_multiple, last_multiple);

#if 1   /* Debug. */
	qDebug() << SG_PREFIX_D << "      graph:" << this->get_title();
	qDebug() << SG_PREFIX_D << "      visible range =" << this->x_visible_range_uu;
	qDebug() << SG_PREFIX_D << "      n rows =" << n_rows << ", n cols =" << n_columns;
	qDebug() << SG_PREFIX_D << "      leftmost px =" << leftmost_px << ", bottommost px =" << bottommost_px;
	qDebug() << SG_PREFIX_D << "      x visible min =" << this->x_visible_min << ", x visible max =" << this->x_visible_max;
	qDebug() << SG_PREFIX_D << "      x interval =" << this->x_interval << ", first_multiple =" << first_multiple << ", last_multiple = " << last_multiple;
	qDebug() << SG_PREFIX_D << "      x pixels per unit =" << x_pixels_per_unit;
#endif

	for (Tx value_uu = first_multiple; value_uu <= last_multiple; value_uu += this->x_interval.value) {
		const Tx value_from_edge_uu = value_uu - this->x_visible_min;
		/* 'x_px' is in "beginning in top-left corner" coordinate system. */
		const int x_px = leftmost_px + x_pixels_per_unit * value_from_edge_uu.value;

		if (x_px >= leftmost_px && x_px <= rightmost_px) {
			qDebug() << SG_PREFIX_D << "      value (inside) =" << value_uu << ", x_px =" << x_px;

			/* Graph line. From bottom of central area to top of central area. */
			this->graph_2d->central_draw_line(this->graph_2d->grid_pen,
							  x_px, topmost_px,
							  x_px, topmost_px + n_rows);

			/* Text label in bottom margin. */
			const QRectF bounding_rect = QRectF(x_px, bottommost_px + 1, bottom_width - 3, bottom_height - 3);
			const QString label = this->get_x_grid_label(value_uu);
			this->graph_2d->draw_text(this->graph_2d->labels_font, this->graph_2d->labels_pen, bounding_rect, Qt::AlignLeft | Qt::AlignTop, label, TextOffset::Left);
		} else {
			qDebug() << SG_PREFIX_N << "      value (outside) =" << value_uu << ", x_px =" << x_px;
		}
	}
}




template <typename Tx, typename Tx_ll>
QString ProfileView<Tx, Tx_ll>::get_y_grid_label(double value_uu)
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




bool ProfileViewBase::domains_are_supported(GisViewportDomain x_domain, GisViewportDomain y_domain)
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




int ProfileViewBase::get_central_n_columns(void) const
{
	return this->graph_2d->cached_central_n_columns;
}




int ProfileViewBase::get_central_n_rows(void) const
{
	return this->graph_2d->cached_central_n_rows;
}




Graph2D::Graph2D(int left, int right, int top, int bottom, QWidget * parent) : ViewportPixmap(left, right, top, bottom, parent)
{
	this->height_unit = Preferences::get_unit_height();
	this->distance_unit = Preferences::get_unit_distance();
	this->speed_unit = Preferences::get_unit_speed();

	this->setMouseTracking(true); /* Without this the ::mouseMoveEvent() method won't be called. */
}




/**
   @reviewed-on tbd
*/
sg_ret Graph2D::cbl_get_cursor_pos(QMouseEvent * ev, ScreenPos & screen_pos) const
{
	const int leftmost_px   = this->central_get_leftmost_pixel();
	const int rightmost_px  = this->central_get_rightmost_pixel();
	const int topmost_px    = this->central_get_topmost_pixel();
	const int bottommost_px = this->central_get_bottommost_pixel();

	const QPoint position = this->mapFromGlobal(QCursor::pos());

#if 0   /* Verbose debug. */
	qDebug() << SG_PREFIX_I << "Difference in cursor position: dx = " << position.x() - ev->x() << ", dy = " << position.y() - ev->y();
#endif

#if 0
	const int x_px = position.x();
	const int y_px = position.y();
#else
	const int x_px = ev->x();
	const int y_px = ev->y();
#endif

	/* Cursor outside of chart area. */
	if (x_px > rightmost_px) {
		return sg_ret::err;
	}
	if (y_px > bottommost_px) {
		return sg_ret::err;
	}
	if (x_px < leftmost_px) {
		return sg_ret::err;
	}
	if (y_px < topmost_px) {
		return sg_ret::err;
	}

	/* Converting from Qt's "beginning is in upper-left" into "beginning is in bottom-left" coordinate system. */
	screen_pos.rx() = x_px;
	screen_pos.ry() = bottommost_px - y_px;

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
	const int leftmost_px = this->central_get_leftmost_pixel();
	const int rigthmost_px = this->central_get_rightmost_pixel();
	const int topmost_px = this->central_get_topmost_pixel();
	const int bottommost_px = this->central_get_bottommost_pixel();

	if (!crosshair.valid) {
		qDebug() << SG_PREFIX_E << "Crosshair" << crosshair.debug << "is invalid";
		/* Position outside of graph area. */
		return;
	}

	//qDebug() << SG_PREFIX_I << "Crosshair" << crosshair.debug << "at coord" << crosshair.x_px << crosshair.y_px << "(central cbl =" << crosshair.central_cbl_x_px << crosshair.central_cbl_y_px << ")";

	if (crosshair.x_px > rigthmost_px || crosshair.x_px < leftmost_px) {
		qDebug() << SG_PREFIX_E << "Crosshair has bad x";
		/* Position outside of graph area. */
		return;
	}
	if (crosshair.y_px > bottommost_px || crosshair.y_px < topmost_px) {
		qDebug() << SG_PREFIX_E << "Crosshair has bad y";
		/* Position outside of graph area. */
		return;
	}


	this->painter.setPen(this->marker_pen);

	/* Small optimization: use QT's drawing primitives directly.
	   Remember that (0,0) screen position is in upper-left corner of viewport. */

	this->painter.drawLine(leftmost_px, crosshair.y_px, rigthmost_px, crosshair.y_px); /* Horizontal line. */
	this->painter.drawLine(crosshair.x_px, topmost_px, crosshair.x_px, bottommost_px); /* Vertical line. */
}
