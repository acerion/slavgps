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




#ifndef _SG_BOUNDING_BOX_H_
#define _SG_BOUNDING_BOX_H_




#include <QString>




namespace SlavGPS {




	class LatLonBBoxStrings {
	public:
		QString min_lon;
		QString max_lon;
		QString min_lat;
		QString max_lat;
	};




	class LatLonBBox  {
	public:
		/* TODO: what should be the initial value? 0.0 or NaN? */
		double north = 0.0; /* max_lat */
		double south = 0.0; /* min_lat */
		double east = 0.0;  /* max_lon */
		double west = 0.0;  /* min_lon */

		static LatLonBBoxStrings to_strings(const LatLonBBox & bbox);
	};




} /* namespace SlavGPS */




/**
 * +--------+
 * |a       |
 * |     +--+----+
 * |     |  |    |
 * +-----+--+    |
 *       |      b|
 *       +-------+
 */
#define BBOX_INTERSECT(a,b) ((a).south < (b).north && (a).north > (b).south && (a).east > (b).west && (a).west < (b).east)




#endif /* #ifndef _SG_BOUNDING_BOX_H_ */
