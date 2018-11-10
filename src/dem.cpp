/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2008, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
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

#if HAVE_UNISTD_H
#include <unistd.h>
#endif




#include <QDebug>
#include <QFile>
#include <QString>




#include <glib.h>




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




static const int g_secs_per_degree = 60 * 60;





static bool get_double_and_continue(char ** buffer, double * tmp, bool warn)
{
	char * endptr;
	*tmp = strtod(*buffer, &endptr);
	if (endptr == NULL || endptr == *buffer) {
		if (warn) {
			qDebug() << SG_PREFIX_W << "Invalid DEM";
		}
		return false;
	}
	*buffer = endptr;
	return true;
}





static bool get_int_and_continue(char ** buffer, int * tmp, bool warn)
{
	char * endptr;
	*tmp = strtol(*buffer, &endptr, 10);
	if (endptr == NULL || endptr == *buffer) {
		if (warn) {
			qDebug() << SG_PREFIX_W << "Invalid DEM";
		}
		return false;
	}
	*buffer = endptr;
	return true;
}





/* Fix Fortran-style exponentiation 1.0D5 -> 1.0E5. */
static void fix_exponentiation(char * buffer)
{
	while (*buffer) {
		if (*buffer == 'D') {
			*buffer = 'E';
		}
		buffer++;
	}
	return;
}





bool DEM::parse_header(char * buffer)
{
	if (strlen(buffer) != 1024) {
		/* Incomplete header. */
		return false;
	}

	fix_exponentiation(buffer);

	/* skip name */
	buffer += 149;

	int int_val;
	/* "DEM level code, pattern code, palaimetric reference system code" -- skip */
	get_int_and_continue(&buffer, &int_val, true);
	get_int_and_continue(&buffer, &int_val, true);
	get_int_and_continue(&buffer, &int_val, true);

	/* zone */
	get_int_and_continue(&buffer, &int_val, true);
	this->utm.zone = int_val;
	/* FIXME: southern or northern hemisphere?! */
	this->utm.set_band_letter('N');

	double val;
	/* skip numbers 5-19  */
	for (int i = 0; i < 15; i++) {
		if (!get_double_and_continue(&buffer, &val, false)) {
			qDebug() << SG_PREFIX_W << "Invalid DEM header";
			return false;
		}
	}

	/* number 20 -- horizontal unit code (utm/ll) */
	get_double_and_continue(&buffer, &val, true);
	this->horiz_units = val;
	get_double_and_continue(&buffer, &val, true);
	/* this->orig_vert_units = val; now done below */

	/* TODO_2_LATER: do this for real. these are only for 1:24k and 1:250k USGS */
	if (this->horiz_units == VIK_DEM_HORIZ_UTM_METERS) {
		this->east_scale = 10.0; /* meters */
		this->north_scale = 10.0;
		this->orig_vert_units = VIK_DEM_VERT_DECIMETERS;
	} else {
		this->east_scale = 3.0; /* arcseconds */
		this->north_scale = 3.0;
		this->orig_vert_units = VIK_DEM_VERT_METERS;
	}

	/* skip next */
	get_double_and_continue(&buffer, &val, true);

	/* now we get the four corner points. record the min and max. */
	get_double_and_continue(&buffer, &val, true);
	this->min_east_seconds = val;
	this->max_east_seconds = val;
	get_double_and_continue(&buffer, &val, true);
	this->min_north_seconds = val;
	this->max_north_seconds = val;

	for (int i = 0; i < 3; i++) {
		get_double_and_continue(&buffer, &val, true);
		if (val < this->min_east_seconds) {
			this->min_east_seconds = val;
		}

		if (val > this->max_east_seconds) {
			this->max_east_seconds = val;
		}
		get_double_and_continue(&buffer, &val, true);
		if (val < this->min_north_seconds) {
			this->min_north_seconds = val;
		}
		if (val > this->max_north_seconds) {
			this->max_north_seconds = val;
		}
	}

	return true;
}





void DEM::parse_block_as_cont(char * buffer, int32_t * cur_column, int32_t * cur_row)
{
	int tmp;
	while (*cur_row < this->columns[*cur_column]->n_points) {
		if (get_int_and_continue(&buffer, &tmp, false)) {
			if (this->orig_vert_units == VIK_DEM_VERT_DECIMETERS) {
				this->columns[*cur_column]->points[*cur_row] = (int16_t) (tmp / 10);
			} else {
				this->columns[*cur_column]->points[*cur_row] = (int16_t) tmp;
			}
		} else {
			return;
		}
		(*cur_row)++;
	}
	*cur_row = -1; /* expecting new column */
}





void DEM::parse_block_as_header(char * buffer, int32_t * cur_column, int32_t * cur_row)
{
	/* 1 x n_rows 1 east_west south x x x DATA */

	double tmp;
	if ((!get_double_and_continue(&buffer, &tmp, true)) || tmp != 1) {
		qDebug() << SG_PREFIX_W << "Parse Block: Incorrect DEM Class B record: expected 1";
		return;
	}

	/* don't need this */
	if (!get_double_and_continue(&buffer, &tmp, true)) {
		return;
	}

	/* n_rows */
	if (!get_double_and_continue(&buffer, &tmp, true)) {
		return;
	}
	int32_t n_rows = (int32_t) tmp;

	if ((!get_double_and_continue(&buffer, &tmp, true)) || tmp != 1) {
		qDebug() << SG_PREFIX_W << "Parse Block: Incorrect DEM Class B record: expected 1";
		return;
	}

	double east_west;
	if (!get_double_and_continue(&buffer, &east_west, true)) {
		return;
	}

	double south;
	if (!get_double_and_continue(&buffer, &south, true)) {
		return;
	}

	/* next three things we don't need */
	if (!get_double_and_continue(&buffer, &tmp, true)) {
		return;
	}

	if (!get_double_and_continue(&buffer, &tmp, true)) {
		return;
	}

	if (!get_double_and_continue(&buffer, &tmp, true)) {
		return;
	}


	this->n_columns++;
	(*cur_column)++;

	/* empty spaces for things before that were skipped */
	(*cur_row) = (south - this->min_north_seconds) / this->north_scale;
	if (south > this->max_north_seconds || (*cur_row) < 0) {
		(*cur_row) = 0;
	}

	n_rows += *cur_row;


	/* FIXME: make sure that array index is valid. */
	this->columns[*cur_column] = new DEMColumn(east_west, south, n_rows);


	/* no information for things before that */
	for (int i = 0; i < (*cur_row); i++) {
		this->columns[*cur_column]->points[i] = DEM_INVALID_ELEVATION;
	}

	/* now just continue */
	this->parse_block_as_cont(buffer, cur_column, cur_row);
}





void DEM::parse_block(char * buffer, int32_t * cur_column, int32_t * cur_row)
{
	/* if haven't read anything or have read all items in a columns and are expecting a new column */
	if (*cur_column == -1 || *cur_row == -1) {
		this->parse_block_as_header(buffer, cur_column, cur_row);
	} else {
		this->parse_block_as_cont(buffer, cur_column, cur_row);
	}
}




bool DEM::read_srtm_hgt(const QString & file_full_path, const QString & file_name, bool zip)
{
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
		return false;
	}


	const int num_rows_3sec = 1201;
	const int num_rows_1sec = 3601;

	this->horiz_units = VIK_DEM_HORIZ_LL_ARCSECONDS;
	this->orig_vert_units = VIK_DEM_VERT_DECIMETERS;


	bool ok;
	this->min_north_seconds = file_name.midRef(1, 2).toInt(&ok) * g_secs_per_degree;
	if (!ok) {
		qDebug() << SG_PREFIX_E << "Failed to convert northing value";
		return false;
	}
	if (file_name[0] == 'S') {
		this->min_north_seconds = -this->min_north_seconds;
	}


	this->min_east_seconds = file_name.midRef(4, 3).toInt(&ok) * g_secs_per_degree;
	if (!ok) {
		qDebug() << SG_PREFIX_E << "Failed to convert easting value";
		return false;
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
		return false;
	}

	off_t file_size = file.size();
	unsigned char * file_contents = file.map(0, file_size, QFileDevice::MapPrivateOption);
	if (!file_contents) {
		qDebug() << SG_PREFIX_E << "Can't map file" << file_full_path << file.error();
		return false;
	}

	int16_t * dem_contents = NULL;
	size_t dem_size = 0;
	if (zip) {
		void * uncompressed_contents = NULL;
		size_t uncompressed_size = 0;

		if ((uncompressed_contents = unzip_file((char *) file_contents, &uncompressed_size)) == NULL) {
			file.unmap(file_contents);
			file.close();
			return false;
		}

		dem_contents = (int16_t *) uncompressed_contents;
		dem_size = uncompressed_size;
	} else {
		dem_contents = (int16_t *) file_contents;
		dem_size = file_size;
	}

	int arcsec;
	if (dem_size == (num_rows_3sec * num_rows_3sec * sizeof (int16_t))) {
		arcsec = 3;
	} else if (dem_size == (num_rows_1sec * num_rows_1sec * sizeof (int16_t))) {
		arcsec = 1;
	} else {
		qDebug() << SG_PREFIX_W << "File" << file_name << "does not have right size";
		file.unmap(file_contents);
		file.close();
		return false;
	}

	int num_rows = (arcsec == 3) ? num_rows_3sec : num_rows_1sec;
	this->east_scale = this->north_scale = arcsec;

	for (int i = 0; i < num_rows; i++) {
		this->n_columns++;
		DEMColumn * new_column = new DEMColumn(this->min_east_seconds + arcsec * i, this->min_north_seconds, num_rows);
		this->columns.push_back(new_column);
	}

	int ent = 0;
	for (int i = (num_rows - 1); i >= 0; i--) {
		for (int j = 0; j < num_rows; j++) {
			this->columns[j]->points[i] = GINT16_FROM_BE(dem_contents[ent]);
			ent++;
		}
	}

	if (zip) {
		free(dem_contents);
	}
	file.unmap(file_contents);
	file.close();
	return true;
}




static bool is_strm_hgt(const QString & file_name)
{
	size_t len = file_name.size();
	return (len == 11 || ((len == 15) && (file_name[11] == '.' && file_name[12] == 'z' && file_name[13] == 'i' && file_name[14] == 'p')))
		&& file_name[7] == '.' && file_name[8] == 'h' && file_name[9] == 'g' && file_name[10] == 't'
		&& (file_name[0] == 'N' || file_name[0] == 'S')
		&& (file_name[3] == 'E' || file_name[3] == 'W');
}





static bool is_zip_file(const QString & file_name)
{
	int len = file_name.size();
	return len == 15 && file_name[len - 3] == 'z' && file_name[len - 2] == 'i' && file_name[len - 1] == 'p';
}





bool DEM::read(const QString & file_full_path)
{
	if (access(file_full_path.toUtf8().constData(), R_OK) != 0) {
		return false;
	}

	const QString & file_name = file_base_name(file_full_path);

	if (is_strm_hgt(file_name)) {
		return this->read_srtm_hgt(file_full_path, file_name, is_zip_file(file_name));
	} else {
		return this->read_other(file_full_path);
	}
}





bool DEM::read_other(const QString & full_path)
{
	/* Header */
	FILE * file = fopen(full_path.toUtf8().constData(), "r");
	if (!file) {
		return false;
	}

	char buffer[DEM_BLOCK_SIZE + 1];
	buffer[fread(buffer, 1, DEM_BLOCK_SIZE, file)] = '\0';
	if (!this->parse_header(buffer)) {
		fclose(file);
		return false;
	}
	/* TODO_2_LATER: actually use header -- i.e. GET # OF COLUMNS EXPECTED */

	this->n_columns = 0;
	/* Use the two variables to record state for ->parse_block(). */
	int32_t cur_column = -1;
	int32_t cur_row = -1;

	/* Column -- Data */
	while (!feof(file)) {
		/* read block */
		buffer[fread(buffer, 1, DEM_BLOCK_SIZE, file)] = '\0';

		fix_exponentiation(buffer);

		this->parse_block(buffer, &cur_column, &cur_row);
	}

	/* TODO_2_LATER - class C records (right now says 'Invalid' and dies) */

	fclose(file);
	file = NULL;

	/* 24k scale */
	if (this->horiz_units == VIK_DEM_HORIZ_UTM_METERS && this->n_columns >= 2) {
		this->east_scale = this->columns[1]->east_west - this->columns[0]->east_west;
		this->north_scale = this->east_scale;

	}

	/* FIXME bug in 10m DEM's */
	if (this->horiz_units == VIK_DEM_HORIZ_UTM_METERS && this->north_scale == 10) {
		this->min_east_seconds -= 100;
		this->min_north_seconds += 200;
	}

	return true;
}





DEM::~DEM()
{
	for (int32_t i = 0; i < this->n_columns; i++) {
		delete this->columns[i];
	}
}





int16_t DEM::get_xy(int32_t col, int32_t row)
{
	if (col < this->n_columns) {
		if (row < this->columns[col]->n_points) {
			return this->columns[col]->points[row];
		}
	}
	return DEM_INVALID_ELEVATION;
}





int16_t DEM::get_east_north(double east, double north)
{
	if (east > this->max_east_seconds || east < this->min_east_seconds ||
	    north > this->max_north_seconds || north < this->min_north_seconds) {

		return DEM_INVALID_ELEVATION;
	}

	int col = (int) floor((east - this->min_east_seconds) / this->east_scale);
	int row = (int) floor((north - this->min_north_seconds) / this->north_scale);

	return this->get_xy(col, row);
}





bool DEM::get_ref_points_elev_dist(double east, double north, /* in seconds */
				   int16_t * elevs, int16_t * dists)
{
	int cols[4], rows[4];
	LatLon ll[4];

	if (east > this->max_east_seconds || east < this->min_east_seconds ||
	    north > this->max_north_seconds || north < this->min_north_seconds) {

		return false;  /* got nothing */
	}

	const LatLon pos(north / 3600, east / 3600);

	/* order of the data: sw, nw, ne, se */
	/* sw */
	cols[0] = (int) floor((east - this->min_east_seconds) / this->east_scale);
	rows[0] = (int) floor((north - this->min_north_seconds) / this->north_scale);
	ll[0].lon = (this->min_east_seconds + this->east_scale*cols[0]) / 3600;
	ll[0].lat = (this->min_north_seconds + this->north_scale*rows[0]) / 3600;
	/* nw */
	cols[1] = cols[0];
	rows[1] = rows[0] + 1;
	ll[1].lon = ll[0].lon;
	ll[1].lat = ll[0].lat + (double) this->north_scale / 3600;
	/* ne */
	cols[2] = cols[0] + 1;
	rows[2] = rows[0] + 1;
	ll[2].lon = ll[0].lon + (double) this->east_scale / 3600;
	ll[2].lat = ll[0].lat + (double) this->north_scale / 3600;
	/* se */
	cols[3] = cols[0] + 1;
	rows[3] = rows[0];
	ll[3].lon = ll[0].lon + (double) this->east_scale / 3600;
	ll[3].lat = ll[0].lat;

	for (int i = 0; i < 4; i++) {
		if ((elevs[i] = this->get_xy(cols[i], rows[i])) == DEM_INVALID_ELEVATION) {
			return false;
		}
		dists[i] = LatLon::latlon_diff(pos, ll[i]);
	}

#if 0   /* Debug. */
	for (int i = 0; i < 4; i++) {
		qDebug() << SG_PREFIX_D << i << ":" << ll[i].lat << ll[i].lon << dists[i] << elevs[i];
	}
	qDebug() << SG_PREFIX_D << "north_scale =" this->north_scale;
#endif

	return true;  /* all OK */
}





int16_t DEM::get_simple_interpol(double east, double north)
{
	int16_t elevs[4], dists[4];
	if (!this->get_ref_points_elev_dist(east, north, elevs, dists)) {
		return DEM_INVALID_ELEVATION;
	}

	for (int i = 0; i < 4; i++) {
		if (dists[i] < 1) {
			return elevs[i];
		}
	}

	double t = (double) elevs[0] / dists[0] + (double) elevs[1] / dists[1] + (double) elevs[2] / dists[2] + (double) elevs[3] / dists[3];
	double b = 1.0 / dists[0] + 1.0 / dists[1] + 1.0 / dists[2] + 1.0 / dists[3];

	return (t/b);
}





int16_t DEM::get_shepard_interpol(double east, double north)
{
	int16_t elevs[4], dists[4];
	if (!this->get_ref_points_elev_dist(east, north, elevs, dists)) {
		return DEM_INVALID_ELEVATION;
	}

	int16_t max_dist = 0;
	for (int i = 0; i < 4; i++) {
		if (dists[i] < 1) {
			return elevs[i];
		}
		if (dists[i] > max_dist) {
			max_dist = dists[i];
		}
	}

	double tmp;
	double t = 0.0;
	double b = 0.0;

#ifdef K_TODO_MAYBE
	/* Derived method by Franke & Nielson. Does not seem to work too well here. */
	for (int i = 0; i < 4; i++) {
		tmp = pow((1.0 * (max_dist - dists[i]) / max_dist * dists[i]), 2);
		t += tmp * elevs[i];
		b += tmp;
	}
#endif

	for (int i = 0; i < 4; i++) {
		tmp = pow((1.0 / dists[i]), 2);
		t += tmp * elevs[i];
		b += tmp;
	}

	qDebug() << SG_PREFIX_D << "Shepard Interpolation: tmp =" << tmp << "t =" << t << "b =" << b << "t/b =" << t/b;

	return (t/b);

}





void DEM::east_north_to_xy(double east, double north, int32_t * col, int32_t * row)
{
	*col = (int32_t) floor((east - this->min_east_seconds) / this->east_scale);
	*row = (int32_t) floor((north - this->min_north_seconds) / this->north_scale);
}





bool DEM::intersect(const LatLonBBox & other_bbox)
{
	LatLon dem_northeast;
	LatLon dem_southwest;

	/* Get min/max lat/lon of DEM data. */
	if (this->horiz_units == VIK_DEM_HORIZ_LL_ARCSECONDS) {
		dem_northeast.lat = this->max_north_seconds / 3600.0;
		dem_northeast.lon = this->max_east_seconds / 3600.0;
		dem_southwest.lat = this->min_north_seconds / 3600.0;
		dem_southwest.lon = this->min_east_seconds / 3600.0;

	} else if (this->horiz_units == VIK_DEM_HORIZ_UTM_METERS) {
		UTM dem_northeast_utm;
		dem_northeast_utm.northing = this->max_north_seconds;
		dem_northeast_utm.easting = this->max_east_seconds;

		dem_northeast_utm.zone = this->utm.zone;
		assert (UTM::is_band_letter(this->utm.get_band_letter())); /* TODO_2_LATER: add smarter error handling. In theory the source object should be valid and for sure contain valid band letter. */
		dem_northeast_utm.set_band_letter(this->utm.get_band_letter());

		UTM dem_southwest_utm;
		dem_southwest_utm.northing = this->min_north_seconds;
		dem_southwest_utm.easting = this->min_east_seconds;

		dem_southwest_utm.zone = this->utm.zone;
		assert (UTM::is_band_letter(this->utm.get_band_letter())); /* TODO_2_LATER: add smarter error handling. In theory the source object should be valid and for sure contain valid band letter. */
		dem_southwest_utm.set_band_letter(this->utm.get_band_letter());

		dem_northeast = UTM::to_latlon(dem_northeast_utm);
		dem_southwest = UTM::to_latlon(dem_southwest_utm);
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

	qDebug() << SG_PREFIX_I << "Viewport:" << other_bbox;
	qDebug() << SG_PREFIX_I << "DEM:     " << bbox;
	qDebug() << SG_PREFIX_I << "Overlap: " << (BBOX_INTERSECT(bbox, other_bbox) ? "true" : "false");

	return BBOX_INTERSECT(bbox, other_bbox);
}




DEMColumn::DEMColumn(double new_east_west, double new_south, int32_t new_n_points)
{
	this->east_west = new_east_west;
	this->south = new_south;
	this->n_points = new_n_points;

	this->points = (int16_t *) malloc(sizeof (int16_t) * this->n_points);
}




DEMColumn::~DEMColumn()
{
	free(this->points);
	this->points = NULL;
}
