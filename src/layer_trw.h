/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (c) 2011-2013, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_LAYER_TRW_H_
#define _SG_LAYER_TRW_H_




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdint>
#include <list>
#include <unordered_map>
#include <deque>

#include <QStandardItem>
#include <QDialog>

#include "layer.h"
#include "layer_tool.h"
#include "layer_interface.h"
#include "layer_trw_definitions.h"
#include "layer_trw_dialogs.h"
#include "layer_trw_track.h"
#include "layer_trw_tracks.h"
#include "layer_trw_waypoint.h"
#include "layer_trw_waypoints.h"
#include "viewport.h"
#include "file.h"




#define VIK_SETTINGS_LIST_DATE_FORMAT "list_date_format"




namespace SlavGPS {




	class LayerTRW;
	class LayerTRWPainter;
	class LayersPanel;
	class Track;
	class TrackpointSearch;
	class WaypointSearch;
	class PropertiesDialogTP;
	class DataSource;




	/* A cached waypoint image. */
	/* This data structure probably should be put somewhere else - it could be reused in other modules. */
	class CachedPixmap {
	public:
		CachedPixmap() {};
		~CachedPixmap();
		QPixmap * pixmap = NULL;
		QString image_file_path;
	};




	/* Binary predicate for searching a pixmap (CachedPixmap) in pixmap cache container. */
	struct CachedPixmapCompareByPath {
	CachedPixmapCompareByPath(const QString & searched_full_path) : searched_full_path_(searched_full_path) { }
		bool operator()(CachedPixmap * item) const { return item->image_file_path == searched_full_path_; }
	private:
		const QString searched_full_path_;
	};




	class TRWMetadata {

	public:
		void set_author(const QString & new_author);
		void set_description(const QString & new_description);
		void set_keywords(const QString & new_keywords);
		void set_timestamp(const QString & new_timestamp);

		QString description;
		QString author;
		QString keywords;  /* TODO: handling/storing a GList of individual tags? */
		/* TODO: verify how the timestamp should work: when and how it should be set. */
		bool has_timestamp;
		QString timestamp; /* TODO: Consider storing as proper time_t. */
	};




	class LayerTRWInterface : public LayerInterface {
	public:
		LayerTRWInterface();
		Layer * unmarshall(uint8_t * data, size_t data_len, Viewport * viewport);
		LayerToolContainer * create_tools(Window * window, Viewport * viewport);
	};




	class LayerTRW : public Layer {
		Q_OBJECT
	public:
		LayerTRW();
		~LayerTRW();


		/* Layer interface methods. */
		void post_read(Viewport * viewport, bool from_file);
		QString get_tooltip();


		/* Methods for generic "Select" tool. */
		bool handle_select_tool_click(QMouseEvent * event, Viewport * viewport, LayerTool * tool);
		bool handle_select_tool_double_click(QMouseEvent * event, Viewport * viewport, LayerTool * tool);
		bool handle_select_tool_move(QMouseEvent * event, Viewport * viewport, LayerTool * tool);
		bool handle_select_tool_release(QMouseEvent * event, Viewport * viewport, LayerTool * tool);
		bool handle_select_tool_context_menu(QMouseEvent * event, Viewport * viewport);

		void handle_select_tool_click_do_track_selection(QMouseEvent * ev, LayerTool * tool, Track * track, TrackPoints::iterator & tp_iter);
		void handle_select_tool_click_do_waypoint_selection(QMouseEvent * ev, LayerTool * tool, Waypoint * wp);
		void handle_select_tool_double_click_do_waypoint_selection(QMouseEvent * ev, LayerTool * tool, Waypoint * wp);

		void set_menu_selection(LayerMenuItem selection);
		LayerMenuItem get_menu_selection();

		void marshall(uint8_t ** data, size_t * data_len);

		void cut_sublayer(TreeItem * item);
		void copy_sublayer(TreeItem * item, uint8_t ** data, unsigned int * len);
		bool paste_sublayer(TreeItem * item, uint8_t * data, size_t len);
		void delete_sublayer(TreeItem * item);

		void change_coord_mode(CoordMode dest_mode);

		time_t get_timestamp();

		void drag_drop_request(Layer * src, TreeIndex & src_item_index, void * GtkTreePath_dest_path);

		int read_layer_data(FILE * file, char const * dirpath);
		void write_layer_data(FILE * file) const;

		void add_menu_items(QMenu & menu);

		void add_children_to_tree(void);
		bool set_param_value(uint16_t id, const SGVariant & param_value, bool is_file_operation);
		SGVariant get_param_value(param_id_t id, bool is_file_operation) const;





		bool create_new_tracks(Track * orig, std::list<TrackPoints *> * tracks_data);

		void add_track(Track * trk);
		void add_route(Track * trk);
		void add_waypoint(Waypoint * wp);

		Tracks & get_track_items();
		Tracks & get_route_items();
		Waypoints & get_waypoint_items();

		LayerTRWTracks & get_tracks_node(void);
		LayerTRWTracks & get_routes_node(void);
		LayerTRWWaypoints & get_waypoints_node(void);

		LayerTRWTracks * tracks = NULL; /* Sub-node, under which all layer's tracks are shown. */
		LayerTRWTracks * routes = NULL; /* Sub-node, under which all layer's routes are shown. */
		LayerTRWWaypoints * waypoints = NULL; /* Sub-node, under which all layer's waypoints are shown. */


		bool get_tracks_visibility();
		bool get_routes_visibility();
		bool get_waypoints_visibility();

		bool is_empty();


		bool find_track_by_date(char const * date, Viewport * viewport, bool select);
		bool find_waypoint_by_date(char const * date, Viewport * viewport, bool select);

		/* Draw all items of the layer, without highlight. */
		void draw(Viewport * viewport);

		/* Draw all items of the layer, with highlight. */
		void draw_tree_item(Viewport * viewport, bool hl_is_allowed, bool hl_is_required);


		void find_maxmin(LatLonMinMax & min_max);
		bool find_center(Coord * dest);

		void set_statusbar_msg_info_tp(TrackPoints::iterator & tp_iter, Track * track);
		void set_statusbar_msg_info_wpt(Waypoint * wp);

		bool move_viewport_to_show_all(Viewport * viewport);


		bool new_waypoint(Window * parent, const Coord & default_coord);
		Track * new_track_create_common(const QString & new_name);
		Track * new_route_create_common(const QString & new_name);




		void cancel_tps_of_track(Track * trk);

		void reset_waypoints();

		QString new_unique_element_name(const QString & type_id, const QString& old_name);


		/* These are meant for use in file loaders (gpspoint.c, gpx.c, etc). */
		void add_waypoint_from_file(Waypoint * wp, const QString & wp_name);
		void add_waypoint_to_data_structure(Waypoint * wp, const QString & wp_name);
		void add_track_to_data_structure(Track * trk);
		void add_route_to_data_structure(Track * trk);
		void add_track_from_file(Track * trk, const QString & trk_name);
		void add_track_from_file2(Track * trk, const QString & trk_name);




		void move_item(LayerTRW * vtl_dest, sg_uid_t sublayer_uid, const QString & type_id);



		bool delete_track(Track * trk);
		bool delete_track_by_name(const QString & trk_name, bool is_route);
		bool delete_route(Track * trk);
		bool delete_waypoint(Waypoint * wp);
		bool delete_waypoint_by_name(const QString & wp_name);


		void delete_all_routes();
		void delete_all_tracks();
		void delete_all_waypoints();


		void smooth_it(Track * trk, bool flat);
		void wp_changed_message(int changed);



		void delete_selected_tp(Track * trk);

		void diary_open(char const * date_str);
		void astro_open(char const * date_str,  char const * time_str, char const * lat_str, char const * lon_str, char const * alt_str);



		std::list<Waypoint *> * create_waypoints_list();

		std::list<Track *> * create_tracks_list();
		std::list<Track *> * create_tracks_list(const QString & type_id);


		void trackpoint_properties_show();



		Trackpoint * search_nearby_tp(Viewport * viewport, int x, int y);
		Waypoint * search_nearby_wp(Viewport * viewport, int x, int y);

		bool get_nearby_snap_coordinates(Coord & point_coord, QMouseEvent * ev, Viewport * viewport);
		bool get_nearby_snap_coordinates_tp(Coord & point_coord, QMouseEvent * ev, Viewport * viewport);


		void sort_all();


		/* General handler of acquire requests.
		   Called by all acquire_from_X_cb() with specific data source. */
		void acquire_handler(DataSource * data_source);


		void set_coord_mode(CoordMode mode);
		CoordMode get_coord_mode(void) const;

		bool uniquify(LayersPanel * panel);

		void goto_coord(Viewport * viewport, const Coord & coord);

		void highest_wp_number_reset();
		void highest_wp_number_add_wp(const QString & new_wp_name);
		void highest_wp_number_remove_wp(const QString & old_wp_name);
		QString highest_wp_number_get();


		void tpwin_update_dialog_data();


		void dialog_shift(QDialog * dialog, const Coord & exposed_coord, bool vertical);


		int get_track_thickness();

		static TRWMetadata * metadata_new();
		static void metadata_free(TRWMetadata * metadata);
		TRWMetadata * get_metadata();
		void set_metadata(TRWMetadata * metadata);

		/* Intended only for use by other trw_layer subwindows. */
		void generate_missing_thumbnails(void);



		/* This should be private. */
		void cancel_current_tp(bool destroy);
		void tpwin_response(int response);
		void update_statusbar();


		Track * get_edited_track(void);
		void set_edited_track(Track * track, const TrackPoints::iterator & tp_iter);
		void set_edited_track(Track * track);
		void reset_edited_track(void);

		Waypoint * get_edited_wp(void);
		void set_edited_wp(Waypoint * wp);
		void reset_edited_wp(void);


		void delete_sublayer_common(TreeItem * item, bool confirm);
		void copy_sublayer_common(TreeItem * item);
		void cut_sublayer_common(TreeItem * item, bool confirm);

		bool handle_selection_in_tree();

		void reset_internal_selections(void);

		/* Export. */
		void export_layer(const QString & title, const QString & default_name, Track * trk, SGFileType file_type);
		void open_layer_with_external_program(const QString & external_program);
		int export_layer_with_gpsbabel(const QString & title, const QString & default_name);


		/* Common for LayerTRW and for Tracks. */
		void upload_to_gps(TreeItem * sublayer);


		bool get_track_creation_in_progress() const;
		void reset_track_creation_in_progress();

		bool get_route_creation_in_progress() const;
		void reset_route_creation_in_progress();

		bool moving_wp = false;

		PropertiesDialogTP * tpwin = NULL;

		/* Track editing tool -- more specifically, moving tps. */
		bool moving_tp = false;

		CoordMode coord_mode;

		int highest_wp_number = 0;



		sort_order_t track_sort_order;
		sort_order_t wp_sort_order;

		TRWMetadata * metadata = NULL;



		LayerTRWPainter * painter = NULL;


		uint16_t ct_x1;
		uint16_t ct_y1;
		uint16_t ct_x2;
		uint16_t ct_y2;
		bool draw_sync_done;
		bool draw_sync_do;

		/* Route finder tool. */
		bool route_finder_started;
		bool route_finder_check_added_track;
		Track * route_finder_added_track = NULL;
		bool route_finder_append;


		/* Viking has been using queue to be able to easily remove (pop()) oldest images (the ones that
		   are the longest in queue) when size of cached images goes over cache size limit. */
		std::deque<CachedPixmap *> wp_image_cache;
		int32_t wp_image_cache_size;


		/* Whether the program needs to generate thumbnails of
		   images that are part of this layer. */
		bool has_missing_thumbnails = true;

		/* Menu. */
		LayerMenuItem menu_selection;


		bool clear_highlight();


	public slots:
		void trackpoint_properties_cb(int response);



		/* Context menu callbacks. */

		void finish_track_cb(void);
		void finish_route_cb(void);

		void move_viewport_to_show_all_cb(void);

		void centerize_cb(void);
		void find_waypoint_dialog_cb(void);    /* In context menu for "Layer TRW" or "Waypoints" node in tree view. */

		void export_as_gpspoint_cb(void);
		void export_as_gpsmapper_cb(void);
		void export_as_gpx_cb(void);
		void export_as_kml_cb(void);
		void export_as_geojson_cb(void);
		void export_via_babel_cb(void);
		void open_with_external_gpx_1_cb(void);
		void open_with_external_gpx_2_cb(void);

		void new_waypoint_cb(void);
		void new_track_cb(void);
		void new_route_cb(void);

#ifdef VIK_CONFIG_GEOTAG
		void geotag_images_cb(void);
#endif

		void acquire_from_gps_cb(void);
		void acquire_from_routing_cb(void);
#ifdef VIK_CONFIG_OPENSTREETMAP
		void acquire_from_osm_cb(void);
		void acquire_from_osm_my_traces_cb(void);
#endif
		void acquire_from_url_cb(void);

		void acquire_from_wikipedia_waypoints_viewport_cb(void);
		void acquire_from_wikipedia_waypoints_layer_cb(void);

#ifdef VIK_CONFIG_GEOCACHES
		void acquire_from_geocache_cb(void);
#endif
#ifdef VIK_CONFIG_GEOTAG
		void acquire_from_geotagged_images_cb(void);
#endif
		void acquire_from_file_cb();

		void upload_to_gps_cb(void);
		void upload_to_osm_traces_cb(void);

		void delete_all_tracks_cb(void);
		void delete_selected_tracks_cb(void);
		void delete_all_routes_cb();
		void delete_selected_routes_cb();
		void delete_all_waypoints_cb(void);
		void delete_selected_waypoints_cb(void);

		void waypoint_list_dialog_cb(void);


		void tracks_stats_cb(void);

		void show_wp_picture_cb(void);

		void merge_by_segment_cb(void);
		void merge_by_timestamp_cb(void);
		void merge_with_other_cb(void);
		void append_track_cb(void);
		void split_at_trackpoint_cb(void);
		void delete_point_selected_cb(void);
		void delete_points_same_position_cb(void);
		void delete_points_same_time_cb(void);
		void download_map_along_track_cb(void);
		void edit_trackpoint_cb(void);
		void track_list_dialog_cb(void);
		void extend_track_end_cb(void);
		void extend_track_end_route_finder_cb(void);

		void append_other_cb(void);
		void insert_point_after_cb(void);
		void insert_point_before_cb(void);
		void routes_stats_cb();


	private:
		void wp_image_cache_flush();

		/* Track or Route that user currently operates on (creates or modifies).
		   Reference to an object already existing in ::tracks or ::routes. */
		Track * current_track_ = NULL;

		/* Waypoint that user currently operates on (creates or modifies).
		   Reference to an object already existing in ::waypoints. */
		Waypoint * current_wp_ = NULL;
	};




	void layer_trw_init(void);
	char * convert_to_dms(double dec);

	bool is_valid_geocache_name(const char * str);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_TRW_H_ */
