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




#define PREFIX " Viewport: "




#define DEFAULT_BACKGROUND_COLOR "#CCCCCC"
#define DEFAULT_HIGHLIGHT_COLOR "#EEA500"
/* Default highlight in orange. */




#define MERCATOR_FACTOR(x) ((65536.0 / 180 / (x)) * 256.0)

#define VIK_SETTINGS_VIEW_LAST_LATITUDE     "viewport_last_latitude"
#define VIK_SETTINGS_VIEW_LAST_LONGITUDE    "viewport_last_longitude"
#define VIK_SETTINGS_VIEW_LAST_ZOOM_X       "viewport_last_zoom_xpp"
#define VIK_SETTINGS_VIEW_LAST_ZOOM_Y       "viewport_last_zoom_ypp"
#define VIK_SETTINGS_VIEW_HISTORY_SIZE      "viewport_history_size"
#define VIK_SETTINGS_VIEW_HISTORY_DIFF_DIST "viewport_history_diff_dist"




extern Tree * g_tree;




static double EASTING_OFFSET = 500000.0;

static bool calcxy(double * pos_x, double * pos_y, double lg, double lt, double zero_long, double zero_lat, double pixelfact_x, double pixelfact_y, int mapSizeX2, int mapSizeY2);
static bool calcxy_rev(double * longitude, double * latitude, int x, int y, double zero_long, double zero_lat, double pixelfact_x, double pixelfact_y, int mapSizeX2, int mapSizeY2);
static double calcR(double lat);

static double Radius[181];
static void viewport_init_ra();





void SlavGPS::viewport_init(void)
{
	viewport_init_ra();
}




void Viewport::init_drawing_area()
{
	//connect(this, SIGNAL(resizeEvent(QResizeEvent *)), this, SLOT(reconfigure_drawing_area_cb(void)));
	//this->qpainter = new QPainter(this);

	this->setFocusPolicy(Qt::ClickFocus);
}




double Viewport::calculate_utm_zone_width(void) const
{
	switch (this->coord_mode) {
	case CoordMode::UTM: {

		/* Get latitude of screen bottom. */
		UTM utm = this->center.utm;
		utm.northing -= this->size_height * ympp / 2;
		LatLon ll = UTM::to_latlon(utm);

		/* Boundary. */
		ll.lon = (utm.zone - 1) * 6 - 180 ;
		utm = LatLon::to_utm(ll);
		return fabs(utm.easting - EASTING_OFFSET) * 2;
	}

	case CoordMode::LATLON:
		return 0.0;

	default:
		qDebug() << "EE:" PREFIX << __FUNCTION__ << __LINE__ << "unexpected coord mode" << (int) this->coord_mode;
		return 0.0;
	}
}




const QColor & Viewport::get_background_color(void) const
{
	return this->background_color;
}




Viewport::Viewport(Window * parent_window) : QWidget(parent_window)
{
	this->window = parent_window;



	this->installEventFilter(this);

	this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	this->setMinimumSize(200, 300);
	//this->setMaximumSize(2700, 2700);

	/* We want to constantly update cursor position in
	   status bar. For this we need cursor tracking in viewport. */
	this->setMouseTracking(true);



	LatLon initial_lat_lon(Preferences::get_default_lat(), Preferences::get_default_lon());
	double zoom_x = 4.0;
	double zoom_y = 4.0;

	if (Preferences::get_startup_method() == VIK_STARTUP_METHOD_LAST_LOCATION) {
		double value;
		if (ApplicationState::get_double(VIK_SETTINGS_VIEW_LAST_LATITUDE, &value)) {
			initial_lat_lon.lat = value;
		}

		if (ApplicationState::get_double(VIK_SETTINGS_VIEW_LAST_LONGITUDE, &value)) {
			initial_lat_lon.lon = value;
		}

		if (ApplicationState::get_double(VIK_SETTINGS_VIEW_LAST_ZOOM_X, &value)) {
			zoom_x = value;
		}

		if (ApplicationState::get_double(VIK_SETTINGS_VIEW_LAST_ZOOM_Y, &value)) {
			zoom_y = value;
		}
	}

	this->xmpp = zoom_x;
	this->ympp = zoom_y;
	this->xmfactor = MERCATOR_FACTOR(this->xmpp);
	this->ymfactor = MERCATOR_FACTOR(this->ympp);



	this->coord_mode = CoordMode::LATLON;
	this->drawmode = ViewportDrawMode::MERCATOR;



	this->centers = new std::list<Coord>;
	this->centers_iter = this->centers->begin();
	if (!ApplicationState::get_integer(VIK_SETTINGS_VIEW_HISTORY_SIZE, &this->centers_max)) {
		this->centers_max = 20;
	}
	if (ApplicationState::get_integer(VIK_SETTINGS_VIEW_HISTORY_DIFF_DIST, &this->centers_radius)) {
		this->centers_radius = 500;
	}



	this->center.mode = CoordMode::LATLON;
	this->center.ll.lat = initial_lat_lon.lat;
	this->center.ll.lon = initial_lat_lon.lon;
	const UTM initial_utm = LatLon::to_utm(initial_lat_lon);
	this->center.utm.zone = initial_utm.zone; /* kamilTODO: why do we assign utm values when mode is CoordMode::LATLON? */
	this->center.utm.letter = initial_utm.letter;
	this->save_current_center();



	this->init_drawing_area();



	strncpy(this->type_string, "Le Viewport", (sizeof (this->type_string)) - 1);
	this->scale_visibility = true;

	this->border_pen.setColor(QColor("black"));
	this->border_pen.setWidth(2);

	this->marker_pen.setColor(QColor("brown"));
	this->marker_pen.setWidth(1);

	this->grid_pen.setColor(QColor("dimgray"));
	this->grid_pen.setWidth(1);

	this->background_pen.setColor(QString(DEFAULT_BACKGROUND_COLOR));
	this->background_pen.setWidth(1);
	this->set_background_color(QString(DEFAULT_BACKGROUND_COLOR));

	this->highlight_pen.setColor(DEFAULT_HIGHLIGHT_COLOR);
	this->highlight_pen.setWidth(1);
	this->set_highlight_color(QString(DEFAULT_HIGHLIGHT_COLOR));
}




Viewport::~Viewport()
{
	qDebug() << "II: Viewport: ~Viewport called";
	if (Preferences::get_startup_method() == VIK_STARTUP_METHOD_LAST_LOCATION) {
		const LatLon lat_lon = this->center.get_latlon();
		ApplicationState::set_double(VIK_SETTINGS_VIEW_LAST_LATITUDE, lat_lon.lat);
		ApplicationState::set_double(VIK_SETTINGS_VIEW_LAST_LONGITUDE, lat_lon.lon);
		ApplicationState::set_double(VIK_SETTINGS_VIEW_LAST_ZOOM_X, this->xmpp);
		ApplicationState::set_double(VIK_SETTINGS_VIEW_LAST_ZOOM_Y, this->ympp);
	}

	delete this->centers;
	delete this->scr_buffer;
	delete this->snapshot_buffer;
}




void Viewport::set_background_color(const QString & color_name)
{
	this->background_color.setNamedColor(color_name);
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




void Viewport::set_highlight_color(const QString & color_name)
{
	this->highlight_color.setNamedColor(color_name);
	this->highlight_pen.setColor(this->highlight_color);
}




void Viewport::set_highlight_color(const QColor & color)
{
	this->highlight_color = color;
	this->highlight_pen.setColor(color);
}




QPen Viewport::get_highlight_pen(void) const
{
	return this->highlight_pen;
}




void Viewport::set_highlight_thickness(int w)
{
	this->highlight_pen.setWidth(w);
	// GDK_LINE_SOLID
	// GDK_CAP_ROUND
	// GDK_JOIN_ROUND
}




void Viewport::reconfigure_drawing_area(int new_width, int new_height)
{
	if (new_width == 0 && new_height == 0) {
		const QRect geom = this->geometry();
		this->size_width = geom.width();
		this->size_height = geom.height();
	} else {
		this->size_width = new_width;
		this->size_height = new_height;
	}

	this->size_width_2 = this->size_width / 2;
	this->size_height_2 = this->size_height / 2;

	if (this->scr_buffer) {
		qDebug() << "II:" PREFIX << __FUNCTION__ << __LINE__ << "deleting old scr_buffer";
		delete this->scr_buffer;
	}

	qDebug() << "II:" PREFIX << __FUNCTION__ << __LINE__ << "creating new scr_buffer with size" << this->size_width << this->size_height;
	this->scr_buffer = new QPixmap(this->size_width, this->size_height);
	this->scr_buffer->fill();

	/* TODO trigger: only if this is enabled!!! */
	if (this->snapshot_buffer) {
		qDebug() << "DD:" PREFIX << __FUNCTION__ << __LINE__ << "deleting old snapshot buffer";
		delete this->snapshot_buffer;
	}
	qDebug() << "II:" PREFIX << __FUNCTION__ << __LINE__ << "creating new snapshot buffer with size" << this->size_width << this->size_height;
	this->snapshot_buffer = new QPixmap(this->size_width, this->size_height);

	qDebug() << "SIGNAL:" PREFIX << __FUNCTION__ << __LINE__ << "sending \"drawing area reconfigured\" from" << this->type_string;
	emit this->drawing_area_reconfigured(this);
}




QPixmap * Viewport::get_pixmap(void) const
{
	return this->scr_buffer;
}




void Viewport::set_pixmap(const QPixmap & pixmap)
{
	QPainter painter(this->scr_buffer);
	/* TODO: Add some comparison of pixmap size and buffer size to verify that both have the same size and that pixmap can be safely used. */
	painter.drawPixmap(0, 0, pixmap, 0, 0, 0, 0);
}




bool Viewport::reconfigure_drawing_area_cb(void)
{
	qDebug() << "SLOT:" PREFIX << __FUNCTION__ << __LINE__;
	this->reconfigure_drawing_area();
	return true;
}




/**
   \brief Clear the whole viewport
*/
void Viewport::clear(void)
{
	qDebug() << "II:" << PREFIX << __FUNCTION__ << __LINE__ << "clear whole viewport" << this->type_string;
	QPainter painter(this->scr_buffer);
	painter.eraseRect(0, 0, this->size_width, this->size_height);

	this->decorations.reset_data();

}




void Viewport::draw_decorations(void)
{
	this->decorations.draw(this);

	return;
}




/**
   \brief Enable/Disable display of center mark
*/
void Viewport::set_center_mark_visibility(bool new_state)
{
	this->center_mark_visibility = new_state;
}




bool Viewport::get_center_mark_visibility() const
{
	return this->center_mark_visibility;
}




void Viewport::set_highlight_usage(bool new_state)
{
	this->highlight_usage = new_state;
}




bool Viewport::get_highlight_usage(void) const
{
	return this->highlight_usage;
}



/**
   \brief Enable/Disable display of scale
*/
void Viewport::set_scale_visibility(bool new_state)
{
	this->scale_visibility = new_state;
}




bool Viewport::get_scale_visibility(void) const
{
	return this->scale_visibility;
}




void Viewport::sync(void)
{
	qDebug() << "II:" PREFIX << __FUNCTION__ << __LINE__ << "sync (will call ->render())";
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
void Viewport::utm_zone_check(void)
{
	if (this->coord_mode == CoordMode::UTM) {
		const UTM utm = LatLon::to_utm(UTM::to_latlon(center.utm));
		if (utm.zone != this->center.utm.zone) {
			this->center.utm = utm;
		}

		/* Misc. stuff so we don't have to check later. */
		this->utm_zone_width = this->calculate_utm_zone_width();
		this->is_one_utm_zone = (this->get_rightmost_zone() == this->get_leftmost_zone());
	}
}




/**
   \brief Remove an individual center position from the history list
*/
void Viewport::free_center(std::list<Coord>::iterator iter)
{
	if (iter == this->centers_iter) {
		this->centers_iter = this->centers->erase(iter);

		if (this->centers_iter == this->centers->end()) {
			/* We have removed last element on the list. */

			if (centers->empty()) {
				this->centers_iter = this->centers->begin();
			} else {
				this->centers_iter--;
			}
		}
	} else {
		this->centers->erase(iter);
	}
}




void Viewport::save_current_center(void)
{
	if (this->centers_iter == prev(this->centers->end())) {
		/* We are at most recent element of history. */
		if (this->centers->size() == (unsigned) this->centers_max) {
			/* List is full, so drop the oldest value to make room for the new one. */
			this->free_center(this->centers->begin());
		}
	} else {
		/* We are somewhere in the middle of history list, possibly at the beginning.
		   Every center visited after current one must be discarded. */
		this->centers->erase(next(this->centers_iter), this->centers->end());
		assert (std::next(this->centers_iter) == this->centers->end());
	}

	/* Store new position. */
	/* push_back() puts at the end. By convention end == newest. */
	this->centers->push_back(this->center);
	this->centers_iter++;
	assert (std::next(this->centers_iter) == this->centers->end());

	this->print_centers("Viewport::save_current_center()");

	qDebug() << "SIGNAL:" PREFIX << __FUNCTION__ << __LINE__ << "emitting center_updated()";
	emit this->center_updated();
}




std::list<QString> Viewport::get_centers_list(void) const
{
	std::list<QString> result;

	for (auto iter = this->centers->begin(); iter != this->centers->end(); iter++) {

		const LatLon lat_lon = (*iter).get_latlon();
		QString lat;
		QString lon;
		LatLon::to_strings(lat_lon, lat, lon);

		QString extra;
		if (iter == prev(this->centers_iter)) {
			extra = tr("[Back]");
		} else if (iter == next(this->centers_iter)) {
			extra = tr("[Forward]");
		} else if (iter == this->centers_iter) {
			extra = tr("[Current]");
		} else {
			; /* NOOP */
		}

		result.push_back(tr("%1 %2%3").arg(lat).arg(lon).arg(extra));
	}

	return result;
}




/**
   \brief Show the list of forward/back positions

   ATM only for debug usage.
*/
void Viewport::show_centers(Window * parent_window) const
{
	const std::list<QString> texts = this->get_centers_list();

	/* Using this function the dialog allows sorting of the list which isn't appropriate here
	   but this doesn't matter much for debug purposes of showing stuff... */
	const QStringList headers = { tr("Back/Forward Locations") };
	std::list<QString> result = a_dialog_select_from_list(texts,
							      false,
							      tr("Back/Forward Locations"),
							      headers,
							      parent_window);

	/* TODO: why do we need result here? */

	for (auto iter = result.begin(); iter != result.end(); iter++) {
		qDebug() << "DD: Viewport: history center item:" << *iter;
	}
}




void Viewport::print_centers(const QString & label) const
{
	const std::list<QString> texts = this->get_centers_list();

	for (auto iter = texts.begin(); iter != texts.end(); iter++) {
		qDebug() << "II: Viewport: centers:" << label << *iter;
	}

	return;
}





/**
   \brief Move back in the position history

   @return true on success
   @return false otherwise
*/
bool Viewport::go_back(void)
{
	/* See if the current position is different from the
	   last saved center position within a certain radius. */
	if (Coord::distance(*this->centers_iter, this->center) > this->centers_radius) {

		if (this->centers_iter == prev(this->centers->end())) {
			/* Only when we haven't already moved back in the list.
			   Remember where this request came from (alternatively we could insert in the list on every back attempt). */
			this->save_current_center();
		}
	}

	if (!this->back_available()) {
		/* Already at the oldest center. */
		return false;
	}

	/* If we inserted a position above, then this will then move to the last saved position.
	   Otherwise this will skip to the previous saved position, as it's probably somewhere else. */

	this->centers_iter--;
	/* kamilTODO: add check of validity of iterator. */
	this->set_center_from_coord(*this->centers_iter, false);
	return true;
}




/**
   \brief Move forward in the position history

   @return true on success
   @return false otherwise
*/
bool Viewport::go_forward(void)
{
	if (!this->forward_available()) {
		/* Already at the latest center. */
		return false;
	}

	this->centers_iter++;
	/* kamilTODO: add check of validity of iterator. */
	this->set_center_from_coord(*this->centers_iter, false);
	return true;
}




/**
   @return true when a previous position in the history is available
   @return false otherwise
*/
bool Viewport::back_available(void) const
{
	return (this->centers->size() > 1 && this->centers_iter != this->centers->begin());
}




/**
   @return true when a next position in the history is available
   @return false otherwise
*/
bool Viewport::forward_available(void) const
{
	return (this->centers->size() > 1 && this->centers_iter != prev(this->centers->end()));
}




/**
   @lat_lon:       The new center position in Lat/Lon format
   @save_position: Whether this new position should be saved into the history of positions
                   Normally only specific user requests should be saved (i.e. to not include Pan and Zoom repositions)
*/
void Viewport::set_center_from_latlon(const LatLon & lat_lon, bool save_position)
{
	this->center = Coord(lat_lon, this->coord_mode);
	if (save_position) {
		this->save_current_center();
	}

	if (this->coord_mode == CoordMode::UTM) {
		this->utm_zone_check();
	}
}




/**
   @utm:           The new center position in UTM format
   @save_position: Whether this new position should be saved into the history of positions
                   Normally only specific user requests should be saved (i.e. to not include Pan and Zoom repositions)
*/
void Viewport::set_center_from_utm(const UTM & utm, bool save_position)
{
	this->center = Coord(utm, this->coord_mode);
	if (save_position) {
		this->save_current_center();
	}

	if (this->coord_mode == CoordMode::UTM) {
		this->utm_zone_check();
	}
}




/**
   @coord:         The new center position in a Coord type
   @save_position: Whether this new position should be saved into the history of positions
                   Normally only specific user requests should be saved (i.e. to not include Pan and Zoom repositions)
*/
void Viewport::set_center_from_coord(const Coord & coord, bool save_position)
{
	this->center = coord;
	if (save_position) {
		this->save_current_center();
	}

	if (this->coord_mode == CoordMode::UTM) {
		this->utm_zone_check();
	}
}




void Viewport::get_corners_for_zone(Coord & coord_ul, Coord & coord_br, int zone)
{
	if (this->coord_mode != CoordMode::UTM) {
		return;
	}

	/* Get center, then just offset. */
	this->get_center_for_zone(coord_ul.utm, zone);
	coord_ul.mode = CoordMode::UTM;

	/* Both coordinates will be now at center. */
	coord_br = coord_ul;

	/* And now we offset the two coordinates:
	   we move the coordinates from center to one of the two corners. */
	coord_ul.utm.northing += (ympp * this->size_height / 2);
	coord_ul.utm.easting -= (xmpp * this->size_width / 2);
	coord_br.utm.northing -= (ympp * this->size_height / 2);
	coord_br.utm.easting += (xmpp * this->size_width / 2);
}




void Viewport::get_center_for_zone(UTM & center_utm, int zone)
{
	if (this->coord_mode != CoordMode::UTM) {
		return;
	}

	center_utm = this->center.utm;
	center_utm.easting -= (zone - center_utm.zone) * this->utm_zone_width;
	center_utm.zone = zone;
}




char Viewport::get_leftmost_zone(void) const
{
	if (coord_mode != CoordMode::UTM) {
		return '\0';
	}

	const Coord coord = this->screen_pos_to_coord(0, 0);
	return coord.utm.zone;
}




char Viewport::get_rightmost_zone(void) const
{
	if (coord_mode != CoordMode::UTM) {
		return '\0';
	}

	const Coord coord = this->screen_pos_to_coord(this->size_width, 0);
	return coord.utm.zone;
}




void Viewport::set_center_from_screen_pos(int x1, int y1)
{
	if (coord_mode == CoordMode::UTM) {
		/* Slightly optimized. */
		this->center.utm.easting += xmpp * (x1 - (this->size_width / 2));
		this->center.utm.northing += ympp * ((this->size_height / 2) - y1);
		this->utm_zone_check();
	} else {
		const Coord coord = this->screen_pos_to_coord(x1, y1);
		this->set_center_from_coord(coord, false);
	}
}




void Viewport::set_center_from_screen_pos(const ScreenPos & pos)
{
	this->set_center_from_screen_pos(pos.x, pos.y);
}




int Viewport::get_width(void) const
{
	return this->size_width;
}




int Viewport::get_height(void) const
{
	return this->size_height;
}




int Viewport::get_graph_width(void) const
{
	return this->width() - this->margin_left - this->margin_right;
}




int Viewport::get_graph_height(void) const
{
	return this->height() - this->margin_top - this->margin_bottom;
}




int Viewport::get_graph_top_edge(void) const
{
	return this->margin_top;
}




int Viewport::get_graph_bottom_edge(void) const
{
	return this->height() - this->margin_bottom;
}




int Viewport::get_graph_left_edge(void) const
{
	return this->margin_left;
}




int Viewport::get_graph_right_edge(void) const
{
	return this->width() - this->margin_right;
}




Coord Viewport::screen_pos_to_coord(int pos_x, int pos_y) const
{
	Coord coord;

	if (this->coord_mode == CoordMode::UTM) {
		coord.mode = CoordMode::UTM;

		coord.utm.zone = this->center.utm.zone;
		coord.utm.letter = this->center.utm.letter;
		coord.utm.easting = ((pos_x - (this->size_width_2)) * xmpp) + this->center.utm.easting;

		int zone_delta = floor((coord.utm.easting - EASTING_OFFSET) / this->utm_zone_width + 0.5);

		coord.utm.zone += zone_delta;
		coord.utm.easting -= zone_delta * this->utm_zone_width;
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




Coord Viewport::screen_pos_to_coord(const ScreenPos & pos) const
{
	return this->screen_pos_to_coord(pos.x, pos.y);
}




/*
  Since this function is used for every drawn trackpoint - it can get called a lot.
  Thus x & y position factors are calculated once on zoom changes,
  avoiding the need to do it here all the time.

  For good measure the half width and height values are also pre calculated too. TODO: do we really do that here?
*/
void Viewport::coord_to_screen_pos(const Coord & coord_in, int * pos_x, int * pos_y)
{
	Coord coord;
	if (coord_in.mode != this->coord_mode) {
		qDebug() << "WW: Viewport: Have to convert in Viewport::coord_to_screen_pos()! This should never happen!";
		coord = coord_in.copy_change_mode(this->coord_mode); /* kamilTODO: what's going on here? Why this special case even exists? */
	} else {
		coord = coord_in;
	}

	if (this->coord_mode == CoordMode::UTM) {
		const UTM * utm_center = &this->center.utm;
		const UTM * utm = &coord.utm;
		if (utm_center->zone != utm->zone && this->is_one_utm_zone){
			*pos_x = *pos_y = VIK_VIEWPORT_UTM_WRONG_ZONE;
			return;
		}

		*pos_x = ((utm->easting - utm_center->easting) / this->xmpp) + (this->size_width_2) -
			(utm_center->zone - utm->zone) * this->utm_zone_width / this->xmpp;
		*pos_y = (this->size_height_2) - ((utm->northing - utm_center->northing) / this->ympp);
	} else if (this->coord_mode == CoordMode::LATLON) {
		const LatLon * ll_center = &this->center.ll;
		const LatLon * ll = &coord.ll;
		if (this->drawmode == ViewportDrawMode::LATLON) {
			*pos_x = this->size_width_2 + (MERCATOR_FACTOR(this->xmpp) * (ll->lon - ll_center->lon));
			*pos_y = this->size_height_2 + (MERCATOR_FACTOR(this->ympp) * (ll_center->lat - ll->lat));
		} else if (this->drawmode == ViewportDrawMode::EXPEDIA) {
			double xx,yy;
			calcxy(&xx, &yy, ll_center->lon, ll_center->lat, ll->lon, ll->lat, this->xmpp * ALTI_TO_MPP, this->ympp * ALTI_TO_MPP, this->size_width_2, this->size_height_2);
			*pos_x = xx;
			*pos_y = yy;
		} else if (this->drawmode == ViewportDrawMode::MERCATOR) {
			*pos_x = this->size_width_2 + (MERCATOR_FACTOR(this->xmpp) * (ll->lon - ll_center->lon));
			*pos_y = this->size_height_2 + (MERCATOR_FACTOR(this->ympp) * (MERCLAT(ll_center->lat) - MERCLAT(ll->lat)));
		}
	}
}




ScreenPos Viewport::coord_to_screen_pos(const Coord & coord_in)
{
	ScreenPos pos;
	this->coord_to_screen_pos(coord_in, &pos.x, &pos.y);
	return pos;
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




void Viewport::draw_line(const QPen & pen, int begin_x, int begin_y, int end_x, int end_y)
{
	//fprintf(stderr, "Called to draw line between points (%d %d) and (%d %d)\n", begin_x, begin_y, end_x, end_y);

	if ((begin_x < 0 && end_x < 0) || (begin_y < 0 && end_y < 0)) {
		return;
	}
	if (begin_x > this->size_width && end_x > this->size_width) {
		return;
	}
	if (begin_y > this->size_height && end_y > this->size_height) {
		return;
	}

	/*** Clipping, yeah! ***/
	Viewport::clip_line(&begin_x, &begin_y, &end_x, &end_y);

	QPainter painter(this->scr_buffer);
	painter.setPen(pen);
	painter.drawLine(this->margin_left + begin_x, this->margin_top + begin_y,
			 this->margin_left + end_x, this->margin_top + end_y);
}




void Viewport::draw_rectangle(const QPen & pen, int upper_left_x, int upper_left_y, int rect_width, int rect_height)
{
	/* Using 32 as half the default waypoint image size, so this draws ensures the highlight gets done. */
	if (upper_left_x > -32 && upper_left_x < this->size_width + 32 && upper_left_y > -32 && upper_left_y < this->size_height + 32) {

		QPainter painter(this->scr_buffer);
		painter.setPen(pen);
		painter.drawRect(upper_left_x, upper_left_y, rect_width, rect_height);
	}
}




void Viewport::draw_rectangle(const QPen & pen, const QRect & rect)
{
	/* Using 32 as half the default waypoint image size, so this draws ensures the highlight gets done. */
	if (rect.x() > -32 && rect.x() < this->size_width + 32 && rect.y() > -32 && rect.y() < this->size_height + 32) {

		QPainter painter(this->scr_buffer);
		painter.setPen(pen);
		painter.drawRect(rect);
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




void Viewport::draw_pixmap(const QRect & canvas_rect, QPixmap const & pixmap, const QRect & pixmap_rect)
{
	QPainter painter(this->scr_buffer);
	painter.drawPixmap(canvas_rect, pixmap, pixmap_rect);
}




void Viewport::draw_arc(QPen const & pen, int center_x, int center_y, int size_w, int size_h, int angle1, int angle2, bool filled)
{
	QPainter painter(this->scr_buffer);
	painter.setPen(pen);
	painter.drawArc(center_x, center_y, size_w, size_h, angle1, angle2 * 16); /* TODO: handle 'filled' argument. */
}




void Viewport::draw_polygon(QPen const & pen, QPoint const * points, int npoints, bool filled) /* TODO: handle 'filled' arg. */
{
	QPainter painter(this->scr_buffer);
	painter.setPen(pen);
	painter.drawPolygon(points, npoints);
}




CoordMode Viewport::get_coord_mode(void) const
{
	return this->coord_mode;
}




void Viewport::set_coord_mode(CoordMode new_mode)
{
	this->coord_mode = new_mode;
	this->center.change_mode(new_mode);
}




/* Thanks GPSDrive. */
static bool calcxy_rev(double * longitude, double * latitude, int x, int y, double zero_long, double zero_lat, double pixelfact_x, double pixelfact_y, int mapSizeX2, int mapSizeY2)
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

	*latitude = lat;
	*longitude = lon;
	return (true);
}




/* Thanks GPSDrive. */
static bool calcxy(double * pos_x, double * pos_y, double lg, double lt, double zero_long, double zero_lat, double pixelfact_x, double pixelfact_y, int mapSizeX2, int mapSizeY2)
{
	int mapSizeX = 2 * mapSizeX2;
	int mapSizeY = 2 * mapSizeY2;

	assert (lt >= -90.0 && lt <= 90.0);
	//    lg *= rad2deg; // FIXME, optimize equations
	//    lt *= rad2deg;
	double Ra = Radius[90 + (int) lt];
	*pos_x = Ra * cos (DEG2RAD(lt)) * (lg - zero_long);
	*pos_y = Ra * (lt - zero_lat);
	double dif = Ra * RAD2DEG(1 - (cos ((DEG2RAD(lg - zero_long)))));
	*pos_y = *pos_y + dif / 1.85;
	*pos_x = *pos_x / pixelfact_x;
	*pos_y = *pos_y / pixelfact_y;
	*pos_x = mapSizeX2 - *pos_x;
	*pos_y += mapSizeY2;
	if ((*pos_x < 0)||(*pos_x >= mapSizeX)||(*pos_y < 0)||(*pos_y >= mapSizeY)) {
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




bool Viewport::is_one_zone(void) const
{
	return coord_mode == CoordMode::UTM && this->is_one_utm_zone;
}




void Viewport::set_drawmode(ViewportDrawMode new_drawmode)
{
	this->drawmode = new_drawmode;

	if (new_drawmode == ViewportDrawMode::UTM) {
		this->set_coord_mode(CoordMode::UTM);
	} else {
		this->set_coord_mode(CoordMode::LATLON);
	}
}




ViewportDrawMode Viewport::get_drawmode(void) const
{
	return this->drawmode;
}




/******** Triggering. *******/
void Viewport::set_trigger(Layer * trg)
{
	this->trigger = trg;
}




Layer * Viewport::get_trigger(void) const
{
	return this->trigger;
}




void Viewport::snapshot_save(void)
{
	qDebug() << "II: Viewport: save snapshot";
	*this->snapshot_buffer = *this->scr_buffer;

#if 0   /* Not used anymore. Keeping it for reference. */
	gdk_draw_drawable(this->snapshot_buffer, this->background_pen, this->scr_buffer, 0, 0, 0, 0, -1, -1);
#endif
}




void Viewport::snapshot_load(void)
{
	qDebug() << "II: Viewport: load snapshot";
	*this->scr_buffer = *this->snapshot_buffer;

#if 0   /* Not used anymore. Keeping it for reference. */
	gdk_draw_drawable(this->scr_buffer, this->background_pen, this->snapshot_buffer, 0, 0, 0, 0, -1, -1);
#endif
}




void Viewport::set_half_drawn(bool new_half_drawn)
{
	this->half_drawn = new_half_drawn;
}




bool Viewport::get_half_drawn(void) const
{
	return this->half_drawn;
}




LatLonMinMax Viewport::get_min_max_lat_lon(void) const
{
	LatLonMinMax min_max;

	Coord tleft =  this->screen_pos_to_coord(0,                0);
	Coord tright = this->screen_pos_to_coord(this->size_width, 0);
	Coord bleft =  this->screen_pos_to_coord(0,                this->size_height);
	Coord bright = this->screen_pos_to_coord(this->size_width, this->size_height);

	tleft.change_mode(CoordMode::LATLON);
	tright.change_mode(CoordMode::LATLON);
	bleft.change_mode(CoordMode::LATLON);
	bright.change_mode(CoordMode::LATLON);

	min_max.max.lat = MAX(tleft.ll.lat, tright.ll.lat);
	min_max.min.lat = MIN(bleft.ll.lat, bright.ll.lat);
	min_max.max.lon = MAX(tright.ll.lon, bright.ll.lon);
	min_max.min.lon = MIN(tleft.ll.lon, bleft.ll.lon);

	return min_max;
}




LatLonBBox Viewport::get_bbox(void) const
{
	Coord tleft =  this->screen_pos_to_coord(0,                0);
	Coord tright = this->screen_pos_to_coord(this->size_width, 0);
	Coord bleft =  this->screen_pos_to_coord(0,                this->size_height);
	Coord bright = this->screen_pos_to_coord(this->size_width, this->size_height);

	tleft.change_mode(CoordMode::LATLON);
	tright.change_mode(CoordMode::LATLON);
	bleft.change_mode(CoordMode::LATLON);
	bright.change_mode(CoordMode::LATLON);

	LatLonBBox bbox;
	bbox.north = MAX(tleft.ll.lat, tright.ll.lat);
	bbox.south = MIN(bleft.ll.lat, bright.ll.lat);
	bbox.east  = MAX(tright.ll.lon, bright.ll.lon);
	bbox.west  = MIN(tleft.ll.lon, bleft.ll.lon);

	return bbox;
}




/**
   \brief Add a copyright to display on viewport

   @copyright: new copyright to display.
*/
void Viewport::add_copyright(QString const & copyright)
{
	/* kamilTODO: make sure that this code is executed. */
	if (!this->decorations.copyrights.contains(copyright)) {
		this->decorations.copyrights.push_front(copyright);
	}
}




#ifdef K
void vik_viewport_add_copyright_cb(Viewport * viewport, QString const & copyright)
{
	viewport->add_copyright(copyright);
}
#endif




void Viewport::add_logo(QPixmap const * logo)
{
	if (!logo) {
		qDebug() << "EE: Viewport: trying to add NULL logo";
		return;
	}

	auto iter = std::find(this->decorations.logos.begin(), this->decorations.logos.end(), logo); /* TODO: how does comparison of pointers work? */
	if (iter == this->decorations.logos.end()) {
		this->decorations.logos.push_front(logo);
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

		Coord test = this->screen_pos_to_coord(x1, y1);
		LatLon ll = test.get_latlon();
		ll.lat += get_ympp() * get_height() / 11000.0; // about 11km per degree latitude

		test = Coord(LatLon::to_utm(ll), CoordMode::UTM); /* kamilFIXME: it was ViewportDrawMode::UTM. */
		const ScreenPos test_pos = this->coord_to_screen_pos(test);

		*baseangle = M_PI - atan2(test_pos.x - x1, test_pos.y - y1);
		*angle -= *baseangle;
	}

	if (*angle < 0) {
		*angle += 2 * M_PI;
	}

	if (*angle > 2 * M_PI) {
		*angle -= 2 * M_PI;
	}
}




void Viewport::paintEvent(QPaintEvent * ev)
{
	qDebug() << "II:" PREFIX << __FUNCTION__ << __LINE__;

	QPainter painter(this);

	painter.drawPixmap(0, 0, *this->scr_buffer);

	painter.setPen(Qt::blue);
	painter.setFont(QFont("Arial", 30));
	painter.drawText(this->rect(), Qt::AlignCenter, "SlavGPS");

	return;
}




void Viewport::resizeEvent(QResizeEvent * ev)
{
	qDebug() << "II: Viewport: resize event";
	this->reconfigure_drawing_area();
	g_tree->tree_get_main_window()->draw_redraw();
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
			this->set_center_from_screen_pos(w / 2, h / 3);
		} else {
			this->set_center_from_screen_pos(w / 2, h * 2 / 3);
		}
	} else if (modifiers == Qt::ShiftModifier) {
		/* Shift == pan left & right. */
		if (scroll_up) {
			this->set_center_from_screen_pos(w / 3, h / 2);
		} else {
			this->set_center_from_screen_pos(w * 2 / 3, h / 2);
		}
	} else if (modifiers == (Qt::ControlModifier | Qt::ShiftModifier)) {
		/* This zoom is on the center position. */
		if (scroll_up) {
			this->zoom_in();
		} else {
			this->zoom_out();
		}
	} else {
		/* TODO: see also code in GenericToolZoom::handle_mouse_click() */

		/* Make sure mouse is still over the same point on the map when we zoom. */
		int center_x = w / 2;
		int center_y = h / 2;
		const Coord orig_coord = this->screen_pos_to_coord(ev->x(), ev->y());
		const ScreenPos orig_pos = this->coord_to_screen_pos(orig_coord);

		if (scroll_up) {
			this->zoom_in();
		} else {
			this->zoom_out();
		}

		this->set_center_from_screen_pos(center_x + (orig_pos.x - ev->x()),
						 center_y + (orig_pos.y - ev->y()));
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
	coord = this->screen_pos_to_coord(pos_x, pos_y);
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
void Viewport::get_location_strings(UTM utm, QString & lat, QString & lon)
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
		const LatLon lat_lon = UTM::to_latlon(utm);
		LatLon::to_strings(lat_lon, lat, lon);
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



/*
  "Simple" means one horizontal and one vertical line crossing at given viewport position.
  @pos should indicate position in viewport's canvas.
  The function makes sure that the crosshair is drawn only inside of graph area.
*/
void Viewport::draw_simple_crosshair(const ScreenPos & pos)
{
	const int graph_width = this->width() - this->margin_left - this->margin_right;
	const int graph_height = this->height() - this->margin_top - this->margin_bottom;

	/* Small optimization: use QT's drawing primitives directly.
	   Remember that (0,0) screen position is in upper-left corner of viewport. */

	qDebug() << "II:" PREFIX << __FUNCTION__ << __LINE__ << "crosshair at" << pos.x << pos.y;

	if (pos.x < this->margin_left || pos.x > this->margin_left + graph_width) {
		/* Position outside of graph area. */
		return;
	}
	if (pos.y < this->margin_top || pos.y > this->margin_top + graph_height) {
		/* Position outside of graph area. */
		return;
	}

	QPainter painter(this->scr_buffer);
	painter.setPen(this->marker_pen);

	const int y = pos.y;

	/* Horizontal line. */
	painter.drawLine(this->margin_left + 0,           y,
			 this->margin_left + graph_width, y);

	/* Vertical line. */
	painter.drawLine(pos.x, this->margin_top + 0,
			 pos.x, this->margin_top + graph_height);
}




ScreenPos ScreenPos::get_average(const ScreenPos & pos1, const ScreenPos & pos2)
{
	return ScreenPos((pos1.x + pos2.x) / 2, (pos1.y + pos2.y) / 2);
}




bool ScreenPos::is_close_enough(const ScreenPos & pos1, const ScreenPos & pos2, int limit)
{
	return (abs(pos1.x - pos2.x) < limit) && (abs(pos1.y - pos2.y) < limit);
}




bool ScreenPos::operator==(const ScreenPos & pos) const
{
	return (this->x == pos.x) && (this->y == pos.y);
}




QString ViewportDrawModes::get_name(ViewportDrawMode mode)
{
	switch (mode) {
	case ViewportDrawMode::UTM:
		return QObject::tr("&UTM Mode");
	case ViewportDrawMode::EXPEDIA:
		return QObject::tr("&Expedia Mode");
	case ViewportDrawMode::MERCATOR:
		return QObject::tr("&Mercator Mode");
	case ViewportDrawMode::LATLON:
		return QObject::tr("&Lat/Lon Mode");
	default:
		qDebug() << "EE:" PREFIX << __FUNCTION__ << __LINE__ << "unexpected draw mode" << (int) mode;
		return "";
	}
}



QString ViewportDrawModes::get_id_string(ViewportDrawMode mode)
{
	QString mode_id_string;

	switch (mode) {
	case ViewportDrawMode::UTM:
		mode_id_string = "utm";
		break;
	case ViewportDrawMode::EXPEDIA:
		mode_id_string = "expedia";
		break;
	case ViewportDrawMode::MERCATOR:
		mode_id_string = "mercator";
		break;
	case ViewportDrawMode::LATLON:
		mode_id_string = "latlon";
		break;
	default:
		qDebug() << "EE:" PREFIX << __FUNCTION__ << __LINE__ << "unexpected draw mode" << (int) mode;
		break;
	}

	return mode_id_string;
}




bool ViewportDrawModes::set_draw_mode_from_file(Viewport * viewport, const char * line)
{
	bool success = true;

	if (0 == strcasecmp(line, "utm")) {
		viewport->set_drawmode(ViewportDrawMode::UTM);

	} else if (0 == strcasecmp(line, "expedia")) {
		viewport->set_drawmode(ViewportDrawMode::EXPEDIA);

	} else if (0 == strcasecmp(line, "google")) {
		success = false;
		qDebug() << "WW:" PREFIX << QObject::tr("Read file: draw mode 'google' no longer supported");

	} else if (0 == strcasecmp(line, "kh")) {
		success = false;
		qDebug() << "WW:" PREFIX << QObject::tr("Read file: draw mode 'kh' no more supported");

	} else if (0 == strcasecmp(line, "mercator")) {
		viewport->set_drawmode(ViewportDrawMode::MERCATOR);

	} else if (0 == strcasecmp(line, "latlon")) {
		viewport->set_drawmode(ViewportDrawMode::LATLON);
	} else {
		qDebug() << "EE:" PREFIX << QObject::tr("Read file: unexpected draw mode") << line;
		success = false;
	}

	return success;
}
