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




#define SG_MODULE "GisViewport"




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




void GisViewport::init(void)
{
	Expedia::init_radius();
}




double GisViewport::calculate_utm_zone_width(void) const
{
	switch (this->coord_mode) {
	case CoordMode::UTM: {

		/* Get latitude of screen bottom. */
		UTM utm = this->center_coord.utm;
		const double center_to_bottom_m = this->get_vpixmap_height_m() / 2;
		utm.shift_northing_by(-center_to_bottom_m);
		LatLon lat_lon = UTM::to_lat_lon(utm);

		/* Boundary. */
		lat_lon.lon = (utm.get_zone() - 1) * 6 - 180 ;
		utm = LatLon::to_utm(lat_lon);
		return fabs(utm.get_easting() - UTM_CENTRAL_MERIDIAN_EASTING) * 2;
	}

	case CoordMode::LatLon:
		return 0.0;

	default:
		qDebug() << SG_PREFIX_E << "Unexpected coord mode" << (int) this->coord_mode;
		return 0.0;
	}
}




const QColor & GisViewport::get_background_color(void) const
{
	return this->background_color;
}




GisViewport::GisViewport(QWidget * parent) : QWidget(parent)
{
	this->window = ThisApp::get_main_window();



	this->installEventFilter(this);

	this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	this->setMinimumSize(200, 300);
	snprintf(this->vpixmap.debug, sizeof (this->vpixmap.debug), "%s", "center");
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

	this->viking_scale.set(zoom_x, zoom_y);
	this->xmfactor = MERCATOR_FACTOR(this->viking_scale.x);
	this->ymfactor = MERCATOR_FACTOR(this->viking_scale.y);



	this->coord_mode = CoordMode::LatLon;
	this->drawmode = GisViewportDrawMode::Mercator;



	this->center_coords = new std::list<Coord>;
	this->center_coords_iter = this->center_coords->begin();
	if (!ApplicationState::get_integer(VIK_SETTINGS_VIEW_HISTORY_SIZE, &this->center_coords_max)) {
		this->center_coords_max = 20;
	}
	if (ApplicationState::get_integer(VIK_SETTINGS_VIEW_HISTORY_DIFF_DIST, &this->center_coords_radius)) {
		this->center_coords_radius = 500;
	}


	this->set_center_coord(initial_lat_lon); /* The function will reject latlon if it's invalid. */


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




GisViewport::~GisViewport()
{
	qDebug() << SG_PREFIX_I;
	if (Preferences::get_startup_method() == StartupMethod::LastLocation) {
		const LatLon lat_lon = this->center_coord.get_lat_lon();
		ApplicationState::set_double(VIK_SETTINGS_VIEW_LAST_LATITUDE, lat_lon.lat);
		ApplicationState::set_double(VIK_SETTINGS_VIEW_LAST_LONGITUDE, lat_lon.lon);

		ApplicationState::set_double(VIK_SETTINGS_VIEW_LAST_ZOOM_X, this->viking_scale.x);
		ApplicationState::set_double(VIK_SETTINGS_VIEW_LAST_ZOOM_Y, this->viking_scale.y);
	}

	delete this->center_coords;
}




void GisViewport::set_background_color(const QString & color_name)
{
	this->background_color.setNamedColor(color_name);
	this->background_pen.setColor(this->background_color);
}




void GisViewport::set_background_color(const QColor & color)
{
	this->background_color = color;
	this->background_pen.setColor(color);
}




const QColor & GisViewport::get_highlight_color(void) const
{
	return this->highlight_color;
}




void GisViewport::set_highlight_color(const QString & color_name)
{
	this->highlight_color.setNamedColor(color_name);
	this->highlight_pen.setColor(this->highlight_color);
}




void GisViewport::set_highlight_color(const QColor & color)
{
	this->highlight_color = color;
	this->highlight_pen.setColor(color);
}




QPen GisViewport::get_highlight_pen(void) const
{
	return this->highlight_pen;
}




void GisViewport::set_highlight_thickness(int w)
{
	this->highlight_pen.setWidth(w);

	this->highlight_pen.setCapStyle(Qt::RoundCap);
	this->highlight_pen.setJoinStyle(Qt::RoundJoin);

#if 0   /* This line style is used by default. */
	this->highlight_pen.setStyle(Qt::SolidLine);
#endif
}




void GisViewport::reconfigure_drawing_area(int new_width, int new_height)
{
	if (new_width == 0 && new_height == 0) {
		qDebug() << SG_PREFIX_I << "Will reconfigure vpixmap with geometry sizes" << this->geometry().width() << this->geometry().height();
		this->vpixmap.reconfigure(this->geometry().width(), this->geometry().height());
	} else {
		qDebug() << SG_PREFIX_I << "Will reconfigure vpixmap with specified sizes" << new_width << new_height;
		this->vpixmap.reconfigure(new_width, new_height);
	}
}




QPixmap GisViewport::get_pixmap(void) const
{
	return *this->vpixmap.pixmap;
}




void GisViewport::set_pixmap(const QPixmap & pixmap)
{
	if (this->vpixmap.pixmap->size() != pixmap.size()) {
		qDebug() << SG_PREFIX_E << "Pixmap size mismatch: vpixmap =" << this->vpixmap.pixmap->size() << ", new vpixmap =" << pixmap.size();
	} else {
		//QPainter painter(this->vpixmap.pixmap);
		this->vpixmap.painter->drawPixmap(0, 0, pixmap, 0, 0, 0, 0);
	}
}




/**
   \brief Clear the whole viewport
*/
void GisViewport::clear(void)
{
	qDebug() << SG_PREFIX_I << "Clear whole viewport" << this->vpixmap;
	//QPainter painter(this->vpixmap.pixmap);
	this->vpixmap.clear();


	/* Some maps may have been removed, so their logos and/or
	   attributions/copyrights must be cleared as well. */
	this->decorations.clear();

}




void GisViewport::draw_decorations(void)
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
void GisViewport::set_center_mark_visibility(bool new_state)
{
	this->center_mark_visibility = new_state;
}




bool GisViewport::get_center_mark_visibility() const
{
	return this->center_mark_visibility;
}




void GisViewport::set_highlight_usage(bool new_state)
{
	this->highlight_usage = new_state;
}




bool GisViewport::get_highlight_usage(void) const
{
	return this->highlight_usage;
}



/**
   \brief Enable/Disable display of scale
*/
void GisViewport::set_scale_visibility(bool new_state)
{
	this->scale_visibility = new_state;
}




bool GisViewport::get_scale_visibility(void) const
{
	return this->scale_visibility;
}




void GisViewport::sync(void)
{
	qDebug() << SG_PREFIX_I << "sync (will call ->render())";
	//gdk_draw_drawable(gtk_widget_get_window(GTK_WIDGET(this)), gtk_widget_get_style(GTK_WIDGET(this))->bg_gc[0], GDK_DRAWABLE(this->vpixmap.pixmap), 0, 0, 0, 0, this->vpixmap.width, this->vpixmap.height);
	this->render(this->vpixmap.pixmap);
}




void GisViewport::pan_sync(int x_off, int y_off)
{
	qDebug() << SG_PREFIX_I;
#ifdef K_FIXME_RESTORE
	int x, y, wid, hei;

	gdk_draw_drawable(gtk_widget_get_window(GTK_WIDGET(this)), gtk_widget_get_style(GTK_WIDGET(this))->bg_gc[0], GDK_DRAWABLE(this->vpixmap.pixmap), 0, 0, x_off, y_off, this->vpixmap.width, this->vpixmap.height);

	if (x_off >= 0) {
		x = 0;
		wid = x_off;
	} else {
		x = this->vpixmap.width + x_off;
		wid = -x_off;
	}

	if (y_off >= 0) {
		y = 0;
		hei = y_off;
	} else {
		y = this->vpixmap.height + y_off;
		hei = -y_off;
	}
	gtk_widget_queue_draw_area(GTK_WIDGET(this), x, 0, wid, this->vpixmap.height);
	gtk_widget_queue_draw_area(GTK_WIDGET(this), 0, y, this->vpixmap.width, hei);
#endif
}




sg_ret GisViewport::set_viking_scale(double new_value)
{
	if (!VikingScale::value_is_valid(new_value)) {
		qDebug() << SG_PREFIX_E << "Failed to set new zoom level, invalid value" << new_value;
		return sg_ret::err;
	}

	if (sg_ret::ok != this->viking_scale.set(new_value, new_value)) {
		return sg_ret::err;
	}

	this->xmfactor = MERCATOR_FACTOR(this->viking_scale.x);
	this->ymfactor = MERCATOR_FACTOR(this->viking_scale.y);

	if (this->drawmode == GisViewportDrawMode::UTM) {
		this->utm_zone_check();
	}

	return sg_ret::ok;
}




void GisViewport::zoom_in(void)
{
	if (this->viking_scale.zoom_in(2)) {
		this->xmfactor = MERCATOR_FACTOR(this->viking_scale.x);
		this->ymfactor = MERCATOR_FACTOR(this->viking_scale.y);

		this->utm_zone_check();
	}
}




void GisViewport::zoom_out(void)
{
	if (this->viking_scale.zoom_out(2)) {
		this->xmfactor = MERCATOR_FACTOR(this->viking_scale.x);
		this->ymfactor = MERCATOR_FACTOR(this->viking_scale.y);

		this->utm_zone_check();
	}
}




const VikingScale & GisViewport::get_viking_scale(void) const
{
	return this->viking_scale;
}




sg_ret GisViewport::set_viking_scale(const VikingScale & new_value)
{
	/* TODO: add validation. */
	this->viking_scale = new_value;
	return sg_ret::ok;
}




sg_ret GisViewport::set_viking_scale_x(double new_value)
{
	if (!VikingScale::value_is_valid(new_value)) {
		qDebug() << SG_PREFIX_E << "Failed to set new zoom level, invalid value" << new_value;
		return sg_ret::err;
	}

	this->viking_scale.x = new_value;
	this->xmfactor = MERCATOR_FACTOR(this->viking_scale.x);
	if (this->drawmode == GisViewportDrawMode::UTM) {
		this->utm_zone_check();
	}

	return sg_ret::ok;
}




sg_ret GisViewport::set_viking_scale_y(double new_value)
{
	if (!VikingScale::value_is_valid(new_value)) {
		qDebug() << SG_PREFIX_E << "Failed to set new zoom level, invalid value" << new_value;
		return sg_ret::err;
	}

	this->viking_scale.y = new_value;
	this->ymfactor = MERCATOR_FACTOR(this->viking_scale.y);
	if (this->drawmode == GisViewportDrawMode::UTM) {
		this->utm_zone_check();
	}

	return sg_ret::ok;
}




const Coord & GisViewport::get_center_coord(void) const
{
	return this->center_coord;
}




/* Called every time we update coordinates/zoom. */
void GisViewport::utm_zone_check(void)
{
	if (this->coord_mode == CoordMode::UTM) {
		const UTM utm = LatLon::to_utm(UTM::to_lat_lon(this->center_coord.utm));
		if (!UTM::is_the_same_zone(utm, this->center_coord.utm)) {
			this->center_coord.utm = utm;
		}

		/* Misc. stuff so we don't have to check later. */
		this->utm_zone_width = this->calculate_utm_zone_width();
		this->is_one_utm_zone = (this->get_rightmost_zone() == this->get_leftmost_zone());
	}
}




/**
   \brief Remove an individual center position from the history list
*/
void GisViewport::free_center_coords(std::list<Coord>::iterator iter)
{
	if (iter == this->center_coords_iter) {
		this->center_coords_iter = this->center_coords->erase(iter);

		if (this->center_coords_iter == this->center_coords->end()) {
			/* We have removed last element on the list. */

			if (center_coords->empty()) {
				this->center_coords_iter = this->center_coords->begin();
			} else {
				this->center_coords_iter--;
			}
		}
	} else {
		this->center_coords->erase(iter);
	}
}




void GisViewport::save_current_center_coord(void)
{
	if (this->center_coords_iter == prev(this->center_coords->end())) {
		/* We are at most recent element of history. */
		if (this->center_coords->size() == (unsigned) this->center_coords_max) {
			/* List is full, so drop the oldest value to make room for the new one. */
			this->free_center_coords(this->center_coords->begin());
		}
	} else {
		/* We are somewhere in the middle of history list, possibly at the beginning.
		   Every center visited after current one must be discarded. */
		this->center_coords->erase(next(this->center_coords_iter), this->center_coords->end());
		assert (std::next(this->center_coords_iter) == this->center_coords->end());
	}

	/* Store new position. */
	/* push_back() puts at the end. By convention end == newest. */
	this->center_coords->push_back(this->center_coord);
	this->center_coords_iter++;
	assert (std::next(this->center_coords_iter) == this->center_coords->end());

	this->print_center_coords("GisViewport::save_current_center_coord()");

	qDebug() << SG_PREFIX_SIGNAL << "Emitting center_coord_updated()";
	emit this->center_coord_updated();
}




std::list<QString> GisViewport::get_center_coords_list(void) const
{
	std::list<QString> result;

	for (auto iter = this->center_coords->begin(); iter != this->center_coords->end(); iter++) {

		QString extra;
		if (iter == prev(this->center_coords_iter)) {
			extra = tr("[Back]");
		} else if (iter == next(this->center_coords_iter)) {
			extra = tr("[Forward]");
		} else if (iter == this->center_coords_iter) {
			extra = tr("[Current]");
		} else {
			; /* NOOP */
		}

		result.push_back(tr("%1%2")
				 .arg((*iter).to_string())
				 .arg(extra));
	}

	return result;
}




/**
   \brief Show the list of forward/back positions

   ATM only for debug usage.
*/
void GisViewport::show_center_coords(Window * parent_window) const
{
	const std::list<QString> texts = this->get_center_coords_list();

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




void GisViewport::print_center_coords(const QString & label) const
{
	const std::list<QString> texts = this->get_center_coords_list();

	for (auto iter = texts.begin(); iter != texts.end(); iter++) {
		qDebug() << SG_PREFIX_I << "Center coords:" << label << *iter;
	}

	return;
}





/**
   \brief Move back in the position history

   @return true on success
   @return false otherwise
*/
bool GisViewport::go_back(void)
{
	/* See if the current position is different from the
	   last saved center position within a certain radius. */
	if (Coord::distance(*this->center_coords_iter, this->center_coord) > this->center_coords_radius) {

		if (this->center_coords_iter == prev(this->center_coords->end())) {
			/* Only when we haven't already moved back in the list.
			   Remember where this request came from (alternatively we could insert in the list on every back attempt). */
			this->save_current_center_coord();
		}
	}

	if (!this->back_available()) {
		/* Already at the oldest center. */
		return false;
	}

	/* If we inserted a position above, then this will then move to the last saved position.
	   Otherwise this will skip to the previous saved position, as it's probably somewhere else. */

	/* This is safe because ::back_available() returned true. */
	this->center_coords_iter--;
	this->set_center_coord(*this->center_coords_iter, false);

	return true;
}




/**
   \brief Move forward in the position history

   @return true on success
   @return false otherwise
*/
bool GisViewport::go_forward(void)
{
	if (!this->forward_available()) {
		/* Already at the latest center. */
		return false;
	}

	/* This is safe because ::forward_available() returned true. */
	this->center_coords_iter++;
	this->set_center_coord(*this->center_coords_iter, false);

	return true;
}




/**
   @return true when a previous position in the history is available
   @return false otherwise
*/
bool GisViewport::back_available(void) const
{
	return (this->center_coords->size() > 1 && this->center_coords_iter != this->center_coords->begin());
}




/**
   @return true when a next position in the history is available
   @return false otherwise
*/
bool GisViewport::forward_available(void) const
{
	return (this->center_coords->size() > 1 && this->center_coords_iter != prev(this->center_coords->end()));
}




/**
   @lat_lon:       The new center position in Lat/Lon format
   @save_position: Whether this new position should be saved into the history of positions
                   Normally only specific user requests should be saved (i.e. to not include Pan and Zoom repositions)
*/
sg_ret GisViewport::set_center_coord(const LatLon & lat_lon, bool save_position)
{
	if (!lat_lon.is_valid()) {
		qDebug() << SG_PREFIX_E << "Not setting lat/lon, value is invalid:" << lat_lon.lat << lat_lon.lon;
		return sg_ret::err;
	}

	return this->set_center_coord(Coord(lat_lon, this->coord_mode), save_position);
}




/**
   @utm:           The new center position in UTM format
   @save_position: Whether this new position should be saved into the history of positions
                   Normally only specific user requests should be saved (i.e. to not include Pan and Zoom repositions)
*/
sg_ret GisViewport::set_center_coord(const UTM & utm, bool save_position)
{
	return this->set_center_coord(Coord(utm, this->coord_mode), save_position);
}




/**
   @coord:         The new center position in a Coord type
   @save_position: Whether this new position should be saved into the history of positions
                   Normally only specific user requests should be saved (i.e. to not include Pan and Zoom repositions)
*/
sg_ret GisViewport::set_center_coord(const Coord & coord, bool save_position)
{
	this->center_coord = coord;
	if (save_position) {
		this->save_current_center_coord();
	}

	if (this->coord_mode == CoordMode::UTM) {
		this->utm_zone_check();
	}

	return sg_ret::ok;
}




sg_ret GisViewport::set_center_coord(int x1, int y1)
{
	const Coord coord = this->screen_pos_to_coord(x1, y1);
	return this->set_center_coord(coord, false);
}




sg_ret GisViewport::set_center_coord(const ScreenPos & pos)
{
	return this->set_center_coord(pos.x, pos.y);
}




sg_ret GisViewport::get_corners_for_zone(Coord & coord_ul, Coord & coord_br, int zone)
{
	if (this->coord_mode != CoordMode::UTM) {
		qDebug() << SG_PREFIX_E << "Coord mode is not UTM:" << (int) this->coord_mode;
		return sg_ret::err;
	}

	/* Get center, then just offset. */
	if (sg_ret::ok != this->utm_recalculate_current_center_coord_for_other_zone(coord_ul.utm, zone)) {
		qDebug() << SG_PREFIX_E << "Can't center for zone" << zone;
		return sg_ret::err;
	}
	coord_ul.set_coord_mode(CoordMode::UTM);


	/* Both coordinates will be now at center. */
	coord_br = coord_ul;


	/* And now we offset the two coordinates:
	   we move the coordinates from center to one of the two corners. */
	const double center_to_top_m = this->get_vpixmap_height_m() / 2;
	const double center_to_left_m = this->get_vpixmap_width_m() / 2;
	coord_ul.utm.shift_northing_by(center_to_top_m);
	coord_ul.utm.shift_easting_by(-center_to_left_m);

	const double center_to_bottom_m = this->get_vpixmap_height_m() / 2;
	const double center_to_right_m = this->get_vpixmap_width_m() / 2;
	coord_br.utm.shift_northing_by(-center_to_bottom_m);
	coord_br.utm.shift_easting_by(center_to_right_m);

	return sg_ret::ok;
}




sg_ret GisViewport::utm_recalculate_current_center_coord_for_other_zone(UTM & center_in_other_zone, int zone)
{
	if (this->coord_mode != CoordMode::UTM) {
		qDebug() << SG_PREFIX_E << "Coord mode is not UTM:" << (int) this->coord_mode;
		return sg_ret::err;
	}

	const int zone_diff = zone - this->center_coord.utm.get_zone();

	/* TODO: why do we have to offset easting? Wouldn't easting of center be the same in each zone? */
	center_in_other_zone = this->center_coord.utm;
	center_in_other_zone.shift_easting_by(-(zone_diff * this->utm_zone_width));
	center_in_other_zone.set_zone(zone);

	return sg_ret::ok;
}




int GisViewport::get_leftmost_zone(void) const
{
	if (coord_mode != CoordMode::UTM) {
		return 0;
	}

	const Coord coord = this->screen_pos_to_coord(this->vpixmap.get_leftmost_pixel(),
						      this->vpixmap.get_upmost_pixel()); /* Second argument shouldn't really matter if we are getting "leftmost" zone. */
	return coord.utm.get_zone();
}




int GisViewport::get_rightmost_zone(void) const
{
	if (coord_mode != CoordMode::UTM) {
		return 0;
	}

	const Coord coord = this->screen_pos_to_coord(this->vpixmap.get_rightmost_pixel(),
						      this->vpixmap.get_upmost_pixel()); /* Second argument shouldn't really matter if we are getting "rightmost" zone. */
	return coord.utm.get_zone();
}




QRect GisViewport::get_rect(void) const
{
	return QRect(0, 0, this->vpixmap.get_width(), this->vpixmap.get_height());
}




Coord GisViewport::screen_pos_to_coord(ScreenPosition screen_pos) const
{
	Coord result;
	int pos_x, pos_y;

	switch (screen_pos) {
	case ScreenPosition::UpperLeft:
		pos_x = this->vpixmap.get_leftmost_pixel();
		pos_y = this->vpixmap.get_upmost_pixel();;
		break;

	case ScreenPosition::UpperRight:
		pos_x = this->vpixmap.get_rightmost_pixel();
		pos_y = this->vpixmap.get_upmost_pixel();
		break;

	case ScreenPosition::BottomLeft:
		pos_x = this->vpixmap.get_leftmost_pixel();
		pos_y = this->vpixmap.get_bottommost_pixel();
		break;

	case ScreenPosition::BottomRight:
		pos_x = this->vpixmap.get_rightmost_pixel();
		pos_y = this->vpixmap.get_bottommost_pixel();
		break;

	default:
		qDebug() << SG_PREFIX_E << "Unexpected screen position" << (int) screen_pos;
		break;
	}

	result = this->screen_pos_to_coord(pos_x, pos_y);
	return result;
}




Coord GisViewport::screen_pos_to_coord(int pos_x, int pos_y) const
{
	Coord coord;
	const double xmpp = this->viking_scale.x;
	const double ympp = this->viking_scale.y;

	/* Distance of pixel specified by pos_x/pos_y from vpixmap' central pixel.
	   TODO: verify location of pos_x and pos_y in these equations. */
	const int delta_horiz_pixels = pos_x - this->vpixmap.get_horiz_center_pixel();
	const int delta_vert_pixels = this->vpixmap.get_vert_center_pixel() - pos_y;

	switch (this->coord_mode) {
	case CoordMode::UTM:
		coord.set_coord_mode(CoordMode::UTM);

		/* Modified (reformatted) formula. */
		{
			coord.utm.set_zone(this->center_coord.utm.get_zone());
			assert (UTM::is_band_letter(this->center_coord.utm.get_band_letter())); /* TODO_2_LATER: add smarter error handling. In theory the source object should be valid and for sure contain valid band letter. */
			coord.utm.set_band_letter(this->center_coord.utm.get_band_letter());
			coord.utm.set_easting((delta_horiz_pixels * xmpp) + this->center_coord.utm.get_easting());

			const int zone_delta = floor((coord.utm.easting - UTM_CENTRAL_MERIDIAN_EASTING) / this->utm_zone_width + 0.5);

			coord.utm.shift_zone_by(zone_delta);
			coord.utm.shift_easting_by(-(zone_delta * this->utm_zone_width));
			coord.utm.set_northing((delta_vert_pixels * ympp) + this->center_coord.utm.get_northing());
		}

		/* Original code, used for comparison of results with new, reformatted formula. */
		{
			int zone_delta = 0;

			Coord test_coord;
			test_coord.set_coord_mode(CoordMode::UTM);

			test_coord.utm.set_zone(this->center_coord.utm.get_zone());
			assert (UTM::is_band_letter(this->center_coord.utm.get_band_letter())); /* TODO_2_LATER: add smarter error handling. In theory the source object should be valid and for sure contain valid band letter. */
			test_coord.utm.set_band_letter(this->center_coord.utm.get_band_letter());
			test_coord.utm.easting = (delta_horiz_pixels * xmpp) + this->center_coord.utm.easting;

			zone_delta = floor((test_coord.utm.easting - UTM_CENTRAL_MERIDIAN_EASTING) / this->utm_zone_width + 0.5);

			test_coord.utm.shift_zone_by(zone_delta);
			test_coord.utm.easting -= zone_delta * this->utm_zone_width;
			test_coord.utm.northing = (delta_vert_pixels * ympp) + this->center_coord.utm.northing;


			if (!UTM::is_the_same_zone(coord.utm, test_coord.utm)) {
				qDebug() << SG_PREFIX_E << "UTM: Zone calculation mismatch" << coord << test_coord << coord.utm.get_zone() << test_coord.utm.get_zone();
			}
			if (coord.utm.get_easting() != test_coord.utm.get_easting()) {
				qDebug() << SG_PREFIX_E << "UTM: Easting calculation mismatch" << coord << test_coord << (coord.utm.get_easting() - test_coord.utm.get_easting());
			}
			if (coord.utm.get_northing() != test_coord.utm.get_northing()) {
				qDebug() << SG_PREFIX_E << "UTM: Northing calculation mismatch" << coord << test_coord << (coord.utm.get_northing() - test_coord.utm.get_northing());
			}
			if (coord.utm.get_band_letter() != test_coord.utm.get_band_letter()) {
				qDebug() << SG_PREFIX_E << "UTM: Band letter calculation mismatch" << coord << test_coord << coord.utm.get_band_as_letter() << test_coord.utm.get_band_as_letter();
			}
		}
		break;

	case CoordMode::LatLon:
		coord.set_coord_mode(CoordMode::LatLon);

		switch (this->drawmode) {
		case GisViewportDrawMode::LatLon:
			/* Modified (reformatted) formula. */
			{
				coord.lat_lon.lon = this->center_coord.lat_lon.lon + (delta_horiz_pixels / REVERSE_MERCATOR_FACTOR(xmpp));
				coord.lat_lon.lat = this->center_coord.lat_lon.lat + (delta_vert_pixels / REVERSE_MERCATOR_FACTOR(ympp));
			}

			/* Original code, used for comparison of results with new, reformatted formula. */
			{
				Coord test_coord;
				test_coord.set_coord_mode(CoordMode::LatLon);

				test_coord.lat_lon.lon = this->center_coord.lat_lon.lon + (180.0 * xmpp / 65536 / 256 * delta_horiz_pixels);
				test_coord.lat_lon.lat = this->center_coord.lat_lon.lat + (180.0 * ympp / 65536 / 256 * delta_vert_pixels);

				if (coord.lat_lon.lat != test_coord.lat_lon.lat) {
					qDebug() << SG_PREFIX_E << "LatLon: Latitude calculation mismatch" << coord << test_coord << (coord.lat_lon.lat - test_coord.lat_lon.lat);
				}
				if (coord.lat_lon.lon != test_coord.lat_lon.lon) {
					qDebug() << SG_PREFIX_E << "LatLon: Longitude calculation mismatch" << coord << test_coord << (coord.lat_lon.lon - test_coord.lat_lon.lon);
				}
			}
			break;

		case GisViewportDrawMode::Expedia:
			Expedia::screen_pos_to_lat_lon(coord.lat_lon, pos_x, pos_y, this->center_coord.lat_lon, xmpp * ALTI_TO_MPP, ympp * ALTI_TO_MPP,
						       /* TODO: make sure that this is ok, that we don't need to use get_width()/get_height() here. */
						       this->vpixmap.get_horiz_center_pixel(),
						       this->vpixmap.get_vert_center_pixel());
			break;

		case GisViewportDrawMode::Mercator:
			/* This isn't called with a high frequently so less need to optimize. */
			/* Modified (reformatted) formula. */
			{
				coord.lat_lon.lon = this->center_coord.lat_lon.lon + (delta_horiz_pixels / REVERSE_MERCATOR_FACTOR(xmpp));
				coord.lat_lon.lat = DEMERCLAT (MERCLAT(this->center_coord.lat_lon.lat) + (delta_vert_pixels / (REVERSE_MERCATOR_FACTOR(ympp))));
			}

			/* Original code, used for comparison of results with new, reformatted formula. */
			{
				Coord test_coord;
				test_coord.set_coord_mode(CoordMode::LatLon);

				test_coord.lat_lon.lon = this->center_coord.lat_lon.lon + (180.0 * xmpp / 65536 / 256 * delta_horiz_pixels);
				test_coord.lat_lon.lat = DEMERCLAT (MERCLAT(this->center_coord.lat_lon.lat) + (180.0 * ympp / 65536 / 256 * delta_vert_pixels));

				if (coord.lat_lon.lat != test_coord.lat_lon.lat) {
					qDebug() << SG_PREFIX_E << "Mercator: Latitude calculation mismatch" << coord << test_coord << (coord.lat_lon.lat - test_coord.lat_lon.lat);
				} else {
					qDebug() << SG_PREFIX_I << "Mercator: OK Latitude" << coord << test_coord << (coord.lat_lon.lat - test_coord.lat_lon.lat);
				}
				if (coord.lat_lon.lon != test_coord.lat_lon.lon) {
					qDebug() << SG_PREFIX_E << "Mercator: Longitude calculation mismatch" << coord << test_coord << (coord.lat_lon.lon - test_coord.lat_lon.lon);
				} else {
					qDebug() << SG_PREFIX_I << "Mercator: OK Longitude" << coord << test_coord << (coord.lat_lon.lon - test_coord.lat_lon.lon);
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




Coord GisViewport::screen_pos_to_coord(const ScreenPos & pos) const
{
	return this->screen_pos_to_coord(pos.x, pos.y);
}




/*
  Since this function is used for every drawn trackpoint - it can get called a lot.
  Thus x & y position factors are calculated once on zoom changes,
  avoiding the need to do it here all the time.
*/
sg_ret GisViewport::coord_to_screen_pos(const Coord & coord_in, int * pos_x, int * pos_y) const
{
	Coord coord = coord_in;
	const double xmpp = this->viking_scale.x;
	const double ympp = this->viking_scale.y;

	const int horiz_center_pixel = this->vpixmap.get_horiz_center_pixel();
	const int vert_center_pixel = this->vpixmap.get_vert_center_pixel();

	if (coord_in.get_coord_mode() != this->coord_mode) {
		/* The intended use of the function is that coord_in
		   argument is always in correct coord mode (i.e. in
		   viewport's coord mode). If it is necessary to
		   convert the coord_in to proper coord mode, it is
		   done before the function call. Therefore if it
		   turns out that coord_in's coordinate mode is
		   different than viewport's coordinate mode, it's an
		   error. */
		qDebug() << SG_PREFIX_W << "Need to convert coord mode! This should never happen!";
		coord.recalculate_to_mode(this->coord_mode);
	}

	switch (this->coord_mode) {
	case CoordMode::UTM:
		{
			const int zone_diff = this->center_coord.utm.get_zone() - coord.utm.get_zone();

			if (0 != zone_diff && this->is_one_utm_zone){
				return sg_ret::err;
			}

			const double horiz_distance_m = coord.utm.get_easting() - this->center_coord.utm.get_easting();
			const double vert_distance_m = coord.utm.get_northing() - this->center_coord.utm.get_northing();

			*pos_x = horiz_center_pixel + (horiz_distance_m / xmpp) - (zone_diff * this->utm_zone_width) / xmpp;
			*pos_y = vert_center_pixel - (vert_distance_m / ympp); /* TODO: plus or minus? */
		}
		break;

	case CoordMode::LatLon:
		switch (this->drawmode) {
		case GisViewportDrawMode::LatLon:
			*pos_x = horiz_center_pixel + (MERCATOR_FACTOR(xmpp) * (coord.lat_lon.lon - this->center_coord.lat_lon.lon));
			*pos_y = vert_center_pixel + (MERCATOR_FACTOR(ympp) * (this->center_coord.lat_lon.lat - coord.lat_lon.lat));
			break;
		case GisViewportDrawMode::Expedia:
			{
				double xx, yy;
				Expedia::lat_lon_to_screen_pos(&xx, &yy, this->center_coord.lat_lon, coord.lat_lon, xmpp * ALTI_TO_MPP, ympp * ALTI_TO_MPP, horiz_center_pixel, vert_center_pixel);
				*pos_x = xx;
				*pos_y = yy;
			}
			break;
		case GisViewportDrawMode::Mercator:
			*pos_x = horiz_center_pixel + (MERCATOR_FACTOR(xmpp) * (coord.lat_lon.lon - this->center_coord.lat_lon.lon));
			*pos_y = vert_center_pixel + (MERCATOR_FACTOR(ympp) * (MERCLAT(this->center_coord.lat_lon.lat) - MERCLAT(coord.lat_lon.lat)));
			break;
		default:
			qDebug() << SG_PREFIX_E << "Unexpected viewport drawing mode" << (int) this->drawmode;
			return sg_ret::err;
		}
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected viewport coord mode" << (int) this->coord_mode;
		return sg_ret::err;
	}

	return sg_ret::ok;
}




ScreenPos GisViewport::coord_to_screen_pos(const Coord & coord_in) const
{
	ScreenPos pos;
	if (sg_ret::ok != this->coord_to_screen_pos(coord_in, &pos.x, &pos.y)) {
		/* TODO: invalidate pos. */
	}
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
void GisViewport::clip_line(int * x1, int * y1, int * x2, int * y2)
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



void Viewport2D::central_draw_line(const QPen & pen, int begin_x, int begin_y, int end_x, int end_y)
{
	//qDebug() << SG_PREFIX_I << "Attempt to draw line between points" << begin_x << begin_y << "and" << end_x << end_y;
	if (this->central->vpixmap.line_is_outside(begin_x, begin_y, end_x, end_y)) {
		return;
	}

	/*** Clipping, yeah! ***/
	//GisViewport::clip_line(&begin_x, &begin_y, &end_x, &end_y);

	/* x/y coordinates are converted here from "beginning in
	   bottom-left corner" to "beginning in top-left corner"
	   coordinate system. */
	const int bottom_pixel = this->central->vpixmap.get_bottommost_pixel();

	this->central->vpixmap.painter->setPen(pen);
	this->central->vpixmap.painter->drawLine(begin_x, bottom_pixel - begin_y,
						 end_x,   bottom_pixel - end_y);
}




void Viewport2D::margin_draw_text(ViewportMargin::Position pos, const QFont & text_font, const QPen & pen, const QRectF & bounding_rect, int flags, const QString & text, int text_offset)
{
	qDebug() << SG_PREFIX_I << "Will draw label" << text;
	ViewportPixmap * vpixmap = NULL;

	switch (pos) {
	case ViewportMargin::Position::Left:
		vpixmap = this->left;
		break;
	case ViewportMargin::Position::Right:
		vpixmap = this->right;
		break;
	case ViewportMargin::Position::Top:
		vpixmap = this->top;
		break;
	case ViewportMargin::Position::Bottom:
		vpixmap = this->bottom;
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled margin position";
		break;
	}

	if (!vpixmap) {
		qDebug() << SG_PREFIX_E << "No margin vpixmap selected";
		return;
	}
	if (!vpixmap->painter) {
		qDebug() << SG_PREFIX_W << "Viewport Pixmap has no painter (yet?)";
		return;
	}


	vpixmap->painter->setFont(text_font);

	/* "Normalize" bounding rectangles that have negative width or height.
	   Otherwise the text will be outside of the bounding rectangle. */
	QRectF final_bounding_rect = bounding_rect.united(bounding_rect);

	QRectF text_rect = vpixmap->painter->boundingRect(final_bounding_rect, flags, text);
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
	vpixmap->painter->setPen(QColor("red"));
	vpixmap->painter->drawEllipse(bounding_rect.left(), bounding_rect.top(), 5, 5);

	vpixmap->painter->setPen(QColor("darkgreen"));
	vpixmap->painter->drawRect(bounding_rect);

	vpixmap->painter->setPen(QColor("red"));
	vpixmap->painter->drawRect(text_rect);
#endif

	vpixmap->painter->setPen(pen);
	vpixmap->painter->drawText(text_rect, flags, text, NULL);
}




void GisViewport::draw_bbox(const LatLonBBox & bbox, const QPen & pen)
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

	this->vpixmap.draw_rectangle(pen, sp_sw.x, sp_ne.y, sp_ne.x - sp_sw.x, sp_sw.y - sp_ne.y);

	return;
}



CoordMode GisViewport::get_coord_mode(void) const
{
	return this->coord_mode;
}




void GisViewport::set_coord_mode(CoordMode new_mode)
{
	this->coord_mode = new_mode;
	this->center_coord.recalculate_to_mode(new_mode);
}




bool GisViewport::get_is_one_utm_zone(void) const
{
	return coord_mode == CoordMode::UTM && this->is_one_utm_zone;
}




void GisViewport::set_drawmode(GisViewportDrawMode new_drawmode)
{
	this->drawmode = new_drawmode;

	if (new_drawmode == GisViewportDrawMode::UTM) {
		this->set_coord_mode(CoordMode::UTM);
	} else {
		this->set_coord_mode(CoordMode::LatLon);
	}
}




GisViewportDrawMode GisViewport::get_drawmode(void) const
{
	return this->drawmode;
}




/******** Triggering. *******/
void GisViewport::set_trigger(Layer * trg)
{
	this->trigger = trg;
}




Layer * GisViewport::get_trigger(void) const
{
	return this->trigger;
}




void GisViewport::snapshot_save(void)
{
	qDebug() << SG_PREFIX_I << "Save snapshot";
	*this->vpixmap.snapshot_buffer = *this->vpixmap.pixmap;
}




void GisViewport::snapshot_load(void)
{
	qDebug() << SG_PREFIX_I << "Load snapshot";
	*this->vpixmap.pixmap = *this->vpixmap.snapshot_buffer;
}




void GisViewport::set_half_drawn(bool new_half_drawn)
{
	this->half_drawn = new_half_drawn;
}




bool GisViewport::get_half_drawn(void) const
{
	return this->half_drawn;
}




LatLonBBox GisViewport::get_bbox(int margin_left, int margin_right, int margin_top, int margin_bottom) const
{
	Coord coord_ul = this->screen_pos_to_coord(this->vpixmap.get_leftmost_pixel() + margin_left,    this->vpixmap.get_upmost_pixel() + margin_top);
	Coord coord_ur = this->screen_pos_to_coord(this->vpixmap.get_rightmost_pixel() + margin_right,  this->vpixmap.get_upmost_pixel() + margin_top);
	Coord coord_bl = this->screen_pos_to_coord(this->vpixmap.get_leftmost_pixel() + margin_left,    this->vpixmap.get_bottommost_pixel() + margin_bottom);
	Coord coord_br = this->screen_pos_to_coord(this->vpixmap.get_rightmost_pixel() + margin_right,  this->vpixmap.get_bottommost_pixel() + margin_bottom);

	coord_ul.recalculate_to_mode(CoordMode::LatLon);
	coord_ur.recalculate_to_mode(CoordMode::LatLon);
	coord_bl.recalculate_to_mode(CoordMode::LatLon);
	coord_br.recalculate_to_mode(CoordMode::LatLon);

	LatLonBBox bbox;
	bbox.north = std::max(coord_ul.lat_lon.lat, coord_ur.lat_lon.lat);
	bbox.south = std::min(coord_bl.lat_lon.lat, coord_br.lat_lon.lat);
	bbox.east  = std::max(coord_ur.lat_lon.lon, coord_br.lat_lon.lon);
	bbox.west  = std::min(coord_ul.lat_lon.lon, coord_bl.lat_lon.lon);
	bbox.validate();

	return bbox;
}




/**
   \brief Add an attribution/copyright to display on viewport

   @copyright: new attribution/copyright to display.
*/
sg_ret GisViewport::add_attribution(QString const & attribution)
{
	return this->decorations.add_attribution(attribution);
}




sg_ret GisViewport::add_logo(const GisViewportLogo & logo)
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
void GisViewport::compute_bearing(int x1, int y1, int x2, int y2, Angle & angle, Angle & base_angle)
{
	double len = sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
	double dx = (x2 - x1) / len * 10;
	double dy = (y2 - y1) / len * 10;

	angle.set_value(atan2(dy, dx) + M_PI_2);

	if (this->get_drawmode() == GisViewportDrawMode::UTM) {

		Coord test = this->screen_pos_to_coord(x1, y1);
		LatLon lat_lon = test.get_lat_lon();
		lat_lon.lat += this->get_viking_scale().y * this->vpixmap.get_height() / 11000.0; // about 11km per degree latitude /* TODO: get_height() or get_bottommost_pixel()? */

		test = Coord(LatLon::to_utm(lat_lon), CoordMode::UTM);
		const ScreenPos test_pos = this->coord_to_screen_pos(test);

		base_angle.set_value(M_PI - atan2(test_pos.x - x1, test_pos.y - y1));
		angle.set_value(angle.get_value() - base_angle.get_value());
	}

	angle.normalize();
}




void GisViewport::paintEvent(QPaintEvent * ev)
{
	qDebug() << SG_PREFIX_I;

	QPainter painter(this);

	painter.drawPixmap(0, 0, *this->vpixmap.pixmap);

	painter.setPen(Qt::blue);
	painter.setFont(QFont("Arial", 30));
	painter.drawText(this->rect(), Qt::AlignCenter, "SlavGPS");

	return;
}




void GisViewport::resizeEvent(QResizeEvent * ev)
{
	qDebug() << SG_PREFIX_I;

	this->reconfigure_drawing_area();

	/* TODO: call of this function should be triggered by signal. */
	ThisApp::get_main_window()->draw_tree_items();
	//this->draw_scale();

	return;
}





void GisViewport::mousePressEvent(QMouseEvent * ev)
{
	qDebug() << SG_PREFIX_I << "Mouse CLICK event, button" << (int) ev->button();

	this->window->get_toolbox()->handle_mouse_click(ev);

	ev->accept();
}




bool GisViewport::eventFilter(QObject * object, QEvent * ev)
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




void GisViewport::mouseMoveEvent(QMouseEvent * ev)
{
	this->draw_mouse_motion_cb(ev);

	this->window->get_toolbox()->handle_mouse_move(ev);

	emit this->cursor_moved(this, ev);

	ev->accept();
}




void GisViewport::mouseReleaseEvent(QMouseEvent * ev)
{
	qDebug() << SG_PREFIX_I << "called with button" << (int) ev->button();

	this->window->get_toolbox()->handle_mouse_release(ev);
	emit this->button_released(this, ev);

	ev->accept();
}




void GisViewport::wheelEvent(QWheelEvent * ev)
{
	QPoint angle = ev->angleDelta();
	qDebug() << SG_PREFIX_I << "Wheel event, buttons =" << (int) ev->buttons() << "angle =" << angle.y();
	ev->accept();

	const Qt::KeyboardModifiers modifiers = ev->modifiers();

	/* TODO: using get_width() and get_height() will give us only 99%-correct results. */
	const int w = this->vpixmap.get_width();
	const int h = this->vpixmap.get_height();
	const bool scroll_up = angle.y() > 0;


	switch (modifiers) {
	case Qt::ControlModifier:
		/* Control == pan up & down. */
		if (scroll_up) {
			this->set_center_coord(w / 2, h / 3);
		} else {
			this->set_center_coord(w / 2, h * 2 / 3);
		}
		break;

	case Qt::ShiftModifier:
		/* Shift == pan left & right. */
		if (scroll_up) {
			this->set_center_coord(w / 3, h / 2);
		} else {
			this->set_center_coord(w * 2 / 3, h / 2);
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
		const ScreenPos center_pos(this->vpixmap.get_horiz_center_pixel(), this->vpixmap.get_vert_center_pixel());
		const ScreenPos event_pos(ev->x(), ev->y());
		const ZoomOperation zoom_operation = SlavGPS::wheel_event_to_zoom_operation(ev);

		/* Clicked coordinate will be put after zoom at the same
		   position in viewport as before zoom.  Before zoom
		   the coordinate was under cursor, and after zoom it
		   will be still under cursor. */
		GisViewportZoom::keep_coordinate_under_cursor(zoom_operation, this, this->window, event_pos, center_pos);
	}
	default:
		/* Other modifier. Just ignore. */
		break;
	};

	qDebug() << SG_PREFIX_I << "Will call Window::draw_tree_items()";
	this->window->draw_tree_items();
}




void GisViewport::draw_mouse_motion_cb(QMouseEvent * ev)
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
	double zoom = this->get_viking_scale().get_x();
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
		/* TODO: add displaying of altitude in statusbar. */
		// altitude.convert_to_unit(Preferences::get_unit_height()).to_string()
		//message = QObject::tr("%1 %2").arg(coord_string).arg();
	}

	this->window->get_statusbar()->set_coord(coord);

#ifdef K_FIXME_RESTORE
	this->window->pan_move(ev);
#endif
}




GisViewport * GisViewport::create_scaled_viewport(Window * a_window, int target_width, int target_height, const VikingScale & expected_viking_scale)
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

	GisViewport * scaled_viewport = new GisViewport(a_window);

	const int orig_width = this->vpixmap.get_width();
	const int orig_height = this->vpixmap.get_height();

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
	scaled_viewport->set_center_coord(this->center_coord, false);



	/* Set zoom - either explicitly passed to the function, or
	   calculated implicitly. */
	if (expected_viking_scale.is_valid()) {
		scaled_viewport->set_viking_scale(expected_viking_scale);
	} else {
		const VikingScale calculated_viking_scale(this->viking_scale.x / scale_factor, this->viking_scale.y / scale_factor);
		if (calculated_viking_scale.is_valid()) {
			scaled_viewport->set_viking_scale(calculated_viking_scale);
		} else {
			/* TODO_HARD: now what? */
		}
	}



	snprintf(scaled_viewport->vpixmap.debug, sizeof (scaled_viewport->vpixmap.debug), "%s", "Scaled Viewport Pixmap");

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




bool GisViewport::print_cb(QPrinter * printer)
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

	GisViewport * scaled_viewport = this->create_scaled_viewport(this->window, target_width, target_height, VikingScale());

	/* Since we are printing viewport as it is, we allow existing
	   highlights to be drawn to print pixmap. */
	ThisApp::get_layers_panel()->draw_tree_items(scaled_viewport, true, false);


	QPainter printer_painter;
	printer_painter.begin(printer);
	QPoint paint_begin; /* Beginning of rectangle, into which we will paint in target device. */
	//paint_begin.setX((target_width / 2.0) - (scaled_viewport->vpixmap.width / 2.0));
	//paint_begin.setY((target_height / 2.0) - (scaled_viewport->vpixmap.height / 2.0));
	paint_begin.setX(0);
	paint_begin.setY(0);

	printer_painter.drawPixmap(paint_begin, *scaled_viewport->vpixmap.pixmap);
	printer_painter.end();

	delete scaled_viewport;

	qDebug() << SG_PREFIX_I << "target rectangle:" << target_rect;
	qDebug() << SG_PREFIX_I << "paint_begin:" << paint_begin;


	return true;
}




/*
  @pos is a position in viewport's pixmap.
  The function makes sure that the crosshair is drawn only inside of graph area.
*/
void Viewport2D::central_draw_simple_crosshair(const ScreenPos & pos)
{
	/* TODO: review, see if we should use get_width()/get_height() or get_bottommost_pixel()&co. */
	const int w = this->central->vpixmap.get_width();
	const int h = this->central->vpixmap.get_height();
	const int x = pos.x;
	const int y = h - pos.y - 1; /* Convert from "beginning in bottom-left corner" to "beginning in top-left corner" coordinate system. */

	qDebug() << SG_PREFIX_I << "Crosshair at" << x << y;

	if (x > w || y > h) {
		/* Position outside of graph area. */
		return;
	}

	this->central->vpixmap.painter->setPen(this->central->marker_pen);

	/* Small optimization: use QT's drawing primitives directly.
	   Remember that (0,0) screen position is in upper-left corner of viewport. */

	this->central->vpixmap.painter->drawLine(0, y, w, y); /* Horizontal line. */
	this->central->vpixmap.painter->drawLine(x, 0, x, h); /* Vertical line. */
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




QString GisViewportDrawModes::get_name(GisViewportDrawMode mode)
{
	switch (mode) {
	case GisViewportDrawMode::UTM:
		return QObject::tr("&UTM Mode");
	case GisViewportDrawMode::Expedia:
		return QObject::tr("&Expedia Mode");
	case GisViewportDrawMode::Mercator:
		return QObject::tr("&Mercator Mode");
	case GisViewportDrawMode::LatLon:
		return QObject::tr("&Lat/Lon Mode");
	default:
		qDebug() << SG_PREFIX_E << "Unexpected draw mode" << (int) mode;
		return "";
	}
}



QString GisViewportDrawModes::get_id_string(GisViewportDrawMode mode)
{
	QString mode_id_string;

	switch (mode) {
	case GisViewportDrawMode::UTM:
		mode_id_string = "utm";
		break;
	case GisViewportDrawMode::Expedia:
		mode_id_string = "expedia";
		break;
	case GisViewportDrawMode::Mercator:
		mode_id_string = "mercator";
		break;
	case GisViewportDrawMode::LatLon:
		mode_id_string = "latlon";
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected draw mode" << (int) mode;
		break;
	}

	return mode_id_string;
}




bool GisViewportDrawModes::set_draw_mode_from_file(GisViewport * gisview, const char * line)
{
	bool success = true;

	if (0 == strcasecmp(line, "utm")) {
		gisview->set_drawmode(GisViewportDrawMode::UTM);

	} else if (0 == strcasecmp(line, "expedia")) {
		gisview->set_drawmode(GisViewportDrawMode::Expedia);

	} else if (0 == strcasecmp(line, "google")) {
		success = false;
		qDebug() << SG_PREFIX_W << QObject::tr("Read file: draw mode 'google' no longer supported");

	} else if (0 == strcasecmp(line, "kh")) {
		success = false;
		qDebug() << SG_PREFIX_W << QObject::tr("Read file: draw mode 'kh' no more supported");

	} else if (0 == strcasecmp(line, "mercator")) {
		gisview->set_drawmode(GisViewportDrawMode::Mercator);

	} else if (0 == strcasecmp(line, "latlon")) {
		gisview->set_drawmode(GisViewportDrawMode::LatLon);
	} else {
		qDebug() << SG_PREFIX_E << QObject::tr("Read file: unexpected draw mode") << line;
		success = false;
	}

	return success;
}




sg_ret GisViewport::set_bbox(const LatLonBBox & new_bbox)
{
	return GisViewportZoom::zoom_to_show_bbox(this, this->get_coord_mode(), new_bbox);
}




void GisViewport::request_redraw(const QString & trigger_descr)
{
	this->emit_center_coord_or_zoom_changed(trigger_descr);
}




/* Tell QT what type of MIME data we accept. */
void GisViewport::dragEnterEvent(QDragEnterEvent * event)
{
	if (event->mimeData()->hasFormat("text/plain")) {
		event->acceptProposedAction();
	}

}




void GisViewport::dropEvent(QDropEvent * event)
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




bool GisViewport::is_ready(void) const
{
	return this->vpixmap.pixmap != NULL;
}




/**
   To be called when action initiated in GisViewport has changed center
   of viewport or zoom of viewport.
*/
void GisViewport::emit_center_coord_or_zoom_changed(const QString & trigger_name)
{
	qDebug() << SG_PREFIX_SIGNAL << "Will emit 'center or zoom changed' signal triggered by" << trigger_name;
	emit this->center_coord_or_zoom_changed();
}




Window * GisViewport::get_window(void) const
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

	this->central = new GisViewport(this);
	this->central->vpixmap.parent_viewport = this;

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

	this->left->parent_viewport   = this;
	this->right->parent_viewport  = this;
	this->top->parent_viewport    = this;
	this->bottom->parent_viewport = this;

	return;
}




void ViewportMargin::paintEvent(QPaintEvent * ev)
{
	qDebug() << SG_PREFIX_I;

	QPainter event_painter(this);
	event_painter.drawPixmap(0, 0, *this->pixmap);

	return;
}




void ViewportMargin::resizeEvent(QResizeEvent * ev)
{
	qDebug() << SG_PREFIX_I;

	this->reconfigure(this->geometry().width(), this->geometry().height());
	this->painter->setPen(this->border_pen);

	switch (this->position) {
	case ViewportMargin::Position::Left:
		this->painter->drawText(this->rect(), Qt::AlignCenter, "left");
		this->painter->drawLine(this->geometry().width() - 1, 0,
					this->geometry().width() - 1, this->geometry().height() - 1);
		break;

	case ViewportMargin::Position::Right:
		this->painter->drawLine(1, 1,
					1, this->geometry().height() - 1);
		this->painter->drawText(this->rect(), Qt::AlignCenter, "right");
		break;

	case ViewportMargin::Position::Top:
		this->painter->drawLine(0,                            this->geometry().height() - 1,
					this->geometry().width() - 1, this->geometry().height() - 1);
		this->painter->drawText(this->rect(), Qt::AlignCenter, "top");
		break;

	case ViewportMargin::Position::Bottom:
		this->painter->drawLine(1,                            1,
					this->geometry().width() - 1, 1);
		this->painter->drawText(this->rect(), Qt::AlignCenter, "bottom");
		break;

	default:
		qDebug() << SG_PREFIX_E << "Unhandled margin position";
		break;
	}

	return;
}




ViewportMargin::ViewportMargin(ViewportMargin::Position pos, int main_size, QWidget * parent) : ViewportPixmap(parent)
{
	this->position = pos;
	this->size = main_size;

	this->border_pen.setColor(QColor("black"));
	this->border_pen.setWidth(2);

	switch (this->position) {
	case ViewportMargin::Position::Left:
		this->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
		this->setMinimumSize(size, 10);
		snprintf(this->debug, sizeof (this->debug), "%s", "left");
		break;

	case ViewportMargin::Position::Right:
		this->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
		this->setMinimumSize(size, 10);
		snprintf(this->debug, sizeof (this->debug), "%s", "right");
		break;

	case ViewportMargin::Position::Top:
		this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		this->setMinimumSize(10, size);
		snprintf(this->debug, sizeof (this->debug), "%s", "top");
		break;

	case ViewportMargin::Position::Bottom:
		this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		this->setMinimumSize(10, size);
		snprintf(this->debug, sizeof (this->debug), "%s", "bottom");
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled margin position";
		break;
	}
}




int Viewport2D::central_get_width(void) const
{
	return this->central ? this->central->vpixmap.get_width() : 0;
}

int Viewport2D::central_get_height(void) const
{
	return this->central ? this->central->vpixmap.get_height() : 0;
}

int Viewport2D::left_get_width(void) const
{
	return this->left ? this->left->get_width() : 0;
}

int Viewport2D::left_get_height(void) const
{
	return this->left ? this->left->get_height() : 0;
}

int Viewport2D::right_get_width(void) const
{
	return this->right ? this->right->get_width() : 0;
}

int Viewport2D::right_get_height(void) const
{
	return this->right ? this->right->get_height() : 0;
}

int Viewport2D::top_get_width(void) const
{
	return this->top ? this->top->get_width() : 0;
}

int Viewport2D::top_get_height(void) const
{
	return this->top ? this->top->get_height() : 0;
}

int Viewport2D::bottom_get_width(void) const
{
	return this->bottom ? this->bottom->get_width() : 0;
}

int Viewport2D::bottom_get_height(void) const
{
	return this->bottom ? this->bottom->get_height() : 0;
}




sg_ret GisViewport::get_cursor_pos(QMouseEvent * ev, ScreenPos & screen_pos) const
{
	/* TODO: review this function and see if we need to use
	   get_width()/get_height() or maybe
	   get_bottommost_pixel()&co. */
	const int w = this->vpixmap.get_width();
	const int h = this->vpixmap.get_height();

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




double GisViewport::get_vpixmap_height_m(void) const
{
	return this->vpixmap.get_height() * this->viking_scale.y;
}




double GisViewport::get_vpixmap_width_m(void) const
{
	return this->vpixmap.get_width() * this->viking_scale.x;
}
