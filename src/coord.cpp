/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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




#include "coords.h"
#include "coord.h"




using namespace SlavGPS;




void VikCoord::change_mode(CoordMode new_mode)
{
	if (this->mode != new_mode) {
		if (new_mode == CoordMode::LATLON) {
			a_coords_utm_to_latlon(&this->ll, &this->utm);
		} else {
			a_coords_latlon_to_utm(&this->utm, &this->ll);
		}
		this->mode = new_mode;
	}
}




VikCoord VikCoord::copy_change_mode(CoordMode new_mode) const
{
	VikCoord dest;

	if (this->mode == new_mode) {
		dest = *this; /* kamilFIXME: verify this assignment of objects. */
	} else {
		if (new_mode == CoordMode::LATLON) {
			a_coords_utm_to_latlon(&dest.ll, &this->utm);
		} else {
			a_coords_latlon_to_utm(&dest.utm, &this->ll);
		}
		dest.mode = new_mode;
	}

	return dest;
}




static double distance_safe(const VikCoord & coord1, const VikCoord & coord2)
{
	struct LatLon a = coord1.get_latlon();
	struct LatLon b = coord2.get_latlon();
	return a_coords_latlon_diff(&a, &b);
}




double VikCoord::distance(const VikCoord & coord1, const VikCoord & coord2)
{
	if (coord1.mode == coord2.mode) {
		return distance_safe(coord1, coord2);
	}

	if (coord1.mode == CoordMode::UTM) {
		return a_coords_utm_diff((const struct UTM *) &coord1, (const struct UTM *) &coord2);
	} else {
		return a_coords_latlon_diff((const struct LatLon *) &coord1, (const struct LatLon *) &coord2);
	}
}




VikCoord::VikCoord(const struct LatLon & ll_, CoordMode mode_)
{
	if (mode_ == CoordMode::LATLON) {
		this->ll = ll_;
	} else {
		a_coords_latlon_to_utm(&this->utm, &ll_);
	}
	this->mode = mode_;
}




VikCoord::VikCoord(const struct UTM & utm_, CoordMode mode_)
{
	if (mode_ == CoordMode::UTM) {
		this->utm = utm_;
	} else {
		a_coords_utm_to_latlon(&this->ll, &utm);
	}
	this->mode = mode_;
}




struct LatLon VikCoord::get_latlon(void) const
{
	struct LatLon dest = { 0, 0 };
	if (this->mode == CoordMode::LATLON) {
		dest = this->ll;
	} else {
		a_coords_utm_to_latlon(&dest, &this->utm);
	}
	return dest; /* Let's hope that Named Return Value Optimisation works. */
}




struct UTM Coord::get_utm(void) const
{
	struct UTM dest;
	if (this->mode == CoordMode::UTM) {
		dest = this->utm;
	} else {
		a_coords_latlon_to_utm(&dest, &this->ll);
	}
	return dest; /* Let's hope that Named Return Value Optimisation works. */
}




bool VikCoord::operator==(const VikCoord & coord) const
{
	if (this->mode != coord.mode) {
		return false;
	}

	if (this->mode == CoordMode::LATLON) {
		return this->ll.lat == coord.ll.lat && this->ll.lon == coord.ll.lon;
	} else { /* CoordMode::UTM */
		return this->utm.zone == coord.utm.zone && this->utm.northing == coord.utm.northing && this->utm.easting == coord.utm.easting;
	}
}




bool VikCoord::operator!=(const VikCoord & coord) const
{
	return !(*this == coord);
}




static void get_north_west(struct LatLon * center, struct LatLon * dist, struct LatLon * nw)
{
	nw->lat = center->lat + dist->lat;
	nw->lon = center->lon - dist->lon;
	if (nw->lon < -180) {
		nw->lon += 360.0;
	}

	if (nw->lat > 90.0) {  /* Over north pole. */
		nw->lat = 180 - nw->lat;
		nw->lon = nw->lon - 180;
	}
}




static void get_south_east(struct LatLon * center, struct LatLon * dist, struct LatLon * se)
{
	se->lat = center->lat - dist->lat;
	se->lon = center->lon + dist->lon;
	if (se->lon > 180.0) {
		se->lon -= 360.0;
	}

	if (se->lat < -90.0) {  /* Over south pole. */
		se->lat += 180;
		se->lon = se->lon - 180;
	}
}




void VikCoord::set_area(const struct LatLon * wh, VikCoord * coord_tl, VikCoord * coord_br) const
{
	struct LatLon ll_nw, ll_se;
	struct LatLon dist;

	dist.lat = wh->lat / 2;
	dist.lon = wh->lon / 2;

	struct LatLon center = this->get_latlon();
	get_north_west(&center, &dist, &ll_nw);
	get_south_east(&center, &dist, &ll_se);

	coord_tl->ll = ll_nw;
	coord_tl->mode = CoordMode::LATLON;

	coord_br->ll = ll_se;
	coord_br->mode = CoordMode::LATLON;
}




bool VikCoord::is_inside(const VikCoord * tl, const VikCoord * br) const
{
	struct LatLon ll_ = this->get_latlon();
	struct LatLon tl_ll = tl->get_latlon();
	struct LatLon br_ll = br->get_latlon();

	if ((ll_.lat > tl_ll.lat) || (ll_.lon < tl_ll.lon)) {
		return false;
	}

	if ((ll_.lat < br_ll.lat) || (ll_.lon > br_ll.lon)) {
		return false;
	}
	return true;
}
