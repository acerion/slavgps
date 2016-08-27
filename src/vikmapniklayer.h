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

#ifndef _SG_MAPNIKLAYER_H_
#define _SG_MAPNIKLAYER_H_




#include <cstdint>

#include "viklayer.h"
#include "mapnik_interface.h"




namespace SlavGPS {




	void vik_mapnik_layer_init(void);
	void vik_mapnik_layer_post_init(void);
	void vik_mapnik_layer_uninit(void);




	class LayerMapnik : public Layer {
	public:
		LayerMapnik();
		LayerMapnik(Viewport * viewport);
		~LayerMapnik();


		/* Layer interface methods. */
		void post_read(Viewport * viewport, bool from_file);
		void draw(Viewport * viewport);
		char const * tooltip();
		void add_menu_items(GtkMenu * menu, void * panel);
		bool set_param(uint16_t id, VikLayerParamData data, Viewport * viewport, bool is_file_operation);
		VikLayerParamData get_param(uint16_t id, bool is_file_operation) const;




		void set_file_xml(char const * name);
		void set_file_css(char const * name);
		void set_cache_dir(char const * name);
		bool carto_load(Viewport * viewport);
		void possibly_save_pixbuf(GdkPixbuf * pixbuf, TileInfo * ulm);
		void render(VikCoord * ul, VikCoord * br, TileInfo * ulm);
		void thread_add(TileInfo * mul, VikCoord * ul, VikCoord * br, int x, int y, int z, int zoom, char const * name);
		GdkPixbuf * load_pixbuf(TileInfo * ulm, TileInfo * brm, bool * rerender);
		GdkPixbuf * get_pixbuf(TileInfo * ulm, TileInfo * brm);
		void rerender();
		void tile_info();
		bool feature_release(GdkEventButton * event, LayerTool * tool);




		char * filename_css = NULL; /* CartoCSS MML File - use 'carto' to convert into xml. */
		char * filename_xml = NULL;
		uint8_t alpha = 0;

		unsigned int tile_size_x = 0; /* Y is the same as X ATM. */
		bool loaded = false;
		MapnikInterface * mi = NULL;
		unsigned int rerender_timeout = 0;

		bool use_file_cache = false;
		char * file_cache_dir = NULL;

		VikCoord rerender_ul;
		VikCoord rerender_br;
		double rerender_zoom = 0;
		GtkWidget * right_click_menu = NULL;
	};




	void layer_mapnik_init(void);




}




#endif /* #ifndef _SG_MAPNIKLAYER_H_ */
