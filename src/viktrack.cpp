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

#include <glib.h>
#include <time.h>
#include <cstdlib>
#include <assert.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include "coords.h"
#include "vikcoord.h"
#include "viktrack.h"
#include "globals.h"
#include "dems.h"
#include "settings.h"
#include "util.h"

using namespace SlavGPS;

Track::Track()
{
	this->trackpoints = NULL;
	visible = false;
	is_route = false;
	draw_name_mode = TRACK_DRAWNAME_NO;
	max_number_dist_labels = 0;
	comment = NULL;
	description = NULL;
	source = NULL;
	type = NULL;
	ref_count = 0;
	name = NULL;
	property_dialog = NULL;
	has_color = 0;
	memset(&color, 0, sizeof (GdkColor));
	memset(&bbox, 0, sizeof (LatLonBBox));

	ref_count = 1;
}

#define VIK_SETTINGS_TRACK_NAME_MODE "track_draw_name_mode"
#define VIK_SETTINGS_TRACK_NUM_DIST_LABELS "track_number_dist_labels"

/**
 * vik_track_set_defaults:
 *
 * Set some default values for a track.
 * ATM This uses the 'settings' method to get values,
 *  so there is no GUI way to control these yet...
 */
void Track::set_defaults()
{
	int tmp;
	if (a_settings_get_integer(VIK_SETTINGS_TRACK_NAME_MODE, &tmp)) {
		draw_name_mode = (TrackDrawnameType) tmp;
	}

	if (a_settings_get_integer(VIK_SETTINGS_TRACK_NUM_DIST_LABELS, &tmp)) {
		max_number_dist_labels = (TrackDrawnameType) tmp;
	}
}

void Track::set_comment_no_copy(char * comment_)
{
	free_string(&comment);

	comment = comment_;
}


void Track::set_name(const char * name_)
{
	free_string(&name);

	if (name_) {
		name = g_strdup(name_);
	}
}

void Track::set_comment(const char * comment_)
{
	free_string(&comment);

	if (comment_ && comment_[0] != '\0') {
		comment = g_strdup(comment_);
	}
}

void Track::set_description(const char * description_)
{
	free_string(&description);

	if (description_ && description_[0] != '\0') {
		description = g_strdup(description_);
	}
}

void Track::set_source(const char * source_)
{
	free_string(&source);

	if (source_ && source_[0] != '\0') {
		source = g_strdup(source_);
	}
}

void Track::set_type(const char * type_)
{
	free_string(&type);

	if (type_ && type_[0] != '\0') {
		type = g_strdup(type_);
	}
}

void Track::ref()
{
	ref_count++;
}

void Track::set_property_dialog(GtkWidget * dialog)
{
	/* Warning: does not check for existing dialog */
	property_dialog = dialog;
}

void Track::clear_property_dialog()
{
	property_dialog = NULL;
}

void Track::free()
{
	if (ref_count-- > 1) {
		return;
	}

	free_string(&name);
	free_string(&comment);
	free_string(&description);
	free_string(&source);
	free_string(&type);

	g_list_foreach (this->trackpoints, (GFunc) (Trackpoint::vik_trackpoint_free), NULL);
	g_list_free(this->trackpoints);
	if (property_dialog) {
		if (GTK_IS_WIDGET(property_dialog)) {
			gtk_widget_destroy(GTK_WIDGET(property_dialog));
		}
	}
	delete this;
}

/**
 * vik_track_copy:
 * @from: The Track to copy
 * @copy_points: Whether to copy the track points or not
 *
 * Normally for copying the track it's best to copy all the trackpoints
 * However for some operations such as splitting tracks the trackpoints will be managed separately, so no need to copy them.
 *
 * Returns: the copied Track
 */
Track::Track(const Track & from, const bool copy_points)
{
	this->trackpoints = NULL;
	visible = false;
	is_route = false;
	draw_name_mode = TRACK_DRAWNAME_NO;
	max_number_dist_labels = 0;
	comment = NULL;
	description = NULL;
	source = NULL;
	type = NULL;
	ref_count = 0;
	name = NULL;
	property_dialog = NULL;
	has_color = 0;
	memset(&color, 0, sizeof (GdkColor));
	memset(&bbox, 0, sizeof (LatLonBBox));

	//this->name = g_strdup(from.name); /* kamilFIXME: in original code initialization of name is duplicated. */
	this->visible = from.visible;
	this->is_route = from.is_route;
	this->draw_name_mode = from.draw_name_mode;
	this->max_number_dist_labels = from.max_number_dist_labels;
	this->has_color = from.has_color;
	this->color = from.color;
	this->bbox = from.bbox;
	if (copy_points) {

		for (GList * iter = from.trackpoints; iter; iter = iter->next) {
			Trackpoint * tp = (Trackpoint *) iter->data;
			Trackpoint * new_tp = new Trackpoint(*tp);
			this->trackpoints = g_list_prepend(this->trackpoints, new_tp);
		}
		if (this->trackpoints) {
			this->trackpoints = g_list_reverse(this->trackpoints);
		}
	}
	this->set_name(from.name);
	this->set_comment(from.comment);
	this->set_description(from.description);
	this->set_source(from.source);
}

Trackpoint::Trackpoint()
{
	name = NULL;

	memset(&coord, 0, sizeof (VikCoord));
	newsegment = false;
	has_timestamp = false;
	timestamp = 0;

	altitude = VIK_DEFAULT_ALTITUDE;
	speed = NAN;
	course = NAN;

	nsats = 0;

	fix_mode = VIK_GPS_MODE_NOT_SEEN;
	hdop = VIK_DEFAULT_DOP;
	vdop = VIK_DEFAULT_DOP;
	pdop = VIK_DEFAULT_DOP;


}

Trackpoint::~Trackpoint()
{
	free_string(&name);
}

void Trackpoint::set_name(char const * name_)
{
	free_string(&name);

	// If the name is blank then completely remove it
	if (name_ && name_[0] == '\0') {
		name = NULL;
	} else if (name_) {
		name = g_strdup(name_);
	} else {
		name = NULL;
	}
}

Trackpoint::Trackpoint(const Trackpoint & tp)
{
	if (this->name) {
		this->name = g_strdup(tp.name);
	}

	memcpy((void *) &(this->coord), (void *) &(tp.coord), sizeof (VikCoord));
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





void Trackpoint::vik_trackpoint_free(Trackpoint *tp)
{
	delete tp;
}





/**
 * track_recalculate_bounds_last_tp:
 * @trk:   The track to consider the recalculation on
 *
 * A faster bounds check, since it only considers the last track point
 */
void Track::recalculate_bounds_last_tp()
{
	GList * tpl = g_list_last(this->trackpoints);

	if (tpl) {
		struct LatLon ll;
		// See if this trackpoint increases the track bounds and update if so
		vik_coord_to_latlon (&(((Trackpoint *) tpl->data)->coord), &ll);
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
 * vik_track_add_trackpoint:
 * @tr:          The track to which the trackpoint will be added
 * @tp:          The trackpoint to add
 * @recalculate: Whether to perform any associated properties recalculations
 *               Generally one should avoid recalculation via this method if adding lots of points
 *               (But ensure calculate_bounds() is called after adding all points!!)
 *
 * The trackpoint is added to the end of the existing trackpoint list
 */
void Track::add_trackpoint(Trackpoint * tp, bool recalculate)
{
	// When it's the first trackpoint need to ensure the bounding box is initialized correctly
	bool adding_first_point = this->trackpoints ? false : true;
	this->trackpoints = g_list_append(this->trackpoints, tp);
	if (adding_first_point) {
		this->calculate_bounds();
	} else if (recalculate) {
		this->recalculate_bounds_last_tp();
	}
}

/**
 * vik_track_get_length_to_trackpoint:
 *
 */
double Track::get_length_to_trackpoint(const Trackpoint * tp)
{
	double len = 0.0;
	if (!this->trackpoints) {
		return len;
	}

	// Is it the very first track point?
	if (((Trackpoint *) this->trackpoints->data) == tp) {
		return len;
	}

	for (GList * iter = this->trackpoints->next; iter; iter = iter->next) {
		Trackpoint * tp1 = ((Trackpoint *) iter->data);
		if (! tp1->newsegment) {
			len += vik_coord_diff(&(tp1->coord),
					      &(((Trackpoint *) iter->prev->data)->coord));
		}

		// Exit when we reach the desired point
		if (tp1 == tp) {
			break;
		}
	}
	return len;
}

double Track::get_length()
{
	double len = 0.0;
	if (!this->trackpoints) {
		return len;
	}

	for (GList * iter = this->trackpoints->next; iter; iter = iter->next) {
		Trackpoint * tp1 = ((Trackpoint *) iter->data);
		if (! tp1->newsegment) {
			len += vik_coord_diff(&(tp1->coord),
					      &(((Trackpoint *) iter->prev->data)->coord));
		}
	}
	return len;
}

double Track::get_length_including_gaps()
{
	double len = 0.0;
	if (!this->trackpoints) {
		return len;
	}

	for (GList * iter = this->trackpoints->next; iter; iter = iter->next) {
		len += vik_coord_diff(&(((Trackpoint *) iter->data)->coord),
				      &(((Trackpoint *) iter->prev->data)->coord));
	}
	return len;
}

unsigned long Track::get_tp_count()
{
	return g_list_length(this->trackpoints);
}

unsigned long Track::get_dup_point_count()
{
	unsigned long num = 0;

	for (GList * iter = this->trackpoints; iter; iter = iter->next) {
		if (iter->next && vik_coord_equals(&(((Trackpoint *) iter->data)->coord),
						   &(((Trackpoint *) iter->next->data)->coord))) {
			num++;
		}
	}
	return num;
}

/*
 * Deletes adjacent points that have the same position
 * Returns the number of points that were deleted
 */
unsigned long Track::remove_dup_points()
{
	unsigned long num = 0;
	GList *iter = this->trackpoints;
	while (iter) {
		if (iter->next && vik_coord_equals(&(((Trackpoint *) iter->data)->coord),
						   &(((Trackpoint *) iter->next->data)->coord))) {
			num++;
			/* Maintain track segments. */
			if (((Trackpoint *) iter->next->data)->newsegment && (iter->next)->next) {
				((Trackpoint *) ((iter->next)->next)->data)->newsegment = true;
			}

			delete ((Trackpoint *) iter->next->data);
			this->trackpoints = g_list_delete_link(this->trackpoints, iter->next);
		} else {
			iter = iter->next;
		}
	}

	// NB isn't really be necessary as removing duplicate points shouldn't alter the bounds!
	this->calculate_bounds();

	return num;
}

/*
 * Get a count of trackpoints with the same defined timestamp
 * Note is using timestamps with a resolution with 1 second
 */
unsigned long Track::get_same_time_point_count()
{
	unsigned long num = 0;

	for (GList * iter = this->trackpoints; iter; iter = iter->next) {
		if (iter->next
		    && (((Trackpoint *) iter->data)->has_timestamp && ((Trackpoint *) iter->next->data)->has_timestamp)
		    && (((Trackpoint *) iter->data)->timestamp == ((Trackpoint *) iter->next->data)->timestamp)) {

			num++;
		}
	}
	return num;
}

/*
 * Deletes adjacent points that have the same defined timestamp
 * Returns the number of points that were deleted
 */
unsigned long Track::remove_same_time_points()
{
	unsigned long num = 0;
	GList *iter = this->trackpoints;
	while (iter) {
		if (iter->next &&
		    (((Trackpoint *) iter->data)->has_timestamp && ((Trackpoint *) iter->next->data)->has_timestamp)
		    && (((Trackpoint *) iter->data)->timestamp == ((Trackpoint *) iter->next->data)->timestamp)) {

			num++;

			/* Maintain track segments. */
			if (((Trackpoint *) iter->next->data)->newsegment && (iter->next)->next) {
				((Trackpoint *) ((iter->next)->next)->data)->newsegment = true;
			}

			delete ((Trackpoint *) iter->next->data);
			this->trackpoints = g_list_delete_link(this->trackpoints, iter->next);
		} else {
			iter = iter->next;
		}
	}

	this->calculate_bounds();

	return num;
}





/*
 * Deletes all 'extra' trackpoint information
 *  such as time stamps, speed, course etc...
 */
void Track::to_routepoints(void)
{
	for (GList * iter = this->trackpoints; iter; iter = iter->next) {

		// c.f. with vik_trackpoint_new()

		((Trackpoint *) iter->data)->has_timestamp = false;
		((Trackpoint *) iter->data)->timestamp = 0;
		((Trackpoint *) iter->data)->speed = NAN;
		((Trackpoint *) iter->data)->course = NAN;
		((Trackpoint *) iter->data)->hdop = VIK_DEFAULT_DOP;
		((Trackpoint *) iter->data)->vdop = VIK_DEFAULT_DOP;
		((Trackpoint *) iter->data)->pdop = VIK_DEFAULT_DOP;
		((Trackpoint *) iter->data)->nsats = 0;
		((Trackpoint *) iter->data)->fix_mode = VIK_GPS_MODE_NOT_SEEN;
	}
}





unsigned int Track::get_segment_count()
{
	unsigned int num = 1;

	if (!this->trackpoints) {
		return 0;
	}

	GList *iter = this->trackpoints;
	while ((iter = iter->next)) {
		if (((Trackpoint *) iter->data)->newsegment) {
			num++;
		}
	}
	return num;
}

/* kamilTODO: revisit this function and compare with original. */
Track ** Track::split_into_segments(unsigned int *ret_len)
{
	unsigned int i;
	unsigned int segs = this->get_segment_count();

	if (segs < 2) {
		*ret_len = 0;
		return NULL;
	}

	Track ** rv = (Track **) malloc(segs * sizeof (Track *));
	Track * trk_copy = new Track(*this, true);
	rv[0] = trk_copy;
	GList *iter = trk_copy->trackpoints;

	i = 1;
	while ((iter = iter->next)) {
		if (((Trackpoint *) iter->data)->newsegment) {
			iter->prev->next = NULL;
			iter->prev = NULL;
			rv[i] = new Track(*trk_copy, false);
			rv[i]->trackpoints = iter;

			rv[i]->calculate_bounds();

			i++;
		}
	}
	*ret_len = segs;
	return rv;
}





/*
 * Simply remove any subsequent segment markers in a track to form one continuous track
 * Return the number of segments merged
 */
unsigned int Track::merge_segments(void)
{
	if (!this->trackpoints) {
		return 0;
	}

	unsigned int num = 0;
	GList *iter = this->trackpoints;

	/* Always skip the first point as this should be the first segment. */
	iter = iter->next;

	while ((iter = iter->next)) {
		if (((Trackpoint *) iter->data)->newsegment) {
			((Trackpoint *) iter->data)->newsegment = false;
			num++;
		}
	}

	return num;
}





void Track::reverse(void)
{
	if (!this->trackpoints) {
		return;
	}

	this->trackpoints = g_list_reverse(this->trackpoints);

	/* fix 'newsegment' */
	GList *iter = g_list_last(this->trackpoints);
	while (iter) {
		if (!iter->next) { /* last segment, was first, cancel newsegment. */
			((Trackpoint *) iter->data)->newsegment = false;
		}

		if (! iter->prev) { /* first segment by convention has newsegment flag. */
			((Trackpoint *) iter->data)->newsegment = true;
		} else if (((Trackpoint *) iter->data)->newsegment && iter->next) {
			((Trackpoint *) iter->next->data)->newsegment = true;
			((Trackpoint *) iter->data)->newsegment = false;
		} else {
			;
		}
		iter = iter->prev;
	}

	return;
}





/**
 * get_duration:
 * @segment_gaps: Whether the duration should include gaps between segments
 *
 * Returns: The time in seconds
 *  NB this may be negative particularly if the track has been reversed
 */
time_t Track::get_duration(bool segment_gaps)
{
	if (!this->trackpoints) {
		return 0;
	}

	time_t duration = 0;

	// Ensure times are available
	if (this->get_tp_first()->has_timestamp) {
		// Get trkpt only once - as using vik_track_get_tp_last() iterates whole track each time
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
			for (GList * iter = this->trackpoints->next; iter; iter = iter->next) {
				if (((Trackpoint *) iter->data)->has_timestamp && ((Trackpoint *) iter->prev->data)->has_timestamp
				    && (!((Trackpoint *) (Trackpoint *) iter->data)->newsegment)) {

					duration += ABS (((Trackpoint *) iter->data)->timestamp - ((Trackpoint *) iter->prev->data)->timestamp);
				}
			}
		}
	}

	return duration;
}





/* Code extracted from make_speed_map() and similar functions. */
double Track::get_duration()
{
	if (!this->trackpoints) {
		return 0.0;
	}

	time_t t1 = ((Trackpoint *) this->trackpoints->data)->timestamp;
	time_t t2 = ((Trackpoint *) g_list_last(this->trackpoints)->data)->timestamp;
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





double Track::get_average_speed()
{
	if (!this->trackpoints) {
		return 0.0;
	}

	double len = 0.0;
	uint32_t time = 0;

	for (GList * iter = this->trackpoints->next; iter; iter = iter->next) {

		if (((Trackpoint *) iter->data)->has_timestamp && ((Trackpoint *) iter->prev->data)->has_timestamp
		    && (! ((Trackpoint *) iter->data)->newsegment)) {

			len += vik_coord_diff(&(((Trackpoint *) iter->data)->coord),
					      &(((Trackpoint *) iter->prev->data)->coord));
			time += ABS (((Trackpoint *) iter->data)->timestamp - ((Trackpoint *) iter->prev->data)->timestamp);
		}
	}
	return (time == 0) ? 0 : ABS(len/time);
}





/**
 * Based on a simple average speed, but with a twist - to give a moving average.
 *  . GPSs often report a moving average in their statistics output
 *  . bicycle speedos often don't factor in time when stopped - hence reporting a moving average for speed
 *
 * Often GPS track will record every second but not when stationary
 * This method doesn't use samples that differ over the specified time limit - effectively skipping that time chunk from the total time
 *
 * Suggest to use 60 seconds as the stop length (as the default used in the TrackWaypoint draw stops factor)
 */
double Track::get_average_speed_moving(int stop_length_seconds)
{
	if (!this->trackpoints) {
		return 0.0;
	}

	double len = 0.0;
	uint32_t time = 0;

	for (GList * iter = this->trackpoints->next; iter; iter = iter->next) {
		if (((Trackpoint *) iter->data)->has_timestamp
		    && ((Trackpoint *) iter->prev->data)->has_timestamp
		    && (!((Trackpoint *) iter->data)->newsegment)) {

			if ((((Trackpoint *) iter->data)->timestamp - ((Trackpoint *) iter->prev->data)->timestamp) < stop_length_seconds) {
				len += vik_coord_diff(&(((Trackpoint *) iter->data)->coord),
						      &(((Trackpoint *) iter->prev->data)->coord));

				time += ABS (((Trackpoint *) iter->data)->timestamp - ((Trackpoint *) iter->prev->data)->timestamp);
			}
		}
	}
	return (time == 0) ? 0 : ABS(len / time);
}





double Track::get_max_speed()
{
	if (!this->trackpoints) {
		return 0.0;
	}

	double maxspeed = 0.0;
	double speed = 0.0;

	for (GList * iter = this->trackpoints->next; iter; iter = iter->next) {
		if (((Trackpoint *) iter->data)->has_timestamp
		    && ((Trackpoint *) iter->prev->data)->has_timestamp
		    && (! ((Trackpoint *) iter->data)->newsegment)) {

			speed = vik_coord_diff(&(((Trackpoint *) iter->data)->coord), &(((Trackpoint *) iter->prev->data)->coord))
				/ ABS (((Trackpoint *) iter->data)->timestamp - ((Trackpoint *) iter->prev->data)->timestamp);

			if (speed > maxspeed) {
				maxspeed = speed;
			}
		}
	}
	return maxspeed;
}





void Track::convert(VikCoordMode dest_mode)
{
	for (GList * iter = this->trackpoints; iter; iter = iter->next) {
		vik_coord_convert(&(((Trackpoint *) iter->data)->coord), dest_mode);
	}
}

/* I understood this when I wrote it ... maybe ... Basically it eats up the
 * proper amounts of length on the track and averages elevation over that. */
double * Track::make_elevation_map(uint16_t num_chunks)
{
	assert (num_chunks < 16000);
	if (!this->trackpoints || !this->trackpoints->next) { /* zero- or one-point track */
		return NULL;
	}

	{ /* Test if there's anything worth calculating. */

		bool okay = false;
		for (GList * iter = this->trackpoints; iter; iter = iter->next) {
			/* Sometimes a GPS device (or indeed any random file) can have stupid numbers for elevations.
			   Since when is 9.9999e+24 a valid elevation!!
			   This can happen when a track (with no elevations) is uploaded to a GPS device and then redownloaded (e.g. using a Garmin Legend EtrexHCx).
			   Some protection against trying to work with crazily massive numbers (otherwise get SIGFPE, Arithmetic exception) */

			if (((Trackpoint *) iter->data)->altitude != VIK_DEFAULT_ALTITUDE
			    && ((Trackpoint *) iter->data)->altitude < 1E9) {

				okay = true;
				break;
			}
		}
		if (!okay) {
			return NULL;
		}
	}

	GList *iter = this->trackpoints;

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
	double current_seg_length = vik_coord_diff(&(((Trackpoint *) iter->data)->coord),
						   &(((Trackpoint *) iter->next->data)->coord));

	double altitude1 = ((Trackpoint *) iter->data)->altitude;
	double altitude2 = ((Trackpoint *) iter->next->data)->altitude;
	double dist_along_seg = 0;

	bool ignore_it = false;
	while (current_chunk < num_chunks) {

		/* go along current seg */
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
				// Seemly can't determine average for this section - so use last known good value (much better than just sticking in zero)
				pts[current_chunk] = altitude1;
			} else {
				pts[current_chunk] = altitude1 + (altitude2 - altitude1) * ((dist_along_seg - (chunk_length / 2)) / current_seg_length);
			}

			current_chunk++;
		} else {
			/* finish current seg */
			if (current_seg_length) {
				double altitude_at_dist_along_seg = altitude1 + (altitude2 - altitude1) / (current_seg_length) * dist_along_seg;
				current_dist = current_seg_length - dist_along_seg;
				current_area_under_curve = current_dist * (altitude_at_dist_along_seg + altitude2) * 0.5;
			} else {
				current_dist = current_area_under_curve = 0;  /* should only happen if first current_seg_length == 0 */
			}
			/* get intervening segs */
			iter = iter->next;
			while (iter && iter->next) {
				current_seg_length = vik_coord_diff(&(((Trackpoint *) iter->data)->coord),
								    &(((Trackpoint *) iter->next->data)->coord));
				altitude1 = ((Trackpoint *) iter->data)->altitude;
				altitude2 = ((Trackpoint *) iter->next->data)->altitude;
				ignore_it = ((Trackpoint *) iter->next->data)->newsegment;

				if (chunk_length - current_dist >= current_seg_length) {
					current_dist += current_seg_length;
					current_area_under_curve += current_seg_length * (altitude1+altitude2) * 0.5;
					iter = iter->next;
				} else {
					break;
				}
			}

			/* final seg */
			dist_along_seg = chunk_length - current_dist;
			if (ignore_it || (iter && !iter->next)) {
				pts[current_chunk] = current_area_under_curve / current_dist;
				if (!iter->next) {
					int i;
					for (i = current_chunk + 1; i < num_chunks; i++) {
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





void Track::get_total_elevation_gain(double *up, double *down)
{
	if (this->trackpoints && ((Trackpoint *) this->trackpoints->data)->altitude != VIK_DEFAULT_ALTITUDE) {
		*up = 0;
		*down = 0;

		for (GList * iter = this->trackpoints->next; iter; iter = iter->next) {
			double diff = ((Trackpoint *) iter->data)->altitude - ((Trackpoint *) iter->prev->data)->altitude;
			if (diff > 0) {
				*up += diff;
			} else {
				*down -= diff;
			}
		}
	} else {
		*up = VIK_DEFAULT_ALTITUDE;
		*down = VIK_DEFAULT_ALTITUDE;
	}
}





double * Track::make_gradient_map(const uint16_t num_chunks)
{
	assert (num_chunks < 16000);

	double total_length = this->get_length_including_gaps();
	double chunk_length = total_length / num_chunks;

	/* Zero chunk_length (eg, track of 2 tp with the same loc) will cause crash */
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


/* by Alex Foobarian */
double * Track::make_speed_map(const uint16_t num_chunks)
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
	GList * iter = this->trackpoints;
	s[numpts] = 0;
	t[numpts] = ((Trackpoint *) iter->data)->timestamp;
	numpts++;
	iter = iter->next;
	while (iter) {
		s[numpts] = s[numpts - 1] + vik_coord_diff(&(((Trackpoint *) iter->prev->data)->coord), &(((Trackpoint *) iter->data)->coord));
		t[numpts] = ((Trackpoint *) iter->data)->timestamp;
		numpts++;
		iter = iter->next;
	}

	/* In the following computation, we iterate through periods of time of duration chunk_size.
	   The first period begins at the beginning of the track.  The last period ends at the end of the track. */
	int index = 0; /* index of the current trackpoint. */
	for (int i = 0; i < num_chunks; i++) {
		/* We are now covering the interval from t[0] + i * chunk_size to t[0] + (i + 1) * chunk_size.
		   find the first trackpoint outside the current interval, averaging the speeds between intermediate trackpoints. */
		if (t[0] + i * chunk_size >= t[index]) {
			double acc_t = 0;
			double acc_s = 0;
			while (t[0] + i * chunk_size >= t[index]) {
				acc_s += (s[index+1] - s[index]);
				acc_t += (t[index+1] - t[index]);
				index++;
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
 * Make a distance/time map, heavily based on the vik_track_make_speed_map method
 */
double * Track::make_distance_map(const uint16_t num_chunks)
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
	GList * iter = this->trackpoints;
	s[numpts] = 0;
	t[numpts] = ((Trackpoint *) iter->data)->timestamp;
	numpts++;
	iter = iter->next;
	while (iter) {
		s[numpts] = s[numpts - 1] + vik_coord_diff(&(((Trackpoint *) iter->prev->data)->coord), &(((Trackpoint *) iter->data)->coord));
		t[numpts] = ((Trackpoint *) iter->data)->timestamp;
		numpts++;
		iter = iter->next;
	}

	/* In the following computation, we iterate through periods of time of duration chunk_size.
	   The first period begins at the beginning of the track.  The last period ends at the end of the track. */
	int index = 0; /* index of the current trackpoint. */
	for (int i = 0; i < num_chunks; i++) {
		/* We are now covering the interval from t[0] + i * chunk_size to t[0] + (i + 1) * chunk_size.
		   find the first trackpoint outside the current interval, averaging the distance between intermediate trackpoints. */
		if (t[0] + i * chunk_size >= t[index]) {
			double acc_s = 0;
			/* No need for acc_t. */
			while (t[0] + i * chunk_size >= t[index]) {
				acc_s += (s[index + 1] - s[index]);
				index++;
			}
			// The only bit that's really different from the speed map - just keep an accululative record distance
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
 * This uses the 'time' based method to make the graph, (which is a simpler compared to the elevation/distance)
 * This results in a slightly blocky graph when it does not have many trackpoints: <60
 * NB Somehow the elevation/distance applies some kind of smoothing algorithm,
 *   but I don't think any one understands it any more (I certainly don't ATM)
 */
double * Track::make_elevation_time_map(const uint16_t num_chunks)
{
	if (!this->trackpoints || !this->trackpoints->next) { /* zero- or one-point track */
		return NULL;
	}

	/* test if there's anything worth calculating */
	bool okay = false;
	for (GList * iter = this->trackpoints; iter; iter = iter->next) {
		if (((Trackpoint *) iter->data)->altitude != VIK_DEFAULT_ALTITUDE) {
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

	double * out = (double *) malloc(sizeof (double) * num_chunks); // The return altitude values
	double * s = (double *) malloc(sizeof (double) * pt_count); // calculation altitudes
	double * t = (double *) malloc(sizeof (double) * pt_count); // calculation times


	int numpts = 0;
	GList * iter = this->trackpoints;
	s[numpts] = ((Trackpoint *) iter->data)->altitude;
	t[numpts] = ((Trackpoint *) iter->data)->timestamp;
	numpts++;
	iter = iter->next;
	while (iter) {
		s[numpts] = ((Trackpoint *) iter->data)->altitude;
		t[numpts] = ((Trackpoint *) iter->data)->timestamp;
		numpts++;
		iter = iter->next;
	}

	/* In the following computation, we iterate through periods of time of duration chunk_size.
	   The first period begins at the beginning of the track.  The last period ends at the end of the track. */
	int index = 0; /* index of the current trackpoint. */
	for (int i = 0; i < num_chunks; i++) {
		/* We are now covering the interval from t[0] + i * chunk_size to t[0] + (i + 1) * chunk_size.
		   find the first trackpoint outside the current interval, averaging the heights between intermediate trackpoints. */
		if (t[0] + i * chunk_size >= t[index]) {
			double acc_s = s[index]; // initialise to first point

			while (t[0] + i * chunk_size >= t[index]) {
				acc_s += (s[index + 1] - s[index]);
				index++;
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
 * Make a speed/distance map
 */
double * Track::make_speed_dist_map(const uint16_t num_chunks)
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

	// No special handling of segments ATM...
	int numpts = 0;
	GList * iter = this->trackpoints;
	s[numpts] = 0;
	t[numpts] = ((Trackpoint *) iter->data)->timestamp;
	numpts++;
	iter = iter->next;
	while (iter) {
		s[numpts] = s[numpts - 1] + vik_coord_diff(&(((Trackpoint *) iter->prev->data)->coord), &(((Trackpoint *) iter->data)->coord));
		t[numpts] = ((Trackpoint *) iter->data)->timestamp;
		numpts++;
		iter = iter->next;
	}

	/* Iterate through a portion of the track to get an average speed for that part
	   This will essentially interpolate between segments, which I think is right given the usage of 'get_length_including_gaps'. */
	int index = 0; /* index of the current trackpoint. */
	for (int i = 0; i < num_chunks; i++) {
		/* Similar to the make_speed_map, but instead of using a time chunk, use a distance chunk.
		 */
		if (s[0] + i * chunk_size >= s[index]) {
			double acc_t = 0;
			double acc_s = 0;
			while (s[0] + i * chunk_size >= s[index]) {
				acc_s += (s[index + 1] - s[index]);
				acc_t += (t[index + 1] - t[index]);
				index++;
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
 * vik_track_get_tp_by_dist:
 * @trk:                  The Track on which to find a Trackpoint
 * @meters_from_start:    The distance along a track that the trackpoint returned is near
 * @get_next_point:       Since there is a choice of trackpoints, this determines which one to return
 * @tp_metres_from_start: For the returned Trackpoint, returns the distance along the track
 *
 * TODO: Consider changing the boolean get_next_point into an enum with these options PREVIOUS, NEXT, NEAREST
 *
 * Returns: The #Trackpoint fitting the criteria or NULL
 */
Trackpoint * Track::get_tp_by_dist(double meters_from_start, bool get_next_point, double *tp_metres_from_start)
{
	double current_dist = 0.0;
	double current_inc = 0.0;
	if (tp_metres_from_start) {
		*tp_metres_from_start = 0.0;
	}

	if (!this->trackpoints) {
		return NULL;
	}

	GList *iter = g_list_next (g_list_first (this->trackpoints));
	while (iter) {
		current_inc = vik_coord_diff(&(((Trackpoint *) iter->data)->coord),
					     &(((Trackpoint *) iter->prev->data)->coord));
		current_dist += current_inc;
		if (current_dist >= meters_from_start) {
			break;
		}
		iter = iter->next;
	}
	// passed the end of the track
	if (!iter) {
		return NULL;
	}

	if (tp_metres_from_start) {
		*tp_metres_from_start = current_dist;
	}

	// we've gone past the distance already, is the previous trackpoint wanted?
	if (!get_next_point) {
		if (iter->prev) {
			if (tp_metres_from_start) {
				*tp_metres_from_start = current_dist-current_inc;
			}
			return ((Trackpoint *) iter->prev->data);
		}
	}
	return ((Trackpoint *) iter->data);
}

/* by Alex Foobarian */
Trackpoint * Track::get_closest_tp_by_percentage_dist(double reldist, double *meters_from_start)
{
	double dist = this->get_length_including_gaps() * reldist;
	double current_dist = 0.0;
	double current_inc = 0.0;
	if (!this->trackpoints) {
		return NULL;
	}

	GList *last_iter = NULL;
	double last_dist = 0.0;

	GList *iter = this->trackpoints->next;
	for (; iter; iter = iter->next) {
		current_inc = vik_coord_diff(&(((Trackpoint *) iter->data)->coord),
					     &(((Trackpoint *) iter->prev->data)->coord));
		last_dist = current_dist;
		current_dist += current_inc;
		if (current_dist >= dist) {
			break;
		}
		last_iter = iter;
	}

	if (!iter) { /* passing the end the track */
		if (last_iter) {
			if (meters_from_start) {
				*meters_from_start = last_dist;
			}
			return(((Trackpoint *) last_iter->data));
		} else {
			return NULL;
		}
	}
	/* we've gone past the dist already, was prev trackpoint closer? */
	/* should do a vik_coord_average_weighted() thingy. */
	if (iter->prev && fabs(current_dist-current_inc-dist) < fabs(current_dist-dist)) {
		if (meters_from_start) {
			*meters_from_start = last_dist;
		}
		iter = iter->prev;
	} else {
		if (meters_from_start) {
			*meters_from_start = current_dist;
		}
	}

	return ((Trackpoint *) iter->data);
}

Trackpoint * Track::get_closest_tp_by_percentage_time (double reltime, time_t *seconds_from_start)
{
	if (!this->trackpoints) {
		return NULL;
	}

	time_t t_start = ((Trackpoint *) this->trackpoints->data)->timestamp;
	time_t t_end = ((Trackpoint *) g_list_last(this->trackpoints)->data)->timestamp;
	time_t t_total = t_end - t_start;
	time_t t_pos = t_start + t_total * reltime;

	GList *iter = this->trackpoints;
	while (iter) {
		if (((Trackpoint *) iter->data)->timestamp == t_pos) {
			break;
		}

		if (((Trackpoint *) iter->data)->timestamp > t_pos) {
			if (iter->prev == NULL) {  /* First trackpoint. */
				break;
			}
			time_t t_before = t_pos - ((Trackpoint *) iter->prev->data)->timestamp;
			time_t t_after = ((Trackpoint *) iter->data)->timestamp - t_pos;
			if (t_before <= t_after) {
				iter = iter->prev;
			}
			break;
		} else if ((iter->next == NULL) && (t_pos < (((Trackpoint *) iter->data)->timestamp + 3))) { /* Last trackpoint: accommodate for round-off. */
			break;
		} else {
			;
		}
		iter = iter->next;
	}

	if (!iter) {
		return NULL;
	}

	if (seconds_from_start) {
		*seconds_from_start = ((Trackpoint *) iter->data)->timestamp - ((Trackpoint *) this->trackpoints->data)->timestamp;
	}

	return ((Trackpoint *) iter->data);
}





Trackpoint * Track::get_tp_by_max_speed()
{
	if (!this->trackpoints) {
		return NULL;
	}

	Trackpoint * max_speed_tp = NULL;
	double maxspeed = 0.0;
	double speed = 0.0;

	for (GList * iter = this->trackpoints; iter; iter = iter->next) {
		if (iter->prev) {
			if (((Trackpoint *) iter->data)->has_timestamp
			    && ((Trackpoint *) iter->prev->data)->has_timestamp
			    && (!((Trackpoint *) iter->data)->newsegment)) {

				speed = vik_coord_diff(&(((Trackpoint *) iter->data)->coord), &(((Trackpoint *) iter->prev->data)->coord))
					/ ABS (((Trackpoint *) iter->data)->timestamp - ((Trackpoint *) iter->prev->data)->timestamp);

				if (speed > maxspeed) {
					maxspeed = speed;
					max_speed_tp = ((Trackpoint *) iter->data);
				}
			}
		}
	}

	if (!max_speed_tp) {
		return NULL;
	}

	return max_speed_tp;
}





Trackpoint * Track::get_tp_by_max_alt()
{
	if (!this->trackpoints) {
		return NULL;
	}

	Trackpoint * max_alt_tp = NULL;
	double maxalt = -5000.0;

	for (GList * iter = this->trackpoints; iter; iter = iter->next) {
		if (((Trackpoint *) iter->data)->altitude > maxalt) {
			maxalt = ((Trackpoint *) iter->data)->altitude;
			max_alt_tp = ((Trackpoint *) iter->data);
		}
	}

	if (!max_alt_tp) {
		return NULL;
	}

	return max_alt_tp;
}





Trackpoint * Track::get_tp_by_min_alt()
{
	if (!this->trackpoints) {
		return NULL;
	}

	Trackpoint * min_alt_tp = NULL;
	double minalt = 25000.0;

	for (GList * iter = this->trackpoints; iter; iter = iter->next) {
		if (((Trackpoint *) iter->data)->altitude < minalt) {
			minalt = ((Trackpoint *) iter->data)->altitude;
			min_alt_tp = ((Trackpoint *) iter->data);
		}
	}

	if (!min_alt_tp) {
		return NULL;
	}

	return min_alt_tp;
}





Trackpoint * Track::get_tp_first()
{
	if (!this->trackpoints) {
		return NULL;
	}

	return (Trackpoint *) g_list_first(this->trackpoints)->data;
}





Trackpoint * Track::get_tp_last()
{
	if (!this->trackpoints) {
		return NULL;
	}

	return (Trackpoint *) g_list_last(this->trackpoints)->data;
}





Trackpoint * Track::get_tp_prev(Trackpoint * tp)
{
	if (!this->trackpoints) {
		return NULL;
	}

	Trackpoint * tp_prev = NULL;

	for (GList * iter = this->trackpoints; iter; iter = iter->next) {
		if (iter->prev) {
			if (((Trackpoint *) iter->data) == tp) {
				tp_prev = ((Trackpoint *) iter->prev->data);
				break;
			}
		}
	}

	return tp_prev;
}

bool Track::get_minmax_alt(double * min_alt, double * max_alt)
{
	*min_alt = 25000;
	*max_alt = -5000;
	if (this->trackpoints && this->trackpoints->data && (((Trackpoint *) this->trackpoints->data)->altitude != VIK_DEFAULT_ALTITUDE)) {
		double tmp_alt;
		for (GList * iter = this->trackpoints->next; iter; iter = iter->next) {
			tmp_alt = ((Trackpoint *) iter->data)->altitude;
			if (tmp_alt > *max_alt) {
				*max_alt = tmp_alt;
			}

			if (tmp_alt < *min_alt) {
				*min_alt = tmp_alt;
			}
		}
		return true;
	}
	return false;
}

void Track::marshall(uint8_t **data, size_t *datalen)
{
	GByteArray * b = g_byte_array_new();
	g_byte_array_append(b, (uint8_t *) this, sizeof(Track));

	/* we'll fill out number of trackpoints later */
	unsigned int intp = b->len;
	unsigned int len;
	g_byte_array_append(b, (uint8_t *)&len, sizeof(len));


	// This allocates space for variant sized strings
	//  and copies that amount of data from the track to byte array
#define vtm_append(s)	       \
	len = (s) ? strlen(s)+1 : 0;			\
	g_byte_array_append(b, (uint8_t *) &len, sizeof(len));	\
	if (s) g_byte_array_append(b, (uint8_t *) s, len);

	GList * tps = this->trackpoints;
	unsigned int ntp = 0;
	while (tps) {
		g_byte_array_append(b, (uint8_t *)tps->data, sizeof(Trackpoint));
		vtm_append(((Trackpoint *) tps->data)->name);
		tps = tps->next;
		ntp++;
	}
	*(unsigned int *)(b->data + intp) = ntp;

	vtm_append(this->name);
	vtm_append(this->comment);
	vtm_append(this->description);
	vtm_append(this->source);

	*data = b->data;
	*datalen = b->len;
	g_byte_array_free(b, false);
}

/*
 * Take a byte array and convert it into a Track
 */
Track * Track::unmarshall(uint8_t *data, size_t datalen)
{
	Track * new_trk = new Track();
	/* basic properties: */
	new_trk->visible = ((Track *)data)->visible;
	new_trk->is_route = ((Track *)data)->is_route;
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

	Trackpoint * new_tp;
	for (int i = 0; i < ntp; i++) {
		new_tp = new Trackpoint();
		memcpy(new_tp, data, sizeof(*new_tp));
		data += sizeof(*new_tp);
		vtu_get(new_tp->name);
		new_trk->trackpoints = g_list_prepend(new_trk->trackpoints, new_tp);
	}
	if (new_trk->trackpoints) {
		new_trk->trackpoints = g_list_reverse(new_trk->trackpoints);
	}

	vtu_get(new_trk->name);
	vtu_get(new_trk->comment);
	vtu_get(new_trk->description);
	vtu_get(new_trk->source);

	return new_trk;
}

/**
 * (Re)Calculate the bounds of the given track,
 *  updating the track's bounds data.
 * This should be called whenever a track's trackpoints are changed
 */
void Track::calculate_bounds()
{
	struct LatLon topleft, bottomright, ll;

	/* Set bounds to first point. */
	GList * iter = this->trackpoints;
	if (iter) {
		vik_coord_to_latlon (&(((Trackpoint *) iter->data)->coord), &topleft);
		vik_coord_to_latlon (&(((Trackpoint *) iter->data)->coord), &bottomright);
	}

	for (; iter; iter = iter->next) {

		/* See if this trackpoint increases the track bounds. */

		vik_coord_to_latlon(&(((Trackpoint *) iter->data)->coord), &ll);

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

	fprintf(stderr, "DEBUG: Bounds of track: '%s' is: %f,%f to: %f,%f\n", name, topleft.lat, topleft.lon, bottomright.lat, bottomright.lon);

	bbox.north = topleft.lat;
	bbox.east = bottomright.lon;
	bbox.south = bottomright.lat;
	bbox.west = topleft.lon;
}

/**
 * vik_track_anonymize_times:
 *
 * Shift all timestamps to be relatively offset from 1901-01-01
 */
void Track::anonymize_times()
{
	GTimeVal gtv;
	// Check result just to please Coverity - even though it shouldn't fail as it's a hard coded value here!
	if (!g_time_val_from_iso8601 ("1901-01-01T00:00:00Z", &gtv)) {
		fprintf(stderr, "CRITICAL: Calendar time value failure\n");
		return;
	}

	time_t anon_timestamp = gtv.tv_sec;
	time_t offset = 0;

	for (GList * iter = this->trackpoints; iter; iter = iter->next) {
		Trackpoint * tp = (Trackpoint *) (iter->data);
		if (tp->has_timestamp) {
			// Calculate an offset in time using the first available timestamp
			if (offset == 0) {
				offset = tp->timestamp - anon_timestamp;
			}

			// Apply this offset to shift all timestamps towards 1901 & hence anonymising the time
			// Note that the relative difference between timestamps is kept - thus calculating speeds will still work
			tp->timestamp = tp->timestamp - offset;
		}
	}
}

/**
 * vik_track_interpolate_times:
 *
 * Interpolate the timestamps between first and last trackpoint,
 * so that the track is driven at equal speed, regardless of the
 * distance between individual trackpoints.
 *
 * NB This will overwrite any existing trackpoint timestamps
 */
void Track::interpolate_times()
{
	GList * iter = this->trackpoints;
	Trackpoint * tp = (Trackpoint *) (iter->data);
	if (!tp->has_timestamp) {
		return;
	}

	time_t tsfirst = tp->timestamp;

	/* Find the end of the track and the last timestamp. */
	while (iter->next) {
		iter = iter->next;
	}
	tp = ((Trackpoint *) iter->data);
	if (tp->has_timestamp) {
		time_t tsdiff = tp->timestamp - tsfirst;

		double tr_dist = this->get_length_including_gaps();
		double cur_dist = 0.0;

		if (tr_dist > 0) {
			iter = this->trackpoints;
			/* Apply the calculated timestamp to all trackpoints except the first and last ones. */
			while (iter->next && iter->next->next) {
				iter = iter->next;
				tp = ((Trackpoint *) iter->data);
				cur_dist += vik_coord_diff(&(tp->coord), &(((Trackpoint *) iter->prev->data)->coord));

				tp->timestamp = (cur_dist / tr_dist) * tsdiff + tsfirst;
				tp->has_timestamp = true;
			}
			/* Some points may now have the same time so remove them. */
			this->remove_same_time_points();
		}
	}
}

/**
 * vik_track_apply_dem_data:
 * @skip_existing: When true, don't change the elevation if the trackpoint already has a value
 *
 * Set elevation data for a track using any available DEM information
 */
unsigned long Track::apply_dem_data(bool skip_existing)
{
	unsigned long num = 0;

	for (GList * iter = this->trackpoints; iter; iter = iter->next) {
		/* Don't apply if the point already has a value and the overwrite is off. */
		if (!(skip_existing && ((Trackpoint *) iter->data)->altitude != VIK_DEFAULT_ALTITUDE)) {
			/* TODO: of the 4 possible choices we have for choosing an
			   elevation (trackpoint in between samples), choose the one
			   with the least elevation change as the last. */
			int16_t elev = dem_cache_get_elev_by_coord (&(((Trackpoint *) iter->data)->coord), VIK_DEM_INTERPOL_BEST);
			if (elev != VIK_DEM_INVALID_ELEVATION) {
				((Trackpoint *) iter->data)->altitude = elev;
				num++;
			}
		}
	}
	return num;
}

/**
 * vik_track_apply_dem_data_last_trackpoint:
 * Apply DEM data (if available) - to only the last trackpoint
 */
void Track::apply_dem_data_last_trackpoint()
{
	if (this->trackpoints) {
		/* As in vik_track_apply_dem_data above - use 'best' interpolation method */
		int16_t elev = dem_cache_get_elev_by_coord (&(((Trackpoint *) g_list_last(this->trackpoints)->data)->coord), VIK_DEM_INTERPOL_BEST);
		if (elev != VIK_DEM_INVALID_ELEVATION) {
			((Trackpoint *) g_list_last(this->trackpoints)->data)->altitude = elev;
		}
	}
}


/**
 * smoothie:
 *
 * Apply elevation smoothing over range of trackpoints between the list start and end points
 */
void Track::smoothie(GList * start, GList * stop, double elev1, double elev2, unsigned int points)
{
	/* If was really clever could try and weigh interpolation according to the distance between trackpoints somehow.
	   Instead a simple average interpolation for the number of points given. */
	double change = (elev2 - elev1)/(points+1);
	int count = 1;
	GList *iter = start;
	while (iter != stop && iter) {
		Trackpoint * tp = (Trackpoint *) (iter->data);

		tp->altitude = elev1 + (change * count);

		count++;
		iter = iter->next;
	}
}

/**
 * vik_track_smooth_missing_elevation_data:
 * @flat: Specify how the missing elevations will be set.
 *        When true it uses a simple flat method, using the last known elevation
 *        When false is uses an interpolation method to the next known elevation
 *
 * For each point with a missing elevation, set it to use the last known available elevation value.
 * Primarily of use for smallish DEM holes where it is missing elevation data.
 * Eg see Austria: around N47.3 & E13.8
 *
 * Returns: The number of points that were adjusted
 */
unsigned long Track::smooth_missing_elevation_data(bool flat)
{
	unsigned long num = 0;
	double elev = VIK_DEFAULT_ALTITUDE;

	Trackpoint * tp_missing = NULL;
	GList *iter_first = NULL;
	unsigned int points = 0;

	for (GList * iter = this->trackpoints; iter; iter = iter->next) {
		Trackpoint * tp = (Trackpoint *) (iter->data);

		if (VIK_DEFAULT_ALTITUDE == tp->altitude) {
			if (flat) {
				// Simply assign to last known value
				if (elev != VIK_DEFAULT_ALTITUDE) {
					tp->altitude = elev;
					num++;
				}
			} else {
				if (!tp_missing) {
					// Remember the first trackpoint (and the list pointer to it) of a section of no altitudes
					tp_missing = tp;
					iter_first = iter;
					points = 1;
				} else {
					// More missing altitudes
					points++;
				}
			}
		} else {
			// Altitude available (maybe again!)
			// If this marks the end of a section of altitude-less points
			//  then apply smoothing for that section of points
			if (points > 0 && elev != VIK_DEFAULT_ALTITUDE) {
				if (!flat) {
					smoothie(iter_first, iter, elev, tp->altitude, points);
					num = num + points;
				}
			}

			// reset
			points = 0;
			tp_missing = NULL;

			// Store for reuse as the last known good value
			elev = tp->altitude;
		}
	}

	return num;
}

/**
 * vik_track_steal_and_append_trackpoints:
 *
 * appends 'from' to track, leaving 'from' with no trackpoints
 */
void Track::steal_and_append_trackpoints(Track * from)
{
	if (this->trackpoints) {
		this->trackpoints = g_list_concat(this->trackpoints, from->trackpoints);
	} else {
		this->trackpoints = from->trackpoints;
	}
	from->trackpoints = NULL;

	// Trackpoints updated - so update the bounds
	this->calculate_bounds();
}

/**
 * vik_track_cut_back_to_double_point:
 *
 * starting at the end, looks backwards for the last "double point", a duplicate trackpoint.
 * If there is no double point, deletes all the trackpoints.
 *
 * Returns: the new end of the track (or the start if there are no double points)
 */
VikCoord * Track::cut_back_to_double_point()
{
	if (!this->trackpoints) {
		return NULL;
	}

	GList *iter = this->trackpoints;
	while (iter->next) {
		iter = iter->next;
	}

	VikCoord *rv;
	while (iter->prev) {
		VikCoord *cur_coord = &((Trackpoint*)iter->data)->coord;
		VikCoord *prev_coord = &((Trackpoint*)iter->prev->data)->coord;
		if (vik_coord_equals(cur_coord, prev_coord)) {
			GList *prev = iter->prev;

			rv = (VikCoord *) malloc(sizeof (VikCoord));
			*rv = *cur_coord;

			/* truncate trackpoint list */
			iter->prev = NULL; /* pretend it's the end */
			g_list_foreach (iter, (GFunc) g_free, NULL);
			g_list_free(iter);

			prev->next = NULL;

			return rv;
		}
		iter = iter->prev;
	}

	/* no double point found! */
	rv = (VikCoord *) malloc(sizeof (VikCoord));
	*rv = ((Trackpoint*) this->trackpoints->data)->coord;
	g_list_foreach (this->trackpoints, (GFunc) g_free, NULL);
	g_list_free(this->trackpoints);
	this->trackpoints = NULL;
	return rv;
}

/**
 * Function to compare two tracks by their first timestamp
 **/
int Track::compare_timestamp(const void * x, const void * y)
{
	Track * a = (Track *) x;
	Track * b = (Track *) y;

	Trackpoint * tpa = NULL;
	Trackpoint * tpb = NULL;

	if (a->trackpoints) {
		tpa = ((Trackpoint *) g_list_first(a->trackpoints)->data);
	}

	if (b->trackpoints) {
		tpb = ((Trackpoint *) g_list_first(b->trackpoints)->data);
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





void Track::delete_track(Track * trk)
{
	trk->free();
	return;
}
