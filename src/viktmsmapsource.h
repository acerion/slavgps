/*
 * viking
 * Copyright (C) 2010, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 *
 * viking is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * viking is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _SLAVGPS_MAP_SOURCE_TMS_H
#define _SLAVGPS_MAP_SOURCE_TMS_H





#include <stdbool.h>
#include <stdint.h>

#include "vikcoord.h"
#include "mapcoord.h"
#include "vikmapsource.h"
#include "map_ids.h"




namespace SlavGPS {





	class MapSourceTms : MapSource {

	public:

		MapSourceTms();
		MapSourceTms(MapTypeID map_type, const char * label_, const char * hostname_, const char * url_);
		~MapSourceTms();

		bool is_direct_file_access();
		bool is_mbtiles();
		bool is_osm_meta_tiles();

		bool supports_download_only_new();

		bool coord_to_mapcoord(const VikCoord * src, double xzoom, double yzoom, MapCoord * dest);
		void mapcoord_to_center_coord(MapCoord *src, VikCoord *dest);

		char * get_server_path(MapCoord *src);
	};





} /* namespace */





#endif /* _SLAVGPS_MAP_SOURCE_TMS_H */
