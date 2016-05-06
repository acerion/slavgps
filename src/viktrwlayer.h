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




struct _VikTrwLayer;


namespace SlavGPS {





	typedef GtkTreeIter TreeIndex;





	typedef struct {
		bool found;
		const char *date_str;
		const Track * trk;
		const Waypoint * wp;
		sg_uid_t trk_uid;
		sg_uid_t wp_uid;
	} date_finder_type;




	typedef struct {
		GList ** result;
		Track * exclude;
		bool with_timestamps;
	} twt_udata;


	typedef struct {
		int x, y;
		int closest_x, closest_y;
		bool draw_images;
		sg_uid_t closest_wp_uid;
		Waypoint * closest_wp;
		Viewport * viewport;
	} WPSearchParams;

	typedef struct {
		int x, y;
		int closest_x, closest_y;
		sg_uid_t closest_track_uid;
		Trackpoint * closest_tp;
		Viewport * viewport;
		GList * closest_tpl;
		LatLonBBox bbox;
	} TPSearchParams;







	class LayerTRW {

	public:

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



		static sg_uid_t find_uid_of_track(std::unordered_map<sg_uid_t, Track *> & input, Track * trk);                       /* Formerly known as trw_layer_track_find_uuid(std::unordered_map<sg_uid_t, Track *> & tracks, Track * trk). */
		static sg_uid_t find_uid_of_waypoint_by_name(std::unordered_map<sg_uid_t, Waypoint *> & input, char const * name);   /* Formerly known as trw_layer_waypoint_find_uuid_by_name(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, Waypoint * wp). */
		static Track *  find_track_by_name(std::unordered_map<sg_uid_t, Track *> & input, char const * name);                /* Formerly known as trw_layer_track_find_uuid_by_name(std::unordered_map<sg_uid_t, Track *> * tracks, char const * name). */
		static sg_uid_t find_uid_of_waypoint(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, Waypoint * wp); /* sg_uid_t trw_layer_waypoint_find_uuid(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, Waypoint * wp). */


		bool find_by_date(char const * date_str, VikCoord * position, VikViewport * vvp, bool do_tracks, bool select);    /* Formerly known as vik_trw_layer_find_date(VikTrwLayer * vtl, char const * date_str, VikCoord * position, VikViewport * vvp, bool do_tracks, bool select). */
		static bool find_track_by_date(std::unordered_map<sg_uid_t, Track *> & tracks, date_finder_type * df);            /* Formerly known as trw_layer_find_date_track(std::unordered_map<sg_uid_t, Track *> & tracks, date_finder_type * df). */
		Track * get_track(const char * name); /* Track * vik_trw_layer_get_track(VikTrwLayer * vtl, const char * name). */
		Track * get_route(const char * name); /* Track * vik_trw_layer_get_route(VikTrwLayer * vtl, const char * name). */
		//static Track * find_track_by_name(std::unordered_map<sg_uid_t, Track *> & tracks, char const * name);             /* static Track * trw_layer_track_find(std::unordered_map<sg_uid_t, Track *> & tracks, char const * name). */
		static bool find_waypoint_by_date(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, date_finder_type * df);   /* Formerly known as trw_layer_find_date_waypoint(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, date_finder_type * df). */
		static Waypoint * find_waypoint_by_name(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, const char * name);        /* static Waypoint * trw_layer_waypoint_find(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, const char * name). */
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
		static void find_maxmin_in_tracks(std::unordered_map<sg_uid_t, Track *> & tracks, struct LatLon maxmin[2]); /* static void trw_layer_find_maxmin_tracks_2(std::unordered_map<sg_uid_t, Track *> & tracks, struct LatLon maxmin[2]). */


		void find_maxmin(struct LatLon maxmin[2]); /* static void trw_layer_find_maxmin(VikTrwLayer * vtl, struct LatLon maxmin[2]). */
		bool find_center(VikCoord * dest); /* bool vik_trw_layer_find_center(VikTrwLayer * vtl, VikCoord * dest). */

		void set_statusbar_msg_info_trkpt(Trackpoint * tp); /* static void set_statusbar_msg_info_trkpt ( VikTrwLayer *vtl, Trackpoint * tp). */
		void set_statusbar_msg_info_wpt(Waypoint * wp); /* static void set_statusbar_msg_info_wpt(VikTrwLayer * vtl, Waypoint * wp). */

		void zoom_to_show_latlons(VikViewport * vvp, struct LatLon maxmin[2]); /* void trw_layer_zoom_to_show_latlons(VikTrwLayer * vtl, VikViewport * vvp, struct LatLon maxmin[2]). */
		bool auto_set_view(VikViewport * vvp); /* bool vik_trw_layer_auto_set_view(VikTrwLayer * vtl, VikViewport * vvp). */



		bool new_waypoint(GtkWindow * w, const VikCoord * def_coord); /* bool vik_trw_layer_new_waypoint(VikTrwLayer * vtl, GtkWindow * w, const VikCoord * def_coord). */
		void new_track_create_common(char * name); /* static void new_track_create_common(VikTrwLayer * vtl, char * name). */
		void new_route_create_common(char * name); /* static void new_route_create_common(VikTrwLayer * vtl, char * name). */



		static void single_waypoint_jump(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, Viewport * viewport); /* static void trw_layer_single_waypoint_jump(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, Viewport * viewport). */
		void cancel_tps_of_track(Track * trk); /* void trw_layer_cancel_tps_of_track(VikTrwLayer * vtl, Track * trk). */

		void reset_waypoints(); /* void vik_trw_layer_reset_waypoints(VikTrwLayer *vtl). */

		char * new_unique_sublayer_name(int sublayer_type, const char * name); /* char *trw_layer_new_unique_sublayer_name (VikTrwLayer *vtl, int sublayer_type, const char *name). */


		void filein_add_waypoint(char * name, Waypoint * wp); /* void vik_trw_layer_filein_add_waypoint(VikTrwLayer * vtl, char * name, Waypoint * wp). */
		void filein_add_track(char * name, Track * trk); /* void vik_trw_layer_filein_add_track(VikTrwLayer * vtl, char * name, Track * trk). */


		static void list_wp_uids(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, GList ** l); /* static void trw_layer_enum_item_wp(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, GList ** l). */
		static void list_trk_uids(std::unordered_map<sg_uid_t, Track *> & tracks, GList ** l); /* void trw_layer_enum_item_trk(std::unordered_map<sg_uid_t, Track *> & tracks, GList ** l). */


		void move_item(_VikTrwLayer * vtl_dest, void * id, int type); /* static void trw_layer_move_item(VikTrwLayer * vtl_src, VikTrwLayer * vtl_dest, void * id, int type). */


		static void remove_item_from_treeview(std::unordered_map<sg_uid_t, TreeIndex *> & items, VikTreeview * vt); /* static void remove_item_from_treeview(std::unordered_map<sg_uid_t, TreeIndex *> & items, VikTreeview * vt). */


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


		static GList * find_tracks_with_timestamp_type(std::unordered_map<sg_uid_t, Track *> * tracks, bool with_timestamps, Track * exclude);
		static GList * find_nearby_tracks_by_time(std::unordered_map<sg_uid_t, Track *> & tracks, Track * orig_trk, unsigned int threshold);
		static void    sorted_track_id_by_name_list_exclude_self(std::unordered_map<sg_uid_t, Track *> * tracks, twt_udata * user_data);


		void split_at_selected_trackpoint(int subtype); /* static void trw_layer_split_at_selected_trackpoint(VikTrwLayer * vtl, int subtype). */
		void trackpoint_selected_delete(Track * trk); /* static void trw_layer_trackpoint_selected_delete(VikTrwLayer * vtl, Track * trk). */


		void diary_open(char const * date_str); /* void trw_layer_diary_open(VikTrwLayer * vtl, char const * date_str) */
		void astro_open(char const * date_str,  char const * time_str, char const * lat_str, char const * lon_str, char const * alt_str); /* static void trw_layer_astro_open(VikTrwLayer * vtl, char const * date_str,  char const * time_str, char const * lat_str, char const * lon_str, char const * alt_str). */



		static void sorted_wp_id_by_name_list(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, GList **list); /* static void trw_layer_sorted_wp_id_by_name_list(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, GList **list). */
		static GList * sorted_track_id_by_name_list(std::unordered_map<sg_uid_t, Track *> & tracks); /* static GList * trw_layer_sorted_track_id_by_name_list(std::unordered_map<sg_uid_t, Track *> & tracks). */
		static bool has_same_track_names(std::unordered_map<sg_uid_t, Track *> & ht_tracks); /* static bool trw_layer_has_same_track_names(std::unordered_map<sg_uid_t, Track *> & ht_tracks). */


		void uniquify_tracks(VikLayersPanel * vlp, std::unordered_map<sg_uid_t, Track *> & track_table, bool ontrack); /* static void vik_trw_layer_uniquify_tracks(VikTrwLayer * vtl, VikLayersPanel * vlp, std::unordered_map<sg_uid_t, Track *> & track_table, bool ontrack). */
		void sort_order_specified(unsigned int sublayer_type, vik_layer_sort_order_t order); /* void trw_layer_sort_order_specified(VikTrwLayer * vtl, unsigned int sublayer_type, vik_layer_sort_order_t order). */

		bool has_same_waypoint_names(); /* bool trw_layer_has_same_waypoint_names(VikTrwLayer * vtl). */
		void uniquify_waypoints(VikLayersPanel * vlp); /* static void vik_trw_layer_uniquify_waypoints(VikTrwLayer * vtl, VikLayersPanel * vlp). */

		static void iter_visibility_toggle(std::unordered_map<sg_uid_t, TreeIndex *> & items, VikTreeview * vt); /*static void trw_layer_iter_visibility_toggle(std::unordered_map<sg_uid_t, TreeIndex *> & items, VikTreeview * vt). */
		static void set_iter_visibility(std::unordered_map<sg_uid_t, TreeIndex *> & items, VikTreeview * vt, bool on_off); /* static void trw_layer_iter_visibility(std::unordered_map<sg_uid_t, TreeIndex *> & items, VikTreeview * vt, bool on_off). */
		static void set_waypoints_visibility(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, bool on_off); /* static void trw_layer_waypoints_visibility(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, bool on_off). */
		static void waypoints_toggle_visibility(std::unordered_map<sg_uid_t, Waypoint *> & waypoints); /* static void trw_layer_waypoints_toggle_visibility(std::unordered_map<sg_uid_t, Waypoint *> & waypoints). */
		static void set_tracks_visibility(std::unordered_map<sg_uid_t, Track *> & tracks, bool on_off); /* static void trw_layer_tracks_visibility(std::unordered_map<sg_uid_t, Track *> & tracks, bool on_off). */
		static void tracks_toggle_visibility(std::unordered_map<sg_uid_t, Track *> & tracks); /* static void trw_layer_tracks_toggle_visibility(std::unordered_map<sg_uid_t, Track *> & tracks). */


		GList * build_waypoint_list_t(GList * waypoints); /* GList * vik_trw_layer_build_waypoint_list_t(VikTrwLayer * vtl, GList * waypoints). */
		GList * build_track_list_t(GList * tracks); /* GList * vik_trw_layer_build_track_list_t(VikTrwLayer * vtl, GList * tracks). */


		static GList * get_track_values(GList ** list, std::unordered_map<sg_uid_t, Track *> & tracks); /* GList * vik_trw_layer_get_track_values(GList ** list, std::unordered_map<sg_uid_t, Track *> & tracks). */

		void tpwin_init(); /* static void trw_layer_tpwin_init(VikTrwLayer * vtl). */


		static void waypoint_search_closest_tp(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, WPSearchParams * params);
		static void track_search_closest_tp(std::unordered_map<sg_uid_t, Track *> & tracks, TPSearchParams * params);


		Trackpoint * closest_tp_in_five_pixel_interval(Viewport * viewport, int x, int y); /* static Trackpoint * closest_tp_in_five_pixel_interval(VikTrwLayer * vtl, Viewport * viewport, int x, int y). */
		Waypoint * closest_wp_in_five_pixel_interval(Viewport * viewport, int x, int y); /* static Waypoint * closest_wp_in_five_pixel_interval(VikTrwLayer * vtl, Viewport * viewport, int x, int y). */

		static char * tool_show_picture_wp(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, int event_x, int event_y, Viewport * viewport);
		static GSList * image_wp_make_list(std::unordered_map<sg_uid_t, Waypoint *> & waypoints);


		void track_alloc_colors(); /* static void trw_layer_track_alloc_colors(VikTrwLayer * vtl). */

		void calculate_bounds_waypoints(); /* void trw_layer_calculate_bounds_waypoints(VikTrwLayer * vtl). */

		static void calculate_bounds_track(std::unordered_map<sg_uid_t, Track *> & tracks); /* static void trw_layer_calculate_bounds_track(std::unordered_map<sg_uid_t, Track *> & tracks). */

		void calculate_bounds_tracks(); /* static void trw_layer_calculate_bounds_tracks(VikTrwLayer * vtl). */


		void sort_all(); /* static void trw_layer_sort_all(VikTrwLayer * vtl). */

		time_t get_timestamp_tracks(); /* static time_t trw_layer_get_timestamp_tracks(VikTrwLayer * vtl). */
		time_t get_timestamp_waypoints(); /* static time_t trw_layer_get_timestamp_waypoints(VikTrwLayer * vtl). */



		VikCoordMode get_coord_mode(); /* VikCoordMode vik_trw_layer_get_coord_mode(VikTrwLayer * vtl). */

		bool uniquify(VikLayersPanel * vlp); /* bool vik_trw_layer_uniquify(VikTrwLayer * vtl, VikLayersPanel * vlp). */


		static void waypoints_convert(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, VikCoordMode * dest_mode);
		static void track_convert(std::unordered_map<sg_uid_t, Track *> & tracks, VikCoordMode * dest_mode);

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


		void * vtl; /* Reference to parent object of type VikTrackLayer. */


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

	};





}





using namespace SlavGPS;


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


typedef struct {
	char * description;
	char * author;
	//bool has_time;
	char * timestamp; // TODO: Consider storing as proper time_t.
	char * keywords; // TODO: handling/storing a GList of individual tags?
} VikTRWMetadata;


struct _VikTrwLayer {
	VikLayer vl;

	LayerTRW trw;


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
	VikTRWMetadata *metadata;

	PangoLayout *tracklabellayout;
	font_size_t track_font_size;
	char *track_fsize_str;

	uint8_t wp_symbol;
	uint8_t wp_size;
	bool wp_draw_symbols;
	font_size_t wp_font_size;
	char *wp_fsize_str;
	vik_layer_sort_order_t wp_sort_order;

	double track_draw_speed_factor;
	GArray *track_gc;
	GdkGC *track_1color_gc;
	GdkColor track_color;
	GdkGC *current_track_gc;
	// Separate GC for a track's potential new point as drawn via separate method
	//  (compared to the actual track points drawn in the main trw_layer_draw_track function)
	GdkGC *current_track_newpoint_gc;
	GdkGC *track_bg_gc; GdkColor track_bg_color;
	GdkGC *waypoint_gc; GdkColor waypoint_color;
	GdkGC *waypoint_text_gc; GdkColor waypoint_text_color;
	GdkGC *waypoint_bg_gc; GdkColor waypoint_bg_color;
	GdkFunction wpbgand;
	Track * current_track; // ATM shared between new tracks and new routes
	uint16_t ct_x1, ct_y1, ct_x2, ct_y2;
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
	GQueue *image_cache;
	uint8_t image_size;
	uint16_t image_cache_size;

	/* for waypoint text */
	PangoLayout *wplabellayout;

	bool has_verified_thumbnails;

	GtkMenu *wp_right_click_menu;
	GtkMenu *track_right_click_menu;

	/* menu */
	VikStdLayerMenuItem menu_selection;


	// One per layer
	GtkWidget *tracks_analysis_dialog;
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

bool vik_trw_layer_find_date ( VikTrwLayer *vtl, const char *date_str, VikCoord *position, VikViewport *vvp, bool do_tracks, bool select );

/* These are meant for use in file loaders (gpspoint.c, gpx.c, etc).
 * These copy the name, so you should free it if necessary. */
//void vik_trw_layer_filein_add_waypoint ( VikTrwLayer *vtl, char *name, Waypoint * wp);
//void vik_trw_layer_filein_add_track ( VikTrwLayer *vtl, char *name, Track * trk);

int vik_trw_layer_get_property_tracks_line_thickness ( VikTrwLayer *vtl );


// Waypoint returned is the first one
Waypoint * vik_trw_layer_get_waypoint(VikTrwLayer * vtl, const char * name);

// Track returned is the first one
Track * vik_trw_layer_get_track(VikTrwLayer * vtl, const char * name);
//bool vik_trw_layer_delete_track ( VikTrwLayer *vtl, Track * trk);
//bool vik_trw_layer_delete_route ( VikTrwLayer *vtl, Track * trk);

// bool vik_trw_layer_auto_set_view ( VikTrwLayer *vtl, VikViewport *vvp );
bool vik_trw_layer_find_center ( VikTrwLayer *vtl, VikCoord *dest );

//bool vik_trw_layer_new_waypoint ( VikTrwLayer *vtl, GtkWindow *w, const VikCoord *def_coord );

VikCoordMode vik_trw_layer_get_coord_mode ( VikTrwLayer *vtl );

bool vik_trw_layer_uniquify ( VikTrwLayer *vtl, VikLayersPanel *vlp );

void vik_trw_layer_delete_all_waypoints ( VikTrwLayer *vtl );
void vik_trw_layer_delete_all_tracks ( VikTrwLayer *vtl );
void vik_trw_layer_delete_all_routes ( VikTrwLayer *vtl );
//void trw_layer_cancel_tps_of_track ( VikTrwLayer *vtl, Track * trk);

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
void trw_layer_verify_thumbnails ( VikTrwLayer *vtl, GtkWidget *vp );
// Other functions only for use by other trw_layer subwindows
//char *trw_layer_new_unique_sublayer_name ( VikTrwLayer *vtl, int sublayer_type, const char *name );
//void trw_layer_waypoint_rename ( VikTrwLayer *vtl, Waypoint * wp, const char *new_name );
//void trw_layer_waypoint_reset_icon ( VikTrwLayer *vtl, Waypoint * wp);
void trw_layer_calculate_bounds_waypoints ( VikTrwLayer *vtl );

//void trw_layer_update_treeview ( VikTrwLayer *vtl, Track * trk);

//void trw_layer_dialog_shift ( VikTrwLayer *vtl, GtkWindow *dialog, VikCoord *coord, bool vertical );

//sg_uid_t trw_layer_waypoint_find_uuid(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, Waypoint * wp);

//void trw_layer_zoom_to_show_latlons ( VikTrwLayer *vtl, VikViewport *vvp, struct LatLon maxmin[2] );


#define VIK_SETTINGS_LIST_DATE_FORMAT "list_date_format"

#ifdef __cplusplus
}
#endif




GList * vik_trw_layer_get_track_values(GList ** list, std::unordered_map<sg_uid_t, Track *> & tracks);




#endif
