/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _VIKING_METATILE_H
#define _VIKING_METATILE_H

// MAX_SIZE is the biggest file which we will return to the user
#define METATILE_MAX_SIZE (1 * 1024 * 1024)

#ifdef __cplusplus
extern "C" {
#endif

int xyz_to_meta(char * path, size_t len, char const * dir, int x, int y, int z);

int metatile_read(char const * dir, int x, int y, int z, char * buf, size_t sz, int * compressed, char * log_msg);

#ifdef __cplusplus
}
#endif

#endif // #ifndef _VIKING_METATILE_H
