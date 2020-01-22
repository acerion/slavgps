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




#include "layer_dem_dem_24k.h"




using namespace SlavGPS;




#define SG_MODULE "DEM 24k"
#define DEM_BLOCK_SIZE 1024




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
	DEM24k::get_double_and_continue(&buffer, &val, "horizontal unit code"); /* TODO: do we really want to read double to get horiz unit? */
	this->horiz_units = (DEMHorizontalUnit) val; /* TODO: we should somehow validate if val has expected value (2 or 3). */

	DEM24k::get_double_and_continue(&buffer, &val, "orig vert units"); /* TODO: do we really want to read double to get vert. unit? */
	/* this->orig_vert_units = val; now done below */

	/* TODO: do this for real. these are only for 1:24k and 1:250k USGS */
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
	/* TODO: actually use header -- i.e. GET # OF COLUMNS EXPECTED */

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

	/* TODO - class C records (right now says 'Invalid' and dies) */

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
