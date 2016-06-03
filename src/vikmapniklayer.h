/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (c) 2015, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _VIKING_MAPNIKLAYER_H
#define _VIKING_MAPNIKLAYER_H

#include <stdbool.h>
#include <stdint.h>

#include "viklayer.h"
#include "mapnik_interface.h"

#ifdef __cplusplus
extern "C" {
#endif


#define VIK_MAPNIK_LAYER_TYPE            (vik_mapnik_layer_get_type ())
#define VIK_MAPNIK_LAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_MAPNIK_LAYER_TYPE, VikMapnikLayer))
#define VIK_MAPNIK_LAYER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_MAPNIK_LAYER_TYPE, VikMapnikLayerClass))
#define IS_VIK_MAPNIK_LAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_MAPNIK_LAYER_TYPE))
#define IS_VIK_MAPNIK_LAYER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_MAPNIK_LAYER_TYPE))

typedef struct _VikMapnikLayerClass VikMapnikLayerClass;

GType vik_mapnik_layer_get_type ();

typedef struct _VikMapnikLayer VikMapnikLayer;

void vik_mapnik_layer_init (void);
void vik_mapnik_layer_post_init (void);
void vik_mapnik_layer_uninit (void);

#ifdef __cplusplus
}
#endif





namespace SlavGPS {





	class LayerMapnik : public Layer {
	public:
		LayerMapnik();
		LayerMapnik(VikLayer * vl);
		LayerMapnik(Viewport * viewport);


		/* Layer interface methods. */
		void post_read(Viewport * viewport, bool from_file);
		void draw(Viewport * viewport);
		char const * tooltip();
		void marshall(uint8_t ** data, int * len);
		void add_menu_items(GtkMenu * menu, void * vlp);
		void free_();




		void set_file_xml(char const * name);
		void set_file_css(char const * name);
		void set_cache_dir(char const * name);
		bool carto_load(VikViewport * vvp);
		void possibly_save_pixbuf(GdkPixbuf * pixbuf, TileInfo * ulm);
		void render(VikCoord * ul, VikCoord * br, TileInfo * ulm);
		void thread_add(TileInfo * mul, VikCoord * ul, VikCoord * br, int x, int y, int z, int zoom, char const * name);
		GdkPixbuf * load_pixbuf(TileInfo * ulm, TileInfo * brm, bool * rerender);
		GdkPixbuf * get_pixbuf(TileInfo * ulm, TileInfo * brm);
		void rerender();
		void tile_info();
		bool feature_release(GdkEventButton * event, Viewport * viewport);




		char * filename_css; // CartoCSS MML File - use 'carto' to convert into xml
		char * filename_xml;
		uint8_t alpha;

		unsigned int tile_size_x; // Y is the same as X ATM
		bool loaded;
		MapnikInterface * mi;
		unsigned int rerender_timeout;

		bool use_file_cache;
		char * file_cache_dir;

		VikCoord rerender_ul;
		VikCoord rerender_br;
		double rerender_zoom;
		GtkWidget * right_click_menu;
	};





}





#endif
