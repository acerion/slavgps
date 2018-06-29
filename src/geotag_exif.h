/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2011-2014, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_GEOTAG_EXIF_H_
#define _SG_GEOTAG_EXIF_H_



#include <QString>




#include "coord.h"




namespace SlavGPS {




	class Waypoint;



	class GeotagExif {

	public:
		static Waypoint * create_waypoint_from_file(const QString & file_full_path, CoordMode coord_mode);

		static QString waypoint_set_comment_get_name(const QString & file_full_path, Waypoint * wp);

		static QString get_exif_date_from_file(const QString & file_full_path, bool * has_GPS_info);

		static LatLon get_position(const QString & file_full_path);

		static int write_exif_gps(const QString & file_full_path, const Coord & coord, double alt, bool no_change_mtime);
	};




} /* namespace SlavGPS */




#endif /* _SG_GEOTAG_EXIF_H_ */
