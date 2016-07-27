/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2012, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _DATASOURCE_GPS_H
#define _DATASOURCE_GPS_H

#include <stdint.h>

#include "viking.h"
#include "vikgpslayer.h"
#include "gtk/gtk.h"

#ifdef __cplusplus
extern "C" {
#endif


void * datasource_gps_setup(GtkWidget *dialog, SlavGPS::GPSTransferType xfer, bool xfer_all);
void datasource_gps_clean_up(void * user_data);

char* datasource_gps_get_protocol(void * user_data);
char* datasource_gps_get_descriptor(void * user_data);

bool datasource_gps_get_do_tracks(void * user_data);
bool datasource_gps_get_do_routes(void * user_data);
bool datasource_gps_get_do_waypoints(void * user_data);

bool datasource_gps_get_off(void * user_data);

#ifdef __cplusplus
}
#endif

#endif
