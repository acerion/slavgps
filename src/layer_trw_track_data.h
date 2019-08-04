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

#ifndef _SG_TRACK_DATA_H_
#define _SG_TRACK_DATA_H_




#include <QDebug>




#include "viewport.h"
#include "measurements.h"




namespace SlavGPS {




	/*
	  Convenience class for pair of vectors representing track's
	  Speed/Time or Altitude/Timestamp information.

	  The two vectors are put in one data structure because
	  values in the vectors are calculated during one iteration
	  over the same trackpoint list. If I decided to split the
	  two vectors, I would have to collect data in the vectors
	  during two separate iterations over trackpoint list. This
	  would take twice as much time.
	*/
	class TrackData {
	public:
		TrackData();
		TrackData(int n_data_points);
		~TrackData();

		TrackData & operator=(const TrackData & other);

		void invalidate(void);
		void calculate_min_max(void);
		void allocate_vector(int n_data_points);

		TrackData compress(int compressed_size) const;

		sg_ret y_distance_convert_meters_to(DistanceUnit distance_unit);
		sg_ret y_speed_convert_mps_to(SpeedUnit speed_unit);

		char debug[100] = { 0 };

		double * x = NULL;
		double * y = NULL;

		double x_min = 0.0;
		double x_max = 0.0;

		double y_min = 0.0;
		double y_max = 0.0;

		bool valid = false;
		int n_points = 0;

		GisViewportDomain x_domain = GisViewportDomain::Max;
		GisViewportDomain y_domain = GisViewportDomain::Max;

		DistanceUnit y_distance_unit = DistanceUnit::Kilometres;
		SupplementaryDistanceUnit y_supplementary_distance_unit = SupplementaryDistanceUnit::Meters;
		SpeedUnit y_speed_unit = SpeedUnit::MetresPerSecond;

	private:
		sg_ret do_compress(TrackData & compressed_data) const;
	};
	QDebug operator<<(QDebug debug, const TrackData & track_data);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TRACK_DATA_H_ */
