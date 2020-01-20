/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2008, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2016-2020, Kamil Ignacak <acerion@wp.pl>
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




#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <cassert>




#include <sys/types.h>
#include <sys/stat.h>
#include <endian.h>
#include <unistd.h>




#include <QDebug>
#include <QString>




#include "bbox.h"
#include "coords.h"
#include "layer_dem_dem.h"
#include "file_utils.h"
#include "globals.h"
#include "util.h"
#include "vikutils.h"




using namespace SlavGPS;




#define SG_MODULE "DEM"




const int16_t DEM::invalid_elevation = INT16_MIN;




/**
   \reviewed 2020-01-18
*/
DEMSource DEM::recognize_source_type(const QString & file_full_path)
{
	const QString file_name = file_base_name(file_full_path);

	const size_t len = file_name.size();

	if (strlen("S01E006.hgt") != len && strlen("S01E006.hgt.zip") != len) {
		return DEMSource::Unknown;
	}
	if (file_name[0] != 'N' && file_name[0] != 'S') {
		return DEMSource::Unknown;
	}
	if (file_name[3] != 'E' && file_name[3] != 'W') {
		return DEMSource::Unknown;
	}
	if (strlen("S01E006.hgt") == len && file_name.right(4) == ".hgt") {
		return DEMSource::SRTM;
	}
	if (strlen("S01E006.hgt.zip") == len && file_name.right(8) == ".hgt.zip") {
		return DEMSource::SRTM;
	}
	return DEMSource::Unknown;
}




DEM::~DEM()
{
	for (int32_t i = 0; i < this->n_columns; i++) {
		delete this->columns[i];
	}
}




/**
   \reviewed 2019-01-29
*/
int16_t DEM::get_elev_at_col_row(int32_t col, int32_t row) const
{
	if (col < this->n_columns) {
		if (row < this->columns[col]->m_size) {
			return this->columns[col]->m_points[row];
		}
	}
	return DEM::invalid_elevation;
}




int16_t DEM::get_elev_at_east_north_no_interpolation(double east_seconds, double north_seconds) const
{
	if (east_seconds > this->max_east_seconds
	    || east_seconds < this->min_east_seconds
	    || north_seconds > this->max_north_seconds
	    || north_seconds < this->min_north_seconds) {

		return DEM::invalid_elevation;
	}

	const int32_t col = (int32_t) floor((east_seconds - this->min_east_seconds) / this->scale.x);
	const int32_t row = (int32_t) floor((north_seconds - this->min_north_seconds) / this->scale.y);

	return this->get_elev_at_col_row(col, row);
}




bool DEM::get_ref_points_elevation_distance(double east_seconds, double north_seconds, int16_t * elevations, int16_t * distances) const
{
	int32_t cols[4];
	int32_t rows[4];
	LatLon lat_lon[4];

	if (east_seconds > this->max_east_seconds
	    || east_seconds < this->min_east_seconds
	    || north_seconds > this->max_north_seconds
	    || north_seconds < this->min_north_seconds) {

		return false;  /* Got nothing. */
	}

	const LatLon pos(north_seconds / 3600, east_seconds / 3600);

	/* order of the data: sw, nw, ne, se */
	/* sw */
	cols[0] = (int32_t) floor((east_seconds - this->min_east_seconds) / this->scale.x);
	rows[0] = (int32_t) floor((north_seconds - this->min_north_seconds) / this->scale.y);
	lat_lon[0].lon = (this->min_east_seconds + this->scale.x * cols[0]) / 3600;
	lat_lon[0].lat = (this->min_north_seconds + this->scale.y * rows[0]) / 3600;
	/* nw */
	cols[1] = cols[0];
	rows[1] = rows[0] + 1;
	lat_lon[1].lon = lat_lon[0].lon;
	lat_lon[1].lat = lat_lon[0].lat + this->scale.y / 3600.0;
	/* ne */
	cols[2] = cols[0] + 1;
	rows[2] = rows[0] + 1;
	lat_lon[2].lon = lat_lon[0].lon + this->scale.x / 3600.0;
	lat_lon[2].lat = lat_lon[0].lat + this->scale.y / 3600.0;
	/* se */
	cols[3] = cols[0] + 1;
	rows[3] = rows[0];
	lat_lon[3].lon = lat_lon[0].lon + this->scale.x / 3600.0;
	lat_lon[3].lat = lat_lon[0].lat;

	for (int i = 0; i < 4; i++) {
		elevations[i] = this->get_elev_at_col_row(cols[i], rows[i]);
		if (elevations[i] == DEM::invalid_elevation) {
			return false;
		}
		distances[i] = LatLon::get_distance(pos, lat_lon[i]);
	}

#if 0   /* Debug. */
	for (int i = 0; i < 4; i++) {
		qDebug() << SG_PREFIX_D << i << ":" << lat_lon[i].lat << lat_lon[i].lon << distances[i] << elevs[i];
	}
	qDebug() << SG_PREFIX_D << "north_scale =" this->north_scale;
#endif

	return true;  /* All OK. */
}




int16_t DEM::get_elev_at_east_north_simple_interpolation(double east_seconds, double north_seconds) const
{
	int16_t elevations[4] = { 0 };
	int16_t distances[4] = { 0 };
	if (!this->get_ref_points_elevation_distance(east_seconds, north_seconds, elevations, distances)) {
		return DEM::invalid_elevation;
	}

	for (int i = 0; i < 4; i++) {
		if (distances[i] < 1) {
			return elevations[i];
		}
	}

	const double t =
		(double) elevations[0] / distances[0]
		+ (double) elevations[1] / distances[1]
		+ (double) elevations[2] / distances[2]
		+ (double) elevations[3] / distances[3];

	const double b =
		1.0 / distances[0]
		+ 1.0 / distances[1]
		+ 1.0 / distances[2]
		+ 1.0 / distances[3];

	return (t/b);
}




int16_t DEM::get_elev_at_east_north_shepard_interpolation(double east_seconds, double north_seconds) const
{
	int16_t elevations[4] = { 0 };
	int16_t distances[4] = { 0 };
	if (!this->get_ref_points_elevation_distance(east_seconds, north_seconds, elevations, distances)) {
		return DEM::invalid_elevation;
	}

	int16_t max_dist = 0;
	for (int i = 0; i < 4; i++) {
		if (distances[i] < 1) {
			return elevations[i];
		}
		if (distances[i] > max_dist) {
			max_dist = distances[i];
		}
	}

	double t = 0.0;
	double b = 0.0;

#ifdef TODO_MAYBE
	/* Derived method by Franke & Nielson. Does not seem to work too well here. */
	for (int i = 0; i < 4; i++) {
		const double tmp = pow((1.0 * (max_dist - distances[i]) / max_dist * distances[i]), 2);
		t += tmp * elevations[i];
		b += tmp;
	}
#endif

	for (int i = 0; i < 4; i++) {
		const double tmp = pow((1.0 / distances[i]), 2);
		t += tmp * elevations[i];
		b += tmp;
	}

	qDebug() << SG_PREFIX_D << "Shepard Interpolation: t =" << t << "b =" << b << "t/b =" << t/b;

	return (t/b);

}




void DEM::east_north_to_col_row(double east_seconds, double north_seconds, int32_t * col, int32_t * row) const
{
	*col = (int32_t) floor((east_seconds - this->min_east_seconds) / this->scale.x);
	*row = (int32_t) floor((north_seconds - this->min_north_seconds) / this->scale.y);
}




bool DEM::intersect(const LatLonBBox & other_bbox) const
{
	LatLon dem_northeast;
	LatLon dem_southwest;

	/* Get min/max lat/lon of DEM data. */
	if (this->horiz_units == DEMHorizontalUnit::LatLonArcSeconds) {
		dem_northeast.lat = this->max_north_seconds / 3600.0;
		dem_northeast.lon = this->max_east_seconds / 3600.0;
		dem_southwest.lat = this->min_north_seconds / 3600.0;
		dem_southwest.lon = this->min_east_seconds / 3600.0;

	} else if (this->horiz_units == DEMHorizontalUnit::UTMMeters) {
		/* TODO_LATER: add smarter error handling of invalid
		   band letter. In theory the source object should be
		   valid and for sure contain valid band letter. */

		const UTM dem_northeast_utm(this->max_north_seconds, this->max_east_seconds, this->utm.get_zone(), this->utm.get_band_letter());
		const UTM dem_southwest_utm(this->min_north_seconds, this->min_east_seconds, this->utm.get_zone(), this->utm.get_band_letter());

		dem_northeast = UTM::to_lat_lon(dem_northeast_utm);
		dem_southwest = UTM::to_lat_lon(dem_southwest_utm);
	} else {
		/* Unknown horiz_units - this shouldn't normally happen.
		   Thus can't work out positions to use. */
		return false;
	}

	LatLonBBox bbox;
	bbox.north = dem_northeast.lat;
	bbox.south = dem_southwest.lat;
	bbox.east = dem_northeast.lon;
	bbox.west = dem_southwest.lon;
	bbox.validate();

	const bool result = bbox.intersects_with(other_bbox);

	qDebug() << SG_PREFIX_I << "DEM's bbox:" << bbox;
	qDebug() << SG_PREFIX_I << "Other bbox:" << other_bbox;
	qDebug() << SG_PREFIX_I << "Intersect: " << (result ? "true" : "false");

	return result;
}




DEMColumn::DEMColumn(double east, double south, int32_t size)
{
	this->m_east = east;
	this->m_south = south;
	this->m_size = size;

	this->m_points = (int16_t *) malloc(sizeof (int16_t) * this->m_size);
}




DEMColumn::~DEMColumn()
{
	free(this->m_points);
	this->m_points = nullptr;
}





sg_ret DEM::get_elev_by_coord(const Coord & coord, DEMInterpolation method, int16_t & elev) const
{
	double lat = 0.0;
	double lon = 0.0;

	if (this->horiz_units == DEMHorizontalUnit::LatLonArcSeconds) {
		const LatLon searched_lat_lon = coord.get_lat_lon();
		lat = searched_lat_lon.lat * 3600;
		lon = searched_lat_lon.lon * 3600;
	} else if (this->horiz_units == DEMHorizontalUnit::UTMMeters) {
		const UTM searched_utm = coord.get_utm();
		if (!UTM::is_the_same_zone(searched_utm, this->utm)) {
			elev = DEM::invalid_elevation;
			return sg_ret::ok;
		}
		lat = searched_utm.get_northing();
		lon = searched_utm.get_easting();
	} else {
		qDebug() << SG_PREFIX_E << "Unexpected horizontal unit" << (int) this->horiz_units;
		elev = DEM::invalid_elevation;
		return sg_ret::err;
	}

	switch (method) {
	case DEMInterpolation::None:
		elev = this->get_elev_at_east_north_no_interpolation(lon, lat);
		return sg_ret::ok;

	case DEMInterpolation::Simple:
		elev = this->get_elev_at_east_north_simple_interpolation(lon, lat);
		return sg_ret::ok;

	case DEMInterpolation::Best:
		elev = this->get_elev_at_east_north_shepard_interpolation(lon, lat);
		return sg_ret::ok;

	default:
		qDebug() << SG_PREFIX_E << "Unexpected interpolation method" << (int) method;
		elev = DEM::invalid_elevation;
		return sg_ret::err;
	}
}
