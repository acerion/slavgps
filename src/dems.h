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
#ifndef _SG_DEM_CACHE_H_
#define _SG_DEM_CACHE_H_




#include <list>
#include <string>
#include <cstdint>

#include "dem.h"
#include "coord.h"
#include "background.h"




namespace SlavGPS {




	typedef enum {
		VIK_DEM_INTERPOL_NONE = 0,
		VIK_DEM_INTERPOL_SIMPLE,
		VIK_DEM_INTERPOL_BEST,
	} VikDemInterpol;




	void    dem_cache_uninit();
	DEM   * dem_cache_load(std::string& filename);
	DEM   * dem_cache_get(std::string& filename);
	int     dem_cache_load_list(std::list<std::string>& filenames, BackgroundJob * bg_job);
	void    dem_cache_list_free(std::list<std::string>& filenames);
	int16_t dem_cache_get_elev_by_coord(const Coord * coord, VikDemInterpol method);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_DEM_CACHE_H_ */
