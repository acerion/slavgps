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




#include <QDebug>
#include <QLocale>

#include "coords.h"
#include "coord.h"




using namespace SlavGPS;




#define PREFIX " Coord: "




void Coord::change_mode(CoordMode new_mode)
{
	if (this->mode != new_mode) {
		if (new_mode == CoordMode::LATLON) {
			a_coords_utm_to_latlon(&this->ll, &this->utm);
		} else {
			a_coords_latlon_to_utm(&this->utm, this->ll);
		}
		this->mode = new_mode;
	}
}




Coord Coord::copy_change_mode(CoordMode new_mode) const
{
	Coord dest;

	if (this->mode == new_mode) {
		dest = *this; /* kamilFIXME: verify this assignment of objects. */
	} else {
		if (new_mode == CoordMode::LATLON) {
			a_coords_utm_to_latlon(&dest.ll, &this->utm);
		} else {
			a_coords_latlon_to_utm(&dest.utm, this->ll);
		}
		dest.mode = new_mode;
	}

	return dest;
}




static double distance_safe(const Coord & coord1, const Coord & coord2)
{
	const LatLon a = coord1.get_latlon();
	const LatLon b = coord2.get_latlon();
	return a_coords_latlon_diff(a, b);
}




double Coord::distance(const Coord & coord1, const Coord & coord2)
{
	if (coord1.mode == coord2.mode) {
		return distance_safe(coord1, coord2);
	}

	if (coord1.mode == CoordMode::UTM) {
		return a_coords_utm_diff(&coord1.utm, &coord2.utm);
	} else {
		return a_coords_latlon_diff(coord1.ll, coord2.ll);
	}
}




Coord::Coord(const LatLon & new_lat_lon, CoordMode new_mode)
{
	switch (new_mode) {
	case CoordMode::UTM:
		a_coords_latlon_to_utm(&this->utm, new_lat_lon);
		break;
	case CoordMode::LATLON:
		this->ll = new_lat_lon;
		break;
	default:
		qDebug() << "EE:" PREFIX << __FUNCTION__ << "unexpected CoordMode" << (int) mode;
		break;
	}

	this->mode = new_mode;
}




Coord::Coord(const struct UTM & utm_, CoordMode mode_)
{
	if (mode_ == CoordMode::UTM) {
		this->utm = utm_;
	} else {
		a_coords_utm_to_latlon(&this->ll, &utm);
	}
	this->mode = mode_;
}




LatLon Coord::get_latlon(void) const
{
	LatLon ret;

	switch (this->mode) {
	case CoordMode::LATLON:
		ret = this->ll;
		break;
	case CoordMode::UTM:
		a_coords_utm_to_latlon(&ret, &this->utm);
		break;
	default:
		qDebug() << "EE:" PREFIX << __FUNCTION__ << "unexpected CoordMode" << (int) this->mode;
		break;
	}

	return ret; /* Let's hope that Named Return Value Optimisation works. */
}




struct UTM Coord::get_utm(void) const
{
	struct UTM dest;
	if (this->mode == CoordMode::UTM) {
		dest = this->utm;
	} else {
		a_coords_latlon_to_utm(&dest, this->ll);
	}
	return dest; /* Let's hope that Named Return Value Optimisation works. */
}




bool Coord::operator==(const Coord & coord) const
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




bool Coord::operator!=(const Coord & coord) const
{
	return !(*this == coord);
}




static LatLon get_north_west_corner(const LatLon & center, const LatLon & distance_from_center)
{
	LatLon ret;

	ret.lat = center.lat + distance_from_center.lat;
	ret.lon = center.lon - distance_from_center.lon;
	if (ret.lon < -180) {
		ret.lon += 360.0;
	}

	if (ret.lat > 90.0) {  /* Over north pole. */
		ret.lat = 180 - ret.lat;
		ret.lon = ret.lon - 180;
	}

	return ret;
}




static LatLon get_south_east_corner(const LatLon & center, const LatLon & distance_from_center)
{
	LatLon ret;

	ret.lat = center.lat - distance_from_center.lat;
	ret.lon = center.lon + distance_from_center.lon;
	if (ret.lon > 180.0) {
		ret.lon -= 360.0;
	}

	if (ret.lat < -90.0) {  /* Over south pole. */
		ret.lat += 180;
		ret.lon = ret.lon - 180;
	}

	return ret;
}




void Coord::get_area_coordinates(const LatLon * area_span, Coord * coord_tl, Coord * coord_br) const
{
	const LatLon center = this->get_latlon();
	const LatLon distance_from_center(area_span->lat / 2, area_span->lon / 2);

	coord_tl->ll = get_north_west_corner(center, distance_from_center);
	coord_tl->mode = CoordMode::LATLON;

	coord_br->ll = get_south_east_corner(center, distance_from_center);
	coord_br->mode = CoordMode::LATLON;
}




bool Coord::is_inside(const Coord * tl, const Coord * br) const
{
	const LatLon this_lat_lon = this->get_latlon();
	const LatLon tl_ll = tl->get_latlon();
	const LatLon br_ll = br->get_latlon();

	if ((this_lat_lon.lat > tl_ll.lat) || (this_lat_lon.lon < tl_ll.lon)) {
		return false;
	}

	if ((this_lat_lon.lat < br_ll.lat) || (this_lat_lon.lon > br_ll.lon)) {
		return false;
	}
	return true;
}




void Coord::to_strings(QString & str1, QString & str2) const
{
	if (this->mode == CoordMode::UTM) {
		/* First string will contain "zone + N/S", second
		   string will contain easting and northing of a UTM
		   format:
		   ZONE[N|S] EASTING NORTHING */

		str1 = QString("%1%2").arg((int) utm.zone).arg(utm.letter); /* Zone is stored in a char but is an actual number. */
		str2 = QString("%1 %2").arg((int) utm.easting).arg((int) utm.northing);

	} else if (this->mode == CoordMode::LATLON) {
		LatLon::to_strings(this->ll, str1, str2);

	} else {
		qDebug() << "EE: Coord: to strings: unrecognized coord mode" << (int) this->mode;

	}

	return;
}







/**
   \brief Convert a double to a string in C locale

   Following GPX specifications, decimal values are xsd:decimal
   So, they must use the period separator, not the localized one.

   This function re-implements glib-based a_coords_dtostr() function
   from coords.cpp.
*/
void CoordUtils::to_string(QString & result, double d)
{
	static QLocale c_locale = QLocale::c();

	/* TODO: adjust parameters of toString() to do the conversion without loss of precision. */
	result = c_locale.toString(d);

	return;
}




/*
  \brief Convert values from LatLon struct to a pair of strings in C locale

  Strings will have a non-localized, regular dot as a separator
  between integer part and fractional part.
*/
void CoordUtils::to_strings(QString & lat, QString & lon, const LatLon & lat_lon)
{
	static QLocale c_locale = QLocale::c();

	/* TODO: adjust parameters of toString() to do the conversion without loss of precision. */
	lat = c_locale.toString(lat_lon.lat);
	lon = c_locale.toString(lat_lon.lon);

	return;
}




/*
  \brief Convert values from LatLon struct to strings stored in LatLongBBoxStrings struct, in C locale

  Strings will have a non-localized, regular dot as a separator
  between integer part and fractional part.
*/
void CoordUtils::to_strings(LatLonBBoxStrings & bbox_strings, const LatLonBBox & bbox)
{
	static QLocale c_locale = QLocale::c();

	bbox_strings.min_lon = c_locale.toString(bbox.west);
	bbox_strings.max_lon = c_locale.toString(bbox.east);
	bbox_strings.min_lat = c_locale.toString(bbox.south);
	bbox_strings.max_lat = c_locale.toString(bbox.north);

	return;
}
