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




	class GraphIntervals {
	};


	template <class T>
	class GraphIntervalsTyped : public GraphIntervals {
	public:
		GraphIntervalsTyped() {};
		GraphIntervalsTyped(const T * interval_values, int n_interval_values) : values(interval_values), n_values(n_interval_values) {};

		int get_interval_index(T min, T max, int n_intervals);
		T get_interval_value(int index);

		const T * values = NULL;
		int n_values = 0;
	};




	/* This method is used for purposes of determining how large a
	   distance - an interval of values - will be if we split min-max
	   range into n_intervals. Then there will be n_interval grid lines
	   drawn on a graph, each spaced at interval. */
	template <class T>
	int GraphIntervalsTyped<T>::get_interval_index(T min, T max, int n_intervals)
	{
		const T interval_upper_limit = (max - min) / n_intervals;
		qDebug() << "II: Intervals:" << __FUNCTION__ << __LINE__ << "min/max/n_intervals/interval upper limit:" << min << max << n_intervals << interval_upper_limit;

		/* Search for index of nearest interval. */
		int index = 0;
		for (index = 0; index < this->n_values; index++) {
			if (interval_upper_limit == this->values[index]) {
				qDebug() << "II: Intervals:" << __FUNCTION__ << __LINE__ << "found exact interval value" << this->values[index];
				break;
			} else if (interval_upper_limit < this->values[index] && index > 0) {
				index--;
				qDebug() << "II: Intervals:" << __FUNCTION__ << __LINE__ << "found smaller interval value" << this->values[index];
				break;
			} else {
				; /* Keep looking. */
			}
		}

		if (index == this->n_values) {
			index--;
			qDebug() << "II: Intervals:" << __FUNCTION__ << __LINE__ << "interval value not found, returning last interval value" << this->values[index];
		}

		return index;
	}




	template <class T>
	T GraphIntervalsTyped<T>::get_interval_value(int index)
	{
		return this->values[index];
	}




	class GraphIntervalsTime {
	public:
		GraphIntervalsTime();
		GraphIntervalsTyped<Time> intervals;
	};

	class GraphIntervalsDistance {
	public:
		GraphIntervalsDistance();
		GraphIntervalsTyped<Distance> intervals;
	};

	class GraphIntervalsAltitude {
	public:
		GraphIntervalsAltitude();
		GraphIntervalsTyped <double> intervals;
	};

	class GraphIntervalsGradient {
	public:
		GraphIntervalsGradient();
		GraphIntervalsTyped <double> intervals;
	};

	class GraphIntervalsSpeed {
	public:
		GraphIntervalsSpeed();
		GraphIntervalsTyped <double> intervals;
	};








	class GraphIntervalsBase {
	};




	template <class T>
	class GraphIntervals2 : public GraphIntervalsBase {
	public:
		GraphIntervals2() {};
		GraphIntervals2(const T * interval_values, int n_interval_values) : values(interval_values), n_values(n_interval_values) {};

		int get_interval_index(T min, T max, int n_intervals);
		T get_interval_value(int index) { return this->values[index]; }

		T * values = NULL;
		int n_values = 0;
	};




	/* This method is used for purposes of determining how large a
	   distance - an interval of values - will be if we split min-max
	   range into n_intervals. Then there will be n_interval grid lines
	   drawn on a graph, each spaced at interval. */
	template <class T>
	int GraphIntervals2<T>::get_interval_index(T min, T max, int n_intervals)
	{
		const T interval_upper_limit = (max - min) / n_intervals;
		qDebug() << "II min/max/n_intervals/interval upper limit:" << min << max << n_intervals << interval_upper_limit;

		/* Search for index of nearest interval. */
		int index = 0;
		for (index = 0; index < this->n_values; index++) {
			if (interval_upper_limit == this->values[index]) {
				qDebug() << "II Found exact interval value" << this->values[index];
				break;
			} else if (interval_upper_limit < this->values[index] && index > 0) {
				index--;
				qDebug() << "II Found smaller interval value" << this->values[index];
				break;
			} else {
				; /* Keep looking. */
			}
		}

		if (index == this->n_values) {
			index--;
			qDebug() << "II Interval value not found, returning last interval value" << this->values[index];
		}

		return index;
	}








}




#endif /* #ifndef _SG_GRAPH_INTERVALS_H_ */
