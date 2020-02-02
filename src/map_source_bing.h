/*
 * viking
 * Copyright (C) 2011, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#ifndef _SG_BING_MAP_SOURCE_H_
#define _SG_BING_MAP_SOURCE_H_




#include <list>




#include <QFile>




#include "coord.h"
#include "map_source_slippy.h"
#include "background.h"
#include "globals.h"




namespace SlavGPS {




	class BingImageryProvider {
	public:
		QString attribution;
		int zoom_min = 0;
		int zoom_max = 0;
		LatLonBBox bbox;
	};




	class MapSourceBing : public MapSourceSlippy {
	public:
		MapSourceBing();
		MapSourceBing(MapTypeID map_type, const QString & ui_label, const QString & key);
		~MapSourceBing();

		void add_copyright(GisViewport * gisview, const LatLonBBox & bbox, const VikingScale & viking_scale);
		const QString get_server_path(const TileInfo & src) const;
		sg_ret load_providers(void);

		QString bing_api_key; /* The API key to access Bing. */

		std::list<BingImageryProvider *> providers;
		bool loading_providers = false;

	private:
		void async_load_providers(void);
		QString compute_quad_tree(int zoom, int tilex, int tiley) const;
		sg_ret parse_file_for_providers(QFile & file);
	};




} /* namespace SlavGPS */




#endif /* _SG_BING_MAP_SOURCE_H_ */
