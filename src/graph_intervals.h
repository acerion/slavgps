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




namespace SlavGPS {




	template <class T>
	class GraphIntervals {
	public:
		GraphIntervals() {};
		GraphIntervals(const T * interval_values, int n_interval_values) : values(interval_values), n_values(n_interval_values) {};

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
	int GraphIntervals<T>::get_interval_index(T min, T max, int n_intervals)
	{
		const T interval_upper_limit = (max - min) / n_intervals;

		/* Search for index of nearest interval. */
		int index = 0;
		while (interval_upper_limit > this->values[index]) {
			index++;
			/* Last Resort Check */
			if (index == this->n_values) {
				/* Return the last valid value. */
				index--;
				qDebug() << "++++" << __FUNCTION__ << __LINE__ << "min/max/n_intervals:" << min << max << n_intervals << "index1 =" << index << ", interval =" << this->values[index];
				return index;
			}
		}

		if (index != 0) {
			/* To cancel out the last index++ in the loop above:
			   that one last increment was one too many. */
			index--;
		}

		qDebug() << "++++" << __FUNCTION__ << __LINE__ << "min/max/n_intervals:" << min << max << n_intervals << "index2 =" << index << ", interval =" << this->values[index];

		return index;
	}




	template <class T>
	T GraphIntervals<T>::get_interval_value(int index)
	{
		return this->values[index];
	}




	class GraphIntervalsTime {
	public:
		GraphIntervalsTime();
		GraphIntervals <time_t> intervals;
	};

	class GraphIntervalsDistance {
	public:
		GraphIntervalsDistance();
		GraphIntervals <double> intervals;
	};

	class GraphIntervalsAltitude {
	public:
		GraphIntervalsAltitude();
		GraphIntervals <double> intervals;
	};

	class GraphIntervalsGradient {
	public:
		GraphIntervalsGradient();
		GraphIntervals <double> intervals;
	};

	class GraphIntervalsSpeed {
	public:
		GraphIntervalsSpeed();
		GraphIntervals <double> intervals;
	};




}




#endif /* #ifndef _SG_GRAPH_INTERVALS_H_ */
