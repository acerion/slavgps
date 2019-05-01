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
 */




#include <algorithm>
#include <cstdlib>
#include <cassert>
#include <mutex>
#include <cstring>
#include <cmath>
#include <time.h>




#include <QDebug>
#include <QFileDialog>




#include "coords.h"
#include "coord.h"
#include "dem_cache.h"
#include "application_state.h"
#include "util.h"
#include "osm_traces.h"
#include "layer_aggregate.h"
#include "layer_trw_track_internal.h"
#include "layer_trw_track_profile_dialog.h"
#include "layer_trw_track_properties_dialog.h"
#include "layer_trw_trackpoint_properties.h"
#include "layer_trw_menu.h"
#include "layer_trw.h"
#include "layer_trw_painter.h"
#include "layer_trw_geotag.h"
#include "layer_trw_tools.h"
#include "window.h"
#include "dialog.h"
#include "layers_panel.h"
#include "tree_view_internal.h"
#include "routing.h"
#include "measurements.h"
#include "preferences.h"
#include "viewport_internal.h"
#include "file.h"
#include "acquire.h"
#include "tree_item_list.h"
#include "astro.h"




using namespace SlavGPS;




#define SG_MODULE "Layer TRW Track"




extern SelectedTreeItems g_selected;

extern bool g_have_astro_program;
extern bool g_have_diary_program;




/* The last used directories. */
static QUrl last_directory_url;



#define VIK_SETTINGS_TRACK_NAME_MODE "track_draw_name_mode"
#define VIK_SETTINGS_TRACK_NUM_DIST_LABELS "track_number_dist_labels"


void do_compress(TrackData & compressed_data, const TrackData & raw_data);


/**
 * Set some default values for a track.
 * ATM This uses the 'settings' method to get values,
 * so there is no GUI way to control these yet...
 */
void Track::set_defaults()
{
	int tmp;
	if (ApplicationState::get_integer(VIK_SETTINGS_TRACK_NAME_MODE, &tmp)) {
		this->draw_name_mode = (TrackDrawNameMode) tmp;
	}

	if (ApplicationState::get_integer(VIK_SETTINGS_TRACK_NUM_DIST_LABELS, &tmp)) {
		this->max_number_dist_labels = tmp;
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




void Track::self_assign_icon(void)
{
	if (this->has_color) {
		QPixmap pixmap(SMALL_ICON_SIZE, SMALL_ICON_SIZE);
		pixmap.fill(this->color);
		this->icon = QIcon(pixmap);
	} else {
		this->icon = QIcon(); /* Invalidate icon. */
	}
}




void Track::self_assign_timestamp(void)
{
	Trackpoint * tp = this->get_tp_first();
	if (tp) {
		this->set_timestamp(tp->timestamp);
	}
}




void Track::ref()
{
	ref_count++;
}




void Track::set_properties_dialog(TrackPropertiesDialog * dialog)
{
	this->props_dialog = dialog;
}




void Track::clear_properties_dialog()
{
	this->props_dialog = NULL;
}




/**
   @brief Update track properties dialog e.g. if the track has been renamed
*/
void Track::update_properties_dialog(void)
{
	/* If not displayed do nothing. */
	if (!this->props_dialog) {
		return;
	}

	/* Update title with current name. */
	if (!this->name.isEmpty()) {
		this->props_dialog->setWindowTitle(tr("%1 - Track Properties").arg(this->name));
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
	if (is_route) {
		this->type_id = "sg.trw.route";
	} else {
		this->type_id = "sg.trw.track";
	}

	this->ref_count = 1;

	this->has_properties_dialog = true;

	this->menu_operation_ids = (MenuOperation) (MenuOperationCut | MenuOperationCopy | MenuOperationDelete);
}




/**
   @brief Build new empty track using @from as template

   @from: The Track to copy properties from.

   The constructor only copies properties, but does not copy nor move trackpoints.
*/
Track::Track(const Track & from) : Track(from.is_route())
{
	this->copy_properties(from);
}




void Track::copy_properties(const Track & from)
{
	this->visible = from.visible;
	this->draw_name_mode = from.draw_name_mode;
	this->max_number_dist_labels = from.max_number_dist_labels;

	this->set_name(from.name);
	this->set_comment(from.comment);
	this->set_description(from.description);
	this->set_source(from.source);

	/* this->type_id is set by Track::Track(bool is_route) called by this constructor. */

	this->has_color = from.has_color;
	this->color = from.color;
	this->bbox = from.bbox;
}




sg_ret Track::move_trackpoints_from(Track & from, const TrackPoints::iterator & from_begin, const TrackPoints::iterator & from_end)
{
	this->trackpoints.splice(this->trackpoints.end(), from.trackpoints, from_begin, from_end);
	/* Trackpoints updated in both tracks, so recalculate bbox of both tracks. */
	this->recalculate_bbox();
	from.recalculate_bbox();
	return sg_ret::ok;
}




sg_ret Track::copy_trackpoints_from(const TrackPoints::iterator & from_begin, const TrackPoints::iterator & from_end)
{
	for (auto iter = from_begin; iter != from_end; iter++) {
		Trackpoint * tp = new Trackpoint(**iter);
		this->trackpoints.push_back(tp);
	}
	this->recalculate_bbox();
	return sg_ret::ok;
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

	this->coord = tp.coord;

	this->newsegment = tp.newsegment;
	this->set_timestamp(tp.timestamp);
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
	const LatLon ll_a = tp_a.coord.get_latlon();
	const LatLon ll_b = tp_b.coord.get_latlon();

	/* Main positional interpolation. */
	this->coord = Coord(LatLon::get_average(ll_a, ll_b), coord_mode);

	/* Now other properties that can be interpolated. */
	this->altitude = (tp_a.altitude + tp_b.altitude) / 2;

	if (tp_a.timestamp.is_valid() && tp_b.timestamp.is_valid()) {
		/* Note here the division is applied to each part, then added
		   This is to avoid potential overflow issues with a 32 time_t for
		   dates after midpoint of this Unix time on 2004/01/04. */
		this->set_timestamp((tp_a.timestamp.get_value() / 2) + (tp_b.timestamp.get_value() / 2));
	}

	if (tp_a.speed != NAN && tp_b.speed != NAN) {
		this->speed = (tp_a.speed + tp_b.speed) / 2;
	}

	if (tp_a.course.is_valid() && tp_b.course.is_valid()) {
		this->course = Angle::get_vector_sum(tp_a.course, tp_b.course);
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
void Track::recalculate_bbox_last_tp()
{
	if (this->trackpoints.empty()) {
		return;
	}

	Trackpoint * tp = *std::prev(this->trackpoints.end());
	if (tp) {
		/* See if this trackpoint increases the track bounds and update if so. */
		const LatLon lat_lon = tp->coord.get_latlon();
		this->bbox.expand_with_lat_lon(lat_lon);
	}
}




/**
 * @tr:          The track to which the trackpoint will be added
 * @tp:          The trackpoint to add
 * @recalculate: Whether to perform any associated properties recalculations
 *               Generally one should avoid recalculation via this method if adding lots of points
 *               (But ensure recalculate_bbox() is called after adding all points!!)
 *
 * The trackpoint is added to the end of the existing trackpoint list.
 */
void Track::add_trackpoint(Trackpoint * tp, bool recalculate)
{
	/* When it's the first trackpoint need to ensure the bounding box is initialized correctly. */
	bool adding_first_point = this->trackpoints.empty();
	this->trackpoints.push_back(tp);
	if (adding_first_point) {
		this->recalculate_bbox();
	} else if (recalculate) {
		this->recalculate_bbox_last_tp();
	}
}




double Track::get_length_value_to_trackpoint(const Trackpoint * tp) const
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




Distance Track::get_length_to_trackpoint(const Trackpoint * tp) const
{
	return Distance(this->get_length_value_to_trackpoint(tp), SupplementaryDistanceUnit::Meters);
}




double Track::get_length_value(void) const
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




Distance Track::get_length(void) const
{
	return Distance(this->get_length_value(), SupplementaryDistanceUnit::Meters);
}




double Track::get_length_value_including_gaps(void) const
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




Distance Track::get_length_including_gaps(void) const
{
	return Distance(this->get_length_value_including_gaps(), SupplementaryDistanceUnit::Meters);
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
	this->recalculate_bbox();

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
		    && ((*iter)->timestamp.is_valid() && (*std::next(iter))->timestamp.is_valid())
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
		    && ((*iter)->timestamp.is_valid() && (*std::next(iter))->timestamp.is_valid())
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

	this->recalculate_bbox();

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

		(*iter)->timestamp.set_valid(false);
		(*iter)->speed = NAN;
		(*iter)->course = NAN;
		(*iter)->hdop = VIK_DEFAULT_DOP;
		(*iter)->vdop = VIK_DEFAULT_DOP;
		(*iter)->pdop = VIK_DEFAULT_DOP;
		(*iter)->nsats = 0;
		(*iter)->fix_mode = GPSFixMode::NotSeen;
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
Time Track::get_duration(bool segment_gaps) const
{
	Time result(0);

	if (this->trackpoints.empty()) {
		return result;
	}

	/* Ensure times are available. */
	if (this->get_tp_first()->timestamp.is_valid()) {
		/* Get trkpt only once - as using vik_track_get_tp_last() iterates whole track each time. */
		if (segment_gaps) {
			// Simple duration
			Trackpoint * tp_last = this->get_tp_last();
			if (tp_last->timestamp.is_valid()) {
				result = tp_last->timestamp - this->get_tp_first()->timestamp;
			}
		} else {
			/* Total within segments. */
			for (auto iter = std::next(this->trackpoints.begin()); iter != this->trackpoints.end(); iter++) {
				if ((*iter)->timestamp.is_valid()
				    && (*std::prev(iter))->timestamp.is_valid()
				    && !(*iter)->newsegment) {

					result += Time::get_abs_diff((*iter)->timestamp, (*std::prev(iter))->timestamp);
				}
			}
		}
	}

	return result;
}




/* Code extracted from make_track_data_speed_over_time() and similar functions. */
Time Track::get_duration(void) const
{
	Time result(0);

	Time ts_begin;
	Time ts_end;
	if (sg_ret::ok != this->get_timestamps(ts_begin, ts_end)) {
		qDebug() << SG_PREFIX_W << "Can't get track's timestamps";
		return result;
	}

	const Time duration = ts_end - ts_begin;
	if (!duration.is_valid()) {
		qDebug() << SG_PREFIX_E << "Invalid duration";
		return result;
	}

	if (duration.get_value() < 0) {
		qDebug() << SG_PREFIX_W << "Negative duration: unsorted trackpoint timestamps?";
		return result;
	}

	result = duration;
	return result;
}




Speed Track::get_average_speed(void) const
{
	Speed result(NAN, SpeedUnit::MetresPerSecond); /* Invalid by default. */

	if (this->trackpoints.empty()) {
		return result;
	}

	double len = 0.0;
	Time duration(0);

	for (auto iter = std::next(this->trackpoints.begin()); iter != this->trackpoints.end(); iter++) {

		if ((*iter)->timestamp.is_valid()
		    && (*std::prev(iter))->timestamp.is_valid()
		    && !(*iter)->newsegment) {

			len += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);
			duration += Time::get_abs_diff((*iter)->timestamp, (*std::prev(iter))->timestamp);
		}
	}

	if (duration.is_valid() && duration.get_value() > 0) {
		result.set_value(std::abs(len / duration.get_value()));
	}

	return result;
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
Speed Track::get_average_speed_moving(int track_min_stop_length_seconds) const
{
	Speed result(NAN, SpeedUnit::MetresPerSecond); /* Invalid by default. */

	if (this->trackpoints.empty()) {
		return result;
	}

	double len = 0.0;
	Time duration(0);

	for (auto iter = std::next(this->trackpoints.begin()); iter != this->trackpoints.end(); iter++) {
		if ((*iter)->timestamp.is_valid()
		    && (*std::prev(iter))->timestamp.is_valid()
		    && !(*iter)->newsegment) {

			if (((*iter)->timestamp.get_value() - (*std::prev(iter))->timestamp.get_value()) < track_min_stop_length_seconds) {
				len += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);

				duration += Time::get_abs_diff((*iter)->timestamp, (*std::prev(iter))->timestamp);
			}
		}
	}

	if (duration.is_valid() && duration.get_value() > 0) {
		result.set_value(std::abs(len / duration.get_value()));
	}

	return result;
}




sg_ret Track::calculate_max_speed(void)
{
	this->max_speed = Speed(NAN, SpeedUnit::MetresPerSecond); /* Initially invalid. */

	if (this->trackpoints.empty()) {
		return sg_ret::ok;
	}

	double maxspeed = 0.0;

	for (auto iter = std::next(this->trackpoints.begin()); iter != this->trackpoints.end(); iter++) {
		if ((*iter)->timestamp.is_valid()
		    && (*std::prev(iter))->timestamp.is_valid()
		    && !(*iter)->newsegment) {

			const double speed = Coord::distance((*iter)->coord, (*std::prev(iter))->coord)
				/ Time::get_abs_diff((*iter)->timestamp, (*std::prev(iter))->timestamp).get_value();

			if (speed > maxspeed) {
				maxspeed = speed;
			}
		}
	}

	this->max_speed.set_value(maxspeed); /* Set the value even if detected max speed is zero. */

	return sg_ret::ok;
}




const Speed & Track::get_max_speed(void) const
{
	return this->max_speed;
}




/* Simple method for copying "distance over time" information from Track to TrackData. */
TrackData Track::make_values_distance_over_time_helper(void) const
{
	/* No special handling of segments ATM... */

	const int tp_count = this->get_tp_count();
	TrackData data(tp_count);
	auto iter = this->trackpoints.begin();

	int i = 0;
	data.x[i] = (*iter)->timestamp.get_value();
	data.y[i] = 0;
	i++;
	iter++;

	while (iter != this->trackpoints.end()) {
		data.x[i] = (*iter)->timestamp.get_value();
		data.y[i] = data.y[i - 1] + Coord::distance((*std::prev(iter))->coord, (*iter)->coord);

		if (data.x[i] <= data.x[i - 1]) {
			qDebug() << SG_PREFIX_W << "Inconsistent time data at index" << i << ":" << data.x[i] << data.x[i - 1];
		}
		i++;
		iter++;
	}

	return data;
}



/* Simple method for copying "altitude over time" information from Track to TrackData. */
TrackData Track::make_values_altitude_over_time_helper(void) const
{
	const int tp_count = this->get_tp_count();

	TrackData data(tp_count);

	int i = 0;
	auto iter = this->trackpoints.begin();

	data.x[i] = (*iter)->timestamp.get_value();
	data.y[i] = (*iter)->altitude.get_value();
	i++;
	iter++;

	while (iter != this->trackpoints.end()) {
		data.x[i] = (*iter)->timestamp.get_value();
		data.y[i] = (*iter)->altitude.get_value();
		i++;
		iter++;
	}

	return data;
}




void Track::convert(CoordMode dest_mode)
{
	for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {
		(*iter)->coord.change_mode(dest_mode);
	}
}




/**
   I understood this when I wrote it ... maybe ... Basically it eats up the
   proper amounts of length on the track and averages elevation over that.
*/
TrackData Track::make_track_data_altitude_over_distance(int compressed_n_points) const
{
	TrackData compressed_ad;

	assert (compressed_n_points < 16000);
	if (this->trackpoints.size() < 2) {
		return compressed_ad;
	}

	{ /* Test if there's anything worth calculating. */

		bool correct = true;
		for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {
			/* Sometimes a GPS device (or indeed any random file) can have stupid numbers for elevations.
			   Since when is 9.9999e+24 a valid elevation!!
			   This can happen when a track (with no elevations) is uploaded to a GPS device and then redownloaded (e.g. using a Garmin Legend EtrexHCx).
			   Some protection against trying to work with crazily massive numbers (otherwise get SIGFPE, Arithmetic exception) */

			if ((*iter)->altitude.get_value() > SG_ALTITUDE_RANGE_MAX) {
				/* TODO_LATER: clamp the invalid values, but still generate vector? */
				qDebug() << SG_PREFIX_W << "Track altitude" << (*iter)->altitude << "out of range; not generating vector";
				correct = false;
				break;
			}
		}
		if (!correct) {
			return compressed_ad;
		}
	}

	double total_length = this->get_length_value_including_gaps();
	const double delta_d = total_length / (compressed_n_points - 1);

	/* Zero delta_d (eg, track of 2 tp with the same loc) will cause crash */
	if (delta_d <= 0) {
		return compressed_ad;
	}

	compressed_ad.allocate_vector(compressed_n_points);

	double current_dist = 0.0;
	double current_area_under_curve = 0;


	auto iter = this->trackpoints.begin();
	double current_seg_length = Coord::distance((*iter)->coord, (*std::next(iter))->coord);

	double altitude1 = (*iter)->altitude.get_value();
	double altitude2 = (*std::next(iter))->altitude.get_value();
	double dist_along_seg = 0;

	bool ignore_it = false;
	int current_chunk = 0;
	while (current_chunk < compressed_n_points) {

		/* Go along current seg. */
		if (current_seg_length && (current_seg_length - dist_along_seg) > delta_d) {
			dist_along_seg += delta_d;

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
				compressed_ad.y[current_chunk] = altitude1;
				if (current_chunk > 0) {
					/* TODO_LATER: verify this. */
					compressed_ad.x[current_chunk] = compressed_ad.x[current_chunk - 1] + delta_d;
				}
			} else {
				compressed_ad.y[current_chunk] = altitude1 + (altitude2 - altitude1) * ((dist_along_seg - (delta_d / 2)) / current_seg_length);
				if (current_chunk > 0) {
					/* TODO_LATER: verify this. */
					compressed_ad.x[current_chunk] = compressed_ad.x[current_chunk - 1] + delta_d;
				}
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
				altitude1 = (*iter)->altitude.get_value();
				altitude2 = (*std::next(iter))->altitude.get_value();
				ignore_it = (*std::next(iter))->newsegment;

				if (delta_d - current_dist >= current_seg_length) {
					current_dist += current_seg_length;
					current_area_under_curve += current_seg_length * (altitude1 + altitude2) * 0.5;
					iter++;
				} else {
					break;
				}
			}

			/* Final seg. */
			dist_along_seg = delta_d - current_dist;
			if (ignore_it
			    || (iter != this->trackpoints.end()
				&& std::next(iter) == this->trackpoints.end())) {

				compressed_ad.y[current_chunk] = current_area_under_curve / current_dist;
				if (current_chunk > 0) {
					/* TODO_LATER: verify this. */
					compressed_ad.x[current_chunk] = compressed_ad.x[current_chunk - 1] + delta_d;
				}
				if (std::next(iter) == this->trackpoints.end()) {
					for (int i = current_chunk + 1; i < compressed_n_points; i++) {
						compressed_ad.y[i] = compressed_ad.y[current_chunk];
						if (current_chunk > 0) {
							/* TODO_LATER: verify this. */
							compressed_ad.x[i] = compressed_ad.x[current_chunk - 1] + delta_d;
						}
					}
					break;
				}
			} else {
				current_area_under_curve += dist_along_seg * (altitude1 + (altitude2 - altitude1) * dist_along_seg / current_seg_length);
				compressed_ad.y[current_chunk] = current_area_under_curve / delta_d;
				if (current_chunk > 0) {
					/* TODO_LATER: verify this. */
					compressed_ad.x[current_chunk] = compressed_ad.x[current_chunk - 1] + delta_d;
				}
			}

			current_dist = 0;
			current_chunk++;
		}
	}

#ifdef K_FIXME_RESTORE
	assert(current_chunk == compressed_n_points);
#endif

	compressed_ad.n_points = compressed_n_points;
	compressed_ad.valid = true;
	return compressed_ad;
}




bool Track::get_total_elevation_gain(Altitude & delta_up, Altitude & delta_down) const
{
	if (this->trackpoints.empty()) {
		return false;
	}

	auto iter = this->trackpoints.begin();

	if (!(*iter)->altitude.is_valid()) {
		delta_up.set_valid(false);
		delta_down.set_valid(false);
		return false;
	} else {
		delta_up.set_value(0);
		delta_down.set_value(0);

		iter++;

		for (; iter != this->trackpoints.end(); iter++) {
			const Altitude diff = (*iter)->altitude - (*std::prev(iter))->altitude;
			if (diff.get_value() > 0.0) {
				delta_up += diff;
			} else {
				delta_down += diff;
			}
		}
		return true;
	}
}




TrackData Track::make_track_data_gradient_over_distance(int compressed_n_points) const
{
	TrackData compressed_gd;

	assert (compressed_n_points < 16000);

	const double total_length = this->get_length_value_including_gaps();
	const double delta_d = total_length / (compressed_n_points - 1);

	/* Zero delta_d (eg, track of 2 tp with the same loc) will cause crash. */
	if (delta_d <= 0) {
		return compressed_gd;
	}

	TrackData compressed_ad = this->make_track_data_altitude_over_distance(compressed_n_points);
	if (!compressed_ad.valid) {
		return compressed_gd;
	}

	compressed_gd.allocate_vector(compressed_n_points);

	int i = 0;
	double current_gradient = 0.0;
	for (i = 0; i < (compressed_n_points - 1); i++) {
		const double altitude1 = compressed_ad.y[i];
		const double altitude2 = compressed_ad.y[i + 1];
		current_gradient = 100.0 * (altitude2 - altitude1) / delta_d;

		if (i > 0) {
			compressed_gd.x[i] = compressed_gd.x[i - 1] + delta_d;
		}
		compressed_gd.y[i] = current_gradient;

	}
	compressed_gd.x[i] = compressed_gd.x[i - 1] + delta_d;
	compressed_gd.y[i] = current_gradient;

	assert (i + 1 == compressed_n_points);

	compressed_gd.n_points = compressed_n_points;
	compressed_gd.valid = true;
	return compressed_gd;
}




/* By Alex Foobarian. */
TrackData Track::make_track_data_speed_over_time(void) const
{
	TrackData result;

	const Time duration = this->get_duration();
	if (!duration.is_valid() || duration.get_value() < 0) {
		return result;
	}

	const int tp_count = this->get_tp_count();
	const TrackData data_dt = this->make_values_distance_over_time_helper();
	assert (data_dt.n_points == (int) this->get_tp_count());

	result.allocate_vector(tp_count);

	int i = 0;
	result.x[i] = data_dt.x[0];
	result.y[i] = 0.0;
	i++;

	for (; i < tp_count; i++) {
		if (data_dt.x[i] <= data_dt.x[i - 1]) {
			/* Handle glitch in values of consecutive time stamps.
			   TODO_LATER: improve code that calculates pseudo-values of result when a glitch has been found. */
			qDebug() << SG_PREFIX_W << "Glitch in timestamps:" << i << data_dt.x[i] << data_dt.x[i - 1];
			result.x[i] = data_dt.x[i - 1];
			result.y[i] = 0;
		} else {
			const double delta_t = (data_dt.x[i] - data_dt.x[i - 1]);
			const double delta_d = (data_dt.y[i] - data_dt.y[i - 1]);

			result.x[i] = data_dt.x[i];
			result.y[i] = delta_d / delta_t;
		}
	}

	result.n_points = tp_count;
	result.valid = true;
	return result;
}




/**
   Make a distance/time map, heavily based on the Track::make_track_data_speed_over_time() method.
*/
TrackData Track::make_track_data_distance_over_time(void) const
{
	TrackData result;

	const Time duration = this->get_duration();
	if (!duration.is_valid() || duration.get_value() < 0) {
		return result;
	}

	const int tp_count = this->get_tp_count();
	const TrackData data_dt = this->make_values_distance_over_time_helper();

	assert (data_dt.n_points == tp_count);

	result.allocate_vector(tp_count);

	int i = 0;
	result.x[i] = data_dt.x[i];
	result.y[i] = result.y[i];
	i++;

	for (; i < data_dt.n_points; i++) {
		if (data_dt.x[i] <= data_dt.x[i - 1]) {
			/* Handle glitch in values of consecutive time stamps.
			   TODO_LATER: improve code that calculates pseudo-values of result when a glitch has been found. */
			qDebug() << SG_PREFIX_W << "Glitch in timestamps" << i << data_dt.x[i] << data_dt.x[i - 1];
			result.x[i] = data_dt.x[i - 1];
			result.y[i] = 0;
		} else {
			result.x[i] = data_dt.x[i];
			result.y[i] = data_dt.y[i - 1];
		}
	}

	result.valid = true;
	return result;
}



void do_compress(TrackData & compressed_data, const TrackData & raw_data)
{
	const double tps_per_data_point = 1.0 * raw_data.n_points / compressed_data.n_points;
	const int floor_ = floor(tps_per_data_point);
	const int ceil_ = ceil(tps_per_data_point);
	int n_tps_compressed = 0;

	//qDebug() << "-----------------------" << floor_ << tps_per_data_point << ceil_ << raw_data.n_points << compressed_data.n_points;

	/* In the following computation, we iterate through periods of time of duration delta_t.
	   The first period begins at the beginning of the track.  The last period ends at the end of the track. */
	int tp_index = 0; /* index of the current trackpoint. */
	int i = 0;
	for (i = 0; i < compressed_data.n_points; i++) {

		int sampling_size;
		if ((i + 1) * tps_per_data_point > n_tps_compressed + floor_) {
		        sampling_size = ceil_;
		} else {
			sampling_size = floor_;
		}

		/* This may happen at the very end of loop, when
		   attempting to calculate last output data point. */
		if (n_tps_compressed + sampling_size > raw_data.n_points) {
			const int fix = (n_tps_compressed + sampling_size) - raw_data.n_points;
			qDebug() << "oooooooooooo truncating from" << n_tps_compressed + sampling_size
				 << "to" << n_tps_compressed + sampling_size - fix
				 << "(sampling_size = " << sampling_size << " -> " << sampling_size - fix << ")";
			sampling_size -= fix;
		}

		double acc_x = 0.0;
		double acc_y = 0.0;
		for (int j = n_tps_compressed; j < n_tps_compressed + sampling_size; j++) {
			acc_x += raw_data.x[j];
			acc_y += raw_data.y[j];
			tp_index++;
		}

		//qDebug() << "------- i =" << i << "/" << compressed_data.n_points << "sampling_size =" << sampling_size << "n_tps_compressed =" << n_tps_compressed << "n_tps_compressed + sampling_size =" << n_tps_compressed + sampling_size << acc_y << acc_y / sampling_size;

		compressed_data.x[i] = acc_x / sampling_size;
		compressed_data.y[i] = acc_y / sampling_size;

		n_tps_compressed += sampling_size;

	}

	assert(i == compressed_data.n_points);

	return;
}




TrackData TrackData::compress(int compressed_n_points) const
{
	TrackData compressed(compressed_n_points);

	do_compress(compressed, *this);

	compressed.n_points = compressed_n_points;
	compressed.valid = true;

	return compressed;
}




/**
   This uses the 'time' based method to make the graph, (which is a simpler compared to the elevation/distance).
   This results in a slightly blocky graph when it does not have many trackpoints: <60.
   NB Somehow the elevation/distance applies some kind of smoothing algorithm,
   but I don't think any one understands it any more (I certainly don't ATM).
*/
TrackData Track::make_track_data_altitude_over_time(void) const
{
	TrackData result;

	const Time duration = this->get_duration();
	if (!duration.is_valid() || duration.get_value() < 0) {
		return result;
	}

	if (this->trackpoints.size() < 2) {
		return result;
	}

	/* Test if there's anything worth calculating. */
	bool okay = false;
	for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {
		if ((*iter)->altitude.is_valid()) {
			okay = true;
			break;
		}
	}
	if (!okay) {
		return result;
	}

	result = this->make_values_altitude_over_time_helper();

	assert (result.n_points == (int) this->get_tp_count());

	return result;
}




/**
   Make a speed/distance map.
*/
TrackData Track::make_track_data_speed_over_distance(void) const
{
	TrackData result;

	double total_length = this->get_length_value_including_gaps();
	if (total_length <= 0) {
		return result;
	}

	const int tp_count = this->get_tp_count();
	const TrackData data_dt = this->make_values_distance_over_time_helper();

	result.allocate_vector(tp_count);

	int i = 0;
	result.x[i] = 0;
	result.y[i] = 0;
	i++;

	for (; i < tp_count; i++) {
		if (data_dt.x[i] <= data_dt.x[i - 1]) {
			/* Handle glitch in values of consecutive time stamps.
			   TODO_LATER: improve code that calculates pseudo-values of result when a glitch has been found. */
			qDebug() << SG_PREFIX_W << "Glitch in timestamps" << i << data_dt.x[i] << data_dt.x[i - 1];
			result.x[i] = result.x[i - 1]; /* TODO_LATER: This won't work for a two or more invalid timestamps in a row. */
			result.y[i] = 0;
		} else {
			/* Iterate over 'n + 1 + n' points of a track to get
			   an average speed for that part.  This will
			   essentially interpolate between segments, which I
			   think is right given the usage of
			   'get_length_value_including_gaps'. n == 0 is no averaging. */
			const int n = 0;
			double delta_d = 0.0;
			double delta_t = 0.0;
			for (int j = i - n; j <= i + n; j++) {
				if (j - 1 >= 0 && j < tp_count) {
					delta_d += (data_dt.y[j] - data_dt.y[j - 1]);
					delta_t += (data_dt.x[j] - data_dt.x[j - 1]);
				}
			}

			result.y[i] = delta_d / delta_t;
			result.x[i] = result.x[i - 1] + (delta_d / (n + 1 + n)); /* Accumulate the distance. */
		}
	}

	assert(i == tp_count);

	result.valid = true;
	return result;
}



/**
 * @trk:                  The Track on which to find a Trackpoint
 * @meters_from_start:    The distance along a track that the trackpoint returned is near
 * @get_next_point:       Since there is a choice of trackpoints, this determines which one to return
 * @tp_metres_from_start: For the returned Trackpoint, returns the distance along the track
 *
 * TODO_MAYBE: Consider changing the boolean get_next_point into an enum with these options PREVIOUS, NEXT, NEAREST
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
bool Track::select_tp_by_percentage_dist(double reldist, double *meters_from_start, tp_idx tp_index)
{
	this->iterators[tp_index].iter_valid = false;

	if (this->trackpoints.empty()) {
		return false;
	}

	double dist = this->get_length_value_including_gaps() * reldist;
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
			this->iterators[tp_index].iter = last_iter;
			this->iterators[tp_index].iter_valid = true;
			return true;
		} else {
			return false;
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

	this->iterators[tp_index].iter = iter;
	this->iterators[tp_index].iter_valid = true;
	return true;
}




bool Track::select_tp_by_percentage_time(double reltime, int tp_index)
{
	this->iterators[tp_index].iter_valid = false;
	if (this->trackpoints.empty()) {
		return false;
	}

	time_t t_start = (*this->trackpoints.begin())->timestamp.get_value();
	time_t t_end = (*std::prev(this->trackpoints.end()))->timestamp.get_value();
	time_t t_total = t_end - t_start;
	time_t t_pos = t_start + t_total * reltime;

	auto iter = this->trackpoints.begin();
	while (iter != this->trackpoints.end()) {
		if ((*iter)->timestamp.get_value() == t_pos) {
			break;
		}

		if ((*iter)->timestamp.get_value() > t_pos) {
			if (iter == this->trackpoints.begin()) { /* First trackpoint. */
				break;
			}
			time_t t_before = t_pos - (*std::prev(iter))->timestamp.get_value();
			time_t t_after = (*iter)->timestamp.get_value() - t_pos;
			if (t_before <= t_after) {
				iter--;
			}
			break;
		} else if (std::next(iter) == this->trackpoints.end()
			   && (t_pos < ((*iter)->timestamp.get_value() + 3))) { /* Last trackpoint: accommodate for round-off. */
			break;
		} else {
			;
		}
		iter++;
	}

	if (iter == this->trackpoints.end()) {
		return false;
	}



	this->iterators[tp_index].iter_valid = true;
	this->iterators[tp_index].iter = iter;
	return true;
}




sg_ret Track::get_tp_relative_timestamp(time_t & seconds_from_start, tp_idx tp_index)
{
	Trackpoint * tp = this->get_tp(tp_index);
	if (NULL == tp) {
		return sg_ret::err;
	}

	seconds_from_start = tp->timestamp.get_value() - (*this->trackpoints.begin())->timestamp.get_value();

	return sg_ret::ok;
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
		if ((*iter)->timestamp.is_valid()
		    && (*std::prev(iter))->timestamp.is_valid()
		    && !(*iter)->newsegment) {

			speed = Coord::distance((*iter)->coord, (*std::prev(iter))->coord)
				/ Time::get_abs_diff((*iter)->timestamp, (*std::prev(iter))->timestamp).get_value();

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
	Altitude max_alt(VIK_VAL_MAX_ALT, HeightUnit::Metres);

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
	Altitude min_alt(VIK_VAL_MIN_ALT, HeightUnit::Metres);

	for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {
		if ((*iter)->altitude < min_alt) {
			min_alt = (*iter)->altitude;
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




bool Track::get_minmax_alt(Altitude & min_alt, Altitude & max_alt) const
{
	if (this->trackpoints.empty()) {
		return false;
	}

	auto iter = this->trackpoints.begin();
	if (!(*iter)->altitude.is_valid()) {
		return false;
	}
	iter++;


	min_alt = Altitude(VIK_VAL_MIN_ALT, HeightUnit::Metres);
	max_alt = Altitude(VIK_VAL_MAX_ALT, HeightUnit::Metres);

	for (; iter != this->trackpoints.end(); iter++) {
		const Altitude & tmp_alt = (*iter)->altitude;
		if (tmp_alt > max_alt) {
			max_alt = tmp_alt;
		}

		if (tmp_alt < min_alt) {
			min_alt = tmp_alt;
		}
	}
	return true;
}




bool Track::get_distances(std::vector<double> & distances) const
{
	if (this->trackpoints.empty()) {
		return false;
	}

	qDebug() << SG_PREFIX_D << "Will reserve" << this->trackpoints.size() << "cells for distances";
	distances.reserve(this->trackpoints.size());

	double acc = 0.0;
	int i = 0;

	distances.push_back(acc);
	i++;

	for (auto iter = std::next(this->trackpoints.begin()); iter != this->trackpoints.end(); iter++) {
		const double delta = Coord::distance((*iter)->coord, (*std::prev(iter))->coord);
		acc += delta;

		distances.push_back(acc);
		i++;
	}
	qDebug() << SG_PREFIX_D << "Filled" << i << distances.size() << "cells with distances";

	return true;
}




bool Track::get_speeds(std::vector<double> & speeds) const
{
	if (this->trackpoints.empty()) {
		return false;
	}

	qDebug() << SG_PREFIX_D << "Will reserve" << this->trackpoints.size() << "cells for speeds";
	speeds.reserve(this->trackpoints.size());

	int i = 0;

	speeds.push_back(0.0);
	i++;

	for (auto iter = std::next(this->trackpoints.begin()); iter != this->trackpoints.end(); iter++) {
		const double delta_d = Coord::distance((*iter)->coord, (*std::prev(iter))->coord);
		const time_t delta_t = (*iter)->timestamp.get_value() - (*std::prev(iter))->timestamp.get_value();
		if (delta_t) {
			speeds.push_back(delta_d/delta_t);
		} else {
			speeds.push_back(0.0);
		}
		i++;
	}
	qDebug() << SG_PREFIX_D << "Filled" << i << speeds.size() << "cells with speeds";

	return true;
}




void Track::marshall(Pickle & pickle)
{
	pickle.put_raw_object((char *) this, sizeof (Track));

	/* we'll fill out number of trackpoints later */
	unsigned int intp = pickle.data_size();
	unsigned int len;
	pickle.put_raw_object((char *) &len, sizeof(len));



	auto iter = this->trackpoints.begin();
	unsigned int ntp = 0;
	while (iter != this->trackpoints.end()) {
		pickle.put_raw_object((char *) *iter, sizeof (Trackpoint));
		pickle.put_string((*iter)->name);
		iter++;
		ntp++;
	}
#if TODO_2_LATER
	*(unsigned int *)(byte_array->data + intp) = ntp;
#endif

	pickle.put_string(this->name);
	pickle.put_string(this->comment);
	pickle.put_string(this->description);
	pickle.put_string(this->source);
	/* TODO_2_LATER: where is ->type? */
}




/*
 * Take a byte array and convert it into a Track.
 */
Track * Track::unmarshall(Pickle & pickle)
{
	const pickle_size_t data_size = pickle.take_size();
	const QString type_id = pickle.take_string();

#ifdef K_TODO_LATER
	Track * new_trk = new Track(((Track *)pickle.data)->is_route());
	/* Basic properties: */
	new_trk->visible = ((Track *)pickle.data)->visible;
	new_trk->draw_name_mode = ((Track *)pickle.data)->draw_name_mode;
	new_trk->max_number_dist_labels = ((Track *)pickle.data)->max_number_dist_labels;
	new_trk->has_color = ((Track *)pickle.data)->has_color;
	new_trk->color = ((Track *)pickle.data)->color;
	new_trk->bbox = ((Track *)pickle.data)->bbox;

	pickle.data += sizeof(*new_trk);

	unsigned int ntp = *(unsigned int *) pickle.data;
	pickle.data += sizeof(ntp);

	for (unsigned int i = 0; i < ntp; i++) {
		Trackpoint * new_tp = new Trackpoint();
		memcpy(new_tp, pickle.data, sizeof(*new_tp));
		pickle.data += sizeof(*new_tp);

		new_tp->name = pickle.take_string();

		new_trk->trackpoints.push_back(new_tp);
	}

	new_trk->name = pickle.take_string();
	new_trk->comment = pickle.take_string();
	new_trk->description = pickle.take_string();
	new_trk->source = pickle.take_string();
	/* TODO_2_LATER: where is ->type? */
#else
	Track * new_trk = new Track(false);
#endif

	return new_trk;
}




LatLonBBox Track::get_bbox(void) const
{
	return this->bbox;
}




/**
   (Re)Calculate the bounds of the given track,
   updating the track's bounds data.
   This should be called whenever a track's trackpoints are changed.
*/
void Track::recalculate_bbox(void)
{
	if (0 == this->trackpoints.size()) {
		return;
	}

	this->bbox.invalidate();
	for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {
		const LatLon lat_lon = (*iter)->coord.get_latlon();
		this->bbox.expand_with_lat_lon(lat_lon);
	}
	this->bbox.validate();

	/* TODO_2_LATER: enable this debug and verify whether it appears only
	   once for a given track during import ("acquire") of the
	   track from external file. */
	// qDebug() << SG_PREFIX_D << "Recalculated bounds of track:" << this->bbox;
}




/**
   Shift all timestamps to be relatively offset from 1901-01-01.

   @return sg_ret::err_arg if track is empty
   @return sg_reg::err_algo on function errors
   @return sg_ret::ok on success
*/
sg_ret Track::anonymize_times(void)
{
	if (this->trackpoints.empty()) {
		return sg_ret::err_arg;
	}

	const QDateTime century = QDateTime::fromString("1901-01-01T00:00:00Z", Qt::ISODate);
	if (century.isNull() || !century.isValid()) {
		/* This should be fixed during development and fixed. */
		qDebug() << SG_PREFIX_E << "Failed to convert date";
		return sg_ret::err_algo;
	}
	/* This will be a negative value */
	qint64 century_secs = century.toMSecsSinceEpoch() / MSECS_PER_SEC; /* TODO_MAYBE: use toSecsSinceEpoch() when new version of QT library becomes more available. */


	time_t offset = 0;
	for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {
		Trackpoint * tp = *iter;
		if (tp->timestamp.is_valid()) {
			/* Calculate an offset in time using the first available timestamp. */
			if (offset == 0) {
				offset = tp->timestamp.get_value() - century_secs;
			}

			/* Apply this offset to shift all timestamps towards 1901 & hence anonymising the time.
			   Note that the relative difference between timestamps is kept - thus calculating speeds will still work. */
			tp->timestamp.value -= offset;
		}
	}

	return sg_ret::ok;
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
	if (!tp->timestamp.is_valid()) {
		return;
	}

	time_t tsfirst = tp->timestamp.get_value();

	/* Find the end of the track and the last timestamp. */
	iter = prev(this->trackpoints.end());

	tp = *iter;
	if (tp->timestamp.is_valid()) {
		time_t tsdiff = tp->timestamp.get_value() - tsfirst;

		double tr_dist = this->get_length_value_including_gaps();
		double cur_dist = 0.0;

		if (tr_dist > 0) {
			iter = this->trackpoints.begin();
			/* Apply the calculated timestamp to all trackpoints except the first and last ones. */
			while (std::next(iter) != this->trackpoints.end()
			       && std::next(std::next(iter)) != this->trackpoints.end()) {

				iter++;
				cur_dist += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);

				(*iter)->timestamp.value = (cur_dist / tr_dist) * tsdiff + tsfirst;
				(*iter)->timestamp.set_valid(true);
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
		if (!(skip_existing && (*iter)->altitude.is_valid())) {
			/* TODO_LATER: of the 4 possible choices we have for choosing an
			   elevation (trackpoint in between samples), choose the one
			   with the least elevation change as the last. */
			const Altitude elev = DEMCache::get_elev_by_coord((*iter)->coord, DemInterpolation::Best);
			if (elev.is_valid()) {
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

	/* As in apply_dem_data() - use 'best' interpolation method. */
	auto last = std::prev(this->trackpoints.end());
	const Altitude elev = DEMCache::get_elev_by_coord((*last)->coord, DemInterpolation::Best);
	if (elev.is_valid()) {
		(*last)->altitude = elev;
	}
}




/**
 * Apply elevation smoothing over range of trackpoints between the list start and end points.
 */
void Track::smoothie(TrackPoints::iterator start, TrackPoints::iterator stop, const Altitude & elev1, const Altitude & elev2, unsigned int points)
{
	/* If was really clever could try and weigh interpolation according to the distance between trackpoints somehow.
	   Instead a simple average interpolation for the number of points given. */
	double change = (elev2 - elev1).get_value() / (points + 1);
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
	Altitude elev; /* Initially invalid. */

	Trackpoint * tp_missing = NULL;
	auto iter_first = this->trackpoints.end();;
	unsigned int points = 0;

	for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {
		Trackpoint * tp = *iter;

		if (!tp->altitude.is_valid()) {
			if (flat) {
				/* Simply assign to last known value. */
				if (elev.is_valid()) {
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
			if (points > 0 && elev.is_valid()) {
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
   \brief Function to compare two tracks by their first timestamp
**/
bool Track::compare_timestamp(const Track & a, const Track & b)
{
	Trackpoint * tpa = NULL;
	Trackpoint * tpb = NULL;

	if (!a.trackpoints.empty()) {
		tpa = *a.trackpoints.begin();
	}

	if (!b.trackpoints.empty()) {
		tpb = *b.trackpoints.begin();
	}

	if (tpa && tpb) {
		return tpa->timestamp < tpb->timestamp;
	}

	/* Any other combination of one or both trackpoints missing. */
	return false;
}




TrackPoints::iterator Track::begin()
{
	return this->trackpoints.begin();
}




TrackPoints::iterator Track::end()
{
	return this->trackpoints.end();
}




bool Track::empty(void) const
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
		qDebug() << SG_PREFIX_E << "Failed to find existing trackpoint in track" << this->name << "in" << __FUNCTION__ << __LINE__;
		return;
	}

	/* std::list::insert() inserts element before position indicated by iter.
	   Iterator can be end() - then element is inserted before end(). */

	if (before) {
		/* insert() by its nature will insert element before given iterator.
		   We can always perform this operation, even if iter is at ::begin(). */
	} else {
		iter++;
		/* Even if iter is at this moment at end(), insert()
		   will safely insert element before the end(). */
	}

	this->trackpoints.insert(iter, tp_new);

	return;
}




std::list<Rect *> * Track::get_rectangles(LatLon * wh)
{
	std::list<Rect *> * rectangles = new std::list<Rect *>;

	bool new_map = true;
	Coord coord_tl;
	Coord coord_br;
	auto iter = this->trackpoints.begin();
	while (iter != this->trackpoints.end()) {
		Coord * cur_coord = &(*iter)->coord;
		if (new_map) {
			cur_coord->get_area_coordinates(wh, &coord_tl, &coord_br);
			Rect * rect = new Rect;
			rect->tl = coord_tl;
			rect->br = coord_br;
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




/* Comparison function used to sort trackpoints. */
bool Trackpoint::compare_timestamps(const Trackpoint * a, const Trackpoint * b)
{
	return a->timestamp < b->timestamp;
}




void Track::sublayer_menu_track_misc(LayerTRW * parent_layer_, QMenu & menu, QMenu * upload_submenu)
{
	QAction * qa = NULL;

	qa = upload_submenu->addAction(QIcon::fromTheme("go-up"), tr("Upload to &OSM..."));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (upload_to_osm_traces_cb()));

	/* Currently filter with functions all use shellcommands and thus don't work in Windows. */
#ifndef WINDOWS
	qa = menu.addAction(QIcon::fromTheme("INDEX"), tr("Use with &Filter"));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (use_with_filter_cb()));
#endif

	/* ATM This function is only available via the layers panel, due to needing a panel. */
	if (ThisApp::get_layers_panel()) {

		Acquire::set_context(ThisApp::get_main_window(),
				     ThisApp::get_main_viewport(),
				     ThisApp::get_layers_panel()->get_top_layer(),
				     ThisApp::get_layers_panel()->get_selected_layer());
		Acquire::set_target(parent_layer_, this);
		QMenu * submenu = Acquire::create_bfilter_track_menu(&menu);
		if (submenu) {
			menu.addMenu(submenu);
		}
	}

#ifdef VIK_CONFIG_GEOTAG
	qa = menu.addAction(tr("Geotag &Images..."));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (geotagging_track_cb()));
#endif
}





void Track::sublayer_menu_track_route_misc(LayerTRW * parent_layer_, QMenu & menu, QMenu * upload_submenu)
{
	QAction * qa = NULL;

	Track * track = parent_layer_->get_edited_track();

	if (parent_layer_->get_track_creation_in_progress()) {
		qa = menu.addAction(tr("&Finish Track"));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (finish_track_cb()));
		menu.addSeparator();

#if 1
		/* Consistency check. */
		if (!track) {
			qDebug() << SG_PREFIX_E << "Track: menu: inconsistency 1: edited item does not exist";
		}
		if (!track->is_track()) {
			qDebug() << SG_PREFIX_E << "Track: menu: inconsistency 2: expected edited item to be track";
		}
		if (!this->is_track()) {
			qDebug() << SG_PREFIX_E << "Track: menu: inconsistency 3: expected this item to be track";
		}
#endif

	} else if (parent_layer_->get_route_creation_in_progress()) {
		qa = menu.addAction(tr("&Finish Route"));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (finish_route_cb()));
		menu.addSeparator();

#if 1
		/* Consistency check. */
		if (!track) {
			qDebug() << SG_PREFIX_E << "Track: menu: inconsistency 4: edited item does not exist";
		}
		if (!track->is_route()) {
			qDebug() << SG_PREFIX_E << "Track: menu: inconsistency 5: expected edited item to be route";
		}
		if (!this->is_route()) {
			qDebug() << SG_PREFIX_E << "Track: menu: inconsistency 6: expected this item to be route";
		}
#endif

	}

	qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), this->is_track() ? tr("&View Track") : tr("&View Route"));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (rezoom_to_show_full_cb()));

	{
		QMenu * goto_submenu = menu.addMenu(QIcon::fromTheme("go-jump"), tr("&Goto"));

		qa = goto_submenu->addAction(QIcon::fromTheme("go-first"), tr("&Startpoint"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (goto_startpoint_cb()));

		qa = goto_submenu->addAction(QIcon::fromTheme("go-jump"), tr("\"&Center\""));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (goto_center_cb()));

		qa = goto_submenu->addAction(QIcon::fromTheme("go-last"), tr("&Endpoint"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (goto_endpoint_cb()));

		qa = goto_submenu->addAction(QIcon::fromTheme("go-top"), tr("&Highest Altitude"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (goto_max_alt_cb()));

		qa = goto_submenu->addAction(QIcon::fromTheme("go-bottom"), tr("&Lowest Altitude"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (goto_min_alt_cb()));

		/* Routes don't have speeds. */
		if (this->is_track()) {
			qa = goto_submenu->addAction(QIcon::fromTheme("media-seek-forward"), tr("&Maximum Speed"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (goto_max_speed_cb()));
		}
	}

	{

		QMenu * combine_submenu = menu.addMenu(QIcon::fromTheme("CONNECT"), tr("Co&mbine"));

		/* Routes don't have times or segments... */
		if (this->is_track()) {
			qa = combine_submenu->addAction(tr("&Merge By Time..."));
			connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (merge_by_timestamp_cb()));

			qa = combine_submenu->addAction(tr("Merge &Segments"));
			connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (merge_by_segment_cb()));
		}

		qa = combine_submenu->addAction(tr("Merge &With Other Tracks..."));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (merge_with_other_cb()));

		qa = combine_submenu->addAction(this->is_track() ? tr("&Append Track...") : tr("&Append Route..."));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (append_track_cb()));

		qa = combine_submenu->addAction(this->is_track() ? tr("Append &Route...") : tr("Append &Track..."));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (append_other_cb()));
	}



	{
		QMenu * split_submenu = menu.addMenu(QIcon::fromTheme("DISCONNECT"), tr("&Split"));

		/* Routes don't have times or segments... */
		if (this->is_track()) {
			qa = split_submenu->addAction(tr("&Split By Time..."));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (split_by_timestamp_cb()));

			/* ATM always enable parent_layer_ entry - don't want to have to analyse the track before displaying the menu - to keep the menu speedy. */
			qa = split_submenu->addAction(tr("Split By Se&gments"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (split_by_segments_cb()));
		}

		qa = split_submenu->addAction(tr("Split By &Number of Points..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (split_by_n_points_cb()));

		qa = split_submenu->addAction(tr("Split at Selected &Trackpoint"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (split_at_selected_trackpoint_cb()));
		/* Make it available only when a trackpoint is selected. */
		qa->setEnabled(track && track->has_selected_tp());
	}



	{
		QMenu * insert_submenu = menu.addMenu(QIcon::fromTheme("list-add"), tr("&Insert Points"));

		qa = insert_submenu->addAction(QIcon::fromTheme(""), tr("Insert Point &Before Selected Point"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (insert_point_before_cb()));
		/* Make it available only when a point is selected. */
		qa->setEnabled(track && track->has_selected_tp());

		qa = insert_submenu->addAction(QIcon::fromTheme(""), tr("Insert Point &After Selected Point"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (insert_point_after_cb()));
		/* Make it available only when a point is selected. */
		qa->setEnabled(track && this->has_selected_tp());
	}


	{
		QMenu * delete_submenu = menu.addMenu(QIcon::fromTheme("list-delete"), tr("Delete Poi&nts"));

		qa = delete_submenu->addAction(QIcon::fromTheme("list-delete"), tr("Delete &Selected Point"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_point_selected_cb()));
		/* Make it available only when a point is selected. */
		qa->setEnabled(track && track->has_selected_tp());

		qa = delete_submenu->addAction(tr("Delete Points With The Same &Position"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_points_same_position_cb()));

		qa = delete_submenu->addAction(tr("Delete Points With The Same &Time"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_points_same_time_cb()));
	}

	{
		QMenu * transform_submenu = menu.addMenu(QIcon::fromTheme("CONVERT"), tr("&Transform"));
		{
			QMenu * dem_submenu = transform_submenu->addMenu(QIcon::fromTheme("vik-icon-DEM Download"), tr("&Apply DEM Data"));

			qa = dem_submenu->addAction(tr("&Overwrite"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (apply_dem_data_all_cb()));
			qa->setToolTip(tr("Overwrite any existing elevation values with DEM values"));

			qa = dem_submenu->addAction(tr("&Keep Existing"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (apply_dem_data_only_missing_cb()));
			qa->setToolTip(tr("Keep existing elevation values, only attempt for missing values"));
		}

		{
			QMenu * smooth_submenu = transform_submenu->addMenu(tr("&Smooth Missing Elevation Data"));

			qa = smooth_submenu->addAction(tr("&Interpolated"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (missing_elevation_data_interp_cb()));
			qa->setToolTip(tr("Interpolate between known elevation values to derive values for the missing elevations"));

			qa = smooth_submenu->addAction(tr("&Flat"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (missing_elevation_data_flat_cb()));
			qa->setToolTip(tr("Set unknown elevation values to the last known value"));
		}

		qa = transform_submenu->addAction(QIcon::fromTheme("CONVERT"), this->is_track() ? tr("C&onvert to a Route") : tr("C&onvert to a Track"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (convert_track_route_cb()));

		/* Routes don't have timestamps - so these are only available for tracks. */
		if (this->is_track()) {
			qa = transform_submenu->addAction(tr("&Anonymize Times"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (anonymize_times_cb()));
			qa->setToolTip(tr("Shift timestamps to a relative offset from 1901-01-01"));

			qa = transform_submenu->addAction(tr("&Interpolate Times"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (interpolate_times_cb()));
			qa->setToolTip(tr("Reset trackpoint timestamps between the first and last points such that track is traveled at equal speed"));
		}
	}


	qa = menu.addAction(QIcon::fromTheme("go-back"), this->is_track() ? tr("&Reverse Track") : tr("&Reverse Route"));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (reverse_cb()));

	if (this->is_route()) {
		qa = menu.addAction(QIcon::fromTheme("edit-find"), tr("Refine Route..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (refine_route_cb()));
	}

	/* ATM Parent_Layer_ function is only available via the layers panel, due to the method in finding out the maps in use. */
	if (ThisApp::get_layers_panel()) {
		qa = menu.addAction(QIcon::fromTheme("vik-icon-Maps Download"), this->is_track() ? tr("Down&load Maps Along Track...") : tr("Down&load Maps Along Route..."));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (download_map_along_track_cb()));
	}

	qa = menu.addAction(QIcon::fromTheme("document-save-as"), this->is_track() ? tr("&Export Track as GPX...") : tr("&Export Route as GPX..."));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (export_track_as_gpx_cb()));

	qa = menu.addAction(QIcon::fromTheme("list-add"), this->is_track() ? tr("E&xtend Track End") : tr("E&xtend Route End"));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (extend_track_end_cb()));

	if (this->is_route()) {
		qa = menu.addAction(QIcon::fromTheme("vik-icon-Route Finder"), tr("Extend &Using Route Finder"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (extend_track_end_route_finder_cb()));
	}

	/* ATM can't upload a single waypoint but can do waypoints to a GPS.
	   TODO_LATER: add this menu item to Waypoints as well? */
	if (this->type_id != "sg.trw.waypoint") {
		qa = upload_submenu->addAction(QIcon::fromTheme("go-forward"), tr("&Upload to GPS..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (upload_to_gps_cb()));
	}
}




bool Track::add_context_menu_items(QMenu & menu, bool tree_view_context_menu)
{
	QAction * qa = NULL;
	bool rv = false;
	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;


	rv = true;
	qa = menu.addAction(QIcon::fromTheme("document-properties"), tr("&Properties"));
	if (this->props_dialog) {
		/* A properties dialog window is already opened.
		   Don't give possibility to open a duplicate properties dialog window. */
		qa->setEnabled(false);
	}
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (properties_dialog_cb()));



	qa = menu.addAction(QIcon::fromTheme("document-properties"), tr("P&rofile"));
	if (this->profile_dialog) {
		/* A profile dialog window is already opened.
		   Don't give possibility to open a duplicate profile dialog window. */
		qa->setEnabled(false);
	}
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (profile_dialog_cb()));


	qa = menu.addAction(tr("&Statistics"));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (statistics_dialog_cb()));


	/* Common "Edit" items. */
	{
		assert (this->menu_operation_ids == (MenuOperationCut | MenuOperationCopy | MenuOperationDelete));

		qa = menu.addAction(QIcon::fromTheme("edit-cut"), QObject::tr("Cut"));
		QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (cut_sublayer_cb()));

		qa = menu.addAction(QIcon::fromTheme("edit-copy"), QObject::tr("Copy"));
		QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (copy_sublayer_cb()));

		qa = menu.addAction(QIcon::fromTheme("edit-delete"), QObject::tr("Delete"));
		qa->setData((unsigned int) this->get_uid());
		if (this->is_track()) {
			QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (delete_track_cb()));
		} else {
			QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (delete_route_cb()));
		}
	}


	menu.addSeparator();


	QMenu * external_submenu = menu.addMenu(QIcon::fromTheme("EXECUTE"), tr("Externa&l"));

	/* These are only made available if a suitable program is installed. */
	if ((g_have_astro_program || g_have_diary_program)
	    && this->is_track()) {

		if (g_have_diary_program) {
			qa = external_submenu->addAction(QIcon::fromTheme("SPELL_CHECK"), QObject::tr("&Diary"));
			QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (open_diary_cb()));
			qa->setToolTip(QObject::tr("Open diary program at this date"));
		}

		if (g_have_astro_program) {
			qa = external_submenu->addAction(QObject::tr("&Astronomy"));
			QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (open_astro_cb()));
			qa->setToolTip(QObject::tr("Open astronomy program at this date and location"));
		}
	}


	LayerTRW * trw = (LayerTRW *) this->owning_layer;


	layer_trw_sublayer_menu_all_add_external_tools(trw, external_submenu);


#ifdef VIK_CONFIG_GOOGLE
	if (this->is_route() && this->is_valid_google_route()) {
		qa = menu.addAction(QIcon::fromTheme("applications-internet"), tr("&View Google Directions"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (google_route_webpage_cb()));
	}
#endif


	QMenu * upload_submenu = menu.addMenu(QIcon::fromTheme("go-up"), tr("&Upload"));

	this->sublayer_menu_track_route_misc(trw, menu, upload_submenu);


	/* Some things aren't usable with routes. */
	if (this->is_track()) {
		this->sublayer_menu_track_misc(trw, menu, upload_submenu);
	}


	/* Only show in viewport context menu, and only when a trackpoint is selected. */
	if (!tree_view_context_menu && this->has_selected_tp()) {
		menu.addSeparator();

		qa = menu.addAction(QIcon::fromTheme("document-properties"), tr("&Edit Trackpoint"));
		connect(qa, SIGNAL (triggered(bool)), trw, SLOT (edit_trackpoint_cb()));
	}


	return rv;
}




void Track::goto_startpoint_cb(void)
{
	if (!this->empty()) {
		Viewport * viewport = ThisApp::get_main_viewport();
		this->owning_layer->request_new_viewport_center(viewport, this->get_tp_first()->coord);
	}
}




void Track::goto_center_cb(void)
{
	if (this->empty()) {
		return;
	}

	LayerTRW * parent_layer_ = (LayerTRW *) this->owning_layer;
	Viewport * viewport = ThisApp::get_main_viewport();

	const Coord coord(this->get_bbox().get_center_lat_lon(), parent_layer_->coord_mode);
	parent_layer_->request_new_viewport_center(viewport, coord);
}




void Track::goto_endpoint_cb(void)
{
	if (this->empty()) {
		return;
	}

	Viewport * viewport = ThisApp::get_main_viewport();
	this->owning_layer->request_new_viewport_center(viewport, this->get_tp_last()->coord);
}




void Track::goto_max_speed_cb()
{
	Trackpoint * tp = this->get_tp_by_max_speed();
	if (!tp) {
		return;
	}

	Viewport * viewport = ThisApp::get_main_viewport();
	this->owning_layer->request_new_viewport_center(viewport, tp->coord);
}




void Track::goto_max_alt_cb(void)
{
	Trackpoint * tp = this->get_tp_by_max_alt();
	if (!tp) {
		return;
	}

	Viewport * viewport = ThisApp::get_main_viewport();
	this->owning_layer->request_new_viewport_center(viewport, tp->coord);
}




void Track::goto_min_alt_cb(void)
{
	Trackpoint * tp = this->get_tp_by_min_alt();
	if (!tp) {
		return;
	}

	Viewport * viewport = ThisApp::get_main_viewport();
	this->owning_layer->request_new_viewport_center(viewport, tp->coord);
}




void Track::anonymize_times_cb(void)
{
	const sg_ret ret = this->anonymize_times();
	switch (ret) {
	case sg_ret::ok:
		break;
	default:
		Dialog::warning(QObject::tr("Failed to anonymize timestamps"), NULL);
		break;
	}
}




void Track::interpolate_times_cb(void)
{
	this->interpolate_times();
}




bool Track::properties_dialog(void)
{
	this->properties_dialog_cb();
	return true;
}




void Track::properties_dialog_cb(void)
{
	if (this->name.isEmpty()) {
		return;
	}

	track_properties_dialog(this, ThisApp::get_main_window());
}




void Track::statistics_dialog_cb(void)
{
	if (this->name.isEmpty()) {
		return;
	}

	track_statistics_dialog(this, ThisApp::get_main_window());
}




void Track::profile_dialog_cb(void)
{
	if (this->name.isEmpty()) {
		return;
	}

	track_profile_dialog(this, ThisApp::get_main_viewport(), ThisApp::get_main_window());
}




/**
 * A common function for applying the elevation smoothing and reporting the results.
 */
void Track::smooth_it(bool flat)
{
	unsigned long n_changed = this->smooth_missing_elevation_data(flat);
	/* Inform user how much was changed. */
	const QString msg = QObject::tr("%n points adjusted", "", n_changed); /* TODO_2_LATER: verify that "%n" format correctly handles unsigned long. */
	Dialog::info(msg, ThisApp::get_main_window());
}




void Track::missing_elevation_data_interp_cb(void)
{
	this->smooth_it(false);
}




void Track::missing_elevation_data_flat_cb(void)
{
	this->smooth_it(true);
}




/*
 * Automatically change the viewport to center on the track and zoom to see the extent of the track.
 */
void Track::rezoom_to_show_full_cb(void)
{
	if (this->empty()) {
		return;
	}

	ThisApp::get_main_viewport()->set_bbox(this->get_bbox());
	ThisApp::get_main_viewport()->request_redraw("Re-align viewport to show whole contents of TRW Track");
}




/* The same tooltip for route and track. */
QString Track::get_tooltip(void) const
{
	/* Could be a better way of handling strings - but this works. */
	QString timestamp_string;
	QString duration_string;

	/* Compact info: Short date eg (11/20/99), duration and length.
	   Hopefully these are the things that are most useful and so promoted into the tooltip. */
	if (!this->empty() && this->get_tp_first()->timestamp.is_valid()) {
		/* %x     The preferred date representation for the current locale without the time. */
		timestamp_string = this->get_tp_first()->timestamp.strftime_utc("%x: ");
		const Time duration = this->get_duration(true);
		if (duration.is_valid() && duration.get_value() > 0) {
			duration_string = QObject::tr("- %1").arg(duration.to_duration_string());
		}
	}

	/* Get length and consider the appropriate distance units. */
	const QString distance_string = this->get_length().convert_to_unit(Preferences::get_unit_distance()).to_string();
	const QString result = QObject::tr("%1%2 %3").arg(timestamp_string).arg(distance_string).arg(duration_string);


	return result;
}




/**
   A common function for applying the DEM values and reporting the results
*/
void Track::apply_dem_data_common(bool skip_existing_elevations)
{
	LayersPanel * panel = ThisApp::get_layers_panel();
	if (!panel->has_any_layer_of_type(LayerType::DEM)) {
		Dialog::error(tr("No DEM layers available, thus no DEM values can be applied."), ThisApp::get_main_window());
		return;
	}

	unsigned long n_changed = this->apply_dem_data(skip_existing_elevations);

	/* Inform user how much was changed. */
	const QString msg = QObject::tr("%n points adjusted", "", n_changed); /* TODO_2_LATER: verify that "%n" format correctly handles unsigned long. */
	Dialog::info(msg, ThisApp::get_main_window());
}




void Track::apply_dem_data_all_cb(void)
{
	this->apply_dem_data_common(false);
}




void Track::apply_dem_data_only_missing_cb(void)
{
	this->apply_dem_data_common(true);
}




void Track::export_track_as_gpx_cb(void)
{
	const QString title = this->is_route() ? tr("Export Route as GPX") : tr("Export Track as GPX");
	const QString auto_save_name = append_file_ext(this->name, SGFileType::GPX);

	this->export_track(title, auto_save_name, SGFileType::GPX);
}




void Track::export_track(const QString & title, const QString & default_file_name, SGFileType file_type)
{
	QFileDialog file_selector(ThisApp::get_main_window(), title);
	file_selector.setFileMode(QFileDialog::AnyFile); /* Specify new or select existing file. */
	file_selector.setAcceptMode(QFileDialog::AcceptSave);

	if (last_directory_url.isValid()) {
		file_selector.setDirectoryUrl(last_directory_url);
	}

	file_selector.selectFile(default_file_name);

	if (QDialog::Accepted == file_selector.exec()) {
		const QString output_file_full_path = file_selector.selectedFiles().at(0);

		last_directory_url = file_selector.directoryUrl();

		ThisApp::get_main_window()->set_busy_cursor();
		const SaveStatus export_status = VikFile::export_trw_track(this, output_file_full_path, file_type, true);
		ThisApp::get_main_window()->clear_busy_cursor();

		if (SaveStatus::Code::Success != export_status) {
			export_status.show_error_dialog(ThisApp::get_main_window());
		}
	}
}




/**
   \brief Open a diary at the date of the track

   Call this method only for a track, not for route.
*/
void Track::open_diary_cb(void)
{
	if (!this->empty() && (*this->trackpoints.begin())->timestamp.is_valid()) {
		const QString date_buf = (*this->trackpoints.begin())->timestamp.strftime_utc("%Y-%m-%d");
		((LayerTRW *) this->owning_layer)->diary_open(date_buf);
	} else {
		Dialog::info(tr("This track has no date information."), ThisApp::get_main_window());
	}
}




/**
   \brief Open an astronomy program at the date & position of the track center or trackpoint

   Call this method only for a track, not for route.
*/
void Track::open_astro_cb(void)
{
	Trackpoint * tp = NULL;
	if (this->has_selected_tp()) {
		/* Current trackpoint. */
		tp = this->get_selected_tp();

	} else if (!this->empty()) {
		/* Otherwise first trackpoint. */
		tp = *this->begin();
	} else {
		/* Give up. */
		return;
	}

	if (tp->timestamp.is_valid()) {
		LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;

		const QString date_buf = tp->timestamp.strftime_utc("%Y%m%d");
		const QString time_buf = tp->timestamp.strftime_utc("%H:%M:%S");

		const LatLon ll = tp->coord.get_latlon();
		const QString lat_str = Astro::convert_to_dms(ll.lat);
		const QString lon_str = Astro::convert_to_dms(ll.lon);
		const QString alt_str = QString("%1").arg((int)round(tp->altitude.get_value()));
		Astro::open(date_buf, time_buf, lat_str, lon_str, alt_str, parent_layer->get_window());
	} else {
		Dialog::info(tr("This track has no date information."), ThisApp::get_main_window());
	}
}




/**
   \brief Reverse a track
*/
void Track::reverse_cb(void)
{
	this->reverse();
	this->emit_tree_item_changed("Track reversed");
}




QString Track::sublayer_rename_request(const QString & new_name)
{
	static const QString empty_string("");

	/* No actual change to the name supplied. */
	if (!this->name.isEmpty()) {
		if (new_name == this->name) {
			return empty_string;
		}
	}


	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;
	LayerTRWTracks * tracks = NULL;
	QString message;

	if (this->is_track()) {
		tracks = &parent_layer->tracks;
		message = tr("A track with the name \"%1\" already exists. Really rename to the same name?").arg(new_name);
	} else {
		tracks = &parent_layer->routes;
		message = tr("A route with the name \"%1\" already exists. Really rename to the same name?").arg(new_name);
	}

	if (tracks->find_track_by_name(new_name)) {
		/* An existing track/route has been found with the requested name. */
		if (!Dialog::yes_or_no(message, ThisApp::get_main_window())) {
			return empty_string;
		}
	}


	/* Update track name and refresh GUI parts. */
	this->set_name(new_name);




	/* Update any subwindows/dialogs that could be displaying this track which has changed name.
	   Currently only one additional window may display a track: Track Edit Dialog. */
	if (parent_layer->tpwin) {
		parent_layer->tpwin->set_dialog_title(new_name);
	}


	/* Update the dialog windows if any of them is visible. */
	this->update_properties_dialog();
	this->update_profile_dialog();


	parent_layer->tree_view->apply_tree_item_name(this);
	parent_layer->tree_view->sort_children(tracks, parent_layer->track_sort_order);

	ThisApp::get_layers_panel()->emit_items_tree_updated_cb("Redrawing items after renaming track");

	return new_name;
}




bool Track::handle_selection_in_tree(void)
{
	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;

#ifdef K_TODO_MAYBE
	/* TODO_MAYBE: to be implemented? */
	parent_layer->set_statusbar_msg_info_trk(this);
#endif
	parent_layer->reset_internal_selections(); /* No other tree item (that is a sublayer of this layer) is selected... */
	parent_layer->set_edited_track(this); /* But this tree item is selected. */

	qDebug() << SG_PREFIX_I << "Tree item" << this->name << "becomes selected tree item";
	g_selected.add_to_set(this);

	return true;
}




/**
 * Only handles a single track
 * It assumes the track belongs to the TRW Layer (it doesn't check this is the case)
 */
void Track::draw_tree_item(Viewport * viewport, bool highlight_selected, bool parent_is_selected)
{
	/* Check the layer for visibility (including all the parents visibilities). */
	if (!this->tree_view->get_tree_item_visibility_with_parents(this)) {
		return;
	}

	if (1) {
		if (g_selected.is_in_set(this)) {
			qDebug() << SG_PREFIX_I << "Drawing tree item" << this->name << "as selected (selected directly)";
		} else if (parent_is_selected) {
			qDebug() << SG_PREFIX_I << "Drawing tree item" << this->name << "as selected (selected through parent)";
		} else {
			qDebug() << SG_PREFIX_I << "Drawing tree item" << this->name << "as non-selected";
		}
	}

	const bool item_is_selected = parent_is_selected || g_selected.is_in_set(this);
	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;
	parent_layer->painter->draw_track(this, viewport, item_is_selected && highlight_selected);
}




typedef struct screen_pos {
	double x = 0.0;
	double y = 0.0;
} screen_pos;




sg_ret Track::draw_e_ft(Viewport * viewport, struct my_data * data)
{
	QPen pen;
	pen.setColor(this->has_color ? this->color : "blue");
	pen.setWidth(1);


	Altitude min_alt;
	Altitude max_alt;
	if (!this->get_minmax_alt(min_alt, max_alt)) {
		qDebug() << SG_PREFIX_N << "Can't get altitudes";
		return sg_ret::err;
	}
	if (!(min_alt.is_valid() && max_alt.is_valid())) {
		qDebug() << SG_PREFIX_N << "Altitudes are invalid";
		return sg_ret::err;
	}


	const double margin = 0.05;
	const double alt_min = min_alt.get_value() - margin * min_alt.get_value();
	const double alt_max = max_alt.get_value() + margin * max_alt.get_value();
	const double visible_range = alt_max - alt_min;

	const int bottom = data->height;

	const double x_scale = 1.0 * this->trackpoints.size() / data->width;

	qDebug() << __FUNCTION__ << "+++++++++++++++++++";

	screen_pos cur_pos;
	screen_pos last_pos;

	last_pos.x = 0.0;
	last_pos.y = bottom;

	double col = 0.0;
	for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {
		const double value = (*iter)->altitude.is_valid() ? (*iter)->altitude.get_value() : 0.0;

		cur_pos.x = col;
		cur_pos.y = bottom - bottom * (value - alt_min) / visible_range;

		viewport->draw_line(pen,
				    last_pos.x, last_pos.y,
				    cur_pos.x, cur_pos.y);

		last_pos = cur_pos;
		col = col + (1 / x_scale);
	}

	return sg_ret::ok;
}




sg_ret Track::draw_d_ft(Viewport * viewport, struct my_data * data)
{
	QPen pen;
	pen.setColor(this->has_color ? this->color : "blue");
	pen.setWidth(1);


	std::vector<double> distances;
	if (!this->get_distances(distances) || 0 == distances.size()) {
		qDebug() << SG_PREFIX_N << "Can't get distances";
		return sg_ret::err;
	}

	const double margin = 0.05;
	const double dist_min = distances[0] - margin * distances[0];
	const double dist_max = distances[distances.size() - 1] + margin * distances[distances.size() - 1];
	const double visible_range = dist_max - dist_min;

	const int bottom = data->height;

	const double x_scale = 1.0 * distances.size() / data->width;

	qDebug() << __FUNCTION__ << "+++++++++++++++++++";

	screen_pos cur_pos;
	screen_pos last_pos;

	last_pos.x = 0.0;
	last_pos.y = bottom;

	double col = 0.0;
	for (auto iter = distances.begin(); iter != distances.end(); iter++) {
		const double value = *iter;

		cur_pos.x = col;
		cur_pos.y = bottom - bottom * (value - dist_min) / visible_range;

		viewport->draw_line(pen,
				    last_pos.x, last_pos.y,
				    cur_pos.x, cur_pos.y);

		last_pos = cur_pos;
		col = col + (1 / x_scale);
	}

	return sg_ret::ok;
}



sg_ret Track::draw_v_ft(Viewport * viewport, struct my_data * data)
{
	QPen pen;
	pen.setColor(this->has_color ? this->color : "blue");
	pen.setWidth(1);


	std::vector<double> values_uu;
	if (!this->get_speeds(values_uu)) {
		qDebug() << SG_PREFIX_N << "Can't get speeds";
		return sg_ret::err;
	}

	const size_t n_values = values_uu.size();
	if (0 == n_values) {
		qDebug() << SG_PREFIX_N << "There were zero speeds";
		return sg_ret::err;
	}


	const double margin = 0.05;
	const double min_value_uu = 0; // TODO: correct calculation
	const double max_value_uu = 6; // TODO: correct calculation
	const double visible_values_range_uu = max_value_uu - min_value_uu;

	const int bottom = data->height;

	const double x_scale = 1.0 * n_values / data->width;

	qDebug() << __FUNCTION__ << "+++++++++++++++++++";

	screen_pos cur_pos;
	screen_pos last_pos;

	last_pos.x = 0.0;
	last_pos.y = bottom;

	double col = 0.0;
	for (auto iter = values_uu.begin(); iter != values_uu.end(); iter++) {
		const auto current_value_uu = *iter;

		cur_pos.x = col;
		cur_pos.y = bottom - bottom * (current_value_uu - min_value_uu) / visible_values_range_uu;

		viewport->draw_line(pen,
				    last_pos.x, last_pos.y,
				    cur_pos.x, cur_pos.y);

		last_pos = cur_pos;
		col = col + (1 / x_scale);
	}

	return sg_ret::ok;
}




sg_ret Track::draw_tree_item(Viewport * viewport, struct my_data * in_data, ViewportDomain x_domain, ViewportDomain y_domain)
{
	if (x_domain != ViewportDomain::Time) {
		qDebug() << SG_PREFIX_W << "Can't draw non-time based graph";
		return sg_ret::err;
	}

	switch (y_domain) {
	case ViewportDomain::Elevation:
		return this->draw_e_ft(viewport, in_data);
		break;
	case ViewportDomain::Distance:
		return this->draw_d_ft(viewport, in_data);
		break;
	case ViewportDomain::Speed:
		return this->draw_v_ft(viewport, in_data);
		break;
	default:
		qDebug() << SG_PREFIX_W << "Can't draw graphs of this y-domain";
		return sg_ret::err;
	}
}




void Track::upload_to_gps_cb(void)
{
	((LayerTRW *) this->owning_layer)->upload_to_gps(this);
}




void Track::upload_to_osm_traces_cb(void)
{
	OSMTraces::upload_trw_layer((LayerTRW *) this->owning_layer, this);
}




void Track::convert_track_route_cb(void)
{
	if (this->is_track()) {
		/* Converting a track to a route may lead to a data
		   loss, so give user a chance to change his mind. */

		const Speed avg = this->get_average_speed();
		if (this->get_segment_count() > 1
		    || (avg.is_valid() && avg.get_value() > 0.0)) {

			if (!Dialog::yes_or_no(tr("Converting a track to a route removes extra track data such as segments, timestamps, etc...\n"
						  "Do you want to continue?"), ThisApp::get_main_window())) {
				return;
			}
		}
	}

	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;


	/* Detach from old location. */
	parent_layer->detach_from_container(this);
	parent_layer->detach_from_tree(this);


	/* Convert and attach to new location. */
	this->type_id = this->is_route() ? "sg.trw.track": "sg.trw.route";
	if (this->is_track()) {
		parent_layer->add_track(this);
	} else {
		/* Extra steps when converting to route. */
		this->merge_segments();
		this->to_routepoints();

		parent_layer->add_route(this);
	}


	/* Redraw. */
	parent_layer->emit_tree_item_changed("Indicating change to TRW Layer after converting track <--> route");
}




/*
 * Use code in separate file for this feature as reasonably complex.
 */
void Track::geotagging_track_cb(void)
{
	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;

	/* Set to true so that thumbnails are generate later if necessary. */
	parent_layer->has_missing_thumbnails = true;
	trw_layer_geotag_dialog(ThisApp::get_main_window(), parent_layer, NULL, this);
}




static Coord * get_next_coord(Coord *from, Coord *to, LatLon *dist, double gradient)
{
	if ((dist->lon >= std::abs(to->ll.lon - from->ll.lon))
	    && (dist->lat >= std::abs(to->ll.lat - from->ll.lat))) {

		return NULL;
	}

	Coord * coord = new Coord();
	coord->mode = CoordMode::LatLon;

	if (std::abs(gradient) < 1) {
		if (from->ll.lon > to->ll.lon) {
			coord->ll.lon = from->ll.lon - dist->lon;
		} else {
			coord->ll.lon = from->ll.lon + dist->lon;
		}
		coord->ll.lat = gradient * (coord->ll.lon - from->ll.lon) + from->ll.lat;
	} else {
		if (from->ll.lat > to->ll.lat) {
			coord->ll.lat = from->ll.lat - dist->lat;
		} else {
			coord->ll.lat = from->ll.lat + dist->lat;
		}
		coord->ll.lon = (1/gradient) * (coord->ll.lat - from->ll.lat) + from->ll.lat;
	}

	return coord;
}




static void add_fillins(std::list<Coord *> & list, Coord * from, Coord * to, LatLon *dist)
{
	/* TODO_LATER: handle vertical track (to->ll.lon - from->ll.lon == 0). */
	double gradient = (to->ll.lat - from->ll.lat)/(to->ll.lon - from->ll.lon);

	Coord * next = from;
	while (true) {
		if ((next = get_next_coord(next, to, dist, gradient)) == NULL) {
			break;
		}
		list.push_front(next);
	}

	return;
}




static int get_download_area_width(const VikingZoomLevel & viking_zoom_level, LatLon * wh) /* kamilFIXME: viewport is unused, why? */
{
	/* TODO_LATER: calculating based on current size of viewport. */
	const double w_at_zoom_0_125 = 0.0013;
	const double h_at_zoom_0_125 = 0.0011;
	const double zoom_factor = viking_zoom_level.get_x() / 0.125;

	wh->lat = h_at_zoom_0_125 * zoom_factor;
	wh->lon = w_at_zoom_0_125 * zoom_factor;

	return 0;   /* All OK. */
}




std::list<Rect *> * Track::get_map_rectangles(const VikingZoomLevel & viking_zoom_level)
{
	if (this->empty()) {
		return NULL;
	}

	LatLon wh;
	if (get_download_area_width(viking_zoom_level, &wh)) {
		return NULL;
	}

	std::list<Rect *> * rects_to_download = this->get_rectangles(&wh);
	std::list<Coord *> fillins;

	/* 'fillin' doesn't work in UTM mode - potentially ending up in massive loop continually allocating memory - hence don't do it. */
	/* Seems that ATM the function get_next_coord works only for LatLon. */
	if (((LayerTRW *) this->owning_layer)->get_coord_mode() == CoordMode::LatLon) {

		/* Fill-ins for far apart points. */
		std::list<Rect *>::iterator cur_rect;
		std::list<Rect *>::iterator next_rect;

		for (cur_rect = rects_to_download->begin();
		     (next_rect = std::next(cur_rect)) != rects_to_download->end();
		     cur_rect++) {

			if ((wh.lon < std::abs((*cur_rect)->center.ll.lon - (*next_rect)->center.ll.lon))
			    || (wh.lat < std::abs((*cur_rect)->center.ll.lat - (*next_rect)->center.ll.lat))) {

				add_fillins(fillins, &(*cur_rect)->center, &(*next_rect)->center, &wh);
			}
		}
	} else {
		qDebug() << SG_PREFIX_W << "'download map' feature works only in Mercator mode";
	}

	Coord coord_tl;
	Coord coord_br;
	for (auto iter = fillins.begin(); iter != fillins.end(); iter++) {
		Coord * cur_coord = *iter;
		cur_coord->get_area_coordinates(&wh, &coord_tl, &coord_br);
		Rect * rect = new Rect;
		rect->tl = coord_tl;
		rect->br = coord_br;
		rect->center = *cur_coord;
		rects_to_download->push_front(rect);

		delete *iter;
	}

	return rects_to_download;
}




#ifdef VIK_CONFIG_GOOGLE

bool Track::is_valid_google_route()
{
	return this->is_route()
		&& this->comment.size() > 7
		&& !strncmp(this->comment.toUtf8().constData(), "from:", 5);
}




void Track::google_route_webpage_cb(void)
{
	const QString escaped = Util::uri_escape(this->comment);
	QString webpage = QString("http://maps.google.com/maps?f=q&hl=en&q=%1").arg(escaped);
	open_url(webpage);
}

#endif




#ifndef WINDOWS
void Track::track_use_with_bfilter_cb(void)
{
	Acquire::set_bfilter_track(this);
}
#endif




/*
  Refine the selected route with a routing engine.
  The routing engine is selected by the user, when requestiong the job.
*/
void Track::refine_route_cb(void)
{
	static int last_engine = 0;

	if (this->empty()) {
		return;
	}

	Window * main_window = ThisApp::get_main_window();
	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;

	/* Check size of the route */
	const int nb = this->get_tp_count();
	if (nb > 100) {
		if (!Dialog::yes_or_no(tr("Refining a track with many points (%1) is unlikely to yield sensible results. Do you want to continue?").arg(nb))) {
			return;
		}
	}


	/* Select engine from dialog */
	BasicDialog dialog(main_window);
	dialog.setWindowTitle(QObject::tr("Refine Route with Routing Engine..."));

	QLabel * label = new QLabel(QObject::tr("Select routing engine:"));

	QComboBox * combo = Routing::create_engines_combo(routing_engine_supports_refine);
	combo->setCurrentIndex(last_engine);

	dialog.grid->addWidget(label, 0, 0);
	dialog.grid->addWidget(combo, 1, 0);


	dialog.button_box->button(QDialogButtonBox::Ok)->setDefault(true);

	if (dialog.exec() == QDialog::Accepted) {
		/* Dialog validated: retrieve selected engine and do the job */
		last_engine = combo->currentIndex();
		RoutingEngine * engine = Routing::get_engine_by_index(combo, last_engine);


		/* Force saving track */
		/* FIXME: remove or rename this hack */
		parent_layer->route_finder_check_added_track = true;


		/* The job */
		main_window->set_busy_cursor();
		engine->refine(parent_layer, this);
		main_window->clear_busy_cursor();


		/* FIXME: remove or rename this hack */
		if (parent_layer->route_finder_added_track) {
			parent_layer->route_finder_added_track->recalculate_bbox();
		}

		parent_layer->route_finder_added_track = NULL;
		parent_layer->route_finder_check_added_track = false;

		parent_layer->emit_tree_item_changed("TRW - refine route");
	}
}




sg_ret Track::create_tp_next_to_selected_tp(bool before)
{
	return this->create_tp_next_to_specified_tp(this->iterators[SELECTED], before);
}




/**
   \brief Create a new trackpoint and insert it next to given @param reference_tp.

   Since this method is private, we have pretty good control over
   @reference_tp and can be rather certain that it belongs to the
   track.

   Insert it before or after @param reference_tp, depending on value of @param before.

   The new trackpoint is created at center position between @param
   reference_tp and one of its neighbours: next tp or previous tp.
*/
sg_ret Track::create_tp_next_to_specified_tp(const TrackpointIter & reference_tp, bool before)
{
	if (!reference_tp.iter_valid) {
		return sg_ret::err;
	}

#if 1   /* Debug code. */
	auto iter = std::find(this->trackpoints.begin(), this->trackpoints.end(), *(reference_tp.iter));
	qDebug() << "Will check assertion for track" << this->name;
	assert (iter != this->trackpoints.end());
#endif

	Trackpoint * other_tp = NULL;

	if (before) {
		qDebug() << "------ insert trackpoint before.";
		if (reference_tp.iter == this->begin()) {
			return sg_ret::err;
		}
		other_tp = *std::prev(reference_tp.iter);
	} else {
		qDebug() << "------ insert trackpoint after.";
		if (std::next(reference_tp.iter) == this->end()) {
			return sg_ret::err;
		}
		other_tp = *std::next(reference_tp.iter);
	}

	/* Use current and other trackpoints to form a new
	   trackpoint which is inserted into the tracklist. */
	if (other_tp) {

		Trackpoint * new_tp = new Trackpoint(**reference_tp.iter, *other_tp, ((LayerTRW *) this->owning_layer)->coord_mode);
		/* Insert new point into the appropriate trackpoint list,
		   either before or after the current trackpoint as directed. */

		this->insert(*reference_tp.iter, new_tp, before);
	}

	this->emit_tree_item_changed(before
				     ? "Track changed after adding a trackpoint before specified trackpoint"
				     : "Track changed after adding a trackpoint after specified trackpoint");

	return sg_ret::ok;
}




void Track::cut_sublayer_cb(void)
{
	/* false: don't require confirmation in callbacks. */
	((LayerTRW *) this->owning_layer)->cut_sublayer_common(this, false);
}

void Track::copy_sublayer_cb(void)
{
	((LayerTRW *) this->owning_layer)->copy_sublayer_common(this);
}

void Track::remove_last_trackpoint(void)
{
	if (this->empty()) {
		return;
	}

	auto iter = std::prev(this->trackpoints.end());
	this->erase_trackpoint(iter);
	this->recalculate_bbox();
}




/**
   Simple accessor

   Created to avoid constant casting of Track::owning_layer to LayerTRW* type.
*/
LayerTRW * Track::get_parent_layer_trw() const
{
	return (LayerTRW *) this->owning_layer;

}




TrackData::TrackData()
{
}




void TrackData::invalidate(void)
{
	this->valid = false;
	this->n_points = 0;

	if (this->x) {
		free(this->x);
		this->x = NULL;
	}

	if (this->y) {
		free(this->y);
		this->y = NULL;
	}
}




void TrackData::calculate_min_max(void)
{
	this->x_min = this->x[0];
	this->x_max = this->x[0];

	for (int i = 0; i < this->n_points; i++) {
		qDebug() << "i / x" << i << this->x[i];
		if (this->x[i] > this->x_max) {
			this->x_max = this->x[i];
			qDebug() << "         max = " << this->x_max;
		}

		if (this->x[i] < this->x_min) {
			this->x_min = this->x[i];
			qDebug() << "         min = " << this->x_min;
		}
	}

	this->y_min = this->y[0];
	this->y_max = this->y[0];
	for (int i = 0; i < this->n_points; i++) {
		if (this->y[i] > this->y_max) {
			this->y_max = this->y[i];
		}

		if (this->y[i] < this->y_min) {
			this->y_min = this->y[i];
		}
	}
}




void TrackData::allocate_vector(int n_data_points)
{
	if (this->x) {
		free(this->x);
		this->x = NULL;
	}

	if (this->y) {
		free(this->y);
		this->y = NULL;
	}

	this->x = (double *) malloc(sizeof (double) * n_data_points);
	this->y = (double *) malloc(sizeof (double) * n_data_points);

	memset(this->x, 0, sizeof (double) * n_data_points);
	memset(this->y, 0, sizeof (double) * n_data_points);

	this->n_points = n_data_points;
}




TrackData::TrackData(int n_data_points)
{
	this->allocate_vector(n_data_points);
}




TrackData::~TrackData()
{
	if (this->x) {
		free(this->x);
		this->x = NULL;
	}

	if (this->y) {
		free(this->y);
		this->y = NULL;
	}
}




TrackData & TrackData::operator=(const TrackData & other)
{
	if (&other == this) {
		return *this;
	}

	/* TODO_LATER: compare size of vectors in both objects to see if
	   reallocation is necessary? */

	if (other.x) {
		if (this->x) {
			free(this->x);
			this->x = NULL;
		}
		this->x = (double *) malloc(sizeof (double) * other.n_points);
		memcpy(this->x, other.x, sizeof (double) * other.n_points);
	}

	if (other.y) {
		if (this->y) {
			free(this->y);
			this->y = NULL;
		}
		this->y = (double *) malloc(sizeof (double) * other.n_points);
		memcpy(this->y, other.y, sizeof (double) * other.n_points);
	}

	this->x_min = other.x_min;
	this->x_max = other.x_max;

	this->y_min = other.y_min;
	this->y_max = other.y_max;

	this->valid = other.valid;
	this->n_points = other.n_points;

	return *this;
}




QList<QStandardItem *> Track::get_list_representation(const TreeItemViewFormat & view_format)
{
	QList<QStandardItem *> items;
	QStandardItem * item = NULL;
	QVariant variant;


	const DistanceUnit distance_unit = Preferences::get_unit_distance();
	const SpeedUnit speed_unit = Preferences::get_unit_speed();
	const HeightUnit height_unit = Preferences::get_unit_height();


	LayerTRW * trw = this->get_parent_layer_trw();

	/* 'visible' doesn't include aggegrate visibility. */
	bool a_visible = trw->visible && this->visible;
	a_visible = a_visible && (this->is_route() ? trw->get_routes_visibility() : trw->get_tracks_visibility());


	Qt::DateFormat date_time_format = Qt::ISODate;
	ApplicationState::get_integer(VIK_SETTINGS_SORTABLE_DATE_TIME_FORMAT, (int *) &date_time_format);


	QString tooltip("");
	if (!this->comment.isEmpty()) {
		tooltip = this->comment;
	} else if (!this->description.isEmpty()) {
		tooltip = this->description;
	} else {
		;
	}


	for (const TreeItemViewColumn & col : view_format.columns) {
		switch (col.id) {
		case TreeItemPropertyID::ParentLayer:
			item = new QStandardItem(trw->name);
			item->setToolTip(tooltip);
			item->setEditable(false); /* This dialog is not a good place to edit layer name. */
			items << item;
			break;

		case TreeItemPropertyID::TheItem:
			item = new QStandardItem(this->name);
			item->setToolTip(tooltip);
			variant = QVariant::fromValue(this);
			item->setData(variant, RoleLayerData);
			items << item;
			break;

		case TreeItemPropertyID::Timestamp:
			{
				QString start_date_str;
				if (!this->empty()
				    && (*this->trackpoints.begin())->timestamp.is_valid()) {

					start_date_str = (*this->trackpoints.begin())->timestamp.get_time_string(date_time_format);
				}
				item = new QStandardItem(start_date_str);
				item->setToolTip(tooltip);
				items << item;
			}
			break;

		case TreeItemPropertyID::Visibility:
			item = new QStandardItem();
			item->setToolTip(tooltip);
			item->setCheckable(true);
			item->setCheckState(a_visible ? Qt::Checked : Qt::Unchecked);
			items << item;
			break;

		case TreeItemPropertyID::Editable:
			item = new QStandardItem();
			variant = QVariant::fromValue(this->editable);
			item->setData(variant, RoleLayerData);
			items << item;
			break;

		case TreeItemPropertyID::Comment:
			item = new QStandardItem(this->comment);
			item->setToolTip(tooltip);
			items << item;
			break;


		case TreeItemPropertyID::Length:
			{
				const Distance trk_dist = this->get_length().convert_to_unit(distance_unit);
				item = new QStandardItem();
				item->setToolTip(tooltip);
				variant = QVariant::fromValue(trk_dist.value);
				item->setData(variant, Qt::DisplayRole);
				item->setEditable(false); /* This dialog is not a good place to edit track length. */
				items << item;
			}
			break;

		case TreeItemPropertyID::Duration:
			{
				const Time trk_duration = this->get_duration();
				item = new QStandardItem();
				item->setToolTip(tooltip);
				variant = QVariant::fromValue(trk_duration.get_value());
				item->setData(variant, Qt::DisplayRole);
				item->setEditable(false); /* This dialog is not a good place to edit track duration. */
				items << item;
			}
			break;

		case TreeItemPropertyID::AverageSpeed:
			item = new QStandardItem();
			item->setToolTip(tooltip);
			variant = QVariant::fromValue(this->get_average_speed().convert_to_unit(speed_unit).to_string());
			item->setData(variant, Qt::DisplayRole);
			item->setEditable(false); /* 'Average speed' is not an editable parameter. */
			items << item;
			break;

		case TreeItemPropertyID::MaximumSpeed:
			item = new QStandardItem();
			item->setToolTip(tooltip);
			variant = QVariant::fromValue(this->get_max_speed().convert_to_unit(speed_unit).to_string());
			item->setData(variant, Qt::DisplayRole);
			item->setEditable(false); /* 'Maximum speed' is not an editable parameter. */
			items << item;
			break;

		case TreeItemPropertyID::MaximumHeight:
			{
				Altitude max_alt(0.0, HeightUnit::Metres);
				TrackData altitudes = this->make_track_data_altitude_over_distance(500); /* TODO_LATER: magic number. */
				if (altitudes.valid) {
					altitudes.calculate_min_max();
					max_alt.set_value(altitudes.y_max);
				}

				item = new QStandardItem();
				item->setToolTip(tooltip);
				variant = QVariant::fromValue(max_alt.convert_to_unit(Preferences::get_unit_height()).to_string());
				item->setData(variant, Qt::DisplayRole);
				item->setEditable(false); /* 'Maximum height' is not an editable parameter. */
				items << item;
			}
			break;

		default:
			qDebug() << SG_PREFIX_E << "Unexpected TreeItem Column ID" << (int) col.id;
			break;
		}
	}

	return items;
}




/**
   Update how track is displayed in tree view - primarily update track's icon
*/
sg_ret Track::update_tree_item_properties(void)
{
	if (!this->index.isValid()) {
		qDebug() << SG_PREFIX_E << "Invalid index of tree item";
		return sg_ret::err;
	}


	this->self_assign_timestamp();
	this->tree_view->apply_tree_item_timestamp(this);

	this->self_assign_icon();
	this->tree_view->apply_tree_item_icon(this);


	return sg_ret::ok;
}




double Track::get_tp_distance_percent(tp_idx tp_idx) const
{
	Trackpoint * tp = this->get_tp(tp_idx);
	if (tp == NULL) {
		return NAN;
	}

	double dist = 0.0;
	auto iter = std::next(this->trackpoints.begin());
	for (; iter != this->trackpoints.end(); iter++) {
		dist += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);
		/* Since tp is private member, it will be either NULL
		   (which would be caught by condition above) or will
		   match this comparison. */
		if (tp == *iter) {
			break;
		}
	}

	double pc = NAN;
	if (iter != this->trackpoints.end()) {
		pc = dist / this->track_length_including_gaps;
	}
	return pc;
}




double Track::get_tp_time_percent(tp_idx tp_idx) const
{
	const Trackpoint * tp = this->get_tp(tp_idx);
	if (tp == NULL) {
		return NAN;
	}

	const time_t t_start = (*this->trackpoints.begin())->timestamp.get_value();
	const time_t t_end = (*std::prev(this->trackpoints.end()))->timestamp.get_value();
	const time_t t_total = t_end - t_start;

	return (tp->timestamp.get_value() - t_start) / (1.0 * t_total);
}




Trackpoint * Track::get_tp(tp_idx tp_idx) const
{
	switch (tp_idx) {
	case SELECTED:
		return this->get_selected_tp();
	case HOVERED:
		return this->get_hovered_tp();
	default:
		qDebug() << SG_PREFIX_E << "Unexpected tp index" << tp_idx;
		return NULL;
	}
}




Trackpoint * Track::get_selected_tp(void) const
{
	if (this->iterators[SELECTED].iter_valid) {
		return *this->iterators[SELECTED].iter;
	} else {
		return NULL;
	}
}




Trackpoint * Track::get_hovered_tp(void) const
{
	if (this->iterators[HOVERED].iter_valid) {
		return *this->iterators[HOVERED].iter;
	} else {
		return NULL;
	}
}




void Trackpoint::set_timestamp(const Time & value)
{
	this->timestamp = value;
}



void Trackpoint::set_timestamp(time_t value)
{
	this->timestamp = Time(value);
}




sg_ret Track::get_timestamps(Time & ts_first, Time & ts_last) const
{
	if (this->trackpoints.size() < 2) {
		return sg_ret::err;
	}

        ts_first = (*this->trackpoints.begin())->timestamp;
	ts_last = (*std::prev(this->trackpoints.end()))->timestamp;

	if (!ts_first.is_valid() || !ts_last.is_valid()) {
		return sg_ret::err;
	}

	return sg_ret::ok;
}




/**
   Insert a trackpoint after currently selected trackpoint
*/
void Track::insert_point_after_cb(void)
{
	if (sg_ret::ok != this->create_tp_next_to_specified_tp(this->iterators[SELECTED], false)) {
		qDebug() << SG_PREFIX_E << "Failed to insert trackpoint after selected trackpoint";
	} else {
		this->emit_tree_item_changed("Track changed after inserting trackpoint 'after'");
	}
}




/**
   Insert a trackpoint before currently selected trackpoint
*/
void Track::insert_point_before_cb(void)
{
	if (sg_ret::ok != this->create_tp_next_to_specified_tp(this->iterators[SELECTED], true)) {
		qDebug() << SG_PREFIX_E << "Failed to insert trackpoint before selected trackpoint";
	} else {
		this->emit_tree_item_changed("Track changed after inserting trackpoint 'before'");
	}
}




/**
   Split a track at the currently selected trackpoint
*/
sg_ret Track::split_at_selected_trackpoint_cb(void)
{
	sg_ret ret = this->split_at_trackpoint(this->iterators[SELECTED]);
	if (sg_ret::ok != ret) {
		qDebug() << SG_PREFIX_W << "Failed to split track" << this->name << "at selected trackpoint";
		return ret;
	}

	this->emit_tree_item_changed("Track changed after splitting at selected trackpoint");

	return sg_ret::ok;
}








void LayerTRW::delete_selected_tp(Track * track)
{
	TrackPoints::iterator new_tp_iter = track->delete_trackpoint(track->iterators[SELECTED].iter);

	if (new_tp_iter != track->end()) {
		/* Set to current to the available adjacent trackpoint. */
		track->set_selected_tp(new_tp_iter);
		track->recalculate_bbox();
	} else {
		this->cancel_current_tp(false);
	}
}




/**
   Delete the selected trackpoint
*/
void Track::delete_point_selected_cb(void)
{
	if (!this->has_selected_tp()) {
		return;
	}

	LayerTRW * parent_layer = this->get_parent_layer_trw();

	parent_layer->delete_selected_tp(this);
	parent_layer->deselect_current_trackpoint(this);

	this->emit_tree_item_changed("Deleted selected trackpoint");
}




/**
   Delete adjacent trackpoints at the same position
*/
void Track::delete_points_same_position_cb(void)
{
	const unsigned long n_removed = this->remove_dup_points();

	LayerTRW * parent_layer = this->get_parent_layer_trw();

	parent_layer->deselect_current_trackpoint(this);

	/* Inform user how much was deleted as it's not obvious from the normal view. */
	const QString msg = QObject::tr("Deleted %n points", "", n_removed); /* TODO_2_LATER: verify that "%n" format correctly handles unsigned long. */
	Dialog::info(msg, ThisApp::get_main_window());

	this->emit_tree_item_changed("Deleted trackpoints with the same position");
}




/**
   Delete adjacent trackpoints with the same timestamp
*/
void Track::delete_points_same_time_cb(void)
{
	const unsigned long n_removed = this->remove_same_time_points();

	LayerTRW * parent_layer = this->get_parent_layer_trw();

	parent_layer->deselect_current_trackpoint(this);

	/* Inform user how much was deleted as it's not obvious from the normal view. */
	const QString msg = QObject::tr("Deleted %n points", "", n_removed); /* TODO_2_LATER: verify that "%n" format correctly handles unsigned long. */
	Dialog::info(msg, ThisApp::get_main_window());

	this->emit_tree_item_changed("Deleted trackpoints with the same timestamp");
}




void Track::extend_track_end_cb(void)
{
	Window * window = ThisApp::get_main_window();
	Viewport * viewport = ThisApp::get_main_viewport();
	LayerTRW * parent_layer = this->get_parent_layer_trw();

	window->activate_tool_by_id(this->is_route() ? LAYER_TRW_TOOL_CREATE_ROUTE : LAYER_TRW_TOOL_CREATE_TRACK);

	if (!this->empty()) {
		parent_layer->request_new_viewport_center(viewport, this->get_tp_last()->coord);
	}
}




/**
   Extend a track using route finder
*/
void Track::extend_track_end_route_finder_cb(void)
{
	Window * window = ThisApp::get_main_window();
	Viewport * viewport = ThisApp::get_main_viewport();
	LayerTRW * parent_layer = this->get_parent_layer_trw();

	window->activate_tool_by_id(LAYER_TRW_TOOL_ROUTE_FINDER);

	parent_layer->route_finder_started = true;

	if (!this->empty()) {
		parent_layer->request_new_viewport_center(viewport, this->get_tp_last()->coord);
	}
}




bool Track::is_route(void) const
{
	return this->type_id == "sg.trw.route";
}




bool Track::is_track(void) const
{
	return this->type_id == "sg.trw.track";
}




sg_ret Track::move_selected_tp_forward(void)
{
	if (!this->has_selected_tp()) {
		return sg_ret::err_cond;
	}
	if (std::next(this->iterators[SELECTED].iter) == this->end()) {
		/* Can't go forward if we are already at the end. */
		return sg_ret::err_cond;
	}

	this->iterators[SELECTED].iter++;

	return sg_ret::ok;
}




sg_ret Track::move_selected_tp_back(void)
{
	if (!this->has_selected_tp()) {
		return sg_ret::err_cond;
	}
	if (this->iterators[SELECTED].iter == this->begin()) {
		/* Can't go back if we are already at the beginning. */
		return sg_ret::err_cond;
	}

	this->iterators[SELECTED].iter--;

	return sg_ret::ok;
}




bool Track::has_selected_tp(void) const
{
	return this->iterators[SELECTED].iter_valid;
}




void Track::set_selected_tp(const TrackPoints::iterator & tp_iter)
{
	this->iterators[SELECTED].iter = tp_iter;
	this->iterators[SELECTED].iter_valid = true; /* TODO: calculate value of this field instead of assuming that the iter is always correct. */
}




void Track::reset_selected_tp(void)
{
	this->iterators[SELECTED].iter_valid = false;
}




bool Track::is_selected(void) const
{
	LayerTRW * trw = this->get_parent_layer_trw();
	return trw->get_edited_track() == this;
}




/**
   @title: the title for the dialog
   @layer: The layer, from which a list of tracks should be extracted
   @type_id_string: TreeItem type to be show in list (empty string for both tracks and routes)

  Common method for showing a list of tracks with extended information
*/
void Track::list_dialog(QString const & title, Layer * layer, const QString & type_id_string)
{
	Window * window = layer->get_window();


	std::list<Track *> tree_items;
	if (layer->type == LayerType::Aggregate) {
		((LayerAggregate *) layer)->get_tracks_list(tree_items, type_id_string);
	} else if (layer->type == LayerType::TRW) {
		((LayerTRW *) layer)->get_tracks_list(tree_items, type_id_string);
	} else {
		assert (0);
	}
	if (tree_items.empty()) {
		Dialog::info(QObject::tr("No Tracks found"), window);
		return;
	}


	const HeightUnit height_unit = Preferences::get_unit_height();
	const SpeedUnit speed_unit = Preferences::get_unit_speed();
	const DistanceUnit distance_unit = Preferences::get_unit_distance();
	TreeItemViewFormat view_format;
	if (layer->type == LayerType::Aggregate) {
		view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::ParentLayer, true, QObject::tr("Parent Layer"))); // this->view->horizontalHeader()->setSectionResizeMode(LAYER_NAME_COLUMN, QHeaderView::Interactive);
	}
	view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::TheItem,       true, QObject::tr("Name"))); // this->view->horizontalHeader()->setSectionResizeMode(TRACK_COLUMN, QHeaderView::Interactive);
	view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::Timestamp,     true, QObject::tr("Timestamp"))); // this->view->horizontalHeader()->setSectionResizeMode(DATE_COLUMN, QHeaderView::ResizeToContents);
	view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::Visibility,    true, QObject::tr("Visibility"))); // this->view->horizontalHeader()->setSectionResizeMode(VISIBLE_COLUMN, QHeaderView::ResizeToContents);
	view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::Comment,       true, QObject::tr("Comment"))); // this->view->horizontalHeader()->setSectionResizeMode(COMMENT_COLUMN, QHeaderView::Interactive);
	view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::Length,        true, QObject::tr("Length\n(%1)").arg(Distance::get_unit_full_string(distance_unit)))); // this->view->horizontalHeader()->setSectionResizeMode(LENGTH_COLUMN, QHeaderView::ResizeToContents);
	view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::Duration,      true, QObject::tr("Duration\n(minutes)"))); // this->view->horizontalHeader()->setSectionResizeMode(DURATION_COLUMN, QHeaderView::ResizeToContents);
	view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::AverageSpeed,  true, QObject::tr("Average Speed\n(%1)").arg(Speed::get_unit_string(speed_unit)))); // this->view->horizontalHeader()->setSectionResizeMode(AVERAGE_SPEED_COLUMN, QHeaderView::ResizeToContents);
	view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::MaximumSpeed,  true, QObject::tr("Maximum Speed\n(%1)").arg(Speed::get_unit_string(speed_unit)))); // this->view->horizontalHeader()->setSectionResizeMode(MAXIMUM_SPEED_COLUMN, QHeaderView::ResizeToContents);
	view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::MaximumHeight, true, QObject::tr("Maximum Height\n(%1)").arg(Altitude::get_unit_full_string(height_unit)))); // this->view->horizontalHeader()->setSectionResizeMode(WaypointListModel::Comment, QHeaderView::Stretch); // this->view->horizontalHeader()->setSectionResizeMode(MAXIMUM_HEIGHT_COLUMN, QHeaderView::ResizeToContents);


	TreeItemListDialogHelper<Track *> dialog_helper;
	dialog_helper.show_dialog(title, view_format, tree_items, window);
}




void Track::prepare_for_profile(void)
{
	this->track_length_including_gaps = this->get_length_value_including_gaps();
}
