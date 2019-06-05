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
#include "globals.h"




using namespace SlavGPS;




#define SG_MODULE "Coord"




void Coord::change_mode(CoordMode new_mode)
{
	if (this->mode != new_mode) {
		if (new_mode == CoordMode::LatLon) {
			this->ll = UTM::to_latlon(this->utm);
		} else {
			this->utm = LatLon::to_utm(this->ll);
		}
		this->mode = new_mode;
	}
}




static double distance_safe(const Coord & coord1, const Coord & coord2)
{
	const LatLon a = coord1.get_latlon();
	const LatLon b = coord2.get_latlon();
	return LatLon::get_distance(a, b);
}




double Coord::distance(const Coord & coord1, const Coord & coord2)
{
	if (coord1.mode == coord2.mode) {
		return distance_safe(coord1, coord2);
	}

	switch (coord1.mode) {
	case CoordMode::UTM:
		return UTM::get_distance(coord1.utm, coord2.utm);

	case CoordMode::LatLon:
		return LatLon::get_distance(coord1.ll, coord2.ll);

	default:
		qDebug() << SG_PREFIX_E << "Unexpected CoordMode" << (int) coord1.mode;
		return 0.0;
	}
}




Distance Coord::distance_2(const Coord & coord1, const Coord & coord2)
{
	/* Re-implementing Coord::distance() to have better control over Distance::valid. */
	const SupplementaryDistanceUnit distance_unit = SupplementaryDistanceUnit::Meters; /* Using meters - the most basic and common unit. */
	Distance result;

	if (coord1.mode == coord2.mode) {
		result = Distance(distance_safe(coord1, coord2), distance_unit);
		return result;
	}

	switch (coord1.mode) {
	case CoordMode::UTM:
		result = Distance(UTM::get_distance(coord1.utm, coord2.utm), distance_unit);
		return result;

	case CoordMode::LatLon:
		result = Distance(LatLon::get_distance(coord1.ll, coord2.ll), distance_unit);
		return result;

	default:
		qDebug() << SG_PREFIX_E << "Unexpected CoordMode" << (int) coord1.mode;
		return result;
	}
}




Coord::Coord(const LatLon & new_lat_lon, CoordMode new_mode)
{
	switch (new_mode) {
	case CoordMode::UTM:
		this->utm = LatLon::to_utm(new_lat_lon);
		break;
	case CoordMode::LatLon:
		this->ll = new_lat_lon;
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected CoordMode" << (int) new_mode;
		break;
	}

	this->mode = new_mode;
}




Coord::Coord(const UTM & new_utm, CoordMode new_mode)
{
	switch (new_mode) {
	case CoordMode::UTM:
		this->utm = new_utm;
		break;
	case CoordMode::LatLon:
		this->ll = UTM::to_latlon(new_utm);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected CoordMode" << (int) new_mode;
	}

	this->mode = new_mode;
}




Coord::Coord(const Coord& coord)
{
	this->ll = coord.ll;
	this->utm = coord.utm;
	this->mode = coord.mode;
}




LatLon Coord::get_latlon(void) const
{
	LatLon ret;

	switch (this->mode) {
	case CoordMode::LatLon:
		ret = this->ll;
		break;
	case CoordMode::UTM:
		ret = UTM::to_latlon(this->utm);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected CoordMode" << (int) this->mode;
		break;
	}

	return ret; /* Let's hope that Named Return Value Optimisation works. */
}




UTM Coord::get_utm(void) const
{
	UTM ret;

	switch (this->mode) {
	case CoordMode::UTM:
		ret = this->utm;
		break;
	case CoordMode::LatLon:
		ret = LatLon::to_utm(this->ll);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected CoordMode" << (int) this->mode;
		break;
	}

	return ret; /* Let's hope that Named Return Value Optimisation works. */
}




bool Coord::operator==(const Coord & coord) const
{
	if (this->mode != coord.mode) {
		return false;
	}

	switch (this->mode) {
	case CoordMode::UTM:
		return UTM::is_equal(this->utm, coord.utm);

	case CoordMode::LatLon:
		return this->ll.lat == coord.ll.lat && this->ll.lon == coord.ll.lon;

	default:
		qDebug() << SG_PREFIX_E << "Unexpected CoordMode" << (int) this->mode;
		return false;
	}
}




bool Coord::operator!=(const Coord & coord) const
{
	return !(*this == coord);
}




Coord & Coord::operator=(const Coord & other)
{
	if (other.mode == CoordMode::LatLon) {
		this->ll = other.ll;
	} else {
		this->utm = other.utm;
	}
	this->mode = other.mode;

        return *this;
}




QDebug SlavGPS::operator<<(QDebug debug, const Coord & coord)
{
	switch (coord.mode) {
	case CoordMode::UTM:
		debug << "Coordinate UTM:" << coord.utm;
		break;
	case CoordMode::LatLon:
		debug << "Coordinate LatLon:" << coord.ll;
		break;
	default:
		debug << "\n" << SG_PREFIX_E << "Unexpected coordinate mode" << (int) coord.mode;
		break;
	}

	return debug;
}




static LatLon get_north_west_corner(const LatLon & center, const LatLon & distance_from_center)
{
	LatLon ret;

	ret.lat = center.lat + distance_from_center.lat;
	ret.lon = center.lon - distance_from_center.lon;
	if (ret.lon < SG_LONGITUDE_MIN) {
		ret.lon += 360.0;
	}

	if (ret.lat > SG_LATITUDE_MAX) {  /* Over north pole. */
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
	if (ret.lon > SG_LONGITUDE_MAX) {
		ret.lon -= 360.0;
	}

	if (ret.lat < SG_LATITUDE_MIN) {  /* Over south pole. */
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
	coord_tl->mode = CoordMode::LatLon;

	coord_br->ll = get_south_east_corner(center, distance_from_center);
	coord_br->mode = CoordMode::LatLon;
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
	switch (this->mode) {
	case CoordMode::UTM:
		/* First string will contain "zone + N/S", second
		   string will contain easting and northing of a UTM
		   format:
		   ZONE[N|S] EASTING NORTHING */

		str1 = QString("%1%2").arg((int) utm.get_zone()).arg(utm.get_band_letter());
		str2 = QString("%1 %2").arg((int) utm.get_easting()).arg((int) utm.get_northing());
		break;
	case CoordMode::LatLon:
		LatLon::to_strings(this->ll, str1, str2);
		break;

	default:
		qDebug() << SG_PREFIX_E << "Unrecognized coord mode" << (int) this->mode;
		break;
	}

	return;
}




QString Coord::to_string(void) const
{
	QString result;

	switch (this->mode) {
	case CoordMode::UTM:
		result = utm.to_string();
		break;

	case CoordMode::LatLon:
		result = this->ll.to_string();
		break;

	default:
		qDebug() << SG_PREFIX_E << "Unrecognized coord mode" << (int) this->mode;
		break;
	}

	return result;
}
