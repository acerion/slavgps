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

#include <glib/gi18n.h>
#include <time.h>

#include <QDebug>

#include "slav_qt.h"
#include "layer_trw.h"
#include "viktrwlayer_propwin.h"
#include "dems.h"
#include "viewport.h" /* ugh */
#include "vikutils.h"
#include "ui_util.h"
#include "dialog.h"
#include "settings.h"
#include "globals.h"
#ifdef K
#include "vik_compat.h"
#endif




using namespace SlavGPS;




typedef enum {
	PROPWIN_GRAPH_TYPE_ELEVATION_DISTANCE,
	PROPWIN_GRAPH_TYPE_GRADIENT_DISTANCE,
	PROPWIN_GRAPH_TYPE_SPEED_TIME,
	PROPWIN_GRAPH_TYPE_DISTANCE_TIME,
	PROPWIN_GRAPH_TYPE_ELEVATION_TIME,
	PROPWIN_GRAPH_TYPE_SPEED_DISTANCE,
	PROPWIN_GRAPH_TYPE_END,
} VikPropWinGraphType_t;

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




typedef struct _propwidgets {

} PropWidgets;




static void get_mouse_event_x(Viewport * viewport, GdkEventMotion * event, TrackProfileDialog * widgets, double * x, int * ix);
static void distance_label_update(QLabel * label, double meters_from_start);
static void elevation_label_update(QLabel * label, Trackpoint * tp);
static void time_label_update(QLabel * label, time_t seconds_from_start);
static void real_time_label_update(QLabel * label, Trackpoint * tp);
static void speed_label_update(QLabel * label, double value);
static void dist_dist_label_update(QLabel * label, double distance);
static void gradient_label_update(QLabel * label, double gradient);




TrackProfileDialog::~TrackProfileDialog()
{
#ifdef K
	if (widgets->elev_graph_saved_img.img)
		g_object_unref(widgets->elev_graph_saved_img.img);
	if (widgets->gradient_graph_saved_img.img)
		g_object_unref(widgets->gradient_graph_saved_img.img);
	if (widgets->speed_graph_saved_img.img)
		g_object_unref(widgets->speed_graph_saved_img.img);
	if (widgets->dist_graph_saved_img.img)
		g_object_unref(widgets->dist_graph_saved_img.img);
	if (widgets->elev_time_graph_saved_img.img)
		g_object_unref(widgets->elev_time_graph_saved_img.img);
	if (widgets->speed_dist_graph_saved_img.img)
		g_object_unref(widgets->speed_dist_graph_saved_img.img);
	if (widgets->altitudes)
		free(widgets->altitudes);
	if (widgets->gradients)
		free(widgets->gradients);
	if (widgets->speeds)
		free(widgets->speeds);
	if (widgets->distances)
		free(widgets->distances);
	if (widgets->ats)
		free(widgets->ats);
	if (widgets->speeds_dist)
		free(widgets->speeds_dist);
	free(widgets);
#endif
}




static void minmax_array(const double * array, double * min, double * max, bool NO_ALT_TEST, int PROFILE_WIDTH)
{
	*max = -1000;
	*min = 20000;

	for (int i = 0; i < PROFILE_WIDTH; i++) {
		if (NO_ALT_TEST || (array[i] != VIK_DEFAULT_ALTITUDE)) {
			if (array[i] > *max) {
				*max = array[i];
			}
			if (array[i] < *min) {
				*min = array[i];
			}
		}
	}
}




#define MARGIN_X 70
#define MARGIN_Y 20
#define LINES 5

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
	double diff_chunk = (maxa - mina)/LINES;

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




static Trackpoint * set_center_at_graph_position(double event_x,
						 int img_width,
						 LayerTRW * trw,
						 LayersPanel * panel,
						 Viewport * viewport,
						 Track * trk,
						 bool time_base,
						 int PROFILE_WIDTH)
{
	Trackpoint * tp;
	double x = event_x - img_width / 2 + PROFILE_WIDTH / 2 - MARGIN_X / 2;
	if (x < 0) {
		x = 0;
	}
	if (x > PROFILE_WIDTH) {
		x = PROFILE_WIDTH;
	}

	if (time_base) {
		tp = trk->get_closest_tp_by_percentage_time((double) x / PROFILE_WIDTH, NULL);
	} else {
		tp = trk->get_closest_tp_by_percentage_dist((double) x / PROFILE_WIDTH, NULL);
	}

	if (tp) {
		VikCoord coord = tp->coord;
		if (panel) {
			panel->get_viewport()->set_center_coord(&coord, true);
			panel->emit_update_cb();
		} else {
			/* Since panel not set, viewport should be valid instead! */
			if (viewport) {
				viewport->set_center_coord(&coord, true);
			}
			trw->emit_changed();
		}
	}
	return tp;
}




/**
 * Returns whether the marker was drawn or not and whether the blob was drawn or not.
 */
static void save_image_and_draw_graph_marks(Viewport * viewport,
					    double marker_x,
					    QPen & pen,
					    int blob_x,
					    int blob_y,
					    PropSaved *saved_img,
					    int PROFILE_WIDTH,
					    int PROFILE_HEIGHT,
					    bool *marker_drawn,
					    bool *blob_drawn)
{
#ifdef K
	GdkPixmap *pix = NULL;
	/* The pixmap = margin + graph area. */
	gtk_image_get_pixmap(GTK_IMAGE(image), &pix, NULL);

	/* Restore previously saved image. */
	if (saved_img->saved) {
		gdk_draw_image(GDK_DRAWABLE(pix), pen, saved_img->img, 0, 0, 0, 0, MARGIN_X+PROFILE_WIDTH, MARGIN_Y+PROFILE_HEIGHT);
		saved_img->saved = false;
	}

	/* ATM always save whole image - as anywhere could have changed. */
	if (saved_img->img) {
		gdk_drawable_copy_to_image(GDK_DRAWABLE(pix), saved_img->img, 0, 0, 0, 0, MARGIN_X+PROFILE_WIDTH, MARGIN_Y+PROFILE_HEIGHT);
	} else {
		saved_img->img = gdk_drawable_copy_to_image(GDK_DRAWABLE(pix), saved_img->img, 0, 0, 0, 0, MARGIN_X+PROFILE_WIDTH, MARGIN_Y+PROFILE_HEIGHT);
	}
	saved_img->saved = true;

	if ((marker_x >= MARGIN_X) && (marker_x < (PROFILE_WIDTH + MARGIN_X))) {
		gdk_draw_line(GDK_DRAWABLE(pix), pen, marker_x, MARGIN_Y, marker_x, PROFILE_HEIGHT + MARGIN_Y);
		*marker_drawn = true;
	} else {
		*marker_drawn = false;
	}

	/* Draw a square blob to indicate where we are on track for this graph. */
	if ((blob_x >= MARGIN_X) && (blob_x < (PROFILE_WIDTH + MARGIN_X)) && (blob_y < PROFILE_HEIGHT+MARGIN_Y)) {
		fill_rectangle(GDK_DRAWABLE(pix), pen, blob_x-3, blob_y-3, 6, 6);
		*blob_drawn = true;
	} else {
		*blob_drawn = false;
	}

	/* Anywhere on image could have changed. */
	if (*marker_drawn || *blob_drawn) {
		gtk_widget_queue_draw(image);
	}
#endif
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
	time_t t_start = (*trk->trackpointsB->begin())->timestamp;
	time_t t_end = (*std::prev(trk->trackpointsB->end()))->timestamp;
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

	auto iter = std::next(trk->trackpointsB->begin());
	for (; iter != trk->trackpointsB->end(); iter++) {
		dist += vik_coord_diff(&(*iter)->coord,
				       &(*std::prev(iter))->coord);
		/* Assuming trackpoint is not a copy. */
		if (tp == *iter) {
			break;
		}
	}
	if (iter != trk->trackpointsB->end()) {
		pc = dist / track_length;
	}
	return pc;
}




static void track_graph_click(GtkWidget * widget, GdkEventButton * event, TrackProfileDialog * widgets, VikPropWinGraphType_t graph_type)
{
	bool is_time_graph =
		(graph_type == PROPWIN_GRAPH_TYPE_SPEED_TIME
		 || graph_type == PROPWIN_GRAPH_TYPE_DISTANCE_TIME
		 || graph_type == PROPWIN_GRAPH_TYPE_ELEVATION_TIME);

#ifdef K
	GtkAllocation allocation;
	gtk_widget_get_allocation(widget, &allocation);

	Trackpoint * tp = set_center_at_graph_position(event->x, allocation.width, widgets->trw, widgets->panel, widgets->viewport, widgets->trk, is_time_graph, widgets->profile_width);
	/* Unable to get the point so give up. */
	if (tp == NULL) {
		gtk_dialog_set_response_sensitive(GTK_DIALOG(widgets->dialog), VIK_TRW_LAYER_PROPWIN_SPLIT_MARKER, false);
		return;
	}

	widgets->marker_tp = tp;

	Viewport * graph_viewport = NULL;
	PropSaved *graph_saved_img;
	double pc = NAN;

	/* Attempt to redraw marker on all graph types. */
	int graphite;
	for (graphite = PROPWIN_GRAPH_TYPE_ELEVATION_DISTANCE;
	     graphite < PROPWIN_GRAPH_TYPE_END;
	     graphite++) {

		/* Switch commonal variables to particular graph type. */
		switch (graphite) {
		default:
		case PROPWIN_GRAPH_TYPE_ELEVATION_DISTANCE:
			graph_viewport  = widgets->elev_viewport;
			graph_saved_img = &widgets->elev_graph_saved_img;
			is_time_graph   = false;
			break;
		case PROPWIN_GRAPH_TYPE_GRADIENT_DISTANCE:
			graph_viewport  = widgets->gradient_viewport;
			graph_saved_img = &widgets->gradient_graph_saved_img;
			is_time_graph   = false;
			break;
		case PROPWIN_GRAPH_TYPE_SPEED_TIME:
			graph_viewport  = widgets->speed_viewport;
			graph_saved_img = &widgets->speed_graph_saved_img;
			is_time_graph   = true;
			break;
		case PROPWIN_GRAPH_TYPE_DISTANCE_TIME:
			graph_viewport  = widgets->dist_viewport;
			graph_saved_img = &widgets->dist_graph_saved_img;
			is_time_graph   = true;
			break;
		case PROPWIN_GRAPH_TYPE_ELEVATION_TIME:
			graph_viewport  = widgets->elev_time_viewport;
			graph_saved_img = &widgets->elev_time_graph_saved_img;
			is_time_graph   = true;
			break;
		case PROPWIN_GRAPH_TYPE_SPEED_DISTANCE:
			graph_viewport  = widgets->speed_dist_viewport;
			graph_saved_img = &widgets->speed_dist_graph_saved_img;
			is_time_graph   = false;
			break;
		}

		/* Commonal method of redrawing marker. */
		if (graph_viewport) {
			if (is_time_graph) {
				pc = tp_percentage_by_time(widgets->trk, tp);
			} else {
				pc = tp_percentage_by_distance(widgets->trk, tp, widgets->track_length_inc_gaps);
			}

			if (!isnan(pc)) {
				double marker_x = (pc * widgets->profile_width) + MARGIN_X;
				save_image_and_draw_graph_marks(graph_viewport,
								marker_x,
								black_pen,
								-1, /* Don't draw blob on clicks. */
								0,
								graph_saved_img,
								widgets->profile_width,
								widgets->profile_height,
								&widgets->is_marker_drawn,
								&widgets->is_blob_drawn);
			}
			g_list_free(child);
		}
	}

	gtk_dialog_set_response_sensitive(GTK_DIALOG(widgets->dialog), VIK_TRW_LAYER_PROPWIN_SPLIT_MARKER, widgets->is_marker_drawn);
#endif
}




static bool track_profile_click(GtkWidget * widget, GdkEventButton *event, void * ptr)
{
	track_graph_click(widget, event, (TrackProfileDialog *) ptr, PROPWIN_GRAPH_TYPE_ELEVATION_DISTANCE);
	return true;  /* Don't call other (further) callbacks. */
}




static bool track_gradient_click(GtkWidget * widget, GdkEventButton *event, void * ptr)
{
	track_graph_click(widget, event, (TrackProfileDialog *) ptr, PROPWIN_GRAPH_TYPE_GRADIENT_DISTANCE);
	return true;  /* Don't call other (further) callbacks. */
}




static bool track_vt_click(GtkWidget * widget, GdkEventButton *event, void * ptr)
{
	track_graph_click(widget, event, (TrackProfileDialog *) ptr, PROPWIN_GRAPH_TYPE_SPEED_TIME);
	return true;  /* Don't call other (further) callbacks. */
}




static bool track_dt_click(GtkWidget *widget, GdkEventButton *event, void * ptr)
{
	track_graph_click(widget, event, (TrackProfileDialog *) ptr, PROPWIN_GRAPH_TYPE_DISTANCE_TIME);
	return true;  /* Don't call other (further) callbacks. */
}




static bool track_et_click(GtkWidget *widget, GdkEventButton *event, void * ptr)
{
	track_graph_click(widget, event, (TrackProfileDialog *) ptr, PROPWIN_GRAPH_TYPE_ELEVATION_TIME);
	return true;  /* Don't call other (further) callbacks. */
}




static bool track_sd_click(GtkWidget *widget, GdkEventButton *event, void * ptr)
{
	track_graph_click(widget, event, (TrackProfileDialog *) ptr, PROPWIN_GRAPH_TYPE_SPEED_DISTANCE);
	return true;  /* Don't call other (further) callbacks. */
}




/**
 * Calculate y position for blob on elevation graph.
 */
int TrackProfileDialog::blobby_altitude(double x_blob)
{
	int ix = (int)x_blob;
	/* Ensure ix is inbounds. */
	if (ix == this->profile_width) {
		ix--;
	}

	int y_blob = this->profile_height - this->profile_height * (this->altitudes[ix] - this->draw_min_altitude) / (chunksa[this->cia] * LINES);

	return y_blob;
}




/**
 * Calculate y position for blob on gradient graph.
 */
int TrackProfileDialog::blobby_gradient(double x_blob)
{
	int ix = (int)x_blob;
	/* Ensure ix is inbounds. */
	if (ix == this->profile_width) {
		ix--;
	}

	int y_blob = this->profile_height - this->profile_height * (this->gradients[ix] - this->draw_min_gradient) / (chunksg[this->cig] * LINES);

	return y_blob;
}




/**
 * Calculate y position for blob on speed graph.
 */
int TrackProfileDialog::blobby_speed(double x_blob)
{
	int ix = (int)x_blob;
	/* Ensure ix is inbounds. */
	if (ix == this->profile_width) {
		ix--;
	}

	int y_blob = this->profile_height - this->profile_height * (this->speeds[ix] - this->draw_min_speed) / (chunkss[this->cis] * LINES);

	return y_blob;
}




/**
 * Calculate y position for blob on distance graph.
 */
int TrackProfileDialog::blobby_distance(double x_blob)
{
	int ix = (int)x_blob;
	/* Ensure ix is inbounds. */
	if (ix == this->profile_width) {
		ix--;
	}

	int y_blob = this->profile_height - this->profile_height * (this->distances[ix]) / (chunksd[this->cid] * LINES);
	/* Min distance is always 0, so no need to subtract that from this __/ */

	return y_blob;
}




/**
 * Calculate y position for blob on elevation/time graph.
 */
int TrackProfileDialog::blobby_altitude_time(double x_blob)
{
	int ix = (int)x_blob;
	/* Ensure ix is inbounds. */
	if (ix == this->profile_width) {
		ix--;
	}

	int y_blob = this->profile_height - this->profile_height * (this->ats[ix] - this->draw_min_altitude_time) / (chunksa[this->ciat] * LINES);
	return y_blob;
}




/**
 * Calculate y position for blob on speed/dist graph.
 */
int TrackProfileDialog::blobby_speed_dist(double x_blob)
{
	int ix = (int)x_blob;
	/* Ensure ix is inbounds. */
	if (ix == this->profile_width) {
		ix--;
	}

	int y_blob = this->profile_height - this->profile_height * (this->speeds_dist[ix] - this->draw_min_speed) / (chunkss[this->cisd] * LINES);

	return y_blob;
}




void track_profile_move(Viewport * viewport, GdkEventMotion *event, TrackProfileDialog * widgets)
{
	if (widgets->altitudes == NULL) {
		return;
	}

	double x = NAN;
	int ix = 0;
	get_mouse_event_x(viewport, event, widgets, &x, &ix);

	double meters_from_start;
	Trackpoint * tp = widgets->trk->get_closest_tp_by_percentage_dist((double) x / widgets->profile_width, &meters_from_start);
	if (tp && widgets->w_cur_dist) {
		distance_label_update(widgets->w_cur_dist, meters_from_start);
	}

	/* Show track elevation for this position - to the nearest whole number. */
	if (tp && widgets->w_cur_elevation) {
		elevation_label_update(widgets->w_cur_elevation, tp);
	}

	widgets->blob_tp = tp;

	int y_blob = widgets->blobby_altitude(x);

	double marker_x = -1.0; /* i.e. don't draw unless we get a valid value. */
	if (widgets->is_marker_drawn) {
		double pc = tp_percentage_by_distance(widgets->trk, widgets->marker_tp, widgets->track_length_inc_gaps);
		if (!isnan(pc)) {
			marker_x = (pc * widgets->profile_width) + MARGIN_X;
		}
	}

	QPen black_pen(QColor("black"));
	save_image_and_draw_graph_marks(viewport,
					marker_x,
					black_pen,
					MARGIN_X+x,
					MARGIN_Y+y_blob,
					&widgets->elev_graph_saved_img,
					widgets->profile_width,
					widgets->profile_height,
					&widgets->is_marker_drawn,
					&widgets->is_blob_drawn);

}




void track_gradient_move(Viewport * viewport, GdkEventMotion * event, TrackProfileDialog * widgets)
{
	if (widgets->gradients == NULL) {
		return;
	}

	double x = NAN;
	int ix = 0;
	get_mouse_event_x(viewport, event, widgets, &x, &ix);

	double meters_from_start;
	Trackpoint * tp = widgets->trk->get_closest_tp_by_percentage_dist((double) x / widgets->profile_width, &meters_from_start);
	if (tp && widgets->w_cur_gradient_dist) {
		distance_label_update(widgets->w_cur_gradient_dist, meters_from_start);
	}

	/* Show track gradient for this position - to the nearest whole number. */
	if (tp && widgets->w_cur_gradient_gradient) {
		gradient_label_update(widgets->w_cur_gradient_gradient, widgets->gradients[ix]);
	}

	widgets->blob_tp = tp;

	int y_blob = widgets->blobby_gradient(x);

	double marker_x = -1.0; /* i.e. don't draw unless we get a valid value. */
	if (widgets->is_marker_drawn) {
		double pc = tp_percentage_by_distance(widgets->trk, widgets->marker_tp, widgets->track_length_inc_gaps);
		if (!isnan(pc)) {
			marker_x = (pc * widgets->profile_width) + MARGIN_X;
		}
	}

	QPen black_pen(QColor("black"));
	save_image_and_draw_graph_marks(viewport,
					marker_x,
					black_pen,
					MARGIN_X+x,
					MARGIN_Y+y_blob,
					&widgets->gradient_graph_saved_img,
					widgets->profile_width,
					widgets->profile_height,
					&widgets->is_marker_drawn,
					&widgets->is_blob_drawn);

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
	SpeedUnit speed_units = a_vik_get_units_speed();
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




void track_vt_move(Viewport * viewport, GdkEventMotion * event, TrackProfileDialog * widgets)
{
	if (widgets->speeds == NULL) {
		return;
	}

	double x = NAN;
	int ix = 0;
	get_mouse_event_x(viewport, event, widgets, &x, &ix);

	time_t seconds_from_start;
	Trackpoint * tp = widgets->trk->get_closest_tp_by_percentage_time((double) x / widgets->profile_width, &seconds_from_start);
	if (tp && widgets->w_cur_time) {
		time_label_update(widgets->w_cur_time, seconds_from_start);
	}

	if (tp && widgets->w_cur_time_real) {
		real_time_label_update(widgets->w_cur_time_real, tp);
	}

	/* Show track speed for this position. */
	if (tp && widgets->w_cur_speed) {
		speed_label_update(widgets->w_cur_speed, widgets->speeds[ix]);
	}

	widgets->blob_tp = tp;

	int y_blob = widgets->blobby_speed(x);

	double marker_x = -1.0; /* i.e. don't draw unless we get a valid value. */
	if (widgets->is_marker_drawn) {
		double pc = tp_percentage_by_time(widgets->trk, widgets->marker_tp);
		if (!isnan(pc)) {
			marker_x = (pc * widgets->profile_width) + MARGIN_X;
		}
	}

	QPen black_pen(QColor("black"));
	save_image_and_draw_graph_marks(viewport,
					marker_x,
					black_pen,
					MARGIN_X+x,
					MARGIN_Y+y_blob,
					&widgets->speed_graph_saved_img,
					widgets->profile_width,
					widgets->profile_height,
					&widgets->is_marker_drawn,
					&widgets->is_blob_drawn);

}




/**
 * Update labels and blob marker on mouse moves in the distance/time graph.
 */
void track_dt_move(Viewport * viewport, GdkEventMotion * event, TrackProfileDialog * widgets)
{
	if (widgets->distances == NULL) {
		return;
	}

	double x = NAN;
	int ix = 0;
	get_mouse_event_x(viewport, event, widgets, &x, &ix);

	time_t seconds_from_start;
	Trackpoint * tp = widgets->trk->get_closest_tp_by_percentage_time((double) x / widgets->profile_width, &seconds_from_start);
	if (tp && widgets->w_cur_dist_time) {
		time_label_update(widgets->w_cur_dist_time, seconds_from_start);
	}

	if (tp && widgets->w_cur_dist_time_real) {
		real_time_label_update(widgets->w_cur_dist_time_real, tp);
	}

	if (tp && widgets->w_cur_dist_dist) {
		dist_dist_label_update(widgets->w_cur_dist_dist, widgets->distances[ix]);
	}

	widgets->blob_tp = tp;

	int y_blob = widgets->blobby_distance(x);

	double marker_x = -1.0; // i.e. Don't draw unless we get a valid value
	if (widgets->is_marker_drawn) {
		double pc = tp_percentage_by_time(widgets->trk, widgets->marker_tp);
		if (!isnan(pc)) {
			marker_x = (pc * widgets->profile_width) + MARGIN_X;
		}
	}

	QPen black_pen(QColor("black"));
	save_image_and_draw_graph_marks(viewport,
					marker_x,
					black_pen,
					MARGIN_X+x,
					MARGIN_Y+y_blob,
					&widgets->dist_graph_saved_img,
					widgets->profile_width,
					widgets->profile_height,
					&widgets->is_marker_drawn,
					&widgets->is_blob_drawn);
}




/**
 * Update labels and blob marker on mouse moves in the elevation/time graph.
 */
void track_et_move(Viewport * viewport, GdkEventMotion * event, TrackProfileDialog * widgets)
{
	if (widgets->ats == NULL) {
		return;
	}

	double x = NAN;
	int ix = 0;
	get_mouse_event_x(viewport, event, widgets, &x, &ix);

	time_t seconds_from_start;
	Trackpoint * tp = widgets->trk->get_closest_tp_by_percentage_time((double) x / widgets->profile_width, &seconds_from_start);
	if (tp && widgets->w_cur_elev_time) {
		time_label_update(widgets->w_cur_elev_time, seconds_from_start);
	}

	if (tp && widgets->w_cur_elev_time_real) {
		real_time_label_update(widgets->w_cur_elev_time_real, tp);
	}

	if (tp && widgets->w_cur_elev_elev) {
		elevation_label_update(widgets->w_cur_elev_elev, tp);
	}

	widgets->blob_tp = tp;

	int y_blob = widgets->blobby_altitude_time(x);

	double marker_x = -1.0; /* i.e. don't draw unless we get a valid value. */
	if (widgets->is_marker_drawn) {
		double pc = tp_percentage_by_time(widgets->trk, widgets->marker_tp);
		if (!isnan(pc)) {
			marker_x = (pc * widgets->profile_width) + MARGIN_X;
		}
	}

	QPen black_pen(QColor("black"));
	save_image_and_draw_graph_marks(viewport,
					marker_x,
					black_pen,
					MARGIN_X+x,
					MARGIN_Y+y_blob,
					&widgets->elev_time_graph_saved_img,
					widgets->profile_width,
					widgets->profile_height,
					&widgets->is_marker_drawn,
					&widgets->is_blob_drawn);
}




void track_sd_move(Viewport * viewport, GdkEventMotion * event, TrackProfileDialog * widgets)
{
	if (widgets->speeds_dist == NULL) {
		return;
	}

	double x = NAN;
	int ix = 0;
	get_mouse_event_x(viewport, event, widgets, &x, &ix);

	double meters_from_start;
	Trackpoint * tp = widgets->trk->get_closest_tp_by_percentage_dist((double) x / widgets->profile_width, &meters_from_start);
	if (tp && widgets->w_cur_speed_dist) {
		distance_label_update(widgets->w_cur_speed_dist, meters_from_start);
	}

	/* Show track speed for this position. */
	if (widgets->w_cur_speed_speed) {
		speed_label_update(widgets->w_cur_speed_speed, widgets->speeds_dist[ix]);
	}

	widgets->blob_tp = tp;

	int y_blob = widgets->blobby_speed_dist(x);

	double marker_x = -1.0; /* i.e. don't draw unless we get a valid value. */
	if (widgets->is_marker_drawn) {
		double pc = tp_percentage_by_distance(widgets->trk, widgets->marker_tp, widgets->track_length_inc_gaps);
		if (!isnan(pc)) {
			marker_x = (pc * widgets->profile_width) + MARGIN_X;
		}
	}

	QPen black_pen(QColor("black"));
	save_image_and_draw_graph_marks(viewport,
					marker_x,
					black_pen,
					MARGIN_X+x,
					MARGIN_Y+y_blob,
					&widgets->speed_dist_graph_saved_img,
					widgets->profile_width,
					widgets->profile_height,
					&widgets->is_marker_drawn,
					&widgets->is_blob_drawn);
}




void get_mouse_event_x(Viewport * viewport, GdkEventMotion * event, TrackProfileDialog * widgets, double * x, int * ix)
{
#ifdef K
	int mouse_x, mouse_y;
	GdkModifierType state;

	if (event->is_hint) {
		gdk_window_get_pointer(event->window, &mouse_x, &mouse_y, &state);
	} else {
		mouse_x = event->x;
	}

	GtkAllocation allocation;
	gtk_widget_get_allocation(widget, &allocation);

	(*x) = mouse_x - allocation.width / 2 + widgets->profile_width / 2 - MARGIN_X / 2;
	if ((*x) < 0) {
		(*x) = 0;
	}
	if ((*x) > widgets->profile_width) {
		(*x) = widgets->profile_width;
	}

	*ix = (int) (*x);
	/* Ensure ix is inbounds. */
	if (*ix == widgets->profile_width) {
		(*ix)--;
	}
#endif

	return;
}




void distance_label_update(QLabel * label, double meters_from_start)
{
	static char tmp_buf[20];
	DistanceUnit distance_unit = a_vik_get_units_distance();
	get_distance_string(tmp_buf, sizeof (tmp_buf), distance_unit, meters_from_start);
	label->setText(QString(tmp_buf));

	return;
}




void elevation_label_update(QLabel * label, Trackpoint * tp)
{
	static char tmp_buf[20];
	if (a_vik_get_units_height() == HeightUnit::FEET) {
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
	switch (a_vik_get_units_distance()) {
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
				    GdkDrawable *pix,
				    QPen & alt_pen,
				    QPen & speed_pen,
				    double alt_offset,
				    double max_speed_in,
				    int cia,
				    int width,
				    int height,
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
	int h2 = height + MARGIN_Y; /* Adjust height for x axis labelling offset. */
	int achunk = chunksa[cia]*LINES;

	for (auto iter = std::next(trk->trackpointsB->begin()); iter != trk->trackpointsB->end(); iter++) {

		dist += vik_coord_diff(&(*iter)->coord,
				       &(*std::prev(iter))->coord);
		int x = (width * dist) / total_length + margin;
		if (do_dem) {
			int16_t elev = dem_cache_get_elev_by_coord(&(*iter)->coord, VIK_DEM_INTERPOL_BEST);
			if (elev != VIK_DEM_INVALID_ELEVATION) {
				/* Convert into height units. */
				if (a_vik_get_units_height() == HeightUnit::FEET) {
					elev = VIK_METERS_TO_FEET(elev);
				}
				/* No conversion needed if already in metres. */

				/* offset is in current height units. */
				elev -= alt_offset;

				/* consider chunk size. */
				int y_alt = h2 - ((height * elev)/achunk);
#ifdef K
				fill_rectangle(GDK_DRAWABLE (pix), alt_pen, x - 2, y_alt - 2, 4, 4);
#endif
			}
		}
		if (do_speed) {
			/* This is just a speed indicator - no actual values can be inferred by user. */
			if (!isnan((*iter)->speed)) {
				int y_speed = h2 - (height * (*iter)->speed) / max_speed;
#ifdef K
				fill_rectangle(GDK_DRAWABLE (pix), speed_pen, x - 2, y_speed - 2, 4, 4);
#endif
			}
		}
	}
}




/**
 * A common way to draw the grid with y axis labels
 */
void TrackProfileDialog::draw_grid_y(Viewport * viewport, QPen & fg_pen, QPen & dark_pen, QPixmap * pix, char * ss, int i)
{
#ifdef K
	PangoLayout * pl = gtk_widget_create_pango_layout(GTK_WIDGET(viewport), NULL);

	pango_layout_set_alignment(pl, PANGO_ALIGN_RIGHT);
	pango_layout_set_font_description(pl, gtk_widget_get_style(window)->font_desc);

	char * label_markup = g_strdup_printf("<span size=\"small\">%s</span>", ss);
	pango_layout_set_markup(pl, label_markup, -1);
	free(label_markup);

	int w, h;
	pango_layout_get_pixel_size(pl, &w, &h);

	gdk_draw_layout(GDK_DRAWABLE(pix), fg_pen,
			MARGIN_X-w-3,
			CLAMP((int)i * this->profile_height/LINES - h/2 + MARGIN_Y, 0, this->profile_height-h+MARGIN_Y),
			pl);
	g_object_unref(G_OBJECT (pl));

	gdk_draw_line(GDK_DRAWABLE(pix), dark_pen,
		      MARGIN_X, MARGIN_Y + this->profile_height/LINES * i,
		      MARGIN_X + this->profile_width, MARGIN_Y + this->profile_height/LINES * i);
#endif
}




/**
 * A common way to draw the grid with x axis labels for time graphs
 */
void TrackProfileDialog::draw_grid_x_time(Viewport * viewport, QPixmap * pix, unsigned int ii, unsigned int tt, unsigned int xx)
{
	char *label_markup = NULL;
#ifdef K
	switch (ii) {
	case 0:
	case 1:
	case 2:
	case 3:
		/* Minutes. */
		label_markup = g_strdup_printf("<span size=\"small\">%d %s</span>", tt/60, _("mins"));
		break;
	case 4:
	case 5:
	case 6:
	case 7:
		/* Hours. */
		label_markup = g_strdup_printf("<span size=\"small\">%.1f %s</span>", (double)tt/(60*60), _("h"));
		break;
	case 8:
	case 9:
	case 10:
		/* Days. */
		label_markup = g_strdup_printf("<span size=\"small\">%.1f %s</span>", (double)tt/(60*60*24), _("d"));
		break;
	case 11:
	case 12:
		/* Weeks. */
		label_markup = g_strdup_printf("<span size=\"small\">%.1f %s</span>", (double)tt/(60*60*24*7), _("w"));
		break;
	case 13:
		/* 'Months'. */
		label_markup = g_strdup_printf("<span size=\"small\">%.1f %s</span>", (double)tt/(60*60*24*28), _("M"));
		break;
	default:
		break;
	}
	if (label_markup) {

		PangoLayout *pl = gtk_widget_create_pango_layout(GTK_WIDGET(viewport), NULL);
		pango_layout_set_font_description(pl, font_desc);

		pango_layout_set_markup(pl, label_markup, -1);
		free(label_markup);
		int ww, hh;
		pango_layout_get_pixel_size(pl, &ww, &hh);

		gdk_draw_layout(GDK_DRAWABLE(pix), fg_pen,
				MARGIN_X+xx-ww/2, MARGIN_Y/2-hh/2, pl);
		g_object_unref(G_OBJECT (pl));
	}

	gdk_draw_line(GDK_DRAWABLE(pix), dark_pen,
		      MARGIN_X + xx, MARGIN_Y, MARGIN_X + xx, MARGIN_Y + this->profile_height);
#endif
}




/**
 * A common way to draw the grid with x axis labels for distance graphs.
 */
void TrackProfileDialog::draw_grid_x_distance(Viewport * viewport, QPixmap * pix, unsigned int ii, double dd, unsigned int xx, DistanceUnit distance_unit)
{
	char *label_markup = NULL;

	char distance_unit_string[16] = { 0 };
	get_distance_unit_string(distance_unit_string, sizeof (distance_unit_string), distance_unit);

#ifdef K

	if (ii > 4) {
		label_markup = g_strdup_printf("<span size=\"small\">%d %s</span>", (unsigned int)dd, distance_unit_string);
	} else {
		label_markup = g_strdup_printf("<span size=\"small\">%.1f %s</span>", dd, distance_unit_string);
	}

	if (label_markup) {
		PangoLayout *pl = gtk_widget_create_pango_layout(GTK_WIDGET(viewport), NULL);
		pango_layout_set_font_description(pl, font_desc);

		pango_layout_set_markup(pl, label_markup, -1);
		free(label_markup);
		int ww, hh;
		pango_layout_get_pixel_size(pl, &ww, &hh);

		gdk_draw_layout(GDK_DRAWABLE(pix), fg_pen,
				MARGIN_X+xx-ww/2, MARGIN_Y/2-hh/2, pl);
		g_object_unref(G_OBJECT (pl));
	}

	gdk_draw_line(GDK_DRAWABLE(pix), dark_pen,
		      MARGIN_X + xx, MARGIN_Y, MARGIN_X + xx, MARGIN_Y + this->profile_height);
#endif
}




/**
 * Clear the images (scale texts & actual graph).
 */
void TrackProfileDialog::clear_image(QPixmap * pix)
{
#ifdef K
	fill_rectangle(GDK_DRAWABLE(pix), bg_pen,
		       0, 0, this->profile_width + MARGIN_X, this->profile_height + MARGIN_Y);
	fill_rectangle(GDK_DRAWABLE(pix), mid_pen,
		       0, 0, this->profile_width + MARGIN_X, this->profile_height + MARGIN_Y);
#endif
}




static void draw_distance_divisions(Viewport * viewport, QPixmap * pix, TrackProfileDialog * widgets, DistanceUnit distance_unit)
{
	/* Set to display units from length in metres. */
	double length = widgets->track_length_inc_gaps;
	length = convert_distance_meters_to(distance_unit, length);

	unsigned int index = get_distance_chunk_index(length);
	double dist_per_pixel = length/widgets->profile_width;

	for (unsigned int i = 1; chunksd[index] * i <= length; i++) {
		widgets->draw_grid_x_distance(viewport, pix, index, chunksd[index] * i, (unsigned int) (chunksd[index] * i / dist_per_pixel), distance_unit);
	}
}




/**
 * Draw just the height profile image.
 */
void TrackProfileDialog::draw_elevations(Viewport * viewport, Track * trk)
{
	/* Free previous allocation. */
	if (this->altitudes) {
		free(this->altitudes);
	}

	this->altitudes = trk->make_elevation_map(this->profile_width);

	if (this->altitudes == NULL) {
		return;
	}

	/* Convert into appropriate units. */
	HeightUnit height_units = a_vik_get_units_height();
	if (height_units == HeightUnit::FEET) {
		/* Convert altitudes into feet units. */
		for (int i = 0; i < this->profile_width; i++) {
			this->altitudes[i] = VIK_METERS_TO_FEET(this->altitudes[i]);
		}
	}
	/* Otherwise leave in metres. */

	minmax_array(this->altitudes, &this->min_altitude, &this->max_altitude, true, this->profile_width);

	get_new_min_and_chunk_index(this->min_altitude, this->max_altitude, chunksa, G_N_ELEMENTS(chunksa), &this->draw_min_altitude, &this->cia);

	/* Assign locally. */
	double mina = this->draw_min_altitude;

#ifdef K
	QPixmap * pix = new QPixmap(this->profile_width + MARGIN_X, this->profile_height + MARGIN_Y);

	gtk_image_set_from_pixmap(GTK_IMAGE(viewport), pix, NULL);

	QPen no_alt_info_pen(QColor("yellow"));

	/* Reset before redrawing. */
	this->clear_image(pix);

	/* Draw grid. */
	QPen fg_pen(QColor("orange"));
	QPen dark_pen(QColor("black"));
	for (int i = 0; i <= LINES; i++) {
		char s[32];

		switch (height_units) {
		case HeightUnit::METRES:
			sprintf(s, "%8dm", (int)(mina + (LINES-i)*chunksa[this->cia]));
			break;
		case HeightUnit::FEET:
			/* NB values already converted into feet. */
			sprintf(s, "%8dft", (int)(mina + (LINES-i)*chunksa[this->cia]));
			break;
		default:
			sprintf(s, "--");
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. height=%d\n", height_units);
		}

		this->draw_grid_y(viewport, fg_pen, dark_pen, pix, s, i);
	}

	draw_distance_divisions(viewport, pix, this, a_vik_get_units_distance());

	/* Draw elevations. */
	unsigned int height = MARGIN_Y+this->profile_height;
	for (int i = 0; i < this->profile_width; i++) {
		if (this->altitudes[i] == VIK_DEFAULT_ALTITUDE) {
			gdk_draw_line(GDK_DRAWABLE(pix), no_alt_info_pen,
				      i + MARGIN_X, MARGIN_Y, i + MARGIN_X, height);
		} else {
			gdk_draw_line(GDK_DRAWABLE(pix), dark_pen3,
				      i + MARGIN_X, height, i + MARGIN_X, height-this->profile_height*(this->altitudes[i]-mina)/(chunksa[this->cia]*LINES));
		}
	}

	if (this->w_show_dem->checkState()
	    || this->w_show_alt_gps_speed->checkState()) {

		QPen dem_alt_pen(QColor("green"));
		QPen gps_speed_pen(QColor("red"));

		/* Ensure somekind of max speed when not set. */
		if (this->max_speed < 0.01) {
			this->max_speed = trk->get_max_speed();
		}

		draw_dem_alt_speed_dist(trk,
					GDK_DRAWABLE(pix),
					dem_alt_pen,
					gps_speed_pen,
					mina,
					this->max_speed,
					this->cia,
					this->profile_width,
					this->profile_height,
					MARGIN_X,
					this->w_show_dem->checkState(),
					this->w_show_alt_gps_speed->checkState());
	}

	/* Draw border. */
	draw_rectangle(GDK_DRAWABLE(pix), black_pen, MARGIN_X, MARGIN_Y, this->profile_width-1, this->profile_height-1);

	g_object_unref(G_OBJECT(pix));
#endif
}




/**
 * Draws representative speed on the supplied pixmap
 * (which is the gradients graph).
 */
static void draw_speed_dist(Track * trk,
			    QPixmap * pix,
			    QPen & speed_pen,
			    double max_speed_in,
			    int width,
			    int height,
			    bool do_speed)
{
	double max_speed = 0;
	double total_length = trk->get_length_including_gaps();

	/* Calculate the max speed factor. */
	if (do_speed) {
		max_speed = max_speed_in * 110 / 100;
	}

	double dist = 0;
	for (auto iter = std::next(trk->trackpointsB->begin()); iter != trk->trackpointsB->end(); iter++) {
#ifdef K
		int x;
		dist += vik_coord_diff(&(*iter)->coord,
				       &(*std::prev(iter))->coord);
		x = (width * dist) / total_length + MARGIN_X;
		if (do_speed) {
			/* This is just a speed indicator - no actual values can be inferred by user. */
			if (!isnan((*iter)->speed)) {
				int y_speed = height - (height * (*iter)->speed) / max_speed;
				fill_rectangle(GDK_DRAWABLE(pix), speed_pen, x - 2, y_speed - 2, 4, 4);
			}
		}
#endif
	}
}




/**
 * Draw just the gradient image.
 */
void TrackProfileDialog::draw_gradients(Viewport * viewport, Track * trk)
{
	/* Free previous allocation. */
	if (this->gradients) {
		free(this->gradients);
	}

	this->gradients = trk->make_gradient_map(this->profile_width);

	if (this->gradients == NULL) {
		return;
	}

	minmax_array(this->gradients, &this->min_gradient, &this->max_gradient, true, this->profile_width);

	get_new_min_and_chunk_index(this->min_gradient, this->max_gradient, chunksg, G_N_ELEMENTS(chunksg), &this->draw_min_gradient, &this->cig);

	/* Assign locally. */
	double mina = this->draw_min_gradient;


	QPixmap * pix = new QPixmap(this->profile_width + MARGIN_X, this->profile_height + MARGIN_Y);

	//gtk_image_set_from_pixmap(GTK_IMAGE(viewport), pix, NULL);

	/* Reset before redrawing. */
	this->clear_image(pix);

	/* Draw grid. */
	QPen fg_pen(QColor("orange"));
	QPen dark_pen(QColor("black"));
	for (int i = 0; i <= LINES; i++) {
		char s[32];

		sprintf(s, "%8d%%", (int)(mina + (LINES-i)*chunksg[this->cig]));
		this->draw_grid_y(viewport, fg_pen, dark_pen, pix, s, i);
	}

	draw_distance_divisions(viewport, pix, this, a_vik_get_units_distance());

	/* Draw gradients. */
	unsigned int height = this->profile_height + MARGIN_Y;
	for (int i = 0; i < this->profile_width; i++) {
#ifdef K
		gdk_draw_line(GDK_DRAWABLE(pix), dark_pen3,
			      i + MARGIN_X, height, i + MARGIN_X, height - this->profile_height*(this->gradients[i]-mina)/(chunksg[this->cig]*LINES));
#endif
	}

	if (this->w_show_gradient_gps_speed->checkState()) {

		QPen gps_speed_pen(QColor("red"));

		/* Ensure somekind of max speed when not set. */
		if (this->max_speed < 0.01) {
			this->max_speed = trk->get_max_speed();
		}

		draw_speed_dist(trk,
				pix,
				gps_speed_pen,
				this->max_speed,
				this->profile_width,
				this->profile_height,
				this->w_show_alt_gps_speed->checkState());
	}

#ifdef K
	/* Draw border. */
	draw_rectangle(GDK_DRAWABLE(pix), black_pen, MARGIN_X, MARGIN_Y, this->profile_width-1, this->profile_height-1);
#endif
}




void TrackProfileDialog::draw_time_lines(Viewport * viewport, QPixmap * pix)
{
	unsigned int index = get_time_chunk_index(this->duration);
	double time_per_pixel = (double)(this->duration)/this->profile_width;

	/* If stupidly long track in time - don't bother trying to draw grid lines. */
	if (this->duration > chunkst[G_N_ELEMENTS(chunkst)-1]*LINES*LINES) {
		return;
	}

	for (unsigned int i=1; chunkst[index]*i <= this->duration; i++) {
		this->draw_grid_x_time(viewport, pix, index, chunkst[index]*i, (unsigned int)(chunkst[index]*i/time_per_pixel));
	}
}




/**
 * Draw just the speed (velocity)/time image.
 */
void TrackProfileDialog::draw_vt(Viewport * viewport, Track * trk)
{
	/* Free previous allocation. */
	if (this->speeds) {
		free(this->speeds);
	}

	this->speeds = trk->make_speed_map(this->profile_width);
	if (this->speeds == NULL) {
		return;
	}

	this->duration = trk->get_duration(true);
	/* Negative time or other problem. */
	if (this->duration <= 0) {
		return;
	}

	/* Convert into appropriate units. */
	SpeedUnit speed_units = a_vik_get_units_speed();
	for (int i = 0; i < this->profile_width; i++) {
		this->speeds[i] = convert_speed_mps_to(speed_units, this->speeds[i]);
	}

	QPixmap * pix = new QPixmap(this->profile_width + MARGIN_X, this->profile_height + MARGIN_Y);
#ifdef K
	gtk_image_set_from_pixmap(GTK_IMAGE(viewport), pix, NULL);
#endif

	minmax_array(this->speeds, &this->min_speed, &this->max_speed, false, this->profile_width);
	if (this->min_speed < 0.0) {
		this->min_speed = 0; /* Splines sometimes give negative speeds. */
	}

	/* Find suitable chunk index. */
	get_new_min_and_chunk_index(this->min_speed, this->max_speed, chunkss, G_N_ELEMENTS(chunkss), &this->draw_min_speed, &this->cis);

	/* Assign locally. */
	double mins = this->draw_min_speed;

	/* Reset before redrawing. */
	this->clear_image(pix);

	/* Draw grid. */
	QPen fg_pen(QColor("orange"));
	QPen dark_pen(QColor("black"));
	for (int i = 0; i <= LINES; i++) {
		char s[32];

		/* NB: No need to convert here anymore as numbers are in the appropriate units. */
		switch (speed_units) {
		case SpeedUnit::KILOMETRES_PER_HOUR:
			sprintf(s, "%8dkm/h", (int)(mins + (LINES-i)*chunkss[this->cis]));
			break;
		case SpeedUnit::MILES_PER_HOUR:
			sprintf(s, "%8dmph", (int)(mins + (LINES-i)*chunkss[this->cis]));
			break;
		case SpeedUnit::METRES_PER_SECOND:
			sprintf(s, "%8dm/s", (int)(mins + (LINES-i)*chunkss[this->cis]));
			break;
		case SpeedUnit::KNOTS:
			sprintf(s, "%8dknots", (int)(mins + (LINES-i)*chunkss[this->cis]));
			break;
		default:
			sprintf(s, "--");
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. speed=%d\n", speed_units);
		}

		this->draw_grid_y(viewport, fg_pen, dark_pen, pix, s, i);
	}

	this->draw_time_lines(viewport, pix);

	/* Draw speeds. */
	unsigned int height = this->profile_height + MARGIN_Y;
	for (int i = 0; i < this->profile_width; i++) {
#ifdef K
		gdk_draw_line(GDK_DRAWABLE(pix), dark_pen3,
			       i + MARGIN_X, height, i + MARGIN_X, height - this->profile_height*(this->speeds[i]-mins)/(chunkss[this->cis]*LINES));
#endif
	}

	if (this->w_show_gps_speed->checkState()) {

		QPen gps_speed_pen(QColor("red"));

		time_t beg_time = (*trk->trackpointsB->begin())->timestamp;
		time_t dur = (*std::prev(trk->trackpointsB->end()))->timestamp - beg_time;

		for (auto iter = trk->trackpointsB->begin(); iter != trk->trackpointsB->end(); iter++) {
			double gps_speed = (*iter)->speed;
			if (isnan(gps_speed)) {
				continue;
			}

			gps_speed = convert_speed_mps_to(speed_units, gps_speed);

			int x = MARGIN_X + this->profile_width * ((*iter)->timestamp - beg_time) / dur;
			int y = height - this->profile_height * (gps_speed - mins) / (chunkss[this->cis] * LINES);
#ifdef K
			fill_rectangle(GDK_DRAWABLE(pix), gps_speed_pen, x - 2, y - 2, 4, 4);
#endif
		}
	}
#ifdef K
	/* Draw border. */
	draw_rectangle(GDK_DRAWABLE(pix), black_pen, MARGIN_X, MARGIN_Y, this->profile_width-1, this->profile_height-1);
#endif
}




/**
 * Draw just the distance/time image.
 */
void TrackProfileDialog::draw_dt(Viewport * viewport, Track * trk)
{
	/* Free previous allocation. */
	if (this->distances) {
		free(this->distances);
	}

	this->distances = trk->make_distance_map(this->profile_width);
	if (this->distances == NULL) {
		return;
	}

	/* Convert into appropriate units. */
	DistanceUnit distance_unit = a_vik_get_units_distance();
	for (int i = 0; i < this->profile_width; i++) {
		this->distances[i] = convert_distance_meters_to(distance_unit, this->distances[i]);
	}

	this->duration = this->trk->get_duration(true);
	/* Negative time or other problem. */
	if (this->duration <= 0) {
		return;
	}

	QPixmap * pix = new QPixmap(this->profile_width + MARGIN_X, this->profile_height + MARGIN_Y);
#ifdef K
	gtk_image_set_from_pixmap(GTK_IMAGE(viewport), pix, NULL);
#endif

	/* Easy to work out min / max of distance!
	   Assign locally.
	   mind = 0.0; - Thus not used. */
	double maxd = convert_distance_meters_to(distance_unit, trk->get_length_including_gaps());

	/* Find suitable chunk index. */
	double dummy = 0.0; /* Expect this to remain the same! (not that it's used). */
	get_new_min_and_chunk_index(0, maxd, chunksd, G_N_ELEMENTS(chunksd), &dummy, &this->cid);

	/* Reset before redrawing. */
	this->clear_image(pix);

	/* Draw grid. */
	QPen fg_pen(QColor("orange"));
	QPen dark_pen(QColor("black"));
	for (int i = 0; i <= LINES; i++) {
		char s[32];

		switch (distance_unit) {
		case DistanceUnit::MILES:
			sprintf(s, _("%.1f miles"), ((LINES-i)*chunksd[this->cid]));
			break;
		case DistanceUnit::NAUTICAL_MILES:
			sprintf(s, _("%.1f NM"), ((LINES-i)*chunksd[this->cid]));
			break;
		default:
			sprintf(s, _("%.1f km"), ((LINES-i)*chunksd[this->cid]));
			break;
		}

		this->draw_grid_y(viewport, fg_pen, dark_pen, pix, s, i);
	}

	this->draw_time_lines(viewport, pix);

	/* Draw distance. */
	unsigned int height = this->profile_height + MARGIN_Y;
	for (int i = 0; i < this->profile_width; i++) {
#ifdef K
		gdk_draw_line(GDK_DRAWABLE(pix), dark_pen3,
			      i + MARGIN_X, height, i + MARGIN_X, height - this->profile_height*(this->distances[i])/(chunksd[this->cid]*LINES));
#endif
	}

	/* Show speed indicator. */
	if (this->w_show_dist_speed->checkState()) {
		QPen dist_speed_gc(QColor("red"));

		double max_speed = 0;
		max_speed = this->max_speed * 110 / 100;

		/* This is just an indicator - no actual values can be inferred by user. */
		for (int i = 0; i < this->profile_width; i++) {
			int y_speed = this->profile_height - (this->profile_height * this->speeds[i])/max_speed;
#ifdef K
			fill_rectangle(GDK_DRAWABLE(pix), dist_speed_pen, i+MARGIN_X-2, y_speed-2, 4, 4);
#endif
		}
	}
#ifdef K
	/* Draw border. */
	draw_rectangle(GDK_DRAWABLE(pix), black_pen, MARGIN_X, MARGIN_Y, this->profile_width-1, this->profile_height-1);
#endif
}




/**
 * Draw just the elevation/time image.
 */
void TrackProfileDialog::draw_et(Viewport * viewport, Track * trk)
{
	/* Free previous allocation. */
	if (this->ats) {
		free(this->ats);
	}

	this->ats = trk->make_elevation_time_map(this->profile_width);

	if (this->ats == NULL) {
		return;
	}

	/* Convert into appropriate units. */
	HeightUnit height_units = a_vik_get_units_height();
	if (height_units == HeightUnit::FEET) {
		/* Convert altitudes into feet units. */
		for (int i = 0; i < this->profile_width; i++) {
			this->ats[i] = VIK_METERS_TO_FEET(this->ats[i]);
		}
	}
	/* Otherwise leave in metres. */

	minmax_array(this->ats, &this->min_altitude, &this->max_altitude, true, this->profile_width);

	get_new_min_and_chunk_index(this->min_altitude, this->max_altitude, chunksa, G_N_ELEMENTS(chunksa), &this->draw_min_altitude_time, &this->ciat);

	/* Assign locally. */
	double mina = this->draw_min_altitude_time;

	this->duration = this->trk->get_duration(true);
	/* Negative time or other problem. */
	if (this->duration <= 0) {
		return;
	}

	QPixmap * pix = new QPixmap(this->profile_width + MARGIN_X, this->profile_height + MARGIN_Y);
#ifdef K
	gtk_image_set_from_pixmap(GTK_IMAGE(viewport), pix, NULL);
#endif

	/* Reset before redrawing. */
	this->clear_image(pix);

	/* Draw grid. */
	QPen fg_pen(QColor("orange"));
	QPen dark_pen(QColor("black"));
	for (int i = 0; i <= LINES; i++) {
		char s[32];

		switch (height_units) {
		case HeightUnit::METRES:
			sprintf(s, "%8dm", (int)(mina + (LINES-i)*chunksa[this->ciat]));
			break;
		case HeightUnit::FEET:
			/* NB values already converted into feet. */
			sprintf(s, "%8dft", (int)(mina + (LINES-i)*chunksa[this->ciat]));
			break;
		default:
			sprintf(s, "--");
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. height=%d\n", height_units);
		}

		this->draw_grid_y(viewport, fg_pen, dark_pen, pix, s, i);
	}

	this->draw_time_lines(viewport, pix);

	/* Draw elevations. */
	unsigned int height = this->profile_height + MARGIN_Y;
	for (int i = 0; i < this->profile_width; i++) {
#ifdef K
		gdk_draw_line(GDK_DRAWABLE(pix), dark_pen3,
			      i + MARGIN_X, height, i + MARGIN_X, height-this->profile_height*(this->ats[i]-mina)/(chunksa[this->ciat]*LINES));
#endif
	}

	/* Show DEMS. */
	if (this->w_show_elev_dem->checkState())  {
		QPen dem_alt_pen(QColor("green"));

		int h2 = this->profile_height + MARGIN_Y; /* Adjust height for x axis labelling offset. */
		int achunk = chunksa[this->ciat]*LINES;

		for (int i = 0; i < this->profile_width; i++) {
			/* This could be slow doing this each time... */
			Trackpoint * tp = this->trk->get_closest_tp_by_percentage_time(((double)i/(double)this->profile_width), NULL);
			if (tp) {
				int16_t elev = dem_cache_get_elev_by_coord(&(tp->coord), VIK_DEM_INTERPOL_SIMPLE);
				if (elev != VIK_DEM_INVALID_ELEVATION) {
					/* Convert into height units. */
					if (a_vik_get_units_height() == HeightUnit::FEET) {
						elev = VIK_METERS_TO_FEET(elev);
					}
					/* No conversion needed if already in metres. */

					/* offset is in current height units. */
					elev -= mina;

					/* consider chunk size. */
					int y_alt = h2 - ((this->profile_height * elev)/achunk);
#ifdef K
					fill_rectangle(GDK_DRAWABLE(pix), dem_alt_pen, i+MARGIN_X-2, y_alt-2, 4, 4);
#endif
				}
			}
		}
	}

	/* Show speeds. */
	if (this->w_show_elev_speed->checkState()) {
		/* This is just an indicator - no actual values can be inferred by user. */
		QPen elev_speed_pen(QColor("red"));

		double max_speed = this->max_speed * 110 / 100;

		for (int i = 0; i < this->profile_width; i++) {
			int y_speed = this->profile_height - (this->profile_height * this->speeds[i])/max_speed;
#ifdef K
			fill_rectangle(GDK_DRAWABLE(pix), elev_speed_pen, i+MARGIN_X-2, y_speed-2, 4, 4);
#endif
		}
	}
#ifdef K
	/* Draw border. */
	draw_rectangle(GDK_DRAWABLE(pix), black_pen, MARGIN_X, MARGIN_Y, this->profile_width-1, this->profile_height-1);
#endif
}




/**
 * Draw just the speed/distance image.
 */
void TrackProfileDialog::draw_sd(Viewport * viewport, Track * trk)
{
	double mins;

	/* Free previous allocation. */
	if (this->speeds_dist) {
		free(this->speeds_dist);
	}

	this->speeds_dist = trk->make_speed_dist_map(this->profile_width);
	if (this->speeds_dist == NULL) {
		return;
	}

	/* Convert into appropriate units. */
	SpeedUnit speed_units = a_vik_get_units_speed();
	for (int i = 0; i < this->profile_width; i++) {
		this->speeds_dist[i] = convert_speed_mps_to(speed_units, this->speeds_dist[i]);
	}

	QPixmap * pix = new QPixmap(this->profile_width + MARGIN_X, this->profile_height + MARGIN_Y);
#ifdef K
	gtk_image_set_from_pixmap(GTK_IMAGE(viewport), pix, NULL);
#endif

	/* OK to resuse min_speed here. */
	minmax_array(this->speeds_dist, &this->min_speed, &this->max_speed_dist, false, this->profile_width);
	if (this->min_speed < 0.0) {
		this->min_speed = 0; /* Splines sometimes give negative speeds. */
	}

	/* Find suitable chunk index. */
	get_new_min_and_chunk_index(this->min_speed, this->max_speed_dist, chunkss, G_N_ELEMENTS(chunkss), &this->draw_min_speed, &this->cisd);

	/* Assign locally. */
	mins = this->draw_min_speed;

	/* Reset before redrawing. */
	this->clear_image(pix);

	/* Draw grid. */
	QPen fg_pen(QColor("orange"));
	QPen dark_pen(QColor("black"));
	for (int i = 0; i <= LINES; i++) {
		char s[32];

		/* NB: No need to convert here anymore as numbers are in the appropriate units. */
		switch (speed_units) {
		case SpeedUnit::KILOMETRES_PER_HOUR:
			sprintf(s, "%8dkm/h", (int)(mins + (LINES-i)*chunkss[this->cisd]));
			break;
		case SpeedUnit::MILES_PER_HOUR:
			sprintf(s, "%8dmph", (int)(mins + (LINES-i)*chunkss[this->cisd]));
			break;
		case SpeedUnit::METRES_PER_SECOND:
			sprintf(s, "%8dm/s", (int)(mins + (LINES-i)*chunkss[this->cisd]));
			break;
		case SpeedUnit::KNOTS:
			sprintf(s, "%8dknots", (int)(mins + (LINES-i)*chunkss[this->cisd]));
			break;
		default:
			sprintf(s, "--");
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. speed=%d\n", speed_units);
		}

		this->draw_grid_y(viewport, fg_pen, dark_pen, pix, s, i);
	}

	draw_distance_divisions(viewport, pix, this, a_vik_get_units_distance());

	/* Draw speeds. */
	unsigned int height = this->profile_height + MARGIN_Y;
	for (int i = 0; i < this->profile_width; i++) {
#ifdef K
		gdk_draw_line(GDK_DRAWABLE(pix), dark_pen3,
			      i + MARGIN_X, height, i + MARGIN_X, height - this->profile_height*(this->speeds_dist[i]-mins)/(chunkss[this->cisd]*LINES));
#endif
	}


	if (this->w_show_sd_gps_speed->checkState()) {

		QPen gps_speed_pen(QColor("red"));

		double dist = trk->get_length_including_gaps();
		double dist_tp = 0.0;

		for (auto iter = std::next(trk->trackpointsB->begin()); iter != trk->trackpointsB->end(); iter++) {
			double gps_speed = (*iter)->speed;
			if (isnan(gps_speed)) {
				continue;
			}

			gps_speed = convert_speed_mps_to(speed_units, gps_speed);

			dist_tp += vik_coord_diff(&(*iter)->coord, &(*std::prev(iter))->coord);
			int x = MARGIN_X + (this->profile_width * dist_tp / dist);
			int y = height - this->profile_height * (gps_speed - mins)/(chunkss[this->cisd] * LINES);
#ifdef K
			fill_rectangle(GDK_DRAWABLE (pix), gps_speed_pen, x - 2, y - 2, 4, 4);
#endif
		}
	}
#ifdef K
	/* Draw border. */
	draw_rectangle(GDK_DRAWABLE(pix), black_pen, MARGIN_X, MARGIN_Y, this->profile_width-1, this->profile_height-1);
#endif
}
#undef LINES




/**
 * Draw all graphs.
 */
void TrackProfileDialog::draw_all_graphs(bool resized)
{
	/* Draw elevations. */
	if (this->elev_viewport) {
		this->draw_single_graph(this->elev_viewport, resized, &TrackProfileDialog::draw_elevations, &TrackProfileDialog::blobby_altitude, false, &this->elev_graph_saved_img);
	}

	/* Draw gradients. */
	if (this->gradient_viewport) {
		this->draw_single_graph(this->gradient_viewport, resized, &TrackProfileDialog::draw_gradients, &TrackProfileDialog::blobby_gradient, false, &this->gradient_graph_saved_img);
	}

	/* Draw speeds. */
	if (this->speed_viewport) {
		this->draw_single_graph(this->speed_viewport, resized, &TrackProfileDialog::draw_vt, &TrackProfileDialog::blobby_speed, true, &this->speed_graph_saved_img);
	}

	/* Draw Distances. */
	if (this->dist_viewport) {
		this->draw_single_graph(this->dist_viewport, resized, &TrackProfileDialog::draw_dt, &TrackProfileDialog::blobby_distance, true, &this->dist_graph_saved_img);
	}

	/* Draw Elevations in timely manner. */
	if (this->elev_time_viewport) {
		this->draw_single_graph(this->elev_time_viewport, resized, &TrackProfileDialog::draw_et, &TrackProfileDialog::blobby_altitude_time, true, &this->elev_time_graph_saved_img);
	}

	/* Draw speed distances. */
	if (this->speed_dist_viewport) {
		this->draw_single_graph(this->speed_dist_viewport, resized, &TrackProfileDialog::draw_sd, &TrackProfileDialog::blobby_speed_dist, true, &this->speed_dist_graph_saved_img);
	}
}




void TrackProfileDialog::draw_single_graph(Viewport * viewport, bool resized, void (TrackProfileDialog::*draw_graph)(Viewport *, Track *), int (TrackProfileDialog::*get_blobby)(double), bool by_time, PropSaved * saved_img)
{
#ifdef K
	/* Saved image no longer any good as we've resized, so we remove it here. */
	if (resized && saved_img->img) {
		g_object_unref(saved_img->img);
		saved_img->img = NULL;
		saved_img->saved = false;
	}
#endif

	(this->*draw_graph)(viewport, this->trk);

	/* Ensure marker or blob are redrawn if necessary. */
	if (this->is_marker_drawn || this->is_blob_drawn) {

		double pc = NAN;
		if (by_time) {
			pc = tp_percentage_by_time(this->trk, this->marker_tp);
		} else {
			pc = tp_percentage_by_distance(this->trk, this->marker_tp, this->track_length_inc_gaps);
		}

		double x_blob = -MARGIN_X - 1.0; /* i.e. don't draw unless we get a valid value. */
		int y_blob = 0;
		if (this->is_blob_drawn) {
			double pc_blob = NAN;
			if (by_time) {
				pc_blob = tp_percentage_by_time(this->trk, this->blob_tp);
			} else {
				pc_blob = tp_percentage_by_distance(this->trk, this->blob_tp, this->track_length_inc_gaps);
			}
			if (!isnan(pc_blob)) {
				x_blob = (pc_blob * this->profile_width);
			}

			y_blob = (this->*get_blobby)(x_blob);
		}

		double marker_x = -1.0; /* i.e. Don't draw unless we get a valid value. */
		if (!isnan(pc)) {
			marker_x = (pc * this->profile_width) + MARGIN_X;
		}

		QPen black_pen(QColor("black"));
		save_image_and_draw_graph_marks(viewport,
						marker_x,
						black_pen,
						x_blob+MARGIN_X,
						y_blob+MARGIN_Y,
						saved_img,
						this->profile_width,
						this->profile_height,
						&this->is_marker_drawn,
						&this->is_blob_drawn);
	}
}




/**
 * Configure/Resize the profile & speed/time images.
 */
static bool configure_event(GtkWidget * widget, GdkEventConfigure * event, TrackProfileDialog * widgets)
{
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

	/* Draw stuff. */
	draw_all_graphs(true);
#endif

	return false;
}




/**
 * Create height profile widgets including the image and callbacks.
 */
Viewport * TrackProfileDialog::create_profile(double * min_alt, double * max_alt)
{
	/* First allocation. */
	this->altitudes = this->trk->make_elevation_map(this->profile_width);

	if (this->altitudes == NULL) {
		*min_alt = *max_alt = VIK_DEFAULT_ALTITUDE;
		return NULL;
	}

	minmax_array(this->altitudes, min_alt, max_alt, true, this->profile_width);
	Viewport * viewport = new Viewport(this->parent);
	//widget->resize(this->profile_width + MARGIN_X, this->profile_height + MARGIN_Y);
	//connect(widget, "button_press_event", this, SLOT (track_profile_click_cb()));
	//connect(widget, "motion_notify_event", this, SLOT (track_profile_move_cb()));

	return viewport;
}




/**
 * Create height profile widgets including the image and callbacks.
 */
Viewport * TrackProfileDialog::create_gradient(void)
{
	/* First allocation. */
	this->gradients = this->trk->make_gradient_map(this->profile_width);

	if (this->gradients == NULL) {
		return NULL;
	}

	Viewport * viewport = new Viewport(this->parent);
	//widget->resize(this->profile_width + MARGIN_X, this->profile_height + MARGIN_Y);
	//connect(widget, "button_press_event", this, SLOT (track_gradient_click_cb()));
	//connect(widget, "motion_notify_event", this, SLOT (track_gradient_move_cb()));

	return viewport;
}




/**
 * Create speed/time widgets including the image and callbacks.
 */
Viewport * TrackProfileDialog::create_vtdiag(void)
{
	/* First allocation. */
	this->speeds = this->trk->make_speed_map(this->profile_width);
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
	//widget->resize(this->profile_width + MARGIN_X, this->profile_height + MARGIN_Y);
	//connect(widget, "button_press_event", this, SLOT (track_vt_click_cb()));
	//connect(widget, "motion_notify_event", this, SLOT (track_vt_move_cb()));

	return viewport;
}




/**
 * Create distance / time widgets including the image and callbacks.
 */
Viewport * TrackProfileDialog::create_dtdiag(void)
{
	/* First allocation. */
	this->distances = this->trk->make_distance_map(this->profile_width);
	if (this->distances == NULL) {
		return NULL;
	}

	Viewport * viewport = new Viewport(this->parent);
	//viewport->resize(this->profile_width + MARGIN_X, this->profile_height + MARGIN_Y);
	//connect(widget, "button_press_event", this, SLOT (track_dt_click_cb()));
	//connect(widget, "motion_notify_event", this, SLOT (track_dt_move_cb()));
	//g_signal_connect_swapped(G_OBJECT(widget), "destroy", G_CALLBACK(g_free), this);

	return viewport;
}




/**
 * Create elevation / time widgets including the image and callbacks.
 */
Viewport * TrackProfileDialog::create_etdiag(void)
{
	/* First allocation. */
	this->ats = this->trk->make_elevation_time_map(this->profile_width);
	if (this->ats == NULL) {
		return NULL;
	}

	Viewport * viewport = new Viewport(this->parent);
	//viewport->resize(this->profile_width + MARGIN_X, this->profile_height + MARGIN_Y);
	//connect(widget, "button_press_event", this, SLOT (track_et_click_cb()));
	//connect(widget, "motion_notify_event", this, SLOT (track_et_move_cb()));

	return viewport;
}




/**
 * Create speed/distance widgets including the image and callbacks.
 */
Viewport * TrackProfileDialog::create_sddiag(void)
{
	/* First allocation. */
	this->speeds_dist = this->trk->make_speed_dist_map(this->profile_width); // kamilFIXME
	if (this->speeds_dist == NULL) {
		return NULL;
	}

	Viewport * viewport = new Viewport(this->parent);
	viewport->resize(this->profile_width + MARGIN_X, this->profile_height + MARGIN_Y);
	//connect(widget, "button_press_event", this, SLOT (track_sd_click_cb()));
	//connect(widget, "motion_notify_event", this, SLOT (track_sd_move_cb()));

	return viewport;
}
#undef MARGIN_X




#define VIK_SETTINGS_TRACK_PROFILE_WIDTH "track_profile_display_width"
#define VIK_SETTINGS_TRACK_PROFILE_HEIGHT "track_profile_display_height"




void TrackProfileDialog::save_values(void)
{
	/* Session settings. */
	a_settings_set_integer(VIK_SETTINGS_TRACK_PROFILE_WIDTH, this->profile_width);
	a_settings_set_integer(VIK_SETTINGS_TRACK_PROFILE_HEIGHT, this->profile_height);

	/* Just for this session. */
	if (this->w_show_dem) {
		show_dem = this->w_show_dem->checkState();
	}
	if (this->w_show_alt_gps_speed) {
		show_alt_gps_speed = this->w_show_alt_gps_speed->checkState();
	}
	if (this->w_show_gps_speed) {
		show_gps_speed = this->w_show_gps_speed->checkState();
	}
	if (this->w_show_gradient_gps_speed) {
		show_gradient_gps_speed = this->w_show_gradient_gps_speed->checkState();
	}
	if (this->w_show_dist_speed) {
		show_dist_speed = this->w_show_dist_speed->checkState();
	}
	if (this->w_show_elev_dem) {
		show_elev_dem = this->w_show_elev_dem->checkState();
	}
	if (this->w_show_elev_speed) {
		show_elev_speed = this->w_show_elev_speed->checkState();
	}
	if (this->w_show_sd_gps_speed) {
		show_sd_gps_speed = this->w_show_sd_gps_speed->checkState();
	}
}




void TrackProfileDialog::destroy_cb(void) /* Slot. */
{
	this->save_values();
	//delete widgets;
}




void TrackProfileDialog::dialog_response_cb(int resp) /* Slot. */
{
	Track * trk = this->trk;
	LayerTRW * trw = this->trw;
	bool keep_dialog = false;

#ifdef K

	/* FIXME: check and make sure the track still exists before doing anything to it. */
	/* Note: destroying diaglog (eg, parent window exit) won't give "response". */
	switch (resp) {
	case GTK_RESPONSE_DELETE_EVENT: /* Received delete event (not from buttons). */
	case GTK_RESPONSE_REJECT:
		break;
	case GTK_RESPONSE_ACCEPT:
		trk->set_comment(this->w_comment->toUtf()->data());
		trk->set_description(this->w_description->toUtf()->data());
		trk->set_source(this->w_source->toUtf()->data())
		trk->set_type(this->w_type->toUtf()->data());
		gtk_color_button_get_color(GTK_COLOR_BUTTON(this->w_color), &(trk->color));
		trk->draw_name_mode = (TrackDrawnameType) gtk_combo_box_get_active(GTK_COMBO_BOX(this->w_namelabel));
		trk->max_number_dist_labels = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(this->w_number_distlabels));
		this->trw->update_treeview(this->trk);
		trw->emit_changed();
		break;
	case VIK_TRW_LAYER_PROPWIN_REVERSE:
		trk->reverse();
		trw->emit_changed();
		break;
	case VIK_TRW_LAYER_PROPWIN_DEL_DUP:
		trk->remove_dup_points(); /* NB ignore the returned answer. */
		/* As we could have seen the nuber of dulplicates that would be deleted in the properties statistics tab,
		   choose not to inform the user unnecessarily. */

		/* Above operation could have deleted current_tp or last_tp. */
		trw->cancel_tps_of_track(trk);
		trw->emit_changed();
		break;
	case VIK_TRW_LAYER_PROPWIN_SPLIT: {
		/* Get new tracks, add them and then the delete old one. old can still exist on clipboard. */
		std::list<Track *> * tracks = trk->split_into_segments();
		char *new_tr_name;
		for (auto iter = tracks->begin(); iter != tracks->end(); iter++) {
			if (*iter) {
				new_tr_name = trw->new_unique_sublayer_name(this->trk->is_route ? SublayerType::ROUTE : SublayerType::TRACK,
									    this->trk->name);
				if (this->trk->is_route) {
					trw->add_route(*iter, new_tr_name);
				} else {
					trw->add_track(*iter, new_tr_name);
				}
				(*iter)->calculate_bounds();

				free(new_tr_name);
			}
		}
		if (tracks) {
			delete tracks;
			/* Don't let track destroy this dialog. */
			trk->clear_property_dialog();
			if (this->trk->is_route) {
				trw->delete_route(trk);
			} else {
				trw->delete_track(trk);
			}
			trw->emit_changed(); /* Chase thru the hoops. */
		}
	}
		break;
	case VIK_TRW_LAYER_PROPWIN_SPLIT_MARKER: {
		auto iter = std::next(trk->begin());
		while (iter != trk->end()) {
			if (this->marker_tp == *iter) {
				break;
			}
			iter++;
		}
		if (iter == trk->end()) {
			dialog_error(QString(_("Failed to split track. Track unchanged")), trw->get_window());
			keep_dialog = true;
			break;
		}

		char *r_name = trw->new_unique_sublayer_name(this->trk->is_route ? SublayerType::ROUTE : SublayerType::TRACK,
							     this->trk->name);


		/* Notice that here Trackpoint pointed to by iter is moved to new track. */
		/* kamilTODO: originally the constructor was just Track(). Should we really pass original trk to constructor? */
		Track * trk_right = new Track(*trk, iter, trk->end());
		trk->erase(iter, trk->end());

		if (trk->comment) {
			trk_right->set_comment(trk->comment);
		}
		trk_right->visible = trk->visible;
		trk_right->is_route = trk->is_route;

		if (this->trk->is_route) {
			trw->add_route(trk_right, r_name);
		} else {
			trw->add_track(trk_right, r_name);
		}
		trk->calculate_bounds();
		trk_right->calculate_bounds();

		free(r_name);

		trw->emit_changed();
	}
		break;
	default:
		fprintf(stderr, "DEBUG: unknown response\n");
		return;
	}

	/* Keep same behaviour for now: destroy dialog if click on any button. */
	if (!keep_dialog) {
		trk->clear_property_dialog();
		gtk_widget_destroy(GTK_WIDGET(dialog));
	}
#endif
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
QWidget * TrackProfileDialog::create_graph_page(QWidget * graph,
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

	graph->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	vbox->addWidget(graph);
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
	QLayout * old = widget->layout();
	delete old;
	widget->setLayout(vbox);

	return widget;

}




static GtkWidget * create_table(int cnt, char * labels[], GtkWidget * contents[])
{
#ifdef K
	GtkTable * table;

	table = GTK_TABLE(gtk_table_new(cnt, 2, false));
	gtk_table_set_col_spacing(table, 0, 10);
	for (int i = 0; i < cnt; i++) {
		GtkWidget *label;

		/* Settings so the text positioning only moves around vertically when the dialog is resized.
		   This also gives more room to see the track comment. */
		label = gtk_label_new(NULL);
		gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5); // Position text centrally in vertical plane
		gtk_label_set_markup(GTK_LABEL(label), _(labels[i]));
		gtk_table_attach(table, label, 0, 1, i, i+1, GTK_FILL, GTK_SHRINK, 0, 0);
		if (GTK_IS_MISC(contents[i])) {
			gtk_misc_set_alignment(GTK_MISC(contents[i]), 0, 0.5);
		}
		if (GTK_IS_COLOR_BUTTON(contents[i]) || GTK_IS_COMBO_BOX(contents[i])) {
			/* Buttons compressed - otherwise look weird (to me) if vertically massive. */
			gtk_table_attach(table, contents[i], 1, 2, i, i+1, GTK_FILL, GTK_SHRINK, 0, 5);
		} else {
			/* Expand for comments + descriptions / labels. */
			gtk_table_attach_defaults(table, contents[i], 1, 2, i, i+1);
		}
	}

	return GTK_WIDGET (table);
#endif
}




void SlavGPS::vik_trw_layer_propwin_run(Window * parent,
					LayerTRW * layer,
					Track * trk,
					void * panel,
					Viewport * viewport,
					bool start_on_stats)
{
	TrackProfileDialog dialog(QString("Track Profile"), layer, trk, panel, viewport, parent);
	dialog.exec();
}




TrackProfileDialog::TrackProfileDialog(QString const & title, LayerTRW * a_layer, Track * a_trk, void * a_panel, Viewport * a_viewport, Window * a_parent) : QDialog(a_parent)
{
	this->setWindowTitle(QString(_("%1 - Track Properties")).arg(a_trk->name));

	this->trw = a_layer;
	this->trk = a_trk;
	this->panel = a_panel;
	this->viewport = a_viewport;
	this->parent = a_parent;


	int profile_size_value;
	/* Ensure minimum values. */
	this->profile_width = 600;
	if (a_settings_get_integer(VIK_SETTINGS_TRACK_PROFILE_WIDTH, &profile_size_value)) {
		if (profile_size_value > this->profile_width) {
			this->profile_width = profile_size_value;
		}
	}

	this->profile_height = 300;
	if (a_settings_get_integer(VIK_SETTINGS_TRACK_PROFILE_HEIGHT, &profile_size_value)) {
		if (profile_size_value > this->profile_height) {
			this->profile_height = profile_size_value;
		}
	}

#ifdef K

	GtkWidget * dialog = gtk_dialog_new_with_buttons(title,
							 parent,
							 (GtkDialogFlags) (GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR),
							 GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
							 _("Split at _Marker"), VIK_TRW_LAYER_PROPWIN_SPLIT_MARKER,
							 _("Split _Segments"), VIK_TRW_LAYER_PROPWIN_SPLIT,
							 _("_Reverse"),        VIK_TRW_LAYER_PROPWIN_REVERSE,
							 _("_Delete Dupl."),   VIK_TRW_LAYER_PROPWIN_DEL_DUP,
							 GTK_STOCK_OK,     GTK_RESPONSE_ACCEPT,
							 NULL);
	widgets->dialog = dialog;
	g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(dialog_response_cb), widgets);

#endif

	double tr_len;
	unsigned long tp_count;
	unsigned int seg_count;

	double min_alt, max_alt;
	this->elev_viewport = this->create_profile(&min_alt, &max_alt);
	this->gradient_viewport = this->create_gradient();
	this->speed_viewport = this->create_vtdiag();
	this->dist_viewport = this->create_dtdiag();
	this->elev_time_viewport = this->create_etdiag();
	this->speed_dist_viewport = this->create_sddiag();
	this->tabs = new QTabWidget();

	QWidget * content_prop[20] = { 0 };
	int cnt_prop = 0;

	static char *label_texts[] = {
		(char *) N_("<b>Comment:</b>"),
		(char *) N_("<b>Description:</b>"),
		(char *) N_("<b>Source:</b>"),
		(char *) N_("<b>Type:</b>"),
		(char *) N_("<b>Color:</b>"),
		(char *) N_("<b>Draw Name:</b>"),
		(char *) N_("<b>Distance Labels:</b>"),
	};
	static char *stats_texts[] = {
		(char *) N_("<b>Track Length:</b>"),
		(char *) N_("<b>Trackpoints:</b>"),
		(char *) N_("<b>Segments:</b>"),
		(char *) N_("<b>Duplicate Points:</b>"),
		(char *) N_("<b>Max Speed:</b>"),
		(char *) N_("<b>Avg. Speed:</b>"),
		(char *) N_("<b>Moving Avg. Speed:</b>"),
		(char *) N_("<b>Avg. Dist. Between TPs:</b>"),
		(char *) N_("<b>Elevation Range:</b>"),
		(char *) N_("<b>Total Elevation Gain/Loss:</b>"),
		(char *) N_("<b>Start:</b>"),
		(char *) N_("<b>End:</b>"),
		(char *) N_("<b>Duration:</b>"),
	};
	static char tmp_buf[50];
	double tmp_speed;
#if 0
	/* Properties. */
	this->w_comment = new QLineEdit(this);
	if (trk->comment) {
		this->w_comment->insert(trk->comment);
	}
	content_prop[cnt_prop++] = this->w_comment;

	this->w_description = new QLineEdit(this);
	if (trk->description) {
		this->w_description->insert(trk->description);
	}
	content_prop[cnt_prop++] = this->w_description;

	this->w_source = new QLineEdit(this);
	if (trk->source) {
		this->w_source->insert(trk->source);
	}
	content_prop[cnt_prop++] = this->w_source;

	this->w_type = new QLineEdit(this);
	if (trk->type) {
		this->w_type->insert(trk->type);
	}
	content_prop[cnt_prop++] = this->w_type;
#endif

#ifdef K

	widgets->w_color = content_prop[cnt_prop++] = gtk_color_button_new_with_color(&(trk->color));

	static char * draw_name_labels[] = {
		(char *) N_("No"),
		(char *) N_("Centre"),
		(char *) N_("Start only"),
		(char *) N_("End only"),
		(char *) N_("Start and End"),
		(char *) N_("Centre, Start and End"),
		NULL
	};

	widgets->w_namelabel = content_prop[cnt_prop++] = vik_combo_box_text_new();
	char **pstr = draw_name_labels;
	while (*pstr) {
		vik_combo_box_text_append(widgets->w_namelabel, *(pstr++));
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(widgets->w_namelabel), trk->draw_name_mode);

	widgets->w_number_distlabels = content_prop[cnt_prop++] =
		gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(trk->max_number_dist_labels, 0, 100, 1, 1, 0)), 1, 0);
	gtk_widget_set_tooltip_text(GTK_WIDGET(widgets->w_number_distlabels), _("Maximum number of distance labels to be shown"));

	GtkWidget * table = create_table(cnt_prop, label_texts, content_prop);

	this->tabs->addTab(table, _("Properties"));

	/* Statistics. */
	GtkWidget *content[20];
	int cnt = 0;

	DistanceUnit distance_unit = a_vik_get_units_distance();

	/* NB This value not shown yet - but is used by internal calculations. */
	widgets->track_length_inc_gaps = trk->get_length_including_gaps();

	tr_len = widgets->track_length = trk->get_length();

	get_distance_string(tmp_buf, sizeof (tmp_buf), distance_unit, tr_len);
	widgets->w_track_length = content[cnt++] = ui_label_new_selectable(tmp_buf, this);

	tp_count = trk->get_tp_count();
	snprintf(tmp_buf, sizeof(tmp_buf), "%lu", tp_count);
	widgets->w_tp_count = content[cnt++] = ui_label_new_selectable(tmp_buf, this);

	seg_count = trk->get_segment_count() ;
	snprintf(tmp_buf, sizeof(tmp_buf), "%u", seg_count);
	widgets->w_segment_count = content[cnt++] = ui_label_new_selectable(tmp_buf, this);

	snprintf(tmp_buf, sizeof(tmp_buf), "%lu", trk->get_dup_point_count());
	widgets->w_duptp_count = content[cnt++] = ui_label_new_selectable(tmp_buf, this);

	SpeedUnit speed_units = a_vik_get_units_speed();
	tmp_speed = trk->get_max_speed();
	if (tmp_speed == 0) {
		snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
	} else {
		get_speed_string(tmp_buf, sizeof (tmp_buf), speed_units, tmp_speed);
	}
	widgets->w_max_speed = content[cnt++] = ui_label_new_selectable(tmp_buf, this);

	tmp_speed = trk->get_average_speed();
	if (tmp_speed == 0) {
		snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
	} else {
		get_speed_string(tmp_buf, sizeof (tmp_buf), speed_units, tmp_speed);
	}
	widgets->w_avg_speed = content[cnt++] = ui_label_new_selectable(tmp_buf, this);

	/* Use 60sec as the default period to be considered stopped.
	   This is the TrackWaypoint draw stops default value 'trw->stop_length'.
	   However this variable is not directly accessible - and I don't expect it's often changed from the default
	   so ATM just put in the number. */
	tmp_speed = trk->get_average_speed_moving(60);
	if (tmp_speed == 0) {
		snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
	} else {
		get_speed_string(tmp_buf, sizeof (tmp_buf), speed_units, tmp_speed);
	}
	widgets->w_mvg_speed = content[cnt++] = ui_label_new_selectable(tmp_buf, this);

	switch (distance_unit) {
	case DistanceUnit::KILOMETRES:
		/* Even though kilometres, the average distance between points is going to be quite small so keep in metres. */
		snprintf(tmp_buf, sizeof(tmp_buf), "%.2f m", (tp_count - seg_count) == 0 ? 0 : tr_len / (tp_count - seg_count));
		break;
	case DistanceUnit::MILES:
		snprintf(tmp_buf, sizeof(tmp_buf), "%.3f miles", (tp_count - seg_count) == 0 ? 0 : VIK_METERS_TO_MILES(tr_len / (tp_count - seg_count)));
		break;
	case DistanceUnit::NAUTICAL_MILES:
		snprintf(tmp_buf, sizeof(tmp_buf), "%.3f NM", (tp_count - seg_count) == 0 ? 0 : VIK_METERS_TO_NAUTICAL_MILES(tr_len / (tp_count - seg_count)));
		break;
	default:
		fprintf(stderr, "CRITICAL: Houston, we've had a problem. distance=%d\n", distance_unit);
	}
	widgets->w_avg_dist = content[cnt++] = ui_label_new_selectable(tmp_buf, this);

	HeightUnit height_units = a_vik_get_units_height();
	if (min_alt == VIK_DEFAULT_ALTITUDE) {
		snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
	} else {
		switch (height_units) {
		case HeightUnit::METRES:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.0f m - %.0f m", min_alt, max_alt);
			break;
		case HeightUnit::FEET:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.0f feet - %.0f feet", VIK_METERS_TO_FEET(min_alt), VIK_METERS_TO_FEET(max_alt));
			break;
		default:
			snprintf(tmp_buf, sizeof(tmp_buf), "--");
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. height=%d\n", height_units);
		}
	}
	widgets->w_elev_range = content[cnt++] = ui_label_new_selectable(tmp_buf, this);

	trk->get_total_elevation_gain(&max_alt, &min_alt);
	if (min_alt == VIK_DEFAULT_ALTITUDE) {
		snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
	} else {
		switch (height_units) {
		case HeightUnit::METRES:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.0f m / %.0f m", max_alt, min_alt);
			break;
		case HeightUnit::FEET:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.0f feet / %.0f feet", VIK_METERS_TO_FEET(max_alt), VIK_METERS_TO_FEET(min_alt));
			break;
		default:
			snprintf(tmp_buf, sizeof(tmp_buf), "--");
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. height=%d\n", height_units);
		}
	}
	widgets->w_elev_gain = content[cnt++] = ui_label_new_selectable(tmp_buf, this);

#if 0
#define PACK(w) gtk_box_pack_start(GTK_BOX(right_vbox), w, false, false, 0);
	gtk_box_pack_start(GTK_BOX(right_vbox), e_cmt, false, false, 0);
	PACK(l_len);
	PACK(l_tps);
	PACK(l_segs);
	PACK(l_dups);
	PACK(l_maxs);
	PACK(l_avgs);
	PACK(l_avgd);
	PACK(l_elev);
	PACK(l_galo);
#undef PACK;
#endif

	if (!trk->empty()
	    && (*trk->trackpointsB->begin())->timestamp) {

		time_t t1 = (*trk->trackpointsB->begin())->timestamp;
		time_t t2 = (*std::prev(trk->trackpointsB->end()))->timestamp;

		VikCoord vc;
		/* Notional center of a track is simply an average of the bounding box extremities. */
		struct LatLon center = { (trk->bbox.north+trk->bbox.south)/2, (trk->bbox.east+trk->bbox.west)/2 };
		vik_coord_load_from_latlon(&vc, layer->get_coord_mode(), &center);

		widgets->tz = vu_get_tz_at_location(&vc);

		char *msg;
		msg = vu_get_time_string(&t1, "%c", &vc, widgets->tz);
		widgets->w_time_start = content[cnt++] = ui_label_new_selectable(msg, this);
		free(msg);

		msg = vu_get_time_string(&t2, "%c", &vc, widgets->tz);
		widgets->w_time_end = content[cnt++] = ui_label_new_selectable(msg, this);
		free(msg);

		int total_duration_s = (int)(t2-t1);
		int segments_duration_s = (int) trk->get_duration(false);
		int total_duration_m = total_duration_s/60;
		int segments_duration_m = segments_duration_s/60;
		snprintf(tmp_buf, sizeof(tmp_buf), _("%d minutes - %d minutes moving"), total_duration_m, segments_duration_m);
		widgets->w_time_dur = content[cnt++] = ui_label_new_selectable(tmp_buf, this);

		/* A tooltip to show in more readable hours:minutes. */
		char tip_buf_total[20];
		unsigned int h_tot = total_duration_s/3600;
		unsigned int m_tot = (total_duration_s - h_tot*3600)/60;
		snprintf(tip_buf_total, sizeof(tip_buf_total), "%d:%02d", h_tot, m_tot);
		char tip_buf_segments[20];
		unsigned int h_seg = segments_duration_s/3600;
		unsigned int m_seg = (segments_duration_s - h_seg*3600)/60;
		snprintf(tip_buf_segments, sizeof(tip_buf_segments), "%d:%02d", h_seg, m_seg);
		char *tip = g_strdup_printf(_("%s total - %s in segments"), tip_buf_total, tip_buf_segments);
		gtk_widget_set_tooltip_text(GTK_WIDGET(widgets->w_time_dur), tip);
		free(tip);
	} else {
		widgets->w_time_start = content[cnt++] = gtk_label_new(_("No Data"));
		widgets->w_time_end = content[cnt++] = gtk_label_new(_("No Data"));
		widgets->w_time_dur = content[cnt++] = gtk_label_new(_("No Data"));
	}

	table = create_table(cnt, stats_texts, content);
	this->tabs->addTab(table, _("Statistics"));
#endif

	if (this->elev_viewport) {
		this->w_cur_dist = ui_label_new_selectable(_("No Data"), this);
		this->w_cur_elevation = ui_label_new_selectable(_("No Data"), this);
		this->w_show_dem = new QCheckBox(_("Show D&EM"), this);
		this->w_show_alt_gps_speed = new QCheckBox(_("Show &GPS Speed"), this);
		QWidget * page = this->create_graph_page(this->elev_viewport,
							 _("Track Distance:"), this->w_cur_dist,
							 _("Track Height:"), this->w_cur_elevation,
							 NULL, NULL,
							 this->w_show_dem, show_dem,
							 this->w_show_alt_gps_speed, show_alt_gps_speed);
		connect(this->w_show_dem, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		connect(this->w_show_alt_gps_speed, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		this->tabs->addTab(page, _("Elevation-distance"));
	}

	if (this->gradient_viewport) {
		this->w_cur_gradient_dist = ui_label_new_selectable(_("No Data"), this);
		this->w_cur_gradient_gradient = ui_label_new_selectable(_("No Data"), this);
		this->w_show_gradient_gps_speed = new QCheckBox(_("Show &GPS Speed"), this);
		QWidget * page = this->create_graph_page(this->gradient_viewport,
							 _("Track Distance:"), this->w_cur_gradient_dist,
							 _("Track Gradient:"), this->w_cur_gradient_gradient,
							 NULL, NULL,
							 this->w_show_gradient_gps_speed, show_gradient_gps_speed,
							 NULL, false);
		connect(this->w_show_gradient_gps_speed, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		this->tabs->addTab(page, _("Gradient-distance"));
	}

	if (this->speed_viewport) {
		this->w_cur_time = ui_label_new_selectable(_("No Data"), this);
		this->w_cur_speed = ui_label_new_selectable(_("No Data"), this);
		this->w_cur_time_real = ui_label_new_selectable(_("No Data"), this);
		this->w_show_gps_speed = new QCheckBox(_("Show &GPS Speed"), this);
		QWidget * page = this->create_graph_page(this->speed_viewport,
							 _("Track Time:"), this->w_cur_time,
							 _("Track Speed:"), this->w_cur_speed,
							 _("Time/Date:"), this->w_cur_time_real,
							 this->w_show_gps_speed, show_gps_speed,
							 NULL, false);
		connect(this->w_show_gps_speed, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		this->tabs->addTab(page, _("Speed-time"));
	}

	if (this->dist_viewport) {
		this->w_cur_dist_time = ui_label_new_selectable(_("No Data"), this);
		this->w_cur_dist_dist = ui_label_new_selectable(_("No Data"), this);
		this->w_cur_dist_time_real = ui_label_new_selectable(_("No Data"), this);
		this->w_show_dist_speed = new QCheckBox(_("Show S&peed"), this);
		QWidget * page = this->create_graph_page(this->dist_viewport,
							 _("Track Distance:"), this->w_cur_dist_dist,
							 _("Track Time:"), this->w_cur_dist_time,
							 _("Time/Date:"), this->w_cur_dist_time_real,
							 this->w_show_dist_speed, show_dist_speed,
							 NULL, false);
		connect(this->w_show_dist_speed, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		this->tabs->addTab(page, _("Distance-time"));
	}

	if (this->elev_time_viewport) {
		this->w_cur_elev_time = ui_label_new_selectable(_("No Data"), this);
		this->w_cur_elev_elev = ui_label_new_selectable(_("No Data"), this);
		this->w_cur_elev_time_real = ui_label_new_selectable(_("No Data"), this);
		this->w_show_elev_speed = new QCheckBox(_("Show S&peed"), this);
		this->w_show_elev_dem = new QCheckBox(_("Show D&EM"), this);
		QWidget * page = this->create_graph_page(this->elev_time_viewport,
							 _("Track Time:"), this->w_cur_elev_time,
							 _("Track Height:"), this->w_cur_elev_elev,
							 _("Time/Date:"), this->w_cur_elev_time_real,
							 this->w_show_elev_dem, show_elev_dem,
							 this->w_show_elev_speed, show_elev_speed);
		connect(this->w_show_elev_dem, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		connect(this->w_show_elev_speed, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		this->tabs->addTab(page, _("Elevation-time"));
	}

	if (this->speed_dist_viewport) {
		this->w_cur_speed_dist = ui_label_new_selectable(_("No Data"), this);
		this->w_cur_speed_speed = ui_label_new_selectable(_("No Data"), this);
		this->w_show_sd_gps_speed = new QCheckBox(_("Show &GPS Speed"), this);
		QWidget * page = this->create_graph_page(this->speed_dist_viewport,
							 _("Track Distance:"), this->w_cur_speed_dist,
							 _("Track Speed:"), this->w_cur_speed_speed,
							 NULL, NULL,
							 this->w_show_sd_gps_speed, show_sd_gps_speed,
							 NULL, false);
		connect(this->w_show_sd_gps_speed, SIGNAL (stateChanged(int)), this, SLOT (checkbutton_toggle_cb()));
		this->tabs->addTab(page, _("Speed-distance"));
	}

	QLayout * old = this->layout();
	delete old;
	QVBoxLayout * vbox = new QVBoxLayout;
	this->setLayout(vbox);
	vbox->addWidget(this->tabs);

#ifdef K

	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), this->tabs, false, false, 0);

	gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), VIK_TRW_LAYER_PROPWIN_SPLIT_MARKER, false);
	if (seg_count <= 1) {
		gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), VIK_TRW_LAYER_PROPWIN_SPLIT, false);
	}
	if (trk->get_dup_point_count() <= 0) {
		gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), VIK_TRW_LAYER_PROPWIN_DEL_DUP, false);
	}

	/* On dialog realization configure_event causes the graphs to be initially drawn. */
	widgets->configure_dialog = true;
	g_signal_connect(G_OBJECT(dialog), "configure-event", G_CALLBACK (configure_event), widgets);

	g_signal_connect(G_OBJECT(dialog), "destroy", G_CALLBACK (destroy_cb), widgets);

	trk->set_property_dialog(dialog);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	gtk_widget_show_all(dialog);

	/* Gtk note: due to historical reasons, this must be done after widgets are shown. */
	if (start_on_stats) {
		gtk_notebook_set_current_page(GTK_NOTEBOOK(this->tabs), 1);
	}
#endif
}


/**
 * Update this property dialog
 * e.g. if the track has been renamed.
 */
void SlavGPS::vik_trw_layer_propwin_update(Track * trk)
{
	/* If not displayed do nothing. */
	if (!trk->property_dialog) {
		return;
	}

	/* Update title with current name. */
	if (trk->name) {
#ifdef K
		char * title = g_strdup_printf(_("%s - Track Properties"), trk->name);
		gtk_window_set_title(GTK_WINDOW(trk->property_dialog), title);
		free(title);
#endif
	}

}
