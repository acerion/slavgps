/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2006-2007, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#ifndef _VIKING_CONVERTER_H
#define _VIKING_CONVERTER_H

#include <glib.h>

G_BEGIN_DECLS

char *convert_lat_dec_to_ddd(double lat);
char *convert_lon_dec_to_ddd(double lon);

char *convert_lat_dec_to_dmm(double lat);
char *convert_lon_dec_to_dmm(double lon);

char *convert_lat_dec_to_dms(double lat);
char *convert_lon_dec_to_dms(double lon);

double convert_dms_to_dec(const char *dms);

G_END_DECLS

#endif
