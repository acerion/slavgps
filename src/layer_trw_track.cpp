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
#include "layer_dem_dem_cache.h"
#include "application_state.h"
#include "util.h"
#include "osm_traces.h"
#include "layer_aggregate.h"

#include "layer_trw.h"
#include "layer_trw_babel_filter.h"
#include "layer_trw_geotag.h"
#include "layer_trw_import.h"
#include "layer_trw_menu.h"
#include "layer_trw_painter.h"
#include "layer_trw_tools.h"
#include "layer_trw_track_internal.h"
#include "layer_trw_track_profile_dialog.h"
#include "layer_trw_track_properties_dialog.h"
#include "layer_trw_trackpoint_properties.h"

#include "window.h"
#include "dialog.h"
#include "layers_panel.h"
#include "tree_view_internal.h"
#include "routing.h"
#include "measurements.h"
#include "preferences.h"
#include "viewport_internal.h"
#include "file.h"
#include "tree_item_list.h"
#include "astro.h"
#include "toolbox.h"




using namespace SlavGPS;




#define SG_MODULE "Layer TRW Track"




extern SelectedTreeItems g_selected;

extern bool g_have_astro_program;
extern bool g_have_diary_program;




/* The last used directories. */
static QUrl last_directory_url;



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
	if (ApplicationState::get_integer(VIK_SETTINGS_TRACK_NAME_MODE, &tmp)) {
		this->draw_name_mode = (TrackDrawNameMode) tmp;
	}

	if (ApplicationState::get_integer(VIK_SETTINGS_TRACK_NUM_DIST_LABELS, &tmp)) {
		this->max_number_dist_labels = tmp;
	}
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
	if (!this->get_name().isEmpty()) {
		this->props_dialog->setWindowTitle(tr("%1 - Track Properties").arg(this->get_name()));
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
	if (!this->get_name().isEmpty()) {
		this->profile_dialog->setWindowTitle(tr("%1 - Track Profile").arg(this->get_name()));
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
		this->m_type_id = Route::type_id();
	} else {
		this->m_type_id = Track::type_id();
	}

	this->ref_count = 1;

	this->has_properties_dialog = true;

	this->m_menu_operation_ids.push_back(StandardMenuOperation::Properties);
	this->m_menu_operation_ids.push_back(StandardMenuOperation::Cut);
	this->m_menu_operation_ids.push_back(StandardMenuOperation::Copy);
	this->m_menu_operation_ids.push_back(StandardMenuOperation::Delete);
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




SGObjectTypeID Track::get_type_id(void) const
{
	return this->m_type_id;
}
SGObjectTypeID Track::type_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.trw.track");
	return id;
}

#if 0
SGObjectTypeID Track::get_type_id(void) const
{
	return this->m_type_id;
}
#endif
SGObjectTypeID Route::type_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.trw.route");
	return id;
}




void Track::copy_properties(const Track & from)
{
	this->visible = from.visible;
	this->draw_name_mode = from.draw_name_mode;
	this->max_number_dist_labels = from.max_number_dist_labels;

	this->set_name(from.get_name());
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
	this->gps_speed = tp.gps_speed;
	this->course = tp.course;
	this->nsats = tp.nsats;

	this->fix_mode = tp.fix_mode;
	this->hdop = tp.hdop;
	this->vdop = tp.vdop;
	this->pdop = tp.pdop;

}




Trackpoint::Trackpoint(Trackpoint const& tp_a, Trackpoint const& tp_b, CoordMode coord_mode)
{
	const LatLon ll_a = tp_a.coord.get_lat_lon();
	const LatLon ll_b = tp_b.coord.get_lat_lon();

	/* Main positional interpolation. */
	this->coord = Coord(LatLon::get_average(ll_a, ll_b), coord_mode);

	/* Now other properties that can be interpolated. */
	this->altitude = (tp_a.altitude + tp_b.altitude) / 2;

	if (tp_a.timestamp.is_valid() && tp_b.timestamp.is_valid()) {
		/* Note here the division is applied to each part, then added.
		   This is to avoid potential overflow issues with a 32 time_t for
		   dates after midpoint of this Unix time on 2004/01/04. */
		this->set_timestamp((tp_a.timestamp / 2) + (tp_b.timestamp / 2));
	}

	if (!std::isnan(tp_a.gps_speed) && !std::isnan(tp_b.gps_speed)) {
		this->gps_speed = (tp_a.gps_speed + tp_b.gps_speed) / 2;
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
		const LatLon lat_lon = tp->coord.get_lat_lon();
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
	return Distance(this->get_length_value_to_trackpoint(tp), DistanceType::Unit::E::Meters);
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
	return Distance(this->get_length_value(), DistanceType::Unit::E::Meters);
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
	return Distance(this->get_length_value_including_gaps(), DistanceType::Unit::E::Meters);
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

		(*iter)->timestamp.invalidate();
		(*iter)->gps_speed = NAN;
		(*iter)->course.invalidate();
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
Duration Track::get_duration(bool segment_gaps) const
{
	Duration duration(0, DurationType::Unit::internal_unit());

	if (this->trackpoints.empty()) {
		return duration;
	}

	/* Ensure times are available. */
	if (this->get_tp_first()->timestamp.is_valid()) {
		/* Get trkpt only once - as using vik_track_get_tp_last() iterates whole track each time. */
		if (segment_gaps) {
			// Simple duration
			Trackpoint * tp_last = this->get_tp_last();
			if (tp_last->timestamp.is_valid()) {
				duration = Duration::get_abs_duration(tp_last->timestamp, this->get_tp_first()->timestamp);
			}
		} else {
			/* Total within segments. */
			for (auto iter = std::next(this->trackpoints.begin()); iter != this->trackpoints.end(); iter++) {
				if ((*iter)->timestamp.is_valid()
				    && (*std::prev(iter))->timestamp.is_valid()
				    && !(*iter)->newsegment) {

					duration += Duration::get_abs_duration((*iter)->timestamp, (*std::prev(iter))->timestamp);
				}
			}
		}
	}

	return duration;
}




/* Code extracted from make_track_data_speed_over_time() and similar functions. */
Duration Track::get_duration(void) const
{
	Duration result(0, DurationType::Unit::internal_unit());

	Time ts_begin;
	Time ts_end;
	if (sg_ret::ok != this->get_timestamps(ts_begin, ts_end)) {
		qDebug() << SG_PREFIX_W << "Can't get track's timestamps";
		return result;
	}

	const Duration duration = Duration::get_abs_duration(ts_end, ts_begin);
	if (!duration.is_valid()) {
		qDebug() << SG_PREFIX_E << "Invalid duration";
		return result;
	}

	if (duration.is_negative()) {
		qDebug() << SG_PREFIX_W << "Negative duration: unsorted trackpoint timestamps?";
		return result;
	}

	result = duration;
	return result;
}




Speed Track::get_average_speed(void) const
{
	Speed result(NAN, SpeedType::Unit::E::MetresPerSecond); /* Invalid by default. */

	if (this->trackpoints.empty()) {
		return result;
	}

	Distance distance(0, DistanceType::Unit::internal_unit());
	Duration duration(0, DurationType::Unit::internal_unit());

	for (auto iter = std::next(this->trackpoints.begin()); iter != this->trackpoints.end(); iter++) {

		if ((*iter)->timestamp.is_valid()
		    && (*std::prev(iter))->timestamp.is_valid()
		    && !(*iter)->newsegment) {

			distance += Coord::distance_2((*iter)->coord, (*std::prev(iter))->coord);
			duration += Duration::get_abs_duration((*iter)->timestamp, (*std::prev(iter))->timestamp);
		}
	}

	if (duration.is_valid() && duration.is_positive()) {
		result.make_speed(distance, duration);
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
Speed Track::get_average_speed_moving(const Duration & track_min_stop_duration) const
{
	Speed result; /* Invalid by default. */

	if (this->trackpoints.empty()) {
		return result;
	}

	Distance distance(0, DistanceType::Unit::internal_unit());
	Duration duration(0, DurationType::Unit::internal_unit());

	for (auto iter = std::next(this->trackpoints.begin()); iter != this->trackpoints.end(); iter++) {
		if ((*iter)->timestamp.is_valid()
		    && (*std::prev(iter))->timestamp.is_valid()
		    && !(*iter)->newsegment) {

			const Duration timestamp_diff = Duration::get_abs_duration((*iter)->timestamp, (*std::prev(iter))->timestamp);
			if (timestamp_diff < track_min_stop_duration) {
				distance += Coord::distance_2((*iter)->coord, (*std::prev(iter))->coord);
				duration += Duration::get_abs_duration((*iter)->timestamp, (*std::prev(iter))->timestamp);
			}
		}
	}

	if (duration.is_valid() && duration.is_positive()) {
		result.make_speed(distance, duration);
	}

	return result;
}




sg_ret Track::calculate_max_speed(void)
{
	this->max_speed = Speed(NAN, SpeedType::Unit::E::MetresPerSecond); /* Invalidate. */

	if (this->trackpoints.empty()) {
		return sg_ret::ok;
	}

	Speed maxspeed(0.0, SpeedType::Unit::E::MetresPerSecond); /* Initialized as valid, but zero. */

	for (auto iter = std::next(this->trackpoints.begin()); iter != this->trackpoints.end(); iter++) {
		if ((*iter)->timestamp.is_valid()
		    && (*std::prev(iter))->timestamp.is_valid()
		    && !(*iter)->newsegment) {

			const Distance distance = Coord::distance_2((*iter)->coord, (*std::prev(iter))->coord);
			const Duration duration = Duration::get_abs_duration((*iter)->timestamp, (*std::prev(iter))->timestamp);
			Speed speed;
			speed.make_speed(distance, duration);

			if (speed.is_valid() && speed > maxspeed) {
				maxspeed = speed;
			}
		}
	}

	this->max_speed = maxspeed; /* Set the value even if detected max speed is zero. */

	return sg_ret::ok;
}




const Speed & Track::get_max_speed(void) const
{
	return this->max_speed;
}




void Track::change_coord_mode(CoordMode dest_mode)
{
	for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {
		(*iter)->coord.recalculate_to_mode(dest_mode);
	}
}




bool Track::get_total_elevation_gain(Altitude & delta_up, Altitude & delta_down) const
{
	auto size = this->trackpoints.size();
	if (size <= 1) {
		/* Checking for more existence of more than one
		   trackpoint, because the loop below will start from
		   a second element of ::trackpoints. */
		qDebug() << SG_PREFIX_N << "Can't get elevation gain for track of size" << size;
		return false;
	}

	delta_up = Altitude(0, AltitudeType::Unit::internal_unit());
	delta_down = Altitude(0, AltitudeType::Unit::internal_unit());

	size_t n = 0;
	for (auto iter = std::next(this->trackpoints.begin()); iter != this->trackpoints.end(); iter++) {

		if (!(*iter)->altitude.is_valid() || !(*std::prev(iter))->altitude.is_valid()) {
			continue;
		}

		const Altitude diff = (*iter)->altitude - (*std::prev(iter))->altitude;
		if (diff.is_positive()) {
			delta_up += diff;
		} else {
			delta_down += diff;
		}
		n++;
	}

	if (0 == n) {
		qDebug() << SG_PREFIX_N << "Zero valid elevation gains";
		delta_up.invalidate();
		delta_down.invalidate();
		return false;
	} else {
		return true;
	}
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




/**
   TODO_LATER: selecting a trackpoint just on basis of pointer address is weak. Maybe also add some unique-per-tp id?
*/
sg_ret Track::select_tp(const Trackpoint * tp)
{
	this->selected_children.references.front().m_iter_valid = false;

	if (this->trackpoints.empty()) {
		return sg_ret::err;
	}

	auto iter = std::next(this->trackpoints.begin());
	for (; iter != this->trackpoints.end(); iter++) {
		if (tp == *iter) {
			this->selected_children.references.front().m_iter_valid = true;
			this->selected_children.references.front().m_iter = iter;
			return sg_ret::ok;
		}
	}

	return sg_ret::err;
}




Trackpoint * Track::get_tp_by_max_speed() const
{
	if (this->trackpoints.empty()) {
		return NULL;
	}

	Trackpoint * speed_max_tp = NULL;
	Speed speed_max;
	Speed speed;
	bool initialized = false;

	for (auto iter = std::next(this->trackpoints.begin()); iter != this->trackpoints.end(); iter++) {
		if ((*iter)->timestamp.is_valid()
		    && (*std::prev(iter))->timestamp.is_valid()
		    && !(*iter)->newsegment) {

			const Distance distance = Coord::distance_2((*iter)->coord, (*std::prev(iter))->coord);
			const Duration duration = Duration::get_abs_duration((*iter)->timestamp, (*std::prev(iter))->timestamp);

			if (sg_ret::ok != speed.make_speed(distance, duration)) {
				continue;
			}

			if (!initialized) {
				speed_max = speed;
				initialized = true;
			} else {
				if (speed > speed_max) {
					speed_max = speed;
					speed_max_tp = *iter;
				}
			}
		}
	}

	if (!speed_max_tp) {
		return NULL;
	}

	return speed_max_tp;
}




Trackpoint * Track::get_tp_with_highest_altitude(void) const
{
	if (this->trackpoints.empty()) {
		return NULL;
	}

	Trackpoint * max_alt_tp = NULL;
	Altitude max_alt; /* Initially invalid. */
	bool initialized = false;

	for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {
		if (!(*iter)->altitude.is_valid()) {
			continue;
		}
		if (!initialized) {
			max_alt = (*iter)->altitude;
			initialized = true;
		} else {
			if ((*iter)->altitude > max_alt) {
				max_alt = (*iter)->altitude;
				max_alt_tp = *iter;
			}
		}
	}

	return max_alt_tp; /* May be NULL. */
}




Trackpoint * Track::get_tp_with_lowest_altitude(void) const
{
	if (this->trackpoints.empty()) {
		return NULL;
	}

	Trackpoint * min_alt_tp = NULL;
	Altitude min_alt; /* Initially invalid. */
	bool initialized = false;

	for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {
		if (!(*iter)->altitude.is_valid()) {
			continue;
		}
		if (!initialized) {
			min_alt = (*iter)->altitude;
			initialized = true;
		} else {
			if ((*iter)->altitude < min_alt) {
				min_alt = (*iter)->altitude;
				min_alt_tp = *iter;
			}
		}
	}

	return min_alt_tp; /* May be NULL. */
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


	/* Initialize as invalid. */
	min_alt = Altitude();
	max_alt = Altitude();
	bool initialized = false;

	for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {
		if (!(*iter)->altitude.is_valid()) {
			continue;
		}

		if (!initialized) {
			min_alt = (*iter)->altitude;
			max_alt = (*iter)->altitude;
			initialized = true;
		} else {
			const Altitude & tmp_alt = (*iter)->altitude;
			if (tmp_alt > max_alt) {
				max_alt = tmp_alt;
			}
			if (tmp_alt < min_alt) {
				min_alt = tmp_alt;
			}
		}
	}
	return initialized;
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
#if TODO_LATER
	*(unsigned int *)(byte_array->data + intp) = ntp;
#endif

	pickle.put_string(this->get_name());
	pickle.put_string(this->comment);
	pickle.put_string(this->description);
	pickle.put_string(this->source);
	/* TODO_LATER: where is ->type? */
}




/*
 * Take a byte array and convert it into a Track.
 */
Track * Track::unmarshall(__attribute__((unused)) Pickle & pickle)
{
#ifdef TODO_LATER
	const pickle_size_t data_size = pickle.take_size();
	const QString type_id = pickle.take_string();

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
	/* TODO_LATER: where is ->type? */
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
		const LatLon lat_lon = (*iter)->coord.get_lat_lon();
		this->bbox.expand_with_lat_lon(lat_lon);
	}
	this->bbox.validate();

	/* TODO_LATER: enable this debug and verify whether it appears only
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
	const qint64 century_secs = century.toMSecsSinceEpoch() / MSECS_PER_SEC; /* TODO_MAYBE: use toSecsSinceEpoch() when new version of QT library becomes more available. */

	time_t offset = 0;
	for (auto iter = this->trackpoints.begin(); iter != this->trackpoints.end(); iter++) {
		Trackpoint * tp = *iter;
		if (tp->timestamp.is_valid()) {
			/* Calculate an offset in time using the first available timestamp. */
			if (offset == 0) {
				offset = tp->timestamp.ll_value() - century_secs;
			}

			/* Apply this offset to shift all timestamps towards 1901 & hence anonymising the time.
			   Note that the relative difference between timestamps is kept - thus calculating speeds will still work. */
			tp->timestamp.set_ll_value(tp->timestamp.ll_value() - offset);
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

	const Time first_timestamp = tp->timestamp;

	/* Find the end of the track and the last timestamp. */
	iter = prev(this->trackpoints.end());

	tp = *iter;
	if (tp->timestamp.is_valid()) {
		const Time timestamp_diff = tp->timestamp - first_timestamp;

		double full_track_length = this->get_length_value_including_gaps();
		double current_distance = 0.0;

		if (full_track_length > 0) {
			iter = this->trackpoints.begin();
			/* Apply the calculated timestamp to all trackpoints except the first and last ones. */
			while (std::next(iter) != this->trackpoints.end()
			       && std::next(std::next(iter)) != this->trackpoints.end()) {

				iter++;
				current_distance += Coord::distance((*iter)->coord, (*std::prev(iter))->coord);

				const double relative = current_distance / full_track_length;

				(*iter)->timestamp = first_timestamp + timestamp_diff * relative;
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
			const Altitude elev = DEMCache::get_elev_by_coord((*iter)->coord, DEMInterpolation::Best);
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
	const Altitude elev = DEMCache::get_elev_by_coord((*last)->coord, DEMInterpolation::Best);
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
	double change = (elev2 - elev1).ll_value() / (points + 1);
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
		qDebug() << SG_PREFIX_E << "Failed to find existing trackpoint in track" << this->get_name();
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




std::list<CoordRectangle> Track::get_coordinate_rectangles(const LatLon & single_rectangle_span)
{
	std::list<CoordRectangle> rectangles;

	bool new_map = true;
	auto iter = this->trackpoints.begin();
	while (iter != this->trackpoints.end()) {
		Coord * cur_coord = &(*iter)->coord;
		if (new_map) {
			CoordRectangle rect;
			cur_coord->get_coord_rectangle(single_rectangle_span, rect);
			rectangles.push_front(rect);
			new_map = false;
			iter++;
			continue;
		}
		bool found = false;
		for (auto rect_iter = rectangles.begin(); rect_iter != rectangles.end(); rect_iter++) {
			if (cur_coord->is_inside((*rect_iter).m_coord_tl, (*rect_iter).m_coord_br)) {
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

	qa = menu.addAction(QIcon::fromTheme("INDEX"), tr("Use with &Filter"));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (use_with_babel_filter_cb()));

	QMenu * filter_submenu = menu.addMenu(QIcon::fromTheme("go-jump"), tr("&Filter"));
	parent_layer_->layer_trw_filter->add_babel_filters_for_track_submenu(*filter_submenu);

#ifdef VIK_CONFIG_GEOTAG
	qa = menu.addAction(tr("Geotag &Images..."));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (geotagging_track_cb()));
#endif
}





void Track::sublayer_menu_track_route_misc(LayerTRW * parent_layer_, QMenu & menu, QMenu * upload_submenu)
{
	QAction * qa = NULL;

	Track * track = parent_layer_->selected_track_get();

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
		qa->setEnabled(track && 1 == track->selected_children.get_count());
	}



	{
		QMenu * insert_submenu = menu.addMenu(QIcon::fromTheme("list-add"), tr("&Insert Points"));

		qa = insert_submenu->addAction(QIcon::fromTheme(""), tr("Insert Point &Before Selected Point"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (insert_point_before_cb()));
		/* Make it available only when a point is selected. */
		qa->setEnabled(track && 1 == track->selected_children.get_count());

		qa = insert_submenu->addAction(QIcon::fromTheme(""), tr("Insert Point &After Selected Point"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (insert_point_after_cb()));
		/* Make it available only when a point is selected. */
		qa->setEnabled(track && 1 == this->selected_children.get_count());
	}


	{
		QMenu * delete_submenu = menu.addMenu(QIcon::fromTheme("list-delete"), tr("Delete Poi&nts"));

		const size_t tp_count = track->selected_children.get_count();
		qa = delete_submenu->addAction(QIcon::fromTheme("list-delete"),
					       tp_count <= 1 ? tr("Delete &Selected Point") : tr("Delete &Selected Points"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_all_selected_tp_cb()));
		qa->setEnabled(track && 0 != tp_count);

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
	if (this->get_type_id() != Waypoint::type_id()) {
		qa = upload_submenu->addAction(QIcon::fromTheme("go-forward"), tr("&Upload to GPS..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (upload_to_gps_cb()));
	}
}




sg_ret Track::menu_add_standard_operations(QMenu & menu, const StandardMenuOperations & ops, __attribute__((unused)) bool in_tree_view)
{
	QAction * qa = NULL;

	if (ops.is_member(StandardMenuOperation::Properties)) {
		qa = menu.addAction(QIcon::fromTheme("document-properties"), tr("&Properties"));
		if (this->props_dialog) {
			/* A properties dialog window is already opened.
			   Don't give possibility to open a duplicate properties dialog window. */
			qa->setEnabled(false);
		}
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (show_properties_dialog_cb()));
	}

	if (ops.is_member(StandardMenuOperation::Cut)) {
		qa = menu.addAction(QIcon::fromTheme("edit-cut"), QObject::tr("Cut"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (cut_tree_item_cb()));
	}

	if (ops.is_member(StandardMenuOperation::Copy)) {
		qa = menu.addAction(QIcon::fromTheme("edit-copy"), QObject::tr("Copy"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (copy_tree_item_cb()));
	}

	if (ops.is_member(StandardMenuOperation::Delete)) {
		qa = menu.addAction(QIcon::fromTheme("edit-delete"), QObject::tr("Delete"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_tree_item_cb()));
	}

	menu.addSeparator();

	return sg_ret::ok;
}




sg_ret Track::menu_add_type_specific_operations(QMenu & menu, bool in_tree_view)
{
	QAction * qa = NULL;

	{
		LayerTRW * parent_trw = (LayerTRW *) this->get_owning_layer();
		Layer * parent_layer = parent_trw->get_owning_layer();

		parent_trw->layer_trw_filter->set_main_fields(parent_trw->get_window(),
							      ThisApp::get_main_gis_view(),
							      parent_layer);
		parent_trw->layer_trw_filter->set_trw_field(parent_trw);
	}


	qa = menu.addAction(QIcon::fromTheme("document-properties"), tr("P&rofile"));
	if (this->profile_dialog) {
		/* A profile dialog window is already opened.
		   Don't give possibility to open a duplicate profile dialog window. */
		qa->setEnabled(false);
	}
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (profile_dialog_cb()));


	qa = menu.addAction(tr("&Statistics"));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (statistics_dialog_cb()));



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
	if (!in_tree_view && 1 == this->selected_children.get_count()) {
		menu.addSeparator();

		qa = menu.addAction(QIcon::fromTheme("document-properties"), tr("&Edit Trackpoint"));
		connect(qa, SIGNAL (triggered(bool)), trw, SLOT (edit_trackpoint_cb()));
	}


	return sg_ret::ok;
}




void Track::goto_startpoint_cb(void)
{
	if (!this->empty()) {
		this->owning_layer->request_new_viewport_center(ThisApp::get_main_gis_view(), this->get_tp_first()->coord);
	}
}




void Track::goto_center_cb(void)
{
	if (this->empty()) {
		return;
	}

	LayerTRW * parent_layer_ = (LayerTRW *) this->owning_layer;

	const Coord coord(this->get_bbox().get_center_lat_lon(), parent_layer_->coord_mode);
	parent_layer_->request_new_viewport_center(ThisApp::get_main_gis_view(), coord);
}




void Track::goto_endpoint_cb(void)
{
	if (this->empty()) {
		return;
	}

	this->owning_layer->request_new_viewport_center(ThisApp::get_main_gis_view(), this->get_tp_last()->coord);
}




void Track::goto_max_speed_cb()
{
	Trackpoint * tp = this->get_tp_by_max_speed();
	if (!tp) {
		return;
	}

	this->owning_layer->request_new_viewport_center(ThisApp::get_main_gis_view(), tp->coord);
}




void Track::goto_max_alt_cb(void)
{
	Trackpoint * tp = this->get_tp_with_highest_altitude();
	if (!tp) {
		return;
	}

	this->owning_layer->request_new_viewport_center(ThisApp::get_main_gis_view(), tp->coord);
}




void Track::goto_min_alt_cb(void)
{
	Trackpoint * tp = this->get_tp_with_lowest_altitude();
	if (!tp) {
		return;
	}

	this->owning_layer->request_new_viewport_center(ThisApp::get_main_gis_view(), tp->coord);
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




bool Track::show_properties_dialog(void)
{
	return this->show_properties_dialog_cb();
}




bool Track::show_properties_dialog_cb(void)
{
	return track_properties_dialog(this, ThisApp::get_main_window());
}




void Track::statistics_dialog_cb(void)
{
	if (this->get_name().isEmpty()) {
		return;
	}

	track_statistics_dialog(this, ThisApp::get_main_window());
}




void Track::profile_dialog_cb(void)
{
	if (this->get_name().isEmpty()) {
		return;
	}

	track_profile_dialog(this, ThisApp::get_main_gis_view(), ThisApp::get_main_window());
}




/**
 * A common function for applying the elevation smoothing and reporting the results.
 */
void Track::smooth_it(bool flat)
{
	unsigned long n_changed = this->smooth_missing_elevation_data(flat);
	/* Inform user how much was changed. */
	const QString msg = QObject::tr("%n points adjusted", "", n_changed); /* TODO_LATER: verify that "%n" format correctly handles unsigned long. */
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

	ThisApp::get_main_gis_view()->set_bbox(this->get_bbox());
	ThisApp::get_main_gis_view()->request_redraw("Re-align viewport to show whole contents of TRW Track");
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
		const Duration duration = this->get_duration(true);
		if (duration.is_valid() && duration.is_positive()) {
			duration_string = QObject::tr("- %1").arg(duration.to_string());
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
	if (!panel->has_any_layer_of_kind(LayerKind::DEM)) {
		Dialog::error(tr("No DEM layers available, thus no DEM values can be applied."), ThisApp::get_main_window());
		return;
	}

	unsigned long n_changed = this->apply_dem_data(skip_existing_elevations);

	/* Inform user how much was changed. */
	const QString msg = QObject::tr("%n points adjusted", "", n_changed); /* TODO_LATER: verify that "%n" format correctly handles unsigned long. */
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
	const QString auto_save_name = append_file_ext(this->get_name(), SGFileType::GPX);

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
			export_status.show_status_dialog(ThisApp::get_main_window());
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
	const Trackpoint * tp = NULL;
	if (1 == this->get_selected_children().get_count()) {
		/* Current trackpoint. */
		const TrackpointReference tp_ref = this->get_selected_children().front();
		if (tp_ref.m_iter_valid) {
			tp = *tp_ref.m_iter;
		}

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

		const LatLon lat_lon = tp->coord.get_lat_lon();
		const QString lat_str = Astro::convert_to_dms(lat_lon.lat);
		const QString lon_str = Astro::convert_to_dms(lat_lon.lon);
		const QString alt_str = QString("%1").arg((int)round(tp->altitude.ll_value()));
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
	if (!this->get_name().isEmpty()) {
		if (new_name == this->get_name()) {
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

#if 0
	/* TODO_MAYBE: to be implemented? */
	parent_layer->set_statusbar_msg_info_trk(this);
#endif
	parent_layer->reset_internal_selections(); /* No other tree item (that is a sublayer of this layer) is selected... */
	parent_layer->selected_track_set(this, this->selected_children.references.front()); /* But this tree item is selected (and maybe its trackpoint too). */

	qDebug() << SG_PREFIX_I << "Tree item" << this->get_name() << "becomes selected tree item";
	g_selected.add_to_set(this);

	return true;
}




/**
 * Only handles a single track
 * It assumes the track belongs to the TRW Layer (it doesn't check this is the case)
 */
void Track::draw_tree_item(GisViewport * gisview, bool highlight_selected, bool parent_is_selected)
{
	/* Check the layer for visibility (including all the parents visibilities). */
	if (!this->tree_view->get_tree_item_visibility_with_parents(this)) {
		return;
	}

	SelectedTreeItems::print_draw_mode(*this, parent_is_selected);

	const bool item_is_selected = parent_is_selected || g_selected.is_in_set(this);
	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;
	parent_layer->painter->draw_track(this, gisview, item_is_selected && highlight_selected);
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
		    || (avg.is_valid() && avg.is_positive())) {

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
	this->m_type_id = this->is_route() ? Track::type_id() : Route::type_id();
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




static Coord * get_next_coord(const Coord & from, const Coord & to, const LatLon & area_span, double gradient)
{
	if ((area_span.lon >= std::abs(to.lat_lon.lon - from.lat_lon.lon))
	    && (area_span.lat >= std::abs(to.lat_lon.lat - from.lat_lon.lat))) {

		return NULL;
	}

	Coord * result = new Coord();
	result->set_coord_mode(CoordMode::LatLon);

	if (std::abs(gradient) < 1) {
		if (from.lat_lon.lon > to.lat_lon.lon) {
			result->lat_lon.lon = from.lat_lon.lon - area_span.lon;
		} else {
			result->lat_lon.lon = from.lat_lon.lon + area_span.lon;
		}
		result->lat_lon.lat = gradient * (result->lat_lon.lon - from.lat_lon.lon) + from.lat_lon.lat;
	} else {
		if (from.lat_lon.lat > to.lat_lon.lat) {
			result->lat_lon.lat = from.lat_lon.lat - area_span.lat;
		} else {
			result->lat_lon.lat = from.lat_lon.lat + area_span.lat;
		}
		result->lat_lon.lon = (1/gradient) * (result->lat_lon.lat - from.lat_lon.lat) + from.lat_lon.lat;
	}

	return result;
}




/*
  "fillins" are probably calculated coordinates between two
  Trackpoints. The coordinates don't really exist in a track, but
  since the Trackpoints are too far apart, we have to somehow fill the
  gaps between them.
*/
static void add_fillins(std::list<Coord *> & list, Coord * from, Coord * to, const LatLon & area_span)
{
	/* TODO_LATER: handle vertical track (to->lat_lon.lon - from->lat_lon.lon == 0). */
	const double gradient = (to->lat_lon.lat - from->lat_lon.lat)/(to->lat_lon.lon - from->lat_lon.lon);

	Coord * next = from;
	while (true) {
		next = get_next_coord(*next, *to, area_span, gradient);
		if (next == nullptr) {
			break;
		}
		list.push_front(next);
	}

	return;
}




static sg_ret get_single_rectangle_span(const VikingScale & viking_scale, LatLon & single_rectangle_span) /* kamilFIXME: viewport is unused, why? */
{
	/* TODO_LATER: calculating based on current size of viewport. */
	const double w_at_zoom_0_125 = 0.0013;
	const double h_at_zoom_0_125 = 0.0011;
	const double zoom_factor = viking_scale.get_x() / 0.125;

	single_rectangle_span.lat = h_at_zoom_0_125 * zoom_factor;
	single_rectangle_span.lon = w_at_zoom_0_125 * zoom_factor;

	return sg_ret::ok;
}




std::list<CoordRectangle> Track::get_coord_rectangles(const VikingScale & viking_scale)
{
	std::list<CoordRectangle> rectangles; /* Rectangles to download. */
	if (this->empty()) {
		return rectangles;
	}

	LatLon single_rectangle_span;
	if (sg_ret::ok != get_single_rectangle_span(viking_scale, single_rectangle_span)) {
		return rectangles;
	}

	rectangles = this->get_coordinate_rectangles(single_rectangle_span);
	std::list<Coord *> fillins;

	/* 'fillin' doesn't work in UTM mode - potentially ending up in massive loop continually allocating memory - hence don't do it. */
	/* Seems that ATM the function get_next_coord() works only for LatLon. */
	if (((LayerTRW *) this->owning_layer)->get_coord_mode() == CoordMode::LatLon) {

		/* Fill-ins for far apart points. */
		std::list<CoordRectangle>::iterator cur_rect;
		std::list<CoordRectangle>::iterator next_rect;

		for (cur_rect = rectangles.begin();
		     (next_rect = std::next(cur_rect)) != rectangles.end();
		     cur_rect++) {

			if ((single_rectangle_span.lon < std::abs((*cur_rect).m_coord_center.lat_lon.lon - (*next_rect).m_coord_center.lat_lon.lon))
			    || (single_rectangle_span.lat < std::abs((*cur_rect).m_coord_center.lat_lon.lat - (*next_rect).m_coord_center.lat_lon.lat))) {

				add_fillins(fillins, &(*cur_rect).m_coord_center, &(*next_rect).m_coord_center, single_rectangle_span);
			}
		}
	} else {
		qDebug() << SG_PREFIX_W << "'download map' feature works only in Mercator mode";
	}

	for (auto iter = fillins.begin(); iter != fillins.end(); iter++) {
		Coord * cur_coord = *iter;
		CoordRectangle rect;
		cur_coord->get_coord_rectangle(single_rectangle_span, rect);
		rectangles.push_front(rect);

		delete *iter;
	}

	return rectangles;
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




void Track::use_with_babel_filter_cb(void)
{
	/* This track can be used with a TRW layer different than an owner of this track. */
	LayerTRWBabelFilter::set_babel_filter_track(this);
}




/*
  Refine the selected route with a routing engine.
  The routing engine is selected by the user, when requestiong the job.
*/
void Track::refine_route_cb(void)
{
	static QString previous_engine_name;

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

	QComboBox * combo = Routing::create_engines_combo(routing_engine_supports_refine, previous_engine_name);

	dialog.grid->addWidget(label, 0, 0);
	dialog.grid->addWidget(combo, 1, 0);


	dialog.button_box->button(QDialogButtonBox::Ok)->setDefault(true);

	if (dialog.exec() == QDialog::Accepted) {
		/* Dialog validated: retrieve selected engine and do the job */
		previous_engine_name = combo->currentText();
		const RoutingEngine * engine = Routing::get_engine_by_name(previous_engine_name);
		if (nullptr == engine) {
			qDebug() << SG_PREFIX_E << "Failed to get routing engine by id" << previous_engine_name;
			return;
		}


		/* Force saving track */
		/* FIXME: remove or rename this hack */
		parent_layer->route_finder_check_added_track = true;


		/* The job */
		main_window->set_busy_cursor();
		engine->refine_route(parent_layer, this);
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
	return this->create_tp_next_to_specified_tp(this->selected_children.references.front(), before);
}




/**
   \brief Create a new trackpoint and insert it next to given @param other_tp.

   Since this method is private, we have pretty good control over
   @other_tp and can be rather certain that it belongs to the
   track.

   Insert it before or after @param other_tp, depending on value of @param before.

   The new trackpoint is created at center position between @param
   other_tp and one of its neighbours: next tp or previous tp.
*/
sg_ret Track::create_tp_next_to_specified_tp(const TrackpointReference & other_tp_ref, bool before)
{
	if (!other_tp_ref.m_iter_valid) {
		return sg_ret::err;
	}

#if 1   /* Debug code. */
	auto iter = std::find(this->trackpoints.begin(), this->trackpoints.end(), *(other_tp_ref.m_iter));
	qDebug() << "Will check assertion for track" << this->get_name();
	assert (iter != this->trackpoints.end());
#endif

	Trackpoint * other_tp = NULL;

	if (before) {
		qDebug() << "------ insert trackpoint before.";
		if (other_tp_ref.m_iter == this->begin()) {
			return sg_ret::err;
		}
		other_tp = *std::prev(other_tp_ref.m_iter);
	} else {
		qDebug() << "------ insert trackpoint after.";
		if (std::next(other_tp_ref.m_iter) == this->end()) {
			return sg_ret::err;
		}
		other_tp = *std::next(other_tp_ref.m_iter);
	}

	/* Use current and other trackpoints to form a new
	   trackpoint which is inserted into the tracklist. */
	if (other_tp) {

		Trackpoint * new_tp = new Trackpoint(**other_tp_ref.m_iter, *other_tp, ((LayerTRW *) this->owning_layer)->coord_mode);
		/* Insert new point into the appropriate trackpoint list,
		   either before or after the current trackpoint as directed. */

		this->insert(*other_tp_ref.m_iter, new_tp, before);
	}

	this->emit_tree_item_changed(before
				     ? "Track changed after adding a trackpoint before specified trackpoint"
				     : "Track changed after adding a trackpoint after specified trackpoint");

	return sg_ret::ok;
}




sg_ret Track::cut_tree_item_cb(void)
{
	return ((LayerTRW *) this->owning_layer)->cut_child_item(this);
}




sg_ret Track::copy_tree_item_cb(void)
{
	return ((LayerTRW *) this->owning_layer)->copy_child_item(this);
}




/**
   @reviewed-on 2019-12-30
*/
sg_ret Track::delete_tree_item_cb(void)
{
	/* false: don't require confirmation in callbacks. */
	return ((LayerTRW *) this->owning_layer)->delete_child_item(this, false);
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
   Method created to avoid constant casting of Track::owning_layer to
   LayerTRW* type.
*/
LayerTRW * Track::get_parent_layer_trw(void) const
{
	return (LayerTRW *) this->owning_layer;

}




QList<QStandardItem *> Track::get_list_representation(const TreeItemViewFormat & view_format)
{
	QList<QStandardItem *> items;
	QStandardItem * item = NULL;
	QVariant variant;


	const DistanceType::Unit distance_unit = Preferences::get_unit_distance();
	const SpeedType::Unit speed_unit = Preferences::get_unit_speed();


	LayerTRW * trw = this->get_parent_layer_trw();

	/* 'visible' doesn't include aggegrate visibility. */
	bool a_visible = trw->is_visible() && this->is_visible();
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
			item = new QStandardItem(trw->get_name());
			item->setToolTip(tooltip);
			item->setEditable(false); /* This dialog is not a good place to edit layer name. */
			items << item;
			break;

		case TreeItemPropertyID::TheItem:
			item = new QStandardItem(this->get_name());
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
				variant = QVariant::fromValue(trk_dist.ll_value());
				item->setData(variant, Qt::DisplayRole);
				item->setEditable(false); /* This dialog is not a good place to edit track length. */
				items << item;
			}
			break;

		case TreeItemPropertyID::DurationProp:
			{
				const Duration trk_duration = this->get_duration();
				item = new QStandardItem();
				item->setToolTip(tooltip);
				variant = QVariant::fromValue(trk_duration.ll_value());
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
				Altitude max_alt(0.0, AltitudeType::Unit::E::Metres);
				TrackData<Distance, Altitude> altitudes;
				altitudes.make_track_data_x_over_y(*this);
				if (altitudes.is_valid()) {
					max_alt = altitudes.y_max();
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




bool TrackSelectedChildren::is_member(const Trackpoint * tp) const
{
	const bool any_tp_selected = 0 != this->get_count();
	if (!any_tp_selected) {
		return false;
	}

	for (auto iter = this->references.begin(); iter != this->references.end(); iter++) {
		const TrackpointReference & tp_ref = *iter;
		if (tp_ref.m_iter_valid && tp == *tp_ref.m_iter) {
			return true;
		}
	}
	return false;
}




TrackpointReference TrackSelectedChildren::front(void) const
{
	/* Since we add one reference to the container in constructor,
	   then here we will always return *some* reference (which may
	   be not valid). */
	return this->references.front();
}




const TrackSelectedChildren & Track::get_selected_children(void) const
{
	return this->selected_children;
}




void Trackpoint::set_timestamp(const Time & value)
{
	this->timestamp = value;
}



void Trackpoint::set_timestamp(time_t value)
{
	this->timestamp = Time(value, TimeType::Unit::internal_unit());
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
	if (sg_ret::ok != this->create_tp_next_to_specified_tp(this->selected_children.references.front(), false)) {
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
	if (sg_ret::ok != this->create_tp_next_to_specified_tp(this->selected_children.references.front(), true)) {
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
	sg_ret ret = this->split_at_trackpoint(this->selected_children.references.front());
	if (sg_ret::ok != ret) {
		qDebug() << SG_PREFIX_W << "Failed to split track" << this->get_name() << "at selected trackpoint";
		return ret;
	}

	this->emit_tree_item_changed("Track changed after splitting at selected trackpoint");

	return sg_ret::ok;
}




sg_ret Track::delete_all_selected_tp(void)
{
	TrackPoints::iterator new_tp_iter = this->delete_trackpoint(this->selected_children.references.front().m_iter);

	if (new_tp_iter != this->end()) {
		/* Set to current to the available adjacent trackpoint. */
		this->selected_tp_set(TrackpointReference(new_tp_iter, true));
		this->recalculate_bbox();
	} else {
		qDebug() << SG_PREFIX_I << "Will reset trackpoint properties dialog data";
		this->selected_tp_reset();
	}

	/* Notice that we may delete all selected trackpoints, and
	   there will be no trackpoins that are selected, but the
	   parent layer still sees this track as selected. */

	return sg_ret::ok;
}




/**
   Delete all selected trackpoint(s)
*/
void Track::delete_all_selected_tp_cb(void)
{
	if (0 == this->selected_children.get_count()) {
		return;
	}

	LayerTRW * parent_layer = this->get_parent_layer_trw();

	this->delete_all_selected_tp();
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
	const QString msg = QObject::tr("Deleted %n points", "", n_removed); /* TODO_LATER: verify that "%n" format correctly handles unsigned long. */
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
	const QString msg = QObject::tr("Deleted %n points", "", n_removed); /* TODO_LATER: verify that "%n" format correctly handles unsigned long. */
	Dialog::info(msg, ThisApp::get_main_window());

	this->emit_tree_item_changed("Deleted trackpoints with the same timestamp");
}




void Track::extend_track_end_cb(void)
{
	Window * window = ThisApp::get_main_window();
	LayerTRW * parent_layer = this->get_parent_layer_trw();

	window->activate_tool_by_id(this->is_route() ? LayerToolTRWNewRoute::tool_id() : LayerToolTRWNewTrack::tool_id());

	if (!this->empty()) {
		parent_layer->request_new_viewport_center(ThisApp::get_main_gis_view(), this->get_tp_last()->coord);
	}
}




/**
   Extend a track using route finder
*/
void Track::extend_track_end_route_finder_cb(void)
{
	Window * window = ThisApp::get_main_window();
	LayerTRW * parent_layer = this->get_parent_layer_trw();

	window->activate_tool_by_id(LayerToolTRWExtendedRouteFinder::tool_id());

	parent_layer->route_finder_started = true;

	if (!this->empty()) {
		parent_layer->request_new_viewport_center(ThisApp::get_main_gis_view(), this->get_tp_last()->coord);
	}
}




bool Track::is_route(void) const
{
	return this->m_type_id == Route::type_id();
}




bool Track::is_track(void) const
{
	return this->m_type_id == Track::type_id();
}




sg_ret Track::move_selection_to_next_tp(void)
{
	if (1 != this->selected_children.get_count()) {
		return sg_ret::err_cond;
	}
	if (std::next(this->selected_children.references.front().m_iter) == this->end()) {
		/* Can't go forward if we are already at the end. */
		return sg_ret::err_cond;
	}

	this->selected_children.references.front().m_iter++;

	return sg_ret::ok;
}




sg_ret Track::move_selection_to_previous_tp(void)
{
	if (1 != this->selected_children.get_count()) {
		return sg_ret::err_cond;
	}
	if (this->selected_children.references.front().m_iter == this->begin()) {
		/* Can't go back if we are already at the beginning. */
		return sg_ret::err_cond;
	}

	this->selected_children.references.front().m_iter--;

	return sg_ret::ok;
}




size_t TrackSelectedChildren::get_count(void) const
{
	/* For now we can select at most one trackpoint. */
	return this->references.front().m_iter_valid ? 1 : 0;
}




void Track::selected_tp_set(const TrackpointReference & tp_ref)
{
	if (tp_ref.m_iter_valid) {
		qDebug() << SG_PREFIX_I << "zzzzz - setting valid tp reference";
	} else {
		qDebug() << SG_PREFIX_E << "zzzzz - setting invalid tp reference";
	}
	this->selected_children.references.front() = tp_ref;

	LayerTRW * trw = this->get_parent_layer_trw();
	this->tp_properties_dialog_set();

	trw->set_statusbar_msg_info_tp(this->selected_children.references.front(), this);
}




bool Track::selected_tp_reset(void)
{
	const bool was_set = this->selected_children.references.front().m_iter_valid;

	if (was_set) {
		qDebug() << SG_PREFIX_E << "zzzzz - reset";
		this->selected_children.references.front().m_iter_valid = false;
	}
	/* Do this regardless of value of 'was_set' - just in case. */
	Track::tp_properties_dialog_reset();

	return was_set;
}




bool Track::is_selected(void) const
{
	LayerTRW * trw = this->get_parent_layer_trw();
	return trw->selected_track_get() == this;
}




/**
   @reviewed-on 2019-12-02
*/
void Track::list_dialog(QString const & title, Layer * parent_layer, const std::list<SGObjectTypeID> & wanted_types)
{
	assert (parent_layer->m_kind == LayerKind::Aggregate || parent_layer->m_kind == LayerKind::TRW);

	TreeItemListDialogWrapper<Track> list_dialog(parent_layer);
	if (!list_dialog.find_tree_items(wanted_types)) {
		Dialog::info(QObject::tr("No Tracks found"), parent_layer->get_window());
		return;
	}
	list_dialog.show_tree_items(title);
}




void Track::prepare_for_profile(void)
{
	this->track_length_including_gaps = this->get_length_value_including_gaps();
}




TrackSelectedChildren::TrackSelectedChildren()
{
	this->references.push_back(TrackpointReference());
}




sg_ret Track::selected_tp_set_coord(const Coord & new_coord, bool do_recalculate_bbox)
{
	const size_t tp_count = this->selected_children.get_count();
	if (1 != tp_count) {
		qDebug() << SG_PREFIX_E << "Will reset trackpoint properties dialog data, wrong selected tp count:" << tp_count;
		Track::tp_properties_dialog_reset();
		return sg_ret::err;
	}
	if (false == this->selected_children.references.front().m_iter_valid) {
		/* That's a double error: function was called for
		   invalid tp, and the tp is invalid while
		   selected_children.get_count() returned 1. */
		qDebug() << SG_PREFIX_E << "Can't set coordinates of tp: iter invalid:" << tp_count;
		qDebug() << SG_PREFIX_E << "Count of selected TPs is" << tp_count << "but iter of selected TP is invalid";
		return sg_ret::err;
	}


	(*this->selected_children.references.front().m_iter)->coord = new_coord;


	/* Update properties dialog with the most recent coordinates
	   of tp. */
	/* TODO_MAYBE: optimize by changing only coordinates in
	   the dialog. This is the only parameter that will change
	   when a point is moved in x/y plane.  We may consider also
	   updating an alternative altitude indicator, if the altitude
	   is retrieved from DEM info. */
	this->tp_properties_dialog_set();


	if (do_recalculate_bbox) {
		LayerTRW * trw = this->get_parent_layer_trw();
		if (this->is_route()) {
			trw->routes.recalculate_bbox();
		} else {
			trw->tracks.recalculate_bbox();
		}
	}

	this->emit_tree_item_changed("Selected trackpoint's position has changed");

	return sg_ret::ok;
}




sg_ret Track::tp_properties_dialog_set(void)
{
	Window * window = ThisApp::get_main_window();
	LayerToolTRWEditTrackpoint * tool = (LayerToolTRWEditTrackpoint *) window->get_toolbox()->get_tool(LayerToolTRWEditTrackpoint::tool_id());
	if (!tool->is_activated()) {
		/* Someone is asking to fill dialog data with
		   trackpoint when TP edit tool is not active. This is
		   ok, maybe generic select tool is active and has
		   been used to select a trackpoint? */
		LayerToolSelect * select_tool = (LayerToolSelect *) window->get_toolbox()->get_tool(LayerToolSelect::tool_id());
		if (!select_tool->is_activated()) {
			qDebug() << SG_PREFIX_E << "Trying to fill 'tp properties' dialog when neither 'tp edit' tool nor 'generic select' tool are active";
			return sg_ret::err;
		}
	}

	tool->point_properties_dialog->dialog_data_set(this);
	return sg_ret::ok;
}




sg_ret Track::tp_properties_dialog_reset(void)
{
	Window * window = ThisApp::get_main_window();
	LayerToolTRWEditTrackpoint * tool = (LayerToolTRWEditTrackpoint *) window->get_toolbox()->get_tool(LayerToolTRWEditTrackpoint::tool_id());
	if (!tool->is_activated()) {
		return sg_ret::ok;
	}
	qDebug() << SG_PREFIX_I << "Will reset trackpoint dialog data";
	tool->point_properties_dialog->dialog_data_reset();
	return sg_ret::ok;
}




TreeItemViewFormat Track::get_view_format_header(bool include_parent_layer)
{
	TreeItemViewFormat view_format;

	const AltitudeType::Unit height_unit = Preferences::get_unit_height();
	const SpeedType::Unit speed_unit = Preferences::get_unit_speed();
	const DistanceType::Unit distance_unit = Preferences::get_unit_distance();

	if (include_parent_layer) {
		view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::ParentLayer, true, QObject::tr("Parent Layer"))); // this->view->horizontalHeader()->setSectionResizeMode(LAYER_NAME_COLUMN, QHeaderView::Interactive);
	}
	view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::TheItem,       true, QObject::tr("Name"))); // this->view->horizontalHeader()->setSectionResizeMode(TRACK_COLUMN, QHeaderView::Interactive);
	view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::Timestamp,     true, QObject::tr("Timestamp"))); // this->view->horizontalHeader()->setSectionResizeMode(DATE_COLUMN, QHeaderView::ResizeToContents);
	view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::Visibility,    true, QObject::tr("Visibility"))); // this->view->horizontalHeader()->setSectionResizeMode(VISIBLE_COLUMN, QHeaderView::ResizeToContents);
	view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::Comment,       true, QObject::tr("Comment"))); // this->view->horizontalHeader()->setSectionResizeMode(COMMENT_COLUMN, QHeaderView::Interactive);
	view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::Length,        true, QObject::tr("Length\n(%1)").arg(Distance::unit_full_string(distance_unit)))); // this->view->horizontalHeader()->setSectionResizeMode(LENGTH_COLUMN, QHeaderView::ResizeToContents);
	view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::DurationProp,  true, QObject::tr("Duration\n(minutes)"))); // this->view->horizontalHeader()->setSectionResizeMode(DURATION_COLUMN, QHeaderView::ResizeToContents);
	view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::AverageSpeed,  true, QObject::tr("Average Speed\n(%1)").arg(Speed::unit_string(speed_unit)))); // this->view->horizontalHeader()->setSectionResizeMode(AVERAGE_SPEED_COLUMN, QHeaderView::ResizeToContents);
	view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::MaximumSpeed,  true, QObject::tr("Maximum Speed\n(%1)").arg(Speed::unit_string(speed_unit)))); // this->view->horizontalHeader()->setSectionResizeMode(MAXIMUM_SPEED_COLUMN, QHeaderView::ResizeToContents);
	view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::MaximumHeight, true, QObject::tr("Maximum Height\n(%1)").arg(Altitude::unit_full_string(height_unit)))); // this->view->horizontalHeader()->setSectionResizeMode(WaypointListModel::Comment, QHeaderView::Stretch); // this->view->horizontalHeader()->setSectionResizeMode(MAXIMUM_HEIGHT_COLUMN, QHeaderView::ResizeToContents);

	return view_format;
}




/**
   @reviewed-on 2019-12-04
*/
Speed Track::get_gps_speed(const Trackpoint * tp)
{
	Speed result;
	if (nullptr == tp) {
		return result;
	} else {
		result = Speed(tp->gps_speed, SpeedType::Unit::E::MetresPerSecond); /* We are dealing with GPS trackpoints, so natural unit is m/s. */
		return result;
	}
}




/**
   @reviewed-on 2019-12-04
*/
Speed Track::get_diff_speed(const Trackpoint * tp, const Trackpoint * tp_prev)
{
	Speed result;
	if (nullptr == tp || nullptr == tp_prev) {
		return result;
	}
	if (!tp->timestamp.is_valid() || !tp_prev->timestamp.is_valid()) {
		/* Can't calculate speed without valid time delta. */
		return result;
	}
	if (tp->timestamp == tp_prev->timestamp) {
		/* This actually is not an error. */
		result = Speed(0, SpeedType::Unit::E::MetresPerSecond); /* Since we deal with GPS trackpoints, let's use m/s. */
		return result;
	}

	const Distance distance_diff = Coord::distance_2(tp_prev->coord, tp->coord);
	const Duration duration = Duration::get_abs_duration(tp_prev->timestamp, tp->timestamp);
	result.make_speed(distance_diff, duration);

	return result;
}




/**
   @reviewed-on 2019-12-05
*/
Duration Track::get_diff_time(const Trackpoint * tp, const Trackpoint * tp_prev)
{
	Duration result;
	if (nullptr == tp || nullptr == tp_prev) {
		return result;
	}
	if (!tp->timestamp.is_valid() || !tp_prev->timestamp.is_valid()) {
		/* Can't calculate duration without valid time delta. */
		return result;
	}
	if (tp->timestamp == tp_prev->timestamp) {
		result = Duration(0, DurationType::Unit::E::Seconds); /* Since we deal with GPS trackpoints, let's use seconds. */
		return result;
	}
	result = Duration::get_abs_duration(tp_prev->timestamp, tp->timestamp);
	return result;
}




sg_ret Track::get_item_position(const TrackpointReference & tp_ref, bool & is_first, bool & is_last) const
{
	if (false == tp_ref.m_iter_valid) {
		is_first = false;
		is_last = false;
		return sg_ret::ok;
	}

	auto iter = std::find(this->trackpoints.begin(), this->trackpoints.end(), *tp_ref.m_iter);
	if (iter != tp_ref.m_iter) {
		qDebug() << SG_PREFIX_E << "Invalid trackpoint reference";
		is_first = false;
		is_last = false;
		return sg_ret::err;
	}

	is_first = iter == this->trackpoints.begin();
	is_last = std::next(iter) == this->trackpoints.end();
	return sg_ret::ok;
}
