/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013 Rob Norris <rw_norris@hotmail.com>
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




#include "layer_trw_track_internal.h"
#include "layer_trw_track_statistics.h"
#include "globals.h"




using namespace SlavGPS;




#define SG_MODULE "Layer TRW Track Statistics"




TrackStatistics::TrackStatistics()
{
	this->duration = Time(0); /* Set some valid initial value. */
}




/**
   Accumulate statistics from given track.

   @trk: The track, which parameters should be added to statistics.
*/
void TrackStatistics::add_track(Track * trk)
{
	qDebug() << SG_PREFIX_I << "Adding track" << trk->name;

	this->count++;

	this->trackpoints += trk->get_tp_count();;
	this->segments    += trk->get_segment_count();
	this->length      += trk->get_length();
	this->length_with_gaps += trk->get_length_including_gaps();

	const Speed incoming_track_max_speed = trk->get_max_speed();
	if (incoming_track_max_speed.get_value() > this->max_speed.get_value()) {
		this->max_speed = incoming_track_max_speed;
	}

	Altitude min_altitude;
	Altitude max_altitude;
	if (trk->get_minmax_alt(min_altitude, max_altitude)) {
		if (min_altitude.get_value() < this->min_alt.get_value()) {
			this->min_alt = min_altitude;
		}
		if (max_altitude.get_value() > this->max_alt.get_value()) {
			this->max_alt = max_altitude;
		}
	}

	Altitude delta_up;
	Altitude delta_down;
	if (trk->get_total_elevation_gain(delta_up, delta_down)) {
		this->elev_gain += delta_up;
		this->elev_loss += delta_down;
	}


	Time ts_first;
	Time ts_last;
	if (sg_ret::ok == trk->get_timestamps(ts_first, ts_last)) {

		/* Update the earliest / the latest timestamps
		   (initialize if necessary). */
		if ((!this->start_time.is_valid()) || ts_first < this->start_time) {
			this->start_time = ts_first;
		}
		if ((!this->end_time.is_valid()) || ts_last > this->end_time) {
			this->end_time = ts_last;
		}

		this->duration = this->duration + (ts_last - ts_first);
	}
}




/**
   @trk: A track or route to be included in statistics.
   @layer_is_visible: Whether layer containing given @trk is visible.
   @tracks_are_visible: Whether tracks in their containing layer are visible.
   @routes_are_visible: Whether routes in their containing layer are visible.
   @include_invisible: Whether to include invisible items in statistics

   Analyze this particular track considering whether it should be
   included depending on visibility arguments.
*/
void TrackStatistics::add_track_maybe(Track * trk, bool layer_is_visible, bool tracks_are_visible, bool routes_are_visible, bool include_invisible)
{
	if (!trk) {
		return;
	}

	if (!include_invisible) {
		/* Skip invisible layers or sublayers. */
		if (!layer_is_visible
		    || (trk->is_track() && !tracks_are_visible)
		    || (trk->is_route() && !routes_are_visible)) {

			return;
		}

		/* Skip invisible tracks. */
		if (!trk->visible) {
			return;
		}
	}

	this->add_track(trk);
}
