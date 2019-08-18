/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2007, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2007-2008, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2012-2014, Rob Norris <rw_norris@hotmail.com>
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




#include <QDebug>




#include "graph_intervals.h"




using namespace SlavGPS;




#define SG_MODULE "Graph Intervals"




/* (Hopefully!) Human friendly altitude grid sizes - note no fixed 'ratio' just numbers that look nice... */
static const double interval_values_altitude[] = {1.0, 2.0, 4.0, 5.0, 10.0, 15.0, 20.0,
						  25.0, 40.0, 50.0, 75.0, 100.0,
						  150.0, 200.0, 250.0, 375.0, 500.0,
						  750.0, 1000.0, 2000.0, 5000.0, 10000.0, 100000.0};

/* (Hopefully!) Human friendly gradient grid sizes - note no fixed 'ratio' just numbers that look nice... */
static const double interval_values_gradient[] = {1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 10.0,
						  12.0, 15.0, 20.0, 25.0, 30.0, 35.0, 40.0, 45.0, 50.0, 75.0,
						  100.0, 150.0, 200.0, 250.0, 375.0, 500.0,
						  750.0, 1000.0, 10000.0, 100000.0};
/* Normally gradients should range up to couple hundred precent at most,
   however there are possibilities of having points with no altitude after a point with a big altitude
  (such as places with invalid DEM values in otherwise mountainous regions) - thus giving huge negative gradients. */

/* (Hopefully!) Human friendly grid sizes - note no fixed 'ratio' just numbers that look nice... */
/* As need to cover walking speeds - have many low numbers (but also may go up to airplane speeds!). */
static const double interval_values_speed[] = {1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 10.0,
					       15.0, 20.0, 25.0, 40.0, 50.0, 75.0,
					       100.0, 150.0, 200.0, 250.0, 375.0, 500.0,
					       750.0, 1000.0, 10000.0};

/* (Hopefully!) Human friendly distance grid sizes - note no fixed 'ratio' just numbers that look nice... */
static const Distance interval_values_distance[] = {
	Distance(0.1),
	Distance(0.2),
	Distance(0.5),
	Distance(1.0),
	Distance(2.0),
	Distance(3.0),
	Distance(4.0),
	Distance(5.0),
	Distance(8.0),
	Distance(10.0),
	Distance(15.0),
	Distance(20.0),
	Distance(25.0),
	Distance(40.0),
	Distance(50.0),
	Distance(75.0),
	Distance(100.0),
	Distance(150.0),
	Distance(200.0),
	Distance(250.0),
	Distance(375.0),
	Distance(500.0),
	Distance(750.0),
	Distance(1000.0),
	Distance(10000.0) };

/* Time intervals in seconds. */
static const Time interval_values_time[] = {
	Time(60),     /* 1 minute. */
	Time(120),    /* 2 minutes. */
	Time(300),    /* 5 minutes. */
	Time(900),    /* 15 minutes. */
	Time(1800),   /* half hour. */
	Time(3600),   /* 1 hour. */
	Time(10800),  /* 3 hours. */
	Time(21600),  /* 6 hours. */
	Time(43200),  /* 12 hours. */
	Time(86400),  /* 1 day. */
	Time(172800), /* 2 days. */
	Time(604800), /* 1 week. */
	Time(1209600),/* 2 weeks. */
	Time(2419200),/* 4 weeks. */
};



/* (Hopefully!) Human friendly altitude grid sizes - note no fixed 'ratio' just numbers that look nice... */
static const Altitude interval_values_altitude2[] = {
	Altitude(1.0),
	Altitude(2.0),
	Altitude(4.0),
	Altitude(5.0),
	Altitude(10.0),
	Altitude(15.0),
	Altitude(20.0),
	Altitude(25.0),
	Altitude(40.0),
	Altitude(50.0),
	Altitude(75.0),
	Altitude(100.0),
	Altitude(150.0),
	Altitude(200.0),
	Altitude(250.0),
	Altitude(375.0),
	Altitude(500.0),
	Altitude(750.0),
	Altitude(1000.0),
	Altitude(2000.0),
	Altitude(5000.0),
	Altitude(10000.0),
	Altitude(100000.0)
};



/* (Hopefully!) Human friendly gradient grid sizes - note no fixed 'ratio' just numbers that look nice... */
static const Gradient interval_values_gradient2[] = {
	Gradient(1.0),
	Gradient(2.0),
	Gradient(3.0),
	Gradient(4.0),
	Gradient(5.0),
	Gradient(8.0),
	Gradient(10.0),
	Gradient(12.0),
	Gradient(15.0),
	Gradient(20.0),
	Gradient(25.0),
	Gradient(30.0),
	Gradient(35.0),
	Gradient(40.0),
	Gradient(45.0),
	Gradient(50.0),
	Gradient(75.0),
	Gradient(100.0),
	Gradient(150.0),
	Gradient(200.0),
	Gradient(250.0),
	Gradient(375.0),
	Gradient(500.0),
	Gradient(750.0),
	Gradient(1000.0),
	Gradient(10000.0),
	Gradient(100000.0)
};

/* Normally gradients should range up to couple hundred precent at most,
   however there are possibilities of having points with no altitude after a point with a big altitude
  (such as places with invalid DEM values in otherwise mountainous regions) - thus giving huge negative gradients. */

/* (Hopefully!) Human friendly grid sizes - note no fixed 'ratio' just numbers that look nice... */
/* As need to cover walking speeds - have many low numbers (but also may go up to airplane speeds!). */
static const SpeedUnit internal_unit_speed = SpeedUnit::MetresPerSecond;
static const Speed interval_values_speed2[] = {
	Speed(1.0, internal_unit_speed),
	Speed(2.0, internal_unit_speed),
	Speed(3.0, internal_unit_speed),
	Speed(4.0, internal_unit_speed),
	Speed(5.0, internal_unit_speed),
	Speed(8.0, internal_unit_speed),
	Speed(10.0, internal_unit_speed),
	Speed(15.0, internal_unit_speed),
	Speed(20.0, internal_unit_speed),
	Speed(25.0, internal_unit_speed),
	Speed(40.0, internal_unit_speed),
	Speed(50.0, internal_unit_speed),
	Speed(75.0, internal_unit_speed),
	Speed(100.0, internal_unit_speed),
	Speed(150.0, internal_unit_speed),
	Speed(200.0, internal_unit_speed),
	Speed(250.0, internal_unit_speed),
	Speed(375.0, internal_unit_speed),
	Speed(500.0, internal_unit_speed),
	Speed(750.0, internal_unit_speed),
	Speed(1000.0, internal_unit_speed),
	Speed(10000.0, internal_unit_speed)
};



GraphIntervalsTime::GraphIntervalsTime()
{
	this->intervals = GraphIntervalsTyped<Time>(interval_values_time, sizeof (interval_values_time) / sizeof (interval_values_time[0]));
}

GraphIntervalsDistance::GraphIntervalsDistance()
{
	this->intervals = GraphIntervalsTyped<Distance>(interval_values_distance, sizeof (interval_values_distance) / sizeof (interval_values_distance[0]));
}

GraphIntervalsAltitude::GraphIntervalsAltitude()
{
	this->intervals = GraphIntervalsTyped<double>(interval_values_altitude, sizeof (interval_values_altitude) / sizeof (interval_values_altitude[0]));
}

GraphIntervalsGradient::GraphIntervalsGradient()
{
	this->intervals = GraphIntervalsTyped<double>(interval_values_gradient, sizeof (interval_values_gradient) / sizeof (interval_values_gradient[0]));
}

GraphIntervalsSpeed::GraphIntervalsSpeed()
{
	this->intervals = GraphIntervalsTyped<double>(interval_values_speed, sizeof (interval_values_speed) / sizeof (interval_values_speed[0]));
}




template <class T>
T GraphIntervals2<T>::get_interval_value(int index)
{
	return this->values[index];
}



namespace SlavGPS {




template <>
GraphIntervals2<Distance>::GraphIntervals2()
{
	this->values = interval_values_distance;
	this->n_values = sizeof (interval_values_distance) / sizeof (interval_values_distance[0]);
}




template <>
GraphIntervals2<Time>::GraphIntervals2()
{
	this->values = interval_values_time;
	this->n_values = sizeof (interval_values_time) / sizeof (interval_values_time[0]);
}




template <>
GraphIntervals2<Altitude>::GraphIntervals2()
{
	this->values = interval_values_altitude2;
	this->n_values = sizeof (interval_values_altitude2) / sizeof (interval_values_altitude2[0]);
}




template <>
GraphIntervals2<Gradient>::GraphIntervals2()
{
	this->values = interval_values_gradient2;
	this->n_values = sizeof (interval_values_gradient2) / sizeof (interval_values_gradient2[0]);
}




template <>
GraphIntervals2<Speed>::GraphIntervals2()
{
	this->values = interval_values_speed2;
	this->n_values = sizeof (interval_values_speed2) / sizeof (interval_values_speed2[0]);
}




}
