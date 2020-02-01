/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2016-2020, Kamil Ignacak <acerion@wp.pl>
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

#ifndef _SG_MAP_SOURCE_MBTILES_H_
#define _SG_MAP_SOURCE_MBTILES_H_




#include "map_source_slippy.h"




namespace SlavGPS {




	class MapSourceMBTiles : public MapSourceSlippy {
	public:

		MapSourceMBTiles();
		~MapSourceMBTiles();

		QPixmap get_tile_pixmap(const MapCacheObj & map_cache_obj, const TileInfo & tile_info, const MapSourceArgs & args) const;
		QStringList get_tile_description(const MapCacheObj & map_cache_obj, const TileInfo & tile_info, const MapSourceArgs & args) const;

		sg_ret open_map_source(const MapSourceArgs & args, QString & error_message) override;
		sg_ret close_map_source(void) override;


	private:
		QPixmap create_pixmap_sql_exec(const TileInfo & tile_info) const;
#ifdef HAVE_SQLITE3_H
		sqlite3 * sqlite_handle = nullptr;
#endif
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_MAP_SOURCE_MBTILES_H_ */
