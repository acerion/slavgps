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

#define PREFIX " Track Profile:" << __FUNCTION__ << __LINE__




enum {
	SG_TRACK_PROFILE_CANCEL,
	SG_TRACK_PROFILE_SPLIT_AT_MARKER,
	SG_TRACK_PROFILE_SPLIT_SEGMENTS,
	SG_TRACK_PROFILE_REVERSE,
	SG_TRACK_PROFILE_OK,
};




template <class T>
class Intervals
{
public:
	Intervals(const T * interval_values, int n_interval_values) : values(interval_values), n_values(n_interval_values) {};
	int get_interval_index(T min, T max, int n_intervals);
	T get_interval_value(int index);

	const T * values = NULL;
	int n_values = 0;
};




static Intervals <time_t> * time_intervals;
static Intervals <double> * distance_intervals;
static Intervals <double> * altitude_intervals;
static Intervals <double> * gradient_intervals;
static Intervals <double> * speed_intervals;





/* (Hopefully!) Human friendly altitude grid sizes - note no fixed 'ratio' just numbers that look nice... */
static const double altitude_interval_values[] = {2.0, 5.0, 10.0, 15.0, 20.0,
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
static const double distance_interval_values[] = {0.1, 0.2, 0.5, 1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 10.0,
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
static bool show_dem                = true;
static bool show_alt_gps_speed      = true;
static bool show_gps_speed          = true;
static bool show_gradient_gps_speed = true;
static bool show_dist_speed         = false;
static bool show_elev_speed         = false;
static bool show_elev_dem           = false;
static bool show_sd_gps_speed       = true;




static int get_cursor_pos_x_in_graph(Viewport * viewport, QMouseEvent * ev);
static void distance_label_update(QLabel * label, double meters_from_start);
static void elevation_label_update(QLabel * label, Trackpoint * tp);
static void time_label_update(QLabel * label, time_t seconds_from_start);
static void real_time_label_update(QLabel * label, Trackpoint * tp);
static void speed_label_update(QLabel * label, double value);
static void dist_dist_label_update(QLabel * label, double distance);
static void gradient_label_update(QLabel * label, double gradient);

static QString get_speed_grid_label(SpeedUnit speed_unit, double value);
static QString get_elevation_grid_label(HeightUnit height_unit, double value);
static QString get_distance_grid_label(DistanceUnit distance_unit, double value);
static QString get_distance_grid_label_2(DistanceUnit distance_unit, int interval_index, double value);
static QString get_time_grid_label(int interval_index, int value);




TrackProfileDialog::~TrackProfileDialog()
{
	delete time_intervals;
	delete distance_intervals;
	delete altitude_intervals;
	delete gradient_intervals;
	delete speed_intervals;

	delete this->graph_ed;
	delete this->graph_gd;
	delete this->graph_st;
	delete this->graph_dt;
	delete this->graph_et;
	delete this->graph_sd;
}




#define GRAPH_INITIAL_WIDTH 400
#define GRAPH_INITIAL_HEIGHT 300

#define GRAPH_MARGIN_LEFT 80 // 70
#define GRAPH_MARGIN_RIGHT 40 // 1
#define GRAPH_MARGIN_TOP 20
#define GRAPH_MARGIN_BOTTOM 30 // 1
#define GRAPH_X_INTERVALS 5
#define GRAPH_Y_INTERVALS 5




TrackData representation_creator_ed(Track * trk, int graph_width)
{
	return trk->make_values_vector_altitude_distance(graph_width);
}

TrackData representation_creator_gd(Track * trk, int graph_width)
{
	return trk->make_values_vector_gradient_distance(graph_width);
}

TrackData representation_creator_st(Track * trk, int graph_width)
{
	return trk->make_values_vector_speed_time(graph_width);
}

TrackData representation_creator_dt(Track * trk, int graph_width)
{
	return trk->make_values_vector_distance_time(graph_width);
}

TrackData representation_creator_et(Track * trk, int graph_width)
{
	return trk->make_values_vector_altitude_time(graph_width);
}

TrackData representation_creator_sd(Track * trk, int graph_width)
{
	return trk->make_values_vector_speed_distance(graph_width);
}




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
			return index;
		}
	}

	if (index != 0) {
		index--;
	}

	return index;
}




template <class T>
T Intervals<T>::get_interval_value(int index)
{
	return this->values[index];
}




void ProfileGraph::set_y_range_min_drawable(int interval_index, const double * interval_values, int n_interval_values, int n_intervals)
{
	/* Ensure adjusted minimum .. maximum covers min->max. */

	/* Now work out adjusted minimum point to the nearest lowest interval divisor value.
	   When negative ensure logic uses lowest value. */
	double interval = interval_values[interval_index];

	if (this->y_range_min < 0) {
		this->y_range_min_drawable = (double) ((int)((this->y_range_min - interval) / interval) * interval);
	} else {
		this->y_range_min_drawable = (double) ((int)(this->y_range_min / interval) * interval);
	}

	/* Range not big enough - as new minimum has lowered. */
	if ((this->y_range_min_drawable + (interval_values[interval_index] * n_intervals) < this->y_range_max)) {
		/* Next interval should cover it. */
		if (interval_index < n_interval_values - 1) {
			interval_index++;
			/* Remember to adjust the minimum too... */
			interval = interval_values[interval_index];
			if (this->y_range_min < 0) {
				this->y_range_min_drawable = (double) ((int)((this->y_range_min - interval) / interval) * interval);
			} else {
				this->y_range_min_drawable = (double) ((int)(this->y_range_min / interval) * interval);
			}
		}
	}

	this->y_interval = interval_values[interval_index];
}




/* Change what is displayed in main viewport in reaction to click event in one of Profile Dialog viewports. */
static Trackpoint * set_center_at_graph_position(int event_x,
						 LayerTRW * trw,
						 Viewport * main_viewport,
						 Track * trk,
						 bool time_base,
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
	if (time_base) {
		tp = trk->get_closest_tp_by_percentage_time((double) x / graph_width, NULL);
	} else {
		tp = trk->get_closest_tp_by_percentage_dist((double) x / graph_width, NULL);
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

   Both "pos" arguments should indicate position in viewport's canvas (the greater region) not viewport's graph area (the narrower region).
*/
void TrackProfileDialog::draw_marks(ProfileGraph * graph, const ScreenPos & selected_pos, const ScreenPos & current_pos)
{
	/* Restore previously saved image that has no marks on it, just the graph, border and margins. */
	if (graph->saved_img.valid) {
#if 0           /* Debug code. */
		qDebug() << "II:" PREFIX << "restoring saved image";
#endif
		graph->viewport->set_pixmap(graph->saved_img.img);
	} else {
		qDebug() << "WW:" PREFIX << "NOT restoring saved image";
	}

#if 0 /* Unused code. Leaving as reference. */
	/* ATM always save whole image - as anywhere could have changed. */
	if (graph->saved_img.img) {
		gdk_drawable_copy_to_image(GDK_DRAWABLE(pix), graph->saved_img.img, 0, 0, 0, 0, GRAPH_MARGIN_LEFT + graph_width, GRAPH_MARGIN_TOP + graph->height);
	} else {
		graph->saved_img.img = gdk_drawable_copy_to_image(GDK_DRAWABLE(pix), graph->saved_img.img, 0, 0, 0, 0, GRAPH_MARGIN_LEFT + graph_width, GRAPH_MARGIN_TOP + graph->height);
	}
	graph->saved_img.valid = true;
#endif

	/* Now draw marks on this fresh (restored from saved) image. */

	if (current_pos.x > 0 && current_pos.y > 0) {
		graph->viewport->draw_simple_crosshair(current_pos);
		this->is_current_drawn = true;
	} else {
		this->is_current_drawn = false;
	}


	if (selected_pos.x > 0 && selected_pos.y > 0) {
		graph->viewport->draw_simple_crosshair(selected_pos);
		this->is_selected_drawn = true;
	} else {
		this->is_selected_drawn = false;
	}

	if (this->is_selected_drawn || this->is_current_drawn) {
		graph->viewport->update();
	}
}




/**
   Return the percentage of how far a trackpoint is along a track via the time method.
*/
static double tp_percentage_by_time(Track * trk, Trackpoint * tp)
{
	if (tp == NULL) {
		return NAN;
	}

	const time_t t_start = (*trk->trackpoints.begin())->timestamp;
	const time_t t_end = (*std::prev(trk->trackpoints.end()))->timestamp;
	const time_t t_total = t_end - t_start;

	return (double) (tp->timestamp - t_start) / t_total;
}




/**
   Return the percentage of how far a trackpoint is a long a track via the distance method.
*/
static double tp_percentage_by_distance(Track * trk, Trackpoint * tp, double track_length)
{
	if (tp == NULL) {
		return NAN;
	}

	double dist = 0.0;
	auto iter = std::next(trk->trackpoints.begin());
	for (; iter != trk->trackpoints.end(); iter++) {
		dist += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);
		/* Assuming trackpoint is not a copy. */
		if (tp == *iter) {
			break;
		}
	}

	double pc = NAN;
	if (iter != trk->trackpoints.end()) {
		pc = dist / track_length;
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
	graph->width = viewport->get_graph_width();

	const int current_pos_x = get_cursor_pos_x_in_graph(viewport, ev);
	Trackpoint * tp = set_center_at_graph_position(current_pos_x, this->trw, this->main_viewport, this->trk, graph->is_time_graph, graph->width);
	if (tp == NULL) {
		/* Unable to get the point so give up. */
		this->button_split_at_marker->setEnabled(false);
		return;
	}

	this->selected_tp = tp;
	this->button_split_at_marker->setEnabled(true);


	ProfileGraph * graphs[SG_TRACK_PROFILE_TYPE_END + 1] = { this->graph_ed, this->graph_gd, this->graph_st, this->graph_dt, this->graph_et, this->graph_sd, NULL };


	/* Attempt to redraw marker on all graph types. TODO: why on all graphs, when event was only on one of them? */
	for (int type = SG_TRACK_PROFILE_TYPE_ED; type < SG_TRACK_PROFILE_TYPE_END; type++) {

		Viewport * graph_viewport = NULL;
		PropSaved * graph_saved_img = NULL;


		/* Common code of redrawing marker. */

		if (!graphs[type]->viewport) {
			continue;
		}


		double pc = NAN;
		if (graphs[type]->is_time_graph) {
			pc = tp_percentage_by_time(this->trk, tp);
		} else {
			pc = tp_percentage_by_distance(this->trk, tp, this->track_length_inc_gaps);
		}

		if (std::isnan(pc)) {
			continue;
		}

		//graph_width = graphs[type]->viewport->get_graph_width();
		graph->height = graphs[type]->viewport->get_graph_height();

		double selected_pos_x = pc * graph->width;
		double selected_pos_y = -1.0; /* TODO: get real value. */
		this->draw_marks(graphs[type],
				 /* Make sure that positions are canvas positions, not graph positions. */
				 ScreenPos(selected_pos_x + GRAPH_MARGIN_LEFT, GRAPH_MARGIN_TOP + graph->height - selected_pos_y),
				 ScreenPos(-1.0, -1.0)); /* Don't draw current position on clicks. */
	}
#ifdef K
	this->button_split_at_marker->setEnabled(this->is_selected_drawn);
#endif
}




bool TrackProfileDialog::track_ed_release_cb(Viewport * viewport, QMouseEvent * ev) /* Slot. */
{
	this->track_graph_release(viewport, ev, this->graph_ed);
	return true;
}




bool TrackProfileDialog::track_gd_release_cb(Viewport * viewport, QMouseEvent * ev) /* Slot. */
{
	this->track_graph_release(viewport, ev, this->graph_gd);
	return true;
}




bool TrackProfileDialog::track_st_release_cb(Viewport * viewport, QMouseEvent * ev) /* Slot. */
{
	this->track_graph_release(viewport, ev, this->graph_st);
	return true;
}




bool TrackProfileDialog::track_dt_release_cb(Viewport * viewport, QMouseEvent * ev) /* Slot. */
{
	this->track_graph_release(viewport, ev, this->graph_dt);
	return true;
}




bool TrackProfileDialog::track_et_release_cb(Viewport * viewport, QMouseEvent * ev) /* Slot. */
{
	this->track_graph_release(viewport, ev, this->graph_et);
	return true;
}




bool TrackProfileDialog::track_sd_release_cb(Viewport * viewport, QMouseEvent * ev) /* Slot. */
{
	this->track_graph_release(viewport, ev, this->graph_sd);
	return true;
}




/**
 * Calculate y position for mark on y=f(x) graph.
 */
double ProfileGraph::get_pos_y(double pos_x, const double * interval_values)
{
	int ix = (int) pos_x;
	/* Ensure ix is inside of graph. */
	if (ix == this->width) {
		ix--;
	}

	return this->height * (this->rep.y[ix] - this->y_range_min_drawable) / (this->y_interval * this->n_intervals_y);
}




void TrackProfileDialog::track_ed_move_cb(Viewport * viewport, QMouseEvent * ev)
{
	this->graph_ed->width = viewport->get_graph_width();
	this->graph_ed->height = viewport->get_graph_height();

	if (!this->graph_ed->rep.valid) {
		return;
	}

	const int current_pos_x = get_cursor_pos_x_in_graph(viewport, ev);
	if (current_pos_x < 0) {
		return;
	}

	double meters_from_start;
	this->current_tp = this->trk->get_closest_tp_by_percentage_dist((double) current_pos_x / this->graph_ed->width, &meters_from_start);
	if (this->current_tp && this->w_ed_current_distance) {
		distance_label_update(this->w_ed_current_distance, meters_from_start);
	}

	/* Show track elevation for this position - to the nearest whole number. */
	if (this->current_tp && this->w_ed_current_elevation) {
		elevation_label_update(this->w_ed_current_elevation, this->current_tp);
	}

	double current_pos_y = this->graph_ed->get_pos_y(current_pos_x, altitude_intervals->values);

	double selected_pos_x = -1.0; /* i.e. don't draw unless we get a valid value. */
	double selected_pos_y = -1.0;
	if (true || this->is_selected_drawn) {
		double pc = tp_percentage_by_distance(this->trk, this->selected_tp, this->track_length_inc_gaps);
		if (!std::isnan(pc)) {
			selected_pos_x = pc * this->graph_ed->width;
			selected_pos_y = this->graph_ed->get_pos_y(selected_pos_x, altitude_intervals->values);
		}
	}

	this->draw_marks(this->graph_ed,
			 /* Make sure that positions are canvas positions, not graph positions. */
			 ScreenPos(selected_pos_x + GRAPH_MARGIN_LEFT, GRAPH_MARGIN_TOP + this->graph_ed->height - selected_pos_y),
			 ScreenPos(current_pos_x + GRAPH_MARGIN_LEFT, GRAPH_MARGIN_TOP + this->graph_ed->height - current_pos_y));

}




void TrackProfileDialog::track_gd_move_cb(Viewport * viewport, QMouseEvent * ev)
{
	this->graph_gd->width = viewport->get_graph_width();
	this->graph_gd->height = viewport->get_graph_height();

	if (!this->graph_gd->rep.valid) {
		return;
	}

	const int current_pos_x = get_cursor_pos_x_in_graph(viewport, ev);
	if (current_pos_x < 0) {
		return;
	}

	double meters_from_start;
	this->current_tp = this->trk->get_closest_tp_by_percentage_dist((double) current_pos_x / this->graph_gd->width, &meters_from_start);
	if (this->current_tp && this->w_gd_current_distance) {
		distance_label_update(this->w_gd_current_distance, meters_from_start);
	}

	/* Show track gradient for this position - to the nearest whole number. */
	if (this->current_tp && this->w_gd_current_gradient) {
		gradient_label_update(this->w_gd_current_gradient, this->graph_gd->rep.y[current_pos_x]);
	}

	double current_pos_y = this->graph_gd->get_pos_y(current_pos_x, gradient_intervals->values);

	double selected_pos_x = -1.0; /* i.e. don't draw unless we get a valid value. */
	double selected_pos_y = -1.0;
	if (true || this->is_selected_drawn) {
		double pc = tp_percentage_by_distance(this->trk, this->selected_tp, this->track_length_inc_gaps);
		if (!std::isnan(pc)) {
			selected_pos_x = pc * this->graph_gd->width;
			selected_pos_y = this->graph_gd->get_pos_y(selected_pos_x, gradient_intervals->values);
		}
	}

	this->draw_marks(this->graph_gd,
			 /* Make sure that positions are canvas positions, not graph positions. */
			 ScreenPos(selected_pos_x + GRAPH_MARGIN_LEFT, GRAPH_MARGIN_TOP + this->graph_gd->height - selected_pos_y),
			 ScreenPos(current_pos_x + GRAPH_MARGIN_LEFT, GRAPH_MARGIN_TOP + this->graph_gd->height - current_pos_y));
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




void speed_label_update(QLabel * label, double value)
{
	/* Even if GPS speed available (tp->speed), the text will correspond to the speed map shown.
	   No conversions needed as already in appropriate units. */
	label->setText(Measurements::get_speed_string_dont_recalculate(value));
	return;
}




void gradient_label_update(QLabel * label, double gradient)
{
	label->setText(QObject::tr("%1").arg((int) gradient));
	return;
}




void TrackProfileDialog::track_st_move_cb(Viewport * viewport, QMouseEvent * ev)
{
	this->graph_st->width = viewport->get_graph_width();
	this->graph_st->height = viewport->get_graph_height();

	if (!this->graph_st->rep.valid) {
		return;
	}

	const int current_pos_x = get_cursor_pos_x_in_graph(viewport, ev);
	if (current_pos_x < 0) {
		return;
	}

	time_t seconds_from_start;
	this->current_tp = this->trk->get_closest_tp_by_percentage_time((double) current_pos_x / this->graph_st->width, &seconds_from_start);
	if (this->current_tp && this->w_st_current_time) {
		time_label_update(this->w_st_current_time, seconds_from_start);
	}

	if (this->current_tp && this->w_st_current_time_real) {
		real_time_label_update(this->w_st_current_time_real, this->current_tp);
	}

	/* Show track speed for this position. */
	if (this->current_tp && this->w_st_current_speed) {
		speed_label_update(this->w_st_current_speed, this->graph_st->rep.y[current_pos_x]);
	}

	double current_pos_y = this->graph_st->get_pos_y(current_pos_x, speed_intervals->values);

	double selected_pos_x = -1.0; /* i.e. don't draw unless we get a valid value. */
	double selected_pos_y = -1.0;
	if (true || this->is_selected_drawn) {
		double pc = tp_percentage_by_time(this->trk, this->selected_tp);
		if (!std::isnan(pc)) {
			selected_pos_x = pc * this->graph_st->width;
			selected_pos_y = this->graph_st->get_pos_y(selected_pos_x, speed_intervals->values);
		}
	}

	this->draw_marks(this->graph_st,
			 /* Make sure that positions are canvas positions, not graph positions. */
			 ScreenPos(selected_pos_x + GRAPH_MARGIN_LEFT, GRAPH_MARGIN_TOP + this->graph_st->height - selected_pos_y),
			 ScreenPos(current_pos_x + GRAPH_MARGIN_LEFT, GRAPH_MARGIN_TOP + this->graph_st->height - current_pos_y));
}




/**
 * Update labels and marker on mouse moves in the distance/time graph.
 */
void TrackProfileDialog::track_dt_move_cb(Viewport * viewport, QMouseEvent * ev)
{
	this->graph_dt->width = viewport->get_graph_width();
	this->graph_dt->height = viewport->get_graph_height();

	if (!this->graph_dt->rep.valid) {
		return;
	}

	const int current_pos_x = get_cursor_pos_x_in_graph(viewport, ev);
	if (current_pos_x < 0) {
		return;
	}

	time_t seconds_from_start;
	this->current_tp = this->trk->get_closest_tp_by_percentage_time((double) current_pos_x / this->graph_dt->width, &seconds_from_start);
	if (this->current_tp && this->w_dt_current_time) {
		time_label_update(this->w_dt_current_time, seconds_from_start);
	}

	if (this->current_tp && this->w_dt_current_time_real) {
		real_time_label_update(this->w_dt_current_time_real, this->current_tp);
	}

	if (this->current_tp && this->w_dt_curent_distance) {
		dist_dist_label_update(this->w_dt_curent_distance, this->graph_dt->rep.y[current_pos_x]);
	}

	double current_pos_y = this->graph_dt->get_pos_y(current_pos_x, distance_intervals->values);

	double selected_pos_x = -1.0; /* i.e. don't draw unless we get a valid value. */
	double selected_pos_y = -1.0;
	if (true || this->is_selected_drawn) {
		double pc = tp_percentage_by_time(this->trk, this->selected_tp);
		if (!std::isnan(pc)) {
			selected_pos_x = pc * this->graph_dt->width;
			selected_pos_y = this->graph_dt->get_pos_y(selected_pos_x, distance_intervals->values);
		}
	}

	this->draw_marks(this->graph_dt,
			 /* Make sure that positions are canvas positions, not graph positions. */
			 ScreenPos(selected_pos_x + GRAPH_MARGIN_LEFT, GRAPH_MARGIN_TOP + this->graph_dt->height - selected_pos_y),
			 ScreenPos(current_pos_x + GRAPH_MARGIN_LEFT, GRAPH_MARGIN_TOP + this->graph_dt->height - current_pos_y));
}




/**
 * Update labels and marker on mouse moves in the elevation/time graph.
 */
void TrackProfileDialog::track_et_move_cb(Viewport * viewport, QMouseEvent * ev)
{
	this->graph_et->width = viewport->get_graph_width();
	this->graph_et->height = viewport->get_graph_height();

	if (!this->graph_et->rep.valid) {
		return;
	}

	const int current_pos_x = get_cursor_pos_x_in_graph(viewport, ev);
	if (current_pos_x < 0) {
		return;
	}

	time_t seconds_from_start;
	this->current_tp = this->trk->get_closest_tp_by_percentage_time((double) current_pos_x / this->graph_et->width, &seconds_from_start);
	if (this->current_tp && this->w_et_current_time) {
		time_label_update(this->w_et_current_time, seconds_from_start);
	}

	if (this->current_tp && this->w_et_current_time_real) {
		real_time_label_update(this->w_et_current_time_real, this->current_tp);
	}

	if (this->current_tp && this->w_et_current_elevation) {
		elevation_label_update(this->w_et_current_elevation, this->current_tp);
	}

	double current_pos_y = this->graph_et->get_pos_y(current_pos_x, altitude_intervals->values);

	double selected_pos_x = -1.0; /* i.e. don't draw unless we get a valid value. */
	double selected_pos_y = -1.0;
	if (true || this->is_selected_drawn) {
		double pc = tp_percentage_by_time(this->trk, this->selected_tp);
		if (!std::isnan(pc)) {
			selected_pos_x = pc * this->graph_et->width;
			selected_pos_y = this->graph_et->get_pos_y(selected_pos_x, altitude_intervals->values);
		}
	}

	this->draw_marks(this->graph_et,
			 /* Make sure that positions are canvas positions, not graph positions. */
			 ScreenPos(selected_pos_x + GRAPH_MARGIN_LEFT, GRAPH_MARGIN_TOP + this->graph_et->height - selected_pos_y),
			 ScreenPos(current_pos_x + GRAPH_MARGIN_LEFT, GRAPH_MARGIN_TOP + this->graph_et->height - current_pos_y));
}




void TrackProfileDialog::track_sd_move_cb(Viewport * viewport, QMouseEvent * ev)
{
	this->graph_sd->width = viewport->get_graph_width();
	this->graph_sd->height = viewport->get_graph_height();

	if (!this->graph_sd->rep.valid) {
		return;
	}

	const int current_pos_x = get_cursor_pos_x_in_graph(viewport, ev);
	if (current_pos_x < 0) {
		return;
	}

	double meters_from_start;
	this->current_tp = this->trk->get_closest_tp_by_percentage_dist((double) current_pos_x / this->graph_sd->width, &meters_from_start);
	if (this->current_tp && this->w_sd_current_distance) {
		distance_label_update(this->w_sd_current_distance, meters_from_start);
	}

	/* Show track speed for this position. */
	if (this->w_sd_current_speed) {
		speed_label_update(this->w_sd_current_speed, this->graph_sd->rep.y[current_pos_x]);
	}

	double current_pos_y = this->graph_sd->get_pos_y(current_pos_x, speed_intervals->values);

	double selected_pos_x = -1.0; /* i.e. don't draw unless we get a valid value. */
	double selected_pos_y = -1.0;
	if (true || this->is_selected_drawn) {
		double pc = tp_percentage_by_distance(this->trk, this->selected_tp, this->track_length_inc_gaps);
		if (!std::isnan(pc)) {
			selected_pos_x = pc * this->graph_sd->width;
			selected_pos_y = this->graph_sd->get_pos_y(selected_pos_x, speed_intervals->values);
		}
	}

	this->draw_marks(this->graph_sd,
			 /* Make sure that positions are canvas positions, not graph positions. */
			 ScreenPos(selected_pos_x + GRAPH_MARGIN_LEFT, GRAPH_MARGIN_TOP + this->graph_sd->height - selected_pos_y),
			 ScreenPos(current_pos_x + GRAPH_MARGIN_LEFT, GRAPH_MARGIN_TOP + this->graph_sd->height - current_pos_y));
}




int get_cursor_pos_x_in_graph(Viewport * viewport, QMouseEvent * ev)
{
	const int graph_width = viewport->get_graph_width();
	const int graph_height = viewport->get_graph_height();
	const int graph_left_edge = viewport->get_graph_left_edge();
	const int graph_top_edge = viewport->get_graph_top_edge();

	QPoint position = viewport->mapFromGlobal(QCursor::pos());

	qDebug() << "II:" PREFIX << "x =" << ev->x() << "y =" << ev->y();

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




void distance_label_update(QLabel * label, double meters_from_start)
{
	const QString tmp_string = get_distance_string(meters_from_start, Preferences::get_unit_distance());
	label->setText(tmp_string);

	return;
}




void elevation_label_update(QLabel * label, Trackpoint * tp)
{
	/* Recalculate value into target unit. */
	label->setText(Measurements::get_altitude_string(tp->altitude, 0));

	return;
}




/* TODO: don't we have a function for this kind of stuff in measurements.cpp? */
void dist_dist_label_update(QLabel * label, double distance)
{
	static QString tmp_buf;
	switch (Preferences::get_unit_distance()) {
	case DistanceUnit::MILES:
		tmp_buf = QObject::tr("%1 miles").arg(distance, 0, 'f', 2);
		break;
	case DistanceUnit::NAUTICAL_MILES:
		tmp_buf = QObject::tr("%1 NM").arg(distance, 0, 'f', 2);
		break;
	default:
		tmp_buf = QObject::tr("%1 km").arg(distance, 0, 'f', 2); /* kamilTODO: why not distance/1000? */
		break;
	}

	label->setText(tmp_buf);

	return;
}




/**
 * Draws DEM points and a respresentative speed on the supplied pixmap
 * (which is the elevations graph).
 */
static void draw_dem_alt_speed_dist(Track * trk, ProfileGraph * graph, QPen & alt_pen, QPen & speed_pen, double max_speed_in, bool do_dem, bool do_speed)
{
	double max_speed = 0;
	double total_length = trk->get_length_including_gaps();

	/* Calculate the max speed factor. */
	if (do_speed) {
		max_speed = max_speed_in * 110 / 100;
	}

	double dist = 0;
	int achunk = graph->y_interval * graph->n_intervals_y;

	for (auto iter = std::next(trk->trackpoints.begin()); iter != trk->trackpoints.end(); iter++) {

		dist += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);
		int x = (graph->width * dist) / total_length + graph->left_edge;
		if (do_dem) {
			int16_t elev = DEMCache::get_elev_by_coord(&(*iter)->coord, DemInterpolation::BEST);
			if (elev != DEM_INVALID_ELEVATION) {
				/* Convert into height units. */
				if (Preferences::get_unit_height() == HeightUnit::FEET) {
					elev = VIK_METERS_TO_FEET(elev);
				}
				/* No conversion needed if already in metres. */

				/* offset is in current height units. */
				elev -= graph->y_range_min_drawable;

				/* consider chunk size. */
				int y_alt = graph->bottom_edge - ((graph->height * elev)/achunk);
				graph->viewport->fill_rectangle(alt_pen.color(), x - 2, y_alt - 2, 4, 4);
			}
		}
		if (do_speed) {
			/* This is just a speed indicator - no actual values can be inferred by user. */
			if (!std::isnan((*iter)->speed)) {
				int y_speed = graph->bottom_edge - (graph->height * (*iter)->speed) / max_speed;
				graph->viewport->fill_rectangle(speed_pen.color(), x - 2, y_speed - 2, 4, 4);
			}
		}
	}
}




/**
 * A common way to draw the grid with y axis labels
 */
void TrackProfileDialog::draw_grid_horizontal_line(ProfileGraph * graph, const QString & label, int pos_y)
{
	const float y_interval_px = 1.0 * graph->height / graph->n_intervals_y;

	QPointF text_anchor(0, graph->viewport->get_graph_top_edge() + graph->height - pos_y);
	QRectF bounding_rect = QRectF(text_anchor.x(), text_anchor.y(), text_anchor.x() + graph->left_edge - 10, y_interval_px - 3);
	graph->viewport->draw_text(this->labels_font, this->labels_pen, bounding_rect, Qt::AlignRight | Qt::AlignTop, label, SG_TEXT_OFFSET_UP);

	graph->viewport->draw_line(graph->viewport->grid_pen,
				   0,                pos_y,
				   0 + graph->width, pos_y);
}




void TrackProfileDialog::draw_grid_vertical_line(ProfileGraph * graph, const QString & label, int pos_x)
{
	float x_interval_px = 1.0 * graph->width / GRAPH_X_INTERVALS; /* TODO: this needs to be fixed. */

	const QPointF text_anchor(graph->left_edge + pos_x, GRAPH_MARGIN_TOP + graph->height);
	QRectF bounding_rect = QRectF(text_anchor.x(), text_anchor.y(), x_interval_px - 3, GRAPH_MARGIN_BOTTOM - 10);
	graph->viewport->draw_text(this->labels_font, this->labels_pen, bounding_rect, Qt::AlignLeft | Qt::AlignTop, label, SG_TEXT_OFFSET_LEFT);

	graph->viewport->draw_line(graph->viewport->grid_pen,
				   pos_x, 0,
				   pos_x, 0 + graph->height);
}



void TrackProfileDialog::draw_distance_grid(ProfileGraph * graph, DistanceUnit distance_unit, int n_intervals)
{
	/* Set to display units from length in metres. */
	double full_distance = this->track_length_inc_gaps;
	full_distance = convert_distance_meters_to(full_distance, distance_unit);

	const int interval_index = distance_intervals->get_interval_index(0, full_distance, n_intervals);
	const double distance_interval = distance_intervals->get_interval_value(interval_index);

	//double dist_per_pixel = full_distance / graph->width;

	const double per_interval_value = distance_interval * graph->width / full_distance;
	for (int interval_idx = 1; distance_interval * interval_idx <= full_distance; interval_idx++) {

		const double distance_value = distance_interval * interval_idx;
		const QString label = get_distance_grid_label_2(distance_unit, interval_index, distance_value);

		const int pos_x = (int) (interval_idx * per_interval_value);
		this->draw_grid_vertical_line(graph, label, pos_x);
	}
}




/**
 * Draw the elevation-distance image.
 */
void TrackProfileDialog::draw_ed(ProfileGraph * graph, Track * trk_)
{
	graph->regenerate_sizes();
	if (!graph->regenerate_y_values(trk_)) {
		return;
	}

	/* Convert into appropriate units. */
	HeightUnit height_unit = Preferences::get_unit_height();
	if (height_unit == HeightUnit::FEET) {
		/* Convert altitudes into feet units. */
		for (int i = 0; i < graph->width; i++) {
			graph->rep.y[i] = VIK_METERS_TO_FEET(graph->rep.y[i]);
		}
	}
	/* Otherwise leave in metres. */

	minmax_array(graph->rep.y, &graph->y_range_min, &graph->y_range_max, true, graph->width);

	graph->n_intervals_y = GRAPH_Y_INTERVALS;

	const int initial_interval_index = altitude_intervals->get_interval_index(graph->y_range_min, graph->y_range_max, graph->n_intervals_y);
        graph->set_y_range_min_drawable(initial_interval_index, altitude_intervals->values, altitude_intervals->n_values, graph->n_intervals_y);


	/* Reset before redrawing. */
	graph->viewport->clear();


	/* Draw values of 'elevation = f(distance)' function. */
	QPen no_alt_info_pen(QColor("yellow"));
	for (int i = 0; i < graph->width; i++) {
		if (graph->rep.y[i] == VIK_DEFAULT_ALTITUDE) {
			graph->viewport->draw_line(no_alt_info_pen,
						   i, 0,
						   i, 0 + graph->height);
		} else {
			graph->viewport->draw_line(this->main_pen,
						   i, graph->height,
						   i, graph->height - graph->height * (graph->rep.y[i] - graph->y_range_min_drawable) / (graph->y_interval * graph->n_intervals_y));
		}
	}

	/* Draw grid on top of graph of values. */
	this->draw_ed_grid(graph, height_unit);


	if (this->w_ed_show_dem->checkState()
	    || this->w_ed_show_gps_speed->checkState()) {

		QPen dem_alt_pen(QColor("green"));
		QPen gps_speed_pen(QColor("red"));

		/* Ensure somekind of max speed when not set. */
		if (this->max_speed < 0.01) {
			this->max_speed = trk_->get_max_speed();
		}

		draw_dem_alt_speed_dist(trk_, graph, dem_alt_pen, gps_speed_pen,
					this->max_speed,
					this->w_ed_show_dem->checkState(),
					this->w_ed_show_gps_speed->checkState());
	}


	//graph->viewport->draw_border();
	graph->viewport->update();

	/* The pixmap = margin + graph area. */
	qDebug() << "II: Track Profile: saving viewport" << graph->viewport->type_string;
	graph->saved_img.img = *graph->viewport->get_pixmap();
	graph->saved_img.valid = true;
}




/**
 * Draws representative speed on the supplied pixmap
 * (which is the gradients graph).
 */
static void draw_speed_dist(Track * trk, ProfileGraph * graph, QPen & speed_pen, double max_speed_in, const int graph_bottom_edge, bool do_speed)
{
	double max_speed = 0;
	double total_length = trk->get_length_including_gaps();

	/* Calculate the max speed factor. */
	if (do_speed) {
		max_speed = max_speed_in * 110 / 100;
	}

	double dist = 0;
	for (auto iter = std::next(trk->trackpoints.begin()); iter != trk->trackpoints.end(); iter++) {
		dist += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);
		int x = (graph->width * dist) / total_length + GRAPH_MARGIN_LEFT;
		if (do_speed) {
			/* This is just a speed indicator - no actual values can be inferred by user. */
			if (!std::isnan((*iter)->speed)) {
				int y_speed = graph_bottom_edge - (graph->height * (*iter)->speed) / max_speed;
				graph->viewport->fill_rectangle(speed_pen.color(), x - 2, y_speed - 2, 4, 4);
			}
		}
	}
}




/**
 * Draw the gradient-distance image.
 */
void TrackProfileDialog::draw_gd(ProfileGraph * graph, Track * trk_)
{
	graph->regenerate_sizes();
	if (!graph->regenerate_y_values(trk_)) {
		return;
	}

	minmax_array(graph->rep.y, &graph->y_range_min, &graph->y_range_max, true, graph->width);

	graph->n_intervals_y = GRAPH_Y_INTERVALS;

	const int initial_interval_index = gradient_intervals->get_interval_index(graph->y_range_min, graph->y_range_max, graph->n_intervals_y);
	graph->set_y_range_min_drawable(initial_interval_index, gradient_intervals->values, gradient_intervals->n_values, graph->n_intervals_y);


	/* Reset before redrawing. */
	graph->viewport->clear();


	/* Draw values of 'gradient = f(distance)' function. */
	for (int i = 0; i < graph->width; i++) {
		graph->viewport->draw_line(this->main_pen,
					   i, graph->height,
					   i, graph->height - graph->height * (graph->rep.y[i] - graph->y_range_min_drawable) / (graph->y_interval * graph->n_intervals_y));
	}

	/* Draw grid on top of graph of values. */
	this->draw_gd_grid(graph);


	if (this->w_gd_show_gps_speed->checkState()) {

		QPen gps_speed_pen(QColor("red"));

		/* Ensure somekind of max speed when not set. */
		if (this->max_speed < 0.01) {
			this->max_speed = trk_->get_max_speed();
		}

		draw_speed_dist(trk_,
				graph,
				gps_speed_pen,
				this->max_speed,
				graph->bottom_edge,
				this->w_gd_show_gps_speed->checkState());
	}


	//graph->viewport->draw_border();
	graph->viewport->update();

	/* The pixmap = margin + graph area. */
	qDebug() << "II: Track Profile: saving viewport" << graph->viewport->type_string;
	graph->saved_img.img = *graph->viewport->get_pixmap();
	graph->saved_img.valid = true;
}




void TrackProfileDialog::draw_time_grid(ProfileGraph * graph, int n_intervals)
{
	const int interval_index = time_intervals->get_interval_index(0, this->duration, n_intervals);
	const time_t time_interval = time_intervals->get_interval_value(interval_index);

	//double time_per_pixel = (double)(1.0 * this->duration) / graph->width;

	/* If stupidly long track in time - don't bother trying to draw grid lines. */
	if (this->duration > time_intervals->values[G_N_ELEMENTS(time_intervals->values)-1] * n_intervals * n_intervals) {
		return;
	}

	const double per_interval_value = time_interval * graph->width / (1.0 * this->duration);
	for (int interval_idx = 1; time_interval * interval_idx <= this->duration; interval_idx++) {

		const int time_value = time_interval * interval_idx;
		const QString label = get_time_grid_label(interval_index, time_value);

		const int pos_x = (int) (interval_idx * per_interval_value);
		this->draw_grid_vertical_line(graph, label, pos_x);
	}
}




/**
 * Draw the speed/time image.
 */
void TrackProfileDialog::draw_st(ProfileGraph * graph, Track * trk_)
{
	graph->regenerate_sizes();
	if (!graph->regenerate_y_values(trk_)) {
		return;
	}

	this->duration = trk_->get_duration(true);
	/* Negative time or other problem. */
	if (this->duration <= 0) {
		return;
	}

	/* Convert into appropriate units. */
	SpeedUnit speed_unit = Preferences::get_unit_speed();
	for (int i = 0; i < graph->width; i++) {
		graph->rep.y[i] = convert_speed_mps_to(graph->rep.y[i], speed_unit);
	}

	minmax_array(graph->rep.y, &graph->y_range_min, &graph->y_range_max, false, graph->width);
	if (graph->y_range_min < 0.0) {
		graph->y_range_min = 0; /* Splines sometimes give negative speeds. */
	}

	graph->n_intervals_y = GRAPH_Y_INTERVALS;

	const int initial_interval_index = speed_intervals->get_interval_index(graph->y_range_min, graph->y_range_max, graph->n_intervals_y);
	graph->set_y_range_min_drawable(initial_interval_index, speed_intervals->values, speed_intervals->n_values, graph->n_intervals_y);


	/* Reset before redrawing. */
	graph->viewport->clear();


	/* Draw values of 'speed = f(time)' function. */
	for (int i = 0; i < graph->width; i++) {
		graph->viewport->draw_line(this->main_pen,
					   i, graph->height,
					   i, graph->height - graph->height * (graph->rep.y[i] - graph->y_range_min_drawable) / (graph->y_interval * graph->n_intervals_y));
	}

	/* Draw grid on top of graph of values. */
	this->draw_st_grid(graph, speed_unit);


	if (this->w_st_show_gps_speed->checkState()) {

		QPen gps_speed_pen(QColor("red"));

		time_t beg_time = (*trk_->trackpoints.begin())->timestamp;
		time_t dur = (*std::prev(trk_->trackpoints.end()))->timestamp - beg_time;

		for (auto iter = trk_->trackpoints.begin(); iter != trk_->trackpoints.end(); iter++) {
			double gps_speed = (*iter)->speed;
			if (std::isnan(gps_speed)) {
				continue;
			}

			gps_speed = convert_speed_mps_to(gps_speed, speed_unit);

			int pos_x = graph->left_edge + graph->width * ((*iter)->timestamp - beg_time) / dur;
			int pos_y = graph->bottom_edge - graph->height * (gps_speed - graph->y_range_min_drawable) / (graph->y_interval * graph->n_intervals_y);
			graph->viewport->fill_rectangle(QColor("red"), pos_x - 2, pos_y - 2, 4, 4);
		}
	}


	//graph->viewport->draw_border();
	graph->viewport->update();

	/* The pixmap = margin + graph area. */
	qDebug() << "II: Track Profile: saving viewport" << graph->viewport->type_string;
	graph->saved_img.img = *graph->viewport->get_pixmap();
	graph->saved_img.valid = true;
}




/**
 * Draw the distance-time image.
 */
void TrackProfileDialog::draw_dt(ProfileGraph * graph, Track * trk_)
{
	graph->regenerate_sizes();
	if (!graph->regenerate_y_values(trk_)) {
		return;
	}

	/* Convert into appropriate units. */
	DistanceUnit distance_unit = Preferences::get_unit_distance();
	for (int i = 0; i < graph->width; i++) {
		graph->rep.y[i] = convert_distance_meters_to(graph->rep.y[i], distance_unit);
	}

	this->duration = this->trk->get_duration(true);
	/* Negative time or other problem. */
	if (this->duration <= 0) {
		return;
	}

	graph->y_range_min = 0;
	graph->y_range_max = convert_distance_meters_to(trk->get_length_including_gaps(), distance_unit);

	graph->n_intervals_y = GRAPH_Y_INTERVALS;

	const int initial_interval_index = distance_intervals->get_interval_index(graph->y_range_min, graph->y_range_max, graph->n_intervals_y);
	graph->set_y_range_min_drawable(initial_interval_index, distance_intervals->values, distance_intervals->n_values, graph->n_intervals_y);


	/* Reset before redrawing. */
	graph->viewport->clear();


	/* Draw values of 'distance = f(time)' function. */
	for (int i = 0; i < graph->width; i++) {
		graph->viewport->draw_line(this->main_pen,
					   i, graph->height,
					   i, graph->height - graph->height * (graph->rep.y[i]) / (graph->y_interval * graph->n_intervals_y));
	}

	/* Draw grid on top of graph of values. */
	this->draw_dt_grid(graph, distance_unit);


	/* Show speed indicator. */
	if (this->w_dt_show_speed->checkState()) {
		QPen dist_speed_gc(QColor("red"));

		double max_speed_ = this->max_speed * 110 / 100;

		/* This is just an indicator - no actual values can be inferred by user. */
		for (int i = 0; i < graph->width; i++) {
			int y_speed = graph->bottom_edge - (graph->height * graph->rep.y[i]) / max_speed_;
			graph->viewport->fill_rectangle(QColor("red"), graph->left_edge + i - 2, y_speed - 2, 4, 4);
		}
	}


	//graph->viewport->draw_border();
	graph->viewport->update();

	/* The pixmap = margin + graph area. */
	qDebug() << "II: Track Profile: saving viewport" << graph->viewport->type_string;
	graph->saved_img.img = *graph->viewport->get_pixmap();
	graph->saved_img.valid = true;
}




/**
 * Draw the elevation-time image.
 */
void TrackProfileDialog::draw_et(ProfileGraph * graph, Track * trk_)
{
	graph->regenerate_sizes();
	if (!graph->regenerate_y_values(trk_)) {
		return;
	}

	/* Convert into appropriate units. */
	HeightUnit height_unit = Preferences::get_unit_height();
	if (height_unit == HeightUnit::FEET) {
		/* Convert altitudes into feet units. */
		for (int i = 0; i < graph->width; i++) {
			graph->rep.y[i] = VIK_METERS_TO_FEET(graph->rep.y[i]);
		}
	}
	/* Otherwise leave in metres. */

	minmax_array(graph->rep.y, &graph->y_range_min, &graph->y_range_max, true, graph->width);

	graph->n_intervals_y = GRAPH_Y_INTERVALS;

	const int initial_interval_index = altitude_intervals->get_interval_index(graph->y_range_min, graph->y_range_max, graph->n_intervals_y);
	graph->set_y_range_min_drawable(initial_interval_index, altitude_intervals->values, altitude_intervals->n_values, graph->n_intervals_y);


	this->duration = this->trk->get_duration(true);
	/* Negative time or other problem. */
	if (this->duration <= 0) {
		return;
	}

	/* Reset before redrawing. */
	graph->viewport->clear();


	/* Draw values of 'elevation = f(time)' function. */
	for (int i = 0; i < graph->width; i++) {
		graph->viewport->draw_line(this->main_pen,
					   i, graph->height,
					   i, graph->height - graph->height * (graph->rep.y[i] - graph->y_range_min_drawable) / (graph->y_interval * graph->n_intervals_y));
	}

	/* Draw grid on top of graph of values. */
	this->draw_et_grid(graph, height_unit);


	/* Show DEMS. */
	if (this->w_et_show_dem->checkState())  {
		QPen dem_alt_pen(QColor("green"));

		int achunk = graph->y_interval * graph->n_intervals_y;

		for (int i = 0; i < graph->width; i++) {
			/* This could be slow doing this each time... */
			Trackpoint * tp = this->trk->get_closest_tp_by_percentage_time(((double) i / (double) graph->width), NULL);
			if (tp) {
				int16_t elev = DEMCache::get_elev_by_coord(&tp->coord, DemInterpolation::SIMPLE);
				if (elev != DEM_INVALID_ELEVATION) {
					/* Convert into height units. */
					if (Preferences::get_unit_height() == HeightUnit::FEET) {
						elev = VIK_METERS_TO_FEET(elev);
					}
					/* No conversion needed if already in metres. */

					/* offset is in current height units. */
					elev -= graph->y_range_min_drawable;

					/* Consider chunk size. */
					int y_alt = graph->bottom_edge - ((graph->height * elev)/achunk);
					graph->viewport->fill_rectangle(dem_alt_pen.color(), graph->left_edge + i - 2, y_alt - 2, 4, 4);
				}
			}
		}
	}


	/* Show speeds. */
	if (this->w_et_show_speed->checkState()) {
		/* This is just an indicator - no actual values can be inferred by user. */
		QPen elev_speed_pen(QColor("red"));

		double max_speed_ = this->max_speed * 110 / 100;

		for (int i = 0; i < graph->width; i++) {
			int y_speed = graph->bottom_edge - (graph->height * graph->rep.y[i]) / max_speed_;
			graph->viewport->fill_rectangle(elev_speed_pen.color(), graph->left_edge + i - 2, y_speed - 2, 4, 4);
		}
	}


	//graph->viewport->draw_border();
	graph->viewport->update();

	/* The pixmap = margin + graph area. */
	qDebug() << "II: Track Profile: saving viewport" << graph->viewport->type_string;
	graph->saved_img.img = *graph->viewport->get_pixmap();
	graph->saved_img.valid = true;
}




/**
 * Draw the speed-distance image.
 */
void TrackProfileDialog::draw_sd(ProfileGraph * graph, Track * trk_)
{
	graph->regenerate_sizes();
	if (!graph->regenerate_y_values(trk_)) {
		return;
	}

	/* Convert into appropriate units. */
	SpeedUnit speed_unit = Preferences::get_unit_speed();
	for (int i = 0; i < graph->width; i++) {
		graph->rep.y[i] = convert_speed_mps_to(graph->rep.y[i], speed_unit);
	}

	minmax_array(graph->rep.y, &graph->y_range_min, &graph->y_range_max, false, graph->width);
	if (graph->y_range_min < 0.0) {
		graph->y_range_min = 0; /* Splines sometimes give negative speeds. */
	}

	graph->n_intervals_y = GRAPH_Y_INTERVALS;

	const int initial_interval_index = speed_intervals->get_interval_index(graph->y_range_min, graph->y_range_max, graph->n_intervals_y);
	graph->set_y_range_min_drawable(initial_interval_index, speed_intervals->values, speed_intervals->n_values, graph->n_intervals_y);


	/* Reset before redrawing. */
	graph->viewport->clear();


	/* Draw values of 'speed = f(distance)' function. */
	for (int i = 0; i < graph->width; i++) {
		graph->viewport->draw_line(this->main_pen,
					   i, graph->height,
					   i, graph->height - graph->height * (graph->rep.y[i] - graph->y_range_min_drawable) / (graph->y_interval * graph->n_intervals_y));
	}

	/* Draw grid on top of graph of values. */
	this->draw_sd_grid(graph, speed_unit);


	if (this->w_sd_show_gps_speed->checkState()) {

		QPen gps_speed_pen(QColor("red"));

		double dist = trk_->get_length_including_gaps();
		double dist_tp = 0.0;

		for (auto iter = std::next(trk_->trackpoints.begin()); iter != trk_->trackpoints.end(); iter++) {
			double gps_speed = (*iter)->speed;
			if (std::isnan(gps_speed)) {
				continue;
			}

			gps_speed = convert_speed_mps_to(gps_speed, speed_unit);

			dist_tp += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);
			int pos_x = graph->left_edge + (graph->width * dist_tp / dist);
			int pos_y = graph->bottom_edge - graph->height * (gps_speed - graph->y_range_min_drawable)/(graph->y_interval * graph->n_intervals_y);
			graph->viewport->fill_rectangle(gps_speed_pen.color(), pos_x - 2, pos_y - 2, 4, 4);
		}
	}


	//graph->viewport->draw_border();
	graph->viewport->update();

	/* The pixmap = margin + graph area. */
	qDebug() << "II: Track Profile: saving viewport" << graph->viewport->type_string;
	graph->saved_img.img = *graph->viewport->get_pixmap();
	graph->saved_img.valid = true;
}




/**
 * Draw all graphs.
 */
void TrackProfileDialog::draw_all_graphs(bool resized)
{
	/* ed = elevation-distance. */
	if (this->graph_ed->viewport) {
		/* If dialog window is resized then saved image is no longer valid. */
		this->graph_ed->saved_img.valid = !resized;
		this->draw_single_graph(this->graph_ed, altitude_intervals->values);
	}

	/* gd = gradient-distance. */
	if (this->graph_gd->viewport) {
		/* If dialog window is resized then saved image is no longer valid. */
		this->graph_gd->saved_img.valid = !resized;
		this->draw_single_graph(this->graph_gd, gradient_intervals->values);
	}

	/* st = speed-time. */
	if (this->graph_st->viewport) {
		/* If dialog window is resized then saved image is no longer valid. */
		this->graph_st->saved_img.valid = !resized;
		this->draw_single_graph(this->graph_st, speed_intervals->values);
	}

	/* dt = distance-time. */
	if (this->graph_dt->viewport) {
		/* If dialog window is resized then saved image is no longer valid. */
		this->graph_dt->saved_img.valid = !resized;
		this->draw_single_graph(this->graph_dt, distance_intervals->values);
	}

	/* et = elevation-time. */
	if (this->graph_et->viewport) {
		/* If dialog window is resized then saved image is no longer valid. */
		this->graph_et->saved_img.valid = !resized;
		this->draw_single_graph(this->graph_et, altitude_intervals->values);
	}

	/* sd = speed-distance. */
	if (this->graph_sd->viewport) {
		/* If dialog window is resized then saved image is no longer valid. */
		this->graph_sd->saved_img.valid = !resized;
		this->draw_single_graph(this->graph_sd, speed_intervals->values);
	}
}




void TrackProfileDialog::draw_single_graph(ProfileGraph * graph, const double * interval_values)
{
	graph->width = graph->viewport->get_graph_width();
	graph->height = graph->viewport->get_graph_height();


	/* Call through pointer to member function... */
	(this->*graph->draw_graph_fn)(graph, this->trk);

	/* Ensure markers are redrawn if necessary. */
	if (this->is_selected_drawn || this->is_current_drawn) {

		double pc = NAN;
		int current_pos_x = -1; /* i.e. don't draw unless we get a valid value. */
		double current_pos_y = 0;
		if (this->is_current_drawn) {
			pc = NAN;
			if (graph->is_time_graph) {
				pc = tp_percentage_by_time(this->trk, this->current_tp);
			} else {
				pc = tp_percentage_by_distance(this->trk, this->current_tp, this->track_length_inc_gaps);
			}
			if (!std::isnan(pc)) {
				current_pos_x = pc * graph->width;
				/* Call through pointer to member function... */
				current_pos_y = graph->get_pos_y(current_pos_x, interval_values);
			}
		}

		double selected_pos_x = -1.0; /* i.e. Don't draw unless we get a valid value. */
		double selected_pos_y = -1.0;
		if (graph->is_time_graph) {
			pc = tp_percentage_by_time(this->trk, this->selected_tp);
		} else {
			pc = tp_percentage_by_distance(this->trk, this->selected_tp, this->track_length_inc_gaps);
		}
		if (!std::isnan(pc)) {
			selected_pos_x = pc * graph->width;
			/* Call through pointer to member function... */
			selected_pos_y = graph->get_pos_y(selected_pos_x, interval_values);
		}

		this->draw_marks(graph,
				 /* Make sure that positions are canvas positions, not graph positions. */
				 ScreenPos(selected_pos_x + GRAPH_MARGIN_LEFT, GRAPH_MARGIN_TOP + graph->height - selected_pos_y),
				 ScreenPos(current_pos_x + GRAPH_MARGIN_LEFT, GRAPH_MARGIN_TOP + graph->height - current_pos_y));
	}
}




/**
 * Configure/Resize the profile & speed/time images.
 */
bool TrackProfileDialog::paint_to_viewport_cb(Viewport * viewport)
{
	qDebug() << "SLOT:" PREFIX << "reacting to signal from viewport" << viewport->type_string;

#ifdef K
	if (widgets->configure_dialog) {
		/* Determine size offsets between dialog size and size for images.
		   Only on the initialisation of the dialog. */
		widgets->profile_width_offset = event->width - widgets->profile_width;
		widgets->profile_height_offset = event->height - widgets->profile_height;
		widgets->configure_dialog = false;

		/* Without this the settting, the dialog will only grow in vertical size - one can not then make it smaller! */
		gtk_widget_set_size_request(widget, widgets->profile_width+widgets->profile_width_offset, widgets->profile_height+widgets->profile_height_offset);

		/* Allow resizing back down to a minimal size (especially useful if the initial size has been made bigger after restoring from the saved settings). */
		GdkGeometry geom = { 600+widgets->profile_width_offset, 300+widgets->profile_height_offset, 0, 0, 0, 0, 0, 0, 0, 0, GDK_GRAVITY_STATIC };
		gdk_window_set_geometry_hints(gtk_widget_get_window(widget), &geom, GDK_HINT_MIN_SIZE);
	} else {
		widgets->profile_width_old = widgets->profile_width;
		widgets->profile_height_old = widgets->profile_height;
	}

	/* Now adjust From Dialog size to get image size. */
	widgets->profile_width = event->width - widgets->profile_width_offset;
	widgets->profile_height = event->height - widgets->profile_height_offset;

	/* ATM we receive configure_events when the dialog is moved and so no further action is necessary. */
	if (!widgets->configure_dialog &&
	    (widgets->profile_width_old == widgets->profile_width) && (widgets->profile_height_old == widgets->profile_height)) {

		return false;
	}
#endif
	/* TODO: shouldn't we re-allocate the per-viewport table of doubles here? */

	/* Draw stuff. */
	draw_all_graphs(true);

	return false;
}




/**
 * Create distance-time viewport.
 */
Viewport * TrackProfileDialog::create_viewport(const char * debug_label)
{
	const int initial_width = GRAPH_MARGIN_LEFT + GRAPH_INITIAL_WIDTH + GRAPH_MARGIN_RIGHT;
	const int initial_height = GRAPH_MARGIN_TOP + GRAPH_INITIAL_HEIGHT + GRAPH_MARGIN_BOTTOM;

	Viewport * viewport = new Viewport(this->parent);
	strcpy(viewport->type_string, debug_label);
	viewport->set_margin(GRAPH_MARGIN_TOP, GRAPH_MARGIN_BOTTOM, GRAPH_MARGIN_LEFT, GRAPH_MARGIN_RIGHT);
	viewport->resize(initial_width, initial_height);
	viewport->reconfigure_drawing_area(initial_width, initial_height);

	return viewport;
}




void TrackProfileDialog::save_values(void)
{
	/* Session settings. */
	ApplicationState::set_integer(VIK_SETTINGS_TRACK_PROFILE_WIDTH, this->profile_width);
	ApplicationState::set_integer(VIK_SETTINGS_TRACK_PROFILE_HEIGHT, this->profile_height);

	/* Just for this session. */
	if (this->w_ed_show_dem) {
		show_dem = this->w_ed_show_dem->checkState();
	}
	if (this->w_ed_show_gps_speed) {
		show_alt_gps_speed = this->w_ed_show_gps_speed->checkState();
	}
	if (this->w_st_show_gps_speed) {
		show_gps_speed = this->w_st_show_gps_speed->checkState();
	}
	if (this->w_gd_show_gps_speed) {
		show_gradient_gps_speed = this->w_gd_show_gps_speed->checkState();
	}
	if (this->w_dt_show_speed) {
		show_dist_speed = this->w_dt_show_speed->checkState();
	}
	if (this->w_et_show_dem) {
		show_elev_dem = this->w_et_show_dem->checkState();
	}
	if (this->w_et_show_speed) {
		show_elev_speed = this->w_et_show_speed->checkState();
	}
	if (this->w_sd_show_gps_speed) {
		show_sd_gps_speed = this->w_sd_show_gps_speed->checkState();
	}
}




void TrackProfileDialog::destroy_cb(void) /* Slot. */
{
	this->save_values();
	//delete widgets;
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
		this->trw->get_tracks_node().update_tree_view(this->trk);
		this->trw->emit_layer_changed();
		this->accept();
		break;
	case SG_TRACK_PROFILE_REVERSE:
		this->trk->reverse();
		this->trw->emit_layer_changed();
		keep_dialog = true;
		break;
	case SG_TRACK_PROFILE_SPLIT_SEGMENTS: {
		/* Get new tracks, add them and then the delete old one. old can still exist on clipboard. */
		std::list<Track *> * tracks = this->trk->split_into_segments();
		for (auto iter = tracks->begin(); iter != tracks->end(); iter++) {
			if (*iter) {
				const QString new_tr_name = this->trw->new_unique_element_name(this->trk->type_id, this->trk->name);
				(*iter)->set_name(new_tr_name);

				if (this->trk->type_id == "sg.trw.route") {
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
			if (this->trk->type_id == "sg.trw.route") {
				this->trw->delete_route(this->trk);
			} else {
				this->trw->delete_track(this->trk);
			}
			this->trw->emit_layer_changed(); /* Chase thru the hoops. */
		}
	}
		break;
	case SG_TRACK_PROFILE_SPLIT_AT_MARKER: {
		auto iter = std::next(this->trk->begin());
		while (iter != this->trk->end()) {
			if (this->selected_tp == *iter) {
				break;
			}
			iter++;
		}
		if (iter == this->trk->end()) {
			Dialog::error(tr("Failed to split track. Track unchanged"), this->trw->get_window());
			keep_dialog = true;
			break;
		}

		const QString r_name = this->trw->new_unique_element_name(this->trk->type_id, this->trk->name);


		/* Notice that here Trackpoint pointed to by iter is moved to new track. */
		/* kamilTODO: originally the constructor was just Track(). Should we really pass original trk to constructor? */
		/* TODO: move more copying of the stuff into constructor. */
		Track * trk_right = new Track(*this->trk, iter, this->trk->end());
		this->trk->erase(iter, this->trk->end());

		if (!this->trk->comment.isEmpty()) {
			trk_right->set_comment(this->trk->comment);
		}
		trk_right->visible = this->trk->visible;
		trk_right->type_id = this->trk->type_id;
		trk_right->set_name(r_name);

		if (this->trk->type_id == "sg.trw.route") {
			this->trw->add_route(trk_right);
		} else {
			this->trw->add_track(trk_right);
		}
		this->trk->calculate_bounds();
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
QWidget * TrackProfileDialog::create_graph_page(Viewport * viewport,
						const QString & text1,
						QLabel * value1,
						const QString & text2,
						QLabel * value2,
						const QString & text3,
						QLabel * value3,
						QCheckBox * checkbutton1,
						bool checkbutton1_default,
						QCheckBox * checkbutton2,
						bool checkbutton2_default)
{

	/* kamilTODO: who deletes these two pointers? */
	QHBoxLayout * hbox1 = new QHBoxLayout;
	QHBoxLayout * hbox2 = new QHBoxLayout;
	QVBoxLayout * vbox = new QVBoxLayout;

	QLabel * label1 = new QLabel(text1, this);
	QLabel * label2 = new QLabel(text2, this);
	QLabel * label3 = new QLabel(text3, this);

	viewport->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	vbox->addWidget(viewport);
	hbox1->addWidget(label1);
	hbox1->addWidget(value1);
	hbox1->addWidget(label2);
	hbox1->addWidget(value2);
	if (value3) {
		hbox1->addWidget(label3);
		hbox1->addWidget(value3);
	}
	vbox->addLayout(hbox1);

	if (checkbutton1) {
		hbox2->addWidget(checkbutton1);
		checkbutton1->setCheckState(checkbutton1_default ? Qt::Checked : Qt::Unchecked);
	}
	if (checkbutton2) {
		hbox2->addWidget(checkbutton2);
		checkbutton2->setCheckState(checkbutton2_default ? Qt::Checked : Qt::Unchecked);
	}
	vbox->addLayout(hbox2);


	QWidget * widget = new QWidget(this);
	widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	widget->setMinimumSize(500, 300);
	QLayout * old = widget->layout();
	delete old;
	widget->setLayout(vbox);

	return widget;

}




void SlavGPS::track_profile_dialog(Window * parent, Track * trk, Viewport * main_viewport)
{
	TrackProfileDialog dialog(QObject::tr("Track Profile"), trk, main_viewport, parent);
	trk->set_profile_dialog(&dialog);
	dialog.exec();
	trk->clear_profile_dialog();
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
	this->trk = a_trk;
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


	this->graph_ed = new ProfileGraph(false, &TrackProfileDialog::draw_ed, representation_creator_ed);
	this->graph_gd = new ProfileGraph(false, &TrackProfileDialog::draw_gd, representation_creator_gd);
	this->graph_st = new ProfileGraph(true, &TrackProfileDialog::draw_st, representation_creator_st);
	this->graph_dt = new ProfileGraph(true, &TrackProfileDialog::draw_dt, representation_creator_dt);
	this->graph_et = new ProfileGraph(true, &TrackProfileDialog::draw_et, representation_creator_et);
	this->graph_sd = new ProfileGraph(false, &TrackProfileDialog::draw_sd, representation_creator_sd);

	this->graph_ed->viewport = this->create_viewport("Viewport, elevation-distance");
	this->graph_gd->viewport = this->create_viewport("Viewport, gradient-distance");
	this->graph_st->viewport = this->create_viewport("Viewport, speed-time");
	this->graph_dt->viewport = this->create_viewport("Viewport, distance-time");
	this->graph_et->viewport = this->create_viewport("Viewport, elevation-time");
	this->graph_sd->viewport = this->create_viewport("Viewport, speed-distance");

	connect(this->graph_ed->viewport, SIGNAL (button_released(Viewport *, QMouseEvent *)), this, SLOT (track_ed_release_cb(Viewport *, QMouseEvent *)));
	connect(this->graph_ed->viewport, SIGNAL (cursor_moved(Viewport *, QMouseEvent *)),    this, SLOT (track_ed_move_cb(Viewport *, QMouseEvent *)));

	connect(this->graph_gd->viewport, SIGNAL (button_released(Viewport *, QMouseEvent *)), this, SLOT (track_gd_release_cb(Viewport *, QMouseEvent *)));
	connect(this->graph_gd->viewport, SIGNAL (cursor_moved(Viewport *, QMouseEvent *)),    this, SLOT (track_gd_move_cb(Viewport *, QMouseEvent *)));

	connect(this->graph_st->viewport, SIGNAL (button_released(Viewport *, QMouseEvent *)), this, SLOT (track_st_release_cb(Viewport *, QMouseEvent *)));
	connect(this->graph_st->viewport, SIGNAL (cursor_moved(Viewport *, QMouseEvent *)),    this, SLOT (track_st_move_cb(Viewport *, QMouseEvent *)));

	connect(this->graph_dt->viewport, SIGNAL (button_released(Viewport *, QMouseEvent *)), this, SLOT (track_dt_release_cb(Viewport *, QMouseEvent *)));
	connect(this->graph_dt->viewport, SIGNAL (cursor_moved(Viewport *, QMouseEvent *)),    this, SLOT (track_dt_move_cb(Viewport *, QMouseEvent *)));

	connect(this->graph_et->viewport, SIGNAL (button_released(Viewport *, QMouseEvent *)), this, SLOT (track_et_release_cb(Viewport *, QMouseEvent *)));
	connect(this->graph_et->viewport, SIGNAL (cursor_moved(Viewport *, QMouseEvent *)),    this, SLOT (track_et_move_cb(Viewport *, QMouseEvent *)));

	connect(this->graph_sd->viewport, SIGNAL (button_released(Viewport *, QMouseEvent *)), this, SLOT (track_sd_release_cb(Viewport *, QMouseEvent *)));
	connect(this->graph_sd->viewport, SIGNAL (cursor_moved(Viewport *, QMouseEvent *)),    this, SLOT (track_sd_move_cb(Viewport *, QMouseEvent *)));





	this->tabs = new QTabWidget();
	this->tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);


	/* NB This value not shown yet - but is used by internal calculations. */
	this->track_length_inc_gaps = trk->get_length_including_gaps();

	if (this->graph_ed->viewport) {
		this->w_ed_current_distance = ui_label_new_selectable(tr("No Data"), this);
		this->w_ed_current_elevation = ui_label_new_selectable(tr("No Data"), this);
		this->w_ed_show_dem = new QCheckBox(tr("Show D&EM"), this);
		this->w_ed_show_gps_speed = new QCheckBox(tr("Show &GPS Speed"), this);
		QWidget * page = this->create_graph_page(this->graph_ed->viewport,
							 tr("Track Distance:"), this->w_ed_current_distance,
							 tr("Track Height:"),   this->w_ed_current_elevation,
							 NULL, NULL,
							 this->w_ed_show_dem, show_dem,
							 this->w_ed_show_gps_speed, show_alt_gps_speed);
		connect(this->w_ed_show_dem, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		connect(this->w_ed_show_gps_speed, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		connect(this->graph_ed->viewport, SIGNAL (drawing_area_reconfigured(Viewport *)), this, SLOT(paint_to_viewport_cb(Viewport *)));
		this->tabs->addTab(page, tr("Elevation-distance"));
	}

	if (this->graph_gd->viewport) {
		this->w_gd_current_distance = ui_label_new_selectable(tr("No Data"), this);
		this->w_gd_current_gradient = ui_label_new_selectable(tr("No Data"), this);
		this->w_gd_show_gps_speed = new QCheckBox(tr("Show &GPS Speed"), this);
		QWidget * page = this->create_graph_page(this->graph_gd->viewport,
							 tr("Track Distance:"), this->w_gd_current_distance,
							 tr("Track Gradient:"), this->w_gd_current_gradient,
							 NULL, NULL,
							 this->w_gd_show_gps_speed, show_gradient_gps_speed,
							 NULL, false);
		connect(this->w_gd_show_gps_speed, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		connect(this->graph_gd->viewport, SIGNAL (drawing_area_reconfigured(Viewport *)), this, SLOT(paint_to_viewport_cb(Viewport *)));
		this->tabs->addTab(page, tr("Gradient-distance"));
	}

	if (this->graph_st->viewport) {
		this->w_st_current_time = ui_label_new_selectable(tr("No Data"), this);
		this->w_st_current_speed = ui_label_new_selectable(tr("No Data"), this);
		this->w_st_current_time_real = ui_label_new_selectable(tr("No Data"), this);
		this->w_st_show_gps_speed = new QCheckBox(tr("Show &GPS Speed"), this);
		QWidget * page = this->create_graph_page(this->graph_st->viewport,
							 tr("Track Time:"),  this->w_st_current_time,
							 tr("Track Speed:"), this->w_st_current_speed,
							 tr("Time/Date:"),   this->w_st_current_time_real,
							 this->w_st_show_gps_speed, show_gps_speed,
							 NULL, false);
		connect(this->w_st_show_gps_speed, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		connect(this->graph_st->viewport, SIGNAL (drawing_area_reconfigured(Viewport *)), this, SLOT(paint_to_viewport_cb(Viewport *)));
		this->tabs->addTab(page, tr("Speed-time"));
	}

	if (this->graph_dt->viewport) {
		this->w_dt_current_time = ui_label_new_selectable(tr("No Data"), this);
		this->w_dt_curent_distance = ui_label_new_selectable(tr("No Data"), this);
		this->w_dt_current_time_real = ui_label_new_selectable(tr("No Data"), this);
		this->w_dt_show_speed = new QCheckBox(tr("Show S&peed"), this);
		QWidget * page = this->create_graph_page(this->graph_dt->viewport,
							 tr("Track Distance:"), this->w_dt_curent_distance,
							 tr("Track Time:"), this->w_dt_current_time,
							 tr("Time/Date:"), this->w_dt_current_time_real,
							 this->w_dt_show_speed, show_dist_speed,
							 NULL, false);
		connect(this->w_dt_show_speed, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		connect(this->graph_dt->viewport, SIGNAL (drawing_area_reconfigured(Viewport *)), this, SLOT(paint_to_viewport_cb(Viewport *)));
		this->tabs->addTab(page, tr("Distance-time"));
	}

	if (this->graph_et->viewport) {
		this->w_et_current_time = ui_label_new_selectable(tr("No Data"), this);
		this->w_et_current_elevation = ui_label_new_selectable(tr("No Data"), this);
		this->w_et_current_time_real = ui_label_new_selectable(tr("No Data"), this);
		this->w_et_show_speed = new QCheckBox(tr("Show S&peed"), this);
		this->w_et_show_dem = new QCheckBox(tr("Show D&EM"), this);
		QWidget * page = this->create_graph_page(this->graph_et->viewport,
							 tr("Track Time:"),   this->w_et_current_time,
							 tr("Track Height:"), this->w_et_current_elevation,
							 tr("Time/Date:"),    this->w_et_current_time_real,
							 this->w_et_show_dem, show_elev_dem,
							 this->w_et_show_speed, show_elev_speed);
		connect(this->w_et_show_dem, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		connect(this->w_et_show_speed, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		connect(this->graph_et->viewport, SIGNAL (drawing_area_reconfigured(Viewport *)), this, SLOT(paint_to_viewport_cb(Viewport *)));
		this->tabs->addTab(page, tr("Elevation-time"));
	}

	if (this->graph_sd->viewport) {
		this->w_sd_current_distance = ui_label_new_selectable(tr("No Data"), this);
		this->w_sd_current_speed = ui_label_new_selectable(tr("No Data"), this);
		this->w_sd_show_gps_speed = new QCheckBox(tr("Show &GPS Speed"), this);
		QWidget * page = this->create_graph_page(this->graph_sd->viewport,
							 tr("Track Distance:"), this->w_sd_current_distance,
							 tr("Track Speed:"), this->w_sd_current_speed,
							 NULL, NULL,
							 this->w_sd_show_gps_speed, show_sd_gps_speed,
							 NULL, false);
		connect(this->w_sd_show_gps_speed, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		connect(this->graph_sd->viewport, SIGNAL (drawing_area_reconfigured(Viewport *)), this, SLOT(paint_to_viewport_cb(Viewport *)));
		this->tabs->addTab(page, tr("Speed-distance"));
	}



	this->button_box = new QDialogButtonBox();


	this->button_cancel = this->button_box->addButton(tr("&Cancel"), QDialogButtonBox::RejectRole);
	this->button_split_at_marker = this->button_box->addButton(tr("Split at &Marker"), QDialogButtonBox::ActionRole);
	this->button_split_segments = this->button_box->addButton(tr("Split &Segments"), QDialogButtonBox::ActionRole);
	this->button_reverse = this->button_box->addButton(tr("&Reverse"), QDialogButtonBox::ActionRole);
	this->button_ok = this->button_box->addButton(tr("&OK"), QDialogButtonBox::AcceptRole);

	this->button_split_segments->setEnabled(trk->get_segment_count() > 1);
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


	this->main_pen.setColor("lightsteelblue");
	this->main_pen.setWidth(1);

	this->labels_pen.setColor("black");

	this->labels_font.setFamily("Helvetica");
	this->labels_font.setPointSize(11);
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
		result = QObject::tr("%1 %m").arg((int) (value / 60));
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
		qDebug() << "EE:" PREFIX << "unexpected time interval index" << interval_index;
		break;
	}

	return result;
}




ProfileGraph::ProfileGraph(bool time_graph, void (TrackProfileDialog::*draw_graph)(ProfileGraph *, Track *), TrackData (*representation_creator)(Track *, int))
{
	this->is_time_graph = time_graph;
	this->draw_graph_fn = draw_graph;
	this->representation_creator_fn = representation_creator;
}




ProfileGraph::~ProfileGraph()
{
	delete this->viewport;
}




bool ProfileGraph::regenerate_y_values(Track * trk)
{
	this->rep.invalidate();

	/* Ask a track to generate a vector of values representing some parameter
	   of a track as a function of either time or distance. */
	this->rep = this->representation_creator_fn(trk, this->width);
	if (!this->rep.valid) {
		return false;
	}

	return true;
}




void ProfileGraph::regenerate_sizes(void)
{
	this->width = this->viewport->get_graph_width();
	this->height = this->viewport->get_graph_height();
	this->bottom_edge = this->viewport->get_graph_bottom_edge();
	this->left_edge = this->viewport->get_graph_left_edge();
}




void TrackProfileDialog::draw_st_grid(ProfileGraph * graph, SpeedUnit speed_unit)
{
	const double per_interval_value = graph->height / graph->n_intervals_y;
	for (int interval_idx = 0; interval_idx <= graph->n_intervals_y; interval_idx++) {
		/* No need to recalculate values based on units, it has been already done. */
		const double value = graph->y_range_min_drawable + interval_idx * graph->y_interval;
		const QString label = get_speed_grid_label(speed_unit, value);

		const int pos_y = (int) (interval_idx * per_interval_value);
		this->draw_grid_horizontal_line(graph, label, pos_y);
	}

	this->draw_time_grid(graph, GRAPH_X_INTERVALS);
}




void TrackProfileDialog::draw_dt_grid(ProfileGraph * graph, DistanceUnit distance_unit)
{
	const double per_interval_value = graph->height / graph->n_intervals_y;
	for (int interval_idx = 0; interval_idx <= graph->n_intervals_y; interval_idx++) {
		/* No need to recalculate values based on units, it has been already done. */
		const double value = graph->y_range_min_drawable + interval_idx * graph->y_interval;
		const QString label = get_distance_grid_label(distance_unit, value);

		const int pos_y = (int) (interval_idx * per_interval_value);
		this->draw_grid_horizontal_line(graph, label, pos_y);
	}

	this->draw_time_grid(graph, GRAPH_X_INTERVALS);
}




void TrackProfileDialog::draw_et_grid(ProfileGraph * graph, HeightUnit height_unit)
{
	const double per_interval_value = graph->height / graph->n_intervals_y;
	for (int interval_idx = 0; interval_idx <= graph->n_intervals_y; interval_idx++) {
		/* No need to recalculate values based on units, it has been already done. */
		const double value = graph->y_range_min_drawable + interval_idx * graph->y_interval;
		const QString label = get_elevation_grid_label(height_unit, value);

		const int pos_y = (int) (interval_idx * per_interval_value);
		this->draw_grid_horizontal_line(graph, label, pos_y);
	}

	this->draw_time_grid(graph, GRAPH_X_INTERVALS);
}




void TrackProfileDialog::draw_sd_grid(ProfileGraph * graph, SpeedUnit speed_unit)
{
	const double per_interval_value = graph->height / graph->n_intervals_y;
	for (int interval_idx = 0; interval_idx <= graph->n_intervals_y; interval_idx++) {
		/* No need to recalculate values based on units, it has been already done. */
		const double value = graph->y_range_min_drawable + interval_idx * graph->y_interval;
		const QString label = get_speed_grid_label(speed_unit, value);

		const int pos_y = (int) (interval_idx * per_interval_value);
		this->draw_grid_horizontal_line(graph, label, pos_y);
	}

	this->draw_distance_grid(graph, Preferences::get_unit_distance(), GRAPH_X_INTERVALS);
}




void TrackProfileDialog::draw_ed_grid(ProfileGraph * graph, HeightUnit height_unit)
{
	const double per_interval_value = graph->height / graph->n_intervals_y;
	for (int interval_idx = 0; interval_idx <= graph->n_intervals_y; interval_idx++) {
		/* No need to recalculate values based on units, it has been already done. */
		const double value = graph->y_range_min_drawable + interval_idx * graph->y_interval;
		const QString label = get_elevation_grid_label(height_unit, value);

		const int pos_y = (int) (interval_idx * per_interval_value);
		this->draw_grid_horizontal_line(graph, label, pos_y);
	}

	this->draw_distance_grid(graph, Preferences::get_unit_distance(), GRAPH_X_INTERVALS);
}




void TrackProfileDialog::draw_gd_grid(ProfileGraph * graph)
{
	const double per_interval_value = graph->height / graph->n_intervals_y;
	for (int interval_idx = 0; interval_idx <= graph->n_intervals_y; interval_idx++) {

		const double value = graph->y_range_min_drawable + interval_idx * graph->y_interval;
		const QString label = QObject::tr("%1%").arg(value, 8, 'f', SG_PRECISION_GRADIENT);

		const int pos_y = (int) (interval_idx * per_interval_value);
		this->draw_grid_horizontal_line(graph, label, pos_y);
	}

	this->draw_distance_grid(graph, Preferences::get_unit_distance(), GRAPH_X_INTERVALS);
}
