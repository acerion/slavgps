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




/*
  (Hopefully!) Human friendly altitude grid sizes - note no fixed
  'ratio' just numbers that look nice...
*/
static const AltitudeType::LL interval_values_altitude[] = {
	1.0, 2.0, 4.0, 5.0,
	10.0, 15.0, 20.0, 25.0, 40.0, 50.0, 75.0,
	100.0, 150.0, 200.0, 250.0, 375.0, 500.0, 750.0,
	1000.0, 2000.0, 5000.0,
	10000.0,
	100000.0
};


/*
  (Hopefully!) Human friendly gradient grid sizes - note no fixed
  'ratio' just numbers that look nice...

  Normally gradients should range up to couple hundred precent at
  most, however there are possibilities of having points with no
  altitude after a point with a big altitude (such as places with
  invalid DEM values in otherwise mountainous regions) - thus giving
  huge negative gradients.
*/
static const GradientType::LL interval_values_gradient[] = {
	1.0, 2.0, 3.0, 4.0, 5.0, 8.0,
	10.0, 12.0, 15.0, 20.0, 25.0, 30.0, 35.0, 40.0, 45.0, 50.0, 75.0,
	100.0, 150.0, 200.0, 250.0, 375.0, 500.0, 750.0,
	1000.0,
	10000.0,
	100000.0
};


/*
  (Hopefully!) Human friendly grid sizes - note no fixed 'ratio' just
  numbers that look nice...

  As need to cover walking speeds - have many low numbers (but also
  may go up to airplane speeds!).
*/
static const SpeedType::LL interval_values_speed[] = {
	1.0, 2.0, 3.0, 4.0, 5.0, 8.0,
	10.0, 15.0, 20.0, 25.0, 40.0, 50.0, 75.0,
	100.0, 150.0, 200.0, 250.0, 375.0, 500.0, 750.0,
	1000.0,
	10000.0
};


/*
  (Hopefully!) Human friendly distance grid sizes - note no fixed
  'ratio' just numbers that look nice...
*/
static const DistanceType::LL interval_values_distance[] = {
	0.1, 0.2, 0.5,
	1.0, 2.0, 3.0, 4.0, 5.0, 8.0,
	10.0, 15.0, 20.0, 25.0, 40.0, 50.0, 75.0,
	100.0, 150.0, 200.0, 250.0, 375.0, 500.0, 750.0,
	1000.0,
	10000.0
};


/* Time intervals in seconds. */
static const TimeType::LL interval_values_time[] = {
	60,     /* 1 minute. */
	120,    /* 2 minutes. */
	300,    /* 5 minutes. */
	900,    /* 15 minutes. */
	1800,   /* half hour. */
	3600,   /* 1 hour. */
	10800,  /* 3 hours. */
	21600,  /* 6 hours. */
	43200,  /* 12 hours. */
	86400,  /* 1 day. */
	172800, /* 2 days. */
	604800, /* 1 week. */
	1209600,/* 2 weeks. */
	2419200,/* 4 weeks. */
};




namespace SlavGPS {




/**
   @reviewed-on: 2019-08-24
*/
template <>
GraphIntervals<Distance>::GraphIntervals()
{
	const DistanceType::Unit unit = DistanceType::Unit::internal_unit();
	const size_t n_values = sizeof (interval_values_distance) / sizeof (interval_values_distance[0]);
	for (size_t i = 0; i < n_values; i++) {
		this->values.push_back(Distance(interval_values_distance[i], unit));
	}
}




/**
   @reviewed-on: 2019-08-24
*/
template <>
GraphIntervals<Time>::GraphIntervals()
{
	const TimeType::Unit unit = TimeType::Unit::internal_unit();
	const size_t n_values = sizeof (interval_values_time) / sizeof (interval_values_time[0]);
	for (size_t i = 0; i < n_values; i++) {
		this->values.push_back(Time(interval_values_time[i], unit));
	}
}




/**
   @reviewed-on: 2019-08-24
*/
template <>
GraphIntervals<Altitude>::GraphIntervals()
{
	const AltitudeType::Unit unit = AltitudeType::Unit::internal_unit();
	const size_t n_values = sizeof (interval_values_altitude) / sizeof (interval_values_altitude[0]);
	for (size_t i = 0; i < n_values; i++) {
		this->values.push_back(Altitude(interval_values_altitude[i], unit));
	}
}




/**
   @reviewed-on: 2019-08-24
*/
template <>
GraphIntervals<Gradient>::GraphIntervals()
{
	const GradientType::Unit unit = GradientType::Unit::internal_unit();
	const size_t n_values = sizeof (interval_values_gradient) / sizeof (interval_values_gradient[0]);
	for (size_t i = 0; i < n_values; i++) {
		this->values.push_back(Gradient(interval_values_gradient[i], unit));
	}
}




/**
   @reviewed-on: 2019-08-24
*/
template <>
GraphIntervals<Speed>::GraphIntervals()
{
	const SpeedType::Unit unit = SpeedType::Unit::internal_unit();
	const size_t n_values = sizeof (interval_values_speed) / sizeof (interval_values_speed[0]);
	for (size_t i = 0; i < n_values; i++) {
		this->values.push_back(Speed(interval_values_speed[i], unit));
	}
}




}
