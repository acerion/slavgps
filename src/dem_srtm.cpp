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




#include <sys/types.h>
#include <sys/stat.h>
#include <endian.h>
#include <unistd.h>




#include <QFile>




#include "compression.h"
#include "dem_srtm.h"
#include "vikutils.h"




using namespace SlavGPS;




#define SG_MODULE "DEM SRTM"




static const int g_secs_per_degree = 60 * 60;




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
