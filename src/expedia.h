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
 */

#ifndef _SG_EXPEDIA_H_
#define _SG_EXPEDIA_H_




#include "coords.h"




namespace SlavGPS {




#define ALTI_TO_MPP 1.4017295
#define MPP_TO_ALTI 0.7134044




	class Expedia {
	public:
		static void init(void);

		static void init_radius(void);

		static bool lat_lon_to_screen_pos(fpixel * pos_x, fpixel * pos_y, const LatLon & lat_lon_center, const LatLon & lat_lon, double pixelfact_x, double pixelfact_y, int mapSizeX2, int mapSizeY2);
		static bool screen_pos_to_lat_lon(LatLon & lat_lon, int x, int y, const LatLon & lat_lon_center, double pixelfact_x, double pixelfact_y, fpixel mapSizeX2, fpixel mapSizeY2);

	};





} /* namespace SlavGPS */




#endif /* #ifndef _SG_EXPEDIA_H_ */
