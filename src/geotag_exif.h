/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2011-2014, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _VIKING_GEOTAG_EXIF_H
#define _VIKING_GEOTAG_EXIF_H

#include <stdbool.h>
#include <stdint.h>

#include "vikwaypoint.h"
#include "vikcoord.h"


using namespace SlavGPS;

#ifdef __cplusplus
extern "C" {
#endif


Waypoint * a_geotag_create_waypoint_from_file ( const char *filename, VikCoordMode vcmode, char **name );

Waypoint * a_geotag_waypoint_positioned ( const char *filename, VikCoord coord, double alt, char **name, Waypoint *wp );

char* a_geotag_get_exif_date_from_file ( const char *filename, bool *has_GPS_info );

struct LatLon a_geotag_get_position ( const char *filename );

int a_geotag_write_exif_gps ( const char *filename, VikCoord coord, double alt, bool no_change_mtime );

#ifdef __cplusplus
}
#endif

#endif // _VIKING_GEOTAG_EXIF_H
