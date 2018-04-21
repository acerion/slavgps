/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_GLOBALS_H_
#define _SG_GLOBALS_H_




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdint>




#define SG_APPLICATION_NAME "slavgps"




namespace SlavGPS {




	enum class SublayerType {
		NONE,
		TRACKS,
		WAYPOINTS,
		TRACK,
		WAYPOINT,
		ROUTES,
		ROUTE
	};




	enum class LayerType {
		AGGREGATE = 0,
		TRW,
		COORD,
		GEOREF,
		GPS,
		MAP,
		DEM,
#ifdef HAVE_LIBMAPNIK
		MAPNIK,
#endif
		NUM_TYPES // Also use this value to indicate no layer association
	};

	LayerType& operator++(LayerType& layer_type);




	typedef int16_t param_id_t; /* This shall be a signed type. */




	typedef enum sort_order_e {
		VL_SO_NONE = 0,
		VL_SO_ALPHABETICAL_ASCENDING,
		VL_SO_ALPHABETICAL_DESCENDING,
		VL_SO_DATE_ASCENDING,
		VL_SO_DATE_DESCENDING,
		VL_SO_LAST
	} sort_order_t;




#define PROJECT "Viking"
#define VIKING_VERSION PACKAGE_VERSION
#define VIKING_VERSION_NAME "This Name For Rent"
#define VIKING_URL PACKAGE_URL




} /* namespace SlavGPS */




#endif /* #ifndef _SG_GLOBALS_H_ */
