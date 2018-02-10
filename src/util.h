/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2007-2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
 * Based on:
 * Copyright (C) 2003-2007, Leandro A. F. Pereira <leandro@linuxmag.com.br>
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

#ifndef _SG_UTIL_H_
#define _SG_UTIL_H_




#include <cstdint>
#include <cstdbool>




namespace SlavGPS {




	unsigned int util_get_number_of_cpus(void);

	char * uri_escape(const char * str);

	bool split_string_from_file_on_equals(char const * buf, char ** key, char ** val);

	void util_add_to_deletion_list(const QString & file_full_path);
	void util_remove_all_in_deletion_list(void);

	char * util_str_remove_chars(char * string, char const * chars);

	bool util_remove(const QString & file_full_path);

	QString util_write_tmp_file_from_bytes(const void * buffer, size_t count);




} /* namespace SlavGPS */




#ifndef N_
#define N_(s) s
#endif




#endif /* #ifndef _SG_UTIL_H_ */
