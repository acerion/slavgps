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




#include <cassert>




#include "layer_trw_track_data.h"




using namespace SlavGPS;




TrackData::TrackData()
{
}




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




TrackData TrackData::compress(int compressed_n_points) const
{
	TrackData compressed(compressed_n_points);

	this->do_compress(compressed);

	compressed.n_points = compressed_n_points;
	compressed.valid = true;

	return compressed;
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




sg_ret TrackData::y_distance_convert_meters_to(DistanceUnit new_distance_unit)
{
#if 0
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
#endif

	return sg_ret::ok;
}




sg_ret TrackData::y_speed_convert_mps_to(SpeedUnit new_speed_unit)
{
#if 0
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
#endif

	return sg_ret::ok;
}




QDebug operator<<(QDebug debug, const TrackData & track_data)
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
