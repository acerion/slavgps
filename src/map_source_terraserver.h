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
#include "map_ids.h"




namespace SlavGPS {



	class MapSourceTerraserver : public MapSource {
	public:
		MapSourceTerraserver();
		MapSourceTerraserver(MapTypeID type_, const char * label_);


		bool coord_to_tile(const Coord * src, double xmpp, double ympp, TileInfo * dest);
		void tile_to_center_coord(TileInfo * src, Coord * dest);
		bool is_direct_file_access(void);
		bool is_mbtiles(void);

		char * get_server_hostname(void);
		char * get_server_path(TileInfo * src);

		MapTypeID type = MAP_TYPE_ID_INITIAL;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TERRASERVER_MAP_SOURCE_H_ */
