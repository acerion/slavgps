/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _VIKING_VIKCOORD_H
#define _VIKING_VIKCOORD_H


#include "coords.h"

#ifdef __cplusplus
extern "C" {
#endif


enum VikCoordMode {
	VIK_COORD_UTM     = 0,
	VIK_COORD_LATLON  = 1
};


typedef struct {
	double north_south; /* northing or lat */
	double east_west;   /* easting or lon */
	char utm_zone;
	char utm_letter;

	VikCoordMode mode;
} VikCoord;
/* notice we can cast to either UTM or LatLon */
/* possible more modes to come? xy? we'll leave that as an option */

void vik_coord_convert(VikCoord * coord, VikCoordMode dest_mode);
void vik_coord_copy_convert(const VikCoord * coord, VikCoordMode dest_mode, VikCoord * dest);
double vik_coord_diff(const VikCoord * c1, const VikCoord * c2);

void vik_coord_load_from_latlon(VikCoord * coord, VikCoordMode mode, const struct LatLon * ll);
void vik_coord_load_from_utm(VikCoord * coord, VikCoordMode mode, const struct UTM * utm);

void vik_coord_to_latlon(const VikCoord * coord, struct LatLon * dest);
void vik_coord_to_utm(const VikCoord * coord, struct UTM * dest);

bool vik_coord_equals(const VikCoord * coord1, const VikCoord * coord2);

void vik_coord_set_area(const VikCoord * coord, const struct LatLon * wh, VikCoord * tl, VikCoord * br);
bool vik_coord_inside(const VikCoord * coord, const VikCoord * tl, const VikCoord * br);
/* all coord operations MUST BE ABSTRACTED!!! */

#ifdef __cplusplus
}
#endif

#endif
