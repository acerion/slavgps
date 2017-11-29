/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
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
#ifndef _SG_MEASUREMENTS_H_
#define _SG_MEASUREMENTS_H_




#include <QString>




namespace SlavGPS {




#define SG_PRECISION_ALTITUDE      5
#define SG_ALTITUDE_PRECISION      2
#define SG_ALTITUDE_RANGE_MIN  -5000 /* [meters] */   /* See also VIK_VAL_MIN_ALT */
#define SG_ALTITUDE_RANGE_MAX  25000 /* [meters] */   /* See also VIK_VAL_MAX_ALT */

#define SG_PRECISION_DISTANCE   2
#define SG_PRECISION_SPEED      2
#define SG_PRECISION_COURSE     1
#define SG_PRECISION_LATITUDE   6
#define SG_PRECISION_LONGITUDE  6




	class Measurements {
	public:
		static QString get_altitude_string(double value, int precision = SG_PRECISION_ALTITUDE);
		static QString get_distance_string_short(double value, int precision = SG_PRECISION_DISTANCE);
		static QString get_distance_string(double value, int precision = SG_PRECISION_DISTANCE);
		static QString get_speed_string(double value, int precision = SG_PRECISION_SPEED);
		static QString get_course_string(double value, int precision = SG_PRECISION_COURSE);

		static QString get_file_size_string(size_t file_size);
	};




}




#endif /* #ifndef _SG_MEASUREMENTS_H_ */
