/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (c) 2012, Rob Norris <rw_norris@hotmail.com>
 * Copyright (c) 2016-2020, Kamil Ignacak <acerion@wp.pl>
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
	  <Y param> over <X param>, where <X param> is Time or
	  Distance.

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
		~TrackDataBase();

		/**
		   Convert values in @param values to unit @param to_unit.
		   Convert values in @param min and @param max to unit @param to_unit.

		   Do the conversions only if necessary, i.e. if
		   existing unit is different than given @param
		   to_unit. Existing unit is gotten from @param min.

		   @reviewed-on 2020-01-04
		*/
		template <typename T, typename T_ll, typename T_u>
		sg_ret apply_unit_conversion(T_ll & values, T & min, T & max, T_u to_unit)
		{
			const T_u from_unit = min.get_unit();
			if (from_unit != to_unit) {
				for (int i = 0; i < this->size(); i++) {
					values[i] = T::convert_to_unit(values[i], from_unit, to_unit);
				}
				min.convert_to_unit_in_place(to_unit);
				max.convert_to_unit_in_place(to_unit);
			}

			return sg_ret::ok;
		}


		bool is_valid(void) const { return this->m_valid; };
		int size(void) const { return this->m_n_points; };

		char m_debug[100] = { 0 };

	protected:
		bool m_valid = false;
		int m_n_points = 0;
	};




	template <typename Tx, typename Tx_ll, typename Tx_u, typename Ty, typename Ty_ll, typename Ty_u>
	class TrackData : public TrackDataBase {
	public:
		TrackData() : TrackDataBase() {};
		TrackData(int n_points) { this->allocate(n_points); }
		~TrackData() { this->clear(); }

		TrackData & operator=(const TrackData & other);


		void calculate_min_max(void);


		sg_ret compress_into(TrackData & target, int compressed_n_points) const;

		sg_ret make_track_data_x_over_y(Track * trk, int compressed_n_points); /* Unused. */
		sg_ret make_track_data_x_over_y(Track * trk);

		sg_ret apply_unit_conversions_xy(Tx_u to_unit_x, Ty_u to_unit_y)
		{
			if (sg_ret::ok != this->apply_unit_conversion(this->x, this->x_min, this->x_max, to_unit_x)) {
				//qDebug() << SG_PREFIX_E << "Failed to apply distance unit conversion to x vector";
				return sg_ret::err;
			}
			if (sg_ret::ok != this->apply_unit_conversion(this->y, this->y_min, this->y_max, to_unit_y)) {
				//qDebug() << SG_PREFIX_E << "Failed to apply speed unit conversion to y vector";
				return sg_ret::err;
			}
			return sg_ret::ok;

		}



		sg_ret allocate(int n_points);
		void clear(void);


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

		/* For averaging/smoothing over N points of a
		   track. Must be odd value. If set to 1, then no
		   averaging will take place. */
		int m_window_size = 1;

		Tx x_min;
		Tx x_max;
		Ty y_min;
		Ty y_max;

		GisViewportDomain x_domain = GisViewportDomain::MaxDomain;
		GisViewportDomain y_domain = GisViewportDomain::MaxDomain;

		Tx_ll * x = nullptr;
		Ty_ll * y = nullptr;

		Trackpoint ** tps = nullptr;


	private:
		DistanceUnit y_distance_unit = DistanceUnit::Unit::Meters;
		SpeedUnit y_speed_unit = SpeedUnit::Unit::MetresPerSecond;

		Tx_ll x_min_ll = 0;
	        Tx_ll x_max_ll = 0;
		Ty_ll y_min_ll = 0;
	        Ty_ll y_max_ll = 0;
	};
	template <typename Tx, typename Tx_ll, typename Tx_u, typename Ty, typename Ty_ll, typename Ty_u>
	QDebug operator<<(QDebug debug, const TrackData<Tx, Tx_ll, Tx_u, Ty, Ty_ll, Ty_u> & track_data);




	/**
	   @reviewed-on tbd
	*/
	template <typename Tx, typename Tx_ll, typename Tx_u, typename Ty, typename Ty_ll, typename Ty_u>
	void TrackData<Tx, Tx_ll, Tx_u, Ty, Ty_ll, Ty_u>::calculate_min_max(void)
	{
		this->x_min_ll = this->x[0];
		this->x_max_ll = this->x[0];
		for (int i = 1; i < this->size(); i++) {
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
		for (int i = 1; i < this->size(); i++) {
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
	template <typename Tx, typename Tx_ll, typename Tx_u, typename Ty, typename Ty_ll, typename Ty_u>
	TrackData<Tx, Tx_ll, Tx_u, Ty, Ty_ll, Ty_u> & TrackData<Tx, Tx_ll, Tx_u, Ty, Ty_ll, Ty_u>::operator=(const TrackData<Tx, Tx_ll, Tx_u, Ty, Ty_ll, Ty_u> & other)
	{
		if (&other == this) {
			return *this;
		}

		if (other.x) {
			const size_t size = sizeof (Tx_ll) * other.size();
			this->x = (Tx_ll *) realloc(this->x, size);
			memcpy(this->x, other.x, size);
		} else {
			free(this->x);
			this->x = nullptr;
		}

		if (other.y) {
			const size_t size = sizeof (Ty_ll) * other.size();
			this->y = (Ty_ll *) realloc(this->y, size);
			memcpy(this->y, other.y, size);
		} else {
			free(this->y);
			this->y = nullptr;
		}

		if (other.tps) {
			const size_t size = sizeof (Trackpoint *) * other.size();
			this->tps = (Trackpoint **) realloc(this->tps, size);
			memcpy(this->tps, other.tps, size);
		} else {
			free(this->tps);
			this->tps = nullptr;
		}

		this->x_min = other.x_min;
		this->x_max = other.x_max;

		this->y_min = other.y_min;
		this->y_max = other.y_max;

		this->m_valid = other.m_valid;
		this->m_n_points = other.m_n_points;

		snprintf(this->m_debug, sizeof (this->m_debug), "%s", other.m_debug);

		this->x_domain = other.x_domain;
		this->y_domain = other.y_domain;

		this->y_distance_unit = other.y_distance_unit;
		this->y_speed_unit = other.y_speed_unit;


		return *this;
	}




	/**
	   @reviewed-on tbd
	*/
	template <typename Tx, typename Tx_ll, typename Tx_u, typename Ty, typename Ty_ll, typename Ty_u>
	QDebug operator<<(QDebug debug, const TrackData<Tx, Tx_ll, Tx_u, Ty, Ty_ll, Ty_u> & track_data)
	{
		if (track_data.is_valid()) {
			debug << "TrackData" << track_data.m_debug << "is valid"
			      << qSetRealNumberPrecision(10)
			      << ", x_min =" << track_data.x_min
			      << ", x_max =" << track_data.x_max
			      << ", y_min =" << track_data.y_min
			      << ", y_max =" << track_data.y_max;
		} else {
			debug << "TrackData" << track_data.m_debug << "is invalid";
		}

		return debug;
	}




	/**
	   @reviewed-on tbd
	*/
	template <typename Tx, typename Tx_ll, typename Tx_u, typename Ty, typename Ty_ll, typename Ty_u>
	void TrackData<Tx, Tx_ll, Tx_u, Ty, Ty_ll, Ty_u>::clear(void)
	{
		this->m_valid = false;
		this->m_n_points = 0;

		if (this->x) {
			free(this->x);
			this->x = nullptr;
		}

		if (this->y) {
			free(this->y);
			this->y = nullptr;
		}

		if (this->tps) {
			free(this->tps);
			this->tps = nullptr;
		}
	}




	/**
	   @reviewed-on tbd
	*/
	template <typename Tx, typename Tx_ll, typename Tx_u, typename Ty, typename Ty_ll, typename Ty_u>
	sg_ret TrackData<Tx, Tx_ll, Tx_u, Ty, Ty_ll, Ty_u>::allocate(int n_points)
	{
		if (this->x) {
			if (this->size()) {
				qDebug() << "EE   TrackData" << __func__ << __LINE__ << "Called the function for already allocated vector x";
			} else {
				qDebug() << "WW   TrackData" << __func__ << __LINE__ << "Called the function for already allocated vector x";
			}
			free(this->x);
			this->x = nullptr;
		}

		if (this->y) {
			if (this->size()) {
				qDebug() << "EE   TrackData" << __func__ << __LINE__ << "Called the function for already allocated vector y";
			} else {
				qDebug() << "WW   TrackData" << __func__ << __LINE__ << "Called the function for already allocated vector y";
			}
			free(this->y);
			this->y = nullptr;
		}

		if (this->tps) {
			if (this->size()) {
				qDebug() << "EE   TrackData" << __func__ << __LINE__ << "Called the function for already allocated vector tps";
			} else {
				qDebug() << "WW   TrackData" << __func__ << __LINE__ << "Called the function for already allocated vector tps";
			}
			free(this->tps);
			this->tps = nullptr;
		}

		this->x = (Tx_ll *) malloc(sizeof (Tx_ll) * n_points);
		if (nullptr == this->x) {
			qDebug() << "EE   TrackData" << __func__ << __LINE__ << "Failed to allocate 'x' vector";
			return sg_ret::err;
		}

		this->y = (Ty_ll *) malloc(sizeof (Ty_ll) * n_points);
		if (nullptr == this->y) {
			free(this->x);
			this->x = nullptr;
			qDebug() << "EE   TrackData" << __func__ << __LINE__ << "Failed to allocate 'y' vector";
			return sg_ret::err;
		}

		this->tps = (Trackpoint **) malloc(sizeof (Trackpoint *) * n_points);
		if (nullptr == this->tps) {
			free(this->x);
			this->x = nullptr;
			free(this->y);
			this->y = nullptr;
			qDebug() << "EE   TrackData" << __func__ << __LINE__ << "Failed to allocate 'tps' vector";
			return sg_ret::err;
		}

		memset(this->x, 0, sizeof (Tx_ll) * n_points);
		memset(this->y, 0, sizeof (Ty_ll) * n_points);
		memset(this->tps, 0, sizeof (Trackpoint *) * n_points);

		/* There are n cells in vectors, but the data in the
		   vectors is still zero. */
		this->m_n_points = n_points;

		return sg_ret::ok;
	}




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TRACK_DATA_H_ */
