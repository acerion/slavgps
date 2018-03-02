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

#ifndef _SG_MAP_SOURCE_SLIPPY_H_
#define _SG_MAP_SOURCE_SLIPPY_H_




#include <cstdint>

#include <QString>

#include "coord.h"
#include "mapcoord.h"
#include "map_source.h"
#include "map_ids.h"




namespace SlavGPS {




	class MapSourceSlippy : public MapSource {
	public:
		MapSourceSlippy();
		~MapSourceSlippy();
		MapSourceSlippy(MapTypeID map_type, const QString & label, const QString & server_hostname, const QString & server_path_format);

		MapSourceSlippy & operator=(MapSourceSlippy map);

		bool coord_to_tile(const Coord & src_coord, double xzoom, double yzoom, TileInfo * dest);
		void tile_to_center_coord(TileInfo * src, Coord & dest_coord);

		bool supports_download_only_new();

		const QString get_server_path(TileInfo * src) const;

		DownloadResult download(TileInfo * src, const QString & dest_file_path, DownloadHandle * dl_handle);


		bool is_direct_file_access();
		bool is_mbtiles();
		bool is_osm_meta_tiles();
	};




} /* namespace SlavGPS */




#endif /* _SG_MAP_SOURCE_SLIPPY_H_ */
