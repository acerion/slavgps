/*
 * viking
 * Copyright (C) 2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#ifndef _SLAVGPS_MAP_SOURCE_SLIPPY_H
#define _SLAVGPS_MAP_SOURCE_SLIPPY_H





#include <stdbool.h>
#include <stdint.h>

#include "vikcoord.h"
#include "mapcoord.h"
#include "vikmapsource.h"





namespace SlavGPS {





	class MapSourceSlippy : public MapSource {
	public:
		MapSourceSlippy();
		~MapSourceSlippy();
		MapSourceSlippy(uint16_t id, const char *label, const char *hostname, const char * path);

		MapSourceSlippy & operator=(MapSourceSlippy map);

		bool coord_to_mapcoord(const VikCoord * src, double xzoom, double yzoom, MapCoord * dest);
		void mapcoord_to_center_coord(MapCoord * src, VikCoord * dest);

		bool supports_download_only_new();

		char * get_server_path(MapCoord * src);

		DownloadResult_t download(MapCoord * src, const char * dest_fn, void * handle);

		uint16_t get_uniq_id();

		bool is_direct_file_access();
		bool is_mbtiles();
		bool is_osm_meta_tiles();
	};





} /* namespace */





#endif /* _SLAVGPS_MAP_SOURCE_SLIPPY_H */
