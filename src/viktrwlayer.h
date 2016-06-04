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

#ifndef _VIKING_TRWLAYER_H
#define _VIKING_TRWLAYER_H

#include <stdbool.h>
#include <stdint.h>

#include <unordered_map>

#include "viklayer.h"
#include "vikviewport.h"
#include "vikwaypoint.h"
#include "viktrack.h"
#include "viklayerspanel.h"
#include "viktrwlayer_tpwin.h"
#include "layer_trw_containers.h"




struct _VikTrwLayer;


namespace SlavGPS {





	typedef struct {
		char * description;
		char * author;
		//bool has_time;
		char * timestamp; // TODO: Consider storing as proper time_t.
		char * keywords; // TODO: handling/storing a GList of individual tags?
	} VikTRWMetadata;





	// See http://developer.gnome.org/pango/stable/PangoMarkupFormat.html
	typedef enum {
		FS_XX_SMALL = 0, // 'xx-small'
		FS_X_SMALL,
		FS_SMALL,
		FS_MEDIUM, // DEFAULT
		FS_LARGE,
		FS_X_LARGE,
		FS_XX_LARGE,
		FS_NUM_SIZES
	} font_size_t;





	class LayerTRW : public Layer {

	public:
		LayerTRW();
		LayerTRW(VikLayer * vl);
		LayerTRW(Viewport * viewport);


		/* Layer interface methods. */
		void draw(Viewport * viewport);
		void post_read(Viewport * viewport, bool from_file);
		char const * tooltip();
		char const * sublayer_tooltip(int subtype, void * sublayer);

		bool selected(int subtype, void * sublayer, int type, void * vlp);

		bool show_selected_viewport_menu(GdkEventButton * event, Viewport * viewport);

		bool select_click(GdkEventButton * event, Viewport * viewport, tool_ed_t * tet);
		bool select_move(GdkEventMotion * event, Viewport * viewport, tool_ed_t * t);
		bool select_release(GdkEventButton * event, Viewport * viewport, tool_ed_t * t);

		void set_menu_selection(uint16_t selection);
		uint16_t get_menu_selection();

		void marshall(uint8_t ** data, int * len);

		void cut_item(int subtype, sg_uid_t sublayer_uid);
		void copy_item(int subtype, sg_uid_t sublayer_uid, uint8_t ** item, unsigned int * len);
		bool paste_item(int subtype, uint8_t * item, size_t len);
		void delete_item(int subtype, sg_uid_t sublayer_uid);

		void change_coord_mode(VikCoordMode dest_mode);

		time_t get_timestamp();

		void drag_drop_request(Layer * src, GtkTreeIter * src_item_iter, GtkTreePath * dest_path);

		int read_file(FILE * f, char const * dirpath);
		void write_file(FILE * f);
		void add_menu_items(GtkMenu * menu, void * vlp);
		bool sublayer_add_menu_items(GtkMenu * menu, void * panel, int subtype, void * sublayer, GtkTreeIter * iter, Viewport * viewport);
		char const * sublayer_rename_request(const char * newname, void * vlp, int subtype, void * sublayer, GtkTreeIter * iter);
		bool sublayer_toggle_visible(int subtype, void * sublayer);

		void realize(VikTreeview * vt, GtkTreeIter * layer_iter);
		void free_();





		void add_track(Track * trk, char const * name);        /* Formerly known as vik_trw_layer_add_track(VikTrwLayer * vtl, char * name, Track * trk). */
		void add_route(Track * trk, char const * name);        /* Formerly known as vik_trw_layer_add_route(VikTrwLayer * vtl, char * name, Track * trk). */
		void add_waypoint(Waypoint * wp, char const * name);   /* Formerly known as vik_trw_layer_add_waypoint(VikTrwLayer * vtl, char * name, Waypoint * wp). */

		std::unordered_map<sg_uid_t, Track *> & get_tracks();         /* Formerly known as vik_trw_layer_get_tracks(VikTrwLayer * l). */
		std::unordered_map<sg_uid_t, Track *> & get_routes();         /* Formerly known as vik_trw_layer_get_routes(VikTrwLayer * l). */
		std::unordered_map<sg_uid_t, Waypoint *> & get_waypoints();   /* Formerly known as vik_trw_layer_get_waypoints(VikTrwLayer * l). */

		std::unordered_map<sg_uid_t, TreeIndex *> & get_tracks_iters();     /* Formerly known as vik_trw_layer_get_tracks_iters(VikTrwLayer * vtl). */
		std::unordered_map<sg_uid_t, TreeIndex *> & get_routes_iters();     /* Formerly known as vik_trw_layer_get_routes_iters(VikTrwLayer * vtl). */
		std::unordered_map<sg_uid_t, TreeIndex *> & get_waypoints_iters();  /* Formerly known as vik_trw_layer_get_waypoints_iters(VikTrwLayer * vtl). */

		bool get_tracks_visibility();      /* Formerly known as vik_trw_layer_get_tracks_visibility(VikTrwLayer * vtl). */
		bool get_routes_visibility();      /* Formerly known as vik_trw_layer_get_routes_visibility(VikTrwLayer * vtl). */
		bool get_waypoints_visibility();   /* Formerly known as vik_trw_layer_get_waypoints_visibility(VikTrwLayer * vtl). */

		bool is_empty();  /* Formerly known as vik_trw_layer_is_empty(VikTrwLayer * vtl). */


		bool find_by_date(char const * date_str, VikCoord * position, Viewport * viewport, bool do_tracks, bool select);    /* Formerly known as vik_trw_layer_find_date(VikTrwLayer * vtl, char const * date_str, VikCoord * position, VikViewport * vvp, bool do_tracks, bool select). */

		Track * get_track(const char * name); /* Track * vik_trw_layer_get_track(VikTrwLayer * vtl, const char * name). */
		Track * get_route(const char * name); /* Track * vik_trw_layer_get_route(VikTrwLayer * vtl, const char * name). */
		//static Track * find_track_by_name(std::unordered_map<sg_uid_t, Track *> & tracks, char const * name);             /* static Track * trw_layer_track_find(std::unordered_map<sg_uid_t, Track *> & tracks, char const * name). */

		Waypoint * get_waypoint(const char * name); /* Waypoint * vik_trw_layer_get_waypoint(VikTrwLayer * vtl, const char * name). */

		void draw_with_highlight(Viewport * viewport, bool highlight); /* void trw_layer_draw_with_highlight(VikTrwLayer * l, Viewport * viewport, bool highlight). */
		void draw_highlight(Viewport * viewport); /* void vik_trw_layer_draw_highlight(VikTrwLayer * vtl, Viewport * viewport). */

		void draw_highlight_item(Track * trk, Waypoint * wp, Viewport * viewport); /* vik_trw_layer_draw_highlight_item(VikTrwLayer *vtl, Track * trk, Waypoint * wp, Viewport * viewport) */
		void draw_highlight_items(std::unordered_map<sg_uid_t, Track *> * tracks, std::unordered_map<sg_uid_t, Waypoint *> * waypoints, Viewport * viewport); /* vik_trw_layer_draw_highlight_items(VikTrwLayer * vtl, std::unordered_map<sg_uid_t, Track *> * tracks, std::unordered_map<sg_uid_t, Waypoint *> * waypoints, Viewport * viewport). */


		void realize_track(std::unordered_map<sg_uid_t, Track *> & tracks, void * pass_along[4], int sublayer_id); /* static void trw_layer_realize_track(std::unordered_map<sg_uid_t, Track *> & tracks, void * pass_along[5], int sublayer_id). */
		void realize_waypoints(std::unordered_map<sg_uid_t, Waypoint *> & data, void * pass_along[4], int sublayer_id); /* static void trw_layer_realize_waypoints(std::unordered_map<sg_uid_t, Waypoint *> & data, void * pass_along[5]). */


		void add_sublayer_tracks(VikTreeview * vt, GtkTreeIter * layer_iter); /* static void trw_layer_add_sublayer_tracks ( VikTrwLayer *vtl, VikTreeview *vt, GtkTreeIter *layer_iter ) */
		void add_sublayer_waypoints(VikTreeview * vt, GtkTreeIter * layer_iter); /* static void trw_layer_add_sublayer_waypoints ( VikTrwLayer *vtl, VikTreeview *vt, GtkTreeIter *layer_iter ). */
		void add_sublayer_routes(VikTreeview * vt, GtkTreeIter * layer_iter); /* static void trw_layer_add_sublayer_routes ( VikTrwLayer *vtl, VikTreeview *vt, GtkTreeIter *layer_iter ). */


		static void find_maxmin_in_track(const Track * trk, struct LatLon maxmin[2]); /* static void trw_layer_find_maxmin_tracks(const Track * trk, struct LatLon maxmin[2]). */



		void find_maxmin(struct LatLon maxmin[2]); /* static void trw_layer_find_maxmin(VikTrwLayer * vtl, struct LatLon maxmin[2]). */
		bool find_center(VikCoord * dest); /* bool vik_trw_layer_find_center(VikTrwLayer * vtl, VikCoord * dest). */

		void set_statusbar_msg_info_trkpt(Trackpoint * tp); /* static void set_statusbar_msg_info_trkpt ( VikTrwLayer *vtl, Trackpoint * tp). */
		void set_statusbar_msg_info_wpt(Waypoint * wp); /* static void set_statusbar_msg_info_wpt(VikTrwLayer * vtl, Waypoint * wp). */

		void zoom_to_show_latlons(Viewport * viewport, struct LatLon maxmin[2]); /* void trw_layer_zoom_to_show_latlons(VikTrwLayer * vtl, VikViewport * vvp, struct LatLon maxmin[2]). */
		bool auto_set_view(Viewport * viewport); /* bool vik_trw_layer_auto_set_view(VikTrwLayer * vtl, VikViewport * vvp). */



		bool new_waypoint(GtkWindow * w, const VikCoord * def_coord); /* bool vik_trw_layer_new_waypoint(VikTrwLayer * vtl, GtkWindow * w, const VikCoord * def_coord). */
		void new_track_create_common(char * name); /* static void new_track_create_common(VikTrwLayer * vtl, char * name). */
		void new_route_create_common(char * name); /* static void new_route_create_common(VikTrwLayer * vtl, char * name). */




		void cancel_tps_of_track(Track * trk); /* void trw_layer_cancel_tps_of_track(VikTrwLayer * vtl, Track * trk). */

		void reset_waypoints(); /* void vik_trw_layer_reset_waypoints(VikTrwLayer *vtl). */

		char * new_unique_sublayer_name(int sublayer_type, const char * name); /* char *trw_layer_new_unique_sublayer_name (VikTrwLayer *vtl, int sublayer_type, const char *name). */


		/* These are meant for use in file loaders (gpspoint.c, gpx.c, etc).
		 * These copy the name, so you should free it if necessary. */
		void filein_add_waypoint(char * name, Waypoint * wp); /* void vik_trw_layer_filein_add_waypoint(VikTrwLayer * vtl, char * name, Waypoint * wp). */
		void filein_add_track(char * name, Track * trk); /* void vik_trw_layer_filein_add_track(VikTrwLayer * vtl, char * name, Track * trk). */




		void move_item(_VikTrwLayer * vtl_dest, void * id, int type); /* static void trw_layer_move_item(VikTrwLayer * vtl_src, VikTrwLayer * vtl_dest, void * id, int type). */



		bool delete_track(Track * trk);                                 /* Formerly known as vik_trw_layer_delete_track(VikTrwLayer * vtl, Track * trk). */
		bool delete_track_by_name(char const * name, bool is_route);    /* Formerly known as trw_layer_delete_track_by_name( VikTrwLayer *vtl, const char *name, std::unordered_map<sg_uid_t, Track *> * ht_tracks). */
		bool delete_route(Track * trk);                                 /* Formerly known as vik_trw_layer_delete_route(VikTrwLayer * vtl, Track * trk). */
		bool delete_waypoint(Waypoint * wp);                            /* Formerly known as trw_layer_delete_waypoint(VikTrwLayer * vtl, Waypoint * wp). */
		bool delete_waypoint_by_name(char const * name);                /* Formerly known as trw_layer_delete_waypoint_by_name(VikTrwLayer * vtl, char const * name). */


		void delete_all_routes();         /* void vik_trw_layer_delete_all_routes(VikTrwLayer * vtl). */
		void delete_all_tracks();         /* void vik_trw_layer_delete_all_tracks(VikTrwLayer * vtl). */
		void delete_all_waypoints();      /* void vik_trw_layer_delete_all_waypoints(VikTrwLayer * vtl). */


		void waypoint_rename(Waypoint * wp, char const * new_name); /* void trw_layer_waypoint_rename(VikTrwLayer * vtl, Waypoint * wp, const char * new_name). */
		void waypoint_reset_icon(Waypoint * wp); /* void trw_layer_waypoint_reset_icon ( VikTrwLayer *vtl, Waypoint * wp). */
		void update_treeview(Track * trk); /* void trw_layer_update_treeview(VikTrwLayer * vtl, Track * trk). */

		bool dem_test(VikLayersPanel * vlp); /* static bool trw_layer_dem_test(VikTrwLayer * vtl, VikLayersPanel * vlp). */
		void apply_dem_data_common(VikLayersPanel * vlp, Track * trk, bool skip_existing_elevations); /* static void apply_dem_data_common(VikTrwLayer * vtl, VikLayersPanel * vlp, Track * trk, bool skip_existing_elevations). */
		void smooth_it(Track * trk, bool flat); /* static void smooth_it(VikTrwLayer * vtl, Track * trk, bool flat). */
		void wp_changed_message(int changed); /* static void wp_changed_message(VikTrwLayer * vtl, int changed). */



		void split_at_selected_trackpoint(int subtype); /* static void trw_layer_split_at_selected_trackpoint(VikTrwLayer * vtl, int subtype). */
		void trackpoint_selected_delete(Track * trk); /* static void trw_layer_trackpoint_selected_delete(VikTrwLayer * vtl, Track * trk). */


		void diary_open(char const * date_str); /* void trw_layer_diary_open(VikTrwLayer * vtl, char const * date_str) */
		void astro_open(char const * date_str,  char const * time_str, char const * lat_str, char const * lon_str, char const * alt_str); /* static void trw_layer_astro_open(VikTrwLayer * vtl, char const * date_str,  char const * time_str, char const * lat_str, char const * lon_str, char const * alt_str). */



		void uniquify_tracks(LayersPanel * panel, std::unordered_map<sg_uid_t, Track *> & track_table, bool ontrack); /* static void vik_trw_layer_uniquify_tracks(VikTrwLayer * vtl, VikLayersPanel * vlp, std::unordered_map<sg_uid_t, Track *> & track_table, bool ontrack). */
		void sort_order_specified(unsigned int sublayer_type, vik_layer_sort_order_t order); /* void trw_layer_sort_order_specified(VikTrwLayer * vtl, unsigned int sublayer_type, vik_layer_sort_order_t order). */

		bool has_same_waypoint_names(); /* bool trw_layer_has_same_waypoint_names(VikTrwLayer * vtl). */
		void uniquify_waypoints(LayersPanel * panel); /* static void vik_trw_layer_uniquify_waypoints(VikTrwLayer * vtl, VikLayersPanel * vlp). */



		GList * build_waypoint_list_t(GList * waypoints); /* GList * vik_trw_layer_build_waypoint_list_t(VikTrwLayer * vtl, GList * waypoints). */
		GList * build_track_list_t(GList * tracks); /* GList * vik_trw_layer_build_track_list_t(VikTrwLayer * vtl, GList * tracks). */




		void tpwin_init(); /* static void trw_layer_tpwin_init(VikTrwLayer * vtl). */




		Trackpoint * closest_tp_in_five_pixel_interval(Viewport * viewport, int x, int y); /* static Trackpoint * closest_tp_in_five_pixel_interval(VikTrwLayer * vtl, Viewport * viewport, int x, int y). */
		Waypoint * closest_wp_in_five_pixel_interval(Viewport * viewport, int x, int y); /* static Waypoint * closest_wp_in_five_pixel_interval(VikTrwLayer * vtl, Viewport * viewport, int x, int y). */


		void track_alloc_colors(); /* static void trw_layer_track_alloc_colors(VikTrwLayer * vtl). */

		void calculate_bounds_waypoints(); /* void trw_layer_calculate_bounds_waypoints(VikTrwLayer * vtl). */

		static void calculate_bounds_track(std::unordered_map<sg_uid_t, Track *> & tracks); /* static void trw_layer_calculate_bounds_track(std::unordered_map<sg_uid_t, Track *> & tracks). */

		void calculate_bounds_tracks(); /* static void trw_layer_calculate_bounds_tracks(VikTrwLayer * vtl). */


		void sort_all(); /* static void trw_layer_sort_all(VikTrwLayer * vtl). */

		time_t get_timestamp_tracks(); /* static time_t trw_layer_get_timestamp_tracks(VikTrwLayer * vtl). */
		time_t get_timestamp_waypoints(); /* static time_t trw_layer_get_timestamp_waypoints(VikTrwLayer * vtl). */



		VikCoordMode get_coord_mode(); /* VikCoordMode vik_trw_layer_get_coord_mode(VikTrwLayer * vtl). */

		bool uniquify(LayersPanel * panel); /* bool vik_trw_layer_uniquify(VikTrwLayer * vtl, VikLayersPanel * vlp). */


		void highest_wp_number_reset(); /* static void highest_wp_number_reset(VikTrwLayer * vtl) */
		void highest_wp_number_add_wp(char const * new_wp_name); /* void highest_wp_number_add_wp(VikTrwLayer * vtl, char const * new_wp_name). */
		void highest_wp_number_remove_wp(char const * old_wp_name); /* static void highest_wp_number_remove_wp(VikTrwLayer * vtl, char const * old_wp_name). */
		char * highest_wp_number_get(); /* char * highest_wp_number_get(VikTrwLayer * vtl). */


#ifdef VIK_CONFIG_GOOGLE
		bool is_valid_google_route(sg_uid_t track_uid); /* static bool is_valid_google_route(VikTrwLayer * vtl, const void * track_id). */
#endif

		void insert_tp_beside_current_tp(bool before); /* static void trw_layer_insert_tp_beside_current_tp(VikTrwLayer * vtl, bool before). */

		void my_tpwin_set_tp(); /* static void my_tpwin_set_tp(VikTrwLayer * vtl). */


		void dialog_shift(GtkWindow * dialog, VikCoord * coord, bool vertical); /* void trw_layer_dialog_shift(VikTrwLayer * vtl, GtkWindow * dialog, VikCoord * coord, bool vertical). */



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


		/* Waypoint editing tool */
		Waypoint * current_wp;
		sg_uid_t current_wp_uid;
		bool moving_wp;
		bool waypoint_rightclick;

		/* track editing tool */
		GList * current_tpl;       /* List of trackpoints, to which belongs currently selected trackpoint (tp)?. The list would be a member of current track current_tp_track. */
		Track * current_tp_track;  /* Track, to which belongs currently selected trackpoint (tp)? */
		sg_uid_t current_tp_uid;   /* uid of track, to which belongs currently selected trackpoint (tp)? */
		VikTrwLayerTpwin *tpwin;

		/* track editing tool -- more specifically, moving tps */
		bool moving_tp;

		VikCoordMode coord_mode;

		int highest_wp_number;



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

		// Metadata
		VikTRWMetadata * metadata;

		PangoLayout * tracklabellayout;
		font_size_t track_font_size;
		char * track_fsize_str;

		uint8_t wp_symbol;
		uint8_t wp_size;
		bool wp_draw_symbols;
		font_size_t wp_font_size;
		char * wp_fsize_str;
		vik_layer_sort_order_t wp_sort_order;


		double track_draw_speed_factor;
		GArray * track_gc;
		GdkGC * track_1color_gc;
		GdkColor track_color;
		GdkGC * current_track_gc;
		// Separate GC for a track's potential new point as drawn via separate method
		//  (compared to the actual track points drawn in the main trw_layer_draw_track function)
		GdkGC * current_track_newpoint_gc;

		GdkGC * track_bg_gc;
		GdkColor track_bg_color;

		GdkGC * waypoint_gc;
		GdkColor waypoint_color;

		GdkGC * waypoint_text_gc;
		GdkColor waypoint_text_color;

		GdkGC * waypoint_bg_gc;
		GdkColor waypoint_bg_color;

		GdkFunction wpbgand;



		Track * current_track; // ATM shared between new tracks and new routes
		uint16_t ct_x1;
		uint16_t ct_y1;
		uint16_t ct_x2;
		uint16_t ct_y2;
		bool draw_sync_done;
		bool draw_sync_do;

		/* route finder tool */
		bool route_finder_started;
		bool route_finder_check_added_track;
		Track * route_finder_added_track;
		bool route_finder_append;


		bool drawlabels;
		bool drawimages;
		uint8_t image_alpha;
		GQueue * image_cache;
		uint8_t image_size;
		uint16_t image_cache_size;




		/* for waypoint text */
		PangoLayout * wplabellayout;

		bool has_verified_thumbnails;

		GtkMenu * wp_right_click_menu;
		GtkMenu * track_right_click_menu;

		/* menu */
		VikStdLayerMenuItem menu_selection;


		// One per layer
		GtkWidget * tracks_analysis_dialog;


	};





}





using namespace SlavGPS;



typedef struct {
	LayerTRW * layer;
	LayersPanel * panel;
} trw_menu_layer_t;


typedef struct {
	LayerTRW * layer;
	LayersPanel * panel;
	int subtype;
	sg_uid_t sublayer_id;
	bool confirm;
	Viewport * viewport;
	GtkTreeIter * tv_iter;
	void * misc;
} trw_menu_sublayer_t;



typedef void * menu_array_layer[2];
typedef void * menu_array_sublayer[8];






struct _VikTrwLayer {
	VikLayer vl;

	LayerTRW * trw;
};




#ifdef __cplusplus
extern "C" {
#endif


#define VIK_TRW_LAYER_TYPE            (vik_trw_layer_get_type ())
#define VIK_TRW_LAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_TRW_LAYER_TYPE, VikTrwLayer))
#define VIK_TRW_LAYER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_TRW_LAYER_TYPE, VikTrwLayerClass))
#define IS_VIK_TRW_LAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_TRW_LAYER_TYPE))
#define IS_VIK_TRW_LAYER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_TRW_LAYER_TYPE))

enum {
  VIK_TRW_LAYER_SUBLAYER_TRACKS,
  VIK_TRW_LAYER_SUBLAYER_WAYPOINTS,
  VIK_TRW_LAYER_SUBLAYER_TRACK,
  VIK_TRW_LAYER_SUBLAYER_WAYPOINT,
  VIK_TRW_LAYER_SUBLAYER_ROUTES,
  VIK_TRW_LAYER_SUBLAYER_ROUTE
};

typedef struct _VikTrwLayerClass VikTrwLayerClass;
struct _VikTrwLayerClass
{
  VikLayerClass object_class;
};


GType vik_trw_layer_get_type ();

typedef struct _VikTrwLayer VikTrwLayer;



VikTRWMetadata *vik_trw_metadata_new();
void vik_trw_metadata_free ( VikTRWMetadata *metadata);
VikTRWMetadata *vik_trw_layer_get_metadata ( VikTrwLayer *vtl );
void vik_trw_layer_set_metadata ( VikTrwLayer *vtl, VikTRWMetadata *metadata);

//bool vik_trw_layer_find_date ( VikTrwLayer *vtl, const char *date_str, VikCoord *position, VikViewport *vvp, bool do_tracks, bool select );

int vik_trw_layer_get_property_tracks_line_thickness ( VikTrwLayer *vtl );

// Waypoint returned is the first one
Waypoint * vik_trw_layer_get_waypoint(VikTrwLayer * vtl, const char * name);

// Track returned is the first one
Track * vik_trw_layer_get_track(VikTrwLayer * vtl, const char * name);

bool vik_trw_layer_find_center ( VikTrwLayer *vtl, VikCoord *dest );

VikCoordMode vik_trw_layer_get_coord_mode ( VikTrwLayer *vtl );

bool vik_trw_layer_uniquify ( VikTrwLayer *vtl, VikLayersPanel *vlp );

void vik_trw_layer_delete_all_waypoints ( VikTrwLayer *vtl );
void vik_trw_layer_delete_all_tracks ( VikTrwLayer *vtl );
void vik_trw_layer_delete_all_routes ( VikTrwLayer *vtl );

void vik_trw_layer_reset_waypoints ( VikTrwLayer *vtl );

void vik_trw_layer_draw_highlight(VikTrwLayer * vtl, Viewport * viewport);
void vik_trw_layer_draw_highlight_item(VikTrwLayer * vtl, Track * trk, Waypoint * wp, Viewport * viewport);
void vik_trw_layer_draw_highlight_items(VikTrwLayer * vtl, std::unordered_map<sg_uid_t, Track *> * tracks, std::unordered_map<sg_uid_t, Waypoint *> * waypoints, Viewport * viewport);

// For creating a list of tracks with the corresponding layer it is in
//  (thus a selection of tracks may be from differing layers)
typedef struct {
  Track * trk;
  VikTrwLayer *vtl;
} vik_trw_track_list_t;

typedef GList* (*VikTrwlayerGetTracksAndLayersFunc) (VikLayer*, void *);
GList *vik_trw_layer_build_track_list_t ( VikTrwLayer *vtl, GList *tracks );

// For creating a list of waypoints with the corresponding layer it is in
//  (thus a selection of waypoints may be from differing layers)
typedef struct {
  Waypoint * wp;
  VikTrwLayer *vtl;
} vik_trw_waypoint_list_t;

typedef GList* (*VikTrwlayerGetWaypointsAndLayersFunc) (VikLayer*, void *);
GList *vik_trw_layer_build_waypoint_list_t ( VikTrwLayer *vtl, GList *waypoints );

GdkPixbuf* get_wp_sym_small ( char *symbol );

/* Exposed Layer Interface function definitions */
// Intended only for use by other trw_layer subwindows
void trw_layer_verify_thumbnails(VikTrwLayer * vtl, Viewport * viewport);
void trw_layer_calculate_bounds_waypoints ( VikTrwLayer *vtl );


typedef struct {
	bool    has_same_track_name;
	const char *same_track_name;
} same_track_name_udata;

int sort_alphabetically (gconstpointer a, gconstpointer b, void * user_data);
int check_tracks_for_same_name ( gconstpointer aa, gconstpointer bb, void * udata );


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
// Specific route versions:
//  Most track handling functions can handle operating on the route list
//  However these ones are easier in separate functions
void trw_layer_auto_routes_view(trw_menu_layer_t * data);
void trw_layer_delete_all_routes(trw_menu_layer_t * data);
void trw_layer_delete_routes_from_selection(trw_menu_layer_t * data);
/* pop-up items */
void trw_layer_properties_item(trw_menu_sublayer_t * data); //TODO??
void trw_layer_goto_waypoint(trw_menu_sublayer_t * data);
void trw_layer_waypoint_gc_webpage(trw_menu_sublayer_t * data);
void trw_layer_waypoint_webpage(trw_menu_sublayer_t * data);
void trw_layer_paste_item_cb(trw_menu_sublayer_t * data);
void trw_layer_export_gpspoint(trw_menu_layer_t * data);
void trw_layer_export_gpsmapper(trw_menu_layer_t * data);
void trw_layer_export_gpx(trw_menu_layer_t * data);
void trw_layer_export_kml(trw_menu_layer_t * data);
void trw_layer_export_geojson(trw_menu_layer_t * data);
void trw_layer_export_babel(void * layer_and_vlp[2]);
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






#ifdef __cplusplus
}
#endif




GList * vik_trw_layer_get_track_values(GList ** list, std::unordered_map<sg_uid_t, Track *> & tracks);




#endif
