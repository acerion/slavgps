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

#include "layer.h"
#include "viewport.h"
#include "vikwaypoint.h"
#include "track.h"
#include "layers_panel.h"
//#include "viktrwlayer_tpwin.h"
#include "layer_trw_containers.h"
#include "slav_qt.h"




typedef struct {
	VikLayer vl;
} VikTrwLayer;




struct _trw_menu_sublayer_t;




namespace SlavGPS {




	class LayerTRW;
	class LayersPanel;




	typedef struct {
		GtkTreeIter * path_iter;
		GtkTreeIter * iter2;
		Layer * layer;
		TreeView * tree_view;
	} trw_data4_t;




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




	class LayerTRW : public Layer {

	public:
		LayerTRW();
		LayerTRW(Viewport * viewport);
		~LayerTRW();


		/* Layer interface methods. */
		void draw(Viewport * viewport);
		void post_read(Viewport * viewport, bool from_file);
		char const * tooltip();
		char const * sublayer_tooltip(SublayerType sublayer_type, sg_uid_t sublayer_uid);

		bool selected(SublayerType sublayer_type, sg_uid_t sublayer_uid, TreeItemType type);

		bool show_selected_viewport_menu(GdkEventButton * event, Viewport * viewport);

		bool select_click(GdkEventButton * event, Viewport * viewport, LayerTool * tool);
		bool select_move(GdkEventMotion * event, Viewport * viewport, LayerTool * tool);
		bool select_release(GdkEventButton * event, Viewport * viewport, LayerTool * tool);

		void set_menu_selection(uint16_t selection);
		uint16_t get_menu_selection();

		void marshall(uint8_t ** data, int * len);

		void cut_sublayer(SublayerType sublayer_type, sg_uid_t sublayer_uid);
		void copy_sublayer(SublayerType sublayer_type, sg_uid_t sublayer_uid, uint8_t ** item, unsigned int * len);
		bool paste_sublayer(SublayerType sublayer_type, uint8_t * item, size_t len);
		void delete_sublayer(SublayerType sublayer_type, sg_uid_t sublayer_uid);

		void change_coord_mode(VikCoordMode dest_mode);

		time_t get_timestamp();

		void drag_drop_request(Layer * src, GtkTreeIter * src_item_iter, GtkTreePath * dest_path);

		int read_file(FILE * f, char const * dirpath);
		void write_file(FILE * f) const;
#ifdef K
		void add_menu_items(GtkMenu * menu, void * panel);
		bool sublayer_add_menu_items(GtkMenu * menu, void * panel, SublayerType sublayer_type, sg_uid_t sublayer_uid, GtkTreeIter * iter, Viewport * viewport);
#endif
		char const * sublayer_rename_request(const char * newname, void * panel, SublayerType sublayer_type, sg_uid_t sublayer_uid, GtkTreeIter * iter);
		bool sublayer_toggle_visible(SublayerType sublayer_type, sg_uid_t sublayer_uid);

		void realize(TreeView * tree_view, GtkTreeIter * layer_iter);
		bool set_param_value(uint16_t id, LayerParamValue param_value, Viewport * viewport, bool is_file_operation);
		LayerParamValue get_param_value(layer_param_id_t id, bool is_file_operation) const;





		bool create_new_tracks(Track * orig, std::list<TrackPoints *> * tracks_data);

		void add_track(Track * trk, char const * name);
		void add_route(Track * trk, char const * name);
		void add_waypoint(Waypoint * wp, char const * name);

		std::unordered_map<sg_uid_t, Track *> & get_tracks();
		std::unordered_map<sg_uid_t, Track *> & get_routes();
		std::unordered_map<sg_uid_t, Waypoint *> & get_waypoints();

		std::unordered_map<sg_uid_t, TreeIndex *> & get_tracks_iters();
		std::unordered_map<sg_uid_t, TreeIndex *> & get_routes_iters();
		std::unordered_map<sg_uid_t, TreeIndex *> & get_waypoints_iters();

		bool get_tracks_visibility();
		bool get_routes_visibility();
		bool get_waypoints_visibility();

		bool is_empty();


		bool find_by_date(char const * date_str, VikCoord * position, Viewport * viewport, bool do_tracks, bool select);

		Track * get_track(const char * name);
		Track * get_route(const char * name);

		Waypoint * get_waypoint(const char * name);

		void draw_with_highlight(Viewport * viewport, bool highlight);
		void draw_highlight(Viewport * viewport);

		void draw_highlight_item(Track * trk, Waypoint * wp, Viewport * viewport);
		void draw_highlight_items(std::unordered_map<sg_uid_t, Track *> * tracks, std::unordered_map<sg_uid_t, Waypoint *> * waypoints, Viewport * viewport);


		void realize_track(std::unordered_map<sg_uid_t, Track *> & tracks, trw_data4_t * pass_along, SublayerType sublayer_type);
		void realize_waypoints(std::unordered_map<sg_uid_t, Waypoint *> & data, trw_data4_t * pass_along, SublayerType sublayer_type);


		void add_sublayer_tracks(TreeView * tree_view, GtkTreeIter * layer_iter);
		void add_sublayer_waypoints(TreeView * tree_view, GtkTreeIter * layer_iter);
		void add_sublayer_routes(TreeView * tree_view, GtkTreeIter * layer_iter);


		static void find_maxmin_in_track(const Track * trk, struct LatLon maxmin[2]);



		void find_maxmin(struct LatLon maxmin[2]);
		bool find_center(VikCoord * dest);

		void set_statusbar_msg_info_trkpt(Trackpoint * tp);
		void set_statusbar_msg_info_wpt(Waypoint * wp);

		void zoom_to_show_latlons(Viewport * viewport, struct LatLon maxmin[2]);
		bool auto_set_view(Viewport * viewport);



		bool new_waypoint(GtkWindow * w, const VikCoord * def_coord);
		void new_track_create_common(char * name);
		void new_route_create_common(char * name);




		void cancel_tps_of_track(Track * trk);

		void reset_waypoints();

		char * new_unique_sublayer_name(SublayerType sublayer_type, const char * name);


		/* These are meant for use in file loaders (gpspoint.c, gpx.c, etc).
		 * These copy the name, so you should free it if necessary. */
		void filein_add_waypoint(char * name, Waypoint * wp);
		void filein_add_track(char * name, Track * trk);




		void move_item(LayerTRW * vtl_dest, void * id, SublayerType sublayer_type);



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



		void uniquify_tracks(LayersPanel * panel, std::unordered_map<sg_uid_t, Track *> & track_table, bool ontrack);
		void sort_order_specified(SublayerType sublayer_type, vik_layer_sort_order_t order);

		bool has_same_waypoint_names();
		void uniquify_waypoints(LayersPanel * panel);



		std::list<waypoint_layer_t *> * create_waypoints_and_layers_list();
		std::list<waypoint_layer_t *> * create_waypoints_and_layers_list_helper(std::list<Waypoint *> * waypoints);


		std::list<track_layer_t *> * create_tracks_and_layers_list();
		std::list<track_layer_t *> * create_tracks_and_layers_list(SublayerType sublayer_type);
		std::list<track_layer_t *> * create_tracks_and_layers_list_helper(std::list<Track *> * tracks);


		void tpwin_init();




		Trackpoint * closest_tp_in_five_pixel_interval(Viewport * viewport, int x, int y);
		Waypoint * closest_wp_in_five_pixel_interval(Viewport * viewport, int x, int y);


		void track_alloc_colors();

		void calculate_bounds_waypoints();

		static void calculate_bounds_track(std::unordered_map<sg_uid_t, Track *> & tracks);

		void calculate_bounds_tracks();


		void sort_all();

		time_t get_timestamp_tracks();
		time_t get_timestamp_waypoints();



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


		void dialog_shift(GtkWindow * dialog, VikCoord * coord, bool vertical);


		int get_property_tracks_line_thickness();

		static TRWMetadata * metadata_new();
		static void metadata_free(TRWMetadata * metadata);
		TRWMetadata * get_metadata();
		void set_metadata(TRWMetadata * metadata);

		/* Intended only for use by other trw_layer subwindows. */
		void verify_thumbnails(void);


		/* Callback-related. */
		bool tool_edit_waypoint_click(GdkEventButton * event, LayerTool * tool);
		bool tool_edit_waypoint_move(GdkEventMotion * event, LayerTool * tool);
		bool tool_edit_waypoint_release(GdkEventButton * event, LayerTool * tool);
		bool tool_extended_route_finder_click(GdkEventButton * event, LayerTool * tool);
		bool tool_extended_route_finder_key_press(GdkEventKey * event, LayerTool * tool);
		bool tool_show_picture_click(GdkEventButton * event, LayerTool * tool);
		bool tool_edit_trackpoint_click(GdkEventButton * event, LayerTool * tool);
		bool tool_edit_trackpoint_move(GdkEventMotion *event, LayerTool * tool);
		bool tool_edit_trackpoint_release(GdkEventButton * event, LayerTool * tool);
		VikLayerToolFuncStatus tool_new_track_move(GdkEventMotion * event, LayerTool * tool);
		bool tool_new_track_key_press(GdkEventKey *event, LayerTool * tool);
		bool tool_new_track_click(GdkEventButton * event, LayerTool * tool);
		bool tool_new_waypoint_click(GdkEventButton * event, LayerTool * tool);
		bool tool_new_route_click(GdkEventButton * event, LayerTool * tool);
		void tool_new_track_release(GdkEventButton *event, LayerTool * tool);




		/* This should be private. */
		void image_cache_free();
		void new_track_gcs(Viewport * viewport);
		void free_track_gcs();
		void cancel_current_tp(bool destroy);
		void tpwin_response(int response);
		Track * get_track_helper(struct _trw_menu_sublayer_t * data);
		void update_statusbar();
		void tool_extended_route_finder_undo();
		bool tool_new_track_or_route_click(GdkEventButton * event, Viewport * viewport);
		void undo_trackpoint_add();



		/* Move this to layer_trw_containers.h. */




		std::unordered_map<sg_uid_t, Track *> tracks;
		std::unordered_map<sg_uid_t, TreeIndex *> tracks_iters;
		GtkTreeIter track_iter;
		bool tracks_visible;

		std::unordered_map<sg_uid_t, Track *> routes;
		std::unordered_map<sg_uid_t, TreeIndex *> routes_iters;
		GtkTreeIter route_iter;
		bool routes_visible;

		std::unordered_map<sg_uid_t, Waypoint *> waypoints;
		std::unordered_map<sg_uid_t, TreeIndex *> waypoints_iters;
		TreeIndex waypoint_iter;
		bool waypoints_visible;


		/* Waypoint editing tool. */
		Waypoint * current_wp = NULL;
		sg_uid_t current_wp_uid = 0;
		bool moving_wp = false;
		bool waypoint_rightclick = false;

		/* Track editing tool. */
		struct {
			bool valid = false;
			TrackPoints::iterator iter;
		} selected_tp;

		Track * selected_track = NULL;  /* Track, to which belongs currently selected trackpoint (tp)? */
		sg_uid_t current_tp_uid = 0;   /* uid of track, to which belongs currently selected trackpoint (tp)? */

		VikTrwLayerTpwin * tpwin = NULL;

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
		uint8_t line_thickness;
		uint8_t bg_line_thickness;
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
		GArray * track_gc = NULL;
		GdkGC * track_1color_gc = NULL;
		GdkColor track_color;
		GdkGC * current_track_gc = NULL;
		/* Separate GC for a track's potential new point as drawn via separate method
		   (compared to the actual track points drawn in the main trw_layer_draw_track function). */
		GdkGC * current_track_newpoint_gc = NULL;

		GdkGC * track_bg_gc = NULL;
		GdkColor track_bg_color;

		GdkGC * waypoint_gc = NULL;
		GdkColor waypoint_color;

		GdkGC * waypoint_text_gc = NULL;
		GdkColor waypoint_text_color;

		GdkGC * waypoint_bg_gc = NULL;
		GdkColor waypoint_bg_color;

		GdkFunction wpbgand;



		Track * current_track = NULL; /* ATM shared between new tracks and new routes. */
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

		GtkMenu * wp_right_click_menu = NULL;
		GtkMenu * track_right_click_menu = NULL;

		/* Menu. */
		VikStdLayerMenuItem menu_selection;


		/* One per layer. */
		GtkWidget * tracks_analysis_dialog = NULL;
	};




	void layer_trw_init(void);




}




typedef struct {
	SlavGPS::LayerTRW * layer;
	SlavGPS::LayersPanel * panel;
} trw_menu_layer_t;


typedef struct _trw_menu_sublayer_t {
	SlavGPS::LayerTRW * layer;
	SlavGPS::LayersPanel * panel;
	SlavGPS::SublayerType sublayer_type;
	sg_uid_t sublayer_uid;
	bool confirm;
	SlavGPS::Viewport * viewport;
	GtkTreeIter * tv_iter;
	void * misc;
} trw_menu_sublayer_t;




GdkPixbuf* get_wp_sym_small(char *symbol);

/* Exposed Layer Interface function definitions.
   Intended only for use by other trw_layer subwindows. */
//void trw_layer_verify_thumbnails(VikLayer * vtl);
void trw_layer_calculate_bounds_waypoints(VikLayer *vtl);


typedef struct {
	bool    has_same_track_name;
	const char *same_track_name;
} same_track_name_udata;

int sort_alphabetically(gconstpointer a, gconstpointer b, void * user_data);
int check_tracks_for_same_name(gconstpointer aa, gconstpointer bb, void * udata);


#define VIK_SETTINGS_LIST_DATE_FORMAT "list_date_format"




/* This needs to be visible in viktrwlayer_ui.h. */
void trw_layer_goto_track_startpoint(trw_menu_sublayer_t * data);
void trw_layer_goto_track_endpoint(trw_menu_sublayer_t * data);
void trw_layer_goto_track_max_speed(trw_menu_sublayer_t * data);
void trw_layer_goto_track_max_alt(trw_menu_sublayer_t * data);
void trw_layer_goto_track_min_alt(trw_menu_sublayer_t * data);
void trw_layer_goto_track_center(trw_menu_sublayer_t * data);
void trw_layer_merge_by_segment(trw_menu_sublayer_t * data);
void trw_layer_merge_by_timestamp(trw_menu_sublayer_t * data);
void trw_layer_merge_with_other(trw_menu_sublayer_t * data);
void trw_layer_append_track(trw_menu_sublayer_t * data);
void trw_layer_split_by_timestamp(trw_menu_sublayer_t * data);
void trw_layer_split_by_n_points(trw_menu_sublayer_t * data);
void trw_layer_split_at_trackpoint(trw_menu_sublayer_t * data);
void trw_layer_split_segments(trw_menu_sublayer_t * data);
void trw_layer_delete_point_selected(trw_menu_sublayer_t * data);
void trw_layer_delete_points_same_position(trw_menu_sublayer_t * data);
void trw_layer_delete_points_same_time(trw_menu_sublayer_t * data);
void trw_layer_reverse(trw_menu_sublayer_t * data);
void trw_layer_download_map_along_track_cb(trw_menu_sublayer_t * data);
void trw_layer_edit_trackpoint(trw_menu_sublayer_t * data);
void trw_layer_show_picture(trw_menu_sublayer_t * data);
void trw_layer_gps_upload_any(trw_menu_sublayer_t * data);
void trw_layer_centerize(trw_menu_layer_t * data);
void trw_layer_auto_view(trw_menu_layer_t * data);
void trw_layer_goto_wp(trw_menu_layer_t * data);
void trw_layer_new_wp(trw_menu_layer_t * data);
void trw_layer_new_track(trw_menu_layer_t * data);
void trw_layer_new_route(trw_menu_layer_t * data);
void trw_layer_finish_track(trw_menu_layer_t * data);
void trw_layer_auto_waypoints_view(trw_menu_layer_t * data);
void trw_layer_auto_tracks_view(trw_menu_layer_t * data);
void trw_layer_delete_all_tracks(trw_menu_layer_t * data);
void trw_layer_delete_tracks_from_selection(trw_menu_layer_t * data);
void trw_layer_delete_all_waypoints(trw_menu_layer_t * data);
void trw_layer_delete_waypoints_from_selection(trw_menu_layer_t * data);
void trw_layer_new_wikipedia_wp_viewport(trw_menu_layer_t * data);
void trw_layer_new_wikipedia_wp_layer(trw_menu_layer_t * data);
#ifdef VIK_CONFIG_GEOTAG
void trw_layer_geotagging_waypoint_mtime_keep(trw_menu_sublayer_t * data);
void trw_layer_geotagging_waypoint_mtime_update(trw_menu_sublayer_t * data);
void trw_layer_geotagging_track(trw_menu_sublayer_t * data);
void trw_layer_geotagging(trw_menu_layer_t * data);
#endif
void trw_layer_acquire_gps_cb(trw_menu_layer_t * data);
void trw_layer_acquire_routing_cb(trw_menu_layer_t * data);
void trw_layer_acquire_url_cb(trw_menu_layer_t * data);
#ifdef VIK_CONFIG_OPENSTREETMAP
void trw_layer_acquire_osm_cb(trw_menu_layer_t * data);
void trw_layer_acquire_osm_my_traces_cb(trw_menu_layer_t * data);
#endif
#ifdef VIK_CONFIG_GEOCACHES
void trw_layer_acquire_geocache_cb(trw_menu_layer_t * data);
#endif
#ifdef VIK_CONFIG_GEOTAG
void trw_layer_acquire_geotagged_cb(trw_menu_layer_t * data);
#endif
void trw_layer_acquire_file_cb(trw_menu_layer_t * data);
void trw_layer_gps_upload(trw_menu_layer_t * data);
void trw_layer_track_list_dialog_single(trw_menu_sublayer_t * data);
void trw_layer_track_list_dialog(trw_menu_layer_t * data);
void trw_layer_waypoint_list_dialog(trw_menu_layer_t * data);
/* Specific route versions:
   Most track handling functions can handle operating on the route list.
   However these ones are easier in separate functions. */
void trw_layer_auto_routes_view(trw_menu_layer_t * data);
void trw_layer_delete_all_routes(trw_menu_layer_t * data);
void trw_layer_delete_routes_from_selection(trw_menu_layer_t * data);
/* Pop-up items. */
void trw_layer_properties_item(trw_menu_sublayer_t * data); /* TODO?? */
void trw_layer_goto_waypoint(trw_menu_sublayer_t * data);
void trw_layer_waypoint_gc_webpage(trw_menu_sublayer_t * data);
void trw_layer_waypoint_webpage(trw_menu_sublayer_t * data);
void trw_layer_paste_item_cb(trw_menu_sublayer_t * data);
void trw_layer_export_gpspoint(trw_menu_layer_t * data);
void trw_layer_export_gpsmapper(trw_menu_layer_t * data);
void trw_layer_export_gpx(trw_menu_layer_t * data);
void trw_layer_export_kml(trw_menu_layer_t * data);
void trw_layer_export_geojson(trw_menu_layer_t * data);
void trw_layer_export_babel(trw_menu_layer_t * data);
void trw_layer_export_external_gpx_1(trw_menu_layer_t * data);
void trw_layer_export_external_gpx_2(trw_menu_layer_t * data);
void trw_layer_export_gpx_track(trw_menu_sublayer_t * data);
void trw_layer_geotagging_waypoint(trw_menu_sublayer_t * data);
void trw_layer_osm_traces_upload_cb(trw_menu_layer_t * data);
void trw_layer_osm_traces_upload_track_cb(trw_menu_sublayer_t * data);
GtkWidget* create_external_submenu(GtkMenu *menu);
void trw_layer_track_statistics(trw_menu_sublayer_t * data);
void trw_layer_convert_track_route(trw_menu_sublayer_t * data);
void trw_layer_anonymize_times(trw_menu_sublayer_t * data);
void trw_layer_interpolate_times(trw_menu_sublayer_t * data);
void trw_layer_extend_track_end(trw_menu_sublayer_t * data);
void trw_layer_extend_track_end_route_finder(trw_menu_sublayer_t * data);
void trw_layer_apply_dem_data_all(trw_menu_sublayer_t * data);
void trw_layer_apply_dem_data_only_missing(trw_menu_sublayer_t * data);
void trw_layer_missing_elevation_data_interp(trw_menu_sublayer_t * data);
void trw_layer_missing_elevation_data_flat(trw_menu_sublayer_t * data);
void trw_layer_apply_dem_data_wpt_all(trw_menu_sublayer_t * data);
void trw_layer_apply_dem_data_wpt_only_missing(trw_menu_sublayer_t * data);
void trw_layer_auto_track_view(trw_menu_sublayer_t * data);
void trw_layer_route_refine(trw_menu_sublayer_t * data);
void trw_layer_append_other(trw_menu_sublayer_t * data);
void trw_layer_insert_point_after(trw_menu_sublayer_t * data);
void trw_layer_insert_point_before(trw_menu_sublayer_t * data);
void trw_layer_diary(trw_menu_sublayer_t * data);
void trw_layer_astro(trw_menu_sublayer_t * data);
void trw_layer_sort_order_a2z(trw_menu_sublayer_t * data);
void trw_layer_sort_order_z2a(trw_menu_sublayer_t * data);
void trw_layer_sort_order_timestamp_ascend(trw_menu_sublayer_t * data);
void trw_layer_sort_order_timestamp_descend(trw_menu_sublayer_t * data);
void trw_layer_waypoints_visibility_off(trw_menu_layer_t * data);
void trw_layer_waypoints_visibility_on(trw_menu_layer_t * data);
void trw_layer_waypoints_visibility_toggle(trw_menu_layer_t * data);
void trw_layer_tracks_visibility_off(trw_menu_layer_t * data);
void trw_layer_tracks_visibility_on(trw_menu_layer_t * data);
void trw_layer_tracks_visibility_toggle(trw_menu_layer_t * data);
void trw_layer_routes_visibility_off(trw_menu_layer_t * data);
void trw_layer_routes_visibility_on(trw_menu_layer_t * data);
void trw_layer_routes_visibility_toggle(trw_menu_layer_t * data);
void trw_layer_tracks_stats(trw_menu_layer_t * data);
void trw_layer_routes_stats(trw_menu_layer_t * data);
bool is_valid_geocache_name(char *str);
#ifndef WINDOWS
void trw_layer_track_use_with_filter(trw_menu_sublayer_t * data);
#endif
void trw_layer_google_route_webpage(trw_menu_sublayer_t * data);
void trw_layer_delete_item(trw_menu_sublayer_t * data);
void trw_layer_copy_item_cb(trw_menu_sublayer_t * data);
void trw_layer_cut_item_cb(trw_menu_sublayer_t * data);




#endif /* #ifndef _SG_LAYER_TRW_H_ */
