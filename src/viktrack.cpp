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
#include <stdlib.h>
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

Track * vik_track_new()
{
	Track * trk = (Track *) malloc(sizeof (Track));
	memset(trk, 0, sizeof (Track));

	trk->ref_count = 1;
	return trk;
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
void vik_track_set_defaults(Track * trk)
{
	int tmp;
	if (a_settings_get_integer (VIK_SETTINGS_TRACK_NAME_MODE, &tmp))
		trk->draw_name_mode = (VikTrackDrawnameType) tmp;

	if (a_settings_get_integer (VIK_SETTINGS_TRACK_NUM_DIST_LABELS, &tmp))
		trk->max_number_dist_labels = (VikTrackDrawnameType) tmp;
}

void vik_track_set_comment_no_copy(Track * trk, char *comment)
{
	if (trk->comment)
		free(trk->comment);
	trk->comment = comment;
}


void vik_track_set_name(Track * trk, const char *name)
{
	if (trk->name)
		free(trk->name);

	trk->name = g_strdup(name);
}

void vik_track_set_comment(Track * trk, const char *comment)
{
	if (trk->comment)
		free(trk->comment);

	if (comment && comment[0] != '\0')
		trk->comment = g_strdup(comment);
	else
		trk->comment = NULL;
}

void vik_track_set_description(Track * trk, const char *description)
{
	if (trk->description)
		free(trk->description);

	if (description && description[0] != '\0')
		trk->description = g_strdup(description);
	else
		trk->description = NULL;
}

void vik_track_set_source(Track * trk, const char *source)
{
	if (trk->source)
		free(trk->source);

	if (source && source[0] != '\0')
		trk->source = g_strdup(source);
	else
		trk->source = NULL;
}

void vik_track_set_type(Track * trk, const char *type)
{
	if (trk->type)
		free(trk->type);

	if (type && type[0] != '\0')
		trk->type = g_strdup(type);
	else
		trk->type = NULL;
}

void vik_track_ref(Track * trk)
{
	trk->ref_count++;
}

void vik_track_set_property_dialog(Track * trk, GtkWidget *dialog)
{
	/* Warning: does not check for existing dialog */
	trk->property_dialog = dialog;
}

void vik_track_clear_property_dialog(Track * trk)
{
	trk->property_dialog = NULL;
}

void vik_track_free(Track * trk)
{
	if (trk->ref_count-- > 1)
		return;

	if (trk->name)
		free(trk->name);
	if (trk->comment)
		free(trk->comment);
	if (trk->description)
		free(trk->description);
	if (trk->source)
		free(trk->source);
	if (trk->type)
		free(trk->type);
	g_list_foreach (trk->trackpoints, (GFunc) (Trackpoint::vik_trackpoint_free), NULL);
	g_list_free(trk->trackpoints);
	if (trk->property_dialog)
		if (GTK_IS_WIDGET(trk->property_dialog))
			gtk_widget_destroy (GTK_WIDGET(trk->property_dialog));
	free(trk);
}

/**
 * vik_track_copy:
 * @tr: The Track to copy
 * @copy_points: Whether to copy the track points or not
 *
 * Normally for copying the track it's best to copy all the trackpoints
 * However for some operations such as splitting tracks the trackpoints will be managed separately, so no need to copy them.
 *
 * Returns: the copied Track
 */
Track *vik_track_copy (const Track * trk, bool copy_points)
{
	Track *new_trk = vik_track_new();
	new_trk->name = g_strdup(trk->name);
	new_trk->visible = trk->visible;
	new_trk->is_route = trk->is_route;
	new_trk->draw_name_mode = trk->draw_name_mode;
	new_trk->max_number_dist_labels = trk->max_number_dist_labels;
	new_trk->has_color = trk->has_color;
	new_trk->color = trk->color;
	new_trk->bbox = trk->bbox;
	new_trk->trackpoints = NULL;
	if (copy_points) {
		GList *tp_iter = trk->trackpoints;
		while (tp_iter) {
			Trackpoint * tp = (Trackpoint *) tp_iter->data;
			Trackpoint * new_tp = new Trackpoint(*tp);
			new_trk->trackpoints = g_list_prepend (new_trk->trackpoints, new_tp);
			tp_iter = tp_iter->next;
		}
		if (new_trk->trackpoints)
			new_trk->trackpoints = g_list_reverse (new_trk->trackpoints);
	}
	vik_track_set_name(new_trk,trk->name);
	vik_track_set_comment(new_trk,trk->comment);
	vik_track_set_description(new_trk,trk->description);
	vik_track_set_source(new_trk,trk->source);
	return new_trk;
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
		name = strdup(name_);
	} else {
		name = NULL;
	}
}

Trackpoint::Trackpoint(const Trackpoint & tp)
{
	if (this->name) {
		this->name = strdup(tp.name);
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
static void track_recalculate_bounds_last_tp (Track * trk)
{
	GList *tpl = g_list_last (trk->trackpoints);

	if (tpl) {
		struct LatLon ll;
		// See if this trackpoint increases the track bounds and update if so
		vik_coord_to_latlon (&(((Trackpoint *) tpl->data)->coord), &ll);
		if (ll.lat > trk->bbox.north)
			trk->bbox.north = ll.lat;
		if (ll.lon < trk->bbox.west)
			trk->bbox.west = ll.lon;
		if (ll.lat < trk->bbox.south)
			trk->bbox.south = ll.lat;
		if (ll.lon > trk->bbox.east)
			trk->bbox.east = ll.lon;
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
void vik_track_add_trackpoint (Track * trk, Trackpoint * tp, bool recalculate)
{
	// When it's the first trackpoint need to ensure the bounding box is initialized correctly
	bool adding_first_point = trk->trackpoints ? false : true;
	trk->trackpoints = g_list_append (trk->trackpoints, tp);
	if (adding_first_point)
		vik_track_calculate_bounds(trk);
	else if (recalculate)
		track_recalculate_bounds_last_tp(trk);
}

/**
 * vik_track_get_length_to_trackpoint:
 *
 */
double vik_track_get_length_to_trackpoint (const Track * trk, const Trackpoint * tp)
{
	double len = 0.0;
	if (trk->trackpoints) {
		// Is it the very first track point?
		if (((Trackpoint *) trk->trackpoints->data) == tp)
			return len;

		GList *iter = trk->trackpoints->next;
		while (iter) {
			Trackpoint * tp1 = ((Trackpoint *) iter->data);
			if (! tp1->newsegment)
				len += vik_coord_diff (&(tp1->coord),
							&(((Trackpoint *) iter->prev->data)->coord));

			// Exit when we reach the desired point
			if (tp1 == tp)
				break;

			iter = iter->next;
		}
	}
	return len;
}

double vik_track_get_length(const Track * trk)
{
	double len = 0.0;
	if (trk->trackpoints) {
		GList *iter = trk->trackpoints->next;
		while (iter)  {
			if (! ((Trackpoint *) iter->data)->newsegment)
				len += vik_coord_diff (&(((Trackpoint *) iter->data)->coord),
							&(((Trackpoint *) iter->prev->data)->coord));
			iter = iter->next;
		}
	}
	return len;
}

double vik_track_get_length_including_gaps(const Track * trk)
{
	double len = 0.0;
	if (trk->trackpoints)	{
		GList *iter = trk->trackpoints->next;
		while (iter) {
			len += vik_coord_diff (&(((Trackpoint *) iter->data)->coord),
						&(((Trackpoint *) iter->prev->data)->coord));
			iter = iter->next;
		}
	}
	return len;
}

unsigned long vik_track_get_tp_count(const Track * trk)
{
	return g_list_length(trk->trackpoints);
}

unsigned long vik_track_get_dup_point_count(const Track * trk)
{
	unsigned long num = 0;
	GList *iter = trk->trackpoints;
	while (iter) {
		if (iter->next && vik_coord_equals (&(((Trackpoint *) iter->data)->coord),
						      &(((Trackpoint *) iter->next->data)->coord)))
			num++;
		iter = iter->next;
	}
	return num;
}

/*
 * Deletes adjacent points that have the same position
 * Returns the number of points that were deleted
 */
unsigned long vik_track_remove_dup_points(Track * trk)
{
	unsigned long num = 0;
	GList *iter = trk->trackpoints;
	while (iter) {
		if (iter->next && vik_coord_equals (&(((Trackpoint *) iter->data)->coord),
						      &(((Trackpoint *) iter->next->data)->coord))) {
			num++;
			// Maintain track segments
			if (((Trackpoint *) iter->next->data)->newsegment && (iter->next)->next)
				((Trackpoint *) ((iter->next)->next)->data)->newsegment = true;

			delete ((Trackpoint *) iter->next->data);
			trk->trackpoints = g_list_delete_link (trk->trackpoints, iter->next);
		}
		else
			iter = iter->next;
	}

	// NB isn't really be necessary as removing duplicate points shouldn't alter the bounds!
	vik_track_calculate_bounds(trk);

	return num;
}

/*
 * Get a count of trackpoints with the same defined timestamp
 * Note is using timestamps with a resolution with 1 second
 */
unsigned long vik_track_get_same_time_point_count(const Track * trk)
{
	unsigned long num = 0;
	GList *iter = trk->trackpoints;
	while (iter) {
		if (iter->next &&
		     (((Trackpoint *) iter->data)->has_timestamp &&
		       ((Trackpoint *) iter->next->data)->has_timestamp) &&
		     (((Trackpoint *) iter->data)->timestamp ==
		       ((Trackpoint *) iter->next->data)->timestamp))
			num++;
		iter = iter->next;
	}
	return num;
}

/*
 * Deletes adjacent points that have the same defined timestamp
 * Returns the number of points that were deleted
 */
unsigned long vik_track_remove_same_time_points(Track * trk)
{
	unsigned long num = 0;
	GList *iter = trk->trackpoints;
	while (iter) {
		if (iter->next &&
		     (((Trackpoint *) iter->data)->has_timestamp &&
		       ((Trackpoint *) iter->next->data)->has_timestamp) &&
		     (((Trackpoint *) iter->data)->timestamp ==
		       ((Trackpoint *) iter->next->data)->timestamp)) {

			num++;

			// Maintain track segments
			if (((Trackpoint *) iter->next->data)->newsegment && (iter->next)->next)
				((Trackpoint *) ((iter->next)->next)->data)->newsegment = true;

			delete ((Trackpoint *) iter->next->data);
			trk->trackpoints = g_list_delete_link (trk->trackpoints, iter->next);
		}
		else
			iter = iter->next;
	}

	vik_track_calculate_bounds(trk);

	return num;
}

/*
 * Deletes all 'extra' trackpoint information
 *  such as time stamps, speed, course etc...
 */
void vik_track_to_routepoints(Track * trk)
{
	GList *iter = trk->trackpoints;
	while (iter) {

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

		iter = iter->next;
	}
}

unsigned int vik_track_get_segment_count(const Track * trk)
{
	unsigned int num = 1;
	GList *iter = trk->trackpoints;
	if (!iter)
		return 0;
	while ((iter = iter->next)) {
		if (((Trackpoint *) iter->data)->newsegment)
			num++;
	}
	return num;
}

/* kamilTODO: revisit this function and compare with original. */
Track ** vik_track_split_into_segments(Track * trk, unsigned int *ret_len)
{
	unsigned int i;
	unsigned int segs = vik_track_get_segment_count(trk);
	GList *iter;

	if (segs < 2) {
		*ret_len = 0;
		return NULL;
	}

	Track ** rv = (Track **) malloc(segs * sizeof (Track *));
	Track * trk_copy = vik_track_copy(trk, true);
	rv[0] = trk_copy;
	iter = trk_copy->trackpoints;

	i = 1;
	while ((iter = iter->next)) {
		if (((Trackpoint *) iter->data)->newsegment) {
			iter->prev->next = NULL;
			iter->prev = NULL;
			rv[i] = vik_track_copy(trk_copy, false);
			rv[i]->trackpoints = iter;

			vik_track_calculate_bounds (rv[i]);

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
unsigned int vik_track_merge_segments(Track * trk)
{
	unsigned int num = 0;
	GList *iter = trk->trackpoints;
	if (!iter)
		return num;

	// Always skip the first point as this should be the first segment
	iter = iter->next;

	while ((iter = iter->next)) {
		if (((Trackpoint *) iter->data)->newsegment) {
			((Trackpoint *) iter->data)->newsegment = false;
			num++;
		}
	}
	return num;
}

void vik_track_reverse(Track * trk)
{
	if (! trk->trackpoints)
		return;

	trk->trackpoints = g_list_reverse(trk->trackpoints);

	/* fix 'newsegment' */
	GList *iter = g_list_last (trk->trackpoints);
	while (iter) {
		if (! iter->next) /* last segment, was first, cancel newsegment. */
			((Trackpoint *) iter->data)->newsegment = false;
		if (! iter->prev) /* first segment by convention has newsegment flag. */
			((Trackpoint *) iter->data)->newsegment = true;
		else if (((Trackpoint *) iter->data)->newsegment && iter->next)
			{
				((Trackpoint *) iter->next->data)->newsegment = true;
				((Trackpoint *) iter->data)->newsegment = false;
			}
		iter = iter->prev;
	}
}

/**
 * vik_track_get_duration:
 * @trk: The track
 * @segment_gaps: Whether the duration should include gaps between segments
 *
 * Returns: The time in seconds
 *  NB this may be negative particularly if the track has been reversed
 */
time_t vik_track_get_duration(const Track * trk, bool segment_gaps)
{
	time_t duration = 0;
	if (trk->trackpoints) {
		// Ensure times are available
		if (vik_track_get_tp_first(trk)->has_timestamp) {
			// Get trkpt only once - as using vik_track_get_tp_last() iterates whole track each time
			if (segment_gaps) {
				// Simple duration
				Trackpoint * trkpt_last = vik_track_get_tp_last(trk);
				if (trkpt_last->has_timestamp) {
					time_t t1 = vik_track_get_tp_first(trk)->timestamp;
					time_t t2 = trkpt_last->timestamp;
					duration = t2 - t1;
				}
			}
			else {
				// Total within segments
				GList *iter = trk->trackpoints->next;
				while (iter) {
					if (((Trackpoint *) iter->data)->has_timestamp &&
					     ((Trackpoint *) iter->prev->data)->has_timestamp &&
					     (!((Trackpoint *) (Trackpoint *) iter->data)->newsegment)) {
						duration += ABS(((Trackpoint *) iter->data)->timestamp - ((Trackpoint *) iter->prev->data)->timestamp);
					}
					iter = iter->next;
				}
			}
		}
	}
	return duration;
}

double vik_track_get_average_speed(const Track * trk)
{
	double len = 0.0;
	uint32_t time = 0;
	if (trk->trackpoints) {
		GList *iter = trk->trackpoints->next;
		while (iter)  {
			if (((Trackpoint *) iter->data)->has_timestamp &&
			     ((Trackpoint *) iter->prev->data)->has_timestamp &&
			     (! ((Trackpoint *) iter->data)->newsegment))
				{
					len += vik_coord_diff (&(((Trackpoint *) iter->data)->coord),
								&(((Trackpoint *) iter->prev->data)->coord));
					time += ABS(((Trackpoint *) iter->data)->timestamp - ((Trackpoint *) iter->prev->data)->timestamp);
				}
			iter = iter->next;
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
double vik_track_get_average_speed_moving (const Track * trk, int stop_length_seconds)
{
	double len = 0.0;
	uint32_t time = 0;
	if (trk->trackpoints) {
		GList *iter = trk->trackpoints->next;
		while (iter) {
			if (((Trackpoint *) iter->data)->has_timestamp &&
			     ((Trackpoint *) iter->prev->data)->has_timestamp &&
			     (! ((Trackpoint *) iter->data)->newsegment))
				{
					if ((((Trackpoint *) iter->data)->timestamp - ((Trackpoint *) iter->prev->data)->timestamp) < stop_length_seconds) {
						len += vik_coord_diff (&(((Trackpoint *) iter->data)->coord),
									&(((Trackpoint *) iter->prev->data)->coord));

						time += ABS(((Trackpoint *) iter->data)->timestamp - ((Trackpoint *) iter->prev->data)->timestamp);
					}
				}
			iter = iter->next;
		}
	}
	return (time == 0) ? 0 : ABS(len/time);
}

double vik_track_get_max_speed(const Track * trk)
{
	double maxspeed = 0.0, speed = 0.0;
	if (trk->trackpoints) {
		GList *iter = trk->trackpoints->next;
		while (iter) {
			if (((Trackpoint *) iter->data)->has_timestamp &&
			     ((Trackpoint *) iter->prev->data)->has_timestamp &&
			     (! ((Trackpoint *) iter->data)->newsegment))
				{
					speed =  vik_coord_diff (&(((Trackpoint *) iter->data)->coord), &(((Trackpoint *) iter->prev->data)->coord))
						/ ABS(((Trackpoint *) iter->data)->timestamp - ((Trackpoint *) iter->prev->data)->timestamp);
					if (speed > maxspeed)
						maxspeed = speed;
				}
			iter = iter->next;
		}
	}
	return maxspeed;
}

void vik_track_convert (Track * trk, VikCoordMode dest_mode)
{
	GList *iter = trk->trackpoints;
	while (iter) {
		vik_coord_convert (&(((Trackpoint *) iter->data)->coord), dest_mode);
		iter = iter->next;
	}
}

/* I understood this when I wrote it ... maybe ... Basically it eats up the
 * proper amounts of length on the track and averages elevation over that. */
double *vik_track_make_elevation_map (const Track * trk, uint16_t num_chunks)
{
	double total_length, chunk_length, current_dist, current_area_under_curve, current_seg_length, dist_along_seg = 0.0;
	double altitude1, altitude2;
	uint16_t current_chunk;
	bool ignore_it = false;

	GList *iter = trk->trackpoints;

	if (!iter || !iter->next) /* zero- or one-point track */
		return NULL;

	{ /* test if there's anything worth calculating */
		bool okay = false;
		while (iter) {
			// Sometimes a GPS device (or indeed any random file) can have stupid numbers for elevations
			// Since when is 9.9999e+24 a valid elevation!!
			// This can happen when a track (with no elevations) is uploaded to a GPS device and then redownloaded (e.g. using a Garmin Legend EtrexHCx)
			// Some protection against trying to work with crazily massive numbers (otherwise get SIGFPE, Arithmetic exception)
			if (((Trackpoint *) iter->data)->altitude != VIK_DEFAULT_ALTITUDE &&
			     ((Trackpoint *) iter->data)->altitude < 1E9) {
				okay = true; break;
			}
			iter = iter->next;
		}
		if (! okay)
			return NULL;
	}

	iter = trk->trackpoints;

	assert (num_chunks < 16000);

	double * pts = (double *) malloc(sizeof (double) * num_chunks);

	total_length = vik_track_get_length_including_gaps(trk);
	chunk_length = total_length / num_chunks;

	/* Zero chunk_length (eg, track of 2 tp with the same loc) will cause crash */
	if (chunk_length <= 0) {
		free(pts);
		return NULL;
	}

	current_dist = 0.0;
	current_area_under_curve = 0;
	current_chunk = 0;
	current_seg_length = 0;

	current_seg_length = vik_coord_diff (&(((Trackpoint *) iter->data)->coord),
					      &(((Trackpoint *) iter->next->data)->coord));
	altitude1 = ((Trackpoint *) iter->data)->altitude;
	altitude2 = ((Trackpoint *) iter->next->data)->altitude;
	dist_along_seg = 0;

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

			if (ignore_it)
				// Seemly can't determine average for this section - so use last known good value (much better than just sticking in zero)
				pts[current_chunk] = altitude1;
			else
				pts[current_chunk] = altitude1 + (altitude2-altitude1)*((dist_along_seg - (chunk_length/2))/current_seg_length);

			current_chunk++;
		} else {
			/* finish current seg */
			if (current_seg_length) {
				double altitude_at_dist_along_seg = altitude1 + (altitude2-altitude1)/(current_seg_length)*dist_along_seg;
				current_dist = current_seg_length - dist_along_seg;
				current_area_under_curve = current_dist*(altitude_at_dist_along_seg + altitude2)*0.5;
			} else { current_dist = current_area_under_curve = 0; } /* should only happen if first current_seg_length == 0 */

			/* get intervening segs */
			iter = iter->next;
			while (iter && iter->next) {
				current_seg_length = vik_coord_diff (&(((Trackpoint *) iter->data)->coord),
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
					for (i = current_chunk + 1; i < num_chunks; i++)
						pts[i] = pts[current_chunk];
					break;
				}
			}
			else {
				current_area_under_curve += dist_along_seg * (altitude1 + (altitude2 - altitude1)*dist_along_seg/current_seg_length);
				pts[current_chunk] = current_area_under_curve / chunk_length;
			}

			current_dist = 0;
			current_chunk++;
		}
	}

	return pts;
}


void vik_track_get_total_elevation_gain(const Track * trk, double *up, double *down)
{
	double diff;
	*up = *down = 0;
	if (trk->trackpoints && ((Trackpoint *) trk->trackpoints->data)->altitude != VIK_DEFAULT_ALTITUDE) {
		GList *iter = trk->trackpoints->next;
		while (iter) {
			diff = ((Trackpoint *) iter->data)->altitude - ((Trackpoint *) iter->prev->data)->altitude;
			if (diff > 0)
				*up += diff;
			else
				*down -= diff;
			iter = iter->next;
		}
	} else
		*up = *down = VIK_DEFAULT_ALTITUDE;
}

double *vik_track_make_gradient_map (const Track * trk, uint16_t num_chunks)
{
	double *altitudes;
	double total_length, chunk_length, current_gradient;
	double altitude1, altitude2;
	uint16_t current_chunk;

	assert (num_chunks < 16000);

	total_length = vik_track_get_length_including_gaps(trk);
	chunk_length = total_length / num_chunks;

	/* Zero chunk_length (eg, track of 2 tp with the same loc) will cause crash */
	if (chunk_length <= 0) {
		return NULL;
	}

	altitudes = vik_track_make_elevation_map(trk, num_chunks);
	if (altitudes == NULL) {
		return NULL;
	}

	current_gradient = 0.0;
	double * pts = (double *) malloc(sizeof (double) * num_chunks);
	for (current_chunk = 0; current_chunk < (num_chunks - 1); current_chunk++) {
		altitude1 = altitudes[current_chunk];
		altitude2 = altitudes[current_chunk + 1];
		current_gradient = 100.0 * (altitude2 - altitude1) / chunk_length;

		pts[current_chunk] = current_gradient;
	}

	pts[current_chunk] = current_gradient;

	free(altitudes);

	return pts;
}

/* by Alex Foobarian */
double *vik_track_make_speed_map (const Track * trk, uint16_t num_chunks)
{
	double duration, chunk_dur;
	time_t t1, t2;
	int i, pt_count, numpts, index;
	GList *iter;

	if (! trk->trackpoints)
		return NULL;

	assert (num_chunks < 16000);

	t1 = ((Trackpoint *) trk->trackpoints->data)->timestamp;
	t2 = ((Trackpoint *) g_list_last(trk->trackpoints)->data)->timestamp;
	duration = t2 - t1;

	if (!t1 || !t2 || !duration)
		return NULL;

	if (duration < 0) {
		fprintf(stderr, "WARNING: negative duration: unsorted trackpoint timestamps?\n");
		return NULL;
	}
	pt_count = vik_track_get_tp_count(trk);

	double * v = (double *) malloc(sizeof (double) * num_chunks);
	chunk_dur = duration / num_chunks;

	double * s = (double *) malloc(sizeof (double) * pt_count);
	double * t = (double *) malloc(sizeof (double) * pt_count);

	iter = trk->trackpoints->next;
	numpts = 0;
	s[0] = 0;
	t[0] = ((Trackpoint *) trk->trackpoints->data)->timestamp;
	numpts++;
	while (iter) {
		s[numpts] = s[numpts-1] + vik_coord_diff (&(((Trackpoint *) iter->prev->data)->coord), &(((Trackpoint *) iter->data)->coord));
		t[numpts] = ((Trackpoint *) iter->data)->timestamp;
		numpts++;
		iter = iter->next;
	}

	/* In the following computation, we iterate through periods of time of duration chunk_dur.
	 * The first period begins at the beginning of the track.  The last period ends at the end of the track.
	 */
	index = 0; /* index of the current trackpoint. */
	for (i = 0; i < num_chunks; i++) {
		/* we are now covering the interval from t[0] + i*chunk_dur to t[0] + (i+1)*chunk_dur.
		 * find the first trackpoint outside the current interval, averaging the speeds between intermediate trackpoints.
		 */
		if (t[0] + i*chunk_dur >= t[index]) {
			double acc_t = 0, acc_s = 0;
			while (t[0] + i*chunk_dur >= t[index]) {
				acc_s += (s[index+1]-s[index]);
				acc_t += (t[index+1]-t[index]);
				index++;
			}
			v[i] = acc_s/acc_t;
		}
		else if (i) {
			v[i] = v[i-1];
		}
		else {
			v[i] = 0;
		}
	}
	free(s);
	free(t);
	return v;
}

/**
 * Make a distance/time map, heavily based on the vik_track_make_speed_map method
 */
double *vik_track_make_distance_map (const Track * trk, uint16_t num_chunks)
{
	double duration, chunk_dur;
	time_t t1, t2;
	int i, pt_count, numpts, index;
	GList *iter;

	if (! trk->trackpoints)
		return NULL;

	t1 = ((Trackpoint *) trk->trackpoints->data)->timestamp;
	t2 = ((Trackpoint *) g_list_last(trk->trackpoints)->data)->timestamp;
	duration = t2 - t1;

	if (!t1 || !t2 || !duration)
		return NULL;

	if (duration < 0) {
		fprintf(stderr, "WARNING: negative duration: unsorted trackpoint timestamps?\n");
		return NULL;
	}
	pt_count = vik_track_get_tp_count(trk);

	double *v = (double *) malloc(sizeof (double) * num_chunks);
	chunk_dur = duration / num_chunks;

	double *s = (double *) malloc(sizeof (double) * pt_count);
	double *t = (double *) malloc(sizeof (double) * pt_count);

	iter = trk->trackpoints->next;
	numpts = 0;
	s[0] = 0;
	t[0] = ((Trackpoint *) trk->trackpoints->data)->timestamp;
	numpts++;
	while (iter) {
		s[numpts] = s[numpts-1] + vik_coord_diff (&(((Trackpoint *) iter->prev->data)->coord), &(((Trackpoint *) iter->data)->coord));
		t[numpts] = ((Trackpoint *) iter->data)->timestamp;
		numpts++;
		iter = iter->next;
	}

	/* In the following computation, we iterate through periods of time of duration chunk_dur.
	 * The first period begins at the beginning of the track.  The last period ends at the end of the track.
	 */
	index = 0; /* index of the current trackpoint. */
	for (i = 0; i < num_chunks; i++) {
		/* we are now covering the interval from t[0] + i*chunk_dur to t[0] + (i+1)*chunk_dur.
		 * find the first trackpoint outside the current interval, averaging the distance between intermediate trackpoints.
		 */
		if (t[0] + i*chunk_dur >= t[index]) {
			double acc_s = 0; // No need for acc_t
			while (t[0] + i*chunk_dur >= t[index]) {
				acc_s += (s[index+1]-s[index]);
				index++;
			}
			// The only bit that's really different from the speed map - just keep an accululative record distance
			v[i] = i ? v[i-1]+acc_s : acc_s;
		}
		else if (i) {
			v[i] = v[i-1];
		}
		else {
			v[i] = 0;
		}
	}
	free(s);
	free(t);
	return v;
}

/**
 * This uses the 'time' based method to make the graph, (which is a simpler compared to the elevation/distance)
 * This results in a slightly blocky graph when it does not have many trackpoints: <60
 * NB Somehow the elevation/distance applies some kind of smoothing algorithm,
 *   but I don't think any one understands it any more (I certainly don't ATM)
 */
double *vik_track_make_elevation_time_map (const Track * trk, uint16_t num_chunks)
{
	time_t t1, t2;
	double duration, chunk_dur;
	GList *iter = trk->trackpoints;

	if (!iter || !iter->next) /* zero- or one-point track */
		return NULL;

	/* test if there's anything worth calculating */
	bool okay = false;
	while (iter) {
		if (((Trackpoint *) iter->data)->altitude != VIK_DEFAULT_ALTITUDE) {
			okay = true;
			break;
		}
		iter = iter->next;
	}
	if (! okay)
		return NULL;

	t1 = ((Trackpoint *) trk->trackpoints->data)->timestamp;
	t2 = ((Trackpoint *) g_list_last(trk->trackpoints)->data)->timestamp;
	duration = t2 - t1;

	if (!t1 || !t2 || !duration)
		return NULL;

	if (duration < 0) {
		fprintf(stderr, "WARNING: negative duration: unsorted trackpoint timestamps?\n");
		return NULL;
	}
	int pt_count = vik_track_get_tp_count(trk);

	// Reset iterator back to the beginning
	iter = trk->trackpoints;

	double *pts = (double *) malloc(sizeof(double) * num_chunks); // The return altitude values
	double *s = (double *) malloc(sizeof(double) * pt_count); // calculation altitudes
	double *t = (double *) malloc(sizeof(double) * pt_count); // calculation times

	chunk_dur = duration / num_chunks;

	s[0] = ((Trackpoint *) iter->data)->altitude;
	t[0] = ((Trackpoint *) iter->data)->timestamp;
	iter = trk->trackpoints->next;
	int numpts = 1;
	while (iter) {
		s[numpts] = ((Trackpoint *) iter->data)->altitude;
		t[numpts] = ((Trackpoint *) iter->data)->timestamp;
		numpts++;
		iter = iter->next;
	}

	/* In the following computation, we iterate through periods of time of duration chunk_dur.
	 * The first period begins at the beginning of the track.  The last period ends at the end of the track.
	 */
	int index = 0; /* index of the current trackpoint. */
	int i;
	for (i = 0; i < num_chunks; i++) {
		/* we are now covering the interval from t[0] + i*chunk_dur to t[0] + (i+1)*chunk_dur.
		 * find the first trackpoint outside the current interval, averaging the heights between intermediate trackpoints.
		 */
		if (t[0] + i*chunk_dur >= t[index]) {
			double acc_s = s[index]; // initialise to first point
			while (t[0] + i*chunk_dur >= t[index]) {
				acc_s += (s[index+1]-s[index]);
				index++;
			}
			pts[i] = acc_s;
		}
		else if (i) {
			pts[i] = pts[i-1];
		}
		else {
			pts[i] = 0;
		}
	}
	free(s);
	free(t);

	return pts;
}

/**
 * Make a speed/distance map
 */
double *vik_track_make_speed_dist_map (const Track * trk, uint16_t num_chunks)
{
	time_t t1, t2;
	int i, pt_count, numpts, index;
	GList *iter;
	double duration, total_length, chunk_length;

	if (! trk->trackpoints)
		return NULL;

	t1 = ((Trackpoint *) trk->trackpoints->data)->timestamp;
	t2 = ((Trackpoint *) g_list_last(trk->trackpoints)->data)->timestamp;
	duration = t2 - t1;

	if (!t1 || !t2 || !duration)
		return NULL;

	if (duration < 0) {
		fprintf(stderr, "WARNING: negative duration: unsorted trackpoint timestamps?\n");
		return NULL;
	}

	total_length = vik_track_get_length_including_gaps (trk);
	chunk_length = total_length / num_chunks;
	pt_count = vik_track_get_tp_count(trk);

	if (chunk_length <= 0) {
		return NULL;
	}

	double *v = (double *) malloc(sizeof (double) * num_chunks);
	double *s = (double *) malloc(sizeof (double) * pt_count);
	double *t = (double *) malloc(sizeof (double) * pt_count);

	// No special handling of segments ATM...
	iter = trk->trackpoints->next;
	numpts = 0;
	s[0] = 0;
	t[0] = ((Trackpoint *) trk->trackpoints->data)->timestamp;
	numpts++;
	while (iter) {
		s[numpts] = s[numpts-1] + vik_coord_diff (&(((Trackpoint *) iter->prev->data)->coord), &(((Trackpoint *) iter->data)->coord));
		t[numpts] = ((Trackpoint *) iter->data)->timestamp;
		numpts++;
		iter = iter->next;
	}

	// Iterate through a portion of the track to get an average speed for that part
	// This will essentially interpolate between segments, which I think is right given the usage of 'get_length_including_gaps'
	index = 0; /* index of the current trackpoint. */
	for (i = 0; i < num_chunks; i++) {
		// Similar to the make_speed_map, but instead of using a time chunk, use a distance chunk
		if (s[0] + i*chunk_length >= s[index]) {
			double acc_t = 0, acc_s = 0;
			while (s[0] + i*chunk_length >= s[index]) {
				acc_s += (s[index+1]-s[index]);
				acc_t += (t[index+1]-t[index]);
				index++;
			}
			v[i] = acc_s/acc_t;
		}
		else if (i) {
			v[i] = v[i-1];
		}
		else {
			v[i] = 0;
		}
	}
	free(s);
	free(t);
	return v;
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
Trackpoint * vik_track_get_tp_by_dist (Track * trk, double meters_from_start, bool get_next_point, double *tp_metres_from_start)
{
	double current_dist = 0.0;
	double current_inc = 0.0;
	if (tp_metres_from_start)
		*tp_metres_from_start = 0.0;

	if (trk->trackpoints) {
		GList *iter = g_list_next (g_list_first (trk->trackpoints));
		while (iter) {
			current_inc = vik_coord_diff (&(((Trackpoint *) iter->data)->coord),
						       &(((Trackpoint *) iter->prev->data)->coord));
			current_dist += current_inc;
			if (current_dist >= meters_from_start)
				break;
			iter = g_list_next (iter);
		}
		// passed the end of the track
		if (!iter)
			return NULL;

		if (tp_metres_from_start)
			*tp_metres_from_start = current_dist;

		// we've gone past the distance already, is the previous trackpoint wanted?
		if (!get_next_point) {
			if (iter->prev) {
				if (tp_metres_from_start)
					*tp_metres_from_start = current_dist-current_inc;
				return ((Trackpoint *) iter->prev->data);
			}
		}
		return ((Trackpoint *) iter->data);
	}

	return NULL;
}

/* by Alex Foobarian */
Trackpoint * vik_track_get_closest_tp_by_percentage_dist (Track * trk, double reldist, double *meters_from_start)
{
	double dist = vik_track_get_length_including_gaps(trk) * reldist;
	double current_dist = 0.0;
	double current_inc = 0.0;
	if (trk->trackpoints) {
		GList *iter = trk->trackpoints->next;
		GList *last_iter = NULL;
		double last_dist = 0.0;
		while (iter) {
			current_inc = vik_coord_diff (&(((Trackpoint *) iter->data)->coord),
						       &(((Trackpoint *) iter->prev->data)->coord));
			last_dist = current_dist;
			current_dist += current_inc;
			if (current_dist >= dist)
				break;
			last_iter = iter;
			iter = iter->next;
		}
		if (!iter) { /* passing the end the track */
			if (last_iter) {
				if (meters_from_start)
					*meters_from_start = last_dist;
				return(((Trackpoint *) last_iter->data));
			}
			else
				return NULL;
		}
		/* we've gone past the dist already, was prev trackpoint closer? */
		/* should do a vik_coord_average_weighted() thingy. */
		if (iter->prev && fabs(current_dist-current_inc-dist) < fabs(current_dist-dist)) {
			if (meters_from_start)
				*meters_from_start = last_dist;
			iter = iter->prev;
		}
		else
			if (meters_from_start)
				*meters_from_start = current_dist;

		return ((Trackpoint *) iter->data);

	}
	return NULL;
}

Trackpoint * vik_track_get_closest_tp_by_percentage_time (Track * trk, double reltime, time_t *seconds_from_start)
{
	if (!trk->trackpoints)
		return NULL;

	time_t t_pos, t_start, t_end, t_total;
	t_start = ((Trackpoint *) trk->trackpoints->data)->timestamp;
	t_end = ((Trackpoint *) g_list_last(trk->trackpoints)->data)->timestamp;
	t_total = t_end - t_start;

	t_pos = t_start + t_total * reltime;

	GList *iter = trk->trackpoints;

	while (iter) {
		if (((Trackpoint *) iter->data)->timestamp == t_pos)
			break;
		if (((Trackpoint *) iter->data)->timestamp > t_pos) {
			if (iter->prev == NULL)  /* first trackpoint */
				break;
			time_t t_before = t_pos - ((Trackpoint *) iter->prev->data)->timestamp;
			time_t t_after = ((Trackpoint *) iter->data)->timestamp - t_pos;
			if (t_before <= t_after)
				iter = iter->prev;
			break;
		}
		else if ((iter->next == NULL) && (t_pos < (((Trackpoint *) iter->data)->timestamp + 3))) /* last trackpoint: accommodate for round-off */
			break;
		iter = iter->next;
	}

	if (!iter)
		return NULL;
	if (seconds_from_start)
		*seconds_from_start = ((Trackpoint *) iter->data)->timestamp - ((Trackpoint *) trk->trackpoints->data)->timestamp;
	return ((Trackpoint *) iter->data);
}

Trackpoint * vik_track_get_tp_by_max_speed (const Track *trk)
{
	double maxspeed = 0.0, speed = 0.0;

	if (!trk->trackpoints)
		return NULL;

	GList *iter = trk->trackpoints;
	Trackpoint * max_speed_tp = NULL;

	while (iter) {
		if (iter->prev) {
			if (((Trackpoint *) iter->data)->has_timestamp &&
			     ((Trackpoint *) iter->prev->data)->has_timestamp &&
			     (! ((Trackpoint *) iter->data)->newsegment)) {
				speed =  vik_coord_diff (&(((Trackpoint *) iter->data)->coord), &(((Trackpoint *) iter->prev->data)->coord))
					/ ABS(((Trackpoint *) iter->data)->timestamp - ((Trackpoint *) iter->prev->data)->timestamp);
				if (speed > maxspeed) {
					maxspeed = speed;
					max_speed_tp = ((Trackpoint *) iter->data);
				}
			}
		}
		iter = iter->next;
	}

	if (!max_speed_tp)
		return NULL;

	return max_speed_tp;
}

Trackpoint * vik_track_get_tp_by_max_alt(const Track * trk)
{
	double maxalt = -5000.0;
	if (!trk->trackpoints)
		return NULL;

	GList *iter = trk->trackpoints;
	Trackpoint * max_alt_tp = NULL;

	while (iter) {
		if (((Trackpoint *) iter->data)->altitude > maxalt) {
			maxalt = ((Trackpoint *) iter->data)->altitude;
			max_alt_tp = ((Trackpoint *) iter->data);
		}
		iter = iter->next;
	}

	if (!max_alt_tp)
		return NULL;

	return max_alt_tp;
}

Trackpoint * vik_track_get_tp_by_min_alt(const Track *trk)
{
	double minalt = 25000.0;
	if (!trk->trackpoints)
		return NULL;

	GList *iter = trk->trackpoints;
	Trackpoint * min_alt_tp = NULL;

	while (iter) {
		if (((Trackpoint *) iter->data)->altitude < minalt) {
			minalt = ((Trackpoint *) iter->data)->altitude;
			min_alt_tp = ((Trackpoint *) iter->data);
		}
		iter = iter->next;
	}

	if (!min_alt_tp)
		return NULL;

	return min_alt_tp;
}

Trackpoint * vik_track_get_tp_first(const Track * trk)
{
	if (!trk->trackpoints)
		return NULL;

	return (Trackpoint *) g_list_first(trk->trackpoints)->data;
}

Trackpoint * vik_track_get_tp_last(const Track *trk)
{
	if (!trk->trackpoints)
		return NULL;

	return (Trackpoint*)g_list_last(trk->trackpoints)->data;
}

Trackpoint * vik_track_get_tp_prev (const Track * trk, Trackpoint * tp)
{
	if (!trk->trackpoints)
		return NULL;

	GList *iter = trk->trackpoints;
	Trackpoint * tp_prev = NULL;

	while (iter) {
		if (iter->prev) {
			if (((Trackpoint *) iter->data) == tp) {
				tp_prev = ((Trackpoint *) iter->prev->data);
				break;
			}
		}
		iter = iter->next;
	}

	return tp_prev;
}

bool vik_track_get_minmax_alt (const Track * trk, double *min_alt, double *max_alt)
{
	*min_alt = 25000;
	*max_alt = -5000;
	if (trk && trk->trackpoints && trk->trackpoints->data && (((Trackpoint *) trk->trackpoints->data)->altitude != VIK_DEFAULT_ALTITUDE)) {
		GList *iter = trk->trackpoints->next;
		double tmp_alt;
		while (iter) {
			tmp_alt = ((Trackpoint *) iter->data)->altitude;
			if (tmp_alt > *max_alt)
				*max_alt = tmp_alt;
			if (tmp_alt < *min_alt)
				*min_alt = tmp_alt;
			iter = iter->next;
		}
		return true;
	}
	return false;
}

void vik_track_marshall (Track * trk, uint8_t **data, size_t *datalen)
{
	GList *tps;
	GByteArray *b = g_byte_array_new();
	unsigned int len;
	unsigned int intp, ntp;

	g_byte_array_append(b, (uint8_t *) trk, sizeof(* trk));

	/* we'll fill out number of trackpoints later */
	intp = b->len;
	g_byte_array_append(b, (uint8_t *)&len, sizeof(len));

	// This allocates space for variant sized strings
	//  and copies that amount of data from the track to byte array
#define vtm_append(s)	       \
	len = (s) ? strlen(s)+1 : 0;			\
	g_byte_array_append(b, (uint8_t *)&len, sizeof(len));	\
	if (s) g_byte_array_append(b, (uint8_t *)s, len);

	tps = trk->trackpoints;
	ntp = 0;
	while (tps) {
		g_byte_array_append(b, (uint8_t *)tps->data, sizeof(Trackpoint));
		vtm_append(((Trackpoint *) tps->data)->name);
		tps = tps->next;
		ntp++;
	}
	*(unsigned int *)(b->data + intp) = ntp;

	vtm_append(trk->name);
	vtm_append(trk->comment);
	vtm_append(trk->description);
	vtm_append(trk->source);

	*data = b->data;
	*datalen = b->len;
	g_byte_array_free(b, false);
}

/*
 * Take a byte array and convert it into a Track
 */
Track * vik_track_unmarshall (uint8_t *data, size_t datalen)
{
	unsigned int len;
	Track * new_trk = vik_track_new();
	Trackpoint * new_tp;
	unsigned int ntp;
	int i;

	/* basic properties: */
	new_trk->visible = ((Track *)data)->visible;
	new_trk->is_route = ((Track *)data)->is_route;
	new_trk->draw_name_mode = ((Track *)data)->draw_name_mode;
	new_trk->max_number_dist_labels = ((Track *)data)->max_number_dist_labels;
	new_trk->has_color = ((Track *)data)->has_color;
	new_trk->color = ((Track *)data)->color;
	new_trk->bbox = ((Track *)data)->bbox;

	data += sizeof(*new_trk);

	ntp = *(unsigned int *)data;
	data += sizeof(ntp);

#define vtu_get(s)	       \
	len = *(unsigned int *)data;		\
	data += sizeof(len);			\
	if (len) {				\
		(s) = g_strdup((char *)data);	\
	} else {				\
		(s) = NULL;			\
	}					\
	data += len;

	for (i=0; i<ntp; i++) {
		new_tp = new Trackpoint();
		memcpy(new_tp, data, sizeof(*new_tp));
		data += sizeof(*new_tp);
		vtu_get(new_tp->name);
		new_trk->trackpoints = g_list_prepend(new_trk->trackpoints, new_tp);
	}
	if (new_trk->trackpoints)
		new_trk->trackpoints = g_list_reverse(new_trk->trackpoints);

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
void vik_track_calculate_bounds(Track * trk)
{
	GList *tp_iter;
	tp_iter = trk->trackpoints;

	struct LatLon topleft, bottomright, ll;

	// Set bounds to first point
	if (tp_iter) {
		vik_coord_to_latlon (&(((Trackpoint *) tp_iter->data)->coord), &topleft);
		vik_coord_to_latlon (&(((Trackpoint *) tp_iter->data)->coord), &bottomright);
	}
	while (tp_iter) {

		// See if this trackpoint increases the track bounds.

		vik_coord_to_latlon (&(((Trackpoint *) tp_iter->data)->coord), &ll);

		if (ll.lat > topleft.lat) topleft.lat = ll.lat;
		if (ll.lon < topleft.lon) topleft.lon = ll.lon;
		if (ll.lat < bottomright.lat) bottomright.lat = ll.lat;
		if (ll.lon > bottomright.lon) bottomright.lon = ll.lon;

		tp_iter = tp_iter->next;
	}

	fprintf(stderr, "DEBUG: Bounds of track: '%s' is: %f,%f to: %f,%f\n", trk->name, topleft.lat, topleft.lon, bottomright.lat, bottomright.lon);

	trk->bbox.north = topleft.lat;
	trk->bbox.east = bottomright.lon;
	trk->bbox.south = bottomright.lat;
	trk->bbox.west = topleft.lon;
}

/**
 * vik_track_anonymize_times:
 *
 * Shift all timestamps to be relatively offset from 1901-01-01
 */
void vik_track_anonymize_times(Track * trk)
{
	GTimeVal gtv;
	// Check result just to please Coverity - even though it shouldn't fail as it's a hard coded value here!
	if (!g_time_val_from_iso8601 ("1901-01-01T00:00:00Z", &gtv)) {
		fprintf(stderr, "CRITICAL: Calendar time value failure\n");
		return;
	}

	time_t anon_timestamp = gtv.tv_sec;
	time_t offset = 0;

	GList *tp_iter;
	tp_iter = trk->trackpoints;
	while (tp_iter) {
		Trackpoint * tp = (Trackpoint *) (tp_iter->data);
		if (tp->has_timestamp) {
			// Calculate an offset in time using the first available timestamp
			if (offset == 0)
				offset = tp->timestamp - anon_timestamp;

			// Apply this offset to shift all timestamps towards 1901 & hence anonymising the time
			// Note that the relative difference between timestamps is kept - thus calculating speeds will still work
			tp->timestamp = tp->timestamp - offset;
		}
		tp_iter = tp_iter->next;
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
void vik_track_interpolate_times(Track * trk)
{
	double tr_dist, cur_dist;
	time_t tsdiff, tsfirst;

	GList *iter;
	iter = trk->trackpoints;

	Trackpoint *tp = (Trackpoint *) (iter->data);
	if (tp->has_timestamp) {
		tsfirst = tp->timestamp;

		// Find the end of the track and the last timestamp
		while (iter->next) {
			iter = iter->next;
		}
		tp = ((Trackpoint *) iter->data);
		if (tp->has_timestamp) {
			tsdiff = tp->timestamp - tsfirst;

			tr_dist = vik_track_get_length_including_gaps(trk);
			cur_dist = 0.0;

			if (tr_dist > 0) {
				iter = trk->trackpoints;
				// Apply the calculated timestamp to all trackpoints except the first and last ones
				while (iter->next && iter->next->next) {
					iter = iter->next;
					tp = ((Trackpoint *) iter->data);
					cur_dist += vik_coord_diff (&(tp->coord), &(((Trackpoint *) iter->prev->data)->coord));

					tp->timestamp = (cur_dist / tr_dist) * tsdiff + tsfirst;
					tp->has_timestamp = true;
				}
				// Some points may now have the same time so remove them.
				vik_track_remove_same_time_points(trk);
			}
		}
	}
}

/**
 * vik_track_apply_dem_data:
 * @skip_existing: When true, don't change the elevation if the trackpoint already has a value
 *
 * Set elevation data for a track using any available DEM information
 */
unsigned long vik_track_apply_dem_data (Track * trk, bool skip_existing)
{
	unsigned long num = 0;
	GList *tp_iter;
	int16_t elev;
	tp_iter = trk->trackpoints;
	while (tp_iter) {
		// Don't apply if the point already has a value and the overwrite is off
		if (!(skip_existing && ((Trackpoint *) tp_iter->data)->altitude != VIK_DEFAULT_ALTITUDE)) {
			/* TODO: of the 4 possible choices we have for choosing an elevation
			 * (trackpoint in between samples), choose the one with the least elevation change
			 * as the last */
			elev = a_dems_get_elev_by_coord (&(((Trackpoint *) tp_iter->data)->coord), VIK_DEM_INTERPOL_BEST);

			if (elev != VIK_DEM_INVALID_ELEVATION) {
				((Trackpoint *) tp_iter->data)->altitude = elev;
				num++;
			}
		}
		tp_iter = tp_iter->next;
	}
	return num;
}

/**
 * vik_track_apply_dem_data_last_trackpoint:
 * Apply DEM data (if available) - to only the last trackpoint
 */
void vik_track_apply_dem_data_last_trackpoint(Track * trk)
{
	int16_t elev;
	if (trk->trackpoints) {
		/* As in vik_track_apply_dem_data above - use 'best' interpolation method */
		elev = a_dems_get_elev_by_coord (&(((Trackpoint *) g_list_last(trk->trackpoints)->data)->coord), VIK_DEM_INTERPOL_BEST);
		if (elev != VIK_DEM_INVALID_ELEVATION)
			((Trackpoint *) g_list_last(trk->trackpoints)->data)->altitude = elev;
	}
}


/**
 * smoothie:
 *
 * Apply elevation smoothing over range of trackpoints between the list start and end points
 */
static void smoothie (GList *tp1, GList *tp2, double elev1, double elev2, unsigned int points)
{
	// If was really clever could try and weigh interpolation according to the distance between trackpoints somehow
	// Instead a simple average interpolation for the number of points given.
	double change = (elev2 - elev1)/(points+1);
	int count = 1;
	GList *tp_iter = tp1;
	while (tp_iter != tp2 && tp_iter) {
		Trackpoint * tp = (Trackpoint *) (tp_iter->data);

		tp->altitude = elev1 + (change*count);

		count++;
		tp_iter = tp_iter->next;
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
unsigned long vik_track_smooth_missing_elevation_data (Track * trk, bool flat)
{
	unsigned long num = 0;

	GList *tp_iter;
	double elev = VIK_DEFAULT_ALTITUDE;

	Trackpoint * tp_missing = NULL;
	GList *iter_first = NULL;
	unsigned int points = 0;

	tp_iter = trk->trackpoints;
	while (tp_iter) {
		Trackpoint * tp = (Trackpoint *) (tp_iter->data);

		if (VIK_DEFAULT_ALTITUDE == tp->altitude) {
			if (flat) {
				// Simply assign to last known value
				if (elev != VIK_DEFAULT_ALTITUDE) {
					tp->altitude = elev;
					num++;
				}
			}
			else {
				if (!tp_missing) {
					// Remember the first trackpoint (and the list pointer to it) of a section of no altitudes
					tp_missing = tp;
					iter_first = tp_iter;
					points = 1;
				}
				else {
					// More missing altitudes
					points++;
				}
			}
		}
		else {
			// Altitude available (maybe again!)
			// If this marks the end of a section of altitude-less points
			//  then apply smoothing for that section of points
			if (points > 0 && elev != VIK_DEFAULT_ALTITUDE)
				if (!flat) {
					smoothie (iter_first, tp_iter, elev, tp->altitude, points);
					num = num + points;
				}

			// reset
			points = 0;
			tp_missing = NULL;

			// Store for reuse as the last known good value
			elev = tp->altitude;
		}

		tp_iter = tp_iter->next;
	}

	return num;
}

/**
 * vik_track_steal_and_append_trackpoints:
 *
 * appends t2 to t1, leaving t2 with no trackpoints
 */
void vik_track_steal_and_append_trackpoints(Track *trk1, Track *trk2)
{
	if (trk1->trackpoints) {
		trk1->trackpoints = g_list_concat (trk1->trackpoints, trk2->trackpoints);
	} else
		trk1->trackpoints = trk2->trackpoints;
	trk2->trackpoints = NULL;

	// Trackpoints updated - so update the bounds
	vik_track_calculate_bounds(trk1);
}

/**
 * vik_track_cut_back_to_double_point:
 *
 * starting at the end, looks backwards for the last "double point", a duplicate trackpoint.
 * If there is no double point, deletes all the trackpoints.
 *
 * Returns: the new end of the track (or the start if there are no double points)
 */
VikCoord *vik_track_cut_back_to_double_point(Track * trk)
{
	GList *iter = trk->trackpoints;
	VikCoord *rv;

	if (!iter)
		return NULL;
	while (iter->next)
		iter = iter->next;


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
	*rv = ((Trackpoint*) trk->trackpoints->data)->coord;
	g_list_foreach (trk->trackpoints, (GFunc) g_free, NULL);
	g_list_free(trk->trackpoints);
	trk->trackpoints = NULL;
	return rv;
}

/**
 * Function to compare two tracks by their first timestamp
 **/
int vik_track_compare_timestamp (const void *x, const void *y)
{
	Track * a = (Track *) x;
	Track * b = (Track *) y;

	Trackpoint * tpa = NULL;
	Trackpoint * tpb = NULL;

	if (a->trackpoints)
		tpa = ((Trackpoint *) g_list_first(a->trackpoints)->data);

	if (b->trackpoints)
		tpb = ((Trackpoint *) g_list_first(b->trackpoints)->data);

	if (tpa && tpb) {
		if (tpa->timestamp < tpb->timestamp)
			return -1;
		if (tpa->timestamp > tpb->timestamp)
			return 1;
	}

	if (tpa && !tpb)
		return 1;

	if (!tpa && tpb)
		return -1;

	return 0;
}
