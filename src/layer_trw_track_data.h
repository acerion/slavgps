/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (c) 2012, Rob Norris <rw_norris@hotmail.com>
 * Copyright (c) 2016-2019, Kamil Ignacak <acerion@wp.pl>
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



	class Track;




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
		sg_ret allocate_vector(int n_data_points);

		sg_ret compress_into(TrackData & target, int compressed_n_points) const;

		sg_ret make_track_data_altitude_over_distance(Track * trk, int compressed_n_points);
		sg_ret make_track_data_gradient_over_distance(Track * trk, int compressed_n_points);
		sg_ret make_track_data_speed_over_time(Track * trk);
		sg_ret make_track_data_distance_over_time(Track * trk);
		sg_ret make_track_data_altitude_over_time(Track * trk);
		sg_ret make_track_data_speed_over_distance(Track * trk);

		sg_ret apply_unit_conversions(SpeedUnit speed_unit, DistanceUnit distance_unit, HeightUnit height_unit);


		char debug[100] = { 0 };

		double * x = NULL;
		double * y = NULL;

		/*
		  It is not that obvious how x_min/x_max should be
		  initialized before we start calculating these values
		  for whole graph.

		  Should x_min for time-based graph be initialized
		  with:
		   - zero? that won't work for tracks that start at non-zero timestamp;
		   - timestamp of first trackpoint? that won't work for tracks where first timestamp has invalid timestamp or invalid 'y' value.
		  Similar considerations should be done for
		  y_min/y_max.

		  Therefore initialization of these four fields is
		  left to code that calculates min/max values. The
		  boolean flag is set the moment the code find some
		  sane and valid initial values.
		*/
		bool extremes_initialized = false;
		double x_min = 0.0;
		double x_max = 0.0;
		double y_min = 0.0;
		double y_max = 0.0;

		bool valid = false;
		int n_points = 0;

		GisViewportDomain x_domain = GisViewportDomain::Max;
		GisViewportDomain y_domain = GisViewportDomain::Max;

	private:
		DistanceUnit y_distance_unit = DistanceUnit::Kilometres;
		SupplementaryDistanceUnit y_supplementary_distance_unit = SupplementaryDistanceUnit::Meters;
		SpeedUnit y_speed_unit = SpeedUnit::MetresPerSecond;
	};
	QDebug operator<<(QDebug debug, const TrackData & track_data);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TRACK_DATA_H_ */
