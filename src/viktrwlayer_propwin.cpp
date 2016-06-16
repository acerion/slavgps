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

#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <time.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include "viktrwlayer.h"
#include "viktrwlayer_propwin.h"
#include "dems.h"
#include "viking.h"
#include "vikviewport.h" /* ugh */
#include "vikutils.h"
#include "ui_util.h"
#include "dialog.h"
#include "settings.h"
#include "globals.h"
#include <gdk-pixbuf/gdk-pixdata.h>


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

/* (Hopefully!) Human friendly altitude grid sizes - note no fixed 'ratio' just numbers that look nice...*/
static const double chunksa[] = {2.0, 5.0, 10.0, 15.0, 20.0,
				 25.0, 40.0, 50.0, 75.0, 100.0,
				 150.0, 200.0, 250.0, 375.0, 500.0,
				 750.0, 1000.0, 2000.0, 5000.0, 10000.0, 100000.0};

/* (Hopefully!) Human friendly gradient grid sizes - note no fixed 'ratio' just numbers that look nice...*/
static const double chunksg[] = {1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 10.0,
				 12.0, 15.0, 20.0, 25.0, 30.0, 35.0, 40.0, 45.0, 50.0, 75.0,
				 100.0, 150.0, 200.0, 250.0, 375.0, 500.0,
				  750.0, 1000.0, 10000.0, 100000.0};
// Normally gradients should range up to couple hundred precent at most,
//  however there are possibilities of having points with no altitude after a point with a big altitude
//  (such as places with invalid DEM values in otherwise mountainous regions) - thus giving huge negative gradients.

/* (Hopefully!) Human friendly grid sizes - note no fixed 'ratio' just numbers that look nice...*/
/* As need to cover walking speeds - have many low numbers (but also may go up to airplane speeds!) */
static const double chunkss[] = {1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 10.0,
				 15.0, 20.0, 25.0, 40.0, 50.0, 75.0,
				 100.0, 150.0, 200.0, 250.0, 375.0, 500.0,
				 750.0, 1000.0, 10000.0};

/* (Hopefully!) Human friendly distance grid sizes - note no fixed 'ratio' just numbers that look nice...*/
static const double chunksd[] = {0.1, 0.2, 0.5, 1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 10.0,
				 15.0, 20.0, 25.0, 40.0, 50.0, 75.0,
				 100.0, 150.0, 200.0, 250.0, 375.0, 500.0,
				 750.0, 1000.0, 10000.0};

// Time chunks in seconds
static const time_t chunkst[] = {
	60,     // 1 minute
	120,    // 2 minutes
	300,    // 5 minutes
	900,    // 15 minutes
	1800,   // half hour
	3600,   // 1 hour
	10800,  // 3 hours
	21600,  // 6 hours
	43200,  // 12 hours
	86400,  // 1 day
	172800, // 2 days
	604800, // 1 week
	1209600,// 2 weeks
	2419200,// 4 weeks
};

// Local show settings to restore on dialog opening
static bool show_dem                = true;
static bool show_alt_gps_speed      = true;
static bool show_gps_speed          = true;
static bool show_gradient_gps_speed = true;
static bool show_dist_speed         = false;
static bool show_elev_speed         = false;
static bool show_elev_dem           = false;
static bool show_sd_gps_speed       = true;

typedef struct _propsaved {
	bool saved;
	GdkImage * img;
} PropSaved;

typedef struct _propwidgets {
	bool  configure_dialog;
	LayerTRW * trw;
	Track * trk;
	Viewport * viewport;
	LayersPanel * panel;
	int      profile_width;
	int      profile_height;
	int      profile_width_old;
	int      profile_height_old;
	int      profile_width_offset;
	int      profile_height_offset;
	GtkWidget *dialog;
	GtkWidget *w_comment;
	GtkWidget *w_description;
	GtkWidget *w_source;
	GtkWidget *w_type;
	GtkWidget *w_track_length;
	GtkWidget *w_tp_count;
	GtkWidget *w_segment_count;
	GtkWidget *w_duptp_count;
	GtkWidget *w_max_speed;
	GtkWidget *w_avg_speed;
	GtkWidget *w_mvg_speed;
	GtkWidget *w_avg_dist;
	GtkWidget *w_elev_range;
	GtkWidget *w_elev_gain;
	GtkWidget *w_time_start;
	GtkWidget *w_time_end;
	GtkWidget *w_time_dur;
	GtkWidget *w_color;
	GtkWidget *w_namelabel;
	GtkWidget *w_number_distlabels;
	GtkWidget *w_cur_dist; /*< Current distance */
	GtkWidget *w_cur_elevation;
	GtkWidget *w_cur_gradient_dist; /*< Current distance on gradient graph */
	GtkWidget *w_cur_gradient_gradient; /*< Current gradient on gradient graph */
	GtkWidget *w_cur_time; /*< Current track time */
	GtkWidget *w_cur_time_real; /*< Actual time as on a clock */
	GtkWidget *w_cur_speed;
	GtkWidget *w_cur_dist_dist; /*< Current distance on distance graph */
	GtkWidget *w_cur_dist_time; /*< Current track time on distance graph */
	GtkWidget *w_cur_dist_time_real; // Clock time
	GtkWidget *w_cur_elev_elev;
	GtkWidget *w_cur_elev_time; // Track time
	GtkWidget *w_cur_elev_time_real; // Clock time
	GtkWidget *w_cur_speed_dist;
	GtkWidget *w_cur_speed_speed;
	GtkWidget *w_show_dem;
	GtkWidget *w_show_alt_gps_speed;
	GtkWidget *w_show_gps_speed;
	GtkWidget *w_show_gradient_gps_speed;
	GtkWidget *w_show_dist_speed;
	GtkWidget *w_show_elev_speed;
	GtkWidget *w_show_elev_dem;
	GtkWidget *w_show_sd_gps_speed;
	double   track_length;
	double   track_length_inc_gaps;
	PropSaved elev_graph_saved_img;
	PropSaved gradient_graph_saved_img;
	PropSaved speed_graph_saved_img;
	PropSaved dist_graph_saved_img;
	PropSaved elev_time_graph_saved_img;
	PropSaved speed_dist_graph_saved_img;
	GtkWidget *elev_box;
	GtkWidget *gradient_box;
	GtkWidget *speed_box;
	GtkWidget *dist_box;
	GtkWidget *elev_time_box;
	GtkWidget *speed_dist_box;
	double   *altitudes;
	double   *ats; // altitudes in time
	double   min_altitude;
	double   max_altitude;
	double   draw_min_altitude;
	double   draw_min_altitude_time;
	int      cia; // Chunk size Index into Altitudes
	int      ciat; // Chunk size Index into Altitudes / Time
	// NB cia & ciat are normally same value but sometimes not due to differing methods of altitude array creation
	//    thus also have draw_min_altitude for each altitude graph type
	double   *gradients;
	double   min_gradient;
	double   max_gradient;
	double   draw_min_gradient;
	int      cig; // Chunk size Index into Gradients
	double   *speeds;
	double   *speeds_dist;
	double   min_speed;
	double   max_speed;
	double   draw_min_speed;
	double   max_speed_dist;
	int      cis; // Chunk size Index into Speeds
	int      cisd; // Chunk size Index into Speed/Distance
	double   *distances;
	int      cid; // Chunk size Index into Distance
	Trackpoint * marker_tp;
	bool  is_marker_drawn;
	Trackpoint * blob_tp;
	bool  is_blob_drawn;
	time_t    duration;
	char     *tz; // TimeZone at track's location
} PropWidgets;

typedef void (* draw_graph_fn_t)(GtkWidget * image, Track * trk, PropWidgets * widgets);
typedef int (* get_blobby_fn_t)(double x_blob, PropWidgets * widgets);

static void get_mouse_event_x(GtkWidget * event_box, GdkEventMotion * event, PropWidgets * widgets, double * x, int * ix);
static void draw_single_graph(GtkWidget * window, PropWidgets * widgets, bool resized, GList * child, draw_graph_fn_t draw_graph, get_blobby_fn_t get_blobby, bool by_time, PropSaved * saved_img);
static void distance_label_update(GtkWidget * widget, double meters_from_start);
static void elevation_label_update(GtkWidget * widget, Trackpoint * tp);
static void real_time_label_update(GtkWidget * widget, Trackpoint * tp);
static void speed_label_update(GtkWidget * widget, double value);
static void dist_dist_label_update(GtkWidget * widget, double distance);
static void gradient_label_update(GtkWidget * widget, double gradient);

static PropWidgets * prop_widgets_new()
{
	PropWidgets * widgets = (PropWidgets *) malloc(sizeof (PropWidgets));
	memset(widgets, 0, sizeof (PropWidgets));

	return widgets;
}

static void prop_widgets_free(PropWidgets * widgets)
{
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
}

static void minmax_array(const double * array, double * min, double * max, bool NO_ALT_TEST, int PROFILE_WIDTH)
{
	*max = -1000;
	*min = 20000;
	unsigned int i;
	for (i=0; i < PROFILE_WIDTH; i++) {
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
 * get_new_min_and_chunk_index:
 * Returns via pointers:
 *   the new minimum value to be used for the graph
 *   the index in to the chunk sizes array (ci = Chunk Index)
 */
static void get_new_min_and_chunk_index(double mina, double maxa, const double * chunks, size_t chunky, double * new_min, int * ci)
{
	/* Get unitized chunk */
	/* Find suitable chunk index */
	*ci = 0;
	double diff_chunk = (maxa - mina)/LINES;

	/* Loop through to find best match */
	while (diff_chunk > chunks[*ci]) {
		(*ci)++;
		/* Last Resort Check */
		if (*ci == chunky) {
			// Use previous value and exit loop
			(*ci)--;
			break;
		}
	}

	/* Ensure adjusted minimum .. maximum covers mina->maxa */

	// Now work out adjusted minimum point to the nearest lowest chunk divisor value
	// When negative ensure logic uses lowest value
	if (mina < 0) {
		*new_min = (double) ((int)((mina - chunks[*ci]) / chunks[*ci]) * chunks[*ci]);
	} else {
		*new_min = (double) ((int)(mina / chunks[*ci]) * chunks[*ci]);
	}

	// Range not big enough - as new minimum has lowered
	if ((*new_min + (chunks[*ci] * LINES) < maxa)) {
		// Next chunk should cover it
		if (*ci < chunky-1) {
			(*ci)++;
			// Remember to adjust the minimum too...
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
	// Grid split
	time_t myduration = duration / LINES;

	// Search nearest chunk index
	unsigned int ci = 0;
	unsigned int last_chunk = G_N_ELEMENTS(chunkst);

	// Loop through to find best match
	while (myduration > chunkst[ci]) {
		ci++;
		// Last Resort Check
		if (ci == last_chunk) {
			break;
		}
	}
	// Use previous value
	if (ci != 0) {
		ci--;
	}

	return ci;
}

/**
 *
 */
static unsigned int get_distance_chunk_index(double length)
{
	// Grid split
	double mylength = length / LINES;

	// Search nearest chunk index
	unsigned int ci = 0;
	unsigned int last_chunk = G_N_ELEMENTS(chunksd);

	// Loop through to find best match
	while (mylength > chunksd[ci]) {
		ci++;
		// Last Resort Check
		if (ci == last_chunk) {
			break;
		}
	}
	// Use previous value
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
			panel->emit_update();
		} else {
			/* since panel not set, viewport should be valid instead! */
			if (viewport) {
				viewport->set_center_coord(&coord, true);
			}
			trw->emit_update();
		}
	}
	return tp;
}

/**
 * Returns whether the marker was drawn or not and whether the blob was drawn or not
 */
static void save_image_and_draw_graph_marks(GtkWidget * image,
					    double marker_x,
					    GdkGC *gc,
					    int blob_x,
					    int blob_y,
					    PropSaved *saved_img,
					    int PROFILE_WIDTH,
					    int PROFILE_HEIGHT,
					    bool *marker_drawn,
					    bool *blob_drawn)
{
	GdkPixmap *pix = NULL;
	/* the pixmap = margin + graph area */
	gtk_image_get_pixmap(GTK_IMAGE(image), &pix, NULL);

	/* Restore previously saved image */
	if (saved_img->saved) {
		gdk_draw_image(GDK_DRAWABLE(pix), gc, saved_img->img, 0, 0, 0, 0, MARGIN_X+PROFILE_WIDTH, MARGIN_Y+PROFILE_HEIGHT);
		saved_img->saved = false;
	}

	// ATM always save whole image - as anywhere could have changed
	if (saved_img->img) {
		gdk_drawable_copy_to_image(GDK_DRAWABLE(pix), saved_img->img, 0, 0, 0, 0, MARGIN_X+PROFILE_WIDTH, MARGIN_Y+PROFILE_HEIGHT);
	} else {
		saved_img->img = gdk_drawable_copy_to_image(GDK_DRAWABLE(pix), saved_img->img, 0, 0, 0, 0, MARGIN_X+PROFILE_WIDTH, MARGIN_Y+PROFILE_HEIGHT);
	}
	saved_img->saved = true;

	if ((marker_x >= MARGIN_X) && (marker_x < (PROFILE_WIDTH + MARGIN_X))) {
		gdk_draw_line(GDK_DRAWABLE(pix), gc, marker_x, MARGIN_Y, marker_x, PROFILE_HEIGHT + MARGIN_Y);
		*marker_drawn = true;
	} else {
		*marker_drawn = false;
	}

	// Draw a square blob to indicate where we are on track for this graph
	if ((blob_x >= MARGIN_X) && (blob_x < (PROFILE_WIDTH + MARGIN_X)) && (blob_y < PROFILE_HEIGHT+MARGIN_Y)) {
		gdk_draw_rectangle(GDK_DRAWABLE(pix), gc, true, blob_x-3, blob_y-3, 6, 6);
		*blob_drawn = true;
	} else {
		*blob_drawn = false;
	}

	// Anywhere on image could have changed
	if (*marker_drawn || *blob_drawn) {
		gtk_widget_queue_draw(image);
	}
}

/**
 * Return the percentage of how far a trackpoint is a long a track via the time method
 */
static double tp_percentage_by_time(Track * trk, Trackpoint * tp)
{
	double pc = NAN;
	if (tp == NULL) {
		return pc;
	}
	time_t t_start, t_end, t_total;
	t_start = ((Trackpoint *) trk->trackpoints->data)->timestamp;
	t_end = ((Trackpoint *) g_list_last(trk->trackpoints)->data)->timestamp;
	t_total = t_end - t_start;
	pc = (double)(tp->timestamp - t_start)/t_total;
	return pc;
}

/**
 * Return the percentage of how far a trackpoint is a long a track via the distance method
 */
static double tp_percentage_by_distance(Track * trk, Trackpoint * tp, double track_length)
{
	double pc = NAN;
	if (tp == NULL) {
		return pc;
	}
	double dist = 0.0;
	GList *iter;
	for (iter = trk->trackpoints->next; iter != NULL; iter = iter->next) {
		dist += vik_coord_diff(&(((Trackpoint *) iter->data)->coord),
				       &(((Trackpoint *) iter->prev->data)->coord));
		/* Assuming trackpoint is not a copy */
		if (tp == ((Trackpoint *) iter->data)) {
			break;
		}
	}
	if (iter != NULL) {
		pc = dist/track_length;
	}
	return pc;
}

static void track_graph_click(GtkWidget * event_box, GdkEventButton * event, PropWidgets * widgets, VikPropWinGraphType_t graph_type)
{
	bool is_time_graph =
		(graph_type == PROPWIN_GRAPH_TYPE_SPEED_TIME ||
		 graph_type == PROPWIN_GRAPH_TYPE_DISTANCE_TIME ||
		 graph_type == PROPWIN_GRAPH_TYPE_ELEVATION_TIME);

	GtkAllocation allocation;
	gtk_widget_get_allocation(event_box, &allocation);

	Trackpoint * tp = set_center_at_graph_position(event->x, allocation.width, widgets->trw, widgets->panel, widgets->viewport, widgets->trk, is_time_graph, widgets->profile_width);
	// Unable to get the point so give up
	if (tp == NULL) {
		gtk_dialog_set_response_sensitive(GTK_DIALOG(widgets->dialog), VIK_TRW_LAYER_PROPWIN_SPLIT_MARKER, false);
		return;
	}

	widgets->marker_tp = tp;

	GList *child;
	GtkWidget *image;
	GtkWidget *window = gtk_widget_get_toplevel(GTK_WIDGET(event_box));
	GtkWidget *graph_box;
	PropSaved *graph_saved_img;
	double pc = NAN;

	// Attempt to redraw marker on all graph types
	int graphite;
	for (graphite = PROPWIN_GRAPH_TYPE_ELEVATION_DISTANCE;
	     graphite < PROPWIN_GRAPH_TYPE_END;
	     graphite++) {

		// Switch commonal variables to particular graph type
		switch (graphite) {
		default:
		case PROPWIN_GRAPH_TYPE_ELEVATION_DISTANCE:
			graph_box       = widgets->elev_box;
			graph_saved_img = &widgets->elev_graph_saved_img;
			is_time_graph   = false;
			break;
		case PROPWIN_GRAPH_TYPE_GRADIENT_DISTANCE:
			graph_box       = widgets->gradient_box;
			graph_saved_img = &widgets->gradient_graph_saved_img;
			is_time_graph   = false;
			break;
		case PROPWIN_GRAPH_TYPE_SPEED_TIME:
			graph_box       = widgets->speed_box;
			graph_saved_img = &widgets->speed_graph_saved_img;
			is_time_graph   = true;
			break;
		case PROPWIN_GRAPH_TYPE_DISTANCE_TIME:
			graph_box       = widgets->dist_box;
			graph_saved_img = &widgets->dist_graph_saved_img;
			is_time_graph   = true;
			break;
		case PROPWIN_GRAPH_TYPE_ELEVATION_TIME:
			graph_box       = widgets->elev_time_box;
			graph_saved_img = &widgets->elev_time_graph_saved_img;
			is_time_graph   = true;
			break;
		case PROPWIN_GRAPH_TYPE_SPEED_DISTANCE:
			graph_box       = widgets->speed_dist_box;
			graph_saved_img = &widgets->speed_dist_graph_saved_img;
			is_time_graph   = false;
			break;
		}

		// Commonal method of redrawing marker
		if (graph_box) {

			child = gtk_container_get_children(GTK_CONTAINER(graph_box));
			image = GTK_WIDGET(child->data);

			if (is_time_graph) {
				pc = tp_percentage_by_time(widgets->trk, tp);
			} else {
				pc = tp_percentage_by_distance(widgets->trk, tp, widgets->track_length_inc_gaps);
			}

			if (!isnan(pc)) {
				double marker_x = (pc * widgets->profile_width) + MARGIN_X;
				save_image_and_draw_graph_marks(image,
								marker_x,
								gtk_widget_get_style(window)->black_gc,
								-1, // Don't draw blob on clicks
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
}

static bool track_profile_click(GtkWidget *event_box, GdkEventButton *event, void * ptr)
{
	track_graph_click(event_box, event, (PropWidgets *) ptr, PROPWIN_GRAPH_TYPE_ELEVATION_DISTANCE);
	return true;  /* don't call other (further) callbacks */
}

static bool track_gradient_click(GtkWidget *event_box, GdkEventButton *event, void * ptr)
{
	track_graph_click(event_box, event, (PropWidgets *) ptr, PROPWIN_GRAPH_TYPE_GRADIENT_DISTANCE);
	return true;  /* don't call other (further) callbacks */
}

static bool track_vt_click(GtkWidget *event_box, GdkEventButton *event, void * ptr)
{
	track_graph_click(event_box, event, (PropWidgets *) ptr, PROPWIN_GRAPH_TYPE_SPEED_TIME);
	return true;  /* don't call other (further) callbacks */
}

static bool track_dt_click(GtkWidget *event_box, GdkEventButton *event, void * ptr)
{
	track_graph_click(event_box, event, (PropWidgets *) ptr, PROPWIN_GRAPH_TYPE_DISTANCE_TIME);
	return true;  /* don't call other (further) callbacks */
}

static bool track_et_click(GtkWidget *event_box, GdkEventButton *event, void * ptr)
{
	track_graph_click(event_box, event, (PropWidgets *) ptr, PROPWIN_GRAPH_TYPE_ELEVATION_TIME);
	return true;  /* don't call other (further) callbacks */
}

static bool track_sd_click(GtkWidget *event_box, GdkEventButton *event, void * ptr)
{
	track_graph_click(event_box, event, (PropWidgets *) ptr, PROPWIN_GRAPH_TYPE_SPEED_DISTANCE);
	return true;  /* don't call other (further) callbacks */
}

/**
 * Calculate y position for blob on elevation graph
 */
static int blobby_altitude(double x_blob, PropWidgets * widgets)
{
	int ix = (int)x_blob;
	// Ensure ix is inbounds
	if (ix == widgets->profile_width) {
		ix--;
	}

	int y_blob = widgets->profile_height-widgets->profile_height*(widgets->altitudes[ix]-widgets->draw_min_altitude)/(chunksa[widgets->cia]*LINES);

	return y_blob;
}

/**
 * Calculate y position for blob on gradient graph
 */
static int blobby_gradient(double x_blob, PropWidgets * widgets)
{
	int ix = (int)x_blob;
	// Ensure ix is inbounds
	if (ix == widgets->profile_width) {
		ix--;
	}

	int y_blob = widgets->profile_height-widgets->profile_height*(widgets->gradients[ix]-widgets->draw_min_gradient)/(chunksg[widgets->cig]*LINES);

	return y_blob;
}

/**
 * Calculate y position for blob on speed graph
 */
static int blobby_speed(double x_blob, PropWidgets * widgets)
{
	int ix = (int)x_blob;
	// Ensure ix is inbounds
	if (ix == widgets->profile_width) {
		ix--;
	}

	int y_blob = widgets->profile_height-widgets->profile_height*(widgets->speeds[ix]-widgets->draw_min_speed)/(chunkss[widgets->cis]*LINES);

	return y_blob;
}

/**
 * Calculate y position for blob on distance graph
 */
static int blobby_distance(double x_blob, PropWidgets * widgets)
{
	int ix = (int)x_blob;
	// Ensure ix is inbounds
	if (ix == widgets->profile_width) {
		ix--;
	}

	int y_blob = widgets->profile_height-widgets->profile_height*(widgets->distances[ix])/(chunksd[widgets->cid]*LINES);
	//NB min distance is always 0, so no need to subtract that from this  ______/

	return y_blob;
}

/**
 * Calculate y position for blob on elevation/time graph
 */
static int blobby_altitude_time(double x_blob, PropWidgets * widgets)
{
	int ix = (int)x_blob;
	// Ensure ix is inbounds
	if (ix == widgets->profile_width) {
		ix--;
	}

	int y_blob = widgets->profile_height-widgets->profile_height*(widgets->ats[ix]-widgets->draw_min_altitude_time)/(chunksa[widgets->ciat]*LINES);
	return y_blob;
}

/**
 * Calculate y position for blob on speed/dist graph
 */
static int blobby_speed_dist(double x_blob, PropWidgets * widgets)
{
	int ix = (int)x_blob;
	// Ensure ix is inbounds
	if (ix == widgets->profile_width) {
		ix--;
	}

	int y_blob = widgets->profile_height-widgets->profile_height*(widgets->speeds_dist[ix]-widgets->draw_min_speed)/(chunkss[widgets->cisd]*LINES);

	return y_blob;
}


void track_profile_move(GtkWidget *event_box, GdkEventMotion *event, PropWidgets *widgets)
{
	if (widgets->altitudes == NULL) {
		return;
	}

	double x = NAN;
	int ix = 0;
	get_mouse_event_x(event_box, event, widgets, &x, &ix);

	double meters_from_start;
	Trackpoint * tp = widgets->trk->get_closest_tp_by_percentage_dist((double) x / widgets->profile_width, &meters_from_start);
	if (tp && widgets->w_cur_dist) {
		distance_label_update(widgets->w_cur_dist, meters_from_start);
	}

	// Show track elevation for this position - to the nearest whole number
	if (tp && widgets->w_cur_elevation) {
		elevation_label_update(widgets->w_cur_elevation, tp);
	}

	widgets->blob_tp = tp;

	GtkWidget * window = gtk_widget_get_toplevel(event_box);
	GList * child = gtk_container_get_children(GTK_CONTAINER(event_box));
	GtkWidget * image = GTK_WIDGET(child->data);

	int y_blob = blobby_altitude(x, widgets);

	double marker_x = -1.0; // i.e. Don't draw unless we get a valid value
	if (widgets->is_marker_drawn) {
		double pc = tp_percentage_by_distance(widgets->trk, widgets->marker_tp, widgets->track_length_inc_gaps);
		if (!isnan(pc)) {
			marker_x = (pc * widgets->profile_width) + MARGIN_X;
		}
	}

	save_image_and_draw_graph_marks(image,
					marker_x,
					gtk_widget_get_style(window)->black_gc,
					MARGIN_X+x,
					MARGIN_Y+y_blob,
					&widgets->elev_graph_saved_img,
					widgets->profile_width,
					widgets->profile_height,
					&widgets->is_marker_drawn,
					&widgets->is_blob_drawn);

	g_list_free(child);
}

void track_gradient_move(GtkWidget * event_box, GdkEventMotion * event, PropWidgets * widgets)
{
	if (widgets->gradients == NULL) {
		return;
	}

	double x = NAN;
	int ix = 0;
	get_mouse_event_x(event_box, event, widgets, &x, &ix);

	double meters_from_start;
	Trackpoint * tp = widgets->trk->get_closest_tp_by_percentage_dist((double) x / widgets->profile_width, &meters_from_start);
	if (tp && widgets->w_cur_gradient_dist) {
		distance_label_update(widgets->w_cur_gradient_dist, meters_from_start);
	}

	// Show track gradient for this position - to the nearest whole number
	if (tp && widgets->w_cur_gradient_gradient) {
		gradient_label_update(widgets->w_cur_gradient_gradient, widgets->gradients[ix]);
	}

	widgets->blob_tp = tp;

	GtkWidget * window = gtk_widget_get_toplevel(event_box);
	GList * child = gtk_container_get_children(GTK_CONTAINER(event_box));
	GtkWidget * image = GTK_WIDGET(child->data);

	int y_blob = blobby_gradient(x, widgets);

	double marker_x = -1.0; // i.e. Don't draw unless we get a valid value
	if (widgets->is_marker_drawn) {
		double pc = tp_percentage_by_distance(widgets->trk, widgets->marker_tp, widgets->track_length_inc_gaps);
		if (!isnan(pc)) {
			marker_x = (pc * widgets->profile_width) + MARGIN_X;
		}
	}

	save_image_and_draw_graph_marks(image,
					marker_x,
					gtk_widget_get_style(window)->black_gc,
					MARGIN_X+x,
					MARGIN_Y+y_blob,
					&widgets->gradient_graph_saved_img,
					widgets->profile_width,
					widgets->profile_height,
					&widgets->is_marker_drawn,
					&widgets->is_blob_drawn);

	g_list_free(child);
}

//
void time_label_update(GtkWidget * widget, time_t seconds_from_start)
{
	static char tmp_buf[20];
	unsigned int h = seconds_from_start/3600;
	unsigned int m = (seconds_from_start - h*3600)/60;
	unsigned int s = seconds_from_start - (3600*h) - (60*m);
	snprintf(tmp_buf, sizeof(tmp_buf), "%02d:%02d:%02d", h, m, s);
	gtk_label_set_text(GTK_LABEL(widget), tmp_buf);

	return;
}

//
void real_time_label_update(GtkWidget * widget, Trackpoint * tp)
{
	static char tmp_buf[64];
	if (tp->has_timestamp) {
		// Alternatively could use %c format but I prefer a slightly more compact form here
		//  The full date can of course be seen on the Statistics tab
		strftime(tmp_buf, sizeof(tmp_buf), "%X %x %Z", localtime(&(tp->timestamp)));
	} else {
		snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
	}
	gtk_label_set_text(GTK_LABEL(widget), tmp_buf);

	return;
}

void speed_label_update(GtkWidget * widget, double value)
{
	static char tmp_buf[20];
	// Even if GPS speed available (tp->speed), the text will correspond to the speed map shown
	// No conversions needed as already in appropriate units
	vik_units_speed_t speed_units = a_vik_get_units_speed();
	switch (speed_units) {
	case VIK_UNITS_SPEED_KILOMETRES_PER_HOUR:
		snprintf(tmp_buf, sizeof(tmp_buf), _("%.1f kph"), value);
		break;
	case VIK_UNITS_SPEED_MILES_PER_HOUR:
		snprintf(tmp_buf, sizeof(tmp_buf), _("%.1f mph"), value);
		break;
	case VIK_UNITS_SPEED_KNOTS:
		snprintf(tmp_buf, sizeof(tmp_buf), _("%.1f knots"), value);
		break;
	default:
		// VIK_UNITS_SPEED_METRES_PER_SECOND:
		snprintf(tmp_buf, sizeof(tmp_buf), _("%.1f m/s"), value);
		break;
	}
	gtk_label_set_text(GTK_LABEL(widget), tmp_buf);

	return;
}

void gradient_label_update(GtkWidget * widget, double gradient)
{
	static char tmp_buf[20];
	snprintf(tmp_buf, sizeof(tmp_buf), "%d%%", (int) gradient);
	gtk_label_set_text(GTK_LABEL(widget), tmp_buf);

	return;
}



void track_vt_move(GtkWidget * event_box, GdkEventMotion * event, PropWidgets * widgets)
{
	if (widgets->speeds == NULL) {
		return;
	}

	double x = NAN;
	int ix = 0;
	get_mouse_event_x(event_box, event, widgets, &x, &ix);

	time_t seconds_from_start;
	Trackpoint * tp = widgets->trk->get_closest_tp_by_percentage_time((double) x / widgets->profile_width, &seconds_from_start);
	if (tp && widgets->w_cur_time) {
		time_label_update(widgets->w_cur_time, seconds_from_start);
	}

	if (tp && widgets->w_cur_time_real) {
		real_time_label_update(widgets->w_cur_time_real, tp);
	}

	// Show track speed for this position
	if (tp && widgets->w_cur_speed) {
		speed_label_update(widgets->w_cur_speed, widgets->speeds[ix]);
	}

	widgets->blob_tp = tp;

	GtkWidget * window = gtk_widget_get_toplevel(event_box);
	GList * child = gtk_container_get_children(GTK_CONTAINER(event_box));
	GtkWidget * image = GTK_WIDGET(child->data);

	int y_blob = blobby_speed(x, widgets);

	double marker_x = -1.0; // i.e. Don't draw unless we get a valid value
	if (widgets->is_marker_drawn) {
		double pc = tp_percentage_by_time(widgets->trk, widgets->marker_tp);
		if (!isnan(pc)) {
			marker_x = (pc * widgets->profile_width) + MARGIN_X;
		}
	}

	save_image_and_draw_graph_marks(image,
					marker_x,
					gtk_widget_get_style(window)->black_gc,
					MARGIN_X+x,
					MARGIN_Y+y_blob,
					&widgets->speed_graph_saved_img,
					widgets->profile_width,
					widgets->profile_height,
					&widgets->is_marker_drawn,
					&widgets->is_blob_drawn);

	g_list_free(child);
}

/**
 * Update labels and blob marker on mouse moves in the distance/time graph
 */
void track_dt_move(GtkWidget * event_box, GdkEventMotion * event, PropWidgets * widgets)
{
	if (widgets->distances == NULL) {
		return;
	}

	double x = NAN;
	int ix = 0;
	get_mouse_event_x(event_box, event, widgets, &x, &ix);

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

	GtkWidget * window = gtk_widget_get_toplevel(event_box);
	GList * child = gtk_container_get_children(GTK_CONTAINER(event_box));
	GtkWidget * image = GTK_WIDGET(child->data);

	int y_blob = blobby_distance(x, widgets);

	double marker_x = -1.0; // i.e. Don't draw unless we get a valid value
	if (widgets->is_marker_drawn) {
		double pc = tp_percentage_by_time(widgets->trk, widgets->marker_tp);
		if (!isnan(pc)) {
			marker_x = (pc * widgets->profile_width) + MARGIN_X;
		}
	}

	save_image_and_draw_graph_marks(image,
					marker_x,
					gtk_widget_get_style(window)->black_gc,
					MARGIN_X+x,
					MARGIN_Y+y_blob,
					&widgets->dist_graph_saved_img,
					widgets->profile_width,
					widgets->profile_height,
					&widgets->is_marker_drawn,
					&widgets->is_blob_drawn);

	g_list_free(child);
}

/**
 * Update labels and blob marker on mouse moves in the elevation/time graph
 */
void track_et_move(GtkWidget * event_box, GdkEventMotion * event, PropWidgets * widgets)
{
	if (widgets->ats == NULL) {
		return;
	}

	double x = NAN;
	int ix = 0;
	get_mouse_event_x(event_box, event, widgets, &x, &ix);

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

	GtkWidget * window = gtk_widget_get_toplevel(event_box);
	GList * child = gtk_container_get_children(GTK_CONTAINER(event_box));
	GtkWidget * image = GTK_WIDGET(child->data);

	int y_blob = blobby_altitude_time(x, widgets);

	double marker_x = -1.0; // i.e. Don't draw unless we get a valid value
	if (widgets->is_marker_drawn) {
		double pc = tp_percentage_by_time(widgets->trk, widgets->marker_tp);
		if (!isnan(pc)) {
			marker_x = (pc * widgets->profile_width) + MARGIN_X;
		}
	}

	save_image_and_draw_graph_marks(image,
					marker_x,
					gtk_widget_get_style(window)->black_gc,
					MARGIN_X+x,
					MARGIN_Y+y_blob,
					&widgets->elev_time_graph_saved_img,
					widgets->profile_width,
					widgets->profile_height,
					&widgets->is_marker_drawn,
					&widgets->is_blob_drawn);

	g_list_free(child);
}

void track_sd_move(GtkWidget * event_box, GdkEventMotion * event, PropWidgets * widgets)
{
	if (widgets->speeds_dist == NULL) {
		return;
	}

	double x = NAN;
	int ix = 0;
	get_mouse_event_x(event_box, event, widgets, &x, &ix);

	double meters_from_start;
	Trackpoint * tp = widgets->trk->get_closest_tp_by_percentage_dist((double) x / widgets->profile_width, &meters_from_start);
	if (tp && widgets->w_cur_speed_dist) {
		distance_label_update(widgets->w_cur_speed_dist, meters_from_start);
	}

	// Show track speed for this position
	if (widgets->w_cur_speed_speed) {
		speed_label_update(widgets->w_cur_speed_speed, widgets->speeds_dist[ix]);
	}

	widgets->blob_tp = tp;

	GtkWidget * window = gtk_widget_get_toplevel(event_box);
	GList * child = gtk_container_get_children(GTK_CONTAINER(event_box));
	GtkWidget * image = GTK_WIDGET(child->data);

	int y_blob = blobby_speed_dist(x, widgets);

	double marker_x = -1.0; // i.e. Don't draw unless we get a valid value
	if (widgets->is_marker_drawn) {
		double pc = tp_percentage_by_distance(widgets->trk, widgets->marker_tp, widgets->track_length_inc_gaps);
		if (!isnan(pc)) {
			marker_x = (pc * widgets->profile_width) + MARGIN_X;
		}
	}

	save_image_and_draw_graph_marks(image,
					marker_x,
					gtk_widget_get_style(window)->black_gc,
					MARGIN_X+x,
					MARGIN_Y+y_blob,
					&widgets->speed_dist_graph_saved_img,
					widgets->profile_width,
					widgets->profile_height,
					&widgets->is_marker_drawn,
					&widgets->is_blob_drawn);

	g_list_free(child);
}



void get_mouse_event_x(GtkWidget * event_box, GdkEventMotion * event, PropWidgets * widgets, double * x, int * ix)
{
	int mouse_x, mouse_y;
	GdkModifierType state;

	if (event->is_hint) {
		gdk_window_get_pointer(event->window, &mouse_x, &mouse_y, &state);
	} else {
		mouse_x = event->x;
	}

	GtkAllocation allocation;
	gtk_widget_get_allocation(event_box, &allocation);

	(*x) = mouse_x - allocation.width / 2 + widgets->profile_width / 2 - MARGIN_X / 2;
	if ((*x) < 0) {
		(*x) = 0;
	}
	if ((*x) > widgets->profile_width) {
		(*x) = widgets->profile_width;
	}

	*ix = (int) (*x);
	// Ensure ix is inbounds
	if (*ix == widgets->profile_width) {
		(*ix)--;
	}

	return;
}


void distance_label_update(GtkWidget * widget, double meters_from_start)
{
	static char tmp_buf[20];
	vik_units_distance_t dist_units = a_vik_get_units_distance();
	switch (dist_units) {
	case VIK_UNITS_DISTANCE_KILOMETRES:
		snprintf(tmp_buf, sizeof(tmp_buf), "%.2f km", meters_from_start/1000.0);
		break;
	case VIK_UNITS_DISTANCE_MILES:
		snprintf(tmp_buf, sizeof(tmp_buf), "%.2f miles", VIK_METERS_TO_MILES(meters_from_start));
		break;
	case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
		snprintf(tmp_buf, sizeof(tmp_buf), "%.2f NM", VIK_METERS_TO_NAUTICAL_MILES(meters_from_start));
		break;
	default:
		fprintf(stderr, "CRITICAL: Houston, we've had a problem. distance=%d\n", dist_units);
	}
	gtk_label_set_text(GTK_LABEL(widget), tmp_buf);

	return;

}

void elevation_label_update(GtkWidget * widget, Trackpoint * tp)
{
	static char tmp_buf[20];
	if (a_vik_get_units_height() == VIK_UNITS_HEIGHT_FEET) {
		snprintf(tmp_buf, sizeof(tmp_buf), "%d ft", (int)VIK_METERS_TO_FEET(tp->altitude));
	} else {
		snprintf(tmp_buf, sizeof(tmp_buf), "%d m", (int) tp->altitude);
	}
	gtk_label_set_text(GTK_LABEL(widget), tmp_buf);

	return;
}

void dist_dist_label_update(GtkWidget * widget, double distance)
{
	static char tmp_buf[20];
	switch (a_vik_get_units_distance()) {
	case VIK_UNITS_DISTANCE_MILES:
		snprintf(tmp_buf, sizeof(tmp_buf), "%.2f miles", distance);
		break;
	case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
		snprintf(tmp_buf, sizeof(tmp_buf), "%.2f NM", distance);
		break;
	default:
		snprintf(tmp_buf, sizeof(tmp_buf), "%.2f km", distance);
		break;
	}
	gtk_label_set_text(GTK_LABEL(widget), tmp_buf);

	return;
}


/**
 * Draws DEM points and a respresentative speed on the supplied pixmap
 *   (which is the elevations graph)
 */
static void draw_dem_alt_speed_dist(Track * trk,
				    GdkDrawable *pix,
				    GdkGC *alt_gc,
				    GdkGC *speed_gc,
				    double alt_offset,
				    double alt_diff,
				    double max_speed_in,
				    int cia,
				    int width,
				    int height,
				    int margin,
				    bool do_dem,
				    bool do_speed)
{
	GList * iter;
	double max_speed = 0;
	double total_length = trk->get_length_including_gaps();

	// Calculate the max speed factor
	if (do_speed) {
		max_speed = max_speed_in * 110 / 100;
	}

	double dist = 0;
	int h2 = height + MARGIN_Y; // Adjust height for x axis labelling offset
	int achunk = chunksa[cia]*LINES;

	for (iter = trk->trackpoints->next; iter; iter = iter->next) {
		int x;
		dist += vik_coord_diff(&(((Trackpoint *) iter->data)->coord),
				       &(((Trackpoint *) iter->prev->data)->coord));
		x = (width * dist)/total_length + margin;
		if (do_dem) {
			int16_t elev = a_dems_get_elev_by_coord(&(((Trackpoint *) iter->data)->coord), VIK_DEM_INTERPOL_BEST);
			if (elev != VIK_DEM_INVALID_ELEVATION) {
				// Convert into height units
				if (a_vik_get_units_height() == VIK_UNITS_HEIGHT_FEET) {
					elev = VIK_METERS_TO_FEET(elev);
				}
				// No conversion needed if already in metres

				// offset is in current height units
				elev -= alt_offset;

				// consider chunk size
				int y_alt = h2 - ((height * elev)/achunk);
				gdk_draw_rectangle(GDK_DRAWABLE(pix), alt_gc, true, x-2, y_alt-2, 4, 4);
			}
		}
		if (do_speed) {
			// This is just a speed indicator - no actual values can be inferred by user
			if (!isnan(((Trackpoint *) iter->data)->speed)) {
				int y_speed = h2 - (height * ((Trackpoint *) iter->data)->speed)/max_speed;
				gdk_draw_rectangle(GDK_DRAWABLE(pix), speed_gc, true, x-2, y_speed-2, 4, 4);
			}
		}
	}
}

/**
 * draw_grid_y:
 *
 * A common way to draw the grid with y axis labels
 *
 */
static void draw_grid_y(GtkWidget * window, GtkWidget * image, PropWidgets * widgets, GdkPixmap * pix, char * ss, int i)
{
	PangoLayout * pl = gtk_widget_create_pango_layout(GTK_WIDGET(image), NULL);

	pango_layout_set_alignment(pl, PANGO_ALIGN_RIGHT);
	pango_layout_set_font_description(pl, gtk_widget_get_style(window)->font_desc);

	char * label_markup = g_strdup_printf("<span size=\"small\">%s</span>", ss);
	pango_layout_set_markup(pl, label_markup, -1);
	free(label_markup);

	int w, h;
	pango_layout_get_pixel_size(pl, &w, &h);

	gdk_draw_layout(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->fg_gc[0],
			MARGIN_X-w-3,
			CLAMP((int)i*widgets->profile_height/LINES - h/2 + MARGIN_Y, 0, widgets->profile_height-h+MARGIN_Y),
			pl);
	g_object_unref(G_OBJECT (pl));

	gdk_draw_line(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->dark_gc[0],
		      MARGIN_X, MARGIN_Y + widgets->profile_height/LINES * i,
		      MARGIN_X + widgets->profile_width, MARGIN_Y + widgets->profile_height/LINES * i);
}

/**
 * draw_grid_x_time:
 *
 * A common way to draw the grid with x axis labels for time graphs
 *
 */
static void draw_grid_x_time(GtkWidget * window, GtkWidget * image, PropWidgets * widgets, GdkPixmap * pix, unsigned int ii, unsigned int tt, unsigned int xx)
{
	char *label_markup = NULL;
	switch (ii) {
	case 0:
	case 1:
	case 2:
	case 3:
		// Minutes
		label_markup = g_strdup_printf("<span size=\"small\">%d %s</span>", tt/60, _("mins"));
		break;
	case 4:
	case 5:
	case 6:
	case 7:
		// Hours
		label_markup = g_strdup_printf("<span size=\"small\">%.1f %s</span>", (double)tt/(60*60), _("h"));
		break;
	case 8:
	case 9:
	case 10:
		// Days
		label_markup = g_strdup_printf("<span size=\"small\">%.1f %s</span>", (double)tt/(60*60*24), _("d"));
		break;
	case 11:
	case 12:
		// Weeks
		label_markup = g_strdup_printf("<span size=\"small\">%.1f %s</span>", (double)tt/(60*60*24*7), _("w"));
		break;
	case 13:
		// 'Months'
		label_markup = g_strdup_printf("<span size=\"small\">%.1f %s</span>", (double)tt/(60*60*24*28), _("M"));
		break;
	default:
		break;
	}
	if (label_markup) {

		PangoLayout *pl = gtk_widget_create_pango_layout(GTK_WIDGET(image), NULL);
		pango_layout_set_font_description(pl, gtk_widget_get_style(window)->font_desc);

		pango_layout_set_markup(pl, label_markup, -1);
		free(label_markup);
		int ww, hh;
		pango_layout_get_pixel_size(pl, &ww, &hh);

		gdk_draw_layout(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->fg_gc[0],
				MARGIN_X+xx-ww/2, MARGIN_Y/2-hh/2, pl);
		g_object_unref(G_OBJECT (pl));
	}

	gdk_draw_line(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->dark_gc[0],
		      MARGIN_X+xx, MARGIN_Y, MARGIN_X+xx, MARGIN_Y+widgets->profile_height);
}

/**
 * draw_grid_x_distance:
 *
 * A common way to draw the grid with x axis labels for distance graphs
 *
 */
static void draw_grid_x_distance(GtkWidget * window, GtkWidget * image, PropWidgets * widgets, GdkPixmap * pix, unsigned int ii, double dd, unsigned int xx, vik_units_distance_t dist_units)
{
	char *label_markup = NULL;
	switch (dist_units) {
	case VIK_UNITS_DISTANCE_MILES:
		if (ii > 4) {
			label_markup = g_strdup_printf("<span size=\"small\">%d %s</span>", (unsigned int)dd, _("miles"));
		} else {
			label_markup = g_strdup_printf("<span size=\"small\">%.1f %s</span>", dd, _("miles"));
		}
		break;
	case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
		if (ii > 4) {
			label_markup = g_strdup_printf("<span size=\"small\">%d %s</span>", (unsigned int)dd, _("NM"));
		} else {
			label_markup = g_strdup_printf("<span size=\"small\">%.1f %s</span>", dd, _("NM"));
		}
		break;
	default:
		// VIK_UNITS_DISTANCE_KILOMETRES:
		if (ii > 4) {
			label_markup = g_strdup_printf("<span size=\"small\">%d %s</span>", (unsigned int)dd, _("km"));
		} else {
			label_markup = g_strdup_printf("<span size=\"small\">%.1f %s</span>", dd, _("km"));
		}
		break;
	}

	if (label_markup) {
		PangoLayout *pl = gtk_widget_create_pango_layout(GTK_WIDGET(image), NULL);
		pango_layout_set_font_description(pl, gtk_widget_get_style(window)->font_desc);

		pango_layout_set_markup(pl, label_markup, -1);
		free(label_markup);
		int ww, hh;
		pango_layout_get_pixel_size(pl, &ww, &hh);

		gdk_draw_layout(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->fg_gc[0],
				MARGIN_X+xx-ww/2, MARGIN_Y/2-hh/2, pl);
		g_object_unref(G_OBJECT (pl));
	}

	gdk_draw_line(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->dark_gc[0],
		      MARGIN_X+xx, MARGIN_Y, MARGIN_X+xx, MARGIN_Y+widgets->profile_height);
}

/**
 * clear the images (scale texts & actual graph)
 */
static void clear_images(GdkPixmap *pix, GtkWidget *window, PropWidgets *widgets)
{
  gdk_draw_rectangle(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->bg_gc[0],
                     true, 0, 0, widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y);
  gdk_draw_rectangle(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->mid_gc[0],
                     true, 0, 0, widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y);
}

/**
 *
 */
static void draw_distance_divisions(GtkWidget * window, GtkWidget * image, GdkPixmap * pix, PropWidgets * widgets, vik_units_distance_t dist_units)
{
	// Set to display units from length in metres.
	double length = widgets->track_length_inc_gaps;
	switch (dist_units) {
	case VIK_UNITS_DISTANCE_MILES:
		length = VIK_METERS_TO_MILES(length);
		break;
	case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
		length = VIK_METERS_TO_NAUTICAL_MILES(length);
		break;
	default:
		// KM
		length = length/1000.0;
		break;
	}
	unsigned int index = get_distance_chunk_index(length);
	double dist_per_pixel = length/widgets->profile_width;

	for (unsigned int i = 1; chunksd[index] * i <= length; i++) {
		draw_grid_x_distance(window, image, widgets, pix, index, chunksd[index] * i, (unsigned int) (chunksd[index] * i / dist_per_pixel), dist_units);
	}
}

/**
 * Draw just the height profile image
 */
static void draw_elevations(GtkWidget * image, Track * trk, PropWidgets * widgets)
{
	unsigned int i;

	GdkGC *no_alt_info;
	GdkColor color;

	// Free previous allocation
	if (widgets->altitudes) {
		free(widgets->altitudes);
	}

	widgets->altitudes = trk->make_elevation_map(widgets->profile_width);

	if (widgets->altitudes == NULL) {
		return;
	}

	// Convert into appropriate units
	vik_units_height_t height_units = a_vik_get_units_height();
	if (height_units == VIK_UNITS_HEIGHT_FEET) {
		// Convert altitudes into feet units
		for (i = 0; i < widgets->profile_width; i++) {
			widgets->altitudes[i] = VIK_METERS_TO_FEET(widgets->altitudes[i]);
		}
	}
	// Otherwise leave in metres

	minmax_array(widgets->altitudes, &widgets->min_altitude, &widgets->max_altitude, true, widgets->profile_width);

	get_new_min_and_chunk_index(widgets->min_altitude, widgets->max_altitude, chunksa, G_N_ELEMENTS(chunksa), &widgets->draw_min_altitude, &widgets->cia);

	// Assign locally
	double mina = widgets->draw_min_altitude;

	GtkWidget * window = gtk_widget_get_toplevel(widgets->elev_box);
	GdkPixmap * pix = gdk_pixmap_new(gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1);

	gtk_image_set_from_pixmap(GTK_IMAGE(image), pix, NULL);

	no_alt_info = gdk_gc_new(gtk_widget_get_window(window));
	gdk_color_parse ("yellow", &color);
	gdk_gc_set_rgb_fg_color(no_alt_info, &color);

	// Reset before redrawing
	clear_images (pix, window, widgets);

	/* draw grid */
	for (i=0; i<=LINES; i++) {
		char s[32];

		switch (height_units) {
		case VIK_UNITS_HEIGHT_METRES:
			sprintf(s, "%8dm", (int)(mina + (LINES-i)*chunksa[widgets->cia]));
			break;
		case VIK_UNITS_HEIGHT_FEET:
			// NB values already converted into feet
			sprintf(s, "%8dft", (int)(mina + (LINES-i)*chunksa[widgets->cia]));
			break;
		default:
			sprintf(s, "--");
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. height=%d\n", height_units);
		}

		draw_grid_y(window, image, widgets, pix, s, i);
	}

	draw_distance_divisions(window, image, pix, widgets, a_vik_get_units_distance());

	/* draw elevations */
	unsigned int height = MARGIN_Y+widgets->profile_height;
	for (i = 0; i < widgets->profile_width; i++) {
		if (widgets->altitudes[i] == VIK_DEFAULT_ALTITUDE) {
			gdk_draw_line(GDK_DRAWABLE(pix), no_alt_info,
				      i + MARGIN_X, MARGIN_Y, i + MARGIN_X, height);
		} else {
			gdk_draw_line(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->dark_gc[3],
				      i + MARGIN_X, height, i + MARGIN_X, height-widgets->profile_height*(widgets->altitudes[i]-mina)/(chunksa[widgets->cia]*LINES));
		}
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets->w_show_dem)) ||
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets->w_show_alt_gps_speed))) {

		GdkGC *dem_alt_gc = gdk_gc_new(gtk_widget_get_window(window));
		GdkGC *gps_speed_gc = gdk_gc_new(gtk_widget_get_window(window));

		gdk_color_parse("green", &color);
		gdk_gc_set_rgb_fg_color(dem_alt_gc, &color);

		gdk_color_parse("red", &color);
		gdk_gc_set_rgb_fg_color(gps_speed_gc, &color);

		// Ensure somekind of max speed when not set
		if (widgets->max_speed < 0.01) {
			widgets->max_speed = trk->get_max_speed();
		}

		draw_dem_alt_speed_dist(trk,
					GDK_DRAWABLE(pix),
					dem_alt_gc,
					gps_speed_gc,
					mina,
					widgets->max_altitude - mina,
					widgets->max_speed,
					widgets->cia,
					widgets->profile_width,
					widgets->profile_height,
					MARGIN_X,
					gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets->w_show_dem)),
					gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets->w_show_alt_gps_speed)));

		g_object_unref(G_OBJECT(dem_alt_gc));
		g_object_unref(G_OBJECT(gps_speed_gc));
	}

	/* draw border */
	gdk_draw_rectangle(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->black_gc, false, MARGIN_X, MARGIN_Y, widgets->profile_width-1, widgets->profile_height-1);

	g_object_unref(G_OBJECT(pix));
	g_object_unref(G_OBJECT(no_alt_info));
}

/**
 * Draws representative speed on the supplied pixmap
 *   (which is the gradients graph)
 */
static void draw_speed_dist(Track * trk,
			    GdkDrawable * pix,
			    GdkGC * speed_gc,
			    double max_speed_in,
			    int width,
			    int height,
			    int margin,
			    bool do_speed)
{
	GList * iter;
	double max_speed = 0;
	double total_length = trk->get_length_including_gaps();

	// Calculate the max speed factor
	if (do_speed) {
		max_speed = max_speed_in * 110 / 100;
	}

	double dist = 0;
	for (iter = trk->trackpoints->next; iter; iter = iter->next) {
		int x;
		dist += vik_coord_diff(&(((Trackpoint *) iter->data)->coord),
				       &(((Trackpoint *) iter->prev->data)->coord));
		x = (width * dist)/total_length + MARGIN_X;
		if (do_speed) {
			// This is just a speed indicator - no actual values can be inferred by user
			if (!isnan(((Trackpoint *) iter->data)->speed)) {
				int y_speed = height - (height * ((Trackpoint *) iter->data)->speed)/max_speed;
				gdk_draw_rectangle(GDK_DRAWABLE(pix), speed_gc, true, x-2, y_speed-2, 4, 4);
			}
		}
	}
}

/**
 * Draw just the gradient image
 */
static void draw_gradients(GtkWidget * image, Track * trk, PropWidgets * widgets)
{
	unsigned int i;

	// Free previous allocation
	if (widgets->gradients) {
		free(widgets->gradients);
	}

	widgets->gradients = trk->make_gradient_map(widgets->profile_width);

	if (widgets->gradients == NULL) {
		return;
	}

	minmax_array(widgets->gradients, &widgets->min_gradient, &widgets->max_gradient, true, widgets->profile_width);

	get_new_min_and_chunk_index(widgets->min_gradient, widgets->max_gradient, chunksg, G_N_ELEMENTS(chunksg), &widgets->draw_min_gradient, &widgets->cig);

	// Assign locally
	double mina = widgets->draw_min_gradient;

	GtkWidget *window = gtk_widget_get_toplevel(widgets->gradient_box);
	GdkPixmap *pix = gdk_pixmap_new(gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1);

	gtk_image_set_from_pixmap(GTK_IMAGE(image), pix, NULL);

	// Reset before redrawing
	clear_images(pix, window, widgets);

	/* draw grid */
	for (i=0; i<=LINES; i++) {
		char s[32];

		sprintf(s, "%8d%%", (int)(mina + (LINES-i)*chunksg[widgets->cig]));

		draw_grid_y(window, image, widgets, pix, s, i);
	}

	draw_distance_divisions(window, image, pix, widgets, a_vik_get_units_distance());

	/* draw gradients */
	unsigned int height = widgets->profile_height + MARGIN_Y;
	for (i = 0; i < widgets->profile_width; i++) {
		gdk_draw_line(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->dark_gc[3],
			      i + MARGIN_X, height, i + MARGIN_X, height - widgets->profile_height*(widgets->gradients[i]-mina)/(chunksg[widgets->cig]*LINES));
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets->w_show_gradient_gps_speed))) {
		GdkGC *gps_speed_gc = gdk_gc_new(gtk_widget_get_window(window));

		GdkColor color;
		gdk_color_parse("red", &color);
		gdk_gc_set_rgb_fg_color(gps_speed_gc, &color);

		// Ensure somekind of max speed when not set
		if (widgets->max_speed < 0.01) {
			widgets->max_speed = trk->get_max_speed();
		}

		draw_speed_dist(trk,
				GDK_DRAWABLE(pix),
				gps_speed_gc,
				widgets->max_speed,
				widgets->profile_width,
				widgets->profile_height,
				MARGIN_X,
				gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets->w_show_alt_gps_speed)));

		g_object_unref(G_OBJECT(gps_speed_gc));
	}

	/* draw border */
	gdk_draw_rectangle(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->black_gc, false, MARGIN_X, MARGIN_Y, widgets->profile_width-1, widgets->profile_height-1);

	g_object_unref(G_OBJECT(pix));
}

static void draw_time_lines(GtkWidget * window, GtkWidget * image, GdkPixmap * pix, PropWidgets * widgets)
{
	unsigned int index = get_time_chunk_index (widgets->duration);
	double time_per_pixel = (double)(widgets->duration)/widgets->profile_width;

	// If stupidly long track in time - don't bother trying to draw grid lines
	if (widgets->duration > chunkst[G_N_ELEMENTS(chunkst)-1]*LINES*LINES) {
		return;
	}

	for (unsigned int i=1; chunkst[index]*i <= widgets->duration; i++) {
		draw_grid_x_time(window, image, widgets, pix, index, chunkst[index]*i, (unsigned int)(chunkst[index]*i/time_per_pixel));
	}
}

/**
 * Draw just the speed (velocity)/time image
 */
static void draw_vt(GtkWidget * image, Track * trk, PropWidgets * widgets)
{
	unsigned int i;

	// Free previous allocation
	if (widgets->speeds) {
		free(widgets->speeds);
	}

	widgets->speeds = trk->make_speed_map(widgets->profile_width);
	if (widgets->speeds == NULL) {
		return;
	}

	widgets->duration = trk->get_duration(true);
	// Negative time or other problem
	if (widgets->duration <= 0) {
		return;
	}

	// Convert into appropriate units
	vik_units_speed_t speed_units = a_vik_get_units_speed();
	switch (speed_units) {
	case VIK_UNITS_SPEED_KILOMETRES_PER_HOUR:
		for (i = 0; i < widgets->profile_width; i++) {
			widgets->speeds[i] = VIK_MPS_TO_KPH(widgets->speeds[i]);
		}
		break;
	case VIK_UNITS_SPEED_MILES_PER_HOUR:
		for (i = 0; i < widgets->profile_width; i++) {
			widgets->speeds[i] = VIK_MPS_TO_MPH(widgets->speeds[i]);
		}
		break;
	case VIK_UNITS_SPEED_KNOTS:
		for (i = 0; i < widgets->profile_width; i++) {
			widgets->speeds[i] = VIK_MPS_TO_KNOTS(widgets->speeds[i]);
		}
		break;
	default:
		// VIK_UNITS_SPEED_METRES_PER_SECOND:
		// No need to convert as already in m/s
		break;
	}

	GtkWidget *window = gtk_widget_get_toplevel(widgets->speed_box);
	GdkPixmap *pix = gdk_pixmap_new(gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1);

	gtk_image_set_from_pixmap(GTK_IMAGE(image), pix, NULL);

	minmax_array(widgets->speeds, &widgets->min_speed, &widgets->max_speed, false, widgets->profile_width);
	if (widgets->min_speed < 0.0) {
		widgets->min_speed = 0; /* splines sometimes give negative speeds */
	}

	/* Find suitable chunk index */
	get_new_min_and_chunk_index(widgets->min_speed, widgets->max_speed, chunkss, G_N_ELEMENTS(chunkss), &widgets->draw_min_speed, &widgets->cis);

	// Assign locally
	double mins = widgets->draw_min_speed;

	// Reset before redrawing
	clear_images(pix, window, widgets);

	/* draw grid */
	for (i = 0; i <= LINES; i++) {
		char s[32];

		// NB: No need to convert here anymore as numbers are in the appropriate units
		switch (speed_units) {
		case VIK_UNITS_SPEED_KILOMETRES_PER_HOUR:
			sprintf(s, "%8dkm/h", (int)(mins + (LINES-i)*chunkss[widgets->cis]));
			break;
		case VIK_UNITS_SPEED_MILES_PER_HOUR:
			sprintf(s, "%8dmph", (int)(mins + (LINES-i)*chunkss[widgets->cis]));
			break;
		case VIK_UNITS_SPEED_METRES_PER_SECOND:
			sprintf(s, "%8dm/s", (int)(mins + (LINES-i)*chunkss[widgets->cis]));
			break;
		case VIK_UNITS_SPEED_KNOTS:
			sprintf(s, "%8dknots", (int)(mins + (LINES-i)*chunkss[widgets->cis]));
			break;
		default:
			sprintf(s, "--");
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. speed=%d\n", speed_units);
		}

		draw_grid_y(window, image, widgets, pix, s, i);
	}

	draw_time_lines(window, image, pix, widgets);

	/* draw speeds */
	unsigned int height = widgets->profile_height + MARGIN_Y;
	for (i = 0; i < widgets->profile_width; i++) {
		gdk_draw_line(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->dark_gc[3],
			       i + MARGIN_X, height, i + MARGIN_X, height - widgets->profile_height*(widgets->speeds[i]-mins)/(chunkss[widgets->cis]*LINES));
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets->w_show_gps_speed))) {

		GdkGC *gps_speed_gc = gdk_gc_new(gtk_widget_get_window(window));
		GdkColor color;
		gdk_color_parse("red", &color);
		gdk_gc_set_rgb_fg_color(gps_speed_gc, &color);

		time_t beg_time = ((Trackpoint *) trk->trackpoints->data)->timestamp;
		time_t dur = ((Trackpoint *) g_list_last(trk->trackpoints)->data)->timestamp - beg_time;

		GList *iter;
		for (iter = trk->trackpoints; iter; iter = iter->next) {
			double gps_speed = ((Trackpoint *) iter->data)->speed;
			if (isnan(gps_speed))
				continue;
			switch (speed_units) {
			case VIK_UNITS_SPEED_KILOMETRES_PER_HOUR:
				gps_speed = VIK_MPS_TO_KPH(gps_speed);
				break;
			case VIK_UNITS_SPEED_MILES_PER_HOUR:
				gps_speed = VIK_MPS_TO_MPH(gps_speed);
				break;
			case VIK_UNITS_SPEED_KNOTS:
				gps_speed = VIK_MPS_TO_KNOTS(gps_speed);
				break;
			default:
				// VIK_UNITS_SPEED_METRES_PER_SECOND:
				// No need to convert as already in m/s
				break;
			}
			int x = MARGIN_X + widgets->profile_width * (((Trackpoint *) iter->data)->timestamp - beg_time) / dur;
			int y = height - widgets->profile_height*(gps_speed - mins)/(chunkss[widgets->cis]*LINES);
			gdk_draw_rectangle(GDK_DRAWABLE(pix), gps_speed_gc, true, x-2, y-2, 4, 4);
		}
		g_object_unref(G_OBJECT(gps_speed_gc));
	}

	/* draw border */
	gdk_draw_rectangle(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->black_gc, false, MARGIN_X, MARGIN_Y, widgets->profile_width-1, widgets->profile_height-1);

	g_object_unref(G_OBJECT(pix));
}

/**
 * Draw just the distance/time image
 */
static void draw_dt(GtkWidget * image, Track * trk, PropWidgets * widgets)
{
	unsigned int i;

	// Free previous allocation
	if (widgets->distances) {
		free(widgets->distances);
	}

	widgets->distances = trk->make_distance_map(widgets->profile_width);
	if (widgets->distances == NULL) {
		return;
	}

	// Convert into appropriate units
	vik_units_distance_t dist_units = a_vik_get_units_distance();
	switch (dist_units) {
	case VIK_UNITS_DISTANCE_MILES:
		for (i = 0; i < widgets->profile_width; i++) {
			widgets->distances[i] = VIK_METERS_TO_MILES(widgets->distances[i]);
		}
		break;
	case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
		for (i = 0; i < widgets->profile_width; i++) {
			widgets->distances[i] = VIK_METERS_TO_NAUTICAL_MILES(widgets->distances[i]);
		}
		break;
	default:
		// Metres - but want in kms
		for (i = 0; i < widgets->profile_width; i++) {
			widgets->distances[i] = widgets->distances[i]/1000.0;
		}
		break;
	}

	widgets->duration = widgets->trk->get_duration(true);
	// Negative time or other problem
	if (widgets->duration <= 0) {
		return;
	}

	GtkWidget * window = gtk_widget_get_toplevel(widgets->dist_box);
	GdkPixmap * pix = gdk_pixmap_new(gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1);

	gtk_image_set_from_pixmap(GTK_IMAGE(image), pix, NULL);

	// easy to work out min / max of distance!
	// Assign locally
	// mind = 0.0; - Thus not used
	double maxd;
	switch (dist_units) {
	case VIK_UNITS_DISTANCE_MILES:
		maxd = VIK_METERS_TO_MILES(trk->get_length_including_gaps());
		break;
	case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
		maxd = VIK_METERS_TO_NAUTICAL_MILES(trk->get_length_including_gaps());
		break;
	default:
		maxd = trk->get_length_including_gaps() / 1000.0;
		break;
	}

	/* Find suitable chunk index */
	double dummy = 0.0; // expect this to remain the same! (not that it's used)
	get_new_min_and_chunk_index(0, maxd, chunksd, G_N_ELEMENTS(chunksd), &dummy, &widgets->cid);

	// Reset before redrawing
	clear_images(pix, window, widgets);

	/* draw grid */
	for (i = 0; i <= LINES; i++) {
		char s[32];

		switch (dist_units) {
		case VIK_UNITS_DISTANCE_MILES:
			sprintf(s, _("%.1f miles"), ((LINES-i)*chunksd[widgets->cid]));
			break;
		case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
			sprintf(s, _("%.1f NM"), ((LINES-i)*chunksd[widgets->cid]));
			break;
		default:
			sprintf(s, _("%.1f km"), ((LINES-i)*chunksd[widgets->cid]));
			break;
		}

		draw_grid_y(window, image, widgets, pix, s, i);
	}

	draw_time_lines(window, image, pix, widgets);

	/* draw distance */
	unsigned int height = widgets->profile_height + MARGIN_Y;
	for (i = 0; i < widgets->profile_width; i++) {
		gdk_draw_line(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->dark_gc[3],
			      i + MARGIN_X, height, i + MARGIN_X, height - widgets->profile_height*(widgets->distances[i])/(chunksd[widgets->cid]*LINES));
	}

	// Show speed indicator
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets->w_show_dist_speed))) {
		GdkGC *dist_speed_gc = gdk_gc_new(gtk_widget_get_window(window));
		GdkColor color;
		gdk_color_parse("red", &color);
		gdk_gc_set_rgb_fg_color(dist_speed_gc, &color);

		double max_speed = 0;
		max_speed = widgets->max_speed * 110 / 100;

		// This is just an indicator - no actual values can be inferred by user
		for (i = 0; i < widgets->profile_width; i++) {
			int y_speed = widgets->profile_height - (widgets->profile_height * widgets->speeds[i])/max_speed;
			gdk_draw_rectangle(GDK_DRAWABLE(pix), dist_speed_gc, true, i+MARGIN_X-2, y_speed-2, 4, 4);
		}
		g_object_unref(G_OBJECT(dist_speed_gc));
	}

	/* draw border */
	gdk_draw_rectangle(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->black_gc, false, MARGIN_X, MARGIN_Y, widgets->profile_width-1, widgets->profile_height-1);

	g_object_unref(G_OBJECT(pix));

}

/**
 * Draw just the elevation/time image
 */
static void draw_et(GtkWidget * image, Track * trk, PropWidgets * widgets)
{
	unsigned int i;

	// Free previous allocation
	if (widgets->ats) {
		free(widgets->ats);
	}

	widgets->ats = trk->make_elevation_time_map(widgets->profile_width);

	if (widgets->ats == NULL) {
		return;
	}

	// Convert into appropriate units
	vik_units_height_t height_units = a_vik_get_units_height();
	if (height_units == VIK_UNITS_HEIGHT_FEET) {
		// Convert altitudes into feet units
		for (i = 0; i < widgets->profile_width; i++) {
			widgets->ats[i] = VIK_METERS_TO_FEET(widgets->ats[i]);
		}
	}
	// Otherwise leave in metres

	minmax_array(widgets->ats, &widgets->min_altitude, &widgets->max_altitude, true, widgets->profile_width);

	get_new_min_and_chunk_index(widgets->min_altitude, widgets->max_altitude, chunksa, G_N_ELEMENTS(chunksa), &widgets->draw_min_altitude_time, &widgets->ciat);

	// Assign locally
	double mina = widgets->draw_min_altitude_time;

	widgets->duration = widgets->trk->get_duration(true);
	// Negative time or other problem
	if (widgets->duration <= 0) {
		return;
	}

	GtkWidget *window = gtk_widget_get_toplevel(widgets->elev_time_box);
	GdkPixmap *pix = gdk_pixmap_new(gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1);

	gtk_image_set_from_pixmap(GTK_IMAGE(image), pix, NULL);

	// Reset before redrawing
	clear_images(pix, window, widgets);

	/* draw grid */
	for (i = 0; i <= LINES; i++) {
		char s[32];

		switch (height_units) {
		case VIK_UNITS_HEIGHT_METRES:
			sprintf(s, "%8dm", (int)(mina + (LINES-i)*chunksa[widgets->ciat]));
			break;
		case VIK_UNITS_HEIGHT_FEET:
			// NB values already converted into feet
			sprintf(s, "%8dft", (int)(mina + (LINES-i)*chunksa[widgets->ciat]));
			break;
		default:
			sprintf(s, "--");
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. height=%d\n", height_units);
		}

		draw_grid_y(window, image, widgets, pix, s, i);
	}

	draw_time_lines(window, image, pix, widgets);

	/* draw elevations */
	unsigned int height = widgets->profile_height + MARGIN_Y;
	for (i = 0; i < widgets->profile_width; i++) {
		gdk_draw_line(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->dark_gc[3],
			      i + MARGIN_X, height, i + MARGIN_X, height-widgets->profile_height*(widgets->ats[i]-mina)/(chunksa[widgets->ciat]*LINES));
	}

	// Show DEMS
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets->w_show_elev_dem)))  {
		GdkColor color;
		GdkGC *dem_alt_gc = gdk_gc_new(gtk_widget_get_window(window));
		gdk_color_parse("green", &color);
		gdk_gc_set_rgb_fg_color(dem_alt_gc, &color);

		int h2 = widgets->profile_height + MARGIN_Y; // Adjust height for x axis labelling offset
		int achunk = chunksa[widgets->ciat]*LINES;

		for (i = 0; i < widgets->profile_width; i++) {
			// This could be slow doing this each time...
			Trackpoint * tp = widgets->trk->get_closest_tp_by_percentage_time(((double)i/(double)widgets->profile_width), NULL);
			if (tp) {
				int16_t elev = a_dems_get_elev_by_coord(&(tp->coord), VIK_DEM_INTERPOL_SIMPLE);
				if (elev != VIK_DEM_INVALID_ELEVATION) {
					// Convert into height units
					if (a_vik_get_units_height() == VIK_UNITS_HEIGHT_FEET) {
						elev = VIK_METERS_TO_FEET(elev);
					}
					// No conversion needed if already in metres

					// offset is in current height units
					elev -= mina;

					// consider chunk size
					int y_alt = h2 - ((widgets->profile_height * elev)/achunk);
					gdk_draw_rectangle(GDK_DRAWABLE(pix), dem_alt_gc, true, i+MARGIN_X-2, y_alt-2, 4, 4);
				}
			}
		}
		g_object_unref(G_OBJECT(dem_alt_gc));
	}

	// Show speeds
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets->w_show_elev_speed))) {
		GdkColor color;
		// This is just an indicator - no actual values can be inferred by user
		GdkGC *elev_speed_gc = gdk_gc_new(gtk_widget_get_window(window));
		gdk_color_parse("red", &color);
		gdk_gc_set_rgb_fg_color(elev_speed_gc, &color);

		double max_speed = widgets->max_speed * 110 / 100;

		for (i = 0; i < widgets->profile_width; i++) {
			int y_speed = widgets->profile_height - (widgets->profile_height * widgets->speeds[i])/max_speed;
			gdk_draw_rectangle(GDK_DRAWABLE(pix), elev_speed_gc, true, i+MARGIN_X-2, y_speed-2, 4, 4);
		}

		g_object_unref(G_OBJECT(elev_speed_gc));
	}

	/* draw border */
	gdk_draw_rectangle(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->black_gc, false, MARGIN_X, MARGIN_Y, widgets->profile_width-1, widgets->profile_height-1);

	g_object_unref(G_OBJECT(pix));
}

/**
 * Draw just the speed/distance image
 */
static void draw_sd(GtkWidget * image, Track * trk, PropWidgets * widgets)
{
	double mins;
	unsigned int i;

	// Free previous allocation
	if (widgets->speeds_dist) {
		free(widgets->speeds_dist);
	}

	widgets->speeds_dist = trk->make_speed_dist_map(widgets->profile_width);
	if (widgets->speeds_dist == NULL) {
		return;
	}

	// Convert into appropriate units
	vik_units_speed_t speed_units = a_vik_get_units_speed();
	switch (speed_units) {
	case VIK_UNITS_SPEED_KILOMETRES_PER_HOUR:
		for (i = 0; i < widgets->profile_width; i++) {
			widgets->speeds_dist[i] = VIK_MPS_TO_KPH(widgets->speeds_dist[i]);
		}
		break;
	case VIK_UNITS_SPEED_MILES_PER_HOUR:
		for (i = 0; i < widgets->profile_width; i++) {
			widgets->speeds_dist[i] = VIK_MPS_TO_MPH(widgets->speeds_dist[i]);
		}
		break;
	case VIK_UNITS_SPEED_KNOTS:
		for (i = 0; i < widgets->profile_width; i++) {
			widgets->speeds_dist[i] = VIK_MPS_TO_KNOTS(widgets->speeds_dist[i]);
		}
		break;
	default:
		// VIK_UNITS_SPEED_METRES_PER_SECOND:
		// No need to convert as already in m/s
		break;
	}

	GtkWidget *window = gtk_widget_get_toplevel(widgets->speed_dist_box);
	GdkPixmap *pix = gdk_pixmap_new(gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1);

	gtk_image_set_from_pixmap(GTK_IMAGE(image), pix, NULL);

	// OK to resuse min_speed here
	minmax_array(widgets->speeds_dist, &widgets->min_speed, &widgets->max_speed_dist, false, widgets->profile_width);
	if (widgets->min_speed < 0.0) {
		widgets->min_speed = 0; /* splines sometimes give negative speeds */
	}

	/* Find suitable chunk index */
	get_new_min_and_chunk_index(widgets->min_speed, widgets->max_speed_dist, chunkss, G_N_ELEMENTS(chunkss), &widgets->draw_min_speed, &widgets->cisd);

	// Assign locally
	mins = widgets->draw_min_speed;

	// Reset before redrawing
	clear_images(pix, window, widgets);

	/* draw grid */
	for (i = 0; i <= LINES; i++) {
		char s[32];

		// NB: No need to convert here anymore as numbers are in the appropriate units
		switch (speed_units) {
		case VIK_UNITS_SPEED_KILOMETRES_PER_HOUR:
			sprintf(s, "%8dkm/h", (int)(mins + (LINES-i)*chunkss[widgets->cisd]));
			break;
		case VIK_UNITS_SPEED_MILES_PER_HOUR:
			sprintf(s, "%8dmph", (int)(mins + (LINES-i)*chunkss[widgets->cisd]));
			break;
		case VIK_UNITS_SPEED_METRES_PER_SECOND:
			sprintf(s, "%8dm/s", (int)(mins + (LINES-i)*chunkss[widgets->cisd]));
			break;
		case VIK_UNITS_SPEED_KNOTS:
			sprintf(s, "%8dknots", (int)(mins + (LINES-i)*chunkss[widgets->cisd]));
			break;
		default:
			sprintf(s, "--");
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. speed=%d\n", speed_units);
		}

		draw_grid_y(window, image, widgets, pix, s, i);
	}

	draw_distance_divisions(window, image, pix, widgets, a_vik_get_units_distance());

	/* draw speeds */
	unsigned int height = widgets->profile_height + MARGIN_Y;
	for (i = 0; i < widgets->profile_width; i++) {
		gdk_draw_line(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->dark_gc[3],
			      i + MARGIN_X, height, i + MARGIN_X, height - widgets->profile_height*(widgets->speeds_dist[i]-mins)/(chunkss[widgets->cisd]*LINES));
	}


	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets->w_show_sd_gps_speed))) {

		GdkGC *gps_speed_gc = gdk_gc_new(gtk_widget_get_window(window));
		GdkColor color;
		gdk_color_parse("red", &color);
		gdk_gc_set_rgb_fg_color(gps_speed_gc, &color);

		double dist = trk->get_length_including_gaps();
		double dist_tp = 0.0;

		GList *iter = trk->trackpoints;
		for (iter = iter->next; iter; iter = iter->next) {
			double gps_speed = ((Trackpoint *) iter->data)->speed;
			if (isnan(gps_speed))
				continue;
			switch (speed_units) {
			case VIK_UNITS_SPEED_KILOMETRES_PER_HOUR:
				gps_speed = VIK_MPS_TO_KPH(gps_speed);
				break;
			case VIK_UNITS_SPEED_MILES_PER_HOUR:
				gps_speed = VIK_MPS_TO_MPH(gps_speed);
				break;
			case VIK_UNITS_SPEED_KNOTS:
				gps_speed = VIK_MPS_TO_KNOTS(gps_speed);
				break;
			default:
				// VIK_UNITS_SPEED_METRES_PER_SECOND:
				// No need to convert as already in m/s
				break;
			}
			dist_tp += vik_coord_diff(&(((Trackpoint *) iter->data)->coord), &(((Trackpoint *) iter->prev->data)->coord));
			int x = MARGIN_X + (widgets->profile_width * dist_tp / dist);
			int y = height - widgets->profile_height*(gps_speed - mins)/(chunkss[widgets->cisd]*LINES);
			gdk_draw_rectangle(GDK_DRAWABLE(pix), gps_speed_gc, true, x-2, y-2, 4, 4);
		}
		g_object_unref(G_OBJECT(gps_speed_gc));
	}

	/* draw border */
	gdk_draw_rectangle(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->black_gc, false, MARGIN_X, MARGIN_Y, widgets->profile_width-1, widgets->profile_height-1);

	g_object_unref(G_OBJECT(pix));
}
#undef LINES



/**
 * Draw all graphs
 */
static void draw_all_graphs(GtkWidget * widget, PropWidgets * widgets, bool resized)
{
	GtkWidget * window = gtk_widget_get_toplevel(widget);

	// Draw elevations
	if (widgets->elev_box != NULL) {
		GList * child = gtk_container_get_children(GTK_CONTAINER(widgets->elev_box));
		draw_single_graph(window, widgets, resized, child, draw_elevations, blobby_altitude, false, &widgets->elev_graph_saved_img);
		g_list_free(child);
	}

	// Draw gradients
	if (widgets->gradient_box != NULL) {
		GList * child = gtk_container_get_children(GTK_CONTAINER(widgets->gradient_box));
		draw_single_graph(window, widgets, resized, child, draw_gradients, blobby_gradient, false, &widgets->gradient_graph_saved_img);
		g_list_free(child);
	}

	// Draw speeds
	if (widgets->speed_box != NULL) {
		GList * child = gtk_container_get_children(GTK_CONTAINER(widgets->speed_box));
		draw_single_graph(window, widgets, resized, child, draw_vt, blobby_speed, true, &widgets->speed_graph_saved_img);
		g_list_free(child);
	}

	// Draw Distances
	if (widgets->dist_box != NULL) {
		GList * child = gtk_container_get_children(GTK_CONTAINER(widgets->dist_box));
		draw_single_graph(window, widgets, resized, child, draw_dt, blobby_distance, true, &widgets->dist_graph_saved_img);
		g_list_free(child);
	}

	// Draw Elevations in timely manner
	if (widgets->elev_time_box != NULL) {
		GList * child = gtk_container_get_children(GTK_CONTAINER(widgets->elev_time_box));
		draw_single_graph(window, widgets, resized, child, draw_et, blobby_altitude_time, true, &widgets->elev_time_graph_saved_img);
		g_list_free(child);
	}

	// Draw speed distances
	if (widgets->speed_dist_box != NULL) {
		GList * child = gtk_container_get_children(GTK_CONTAINER(widgets->speed_dist_box));
		draw_single_graph(window, widgets, resized, child, draw_sd, blobby_speed_dist, true, &widgets->speed_dist_graph_saved_img);
		g_list_free(child);
	}

}



void draw_single_graph(GtkWidget * window, PropWidgets * widgets, bool resized, GList * child, draw_graph_fn_t draw_graph, get_blobby_fn_t get_blobby, bool by_time, PropSaved * saved_img)
{
	// Saved image no longer any good as we've resized, so we remove it here
	if (resized && saved_img->img) {
		g_object_unref(saved_img->img);
		saved_img->img = NULL;
		saved_img->saved = false;
	}

	draw_graph(GTK_WIDGET(child->data), widgets->trk, widgets);

	GtkWidget * image = GTK_WIDGET(child->data);

	// Ensure marker or blob are redrawn if necessary
	if (widgets->is_marker_drawn || widgets->is_blob_drawn) {

		double pc = NAN;
		if (by_time) {
			pc = tp_percentage_by_time(widgets->trk, widgets->marker_tp);
		} else {
			pc = tp_percentage_by_distance(widgets->trk, widgets->marker_tp, widgets->track_length_inc_gaps);
		}

		double x_blob = -MARGIN_X - 1.0; // i.e. Don't draw unless we get a valid value
		int y_blob = 0;
		if (widgets->is_blob_drawn) {
			double pc_blob = NAN;
			if (by_time) {
				pc_blob = tp_percentage_by_time(widgets->trk, widgets->blob_tp);
			} else {
				pc_blob = tp_percentage_by_distance(widgets->trk, widgets->blob_tp, widgets->track_length_inc_gaps);
			}
			if (!isnan(pc_blob)) {
				x_blob = (pc_blob * widgets->profile_width);
			}

			y_blob = get_blobby(x_blob, widgets);
		}

		double marker_x = -1.0; // i.e. Don't draw unless we get a valid value
		if (!isnan(pc)) {
			marker_x = (pc * widgets->profile_width) + MARGIN_X;
		}

		save_image_and_draw_graph_marks(image,
						marker_x,
						gtk_widget_get_style(window)->black_gc,
						x_blob+MARGIN_X,
						y_blob+MARGIN_Y,
						saved_img,
						widgets->profile_width,
						widgets->profile_height,
						&widgets->is_marker_drawn,
						&widgets->is_blob_drawn);
	}
}

/**
 * Configure/Resize the profile & speed/time images
 */
static bool configure_event(GtkWidget * widget, GdkEventConfigure * event, PropWidgets * widgets)
{
	if (widgets->configure_dialog) {
		// Determine size offsets between dialog size and size for images
		// Only on the initialisation of the dialog
		widgets->profile_width_offset = event->width - widgets->profile_width;
		widgets->profile_height_offset = event->height - widgets->profile_height;
		widgets->configure_dialog = false;

		// Without this the settting, the dialog will only grow in vertical size - one can not then make it smaller!
		gtk_widget_set_size_request(widget, widgets->profile_width+widgets->profile_width_offset, widgets->profile_height+widgets->profile_height_offset);

		// Allow resizing back down to a minimal size (especially useful if the initial size has been made bigger after restoring from the saved settings)
		GdkGeometry geom = { 600+widgets->profile_width_offset, 300+widgets->profile_height_offset, 0, 0, 0, 0, 0, 0, 0, 0, GDK_GRAVITY_STATIC };
		gdk_window_set_geometry_hints(gtk_widget_get_window(widget), &geom, GDK_HINT_MIN_SIZE);
	} else {
		widgets->profile_width_old = widgets->profile_width;
		widgets->profile_height_old = widgets->profile_height;
	}

	// Now adjust From Dialog size to get image size
	widgets->profile_width = event->width - widgets->profile_width_offset;
	widgets->profile_height = event->height - widgets->profile_height_offset;

	// ATM we receive configure_events when the dialog is moved and so no further action is necessary
	if (!widgets->configure_dialog &&
	    (widgets->profile_width_old == widgets->profile_width) && (widgets->profile_height_old == widgets->profile_height)) {

		return false;
	}

	// Draw stuff
	draw_all_graphs(widget, widgets, true);

	return false;
}

/**
 * Create height profile widgets including the image and callbacks
 */
GtkWidget * vik_trw_layer_create_profile(GtkWidget * window, PropWidgets * widgets, double * min_alt, double * max_alt)
{
	GdkPixmap *pix;
	GtkWidget *image;
	GtkWidget *eventbox;

	// First allocation
	widgets->altitudes = widgets->trk->make_elevation_map(widgets->profile_width);

	if (widgets->altitudes == NULL) {
		*min_alt = *max_alt = VIK_DEFAULT_ALTITUDE;
		return NULL;
	}

	minmax_array(widgets->altitudes, min_alt, max_alt, true, widgets->profile_width);

	pix = gdk_pixmap_new(gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1);
	image = gtk_image_new_from_pixmap(pix, NULL);

	g_object_unref(G_OBJECT(pix));

	eventbox = gtk_event_box_new();
	g_signal_connect(G_OBJECT(eventbox), "button_press_event", G_CALLBACK(track_profile_click), widgets);
	g_signal_connect(G_OBJECT(eventbox), "motion_notify_event", G_CALLBACK(track_profile_move), widgets);
	gtk_container_add(GTK_CONTAINER(eventbox), image);
	gtk_widget_set_events(eventbox, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_STRUCTURE_MASK);

	return eventbox;
}

/**
 * Create height profile widgets including the image and callbacks
 */
GtkWidget * vik_trw_layer_create_gradient(GtkWidget * window, PropWidgets * widgets)
{
	GdkPixmap *pix;
	GtkWidget *image;
	GtkWidget *eventbox;

	// First allocation
	widgets->gradients = widgets->trk->make_gradient_map(widgets->profile_width);

	if (widgets->gradients == NULL) {
		return NULL;
	}

	pix = gdk_pixmap_new(gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1);
	image = gtk_image_new_from_pixmap(pix, NULL);

	g_object_unref(G_OBJECT(pix));

	eventbox = gtk_event_box_new();
	g_signal_connect(G_OBJECT(eventbox), "button_press_event", G_CALLBACK(track_gradient_click), widgets);
	g_signal_connect(G_OBJECT(eventbox), "motion_notify_event", G_CALLBACK(track_gradient_move), widgets);
	gtk_container_add(GTK_CONTAINER(eventbox), image);
	gtk_widget_set_events(eventbox, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_STRUCTURE_MASK);

	return eventbox;
}

/**
 * Create speed/time widgets including the image and callbacks
 */
GtkWidget * vik_trw_layer_create_vtdiag(GtkWidget * window, PropWidgets * widgets)
{
	GdkPixmap *pix;
	GtkWidget *image;
	GtkWidget *eventbox;

	// First allocation
	widgets->speeds = widgets->trk->make_speed_map(widgets->profile_width);
	if (widgets->speeds == NULL) {
		return NULL;
	}

	pix = gdk_pixmap_new(gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1);
	image = gtk_image_new_from_pixmap(pix, NULL);

#if 0
	/* XXX this can go out, it's just a helpful dev tool */
	{
		int j;
		GdkGC **colors[8] = { gtk_widget_get_style(window)->bg_gc,
				      gtk_widget_get_style(window)->fg_gc,
				      gtk_widget_get_style(window)->light_gc,
				      gtk_widget_get_style(window)->dark_gc,
				      gtk_widget_get_style(window)->mid_gc,
				      gtk_widget_get_style(window)->text_gc,
				      gtk_widget_get_style(window)->base_gc,
				      gtk_widget_get_style(window)->text_aa_gc };
		for (i=0; i<5; i++) {
			for (j=0; j<8; j++) {
				gdk_draw_rectangle(GDK_DRAWABLE(pix), colors[j][i],
						   true, i*20, j*20, 20, 20);
				gdk_draw_rectangle(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->black_gc,
						   false, i*20, j*20, 20, 20);
			}
		}
	}
#endif

	g_object_unref(G_OBJECT(pix));

	eventbox = gtk_event_box_new();
	g_signal_connect(G_OBJECT(eventbox), "button_press_event", G_CALLBACK(track_vt_click), widgets);
	g_signal_connect(G_OBJECT(eventbox), "motion_notify_event", G_CALLBACK(track_vt_move), widgets);
	gtk_container_add(GTK_CONTAINER(eventbox), image);
	gtk_widget_set_events(eventbox, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);

	return eventbox;
}

/**
 * Create distance / time widgets including the image and callbacks
 */
GtkWidget * vik_trw_layer_create_dtdiag(GtkWidget * window, PropWidgets * widgets)
{
	GdkPixmap *pix;
	GtkWidget *image;
	GtkWidget *eventbox;

	// First allocation
	widgets->distances = widgets->trk->make_distance_map(widgets->profile_width);
	if (widgets->distances == NULL) {
		return NULL;
	}

	pix = gdk_pixmap_new(gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1);
	image = gtk_image_new_from_pixmap(pix, NULL);

	g_object_unref(G_OBJECT(pix));

	eventbox = gtk_event_box_new();
	g_signal_connect(G_OBJECT(eventbox), "button_press_event", G_CALLBACK(track_dt_click), widgets);
	g_signal_connect(G_OBJECT(eventbox), "motion_notify_event", G_CALLBACK(track_dt_move), widgets);
	//g_signal_connect_swapped(G_OBJECT(eventbox), "destroy", G_CALLBACK(g_free), widgets);
	gtk_container_add(GTK_CONTAINER(eventbox), image);
	gtk_widget_set_events(eventbox, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);

	return eventbox;
}

/**
 * Create elevation / time widgets including the image and callbacks
 */
GtkWidget * vik_trw_layer_create_etdiag(GtkWidget * window, PropWidgets * widgets)
{
	GdkPixmap *pix;
	GtkWidget *image;
	GtkWidget *eventbox;

	// First allocation
	widgets->ats = widgets->trk->make_elevation_time_map(widgets->profile_width);
	if (widgets->ats == NULL) {
		return NULL;
	}

	pix = gdk_pixmap_new(gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1);
	image = gtk_image_new_from_pixmap(pix, NULL);

	g_object_unref(G_OBJECT(pix));

	eventbox = gtk_event_box_new();
	g_signal_connect(G_OBJECT(eventbox), "button_press_event", G_CALLBACK(track_et_click), widgets);
	g_signal_connect(G_OBJECT(eventbox), "motion_notify_event", G_CALLBACK(track_et_move), widgets);
	gtk_container_add(GTK_CONTAINER(eventbox), image);
	gtk_widget_set_events(eventbox, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);

	return eventbox;
}

/**
 * Create speed/distance widgets including the image and callbacks
 */
GtkWidget * vik_trw_layer_create_sddiag(GtkWidget * window, PropWidgets * widgets)
{
	GdkPixmap *pix;
	GtkWidget *image;
	GtkWidget *eventbox;

	// First allocation
	widgets->speeds_dist = widgets->trk->make_speed_dist_map(widgets->profile_width); // kamilFIXME
	if (widgets->speeds_dist == NULL) {
		return NULL;
	}

	pix = gdk_pixmap_new(gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1);
	image = gtk_image_new_from_pixmap(pix, NULL);

	g_object_unref(G_OBJECT(pix));

	eventbox = gtk_event_box_new();
	g_signal_connect(G_OBJECT(eventbox), "button_press_event", G_CALLBACK(track_sd_click), widgets);
	g_signal_connect(G_OBJECT(eventbox), "motion_notify_event", G_CALLBACK(track_sd_move), widgets);
	gtk_container_add(GTK_CONTAINER(eventbox), image);
	gtk_widget_set_events(eventbox, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);

	return eventbox;
}
#undef MARGIN_X

#define VIK_SETTINGS_TRACK_PROFILE_WIDTH "track_profile_display_width"
#define VIK_SETTINGS_TRACK_PROFILE_HEIGHT "track_profile_display_height"

static void save_values(PropWidgets * widgets)
{
	// Session settings
	a_settings_set_integer(VIK_SETTINGS_TRACK_PROFILE_WIDTH, widgets->profile_width);
	a_settings_set_integer(VIK_SETTINGS_TRACK_PROFILE_HEIGHT, widgets->profile_height);

	// Just for this session ATM
	if (widgets->w_show_dem)
		show_dem                = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets->w_show_dem));
	if (widgets->w_show_alt_gps_speed)
		show_alt_gps_speed      = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets->w_show_alt_gps_speed));
	if (widgets->w_show_gps_speed)
		show_gps_speed          = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets->w_show_gps_speed));
	if (widgets->w_show_gradient_gps_speed)
		show_gradient_gps_speed = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets->w_show_gradient_gps_speed));
	if (widgets->w_show_dist_speed)
		show_dist_speed         = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets->w_show_dist_speed));
	if (widgets->w_show_elev_dem)
		show_elev_dem           = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets->w_show_elev_dem));
	if (widgets->w_show_elev_speed)
		show_elev_speed         = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets->w_show_elev_speed));
	if (widgets->w_show_sd_gps_speed)
		show_sd_gps_speed       = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets->w_show_sd_gps_speed));
}

static void destroy_cb(GtkDialog * dialog, PropWidgets * widgets)
{
	save_values(widgets);
	prop_widgets_free(widgets);
}

static void propwin_response_cb(GtkDialog * dialog, int resp, PropWidgets * widgets)
{
	Track * trk = widgets->trk;
	LayerTRW * trw = widgets->trw;
	bool keep_dialog = false;

	/* FIXME: check and make sure the track still exists before doing anything to it */
	/* Note: destroying diaglog (eg, parent window exit) won't give "response" */
	switch (resp) {
	case GTK_RESPONSE_DELETE_EVENT: /* received delete event (not from buttons) */
	case GTK_RESPONSE_REJECT:
		break;
	case GTK_RESPONSE_ACCEPT:
		trk->set_comment(gtk_entry_get_text(GTK_ENTRY(widgets->w_comment)));
		trk->set_description(gtk_entry_get_text(GTK_ENTRY(widgets->w_description)));
		trk->set_source(gtk_entry_get_text(GTK_ENTRY(widgets->w_source)));
		trk->set_type(gtk_entry_get_text(GTK_ENTRY(widgets->w_type)));
		gtk_color_button_get_color(GTK_COLOR_BUTTON(widgets->w_color), &(trk->color));
		trk->draw_name_mode = (TrackDrawnameType) gtk_combo_box_get_active(GTK_COMBO_BOX(widgets->w_namelabel));
		trk->max_number_dist_labels = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widgets->w_number_distlabels));
		widgets->trw->update_treeview(widgets->trk);
		trw->emit_update();
		break;
	case VIK_TRW_LAYER_PROPWIN_REVERSE:
		trk->reverse();
		trw->emit_update();
		break;
	case VIK_TRW_LAYER_PROPWIN_DEL_DUP:
		trk->remove_dup_points(); // NB ignore the returned answer
		// As we could have seen the nuber of dulplicates that would be deleted in the properties statistics tab,
		//   choose not to inform the user unnecessarily

		/* above operation could have deleted current_tp or last_tp */
		trw->cancel_tps_of_track(trk);
		trw->emit_update();
		break;
	case VIK_TRW_LAYER_PROPWIN_SPLIT: {
		/* get new tracks, add them and then the delete old one. old can still exist on clipboard. */
		unsigned int ntracks;

		Track **tracks = trk->split_into_segments(&ntracks);
		char *new_tr_name;
		unsigned int i;
		for (i = 0; i < ntracks; i++) {
			if (tracks[i]) {
				new_tr_name = trw->new_unique_sublayer_name(widgets->trk->is_route ? VIK_TRW_LAYER_SUBLAYER_ROUTE : VIK_TRW_LAYER_SUBLAYER_TRACK,
									    widgets->trk->name);
				if (widgets->trk->is_route) {
					trw->add_route(tracks[i], new_tr_name);
				} else {
					trw->add_track(tracks[i], new_tr_name);
				}
				tracks[i]->calculate_bounds();

				free(new_tr_name);
			}
		}
		if (tracks) {
			free(tracks);
			/* Don't let track destroy this dialog */
			trk->clear_property_dialog();
			if (widgets->trk->is_route) {
				trw->delete_route(trk);
			} else {
				trw->delete_track(trk);
			}
			trw->emit_update(); /* chase thru the hoops */
		}
	}
		break;
	case VIK_TRW_LAYER_PROPWIN_SPLIT_MARKER: {
		GList *iter = trk->trackpoints;
		while ((iter = iter->next)) {
			if (widgets->marker_tp == ((Trackpoint *) iter->data)) {
				break;
			}
		}
		if (iter == NULL) {
			a_dialog_msg(VIK_GTK_WINDOW_FROM_LAYER(trw->vl), GTK_MESSAGE_ERROR,
				     _("Failed spliting track. Track unchanged"), NULL);
			keep_dialog = true;
			break;
		}

		char *r_name = trw->new_unique_sublayer_name(widgets->trk->is_route ? VIK_TRW_LAYER_SUBLAYER_ROUTE : VIK_TRW_LAYER_SUBLAYER_TRACK,
							     widgets->trk->name);
		iter->prev->next = NULL;
		iter->prev = NULL;
		Track * trk_right = new Track();
		if (trk->comment) {
			trk_right->set_comment(trk->comment);
		}
		trk_right->visible = trk->visible;
		trk_right->is_route = trk->is_route;
		trk_right->trackpoints = iter;

		if (widgets->trk->is_route) {
			trw->add_route(trk_right, r_name);
		} else {
			trw->add_track(trk_right, r_name);
		}
		trk->calculate_bounds();
		trk_right->calculate_bounds();

		free(r_name);

		trw->emit_update();
	}
		break;
	default:
		fprintf(stderr, "DEBUG: unknown response\n");
		return;
	}

	/* Keep same behaviour for now: destroy dialog if click on any button */
	if (!keep_dialog) {
		trk->clear_property_dialog();
		gtk_widget_destroy(GTK_WIDGET(dialog));
	}
}

/**
 * Force a redraw when checkbutton has been toggled to show/hide that information
 */
static void checkbutton_toggle_cb(GtkToggleButton * togglebutton, PropWidgets * widgets, void * dummy)
{
	// Even though not resized, we'll pretend it is -
	//  as this invalidates the saved images (since the image may have changed)
	draw_all_graphs(widgets->dialog, widgets, true);
}

/**
 *  Create the widgets for the given graph tab
 */
static GtkWidget * create_graph_page(GtkWidget *graph,
				     const char *markup,
				     GtkWidget *value,
				     const char *markup2,
				     GtkWidget *value2,
				     const char *markup3,
				     GtkWidget *value3,
				     GtkWidget *checkbutton1,
				      bool checkbutton1_default,
				     GtkWidget *checkbutton2,
				     bool checkbutton2_default)
{
	GtkWidget *hbox = gtk_hbox_new (false, 10);
	GtkWidget *vbox = gtk_vbox_new (false, 10);
	GtkWidget *label = gtk_label_new (NULL);
	GtkWidget *label2 = gtk_label_new (NULL);
	GtkWidget *label3 = gtk_label_new (NULL);
	gtk_box_pack_start (GTK_BOX(vbox), graph, false, false, 0);
	gtk_label_set_markup (GTK_LABEL(label), markup);
	gtk_label_set_markup (GTK_LABEL(label2), markup2);
	gtk_label_set_markup (GTK_LABEL(label3), markup3);
	gtk_box_pack_start (GTK_BOX(hbox), label, false, false, 0);
	gtk_box_pack_start (GTK_BOX(hbox), value, false, false, 0);
	gtk_box_pack_start (GTK_BOX(hbox), label2, false, false, 0);
	gtk_box_pack_start (GTK_BOX(hbox), value2, false, false, 0);
	if (value3) {
		gtk_box_pack_start (GTK_BOX(hbox), label3, false, false, 0);
		gtk_box_pack_start (GTK_BOX(hbox), value3, false, false, 0);
	}
	if (checkbutton2) {
		gtk_box_pack_end (GTK_BOX(hbox), checkbutton2, false, false, 0);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(checkbutton2), checkbutton2_default);
	}
	if (checkbutton1) {
		gtk_box_pack_end (GTK_BOX(hbox), checkbutton1, false, false, 0);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(checkbutton1), checkbutton1_default);
	}
	gtk_box_pack_start (GTK_BOX(vbox), hbox, false, false, 0);

	return vbox;
}

static GtkWidget * create_table(int cnt, char * labels[], GtkWidget * contents[])
{
	GtkTable * table;
	int i;

	table = GTK_TABLE(gtk_table_new(cnt, 2, false));
	gtk_table_set_col_spacing(table, 0, 10);
	for (i = 0; i < cnt; i++) {
		GtkWidget *label;

		// Settings so the text positioning only moves around vertically when the dialog is resized
		// This also gives more room to see the track comment
		label = gtk_label_new(NULL);
		gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5); // Position text centrally in vertical plane
		gtk_label_set_markup(GTK_LABEL(label), _(labels[i]));
		gtk_table_attach(table, label, 0, 1, i, i+1, GTK_FILL, GTK_SHRINK, 0, 0);
		if (GTK_IS_MISC(contents[i])) {
			gtk_misc_set_alignment(GTK_MISC(contents[i]), 0, 0.5);
		}
		if (GTK_IS_COLOR_BUTTON(contents[i]) || GTK_IS_COMBO_BOX(contents[i])) {
			// Buttons compressed - otherwise look weird (to me) if vertically massive
			gtk_table_attach(table, contents[i], 1, 2, i, i+1, GTK_FILL, GTK_SHRINK, 0, 5);
		} else {
			// Expand for comments + descriptions / labels
			gtk_table_attach_defaults(table, contents[i], 1, 2, i, i+1);
		}
	}

	return GTK_WIDGET (table);
}

void vik_trw_layer_propwin_run(GtkWindow *parent,
			       LayerTRW * layer,
			       Track * trk,
			       void * panel,
			       Viewport * viewport,
			       bool start_on_stats)
{
	PropWidgets * widgets = prop_widgets_new();
	widgets->trw = layer;
	widgets->viewport = viewport;
	widgets->panel = (LayersPanel *) panel;
	widgets->trk = trk;

	int profile_size_value;
	// Ensure minimum values
	widgets->profile_width = 600;
	if (a_settings_get_integer(VIK_SETTINGS_TRACK_PROFILE_WIDTH, &profile_size_value)) {
		if (profile_size_value > widgets->profile_width) {
			widgets->profile_width = profile_size_value;
		}
	}

	widgets->profile_height = 300;
	if (a_settings_get_integer(VIK_SETTINGS_TRACK_PROFILE_HEIGHT, &profile_size_value)) {
		if (profile_size_value > widgets->profile_height) {
			widgets->profile_height = profile_size_value;
		}
	}

	char * title = g_strdup_printf(_("%s - Track Properties"), trk->name);
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
	g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(propwin_response_cb), widgets);

	free(title);
	GtkWidget *table;
	double tr_len;
	unsigned long tp_count;
	unsigned int seg_count;

	double min_alt, max_alt;
	widgets->elev_box = vik_trw_layer_create_profile(GTK_WIDGET(parent), widgets, &min_alt, &max_alt);
	widgets->gradient_box = vik_trw_layer_create_gradient(GTK_WIDGET(parent), widgets);
	widgets->speed_box = vik_trw_layer_create_vtdiag(GTK_WIDGET(parent), widgets);
	widgets->dist_box = vik_trw_layer_create_dtdiag(GTK_WIDGET(parent), widgets);
	widgets->elev_time_box = vik_trw_layer_create_etdiag(GTK_WIDGET(parent), widgets);
	widgets->speed_dist_box = vik_trw_layer_create_sddiag(GTK_WIDGET(parent), widgets);
	GtkWidget *graphs = gtk_notebook_new();

	GtkWidget *content_prop[20];
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

	// Properties
	widgets->w_comment = gtk_entry_new();
	if (trk->comment) {
		gtk_entry_set_text(GTK_ENTRY(widgets->w_comment), trk->comment);
	}
	content_prop[cnt_prop++] = widgets->w_comment;

	widgets->w_description = gtk_entry_new();
	if (trk->description) {
		gtk_entry_set_text(GTK_ENTRY(widgets->w_description), trk->description);
	}
	content_prop[cnt_prop++] = widgets->w_description;

	widgets->w_source = gtk_entry_new();
	if (trk->source) {
		gtk_entry_set_text(GTK_ENTRY(widgets->w_source), trk->source);
	}
	content_prop[cnt_prop++] = widgets->w_source;

	widgets->w_type = gtk_entry_new();
	if (trk->type) {
		gtk_entry_set_text(GTK_ENTRY(widgets->w_type), trk->type);
	}
	content_prop[cnt_prop++] = widgets->w_type;

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

	table = create_table(cnt_prop, label_texts, content_prop);

	gtk_notebook_append_page(GTK_NOTEBOOK(graphs), GTK_WIDGET(table), gtk_label_new(_("Properties")));

	// Statistics
	GtkWidget *content[20];
	int cnt = 0;

	vik_units_distance_t dist_units = a_vik_get_units_distance();

	// NB This value not shown yet - but is used by internal calculations
	widgets->track_length_inc_gaps = trk->get_length_including_gaps();

	tr_len = widgets->track_length = trk->get_length();
	switch (dist_units) {
	case VIK_UNITS_DISTANCE_KILOMETRES:
		snprintf(tmp_buf, sizeof(tmp_buf), "%.2f km", tr_len/1000.0);
		break;
	case VIK_UNITS_DISTANCE_MILES:
		snprintf(tmp_buf, sizeof(tmp_buf), "%.2f miles", VIK_METERS_TO_MILES(tr_len));
		break;
	case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
		snprintf(tmp_buf, sizeof(tmp_buf), "%.2f NM", VIK_METERS_TO_NAUTICAL_MILES(tr_len));
		break;
	default:
		fprintf(stderr, "CRITICAL: Houston, we've had a problem. distance=%d\n", dist_units);
	}
	widgets->w_track_length = content[cnt++] = ui_label_new_selectable(tmp_buf);

	tp_count = trk->get_tp_count();
	snprintf(tmp_buf, sizeof(tmp_buf), "%lu", tp_count);
	widgets->w_tp_count = content[cnt++] = ui_label_new_selectable(tmp_buf);

	seg_count = trk->get_segment_count() ;
	snprintf(tmp_buf, sizeof(tmp_buf), "%u", seg_count);
	widgets->w_segment_count = content[cnt++] = ui_label_new_selectable(tmp_buf);

	snprintf(tmp_buf, sizeof(tmp_buf), "%lu", trk->get_dup_point_count());
	widgets->w_duptp_count = content[cnt++] = ui_label_new_selectable(tmp_buf);

	vik_units_speed_t speed_units = a_vik_get_units_speed();
	tmp_speed = trk->get_max_speed();
	if (tmp_speed == 0) {
		snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
	} else {
		switch (speed_units) {
		case VIK_UNITS_SPEED_KILOMETRES_PER_HOUR:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.2f km/h", VIK_MPS_TO_KPH(tmp_speed));
			break;
		case VIK_UNITS_SPEED_MILES_PER_HOUR:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.2f mph", VIK_MPS_TO_MPH(tmp_speed));
			break;
		case VIK_UNITS_SPEED_METRES_PER_SECOND:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.2f m/s", tmp_speed);
			break;
		case VIK_UNITS_SPEED_KNOTS:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.2f knots", VIK_MPS_TO_KNOTS(tmp_speed));
			break;
		default:
			snprintf(tmp_buf, sizeof(tmp_buf), "--");
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. speed=%d\n", speed_units);
		}
	}
	widgets->w_max_speed = content[cnt++] = ui_label_new_selectable(tmp_buf);

	tmp_speed = trk->get_average_speed();
	if (tmp_speed == 0) {
		snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
	} else {
		switch (speed_units) {
		case VIK_UNITS_SPEED_KILOMETRES_PER_HOUR:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.2f km/h", VIK_MPS_TO_KPH(tmp_speed));
			break;
		case VIK_UNITS_SPEED_MILES_PER_HOUR:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.2f mph", VIK_MPS_TO_MPH(tmp_speed));
			break;
		case VIK_UNITS_SPEED_METRES_PER_SECOND:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.2f m/s", tmp_speed);
			break;
		case VIK_UNITS_SPEED_KNOTS:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.2f knots", VIK_MPS_TO_KNOTS(tmp_speed));
			break;
		default:
			snprintf(tmp_buf, sizeof(tmp_buf), "--");
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. speed=%d\n", speed_units);
		}
	}
	widgets->w_avg_speed = content[cnt++] = ui_label_new_selectable(tmp_buf);

	// Use 60sec as the default period to be considered stopped
	//  this is the TrackWaypoint draw stops default value 'trw->stop_length'
	//  however this variable is not directly accessible - and I don't expect it's often changed from the default
	//  so ATM just put in the number
	tmp_speed = trk->get_average_speed_moving(60);
	if (tmp_speed == 0) {
		snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
	} else {
		switch (speed_units) {
		case VIK_UNITS_SPEED_KILOMETRES_PER_HOUR:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.2f km/h", VIK_MPS_TO_KPH(tmp_speed));
			break;
		case VIK_UNITS_SPEED_MILES_PER_HOUR:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.2f mph", VIK_MPS_TO_MPH(tmp_speed));
			break;
		case VIK_UNITS_SPEED_METRES_PER_SECOND:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.2f m/s", tmp_speed);
			break;
		case VIK_UNITS_SPEED_KNOTS:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.2f knots", VIK_MPS_TO_KNOTS(tmp_speed));
			break;
		default:
			snprintf(tmp_buf, sizeof(tmp_buf), "--");
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. speed=%d\n", speed_units);
		}
	}
	widgets->w_mvg_speed = content[cnt++] = ui_label_new_selectable(tmp_buf);

	switch (dist_units) {
	case VIK_UNITS_DISTANCE_KILOMETRES:
		// Even though kilometres, the average distance between points is going to be quite small so keep in metres
		snprintf(tmp_buf, sizeof(tmp_buf), "%.2f m", (tp_count - seg_count) == 0 ? 0 : tr_len / (tp_count - seg_count));
		break;
	case VIK_UNITS_DISTANCE_MILES:
		snprintf(tmp_buf, sizeof(tmp_buf), "%.3f miles", (tp_count - seg_count) == 0 ? 0 : VIK_METERS_TO_MILES(tr_len / (tp_count - seg_count)));
		break;
	case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
		snprintf(tmp_buf, sizeof(tmp_buf), "%.3f NM", (tp_count - seg_count) == 0 ? 0 : VIK_METERS_TO_NAUTICAL_MILES(tr_len / (tp_count - seg_count)));
		break;
	default:
		fprintf(stderr, "CRITICAL: Houston, we've had a problem. distance=%d\n", dist_units);
	}
	widgets->w_avg_dist = content[cnt++] = ui_label_new_selectable(tmp_buf);

	vik_units_height_t height_units = a_vik_get_units_height();
	if (min_alt == VIK_DEFAULT_ALTITUDE) {
		snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
	} else {
		switch (height_units) {
		case VIK_UNITS_HEIGHT_METRES:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.0f m - %.0f m", min_alt, max_alt);
			break;
		case VIK_UNITS_HEIGHT_FEET:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.0f feet - %.0f feet", VIK_METERS_TO_FEET(min_alt), VIK_METERS_TO_FEET(max_alt));
			break;
		default:
			snprintf(tmp_buf, sizeof(tmp_buf), "--");
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. height=%d\n", height_units);
		}
	}
	widgets->w_elev_range = content[cnt++] = ui_label_new_selectable(tmp_buf);

	trk->get_total_elevation_gain(&max_alt, &min_alt);
	if (min_alt == VIK_DEFAULT_ALTITUDE) {
		snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
	} else {
		switch (height_units) {
		case VIK_UNITS_HEIGHT_METRES:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.0f m / %.0f m", max_alt, min_alt);
			break;
		case VIK_UNITS_HEIGHT_FEET:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.0f feet / %.0f feet", VIK_METERS_TO_FEET(max_alt), VIK_METERS_TO_FEET(min_alt));
			break;
		default:
			snprintf(tmp_buf, sizeof(tmp_buf), "--");
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. height=%d\n", height_units);
		}
	}
	widgets->w_elev_gain = content[cnt++] = ui_label_new_selectable(tmp_buf);

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

	if (trk->trackpoints && ((Trackpoint *) trk->trackpoints->data)->timestamp) {
		time_t t1, t2;
		t1 = ((Trackpoint *) trk->trackpoints->data)->timestamp;
		t2 = ((Trackpoint *) g_list_last(trk->trackpoints)->data)->timestamp;

		VikCoord vc;
		// Notional center of a track is simply an average of the bounding box extremities
		struct LatLon center = { (trk->bbox.north+trk->bbox.south)/2, (trk->bbox.east+trk->bbox.west)/2 };
		vik_coord_load_from_latlon(&vc, layer->get_coord_mode(), &center);

		widgets->tz = vu_get_tz_at_location(&vc);

		char *msg;
		msg = vu_get_time_string(&t1, "%c", &vc, widgets->tz);
		widgets->w_time_start = content[cnt++] = ui_label_new_selectable(msg);
		free(msg);

		msg = vu_get_time_string(&t2, "%c", &vc, widgets->tz);
		widgets->w_time_end = content[cnt++] = ui_label_new_selectable(msg);
		free(msg);

		int total_duration_s = (int)(t2-t1);
		int segments_duration_s = (int) trk->get_duration(false);
		int total_duration_m = total_duration_s/60;
		int segments_duration_m = segments_duration_s/60;
		snprintf(tmp_buf, sizeof(tmp_buf), _("%d minutes - %d minutes moving"), total_duration_m, segments_duration_m);
		widgets->w_time_dur = content[cnt++] = ui_label_new_selectable(tmp_buf);

		// A tooltip to show in more readable hours:minutes
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

	gtk_notebook_append_page(GTK_NOTEBOOK(graphs), GTK_WIDGET(table), gtk_label_new(_("Statistics")));

	if (widgets->elev_box) {
		GtkWidget *page = NULL;
		widgets->w_cur_dist = ui_label_new_selectable(_("No Data"));
		widgets->w_cur_elevation = ui_label_new_selectable(_("No Data"));
		widgets->w_show_dem = gtk_check_button_new_with_mnemonic(_("Show D_EM"));
		widgets->w_show_alt_gps_speed = gtk_check_button_new_with_mnemonic(_("Show _GPS Speed"));
		page = create_graph_page(widgets->elev_box,
					 _("<b>Track Distance:</b>"), widgets->w_cur_dist,
					 _("<b>Track Height:</b>"), widgets->w_cur_elevation,
					 NULL, NULL,
					 widgets->w_show_dem, show_dem,
					 widgets->w_show_alt_gps_speed, show_alt_gps_speed);
		g_signal_connect(widgets->w_show_dem, "toggled", G_CALLBACK (checkbutton_toggle_cb), widgets);
		g_signal_connect(widgets->w_show_alt_gps_speed, "toggled", G_CALLBACK (checkbutton_toggle_cb), widgets);
		gtk_notebook_append_page(GTK_NOTEBOOK(graphs), page, gtk_label_new(_("Elevation-distance")));
	}

	if (widgets->gradient_box) {
		GtkWidget *page = NULL;
		widgets->w_cur_gradient_dist = ui_label_new_selectable(_("No Data"));
		widgets->w_cur_gradient_gradient = ui_label_new_selectable(_("No Data"));
		widgets->w_show_gradient_gps_speed = gtk_check_button_new_with_mnemonic(_("Show _GPS Speed"));
		page = create_graph_page(widgets->gradient_box,
					 _("<b>Track Distance:</b>"), widgets->w_cur_gradient_dist,
					 _("<b>Track Gradient:</b>"), widgets->w_cur_gradient_gradient,
					 NULL, NULL,
					 widgets->w_show_gradient_gps_speed, show_gradient_gps_speed,
					 NULL, false);
		g_signal_connect(widgets->w_show_gradient_gps_speed, "toggled", G_CALLBACK (checkbutton_toggle_cb), widgets);
		gtk_notebook_append_page(GTK_NOTEBOOK(graphs), page, gtk_label_new(_("Gradient-distance")));
	}

	if (widgets->speed_box) {
		GtkWidget *page = NULL;
		widgets->w_cur_time = ui_label_new_selectable(_("No Data"));
		widgets->w_cur_speed = ui_label_new_selectable(_("No Data"));
		widgets->w_cur_time_real = ui_label_new_selectable(_("No Data"));
		widgets->w_show_gps_speed = gtk_check_button_new_with_mnemonic(_("Show _GPS Speed"));
		page = create_graph_page(widgets->speed_box,
					 _("<b>Track Time:</b>"), widgets->w_cur_time,
					 _("<b>Track Speed:</b>"), widgets->w_cur_speed,
					 _("<b>Time/Date:</b>"), widgets->w_cur_time_real,
					 widgets->w_show_gps_speed, show_gps_speed,
					 NULL, false);
		g_signal_connect(widgets->w_show_gps_speed, "toggled", G_CALLBACK (checkbutton_toggle_cb), widgets);
		gtk_notebook_append_page(GTK_NOTEBOOK(graphs), page, gtk_label_new(_("Speed-time")));
	}

	if (widgets->dist_box) {
		GtkWidget *page = NULL;
		widgets->w_cur_dist_time = ui_label_new_selectable(_("No Data"));
		widgets->w_cur_dist_dist = ui_label_new_selectable(_("No Data"));
		widgets->w_cur_dist_time_real = ui_label_new_selectable(_("No Data"));
		widgets->w_show_dist_speed = gtk_check_button_new_with_mnemonic(_("Show S_peed"));
		page = create_graph_page(widgets->dist_box,
					 _("<b>Track Distance:</b>"), widgets->w_cur_dist_dist,
					 _("<b>Track Time:</b>"), widgets->w_cur_dist_time,
					 _("<b>Time/Date:</b>"), widgets->w_cur_dist_time_real,
					  widgets->w_show_dist_speed, show_dist_speed,
					 NULL, false);
		g_signal_connect(widgets->w_show_dist_speed, "toggled", G_CALLBACK (checkbutton_toggle_cb), widgets);
		gtk_notebook_append_page(GTK_NOTEBOOK(graphs), page, gtk_label_new(_("Distance-time")));
	}

	if (widgets->elev_time_box) {
		GtkWidget *page = NULL;
		widgets->w_cur_elev_time = ui_label_new_selectable(_("No Data"));
		widgets->w_cur_elev_elev = ui_label_new_selectable(_("No Data"));
		widgets->w_cur_elev_time_real = ui_label_new_selectable(_("No Data"));
		widgets->w_show_elev_speed = gtk_check_button_new_with_mnemonic(_("Show S_peed"));
		widgets->w_show_elev_dem = gtk_check_button_new_with_mnemonic(_("Show D_EM"));
		page = create_graph_page(widgets->elev_time_box,
					 _("<b>Track Time:</b>"), widgets->w_cur_elev_time,
					 _("<b>Track Height:</b>"), widgets->w_cur_elev_elev,
					 _("<b>Time/Date:</b>"), widgets->w_cur_elev_time_real,
					 widgets->w_show_elev_dem, show_elev_dem,
					 widgets->w_show_elev_speed, show_elev_speed);
		g_signal_connect(widgets->w_show_elev_dem, "toggled", G_CALLBACK (checkbutton_toggle_cb), widgets);
		g_signal_connect(widgets->w_show_elev_speed, "toggled", G_CALLBACK (checkbutton_toggle_cb), widgets);
		gtk_notebook_append_page(GTK_NOTEBOOK(graphs), page, gtk_label_new(_("Elevation-time")));
	}

	if (widgets->speed_dist_box) {
		GtkWidget *page = NULL;
		widgets->w_cur_speed_dist = ui_label_new_selectable(_("No Data"));
		widgets->w_cur_speed_speed = ui_label_new_selectable(_("No Data"));
		widgets->w_show_sd_gps_speed = gtk_check_button_new_with_mnemonic(_("Show _GPS Speed"));
		page = create_graph_page(widgets->speed_dist_box,
					 _("<b>Track Distance:</b>"), widgets->w_cur_speed_dist,
					 _("<b>Track Speed:</b>"), widgets->w_cur_speed_speed,
					 NULL, NULL,
					 widgets->w_show_sd_gps_speed, show_sd_gps_speed,
					 NULL, false);
		g_signal_connect(widgets->w_show_sd_gps_speed, "toggled", G_CALLBACK (checkbutton_toggle_cb), widgets);
		gtk_notebook_append_page(GTK_NOTEBOOK(graphs), page, gtk_label_new(_("Speed-distance")));
	}

	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), graphs, false, false, 0);

	gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), VIK_TRW_LAYER_PROPWIN_SPLIT_MARKER, false);
	if (seg_count <= 1) {
		gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), VIK_TRW_LAYER_PROPWIN_SPLIT, false);
	}
	if (trk->get_dup_point_count() <= 0) {
		gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), VIK_TRW_LAYER_PROPWIN_DEL_DUP, false);
	}

	// On dialog realization configure_event causes the graphs to be initially drawn
	widgets->configure_dialog = true;
	g_signal_connect(G_OBJECT(dialog), "configure-event", G_CALLBACK (configure_event), widgets);

	g_signal_connect(G_OBJECT(dialog), "destroy", G_CALLBACK (destroy_cb), widgets);

	trk->set_property_dialog(dialog);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	gtk_widget_show_all(dialog);

	// Gtk note: due to historical reasons, this must be done after widgets are shown
	if (start_on_stats) {
		gtk_notebook_set_current_page(GTK_NOTEBOOK(graphs), 1);
	}
}


/**
 * Update this property dialog
 * e.g. if the track has been renamed
 */
void vik_trw_layer_propwin_update(Track * trk)
{
	// If not displayed do nothing
	if (!trk->property_dialog) {
		return;
	}

	// Update title with current name
	if (trk->name) {
		char * title = g_strdup_printf(_("%s - Track Properties"), trk->name);
		gtk_window_set_title(GTK_WINDOW(trk->property_dialog), title);
		free(title);
	}

}
