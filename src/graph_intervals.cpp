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
static const DistanceUnit internal_unit_distance = Distance::get_internal_unit();
static const Distance interval_values_distance[] = {
	Distance(0.1, internal_unit_distance),
	Distance(0.2, internal_unit_distance),
	Distance(0.5, internal_unit_distance),
	Distance(1.0, internal_unit_distance),
	Distance(2.0, internal_unit_distance),
	Distance(3.0, internal_unit_distance),
	Distance(4.0, internal_unit_distance),
	Distance(5.0, internal_unit_distance),
	Distance(8.0, internal_unit_distance),
	Distance(10.0, internal_unit_distance),
	Distance(15.0, internal_unit_distance),
	Distance(20.0, internal_unit_distance),
	Distance(25.0, internal_unit_distance),
	Distance(40.0, internal_unit_distance),
	Distance(50.0, internal_unit_distance),
	Distance(75.0, internal_unit_distance),
	Distance(100.0, internal_unit_distance),
	Distance(150.0, internal_unit_distance),
	Distance(200.0, internal_unit_distance),
	Distance(250.0, internal_unit_distance),
	Distance(375.0, internal_unit_distance),
	Distance(500.0, internal_unit_distance),
	Distance(750.0, internal_unit_distance),
	Distance(1000.0, internal_unit_distance),
	Distance(10000.0, internal_unit_distance)
};

/* Time intervals in seconds. */
static const TimeUnit internal_unit_time = Time::get_internal_unit();
static const Time interval_values_time[] = {
	Time(60, internal_unit_time),     /* 1 minute. */
	Time(120, internal_unit_time),    /* 2 minutes. */
	Time(300, internal_unit_time),    /* 5 minutes. */
	Time(900, internal_unit_time),    /* 15 minutes. */
	Time(1800, internal_unit_time),   /* half hour. */
	Time(3600, internal_unit_time),   /* 1 hour. */
	Time(10800, internal_unit_time),  /* 3 hours. */
	Time(21600, internal_unit_time),  /* 6 hours. */
	Time(43200, internal_unit_time),  /* 12 hours. */
	Time(86400, internal_unit_time),  /* 1 day. */
	Time(172800, internal_unit_time), /* 2 days. */
	Time(604800, internal_unit_time), /* 1 week. */
	Time(1209600, internal_unit_time),/* 2 weeks. */
	Time(2419200, internal_unit_time),/* 4 weeks. */
};



/* (Hopefully!) Human friendly altitude grid sizes - note no fixed 'ratio' just numbers that look nice... */
static const HeightUnit internal_unit_altitude = Altitude::get_internal_unit();
static const Altitude interval_values_altitude2[] = {
	Altitude(1.0, internal_unit_altitude),
	Altitude(2.0, internal_unit_altitude),
	Altitude(4.0, internal_unit_altitude),
	Altitude(5.0, internal_unit_altitude),
	Altitude(10.0, internal_unit_altitude),
	Altitude(15.0, internal_unit_altitude),
	Altitude(20.0, internal_unit_altitude),
	Altitude(25.0, internal_unit_altitude),
	Altitude(40.0, internal_unit_altitude),
	Altitude(50.0, internal_unit_altitude),
	Altitude(75.0, internal_unit_altitude),
	Altitude(100.0, internal_unit_altitude),
	Altitude(150.0, internal_unit_altitude),
	Altitude(200.0, internal_unit_altitude),
	Altitude(250.0, internal_unit_altitude),
	Altitude(375.0, internal_unit_altitude),
	Altitude(500.0, internal_unit_altitude),
	Altitude(750.0, internal_unit_altitude),
	Altitude(1000.0, internal_unit_altitude),
	Altitude(2000.0, internal_unit_altitude),
	Altitude(5000.0, internal_unit_altitude),
	Altitude(10000.0, internal_unit_altitude),
	Altitude(100000.0, internal_unit_altitude)
};



/* (Hopefully!) Human friendly gradient grid sizes - note no fixed 'ratio' just numbers that look nice... */
static const GradientUnit internal_unit_gradient = Gradient::get_internal_unit();
static const Gradient interval_values_gradient2[] = {
	Gradient(1.0, internal_unit_gradient),
	Gradient(2.0, internal_unit_gradient),
	Gradient(3.0, internal_unit_gradient),
	Gradient(4.0, internal_unit_gradient),
	Gradient(5.0, internal_unit_gradient),
	Gradient(8.0, internal_unit_gradient),
	Gradient(10.0, internal_unit_gradient),
	Gradient(12.0, internal_unit_gradient),
	Gradient(15.0, internal_unit_gradient),
	Gradient(20.0, internal_unit_gradient),
	Gradient(25.0, internal_unit_gradient),
	Gradient(30.0, internal_unit_gradient),
	Gradient(35.0, internal_unit_gradient),
	Gradient(40.0, internal_unit_gradient),
	Gradient(45.0, internal_unit_gradient),
	Gradient(50.0, internal_unit_gradient),
	Gradient(75.0, internal_unit_gradient),
	Gradient(100.0, internal_unit_gradient),
	Gradient(150.0, internal_unit_gradient),
	Gradient(200.0, internal_unit_gradient),
	Gradient(250.0, internal_unit_gradient),
	Gradient(375.0, internal_unit_gradient),
	Gradient(500.0, internal_unit_gradient),
	Gradient(750.0, internal_unit_gradient),
	Gradient(1000.0, internal_unit_gradient),
	Gradient(10000.0, internal_unit_gradient),
	Gradient(100000.0, internal_unit_gradient)
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




namespace SlavGPS {




template <>
GraphIntervals2<Distance>::GraphIntervals2()
{
	this->n_values = sizeof (interval_values_distance) / sizeof (interval_values_distance[0]);

	const int size = this->n_values * sizeof (Distance);
	this->values = (Distance *) malloc(size);
	memcpy(this->values, interval_values_distance, size);
}




template <>
GraphIntervals2<Time>::GraphIntervals2()
{
	this->n_values = sizeof (interval_values_time) / sizeof (interval_values_time[0]);

	const int size = this->n_values * sizeof (Time);
	this->values = (Time *) malloc(size);
	memcpy(this->values, interval_values_time, size);
}




template <>
GraphIntervals2<Altitude>::GraphIntervals2()
{
	this->n_values = sizeof (interval_values_altitude2) / sizeof (interval_values_altitude2[0]);

	const int size = this->n_values * sizeof (Altitude);
	this->values = (Altitude *) malloc(size); /* TODO: this must be deallocated. */
	memcpy(this->values, interval_values_altitude2, size);
}




template <>
GraphIntervals2<Gradient>::GraphIntervals2()
{
	this->n_values = sizeof (interval_values_gradient2) / sizeof (interval_values_gradient2[0]);

	const int size = this->n_values * sizeof (Gradient);
	this->values = (Gradient *) malloc(size);
	memcpy(this->values, interval_values_gradient2, size);
}




template <>
GraphIntervals2<Speed>::GraphIntervals2()
{
	this->n_values = sizeof (interval_values_speed2) / sizeof (interval_values_speed2[0]);

	const int size = this->n_values * sizeof (Speed);
	this->values = (Speed *) malloc(size);
	memcpy(this->values, interval_values_speed2, size);
}




}
