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
	class Trackpoint;




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

	class TrackDataBase {
	public:
		TrackDataBase();
		TrackDataBase(int n_data_points);
		~TrackDataBase();

		sg_ret allocate_vector(int n_data_points);
		void invalidate(void);

		char debug[100] = { 0 };

		bool valid = false;
		int n_points = 0;

		double * x = NULL;
		double * y = NULL;

		Trackpoint ** tps = NULL;
	};




	template <typename Tx, typename Tx_ll, typename Ty, typename Ty_ll>
	class TrackData : public TrackDataBase {
	public:
		TrackData() : TrackDataBase() {};
		TrackData(int n_data_points) : TrackDataBase(n_data_points) {};


		TrackData & operator=(const TrackData & other);


		void calculate_min_max(void);


		sg_ret compress_into(TrackData & target, int compressed_n_points) const;

		sg_ret make_track_data_altitude_over_distance(Track * trk, int compressed_n_points);
		sg_ret make_track_data_gradient_over_distance(Track * trk, int compressed_n_points);
		sg_ret make_track_data_speed_over_time(Track * trk);
		sg_ret make_track_data_distance_over_time(Track * trk);
		sg_ret make_track_data_altitude_over_time(Track * trk);
		sg_ret make_track_data_speed_over_distance(Track * trk);

		sg_ret apply_unit_conversions_x(DistanceUnit distance_unit);
		sg_ret apply_unit_conversions_y(SpeedUnit speed_unit, DistanceUnit distance_unit, HeightUnit height_unit);



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

		Tx x_min;
		Tx x_max;
		Ty y_min;
		Ty y_max;

		GisViewportDomain x_domain = GisViewportDomain::MaxDomain;
		GisViewportDomain y_domain = GisViewportDomain::MaxDomain;

	private:
		DistanceUnit y_distance_unit = DistanceUnit::Meters;
		SpeedUnit y_speed_unit = SpeedUnit::MetresPerSecond;

		Tx_ll x_min_ll = 0;
	        Tx_ll x_max_ll = 0;
		Ty_ll y_min_ll = 0;
	        Ty_ll y_max_ll = 0;
	};
	template <typename Tx, typename Tx_ll, typename Ty, typename Ty_ll>
	QDebug operator<<(QDebug debug, const TrackData<Tx, Tx_ll, Ty, Ty_ll> & track_data);



	/**
	   @reviewed-on tbd
	*/
	template <typename Tx, typename Tx_ll, typename Ty, typename Ty_ll>
	void TrackData<Tx, Tx_ll, Ty, Ty_ll>::calculate_min_max(void)
	{
		this->x_min_ll = this->x[0];
		this->x_max_ll = this->x[0];
		for (int i = 1; i < this->n_points; i++) {
			if (this->x[i] >= this->x[i - 1]) { /* Non-decreasing x. */
				if (this->x[i] > this->x_max_ll) {
					this->x_max_ll = this->x[i];
				}

				if (this->x[i] < this->x_min_ll) {
					this->x_min_ll = this->x[i];
				}
				qDebug() << this->debug << " x[" << i << "] =" << qSetRealNumberPrecision(10)
					 << this->x[i] << ", x_min =" << this->x_min_ll << ", x_max =" << this->x_max_ll;
			}
		}


		this->y_min_ll = this->y[0];
		this->y_max_ll = this->y[0];
		for (int i = 1; i < this->n_points; i++) {
			if (!std::isnan(this->y[i])) {
				if (this->y[i] > this->y_max_ll) {
					this->y_max_ll = this->y[i];
				}

				if (this->y[i] < this->y_min_ll) {
					this->y_min_ll = this->y[i];
				}
				qDebug() << this->debug << "y[" << i << "] =" << qSetRealNumberPrecision(10)
					 << this->y[i] << ", y_min =" << this->y_min_ll << ", y_max =" << this->y_max_ll;
			}
		}

		this->x_min = Tx(this->x_min_ll, Tx::get_internal_unit());
		this->x_max = Tx(this->x_max_ll, Tx::get_internal_unit());
		this->y_min = Ty(this->y_min_ll, Ty::get_internal_unit());
		this->y_max = Ty(this->y_max_ll, Ty::get_internal_unit());
	}



	/**
	   @reviewed-on tbd
	*/
	template <typename Tx, typename Tx_ll, typename Ty, typename Ty_ll>
	TrackData<Tx, Tx_ll, Ty, Ty_ll> & TrackData<Tx, Tx_ll, Ty, Ty_ll>::operator=(const TrackData<Tx, Tx_ll, Ty, Ty_ll> & other)
	{
		if (&other == this) {
			return *this;
		}

		/* TODO_LATER: compare size of vectors in both objects to see if
		   reallocation is necessary? */

		if (other.x) {
			if (this->x) {
				free(this->x);
				this->x = NULL;
			}
			this->x = (double *) malloc(sizeof (double) * other.n_points);
			memcpy(this->x, other.x, sizeof (double) * other.n_points);
		}

		if (other.y) {
			if (this->y) {
				free(this->y);
				this->y = NULL;
			}
			this->y = (double *) malloc(sizeof (double) * other.n_points);
			memcpy(this->y, other.y, sizeof (double) * other.n_points);
		}

		if (other.tps) {
			if (this->tps) {
				free(this->tps);
				this->tps = NULL;
			}
			this->tps = (Trackpoint **) malloc(sizeof (Trackpoint *) * other.n_points);
			memcpy(this->tps, other.tps, sizeof (double) * other.n_points);
		}

		this->x_min = other.x_min;
		this->x_max = other.x_max;

		this->y_min = other.y_min;
		this->y_max = other.y_max;

		this->valid = other.valid;
		this->n_points = other.n_points;

		snprintf(this->debug, sizeof (this->debug), "%s", other.debug);

		this->x_domain = other.x_domain;
		this->y_domain = other.y_domain;

		this->y_distance_unit = other.y_distance_unit;
		this->y_speed_unit = other.y_speed_unit;


		return *this;
	}




	/**
	   @reviewed-on tbd
	*/
	template <typename Tx, typename Tx_ll, typename Ty, typename Ty_ll>
	QDebug operator<<(QDebug debug, const TrackData<Tx, Tx_ll, Ty, Ty_ll> & track_data)
	{
		if (track_data.valid) {
			debug << "TrackData" << track_data.debug << "is valid"
			      << qSetRealNumberPrecision(10)
			      << ", x_min =" << track_data.x_min.value
			      << ", x_max =" << track_data.x_max.value
			      << ", y_min =" << track_data.y_min.value
			      << ", y_max =" << track_data.y_max.value;
		} else {
			debug << "TrackData" << track_data.debug << "is invalid";
		}

		return debug;
	}




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TRACK_DATA_H_ */
