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
 *
 */

#ifndef __VIKING_DEM_H
#define __VIKING_DEM_H

#include <glib.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


#define VIK_DEM_INVALID_ELEVATION -32768

/* unit codes */
#define VIK_DEM_HORIZ_UTM_METERS 2
#define VIK_DEM_HORIZ_LL_ARCSECONDS  3

#define VIK_DEM_VERT_DECIMETERS 2

#define VIK_DEM_VERT_METERS 1 /* wrong in 250k?	 */


typedef struct {
  unsigned int n_columns;
  GPtrArray *columns;

  uint8_t horiz_units;
  uint8_t orig_vert_units; /* original, always converted to meters when loading. */
  double east_scale; /* gap between samples */
  double north_scale;

  double min_east, min_north, max_east, max_north;

  uint8_t utm_zone;
  char utm_letter;
} VikDEM;

typedef struct {
  /* east-west coordinate for ALL items in the column */
  double east_west;

  /* coordinate of northern and southern boundaries */
  double south;
//  double north;

  unsigned int n_points;
  int16_t *points;
} VikDEMColumn;


VikDEM *vik_dem_new_from_file(const char *file);
void vik_dem_free ( VikDEM *dem );
int16_t vik_dem_get_xy ( VikDEM *dem, unsigned int x, unsigned int y );

int16_t vik_dem_get_east_north ( VikDEM *dem, double east, double north );
int16_t vik_dem_get_simple_interpol ( VikDEM *dem, double east, double north );
int16_t vik_dem_get_shepard_interpol ( VikDEM *dem, double east, double north );
int16_t vik_dem_get_best_interpol ( VikDEM *dem, double east, double north );

void vik_dem_east_north_to_xy ( VikDEM *dem, double east, double north, unsigned int *col, unsigned int *row );

#ifdef __cplusplus
}
#endif

#endif
