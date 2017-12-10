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
 *
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




using namespace SlavGPS;




enum {
	SG_TRACK_PROFILE_CANCEL,
	SG_TRACK_PROFILE_SPLIT_AT_MARKER,
	SG_TRACK_PROFILE_SPLIT_SEGMENTS,
	SG_TRACK_PROFILE_REVERSE,
	SG_TRACK_PROFILE_OK,
};




/* (Hopefully!) Human friendly altitude grid sizes - note no fixed 'ratio' just numbers that look nice... */
static const double chunksa[] = {2.0, 5.0, 10.0, 15.0, 20.0,
				 25.0, 40.0, 50.0, 75.0, 100.0,
				 150.0, 200.0, 250.0, 375.0, 500.0,
				 750.0, 1000.0, 2000.0, 5000.0, 10000.0, 100000.0};

/* (Hopefully!) Human friendly gradient grid sizes - note no fixed 'ratio' just numbers that look nice... */
static const double chunksg[] = {1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 10.0,
				 12.0, 15.0, 20.0, 25.0, 30.0, 35.0, 40.0, 45.0, 50.0, 75.0,
				 100.0, 150.0, 200.0, 250.0, 375.0, 500.0,
				  750.0, 1000.0, 10000.0, 100000.0};
/* Normally gradients should range up to couple hundred precent at most,
   however there are possibilities of having points with no altitude after a point with a big altitude
  (such as places with invalid DEM values in otherwise mountainous regions) - thus giving huge negative gradients. */

/* (Hopefully!) Human friendly grid sizes - note no fixed 'ratio' just numbers that look nice... */
/* As need to cover walking speeds - have many low numbers (but also may go up to airplane speeds!). */
static const double chunkss[] = {1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 10.0,
				 15.0, 20.0, 25.0, 40.0, 50.0, 75.0,
				 100.0, 150.0, 200.0, 250.0, 375.0, 500.0,
				 750.0, 1000.0, 10000.0};

/* (Hopefully!) Human friendly distance grid sizes - note no fixed 'ratio' just numbers that look nice... */
static const double chunksd[] = {0.1, 0.2, 0.5, 1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 10.0,
				 15.0, 20.0, 25.0, 40.0, 50.0, 75.0,
				 100.0, 150.0, 200.0, 250.0, 375.0, 500.0,
				 750.0, 1000.0, 10000.0};

/* Time chunks in seconds. */
static const time_t chunkst[] = {
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




TrackProfileDialog::~TrackProfileDialog()
{
	if (this->altitudes) {
		free(this->altitudes);
	}

	if (this->gradients) {
		free(this->gradients);
	}

	if (this->speeds) {
		free(this->speeds);
	}

	if (this->distances) {
		free(this->distances);
	}

	if (this->ats) {
		free(this->ats);
	}

	if (this->speeds_dist) {
		free(this->speeds_dist);
	}


	delete this->viewport_ed;
	delete this->viewport_gd;
	delete this->viewport_st;
	delete this->viewport_dt;
	delete this->viewport_et;
	delete this->viewport_sd;
}




#define GRAPH_INITIAL_WIDTH 400
#define GRAPH_INITIAL_HEIGHT 300

#define GRAPH_MARGIN_LEFT 80 // 70
#define GRAPH_MARGIN_RIGHT 40 // 1
#define GRAPH_MARGIN_TOP 20
#define GRAPH_MARGIN_BOTTOM 30 // 1
#define LINES 5
#define GRAPH_VERTICAL_LINE_S LINES
#define GRAPH_HORIZONTAL_LINE_S LINES




/**
 * Returns via pointers:
 * - the new minimum value to be used for the graph
 * - the index in to the chunk sizes array (ci = Chunk Index)
 */
static void get_new_min_and_chunk_index(double mina, double maxa, const double * chunks, size_t chunky, double * new_min, int * ci)
{
	/* Get unitized chunk. */
	/* Find suitable chunk index. */
	*ci = 0;
	double diff_chunk = (maxa - mina) / LINES;

	/* Loop through to find best match. */
	while (diff_chunk > chunks[*ci]) {
		(*ci)++;
		/* Last Resort Check */
		if (*ci == chunky) {
			/* Use previous value and exit loop. */
			(*ci)--;
			break;
		}
	}

	/* Ensure adjusted minimum .. maximum covers mina->maxa. */

	/* Now work out adjusted minimum point to the nearest lowest chunk divisor value.
	   When negative ensure logic uses lowest value. */
	if (mina < 0) {
		*new_min = (double) ((int)((mina - chunks[*ci]) / chunks[*ci]) * chunks[*ci]);
	} else {
		*new_min = (double) ((int)(mina / chunks[*ci]) * chunks[*ci]);
	}

	/* Range not big enough - as new minimum has lowered. */
	if ((*new_min + (chunks[*ci] * LINES) < maxa)) {
		/* Next chunk should cover it. */
		if (*ci < chunky-1) {
			(*ci)++;
			/* Remember to adjust the minimum too... */
			if (mina < 0) {
				*new_min = (double) ((int)((mina - chunks[*ci]) / chunks[*ci]) * chunks[*ci]);
			} else {
				*new_min = (double) ((int)(mina / chunks[*ci]) * chunks[*ci]);
			}
		}
	}
}




static unsigned int get_time_chunk_index(time_t duration)
{
	/* Grid split. */
	time_t myduration = duration / LINES;

	/* Search nearest chunk index. */
	unsigned int ci = 0;
	unsigned int last_chunk = G_N_ELEMENTS(chunkst);

	/* Loop through to find best match. */
	while (myduration > chunkst[ci]) {
		ci++;
		/* Last Resort Check. */
		if (ci == last_chunk) {
			break;
		}
	}
	/* Use previous value. */
	if (ci != 0) {
		ci--;
	}

	return ci;
}




static unsigned int get_distance_chunk_index(double length)
{
	/* Grid split. */
	double mylength = length / LINES;

	/* Search nearest chunk index. */
	unsigned int ci = 0;
	unsigned int last_chunk = G_N_ELEMENTS(chunksd);

	/* Loop through to find best match. */
	while (mylength > chunksd[ci]) {
		ci++;
		/* Last Resort Check. */
		if (ci == last_chunk) {
			break;
		}
	}
	/* Use previous value. */
	if (ci != 0) {
		ci--;
	}

	return ci;
}




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
   The other pair is for position of current trackpoint (trackpoint corresponding to current cursor position).
*/
void TrackProfileDialog::draw_marks(Viewport * viewport,
				    double selected_pos_x,
				    double selected_pos_y,
				    double current_pos_x,
				    double current_pos_y,
				    PropSaved * saved_img,
				    unsigned int graph_width,
				    unsigned int graph_height)
{
	unsigned int graph_top = GRAPH_MARGIN_TOP;

	/* Restore previously saved image that has no marks on it, just the graph, border and margins. */
	if (saved_img->valid) {
		qDebug() << "II: Track Profile: restoring saved image";
		viewport->set_pixmap(saved_img->img);
	} else {
		qDebug() << "WW: Track Profile: NOT restoring saved image";
	}

#if 0 /* Unused code. Leaving as reference. */
	/* ATM always save whole image - as anywhere could have changed. */
	if (saved_img->img) {
		gdk_drawable_copy_to_image(GDK_DRAWABLE(pix), saved_img->img, 0, 0, 0, 0, GRAPH_MARGIN_LEFT + graph_width, GRAPH_MARGIN_TOP + graph_height);
	} else {
		saved_img->img = gdk_drawable_copy_to_image(GDK_DRAWABLE(pix), saved_img->img, 0, 0, 0, 0, GRAPH_MARGIN_LEFT + graph_width, GRAPH_MARGIN_TOP + graph_height);
	}
	saved_img->valid = true;
#endif

	/* Now draw marks on this fresh image. */

	if (current_pos_x > 0 && current_pos_y > 0) {
		viewport->draw_line(viewport->marker_pen,
				    current_pos_x, 0,
				    current_pos_x, 0 + graph_height);

		viewport->draw_line(viewport->marker_pen,
				    0,               graph_height - current_pos_y,
				    0 + graph_width, graph_height - current_pos_y);

		this->is_current_drawn = true;
	} else {
		this->is_current_drawn = false;
	}


	if (selected_pos_x > 0 && selected_pos_y > 0) {
		viewport->draw_line(viewport->marker_pen,
				    selected_pos_x, 0,
				    selected_pos_x, 0 + graph_height);

		viewport->draw_line(viewport->marker_pen,
				    0,               graph_height - selected_pos_y,
				    0 + graph_width, graph_height - selected_pos_y);

		this->is_selected_drawn = true;
	} else {
		this->is_selected_drawn = false;
	}

	if (this->is_selected_drawn || this->is_current_drawn) {
		viewport->update();
	}
}




/**
 * Return the percentage of how far a trackpoint is a long a track via the time method.
 */
static double tp_percentage_by_time(Track * trk, Trackpoint * tp)
{
	double pc = NAN;
	if (tp == NULL) {
		return pc;
	}
	time_t t_start = (*trk->trackpoints.begin())->timestamp;
	time_t t_end = (*std::prev(trk->trackpoints.end()))->timestamp;
	time_t t_total = t_end - t_start;
	pc = (double) (tp->timestamp - t_start) / t_total;
	return pc;
}




/**
 * Return the percentage of how far a trackpoint is a long a track via the distance method.
 */
static double tp_percentage_by_distance(Track * trk, Trackpoint * tp, double track_length)
{
	double pc = NAN;
	if (tp == NULL) {
		return pc;
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
void TrackProfileDialog::track_graph_release(Viewport * viewport, QMouseEvent * ev, TrackProfileType graph_type)
{
	unsigned int graph_width = viewport->width() - GRAPH_MARGIN_LEFT - GRAPH_MARGIN_RIGHT;

	bool is_time_graph = (graph_type == SG_TRACK_PROFILE_TYPE_ST
			      || graph_type == SG_TRACK_PROFILE_TYPE_DT
			      || graph_type == SG_TRACK_PROFILE_TYPE_ET);

	int pos_x = get_cursor_pos_x_in_graph(viewport, ev);
	Trackpoint * tp = set_center_at_graph_position(pos_x, this->trw, this->main_viewport, this->trk, is_time_graph, graph_width);
	if (tp == NULL) {
		/* Unable to get the point so give up. */
		this->button_split_at_marker->setEnabled(false);
		return;
	}

	this->selected_tp = tp;
	this->button_split_at_marker->setEnabled(true);

	Viewport * graph_viewport = NULL;
	PropSaved * graph_saved_img = NULL;

	/* Attempt to redraw marker on all graph types. */
	for (int type = SG_TRACK_PROFILE_TYPE_ED; type < SG_TRACK_PROFILE_TYPE_END; type++) {

		/* Switch commonal variables to particular graph type. */
		switch (type) {
		default:
		case SG_TRACK_PROFILE_TYPE_ED:
			graph_viewport  = this->viewport_ed;
			graph_saved_img = &this->saved_img_ed;
			is_time_graph   = false;
			break;
		case SG_TRACK_PROFILE_TYPE_GD:
			graph_viewport  = this->viewport_gd;
			graph_saved_img = &this->saved_img_gd;
			is_time_graph   = false;
			break;
		case SG_TRACK_PROFILE_TYPE_ST:
			graph_viewport  = this->viewport_st;
			graph_saved_img = &this->saved_img_st;
			is_time_graph   = true;
			break;
		case SG_TRACK_PROFILE_TYPE_DT:
			graph_viewport  = this->viewport_dt;
			graph_saved_img = &this->saved_img_dt;
			is_time_graph   = true;
			break;
		case SG_TRACK_PROFILE_TYPE_ET:
			graph_viewport  = this->viewport_et;
			graph_saved_img = &this->saved_img_et;
			is_time_graph   = true;
			break;
		case SG_TRACK_PROFILE_TYPE_SD:
			graph_viewport  = this->viewport_sd;
			graph_saved_img = &this->saved_img_sd;
			is_time_graph   = false;
			break;
		}

		/* Common code of redrawing marker. */

		if (!graph_viewport) {
			continue;
		}

		double pc = NAN;
		if (is_time_graph) {
			pc = tp_percentage_by_time(this->trk, tp);
		} else {
			pc = tp_percentage_by_distance(this->trk, tp, this->track_length_inc_gaps);
		}

		if (std::isnan(pc)) {
			continue;
		}

		graph_width = graph_viewport->width() - GRAPH_MARGIN_LEFT - GRAPH_MARGIN_RIGHT;
		unsigned int graph_height = graph_viewport->height() - GRAPH_MARGIN_TOP - GRAPH_MARGIN_BOTTOM;

		double selected_pos_x = pc * graph_width;
		double selected_pos_y = -1.0; /* TODO: get real value. */
		this->draw_marks(graph_viewport,
				 selected_pos_x,
				 selected_pos_y,
				 -1.0, /* Don't draw current position on clicks. */
				 -1.0,
				 graph_saved_img,
				 graph_width,
				 graph_height);
	}
#ifdef K
	this->button_split_at_marker->setEnabled(this->is_selected_drawn);
#endif
}




bool TrackProfileDialog::track_ed_release_cb(Viewport * viewport, QMouseEvent * ev) /* Slot. */
{
	this->track_graph_release(viewport, ev, SG_TRACK_PROFILE_TYPE_ED);
	return true;
}




bool TrackProfileDialog::track_gd_release_cb(Viewport * viewport, QMouseEvent * ev) /* Slot. */
{
	this->track_graph_release(viewport, ev, SG_TRACK_PROFILE_TYPE_GD);
	return true;
}




bool TrackProfileDialog::track_st_release_cb(Viewport * viewport, QMouseEvent * ev) /* Slot. */
{
	this->track_graph_release(viewport, ev, SG_TRACK_PROFILE_TYPE_ST);
	return true;
}




bool TrackProfileDialog::track_dt_release_cb(Viewport * viewport, QMouseEvent * ev) /* Slot. */
{
	this->track_graph_release(viewport, ev, SG_TRACK_PROFILE_TYPE_DT);
	return true;
}




bool TrackProfileDialog::track_et_release_cb(Viewport * viewport, QMouseEvent * ev) /* Slot. */
{
	this->track_graph_release(viewport, ev, SG_TRACK_PROFILE_TYPE_ET);
	return true;
}




bool TrackProfileDialog::track_sd_release_cb(Viewport * viewport, QMouseEvent * ev) /* Slot. */
{
	this->track_graph_release(viewport, ev, SG_TRACK_PROFILE_TYPE_SD);
	return true;
}




/**
 * Calculate y position for mark on elevation-distance graph.
 */
double TrackProfileDialog::get_pos_y_ed(double pos_x, int width_size, int height_size)
{
	int ix = (int) pos_x;
	/* Ensure ix is inside of graph. */
	if (ix == width_size) {
		ix--;
	}

	return height_size * (this->altitudes[ix] - this->draw_min_altitude) / (chunksa[this->cia] * LINES);
}




/**
 * Calculate y position for mark on gradient-distance graph.
 */
double TrackProfileDialog::get_pos_y_gd(double pos_x, int width_size, int height_size)
{
	int ix = (int) pos_x;
	/* Ensure ix is inside of graph. */
	if (ix == width_size) {
		ix--;
	}

	return height_size * (this->gradients[ix] - this->draw_min_gradient) / (chunksg[this->cig] * LINES);
}




/**
 * Calculate y position for mark on speed-time graph.
 */
double TrackProfileDialog::get_pos_y_st(double pos_x, int width_size, int height_size)
{
	int ix = (int) pos_x;
	/* Ensure ix is inside of graph. */
	if (ix == width_size) {
		ix--;
	}
	return height_size * (this->speeds[ix] - this->draw_min_speed) / (chunkss[this->cis] * LINES);
}




/**
 * Calculate y position for mark on distance-time graph.
 */
double TrackProfileDialog::get_pos_y_dt(double pos_x, int width_size, int height_size)
{
	int ix = (int) pos_x;
	/* Ensure ix is inside of graph. */
	if (ix == width_size) {
		ix--;
	}
	/* Min distance is always 0, so no need to subtract that from distances[ix]. */
	return height_size * (this->distances[ix]) / (chunksd[this->cid] * LINES);
}




/**
 * Calculate y position for mark on elevation-time graph.
 */
double TrackProfileDialog::get_pos_y_et(double pos_x, int width_size, int height_size)
{
	int ix = (int) pos_x;
	/* Ensure ix is inside of graph. */
	if (ix == width_size) {
		ix--;
	}
	return height_size * (this->ats[ix] - this->draw_min_altitude_time) / (chunksa[this->ciat] * LINES);
}




/**
 * Calculate y position for mark on speed-distance graph.
 */
double TrackProfileDialog::get_pos_y_sd(double pos_x, int width_size, int height_size)
{
	int ix = (int) pos_x;
	/* Ensure ix is inside of graph. */
	if (ix == width_size) {
		ix--;
	}
	return height_size * (this->speeds_dist[ix] - this->draw_min_speed) / (chunkss[this->cisd] * LINES);
}




void TrackProfileDialog::track_ed_move_cb(Viewport * viewport, QMouseEvent * ev)
{
	unsigned int graph_width = viewport->width() - GRAPH_MARGIN_LEFT - GRAPH_MARGIN_RIGHT;
	unsigned int graph_height = viewport->height() - GRAPH_MARGIN_TOP - GRAPH_MARGIN_BOTTOM;

	if (this->altitudes == NULL) {
		return;
	}

	int current_pos_x = get_cursor_pos_x_in_graph(viewport, ev);
	if (current_pos_x < 0) {
		return;
	}

	double meters_from_start;
	this->current_tp = this->trk->get_closest_tp_by_percentage_dist((double) current_pos_x / graph_width, &meters_from_start);
	if (this->current_tp && this->w_ed_current_distance) {
		distance_label_update(this->w_ed_current_distance, meters_from_start);
	}

	/* Show track elevation for this position - to the nearest whole number. */
	if (this->current_tp && this->w_ed_current_elevation) {
		elevation_label_update(this->w_ed_current_elevation, this->current_tp);
	}

	double current_pos_y = this->get_pos_y_ed(current_pos_x, graph_width, graph_height);

	double selected_pos_x = -1.0; /* i.e. don't draw unless we get a valid value. */
	double selected_pos_y = -1.0;
	if (true || this->is_selected_drawn) {
		double pc = tp_percentage_by_distance(this->trk, this->selected_tp, this->track_length_inc_gaps);
		if (!std::isnan(pc)) {
			selected_pos_x = pc * graph_width;
			selected_pos_y = this->get_pos_y_ed(selected_pos_x, graph_width, graph_height);
		}
	}

	this->draw_marks(viewport,
			 selected_pos_x,
			 selected_pos_y,
			 current_pos_x,
			 current_pos_y,
			 &this->saved_img_ed,
			 graph_width,
			 graph_height);

}




void TrackProfileDialog::track_gd_move_cb(Viewport * viewport, QMouseEvent * ev)
{
	unsigned int graph_width = viewport->width() - GRAPH_MARGIN_LEFT - GRAPH_MARGIN_RIGHT;
	unsigned int graph_height = viewport->height() - GRAPH_MARGIN_TOP - GRAPH_MARGIN_BOTTOM;

	if (this->gradients == NULL) {
		return;
	}

	int current_pos_x = get_cursor_pos_x_in_graph(viewport, ev);
	if (current_pos_x < 0) {
		return;
	}

	double meters_from_start;
	this->current_tp = this->trk->get_closest_tp_by_percentage_dist((double) current_pos_x / graph_width, &meters_from_start);
	if (this->current_tp && this->w_gd_current_distance) {
		distance_label_update(this->w_gd_current_distance, meters_from_start);
	}

	/* Show track gradient for this position - to the nearest whole number. */
	if (this->current_tp && this->w_gd_current_gradient) {
		gradient_label_update(this->w_gd_current_gradient, this->gradients[current_pos_x]);
	}

	double current_pos_y = this->get_pos_y_gd(current_pos_x, graph_width, graph_height);

	double selected_pos_x = -1.0; /* i.e. don't draw unless we get a valid value. */
	double selected_pos_y = -1.0;
	if (true || this->is_selected_drawn) {
		double pc = tp_percentage_by_distance(this->trk, this->selected_tp, this->track_length_inc_gaps);
		if (!std::isnan(pc)) {
			selected_pos_x = pc * graph_width;
			selected_pos_y = this->get_pos_y_gd(selected_pos_x, graph_width, graph_height);
		}
	}

	this->draw_marks(viewport,
			 selected_pos_x,
			 selected_pos_y,
			 current_pos_x,
			 current_pos_y,
			 &this->saved_img_gd,
			 graph_width,
			 graph_height);
}




void time_label_update(QLabel * label, time_t seconds_from_start)
{
	static char tmp_buf[20];
	unsigned int h = seconds_from_start/3600;
	unsigned int m = (seconds_from_start - h*3600)/60;
	unsigned int s = seconds_from_start - (3600*h) - (60*m);
	snprintf(tmp_buf, sizeof(tmp_buf), "%02d:%02d:%02d", h, m, s);
	label->setText(QString(tmp_buf));

	return;
}




void real_time_label_update(QLabel * label, Trackpoint * tp)
{
	static char tmp_buf[64];
	if (tp->has_timestamp) {
		/* Alternatively could use %c format but I prefer a slightly more compact form here.
		   The full date can of course be seen on the Statistics tab. */
		strftime(tmp_buf, sizeof(tmp_buf), "%X %x %Z", localtime(&(tp->timestamp)));
	} else {
		snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
	}
	label->setText(QString(tmp_buf));

	return;
}




void speed_label_update(QLabel * label, double value)
{
	static char tmp_buf[20];
	/* Even if GPS speed available (tp->speed), the text will correspond to the speed map shown.
	   No conversions needed as already in appropriate units. */
	SpeedUnit speed_units = Preferences::get_unit_speed();
	switch (speed_units) {
	case SpeedUnit::KILOMETRES_PER_HOUR:
		snprintf(tmp_buf, sizeof(tmp_buf), _("%.1f kph"), value);
		break;
	case SpeedUnit::MILES_PER_HOUR:
		snprintf(tmp_buf, sizeof(tmp_buf), _("%.1f mph"), value);
		break;
	case SpeedUnit::KNOTS:
		snprintf(tmp_buf, sizeof(tmp_buf), _("%.1f knots"), value);
		break;
	default:
		/* SpeedUnit::METRES_PER_SECOND */
		snprintf(tmp_buf, sizeof(tmp_buf), _("%.1f m/s"), value);
		break;
	}
	label->setText(QString(tmp_buf));

	return;
}




void gradient_label_update(QLabel * label, double gradient)
{
	static char tmp_buf[20];
	snprintf(tmp_buf, sizeof(tmp_buf), "%d%%", (int) gradient);
	label->setText(QString(tmp_buf));

	return;
}




void TrackProfileDialog::track_st_move_cb(Viewport * viewport, QMouseEvent * ev)
{
	unsigned int graph_width = viewport->width() - GRAPH_MARGIN_LEFT - GRAPH_MARGIN_RIGHT;
	unsigned int graph_height = viewport->height() - GRAPH_MARGIN_TOP - GRAPH_MARGIN_BOTTOM;

	if (this->speeds == NULL) {
		return;
	}

	int current_pos_x = get_cursor_pos_x_in_graph(viewport, ev);
	if (current_pos_x < 0) {
		return;
	}

	time_t seconds_from_start;
	this->current_tp = this->trk->get_closest_tp_by_percentage_time((double) current_pos_x / graph_width, &seconds_from_start);
	if (this->current_tp && this->w_st_current_time) {
		time_label_update(this->w_st_current_time, seconds_from_start);
	}

	if (this->current_tp && this->w_st_current_time_real) {
		real_time_label_update(this->w_st_current_time_real, this->current_tp);
	}

	/* Show track speed for this position. */
	if (this->current_tp && this->w_st_current_speed) {
		speed_label_update(this->w_st_current_speed, this->speeds[current_pos_x]);
	}

	double current_pos_y = this->get_pos_y_st(current_pos_x, graph_width, graph_height);

	double selected_pos_x = -1.0; /* i.e. don't draw unless we get a valid value. */
	double selected_pos_y = -1.0;
	if (true || this->is_selected_drawn) {
		double pc = tp_percentage_by_time(this->trk, this->selected_tp);
		if (!std::isnan(pc)) {
			selected_pos_x = pc * graph_width;
			selected_pos_y = this->get_pos_y_st(selected_pos_x, graph_width, graph_height);
		}
	}

	this->draw_marks(viewport,
			 selected_pos_x,
			 selected_pos_y,
			 current_pos_x,
			 current_pos_y,
			 &this->saved_img_st,
			 graph_width,
			 graph_height);
}




/**
 * Update labels and marker on mouse moves in the distance/time graph.
 */
void TrackProfileDialog::track_dt_move_cb(Viewport * viewport, QMouseEvent * ev)
{
	unsigned int graph_width = viewport->width() - GRAPH_MARGIN_LEFT - GRAPH_MARGIN_RIGHT;
	unsigned int graph_height = viewport->height() - GRAPH_MARGIN_TOP - GRAPH_MARGIN_BOTTOM;

	if (this->distances == NULL) {
		return;
	}

	int current_pos_x = get_cursor_pos_x_in_graph(viewport, ev);
	if (current_pos_x < 0) {
		return;
	}

	time_t seconds_from_start;
	this->current_tp = this->trk->get_closest_tp_by_percentage_time((double) current_pos_x / graph_width, &seconds_from_start);
	if (this->current_tp && this->w_dt_current_time) {
		time_label_update(this->w_dt_current_time, seconds_from_start);
	}

	if (this->current_tp && this->w_dt_current_time_real) {
		real_time_label_update(this->w_dt_current_time_real, this->current_tp);
	}

	if (this->current_tp && this->w_dt_curent_distance) {
		dist_dist_label_update(this->w_dt_curent_distance, this->distances[current_pos_x]);
	}

	double current_pos_y = this->get_pos_y_dt(current_pos_x, graph_width, graph_height);

	double selected_pos_x = -1.0; /* i.e. don't draw unless we get a valid value. */
	double selected_pos_y = -1.0;
	if (true || this->is_selected_drawn) {
		double pc = tp_percentage_by_time(this->trk, this->selected_tp);
		if (!std::isnan(pc)) {
			selected_pos_x = pc * graph_width;
			selected_pos_y = this->get_pos_y_dt(selected_pos_x, graph_width, graph_height);
		}
	}

	this->draw_marks(viewport,
			 selected_pos_x,
			 selected_pos_y,
			 current_pos_x,
			 current_pos_y,
			 &this->saved_img_dt,
			 graph_width,
			 graph_height);
}




/**
 * Update labels and marker on mouse moves in the elevation/time graph.
 */
void TrackProfileDialog::track_et_move_cb(Viewport * viewport, QMouseEvent * ev)
{
	unsigned int graph_width = viewport->width() - GRAPH_MARGIN_LEFT - GRAPH_MARGIN_RIGHT;
	unsigned int graph_height = viewport->height() - GRAPH_MARGIN_TOP - GRAPH_MARGIN_BOTTOM;

	if (this->ats == NULL) {
		return;
	}

	int current_pos_x = get_cursor_pos_x_in_graph(viewport, ev);
	if (current_pos_x < 0) {
		return;
	}

	time_t seconds_from_start;
	this->current_tp = this->trk->get_closest_tp_by_percentage_time((double) current_pos_x / graph_width, &seconds_from_start);
	if (this->current_tp && this->w_et_current_time) {
		time_label_update(this->w_et_current_time, seconds_from_start);
	}

	if (this->current_tp && this->w_et_current_time_real) {
		real_time_label_update(this->w_et_current_time_real, this->current_tp);
	}

	if (this->current_tp && this->w_et_current_elevation) {
		elevation_label_update(this->w_et_current_elevation, this->current_tp);
	}

	double current_pos_y = this->get_pos_y_et(current_pos_x, graph_width, graph_height);

	double selected_pos_x = -1.0; /* i.e. don't draw unless we get a valid value. */
	double selected_pos_y = -1.0;
	if (true || this->is_selected_drawn) {
		double pc = tp_percentage_by_time(this->trk, this->selected_tp);
		if (!std::isnan(pc)) {
			selected_pos_x = pc * graph_width;
			selected_pos_y = this->get_pos_y_et(selected_pos_x, graph_width, graph_height);
		}
	}

	this->draw_marks(viewport,
			 selected_pos_x,
			 selected_pos_y,
			 current_pos_x,
			 current_pos_y,
			 &this->saved_img_et,
			 graph_width,
			 graph_height);
}




void TrackProfileDialog::track_sd_move_cb(Viewport * viewport, QMouseEvent * ev)
{
	unsigned int graph_width = viewport->width() - GRAPH_MARGIN_LEFT - GRAPH_MARGIN_RIGHT;
	unsigned int graph_height = viewport->height() - GRAPH_MARGIN_TOP - GRAPH_MARGIN_BOTTOM;

	if (this->speeds_dist == NULL) {
		return;
	}

	int current_pos_x = get_cursor_pos_x_in_graph(viewport, ev);
	if (current_pos_x < 0) {
		return;
	}

	double meters_from_start;
	this->current_tp = this->trk->get_closest_tp_by_percentage_dist((double) current_pos_x / graph_width, &meters_from_start);
	if (this->current_tp && this->w_sd_current_distance) {
		distance_label_update(this->w_sd_current_distance, meters_from_start);
	}

	/* Show track speed for this position. */
	if (this->w_sd_current_speed) {
		speed_label_update(this->w_sd_current_speed, this->speeds_dist[current_pos_x]);
	}

	double current_pos_y = this->get_pos_y_sd(current_pos_x, graph_width, graph_height);

	double selected_pos_x = -1.0; /* i.e. don't draw unless we get a valid value. */
	double selected_pos_y = -1.0;
	if (true || this->is_selected_drawn) {
		double pc = tp_percentage_by_distance(this->trk, this->selected_tp, this->track_length_inc_gaps);
		if (!std::isnan(pc)) {
			selected_pos_x = pc * graph_width;
			selected_pos_y = this->get_pos_y_sd(selected_pos_x, graph_width, graph_height);
		}
	}

	this->draw_marks(viewport,
			 selected_pos_x,
			 selected_pos_y,
			 current_pos_x,
			 current_pos_y,
			 &this->saved_img_sd,
			 graph_width,
			 graph_height);
}




int get_cursor_pos_x_in_graph(Viewport * viewport, QMouseEvent * ev)
{
	int graph_width = viewport->width() - GRAPH_MARGIN_LEFT - GRAPH_MARGIN_RIGHT;
	int graph_height = viewport->height() - GRAPH_MARGIN_TOP - GRAPH_MARGIN_BOTTOM;
	int graph_left = GRAPH_MARGIN_LEFT;
	int graph_top = GRAPH_MARGIN_TOP;

	QPoint position = viewport->mapFromGlobal(QCursor::pos());

	int mouse_x = position.x();
	int mouse_y = position.y();

	if (!(mouse_x >= graph_left && mouse_x <= graph_left + graph_width
	      && mouse_y >= graph_top && mouse_y <= graph_top + graph_height)) {

		/* Cursor outside of chart area. */
		return -1;
	}

	int x = position.x() - graph_left;
	if (x < 0) {
		qDebug() << "EE: Track Profile: condition 1 for mouse movement failed:" << x << position.x() << graph_left;
		x = 0;
	}

	if (x > graph_width) {
		qDebug() << "EE: Track Profile: condition 2 for mouse movement failed:" << x << position.x() << graph_width;
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
	static char tmp_buf[20];
	if (Preferences::get_unit_height() == HeightUnit::FEET) {
		snprintf(tmp_buf, sizeof(tmp_buf), "%d ft", (int)VIK_METERS_TO_FEET(tp->altitude));
	} else {
		snprintf(tmp_buf, sizeof(tmp_buf), "%d m", (int) tp->altitude);
	}
	label->setText(QString(tmp_buf));

	return;
}




void dist_dist_label_update(QLabel * label, double distance)
{
	static char tmp_buf[20];
	switch (Preferences::get_unit_distance()) {
	case DistanceUnit::MILES:
		snprintf(tmp_buf, sizeof(tmp_buf), "%.2f miles", distance);
		break;
	case DistanceUnit::NAUTICAL_MILES:
		snprintf(tmp_buf, sizeof(tmp_buf), "%.2f NM", distance);
		break;
	default:
		snprintf(tmp_buf, sizeof(tmp_buf), "%.2f km", distance); /* kamilTODO: why not distance/1000? */
		break;
	}

	label->setText(QString(tmp_buf));

	return;
}




/**
 * Draws DEM points and a respresentative speed on the supplied pixmap
 * (which is the elevations graph).
 */
static void draw_dem_alt_speed_dist(Track * trk,
				    Viewport * viewport,
				    QPen & alt_pen,
				    QPen & speed_pen,
				    double alt_offset,
				    double max_speed_in,
				    int cia,
				    unsigned int graph_width,
				    unsigned int graph_height,
				    unsigned int graph_bottom,
				    int margin,
				    bool do_dem,
				    bool do_speed)
{
	double max_speed = 0;
	double total_length = trk->get_length_including_gaps();

	/* Calculate the max speed factor. */
	if (do_speed) {
		max_speed = max_speed_in * 110 / 100;
	}

	double dist = 0;
	int achunk = chunksa[cia] * LINES;

	for (auto iter = std::next(trk->trackpoints.begin()); iter != trk->trackpoints.end(); iter++) {

		dist += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);
		int x = (graph_width * dist) / total_length + margin;
		if (do_dem) {
			int16_t elev = DEMCache::get_elev_by_coord(&(*iter)->coord, DemInterpolation::BEST);
			if (elev != DEM_INVALID_ELEVATION) {
				/* Convert into height units. */
				if (Preferences::get_unit_height() == HeightUnit::FEET) {
					elev = VIK_METERS_TO_FEET(elev);
				}
				/* No conversion needed if already in metres. */

				/* offset is in current height units. */
				elev -= alt_offset;

				/* consider chunk size. */
				int y_alt = graph_bottom - ((graph_height * elev)/achunk);
				viewport->fill_rectangle(alt_pen.color(), x - 2, y_alt - 2, 4, 4);
			}
		}
		if (do_speed) {
			/* This is just a speed indicator - no actual values can be inferred by user. */
			if (!std::isnan((*iter)->speed)) {
				int y_speed = graph_bottom - (graph_height * (*iter)->speed) / max_speed;
				viewport->fill_rectangle(speed_pen.color(), x - 2, y_speed - 2, 4, 4);
			}
		}
	}
}




/**
 * A common way to draw the grid with y axis labels
 */
void TrackProfileDialog::draw_horizontal_grid(Viewport * viewport, char * ss, int i)
{
	unsigned int graph_height = viewport->height() - GRAPH_MARGIN_TOP - GRAPH_MARGIN_BOTTOM;
	unsigned int graph_width = viewport->width() - GRAPH_MARGIN_LEFT - GRAPH_MARGIN_RIGHT;
	unsigned int graph_left = GRAPH_MARGIN_LEFT;
	unsigned int graph_top = GRAPH_MARGIN_TOP;

	float delta_y = 1.0 * graph_height / LINES;
	float pos_y = graph_height - delta_y * i;

	QString text(ss);
	QPointF text_anchor(0, graph_top + graph_height - pos_y);
	QRectF bounding_rect = QRectF(text_anchor.x(), text_anchor.y(), text_anchor.x() + graph_left - 10, delta_y - 3);
	viewport->draw_text(this->labels_font, this->labels_pen, bounding_rect, Qt::AlignRight | Qt::AlignTop, text, SG_TEXT_OFFSET_UP);


	viewport->draw_line(viewport->grid_pen,
			    0,               pos_y,
			    0 + graph_width, pos_y);
}




/**
 * A common way to draw the grid with x axis labels for time graphs
 */
void TrackProfileDialog::draw_vertical_grid_time(Viewport * viewport, unsigned int index, unsigned int grid_x, unsigned int time_value)
{
	unsigned int graph_width = viewport->width() - GRAPH_MARGIN_LEFT - GRAPH_MARGIN_RIGHT;
	unsigned int graph_height = viewport->height() - GRAPH_MARGIN_TOP - GRAPH_MARGIN_BOTTOM;
	unsigned int graph_left = GRAPH_MARGIN_LEFT;

	char buf[64] = { 0 };

	switch (index) {
	case 0:
	case 1:
	case 2:
	case 3:
		/* Minutes. */
		sprintf(buf, "%d %s", time_value / 60, _("mins"));
		break;
	case 4:
	case 5:
	case 6:
	case 7:
		/* Hours. */
		sprintf(buf, "%.1f %s", (double) time_value / (60 * 60), _("h"));
		break;
	case 8:
	case 9:
	case 10:
		/* Days. */
		sprintf(buf, "%.1f %s", (double) time_value / (60 *60 * 24), _("d"));
		break;
	case 11:
	case 12:
		/* Weeks. */
		sprintf(buf, "%.1f %s", (double) time_value / (60 * 60 * 24 * 7), _("w"));
		break;
	case 13:
		/* Months. */
		sprintf(buf, "%.1f %s", (double) time_value / (60 * 60 * 24 * 28), _("M"));
		break;
	default:
		break;
	}

	float delta_x = 1.0 * graph_width / LINES; /* TODO: this needs to be fixed. */

	QString text(buf);
	QPointF text_anchor(graph_left + grid_x, GRAPH_MARGIN_TOP + graph_height);
	QRectF bounding_rect = QRectF(text_anchor.x(), text_anchor.y(), delta_x - 3, GRAPH_MARGIN_BOTTOM - 10);
	viewport->draw_text(this->labels_font, this->labels_pen, bounding_rect, Qt::AlignLeft | Qt::AlignTop, text, SG_TEXT_OFFSET_LEFT);


	viewport->draw_line(viewport->grid_pen,
			    grid_x, 0,
			    grid_x, 0 + graph_height);

}




/**
 * A common way to draw the grid with x axis labels for distance graphs.
 */
void TrackProfileDialog::draw_vertical_grid_distance(Viewport * viewport, unsigned int index, unsigned int grid_x, double distance_value, DistanceUnit distance_unit)
{
	unsigned int graph_height = viewport->height() - GRAPH_MARGIN_TOP - GRAPH_MARGIN_BOTTOM;
	unsigned int graph_width = viewport->width() - GRAPH_MARGIN_LEFT - GRAPH_MARGIN_RIGHT;
	unsigned int graph_left = GRAPH_MARGIN_LEFT;

	const QString distance_unit_string = get_distance_unit_string(distance_unit);

	QString text;
	if (index > 4) {
		text = QObject::tr("%1 %2").arg((unsigned int) distance_value).arg(distance_unit_string);
	} else {
		text = QObject::tr("%1 %2").arg(distance_value, 0, 'f', 1).arg(distance_unit_string);
	}


	float delta_x = 1.0 * graph_width / LINES; /* TODO: this needs to be fixed. */

	QPointF text_anchor(graph_left + grid_x, GRAPH_MARGIN_TOP + graph_height);
	QRectF bounding_rect = QRectF(text_anchor.x(), text_anchor.y(), delta_x - 3, GRAPH_MARGIN_BOTTOM - 10);
	viewport->draw_text(this->labels_font, this->labels_pen, bounding_rect, Qt::AlignLeft | Qt::AlignTop, text, SG_TEXT_OFFSET_LEFT);


	viewport->draw_line(viewport->grid_pen,
			    grid_x, 0,
			    grid_x, 0 + graph_height);
}




void TrackProfileDialog::draw_distance_divisions(Viewport * viewport, DistanceUnit distance_unit)
{
	/* Set to display units from length in metres. */
	double length = this->track_length_inc_gaps;
	length = convert_distance_meters_to(length, distance_unit);

	unsigned int index = get_distance_chunk_index(length);
	unsigned int graph_width = viewport->width() - GRAPH_MARGIN_LEFT - GRAPH_MARGIN_RIGHT;
	double dist_per_pixel = length / graph_width;

	for (unsigned int i = 1; chunksd[index] * i <= length; i++) {
		double distance_value = chunksd[index] * i;
		unsigned int grid_x = (unsigned int) (chunksd[index] * i / dist_per_pixel);
		this->draw_vertical_grid_distance(viewport, index, grid_x, distance_value, distance_unit);
	}
}




/**
 * Draw the elevation-distance image.
 */
void TrackProfileDialog::draw_ed(Viewport * viewport, Track * trk_)
{
	/* Free previous allocation. */
	if (this->altitudes) {
		free(this->altitudes);
	}

	unsigned int graph_width = viewport->width() - GRAPH_MARGIN_LEFT - GRAPH_MARGIN_RIGHT;
	unsigned int graph_bottom = viewport->height() - GRAPH_MARGIN_BOTTOM;
	unsigned int graph_height = viewport->height() - GRAPH_MARGIN_TOP - GRAPH_MARGIN_BOTTOM;

	this->altitudes = trk_->make_elevation_map(graph_width);
	if (this->altitudes == NULL) {
		return;
	}

	/* Convert into appropriate units. */
	HeightUnit height_units = Preferences::get_unit_height();
	if (height_units == HeightUnit::FEET) {
		/* Convert altitudes into feet units. */
		for (unsigned int i = 0; i < graph_width; i++) {
			this->altitudes[i] = VIK_METERS_TO_FEET(this->altitudes[i]);
		}
	}
	/* Otherwise leave in metres. */

	minmax_array(this->altitudes, &this->min_altitude, &this->max_altitude, true, graph_width);

	get_new_min_and_chunk_index(this->min_altitude, this->max_altitude, chunksa, G_N_ELEMENTS(chunksa), &this->draw_min_altitude, &this->cia);

	/* Assign locally. */
	double mina = this->draw_min_altitude;

	/* Reset before redrawing. */
	viewport->clear();


	/* Draw values of 'elevation = f(distance)' function. */
	QPen no_alt_info_pen(QColor("yellow"));
	for (unsigned int i = 0; i < graph_width; i++) {
		if (this->altitudes[i] == VIK_DEFAULT_ALTITUDE) {
			viewport->draw_line(no_alt_info_pen,
					    i, 0,
					    i, 0 + graph_height);
		} else {
			viewport->draw_line(this->main_pen,
					    i, graph_height,
					    i, graph_height - graph_height * (this->altitudes[i] - mina) / (chunksa[this->cia] * LINES));
		}
	}


	/* Draw grid on top of graph of values. */
	for (int i = 0; i <= LINES; i++) {
		char s[32];

		switch (height_units) {
		case HeightUnit::METRES:
			sprintf(s, "%8dm", (int)(mina + (LINES - i) * chunksa[this->cia]));
			break;
		case HeightUnit::FEET:
			/* NB values already converted into feet. */
			sprintf(s, "%8dft", (int)(mina + (LINES - i) * chunksa[this->cia]));
			break;
		default:
			sprintf(s, "--");
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. height=%d\n", (int) height_units);
		}

		this->draw_horizontal_grid(viewport, s, i);
	}
	this->draw_distance_divisions(viewport, Preferences::get_unit_distance());


	if (this->w_ed_show_dem->checkState()
	    || this->w_ed_show_gps_speed->checkState()) {

		QPen dem_alt_pen(QColor("green"));
		QPen gps_speed_pen(QColor("red"));

		/* Ensure somekind of max speed when not set. */
		if (this->max_speed < 0.01) {
			this->max_speed = trk_->get_max_speed();
		}

		draw_dem_alt_speed_dist(trk_,
					viewport,
					dem_alt_pen,
					gps_speed_pen,
					mina,
					this->max_speed,
					this->cia,
					graph_width,
					graph_height,
					graph_bottom,
					GRAPH_MARGIN_LEFT,
					this->w_ed_show_dem->checkState(),
					this->w_ed_show_gps_speed->checkState());
	}


	viewport->draw_border();
	viewport->update();

	/* The pixmap = margin + graph area. */
	qDebug() << "II: Track Profile: saving viewport" << viewport->type_string;
	this->saved_img_ed.img = *viewport->get_pixmap();
	this->saved_img_ed.valid = true;
}




/**
 * Draws representative speed on the supplied pixmap
 * (which is the gradients graph).
 */
static void draw_speed_dist(Track * trk_,
			    Viewport * viewport,
			    QPen & speed_pen,
			    double max_speed_in,
			    unsigned int graph_width,
			    unsigned int graph_height,
			    unsigned int graph_bottom,
			    bool do_speed)
{
	double max_speed = 0;
	double total_length = trk_->get_length_including_gaps();

	/* Calculate the max speed factor. */
	if (do_speed) {
		max_speed = max_speed_in * 110 / 100;
	}

	double dist = 0;
	for (auto iter = std::next(trk_->trackpoints.begin()); iter != trk_->trackpoints.end(); iter++) {
		dist += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);
		int x = (graph_width * dist) / total_length + GRAPH_MARGIN_LEFT;
		if (do_speed) {
			/* This is just a speed indicator - no actual values can be inferred by user. */
			if (!std::isnan((*iter)->speed)) {
				int y_speed = graph_bottom - (graph_height * (*iter)->speed) / max_speed;
				viewport->fill_rectangle(speed_pen.color(), x - 2, y_speed - 2, 4, 4);
			}
		}
	}
}




/**
 * Draw the gradient-distance image.
 */
void TrackProfileDialog::draw_gd(Viewport * viewport, Track * trk_)
{
	/* Free previous allocation. */
	if (this->gradients) {
		free(this->gradients);
	}

	unsigned int graph_width = viewport->width() - GRAPH_MARGIN_LEFT - GRAPH_MARGIN_RIGHT;
	unsigned int graph_bottom = viewport->height() - GRAPH_MARGIN_BOTTOM;
	unsigned int graph_height = viewport->height() - GRAPH_MARGIN_TOP - GRAPH_MARGIN_BOTTOM;

	this->gradients = trk_->make_gradient_map(graph_width);

	if (this->gradients == NULL) {
		return;
	}

	minmax_array(this->gradients, &this->min_gradient, &this->max_gradient, true, graph_width);

	get_new_min_and_chunk_index(this->min_gradient, this->max_gradient, chunksg, G_N_ELEMENTS(chunksg), &this->draw_min_gradient, &this->cig);

	/* Assign locally. */
	double mina = this->draw_min_gradient;

	/* Reset before redrawing. */
	viewport->clear();


	/* Draw values of 'gradient = f(distance)' function. */
	for (unsigned int i = 0; i < graph_width; i++) {
		viewport->draw_line(this->main_pen,
				    i, graph_height,
				    i, graph_height - graph_height * (this->gradients[i] - mina) / (chunksg[this->cig] * LINES));
	}


	/* Draw grid on top of graph of values. */
	for (int i = 0; i <= LINES; i++) {
		char s[32];
		sprintf(s, "%8d%%", (int)(mina + (LINES - i)*chunksg[this->cig]));
		this->draw_horizontal_grid(viewport, s, i);
	}
	this->draw_distance_divisions(viewport, Preferences::get_unit_distance());


	if (this->w_gd_show_gps_speed->checkState()) {

		QPen gps_speed_pen(QColor("red"));

		/* Ensure somekind of max speed when not set. */
		if (this->max_speed < 0.01) {
			this->max_speed = trk_->get_max_speed();
		}

		draw_speed_dist(trk_,
				viewport,
				gps_speed_pen,
				this->max_speed,
				graph_width,
				graph_height,
				graph_bottom,
				this->w_gd_show_gps_speed->checkState());
	}


	viewport->draw_border();
	viewport->update();

	/* The pixmap = margin + graph area. */
	qDebug() << "II: Track Profile: saving viewport" << viewport->type_string;
	this->saved_img_gd.img = *viewport->get_pixmap();
	this->saved_img_gd.valid = true;
}




void TrackProfileDialog::draw_time_lines(Viewport * viewport)
{
	unsigned int index = get_time_chunk_index(this->duration);
	unsigned int graph_width = viewport->width() - GRAPH_MARGIN_LEFT - GRAPH_MARGIN_RIGHT;
	double time_per_pixel = (double)(this->duration) / graph_width;

	/* If stupidly long track in time - don't bother trying to draw grid lines. */
	if (this->duration > chunkst[G_N_ELEMENTS(chunkst)-1] * LINES * LINES) {
		return;
	}

	for (unsigned int i = 1; chunkst[index] * i <= this->duration; i++) {
		unsigned int grid_x = (unsigned int) (chunkst[index] * i / time_per_pixel);
		unsigned int time_value = chunkst[index] * i;
		this->draw_vertical_grid_time(viewport, index, grid_x, time_value);
	}
}




/**
 * Draw the speed/time image.
 */
void TrackProfileDialog::draw_st(Viewport * viewport, Track * trk_)
{
	/* Free previous allocation. */
	if (this->speeds) {
		free(this->speeds);
	}

	unsigned int graph_width = viewport->width() - GRAPH_MARGIN_LEFT - GRAPH_MARGIN_RIGHT;
	unsigned int graph_bottom = viewport->height() - GRAPH_MARGIN_BOTTOM;
	unsigned int graph_height = viewport->height() - GRAPH_MARGIN_TOP - GRAPH_MARGIN_BOTTOM;
	unsigned int graph_left = GRAPH_MARGIN_LEFT;

	this->speeds = trk_->make_speed_map(graph_width);
	if (this->speeds == NULL) {
		return;
	}

	this->duration = trk_->get_duration(true);
	/* Negative time or other problem. */
	if (this->duration <= 0) {
		return;
	}

	/* Convert into appropriate units. */
	SpeedUnit speed_units = Preferences::get_unit_speed();
	for (unsigned int i = 0; i < graph_width; i++) {
		this->speeds[i] = convert_speed_mps_to(this->speeds[i], speed_units);
	}

	minmax_array(this->speeds, &this->min_speed, &this->max_speed, false, graph_width);
	if (this->min_speed < 0.0) {
		this->min_speed = 0; /* Splines sometimes give negative speeds. */
	}

	/* Find suitable chunk index. */
	get_new_min_and_chunk_index(this->min_speed, this->max_speed, chunkss, G_N_ELEMENTS(chunkss), &this->draw_min_speed, &this->cis);

	/* Assign locally. */
	double mins = this->draw_min_speed;

	/* Reset before redrawing. */
	viewport->clear();


	/* Draw values of 'speed = f(time)' function. */
	for (unsigned int i = 0; i < graph_width; i++) {
		viewport->draw_line(this->main_pen,
				    i, graph_height,
				    i, graph_height - graph_height * (this->speeds[i] - mins) / (chunkss[this->cis] * LINES));
	}


	/* Draw grid on top of graph of values. */
	for (int i = 0; i <= LINES; i++) {
		char s[32];

		/* NB: No need to convert here anymore as numbers are in the appropriate units. */
		switch (speed_units) {
		case SpeedUnit::KILOMETRES_PER_HOUR:
			sprintf(s, "%8dkm/h", (int)(mins + (LINES - i) * chunkss[this->cis]));
			break;
		case SpeedUnit::MILES_PER_HOUR:
			sprintf(s, "%8dmph", (int)(mins + (LINES - i) * chunkss[this->cis]));
			break;
		case SpeedUnit::METRES_PER_SECOND:
			sprintf(s, "%8dm/s", (int)(mins + (LINES - i) * chunkss[this->cis]));
			break;
		case SpeedUnit::KNOTS:
			sprintf(s, "%8dknots", (int)(mins + (LINES - i) * chunkss[this->cis]));
			break;
		default:
			sprintf(s, "--");
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. speed=%d\n", (int) speed_units);
		}

		this->draw_horizontal_grid(viewport, s, i);
	}
	this->draw_time_lines(viewport);


	if (this->w_st_show_gps_speed->checkState()) {

		QPen gps_speed_pen(QColor("red"));

		time_t beg_time = (*trk_->trackpoints.begin())->timestamp;
		time_t dur = (*std::prev(trk_->trackpoints.end()))->timestamp - beg_time;

		for (auto iter = trk_->trackpoints.begin(); iter != trk_->trackpoints.end(); iter++) {
			double gps_speed = (*iter)->speed;
			if (std::isnan(gps_speed)) {
				continue;
			}

			gps_speed = convert_speed_mps_to(gps_speed, speed_units);

			int pos_x = graph_left + graph_width * ((*iter)->timestamp - beg_time) / dur;
			int pos_y = graph_bottom - graph_height * (gps_speed - mins) / (chunkss[this->cis] * LINES);
			viewport->fill_rectangle(QColor("red"), pos_x - 2, pos_y - 2, 4, 4);
		}
	}


	viewport->draw_border();
	viewport->update();

	/* The pixmap = margin + graph area. */
	qDebug() << "II: Track Profile: saving viewport" << viewport->type_string;
	this->saved_img_st.img = *viewport->get_pixmap();
	this->saved_img_st.valid = true;
}




/**
 * Draw the distance-time image.
 */
void TrackProfileDialog::draw_dt(Viewport * viewport, Track * trk_)
{
	/* Free previous allocation. */
	if (this->distances) {
		free(this->distances);
	}

	unsigned int graph_width = viewport->width() - GRAPH_MARGIN_LEFT - GRAPH_MARGIN_RIGHT;
	unsigned int graph_bottom = viewport->height() - GRAPH_MARGIN_BOTTOM;
	unsigned int graph_height = viewport->height() - GRAPH_MARGIN_TOP - GRAPH_MARGIN_BOTTOM;
	unsigned int graph_left = GRAPH_MARGIN_LEFT;

	this->distances = trk_->make_distance_map(graph_width);
	if (this->distances == NULL) {
		return;
	}

	/* Convert into appropriate units. */
	DistanceUnit distance_unit = Preferences::get_unit_distance();
	for (unsigned int i = 0; i < graph_width; i++) {
		this->distances[i] = convert_distance_meters_to(this->distances[i], distance_unit);
	}

	this->duration = this->trk->get_duration(true);
	/* Negative time or other problem. */
	if (this->duration <= 0) {
		return;
	}

	/* Easy to work out min / max of distance!
	   Assign locally.
	   mind = 0.0; - Thus not used. */
	double maxd = convert_distance_meters_to(trk->get_length_including_gaps(), distance_unit);

	/* Find suitable chunk index. */
	double dummy = 0.0; /* Expect this to remain the same! (not that it's used). */
	get_new_min_and_chunk_index(0, maxd, chunksd, G_N_ELEMENTS(chunksd), &dummy, &this->cid);

	/* Reset before redrawing. */
	viewport->clear();


	/* Draw values of 'distance = f(time)' function. */
	for (unsigned int i = 0; i < graph_width; i++) {
		viewport->draw_line(this->main_pen,
				    i, graph_height,
				    i, graph_height - graph_height * (this->distances[i]) / (chunksd[this->cid] * LINES));
	}


	/* Draw grid on top of graph of values.. */
	for (int i = 0; i <= LINES; i++) {
		char s[32];

		switch (distance_unit) {
		case DistanceUnit::MILES:
			sprintf(s, _("%.1f miles"), ((LINES - i) * chunksd[this->cid]));
			break;
		case DistanceUnit::NAUTICAL_MILES:
			sprintf(s, _("%.1f NM"), ((LINES - i) * chunksd[this->cid]));
			break;
		default:
			sprintf(s, _("%.1f km"), ((LINES - i) * chunksd[this->cid]));
			break;
		}

		this->draw_horizontal_grid(viewport, s, i);
	}
	this->draw_time_lines(viewport);


	/* Show speed indicator. */
	if (this->w_dt_show_speed->checkState()) {
		QPen dist_speed_gc(QColor("red"));

		double max_speed_ = this->max_speed * 110 / 100;

		/* This is just an indicator - no actual values can be inferred by user. */
		for (unsigned int i = 0; i < graph_width; i++) {
			int y_speed = graph_bottom - (graph_height * this->speeds[i]) / max_speed_;
			viewport->fill_rectangle(QColor("red"), graph_left + i - 2, y_speed - 2, 4, 4);
		}
	}


	viewport->draw_border();
	viewport->update();

	/* The pixmap = margin + graph area. */
	qDebug() << "II: Track Profile: saving viewport" << viewport->type_string;
	this->saved_img_dt.img = *viewport->get_pixmap();
	this->saved_img_dt.valid = true;
}




/**
 * Draw the elevation-time image.
 */
void TrackProfileDialog::draw_et(Viewport * viewport, Track * trk_)
{
	unsigned int graph_width = viewport->width() - GRAPH_MARGIN_LEFT - GRAPH_MARGIN_RIGHT;
	unsigned int graph_bottom = viewport->height() - GRAPH_MARGIN_BOTTOM;
	unsigned int graph_height = viewport->height() - GRAPH_MARGIN_TOP - GRAPH_MARGIN_BOTTOM;
	unsigned int graph_left = GRAPH_MARGIN_LEFT;

	/* Free previous allocation. */
	if (this->ats) {
		free(this->ats);
	}

	this->ats = trk_->make_elevation_time_map(graph_width);
	if (this->ats == NULL) {
		return;
	}

	/* Convert into appropriate units. */
	HeightUnit height_units = Preferences::get_unit_height();
	if (height_units == HeightUnit::FEET) {
		/* Convert altitudes into feet units. */
		for (unsigned int i = 0; i < graph_width; i++) {
			this->ats[i] = VIK_METERS_TO_FEET(this->ats[i]);
		}
	}
	/* Otherwise leave in metres. */

	minmax_array(this->ats, &this->min_altitude, &this->max_altitude, true, graph_width);

	get_new_min_and_chunk_index(this->min_altitude, this->max_altitude, chunksa, G_N_ELEMENTS(chunksa), &this->draw_min_altitude_time, &this->ciat);

	/* Assign locally. */
	double mina = this->draw_min_altitude_time;

	this->duration = this->trk->get_duration(true);
	/* Negative time or other problem. */
	if (this->duration <= 0) {
		return;
	}

	/* Reset before redrawing. */
	viewport->clear();


	/* Draw values of 'elevation = f(time)' function. */
	for (unsigned int i = 0; i < graph_width; i++) {
		viewport->draw_line(this->main_pen,
				    i, graph_height,
				    i, graph_height - graph_height * (this->ats[i] - mina) / (chunksa[this->ciat] * LINES));
	}


	/* Draw grid on top of graph of values. */
	for (int i = 0; i <= LINES; i++) {
		char s[32];

		switch (height_units) {
		case HeightUnit::METRES:
			sprintf(s, "%8dm", (int)(mina + (LINES - i) * chunksa[this->ciat]));
			break;
		case HeightUnit::FEET:
			/* NB values already converted into feet. */
			sprintf(s, "%8dft", (int)(mina + (LINES - i) * chunksa[this->ciat]));
			break;
		default:
			sprintf(s, "--");
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. height=%d\n", (int) height_units);
		}

		this->draw_horizontal_grid(viewport, s, i);
	}
	this->draw_time_lines(viewport);


	/* Show DEMS. */
	if (this->w_et_show_dem->checkState())  {
		QPen dem_alt_pen(QColor("green"));

		int achunk = chunksa[this->ciat] * LINES;

		for (unsigned i = 0; i < graph_width; i++) {
			/* This could be slow doing this each time... */
			Trackpoint * tp = this->trk->get_closest_tp_by_percentage_time(((double) i / (double) graph_width), NULL);
			if (tp) {
				int16_t elev = DEMCache::get_elev_by_coord(&tp->coord, DemInterpolation::SIMPLE);
				if (elev != DEM_INVALID_ELEVATION) {
					/* Convert into height units. */
					if (Preferences::get_unit_height() == HeightUnit::FEET) {
						elev = VIK_METERS_TO_FEET(elev);
					}
					/* No conversion needed if already in metres. */

					/* offset is in current height units. */
					elev -= mina;

					/* Consider chunk size. */
					int y_alt = graph_bottom - ((graph_height * elev)/achunk);
					viewport->fill_rectangle(dem_alt_pen.color(), graph_left + i - 2, y_alt - 2, 4, 4);
				}
			}
		}
	}


	/* Show speeds. */
	if (this->w_et_show_speed->checkState()) {
		/* This is just an indicator - no actual values can be inferred by user. */
		QPen elev_speed_pen(QColor("red"));

		double max_speed_ = this->max_speed * 110 / 100;

		for (unsigned int i = 0; i < graph_width; i++) {
			int y_speed = graph_bottom - (graph_height * this->speeds[i]) / max_speed_;
			viewport->fill_rectangle(elev_speed_pen.color(), graph_left + i - 2, y_speed - 2, 4, 4);
		}
	}


	viewport->draw_border();
	viewport->update();

	/* The pixmap = margin + graph area. */
	qDebug() << "II: Track Profile: saving viewport" << viewport->type_string;
	this->saved_img_et.img = *viewport->get_pixmap();
	this->saved_img_et.valid = true;
}




/**
 * Draw the speed-distance image.
 */
void TrackProfileDialog::draw_sd(Viewport * viewport, Track * trk_)
{
	/* Free previous allocation. */
	if (this->speeds_dist) {
		free(this->speeds_dist);
	}

	unsigned int graph_width = viewport->width() - GRAPH_MARGIN_LEFT - GRAPH_MARGIN_RIGHT;
	unsigned int graph_bottom = viewport->height() - GRAPH_MARGIN_BOTTOM;
	unsigned int graph_height = viewport->height() - GRAPH_MARGIN_TOP - GRAPH_MARGIN_BOTTOM;
	unsigned int graph_left = GRAPH_MARGIN_LEFT;

	this->speeds_dist = trk_->make_speed_dist_map(graph_width);
	if (this->speeds_dist == NULL) {
		return;
	}

	/* Convert into appropriate units. */
	SpeedUnit speed_units = Preferences::get_unit_speed();
	for (unsigned int i = 0; i < graph_width; i++) {
		this->speeds_dist[i] = convert_speed_mps_to(this->speeds_dist[i], speed_units);
	}

	/* OK to reuse min_speed here. */
	minmax_array(this->speeds_dist, &this->min_speed, &this->max_speed_dist, false, graph_width);
	if (this->min_speed < 0.0) {
		this->min_speed = 0; /* Splines sometimes give negative speeds. */
	}

	/* Find suitable chunk index. */
	get_new_min_and_chunk_index(this->min_speed, this->max_speed_dist, chunkss, G_N_ELEMENTS(chunkss), &this->draw_min_speed, &this->cisd);

	/* Assign locally. */
	double mins = this->draw_min_speed;

	/* Reset before redrawing. */
	viewport->clear();


	/* Draw values of 'speed = f(distance)' function. */
	for (unsigned int i = 0; i < graph_width; i++) {
		viewport->draw_line(this->main_pen,
				    i, graph_height,
				    i, graph_height - graph_height * (this->speeds_dist[i] - mins) / (chunkss[this->cisd] * LINES));
	}


	/* Draw grid on top of graph of values. */
	for (int i = 0; i <= LINES; i++) {
		char s[32];

		/* NB: No need to convert here anymore as numbers are in the appropriate units. */
		switch (speed_units) {
		case SpeedUnit::KILOMETRES_PER_HOUR:
			sprintf(s, "%8dkm/h", (int)(mins + (LINES - i)*chunkss[this->cisd]));
			break;
		case SpeedUnit::MILES_PER_HOUR:
			sprintf(s, "%8dmph", (int)(mins + (LINES - i)*chunkss[this->cisd]));
			break;
		case SpeedUnit::METRES_PER_SECOND:
			sprintf(s, "%8dm/s", (int)(mins + (LINES - i)*chunkss[this->cisd]));
			break;
		case SpeedUnit::KNOTS:
			sprintf(s, "%8dknots", (int)(mins + (LINES - i)*chunkss[this->cisd]));
			break;
		default:
			sprintf(s, "--");
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. speed=%d\n", (int) speed_units);
		}

		this->draw_horizontal_grid(viewport, s, i);
	}
	this->draw_distance_divisions(viewport, Preferences::get_unit_distance());


	if (this->w_sd_show_gps_speed->checkState()) {

		QPen gps_speed_pen(QColor("red"));

		double dist = trk_->get_length_including_gaps();
		double dist_tp = 0.0;

		for (auto iter = std::next(trk_->trackpoints.begin()); iter != trk_->trackpoints.end(); iter++) {
			double gps_speed = (*iter)->speed;
			if (std::isnan(gps_speed)) {
				continue;
			}

			gps_speed = convert_speed_mps_to(gps_speed, speed_units);

			dist_tp += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);
			int pos_x = graph_left + (graph_width * dist_tp / dist);
			int pos_y = graph_bottom - graph_height * (gps_speed - mins)/(chunkss[this->cisd] * LINES);
			viewport->fill_rectangle(gps_speed_pen.color(), pos_x - 2, pos_y - 2, 4, 4);
		}
	}


	viewport->draw_border();
	viewport->update();

	/* The pixmap = margin + graph area. */
	qDebug() << "II: Track Profile: saving viewport" << viewport->type_string;
	this->saved_img_sd.img = *viewport->get_pixmap();
	this->saved_img_sd.valid = true;
}




/**
 * Draw all graphs.
 */
void TrackProfileDialog::draw_all_graphs(bool resized)
{
	/* ed = elevation-distance. */
	if (this->viewport_ed) {
		this->draw_single_graph(this->viewport_ed, resized, &TrackProfileDialog::draw_ed, &TrackProfileDialog::get_pos_y_ed, false, &this->saved_img_ed);
	}

	/* gd = gradient-distance. */
	if (this->viewport_gd) {
		this->draw_single_graph(this->viewport_gd, resized, &TrackProfileDialog::draw_gd, &TrackProfileDialog::get_pos_y_gd, false, &this->saved_img_gd);
	}

	/* st = speed-time. */
	if (this->viewport_st) {
		this->draw_single_graph(this->viewport_st, resized, &TrackProfileDialog::draw_st, &TrackProfileDialog::get_pos_y_st, true, &this->saved_img_st);
	}

	/* dt = distance-time. */
	if (this->viewport_dt) {
		this->draw_single_graph(this->viewport_dt, resized, &TrackProfileDialog::draw_dt, &TrackProfileDialog::get_pos_y_dt, true, &this->saved_img_dt);
	}

	/* et = elevation-time. */
	if (this->viewport_et) {
		this->draw_single_graph(this->viewport_et, resized, &TrackProfileDialog::draw_et, &TrackProfileDialog::get_pos_y_et, true, &this->saved_img_et);
	}

	/* sd = speed-distance. */
	if (this->viewport_sd) {
		this->draw_single_graph(this->viewport_sd, resized, &TrackProfileDialog::draw_sd, &TrackProfileDialog::get_pos_y_sd, true, &this->saved_img_sd);
	}
}




void TrackProfileDialog::draw_single_graph(Viewport * viewport, bool resized, void (TrackProfileDialog::*draw_graph)(Viewport *, Track *), double (TrackProfileDialog::*get_pos_y)(double, int, int), bool by_time, PropSaved * saved_img)
{
	unsigned int graph_width = viewport->width() - GRAPH_MARGIN_LEFT - GRAPH_MARGIN_RIGHT;
	unsigned int graph_height = viewport->height() - GRAPH_MARGIN_TOP - GRAPH_MARGIN_BOTTOM;

	/* Saved image no longer any good as we've resized, so we invalidate it here. */
	if (resized) {
		saved_img->valid = false;
	}

	(this->*draw_graph)(viewport, this->trk);

	/* Ensure markers are redrawn if necessary. */
	if (this->is_selected_drawn || this->is_current_drawn) {

		double pc = NAN;
		int current_pos_x = -1; /* i.e. don't draw unless we get a valid value. */
		double current_pos_y = 0;
		if (this->is_current_drawn) {
			pc = NAN;
			if (by_time) {
				pc = tp_percentage_by_time(this->trk, this->current_tp);
			} else {
				pc = tp_percentage_by_distance(this->trk, this->current_tp, this->track_length_inc_gaps);
			}
			if (!std::isnan(pc)) {
				current_pos_x = pc * graph_width;
				current_pos_y = (this->*get_pos_y)(current_pos_x, graph_width, graph_height);
			}
		}

		double selected_pos_x = -1.0; /* i.e. Don't draw unless we get a valid value. */
		double selected_pos_y = -1.0;
		if (by_time) {
			pc = tp_percentage_by_time(this->trk, this->selected_tp);
		} else {
			pc = tp_percentage_by_distance(this->trk, this->selected_tp, this->track_length_inc_gaps);
		}
		if (!std::isnan(pc)) {
			selected_pos_x = pc * graph_width;
			selected_pos_y = (this->*get_pos_y)(selected_pos_x, graph_width, graph_height);
		}

		this->draw_marks(viewport,
				 selected_pos_x,
				 selected_pos_y,
				 current_pos_x,
				 current_pos_y,
				 saved_img,
				 graph_width,
				 graph_height);
	}
}




/**
 * Configure/Resize the profile & speed/time images.
 */
bool TrackProfileDialog::configure_event_cb(Viewport * viewport)
{
	qDebug() << "SLOT: Track Profile: received \"reconfigured\" signal from" << viewport->type_string;

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
 * Create elevation-distance viewport.
 */
Viewport * TrackProfileDialog::create_ed_viewport(double * min_alt, double * max_alt)
{
	const int initial_width = GRAPH_MARGIN_LEFT + GRAPH_INITIAL_WIDTH + GRAPH_MARGIN_RIGHT;
	const int initial_height = GRAPH_MARGIN_TOP + GRAPH_INITIAL_HEIGHT + GRAPH_MARGIN_BOTTOM;

	/* First allocation. */
	this->altitudes = this->trk->make_elevation_map(initial_width);

	if (this->altitudes == NULL) {
		*min_alt = *max_alt = VIK_DEFAULT_ALTITUDE;
		return NULL;
	}

	Viewport * viewport = new Viewport(this->parent);
	strcpy(viewport->type_string, "Viewport, elevation-distance");
	viewport->set_margin(GRAPH_MARGIN_TOP, GRAPH_MARGIN_BOTTOM, GRAPH_MARGIN_LEFT, GRAPH_MARGIN_RIGHT);
	viewport->resize(initial_width, initial_height);
	viewport->reconfigure_drawing_area(initial_width, initial_height);

	connect(viewport, SIGNAL (button_released(Viewport *, QMouseEvent *)), this, SLOT (track_ed_release_cb(Viewport *, QMouseEvent *)));
	connect(viewport, SIGNAL (cursor_moved(Viewport *, QMouseEvent *)), this, SLOT (track_ed_move_cb(Viewport *, QMouseEvent *)));

	/* TODO: move it outside of this function. */
	unsigned int graph_width = viewport->width() - GRAPH_MARGIN_LEFT - GRAPH_MARGIN_RIGHT;
	minmax_array(this->altitudes, min_alt, max_alt, true, graph_width);

	return viewport;
}




/**
 * Create gradient-distance viewport.
 */
Viewport * TrackProfileDialog::create_gd_viewport(void)
{
	const int initial_width = GRAPH_MARGIN_LEFT + GRAPH_INITIAL_WIDTH + GRAPH_MARGIN_RIGHT;
	const int initial_height = GRAPH_MARGIN_TOP + GRAPH_INITIAL_HEIGHT + GRAPH_MARGIN_BOTTOM;

	/* First allocation. */
	this->gradients = this->trk->make_gradient_map(initial_width);

	if (this->gradients == NULL) {
		return NULL;
	}

	Viewport * viewport = new Viewport(this->parent);
	strcpy(viewport->type_string, "Viewport, gradient-distance");
	viewport->set_margin(GRAPH_MARGIN_TOP, GRAPH_MARGIN_BOTTOM, GRAPH_MARGIN_LEFT, GRAPH_MARGIN_RIGHT);
	viewport->resize(initial_width, initial_height);
	viewport->reconfigure_drawing_area(initial_width, initial_height);

	connect(viewport, SIGNAL (button_released(Viewport *, QMouseEvent *)), this, SLOT (track_gd_release_cb(Viewport *, QMouseEvent *)));
	connect(viewport, SIGNAL (cursor_moved(Viewport *, QMouseEvent *)), this, SLOT (track_gd_move_cb(Viewport *, QMouseEvent *)));

	return viewport;
}




/**
 * Create speed-time viewport.
 */
Viewport * TrackProfileDialog::create_st_viewport(void)
{
	const int initial_width = GRAPH_MARGIN_LEFT + GRAPH_INITIAL_WIDTH + GRAPH_MARGIN_RIGHT;
	const int initial_height = GRAPH_MARGIN_TOP + GRAPH_INITIAL_HEIGHT + GRAPH_MARGIN_BOTTOM;

	/* First allocation. */
	this->speeds = this->trk->make_speed_map(initial_width);
	if (this->speeds == NULL) {
		return NULL;
	}

#if 0
	/* XXX this can go out, it's just a helpful dev tool. */
	{
		GdkGC **colors[8] = { gtk_widget_get_style(window)->bg_gc,
				      gtk_widget_get_style(window)->fg_gc,
				      gtk_widget_get_style(window)->light_gc,
				      gtk_widget_get_style(window)->dark_gc,
				      gtk_widget_get_style(window)->mid_gc,
				      gtk_widget_get_style(window)->text_gc,
				      gtk_widget_get_style(window)->base_gc,
				      gtk_widget_get_style(window)->text_aa_gc };
		for (i = 0; i < 5; i++) {
			for (int j = 0; j < 8; j++) {
				fill_rectangle(GDK_DRAWABLE(pix), colors[j][i],
					       i*20, j*20, 20, 20);
				draw_rectangle(GDK_DRAWABLE(pix), black_pen,
					       i*20, j*20, 20, 20);
			}
		}
	}
#endif

	Viewport * viewport = new Viewport(this->parent);
	strcpy(viewport->type_string, "Viewport, speed-time");
	viewport->set_margin(GRAPH_MARGIN_TOP, GRAPH_MARGIN_BOTTOM, GRAPH_MARGIN_LEFT, GRAPH_MARGIN_RIGHT);
	viewport->resize(initial_width, initial_height);
	viewport->reconfigure_drawing_area(initial_width, initial_height);

	connect(viewport, SIGNAL (button_released(Viewport *, QMouseEvent *)), this, SLOT (track_st_release_cb(Viewport *, QMouseEvent *)));
	connect(viewport, SIGNAL (cursor_moved(Viewport *, QMouseEvent *)), this, SLOT (track_st_move_cb(Viewport *, QMouseEvent *)));

	return viewport;
}




/**
 * Create distance-time viewport.
 */
Viewport * TrackProfileDialog::create_dt_viewport(void)
{
	const int initial_width = GRAPH_MARGIN_LEFT + GRAPH_INITIAL_WIDTH + GRAPH_MARGIN_RIGHT;
	const int initial_height = GRAPH_MARGIN_TOP + GRAPH_INITIAL_HEIGHT + GRAPH_MARGIN_BOTTOM;

	/* First allocation. */
	this->distances = this->trk->make_distance_map(initial_width);
	if (this->distances == NULL) {
		return NULL;
	}

	Viewport * viewport = new Viewport(this->parent);
	strcpy(viewport->type_string, "Viewport, distance-time");
	viewport->set_margin(GRAPH_MARGIN_TOP, GRAPH_MARGIN_BOTTOM, GRAPH_MARGIN_LEFT, GRAPH_MARGIN_RIGHT);
	viewport->resize(initial_width, initial_height);
	viewport->reconfigure_drawing_area(initial_width, initial_height);

	connect(viewport, SIGNAL (button_released(Viewport *, QMouseEvent *)), this, SLOT (track_dt_release_cb(Viewport *, QMouseEvent *)));
	connect(viewport, SIGNAL (cursor_moved(Viewport *, QMouseEvent *)), this, SLOT (track_dt_move_cb(Viewport *, QMouseEvent *)));
	//QObject::connect(widget, SIGNAL("destroy"), this, SLOT (g_free));

	return viewport;
}




/**
 * Create elevation-time viewport.
 */
Viewport * TrackProfileDialog::create_et_viewport(void)
{
	const int initial_width = GRAPH_MARGIN_LEFT + GRAPH_INITIAL_WIDTH + GRAPH_MARGIN_RIGHT;
	const int initial_height = GRAPH_MARGIN_TOP + GRAPH_INITIAL_HEIGHT + GRAPH_MARGIN_BOTTOM;

	/* First allocation. */
	this->ats = this->trk->make_elevation_time_map(initial_width);
	if (this->ats == NULL) {
		return NULL;
	}

	Viewport * viewport = new Viewport(this->parent);
	strcpy(viewport->type_string, "Viewport, elevation-time");
	viewport->set_margin(GRAPH_MARGIN_TOP, GRAPH_MARGIN_BOTTOM, GRAPH_MARGIN_LEFT, GRAPH_MARGIN_RIGHT);
	viewport->resize(initial_width, initial_height);
	viewport->reconfigure_drawing_area(initial_width, initial_height);

	connect(viewport, SIGNAL (button_released(Viewport *, QMouseEvent *)), this, SLOT (track_et_release_cb(Viewport *, QMouseEvent *)));
	connect(viewport, SIGNAL (cursor_moved(Viewport *, QMouseEvent *)), this, SLOT (track_et_move_cb(Viewport *, QMouseEvent *)));

	return viewport;
}




/**
 * Create speed-distance viewport.
 */
Viewport * TrackProfileDialog::create_sd_viewport(void)
{
	const int initial_width = GRAPH_MARGIN_LEFT + GRAPH_INITIAL_WIDTH + GRAPH_MARGIN_RIGHT;
	const int initial_height = GRAPH_MARGIN_TOP + GRAPH_INITIAL_HEIGHT + GRAPH_MARGIN_BOTTOM;

	/* First allocation. */
	this->speeds_dist = this->trk->make_speed_dist_map(initial_width); // kamilFIXME
	if (this->speeds_dist == NULL) {
		return NULL;
	}

	Viewport * viewport = new Viewport(this->parent);
	strcpy(viewport->type_string, "Viewport, speed-distance");
	viewport->set_margin(GRAPH_MARGIN_TOP, GRAPH_MARGIN_BOTTOM, GRAPH_MARGIN_LEFT, GRAPH_MARGIN_RIGHT);
	viewport->resize(initial_width, initial_height);
	viewport->reconfigure_drawing_area(initial_width, initial_height);

	connect(viewport, SIGNAL (button_released(Viewport *, QMouseEvent *)), this, SLOT (track_sd_release_cb(Viewport *, QMouseEvent *)));
	connect(viewport, SIGNAL (cursor_moved(Viewport *, QMouseEvent *)), this, SLOT (track_sd_move_cb(Viewport *, QMouseEvent *)));

	return viewport;
}





#define VIK_SETTINGS_TRACK_PROFILE_WIDTH "track_profile_display_width"
#define VIK_SETTINGS_TRACK_PROFILE_HEIGHT "track_profile_display_height"




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
						const char * text1,
						QLabel * value1,
						const char * text2,
						QLabel * value2,
						const char * text3,
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
	TrackProfileDialog dialog(QString("Track Profile"), trk, main_viewport, parent);
	trk->set_profile_dialog(&dialog);
	dialog.exec();
	trk->clear_profile_dialog();
}




TrackProfileDialog::TrackProfileDialog(QString const & title, Track * a_trk, Viewport * main_viewport_, Window * a_parent) : QDialog(a_parent)
{
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


	double min_alt, max_alt;
	this->viewport_ed = this->create_ed_viewport(&min_alt, &max_alt);
	this->viewport_gd = this->create_gd_viewport();
	this->viewport_st = this->create_st_viewport();
	this->viewport_dt = this->create_dt_viewport();
	this->viewport_et = this->create_et_viewport();
	this->viewport_sd = this->create_sd_viewport();
	this->tabs = new QTabWidget();
	this->tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);


	/* NB This value not shown yet - but is used by internal calculations. */
	this->track_length_inc_gaps = trk->get_length_including_gaps();

	if (this->viewport_ed) {
		this->w_ed_current_distance = ui_label_new_selectable(_("No Data"), this);
		this->w_ed_current_elevation = ui_label_new_selectable(_("No Data"), this);
		this->w_ed_show_dem = new QCheckBox(tr("Show D&EM"), this);
		this->w_ed_show_gps_speed = new QCheckBox(tr("Show &GPS Speed"), this);
		QWidget * page = this->create_graph_page(this->viewport_ed,
							 _("Track Distance:"), this->w_ed_current_distance,
							 _("Track Height:"),   this->w_ed_current_elevation,
							 NULL, NULL,
							 this->w_ed_show_dem, show_dem,
							 this->w_ed_show_gps_speed, show_alt_gps_speed);
		connect(this->w_ed_show_dem, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		connect(this->w_ed_show_gps_speed, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		connect(this->viewport_ed, SIGNAL (reconfigured(Viewport *)), this, SLOT(configure_event_cb(Viewport *)));
		this->tabs->addTab(page, tr("Elevation-distance"));
	}

	if (this->viewport_gd) {
		this->w_gd_current_distance = ui_label_new_selectable(_("No Data"), this);
		this->w_gd_current_gradient = ui_label_new_selectable(_("No Data"), this);
		this->w_gd_show_gps_speed = new QCheckBox(tr("Show &GPS Speed"), this);
		QWidget * page = this->create_graph_page(this->viewport_gd,
							 _("Track Distance:"), this->w_gd_current_distance,
							 _("Track Gradient:"), this->w_gd_current_gradient,
							 NULL, NULL,
							 this->w_gd_show_gps_speed, show_gradient_gps_speed,
							 NULL, false);
		connect(this->w_gd_show_gps_speed, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		connect(this->viewport_gd, SIGNAL (reconfigured(Viewport *)), this, SLOT(configure_event_cb(Viewport *)));
		this->tabs->addTab(page, tr("Gradient-distance"));
	}

	if (this->viewport_st) {
		this->w_st_current_time = ui_label_new_selectable(_("No Data"), this);
		this->w_st_current_speed = ui_label_new_selectable(_("No Data"), this);
		this->w_st_current_time_real = ui_label_new_selectable(_("No Data"), this);
		this->w_st_show_gps_speed = new QCheckBox(tr("Show &GPS Speed"), this);
		QWidget * page = this->create_graph_page(this->viewport_st,
							 _("Track Time:"),  this->w_st_current_time,
							 _("Track Speed:"), this->w_st_current_speed,
							 _("Time/Date:"),   this->w_st_current_time_real,
							 this->w_st_show_gps_speed, show_gps_speed,
							 NULL, false);
		connect(this->w_st_show_gps_speed, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		connect(this->viewport_st, SIGNAL (reconfigured(Viewport *)), this, SLOT(configure_event_cb(Viewport *)));
		this->tabs->addTab(page, tr("Speed-time"));
	}

	if (this->viewport_dt) {
		this->w_dt_current_time = ui_label_new_selectable(_("No Data"), this);
		this->w_dt_curent_distance = ui_label_new_selectable(_("No Data"), this);
		this->w_dt_current_time_real = ui_label_new_selectable(_("No Data"), this);
		this->w_dt_show_speed = new QCheckBox(tr("Show S&peed"), this);
		QWidget * page = this->create_graph_page(this->viewport_dt,
							 _("Track Distance:"), this->w_dt_curent_distance,
							 _("Track Time:"), this->w_dt_current_time,
							 _("Time/Date:"), this->w_dt_current_time_real,
							 this->w_dt_show_speed, show_dist_speed,
							 NULL, false);
		connect(this->w_dt_show_speed, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		connect(this->viewport_dt, SIGNAL (reconfigured(Viewport *)), this, SLOT(configure_event_cb(Viewport *)));
		this->tabs->addTab(page, tr("Distance-time"));
	}

	if (this->viewport_et) {
		this->w_et_current_time = ui_label_new_selectable(_("No Data"), this);
		this->w_et_current_elevation = ui_label_new_selectable(_("No Data"), this);
		this->w_et_current_time_real = ui_label_new_selectable(_("No Data"), this);
		this->w_et_show_speed = new QCheckBox(tr("Show S&peed"), this);
		this->w_et_show_dem = new QCheckBox(tr("Show D&EM"), this);
		QWidget * page = this->create_graph_page(this->viewport_et,
							 _("Track Time:"),   this->w_et_current_time,
							 _("Track Height:"), this->w_et_current_elevation,
							 _("Time/Date:"),    this->w_et_current_time_real,
							 this->w_et_show_dem, show_elev_dem,
							 this->w_et_show_speed, show_elev_speed);
		connect(this->w_et_show_dem, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		connect(this->w_et_show_speed, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		connect(this->viewport_et, SIGNAL (reconfigured(Viewport *)), this, SLOT(configure_event_cb(Viewport *)));
		this->tabs->addTab(page, tr("Elevation-time"));
	}

	if (this->viewport_sd) {
		this->w_sd_current_distance = ui_label_new_selectable(_("No Data"), this);
		this->w_sd_current_speed = ui_label_new_selectable(_("No Data"), this);
		this->w_sd_show_gps_speed = new QCheckBox(tr("Show &GPS Speed"), this);
		QWidget * page = this->create_graph_page(this->viewport_sd,
							 _("Track Distance:"), this->w_sd_current_distance,
							 _("Track Speed:"), this->w_sd_current_speed,
							 NULL, NULL,
							 this->w_sd_show_gps_speed, show_sd_gps_speed,
							 NULL, false);
		connect(this->w_sd_show_gps_speed, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		connect(this->viewport_sd, SIGNAL (reconfigured(Viewport *)), this, SLOT(configure_event_cb(Viewport *)));
		this->tabs->addTab(page, tr("Speed-distance"));
	}



	this->button_box = new QDialogButtonBox();


	this->button_cancel = this->button_box->addButton("&Cancel", QDialogButtonBox::RejectRole);
	this->button_split_at_marker = this->button_box->addButton("Split at &Marker", QDialogButtonBox::ActionRole);
	this->button_split_segments = this->button_box->addButton("Split &Segments", QDialogButtonBox::ActionRole);
	this->button_reverse = this->button_box->addButton("&Reverse", QDialogButtonBox::ActionRole);
	this->button_ok = this->button_box->addButton("&OK", QDialogButtonBox::AcceptRole);

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
