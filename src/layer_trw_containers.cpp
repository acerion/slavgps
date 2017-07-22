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
#include <cstring>
#endif

#include "layer_trw.h"
#include "layer_trw_containers.h"
#include "viewport_internal.h"
#include "track_internal.h"
//#include "thumbnails.h"




using namespace SlavGPS;




/* This is how it knows when you click if you are clicking close to a trackpoint. */
#define TRACKPOINT_SIZE_APPROX 5
#define WAYPOINT_SIZE_APPROX 5




Track * LayerTRWc::find_track_by_date(Tracks & tracks, char const * date)
{
	char date_buf[20];
	Track * trk = NULL;

	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		date_buf[0] = '\0';
		trk = i->second;

		/* Might be an easier way to compare dates rather than converting the strings all the time... */
		if (!trk->empty()
		    && (*trk->trackpoints.begin())->has_timestamp) {

			strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", gmtime(&(*trk->trackpoints.begin())->timestamp));

			if (!g_strcmp0(date, date_buf)) {
				return trk;
			}
		}
	}
	return NULL;
}




Waypoint * LayerTRWc::find_waypoint_by_date(Waypoints & waypoints, char const * date)
{
	char date_buf[20];
	Waypoint * wp = NULL;

	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {
		date_buf[0] = '\0';
		wp = i->second;

		/* Might be an easier way to compare dates rather than converting the strings all the time... */
		if (wp->has_timestamp) {
			strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", gmtime(&(wp->timestamp)));

			if (!g_strcmp0(date, date_buf)) {
				return wp;
			}
		}
	}
	return NULL;
}




/*
 * ATM use a case sensitive find.
 * Finds the first one.
 */
Waypoint * LayerTRWc::find_waypoint_by_name(Waypoints & waypoints, const QString & wp_name)
{
	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {
		if (i->second && !i->second->name.isEmpty()) {
			if (i->second->name == wp_name) {
				return i->second;
			}
		}
	}
	return NULL;
}




/*
 * ATM use a case sensitive find.
 * Finds the first one.
 */
Track * LayerTRWc::find_track_by_name(Tracks & tracks, const QString & trk_name)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		Track * trk = i->second;
		if (trk && !trk->name.isEmpty()) {
			if (trk->name == trk_name) {
				return trk;
			}
		}
	}
	return NULL;
}




void LayerTRWc::find_maxmin_in_tracks(Tracks & tracks, struct LatLon maxmin[2])
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		LayerTRW::find_maxmin_in_track(i->second, maxmin);
	}
}




void LayerTRWc::single_waypoint_jump(Waypoints & waypoints, Viewport * viewport)
{
	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {
		/* NB do not care if wp is visible or not. */
		viewport->set_center_coord(i->second->coord, true);
	}
}




void LayerTRWc::list_wp_uids(Waypoints & waypoints, GList ** l)
{
	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {
		*l = g_list_append(*l, (void *) ((long) i->first)); /* kamilTODO: i->first or i->second? */
	}
}




void LayerTRWc::list_trk_uids(Tracks & tracks, GList ** l)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		*l = g_list_append(*l, (void *) ((long) i->first)); /* kamilTODO: i->first or i->second? */
	}
}




#if 0
Track * LayerTRWc::find_track_by_name(Tracks & input, char const * name)
{
	for (auto i = input.begin(); i != input.end(); i++) {
		if (0 == strcmp(i->second->name, name)) {
			return i->second;
		}
	}
	return NULL;
}
#endif




std::list<sg_uid_t> * LayerTRWc::find_tracks_with_timestamp_type(Tracks * tracks, bool with_timestamps, Track * exclude)
{
	std::list<sg_uid_t> * result = new std::list<sg_uid_t>;

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
				/* Don't add tracks with timestamps when getting non timestamp tracks. */
				if (p1->has_timestamp || p2->has_timestamp) {
					continue;
				}
			}
		}

		result->push_front(i->first);
	}

	return result;
}




/**
 * Called for each track in track hash table.
 * If the original track orig_trk is close enough (threshold)
 * to given track, then the given track is added to returned list.
 */
GList * LayerTRWc::find_nearby_tracks_by_time(Tracks & tracks, Track * orig_trk, unsigned int threshold)
{
	GList * nearby_tracks = NULL;

	if (!orig_trk || orig_trk->empty()) {
		return NULL;
	}

	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		Track * trk = i->second;

		/* Outline:
		 * Detect reasons for not merging, and return.
		 * If no reason is found not to merge, then do it.
		 */

		/* Exclude the original track from the compiled list. */
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
			if (! (labs(t1 - p2->timestamp) < threshold
			       /*  p1 p2      t1 t2 */
			       || labs(p1->timestamp - t2) < threshold)
			    /*  t1 t2      p1 p2 */
			    ) {
				continue;
			}
		}

		nearby_tracks = g_list_prepend(nearby_tracks, i->second);
	}

	return nearby_tracks;
}




/* c.f. get_sorted_track_name_list()
   but don't add the specified track to the list (normally current track). */
std::list<QString> LayerTRWc::get_sorted_track_name_list_exclude_self(Tracks * tracks, Track const * self)
{
	std::list<QString> result;
	for (auto i = tracks->begin(); i != tracks->end(); i++) {

		/* Skip self. */
		if (i->second == self) {
			continue;
		}
		result.push_back(i->second->name);
	}

	result.sort();

	return result;
}




/* Currently unused
static void trw_layer_sorted_name_list(void * key, void * value, void * udata)
{
	GList **list = (GList**)udata;
	// *list = g_list_prepend(*all, key); //unsorted method
	// Sort named list alphabetically
	*list = g_list_insert_sorted_with_data (*list, key, sort_alphabetically, NULL);
}
*/




std::list<QString> LayerTRWc::get_sorted_wp_name_list(Waypoints & waypoints)
{
	std::list<QString> result;
	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {
		result.push_back(i->second->name);
	}
	result.sort();

	return result;
}




std::list<QString> LayerTRWc::get_sorted_track_name_list(Tracks & tracks)
{
	std::list<QString> result;
	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		result.push_back(i->second->name);
	}

	/* Sort list of names alphabetically. */
	result.sort();

	return result;
}




/**
 * Find out if any tracks have the same name in this hash table.
 */
QString LayerTRWc::has_duplicate_track_names(Tracks & tracks)
{
	/* Build list of names. Sort list alphabetically. Find any two adjacent duplicates on the list. */

	if (tracks.size() <= 1) {
		return QString("");
	}

	std::list<QString> track_names = LayerTRWc::get_sorted_track_name_list(tracks);

	bool found = false;
	for (auto iter = std::next(track_names.begin()); iter != track_names.end(); iter++) {
		QString const this_one = *iter;
		QString const previous = *(std::prev(iter));

		if (this_one == previous) {
			return this_one;
		}
	}

	return QString("");
}




/**
 * Find out if any waypoints have the same name in this layer.
 */
QString LayerTRWc::has_duplicate_waypoint_names(Waypoints & waypoints)
{
	/* Build list of names. Sort list alphabetically. Find any two adjacent duplicates on the list. */

	if (waypoints.size() <= 1) {
		return QString("");
	}

	std::list<QString> waypoint_names = LayerTRWc::get_sorted_wp_name_list(waypoints);

	for (auto iter = std::next(waypoint_names.begin()); iter != waypoint_names.end(); iter++) {
		QString const this_one = *iter;
		QString const previous = *(std::prev(iter));

		if (this_one == previous) {
			return this_one;
		}
	}

	return QString("");
}




/**
 *
 */
void LayerTRWc::set_waypoints_visibility(Waypoints & waypoints, bool on_off)
{
	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {
		i->second->visible = on_off;
#ifdef K
		tree_view->set_visibility(i->second->index, on_off);
#endif
	}
}




void LayerTRWc::waypoints_toggle_visibility(Waypoints & waypoints)
{
	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {
		i->second->visible = !i->second->visible;
#ifdef K
		tree_view->toggle_visibility(i->second->index);
#endif
	}
}




void LayerTRWc::set_tracks_visibility(Tracks & tracks, bool on_off)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		i->second->visible = on_off;
#ifdef K
		tree_view->set_visibility(i->second->index, on_off);
#endif
	}
}




void LayerTRWc::tracks_toggle_visibility(Tracks & tracks)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		i->second->visible = !i->second->visible;
#ifdef K
		tree_view->toggle_visibility(i->second->index);
#endif
	}
}




std::list<Track *> * LayerTRWc::get_track_values(std::list<Track *> * target, Tracks & tracks)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		target->push_back(i->second);
	}

	return target;
}




void LayerTRWc::waypoint_search_closest_tp(Waypoints & waypoints, WaypointSearch * search)
{
	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {
		Waypoint * wp = i->second;
		if (!wp->visible) {
			continue;
		}

		int x, y;
		search->viewport->coord_to_screen(&wp->coord, &x, &y);

		/* If waypoint has an image then use the image size to select. */
		if (search->draw_images && wp->image) {

			int slackx = wp->image_width / 2;
			int slacky = wp->image_height / 2;

			if (x <= search->x + slackx && x >= search->x - slackx
			    && y <= search->y + slacky && y >= search->y - slacky) {

				search->closest_wp = wp;
				search->closest_x = x;
				search->closest_y = y;
			}
		} else if (abs(x - search->x) <= WAYPOINT_SIZE_APPROX && abs(y - search->y) <= WAYPOINT_SIZE_APPROX
			   && ((!search->closest_wp)        /* Was the old waypoint we already found closer than this one? */
			       || abs(x - search->x) + abs(y - search->y) < abs(x - search->closest_x) + abs(y - search->closest_y))) {

			search->closest_wp = wp;
			search->closest_x = x;
			search->closest_y = y;
		}

	}
}




void LayerTRWc::track_search_closest_tp(Tracks & tracks, TrackpointSearch * search)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {

		Track * trk = i->second;

		if (!trk->visible) {
			continue;
		}

		if (!BBOX_INTERSECT (trk->bbox, search->bbox)) {
			continue;
		}


		for (auto iter = trk->trackpoints.begin(); iter != trk->trackpoints.end(); iter++) {

			int x, y;
			search->viewport->coord_to_screen(&(*iter)->coord, &x, &y);

			if (abs(x - search->x) <= TRACKPOINT_SIZE_APPROX && abs(y - search->y) <= TRACKPOINT_SIZE_APPROX
			    && ((!search->closest_tp)        /* Was the old trackpoint we already found closer than this one? */
				|| abs(x - search->x) + abs(y - search->y) < abs(x - search->closest_x) + abs(y - search->closest_y))) {

				search->closest_track = i->second;
				search->closest_tp = *iter;
				search->closest_tp_iter = iter;
				search->closest_x = x;
				search->closest_y = y;
			}

		}
	}
}




/* Params are: viewport, event, last match found or NULL. */
char * LayerTRWc::tool_show_picture_wp(Waypoints & waypoints, int event_x, int event_y, Viewport * viewport)
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




QStringList * LayerTRWc::image_wp_make_list(Waypoints & waypoints)
{
	QStringList * pics = new QStringList;

	Waypoint * wp = NULL;
	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {
		wp = i->second;
#ifdef K
		if (!wp->image.isEmpty() && (!a_thumbnails_exists(wp->image))) {
			pics->push_back(wp->image);
		}
#endif
	}

	return pics;
}




void LayerTRWc::waypoints_convert(Waypoints & waypoints, CoordMode dest_mode)
{
	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {
		i->second->convert(dest_mode);
	}
}




void LayerTRWc::tracks_convert(Tracks & tracks, CoordMode dest_mode)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		i->second->convert(dest_mode);
	}
}
