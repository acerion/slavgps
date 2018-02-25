/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013-2014, Rob Norris <rw_norris@hotmail.com>
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




#ifndef _SG_MAP_UTILS_H_
#define _SG_MAP_UTILS_H_




#include "mapcoord.h"
#include "coord.h"




namespace SlavGPS {




	/* 1 << (x) is like a 2**(x) */
	/* Not sure what GZ stands for probably Google Zoom. */
	#define VIK_GZ(x) ((1<<(x)))


	int map_utils_mpp_to_scale(double mpp);

	int map_utils_mpp_to_zoom_level(double mpp);

	bool map_utils_coord_to_iTMS(const Coord & src_coord, double xzoom, double yzoom, TileInfo * dest);

	Coord map_utils_iTMS_to_center_coord(const TileInfo * src);

	Coord map_utils_iTMS_to_coord(const TileInfo * src);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_MAP_UTILS_H_ */
