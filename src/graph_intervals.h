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
			iter->unit = min.unit;
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

		return this->values[index];
	}




}




#endif /* #ifndef _SG_GRAPH_INTERVALS_H_ */
