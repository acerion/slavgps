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
#include <QFile>
#include <QString>




#include "compression.h"
#include "dem.h"
#include "coords.h"
#include "file_utils.h"
#include "util.h"
#include "vikutils.h"
#include "bbox.h"
#include "globals.h"




using namespace SlavGPS;




#define SG_MODULE "DEM"




#define DEM_BLOCK_SIZE 1024



const int16_t DEM::invalid_elevation = INT16_MIN;
static const int g_secs_per_degree = 60 * 60;




/**
   \reviewed 2019-01-28
*/
bool DEM24k::get_double_and_continue(char ** buffer, double * result, const char * msg)
{
	char * endptr;
	*result = strtod(*buffer, &endptr);
	if (endptr == NULL || endptr == *buffer) {
		if (msg) {
			qDebug() << SG_PREFIX_W << "Invalid data:" << msg;
		}
		return false;
	}
	*buffer = endptr;
	return true;
}




/**
   \reviewed 2019-01-28
*/
bool DEM24k::get_int_and_continue(char ** buffer, int * result, const char * msg)
{
	char * endptr;
	*result = strtol(*buffer, &endptr, 10);
	if (endptr == NULL || endptr == *buffer) {
		if (msg) {
			qDebug() << SG_PREFIX_W << "Invalid data:" << msg;
		}
		return false;
	}
	*buffer = endptr;
	return true;
}




/**
   \brief Fix Fortran-style exponentiation 1.0D5 -> 1.0E5

   \reviewed 2019-01-28
*/
void DEM24k::fix_exponentiation(char * buffer)
{
	while (*buffer) {
		if (*buffer == 'D') {
			*buffer = 'E';
		}
		buffer++;
	}
	return;
}





bool DEM24k::parse_header(char * buffer)
{
	if (strlen(buffer) != 1024) {
		/* Incomplete header. */
		return false;
	}

	DEM24k::fix_exponentiation(buffer);

	/* skip name */
	buffer += 149;

	int int_val;
	/* "DEM level code, pattern code, palaimetric reference system code" -- skip */
	DEM24k::get_int_and_continue(&buffer, &int_val, "dem level code");
	DEM24k::get_int_and_continue(&buffer, &int_val, "pattern code");
	DEM24k::get_int_and_continue(&buffer, &int_val, "palaimetric reference system code");

	/* zone */
	DEM24k::get_int_and_continue(&buffer, &int_val, "zone");
	this->utm.set_zone(int_val);
	/* FIXME: southern or northern hemisphere?! */
	this->utm.set_band_letter(UTMLetter::N);

	double val;
	/* skip numbers 5-19  */
	for (int i = 0; i < 15; i++) {
		if (!DEM24k::get_double_and_continue(&buffer, &val, "header")) {
			qDebug() << SG_PREFIX_W << "Invalid DEM header";
			return false;
		}
	}

	/* number 20 -- horizontal unit code (UTM/LatLon) */
	DEM24k::get_double_and_continue(&buffer, &val, "horizontal unit code"); /* TODO_LATER: do we really want to read double to get horiz unit? */
	this->horiz_units = (DEMHorizontalUnit) val; /* TODO_LATER: we should somehow validate if val has expected value (2 or 3). */

	DEM24k::get_double_and_continue(&buffer, &val, "orig vert units"); /* TODO_LATER: do we really want to read double to get vert. unit? */
	/* this->orig_vert_units = val; now done below */

	/* TODO_LATER: do this for real. these are only for 1:24k and 1:250k USGS */
	if (this->horiz_units == DEMHorizontalUnit::UTMMeters) {
		this->scale.x = 10.0; /* meters */
		this->scale.y = 10.0;
		this->orig_vert_units = DEMVerticalUnit::Decimeters;
	} else {
		this->scale.x = 3.0; /* arcseconds */
		this->scale.y = 3.0;
		this->orig_vert_units = DEMVerticalUnit::Meters;
	}

	/* skip next */
	DEM24k::get_double_and_continue(&buffer, &val, "skip 1");

	/* now we get the four corner points. record the min and max. */
	DEM24k::get_double_and_continue(&buffer, &val, "corner east");
	this->min_east_seconds = val;
	this->max_east_seconds = val;
	DEM24k::get_double_and_continue(&buffer, &val, "corner north");
	this->min_north_seconds = val;
	this->max_north_seconds = val;

	for (int i = 0; i < 3; i++) {
		DEM24k::get_double_and_continue(&buffer, &val, "east seconds");
		if (val < this->min_east_seconds) {
			this->min_east_seconds = val;
		}

		if (val > this->max_east_seconds) {
			this->max_east_seconds = val;
		}
		DEM24k::get_double_and_continue(&buffer, &val, "north seconds");
		if (val < this->min_north_seconds) {
			this->min_north_seconds = val;
		}
		if (val > this->max_north_seconds) {
			this->max_north_seconds = val;
		}
	}

	return true;
}





void DEM24k::parse_block_as_cont(char * buffer, int32_t * cur_column, int32_t * cur_row)
{
	int tmp;
	while (*cur_row < this->columns[*cur_column]->m_size) {
		if (DEM24k::get_int_and_continue(&buffer, &tmp, NULL)) {
			if (this->orig_vert_units == DEMVerticalUnit::Decimeters) {
				this->columns[*cur_column]->m_points[*cur_row] = (int16_t) (tmp / 10);
			} else {
				this->columns[*cur_column]->m_points[*cur_row] = (int16_t) tmp;
			}
		} else {
			return;
		}
		(*cur_row)++;
	}
	*cur_row = -1; /* expecting new column */
}





void DEM24k::parse_block_as_header(char * buffer, int32_t * cur_column, int32_t * cur_row)
{
	/* 1 x n_rows 1 east_west south x x x DATA */

	double tmp;
	if ((!DEM24k::get_double_and_continue(&buffer, &tmp, "header 2")) || tmp != 1) {
		qDebug() << SG_PREFIX_W << "Parse Block: Incorrect DEM Class B record: expected 1";
		return;
	}

	/* don't need this */
	if (!DEM24k::get_double_and_continue(&buffer, &tmp, "skip 2")) {
		return;
	}

	/* n_rows */
	if (!DEM24k::get_double_and_continue(&buffer, &tmp, "n_rows")) {
		return;
	}
	int32_t n_rows = (int32_t) tmp;

	if ((!DEM24k::get_double_and_continue(&buffer, &tmp, "header 3")) || tmp != 1) {
		qDebug() << SG_PREFIX_W << "Parse Block: Incorrect DEM Class B record: expected 1";
		return;
	}

	double east_west;
	if (!DEM24k::get_double_and_continue(&buffer, &east_west, "east west")) {
		return;
	}

	double south;
	if (!DEM24k::get_double_and_continue(&buffer, &south, "south")) {
		return;
	}

	/* next three things we don't need */
	if (!DEM24k::get_double_and_continue(&buffer, &tmp, "skip 3")) {
		return;
	}

	if (!DEM24k::get_double_and_continue(&buffer, &tmp, "skip 4")) {
		return;
	}

	if (!DEM24k::get_double_and_continue(&buffer, &tmp, "skip 5")) {
		return;
	}


	this->n_columns++;
	(*cur_column)++;

	/* empty spaces for things before that were skipped */
	(*cur_row) = (south - this->min_north_seconds) / this->scale.y;
	if (south > this->max_north_seconds || (*cur_row) < 0) {
		(*cur_row) = 0;
	}

	n_rows += *cur_row;


	/* FIXME: make sure that array index is valid. */
	this->columns[*cur_column] = new DEMColumn(east_west, south, n_rows);


	/* no information for things before that */
	for (int i = 0; i < (*cur_row); i++) {
		this->columns[*cur_column]->m_points[i] = DEM::invalid_elevation;
	}

	/* now just continue */
	this->parse_block_as_cont(buffer, cur_column, cur_row);
}





void DEM24k::parse_block(char * buffer, int32_t * cur_column, int32_t * cur_row)
{
	/* if haven't read anything or have read all items in a columns and are expecting a new column */
	if (*cur_column == -1 || *cur_row == -1) {
		this->parse_block_as_header(buffer, cur_column, cur_row);
	} else {
		this->parse_block_as_cont(buffer, cur_column, cur_row);
	}
}




/**
   \reviewed 2019-01-28
*/
sg_ret DEMSRTM::read_from_file(const QString & file_full_path)
{
	if (access(file_full_path.toUtf8().constData(), R_OK) != 0) {
		return sg_ret::err;
	}
	const bool is_zip = file_full_path.endsWith(".zip");

	const QString file_name = file_base_name(file_full_path);
	/*
	  S01E006.hgt.zip
	  S11E119.hgt.zip
	  S12E096.hgt.zip
	  S22W136.hgt.zip
	  N00E072.hgt.zip
	  N41E056.hgt.zip
	*/

	if (file_name.length() < (int) strlen("S01E006")) {
		qDebug() << SG_PREFIX_E << "Invalid file name" << file_name;
		return sg_ret::err;
	}


	const int32_t num_rows_3sec = 1201;
	const int32_t num_rows_1sec = 3601;

	this->horiz_units = DEMHorizontalUnit::LatLonArcSeconds;
	this->orig_vert_units = DEMVerticalUnit::Decimeters;


	bool ok;
	this->min_north_seconds = file_name.midRef(1, 2).toInt(&ok) * g_secs_per_degree;
	if (!ok) {
		qDebug() << SG_PREFIX_E << "Failed to convert northing value";
		return sg_ret::err;
	}
	if (file_name[0] == 'S') {
		this->min_north_seconds = -this->min_north_seconds;
	}


	this->min_east_seconds = file_name.midRef(4, 3).toInt(&ok) * g_secs_per_degree;
	if (!ok) {
		qDebug() << SG_PREFIX_E << "Failed to convert easting value";
		return sg_ret::err;
	}
	if (file_name[3] == 'W') {
		this->min_east_seconds = -this->min_east_seconds;
	}


	this->max_north_seconds = 3600 + this->min_north_seconds;
	this->max_east_seconds = 3600 + this->min_east_seconds;


	this->n_columns = 0;


	QFile file(file_full_path);
	if (!file.open(QIODevice::ReadOnly)) {
		qDebug() << SG_PREFIX_E << "Can't open file" << file_full_path << file.error();
		return sg_ret::err;
	}

	off_t file_size = file.size();
	unsigned char * file_contents = file.map(0, file_size, QFileDevice::MapPrivateOption);
	if (!file_contents) {
		qDebug() << SG_PREFIX_E << "Can't map file" << file_full_path << file.error();
		return sg_ret::err;
	}

	int16_t * dem_contents = NULL;
	size_t dem_size = 0;
	if (is_zip) {
		void * uncompressed_contents = NULL;
		size_t uncompressed_size = 0;

		if ((uncompressed_contents = unzip_file((char *) file_contents, &uncompressed_size)) == NULL) {
			file.unmap(file_contents);
			file.close();
			return sg_ret::err;
		}

		dem_contents = (int16_t *) uncompressed_contents;
		dem_size = uncompressed_size;
	} else {
		dem_contents = (int16_t *) file_contents;
		dem_size = file_size;
	}


	/* Calculate which dataset are we dealing with: 1-arc-second
	   or 3-arc-second.  For freshly downloaded files user can
	   specify in UI which file format (1 or 3 arc) will be used,
	   but for files added "manually" in layer's config it can be
	   both ways. So we can't rely solely on config in UI, we have
	   to discover the file format here. */
	int arcsec;
	if (dem_size == (num_rows_3sec * num_rows_3sec * sizeof (int16_t))) {
		arcsec = 3;
	} else if (dem_size == (num_rows_1sec * num_rows_1sec * sizeof (int16_t))) {
		arcsec = 1;
	} else {
		qDebug() << SG_PREFIX_W << "File" << file_name << "does not have right size, dem size = " << dem_size;
		file.unmap(file_contents);
		file.close();
		return sg_ret::err;
	}

	const int32_t num_rows = (arcsec == 3) ? num_rows_3sec : num_rows_1sec;
	const int32_t num_cols = num_rows;
	this->scale.x = arcsec;
	this->scale.y = arcsec;

	this->columns.reserve(num_cols);
	for (int col = 0; col < num_cols; col++) {
		this->n_columns++;
		DEMColumn * new_column = new DEMColumn(this->min_east_seconds + arcsec * col, this->min_north_seconds, num_rows);
		this->columns.push_back(new_column);
	}

	int32_t point = 0;
	for (int32_t row = (num_rows - 1); row >= 0; row--) {
		for (int32_t col = 0; col < num_cols; col++) {
			this->columns[col]->m_points[row] = be16toh(dem_contents[point]);
			point++;
		}
	}

	if (is_zip) {
		free(dem_contents);
	}
	file.unmap(file_contents);
	file.close();
	return sg_ret::ok;
}




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




sg_ret DEM24k::read_from_file(const QString & file_full_path)
{
	/* Header */
	FILE * file = fopen(file_full_path.toUtf8().constData(), "r");
	if (nullptr == file) {
		return sg_ret::err;
	}

	char buffer[DEM_BLOCK_SIZE + 1];
	buffer[fread(buffer, 1, DEM_BLOCK_SIZE, file)] = '\0';
	if (!this->parse_header(buffer)) {
		fclose(file);
		return sg_ret::err;
	}
	/* TODO_LATER: actually use header -- i.e. GET # OF COLUMNS EXPECTED */

	this->n_columns = 0;
	/* Use the two variables to record state for ->parse_block(). */
	int32_t cur_column = -1;
	int32_t cur_row = -1;

	/* Column -- Data */
	while (!feof(file)) {
		/* read block */
		buffer[fread(buffer, 1, DEM_BLOCK_SIZE, file)] = '\0';

		DEM24k::fix_exponentiation(buffer);

		this->parse_block(buffer, &cur_column, &cur_row);
	}

	/* TODO_LATER - class C records (right now says 'Invalid' and dies) */

	fclose(file);
	file = NULL;

	/* 24k scale */
	if (this->horiz_units == DEMHorizontalUnit::UTMMeters && this->n_columns >= 2) {
		this->scale.x = this->columns[1]->m_east - this->columns[0]->m_east;
		this->scale.y = this->scale.x;

	}

	/* FIXME bug in 10m DEM's */
	if (this->horiz_units == DEMHorizontalUnit::UTMMeters && this->scale.y == 10) {
		this->min_east_seconds -= 100;
		this->min_north_seconds += 200;
	}

	return sg_ret::ok;
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
