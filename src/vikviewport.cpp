/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
 *
 * Lat/Lon plotting functions calcxy* are from GPSDrive
 * GPSDrive Copyright (C) 2001-2004 Fritz Ganter <ganter@ganter.at>
 *
 * Multiple UTM zone patch by Kit Transue <notlostyet@didactek.com>
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

#define DEFAULT_BACKGROUND_COLOR "#CCCCCC"
#define DEFAULT_HIGHLIGHT_COLOR "#EEA500"
/* Default highlight in orange */

#include <gtk/gtk.h>
#ifdef HAVE_MATH_H
#include <math.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <stdlib.h>
#include <assert.h>

#include "coords.h"
#include "vikcoord.h"
#include "vikwindow.h"
#include "vikviewport.h"
#include "mapcoord.h"

/* for ALTI_TO_MPP */
#include "globals.h"
#include "settings.h"
#include "dialog.h"

#define MERCATOR_FACTOR(x) ((65536.0 / 180 / (x)) * 256.0)

using namespace SlavGPS;

static double EASTING_OFFSET = 500000.0;

static int PAD = 10;

static void viewport_finalize (GObject *gob);


static bool calcxy(double *x, double *y, double lg, double lt, double zero_long, double zero_lat, double pixelfact_x, double pixelfact_y, int mapSizeX2, int mapSizeY2);
static bool calcxy_rev(double *lg, double *lt, int x, int y, double zero_long, double zero_lat, double pixelfact_x, double pixelfact_y, int mapSizeX2, int mapSizeY2);
double calcR (double lat);

static double Radius[181];
static void viewport_init_ra();

static GObjectClass *parent_class;



double Viewport::calculate_utm_zone_width()
{
	if (coord_mode == VIK_COORD_UTM) {
		struct LatLon ll;

		/* get latitude of screen bottom */
		struct UTM utm = *((struct UTM *)(get_center()));
		utm.northing -= height * ympp / 2;
		a_coords_utm_to_latlon(&utm, &ll);

		/* boundary */
		ll.lon = (utm.zone - 1) * 6 - 180 ;
		a_coords_latlon_to_utm(&ll, &utm);
		return fabs(utm.easting - EASTING_OFFSET) * 2;
	} else {
		return 0.0;
	}
}

enum {
	VW_UPDATED_CENTER_SIGNAL = 0,
	VW_LAST_SIGNAL,
};
static unsigned int viewport_signals[VW_LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (VikViewport, vik_viewport, GTK_TYPE_DRAWING_AREA)

static void vik_viewport_class_init (VikViewportClass *klass)
{
	/* Destructor */
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = viewport_finalize;

	parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

	viewport_signals[VW_UPDATED_CENTER_SIGNAL] = g_signal_new ("updated_center", G_TYPE_FROM_CLASS (klass),
								    (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION), G_STRUCT_OFFSET (VikViewportClass, updated_center), NULL, NULL,
								    g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

VikViewport *vik_viewport_new ()
{
	VikViewport *vv = VIK_VIEWPORT (g_object_new (VIK_VIEWPORT_TYPE, NULL));

	vv->port.scr_buffer = NULL;
	vv->port.width = 0;
	vv->port.height = 0;
	vv->port.width_2 = 0;
	vv->port.height_2 = 0;
	vv->port.vvp = (void *) vv;

	return vv;
}

#define VIK_SETTINGS_VIEW_LAST_LATITUDE "viewport_last_latitude"
#define VIK_SETTINGS_VIEW_LAST_LONGITUDE "viewport_last_longitude"
#define VIK_SETTINGS_VIEW_LAST_ZOOM_X "viewport_last_zoom_xpp"
#define VIK_SETTINGS_VIEW_LAST_ZOOM_Y "viewport_last_zoom_ypp"
#define VIK_SETTINGS_VIEW_HISTORY_SIZE "viewport_history_size"
#define VIK_SETTINGS_VIEW_HISTORY_DIFF_DIST "viewport_history_diff_dist"

static void vik_viewport_init(VikViewport *vvp)
{
	viewport_init_ra();

	vvp->trigger = NULL;
	vvp->snapshot_buffer = NULL;
	vvp->half_drawn = false;

	g_signal_connect (G_OBJECT(vvp), "configure_event", G_CALLBACK(vik_viewport_configure), NULL);

#if GTK_CHECK_VERSION (2,18,0)
	gtk_widget_set_can_focus (GTK_WIDGET(vvp), true);
#else
	GTK_WIDGET_SET_FLAGS(vvp, GTK_CAN_FOCUS); /* allow VVP to have focus -- enabling key events, etc */
#endif
}

GdkColor * Viewport::get_background_gdkcolor()
{
	GdkColor *rv = (GdkColor *) malloc(sizeof (GdkColor));
	*rv = background_color;  /* kamilTODO: what? */
	return rv;
}

Viewport::Viewport()
{
	struct UTM utm;
	struct LatLon ll;
	ll.lat = a_vik_get_default_lat();
	ll.lon = a_vik_get_default_long();
	double zoom_x = 4.0;
	double zoom_y = 4.0;

	if (a_vik_get_startup_method() == VIK_STARTUP_METHOD_LAST_LOCATION) {
		double lat, lon, dzoom;
		if (a_settings_get_double(VIK_SETTINGS_VIEW_LAST_LATITUDE, &lat)) {
			ll.lat = lat;
		}

		if (a_settings_get_double(VIK_SETTINGS_VIEW_LAST_LONGITUDE, &lon)) {
			ll.lon = lon;
		}

		if (a_settings_get_double(VIK_SETTINGS_VIEW_LAST_ZOOM_X, &dzoom)) {
			zoom_x = dzoom;
		}

		if (a_settings_get_double(VIK_SETTINGS_VIEW_LAST_ZOOM_Y, &dzoom)) {
			zoom_y = dzoom;
		}
	}

	a_coords_latlon_to_utm(&ll, &utm);

	xmpp = zoom_x;
	ympp = zoom_y;
	xmfactor = MERCATOR_FACTOR(xmpp);
	ymfactor = MERCATOR_FACTOR(ympp);
	coord_mode = VIK_COORD_LATLON;
	drawmode = VIK_VIEWPORT_DRAWMODE_MERCATOR;
	center.mode = VIK_COORD_LATLON;
	center.north_south = ll.lat;
	center.east_west = ll.lon;
	center.utm_zone = (int) utm.zone;
	center.utm_letter = utm.letter;
	utm_zone_width = 0.0;
	background_gc = NULL;
	highlight_gc = NULL;
	scale_bg_gc = NULL;

	scr_buffer = NULL;
	width = 0;
	height = 0;
	width_2 = 0;
	height_2 = 0;

	copyrights = NULL;
	logos = NULL;
	scr_buffer = NULL;
	centers = NULL;
	centers_index = 0;
	centers_max = 20;
	int tmp = centers_max;
	if (a_settings_get_integer(VIK_SETTINGS_VIEW_HISTORY_SIZE, &tmp)) {
		centers_max = tmp;
	}

	centers_radius = 500;
	if (a_settings_get_integer(VIK_SETTINGS_VIEW_HISTORY_DIFF_DIST, &tmp)) {
		centers_radius = tmp;
	}

	do_draw_scale = true;
	do_draw_centermark = true;
	do_draw_highlight = true;


	// Initiate center history
	update_centers();
}

/* returns pointer to internal static storage, changes next time function called, use quickly */
const char * Viewport::get_background_color()
{
	static char color[8];
	snprintf(color, sizeof(color), "#%.2x%.2x%.2x",
		 (int) (background_color.red / 256),
		 (int) (background_color.green / 256),
		 (int) (background_color.blue / 256));
	return color;
}

void Viewport::set_background_color(const char * colorname)
{
	assert (background_gc);
	if (gdk_color_parse (colorname, &(background_color))) {
		gdk_gc_set_rgb_fg_color(background_gc, &(background_color));
	} else {
		fprintf(stderr, "WARNING: %s: Failed to parse color '%s'\n", __FUNCTION__, colorname);
	}
}

void Viewport::set_background_gdkcolor(GdkColor *color)
{
	assert (background_gc);
	background_color = *color;
	gdk_gc_set_rgb_fg_color(background_gc, color);
}

GdkColor * Viewport::get_highlight_gdkcolor()
{
	GdkColor *rv = (GdkColor *) malloc(sizeof (GdkColor));
	*rv = highlight_color; /* kamilTODO: what? */
	return rv;
}

/* returns pointer to internal static storage, changes next time function called, use quickly */
const char * Viewport::get_highlight_color()
{
	static char color[8];
	snprintf(color, sizeof(color), "#%.2x%.2x%.2x",
		 (int) (highlight_color.red / 256),
		 (int) (highlight_color.green / 256),
		 (int) (highlight_color.blue / 256));
	return color;
}

void Viewport::set_highlight_color(const char *colorname)
{
	assert (highlight_gc);
	gdk_color_parse(colorname, &(highlight_color));
	gdk_gc_set_rgb_fg_color(highlight_gc, &(highlight_color));
}

void Viewport::set_highlight_gdkcolor(GdkColor *color)
{
	assert (highlight_gc);
	highlight_color = *color;
	gdk_gc_set_rgb_fg_color(highlight_gc, color);
}

GdkGC * Viewport::get_gc_highlight()
{
	return highlight_gc;
}

void Viewport::set_highlight_thickness(int thickness)
{
	// Otherwise same GDK_* attributes as in vik_viewport_new_gc
	gdk_gc_set_line_attributes(highlight_gc, thickness, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND);
}

GdkGC *vik_viewport_new_gc (VikViewport *vvp, const char *colorname, int thickness)
{
	GdkColor color;

	GdkGC * rv = gdk_gc_new(gtk_widget_get_window(GTK_WIDGET(vvp)));
	if (gdk_color_parse(colorname, &color)) {
		gdk_gc_set_rgb_fg_color (rv, &color);
	} else {
		fprintf(stderr, "WARNING: %s: Failed to parse color '%s'\n", __FUNCTION__, colorname);
	}
	gdk_gc_set_line_attributes(rv, thickness, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND);
	return rv;
}

GdkGC *vik_viewport_new_gc_from_color (VikViewport *vvp, GdkColor *color, int thickness)
{
	GdkGC * rv = gdk_gc_new(gtk_widget_get_window(GTK_WIDGET(vvp)));
	gdk_gc_set_rgb_fg_color(rv, color);
	gdk_gc_set_line_attributes(rv, thickness, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND);
	return rv;
}

void vik_viewport_configure_manually (VikViewport *vvp, int width, unsigned int height)
{
	vvp->port.width = width;
	vvp->port.height = height;

	vvp->port.width_2 = vvp->port.width/2;
	vvp->port.height_2 = vvp->port.height/2;

	if (vvp->port.scr_buffer) {
		g_object_unref (G_OBJECT (vvp->port.scr_buffer));
	}
	vvp->port.scr_buffer = gdk_pixmap_new (gtk_widget_get_window(GTK_WIDGET(vvp)), vvp->port.width, vvp->port.height, -1);

	/* TODO trigger: only if this is enabled !!! */
	if (vvp->snapshot_buffer) {
		g_object_unref (G_OBJECT (vvp->snapshot_buffer));
	}
	vvp->snapshot_buffer = gdk_pixmap_new (gtk_widget_get_window(GTK_WIDGET(vvp)), vvp->port.width, vvp->port.height, -1);
}


GdkPixmap *vik_viewport_get_pixmap (VikViewport *vvp)
{
	return vvp->port.scr_buffer;
}

bool vik_viewport_configure (VikViewport *vvp)
{
	g_return_val_if_fail (vvp != NULL, true);

	Viewport * viewport = &vvp->port;

	GtkAllocation allocation;
	gtk_widget_get_allocation (GTK_WIDGET(vvp), &allocation);
	viewport->width = allocation.width;
	viewport->height = allocation.height;

	viewport->width_2 = viewport->width / 2;
	viewport->height_2 = viewport->height/2;

	if (viewport->scr_buffer) {
		g_object_unref (G_OBJECT (viewport->scr_buffer));
	}

	viewport->scr_buffer = gdk_pixmap_new (gtk_widget_get_window(GTK_WIDGET(vvp)), viewport->width, viewport->height, -1);

	/* TODO trigger: only if enabled! */
	if (vvp->snapshot_buffer) {
		g_object_unref (G_OBJECT (vvp->snapshot_buffer));
	}

	vvp->snapshot_buffer = gdk_pixmap_new (gtk_widget_get_window(GTK_WIDGET(vvp)), viewport->width, viewport->height, -1);
	/* TODO trigger */

	/* this is down here so it can get a GC (necessary?) */
	if (!viewport->background_gc) {
		viewport->background_gc = vik_viewport_new_gc (vvp, DEFAULT_BACKGROUND_COLOR, 1);
		viewport->set_background_color(DEFAULT_BACKGROUND_COLOR);
	}
	if (!viewport->highlight_gc) {
		viewport->highlight_gc = vik_viewport_new_gc (vvp, DEFAULT_HIGHLIGHT_COLOR, 1);
		viewport->set_highlight_color(DEFAULT_HIGHLIGHT_COLOR);
	}
	if (!viewport->scale_bg_gc) {
		viewport->scale_bg_gc = vik_viewport_new_gc(vvp, "grey", 3);
	}

	return false;
}

static void viewport_finalize (GObject *gob)
{
	VikViewport * vvp = VIK_VIEWPORT(gob);
	Viewport * viewport = &vvp->port;

	g_return_if_fail (vvp != NULL);

	if (a_vik_get_startup_method () == VIK_STARTUP_METHOD_LAST_LOCATION) {
		struct LatLon ll;
		vik_coord_to_latlon (&(viewport->center), &ll);
		a_settings_set_double (VIK_SETTINGS_VIEW_LAST_LATITUDE, ll.lat);
		a_settings_set_double (VIK_SETTINGS_VIEW_LAST_LONGITUDE, ll.lon);
		a_settings_set_double (VIK_SETTINGS_VIEW_LAST_ZOOM_X, viewport->xmpp);
		a_settings_set_double (VIK_SETTINGS_VIEW_LAST_ZOOM_Y, viewport->ympp);
	}

	if (viewport->centers) {
		viewport->free_centers(0);
	}

	if (viewport->scr_buffer) {
		g_object_unref (G_OBJECT (viewport->scr_buffer));
	}

	if (vvp->snapshot_buffer) {
		g_object_unref (G_OBJECT (vvp->snapshot_buffer));
	}

	if (viewport->background_gc) {
		g_object_unref (G_OBJECT (viewport->background_gc));
	}

	if (viewport->highlight_gc) {
		g_object_unref (G_OBJECT (viewport->highlight_gc));
	}

	if (viewport->scale_bg_gc) {
		g_object_unref (G_OBJECT (viewport->scale_bg_gc));
		viewport->scale_bg_gc = NULL;
	}

	G_OBJECT_CLASS(parent_class)->finalize(gob);
}

/**
 * vik_viewport_clear:
 * @vvp: self object
 *
 * Clear the whole viewport.
 */
void Viewport::clear()
{
	//g_return_if_fail (vvp != NULL);
	gdk_draw_rectangle(GDK_DRAWABLE(scr_buffer), background_gc, true, 0, 0, width, height);
	this->reset_copyrights();
	this->reset_logos();
}

/**
 * vik_viewport_set_draw_scale:
 * @vvp: self
 * @draw_scale: new value
 *
 * Enable/Disable display of scale.
 */
void Viewport::set_draw_scale(bool draw_scale_)
{
	do_draw_scale = draw_scale_;
}

bool Viewport::get_draw_scale()
{
	return do_draw_scale;
}

void Viewport::draw_scale()
{
	VikViewport * vvp = (VikViewport *) this->vvp;
	Viewport * viewport = &vvp->port;
	//g_return_if_fail (vvp != NULL);

	if (1) { // do_draw_scale) {
		VikCoord left, right;
		double base;
		int SCSIZE = 5, HEIGHT=10;


		this->screen_to_coord(0, height / 2, &left);
		this->screen_to_coord(width / SCSIZE, height / 2, &right);

		vik_units_distance_t dist_units = a_vik_get_units_distance ();
		switch (dist_units) {
		case VIK_UNITS_DISTANCE_KILOMETRES:
			base = vik_coord_diff (&left, &right); // in meters
			break;
		case VIK_UNITS_DISTANCE_MILES:
			// in 0.1 miles (copes better when zoomed in as 1 mile can be too big)
			base = VIK_METERS_TO_MILES(vik_coord_diff (&left, &right)) * 10.0;
			break;
		case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
			// in 0.1 NM (copes better when zoomed in as 1 NM can be too big)
			base = VIK_METERS_TO_NAUTICAL_MILES(vik_coord_diff (&left, &right)) * 10.0;
			break;
		default:
			base = 1; // Keep the compiler happy
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. distance=%d\n", dist_units);
		}
		double ratio = (width / SCSIZE) / base;

		double unit = 1;
		double diff = fabs(base-unit);
		double old_unit = unit;
		double old_diff = diff;
		int odd = 1;
		while (diff <= old_diff) {
			old_unit = unit;
			old_diff = diff;
			unit = unit * (odd%2 ? 5 : 2);
			diff = fabs(base-unit);
			odd++;
		}
		unit = old_unit;
		int len = unit * ratio;

		/* white background */
		this->draw_line(scale_bg_gc,
				       PAD, height-PAD, PAD + len, height-PAD);
		this->draw_line(scale_bg_gc,
				       PAD, height-PAD, PAD, height-PAD-HEIGHT);
		this->draw_line(scale_bg_gc,
				       PAD + len, height-PAD, PAD + len, height-PAD-HEIGHT);
		/* black scale */
		this->draw_line(gtk_widget_get_style(GTK_WIDGET(&vvp->drawing_area))->black_gc,
				       PAD, height-PAD, PAD + len, height-PAD);
		this->draw_line(gtk_widget_get_style(GTK_WIDGET(&vvp->drawing_area))->black_gc,
				       PAD, height-PAD, PAD, height-PAD-HEIGHT);
		this->draw_line(gtk_widget_get_style(GTK_WIDGET(&vvp->drawing_area))->black_gc,
				       PAD + len, height-PAD, PAD + len, height-PAD-HEIGHT);
		if (odd%2) {
			int i;
			for (i=1; i<5; i++) {
				this->draw_line(scale_bg_gc,
						       PAD+i*len/5, height-PAD, PAD+i*len/5, height-PAD-(HEIGHT/2));
				this->draw_line(gtk_widget_get_style(GTK_WIDGET(&vvp->drawing_area))->black_gc,
						       PAD+i*len/5, height-PAD, PAD+i*len/5, height-PAD-(HEIGHT/2));
			}
		} else {
			int i;
			for (i=1; i<10; i++) {
				this->draw_line(scale_bg_gc,
						       PAD+i*len/10, height-PAD, PAD+i*len/10, height-PAD-((i==5)?(2*HEIGHT/3):(HEIGHT/2)));
				this->draw_line(gtk_widget_get_style(GTK_WIDGET(&vvp->drawing_area))->black_gc,
						       PAD+i*len/10, height-PAD, PAD+i*len/10, height-PAD-((i==5)?(2*HEIGHT/3):(HEIGHT/2)));
			}
		}
		PangoLayout * pl = gtk_widget_create_pango_layout (GTK_WIDGET(&vvp->drawing_area), NULL);
		pango_layout_set_font_description (pl, gtk_widget_get_style(GTK_WIDGET(&vvp->drawing_area))->font_desc);

		char s[128];
		switch (dist_units) {
		case VIK_UNITS_DISTANCE_KILOMETRES:
			if (unit >= 1000) {
				sprintf(s, "%d km", (int)unit/1000);
			} else {
				sprintf(s, "%d m", (int)unit);
			}
			break;
		case VIK_UNITS_DISTANCE_MILES:
			// Handle units in 0.1 miles
			if (unit < 10.0) {
				sprintf(s, "%0.1f miles", unit/10.0);
			} else if ((int)unit == 10.0) {
				sprintf(s, "1 mile");
			} else {
				sprintf(s, "%d miles", (int)(unit/10.0));
			}
			break;
		case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
			// Handle units in 0.1 NM
			if (unit < 10.0) {
				sprintf(s, "%0.1f NM", unit/10.0);
			} else if ((int)unit == 10.0) {
				sprintf(s, "1 NM");
			} else {
				sprintf(s, "%d NMs", (int)(unit/10.0));
			}
			break;
		default:
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. distance=%d\n", dist_units);
		}
		pango_layout_set_text(pl, s, -1);
		this->draw_layout(gtk_widget_get_style(GTK_WIDGET(&vvp->drawing_area))->black_gc,
					 PAD + len + PAD, height - PAD - 10, pl);
		g_object_unref(pl);
		pl = NULL;
	}
}

void Viewport::draw_copyright()
{
	//g_return_if_fail (vvp != NULL);

	char s[128] = "";

	/* compute copyrights string */
	unsigned int len = g_slist_length(copyrights);

	for (int i = 0 ; i < len ; i++) {
		// Stop when buffer is full
		int slen = strlen(s);
		if (slen >= 127) {
			break;
		}

		char *copyright = (char *) g_slist_nth_data(copyrights, i);

		// Only use part of this copyright that fits in the available space left
		//  remembering 1 character is left available for the appended space
		int clen = strlen (copyright);
		if (slen + clen > 126) {
			clen = 126 - slen;
		}

		strncat(s, copyright, clen);
		strcat(s, " ");
	}

	VikViewport * vvp = (VikViewport *) (this->vvp);

	/* create pango layout */
	PangoLayout * pl = gtk_widget_create_pango_layout (GTK_WIDGET(&vvp->drawing_area), NULL);
	pango_layout_set_font_description (pl, gtk_widget_get_style(GTK_WIDGET(&vvp->drawing_area))->font_desc);
	pango_layout_set_alignment (pl, PANGO_ALIGN_RIGHT);

	/* Set the text */
	pango_layout_set_text(pl, s, -1);

	PangoRectangle ink_rect, logical_rect;
	/* Use maximum of half the viewport width */
	pango_layout_set_width (pl, (width / 2) * PANGO_SCALE);
	pango_layout_get_pixel_extents(pl, &ink_rect, &logical_rect);
	this->draw_layout(gtk_widget_get_style(GTK_WIDGET(&vvp->drawing_area))->black_gc,
			  width / 2, height - logical_rect.height, pl);

	/* Free memory */
	g_object_unref(pl);
	pl = NULL;
}

/**
 * vik_viewport_set_draw_centermark:
 * @vvp: self object
 * @draw_centermark: new value
 *
 * Enable/Disable display of center mark.
 */
void Viewport::set_draw_centermark(bool draw_centermark_)
{
	do_draw_centermark = draw_centermark_;
}

bool Viewport::get_draw_centermark()
{
	return do_draw_centermark;
}

void Viewport::draw_centermark()
{
	//g_return_if_fail (vvp != NULL);

	if (!do_draw_centermark) {
		return;
	}

	const int len = 30;
	const int gap = 4;
	int center_x = width / 2;
	int center_y = height / 2;
	VikViewport * vvp = (VikViewport *) this->vvp;
	GdkGC * black_gc = gtk_widget_get_style(GTK_WIDGET(&vvp->drawing_area))->black_gc;

	/* white back ground */
	this->draw_line(scale_bg_gc, center_x - len, center_y, center_x - gap, center_y);
	this->draw_line(scale_bg_gc, center_x + gap, center_y, center_x + len, center_y);
	this->draw_line(scale_bg_gc, center_x, center_y - len, center_x, center_y - gap);
	this->draw_line(scale_bg_gc, center_x, center_y + gap, center_x, center_y + len);
	/* black fore ground */
	this->draw_line(black_gc, center_x - len, center_y, center_x - gap, center_y);
	this->draw_line(black_gc, center_x + gap, center_y, center_x + len, center_y);
	this->draw_line(black_gc, center_x, center_y - len, center_x, center_y - gap);
	this->draw_line(black_gc, center_x, center_y + gap, center_x, center_y + len);

}

void Viewport::draw_logo()
{
	//g_return_if_fail (vvp != NULL);

	unsigned int len = g_slist_length(logos);
	int x = width - PAD;
	int y = PAD;
	for (int i = 0 ; i < len ; i++) {
		GdkPixbuf *logo = (GdkPixbuf *) g_slist_nth_data(logos, i);
		int width = gdk_pixbuf_get_width (logo);
		int height = gdk_pixbuf_get_height (logo);
		this->draw_pixbuf(logo, 0, 0, x - width, y, width, height);
		x = x - width - PAD;
	}
}

void Viewport::set_draw_highlight(bool draw_highlight_)
{
	do_draw_highlight = draw_highlight_;
}

bool Viewport::get_draw_highlight()
{
	return do_draw_highlight;
}

void vik_viewport_sync(Viewport * viewport)
{
	VikViewport * vvp = (VikViewport *) viewport->vvp;
	g_return_if_fail (vvp != NULL);
	gdk_draw_drawable(gtk_widget_get_window(GTK_WIDGET(vvp)), gtk_widget_get_style(GTK_WIDGET(vvp))->bg_gc[0], GDK_DRAWABLE(viewport->scr_buffer), 0, 0, 0, 0, viewport->width, viewport->height);
}

void vik_viewport_pan_sync (VikViewport *vvp, int x_off, int y_off)
{
	int x, y, wid, hei;

	g_return_if_fail (vvp != NULL);
	gdk_draw_drawable(gtk_widget_get_window(GTK_WIDGET(vvp)), gtk_widget_get_style(GTK_WIDGET(vvp))->bg_gc[0], GDK_DRAWABLE(vvp->port.scr_buffer), 0, 0, x_off, y_off, vvp->port.width, vvp->port.height);

	if (x_off >= 0) {
		x = 0;
		wid = x_off;
	} else {
		x = vvp->port.width+x_off;
		wid = -x_off;
	}

	if (y_off >= 0) {
		y = 0;
		hei = y_off;
	} else {
		y = vvp->port.height+y_off;
		hei = -y_off;
	}
	gtk_widget_queue_draw_area(GTK_WIDGET(vvp), x, 0, wid, vvp->port.height);
	gtk_widget_queue_draw_area(GTK_WIDGET(vvp), 0, y, vvp->port.width, hei);
}

void Viewport::set_zoom(double xympp_)
{
	//g_return_if_fail (vvp != NULL);
	if (xympp_ >= VIK_VIEWPORT_MIN_ZOOM && xympp_ <= VIK_VIEWPORT_MAX_ZOOM) {
		xmpp = ympp = xympp_;
		// Since xmpp & ympp are the same it doesn't matter which one is used here
		xmfactor = ymfactor = MERCATOR_FACTOR(xmpp);
	}

	if (drawmode == VIK_VIEWPORT_DRAWMODE_UTM) {
		this->utm_zone_check();
	}
}

/* or could do factor */
void Viewport::zoom_in()
{
	//g_return_if_fail (vvp != NULL);
	if (xmpp >= (VIK_VIEWPORT_MIN_ZOOM*2) && ympp >= (VIK_VIEWPORT_MIN_ZOOM*2)) {
		xmpp /= 2;
		ympp /= 2;

		xmfactor = MERCATOR_FACTOR(xmpp);
		ymfactor = MERCATOR_FACTOR(ympp);

		this->utm_zone_check();
	}
}

void Viewport::zoom_out()
{
	//g_return_if_fail (vvp != NULL);
	if (xmpp <= (VIK_VIEWPORT_MAX_ZOOM/2) && ympp <= (VIK_VIEWPORT_MAX_ZOOM/2)) {
		xmpp *= 2;
		ympp *= 2;

		xmfactor = MERCATOR_FACTOR(xmpp);
		ymfactor = MERCATOR_FACTOR(ympp);

		this->utm_zone_check();
	}
}

double Viewport::get_zoom()
{
	if (xmpp == ympp) {
		return xmpp;
	}
	return 0.0;
}

double Viewport::get_xmpp()
{
	return xmpp;
}

double Viewport::get_ympp()
{
	return ympp;
}

void Viewport::set_xmpp(double xmpp_)
{
	if (xmpp_ >= VIK_VIEWPORT_MIN_ZOOM && xmpp_ <= VIK_VIEWPORT_MAX_ZOOM) {
		xmpp = xmpp_;
		ymfactor = MERCATOR_FACTOR(ympp);
		if (drawmode == VIK_VIEWPORT_DRAWMODE_UTM) {
			this->utm_zone_check();
		}
	}
}

void Viewport::set_ympp(double ympp_)
{
	if (ympp_ >= VIK_VIEWPORT_MIN_ZOOM && ympp_ <= VIK_VIEWPORT_MAX_ZOOM) {
		ympp = ympp_;
		ymfactor = MERCATOR_FACTOR(ympp);
		if (drawmode == VIK_VIEWPORT_DRAWMODE_UTM) {
			this->utm_zone_check();
		}
	}
}


VikCoord * Viewport::get_center()
{
	// g_return_val_if_fail (vvp != NULL, NULL);
	return &center;
}

/* called every time we update coordinates/zoom */
void Viewport::utm_zone_check()
{
	if (coord_mode == VIK_COORD_UTM) {
		struct UTM utm;
		struct LatLon ll;
		a_coords_utm_to_latlon((struct UTM *) &(center), &ll);
		a_coords_latlon_to_utm(&ll, &utm);
		if (utm.zone != center.utm_zone) {
			*((struct UTM *)(&center)) = utm;
		}

		/* misc. stuff so we don't have to check later */
		utm_zone_width = this->calculate_utm_zone_width();
		one_utm_zone = (this->rightmost_zone() == this->leftmost_zone());
	}
}

/**
 * Free an individual center position in the history list
 */
void Viewport::free_center(unsigned int index)
{
	VikCoord *coord = (VikCoord *) g_list_nth_data(centers, index);
	if (coord) {
		free(coord);
	}

	GList *gl = g_list_nth(centers, index);
	if (gl) {
		centers = g_list_delete_link(centers, gl);
	}
}

/**
 * Free a set of center positions in the history list,
 *  from the indicated start index to the end of the list
 */
void Viewport::free_centers(unsigned int start)
{
	// Have to work backward since we delete items referenced by the '_nth()' values,
	//  otherwise if processed forward - removing the lower nth index entries would change the subsequent indexing
	for (unsigned int i = g_list_length(centers) - 1; i > start; i--) {
		this->free_center(i);
	}
}

/**
 * Store the current center position into the history list
 *  and emit a signal to notify clients the list has been updated
 */
void Viewport::update_centers()
{
	VikCoord *new_center = (VikCoord *) malloc(sizeof (VikCoord));
	*new_center = center; /* kamilFIXME: what does this assignment do? */

	if (centers_index) {

		if (centers_index == centers_max - 1) {
			// List is full, so drop the oldest value to make room for the new one
			this->free_center(0);
			centers_index--;
		} else {
			// Reset the now unused section of the list
			// Free from the index to the end
			this->free_centers(centers_index + 1);
		}

	}

	// Store new position
	// NB ATM this can be the same location as the last one in the list
	centers = g_list_append(centers, new_center);

	// Reset to the end (NB should be same as centers_index++)
	centers_index = g_list_length(centers) - 1;

	// Inform interested subscribers that this change has occurred
	g_signal_emit (G_OBJECT((VikViewport *) this->vvp), viewport_signals[VW_UPDATED_CENTER_SIGNAL], 0);
}

/**
 * Show the list of forward/backward positions
 * ATM only for debug usage
 */
void Viewport::show_centers(GtkWindow *parent)
{
	GList * node = NULL;
	GList * texts = NULL;
	int index = 0;
	for (node = centers; node != NULL; node = g_list_next(node)) {
		char *lat = NULL, *lon = NULL;
		struct LatLon ll;
		vik_coord_to_latlon ((const VikCoord *) node->data, &ll);
		a_coords_latlon_to_string (&ll, &lat, &lon);
		char *extra = NULL;
		if (index == centers_index - 1) {
			extra = strdup(" [Back]");
		} else if (index == centers_index + 1) {
			extra = strdup(" [Forward]");
		} else {
			extra = strdup("");
		}
		texts = g_list_prepend(texts, g_strdup_printf("%s %s%s", lat, lon, extra));
		free(lat);
		free(lon);
		free(extra);
		index++;
	}

	// NB: No i18n as this is just for debug
	// Using this function the dialog allows sorting of the list which isn't appropriate here
	//  but this doesn't matter much for debug purposes of showing stuff...
	GList *ans = a_dialog_select_from_list(parent,
					       texts,
					       false,
					       "Back/Forward Locations",
					       "Back/Forward Locations");
	for (node = ans; node != NULL; node = g_list_next(node)) {
		free(node->data);
	}

	g_list_free(ans);
	for (node = texts; node != NULL; node = g_list_next(node)) {
		free(node->data);
	}
	g_list_free(texts);
}

/**
 * vik_viewport_go_back:
 *
 * Move back in the position history
 *
 * Returns: %true one success
 */
bool Viewport::go_back()
{
	// see if the current position is different from the last saved center position within a certain radius
	VikCoord * last_center = (VikCoord *) g_list_nth_data(centers, centers_index);
	if (last_center) {
		// Consider an exclusion size (should it zoom level dependent, rather than a fixed value?)
		// When still near to the last saved position we'll jump over it to the one before
		if (vik_coord_diff(last_center, &center) > centers_radius) {

			if (centers_index == g_list_length(centers) - 1) {
				// Only when we haven't already moved back in the list
				// Remember where this request came from
				//   (alternatively we could insert in the list on every back attempt)
				update_centers();
			}

		}
		// 'Go back' if possible
		// NB if we inserted a position above, then this will then move to the last saved position
		//  otherwise this will skip to the previous saved position, as it's probably somewhere else.
		if (centers_index > 0) {
			centers_index--;
		}
	} else {
		return false;
	}

	VikCoord * new_center = (VikCoord *) g_list_nth_data(centers, centers_index);
	if (new_center) {
		set_center_coord(new_center, false);
		return true;
	}
	return false;
}

/**
 * vik_viewport_go_forward:
 *
 * Move forward in the position history
 *
 * Returns: %true one success
 */
bool Viewport::go_forward()
{
	if (centers_index == centers_max - 1) {
		return false;
	}

	centers_index++;
	VikCoord * new_center = (VikCoord *) g_list_nth_data(centers, centers_index);
	if (new_center) {
		set_center_coord(new_center, false);
		return true;
	} else {
		// Set to end of list
		centers_index = g_list_length(centers) - 1;
	}

	return false;
}

/**
 * vik_viewport_back_available:
 *
 * Returns: %true when a previous position in the history is available
 */
bool Viewport::back_available()
{
	return (centers_index > 0);
}

/**
 * vik_viewport_forward_available:
 *
 * Returns: %true when a next position in the history is available
 */
bool Viewport::forward_available()
{
	return (centers_index < g_list_length(centers) - 1);
}

/**
 * vik_viewport_set_center_latlon:
 * @vvp:           The viewport to reposition.
 * @ll:            The new center position in Lat/Lon format
 * @save_position: Whether this new position should be saved into the history of positions
 *                 Normally only specific user requests should be saved (i.e. to not include Pan and Zoom repositions)
 */
void Viewport::set_center_latlon(const struct LatLon *ll, bool save_position)
{
	vik_coord_load_from_latlon(&(center), coord_mode, ll);
	if (save_position) {
		this->update_centers();
	}

	if (coord_mode == VIK_COORD_UTM) {
		this->utm_zone_check();
	}
}

/**
 * vik_viewport_set_center_utm:
 * @vvp:           The viewport to reposition.
 * @utm:           The new center position in UTM format
 * @save_position: Whether this new position should be saved into the history of positions
 *                 Normally only specific user requests should be saved (i.e. to not include Pan and Zoom repositions)
 */
void Viewport::set_center_utm(const struct UTM *utm, bool save_position)
{
	vik_coord_load_from_utm (&(center), coord_mode, utm);
	if (save_position) {
		this->update_centers();
	}

	if (coord_mode == VIK_COORD_UTM) {
		this->utm_zone_check();
	}
}

/**
 * vik_viewport_set_center_coord:
 * @vvp:           The viewport to reposition.
 * @coord:         The new center position in a VikCoord type
 * @save_position: Whether this new position should be saved into the history of positions
 *                 Normally only specific user requests should be saved (i.e. to not include Pan and Zoom repositions)
 */
void Viewport::set_center_coord(const VikCoord *coord, bool save_position)
{
	center = *coord;
	if (save_position) {
		update_centers();
	}
	if (coord_mode == VIK_COORD_UTM) {
		this->utm_zone_check();
	}
}

void Viewport::corners_for_zonen(int zone, VikCoord *ul, VikCoord *br)
{
	g_return_if_fail(coord_mode == VIK_COORD_UTM);

	/* get center, then just offset */
	this->center_for_zonen(VIK_UTM(ul), zone);
	ul->mode = VIK_COORD_UTM;
	*br = *ul;

	ul->north_south += (ympp * height / 2);
	ul->east_west -= (xmpp * width / 2);
	br->north_south -= (ympp * height / 2);
	br->east_west += (xmpp * width / 2);
}

void Viewport::center_for_zonen(struct UTM *center_, int zone)
{
	if (coord_mode == VIK_COORD_UTM) {
		*center_ = *((struct UTM *)(get_center()));
		center_->easting -= (zone - center_->zone) * utm_zone_width;
		center_->zone = zone;
	}
}

char Viewport::leftmost_zone()
{
	if (coord_mode == VIK_COORD_UTM) {
		VikCoord coord;
		//assert (vvp != NULL);
		this->screen_to_coord(0, 0, &coord);
		return coord.utm_zone;
	}
	return '\0';
}

char Viewport::rightmost_zone()
{
	if (coord_mode == VIK_COORD_UTM) {
		VikCoord coord;
		//assert (vvp != NULL);
		this->screen_to_coord(width, 0, &coord);
		return coord.utm_zone;
	}
	return '\0';
}


void Viewport::set_center_screen(int x, int y)
{
	//g_return_if_fail (vvp != NULL);
	if (coord_mode == VIK_COORD_UTM) {
		/* slightly optimized */
		center.east_west += xmpp * (x - (width/2));
		center.north_south += ympp * ((height/2) - y);
		this->utm_zone_check();
	} else {
		VikCoord tmp;
		this->screen_to_coord(x, y, &tmp);
		set_center_coord(&tmp, false);
	}
}

int Viewport::get_width()
{
	//g_return_val_if_fail (vvp != NULL, 0);
	return width;
}

int Viewport::get_height()
{
	//g_return_val_if_fail (vvp != NULL, 0);
	return height;
}

void Viewport::screen_to_coord(int x, int y, VikCoord *coord)
{
	//g_return_if_fail (vvp != NULL);

	VikViewport * vvp = (VikViewport *) (this->vvp);

	if (coord_mode == VIK_COORD_UTM) {
		int zone_delta;
		struct UTM *utm = (struct UTM *) coord;
		coord->mode = VIK_COORD_UTM;

		utm->zone = center.utm_zone;
		utm->letter = center.utm_letter;
		utm->easting = ((x - (width_2)) * xmpp) + center.east_west;
		zone_delta = floor((utm->easting - EASTING_OFFSET) / utm_zone_width + 0.5);
		utm->zone += zone_delta;
		utm->easting -= zone_delta * utm_zone_width;
		utm->northing = (((height_2) - y) * ympp) + center.north_south;
	} else if (coord_mode == VIK_COORD_LATLON) {
		coord->mode = VIK_COORD_LATLON;
		if (drawmode == VIK_VIEWPORT_DRAWMODE_LATLON) {
			coord->east_west = center.east_west + (180.0 * xmpp / 65536 / 256 * (x - width_2));
			coord->north_south = center.north_south + (180.0 * ympp / 65536 / 256 * (height_2 - y));
		} else if (drawmode == VIK_VIEWPORT_DRAWMODE_EXPEDIA) {
			calcxy_rev(&(coord->east_west), &(coord->north_south), x, y, center.east_west, center.north_south, xmpp * ALTI_TO_MPP, ympp * ALTI_TO_MPP, width_2, height_2);
		} else if (drawmode == VIK_VIEWPORT_DRAWMODE_MERCATOR) {
			/* This isn't called with a high frequently so less need to optimize */
			coord->east_west = center.east_west + (180.0 * xmpp / 65536 / 256 * (x - width_2));
			coord->north_south = DEMERCLAT (MERCLAT(center.north_south) + (180.0 * ympp / 65536 / 256 * (height_2 - y)));
		} else {
			;
		}
	}
}

/*
 * Since this function is used for every drawn trackpoint - it can get called alot
 * Thus x & y position factors are calculated once on zoom changes,
 *  avoiding the need to do it here all the time.
 * For good measure the half width and height values are also pre calculated too.
 */
void Viewport::coord_to_screen(const VikCoord * coord, int *x, int *y)
{
	static VikCoord tmp;
	//g_return_if_fail (vvp != NULL);

	VikViewport * vvp = (VikViewport *) (this->vvp);

	if (coord->mode != vvp->port.coord_mode){
		fprintf(stderr, "WARNING: Have to convert in Viewport::coord_to_screen()! This should never happen!\n");
		vik_coord_copy_convert (coord, vvp->port.coord_mode, &tmp);
		coord = &tmp;
	}

	if (vvp->port.coord_mode == VIK_COORD_UTM) {
		struct UTM *center = (struct UTM *) &(vvp->port.center);
		struct UTM *utm = (struct UTM *) coord;
		if (center->zone != utm->zone && vvp->port.one_utm_zone){
			*x = *y = VIK_VIEWPORT_UTM_WRONG_ZONE;
			return;
		}

		*x = ((utm->easting - center->easting) / vvp->port.xmpp) + (vvp->port.width_2) -
			(center->zone - utm->zone) * vvp->port.utm_zone_width / vvp->port.xmpp;
		*y = (vvp->port.height_2) - ((utm->northing - center->northing) / vvp->port.ympp);
	} else if (vvp->port.coord_mode == VIK_COORD_LATLON) {
		struct LatLon *center = (struct LatLon *) &(vvp->port.center);
		struct LatLon *ll = (struct LatLon *) coord;
		double xx,yy;
		if (vvp->port.drawmode == VIK_VIEWPORT_DRAWMODE_LATLON) {
			*x = vvp->port.width_2 + (MERCATOR_FACTOR(vvp->port.xmpp) * (ll->lon - center->lon));
			*y = vvp->port.height_2 + (MERCATOR_FACTOR(vvp->port.ympp) * (center->lat - ll->lat));
		} else if (vvp->port.drawmode == VIK_VIEWPORT_DRAWMODE_EXPEDIA) {
			calcxy (&xx, &yy, center->lon, center->lat, ll->lon, ll->lat, vvp->port.xmpp * ALTI_TO_MPP, vvp->port.ympp * ALTI_TO_MPP, vvp->port.width_2, vvp->port.height_2);
			*x = xx; *y = yy;
		} else if (vvp->port.drawmode == VIK_VIEWPORT_DRAWMODE_MERCATOR) {
			*x = vvp->port.width_2 + (MERCATOR_FACTOR(vvp->port.xmpp) * (ll->lon - center->lon));
			*y = vvp->port.height_2 + (MERCATOR_FACTOR(vvp->port.ympp) * (MERCLAT(center->lat) - MERCLAT(ll->lat)));
		}
	}
}

// Clip functions continually reduce the value by a factor until it is in the acceptable range
//  whilst also scaling the other coordinate value.
static void clip_x (int *x1, int *y1, int *x2, int *y2)
{
	while (ABS(*x1) > 32768) {
		*x1 = *x2 + (0.5 * (*x1-*x2));
		*y1 = *y2 + (0.5 * (*y1-*y2));
	}
}

static void clip_y (int *x1, int *y1, int *x2, int *y2)
{
	while (ABS(*y1) > 32767) {
		*x1 = *x2 + (0.5 * (*x1-*x2));
		*y1 = *y2 + (0.5 * (*y1-*y2));
	}
}

/**
 * a_viewport_clip_line:
 * @x1: screen coord
 * @y1: screen coord
 * @x2: screen coord
 * @y2: screen coord
 *
 * Due to the seemingly undocumented behaviour of gdk_draw_line(), we need to restrict the range of values passed in.
 * So despite it accepting ints, the effective range seems to be the actually the minimum C int range (2^16).
 * This seems to be limitations coming from the X Window System.
 *
 * See http://www.rahul.net/kenton/40errs.html
 * ERROR 7. Boundary conditions.
 * "The X coordinate space is not infinite.
 *  Most drawing functions limit position, width, and height to 16 bit integers (sometimes signed, sometimes unsigned) of accuracy.
 *  Because most C compilers use 32 bit integers, Xlib will not complain if you exceed the 16 bit limit, but your results will usually not be what you expected.
 *  You should be especially careful of this if you are implementing higher level scalable graphics packages."
 *
 * This function should be called before calling gdk_draw_line().
 */
void Viewport::clip_line(int * x1, int * y1, int * x2, int * y2)
{
	if (*x1 > 32768 || *x1 < -32767) {
		clip_x(x1, y1, x2, y2);
	}

	if (*y1 > 32768 || *y1 < -32767) {
		clip_y(x1, y1, x2, y2);
	}

	if (*x2 > 32768 || *x2 < -32767) {
		clip_x(x2, y2, x1, y1);
	}

	if (*y2 > 32768 || *y2 < -32767) {
		clip_y(x2, y2, x1, y1);
	}
}

void Viewport::draw_line(GdkGC *gc, int x1, int y1, int x2, int y2)
{
	if (! ((x1 < 0 && x2 < 0) || (y1 < 0 && y2 < 0) ||
		 (x1 > this->width && x2 > this->width) || (y1 > this->height && y2 > this->height))) {
		/*** clipping, yeah! ***/
		Viewport::clip_line(&x1, &y1, &x2, &y2);
		gdk_draw_line(this->scr_buffer, gc, x1, y1, x2, y2);
	}
}

void Viewport::draw_rectangle(GdkGC *gc, bool filled, int x1, int y1, int x2, int y2)
{
	// Using 32 as half the default waypoint image size, so this draws ensures the highlight gets done
	if (x1 > -32 && x1 < this->width + 32 && y1 > -32 && y1 < this->height + 32) {
		gdk_draw_rectangle(this->scr_buffer, gc, filled, x1, y1, x2, y2);
	}
}

void Viewport::draw_string(GdkFont *font, GdkGC *gc, int x1, int y1, const char *string)
{
	if (x1 > -100 && x1 < this->width + 100 && y1 > -100 && y1 < this->height + 100) {
		gdk_draw_string(this->scr_buffer, font, gc, x1, y1, string);
	}
}

void Viewport::draw_pixbuf(GdkPixbuf *pixbuf, int src_x, int src_y,
			   int dest_x, int dest_y, int region_width, int region_height)
{
	gdk_draw_pixbuf(this->scr_buffer,
			NULL,
			pixbuf,
			src_x, src_y, dest_x, dest_y, region_width, region_height,
			GDK_RGB_DITHER_NONE, 0, 0);
}

void Viewport::draw_arc(GdkGC *gc, bool filled, int x, int y, int width, int height, int angle1, int angle2)
{
	gdk_draw_arc(this->scr_buffer, gc, filled, x, y, width, height, angle1, angle2);
}


void Viewport::draw_polygon(GdkGC *gc, bool filled, GdkPoint *points, int npoints)
{
	gdk_draw_polygon(this->scr_buffer, gc, filled, points, npoints);
}

VikCoordMode Viewport::get_coord_mode()
{
	return coord_mode;
}

void Viewport::set_coord_mode(VikCoordMode mode_)
{
	// g_return_if_fail (vvp != NULL);
	coord_mode = mode_;
	vik_coord_convert(&(center), mode_);
}

/* Thanks GPSDrive */
static bool calcxy_rev(double *lg, double *lt, int x, int y, double zero_long, double zero_lat, double pixelfact_x, double pixelfact_y, int mapSizeX2, int mapSizeY2)
{
	double Ra = Radius[90+(int)zero_lat];

	int px = (mapSizeX2 - x) * pixelfact_x;
	int py = (-mapSizeY2 + y) * pixelfact_y;

	double lat = zero_lat - py / Ra;
	double lon =
		zero_long -
		px / (Ra *
		      cos (DEG2RAD(lat)));

	double dif = lat * (1 - (cos (DEG2RAD(fabs (lon - zero_long)))));
	lat = lat - dif / 1.5;
	lon =
		zero_long -
		px / (Ra *
		      cos (DEG2RAD(lat)));

	*lt = lat;
	*lg = lon;
	return (true);
}

/* Thanks GPSDrive */
static bool calcxy(double *x, double *y, double lg, double lt, double zero_long, double zero_lat, double pixelfact_x, double pixelfact_y, int mapSizeX2, int mapSizeY2)
{
	int mapSizeX = 2 * mapSizeX2;
	int mapSizeY = 2 * mapSizeY2;

	assert (lt >= -90.0 && lt <= 90.0);
	//    lg *= rad2deg; // FIXME, optimize equations
	//    lt *= rad2deg;
	double Ra = Radius[90+(int)lt];
	*x = Ra *
		cos (DEG2RAD(lt)) * (lg - zero_long);
	*y = Ra * (lt - zero_lat);
	double dif = Ra * RAD2DEG(1 - (cos ((DEG2RAD(lg - zero_long)))));
	*y = *y + dif / 1.85;
	*x = *x / pixelfact_x;
	*y = *y / pixelfact_y;
	*x = mapSizeX2 - *x;
	*y += mapSizeY2;
	if ((*x < 0)||(*x >= mapSizeX)||(*y < 0)||(*y >= mapSizeY)) {
		return (false);
	}
	return (true);
}

static void viewport_init_ra()
{
	static bool done_before = false;
	if (!done_before) {
		for (int i = -90; i <= 90; i++) {
			Radius[i+90] = calcR (DEG2RAD((double)i));
		}
		done_before = true;
	}
}

double calcR(double lat)
{
	/*
	 * the radius of curvature of an ellipsoidal Earth in the plane of the
	 * meridian is given by
	 *
	 * R' = a * (1 - e^2) / (1 - e^2 * (sin(lat))^2)^(3/2)
	 *
	 *
	 * where a is the equatorial radius, b is the polar radius, and e is
	 * the eccentricity of the ellipsoid = sqrt(1 - b^2/a^2)
	 *
	 * a = 6378 km (3963 mi) Equatorial radius (surface to center distance)
	 * b = 6356.752 km (3950 mi) Polar radius (surface to center distance) e
	 * = 0.081082 Eccentricity
	 */
	double a = 6378.137;
	double e2 = 0.081082 * 0.081082;
	lat = DEG2RAD(lat);
	double sc = sin (lat);
	double x = a * (1.0 - e2);
	double z = 1.0 - e2 * sc * sc;
	double y = pow (z, 1.5);
	double r = x / y;
	r = r * 1000.0;
	return r;
}

bool Viewport::is_one_zone()
{
	return coord_mode == VIK_COORD_UTM && one_utm_zone;
}

void Viewport::draw_layout(GdkGC *gc, int x, int y, PangoLayout *layout)
{
	if (x > -100 && x < this->width + 100 && y > -100 && y < this->height + 100) {
		gdk_draw_layout(this->scr_buffer, gc, x, y, layout);
	}
}

void vik_gc_get_fg_color(GdkGC *gc, GdkColor *dest)
{
	static GdkGCValues values;
	gdk_gc_get_values(gc, &values);
	gdk_colormap_query_color(gdk_colormap_get_system(), values.foreground.pixel, dest);
}

GdkFunction vik_gc_get_function (GdkGC *gc)
{
	static GdkGCValues values;
	gdk_gc_get_values (gc, &values);
	return values.function;
}

void Viewport::set_drawmode(VikViewportDrawMode drawmode_)
{
	drawmode = drawmode_;
	if (drawmode_ == VIK_VIEWPORT_DRAWMODE_UTM) {
		this->set_coord_mode(VIK_COORD_UTM);
	} else {
		this->set_coord_mode(VIK_COORD_LATLON);
	}
}

VikViewportDrawMode Viewport::get_drawmode()
{
	return drawmode;
}

/******** triggering *******/
void vik_viewport_set_trigger (VikViewport *vp, void * trigger)
{
	vp->trigger = trigger;
}

void * vik_viewport_get_trigger (VikViewport *vp)
{
	return vp->trigger;
}

void vik_viewport_snapshot_save (VikViewport *vp)
{
	gdk_draw_drawable (vp->snapshot_buffer, vp->port.background_gc, vp->port.scr_buffer, 0, 0, 0, 0, -1, -1);
}

void vik_viewport_snapshot_load (VikViewport *vp)
{
	gdk_draw_drawable (vp->port.scr_buffer, vp->port.background_gc, vp->snapshot_buffer, 0, 0, 0, 0, -1, -1);
}

void vik_viewport_set_half_drawn(VikViewport *vp, bool half_drawn)
{
	vp->half_drawn = half_drawn;
}

bool vik_viewport_get_half_drawn(VikViewport *vp)
{
	return vp->half_drawn;
}


char const * Viewport::get_drawmode_name(VikViewportDrawMode mode)
{
	 VikViewport * vvp = (VikViewport *) this->vvp;

	 VikWindow * vw = VIK_WINDOW_FROM_WIDGET(vvp);
	 GtkWidget * mode_button = vik_window_get_drawmode_button(vw, mode);
	 GtkWidget * label = gtk_bin_get_child(GTK_BIN(mode_button));

	 return gtk_label_get_text(GTK_LABEL(label));

 }

/* kamilTODO: perhaps make the method accept bounding box? */
void Viewport::get_min_max_lat_lon(double *min_lat, double *max_lat, double *min_lon, double *max_lon)
{
	VikCoord tleft, tright, bleft, bright;

	this->screen_to_coord(0, 0, &tleft);
	this->screen_to_coord(this->get_width(), 0, &tright);
	this->screen_to_coord(0, this->get_height(), &bleft);
	this->screen_to_coord(width, height, &bright);

	vik_coord_convert(&tleft, VIK_COORD_LATLON);
	vik_coord_convert(&tright, VIK_COORD_LATLON);
	vik_coord_convert(&bleft, VIK_COORD_LATLON);
	vik_coord_convert(&bright, VIK_COORD_LATLON);

	*max_lat = MAX(tleft.north_south, tright.north_south);
	*min_lat = MIN(bleft.north_south, bright.north_south);
	*max_lon = MAX(tright.east_west, bright.east_west);
	*min_lon = MIN(tleft.east_west, bleft.east_west);
}

void Viewport::reset_copyrights()
{
	//g_return_if_fail (vp != NULL);
	g_slist_foreach(copyrights, (GFunc) g_free, NULL);
	g_slist_free(copyrights);
	copyrights = NULL;
}

/**
 * vik_viewport_add_copyright:
 * @vp: self object
 * @copyright: new copyright to display
 *
 * Add a copyright to display on viewport.
 */
void Viewport::add_copyright(const char *copyright_)
{
	//g_return_if_fail (vp != NULL);
	if (copyright_) {
		GSList * found = g_slist_find_custom(copyrights, copyright_, (GCompareFunc) strcmp);
		if (found == NULL) {
			char *duple = g_strdup(copyright_);
			copyrights = g_slist_prepend(copyrights, duple);
		}
	}
}

void viewport_add_copyright(VikViewport * vvp, const char * copyright_)
{
	vvp->port.add_copyright(copyright_);
}

void Viewport::reset_logos()
{
	//g_return_if_fail (vp != NULL);
	/* do not free elem */
	g_slist_free(logos);
	logos = NULL;
}

void Viewport::add_logo(const GdkPixbuf *logo_)
{
	// g_return_if_fail (vp != NULL);
	if (logo_) {
		GdkPixbuf * found = NULL; /* FIXME (GdkPixbuf*)g_slist_find_custom (vp->port.logos, logo, (GCompareFunc)==); */
		if (found == NULL) {
			logos = g_slist_prepend(logos, (void *) logo_);
		}
	}
}

/**
 * vik_viewport_compute_bearing:
 * @vp: self object
 * @x1: screen coord
 * @y1: screen coord
 * @x2: screen coord
 * @y2: screen coord
 * @angle: bearing in Radian (output)
 * @baseangle: UTM base angle in Radian (output)
 *
 * Compute bearing.
 */
void Viewport::compute_bearing(int x1, int y1, int x2, int y2, double *angle, double *baseangle)
{
	double len = sqrt((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2));
	double dx = (x2-x1)/len*10;
	double dy = (y2-y1)/len*10;

	*angle = atan2(dy, dx) + M_PI_2;

	if (this->get_drawmode() == VIK_VIEWPORT_DRAWMODE_UTM) {
		VikCoord test;
		struct LatLon ll;
		struct UTM u;
		int tx, ty;

		this->screen_to_coord(x1, y1, &test);
		vik_coord_to_latlon(&test, &ll);
		ll.lat += get_ympp() * get_height() / 11000.0; // about 11km per degree latitude
		a_coords_latlon_to_utm(&ll, &u);
		vik_coord_load_from_utm(&test, VIK_COORD_UTM, &u); /* kamilFIXME: it was VIK_VIEWPORT_DRAWMODE_UTM. */
		this->coord_to_screen(&test, &tx, &ty);

		*baseangle = M_PI - atan2(tx-x1, ty-y1);
		*angle -= *baseangle;
	}

	if (*angle < 0) {
		*angle += 2*M_PI;
	}

	if (*angle > 2*M_PI) {
		*angle -= 2*M_PI;
	}
}
