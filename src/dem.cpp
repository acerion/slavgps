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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <glib.h>
#ifdef HAVE_MATH_H
#include <math.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <stdlib.h>

#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "compression.h"
#include "dem.h"
#include "coords.h"
#include "fileutils.h"





using namespace SlavGPS;





/* Compatibility */
#if ! GLIB_CHECK_VERSION(2,22,0)
#define g_mapped_file_unref g_mapped_file_free
#endif

#define DEM_BLOCK_SIZE 1024





static bool get_double_and_continue(char ** buffer, double * tmp, bool warn)
{
	char * endptr;
	*tmp = g_strtod(*buffer, &endptr);
	if (endptr == NULL || endptr == *buffer) {
		if (warn) {
			fprintf(stderr, _("WARNING: Invalid DEM\n"));
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
			fprintf(stderr, _("WARNING: Invalid DEM\n"));
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
	this->utm_zone = int_val;
	/* TODO -- southern or northern hemisphere?! */
	this->utm_letter = 'N';

	double val;
	/* skip numbers 5-19  */
	for (int i = 0; i < 15; i++) {
		if (!get_double_and_continue(&buffer, &val, false)) {
			fprintf(stderr, _("WARNING: Invalid DEM header\n"));
			return false;
		}
	}

	/* number 20 -- horizontal unit code (utm/ll) */
	get_double_and_continue(&buffer, &val, true);
	this->horiz_units = val;
	get_double_and_continue(&buffer, &val, true);
	/* this->orig_vert_units = val; now done below */

	/* TODO: do this for real. these are only for 1:24k and 1:250k USGS */
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
	this->min_east = this->max_east = val;
	get_double_and_continue(&buffer, &val, true);
	this->min_north = this->max_north = val;

	for (int i = 0; i < 3; i++) {
		get_double_and_continue(&buffer, &val, true);
		if (val < this->min_east) {
			this->min_east = val;
		}

		if (val > this->max_east) {
			this->max_east = val;
		}
		get_double_and_continue(&buffer, &val, true);
		if (val < this->min_north) {
			this->min_north = val;
		}
		if (val > this->max_north) {
			this->max_north = val;
		}
	}

	return true;
}





void DEM::parse_block_as_cont(char * buffer, int * cur_column, int * cur_row)
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





void DEM::parse_block_as_header(char * buffer, int * cur_column, int * cur_row)
{
	/* 1 x n_rows 1 east_west south x x x DATA */

	double tmp;
	if ((!get_double_and_continue(&buffer, &tmp, true)) || tmp != 1) {
		fprintf(stderr, _("WARNING: Incorrect DEM Class B record: expected 1\n"));
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
	unsigned int n_rows = (unsigned int) tmp;

	if ((!get_double_and_continue(&buffer, &tmp, true)) || tmp != 1) {
		fprintf(stderr, _("WARNING: Incorrect DEM Class B record: expected 1\n"));
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
	(*cur_row) = (south - this->min_north) / this->north_scale;
	if (south > this->max_north || (*cur_row) < 0) {
		(*cur_row) = 0;
	}

	n_rows += *cur_row;

	DEMColumn * new_column = (DEMColumn *) malloc(sizeof (DEMColumn));
	this->columns[*cur_column] = new_column; /* kamilFIXME: this may cause crash if index is out of range. */
	this->columns[*cur_column]->east_west = east_west;
	this->columns[*cur_column]->south = south;
	this->columns[*cur_column]->n_points = n_rows;
	this->columns[*cur_column]->points = (int16_t *) malloc(sizeof (int16_t) * n_rows);

	/* no information for things before that */
	for (int i = 0; i < (*cur_row); i++) {
		this->columns[*cur_column]->points[i] = VIK_DEM_INVALID_ELEVATION;
	}

	/* now just continue */
	this->parse_block_as_cont(buffer, cur_column, cur_row);
}





void DEM::parse_block(char * buffer, int * cur_column, int * cur_row)
{
	/* if haven't read anything or have read all items in a columns and are expecting a new column */
	if (*cur_column == -1 || *cur_row == -1) {
		this->parse_block_as_header(buffer, cur_column, cur_row);
	} else {
		this->parse_block_as_cont(buffer, cur_column, cur_row);
	}
}





bool DEM::read_srtm_hgt(char const * file_name, char const * basename, bool zip)
{
	const int num_rows_3sec = 1201;
	const int num_rows_1sec = 3601;

	this->horiz_units = VIK_DEM_HORIZ_LL_ARCSECONDS;
	this->orig_vert_units = VIK_DEM_VERT_DECIMETERS;

	/* TODO */
	this->min_north = atoi(basename + 1) * 3600;
	this->min_east = atoi(basename + 4) * 3600;
	if (basename[0] == 'S') {
		this->min_north = -this->min_north;
	}

	if (basename[3] == 'W') {
		this->min_east = -this->min_east;
	}

	this->max_north = 3600 + this->min_north;
	this->max_east = 3600 + this->min_east;

	this->n_columns = 0;

	GError * error = NULL;
	GMappedFile * mf;
	if ((mf = g_mapped_file_new(file_name, false, &error)) == NULL) {
		fprintf(stderr, _("CRITICAL: Couldn't map file %s: %s\n"), file_name, error->message);
		g_error_free(error);
		return false;
	}
	off_t file_size = g_mapped_file_get_length(mf);
	char * dem_file = g_mapped_file_get_contents(mf);

	int16_t * dem_mem = NULL;
	if (zip) {
		void * unzip_mem = NULL;
		unsigned long ucsize;

		if ((unzip_mem = unzip_file(dem_file, &ucsize)) == NULL) {
			g_mapped_file_unref(mf);
			return false;
		}

		dem_mem = (int16_t *) unzip_mem;
		file_size = ucsize;
	} else {
		dem_mem = (int16_t *) dem_file;
	}

	int arcsec;
	if (file_size == (num_rows_3sec * num_rows_3sec * sizeof (int16_t))) {
		arcsec = 3;
	} else if (file_size == (num_rows_1sec * num_rows_1sec * sizeof (int16_t))) {
		arcsec = 1;
	} else {
		fprintf(stderr, "WARNING: %s(): file %s does not have right size\n", __PRETTY_FUNCTION__, basename);
		g_mapped_file_unref(mf);
		return false;
	}

	int num_rows = (arcsec == 3) ? num_rows_3sec : num_rows_1sec;
	this->east_scale = this->north_scale = arcsec;

	for (int i = 0; i < num_rows; i++) {
		this->n_columns++;
		DEMColumn * new_column = (DEMColumn *) malloc(sizeof (DEMColumn));
		this->columns.push_back(new_column);
		this->columns[i]->east_west = this->min_east + arcsec * i;
		this->columns[i]->south = this->min_north;
		this->columns[i]->n_points = num_rows;
		this->columns[i]->points = (int16_t *) malloc(sizeof (int16_t) * num_rows);
	}

	int ent = 0;
	for (int i = (num_rows - 1); i >= 0; i--) {
		for (int j = 0; j < num_rows; j++) {
			this->columns[j]->points[i] = GINT16_FROM_BE(dem_mem[ent]);
			ent++;
		}
	}

	if (zip) {
		free(dem_mem);
	}
	g_mapped_file_unref(mf);
	return true;
}





#define IS_SRTM_HGT(fn) (strlen((fn))==11 && (fn)[7]=='.' && (fn)[8]=='h' && (fn)[9]=='g' && (fn)[10]=='t' && ((fn)[0]=='N' || (fn)[0]=='S') && ((fn)[3]=='E' || (fn)[3]=='W'))





static bool is_strm_hgt(char const * basename)
{
	return (strlen(basename) == 11 || ((strlen(basename) == 15) && (basename[11] == '.' && basename[12] == 'z' && basename[13] == 'i' && basename[14] == 'p')))
		&& basename[7] == '.' && basename[8] == 'h' && basename[9] == 'g' && basename[10] == 't'
		&& (basename[0] == 'N' || basename[0] == 'S') && (basename[3] == 'E' || basename[3] == 'W');
}





static bool is_zip_file(char const * basename)
{
	size_t len = strlen(basename);
	return len == 15 && basename[len - 3] == 'z' && basename[len - 2] == 'i' && basename[len - 1] == 'p';
}





bool DEM::read(char const * file)
{
	if (access(file, R_OK) != 0) {
		return false;
	}

	const char * basename = a_file_basename(file);

	if (is_strm_hgt(basename)) {
		return this->read_srtm_hgt(file, basename, is_zip_file(basename));
	} else {
		return this->read_other(file);
	}
}





bool DEM::read_other(char const * file_name)
{
	/* Header */
	FILE * f = fopen(file_name, "r");
	if (!f) {
		return false;
	}

	char buffer[DEM_BLOCK_SIZE + 1];
	buffer[fread(buffer, 1, DEM_BLOCK_SIZE, f)] = '\0';
	if (!this->parse_header(buffer)) {
		fclose(f);
		return false;
	}
	/* TODO: actually use header -- i.e. GET # OF COLUMNS EXPECTED */

	this->n_columns = 0;

	/* Column -- Data */
	while (!feof(f)) {
		/* read block */
		buffer[fread(buffer, 1, DEM_BLOCK_SIZE, f)] = '\0';

		fix_exponentiation(buffer);

		/* Use the two variables to record state for ->parse_block(). */
		int cur_column = -1;
		int cur_row = -1;

		this->parse_block(buffer, &cur_column, &cur_row);
	}

	/* TODO - class C records (right now says 'Invalid' and dies) */

	fclose(f);
	f = NULL;

	/* 24k scale */
	if (this->horiz_units == VIK_DEM_HORIZ_UTM_METERS && this->n_columns >= 2) {
		this->north_scale = this->east_scale = this->columns[1]->east_west - this->columns[0]->east_west;
	}

	/* FIXME bug in 10m DEM's */
	if (this->horiz_units == VIK_DEM_HORIZ_UTM_METERS && this->north_scale == 10) {
		this->min_east -= 100;
		this->min_north += 200;
	}

	return true;
}





DEM::~DEM()
{
	for (unsigned int i = 0; i < this->n_columns; i++) {
		free(this->columns[i]->points);
		free(this->columns[i]);
	}
}





int16_t DEM::get_xy(unsigned int col, unsigned int row)
{
	if (col < this->n_columns) {
		if (row < this->columns[col]->n_points) {
			return this->columns[col]->points[row];
		}
	}
	return VIK_DEM_INVALID_ELEVATION;
}





int16_t DEM::get_east_north(double east, double north)
{
	if (east > this->max_east || east < this->min_east ||
	    north > this->max_north || north < this->min_north) {

		return VIK_DEM_INVALID_ELEVATION;
	}

	int col = (int) floor((east - this->min_east) / this->east_scale);
	int row = (int) floor((north - this->min_north) / this->north_scale);

	return this->get_xy(col, row);
}





bool DEM::get_ref_points_elev_dist(double east, double north, /* in seconds */
				      int16_t * elevs, int16_t * dists)
{
	int cols[4], rows[4];
	struct LatLon ll[4];
	struct LatLon pos;

	if (east > this->max_east || east < this->min_east ||
	    north > this->max_north || north < this->min_north) {

		return false;  /* got nothing */
	}

	pos.lon = east / 3600;
	pos.lat = north / 3600;

	/* order of the data: sw, nw, ne, se */
	/* sw */
	cols[0] = (int) floor((east - this->min_east) / this->east_scale);
	rows[0] = (int) floor((north - this->min_north) / this->north_scale);
	ll[0].lon = (this->min_east + this->east_scale*cols[0]) / 3600;
	ll[0].lat = (this->min_north + this->north_scale*rows[0]) / 3600;
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
		if ((elevs[i] = this->get_xy(cols[i], rows[i])) == VIK_DEM_INVALID_ELEVATION) {
			return false;
		}
		dists[i] = a_coords_latlon_diff(&pos, &ll[i]);
	}

#if 0  /* debug */
	for (int i = 0; i < 4; i++) {
		fprintf(stderr, "%f:%f:%d:%d  \n", ll[i].lat, ll[i].lon, dists[i], elevs[i]);
	}
	fprintf(stderr, "   north_scale=%f\n", this->north_scale);
#endif

	return true;  /* all OK */
}





int16_t DEM::get_simple_interpol(double east, double north)
{
	int16_t elevs[4], dists[4];
	if (!this->get_ref_points_elev_dist(east, north, elevs, dists)) {
		return VIK_DEM_INVALID_ELEVATION;
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
		return VIK_DEM_INVALID_ELEVATION;
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

#if 0 /* derived method by Franke & Nielson. Does not seem to work too well here */
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

	// fprintf(stderr, "DEBUG: tmp=%f t=%f b=%f %f\n", tmp, t, b, t/b);

	return (t/b);

}





void DEM::east_north_to_xy(double east, double north, unsigned int * col, unsigned int * row)
{
	*col = (unsigned int) floor((east - this->min_east) / this->east_scale);
	*row = (unsigned int) floor((north - this->min_north) / this->north_scale);
}





bool DEM::overlap(LatLonBBox * bbox)
{
	struct LatLon dem_northeast, dem_southwest;

	/* get min, max lat/lon of DEM data */
	if (this->horiz_units == VIK_DEM_HORIZ_LL_ARCSECONDS) {
		dem_northeast.lat = this->max_north / 3600.0;
		dem_northeast.lon = this->max_east / 3600.0;
		dem_southwest.lat = this->min_north / 3600.0;
		dem_southwest.lon = this->min_east / 3600.0;
	} else if (this->horiz_units == VIK_DEM_HORIZ_UTM_METERS) {
		struct UTM dem_northeast_utm, dem_southwest_utm;
		dem_northeast_utm.northing = this->max_north;
		dem_northeast_utm.easting = this->max_east;
		dem_southwest_utm.northing = this->min_north;
		dem_southwest_utm.easting = this->min_east;
		dem_northeast_utm.zone = dem_southwest_utm.zone = this->utm_zone;
		dem_northeast_utm.letter = dem_southwest_utm.letter = this->utm_letter;

		a_coords_utm_to_latlon(&dem_northeast_utm, &dem_northeast);
		a_coords_utm_to_latlon(&dem_southwest_utm, &dem_southwest);
	} else {
		// Unknown horiz_units - this shouldn't normally happen
		// Thus can't work out positions to use
		return false;
	}

	/* I wish we could use BBOX_INTERSECT() here. */

	if ((bbox->north > dem_northeast.lat && bbox->south > dem_northeast.lat) ||
	    (bbox->north < dem_southwest.lat && bbox->south < dem_southwest.lat)) {

		fprintf(stderr, "no overlap 1\n");
		return false;

	} else if ((bbox->east > dem_northeast.lon && bbox->west > dem_northeast.lon) ||
		   (bbox->east < dem_southwest.lon && bbox->west < dem_southwest.lon)) {

		fprintf(stderr, "no overlap 2\n");
		return false;
	} else {
		fprintf(stderr, "overlap\n");
		return true;
	}
}
