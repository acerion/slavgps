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




sg_ret Coord::recalculate_to_mode(CoordMode new_mode)
{
	if (this->mode != new_mode) {
		switch (new_mode) {
		case CoordMode::LatLon:
			this->lat_lon = UTM::to_lat_lon(this->utm);
			break;
		case CoordMode::UTM:
			this->utm = LatLon::to_utm(this->lat_lon);
			break;
		default:
			qDebug() << SG_PREFIX_E << "Invalid new mode" << (int) new_mode;
			return sg_ret::err;
		}

		this->mode = new_mode;
	}

	return sg_ret::ok;
}




static double distance_safe(const Coord & coord1, const Coord & coord2)
{
	const LatLon a = coord1.get_lat_lon();
	const LatLon b = coord2.get_lat_lon();
	return LatLon::get_distance(a, b);
}




double Coord::distance(const Coord & coord1, const Coord & coord2)
{
	if (coord1.mode != CoordMode::LatLon && coord1.mode != CoordMode::UTM) {
		qDebug() << SG_PREFIX_E << "Unexpected CoordMode in first argument:" << coord1.mode;
		return 0.0;
	}
	if (coord2.mode != CoordMode::LatLon && coord2.mode != CoordMode::UTM) {
		qDebug() << SG_PREFIX_E << "Unexpected CoordMode in second argument:" << coord2.mode;
		return 0.0;
	}

	switch (coord1.mode) {
	case CoordMode::UTM:
		return UTM::get_distance(coord1.utm, coord2.utm);
	case CoordMode::LatLon:
		return LatLon::get_distance(coord1.lat_lon, coord2.lat_lon);
	default:
		qDebug() << SG_PREFIX_E << "Unexpected CoordMode:" << coord1.mode;
		return 0.0;
	}
}




Distance Coord::distance_2(const Coord & coord1, const Coord & coord2)
{
	/* Re-implementing Coord::distance() to have better control over Distance::valid. */

	const DistanceType::Unit distance_unit = DistanceType::Unit::E::Meters; /* Using meters - the most basic and common unit. */
	Distance result;

	if (coord1.mode != CoordMode::LatLon && coord1.mode != CoordMode::UTM) {
		qDebug() << SG_PREFIX_E << "Unexpected CoordMode in first argument:" << coord1.mode;
		return result;
	}
	if (coord2.mode != CoordMode::LatLon && coord2.mode != CoordMode::UTM) {
		qDebug() << SG_PREFIX_E << "Unexpected CoordMode in second argument:" << coord2.mode;
		return result;
	}
	if (coord1.mode != coord2.mode) {
		qDebug() << SG_PREFIX_E << "CoordMode mismatch:" << coord1.mode << coord2.mode;
		return result;
	}


	switch (coord1.mode) {
	case CoordMode::UTM:
		result = Distance(UTM::get_distance(coord1.utm, coord2.utm), distance_unit);
		return result;
	case CoordMode::LatLon:
		result = Distance(LatLon::get_distance(coord1.lat_lon, coord2.lat_lon), distance_unit);
		return result;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected CoordMode:" << coord1.mode;
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
		this->lat_lon = new_lat_lon;
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
		this->lat_lon = UTM::to_lat_lon(new_utm);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected CoordMode" << (int) new_mode;
	}

	this->mode = new_mode;
}




Coord::Coord(const Coord& coord)
{
	this->lat_lon = coord.lat_lon;
	this->utm = coord.utm;
	this->mode = coord.mode;
}




LatLon Coord::get_lat_lon(void) const
{
	LatLon ret;

	switch (this->mode) {
	case CoordMode::LatLon:
		ret = this->lat_lon;
		break;
	case CoordMode::UTM:
		ret = UTM::to_lat_lon(this->utm);
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
		ret = LatLon::to_utm(this->lat_lon);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected CoordMode" << (int) this->mode;
		break;
	}

	return ret; /* Let's hope that Named Return Value Optimisation works. */
}




CoordMode Coord::get_coord_mode(void) const
{
	return this->mode;
}




void Coord::set_coord_mode(CoordMode new_mode)
{
	this->mode = new_mode;
	return;
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
		return this->lat_lon.lat == coord.lat_lon.lat && this->lat_lon.lon == coord.lat_lon.lon;

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
		this->lat_lon = other.lat_lon;
	} else {
		this->utm = other.utm;
	}
	this->mode = other.mode;

        return *this;
}




QDebug SlavGPS::operator<<(QDebug debug, const Coord & coord)
{
	switch (coord.get_coord_mode()) {
	case CoordMode::UTM:
		debug << "Coordinate UTM:" << coord.utm;
		break;
	case CoordMode::LatLon:
		debug << "Coordinate LatLon:" << coord.lat_lon;
		break;
	default:
		debug << "\n" << SG_PREFIX_E << "Unexpected coordinate mode" << coord.get_coord_mode();
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




void Coord::get_coord_rectangle(const LatLon & single_rectangle_span, CoordRectangle & rect) const
{
	const LatLon center = this->get_lat_lon();
	const LatLon distance_from_center(single_rectangle_span.lat / 2, single_rectangle_span.lon / 2);

	rect.m_coord_tl.lat_lon = get_north_west_corner(center, distance_from_center);
	rect.m_coord_tl.mode = CoordMode::LatLon;

	rect.m_coord_br.lat_lon = get_south_east_corner(center, distance_from_center);
	rect.m_coord_br.mode = CoordMode::LatLon;

	rect.m_coord_center = *this;
}




bool Coord::is_inside(const Coord & tl, const Coord & br) const
{
	const LatLon this_lat_lon = this->get_lat_lon();
	const LatLon tl_lat_lon = tl.get_lat_lon();
	const LatLon br_lat_lon = br.get_lat_lon();

	if ((this_lat_lon.lat > tl_lat_lon.lat) || (this_lat_lon.lon < tl_lat_lon.lon)) {
		return false;
	}

	if ((this_lat_lon.lat < br_lat_lon.lat) || (this_lat_lon.lon > br_lat_lon.lon)) {
		return false;
	}
	return true;
}




QString Coord::to_string(void) const
{
	QString result;

	switch (this->mode) {
	case CoordMode::UTM:
		result = utm.to_string();
		break;

	case CoordMode::LatLon:
		result = this->lat_lon.to_string();
		break;

	default:
		qDebug() << SG_PREFIX_E << "Unrecognized coord mode" << this->mode;
		break;
	}

	return result;
}




QDebug SlavGPS::operator<<(QDebug debug, const CoordMode mode)
{
	switch (mode) {
	case CoordMode::Invalid:
		debug << "CoordMode::Invalid";
		break;
	case CoordMode::UTM:
		debug << "CoordMode::UTM";
		break;
	case CoordMode::LatLon:
		debug << "CoordMode::LatLon";
		break;
	default:
		debug << "CoordMode::Unknown (" << (int) mode << ")";
		break;
	}

	return debug;
}
