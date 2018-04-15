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
#include <cstdint>

#include "coord.h"
#include "mapcoord.h"
#include "map_source_slippy.h"
#include "background.h"




namespace SlavGPS {




	class Attribution {
	public:
		QString attribution;
		int minZoom = 0;
		int maxZoom = 0;
		LatLonBBox bounds;
	};




	class MapSourceBing : public MapSourceSlippy {
	public:
		MapSourceBing();
		MapSourceBing(MapTypeID map_type, const QString & label, const QString & key);
		~MapSourceBing();

		void get_copyright(LatLonBBox bbox, double zoom, void (*fct)(Viewport *, QString const &), Viewport * viewport);
		const QString get_server_path(TileInfo * src) const;


		QString bing_api_key;

		std::list<Attribution *> attributions;
		/* Current attribution, when parsing. */
		QString attribution;
		bool loading_attributions = false;

	private:
		int load_attributions();
		void async_load_attributions();
		QString compute_quad_tree(int zoom, int tilex, int tiley) const;
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

		bool parse_file_for_attributions(QFile & file);
		int emit_update(void * data);
		background_thread_fn thread_fn = NULL;
	};




} /* namespace SlavGPS */




#endif /* _SG_BING_MAP_SOURCE_H_ */
