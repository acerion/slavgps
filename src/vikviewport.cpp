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

#ifdef HAVE_MATH_H
#include <math.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <cstdlib>
#include <cassert>

#include <glib-object.h>

#include "vikviewport.h"
#include "coords.h"
#include "window.h"
#include "mapcoord.h"

/* For ALTI_TO_MPP. */
#include "globals.h"
#include "settings.h"
#include "dialog.h"




using namespace SlavGPS;




#define MERCATOR_FACTOR(x) ((65536.0 / 180 / (x)) * 256.0)

#define VIK_SETTINGS_VIEW_LAST_LATITUDE "viewport_last_latitude"
#define VIK_SETTINGS_VIEW_LAST_LONGITUDE "viewport_last_longitude"
#define VIK_SETTINGS_VIEW_LAST_ZOOM_X "viewport_last_zoom_xpp"
#define VIK_SETTINGS_VIEW_LAST_ZOOM_Y "viewport_last_zoom_ypp"
#define VIK_SETTINGS_VIEW_HISTORY_SIZE "viewport_history_size"
#define VIK_SETTINGS_VIEW_HISTORY_DIFF_DIST "viewport_history_diff_dist"




static double EASTING_OFFSET = 500000.0;
static int PAD = 10;

static bool calcxy(double *x, double *y, double lg, double lt, double zero_long, double zero_lat, double pixelfact_x, double pixelfact_y, int mapSizeX2, int mapSizeY2);
static bool calcxy_rev(double *lg, double *lt, int x, int y, double zero_long, double zero_lat, double pixelfact_x, double pixelfact_y, int mapSizeX2, int mapSizeY2);
double calcR(double lat);

static double Radius[181];
static void viewport_init_ra();
bool vik_viewport_configure_cb(GtkDrawingArea * area);




enum {
	VW_UPDATED_CENTER_SIGNAL = 0,
	VW_LAST_SIGNAL,
};
static unsigned int viewport_signals[VW_LAST_SIGNAL] = { 0 };




void SlavGPS::viewport_init(void)
{
	viewport_signals[VW_UPDATED_CENTER_SIGNAL] = g_signal_new("updated_center", G_TYPE_OBJECT,
								  (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
								  0, NULL, NULL,
								  g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
	viewport_init_ra();
}




void Viewport::init_drawing_area()
{
	this->drawing_area_ = (GtkDrawingArea *) gtk_drawing_area_new();

	g_signal_connect(G_OBJECT (this->drawing_area_), "configure_event", G_CALLBACK (vik_viewport_configure_cb), NULL);

#if GTK_CHECK_VERSION (2,18,0)
	gtk_widget_set_can_focus(GTK_WIDGET (this->drawing_area_), true);
#else
	GTK_WIDGET_SET_FLAGS (this->drawing_area_, GTK_CAN_FOCUS); /* Allow drawing area to have focus -- enabling key events, etc. */
#endif
	g_object_set_data((GObject *) this->drawing_area_, "viewport", this);
}




double Viewport::calculate_utm_zone_width()
{
	if (coord_mode == VIK_COORD_UTM) {
		struct LatLon ll;

		/* Get latitude of screen bottom. */
		struct UTM utm = *((struct UTM *)(get_center()));
		utm.northing -= this->size_height * ympp / 2;
		a_coords_utm_to_latlon(&utm, &ll);

		/* Boundary. */
		ll.lon = (utm.zone - 1) * 6 - 180 ;
		a_coords_latlon_to_utm(&ll, &utm);
		return fabs(utm.easting - EASTING_OFFSET) * 2;
	} else {
		return 0.0;
	}
}




GdkColor * Viewport::get_background_gdkcolor()
{
	GdkColor *rv = (GdkColor *) malloc(sizeof (GdkColor));
	*rv = background_color;
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

	centers = new std::list<Coord *>;
	centers_iter = centers->begin();
	centers_max = 20;
	int tmp = centers_max;
	if (a_settings_get_integer(VIK_SETTINGS_VIEW_HISTORY_SIZE, &tmp)) {
		centers_max = tmp;
	}

	centers_radius = 500;
	if (a_settings_get_integer(VIK_SETTINGS_VIEW_HISTORY_DIFF_DIST, &tmp)) {
		centers_radius = tmp;
	}

	this->init_drawing_area();

	/* Initiate center history. */
	update_centers();

	strncpy(this->type_string, "Le Viewport", (sizeof (this->type_string)) - 1);
}




Viewport::~Viewport()
{
	fprintf(stderr, "~Viewport called\n");
	if (a_vik_get_startup_method() == VIK_STARTUP_METHOD_LAST_LOCATION) {
		struct LatLon ll;
		vik_coord_to_latlon(&(this->center), &ll);
		a_settings_set_double(VIK_SETTINGS_VIEW_LAST_LATITUDE, ll.lat);
		a_settings_set_double(VIK_SETTINGS_VIEW_LAST_LONGITUDE, ll.lon);
		a_settings_set_double(VIK_SETTINGS_VIEW_LAST_ZOOM_X, this->xmpp);
		a_settings_set_double(VIK_SETTINGS_VIEW_LAST_ZOOM_Y, this->ympp);
	}

	if (this->centers) {
		delete this->centers;
	}

	if (this->scr_buffer) {
		g_object_unref(G_OBJECT (this->scr_buffer));
	}

	if (this->snapshot_buffer) {
		g_object_unref(G_OBJECT (this->snapshot_buffer));
	}

	if (this->background_gc) {
		g_object_unref(G_OBJECT (this->background_gc));
	}

	if (this->highlight_gc) {
		g_object_unref(G_OBJECT (this->highlight_gc));
	}

	if (this->scale_bg_gc) {
		g_object_unref(G_OBJECT (this->scale_bg_gc));
		this->scale_bg_gc = NULL;
	}
}




/* Returns pointer to internal static storage, changes next time function called, use quickly. */
char const * Viewport::get_background_color()
{
	static char color[8];
	snprintf(color, sizeof(color), "#%.2x%.2x%.2x",
		 (int) (background_color.red / 256),
		 (int) (background_color.green / 256),
		 (int) (background_color.blue / 256));
	return color;
}




void Viewport::set_background_color(char const * colorname)
{
	assert (background_gc);
	if (gdk_color_parse(colorname, &(background_color))) {
		gdk_gc_set_rgb_fg_color(background_gc, &(background_color));
	} else {
		fprintf(stderr, "WARNING: %s: Failed to parse color '%s'\n", __FUNCTION__, colorname);
	}
}




void Viewport::set_background_gdkcolor(GdkColor * color)
{
	assert (background_gc);
	background_color = *color;
	gdk_gc_set_rgb_fg_color(background_gc, color);
}




GdkColor * Viewport::get_highlight_gdkcolor()
{
	GdkColor * rv = (GdkColor *) malloc(sizeof (GdkColor));
	*rv = highlight_color; /* kamilTODO: what? */
	return rv;
}




/* Returns pointer to internal static storage, changes next time function called, use quickly. */
const char * Viewport::get_highlight_color()
{
	static char color[8];
	snprintf(color, sizeof(color), "#%.2x%.2x%.2x",
		 (int) (highlight_color.red / 256),
		 (int) (highlight_color.green / 256),
		 (int) (highlight_color.blue / 256));
	return color;
}




void Viewport::set_highlight_color(char const * colorname)
{
	assert (highlight_gc);
	gdk_color_parse(colorname, &(highlight_color));
	gdk_gc_set_rgb_fg_color(highlight_gc, &(highlight_color));
}




void Viewport::set_highlight_gdkcolor(GdkColor * color)
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
	/* Otherwise same GDK_* attributes as in Viewport::new_gc(). */
	gdk_gc_set_line_attributes(highlight_gc, thickness, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND);
}




GdkGC * Viewport::new_gc(char const * colorname, int thickness)
{
	GdkColor color;

	GdkGC * rv = gdk_gc_new(GTK_WIDGET(this->drawing_area_)->window);
	if (gdk_color_parse(colorname, &color)) {
		gdk_gc_set_rgb_fg_color(rv, &color);
	} else {
		fprintf(stderr, "WARNING: %s: Failed to parse color '%s'\n", __FUNCTION__, colorname);
	}
	gdk_gc_set_line_attributes(rv, thickness, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND);
	return rv;
}




GdkGC * Viewport::new_gc_from_color(GdkColor * color, int thickness)
{
	GdkGC * rv = gdk_gc_new(gtk_widget_get_window(GTK_WIDGET(this->drawing_area_)));
	gdk_gc_set_rgb_fg_color(rv, color);
	gdk_gc_set_line_attributes(rv, thickness, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND);
	return rv;
}




void Viewport::configure_manually(int width_, unsigned int height_)
{
	this->size_width = width_;
	this->size_height = height_;

	this->size_width_2 = this->size_width / 2;
	this->size_height_2 = this->size_height / 2;

	if (this->scr_buffer) {
		g_object_unref(G_OBJECT (this->scr_buffer));
	}
	this->scr_buffer = gdk_pixmap_new(gtk_widget_get_window(GTK_WIDGET(this->drawing_area_)), this->size_width, this->size_height, -1);

	/* TODO trigger: only if this is enabled!!! */
	if (this->snapshot_buffer) {
		g_object_unref(G_OBJECT (this->snapshot_buffer));
	}
	this->snapshot_buffer = gdk_pixmap_new(gtk_widget_get_window(GTK_WIDGET(this->drawing_area_)), this->size_width, this->size_height, -1);
}




GdkPixmap * Viewport::get_pixmap()
{
	return this->scr_buffer;
}




bool vik_viewport_configure_cb(GtkDrawingArea * drawing_area)
{
	fprintf(stderr, "=========== viewport configure event\n");
	assert (drawing_area);


	void * viewport = g_object_get_data((GObject *) drawing_area, "viewport");
	return ((Viewport *) viewport)->configure();
}




bool Viewport::configure()
{
	GtkAllocation allocation;
	gtk_widget_get_allocation(GTK_WIDGET(this->drawing_area_), &allocation);
	this->size_width = allocation.width;
	this->size_height = allocation.height;

	this->size_width_2 = this->size_width / 2;
	this->size_height_2 = this->size_height / 2;

	if (this->scr_buffer) {
		g_object_unref (G_OBJECT (this->scr_buffer));
	}

	this->scr_buffer = gdk_pixmap_new(gtk_widget_get_window(GTK_WIDGET(this->drawing_area_)), this->size_width, this->size_height, -1);

	/* TODO trigger: only if enabled! */
	if (this->snapshot_buffer) {
		g_object_unref(G_OBJECT (this->snapshot_buffer));
	}

	this->snapshot_buffer = gdk_pixmap_new(gtk_widget_get_window(GTK_WIDGET(this->drawing_area_)), this->size_width, this->size_height, -1);
	/* TODO trigger. */

	/* Rhis is down here so it can get a GC (necessary?). */
	if (!this->background_gc) {
		this->background_gc = this->new_gc(DEFAULT_BACKGROUND_COLOR, 1);
		this->set_background_color(DEFAULT_BACKGROUND_COLOR);
	}
	if (!this->highlight_gc) {
		this->highlight_gc = this->new_gc(DEFAULT_HIGHLIGHT_COLOR, 1);
		this->set_highlight_color(DEFAULT_HIGHLIGHT_COLOR);
	}
	if (!this->scale_bg_gc) {
		this->scale_bg_gc = this->new_gc("grey", 3);
	}

	return false;
}




/**
 * Clear the whole viewport.
 */
void Viewport::clear()
{
	gdk_draw_rectangle(GDK_DRAWABLE(scr_buffer), background_gc, true, 0, 0, this->size_width, this->size_height);
	this->reset_copyrights();
	this->reset_logos();
}




/**
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



/* Return length of scale bar, in pixels. */
int rescale_unit(double * base_distance, double * scale_unit, int maximum_width)
{
	double ratio = *base_distance / *scale_unit;

	int n = 0;
	if (ratio > 1) {
		n = (int) floor(log10(ratio));
	} else {
		n = (int) floor(log10(1.0 / ratio));
	}


	*scale_unit = pow(10.0, n); /* scale_unit is still a unit (1 km, 10 miles, 100 km, etc. ), only 10^n times larger. */
	ratio = *base_distance / *scale_unit;
	double len = maximum_width / ratio; /* [px] */


	/* I don't want the scale unit to be always 10^n.

	   Let's say that at this point we have a scale of length 10km
	   = 344 px. Let's see what actually happens as we zoom out:
	   zoom  0: 10 km / 344 px
	   zoom -1: 10 km / 172 px
	   zoom -2: 10 km /  86 px
	   zoom -3: 10 km /  43 px

	   At zoom -3 the scale is small and not very useful. With the code
	   below enabled we get:

	   zoom  0: 10 km / 345 px
	   zoom -1: 20 km / 345 px
	   zoom -2: 20 km / 172 px
	   zoom -3: 50 km / 216 px

	   We can see that the scale doesn't become very short, and keeps being usable. */

	if (maximum_width / len > 5) {
		*scale_unit *= 5;
		ratio = *base_distance / *scale_unit;
		len = maximum_width / ratio;

	} else if (maximum_width / len > 2) {
		*scale_unit *= 2;
		ratio = *base_distance / *scale_unit;
		len = maximum_width / ratio;

	} else {
		;
	}

	return (int) len;
}




void Viewport::draw_scale()
{
	if (!this->do_draw_scale) {
		return;
	}

	VikCoord left, right;
	double base_distance;       /* Physical (real world) distance corresponding to full width of drawn scale. Physical units (miles, meters). */
	int HEIGHT = 20;            /* Height of scale in pixels. */
	float RELATIVE_WIDTH = 0.5; /* Width of scale, relative to width of viewport. */
	int MAXIMUM_WIDTH = this->size_width * RELATIVE_WIDTH;

	this->screen_to_coord(0,                                 this->size_height / 2, &left);
	this->screen_to_coord(this->size_width * RELATIVE_WIDTH, this->size_height / 2, &right);

	DistanceUnit distance_unit = a_vik_get_units_distance();
	switch (distance_unit) {
	case DistanceUnit::KILOMETRES:
		base_distance = vik_coord_diff(&left, &right); /* In meters. */
		break;
	case DistanceUnit::MILES:
		/* In 0.1 miles (copes better when zoomed in as 1 mile can be too big). */
		base_distance = VIK_METERS_TO_MILES(vik_coord_diff(&left, &right)) * 10.0;
		break;
	case DistanceUnit::NAUTICAL_MILES:
		/* In 0.1 NM (copes better when zoomed in as 1 NM can be too big). */
		base_distance = VIK_METERS_TO_NAUTICAL_MILES(vik_coord_diff(&left, &right)) * 10.0;
		break;
	default:
		base_distance = 1; /* Keep the compiler happy. */
		fprintf(stderr, "CRITICAL: failed to get correct units of distance, got %d\n", distance_unit);
	}

	/* At this point "base_distance" is a distance between "left" and "right" in physical units.
	   But a scale can't have an arbitrary length (e.g. 3.07 miles, or 23.2 km),
	   it should be something like 1.00 mile or 10.00 km - a unit. */
	double scale_unit = 1; /* [km, miles, nautical miles] */

	int len = rescale_unit(&base_distance, &scale_unit, MAXIMUM_WIDTH);

	GdkGC * paint_bg = scale_bg_gc;
	GdkGC * paint_fg = gtk_widget_get_style(GTK_WIDGET(this->drawing_area_))->black_gc;

	/* White background. */
	this->draw_line(paint_bg, PAD,       this->size_height - PAD, PAD + len, this->size_height - PAD);
	this->draw_line(paint_bg, PAD,       this->size_height - PAD, PAD,       this->size_height - PAD - HEIGHT);
	this->draw_line(paint_bg, PAD + len, this->size_height - PAD, PAD + len, this->size_height - PAD - HEIGHT);

	/* Black scale. */
	this->draw_line(paint_fg, PAD,       this->size_height - PAD, PAD + len, this->size_height - PAD);
	this->draw_line(paint_fg, PAD,       this->size_height - PAD, PAD,       this->size_height - PAD - HEIGHT);
	this->draw_line(paint_fg, PAD + len, this->size_height - PAD, PAD + len, this->size_height - PAD - HEIGHT);


	int y1 = this->size_height - PAD;
	for (int i = 1; i < 10; i++) {
		int x1 = PAD + i * len / 10;
		int diff = ((i == 5) ? (2 * HEIGHT / 3) : (1 * HEIGHT / 3));
		this->draw_line(paint_bg, x1, y1, x1, y1 - diff);
		this->draw_line(paint_fg, x1, y1, x1, y1 - diff);
	}

	char s[128];
	switch (distance_unit) {
	case DistanceUnit::KILOMETRES:
		if (scale_unit >= 1000) {
			sprintf(s, "%d km", (int) scale_unit / 1000);
		} else {
			sprintf(s, "%d m", (int) scale_unit);
		}
		break;
	case DistanceUnit::MILES:
		/* Handle units in 0.1 miles. */
		if (scale_unit < 10.0) {
			sprintf(s, "%0.1f miles", scale_unit / 10.0);
		} else if ((int) scale_unit == 10.0) {
			sprintf(s, "1 mile");
		} else {
			sprintf(s, "%d miles", (int) (scale_unit / 10.0));
		}
		break;
	case DistanceUnit::NAUTICAL_MILES:
		/* Handle units in 0.1 NM. */
		if (scale_unit < 10.0) {
			sprintf(s, "%0.1f NM", scale_unit / 10.0);
		} else if ((int) scale_unit == 10.0) {
			sprintf(s, "1 NM");
		} else {
			sprintf(s, "%d NMs", (int) (scale_unit / 10.0));
		}
		break;
	default:
		fprintf(stderr, "CRITICAL: failed to get correct units of distance, got %d\n", distance_unit);
	}


	PangoLayout * pl = gtk_widget_create_pango_layout(GTK_WIDGET(this->drawing_area_), NULL);
	pango_layout_set_font_description(pl, gtk_widget_get_style(GTK_WIDGET(this->drawing_area_))->font_desc);
	pango_layout_set_text(pl, s, -1);
	this->draw_layout(paint_fg, PAD + len + PAD, this->size_height - PAD - 10, pl);
	g_object_unref(pl);
	pl = NULL;
}




void Viewport::draw_copyright()
{
	char s[128] = "";

	/* Compute copyrights string. */
	unsigned int len = g_slist_length(copyrights);

	for (unsigned int i = 0 ; i < len ; i++) {
		/* Stop when buffer is full. */
		int slen = strlen(s);
		if (slen >= 127) {
			break;
		}

		char * copyright = (char *) g_slist_nth_data(copyrights, i);

		/* Only use part of this copyright that fits in the available space left,
		   remembering 1 character is left available for the appended space. */
		int clen = strlen (copyright);
		if (slen + clen > 126) {
			clen = 126 - slen;
		}

		strncat(s, copyright, clen);
		strcat(s, " ");
	}

	/* Create pango layout. */
	PangoLayout * pl = gtk_widget_create_pango_layout(GTK_WIDGET(this->drawing_area_), NULL);
	pango_layout_set_font_description(pl, gtk_widget_get_style(GTK_WIDGET(this->drawing_area_))->font_desc);
	pango_layout_set_alignment(pl, PANGO_ALIGN_RIGHT);

	/* Set the text. */
	pango_layout_set_text(pl, s, -1);

	PangoRectangle ink_rect, logical_rect;
	/* Use maximum of half the viewport width. */
	pango_layout_set_width(pl, (this->size_width / 2) * PANGO_SCALE);
	pango_layout_get_pixel_extents(pl, &ink_rect, &logical_rect);
	this->draw_layout(gtk_widget_get_style(GTK_WIDGET(this->drawing_area_))->black_gc,
			  this->size_width / 2, this->size_height - logical_rect.height, pl);

	/* Free memory. */
	g_object_unref(pl);
	pl = NULL;
}




/**
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
	if (!do_draw_centermark) {
		return;
	}

	const int len = 30;
	const int gap = 4;
	int center_x = this->size_width / 2;
	int center_y = this->size_height / 2;

	GdkGC * black_gc = gtk_widget_get_style(GTK_WIDGET(this->drawing_area_))->black_gc;

	/* White background. */
	this->draw_line(scale_bg_gc, center_x - len, center_y,       center_x - gap, center_y);
	this->draw_line(scale_bg_gc, center_x + gap, center_y,       center_x + len, center_y);
	this->draw_line(scale_bg_gc, center_x,       center_y - len, center_x,       center_y - gap);
	this->draw_line(scale_bg_gc, center_x,       center_y + gap, center_x,       center_y + len);

	/* Black foreground. */
	this->draw_line(black_gc, center_x - len, center_y,        center_x - gap, center_y);
	this->draw_line(black_gc, center_x + gap, center_y,        center_x + len, center_y);
	this->draw_line(black_gc, center_x,       center_y - len,  center_x,       center_y - gap);
	this->draw_line(black_gc, center_x,       center_y + gap,  center_x,       center_y + len);
}




void Viewport::draw_logo()
{
	unsigned int len = g_slist_length(logos);
	int x = this->size_width - PAD;
	int y = PAD;
	for (unsigned int i = 0; i < len; i++) {
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




void Viewport::sync()
{
	gdk_draw_drawable(gtk_widget_get_window(GTK_WIDGET(this->drawing_area_)), gtk_widget_get_style(GTK_WIDGET(this->drawing_area_))->bg_gc[0], GDK_DRAWABLE(this->scr_buffer), 0, 0, 0, 0, this->size_width, this->size_height);
}




void Viewport::pan_sync(int x_off, int y_off)
{
	int x, y, wid, hei;

	gdk_draw_drawable(gtk_widget_get_window(GTK_WIDGET(this->drawing_area_)), gtk_widget_get_style(GTK_WIDGET(this->drawing_area_))->bg_gc[0], GDK_DRAWABLE(this->scr_buffer), 0, 0, x_off, y_off, this->size_width, this->size_height);

	if (x_off >= 0) {
		x = 0;
		wid = x_off;
	} else {
		x = this->size_width + x_off;
		wid = -x_off;
	}

	if (y_off >= 0) {
		y = 0;
		hei = y_off;
	} else {
		y = this->size_height + y_off;
		hei = -y_off;
	}
	gtk_widget_queue_draw_area(GTK_WIDGET(this->drawing_area_), x, 0, wid, this->size_height);
	gtk_widget_queue_draw_area(GTK_WIDGET(this->drawing_area_), 0, y, this->size_width, hei);
}




void Viewport::set_zoom(double xympp_)
{
	if (xympp_ >= VIK_VIEWPORT_MIN_ZOOM && xympp_ <= VIK_VIEWPORT_MAX_ZOOM) {
		xmpp = ympp = xympp_;
		/* Since xmpp & ympp are the same it doesn't matter which one is used here. */
		xmfactor = ymfactor = MERCATOR_FACTOR(xmpp);
	}

	if (drawmode == VIK_VIEWPORT_DRAWMODE_UTM) {
		this->utm_zone_check();
	}
}




/* Or could do factor. */
void Viewport::zoom_in()
{
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
	return &center;
}




/* Called every time we update coordinates/zoom. */
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

		/* Misc. stuff so we don't have to check later. */
		utm_zone_width = this->calculate_utm_zone_width();
		one_utm_zone = (this->rightmost_zone() == this->leftmost_zone());
	}
}




/**
 * Free an individual center position in the history list.
 */
void Viewport::free_center(std::list<Coord *>::iterator iter)
{
	VikCoord * coord = *iter;
	if (coord) {
		free(coord);
	}

	if (iter == centers_iter) {
		centers_iter = centers->erase(iter);
		if (centers_iter == centers->end()) {
			if (centers->empty()) {
				centers_iter = centers->begin();
			} else {
				centers_iter--;
			}
		}
	} else {
		centers->erase(iter);
	}
}




/**
 * Store the current center position into the history list
 * and emit a signal to notify clients the list has been updated.
 */
void Viewport::update_centers()
{
	VikCoord * new_center = (VikCoord *) malloc(sizeof (VikCoord));
	*new_center = center; /* kamilFIXME: what does this assignment do? */

	if (centers_iter == prev(centers->end())) {
		/* We are at most recent element of history. */
		if (centers->size() == centers_max) {
			/* List is full, so drop the oldest value to make room for the new one. */
			this->free_center(centers->begin());
		}
	} else {
		/* We are somewhere in the middle of history list, possibly at the beginning.
		   Every center visited after current one must be discarded. */
		centers->erase(next(centers_iter), centers->end());
		assert (std::next(centers_iter) == centers->end());
	}

	/* Store new position. */
	/* push_back() puts at the end. By convention end == newest. */
	centers->push_back(new_center);
	centers_iter++;
	assert (std::next(centers_iter) == centers->end());

	this->print_centers((char *) "update_centers");

	fprintf(stderr, "=========== issuing updated center signal\n");

	/* Inform interested subscribers that this change has occurred. */
	g_signal_emit(G_OBJECT(this->drawing_area_), viewport_signals[VW_UPDATED_CENTER_SIGNAL], 0);
}




/**
 * Show the list of forward/backward positions.
 * ATM only for debug usage.
 */
void Viewport::show_centers(GtkWindow * parent)
{
	GList * texts = NULL;
	for (auto iter = centers->begin(); iter != centers->end(); iter++) {
		char *lat = NULL, *lon = NULL;
		struct LatLon ll;
		vik_coord_to_latlon(*iter, &ll);
		a_coords_latlon_to_string(&ll, &lat, &lon);
		char *extra = NULL;
		if (iter == next(centers_iter)) {
			extra = strdup(" [Back]");
		} else if (iter == prev(centers_iter)) {
			extra = strdup(" [Forward]");
		} else {
			extra = strdup("");
		}

		texts = g_list_prepend(texts, g_strdup_printf("%s %s%s", lat, lon, extra));
		free(lat);
		free(lon);
		free(extra);
	}

	/* NB: No i18n as this is just for debug.
	   Using this function the dialog allows sorting of the list which isn't appropriate here
	   but this doesn't matter much for debug purposes of showing stuff... */
	GList * ans = a_dialog_select_from_list(parent,
						texts,
						false,
						"Back/Forward Locations",
						"Back/Forward Locations");
	for (GList * node = ans; node != NULL; node = g_list_next(node)) {
		free(node->data);
	}

	g_list_free(ans);
	for (GList * node = texts; node != NULL; node = g_list_next(node)) {
		free(node->data);
	}
	g_list_free(texts);
}




void Viewport::print_centers(char * label)
{
	for (auto iter = centers->begin(); iter != centers->end(); iter++) {
		char *lat = NULL, *lon = NULL;
		struct LatLon ll;
		vik_coord_to_latlon(*iter, &ll);
		a_coords_latlon_to_string(&ll, &lat, &lon);
		char * extra = NULL;
		if (iter == prev(centers_iter)) {
			extra = (char *) "[Back]";
		} else if (iter == next(centers_iter)) {
			extra = (char *) "[Forward]";
		} else if (iter == centers_iter) {
			extra = (char *) "[Current]";
		} else {
			extra = (char *) "";
		}

		fprintf(stderr, "*** centers (%s): %s %s %s\n", label, lat, lon, extra);

		free(lat);
		free(lon);
	}

	return;
}





/**
 * Returns: true on success.
 */
bool Viewport::go_back()
{
	/* See if the current position is different from the last saved center position within a certain radius. */
	VikCoord * last_center = *centers_iter;
	if (last_center) {
		/* Consider an exclusion size (should it zoom level dependent, rather than a fixed value?).
		   When still near to the last saved position we'll jump over it to the one before. */
		if (vik_coord_diff(last_center, &center) > centers_radius) {

			if (centers_iter == prev(centers->end())) {
				/* Only when we haven't already moved back in the list.
				   Remember where this request came from (alternatively we could insert in the list on every back attempt). */
				update_centers();
			}

		}
		/* 'Go back' if possible.
		   NB if we inserted a position above, then this will then move to the last saved position.
		   Otherwise this will skip to the previous saved position, as it's probably somewhere else. */
		if (back_available()) {
			centers_iter--;
		}
	} else {
		return false;
	}

	VikCoord * new_center = *centers_iter;
	if (new_center) {
		set_center_coord(new_center, false);
		return true;
	}
	return false;
}




/**
 * Move forward in the position history.
 *
 * Returns: true on success.
 */
bool Viewport::go_forward()
{
	if (centers_iter == prev(centers->end())) {
		/* Already at the latest center. */
		return false;
	}

	centers_iter++;
	VikCoord * new_center = *centers_iter;
	if (new_center) {
		set_center_coord(new_center, false);
		return true;
	} else {
		centers_iter = prev(centers->end());
	}

	return false;
}




/**
 * Returns: true when a previous position in the history is available.
 */
bool Viewport::back_available()
{
	return (centers->size() > 1 && centers_iter != centers->begin());
}




/**
 * Returns: true when a next position in the history is available.
 */
bool Viewport::forward_available()
{
	return (centers->size() > 1 && centers_iter != prev(centers->end()));
}




/**
 * @ll:            The new center position in Lat/Lon format
 * @save_position: Whether this new position should be saved into the history of positions
 *                 Normally only specific user requests should be saved (i.e. to not include Pan and Zoom repositions)
 */
void Viewport::set_center_latlon(const struct LatLon * ll, bool save_position)
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
 * @utm:           The new center position in UTM format
 * @save_position: Whether this new position should be saved into the history of positions
 *                 Normally only specific user requests should be saved (i.e. to not include Pan and Zoom repositions)
 */
void Viewport::set_center_utm(const struct UTM * utm, bool save_position)
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
 * @coord:         The new center position in a VikCoord type
 * @save_position: Whether this new position should be saved into the history of positions
 *                 Normally only specific user requests should be saved (i.e. to not include Pan and Zoom repositions)
 */
void Viewport::set_center_coord(const VikCoord * coord, bool save_position)
{
	center = *coord;
	if (save_position) {
		update_centers();
	}
	if (coord_mode == VIK_COORD_UTM) {
		this->utm_zone_check();
	}
}




void Viewport::corners_for_zonen(int zone, VikCoord * ul, VikCoord * br)
{
	if (coord_mode != VIK_COORD_UTM) {
		return;
	}

	/* Get center, then just offset. */
	this->center_for_zonen((struct UTM *) (ul), zone);
	ul->mode = VIK_COORD_UTM;
	*br = *ul;

	ul->north_south += (ympp * this->size_height / 2);
	ul->east_west -= (xmpp * this->size_width / 2);
	br->north_south -= (ympp * this->size_height / 2);
	br->east_west += (xmpp * this->size_width / 2);
}




void Viewport::center_for_zonen(struct UTM * center_, int zone)
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
		this->screen_to_coord(0, 0, &coord);
		return coord.utm_zone;
	}
	return '\0';
}




char Viewport::rightmost_zone()
{
	if (coord_mode == VIK_COORD_UTM) {
		VikCoord coord;
		this->screen_to_coord(this->size_width, 0, &coord);
		return coord.utm_zone;
	}
	return '\0';
}




void Viewport::set_center_screen(int x, int y)
{
	if (coord_mode == VIK_COORD_UTM) {
		/* Slightly optimized. */
		center.east_west += xmpp * (x - (this->size_width / 2));
		center.north_south += ympp * ((this->size_height / 2) - y);
		this->utm_zone_check();
	} else {
		VikCoord tmp;
		this->screen_to_coord(x, y, &tmp);
		set_center_coord(&tmp, false);
	}
}




int Viewport::get_width()
{
	return this->size_width;
}




int Viewport::get_height()
{
	return this->size_height;
}




void Viewport::screen_to_coord(int x, int y, VikCoord * coord)
{
	if (coord_mode == VIK_COORD_UTM) {
		int zone_delta;
		struct UTM *utm = (struct UTM *) coord;
		coord->mode = VIK_COORD_UTM;

		utm->zone = center.utm_zone;
		utm->letter = center.utm_letter;
		utm->easting = ((x - (this->size_width_2)) * xmpp) + center.east_west;
		zone_delta = floor((utm->easting - EASTING_OFFSET) / utm_zone_width + 0.5);
		utm->zone += zone_delta;
		utm->easting -= zone_delta * utm_zone_width;
		utm->northing = (((this->size_height_2) - y) * ympp) + center.north_south;
	} else if (coord_mode == VIK_COORD_LATLON) {
		coord->mode = VIK_COORD_LATLON;
		if (drawmode == VIK_VIEWPORT_DRAWMODE_LATLON) {
			coord->east_west = center.east_west + (180.0 * xmpp / 65536 / 256 * (x - this->size_width_2));
			coord->north_south = center.north_south + (180.0 * ympp / 65536 / 256 * (this->size_height_2 - y));
		} else if (drawmode == VIK_VIEWPORT_DRAWMODE_EXPEDIA) {
			calcxy_rev(&(coord->east_west), &(coord->north_south), x, y, center.east_west, center.north_south, xmpp * ALTI_TO_MPP, ympp * ALTI_TO_MPP, this->size_width_2, this->size_height_2);
		} else if (drawmode == VIK_VIEWPORT_DRAWMODE_MERCATOR) {
			/* This isn't called with a high frequently so less need to optimize. */
			coord->east_west = center.east_west + (180.0 * xmpp / 65536 / 256 * (x - this->size_width_2));
			coord->north_south = DEMERCLAT (MERCLAT(center.north_south) + (180.0 * ympp / 65536 / 256 * (this->size_height_2 - y)));
		} else {
			;
		}
	}
}




/*
 * Since this function is used for every drawn trackpoint - it can get called a lot.
 * Thus x & y position factors are calculated once on zoom changes,
 * avoiding the need to do it here all the time.
 * For good measure the half width and height values are also pre calculated too.
 */
void Viewport::coord_to_screen(const VikCoord * coord, int * x, int * y)
{
	static VikCoord tmp;

	if (coord->mode != this->coord_mode){
		fprintf(stderr, "WARNING: Have to convert in Viewport::coord_to_screen()! This should never happen!\n");
		vik_coord_copy_convert (coord, this->coord_mode, &tmp);
		coord = &tmp;
	}

	if (this->coord_mode == VIK_COORD_UTM) {
		struct UTM *center = (struct UTM *) &(this->center);
		struct UTM *utm = (struct UTM *) coord;
		if (center->zone != utm->zone && this->one_utm_zone){
			*x = *y = VIK_VIEWPORT_UTM_WRONG_ZONE;
			return;
		}

		*x = ((utm->easting - center->easting) / this->xmpp) + (this->size_width_2) -
			(center->zone - utm->zone) * this->utm_zone_width / this->xmpp;
		*y = (this->size_height_2) - ((utm->northing - center->northing) / this->ympp);
	} else if (this->coord_mode == VIK_COORD_LATLON) {
		struct LatLon *center = (struct LatLon *) &(this->center);
		struct LatLon *ll = (struct LatLon *) coord;
		double xx,yy;
		if (this->drawmode == VIK_VIEWPORT_DRAWMODE_LATLON) {
			*x = this->size_width_2 + (MERCATOR_FACTOR(this->xmpp) * (ll->lon - center->lon));
			*y = this->size_height_2 + (MERCATOR_FACTOR(this->ympp) * (center->lat - ll->lat));
		} else if (this->drawmode == VIK_VIEWPORT_DRAWMODE_EXPEDIA) {
			calcxy (&xx, &yy, center->lon, center->lat, ll->lon, ll->lat, this->xmpp * ALTI_TO_MPP, this->ympp * ALTI_TO_MPP, this->size_width_2, this->size_height_2);
			*x = xx; *y = yy;
		} else if (this->drawmode == VIK_VIEWPORT_DRAWMODE_MERCATOR) {
			*x = this->size_width_2 + (MERCATOR_FACTOR(this->xmpp) * (ll->lon - center->lon));
			*y = this->size_height_2 + (MERCATOR_FACTOR(this->ympp) * (MERCLAT(center->lat) - MERCLAT(ll->lat)));
		}
	}
}




/* Clip functions continually reduce the value by a factor until it is in the acceptable range
   whilst also scaling the other coordinate value. */
static void clip_x(int * x1, int * y1, int * x2, int * y2)
{
	while (ABS(*x1) > 32768) {
		*x1 = *x2 + (0.5 * (*x1 - *x2));
		*y1 = *y2 + (0.5 * (*y1 - *y2));
	}
}




static void clip_y(int * x1, int * y1, int * x2, int * y2)
{
	while (ABS(*y1) > 32767) {
		*x1 = *x2 + (0.5 * (*x1 - *x2));
		*y1 = *y2 + (0.5 * (*y1 - *y2));
	}
}




/**
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




void Viewport::draw_line(GdkGC * gc, int x1, int y1, int x2, int y2)
{
	if (! ((x1 < 0 && x2 < 0) || (y1 < 0 && y2 < 0)
	       || (x1 > this->size_width && x2 > this->size_width)
	       || (y1 > this->size_height && y2 > this->size_height))) {

		/*** Clipping, yeah! ***/
		Viewport::clip_line(&x1, &y1, &x2, &y2);
		gdk_draw_line(this->scr_buffer, gc, x1, y1, x2, y2);
	}
}




void Viewport::draw_rectangle(GdkGC * gc, bool filled, int x1, int y1, int x2, int y2)
{
	/* Using 32 as half the default waypoint image size, so this draws ensures the highlight gets done. */
	if (x1 > -32 && x1 < this->size_width + 32 && y1 > -32 && y1 < this->size_height + 32) {
		gdk_draw_rectangle(this->scr_buffer, gc, filled, x1, y1, x2, y2);
	}
}




void Viewport::draw_string(GdkFont * font, GdkGC * gc, int x1, int y1, const char *string)
{
	if (x1 > -100 && x1 < this->size_width + 100 && y1 > -100 && y1 < this->size_height + 100) {
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




void Viewport::draw_arc(GdkGC * gc, bool filled, int x, int y, int width, int height, int angle1, int angle2)
{
	gdk_draw_arc(this->scr_buffer, gc, filled, x, y, width, height, angle1, angle2);
}




void Viewport::draw_polygon(GdkGC * gc, bool filled, GdkPoint *points, int npoints)
{
	gdk_draw_polygon(this->scr_buffer, gc, filled, points, npoints);
}




VikCoordMode Viewport::get_coord_mode()
{
	return coord_mode;
}




void Viewport::set_coord_mode(VikCoordMode mode_)
{
	coord_mode = mode_;
	vik_coord_convert(&(center), mode_);
}




/* Thanks GPSDrive. */
static bool calcxy_rev(double * lg, double * lt, int x, int y, double zero_long, double zero_lat, double pixelfact_x, double pixelfact_y, int mapSizeX2, int mapSizeY2)
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




/* Thanks GPSDrive. */
static bool calcxy(double * x, double * y, double lg, double lt, double zero_long, double zero_lat, double pixelfact_x, double pixelfact_y, int mapSizeX2, int mapSizeY2)
{
	int mapSizeX = 2 * mapSizeX2;
	int mapSizeY = 2 * mapSizeY2;

	assert (lt >= -90.0 && lt <= 90.0);
	//    lg *= rad2deg; // FIXME, optimize equations
	//    lt *= rad2deg;
	double Ra = Radius[90 + (int) lt];
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
		return false;
	}
	return true;
}




static void viewport_init_ra()
{
	static bool done_before = false;
	if (!done_before) {
		for (int i = -90; i <= 90; i++) {
			Radius[i + 90] = calcR (DEG2RAD((double)i));
		}
		done_before = true;
	}
}




double calcR(double lat)
{
	/*
	 * The radius of curvature of an ellipsoidal Earth in the plane of the
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




void Viewport::draw_layout(GdkGC * gc, int x, int y, PangoLayout * layout)
{
	if (x > -100 && x < this->size_width + 100 && y > -100 && y < this->size_height + 100) {
		gdk_draw_layout(this->scr_buffer, gc, x, y, layout);
	}
}




void vik_gc_get_fg_color(GdkGC * gc, GdkColor * dest)
{
	static GdkGCValues values;
	gdk_gc_get_values(gc, &values);
	gdk_colormap_query_color(gdk_colormap_get_system(), values.foreground.pixel, dest);
}




GdkFunction vik_gc_get_function(GdkGC * gc)
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




/******** Triggering. *******/
void Viewport::set_trigger(Layer * trigger)
{
	this->trigger = trigger;
}




Layer * Viewport::get_trigger()
{
	return this->trigger;
}




void Viewport::snapshot_save()
{
	gdk_draw_drawable(this->snapshot_buffer, this->background_gc, this->scr_buffer, 0, 0, 0, 0, -1, -1);
}




void Viewport::snapshot_load()
{
	gdk_draw_drawable(this->scr_buffer, this->background_gc, this->snapshot_buffer, 0, 0, 0, 0, -1, -1);
}




void Viewport::set_half_drawn(bool half_drawn_)
{
	half_drawn = half_drawn_;
}




bool Viewport::get_half_drawn()
{
	return this->half_drawn;
}




char const * Viewport::get_drawmode_name(VikViewportDrawMode mode)
{
	GtkWidget * mode_button = this->get_window()->get_drawmode_button(mode);
	GtkWidget * label = gtk_bin_get_child(GTK_BIN(mode_button));

	return gtk_label_get_text(GTK_LABEL(label));

}




void Viewport::get_min_max_lat_lon(double * min_lat, double * max_lat, double * min_lon, double * max_lon)
{
	VikCoord tleft, tright, bleft, bright;

	this->screen_to_coord(0,                0,                 &tleft);
	this->screen_to_coord(this->size_width, 0,                 &tright);
	this->screen_to_coord(0,                this->size_height, &bleft);
	this->screen_to_coord(this->size_width, this->size_height, &bright);

	vik_coord_convert(&tleft, VIK_COORD_LATLON);
	vik_coord_convert(&tright, VIK_COORD_LATLON);
	vik_coord_convert(&bleft, VIK_COORD_LATLON);
	vik_coord_convert(&bright, VIK_COORD_LATLON);

	*max_lat = MAX(tleft.north_south, tright.north_south);
	*min_lat = MIN(bleft.north_south, bright.north_south);
	*max_lon = MAX(tright.east_west, bright.east_west);
	*min_lon = MIN(tleft.east_west, bleft.east_west);
}




void Viewport::get_bbox(LatLonBBox * bbox)
{
	VikCoord tleft, tright, bleft, bright;

	this->screen_to_coord(0,                0,                 &tleft);
	this->screen_to_coord(this->size_width, 0,                 &tright);
	this->screen_to_coord(0,                this->size_height, &bleft);
	this->screen_to_coord(this->size_width, this->size_height, &bright);

	vik_coord_convert(&tleft, VIK_COORD_LATLON);
	vik_coord_convert(&tright, VIK_COORD_LATLON);
	vik_coord_convert(&bleft, VIK_COORD_LATLON);
	vik_coord_convert(&bright, VIK_COORD_LATLON);

	bbox->north = MAX(tleft.north_south, tright.north_south);
	bbox->south = MIN(bleft.north_south, bright.north_south);
	bbox->east  = MAX(tright.east_west, bright.east_west);
	bbox->west  = MIN(tleft.east_west, bleft.east_west);
}




void Viewport::get_bbox_strings(LatLonBBoxStrings * bbox_strings)
{
	LatLonBBox bbox;
	/* Get Viewport bounding box. */
	this->get_bbox(&bbox);

	/* Cannot simply use g_strdup_printf and double due to locale.
	   As we compute an URL, we have to work in C locale. */
	g_ascii_dtostr(bbox_strings->sminlon, G_ASCII_DTOSTR_BUF_SIZE, bbox.west);
	g_ascii_dtostr(bbox_strings->smaxlon, G_ASCII_DTOSTR_BUF_SIZE, bbox.east);
	g_ascii_dtostr(bbox_strings->sminlat, G_ASCII_DTOSTR_BUF_SIZE, bbox.south);
	g_ascii_dtostr(bbox_strings->smaxlat, G_ASCII_DTOSTR_BUF_SIZE, bbox.north);

	return;
}




void Viewport::reset_copyrights()
{
	g_slist_foreach(copyrights, (GFunc) g_free, NULL);
	g_slist_free(copyrights);
	copyrights = NULL;
}




/**
 * @copyright: new copyright to display.
 *
 * Add a copyright to display on viewport.
 */
void Viewport::add_copyright(char const * copyright_)
{
	/* kamilTODO: make sure that this code is executed. */
	if (copyright_) {
		GSList * found = g_slist_find_custom(copyrights, copyright_, (GCompareFunc) strcmp);
		if (found == NULL) {
			char *duple = g_strdup(copyright_);
			copyrights = g_slist_prepend(copyrights, duple);
		}
	}
}




void vik_viewport_add_copyright_cb(Viewport * viewport, char const * copyright_)
{
	viewport->add_copyright(copyright_);
}




void Viewport::reset_logos()
{
	/* do not free elem */
	g_slist_free(logos);
	logos = NULL;
}




void Viewport::add_logo(const GdkPixbuf *logo_)
{
	if (logo_) {
		GdkPixbuf * found = NULL; /* FIXME (GdkPixbuf*)g_slist_find_custom (vp->port.logos, logo, (GCompareFunc)==); */
		if (found == NULL) {
			logos = g_slist_prepend(logos, (void *) logo_);
		}
	}
}




/**
 * @x1: screen coord
 * @y1: screen coord
 * @x2: screen coord
 * @y2: screen coord
 * @angle: bearing in Radian (output)
 * @baseangle: UTM base angle in Radian (output)
 *
 * Compute bearing.
 */
void Viewport::compute_bearing(int x1, int y1, int x2, int y2, double * angle, double * baseangle)
{
	double len = sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
	double dx = (x2 - x1) / len * 10;
	double dy = (y2 - y1) / len * 10;

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

		*baseangle = M_PI - atan2(tx - x1, ty - y1);
		*angle -= *baseangle;
	}

	if (*angle < 0) {
		*angle += 2 * M_PI;
	}

	if (*angle > 2 * M_PI) {
		*angle -= 2 * M_PI;
	}
}




GtkWidget * Viewport::get_toolkit_widget(void)
{
	return GTK_WIDGET(this->drawing_area_);
}




GtkWindow * Viewport::get_toolkit_window(void)
{
	return GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(this->drawing_area_)));
}




void * Viewport::get_toolkit_object(void)
{
	return (void *) this->drawing_area_;
}




Window * Viewport::get_window(void)
{
	GtkWindow * w = (GtkWindow *) gtk_widget_get_toplevel(GTK_WIDGET(this->drawing_area_));
	Window * window = (Window *) g_object_get_data((GObject *) w, "window");

	return window;
}
