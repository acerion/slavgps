/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_SETTINGS_H_
#define _SG_SETTINGS_H_




#include <cstdint>




void a_settings_init();

void a_settings_uninit();

bool a_settings_get_boolean(const char * name, bool * val);

void a_settings_set_boolean(const char * name, bool val);

bool a_settings_get_string(const char * name, char ** val);

void a_settings_set_string(const char * name, const char * val);

bool a_settings_get_integer(const char * name, int * val);

void a_settings_set_integer(const char * name, int val);

bool a_settings_get_double(const char * name, double * val);

void a_settings_set_double(const char * name, double val);

/*
bool a_settings_get_integer_list(const char * name, int * vals, size_t * length);

void a_settings_set_integer_list(const char * name, int vals[], size_t length);
*/
bool a_settings_get_integer_list_contains(const char * name, int val);

void a_settings_set_integer_list_containing(const char * name, int val);




#endif /* #ifndef _SG_SETTINGS_H_ */
