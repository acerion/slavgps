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
		this->north.set_value(corner1.lat);
		this->south.set_value(corner2.lat);
	} else {
		this->north.set_value(corner2.lat);
		this->south.set_value(corner1.lat);
	}

	if (corner1.lon > corner2.lon) {
		this->east.set_value(corner1.lon);
		this->west.set_value(corner2.lon);
	} else {
		this->east.set_value(corner2.lon);
		this->west.set_value(corner1.lon);
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
	debug.nospace() << "North: " << bbox.north.get_value() << ", South: " << bbox.south.get_value() << ", East: " << bbox.east.get_value() << ", West: " << bbox.west.get_value();
	return debug;
}




LatLon LatLonBBox::get_center_lat_lon(void) const
{
	LatLon result;
	if (this->valid) {
		result = LatLon((this->north.get_value() + this->south.get_value()) / 2, (this->east.get_value() + this->west.get_value()) / 2);
	} else {
		; /* Return invalid latlon. */
	}
	return result;
}




bool LatLonBBox::contains_point(const LatLon & point) const
{
	/* TODO_HARD: handle situation where the bbox is at the border of +/- 180 degrees longitude. */

	if (point.lat <= this->north.get_value()
	    && point.lat >= this->south.get_value()
	    && point.lon <= this->east.get_value()
	    && point.lon >= this->west.get_value()) {

		return true;
	} else {
		return false;
	}
}




bool LatLonBBox::contains_bbox(const LatLonBBox & bbox) const
{
	/* TODO_HARD: handle situation where the bbox is at the border of +/- 180 degrees longitude. */

	/* Convert into definite 'smallest' and 'largest' positions. */
	const double minimal_latitude = std::min(bbox.north.get_value(), bbox.south.get_value());
	const double maximal_longitude = std::max(bbox.east.get_value(), bbox.west.get_value());

	if (this->south.get_value() <= minimal_latitude
	    && this->north.get_value() >= minimal_latitude
	    && this->west.get_value() <= maximal_longitude
	    && this->east.get_value() >= maximal_longitude) {

		return true;
	} else {
		return false;
	}
}




sg_ret LatLonBBox::expand_with_lat_lon(const LatLon & lat_lon)
{
	if (!lat_lon.is_valid()) {
		qDebug() << SG_PREFIX_E << "Trying to expand with invalid LatLon";
		return sg_ret::err;
	}

	if (!this->north.is_valid() || (lat_lon.lat > this->north.get_value())) {
		this->north.set_value(lat_lon.lat);
	}
	if (!this->south.is_valid() || (lat_lon.lat < this->south.get_value())) {
		this->south.set_value(lat_lon.lat);
	}
	if (!this->east.is_valid() || (lat_lon.lon > this->east.get_value())) {
		this->east.set_value(lat_lon.lon);
	}
	if (!this->west.is_valid() || (lat_lon.lon < this->west.get_value())) {
		this->west.set_value(lat_lon.lon);
	}

	return sg_ret::ok;
}




sg_ret LatLonBBox::expand_with_bbox(const LatLonBBox & other)
{
	if (!other.is_valid()) {
		qDebug() << SG_PREFIX_E << "Trying to expand with invalid BBox";
		return sg_ret::err;
	}

	if (!this->north.is_valid() || other.north.get_value() > this->north.get_value()) {
		this->north = other.north;
	}
	if (!this->south.is_valid() || other.south.get_value() < this->south.get_value()) {
		this->south = other.south;
	}
	if (!this->east.is_valid() || other.east.get_value() > this->east.get_value()) {
		this->east = other.east;
	}
	if (!this->west.is_valid() || other.west.get_value() < this->west.get_value()) {
		this->west = other.west;
	}

	return sg_ret::ok;
}
