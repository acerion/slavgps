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




typedef uint32_t sg_uid_t;
#define SG_UID_INITIAL  1
#define SG_UID_NONE     0




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

#define ALTI_TO_MPP 1.4017295
#define MPP_TO_ALTI 0.7134044

#define VIK_FEET_IN_METER 3.2808399
#define VIK_METERS_TO_FEET(X) ((X)*VIK_FEET_IN_METER)
#define VIK_FEET_TO_METERS(X) ((X)/VIK_FEET_IN_METER)

#define VIK_MILES_IN_METER 0.000621371192
#define VIK_METERS_TO_MILES(X) ((X)*VIK_MILES_IN_METER)
#define VIK_MILES_TO_METERS(X) ((X)/VIK_MILES_IN_METER)

#define VIK_NAUTICAL_MILES_IN_METER 0.000539957
#define VIK_METERS_TO_NAUTICAL_MILES(X) ((X)*VIK_NAUTICAL_MILES_IN_METER)
#define VIK_NAUTICAL_MILES_TO_METERS(X) ((X)/VIK_NAUTICAL_MILES_IN_METER)

/* MPS - Metres Per Second. */
/* MPH - Metres Per Hour. */
#define VIK_MPH_IN_MPS 2.23693629
#define VIK_MPS_TO_MPH(X) ((X)*VIK_MPH_IN_MPS)
#define VIK_MPH_TO_MPS(X) ((X)/VIK_MPH_IN_MPS)

/* KPH - Kilometres Per Hour. */
#define VIK_KPH_IN_MPS 3.6
#define VIK_MPS_TO_KPH(X) ((X)*VIK_KPH_IN_MPS)
#define VIK_KPH_TO_MPS(X) ((X)/VIK_KPH_IN_MPS)

#define VIK_KNOTS_IN_MPS 1.94384449
#define VIK_MPS_TO_KNOTS(X) ((X)*VIK_KNOTS_IN_MPS)
#define VIK_KNOTS_TO_MPS(X) ((X)/VIK_KNOTS_IN_MPS)

#define VIK_DEFAULT_ALTITUDE 0.0
#define VIK_DEFAULT_DOP 0.0


#define DEG2RAD(x) ((x)*(M_PI/180))
#define RAD2DEG(x) ((x)*(180/M_PI))

/* Mercator projection, latitude conversion (degrees). */
#define MERCLAT(x) (RAD2DEG(log(tan((0.25 * M_PI) + (0.5 * DEG2RAD(x))))))
#define DEMERCLAT(x) (RAD2DEG(atan(sinh(DEG2RAD(x)))))




	/* Time display format. */
	enum class SGTimeReference {
		Locale, /* User's locale. */
		World,  /* Derive the local timezone at the object's position. */
		UTC,
	};



	/* KML export preferences. */
	typedef enum {
		VIK_KML_EXPORT_UNITS_METRIC,
		VIK_KML_EXPORT_UNITS_STATUTE,
		VIK_KML_EXPORT_UNITS_NAUTICAL,
	} vik_kml_export_units_t;




	enum class GPXExportTrackSort {
		Alpha,
		Time,
		Creation
	};




	typedef enum {
		VIK_GPX_EXPORT_WPT_SYM_NAME_TITLECASE,
		VIK_GPX_EXPORT_WPT_SYM_NAME_LOWERCASE,
	} vik_gpx_export_wpt_sym_name_t;




} /* namespace SlavGPS */




#endif /* #ifndef _SG_GLOBALS_H_ */
