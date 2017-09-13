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
 */

#ifndef _SG_LAYER_MAPNIK_H_
#define _SG_LAYER_MAPNIK_H_




#include <cstdint>

#include "layer.h"
#include "mapnik_interface.h"




namespace SlavGPS {




	void vik_mapnik_layer_init(void);
	void vik_mapnik_layer_post_init(void);
	void vik_mapnik_layer_uninit(void);




	class LayerMapnikInterface : public LayerInterface {
	public:
		LayerMapnikInterface();
		Layer * unmarshall(uint8_t * data, int len, Viewport * viewport);
	};




	class LayerMapnik : public Layer {
	public:
		LayerMapnik();
		~LayerMapnik();


		/* Layer interface methods. */
		void post_read(Viewport * viewport, bool from_file);
		void draw(Viewport * viewport);
		QString tooltip();
		void add_menu_items(QMenu & menu);
		bool set_param_value(uint16_t id, const SGVariant & param_value, bool is_file_operation);
		SGVariant get_param_value(param_id_t id, bool is_file_operation) const;




		void set_file_xml(const QString & name);
		void set_file_css(const QString & name);
		void set_cache_dir(const QString & name);

		bool carto_load(void);
		void possibly_save_pixmap(QPixmap * pixmap, TileInfo * ulm);
		void render(Coord * coord_ul, Coord * coord_br, TileInfo * ti_ul);
		void thread_add(TileInfo * ti_ul, Coord * coord_ul, Coord * coord_br, int x, int y, int z, int zoom, char const * name);
		QPixmap * load_pixmap(TileInfo * ulm, TileInfo * brm, bool * rerender);
		QPixmap * get_pixmap(TileInfo * ulm, TileInfo * brm);
		void rerender();
		void tile_info();
		bool feature_release(QMouseEvent * event, LayerTool * tool);




		QString filename_css; /* CartoCSS MML File - use 'carto' to convert into xml. */
		QString filename_xml;
		int32_t alpha = 0;

		unsigned int tile_size_x = 0; /* Y is the same as X ATM. */
		bool loaded = false;
		MapnikInterface * mi = NULL;
		unsigned int rerender_timeout = 0;

		bool use_file_cache = false;
		QString file_cache_dir;

		Coord rerender_ul;
		Coord rerender_br;
		double rerender_zoom = 0;
		GtkWidget * right_click_menu = NULL;
	};




	void layer_mapnik_init(void);




	class LayerToolMapnikFeature : public LayerTool {
	public:
		LayerToolMapnikFeature(Window * window, Viewport * viewport);

		LayerToolFuncStatus handle_mouse_release(Layer * layer, QMouseEvent * event);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_MAPNIK_H_ */
