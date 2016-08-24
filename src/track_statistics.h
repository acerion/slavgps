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

#ifndef _SG_TRACK_STATISTICS_H_
#define _SG_TRACK_STATISTICS_H_




#include <time.h>

#include "viktrack.h"




namespace SlavGPS {




	class TrackStatistics {
	public:
		void add_track_maybe(Track * trk, bool layer_is_visible, bool tracks_are_visible, bool routes_are_visible, bool include_invisible);
		void add_track(Track * trk);

		/* Units of each item are in SI Units (as returned by
		   the appropriate internal Track functions). */

		double min_alt            = VIK_VAL_MIN_ALT;
		double max_alt            = VIK_VAL_MAX_ALT;
		double elev_gain          = 0.0;
		double elev_loss          = 0.0;
		double length             = 0.0;
		double length_gaps        = 0.0;
		double max_speed          = 0.0;
		unsigned long trackpoints = 0;
		unsigned int segments     = 0;
		int duration              = 0;
		time_t start_time         = 0;
		time_t end_time           = 0;
		int count                 = 0;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TRACK_STATISTICS_H_ */