/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2011, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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




#include "bbox.h"
#include "coord.h"
#include "lat_lon.h"




using namespace SlavGPS;




#define SG_MODULE "BBox"




LatLonBBox::LatLonBBox(const LatLon & corner1, const LatLon & corner2)
{
	/* TODO_HARD: what happens if corner1/corner2 crosses the boundary of +/- longitude? */

	if (corner1.lat > corner2.lat) {
		this->north = corner1.lat;
		this->south = corner2.lat;
	} else {
		this->north = corner2.lat;
		this->south = corner1.lat;
	}

	if (corner1.lon > corner2.lon) {
		this->east = corner1.lon;
		this->west = corner2.lon;
	} else {
		this->east = corner2.lon;
		this->west = corner1.lon;
	}

	this->validate();
}




bool LatLonBBox::is_valid(void) const
{
	return this->valid;
}




void LatLonBBox::invalidate(void)
{
	this->north.invalidate();
	this->east.invalidate();
	this->south.invalidate();
	this->west.invalidate();

	this->valid = false;
}




bool LatLonBBox::validate(void)
{
	if (this->north.is_valid()
	    && this->south.is_valid()
	    && this->east.is_valid()
	    && this->west.is_valid()) {

		this->valid = true;
	} else {
		this->invalidate();
	}

	return this->valid;
}




/**
   \brief Convert values from LatLonBBox struct to strings stored in LatLongBBoxStrings struct, in C locale

   Strings will have a non-localized, regular dot as a separator
   between integer part and fractional part.
*/
LatLonBBoxStrings LatLonBBox::values_to_c_strings(void) const
{
	LatLonBBoxStrings bbox_strings;
	bbox_strings.north = this->north.value_to_string_for_file();
	bbox_strings.south = this->south.value_to_string_for_file();
	bbox_strings.east  = this->east.value_to_string_for_file();
	bbox_strings.west  = this->west.value_to_string_for_file();

	return bbox_strings;
}




QDebug SlavGPS::operator<<(QDebug debug, const LatLonBBox & bbox)
{
	debug.nospace() << "North: " << bbox.north << ", South: " << bbox.south << ", East: " << bbox.east << ", West: " << bbox.west;
	return debug;
}




LatLon LatLonBBox::get_center_lat_lon(void) const
{
	LatLon result;
	if (this->valid) {
		result = LatLon((this->north.value() + this->south.value()) / 2, (this->east.unbound_value() + this->west.unbound_value()) / 2); /* TODO_LATER: replace the calculation with some LatLon method. */
	} else {
		; /* Return invalid latlon. */
	}
	return result;
}




bool LatLonBBox::contains_point(const LatLon & point) const
{
	/* TODO_HARD: handle situation where the bbox is at the border of +/- 180 degrees longitude. */

	if (point.lat <= this->north
	    && point.lat >= this->south
	    && point.lon <= this->east
	    && point.lon >= this->west) {

		return true;
	} else {
		return false;
	}
}




bool LatLonBBox::contains_bbox(const LatLonBBox & bbox) const
{
	/* TODO_HARD: handle situation where the bbox is at the border of +/- 180 degrees longitude. */

	/* Convert into definite 'smallest' and 'largest' positions. */
	const double minimal_latitude = std::min(bbox.north.value(), bbox.south.value());
	const double maximal_longitude = std::max(bbox.east.unbound_value(), bbox.west.unbound_value());

	if (this->south.value() <= minimal_latitude
	    && this->north.value() >= minimal_latitude
	    && this->west.unbound_value() <= maximal_longitude
	    && this->east.unbound_value() >= maximal_longitude) {

		return true;
	} else {
		return false;
	}
}




bool LatLonBBox::intersects_with(const LatLonBBox & bbox) const
{
	return this->south < bbox.north
		&& this->north > bbox.south
		&& this->east > bbox.west
		&& this->west < bbox.east;
}




sg_ret LatLonBBox::expand_with_lat_lon(const LatLon & lat_lon)
{
	if (!lat_lon.is_valid()) {
		qDebug() << SG_PREFIX_E << "Trying to expand with invalid LatLon";
		return sg_ret::err;
	}

	if (!this->north.is_valid() || (lat_lon.lat > this->north)) {
		this->north = lat_lon.lat;
	}
	if (!this->south.is_valid() || (lat_lon.lat < this->south)) {
		this->south = lat_lon.lat;
	}
	if (!this->east.is_valid() || (lat_lon.lon > this->east)) {
		this->east = lat_lon.lon;
	}
	if (!this->west.is_valid() || (lat_lon.lon < this->west)) {
		this->west = lat_lon.lon;
	}

	return sg_ret::ok;
}




sg_ret LatLonBBox::expand_with_bbox(const LatLonBBox & other)
{
	if (!other.is_valid()) {
		qDebug() << SG_PREFIX_E << "Trying to expand with invalid BBox";
		return sg_ret::err;
	}

	if (!this->north.is_valid() || other.north > this->north) {
		this->north = other.north;
	}
	if (!this->south.is_valid() || other.south < this->south) {
		this->south = other.south;
	}
	if (!this->east.is_valid() || other.east > this->east) {
		this->east = other.east;
	}
	if (!this->west.is_valid() || other.west < this->west) {
		this->west = other.west;
	}

	return sg_ret::ok;
}
