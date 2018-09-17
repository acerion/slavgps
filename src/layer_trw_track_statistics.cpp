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

	double ms = trk->get_max_speed();
	if (ms > this->max_speed) {
		this->max_speed = ms;
	}

	double min_altitude;
	double max_altitude;
	if (trk->get_minmax_alt(&min_altitude, &max_altitude)) {
		if (min_altitude < this->min_alt) {
			this->min_alt = min_altitude;
		}
		if (max_altitude > this->max_alt) {
			this->max_alt = max_altitude;
		}
	}

	double up;
	double down;
	if (trk->get_total_elevation_gain(&up, &down)) {
		this->elev_gain += up;
		this->elev_loss += down;
	}

	if (!trk->empty()
	    && (*trk->trackpoints.begin())->timestamp) {

		time_t t1 = (*trk->trackpoints.begin())->timestamp;
		time_t t2 = (*std::prev(trk->trackpoints.end()))->timestamp;

		/* Assume never actually have a track with a time of 0 (1st Jan 1970). */
		if (this->start_time == 0) {
			this->start_time = t1;
		}
		if (this->end_time == 0) {
			this->end_time = t2;
		}

		/* Initialize to the first value. */
		if (t1 < this->start_time) {
			this->start_time = t1;
		}
		if (t2 > this->end_time) {
			this->end_time = t2;
		}

		this->duration = this->duration + (int) (t2 - t1);
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
		    || (trk->type_id == "sg.trw.track" && !tracks_are_visible)
		    || (trk->type_id == "sg.trw.route" && !routes_are_visible)) {

			return;
		}

		/* Skip invisible tracks. */
		if (!trk->visible) {
			return;
		}
	}

	this->add_track(trk);
}
