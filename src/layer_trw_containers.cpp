/*
 * SlavGPS -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Project started in 2016 by forking viking project.
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2008, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2009, Hein Ragas <viking@ragas.nl>
 * Copyright (c) 2012-2015, Rob Norris <rw_norris@hotmail.com>
 * Copyright (c) 2012-2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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





#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unordered_map>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "viktrwlayer.h"
#include "thumbnails.h"





using namespace SlavGPS;





/* this is how it knows when you click if you are clicking close to a trackpoint. */
#define TRACKPOINT_SIZE_APPROX 5
#define WAYPOINT_SIZE_APPROX 5





bool LayerTRWc::find_track_by_date(std::unordered_map<sg_uid_t, Track *> & tracks, date_finder_type * df)
{
	char date_buf[20];
	Track * trk = NULL;

	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		date_buf[0] = '\0';
		trk = i->second;

		// Might be an easier way to compare dates rather than converting the strings all the time...
		if (!trk->empty()
		    && (*trk->trackpointsB->begin())->has_timestamp) {

			strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", gmtime(&(*trk->trackpointsB->begin())->timestamp));

			if (!g_strcmp0(df->date_str, date_buf)) {
				df->found = true;
				df->trk = trk;
				df->trk_uid = i->first;
				break;
			}
		}
	}
	return df->found;
}





bool LayerTRWc::find_waypoint_by_date(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, date_finder_type * df)
{
	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {
		char date_buf[20];
		date_buf[0] = '\0';
		// Might be an easier way to compare dates rather than converting the strings all the time...
		if (i->second->has_timestamp) {
			strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", gmtime(&(i->second->timestamp)));

			if (!g_strcmp0(df->date_str, date_buf)) {
				df->found = true;
				df->wp = i->second;
				df->wp_uid = i->first;
				break;
			}
		}
	}
    return df->found;
}




/*
 * ATM use a case sensitive find
 * Finds the first one
 */
Waypoint * LayerTRWc::find_waypoint_by_name(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, const char * name)
{
	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {
		if (i->second && i->second->name) {
			if (0 == strcmp(i->second->name, name)) {
				return i->second;
			}
		}
	}
	return NULL;
}





/*
 * ATM use a case sensitive find
 * Finds the first one
 */
Track * LayerTRWc::find_track_by_name(std::unordered_map<sg_uid_t, Track *> & tracks, char const * name)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		Track * trk = i->second;
		if (trk && trk->name) {
			if (0 == strcmp(trk->name, name)) {
				return trk;
			}
		}
	}
	return NULL;
}





void LayerTRWc::find_maxmin_in_tracks(std::unordered_map<sg_uid_t, Track *> & tracks, struct LatLon maxmin[2])
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		LayerTRW::find_maxmin_in_track(i->second, maxmin);
	}
}





sg_uid_t LayerTRWc::find_uid_of_waypoint(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, Waypoint * wp)
{
	std::unordered_map<sg_uid_t, Waypoint *>::const_iterator i;
	for (i = waypoints.begin(); i != waypoints.end(); i++) {
		if (i->second == wp) {
			return i->first;
		}
	}
	return 0;
}





void LayerTRWc::single_waypoint_jump(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, Viewport * viewport)
{
	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {
		/* NB do not care if wp is visible or not */
		viewport->set_center_coord(&(i->second->coord), true);
	}
}





void LayerTRWc::list_wp_uids(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, GList ** l)
{
	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {
		*l = g_list_append(*l, (void *) ((long) i->first)); /* kamilTODO: i->first or i->second? */
	}
}





void LayerTRWc::list_trk_uids(std::unordered_map<sg_uid_t, Track *> & tracks, GList ** l)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		*l = g_list_append(*l, (void *) ((long) i->first)); /* kamilTODO: i->first or i->second? */
	}
}





sg_uid_t LayerTRWc::find_uid_of_track(std::unordered_map<sg_uid_t, Track *> & input, Track * trk)
{
	for (auto i = input.begin(); i != input.end(); i++) {
		if (i->second == trk) {
			return i->first;
		}
	}
	return 0;
}





/* kamilTODO: convert to "Waypoint * LayerTRWc::find_waypoint_by_name() */
sg_uid_t LayerTRWc::find_uid_of_waypoint_by_name(std::unordered_map<sg_uid_t, Waypoint *> & input, char const * name)
{
	for (auto i = input.begin(); i != input.end(); i++) {
		if (0 == strcmp(name, i->second->name ) ) {
			return i->first;
		}
	}
	return 0;
}




#if 0
Track * LayerTRWc::find_track_by_name(std::unordered_map<sg_uid_t, Track *> & input, char const * name)
{
	for (auto i = input.begin(); i != input.end(); i++) {
		if (0 == strcmp(i->second->name, name)) {
			return i->second;
		}
	}
	return NULL;
}
#endif





void LayerTRWc::remove_item_from_treeview(std::unordered_map<sg_uid_t, TreeIndex *> & items, TreeView * tree_view)
{
	for (auto i = items.begin(); i != items.end(); i++) {
		tree_view->erase(i->second);
	}
}





GList * LayerTRWc::find_tracks_with_timestamp_type(std::unordered_map<sg_uid_t, Track *> * tracks, bool with_timestamps, Track * exclude)
{
	GList * result = NULL;
	for (auto i = tracks->begin(); i != tracks->end(); i++) {
		Trackpoint * p1, * p2;
		Track * trk = i->second;
		if (trk == exclude) {
			continue;
		}

		if (!trk->empty()) {
			p1 = trk->get_tp_first();
			p2 = trk->get_tp_last();

			if (with_timestamps) {
				if (!p1->has_timestamp || !p2->has_timestamp) {
					continue;
				}
			} else {
				// Don't add tracks with timestamps when getting non timestamp tracks
				if (p1->has_timestamp || p2->has_timestamp) {
					continue;
				}
			}
		}

		result = g_list_prepend(result, (void *) ((long) i->first));
	}

	return result;
}





/**
 * find_nearby_tracks_by_time:
 *
 * Called for each track in track hash table.
 *  If the original track orig_trk is close enough (threshold)
 *  to given track, then the given track is added to returned list
 */
GList * LayerTRWc::find_nearby_tracks_by_time(std::unordered_map<sg_uid_t, Track *> & tracks, Track * orig_trk, unsigned int threshold)
{
	GList * nearby_tracks = NULL;

	if (!orig_trk || orig_trk->empty()) {
		return NULL;
	}

	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		Track * trk = i->second;

		/* outline:
		 * detect reasons for not merging, and return
		 * if no reason is found not to merge, then do it.
		 */

		// Exclude the original track from the compiled list
		if (trk == orig_trk) { /* kamilFIXME: originally it was "if (trk == udata->exclude)" */
			continue;
		}

		time_t t1 = orig_trk->get_tp_first()->timestamp;
		time_t t2 = orig_trk->get_tp_last()->timestamp;

		if (!trk->empty()) {

			Trackpoint * p1 = trk->get_tp_first();
			Trackpoint * p2 = trk->get_tp_last();

			if (!p1->has_timestamp || !p2->has_timestamp) {
				//fprintf(stdout, "no timestamp\n");
				continue;
			}

			//fprintf(stdout, "Got track named %s, times %d, %d\n", trk->name, p1->timestamp, p2->timestamp);
			if (! (labs(t1 - p2->timestamp) < threshold ||
			       /*  p1 p2      t1 t2 */
			       labs(p1->timestamp - t2) < threshold)
			    /*  t1 t2      p1 p2 */
			    ) {
				continue;
			}
		}

		nearby_tracks = g_list_prepend(nearby_tracks, i->second);
	}

	return nearby_tracks;
}





// c.f. trw_layer_sorted_track_id_by_name_list
//  but don't add the specified track to the list (normally current track)
void LayerTRWc::sorted_track_id_by_name_list_exclude_self(std::unordered_map<sg_uid_t, Track *> * tracks, twt_udata * user_data)
{
	for (auto i = tracks->begin(); i != tracks->end(); i++) {

		// Skip self
		if (i->second == user_data->exclude) {
			continue;
		}

		// Sort named list alphabetically
		*(user_data->result) = g_list_insert_sorted_with_data (*(user_data->result), i->second->name, sort_alphabetically, NULL);
	}
}





/**
 * Similar to trw_layer_enum_item, but this uses a sorted method
 */
/* Currently unused
static void trw_layer_sorted_name_list(void * key, void * value, void * udata)
{
  GList **list = (GList**)udata;
  // *list = g_list_prepend(*all, key); //unsorted method
  // Sort named list alphabetically
  *list = g_list_insert_sorted_with_data (*list, key, sort_alphabetically, NULL);
}
*/





/**
 * Now Waypoint specific sort
 */
void LayerTRWc::sorted_wp_id_by_name_list(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, GList **list)
{
	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {
		// Sort named list alphabetically
		*list = g_list_insert_sorted_with_data(*list, i->second->name, sort_alphabetically, NULL);
	}

	return;
}





/**
 * Track specific sort
 */
GList * LayerTRWc::sorted_track_id_by_name_list(std::unordered_map<sg_uid_t, Track *> & tracks)
{
	GList * result = NULL;
	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		// Sort named list alphabetically
		result = g_list_insert_sorted_with_data(result, i->second->name, sort_alphabetically, NULL);
	}

	return result;
}





/**
 * Find out if any tracks have the same name in this hash table
 */
bool LayerTRWc::has_same_track_names(std::unordered_map<sg_uid_t, Track *> & ht_tracks)
{
	// Sort items by name, then compare if any next to each other are the same

	GList * track_names = LayerTRWc::sorted_track_id_by_name_list(ht_tracks);

	// No tracks
	if (!track_names) {
		return false;
	}

	same_track_name_udata udata;
	udata.has_same_track_name = false;

	// Use sort routine to traverse list comparing items
	// Don't care how this list ends up ordered ( doesn't actually change ) - care about the returned status
	GList * dummy_list = g_list_sort_with_data(track_names, check_tracks_for_same_name, &udata);
	// Still no tracks...
	if (!dummy_list) {
		return false;
	}

	return udata.has_same_track_name;
}





/**
 *
 */
void LayerTRWc::iter_visibility_toggle(std::unordered_map<sg_uid_t, TreeIndex *> & items, TreeView * tree_view)
{
	for (auto i = items.begin(); i != items.end(); i++) {
		tree_view->toggle_visibility(i->second);
	}
}





/**
 *
 */
void LayerTRWc::set_iter_visibility(std::unordered_map<sg_uid_t, TreeIndex *> & items, TreeView * tree_view, bool on_off)
{
	for (auto i = items.begin(); i != items.end(); i++) {
		tree_view->set_visibility(i->second, on_off);
	}
}





/**
 *
 */
void LayerTRWc::set_waypoints_visibility(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, bool on_off)
{
	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {
		i->second->visible = on_off;
	}
}





/**
 *
 */
void LayerTRWc::waypoints_toggle_visibility(std::unordered_map<sg_uid_t, Waypoint *> & waypoints)
{
	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {
		i->second->visible = !i->second->visible;
	}
}





/**
 *
 */
void LayerTRWc::set_tracks_visibility(std::unordered_map<sg_uid_t, Track *> & tracks, bool on_off)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		i->second->visible = on_off;
	}
}





/**
 *
 */
void LayerTRWc::tracks_toggle_visibility(std::unordered_map<sg_uid_t, Track *> & tracks)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		i->second->visible = !i->second->visible;
	}
}





std::list<Track *> * LayerTRWc::get_track_values(std::list<Track *> * target, std::unordered_map<sg_uid_t, Track *> & tracks)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		target->push_back(i->second);
	}

	return target;
}




void LayerTRWc::waypoint_search_closest_tp(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, WPSearchParams * params)
{
	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {
		Waypoint * wp = i->second;
		if (!wp->visible) {
			continue;
		}

		int x, y;
		params->viewport->coord_to_screen(&(wp->coord), &x, &y);

		// If waypoint has an image then use the image size to select
		if (params->draw_images && wp->image) {

			int slackx, slacky;

			slackx = wp->image_width / 2;
			slacky = wp->image_height / 2;

			if (x <= params->x + slackx && x >= params->x - slackx
			    && y <= params->y + slacky && y >= params->y - slacky) {

				params->closest_wp_uid = i->first;
				params->closest_wp = wp;
				params->closest_x = x;
				params->closest_y = y;
			}
		} else if (abs(x - params->x) <= WAYPOINT_SIZE_APPROX && abs(y - params->y) <= WAYPOINT_SIZE_APPROX &&
			   ((!params->closest_wp) ||        /* was the old waypoint we already found closer than this one? */
			    abs(x - params->x) + abs(y - params->y) < abs(x - params->closest_x) + abs(y - params->closest_y)))	{

			params->closest_wp_uid = i->first;
			params->closest_wp = wp;
			params->closest_x = x;
			params->closest_y = y;
		}

	}
}





void LayerTRWc::track_search_closest_tp(std::unordered_map<sg_uid_t, Track *> & tracks, TPSearchParams * params)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {

		Track * trk = i->second;

		if (!trk->visible) {
			continue;
		}

		if (!BBOX_INTERSECT (trk->bbox, params->bbox)) {
			continue;
		}


		for (auto iter = trk->trackpointsB->begin(); iter != trk->trackpointsB->end(); iter++) {

			int x, y;
			params->viewport->coord_to_screen(&(*iter)->coord, &x, &y);

			if (abs(x - params->x) <= TRACKPOINT_SIZE_APPROX && abs(y - params->y) <= TRACKPOINT_SIZE_APPROX
			    && ((!params->closest_tp)        /* was the old trackpoint we already found closer than this one? */
				|| abs(x - params->x) + abs(y - params->y) < abs(x - params->closest_x) + abs(y - params->closest_y))) {

				params->closest_track_uid = (sg_uid_t) ((long) i->first);
				params->closest_tp = *iter;
				params->closest_tp_iter = iter;
				params->closest_x = x;
				params->closest_y = y;
			}

		}
	}
}





/* Params are: viewport, event, last match found or NULL */
char * LayerTRWc::tool_show_picture_wp(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, int event_x, int event_y, Viewport * viewport)
{
	char * found = NULL;

	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {

		Waypoint * wp = i->second;
		if (wp->image && wp->visible) {
			int x, y;
			viewport->coord_to_screen(&wp->coord, &x, &y);
			int slackx = wp->image_width / 2;
			int slacky = wp->image_height / 2;
			if (x <= event_x + slackx && x >= event_x - slackx
			    && y <= event_y + slacky && y >= event_y - slacky) {

				found = wp->image; /* We've found a match. however continue searching
						      since we want to find the last match -- that
						      is, the match that was drawn last. */
			}
		}

	}

	return found;
}





GSList * LayerTRWc::image_wp_make_list(std::unordered_map<sg_uid_t, Waypoint *> & waypoints)
{
	GSList * pics = NULL;

	Waypoint * wp = NULL;
	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {
		wp = i->second;
		if (wp->image && (!a_thumbnails_exists(wp->image))) {
			pics = g_slist_append(pics, (void *) g_strdup(wp->image));
		}
	}

	return pics;
}





void LayerTRWc::waypoints_convert(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, VikCoordMode * dest_mode)
{
	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {
		vik_coord_convert(&(i->second->coord), *dest_mode);
	}
}





void LayerTRWc::track_convert(std::unordered_map<sg_uid_t, Track *> & tracks, VikCoordMode * dest_mode)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		i->second->convert(*dest_mode);
	}
}
