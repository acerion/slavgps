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

#ifndef _SG_GRAPH_INTERVALS_H_
#define _SG_GRAPH_INTERVALS_H_




#include "measurements.h"




namespace SlavGPS {




	template <class T>
	class GraphIntervals {
	public:
		GraphIntervals() {};

		const T & get_interval(const T & min, const T & max, int n_intervals);

		static void find_multiples_of_interval(const T & min_visible, const T & max_visible, const T & interval, T & first_multiple, T & last_multiple);

	private:
		std::vector<T> values;
	};




	/*
	  This method is used for purposes of determining how large a
	  distance - an interval of values - will be if we split
	  min-max range into n_intervals. Then there will be
	  n_interval grid lines drawn on a graph, each spaced at
	  interval.

	  @reviewed-on: 2019-08-24
	*/
	template <class T>
	const T & GraphIntervals<T>::get_interval(const T & min, const T & max, int n_intervals)
	{
		const T interval_upper_limit = (max - min) / n_intervals;
		qDebug() << "II  " << __func__ << "min/max/n_intervals/interval upper limit:" << min << max << n_intervals << interval_upper_limit;

		/* Range (min, max) for which we want to calculate
		   interval, may have different units. Let's fix
		   this. */
		for (auto iter = this->values.begin(); iter != this->values.end(); iter++) {
			qDebug() << "II  " << __func__ << __LINE__ << (*iter);
			iter->unit = min.unit;
			qDebug() << "II  " << __func__ << __LINE__ << (*iter);
		}

		/* Search for index of nearest interval. */
		const auto n_values = this->values.size();
		auto index = n_values;
		for (index = 0; index < n_values; index++) {
			if (interval_upper_limit == this->values[index]) {
				qDebug() << "II  " << __func__ << "Found exact interval value" << this->values[index];
				break;
			} else if (interval_upper_limit < this->values[index] && index > 0) {
				index--;
				qDebug() << "II  " << __func__ << "Found smaller interval value" << this->values[index];
				break;
			} else {
				; /* Keep looking. */
			}
		}

		if (index == n_values) {
			index--;
			qDebug() << "NN  " << __func__ << "Interval value not found, returning last interval value" << this->values[index];
		}

		qDebug() << "II  " << __func__ << __LINE__ << "Returning interval" << this->values[index];
		return this->values[index];
	}



	/**
	   Find first grid line. It will be a multiple of interval
	   just below min_visible.

	   Find last grid line. It will be a multiple of interval
	   just above max_visible.

	   All grid lines will be drawn starting from the first to
	   last (provided that they will fall within graph's main
	   area).

	   When looking for first and last line, start from zero value
	   and go up or down: a grid line will be always drawn at zero
	   and/or at multiples of interval (depending whether they
	   will fall within graph's main area).
	*/
	template <class T>
	void GraphIntervals<T>::find_multiples_of_interval(const T & min_visible, const T & max_visible, const T & interval, T & first_multiple, T & last_multiple)
	{
		int n = 0;

		n = floor(min_visible / interval);
		first_multiple = interval * (n - 1);

		n = ceil(max_visible / interval);
		last_multiple = interval * (n + 1);

		qDebug() << "II   ProfileView" << __func__ << __LINE__
			 << "min visible =" << min_visible
			 << ", max visible =" << max_visible
			 << ", interval =" << interval
			 << ", first multiple =" << first_multiple
			 << ", last multiple =" << last_multiple;

#if 0

		int k = 0;
		qDebug() << "II   ProfileView" << __func__ << __LINE__ << "min_visible =" << min_visible << ", max_visible =" << max_visible << QTime::currentTime();
		/* 'first_multiple * y_interval' will be below y_visible_min. */
		if (min_visible <= 0) {

			qDebug() << "II   ProfileView" << __func__ << __LINE__ << "Before first loop:" << QTime::currentTime();
			while (interval * first_multiple > min_visible) {
				first_multiple--;
				k++;
			}
			qDebug() << "II   ProfileView" << __func__ << __LINE__ << "After first loop:" << QTime::currentTime() << "total iterations =" << k;
		} else {
			qDebug() << "II   ProfileView" << __func__ << __LINE__ << "Before second loop:" << QTime::currentTime();
			while (interval * first_multiple + interval < min_visible) {
				first_multiple++;
				k++;
			}
			qDebug() << "II   ProfileView" << __func__ << __LINE__ << "After second loop:" << QTime::currentTime() << "total iterations =" << k;
		}

		/* 'last_multiple * y_interval' will be above y_visible_max. */
		if (max_visible <= 0) {
			qDebug() << "II   ProfileView" << __func__ << __LINE__ << "Before third loop:" << QTime::currentTime();
			while (interval * last_multiple - interval > max_visible) {
				last_multiple--;
				k++;
			}
			qDebug() << "II   ProfileView" << __func__ << __LINE__ << "After third loop:" << QTime::currentTime() << "total iterations =" << k;
		} else {
			qDebug() << "II   ProfileView" << __func__ << __LINE__ << "Before fourth loop:" << QTime::currentTime();
			while (interval * last_multiple < max_visible) {
				last_multiple++;
				k++;
			}
			qDebug() << "II   ProfileView" << __func__ << __LINE__ << "After fourth loop:" << QTime::currentTime() << "total iterations =" << k;
		}

		if (k > 30) {
			/* I've encountered a problem where this function has
			   been executing for 3 seconds and one of while loops
			   was making 25034897 iterations. It should be only a
			   few of iterations. */
			qDebug() << "EE   ProfileView" << __func__ << __LINE__ << "Problems with calculating grid line indices, k =" << k;
		}
#endif
	}




}




#endif /* #ifndef _SG_GRAPH_INTERVALS_H_ */
