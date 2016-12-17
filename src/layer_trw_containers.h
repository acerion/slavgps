/*
 * SlavGPS -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * A fork of viking, project started in 2016.
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (c) 2011-2013, Rob Norris <rw_norris@hotmail.com>
 * Copyright (c) 2016 Kamil Ignacak <acerion@wp.pl>
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
 *
 */

#ifndef _SG_LAYER_TRW_CONTAINERS_H_
#define _SG_LAYER_TRW_CONTAINERS_H_




#include <list>

#include "waypoint.h"
#include "track.h"
#include "layer.h"




namespace SlavGPS {




	typedef std::unordered_map<sg_uid_t, Track *> Tracks;
	typedef std::unordered_map<sg_uid_t, Waypoint *> Waypoints;




	typedef struct {
		GList ** result;
		Track * exclude;
		bool with_timestamps;
	} twt_udata;




	class WaypointSearch {
	public:
		int x = 0;
		int y = 0;
		int closest_x = 0;
		int closest_y = 0;
		bool draw_images = false;
		Waypoint * closest_wp = NULL;
		Viewport * viewport = NULL;
	};




	class TrackpointSearch {
	public:
		int x = 0;
		int y = 0;
		int closest_x = 0;
		int closest_y = 0;
		Track * closest_track = NULL;
		Trackpoint * closest_tp = NULL;
		Viewport * viewport = NULL;
		TrackPoints::iterator closest_tp_iter;
		LatLonBBox bbox;
	};




	class LayerTRWc {
	public:
		static Track * find_track_by_name(Tracks & input, char const * name);
		static Waypoint * find_waypoint_by_name(Waypoints & waypoints, const char * name);
		static Track * find_track_by_date(Tracks & tracks, char const * date_str);
		static Waypoint * find_waypoint_by_date(Waypoints & waypoints, char const * date_str);

		static void find_maxmin_in_tracks(Tracks & tracks, struct LatLon maxmin[2]);
		static void single_waypoint_jump(Waypoints & waypoints, Viewport * viewport);

		static void list_wp_uids(Waypoints & waypoints, GList ** l);
		static void list_trk_uids(Tracks & tracks, GList ** l);

		static std::list<sg_uid_t> * find_tracks_with_timestamp_type(Tracks * tracks, bool with_timestamps, Track * exclude);
		static GList * find_nearby_tracks_by_time(Tracks & tracks, Track * orig_trk, unsigned int threshold);
		static std::list<QString> get_sorted_track_name_list(Tracks & tracks);
		static std::list<QString> get_sorted_track_name_list_exclude_self(Tracks * tracks, Track const * self);
		static std::list<QString> get_sorted_wp_name_list(Waypoints & waypoints);
		static QString has_duplicate_track_names(Tracks & tracks);
		static QString has_duplicate_waypoint_names(Waypoints & waypoints);


		static void set_waypoints_visibility(Waypoints & waypoints, bool on_off);
		static void waypoints_toggle_visibility(Waypoints & waypoints);
		static void set_tracks_visibility(Tracks & tracks, bool on_off);
		static void tracks_toggle_visibility(Tracks & tracks);

		static std::list<Track *> * get_track_values(std::list<Track *> * target, Tracks & tracks);
		static void waypoint_search_closest_tp(Waypoints & waypoints, WaypointSearch * search);
		static void track_search_closest_tp(Tracks & tracks, TrackpointSearch * search);

		static char * tool_show_picture_wp(Waypoints & waypoints, int event_x, int event_y, Viewport * viewport);
		static GSList * image_wp_make_list(Waypoints & waypoints);


		static void waypoints_convert(Waypoints & waypoints, VikCoordMode dest_mode);
		static void tracks_convert(Tracks & tracks, VikCoordMode dest_mode);
	};




} /* namespace SlavGSP */




#endif /* #ifndef _SG_LAYER_TRW_CONTAINERS_H_ */
