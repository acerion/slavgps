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




#include <cstdint>
#include <list>
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
#include "mem_cache.h"




#define VIK_SETTINGS_SORTABLE_DATE_TIME_FORMAT "sortable_date_time_format"




namespace SlavGPS {




	class Viewport;
	class LayerTRW;
	class LayerTRWPainter;
	class LayersPanel;
	class Track;
	class TrackpointSearch;
	class WaypointSearch;
	class PropertiesDialogTP;
	class DataSource;
	enum class TreeViewSortOrder;
	enum class LayerDataReadStatus;
	enum class SGFileType;




	class TRWMetadata {

	public:
		void set_author(const QString & new_author);
		void set_description(const QString & new_description);
		void set_keywords(const QString & new_keywords);
		void set_iso8601_timestamp(const QString & new_timestamp);

		QString description;
		QString author;
		QString keywords;  /* TODO_LATER: handling/storing a list of individual tags? */
		/* TODO_LATER: verify how the timestamp should work: when and how it should be set. */
		bool has_timestamp;
		QString iso8601_timestamp; /* TODO_MAYBE: Consider storing as proper time_t. */
	};




	class LayerTRWInterface : public LayerInterface {
	public:
		LayerTRWInterface();
		Layer * unmarshall(Pickle & pickle, Viewport * viewport);
		LayerToolContainer * create_tools(Window * window, Viewport * viewport);
	};




	class LayerTRW : public Layer {
		Q_OBJECT
	public:
		LayerTRW();
		~LayerTRW();

		/* Module initialization. */
		static void init(void);


		/* Layer interface methods. */
		void post_read(Viewport * viewport, bool from_file);
		QString get_tooltip(void) const;


		/* Methods for generic "Select" tool. */
		bool handle_select_tool_click(QMouseEvent * event, Viewport * viewport, LayerToolSelect * select_tool);
		bool handle_select_tool_double_click(QMouseEvent * event, Viewport * viewport, LayerToolSelect * select_tool);
		bool handle_select_tool_move(QMouseEvent * event, Viewport * viewport, LayerToolSelect * select_tool);
		bool handle_select_tool_release(QMouseEvent * event, Viewport * viewport, LayerToolSelect * select_tool);
		bool handle_select_tool_context_menu(QMouseEvent * event, Viewport * viewport);

		void handle_select_tool_click_do_track_selection(QMouseEvent * ev, LayerToolSelect * select_tool, Track * track, TrackPoints::iterator & tp_iter);
		void handle_select_tool_click_do_waypoint_selection(QMouseEvent * ev, LayerToolSelect * select_tool, Waypoint * wp);
		void handle_select_tool_double_click_do_waypoint_selection(QMouseEvent * ev, LayerToolSelect * select_tool, Waypoint * wp);

		void marshall(Pickle & pickle);

		void cut_sublayer(TreeItem * item);
		void copy_sublayer(TreeItem * item, Pickle & pickle);
		bool paste_sublayer(TreeItem * item, Pickle & pickle);
		void delete_sublayer(TreeItem * item);

		void change_coord_mode(CoordMode dest_mode);

		time_t get_timestamp();

		void drag_drop_request(Layer * src, TreeIndex & src_item_index, void * GtkTreePath_dest_path);
		sg_ret drag_drop_request(TreeItem * tree_item, int row, int col);
		sg_ret dropped_item_is_acceptable(TreeItem * tree_item, bool * result) const;

		LayerDataReadStatus read_layer_data(QFile & file, const QString & dirpath);
		sg_ret write_layer_data(FILE * file) const;

		void add_menu_items(QMenu & menu);

		void attach_children_to_tree(void);
		bool set_param_value(param_id_t param_id, const SGVariant & param_value, bool is_file_operation);
		SGVariant get_param_value(param_id_t param_id, bool is_file_operation) const;





		bool create_new_tracks(Track * orig, std::list<TrackPoints *> * tracks_data);

		sg_ret add_track(Track * trk);
		sg_ret add_route(Track * trk);
		sg_ret add_waypoint(Waypoint * wp);

		const std::list<Track *> & get_tracks(void) const { return this->tracks.children_list; };
		const std::list<Track *> & get_routes(void) const { return this->routes.children_list; };
		const std::list<Waypoint *> & get_waypoints(void) const { return this->waypoints.children_list; };

		LayerTRWTracks & get_tracks_node(void) { return this->tracks; };
		LayerTRWTracks & get_routes_node(void) { return this->routes; };
		LayerTRWWaypoints & get_waypoints_node(void) { return this->waypoints; };

		LayerTRWTracks tracks{false}; /* Sub-node, under which all layer's tracks are shown. */
		LayerTRWTracks routes{true};  /* Sub-node, under which all layer's routes are shown. */
		LayerTRWWaypoints waypoints;  /* Sub-node, under which all layer's waypoints are shown. */


		bool get_tracks_visibility(void) const;
		bool get_routes_visibility(void) const;
		bool get_waypoints_visibility(void) const;

		bool is_empty(void) const;


		std::list<TreeItem *> get_tracks_by_date(const QDate & search_date) const;
		std::list<TreeItem *> get_waypoints_by_date(const QDate & search_date) const;
		std::list<TreeItem *> get_items_by_date(const QDate & search_date) const;


		/* Draw all items of the layer, with highlight. */
		void draw_tree_item(Viewport * viewport, bool highlight_selected, bool parent_is_selected);


		void recalculate_bbox(void);
		LatLonBBox get_bbox(void);

		bool find_center(Coord * dest);

		void set_statusbar_msg_info_tp(TrackPoints::iterator & tp_iter, Track * track);
		void set_statusbar_msg_info_wpt(Waypoint * wp);

		bool move_viewport_to_show_all(Viewport * viewport);


		bool new_waypoint(const Coord & default_coord, Window * parent = NULL);
		Track * new_track_create_common(const QString & new_name);
		Track * new_route_create_common(const QString & new_name);




		void cancel_tps_of_track(Track * trk);

		void reset_waypoints();

		QString new_unique_element_name(const QString & type_id, const QString& old_name);


		/* These are meant for use in file loaders (gpspoint.c, gpx.c, etc). */
		void add_waypoint_from_file(Waypoint * wp);
		void add_track_from_file(Track * trk);


		void move_item(LayerTRW * vtl_dest, sg_uid_t sublayer_uid, const QString & type_id);


		void delete_all_routes();
		void delete_all_tracks();
		void delete_all_waypoints();



		/* Add to layer's data structure. */
		sg_ret attach_to_container(Track * trk);
		sg_ret attach_to_container(Waypoint * wp);

		/* Remove from layer's data structure. */
		sg_ret detach_from_container(Track * trk, bool * was_visible = NULL);
		sg_ret detach_from_container(Waypoint * wp, bool * was_visible = NULL);

		sg_ret attach_to_tree(Track * trk);
		sg_ret attach_to_tree(Waypoint * wp);

		sg_ret detach_from_tree(TreeItem * tree_item);



		void smooth_it(Track * trk, bool flat);
		void wp_changed_message(int changed);



		void delete_selected_tp(Track * trk);

		void diary_open(char const * date_str);
		void astro_open(char const * date_str,  char const * time_str, char const * lat_str, char const * lon_str, char const * alt_str);



		void get_waypoints_list(std::list<Waypoint *> & list);
		void get_tracks_list(std::list<Track *> & list, const QString & type_id_string);


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

		void tpwin_update_dialog_data();


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

		/* Returns true if there was some waypoint that was selected/edited. */
		bool reset_edited_wp(void);


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


		TreeViewSortOrder track_sort_order;
		TreeViewSortOrder wp_sort_order;

		TRWMetadata * metadata = NULL;



		LayerTRWPainter * painter = NULL;


		bool draw_sync_done;
		bool draw_sync_do;

		/* Route finder tool. */
		bool route_finder_started;
		bool route_finder_check_added_track;
		Track * route_finder_added_track = NULL;
		bool route_finder_append;


		/* In-memory cache of waypoint images. */
		MemCache<CachedPixmap> wp_image_cache;


		/* Whether the program needs to generate thumbnails of
		   images that are part of this layer. */
		bool has_missing_thumbnails = true;


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
		void acquire_from_osm_cb(void);
		void acquire_from_osm_my_traces_cb(void);
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

		void wp_image_cache_add(const CachedPixmap & cached_pixmap);


	private:
		void wp_image_cache_flush(void);

		/* Structure to hold multiple track information for a layer. */
		class TracksTooltipData {
		public:
			Distance length;
			time_t start_time = 0;
			time_t end_time = 0;
			int    duration = 0;
		};

		TracksTooltipData get_tracks_tooltip_data(void) const;
		Distance get_routes_tooltip_data(void) const;

		/* Track or Route that user currently operates on (creates or modifies).
		   Reference to an object already existing in ::tracks or ::routes. */
		Track * current_track_ = NULL;

		/* Waypoint that user currently operates on (creates or modifies).
		   Reference to an object already existing in ::waypoints. */
		Waypoint * current_wp_ = NULL;
	};





	char * convert_to_dms(double dec);

	bool is_valid_geocache_name(const char * str);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_TRW_H_ */
