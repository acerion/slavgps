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




#include <QObject>




#include "layer.h"
#include "layer_interface.h"
#include "layer_tool.h"
#include "mapnik_interface.h"




namespace SlavGPS {




	class Viewport;




	class LayerMapnikInterface : public LayerInterface {
	public:
		LayerMapnikInterface();
		Layer * unmarshall(Pickle & pickle, Viewport * viewport);
		LayerToolContainer * create_tools(Window * window, Viewport * viewport);
	};




	class LayerMapnik : public Layer {
		Q_OBJECT
	public:
		LayerMapnik();
		~LayerMapnik();

		/* Module initialization/deinitialization. */
		static void init(void);
		static void post_init(void);
		static void uninit(void);


		/* Layer interface methods. */
		void post_read(Viewport * viewport, bool from_file);
		void draw_tree_item(Viewport * viewport, bool highlight_selected, bool parent_is_selected);
		QString get_tooltip(void) const;
		void add_menu_items(QMenu & menu);
		bool set_param_value(param_id_t param_id, const SGVariant & param_value, bool is_file_operation);
		SGVariant get_param_value(param_id_t param_id, bool is_file_operation) const override;




		void set_file_xml(const QString & name);
		void set_file_css(const QString & name);
		void set_cache_dir(const QString & name);

		bool carto_load(void);
		void possibly_save_pixmap(QPixmap & pixmap, const TileInfo & ulm);
		void render(const TileInfo & tile_info, const LatLon & lat_lon_ul, const LatLon & lat_lon_br);
		void thread_add(const TileInfo & tile_info, const LatLon & lat_lon_ul, const LatLon & lat_lon_br, const QString & file_name);
		QPixmap load_pixmap(const TileInfo & tile_info, bool * rerender) const;
		QPixmap get_pixmap(const TileInfo & tile_info);
		void render_tile(const TileInfo & tile_info);

		ToolStatus feature_release(QMouseEvent * event, LayerTool * tool);




		QString filename_css; /* CartoCSS MML File - use 'carto' to convert into xml. */
		QString filename_xml;
		int alpha = 0;

		int tile_size_x = 0; /* Y is the same as X ATM. */

		MapnikInterface * mi = NULL;
		unsigned int rerender_timeout = 0;

		bool use_file_cache = false;
		QString file_cache_dir;



	public slots:
		void tile_info_cb(void);
		void flush_map_cache_cb(void);
		void reload_map_cb(void);
		void run_carto_cb(void);
		void mapnik_information_cb(void);
		void about_mapnik_cb(void);
		void rerender_tile_cb(void);

	private:
		static void init_interface(void);

		bool map_file_loaded = false;

		/* Coordinates of mouse right-click-and-release. */
		LatLon clicked_lat_lon;
		/* Zoom level at the moment of right-click-and-release. */
		VikingZoomLevel clicked_viking_zoom_level;
	};




	class LayerToolMapnikFeature : public LayerTool {
	public:
		LayerToolMapnikFeature(Window * window, Viewport * viewport);

		ToolStatus handle_mouse_release(Layer * layer, QMouseEvent * event);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_MAPNIK_H_ */
