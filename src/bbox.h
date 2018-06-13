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




#include "coords.h"




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

		LatLonBBoxStrings to_strings(void) const;
		LatLon get_center_coordinate(void) const;

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

		/* Get coordinate of a point that is a simple
		   arithmetic average of north/south, east/west
		   values. */
		LatLon get_center(void) const;


		double north = NAN; /* Maximal latitude (towards +90 north). */
		double south = NAN; /* Minimal latitude (towards -90 south). */
		double east  = NAN; /* Maximal longitude (towards +180 east). */
		double west  = NAN; /* Minimal longitude (towards -180 west). */

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
#define BBOX_INTERSECT(a,b) ((a).south < (b).north && (a).north > (b).south && (a).east > (b).west && (a).west < (b).east)




/* Make the given LatLonBBox larger by expanding it to include given LatLon. */
#define BBOX_EXPAND_WITH_LATLON(m_bbox, m_lat_lon) {			\
		if ((m_lat_lon.lat > m_bbox.north) || std::isnan(m_bbox.north)) { \
			m_bbox.north = m_lat_lon.lat;			\
		}							\
		if ((m_lat_lon.lat < m_bbox.south) || std::isnan(m_bbox.south)) { \
			m_bbox.south = m_lat_lon.lat;			\
		}							\
		if ((m_lat_lon.lon > m_bbox.east) || std::isnan(m_bbox.east)) { \
			m_bbox.east = m_lat_lon.lon;			\
		}							\
		if ((m_lat_lon.lon < m_bbox.west) || std::isnan(m_bbox.west)) {	\
			m_bbox.west = m_lat_lon.lon;			\
		}							\
	}




/* Make the first LatLonBBox larger by expanding it to include second LatLonBBox. */
#define BBOX_EXPAND_WITH_BBOX(m_target, m_source) {			\
		if (m_source.north > m_target.north || std::isnan(m_target.north)) { \
			m_target.north = m_source.north;		\
		}							\
		if (m_source.south < m_target.south || std::isnan(m_target.south)) { \
			m_target.south = m_source.south;		\
		}							\
		if (m_source.east > m_target.east || std::isnan(m_target.east)) { \
			m_target.east = m_source.east;			\
		}							\
		if (m_source.west < m_target.west || std::isnan(m_target.west)) { \
			m_target.west = m_source.west;			\
		}							\
	}




#endif /* #ifndef _SG_BOUNDING_BOX_H_ */
