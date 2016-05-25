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

#ifndef _VIKING_MAPSLAYER_H
#define _VIKING_MAPSLAYER_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <stdbool.h>
#include <stdint.h>

#include "vikcoord.h"
#include "viklayer.h"
#include "vikviewport.h"
#include "vikmapsource.h"
#include "mapcoord.h"
#include "vikmapslayer_compat.h"

#ifdef HAVE_SQLITE3_H
#include "sqlite3.h"
#include <gio/gio.h>
#endif


using namespace SlavGPS;

#ifdef __cplusplus
extern "C" {
#endif


#define VIK_MAPS_LAYER_TYPE            (vik_maps_layer_get_type ())
#define VIK_MAPS_LAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_MAPS_LAYER_TYPE, VikMapsLayer))
#define VIK_MAPS_LAYER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_MAPS_LAYER_TYPE, VikMapsLayerClass))
#define IS_VIK_MAPS_LAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_MAPS_LAYER_TYPE))
#define IS_VIK_MAPS_LAYER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_MAPS_LAYER_TYPE))

typedef struct _VikMapsLayerClass VikMapsLayerClass;
struct _VikMapsLayerClass
{
  VikLayerClass object_class;
};

GType vik_maps_layer_get_type ();

typedef struct {
	VikLayer vl;
} VikMapsLayer;

typedef enum {
  VIK_MAPS_CACHE_LAYOUT_VIKING=0, // CacheDir/t<MapId>s<VikingZoom>z0/X/Y (NB no file extension) - Legacy default layout
  VIK_MAPS_CACHE_LAYOUT_OSM,      // CacheDir/<OptionalMapName>/OSMZoomLevel/X/Y.ext (Default ext=png)
  VIK_MAPS_CACHE_LAYOUT_NUM       // Last enum
} VikMapsCacheLayout;

// OSM definition is a TMS derivative, (Global Mercator profile with Flipped Y)
//  http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
//  http://wiki.openstreetmap.org/wiki/TMS
//  http://wiki.osgeo.org/wiki/Tile_Map_Service_Specification

void maps_layer_init();
void maps_layer_set_autodownload_default( bool autodownload);
void maps_layer_set_cache_default(VikMapsCacheLayout layout);
MapTypeID vik_maps_layer_get_default_map_type();
void maps_layer_register_map_source(MapSource *map);
void vik_maps_layer_download_section(VikMapsLayer *vml, VikCoord *ul, VikCoord *br, double zoom);
MapTypeID vik_maps_layer_get_map_type(VikMapsLayer *vml);
void vik_maps_layer_set_map_type(VikMapsLayer *vml, MapTypeID type_id);
char *maps_layer_default_dir();
void vik_maps_layer_download(VikMapsLayer *vml, Viewport * viewport, bool only_new);

#ifdef __cplusplus
}
#endif





namespace SlavGPS {





	class LayerMaps : public Layer {
	public:
		LayerMaps(VikLayer * vl);


		/* Layer interface methods. */
		void post_read(Viewport * viewport, bool from_file);
		void draw(Viewport * viewport);
		void draw_section(Viewport * viewport, VikCoord *ul, VikCoord *br);
		char const * tooltip();
		void marshall(uint8_t ** data, int * len);
		void add_menu_items(GtkMenu * menu, void * vlp);



		char * get_map_label();
		int how_many_maps(Viewport * viewport, VikCoord *ul, VikCoord *br, double zoom, int redownload_mode);
		void download_section_sub(VikCoord *ul, VikCoord *br, double zoom, int redownload_mode);
		void set_cache_dir(char const * dir);
		void mkdir_if_default_dir();
		void set_file(char const * name);


		int map_index;
		char * cache_dir;
		VikMapsCacheLayout cache_layout;
		uint8_t alpha;


		unsigned int mapzoom_id;
		double xmapzoom, ymapzoom;

		bool autodownload;
		bool adl_only_missing;


		VikCoord *last_center;
		double last_xmpp;
		double last_ympp;


		int dl_tool_x, dl_tool_y;

		GtkMenu * dl_right_click_menu;
		VikCoord redownload_ul, redownload_br; /* right click menu only */
		Viewport * redownload_viewport;
		char *filename;
#ifdef HAVE_SQLITE3_H
		sqlite3 * mbtiles;
#endif

	};





}





#endif
