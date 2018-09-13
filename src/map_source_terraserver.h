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

#ifndef _SG_TERRASERVER_MAP_SOURCE_H_
#define _SG_TERRASERVER_MAP_SOURCE_H_




#include <cstdint>

#include "map_source.h"




namespace SlavGPS {



	class MapSourceTerraserver : public MapSource {
	public:
		MapSourceTerraserver();
		MapSourceTerraserver(MapTypeID type, const QString & label);


		bool coord_to_tile(const Coord & src_coord, const VikingZoomLevel & viking_zoom_level, TileInfo & dest) const;
		void tile_to_center_coord(const TileInfo & src, Coord & dest_coord) const;
		bool is_direct_file_access(void) const;

		const QString get_server_hostname(void) const;
		const QString get_server_path(const TileInfo & src) const;

		MapTypeID type = MapTypeID::Initial;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TERRASERVER_MAP_SOURCE_H_ */
