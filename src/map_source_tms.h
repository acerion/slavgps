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

#ifndef _SG_MAP_SOURCE_TMS_H_
#define _SG_MAP_SOURCE_TMS_H_




#include <cstdint>

#include "coord.h"
#include "mapcoord.h"
#include "map_source.h"




namespace SlavGPS {




	class MapSourceTms : MapSource {

	public:

		MapSourceTms();
		MapSourceTms(MapTypeID map_type, const QString & label, const QString & server_hostname, const QString & server_path_format);
		~MapSourceTms();

		bool is_direct_file_access(void) const;
		bool is_osm_meta_tiles(void) const;

		bool supports_download_only_new(void) const;

		bool coord_to_tile(const Coord & src_coord, const VikingZoomLevel & viking_zoom_level, TileInfo & dest) const;
		void tile_to_center_coord(const TileInfo & src, Coord & dest_coord) const;

		const QString get_server_path(const TileInfo & src) const;
	};




} /* namespace SlavGPS */




#endif /* _SG_MAP_SOURCE_TMS_H_ */
