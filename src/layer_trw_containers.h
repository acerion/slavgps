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
		bool found;
		const char *date_str;
		const Track * trk;
		const Waypoint * wp;
		sg_uid_t trk_uid;
		sg_uid_t wp_uid;
	} date_finder_type;




	typedef struct {
		GList ** result;
		Track * exclude;
		bool with_timestamps;
	} twt_udata;




	typedef struct {
		int x, y;
		int closest_x, closest_y;
		bool draw_images;
		sg_uid_t closest_wp_uid;
		Waypoint * closest_wp;
		Viewport * viewport;
	} WPSearchParams;




	typedef struct {
		int x, y;
		int closest_x, closest_y;
		sg_uid_t closest_track_uid;
		Trackpoint * closest_tp;
		Viewport * viewport;
		TrackPoints::iterator closest_tp_iter;
		LatLonBBox bbox;
	} TPSearchParams;




	class LayerTRWc {
	public:
		static sg_uid_t find_uid_of_track(std::unordered_map<sg_uid_t, Track *> & input, Track * trk);
		static sg_uid_t find_uid_of_waypoint_by_name(std::unordered_map<sg_uid_t, Waypoint *> & input, char const * name);
		static Track *  find_track_by_name(std::unordered_map<sg_uid_t, Track *> & input, char const * name);
		static sg_uid_t find_uid_of_waypoint(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, Waypoint * wp);
		static bool find_track_by_date(std::unordered_map<sg_uid_t, Track *> & tracks, date_finder_type * df);
		static bool find_waypoint_by_date(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, date_finder_type * df);
		static Waypoint * find_waypoint_by_name(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, const char * name);
		static void find_maxmin_in_tracks(std::unordered_map<sg_uid_t, Track *> & tracks, struct LatLon maxmin[2]);
		static void single_waypoint_jump(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, Viewport * viewport);

		static void list_wp_uids(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, GList ** l);
		static void list_trk_uids(std::unordered_map<sg_uid_t, Track *> & tracks, GList ** l);
		static void remove_item_from_treeview(std::unordered_map<sg_uid_t, TreeIndex *> & items, TreeView * tree_view);

		static std::list<sg_uid_t> * find_tracks_with_timestamp_type(std::unordered_map<sg_uid_t, Track *> * tracks, bool with_timestamps, Track * exclude);
		static GList * find_nearby_tracks_by_time(std::unordered_map<sg_uid_t, Track *> & tracks, Track * orig_trk, unsigned int threshold);
		static std::list<QString> get_sorted_track_name_list(std::unordered_map<sg_uid_t, Track *> & tracks);
		static std::list<QString> get_sorted_track_name_list_exclude_self(std::unordered_map<sg_uid_t, Track *> * tracks, Track const * self);
		static std::list<QString> get_sorted_wp_name_list(std::unordered_map<sg_uid_t, Waypoint *> & waypoints);
		static bool has_duplicate_track_names(std::unordered_map<sg_uid_t, Track *> & tracks);
		static bool has_duplicate_waypoint_names(std::unordered_map<sg_uid_t, Waypoint *> & waypoints);


		static void iter_visibility_toggle(std::unordered_map<sg_uid_t, TreeIndex *> & items, TreeView * tree_view);
		static void set_iter_visibility(std::unordered_map<sg_uid_t, TreeIndex *> & items, TreeView * tree_view, bool on_off);
		static void set_waypoints_visibility(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, bool on_off);
		static void waypoints_toggle_visibility(std::unordered_map<sg_uid_t, Waypoint *> & waypoints);
		static void set_tracks_visibility(std::unordered_map<sg_uid_t, Track *> & tracks, bool on_off);
		static void tracks_toggle_visibility(std::unordered_map<sg_uid_t, Track *> & tracks);

		static std::list<Track *> * get_track_values(std::list<Track *> * target, std::unordered_map<sg_uid_t, Track *> & tracks);
		static void waypoint_search_closest_tp(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, WPSearchParams * params);
		static void track_search_closest_tp(std::unordered_map<sg_uid_t, Track *> & tracks, TPSearchParams * params);

		static char * tool_show_picture_wp(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, int event_x, int event_y, Viewport * viewport);
		static GSList * image_wp_make_list(std::unordered_map<sg_uid_t, Waypoint *> & waypoints);


		static void waypoints_convert(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, VikCoordMode * dest_mode);
		static void track_convert(std::unordered_map<sg_uid_t, Track *> & tracks, VikCoordMode * dest_mode);
	};




} /* namespace SlavGSP */




#endif /* #ifndef _SG_LAYER_TRW_CONTAINERS_H_ */
