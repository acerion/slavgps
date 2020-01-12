/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (c) 2012, Rob Norris <rw_norris@hotmail.com>
 * Copyright (c) 2016-2020, Kamil Ignacak <acerion@wp.pl>
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




#include <cassert>




#include "globals.h"
#include "layer_trw_track_internal.h"
#include "layer_trw_track_data.h"

#include "measurements.h"




#define SG_MODULE "Layer TRW Track Data"




using namespace SlavGPS;




/*
  It is not that obvious how x_min/x_max should be initialized before
  we start calculating these values for whole graph.

  Should x_min for time-based graph be initialized with:
  - zero? that won't work for tracks that start at non-zero timestamp;
  - timestamp of first trackpoint? that won't work for tracks where first timestamp has invalid timestamp or invalid 'y' value.
  Similar considerations should be done for y_min/y_max.

  Therefore initialization of these four fields is left to code that
  calculates min/max values. The 'extremes_initialized' boolean flag
  is set the moment the code find some sane and valid initial values.
*/
#define TRW_TRACK_DATA_UPDATE_MIN_MAX(_track_data_, _i_, _point_valid_) \
	if ((_point_valid_)) {						\
		if (!extremes_initialized) {				\
			x_min_ll = _track_data_->m_x_ll[_i_];		\
			x_max_ll = _track_data_->m_x_ll[_i_];		\
			y_min_ll = _track_data_->m_y_ll[_i_];		\
			y_max_ll = _track_data_->m_y_ll[_i_];		\
			extremes_initialized = true;			\
		}							\
									\
		if (_track_data_->m_x_ll[_i_] < x_min_ll) {		\
			x_min_ll = _track_data_->m_x_ll[_i_];		\
		} else if (_track_data_->m_x_ll[_i_] > x_max_ll) {	\
			x_max_ll = _track_data_->m_x_ll[_i_];		\
		}							\
									\
		if (_track_data_->m_y_ll[_i_] < y_min_ll) {		\
			y_min_ll = _track_data_->m_y_ll[_i_];		\
		} else if (_track_data_->m_y_ll[_i_] > y_max_ll) {	\
			y_max_ll = _track_data_->m_y_ll[_i_];		\
		}							\
	}								\




/**
   @reviewed-on tbd
*/
template <typename Tx, typename Ty>
sg_ret TrackData<Tx, Ty>::compress_into(TrackData & target, int compressed_n_points) const
{
	target.invalidate();
	if (sg_ret::ok != target.allocate(compressed_n_points)) {
		qDebug() << SG_PREFIX_E << "Failed to allocate vector for compressed track data";
		return sg_ret::err;
	}


	const double tps_per_data_point = 1.0 * this->n_points / target.n_points;
	const int floor_ = floor(tps_per_data_point);
	const int ceil_ = ceil(tps_per_data_point);
	int n_tps_compressed = 0;

	//qDebug() << "-----------------------" << floor_ << tps_per_data_point << ceil_ << this->n_points << target.n_points;

	/* In the following computation, we iterate through periods of time of duration delta_t.
	   The first period begins at the beginning of the track.  The last period ends at the end of the track. */
	int tp_index = 0; /* index of the current trackpoint. */
	int i = 0;
	for (i = 0; i < target.n_points; i++) {

		int sampling_size;
		if ((i + 1) * tps_per_data_point > n_tps_compressed + floor_) {
		        sampling_size = ceil_;
		} else {
			sampling_size = floor_;
		}

		/* This may happen at the very end of loop, when
		   attempting to calculate last output data point. */
		if (n_tps_compressed + sampling_size > this->n_points) {
			const int fix = (n_tps_compressed + sampling_size) - this->n_points;
			qDebug() << "oooooooooooo truncating from" << n_tps_compressed + sampling_size
				 << "to" << n_tps_compressed + sampling_size - fix
				 << "(sampling_size = " << sampling_size << " -> " << sampling_size - fix << ")";
			sampling_size -= fix;
		}

		double acc_x = 0.0;
		double acc_y = 0.0;
		for (int j = n_tps_compressed; j < n_tps_compressed + sampling_size; j++) {
			acc_x += this->x[j];
			acc_y += this->m_y_ll[j];
			tp_index++;
		}

		//qDebug() << "------- i =" << i << "/" << target.n_points << "sampling_size =" << sampling_size << "n_tps_compressed =" << n_tps_compressed << "n_tps_compressed + sampling_size =" << n_tps_compressed + sampling_size << acc_y << acc_y / sampling_size;

		target.m_x_ll[i] = acc_x / sampling_size;
		target.m_y_ll[i] = acc_y / sampling_size;
		target->m_tps[i] = this->tps[n_tps_compressed];

		n_tps_compressed += sampling_size;
	}

	assert(i == target.n_points);

	target.valid = true;
	target.calculate_min_max(); /* TODO_LATER: rethink how we calculate min/max of compressed. */
	snprintf(target.debug, sizeof (target.debug), "Compressed %s", this->debug);

	return sg_ret::ok;
}




/**
   @reviewed-on tbd
*/
TrackDataBase::TrackDataBase()
{

}




/**
   @reviewed-on tbd
*/
TrackDataBase::~TrackDataBase()
{

}




namespace SlavGPS { /* Template specializations need to be put inside a namespace. */




/**
   Simple method for copying "distance over time" information from Track to TrackData.
   Make a distance/time map, heavily based on the Track::make_track_data_speed_over_time() method.

   @reviewed-on tbd
*/
template <> /* Template specialisation for specific type. */
sg_ret TrackData<Time, Distance>::make_track_data_x_over_y(Track * trk)
{
	/* No special handling of segments ATM... */

	bool extremes_initialized = false;
	TimeType::LL x_min_ll = 0;
	TimeType::LL x_max_ll = 0;
	DistanceType::LL y_min_ll = 0;
	DistanceType::LL y_max_ll = 0;

	const Duration duration = trk->get_duration();
	if (!duration.is_valid() || duration.is_negative()) {
		qDebug() << SG_PREFIX_W << "Trying to calculate track data from track with incorrect duration" << duration;
		return sg_ret::err;
	}


	const int tp_count = trk->get_tp_count();
	if (tp_count < 1) {
		qDebug() << SG_PREFIX_W << "Trying to calculate track data from empty track";
		return sg_ret::err;
	}
	this->allocate(tp_count);


	int i = 0;
	auto iter = trk->trackpoints.begin();
	this->m_x_ll[i] = (*iter)->timestamp.ll_value();
	this->m_y_ll[i] = 0;
	this->m_tps[i] = (*iter);
	TRW_TRACK_DATA_UPDATE_MIN_MAX(this, i, (!std::isnan(this->m_y_ll[i])));
	i++;
	iter++;

	while (iter != trk->trackpoints.end()) {

		this->m_x_ll[i] = (*iter)->timestamp.ll_value();
		if (i > 0 && this->m_x_ll[i] <= this->m_x_ll[i - 1]) {
			/* TODO_LATER: this doesn't solve problem in any way if glitch is at the beginning of dataset. */
			qDebug() << SG_PREFIX_W << "Glitch in timestamps" << i << this->m_x_ll[i] << this->m_x_ll[i - 1];
			this->m_x_ll[i] = this->m_x_ll[i - 1];
		}

		this->m_y_ll[i] = this->m_y_ll[i - 1] + Coord::distance((*std::prev(iter))->coord, (*iter)->coord);

		this->m_tps[i] = (*iter);

		TRW_TRACK_DATA_UPDATE_MIN_MAX(this, i, (!std::isnan(this->m_y_ll[i])));

		i++;
		iter++;

#if 0
		if (data_dt.m_x_ll[i] <= data_dt.m_x_ll[i - 1]) {
			/* Handle glitch in values of consecutive time stamps.
			   TODO_LATER: improve code that calculates pseudo-values of result when a glitch has been found. */
			qDebug() << SG_PREFIX_W << "Glitch in timestamps" << i << data_dt.m_x_ll[i] << data_dt.m_x_ll[i - 1];
			this->m_x_ll[i] = data_dt.m_x_ll[i - 1];
			this->m_y_ll[i] = NAN;
			TRW_TRACK_DATA_UPDATE_MIN_MAX(this, i, false);
		} else {
			this->m_x_ll[i] = data_dt.m_x_ll[i];
			this->m_y_ll[i] = data_dt.m_y_ll[i - 1];
			TRW_TRACK_DATA_UPDATE_MIN_MAX(this, i, true);
		}
#endif
	}

	assert (i == tp_count);
	qDebug() << SG_PREFIX_D << "Collected" << i << "track data values";

#if 0 /* Debug. */
	for (int j = 0; j < tp_count; j++) {
		qDebug() << SG_PREFIX_I << "Distance over time: t[" << j << "] = " << this->m_x_ll[j] << ", d[" << j << "] =" << this->m_y_ll[j];
	}
#endif

	this->m_valid = true;
	this->x_domain = GisViewportDomain::TimeDomain;
	this->y_domain = GisViewportDomain::DistanceDomain;

	this->x_unit = TimeType::Unit::internal_unit();
	this->y_unit = DistanceType::Unit::internal_unit();

	snprintf(this->m_debug, sizeof (this->m_debug), "%s", "Distance over Time");

	this->m_x_min = Time(x_min_ll, this->x_unit);
	this->m_x_max = Time(x_max_ll, this->x_unit);
	this->m_y_min = Distance(y_min_ll, this->y_unit);
	this->m_y_max = Distance(y_max_ll, this->y_unit);

	qDebug() << SG_PREFIX_I << "TrackData ready:" << *this;

	return sg_ret::ok;
}




template <> /* Template specialisation for specific type. */
sg_ret TrackData<Distance, Altitude>::make_track_data_x_over_y(Track * trk)
{
	TrackData result;

	bool extremes_initialized = false;
	DistanceType::LL x_min_ll = 0;
	DistanceType::LL x_max_ll = 0;
	AltitudeType::LL y_min_ll = 0;
	AltitudeType::LL y_max_ll = 0;

	const double total_length = trk->get_length_value_including_gaps();
	if (total_length <= 0) {
		return sg_ret::err;
	}

	const int tp_count = trk->get_tp_count();
	this->allocate(tp_count);


	int i = 0;
	bool y_valid = false;

	auto iter = trk->trackpoints.begin();
	/* Initial step. */
	{
		this->m_x_ll[i] = 0; /* Distance at the beginning is zero. */

		y_valid = (*iter)->altitude.is_valid();
		this->m_y_ll[i] = y_valid ? (*iter)->altitude.ll_value() : NAN;
		this->m_y_ll[i] = (*iter)->altitude.ll_value();

		this->m_tps[i] = (*iter);
		TRW_TRACK_DATA_UPDATE_MIN_MAX(this, i, (!std::isnan(this->m_y_ll[i])));
		i++;
		iter++;
	}


	while (iter != trk->trackpoints.end()) {

		/* Iterative step. */
		{
			/* Accumulate distance. */
			this->m_x_ll[i] = this->m_x_ll[i - 1] + Coord::distance((*std::prev(iter))->coord, (*iter)->coord);

			y_valid = (*iter)->altitude.is_valid();
			this->m_y_ll[i] = y_valid ? (*iter)->altitude.ll_value() : NAN;
			this->m_y_ll[i] = (*iter)->altitude.ll_value();

			this->m_tps[i] = (*iter);
			TRW_TRACK_DATA_UPDATE_MIN_MAX(this, i, (!std::isnan(this->m_y_ll[i])));
			i++;
			iter++;
		}
	}

	assert(i == tp_count);

	this->m_valid = true;
	this->x_domain = GisViewportDomain::DistanceDomain;
	this->y_domain = GisViewportDomain::ElevationDomain;

	this->x_unit = DistanceType::Unit::internal_unit();
	this->y_unit = AltitudeType::Unit::internal_unit();

	snprintf(this->m_debug, sizeof (this->m_debug), "%s", "Altitude over Distance");

	this->m_x_min = Distance(x_min_ll, this->x_unit);
	this->m_x_max = Distance(x_max_ll, this->x_unit);
	this->m_y_min = Altitude(y_min_ll, this->y_unit);
	this->m_y_max = Altitude(y_max_ll, this->y_unit);

	qDebug() << SG_PREFIX_I << "TrackData ready:" << *this;

	return sg_ret::ok;
}



/**
   I understood this when I wrote it ... maybe ... Basically it eats up the
   proper amounts of length on the track and averages elevation over that.

   @reviewed-on tbd
*/
template <> /* Template specialisation for specific type. */
sg_ret TrackData<Distance, Altitude>::make_track_data_x_over_y(Track * trk, int compressed_n_points)
{
	TrackData result;

	bool extremes_initialized = false;
	DistanceType::LL x_min_ll = 0;
	DistanceType::LL x_max_ll = 0;
	AltitudeType::LL y_min_ll = 0;
	AltitudeType::LL y_max_ll = 0;

	/* TODO_LATER: this function does not set this->m_tps[]. */


	const int tp_count = trk->get_tp_count();
	if (tp_count < 1) {
		qDebug() << SG_PREFIX_W << "Trying to calculate track data from track with size" << tp_count;
		return sg_ret::err;
	}


	{ /* Test if there's anything worth calculating. */

		bool correct = true;
		for (auto iter = trk->trackpoints.begin(); iter != trk->trackpoints.end(); iter++) {
			/* Sometimes a GPS device (or indeed any random file) can have stupid numbers for elevations.
			   Since when is 9.9999e+24 a valid elevation!!
			   This can happen when a track (with no elevations) is uploaded to a GPS device and then redownloaded (e.g. using a Garmin Legend EtrexHCx).
			   Some protection against trying to work with crazily massive numbers (otherwise get SIGFPE, Arithmetic exception) */

			if ((*iter)->altitude.ll_value() > SG_ALTITUDE_RANGE_MAX) {
				/* TODO_LATER: clamp the invalid values, but still generate vector? */
				qDebug() << SG_PREFIX_W << "Track altitude" << (*iter)->altitude << "out of range; not generating vector";
				correct = false;
				break;
			}
		}
		if (!correct) {
			return sg_ret::err;
		}
	}

	const double total_length = trk->get_length_value_including_gaps();
	const double delta_d = total_length / (compressed_n_points - 1);

	/* Zero delta_d (eg, track of 2 tp with the same loc) will cause crash */
	if (delta_d <= 0) {
		return sg_ret::err;
	}

	this->allocate(compressed_n_points);

	double current_dist = 0.0;
	double current_area_under_curve = 0;


	auto iter = trk->trackpoints.begin();
	double current_seg_length = Coord::distance((*iter)->coord, (*std::next(iter))->coord);

	double altitude1 = (*iter)->altitude.ll_value();
	double altitude2 = (*std::next(iter))->altitude.ll_value();
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
				this->m_y_ll[current_chunk] = altitude1;
				if (current_chunk > 0) {
					/* TODO_LATER: verify this. */
					this->m_x_ll[current_chunk] = this->m_x_ll[current_chunk - 1] + delta_d;
				}
			} else {
				this->m_y_ll[current_chunk] = altitude1 + (altitude2 - altitude1) * ((dist_along_seg - (delta_d / 2)) / current_seg_length);
				if (current_chunk > 0) {
					/* TODO_LATER: verify this. */
					this->m_x_ll[current_chunk] = this->m_x_ll[current_chunk - 1] + delta_d;
				}
			}
			TRW_TRACK_DATA_UPDATE_MIN_MAX(this, current_chunk, true);
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
			while (iter != trk->trackpoints.end()
			       && std::next(iter) != trk->trackpoints.end()) {

				current_seg_length = Coord::distance((*iter)->coord, (*std::next(iter))->coord);
				altitude1 = (*iter)->altitude.ll_value();
				altitude2 = (*std::next(iter))->altitude.ll_value();
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
			    || (iter != trk->trackpoints.end()
				&& std::next(iter) == trk->trackpoints.end())) {

				this->m_y_ll[current_chunk] = current_area_under_curve / current_dist;
				if (current_chunk > 0) {
					/* TODO_LATER: verify this. */
					this->m_x_ll[current_chunk] = this->m_x_ll[current_chunk - 1] + delta_d;
				}
				if (std::next(iter) == trk->trackpoints.end()) {
					for (int i = current_chunk + 1; i < compressed_n_points; i++) {
						this->m_y_ll[i] = this->m_y_ll[current_chunk];
						if (current_chunk > 0) {
							/* TODO_LATER: verify this. */
							this->m_x_ll[i] = this->m_x_ll[current_chunk - 1] + delta_d;
						}
					}
					break;
				}
			} else {
				current_area_under_curve += dist_along_seg * (altitude1 + (altitude2 - altitude1) * dist_along_seg / current_seg_length);
				this->m_y_ll[current_chunk] = current_area_under_curve / delta_d;
				if (current_chunk > 0) {
					/* TODO_LATER: verify this. */
					this->m_x_ll[current_chunk] = this->m_x_ll[current_chunk - 1] + delta_d;
				}
			}

			TRW_TRACK_DATA_UPDATE_MIN_MAX(this, current_chunk, true);

			current_dist = 0;
			current_chunk++;
		}
	}

#ifdef K_FIXME_RESTORE
	assert(current_chunk == compressed_n_points);
#endif

	this->m_valid = true;
	this->x_domain = GisViewportDomain::DistanceDomain;
	this->y_domain = GisViewportDomain::ElevationDomain;

	this->x_unit = DistanceType::Unit::internal_unit();
	this->y_unit = AltitudeType::Unit::internal_unit();
	snprintf(this->m_debug, sizeof (this->m_debug), "%s", "Altitude over Distance");

	this->m_x_min = Distance(x_min_ll, this->x_unit);
	this->m_x_max = Distance(x_max_ll, this->x_unit);
	this->m_y_min = Altitude(y_min_ll, this->y_unit);
	this->m_y_max = Altitude(y_max_ll, this->y_unit);

	qDebug() << SG_PREFIX_I << "TrackData ready:" << *this;

	return sg_ret::ok;
}




/**
   @reviewed-on tbd
*/
template <> /* Template specialisation for specific type. */
sg_ret TrackData<Distance, Gradient>::make_track_data_x_over_y(Track * trk)
{
	TrackData result;

	bool extremes_initialized = false;
	DistanceType::LL x_min_ll = 0;
	DistanceType::LL x_max_ll = 0;
	GradientType::LL y_min_ll = 0;
	GradientType::LL y_max_ll = 0;

	const int tp_count = trk->get_tp_count();
	if (tp_count < 2) {
		qDebug() << SG_PREFIX_W << "Trying to calculate track data from track with size" << tp_count;
		return sg_ret::err;
	}


	TrackData<Distance, Altitude> data_ad;
	data_ad.make_track_data_x_over_y(trk);
	if (!data_ad.is_valid()) {
		qDebug() << SG_PREFIX_I << "Failed to collect 'altitude over distance' track data";
		return sg_ret::err;
	}

	this->allocate(tp_count);

	int i = 0;
	GradientType::LL gradient;
	for (i = 0; i < (tp_count - 1); i++) {

		this->m_x_ll[i] = data_ad.x_ll(i);

		const DistanceType::LL delta_distance = data_ad.x_ll(i + 1) - data_ad.x_ll(i);

		if (delta_distance <= 0.0) { /* e.g. two trackpoints in the same location. */
			this->m_y_ll[i] = 0;
		} else {
			const AltitudeType::LL delta_altitude = data_ad.y_ll(i + 1) - data_ad.y_ll(i);
			gradient = 100.0 * delta_altitude / delta_distance;
			this->m_y_ll[i] = gradient;
		}
		this->m_tps[i] = data_ad.tp(i);

		TRW_TRACK_DATA_UPDATE_MIN_MAX(this, i, true);

	}
	this->m_x_ll[i] = data_ad.x_ll(i);
	this->m_y_ll[i] = gradient;
	this->m_tps[i] = data_ad.tp(i);
	TRW_TRACK_DATA_UPDATE_MIN_MAX(this, i, true);

	assert (i + 1 == tp_count);

	this->m_valid = true;
	this->x_domain = GisViewportDomain::DistanceDomain;
	this->y_domain = GisViewportDomain::GradientDomain;

	this->x_unit = DistanceType::Unit::internal_unit();
	this->y_unit = GradientType::Unit::internal_unit();

	snprintf(this->m_debug, sizeof (this->m_debug), "%s", "Gradient over Distance");

	this->m_x_min = Distance(x_min_ll, this->x_unit);
	this->m_x_max = Distance(x_max_ll, this->x_unit);
	this->m_y_min = Gradient(y_min_ll, this->y_unit);
	this->m_y_max = Gradient(y_max_ll, this->y_unit);

	qDebug() << SG_PREFIX_I << "TrackData ready:" << *this;

	return sg_ret::ok;
}




/**
   By Alex Foobarian.

   @reviewed-on tbd
*/
template <> /* Template specialisation for specific type. */
sg_ret TrackData<Time, Speed>::make_track_data_x_over_y(Track * trk)
{
	TrackData result;

	bool extremes_initialized = false;
	TimeType::LL x_min_ll = 0;
	TimeType::LL x_max_ll = 0;
	SpeedType::LL y_min_ll = 0;
	SpeedType::LL y_max_ll = 0;

	const Duration duration = trk->get_duration();
	if (!duration.is_valid() || duration.is_negative()) {
		qDebug() << SG_PREFIX_W << "Trying to calculate track data from track with incorrect duration" << duration;
		return sg_ret::err;
	}


	const int tp_count = trk->get_tp_count();
	if (tp_count < 1) {
		qDebug() << SG_PREFIX_W << "Trying to calculate track data from empty track";
		return sg_ret::err;
	}
	TrackData<Time, Distance> data_dt;
	data_dt.make_track_data_x_over_y(trk);


	if (data_dt.size() != tp_count) {
		qDebug() << SG_PREFIX_E << "Mismatch of data: data points in 'distance over time' =" << data_dt.size() << ", trackpoints count =" << tp_count;
	}
	assert (data_dt.size() == tp_count);


	this->allocate(tp_count);


	int i = 0;
	this->m_x_ll[i] = data_dt.x_ll(i);
	this->m_y_ll[i] = 0.0;
	this->m_tps[i] = data_dt.tp(i);
	TRW_TRACK_DATA_UPDATE_MIN_MAX(this, i, (!std::isnan(this->m_y_ll[i])));
	i++;

	for (; i < tp_count; i++) {
		/* TODO_LATER: handle invalid distance values in data_dt. */

		if (data_dt.x_ll(i) <= data_dt.x_ll(i - 1)) {
			/* Handle glitch in values of consecutive time stamps.
			   TODO_LATER: improve code that calculates pseudo-values of result when a glitch has been found. */
			qDebug() << SG_PREFIX_W << "Glitch in timestamps:" << i << data_dt.x_ll(i) << data_dt.x_ll(i - 1);
			this->m_x_ll[i] = data_dt.x_ll(i - 1);
			this->m_y_ll[i] = 0;

			TRW_TRACK_DATA_UPDATE_MIN_MAX(this, i, false);

		} else {
			const double delta_t = (data_dt.x_ll(i) - data_dt.x_ll(i - 1));
			const double delta_d = (data_dt.y_ll(i) - data_dt.y_ll(i - 1));

			this->m_x_ll[i] = data_dt.x_ll(i);
			this->m_y_ll[i] = delta_d / delta_t;

			TRW_TRACK_DATA_UPDATE_MIN_MAX(this, i, true);
		}
		this->m_tps[i] = data_dt.tp(i);
	}

	this->m_valid = true;
	this->x_domain = GisViewportDomain::TimeDomain;
	this->y_domain = GisViewportDomain::SpeedDomain;

	this->x_unit = TimeType::Unit::internal_unit();
	this->y_unit = SpeedType::Unit::internal_unit();

	snprintf(this->m_debug, sizeof (this->m_debug), "%s", "Speed over Time");

	this->m_x_min = Time(x_min_ll, this->x_unit);
	this->m_x_max = Time(x_max_ll, this->x_unit);
	this->m_y_min = Speed(y_min_ll, this->y_unit);
	this->m_y_max = Speed(y_max_ll, this->y_unit);

	qDebug() << SG_PREFIX_I << "TrackData ready:" << *this;

	return sg_ret::ok;
}




/**
   Simple method for copying "altitude over time" information from Track to TrackData.

   This uses the 'time' based method to make the graph, (which is a simpler compared to the elevation/distance).
   This results in a slightly blocky graph when it does not have many trackpoints: <60.
   NB Somehow the elevation/distance applies some kind of smoothing algorithm,
   but I don't think any one understands it any more (I certainly don't ATM).

   @reviewed-on tbd
*/
template <> /* Template specialisation for specific type. */
sg_ret TrackData<Time, Altitude>::make_track_data_x_over_y(Track * trk)
{
	TrackData result;

	bool extremes_initialized = false;
	TimeType::LL x_min_ll = 0;
	TimeType::LL x_max_ll = 0;
	AltitudeType::LL y_min_ll = 0;
	AltitudeType::LL y_max_ll = 0;

	const Duration duration = trk->get_duration();
	if (!duration.is_valid() || duration.is_negative()) {
		qDebug() << SG_PREFIX_W << "Trying to calculate track data from track with incorrect duration" << duration;
		return sg_ret::err;
	}


	const int tp_count = trk->get_tp_count();
	if (tp_count < 1) {
		qDebug() << SG_PREFIX_W << "Trying to calculate track data from empty track";
		return sg_ret::err;
	}
	this->allocate(tp_count);


	int i = 0;
	auto iter = trk->trackpoints.begin();
	do {
		this->m_x_ll[i] = (*iter)->timestamp.ll_value();
		if (i > 0 && this->m_x_ll[i] <= this->m_x_ll[i - 1]) {
			/* TODO_LATER: this doesn't solve problem in any way if glitch is at the beginning of dataset. */
			qDebug() << SG_PREFIX_W << "Glitch in timestamps" << i << this->m_x_ll[i] << this->m_x_ll[i - 1];
			this->m_x_ll[i] = this->m_x_ll[i - 1];
		}

		const bool y_valid = (*iter)->altitude.is_valid();
		this->m_y_ll[i] = y_valid ? (*iter)->altitude.ll_value() : NAN;
		TRW_TRACK_DATA_UPDATE_MIN_MAX(this, i, y_valid);

		this->m_tps[i] = (*iter);

		i++;
		iter++;
	} while (iter != trk->trackpoints.end());


	assert (i == tp_count);
	qDebug() << SG_PREFIX_D << "Collected" << i << "track data values";


	this->m_valid = true;
	this->x_domain = GisViewportDomain::TimeDomain;
	this->y_domain = GisViewportDomain::ElevationDomain;

	this->x_unit = TimeType::Unit::internal_unit();
	this->y_unit = AltitudeType::Unit::internal_unit();

	snprintf(this->m_debug, sizeof (this->m_debug), "%s", "Altitude over Time");

	this->m_x_min = Time(x_min_ll, this->x_unit);
	this->m_x_max = Time(x_max_ll, this->x_unit);
	this->m_y_min = Altitude(y_min_ll, this->y_unit);
	this->m_y_max = Altitude(y_max_ll, this->y_unit);

	qDebug() << SG_PREFIX_I << "TrackData ready:" << *this;

	return sg_ret::ok;
}




/**
   Make a speed/distance map.

   @reviewed-on tbd
*/
template <> /* Template specialisation for specific type. */
sg_ret TrackData<Distance, Speed>::make_track_data_x_over_y(Track * trk)
{
	TrackData result;

	bool extremes_initialized = false;
	DistanceType::LL x_min_ll = 0;
	DistanceType::LL x_max_ll = 0;
	SpeedType::LL y_min_ll = 0;
	SpeedType::LL y_max_ll = 0;

	const double total_length = trk->get_length_value_including_gaps();
	if (total_length <= 0) {
		return sg_ret::err;
	}

	const int tp_count = trk->get_tp_count();
	TrackData<Time, Distance> data_dt;
	data_dt.make_track_data_x_over_y(trk);

	this->allocate(tp_count);

	const int side = (this->m_window_size - 1) / 2;

	int i = 0;
	this->m_x_ll[i] = 0;
	this->m_y_ll[i] = 0;
	this->m_tps[i] = data_dt.tp(i);
	TRW_TRACK_DATA_UPDATE_MIN_MAX(this, i, (!std::isnan(this->m_y_ll[i])));
	i++;

	for (; i < tp_count; i++) {
		if (data_dt.x_ll(i) <= data_dt.x_ll(i - 1)) {
			/* Handle glitch in values of consecutive time stamps.
			   TODO_LATER: improve code that calculates pseudo-values of result when a glitch has been found. */
			qDebug() << SG_PREFIX_W << "Glitch in timestamps" << i << data_dt.x_ll(i) << data_dt.x_ll(i - 1);
			this->m_x_ll[i] = this->m_x_ll[i - 1]; /* TODO_LATER: This won't work for a two or more invalid timestamps in a row. */
			this->m_y_ll[i] = 0;
		} else {
			double delta_d = 0.0;
			double delta_t = 0.0;
			for (int j = i - side; j <= i + side; j++) {
				if (j - 1 >= 0 && j < tp_count) {
					delta_d += (data_dt.y_ll(j) - data_dt.y_ll(j - 1));
					delta_t += (data_dt.x_ll(j) - data_dt.x_ll(j - 1));
				}
			}

			this->m_y_ll[i] = delta_d / delta_t;
			this->m_x_ll[i] = this->m_x_ll[i - 1] + (delta_d / (side + 1 + side)); /* Accumulate the distance. */
			TRW_TRACK_DATA_UPDATE_MIN_MAX(this, i, true);
		}
		this->m_tps[i] = data_dt.tp(i);
	}

	assert(i == tp_count);

	this->m_valid = true;
	this->x_domain = GisViewportDomain::DistanceDomain;
	this->y_domain = GisViewportDomain::SpeedDomain;

	this->x_unit = DistanceType::Unit::internal_unit();
	this->y_unit = SpeedType::Unit::internal_unit();

	snprintf(this->m_debug, sizeof (this->m_debug), "%s", "Speed over Distance");

	this->m_x_min = Distance(x_min_ll, this->x_unit);
	this->m_x_max = Distance(x_max_ll, this->x_unit);
	this->m_y_min = Speed(y_min_ll, this->y_unit);
	this->m_y_max = Speed(y_max_ll, this->y_unit);

	qDebug() << SG_PREFIX_I << "TrackData ready:" << *this;

	return sg_ret::ok;
}




} /* namespace SlavGPS */
