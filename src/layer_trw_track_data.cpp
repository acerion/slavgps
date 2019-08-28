/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (c) 2012, Rob Norris <rw_norris@hotmail.com>
 * Copyright (c) 2016-2019, Kamil Ignacak <acerion@wp.pl>
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
  See comment in class declaration for explanation of why we
  initialize min/max the way we do it here.
*/
#define TRW_TRACK_DATA_CALCULATE_MIN_MAX(_track_data_, _i_, _point_valid_) \
	if ((_point_valid_)) {						\
		if (!_track_data_->extremes_initialized) {		\
			_track_data_->x_min_ll = _track_data_->x[_i_];	\
			_track_data_->x_max_ll = _track_data_->x[_i_];	\
			_track_data_->y_min_ll = _track_data_->y[_i_];	\
			_track_data_->y_max_ll = _track_data_->y[_i_];	\
			_track_data_->extremes_initialized = true;	\
		}							\
									\
		if (_track_data_->x[_i_] < _track_data_->x_min_ll) {	\
			_track_data_->x_min_ll = _track_data_->x[_i_];	\
		} else if (_track_data_->x[_i_] > _track_data_->x_max_ll) { \
			_track_data_->x_max_ll = _track_data_->x[_i_];	\
		}							\
									\
		if (_track_data_->y[_i_] < _track_data_->y_min_ll) {	\
			_track_data_->y_min_ll = _track_data_->y[_i_];	\
		} else if (_track_data_->y[_i_] > _track_data_->y_max_ll) { \
			_track_data_->y_max_ll = _track_data_->y[_i_];	\
		}							\
	}								\




/**
   @reviewed-on tbd
*/
template <typename Tx, typename Tx_ll, typename Ty, typename Ty_ll>
sg_ret TrackData<Tx, Tx_ll, Ty, Ty_ll>::compress_into(TrackData & target, int compressed_n_points) const
{
	target.invalidate();
	if (sg_ret::ok != target.allocate_vector(compressed_n_points)) {
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
			acc_y += this->y[j];
			tp_index++;
		}

		//qDebug() << "------- i =" << i << "/" << target.n_points << "sampling_size =" << sampling_size << "n_tps_compressed =" << n_tps_compressed << "n_tps_compressed + sampling_size =" << n_tps_compressed + sampling_size << acc_y << acc_y / sampling_size;

		target.x[i] = acc_x / sampling_size;
		target.y[i] = acc_y / sampling_size;
		target.tps[i] = this->tps[n_tps_compressed];

		n_tps_compressed += sampling_size;
	}

	assert(i == target.n_points);

	target.valid = true;
	target.calculate_min_max(); /* TODO: rethink how we calculate min/max of compressed. */
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
sg_ret TrackData<Time, Time_ll, Distance, Distance_ll>::make_track_data_distance_over_time(Track * trk)
{
	/* No special handling of segments ATM... */


	const Time duration = trk->get_duration();
	if (!duration.is_valid() || duration.is_negative()) {
		qDebug() << SG_PREFIX_W << "Trying to calculate track data from track with incorrect duration" << duration;
		return sg_ret::err;
	}


	const int tp_count = trk->get_tp_count();
	if (tp_count < 1) {
		qDebug() << SG_PREFIX_W << "Trying to calculate track data from empty track";
		return sg_ret::err;
	}
	this->allocate_vector(tp_count);


	int i = 0;
	auto iter = trk->trackpoints.begin();
	this->x[i] = (*iter)->timestamp.get_ll_value();
	this->y[i] = 0;
	this->tps[i] = (*iter);
	TRW_TRACK_DATA_CALCULATE_MIN_MAX(this, i, (!std::isnan(this->y[i])));
	i++;
	iter++;

	while (iter != trk->trackpoints.end()) {

		this->x[i] = (*iter)->timestamp.get_ll_value();
		if (i > 0 && this->x[i] <= this->x[i - 1]) {
			/* TODO: this doesn't solve problem in any way if glitch is at the beginning of dataset. */
			qDebug() << SG_PREFIX_W << "Glitch in timestamps" << i << this->x[i] << this->x[i - 1];
			this->x[i] = this->x[i - 1];
		}

		this->y[i] = this->y[i - 1] + Coord::distance((*std::prev(iter))->coord, (*iter)->coord);

		this->tps[i] = (*iter);

		TRW_TRACK_DATA_CALCULATE_MIN_MAX(this, i, (!std::isnan(this->y[i])));

		i++;
		iter++;

#if 0
		if (data_dt.x[i] <= data_dt.x[i - 1]) {
			/* Handle glitch in values of consecutive time stamps.
			   TODO_LATER: improve code that calculates pseudo-values of result when a glitch has been found. */
			qDebug() << SG_PREFIX_W << "Glitch in timestamps" << i << data_dt.x[i] << data_dt.x[i - 1];
			this->x[i] = data_dt.x[i - 1];
			this->y[i] = NAN;
			TRW_TRACK_DATA_CALCULATE_MIN_MAX(this, i, false);
		} else {
			this->x[i] = data_dt.x[i];
			this->y[i] = data_dt.y[i - 1];
			TRW_TRACK_DATA_CALCULATE_MIN_MAX(this, i, true);
		}
#endif
	}

	assert (i == tp_count);
	qDebug() << SG_PREFIX_D << "Collected" << i << "track data values";

#if 0 /* Debug. */
	for (int j = 0; j < tp_count; j++) {
		qDebug() << SG_PREFIX_I << "Distance over time: t[" << j << "] = " << this->x[j] << ", d[" << j << "] =" << this->y[j];
	}
#endif

	this->valid = true;
	this->x_domain = GisViewportDomain::TimeDomain;
	this->y_domain = GisViewportDomain::DistanceDomain;
	this->y_distance_unit = DistanceUnit::Meters;
	snprintf(this->debug, sizeof (this->debug), "Distance over Time");

	this->x_min = Time(this->x_min_ll, Time::get_internal_unit());
	this->x_max = Time(this->x_max_ll, Time::get_internal_unit());
	this->y_min = Distance(this->y_min_ll, Distance::get_internal_unit());
	this->y_max = Distance(this->y_max_ll, Distance::get_internal_unit());

	qDebug() << SG_PREFIX_I << "TrackData ready:" << *this;

	return sg_ret::ok;
}




template <> /* Template specialisation for specific type. */
sg_ret TrackData<Distance, Distance_ll, Altitude, Altitude_ll>::make_track_data_altitude_over_distance(Track * trk)
{
	TrackData result;

	const double total_length = trk->get_length_value_including_gaps();
	if (total_length <= 0) {
		return sg_ret::err;
	}

	const int tp_count = trk->get_tp_count();
	this->allocate_vector(tp_count);


	int i = 0;
	bool y_valid = false;

	auto iter = trk->trackpoints.begin();
	/* Initial step. */
	{
		this->x[i] = 0; /* Distance at the beginning is zero. */

		y_valid = (*iter)->altitude.is_valid();
		this->y[i] = y_valid ? (*iter)->altitude.get_ll_value() : NAN;
		this->y[i] = (*iter)->altitude.get_ll_value();

		this->tps[i] = (*iter);
		TRW_TRACK_DATA_CALCULATE_MIN_MAX(this, i, (!std::isnan(this->y[i])));
		i++;
		iter++;
	}


	while (iter != trk->trackpoints.end()) {

		/* Iterative step. */
		{
			/* Accumulate distance. */
			this->x[i] = this->x[i - 1] + Coord::distance((*std::prev(iter))->coord, (*iter)->coord);

			y_valid = (*iter)->altitude.is_valid();
			this->y[i] = y_valid ? (*iter)->altitude.get_ll_value() : NAN;
			this->y[i] = (*iter)->altitude.get_ll_value();

			this->tps[i] = (*iter);
			TRW_TRACK_DATA_CALCULATE_MIN_MAX(this, i, (!std::isnan(this->y[i])));
			i++;
			iter++;
		}
	}

	assert(i == tp_count);

	this->valid = true;
	this->x_domain = GisViewportDomain::DistanceDomain;
	this->y_domain = GisViewportDomain::ElevationDomain;
	snprintf(this->debug, sizeof (this->debug), "Altitude over Distance");

	this->x_min = Distance(this->x_min_ll, Distance::get_internal_unit());
	this->x_max = Distance(this->x_max_ll, Distance::get_internal_unit());
	this->y_min = Altitude(this->y_min_ll, Altitude::get_internal_unit());
	this->y_max = Altitude(this->y_max_ll, Altitude::get_internal_unit());

	qDebug() << SG_PREFIX_I << "TrackData ready:" << *this;

	return sg_ret::ok;
}



/**
   I understood this when I wrote it ... maybe ... Basically it eats up the
   proper amounts of length on the track and averages elevation over that.

   @reviewed-on tbd
*/
template <> /* Template specialisation for specific type. */
sg_ret TrackData<Distance, Distance_ll, Altitude, Altitude_ll>::make_track_data_altitude_over_distance(Track * trk, int compressed_n_points)
{
	TrackData result;

	/* TODO: this function does not set this->tps[]. */


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

			if ((*iter)->altitude.get_ll_value() > SG_ALTITUDE_RANGE_MAX) {
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

	this->allocate_vector(compressed_n_points);

	double current_dist = 0.0;
	double current_area_under_curve = 0;


	auto iter = trk->trackpoints.begin();
	double current_seg_length = Coord::distance((*iter)->coord, (*std::next(iter))->coord);

	double altitude1 = (*iter)->altitude.get_ll_value();
	double altitude2 = (*std::next(iter))->altitude.get_ll_value();
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
				this->y[current_chunk] = altitude1;
				if (current_chunk > 0) {
					/* TODO_LATER: verify this. */
					this->x[current_chunk] = this->x[current_chunk - 1] + delta_d;
				}
			} else {
				this->y[current_chunk] = altitude1 + (altitude2 - altitude1) * ((dist_along_seg - (delta_d / 2)) / current_seg_length);
				if (current_chunk > 0) {
					/* TODO_LATER: verify this. */
					this->x[current_chunk] = this->x[current_chunk - 1] + delta_d;
				}
			}
			TRW_TRACK_DATA_CALCULATE_MIN_MAX(this, current_chunk, true);
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
				altitude1 = (*iter)->altitude.get_ll_value();
				altitude2 = (*std::next(iter))->altitude.get_ll_value();
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

				this->y[current_chunk] = current_area_under_curve / current_dist;
				if (current_chunk > 0) {
					/* TODO_LATER: verify this. */
					this->x[current_chunk] = this->x[current_chunk - 1] + delta_d;
				}
				if (std::next(iter) == trk->trackpoints.end()) {
					for (int i = current_chunk + 1; i < compressed_n_points; i++) {
						this->y[i] = this->y[current_chunk];
						if (current_chunk > 0) {
							/* TODO_LATER: verify this. */
							this->x[i] = this->x[current_chunk - 1] + delta_d;
						}
					}
					break;
				}
			} else {
				current_area_under_curve += dist_along_seg * (altitude1 + (altitude2 - altitude1) * dist_along_seg / current_seg_length);
				this->y[current_chunk] = current_area_under_curve / delta_d;
				if (current_chunk > 0) {
					/* TODO_LATER: verify this. */
					this->x[current_chunk] = this->x[current_chunk - 1] + delta_d;
				}
			}

			TRW_TRACK_DATA_CALCULATE_MIN_MAX(this, current_chunk, true);

			current_dist = 0;
			current_chunk++;
		}
	}

#ifdef K_FIXME_RESTORE
	assert(current_chunk == compressed_n_points);
#endif

	this->valid = true;
	this->x_domain = GisViewportDomain::DistanceDomain;
	this->y_domain = GisViewportDomain::ElevationDomain;
	snprintf(this->debug, sizeof (this->debug), "Altitude over Distance");

	this->x_min = Distance(this->x_min_ll, Distance::get_internal_unit());
	this->x_max = Distance(this->x_max_ll, Distance::get_internal_unit());
	this->y_min = Altitude(this->y_min_ll, Altitude::get_internal_unit());
	this->y_max = Altitude(this->y_max_ll, Altitude::get_internal_unit());

	qDebug() << SG_PREFIX_I << "TrackData ready:" << *this;

	return sg_ret::ok;
}




/**
   @reviewed-on tbd
*/
template <> /* Template specialisation for specific type. */
sg_ret TrackData<Distance, Distance_ll, Gradient, Gradient_ll>::make_track_data_gradient_over_distance(Track * trk)
{
	TrackData result;

	const int tp_count = trk->get_tp_count();
	if (tp_count < 2) {
		qDebug() << SG_PREFIX_W << "Trying to calculate track data from track with size" << tp_count;
		return sg_ret::err;
	}


	TrackData<Distance, Distance_ll, Altitude, Altitude_ll> data_ad;
	data_ad.make_track_data_altitude_over_distance(trk);
	if (!data_ad.valid) {
		qDebug() << SG_PREFIX_I << "Failed to collect 'altitude over distance' track data";
		return sg_ret::err;
	}

	this->allocate_vector(tp_count);

	int i = 0;
	Gradient_ll gradient;
	for (i = 0; i < (tp_count - 1); i++) {

		this->x[i] = data_ad.x[i];

		const Distance_ll delta_distance = data_ad.x[i + 1] - data_ad.x[i];

		if (delta_distance <= 0.0) { /* e.g. two trackpoints in the same location. */
			this->y[i] = 0;
		} else {
			const Altitude_ll delta_altitude = data_ad.y[i + 1] - data_ad.y[i];
			gradient = 100.0 * delta_altitude / delta_distance;
			this->y[i] = gradient;
		}
		this->tps[i] = data_ad.tps[i];

		TRW_TRACK_DATA_CALCULATE_MIN_MAX(this, i, true);

	}
	this->x[i] = data_ad.x[i];
	this->y[i] = gradient;
	this->tps[i] = data_ad.tps[i];
	TRW_TRACK_DATA_CALCULATE_MIN_MAX(this, i, true);

	assert (i + 1 == tp_count);

	this->valid = true;
	this->x_domain = GisViewportDomain::DistanceDomain;
	this->y_domain = GisViewportDomain::GradientDomain;
	snprintf(this->debug, sizeof (this->debug), "Gradient over Distance");

	this->x_min = Distance(this->x_min_ll, Distance::get_internal_unit());
	this->x_max = Distance(this->x_max_ll, Distance::get_internal_unit());
	this->y_min = Gradient(this->y_min_ll, Gradient::get_internal_unit());
	this->y_max = Gradient(this->y_max_ll, Gradient::get_internal_unit());

	qDebug() << SG_PREFIX_I << "TrackData ready:" << *this;

	return sg_ret::ok;
}




/**
   By Alex Foobarian.

   @reviewed-on tbd
*/
template <> /* Template specialisation for specific type. */
sg_ret TrackData<Time, Time_ll, Speed, Speed_ll>::make_track_data_speed_over_time(Track * trk)
{
	TrackData result;

	const Time duration = trk->get_duration();
	if (!duration.is_valid() || duration.is_negative()) {
		qDebug() << SG_PREFIX_W << "Trying to calculate track data from track with incorrect duration" << duration;
		return sg_ret::err;
	}


	const int tp_count = trk->get_tp_count();
	if (tp_count < 1) {
		qDebug() << SG_PREFIX_W << "Trying to calculate track data from empty track";
		return sg_ret::err;
	}
	TrackData<Time, Time_ll, Distance, Distance_ll> data_dt;
	data_dt.make_track_data_distance_over_time(trk);


	if (data_dt.n_points != tp_count) {
		qDebug() << SG_PREFIX_E << "Mismatch of data: data points in 'distance over time' =" << data_dt.n_points << ", trackpoints count =" << tp_count;
	}
	assert (data_dt.n_points == tp_count);


	this->allocate_vector(tp_count);


	int i = 0;
	this->x[i] = data_dt.x[i];
	this->y[i] = 0.0;
	this->tps[i] = data_dt.tps[i];
	TRW_TRACK_DATA_CALCULATE_MIN_MAX(this, i, (!std::isnan(this->y[i])));
	i++;

	for (; i < tp_count; i++) {
		/* TODO: handle invalid distance values in data_dt. */

		if (data_dt.x[i] <= data_dt.x[i - 1]) {
			/* Handle glitch in values of consecutive time stamps.
			   TODO_LATER: improve code that calculates pseudo-values of result when a glitch has been found. */
			qDebug() << SG_PREFIX_W << "Glitch in timestamps:" << i << data_dt.x[i] << data_dt.x[i - 1];
			this->x[i] = data_dt.x[i - 1];
			this->y[i] = 0;

			TRW_TRACK_DATA_CALCULATE_MIN_MAX(this, i, false);

		} else {
			const double delta_t = (data_dt.x[i] - data_dt.x[i - 1]);
			const double delta_d = (data_dt.y[i] - data_dt.y[i - 1]);

			this->x[i] = data_dt.x[i];
			this->y[i] = delta_d / delta_t;

			TRW_TRACK_DATA_CALCULATE_MIN_MAX(this, i, true);
		}
		this->tps[i] = data_dt.tps[i];
	}

	this->valid = true;
	this->x_domain = GisViewportDomain::TimeDomain;
	this->y_domain = GisViewportDomain::SpeedDomain;
	this->y_speed_unit = SpeedUnit::MetresPerSecond;
	snprintf(this->debug, sizeof (this->debug), "Speed over Time");

	this->x_min = Time(this->x_min_ll, Time::get_internal_unit());
	this->x_max = Time(this->x_max_ll, Time::get_internal_unit());
	this->y_min = Speed(this->y_min_ll, Speed::get_internal_unit());
	this->y_max = Speed(this->y_max_ll, Speed::get_internal_unit());

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
sg_ret TrackData<Time, Time_ll, Altitude, Altitude_ll>::make_track_data_altitude_over_time(Track * trk)
{
	TrackData result;


	const Time duration = trk->get_duration();
	if (!duration.is_valid() || duration.is_negative()) {
		qDebug() << SG_PREFIX_W << "Trying to calculate track data from track with incorrect duration" << duration;
		return sg_ret::err;
	}


	const int tp_count = trk->get_tp_count();
	if (tp_count < 1) {
		qDebug() << SG_PREFIX_W << "Trying to calculate track data from empty track";
		return sg_ret::err;
	}
	this->allocate_vector(tp_count);


	int i = 0;
	auto iter = trk->trackpoints.begin();
	do {
		this->x[i] = (*iter)->timestamp.get_ll_value();
		if (i > 0 && this->x[i] <= this->x[i - 1]) {
			/* TODO: this doesn't solve problem in any way if glitch is at the beginning of dataset. */
			qDebug() << SG_PREFIX_W << "Glitch in timestamps" << i << this->x[i] << this->x[i - 1];
			this->x[i] = this->x[i - 1];
		}

		const bool y_valid = (*iter)->altitude.is_valid();
		this->y[i] = y_valid ? (*iter)->altitude.get_ll_value() : NAN;
		TRW_TRACK_DATA_CALCULATE_MIN_MAX(this, i, y_valid);

		this->tps[i] = (*iter);

		i++;
		iter++;
	} while (iter != trk->trackpoints.end());


	assert (i == tp_count);
	qDebug() << SG_PREFIX_D << "Collected" << i << "track data values";


	this->valid = true;
	this->x_domain = GisViewportDomain::TimeDomain;
	this->y_domain = GisViewportDomain::ElevationDomain;
	snprintf(this->debug, sizeof (this->debug), "Altitude over Time");

	this->x_min = Time(this->x_min_ll, Time::get_internal_unit());
	this->x_max = Time(this->x_max_ll, Time::get_internal_unit());
	this->y_min = Altitude(this->y_min_ll, Altitude::get_internal_unit());
	this->y_max = Altitude(this->y_max_ll, Altitude::get_internal_unit());

	qDebug() << SG_PREFIX_I << "TrackData ready:" << *this;

	return sg_ret::ok;
}




/**
   Make a speed/distance map.

   @reviewed-on tbd
*/
template <> /* Template specialisation for specific type. */
sg_ret TrackData<Distance, Distance_ll, Speed, Speed_ll>::make_track_data_speed_over_distance(Track * trk)
{
	TrackData result;

	const double total_length = trk->get_length_value_including_gaps();
	if (total_length <= 0) {
		return sg_ret::err;
	}

	const int tp_count = trk->get_tp_count();
	TrackData<Time, Time_ll, Distance, Distance_ll> data_dt;
	data_dt.make_track_data_distance_over_time(trk);

	this->allocate_vector(tp_count);


	int i = 0;
	this->x[i] = 0;
	this->y[i] = 0;
	this->tps[i] = data_dt.tps[i];
	TRW_TRACK_DATA_CALCULATE_MIN_MAX(this, i, (!std::isnan(this->y[i])));
	i++;

	for (; i < tp_count; i++) {
		if (data_dt.x[i] <= data_dt.x[i - 1]) {
			/* Handle glitch in values of consecutive time stamps.
			   TODO_LATER: improve code that calculates pseudo-values of result when a glitch has been found. */
			qDebug() << SG_PREFIX_W << "Glitch in timestamps" << i << data_dt.x[i] << data_dt.x[i - 1];
			this->x[i] = this->x[i - 1]; /* TODO_LATER: This won't work for a two or more invalid timestamps in a row. */
			this->y[i] = 0;
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

			this->y[i] = delta_d / delta_t;
			this->x[i] = this->x[i - 1] + (delta_d / (n + 1 + n)); /* Accumulate the distance. */
			TRW_TRACK_DATA_CALCULATE_MIN_MAX(this, i, true);
		}
		this->tps[i] = data_dt.tps[i];
	}

	assert(i == tp_count);

	this->valid = true;
	this->x_domain = GisViewportDomain::DistanceDomain;
	this->y_domain = GisViewportDomain::SpeedDomain;
	snprintf(this->debug, sizeof (this->debug), "Speed over Distance");

	this->x_min = Distance(this->x_min_ll, Distance::get_internal_unit());
	this->x_max = Distance(this->x_max_ll, Distance::get_internal_unit());
	this->y_min = Speed(this->y_min_ll, Speed::get_internal_unit());
	this->y_max = Speed(this->y_max_ll, Speed::get_internal_unit());

	qDebug() << SG_PREFIX_I << "TrackData ready:" << *this;

	return sg_ret::ok;
}




template <>
sg_ret TrackData<Distance, Distance_ll, Speed, Speed_ll>::apply_unit_conversions(SpeedUnit speed_unit, DistanceUnit distance_unit, HeightUnit height_unit)
{
	if (sg_ret::ok != this->apply_unit_conversion_distance(this->x, this->x_min, this->x_max, distance_unit)) {
		qDebug() << "EE   " << __func__ << __LINE__ << "TrackData: Failed to apply distance unit conversion to x vector";
		return sg_ret::err;
	}
	if (sg_ret::ok != this->apply_unit_conversion_speed(this->y, this->y_min, this->y_max, speed_unit)) {
		qDebug() << "EE   " << __func__ << __LINE__ << "TrackData: Failed to apply speed unit conversion to y vector";
		return sg_ret::err;
	}
	return sg_ret::ok;
}




template <>
sg_ret TrackData<Distance, Distance_ll, Altitude, Altitude_ll>::apply_unit_conversions(SpeedUnit speed_unit, DistanceUnit distance_unit, HeightUnit height_unit)
{
	if (sg_ret::ok != this->apply_unit_conversion_distance(this->x, this->x_min, this->x_max, distance_unit)) {
		qDebug() << "EE   " << __func__ << __LINE__ << "TrackData: Failed to apply distance unit conversion to x vector";
		return sg_ret::err;
	}
	if (sg_ret::ok != this->apply_unit_conversion_height(this->y, this->y_min, this->y_max, height_unit)) {
		qDebug() << "EE   " << __func__ << __LINE__ << "TrackData: Failed to apply height unit conversion to y vector";
		return sg_ret::err;
	}
	return sg_ret::ok;
}




template <>
sg_ret TrackData<Distance, Distance_ll, Gradient, Gradient_ll>::apply_unit_conversions(SpeedUnit speed_unit, DistanceUnit distance_unit, HeightUnit height_unit)
{
	if (sg_ret::ok != this->apply_unit_conversion_distance(this->x, this->x_min, this->x_max, distance_unit)) {
		qDebug() << "EE   " << __func__ << __LINE__ << "TrackData: Failed to apply distance unit conversion to x vector";
		return sg_ret::err;
	}
	if (sg_ret::ok != this->apply_unit_conversion_gradient(this->y, this->y_min, this->y_max, GradientUnit::Percents)) {
		qDebug() << "EE   " << __func__ << __LINE__ << "TrackData: Failed to apply time unit conversion to y vector";
		return sg_ret::err;
	}
	return sg_ret::ok;
}




template <>
sg_ret TrackData<Time, Time_ll, Speed, Speed_ll>::apply_unit_conversions(SpeedUnit speed_unit, DistanceUnit distance_unit, HeightUnit height_unit)
{
	if (sg_ret::ok != this->apply_unit_conversion_time(this->x, this->x_min, this->x_max, TimeUnit::Seconds)) {
		qDebug() << "EE   " << __func__ << __LINE__ << "TrackData: Failed to apply time unit conversion to x";
		return sg_ret::err;
	}
	if (sg_ret::ok != this->apply_unit_conversion_speed(this->y, this->y_min, this->y_max, speed_unit)) {
		qDebug() << "EE   " << __func__ << __LINE__ << "TrackData: Failed to apply speed unit conversion to y vector";
		return sg_ret::err;
	}

	return sg_ret::ok;
}




template <>
sg_ret TrackData<Time, Time_ll, Distance, Distance_ll>::apply_unit_conversions(SpeedUnit speed_unit, DistanceUnit distance_unit, HeightUnit height_unit)
{
	if (sg_ret::ok != this->apply_unit_conversion_time(this->x, this->x_min, this->x_max, TimeUnit::Seconds)) {
		qDebug() << "EE   " << __func__ << __LINE__ << "TrackData: Failed to apply time unit conversion to x";
		return sg_ret::err;
	}
	if (sg_ret::ok != this->apply_unit_conversion_distance(this->y, this->y_min, this->y_max, distance_unit)) {
		qDebug() << "EE   " << __func__ << __LINE__ << "TrackData: Failed to apply distance unit conversion to y vector";
		return sg_ret::err;
	}
	return sg_ret::ok;
}




template <>
sg_ret TrackData<Time, Time_ll, Altitude, Altitude_ll>::apply_unit_conversions(SpeedUnit speed_unit, DistanceUnit distance_unit, HeightUnit height_unit)
{
	if (sg_ret::ok != this->apply_unit_conversion_time(this->x, this->x_min, this->x_max, TimeUnit::Seconds)) {
		qDebug() << "EE   " << __func__ << __LINE__ << "TrackData: Failed to apply time unit conversion to x";
		return sg_ret::err;
	}
	if (sg_ret::ok != this->apply_unit_conversion_height(this->y, this->y_min, this->y_max, height_unit)) {
		qDebug() << "EE   " << __func__ << __LINE__ << "TrackData: Failed to apply height unit conversion to y vector";
		return sg_ret::err;
	}

	return sg_ret::ok;
}




} /* namespace SlavGPS */




sg_ret TrackDataBase::apply_unit_conversion_distance(Distance_ll * values, Distance & min, Distance & max, DistanceUnit to_unit)
{
	if (NULL == values) {
		qDebug() << "EE   " << __func__ << __LINE__ << "TrackData: Can't apply distance unit conversion, values vector is NULL";
		return sg_ret::err;
	}

	const DistanceUnit from_unit = min.get_unit();
	if (from_unit != to_unit) {
		for (int i = 0; i < this->n_points; i++) {
			values[i] = Distance::convert_to_unit(values[i], from_unit, to_unit);
		}

		min.convert_to_unit_in_place(to_unit);
		max.convert_to_unit_in_place(to_unit);
	}

	return sg_ret::ok;
}




sg_ret TrackDataBase::apply_unit_conversion_speed(Speed_ll * values, Speed & min, Speed & max, SpeedUnit to_unit)
{
	if (NULL == values) {
		qDebug() << "EE   " << __func__ << __LINE__ << "TrackData: Can't apply speed unit conversion, values vector is NULL";
		return sg_ret::err;
	}
	const SpeedUnit from_unit = min.get_unit();
	if (from_unit != to_unit) {
		for (int i = 0; i < this->n_points; i++) {
			values[i] = Speed::convert_to_unit(values[i], from_unit, to_unit);
		}
		min.convert_to_unit_in_place(to_unit);
		max.convert_to_unit_in_place(to_unit);
	}

	return sg_ret::ok;
}




sg_ret TrackDataBase::apply_unit_conversion_height(Altitude_ll * values, Altitude & min, Altitude & max, HeightUnit to_unit)
{
	if (NULL == values) {
		qDebug() << "EE   " << __func__ << __LINE__ << "TrackData: Can't apply height unit conversion, values vector is NULL";
		return sg_ret::err;
	}

	const HeightUnit from_unit = min.get_unit();
	if (from_unit != to_unit) {
		for (int i = 0; i < this->n_points; i++) {
			values[i] = Altitude::convert_to_unit(values[i], from_unit, to_unit);
		}
		min.convert_to_unit_in_place(to_unit);
		max.convert_to_unit_in_place(to_unit);
	}

	return sg_ret::ok;
}




sg_ret TrackDataBase::apply_unit_conversion_time(Time_ll * values, Time & min, Time & max, TimeUnit to_unit)
{
	if (NULL == values) {
		qDebug() << "EE    " << __func__ << __LINE__ << "TrackData: Can't apply time unit conversion: values vector is NULL";
		return sg_ret::err;
	}

	/* There is only one unit for time, so there is no need to
	   convert anything to different unit.

	   TODO: check in future that this is still true.
	*/

	return sg_ret::ok;
}




sg_ret TrackDataBase::apply_unit_conversion_gradient(Gradient_ll * values, Gradient & min, Gradient & max, GradientUnit to_unit)
{
	if (NULL == values) {
		qDebug() << "EE    " << __func__ << __LINE__ << "TrackData: Can't apply gradient unit conversion: values vector is NULL";
		return sg_ret::err;
	}

	/* There is only one unit for gradient, so there is no need to
	   convert anything to different unit.

	   TODO: check in future that this is still true. */

	return sg_ret::ok;
}
