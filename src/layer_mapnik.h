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




#include <QObject>




#include "layer.h"
#include "layer_interface.h"
#include "layer_tool.h"
#include "layer_mapnik_wrapper.h"




namespace SlavGPS {




	class GisViewport;




	class LayerMapnikInterface : public LayerInterface {
	public:
		LayerMapnikInterface();
		Layer * unmarshall(Pickle & pickle, GisViewport * gisview);
		LayerToolContainer create_tools(Window * window, GisViewport * gisview);
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
		sg_ret post_read(GisViewport * gisview, bool from_file) override;
		void draw_tree_item(GisViewport * gisview, bool highlight_selected, bool parent_is_selected);
		QString get_tooltip(void) const;
		sg_ret menu_add_type_specific_operations(QMenu & menu, bool in_tree_view) override;
		bool set_param_value(param_id_t param_id, const SGVariant & param_value, bool is_file_operation);
		SGVariant get_param_value(param_id_t param_id, bool is_file_operation) const override;

		sg_ret set_tile_size(int tile_size);

		/**
		   Common tile render function which can run in separate thread or in main thread
		*/
		void render_tile_now(const TileInfo & tile_info);

		LayerTool::Status feature_release(QMouseEvent * event, LayerTool * tool);

	public slots:
		void tile_info_cb(void);
		void flush_map_cache_cb(void);
		void reload_map_cb(void);
		void run_carto_cb(void);

		/* Show Mapnik layer's parameters/meta info. */
		void mapnik_layer_information_cb(void);

		void about_mapnik_cb(void);
		void rerender_tile_cb(void);

	private:
		static void init_wrapper(void);

		void set_file_xml(const QString & file_full_path);
		void set_file_css(const QString & name);
		void set_cache_dir(const QString & file_full_path);

		void possibly_save_pixmap(QPixmap & pixmap, const TileInfo & tile_info) const;

		void queue_rendering_in_background(const TileInfo & tile_info, const QString & file_full_path);
		QPixmap load_pixmap(const TileInfo & tile_info, bool & rerender) const;
		QPixmap get_pixmap(const TileInfo & tile_info);

		bool should_run_carto(void) const;
		sg_ret carto_load(void);

		/* Draw single tile to viewport. */
		sg_ret draw_tile(GisViewport * gisview, const TileInfo & tile_info);

		/* Get range of tiles that will cover current
		   viewport.  Also get tile info of first tile in that
		   range (upper-left tile). */
		sg_ret get_tiles_range(const GisViewport * gisview, TilesRange & range, TileInfo & tile_info_ul);

		void draw_grid(GisViewport * gisview, const TilesRange & range, const TileInfo & tile_info_ul) const;

		QString css_file_full_path; /* CartoCSS MML File - use 'carto' to convert into xml. */
		QString xml_map_file_full_path;

		bool xml_map_file_loaded = false;

		/* Coordinates of mouse right-click-and-release. */
		LatLon clicked_lat_lon;
		/* Scale at the moment of right-click-and-release. */
		VikingScale clicked_viking_scale;

		ImageAlpha alpha;

		int tile_size_x = 0; /* Y is the same as X ATM. */

		MapnikWrapper mw;
		unsigned int rerender_timeout = 0;

		bool use_file_cache = false;
		QString file_cache_dir;
	};




	class LayerToolMapnikFeature : public LayerTool {
	public:
		LayerToolMapnikFeature(Window * window, GisViewport * gisview);

		SGObjectTypeID get_tool_id(void) const override;
		static SGObjectTypeID tool_id(void);

		LayerTool::Status handle_mouse_release(Layer * layer, QMouseEvent * event);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_MAPNIK_H_ */
