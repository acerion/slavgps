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

#ifndef _SG_MAP_SOURCE_WMSC_H_
#define _SG_MAP_SOURCE_WMSC_H_




#include <cstdint>

#include "coord.h"
#include "mapcoord.h"
#include "map_source.h"




namespace SlavGPS {




	struct MapSourceWmsc : MapSource {

	public:
		MapSourceWmsc();
		~MapSourceWmsc();
		MapSourceWmsc(MapTypeID map_type, const QString & label, const QString & server_hostname, const QString & server_path_format);

		bool coord_to_tile(const Coord & src_coord, double xzoom, double yzoom, TileInfo * dest) const;
		void tile_to_center_coord(TileInfo * src, Coord & dest_coord) const;

		bool supports_download_only_new(void) const;

		bool is_direct_file_access(void) const;
		bool is_osm_meta_tiles(void) const;

		const QString get_server_path(TileInfo * src) const;
	};




} /* namespace SlavGPS */




#endif /* _SG_MAP_SOURCE_WMSC_H_ */
