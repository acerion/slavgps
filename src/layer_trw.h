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
 *
 */

#ifndef _SG_LAYER_TRW_H_
#define _SG_LAYER_TRW_H_




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdint>
#include <list>
#include <unordered_map>

#include <QStandardItem>
#include <QDialog>

#include "layer.h"
#include "viewport.h"
#include "waypoint.h"
#include "track.h"
#include "trackpoint_properties.h"
#include "layer_trw_containers.h"
#include "slav_qt.h"




namespace SlavGPS {




	class LayerTRW;
	class LayersPanel;

	struct _VikDataSourceInterface;
	typedef struct _VikDataSourceInterface VikDataSourceInterface;



	class TRWMetadata {

	public:
		void set_author(char const * new_author);
		void set_description(char const * new_description);
		void set_keywords(char const * new_keywords);
		void set_timestamp(char const * timestamp);

		char * description = NULL;
		char * author = NULL;
		//bool has_time;
		char * timestamp = NULL; /* TODO: Consider storing as proper time_t. */
		char * keywords = NULL;  /* TODO: handling/storing a GList of individual tags? */
	};





	/* See http://developer.gnome.org/pango/stable/PangoMarkupFormat.html */
	typedef enum {
		FS_XX_SMALL = 0,
		FS_X_SMALL,
		FS_SMALL,
		FS_MEDIUM, // DEFAULT
		FS_LARGE,
		FS_X_LARGE,
		FS_XX_LARGE,
		FS_NUM_SIZES
	} font_size_t;





	/* For creating a list of tracks with the corresponding layer it is in
	   (thus a selection of tracks may be from differing layers). */
	struct track_layer_t {
		Track * trk;
		LayerTRW * trw;
	};
	typedef struct track_layer_t track_layer_t;

	/* For creating a list of waypoints with the corresponding layer it is in
	   (thus a selection of waypoints may be from differing layers). */
	struct waypoint_layer_t {
		Waypoint * wp;
		LayerTRW * trw;
	};
	typedef struct waypoint_layer_t waypoint_layer_t;




	class LayerTRWInterface : public LayerInterface {
	public:
		LayerTRWInterface();
		Layer * unmarshall(uint8_t * data, int len, Viewport * viewport);
		void change_param(GtkWidget * widget, ui_change_values * values);
	};




	class LayerTRW : public Layer {
		Q_OBJECT
	public:
		LayerTRW();
		~LayerTRW();


		/* Layer interface methods. */
		void draw(Viewport * viewport);
		void post_read(Viewport * viewport, bool from_file);
		QString tooltip();
		QString sublayer_tooltip(Sublayer * sublayer);

		bool selected(TreeItemType type, Sublayer * sublayer);

		/* Methods for generic "Select" tool. */
		bool select_click(QMouseEvent * event, Viewport * viewport, LayerTool * tool);
		bool select_move(QMouseEvent * event, Viewport * viewport, LayerTool * tool);
		bool select_release(QMouseEvent * event, Viewport * viewport, LayerTool * tool);
		bool select_tool_context_menu(QMouseEvent * event, Viewport * viewport);

		void set_menu_selection(uint16_t selection);
		uint16_t get_menu_selection();

		void marshall(uint8_t ** data, int * len);

		void cut_sublayer(Sublayer * sublayer);
		void copy_sublayer(Sublayer * sublayer, uint8_t ** item, unsigned int * len);
		bool paste_sublayer(Sublayer * sublayer, uint8_t * item, size_t len);
		void delete_sublayer(Sublayer * sublayer);

		void change_coord_mode(VikCoordMode dest_mode);

		time_t get_timestamp();

		void drag_drop_request(Layer * src, GtkTreeIter * src_item_iter, GtkTreePath * dest_path);

		int read_file(FILE * f, char const * dirpath);
		void write_file(FILE * f) const;

		void add_menu_items(QMenu & menu);
		bool sublayer_add_menu_items(QMenu & menu);

		char const * sublayer_rename_request(Sublayer * sublayer, const char * newname, LayersPanel * panel);
		bool sublayer_toggle_visible(Sublayer * sublayer);

		void connect_to_tree(TreeView * tree_view, TreeIndex const & layer_index);
		bool set_param_value(uint16_t id, ParameterValue param_value, bool is_file_operation);
		ParameterValue get_param_value(param_id_t id, bool is_file_operation) const;





		bool create_new_tracks(Track * orig, std::list<TrackPoints *> * tracks_data);

		void add_track(Track * trk);
		void add_route(Track * trk);
		void add_waypoint(Waypoint * wp);

		Tracks & get_tracks();
		Tracks & get_routes();
		Waypoints & get_waypoints();

		bool get_tracks_visibility();
		bool get_routes_visibility();
		bool get_waypoints_visibility();

		bool is_empty();


		bool find_track_by_date(char const * date, VikCoord * position, Viewport * viewport, bool select);
		bool find_waypoint_by_date(char const * date, VikCoord * position, Viewport * viewport, bool select);

		Track * get_track(const char * name);
		Track * get_route(const char * name);

		Waypoint * get_waypoint(const char * name);

		void draw_with_highlight(Viewport * viewport, bool highlight);
		void draw_highlight(Viewport * viewport);

		void draw_highlight_item(Track * trk, Waypoint * wp, Viewport * viewport);
		void draw_highlight_items(Tracks * tracks, Waypoints * waypoints, Viewport * viewport);


		void realize_tracks(Tracks & tracks, Layer * parent_layer, TreeIndex const & a_parent_index, TreeView * a_tree_view);
		void realize_waypoints(Waypoints & data, Layer * parent_layer, TreeIndex const & a_parent_index, TreeView * a_tree_view);


		static void find_maxmin_in_track(const Track * trk, struct LatLon maxmin[2]);



		void find_maxmin(struct LatLon maxmin[2]);
		bool find_center(VikCoord * dest);

		void set_statusbar_msg_info_trkpt(Trackpoint * tp);
		void set_statusbar_msg_info_wpt(Waypoint * wp);

		void zoom_to_show_latlons(Viewport * viewport, struct LatLon maxmin[2]);
		bool auto_set_view(Viewport * viewport);



		bool new_waypoint(Window * parent, const VikCoord * def_coord);
		void new_track_create_common(char * name);
		void new_route_create_common(char * name);




		void cancel_tps_of_track(Track * trk);

		void reset_waypoints();

		char * new_unique_sublayer_name(SublayerType sublayer_type, const char * name);


		/* These are meant for use in file loaders (gpspoint.c, gpx.c, etc).
		 * These copy the name, so you should free it if necessary. */
		void filein_add_waypoint(Waypoint * wp, char const * name);
		void filein_add_track(Track * trk, char const * name);




		void move_item(LayerTRW * vtl_dest, sg_uid_t sublayer_uid, SublayerType sublayer_type);



		bool delete_track(Track * trk);
		bool delete_track_by_name(char const * name, bool is_route);
		bool delete_route(Track * trk);
		bool delete_waypoint(Waypoint * wp);
		bool delete_waypoint_by_name(char const * name);


		void delete_all_routes();
		void delete_all_tracks();
		void delete_all_waypoints();


		void waypoint_rename(Waypoint * wp, char const * new_name);
		void waypoint_reset_icon(Waypoint * wp);
		void update_treeview(Track * trk);

		bool dem_test(LayersPanel * panel);
		void apply_dem_data_common(LayersPanel * panel, Track * trk, bool skip_existing_elevations);
		void smooth_it(Track * trk, bool flat);
		void wp_changed_message(int changed);



		void split_at_selected_trackpoint(SublayerType sublayer_type);
		void trackpoint_selected_delete(Track * trk);


		void diary_open(char const * date_str);
		void astro_open(char const * date_str,  char const * time_str, char const * lat_str, char const * lon_str, char const * alt_str);



		void uniquify_tracks(LayersPanel * panel, Tracks & track_table, bool ontrack);
		void sort_order_specified(SublayerType sublayer_type, vik_layer_sort_order_t order);

		void uniquify_waypoints(LayersPanel * panel);



		std::list<waypoint_layer_t *> * create_waypoints_and_layers_list();
		std::list<waypoint_layer_t *> * create_waypoints_and_layers_list_helper(std::list<Waypoint *> * waypoints);


		std::list<track_layer_t *> * create_tracks_and_layers_list();
		std::list<track_layer_t *> * create_tracks_and_layers_list(SublayerType sublayer_type);
		std::list<track_layer_t *> * create_tracks_and_layers_list_helper(std::list<Track *> * tracks);


		void trackpoint_properties_show();



		Trackpoint * closest_tp_in_five_pixel_interval(Viewport * viewport, int x, int y);
		Waypoint * closest_wp_in_five_pixel_interval(Viewport * viewport, int x, int y);


		void track_alloc_colors();

		void calculate_bounds_waypoints();

		static void calculate_bounds_track(Tracks & tracks);

		void calculate_bounds_tracks();


		void sort_all();

		time_t get_timestamp_tracks();
		time_t get_timestamp_waypoints();


		void set_coord_mode(VikCoordMode mode);
		VikCoordMode get_coord_mode();

		bool uniquify(LayersPanel * panel);


		void highest_wp_number_reset();
		void highest_wp_number_add_wp(char const * new_wp_name);
		void highest_wp_number_remove_wp(char const * old_wp_name);
		char * highest_wp_number_get();


#ifdef VIK_CONFIG_GOOGLE
		bool is_valid_google_route(sg_uid_t track_uid);
#endif

		void insert_tp_beside_current_tp(bool before);

		void my_tpwin_set_tp();


		void dialog_shift(QDialog * dialog, VikCoord * coord, bool vertical);


		uint32_t get_property_tracks_line_thickness();

		static TRWMetadata * metadata_new();
		static void metadata_free(TRWMetadata * metadata);
		TRWMetadata * get_metadata();
		void set_metadata(TRWMetadata * metadata);

		/* Intended only for use by other trw_layer subwindows. */
		void verify_thumbnails(void);



		/* This should be private. */
		void image_cache_free();
		void new_track_pens(void);
		void cancel_current_tp(bool destroy);
		void tpwin_response(int response);
		Track * get_track_helper(Sublayer * sublayer);
		void update_statusbar();
		void tool_extended_route_finder_undo();
		LayerToolFuncStatus tool_new_track_or_route_click(QMouseEvent * event, Viewport * viewport);
		void undo_trackpoint_add();



		/* Move this to layer_trw_containers.h. */




		Tracks tracks;
		Sublayer * tracks_node = NULL; /* Sub-node, under which all layer's tracks are shown. */
		bool tracks_visible;

		Tracks routes;
		Sublayer * routes_node = NULL; /* Sub-node, under which all layer's routes are shown. */
		bool routes_visible;

		Waypoints waypoints;
		Sublayer * waypoints_node = NULL; /* Sub-node, under which all layer's waypoints are shown. */
		bool waypoints_visible;


		/* Waypoint editing tool. */
		Waypoint * current_wp = NULL;
		bool moving_wp = false;
		bool waypoint_rightclick = false;

		/* Track editing tool. */
		struct {
			bool valid = false;
			TrackPoints::iterator iter;
		} selected_tp;

		Track * selected_track = NULL;  /* Track, to which belongs currently selected trackpoint (tp)? */

		PropertiesDialogTP * tpwin = NULL;

		/* Track editing tool -- more specifically, moving tps. */
		bool moving_tp = false;

		VikCoordMode coord_mode;

		int highest_wp_number = 0;



		LatLonBBox waypoints_bbox;
		bool track_draw_labels;

		uint8_t drawmode;
		uint8_t drawpoints;
		uint8_t drawpoints_size;
		uint8_t drawelevation;
		uint8_t elevation_factor;
		uint8_t drawstops;
		uint32_t stop_length;
		uint8_t drawlines;
		uint8_t drawdirections;
		uint8_t drawdirections_size;
		uint32_t line_thickness;
		uint32_t bg_line_thickness;
		vik_layer_sort_order_t track_sort_order;

		TRWMetadata * metadata = NULL;

		PangoLayout * tracklabellayout = NULL;
		font_size_t track_font_size;
		char * track_fsize_str = NULL;

		uint8_t wp_symbol;
		uint8_t wp_size;
		bool wp_draw_symbols;
		font_size_t wp_font_size;
		char * wp_fsize_str = NULL;
		vik_layer_sort_order_t wp_sort_order;


		double track_draw_speed_factor;
		std::vector<QPen> track_pens;
		QPen track_1color_pen;
		QColor track_color;
		QPen current_trk_pen;
		/* Separate pen for a track's potential new point as drawn via separate method
		   (compared to the actual track points drawn in the main trw_layer_draw_track function). */
		QPen current_trk_new_point_pen;

		QPen track_bg_pen;
		QColor track_bg_color;

		QPen  waypoint_pen;
		QColor waypoint_color;

		QPen waypoint_text_pen;
		QColor waypoint_text_color;

		QPen waypoint_bg_pen;
		QColor waypoint_bg_color;

		GdkFunction wpbgand;


		/* Track or Route that user currently operates.
		   Reference to an object already existing in ::tracks or ::routes. */
		Track * current_trk = NULL;

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


		bool drawlabels;
		bool drawimages;
		uint8_t image_alpha;
		GQueue * image_cache = NULL;
		uint8_t image_size;
		uint16_t image_cache_size;




		/* For waypoint text. */
		PangoLayout * wplabellayout = NULL;

		bool has_verified_thumbnails;

		/* Menu. */
		LayerMenuItem menu_selection;

	public slots:
		void trackpoint_properties_cb(int response);



		/* Context menu callbacks. */

		void finish_track_cb(void);

		void full_view_cb(void);
		void full_view_tracks_cb(void);
		void full_view_routes_cb(void);
		void full_view_waypoints_cb(void);

		void centerize_cb(void);
		void find_waypoint_dialog_cb(void);    /* In context menu for "Layer TRW" or "Waypoints" node in tree view. */
		void go_to_selected_waypoint_cb(void); /* In context menu for specific Waypoint node in tree view. */

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

		void waypoints_visibility_on_cb(void);
		void waypoints_visibility_off_cb(void);
		void waypoints_visibility_toggle_cb(void);

		void tracks_visibility_off_cb(void);
		void tracks_visibility_on_cb(void);
		void tracks_visibility_toggle_cb(void);

		void routes_visibility_off_cb(void);
		void routes_visibility_on_cb(void);
		void routes_visibility_toggle_cb(void);

		void waypoint_list_dialog_cb(void);

		void delete_sublayer_cb(void);
		void copy_sublayer_cb(void);
		void cut_sublayer_cb(void);
		void paste_sublayer_cb(void);

		void waypoint_geocache_webpage_cb(void);
		void geotagging_waypoint_cb(void);

		void track_list_dialog_single_cb(void);
		void tracks_stats_cb(void);

		void show_picture_cb(void);

#ifdef VIK_CONFIG_GEOTAG
		void geotagging_waypoint_mtime_keep_cb(void);
		void geotagging_waypoint_mtime_update_cb(void);
		void geotagging_track_cb(void);
#endif

		void goto_track_startpoint_cb(void);
		void goto_track_endpoint_cb(void);
		void goto_track_max_speed_cb(void);
		void goto_track_max_alt_cb(void);
		void goto_track_min_alt_cb(void);
		void goto_track_center_cb(void);

		void merge_by_segment_cb(void);
		void merge_by_timestamp_cb(void);
		void merge_with_other_cb(void);
		void append_track_cb(void);
		void split_by_timestamp_cb(void);
		void split_by_n_points_cb(void);
		void split_at_trackpoint_cb(void);
		void split_segments_cb(void);
		void delete_point_selected_cb(void);
		void delete_points_same_position_cb(void);
		void delete_points_same_time_cb(void);
		void reverse_cb(void);
		void download_map_along_track_cb(void);
		void edit_trackpoint_cb(void);
		void gps_upload_any_cb(void);
		void track_list_dialog_cb(void);
		void properties_item_cb(void); /* TODO?? */
		void profile_item_cb(void);
		void waypoint_webpage_cb(void);
		void export_gpx_track_cb(void);
		void osm_traces_upload_track_cb(void);
		void track_statistics_cb(void);
		void convert_track_route_cb(void);
		void anonymize_times_cb(void);
		void interpolate_times_cb(void);
		void extend_track_end_cb(void);
		void extend_track_end_route_finder_cb(void);
		void apply_dem_data_all_cb(void);
		void apply_dem_data_only_missing_cb(void);
		void missing_elevation_data_interp_cb(void);
		void missing_elevation_data_flat_cb(void);
		void apply_dem_data_wpt_all_cb(void);
		void apply_dem_data_wpt_only_missing_cb(void);
		void auto_track_view_cb(void);
		void route_refine_cb(void);
		void append_other_cb(void);
		void insert_point_after_cb(void);
		void insert_point_before_cb(void);
		void diary_cb(void);
		void astro_cb(void);
		void sort_order_a2z_cb(void);
		void sort_order_z2a_cb(void);
		void sort_order_timestamp_ascend_cb(void);
		void sort_order_timestamp_descend_cb(void);
		void routes_stats_cb();
#ifndef WINDOWS
		void track_use_with_filter_cb(void);
#endif
		void google_route_webpage_cb(void);

	public:
		void acquire(VikDataSourceInterface *datasource);

	private:
		/* Add a node in tree view, under which layers' tracks/waypoints/routes will be displayed. */
		void add_tracks_node(void);
		void add_waypoints_node(void);
		void add_routes_node(void);
	};




	void layer_trw_init(void);




} /* namespace SlavGPS */




QIcon * get_wp_sym_small(char * symbol);




int sort_alphabetically(gconstpointer a, gconstpointer b, void * user_data);
int check_tracks_for_same_name(gconstpointer aa, gconstpointer bb, void * udata);
bool is_valid_geocache_name(char *str);




#define VIK_SETTINGS_LIST_DATE_FORMAT "list_date_format"




#endif /* #ifndef _SG_LAYER_TRW_H_ */
