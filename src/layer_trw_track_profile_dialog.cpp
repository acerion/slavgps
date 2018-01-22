/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2007, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2007-2008, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2012-2014, Rob Norris <rw_norris@hotmail.com>
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

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cassert>

#include <QDebug>

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
#include "globals.h"
#include "preferences.h"
#include "measurements.h"




using namespace SlavGPS;




#define VIK_SETTINGS_TRACK_PROFILE_WIDTH "track_profile_display_width"
#define VIK_SETTINGS_TRACK_PROFILE_HEIGHT "track_profile_display_height"

#define PREFIX " Track Profile:" << __FUNCTION__ << __LINE__ << ">"




enum {
	SG_TRACK_PROFILE_CANCEL,
	SG_TRACK_PROFILE_SPLIT_AT_MARKER,
	SG_TRACK_PROFILE_SPLIT_SEGMENTS,
	SG_TRACK_PROFILE_REVERSE,
	SG_TRACK_PROFILE_OK,
};




static Intervals <time_t> * time_intervals;
static Intervals <double> * distance_intervals;
static Intervals <double> * altitude_intervals;
static Intervals <double> * gradient_intervals;
static Intervals <double> * speed_intervals;





/* (Hopefully!) Human friendly altitude grid sizes - note no fixed 'ratio' just numbers that look nice... */
static const double altitude_interval_values[] = {1.0, 2.0, 4.0, 5.0, 10.0, 15.0, 20.0,
						  25.0, 40.0, 50.0, 75.0, 100.0,
						  150.0, 200.0, 250.0, 375.0, 500.0,
						  750.0, 1000.0, 2000.0, 5000.0, 10000.0, 100000.0};

/* (Hopefully!) Human friendly gradient grid sizes - note no fixed 'ratio' just numbers that look nice... */
static const double gradient_interval_values[] = {1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 10.0,
						  12.0, 15.0, 20.0, 25.0, 30.0, 35.0, 40.0, 45.0, 50.0, 75.0,
						  100.0, 150.0, 200.0, 250.0, 375.0, 500.0,
						  750.0, 1000.0, 10000.0, 100000.0};
/* Normally gradients should range up to couple hundred precent at most,
   however there are possibilities of having points with no altitude after a point with a big altitude
  (such as places with invalid DEM values in otherwise mountainous regions) - thus giving huge negative gradients. */

/* (Hopefully!) Human friendly grid sizes - note no fixed 'ratio' just numbers that look nice... */
/* As need to cover walking speeds - have many low numbers (but also may go up to airplane speeds!). */
static const double speed_interval_values[] = {1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 10.0,
					       15.0, 20.0, 25.0, 40.0, 50.0, 75.0,
					       100.0, 150.0, 200.0, 250.0, 375.0, 500.0,
					       750.0, 1000.0, 10000.0};

/* (Hopefully!) Human friendly distance grid sizes - note no fixed 'ratio' just numbers that look nice... */
static const double distance_interval_values[] = {0.1, 0.2, 0.5,
						  1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 10.0,
						  15.0, 20.0, 25.0, 40.0, 50.0, 75.0,
						  100.0, 150.0, 200.0, 250.0, 375.0, 500.0,
						  750.0, 1000.0, 10000.0};

/* Time intervals in seconds. */
static const time_t time_interval_values[] = {
	60,     /* 1 minute. */
	120,    /* 2 minutes. */
	300,    /* 5 minutes. */
	900,    /* 15 minutes. */
	1800,   /* half hour. */
	3600,   /* 1 hour. */
	10800,  /* 3 hours. */
	21600,  /* 6 hours. */
	43200,  /* 12 hours. */
	86400,  /* 1 day. */
	172800, /* 2 days. */
	604800, /* 1 week. */
	1209600,/* 2 weeks. */
	2419200,/* 4 weeks. */
};




/* Local show settings to restore on dialog opening. */
static bool g_show_dem                = true;
static bool g_show_alt_gps_speed      = true;
static bool g_show_gps_speed          = true;
static bool g_show_gradient_gps_speed = true;
static bool g_show_dist_speed         = false;
static bool g_show_elev_speed         = false;
static bool g_show_elev_dem           = false;
static bool g_show_sd_gps_speed       = true;




static void time_label_update(QLabel * label, time_t seconds_from_start);
static void real_time_label_update(QLabel * label, Trackpoint * tp);
static QString get_y_distance_string(double distance);

static QString get_speed_grid_label(SpeedUnit speed_unit, double value);
static QString get_elevation_grid_label(HeightUnit height_unit, double value);
static QString get_distance_grid_label(DistanceUnit distance_unit, double value);
static QString get_distance_grid_label_2(DistanceUnit distance_unit, int interval_index, double value);
static QString get_time_grid_label(int interval_index, int value);
static QString get_time_grid_label_2(time_t interval_value, time_t value);

static QString get_graph_title(void);




TrackProfileDialog::~TrackProfileDialog()
{
	delete time_intervals;
	delete distance_intervals;
	delete altitude_intervals;
	delete gradient_intervals;
	delete speed_intervals;

	for (int i = 0; i < SG_TRACK_PROFILE_TYPE_MAX; i++ ) {
		delete this->graphs[i];
	}
}




#define GRAPH_INITIAL_WIDTH 400
#define GRAPH_INITIAL_HEIGHT 300

#define GRAPH_MARGIN_LEFT 80 // 70
#define GRAPH_MARGIN_RIGHT 40 // 1
#define GRAPH_MARGIN_TOP 20
#define GRAPH_MARGIN_BOTTOM 30 // 1
#define GRAPH_X_INTERVALS 5
#define GRAPH_Y_INTERVALS 5




bool ProfileGraph::regenerate_data_from_scratch(Track * trk)
{
	/* First create track data using appropriate Track method. */

	if (this->geocanvas.y_domain == GeoCanvasDomain::Elevation && this->geocanvas.x_domain == GeoCanvasDomain::Distance) {
		this->track_data = trk->make_values_vector_altitude_distance(this->width);
	} else if (this->geocanvas.y_domain == GeoCanvasDomain::Gradient && this->geocanvas.x_domain == GeoCanvasDomain::Distance) {
		this->track_data = trk->make_values_vector_gradient_distance(this->width);
	} else if (this->geocanvas.y_domain == GeoCanvasDomain::Speed && this->geocanvas.x_domain == GeoCanvasDomain::Time) {
		this->track_data = trk->make_values_vector_speed_time(this->width);
	} else if (this->geocanvas.y_domain == GeoCanvasDomain::Distance && this->geocanvas.x_domain == GeoCanvasDomain::Time) {
		this->track_data = trk->make_values_vector_distance_time(this->width);
	} else if (this->geocanvas.y_domain == GeoCanvasDomain::Elevation && this->geocanvas.x_domain == GeoCanvasDomain::Time) {
		this->track_data = trk->make_values_vector_altitude_time(this->width);
	} else if (this->geocanvas.y_domain == GeoCanvasDomain::Speed && this->geocanvas.x_domain == GeoCanvasDomain::Distance) {
		this->track_data = trk->make_values_vector_speed_distance(this->width);
	} else {
		qDebug() << "EE:" PREFIX << "unhandled x/y domain" << (int) this->geocanvas.x_domain << (int) this->geocanvas.y_domain;
	}

	if (!this->track_data.valid) {
		return false;
	}


	qDebug() << "II:" PREFIX << "generated value vector for" << this->get_graph_title() << ", will now adjust y values";


	/* Do necessary adjustments to y values. */

	switch (this->geocanvas.y_domain) {
	case GeoCanvasDomain::Speed:
		/* Convert into appropriate units. */
		for (int i = 0; i < this->width; i++) {
			this->track_data.y[i] = convert_speed_mps_to(this->track_data.y[i], this->geocanvas.speed_unit);
		}

		qDebug() << "kamil calculating min/max y speed for" << this->get_graph_title();
		this->track_data.calculate_min_max();
		if (this->track_data.y_min < 0.0) {
			this->track_data.y_min = 0; /* Splines sometimes give negative speeds. */
		}
		break;
	case GeoCanvasDomain::Elevation:
		/* Convert into appropriate units. */
		if (this->geocanvas.height_unit == HeightUnit::FEET) {
			/* Convert altitudes into feet units. */
			for (int i = 0; i < this->width; i++) {
				this->track_data.y[i] = VIK_METERS_TO_FEET(this->track_data.y[i]);
			}
		}
		/* Otherwise leave in metres. */

		this->track_data.calculate_min_max();
		break;
	case GeoCanvasDomain::Distance:
		/* Convert into appropriate units. */
		for (int i = 0; i < this->width; i++) {
			this->track_data.y[i] = convert_distance_meters_to(this->track_data.y[i], this->geocanvas.distance_unit);
		}

		this->track_data.y_min = 0;
		this->track_data.y_max = convert_distance_meters_to(trk->get_length_including_gaps(), this->geocanvas.distance_unit);

		break;
	case GeoCanvasDomain::Gradient:
		/* No unit conversion needed. */
		this->track_data.calculate_min_max();
		break;
	default:
		qDebug() << "EE:" PREFIX << "unhandled y domain" << (int) this->geocanvas.y_domain;
		break;
	};



	qDebug() << "II:" PREFIX << "after calling calculate_min_max; x_min/x_max =" << this->track_data.x_min << this->track_data.x_max << this->get_graph_title();



	/* Prepare x/y-range values. */

	switch (this->geocanvas.x_domain) {
	case GeoCanvasDomain::Distance:
		this->set_initial_visible_range_x_distance();
		break;
	case GeoCanvasDomain::Time:
		this->set_initial_visible_range_x_time();
		break;
	default:
		qDebug() << "EE:" PREFIX << "unhandled x domain" << (int) this->geocanvas.x_domain;
		break;
	};

	this->set_initial_visible_range_y();

	qDebug() << "II:" PREFIX << "return";


	return true;
}




/* This method is used for purposes of determining how large a
   distance - an interval of values - will be if we split min-max
   range into n_intervals. Then there will be n_interval grid lines
   drawn on a graph, each spaced at interval. */
template <class T>
int Intervals<T>::get_interval_index(T min, T max, int n_intervals)
{
	const T interval_upper_limit = (max - min) / n_intervals;

	/* Search for index of nearest interval. */
	int index = 0;
	while (interval_upper_limit > this->values[index]) {
		index++;
		/* Last Resort Check */
		if (index == this->n_values) {
			/* Return the last valid value. */
			index--;
			qDebug() << "++++" << __FUNCTION__ << __LINE__ << "min/max/n_intervals:" << min << max << n_intervals << "index1 =" << index << ", interval =" << this->values[index];
			return index;
		}
	}

	if (index != 0) {
		/* To cancel out the last index++ in the loop above:
		   that one last increment was one too many. */
		index--;
	}

	qDebug() << "++++" << __FUNCTION__ << __LINE__ << "min/max/n_intervals:" << min << max << n_intervals << "index2 =" << index << ", interval =" << this->values[index];

	return index;
}




template <class T>
T Intervals<T>::get_interval_value(int index)
{
	return this->values[index];
}




void ProfileGraph::set_initial_visible_range_x_distance(void)
{
	/* We won't display any x values outside of
	   track_data.x_min/max. We will never be able to zoom out to
	   show e.g. negative distances. */
	this->x_min_visible_d = convert_distance_meters_to(this->track_data.x_min, this->geocanvas.distance_unit);
	this->x_max_visible_d = convert_distance_meters_to(this->track_data.x_max, this->geocanvas.distance_unit);

	if (this->x_max_visible_d - this->x_min_visible_d == 0) {
		/* TODO: verify what happens if we return here. */
		qDebug() << "EE:" PREFIX << "zero distance span: min/max = " << this->x_min_visible_d << this->x_max_visible_d;
		return;
	}

	/* Now, given the n_intervals value, find a suitable interval
	   index and value that will nicely cover visible range of
	   data. */

	static Intervals<double> * intervals = distance_intervals;
	const int n_intervals = GRAPH_X_INTERVALS;

	int interval_index = intervals->get_interval_index(this->x_min_visible_d, this->x_max_visible_d, n_intervals);
	this->x_interval_d = intervals->values[interval_index];
}




void ProfileGraph::set_initial_visible_range_x_time(void)
{
	/* We won't display any x values outside of
	   track_data.x_min/max. We will never be able to zoom out to
	   show e.g. negative times. */
	this->x_min_visible_t = this->track_data.x_min;
	this->x_max_visible_t = this->track_data.x_max;

	if (this->x_max_visible_t - this->x_min_visible_t == 0) {
		/* TODO: verify what happens if we return here. */
		qDebug() << "EE:" PREFIX << "zero time span: min/max x = " << this->x_min_visible_t << this->x_max_visible_t << this->get_graph_title();
		return;
	}

	/* Now, given the n_intervals value, find a suitable interval
	   index and value that will nicely cover visible range of
	   data. */

	static Intervals<time_t> * intervals = time_intervals;
	const int n_intervals = GRAPH_X_INTERVALS;

	int interval_index = intervals->get_interval_index(this->x_min_visible_t, this->x_max_visible_t, n_intervals);
	this->x_interval_t = intervals->values[interval_index];
}




void ProfileGraph::set_initial_visible_range_y(void)
{
	/* When user will be zooming in and out, and (in particular)
	   moving graph up and down, the y_min/max_visible values will
	   be non-rounded (i.e. random).  Make them non-rounded from
	   the start, and be prepared to handle non-rounded
	   y_min/max_visible from the start. */
	const double range = abs(this->track_data.y_max - this->track_data.y_min);
	if (this->track_data.y_min != 0.0) {
		/* Some graphs better start at zero, e.g. speed graph
		   or distance graph. Showing negative speed values on
		   a graph wouldn't make sense. */
		this->y_min_visible = this->track_data.y_min - range * 0.05;
	}
	this->y_max_visible = this->track_data.y_max + range * 0.05;



	/* Now, given the n_intervals value, find a suitable interval
	   index and value that will nicely cover visible range of
	   data. */
	const int n_intervals = GRAPH_Y_INTERVALS;
	static Intervals<double> * intervals = NULL;

	switch (this->geocanvas.y_domain) {
	case GeoCanvasDomain::Speed:
		intervals = speed_intervals;
		break;
	case GeoCanvasDomain::Elevation:
		intervals = altitude_intervals;
		break;
	case GeoCanvasDomain::Distance:
		intervals = distance_intervals;
		break;
	case GeoCanvasDomain::Gradient:
		intervals = gradient_intervals;
		break;
	default:
		qDebug() << "EE:" PREFIX << "unhandled y domain" << (int) this->geocanvas.y_domain;
		return;
	};

	const int interval_index = intervals->get_interval_index(this->y_min_visible, this->y_max_visible, n_intervals);
	this->y_interval = intervals->values[interval_index];
}




/* Change what is displayed in main viewport in reaction to click event in one of Profile Dialog viewports. */
static Trackpoint * set_center_at_graph_position(int event_x,
						 LayerTRW * trw,
						 Viewport * main_viewport,
						 TrackInfo & track_info,
						 GeoCanvasDomain x_domain,
						 int graph_width)
{
	int x = event_x;
	if (x >= graph_width) {
		qDebug() << "EE: Track Profile: set center: condition 1 error:" << x << graph_width;
		x = graph_width; /* Notice that it's not 'x = graph_width - 1'. Current assignment will put mark at the border of graph. */
	}
	if (x < 0) {
		qDebug() << "EE: Track Profile: set center: condition 2 error:" << x;
		x = 0;
	}

	Trackpoint * tp = NULL;
	switch (x_domain) {
	case GeoCanvasDomain::Time:
		tp = track_info.trk->get_closest_tp_by_percentage_time((double) x / graph_width, NULL);
		break;
	case GeoCanvasDomain::Distance:
		tp = track_info.trk->get_closest_tp_by_percentage_dist((double) x / graph_width, NULL);
		break;
	default:
		qDebug() << "EE:" PREFIX << "unhandled x domain" << (int) x_domain;
		break;
	}

	if (tp) {
		main_viewport->set_center_from_coord(tp->coord, true);
		trw->emit_layer_changed();
	}
	return tp;
}




/**
   Draw two pairs of horizontal and vertical lines intersecting at given position.

   One pair is for position of selected trackpoint.
   The other pair is for current position of cursor.

   Both "pos" arguments should indicate position in graph's coordinate system.
*/
void ProfileGraph::draw_marks(const ScreenPos & selected_pos, const ScreenPos & current_pos, bool & is_selected_drawn, bool & is_current_drawn)
{
	/* Restore previously saved image that has no marks on it, just the graph, grids, borders and margins. */
	if (this->saved_img.valid) {
		/* Debug code. */
		//D qDebug() << "II:" PREFIX << "restoring saved image";
		this->viewport->set_pixmap(this->saved_img.img);
	} else {
		qDebug() << "WW:" PREFIX << "NOT restoring saved image";
	}



	/* Now draw marks on this fresh (restored from saved) image. */

	if (current_pos.x > 0 && current_pos.y > 0) {
		qDebug() << "DD:" PREFIX << "++++++ crosshair pos =" << current_pos.x << current_pos.y;

		/* Here we convert point's position from graph's
		   coordinate system (beginning in bottom-left corner)
		   to viewport coordinates (beginning in upper-left
		   corner + viewport's active area margins). */
		this->viewport->draw_simple_crosshair(ScreenPos(GRAPH_MARGIN_LEFT + current_pos.x, GRAPH_MARGIN_TOP + this->height - current_pos.y));
		is_current_drawn = true;
	} else {
		is_current_drawn = false;
	}

	if (selected_pos.x > 0 && selected_pos.y > 0) {
		/* Here we convert point's position from graph's
		   coordinate system (beginning in bottom-left corner)
		   to viewport coordinates (beginning in upper-left
		   corner + viewport's active area margins). */
		this->viewport->draw_simple_crosshair(ScreenPos(GRAPH_MARGIN_LEFT + selected_pos.x, GRAPH_MARGIN_TOP + this->height - selected_pos.y));
		is_selected_drawn = true;
	} else {
		is_selected_drawn = false;
	}



	if (is_selected_drawn || is_current_drawn) {
		this->viewport->update();
	}
}




/**
   Return the percentage of how far a trackpoint is along a track via the time method.
*/
static double tp_percentage_by_time(TrackInfo & track_info, Trackpoint * tp)
{
	if (tp == NULL) {
		return NAN;
	}

	const time_t t_start = (*track_info.trk->trackpoints.begin())->timestamp;
	const time_t t_end = (*std::prev(track_info.trk->trackpoints.end()))->timestamp;
	const time_t t_total = t_end - t_start;

	return (double) (tp->timestamp - t_start) / t_total;
}




/**
   Return the percentage of how far a trackpoint is a long a track via the distance method.
*/
static double tp_percentage_by_distance(TrackInfo & track_info, Trackpoint * tp)
{
	if (tp == NULL) {
		return NAN;
	}

	double dist = 0.0;
	auto iter = std::next(track_info.trk->trackpoints.begin());
	for (; iter != track_info.trk->trackpoints.end(); iter++) {
		dist += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);
		/* FIXME: This condition assumes that trackpoint variable is not a copy. */
		if (tp == *iter) {
			break;
		}
	}

	double pc = NAN;
	if (iter != track_info.trk->trackpoints.end()) {
		pc = dist / track_info.track_length_including_gaps;
	}
	return pc;
}




/**
   React to mouse button release

   Find a trackpoint corresponding to cursor position when button was released.
   Draw marking for this trackpoint.
*/
void TrackProfileDialog::track_graph_release(Viewport * viewport, QMouseEvent * ev, ProfileGraph * graph)
{
	assert (graph->viewport == viewport);

	graph->width = viewport->get_graph_width();

	const int current_pos_x = graph->get_cursor_pos_x(ev);
	this->selected_tp = set_center_at_graph_position(current_pos_x, this->trw, this->main_viewport, this->track_info, graph->geocanvas.x_domain, graph->width);
	if (this->selected_tp == NULL) {
		/* Unable to get the point so give up. */
		this->button_split_at_marker->setEnabled(false);
		return;
	}

	this->button_split_at_marker->setEnabled(true);


	/* Attempt to redraw marker on all graphs. TODO: why on all graphs, when event was only on one of them? */
	for (int type = SG_TRACK_PROFILE_TYPE_ED; type < SG_TRACK_PROFILE_TYPE_MAX; type++) {

		ProfileGraph * a_graph = this->graphs[type];

		if (!a_graph->viewport) {
			continue;
		}

#ifdef K        /* TODO: is it necessary here? */
		a_graph->width = a_graph->viewport->get_graph_width();
		a_graph->height = a_graph->viewport->get_graph_height();
#endif

		QPointF selected_pos = a_graph->get_position_of_tp(this->track_info, this->selected_tp);
		if (selected_pos.x() == -1 || selected_pos.y() == -1) {
			continue;
		}

		/* Positions passed to draw_marks() are in graph's coordinate system, not viewport's coordinate system. */
		a_graph->draw_marks(ScreenPos(selected_pos.x(), selected_pos.y()),
				    ScreenPos(-1.0, -1.0), /* Don't draw current position on clicks. */
				    this->is_selected_drawn, this->is_current_drawn);
	}

	this->button_split_at_marker->setEnabled(this->is_selected_drawn);
}




bool TrackProfileDialog::track_ed_release_cb(Viewport * viewport, QMouseEvent * ev) /* Slot. */
{
	this->track_graph_release(viewport, ev, this->graphs[SG_TRACK_PROFILE_TYPE_ED]);
	return true;
}




bool TrackProfileDialog::track_gd_release_cb(Viewport * viewport, QMouseEvent * ev) /* Slot. */
{
	this->track_graph_release(viewport, ev, this->graphs[SG_TRACK_PROFILE_TYPE_GD]);
	return true;
}




bool TrackProfileDialog::track_st_release_cb(Viewport * viewport, QMouseEvent * ev) /* Slot. */
{
	this->track_graph_release(viewport, ev, this->graphs[SG_TRACK_PROFILE_TYPE_ST]);
	return true;
}




bool TrackProfileDialog::track_dt_release_cb(Viewport * viewport, QMouseEvent * ev) /* Slot. */
{
	this->track_graph_release(viewport, ev, this->graphs[SG_TRACK_PROFILE_TYPE_DT]);
	return true;
}




bool TrackProfileDialog::track_et_release_cb(Viewport * viewport, QMouseEvent * ev) /* Slot. */
{
	this->track_graph_release(viewport, ev, this->graphs[SG_TRACK_PROFILE_TYPE_ET]);
	return true;
}




bool TrackProfileDialog::track_sd_release_cb(Viewport * viewport, QMouseEvent * ev) /* Slot. */
{
	this->track_graph_release(viewport, ev, this->graphs[SG_TRACK_PROFILE_TYPE_SD]);
	return true;
}




/**
 * Calculate y position for mark on y=f(x) graph.
 */
double ProfileGraph::get_pos_y(double pos_x)
{
	int ix = (int) pos_x;
	/* Ensure ix is inside of graph. */
	if (ix == this->width) {
		ix--;
	}

	return this->height * (this->track_data.y[ix] - this->y_min_visible) / (this->y_max_visible - this->y_min_visible);
}




/* Draw cursor marks on a graph that is a function of distance. */
bool TrackProfileDialog::draw_cursor_by_distance(QMouseEvent * ev, ProfileGraph * graph, double & meters_from_start, int & current_pos_x)
{
	if (!graph->track_data.valid) {
		return false;
	}

	current_pos_x = graph->get_cursor_pos_x(ev);
	if (current_pos_x < 0) {
		return false;
	}

	/* TODO: are these assignments necessary in this function?
	   Shouldn't they be done only once, on resize? */
	graph->width = graph->viewport->get_graph_width();
	graph->height = graph->viewport->get_graph_height();

	this->current_tp = this->track_info.trk->get_closest_tp_by_percentage_dist((double) current_pos_x / graph->width, &meters_from_start);

	double current_pos_y = graph->get_pos_y(current_pos_x);

	double selected_pos_x = -1.0; /* i.e. don't draw unless we get a valid value. */
	double selected_pos_y = -1.0;
	if (true || this->is_selected_drawn) {
		double pc = tp_percentage_by_distance(this->track_info, this->selected_tp);
		if (!std::isnan(pc)) {
			selected_pos_x = pc * graph->width;
			selected_pos_y = graph->get_pos_y(selected_pos_x);
		}
	}

	graph->draw_marks(ScreenPos(selected_pos_x, selected_pos_y), ScreenPos(current_pos_x, current_pos_y), this->is_selected_drawn, this->is_current_drawn);

	return true;
}




/* Draw cursor marks on a graph that is a function of time. */
bool TrackProfileDialog::draw_cursor_by_time(QMouseEvent * ev, ProfileGraph * graph, time_t & seconds_from_start, int & current_pos_x)
{
	if (!graph->track_data.valid) {
		return false;
	}

	current_pos_x = graph->get_cursor_pos_x(ev);
	if (current_pos_x < 0) {
		return false;
	}

	/* TODO: are these assignments necessary in this function?
	   Shouldn't they be done only once, on resize? */
	graph->width = graph->viewport->get_graph_width();
	graph->height = graph->viewport->get_graph_height();

	this->current_tp = this->track_info.trk->get_closest_tp_by_percentage_time((double) current_pos_x / graph->width, &seconds_from_start);

	double current_pos_y = graph->get_pos_y(current_pos_x);

	double selected_pos_x = -1.0; /* i.e. don't draw unless we get a valid value. */
	double selected_pos_y = -1.0;
	if (true || this->is_selected_drawn) {
		double pc = tp_percentage_by_time(this->track_info, this->selected_tp);
		if (!std::isnan(pc)) {
			selected_pos_x = pc * graph->width;
			selected_pos_y = graph->get_pos_y(selected_pos_x);
		}
	}

	graph->draw_marks(ScreenPos(selected_pos_x, selected_pos_y), ScreenPos(current_pos_x, current_pos_y), this->is_selected_drawn, this->is_current_drawn);

	return true;
}




void TrackProfileDialog::handle_cursor_move_ed_cb(Viewport * viewport, QMouseEvent * ev)
{
	const int index = SG_TRACK_PROFILE_TYPE_ED;
	assert (this->graphs[index]->viewport == viewport);

	this->handle_cursor_move(this->graphs[index], ev);
	return;
}




void TrackProfileDialog::handle_cursor_move_gd_cb(Viewport * viewport, QMouseEvent * ev)
{
	const int index = SG_TRACK_PROFILE_TYPE_GD;
	assert (this->graphs[index]->viewport == viewport);

	this->handle_cursor_move(this->graphs[index], ev);
	return;
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




void real_time_label_update(QLabel * label, Trackpoint * tp)
{
	QString result;
	if (tp->has_timestamp) {
		/* Alternatively could use %c format but I prefer a slightly more compact form here.
		   The full date can of course be seen on the Statistics tab. */
		static char tmp_buf[64];
		strftime(tmp_buf, sizeof(tmp_buf), "%X %x %Z", localtime(&tp->timestamp)); /* TODO: translate the string. */
		result = QString(tmp_buf);
	} else {
		result = QObject::tr("No Data");
	}
	label->setText(result);

	return;
}




void TrackProfileDialog::handle_cursor_move_st_cb(Viewport * viewport, QMouseEvent * ev)
{
	const int index = SG_TRACK_PROFILE_TYPE_ST;
	assert (this->graphs[index]->viewport == viewport);

	this->handle_cursor_move(this->graphs[index], ev);
	return;
}




/**
 * Update labels and marker on mouse moves in the distance/time graph.
 */
void TrackProfileDialog::handle_cursor_move_dt_cb(Viewport * viewport, QMouseEvent * ev)
{
	const int index = SG_TRACK_PROFILE_TYPE_DT;
	assert (this->graphs[index]->viewport == viewport);

	this->handle_cursor_move(this->graphs[index], ev);
	return;
}




/**
 * Update labels and marker on mouse moves in the elevation/time graph.
 */
void TrackProfileDialog::handle_cursor_move_et_cb(Viewport * viewport, QMouseEvent * ev)
{
	const int index = SG_TRACK_PROFILE_TYPE_ET;
	assert (this->graphs[index]->viewport == viewport);

	this->handle_cursor_move(this->graphs[index], ev);
	return;
}




void TrackProfileDialog::handle_cursor_move_sd_cb(Viewport * viewport, QMouseEvent * ev)
{
	const int index = SG_TRACK_PROFILE_TYPE_SD;
	assert (this->graphs[index]->viewport == viewport);

	this->handle_cursor_move(this->graphs[index], ev);
	return;
}




void TrackProfileDialog::handle_cursor_move(ProfileGraph * graph, QMouseEvent * ev)
{
	double meters_from_start = 0.0;
	time_t seconds_from_start = 0;
	int current_pos_x = 0;

	switch (graph->geocanvas.x_domain) {
	case GeoCanvasDomain::Distance:
		if (!this->draw_cursor_by_distance(ev, graph, meters_from_start, current_pos_x)) {
			return;
		}
		if (graph->labels.x_value) {
			graph->labels.x_value->setText(get_distance_string(meters_from_start, Preferences::get_unit_distance()));
		}
		break;

	case GeoCanvasDomain::Time:
		if (!this->draw_cursor_by_time(ev, graph, seconds_from_start, current_pos_x)) {
			return;
		}
		if (graph->labels.x_value) {
			time_label_update(graph->labels.x_value, seconds_from_start);
		}
		if (this->current_tp && graph->labels.t_value) {
			real_time_label_update(graph->labels.t_value, this->current_tp);
		}
		break;
	default:
		qDebug() << "EE:" PREFIX << "unhandled x domain" << (int) graph->geocanvas.x_domain;
		break;
	};


	double y = graph->track_data.y[current_pos_x];
	switch (graph->geocanvas.y_domain) {
	case GeoCanvasDomain::Speed:
		if (graph->labels.y_value) {
			/* Even if GPS speed available (tp->speed), the text will correspond to the speed map shown.
			   No conversions needed as already in appropriate units. */
			graph->labels.y_value->setText(Measurements::get_speed_string_dont_recalculate(y));
		}
		break;
	case GeoCanvasDomain::Elevation:
		if (this->current_tp && graph->labels.y_value) {
			/* Recalculate value into target unit. */
			graph->labels.y_value->setText(Measurements::get_altitude_string(this->current_tp->altitude));
		}
		break;
	case GeoCanvasDomain::Distance:
		if (graph->labels.y_value) {
			graph->labels.y_value->setText(get_y_distance_string(y));
		}
		break;
	case GeoCanvasDomain::Gradient:
		if (graph->labels.y_value) {
			graph->labels.y_value->setText(QObject::tr("%1").arg((int) y));
		}
		break;
	default:
		qDebug() << "EE:" PREFIX << "unhandled y domain" << (int) graph->geocanvas.y_domain;
		break;
	};

	return;
}




int ProfileGraph::get_cursor_pos_x(QMouseEvent * ev) const
{
	const int graph_width = this->viewport->get_graph_width();
	const int graph_height = this->viewport->get_graph_height();
	const int graph_left_edge = this->viewport->get_graph_left_edge();
	const int graph_top_edge = this->viewport->get_graph_top_edge();

	QPoint position = this->viewport->mapFromGlobal(QCursor::pos());

	//qDebug() << "II:" PREFIX << "x =" << ev->x() << "y =" << ev->y();

#if 0   /* Verbose debug. */
	qDebug() << "II:" PREFIX << "difference in cursor position: dx = " << position.x() - ev->x() << ", dy = " << position.y() - ev->y();
#endif

#if 0
	const int mouse_x = position.x();
	const int mouse_y = position.y();
#else
	const int mouse_x = ev->x();
	const int mouse_y = ev->y();
#endif

	if (mouse_x < graph_left_edge || mouse_x > graph_left_edge + graph_width) {
		/* Cursor outside of chart area. */
		return -1;
	}
	if (mouse_y < graph_top_edge || mouse_y > graph_top_edge + graph_height) {
		/* Cursor outside of chart area. */
		return -1;
	}

	int x = mouse_x - graph_left_edge;
	if (x < 0) {
		qDebug() << "EE: Track Profile: condition 1 for mouse movement failed:" << x << mouse_x << graph_left_edge;
		x = 0;
	}

	if (x > graph_width) {
		qDebug() << "EE: Track Profile: condition 2 for mouse movement failed:" << x << mouse_x << graph_width;
		x = graph_width;
	}

	return x;
}




/* TODO: don't we have a function for this kind of stuff in measurements.cpp? */
QString get_y_distance_string(double distance)
{
	QString result;

	switch (Preferences::get_unit_distance()) {
	case DistanceUnit::MILES:
		result = QObject::tr("%1 miles").arg(distance, 0, 'f', 2);
		break;
	case DistanceUnit::NAUTICAL_MILES:
		result = QObject::tr("%1 NM").arg(distance, 0, 'f', 2);
		break;
	default:
		result = QObject::tr("%1 km").arg(distance, 0, 'f', 2); /* kamilTODO: why not distance/1000? */
		break;
	}

	return result;
}




/**
 * Draws DEM points and a respresentative speed on the supplied pixmap
 * (which is the elevations graph).
 */
void ProfileGraph::draw_dem_alt_speed_dist(Track * trk, double max_speed_in, bool do_dem, bool do_speed)
{
	double max_function_arg = trk->get_length_including_gaps();
	double max_function_value_speed = 0;

	/* Calculate the max speed factor. */
	if (do_speed) {
		max_function_value_speed = max_speed_in * 110 / 100;
	}

	double current_function_arg = 0.0;
	const double max_function_value_dem = this->y_max_visible;

	const QColor dem_color = this->dem_alt_pen.color();
	const QColor speed_color = this->gps_speed_pen.color();

	for (auto iter = std::next(trk->trackpoints.begin()); iter != trk->trackpoints.end(); iter++) {

		current_function_arg += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);
		if (do_dem) {
			int16_t elev = DEMCache::get_elev_by_coord(&(*iter)->coord, DemInterpolation::BEST);
			if (elev != DEM_INVALID_ELEVATION) {
				/* Convert into height units. */
				if (this->geocanvas.height_unit == HeightUnit::FEET) {
					elev = VIK_METERS_TO_FEET(elev);
				}
				/* No conversion needed if already in metres. */

				/* offset is in current height units. */
				const double current_function_value = elev - this->y_min_visible;

				const int x = this->left_edge + this->width * current_function_arg / max_function_arg;
				const int y = this->bottom_edge - this->height * current_function_value / max_function_value_dem;
				this->viewport->fill_rectangle(dem_color, x - 2, y - 2, 4, 4);
			}
		}
		if (do_speed) {
			/* This is just a speed indicator - no actual values can be inferred by user. */
			if (!std::isnan((*iter)->speed)) {

				const double current_function_value = (*iter)->speed;

				const int x = this->left_edge + this->width * current_function_arg / max_function_arg;
				const int y = this->bottom_edge - this->height * current_function_value / max_function_value_speed;
				this->viewport->fill_rectangle(speed_color, x - 2, y - 2, 4, 4);
			}
		}
	}
}




/**
   A common way to draw the grid with y axis labels

   @pos_y is position of grid line in "beginning in bottom-left
   corner" coordinate system.
 */
void ProfileGraph::draw_grid_horizontal_line(int pos_y, const QString & label)
{
	const float y_interval_px = 1.0 * this->height;

	QPointF text_anchor(0, this->viewport->get_graph_top_edge() + this->height - pos_y);
	QRectF bounding_rect = QRectF(text_anchor.x(), text_anchor.y(), text_anchor.x() + this->left_edge - 10, y_interval_px - 3);
	this->viewport->draw_text(this->viewport->labels_font, this->viewport->labels_pen, bounding_rect, Qt::AlignRight | Qt::AlignTop, label, SG_TEXT_OFFSET_UP);

	/* x/y coordinates are converted here from "beginning in
	   bottom-left corner" to "beginning in upper-left corner"
	   coordinate system. */
	this->viewport->draw_line(this->viewport->grid_pen,
				  0,               this->height - pos_y,
				  0 + this->width, this->height - pos_y);
}




void ProfileGraph::draw_grid_vertical_line(int pos_x, const QString & label)
{
	const QPointF text_anchor(this->left_edge + pos_x, this->viewport->margin_top + this->height);
	QRectF bounding_rect = QRectF(text_anchor.x(), text_anchor.y(), this->width, this->viewport->margin_bottom - 10);
	this->viewport->draw_text(this->viewport->labels_font, this->viewport->labels_pen, bounding_rect, Qt::AlignLeft | Qt::AlignTop, label, SG_TEXT_OFFSET_LEFT);

	this->viewport->draw_line(this->viewport->grid_pen,
				  pos_x, 0,
				  pos_x, 0 + this->height);
}




void ProfileGraph::draw_x_grid_distance(double visible_begin, double visible_end)
{
	const int n_intervals = GRAPH_X_INTERVALS;

	/* Set to display units from length in metres. */
	visible_begin = convert_distance_meters_to(visible_begin, this->geocanvas.distance_unit);
	visible_end = convert_distance_meters_to(visible_end, this->geocanvas.distance_unit);

	const int interval_index = distance_intervals->get_interval_index(visible_begin, visible_end, n_intervals);
	const double distance_interval = distance_intervals->get_interval_value(interval_index);

#if 1
	//double dist_per_pixel = full_distance / this->width;

	const double per_interval_value = distance_interval * this->width / this->track_data.y_max;
	for (int interval_idx = 1; distance_interval * interval_idx <= this->track_data.y_max; interval_idx++) {

		const double distance_value = distance_interval * interval_idx;
		const QString label = get_distance_grid_label_2(this->geocanvas.distance_unit, interval_index, distance_value);

		const int pos_x = (int) (interval_idx * per_interval_value);
		this->draw_grid_vertical_line(pos_x, label);
	}
#else

	/* For last iteration of 'for' loop this denominator will be
	   equal to numerator, giving 'x = this->width * 1' as a result. */
	const int n = visible_end - visible_begin;

	for (int i = visible_begin; i <= visible_end; i++) {
		if (i % ((int) distance_interval) == 0) {
			const QString label = get_distance_grid_label_2(this->geocanvas.distance_unit, interval_index, i);
			const int x = this->width * (i - visible_begin) / n;
			this->draw_grid_vertical_line(x, label);
		}
	}
#endif
}




void ProfileGraph::draw_function_values(void)
{
	const double total = this->y_max_visible - this->y_min_visible;

	if (this->geocanvas.y_domain == GeoCanvasDomain::Elevation) {
		for (int i = 0; i < this->width; i++) {
			if (this->track_data.y[i] == VIK_DEFAULT_ALTITUDE) {
				this->viewport->draw_line(this->no_alt_info_pen,
							   i, 0,
							   i, 0 + this->height);
			} else {
				this->viewport->draw_line(this->main_pen,
							   i, this->height,
							   i, this->height - this->height * (this->track_data.y[i] - this->y_min_visible) / total);
			}
		}
	} else {
		for (int i = 0; i < this->width; i++) {
			this->viewport->draw_line(this->main_pen,
						   i, this->height,
						   i, this->height - this->height * (this->track_data.y[i] - this->y_min_visible) / total);
		}
	}
}



static void draw_additional_indicators_et(ProfileGraph * graph, TrackInfo & track_info)
{
	/* Show DEMS. */
	if (graph->controls.show_dem && graph->controls.show_dem->checkState())  {

		const double max_function_value = graph->y_max_visible;

		const QColor color = graph->dem_alt_pen.color();

		for (int i = 0; i < graph->width; i++) {
			/* This could be slow doing this each time... */
			Trackpoint * tp = track_info.trk->get_closest_tp_by_percentage_time(((double) i / (double) graph->width), NULL);
			if (tp) {
				int16_t elev = DEMCache::get_elev_by_coord(&tp->coord, DemInterpolation::SIMPLE);
				if (elev != DEM_INVALID_ELEVATION) {
					/* Convert into height units. */
					if (Preferences::get_unit_height() == HeightUnit::FEET) {
						elev = VIK_METERS_TO_FEET(elev);
					}
					/* No conversion needed if already in metres. */

					/* offset is in current height units. */
					const double current_function_value = elev - graph->y_min_visible;

					const int x = graph->left_edge + i;
					const int y = graph->bottom_edge - graph->height * current_function_value / max_function_value;
					graph->viewport->fill_rectangle(color, x - 2, y - 2, 4, 4);
				}
			}
		}
	}


	/* Show speeds. */
	if (graph->controls.show_speed && graph->controls.show_speed->checkState()) {
		/* This is just an indicator - no actual values can be inferred by user. */

		const double max_function_value = track_info.max_speed * 110 / 100;

		const QColor color = graph->gps_speed_pen.color();
		for (int i = 0; i < graph->width; i++) {

			const double current_function_value = graph->track_data.y[i];

			const int x = graph->left_edge + i;
			const int y = graph->bottom_edge - graph->height * current_function_value / max_function_value;
			graph->viewport->fill_rectangle(color, x - 2, y - 2, 4, 4);
		}
	}

	return;
}




static void draw_additional_indicators_sd(ProfileGraph * graph, TrackInfo & track_info)
{
	if (graph->controls.show_gps_speed && graph->controls.show_gps_speed->checkState()) {

		const double max_function_arg = track_info.trk->get_length_including_gaps();
		const double max_function_value = graph->y_max_visible;
		double current_function_arg = 0.0;
		double current_function_value = 0.0;

		const QColor color = graph->gps_speed_pen.color();
		for (auto iter = std::next(track_info.trk->trackpoints.begin()); iter != track_info.trk->trackpoints.end(); iter++) {
			double gps_speed = (*iter)->speed;
			if (std::isnan(gps_speed)) {
				continue;
			}

			gps_speed = convert_speed_mps_to(gps_speed, graph->geocanvas.speed_unit);

			current_function_arg += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);
			current_function_value = gps_speed - graph->y_min_visible;

			const int x = graph->left_edge + graph->width * current_function_arg / max_function_arg;
			const int y = graph->bottom_edge - graph->height * current_function_value / max_function_value;
			graph->viewport->fill_rectangle(color, x - 2, y - 2, 4, 4);
		}
	}
}




void draw_additional_indicators_ed(ProfileGraph * graph, TrackInfo & track_info)
{
	const bool do_show_dem = graph->controls.show_dem && graph->controls.show_dem->checkState();
	const bool do_show_gps_speed = graph->controls.show_gps_speed && graph->controls.show_gps_speed->checkState();

	if (do_show_dem || do_show_gps_speed) {

		/* Ensure somekind of max speed when not set. */
		if (track_info.max_speed < 0.01) {
			track_info.max_speed = track_info.trk->get_max_speed();
		}

		graph->draw_dem_alt_speed_dist(track_info.trk, track_info.max_speed, do_show_dem, do_show_gps_speed);
	}
}




static void draw_additional_indicators_gd(ProfileGraph * graph, TrackInfo & track_info)
{
	const bool do_show_gps_speed = graph->controls.show_gps_speed && graph->controls.show_gps_speed->checkState();

	if (do_show_gps_speed) {
		/* Ensure somekind of max speed when not set. */
		if (track_info.max_speed < 0.01) {
			track_info.max_speed = track_info.trk->get_max_speed();
		}
		graph->draw_speed_dist(track_info.trk, track_info.max_speed, do_show_gps_speed);
	}
}




static void draw_additional_indicators_st(ProfileGraph * graph, TrackInfo & track_info)
{
	if (graph->controls.show_gps_speed && graph->controls.show_gps_speed->checkState()) {

		time_t beg_time = (*track_info.trk->trackpoints.begin())->timestamp;
		const time_t max_function_arg = (*std::prev(track_info.trk->trackpoints.end()))->timestamp - beg_time;
		const double max_function_value = graph->y_max_visible;

		const QColor color = graph->gps_speed_pen.color();

		for (auto iter = track_info.trk->trackpoints.begin(); iter != track_info.trk->trackpoints.end(); iter++) {
			double gps_speed = (*iter)->speed;
			if (std::isnan(gps_speed)) {
				continue;
			}

			gps_speed = convert_speed_mps_to(gps_speed, graph->geocanvas.speed_unit);

			const time_t current_function_arg = (*iter)->timestamp - beg_time;
			const double current_function_value = gps_speed - graph->y_min_visible;

			const int x = graph->left_edge + graph->width * current_function_arg / max_function_arg;
			const int y = graph->bottom_edge - graph->height * current_function_value / max_function_value;
			graph->viewport->fill_rectangle(color, x - 2, y - 2, 4, 4);
		}
	}
}




static void draw_additional_indicators_dt(ProfileGraph * graph, TrackInfo & track_info)
{
	/* Show speed indicator. */
	if (graph->controls.show_speed && graph->controls.show_speed->checkState()) {

		const double max_function_value = track_info.max_speed * 110 / 100;

		const QColor color = graph->gps_speed_pen.color();
		/* This is just an indicator - no actual values can be inferred by user. */
		for (int i = 0; i < graph->width; i++) {

			const double current_function_value = graph->track_data.y[i];

			const int x = graph->left_edge + i;
			const int y = graph->bottom_edge - graph->height * current_function_value / max_function_value;
			graph->viewport->fill_rectangle(color, x - 2, y - 2, 4, 4);
		}
	}
}




/**
   \brief Draw the y = f(x) graph
*/
void ProfileGraph::draw_graph(TrackInfo & track_info)
{
	if (this->geocanvas.x_domain == GeoCanvasDomain::Time) {
		track_info.duration = track_info.trk->get_duration(true);
		if (track_info.duration <= 0) {
			return;
		}
	}

	this->regenerate_sizes();

	if (!this->regenerate_data(track_info.trk)) {
		return;
	}

	/* Clear before redrawing. */
	this->viewport->clear();

	this->draw_function_values();

	/* Draw grid on top of graph of values. */
	this->draw_x_grid(track_info);
	this->draw_y_grid();

	if (this->draw_additional_indicators_fn) {
		this->draw_additional_indicators_fn(this, track_info);
	}

	this->viewport->draw_border();
	this->viewport->update();

	/* The pixmap = margin + graph area. */
	qDebug() << "II: Track Profile: saving viewport" << this->viewport->type_string;
	this->saved_img.img = *this->viewport->get_pixmap();
	this->saved_img.valid = true;
}




/**
   Draws representative speed on the supplied pixmap
   (which is the gradients graph).
*/
void ProfileGraph::draw_speed_dist(Track * trk, double max_speed_in, bool do_speed)
{
	double max_function_value = 0;
	double max_function_arg = trk->get_length_including_gaps();

	/* Calculate the max speed factor. */
	if (do_speed) {
		max_function_value = max_speed_in * 110 / 100;
	}

	const QColor color = this->gps_speed_pen.color();
	double current_function_arg = 0.0;
	double current_function_value = 0.0;
	for (auto iter = std::next(trk->trackpoints.begin()); iter != trk->trackpoints.end(); iter++) {

		if (do_speed) {
			/* This is just a speed indicator - no actual values can be inferred by user. */
			if (!std::isnan((*iter)->speed)) {

				current_function_arg += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);
				current_function_value = (*iter)->speed;

				const int x = this->left_edge + this->width * current_function_arg / max_function_arg;
				const int y = this->bottom_edge - this->height * current_function_value / max_function_value;
				this->viewport->fill_rectangle(color, x - 2, y - 2, 4, 4);
			}
		}
	}
}




void ProfileGraph::draw_x_grid_time(time_t visible_begin, time_t visible_end)
{
	const int n_intervals = GRAPH_Y_INTERVALS;

	const int interval_index = time_intervals->get_interval_index(visible_begin, visible_end, n_intervals);
	const time_t time_interval = time_intervals->get_interval_value(interval_index);

#ifdef K
	/* If stupidly long track in time - don't bother trying to draw grid lines. */
	if ((visible_end - visible_begin + 1) > time_intervals->values[G_N_ELEMENTS(time_intervals->values)-1] * n_intervals * n_intervals) {
		return;
	}
#endif

	/* For last iteration of 'for' loop this denominator will be
	   equal to numerator, giving 'x = this->width * 1' as a result. */
	const time_t n = visible_end - visible_begin;

	/* TODO: optimise this: do we really need to go through N
	   timestamps? Can't we increase loop incrementation after
	   drawing first vertical line? */
	for (time_t i = visible_begin; i <= visible_end; i++) {
		if (i % time_interval == 0) {
			const int x = this->width * (i - visible_begin) / n;
			this->draw_grid_vertical_line(x, get_time_grid_label(interval_index, i));
		}
	}
}




/**
   Draw all graphs
*/
void TrackProfileDialog::draw_all_graphs(bool resized)
{
	for (int i = SG_TRACK_PROFILE_TYPE_ED; i < SG_TRACK_PROFILE_TYPE_MAX; i++) {
		if (this->graphs[i]->viewport) {
			/* If dialog window is resized then saved image is no longer valid. */
			this->graphs[i]->saved_img.valid = !resized;
			this->draw_single_graph(this->graphs[i]);
		}
	}
}




QPointF ProfileGraph::get_position_of_tp(TrackInfo & track_info, Trackpoint * tp)
{
	QPointF pos(-1.0, -1.0);

	double pc = NAN;

	switch (this->geocanvas.x_domain) {
	case GeoCanvasDomain::Time:
		pc = tp_percentage_by_time(track_info, tp);
		break;
	case GeoCanvasDomain::Distance:
		pc = tp_percentage_by_distance(track_info, tp);
		break;
	default:
		qDebug() << "EE:" PREFIX << "unhandled x domain" << (int) this->geocanvas.x_domain;
		/* pc = NAN */
		break;
	}

	if (!std::isnan(pc)) {
		const double x = pc * this->width;
		pos.setX(x);
		/* Call through pointer to member function... */
		pos.setY(this->get_pos_y(x));
	}

	return pos;
}




void TrackProfileDialog::draw_single_graph(ProfileGraph * graph)
{
	graph->width = graph->viewport->get_graph_width();
	graph->height = graph->viewport->get_graph_height();

	graph->draw_graph(this->track_info);

	/* Ensure markers are redrawn if necessary. */
	if (this->is_selected_drawn || this->is_current_drawn) {

		QPointF current_pos(-1.0, -1.0);
		if (this->is_current_drawn) {
			current_pos = graph->get_position_of_tp(this->track_info, this->current_tp);
		}

		QPointF selected_pos = graph->get_position_of_tp(this->track_info, this->selected_tp);

		graph->draw_marks(ScreenPos(selected_pos.x(), selected_pos.y()), ScreenPos(current_pos.x(), current_pos.y()), this->is_selected_drawn, this->is_current_drawn);
	}
}




/**
 * Configure/Resize the profile & speed/time images.
 */
bool TrackProfileDialog::paint_to_viewport_cb(Viewport * viewport)
{
	qDebug() << "SLOT:" PREFIX << "reacting to signal from viewport" << viewport->type_string;

#ifdef K
	if (controls->configure_dialog) {
		/* Determine size offsets between dialog size and size for images.
		   Only on the initialisation of the dialog. */
		controls->profile_width_offset = event->width - controls->profile_width;
		controls->profile_height_offset = event->height - controls->profile_height;
		controls->configure_dialog = false;

		/* Without this the settting, the dialog will only grow in vertical size - one can not then make it smaller! */
		gtk_widget_set_size_request(widget, controls->profile_width+controls->profile_width_offset, controls->profile_height+controls->profile_height_offset);

		/* Allow resizing back down to a minimal size (especially useful if the initial size has been made bigger after restoring from the saved settings). */
		GdkGeometry geom = { 600+controls->profile_width_offset, 300+controls->profile_height_offset, 0, 0, 0, 0, 0, 0, 0, 0, GDK_GRAVITY_STATIC };
		gdk_window_set_geometry_hints(gtk_widget_get_window(widget), &geom, GDK_HINT_MIN_SIZE);
	} else {
		controls->profile_width_old = controls->profile_width;
		controls->profile_height_old = controls->profile_height;
	}

	/* Now adjust From Dialog size to get image size. */
	controls->profile_width = event->width - controls->profile_width_offset;
	controls->profile_height = event->height - controls->profile_height_offset;

	/* ATM we receive configure_events when the dialog is moved and so no further action is necessary. */
	if (!controls->configure_dialog &&
	    (controls->profile_width_old == controls->profile_width) && (controls->profile_height_old == controls->profile_height)) {

		return false;
	}
#endif
	/* TODO: shouldn't we re-allocate the per-viewport table of doubles here? */

	/* Draw stuff. */
	draw_all_graphs(true);

	return false;
}




void ProfileGraph::create_viewport(int index, TrackProfileDialog * dialog)
{
	const int initial_width = GRAPH_MARGIN_LEFT + GRAPH_INITIAL_WIDTH + GRAPH_MARGIN_RIGHT;
	const int initial_height = GRAPH_MARGIN_TOP + GRAPH_INITIAL_HEIGHT + GRAPH_MARGIN_BOTTOM;

	this->viewport = new Viewport(dialog->parent);
	strcpy(this->viewport->type_string, this->get_graph_title().toUtf8().constData());
	this->viewport->set_margin(GRAPH_MARGIN_TOP, GRAPH_MARGIN_BOTTOM, GRAPH_MARGIN_LEFT, GRAPH_MARGIN_RIGHT);
	this->viewport->resize(initial_width, initial_height);
	this->viewport->reconfigure_drawing_area(initial_width, initial_height);

	switch (index) {
	case SG_TRACK_PROFILE_TYPE_ED:
		QObject::connect(this->viewport, SIGNAL (button_released(Viewport *, QMouseEvent *)), dialog, SLOT (track_ed_release_cb(Viewport *, QMouseEvent *)));
		QObject::connect(this->viewport, SIGNAL (cursor_moved(Viewport *, QMouseEvent *)),    dialog, SLOT (handle_cursor_move_ed_cb(Viewport *, QMouseEvent *)));
		break;

	case SG_TRACK_PROFILE_TYPE_GD:
		QObject::connect(this->viewport, SIGNAL (button_released(Viewport *, QMouseEvent *)), dialog, SLOT (track_gd_release_cb(Viewport *, QMouseEvent *)));
		QObject::connect(this->viewport, SIGNAL (cursor_moved(Viewport *, QMouseEvent *)),    dialog, SLOT (handle_cursor_move_gd_cb(Viewport *, QMouseEvent *)));
		break;

	case SG_TRACK_PROFILE_TYPE_ST:
		QObject::connect(this->viewport, SIGNAL (button_released(Viewport *, QMouseEvent *)), dialog, SLOT (track_st_release_cb(Viewport *, QMouseEvent *)));
		QObject::connect(this->viewport, SIGNAL (cursor_moved(Viewport *, QMouseEvent *)),    dialog, SLOT (handle_cursor_move_st_cb(Viewport *, QMouseEvent *)));
		break;

	case SG_TRACK_PROFILE_TYPE_DT:
		QObject::connect(this->viewport, SIGNAL (button_released(Viewport *, QMouseEvent *)), dialog, SLOT (track_dt_release_cb(Viewport *, QMouseEvent *)));
		QObject::connect(this->viewport, SIGNAL (cursor_moved(Viewport *, QMouseEvent *)),    dialog, SLOT (handle_cursor_move_dt_cb(Viewport *, QMouseEvent *)));
		break;

	case SG_TRACK_PROFILE_TYPE_ET:
		QObject::connect(this->viewport, SIGNAL (button_released(Viewport *, QMouseEvent *)), dialog, SLOT (track_et_release_cb(Viewport *, QMouseEvent *)));
		QObject::connect(this->viewport, SIGNAL (cursor_moved(Viewport *, QMouseEvent *)),    dialog, SLOT (handle_cursor_move_et_cb(Viewport *, QMouseEvent *)));
		break;

	case SG_TRACK_PROFILE_TYPE_SD:
		QObject::connect(this->viewport, SIGNAL (button_released(Viewport *, QMouseEvent *)), dialog, SLOT (track_sd_release_cb(Viewport *, QMouseEvent *)));
		QObject::connect(this->viewport, SIGNAL (cursor_moved(Viewport *, QMouseEvent *)),    dialog, SLOT (handle_cursor_move_sd_cb(Viewport *, QMouseEvent *)));
		break;

	default:
		qDebug() << "EE:" PREFIX << "unhandled index" << index;
		break;
	}

	return;
}




void TrackProfileDialog::save_values(void)
{
	/* Session settings. */
	ApplicationState::set_integer(VIK_SETTINGS_TRACK_PROFILE_WIDTH, this->profile_width);
	ApplicationState::set_integer(VIK_SETTINGS_TRACK_PROFILE_HEIGHT, this->profile_height);

	/* Just for this session. */
	/* TODO: we need smarter approach to saving state of graphs. */
	if (this->graphs[SG_TRACK_PROFILE_TYPE_ED]->controls.show_dem) {
		g_show_dem = this->graphs[SG_TRACK_PROFILE_TYPE_ED]->controls.show_dem->checkState();
	}
	if (this->graphs[SG_TRACK_PROFILE_TYPE_ED]->controls.show_gps_speed) {
		g_show_alt_gps_speed = this->graphs[SG_TRACK_PROFILE_TYPE_ED]->controls.show_gps_speed->checkState();
	}
	if (this->graphs[SG_TRACK_PROFILE_TYPE_ST]->controls.show_gps_speed) {
		g_show_gps_speed = this->graphs[SG_TRACK_PROFILE_TYPE_ST]->controls.show_gps_speed->checkState();
	}
	if (this->graphs[SG_TRACK_PROFILE_TYPE_GD]->controls.show_gps_speed) {
		g_show_gradient_gps_speed = this->graphs[SG_TRACK_PROFILE_TYPE_GD]->controls.show_gps_speed->checkState();
	}
	if (this->graphs[SG_TRACK_PROFILE_TYPE_DT]->controls.show_speed) {
		g_show_dist_speed = this->graphs[SG_TRACK_PROFILE_TYPE_DT]->controls.show_speed->checkState();
	}
	if (this->graphs[SG_TRACK_PROFILE_TYPE_ET]->controls.show_dem) {
		g_show_elev_dem = this->graphs[SG_TRACK_PROFILE_TYPE_ET]->controls.show_dem->checkState();
	}
	if (this->graphs[SG_TRACK_PROFILE_TYPE_ET]->controls.show_speed) {
		g_show_elev_speed = this->graphs[SG_TRACK_PROFILE_TYPE_ET]->controls.show_speed->checkState();
	}
	if (this->graphs[SG_TRACK_PROFILE_TYPE_SD]->controls.show_gps_speed) {
		g_show_sd_gps_speed = this->graphs[SG_TRACK_PROFILE_TYPE_SD]->controls.show_gps_speed->checkState();
	}
}




void TrackProfileDialog::destroy_cb(void) /* Slot. */
{
	this->save_values();
	//delete controls;
}




void TrackProfileDialog::dialog_response_cb(int resp) /* Slot. */
{
	bool keep_dialog = false;

	/* FIXME: check and make sure the track still exists before doing anything to it. */
	/* Note: destroying diaglog (eg, parent window exit) won't give "response". */
	switch (resp) {
	case SG_TRACK_PROFILE_CANCEL:
		this->reject();
		break;
	case SG_TRACK_PROFILE_OK:
		this->trw->get_tracks_node().update_tree_view(this->track_info.trk);
		this->trw->emit_layer_changed();
		this->accept();
		break;
	case SG_TRACK_PROFILE_REVERSE:
		this->track_info.trk->reverse();
		this->trw->emit_layer_changed();
		keep_dialog = true;
		break;
	case SG_TRACK_PROFILE_SPLIT_SEGMENTS: {
		/* Get new tracks, add them and then the delete old one. old can still exist on clipboard. */
		std::list<Track *> * tracks = this->track_info.trk->split_into_segments();
		for (auto iter = tracks->begin(); iter != tracks->end(); iter++) {
			if (*iter) {
				const QString new_tr_name = this->trw->new_unique_element_name(this->track_info.trk->type_id, this->track_info.trk->name);
				(*iter)->set_name(new_tr_name);

				if (this->track_info.trk->type_id == "sg.trw.route") {
					this->trw->add_route(*iter);
				} else {
					this->trw->add_track(*iter);
				}
				(*iter)->calculate_bounds();
			}
		}
		if (tracks) {
			delete tracks;
			/* Don't let track destroy this dialog. */
			if (this->track_info.trk->type_id == "sg.trw.route") {
				this->trw->delete_route(this->track_info.trk);
			} else {
				this->trw->delete_track(this->track_info.trk);
			}
			this->trw->emit_layer_changed(); /* Chase thru the hoops. */
		}
	}
		break;
	case SG_TRACK_PROFILE_SPLIT_AT_MARKER: {
		auto iter = std::next(this->track_info.trk->begin());
		while (iter != this->track_info.trk->end()) {
			if (this->selected_tp == *iter) {
				break;
			}
			iter++;
		}
		if (iter == this->track_info.trk->end()) {
			Dialog::error(tr("Failed to split track. Track unchanged"), this->trw->get_window());
			keep_dialog = true;
			break;
		}

		const QString r_name = this->trw->new_unique_element_name(this->track_info.trk->type_id, this->track_info.trk->name);


		/* Notice that here Trackpoint pointed to by iter is moved to new track. */
		/* kamilTODO: originally the constructor was just Track(). Should we really pass original trk to constructor? */
		/* TODO: move more copying of the stuff into constructor. */
		Track * trk_right = new Track(*this->track_info.trk, iter, this->track_info.trk->end());
		this->track_info.trk->erase(iter, this->track_info.trk->end());

		if (!this->track_info.trk->comment.isEmpty()) {
			trk_right->set_comment(this->track_info.trk->comment);
		}
		trk_right->visible = this->track_info.trk->visible;
		trk_right->type_id = this->track_info.trk->type_id;
		trk_right->set_name(r_name);

		if (this->track_info.trk->type_id == "sg.trw.route") {
			this->trw->add_route(trk_right);
		} else {
			this->trw->add_track(trk_right);
		}
		this->track_info.trk->calculate_bounds();
		trk_right->calculate_bounds();

		this->trw->emit_layer_changed();
	}
		break;
	default:
		qDebug() << "EE: Track Profile: dialog response slot: unknown response" << resp;
		return;
	}

	/* Keep same behaviour for now: destroy dialog if click on any button. */
	if (!keep_dialog) {
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
 *  Create the widgets for the given graph tab.
 */
QWidget * TrackProfileDialog::create_graph_page(ProfileGraph * graph)
{
	/* kamilTODO: who deletes these two pointers? */
	graph->labels_grid = new QGridLayout();
	QVBoxLayout * controls_vbox = new QVBoxLayout;
	graph->controls_vbox = new QVBoxLayout;

	graph->viewport->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	graph->controls_vbox->addWidget(graph->viewport);


	graph->labels_grid->addWidget(new QLabel(graph->labels.x_label), 0, 0);
	graph->labels_grid->addWidget(graph->labels.x_value, 0, 1);

	graph->labels_grid->addWidget(new QLabel(graph->labels.y_label), 1, 0);
	graph->labels_grid->addWidget(graph->labels.y_value, 1, 1);

	if (graph->labels.t_value) {
		graph->labels_grid->addWidget(new QLabel(graph->labels.t_label), 2, 0);
		graph->labels_grid->addWidget(graph->labels.t_value, 2, 1);
	}
	graph->controls_vbox->addLayout(graph->labels_grid);


	if (graph->controls.show_dem) {
		controls_vbox->addWidget(graph->controls.show_dem);
	}
	if (graph->controls.show_gps_speed) {
		controls_vbox->addWidget(graph->controls.show_gps_speed);
	}
	if (graph->controls.show_speed) {
		controls_vbox->addWidget(graph->controls.show_speed);
	}
	graph->controls_vbox->addLayout(controls_vbox);


	QWidget * widget = new QWidget(this);
	widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	widget->setMinimumSize(500, 300);
	QLayout * old = widget->layout();
	delete old;
	widget->setLayout(graph->controls_vbox);

	return widget;

}




void SlavGPS::track_profile_dialog(Window * parent, Track * trk, Viewport * main_viewport)
{
	TrackProfileDialog dialog(QObject::tr("Track Profile"), trk, main_viewport, parent);
	trk->set_profile_dialog(&dialog);
	dialog.exec();
	trk->clear_profile_dialog();
}




QString ProfileGraph::get_graph_title(void) const
{
	QString result;

	if (this->geocanvas.y_domain == GeoCanvasDomain::Elevation && this->geocanvas.x_domain == GeoCanvasDomain::Distance) {
		result = QObject::tr("Elevation over distance");

	} else if (this->geocanvas.y_domain == GeoCanvasDomain::Gradient && this->geocanvas.x_domain == GeoCanvasDomain::Distance) {
		result = QObject::tr("Gradient over distance");

	} else if (this->geocanvas.y_domain == GeoCanvasDomain::Speed && this->geocanvas.x_domain == GeoCanvasDomain::Time) {
		result = QObject::tr("Speed over time");

	} else if (this->geocanvas.y_domain == GeoCanvasDomain::Distance && this->geocanvas.x_domain == GeoCanvasDomain::Time) {
		result = QObject::tr("Distance over time");

	} else if (this->geocanvas.y_domain == GeoCanvasDomain::Elevation && this->geocanvas.x_domain == GeoCanvasDomain::Time) {
		result = QObject::tr("Elevation over time");

	} else if (this->geocanvas.y_domain == GeoCanvasDomain::Speed && this->geocanvas.x_domain == GeoCanvasDomain::Distance) {
		result = QObject::tr("Speed over distance");

	} else {
		qDebug() << "EE:" PREFIX << "unhandled x/y domain" << (int) this->geocanvas.x_domain << (int) this->geocanvas.y_domain;
	}

	return result;
}




TrackProfileDialog::TrackProfileDialog(QString const & title, Track * a_trk, Viewport * main_viewport_, Window * a_parent) : QDialog(a_parent)
{
	time_intervals = new Intervals<time_t>(time_interval_values, sizeof (time_interval_values) / sizeof (time_interval_values[0]));
	distance_intervals = new Intervals<double>(distance_interval_values, sizeof (distance_interval_values) / sizeof (distance_interval_values[0]));
	altitude_intervals = new Intervals<double>(altitude_interval_values, sizeof (altitude_interval_values) / sizeof (altitude_interval_values[0]));
	gradient_intervals = new Intervals<double>(gradient_interval_values, sizeof (gradient_interval_values) / sizeof (gradient_interval_values[0]));
	speed_intervals = new Intervals<double>(speed_interval_values, sizeof (speed_interval_values) / sizeof (speed_interval_values[0]));


	this->setWindowTitle(tr("%1 - Track Profile").arg(a_trk->name));

	this->trw = (LayerTRW *) a_trk->owning_layer;
	this->track_info.trk = a_trk;
	this->main_viewport = main_viewport_;
	this->parent = a_parent;


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


	this->graphs[SG_TRACK_PROFILE_TYPE_ED] = new ProfileGraph(GeoCanvasDomain::Distance, GeoCanvasDomain::Elevation, SG_TRACK_PROFILE_TYPE_ED, this);
	this->graphs[SG_TRACK_PROFILE_TYPE_GD] = new ProfileGraph(GeoCanvasDomain::Distance, GeoCanvasDomain::Gradient,  SG_TRACK_PROFILE_TYPE_GD, this);
	this->graphs[SG_TRACK_PROFILE_TYPE_ST] = new ProfileGraph(GeoCanvasDomain::Time,     GeoCanvasDomain::Speed,     SG_TRACK_PROFILE_TYPE_ST, this);
	this->graphs[SG_TRACK_PROFILE_TYPE_DT] = new ProfileGraph(GeoCanvasDomain::Time,     GeoCanvasDomain::Distance,  SG_TRACK_PROFILE_TYPE_DT, this);
	this->graphs[SG_TRACK_PROFILE_TYPE_ET] = new ProfileGraph(GeoCanvasDomain::Time,     GeoCanvasDomain::Elevation, SG_TRACK_PROFILE_TYPE_ET, this);
	this->graphs[SG_TRACK_PROFILE_TYPE_SD] = new ProfileGraph(GeoCanvasDomain::Distance, GeoCanvasDomain::Speed,     SG_TRACK_PROFILE_TYPE_SD, this);


	this->tabs = new QTabWidget();
	this->tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);


	/* NB This value not shown yet - but is used by internal calculations. */
	this->track_info.track_length_including_gaps = this->track_info.trk->get_length_including_gaps();


	for (int index = SG_TRACK_PROFILE_TYPE_ED; index < SG_TRACK_PROFILE_TYPE_MAX; index++) {
		if (!this->graphs[index]->viewport) {
			continue;
		}

		this->configure_widgets(index);

		QWidget * page = this->create_graph_page(this->graphs[index]);
		this->tabs->addTab(page, this->graphs[index]->get_graph_title());

		connect(this->graphs[index]->viewport, SIGNAL (drawing_area_reconfigured(Viewport *)), this, SLOT (paint_to_viewport_cb(Viewport *)));
	}


	this->button_box = new QDialogButtonBox();


	this->button_cancel = this->button_box->addButton(tr("&Cancel"), QDialogButtonBox::RejectRole);
	this->button_split_at_marker = this->button_box->addButton(tr("Split at &Marker"), QDialogButtonBox::ActionRole);
	this->button_split_segments = this->button_box->addButton(tr("Split &Segments"), QDialogButtonBox::ActionRole);
	this->button_reverse = this->button_box->addButton(tr("&Reverse"), QDialogButtonBox::ActionRole);
	this->button_ok = this->button_box->addButton(tr("&OK"), QDialogButtonBox::AcceptRole);

	this->button_split_segments->setEnabled(this->track_info.trk->get_segment_count() > 1);
	this->button_split_at_marker->setEnabled(this->selected_tp); /* Initially no trackpoint is selected. */

	this->signal_mapper = new QSignalMapper(this);
	connect(this->button_cancel,          SIGNAL (released()), signal_mapper, SLOT (map()));
	connect(this->button_split_at_marker, SIGNAL (released()), signal_mapper, SLOT (map()));
	connect(this->button_split_segments,  SIGNAL (released()), signal_mapper, SLOT (map()));
	connect(this->button_reverse,         SIGNAL (released()), signal_mapper, SLOT (map()));
	connect(this->button_ok,              SIGNAL (released()), signal_mapper, SLOT (map()));

	this->signal_mapper->setMapping(this->button_cancel,          SG_TRACK_PROFILE_CANCEL);
	this->signal_mapper->setMapping(this->button_split_at_marker, SG_TRACK_PROFILE_SPLIT_AT_MARKER);
	this->signal_mapper->setMapping(this->button_split_segments,  SG_TRACK_PROFILE_SPLIT_SEGMENTS);
	this->signal_mapper->setMapping(this->button_reverse,         SG_TRACK_PROFILE_REVERSE);
	this->signal_mapper->setMapping(this->button_ok,              SG_TRACK_PROFILE_OK);

	connect(this->signal_mapper, SIGNAL (mapped(int)), this, SLOT (dialog_response_cb(int)));


	QLayout * old = this->layout();
	delete old;
	QVBoxLayout * vbox = new QVBoxLayout;
	this->setLayout(vbox);
	vbox->addWidget(this->tabs);
	vbox->addWidget(this->button_box);
}




void TrackProfileDialog::configure_widgets(int index)
{
	ProfileGraph * graph = this->graphs[index];

	switch (graph->geocanvas.x_domain) {
	case GeoCanvasDomain::Distance:
		graph->labels.x_label = tr("Track Distance:");
		graph->labels.x_value = ui_label_new_selectable(tr("No Data"), this);

#ifdef K
		if (this->graphs[index].y_domain == GeoCanvasDomain::Speed) {
			graph->controls.show_gps_speed = new QCheckBox(tr("Show &GPS Speed"), this);
			graph->controls.show_gps_speed->setCheckState(show_sd_gps_speed ? Qt::Checked : Qt::Unchecked);
			connect(graph->controls.show_gps_speed, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		}
#endif

		break;

	case GeoCanvasDomain::Time:
		graph->labels.x_label = tr("Track Time:");
		graph->labels.x_value = ui_label_new_selectable(tr("No Data"), this);

		/* Additional timestamp to provide more information in UI. */
		graph->labels.t_label = tr("Time/Date:");
		graph->labels.t_value = ui_label_new_selectable(tr("No Data"), this);

		graph->controls.show_gps_speed = new QCheckBox(tr("Show &GPS Speed"), this);
		graph->controls.show_gps_speed->setCheckState(g_show_gps_speed ? Qt::Checked : Qt::Unchecked);
		connect(graph->controls.show_speed, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));

		graph->controls.show_speed = new QCheckBox(tr("Show S&peed"), this);
		graph->controls.show_speed->setCheckState(g_show_elev_dem ? Qt::Checked : Qt::Unchecked);
		connect(graph->controls.show_gps_speed, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));

		break;
	default:
		qDebug() << "EE:" PREFIX << "unhandled x domain" << (int) this->graphs[index]->geocanvas.x_domain << "for index" << index;
		break;
	}


	switch (this->graphs[index]->geocanvas.y_domain) {
	case GeoCanvasDomain::Elevation:
		graph->labels.y_label = tr("Track Height:");
		graph->labels.y_value = ui_label_new_selectable(tr("No Data"), this);

		if (!graph->controls.show_dem) {
			graph->controls.show_dem = new QCheckBox(tr("Show D&EM"), this);
			graph->controls.show_dem->setCheckState(g_show_elev_speed ? Qt::Checked : Qt::Unchecked);
			connect(graph->controls.show_dem, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		}

		break;

	case GeoCanvasDomain::Gradient:
		graph->labels.y_label = tr("Track Gradient:");
		graph->labels.y_value = ui_label_new_selectable(tr("No Data"), this);

		break;

	case GeoCanvasDomain::Speed:
		graph->labels.y_label = tr("Track Speed:");
		graph->labels.y_value = ui_label_new_selectable(tr("No Data"), this);

		break;

	case GeoCanvasDomain::Distance:
		graph->labels.y_label = tr("Track Distance:");
		graph->labels.y_value = ui_label_new_selectable(tr("No Data"), this);

		if (!graph->controls.show_speed) {
			graph->controls.show_speed = new QCheckBox(tr("Show S&peed"), this);
			graph->controls.show_speed->setCheckState(g_show_dist_speed ? Qt::Checked : Qt::Unchecked);
			connect(graph->controls.show_speed, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		}

		break;
	default:
		qDebug() << "EE:" PREFIX << "unhandled y domain" << (int) this->graphs[index]->geocanvas.y_domain << "for index" << index;
		break;
	}

	return;
}



QString get_speed_grid_label(SpeedUnit speed_unit, double value)
{
	QString result;

	switch (speed_unit) {
	case SpeedUnit::KILOMETRES_PER_HOUR:
		result = QObject::tr("%1 km/h").arg(value, 0, 'f', SG_PRECISION_SPEED);
		break;
	case SpeedUnit::MILES_PER_HOUR:
		result = QObject::tr("%1 mph").arg(value, 0, 'f', SG_PRECISION_SPEED);
		break;
	case SpeedUnit::METRES_PER_SECOND:
		result = QObject::tr("%1 m/s").arg(value, 0, 'f', SG_PRECISION_SPEED);
		break;
	case SpeedUnit::KNOTS:
		result = QObject::tr("%1 knots").arg(value, 0, 'f', SG_PRECISION_SPEED);
		break;
	default:
		result = QObject::tr("--");
		qDebug() << "EE:" PREFIX << "unrecognized speed unit" << (int) speed_unit;
		break;
	}

	return result;
}




QString get_elevation_grid_label(HeightUnit height_unit, double value)
{
	QString result;

	switch (height_unit) {
	case HeightUnit::METRES:
		result = QObject::tr("%1 m").arg(value, 0, 'f', SG_PRECISION_ALTITUDE);
		break;
	case HeightUnit::FEET:
		result = QObject::tr("%1 ft").arg(value, 0, 'f', SG_PRECISION_ALTITUDE);
		break;
	default:
		result = QObject::tr("--");
		qDebug() << "EE:" PREFIX << "unrecognized height unit" << (int) height_unit;
		break;
	}

	return result;
}




QString get_distance_grid_label(DistanceUnit distance_unit, double value)
{
	QString result;

	/* TODO: why those grid labels display value after comma,
	   while grid labels for speed and elevation don't display
	   values after comma? */

	switch (distance_unit) {
	case DistanceUnit::KILOMETRES:
		result = QObject::tr("%1 km").arg(value, 0, 'f', SG_PRECISION_DISTANCE);
		break;
	case DistanceUnit::MILES:
		result = QObject::tr("%1 miles").arg(value, 0, 'f', SG_PRECISION_DISTANCE);
		break;
	case DistanceUnit::NAUTICAL_MILES:
		result = QObject::tr("%1 NM").arg(value, 0, 'f', SG_PRECISION_DISTANCE);
		break;
	default:
		result = QObject::tr("--");
		qDebug() << "EE:" PREFIX << "unrecognized distance unit" << (int) distance_unit;
		break;
	}

	return result;
}




QString get_distance_grid_label_2(DistanceUnit distance_unit, int interval_index, double value)
{
	/* TODO: improve localization of the strings: don't use get_distance_unit_string() */
	const QString distance_unit_string = get_distance_unit_string(distance_unit);

	QString label;
	if (interval_index > 4) {
		label = QObject::tr("%1 %2").arg((unsigned int) value).arg(distance_unit_string);
	} else {
		label = QObject::tr("%1 %2").arg(value, 0, 'f', SG_PRECISION_DISTANCE).arg(distance_unit_string);
	}

	return label;
}




QString get_time_grid_label(int interval_index, int value)
{
	QString result;

	switch (interval_index) {
	case 0:
	case 1:
	case 2:
	case 3:
		/* Minutes. */
		result = QObject::tr("%1 m").arg((int) (value / 60));
		break;
	case 4:
	case 5:
	case 6:
	case 7:
		/* Hours. */
		result = QObject::tr("%1 h").arg(((double) (value / (60 * 60))), 0, 'f', 1);
		break;
	case 8:
	case 9:
	case 10:
		/* Days. */
		result = QObject::tr("%1 d").arg(((double) value / (60 *60 * 24)), 0, 'f', 1);
		break;
	case 11:
	case 12:
		/* Weeks. */
		result = QObject::tr("%1 w").arg(((double) value / (60 * 60 * 24 * 7)), 0, 'f', 1);
		break;
	case 13:
		/* Months. */
		result = QObject::tr("%1 M").arg(((double) value / (60 * 60 * 24 * 28)), 0, 'f', 1);
		break;
	default:
		qDebug() << "EE:" PREFIX << "unhandled time interval index" << interval_index;
		break;
	}

	return result;
}




QString get_time_grid_label_2(time_t interval_value, time_t value)
{
	QString result;

	switch (interval_value) {
	case 60:
	case 120:
	case 300:
	case 900:
		/* Minutes. */
		result = QObject::tr("%1 m").arg((int) (value / 60));
		break;
	case 1800:
	case 3600:
	case 10800:
	case 21600:
		/* Hours. */
		result = QObject::tr("%1 h").arg(((double) (value / (60 * 60))), 0, 'f', 1);
		break;
	case 43200:
	case 86400:
	case 172800:
		/* Days. */
		result = QObject::tr("%1 d").arg(((double) value / (60 *60 * 24)), 0, 'f', 1);
		break;
	case 604800:
	case 1209600:
		/* Weeks. */
		result = QObject::tr("%1 w").arg(((double) value / (60 * 60 * 24 * 7)), 0, 'f', 1);
		break;
	case 2419200:
		/* Months. */
		result = QObject::tr("%1 M").arg(((double) value / (60 * 60 * 24 * 28)), 0, 'f', 1);
		break;
	default:
		qDebug() << "EE:" PREFIX << "unhandled time interval value" << interval_value;
		break;
	}

	return result;
}




ProfileGraph::ProfileGraph(GeoCanvasDomain x_domain, GeoCanvasDomain y_domain, int index, TrackProfileDialog * dialog)
{
	this->geocanvas.x_domain = x_domain;
	this->geocanvas.y_domain = y_domain;


	if (x_domain == GeoCanvasDomain::Distance && y_domain == GeoCanvasDomain::Elevation) {
		this->draw_additional_indicators_fn = draw_additional_indicators_ed;

	} else if (x_domain == GeoCanvasDomain::Distance&& y_domain == GeoCanvasDomain::Gradient) {
		this->draw_additional_indicators_fn = draw_additional_indicators_gd;

	} else if (x_domain == GeoCanvasDomain::Time && y_domain == GeoCanvasDomain::Speed) {
		this->draw_additional_indicators_fn = draw_additional_indicators_st;

	} else if (x_domain == GeoCanvasDomain::Time && y_domain == GeoCanvasDomain::Distance) {
		this->draw_additional_indicators_fn = draw_additional_indicators_dt;

	} else if (x_domain == GeoCanvasDomain::Time && y_domain == GeoCanvasDomain::Elevation) {
		this->draw_additional_indicators_fn = draw_additional_indicators_et;

	} else if (x_domain == GeoCanvasDomain::Distance && y_domain == GeoCanvasDomain::Speed) {
		this->draw_additional_indicators_fn = draw_additional_indicators_sd;
	} else {
		qDebug() << "EE" PREFIX << "unhandled combination of x/y domains:" << (int) x_domain << (int) y_domain;
		this->draw_additional_indicators_fn = NULL;
	}


	this->main_pen.setColor("lightsteelblue");
	this->main_pen.setWidth(1);

	this->gps_speed_pen.setColor(QColor("red"));
	this->dem_alt_pen.setColor(QColor("green"));
	this->no_alt_info_pen.setColor(QColor("yellow"));

	this->create_viewport(index, dialog);
}




ProfileGraph::~ProfileGraph()
{
	delete this->viewport;
}




bool ProfileGraph::regenerate_data(Track * trk)
{
	this->track_data.invalidate();

	/* Ask a track to generate a vector of values representing some parameter
	   of a track as a function of either time or distance. */
	return this->regenerate_data_from_scratch(trk);
}




void ProfileGraph::regenerate_sizes(void)
{
	this->width = this->viewport->get_graph_width();
	this->height = this->viewport->get_graph_height();
	this->bottom_edge = this->viewport->get_graph_bottom_edge();
	this->left_edge = this->viewport->get_graph_left_edge();
}




/**
   Find first grid line. It will be a multiple of y-interval
   just below y_min_visible.

   Find last grid line. It will be a multiple of y-interval
   just above y_max_visible.

   All grid lines will be drawn starting from the first to
   last (provided that they will fall within graph's main
   area).

   When looking for first and last line, start from zero value
   and go up or down: a grid line will be always drawn at zero
   and/or at multiples of y-interval (depending whether they
   will fall within graph's main area).
*/
template <typename T>
void find_grid_line_indices(T min_visible, T max_visible, T interval, int * first_line, int * last_line)
{
	/* 'first_line * y_interval' will be below y_min_visible. */
	if (min_visible <= 0) {
		while ((*first_line) * interval > min_visible) {
			(*first_line)--;
		}
	} else {
		while ((*first_line) * interval + interval < min_visible) {
			(*first_line)++;
		}
	}

	/* 'last_line * y_interval' will be above y_max_visible. */
	if (max_visible <= 0) {
		while ((*last_line) * interval - interval > max_visible) {
			(*last_line)--;
		}
	} else {
		while ((*last_line) * interval < max_visible) {
			(*last_line)++;
		}
	}
}




void ProfileGraph::draw_y_grid_sub(void)
{
	if (this->y_max_visible - this->y_min_visible == 0) {
		qDebug() << "EE:" PREFIX << "zero visible range:" << this->y_min_visible << this->y_max_visible;
		return;
	}

	int first_line = 0;
	int last_line = 0;
	find_grid_line_indices(this->y_min_visible, this->y_max_visible, this->y_interval, &first_line, &last_line);

#if 0
	qDebug() << "===== drawing y grid for graph" << this->get_graph_title() << ", height =" << this->height;
	qDebug() << "      min/max y visible:" << this->y_min_visible << this->y_max_visible;
	qDebug() << "      interval =" << this->y_interval << ", first_line/last_line =" << first_line << last_line;
#endif

	for (int i = first_line; i <= last_line; i++) {
		const double value = i * this->y_interval;

		/* 'row' is in "beginning in bottom-left corner" coordinate system. */
		const int row = this->height * ((value - this->y_min_visible) / (this->y_max_visible - this->y_min_visible));

		if (row >= 0 && row < this->height) {
			//qDebug() << "      value (inside) =" << value << ", row =" << row;
			this->draw_grid_horizontal_line(row, this->get_y_grid_label(value));
		} else {
			//qDebug() << "      value (outside) =" << value << ", row =" << row;
		}
	}
}




void ProfileGraph::draw_x_grid_sub_d(void)
{
	if (this->x_max_visible_d - this->x_min_visible_d == 0) {
		qDebug() << "EE:" PREFIX << "zero visible range:" << this->x_min_visible_d << this->x_max_visible_d;
		return;
	}

	int first_line = 0;
	int last_line = 0;
	find_grid_line_indices(this->x_min_visible_d, this->x_max_visible_d, this->x_interval_d, &first_line, &last_line);

#if 1
	qDebug() << "===== drawing x grid for graph" << this->get_graph_title() << ", width =" << this->width;
	qDebug() << "      min/max d on x axis visible:" << this->x_min_visible_d << this->x_max_visible_d;
	qDebug() << "      interval =" << this->x_interval_d << ", first_line/last_line =" << first_line << last_line;
#endif

	for (int i = first_line; i <= last_line; i++) {
		const double value = i * this->x_interval_d;

		/* 'col' is in "beginning in bottom-left corner" coordinate system. */
		const int col = this->width * ((value - this->x_min_visible_d) / (this->x_max_visible_d - this->x_min_visible_d));

		if (col >= 0 && col < this->width) {
			qDebug() << "      value (inside) =" << value << ", col =" << col;
			/* TODO: take into account magnitude of distance_value and adjust units accordingly. Look at get_distance_grid_label_2. */
			this->draw_grid_vertical_line(col, get_distance_grid_label(this->geocanvas.distance_unit, value));
		} else {
			qDebug() << "      value (outside) =" << value << ", col =" << col;
		}
	}
}




void ProfileGraph::draw_x_grid_sub_t(void)
{
	if (this->x_max_visible_t - this->x_min_visible_t == 0) {
		qDebug() << "EE:" PREFIX << "zero visible range:" << this->x_min_visible_t << this->x_max_visible_t;
		return;
	}

	int first_line = 0;
	int last_line = 0;
	find_grid_line_indices(this->x_min_visible_t, this->x_max_visible_t, this->x_interval_t, &first_line, &last_line);

#if 1
	qDebug() << "===== drawing x grid for graph" << this->get_graph_title() << ", width =" << this->width;
	qDebug() << "      min/max t on x axis visible:" << this->x_min_visible_t << this->x_max_visible_t;
	qDebug() << "      interval =" << this->x_interval_t << ", first_line/last_line =" << first_line << last_line;
#endif

	for (int i = first_line; i <= last_line; i++) {
		const time_t value = i * this->x_interval_t;

		/* 'col' is in "beginning in bottom-left corner" coordinate system. */
		/* Purposefully use "1.0 *" to enforce conversion to
		   float, to avoid losing data during integer division. */
		const int col = 1.0 * this->width * (value - this->x_min_visible_t) / (1.0 * (this->x_max_visible_t - this->x_min_visible_t));

		if (col >= 0 && col < this->width) {
			qDebug() << "      value (inside) =" << value << ", col =" << col;
			this->draw_grid_vertical_line(col, get_time_grid_label_2(this->x_interval_t, value));
		} else {
			qDebug() << "      value (outside) =" << value << ", col =" << col;
		}
	}
}




QString ProfileGraph::get_y_grid_label(float value)
{
	switch (this->geocanvas.y_domain) {
	case GeoCanvasDomain::Elevation:
		return get_elevation_grid_label(this->geocanvas.height_unit, value);

	case GeoCanvasDomain::Distance:
		return get_distance_grid_label(this->geocanvas.distance_unit, value);

	case GeoCanvasDomain::Speed:
		return get_speed_grid_label(this->geocanvas.speed_unit, value);

	case GeoCanvasDomain::Gradient:
		return QObject::tr("%1%").arg(value, 8, 'f', SG_PRECISION_GRADIENT);

	default:
		qDebug() << "EE:" PREFIX << "unhandled y domain" << (int) this->geocanvas.y_domain;
		return "";
	}
}




void ProfileGraph::draw_x_grid(const TrackInfo & track_info)
{
	switch (this->geocanvas.x_domain) {
	case GeoCanvasDomain::Time:
		this->draw_x_grid_sub_t();
		//this->draw_x_grid_time(0, track_info.duration);
		break;

	case GeoCanvasDomain::Distance:
		this->draw_x_grid_sub_d();
		//this->draw_x_grid_distance(0, track_info.track_length_including_gaps);
		break;

	default:
		qDebug() << "EE:" PREFIX << "unhandled x domain" << (int) this->geocanvas.x_domain;
		break;
	}
}



void ProfileGraph::draw_y_grid(void)
{
#if 1
	this->draw_y_grid_sub();
#else
	switch (this->geocanvas.y_domain) {

	case GeoCanvasDomain::Elevation:
		this->draw_y_grid_elevation();
		break;

	case GeoCanvasDomain::Distance:
		this->draw_y_grid_distance();
		break;

	case GeoCanvasDomain::Speed:
		this->draw_y_grid_speed();
		break;

	case GeoCanvasDomain::Gradient:
		this->draw_y_grid_gradient();
		break;

	default:
		qDebug() << "EE:" PREFIX << "unhandled y domain" << (int) this->geocanvas.y_domain;
		break;
	}
#endif
}




GeoCanvas::GeoCanvas()
{
	this->height_unit = Preferences::get_unit_height();
	this->distance_unit = Preferences::get_unit_distance();
	this->speed_unit = Preferences::get_unit_speed();

}
