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




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string>
#include <cstdint>

#include "coord.h"
#include "layer.h"
#include "layer_tool.h"
#include "layer_interface.h"
#include "map_source.h"
#include "mapcoord.h"
#include "vikmapslayer_compat.h"

#ifdef HAVE_SQLITE3_H
#include "sqlite3.h"
#endif




namespace SlavGPS {




	class Viewport;




	enum class MapsCacheLayout {
		Viking = 0, /* CacheDir/t<MapId>s<VikingZoom>z0/X/Y (NB no file extension) - Legacy default layout. */
		OSM,        /* CacheDir/<OptionalMapName>/OSMZoomLevel/X/Y.ext (Default ext=png). */
		Num         /* Last enum. */
	};




	enum class MapDownloadMode {
		MissingOnly = 0,    /* Download only missing maps. */
		MissingAndBad,      /* Download missing and bad maps. */
		New,                /* Download missing maps that are newer on server only. */
		All,                /* Download all maps. */
		DownloadAndRefresh  /* Download missing maps and refresh cache. */
	};




	class LayerMapInterface : public LayerInterface {
	public:
		LayerMapInterface();
		Layer * unmarshall(uint8_t * data, size_t data_len, Viewport * viewport);
		LayerToolContainer * create_tools(Window * window, Viewport * viewport);
	};




	class LayerMap : public Layer {
	public:
		LayerMap();
		~LayerMap();

		static void init(void);

		/* Layer interface methods. */
		void post_read(Viewport * viewport, bool from_file);
		void draw(Viewport * viewport);
		void draw_section(Viewport * viewport, const Coord & coord_ul, const Coord & coord_br);
		QString get_tooltip();
		void add_menu_items(QMenu & menu);
		bool set_param_value(uint16_t id, const SGVariant & param_value, bool is_file_operation);
		SGVariant get_param_value(param_id_t id, bool is_file_operation) const;

		QString get_map_label(void) const;

		void set_cache_dir(const QString & dir);
		void mkdir_if_default_dir(void);
		void set_file_full_path(const QString & new_file_full_path);

		bool set_map_type_id(MapTypeID type_id);
		MapTypeID get_map_type_id(void) const;
		static MapTypeID get_default_map_type_id(void);

		void start_download_thread(Viewport * viewport, const Coord & coord_ul, const Coord & coord_br, MapDownloadMode map_download_mode);
		void download(Viewport * viewport, bool only_new);
		void download_section(const Coord & coord_ul, const Coord & coord_br, double zoom);

		void download_onscreen_maps(MapDownloadMode map_download_mode);

		static void weak_ref_cb(void * ptr, void * dead_vml);


		static void set_autodownload_default(bool autodownload);
		static void set_cache_default(MapsCacheLayout layout);


		MapTypeID map_type_id = MapTypeID::Initial;
		QString cache_dir;
		MapsCacheLayout cache_layout = MapsCacheLayout::Viking;
		int32_t alpha = 0;

		int mapzoom_id = 0;
		double xmapzoom = 0.0;
		double ymapzoom = 0.0;

		bool autodownload = false;
		bool adl_only_missing = false;

		Coord * last_center = NULL;
		double last_xmpp = 0.0;
		double last_ympp = 0.0;

		/* TODO: Should this be 0 or -1? */
		int dl_tool_x = -1;
		int dl_tool_y = -1;

		QMenu * dl_right_click_menu = NULL;

		Coord redownload_ul; /* Right click menu only. */
		Coord redownload_br;

		Viewport * redownload_viewport = NULL;
		QString file_full_path;

#ifdef HAVE_SQLITE3_H
		sqlite3 * mbtiles = NULL;
#endif

	private:
		int how_many_maps(const Coord & coord_ul, const Coord & coord_br, double zoom, MapDownloadMode map_download_mode);
		void download_section_sub(const Coord & coord_ul, const Coord & coord_br, double zoom, MapDownloadMode map_download_mode);

		bool try_draw_scale_down(Viewport * viewport, TileInfo ulm, int viewport_x, int viewport_y, int tilesize_x_ceil, int tilesize_y_ceil, double xshrinkfactor, double yshrinkfactor, const QString & map_name, QString & tile_file_full_path);
		bool try_draw_scale_up(Viewport * viewport, TileInfo ulm, int viewport_x, int viewport_y, int tilesize_x_ceil, int tilesize_y_ceil, double xshrinkfactor, double yshrinkfactor, const QString & map_name, QString & path_buf);

		bool should_start_autodownload(Viewport * viewport);

		QPixmap * get_pixmap_ref(const QString & map_type_string, TileInfo * mapcoord, QString & tile_file_full_path, double scale_x, double scale_y);
		QPixmap * get_pixmap_ref(const QString & map_type_string, TileInfo * mapcoord, double scale_x, double scale_y);
		QPixmap * create_mbtiles_pixmap(int xx, int yy, int zoom);
		QPixmap * create_pixmap_from_metatile(int xx, int yy, int zz);
		QPixmap * create_pixmap_from_file(const QString & file_full_path);

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
	};




	class LayerToolMapsDownload : public LayerTool {
	public:
		LayerToolMapsDownload(Window * window, Viewport * viewport);

		ToolStatus handle_mouse_click(Layer * layer, QMouseEvent * event);
		ToolStatus handle_mouse_release(Layer * layer, QMouseEvent * event);
	};




	class MapSources {
	public:
		static void register_map_source(MapSource * map_source);
	};




} /* namespace SlavGPS */




/*
  OSM definition is a TMS derivative, (Global Mercator profile with Flipped Y)
  http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
  http://wiki.openstreetmap.org/wiki/TMS
  http://wiki.osgeo.org/wiki/Tile_Map_Service_Specification
*/




#endif /* #ifndef _SG_LAYER_MAP_H_ */
