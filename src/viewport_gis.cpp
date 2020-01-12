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
#include "widget_measurement_entry.h"
#include "application_state.h"
#include "dialog.h"
#include "statusbar.h"
#include "expedia.h"




using namespace SlavGPS;




#define SG_MODULE "GisViewport"




#define MERCATOR_FACTOR(_mpp_) ((65536.0 / 180 / (_mpp_)) * 256.0)
/* TODO_LATER: Form of this expression should be optimized for usage in denominator. */
#define REVERSE_MERCATOR_FACTOR(_mpp_) ((65536.0 / 180 / (_mpp_)) * 256.0)

#define VIK_SETTINGS_VIEW_LAST_LATITUDE     "viewport_last_latitude"
#define VIK_SETTINGS_VIEW_LAST_LONGITUDE    "viewport_last_longitude"
#define VIK_SETTINGS_VIEW_LAST_ZOOM_X       "viewport_last_zoom_xpp"
#define VIK_SETTINGS_VIEW_LAST_ZOOM_Y       "viewport_last_zoom_ypp"
#define VIK_SETTINGS_VIEW_HISTORY_SIZE      "viewport_history_size"
#define VIK_SETTINGS_VIEW_HISTORY_DIFF_DIST "viewport_history_diff_dist"




/**
   @reviewed-on tbd
*/
void GisViewport::init(void)
{
	Expedia::init_radius();
}




/**
   @reviewed-on tbd
*/
double GisViewport::calculate_utm_zone_width(void) const
{
	switch (this->coord_mode) {
	case CoordMode::UTM: {

		/* Get latitude of screen bottom. */
		UTM utm = this->center_coord.utm;
		const double center_to_bottom_m = this->central_get_height_m() / 2;
		utm.shift_northing_by(-center_to_bottom_m);
		LatLon lat_lon = UTM::to_lat_lon(utm);

		/* Boundary. */
		lat_lon.lon = (utm.get_zone() - 1) * 6 - 180 ;
		utm = LatLon::to_utm(lat_lon);
		return std::fabs(utm.get_easting() - UTM_CENTRAL_MERIDIAN_EASTING) * 2;
	}

	case CoordMode::LatLon:
		return 0.0;

	default:
		qDebug() << SG_PREFIX_E << "Unexpected coord mode:" << this->coord_mode;
		return 0.0;
	}
}




/**
   @reviewed-on tbd
*/
GisViewport::GisViewport(int left, int right, int top, int bottom, QWidget * parent) : ViewportPixmap(left, right, top, bottom, parent)
{
	this->window = ThisApp::get_main_window();


	this->installEventFilter(this);


	this->setMinimumSize(200, 300);
	snprintf(this->debug, sizeof (this->debug), "%s", "center");
	//this->setMaximumSize(2700, 2700);



	LatLon initial_lat_lon = Preferences::get_default_lat_lon();
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


	if (!ApplicationState::get_integer(VIK_SETTINGS_VIEW_HISTORY_SIZE, &this->center_coords.max_items)) {
		this->center_coords.max_items = 20;
	}
	if (ApplicationState::get_integer(VIK_SETTINGS_VIEW_HISTORY_DIFF_DIST, &this->center_coords.radius)) {
		this->center_coords.radius = 500;
	}


	this->set_center_coord(initial_lat_lon); /* The function will reject latlon if it's invalid. */


	this->setFocusPolicy(Qt::ClickFocus);

	this->scale_visibility = true;

}




/**
   @reviewed-on tbd
*/
GisViewport::GisViewport(int new_total_width, int new_total_height, int left, int right, int top, int bottom, QWidget * parent)
	: GisViewport(left, right, top, bottom, parent)
{
	qDebug() << SG_PREFIX_I << "Resizing new viewport to width =" << new_total_width << ", height =" << new_total_height;
	this->resize(new_total_width, new_total_height);
	this->apply_total_sizes(new_total_width, new_total_height);
}




/**
   @reviewed-on tbd
*/
GisViewport * GisViewport::copy(int target_total_width, int target_total_height, QWidget * parent) const
{
	const double scale = 1.0 * target_total_width / this->total_get_width();
	return this->copy(target_total_width, target_total_height, this->viking_scale / scale, parent);
}




/**
   @reviewed-on tbd
*/
GisViewport * GisViewport::copy(int target_total_width, int target_total_height, const VikingScale & target_viking_scale, QWidget * parent) const
{
	if (!target_viking_scale.is_valid()) {
		qDebug() << SG_PREFIX_E << "Invalid 'viking scale' argument";
		return NULL;
	}

	const double scale = 1.0 * target_total_width / this->total_get_width();

	GisViewport * new_object = new GisViewport(target_total_width,
						   target_total_height,
						   floor(scale * this->left_margin_width),
						   floor(scale * this->right_margin_width),
						   floor(scale * this->top_margin_height),
						   floor(scale * this->bottom_margin_height),
						   parent);

	snprintf(new_object->debug, sizeof (new_object->debug), "Copy of %s", this->debug);

	new_object->set_draw_mode(this->get_draw_mode());
	new_object->set_coord_mode(this->get_coord_mode());
	new_object->set_center_coord(this->center_coord, false);
	new_object->set_viking_scale(target_viking_scale);
	//new_object->set_bbox(this->get_bbox()); /* TODO_LATER: why without this the scaling works correctly? */

	qDebug() << SG_PREFIX_I << "Original viewport's bbox =" << this->get_bbox();
	qDebug() << SG_PREFIX_I << "Scaled viewport's bbox =  " << new_object->get_bbox();

	qDebug() << SG_PREFIX_I << "Original viewport:";
	this->debug_print_info();
	qDebug() << SG_PREFIX_I << "Scaled viewport:";
	new_object->debug_print_info();


	return new_object;
}




/**
   @reviewed-on tbd
*/
GisViewport::~GisViewport()
{
	qDebug() << SG_PREFIX_I << "Deleting viewport" << QString(this->debug);
	if (Preferences::get_startup_method() == StartupMethod::LastLocation) {
		const LatLon lat_lon = this->center_coord.get_lat_lon();
		ApplicationState::set_double(VIK_SETTINGS_VIEW_LAST_LATITUDE, lat_lon.lat);
		ApplicationState::set_double(VIK_SETTINGS_VIEW_LAST_LONGITUDE, lat_lon.lon);

		ApplicationState::set_double(VIK_SETTINGS_VIEW_LAST_ZOOM_X, this->viking_scale.x);
		ApplicationState::set_double(VIK_SETTINGS_VIEW_LAST_ZOOM_Y, this->viking_scale.y);
	}
}




/**
   \brief Clear the whole viewport

   @reviewed-on tbd
*/
void GisViewport::clear(void)
{
	qDebug() << SG_PREFIX_I << "Clear whole viewport" << QString(this->debug);

	/* Some maps may have been removed, so their logos and/or
	   attributions/copyrights must be cleared as well. */
	this->decorations.clear();

	ViewportPixmap::clear();
}




/**
   @reviewed-on 2019-07-21
*/
void GisViewport::draw_decorations(void)
{
	if (1) {
		/*
		  Debug. Fake attribution strings to verify display of
		  attributions.
		*/
		this->decorations.add_attribution("© Test attribution holder 1");
		this->decorations.add_attribution("© Another test attribution holder 2017-2019");
	}

	if (1) {
		/*
		  Debug. Fake logo pixmaps to verify display of logos.
		*/

		GisViewportLogo logo;

		logo.logo_id = ":/test_data/pixmap_checkered_black_alpha.png";
		logo.logo_pixmap = QPixmap(logo.logo_id);
		this->decorations.add_logo(logo);

		/* This pixmap is smaller than ViewportDecoration's
		   MAX_LOGO_HEIGHT, so it shouldn't be scaled down by
		   ViewportDecorations. It will be displayed with its original
		   size. */
		logo.logo_id = ":/test_data/pixmap_16x16.png";
		logo.logo_pixmap = QPixmap(logo.logo_id);
		this->decorations.add_logo(logo);

		logo.logo_id = ":/test_data/test_pixmap_2.png";
		logo.logo_pixmap = QPixmap(logo.logo_id);
		this->decorations.add_logo(logo);

		logo.logo_id = ":/test_data/test_pixmap_3.png";
		logo.logo_pixmap = QPixmap(logo.logo_id);
		this->decorations.add_logo(logo);
	}

	this->decorations.draw(*this);

	return;
}




/**
   @reviewed-on 2019-07-21
*/
void GisViewport::debug_draw_debugs(void)
{
	this->debug_gisviewport_draw();
	this->debug_pixmap_draw();
}




/**
   @reviewed-on tbd
*/
void GisViewport::debug_gisviewport_draw(void)
{
	const int padding = 10;

	/* Use additional margins to prevent overlap of debugs with
	   additional elements placed at the top and bottom of
	   viewport.

	   Using padding + these margins means that debugs' bounding
	   rect is smaller than actual central part of viewport. */
	const int top_protection = 40;
	const int bottom_protection = 100;
	const QRectF bounding_rect = QRectF(this->central_get_leftmost_pixel() + padding,
					    this->central_get_topmost_pixel() + padding + top_protection,
					    this->central_get_width() - 2 * padding,
					    this->central_get_height() - 2 * bottom_protection);

	/* These debugs are really useful. Don't be shy about them,
	   print them large and readable. */
	QFont font = QFont("Helvetica", 12);
	font.setBold(true);
	QPen pen(QColor("black"));


	if (1) { /* Bounding Box lat/lon information. */
		const LatLonBBox bbox = this->get_bbox();
		const QString north = "bbox: " + bbox.north.to_string();
		const QString west =  "bbox: " + bbox.west.to_string();
		const QString east =  "bbox: " + bbox.east.to_string();
		const QString south = "bbox: " + bbox.south.to_string();
		this->draw_text(font, pen, bounding_rect, Qt::AlignTop | Qt::AlignHCenter, north);
		this->draw_text(font, pen, bounding_rect, Qt::AlignVCenter | Qt::AlignRight, east);
		this->draw_text(font, pen, bounding_rect, Qt::AlignVCenter | Qt::AlignLeft, west);
		this->draw_text(font, pen, bounding_rect, Qt::AlignBottom | Qt::AlignHCenter, south);
	}


	if (1) { /* Width/height of central area. */
		const QString size = QString("central width = %1\ncentral height = %2").arg(this->central_get_width()).arg(this->central_get_height());
		this->draw_text(font, pen, bounding_rect, Qt::AlignVCenter | Qt::AlignHCenter, size);
	}


	if (1) { /* Geo coordinates of corners of central area. */
		Coord coord_ul = this->screen_pos_to_coord(ScreenPosition::UpperLeft);
		Coord coord_ur = this->screen_pos_to_coord(ScreenPosition::UpperRight);
		Coord coord_bl = this->screen_pos_to_coord(ScreenPosition::BottomLeft);
		Coord coord_br = this->screen_pos_to_coord(ScreenPosition::BottomRight);

		QString ul = "ul: ";
		QString ur = "ur: ";
		QString bl = "bl: ";
		QString br = "br: ";

		switch (this->coord_mode) {
		case CoordMode::UTM:
			/* UTM first, then LatLon. */
			ul += coord_ul.get_utm().to_string() + "\n";
			ul += coord_ul.get_lat_lon().to_string();
			ur += coord_ur.get_utm().to_string() + "\n";
			ur += coord_ur.get_lat_lon().to_string();
			bl += coord_bl.get_utm().to_string() + "\n";
			bl += coord_bl.get_lat_lon().to_string();
			br += coord_br.get_utm().to_string() + "\n";
			br += coord_br.get_lat_lon().to_string();
			break;

		case CoordMode::LatLon:
			/* LatLon first, then UTM. */
			ul += coord_ul.get_lat_lon().to_string() + "\n";
			ul += coord_ul.get_utm().to_string();
			ur += coord_ur.get_lat_lon().to_string() + "\n";
			ur += coord_ur.get_utm().to_string();
			bl += coord_bl.get_lat_lon().to_string() + "\n";
			bl += coord_bl.get_utm().to_string();
			br += coord_br.get_lat_lon().to_string() + "\n";
			br += coord_br.get_utm().to_string();
			break;
		default:
			qDebug() << SG_PREFIX_E << "Unexpected coord mode:" << this->coord_mode;
			break;
		}
		ul.replace("Zone", "\nZone"); /* Coordinate strings will get very long. Put zone + band in new line. */
		ur.replace("Zone", "\nZone");
		bl.replace("Zone", "\nZone");
		br.replace("Zone", "\nZone");
		this->draw_text(font, pen, bounding_rect, Qt::AlignTop    | Qt::AlignLeft,  ul);
		this->draw_text(font, pen, bounding_rect, Qt::AlignTop    | Qt::AlignRight, ur);
		this->draw_text(font, pen, bounding_rect, Qt::AlignBottom | Qt::AlignLeft,  bl);
		this->draw_text(font, pen, bounding_rect, Qt::AlignBottom | Qt::AlignRight, br);
	}


	this->draw_rectangle(QPen(QColor("maroon")), bounding_rect);
}




/**
   \brief Enable/Disable display of center mark

   @reviewed-on 2019-07-20
*/
void GisViewport::set_center_mark_visibility(bool new_state)
{
	this->center_mark_visibility = new_state;
}




/**
   @reviewed-on 2019-07-20
*/
bool GisViewport::get_center_mark_visibility(void) const
{
	return this->center_mark_visibility;
}




/**
   \brief Enable/Disable display of scale

   @reviewed-on 2019-07-20
*/
void GisViewport::set_scale_visibility(bool new_state)
{
	this->scale_visibility = new_state;
}




/**
   @reviewed-on 2019-07-20
*/
bool GisViewport::get_scale_visibility(void) const
{
	return this->scale_visibility;
}




/**
   @reviewed-on tbd
*/
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

	if (this->draw_mode == GisViewportDrawMode::UTM) {
		this->utm_zone_check();
	}

	return sg_ret::ok;
}




/**
   @reviewed-on tbd
*/
void GisViewport::zoom_in_on_center_pixel(void)
{
	if (this->viking_scale.zoom_in(2)) {
		this->xmfactor = MERCATOR_FACTOR(this->viking_scale.x);
		this->ymfactor = MERCATOR_FACTOR(this->viking_scale.y);

		this->utm_zone_check();
	}
}




/**
   @reviewed-on tbd
*/
void GisViewport::zoom_out_on_center_pixel(void)
{
	if (this->viking_scale.zoom_out(2)) {
		this->xmfactor = MERCATOR_FACTOR(this->viking_scale.x);
		this->ymfactor = MERCATOR_FACTOR(this->viking_scale.y);

		this->utm_zone_check();
	}
}




/**
   @reviewed-on 2019-07-20
*/
const VikingScale & GisViewport::get_viking_scale(void) const
{
	return this->viking_scale;
}




/**
   @reviewed-on tbd
*/
sg_ret GisViewport::set_viking_scale(const VikingScale & new_value)
{
	if (new_value.is_valid()) {
		this->viking_scale = new_value;
		return sg_ret::ok;
	} else {
		qDebug() << SG_PREFIX_E << "New value is invalid";
		return sg_ret::err;
	}
}




/**
   @reviewed-on tbd
*/
sg_ret GisViewport::set_viking_scale_x(double new_value)
{
	if (!VikingScale::value_is_valid(new_value)) {
		qDebug() << SG_PREFIX_E << "Failed to set new zoom level, invalid value" << new_value;
		return sg_ret::err;
	}

	this->viking_scale.x = new_value;
	this->xmfactor = MERCATOR_FACTOR(this->viking_scale.x);
	if (this->draw_mode == GisViewportDrawMode::UTM) {
		this->utm_zone_check();
	}

	return sg_ret::ok;
}




/**
   @reviewed-on tbd
*/
sg_ret GisViewport::set_viking_scale_y(double new_value)
{
	if (!VikingScale::value_is_valid(new_value)) {
		qDebug() << SG_PREFIX_E << "Failed to set new zoom level, invalid value" << new_value;
		return sg_ret::err;
	}

	this->viking_scale.y = new_value;
	this->ymfactor = MERCATOR_FACTOR(this->viking_scale.y);
	if (this->draw_mode == GisViewportDrawMode::UTM) {
		this->utm_zone_check();
	}

	return sg_ret::ok;
}




/**
   @reviewed-on 2019-07-20
*/
const Coord & GisViewport::get_center_coord(void) const
{
	return this->center_coord;
}




/*
  Called every time we update coordinates/zoom.

  @reviewed-on tbd
*/
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

   @reviewed-on tbd
*/
void CenterCoords::remove_item(std::list<Coord>::iterator iter)
{
	if (iter == this->current_iter) {
		this->current_iter = this->erase(iter);

		if (this->current_iter == this->end()) {
			/* We have removed last element on the list. */

			if (this->empty()) {
				this->current_iter = this->begin();
			} else {
				this->current_iter--;
			}
		}
	} else {
		this->erase(iter);
	}
}




/**
   @reviewed-on tbd
*/
void GisViewport::save_current_center_coord(void)
{
	if (this->center_coords.current_iter == prev(this->center_coords.end())) {
		/* We are at most recent element of history. */
		if (this->center_coords.size() == (unsigned) this->center_coords.max_items) {
			/* List is full, so drop the oldest value to make room for the new one. */
			this->center_coords.remove_item(this->center_coords.begin());
		}
	} else {
		/* We are somewhere in the middle of history list, possibly at the beginning.
		   Every center visited after current one must be discarded. */
		this->center_coords.erase(next(this->center_coords.current_iter), this->center_coords.end());
		assert (std::next(this->center_coords.current_iter) == this->center_coords.end());
	}

	/* Store new position. */
	/* push_back() puts at the end. By convention end == newest. */
	this->center_coords.push_back(this->center_coord);
	this->center_coords.current_iter++;
	assert (std::next(this->center_coords.current_iter) == this->center_coords.end());

	this->print_center_coords("GisViewport::save_current_center_coord()");

	qDebug() << SG_PREFIX_SIGNAL << "Emitting list_of_center_coords_changed()";
	emit this->list_of_center_coords_changed(this);
}




/**
   @reviewed-on tbd
*/
std::list<QString> GisViewport::get_center_coords_list(void) const
{
	std::list<QString> result;

	for (auto iter = this->center_coords.begin(); iter != this->center_coords.end(); iter++) {

		QString extra;
		if (iter == prev(this->center_coords.current_iter)) {
			extra = tr("[Back]");
		} else if (iter == next(this->center_coords.current_iter)) {
			extra = tr("[Forward]");
		} else if (iter == this->center_coords.current_iter) {
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

   @reviewed-on tbd
*/
void GisViewport::show_center_coords(Window * parent_window) const
{
	const std::list<QString> texts = this->get_center_coords_list();

	/* Using this function the dialog allows sorting of the list which isn't appropriate here
	   but this doesn't matter much for debug purposes of showing stuff... */
	const QStringList headers = { tr("Back/Forward Locations") };
	ListSelectionDialog<QString> dialog(tr("Back/Forward Locations"), ListSelectionMode::SingleItem, headers, parent_window);
	dialog.set_list(texts);
	if (QDialog::Accepted == dialog.exec()) {
		/* We don't have to get the list of results, but for
		   debug purposes lets get it and display it. */
		const std::list<QString> result = dialog.get_selection();
		/* Because we have used ListSelectionMode::SingleItem
		   selection mode, this list has at most one
		   element. */
		if (!result.empty()) {
			qDebug() << SG_PREFIX_D << "History center item:" << *result.begin();
		}
	}
}




/**
   @reviewed-on tbd
*/
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

   @reviewed-on tbd

   @return true on success
   @return false otherwise
*/
bool GisViewport::go_back(void)
{
	/* See if the current position is different from the
	   last saved center position within a certain radius. */
	if (Coord::distance(*this->center_coords.current_iter, this->center_coord) > this->center_coords.radius) {

		if (this->center_coords.current_iter == prev(this->center_coords.end())) {
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
	this->center_coords.current_iter--;
	this->set_center_coord(*this->center_coords.current_iter, false);

	/* Inform about center_coords.current_iter being changed. */
	qDebug() << SG_PREFIX_SIGNAL << "Emitting list_of_center_coords_changed() after going back";
	emit this->list_of_center_coords_changed(this);

	return true;
}




/**
   \brief Move forward in the position history

   @reviewed-on tbd

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
	this->center_coords.current_iter++;
	this->set_center_coord(*this->center_coords.current_iter, false);

	/* Inform about center_coords.current_iter being changed. */
	qDebug() << SG_PREFIX_SIGNAL << "Emitting list_of_center_coords_changed() after going forward";
	emit this->list_of_center_coords_changed(this);

	return true;
}




/**
   @reviewed-on tbd

   @return true when a previous position in the history is available
   @return false otherwise
*/
bool GisViewport::back_available(void) const
{
	return (this->center_coords.size() > 1 && this->center_coords.current_iter != this->center_coords.begin());
}




/**
   @reviewed-on tbd

   @return true when a next position in the history is available
   @return false otherwise
*/
bool GisViewport::forward_available(void) const
{
	return (this->center_coords.size() > 1 && this->center_coords.current_iter != prev(this->center_coords.end()));
}




/**
   @reviewed-on tbd

   @lat_lon:       The new center position in Lat/Lon format
   @save_position: Whether this new position should be saved into the history of positions
                   Normally only specific user requests should be saved (i.e. to not include Pan and Zoom repositions)
*/
sg_ret GisViewport::set_center_coord(const LatLon & lat_lon, bool save_position)
{
	if (!lat_lon.is_valid()) {
		qDebug() << SG_PREFIX_E << "Not setting center coord from lat/lon, value is invalid:" << lat_lon;
		return sg_ret::err;
	}

	return this->set_center_coord(Coord(lat_lon, this->coord_mode), save_position);
}




/**
   @reviewed-on tbd

   @utm:           The new center position in UTM format
   @save_position: Whether this new position should be saved into the history of positions
                   Normally only specific user requests should be saved (i.e. to not include Pan and Zoom repositions)
*/
sg_ret GisViewport::set_center_coord(const UTM & utm, bool save_position)
{
	return this->set_center_coord(Coord(utm, this->coord_mode), save_position);
}




/**
   @reviewed-on tbd

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




/**
   @reviewed-on tbd
*/
sg_ret GisViewport::set_center_coord(fpixel x1, fpixel y1)
{
	const Coord coord = this->screen_pos_to_coord(x1, y1);
	return this->set_center_coord(coord, false);
}




/**
   @reviewed-on tbd
*/
sg_ret GisViewport::set_center_coord(const ScreenPos & pos)
{
	return this->set_center_coord(pos.x(), pos.y());
}




/**
   @reviewed-on tbd
*/
sg_ret GisViewport::get_corners_for_zone(Coord & coord_ul, Coord & coord_br, int zone)
{
	if (this->coord_mode != CoordMode::UTM) {
		qDebug() << SG_PREFIX_E << "Coord mode is not UTM:" << this->coord_mode;
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
	const double center_to_top_m = this->central_get_height_m() / 2;
	const double center_to_left_m = this->central_get_width_m() / 2;
	coord_ul.utm.shift_northing_by(center_to_top_m);
	coord_ul.utm.shift_easting_by(-center_to_left_m);

	const double center_to_bottom_m = this->central_get_height_m() / 2;
	const double center_to_right_m = this->central_get_width_m() / 2;
	coord_br.utm.shift_northing_by(-center_to_bottom_m);
	coord_br.utm.shift_easting_by(center_to_right_m);

	return sg_ret::ok;
}




/**
   @reviewed-on tbd
*/
sg_ret GisViewport::utm_recalculate_current_center_coord_for_other_zone(UTM & center_in_other_zone, int zone)
{
	if (this->coord_mode != CoordMode::UTM) {
		qDebug() << SG_PREFIX_E << "Coord mode is not UTM:" << this->coord_mode;
		return sg_ret::err;
	}

	const int zone_diff = zone - this->center_coord.utm.get_zone();

	/* TODO_LATER: why do we have to offset easting? Wouldn't easting of center be the same in each zone? */
	center_in_other_zone = this->center_coord.utm;
	center_in_other_zone.shift_easting_by(-(zone_diff * this->utm_zone_width));
	center_in_other_zone.set_zone(zone);

	return sg_ret::ok;
}




/**
   @reviewed-on tbd
*/
int GisViewport::get_leftmost_zone(void) const
{
	if (coord_mode != CoordMode::UTM) {
		return 0;
	}

	const Coord coord = this->screen_pos_to_coord(ScreenPosition::UpperLeft); /* Upper/Lower argument shouldn't really matter, because we want to get "leftmost" zone. */
	return coord.utm.get_zone();
}




/**
   @reviewed-on tbd
*/
int GisViewport::get_rightmost_zone(void) const
{
	if (coord_mode != CoordMode::UTM) {
		return 0;
	}

	const Coord coord = this->screen_pos_to_coord(ScreenPosition::UpperRight); /* Upper/Lower position shouldn't really matter, we just want to get "rightmost" zone. */
	return coord.utm.get_zone();
}




/**
   @reviewed-on tbd
*/
Coord GisViewport::screen_pos_to_coord(ScreenPosition screen_pos) const
{
	Coord result;
	int pos_x = 0;
	int pos_y = 0;

	switch (screen_pos) {
	case ScreenPosition::UpperLeft:
		pos_x = this->central_get_leftmost_pixel();
		pos_y = this->central_get_topmost_pixel();;
		result = this->screen_pos_to_coord(pos_x, pos_y);
		break;

	case ScreenPosition::UpperRight:
		pos_x = this->central_get_rightmost_pixel();
		pos_y = this->central_get_topmost_pixel();
		result = this->screen_pos_to_coord(pos_x, pos_y);
		break;

	case ScreenPosition::BottomLeft:
		pos_x = this->central_get_leftmost_pixel();
		pos_y = this->central_get_bottommost_pixel();
		result = this->screen_pos_to_coord(pos_x, pos_y);
		break;

	case ScreenPosition::BottomRight:
		pos_x = this->central_get_rightmost_pixel();
		pos_y = this->central_get_bottommost_pixel();
		result = this->screen_pos_to_coord(pos_x, pos_y);
		break;

	default:
		qDebug() << SG_PREFIX_E << "Unexpected screen position" << (int) screen_pos;
		break;
	}

	return result;
}




/**
   @reviewed-on tbd
*/
Coord GisViewport::screen_pos_to_coord(fpixel pos_x, fpixel pos_y) const
{
	Coord coord;
	const double xmpp = this->viking_scale.x;
	const double ympp = this->viking_scale.y;

	/* Distance of pixel specified by pos_x/pos_y from viewport's
	   central pixel.  TODO_LATER: verify location of pos_x and pos_y in
	   these equations. */
	const fpixel delta_x_pixels = pos_x - this->central_get_x_center_pixel();
	const fpixel delta_y_pixels = this->central_get_y_center_pixel() - pos_y;

	switch (this->coord_mode) {
	case CoordMode::UTM:
		coord.set_coord_mode(CoordMode::UTM);

		/* Modified (reformatted) formula. */
		{
			coord.utm.set_northing((delta_y_pixels * ympp) + this->center_coord.utm.get_northing());
			coord.utm.set_easting((delta_x_pixels * xmpp) + this->center_coord.utm.get_easting());
			coord.utm.set_zone(this->center_coord.utm.get_zone());

			const int zone_delta = floor((coord.utm.easting - UTM_CENTRAL_MERIDIAN_EASTING) / this->utm_zone_width + 0.5);
			coord.utm.shift_zone_by(zone_delta);
			coord.utm.shift_easting_by(-(zone_delta * this->utm_zone_width));

			/* Calculate correct band letter.  TODO_LATER: there
			   has to be an easier way to do this. */
			{
				/* We only need this initial
				   assignment to avoid assertion fail
				   in ::to_lat_lon(). */
				assert (UTM::is_band_letter(this->center_coord.utm.get_band_letter())); /* TODO_LATER: add smarter error handling. In theory the source object should be valid and for sure contain valid band letter. */
				coord.utm.set_band_letter(this->center_coord.utm.get_band_letter());

				/* Calculated lat_lon will contain
				   information about latitude, and
				   converting latitude to band letter
				   is piece of cake. */
				LatLon lat_lon = UTM::to_lat_lon(coord.utm);
				UTM utm = LatLon::to_utm(lat_lon);
				coord.utm.set_band_letter(utm.get_band_letter());
			}
		}


		/* Original code, used for comparison of results with new, reformatted formula. */
		{
			int zone_delta = 0;

			Coord test_coord;
			test_coord.set_coord_mode(CoordMode::UTM);

			test_coord.utm.set_zone(this->center_coord.utm.get_zone());
			assert (UTM::is_band_letter(this->center_coord.utm.get_band_letter())); /* TODO_LATER: add smarter error handling. In theory the source object should be valid and for sure contain valid band letter. */
			test_coord.utm.set_band_letter(this->center_coord.utm.get_band_letter());
			test_coord.utm.easting = (delta_x_pixels * xmpp) + this->center_coord.utm.easting;

			zone_delta = floor((test_coord.utm.easting - UTM_CENTRAL_MERIDIAN_EASTING) / this->utm_zone_width + 0.5);

			test_coord.utm.shift_zone_by(zone_delta);
			test_coord.utm.easting -= zone_delta * this->utm_zone_width;
			test_coord.utm.northing = (delta_y_pixels * ympp) + this->center_coord.utm.northing;


			if (!UTM::is_the_same_zone(coord.utm, test_coord.utm)) {
				qDebug() << SG_PREFIX_E << "UTM: Zone calculation mismatch" << coord << test_coord << coord.utm.get_zone() << test_coord.utm.get_zone();
			}
			if (coord.utm.get_easting() != test_coord.utm.get_easting()) {
				qDebug() << SG_PREFIX_E << "UTM: Easting calculation mismatch" << coord << test_coord << (coord.utm.get_easting() - test_coord.utm.get_easting());
			}
			if (coord.utm.get_northing() != test_coord.utm.get_northing()) {
				qDebug() << SG_PREFIX_E << "UTM: Northing calculation mismatch" << coord << test_coord << (coord.utm.get_northing() - test_coord.utm.get_northing());
			}
		}
		break;

	case CoordMode::LatLon:
		coord.set_coord_mode(CoordMode::LatLon);

		switch (this->draw_mode) {
		case GisViewportDrawMode::LatLon:
			/* Modified (reformatted) formula. */
			{
				coord.lat_lon.lon = this->center_coord.lat_lon.lon + (delta_x_pixels / REVERSE_MERCATOR_FACTOR(xmpp));
				coord.lat_lon.lat = this->center_coord.lat_lon.lat + (delta_y_pixels / REVERSE_MERCATOR_FACTOR(ympp));
			}

			/* Original code, used for comparison of results with new, reformatted formula. */
			{
				Coord test_coord;
				test_coord.set_coord_mode(CoordMode::LatLon);

				test_coord.lat_lon.lon = this->center_coord.lat_lon.lon + (180.0 * xmpp / 65536 / 256 * delta_x_pixels);
				test_coord.lat_lon.lat = this->center_coord.lat_lon.lat + (180.0 * ympp / 65536 / 256 * delta_y_pixels);

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
						       /* TODO_LATER: make sure that this is ok, that we don't need to use central_get_width()/get_height() here. */
						       this->central_get_x_center_pixel(),
						       this->central_get_y_center_pixel());
			break;

		case GisViewportDrawMode::Mercator:
			/* This isn't called with a high frequently so less need to optimize. */
			/* Modified (reformatted) formula. */
			{
				coord.lat_lon.lon = this->center_coord.lat_lon.lon + (delta_x_pixels / REVERSE_MERCATOR_FACTOR(xmpp));
				coord.lat_lon.lat = DEMERCLAT (MERCLAT(this->center_coord.lat_lon.lat) + (delta_y_pixels / (REVERSE_MERCATOR_FACTOR(ympp))));
			}

			/* Original code, used for comparison of results with new, reformatted formula. */
			{
				Coord test_coord;
				test_coord.set_coord_mode(CoordMode::LatLon);

				test_coord.lat_lon.lon = this->center_coord.lat_lon.lon + (180.0 * xmpp / 65536 / 256 * delta_x_pixels);
				test_coord.lat_lon.lat = DEMERCLAT (MERCLAT(this->center_coord.lat_lon.lat) + (180.0 * ympp / 65536 / 256 * delta_y_pixels));

				if (coord.lat_lon.lat != test_coord.lat_lon.lat) {
					qDebug() << SG_PREFIX_E << "Mercator: Latitude calculation mismatch" << coord << test_coord << (coord.lat_lon.lat - test_coord.lat_lon.lat);
				} else {
					//qDebug() << SG_PREFIX_I << "Mercator: OK Latitude" << coord << test_coord << (coord.lat_lon.lat - test_coord.lat_lon.lat);
				}
				if (coord.lat_lon.lon != test_coord.lat_lon.lon) {
					qDebug() << SG_PREFIX_E << "Mercator: Longitude calculation mismatch" << coord << test_coord << (coord.lat_lon.lon - test_coord.lat_lon.lon);
				} else {
					//qDebug() << SG_PREFIX_I << "Mercator: OK Longitude" << coord << test_coord << (coord.lat_lon.lon - test_coord.lat_lon.lon);
				}
			}
			break;
		default:
			qDebug() << SG_PREFIX_E << "Unrecognized draw mode" << this->draw_mode;
		}
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unrecognized coord mode" << this->coord_mode;
		break;

	}

	return coord; /* Named Return Value Optimization. */
}




/**
   @reviewed-on tbd
*/
Coord GisViewport::screen_pos_to_coord(const ScreenPos & pos) const
{
	return this->screen_pos_to_coord(pos.x(), pos.y());
}




/**
   @reviewed-on tbd

   Since this function is used for every drawn trackpoint - it can get called a lot.
   Thus x & y position factors are calculated once on zoom changes,
   avoiding the need to do it here all the time.
*/
sg_ret GisViewport::coord_to_screen_pos(const Coord & coord_in, fpixel * pos_x, fpixel * pos_y) const
{
	Coord coord = coord_in;
	const double xmpp = this->viking_scale.x;
	const double ympp = this->viking_scale.y;

	const fpixel x_center_pixel = this->central_get_x_center_pixel();
	const fpixel y_center_pixel = this->central_get_y_center_pixel();

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

	//qDebug() << SG_PREFIX_I << this->coord_mode << this->draw_mode;

	switch (this->coord_mode) {
	case CoordMode::UTM:
		{
			const int zone_diff = this->center_coord.utm.get_zone() - coord.utm.get_zone();

			if (0 != zone_diff && this->is_one_utm_zone){
				return sg_ret::err;
			}

			const double horiz_distance_m = coord.utm.get_easting() - this->center_coord.utm.get_easting();
			const double vert_distance_m = coord.utm.get_northing() - this->center_coord.utm.get_northing();

			*pos_x = x_center_pixel + (horiz_distance_m / xmpp) - (zone_diff * this->utm_zone_width) / xmpp;
			*pos_y = y_center_pixel - (vert_distance_m / ympp); /* TODO_LATER: plus or minus? */
		}
		break;

	case CoordMode::LatLon:
		switch (this->draw_mode) {
		case GisViewportDrawMode::LatLon:
			*pos_x = x_center_pixel + (MERCATOR_FACTOR(xmpp) * (coord.lat_lon.lon - this->center_coord.lat_lon.lon));
			*pos_y = y_center_pixel + (MERCATOR_FACTOR(ympp) * (this->center_coord.lat_lon.lat - coord.lat_lon.lat));
			break;
		case GisViewportDrawMode::Expedia:
			{
				fpixel xx;
				fpixel yy;
				Expedia::lat_lon_to_screen_pos(&xx, &yy, this->center_coord.lat_lon, coord.lat_lon, xmpp * ALTI_TO_MPP, ympp * ALTI_TO_MPP, x_center_pixel, y_center_pixel);
				*pos_x = xx;
				*pos_y = yy;
			}
			break;
		case GisViewportDrawMode::Mercator:
			*pos_x = x_center_pixel + (MERCATOR_FACTOR(xmpp) * (coord.lat_lon.lon - this->center_coord.lat_lon.lon));
			*pos_y = y_center_pixel + (MERCATOR_FACTOR(ympp) * (MERCLAT(this->center_coord.lat_lon.lat) - MERCLAT(coord.lat_lon.lat)));
			break;
		default:
			qDebug() << SG_PREFIX_E << "Unexpected viewport drawing mode" << this->draw_mode;
			return sg_ret::err;
		}
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected viewport coord mode" << this->coord_mode;
		return sg_ret::err;
	}

	return sg_ret::ok;
}




/**
   @reviewed-on tbd
*/
sg_ret GisViewport::coord_to_screen_pos(const Coord & coord_in, ScreenPos & result) const
{
	fpixel x;
	fpixel y;
	if (sg_ret::ok == this->coord_to_screen_pos(coord_in, &x, &y)) {
		result.rx() = x;
		result.ry() = y;
		return sg_ret::ok;
	} else {
		return sg_ret::err;
	}

}




/**
   @reviewed-on tbd
*/
void GisViewport::draw_bbox(const LatLonBBox & bbox, const QPen & pen)
{
	if (!bbox.intersects_with(this->get_bbox())) {
		qDebug() << SG_PREFIX_I << "Not drawing bbox" << bbox << ", does not intersects with viewport bbox" << this->get_bbox();
		return;
	}


	ScreenPos sp_sw;
	this->coord_to_screen_pos(Coord(LatLon(bbox.south, bbox.west), this->coord_mode), sp_sw);
	ScreenPos sp_ne;
	this->coord_to_screen_pos(Coord(LatLon(bbox.north, bbox.east), this->coord_mode), sp_ne);

	if (sp_sw.x() < 0) {
		sp_sw.rx() = 0;
	}

	if (sp_ne.y() < 0) {
		sp_ne.ry() = 0;
	}

	this->draw_rectangle(pen, sp_sw.x(), sp_ne.y(), sp_ne.x() - sp_sw.x(), sp_sw.y() - sp_ne.y());

	return;
}



/**
   @reviewed-on 2019-07-20
*/
CoordMode GisViewport::get_coord_mode(void) const
{
	return this->coord_mode;
}




/**
   @reviewed-on tbd
*/
void GisViewport::set_coord_mode(CoordMode new_mode)
{
	this->coord_mode = new_mode;
	this->center_coord.recalculate_to_mode(new_mode);
}




/**
   @reviewed-on tbd
*/
bool GisViewport::get_is_one_utm_zone(void) const
{
	return coord_mode == CoordMode::UTM && this->is_one_utm_zone;
}




/**
   @reviewed-on tbd
*/
void GisViewport::set_draw_mode(GisViewportDrawMode new_drawmode)
{
	this->draw_mode = new_drawmode;

	if (new_drawmode == GisViewportDrawMode::UTM) {
		this->set_coord_mode(CoordMode::UTM);
	} else {
		this->set_coord_mode(CoordMode::LatLon);
	}
}




/**
   @reviewed-on 2019-07-20
*/
GisViewportDrawMode GisViewport::get_draw_mode(void) const
{
	return this->draw_mode;
}




/**
   @reviewed-on tbd
*/
void GisViewport::set_trigger(Layer * trg)
{
	this->trigger = trg;
}




/**
   @reviewed-on tbd
*/
Layer * GisViewport::get_trigger(void) const
{
	return this->trigger;
}




/**
   @reviewed-on tbd
*/
void GisViewport::set_half_drawn(bool new_half_drawn)
{
	this->half_drawn = new_half_drawn;
}




/**
   @reviewed-on tbd
*/
bool GisViewport::get_half_drawn(void) const
{
	return this->half_drawn;
}




/**
   @reviewed-on tbd
*/
LatLonBBox GisViewport::get_bbox(int margin_left, int margin_right, int margin_top, int margin_bottom) const
{
	/* Remember that positive values of margins should shrink the
	   bbox, and that get() methods used here return values in
	   Qt's coordinate system where beginning of the coordinate
	   system is in upper-left corner.  */
	Coord coord_ul = this->screen_pos_to_coord(this->central_get_leftmost_pixel() + margin_left,   this->central_get_topmost_pixel() + margin_top);
	Coord coord_ur = this->screen_pos_to_coord(this->central_get_rightmost_pixel() - margin_right, this->central_get_topmost_pixel() + margin_top);
	Coord coord_bl = this->screen_pos_to_coord(this->central_get_leftmost_pixel() + margin_left,   this->central_get_bottommost_pixel() - margin_bottom);
	Coord coord_br = this->screen_pos_to_coord(this->central_get_rightmost_pixel() - margin_right, this->central_get_bottommost_pixel() - margin_bottom);

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
   @brief Add an attribution/copyright to display on viewport

   @reviewed-on tbd

   @copyright: new attribution/copyright to display.
*/
sg_ret GisViewport::add_attribution(QString const & attribution)
{
	return this->decorations.add_attribution(attribution);
}




/**
   @reviewed-on tbd
*/
sg_ret GisViewport::add_logo(const GisViewportLogo & logo)
{
	return this->decorations.add_logo(logo);
}




/**
   @reviewed-on tbd
*/
void GisViewport::mousePressEvent(QMouseEvent * ev)
{
	qDebug() << SG_PREFIX_I << "Mouse CLICK event, button" << (int) ev->button();

	this->window->get_toolbox()->handle_mouse_click(ev);

	ev->accept();
}




/**
   @reviewed-on tbd
*/
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




/**
   @reviewed-on tbd
*/
void GisViewport::mouseMoveEvent(QMouseEvent * ev)
{
	this->draw_mouse_motion_cb(ev);

	this->window->get_toolbox()->handle_mouse_move(ev);

	emit this->cursor_moved(this, ev);

	ev->accept();
}




/**
   @reviewed-on tbd
*/
void GisViewport::mouseReleaseEvent(QMouseEvent * ev)
{
	qDebug() << SG_PREFIX_I << "called with button" << (int) ev->button();

	this->window->get_toolbox()->handle_mouse_release(ev);
	emit this->button_released(this, ev);

	ev->accept();
}




/**
   @reviewed-on 2019-07-20
*/
void GisViewport::wheelEvent(QWheelEvent * ev)
{
	/* By how much will we move x or y coordinate of pixel that is
	   in the center of central part of viewport? */
	const fpixel delta_x = 0.333 * this->central_get_width();
	const fpixel delta_y = 0.333 * this->central_get_height();

	const Qt::KeyboardModifiers modifiers = ev->modifiers();
	const QPoint angle = ev->angleDelta();

	/* Does mouse wheel move up or down? */
	const bool mouse_wheel_up = angle.y() > 0;

	qDebug() << SG_PREFIX_I
		 << "Wheel event" << (mouse_wheel_up ? "up" : "down")
		 << ", buttons =" << (int) ev->buttons()
		 << ", angle =" << angle.y();

	switch (modifiers) {
	case Qt::ControlModifier: /* Pan up & down. */
		/* New 'x' pixel coordinate of a center does not
		   change and is still 'x_center'. */

		if (mouse_wheel_up) { /* Move viewport's content up. */
			/* Get pixel that was above old center by
			   delta_y, and move it to center. */
			this->set_center_coord(this->central_get_x_center_pixel(),
					       this->central_get_y_center_pixel() - delta_y);
		} else { /* Move viewport's content down. */
			/* Get pixel that was below old center by
			   delta_y, and move it to center. */
			this->set_center_coord(this->central_get_x_center_pixel(),
					       this->central_get_y_center_pixel() + delta_y);
		}
		ev->accept();
		break;

	case Qt::ShiftModifier: /* Pan left & right. */
		/* New 'y' pixel coordinate of a center does not
		   change and is still 'y_center'. */

		if (mouse_wheel_up) { /* Move viewport's content right. */
			/* Get pixel that was to the left of old
			   center by delta_x, and move it to
			   center. */
			this->set_center_coord(this->central_get_x_center_pixel() - delta_x,
					       this->central_get_y_center_pixel());
		} else { /* Move viewport's content left. */
			/* Get pixel that was to the right of old
			   center by delta_x, and move it to
			   center. */
			this->set_center_coord(this->central_get_x_center_pixel() + delta_x,
					       this->central_get_y_center_pixel());
		}
		ev->accept();
		break;

	case (Qt::ControlModifier | Qt::ShiftModifier):
		/* Zoom in/out, preserve screen position of geo point
		   that is in the center position of center part of
		   viewport. */
		if (mouse_wheel_up) {
			this->zoom_in_on_center_pixel();
		} else {
			this->zoom_out_on_center_pixel();
		}
		ev->accept();
		break;

	case Qt::NoModifier: {
		/* Geo coordinate under cursor will be put after zoom
		   at the same position in viewport as before zoom.
		   Before zoom the geo coordinate was under cursor,
		   and after zoom it will be still under cursor. */

		const ScreenPos center_pos = this->central_get_center_screen_pos();
		const ScreenPos event_pos(ev->x(), ev->y());
		const ZoomOperation zoom_operation = SlavGPS::wheel_event_to_zoom_operation(ev);

		GisViewportZoom::keep_coordinate_under_cursor(zoom_operation, this, this->window, event_pos, center_pos);
		}
		ev->accept();
		break;
	default:
		/* Other modifier. Just ignore. */
		ev->ignore();
		return;
	};

	qDebug() << SG_PREFIX_SIGNAL << "Will emit center_coord_or_zoom_changed()";
	emit this->center_coord_or_zoom_changed(this);

	return;
}




/**
   @reviewed-on tbd
*/
void GisViewport::draw_mouse_motion_cb(QMouseEvent * ev)
{
	QPoint position = this->mapFromGlobal(QCursor::pos());

#if 0   /* Verbose debug. */
	qDebug() << SG_PREFIX_I << "Difference in cursor position: dx = " << position.x() - ev->x() << ", dy = " << position.y() - ev->y();
#endif

#ifdef K_FIXME_RESTORE
	this->window->tb->move(ev);
#endif


	const int pos_x = position.x();
	const int pos_y = position.y();



	/* Get coordinates in viewport's coordinates mode. */
	const Coord coord = this->screen_pos_to_coord(pos_x, pos_y);
	this->window->get_statusbar()->set_coord(coord);



	/* Change interpolate method according to scale. */
	const double zoom = this->get_viking_scale().get_x();
	DemInterpolation interpol_method;
	if (zoom > 2.0) {
		interpol_method = DemInterpolation::None;
	} else if (zoom >= 1.0) {
		interpol_method = DemInterpolation::Simple;
	} else {
		interpol_method = DemInterpolation::Best;
	}
	Altitude altitude = DEMCache::get_elev_by_coord(coord, interpol_method);
	if (altitude.is_valid()) {
		altitude.convert_to_unit(Preferences::get_unit_height());
	}
	this->window->get_statusbar()->set_altitude_uu(altitude);



#ifdef K_FIXME_RESTORE
	this->window->pan_move(ev);
#endif
}




/**
   @reviewed-on tbd
*/
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
		qDebug() << SG_PREFIX_E << "orientation: unknown";
		break;
	}


	/* Since aspect ratio of target device may be different than
	   aspect ratio of viewport, we need to call function that
	   will calculate size of scaled viewport that will guarantee
	   filling target device while keeping aspect ratio of
	   original viewport (so that the image printed to target
	   device is not distorted). */
	const int target_device_width = page_rect.width();
	const int target_device_height = page_rect.height();
	int scaled_width = 0;
	int scaled_height = 0;
	double dummy;
	this->calculate_scaled_sizes(target_device_width, target_device_height, scaled_width, scaled_height, dummy);
	GisViewport * scaled_viewport = this->copy(scaled_width, scaled_height, this->window);

	/* Since we are printing viewport as it is, we allow existing
	   highlights to be drawn to print pixmap. */
	ThisApp::get_layers_panel()->draw_tree_items(scaled_viewport, true, false);


	QPainter printer_painter;
	printer_painter.begin(printer);
	ScreenPos paint_begin; /* Beginning of rectangle, into which we will paint in target device. */
	//paint_begin.setX((target_width / 2.0) - (scaled_viewport->width / 2.0));
	//paint_begin.setY((target_height / 2.0) - (scaled_viewport->height / 2.0));
	paint_begin.setX(0);
	paint_begin.setY(0);

	printer_painter.drawPixmap(paint_begin, scaled_viewport->vpixmap);
	printer_painter.end();

	delete scaled_viewport;

	qDebug() << SG_PREFIX_I << "page rectangle:" << page_rect;
	qDebug() << SG_PREFIX_I << "paint_begin:" << paint_begin;


	return true;
}




/**
   @reviewed-on 2019-07-20
*/
void ScreenPos::set(fpixel new_x, fpixel new_y)
{
	this->rx() = new_x;
	this->ry() = new_y;
}




/**
   @reviewed-on tbd
*/
ScreenPos ScreenPos::get_average(const ScreenPos & pos1, const ScreenPos & pos2)
{
	return ScreenPos((pos1.x() + pos2.x()) / 2.0, (pos1.y() + pos2.y()) / 2.0);
}




/**
   @reviewed-on tbd
*/
bool ScreenPos::are_closer_than(const ScreenPos & pos1, const ScreenPos & pos2, fpixel limit)
{
	return (std::fabs(pos1.x() - pos2.x()) < limit) && (std::fabs(pos1.y() - pos2.y()) < limit);
}




/**
   @reviewed-on tbd
*/
bool ScreenPos::operator==(const ScreenPos & pos) const
{
	return (this->x() == pos.x()) && (this->y() == pos.y());
}




/**
   @reviewed-on 2019-07-20
*/
QString GisViewportDrawModes::get_label_with_accelerator(GisViewportDrawMode mode)
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
		return QObject::tr("<unknown>");
	}
}



/**
   @reviewed-on 2019-07-20
*/
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
		mode_id_string = "<unknown>";
		break;
	}

	return mode_id_string;
}




/**
   @reviewed-on tbd
*/
bool GisViewportDrawModes::set_draw_mode_from_file(GisViewport * gisview, const char * line)
{
	bool success = true;

	if (0 == strcasecmp(line, "utm")) {
		gisview->set_draw_mode(GisViewportDrawMode::UTM);

	} else if (0 == strcasecmp(line, "expedia")) {
		gisview->set_draw_mode(GisViewportDrawMode::Expedia);

	} else if (0 == strcasecmp(line, "google")) {
		success = false;
		qDebug() << SG_PREFIX_W << QObject::tr("Read file: draw mode 'google' no longer supported");

	} else if (0 == strcasecmp(line, "kh")) {
		success = false;
		qDebug() << SG_PREFIX_W << QObject::tr("Read file: draw mode 'kh' no more supported");

	} else if (0 == strcasecmp(line, "mercator")) {
		gisview->set_draw_mode(GisViewportDrawMode::Mercator);

	} else if (0 == strcasecmp(line, "latlon")) {
		gisview->set_draw_mode(GisViewportDrawMode::LatLon);
	} else {
		qDebug() << SG_PREFIX_E << QObject::tr("Read file: unexpected draw mode") << line;
		success = false;
	}

	return success;
}




/**
   @reviewed-on 2019-07-20
*/
QDebug SlavGPS::operator<<(QDebug debug, const GisViewportDrawMode mode)
{
	switch (mode) {
	case GisViewportDrawMode::Invalid:
		debug << "GisViewportDrawMode::Invalid";
		break;
	case GisViewportDrawMode::UTM:
		debug << "GisViewportDrawMode::UTM";
		break;
	case GisViewportDrawMode::Expedia:
		debug << "GisViewportDrawMode::Expedia";
		break;
	case GisViewportDrawMode::Mercator:
		debug << "GisViewportDrawMode::Mercator";
		break;
	case GisViewportDrawMode::LatLon:
		debug << "GisViewportDrawMode::LatLon";
		break;
	default:
		debug << "GisViewportDrawMode::Unknown (" << (int) mode << ")";
		break;
	}

	return debug;
}




/**
   @reviewed-on tbd
*/
sg_ret GisViewport::set_bbox(const LatLonBBox & new_bbox)
{
	return GisViewportZoom::zoom_to_show_bbox(this, this->get_coord_mode(), new_bbox);
}




/**
   @reviewed-on tbd
*/
void GisViewport::request_redraw(const QString & trigger_descr)
{
	qDebug() << SG_PREFIX_SIGNAL << "Will emit 'center or zoom changed' signal triggered by" << trigger_descr;
	emit this->center_coord_or_zoom_changed(this);
}




/**
   @reviewed-on tbd

   Tell QT what type of MIME data we accept.
*/
void GisViewport::dragEnterEvent(QDragEnterEvent * event)
{
	if (event->mimeData()->hasFormat("text/plain")) {
		event->acceptProposedAction();
	}

}




/**
   @reviewed-on tbd
*/
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




/**
   @reviewed-on tbd
*/
Window * GisViewport::get_window(void) const
{
	return this->window;
}




/**
   @reviewed-on tbd
*/
sg_ret GisViewport::get_cursor_pos_cbl(QMouseEvent * ev, ScreenPos & screen_pos) const
{
	const int leftmost   = this->central_get_leftmost_pixel();
	const int rightmost  = this->central_get_rightmost_pixel();
	const int topmost    = this->central_get_topmost_pixel();
	const int bottommost = this->central_get_bottommost_pixel();

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

	/* Cursor outside of chart area. */
	if (x > rightmost) {
		return sg_ret::err;
	}
	if (y > bottommost) {
		return sg_ret::err;
	}
	if (x < leftmost) {
		return sg_ret::err;
	}
	if (y < topmost) {
		return sg_ret::err;
	}

	/* Converting from Qt's "beginning is in upper-left" into "beginning is in bottom-left" coordinate system. */
	screen_pos.rx() = x;
	screen_pos.ry() = bottommost - y;

	return sg_ret::ok;
}




/**
   @reviewed-on tbd
*/
double GisViewport::central_get_height_m(void) const
{
	return this->central_get_height() * this->viking_scale.y;
}




/**
   @reviewed-on tbd
*/
double GisViewport::central_get_width_m(void) const
{
	return this->central_get_width() * this->viking_scale.x;
}




/**
   @reviewed-on 2019-07-20
*/
QDebug SlavGPS::operator<<(QDebug debug, const ScreenPos & screen_pos)
{
	debug << "ScreenPos:" << QString("(%1,%2)").arg(screen_pos.x()).arg(screen_pos.y());
	return debug;
}





/**
   @reviewed-on tbd
*/
ArrowSymbol::ArrowSymbol(double blades_width_degrees, int size_factor_)
{
	/* How widely the arrow blades are spread. */
	this->cosine_factor = cos(DEG2RAD(blades_width_degrees)) * size_factor_;
	this->sine_factor = sin(DEG2RAD(blades_width_degrees)) * size_factor_;
}




/**
   @reviewed-on tbd
*/
void ArrowSymbol::set_arrow_tip(int x, int y, int direction_)
{
	this->tip_x = x;
	this->tip_y = y;
	this->direction = direction_;
}




/**
   @reviewed-on tbd
*/
sg_ret ArrowSymbol::paint(QPainter & painter, double dx, double dy)
{
	painter.drawLine(this->tip_x,
			 this->tip_y,
			 this->tip_x + this->direction * (dx * this->cosine_factor + dy * this->sine_factor),
			 this->tip_y + this->direction * (dy * this->cosine_factor - dx * this->sine_factor));

	painter.drawLine(this->tip_x,
			 this->tip_y,
			 this->tip_x + this->direction * (dx * this->cosine_factor - dy * this->sine_factor),
			 this->tip_y + this->direction * (dy * this->cosine_factor + dx * this->sine_factor));

	return sg_ret::ok;
}



/**
   @reviewed-on tbd
*/
CenterCoords::CenterCoords()
{
	this->current_iter = this->begin();
}
