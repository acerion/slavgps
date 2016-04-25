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
#ifndef __MAP_IDS_H
#define __MAP_IDS_H





namespace SlavGPS {





	typedef enum {
		MAP_TYPE_ID_INITIAL        =  -1, /* No ID set yet. */
		MAP_TYPE_ID_DEFAULT        =   0, /* Let the program select default map type id. */

		// OLD Terraserver ids - listed for compatibility
		MAP_ID_TERRASERVER_AERIAL  =   1,
		MAP_ID_TERRASERVER_TOPO    =   2,
		MAP_ID_TERRASERVER_URBAN   =   4,

		MAP_ID_EXPEDIA             =   5,

		MAP_ID_MAPNIK_RENDER       =   7,

		// Mostly OSM related - except the Blue Marble value
		MAP_ID_OSM_MAPNIK          =  13,
		MAP_ID_BLUE_MARBLE         =  15,
		MAP_ID_OSM_CYCLE           =  17,
		MAP_ID_MAPQUEST_OSM        =  19,
		MAP_ID_OSM_TRANSPORT       =  20,
		MAP_ID_OSM_ON_DISK         =  21,
		MAP_ID_OSM_HUMANITARIAN    =  22,
		MAP_ID_MBTILES             =  23,
		MAP_ID_OSM_METATILES       =  24,

		MAP_ID_BING_AERIAL         = 212

		// Unfortunately previous ID allocations have been a little haphazard,
		//  but hopefully future IDs can be follow this scheme:
		//   0 to 31 are intended for hard coded internal defaults
		//   32-127 are intended for XML configuration map supplied defaults: see data/maps.xml
		//   128 and above are intended for end user configurations.
	} MapTypeID;





} /* namespace SlavGPS */





#endif
