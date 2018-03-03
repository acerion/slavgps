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

#ifndef _SG_TRACK_H_
#define _SG_TRACK_H_




#include <list>




/* cf with vik_track_get_minmax_alt internals. */
#define VIK_VAL_MIN_ALT 25000.0
#define VIK_VAL_MAX_ALT -5000.0




namespace SlavGPS {




	class Trackpoint;
	class Track;
	class TrackPropertiesDialog;
	class TrackProfileDialog;




	enum class GPSFixMode {
		NotSeen = 0,     /* Mode update not seen yet. */
		NoFix   = 1,     /* None. */
		Fix2D   = 2,     /* Good for latitude/longitude. */
		Fix3D   = 3,     /* Good for altitude/climb too. */
		DGPS    = 4,
		PPS     = 5      /* Military signal used. */
	};




	enum class TrackDrawNameMode {
		None = 0,
		Centre,
		Start,
		End,
		StartEnd,
		StartCentreEnd,
		Max
	};




	typedef std::list<Trackpoint *> TrackPoints;
	typedef bool (* compare_trackpoints_t)(const Trackpoint * a, const Trackpoint * b);




	class Trackpoint2 {
	public:
		bool valid = false;
		TrackPoints::iterator iter;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TRACK_H_ */
