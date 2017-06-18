/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2011, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#ifndef _SG_BOUNDING_BOX_H_
#define _SG_BOUNDING_BOX_H_




#include <glib.h>




typedef struct {
	double north; /* max_lat */
	double south; /* min_lat */
	double east;  /* max_lon */
	double west;  /* min_lon */
} LatLonBBox;





typedef struct {
	char sminlon[G_ASCII_DTOSTR_BUF_SIZE];
	char smaxlon[G_ASCII_DTOSTR_BUF_SIZE];
	char sminlat[G_ASCII_DTOSTR_BUF_SIZE];
	char smaxlat[G_ASCII_DTOSTR_BUF_SIZE];
} LatLonBBoxStrings;





/**
 * +--------+
 * |a       |
 * |     +--+----+
 * |     |  |    |
 * +-----+--+    |
 *       |      b|
 *       +-------+
 */
#define BBOX_INTERSECT(a,b) ((a).south < (b).north && (a).north > (b).south && (a).east > (b).west && (a).west < (b).east)




#endif /* #ifndef _SG_BOUNDING_BOX_H_ */
