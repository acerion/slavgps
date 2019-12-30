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
 */

#ifndef _SG_LAYER_MAP_H_
#define _SG_LAYER_MAP_H_




#include <cstdint>




#include <QComboBox>




#include "coord.h"
#include "layer.h"
#include "layer_tool.h"
#include "layer_interface.h"
#include "map_source.h"
#include "map_cache.h"
#include "mapcoord.h"

#ifdef HAVE_SQLITE3_H
#include "sqlite3.h"
#endif




namespace SlavGPS {




	enum {
		/* Must be kept as zero for backwards compatibility
		   reasons. Not a big deal, just don't change it. */
		LAYER_MAP_ZOOM_ID_USE_VIKING_SCALE = 0,
	};




	class GisViewport;
	class MapCacheObj;




	class TileGeometry {
	public:
		void scale_up(int scale_factor);

		QPixmap pixmap;

		/* x/y coordinates in target viewport, where the pixmap will be drawn. */
		fpixel dest_x = 0.0;
		fpixel dest_y = 0.0;

		fpixel begin_x = 0.0;
		fpixel begin_y = 0.0;
		fpixel width = 0.0;
		fpixel height = 0.0;
	};




	class PixmapScale {
	public:
		PixmapScale(double x, double y);
		bool condition_1(void) const;
		bool condition_2(void) const;

		void scale_down(int scale_factor);
		void scale_up(int scale_factor);

		double x = 0.0;
		double y = 0.0;
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
		void post_read(GisViewport * gisview, bool from_file);
		void draw_tree_item(GisViewport * gisview, bool highlight_selected, bool parent_is_selected);
		sg_ret draw_section(GisViewport * gisview, const Coord & coord_ul, const Coord & coord_br);
		QString get_tooltip(void) const;
		bool menu_add_type_specific_operations(QMenu & menu, bool tree_view_context_menu) override;
		bool set_param_value(param_id_t param_id, const SGVariant & param_value, bool is_file_operation);
		SGVariant get_param_value(param_id_t param_id, bool is_file_operation) const override;

		QString get_map_label(void) const;

		void set_cache_dir(const QString & dir);
		void mkdir_if_default_dir(void);
		void set_file_full_path(const QString & new_file_full_path);

		bool set_map_type_id(MapTypeID type_id);
		MapTypeID get_map_type_id(void) const;
		static MapTypeID get_default_map_type_id(void);

		void start_download_thread(GisViewport * gisview, const Coord & coord_ul, const Coord & coord_br, MapDownloadMode map_download_mode);
		void download(GisViewport * gisview, bool only_new);
		void download_section(const Coord & coord_ul, const Coord & coord_br, const VikingScale & viking_scale);

		void download_onscreen_maps(MapDownloadMode map_download_mode);

		/* Check if given tile is visible in viewport.
		   Otherwise redraw of viewport is not needed. */
		bool is_tile_visible(const TileInfo & tile_info);

		VikingScale calculate_viking_scale(const GisViewport * gisview);

		static void set_autodownload_default(bool autodownload);
		static void set_cache_default(MapCacheLayout layout);


		static QString get_cache_filename(const MapCacheObj & map_cache_obj, MapTypeID map_type_id, const QString & map_type_string, const TileInfo & tile_info, const QString & file_extension);


		/*
		  Draw grid lines at viewport coordinates x/y.  The
		  grid starts at x_begin/y_begin (inclusive) and ends
		  and x_end/y_end (exclusive).

		  The grid lines are drawn at delta_x/delta_y intervals.
		*/
		static void draw_grid(GisViewport * gisview, const QPen & pen, fpixel viewport_x, fpixel viewport_y, fpixel x_begin, fpixel delta_x, fpixel x_end, fpixel y_begin, fpixel delta_y, fpixel y_end, double tile_width, double tile_height);


		MapTypeID map_type_id = MapTypeID::Initial;
		QString cache_dir;
		MapCacheLayout cache_layout = MapCacheLayout::Viking;
		int alpha = 0;


		int map_zoom_id = LAYER_MAP_ZOOM_ID_USE_VIKING_SCALE; /* This member is used as array index, so make sure to use with non-sparse enum values. */
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

#ifdef HAVE_SQLITE3_H
		sqlite3 * sqlite_handle = NULL;
#endif

	private:
		int how_many_maps(const Coord & coord_ul, const Coord & coord_br, const VikingScale & viking_scale, MapDownloadMode map_download_mode);
		void download_section_sub(const Coord & coord_ul, const Coord & coord_br, const VikingScale & viking_scale, MapDownloadMode map_download_mode);

		TileGeometry find_tile(const TileInfo & tile_info, const TileGeometry & tile_geometry, const PixmapScale & pixmap_scale, const QString & map_type_string, int scale_factor);
		TileGeometry find_scaled_down_tile(const TileInfo & tile_info, const TileGeometry & tile_geometry, const PixmapScale & pixmap_scale, const QString & map_type_string);
		TileGeometry find_scaled_up_tile(const TileInfo & tile_info, const TileGeometry & tile_geometry, const PixmapScale & pixmap_scale, const QString & map_type_string);

		void draw_existence(GisViewport * gisview, const TileInfo & tile_info, const TileGeometry & tile_geometry, const MapSource * map_source, const MapCacheObj & map_cache_obj);

		bool should_start_autodownload(GisViewport * gisview);

		QPixmap get_tile_pixmap(const QString & map_type_string, const TileInfo & tile_info, const PixmapScale & scale);
		QPixmap create_pixmap_from_file(const QString & file_full_path);

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
		virtual ToolStatus internal_handle_mouse_click(Layer * layer, QMouseEvent * event) override;
		virtual ToolStatus internal_handle_mouse_release(Layer * layer, QMouseEvent * event) override;
	};




	class MapSources {
	public:
		static void register_map_source(MapSource * map_source);
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




/*
  OSM definition is a TMS derivative, (Global Mercator profile with Flipped Y)
  http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
  http://wiki.openstreetmap.org/wiki/TMS
  http://wiki.osgeo.org/wiki/Tile_Map_Service_Specification
*/




#endif /* #ifndef _SG_LAYER_MAP_H_ */
