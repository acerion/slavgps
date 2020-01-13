/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2008, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _SG_DEM_H
#define _SG_DEM_H




#include <vector>
#include <cstdint>




#include <QString>




#include "coords.h"




namespace SlavGPS {




#define DEM_INVALID_ELEVATION -32768

/* Unit codes. */
#define VIK_DEM_HORIZ_UTM_METERS 2
#define VIK_DEM_HORIZ_LL_ARCSECONDS  3

#define VIK_DEM_VERT_DECIMETERS 2

#define VIK_DEM_VERT_METERS 1 /* Wrong in 250k? */




	class LatLonBBox;




	class DEMColumn {

	public:
		DEMColumn(double east_west, double south, int32_t n_points);
		~DEMColumn();

		/* East-West coordinate for ALL items in the column. */
		double east_west;

		/* Coordinate of northern and southern boundaries. */
		double south;
		// double north;

		int32_t n_points;
		int16_t * points;
	};





	class DEM {
	public:
		~DEM();

		bool read_from_file(const QString & file_full_path);

		int16_t get_xy(int x, int y);
		int16_t get_east_north_no_interpolation(double east_seconds, double north_seconds);
		int16_t get_east_north_simple_interpolation(double east_seconds, double north_seconds);
		int16_t get_east_north_shepard_interpolation(double east_seconds, double north_seconds);
		// int16_t vik_dem_get_best_interpol(DEM * dem, double east_seconds, double north_seconds);

		void east_north_to_xy(double east_seconds, double north_seconds, int32_t * col, int32_t * row) const;

		bool get_ref_points_elevation_distance(double east_seconds,
						       double north_seconds,
						       int16_t * elevations,
						       int16_t * distances);

		bool intersect(const LatLonBBox & other_bbox);

		int32_t n_columns;
		std::vector<DEMColumn *> columns;

		uint8_t horiz_units;
		uint8_t orig_vert_units; /* Original, always converted to meters when loading. */
		double east_scale; /* Gap between samples. */
		double north_scale;

		double min_east_seconds;
		double min_north_seconds;
		double max_east_seconds;
		double max_north_seconds;

		UTM utm; /* UTM object, but only for storing band letter and zone number. */

		const char type_string[30] = "DEM object";


	private:
		bool read_srtm_hgt(const QString & file_full_path, const QString & file_name, bool zip);
		bool read_other(const QString & full_path);

		bool parse_header(char * buffer);
		void parse_block(char * buffer, int32_t * cur_column, int * cur_row);
		void parse_block_as_header(char * buffer, int32_t * cur_column, int32_t * cur_row);
		void parse_block_as_cont(char * buffer, int32_t * cur_column, int32_t * cur_row);
	};





} /* namespace SlavGPS */





#endif /* #ifndef _SG_DEM_H */
