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
 */

#ifndef _MAP_IDS_H_
#define _MAP_IDS_H_




namespace SlavGPS {




	enum class MapTypeID {
		Initial             =  -1, /* No ID set yet. */
		Default             =   0, /* Let the program select default map type id. */

		/* OLD Terraserver ids - listed for compatibility. */
		TERRASERVER_AERIAL  =   1,
		TERRASERVER_TOPO    =   2,
		TERRASERVER_URBAN   =   4,

		EXPEDIA             =   5,

		MAPNIK_RENDER       =   7,

		/* Mostly OSM related - except the Blue Marble value. */
		OSM_MAPNIK          =  13,
		BLUE_MARBLE         =  15,
		OSM_CYCLE           =  17,
		MAPQUEST_OSM        =  19,
		OSM_TRANSPORT       =  20,
		OSM_ON_DISK         =  21,
		OSM_HUMANITARIAN    =  22,
		MBTILES             =  23,
		OSM_METATILES       =  24,

		BING_AERIAL         = 212

		/*
		  Unfortunately previous ID allocations have been a
		  little haphazard, but hopefully future IDs can be
		  follow this scheme:

		   - 0 to 31 are intended for hard coded internal defaults
		   - 32-127 are intended for XML configuration map supplied defaults: see data/maps.xml
		   - 128 and above are intended for end user configurations.
		*/
	};




} /* namespace SlavGPS */




#endif /* #ifndef _MAP_IDS_H_ */
