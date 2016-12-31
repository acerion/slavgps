/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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
 *
 */

#ifndef _SG_LAYER_MAPS_H
#define _SG_LAYER_MAPS_H




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string>

#include <stdint.h>

#include "coord.h"
#include "layer.h"
#include "viewport.h"
#include "vikmapsource.h"
#include "mapcoord.h"
#include "vikmapslayer_compat.h"

#ifdef HAVE_SQLITE3_H
#include "sqlite3.h"
#include <gio/gio.h>
#endif




namespace SlavGPS {




	enum class MapsCacheLayout {
		VIKING = 0, /* CacheDir/t<MapId>s<VikingZoom>z0/X/Y (NB no file extension) - Legacy default layout. */
		OSM,        /* CacheDir/<OptionalMapName>/OSMZoomLevel/X/Y.ext (Default ext=png). */
		NUM         /* Last enum. */
	};




	class LayerMaps : public Layer {
	public:
		LayerMaps();
		LayerMaps(Viewport * viewport);
		~LayerMaps();


		/* Layer interface methods. */
		void post_read(Viewport * viewport, bool from_file);
		void draw(Viewport * viewport);
		void draw_section(Viewport * viewport, VikCoord *ul, VikCoord *br);
		QString tooltip();
		void add_menu_items(QMenu & menu);
		bool set_param_value(uint16_t id, ParameterValue param_value, bool is_file_operation);
		ParameterValue get_param_value(param_id_t id, bool is_file_operation) const;

		char * get_map_label();
		int how_many_maps(VikCoord * ul, VikCoord * br, double zoom, int redownload_mode);

		void set_cache_dir(char const * dir);
		void mkdir_if_default_dir();
		void set_file(char const * name);

		void set_map_type(MapTypeID type_id);
		MapTypeID get_map_type();

		void download(Viewport * viewport, bool only_new);
		void download_section(VikCoord * ul, VikCoord * br, double zoom);
		void download_section_sub(VikCoord *ul, VikCoord *br, double zoom, int redownload_mode);


		static void weak_ref_cb(void * ptr, GObject * dead_vml);




		unsigned int map_index = 0;
		char * cache_dir = NULL;
		MapsCacheLayout cache_layout = MapsCacheLayout::VIKING;
		uint8_t alpha = 0;

		unsigned int mapzoom_id = 0;
		double xmapzoom = 0.0;
		double ymapzoom = 0.0;

		bool autodownload = false;
		bool adl_only_missing = false;

		VikCoord * last_center = NULL;
		double last_xmpp = 0.0;
		double last_ympp = 0.0;

		/* Should this be 0 or -1? */
		int dl_tool_x = -1;
		int dl_tool_y = -1;

		GtkMenu * dl_right_click_menu = NULL;
		VikCoord redownload_ul, redownload_br; /* Right click menu only. */
		Viewport * redownload_viewport = NULL;
		char * filename = NULL;

#ifdef HAVE_SQLITE3_H
		sqlite3 * mbtiles = NULL;
#endif

	};




	class LayerToolMapsDownload : public LayerTool {
	public:
		LayerToolMapsDownload(Window * window, Viewport * viewport);

		LayerToolFuncStatus click_(Layer * layer, QMouseEvent * event);
		LayerToolFuncStatus release_(Layer * layer, QMouseEvent * event);
	};



} /* namespace SlavGPS */




/*
  OSM definition is a TMS derivative, (Global Mercator profile with Flipped Y)
  http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
  http://wiki.openstreetmap.org/wiki/TMS
  http://wiki.osgeo.org/wiki/Tile_Map_Service_Specification
*/

void maps_layer_init();
void maps_layer_set_autodownload_default(bool autodownload);
void maps_layer_set_cache_default(SlavGPS::MapsCacheLayout layout);
SlavGPS::MapTypeID maps_layer_get_default_map_type();
void maps_layer_register_map_source(SlavGPS::MapSource * map);


char * maps_layer_default_dir();
std::string& maps_layer_default_dir_2();




#endif /* #ifndef _SG_LAYER_MAPS_H */
