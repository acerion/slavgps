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
#include <QDebug>
#include <QPainter>

#include "generic_tools.h"
#include "viewport.h"
#include "viewport_internal.h"
#include "viewport_zoom.h"
#include "window.h"
#include "coords.h"
#include "dem_cache.h"
#include "mapcoord.h"
#include "toolbox.h"
#include "preferences.h"
#include "widget_list_selection.h"

/* For ALTI_TO_MPP. */
#include "globals.h"
#include "application_state.h"
#include "dialog.h"




using namespace SlavGPS;




#define DEFAULT_BACKGROUND_COLOR "#CCCCCC"
#define DEFAULT_HIGHLIGHT_COLOR "#EEA500"
/* Default highlight in orange. */




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
}




double Viewport::calculate_utm_zone_width()
{
	if (coord_mode == CoordMode::UTM) {
		struct LatLon ll;

		/* Get latitude of screen bottom. */
		struct UTM utm = this->center.utm;
		utm.northing -= this->size_height * ympp / 2;
		a_coords_utm_to_latlon(&ll, &utm);

		/* Boundary. */
		ll.lon = (utm.zone - 1) * 6 - 180 ;
		a_coords_latlon_to_utm(&utm, &ll);
		return fabs(utm.easting - EASTING_OFFSET) * 2;
	} else {
		return 0.0;
	}
}




const QColor & Viewport::get_background_color(void) const
{
	return this->background_color;
}




Viewport::Viewport(Window * parent_window) : QWidget((QWidget *) parent_window)
{
	this->window = parent_window;

	this->installEventFilter(this);

	this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	this->setMinimumSize(200, 300);
	//this->setMaximumSize(2700, 2700);


	struct UTM utm;
	struct LatLon ll;
	ll.lat = Preferences::get_default_lat();
	ll.lon = Preferences::get_default_lon();
	double zoom_x = 4.0;
	double zoom_y = 4.0;

	if (Preferences::get_startup_method() == VIK_STARTUP_METHOD_LAST_LOCATION) {
		double value;
		if (ApplicationState::get_double(VIK_SETTINGS_VIEW_LAST_LATITUDE, &value)) {
			ll.lat = value;
		}

		if (ApplicationState::get_double(VIK_SETTINGS_VIEW_LAST_LONGITUDE, &value)) {
			ll.lon = value;
		}

		if (ApplicationState::get_double(VIK_SETTINGS_VIEW_LAST_ZOOM_X, &value)) {
			zoom_x = value;
		}

		if (ApplicationState::get_double(VIK_SETTINGS_VIEW_LAST_ZOOM_Y, &value)) {
			zoom_y = value;
		}
	}

	a_coords_latlon_to_utm(&utm, &ll);

	xmpp = zoom_x;
	ympp = zoom_y;
	xmfactor = MERCATOR_FACTOR(xmpp);
	ymfactor = MERCATOR_FACTOR(ympp);
	coord_mode = CoordMode::LATLON;
	drawmode = ViewportDrawMode::MERCATOR;
	center.mode = CoordMode::LATLON;
	center.ll.lat = ll.lat;
	center.ll.lon = ll.lon;
	center.utm.zone = (int) utm.zone; /* kamilTODO: why do we assign utm values when mode is CoordMode::LATLON? */
	center.utm.letter = utm.letter;
	utm_zone_width = 0.0;

	centers = new std::list<Coord *>;
	centers_iter = centers->begin();
	centers_max = 20;
	int tmp = centers_max;
	if (ApplicationState::get_integer(VIK_SETTINGS_VIEW_HISTORY_SIZE, &tmp)) {
		centers_max = tmp;
	}

	centers_radius = 500;
	if (ApplicationState::get_integer(VIK_SETTINGS_VIEW_HISTORY_DIFF_DIST, &tmp)) {
		centers_radius = tmp;
	}

	this->init_drawing_area();

	/* We want to constantly update cursor position in status bar. For this we need cursor tracking in viewport. */
	this->setMouseTracking(true);

	/* Initiate center history. */
	update_centers();

	strncpy(this->type_string, "Le Viewport", (sizeof (this->type_string)) - 1);
	this->scale_visibility = true;

	this->border_pen.setColor(QColor("black"));
	this->border_pen.setWidth(2);

	this->marker_pen.setColor(QColor("brown"));
	this->marker_pen.setWidth(1);

	this->grid_pen.setColor(QColor("dimgray"));
	this->grid_pen.setWidth(1);
}




Viewport::~Viewport()
{
	qDebug() << "II: Viewport: ~Viewport called";
	if (Preferences::get_startup_method() == VIK_STARTUP_METHOD_LAST_LOCATION) {
		struct LatLon ll = this->center.get_latlon();
		ApplicationState::set_double(VIK_SETTINGS_VIEW_LAST_LATITUDE, ll.lat);
		ApplicationState::set_double(VIK_SETTINGS_VIEW_LAST_LONGITUDE, ll.lon);
		ApplicationState::set_double(VIK_SETTINGS_VIEW_LAST_ZOOM_X, this->xmpp);
		ApplicationState::set_double(VIK_SETTINGS_VIEW_LAST_ZOOM_Y, this->ympp);
	}

	delete this->centers;
	delete this->scr_buffer;
	delete this->snapshot_buffer;
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
	this->background_pen.setColor(this->background_color);
}




void Viewport::set_background_color(const QColor & color)
{
	this->background_color = color;
	this->background_pen.setColor(color);
}




const QColor & Viewport::get_highlight_color(void) const
{
	return this->highlight_color;
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
	this->highlight_color.setNamedColor(colorname);
	this->highlight_pen.setColor(this->highlight_color);
}




void Viewport::set_highlight_color(const QColor & color)
{
	this->highlight_color = color;
	this->highlight_pen.setColor(color);
}




QPen Viewport::get_highlight_pen()
{
	return highlight_pen;
}




void Viewport::set_highlight_thickness(int w)
{
	this->highlight_pen.setWidth(w);
	// GDK_LINE_SOLID
	// GDK_CAP_ROUND
	// GDK_JOIN_ROUND
}




QPen * Viewport::new_pen(char const * colorname, int w)
{
	QPen * pen = new QPen(colorname);
	pen->setWidth(w);
	// GDK_LINE_SOLID
	// GDK_CAP_ROUND
	// GDK_JOIN_ROUND

	return pen;
}



QPen * Viewport::new_pen(const QColor & color, int w)
{
	QPen * pen = new QPen(color);
	pen->setWidth(w);
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

	qDebug() << "SIGNAL: Viewport: sending \"reconfigured\" from" << this->type_string <<  __FUNCTION__;
	emit this->reconfigured(this);
}




QPixmap * Viewport::get_pixmap()
{
	return this->scr_buffer;
}




void Viewport::set_pixmap(QPixmap & pixmap)
{
	QPainter painter(this->scr_buffer);
	/* TODO: Add some comparison of pixmap size and buffer size to verify that both have the same size and that pixmap can be safely used. */
	painter.drawPixmap(0, 0, pixmap, 0, 0, 0, 0);
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

	this->background_pen.setColor(DEFAULT_BACKGROUND_COLOR);
	this->background_pen.setWidth(1);
	this->set_background_color(DEFAULT_BACKGROUND_COLOR);

	this->highlight_pen.setColor(DEFAULT_HIGHLIGHT_COLOR);
	this->highlight_pen.setWidth(1);
	this->set_highlight_color(DEFAULT_HIGHLIGHT_COLOR);

	qDebug() << "SIGNAL: Viewport: sending \"reconfigured\" from" << this->type_string << __FUNCTION__;
	emit this->reconfigured(this);

	return false;
}




/**
 * Clear the whole viewport.
 */
void Viewport::clear()
{
	qDebug() << "II: Viewport: clear whole viewport" << this->type_string << __FUNCTION__ << __LINE__;
	QPainter painter(this->scr_buffer);
	painter.eraseRect(0, 0, this->size_width, this->size_height);

	this->reset_copyrights();
	this->reset_logos();
}




/**
 * Enable/Disable display of scale.
 */
void Viewport::set_scale_visibility(bool new_state)
{
	this->scale_visibility = new_state;
}




bool Viewport::get_scale_visibility()
{
	return this->scale_visibility;
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
	if (!this->scale_visibility) {
		return;
	}

	double base_distance;       /* Physical (real world) distance corresponding to full width of drawn scale. Physical units (miles, meters). */
	int HEIGHT = 20;            /* Height of scale in pixels. */
	float RELATIVE_WIDTH = 0.5; /* Width of scale, relative to width of viewport. */
	int MAXIMUM_WIDTH = this->size_width * RELATIVE_WIDTH;

	Coord left = this->screen_to_coord(0,                                  this->size_height / 2);
	Coord right = this->screen_to_coord(this->size_width * RELATIVE_WIDTH, this->size_height / 2);

	DistanceUnit distance_unit = Preferences::get_unit_distance();
	switch (distance_unit) {
	case DistanceUnit::KILOMETRES:
		base_distance = Coord::distance(left, right); /* In meters. */
		break;
	case DistanceUnit::MILES:
		/* In 0.1 miles (copes better when zoomed in as 1 mile can be too big). */
		base_distance = VIK_METERS_TO_MILES (Coord::distance(left, right)) * 10.0;
		break;
	case DistanceUnit::NAUTICAL_MILES:
		/* In 0.1 NM (copes better when zoomed in as 1 NM can be too big). */
		base_distance = VIK_METERS_TO_NAUTICAL_MILES (Coord::distance(left, right)) * 10.0;
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

	const QString scale_value = this->draw_scale_helper_value(distance_unit, scale_unit);


	QPointF scale_start(PAD, this->size_height - PAD); /* Bottom-left corner of scale. */
	QPointF value_start = QPointF(scale_start.x() + len + PAD, scale_start.y()); /* Bottom-left corner of value. */
	QRectF bounding_rect = QRectF((int) value_start.x(), 0, (int) value_start.x() + 300, (int) value_start.y());
	this->draw_text(QFont("Helvetica", 40), pen_fg, bounding_rect, Qt::AlignBottom | Qt::AlignLeft, scale_value, 0);
	/* TODO: we need to draw background of the text in some color,
	   so that it's more visible on a map that will be present in the background. */

#if 1
	/* Debug. */
	QPainter painter(this->scr_buffer);
	painter.setPen(QColor("red"));
	painter.drawEllipse(scale_start, 3, 3);
	painter.setPen(QColor("blue"));
	painter.drawEllipse(value_start, 3, 3);
#endif

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




QString Viewport::draw_scale_helper_value(DistanceUnit distance_unit, double scale_unit)
{
	QString scale_value;

	switch (distance_unit) {
	case DistanceUnit::KILOMETRES:
		if (scale_unit >= 1000) {
			scale_value = tr("y%1 km").arg((int) scale_unit / 1000);
		} else {
			scale_value = tr("y%1 m").arg((int) scale_unit);
		}
		break;
	case DistanceUnit::MILES:
		/* Handle units in 0.1 miles. */
		if (scale_unit < 10.0) {
			scale_value = tr("%1 miles").arg(scale_unit / 10.0, 0, 'f', 1); /* "%0.1f" */
		} else if ((int) scale_unit == 10.0) {
			scale_value = tr("1 mile");
		} else {
			scale_value = tr("%1 miles").arg((int) (scale_unit / 10.0));
		}
		break;
	case DistanceUnit::NAUTICAL_MILES:
		/* Handle units in 0.1 NM. */
		if (scale_unit < 10.0) {
			scale_value = tr("%1 NM").arg(scale_unit / 10.0, 0, 'f', 1); /* "%0.1f" */
		} else if ((int) scale_unit == 10.0) {
			scale_value = tr("1 NM");
		} else {
			scale_value = tr("%1 NMs").arg((int) (scale_unit / 10.0));
		}
		break;
	default:
		qDebug() << "EE: Viewport: failed to get correct units of distance, got" << (int) distance_unit;
	}

	return scale_value;
}




void Viewport::draw_copyrights(void)
{
	/* TODO: how to ensure that these 128 chars fit into bounding rectangle used below? */
#define MAX_COPYRIGHTS_LEN 128
	QString result;
	int free_space = MAX_COPYRIGHTS_LEN;

#if 1
	this->copyrights << "test copyright 1";
	this->copyrights << "another test copyright";
#endif

	/* Compute copyrights string. */

	unsigned int len = this->copyrights.size();
	for (unsigned int i = 0 ; i < len ; i++) {

		if (free_space < 0) {
			break;
		}

		QString const & copyright = copyrights[i];

		/* Only use part of this copyright that fits in the available space left,
		   remembering 1 character is left available for the appended space. */

		result.append(copyright.left(free_space));
		free_space -= copyright.size();

		result.append(" ");
		free_space--;
	}

	/* Copyright text will be in bottom-right corner. */
	/* Use no more than half of width of viewport. */
	int x_size = 0.5 * this->size_width;
	int y_size = 0.7 * this->size_height;
	int w_size = this->size_width - x_size - PAD;
	int h_size = this->size_height - y_size - PAD;

	QPointF box_start = QPointF(this->size_width - PAD, this->size_height - PAD); /* Anchor in bottom-right corner. */
	QRectF bounding_rect = QRectF(box_start.x(), box_start.y(), -w_size, -h_size);
	this->draw_text(QFont("Helvetica", 12), this->pen_marks_fg, bounding_rect, Qt::AlignBottom | Qt::AlignRight, result, 0);

#undef MAX_COPYRIGHTS_LEN
}




/**
 * Enable/Disable display of center mark.
 */
void Viewport::set_center_mark_visibility(bool new_state)
{
	this->center_mark_visibility = new_state;
}




bool Viewport::get_center_mark_visibility()
{
	return this->center_mark_visibility;
}




void Viewport::draw_centermark()
{
	qDebug() << "II: Viewport: draw centermark:" << this->center_mark_visibility;

	if (!this->center_mark_visibility) {
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
	int x_size = this->size_width - PAD;
	int y_size = PAD;
	for (auto iter = this->logos.begin(); iter != this->logos.end(); iter++) {
		QPixmap const * logo = *iter;
		int w_size = logo->width();
		int h_size = logo->height();
		this->draw_pixmap(*logo, 0, 0, x_size - w_size, y_size, w_size, h_size);
		x_size = x_size - w_size - PAD;
	}
}




void Viewport::set_highlight_usage(bool new_state)
{
	this->highlight_usage = new_state;
}




bool Viewport::get_highlight_usage()
{
	return this->highlight_usage;
}




void Viewport::sync()
{
	qDebug() << "II: Viewport: ->sync() (will call ->render())" << __FUNCTION__ << __LINE__;
	//gdk_draw_drawable(gtk_widget_get_window(GTK_WIDGET(this)), gtk_widget_get_style(GTK_WIDGET(this))->bg_gc[0], GDK_DRAWABLE(this->scr_buffer), 0, 0, 0, 0, this->size_width, this->size_height);
	this->render(this->scr_buffer);
}




void Viewport::pan_sync(int x_off, int y_off)
{
	qDebug() << "II: Viewport: Pan Sync";
#ifdef K
	int x, y, wid, hei;

	gdk_draw_drawable(gtk_widget_get_window(GTK_WIDGET(this)), gtk_widget_get_style(GTK_WIDGET(this))->bg_gc[0], GDK_DRAWABLE(this->scr_buffer), 0, 0, x_off, y_off, this->size_width, this->size_height);

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
	gtk_widget_queue_draw_area(GTK_WIDGET(this), x, 0, wid, this->size_height);
	gtk_widget_queue_draw_area(GTK_WIDGET(this), 0, y, this->size_width, hei);
#endif
}




void Viewport::set_zoom(double new_mpp)
{
	if (new_mpp >= SG_VIEWPORT_ZOOM_MIN && new_mpp <= SG_VIEWPORT_ZOOM_MAX) {
		this->xmpp = new_mpp;
		this->ympp = new_mpp;
		/* Since xmpp & ympp are the same it doesn't matter which one is used here. */
		this->xmfactor = this->ymfactor = MERCATOR_FACTOR(this->xmpp);
	}

	if (this->drawmode == ViewportDrawMode::UTM) {
		this->utm_zone_check();
	}
}




void Viewport::zoom_in(void)
{
	if (this->xmpp >= (SG_VIEWPORT_ZOOM_MIN * 2) && this->ympp >= (SG_VIEWPORT_ZOOM_MIN * 2)) {
		this->xmpp /= 2;
		this->ympp /= 2;

		this->xmfactor = MERCATOR_FACTOR(this->xmpp);
		this->ymfactor = MERCATOR_FACTOR(this->ympp);

		this->utm_zone_check();
	}
}




void Viewport::zoom_out(void)
{
	if (this->xmpp <= (SG_VIEWPORT_ZOOM_MAX / 2) && this->ympp <= (SG_VIEWPORT_ZOOM_MAX / 2)) {
		this->xmpp *= 2;
		this->ympp *= 2;

		xmfactor = MERCATOR_FACTOR(this->xmpp);
		ymfactor = MERCATOR_FACTOR(this->ympp);

		this->utm_zone_check();
	}
}




double Viewport::get_zoom(void) const
{
	if (this->xmpp == this->ympp) {
		return this->xmpp;
	}
	return 0.0; /* kamilTODO: why 0.0? */
}




double Viewport::get_xmpp(void) const
{
	return this->xmpp;
}




double Viewport::get_ympp(void) const
{
	return this->ympp;
}




void Viewport::set_xmpp(double new_xmpp)
{
	if (new_xmpp >= SG_VIEWPORT_ZOOM_MIN && new_xmpp <= SG_VIEWPORT_ZOOM_MAX) {
		this->xmpp = new_xmpp;
		this->ymfactor = MERCATOR_FACTOR(this->ympp);
		if (this->drawmode == ViewportDrawMode::UTM) {
			this->utm_zone_check();
		}
	}
}




void Viewport::set_ympp(double new_ympp)
{
	if (new_ympp >= SG_VIEWPORT_ZOOM_MIN && new_ympp <= SG_VIEWPORT_ZOOM_MAX) {
		this->ympp = new_ympp;
		this->ymfactor = MERCATOR_FACTOR(this->ympp);
		if (this->drawmode == ViewportDrawMode::UTM) {
			this->utm_zone_check();
		}
	}
}




const Coord * Viewport::get_center() const
{
	return &center;
}




/* Called every time we update coordinates/zoom. */
void Viewport::utm_zone_check()
{
	if (coord_mode == CoordMode::UTM) {
		struct UTM utm;
		struct LatLon ll;
		a_coords_utm_to_latlon(&ll, &center.utm);
		a_coords_latlon_to_utm(&utm, &ll);
		if (utm.zone != center.utm.zone) {
			this->center.utm = utm;
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
	Coord * coord = *iter;
	if (coord) {
		delete coord;
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
	Coord * new_center = new Coord();
	*new_center = center; /* kamilFIXME: review this assignment of object. */

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

	// qDebug() << "SIGNAL: Viewport: emitting updated_center()";
	emit this->updated_center();
}




/**
 * Show the list of forward/backward positions.
 * ATM only for debug usage.
 */
void Viewport::show_centers(Window * parent_window)
{
	std::list<QString> texts;
	for (auto iter = centers->begin(); iter != centers->end(); iter++) {
		char * lat = NULL;
		char * lon = NULL;
		struct LatLon ll = (*iter)->get_latlon();
		a_coords_latlon_to_string(&ll, &lat, &lon);
		QString extra;
		/* Put the separating space in 'extra'. */
		if (iter == next(centers_iter)) {
			extra = QString(" [Back]");
		} else if (iter == prev(centers_iter)) {
			extra = QString(" [Forward]");
		} else {
			;
		}

		texts.push_back(QString("%1 %2%3").arg(lat).arg(lon).arg(extra));
		free(lat);
		free(lon);
	}

	/* No i18n as this is just for debug.
	   Using this function the dialog allows sorting of the list which isn't appropriate here
	   but this doesn't matter much for debug purposes of showing stuff... */
	const QStringList headers = { tr("Back/Forward Locations") };
	std::list<QString> result = a_dialog_select_from_list(texts,
							      false,
							      tr("Back/Forward Locations"),
							      headers,
							      parent_window);

	for (auto iter = result.begin(); iter != result.end(); iter++) {
		qDebug() << "DD: Viewport: history center item:" << *iter;
	}
}




void Viewport::print_centers(char * label)
{
	for (auto iter = centers->begin(); iter != centers->end(); iter++) {
		char *lat = NULL, *lon = NULL;
		struct LatLon ll = (*iter)->get_latlon();
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
	Coord * last_center = *centers_iter; /* kamilTODO: add check of validity of iterator? */
	if (last_center) {
		/* Consider an exclusion size (should it zoom level dependent, rather than a fixed value?).
		   When still near to the last saved position we'll jump over it to the one before. */
		if (Coord::distance(*last_center, this->center) > centers_radius) {

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

	Coord * new_center = *centers_iter; /* kamilTODO: add check of validity of iterator. */
	if (new_center) {
		set_center_coord(*new_center, false);
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
	Coord * new_center = *centers_iter; /* kamilTODO: add check of validity of iterator. */
	if (new_center) {
		set_center_coord(*new_center, false);
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
	this->center = Coord(*ll, coord_mode);
	if (save_position) {
		this->update_centers();
	}

	if (coord_mode == CoordMode::UTM) {
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
	this->center = Coord(*utm, coord_mode);
	if (save_position) {
		this->update_centers();
	}

	if (coord_mode == CoordMode::UTM) {
		this->utm_zone_check();
	}
}




/**
 * @coord:         The new center position in a Coord type
 * @save_position: Whether this new position should be saved into the history of positions
 *                 Normally only specific user requests should be saved (i.e. to not include Pan and Zoom repositions)
 */
void Viewport::set_center_coord(const Coord & coord, bool save_position)
{
	this->center = coord;
	if (save_position) {
		this->update_centers();
	}
	if (this->coord_mode == CoordMode::UTM) {
		this->utm_zone_check();
	}
}




void Viewport::corners_for_zonen(int zone, Coord * ul, Coord * br)
{
	if (coord_mode != CoordMode::UTM) {
		return;
	}

	/* Get center, then just offset. */
	this->center_for_zonen(&ul->utm, zone);
	ul->mode = CoordMode::UTM;
	*br = *ul;

	ul->utm.northing += (ympp * this->size_height / 2);
	ul->utm.easting -= (xmpp * this->size_width / 2);
	br->utm.northing -= (ympp * this->size_height / 2);
	br->utm.easting += (xmpp * this->size_width / 2);
}




void Viewport::center_for_zonen(struct UTM * center_utm, int zone)
{
	if (coord_mode == CoordMode::UTM) {
		*center_utm = this->center.utm;
		center_utm->easting -= (zone - center_utm->zone) * utm_zone_width;
		center_utm->zone = zone;
	}
}




char Viewport::leftmost_zone()
{
	if (coord_mode == CoordMode::UTM) {
		Coord coord = this->screen_to_coord(0, 0);
		return coord.utm.zone;
	}
	return '\0';
}




char Viewport::rightmost_zone()
{
	if (coord_mode == CoordMode::UTM) {
		Coord coord = this->screen_to_coord(this->size_width, 0);
		return coord.utm.zone;
	}
	return '\0';
}




void Viewport::set_center_screen(int x1, int y1)
{
	if (coord_mode == CoordMode::UTM) {
		/* Slightly optimized. */
		center.utm.easting += xmpp * (x1 - (this->size_width / 2));
		center.utm.northing += ympp * ((this->size_height / 2) - y1);
		this->utm_zone_check();
	} else {
		Coord coord = this->screen_to_coord(x1, y1);
		this->set_center_coord(coord, false);
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




Coord Viewport::screen_to_coord(int pos_x, int pos_y)
{
	Coord coord;

	if (this->coord_mode == CoordMode::UTM) {
		coord.mode = CoordMode::UTM;

		coord.utm.zone = this->center.utm.zone;
		coord.utm.letter = this->center.utm.letter;
		coord.utm.easting = ((pos_x - (this->size_width_2)) * xmpp) + this->center.utm.easting;

		int zone_delta = floor((coord.utm.easting - EASTING_OFFSET) / utm_zone_width + 0.5);

		coord.utm.zone += zone_delta;
		coord.utm.easting -= zone_delta * utm_zone_width;
		coord.utm.northing = (((this->size_height_2) - pos_y) * ympp) + this->center.utm.northing;

	} else if (this->coord_mode == CoordMode::LATLON) {
		coord.mode = CoordMode::LATLON;

		if (this->drawmode == ViewportDrawMode::LATLON) {
			coord.ll.lon = this->center.ll.lon + (180.0 * xmpp / 65536 / 256 * (pos_x - this->size_width_2));
			coord.ll.lat = this->center.ll.lat + (180.0 * ympp / 65536 / 256 * (this->size_height_2 - pos_y));

		} else if (this->drawmode == ViewportDrawMode::EXPEDIA) {
			calcxy_rev(&coord.ll.lon, &coord.ll.lat, pos_x, pos_y, center.ll.lon, this->center.ll.lat, xmpp * ALTI_TO_MPP, ympp * ALTI_TO_MPP, this->size_width_2, this->size_height_2);

		} else if (this->drawmode == ViewportDrawMode::MERCATOR) {
			/* This isn't called with a high frequently so less need to optimize. */
			coord.ll.lon = this->center.ll.lon + (180.0 * xmpp / 65536 / 256 * (pos_x - this->size_width_2));
			coord.ll.lat = DEMERCLAT (MERCLAT(this->center.ll.lat) + (180.0 * ympp / 65536 / 256 * (this->size_height_2 - pos_y)));
		} else {
			qDebug() << "EE: Viewport: Screen to coord: unrecognized draw mode" << (int) this->drawmode;
		}
	} else {
		qDebug() << "EE: Viewport: Screen to coord: unrecognized coord mode" << (int) this->coord_mode;
	}

	return coord; /* Named Return Value Optimization. */
}




/*
 * Since this function is used for every drawn trackpoint - it can get called a lot.
 * Thus x & y position factors are calculated once on zoom changes,
 * avoiding the need to do it here all the time.
 * For good measure the half width and height values are also pre calculated too.
 */
void Viewport::coord_to_screen(const Coord * coord, int * pos_x, int * pos_y)
{
	static Coord tmp;

	if (coord->mode != this->coord_mode) {
		qDebug() << "WW: Viewport: Have to convert in Viewport::coord_to_screen()! This should never happen!";
		tmp = coord->copy_change_mode(this->coord_mode); /* kamilTODO: what's going on here? Why this special case even exists? */
		coord = &tmp;
	}

	if (this->coord_mode == CoordMode::UTM) {
		const struct UTM * utm_center = &this->center.utm;
		const struct UTM * utm = &coord->utm;
		if (utm_center->zone != utm->zone && this->one_utm_zone){
			*pos_x = *pos_y = VIK_VIEWPORT_UTM_WRONG_ZONE;
			return;
		}

		*pos_x = ((utm->easting - utm_center->easting) / this->xmpp) + (this->size_width_2) -
			(utm_center->zone - utm->zone) * this->utm_zone_width / this->xmpp;
		*pos_y = (this->size_height_2) - ((utm->northing - utm_center->northing) / this->ympp);
	} else if (this->coord_mode == CoordMode::LATLON) {
		const struct LatLon * ll_center = &this->center.ll;
		const struct LatLon * ll = &coord->ll;
		double xx,yy;
		if (this->drawmode == ViewportDrawMode::LATLON) {
			*pos_x = this->size_width_2 + (MERCATOR_FACTOR(this->xmpp) * (ll->lon - ll_center->lon));
			*pos_y = this->size_height_2 + (MERCATOR_FACTOR(this->ympp) * (ll_center->lat - ll->lat));
		} else if (this->drawmode == ViewportDrawMode::EXPEDIA) {
			calcxy (&xx, &yy, ll_center->lon, ll_center->lat, ll->lon, ll->lat, this->xmpp * ALTI_TO_MPP, this->ympp * ALTI_TO_MPP, this->size_width_2, this->size_height_2);
			*pos_x = xx; *pos_y = yy;
		} else if (this->drawmode == ViewportDrawMode::MERCATOR) {
			*pos_x = this->size_width_2 + (MERCATOR_FACTOR(this->xmpp) * (ll->lon - ll_center->lon));
			*pos_y = this->size_height_2 + (MERCATOR_FACTOR(this->ympp) * (MERCLAT(ll_center->lat) - MERCLAT(ll->lat)));
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
		painter.drawLine(this->margin_left + x1, this->margin_top + y1,
				 this->margin_left + x2, this->margin_top + y2);
	}
}




void Viewport::draw_rectangle(const QPen & pen, int pos_x, int pos_y, int rect_width, int rect_height)
{
	/* Using 32 as half the default waypoint image size, so this draws ensures the highlight gets done. */
	if (pos_x > -32 && pos_x < this->size_width + 32 && pos_y > -32 && pos_y < this->size_height + 32) {

		QPainter painter(this->scr_buffer);
		painter.setPen(pen);
		painter.drawRect(pos_x, pos_y, rect_width, rect_height);
	}
}




void Viewport::fill_rectangle(const QColor & color, int pos_x, int pos_y, int rect_width, int rect_height)
{
	/* Using 32 as half the default waypoint image size, so this draws ensures the highlight gets done. */
	if (pos_x > -32 && pos_x < this->size_width + 32 && pos_y > -32 && pos_y < this->size_height + 32) {

		QPainter painter(this->scr_buffer);
		painter.fillRect(pos_x, pos_y, rect_width, rect_height, color);
	}
}




void Viewport::draw_text(QFont const & text_font, QPen const & pen, int pos_x, int pos_y, QString const & text)
{
	if (pos_x > -100 && pos_x < this->size_width + 100 && pos_y > -100 && pos_y < this->size_height + 100) {
		QPainter painter(this->scr_buffer);
		painter.setPen(pen);
		painter.setFont(text_font);
		painter.drawText(pos_x, pos_y, text);
	}
}




void Viewport::draw_text(QFont const & text_font, QPen const & pen, QRectF & bounding_rect, int flags, QString const & text, int text_offset)
{
	QPainter painter(this->scr_buffer);
	painter.setFont(text_font);

	/* "Normalize" bounding rectangles that have negative width or height.
	   Otherwise the text will be outside of the bounding rectangle. */
	QRectF final_bounding_rect = bounding_rect.united(bounding_rect);

	QRectF text_rect = painter.boundingRect(final_bounding_rect, flags, text);
	if (text_offset & SG_TEXT_OFFSET_UP) {
		/* Move boxes a bit up, so that text is right against grid line, not below it. */
		qreal new_top = text_rect.top() - (text_rect.height() / 2);
		final_bounding_rect.moveTop(new_top);
		text_rect.moveTop(new_top);
	}

	if (text_offset & SG_TEXT_OFFSET_LEFT) {
		/* Move boxes a bit left, so that text is right below grid line, not to the right of it. */
		qreal new_left = text_rect.left() - (text_rect.width() / 2);
		final_bounding_rect.moveLeft(new_left);
		text_rect.moveLeft(new_left);
	}


#if 1
	/* Debug. */
	painter.setPen(QColor("red"));
	painter.drawEllipse(bounding_rect.left(), bounding_rect.top(), 3, 3);

	painter.setPen(QColor("darkgreen"));
	painter.drawRect(bounding_rect);

	painter.setPen(QColor("red"));
	painter.drawRect(text_rect);
#endif

	painter.setPen(pen);
	painter.drawText(text_rect, flags, text, NULL);
	painter.end();
}




void Viewport::draw_text(QFont const & text_font, QPen const & pen, const QColor & bg_color, const QRectF & bounding_rect, int flags, const QString & text, int text_offset)
{
	QPainter painter(this->scr_buffer);
	painter.setFont(text_font);

	/* "Normalize" bounding rectangles that have negative width or height.
	   Otherwise the text will be outside of the bounding rectangle. */
	QRectF final_bounding_rect = bounding_rect.united(bounding_rect);

	QRectF text_rect = painter.boundingRect(final_bounding_rect, flags, text);
	if (text_offset & SG_TEXT_OFFSET_UP) {
		/* Move boxes a bit up, so that text is right against grid line, not below it. */
		qreal new_top = text_rect.top() - (text_rect.height() / 2);
		final_bounding_rect.moveTop(new_top);
		text_rect.moveTop(new_top);
	}

	if (text_offset & SG_TEXT_OFFSET_LEFT) {
		/* Move boxes a bit left, so that text is right below grid line, not to the right of it. */
		qreal new_left = text_rect.left() - (text_rect.width() / 2);
		final_bounding_rect.moveLeft(new_left);
		text_rect.moveLeft(new_left);
	}


#if 1
	/* Debug. */
	painter.setPen(QColor("red"));
	painter.drawEllipse(bounding_rect.left(), bounding_rect.top(), 3, 3);

	painter.setPen(QColor("darkgreen"));
	painter.drawRect(bounding_rect);
#endif

	/* A highlight of drawn text, must be executed before .drawText(). */
	painter.fillRect(text_rect, bg_color);

	painter.setPen(pen);
	painter.drawText(text_rect, flags, text, NULL);
	painter.end();
}




void Viewport::draw_pixmap(QPixmap const & pixmap, int src_x, int src_y, int dest_x, int dest_y, int dest_width, int dest_height)
{
	QPainter painter(this->scr_buffer);
	/* TODO: This clearly needs to be improved. */
	painter.drawPixmap(0, 0, pixmap, 0, 0, 0, 0);
#if 0
	gdk_draw_pixbuf(this->scr_buffer,
			NULL,
			pixbuf,
			src_x, src_y, dest_x, dest_y, region_width, region_height,
			GDK_RGB_DITHER_NONE, 0, 0);
#endif
}




void Viewport::draw_arc(QPen const & pen, int pos_x, int pos_y, int size_w, int size_h, int angle1, int angle2, bool filled)
{
	QPainter painter(this->scr_buffer);
	painter.setPen(pen);
	painter.drawArc(pos_x, pos_y, size_w, size_h, angle1, angle2 * 16); /* TODO: handle 'filled' argument. */
}





void Viewport::draw_polygon(QPen const & pen, QPoint const * points, int npoints, bool filled) /* TODO: handle 'filled' arg. */
{
	QPainter painter(this->scr_buffer);
	painter.setPen(pen);
	painter.drawPolygon(points, npoints);
}




CoordMode Viewport::get_coord_mode()
{
	return coord_mode;
}




void Viewport::set_coord_mode(CoordMode mode_)
{
	coord_mode = mode_;
	this->center.change_mode(mode_);
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
	return coord_mode == CoordMode::UTM && one_utm_zone;
}




void Viewport::set_drawmode(ViewportDrawMode drawmode_)
{
	drawmode = drawmode_;
	if (drawmode_ == ViewportDrawMode::UTM) {
		this->set_coord_mode(CoordMode::UTM);
	} else {
		this->set_coord_mode(CoordMode::LATLON);
	}
}




ViewportDrawMode Viewport::get_drawmode()
{
	return drawmode;
}




/******** Triggering. *******/
void Viewport::set_trigger(Layer * trg)
{
	this->trigger = trg;
}




Layer * Viewport::get_trigger()
{
	return this->trigger;
}




void Viewport::snapshot_save()
{
	qDebug() << "II: Viewport: save snapshot";
	*this->snapshot_buffer = *this->scr_buffer;
#ifdef K
	gdk_draw_drawable(this->snapshot_buffer, this->background_pen, this->scr_buffer, 0, 0, 0, 0, -1, -1);
#endif
}




void Viewport::snapshot_load()
{
	qDebug() << "II: Viewport: load snapshot";
	*this->scr_buffer = *this->snapshot_buffer;
#ifdef K
	gdk_draw_drawable(this->scr_buffer, this->background_pen, this->snapshot_buffer, 0, 0, 0, 0, -1, -1);
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




const QString Viewport::get_drawmode_name(ViewportDrawMode mode)
{
	return this->get_window()->get_drawmode_action(mode)->text();
}




void Viewport::get_min_max_lat_lon(double * min_lat, double * max_lat, double * min_lon, double * max_lon)
{
	Coord tleft = this->screen_to_coord(0,                 0);
	Coord tright = this->screen_to_coord(this->size_width, 0);
	Coord bleft = this->screen_to_coord(0,                 this->size_height);
	Coord bright = this->screen_to_coord(this->size_width, this->size_height);

	tleft.change_mode(CoordMode::LATLON);
	tright.change_mode(CoordMode::LATLON);
	bleft.change_mode(CoordMode::LATLON);
	bright.change_mode(CoordMode::LATLON);

	*max_lat = MAX(tleft.ll.lat, tright.ll.lat);
	*min_lat = MIN(bleft.ll.lat, bright.ll.lat);
	*max_lon = MAX(tright.ll.lon, bright.ll.lon);
	*min_lon = MIN(tleft.ll.lon, bleft.ll.lon);
}




void Viewport::get_bbox(LatLonBBox * bbox)
{
	Coord tleft = this->screen_to_coord(0,                 0);
	Coord tright = this->screen_to_coord(this->size_width, 0);
	Coord bleft = this->screen_to_coord(0,                 this->size_height);
	Coord bright = this->screen_to_coord(this->size_width, this->size_height);

	tleft.change_mode(CoordMode::LATLON);
	tright.change_mode(CoordMode::LATLON);
	bleft.change_mode(CoordMode::LATLON);
	bright.change_mode(CoordMode::LATLON);

	bbox->north = MAX(tleft.ll.lat, tright.ll.lat);
	bbox->south = MIN(bleft.ll.lat, bright.ll.lat);
	bbox->east  = MAX(tright.ll.lon, bright.ll.lon);
	bbox->west  = MIN(tleft.ll.lon, bleft.ll.lon);
}




void Viewport::get_bbox_strings(LatLonBBoxStrings & bbox_strings)
{
	LatLonBBox bbox;
	/* Get Viewport bounding box. */
	this->get_bbox(&bbox);

	CoordUtils::to_strings(bbox_strings, bbox);

	return;
}




void Viewport::reset_copyrights()
{
	this->copyrights.clear();
}




/**
 * @copyright: new copyright to display.
 *
 * Add a copyright to display on viewport.
 */
void Viewport::add_copyright(QString const & copyright)
{
	/* kamilTODO: make sure that this code is executed. */
	if (!this->copyrights.contains(copyright)) {
		this->copyrights.push_front(copyright);
	}
}




void vik_viewport_add_copyright_cb(Viewport * viewport, QString const & copyright)
{
	viewport->add_copyright(copyright);
}




void Viewport::reset_logos()
{
	/* Do not free pointers, they are owned by someone else.
	   TODO: this is potentially a source of problems - if owner deletes pointer, it becomes invalid in viewport, right?. */
	logos.clear();
}




void Viewport::add_logo(QPixmap const * logo)
{
	if (!logo) {
		qDebug() << "EE: Viewport: trying to add NULL logo";
		return;
	}

	auto iter = std::find(this->logos.begin(), this->logos.end(), logo); /* TODO: how does comparison of pointers work? */
	if (iter == this->logos.end()) {
		logos.push_front(logo);
	}

	return;
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

	if (this->get_drawmode() == ViewportDrawMode::UTM) {
		struct UTM u;
		int tx, ty;

		Coord test = this->screen_to_coord(x1, y1);
		struct LatLon ll = test.get_latlon();
		ll.lat += get_ympp() * get_height() / 11000.0; // about 11km per degree latitude
		a_coords_latlon_to_utm(&u, &ll);

		test = Coord(u, CoordMode::UTM); /* kamilFIXME: it was ViewportDrawMode::UTM. */
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




Window * Viewport::get_window(void)
{
	return this->window;
}




void Viewport::paintEvent(QPaintEvent * ev)
{
	qDebug() << "II: Viewport: paintEvent()" << __FUNCTION__ << __LINE__;

	QPainter painter(this);

	painter.drawPixmap(0, 0, *this->scr_buffer);

	painter.setPen(Qt::blue);
	painter.setFont(QFont("Arial", 30));
	painter.drawText(this->rect(), Qt::AlignCenter, "Qt");

	return;
}




void Viewport::resizeEvent(QResizeEvent * ev)
{
	qDebug() << "II: Viewport: resize event";
	this->configure();
	this->get_window()->draw_redraw();
	//this->draw_scale();

	return;
}





void Viewport::mousePressEvent(QMouseEvent * ev)
{
	qDebug() << "II: Viewport: mouse press event, button" << (int) ev->button();

	this->window->get_toolbox()->handle_mouse_click(ev);

	ev->accept();
}




bool Viewport::eventFilter(QObject * object, QEvent * ev)
{
	if (ev->type() == QEvent::MouseButtonDblClick) {
		QMouseEvent * m = (QMouseEvent *) ev;
		qDebug() << "II: Viewport: mouse DOUBLE CLICK event, button" << (int) m->button();

		if (m->button() == Qt::LeftButton) {
			this->window->get_toolbox()->handle_mouse_double_click(m);
			m->accept();
			return true; /* Eat event. */
		}
	}
	/* Standard event processing. */
	return false;
}




void Viewport::mouseMoveEvent(QMouseEvent * ev)
{
	this->draw_mouse_motion_cb(ev);

	if (ev->buttons() != Qt::NoButton) {
		// qDebug() << "II: Viewport: mouse move with buttons";
		this->window->get_toolbox()->handle_mouse_move(ev);
	} else {
		//qDebug() << "II: Viewport: mouse move without buttons";
	}

	emit this->cursor_moved(this, ev);

	ev->accept();
}


void Viewport::mouseReleaseEvent(QMouseEvent * ev)
{
	qDebug() << "II: Viewport: mouse release event, button" << (int) ev->button();

	this->window->get_toolbox()->handle_mouse_release(ev);
	emit this->button_released(this, ev);

	ev->accept();
}




void Viewport::wheelEvent(QWheelEvent * ev)
{
	QPoint angle = ev->angleDelta();
	qDebug() << "II: Viewport: wheel event, buttons =" << (int) ev->buttons() << "angle =" << angle.y();
	ev->accept();

	const Qt::KeyboardModifiers modifiers = ev->modifiers(); // (GDK_SHIFT_MASK | GDK_CONTROL_MASK);

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
		int pos_x, pos_y;
		int center_x = w / 2;
		int center_y = h / 2;
		Coord coord = this->screen_to_coord(ev->x(), ev->y());
		if (scroll_up) {
			this->zoom_in();
		} else {
			this->zoom_out();
		}
		this->coord_to_screen(&coord, &pos_x, &pos_y);
		this->set_center_screen(center_x + (pos_x - ev->x()),
					center_y + (pos_y - ev->y()));
	}

	qDebug() << "II: Viewport: wheel event, call Window::draw_update()" << __FUNCTION__ << __LINE__;
	this->window->draw_update_cb();
}




void Viewport::draw_mouse_motion_cb(QMouseEvent * ev)
{
	QPoint position = this->mapFromGlobal(QCursor::pos());

#if 0   /* Verbose debug. */
	qDebug() << "II: Viewport: difference in cursor position: dx = " << position.x() - ev->x() << ", dy = " << position.y() - ev->y();
#endif

	int pos_x = position.x();
	int pos_y = position.y();

	//this->window->tb->move(ev); /* TODO: uncomment this. */

	/* Get coordinates in viewport's coordinates mode. Get them as strings, just for presentation purposes. */
	static Coord coord;
	coord = this->screen_to_coord(pos_x, pos_y);
	QString first;
	QString second;
	coord.to_strings(first, second);

#if 0   /* Verbose debug. */
	qDebug() << "DD: Viewport: mouse motion: cursor pos:" << position << ", coordinates:" << first << second;
#endif

	/* Change interpolate method according to scale. */
	double zoom = this->get_zoom();
	DemInterpolation interpol_method;
	if (zoom > 2.0) {
		interpol_method = DemInterpolation::NONE;
	} else if (zoom >= 1.0) {
		interpol_method = DemInterpolation::SIMPLE;
	} else {
		interpol_method = DemInterpolation::BEST;
	}

	int16_t alt;
	QString message;
	if ((alt = DEMCache::get_elev_by_coord(&coord, interpol_method)) != DEM_INVALID_ELEVATION) {
		if (Preferences::get_unit_height() == HeightUnit::METRES) {
			message = QString("%1 %2 %3m").arg(first).arg(second).arg(alt);
		} else {
			message = QString("%1 %2 %3ft").arg(first).arg(second).arg((int) VIK_METERS_TO_FEET(alt));
		}
	} else {
		message = QString("%1 %2").arg(first).arg(second);
	}

	this->window->get_statusbar()->set_message(StatusBarField::POSITION, message);

	//this->window->pan_move(ev); /* TODO: uncomment this. */
}




#if 0 /* kamil: no longer used. */
/**
 * Utility function to get positional strings for the given location
 * lat and lon strings will get allocated and so need to be freed after use
 */
void Viewport::get_location_strings(struct UTM utm, char **lat, char **lon)
{
	if (this->get_drawmode() == ViewportDrawMode::UTM) {
		// Reuse lat for the first part (Zone + N or S, and lon for the second part (easting and northing) of a UTM format:
		//  ZONE[N|S] EASTING NORTHING
		*lat = (char *) malloc(4*sizeof(char));
		// NB zone is stored in a char but is an actual number
		snprintf(*lat, 4, "%d%c", utm.zone, utm.letter);
		*lon = (char *) malloc(16*sizeof(char));
		snprintf(*lon, 16, "%d %d", (int)utm.easting, (int)utm.northing);
	} else {
		struct LatLon ll;
		a_coords_utm_to_latlon(&ll, &utm);
		a_coords_latlon_to_string(&ll, lat, lon);
	}
}
#endif




void Viewport::set_margin(int top, int bottom, int left, int right)
{
	this->margin_top = top;
	this->margin_bottom = bottom;
	this->margin_left = left;
	this->margin_right = right;
}




/*
  Draw border between margins of viewport and main (central) area of viewport.
  Don't draw anything if all margins have zero width.
*/
void Viewport::draw_border(void)
{
	if (this->margin_top > 0
	    || this->margin_bottom > 0
	    || this->margin_left > 0
	    || this->margin_right > 0) {

		this->draw_rectangle(this->border_pen,
				     this->margin_left,
				     this->margin_top,
				     this->width() - this->margin_left - this->margin_right, /* Width of main area inside margins. */
				     this->height() - this->margin_top - this->margin_bottom); /* Height of main area inside margins. */

	}
}




bool Viewport::print_cb(QPrinter * printer)
{
	//QPageLayout page_layout = printer->pageLayout();
	printer->setPaperSize(QPrinter::A4);
	QRectF page_rect = printer->pageRect(QPrinter::DevicePixel);
	QRectF paper_rect = printer->paperRect(QPrinter::DevicePixel);

	qDebug() << "II: Viewport: print callback: page:" << page_rect << ", paper:" << paper_rect;


	QPainter painter(printer);
	//painter.drawPixmap(0, 0, *this->scr_buffer);

	QPen pen;

	pen.setColor(QColor("black"));
	pen.setWidth(2);
	painter.setPen(pen);
	painter.drawRect(page_rect);

	pen.setColor(QColor("red"));
	pen.setWidth(1);
	painter.setPen(pen);
	QRectF new_rect(4, 4, 40, 40);
	painter.drawRect(new_rect);
	new_rect = QRectF(16, 16, 40, 40);
	painter.drawRect(new_rect);

	return true;
}
