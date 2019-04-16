/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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




#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cassert>




#include <QMouseEvent>
#include <QWheelEvent>
#include <QDebug>
#include <QPainter>
#include <QMimeData>




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
#include "layers_panel.h"
#include "measurements.h"
#include "application_state.h"
#include "dialog.h"
#include "statusbar.h"
#include "expedia.h"




using namespace SlavGPS;




#define SG_MODULE "Viewport"




#define DEFAULT_BACKGROUND_COLOR "#CCCCCC"
#define DEFAULT_HIGHLIGHT_COLOR "#EEA500"
/* Default highlight in orange. */




#define MERCATOR_FACTOR(_mpp_) ((65536.0 / 180 / (_mpp_)) * 256.0)
/* TODO_LATER: Form of this expression should be optimized for usage in denominator. */
#define REVERSE_MERCATOR_FACTOR(_mpp_) ((65536.0 / 180 / (_mpp_)) * 256.0)

#define VIK_SETTINGS_VIEW_LAST_LATITUDE     "viewport_last_latitude"
#define VIK_SETTINGS_VIEW_LAST_LONGITUDE    "viewport_last_longitude"
#define VIK_SETTINGS_VIEW_LAST_ZOOM_X       "viewport_last_zoom_xpp"
#define VIK_SETTINGS_VIEW_LAST_ZOOM_Y       "viewport_last_zoom_ypp"
#define VIK_SETTINGS_VIEW_HISTORY_SIZE      "viewport_history_size"
#define VIK_SETTINGS_VIEW_HISTORY_DIFF_DIST "viewport_history_diff_dist"




static double EASTING_OFFSET = 500000.0;




void Viewport::init(void)
{
	Expedia::init_radius();
}




double Viewport::calculate_utm_zone_width(void) const
{
	switch (this->coord_mode) {
	case CoordMode::UTM: {

		/* Get latitude of screen bottom. */
		UTM utm = this->center.utm;
		utm.northing -= this->canvas.height * this->viking_zoom_level.y / 2;
		LatLon ll = UTM::to_latlon(utm);

		/* Boundary. */
		ll.lon = (utm.zone - 1) * 6 - 180 ;
		utm = LatLon::to_utm(ll);
		return fabs(utm.easting - EASTING_OFFSET) * 2;
	}

	case CoordMode::LatLon:
		return 0.0;

	default:
		qDebug() << SG_PREFIX_E << "Unexpected coord mode" << (int) this->coord_mode;
		return 0.0;
	}
}




const QColor & Viewport::get_background_color(void) const
{
	return this->background_color;
}




Viewport::Viewport(QWidget * parent) : QWidget(parent)
{
	this->window = ThisApp::get_main_window();



	this->installEventFilter(this);

	this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	this->setMinimumSize(200, 300);
	snprintf(this->canvas.debug, sizeof (this->canvas.debug), "%s", "center");
	//this->setMaximumSize(2700, 2700);

	/* We want to constantly update cursor position in
	   status bar. For this we need cursor tracking in viewport. */
	this->setMouseTracking(true);



	LatLon initial_lat_lon(Preferences::get_default_lat(), Preferences::get_default_lon());
	double zoom_x = 4.0;
	double zoom_y = 4.0;

	if (Preferences::get_startup_method() == StartupMethod::LastLocation) {
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

	this->viking_zoom_level.set(zoom_x, zoom_y);
	this->xmfactor = MERCATOR_FACTOR(this->viking_zoom_level.x);
	this->ymfactor = MERCATOR_FACTOR(this->viking_zoom_level.y);



	this->coord_mode = CoordMode::LatLon;
	this->drawmode = ViewportDrawMode::Mercator;



	this->centers = new std::list<Coord>;
	this->centers_iter = this->centers->begin();
	if (!ApplicationState::get_integer(VIK_SETTINGS_VIEW_HISTORY_SIZE, &this->centers_max)) {
		this->centers_max = 20;
	}
	if (ApplicationState::get_integer(VIK_SETTINGS_VIEW_HISTORY_DIFF_DIST, &this->centers_radius)) {
		this->centers_radius = 500;
	}


	this->set_center_from_lat_lon(initial_lat_lon); /* The function will reject latlon if it's invalid. */


	this->setFocusPolicy(Qt::ClickFocus);
	//this->qpainter = new QPainter(this);



	this->scale_visibility = true;

	this->labels_pen.setColor("black");
	this->labels_font.setFamily("Helvetica");
	this->labels_font.setPointSize(11);

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
	qDebug() << SG_PREFIX_I;
	if (Preferences::get_startup_method() == StartupMethod::LastLocation) {
		const LatLon lat_lon = this->center.get_latlon();
		ApplicationState::set_double(VIK_SETTINGS_VIEW_LAST_LATITUDE, lat_lon.lat);
		ApplicationState::set_double(VIK_SETTINGS_VIEW_LAST_LONGITUDE, lat_lon.lon);

		ApplicationState::set_double(VIK_SETTINGS_VIEW_LAST_ZOOM_X, this->viking_zoom_level.x);
		ApplicationState::set_double(VIK_SETTINGS_VIEW_LAST_ZOOM_Y, this->viking_zoom_level.y);
	}

	delete this->centers;
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

	this->highlight_pen.setCapStyle(Qt::RoundCap);
	this->highlight_pen.setJoinStyle(Qt::RoundJoin);

#if 0   /* This line style is used by default. */
	this->highlight_pen.setStyle(Qt::SolidLine);
#endif
}




void Viewport::reconfigure_drawing_area(int new_width, int new_height)
{
	if (new_width == 0 && new_height == 0) {
		qDebug() << SG_PREFIX_I << "Will reconfigure canvas with geometry sizes" << this->geometry().width() << this->geometry().height();
		this->canvas.reconfigure(this->geometry().width(), this->geometry().height());
	} else {
		qDebug() << SG_PREFIX_I << "Will reconfigure canvas with specified sizes" << new_width << new_height;
		this->canvas.reconfigure(new_width, new_height);
	}
}




QPixmap Viewport::get_pixmap(void) const
{
	return *this->canvas.pixmap;
}




void Viewport::set_pixmap(const QPixmap & pixmap)
{
	if (this->canvas.pixmap->size() != pixmap.size()) {
		qDebug() << SG_PREFIX_E << "Pixmap size mismatch: canvas =" << this->canvas.pixmap->size() << ", new pixmap =" << pixmap.size();
	} else {
		//QPainter painter(this->canvas.pixmap);
		this->canvas.painter->drawPixmap(0, 0, pixmap, 0, 0, 0, 0);
	}
}




/**
   \brief Clear the whole viewport
*/
void Viewport::clear(void)
{
	qDebug() << SG_PREFIX_I << "Clear whole viewport" << this->canvas.debug << this->canvas.width << this->canvas.height;
	//QPainter painter(this->canvas.pixmap);
	this->canvas.painter->eraseRect(0, 0, this->canvas.width, this->canvas.height);


	/* Some maps may have been removed, so their logos and/or
	   attributions/copyrights must be cleared as well. */
	this->decorations.clear();

}




void Viewport::draw_decorations(void)
{
#if 1   /* Debug. To verify display of attribution when there are no
	   maps, or only maps without attributions. */
	this->decorations.add_attribution("© Test attribution holder 1");
	this->decorations.add_attribution("© Another test attribution holder 2017-2019");
#endif

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
	qDebug() << SG_PREFIX_I << "sync (will call ->render())";
	//gdk_draw_drawable(gtk_widget_get_window(GTK_WIDGET(this)), gtk_widget_get_style(GTK_WIDGET(this))->bg_gc[0], GDK_DRAWABLE(this->canvas.pixmap), 0, 0, 0, 0, this->canvas.width, this->canvas.height);
	this->render(this->canvas.pixmap);
}




void Viewport::pan_sync(int x_off, int y_off)
{
	qDebug() << SG_PREFIX_I;
#ifdef K_FIXME_RESTORE
	int x, y, wid, hei;

	gdk_draw_drawable(gtk_widget_get_window(GTK_WIDGET(this)), gtk_widget_get_style(GTK_WIDGET(this))->bg_gc[0], GDK_DRAWABLE(this->canvas.pixmap), 0, 0, x_off, y_off, this->canvas.width, this->canvas.height);

	if (x_off >= 0) {
		x = 0;
		wid = x_off;
	} else {
		x = this->canvas.width + x_off;
		wid = -x_off;
	}

	if (y_off >= 0) {
		y = 0;
		hei = y_off;
	} else {
		y = this->canvas.height + y_off;
		hei = -y_off;
	}
	gtk_widget_queue_draw_area(GTK_WIDGET(this), x, 0, wid, this->canvas.height);
	gtk_widget_queue_draw_area(GTK_WIDGET(this), 0, y, this->canvas.width, hei);
#endif
}




sg_ret Viewport::set_viking_zoom_level(double new_value)
{
	if (!VikingZoomLevel::value_is_valid(new_value)) {
		qDebug() << SG_PREFIX_E << "Failed to set new zoom level, invalid value" << new_value;
		return sg_ret::err;
	}

	if (sg_ret::ok != this->viking_zoom_level.set(new_value, new_value)) {
		return sg_ret::err;
	}

	this->xmfactor = MERCATOR_FACTOR(this->viking_zoom_level.x);
	this->ymfactor = MERCATOR_FACTOR(this->viking_zoom_level.y);

	if (this->drawmode == ViewportDrawMode::UTM) {
		this->utm_zone_check();
	}

	return sg_ret::ok;
}




void Viewport::zoom_in(void)
{
	if (this->viking_zoom_level.zoom_in(2)) {
		this->xmfactor = MERCATOR_FACTOR(this->viking_zoom_level.x);
		this->ymfactor = MERCATOR_FACTOR(this->viking_zoom_level.y);

		this->utm_zone_check();
	}
}




void Viewport::zoom_out(void)
{
	if (this->viking_zoom_level.zoom_out(2)) {
		this->xmfactor = MERCATOR_FACTOR(this->viking_zoom_level.x);
		this->ymfactor = MERCATOR_FACTOR(this->viking_zoom_level.y);

		this->utm_zone_check();
	}
}




const VikingZoomLevel & Viewport::get_viking_zoom_level(void) const
{
	return this->viking_zoom_level;
}




sg_ret Viewport::set_viking_zoom_level(const VikingZoomLevel & new_value)
{
	this->viking_zoom_level = new_value;
	return sg_ret::ok;
}




sg_ret Viewport::set_viking_zoom_level_x(double new_value)
{
	if (!VikingZoomLevel::value_is_valid(new_value)) {
		qDebug() << SG_PREFIX_E << "Failed to set new zoom level, invalid value" << new_value;
		return sg_ret::err;
	}

	this->viking_zoom_level.x = new_value;
	this->xmfactor = MERCATOR_FACTOR(this->viking_zoom_level.x);
	if (this->drawmode == ViewportDrawMode::UTM) {
		this->utm_zone_check();
	}

	return sg_ret::ok;
}




sg_ret Viewport::set_viking_zoom_level_y(double new_value)
{
	if (!VikingZoomLevel::value_is_valid(new_value)) {
		qDebug() << SG_PREFIX_E << "Failed to set new zoom level, invalid value" << new_value;
		return sg_ret::err;
	}

	this->viking_zoom_level.y = new_value;
	this->ymfactor = MERCATOR_FACTOR(this->viking_zoom_level.y);
	if (this->drawmode == ViewportDrawMode::UTM) {
		this->utm_zone_check();
	}

	return sg_ret::ok;
}




const Coord * Viewport::get_center(void) const
{
	return &this->center;
}




Coord Viewport::get_center2(void) const
{
	return this->center;
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

	qDebug() << SG_PREFIX_SIGNAL << "Emitting center_updated()";
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
	BasicDialog dialog(tr("Back/Forward Locations"), parent_window);
	std::list<QString> result = a_dialog_select_from_list(dialog, texts, ListSelectionMode::SingleItem, headers);

	/* TODO_MAYBE: why do we allow any selection and why do we use result here? */

	/* Because we have used ListSelectionMode::SingleItem selection
	   mode, this list has at most one element. */
	if (!result.empty()) {
		qDebug() << SG_PREFIX_D << "History center item:" << *result.begin();
	}
}




void Viewport::print_centers(const QString & label) const
{
	const std::list<QString> texts = this->get_centers_list();

	for (auto iter = texts.begin(); iter != texts.end(); iter++) {
		qDebug() << SG_PREFIX_I << "Centers:" << label << *iter;
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

	/* This is safe because ::back_available() returned true. */
	this->centers_iter--;
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

	/* This is safe because ::forward_available() returned true. */
	this->centers_iter++;
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
sg_ret Viewport::set_center_from_lat_lon(const LatLon & lat_lon, bool save_position)
{
	if (!lat_lon.is_valid()) {
		qDebug() << SG_PREFIX_E << "Not setting lat/lon, value is invalid:" << lat_lon.lat << lat_lon.lon;
		return sg_ret::err;
	}

	this->center = Coord(lat_lon, this->coord_mode);
	if (save_position) {
		this->save_current_center();
	}

	if (this->coord_mode == CoordMode::UTM) {
		this->utm_zone_check();
	}

	return sg_ret::ok;
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
	coord_ul.utm.northing += (this->viking_zoom_level.y * this->canvas.height / 2);
	coord_ul.utm.easting  -= (this->viking_zoom_level.x * this->canvas.width / 2);
	coord_br.utm.northing -= (this->viking_zoom_level.y * this->canvas.height / 2);
	coord_br.utm.easting  += (this->viking_zoom_level.x * this->canvas.width / 2);
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




int Viewport::get_leftmost_zone(void) const
{
	if (coord_mode != CoordMode::UTM) {
		return 0;
	}

	const Coord coord = this->screen_pos_to_coord(0, 0);
	return coord.utm.zone;
}




int Viewport::get_rightmost_zone(void) const
{
	if (coord_mode != CoordMode::UTM) {
		return 0;
	}

	const Coord coord = this->screen_pos_to_coord(this->canvas.width, 0);
	return coord.utm.zone;
}




void Viewport::set_center_from_screen_pos(int x1, int y1)
{
	if (coord_mode == CoordMode::UTM) {
		/* Slightly optimized. */
		this->center.utm.easting += this->viking_zoom_level.x * (x1 - (this->canvas.width / 2));
		this->center.utm.northing += this->viking_zoom_level.y * ((this->canvas.height / 2) - y1);
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
	return this->canvas.width;
}




int Viewport::get_height(void) const
{
	return this->canvas.height;
}




QRect Viewport::get_rect(void) const
{
	return QRect(0, 0, this->canvas.width, this->canvas.height);
}




Coord Viewport::screen_pos_to_coord(int pos_x, int pos_y) const
{
	Coord coord;
	const double xmpp = this->viking_zoom_level.x;
	const double ympp = this->viking_zoom_level.y;

	switch (this->coord_mode) {
	case CoordMode::UTM:
		coord.mode = CoordMode::UTM;

		/* Modified (reformatted) formula. */
		{
			const int delta_x = pos_x - this->canvas.width_2;
			const int delta_y = this->canvas.height_2 - pos_y;

			coord.utm.zone = this->center.utm.zone;
			assert (UTM::is_band_letter(this->center.utm.get_band_letter())); /* TODO_2_LATER: add smarter error handling. In theory the source object should be valid and for sure contain valid band letter. */
			coord.utm.set_band_letter(this->center.utm.get_band_letter());
			coord.utm.easting = (delta_x * xmpp) + this->center.utm.easting;

			const int zone_delta = floor((coord.utm.easting - EASTING_OFFSET) / this->utm_zone_width + 0.5);

			coord.utm.zone += zone_delta;
			coord.utm.easting -= zone_delta * this->utm_zone_width;
			coord.utm.northing = (delta_y * ympp) + this->center.utm.northing;
		}

		/* Original code, used for comparison of results with new, reformatted formula. */
		{
			int zone_delta = 0;

			Coord test_coord;
			test_coord.mode = CoordMode::UTM;

			test_coord.utm.zone = this->center.utm.zone;
			assert (UTM::is_band_letter(this->center.utm.get_band_letter())); /* TODO_2_LATER: add smarter error handling. In theory the source object should be valid and for sure contain valid band letter. */
			test_coord.utm.set_band_letter(this->center.utm.get_band_letter());
			test_coord.utm.easting = ((pos_x - (this->canvas.width_2)) * xmpp) + this->center.utm.easting;

			zone_delta = floor((test_coord.utm.easting - EASTING_OFFSET) / this->utm_zone_width + 0.5);

			test_coord.utm.zone += zone_delta;
			test_coord.utm.easting -= zone_delta * this->utm_zone_width;
			test_coord.utm.northing = (((this->canvas.height_2) - pos_y) * ympp) + this->center.utm.northing;


			if (coord.utm.zone != test_coord.utm.zone) {
				qDebug() << SG_PREFIX_E << "UTM: Zone calculation mismatch" << coord << test_coord << coord.utm.zone << test_coord.utm.zone;
			}
			if (coord.utm.easting != test_coord.utm.easting) {
				qDebug() << SG_PREFIX_E << "UTM: Easting calculation mismatch" << coord << test_coord << (coord.utm.easting - test_coord.utm.easting);
			}
			if (coord.utm.northing != test_coord.utm.northing) {
				qDebug() << SG_PREFIX_E << "UTM: Northing calculation mismatch" << coord << test_coord << (coord.utm.northing - test_coord.utm.northing);
			}
			if (coord.utm.get_band_letter() != test_coord.utm.get_band_letter()) {
				qDebug() << SG_PREFIX_E << "UTM: Band letter calculation mismatch" << coord << test_coord << coord.utm.get_band_letter() << test_coord.utm.get_band_letter();
			}
		}
		break;

	case CoordMode::LatLon:
		coord.mode = CoordMode::LatLon;

		switch (this->drawmode) {
		case ViewportDrawMode::LatLon:
			/* Modified (reformatted) formula. */
			{
				const int delta_x = pos_x - this->canvas.width_2;
				const int delta_y = this->canvas.height_2 - pos_y;

				coord.ll.lon = this->center.ll.lon + (delta_x / REVERSE_MERCATOR_FACTOR(xmpp));
				coord.ll.lat = this->center.ll.lat + (delta_y / REVERSE_MERCATOR_FACTOR(ympp));
			}

			/* Original code, used for comparison of results with new, reformatted formula. */
			{
				Coord test_coord;
				test_coord.mode = CoordMode::LatLon;

				test_coord.ll.lon = this->center.ll.lon + (180.0 * xmpp / 65536 / 256 * (pos_x - this->canvas.width_2));
				test_coord.ll.lat = this->center.ll.lat + (180.0 * ympp / 65536 / 256 * (this->canvas.height_2 - pos_y));

				if (coord.ll.lat != test_coord.ll.lat) {
					qDebug() << SG_PREFIX_E << "LatLon: Latitude calculation mismatch" << coord << test_coord << (coord.ll.lat - test_coord.ll.lat);
				}
				if (coord.ll.lon != test_coord.ll.lon) {
					qDebug() << SG_PREFIX_E << "LatLon: Longitude calculation mismatch" << coord << test_coord << (coord.ll.lon - test_coord.ll.lon);
				}
			}
			break;

		case ViewportDrawMode::Expedia:
			Expedia::screen_pos_to_lat_lon(coord.ll, pos_x, pos_y, this->center.ll, xmpp * ALTI_TO_MPP, ympp * ALTI_TO_MPP, this->canvas.width_2, this->canvas.height_2);
			break;

		case ViewportDrawMode::Mercator:
			/* This isn't called with a high frequently so less need to optimize. */
			/* Modified (reformatted) formula. */
			{
				const int delta_x = pos_x - this->canvas.width_2;
				const int delta_y = this->canvas.height_2 - pos_y;

				coord.ll.lon = this->center.ll.lon + (delta_x / REVERSE_MERCATOR_FACTOR(xmpp));
				coord.ll.lat = DEMERCLAT (MERCLAT(this->center.ll.lat) + (delta_y / (REVERSE_MERCATOR_FACTOR(ympp))));
			}

			/* Original code, used for comparison of results with new, reformatted formula. */
			{
				Coord test_coord;
				test_coord.mode = CoordMode::LatLon;

				test_coord.ll.lon = this->center.ll.lon + (180.0 * xmpp / 65536 / 256 * (pos_x - this->canvas.width_2));
				test_coord.ll.lat = DEMERCLAT (MERCLAT(this->center.ll.lat) + (180.0 * ympp / 65536 / 256 * (this->canvas.height_2 - pos_y)));

				if (coord.ll.lat != test_coord.ll.lat) {
					qDebug() << SG_PREFIX_E << "Mercator: Latitude calculation mismatch" << coord << test_coord << (coord.ll.lat - test_coord.ll.lat);
				} else {
					qDebug() << SG_PREFIX_I << "Mercator: OK Latitude" << coord << test_coord << (coord.ll.lat - test_coord.ll.lat);
				}
				if (coord.ll.lon != test_coord.ll.lon) {
					qDebug() << SG_PREFIX_E << "Mercator: Longitude calculation mismatch" << coord << test_coord << (coord.ll.lon - test_coord.ll.lon);
				} else {
					qDebug() << SG_PREFIX_I << "Mercator: OK Longitude" << coord << test_coord << (coord.ll.lon - test_coord.ll.lon);
				}
			}
			break;
		default:
			qDebug() << SG_PREFIX_E << "Unrecognized draw mode" << (int) this->drawmode;
		}
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unrecognized coord mode" << (int) this->coord_mode;
		break;

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
*/
void Viewport::coord_to_screen_pos(const Coord & coord_in, int * pos_x, int * pos_y) const
{
	Coord coord = coord_in;
	const double xmpp = this->viking_zoom_level.x;
	const double ympp = this->viking_zoom_level.y;

	if (coord_in.mode != this->coord_mode) {
		/* The intended use of the function is that coord_in
		   argument is always in correct coord mode (i.e. in
		   viewport's coord mode). If it is necessary to
		   convert the coord_in to proper coord mode, it is
		   done before the function call. Therefore if it
		   turns out that coord_in's coordinate mode is
		   different than viewport's coordinate mode, it's an
		   error. */
		qDebug() << SG_PREFIX_W << "Need to convert coord mode! This should never happen!";
		coord.change_mode(this->coord_mode);
	}

	switch (this->coord_mode) {
	case CoordMode::UTM:
		{
			const UTM * utm_center = &this->center.utm;
			const UTM * utm = &coord.utm;
			if (utm_center->zone != utm->zone && this->is_one_utm_zone){
				*pos_x = *pos_y = VIK_VIEWPORT_UTM_WRONG_ZONE;
				return;
			}

			*pos_x = ((utm->easting - utm_center->easting) / xmpp) + (this->canvas.width_2) -
				(utm_center->zone - utm->zone) * this->utm_zone_width / xmpp;
			*pos_y = (this->canvas.height_2) - ((utm->northing - utm_center->northing) / ympp);
		}
		break;

	case CoordMode::LatLon:
		switch (this->drawmode) {
		case ViewportDrawMode::LatLon:
			*pos_x = this->canvas.width_2 + (MERCATOR_FACTOR(xmpp) * (coord.ll.lon - this->center.ll.lon));
			*pos_y = this->canvas.height_2 + (MERCATOR_FACTOR(ympp) * (this->center.ll.lat - coord.ll.lat));
			break;
		case ViewportDrawMode::Expedia:
			{
				double xx, yy;
				Expedia::lat_lon_to_screen_pos(&xx, &yy, this->center.ll, coord.ll, xmpp * ALTI_TO_MPP, ympp * ALTI_TO_MPP, this->canvas.width_2, this->canvas.height_2);
				*pos_x = xx;
				*pos_y = yy;
			}
			break;
		case ViewportDrawMode::Mercator:
			*pos_x = this->canvas.width_2 + (MERCATOR_FACTOR(xmpp) * (coord.ll.lon - this->center.ll.lon));
			*pos_y = this->canvas.height_2 + (MERCATOR_FACTOR(ympp) * (MERCLAT(this->center.ll.lat) - MERCLAT(coord.ll.lat)));
			break;
		default:
			qDebug() << SG_PREFIX_E << "Unexpected viewport drawing mode" << (int) this->drawmode;
			break;
		}
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected viewport coord mode" << (int) this->coord_mode;
		break;
	}
}




ScreenPos Viewport::coord_to_screen_pos(const Coord & coord_in) const
{
	ScreenPos pos;
	this->coord_to_screen_pos(coord_in, &pos.x, &pos.y);
	return pos;
}




/*
  Since this function is used for every drawn trackpoint - it can get called a lot.
  Thus x & y position factors are calculated once on zoom changes,
  avoiding the need to do it here all the time.
*/
void Viewport::lat_lon_to_screen_pos(const LatLon & lat_lon, int * pos_x, int * pos_y) const
{
	const double xmpp = this->viking_zoom_level.x;
	const double ympp = this->viking_zoom_level.y;

	switch (this->drawmode) {
	case ViewportDrawMode::LatLon:
		*pos_x = this->canvas.width_2 + (MERCATOR_FACTOR(xmpp) * (lat_lon.lon - this->center.ll.lon));
		*pos_y = this->canvas.height_2 + (MERCATOR_FACTOR(ympp) * (this->center.ll.lat - lat_lon.lat));
		break;
	case ViewportDrawMode::Expedia:
		{
			double xx,yy;
			Expedia::lat_lon_to_screen_pos(&xx, &yy, this->center.ll, lat_lon, xmpp * ALTI_TO_MPP, ympp * ALTI_TO_MPP, this->canvas.width_2, this->canvas.height_2);
			*pos_x = xx;
			*pos_y = yy;
		}
		break;
	case ViewportDrawMode::Mercator:
		*pos_x = this->canvas.width_2 + (MERCATOR_FACTOR(xmpp) * (lat_lon.lon - this->center.ll.lon));
		*pos_y = this->canvas.height_2 + (MERCATOR_FACTOR(ympp) * (MERCLAT(this->center.ll.lat) - MERCLAT(lat_lon.lat)));
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected viewport drawing mode" << (int) this->drawmode;
		break;
	}
}




ScreenPos Viewport::lat_lon_to_screen_pos(const LatLon & lat_lon) const
{
	ScreenPos pos;
	this->lat_lon_to_screen_pos(lat_lon, &pos.x, &pos.y);
	return pos;
}




/*
  Since this function is used for every drawn trackpoint - it can get called a lot.
  Thus x & y position factors are calculated once on zoom changes,
  avoiding the need to do it here all the time.
*/
void Viewport::utm_to_screen_pos(const UTM & utm, int * pos_x, int * pos_y) const
{
	const double xmpp = this->viking_zoom_level.x;
	const double ympp = this->viking_zoom_level.y;

	const UTM * utm_center = &this->center.utm;

	if (utm_center->zone != utm.zone && this->is_one_utm_zone){
		*pos_x = *pos_y = VIK_VIEWPORT_UTM_WRONG_ZONE;
		return;
	}

	*pos_x = ((utm.easting - utm_center->easting) / xmpp) + (this->canvas.width_2) -
		(utm_center->zone - utm.zone) * this->utm_zone_width / xmpp;
	*pos_y = (this->canvas.height_2) - ((utm.northing - utm_center->northing) / ympp);
}




ScreenPos Viewport::utm_to_screen_pos(const UTM & utm) const
{
	ScreenPos pos;
	this->utm_to_screen_pos(utm, &pos.x, &pos.y);
	return pos;
}




/* Clip functions continually reduce the value by a factor until it is in the acceptable range
   whilst also scaling the other coordinate value. */
static void clip_x(int * x1, int * y1, int * x2, int * y2)
{
	while (std::abs(*x1) > 32768) {
		*x1 = *x2 + (0.5 * (*x1 - *x2));
		*y1 = *y2 + (0.5 * (*y1 - *y2));
	}
}




static void clip_y(int * x1, int * y1, int * x2, int * y2)
{
	while (std::abs(*y1) > 32767) {
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
	if (begin_x > this->canvas.width && end_x > this->canvas.width) {
		return;
	}
	if (begin_y > this->canvas.height && end_y > this->canvas.height) {
		return;
	}

	/*** Clipping, yeah! ***/
	Viewport::clip_line(&begin_x, &begin_y, &end_x, &end_y);

	//QPainter painter(this->canvas.pixmap);
	this->canvas.painter->setPen(pen);
	this->canvas.painter->drawLine(begin_x, begin_y,
				       end_x,   end_y);
}





void Viewport2D::central_draw_line(const QPen & pen, int begin_x, int begin_y, int end_x, int end_y)
{
	if ((begin_x < 0 && end_x < 0) || (begin_y < 0 && end_y < 0)) {
		return;
	}
	if (begin_x > this->central->canvas.width && end_x > this->central->canvas.width) {
		return;
	}
	if (begin_y > this->central->canvas.height && end_y > this->central->canvas.height) {
		return;
	}

	// fprintf(stderr, "Called to draw line between points (%d %d) and (%d %d) (canvas height = %d)\n", begin_x, begin_y, end_x, end_y, this->canvas.height);

	/*** Clipping, yeah! ***/
	//Viewport::clip_line(&begin_x, &begin_y, &end_x, &end_y);

	/* x/y coordinates are converted here from "beginning in
	   bottom-left corner" to "beginning in upper-left corner"
	   coordinate system. */
	const int h = this->central->canvas.height;

	this->central->canvas.painter->setPen(pen);
	this->central->canvas.painter->drawLine(begin_x, h - begin_y - 1,
						end_x,   h - end_y - 1);
}




void Viewport::draw_rectangle(const QPen & pen, int upper_left_x, int upper_left_y, int rect_width, int rect_height)
{
	/* Using 32 as half the default waypoint image size, so this draws ensures the highlight gets done. */
	if (upper_left_x > -32 && upper_left_x < this->canvas.width + 32 && upper_left_y > -32 && upper_left_y < this->canvas.height + 32) {

		//QPainter painter(this->canvas.pixmap);
		this->canvas.painter->setPen(pen);
		this->canvas.painter->drawRect(upper_left_x, upper_left_y, rect_width, rect_height);
	}
}




void Viewport::draw_rectangle(const QPen & pen, const QRect & rect)
{
	/* Using 32 as half the default waypoint image size, so this draws ensures the highlight gets done. */
	if (rect.x() > -32 && rect.x() < this->canvas.width + 32 && rect.y() > -32 && rect.y() < this->canvas.height + 32) {

		//QPainter painter(this->canvas.pixmap);
		this->canvas.painter->setPen(pen);
		this->canvas.painter->drawRect(rect);
	}
}




void Viewport::fill_rectangle(const QColor & color, int pos_x, int pos_y, int rect_width, int rect_height)
{
	/* Using 32 as half the default waypoint image size, so this draws ensures the highlight gets done. */
	if (pos_x > -32 && pos_x < this->canvas.width + 32 && pos_y > -32 && pos_y < this->canvas.height + 32) {

		//QPainter painter(this->canvas.pixmap);
		this->canvas.painter->fillRect(pos_x, pos_y, rect_width, rect_height, color);
	}
}




void Viewport::draw_text(QFont const & text_font, QPen const & pen, int pos_x, int pos_y, QString const & text)
{
	if (pos_x > -100 && pos_x < this->canvas.width + 100 && pos_y > -100 && pos_y < this->canvas.height + 100) {
		//QPainter painter(this->canvas.pixmap);
		this->canvas.painter->setPen(pen);
		this->canvas.painter->setFont(text_font);
		this->canvas.painter->drawText(pos_x, pos_y, text);
	}
}




void Viewport::draw_text(const QFont & text_font, const QPen & pen, const QRectF & bounding_rect, int flags, const QString & text, int text_offset)
{
	//QPainter painter(this->canvas.pixmap);
	this->canvas.painter->setFont(text_font);

	/* "Normalize" bounding rectangles that have negative width or height.
	   Otherwise the text will be outside of the bounding rectangle. */
	QRectF final_bounding_rect = bounding_rect.united(bounding_rect);

	QRectF text_rect = this->canvas.painter->boundingRect(final_bounding_rect, flags, text);
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
	this->canvas.painter->setPen(QColor("red"));
	this->canvas.painter->drawEllipse(bounding_rect.left(), bounding_rect.top(), 3, 3);

	this->canvas.painter->setPen(QColor("darkgreen"));
	this->canvas.painter->drawRect(bounding_rect);

	this->canvas.painter->setPen(QColor("red"));
	this->canvas.painter->drawRect(text_rect);
#endif

	this->canvas.painter->setPen(pen);
	this->canvas.painter->drawText(text_rect, flags, text, NULL);
}




void Viewport2D::margin_draw_text(ViewportMargin::Position pos, const QFont & text_font, const QPen & pen, const QRectF & bounding_rect, int flags, const QString & text, int text_offset)
{
	qDebug() << SG_PREFIX_I << "Will draw label" << text;
	ViewportCanvas * canv = NULL;

	switch (pos) {
	case ViewportMargin::Position::Left:
		canv = &this->left->canvas;
		break;
	case ViewportMargin::Position::Right:
		canv = &this->right->canvas;
		break;
	case ViewportMargin::Position::Top:
		canv = &this->top->canvas;
		break;
	case ViewportMargin::Position::Bottom:
		canv = &this->bottom->canvas;
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled margin position";
		break;
	}

	if (!canv) {
		qDebug() << SG_PREFIX_E << "No margin canvas selected";
		return;
	}
	if (!canv->painter) {
		qDebug() << SG_PREFIX_W << "Canvas has no painter (yet?)";
		return;
	}


	canv->painter->setFont(text_font);

	/* "Normalize" bounding rectangles that have negative width or height.
	   Otherwise the text will be outside of the bounding rectangle. */
	QRectF final_bounding_rect = bounding_rect.united(bounding_rect);

	QRectF text_rect = canv->painter->boundingRect(final_bounding_rect, flags, text);
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
	canv->painter->setPen(QColor("red"));
	canv->painter->drawEllipse(bounding_rect.left(), bounding_rect.top(), 3, 3);

	canv->painter->setPen(QColor("darkgreen"));
	canv->painter->drawRect(bounding_rect);

	canv->painter->setPen(QColor("red"));
	canv->painter->drawRect(text_rect);
#endif

	canv->painter->setPen(pen);
	canv->painter->drawText(text_rect, flags, text, NULL);

}


void Viewport::draw_outlined_text(QFont const & text_font, QPen const & outline_pen, const QColor & fill_color, const QPointF & base_point, QString const & text)
{
	/* http://doc.qt.io/qt-5/qpainterpath.html#addText */

	this->canvas.painter->setPen(outline_pen);
	this->canvas.painter->setBrush(QBrush(fill_color));

	QPainterPath path;
	path.addText(base_point, text_font, text);

	this->canvas.painter->drawPath(path);

	/* Reset painter. */
	this->canvas.painter->setPen(QPen());
	this->canvas.painter->setBrush(QBrush());
}




void Viewport::draw_text(QFont const & text_font, QPen const & pen, const QColor & bg_color, const QRectF & bounding_rect, int flags, const QString & text, int text_offset)
{
	//QPainter painter(this->canvas.pixmap);
	this->canvas.painter->setFont(text_font);

	/* "Normalize" bounding rectangles that have negative width or height.
	   Otherwise the text will be outside of the bounding rectangle. */
	QRectF final_bounding_rect = bounding_rect.united(bounding_rect);

	QRectF text_rect = this->canvas.painter->boundingRect(final_bounding_rect, flags, text);
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
	this->canvas.painter->setPen(QColor("red"));
	this->canvas.painter->drawEllipse(bounding_rect.left(), bounding_rect.top(), 3, 3);

	this->canvas.painter->setPen(QColor("darkgreen"));
	this->canvas.painter->drawRect(bounding_rect);
#endif

	/* A highlight of drawn text, must be executed before .drawText(). */
	this->canvas.painter->fillRect(text_rect, bg_color);

	this->canvas.painter->setPen(pen);
	this->canvas.painter->drawText(text_rect, flags, text, NULL);
}




void Viewport::draw_pixmap(QPixmap const & pixmap, int viewport_x, int viewport_y, int pixmap_x, int pixmap_y, int pixmap_width, int pixmap_height)
{
	//QPainter painter(this->canvas.pixmap);
	this->canvas.painter->drawPixmap(viewport_x, viewport_y, pixmap, pixmap_x, pixmap_y, pixmap_width, pixmap_height);
}




void Viewport::draw_pixmap(QPixmap const & pixmap, int viewport_x, int viewport_y)
{
	//QPainter painter(this->canvas.pixmap);
	this->canvas.painter->drawPixmap(viewport_x, viewport_y, pixmap);
}




void Viewport::draw_pixmap(QPixmap const & pixmap, const QRect & viewport_rect, const QRect & pixmap_rect)
{
	//QPainter painter(this->canvas.pixmap);
	this->canvas.painter->drawPixmap(viewport_rect, pixmap, pixmap_rect);
}




void Viewport::draw_bbox(const LatLonBBox & bbox, const QPen & pen)
{
	if (!BBOX_INTERSECT(bbox, this->get_bbox())) {
		qDebug() << SG_PREFIX_I << "Not drawing bbox" << bbox << ", does not intersects with viewport bbox" << this->get_bbox();
		return;
	}


	ScreenPos sp_sw = this->coord_to_screen_pos(Coord(LatLon(bbox.south, bbox.west), this->coord_mode));
	ScreenPos sp_ne = this->coord_to_screen_pos(Coord(LatLon(bbox.north, bbox.east), this->coord_mode));

	if (sp_sw.x < 0) {
		sp_sw.x = 0;
	}

	if (sp_ne.y < 0) {
		sp_ne.y = 0;
	}

	this->draw_rectangle(pen, sp_sw.x, sp_ne.y, sp_ne.x - sp_sw.x, sp_sw.y - sp_ne.y);

	return;
}



void Viewport::draw_arc(QPen const & pen, int center_x, int center_y, int size_w, int size_h, int start_angle, int span_angle)
{
	this->canvas.painter->setPen(pen);
	this->canvas.painter->drawArc(center_x, center_y, size_w, size_h, start_angle, span_angle * 16);
}




void Viewport::draw_ellipse(QPen const & pen, const QPoint & ellipse_center, int radius_x, int radius_y, bool filled)
{
	/* TODO_LATER: see if there is a difference in outer size of ellipse drawn with and without a filling. */

	if (filled) {
		/* Set brush to fill ellipse. */
		this->canvas.painter->setBrush(QBrush(pen.color()));
	} else {
		this->canvas.painter->setPen(pen);
	}

	this->canvas.painter->drawEllipse(ellipse_center, radius_x, radius_y);

	if (filled) {
		/* Reset brush. */
		this->canvas.painter->setBrush(QBrush());
	}
}




void Viewport::draw_polygon(QPen const & pen, QPoint const * points, int npoints, bool filled)
{
	//QPainter painter(this->canvas.pixmap);

	if (filled) {
		QPainterPath path;
		path.moveTo(points[0]);
		for (int i = 1; i < npoints; i++) {
			path.lineTo(points[i]);
		}

		this->canvas.painter->setPen(Qt::NoPen);
		this->canvas.painter->fillPath(path, QBrush(QColor(pen.color())));
	} else {
		this->canvas.painter->setPen(pen);
		this->canvas.painter->drawPolygon(points, npoints);
	}
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




bool Viewport::get_is_one_utm_zone(void) const
{
	return coord_mode == CoordMode::UTM && this->is_one_utm_zone;
}




void Viewport::set_drawmode(ViewportDrawMode new_drawmode)
{
	this->drawmode = new_drawmode;

	if (new_drawmode == ViewportDrawMode::UTM) {
		this->set_coord_mode(CoordMode::UTM);
	} else {
		this->set_coord_mode(CoordMode::LatLon);
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
	qDebug() << SG_PREFIX_I << "Save snapshot";
	*this->canvas.snapshot_buffer = *this->canvas.pixmap;
}




void Viewport::snapshot_load(void)
{
	qDebug() << SG_PREFIX_I << "Load snapshot";
	*this->canvas.pixmap = *this->canvas.snapshot_buffer;
}




void Viewport::set_half_drawn(bool new_half_drawn)
{
	this->half_drawn = new_half_drawn;
}




bool Viewport::get_half_drawn(void) const
{
	return this->half_drawn;
}




LatLonBBox Viewport::get_bbox(int margin_left, int margin_right, int margin_top, int margin_bottom) const
{
	Coord tleft =  this->screen_pos_to_coord(margin_left,                       margin_top);
	Coord tright = this->screen_pos_to_coord(this->canvas.width + margin_right, margin_top);
	Coord bleft =  this->screen_pos_to_coord(margin_left,                       this->canvas.height + margin_bottom);
	Coord bright = this->screen_pos_to_coord(this->canvas.width + margin_right, this->canvas.height + margin_bottom);

	tleft.change_mode(CoordMode::LatLon);
	tright.change_mode(CoordMode::LatLon);
	bleft.change_mode(CoordMode::LatLon);
	bright.change_mode(CoordMode::LatLon);

	LatLonBBox bbox;
	bbox.north = std::max(tleft.ll.lat, tright.ll.lat);
	bbox.south = std::min(bleft.ll.lat, bright.ll.lat);
	bbox.east  = std::max(tright.ll.lon, bright.ll.lon);
	bbox.west  = std::min(tleft.ll.lon, bleft.ll.lon);
	bbox.validate();

	return bbox;
}




/**
   \brief Add an attribution/copyright to display on viewport

   @copyright: new attribution/copyright to display.
*/
sg_ret Viewport::add_attribution(QString const & attribution)
{
	return this->decorations.add_attribution(attribution);
}




sg_ret Viewport::add_logo(const ViewportLogo & logo)
{
	return this->decorations.add_logo(logo);
}




/**
 * @x1: screen coord
 * @y1: screen coord
 * @x2: screen coord
 * @y2: screen coord
 * @angle: bearing in Radian (output)
 * @base_angle: UTM base angle in Radian (output)
 *
 * Compute bearing.
 */
void Viewport::compute_bearing(int x1, int y1, int x2, int y2, Angle & angle, Angle & base_angle)
{
	double len = sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
	double dx = (x2 - x1) / len * 10;
	double dy = (y2 - y1) / len * 10;

	angle.value = atan2(dy, dx) + M_PI_2;

	if (this->get_drawmode() == ViewportDrawMode::UTM) {

		Coord test = this->screen_pos_to_coord(x1, y1);
		LatLon ll = test.get_latlon();
		ll.lat += this->get_viking_zoom_level().y * this->canvas.height / 11000.0; // about 11km per degree latitude

		test = Coord(LatLon::to_utm(ll), CoordMode::UTM);
		const ScreenPos test_pos = this->coord_to_screen_pos(test);

		base_angle.value = M_PI - atan2(test_pos.x - x1, test_pos.y - y1);
		angle.value -= base_angle.value;
	}

	if (angle.value < 0) {
		angle.value += 2 * M_PI;
	}

	if (angle.value > 2 * M_PI) {
		angle.value -= 2 * M_PI;
	}
}




void Viewport::paintEvent(QPaintEvent * ev)
{
	qDebug() << SG_PREFIX_I;

	QPainter painter(this);

	painter.drawPixmap(0, 0, *this->canvas.pixmap);

	painter.setPen(Qt::blue);
	painter.setFont(QFont("Arial", 30));
	painter.drawText(this->rect(), Qt::AlignCenter, "SlavGPS");

	return;
}




void Viewport::resizeEvent(QResizeEvent * ev)
{
	qDebug() << SG_PREFIX_I;

	this->reconfigure_drawing_area();
	ThisApp::get_main_window()->draw_tree_items();
	//this->draw_scale();

	return;
}





void Viewport::mousePressEvent(QMouseEvent * ev)
{
	qDebug() << SG_PREFIX_I << "Mouse CLICK event, button" << (int) ev->button();

	this->window->get_toolbox()->handle_mouse_click(ev);

	ev->accept();
}




bool Viewport::eventFilter(QObject * object, QEvent * ev)
{
	if (ev->type() == QEvent::MouseButtonDblClick) {
		QMouseEvent * m = (QMouseEvent *) ev;
		qDebug() << SG_PREFIX_I << "Mouse DOUBLE CLICK event, button" << (int) m->button();

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

	this->window->get_toolbox()->handle_mouse_move(ev);

	emit this->cursor_moved(this, ev);

	ev->accept();
}




void Viewport::mouseReleaseEvent(QMouseEvent * ev)
{
	qDebug() << SG_PREFIX_I << "called with button" << (int) ev->button();

	this->window->get_toolbox()->handle_mouse_release(ev);
	emit this->button_released(this, ev);

	ev->accept();
}




void Viewport::wheelEvent(QWheelEvent * ev)
{
	QPoint angle = ev->angleDelta();
	qDebug() << SG_PREFIX_I << "Wheel event, buttons =" << (int) ev->buttons() << "angle =" << angle.y();
	ev->accept();

	const Qt::KeyboardModifiers modifiers = ev->modifiers();

	const int w = this->canvas.width;
	const int h = this->canvas.height;
	const bool scroll_up = angle.y() > 0;


	switch (modifiers) {
	case Qt::ControlModifier:
		/* Control == pan up & down. */
		if (scroll_up) {
			this->set_center_from_screen_pos(w / 2, h / 3);
		} else {
			this->set_center_from_screen_pos(w / 2, h * 2 / 3);
		}
		break;

	case Qt::ShiftModifier:
		/* Shift == pan left & right. */
		if (scroll_up) {
			this->set_center_from_screen_pos(w / 3, h / 2);
		} else {
			this->set_center_from_screen_pos(w * 2 / 3, h / 2);
		}
		break;

	case (Qt::ControlModifier | Qt::ShiftModifier):
		/* This zoom is on the center position. */
		if (scroll_up) {
			this->zoom_in();
		} else {
			this->zoom_out();
		}
		break;
	case Qt::NoModifier: {
		const ScreenPos center_pos(this->canvas.width / 2, this->canvas.height / 2);
		const ScreenPos event_pos(ev->x(), ev->y());
		const ZoomOperation zoom_operation = SlavGPS::wheel_event_to_zoom_operation(ev);

		/* Clicked coordinate will be put after zoom at the same
		   position in viewport as before zoom.  Before zoom
		   the coordinate was under cursor, and after zoom it
		   will be still under cursor. */
		ViewportZoom::keep_coordinate_under_cursor(zoom_operation, this, this->window, event_pos, center_pos);
	}
	default:
		/* Other modifier. Just ignore. */
		break;
	};

	qDebug() << SG_PREFIX_I << "Will call Window::draw_tree_items()";
	this->window->draw_tree_items();
}




void Viewport::draw_mouse_motion_cb(QMouseEvent * ev)
{
	QPoint position = this->mapFromGlobal(QCursor::pos());

#if 0   /* Verbose debug. */
	qDebug() << SG_PREFIX_I << "Difference in cursor position: dx = " << position.x() - ev->x() << ", dy = " << position.y() - ev->y();
#endif

	const int pos_x = position.x();
	const int pos_y = position.y();

#ifdef K_FIXME_RESTORE
	this->window->tb->move(ev);
#endif

	/* Get coordinates in viewport's coordinates mode. */
	Coord coord = this->screen_pos_to_coord(pos_x, pos_y);
	const QString coord_string = coord.to_string();

#if 0   /* Verbose debug. */
	qDebug() << SG_PREFIX_D << "Mouse motion: cursor pos:" << position << ", coordinates:" << first << second;
#endif

	/* Change interpolate method according to scale. */
	double zoom = this->get_viking_zoom_level().get_x();
	DemInterpolation interpol_method;
	if (zoom > 2.0) {
		interpol_method = DemInterpolation::None;
	} else if (zoom >= 1.0) {
		interpol_method = DemInterpolation::Simple;
	} else {
		interpol_method = DemInterpolation::Best;
	}

	const Altitude altitude = DEMCache::get_elev_by_coord(coord, interpol_method);

	QString message;
	if (altitude.is_valid()) {
		message = QObject::tr("%1 %2").arg(coord_string).arg(altitude.convert_to_unit(Preferences::get_unit_height()).to_string());
	} else {
		message = coord_string;
	}

	this->window->get_statusbar()->set_message(StatusBarField::Position, message);

#ifdef K_FIXME_RESTORE
	this->window->pan_move(ev);
#endif
}




#ifdef K_OLD_IMPLEMENTATION
/* No longer used. */
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
		snprintf(*lat, 4, "%d%c", utm.zone, utm.get_band_letter());
		*lon = (char *) malloc(16*sizeof(char));
		snprintf(*lon, 16, "%d %d", (int)utm.easting, (int)utm.northing);
	} else {
		const LatLon lat_lon = UTM::to_latlon(utm);
		LatLon::to_strings(lat_lon, lat, lon);
	}
}
#endif




Viewport * Viewport::create_scaled_viewport(Window * a_window, int target_width, int target_height, const VikingZoomLevel & expected_viking_zoom_level)
{
	/*
	  We always want to print original viewport in its
	  fullness. If necessary, we should scale our drawing so that
	  it always covers the most of target device, but we want to
	  keep proportions of original viewport. The question is how
	  much should we scale our drawing?

	  We need to look at how shapes of original viewport and
	  target device match. In the example below the original
	  viewport is more "height-ish" than target device. We will
	  want to scale the drawing from original viewport by factor of
	  ~2 (target device height / original height) before drawing it to
	  target device. Scaling drawing from original viewport in
	  this example to match width of target device would make
	  the drawing too tall.

	   +--------------------------------+
	   |                                |
	   |   +-----+                      |
	   |   |     |original viewport     |
	   |   |     |                      |
	   |   |     |                      |target device
	   |   +-----+                      |
	   |                                |
	   |                                |
	   +--------------------------------+

	   TODO_HARD: make sure that this works also for target
	   device smaller than original viewport.
	*/

	qDebug() << SG_PREFIX_I << "Expected image size =" << target_width << target_height;

	Viewport * scaled_viewport = new Viewport(a_window);

	const int orig_width = this->canvas.width;
	const int orig_height = this->canvas.height;

	const double orig_factor = 1.0 * orig_width / orig_height;
	const double target_factor = 1.0 * target_width / target_height;

	double scale_factor = 0.0;
	if (orig_factor > target_factor) {
		/* Original viewport is more "wide-ish" than target device. */
		scale_factor = 1.0 * target_width / orig_width;
	} else {
		/* Original viewport is more "height-ish" than target device. */
		scale_factor = 1.0 * target_height / orig_height;
	}



	/* Copy/set selected properties of viewport. */
	scaled_viewport->set_drawmode(this->get_drawmode());
	scaled_viewport->set_coord_mode(this->get_coord_mode());
	scaled_viewport->set_center_from_coord(this->center, false);



	/* Set zoom - either explicitly passed to the function, or
	   calculated implicitly. */
	if (expected_viking_zoom_level.is_valid()) {
		scaled_viewport->set_viking_zoom_level(expected_viking_zoom_level);
	} else {
		const VikingZoomLevel calculated_viking_zoom_level(this->viking_zoom_level.x / scale_factor, this->viking_zoom_level.y / scale_factor);
		if (calculated_viking_zoom_level.is_valid()) {
			scaled_viewport->set_viking_zoom_level(calculated_viking_zoom_level);
		} else {
			/* TODO_HARD: now what? */
		}
	}



	snprintf(scaled_viewport->canvas.debug, sizeof (scaled_viewport->canvas.debug), "%s", "Scaled Viewport");

	/* Notice that we configure size of the print viewport using
	   size of scaled source, not size of target device (i.e. not
	   of target paper or target image). The image that we will
	   print to target device should cover the same area
	   (i.e. have the same bounding box) as original viewport. */
	scaled_viewport->reconfigure_drawing_area(orig_width * scale_factor, orig_height * scale_factor);

	qDebug() << SG_PREFIX_I << "Original viewport's bbox =" << this->get_bbox();
	qDebug() << SG_PREFIX_I << "Scaled viewport's bbox =  " << scaled_viewport->get_bbox();
	scaled_viewport->set_bbox(this->get_bbox());
	qDebug() << SG_PREFIX_I << "Scaled viewport's bbox =  " << scaled_viewport->get_bbox();

	return scaled_viewport;
}




bool Viewport::print_cb(QPrinter * printer)
{
	const QRectF page_rect = printer->pageRect(QPrinter::DevicePixel);
	const QRectF paper_rect = printer->paperRect(QPrinter::DevicePixel);

	qDebug() << SG_PREFIX_I << "---- Printer Info ----";
	qDebug() << SG_PREFIX_I << "printer name:" << printer->printerName();
	qDebug() << SG_PREFIX_I << "page rectangle:" << printer->pageRect(QPrinter::DevicePixel);
	qDebug() << SG_PREFIX_I << "paper rectangle:" << printer->paperRect(QPrinter::DevicePixel);
	qDebug() << SG_PREFIX_I << "resolution:" << printer->resolution();
	qDebug() << SG_PREFIX_I << "supported resolutions:" << printer->supportedResolutions();

	qDebug() << SG_PREFIX_I << "---- Page Layout ----";
	QPageLayout layout = printer->pageLayout();
	qDebug() << SG_PREFIX_I << "full rectangle (points):" << layout.fullRect(QPageLayout::Point);
	qDebug() << SG_PREFIX_I << "paint rectangle (points):" << layout.paintRect(QPageLayout::Point);
	qDebug() << SG_PREFIX_I << "margins (points):" << layout.margins(QPageLayout::Point);

	QPageLayout::Orientation orientation = layout.orientation();
	switch (orientation) {
	case QPrinter::Portrait:
		qDebug() << SG_PREFIX_I << "orientation: Portrait";
		break;
	case QPrinter::Landscape:
		qDebug() << SG_PREFIX_I << "orientation: Landscape";
		break;
	default:
		qDebug() << SG_PREFIX_I << "orientation: unknown";
		break;
	}

	const QRectF target_rect(page_rect);
	const int target_width = target_rect.width();
	const int target_height = target_rect.height();

	Viewport * scaled_viewport = this->create_scaled_viewport(this->window, target_width, target_height, VikingZoomLevel());

	/* Since we are printing viewport as it is, we allow existing
	   highlights to be drawn to print canvas. */
	ThisApp::get_layers_panel()->draw_tree_items(scaled_viewport, true, false);


	QPainter printer_painter;
	printer_painter.begin(printer);
	QPoint paint_begin; /* Beginning of rectangle, into which we will paint in target device. */
	//paint_begin.setX((target_width / 2.0) - (scaled_viewport->canvas.width / 2.0));
	//paint_begin.setY((target_height / 2.0) - (scaled_viewport->canvas.height / 2.0));
	paint_begin.setX(0);
	paint_begin.setY(0);

	printer_painter.drawPixmap(paint_begin, *scaled_viewport->canvas.pixmap);
	printer_painter.end();

	delete scaled_viewport;

	qDebug() << SG_PREFIX_I << "target rectangle:" << target_rect;
	qDebug() << SG_PREFIX_I << "paint_begin:" << paint_begin;


	return true;
}




/*
  @pos is a position in viewport's canvas.
  The function makes sure that the crosshair is drawn only inside of graph area.
*/
void Viewport2D::central_draw_simple_crosshair(const ScreenPos & pos)
{
	const int w = this->central->canvas.width;
	const int h = this->central->canvas.height;
	const int x = pos.x;
	const int y = h - pos.y - 1; /* Convert from "beginning in bottom-left corner" to "beginning in top-left corner" coordinate system. */

	qDebug() << SG_PREFIX_I << "Crosshair at" << x << y;

	if (x > w || y > h) {
		/* Position outside of graph area. */
		return;
	}

	this->central->canvas.painter->setPen(this->central->marker_pen);

	/* Small optimization: use QT's drawing primitives directly.
	   Remember that (0,0) screen position is in upper-left corner of viewport. */

	this->central->canvas.painter->drawLine(0, y, w, y); /* Horizontal line. */
	this->central->canvas.painter->drawLine(x, 0, x, h); /* Vertical line. */
}




void ScreenPos::set(int new_x, int new_y)
{
	this->x = new_x;
	this->y = new_y;
}




void ScreenPos::set(double new_x, double new_y)
{
	this->x = new_x;
	this->y = new_y;
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
	case ViewportDrawMode::Expedia:
		return QObject::tr("&Expedia Mode");
	case ViewportDrawMode::Mercator:
		return QObject::tr("&Mercator Mode");
	case ViewportDrawMode::LatLon:
		return QObject::tr("&Lat/Lon Mode");
	default:
		qDebug() << SG_PREFIX_E << "Unexpected draw mode" << (int) mode;
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
	case ViewportDrawMode::Expedia:
		mode_id_string = "expedia";
		break;
	case ViewportDrawMode::Mercator:
		mode_id_string = "mercator";
		break;
	case ViewportDrawMode::LatLon:
		mode_id_string = "latlon";
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected draw mode" << (int) mode;
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
		viewport->set_drawmode(ViewportDrawMode::Expedia);

	} else if (0 == strcasecmp(line, "google")) {
		success = false;
		qDebug() << SG_PREFIX_W << QObject::tr("Read file: draw mode 'google' no longer supported");

	} else if (0 == strcasecmp(line, "kh")) {
		success = false;
		qDebug() << SG_PREFIX_W << QObject::tr("Read file: draw mode 'kh' no more supported");

	} else if (0 == strcasecmp(line, "mercator")) {
		viewport->set_drawmode(ViewportDrawMode::Mercator);

	} else if (0 == strcasecmp(line, "latlon")) {
		viewport->set_drawmode(ViewportDrawMode::LatLon);
	} else {
		qDebug() << SG_PREFIX_E << QObject::tr("Read file: unexpected draw mode") << line;
		success = false;
	}

	return success;
}




sg_ret Viewport::set_bbox(const LatLonBBox & new_bbox)
{
	return ViewportZoom::zoom_to_show_bbox(this, this->get_coord_mode(), new_bbox);
}




void Viewport::request_redraw(const QString & trigger_descr)
{
	this->emit_center_or_zoom_changed(trigger_descr);
}




/* Tell QT what type of MIME data we accept. */
void Viewport::dragEnterEvent(QDragEnterEvent * event)
{
	if (event->mimeData()->hasFormat("text/plain")) {
		event->acceptProposedAction();
	}

}




void Viewport::dropEvent(QDropEvent * event)
{
	const QString & text = event->mimeData()->text();

	qDebug() << SG_PREFIX_I << "--------- drop event with text" << text;

	/* If our parent window has enabled dropping, it needs to be able to handle dropped data. */
	if (text.length()) {
		if (this->window->save_on_dirty_flag()) {
			this->window->open_file(text, false);
		}
	}

	event->acceptProposedAction();
}




bool Viewport::is_ready(void) const
{
	return this->canvas.pixmap != NULL;
}




/**
   To be called when action initiated in Viewport has changed center
   of viewport or zoom of viewport.
*/
void Viewport::emit_center_or_zoom_changed(const QString & trigger_name)
{
	qDebug() << SG_PREFIX_SIGNAL << "Will emit 'center or zoom changed' signal triggered by" << trigger_name;
	emit this->center_or_zoom_changed();
}




ViewportCanvas::ViewportCanvas()
{
}




ViewportCanvas::~ViewportCanvas()
{
	/* Painter must be deleted before paint device, otherwise
	   destructor of the paint device will complain. */
	delete this->painter;
	delete this->pixmap;
}




void ViewportCanvas::reconfigure(int new_width, int new_height)
{
	/* We don't handle situation when size of the pixmap doesn't
	   change.  Such situation will occur rarely, and adding a
	   code path to handle this special case would bring little
	   gain. */

	qDebug() << SG_PREFIX_N << this->debug << "*** Canvas is being reconfigured with size" << new_width << new_height << "***";

	if (this->width == new_width && this->height == new_height) {
		qDebug() << SG_PREFIX_I << this->debug << "Canvas not reconfigured: size didn't change";
		return;
	}

	this->width = new_width;
	this->height = new_height;

	this->width_2 = this->width / 2;
	this->height_2 = this->height / 2;

	if (this->pixmap) {
		qDebug() << SG_PREFIX_I << this->debug << "Deleting old canvas pixmap";
		/* Painter must be deleted before paint device, otherwise
		   destructor of the paint device will complain. */
		delete this->painter;
		delete this->pixmap;
	}

	qDebug() << SG_PREFIX_I << this->debug << "Creating new canvas pixmap with size" << this->width << this->height;
	this->pixmap = new QPixmap(this->width, this->height);
	this->pixmap->fill();

	this->painter = new QPainter(this->pixmap);


	/* TODO_UNKNOWN trigger: only if this is enabled!!! */
	if (this->snapshot_buffer) {
		qDebug() << SG_PREFIX_D << this->debug << "Deleting old snapshot buffer";
		delete this->snapshot_buffer;
	}
	qDebug() << SG_PREFIX_I << this->debug << "Creating new snapshot buffer with size" << this->width << this->height;
	this->snapshot_buffer = new QPixmap(this->width, this->height);

	qDebug() << SG_PREFIX_SIGNAL << this->debug << "Sending \"reconfigured\" signal";
	emit this->reconfigured(this->viewport);
}




Window * Viewport::get_window(void) const
{
	return this->window;
}




Viewport2D::Viewport2D(int l, int r, int t, int b, QWidget * parent) : QWidget(parent)
{
	this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	this->height_unit = Preferences::get_unit_height();
	this->distance_unit = Preferences::get_unit_distance();
	this->speed_unit = Preferences::get_unit_speed();

	this->grid = new QGridLayout();
	this->grid->setSpacing(0); /* Space between contents of grid. */
	this->grid->setContentsMargins(0, 0, 0, 0); /* Outermost margins of a grid. */

	QLayout * old = this->layout();
	delete old;
	qDeleteAll(this->children());
	this->setLayout(this->grid);

	this->create_central();
	this->create_margins(l, r, t, b);
}




void Viewport2D::create_central(void)
{
	if (this->central) {
		qDebug() << SG_PREFIX_I << "Removing old central widget";
		this->grid->removeWidget(this->central);
		delete this->central;
	}

	this->central = new Viewport(this);
	this->central->canvas.viewport = this;

	this->grid->addWidget(this->central, 1, 1);
}




void Viewport2D::create_margins(int l, int r, int t, int b)
{
	if (this->left) {
		qDebug() << SG_PREFIX_I << "Removing old left margin";
		this->grid->removeWidget(this->left);
		delete this->left;
	}
	if (this->right) {
		qDebug() << SG_PREFIX_I << "Removing old right margin";
		this->grid->removeWidget(this->right);
		delete this->right;
	}
	if (this->top) {
		qDebug() << SG_PREFIX_I << "Removing old top margin";
		this->grid->removeWidget(this->top);
		delete this->top;
	}
	if (this->bottom) {
		qDebug() << SG_PREFIX_I << "Removing old bottom margin";
		this->grid->removeWidget(this->bottom);
		delete this->bottom;
	}

	this->left   = new ViewportMargin(ViewportMargin::Position::Left,   l);
	this->right  = new ViewportMargin(ViewportMargin::Position::Right,  r);
	this->top    = new ViewportMargin(ViewportMargin::Position::Top,    t);
	this->bottom = new ViewportMargin(ViewportMargin::Position::Bottom, b);

	this->grid->addWidget(this->left,    1, 0);
	this->grid->addWidget(this->right,   1, 2);
	this->grid->addWidget(this->top,     0, 1);
	this->grid->addWidget(this->bottom,  2, 1);

	this->left->canvas.viewport = this;
	this->right->canvas.viewport = this;
	this->top->canvas.viewport = this;
	this->bottom->canvas.viewport = this;

	return;
}




void ViewportMargin::paintEvent(QPaintEvent * ev)
{
	qDebug() << SG_PREFIX_I;

	QPainter painter(this);
	painter.drawPixmap(0, 0, *this->canvas.pixmap);

	return;
}




void ViewportMargin::resizeEvent(QResizeEvent * ev)
{
	qDebug() << SG_PREFIX_I;

	this->canvas.reconfigure(this->geometry().width(), this->geometry().height());
	this->canvas.painter->setPen(this->border_pen);

	switch (this->position) {
	case ViewportMargin::Position::Left:
		this->canvas.painter->drawText(this->rect(), Qt::AlignCenter, "left");
		this->canvas.painter->drawLine(this->geometry().width() - 1, 0,
					       this->geometry().width() - 1, this->geometry().height() - 1);
		break;
	case ViewportMargin::Position::Right:
		this->canvas.painter->drawLine(1, 1,
					       1, this->geometry().height() - 1);
		this->canvas.painter->drawText(this->rect(), Qt::AlignCenter, "right");
		break;
	case ViewportMargin::Position::Top:
		this->canvas.painter->drawLine(0,                            this->geometry().height() - 1,
					       this->geometry().width() - 1, this->geometry().height() - 1);
		this->canvas.painter->drawText(this->rect(), Qt::AlignCenter, "top");
		break;
	case ViewportMargin::Position::Bottom:
		this->canvas.painter->drawLine(1,                            1,
					       this->geometry().width() - 1, 1);
		this->canvas.painter->drawText(this->rect(), Qt::AlignCenter, "bottom");
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled margin position";
		break;
	}

	return;
}




ViewportMargin::ViewportMargin(ViewportMargin::Position pos, int main_size, QWidget * parent) : QWidget(parent)
{
	this->position = pos;
	this->size = main_size;

	this->border_pen.setColor(QColor("black"));
	this->border_pen.setWidth(2);

	switch (this->position) {
	case ViewportMargin::Position::Left:
		this->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
		this->setMinimumSize(size, 10);
		snprintf(this->canvas.debug, sizeof (this->canvas.debug), "%s", "left");
		break;

	case ViewportMargin::Position::Right:
		this->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
		this->setMinimumSize(size, 10);
		snprintf(this->canvas.debug, sizeof (this->canvas.debug), "%s", "right");
		break;

	case ViewportMargin::Position::Top:
		this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		this->setMinimumSize(10, size);
		snprintf(this->canvas.debug, sizeof (this->canvas.debug), "%s", "top");
		break;

	case ViewportMargin::Position::Bottom:
		this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		this->setMinimumSize(10, size);
		snprintf(this->canvas.debug, sizeof (this->canvas.debug), "%s", "bottom");
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled margin position";
		break;
	}
}




int Viewport2D::central_get_width(void) const
{
	return this->central ? this->central->canvas.width : 0;
}

int Viewport2D::central_get_height(void) const
{
	return this->central ? this->central->canvas.height : 0;
}

int Viewport2D::left_get_width(void) const
{
	return this->left ? this->left->canvas.width : 0;
}

int Viewport2D::left_get_height(void) const
{
	return this->left ? this->left->canvas.height : 0;
}

int Viewport2D::right_get_width(void) const
{
	return this->right ? this->right->canvas.width : 0;
}

int Viewport2D::right_get_height(void) const
{
	return this->right ? this->right->canvas.height : 0;
}

int Viewport2D::top_get_width(void) const
{
	return this->top ? this->top->canvas.width : 0;
}

int Viewport2D::top_get_height(void) const
{
	return this->top ? this->top->canvas.height : 0;
}

int Viewport2D::bottom_get_width(void) const
{
	return this->bottom ? this->bottom->canvas.width : 0;
}

int Viewport2D::bottom_get_height(void) const
{
	return this->bottom ? this->bottom->canvas.height : 0;
}




sg_ret Viewport::get_cursor_pos(QMouseEvent * ev, ScreenPos & screen_pos) const
{
	const int w = this->canvas.width;
	const int h = this->canvas.height;

	const QPoint position = this->mapFromGlobal(QCursor::pos());

#if 0   /* Verbose debug. */
	qDebug() << SG_PREFIX_I << "Difference in cursor position: dx = " << position.x() - ev->x() << ", dy = " << position.y() - ev->y();
#endif

#if 0
	const int x = position.x();
	const int y = position.y();
#else
	const int x = ev->x();
	const int y = ev->y();
#endif

	if (x > w) {
		/* Cursor outside of chart area. */
		return sg_ret::err;
	}
	if (y > h) {
		/* Cursor outside of chart area. */
		return sg_ret::err;
	}

	screen_pos.x = x;
	if (screen_pos.x < 0) {
		qDebug() << SG_PREFIX_E << "Condition 1 for mouse movement failed:" << screen_pos.x << x;
		screen_pos.x = 0;
	}
	if (screen_pos.x > w) {
		qDebug() << SG_PREFIX_E << "Condition 2 for mouse movement failed:" << screen_pos.x << x << w;
		screen_pos.x = w;
	}


	screen_pos.y = h - y - 1; /* Converting from QT's "beginning is in upper-left" into "beginning is in bottom-left" coordinate system. */
	if (screen_pos.y < 0) {
		qDebug() << SG_PREFIX_E << "Condition 3 for mouse movement failed:" << screen_pos.y << y;
		screen_pos.y = 0;
	}
	if (screen_pos.y > h) {
		qDebug() << SG_PREFIX_E << "Condition 4 for mouse movement failed:" << screen_pos.y << x << h;
		screen_pos.y = h;
	}

	return sg_ret::ok;
}
