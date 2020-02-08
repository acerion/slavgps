/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2016-2020, Kamil Ignacak <acerion@wp.pl>
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

#ifndef _SG_LAYER_MAP_H_
#define _SG_LAYER_MAP_H_




#include <cstdint>




#include <QComboBox>




#include "coord.h"
#include "layer.h"
#include "layer_tool.h"
#include "layer_interface.h"
#include "layer_map_source.h"
#include "map_cache.h"
#include "mapcoord.h"




namespace SlavGPS {




	enum {
		/* Must be kept as zero for backwards compatibility
		   reasons. Not a big deal, just don't change it. */
		LAYER_MAP_ZOOM_ID_FOLLOW_VIEWPORT_ZOOM_LEVEL = 0,
	};




	class GisViewport;
	class MapCacheObj;




	class TileGeometry {
	public:
		void resize_up(int scale_factor);

		QPixmap pixmap;

		/*
		  x/y coordinates in target viewport, from which a
		  tile pixmap will be drawn (where the upper-left
		  corner of pixmap will be drawn).

		  These fields match Qt's coordinate system (beginning
		  in upper-left corner).
		*/
		fpixel viewport_begin_x = 0.0;
		fpixel viewport_begin_y = 0.0;

		/*
		  x/y coordinates of first pixel of the pixmap that
		  should be drawn. Most of time it will be zero (for
		  pixmaps that should be drawn somewhat in the center
		  of viewport), but not always: for some pixmaps that
		  are in topmost or leftmost row the values may be
		  non-zero.

		  Pixels that are to the left of pixmap_begin_x will
		  not be drawn.

		  Pixels that are above of pixmap_begin_y will not be
		  drawn.
		*/
		fpixel pixmap_begin_x = 0.0;
		fpixel pixmap_begin_y = 0.0;

		/*
		  Size of pixmap. Always full size of pixmap, even if
		  pixmap_begin_x/y is non-zero.
		*/
		fpixel total_pixmap_width = 0.0;
		fpixel total_pixmap_height = 0.0;
	};




	class TilePixmapResize {
	public:
		TilePixmapResize(double horiz, double vert);
		bool resize_factors_in_allowed_range(void) const;
		bool resize_factors_in_existence_only_range(void) const;

		void resize_down(int resize_times);
		void resize_up(int resize_times);

		double horiz_resize = 0.0;
		double vert_resize = 0.0;
	};




	enum class MapDownloadMode {
		MissingOnly = 0,    /* Download only missing maps. */
		MissingAndBad,      /* Download missing and bad maps. */
		New,                /* Download missing maps that are newer on server only. */
		All,                /* Download all maps. */
		DownloadAndRefresh  /* Download missing maps and refresh cache. */
	};
	QString to_string(MapDownloadMode download_mode);




	class LayerMapInterface : public LayerInterface {
	public:
		LayerMapInterface();
		Layer * unmarshall(Pickle & pickle, GisViewport * gisview);
		LayerToolContainer create_tools(Window * window, GisViewport * gisview);
	};




	class LayerMap : public Layer {
		Q_OBJECT
	public:
		LayerMap();
		~LayerMap();

		static void init(void);

		/* Layer interface methods. */
		sg_ret post_read(GisViewport * gisview, bool from_file) override;
		void draw_tree_item(GisViewport * gisview, bool highlight_selected, bool parent_is_selected);
		sg_ret draw_section(GisViewport * gisview, const Coord & coord_ul, const Coord & coord_br);
		QString get_tooltip(void) const;
		sg_ret menu_add_type_specific_operations(QMenu & menu, bool in_tree_view) override;
		bool set_param_value(param_id_t param_id, const SGVariant & param_value, bool is_file_operation);
		SGVariant get_param_value(param_id_t param_id, bool is_file_operation) const override;

		/**
		   @brief Get user-facing label (name) of this map's source type

		   Translatable. Visible in GUI.
		*/
		QString get_map_type_ui_label(void) const;

		void set_cache_dir(const QString & dir);
		void mkdir_if_default_dir(void);
		void set_file_full_path(const QString & new_file_full_path);

		/**
		   @brief Set type of this map layer
		*/
		sg_ret set_map_type_id(MapTypeID type_id);

		/**
		   @brief Get type of this map layer
		*/
		MapTypeID map_type_id(void) const { return this->m_map_type_id; };

		static MapTypeID get_default_map_type_id(void);

		void start_download_thread(GisViewport * gisview, const Coord & coord_ul, const Coord & coord_br, MapDownloadMode map_download_mode);
		void download(GisViewport * gisview, bool only_new);
		void download_section(const Coord & coord_ul, const Coord & coord_br, const VikingScale & viking_scale);

		void download_onscreen_maps(MapDownloadMode map_download_mode);

		/* Check if given tile is visible in viewport.
		   Otherwise redraw of viewport is not needed. */
		bool is_tile_visible(const TileInfo & tile_info);



		/**
		   Set default value of parameter telling whether
		   during first run of the problem user has agreed to
		   auto-downloading of maps from internet
		*/
		static void set_autodownload_default(bool autodownload);

		/**
		   Set default layout of maps cache
		*/
		static void set_cache_default(MapCacheLayout layout);


		static QString get_cache_filename(const MapCacheObj & map_cache_obj, MapTypeID map_type_id, const QString & map_type_string, const TileInfo & tile_info, const QString & file_extension);


		/**
		   @brief Draw grid lines for Map Layer

		   The x/y beginning of the lines is at @param
		   first_viewport_x and @param first_viewport_y.

		   The lines are spaced at @param grid_width and
		   @param grid_height.

		   Tile ranges (in vertical and horizontal direction)
		   from @param tiles_range allows for control of how
		   many grid lines will be drawn.
		*/
		static void draw_grid(GisViewport & gisview, const QPen & pen, fpixel first_viewport_x, fpixel first_viewport_y, fpixel tile_width, fpixel tile_height, const TilesRange & tiles_range);


		MapSource * map_source(void) const { return this->m_map_source; };


		/**
		   @brief Get viking scale that will be passed to map
		   tiles source

		   We will want to get from the source the tiles with
		   that scale.
		*/
		VikingScale get_desired_viking_scale(const GisViewport & gisview) const;

		QString cache_dir;
		MapCacheLayout cache_layout = MapCacheLayout::Viking;
		int alpha = 0;


		int map_zoom_id = LAYER_MAP_ZOOM_ID_FOLLOW_VIEWPORT_ZOOM_LEVEL; /* This member is used as array index, so make sure to use with non-sparse enum values. */
		double map_zoom_x = 0.0;
		double map_zoom_y = 0.0;

		bool autodownload = false;
		bool adl_only_missing = false;

		Coord * last_center = NULL;
		VikingScale last_map_scale;

		/* These event coordinates indicate pixel in Qt's
		   coordinate system, where beginning is in top-left
		   corner of screen. */
		int dl_tool_x = -1;
		int dl_tool_y = -1;

		QMenu * dl_right_click_menu = NULL;

		Coord redownload_ul; /* Right click menu only. */
		Coord redownload_br;

		GisViewport * redownload_gisview = NULL;
		QString file_full_path;

	private:

		/**
		   @brief Get resizing factor for tile pixmaps that
		   will be used when resizing of the pixmaps will be
		   necessary
		*/
		TilePixmapResize get_desired_pixmap_resize(const GisViewport & gisview) const;

		bool validate_tile_pixmap_resize(const TilePixmapResize & tile_pixmap_resize, bool & existence_only) const;

		int how_many_maps(const Coord & coord_ul, const Coord & coord_br, const VikingScale & viking_scale, MapDownloadMode map_download_mode);
		void download_section_sub(const Coord & coord_ul, const Coord & coord_br, const VikingScale & viking_scale, MapDownloadMode map_download_mode);

		TileGeometry find_tile(const TileInfo & tile_info, const TileGeometry & tile_geometry, const TilePixmapResize & tile_pixmap_resize);

		/**
		   Look for pixmap representing given @param
		   tile_info. Search in set of tiles that have zoom
		   level lower than current zoom level of @param
		   tile_info. If found, the pixmap will be resized
		   down to match viewport's current zoom level.
		*/
		TileGeometry find_resized_down_tile(const TileInfo & tile_info, const TileGeometry & tile_geometry, const TilePixmapResize & tile_pixmap_resize);

		/**
		   Look for pixmap representing given @param
		   tile_info. Search in set of tiles that have zoom
		   level higher than current zoom level of @param
		   tile_info. If found, the pixmap will be resized up
		   to match viewport's current zoom level.
		*/
		TileGeometry find_resized_up_tile(const TileInfo & tile_info, const TileGeometry & tile_geometry, const TilePixmapResize & tile_pixmap_resize);

		void draw_existence(GisViewport * gisview, const TileInfo & tile_info, const TileGeometry & tile_geometry, const MapCacheObj & map_cache_obj);

		bool should_start_autodownload(const GisViewport * gisview);

		/*
		  Get pixmap of a tile. If necessary, ask map source
		  to download the pixmap from remote server, or ask
		  map source to retrieve the pixmap from local
		  database.
		*/
		QPixmap get_tile_pixmap_with_stretch(const TileInfo & tile_info, const TilePixmapResize & tile_pixmap_resize);


		/**
		   @brief Calculate where in viewport should be put a
		   beginning of pixmap corresponding to given @param
		   tile_info

		   Calculate TileGeometry::viewport_begin_x and TileGeometry::viewport_begin_y of given @param tile_geometry.
		*/
		sg_ret calculate_tile_geometry_viewport_begin(const GisViewport & gisview, const TileInfo & tile_info, fpixel tile_width, fpixel tile_height, TileGeometry & tile_geometry);


		MapTypeID m_map_type_id = MapTypeID::Initial;
		MapSource * m_map_source = nullptr;

	public slots:
		void download_all_cb(void);
		void redownload_new_cb(void);
		void redownload_all_cb(void);
		void redownload_all_onscreen_maps_cb(void);
		void redownload_bad_cb(void);
		void tile_info_cb(void);
		void download_missing_onscreen_maps_cb(void);
		void download_new_onscreen_maps_cb(void);

		void about_cb(void);
		void flush_cb(void);

		sg_ret handle_downloaded_tile_cb(void);
	};




	class LayerToolMapsDownload : public LayerTool {
	public:
		LayerToolMapsDownload(Window * window, GisViewport * gisview);

		SGObjectTypeID get_tool_id(void) const override;
		static SGObjectTypeID tool_id(void);

	private:
		LayerTool::Status handle_mouse_click(Layer * layer, QMouseEvent * event) override;
		LayerTool::Status handle_mouse_release(Layer * layer, QMouseEvent * event) override;
	};




	class MapSources {
	public:
		static void register_map_source_maker(MapSourceMaker map_source_maker_fn, MapTypeID map_type_id, const QString & map_type_ui_label);
	};




	class DownloadMethodsAndZoomsDialog : public BasicDialog {
		Q_OBJECT
	public:
		DownloadMethodsAndZoomsDialog() {};
		DownloadMethodsAndZoomsDialog(const QString & title,
					      const std::vector<VikingScale> & viking_scales,
					      const std::vector<MapDownloadMode> & download_modes,
					      QWidget * parent = NULL);

		void preselect(int smaller_zoom_idx, int larger_zoom_idx, int download_mode_idx);

		int get_smaller_zoom_idx(void) const; /* Smaller zoom - closer to totally zoomed out. */
		int get_larger_zoom_idx(void) const;  /* Larger zoom - closer to totally zoomed in. */
		int get_download_mode_idx(void) const;

	private:
		QComboBox * smaller_zoom_combo = NULL;
		QComboBox * larger_zoom_combo = NULL;
		QComboBox * download_mode_combo = NULL;
	};




	void tile_info_add_file_info_strings(QStringList & items, const QString & tile_file_full_path);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_MAP_H_ */
