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
 */

#ifndef _SG_DEM_CACHE_H_
#define _SG_DEM_CACHE_H_




#include <list>
#include <cstdint>

#include <QString>

#include "dem.h"
#include "coord.h"
#include "background.h"




namespace SlavGPS {




	enum class DemInterpolation {
		NONE = 0,
		SIMPLE,
		BEST,
	};




	class DEMCache {
	public:
		static void uninit(void); /* For module deinitialization. */

		static DEM * load_file_into_cache(const QString & file_path);
		static int   load_files_into_cache(std::list<QString> & file_paths, BackgroundJob * bg_job);
		static void  unload_from_cache(std::list<QString> & file_paths);

		static DEM * get(const QString & file_path);

		static int16_t get_elev_by_coord(const Coord * coord, DemInterpolation method);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_DEM_CACHE_H_ */
