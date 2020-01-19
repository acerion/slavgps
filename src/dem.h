/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2008, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _SG_DEM_H
#define _SG_DEM_H




#include <vector>
#include <cstdint>




#include <QString>




#include "coord.h"




namespace SlavGPS {




/* Unit codes. */




	class LatLonBBox;




	enum class DEMVerticalUnit {
		Meters = 1,     /* Wrong in 250k? */
		Decimeters = 2
	};




	enum class DEMHorizontalUnit {
		UTMMeters = 2,
		LatLonArcSeconds = 3
	};




	enum class DEMInterpolation {
		None = 0,
		Simple,
		Best,
	};




	enum class DEMSource {
		SRTM,
#ifdef VIK_CONFIG_DEM24K
		DEM24k,
#endif
		Unknown
	};




	class DEMColumn {

	public:
		DEMColumn(double east, double south, int32_t size);
		~DEMColumn();

		/* East-West coordinate for ALL items in the column. */
		double m_east = 0.0;

		/* Coordinate of northern and southern boundaries. */
		double m_south = 0.0;

		int32_t m_size = 0;
		int16_t * m_points = nullptr;
	};





	class DEM {
	public:
		virtual ~DEM();

		static DEMSource recognize_source_type(const QString & file_full_path);
		virtual sg_ret read_from_file(const QString & file_full_path) = 0;

		/**
		   @brief Try to find given @param coord in this DEM, and return its elevation

		   @return sg_ret::ok on logic errors
		   @return sg_ret::ok if no logic errors occurred (doesn't imply that a coordinate matched this DEM)
		   @return through @param elev: DEM::invalid_elevation on errors or if coordinate was not found in this DEM
		*/
		sg_ret get_elev_by_coord(const Coord & coord, DEMInterpolation method, int16_t & elev) const;

		void east_north_to_col_row(double east_seconds, double north_seconds, int32_t * col, int32_t * row) const;

		bool intersect(const LatLonBBox & other_bbox) const;

		int32_t n_columns = 0;
		std::vector<DEMColumn *> columns;

		DEMHorizontalUnit horiz_units = DEMHorizontalUnit::LatLonArcSeconds;
		DEMVerticalUnit orig_vert_units = DEMVerticalUnit::Decimeters; /* Original, always converted to meters when loading. */

		/*
		  Distance between samples.

		  For SRTM data this will be in arc seconds (LatLon
		  coord mode): either 1-arc-sec resolution or
		  3-arc-sec resolution.

		  For DEM24k data this may be either arc seconds
		  (LatLon) or meters (UTM).
		*/
		struct {
			double x;
			double y;
		} scale = { 0.0, 0.0 };


		double min_east_seconds;
		double min_north_seconds;
		double max_east_seconds;
		double max_north_seconds;

		UTM utm; /* UTM object, but only for storing band letter and zone number. */

		const char type_string[30] = "DEM object";

		static const int16_t invalid_elevation;

	private:
		int16_t get_elev_at_east_north_no_interpolation(double east_seconds, double north_seconds) const;
		int16_t get_elev_at_east_north_simple_interpolation(double east_seconds, double north_seconds) const;
		int16_t get_elev_at_east_north_shepard_interpolation(double east_seconds, double north_seconds) const;
		int16_t get_elev_at_col_row(int32_t col, int32_t row) const;

		bool get_ref_points_elevation_distance(double east_seconds, double north_seconds, int16_t * elevations, int16_t * distances) const;
	};




	class DEMSRTM : public DEM {
	public:
		sg_ret read_from_file(const QString & file_full_path) override;
	};




	class DEM24k : public DEM {
	public:
		sg_ret read_from_file(const QString & file_full_path) override;

	private:
		bool parse_header(char * buffer);
		void parse_block(char * buffer, int32_t * cur_column, int * cur_row);
		void parse_block_as_header(char * buffer, int32_t * cur_column, int32_t * cur_row);
		void parse_block_as_cont(char * buffer, int32_t * cur_column, int32_t * cur_row);

		void fix_exponentiation(char * buffer);
		bool get_int_and_continue(char ** buffer, int * result, const char * msg);
		bool get_double_and_continue(char ** buffer, double * result, const char * msg);
	};




} /* namespace SlavGPS */





#endif /* #ifndef _SG_DEM_H */
