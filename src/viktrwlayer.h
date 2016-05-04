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


		bool find_by_date(char const * date_str, VikCoord * position, VikViewport * vvp, bool do_tracks, bool select);    /* Formerly known as vik_trw_layer_find_date(VikTrwLayer * vtl, char const * date_str, VikCoord * position, VikViewport * vvp, bool do_tracks, bool select). */
		static bool find_track_by_date(std::unordered_map<sg_uid_t, Track *> & tracks, date_finder_type * df);            /* Formerly known as trw_layer_find_date_track(std::unordered_map<sg_uid_t, Track *> & tracks, date_finder_type * df). */
		static bool find_waypoint_by_date(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, date_finder_type * df);   /* Formerly known as trw_layer_find_date_waypoint(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, date_finder_type * df). */

		void draw_with_highlight(Viewport * viewport, bool highlight); /* void trw_layer_draw_with_highlight(VikTrwLayer * l, Viewport * viewport, bool highlight). */
		void draw_highlight(Viewport * viewport); /* void vik_trw_layer_draw_highlight(VikTrwLayer * vtl, Viewport * viewport). */

		void draw_highlight_item(Track * trk, Waypoint * wp, Viewport * viewport); /* vik_trw_layer_draw_highlight_item(VikTrwLayer *vtl, Track * trk, Waypoint * wp, Viewport * viewport) */
		void draw_highlight_items(std::unordered_map<sg_uid_t, Track *> * tracks, std::unordered_map<sg_uid_t, Waypoint *> * waypoints, Viewport * viewport); /* vik_trw_layer_draw_highlight_items(VikTrwLayer * vtl, std::unordered_map<sg_uid_t, Track *> * tracks, std::unordered_map<sg_uid_t, Waypoint *> * waypoints, Viewport * viewport). */


		void realize_track(std::unordered_map<sg_uid_t, Track *> & tracks, void * pass_along[4], int sublayer_id); /* static void trw_layer_realize_track(std::unordered_map<sg_uid_t, Track *> & tracks, void * pass_along[5], int sublayer_id). */
		void realize_waypoints(std::unordered_map<sg_uid_t, Waypoint *> & data, void * pass_along[4], int sublayer_id); /* static void trw_layer_realize_waypoints(std::unordered_map<sg_uid_t, Waypoint *> & data, void * pass_along[5]). */


		void add_sublayer_tracks(VikTreeview * vt, GtkTreeIter * layer_iter); /* static void trw_layer_add_sublayer_tracks ( VikTrwLayer *vtl, VikTreeview *vt, GtkTreeIter *layer_iter ) */
		void add_sublayer_waypoints(VikTreeview * vt, GtkTreeIter * layer_iter); /* static void trw_layer_add_sublayer_waypoints ( VikTrwLayer *vtl, VikTreeview *vt, GtkTreeIter *layer_iter ). */
		void add_sublayer_routes(VikTreeview * vt, GtkTreeIter * layer_iter); /* static void trw_layer_add_sublayer_routes ( VikTrwLayer *vtl, VikTreeview *vt, GtkTreeIter *layer_iter ). */



		void set_statusbar_msg_info_trkpt(Trackpoint * tp); /* static void set_statusbar_msg_info_trkpt ( VikTrwLayer *vtl, Trackpoint * tp). */



		bool delete_track(Track * trk);                                 /* Formerly known as vik_trw_layer_delete_track(VikTrwLayer * vtl, Track * trk). */
		bool delete_track_by_name(char const * name, bool is_route);    /* Formerly known as trw_layer_delete_track_by_name( VikTrwLayer *vtl, const char *name, std::unordered_map<sg_uid_t, Track *> * ht_tracks). */
		bool delete_route(Track * trk);                                 /* Formerly known as vik_trw_layer_delete_route(VikTrwLayer * vtl, Track * trk). */
		bool delete_waypoint(Waypoint * wp);                            /* Formerly known as trw_layer_delete_waypoint(VikTrwLayer * vtl, Waypoint * wp). */
		bool delete_waypoint_by_name(char const * name);                /* Formerly known as trw_layer_delete_waypoint_by_name(VikTrwLayer * vtl, char const * name). */



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

	VikCoordMode coord_mode;




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

	int highest_wp_number;

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
void vik_trw_layer_filein_add_waypoint ( VikTrwLayer *vtl, char *name, Waypoint * wp);
void vik_trw_layer_filein_add_track ( VikTrwLayer *vtl, char *name, Track * trk);

int vik_trw_layer_get_property_tracks_line_thickness ( VikTrwLayer *vtl );


// Waypoint returned is the first one
Waypoint * vik_trw_layer_get_waypoint(VikTrwLayer * vtl, const char * name);

// Track returned is the first one
Track * vik_trw_layer_get_track(VikTrwLayer * vtl, const char * name);
//bool vik_trw_layer_delete_track ( VikTrwLayer *vtl, Track * trk);
//bool vik_trw_layer_delete_route ( VikTrwLayer *vtl, Track * trk);

bool vik_trw_layer_auto_set_view ( VikTrwLayer *vtl, VikViewport *vvp );
bool vik_trw_layer_find_center ( VikTrwLayer *vtl, VikCoord *dest );

bool vik_trw_layer_new_waypoint ( VikTrwLayer *vtl, GtkWindow *w, const VikCoord *def_coord );

VikCoordMode vik_trw_layer_get_coord_mode ( VikTrwLayer *vtl );

bool vik_trw_layer_uniquify ( VikTrwLayer *vtl, VikLayersPanel *vlp );

void vik_trw_layer_delete_all_waypoints ( VikTrwLayer *vtl );
void vik_trw_layer_delete_all_tracks ( VikTrwLayer *vtl );
void vik_trw_layer_delete_all_routes ( VikTrwLayer *vtl );
void trw_layer_cancel_tps_of_track ( VikTrwLayer *vtl, Track * trk);

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
char *trw_layer_new_unique_sublayer_name ( VikTrwLayer *vtl, int sublayer_type, const char *name );
void trw_layer_waypoint_rename ( VikTrwLayer *vtl, Waypoint * wp, const char *new_name );
void trw_layer_waypoint_reset_icon ( VikTrwLayer *vtl, Waypoint * wp);
void trw_layer_calculate_bounds_waypoints ( VikTrwLayer *vtl );

void trw_layer_update_treeview ( VikTrwLayer *vtl, Track * trk);

void trw_layer_dialog_shift ( VikTrwLayer *vtl, GtkWindow *dialog, VikCoord *coord, bool vertical );

sg_uid_t trw_layer_waypoint_find_uuid(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, Waypoint * wp);

void trw_layer_zoom_to_show_latlons ( VikTrwLayer *vtl, VikViewport *vvp, struct LatLon maxmin[2] );


#define VIK_SETTINGS_LIST_DATE_FORMAT "list_date_format"

#ifdef __cplusplus
}
#endif




GList * vik_trw_layer_get_track_values(GList ** list, std::unordered_map<sg_uid_t, Track *> & tracks);




#endif
