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
		const double center_to_bottom_m = this->central_get_height_m() / 2;
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




GisViewport::GisViewport(int left, int right, int top, int bottom, QWidget * parent) : ViewportPixmap(left, right, top, bottom, parent)
{
	this->window = ThisApp::get_main_window();


	this->installEventFilter(this);

	this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	this->setMinimumSize(200, 300);
	snprintf(this->debug, sizeof (this->debug), "%s", "center");
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


	if (!ApplicationState::get_integer(VIK_SETTINGS_VIEW_HISTORY_SIZE, &this->center_coords.max_items)) {
		this->center_coords.max_items = 20;
	}
	if (ApplicationState::get_integer(VIK_SETTINGS_VIEW_HISTORY_DIFF_DIST, &this->center_coords.radius)) {
		this->center_coords.radius = 500;
	}


	this->set_center_coord(initial_lat_lon); /* The function will reject latlon if it's invalid. */


	this->setFocusPolicy(Qt::ClickFocus);
	//this->qpainter = new QPainter(this);



	this->scale_visibility = true;

	this->height_unit = Preferences::get_unit_height();
	this->distance_unit = Preferences::get_unit_distance();
	this->speed_unit = Preferences::get_unit_speed();
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
}




/**
   \brief Clear the whole viewport
*/
void GisViewport::clear(void)
{
	qDebug() << SG_PREFIX_I << "Clear whole viewport" << this->debug;
	ViewportPixmap::clear();

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
	if (new_value.is_valid()) {
		this->viking_scale = new_value;
		return sg_ret::ok;
	} else {
		qDebug() << SG_PREFIX_E << "New value is invalid";
		return sg_ret::err;
	}
}




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
	emit this->list_of_center_coords_changed();
}




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
	this->center_coords.current_iter++;
	this->set_center_coord(*this->center_coords.current_iter, false);

	return true;
}




/**
   @return true when a previous position in the history is available
   @return false otherwise
*/
bool GisViewport::back_available(void) const
{
	return (this->center_coords.size() > 1 && this->center_coords.current_iter != this->center_coords.begin());
}




/**
   @return true when a next position in the history is available
   @return false otherwise
*/
bool GisViewport::forward_available(void) const
{
	return (this->center_coords.size() > 1 && this->center_coords.current_iter != prev(this->center_coords.end()));
}




/**
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

	const Coord coord = this->screen_pos_to_coord(this->central_get_leftmost_pixel(),
						      this->central_get_upmost_pixel()); /* Second argument shouldn't really matter if we are getting "leftmost" zone. */
	return coord.utm.get_zone();
}




int GisViewport::get_rightmost_zone(void) const
{
	if (coord_mode != CoordMode::UTM) {
		return 0;
	}

	const Coord coord = this->screen_pos_to_coord(this->central_get_rightmost_pixel(),
						      this->central_get_upmost_pixel()); /* Second argument shouldn't really matter if we are getting "rightmost" zone. */
	return coord.utm.get_zone();
}




Coord GisViewport::screen_pos_to_coord(ScreenPosition screen_pos) const
{
	Coord result;
	int pos_x, pos_y;

	switch (screen_pos) {
	case ScreenPosition::UpperLeft:
		pos_x = this->central_get_leftmost_pixel();
		pos_y = this->central_get_upmost_pixel();;
		break;

	case ScreenPosition::UpperRight:
		pos_x = this->central_get_rightmost_pixel();
		pos_y = this->central_get_upmost_pixel();
		break;

	case ScreenPosition::BottomLeft:
		pos_x = this->central_get_leftmost_pixel();
		pos_y = this->central_get_bottommost_pixel();
		break;

	case ScreenPosition::BottomRight:
		pos_x = this->central_get_rightmost_pixel();
		pos_y = this->central_get_bottommost_pixel();
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

	/* Distance of pixel specified by pos_x/pos_y from viewport's
	   central pixel.  TODO: verify location of pos_x and pos_y in
	   these equations. */
	const int delta_horiz_pixels = pos_x - this->central_get_x_center_pixel();
	const int delta_vert_pixels = this->central_get_y_center_pixel() - pos_y;

	switch (this->coord_mode) {
	case CoordMode::UTM:
		coord.set_coord_mode(CoordMode::UTM);

		/* Modified (reformatted) formula. */
		{
			coord.utm.set_northing((delta_vert_pixels * ympp) + this->center_coord.utm.get_northing());
			coord.utm.set_easting((delta_horiz_pixels * xmpp) + this->center_coord.utm.get_easting());
			coord.utm.set_zone(this->center_coord.utm.get_zone());

			const int zone_delta = floor((coord.utm.easting - UTM_CENTRAL_MERIDIAN_EASTING) / this->utm_zone_width + 0.5);
			coord.utm.shift_zone_by(zone_delta);
			coord.utm.shift_easting_by(-(zone_delta * this->utm_zone_width));

			/* Calculate correct band letter.  TODO: there
			   has to be an easier way to do this. */
			{
				/* We only need this initial
				   assignment to avoid assertion fail
				   in ::to_lat_lon(). */
				assert (UTM::is_band_letter(this->center_coord.utm.get_band_letter())); /* TODO_2_LATER: add smarter error handling. In theory the source object should be valid and for sure contain valid band letter. */
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
		}
		break;

	case CoordMode::LatLon:
		coord.set_coord_mode(CoordMode::LatLon);

		switch (this->draw_mode) {
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
						       /* TODO: make sure that this is ok, that we don't need to use central_get_width()/get_height() here. */
						       this->central_get_x_center_pixel(),
						       this->central_get_y_center_pixel());
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
			qDebug() << SG_PREFIX_E << "Unrecognized draw mode" << this->draw_mode;
		}
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unrecognized coord mode" << this->coord_mode;
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

	const int horiz_center_pixel = this->central_get_x_center_pixel();
	const int vert_center_pixel = this->central_get_y_center_pixel();

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

	qDebug() << SG_PREFIX_I << this->coord_mode << this->draw_mode;

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
		switch (this->draw_mode) {
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




ScreenPos GisViewport::coord_to_screen_pos(const Coord & coord_in) const
{
	ScreenPos pos;
	if (sg_ret::ok == this->coord_to_screen_pos(coord_in, &pos.x, &pos.y)) {
		pos.valid = true;
	} /* else: invalid by default. */

	return pos;
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

	this->draw_rectangle(pen, sp_sw.x, sp_ne.y, sp_ne.x - sp_sw.x, sp_sw.y - sp_ne.y);

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




void GisViewport::set_draw_mode(GisViewportDrawMode new_drawmode)
{
	this->draw_mode = new_drawmode;

	if (new_drawmode == GisViewportDrawMode::UTM) {
		this->set_coord_mode(CoordMode::UTM);
	} else {
		this->set_coord_mode(CoordMode::LatLon);
	}
}




GisViewportDrawMode GisViewport::get_draw_mode(void) const
{
	return this->draw_mode;
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
	/* Remember that positive values of margins should shrink the
	   bbox, and that get() methods used here return values in
	   Qt's coordinate system where beginning of the coordinate
	   system is in upper-left corner.  */
	Coord coord_ul = this->screen_pos_to_coord(this->central_get_leftmost_pixel() + margin_left,   this->central_get_upmost_pixel() + margin_top);
	Coord coord_ur = this->screen_pos_to_coord(this->central_get_rightmost_pixel() - margin_right, this->central_get_upmost_pixel() + margin_top);
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

	/* TODO: using central_get_width() and central_get_height() will give us only 99%-correct results. */
	const int w = this->central_get_width();
	const int h = this->central_get_height();
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
		const ScreenPos center_pos(this->central_get_x_center_pixel(), this->central_get_y_center_pixel());
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

	GisViewport * scaled_viewport = new GisViewport(0, 0, 0, 0, a_window); /* TODO: get proper values of margins in constructor call. */

	const int orig_width = this->central_get_width();
	const int orig_height = this->central_get_height();

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
	scaled_viewport->set_draw_mode(this->get_draw_mode());
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



	snprintf(scaled_viewport->debug, sizeof (scaled_viewport->debug), "%s", "Scaled Viewport Pixmap");

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
	//paint_begin.setX((target_width / 2.0) - (scaled_viewport->width / 2.0));
	//paint_begin.setY((target_height / 2.0) - (scaled_viewport->height / 2.0));
	paint_begin.setX(0);
	paint_begin.setY(0);

	printer_painter.drawPixmap(paint_begin, *scaled_viewport->pixmap);
	printer_painter.end();

	delete scaled_viewport;

	qDebug() << SG_PREFIX_I << "target rectangle:" << target_rect;
	qDebug() << SG_PREFIX_I << "paint_begin:" << paint_begin;


	return true;
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




sg_ret GisViewport::set_bbox(const LatLonBBox & new_bbox)
{
	return GisViewportZoom::zoom_to_show_bbox(this, this->get_coord_mode(), new_bbox);
}




void GisViewport::request_redraw(const QString & trigger_descr)
{
	qDebug() << SG_PREFIX_SIGNAL << "Will emit 'center or zoom changed' signal triggered by" << trigger_descr;
	emit this->center_coord_or_zoom_changed();
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




Window * GisViewport::get_window(void) const
{
	return this->window;
}




sg_ret GisViewport::get_cursor_pos_cbl(QMouseEvent * ev, ScreenPos & screen_pos) const
{
	const int leftmost   = this->central_get_leftmost_pixel();
	const int rightmost  = this->central_get_rightmost_pixel();
	const int upmost     = this->central_get_upmost_pixel();
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
	if (y < upmost) {
		return sg_ret::err;
	}

	/* Converting from Qt's "beginning is in upper-left" into "beginning is in bottom-left" coordinate system. */
	screen_pos.x = x;
	screen_pos.y = bottommost - y;

	return sg_ret::ok;
}




double GisViewport::central_get_height_m(void) const
{
	return this->central_get_height() * this->viking_scale.y;
}




double GisViewport::central_get_width_m(void) const
{
	return this->central_get_width() * this->viking_scale.x;
}




QDebug SlavGPS::operator<<(QDebug debug, const ScreenPos & screen_pos)
{
	debug << "ScreenPos:" << QString("(%1,%2)").arg(screen_pos.x).arg(screen_pos.y);
	return debug;
}





ArrowSymbol::ArrowSymbol(double blades_width_degrees, int size_factor_)
{
	/* How widely the arrow blades are spread. */
	this->cosine_factor = cos(DEG2RAD(blades_width_degrees)) * size_factor_;
	this->sine_factor = sin(DEG2RAD(blades_width_degrees)) * size_factor_;
}




void ArrowSymbol::set_arrow_tip(int x, int y, int direction_)
{
	this->tip_x = x;
	this->tip_y = y;
	this->direction = direction_;
}




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



CenterCoords::CenterCoords()
{
	this->current_iter = this->begin();
}
