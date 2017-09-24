/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (c) 2012, Rob Norris <rw_norris@hotmail.com>
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

#include <algorithm>
#include <cstdlib>
#include <cassert>
#include <mutex>
#include <cstring>
#include <cmath>

#include <QDebug>

#include <glib.h>
#include <time.h>

#include "coords.h"
#include "coord.h"
#include "track_internal.h"
#include "dem_cache.h"
#include "settings.h"
#include "util.h"

#include "track_profile_dialog.h"
#include "track_properties_dialog.h"
#include "layer_trw_menu.h"
#include "layer_trw.h"
#include "window.h"




using namespace SlavGPS;




extern bool g_have_astro_program;
extern bool g_have_diary_program;




/* Simple UID implementation using an integer. */
static sg_uid_t global_trk_uid = SG_UID_INITIAL;
static sg_uid_t global_rt_uid = SG_UID_INITIAL;
static std::mutex trk_uid_mutex;
static std::mutex rt_uid_mutex;




#define VIK_SETTINGS_TRACK_NAME_MODE "track_draw_name_mode"
#define VIK_SETTINGS_TRACK_NUM_DIST_LABELS "track_number_dist_labels"




/**
 * Set some default values for a track.
 * ATM This uses the 'settings' method to get values,
 * so there is no GUI way to control these yet...
 */
void Track::set_defaults()
{
	int tmp;
	if (a_settings_get_integer(VIK_SETTINGS_TRACK_NAME_MODE, &tmp)) {
		this->draw_name_mode = (TrackDrawNameMode) tmp;
	}

	if (a_settings_get_integer(VIK_SETTINGS_TRACK_NUM_DIST_LABELS, &tmp)) {
		max_number_dist_labels = (uint8_t) (TrackDrawNameMode) tmp; /* TODO: why such cast? */
	}
}




void Track::set_name(const QString & new_name)
{
	this->name = new_name;
}




void Track::set_comment(const QString & new_comment)
{
	this->comment = new_comment;
}




void Track::set_description(const QString & new_description)
{
	this->description = new_description;
}




void Track::set_source(const QString & new_source)
{
	this->source = new_source;
}




void Track::set_type(const QString & new_type)
{
	this->type = new_type;
}




void Track::ref()
{
	ref_count++;
}




void Track::set_properties_dialog(TrackPropertiesDialog * dialog)
{
	this->properties_dialog = dialog;
}




void Track::clear_properties_dialog()
{
	this->properties_dialog = NULL;
}




/**
   @brief Update track properties dialog e.g. if the track has been renamed
*/
void Track::update_properties_dialog(void)
{
	/* If not displayed do nothing. */
	if (!this->properties_dialog) {
		return;
	}

	/* Update title with current name. */
	if (!this->name.isEmpty()) {
		this->properties_dialog->setWindowTitle(tr("%1 - Track Properties").arg(this->name));
	}
}




void Track::set_profile_dialog(TrackProfileDialog * dialog)
{
	this->profile_dialog = dialog;
}




void Track::clear_profile_dialog()
{
	this->profile_dialog = NULL;
}




/**
   @brief Update track profile dialog e.g. if the track has been renamed
*/
void Track::update_profile_dialog(void)
{
	/* If not displayed do nothing. */
	if (!this->profile_dialog) {
		return;
	}

	/* Update title with current name. */
	if (!this->name.isEmpty()) {
		this->profile_dialog->setWindowTitle(tr("%1 - Track Profile").arg(this->name));
	}
}




void Track::free()
{
	if (ref_count-- > 1) {
		return;
	}

	delete this;
}




Track::Track(bool is_route)
{
	this->tree_item_type = TreeItemType::SUBLAYER;

	if (is_route) {
		this->type_id = "sg.trw.route";

		rt_uid_mutex.lock();
		this->uid = ++global_rt_uid;
		rt_uid_mutex.unlock();
	} else {
		this->type_id = "sg.trw.track";

		trk_uid_mutex.lock();
		this->uid = ++global_trk_uid;
		trk_uid_mutex.unlock();
	}

	memset(&bbox, 0, sizeof (LatLonBBox));

	ref_count = 1;
}




/**
 * @from: The Track to copy
 * @copy_points: Whether to copy the track points or not
 *
 * Normally for copying the track it's best to copy all the trackpoints.
 * However for some operations such as splitting tracks the trackpoints will be managed separately, so no need to copy them.
 *
 * Returns: the copied Track.
 */
Track::Track(const Track & from) : Track(from.type_id == "sg.trw.route")
{
	this->tree_item_type = TreeItemType::SUBLAYER;

	/* Copy points. */
	for (auto iter = from.trackpoints.begin(); iter != from.trackpoints.end(); iter++) {
		Trackpoint * new_tp = new Trackpoint(**iter);
		this->trackpoints.push_back(new_tp);
	}

	//this->name = g_strdup(from.name); /* kamilFIXME: in original code initialization of name is duplicated. */
	this->visible = from.visible;
	this->draw_name_mode = from.draw_name_mode;
	this->max_number_dist_labels = from.max_number_dist_labels;

	this->set_name(from.name);
	this->set_comment(from.comment);
	this->set_description(from.description);
	this->set_source(from.source);
	/* kamilFIXME: where is ->type? */

	this->has_color = from.has_color;
	this->color = from.color;
	this->bbox = from.bbox;
}




/* kamilFIXME: parent constructor first copies all trackpoints from 'from', this constructor does ->assign(). Copying in parent constructor is unnecessary. */
Track::Track(const Track & from, TrackPoints::iterator first, TrackPoints::iterator last) : Track(from)
{
	this->tree_item_type = TreeItemType::SUBLAYER;

	this->trackpoints.assign(first, last);
}




Track::~Track()
{
	for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {
		delete *iter;
	}
	this->trackpoints.clear();
}




Trackpoint::Trackpoint(const Trackpoint & tp)
{
	this->name = tp.name;

	memcpy((void *) &(this->coord), (void *) &(tp.coord), sizeof (Coord)); /* kamilTODO: review this copy of coord */
	this->newsegment = tp.newsegment;
	this->has_timestamp = tp.has_timestamp;
	this->timestamp = tp.timestamp;
	this->altitude = tp.altitude;
	this->speed = tp.speed;
	this->course = tp.course;
	this->nsats = tp.nsats;

	this->fix_mode = tp.fix_mode;
	this->hdop = tp.hdop;
	this->vdop = tp.vdop;
	this->pdop = tp.pdop;

}




Trackpoint::Trackpoint(Trackpoint const& tp_a, Trackpoint const& tp_b, CoordMode coord_mode)
{
	struct LatLon ll_a = tp_a.coord.get_latlon();
	struct LatLon ll_b = tp_b.coord.get_latlon();

	/* Main positional interpolation. */
	struct LatLon ll_new = { (ll_a.lat + ll_b.lat) / 2, (ll_a.lon + ll_b.lon) / 2 };
	this->coord = Coord(ll_new, coord_mode);

	/* Now other properties that can be interpolated. */
	this->altitude = (tp_a.altitude + tp_b.altitude) / 2;

	if (tp_a.has_timestamp && tp_b.has_timestamp) {
		/* Note here the division is applied to each part, then added
		   This is to avoid potential overflow issues with a 32 time_t for
		   dates after midpoint of this Unix time on 2004/01/04. */
		this->timestamp = (tp_a.timestamp / 2) + (tp_b.timestamp / 2);
		this->has_timestamp = true;
	}

	if (tp_a.speed != NAN && tp_b.speed != NAN) {
		this->speed = (tp_a.speed + tp_b.speed) / 2;
	}

	/* TODO - improve interpolation of course, as it may not be correct.
	   if courses in degrees are 350 + 020, the mid course more likely to be 005 (not 185)
	   [similar applies if value is in radians] */
	if (tp_a.course != NAN && tp_b.course != NAN) {
		this->course = (tp_a.course + tp_b.course) / 2;
	}

	/* DOP / sat values remain at defaults as not
	   they do not seem applicable to a dreamt up point. */
}




void Trackpoint::set_name(const QString & new_name)
{
	this->name = new_name;
}




/**
 * @trk:   The track to consider the recalculation on.
 *
 * A faster bounds check, since it only considers the last track point.
 */
void Track::recalculate_bounds_last_tp()
{
	if (this->trackpoints.empty()) {
		return;
	}

	Trackpoint * tp = *std::prev(this->trackpoints.end());
	if (tp) {
		/* See if this trackpoint increases the track bounds and update if so. */
		struct LatLon ll = tp->coord.get_latlon();
		if (ll.lat > bbox.north) {
			bbox.north = ll.lat;
		}

		if (ll.lon < bbox.west) {
			bbox.west = ll.lon;
		}

		if (ll.lat < bbox.south) {
			bbox.south = ll.lat;
		}

		if (ll.lon > bbox.east) {
			bbox.east = ll.lon;
		}
	}
}




/**
 * @tr:          The track to which the trackpoint will be added
 * @tp:          The trackpoint to add
 * @recalculate: Whether to perform any associated properties recalculations
 *               Generally one should avoid recalculation via this method if adding lots of points
 *               (But ensure calculate_bounds() is called after adding all points!!)
 *
 * The trackpoint is added to the end of the existing trackpoint list.
 */
void Track::add_trackpoint(Trackpoint * tp, bool recalculate)
{
	/* When it's the first trackpoint need to ensure the bounding box is initialized correctly. */
	bool adding_first_point = this->trackpoints.empty();
	this->trackpoints.push_back(tp);
	if (adding_first_point) {
		this->calculate_bounds();
	} else if (recalculate) {
		this->recalculate_bounds_last_tp();
	}
}




double Track::get_length_to_trackpoint(const Trackpoint * tp) const
{
	double len = 0.0;
	if (this->trackpoints.empty()) {
		return len;
	}

	auto iter = this->trackpoints.begin();
	/* Is it the very first track point? */
	if (*iter == tp) {
		return len;
	}
	iter++;

	for (; iter != this->trackpoints.end(); iter++) {
		Trackpoint * tp1 = *iter;
		if (!tp1->newsegment) {
			len += Coord::distance(tp1->coord, (*std::prev(iter))->coord);
		}

		/* Exit when we reach the desired point. */
		if (tp1 == tp) {
			break;
		}
	}
	return len;
}




/* Get total length along a track. */
double Track::get_length() const
{
	double len = 0.0;
	if (this->trackpoints.empty()) {
		return len;
	}

	for (auto iter = std::next(this->trackpoints.begin()); iter != this->trackpoints.end(); iter++) {
		Trackpoint * tp1 = *iter;
		if (!tp1->newsegment) {
			len += Coord::distance(tp1->coord, (*std::prev(iter))->coord);
		}
	}
	return len;
}




/* Get total length along a track. */
double Track::get_length_including_gaps() const
{
	double len = 0.0;
	if (this->trackpoints.empty()) {
		return len;
	}

	for (auto iter = std::next(this->trackpoints.begin()); iter != this->trackpoints.end(); iter++) {
		len += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);
	}
	return len;
}




unsigned long Track::get_tp_count() const
{
	return this->trackpoints.size();
}




unsigned long Track::get_dup_point_count() const
{
	unsigned long num = 0;

	for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {
		if (std::next(iter) != this->trackpoints.end()
		    && (*iter)->coord == (*std::next(iter))->coord) {
			num++;
		}
	}

	return num;
}




/*
 * Deletes adjacent points that have the same position.
 * Returns the number of points that were deleted.
 */
unsigned long Track::remove_dup_points()
{
	unsigned long num = 0;
	auto iter = this->trackpoints.begin();
	while (iter != this->trackpoints.end()) {

		if (std::next(iter) != this->trackpoints.end()
		    && (*iter)->coord == (*std::next(iter))->coord) {

			num++;
			/* Maintain track segments. */
			if ((*std::next(iter))->newsegment
			    && std::next(std::next(iter)) != this->trackpoints.end()) {

				(*std::next(std::next(iter)))->newsegment = true;
			}

			delete *std::next(iter);
			this->trackpoints.erase(std::next(iter));
		} else {
			iter++;
		}
	}

	/* NB isn't really be necessary as removing duplicate points shouldn't alter the bounds! */
	this->calculate_bounds();

	return num;
}




/*
 * Get a count of trackpoints with the same defined timestamp.
 * Note is using timestamps with a resolution with 1 second.
 */
unsigned long Track::get_same_time_point_count() const
{
	unsigned long num = 0;

	for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {
		if (std::next(iter) != this->trackpoints.end()
		    && ((*iter)->has_timestamp && (*std::next(iter))->has_timestamp)
		    && ((*iter)->timestamp == (*std::next(iter))->timestamp)) {

			num++;
		}
	}
	return num;
}




/*
 * Deletes adjacent points that have the same defined timestamp.
 * Returns the number of points that were deleted.
 */
unsigned long Track::remove_same_time_points()
{
	unsigned long num = 0;
	auto iter = this->trackpoints.begin();
	while (iter != this->trackpoints.end()) {
		if (std::next(iter) != this->trackpoints.end()
		    && ((*iter)->has_timestamp && (*std::next(iter))->has_timestamp)
		    && ((*iter)->timestamp == (*std::next(iter))->timestamp)) {

			num++;

			/* Maintain track segments. */
			if ((*std::next(iter))->newsegment
			    && std::next(std::next(iter)) != this->trackpoints.end()) {

				(*std::next(std::next(iter)))->newsegment = true;
			}

			delete *std::next(iter);
			this->trackpoints.erase(std::next(iter));
		} else {
			iter++;
		}
	}

	this->calculate_bounds();

	return num;
}




/*
 * Deletes all 'extra' trackpoint information
 * such as time stamps, speed, course etc...
 */
void Track::to_routepoints(void)
{
	for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {

		/* c.f. with vik_trackpoint_new(). */

		(*iter)->has_timestamp = false;
		(*iter)->timestamp = 0;
		(*iter)->speed = NAN;
		(*iter)->course = NAN;
		(*iter)->hdop = VIK_DEFAULT_DOP;
		(*iter)->vdop = VIK_DEFAULT_DOP;
		(*iter)->pdop = VIK_DEFAULT_DOP;
		(*iter)->nsats = 0;
		(*iter)->fix_mode = GPSFixMode::NOT_SEEN;
	}

}




unsigned int Track::get_segment_count() const
{
	unsigned int num = 0;

	if (this->trackpoints.empty()) {
		return num;
	}

	for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {
		if ((*iter)->newsegment) {
			num++;
		}
	}

	return num;
}




/* kamilTODO: revisit this function and compare with original. */
std::list<Track *> * Track::split_into_segments()
{
	unsigned int segs = this->get_segment_count();
	if (segs < 2) {
		return NULL;
	}

	std::list<Track *> * tracks = new std::list<Track *>;
	for (auto first = this->trackpoints.begin(); first != this->trackpoints.end(); ) {
		if ((*first)->newsegment) {

			auto last = next(first);
			while (last != this->trackpoints.end()
			       && !(*last)->newsegment) {

				last++;
			}

			/* kamilFIXME: first constructor of new_track copies all trackpoints from *this, and then we do new_track->assign(). Copying in constructor is unnecessary. */
			Track * new_track = new Track(*this);
			new_track->trackpoints.assign(first, last);
			new_track->calculate_bounds();
			tracks->push_back(new_track);

			/* First will now point at either ->end() or beginning of next segment. */
			first = last;
		} else {
			/* I think that this branch of if/else will never be executed
			   because 'first' will either point at ->begin() at the very
			   beginning of the loop, or will always be moved to start of
			   next segment with 'first = last' assignment in first branch
			   of this if/else. */
			first++;
		}

	}
	return tracks;
}




/*
 * Simply remove any subsequent segment markers in a track to form one continuous track.
 * Return the number of segments merged.
 */
unsigned int Track::merge_segments(void)
{
	if (this->trackpoints.empty()) {
		return 0;
	}

	unsigned int num = 0;

	/* Always skip the first point as this should be the first segment. */

	for (auto iter = std::next(this->trackpoints.begin()); iter != this->trackpoints.end(); iter++) {
		if ((*iter)->newsegment) {
			(*iter)->newsegment = false;
			num++;
		}
	}

	return num;
}




void Track::reverse(void)
{
	if (this->trackpoints.empty()) {
		return;
	}

	this->trackpoints.reverse();

	/* Fix 'newsegment' flags. */

	auto iter = this->trackpoints.end();
	iter--;

	/* Last point that was previously a first one and had newsegment flag set. Last point should have this flag cleared. */
	(*iter)->newsegment;

	while (--iter != this->trackpoints.begin()) {
		if ((*iter)->newsegment && std::next(iter) != this->trackpoints.end()) {
			(*std::next(iter))->newsegment = true;
			(*iter)->newsegment = false;
		}
	}

	assert (iter == this->trackpoints.begin());
	/* First segment by convention has newsegment flag set. */
	(*iter)->newsegment = true;

	return;
}




/**
 * @segment_gaps: Whether the duration should include gaps between segments.
 *
 * Returns: The time in seconds.
 * NB this may be negative particularly if the track has been reversed.
 */
time_t Track::get_duration(bool segment_gaps) const
{
	if (this->trackpoints.empty()) {
		return 0;
	}

	time_t duration = 0;

	/* Ensure times are available. */
	if (this->get_tp_first()->has_timestamp) {
		/* Get trkpt only once - as using vik_track_get_tp_last() iterates whole track each time. */
		if (segment_gaps) {
			// Simple duration
			Trackpoint * tp_last = this->get_tp_last();
			if (tp_last->has_timestamp) {
				time_t t1 = this->get_tp_first()->timestamp;
				time_t t2 = tp_last->timestamp;
				duration = t2 - t1;
			}
		} else {
			/* Total within segments. */
			for (auto iter = std::next(this->trackpoints.begin()); iter != this->trackpoints.end(); iter++) {
				if ((*iter)->has_timestamp
				    && (*std::prev(iter))->has_timestamp
				    && !(*iter)->newsegment) {

					duration += ABS ((*iter)->timestamp - (*std::prev(iter))->timestamp);
				}
			}
		}
	}

	return duration;
}




/* Code extracted from make_speed_map() and similar functions. */
double Track::get_duration() const
{
	if (this->trackpoints.empty()) {
		return 0.0;
	}

	time_t t1 = (*this->trackpoints.begin())->timestamp;
	time_t t2 = (*std::prev(this->trackpoints.end()))->timestamp;
	double duration = t2 - t1;

	if (!t1 || !t2 || !duration) {
		return 0.0;
	}

	if (duration < 0) {
		fprintf(stderr, "WARNING: negative duration: unsorted trackpoint timestamps?\n");
		return 0.0;
	}

	return duration;
}




double Track::get_average_speed() const
{
	if (this->trackpoints.empty()) {
		return 0.0;
	}

	double len = 0.0;
	uint32_t time = 0;

	for (auto iter = std::next(this->trackpoints.begin()); iter != this->trackpoints.end(); iter++) {

		if ((*iter)->has_timestamp
		    && (*std::prev(iter))->has_timestamp
		    && !(*iter)->newsegment) {

			len += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);
			time += ABS ((*iter)->timestamp - (*std::prev(iter))->timestamp);
		}
	}

	return (time == 0) ? 0 : ABS(len/time);
}




/**
 * Based on a simple average speed, but with a twist - to give a moving average.
 *  . GPSs often report a moving average in their statistics output
 *  . bicycle speedos often don't factor in time when stopped - hence reporting a moving average for speed
 *
 * Often GPS track will record every second but not when stationary.
 * This method doesn't use samples that differ over the specified time limit - effectively skipping that time chunk from the total time.
 *
 * Suggest to use 60 seconds as the stop length (as the default used in the TrackWaypoint draw stops factor).
 */
double Track::get_average_speed_moving(int stop_length_seconds) const
{
	if (this->trackpoints.empty()) {
		return 0.0;
	}

	double len = 0.0;
	uint32_t time = 0;

	for (auto iter = std::next(this->trackpoints.begin()); iter != this->trackpoints.end(); iter++) {
		if ((*iter)->has_timestamp
		    && (*std::prev(iter))->has_timestamp
		    && !(*iter)->newsegment) {

			if (((*iter)->timestamp - (*std::prev(iter))->timestamp) < stop_length_seconds) {
				len += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);

				time += ABS ((*iter)->timestamp - (*std::prev(iter))->timestamp);
			}
		}
	}

	return (time == 0) ? 0 : ABS(len / time);
}




double Track::get_max_speed() const
{
	if (this->trackpoints.empty()) {
		return 0.0;
	}

	double maxspeed = 0.0;
	double speed = 0.0;

	for (auto iter = std::next(this->trackpoints.begin()); iter != this->trackpoints.end(); iter++) {
		if ((*iter)->has_timestamp
		    && (*std::prev(iter))->has_timestamp
		    && (!(*iter)->newsegment)) {

			speed = Coord::distance((*iter)->coord, (*std::prev(iter))->coord)
				/ ABS ((*iter)->timestamp - (*std::prev(iter))->timestamp);

			if (speed > maxspeed) {
				maxspeed = speed;
			}
		}
	}

	return maxspeed;
}




void Track::convert(CoordMode dest_mode)
{
	for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {
		(*iter)->coord.change_mode(dest_mode);
	}
}




/* I understood this when I wrote it ... maybe ... Basically it eats up the
 * proper amounts of length on the track and averages elevation over that. */
double * Track::make_elevation_map(uint16_t num_chunks) const
{
	assert (num_chunks < 16000);
	if (this->trackpoints.size() < 2) {
		return NULL;
	}

	{ /* Test if there's anything worth calculating. */

		bool okay = false;
		for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {
			/* Sometimes a GPS device (or indeed any random file) can have stupid numbers for elevations.
			   Since when is 9.9999e+24 a valid elevation!!
			   This can happen when a track (with no elevations) is uploaded to a GPS device and then redownloaded (e.g. using a Garmin Legend EtrexHCx).
			   Some protection against trying to work with crazily massive numbers (otherwise get SIGFPE, Arithmetic exception) */

			if ((*iter)->altitude != VIK_DEFAULT_ALTITUDE
			    && (*iter)->altitude < 1E9) {

				okay = true;
				break;
			}
		}
		if (!okay) {
			return NULL;
		}
	}

	double * pts = (double *) malloc(sizeof (double) * num_chunks);

	double total_length = this->get_length_including_gaps();
	double chunk_length = total_length / num_chunks;

	/* Zero chunk_length (eg, track of 2 tp with the same loc) will cause crash */
	if (chunk_length <= 0) {
		std::free(pts);
		return NULL;
	}

	double current_dist = 0.0;
	double current_area_under_curve = 0;
	uint16_t current_chunk = 0;

	auto iter = this->trackpoints.begin();
	double current_seg_length = Coord::distance((*iter)->coord, (*std::next(iter))->coord);

	double altitude1 = (*iter)->altitude;
	double altitude2 = (*std::next(iter))->altitude;
	double dist_along_seg = 0;

	bool ignore_it = false;
	while (current_chunk < num_chunks) {

		/* Go along current seg. */
		if (current_seg_length && (current_seg_length - dist_along_seg) > chunk_length) {
			dist_along_seg += chunk_length;

			/*        /
			 *   pt2 *
			 *      /x       altitude = alt_at_pt_1 + alt_at_pt_2 / 2 = altitude1 + slope * dist_value_of_pt_inbetween_pt1_and_pt2
			 *     /xx   avg altitude = area under curve / chunk len
			 *pt1 *xxx   avg altitude = altitude1 + (altitude2-altitude1)/(current_seg_length)*(dist_along_seg + (chunk_len/2))
			 *   / xxx
			 *  /  xxx
			 **/

			if (ignore_it) {
				/* Seemly can't determine average for this section - so use last known good value (much better than just sticking in zero). */
				pts[current_chunk] = altitude1;
			} else {
				pts[current_chunk] = altitude1 + (altitude2 - altitude1) * ((dist_along_seg - (chunk_length / 2)) / current_seg_length);
			}

			current_chunk++;
		} else {
			/* Finish current seg. */
			if (current_seg_length) {
				double altitude_at_dist_along_seg = altitude1 + (altitude2 - altitude1) / (current_seg_length) * dist_along_seg;
				current_dist = current_seg_length - dist_along_seg;
				current_area_under_curve = current_dist * (altitude_at_dist_along_seg + altitude2) * 0.5;
			} else {
				current_dist = current_area_under_curve = 0;  /* Should only happen if first current_seg_length == 0. */
			}
			/* Get intervening segs. */
			iter++;
			while (iter != this->trackpoints.end()
			       && std::next(iter) != this->trackpoints.end()) {

				current_seg_length = Coord::distance((*iter)->coord, (*std::next(iter))->coord);
				altitude1 = (*iter)->altitude;
				altitude2 = (*std::next(iter))->altitude;
				ignore_it = (*std::next(iter))->newsegment;

				if (chunk_length - current_dist >= current_seg_length) {
					current_dist += current_seg_length;
					current_area_under_curve += current_seg_length * (altitude1 + altitude2) * 0.5;
					iter++;
				} else {
					break;
				}
			}

			/* Final seg. */
			dist_along_seg = chunk_length - current_dist;
			if (ignore_it
			    || (iter != this->trackpoints.end()
				&& std::next(iter) == this->trackpoints.end())) {

				pts[current_chunk] = current_area_under_curve / current_dist;
				if (std::next(iter) == this->trackpoints.end()) {
					for (int i = current_chunk + 1; i < num_chunks; i++) {
						pts[i] = pts[current_chunk];
					}
					break;
				}
			} else {
				current_area_under_curve += dist_along_seg * (altitude1 + (altitude2 - altitude1) * dist_along_seg / current_seg_length);
				pts[current_chunk] = current_area_under_curve / chunk_length;
			}

			current_dist = 0;
			current_chunk++;
		}
	}

	return pts;
}




bool Track::get_total_elevation_gain(double * up, double * down) const
{
	if (this->trackpoints.empty()) {
		return false;
	}

	auto iter = this->trackpoints.begin();

	if ((*iter)->altitude == VIK_DEFAULT_ALTITUDE) {
		*up = VIK_DEFAULT_ALTITUDE;
		*down = VIK_DEFAULT_ALTITUDE;
	} else {
		*up = 0;
		*down = 0;

		iter++;

		for (; iter != this->trackpoints.end(); iter++) {
			double diff = (*iter)->altitude - (*std::prev(iter))->altitude;
			if (diff > 0) {
				*up += diff;
			} else {
				*down -= diff;
			}
		}
	}

	return true;
}




double * Track::make_gradient_map(const uint16_t num_chunks) const
{
	assert (num_chunks < 16000);

	double total_length = this->get_length_including_gaps();
	double chunk_length = total_length / num_chunks;

	/* Zero chunk_length (eg, track of 2 tp with the same loc) will cause crash. */
	if (chunk_length <= 0) {
		return NULL;
	}

	double * altitudes = this->make_elevation_map(num_chunks);
	if (altitudes == NULL) {
		return NULL;
	}

	double current_gradient = 0.0;
	double * pts = (double *) malloc(sizeof (double) * num_chunks);
	double altitude1, altitude2;
	uint16_t current_chunk;
	for (current_chunk = 0; current_chunk < (num_chunks - 1); current_chunk++) {
		altitude1 = altitudes[current_chunk];
		altitude2 = altitudes[current_chunk + 1];
		current_gradient = 100.0 * (altitude2 - altitude1) / chunk_length;

		pts[current_chunk] = current_gradient;
	}

	pts[current_chunk] = current_gradient;

	std::free(altitudes);

	return pts;
}




/* By Alex Foobarian. */
double * Track::make_speed_map(const uint16_t num_chunks) const
{
	assert (num_chunks < 16000);

	double duration = this->get_duration();
	if (duration < 0) {
		return NULL;
	}

	double chunk_size = duration / num_chunks;
	int pt_count = this->get_tp_count();

	double * out = (double *) malloc(sizeof (double) * num_chunks);
	double * s = (double *) malloc(sizeof (double) * pt_count);
	double * t = (double *) malloc(sizeof (double) * pt_count);


	int numpts = 0;
	auto iter = this->trackpoints.begin();
	s[numpts] = 0;
	t[numpts] = (*iter)->timestamp;
	numpts++;
	iter++;
	while (iter != this->trackpoints.end()) {
		s[numpts] = s[numpts - 1] + Coord::distance((*std::prev(iter))->coord, (*iter)->coord);
		t[numpts] = (*iter)->timestamp;
		numpts++;
		iter++;
	}

	/* In the following computation, we iterate through periods of time of duration chunk_size.
	   The first period begins at the beginning of the track.  The last period ends at the end of the track. */
	int tp_index = 0; /* index of the current trackpoint. */
	for (int i = 0; i < num_chunks; i++) {
		/* We are now covering the interval from t[0] + i * chunk_size to t[0] + (i + 1) * chunk_size.
		   Find the first trackpoint outside the current interval, averaging the speeds between intermediate trackpoints. */
		if (t[0] + i * chunk_size >= t[tp_index]) {
			double acc_t = 0;
			double acc_s = 0;
			while (t[0] + i * chunk_size >= t[tp_index]) {
				acc_s += (s[tp_index+1] - s[tp_index]);
				acc_t += (t[tp_index+1] - t[tp_index]);
				tp_index++;
			}
			out[i] = acc_s / acc_t;
		} else if (i) {
			out[i] = out[i - 1];
		} else {
			out[i] = 0;
		}
	}
	std::free(s);
	std::free(t);
	return out;
}




/**
 * Make a distance/time map, heavily based on the vik_track_make_speed_map method.
 */
double * Track::make_distance_map(const uint16_t num_chunks) const
{
	double duration = this->get_duration();
	if (duration < 0) {
		return NULL;
	}

	double chunk_size = duration / num_chunks;
	int pt_count = this->get_tp_count();

	double * out = (double *) malloc(sizeof (double) * num_chunks);
	double * s = (double *) malloc(sizeof (double) * pt_count);
	double * t = (double *) malloc(sizeof (double) * pt_count);


	int numpts = 0;
	auto iter = this->trackpoints.begin();
	s[numpts] = 0;
	t[numpts] = (*iter)->timestamp;
	numpts++;
	iter++;
	while (iter != this->trackpoints.end()) {
		s[numpts] = s[numpts - 1] + Coord::distance((*std::prev(iter))->coord, (*iter)->coord);
		t[numpts] = (*iter)->timestamp;
		numpts++;
		iter++;
	}

	/* In the following computation, we iterate through periods of time of duration chunk_size.
	   The first period begins at the beginning of the track.  The last period ends at the end of the track. */
	int tp_index = 0; /* index of the current trackpoint. */
	for (int i = 0; i < num_chunks; i++) {
		/* We are now covering the interval from t[0] + i * chunk_size to t[0] + (i + 1) * chunk_size.
		   find the first trackpoint outside the current interval, averaging the distance between intermediate trackpoints. */
		if (t[0] + i * chunk_size >= t[tp_index]) {
			double acc_s = 0;
			/* No need for acc_t. */
			while (t[0] + i * chunk_size >= t[tp_index]) {
				acc_s += (s[tp_index + 1] - s[tp_index]);
				tp_index++;
			}
			/* The only bit that's really different from the speed map - just keep an accululative record distance. */
			out[i] = i ? out[i - 1] + acc_s : acc_s;
		} else if (i) {
			out[i] = out[i - 1];
		} else {
			out[i] = 0;
		}
	}
	std::free(s);
	std::free(t);
	return out;
}




/**
 * This uses the 'time' based method to make the graph, (which is a simpler compared to the elevation/distance).
 * This results in a slightly blocky graph when it does not have many trackpoints: <60.
 * NB Somehow the elevation/distance applies some kind of smoothing algorithm,
 * but I don't think any one understands it any more (I certainly don't ATM).
 */
double * Track::make_elevation_time_map(const uint16_t num_chunks) const
{
	if (this->trackpoints.size() < 2) {
		return NULL;
	}

	/* Test if there's anything worth calculating. */
	bool okay = false;
	for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {
		if ((*iter)->altitude != VIK_DEFAULT_ALTITUDE) {
			okay = true;
			break;
		}
	}
	if (!okay) {
		return NULL;
	}

	double duration = this->get_duration();
	if (duration < 0) {
		return NULL;
	}

	double chunk_size = duration / num_chunks;
	int pt_count = this->get_tp_count();

	double * out = (double *) malloc(sizeof (double) * num_chunks); /* The return altitude values. */
	double * s = (double *) malloc(sizeof (double) * pt_count); // calculation altitudes
	double * t = (double *) malloc(sizeof (double) * pt_count); // calculation times


	int numpts = 0;
	auto iter = this->trackpoints.begin();
	s[numpts] = (*iter)->altitude;
	t[numpts] = (*iter)->timestamp;
	numpts++;
	iter++;
	while (iter != this->trackpoints.end()) {
		s[numpts] = (*iter)->altitude;
		t[numpts] = (*iter)->timestamp;
		numpts++;
		iter++;
	}

	/* In the following computation, we iterate through periods of time of duration chunk_size.
	   The first period begins at the beginning of the track.  The last period ends at the end of the track. */
	int tp_index = 0; /* index of the current trackpoint. */
	for (int i = 0; i < num_chunks; i++) {
		/* We are now covering the interval from t[0] + i * chunk_size to t[0] + (i + 1) * chunk_size.
		   find the first trackpoint outside the current interval, averaging the heights between intermediate trackpoints. */
		if (t[0] + i * chunk_size >= t[tp_index]) {
			double acc_s = s[tp_index]; /* Initialise to first point. */

			while (t[0] + i * chunk_size >= t[tp_index]) {
				acc_s += (s[tp_index + 1] - s[tp_index]);
				tp_index++;
			}

			out[i] = acc_s;
		} else if (i) {
			out[i] = out[i - 1];
		} else {
			out[i] = 0;
		}
	}
	std::free(s);
	std::free(t);
	return out;
}




/**
 * Make a speed/distance map.
 */
double * Track::make_speed_dist_map(const uint16_t num_chunks) const
{
	double total_length = this->get_length_including_gaps();
	if (total_length <= 0) {
		return NULL;
	}

	double chunk_size = total_length / num_chunks;
	int pt_count = this->get_tp_count();

	double * out = (double *) malloc(sizeof (double) * num_chunks);
	double * s = (double *) malloc(sizeof (double) * pt_count);
	double * t = (double *) malloc(sizeof (double) * pt_count);

	/* No special handling of segments ATM... */
	int numpts = 0;
	auto iter = this->trackpoints.begin();
	s[numpts] = 0;
	t[numpts] = (*iter)->timestamp;
	numpts++;
	iter++;
	while (iter != this->trackpoints.end()) {
		s[numpts] = s[numpts - 1] + Coord::distance((*std::prev(iter))->coord, (*iter)->coord);
		t[numpts] = (*iter)->timestamp;
		numpts++;
		iter++;
	}

	/* Iterate through a portion of the track to get an average speed for that part.
	   This will essentially interpolate between segments, which I think is right given the usage of 'get_length_including_gaps'. */
	int tp_index = 0; /* Index of the current trackpoint. */
	for (int i = 0; i < num_chunks; i++) {
		/* Similar to the make_speed_map, but instead of using a time chunk, use a distance chunk. */
		if (s[0] + i * chunk_size >= s[tp_index]) {
			double acc_t = 0;
			double acc_s = 0;
			while (s[0] + i * chunk_size >= s[tp_index]) {
				acc_s += (s[tp_index + 1] - s[tp_index]);
				acc_t += (t[tp_index + 1] - t[tp_index]);
				tp_index++;
			}
			out[i] = acc_s / acc_t;
		} else if (i) {
			out[i] = out[i - 1];
		} else {
			out[i] = 0;
		}
	}
	std::free(s);
	std::free(t);
	return out;
}



/**
 * @trk:                  The Track on which to find a Trackpoint
 * @meters_from_start:    The distance along a track that the trackpoint returned is near
 * @get_next_point:       Since there is a choice of trackpoints, this determines which one to return
 * @tp_metres_from_start: For the returned Trackpoint, returns the distance along the track
 *
 * TODO: Consider changing the boolean get_next_point into an enum with these options PREVIOUS, NEXT, NEAREST
 *
 * Returns: The #Trackpoint fitting the criteria or NULL.
 */
Trackpoint * Track::get_tp_by_dist(double meters_from_start, bool get_next_point, double *tp_metres_from_start)
{
	if (this->trackpoints.empty()) {
		return NULL;
	}

	double current_dist = 0.0;
	double current_inc = 0.0;
	if (tp_metres_from_start) {
		*tp_metres_from_start = 0.0;
	}

	auto iter = std::next(this->trackpoints.begin());
	while (iter != this->trackpoints.end()) {
		current_inc = Coord::distance((*iter)->coord, (*std::prev(iter))->coord);
		current_dist += current_inc;
		if (current_dist >= meters_from_start) {
			break;
		}
		iter++;
	}
	/* Passed the end of the track? */
	if (iter == this->trackpoints.end()) {
		return NULL;
	}

	if (tp_metres_from_start) {
		*tp_metres_from_start = current_dist;
	}

	/* We've gone past the distance already, is the previous trackpoint wanted? */
	if (!get_next_point) {
		if (iter != this->trackpoints.begin()) {
			if (tp_metres_from_start) {
				*tp_metres_from_start = current_dist - current_inc;
			}
			return *std::prev(iter);
		}
	}
	return *iter;
}




/* By Alex Foobarian. */
Trackpoint * Track::get_closest_tp_by_percentage_dist(double reldist, double *meters_from_start)
{
	if (this->trackpoints.empty()) {
		return NULL;
	}

	double dist = this->get_length_including_gaps() * reldist;
	double current_dist = 0.0;
	double current_inc = 0.0;

	TrackPoints::iterator last_iter = this->trackpoints.end();
	double last_dist = 0.0;

	auto iter = std::next(this->trackpoints.begin());
	for (; iter != this->trackpoints.end(); iter++) {
		current_inc = Coord::distance((*iter)->coord, (*std::prev(iter))->coord);
		last_dist = current_dist;
		current_dist += current_inc;
		if (current_dist >= dist) {
			break;
		}
		last_iter = iter;
	}

	if (iter == this->trackpoints.end()) { /* passing the end the track */
		if (last_iter != this->trackpoints.end()) {
			if (meters_from_start) {
				*meters_from_start = last_dist;
			}
			return *last_iter;
		} else {
			return NULL;
		}
	}

	/* We've gone past the dist already, was prev trackpoint closer? */
	/* Should do a vik_coord_average_weighted() thingy. */
	if (iter != this->trackpoints.begin()
	    && fabs(current_dist-current_inc-dist) < fabs(current_dist - dist)) {
		if (meters_from_start) {
			*meters_from_start = last_dist;
		}
		iter--;
	} else {
		if (meters_from_start) {
			*meters_from_start = current_dist;
		}
	}

	return *iter;
}




Trackpoint * Track::get_closest_tp_by_percentage_time(double reltime, time_t *seconds_from_start)
{
	if (this->trackpoints.empty()) {
		return NULL;
	}

	time_t t_start = (*this->trackpoints.begin())->timestamp;
	time_t t_end = (*std::prev(this->trackpoints.end()))->timestamp;
	time_t t_total = t_end - t_start;
	time_t t_pos = t_start + t_total * reltime;

	auto iter = this->trackpoints.begin();
	while (iter != this->trackpoints.end()) {
		if ((*iter)->timestamp == t_pos) {
			break;
		}

		if ((*iter)->timestamp > t_pos) {
			if (iter == this->trackpoints.begin()) { /* First trackpoint. */
				break;
			}
			time_t t_before = t_pos - (*std::prev(iter))->timestamp;
			time_t t_after = (*iter)->timestamp - t_pos;
			if (t_before <= t_after) {
				iter--;
			}
			break;
		} else if (std::next(iter) == this->trackpoints.end()
			   && (t_pos < ((*iter)->timestamp + 3))) { /* Last trackpoint: accommodate for round-off. */
			break;
		} else {
			;
		}
		iter++;
	}

	if (iter == this->trackpoints.end()) {
		return NULL;
	}

	if (seconds_from_start) {
		*seconds_from_start = (*iter)->timestamp - (*this->trackpoints.begin())->timestamp;
	}

	return *iter;
}




Trackpoint * Track::get_tp_by_max_speed() const
{
	if (this->trackpoints.empty()) {
		return NULL;
	}

	Trackpoint * max_speed_tp = NULL;
	double maxspeed = 0.0;
	double speed = 0.0;

	for (auto iter = std::next(this->trackpoints.begin()); iter != this->trackpoints.end(); iter++) {
		if ((*iter)->has_timestamp
		    && (*std::prev(iter))->has_timestamp
		    && !(*iter)->newsegment) {

			speed = Coord::distance((*iter)->coord, (*std::prev(iter))->coord)
				/ ABS ((*iter)->timestamp - (*std::prev(iter))->timestamp);

			if (speed > maxspeed) {
				maxspeed = speed;
				max_speed_tp = *iter;
			}
		}
	}

	if (!max_speed_tp) {
		return NULL;
	}

	return max_speed_tp;
}




Trackpoint * Track::get_tp_by_max_alt() const
{
	if (this->trackpoints.empty()) {
		return NULL;
	}

	Trackpoint * max_alt_tp = NULL;
	double max_alt = VIK_VAL_MAX_ALT;

	for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {
		if ((*iter)->altitude > max_alt) {
			max_alt = (*iter)->altitude;
			max_alt_tp = *iter;
		}
	}

	if (!max_alt_tp) {
		return NULL;
	}

	return max_alt_tp;
}




Trackpoint * Track::get_tp_by_min_alt() const
{
	if (this->trackpoints.empty()) {
		return NULL;
	}

	Trackpoint * min_alt_tp = NULL;
	double minalt = VIK_VAL_MIN_ALT;

	for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {
		if ((*iter)->altitude < minalt) {
			minalt = (*iter)->altitude;
			min_alt_tp = *iter;
		}
	}

	if (!min_alt_tp) {
		return NULL;
	}

	return min_alt_tp;
}




Trackpoint * Track::get_tp_first() const
{
	if (this->trackpoints.empty()) {
		return NULL;
	}

	return *this->trackpoints.begin();
}




Trackpoint * Track::get_tp_last() const
{
	if (this->trackpoints.empty()) {
		return NULL;
	}

	return *std::prev(this->trackpoints.end());
}




Trackpoint * Track::get_tp_prev(Trackpoint * tp) const
{
	if (this->trackpoints.empty()) {
		return NULL;
	}

	Trackpoint * tp_prev = NULL;

	for (auto iter = std::prev(this->trackpoints.begin()); iter != this->trackpoints.end(); iter++) {
		if (*iter == tp) {
			tp_prev = *std::prev(iter);
			break;
		}
	}

	return tp_prev;
}




bool Track::get_minmax_alt(double * min_alt, double * max_alt) const
{
	*min_alt = VIK_VAL_MIN_ALT;
	*max_alt = VIK_VAL_MAX_ALT;

	if (this->trackpoints.empty()) {
		return false;
	}

	auto iter = this->trackpoints.begin();
	if ((*iter)->altitude == VIK_DEFAULT_ALTITUDE) {
		return false;
	}
	iter++;

	double tmp_alt;
	for (; iter != this->trackpoints.end(); iter++) {
		tmp_alt = (*iter)->altitude;
		if (tmp_alt > *max_alt) {
			*max_alt = tmp_alt;
		}

		if (tmp_alt < *min_alt) {
			*min_alt = tmp_alt;
		}
	}
	return true;
}




void Track::marshall(uint8_t **data, size_t *datalen)
{
	GByteArray * b = g_byte_array_new();
	g_byte_array_append(b, (uint8_t *) this, sizeof(Track));

	/* we'll fill out number of trackpoints later */
	unsigned int intp = b->len;
	unsigned int len;
	g_byte_array_append(b, (uint8_t *)&len, sizeof(len));


	/* This allocates space for variant sized strings
	   and copies that amount of data from the track to byte array. */
#define vtm_append(s)	       \
	len = (s) ? strlen(s)+1 : 0;			\
	g_byte_array_append(b, (uint8_t *) &len, sizeof(len));	\
	if (s) g_byte_array_append(b, (uint8_t *) s, len);

	auto iter = this->trackpoints.begin();
	unsigned int ntp = 0;
	while (iter != this->trackpoints.end()) {
		g_byte_array_append(b, (uint8_t *) *iter, sizeof (Trackpoint));
		vtm_append((*iter)->name.toUtf8().constData());
		iter++;
		ntp++;
	}
	*(unsigned int *)(b->data + intp) = ntp;

	vtm_append(this->name.toUtf8().constData());
	vtm_append(this->comment.toUtf8().constData());
	vtm_append(this->description.toUtf8().constData());
	vtm_append(this->source.toUtf8().constData());
	/* kamilTODO: where is ->type? */

	*data = b->data;
	*datalen = b->len;
	g_byte_array_free(b, false);
}




/*
 * Take a byte array and convert it into a Track.
 */
Track * Track::unmarshall(uint8_t *data, size_t datalen)
{
	Track * new_trk = new Track(((Track *)data)->type_id == "sg.trw.route");
	/* Basic properties: */
	new_trk->visible = ((Track *)data)->visible;
	new_trk->draw_name_mode = ((Track *)data)->draw_name_mode;
	new_trk->max_number_dist_labels = ((Track *)data)->max_number_dist_labels;
	new_trk->has_color = ((Track *)data)->has_color;
	new_trk->color = ((Track *)data)->color;
	new_trk->bbox = ((Track *)data)->bbox;

	data += sizeof(*new_trk);

	unsigned int ntp = *(unsigned int *) data;
	data += sizeof(ntp);

	unsigned int len;

#define vtu_get(s)	       \
	len = *(unsigned int *)data;		\
	data += sizeof(len);			\
	if (len) {				\
		(s) = g_strdup((char *)data);	\
	} else {				\
		(s) = NULL;			\
	}					\
	data += len;

#ifdef K
	Trackpoint * new_tp;
	for (unsigned int i = 0; i < ntp; i++) {
		new_tp = new Trackpoint();
		memcpy(new_tp, data, sizeof(*new_tp));
		data += sizeof(*new_tp);
		vtu_get(new_tp->name_.toUtf8().constData());
		new_trk->trackpoints.push_back(new_tp);
	}
	vtu_get(new_trk->name.toUtf8().constData());
	vtu_get(new_trk->comment.toUtf8().constData());
	vtu_get(new_trk->description);
	vtu_get(new_trk->source);
	/* kamilTODO: where is ->type? */
#endif

	return new_trk;
}




/**
 * (Re)Calculate the bounds of the given track,
 * updating the track's bounds data.
 * This should be called whenever a track's trackpoints are changed.
 */
void Track::calculate_bounds()
{
	struct LatLon topleft, bottomright;

	/* Set bounds to first point. */
	auto iter = this->trackpoints.begin();
	if (iter != this->trackpoints.end()) {
		topleft = (*iter)->coord.get_latlon();
		bottomright = (*iter)->coord.get_latlon();
	}

	for (; iter != this->trackpoints.end(); iter++) {

		/* See if this trackpoint increases the track bounds. */

		struct LatLon ll = (*iter)->coord.get_latlon();

		if (ll.lat > topleft.lat) {
			topleft.lat = ll.lat;
		}

		if (ll.lon < topleft.lon) {
			topleft.lon = ll.lon;
		}

		if (ll.lat < bottomright.lat) {
			bottomright.lat = ll.lat;
		}

		if (ll.lon > bottomright.lon) {
			bottomright.lon = ll.lon;
		}
	}

	qDebug() << QString("DD: Track: Bounds of track: '%1' is: %2,%3 to: %4,%5").arg(this->name).arg(topleft.lat).arg(topleft.lon).arg(bottomright.lat).arg(bottomright.lon);

	bbox.north = topleft.lat;
	bbox.east = bottomright.lon;
	bbox.south = bottomright.lat;
	bbox.west = topleft.lon;

}




/**
 * Shift all timestamps to be relatively offset from 1901-01-01.
 */
void Track::anonymize_times()
{
	if (this->trackpoints.empty()) {
		return;
	}

	GTimeVal gtv;
	/* Check result just to please Coverity - even though it shouldn't fail as it's a hard coded value here! */
	if (!g_time_val_from_iso8601 ("1901-01-01T00:00:00Z", &gtv)) {
		fprintf(stderr, "CRITICAL: Calendar time value failure\n");
		return;
	}

	time_t anon_timestamp = gtv.tv_sec;
	time_t offset = 0;

	for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {
		Trackpoint * tp = *iter;
		if (tp->has_timestamp) {
			/* Calculate an offset in time using the first available timestamp. */
			if (offset == 0) {
				offset = tp->timestamp - anon_timestamp;
			}

			/* Apply this offset to shift all timestamps towards 1901 & hence anonymising the time.
			   Note that the relative difference between timestamps is kept - thus calculating speeds will still work. */
			tp->timestamp = tp->timestamp - offset;
		}
	}
}




/**
 * Interpolate the timestamps between first and last trackpoint,
 * so that the track is driven at equal speed, regardless of the
 * distance between individual trackpoints.
 *
 * NB This will overwrite any existing trackpoint timestamps.
 */
void Track::interpolate_times()
{
	if (this->trackpoints.empty()) {
		return;
	}

	auto iter = this->trackpoints.begin();
	Trackpoint * tp = *iter;
	if (!tp->has_timestamp) {
		return;
	}

	time_t tsfirst = tp->timestamp;

	/* Find the end of the track and the last timestamp. */
	iter = prev(this->trackpoints.end());

	tp = *iter;
	if (tp->has_timestamp) {
		time_t tsdiff = tp->timestamp - tsfirst;

		double tr_dist = this->get_length_including_gaps();
		double cur_dist = 0.0;

		if (tr_dist > 0) {
			iter = this->trackpoints.begin();
			/* Apply the calculated timestamp to all trackpoints except the first and last ones. */
			while (std::next(iter) != this->trackpoints.end()
			       && std::next(std::next(iter)) != this->trackpoints.end()) {

				iter++;
				cur_dist += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);

				(*iter)->timestamp = (cur_dist / tr_dist) * tsdiff + tsfirst;
				(*iter)->has_timestamp = true;
			}
			/* Some points may now have the same time so remove them. */
			this->remove_same_time_points();
		}
	}
}




/**
 * @skip_existing: When true, don't change the elevation if the trackpoint already has a value.
 *
 * Set elevation data for a track using any available DEM information.
 */
unsigned long Track::apply_dem_data(bool skip_existing)
{
	unsigned long num = 0;

	for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {
		/* Don't apply if the point already has a value and the overwrite is off. */
		if (!(skip_existing && (*iter)->altitude != VIK_DEFAULT_ALTITUDE)) {
			/* TODO: of the 4 possible choices we have for choosing an
			   elevation (trackpoint in between samples), choose the one
			   with the least elevation change as the last. */
			int16_t elev = DEMCache::get_elev_by_coord(&(*iter)->coord, DemInterpolation::BEST);
			if (elev != DEM_INVALID_ELEVATION) {
				(*iter)->altitude = elev;
				num++;
			}
		}
	}

	return num;
}




/**
 * Apply DEM data (if available) - to only the last trackpoint.
 */
void Track::apply_dem_data_last_trackpoint()
{
	if (this->trackpoints.empty()) {
		return;
	}

	/* As in vik_track_apply_dem_data above - use 'best' interpolation method. */
	auto last = std::prev(this->trackpoints.end());
	int16_t elev = DEMCache::get_elev_by_coord(&(*last)->coord, DemInterpolation::BEST);
	if (elev != DEM_INVALID_ELEVATION) {
		(*last)->altitude = elev;
	}
}




/**
 * Apply elevation smoothing over range of trackpoints between the list start and end points.
 */
void Track::smoothie(TrackPoints::iterator start, TrackPoints::iterator stop, double elev1, double elev2, unsigned int points)
{
	/* If was really clever could try and weigh interpolation according to the distance between trackpoints somehow.
	   Instead a simple average interpolation for the number of points given. */
	double change = (elev2 - elev1) / (points + 1);
	int count = 1;
	auto iter = start;
	while (iter != stop) {
		Trackpoint * tp = *iter;

		tp->altitude = elev1 + (change * count);

		count++;
		iter++;
	}
}




/**
 * @flat: Specify how the missing elevations will be set.
 *        When true it uses a simple flat method, using the last known elevation
 *        When false is uses an interpolation method to the next known elevation
 *
 * For each point with a missing elevation, set it to use the last known available elevation value.
 * Primarily of use for smallish DEM holes where it is missing elevation data.
 * Eg see Austria: around N47.3 & E13.8.
 *
 * Returns: The number of points that were adjusted.
 */
unsigned long Track::smooth_missing_elevation_data(bool flat)
{
	unsigned long num = 0;
	double elev = VIK_DEFAULT_ALTITUDE;

	Trackpoint * tp_missing = NULL;
	auto iter_first = this->trackpoints.end();;
	unsigned int points = 0;

	for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {
		Trackpoint * tp = *iter;

		if (VIK_DEFAULT_ALTITUDE == tp->altitude) {
			if (flat) {
				/* Simply assign to last known value. */
				if (elev != VIK_DEFAULT_ALTITUDE) {
					tp->altitude = elev;
					num++;
				}
			} else {
				if (!tp_missing) {
					/* Remember the first trackpoint (and the list pointer to it) of a section of no altitudes. */
					tp_missing = tp;
					iter_first = iter;
					points = 1;
				} else {
					/* More missing altitudes. */
					points++;
				}
			}
		} else {
			/* Altitude available (maybe again!).
			   If this marks the end of a section of altitude-less points
			   then apply smoothing for that section of points. */
			if (points > 0 && elev != VIK_DEFAULT_ALTITUDE) {
				if (!flat && iter_first != this->trackpoints.end()) {
					this->smoothie(iter_first, iter, elev, tp->altitude, points);
					num = num + points;
				}
			}

			/* Reset. */
			points = 0;
			tp_missing = NULL;

			/* Store for reuse as the last known good value. */
			elev = tp->altitude;
		}
	}

	return num;
}




/**
 * Appends 'from' to track, leaving 'from' with no trackpoints.
 */
void Track::steal_and_append_trackpoints(Track * from)
{
	this->trackpoints.insert(this->trackpoints.end(), from->trackpoints.begin(), from->trackpoints.end());
	from->trackpoints.clear();

	/* Trackpoints updated - so update the bounds. */
	this->calculate_bounds();
}




/**
 * Starting at the end, looks backwards for the last "double point", a duplicate trackpoint.
 * If there is no double point, deletes all the trackpoints.
 *
 * Returns: the new end of the track (or the start if there are no double points).
 */
Coord * Track::cut_back_to_double_point()
{
	if (this->trackpoints.empty()) {
		return NULL;
	}

	auto iter = std::prev(this->trackpoints.end());

	Coord * rv = new Coord();

	while (iter != this->trackpoints.begin()) {
		Coord * cur_coord = &(*iter)->coord;
		Coord * prev_coord = &(*std::prev(iter))->coord;

		if (*cur_coord == *prev_coord) {

			*rv = *cur_coord;

			/* Truncate trackpoint list from double point to the end. */
			for (auto here = iter; here != this->trackpoints.end(); here++) {
				delete *here;
			}
			this->trackpoints.erase(iter, this->trackpoints.end());

			return rv;
		}
		iter--;
	}

	/* No double point found! */

	*rv = (*this->trackpoints.begin())->coord;

	for (auto iter2 = this->trackpoints.begin(); iter2 != this->trackpoints.end(); iter2++) {
		delete *iter2;
	}
	this->trackpoints.erase(iter, this->trackpoints.end());
	this->trackpoints.clear();

	return rv;
}




/**
 * Function to compare two tracks by their first timestamp.
 **/
int Track::compare_timestamp(const void * x, const void * y)
{
	Track * a = (Track *) x;
	Track * b = (Track *) y;

	Trackpoint * tpa = NULL;
	Trackpoint * tpb = NULL;

	if (!a->trackpoints.empty()) {
		tpa = *a->trackpoints.begin();
	}

	if (!b->trackpoints.empty()) {
		tpb = *b->trackpoints.begin();
	}

	if (tpa && tpb) {
		if (tpa->timestamp < tpb->timestamp) {
			return -1;
		}
		if (tpa->timestamp > tpb->timestamp) {
			return 1;
		}
	}

	if (tpa && !tpb) {
		return 1;
	}

	if (!tpa && tpb) {
		return -1;
	}

	return 0;
}




TrackPoints::iterator Track::begin()
{
	return this->trackpoints.begin();
}




TrackPoints::iterator Track::end()
{
	return this->trackpoints.end();
}




bool Track::empty()
{
	return this->trackpoints.empty();
}




void Track::push_front(Trackpoint * tp)
{
	this->trackpoints.push_front(tp);
}




TrackPoints::iterator Track::erase(TrackPoints::iterator first, TrackPoints::iterator last)
{
	return this->trackpoints.erase(first, last);
}




void Track::sort(compare_trackpoints_t compare_func)
{
	this->trackpoints.sort(compare_func);
}




TrackPoints::iterator Track::delete_trackpoint(TrackPoints::iterator iter)
{
	TrackPoints::iterator new_iter;

	if ((new_iter = std::next(iter)) != this->trackpoints.end()
	    || (new_iter = std::next(iter)) != this->trackpoints.end()) {

		if ((*iter)->newsegment
		    && std::next(iter) != this->trackpoints.end()) {

			(*std::next(iter))->newsegment = true; /* don't concat segments on del */
		}

		/* Delete current trackpoint. */
		this->erase_trackpoint(iter);
		return new_iter;
	} else {
		/* Delete current trackpoint. */
		this->erase_trackpoint(iter);
		return this->trackpoints.end();
	}
}




TrackPoints::iterator Track::erase_trackpoint(TrackPoints::iterator iter)
{
	delete *iter;
	return this->trackpoints.erase(iter);
}




void Track::insert(Trackpoint * tp_at, Trackpoint * tp_new, bool before)
{
	TrackPoints::iterator iter = std::find(this->trackpoints.begin(), this->trackpoints.end(), tp_at);
	if (iter == this->trackpoints.end()) {
		/* kamilTODO: report some error. */
		return;
	}

	if (iter == std::prev(this->trackpoints.end()) && !before) {
		return;
	}

	if (iter == this->trackpoints.begin() && !before) {
		iter--;
	}

	this->trackpoints.insert(iter, tp_new);
	return;
}




/* kamilFIXME: this assumes that trackpoints is non-NULL and has some elements. */
TrackPoints::iterator Track::get_last()
{
	return std::prev(this->trackpoints.end());
}




std::list<Rect *> * Track::get_rectangles(LatLon * wh)
{
	std::list<Rect *> * rectangles = new std::list<Rect *>;

	bool new_map = true;
	Coord tl, br;
	auto iter = this->trackpoints.begin();
	while (iter != this->trackpoints.end()) {
		Coord * cur_coord = &(*iter)->coord;
		if (new_map) {
			cur_coord->set_area(wh, &tl, &br);
			Rect * rect = (Rect *) malloc(sizeof (Rect));
			rect->tl = tl;
			rect->br = br;
			rect->center = *cur_coord;
			rectangles->push_front(rect);
			new_map = false;
			iter++;
			continue;
		}
		bool found = false;
		for (auto rect_iter = rectangles->begin(); rect_iter != rectangles->end(); rect_iter++) {
			if (cur_coord->is_inside(&(*rect_iter)->tl, &(*rect_iter)->br)) {
				found = true;
				break;
			}
		}
		if (found) {
			iter++;
		} else {
			new_map = true;
		}
	}

	return rectangles;
 }




/* kamilFIXME: this assumes that there are any trackpoints on the list. */
CoordMode Track::get_coord_mode()
{
	assert (!this->trackpoints.empty());

	return (*this->trackpoints.begin())->coord.mode;
}




/* Comparison function used to sort trackpoints. */
bool Trackpoint::compare_timestamps(const Trackpoint * a, const Trackpoint * b)
{
	/* kamilFIXME: shouldn't this be difftime()? */
	return a->timestamp < b->timestamp;
}




void Track::find_maxmin(struct LatLon maxmin[2])
{
	if (this->bbox.north > maxmin[0].lat || maxmin[0].lat == 0.0) {
		maxmin[0].lat = this->bbox.north;
	}

	if (this->bbox.south < maxmin[1].lat || maxmin[1].lat == 0.0) {
		maxmin[1].lat = this->bbox.south;
	}

	if (this->bbox.east > maxmin[0].lon || maxmin[0].lon == 0.0) {
		maxmin[0].lon = this->bbox.east;
	}

	if (this->bbox.west < maxmin[1].lon || maxmin[1].lon == 0.0) {
		maxmin[1].lon = this->bbox.west;
	}
}





void Track::sublayer_menu_track_misc(LayerTRW * parent_layer_, QMenu & menu, QMenu * upload_submenu)
{
	QAction * qa = NULL;

#ifdef VIK_CONFIG_OPENSTREETMAP
	qa = upload_submenu->addAction(QIcon::fromTheme("go-up"), tr("Upload to &OSM..."));
	/* Convert internal pointer into track. */
	parent_layer_->menu_data->misc = parent_layer_->tracks.items.at(parent_layer_->menu_data->sublayer->uid);
	connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (osm_traces_upload_track_cb()));
#endif

	/* Currently filter with functions all use shellcommands and thus don't work in Windows. */
#ifndef WINDOWS
	qa = menu.addAction(QIcon::fromTheme("INDEX"), tr("Use with &Filter"));
	connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (track_use_with_filter_cb()));
#endif


	/* ATM This function is only available via the layers panel, due to needing a panel. */
	if (this->window->get_layers_panel()) {
		QMenu * submenu = a_acquire_track_menu(this->window, this->window->get_layers_panel(),
						       parent_layer_->menu_data->viewport,
						       parent_layer_->tracks.items.at(parent_layer_->menu_data->sublayer->uid));
		if (submenu) {
			/* kamilFIXME: .addMenu() does not make menu take ownership of submenu. */
			menu.addMenu(submenu);
		}
	}

#ifdef VIK_CONFIG_GEOTAG
	qa = menu.addAction(tr("Geotag _Images..."));
	connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (geotagging_track_cb()));
#endif
}





void Track::sublayer_menu_track_route_misc(LayerTRW * parent_layer_, QMenu & menu, QMenu * upload_submenu)
{
	QAction * qa = NULL;

	if (parent_layer_->current_trk && this->type_id == "sg.trw.track" && parent_layer_->current_trk->type_id == "sg.trw.track") {
		qa = menu.addAction(tr("&Finish Track"));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (finish_track_cb()));
		menu.addSeparator();
	} else if (parent_layer_->current_trk && this->type_id == "sg.trw.route" && parent_layer_->current_trk->type_id == "sg.trw.route") {
		qa = menu.addAction(tr("&Finish Route"));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (finish_track_cb()));
		menu.addSeparator();
	}

	qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), this->type_id == "sg.trw.track" ? tr("&View Track") : tr("&View Route"));
	connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (auto_track_view_cb()));

	qa = menu.addAction(tr("&Statistics"));
	connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (track_statistics_cb()));

	{
		QMenu * goto_submenu = menu.addMenu(QIcon::fromTheme("go-jump"), tr("&Goto"));

		qa = goto_submenu->addAction(QIcon::fromTheme("go-first"), tr("&Startpoint"));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (goto_track_startpoint_cb()));

		qa = goto_submenu->addAction(QIcon::fromTheme("go-jump"), tr("\"&Center\""));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (goto_track_center_cb()));

		qa = goto_submenu->addAction(QIcon::fromTheme("go-last"), tr("&Endpoint"));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (goto_track_endpoint_cb()));

		qa = goto_submenu->addAction(QIcon::fromTheme("go-top"), tr("&Highest Altitude"));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (goto_track_max_alt_cb()));

		qa = goto_submenu->addAction(QIcon::fromTheme("go-bottom"), tr("&Lowest Altitude"));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (goto_track_min_alt_cb()));

		/* Routes don't have speeds. */
		if (this->type_id == "sg.trw.track") {
			qa = goto_submenu->addAction(QIcon::fromTheme("media-seek-forward"), tr("&Maximum Speed"));
			connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (goto_track_max_speed_cb()));
		}
	}

	{

		QMenu * combine_submenu = menu.addMenu(QIcon::fromTheme("CONNECT"), tr("Co&mbine"));

		/* Routes don't have times or segments... */
		if (this->type_id == "sg.trw.track") {
			qa = combine_submenu->addAction(tr("&Merge By Time..."));
			connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (merge_by_timestamp_cb()));

			qa = combine_submenu->addAction(tr("Merge &Segments"));
			connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (merge_by_segment_cb()));
		}

		qa = combine_submenu->addAction(tr("Merge &With Other Tracks..."));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (merge_with_other_cb()));

		qa = combine_submenu->addAction(this->type_id == "sg.trw.track" ? tr("&Append Track...") : tr("&Append Route..."));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (append_track_cb()));

		qa = combine_submenu->addAction(this->type_id == "sg.trw.track" ? tr("Append &Route...") : tr("Append &Track..."));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (append_other_cb()));
	}



	{
		QMenu * split_submenu = menu.addMenu(QIcon::fromTheme("DISCONNECT"), tr("&Split"));

		/* Routes don't have times or segments... */
		if (this->type_id == "sg.trw.track") {
			qa = split_submenu->addAction(tr("&Split By Time..."));
			connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (split_by_timestamp_cb()));

			/* ATM always enable parent_layer_ entry - don't want to have to analyse the track before displaying the menu - to keep the menu speedy. */
			qa = split_submenu->addAction(tr("Split Se&gments"));
			connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (split_segments_cb()));
		}

		qa = split_submenu->addAction(tr("Split By &Number of Points..."));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (split_by_n_points_cb()));

		qa = split_submenu->addAction(tr("Split at &Trackpoint"));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (split_at_trackpoint_cb()));
		/* Make it available only when a trackpoint is selected. */
		qa->setEnabled(parent_layer_->selected_tp.valid);
	}



	{
		QMenu * insert_submenu = menu.addMenu(QIcon::fromTheme("list-add"), tr("&Insert Points"));

		qa = insert_submenu->addAction(QIcon::fromTheme(""), tr("Insert Point &Before Selected Point"));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (insert_point_before_cb()));
		/* Make it available only when a point is selected. */
		qa->setEnabled(parent_layer_->selected_tp.valid);

		qa = insert_submenu->addAction(QIcon::fromTheme(""), tr("Insert Point &After Selected Point"));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (insert_point_after_cb()));
		/* Make it available only when a point is selected. */
		qa->setEnabled(parent_layer_->selected_tp.valid);
	}


	{
		QMenu * delete_submenu = menu.addMenu(QIcon::fromTheme("list-delete"), tr("Delete Poi&nts"));

		qa = delete_submenu->addAction(QIcon::fromTheme("list-delete"), tr("Delete &Selected Point"));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (delete_point_selected_cb()));
		/* Make it available only when a point is selected. */
		qa->setEnabled(parent_layer_->selected_tp.valid);

		qa = delete_submenu->addAction(tr("Delete Points With The Same &Position"));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (delete_points_same_position_cb()));

		qa = delete_submenu->addAction(tr("Delete Points With The Same &Time"));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (delete_points_same_time_cb()));
	}

	{
		QMenu * transform_submenu = menu.addMenu(QIcon::fromTheme("CONVERT"), tr("&Transform"));
		{
			QMenu * dem_submenu = transform_submenu->addMenu(QIcon::fromTheme("vik-icon-DEM Download"), tr("&Apply DEM Data"));

			qa = dem_submenu->addAction(tr("&Overwrite"));
			connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (apply_dem_data_all_cb()));
			qa->setToolTip(tr("Overwrite any existing elevation values with DEM values"));

			qa = dem_submenu->addAction(tr("&Keep Existing"));
			connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (apply_dem_data_only_missing_cb()));
			qa->setToolTip(tr("Keep existing elevation values, only attempt for missing values"));
		}

		{
			QMenu * smooth_submenu = transform_submenu->addMenu(tr("&Smooth Missing Elevation Data"));

			qa = smooth_submenu->addAction(tr("&Interpolated"));
			connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (missing_elevation_data_interp_cb()));
			qa->setToolTip(tr("Interpolate between known elevation values to derive values for the missing elevations"));

			qa = smooth_submenu->addAction(tr("&Flat"));
			connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (missing_elevation_data_flat_cb()));
			qa->setToolTip(tr("Set unknown elevation values to the last known value"));
		}

		qa = transform_submenu->addAction(QIcon::fromTheme("CONVERT"), this->type_id == "sg.trw.track" ? tr("C&onvert to a Route") : tr("C&onvert to a Track"));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (convert_track_route_cb()));

		/* Routes don't have timestamps - so these are only available for tracks. */
		if (this->type_id == "sg.trw.track") {
			qa = transform_submenu->addAction(tr("&Anonymize Times"));
			connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (anonymize_times_cb()));
			qa->setToolTip(tr("Shift timestamps to a relative offset from 1901-01-01"));

			qa = transform_submenu->addAction(tr("&Interpolate Times"));
			connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (interpolate_times_cb()));
			qa->setToolTip(tr("Reset trackpoint timestamps between the first and last points such that track is traveled at equal speed"));
		}
	}


	qa = menu.addAction(QIcon::fromTheme("go-back"), this->type_id == "sg.trw.track" ? tr("&Reverse Track") : tr("&Reverse Route"));
	connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (reverse_cb()));

	if (this->type_id == "sg.trw.route") {
		qa = menu.addAction(QIcon::fromTheme("edit-find"), tr("Refine Route..."));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (route_refine_cb()));
	}

	/* ATM Parent_Layer_ function is only available via the layers panel, due to the method in finding out the maps in use. */
	if (parent_layer_->get_window()->get_layers_panel()) {
		qa = menu.addAction(QIcon::fromTheme("vik-icon-Maps Download"), this->type_id == "sg.trw.track" ? tr("Down&load Maps Along Track...") : tr("Down&load Maps Along Route..."));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (download_map_along_track_cb()));
	}

	qa = menu.addAction(QIcon::fromTheme("document-save-as"), this->type_id == "sg.trw.track" ? tr("&Export Track as GPX...") : tr("&Export Route as GPX..."));
	connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (export_gpx_track_cb()));

	qa = menu.addAction(QIcon::fromTheme("list-add"), this->type_id == "sg.trw.track" ? tr("E&xtend Track End") : tr("E&xtend Route End"));
	connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (extend_track_end_cb()));

	if (this->type_id == "sg.trw.route") {
		qa = menu.addAction(QIcon::fromTheme("vik-icon-Route Finder"), tr("Extend &Using Route Finder"));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (extend_track_end_route_finder_cb()));
	}

	/* ATM can't upload a single waypoint but can do waypoints to a GPS. */
	if (this->type_id != "sg.trw.waypoint") { /* FIXME: this should be also in some other TRW sublayer. */
		qa = upload_submenu->addAction(QIcon::fromTheme("go-forward"), tr("&Upload to GPS..."));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (gps_upload_any_cb()));
	}
}











bool Track::add_context_menu_items(QMenu & menu)
{
	QAction * qa = NULL;
	bool rv = false;


	rv = true;
	qa = menu.addAction(QIcon::fromTheme("document-properties"), tr("&Properties"));
	if (this->properties_dialog) {
		/* A properties dialog window is already opened.
		   Don't give possibility to open a duplicate properties dialog window. */
		qa->setEnabled(false);
	}
	connect(qa, SIGNAL (triggered(bool)), (LayerTRW *) this->parent_layer, SLOT (properties_item_cb()));



	qa = menu.addAction(QIcon::fromTheme("document-properties"), tr("P&rofile"));
	if (this->profile_dialog) {
		/* A profile dialog window is already opened.
		   Don't give possibility to open a duplicate profile dialog window. */
		qa->setEnabled(false);
	}
	connect(qa, SIGNAL (triggered(bool)), (LayerTRW *) this->parent_layer, SLOT (profile_item_cb()));


	layer_trw_sublayer_menu_waypoint_track_route_edit((LayerTRW *) this->parent_layer, menu);


	menu.addSeparator();


	QMenu * external_submenu = menu.addMenu(QIcon::fromTheme("EXECUTE"), tr("Externa&l"));

	/* These are only made available if a suitable program is installed. */
	if ((g_have_astro_program || g_have_diary_program)
	    && this->type_id == "sg.trw.track") {

		layer_trw_sublayer_menu_track_waypoint_diary_astro((LayerTRW *) this->parent_layer, menu, external_submenu);
	}


	layer_trw_sublayer_menu_all_add_external_tools((LayerTRW *) this->parent_layer, menu, external_submenu);


#ifdef K
#ifdef VIK_CONFIG_GOOGLE
	if (this->type_id == "sg.trw.route" && (this->is_valid_google_route(this->uid))) {
		qa = menu.addAction(QIcon::fromTheme("applications-internet"), tr("&View Google Directions"));
		connect(qa, SIGNAL (triggered(bool)), (LayerTRW *) this->parent_layer, SLOT (google_route_webpage_cb()));
	}
#endif
#endif


	QMenu * upload_submenu = menu.addMenu(QIcon::fromTheme("go-up"), tr("&Upload"));

	this->sublayer_menu_track_route_misc((LayerTRW *) this->parent_layer, menu, upload_submenu);


	/* Some things aren't usable with routes. */
	if (this->type_id == "sg.trw.track") {
		this->sublayer_menu_track_misc((LayerTRW *) this->parent_layer, menu, upload_submenu);
	}


	/* Only show on viewport popmenu when a trackpoint is selected. */
	if (!this->window->get_layers_panel() && ((LayerTRW *) parent_layer)->selected_tp.valid) {
		menu.addSeparator();

		qa = menu.addAction(QIcon::fromTheme("document-properties"), tr("&Edit Trackpoint"));
		connect(qa, SIGNAL (triggered(bool)), (LayerTRW *) this->parent_layer, SLOT (edit_trackpoint_cb()));
	}


	return rv;
}
