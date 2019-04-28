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




#include <cmath>




#include <QString>
#include <QDebug>




#include "globals.h"
#include "coords.h"
#include "lat_lon.h"




namespace SlavGPS {




	class LatLonBBoxStrings {
	public:
		QString north;
		QString south;
		QString east;
		QString west;
	};




	class LatLonBBox  {
	public:
		LatLonBBox() {};
		LatLonBBox(const LatLon & corner1, const LatLon & corner2);

		LatLonBBoxStrings values_to_c_strings(void) const;

		/* Set all fields of bbox (coordinates and 'valid'
		   field) to initial, invalid values. */
		void invalidate(void);

		/* See if coordinate fields of bbox are all valid. Set
		   'valid' field appropriately and return value of
		   this field.

		   If one of coordinate fields is invalid, set all
		   coordinate fields to invalid. */
		bool validate(void);

		/* Is this bbox valid? */
		bool is_valid(void) const;


		/**
		   +--------------+
		   |this          |
		   |              |
		   |    . point   |
		   |              |
		   |              |
		   +--------------+
		*/
		bool contains_point(const LatLon & point) const;


		/**
		   +--------------+
		   |this          |
		   |              |
		   | +-------+    |
		   | |  bbox |    |
		   | +-------+    |
		   +--------------+
		*/
		bool contains_bbox(const LatLonBBox & bbox) const;

		/* Make the given LatLonBBox larger by expanding it to include given LatLon. */
		sg_ret expand_with_lat_lon(const LatLon & lat_lon);

		/* Make a bbox larger by expanding it to include other bbox. */
		sg_ret expand_with_bbox(const LatLonBBox & other);


		/* Get coordinate of a point that is a simple
		   arithmetic average of north/south, east/west
		   values. */
		LatLon get_center_lat_lon(void) const;


		Latitude north; /* Maximal latitude (towards +90 north). */
		Latitude south; /* Minimal latitude (towards -90 south). */
		Longitude east; /* Maximal longitude (towards +180 east). */
		Longitude west; /* Minimal longitude (towards -180 west). */

	private:
		bool valid = false;
	};


	QDebug operator<<(QDebug debug, const LatLonBBox & bbox);




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
#define BBOX_INTERSECT(a,b) ((a).south.get_value() < (b).north.get_value() && (a).north.get_value() > (b).south.get_value() && (a).east.get_value() > (b).west.get_value() && (a).west.get_value() < (b).east.get_value())




#endif /* #ifndef _SG_BOUNDING_BOX_H_ */
