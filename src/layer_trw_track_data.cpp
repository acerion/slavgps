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




#define TRW_TRACK_DATA_CALCULATE_MIN_MAX(_track_data_, _i_, _y_valid_)	\
	if (_track_data_.x[_i_] > _track_data_.x_max) {			\
		_track_data_.x_max = _track_data_.x[_i_];		\
	}								\
	if (_track_data_.x[_i_] < _track_data_.x_min) {			\
		_track_data_.x_min = _track_data_.x[_i_];		\
	}								\
	if (_y_valid_) {						\
		if (_track_data_.y[_i_] > _track_data_.y_max) {		\
			_track_data_.y_max = _track_data_.y[_i_];	\
		}							\
		if (_track_data_.y[_i_] < _track_data_.y_min) {		\
			_track_data_.y_min = _track_data_.y[_i_];	\
		}							\
	}								\




/**
   @reviewed-on tbd
*/
TrackData::TrackData()
{
}




/**
   @reviewed-on tbd
*/
sg_ret TrackData::do_compress(TrackData & compressed_data) const
{
	const double tps_per_data_point = 1.0 * this->n_points / compressed_data.n_points;
	const int floor_ = floor(tps_per_data_point);
	const int ceil_ = ceil(tps_per_data_point);
	int n_tps_compressed = 0;

	//qDebug() << "-----------------------" << floor_ << tps_per_data_point << ceil_ << this->n_points << compressed_data.n_points;

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

		//qDebug() << "------- i =" << i << "/" << compressed_data.n_points << "sampling_size =" << sampling_size << "n_tps_compressed =" << n_tps_compressed << "n_tps_compressed + sampling_size =" << n_tps_compressed + sampling_size << acc_y << acc_y / sampling_size;

		compressed_data.x[i] = acc_x / sampling_size;
		compressed_data.y[i] = acc_y / sampling_size;

		n_tps_compressed += sampling_size;
	}

	assert(i == compressed_data.n_points);

	return sg_ret::ok;
}




/**
   @reviewed-on tbd
*/
TrackData TrackData::compress(int compressed_n_points) const
{
	TrackData compressed(compressed_n_points);

	this->do_compress(compressed);

	compressed.n_points = compressed_n_points;
	compressed.valid = true;

	return compressed;
}




/**
   @reviewed-on tbd
*/
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




/**
   @reviewed-on tbd
*/
void TrackData::calculate_min_max(void)
{
	this->x_min = this->x[0];
	this->x_max = this->x[0];
	for (int i = 0; i < this->n_points; i++) {
		if (this->x[i] > this->x_max) {
			this->x_max = this->x[i];
		}

		if (this->x[i] < this->x_min) {
			this->x_min = this->x[i];
		}
		qDebug() << "i =" << i << ", x =" << this->x[i] << ", x_min =" << this->x_min << ", x_max =" << this->x_max;
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
		qDebug() << "i =" << i << ", x =" << this->y[i] << ", y_min =" << this->y_min << ", y_max =" << this->y_max;
	}
}




/**
   @reviewed-on tbd
*/
void TrackData::allocate_vector(int n_data_points)
{
	if (this->x) {
		if (this->n_points) {
			qDebug() << SG_PREFIX_E << "Called the function for already allocated vector x";
		} else {
			qDebug() << SG_PREFIX_W << "Called the function for already allocated vector x";
		}
		free(this->x);
		this->x = NULL;
	}

	if (this->y) {
		if (this->n_points) {
			qDebug() << SG_PREFIX_E << "Called the function for already allocated vector y";
		} else {
			qDebug() << SG_PREFIX_W << "Called the function for already allocated vector y";
		}
		free(this->y);
		this->y = NULL;
	}

	this->x = (double *) malloc(sizeof (double) * n_data_points);
	this->y = (double *) malloc(sizeof (double) * n_data_points);

	memset(this->x, 0, sizeof (double) * n_data_points);
	memset(this->y, 0, sizeof (double) * n_data_points);

	this->n_points = n_data_points;
}




/**
   @reviewed-on tbd
*/
TrackData::TrackData(int n_data_points)
{
	this->allocate_vector(n_data_points);
}




/**
   @reviewed-on tbd
*/
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




/**
   @reviewed-on tbd
*/
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




/**
   @reviewed-on tbd
*/
sg_ret TrackData::y_distance_convert_units(DistanceUnit new_distance_unit)
{
	if (this->y_supplementary_distance_unit != SupplementaryDistanceUnit::Meters) {
		qDebug() << SG_PREFIX_E << "Unexpected y supplementary distance unit" << (int) this->y_supplementary_distance_unit << "in" << this->debug;
		return sg_ret::err;
	}

	for (int i = 0; i < this->n_points; i++) {
		this->y[i] = Distance::convert_meters_to(this->y[i], new_distance_unit);
	}

	this->y_min = Distance::convert_meters_to(this->y_min, new_distance_unit);
	this->y_max = Distance::convert_meters_to(this->y_max, new_distance_unit);

	this->y_supplementary_distance_unit = (SupplementaryDistanceUnit) -1;
	this->y_distance_unit = new_distance_unit;

	return sg_ret::ok;
}




/**
   @reviewed-on tbd
*/
sg_ret TrackData::y_speed_convert_units(SpeedUnit new_speed_unit)
{
	if (this->y_speed_unit != SpeedUnit::MetresPerSecond) {
		qDebug() << SG_PREFIX_E << "Unexpected speed unit" << (int) this->y_speed_unit << "in" << this->debug;
		return sg_ret::err;
	}

	for (int i = 0; i < this->n_points; i++) {
		this->y[i] = Speed::convert_mps_to(this->y[i], new_speed_unit);
	}

	this->y_min = Speed::convert_mps_to(this->y_min, new_speed_unit);
	this->y_max = Speed::convert_mps_to(this->y_max, new_speed_unit);

	if (this->y_min < 0.0) {
		this->y_min = 0; /* TODO: old comment, to be verified: "Splines sometimes give negative speeds". */
	}

	this->y_speed_unit = new_speed_unit;

	return sg_ret::ok;
}




/**
   @reviewed-on tbd
*/
QDebug SlavGPS::operator<<(QDebug debug, const TrackData & track_data)
{
	if (track_data.valid) {
		debug << "TrackData" << track_data.debug << "is valid"
		      << ", x_min =" << track_data.x_min
		      << ", x_max =" << track_data.x_max
		      << ", y_min =" << track_data.y_min
		      << ", y_max =" << track_data.y_max;
	} else {
		debug << "TrackData" << track_data.debug << "is invalid";
	}

	return debug;
}




/**
   Simple method for copying "distance over time" information from Track to TrackData.

   @reviewed-on tbd
*/
TrackData TrackData::make_values_distance_over_time_helper(Track * trk)
{
	/* No special handling of segments ATM... */

	TrackData result;


	const int tp_count = trk->get_tp_count();
	if (tp_count < 1) {
		qDebug() << SG_PREFIX_W << "Trying to calculate track data from empty track";
		return result;
	}
	result.allocate_vector(tp_count);


	int i = 0;
	auto iter = trk->trackpoints.begin();
	result.x[i] = (*iter)->timestamp.get_value();
	result.y[i] = 0;
	TRW_TRACK_DATA_CALCULATE_MIN_MAX(result, i, (!std::isnan(result.y[i])));
	i++;
	iter++;

	while (iter != trk->trackpoints.end()) {

		result.x[i] = (*iter)->timestamp.get_value();
		result.y[i] = result.y[i - 1] + Coord::distance((*std::prev(iter))->coord, (*iter)->coord);

		TRW_TRACK_DATA_CALCULATE_MIN_MAX(result, i, (!std::isnan(result.y[i])));

		if (result.x[i] <= result.x[i - 1]) {
			qDebug() << SG_PREFIX_W << "Inconsistent time data at index" << i << ":" << result.x[i] << result.x[i - 1];
		}
		i++;
		iter++;
	}

	assert (i == tp_count);

#if 1 /* Debug. */
	for (int j = 0; j < tp_count; j++) {
		qDebug() << SG_PREFIX_I << "Distance over time: i =" << j << ", t = " << result.x[j] << ", d =" << result.y[j];
	}
#endif

	result.valid = true;
	result.x_domain = GisViewportDomain::Time;
	result.y_domain = GisViewportDomain::Distance;
	result.y_supplementary_distance_unit = SupplementaryDistanceUnit::Meters;
	snprintf(result.debug, sizeof (result.debug), "Distance over Time");

	return result;
}




/**
   Simple method for copying "altitude over time" information from Track to TrackData.

   @reviewed-on tbd
*/
TrackData TrackData::make_values_altitude_over_time_helper(Track * trk)
{
	TrackData result;


	const int tp_count = trk->get_tp_count();
	if (tp_count < 1) {
		qDebug() << SG_PREFIX_W << "Trying to calculate track data from empty track";
		return result;
	}
	result.allocate_vector(tp_count);


	int i = 0;
	auto iter = trk->trackpoints.begin();
	do {
		const bool y_valid = (*iter)->altitude.is_valid();

		result.x[i] = (*iter)->timestamp.get_value();
		result.y[i] = y_valid ? (*iter)->altitude.get_value() : NAN;
		TRW_TRACK_DATA_CALCULATE_MIN_MAX(result, i, y_valid);

		i++;
		iter++;
	} while (iter != trk->trackpoints.end());


	assert (i == tp_count);
	result.valid = true;
	result.x_domain = GisViewportDomain::Time;
	result.y_domain = GisViewportDomain::Elevation;
	snprintf(result.debug, sizeof (result.debug), "Altitude over Time");

	qDebug() << SG_PREFIX_D << "Collected" << i << "track data values";

	return result;
}




/**
   I understood this when I wrote it ... maybe ... Basically it eats up the
   proper amounts of length on the track and averages elevation over that.

   @reviewed-on tbd
*/
TrackData TrackData::make_track_data_altitude_over_distance(Track * trk, int compressed_n_points)
{
	assert (compressed_n_points < 16000); /* TODO: why this limitation? */

	TrackData result;


	const int tp_count = trk->get_tp_count();
	if (tp_count < 2) {
		qDebug() << SG_PREFIX_W << "Trying to calculate track data from track with size" << tp_count;
		return result;
	}


	{ /* Test if there's anything worth calculating. */

		bool correct = true;
		for (auto iter = trk->trackpoints.begin(); iter != trk->trackpoints.end(); iter++) {
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
			return result;
		}
	}

	double total_length = trk->get_length_value_including_gaps();
	const double delta_d = total_length / (compressed_n_points - 1);

	/* Zero delta_d (eg, track of 2 tp with the same loc) will cause crash */
	if (delta_d <= 0) {
		return result;
	}

	result.allocate_vector(compressed_n_points);

	double current_dist = 0.0;
	double current_area_under_curve = 0;


	auto iter = trk->trackpoints.begin();
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
				result.y[current_chunk] = altitude1;
				if (current_chunk > 0) {
					/* TODO_LATER: verify this. */
					result.x[current_chunk] = result.x[current_chunk - 1] + delta_d;
				}
			} else {
				result.y[current_chunk] = altitude1 + (altitude2 - altitude1) * ((dist_along_seg - (delta_d / 2)) / current_seg_length);
				if (current_chunk > 0) {
					/* TODO_LATER: verify this. */
					result.x[current_chunk] = result.x[current_chunk - 1] + delta_d;
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
			while (iter != trk->trackpoints.end()
			       && std::next(iter) != trk->trackpoints.end()) {

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
			    || (iter != trk->trackpoints.end()
				&& std::next(iter) == trk->trackpoints.end())) {

				result.y[current_chunk] = current_area_under_curve / current_dist;
				if (current_chunk > 0) {
					/* TODO_LATER: verify this. */
					result.x[current_chunk] = result.x[current_chunk - 1] + delta_d;
				}
				if (std::next(iter) == trk->trackpoints.end()) {
					for (int i = current_chunk + 1; i < compressed_n_points; i++) {
						result.y[i] = result.y[current_chunk];
						if (current_chunk > 0) {
							/* TODO_LATER: verify this. */
							result.x[i] = result.x[current_chunk - 1] + delta_d;
						}
					}
					break;
				}
			} else {
				current_area_under_curve += dist_along_seg * (altitude1 + (altitude2 - altitude1) * dist_along_seg / current_seg_length);
				result.y[current_chunk] = current_area_under_curve / delta_d;
				if (current_chunk > 0) {
					/* TODO_LATER: verify this. */
					result.x[current_chunk] = result.x[current_chunk - 1] + delta_d;
				}
			}

			current_dist = 0;
			current_chunk++;
		}
	}

#ifdef K_FIXME_RESTORE
	assert(current_chunk == compressed_n_points);
#endif

	result.valid = true;
	result.x_domain = GisViewportDomain::Distance;
	result.y_domain = GisViewportDomain::Elevation;
	snprintf(result.debug, sizeof (result.debug), "Altitude over Distance");

	return result;
}




/**
   @reviewed-on tbd
*/
TrackData TrackData::make_track_data_gradient_over_distance(Track * trk, int compressed_n_points)
{
	assert (compressed_n_points < 16000); /* TODO: why this limitation? */


	TrackData result;

	const int tp_count = trk->get_tp_count();
	if (tp_count < 2) {
		qDebug() << SG_PREFIX_W << "Trying to calculate track data from track with size" << tp_count;
		return result;
	}




	const double total_length = trk->get_length_value_including_gaps();
	const double delta_d = total_length / (compressed_n_points - 1);

	/* Zero delta_d (eg, track of 2 tp with the same loc) will cause crash. */
	if (delta_d <= 0) {
		return result;
	}

	TrackData compressed_ad;
	compressed_ad.make_track_data_altitude_over_distance(trk, compressed_n_points);
	if (!compressed_ad.valid) {
		return result;
	}

	result.allocate_vector(compressed_n_points);

	int i = 0;
	double current_gradient = 0.0;
	for (i = 0; i < (compressed_n_points - 1); i++) {
		const double altitude1 = compressed_ad.y[i];
		const double altitude2 = compressed_ad.y[i + 1];
		current_gradient = 100.0 * (altitude2 - altitude1) / delta_d;

		if (i > 0) {
			result.x[i] = result.x[i - 1] + delta_d;
		}
		result.y[i] = current_gradient;

	}
	result.x[i] = result.x[i - 1] + delta_d;
	result.y[i] = current_gradient;

	assert (i + 1 == compressed_n_points);

	result.valid = true;
	result.x_domain = GisViewportDomain::Distance;
	result.y_domain = GisViewportDomain::Gradient;
	snprintf(result.debug, sizeof (result.debug), "Gradient over Distance");


	return result;
}




/**
   By Alex Foobarian.

   @reviewed-on tbd
*/
TrackData TrackData::make_track_data_speed_over_time(Track * trk)
{
	TrackData result;

	const Time duration = trk->get_duration();
	if (!duration.is_valid() || duration.get_value() < 0) {
		return result;
	}

	const int tp_count = trk->get_tp_count();
	TrackData data_dt;
	data_dt.make_values_distance_over_time_helper(trk);
	assert (data_dt.n_points == (int) trk->get_tp_count());

	result.allocate_vector(tp_count);


	int i = 0;
	result.x[i] = data_dt.x[0];
	result.y[i] = 0.0;
	TRW_TRACK_DATA_CALCULATE_MIN_MAX(result, i, (!std::isnan(result.y[i])));
	i++;

	for (; i < tp_count; i++) {
		/* TODO: handle invalid distance values in data_dt. */

		if (data_dt.x[i] <= data_dt.x[i - 1]) {
			/* Handle glitch in values of consecutive time stamps.
			   TODO_LATER: improve code that calculates pseudo-values of result when a glitch has been found. */
			qDebug() << SG_PREFIX_W << "Glitch in timestamps:" << i << data_dt.x[i] << data_dt.x[i - 1];
			result.x[i] = data_dt.x[i - 1];
			result.y[i] = 0;

			TRW_TRACK_DATA_CALCULATE_MIN_MAX(result, i, false);

		} else {
			const double delta_t = (data_dt.x[i] - data_dt.x[i - 1]);
			const double delta_d = (data_dt.y[i] - data_dt.y[i - 1]);

			result.x[i] = data_dt.x[i];
			result.y[i] = delta_d / delta_t;

			TRW_TRACK_DATA_CALCULATE_MIN_MAX(result, i, true);
		}
	}

	result.valid = true;
	result.x_domain = GisViewportDomain::Time;
	result.y_domain = GisViewportDomain::Speed;
	result.y_speed_unit = SpeedUnit::MetresPerSecond;
	snprintf(result.debug, sizeof (result.debug), "Speed over Time");

	return result;
}




/**
   Make a distance/time map, heavily based on the Track::make_track_data_speed_over_time() method.

   @reviewed-on tbd
*/
TrackData TrackData::make_track_data_distance_over_time(Track * trk)
{
	TrackData result;

	const Time duration = trk->get_duration();
	if (!duration.is_valid() || duration.get_value() < 0) {
		qDebug() << SG_PREFIX_E << "Track duration is either invalid or negative";
		return result;
	}

	const int tp_count = trk->get_tp_count();
	TrackData data_dt;
	data_dt.make_values_distance_over_time_helper(trk);

	/* TODO: why do we need to generate 'result', if we already have 'data_dt'? */

	assert (data_dt.n_points == tp_count);

	result.allocate_vector(tp_count);

	int i = 0;
	result.x[i] = data_dt.x[i];
	result.y[i] = result.y[i];
	TRW_TRACK_DATA_CALCULATE_MIN_MAX(result, i, (!std::isnan(result.y[i])));
	i++;

	for (; i < data_dt.n_points; i++) {
		if (data_dt.x[i] <= data_dt.x[i - 1]) {
			/* Handle glitch in values of consecutive time stamps.
			   TODO_LATER: improve code that calculates pseudo-values of result when a glitch has been found. */
			qDebug() << SG_PREFIX_W << "Glitch in timestamps" << i << data_dt.x[i] << data_dt.x[i - 1];
			result.x[i] = data_dt.x[i - 1];
			result.y[i] = NAN;
			TRW_TRACK_DATA_CALCULATE_MIN_MAX(result, i, false);
		} else {
			result.x[i] = data_dt.x[i];
			result.y[i] = data_dt.y[i - 1];
			TRW_TRACK_DATA_CALCULATE_MIN_MAX(result, i, true);
		}
	}

	assert (i == tp_count);
	qDebug() << SG_PREFIX_D << "Filled" << i << "cells with altitudes";

	result.valid = true;
	result.x_domain = GisViewportDomain::Time;
	result.y_domain = GisViewportDomain::Distance;
	result.y_supplementary_distance_unit = SupplementaryDistanceUnit::Meters;
	snprintf(result.debug, sizeof (result.debug), "Distance over Time");

	return result;
}




/**
   This uses the 'time' based method to make the graph, (which is a simpler compared to the elevation/distance).
   This results in a slightly blocky graph when it does not have many trackpoints: <60.
   NB Somehow the elevation/distance applies some kind of smoothing algorithm,
   but I don't think any one understands it any more (I certainly don't ATM).

   @reviewed-on tbd
*/
TrackData TrackData::make_track_data_altitude_over_time(Track * trk)
{
	TrackData result;

	const Time duration = trk->get_duration();
	if (!duration.is_valid() || duration.get_value() < 0) {
		return result;
	}

	if (trk->trackpoints.size() < 2) {
		return result;
	}

	/* Test if there's anything worth calculating. */
	bool okay = false;
	for (auto iter = trk->trackpoints.begin(); iter != trk->trackpoints.end(); iter++) {
		if ((*iter)->altitude.is_valid()) {
			okay = true;
			break;
		}
	}
	if (!okay) {
		return result;
	}

	result.make_values_altitude_over_time_helper(trk);

	assert (result.n_points == (int) trk->get_tp_count());

	return result;
}




/**
   Make a speed/distance map.

   @reviewed-on tbd
*/
TrackData TrackData::make_track_data_speed_over_distance(Track * trk)
{
	TrackData result;

	double total_length = trk->get_length_value_including_gaps();
	if (total_length <= 0) {
		return result;
	}

	const int tp_count = trk->get_tp_count();
	TrackData data_dt;
	data_dt.make_values_distance_over_time_helper(trk);

	result.allocate_vector(tp_count);

	int i = 0;
	result.x[i] = 0;
	result.y[i] = 0;
	TRW_TRACK_DATA_CALCULATE_MIN_MAX(result, i, (!std::isnan(result.y[i])));
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
			TRW_TRACK_DATA_CALCULATE_MIN_MAX(result, i, true);
		}
	}

	assert(i == tp_count);

	result.valid = true;
	result.x_domain = GisViewportDomain::Distance;
	result.y_domain = GisViewportDomain::Speed;
	snprintf(result.debug, sizeof (result.debug), "Speed over Distance");

	return result;
}
