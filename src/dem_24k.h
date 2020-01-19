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

#ifndef _SG_DEM_24K_H
#define _SG_DEM_24K_H




#include "dem.h"




namespace SlavGPS {




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




#endif /* #ifndef _SG_DEM_24K_H */
