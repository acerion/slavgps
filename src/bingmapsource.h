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

#ifndef _BING_MAP_SOURCE_H
#define _BING_MAP_SOURCE_H

#include <stdbool.h>
#include <stdint.h>

#include "vikcoord.h"
#include "mapcoord.h"
#include "vikslippymapsource.h"
#include "map_ids.h"





namespace SlavGPS {





	struct _Attribution {
		char * attribution;
		int minZoom;
		int maxZoom;
		LatLonBBox bounds;
	};





	class MapSourceBing : public MapSourceSlippy {
	public:
		MapSourceBing::MapSourceBing();
		MapSourceBing::MapSourceBing(MapTypeID map_type_, const char * label_, const char * key_);
		MapSourceBing::~MapSourceBing();

		void MapSourceBing::get_copyright(LatLonBBox bbox, double zoom, void (*fct)(Viewport *, const char *), void * data)
		char * MapSourceBing::get_server_path(TileInfo * src);


		char * bing_api_key;

		GList *attributions;
		/* Current attribution, when parsing */
		char *attribution;
		bool loading_attributions;

	private:
		int load_attributions();
		void async_load_attributions();
		char * compute_quad_tree(int zoom, int tilex, int tiley);
		static void bstart_element(GMarkupParseContext * context,
					   const char          * element_name,
					   const char         ** attribute_names,
					   const char         ** attribute_values,
					   void                * user_data,
					   GError             ** error);

		void btext(GMarkupParseContext * context,
			   const char          * text,
			   size_t                text_len,
			   void                * user_data,
			   GError             ** error);

		bool parse_file_for_attributions(char *filename);
		int emit_update(void * data);
		int load_attributions_thread(void * threaddata);
		void async_load_attributions();
	};





} /* namespace SlavGPS */





#endif /* _BING_MAP_SOURCE_H_ */
