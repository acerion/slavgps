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




LatLon LatLonBBox::get_center_coordinate(void) const
{
	return LatLon((this->north + this->south) / 2, (this->east + this->west) / 2);
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
	if (this->north    != NAN
	    && this->east  != NAN
	    && this->south != NAN
	    && this->west  != NAN) {

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
	bbox_strings.max_lat = c_locale.toString(this->north);
	bbox_strings.min_lat = c_locale.toString(this->south);
	bbox_strings.max_lon = c_locale.toString(this->east);
	bbox_strings.min_lon = c_locale.toString(this->west);

	return bbox_strings;
}




QDebug SlavGPS::operator<<(QDebug debug, const LatLonBBox & bbox)
{
	debug.nospace() << "North: " << bbox.north << ", South: " << bbox.south << ", East: " << bbox.east << ", West: " << bbox.west;
	return debug;
}
