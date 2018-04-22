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

#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
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




using namespace SlavGPS;




#define PREFIX ": DEM:" << __FUNCTION__ << __LINE__ << ">"




#define DEM_BLOCK_SIZE 1024





static bool get_double_and_continue(char ** buffer, double * tmp, bool warn)
{
	char * endptr;
	*tmp = g_strtod(*buffer, &endptr);
	if (endptr == NULL || endptr == *buffer) {
		if (warn) {
			qDebug() << "WW: DEM: invalid DEM";
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
			qDebug() << "WW: DEM: invalid DEM";
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
			qDebug() << "WW: DEM: invalid DEM header";
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
		qDebug() << "WW: DEM: Parse Block: Incorrect DEM Class B record: expected 1";
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
		qDebug() << "WW: DEM: Parse Block: Incorrect DEM Class B record: expected 1";
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
	this->columns[*cur_column] = new_column; /* FIXME: this may cause crash if index is out of range. */
	this->columns[*cur_column]->east_west = east_west;
	this->columns[*cur_column]->south = south;
	this->columns[*cur_column]->n_points = n_rows;
	this->columns[*cur_column]->points = (int16_t *) malloc(sizeof (int16_t) * n_rows);

	/* no information for things before that */
	for (int i = 0; i < (*cur_row); i++) {
		this->columns[*cur_column]->points[i] = DEM_INVALID_ELEVATION;
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




bool DEM::read_srtm_hgt(const QString & file_full_path, const QString & file_name, bool zip)
{
	const int num_rows_3sec = 1201;
	const int num_rows_1sec = 3601;

	this->horiz_units = VIK_DEM_HORIZ_LL_ARCSECONDS;
	this->orig_vert_units = VIK_DEM_VERT_DECIMETERS;

	/* TODO */
	this->min_north = atoi(file_name.toUtf8().constData() + 1) * 3600;
	this->min_east = atoi(file_name.toUtf8().constData() + 4) * 3600;
	if (file_name[0] == 'S') {
		this->min_north = -this->min_north;
	}

	if (file_name[3] == 'W') {
		this->min_east = -this->min_east;
	}

	this->max_north = 3600 + this->min_north;
	this->max_east = 3600 + this->min_east;

	this->n_columns = 0;


	QFile file(file_full_path);
	if (!file.open(QIODevice::ReadOnly)) {
		qDebug() << "EE" PREFIX << "Can't open file" << file_full_path << file.error();
		return false;
	}

	off_t file_size = file.size();
	unsigned char * file_contents = file.map(0, file_size, QFileDevice::MapPrivateOption);
	if (!file_contents) {
		qDebug() << "EE" PREFIX << "Can't map file" << file_full_path << file.error();
		return false;
	}

	int16_t * dem_contents = NULL;
	unsigned long dem_size = 0;
	if (zip) {
		void * uncompressed_contents = NULL;
		unsigned long uncompressed_size;

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
		qDebug() << "WW: DEM: Read SRTM HGT: file" << file_name << "does not have right size";
		file.unmap(file_contents);
		file.close();
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




#define IS_SRTM_HGT(fn) (strlen((fn))==11 && (fn)[7]=='.' && (fn)[8]=='h' && (fn)[9]=='g' && (fn)[10]=='t' && ((fn)[0]=='N' || (fn)[0]=='S') && ((fn)[3]=='E' || (fn)[3]=='W'))




static bool is_strm_hgt(const QString & file_name)
{
	size_t len = file_name.size();
	return (len == 11 || ((len == 15) && (file_name[11] == '.' && file_name[12] == 'z' && file_name[13] == 'i' && file_name[14] == 'p')))
		&& file_name[7] == '.' && file_name[8] == 'h' && file_name[9] == 'g' && file_name[10] == 't'
		&& (file_name[0] == 'N' || file_name[0] == 'S') && (file_name[3] == 'E' || file_name[3] == 'W');
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
	FILE * f = fopen(full_path.toUtf8().constData(), "r");
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
	return DEM_INVALID_ELEVATION;
}





int16_t DEM::get_east_north(double east, double north)
{
	if (east > this->max_east || east < this->min_east ||
	    north > this->max_north || north < this->min_north) {

		return DEM_INVALID_ELEVATION;
	}

	int col = (int) floor((east - this->min_east) / this->east_scale);
	int row = (int) floor((north - this->min_north) / this->north_scale);

	return this->get_xy(col, row);
}





bool DEM::get_ref_points_elev_dist(double east, double north, /* in seconds */
				   int16_t * elevs, int16_t * dists)
{
	int cols[4], rows[4];
	LatLon ll[4];

	if (east > this->max_east || east < this->min_east ||
	    north > this->max_north || north < this->min_north) {

		return false;  /* got nothing */
	}

	const LatLon pos(north / 3600, east / 3600);

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
		if ((elevs[i] = this->get_xy(cols[i], rows[i])) == DEM_INVALID_ELEVATION) {
			return false;
		}
		dists[i] = a_coords_latlon_diff(pos, ll[i]);
	}

#if 0   /* Debug. */
	for (int i = 0; i < 4; i++) {
		qDebug() << "DD: DEM:" << i << ":" << ll[i].lat << ll[i].lon << dists[i] << elevs[i];
	}
	qDebug() << "DD: DEM:   north_scale =" this->north_scale;
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

#ifdef K_TODO
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

	qDebug() << "DD: DEM: Shepard Interpolation: tmp =" << tmp << "t =" << t << "b =" << b << "t/b =" << t/b;

	return (t/b);

}





void DEM::east_north_to_xy(double east, double north, unsigned int * col, unsigned int * row)
{
	*col = (unsigned int) floor((east - this->min_east) / this->east_scale);
	*row = (unsigned int) floor((north - this->min_north) / this->north_scale);
}





bool DEM::overlap(const LatLonBBox & other_bbox)
{
	LatLon dem_northeast;
	LatLon dem_southwest;

	/* get min, max lat/lon of DEM data */
	if (this->horiz_units == VIK_DEM_HORIZ_LL_ARCSECONDS) {
		dem_northeast.lat = this->max_north / 3600.0;
		dem_northeast.lon = this->max_east / 3600.0;
		dem_southwest.lat = this->min_north / 3600.0;
		dem_southwest.lon = this->min_east / 3600.0;

	} else if (this->horiz_units == VIK_DEM_HORIZ_UTM_METERS) {
		UTM dem_northeast_utm;
		dem_northeast_utm.northing = this->max_north;
		dem_northeast_utm.easting = this->max_east;
		dem_northeast_utm.zone = this->utm_zone;
		dem_northeast_utm.letter = this->utm_letter;

		UTM dem_southwest_utm;
		dem_southwest_utm.northing = this->min_north;
		dem_southwest_utm.easting = this->min_east;
		dem_southwest_utm.zone = this->utm_zone;
		dem_southwest_utm.letter = this->utm_letter;

		dem_northeast = UTM::to_latlon(dem_northeast_utm);
		dem_southwest = UTM::to_latlon(dem_southwest_utm);
	} else {
		// Unknown horiz_units - this shouldn't normally happen
		// Thus can't work out positions to use
		return false;
	}

	/* I wish we could use BBOX_INTERSECT() here. */

	if ((other_bbox.north > dem_northeast.lat && other_bbox.south > dem_northeast.lat) ||
	    (other_bbox.north < dem_southwest.lat && other_bbox.south < dem_southwest.lat)) {

		qDebug() << "DD: DEM: Overlap: false 1";
		return false;

	} else if ((other_bbox.east > dem_northeast.lon && other_bbox.west > dem_northeast.lon) ||
		   (other_bbox.east < dem_southwest.lon && other_bbox.west < dem_southwest.lon)) {

		qDebug() << "DD: DEM: Overlap: false 2";
		return false;
	} else {
		qDebug() << "DD: DEM: Overlap: true";
		return true;
	}
}
