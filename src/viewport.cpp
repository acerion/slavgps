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

#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cassert>

#include <QMouseEvent>
#include <QWheelEvent>

#include "viewport.h"
#include "window.h"
#include "coords.h"
#include "dems.h"
#include "mapcoord.h"

/* For ALTI_TO_MPP. */
#include "globals.h"
#include "settings.h"
#include "dialog.h"




using namespace SlavGPS;




#define DEFAULT_BACKGROUND_COLOR "#CCCCCC"
#define DEFAULT_HIGHLIGHT_COLOR "#EEA500"
/* Default highlight in orange */




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





void SlavGPS::viewport_init(void)
{
	viewport_init_ra();
}




void Viewport::init_drawing_area()
{
	//connect(this, SIGNAL(resizeEvent(QResizeEvent *)), this, SLOT(configure_cb(void)));
	//this->qpainter = new QPainter(this);

	this->setFocusPolicy(Qt::ClickFocus);
#if 0

#if GTK_CHECK_VERSION (2,18,0)
	gtk_widget_set_can_focus(GTK_WIDGET (this->drawing_area_), true);
#else
	GTK_WIDGET_SET_FLAGS (this->drawing_area_, GTK_CAN_FOCUS); /* Allow drawing area to have focus -- enabling key events, etc. */
#endif
#endif
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




QColor * Viewport::get_background_gdkcolor()
{
	return new QColor(this->background_color);
}




Viewport::Viewport(Window * parent) : QWidget((QWidget *) parent)
{
	this->window = parent;

	this->setMinimumSize(200, 300);
	this->setMaximumSize(2700, 2700);

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

	/* We want to constantly update cursor position in status bar. For this we need cursor tracking in viewport. */
	this->setMouseTracking(true);

	/* Initiate center history. */
	update_centers();

	strncpy(this->type_string, "Le Viewport", (sizeof (this->type_string)) - 1);
	this->do_draw_scale = true;
}




Viewport::~Viewport()
{
	qDebug() << "II: Viewport: ~Viewport called";
	if (a_vik_get_startup_method() == VIK_STARTUP_METHOD_LAST_LOCATION) {
		struct LatLon ll;
		vik_coord_to_latlon(&(this->center), &ll);
		a_settings_set_double(VIK_SETTINGS_VIEW_LAST_LATITUDE, ll.lat);
		a_settings_set_double(VIK_SETTINGS_VIEW_LAST_LONGITUDE, ll.lon);
		a_settings_set_double(VIK_SETTINGS_VIEW_LAST_ZOOM_X, this->xmpp);
		a_settings_set_double(VIK_SETTINGS_VIEW_LAST_ZOOM_Y, this->ympp);
	}

	delete this->centers;
	delete this->scr_buffer;
	delete this->snapshot_buffer;

#ifndef SLAVGPS_QT
	if (this->background_gc) {
		g_object_unref(G_OBJECT (this->background_gc));
	}

	if (this->highlight_gc) {
		g_object_unref(G_OBJECT (this->highlight_gc));
	}
#endif
}




/* Returns pointer to internal static storage, changes next time function called, use quickly. */
char const * Viewport::get_background_color()
{
	static char color[8];
	snprintf(color, sizeof (color), "#%.2x%.2x%.2x",
		 (int) (this->background_color.red() / 256),
		 (int) (this->background_color.green() / 256),
		 (int) (this->background_color.blue() / 256));
	return color;
}




void Viewport::set_background_color(char const * colorname)
{
	this->background_color.setNamedColor(colorname);
	this->background_gc->setColor(this->background_color);
}




void Viewport::set_background_gdkcolor(QColor * color)
{
	assert (this->background_gc);
	this->background_color = *color;
	this->background_gc->setColor(this->background_color);
}




QColor * Viewport::get_highlight_gdkcolor()
{
	return new QColor(this->highlight_color);
}




/* Returns pointer to internal static storage, changes next time function called, use quickly. */
const char * Viewport::get_highlight_color()
{
	static char color[8];
	snprintf(color, sizeof(color), "#%.2x%.2x%.2x",
		 (int) (highlight_color.red() / 256),
		 (int) (highlight_color.green() / 256),
		 (int) (highlight_color.blue() / 256));
	return color;
}




void Viewport::set_highlight_color(char const * colorname)
{
	assert (this->highlight_gc);
	this->highlight_color.setNamedColor(colorname);
	this->highlight_gc->setColor(this->highlight_color);
}




void Viewport::set_highlight_gdkcolor(QColor * color)
{
	assert (this->highlight_gc);
	this->highlight_color = *color;
	this->highlight_gc->setColor(*color);
}




QPen * Viewport::get_gc_highlight()
{
	return highlight_gc;
}




void Viewport::set_highlight_thickness(int width)
{
	/* Otherwise same GDK_* attributes as in Viewport::new_pen(). */
	this->highlight_gc->setWidth(width);
	// GDK_LINE_SOLID
	// GDK_CAP_ROUND
	// GDK_JOIN_ROUND
}




QPen * Viewport::new_pen(char const * colorname, int width)
{
	QColor color;

	QPen * pen = new QPen(colorname);
	pen->setWidth(width);
	// GDK_LINE_SOLID
	// GDK_CAP_ROUND
	// GDK_JOIN_ROUND

	return pen;
}



GdkGC * Viewport::new_gc(char const * colorname, int thickness)
{
#ifndef SLAVGPS_QT
	GdkColor color;

	GdkGC * rv = gdk_gc_new(GTK_WIDGET(this->drawing_area_)->window);
	if (gdk_color_parse(colorname, &color)) {
		gdk_gc_set_rgb_fg_color(rv, &color);
	} else {
		fprintf(stderr, "WARNING: %s: Failed to parse color '%s'\n", __FUNCTION__, colorname);
	}
	gdk_gc_set_line_attributes(rv, thickness, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND);
	return rv;
#endif
}




GdkGC * Viewport::new_gc_from_color(GdkColor * color, int thickness)
{
#ifndef SLAVGPS_QT
	GdkGC * rv = gdk_gc_new(gtk_widget_get_window(GTK_WIDGET(this->drawing_area_)));
	gdk_gc_set_rgb_fg_color(rv, color);
	gdk_gc_set_line_attributes(rv, thickness, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND);
	return rv;
#endif
}



QPen * Viewport::new_pen_from_color(const QColor & color, int width)
{
	QPen * pen = new QPen(color);
	pen->setWidth(width);
	// GDK_LINE_SOLID
	// GDK_CAP_ROUND
	// GDK_JOIN_ROUND

	return pen;
}




void Viewport::configure_manually(int width_, unsigned int height_)
{
	this->size_width = width_;
	this->size_height = height_;

	this->size_width_2 = this->size_width / 2;
	this->size_height_2 = this->size_height / 2;

	if (this->scr_buffer) {
		qDebug() << "II: Viewport: deleting old scr_buffer";
		delete this->scr_buffer;
	}

	qDebug() << "II: Viewport creating new scr_buffer with size" << this->size_width << this->size_height;
	this->scr_buffer = new QPixmap(this->size_width, this->size_height);
	this->scr_buffer->fill();

	/* TODO trigger: only if this is enabled!!! */
	if (this->snapshot_buffer) {
		qDebug() << "DD: Viewport: deleting old snapshot buffer";
		delete this->snapshot_buffer;
	}
	qDebug() << "II: Viewport creating new snapshot buffer with size" << this->size_width << this->size_height;
	this->snapshot_buffer = new QPixmap(this->size_width, this->size_height);
}




QPixmap * Viewport::get_pixmap()
{
	return this->scr_buffer;
}




bool Viewport::configure_cb(void)
{
	qDebug() << "II: Viewport: handling signal \"configure event\"";
	return this->configure();
}




bool Viewport::configure()
{
	const QRect geom = this->geometry();
	this->size_width = geom.width();
	this->size_height = geom.height();

	this->size_width_2 = this->size_width / 2;
	this->size_height_2 = this->size_height / 2;

	if (this->scr_buffer) {
		qDebug() << "II: deleting old scr_buffer" << __FUNCTION__ << __LINE__;
		delete this->scr_buffer;
	}

	qDebug() << "II: Viewport creating new scr_buffer with size" << this->size_width << this->size_height << __FUNCTION__ << __LINE__;
	this->scr_buffer = new QPixmap(this->size_width, this->size_height);
	this->scr_buffer->fill();

	this->pen_marks_fg.setColor(QColor("grey"));
	this->pen_marks_fg.setWidth(2);
	this->pen_marks_bg.setColor(QColor("pink"));
	this->pen_marks_bg.setWidth(6);


	/* TODO trigger: only if enabled! */
	if (this->snapshot_buffer) {
		qDebug() << "DD: Viewport: deleting old snapshot buffer";
		delete this->snapshot_buffer;
	}
	qDebug() << "II: Viewport creating new snapshot buffer with size" << this->size_width << this->size_height;
	this->snapshot_buffer = new QPixmap(this->size_width, this->size_height);
	/* TODO trigger. */


	/* This is down here so it can get a GC (necessary?). */
	if (!this->background_gc) {
		this->background_gc = this->new_pen(DEFAULT_BACKGROUND_COLOR, 1);
		this->set_background_color(DEFAULT_BACKGROUND_COLOR);
	}
	if (!this->highlight_gc) {
		this->highlight_gc = this->new_pen(DEFAULT_HIGHLIGHT_COLOR, 1);
		this->set_highlight_color(DEFAULT_HIGHLIGHT_COLOR);
	}

	return false;
}




/**
 * Clear the whole viewport.
 */
void Viewport::clear()
{
	qDebug() << "II: Viewport: clear whole viewport" << __FUNCTION__ << __LINE__;
	QPainter painter(this->scr_buffer);
	painter.eraseRect(0, 0, this->size_width, this->size_height);

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
	//fprintf(stderr, "%s:%d: %d / %d / %d\n", __FUNCTION__, __LINE__, (int) *base_distance, (int) *scale_unit, maximum_width);

	int n = 0;
	if (ratio > 1) {
		n = (int) floor(log10(ratio));
	} else {
		n = (int) floor(log10(1.0 / ratio));
	}

	//fprintf(stderr, "%s:%d: ratio = %f, n = %d\n", __FUNCTION__, __LINE__, ratio, n);

	*scale_unit = pow(10.0, n); /* scale_unit is still a unit (1 km, 10 miles, 100 km, etc. ), only 10^n times larger. */
	ratio = *base_distance / *scale_unit;
	double len = maximum_width / ratio; /* [px] */


	//fprintf(stderr, "%s:%d: len = %f\n", __FUNCTION__, __LINE__, len);
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
	//fprintf(stderr, "rescale unit len = %g\n", len);

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
		qDebug() << "EE: Viewport: failed to get correct units of distance, got" << (int) distance_unit;
	}

	/* At this point "base_distance" is a distance between "left" and "right" in physical units.
	   But a scale can't have an arbitrary length (e.g. 3.07 miles, or 23.2 km),
	   it should be something like 1.00 mile or 10.00 km - a unit. */
	double scale_unit = 1; /* [km, miles, nautical miles] */

	//fprintf(stderr, "%s:%d: base_distance = %g, scale_unit = %g, MAXIMUM_WIDTH = %d\n", __FUNCTION__, __LINE__, base_distance, scale_unit, MAXIMUM_WIDTH);
	int len = rescale_unit(&base_distance, &scale_unit, MAXIMUM_WIDTH);
	//fprintf(stderr, "resolved len = %d\n", len);


	const QPen & pen_fg = this->pen_marks_fg;
	const QPen & pen_bg = this->pen_marks_bg;

	this->draw_scale_helper_scale(pen_bg, len, HEIGHT); /* Bright background. */
	this->draw_scale_helper_scale(pen_fg, len, HEIGHT); /* Darker scale on the bright background. */

	char s[128];
	this->draw_scale_helper_value(s, distance_unit, scale_unit);


	QString text(s);
	QPainter painter(this->scr_buffer);

	QPointF scale_start(PAD, this->size_height - PAD); /* Bottom-left corner of scale. */
	QPointF value_start = QPointF(scale_start.x() + len + PAD, scale_start.y()); /* Bottom-left corner of value. */

#if 1
	/* Debug. */
	painter.setPen(QColor("red"));
	painter.drawEllipse(scale_start, 3, 3);
	painter.setPen(QColor("blue"));
	painter.drawEllipse(value_start, 3, 3);
#endif

	painter.setFont(QFont("Helvetica", 40));

	QRectF input_rect = QRectF((int) value_start.x(), 0, (int) value_start.x() + 1000, (int) value_start.y());
	QRectF text_rect = painter.boundingRect(input_rect, Qt::AlignBottom, text);
	QRectF margins_rect(text_rect.x() - 2, text_rect.y() - 2, text_rect.width() + 4, text_rect.height() + 4);
	painter.fillRect(margins_rect, pen_bg.color());

#if 1
	/* Debug. */
	painter.setPen(QColor("orange"));
	painter.drawRect(input_rect);
	painter.setPen(QColor("red"));
	painter.drawRect(text_rect);
#endif

	painter.setPen(pen_fg);
	painter.drawText(PAD + len + PAD, this->size_height - PAD - 10, text);

	this->repaint();
}




void Viewport::draw_scale_helper_scale(const QPen & pen, int scale_len, int h)
{
	/* Black scale. */
	this->draw_line(pen, PAD,             this->size_height - PAD, PAD + scale_len, this->size_height - PAD);
	this->draw_line(pen, PAD,             this->size_height - PAD, PAD,             this->size_height - PAD - h);
	this->draw_line(pen, PAD + scale_len, this->size_height - PAD, PAD + scale_len, this->size_height - PAD - h);

	int y1 = this->size_height - PAD;
	for (int i = 1; i < 10; i++) {
		int x1 = PAD + i * scale_len / 10;
		int diff = ((i == 5) ? (2 * h / 3) : (1 * h / 3));
		this->draw_line(pen, x1, y1, x1, y1 - diff);
	}
}




void Viewport::draw_scale_helper_value(char * s, DistanceUnit distance_unit, double scale_unit)
{
	switch (distance_unit) {
	case DistanceUnit::KILOMETRES:
		if (scale_unit >= 1000) {
			sprintf(s, "y%d km", (int) scale_unit / 1000);
		} else {
			sprintf(s, "y%d m", (int) scale_unit);
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
		qDebug() << "EE: Viewport: failed to get correct units of distance, got" << (int) distance_unit;
	}
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

#ifndef SLAVGPS_QT
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
#endif
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
	qDebug() << "II: Viewport: draw centermark:" << do_draw_centermark;

	if (!do_draw_centermark) {
		return;
	}

	const int len = 30;
	const int gap = 4;
	int center_x = this->size_width / 2;
	int center_y = this->size_height / 2;

	const QPen & pen_fg = this->pen_marks_fg;
	const QPen & pen_bg = this->pen_marks_bg;

	/* White background. */
	this->draw_line(pen_bg, center_x - len, center_y,       center_x - gap, center_y);
	this->draw_line(pen_bg, center_x + gap, center_y,       center_x + len, center_y);
	this->draw_line(pen_bg, center_x,       center_y - len, center_x,       center_y - gap);
	this->draw_line(pen_bg, center_x,       center_y + gap, center_x,       center_y + len);

	/* Black foreground. */
	this->draw_line(pen_fg, center_x - len, center_y,        center_x - gap, center_y);
	this->draw_line(pen_fg, center_x + gap, center_y,        center_x + len, center_y);
	this->draw_line(pen_fg, center_x,       center_y - len,  center_x,       center_y - gap);
	this->draw_line(pen_fg, center_x,       center_y + gap,  center_x,       center_y + len);

	this->update();
}




void Viewport::draw_logo()
{
#ifndef SLAVGPS_QT
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
#endif
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
	qDebug() << "II: Viewport: ->sync() (will call ->render())" << __FUNCTION__ << __LINE__;
	//gdk_draw_drawable(gtk_widget_get_window(GTK_WIDGET(this->drawing_area_)), gtk_widget_get_style(GTK_WIDGET(this->drawing_area_))->bg_gc[0], GDK_DRAWABLE(this->scr_buffer), 0, 0, 0, 0, this->size_width, this->size_height);
	this->render(this->scr_buffer);
}




void Viewport::pan_sync(int x_off, int y_off)
{
	qDebug() << "II: Viewport: Pan Sync";
#ifndef SLAVGPS_QT
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
#endif
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

	//fprintf(stderr, "VIEWPORT: emitting updated_center() (%s:%d)\n", __FUNCTION__, __LINE__);
	emit this->updated_center();
}




/**
 * Show the list of forward/backward positions.
 * ATM only for debug usage.
 */
void Viewport::show_centers(GtkWindow * parent)
{
#ifndef SLAVGPS_QT
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
#endif
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

		qDebug() << "II: Viewport: centers" << label << lat << lon << extra;

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
		qDebug() << "WW: Viewport: Have to convert in Viewport::coord_to_screen()! This should never happen!";
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




void Viewport::draw_line(const QPen & pen, int x1, int y1, int x2, int y2)
{
	//fprintf(stderr, "Called to draw line between points (%d %d) and (%d %d)\n", x1, y1, x2, y2);
	if (! ((x1 < 0 && x2 < 0) || (y1 < 0 && y2 < 0)
	       || (x1 > this->size_width && x2 > this->size_width)
	       || (y1 > this->size_height && y2 > this->size_height))) {

		/*** Clipping, yeah! ***/
		Viewport::clip_line(&x1, &y1, &x2, &y2);

		QPainter painter(this->scr_buffer);
		painter.setPen(pen);
		painter.drawLine(x1, y1, x2, y2);
	}
}




void Viewport::draw_rectangle(const QPen & pen, bool filled, int x, int y, int width, int height)
{
	/* Using 32 as half the default waypoint image size, so this draws ensures the highlight gets done. */
	if (x > -32 && x < this->size_width + 32 && y > -32 && y < this->size_height + 32) {

		QPainter painter(this->scr_buffer);
		painter.setPen(pen);
		painter.drawRect(x, y, width, height);
	}
}




void Viewport::fill_rectangle(const QColor & color, int x, int y, int width, int height)
{
	/* Using 32 as half the default waypoint image size, so this draws ensures the highlight gets done. */
	if (x > -32 && x < this->size_width + 32 && y > -32 && y < this->size_height + 32) {

		QPainter painter(this->scr_buffer);
		painter.fillRect(x, y, width, height, color);
	}
}




void Viewport::draw_string(GdkFont * font, GdkGC * gc, int x1, int y1, const char *string)
{
	if (x1 > -100 && x1 < this->size_width + 100 && y1 > -100 && y1 < this->size_height + 100) {
		//this->qpainter->drawText(x1, y1, string);
		//gdk_draw_string(this->scr_buffer, font, gc, x1, y1, string);
	}
}




void Viewport::draw_pixbuf(GdkPixbuf *pixbuf, int src_x, int src_y,
			   int dest_x, int dest_y, int region_width, int region_height)
{
#if 0
	gdk_draw_pixbuf(this->scr_buffer,
			NULL,
			pixbuf,
			src_x, src_y, dest_x, dest_y, region_width, region_height,
			GDK_RGB_DITHER_NONE, 0, 0);
#endif
}




void Viewport::draw_arc(GdkGC * gc, bool filled, int x, int y, int width, int height, int angle1, int angle2)
{
	//this->qpainter->drawArc(x, y, width, height, angle1, angle2);
}





void Viewport::draw_polygon(GdkGC * gc, bool filled, QPointF * points, int npoints)
{
	//this->qpainter->drawPoints(points, npoints);
	//gdk_draw_polygon(this->scr_buffer, gc, filled, points, npoints);
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
#ifndef SLAVGPS_QT
	if (x > -100 && x < this->size_width + 100 && y > -100 && y < this->size_height + 100) {
		gdk_draw_layout(this->scr_buffer, gc, x, y, layout);
	}
#endif
}




void vik_gc_get_fg_color(GdkGC * gc, GdkColor * dest)
{
#ifndef SLAVGPS_QT
	static GdkGCValues values;
	gdk_gc_get_values(gc, &values);
	gdk_colormap_query_color(gdk_colormap_get_system(), values.foreground.pixel, dest);
#endif
}




GdkFunction vik_gc_get_function(GdkGC * gc)
{
#ifndef SLAVGPS_QT
	static GdkGCValues values;
	gdk_gc_get_values (gc, &values);
	return values.function;
#endif
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
	qDebug() << "II: Viewport: save snapshot";
	*this->snapshot_buffer = *this->scr_buffer;
#ifndef SLAVGPS_QT
	gdk_draw_drawable(this->snapshot_buffer, this->background_gc, this->scr_buffer, 0, 0, 0, 0, -1, -1);
#endif
}




void Viewport::snapshot_load()
{
	qDebug() << "II: Viewport: load snapshot";
	*this->scr_buffer = *this->snapshot_buffer;
#ifndef SLAVGPS_QT
	gdk_draw_drawable(this->scr_buffer, this->background_gc, this->snapshot_buffer, 0, 0, 0, 0, -1, -1);
#endif
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
#ifdef SLAVGPS_QT
	return NULL;
#else
	GtkWidget * mode_button = this->get_window()->get_drawmode_button(mode);
	GtkWidget * label = gtk_bin_get_child(GTK_BIN(mode_button));

	return gtk_label_get_text(GTK_LABEL(label));

#endif
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
#ifndef SLAVGPS_QT
	if (logo_) {
		GdkPixbuf * found = NULL; /* FIXME (GdkPixbuf*)g_slist_find_custom (vp->port.logos, logo, (GCompareFunc)==); */
		if (found == NULL) {
			logos = g_slist_prepend(logos, (void *) logo_);
		}
	}
#endif
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
#ifdef SLAVGPS_QT
	return NULL;
#else
	return GTK_WIDGET(this->drawing_area_);
#endif
}




GtkWindow * Viewport::get_toolkit_window(void)
{
#ifdef SLAVGPS_QT
	return NULL;
#else
	return GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(this->drawing_area_)));
#endif
}




void * Viewport::get_toolkit_object(void)
{
#ifdef SLAVGPS_QT
	return NULL;
#else
	return (void *) this->drawing_area_;
#endif
}




Window * Viewport::get_window(void)
{
	return this->window;
}




void Viewport::paintEvent(QPaintEvent *event)
{
	qDebug() << "II: Viewport: paintEvent()" << __FUNCTION__ << __LINE__;

	QPainter painter(this);

	painter.drawPixmap(0, 0, *this->scr_buffer);

	painter.setPen(Qt::blue);
	painter.setFont(QFont("Arial", 30));
	painter.drawText(this->rect(), Qt::AlignCenter, "Qt");

	return;
}




void Viewport::resizeEvent(QResizeEvent * event)
{
	qDebug() << "II: Viewport: resize event";
	this->configure();
	this->get_window()->draw_redraw();
	//this->draw_scale();

	return;
}





void Viewport::mousePressEvent(QMouseEvent * event)
{
	qDebug() << "II: Viewport: mouse press event, button" << (int) event->button();

	this->window->get_layer_tools_box()->click(event);

	event->accept();
}



void Viewport::mouseMoveEvent(QMouseEvent * event)
{
	this->draw_mouse_motion_cb(event);

	if (event->buttons() != Qt::NoButton) {
		// qDebug() << "II: Viewport: mouse move with buttons";
		this->window->get_layer_tools_box()->move(event);
	} else {
		//qDebug() << "II: Viewport: mouse move without buttons";
	}

	event->accept();
}


void Viewport::mouseReleaseEvent(QMouseEvent * event)
{
	qDebug() << "II: Viewport: mouse release event, button" << (int) event->button();

	this->window->get_layer_tools_box()->release(event);

	event->accept();
}




void Viewport::wheelEvent(QWheelEvent * event)
{
	QPoint angle = event->angleDelta();
	qDebug() << "II: Viewport: wheel event, buttons =" << (int) event->buttons() << "angle =" << angle.y();
	event->accept();

	const Qt::KeyboardModifiers modifiers = event->modifiers(); // (GDK_SHIFT_MASK | GDK_CONTROL_MASK);

	const int w = this->get_width();
	const int h = this->get_height();
	const bool scroll_up = angle.y() > 0;

	if (modifiers == Qt::ControlModifier) {
		/* Control == pan up & down. */
		if (scroll_up) {
			this->set_center_screen(w / 2, h / 3);
		} else {
			this->set_center_screen(w / 2, h * 2 / 3);
		}
	} else if (modifiers == Qt::ShiftModifier) {
		/* Shift == pan left & right. */
		if (scroll_up) {
			this->set_center_screen(w / 3, h / 2);
		} else {
			this->set_center_screen(w * 2 / 3, h / 2);
		}
	} else if (modifiers == (Qt::ControlModifier | Qt::ShiftModifier)) {
		/* This zoom is on the center position. */
		if (scroll_up) {
			this->zoom_in();
		} else {
			this->zoom_out();
		}
	} else {
		/* Make sure mouse is still over the same point on the map when we zoom. */
		VikCoord coord;
		int x, y;
		int center_x = w / 2;
		int center_y = h / 2;
		this->screen_to_coord(event->x(), event->y(), &coord);
		if (scroll_up) {
			this->zoom_in();
		} else {
			this->zoom_out();
		}
		this->coord_to_screen(&coord, &x, &y);
		this->set_center_screen(center_x + (x - event->x()),
					center_y + (y - event->y()));
	}

	qDebug() << "II: Viewport: wheel event, call Window::draw_update()" << __FUNCTION__ << __LINE__;
	this->window->draw_update();
}




void Viewport::draw_mouse_motion_cb(QMouseEvent * event)
{
#define BUFFER_SIZE 50
	QPoint position = this->mapFromGlobal(QCursor::pos());

#if 0   /* Verbose debug. */
	qDebug() << "II: Viewport: difference in cursor position: dx = " << position.x() - event->x() << ", dy = " << position.y() - event->y();
#endif

	int x = position.x();
	int y = position.y();

	//this->window->tb->move(event); /* TODO: uncomment this. */

	static VikCoord coord;
	static struct UTM utm;
	this->screen_to_coord(x, y, &coord);
	vik_coord_to_utm(&coord, &utm);

	char * lat = NULL;
	char * lon = NULL;
	this->get_location_strings(utm, &lat, &lon);

	/* Change interpolate method according to scale. */
	double zoom = this->get_zoom();
	VikDemInterpol interpol_method;
	if (zoom > 2.0) {
		interpol_method = VIK_DEM_INTERPOL_NONE;
	} else if (zoom >= 1.0) {
		interpol_method = VIK_DEM_INTERPOL_SIMPLE;
	} else {
		interpol_method = VIK_DEM_INTERPOL_BEST;
	}

	int16_t alt;
	static char pointer_buf[BUFFER_SIZE] = { 0 };
	if ((alt = dem_cache_get_elev_by_coord(&coord, interpol_method)) != VIK_DEM_INVALID_ELEVATION) {
		if (a_vik_get_units_height() == HeightUnit::METRES) {
			snprintf(pointer_buf, BUFFER_SIZE, "%s %s %dm", lat, lon, alt);
		} else {
			snprintf(pointer_buf, BUFFER_SIZE, "%s %s %dft", lat, lon, (int) VIK_METERS_TO_FEET(alt));
		}
	} else {
		snprintf(pointer_buf, BUFFER_SIZE, "%s %s", lat, lon);
	}
	free(lat);
	lat = NULL;
	free(lon);
	lon = NULL;
	QString message(pointer_buf);
	this->window->status_bar->set_message(StatusBarField::POSITION, message);

	//this->window->pan_move(event); /* TODO: uncomment this. */
#undef BUFFER_SIZE
}




/**
 * Utility function to get positional strings for the given location
 * lat and lon strings will get allocated and so need to be freed after use
 */
void Viewport::get_location_strings(struct UTM utm, char **lat, char **lon)
{
	if (this->get_drawmode() == VIK_VIEWPORT_DRAWMODE_UTM) {
		// Reuse lat for the first part (Zone + N or S, and lon for the second part (easting and northing) of a UTM format:
		//  ZONE[N|S] EASTING NORTHING
		*lat = (char *) malloc(4*sizeof(char));
		// NB zone is stored in a char but is an actual number
		snprintf(*lat, 4, "%d%c", utm.zone, utm.letter);
		*lon = (char *) malloc(16*sizeof(char));
		snprintf(*lon, 16, "%d %d", (int)utm.easting, (int)utm.northing);
	} else {
		struct LatLon ll;
		a_coords_utm_to_latlon(&utm, &ll);
		a_coords_latlon_to_string(&ll, lat, lon);
	}
}
