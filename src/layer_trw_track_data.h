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

		bool is_valid(void) const { return this->m_valid; };
		int size(void) const { return this->m_n_points; };

		char m_debug[100] = { 0 };

	protected:
		bool m_valid = false;
		int m_n_points = 0;
	};




	template <typename Tx, typename Ty>
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

		/**
		   Convert all unit-bearing values in the track data to new units.

		   Do the conversions only if necessary, i.e. if
		   existing units are different than given @param
		   to_unit_x and @param to_unit_y units.

		   @reviewed-on 2020-01-07
		*/
		sg_ret apply_unit_conversions_xy(typename Tx::Unit to_unit_x, typename Ty::Unit to_unit_y)
		{
			if (to_unit_x != this->x_unit) {
				for (int i = 0; i < this->size(); i++) {
					this->m_x_ll[i] = Tx::convert_to_unit(this->m_x_ll[i], this->x_unit, to_unit_x);
				}
				this->m_x_min.convert_to_unit_in_place(to_unit_x);
				this->m_x_max.convert_to_unit_in_place(to_unit_x);
				this->x_unit = to_unit_x;
			}
			if (to_unit_y != this->y_unit) {
				for (int i = 0; i < this->size(); i++) {
					this->m_y_ll[i] = Ty::convert_to_unit(this->m_y_ll[i], this->y_unit, to_unit_y);
				}
				this->m_y_min.convert_to_unit_in_place(to_unit_y);
				this->m_y_max.convert_to_unit_in_place(to_unit_y);
				this->y_unit = to_unit_y;
			}
			return sg_ret::ok;
		}



		sg_ret allocate(int n_points);
		void clear(void);



		/* For averaging/smoothing over N points of a
		   track. Must be odd value. If set to 1, then no
		   averaging will take place. */
		int m_window_size = 1;


		GisViewportDomain x_domain = GisViewportDomain::MaxDomain;
		GisViewportDomain y_domain = GisViewportDomain::MaxDomain;

		typename Tx::LL x_ll(int i) const { return this->m_x_ll[i]; }
		typename Ty::LL y_ll(int i) const { return this->m_y_ll[i]; }
		Trackpoint * tp(int i) const { return this->m_tps[i]; }

		const Tx & x_min(void) const { return this->m_x_min; };
		const Tx & x_max(void) const { return this->m_x_max; };
		const Ty & y_min(void) const { return this->m_y_min; };
		const Ty & y_max(void) const { return this->m_y_max; };

	protected:
		typename Tx::Unit x_unit;
		typename Ty::Unit y_unit;

		typename Tx::LL * m_x_ll = nullptr;
		typename Ty::LL * m_y_ll = nullptr;

		Trackpoint ** m_tps = nullptr;

		Tx m_x_min;
		Tx m_x_max;
		Ty m_y_min;
		Ty m_y_max;
	};





	/**
	   @reviewed-on tbd
	*/
	template <typename Tx, typename Ty>
	void TrackData<Tx, Ty>::calculate_min_max(void)
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
	template <typename Tx, typename Ty>
	TrackData<Tx, Ty> & TrackData<Tx, Ty>::operator=(const TrackData<Tx, Ty> & other)
	{
		if (&other == this) {
			return *this;
		}

		if (other.m_x_ll) {
			const size_t size = sizeof (typename Tx::LL) * other.size();
			this->m_x_ll = (typename Tx::LL *) realloc(this->m_x_ll, size);
			memcpy(this->m_x_ll, other.m_x_ll, size);
		} else {
			free(this->m_x_ll);
			this->m_x_ll = nullptr;
			this->m_valid = false;
		}

		if (other.m_y_ll) {
			const size_t size = sizeof (typename Ty::LL) * other.size();
			this->m_y_ll = (typename Ty::LL *) realloc(this->m_y_ll, size);
			memcpy(this->m_y_ll, other.m_y_ll, size);
		} else {
			free(this->m_y_ll);
			this->m_y_ll = nullptr;
			this->m_valid = false;
		}

		if (other.m_tps) {
			const size_t size = sizeof (Trackpoint *) * other.size();
			this->m_tps = (Trackpoint **) realloc(this->m_tps, size);
			memcpy(this->m_tps, other.m_tps, size);
		} else {
			free(this->m_tps);
			this->m_tps = nullptr;
			this->m_valid = false;
		}

		this->m_x_min = other.m_x_min;
		this->m_x_max = other.m_x_max;

		this->m_y_min = other.m_y_min;
		this->m_y_max = other.m_y_max;

		this->m_valid = other.m_valid;
		this->m_n_points = other.m_n_points;

		snprintf(this->m_debug, sizeof (this->m_debug), "%s", other.m_debug);

		this->x_domain = other.x_domain;
		this->y_domain = other.y_domain;

		this->x_unit = other.x_unit;
		this->y_unit = other.y_unit;

		return *this;
	}




	/**
	   @reviewed-on tbd
	*/
	template <typename Tx, typename Ty>
	QDebug operator<<(QDebug debug, const TrackData<Tx, Ty> & track_data)
	{
		if (track_data.is_valid()) {
			debug << "TrackData" << track_data.m_debug << "is valid"
			      << qSetRealNumberPrecision(10)
			      << ", x_min =" << track_data.x_min()
			      << ", x_max =" << track_data.x_max()
			      << ", y_min =" << track_data.y_min()
			      << ", y_max =" << track_data.y_max();
		} else {
			debug << "TrackData" << track_data.m_debug << "is invalid";
		}

		return debug;
	}




	/**
	   @reviewed-on tbd
	*/
	template <typename Tx, typename Ty>
	void TrackData<Tx, Ty>::clear(void)
	{
		this->m_valid = false;
		this->m_n_points = 0;

		if (this->m_x_ll) {
			free(this->m_x_ll);
			this->m_x_ll = nullptr;
		}

		if (this->m_y_ll) {
			free(this->m_y_ll);
			this->m_y_ll = nullptr;
		}

		if (this->m_tps) {
			free(this->m_tps);
			this->m_tps = nullptr;
		}
	}




	/**
	   @reviewed-on tbd
	*/
	template <typename Tx, typename Ty>
	sg_ret TrackData<Tx, Ty>::allocate(int n_points)
	{
		if (this->m_x_ll) {
			if (this->size()) {
				qDebug() << "EE   TrackData" << __func__ << __LINE__ << "Called the function for already allocated vector x";
			} else {
				qDebug() << "WW   TrackData" << __func__ << __LINE__ << "Called the function for already allocated vector x";
			}
			free(this->m_x_ll);
			this->m_x_ll = nullptr;
		}

		if (this->m_y_ll) {
			if (this->size()) {
				qDebug() << "EE   TrackData" << __func__ << __LINE__ << "Called the function for already allocated vector y";
			} else {
				qDebug() << "WW   TrackData" << __func__ << __LINE__ << "Called the function for already allocated vector y";
			}
			free(this->m_y_ll);
			this->m_y_ll = nullptr;
		}

		if (this->m_tps) {
			if (this->size()) {
				qDebug() << "EE   TrackData" << __func__ << __LINE__ << "Called the function for already allocated vector tps";
			} else {
				qDebug() << "WW   TrackData" << __func__ << __LINE__ << "Called the function for already allocated vector tps";
			}
			free(this->m_tps);
			this->m_tps = nullptr;
		}

		this->m_x_ll = (typename Tx::LL *) malloc(sizeof (typename Tx::LL) * n_points);
		if (nullptr == this->m_x_ll) {
			qDebug() << "EE   TrackData" << __func__ << __LINE__ << "Failed to allocate 'x' vector";
			return sg_ret::err;
		}

		this->m_y_ll = (typename Ty::LL *) malloc(sizeof (typename Ty::LL) * n_points);
		if (nullptr == this->m_y_ll) {
			free(this->m_x_ll);
			this->m_x_ll = nullptr;
			qDebug() << "EE   TrackData" << __func__ << __LINE__ << "Failed to allocate 'y' vector";
			return sg_ret::err;
		}

		this->m_tps = (Trackpoint **) malloc(sizeof (Trackpoint *) * n_points);
		if (nullptr == this->m_tps) {
			free(this->m_x_ll);
			this->m_x_ll = nullptr;
			free(this->m_y_ll);
			this->m_y_ll = nullptr;
			qDebug() << "EE   TrackData" << __func__ << __LINE__ << "Failed to allocate 'tps' vector";
			return sg_ret::err;
		}

		memset(this->m_x_ll, 0, sizeof (typename Tx::LL) * n_points);
		memset(this->m_y_ll, 0, sizeof (typename Ty::LL) * n_points);
		memset(this->m_tps, 0, sizeof (Trackpoint *) * n_points);

		/* There are n cells in vectors, but the data in the
		   vectors is still zero. */
		this->m_n_points = n_points;

		return sg_ret::ok;
	}




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TRACK_DATA_H_ */
