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




using namespace SlavGPS;




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




LatLon LatLonBBox::get_center_coordinate(void) const
{
	return LatLon((this->north + this->south) / 2, (this->east + this->west) / 2);
}




bool LatLonBBox::is_valid(void) const
{
	return this->valid;
}




void LatLonBBox::invalidate(void)
{
	this->north = NAN;
	this->east  = NAN;
	this->south = NAN;
	this->west  = NAN;

	this->valid = false;
}




bool LatLonBBox::validate(void)
{
	if (!std::isnan(this->north) && !std::isnan(this->east) && !std::isnan(this->south) && !std::isnan(this->west)
	    && this->north >= -90.0 && this->north <= +90.0
	    && this->south >= -90.0 && this->south <= +90.0
	    && this->east >= -180.0 && this->east <= +180.0
	    && this->west >= -180.0 && this->west <= +180.0) {

		this->valid = true;
	} else {
		this->north = NAN;
		this->east  = NAN;
		this->south = NAN;
		this->west  = NAN;

		this->valid = false;
	}

	return this->valid;
}




/**
   \brief Convert values from LatLonBBox struct to strings stored in LatLongBBoxStrings struct, in C locale

   Strings will have a non-localized, regular dot as a separator
   between integer part and fractional part.
*/
LatLonBBoxStrings LatLonBBox::to_strings(void) const
{
	static QLocale c_locale = QLocale::c();

	LatLonBBoxStrings bbox_strings;
	bbox_strings.north = c_locale.toString(this->north);
	bbox_strings.south = c_locale.toString(this->south);
	bbox_strings.east  = c_locale.toString(this->east);
	bbox_strings.west  = c_locale.toString(this->west);

	return bbox_strings;
}




QDebug SlavGPS::operator<<(QDebug debug, const LatLonBBox & bbox)
{
	debug.nospace() << "North: " << bbox.north << ", South: " << bbox.south << ", East: " << bbox.east << ", West: " << bbox.west;
	return debug;
}




LatLon LatLonBBox::get_center(void) const
{
	LatLon result;
	if (this->valid) {
		result = LatLon((this->north + this->south) / 2, (this->west + this->east) / 2);
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
	double lowest_latitude = 0.0;
	if (bbox.north < bbox.south) {
		lowest_latitude = bbox.north;
	} else {
		lowest_latitude = bbox.south;
	}

	double maximal_longitude = 0.0;
	if (bbox.east > bbox.west) {
	        maximal_longitude = bbox.east;
	} else {
		maximal_longitude = bbox.west;
	}


	if (this->south <= lowest_latitude
	    && this->north >= lowest_latitude
	    && this->west <= maximal_longitude
	    && this->east >= maximal_longitude) {

		return true;
	} else {
		return false;
	}
}
