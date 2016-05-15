/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2008, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2009, Hein Ragas <viking@ragas.nl>
 * Copyright (c) 2012-2015, Rob Norris <rw_norris@hotmail.com>
 * Copyright (c) 2012-2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
/* WARNING: If you go beyond this point, we are NOT responsible for any ill effects on your sanity */
/* viktrwlayer.c -- 8000+ lines can make a difference in the state of things */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "viking.h"
#include "vikmapslayer.h"
#include "vikgpslayer.h"
#include "viktrwlayer_export.h"
#include "viktrwlayer_wpwin.h"
#include "viktrwlayer_propwin.h"
#include "viktrwlayer_analysis.h"
#include "viktrwlayer_tracklist.h"
#include "viktrwlayer_waypointlist.h"
#ifdef VIK_CONFIG_GEOTAG
#include "viktrwlayer_geotag.h"
#include "geotag_exif.h"
#endif
#include "garminsymbols.h"
#include "thumbnails.h"
#include "background.h"
#include "gpx.h"
#include "file.h"
#include "dialog.h"
#include "geojson.h"
#include "babel.h"
#include "dem.h"
#include "dems.h"
#include "geonamessearch.h"
#ifdef VIK_CONFIG_OPENSTREETMAP
#include "osm-traces.h"
#endif
#include "acquire.h"
#include "datasources.h"
#include "datasource_gps.h"
#include "vikexttools.h"
#include "vikexttool_datasources.h"
#include "ui_util.h"
#include "vikutils.h"
#include "gpspoint.h"
#include "clipboard.h"
#include "settings.h"
#include "util.h"
#include "globals.h"

#include "vikrouting.h"

#include "icons/icons.h"

#include "layer_trw_draw.h"

#ifdef HAVE_MATH_H
#include <math.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>


#define POINTS 1
#define LINES 2

/* this is how it knows when you click if you are clicking close to a trackpoint. */
#define TRACKPOINT_SIZE_APPROX 5
#define WAYPOINT_SIZE_APPROX 5

#define MIN_STOP_LENGTH 15
#define MAX_STOP_LENGTH 86400

using namespace SlavGPS;









typedef enum {
  MA_VTL = 0,
  MA_VLP,
  MA_SUBTYPE, // OR END for Layer only
  MA_SUBLAYER_ID,
  MA_CONFIRM,
  MA_VVP,
  MA_TV_ITER,
  MA_MISC,
  MA_LAST,
} menu_array_index;

typedef void * menu_array_layer[2];
typedef void * menu_array_sublayer[MA_LAST];

static void trw_layer_delete_item ( menu_array_sublayer values );
static void trw_layer_copy_item_cb ( menu_array_sublayer values );
static void trw_layer_cut_item_cb ( menu_array_sublayer values );

//static void trw_layer_find_maxmin_tracks(const Track * trk, struct LatLon maxmin[2] );
//static void trw_layer_find_maxmin_tracks_2(std::unordered_map<sg_uid_t, Track *> & tracks, struct LatLon maxmin[2]);
static void trw_layer_find_maxmin (VikTrwLayer *vtl, struct LatLon maxmin[2]);

static void trw_layer_new_track_gcs ( VikTrwLayer *vtl, VikViewport *vp );
static void trw_layer_free_track_gcs ( VikTrwLayer *vtl );


static void goto_coord ( void ** vlp, void * vvp, void * vl, const VikCoord *coord );
static void trw_layer_goto_track_startpoint ( menu_array_sublayer values );
static void trw_layer_goto_track_endpoint ( menu_array_sublayer values );
static void trw_layer_goto_track_max_speed ( menu_array_sublayer values );
static void trw_layer_goto_track_max_alt ( menu_array_sublayer values );
static void trw_layer_goto_track_min_alt ( menu_array_sublayer values );
static void trw_layer_goto_track_center ( menu_array_sublayer values );
static void trw_layer_merge_by_segment ( menu_array_sublayer values );
static void trw_layer_merge_by_timestamp ( menu_array_sublayer values );
static void trw_layer_merge_with_other ( menu_array_sublayer values );
static void trw_layer_append_track ( menu_array_sublayer values );
static void trw_layer_split_by_timestamp ( menu_array_sublayer values );
static void trw_layer_split_by_n_points ( menu_array_sublayer values );
static void trw_layer_split_at_trackpoint ( menu_array_sublayer values );
static void trw_layer_split_segments ( menu_array_sublayer values );
static void trw_layer_delete_point_selected ( menu_array_sublayer values );
static void trw_layer_delete_points_same_position ( menu_array_sublayer values );
static void trw_layer_delete_points_same_time ( menu_array_sublayer values );
static void trw_layer_reverse ( menu_array_sublayer values );
static void trw_layer_download_map_along_track_cb ( menu_array_sublayer values );
static void trw_layer_edit_trackpoint ( menu_array_sublayer values );
static void trw_layer_show_picture ( menu_array_sublayer values );
static void trw_layer_gps_upload_any ( menu_array_sublayer values );

static void trw_layer_centerize ( menu_array_layer values );
static void trw_layer_auto_view ( menu_array_layer values );
static void trw_layer_goto_wp ( menu_array_layer values );
static void trw_layer_new_wp ( menu_array_layer values );
static void trw_layer_new_track ( menu_array_layer values );
static void trw_layer_new_route ( menu_array_layer values );
static void trw_layer_finish_track ( menu_array_layer values );
static void trw_layer_auto_waypoints_view ( menu_array_layer values );
static void trw_layer_auto_tracks_view ( menu_array_layer values );
static void trw_layer_delete_all_tracks ( menu_array_layer values );
static void trw_layer_delete_tracks_from_selection ( menu_array_layer values );
static void trw_layer_delete_all_waypoints ( menu_array_layer values );
static void trw_layer_delete_waypoints_from_selection ( menu_array_layer values );
static void trw_layer_new_wikipedia_wp_viewport ( menu_array_layer values );
static void trw_layer_new_wikipedia_wp_layer ( menu_array_layer values );
#ifdef VIK_CONFIG_GEOTAG
static void trw_layer_geotagging_waypoint_mtime_keep ( menu_array_sublayer values );
static void trw_layer_geotagging_waypoint_mtime_update ( menu_array_sublayer values );
static void trw_layer_geotagging_track ( menu_array_sublayer values );
static void trw_layer_geotagging ( menu_array_layer values );
#endif
static void trw_layer_acquire_gps_cb ( menu_array_layer values );
static void trw_layer_acquire_routing_cb ( menu_array_layer values );
static void trw_layer_acquire_url_cb ( menu_array_layer values );
#ifdef VIK_CONFIG_OPENSTREETMAP
static void trw_layer_acquire_osm_cb ( menu_array_layer values );
static void trw_layer_acquire_osm_my_traces_cb ( menu_array_layer values );
#endif
#ifdef VIK_CONFIG_GEOCACHES
static void trw_layer_acquire_geocache_cb ( menu_array_layer values );
#endif
#ifdef VIK_CONFIG_GEOTAG
static void trw_layer_acquire_geotagged_cb ( menu_array_layer values );
#endif
static void trw_layer_acquire_file_cb ( menu_array_layer values );
static void trw_layer_gps_upload ( menu_array_layer values );

static void trw_layer_track_list_dialog_single ( menu_array_sublayer values );
static void trw_layer_track_list_dialog ( menu_array_layer values );
static void trw_layer_waypoint_list_dialog ( menu_array_layer values );

// Specific route versions:
//  Most track handling functions can handle operating on the route list
//  However these ones are easier in separate functions
static void trw_layer_auto_routes_view ( menu_array_layer values );
static void trw_layer_delete_all_routes ( menu_array_layer values );
static void trw_layer_delete_routes_from_selection ( menu_array_layer values );

/* pop-up items */
static void trw_layer_properties_item ( void * pass_along[7] ); //TODO??
static void trw_layer_goto_waypoint ( menu_array_sublayer values );
static void trw_layer_waypoint_gc_webpage ( menu_array_sublayer values );
static void trw_layer_waypoint_webpage ( menu_array_sublayer values );




//static void trw_layer_insert_tp_beside_current_tp ( VikTrwLayer *vtl, bool );
static void trw_layer_cancel_current_tp ( VikTrwLayer *vtl, bool destroy );
static void trw_layer_tpwin_response ( VikTrwLayer *vtl, int response );
//static void trw_layer_tpwin_init ( VikTrwLayer *vtl );

//static void trw_layer_sort_all ( VikTrwLayer *vtl );

static void * tool_edit_trackpoint_create ( VikWindow *vw, Viewport * viewport);
static void tool_edit_trackpoint_destroy ( tool_ed_t *t );
static bool tool_edit_trackpoint_click ( VikTrwLayer *vtl, GdkEventButton *event, void * data );
static bool tool_edit_trackpoint_move ( VikTrwLayer *vtl, GdkEventMotion *event, void * data );
static bool tool_edit_trackpoint_release ( VikTrwLayer *vtl, GdkEventButton *event, void * data );
static void * tool_show_picture_create ( VikWindow *vw, Viewport * viewport);
static bool tool_show_picture_click ( VikTrwLayer *vtl, GdkEventButton *event, Viewport * viewport);
static void * tool_edit_waypoint_create ( VikWindow *vw, Viewport * viewport);
static void tool_edit_waypoint_destroy ( tool_ed_t *t );
static bool tool_edit_waypoint_click ( VikTrwLayer *vtl, GdkEventButton *event, void * data );
static bool tool_edit_waypoint_move ( VikTrwLayer *vtl, GdkEventMotion *event, void * data );
static bool tool_edit_waypoint_release ( VikTrwLayer *vtl, GdkEventButton *event, void * data );
static void * tool_new_route_create ( VikWindow *vw, Viewport * viewport);
static bool tool_new_route_click ( VikTrwLayer *vtl, GdkEventButton *event, Viewport * viewport);
static void * tool_new_track_create ( VikWindow *vw, Viewport * viewport);
static bool tool_new_track_click ( VikTrwLayer *vtl, GdkEventButton *event, Viewport * viewport);
static VikLayerToolFuncStatus tool_new_track_move ( VikTrwLayer *vtl, GdkEventMotion *event, Viewport * viewport);
static void tool_new_track_release ( VikTrwLayer *vtl, GdkEventButton *event, Viewport * viewport);
static bool tool_new_track_key_press ( VikTrwLayer *vtl, GdkEventKey *event, Viewport * viewport);
static void * tool_new_waypoint_create ( VikWindow *vw, Viewport * viewport);
static bool tool_new_waypoint_click ( VikTrwLayer *vtl, GdkEventButton *event, Viewport * viewport);
static void * tool_extended_route_finder_create ( VikWindow *vw, Viewport * viewport);
static bool tool_extended_route_finder_click ( VikTrwLayer *vtl, GdkEventButton *event, Viewport * viewport);
static bool tool_extended_route_finder_key_press ( VikTrwLayer *vtl, GdkEventKey *event, Viewport * viewport);


//static Trackpoint * closest_tp_in_five_pixel_interval(VikTrwLayer * vtl, Viewport * viewport, int x, int y);
//static Waypoint * closest_wp_in_five_pixel_interval(VikTrwLayer * vtl, Viewport * viewport, int x, int y);

//static void waypoints_convert(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, VikCoordMode * dest_mode);
static void waypoint_convert(Waypoint * wp, VikCoordMode * dest_mode);
//static void track_convert(std::unordered_map<sg_uid_t, Track *> & tracks, VikCoordMode * dest_mode);

//static char *highest_wp_number_get(VikTrwLayer *vtl);
//static void highest_wp_number_reset(VikTrwLayer *vtl);
//static void highest_wp_number_add_wp(VikTrwLayer *vtl, const char *new_wp_name);
//static void highest_wp_number_remove_wp(VikTrwLayer *vtl, const char *old_wp_name);

static Track * trw_layer_get_track_helper(menu_array_sublayer values, VikTrwLayer * vtl);

// Note for the following tool GtkRadioActionEntry texts:
//  the very first text value is an internal name not displayed anywhere
//  the first N_ text value is the name used for menu entries - hence has an underscore for the keyboard accelerator
//    * remember not to clash with the values used for VikWindow level tools (Pan, Zoom, Ruler + Select)
//  the second N_ text value is used for the button tooltip (i.e. generally don't want an underscore here)
//  the value is always set to 0 and the tool loader in VikWindow will set the actual appropriate value used
static VikToolInterface trw_layer_tools[] = {
  { { "CreateWaypoint", "vik-icon-Create Waypoint", N_("Create _Waypoint"), "<control><shift>W", N_("Create Waypoint"), 0 },
    (VikToolConstructorFunc) tool_new_waypoint_create,    NULL, NULL, NULL,
    (VikToolMouseFunc) tool_new_waypoint_click,
    NULL,
    NULL,
    (VikToolKeyFunc) NULL,
    false,
    GDK_CURSOR_IS_PIXMAP, &cursor_addwp_pixbuf, NULL },

  { { "CreateTrack", "vik-icon-Create Track", N_("Create _Track"), "<control><shift>T", N_("Create Track"), 0 },
    (VikToolConstructorFunc) tool_new_track_create,       NULL, NULL, NULL,
    (VikToolMouseFunc) tool_new_track_click,
    (VikToolMouseMoveFunc) tool_new_track_move,
    (VikToolMouseFunc) tool_new_track_release,
    (VikToolKeyFunc) tool_new_track_key_press,
    true, // Still need to handle clicks when in PAN mode to disable the potential trackpoint drawing
    GDK_CURSOR_IS_PIXMAP, &cursor_addtr_pixbuf, NULL },

  { { "CreateRoute", "vik-icon-Create Route", N_("Create _Route"), "<control><shift>B", N_("Create Route"), 0 },
    (VikToolConstructorFunc) tool_new_route_create,       NULL, NULL, NULL,
    (VikToolMouseFunc) tool_new_route_click,
    (VikToolMouseMoveFunc) tool_new_track_move, // -\#
    (VikToolMouseFunc) tool_new_track_release,  //   -> Reuse these track methods on a route
    (VikToolKeyFunc) tool_new_track_key_press,  // -/#
    true, // Still need to handle clicks when in PAN mode to disable the potential trackpoint drawing
    GDK_CURSOR_IS_PIXMAP, &cursor_new_route_pixbuf, NULL },

  { { "ExtendedRouteFinder", "vik-icon-Route Finder", N_("Route _Finder"), "<control><shift>F", N_("Route Finder"), 0 },
    (VikToolConstructorFunc) tool_extended_route_finder_create,  NULL, NULL, NULL,
    (VikToolMouseFunc) tool_extended_route_finder_click,
    (VikToolMouseMoveFunc) tool_new_track_move, // -\#
    (VikToolMouseFunc) tool_new_track_release,  //   -> Reuse these track methods on a route
    (VikToolKeyFunc) tool_extended_route_finder_key_press,
    true, // Still need to handle clicks when in PAN mode to disable the potential trackpoint drawing
    GDK_CURSOR_IS_PIXMAP, &cursor_route_finder_pixbuf, NULL },

  { { "EditWaypoint", "vik-icon-Edit Waypoint", N_("_Edit Waypoint"), "<control><shift>E", N_("Edit Waypoint"), 0 },
    (VikToolConstructorFunc) tool_edit_waypoint_create,
    (VikToolDestructorFunc) tool_edit_waypoint_destroy,
    NULL, NULL,
    (VikToolMouseFunc) tool_edit_waypoint_click,
    (VikToolMouseMoveFunc) tool_edit_waypoint_move,
    (VikToolMouseFunc) tool_edit_waypoint_release,
    (VikToolKeyFunc) NULL,
    false,
    GDK_CURSOR_IS_PIXMAP, &cursor_edwp_pixbuf, NULL },

  { { "EditTrackpoint", "vik-icon-Edit Trackpoint", N_("Edit Trac_kpoint"), "<control><shift>K", N_("Edit Trackpoint"), 0 },
    (VikToolConstructorFunc) tool_edit_trackpoint_create,
    (VikToolDestructorFunc) tool_edit_trackpoint_destroy,
    NULL, NULL,
    (VikToolMouseFunc) tool_edit_trackpoint_click,
    (VikToolMouseMoveFunc) tool_edit_trackpoint_move,
    (VikToolMouseFunc) tool_edit_trackpoint_release,
    (VikToolKeyFunc) NULL,
    false,
    GDK_CURSOR_IS_PIXMAP, &cursor_edtr_pixbuf, NULL },

  { { "ShowPicture", "vik-icon-Show Picture", N_("Show P_icture"), "<control><shift>I", N_("Show Picture"), 0 },
    (VikToolConstructorFunc) tool_show_picture_create,    NULL, NULL, NULL,
    (VikToolMouseFunc) tool_show_picture_click,
    NULL, NULL, (VikToolKeyFunc) NULL,
    false,
    GDK_CURSOR_IS_PIXMAP, &cursor_showpic_pixbuf, NULL },

};

enum {
  TOOL_CREATE_WAYPOINT=0,
  TOOL_CREATE_TRACK,
  TOOL_CREATE_ROUTE,
  TOOL_ROUTE_FINDER,
  TOOL_EDIT_WAYPOINT,
  TOOL_EDIT_TRACKPOINT,
  TOOL_SHOW_PICTURE,
  NUM_TOOLS
};

/****** PARAMETERS ******/

static char *params_groups[] = { (char *) N_("Waypoints"), (char *) N_("Tracks"), (char *) N_("Waypoint Images"), (char *) N_("Tracks Advanced"), (char *) N_("Metadata") };
enum { GROUP_WAYPOINTS, GROUP_TRACKS, GROUP_IMAGES, GROUP_TRACKS_ADV, GROUP_METADATA };

static char *params_drawmodes[] = { (char *) N_("Draw by Track"), (char *) N_("Draw by Speed"), (char *) N_("All Tracks Same Color"), NULL };
static char *params_wpsymbols[] = { (char *) N_("Filled Square"), (char *) N_("Square"), (char *) N_("Circle"), (char *) N_("X"), 0 };

#define MIN_POINT_SIZE 2
#define MAX_POINT_SIZE 10

#define MIN_ARROW_SIZE 3
#define MAX_ARROW_SIZE 20

static VikLayerParamScale params_scales[] = {
 /* min  max    step digits */
 {  1,   10,    1,   0 }, /* line_thickness */
 {  0,   100,   1,   0 }, /* track draw speed factor */
 {  1.0, 100.0, 1.0, 2 }, /* UNUSED */
                /* 5 * step == how much to turn */
 {  16,   128,  4,   0 }, // 3: image_size - NB step size ignored when an HSCALE used
 {   0,   255,  5,   0 }, // 4: image alpha -    "     "      "            "
 {   5,   500,  5,   0 }, // 5: image cache_size -     "      "
 {   0,   8,    1,   0 }, // 6: Background line thickness
 {   1,  64,    1,   0 }, /* wpsize */
 {   MIN_STOP_LENGTH, MAX_STOP_LENGTH, 1,   0 }, /* stop_length */
 {   1, 100, 1,   0 }, // 9: elevation factor
 {   MIN_POINT_SIZE,  MAX_POINT_SIZE,  1,   0 }, // 10: track point size
 {   MIN_ARROW_SIZE,  MAX_ARROW_SIZE,  1,   0 }, // 11: direction arrow size
};

static char* params_font_sizes[] = {
	(char *) N_("Extra Extra Small"),
	(char *) N_("Extra Small"),
	(char *) N_("Small"),
	(char *) N_("Medium"),
	(char *) N_("Large"),
	(char *) N_("Extra Large"),
	(char *) N_("Extra Extra Large"),
	NULL };

// Needs to align with vik_layer_sort_order_t
static char* params_sort_order[] = {
	(char *) N_("None"),
	(char *) N_("Name Ascending"),
	(char *) N_("Name Descending"),
	(char *) N_("Date Ascending"),
	(char *) N_("Date Descending"),
	NULL
};

static VikLayerParamData black_color_default ( void ) {
  VikLayerParamData data; gdk_color_parse ( "#000000", &data.c ); return data; // Black
}
static VikLayerParamData drawmode_default ( void ) { return VIK_LPD_UINT ( DRAWMODE_BY_TRACK ); }
static VikLayerParamData line_thickness_default ( void ) { return VIK_LPD_UINT ( 1 ); }
static VikLayerParamData trkpointsize_default ( void ) { return VIK_LPD_UINT ( MIN_POINT_SIZE ); }
static VikLayerParamData trkdirectionsize_default ( void ) { return VIK_LPD_UINT ( 5 ); }
static VikLayerParamData bg_line_thickness_default ( void ) { return VIK_LPD_UINT ( 0 ); }
static VikLayerParamData trackbgcolor_default ( void ) {
  VikLayerParamData data; gdk_color_parse ( "#FFFFFF", &data.c ); return data; // White
}
static VikLayerParamData elevation_factor_default ( void ) { return VIK_LPD_UINT ( 30 ); }
static VikLayerParamData stop_length_default ( void ) { return VIK_LPD_UINT ( 60 ); }
static VikLayerParamData speed_factor_default ( void ) { return VIK_LPD_DOUBLE ( 30.0 ); }

static VikLayerParamData tnfontsize_default ( void ) { return VIK_LPD_UINT ( FS_MEDIUM ); }
static VikLayerParamData wpfontsize_default ( void ) { return VIK_LPD_UINT ( FS_MEDIUM ); }
static VikLayerParamData wptextcolor_default ( void ) {
  VikLayerParamData data; gdk_color_parse ( "#FFFFFF", &data.c ); return data; // White
}
static VikLayerParamData wpbgcolor_default ( void ) {
  VikLayerParamData data; gdk_color_parse ( "#8383C4", &data.c ); return data; // Kind of Blue
}
static VikLayerParamData wpsize_default ( void ) { return VIK_LPD_UINT ( 4 ); }
static VikLayerParamData wpsymbol_default ( void ) { return VIK_LPD_UINT ( WP_SYMBOL_FILLED_SQUARE ); }

static VikLayerParamData image_size_default ( void ) { return VIK_LPD_UINT ( 64 ); }
static VikLayerParamData image_alpha_default ( void ) { return VIK_LPD_UINT ( 255 ); }
static VikLayerParamData image_cache_size_default ( void ) { return VIK_LPD_UINT ( 300 ); }

static VikLayerParamData sort_order_default ( void ) { return VIK_LPD_UINT ( 0 ); }

static VikLayerParamData string_default ( void )
{
  VikLayerParamData data;
  data.s = "";
  return data;
}

VikLayerParam trw_layer_params[] = {
  { VIK_LAYER_TRW, "tracks_visible", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_NOT_IN_PROPERTIES, NULL, (VikLayerWidgetType) 0, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_TRW, "waypoints_visible", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_NOT_IN_PROPERTIES, NULL, (VikLayerWidgetType) 0, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_TRW, "routes_visible", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_NOT_IN_PROPERTIES, NULL, (VikLayerWidgetType) 0, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },

  { VIK_LAYER_TRW, "trackdrawlabels", VIK_LAYER_PARAM_BOOLEAN, GROUP_TRACKS, N_("Draw Labels"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL,
    N_("Note: the individual track controls what labels may be displayed"), vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_TRW, "trackfontsize", VIK_LAYER_PARAM_UINT, GROUP_TRACKS_ADV, N_("Track Labels Font Size:"), VIK_LAYER_WIDGET_COMBOBOX, params_font_sizes, NULL, NULL, tnfontsize_default, NULL, NULL },
  { VIK_LAYER_TRW, "drawmode", VIK_LAYER_PARAM_UINT, GROUP_TRACKS, N_("Track Drawing Mode:"), VIK_LAYER_WIDGET_COMBOBOX, params_drawmodes, NULL, NULL, drawmode_default, NULL, NULL },
  { VIK_LAYER_TRW, "trackcolor", VIK_LAYER_PARAM_COLOR, GROUP_TRACKS, N_("All Tracks Color:"), VIK_LAYER_WIDGET_COLOR, NULL, NULL,
    N_("The color used when 'All Tracks Same Color' drawing mode is selected"), black_color_default, NULL, NULL },
  { VIK_LAYER_TRW, "drawlines", VIK_LAYER_PARAM_BOOLEAN, GROUP_TRACKS, N_("Draw Track Lines"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_TRW, "line_thickness", VIK_LAYER_PARAM_UINT, GROUP_TRACKS_ADV, N_("Track Thickness:"), VIK_LAYER_WIDGET_SPINBUTTON, &params_scales[0], NULL, NULL, line_thickness_default, NULL, NULL },
  { VIK_LAYER_TRW, "drawdirections", VIK_LAYER_PARAM_BOOLEAN, GROUP_TRACKS, N_("Draw Track Direction"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_false_default, NULL, NULL },
  { VIK_LAYER_TRW, "trkdirectionsize", VIK_LAYER_PARAM_UINT, GROUP_TRACKS_ADV, N_("Direction Size:"), VIK_LAYER_WIDGET_SPINBUTTON, &params_scales[11], NULL, NULL, trkdirectionsize_default, NULL, NULL },
  { VIK_LAYER_TRW, "drawpoints", VIK_LAYER_PARAM_BOOLEAN, GROUP_TRACKS, N_("Draw Trackpoints"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_TRW, "trkpointsize", VIK_LAYER_PARAM_UINT, GROUP_TRACKS_ADV, N_("Trackpoint Size:"), VIK_LAYER_WIDGET_SPINBUTTON, &params_scales[10], NULL, NULL, trkpointsize_default, NULL, NULL },
  { VIK_LAYER_TRW, "drawelevation", VIK_LAYER_PARAM_BOOLEAN, GROUP_TRACKS, N_("Draw Elevation"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_false_default, NULL, NULL },
  { VIK_LAYER_TRW, "elevation_factor", VIK_LAYER_PARAM_UINT, GROUP_TRACKS_ADV, N_("Draw Elevation Height %:"), VIK_LAYER_WIDGET_HSCALE, &params_scales[9], NULL, NULL, elevation_factor_default, NULL, NULL },
  { VIK_LAYER_TRW, "drawstops", VIK_LAYER_PARAM_BOOLEAN, GROUP_TRACKS, N_("Draw Stops"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL,
    N_("Whether to draw a marker when trackpoints are at the same position but over the minimum stop length apart in time"), vik_lpd_false_default, NULL, NULL },
  { VIK_LAYER_TRW, "stop_length", VIK_LAYER_PARAM_UINT, GROUP_TRACKS_ADV, N_("Min Stop Length (seconds):"), VIK_LAYER_WIDGET_SPINBUTTON, &params_scales[8], NULL, NULL, stop_length_default, NULL, NULL },

  { VIK_LAYER_TRW, "bg_line_thickness", VIK_LAYER_PARAM_UINT, GROUP_TRACKS_ADV, N_("Track BG Thickness:"), VIK_LAYER_WIDGET_SPINBUTTON, &params_scales[6], NULL, NULL, bg_line_thickness_default, NULL, NULL },
  { VIK_LAYER_TRW, "trackbgcolor", VIK_LAYER_PARAM_COLOR, GROUP_TRACKS_ADV, N_("Track Background Color"), VIK_LAYER_WIDGET_COLOR, NULL, NULL, NULL, trackbgcolor_default, NULL, NULL },
  { VIK_LAYER_TRW, "speed_factor", VIK_LAYER_PARAM_DOUBLE, GROUP_TRACKS_ADV, N_("Draw by Speed Factor (%):"), VIK_LAYER_WIDGET_HSCALE, &params_scales[1], NULL,
    N_("The percentage factor away from the average speed determining the color used"), speed_factor_default, NULL, NULL },
  { VIK_LAYER_TRW, "tracksortorder", VIK_LAYER_PARAM_UINT, GROUP_TRACKS_ADV, N_("Track Sort Order:"), VIK_LAYER_WIDGET_COMBOBOX, params_sort_order, NULL, NULL, sort_order_default, NULL, NULL },

  { VIK_LAYER_TRW, "drawlabels", VIK_LAYER_PARAM_BOOLEAN, GROUP_WAYPOINTS, N_("Draw Labels"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_TRW, "wpfontsize", VIK_LAYER_PARAM_UINT, GROUP_WAYPOINTS, N_("Waypoint Font Size:"), VIK_LAYER_WIDGET_COMBOBOX, params_font_sizes, NULL, NULL, wpfontsize_default, NULL, NULL },
  { VIK_LAYER_TRW, "wpcolor", VIK_LAYER_PARAM_COLOR, GROUP_WAYPOINTS, N_("Waypoint Color:"), VIK_LAYER_WIDGET_COLOR, NULL, NULL, NULL, black_color_default, NULL, NULL },
  { VIK_LAYER_TRW, "wptextcolor", VIK_LAYER_PARAM_COLOR, GROUP_WAYPOINTS, N_("Waypoint Text:"), VIK_LAYER_WIDGET_COLOR, NULL, NULL, NULL, wptextcolor_default, NULL, NULL },
  { VIK_LAYER_TRW, "wpbgcolor", VIK_LAYER_PARAM_COLOR, GROUP_WAYPOINTS, N_("Background:"), VIK_LAYER_WIDGET_COLOR, NULL, NULL, NULL, wpbgcolor_default, NULL, NULL },
  { VIK_LAYER_TRW, "wpbgand", VIK_LAYER_PARAM_BOOLEAN, GROUP_WAYPOINTS, N_("Fake BG Color Translucency:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_false_default, NULL, NULL },
  { VIK_LAYER_TRW, "wpsymbol", VIK_LAYER_PARAM_UINT, GROUP_WAYPOINTS, N_("Waypoint marker:"), VIK_LAYER_WIDGET_COMBOBOX, params_wpsymbols, NULL, NULL, wpsymbol_default, NULL, NULL },
  { VIK_LAYER_TRW, "wpsize", VIK_LAYER_PARAM_UINT, GROUP_WAYPOINTS, N_("Waypoint size:"), VIK_LAYER_WIDGET_SPINBUTTON, &params_scales[7], NULL, NULL, wpsize_default, NULL, NULL },
  { VIK_LAYER_TRW, "wpsyms", VIK_LAYER_PARAM_BOOLEAN, GROUP_WAYPOINTS, N_("Draw Waypoint Symbols:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_TRW, "wpsortorder", VIK_LAYER_PARAM_UINT, GROUP_WAYPOINTS, N_("Waypoint Sort Order:"), VIK_LAYER_WIDGET_COMBOBOX, params_sort_order, NULL, NULL, sort_order_default, NULL, NULL },

  { VIK_LAYER_TRW, "drawimages", VIK_LAYER_PARAM_BOOLEAN, GROUP_IMAGES, N_("Draw Waypoint Images"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_TRW, "image_size", VIK_LAYER_PARAM_UINT, GROUP_IMAGES, N_("Image Size (pixels):"), VIK_LAYER_WIDGET_HSCALE, &params_scales[3], NULL, NULL, image_size_default, NULL, NULL },
  { VIK_LAYER_TRW, "image_alpha", VIK_LAYER_PARAM_UINT, GROUP_IMAGES, N_("Image Alpha:"), VIK_LAYER_WIDGET_HSCALE, &params_scales[4], NULL, NULL, image_alpha_default, NULL, NULL },
  { VIK_LAYER_TRW, "image_cache_size", VIK_LAYER_PARAM_UINT, GROUP_IMAGES, N_("Image Memory Cache Size:"), VIK_LAYER_WIDGET_HSCALE, &params_scales[5], NULL, NULL, image_cache_size_default, NULL, NULL },

  { VIK_LAYER_TRW, "metadatadesc", VIK_LAYER_PARAM_STRING, GROUP_METADATA, N_("Description"), VIK_LAYER_WIDGET_ENTRY, NULL, NULL, NULL, string_default, NULL, NULL },
  { VIK_LAYER_TRW, "metadataauthor", VIK_LAYER_PARAM_STRING, GROUP_METADATA, N_("Author"), VIK_LAYER_WIDGET_ENTRY, NULL, NULL, NULL, string_default, NULL, NULL },
  { VIK_LAYER_TRW, "metadatatime", VIK_LAYER_PARAM_STRING, GROUP_METADATA, N_("Creation Time"), VIK_LAYER_WIDGET_ENTRY, NULL, NULL, NULL, string_default, NULL, NULL },
  { VIK_LAYER_TRW, "metadatakeywords", VIK_LAYER_PARAM_STRING, GROUP_METADATA, N_("Keywords"), VIK_LAYER_WIDGET_ENTRY, NULL, NULL, NULL, string_default, NULL, NULL },
};

// ENUMERATION MUST BE IN THE SAME ORDER AS THE NAMED PARAMS ABOVE
enum {
  // Sublayer visibilities
  PARAM_TV,
  PARAM_WV,
  PARAM_RV,
  // Tracks
  PARAM_TDL,
  PARAM_TLFONTSIZE,
  PARAM_DM,
  PARAM_TC,
  PARAM_DL,
  PARAM_LT,
  PARAM_DD,
  PARAM_DDS,
  PARAM_DP,
  PARAM_DPS,
  PARAM_DE,
  PARAM_EF,
  PARAM_DS,
  PARAM_SL,
  PARAM_BLT,
  PARAM_TBGC,
  PARAM_TDSF,
  PARAM_TSO,
  // Waypoints
  PARAM_DLA,
  PARAM_WPFONTSIZE,
  PARAM_WPC,
  PARAM_WPTC,
  PARAM_WPBC,
  PARAM_WPBA,
  PARAM_WPSYM,
  PARAM_WPSIZE,
  PARAM_WPSYMS,
  PARAM_WPSO,
  // WP images
  PARAM_DI,
  PARAM_IS,
  PARAM_IA,
  PARAM_ICS,
  // Metadata
  PARAM_MDDESC,
  PARAM_MDAUTH,
  PARAM_MDTIME,
  PARAM_MDKEYS,
  NUM_PARAMS
};

/*** TO ADD A PARAM:
 *** 1) Add to trw_layer_params and enumeration
 *** 2) Handle in get_param & set_param (presumably adding on to VikTrwLayer struct)
 ***/

/****** END PARAMETERS ******/

/* Layer Interface function definitions */
static VikTrwLayer* trw_layer_create ( VikViewport *vp );
static void trw_layer_realize ( VikTrwLayer *vtl, VikTreeview *vt, GtkTreeIter *layer_iter );
static void trw_layer_post_read ( VikTrwLayer *vtl, GtkWidget *vvp, bool from_file );
static void trw_layer_free ( VikTrwLayer *trwlayer );
static void trw_layer_draw ( VikTrwLayer *l, void * data );
static void trw_layer_change_coord_mode ( VikTrwLayer *vtl, VikCoordMode dest_mode );
static time_t trw_layer_get_timestamp ( VikTrwLayer *vtl );
static void trw_layer_set_menu_selection ( VikTrwLayer *vtl, uint16_t );
static uint16_t trw_layer_get_menu_selection ( VikTrwLayer *vtl );
static void trw_layer_add_menu_items ( VikTrwLayer *vtl, GtkMenu *menu, void * vlp );
static bool trw_layer_sublayer_add_menu_items ( VikTrwLayer *l, GtkMenu *menu, void * vlp, int subtype, void * sublayer, GtkTreeIter *iter, VikViewport *vvp );
static const char* trw_layer_sublayer_rename_request ( VikTrwLayer *l, const char *newname, void * vlp, int subtype, void * sublayer, GtkTreeIter *iter );
static bool trw_layer_sublayer_toggle_visible ( VikTrwLayer *l, int subtype, void * sublayer );
static const char* trw_layer_layer_tooltip ( VikTrwLayer *vtl );
static const char* trw_layer_sublayer_tooltip ( VikTrwLayer *l, int subtype, void * sublayer );
static bool trw_layer_selected ( VikTrwLayer *l, int subtype, void * sublayer, int type, void * vlp );
static void trw_layer_marshall ( VikTrwLayer *vtl, uint8_t **data, int *len );
static VikTrwLayer *trw_layer_unmarshall ( uint8_t *data, int len, VikViewport *vvp );
static bool trw_layer_set_param ( VikTrwLayer *vtl, uint16_t id, VikLayerParamData data, VikViewport *vp, bool is_file_operation );
static VikLayerParamData trw_layer_get_param ( VikTrwLayer *vtl, uint16_t id, bool is_file_operation );
static void trw_layer_change_param ( GtkWidget *widget, ui_change_values values );
static void trw_layer_del_item ( VikTrwLayer *vtl, int subtype, void * sublayer );
static void trw_layer_cut_item ( VikTrwLayer *vtl, int subtype, void * sublayer );
static void trw_layer_copy_item ( VikTrwLayer *vtl, int subtype, void * sublayer, uint8_t **item, unsigned int *len );
static bool trw_layer_paste_item ( VikTrwLayer *vtl, int subtype, uint8_t *item, size_t len );
static void trw_layer_free_copied_item ( int subtype, void * item );
static void trw_layer_drag_drop_request ( VikTrwLayer *vtl_src, VikTrwLayer *vtl_dest, GtkTreeIter *src_item_iter, GtkTreePath *dest_path );
static bool trw_layer_select_click ( VikTrwLayer *vtl, GdkEventButton *event, Viewport * viewport, tool_ed_t *t );
static bool trw_layer_select_move ( VikTrwLayer *vtl, GdkEventMotion *event, Viewport * viewport, tool_ed_t *t );
static bool trw_layer_select_release ( VikTrwLayer *vtl, GdkEventButton *event, Viewport * viewport, tool_ed_t *t );
static bool trw_layer_show_selected_viewport_menu ( VikTrwLayer *vtl, GdkEventButton *event, Viewport * viewport );
/* End Layer Interface function definitions */

VikLayerInterface vik_trw_layer_interface = {
  "TrackWaypoint",
  N_("TrackWaypoint"),
  "<control><shift>Y",
  &viktrwlayer_pixbuf,

  trw_layer_tools,
  sizeof(trw_layer_tools) / sizeof(VikToolInterface),

  trw_layer_params,
  NUM_PARAMS,
  params_groups, /* params_groups */
  sizeof(params_groups)/sizeof(params_groups[0]),    /* number of groups */

  VIK_MENU_ITEM_ALL,

  (VikLayerFuncCreate)                  trw_layer_create,
  (VikLayerFuncRealize)                 trw_layer_realize,
  (VikLayerFuncPostRead)                trw_layer_post_read,
  (VikLayerFuncFree)                    trw_layer_free,

  (VikLayerFuncProperties)              NULL,
  (VikLayerFuncDraw)                    trw_layer_draw,
  (VikLayerFuncChangeCoordMode)         trw_layer_change_coord_mode,
  (VikLayerFuncGetTimestamp)            trw_layer_get_timestamp,

  (VikLayerFuncSetMenuItemsSelection)   trw_layer_set_menu_selection,
  (VikLayerFuncGetMenuItemsSelection)   trw_layer_get_menu_selection,

  (VikLayerFuncAddMenuItems)            trw_layer_add_menu_items,
  (VikLayerFuncSublayerAddMenuItems)    trw_layer_sublayer_add_menu_items,

  (VikLayerFuncSublayerRenameRequest)   trw_layer_sublayer_rename_request,
  (VikLayerFuncSublayerToggleVisible)   trw_layer_sublayer_toggle_visible,
  (VikLayerFuncSublayerTooltip)         trw_layer_sublayer_tooltip,
  (VikLayerFuncLayerTooltip)            trw_layer_layer_tooltip,
  (VikLayerFuncLayerSelected)           trw_layer_selected,

  (VikLayerFuncMarshall)                trw_layer_marshall,
  (VikLayerFuncUnmarshall)              trw_layer_unmarshall,

  (VikLayerFuncSetParam)                trw_layer_set_param,
  (VikLayerFuncGetParam)                trw_layer_get_param,
  (VikLayerFuncChangeParam)             trw_layer_change_param,

  (VikLayerFuncReadFileData)            a_gpspoint_read_file,
  (VikLayerFuncWriteFileData)           a_gpspoint_write_file,

  (VikLayerFuncDeleteItem)              trw_layer_del_item,
  (VikLayerFuncCutItem)                 trw_layer_cut_item,
  (VikLayerFuncCopyItem)                trw_layer_copy_item,
  (VikLayerFuncPasteItem)               trw_layer_paste_item,
  (VikLayerFuncFreeCopiedItem)          trw_layer_free_copied_item,

  (VikLayerFuncDragDropRequest)         trw_layer_drag_drop_request,

  (VikLayerFuncSelectClick)             trw_layer_select_click,
  (VikLayerFuncSelectMove)              trw_layer_select_move,
  (VikLayerFuncSelectRelease)           trw_layer_select_release,
  (VikLayerFuncSelectedViewportMenu)    trw_layer_show_selected_viewport_menu,
};




static bool have_diary_program = false;
static char *diary_program = NULL;
#define VIK_SETTINGS_EXTERNAL_DIARY_PROGRAM "external_diary_program"

static bool have_geojson_export = false;

static bool have_astro_program = false;
static char *astro_program = NULL;
#define VIK_SETTINGS_EXTERNAL_ASTRO_PROGRAM "external_astro_program"

// NB Only performed once per program run
static void vik_trwlayer_class_init ( VikTrwLayerClass *klass )
{
  if ( ! a_settings_get_string ( VIK_SETTINGS_EXTERNAL_DIARY_PROGRAM, &diary_program ) ) {
#ifdef WINDOWS
    //diary_program = g_strdup( "C:\\Program Files\\Rednotebook\\rednotebook.exe" );
    diary_program = g_strdup( "C:/Progra~1/Rednotebook/rednotebook.exe" );
#else
    diary_program = g_strdup( "rednotebook" );
#endif
  }
  else {
    // User specified so assume it works
    have_diary_program = true;
  }

  if ( g_find_program_in_path( diary_program ) ) {
    char *mystdout = NULL;
    char *mystderr = NULL;
    // Needs RedNotebook 1.7.3+ for support of opening on a specified date
    char *cmd = g_strconcat ( diary_program, " --version", NULL ); // "rednotebook --version"
    if ( g_spawn_command_line_sync ( cmd, &mystdout, &mystderr, NULL, NULL ) ) {
      // Annoyingly 1.7.1|2|3 versions of RedNotebook prints the version to stderr!!
      if ( mystdout )
        fprintf(stderr, "DEBUG: Diary: %s\n", mystdout ); // Should be something like 'RedNotebook 1.4'
      if ( mystderr )
        fprintf(stderr, "WARNING: Diary: stderr: %s\n", mystderr );

      char **tokens = NULL;
      if ( mystdout && g_strcmp0(mystdout, "") )
        tokens = g_strsplit(mystdout, " ", 0);
      else if ( mystderr )
        tokens = g_strsplit(mystderr, " ", 0);

      if ( tokens ) {
        int num = 0;
        char *token = tokens[num];
        while ( token && num < 2 ) {
          if (num == 1) {
            if ( viking_version_to_number(token) >= viking_version_to_number((char *) "1.7.3") )
              have_diary_program = true;
          }
          num++;
          token = tokens[num];
        }
      }
      g_strfreev ( tokens );
    }
    free( mystdout );
    free( mystderr );
    free( cmd );
  }

  if ( g_find_program_in_path ( a_geojson_program_export() ) ) {
    have_geojson_export = true;
  }

  // Astronomy Domain
  if ( ! a_settings_get_string ( VIK_SETTINGS_EXTERNAL_ASTRO_PROGRAM, &astro_program ) ) {
#ifdef WINDOWS
    //astro_program = g_strdup( "C:\\Program Files\\Stellarium\\stellarium.exe" );
    astro_program = g_strdup( "C:/Progra~1/Stellarium/stellarium.exe" );
#else
    astro_program = g_strdup( "stellarium" );
#endif
  }
  else {
    // User specified so assume it works
    have_astro_program = true;
  }
  if ( g_find_program_in_path( astro_program ) ) {
    have_astro_program = true;
  }
}

GType vik_trw_layer_get_type ()
{
  static GType vtl_type = 0;

  if (!vtl_type)
  {
    static const GTypeInfo vtl_info =
    {
      sizeof (VikTrwLayerClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      (GClassInitFunc) vik_trwlayer_class_init, /* class init */
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikTrwLayer),
      0,
      NULL /* instance init */
    };
    vtl_type = g_type_register_static ( VIK_LAYER_TYPE, "VikTrwLayer", &vtl_info, (GTypeFlags) 0 );
  }
  return vtl_type;
}

VikTRWMetadata *vik_trw_metadata_new()
{
  VikTRWMetadata * data = (VikTRWMetadata *) malloc(sizeof (VikTRWMetadata));
  memset(data, 0, sizeof (VikTRWMetadata));

  return data;
}

void vik_trw_metadata_free ( VikTRWMetadata *metadata)
{
  free(metadata);
}

VikTRWMetadata *vik_trw_layer_get_metadata ( VikTrwLayer *vtl )
{
  return vtl->metadata;
}

void vik_trw_layer_set_metadata ( VikTrwLayer *vtl, VikTRWMetadata *metadata)
{
  if ( vtl->metadata )
    vik_trw_metadata_free ( vtl->metadata );
  vtl->metadata = metadata;
}





/**
 * Find an item by date
 */
bool LayerTRW::find_by_date(char const * date_str, VikCoord * position, VikViewport * vvp, bool do_tracks, bool select)
{
	date_finder_type df;
	df.found = false;
	df.date_str = date_str;
	df.trk = NULL;
	df.wp = NULL;
	// Only tracks ATM
	if (do_tracks) {
		LayerTRW::find_track_by_date(tracks, &df);
	} else {
		LayerTRW::find_waypoint_by_date(waypoints, &df);
	}

	if (select && df.found) {
		if (do_tracks && df.trk) {
			struct LatLon maxmin[2] = { {0,0}, {0,0} };
			LayerTRW::find_maxmin_in_track(df.trk, maxmin);
			this->zoom_to_show_latlons(&vvp->port, maxmin);
			vik_treeview_select_iter(VIK_LAYER(this->vl)->vt, tracks_iters.at(df.trk_uid), true);
		} else if (df.wp) {
			vvp->port.set_center_coord(&(df.wp->coord), true);
			vik_treeview_select_iter(VIK_LAYER(this->vl)->vt, waypoints_iters.at(df.wp_uid), true);
		}
		vik_layer_emit_update(VIK_LAYER(this->vl));
	}
	return df.found;
}





static void trw_layer_del_item ( VikTrwLayer *vtl, int subtype, void * sublayer )
{
  static menu_array_sublayer values;
  if (!sublayer) {
    return;
  }

  int ii;
  for ( ii = MA_VTL; ii < MA_LAST; ii++ )
    values[ii] = NULL;

  values[MA_VTL]         = vtl;
  values[MA_SUBTYPE]     = KINT_TO_POINTER (subtype);
  values[MA_SUBLAYER_ID] = sublayer;
  values[MA_CONFIRM]     = KINT_TO_POINTER (1); // Confirm delete request

  trw_layer_delete_item ( values );
}

static void trw_layer_cut_item ( VikTrwLayer *vtl, int subtype, void * sublayer )
{
  static menu_array_sublayer values;
  if (!sublayer) {
    return;
  }

  int ii;
  for ( ii = MA_VTL; ii < MA_LAST; ii++ )
    values[ii] = NULL;

  values[MA_VTL]         = vtl;
  values[MA_SUBTYPE]     = KINT_TO_POINTER (subtype);
  values[MA_SUBLAYER_ID] = sublayer;
  values[MA_CONFIRM]     = KINT_TO_POINTER (1); // Confirm delete request

  trw_layer_copy_item_cb(values);
  trw_layer_cut_item_cb(values);
}

static void trw_layer_copy_item_cb ( menu_array_sublayer values)
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  int subtype = KPOINTER_TO_INT (values[MA_SUBTYPE]);
  void ** sublayer = (void **) values[MA_SUBLAYER_ID];
  sg_uid_t uid = (sg_uid_t) ((long) sublayer);
  uint8_t *data = NULL;
  unsigned int len;

  trw_layer_copy_item( vtl, subtype, sublayer, &data, &len);

  if (data) {
    const char* name;
    if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT ) {
      sg_uid_t wp_uid = (sg_uid_t) ((long) sublayer);
      Waypoint * wp = vtl->trw->waypoints.at(uid);
      if ( wp && wp->name )
        name = wp->name;
      else
        name = NULL; // Broken :(
    }
    else if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
      Track * trk = vtl->trw->tracks.at(uid);
      if ( trk && trk->name )
        name = trk->name;
      else
        name = NULL; // Broken :(
    }
    else {
      Track * trk = vtl->trw->routes.at(uid);
      if ( trk && trk->name )
        name = trk->name;
      else
        name = NULL; // Broken :(
    }

    a_clipboard_copy( VIK_CLIPBOARD_DATA_SUBLAYER, VIK_LAYER_TRW,
		      subtype, len, name, data);
  }
}

static void trw_layer_cut_item_cb ( menu_array_sublayer values)
{
  trw_layer_copy_item_cb(values);
  values[MA_CONFIRM] = KINT_TO_POINTER (0); // Never need to confirm automatic delete
  trw_layer_delete_item(values);
}

static void trw_layer_paste_item_cb ( menu_array_sublayer values)
{
  // Slightly cheating method, routing via the panels capability
  a_clipboard_paste (VIK_LAYERS_PANEL(values[MA_VLP]));
}

static void trw_layer_copy_item ( VikTrwLayer *vtl, int subtype, void * sublayer, uint8_t **item, unsigned int *len )
{
  uint8_t *id;
  size_t il;

  if (!sublayer) {
    *item = NULL;
    return;
  }

  GByteArray *ba = g_byte_array_new ();
  sg_uid_t uid = (sg_uid_t) ((long) sublayer);

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT ) {
	  vtl->trw->waypoints.at(uid)->marshall(&id, &il);
  } else if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
	  vtl->trw->tracks.at(uid)->marshall(&id, &il);
  } else {
	  vtl->trw->routes.at(uid)->marshall(&id, &il);
  }

  g_byte_array_append ( ba, id, il );

  free(id);

  *len = ba->len;
  *item = ba->data;
}

static bool trw_layer_paste_item ( VikTrwLayer *vtl, int subtype, uint8_t *item, size_t len )
{
  if ( !item )
    return false;

  char *name;

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT )
  {
    Waypoint * wp = Waypoint::unmarshall(item, len);
    // When copying - we'll create a new name based on the original
    name = vtl->trw->new_unique_sublayer_name(VIK_TRW_LAYER_SUBLAYER_WAYPOINT, wp->name);
    vtl->trw->add_waypoint(wp, name);
    waypoint_convert(wp, &vtl->trw->coord_mode);
    free( name );

    vtl->trw->calculate_bounds_waypoints();

    // Consider if redraw necessary for the new item
    if ( vtl->vl.visible && vtl->trw->waypoints_visible && wp->visible )
      vik_layer_emit_update ( VIK_LAYER(vtl) );
    return true;
  }
  if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK )
  {
    Track * trk = Track::unmarshall(item, len);
    // When copying - we'll create a new name based on the original
    name = vtl->trw->new_unique_sublayer_name(VIK_TRW_LAYER_SUBLAYER_TRACK, trk->name);
    vtl->trw->add_track(trk, name);
    trk->convert(vtl->trw->coord_mode);
    free( name );

    // Consider if redraw necessary for the new item
    if ( vtl->vl.visible && vtl->trw->tracks_visible && trk->visible )
      vik_layer_emit_update ( VIK_LAYER(vtl) );
    return true;
  }
  if ( subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE )
  {
    Track * trk = Track::unmarshall(item, len);
    // When copying - we'll create a new name based on the original
    name = vtl->trw->new_unique_sublayer_name(VIK_TRW_LAYER_SUBLAYER_ROUTE, trk->name);
    vtl->trw->add_route(trk, name);
    trk->convert(vtl->trw->coord_mode);
    free( name );

    // Consider if redraw necessary for the new item
    if ( vtl->vl.visible && vtl->trw->routes_visible && trk->visible )
      vik_layer_emit_update ( VIK_LAYER(vtl) );
    return true;
  }
  return false;
}

static void trw_layer_free_copied_item ( int subtype, void * item )
{
  if (item) {
    free(item);
  }
}

static void image_cache_free ( VikTrwLayer *vtl )
{
  g_list_foreach ( vtl->image_cache->head, (GFunc)cached_pixbuf_free, NULL );
  g_queue_free ( vtl->image_cache );
}

static bool trw_layer_set_param ( VikTrwLayer *vtl, uint16_t id, VikLayerParamData data, VikViewport *vp, bool is_file_operation )
{
  switch ( id )
  {
    case PARAM_TV: vtl->trw->tracks_visible = data.b; break;
    case PARAM_WV: vtl->trw->waypoints_visible = data.b; break;
    case PARAM_RV: vtl->trw->routes_visible = data.b; break;
    case PARAM_TDL: vtl->track_draw_labels = data.b; break;
    case PARAM_TLFONTSIZE:
      if ( data.u < FS_NUM_SIZES ) {
        vtl->track_font_size = (font_size_t) data.u;
        free( vtl->track_fsize_str );
        switch ( vtl->track_font_size ) {
          case FS_XX_SMALL: vtl->track_fsize_str = g_strdup( "xx-small" ); break;
          case FS_X_SMALL: vtl->track_fsize_str = g_strdup( "x-small" ); break;
          case FS_SMALL: vtl->track_fsize_str = g_strdup( "small" ); break;
          case FS_LARGE: vtl->track_fsize_str = g_strdup( "large" ); break;
          case FS_X_LARGE: vtl->track_fsize_str = g_strdup( "x-large" ); break;
          case FS_XX_LARGE: vtl->track_fsize_str = g_strdup( "xx-large" ); break;
          default: vtl->track_fsize_str = g_strdup( "medium" ); break;
        }
      }
      break;
    case PARAM_DM: vtl->drawmode = data.u; break;
    case PARAM_TC:
      vtl->track_color = data.c;
      if ( vp ) trw_layer_new_track_gcs ( vtl, vp );
      break;
    case PARAM_DP: vtl->drawpoints = data.b; break;
    case PARAM_DPS:
      if ( data.u >= MIN_POINT_SIZE && data.u <= MAX_POINT_SIZE )
        vtl->drawpoints_size = data.u;
      break;
    case PARAM_DE: vtl->drawelevation = data.b; break;
    case PARAM_DS: vtl->drawstops = data.b; break;
    case PARAM_DL: vtl->drawlines = data.b; break;
    case PARAM_DD: vtl->drawdirections = data.b; break;
    case PARAM_DDS:
      if ( data.u >= MIN_ARROW_SIZE && data.u <= MAX_ARROW_SIZE )
        vtl->drawdirections_size = data.u;
      break;
    case PARAM_SL: if ( data.u >= MIN_STOP_LENGTH && data.u <= MAX_STOP_LENGTH )
                     vtl->stop_length = data.u;
                   break;
    case PARAM_EF: if ( data.u >= 1 && data.u <= 100 )
                     vtl->elevation_factor = data.u;
                   break;
    case PARAM_LT: if ( data.u > 0 && data.u < 15 && data.u != vtl->line_thickness )
                   {
                     vtl->line_thickness = data.u;
                     if ( vp ) trw_layer_new_track_gcs ( vtl, vp );
                   }
                   break;
    case PARAM_BLT: if ( data.u <= 8 && data.u != vtl->bg_line_thickness )
                   {
                     vtl->bg_line_thickness = data.u;
                     if ( vp ) trw_layer_new_track_gcs ( vtl, vp );
                   }
                   break;
    case PARAM_TBGC:
      vtl->track_bg_color = data.c;
      if ( vtl->track_bg_gc )
        gdk_gc_set_rgb_fg_color(vtl->track_bg_gc, &(vtl->track_bg_color));
      break;
    case PARAM_TDSF: vtl->track_draw_speed_factor = data.d; break;
  case PARAM_TSO: if ( data.u < VL_SO_LAST ) vtl->track_sort_order = (vik_layer_sort_order_t) data.u; break;
    case PARAM_DLA: vtl->drawlabels = data.b; break;
    case PARAM_DI: vtl->drawimages = data.b; break;
    case PARAM_IS: if ( data.u != vtl->image_size )
      {
        vtl->image_size = data.u;
        image_cache_free ( vtl );
        vtl->image_cache = g_queue_new ();
      }
      break;
    case PARAM_IA: if ( data.u != vtl->image_alpha )
      {
        vtl->image_alpha = data.u;
        image_cache_free ( vtl );
        vtl->image_cache = g_queue_new ();
      }
      break;
    case PARAM_ICS: vtl->image_cache_size = data.u;
      while ( vtl->image_cache->length > vtl->image_cache_size ) /* if shrinking cache_size, free pixbuf ASAP */
	cached_pixbuf_free ( (CachedPixbuf *) g_queue_pop_tail ( vtl->image_cache ) );
      break;
    case PARAM_WPC:
      vtl->waypoint_color = data.c;
      if ( vtl->waypoint_gc )
        gdk_gc_set_rgb_fg_color(vtl->waypoint_gc, &(vtl->waypoint_color));
      break;
    case PARAM_WPTC:
      vtl->waypoint_text_color = data.c;
      if ( vtl->waypoint_text_gc )
        gdk_gc_set_rgb_fg_color(vtl->waypoint_text_gc, &(vtl->waypoint_text_color));
      break;
    case PARAM_WPBC:
      vtl->waypoint_bg_color = data.c;
      if ( vtl->waypoint_bg_gc )
        gdk_gc_set_rgb_fg_color(vtl->waypoint_bg_gc, &(vtl->waypoint_bg_color));
      break;
    case PARAM_WPBA:
      vtl->wpbgand = (GdkFunction) data.b;
      if ( vtl->waypoint_bg_gc )
        gdk_gc_set_function(vtl->waypoint_bg_gc, data.b ? GDK_AND : GDK_COPY );
      break;
    case PARAM_WPSYM: if ( data.u < WP_NUM_SYMBOLS ) vtl->wp_symbol = data.u; break;
    case PARAM_WPSIZE: if ( data.u > 0 && data.u <= 64 ) vtl->wp_size = data.u; break;
    case PARAM_WPSYMS: vtl->wp_draw_symbols = data.b; break;
    case PARAM_WPFONTSIZE:
      if ( data.u < FS_NUM_SIZES ) {
        vtl->wp_font_size = (font_size_t) data.u;
        free( vtl->wp_fsize_str );
        switch ( vtl->wp_font_size ) {
          case FS_XX_SMALL: vtl->wp_fsize_str = g_strdup( "xx-small" ); break;
          case FS_X_SMALL: vtl->wp_fsize_str = g_strdup( "x-small" ); break;
          case FS_SMALL: vtl->wp_fsize_str = g_strdup( "small" ); break;
          case FS_LARGE: vtl->wp_fsize_str = g_strdup( "large" ); break;
          case FS_X_LARGE: vtl->wp_fsize_str = g_strdup( "x-large" ); break;
          case FS_XX_LARGE: vtl->wp_fsize_str = g_strdup( "xx-large" ); break;
          default: vtl->wp_fsize_str = g_strdup( "medium" ); break;
        }
      }
      break;
  case PARAM_WPSO: if ( data.u < VL_SO_LAST ) vtl->wp_sort_order = (vik_layer_sort_order_t) data.u; break;
    // Metadata
    case PARAM_MDDESC: if ( data.s && vtl->metadata ) vtl->metadata->description = g_strdup(data.s); break;
    case PARAM_MDAUTH: if ( data.s && vtl->metadata ) vtl->metadata->author = g_strdup(data.s); break;
    case PARAM_MDTIME: if ( data.s && vtl->metadata ) vtl->metadata->timestamp = g_strdup(data.s); break;
    case PARAM_MDKEYS: if ( data.s && vtl->metadata ) vtl->metadata->keywords = g_strdup(data.s); break;
    default: break;
  }
  return true;
}

static VikLayerParamData trw_layer_get_param ( VikTrwLayer *vtl, uint16_t id, bool is_file_operation )
{
  VikLayerParamData rv;
  switch ( id )
  {
    case PARAM_TV: rv.b = vtl->trw->tracks_visible; break;
    case PARAM_WV: rv.b = vtl->trw->waypoints_visible; break;
    case PARAM_RV: rv.b = vtl->trw->routes_visible; break;
    case PARAM_TDL: rv.b = vtl->track_draw_labels; break;
    case PARAM_TLFONTSIZE: rv.u = vtl->track_font_size; break;
    case PARAM_DM: rv.u = vtl->drawmode; break;
    case PARAM_TC: rv.c = vtl->track_color; break;
    case PARAM_DP: rv.b = vtl->drawpoints; break;
    case PARAM_DPS: rv.u = vtl->drawpoints_size; break;
    case PARAM_DE: rv.b = vtl->drawelevation; break;
    case PARAM_EF: rv.u = vtl->elevation_factor; break;
    case PARAM_DS: rv.b = vtl->drawstops; break;
    case PARAM_SL: rv.u = vtl->stop_length; break;
    case PARAM_DL: rv.b = vtl->drawlines; break;
    case PARAM_DD: rv.b = vtl->drawdirections; break;
    case PARAM_DDS: rv.u = vtl->drawdirections_size; break;
    case PARAM_LT: rv.u = vtl->line_thickness; break;
    case PARAM_BLT: rv.u = vtl->bg_line_thickness; break;
    case PARAM_DLA: rv.b = vtl->drawlabels; break;
    case PARAM_DI: rv.b = vtl->drawimages; break;
    case PARAM_TBGC: rv.c = vtl->track_bg_color; break;
    case PARAM_TDSF: rv.d = vtl->track_draw_speed_factor; break;
    case PARAM_TSO: rv.u = vtl->track_sort_order; break;
    case PARAM_IS: rv.u = vtl->image_size; break;
    case PARAM_IA: rv.u = vtl->image_alpha; break;
    case PARAM_ICS: rv.u = vtl->image_cache_size; break;
    case PARAM_WPC: rv.c = vtl->waypoint_color; break;
    case PARAM_WPTC: rv.c = vtl->waypoint_text_color; break;
    case PARAM_WPBC: rv.c = vtl->waypoint_bg_color; break;
    case PARAM_WPBA: rv.b = vtl->wpbgand; break;
    case PARAM_WPSYM: rv.u = vtl->wp_symbol; break;
    case PARAM_WPSIZE: rv.u = vtl->wp_size; break;
    case PARAM_WPSYMS: rv.b = vtl->wp_draw_symbols; break;
    case PARAM_WPFONTSIZE: rv.u = vtl->wp_font_size; break;
    case PARAM_WPSO: rv.u = vtl->wp_sort_order; break;
    // Metadata
    case PARAM_MDDESC: if (vtl->metadata) { rv.s = vtl->metadata->description; } break;
    case PARAM_MDAUTH: if (vtl->metadata) { rv.s = vtl->metadata->author; } break;
    case PARAM_MDTIME: if (vtl->metadata) { rv.s = vtl->metadata->timestamp; } break;
    case PARAM_MDKEYS: if (vtl->metadata) { rv.s = vtl->metadata->keywords; } break;
    default: break;
  }
  return rv;
}

static void trw_layer_change_param ( GtkWidget *widget, ui_change_values values )
{
  // This '-3' is to account for the first few parameters not in the properties
  const int OFFSET = -3;

  switch ( KPOINTER_TO_INT(values[UI_CHG_PARAM_ID]) ) {
    // Alter sensitivity of waypoint draw image related widgets according to the draw image setting.
    case PARAM_DI: {
      // Get new value
      VikLayerParamData vlpd = a_uibuilder_widget_get_value ( widget, (VikLayerParam *) values[UI_CHG_PARAM] );
      GtkWidget **ww1 = (GtkWidget **) &values[UI_CHG_WIDGETS];
      GtkWidget **ww2 = (GtkWidget **) &values[UI_CHG_LABELS];
      GtkWidget *w1 = ww1[OFFSET + PARAM_IS];
      GtkWidget *w2 = ww2[OFFSET + PARAM_IS];
      GtkWidget *w3 = ww1[OFFSET + PARAM_IA];
      GtkWidget *w4 = ww2[OFFSET + PARAM_IA];
      GtkWidget *w5 = ww1[OFFSET + PARAM_ICS];
      GtkWidget *w6 = ww2[OFFSET + PARAM_ICS];
      if ( w1 ) gtk_widget_set_sensitive ( w1, vlpd.b );
      if ( w2 ) gtk_widget_set_sensitive ( w2, vlpd.b );
      if ( w3 ) gtk_widget_set_sensitive ( w3, vlpd.b );
      if ( w4 ) gtk_widget_set_sensitive ( w4, vlpd.b );
      if ( w5 ) gtk_widget_set_sensitive ( w5, vlpd.b );
      if ( w6 ) gtk_widget_set_sensitive ( w6, vlpd.b );
      break;
    }
    // Alter sensitivity of waypoint label related widgets according to the draw label setting.
    case PARAM_DLA: {
      // Get new value
      VikLayerParamData vlpd = a_uibuilder_widget_get_value ( widget, (VikLayerParam *) values[UI_CHG_PARAM] );
      GtkWidget **ww1 = (GtkWidget **) &values[UI_CHG_WIDGETS];
      GtkWidget **ww2 = (GtkWidget **) &values[UI_CHG_LABELS];
      GtkWidget *w1 = ww1[OFFSET + PARAM_WPTC];
      GtkWidget *w2 = ww2[OFFSET + PARAM_WPTC];
      GtkWidget *w3 = ww1[OFFSET + PARAM_WPBC];
      GtkWidget *w4 = ww2[OFFSET + PARAM_WPBC];
      GtkWidget *w5 = ww1[OFFSET + PARAM_WPBA];
      GtkWidget *w6 = ww2[OFFSET + PARAM_WPBA];
      GtkWidget *w7 = ww1[OFFSET + PARAM_WPFONTSIZE];
      GtkWidget *w8 = ww2[OFFSET + PARAM_WPFONTSIZE];
      if ( w1 ) gtk_widget_set_sensitive ( w1, vlpd.b );
      if ( w2 ) gtk_widget_set_sensitive ( w2, vlpd.b );
      if ( w3 ) gtk_widget_set_sensitive ( w3, vlpd.b );
      if ( w4 ) gtk_widget_set_sensitive ( w4, vlpd.b );
      if ( w5 ) gtk_widget_set_sensitive ( w5, vlpd.b );
      if ( w6 ) gtk_widget_set_sensitive ( w6, vlpd.b );
      if ( w7 ) gtk_widget_set_sensitive ( w7, vlpd.b );
      if ( w8 ) gtk_widget_set_sensitive ( w8, vlpd.b );
      break;
    }
    // Alter sensitivity of all track colours according to the draw track mode.
    case PARAM_DM: {
      // Get new value
      VikLayerParamData vlpd = a_uibuilder_widget_get_value ( widget, (VikLayerParam *) values[UI_CHG_PARAM] );
      bool sensitive = ( vlpd.u == DRAWMODE_ALL_SAME_COLOR );
      GtkWidget **ww1 = (GtkWidget **) &values[UI_CHG_WIDGETS];
      GtkWidget **ww2 = (GtkWidget **) &values[UI_CHG_LABELS];
      GtkWidget *w1 = ww1[OFFSET + PARAM_TC];
      GtkWidget *w2 = ww2[OFFSET + PARAM_TC];
      if ( w1 ) gtk_widget_set_sensitive ( w1, sensitive );
      if ( w2 ) gtk_widget_set_sensitive ( w2, sensitive );
      break;
    }
    case PARAM_MDTIME: {
      // Force metadata->timestamp to be always read-only for now.
      GtkWidget **ww = (GtkWidget **) &values[UI_CHG_WIDGETS];
      GtkWidget *w1 = ww[OFFSET + PARAM_MDTIME];
      if ( w1 ) gtk_widget_set_sensitive ( w1, false );
    }
    // NB Since other track settings have been split across tabs,
    // I don't think it's useful to set sensitivities on widgets you can't immediately see
    default: break;
  }
}

static void trw_layer_marshall( VikTrwLayer *vtl, uint8_t **data, int *len )
{
  uint8_t *pd;
  int pl;

  *data = NULL;

  // Use byte arrays to store sublayer data
  // much like done elsewhere e.g. vik_layer_marshall_params()
  GByteArray *ba = g_byte_array_new ( );

  uint8_t *sl_data;
  size_t sl_len;

  unsigned int object_length;
  unsigned int subtype;
  // store:
  // the length of the item
  // the sublayer type of item
  // the the actual item
#define tlm_append(object_pointer, size, type)	\
  subtype = (type); \
  object_length = (size); \
  g_byte_array_append ( ba, (uint8_t *)&object_length, sizeof(object_length) ); \
  g_byte_array_append ( ba, (uint8_t *)&subtype, sizeof(subtype) ); \
  g_byte_array_append ( ba, (object_pointer), object_length );

  // Layer parameters first
  vik_layer_marshall_params(VIK_LAYER(vtl), &pd, &pl);
  g_byte_array_append ( ba, (uint8_t *)&pl, sizeof(pl) ); \
  g_byte_array_append ( ba, pd, pl );
  free( pd );

  // Now sublayer data
  GHashTableIter iter;
  void * key, *value;

  // Waypoints
  for (auto i = vtl->trw->waypoints.begin(); i != vtl->trw->waypoints.end(); i++) {
	  i->second->marshall(&sl_data, &sl_len);
	  tlm_append(sl_data, sl_len, VIK_TRW_LAYER_SUBLAYER_WAYPOINT);
	  free(sl_data);
  }

  // Tracks
  for (auto i = vtl->trw->tracks.begin(); i != vtl->trw->tracks.end(); i++) {
	  i->second->marshall(&sl_data, &sl_len);
	  tlm_append(sl_data, sl_len, VIK_TRW_LAYER_SUBLAYER_TRACK);
	  free(sl_data);
  }

  // Routes
  for (auto i = vtl->trw->routes.begin(); i != vtl->trw->routes.end(); i++) {
	  i->second->marshall(&sl_data, &sl_len);
	  tlm_append(sl_data, sl_len, VIK_TRW_LAYER_SUBLAYER_ROUTE);
	  free(sl_data);
  }

#undef tlm_append

  *data = ba->data;
  *len = ba->len;
}

static VikTrwLayer *trw_layer_unmarshall( uint8_t *data, int len, VikViewport *vvp )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(vik_layer_create ( VIK_LAYER_TRW, vvp, false ));
  int pl;
  int consumed_length;

  // First the overall layer parameters
  memcpy(&pl, data, sizeof(pl));
  data += sizeof(pl);
  vik_layer_unmarshall_params ( VIK_LAYER(vtl), data, pl, vvp );
  data += pl;

  consumed_length = pl;
  const int sizeof_len_and_subtype = sizeof(int) + sizeof(int);

#define tlm_size (*(int *)data)
  // See marshalling above for order of how this is written
#define tlm_next \
  data += sizeof_len_and_subtype + tlm_size;

  // Now the individual sublayers:

  while ( *data && consumed_length < len ) {
    // Normally four extra bytes at the end of the datastream
    //  (since it's a GByteArray and that's where it's length is stored)
    //  So only attempt read when there's an actual block of sublayer data
    if ( consumed_length + tlm_size < len ) {

      // Reuse pl to read the subtype from the data stream
      memcpy(&pl, data+sizeof(int), sizeof(pl));

      // Also remember to (attempt to) convert each coordinate in case this is pasted into a different drawmode
      if ( pl == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
	Track * trk = Track::unmarshall(data + sizeof_len_and_subtype, 0);
        char *name = g_strdup( trk->name );
        vtl->trw->add_track(trk, name);
        free( name );
        trk->convert(vtl->trw->coord_mode);
      }
      if ( pl == VIK_TRW_LAYER_SUBLAYER_WAYPOINT ) {
	Waypoint * wp = Waypoint::unmarshall(data + sizeof_len_and_subtype, 0);
        char *name = g_strdup( wp->name );
        vtl->trw->add_waypoint(wp, name);
        free( name );
        waypoint_convert(wp, &vtl->trw->coord_mode);
      }
      if ( pl == VIK_TRW_LAYER_SUBLAYER_ROUTE ) {
	Track * trk = Track::unmarshall(data + sizeof_len_and_subtype, 0);
        char *name = g_strdup( trk->name );
        vtl->trw->add_route(trk, name);
        free( name );
        trk->convert(vtl->trw->coord_mode);
      }
    }
    consumed_length += tlm_size + sizeof_len_and_subtype;
    tlm_next;
  }
  //fprintf(stderr, "DEBUG: consumed_length %d vs len %d\n", consumed_length, len);

  // Not stored anywhere else so need to regenerate
  vtl->trw->calculate_bounds_waypoints();

  return vtl;
}

// Keep interesting hash function at least visible
/*
static unsigned int strcase_hash(gconstpointer v)
{
  // 31 bit hash function
  int i;
  const char *t = v;
  char s[128];   // malloc is too slow for reading big files
  char *p = s;

  for (i = 0; (i < (sizeof(s)- 1)) && t[i]; i++)
      p[i] = toupper(t[i]);
  p[i] = '\0';

  p = s;
  uint32_t h = *p;
  if (h) {
    for (p += 1; *p != '\0'; p++)
      h = (h << 5) - h + *p;
  }

  return h;
}
*/

// Stick a 1 at the end of the function name to make it more unique
//  thus more easily searchable in a simple text editor
static VikTrwLayer* trw_layer_new1 ( VikViewport *vvp )
{
  VikTrwLayer *rv = VIK_TRW_LAYER ( g_object_new ( VIK_TRW_LAYER_TYPE, NULL ) );
  vik_layer_set_type ( VIK_LAYER(rv), VIK_LAYER_TRW );

  // It's not entirely clear the benefits of hash tables usage here - possibly the simplicity of first implementation for unique names
  // Now with the name of the item stored as part of the item - these tables are effectively straightforward lists

  // For this reworking I've choosen to keep the use of hash tables since for the expected data sizes
  // - even many hundreds of waypoints and tracks is quite small in the grand scheme of things,
  //  and with normal PC processing capabilities - it has negligibile performance impact
  // This also minimized the amount of rework - as the management of the hash tables already exists.

  // The hash tables are indexed by simple integers acting as a UUID hash, which again shouldn't affect performance much
  //   we have to maintain a uniqueness (as before when multiple names where not allowed),
  //   this is to ensure it refers to the same item in the data structures used on the viewport and on the layers panel

  //rv->waypoints = g_hash_table_new_full ( g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) Waypoint::delete_waypoint);
  //rv->waypoints_iters = g_hash_table_new_full ( g_direct_hash, g_direct_equal, NULL, g_free );
  //rv->tracks = g_hash_table_new_full ( g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) Track::delete_track);
  //rv->tracks_iters = g_hash_table_new_full ( g_direct_hash, g_direct_equal, NULL, g_free );
  //rv->routes = g_hash_table_new_full ( g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) Track::delete_track);
  //rv->routes_iters = g_hash_table_new_full ( g_direct_hash, g_direct_equal, NULL, g_free );

  rv->image_cache = g_queue_new(); // Must be performed before set_params via set_defaults

  vik_layer_set_defaults ( VIK_LAYER(rv), vvp );


  ((VikLayer *) rv)->layer = new LayerTRW((VikLayer *) rv);
  rv->trw = (LayerTRW *) ((VikLayer *) rv)->layer;

  // Param settings that are not available via the GUI
  // Force to on after processing params (which defaults them to off with a zero value)
  rv->trw->waypoints_visible = rv->trw->tracks_visible = rv->trw->routes_visible = true;

  rv->metadata = vik_trw_metadata_new ();
  rv->draw_sync_done = true;
  rv->draw_sync_do = true;
  // Everything else is 0, false or NULL

  //rv->trw->vtl = rv; /* Reference to parent object of type VikTrwLayer for trw. */



  return rv;
}


static void trw_layer_free ( VikTrwLayer *trwlayer )
{
  /* kamilTODO: call destructors of objects in these maps. */
  trwlayer->trw->waypoints.clear();
  trwlayer->trw->waypoints_iters.clear();
  trwlayer->trw->tracks.clear();
  trwlayer->trw->tracks_iters.clear();
  trwlayer->trw->routes.clear();
  trwlayer->trw->routes_iters.clear();

  /* ODC: replace with GArray */
  trw_layer_free_track_gcs ( trwlayer );

  if ( trwlayer->wp_right_click_menu )
    g_object_ref_sink ( G_OBJECT(trwlayer->wp_right_click_menu) );

  if ( trwlayer->track_right_click_menu )
    g_object_ref_sink ( G_OBJECT(trwlayer->track_right_click_menu) );

  if ( trwlayer->tracklabellayout != NULL)
    g_object_unref ( G_OBJECT ( trwlayer->tracklabellayout ) );

  if ( trwlayer->wplabellayout != NULL)
    g_object_unref ( G_OBJECT ( trwlayer->wplabellayout ) );

  if ( trwlayer->waypoint_gc != NULL )
    g_object_unref ( G_OBJECT ( trwlayer->waypoint_gc ) );

  if ( trwlayer->waypoint_text_gc != NULL )
    g_object_unref ( G_OBJECT ( trwlayer->waypoint_text_gc ) );

  if ( trwlayer->waypoint_bg_gc != NULL )
    g_object_unref ( G_OBJECT ( trwlayer->waypoint_bg_gc ) );

  free( trwlayer->wp_fsize_str );
  free( trwlayer->track_fsize_str );

  if ( trwlayer->trw->tpwin != NULL )
    gtk_widget_destroy ( GTK_WIDGET(trwlayer->trw->tpwin) );

  if ( trwlayer->tracks_analysis_dialog != NULL )
    gtk_widget_destroy ( GTK_WIDGET(trwlayer->tracks_analysis_dialog) );

  image_cache_free ( trwlayer );
}





void LayerTRW::draw_with_highlight(Viewport * viewport, bool highlight)
{
	static DrawingParams dp;
	init_drawing_params(&dp, (VikTrwLayer *) this->vl, viewport, highlight);

	if (tracks_visible) {
		trw_layer_draw_track_cb(tracks, &dp);
	}

	if (routes_visible) {
		trw_layer_draw_track_cb(routes, &dp);
	}

	if (waypoints_visible) {
		trw_layer_draw_waypoints_cb(&waypoints, &dp);
	}
}





static void trw_layer_draw(VikTrwLayer * l, void * data)
{
	Viewport * viewport = &(VIK_VIEWPORT(data)->port);

	// If this layer is to be highlighted - then don't draw now - as it will be drawn later on in the specific highlight draw stage
	// This may seem slightly inefficient to test each time for every layer
	//  but for a layer with *lots* of tracks & waypoints this can save some effort by not drawing the items twice
	if (viewport->get_draw_highlight() &&
	    vik_window_get_selected_trw_layer((VikWindow*)VIK_GTK_WINDOW_FROM_LAYER((VikLayer*)l)) == l) {
		return;
	}

	l->trw->draw_with_highlight(viewport, false);
}





void LayerTRW::draw_highlight(Viewport * viewport)
{
	VikLayer * parent = VIK_LAYER(this->vl);

	// Check the layer for visibility (including all the parents visibilities)
	if (!vik_treeview_item_get_visible_tree(parent->vt, &parent->iter)) {
		return;
	}
	this->draw_with_highlight(viewport, true);
}





/**
 * vik_trw_layer_draw_highlight_item:
 *
 * Only handles a single track or waypoint ATM
 * It assumes the track or waypoint belongs to the TRW Layer (it doesn't check this is the case)
 */
void LayerTRW::draw_highlight_item(Track * trk, Waypoint * wp, Viewport * viewport)
{
	VikLayer * parent = VIK_LAYER(this->vl);

	// Check the layer for visibility (including all the parents visibilities)
	if (!vik_treeview_item_get_visible_tree(parent->vt, &(parent->iter))) {
		return;
	}

	static DrawingParams dp;
	init_drawing_params(&dp, (VikTrwLayer *) this->vl, viewport, true);

	if (trk) {
		bool draw = (trk->is_route && routes_visible) || (!trk->is_route && tracks_visible);
		if (draw) {
			trw_layer_draw_track_cb(NULL, trk, &dp);
		}
	}
	if (waypoints_visible && wp) {
		trw_layer_draw_waypoint_cb(wp, &dp);
	}
}





/**
 * vik_trw_layer_draw_highlight_item:
 *
 * Generally for drawing all tracks or routes or waypoints
 * tracks may be actually routes
 * It assumes they belong to the TRW Layer (it doesn't check this is the case)
 */
void LayerTRW::draw_highlight_items(std::unordered_map<sg_uid_t, Track *> * tracks, std::unordered_map<sg_uid_t, Waypoint *> * waypoints, Viewport * viewport)
{
	VikLayer * parent = VIK_LAYER(this->vl);

	// Check the layer for visibility (including all the parents visibilities)
	if (!vik_treeview_item_get_visible_tree(parent->vt, &(parent->iter))) {
		return;
	}

	static DrawingParams dp;
	init_drawing_params(&dp, (VikTrwLayer *) this->vl, viewport, true);

	if (tracks) {
		bool is_routes = (tracks == &routes);
		bool draw = (is_routes && routes_visible) || (!is_routes && tracks_visible);
		if (draw) {
			trw_layer_draw_track_cb(*tracks, &dp);
		}
	}

	if (waypoints_visible) {
		trw_layer_draw_waypoints_cb(waypoints, &dp);
	}
}





static void trw_layer_free_track_gcs ( VikTrwLayer *vtl )
{
  int i;
  if ( vtl->track_bg_gc )
  {
    g_object_unref ( vtl->track_bg_gc );
    vtl->track_bg_gc = NULL;
  }
  if ( vtl->track_1color_gc )
  {
    g_object_unref ( vtl->track_1color_gc );
    vtl->track_1color_gc = NULL;
  }
  if ( vtl->current_track_gc )
  {
    g_object_unref ( vtl->current_track_gc );
    vtl->current_track_gc = NULL;
  }
  if ( vtl->current_track_newpoint_gc )
  {
    g_object_unref ( vtl->current_track_newpoint_gc );
    vtl->current_track_newpoint_gc = NULL;
  }

  if ( ! vtl->track_gc )
    return;
  for ( i = vtl->track_gc->len - 1; i >= 0; i-- )
    g_object_unref ( g_array_index ( vtl->track_gc, GObject *, i ) );
  g_array_free ( vtl->track_gc, true );
  vtl->track_gc = NULL;
}

static void trw_layer_new_track_gcs ( VikTrwLayer *vtl, VikViewport *vp )
{
  GdkGC *gc[ VIK_TRW_LAYER_TRACK_GC ];
  int width = vtl->line_thickness;

  if ( vtl->track_gc )
    trw_layer_free_track_gcs ( vtl );

  if ( vtl->track_bg_gc )
    g_object_unref ( vtl->track_bg_gc );
  vtl->track_bg_gc = vp->port.new_gc_from_color(&(vtl->track_bg_color), width + vtl->bg_line_thickness);

  // Ensure new track drawing heeds line thickness setting
  //  however always have a minium of 2, as 1 pixel is really narrow
  int new_track_width = (vtl->line_thickness < 2) ? 2 : vtl->line_thickness;

  if ( vtl->current_track_gc )
    g_object_unref ( vtl->current_track_gc );
  vtl->current_track_gc = vp->port.new_gc("#FF0000", new_track_width);
  gdk_gc_set_line_attributes ( vtl->current_track_gc, new_track_width, GDK_LINE_ON_OFF_DASH, GDK_CAP_ROUND, GDK_JOIN_ROUND );

  // 'newpoint' gc is exactly the same as the current track gc
  if ( vtl->current_track_newpoint_gc )
    g_object_unref ( vtl->current_track_newpoint_gc );
  vtl->current_track_newpoint_gc = vp->port.new_gc("#FF0000", new_track_width );
  gdk_gc_set_line_attributes ( vtl->current_track_newpoint_gc, new_track_width, GDK_LINE_ON_OFF_DASH, GDK_CAP_ROUND, GDK_JOIN_ROUND );

  vtl->track_gc = g_array_sized_new ( false, false, sizeof ( GdkGC * ), VIK_TRW_LAYER_TRACK_GC );

  gc[VIK_TRW_LAYER_TRACK_GC_STOP] = vp->port.new_gc("#874200", width);
  gc[VIK_TRW_LAYER_TRACK_GC_BLACK] = vp->port.new_gc("#000000", width); // black

  gc[VIK_TRW_LAYER_TRACK_GC_SLOW] = vp->port.new_gc("#E6202E", width); // red-ish
  gc[VIK_TRW_LAYER_TRACK_GC_AVER] = vp->port.new_gc("#D2CD26", width); // yellow-ish
  gc[VIK_TRW_LAYER_TRACK_GC_FAST] = vp->port.new_gc("#2B8700", width); // green-ish

  gc[VIK_TRW_LAYER_TRACK_GC_SINGLE] = vp->port.new_gc_from_color(&(vtl->track_color), width );

  g_array_append_vals ( vtl->track_gc, gc, VIK_TRW_LAYER_TRACK_GC );
}

static VikTrwLayer* trw_layer_create ( VikViewport *vp )
{
  VikTrwLayer *rv = trw_layer_new1 ( vp );
  vik_layer_rename ( VIK_LAYER(rv), vik_trw_layer_interface.name );

  if ( vp == NULL || gtk_widget_get_window(GTK_WIDGET(vp)) == NULL ) {
    /* early exit, as the rest is GUI related */
    return rv;
  }

  rv->wplabellayout = gtk_widget_create_pango_layout (GTK_WIDGET(vp), NULL);
  pango_layout_set_font_description (rv->wplabellayout, gtk_widget_get_style(GTK_WIDGET(vp))->font_desc);

  rv->tracklabellayout = gtk_widget_create_pango_layout (GTK_WIDGET(vp), NULL);
  pango_layout_set_font_description (rv->tracklabellayout, gtk_widget_get_style(GTK_WIDGET(vp))->font_desc);

  trw_layer_new_track_gcs ( rv, vp );

  rv->waypoint_gc = vp->port.new_gc_from_color(&(rv->waypoint_color), 2);
  rv->waypoint_text_gc = vp->port.new_gc_from_color(&(rv->waypoint_text_color), 1);
  rv->waypoint_bg_gc = vp->port.new_gc_from_color(&(rv->waypoint_bg_color), 1);
  gdk_gc_set_function ( rv->waypoint_bg_gc, rv->wpbgand );

  rv->trw->coord_mode = vp->port.get_coord_mode();

  rv->menu_selection = vik_layer_get_interface(VIK_LAYER(rv)->type)->menu_items_selection;

  return rv;
}

#define SMALL_ICON_SIZE 18
/*
 * Can accept a null symbol, and may return null value
 */
GdkPixbuf* get_wp_sym_small ( char *symbol )
{
  GdkPixbuf* wp_icon = a_get_wp_sym (symbol);
  // ATM a_get_wp_sym returns a cached icon, with the size dependent on the preferences.
  //  So needing a small icon for the treeview may need some resizing:
  if ( wp_icon && gdk_pixbuf_get_width ( wp_icon ) != SMALL_ICON_SIZE )
    wp_icon = gdk_pixbuf_scale_simple ( wp_icon, SMALL_ICON_SIZE, SMALL_ICON_SIZE, GDK_INTERP_BILINEAR );
  return wp_icon;
}





void LayerTRW::realize_track(std::unordered_map<sg_uid_t, Track *> & tracks, void * pass_along[4], int sublayer_id)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		GtkTreeIter * new_iter = (GtkTreeIter *) malloc(sizeof(GtkTreeIter));
		Track * trk = i->second;

		GdkPixbuf *pixbuf = NULL;

		if (trk->has_color) {
			pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, false, 8, SMALL_ICON_SIZE, SMALL_ICON_SIZE);
			// Annoyingly the GdkColor.pixel does not give the correct color when passed to gdk_pixbuf_fill (even when alloc'ed)
			// Here is some magic found to do the conversion
			// http://www.cs.binghamton.edu/~sgreene/cs360-2011s/topics/gtk+-2.20.1/gtk/gtkcolorbutton.c
			uint32_t pixel = ((trk->color.red & 0xff00) << 16)
				| ((trk->color.green & 0xff00) << 8)
				| (trk->color.blue & 0xff00);

			gdk_pixbuf_fill(pixbuf, pixel);
		}

		time_t timestamp = 0;
		Trackpoint * tpt = trk->get_tp_first();
		if (tpt && tpt->has_timestamp) {
			timestamp = tpt->timestamp;
		}

		vik_treeview_add_sublayer((VikTreeview *) pass_along[3], (GtkTreeIter *) pass_along[0], (GtkTreeIter *) pass_along[1], trk->name, pass_along[2], (void *) ((long) i->first), sublayer_id, pixbuf, true, timestamp);

		if (pixbuf) {
			g_object_unref(pixbuf);
		}

		*new_iter = *((GtkTreeIter *) pass_along[1]);
		if (trk->is_route) {
			this->routes_iters.insert({{ i->first, new_iter }});
		} else {
			this->tracks_iters.insert({{ i->first, new_iter }});
		}

		if (!trk->visible) {
			vik_treeview_item_set_visible((VikTreeview *) pass_along[3], (GtkTreeIter *) pass_along[1], false);
		}
	}
}





void LayerTRW::realize_waypoints(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, void * pass_along[4], int sublayer_id)
{
	for(auto i = waypoints.begin(); i != waypoints.end(); i++) {
		GtkTreeIter *new_iter = (GtkTreeIter *) malloc(sizeof (GtkTreeIter));

		time_t timestamp = 0;
		if (i->second->has_timestamp) {
			timestamp = i->second->timestamp;
		}

		vik_treeview_add_sublayer((VikTreeview *) pass_along[3], (GtkTreeIter *) pass_along[0], (GtkTreeIter *) pass_along[1], i->second->name, pass_along[2], (void *) ((long) i->first), sublayer_id, get_wp_sym_small(i->second->symbol), true, timestamp);

		*new_iter = *((GtkTreeIter *) pass_along[1]);
		this->waypoints_iters.insert({{ i->first, new_iter }});

		if (!i->second->visible) {
			vik_treeview_item_set_visible((VikTreeview *) pass_along[3], (GtkTreeIter *) pass_along[1], false);
		}
	}
}





void LayerTRW::add_sublayer_tracks(VikTreeview * vt, GtkTreeIter * layer_iter)
{
	vik_treeview_add_sublayer(vt, layer_iter, &(track_iter), _("Tracks"), this->vl, NULL, VIK_TRW_LAYER_SUBLAYER_TRACKS, NULL, false, 0);
}





void LayerTRW::add_sublayer_waypoints(VikTreeview * vt, GtkTreeIter * layer_iter)
{
	vik_treeview_add_sublayer(vt, layer_iter, &(waypoint_iter), _("Waypoints"), this->vl, NULL, VIK_TRW_LAYER_SUBLAYER_WAYPOINTS, NULL, false, 0 );
}





void LayerTRW::add_sublayer_routes(VikTreeview * vt, GtkTreeIter * layer_iter)
{
	vik_treeview_add_sublayer(vt, layer_iter, &(route_iter), _("Routes"), this->vl, NULL, VIK_TRW_LAYER_SUBLAYER_ROUTES, NULL, false, 0);
}





static void trw_layer_realize(VikTrwLayer * vtl, VikTreeview * vt, GtkTreeIter * layer_iter)
{
	GtkTreeIter iter2;
	void * pass_along[4] = { &(vtl->trw->track_iter), &iter2, vtl, vt };

	if (vtl->trw->tracks.size() > 0) {
		vtl->trw->add_sublayer_tracks(vt, layer_iter);
		vtl->trw->realize_track(vtl->trw->tracks, pass_along, VIK_TRW_LAYER_SUBLAYER_TRACK);
		vik_treeview_item_set_visible(vt, &(vtl->trw->track_iter), vtl->trw->tracks_visible);
	}

	if (vtl->trw->routes.size() > 0) {
		pass_along[0] = &(vtl->trw->route_iter);

		vtl->trw->add_sublayer_routes(vt, layer_iter);
		vtl->trw->realize_track(vtl->trw->routes, pass_along, VIK_TRW_LAYER_SUBLAYER_ROUTE);
		vik_treeview_item_set_visible(vt, &(vtl->trw->route_iter), vtl->trw->routes_visible);
	}

	if (vtl->trw->waypoints.size() > 0) {
		pass_along[0] = &(vtl->trw->waypoint_iter);

		vtl->trw->add_sublayer_waypoints(vt, layer_iter);
		vtl->trw->realize_waypoints(vtl->trw->waypoints, pass_along, VIK_TRW_LAYER_SUBLAYER_WAYPOINT);
		vik_treeview_item_set_visible(vt, &(vtl->trw->waypoint_iter), vtl->trw->waypoints_visible);
	}

	trw_layer_verify_thumbnails(vtl, NULL);

	vtl->trw->sort_all();
}





static bool trw_layer_sublayer_toggle_visible ( VikTrwLayer *l, int subtype, void * sublayer )
{
  sg_uid_t uid = (sg_uid_t) ((long) sublayer);
  switch ( subtype )
  {
    case VIK_TRW_LAYER_SUBLAYER_TRACKS: return (l->trw->tracks_visible ^= 1);
    case VIK_TRW_LAYER_SUBLAYER_WAYPOINTS: return (l->trw->waypoints_visible ^= 1);
    case VIK_TRW_LAYER_SUBLAYER_ROUTES: return (l->trw->routes_visible ^= 1);
    case VIK_TRW_LAYER_SUBLAYER_TRACK:
    {
      Track * trk = l->trw->tracks.at(uid);
      if (trk)
        return (trk->visible ^= 1);
      else
        return true;
    }
    case VIK_TRW_LAYER_SUBLAYER_WAYPOINT:
    {

      Waypoint * wp = l->trw->waypoints.at(uid);
      if (wp)
        return (wp->visible ^= 1);
      else
        return true;
    }
    case VIK_TRW_LAYER_SUBLAYER_ROUTE:
    {
      Track * trk = l->trw->routes.at(uid);
      if (trk)
        return (trk->visible ^= 1);
      else
        return true;
    }
    default: break;
  }
  return true;
}

/*
 * Return a property about tracks for this layer
 */
int vik_trw_layer_get_property_tracks_line_thickness ( VikTrwLayer *vtl )
{
  return vtl->line_thickness;
}





/*
 * Build up multiple routes information
 */
static void trw_layer_routes_tooltip(std::unordered_map<sg_uid_t, Track *> & tracks, double * length)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		*length = *length + i->second->get_length();
	}
}





// Structure to hold multiple track information for a layer
typedef struct {
  double length;
  time_t  start_time;
  time_t  end_time;
  int    duration;
} tooltip_tracks;

/*
 * Build up layer multiple track information via updating the tooltip_tracks structure
 */
static void trw_layer_tracks_tooltip(std::unordered_map<sg_uid_t, Track *> & tracks, tooltip_tracks * tt)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {

		Track * trk = i->second;

		tt->length = tt->length + trk->get_length();

		// Ensure times are available
		if ( trk->trackpoints && trk->get_tp_first()->has_timestamp ) {
			// Get trkpt only once - as using get_tp_last() iterates whole track each time
			Trackpoint * trkpt_last = trk->get_tp_last();
			if ( trkpt_last->has_timestamp ) {
				time_t t1, t2;
				t1 = trk->get_tp_first()->timestamp;
				t2 = trkpt_last->timestamp;

				// Assume never actually have a track with a time of 0 (1st Jan 1970)
				// Hence initialize to the first 'proper' value
				if ( tt->start_time == 0 ) {
					tt->start_time = t1;
				}
				if ( tt->end_time == 0 ) {
					tt->end_time = t2;
				}

				// Update find the earliest / last times
				if ( t1 < tt->start_time ) {
					tt->start_time = t1;
				}

				if ( t2 > tt->end_time ) {
					tt->end_time = t2;
				}

				// Keep track of total time
				//  there maybe gaps within a track (eg segments)
				//  but this should be generally good enough for a simple indicator
				tt->duration = tt->duration + (int)(t2-t1);
			}
		}
	}
}

/*
 * Generate tooltip text for the layer.
 * This is relatively complicated as it considers information for
 *   no tracks, a single track or multiple tracks
 *     (which may or may not have timing information)
 */
static const char* trw_layer_layer_tooltip ( VikTrwLayer *vtl )
{
  char tbuf1[64];
  char tbuf2[64];
  char tbuf3[64];
  char tbuf4[10];
  tbuf1[0] = '\0';
  tbuf2[0] = '\0';
  tbuf3[0] = '\0';
  tbuf4[0] = '\0';

  static char tmp_buf[128];
  tmp_buf[0] = '\0';

  // For compact date format I'm using '%x'     [The preferred date representation for the current locale without the time.]

  if (true) { //vtl->trw->tracks) {
    tooltip_tracks tt = { 0.0, 0, 0, 0 };
    trw_layer_tracks_tooltip(vtl->trw->tracks, &tt);

    GDate* gdate_start = g_date_new ();
    g_date_set_time_t (gdate_start, tt.start_time);

    GDate* gdate_end = g_date_new ();
    g_date_set_time_t (gdate_end, tt.end_time);

    if ( g_date_compare (gdate_start, gdate_end) ) {
      // Dates differ so print range on separate line
      g_date_strftime (tbuf1, sizeof(tbuf1), "%x", gdate_start);
      g_date_strftime (tbuf2, sizeof(tbuf2), "%x", gdate_end);
      snprintf(tbuf3, sizeof(tbuf3), "%s to %s\n", tbuf1, tbuf2);
    }
    else {
      // Same date so just show it and keep rest of text on the same line - provided it's a valid time!
      if ( tt.start_time != 0 )
	g_date_strftime (tbuf3, sizeof(tbuf3), "%x: ", gdate_start);
    }

    tbuf2[0] = '\0';
    if ( tt.length > 0.0 ) {
      double len_in_units;

      // Setup info dependent on distance units
      switch ( a_vik_get_units_distance() ) {
      case VIK_UNITS_DISTANCE_MILES:
        snprintf(tbuf4, sizeof(tbuf4), "miles");
        len_in_units = VIK_METERS_TO_MILES(tt.length);
        break;
      case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
        snprintf(tbuf4, sizeof(tbuf4), "NM");
        len_in_units = VIK_METERS_TO_NAUTICAL_MILES(tt.length);
        break;
      default:
        snprintf(tbuf4, sizeof(tbuf4), "kms");
        len_in_units = tt.length/1000.0;
        break;
      }

      // Timing information if available
      tbuf1[0] = '\0';
      if ( tt.duration > 0 ) {
        snprintf(tbuf1, sizeof(tbuf1),
                    _(" in %d:%02d hrs:mins"),
                    (int)(tt.duration/3600), (int)round(tt.duration/60.0)%60);
      }
      snprintf(tbuf2, sizeof(tbuf2),
		  _("\n%sTotal Length %.1f %s%s"),
		  tbuf3, len_in_units, tbuf4, tbuf1);
    }

    tbuf1[0] = '\0';
    double rlength = 0.0;
    trw_layer_routes_tooltip(vtl->trw->routes, &rlength);
    if ( rlength > 0.0 ) {
      double len_in_units;
      // Setup info dependent on distance units
      switch ( a_vik_get_units_distance() ) {
      case VIK_UNITS_DISTANCE_MILES:
        snprintf(tbuf4, sizeof(tbuf4), "miles");
        len_in_units = VIK_METERS_TO_MILES(rlength);
        break;
      case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
        snprintf(tbuf4, sizeof(tbuf4), "NM");
        len_in_units = VIK_METERS_TO_NAUTICAL_MILES(rlength);
        break;
      default:
        snprintf(tbuf4, sizeof(tbuf4), "kms");
        len_in_units = rlength/1000.0;
        break;
      }
      snprintf(tbuf1, sizeof(tbuf1), _("\nTotal route length %.1f %s"), len_in_units, tbuf4);
    }

    // Put together all the elements to form compact tooltip text
    snprintf(tmp_buf, sizeof(tmp_buf),
	     _("Tracks: %d - Waypoints: %d - Routes: %d%s%s"),
	     vtl->trw->tracks.size(), vtl->trw->waypoints.size(), vtl->trw->routes.size(), tbuf2, tbuf1);

    g_date_free (gdate_start);
    g_date_free (gdate_end);
  }

  return tmp_buf;
}

static const char* trw_layer_sublayer_tooltip ( VikTrwLayer *l, int subtype, void * sublayer )
{
  switch ( subtype )
  {
    case VIK_TRW_LAYER_SUBLAYER_TRACKS:
    {
      // Very simple tooltip - may expand detail in the future...
      static char tmp_buf[32];
      snprintf(tmp_buf, sizeof(tmp_buf), _("Tracks: %d"), l->trw->tracks.size());
      return tmp_buf;
    }
    break;
    case VIK_TRW_LAYER_SUBLAYER_ROUTES:
    {
      // Very simple tooltip - may expand detail in the future...
      static char tmp_buf[32];
      snprintf(tmp_buf, sizeof(tmp_buf), _("Routes: %d"), l->trw->routes.size());
      return tmp_buf;
    }
    break;

    case VIK_TRW_LAYER_SUBLAYER_ROUTE:
      // Same tooltip for a route
    case VIK_TRW_LAYER_SUBLAYER_TRACK:
    {
      Track * trk = NULL;
      sg_uid_t uid = (sg_uid_t) ((long) sublayer);
      if (subtype == VIK_TRW_LAYER_SUBLAYER_TRACK) {
	      trk = l->trw->tracks.at(uid);
      } else {
	      trk = l->trw->routes.at(uid);
      }

      if (trk) {
	// Could be a better way of handling strings - but this works...
	char time_buf1[20];
	char time_buf2[20];
	time_buf1[0] = '\0';
	time_buf2[0] = '\0';
	static char tmp_buf[100];
	// Compact info: Short date eg (11/20/99), duration and length
	// Hopefully these are the things that are most useful and so promoted into the tooltip
	if ( trk->trackpoints && trk->get_tp_first()->has_timestamp ) {
	  // %x     The preferred date representation for the current locale without the time.
	  strftime (time_buf1, sizeof(time_buf1), "%x: ", gmtime(&(trk->get_tp_first()->timestamp)));
	  time_t dur = trk->get_duration(true);
	  if ( dur > 0 )
	    snprintf( time_buf2, sizeof(time_buf2), _("- %d:%02d hrs:mins"), (int)(dur/3600), (int)round(dur/60.0)%60 );
	}
	// Get length and consider the appropriate distance units
	double tr_len = trk->get_length();
	vik_units_distance_t dist_units = a_vik_get_units_distance ();
	switch (dist_units) {
	case VIK_UNITS_DISTANCE_KILOMETRES:
	  snprintf(tmp_buf, sizeof(tmp_buf), _("%s%.1f km %s"), time_buf1, tr_len/1000.0, time_buf2);
	  break;
	case VIK_UNITS_DISTANCE_MILES:
	  snprintf(tmp_buf, sizeof(tmp_buf), _("%s%.1f miles %s"), time_buf1, VIK_METERS_TO_MILES(tr_len), time_buf2);
	  break;
	case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
	  snprintf(tmp_buf, sizeof(tmp_buf), _("%s%.1f NM %s"), time_buf1, VIK_METERS_TO_NAUTICAL_MILES(tr_len), time_buf2);
	  break;
	default:
	  break;
	}
	return tmp_buf;
      }
    }
    break;
    case VIK_TRW_LAYER_SUBLAYER_WAYPOINTS:
    {
      // Very simple tooltip - may expand detail in the future...
      static char tmp_buf[32];
      snprintf(tmp_buf, sizeof(tmp_buf), _("Waypoints: %d"), l->trw->waypoints.size());
      return tmp_buf;
    }
    break;
    case VIK_TRW_LAYER_SUBLAYER_WAYPOINT:
    {
      sg_uid_t wp_uid = (sg_uid_t) ((long) sublayer);
      Waypoint * wp = l->trw->waypoints.at(wp_uid);
      // NB It's OK to return NULL
      if (wp) {
        if ( wp->comment )
          return wp->comment;
        else
          return wp->description;
      }
    }
    break;
    default: break;
  }
  return NULL;
}

#define VIK_SETTINGS_TRKPT_SELECTED_STATUSBAR_FORMAT "trkpt_selected_statusbar_format"

/**
 * set_statusbar_msg_info_trkpt:
 *
 * Function to show track point information on the statusbar
 *  Items displayed is controlled by the settings format code
 */
void LayerTRW::set_statusbar_msg_info_trkpt(Trackpoint * tp)
{
	char * statusbar_format_code = NULL;
	bool need2free = false;
	Trackpoint * tp_prev = NULL;
	if (!a_settings_get_string(VIK_SETTINGS_TRKPT_SELECTED_STATUSBAR_FORMAT, &statusbar_format_code)) {
		// Otherwise use default
		statusbar_format_code =strdup("KEATDN");
		need2free = true;
	} else {
		// Format code may want to show speed - so may need previous trkpt to work it out
		tp_prev = current_tp_track->get_tp_prev(tp);
	}

	char * msg = vu_trackpoint_formatted_message(statusbar_format_code, tp, tp_prev, current_tp_track, NAN);
	vik_statusbar_set_message(vik_window_get_statusbar(VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(this->vl))), VIK_STATUSBAR_INFO, msg);
	free(msg);

	if (need2free) {
		free(statusbar_format_code);
	}
}





/*
 * Function to show basic waypoint information on the statusbar
 */
void LayerTRW::set_statusbar_msg_info_wpt(Waypoint * wp)
{
	char tmp_buf1[64];
	switch (a_vik_get_units_height()) {
	case VIK_UNITS_HEIGHT_FEET:
		snprintf(tmp_buf1, sizeof(tmp_buf1), _("Wpt: Alt %dft"), (int) round(VIK_METERS_TO_FEET(wp->altitude)));
		break;
	default:
		//VIK_UNITS_HEIGHT_METRES:
		snprintf(tmp_buf1, sizeof(tmp_buf1), _("Wpt: Alt %dm"), (int) round(wp->altitude));
	}

	// Position part
	// Position is put last, as this bit is most likely not to be seen if the display is not big enough,
	//   one can easily use the current pointer position to see this if needed
	char *lat = NULL, *lon = NULL;
	static struct LatLon ll;
	vik_coord_to_latlon(&(wp->coord), &ll);
	a_coords_latlon_to_string(&ll, &lat, &lon);

	// Combine parts to make overall message
	char * msg;
	if (wp->comment) {
		// Add comment if available
		msg = g_strdup_printf(_("%s | %s %s | Comment: %s"), tmp_buf1, lat, lon, wp->comment);
	} else {
		msg = g_strdup_printf(_("%s | %s %s"), tmp_buf1, lat, lon);
	}
	vik_statusbar_set_message(vik_window_get_statusbar(VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(this->vl))), VIK_STATUSBAR_INFO, msg);
	free(lat);
	free(lon);
	free(msg);
}

/**
 * General layer selection function, find out which bit is selected and take appropriate action
 */
static bool trw_layer_selected ( VikTrwLayer *l, int subtype, void * sublayer, int type, void * vlp )
{
  // Reset
  l->trw->current_wp    = NULL;
  l->trw->current_wp_uid = 0;
  trw_layer_cancel_current_tp ( l, false );

  // Clear statusbar
  vik_statusbar_set_message ( vik_window_get_statusbar (VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(l))), VIK_STATUSBAR_INFO, "" );

  switch ( type )
    {
    case VIK_TREEVIEW_TYPE_LAYER:
      {
	vik_window_set_selected_trw_layer ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(l), l );
	/* Mark for redraw */
	return true;
      }
      break;

    case VIK_TREEVIEW_TYPE_SUBLAYER:
      {
	switch ( subtype )
	  {
	  case VIK_TRW_LAYER_SUBLAYER_TRACKS:
	    {
	      vik_window_set_selected_tracks ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(l), &l->trw->tracks, l );
	      /* Mark for redraw */
	      return true;
	    }
	    break;
	  case VIK_TRW_LAYER_SUBLAYER_TRACK:
	    {
	      sg_uid_t uid = (sg_uid_t) ((long) sublayer);
	      Track * trk = l->trw->tracks.at(uid);
	      vik_window_set_selected_track ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(l), (void **) trk, l );
	      /* Mark for redraw */
	      return true;
	    }
	    break;
	  case VIK_TRW_LAYER_SUBLAYER_ROUTES:
	    {
	      vik_window_set_selected_tracks ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(l), &l->trw->routes, l );
	      /* Mark for redraw */
	      return true;
	    }
	    break;
	  case VIK_TRW_LAYER_SUBLAYER_ROUTE:
	    {
	      sg_uid_t uid = (sg_uid_t) ((long) sublayer);
	      Track * trk = l->trw->routes.at(uid);
	      vik_window_set_selected_track ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(l), (void **) trk, l );
	      /* Mark for redraw */
	      return true;
	    }
	    break;
	  case VIK_TRW_LAYER_SUBLAYER_WAYPOINTS:
	    {
	      vik_window_set_selected_waypoints ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(l), &l->trw->waypoints, l );
	      /* Mark for redraw */
	      return true;
	    }
	    break;
	  case VIK_TRW_LAYER_SUBLAYER_WAYPOINT:
	    {
	      sg_uid_t wp_uid = (sg_uid_t) ((long) sublayer);
	      Waypoint * wp = l->trw->waypoints.at(wp_uid);
              if (wp) {
                vik_window_set_selected_waypoint ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(l), (void **) wp, l );
                // Show some waypoint info
                l->trw->set_statusbar_msg_info_wpt(wp);
                /* Mark for redraw */
                return true;
              }
	    }
	    break;
	  default:
	    {
	      return vik_window_clear_highlight ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(l) );
	    }
	    break;
	  }
	return false;
      }
      break;

    default:
      return vik_window_clear_highlight ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(l) );
      break;
    }
}

std::unordered_map<sg_uid_t, Track *> & LayerTRW::get_tracks()
{
	return tracks;
}

std::unordered_map<sg_uid_t, Track *> & LayerTRW::get_routes()
{
	return routes;
}

std::unordered_map<sg_uid_t, Waypoint *> & LayerTRW::get_waypoints()
{
	return waypoints;
}

std::unordered_map<sg_uid_t, TreeIndex *> & LayerTRW::get_tracks_iters()
{
	return tracks_iters;
}

std::unordered_map<sg_uid_t, TreeIndex *> & LayerTRW::get_routes_iters()
{
	return routes_iters;
}

std::unordered_map<sg_uid_t, TreeIndex *> & LayerTRW::get_waypoints_iters()
{
	return waypoints_iters;
}

bool LayerTRW::is_empty()
{
	return ! (tracks.size() || routes.size() || waypoints.size());
}

bool LayerTRW::get_tracks_visibility()
{
	return tracks_visible;
}

bool LayerTRW::get_routes_visibility()
{
	return routes_visible;
}

bool LayerTRW::get_waypoints_visibility()
{
	return waypoints_visible;
}





/*
 * Get waypoint by name - not guaranteed to be unique
 * Finds the first one
 */
Waypoint * LayerTRW::get_waypoint(const char * name)
{
	return LayerTRW::find_waypoint_by_name(waypoints, name);
}





/*
 * Get track by name - not guaranteed to be unique
 * Finds the first one
 */
Track * LayerTRW::get_track(const char * name)
{
	return LayerTRW::find_track_by_name(tracks, name);
}





/*
 * Get route by name - not guaranteed to be unique
 * Finds the first one
 */
Track * LayerTRW::get_route(const char * name)
{
	return LayerTRW::find_track_by_name(routes, name);
}





/* kamilTODO: move this to class Track. */
void LayerTRW::find_maxmin_in_track(const Track * trk, struct LatLon maxmin[2])
{
	if (trk->bbox.north > maxmin[0].lat || maxmin[0].lat == 0.0) {
		maxmin[0].lat = trk->bbox.north;
	}

	if (trk->bbox.south < maxmin[1].lat || maxmin[1].lat == 0.0) {
		maxmin[1].lat = trk->bbox.south;
	}

	if (trk->bbox.east > maxmin[0].lon || maxmin[0].lon == 0.0) {
		maxmin[0].lon = trk->bbox.east;
	}

	if (trk->bbox.west < maxmin[1].lon || maxmin[1].lon == 0.0) {
		maxmin[1].lon = trk->bbox.west;
	}
}





void LayerTRW::find_maxmin(struct LatLon maxmin[2])
{
	VikTrwLayer * vtl = (VikTrwLayer *) this->vl;

	// Continually reuse maxmin to find the latest maximum and minimum values
	// First set to waypoints bounds
	maxmin[0].lat = vtl->waypoints_bbox.north;
	maxmin[1].lat = vtl->waypoints_bbox.south;
	maxmin[0].lon = vtl->waypoints_bbox.east;
	maxmin[1].lon = vtl->waypoints_bbox.west;

	LayerTRW::find_maxmin_in_tracks(tracks, maxmin);
	LayerTRW::find_maxmin_in_tracks(routes, maxmin);
}





bool LayerTRW::find_center(VikCoord * dest)
{
	/* TODO: what if there's only one waypoint @ 0,0, it will think nothing found. like I don't have more important things to worry about... */
	struct LatLon maxmin[2] = { {0.0,0.0}, {0.0,0.0} };
	this->find_maxmin(maxmin);
	if (maxmin[0].lat == 0.0 && maxmin[0].lon == 0.0 && maxmin[1].lat == 0.0 && maxmin[1].lon == 0.0) {
		return false;
	} else {
		struct LatLon average = { (maxmin[0].lat+maxmin[1].lat)/2, (maxmin[0].lon+maxmin[1].lon)/2 };
		vik_coord_load_from_latlon(dest, this->coord_mode, &average);
		return true;
	}
}





static void trw_layer_centerize ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikCoord coord;
  if (vtl->trw->find_center(&coord))
	  goto_coord ( (void **) values[MA_VLP], NULL, NULL, &coord );
  else
    a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("This layer has no waypoints or trackpoints.") );
}





void LayerTRW::zoom_to_show_latlons(Viewport * viewport, struct LatLon maxmin[2])
{
	vu_zoom_to_show_latlons(coord_mode, (VikViewport *) viewport->vvp, maxmin);
}





bool LayerTRW::auto_set_view(Viewport * viewport)
{
	/* TODO: what if there's only one waypoint @ 0,0, it will think nothing found. */
	struct LatLon maxmin[2] = { {0.0,0.0}, {0.0,0.0} };
	this->find_maxmin(maxmin);
	if (maxmin[0].lat == 0.0 && maxmin[0].lon == 0.0 && maxmin[1].lat == 0.0 && maxmin[1].lon == 0.0) {
		return false;
	} else {
		this->zoom_to_show_latlons(viewport, maxmin);
		return true;
	}
}





static void trw_layer_auto_view ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(values[MA_VLP]);
  if (vtl->trw->auto_set_view(vlp->panel_ref->get_viewport())) {
    vik_layers_panel_emit_update ( vlp->panel_ref );
  }
  else
    a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("This layer has no waypoints or trackpoints.") );
}

static void trw_layer_export_gpspoint ( menu_array_layer values )
{
  char *auto_save_name = append_file_ext ( vik_layer_get_name(VIK_LAYER(values[MA_VTL])), FILE_TYPE_GPSPOINT );

  vik_trw_layer_export ( VIK_TRW_LAYER (values[MA_VTL]), _("Export Layer"), auto_save_name, NULL, FILE_TYPE_GPSPOINT );

  free( auto_save_name );
}

static void trw_layer_export_gpsmapper ( menu_array_layer values )
{
  char *auto_save_name = append_file_ext ( vik_layer_get_name(VIK_LAYER(values[MA_VTL])), FILE_TYPE_GPSMAPPER );

  vik_trw_layer_export ( VIK_TRW_LAYER (values[MA_VTL]), _("Export Layer"), auto_save_name, NULL, FILE_TYPE_GPSMAPPER );

  free( auto_save_name );
}

static void trw_layer_export_gpx ( menu_array_layer values )
{
  char *auto_save_name = append_file_ext ( vik_layer_get_name(VIK_LAYER(values[MA_VTL])), FILE_TYPE_GPX );

  vik_trw_layer_export ( VIK_TRW_LAYER (values[MA_VTL]), _("Export Layer"), auto_save_name, NULL, FILE_TYPE_GPX );

  free( auto_save_name );
}

static void trw_layer_export_kml ( menu_array_layer values )
{
  char *auto_save_name = append_file_ext ( vik_layer_get_name(VIK_LAYER(values[MA_VTL])), FILE_TYPE_KML );

  vik_trw_layer_export ( VIK_TRW_LAYER (values[MA_VTL]), _("Export Layer"), auto_save_name, NULL, FILE_TYPE_KML );

  free( auto_save_name );
}

static void trw_layer_export_geojson ( menu_array_layer values )
{
  char *auto_save_name = append_file_ext ( vik_layer_get_name(VIK_LAYER(values[MA_VTL])), FILE_TYPE_GEOJSON );

  vik_trw_layer_export ( VIK_TRW_LAYER (values[MA_VTL]), _("Export Layer"), auto_save_name, NULL, FILE_TYPE_GEOJSON );

  free( auto_save_name );
}

static void trw_layer_export_babel ( void * layer_and_vlp[2] )
{
  const char *auto_save_name = vik_layer_get_name(VIK_LAYER(layer_and_vlp[0]));
  vik_trw_layer_export_gpsbabel ( VIK_TRW_LAYER (layer_and_vlp[0]), _("Export Layer"), auto_save_name );
}

static void trw_layer_export_external_gpx_1 ( menu_array_layer values )
{
  vik_trw_layer_export_external_gpx ( VIK_TRW_LAYER (values[MA_VTL]), a_vik_get_external_gpx_program_1() );
}

static void trw_layer_export_external_gpx_2 ( menu_array_layer values )
{
  vik_trw_layer_export_external_gpx ( VIK_TRW_LAYER (values[MA_VTL]), a_vik_get_external_gpx_program_2() );
}

static void trw_layer_export_gpx_track ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if ( !trk || !trk->name )
    return;

  char *auto_save_name = append_file_ext ( trk->name, FILE_TYPE_GPX );

  char *label = NULL;
  if ( KPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    label = _("Export Route as GPX");
  else
    label = _("Export Track as GPX");
  vik_trw_layer_export ( VIK_TRW_LAYER (values[MA_VTL]), label, auto_save_name, trk, FILE_TYPE_GPX );

  free( auto_save_name );
}





static void trw_layer_goto_wp ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(values[MA_VLP]);
  GtkWidget *dia = gtk_dialog_new_with_buttons (_("Find"),
                                                 VIK_GTK_WINDOW_FROM_LAYER(vtl),
						(GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                                 GTK_STOCK_CANCEL,
                                                 GTK_RESPONSE_REJECT,
                                                 GTK_STOCK_OK,
                                                 GTK_RESPONSE_ACCEPT,
                                                 NULL);

  GtkWidget *label, *entry;
  label = gtk_label_new(_("Waypoint Name:"));
  entry = gtk_entry_new();

  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), label, false, false, 0);
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), entry, false, false, 0);
  gtk_widget_show_all ( dia );
  // 'ok' when press return in the entry
  g_signal_connect_swapped ( entry, "activate", G_CALLBACK(a_dialog_response_accept), dia );
  gtk_dialog_set_default_response ( GTK_DIALOG(dia), GTK_RESPONSE_ACCEPT );

  while ( gtk_dialog_run ( GTK_DIALOG(dia) ) == GTK_RESPONSE_ACCEPT )
  {
    char *name = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
    // Find *first* wp with the given name
    Waypoint * wp = vtl->trw->get_waypoint((const char *) name);

    if ( !wp )
      a_dialog_error_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("Waypoint not found in this layer.") );
    else
    {
      vlp->panel_ref->get_viewport()->set_center_coord(&(wp->coord), true );
      vlp->panel_ref->emit_update();

      // Find and select on the side panel
      sg_uid_t wp_uid = LayerTRW::find_uid_of_waypoint(vtl->trw->waypoints, wp);
      if (wp_uid) {
        GtkTreeIter * it = vtl->trw->waypoints_iters.at(wp_uid);
        vik_treeview_select_iter ( VIK_LAYER(vtl)->vt, it, true );
      }

      break;
    }

    free( name );

  }
  gtk_widget_destroy ( dia );
}





bool LayerTRW::new_waypoint(GtkWindow * w, const VikCoord * def_coord)
{
	char * default_name = this->highest_wp_number_get();
	Waypoint * wp = new Waypoint();
	char * returned_name = NULL;
	bool updated;
	wp->coord = *def_coord; /* kamilFIXME: what? */

	// Attempt to auto set height if DEM data is available
	wp->apply_dem_data(true);

	returned_name = a_dialog_waypoint(w, default_name, (VikTrwLayer *) this->vl, wp, this->coord_mode, true, &updated);

	if (returned_name) {
		wp->visible = true;
		this->add_waypoint(wp, returned_name);
		free(default_name);
		free(returned_name);
		return true;
	}
	free(default_name);
	delete wp;
	return false;
}





static void trw_layer_new_wikipedia_wp_viewport ( menu_array_layer values )
{
  struct LatLon maxmin[2] = { {0.0,0.0}, {0.0,0.0} };
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(values[MA_VLP]);
  VikWindow *vw = (VikWindow *)(VIK_GTK_WINDOW_FROM_LAYER(vtl));
  VikViewport *vvp =  vik_window_viewport(vw);

  // Note the order is max part first then min part - thus reverse order of use in min_max function:
  vvp->port.get_min_max_lat_lon(&maxmin[1].lat, &maxmin[0].lat, &maxmin[1].lon, &maxmin[0].lon );
  a_geonames_wikipedia_box((VikWindow *)(VIK_GTK_WINDOW_FROM_LAYER(vtl)), vtl, maxmin);
  vtl->trw->calculate_bounds_waypoints();
  vik_layers_panel_emit_update ( vlp->panel_ref );
}

static void trw_layer_new_wikipedia_wp_layer ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(values[MA_VLP]);
  struct LatLon maxmin[2] = { {0.0,0.0}, {0.0,0.0} };

  vtl->trw->find_maxmin(maxmin);
  a_geonames_wikipedia_box((VikWindow *)(VIK_GTK_WINDOW_FROM_LAYER(vtl)), vtl, maxmin);
  vtl->trw->calculate_bounds_waypoints();
  vik_layers_panel_emit_update ( vlp->panel_ref );
}

#ifdef VIK_CONFIG_GEOTAG
static void trw_layer_geotagging_waypoint_mtime_keep ( menu_array_sublayer values )
{
  sg_uid_t wp_uid = (sg_uid_t) ((long) values[MA_SUBLAYER_ID]);
  Waypoint * wp = VIK_TRW_LAYER(values[MA_VTL])->trw->waypoints.at(wp_uid);
  if ( wp )
    // Update directly - not changing the mtime
    a_geotag_write_exif_gps ( wp->image, wp->coord, wp->altitude, true );
}

static void trw_layer_geotagging_waypoint_mtime_update ( menu_array_sublayer values )
{
  sg_uid_t wp_uid = (sg_uid_t) ((long) values[MA_SUBLAYER_ID]);
  Waypoint * wp = VIK_TRW_LAYER(values[MA_VTL])->trw->waypoints.at(wp_uid);
  if ( wp )
    // Update directly
    a_geotag_write_exif_gps ( wp->image, wp->coord, wp->altitude, false );
}

/*
 * Use code in separate file for this feature as reasonably complex
 */
static void trw_layer_geotagging_track ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  sg_uid_t uid = (sg_uid_t) ((long) values[MA_SUBLAYER_ID]);
  Track * trk = vtl->trw->tracks.at(uid);
  // Unset so can be reverified later if necessary
  vtl->has_verified_thumbnails = false;

  trw_layer_geotag_dialog ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                            vtl,
                            NULL,
                            trk);
}

static void trw_layer_geotagging_waypoint ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  sg_uid_t wp_uid = (sg_uid_t) ((long) values[MA_SUBLAYER_ID]);
  Waypoint * wp = vtl->trw->waypoints.at(wp_uid);

  trw_layer_geotag_dialog ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                            vtl,
                            wp,
                            NULL );
}

static void trw_layer_geotagging ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  // Unset so can be reverified later if necessary
  vtl->has_verified_thumbnails = false;

  trw_layer_geotag_dialog ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                            vtl,
                            NULL,
                            NULL );
}
#endif

// 'Acquires' - Same as in File Menu -> Acquire - applies into the selected TRW Layer //

static void trw_layer_acquire ( menu_array_layer values, VikDataSourceInterface *datasource )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(values[MA_VLP]);
  VikWindow *vw = (VikWindow *)(VIK_GTK_WINDOW_FROM_LAYER(vtl));
  VikViewport *vvp =  vik_window_viewport(vw);

  vik_datasource_mode_t mode = datasource->mode;
  if ( mode == VIK_DATASOURCE_AUTO_LAYER_MANAGEMENT )
    mode = VIK_DATASOURCE_ADDTOLAYER;
  a_acquire ( vw, vlp, vvp, mode, datasource, NULL, NULL );
}

/*
 * Acquire into this TRW Layer straight from GPS Device
 */
static void trw_layer_acquire_gps_cb ( menu_array_layer values )
{
  trw_layer_acquire ( values, &vik_datasource_gps_interface );
}

/*
 * Acquire into this TRW Layer from Directions
 */
static void trw_layer_acquire_routing_cb ( menu_array_layer values )
{
  trw_layer_acquire ( values, &vik_datasource_routing_interface );
}

/*
 * Acquire into this TRW Layer from an entered URL
 */
static void trw_layer_acquire_url_cb ( menu_array_layer values )
{
  trw_layer_acquire ( values, &vik_datasource_url_interface );
}

#ifdef VIK_CONFIG_OPENSTREETMAP
/*
 * Acquire into this TRW Layer from OSM
 */
static void trw_layer_acquire_osm_cb ( menu_array_layer values )
{
  trw_layer_acquire ( values, &vik_datasource_osm_interface );
}

/**
 * Acquire into this TRW Layer from OSM for 'My' Traces
 */
static void trw_layer_acquire_osm_my_traces_cb ( menu_array_layer values )
{
  trw_layer_acquire ( values, &vik_datasource_osm_my_traces_interface );
}
#endif

#ifdef VIK_CONFIG_GEOCACHES
/*
 * Acquire into this TRW Layer from Geocaching.com
 */
static void trw_layer_acquire_geocache_cb ( menu_array_layer values )
{
  trw_layer_acquire ( values, &vik_datasource_gc_interface );
}
#endif

#ifdef VIK_CONFIG_GEOTAG
/*
 * Acquire into this TRW Layer from images
 */
static void trw_layer_acquire_geotagged_cb ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);

  trw_layer_acquire ( values, &vik_datasource_geotag_interface );

  // Reverify thumbnails as they may have changed
  vtl->has_verified_thumbnails = false;
  trw_layer_verify_thumbnails ( vtl, NULL );
}
#endif

/*
 * Acquire into this TRW Layer from any GPS Babel supported file
 */
static void trw_layer_acquire_file_cb ( menu_array_layer values )
{
  trw_layer_acquire ( values, &vik_datasource_file_interface );
}

static void trw_layer_gps_upload ( menu_array_layer values )
{
  menu_array_sublayer data;
  int ii;
  for ( ii = MA_VTL; ii < MA_LAST; ii++ )
    data[ii] = NULL;
  data[MA_VTL] = values[MA_VTL];
  data[MA_VLP] = values[MA_VLP];

  trw_layer_gps_upload_any ( data );
}

/**
 * If pass_along[3] is defined that this will upload just that track
 */
static void trw_layer_gps_upload_any ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(values[MA_VLP]);
  void * sublayer = values[MA_SUBLAYER_ID];
  sg_uid_t uid = (sg_uid_t) ((long) sublayer);

  // May not actually get a track here as values[2&3] can be null
  Track * trk = NULL;
  vik_gps_xfer_type xfer_type = TRK; // VIK_TRW_LAYER_SUBLAYER_TRACKS = 0 so hard to test different from NULL!
  bool xfer_all = false;

  if ( values[MA_SUBTYPE] ) {
    xfer_all = false;
    if ( KPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE ) {
      trk = vtl->trw->routes.at(uid);
      xfer_type = RTE;
    }
    else if ( KPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
      trk = vtl->trw->tracks.at(uid);
      xfer_type = TRK;
    }
    else if ( KPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_WAYPOINTS ) {
      xfer_type = WPT;
    }
    else if ( KPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTES ) {
      xfer_type = RTE;
    }
  }
  else if ( !values[MA_CONFIRM] )
    xfer_all = true; // i.e. whole layer

  if (trk && !trk->visible) {
    a_dialog_error_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("Can not upload invisible track.") );
    return;
  }

  GtkWidget *dialog = gtk_dialog_new_with_buttons ( _("GPS Upload"),
                                                    VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    GTK_STOCK_OK,
                                                    GTK_RESPONSE_ACCEPT,
                                                    GTK_STOCK_CANCEL,
                                                    GTK_RESPONSE_REJECT,
                                                    NULL );

  gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );
  GtkWidget *response_w = NULL;
#if GTK_CHECK_VERSION (2, 20, 0)
  response_w = gtk_dialog_get_widget_for_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );
#endif

  if ( response_w )
    gtk_widget_grab_focus ( response_w );

  void * dgs = datasource_gps_setup ( dialog, xfer_type, xfer_all );

  if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) != GTK_RESPONSE_ACCEPT ) {
    datasource_gps_clean_up ( dgs );
    gtk_widget_destroy ( dialog );
    return;
  }

  // Get info from reused datasource dialog widgets
  char* protocol = datasource_gps_get_protocol ( dgs );
  char* port = datasource_gps_get_descriptor ( dgs );
  // NB don't free the above strings as they're references to values held elsewhere
  bool do_tracks = datasource_gps_get_do_tracks ( dgs );
  bool do_routes = datasource_gps_get_do_routes ( dgs );
  bool do_waypoints = datasource_gps_get_do_waypoints ( dgs );
  bool turn_off = datasource_gps_get_off ( dgs );

  gtk_widget_destroy ( dialog );

  // When called from the viewport - work the corresponding layerspanel:
  if ( !vlp ) {
    vlp = vik_window_layers_panel ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)) );
  }

  // Apply settings to transfer to the GPS device
  vik_gps_comm ( vtl,
                 trk,
                 GPS_UP,
                 protocol,
                 port,
                 false,
                 (VikViewport *) vlp->panel_ref->get_viewport()->vvp,
                 vlp,
                 do_tracks,
                 do_routes,
                 do_waypoints,
                 turn_off );
}

static void trw_layer_new_wp ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(values[MA_VLP]);
  /* TODO longone: okay, if layer above (aggregate) is invisible but vtl->visible is true, this redraws for no reason.
     instead return true if you want to update. */
  if (vtl->trw->new_waypoint(VIK_GTK_WINDOW_FROM_LAYER(vtl), vlp->panel_ref->get_viewport()->get_center()) ) {
    vtl->trw->calculate_bounds_waypoints();
    if ( VIK_LAYER(vtl)->visible )
      vik_layers_panel_emit_update ( vlp->panel_ref );
  }
}





void LayerTRW::new_track_create_common(char * name)
{
	VikTrwLayer * vtl = (VikTrwLayer *) this->vl;

	vtl->current_track = new Track();
	vtl->current_track->set_defaults();
	vtl->current_track->visible = true;

	if (vtl->drawmode == DRAWMODE_ALL_SAME_COLOR) {
		// Create track with the preferred colour from the layer properties
		vtl->current_track->color = vtl->track_color;
	} else {
		gdk_color_parse( "#000000", &(vtl->current_track->color));
	}

	vtl->current_track->has_color = true;
	this->add_track(vtl->current_track, name);
}





static void trw_layer_new_track ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);

  if ( ! vtl->current_track ) {
    char *name = vtl->trw->new_unique_sublayer_name(VIK_TRW_LAYER_SUBLAYER_TRACK, _("Track")) ;
    vtl->trw->new_track_create_common(name);
    free( name );

    vik_window_enable_layer_tool ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)), VIK_LAYER_TRW, TOOL_CREATE_TRACK );
  }
}





void LayerTRW::new_route_create_common(char * name)
{
	VikTrwLayer * vtl = (VikTrwLayer *) this->vl;

	vtl->current_track = new Track();
	vtl->current_track->set_defaults();
	vtl->current_track->visible = true;
	vtl->current_track->is_route = true;
	// By default make all routes red
	vtl->current_track->has_color = true;
	gdk_color_parse ( "red", &vtl->current_track->color );
	this->add_route(vtl->current_track, name);
}





static void trw_layer_new_route ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);

  if ( ! vtl->current_track ) {
    char *name = vtl->trw->new_unique_sublayer_name(VIK_TRW_LAYER_SUBLAYER_ROUTE, _("Route")) ;
    vtl->trw->new_route_create_common(name);
    free( name );
    vik_window_enable_layer_tool ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)), VIK_LAYER_TRW, TOOL_CREATE_ROUTE );
  }
}

static void trw_layer_auto_routes_view ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(values[MA_VLP]);

  if (vtl->trw->routes.size() > 0 ) {
    struct LatLon maxmin[2] = { {0,0}, {0,0} };
    LayerTRW::find_maxmin_in_tracks(vtl->trw->routes, maxmin);
    vtl->trw->zoom_to_show_latlons(vlp->panel_ref->get_viewport(), maxmin);
    vik_layers_panel_emit_update ( vlp->panel_ref );
  }
}


static void trw_layer_finish_track ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  vtl->current_track = NULL;
  vtl->route_finder_started = false;
  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

static void trw_layer_auto_tracks_view ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(values[MA_VLP]);

  if (vtl->trw->tracks.size() > 0) {
    struct LatLon maxmin[2] = { {0,0}, {0,0} };
    LayerTRW::find_maxmin_in_tracks(vtl->trw->tracks, maxmin);
    vtl->trw->zoom_to_show_latlons(vlp->panel_ref->get_viewport(), maxmin);
    vik_layers_panel_emit_update ( vlp->panel_ref );
  }
}





static void trw_layer_auto_waypoints_view ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(values[MA_VLP]);

  /* Only 1 waypoint - jump straight to it */
  if (vtl->trw->waypoints.size() == 1) {
    Viewport * viewport = vlp->panel_ref->get_viewport();
    LayerTRW::single_waypoint_jump(vtl->trw->waypoints, viewport);
  }
  /* If at least 2 waypoints - find center and then zoom to fit */
  else if (vtl->trw->waypoints.size() > 1)
  {
    struct LatLon maxmin[2] = { {0,0}, {0,0} };
    maxmin[0].lat = vtl->waypoints_bbox.north;
    maxmin[1].lat = vtl->waypoints_bbox.south;
    maxmin[0].lon = vtl->waypoints_bbox.east;
    maxmin[1].lon = vtl->waypoints_bbox.west;
    vtl->trw->zoom_to_show_latlons(vlp->panel_ref->get_viewport(), maxmin);
  }

  vik_layers_panel_emit_update ( vlp->panel_ref );
}

void trw_layer_osm_traces_upload_cb ( menu_array_layer values )
{
  osm_traces_upload_viktrwlayer(VIK_TRW_LAYER(values[MA_VTL]), NULL);
}

void trw_layer_osm_traces_upload_track_cb ( menu_array_sublayer values )
{
  if ( values[MA_MISC] ) {
    Track * trk = ((Track *) values[MA_MISC]);
    osm_traces_upload_viktrwlayer(VIK_TRW_LAYER(values[MA_VTL]), trk);
  }
}

static GtkWidget* create_external_submenu ( GtkMenu *menu )
{
  GtkWidget *external_submenu = gtk_menu_new ();
  GtkWidget *item = gtk_image_menu_item_new_with_mnemonic ( _("Externa_l") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_EXECUTE, GTK_ICON_SIZE_MENU) );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), external_submenu );
  return external_submenu;
}

static void trw_layer_add_menu_items ( VikTrwLayer *vtl, GtkMenu *menu, void * vlp )
{
  static menu_array_layer pass_along;
  GtkWidget *item;
  GtkWidget *export_submenu;
  pass_along[MA_VTL] = vtl;
  pass_along[MA_VLP] = vlp;

  item = gtk_menu_item_new();
  gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
  gtk_widget_show ( item );

  if ( vtl->current_track ) {
    if ( vtl->current_track->is_route )
      item = gtk_menu_item_new_with_mnemonic ( _("_Finish Route") );
    else
      item = gtk_menu_item_new_with_mnemonic ( _("_Finish Track") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_finish_track), pass_along );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );

    // Add separator
    item = gtk_menu_item_new ();
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );
  }

  /* Now with icons */
  item = gtk_image_menu_item_new_with_mnemonic ( _("_View Layer") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_auto_view), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );

  GtkWidget *view_submenu = gtk_menu_new();
  item = gtk_image_menu_item_new_with_mnemonic ( _("V_iew") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_FIND, GTK_ICON_SIZE_MENU) );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), view_submenu );

  item = gtk_menu_item_new_with_mnemonic ( _("View All _Tracks") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_auto_tracks_view), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (view_submenu), item);
  gtk_widget_show ( item );

  item = gtk_menu_item_new_with_mnemonic ( _("View All _Routes") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_auto_routes_view), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (view_submenu), item);
  gtk_widget_show ( item );

  item = gtk_menu_item_new_with_mnemonic ( _("View All _Waypoints") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_auto_waypoints_view), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (view_submenu), item);
  gtk_widget_show ( item );

  item = gtk_image_menu_item_new_with_mnemonic ( _("_Goto Center of Layer") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_centerize), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );

  item = gtk_menu_item_new_with_mnemonic ( _("Goto _Waypoint...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_wp), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );

  export_submenu = gtk_menu_new ();
  item = gtk_image_menu_item_new_with_mnemonic ( _("_Export Layer") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_HARDDISK, GTK_ICON_SIZE_MENU) );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), export_submenu );

  item = gtk_menu_item_new_with_mnemonic ( _("Export as GPS_Point...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_export_gpspoint), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (export_submenu), item);
  gtk_widget_show ( item );

  item = gtk_menu_item_new_with_mnemonic ( _("Export as GPS_Mapper...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_export_gpsmapper), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (export_submenu), item);
  gtk_widget_show ( item );

  item = gtk_menu_item_new_with_mnemonic ( _("Export as _GPX...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_export_gpx), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (export_submenu), item);
  gtk_widget_show ( item );

  item = gtk_menu_item_new_with_mnemonic ( _("Export as _KML...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_export_kml), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (export_submenu), item);
  gtk_widget_show ( item );

  if ( have_geojson_export ) {
    item = gtk_menu_item_new_with_mnemonic ( _("Export as GEO_JSON...") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_export_geojson), pass_along );
    gtk_menu_shell_append (GTK_MENU_SHELL (export_submenu), item);
    gtk_widget_show ( item );
  }

  item = gtk_menu_item_new_with_mnemonic ( _("Export via GPSbabel...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_export_babel), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (export_submenu), item);
  gtk_widget_show ( item );

  char* external1 = g_strdup_printf ( _("Open with External Program_1: %s"), a_vik_get_external_gpx_program_1() );
  item = gtk_menu_item_new_with_mnemonic ( external1 );
  free( external1 );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_export_external_gpx_1), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (export_submenu), item);
  gtk_widget_show ( item );

  char* external2 = g_strdup_printf ( _("Open with External Program_2: %s"), a_vik_get_external_gpx_program_2() );
  item = gtk_menu_item_new_with_mnemonic ( external2 );
  free( external2 );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_export_external_gpx_2), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (export_submenu), item);
  gtk_widget_show ( item );

  GtkWidget *new_submenu = gtk_menu_new();
  item = gtk_image_menu_item_new_with_mnemonic ( _("_New") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU) );
  gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
  gtk_widget_show(item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), new_submenu);

  item = gtk_image_menu_item_new_with_mnemonic ( _("New _Waypoint...") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_new_wp), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (new_submenu), item);
  gtk_widget_show ( item );

  item = gtk_image_menu_item_new_with_mnemonic ( _("New _Track") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_new_track), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (new_submenu), item);
  gtk_widget_show ( item );
  // Make it available only when a new track *not* already in progress
  gtk_widget_set_sensitive ( item, ! (bool)KPOINTER_TO_INT(vtl->current_track) );

  item = gtk_image_menu_item_new_with_mnemonic ( _("New _Route") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_new_route), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (new_submenu), item);
  gtk_widget_show ( item );
  // Make it available only when a new track *not* already in progress
  gtk_widget_set_sensitive ( item, ! (bool)KPOINTER_TO_INT(vtl->current_track) );

#ifdef VIK_CONFIG_GEOTAG
  item = gtk_menu_item_new_with_mnemonic ( _("Geotag _Images...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_geotagging), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );
#endif

  GtkWidget *acquire_submenu = gtk_menu_new ();
  item = gtk_image_menu_item_new_with_mnemonic ( _("_Acquire") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GO_DOWN, GTK_ICON_SIZE_MENU) );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), acquire_submenu );

  item = gtk_menu_item_new_with_mnemonic ( _("From _GPS...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_acquire_gps_cb), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (acquire_submenu), item);
  gtk_widget_show ( item );

  /* FIXME: only add menu when at least a routing engine has support for Directions */
  item = gtk_menu_item_new_with_mnemonic ( _("From _Directions...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_acquire_routing_cb), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (acquire_submenu), item);
  gtk_widget_show ( item );

#ifdef VIK_CONFIG_OPENSTREETMAP
  item = gtk_menu_item_new_with_mnemonic ( _("From _OSM Traces...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_acquire_osm_cb), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (acquire_submenu), item);
  gtk_widget_show ( item );

  item = gtk_menu_item_new_with_mnemonic ( _("From _My OSM Traces...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_acquire_osm_my_traces_cb), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (acquire_submenu), item);
  gtk_widget_show ( item );
#endif

  item = gtk_menu_item_new_with_mnemonic ( _("From _URL...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_acquire_url_cb), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (acquire_submenu), item);
  gtk_widget_show ( item );

#ifdef VIK_CONFIG_GEONAMES
  GtkWidget *wikipedia_submenu = gtk_menu_new();
  item = gtk_image_menu_item_new_with_mnemonic ( _("From _Wikipedia Waypoints") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU) );
  gtk_menu_shell_append(GTK_MENU_SHELL (acquire_submenu), item);
  gtk_widget_show(item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), wikipedia_submenu);

  item = gtk_image_menu_item_new_with_mnemonic ( _("Within _Layer Bounds") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_new_wikipedia_wp_layer), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (wikipedia_submenu), item);
  gtk_widget_show ( item );

  item = gtk_image_menu_item_new_with_mnemonic ( _("Within _Current View") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ZOOM_100, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_new_wikipedia_wp_viewport), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (wikipedia_submenu), item);
  gtk_widget_show ( item );
#endif

#ifdef VIK_CONFIG_GEOCACHES
  item = gtk_menu_item_new_with_mnemonic ( _("From Geo_caching...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_acquire_geocache_cb), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (acquire_submenu), item);
  gtk_widget_show ( item );
#endif

#ifdef VIK_CONFIG_GEOTAG
  item = gtk_menu_item_new_with_mnemonic ( _("From Geotagged _Images...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_acquire_geotagged_cb), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (acquire_submenu), item);
  gtk_widget_show ( item );
#endif

  item = gtk_menu_item_new_with_mnemonic ( _("From _File...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_acquire_file_cb), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (acquire_submenu), item);
  gtk_widget_set_tooltip_text (item, _("Import File With GPS_Babel..."));
  gtk_widget_show ( item );

  vik_ext_tool_datasources_add_menu_items_to_menu ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)), GTK_MENU (acquire_submenu) );

  GtkWidget *upload_submenu = gtk_menu_new ();
  item = gtk_image_menu_item_new_with_mnemonic ( _("_Upload") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GO_UP, GTK_ICON_SIZE_MENU) );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), upload_submenu );

  item = gtk_image_menu_item_new_with_mnemonic ( _("Upload to _GPS...") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GO_FORWARD, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_gps_upload), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (upload_submenu), item);
  gtk_widget_show ( item );

#ifdef VIK_CONFIG_OPENSTREETMAP
  item = gtk_image_menu_item_new_with_mnemonic ( _("Upload to _OSM...") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GO_UP, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_osm_traces_upload_cb), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (upload_submenu), item);
  gtk_widget_show ( item );
#endif

  GtkWidget *delete_submenu = gtk_menu_new ();
  item = gtk_image_menu_item_new_with_mnemonic ( _("De_lete") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), delete_submenu );

  item = gtk_image_menu_item_new_with_mnemonic ( _("Delete All _Tracks") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_all_tracks), pass_along );
  gtk_menu_shell_append ( GTK_MENU_SHELL(delete_submenu), item );
  gtk_widget_show ( item );

  item = gtk_image_menu_item_new_with_mnemonic ( _("Delete Tracks _From Selection...") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_tracks_from_selection), pass_along );
  gtk_menu_shell_append ( GTK_MENU_SHELL(delete_submenu), item );
  gtk_widget_show ( item );

  item = gtk_image_menu_item_new_with_mnemonic ( _("Delete _All Routes") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_all_routes), pass_along );
  gtk_menu_shell_append ( GTK_MENU_SHELL(delete_submenu), item );
  gtk_widget_show ( item );

  item = gtk_image_menu_item_new_with_mnemonic ( _("_Delete Routes From Selection...") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_routes_from_selection), pass_along );
  gtk_menu_shell_append ( GTK_MENU_SHELL(delete_submenu), item );
  gtk_widget_show ( item );

  item = gtk_image_menu_item_new_with_mnemonic ( _("Delete All _Waypoints") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_all_waypoints), pass_along );
  gtk_menu_shell_append ( GTK_MENU_SHELL(delete_submenu), item );
  gtk_widget_show ( item );

  item = gtk_image_menu_item_new_with_mnemonic ( _("Delete Waypoints From _Selection...") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_waypoints_from_selection), pass_along );
  gtk_menu_shell_append ( GTK_MENU_SHELL(delete_submenu), item );
  gtk_widget_show ( item );

  VikLayersPanel * vlp_ = VIK_LAYERS_PANEL(vlp);
  item = a_acquire_trwlayer_menu ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)), (VikLayersPanel *) vlp,
				   (VikViewport *) vlp_->panel_ref->get_viewport()->vvp, vtl );
  if ( item ) {
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
  }

  item = a_acquire_trwlayer_track_menu ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)), (VikLayersPanel *) vlp,
					 (VikViewport *) vlp_->panel_ref->get_viewport()->vvp, vtl );
  if ( item ) {
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
  }

  item = gtk_image_menu_item_new_with_mnemonic ( _("Track _List...") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_track_list_dialog), pass_along );
  gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
  gtk_widget_show ( item );
  gtk_widget_set_sensitive ( item, (bool)(vtl->trw->tracks.size() + vtl->trw->routes.size()) );

  item = gtk_image_menu_item_new_with_mnemonic ( _("_Waypoint List...") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_waypoint_list_dialog), pass_along );
  gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
  gtk_widget_show ( item );
  gtk_widget_set_sensitive ( item, (bool) (vtl->trw->waypoints.size()) );

  GtkWidget *external_submenu = create_external_submenu ( menu );
  // TODO: Should use selected layer's centre - rather than implicitly using the current viewport
  vik_ext_tools_add_menu_items_to_menu ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)), GTK_MENU (external_submenu), NULL );
}

// Fake Waypoint UUIDs vith simple increasing integer
static sg_uid_t global_wp_uid = 0;

void LayerTRW::add_waypoint(Waypoint * wp, char const * name)
{
	VikLayer * parent = VIK_LAYER(this->vl);

	global_wp_uid++;

	wp->set_name(name);

	if (parent->realized) {
		// Do we need to create the sublayer:
		if (waypoints.size() == 0) {
			((VikTrwLayer *) this->vl)->trw->add_sublayer_waypoints(parent->vt, &(parent->iter));
		}

		GtkTreeIter *iter = (GtkTreeIter *) malloc(sizeof(GtkTreeIter));

		time_t timestamp = 0;
		if (wp->has_timestamp) {
			timestamp = wp->timestamp;
		}

		// Visibility column always needed for waypoints
		vik_treeview_add_sublayer(parent->vt, &(waypoint_iter), iter, name, this->vl, KUINT_TO_POINTER(global_wp_uid), VIK_TRW_LAYER_SUBLAYER_WAYPOINT, get_wp_sym_small (wp->symbol), true, timestamp);

		// Actual setting of visibility dependent on the waypoint
		vik_treeview_item_set_visible(parent->vt, iter, wp->visible);

		waypoints_iters.insert({{ global_wp_uid, iter }});

		// Sort now as post_read is not called on a realized waypoint
		vik_treeview_sort_children(parent->vt, &(waypoint_iter), ((VikTrwLayer *) this->vl)->wp_sort_order);
	}

	this->highest_wp_number_add_wp(name);
	waypoints.insert({{ global_wp_uid, wp }});
}





// Fake Track UUIDs vi simple increasing integer
static unsigned int global_tr_uuid = 0;

void LayerTRW::add_track(Track * trk, char const * name)
{
	VikLayer * parent = VIK_LAYER(this->vl);

	global_tr_uuid++;

	trk->set_name(name);

	if (parent->realized) {
		// Do we need to create the sublayer:
		if (tracks.size() == 0) {
			((VikTrwLayer *) this->vl)->trw->add_sublayer_tracks(parent->vt, &(parent->iter));
		}

		GtkTreeIter *iter = (GtkTreeIter *) malloc(sizeof(GtkTreeIter));

		time_t timestamp = 0;
		Trackpoint * tp = trk->get_tp_first();
		if (tp && tp->has_timestamp) {
			timestamp = tp->timestamp;
		}

		// Visibility column always needed for tracks
		vik_treeview_add_sublayer(parent->vt, &(track_iter), iter, name, this->vl, KUINT_TO_POINTER(global_tr_uuid), VIK_TRW_LAYER_SUBLAYER_TRACK, NULL, true, timestamp);

		// Actual setting of visibility dependent on the track
		vik_treeview_item_set_visible(parent->vt, iter, trk->visible);

		tracks_iters.insert({{ global_tr_uuid, iter }});

		// Sort now as post_read is not called on a realized track
		vik_treeview_sort_children(parent->vt, &(track_iter), ((VikTrwLayer *) this->vl)->track_sort_order);
	}

	tracks.insert({{ global_tr_uuid, trk }});

	this->update_treeview(trk);
}





// Fake Route UUIDs vi simple increasing integer
static unsigned int global_rt_uuid = 0;

void LayerTRW::add_route(Track * trk, char const * name)
{
	VikLayer * parent = VIK_LAYER(this->vl);

	global_rt_uuid++;

	trk->set_name(name);

	if (parent->realized) {
		// Do we need to create the sublayer:
		if (routes.size() == 0) {
			((VikTrwLayer *) this->vl)->trw->add_sublayer_routes(parent->vt, &(parent->iter));
		}

		GtkTreeIter *iter = (GtkTreeIter *) malloc(sizeof(GtkTreeIter));
		// Visibility column always needed for routes
		vik_treeview_add_sublayer(parent->vt, &(route_iter), iter, name, this->vl, KUINT_TO_POINTER(global_rt_uuid), VIK_TRW_LAYER_SUBLAYER_ROUTE, NULL, true, 0); // Routes don't have times
		// Actual setting of visibility dependent on the route
		vik_treeview_item_set_visible(parent->vt, iter, trk->visible);

		routes_iters.insert({{ global_rt_uuid, iter }});

		// Sort now as post_read is not called on a realized route
		vik_treeview_sort_children(parent->vt, &(route_iter), ((VikTrwLayer *) this->vl)->track_sort_order);
	}

	routes.insert({{ global_rt_uuid, trk }});

	this->update_treeview(trk);
}





/* to be called whenever a track has been deleted or may have been changed. */
void LayerTRW::cancel_tps_of_track(Track * trk)
{
	VikTrwLayer * vtl = (VikTrwLayer *) this->vl;

	if (this->current_tp_track == trk) {
		trw_layer_cancel_current_tp(vtl, false);
	}
}





/**
 * Normally this is done to due the waypoint size preference having changed
 */
void LayerTRW::reset_waypoints()
{
	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {
		Waypoint * wp = i->second;
		if (wp->symbol) {
			// Reapply symbol setting to update the pixbuf
			char * tmp_symbol = g_strdup(wp->symbol);
			wp->set_symbol(tmp_symbol);
			free(tmp_symbol);
		}
	}
}





/**
 * trw_layer_new_unique_sublayer_name:
 *
 * Allocates a unique new name
 */
char * LayerTRW::new_unique_sublayer_name(int sublayer_type, const char * name)
{
	int i = 2;
	char * newname = g_strdup(name);

	void * id = NULL;
	do {
		id = NULL;
		switch (sublayer_type) {
		case VIK_TRW_LAYER_SUBLAYER_TRACK:
			id = (void *) this->get_track((const char *) newname);
			break;
		case VIK_TRW_LAYER_SUBLAYER_WAYPOINT:
			id = (void *) this->get_waypoint((const char *) newname);
			break;
		default:
			id = (void *) this->get_route((const char *) newname);
			break;
		}
		// If found a name already in use try adding 1 to it and we try again
		if (id) {
			char * new_newname = g_strdup_printf("%s#%d", name, i);
			free(newname);
			newname = new_newname;
			i++;
		}
	} while (id != NULL);

	return newname;
}





void LayerTRW::filein_add_waypoint(char * name, Waypoint * wp)
{
	// No more uniqueness of name forced when loading from a file
	// This now makes this function a little redunant as we just flow the parameters through
	this->add_waypoint(wp, name);
}





void LayerTRW::filein_add_track(char * name, Track * trk)
{
	VikTrwLayer * vtl = (VikTrwLayer *) this->vl;

	if (vtl->route_finder_append && vtl->current_track) {
		trk->remove_dup_points(); /* make "double point" track work to undo */

		// enforce end of current track equal to start of tr
		Trackpoint * cur_end = vtl->current_track->get_tp_last();
		Trackpoint * new_start = trk->get_tp_first();
		if (cur_end && new_start) {
			if (!vik_coord_equals(&cur_end->coord, &new_start->coord)) {
				vtl->current_track->add_trackpoint(new Trackpoint(*cur_end), false);
			}
		}

		vtl->current_track->steal_and_append_trackpoints(trk);
		trk->free();
		vtl->route_finder_append = false; /* this means we have added it */
	} else {

		// No more uniqueness of name forced when loading from a file
		if (trk->is_route) {
			this->add_route(trk, name);
		} else {
			this->add_track(trk, name);
		}

		if (vtl->route_finder_check_added_track) {
			trk->remove_dup_points(); /* make "double point" track work to undo */
			vtl->route_finder_added_track = trk;
		}
	}
}





#if 0
static void trw_layer_enum_item ( void * id, GList **tr, GList **l )
{
  *l = g_list_append(*l, id);
}
#endif





/*
 * Move an item from one TRW layer to another TRW layer
 */
void LayerTRW::move_item(VikTrwLayer * vtl_dest, void * id, int type)
{
	VikTrwLayer * vtl_src = (VikTrwLayer *) this->vl;
	// When an item is moved the name is checked to see if it clashes with an existing name
	//  in the destination layer and if so then it is allocated a new name

	// TODO reconsider strategy when moving within layer (if anything...)
	if (vtl_src == vtl_dest) {
		return;
	}

	sg_uid_t uid = (sg_uid_t) ((long) id);
	if (type == VIK_TRW_LAYER_SUBLAYER_TRACK) {
		Track * trk = this->tracks.at(uid);

		char * newname = vtl_dest->trw->new_unique_sublayer_name(type, trk->name);

		Track * trk2 = new Track(*trk, true);
		vtl_dest->trw->add_track(trk2, newname);
		free(newname);
		this->delete_track(trk);
		// Reset layer timestamps in case they have now changed
		vik_treeview_item_set_timestamp(vtl_dest->vl.vt, &vtl_dest->vl.iter, trw_layer_get_timestamp(vtl_dest));
		vik_treeview_item_set_timestamp(vtl_src->vl.vt, &vtl_src->vl.iter, trw_layer_get_timestamp(vtl_src));
	}

	if (type == VIK_TRW_LAYER_SUBLAYER_ROUTE) {
		Track * trk = this->routes.at(uid);

		char * newname = vtl_dest->trw->new_unique_sublayer_name(type, trk->name);

		Track * trk2 = new Track (*trk, true);
		vtl_dest->trw->add_route(trk2, newname);
		free( newname );
		this->delete_route(trk);
	}

	if (type == VIK_TRW_LAYER_SUBLAYER_WAYPOINT) {
		Waypoint * wp = this->waypoints.at(uid);

		char *newname = vtl_dest->trw->new_unique_sublayer_name(type, wp->name);

		Waypoint * wp2 = new Waypoint(*wp);
		vtl_dest->trw->add_waypoint(wp2, newname);
		free(newname);
		this->delete_waypoint(wp);

		// Recalculate bounds even if not renamed as maybe dragged between layers
		vtl_dest->trw->calculate_bounds_waypoints();
		vtl_src->trw->calculate_bounds_waypoints();
		// Reset layer timestamps in case they have now changed
		vik_treeview_item_set_timestamp(vtl_dest->vl.vt, &vtl_dest->vl.iter, trw_layer_get_timestamp(vtl_dest));
		vik_treeview_item_set_timestamp(vtl_src->vl.vt, &vtl_src->vl.iter, trw_layer_get_timestamp(vtl_src));
	}
}





static void trw_layer_drag_drop_request ( VikTrwLayer *vtl_src, VikTrwLayer *vtl_dest, GtkTreeIter *src_item_iter, GtkTreePath *dest_path )
{
  VikTreeview *vt = VIK_LAYER(vtl_src)->vt;
  int type = vik_treeview_item_get_data(vt, src_item_iter);

  if (!vik_treeview_item_get_pointer(vt, src_item_iter)) {
    GList *items = NULL;
    GList *iter;

    if (type==VIK_TRW_LAYER_SUBLAYER_TRACKS) {
      LayerTRW::list_trk_uids(vtl_src->trw->tracks, &items);
    }
    if (type==VIK_TRW_LAYER_SUBLAYER_WAYPOINTS) {
      LayerTRW::list_wp_uids(vtl_src->trw->waypoints, &items);
    }
    if (type==VIK_TRW_LAYER_SUBLAYER_ROUTES) {
      LayerTRW::list_trk_uids(vtl_src->trw->routes, &items);
    }

    iter = items;
    while (iter) {
      if (type==VIK_TRW_LAYER_SUBLAYER_TRACKS) {
	vtl_src->trw->move_item(vtl_dest, iter->data, VIK_TRW_LAYER_SUBLAYER_TRACK);
      } else if (type==VIK_TRW_LAYER_SUBLAYER_ROUTES) {
	vtl_src->trw->move_item(vtl_dest, iter->data, VIK_TRW_LAYER_SUBLAYER_ROUTE);
      } else {
	vtl_src->trw->move_item(vtl_dest, iter->data, VIK_TRW_LAYER_SUBLAYER_WAYPOINT);
      }
      iter = iter->next;
    }
    if (items)
      g_list_free(items);
  } else {
    char *name = (char *) vik_treeview_item_get_pointer(vt, src_item_iter);
    vtl_src->trw->move_item(vtl_dest, name, type);
  }
}





bool LayerTRW::delete_track(Track * trk)
{
	bool was_visible = false;

	/* kamilTODO: why check for trk->name here? */
	if (trk && trk->name) {

		if (trk == ((VikTrwLayer *) this->vl)->current_track) {
			((VikTrwLayer *) this->vl)->current_track = NULL;
			current_tp_track = NULL;
			current_tp_uid = 0;
			moving_tp = false;
			((VikTrwLayer *) this->vl)->route_finder_started = false;
		}

		was_visible = trk->visible;

		if (trk == ((VikTrwLayer *) this->vl)->route_finder_added_track) {
			((VikTrwLayer *) this->vl)->route_finder_added_track = NULL;
		}

		sg_uid_t uid = LayerTRW::find_uid_of_track(tracks, trk);
		if (uid) {
			/* could be current_tp, so we have to check */
			this->cancel_tps_of_track(trk);

			TreeIndex * it = tracks_iters.at(uid);
			if (it) {
				vik_treeview_item_delete(VIK_LAYER(this->vl)->vt, it);
				tracks_iters.erase(uid);
				tracks.erase(uid); /* kamilTODO: should this line be inside of "if (it)"? */

				// If last sublayer, then remove sublayer container
				if (tracks.size() == 0) {
					vik_treeview_item_delete(VIK_LAYER(this->vl)->vt, &(track_iter));
				}
			}
			// In case it was selected (no item delete signal ATM)
			vik_window_clear_highlight(VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(this->vl)));
		}
	}
	return was_visible;
}





bool LayerTRW::delete_route(Track * trk)
{
	bool was_visible = false;


	/* kamilTODO: why check for trk->name here? */
	if (trk && trk->name) {

		if (trk == ((VikTrwLayer *) this->vl)->current_track) {
			((VikTrwLayer *) this->vl)->current_track = NULL;
			current_tp_track = NULL;
			current_tp_uid = 0;
			moving_tp = false;
		}

		was_visible = trk->visible;

		if (trk == ((VikTrwLayer *) this->vl)->route_finder_added_track) {
			((VikTrwLayer *) this->vl)->route_finder_added_track = NULL;
		}

		// Hmmm, want key of it
		sg_uid_t uid = LayerTRW::find_uid_of_track(routes, trk);
		if (uid) {
			/* could be current_tp, so we have to check */
			this->cancel_tps_of_track(trk);

			GtkTreeIter * it = routes_iters.at(uid);

			if (it) {
				vik_treeview_item_delete(VIK_LAYER(this->vl)->vt, it);
				routes_iters.erase(uid);
				routes.erase(uid); /* kamilTODO: should this line be inside of "if (it)"? */

				// If last sublayer, then remove sublayer container
				if (routes.size() == 0) {
					vik_treeview_item_delete(VIK_LAYER(this->vl)->vt, &(route_iter));
				}
			}
			// Incase it was selected (no item delete signal ATM)
			vik_window_clear_highlight(VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(this->vl)));
		}
	}
	return was_visible;
}





bool LayerTRW::delete_waypoint(Waypoint * wp)
{
	bool was_visible = false;

	/* kamilTODO: why check for trk->name here? */
	if (wp && wp->name) {

		if (wp == current_wp) {
			current_wp = NULL;
			current_wp_uid = 0;
			moving_wp = false;
		}

		was_visible = wp->visible;

		sg_uid_t uid = LayerTRW::find_uid_of_waypoint(waypoints, wp);
		if (uid) {
			TreeIndex * it = waypoints_iters.at(uid);

			if (it) {
				vik_treeview_item_delete(VIK_LAYER(this->vl)->vt, it);
				waypoints_iters.erase(uid);

				this->highest_wp_number_remove_wp(wp->name);

				/* kamilTODO: should this line be inside of "if (it)"? */
				waypoints.erase(uid); // last because this frees the name

				// If last sublayer, then remove sublayer container
				if (waypoints.size() == 0) {
					vik_treeview_item_delete(VIK_LAYER(this->vl)->vt, &(waypoint_iter));
				}
			}
			// Incase it was selected (no item delete signal ATM)
			vik_window_clear_highlight(VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(this->vl)));
		}
	}

	return was_visible;
}





/*
 * Delete a waypoint by the given name
 * NOTE: ATM this will delete the first encountered Waypoint with the specified name
 *   as there be multiple waypoints with the same name
 */
bool LayerTRW::delete_waypoint_by_name(char const * name)
{
	// Currently only the name is used in this waypoint find function
	sg_uid_t uid = LayerTRW::find_uid_of_waypoint_by_name(waypoints, name);
	if (uid) {
		return delete_waypoint(waypoints.at(uid));
	} else {
		return false;
	}
}





/*
 * Delete a track by the given name
 * NOTE: ATM this will delete the first encountered Track with the specified name
 *   as there may be multiple tracks with the same name within the specified hash table
 */
bool LayerTRW::delete_track_by_name(const char * name, bool is_route)
{
	if (is_route) {
		Track * trk = LayerTRW::find_track_by_name(routes, name);
		if (trk) {
			return delete_route(trk);
		}
	} else {
		Track * trk = LayerTRW::find_track_by_name(tracks, name);
		if (trk) {
			return delete_track(trk);
		}
	}

	return false;
}





void LayerTRW::delete_all_routes()
{
	VikTrwLayer * vtl = (VikTrwLayer *) this->vl;

	vtl->current_track = NULL;
	vtl->route_finder_added_track = NULL;
	if (this->current_tp_track) {
		trw_layer_cancel_current_tp(vtl, false);
	}

	remove_item_from_treeview(this->routes_iters, VIK_LAYER(vtl)->vt);
	this->routes_iters.clear(); /* kamilTODO: call destructors of route iters. */
	this->routes.clear(); /* kamilTODO: call destructors of routes. */

	vik_treeview_item_delete(VIK_LAYER(vtl)->vt, &(this->route_iter));

	vik_layer_emit_update(VIK_LAYER(vtl));
}





void LayerTRW::delete_all_tracks()
{
	VikTrwLayer * vtl = (VikTrwLayer *) this->vl;

	vtl->current_track = NULL;
	vtl->route_finder_added_track = NULL;
	if (this->current_tp_track) {
		trw_layer_cancel_current_tp(vtl, false);
	}

	remove_item_from_treeview(this->tracks_iters, VIK_LAYER(vtl)->vt);
	this->tracks_iters.clear();
	this->tracks.clear(); /* kamilTODO: call destructors of tracks. */

	vik_treeview_item_delete(VIK_LAYER(vtl)->vt, &(this->track_iter));

	vik_layer_emit_update(VIK_LAYER(vtl));
}





void LayerTRW::delete_all_waypoints()
{
	VikTrwLayer * vtl = (VikTrwLayer *) this->vl;

	this->current_wp = NULL;
	this->current_wp_uid = 0;
	this->moving_wp = false;

	this->highest_wp_number_reset();

	remove_item_from_treeview(this->waypoints_iters, VIK_LAYER(vtl)->vt);
	this->waypoints_iters.clear();
	this->waypoints.clear(); /* kamilTODO: does this really call destructors of Waypoints? */

	vik_treeview_item_delete(VIK_LAYER(vtl)->vt, &(this->waypoint_iter));

	vik_layer_emit_update(VIK_LAYER(vtl));
}





static void trw_layer_delete_all_tracks ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  // Get confirmation from the user
  if ( a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
			    _("Are you sure you want to delete all tracks in %s?"),
			    vik_layer_get_name ( VIK_LAYER(vtl) ) ) )
     vtl->trw->delete_all_tracks();
}

static void trw_layer_delete_all_routes ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  // Get confirmation from the user
  if ( a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                            _("Are you sure you want to delete all routes in %s?"),
                            vik_layer_get_name ( VIK_LAYER(vtl) ) ) )
    vtl->trw->delete_all_routes();
}

static void trw_layer_delete_all_waypoints ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  // Get confirmation from the user
  if ( a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
			    _("Are you sure you want to delete all waypoints in %s?"),
			    vik_layer_get_name ( VIK_LAYER(vtl) ) ) )
     vtl->trw->delete_all_waypoints();
}

static void trw_layer_delete_item ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  sg_uid_t uid = (sg_uid_t) ((long) values[MA_SUBLAYER_ID]);
  bool was_visible = false;
  if ( KPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_WAYPOINT )
  {
    Waypoint * wp = vtl->trw->waypoints.at(uid);
    if ( wp && wp->name ) {
      if ( KPOINTER_TO_INT (values[MA_CONFIRM]) )
        // Get confirmation from the user
        // Maybe this Waypoint Delete should be optional as is it could get annoying...
        if ( ! a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
            _("Are you sure you want to delete the waypoint \"%s\"?"),
            wp->name ) )
          return;
      was_visible = vtl->trw->delete_waypoint(wp);
      vtl->trw->calculate_bounds_waypoints();
      // Reset layer timestamp in case it has now changed
      vik_treeview_item_set_timestamp ( vtl->vl.vt, &vtl->vl.iter, trw_layer_get_timestamp(vtl) );
    }
  }
  else if ( KPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_TRACK )
  {
    Track * trk = vtl->trw->tracks.at(uid);
    if ( trk && trk->name ) {
      if ( KPOINTER_TO_INT (values[MA_CONFIRM]) )
        // Get confirmation from the user
        if ( ! a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
				  _("Are you sure you want to delete the track \"%s\"?"),
				  trk->name ) )
          return;
      was_visible = vtl->trw->delete_track(trk);
      // Reset layer timestamp in case it has now changed
      vik_treeview_item_set_timestamp ( vtl->vl.vt, &vtl->vl.iter, trw_layer_get_timestamp(vtl) );
    }
  }
  else
  {
    Track * trk = vtl->trw->routes.at(uid);
    if ( trk && trk->name ) {
      if ( KPOINTER_TO_INT (values[MA_CONFIRM]) )
        // Get confirmation from the user
        if ( ! a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                    _("Are you sure you want to delete the route \"%s\"?"),
                                    trk->name ) )
          return;
      was_visible = vtl->trw->delete_route(trk);
    }
  }
  if ( was_visible )
    vik_layer_emit_update ( VIK_LAYER(vtl) );
}





/**
 *  Rename waypoint and maintain corresponding name of waypoint in the treeview
 */
void LayerTRW::waypoint_rename(Waypoint * wp, char const * new_name)
{
	wp->set_name(new_name);

	// Now update the treeview as well
	// Need key of it for treeview update
	sg_uid_t uid = LayerTRW::find_uid_of_waypoint(this->waypoints, wp);
	if (uid) {
		GtkTreeIter * it = this->waypoints_iters.at(uid);
		if (it) {
			VikTrwLayer * vtl = (VikTrwLayer *) this->vl;
			vik_treeview_item_set_name(VIK_LAYER(vtl)->vt, it, new_name);
			vik_treeview_sort_children(VIK_LAYER(vtl)->vt, &(this->waypoint_iter), vtl->wp_sort_order);
		}
	}
}





/**
 *  Maintain icon of waypoint in the treeview
 */
void LayerTRW::waypoint_reset_icon(Waypoint * wp)
{
	// update the treeview
	// Need key of it for treeview update
	sg_uid_t uid = LayerTRW::find_uid_of_waypoint(this->waypoints, wp);
	if (uid) {
		GtkTreeIter * it = this->waypoints_iters.at(uid);
		if (it) {
			VikTrwLayer * vtl = (VikTrwLayer *) this->vl;
			vik_treeview_item_set_icon( VIK_LAYER(vtl)->vt, it, get_wp_sym_small(wp->symbol));
		}
	}
}





static void trw_layer_properties_item ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  if ( KPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_WAYPOINT )
  {
    sg_uid_t wp_uid = (sg_uid_t) ((long) values[MA_SUBLAYER_ID]);
    Waypoint * wp = vtl->trw->waypoints.at(wp_uid);

    if ( wp && wp->name )
    {
      bool updated = false;
      char *new_name = a_dialog_waypoint ( VIK_GTK_WINDOW_FROM_LAYER(vtl), wp->name, vtl, wp, vtl->trw->coord_mode, false, &updated );
      if ( new_name )
	vtl->trw->waypoint_rename(wp, new_name);

      if ( updated && values[MA_TV_ITER] )
        vik_treeview_item_set_icon ( VIK_LAYER(vtl)->vt, (GtkTreeIter *) values[MA_TV_ITER], get_wp_sym_small (wp->symbol) );

      if ( updated && VIK_LAYER(vtl)->visible )
	vik_layer_emit_update ( VIK_LAYER(vtl) );
    }
  }
  else
  {
    Track * trk = trw_layer_get_track_helper(values, vtl);

    if ( trk && trk->name )
    {
      vik_trw_layer_propwin_run ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                  vtl,
                                  trk,
                                  values[MA_VLP],
                                  (VikViewport *) values[MA_VVP],
                                  false );
    }
  }
}

/**
 * trw_layer_track_statistics:
 *
 * Show track statistics.
 * ATM jump to the stats page in the properties
 * TODO: consider separating the stats into an individual dialog?
 */
static void trw_layer_track_statistics ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if ( trk && trk->name ) {
    vik_trw_layer_propwin_run ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                vtl,
                                trk,
                                values[MA_VLP],
                                (VikViewport *) values[MA_VVP],
                                true );
  }
}





/*
 * Update the treeview of the track id - primarily to update the icon
 */
void LayerTRW::update_treeview(Track * trk)
{
	sg_uid_t uid = 0;
	if (trk->is_route) {
		uid = LayerTRW::find_uid_of_track(this->routes, trk);
	} else {
		uid = LayerTRW::find_uid_of_track(this->tracks, trk);
	}

	if (uid) {
		/* kamilFIXME: uid should be a valid key of either routes_iters or tracks_iters, but there is no such key in the maps yet. Check why. */
		fprintf(stderr, "uid = %d, size of tracks_iters = %d, size of routes_iters = %d\n", uid, this->tracks_iters.size(), this->routes_iters.size());
		GtkTreeIter *iter = NULL;
		if (trk->is_route) {
			if (this->routes_iters.size()) {
				iter = this->routes_iters.at(uid);
			}
		} else {
			if (this->tracks_iters.size()) {
				iter = this->tracks_iters.at(uid);
			}
		}

		if (iter) {
			// TODO: Make this a function
			GdkPixbuf * pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, false, 8, 18, 18);
			uint32_t pixel = ((trk->color.red & 0xff00) << 16)
				| ((trk->color.green & 0xff00) << 8)
				| (trk->color.blue & 0xff00);
			gdk_pixbuf_fill(pixbuf, pixel);

			VikTrwLayer * vtl = (VikTrwLayer *) this->vl;
			vik_treeview_item_set_icon(VIK_LAYER(vtl)->vt, iter, pixbuf);
			g_object_unref(pixbuf);
		}
	}
}





/*
   Parameter 1 -> VikLayersPanel
   Parameter 2 -> VikLayer
   Parameter 3 -> VikViewport
*/
static void goto_coord ( void ** vlp, void * vl, void * vvp, const VikCoord *coord )
{
  if ( vlp ) {
	  VikLayersPanel * vlp_ = VIK_LAYERS_PANEL(vlp);
    vlp_->panel_ref->get_viewport()->set_center_coord(coord, true );
    vik_layers_panel_emit_update ( VIK_LAYERS_PANEL(vlp)->panel_ref );
  }
  else {
    /* since vlp not set, vl & vvp should be valid instead! */
    if ( vl && vvp ) {
      VIK_VIEWPORT(vvp)->port.set_center_coord(coord, true );
      vik_layer_emit_update ( VIK_LAYER(vl) );
    }
  }
}

static void trw_layer_goto_track_startpoint ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if ( trk && trk->trackpoints )
	  goto_coord ( (void **) values[MA_VLP], vtl, values[MA_VVP], &(trk->get_tp_first()->coord) );
}

static void trw_layer_goto_track_center ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if ( trk && trk->trackpoints )
  {
    struct LatLon average, maxmin[2] = { {0,0}, {0,0} };
    VikCoord coord;
    LayerTRW::find_maxmin_in_track(trk, maxmin);
    average.lat = (maxmin[0].lat+maxmin[1].lat)/2;
    average.lon = (maxmin[0].lon+maxmin[1].lon)/2;
    vik_coord_load_from_latlon ( &coord, vtl->trw->coord_mode, &average );
    goto_coord ( (void **) values[MA_VLP], vtl, values[MA_VVP], &coord);
  }
}

static void trw_layer_convert_track_route ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if ( !trk )
    return;

  // Converting a track to a route can be a bit more complicated,
  //  so give a chance to change our minds:
  if ( !trk->is_route &&
       ( (trk->get_segment_count() > 1 ) ||
         (trk->get_average_speed() > 0.0 ) ) ) {

    if ( ! a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                _("Converting a track to a route removes extra track data such as segments, timestamps, etc...\nDo you want to continue?"), NULL ) )
      return;
}

  // Copy it
  Track * trk_copy = new Track(*trk, true);

  // Convert
  trk_copy->is_route = !trk_copy->is_route;

  // ATM can't set name to self - so must create temporary copy
  char *name = g_strdup( trk_copy->name );

  // Delete old one and then add new one
  if ( trk->is_route ) {
    vtl->trw->delete_route(trk);
    vtl->trw->add_track(trk_copy, name);
  }
  else {
    // Extra route conversion bits...
    trk_copy->merge_segments();
    trk_copy->to_routepoints();

    vtl->trw->delete_track(trk);
    vtl->trw->add_route(trk_copy, name);
  }
  free( name );

  // Update in case color of track / route changes when moving between sublayers
  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

static void trw_layer_anonymize_times ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if (trk)
    trk->anonymize_times();
}

static void trw_layer_interpolate_times ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if (trk)
    trk->interpolate_times();
}

static void trw_layer_extend_track_end ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if (!trk)
    return;

  vtl->current_track = trk;
  vik_window_enable_layer_tool ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)), VIK_LAYER_TRW, trk->is_route ? TOOL_CREATE_ROUTE : TOOL_CREATE_TRACK);

  if (trk->trackpoints)
	  goto_coord ( (void **) values[MA_VLP], vtl, values[MA_VVP], &(trk->get_tp_last()->coord) );
}

/**
 * extend a track using route finder
 */
static void trw_layer_extend_track_end_route_finder ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  sg_uid_t uid = (sg_uid_t) ((long) values[MA_SUBLAYER_ID]);
  Track * trk = vtl->trw->routes.at(uid);
  if ( !trk )
    return;

  vik_window_enable_layer_tool ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)), VIK_LAYER_TRW, TOOL_ROUTE_FINDER );
  vtl->current_track = trk;
  vtl->route_finder_started = true;

  if (trk->trackpoints)
	  goto_coord ( (void **) values[MA_VLP], vtl, values[MA_VVP], &trk->get_tp_last()->coord );
}





/**
 *
 */
bool LayerTRW::dem_test(VikLayersPanel * vlp)
{
	// If have a vlp then perform a basic test to see if any DEM info available...
	if (vlp) {
		std::list<Layer *> * dems = vlp->panel_ref->get_all_layers_of_type(VIK_LAYER_DEM, true); // Includes hidden DEM layer types
		if (dems->empty()) {
			a_dialog_error_msg(VIK_GTK_WINDOW_FROM_LAYER(this->vl), _("No DEM layers available, thus no DEM values can be applied."));
			return false;
		}
	}
	return true;
}





/**
 * apply_dem_data_common:
 *
 * A common function for applying the DEM values and reporting the results.
 */
void LayerTRW::apply_dem_data_common(VikLayersPanel * vlp, Track * trk, bool skip_existing_elevations)
{
	if (!this->dem_test(vlp)) {
		return;
	}

	unsigned long changed = trk->apply_dem_data(skip_existing_elevations);
	// Inform user how much was changed
	char str[64];
	const char * tmp_str = ngettext("%ld point adjusted", "%ld points adjusted", changed);
	snprintf(str, 64, tmp_str, changed);
	a_dialog_info_msg(VIK_GTK_WINDOW_FROM_LAYER(this->vl), str);
}





static void trw_layer_apply_dem_data_all ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if ( trk )
    vtl->trw->apply_dem_data_common((VikLayersPanel *) values[MA_VLP], trk, false);
}

static void trw_layer_apply_dem_data_only_missing ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if (trk)
    vtl->trw->apply_dem_data_common((VikLayersPanel *) values[MA_VLP], trk, true);
}





/**
 * smooth_it:
 *
 * A common function for applying the elevation smoothing and reporting the results.
 */
void LayerTRW::smooth_it(Track * trk, bool flat)
{
	unsigned long changed = trk->smooth_missing_elevation_data(flat);
	// Inform user how much was changed
	char str[64];
	const char * tmp_str = ngettext("%ld point adjusted", "%ld points adjusted", changed);
	snprintf(str, 64, tmp_str, changed);
	a_dialog_info_msg(VIK_GTK_WINDOW_FROM_LAYER(this->vl), str);
}





/**
 *
 */
static void trw_layer_missing_elevation_data_interp ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if (!trk)
    return;

  vtl->trw->smooth_it(trk, false);
}

static void trw_layer_missing_elevation_data_flat ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if (!trk)
    return;

  vtl->trw->smooth_it(trk, true);
}





/**
 * Commonal helper function
 */
void LayerTRW::wp_changed_message(int changed)
{
	char str[64];
	const char * tmp_str = ngettext("%ld waypoint changed", "%ld waypoints changed", changed);
	snprintf(str, 64, tmp_str, changed);
	a_dialog_info_msg(VIK_GTK_WINDOW_FROM_LAYER(this->vl), str);
}





static void trw_layer_apply_dem_data_wpt_all ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikLayersPanel *vlp = (VikLayersPanel *)values[MA_VLP];

  if (!vtl->trw->dem_test(vlp))
    return;

  int changed = 0;
  if ( KPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_WAYPOINT ) {
    // Single Waypoint
    sg_uid_t wp_uid = (sg_uid_t) ((long) values[MA_SUBLAYER_ID]);
    Waypoint * wp = vtl->trw->waypoints.at(wp_uid);
    if ( wp )
      changed = (int) wp->apply_dem_data(false );
  }
  else {
      // All waypoints
      for (auto i = vtl->trw->waypoints.begin(); i != vtl->trw->waypoints.end(); i++) {
      changed = changed + (int) i->second->apply_dem_data(false);
    }
  }
  vtl->trw->wp_changed_message(changed);
}

static void trw_layer_apply_dem_data_wpt_only_missing ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikLayersPanel *vlp = (VikLayersPanel *)values[MA_VLP];

  if ( !vtl->trw->dem_test(vlp))
    return;

  int changed = 0;
  if ( KPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_WAYPOINT ) {
    // Single Waypoint
    sg_uid_t wp_uid = (sg_uid_t) ((long) values[MA_SUBLAYER_ID]);
    Waypoint * wp = vtl->trw->waypoints.at(wp_uid);
    if ( wp )
      changed = (int) wp->apply_dem_data(true);
  }
  else {
    // All waypoints
    for (auto i = vtl->trw->waypoints.begin(); i != vtl->trw->waypoints.end(); i++) {
      changed = changed + (int) i->second->apply_dem_data(true);
    }
  }
  vtl->trw->wp_changed_message(changed);
}

static void trw_layer_goto_track_endpoint ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if ( !trk )
    return;
  if ( !trk->trackpoints )
    return;
  goto_coord ( (void **) values[MA_VLP], vtl, values[MA_VVP], &(trk->get_tp_last()->coord));
}

static void trw_layer_goto_track_max_speed ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if ( !trk )
    return;

  Trackpoint * vtp = trk->get_tp_by_max_speed();
  if ( !vtp )
    return;
  goto_coord ( (void **) values[MA_VLP], vtl, values[MA_VVP], &(vtp->coord));
}

static void trw_layer_goto_track_max_alt ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if ( !trk )
    return;

  Trackpoint* vtp = trk->get_tp_by_max_alt();
  if ( !vtp )
    return;
  goto_coord ( (void **) values[MA_VLP], vtl, values[MA_VVP], &(vtp->coord));
}

static void trw_layer_goto_track_min_alt ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if ( !trk )
    return;

  Trackpoint* vtp = trk->get_tp_by_min_alt();
  if ( !vtp )
    return;
  goto_coord ( (void **) values[MA_VLP], vtl, values[MA_VVP], &(vtp->coord));
}

/*
 * Automatically change the viewport to center on the track and zoom to see the extent of the track
 */
static void trw_layer_auto_track_view ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if ( trk && trk->trackpoints )
  {
    struct LatLon maxmin[2] = { {0,0}, {0,0} };
    LayerTRW::find_maxmin_in_track(trk, maxmin);
    vtl->trw->zoom_to_show_latlons(&((VikViewport *) values[MA_VVP])->port, maxmin);
    if ( values[MA_VLP] )
      vik_layers_panel_emit_update ( VIK_LAYERS_PANEL(values[MA_VLP])->panel_ref );
    else
      vik_layer_emit_update ( VIK_LAYER(vtl) );
  }
}

/*
 * Refine the selected track/route with a routing engine.
 * The routing engine is selected by the user, when requestiong the job.
 */
static void trw_layer_route_refine ( menu_array_sublayer values )
{
  static int last_engine = 0;
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if ( trk && trk->trackpoints )
  {
    /* Check size of the route */
    int nb = trk->get_tp_count();
    if (nb > 100) {
      GtkWidget *dialog = gtk_message_dialog_new (VIK_GTK_WINDOW_FROM_LAYER (vtl),
                                                  (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                                  GTK_MESSAGE_WARNING,
                                                  GTK_BUTTONS_OK_CANCEL,
                                                  _("Refining a track with many points (%d) is unlikely to yield sensible results. Do you want to Continue?"),
                                                  nb);
      int response = gtk_dialog_run ( GTK_DIALOG(dialog) );
      gtk_widget_destroy ( dialog );
      if (response != GTK_RESPONSE_OK )
        return;
    }
    /* Select engine from dialog */
    GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Refine Route with Routing Engine..."),
                                                  VIK_GTK_WINDOW_FROM_LAYER (vtl),
						     (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                                  GTK_STOCK_CANCEL,
                                                  GTK_RESPONSE_REJECT,
                                                  GTK_STOCK_OK,
                                                  GTK_RESPONSE_ACCEPT,
                                                  NULL);
    GtkWidget *label = gtk_label_new ( _("Select routing engine") );
    gtk_widget_show_all(label);

    gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), label, true, true, 0 );

    GtkWidget * combo = vik_routing_ui_selector_new ( (Predicate)vik_routing_engine_supports_refine, NULL );
    gtk_combo_box_set_active (GTK_COMBO_BOX (combo), last_engine);
    gtk_widget_show_all(combo);

    gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), combo, true, true, 0 );

    gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );

    if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT )
    {
        /* Dialog validated: retrieve selected engine and do the job */
        last_engine = gtk_combo_box_get_active ( GTK_COMBO_BOX(combo) );
        VikRoutingEngine *routing = vik_routing_ui_selector_get_nth (combo, last_engine);

        /* Change cursor */
        vik_window_set_busy_cursor ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)) );

        /* Force saving track */
        /* FIXME: remove or rename this hack */
        vtl->route_finder_check_added_track = true;

        /* the job */
        vik_routing_engine_refine (routing, vtl, trk);

        /* FIXME: remove or rename this hack */
        if ( vtl->route_finder_added_track )
          vtl->route_finder_added_track->calculate_bounds();

        vtl->route_finder_added_track = NULL;
        vtl->route_finder_check_added_track = false;

        vik_layer_emit_update ( VIK_LAYER(vtl) );

        /* Restore cursor */
        vik_window_clear_busy_cursor ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)) );
    }
    gtk_widget_destroy ( dialog );
  }
}

static void trw_layer_edit_trackpoint ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  vtl->trw->tpwin_init();
}





/*************************************
 * merge/split by time routines
 *************************************/

/* called for each key in track hash table.
 * If the current track has the same time stamp type, add it to the result,
 * except the one pointed by "exclude".
 * set exclude to NULL if there is no exclude to check.
 * Note that the result is in reverse (for performance reasons).
 */





/* comparison function used to sort tracks; a and b are hash table keys */
/* Not actively used - can be restored if needed
static int track_compare(gconstpointer a, gconstpointer b, void * user_data)
{
  GHashTable *tracks = user_data;
  time_t t1, t2;

  t1 = ((Trackpoint *) ((Track *) g_hash_table_lookup(tracks, a))->trackpoints->data)->timestamp;
  t2 = ((Trackpoint *) ((Track *) g_hash_table_lookup(tracks, b))->trackpoints->data)->timestamp;

  if (t1 < t2) return -1;
  if (t1 > t2) return 1;
  return 0;
}
*/

/* comparison function used to sort trackpoints */
static int trackpoint_compare(gconstpointer a, gconstpointer b)
{
  time_t t1 = ((Trackpoint *) a)->timestamp, t2 = ((Trackpoint *) b)->timestamp;

  if (t1 < t2) return -1;
  if (t1 > t2) return 1;
  return 0;
}

/**
 * comparison function which can be used to sort tracks or waypoints by name
 */
int sort_alphabetically (gconstpointer a, gconstpointer b, void * user_data)
{
  const char* namea = (const char*) a;
  const char* nameb = (const char*) b;
  if ( namea == NULL || nameb == NULL)
    return 0;
  else
    // Same sort method as used in the vik_treeview_*_alphabetize functions
    return strcmp ( namea, nameb );
}





/**
 * Attempt to merge selected track with other tracks specified by the user
 * Tracks to merge with must be of the same 'type' as the selected track -
 *  either all with timestamps, or all without timestamps
 */
static void trw_layer_merge_with_other ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  sg_uid_t uid = (sg_uid_t) ((long) values[MA_SUBLAYER_ID]);
  std::unordered_map<sg_uid_t, SlavGPS::Track*> * ght_tracks = NULL;
  if ( KPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    ght_tracks = &vtl->trw->routes;
  else
    ght_tracks = &vtl->trw->tracks;

  Track * trk = ght_tracks->at(uid);

  if ( !trk )
    return;

  if ( !trk->trackpoints )
    return;

  /* with_timestamps: allow merging with 'similar' time type time tracks
     i.e. either those times, or those without */
  bool with_timestamps = trk->get_tp_first()->has_timestamp;
  GList * other_tracks = LayerTRW::find_tracks_with_timestamp_type(ght_tracks, with_timestamps, trk);
  other_tracks = g_list_reverse(other_tracks);

  if ( !other_tracks ) {
    if (with_timestamps)
      a_dialog_error_msg(VIK_GTK_WINDOW_FROM_LAYER(vtl), _("Failed. No other tracks with timestamps in this layer found"));
    else
      a_dialog_error_msg(VIK_GTK_WINDOW_FROM_LAYER(vtl), _("Failed. No other tracks without timestamps in this layer found"));
    return;
  }

  // Sort alphabetically for user presentation
  // Convert into list of names for usage with dialog function
  // TODO: Need to consider how to work best when we can have multiple tracks the same name...
  GList *other_tracks_names = NULL;
  GList *iter = g_list_first ( other_tracks );
  while ( iter ) {
    other_tracks_names = g_list_append(other_tracks_names, ght_tracks->at((sg_uid_t) ((long) iter->data))->name);
    iter = g_list_next ( iter );
  }

  other_tracks_names = g_list_sort_with_data (other_tracks_names, sort_alphabetically, NULL);

  GList *merge_list = a_dialog_select_from_list(VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                                other_tracks_names,
                                                true,
                                                _("Merge with..."),
                                                trk->is_route ? _("Select route to merge with") : _("Select track to merge with"));
  g_list_free(other_tracks);
  g_list_free(other_tracks_names);

  if (merge_list)
  {
    GList *l;
    for (l = merge_list; l != NULL; l = g_list_next(l)) {
      Track *merge_track;
      if ( trk->is_route )
        merge_track = vtl->trw->get_route((const char *) l->data);
      else
	merge_track = vtl->trw->get_track((const char *) l->data);

      if (merge_track) {
        trk->steal_and_append_trackpoints(merge_track);
        if (trk->is_route )
	  vtl->trw->delete_route(merge_track);
        else
	  vtl->trw->delete_track(merge_track);
        trk->trackpoints = g_list_sort(trk->trackpoints, trackpoint_compare);
      }
    }
    for (l = merge_list; l != NULL; l = g_list_next(l))
      free(l->data);
    g_list_free(merge_list);

    vik_layer_emit_update( VIK_LAYER(vtl) );
  }
}





/**
 * Join - this allows combining 'tracks' and 'track routes'
 *  i.e. doesn't care about whether tracks have consistent timestamps
 * ATM can only append one track at a time to the currently selected track
 */
static void trw_layer_append_track ( menu_array_sublayer values )
{

  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  Track *trk;
  std::unordered_map<sg_uid_t, SlavGPS::Track*> * ght_tracks = NULL;
  if ( KPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    ght_tracks = &vtl->trw->routes;
  else
    ght_tracks = &vtl->trw->tracks;

  sg_uid_t uid = (sg_uid_t) ((long) values[MA_SUBLAYER_ID]);
  trk = ght_tracks->at(uid);

  if ( !trk )
    return;

  GList *other_tracks_names = NULL;

  // Sort alphabetically for user presentation
  // Convert into list of names for usage with dialog function
  // TODO: Need to consider how to work best when we can have multiple tracks the same name...
  twt_udata udata;
  udata.result = &other_tracks_names;
  udata.exclude = trk;

  LayerTRW::sorted_track_id_by_name_list_exclude_self(ght_tracks, &udata);

  // Note the limit to selecting one track only
  //  this is to control the ordering of appending tracks, i.e. the selected track always goes after the current track
  //  (otherwise with multiple select the ordering would not be controllable by the user - automatically being alphabetically)
  GList *append_list = a_dialog_select_from_list(VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                                 other_tracks_names,
                                                 false,
                                                 trk->is_route ? _("Append Route"): _("Append Track"),
                                                 trk->is_route ? _("Select the route to append after the current route") :
                                                                 _("Select the track to append after the current track") );

  g_list_free(other_tracks_names);

  // It's a list, but shouldn't contain more than one other track!
  if ( append_list ) {
    GList *l;
    for (l = append_list; l != NULL; l = g_list_next(l)) {
      // TODO: at present this uses the first track found by name,
      //  which with potential multiple same named tracks may not be the one selected...
      Track *append_track;
      if ( trk->is_route )
	append_track = vtl->trw->get_route((const char *) l->data);
      else
        append_track = vtl->trw->get_track((const char *) l->data);

      if ( append_track ) {
        trk->steal_and_append_trackpoints(append_track);
        if ( trk->is_route )
	  vtl->trw->delete_route(append_track);
        else
	  vtl->trw->delete_track(append_track);
      }
    }
    for (l = append_list; l != NULL; l = g_list_next(l))
      free(l->data);
    g_list_free(append_list);

    vik_layer_emit_update( VIK_LAYER(vtl) );
  }
}

/**
 * Very similar to trw_layer_append_track for joining
 * but this allows selection from the 'other' list
 * If a track is selected, then is shows routes and joins the selected one
 * If a route is selected, then is shows tracks and joins the selected one
 */
static void trw_layer_append_other ( menu_array_sublayer values )
{

  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  sg_uid_t uid = (sg_uid_t) ((long) values[MA_SUBLAYER_ID]);
  Track *trk;
  std::unordered_map<sg_uid_t, SlavGPS::Track*> * ght_mykind;
  std::unordered_map<sg_uid_t, SlavGPS::Track*> * ght_others;
  if ( KPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE ) {
    ght_mykind = &vtl->trw->routes;
    ght_others = &vtl->trw->tracks;
  }
  else {
    ght_mykind = &vtl->trw->tracks;
    ght_others = &vtl->trw->routes;
  }

  trk = ght_mykind->at(uid);

  if ( !trk )
    return;

  GList *other_tracks_names = NULL;

  // Sort alphabetically for user presentation
  // Convert into list of names for usage with dialog function
  // TODO: Need to consider how to work best when we can have multiple tracks the same name...
  twt_udata udata;
  udata.result = &other_tracks_names;
  udata.exclude = trk;

  LayerTRW::sorted_track_id_by_name_list_exclude_self(ght_others, &udata);

  // Note the limit to selecting one track only
  //  this is to control the ordering of appending tracks, i.e. the selected track always goes after the current track
  //  (otherwise with multiple select the ordering would not be controllable by the user - automatically being alphabetically)
  GList *append_list = a_dialog_select_from_list(VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                                 other_tracks_names,
                                                 false,
                                                 trk->is_route ? _("Append Track"): _("Append Route"),
                                                 trk->is_route ? _("Select the track to append after the current route") :
                                                                 _("Select the route to append after the current track") );

  g_list_free(other_tracks_names);

  // It's a list, but shouldn't contain more than one other track!
  if ( append_list ) {
    GList *l;
    for (l = append_list; l != NULL; l = g_list_next(l)) {
      // TODO: at present this uses the first track found by name,
      //  which with potential multiple same named tracks may not be the one selected...

      // Get FROM THE OTHER TYPE list
      Track *append_track;
      if ( trk->is_route )
        append_track = vtl->trw->get_track((const char *) l->data);
      else
	append_track = vtl->trw->get_route((const char *) l->data);

      if ( append_track ) {

        if ( !append_track->is_route &&
             ((append_track->get_segment_count() > 1) ||
               (append_track->get_average_speed() > 0.0))) {

          if ( a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                      _("Converting a track to a route removes extra track data such as segments, timestamps, etc...\nDo you want to continue?"), NULL ) ) {
	    append_track->merge_segments();
	    append_track->to_routepoints();
	  }
          else {
            break;
          }
        }

        trk->steal_and_append_trackpoints(append_track);

	// Delete copied which is FROM THE OTHER TYPE list
        if ( trk->is_route )
	  vtl->trw->delete_track(append_track);
	else
	  vtl->trw->delete_route(append_track);
      }
    }
    for (l = append_list; l != NULL; l = g_list_next(l))
      free(l->data);
    g_list_free(append_list);
    vik_layer_emit_update( VIK_LAYER(vtl) );
  }
}

/* merge by segments */
static void trw_layer_merge_by_segment ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  sg_uid_t uid = (sg_uid_t) ((long) values[MA_SUBLAYER_ID]);
  Track * trk = vtl->trw->tracks.at(uid);
  unsigned int segments = trk->merge_segments();
  // NB currently no need to redraw as segments not actually shown on the display
  // However inform the user of what happened:
  char str[64];
  const char *tmp_str = ngettext("%d segment merged", "%d segments merged", segments);
  snprintf(str, 64, tmp_str, segments);
  a_dialog_info_msg (VIK_GTK_WINDOW_FROM_LAYER(vtl), str );
}

/* merge by time routine */
static void trw_layer_merge_by_timestamp ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  sg_uid_t uid = (sg_uid_t) ((long) values[MA_SUBLAYER_ID]);

  //time_t t1, t2;


  Track *orig_trk = vtl->trw->tracks.at(uid);
  if (orig_trk->trackpoints &&
      !orig_trk->get_tp_first()->has_timestamp) {
    a_dialog_error_msg(VIK_GTK_WINDOW_FROM_LAYER(vtl), _("Failed. This track does not have timestamp"));
    return;
  }

  GList * tracks_with_timestamp = LayerTRW::find_tracks_with_timestamp_type(&vtl->trw->tracks, true, orig_trk);
  tracks_with_timestamp = g_list_reverse(tracks_with_timestamp);

  if (!tracks_with_timestamp) {
    a_dialog_error_msg(VIK_GTK_WINDOW_FROM_LAYER(vtl), _("Failed. No other track in this layer has timestamp"));
    return;
  }
  g_list_free(tracks_with_timestamp);

  static unsigned int threshold_in_minutes = 1;
  if (!a_dialog_time_threshold(VIK_GTK_WINDOW_FROM_LAYER(vtl),
                               _("Merge Threshold..."),
                               _("Merge when time between tracks less than:"),
                               &threshold_in_minutes)) {
    return;
  }

  // keep attempting to merge all tracks until no merges within the time specified is possible
  bool attempt_merge = true;
  GList *nearby_tracks = NULL;
  GList *trps;

  while ( attempt_merge ) {

    // Don't try again unless tracks have changed
    attempt_merge = false;

    trps = orig_trk->trackpoints;
    if ( !trps )
      return;

    if (nearby_tracks) {
      g_list_free(nearby_tracks);
      nearby_tracks = NULL;
    }

    /* get a list of adjacent-in-time tracks */
    nearby_tracks = LayerTRW::find_nearby_tracks_by_time(vtl->trw->tracks, orig_trk, (threshold_in_minutes * 60));

    /* merge them */
    GList *l = nearby_tracks;
    while ( l ) {
      /* remove trackpoints from merged track, delete track */
      orig_trk->steal_and_append_trackpoints(((Track *) l->data) );
      vtl->trw->delete_track(((Track *) l->data));

      // Tracks have changed, therefore retry again against all the remaining tracks
      attempt_merge = true;

      l = g_list_next(l);
    }

    orig_trk->trackpoints = g_list_sort(orig_trk->trackpoints, trackpoint_compare);
  }

  g_list_free(nearby_tracks);

  vik_layer_emit_update( VIK_LAYER(vtl) );
}





/**
 * Split a track at the currently selected trackpoint
 */
void LayerTRW::split_at_selected_trackpoint(int subtype)
{
	if (!this->current_tpl) {
		return;
	}

	if (this->current_tpl->next && this->current_tpl->prev) {
		char * name = this->new_unique_sublayer_name(subtype, this->current_tp_track->name);
		if (name) {
			Track * new_trk = new Track(*this->current_tp_track, false);
			GList * newglist = g_list_alloc();
			newglist->prev = NULL;
			newglist->next = this->current_tpl->next;
			newglist->data = new Trackpoint(*((Trackpoint *) this->current_tpl->data));
			new_trk->trackpoints = newglist;

			this->current_tpl->next->prev = newglist; /* end old track here */
			this->current_tpl->next = NULL;

			// Bounds of the selected track changed due to the split
			this->current_tp_track->calculate_bounds();

			this->current_tpl = newglist; /* change tp to first of new track. */
			this->current_tp_track = new_trk;

			// Bounds of the new track created by the split
			new_trk->calculate_bounds();

			sg_uid_t uid = 0;
			if (new_trk->is_route) {
				this->add_route(new_trk, name);
				uid = LayerTRW::find_uid_of_track(this->routes, new_trk);
			} else {
				this->add_track(new_trk, name);
				uid = LayerTRW::find_uid_of_track(this->tracks, new_trk);
			}
			/* kamilTODO: how it's possible that a new track will already have an uid? */
			fprintf(stderr, "uid of new track is %u\n", uid);

			this->current_tp_uid = uid;

			vik_layer_emit_update(VIK_LAYER(this->vl));
		}
		free(name);
	}
}





/* split by time routine */
static void trw_layer_split_by_timestamp ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  sg_uid_t uid = (sg_uid_t) ((long) values[MA_SUBLAYER_ID]);
  Track * trk = vtl->trw->tracks.at(uid);
  GList *trps = trk->trackpoints;
  GList *iter;
  GList *newlists = NULL;
  GList *newtps = NULL;
  static unsigned int thr = 1;

  time_t ts, prev_ts;

  if ( !trps )
    return;

  if (!a_dialog_time_threshold(VIK_GTK_WINDOW_FROM_LAYER(vtl),
			       _("Split Threshold..."),
			       _("Split when time between trackpoints exceeds:"),
			       &thr)) {
    return;
  }

  /* iterate through trackpoints, and copy them into new lists without touching original list */
  prev_ts = ((Trackpoint *) trps->data)->timestamp;
  iter = trps;

  while (iter) {
    ts = ((Trackpoint *) iter->data)->timestamp;

    // Check for unordered time points - this is quite a rare occurence - unless one has reversed a track.
    if (ts < prev_ts) {
      char tmp_str[64];
      strftime ( tmp_str, sizeof(tmp_str), "%c", localtime(&ts) );
      if ( a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                _("Can not split track due to trackpoints not ordered in time - such as at %s.\n\nGoto this trackpoint?"),
                                tmp_str ) ) {
        goto_coord ( (void **) values[MA_VLP], vtl, values[MA_VVP], &(((Trackpoint *) iter->data)->coord) );
      }
      return;
    }

    if (ts - prev_ts > thr*60) {
      /* flush accumulated trackpoints into new list */
      newlists = g_list_append(newlists, g_list_reverse(newtps));
      newtps = NULL;
    }

    /* accumulate trackpoint copies in newtps, in reverse order */
    newtps = g_list_prepend(newtps, new Trackpoint(*((Trackpoint *) iter->data)));
    prev_ts = ts;
    iter = g_list_next(iter);
  }
  if (newtps) {
      newlists = g_list_append(newlists, g_list_reverse(newtps));
  }

  /* put lists of trackpoints into tracks */
  iter = newlists;
  // Only bother updating if the split results in new tracks
  if (g_list_length (newlists) > 1) {
    while (iter) {
      char *new_tr_name;
      Track *trk_copy = new Track(*trk, false);
      trk_copy->trackpoints = (GList *)(iter->data);

      new_tr_name = vtl->trw->new_unique_sublayer_name(VIK_TRW_LAYER_SUBLAYER_TRACK, trk->name);
      vtl->trw->add_track(trk_copy, new_tr_name);
      free( new_tr_name );
      trk_copy->calculate_bounds();
      iter = g_list_next(iter);
    }
    // Remove original track and then update the display
    vtl->trw->delete_track(trk);
    vik_layer_emit_update(VIK_LAYER(vtl));
  }
  g_list_free(newlists);
}

/**
 * Split a track by the number of points as specified by the user
 */
static void trw_layer_split_by_n_points ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if ( !trk )
    return;

  // Check valid track
  GList *trps = trk->trackpoints;
  if ( !trps )
    return;

  int points = a_dialog_get_positive_number(VIK_GTK_WINDOW_FROM_LAYER(vtl),
					     _("Split Every Nth Point"),
					     _("Split on every Nth point:"),
					     250,   // Default value as per typical limited track capacity of various GPS devices
					     2,     // Min
					     65536, // Max
					     5);    // Step
  // Was a valid number returned?
  if (!points)
    return;

  // Now split...
  GList *iter;
  GList *newlists = NULL;
  GList *newtps = NULL;
  int count = 0;
  iter = trps;

  while (iter) {
    /* accumulate trackpoint copies in newtps, in reverse order */
	  newtps = g_list_prepend(newtps, new Trackpoint(*((Trackpoint *) iter->data)));
    count++;
    if (count >= points) {
      /* flush accumulated trackpoints into new list */
      newlists = g_list_append(newlists, g_list_reverse(newtps));
      newtps = NULL;
      count = 0;
    }
    iter = g_list_next(iter);
  }

  // If there is a remaining chunk put that into the new split list
  // This may well be the whole track if no split points were encountered
  if (newtps) {
      newlists = g_list_append(newlists, g_list_reverse(newtps));
  }

  /* put lists of trackpoints into tracks */
  iter = newlists;
  // Only bother updating if the split results in new tracks
  if (g_list_length (newlists) > 1) {
    while (iter) {
      char *new_tr_name;
      Track *tr_copy = new Track(*trk, false);
      tr_copy->trackpoints = (GList *)(iter->data);

      if ( trk->is_route ) {
	new_tr_name = vtl->trw->new_unique_sublayer_name(VIK_TRW_LAYER_SUBLAYER_ROUTE, trk->name);
        vtl->trw->add_route(tr_copy, new_tr_name);
      }
      else {
	new_tr_name = vtl->trw->new_unique_sublayer_name(VIK_TRW_LAYER_SUBLAYER_TRACK, trk->name);
        vtl->trw->add_track(tr_copy, new_tr_name);
      }
      free( new_tr_name );
      tr_copy->calculate_bounds();

      iter = g_list_next(iter);
    }
    // Remove original track and then update the display
    if ( trk->is_route )
      vtl->trw->delete_route(trk);
    else
      vtl->trw->delete_track(trk);
    vik_layer_emit_update(VIK_LAYER(vtl));
  }
  g_list_free(newlists);
}

/**
 * Split a track at the currently selected trackpoint
 */
static void trw_layer_split_at_trackpoint ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  int subtype = KPOINTER_TO_INT (values[MA_SUBTYPE]);
  vtl->trw->split_at_selected_trackpoint(subtype);
}

/**
 * Split a track by its segments
 * Routes do not have segments so don't call this for routes
 */
static void trw_layer_split_segments ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  sg_uid_t uid = (sg_uid_t) ((long) values[MA_SUBLAYER_ID]);
  Track *trk = vtl->trw->tracks.at(uid);

  if ( !trk )
    return;

  unsigned int ntracks;

  Track **tracks = trk->split_into_segments(&ntracks);
  char *new_tr_name;
  unsigned int i;
  for ( i = 0; i < ntracks; i++ ) {
    if ( tracks[i] ) {
      new_tr_name = vtl->trw->new_unique_sublayer_name(VIK_TRW_LAYER_SUBLAYER_TRACK, trk->name);
      vtl->trw->add_track(tracks[i], new_tr_name);
      free( new_tr_name );
    }
  }
  if ( tracks ) {
    free( tracks );
    // Remove original track
    vtl->trw->delete_track(trk);
    vik_layer_emit_update ( VIK_LAYER(vtl) );
  }
  else {
    a_dialog_error_msg (VIK_GTK_WINDOW_FROM_LAYER(vtl), _("Can not split track as it has no segments"));
  }
}
/* end of split/merge routines */





void LayerTRW::trackpoint_selected_delete(Track * trk)
{
	GList * new_tpl;

	// Find available adjacent trackpoint
	if ((new_tpl = this->current_tpl->next) || (new_tpl = this->current_tpl->prev)) {
		if (((Trackpoint *) this->current_tpl->data)->newsegment && this->current_tpl->next)
			((Trackpoint *) this->current_tpl->next->data)->newsegment = true; /* don't concat segments on del */

		// Delete current trackpoint
		delete (Trackpoint *) this->current_tpl->data;
		trk->trackpoints = g_list_delete_link(trk->trackpoints, this->current_tpl);

		// Set to current to the available adjacent trackpoint
		this->current_tpl = new_tpl;

		if (this->current_tp_track) {
			this->current_tp_track->calculate_bounds();
		}
	} else {
		// Delete current trackpoint
		delete (Trackpoint *) this->current_tpl->data;
		trk->trackpoints = g_list_delete_link(trk->trackpoints, this->current_tpl);
		trw_layer_cancel_current_tp((VikTrwLayer *) this->vl, false );
	}
}





/**
 * Delete the selected point
 */
static void trw_layer_delete_point_selected ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if ( !trk )
    return;

  if ( !vtl->trw->current_tpl )
    return;

  vtl->trw->trackpoint_selected_delete(trk);

  // Track has been updated so update tps:
  vtl->trw->cancel_tps_of_track(trk);

  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

/**
 * Delete adjacent track points at the same position
 * AKA Delete Dulplicates on the Properties Window
 */
static void trw_layer_delete_points_same_position ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if ( !trk )
    return;

  unsigned long removed = trk->remove_dup_points();

  // Track has been updated so update tps:
  vtl->trw->cancel_tps_of_track(trk);

  // Inform user how much was deleted as it's not obvious from the normal view
  char str[64];
  const char *tmp_str = ngettext("Deleted %ld point", "Deleted %ld points", removed);
  snprintf(str, 64, tmp_str, removed);
  a_dialog_info_msg (VIK_GTK_WINDOW_FROM_LAYER(vtl), str);

  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

/**
 * Delete adjacent track points with the same timestamp
 * Normally new tracks that are 'routes' won't have any timestamps so should be OK to clean up the track
 */
static void trw_layer_delete_points_same_time ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if ( !trk )
    return;

  unsigned long removed = trk->remove_same_time_points( );

  // Track has been updated so update tps:
  vtl->trw->cancel_tps_of_track(trk);

  // Inform user how much was deleted as it's not obvious from the normal view
  char str[64];
  const char *tmp_str = ngettext("Deleted %ld point", "Deleted %ld points", removed);
  snprintf(str, 64, tmp_str, removed);
  a_dialog_info_msg (VIK_GTK_WINDOW_FROM_LAYER(vtl), str);

  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

/**
 * Insert a point
 */
static void trw_layer_insert_point_after ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if (!trk)
    return;

  vtl->trw->insert_tp_beside_current_tp(false);

  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

static void trw_layer_insert_point_before ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if (!trk)
    return;

  vtl->trw->insert_tp_beside_current_tp(true);

  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

/**
 * Reverse a track
 */
static void trw_layer_reverse ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if (!trk)
    return;

  trk->reverse();

  vik_layer_emit_update ( VIK_LAYER(vtl) );
}





/**
 * Open a program at the specified date
 * Mainly for RedNotebook - http://rednotebook.sourceforge.net/
 * But could work with any program that accepts a command line of --date=<date>
 * FUTURE: Allow configuring of command line options + date format
 */
void LayerTRW::diary_open(char const * date_str)
{
	GError *err = NULL;
	char * cmd = g_strdup_printf("%s %s%s", diary_program, "--date=", date_str);
	if (!g_spawn_command_line_async(cmd, &err)) {
		a_dialog_error_msg_extra(VIK_GTK_WINDOW_FROM_LAYER(this->vl), _("Could not launch %s to open file."), diary_program);
		g_error_free(err);
	}
	free(cmd);
}





/**
 * Open a diary at the date of the track or waypoint
 */
static void trw_layer_diary ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  sg_uid_t uid = (sg_uid_t) ((long) values[MA_SUBLAYER_ID]);

  if ( KPOINTER_TO_INT(values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
    Track * trk = vtl->trw->tracks.at(uid);
    if (!trk)
      return;

    char date_buf[20];
    date_buf[0] = '\0';
    if ( trk->trackpoints && ((Trackpoint *) trk->trackpoints->data)->has_timestamp ) {
      strftime (date_buf, sizeof(date_buf), "%Y-%m-%d", gmtime(&(((Trackpoint *) trk->trackpoints->data)->timestamp)));
      vtl->trw->diary_open(date_buf);
    }
    else
      a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("This track has no date information.") );
  }
  else if ( KPOINTER_TO_INT(values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_WAYPOINT ) {
    Waypoint * wp = vtl->trw->waypoints.at(uid);
    if (!wp)
      return;

    char date_buf[20];
    date_buf[0] = '\0';
    if (wp->has_timestamp) {
      strftime (date_buf, sizeof(date_buf), "%Y-%m-%d", gmtime(&(wp->timestamp)));
      vtl->trw->diary_open(date_buf);
    }
    else
      a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("This waypoint has no date information.") );
  }
}

/**
 * Open a program at the specified date
 * Mainly for Stellarium - http://stellarium.org/
 * But could work with any program that accepts the same command line options...
 * FUTURE: Allow configuring of command line options + format or parameters
 */
void LayerTRW::astro_open(char const * date_str,  char const * time_str, char const * lat_str, char const * lon_str, char const * alt_str)
{
	GError *err = NULL;
	char *tmp;
	int fd = g_file_open_tmp("vik-astro-XXXXXX.ini", &tmp, &err);
	if (fd < 0) {
		fprintf(stderr, "WARNING: %s: Failed to open temporary file: %s\n", __FUNCTION__, err->message);
		g_clear_error(&err);
		return;
	}
	char *cmd = g_strdup_printf("%s %s %s %s %s %s %s %s %s %s %s %s %s %s",
				    astro_program, "-c", tmp, "--full-screen no", "--sky-date", date_str, "--sky-time", time_str, "--latitude", lat_str, "--longitude", lon_str, "--altitude", alt_str);
	fprintf(stderr, "WARNING: %s\n", cmd);
	if (!g_spawn_command_line_async(cmd, &err)) {
		a_dialog_error_msg_extra(VIK_GTK_WINDOW_FROM_LAYER(this->vl), _("Could not launch %s"), astro_program);
		fprintf(stderr, "WARNING: %s\n", err->message);
		g_error_free(err);
	}
	util_add_to_deletion_list(tmp);
	free(tmp);
	free(cmd);
}





// Format of stellarium lat & lon seems designed to be particularly awkward
//  who uses ' & " in the parameters for the command line?!
// -1d4'27.48"
// +53d58'16.65"
static char *convert_to_dms ( double dec )
{
  double tmp;
  char sign_c = ' ';
  int val_d, val_m;
  double val_s;
  char *result = NULL;

  if ( dec > 0 )
    sign_c = '+';
  else if ( dec < 0 )
    sign_c = '-';
  else // Nul value
    sign_c = ' ';

  // Degrees
  tmp = fabs(dec);
  val_d = (int)tmp;

  // Minutes
  tmp = (tmp - val_d) * 60;
  val_m = (int)tmp;

  // Seconds
  val_s = (tmp - val_m) * 60;

  // Format
  result = g_strdup_printf ( "%c%dd%d\\\'%.4f\\\"", sign_c, val_d, val_m, val_s );
  return result;
}

/**
 * Open an astronomy program at the date & position of the track center, trackpoint or waypoint
 */
static void trw_layer_astro ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  sg_uid_t uid = (sg_uid_t) ((long) values[MA_SUBLAYER_ID]);

  if ( KPOINTER_TO_INT(values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
    Track * trk = vtl->trw->tracks.at(uid);
    if (!trk)
      return;

    Trackpoint * tp = NULL;
    if ( vtl->trw->current_tpl )
      // Current Trackpoint
      tp = ((Trackpoint *) vtl->trw->current_tpl->data);
    else if ( trk->trackpoints )
      // Otherwise first trackpoint
      tp = ((Trackpoint *) trk->trackpoints->data);
    else
      // Give up
      return;

    if ( tp->has_timestamp ) {
      char date_buf[20];
      strftime (date_buf, sizeof(date_buf), "%Y%m%d", gmtime(&(tp->timestamp)));
      char time_buf[20];
      strftime (time_buf, sizeof(time_buf), "%H:%M:%S", gmtime(&(tp->timestamp)));
      struct LatLon ll;
      vik_coord_to_latlon ( &tp->coord, &ll );
      char *lat_str = convert_to_dms ( ll.lat );
      char *lon_str = convert_to_dms ( ll.lon );
      char alt_buf[20];
      snprintf (alt_buf, sizeof(alt_buf), "%d", (int)round(tp->altitude) );
      vtl->trw->astro_open(date_buf, time_buf, lat_str, lon_str, alt_buf);
      free( lat_str );
      free( lon_str );
    }
    else
      a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("This track has no date information.") );
  }
  else if ( KPOINTER_TO_INT(values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_WAYPOINT ) {
    sg_uid_t wp_uid = (sg_uid_t) ((long) values[MA_SUBLAYER_ID]);
    Waypoint * wp = vtl->trw->waypoints.at(wp_uid);
    if (!wp)
      return;

    if ( wp->has_timestamp ) {
      char date_buf[20];
      strftime (date_buf, sizeof(date_buf), "%Y%m%d", gmtime(&(wp->timestamp)));
      char time_buf[20];
      strftime (time_buf, sizeof(time_buf), "%H:%M:%S", gmtime(&(wp->timestamp)));
      struct LatLon ll;
      vik_coord_to_latlon ( &wp->coord, &ll );
      char *lat_str = convert_to_dms ( ll.lat );
      char *lon_str = convert_to_dms ( ll.lon );
      char alt_buf[20];
      snprintf (alt_buf, sizeof(alt_buf), "%d", (int)round(wp->altitude) );
      vtl->trw->astro_open(date_buf, time_buf, lat_str, lon_str, alt_buf);
      free( lat_str );
      free( lon_str );
    }
    else
      a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("This waypoint has no date information.") );
  }
}





int check_tracks_for_same_name ( gconstpointer aa, gconstpointer bb, void * udata )
{
  const char* namea = (const char*) aa;
  const char* nameb = (const char*) bb;

  // the test
  int result = strcmp ( namea, nameb );

  if ( result == 0 ) {
    // Found two names the same
    same_track_name_udata *user_data = (same_track_name_udata *) udata;
    user_data->has_same_track_name = true;
    user_data->same_track_name = namea;
  }

  // Leave ordering the same
  return 0;
}





/**
 * Force unqiue track names for the track table specified
 * Note the panel is a required parameter to enable the update of the names displayed
 * Specify if on tracks or else on routes
 */
void LayerTRW::uniquify_tracks(VikLayersPanel * vlp, std::unordered_map<sg_uid_t, Track *> & track_table, bool ontrack)
{
	// . Search list for an instance of repeated name
	// . get track of this name
	// . create new name
	// . rename track & update equiv. treeview iter
	// . repeat until all different

	same_track_name_udata udata;


	udata.has_same_track_name = false;
	udata.same_track_name = NULL;

	GList * track_names = LayerTRW::sorted_track_id_by_name_list(track_table);

	// No tracks
	if (!track_names) {
		return;
	}

	GList * dummy_list1 = g_list_sort_with_data(track_names, check_tracks_for_same_name, &udata);

	// Still no tracks...
	if (!dummy_list1) {
		return;
	}

	VikTrwLayer * vtl = (VikTrwLayer *) this->vl;

	while (udata.has_same_track_name) {

		// Find a track with the same name
		Track * trk;
		if (ontrack) {
			trk = this->get_track(udata.same_track_name);
		} else {
			trk = this->get_route(udata.same_track_name);
		}

		if (!trk) {
			// Broken :(
			fprintf(stderr, "CRITICAL: Houston, we've had a problem.\n");
			vik_statusbar_set_message(vik_window_get_statusbar(VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl))), VIK_STATUSBAR_INFO,
						    _("Internal Error in LayerTRW::uniquify_tracks"));
			return;
		}

		// Rename it
		char * newname = this->new_unique_sublayer_name(VIK_TRW_LAYER_SUBLAYER_TRACK, udata.same_track_name);
		trk->set_name(newname);

		// Need want key of it for treeview update
		sg_uid_t uid = LayerTRW::find_uid_of_track(track_table, trk);
		if (uid) {

			GtkTreeIter *it;
			if (ontrack) {
				it = this->tracks_iters.at(uid);
			} else {
				it = this->routes_iters.at(uid);
			}

			if (it) {
				vik_treeview_item_set_name(VIK_LAYER(vtl)->vt, it, newname);
				if (ontrack) {
					vik_treeview_sort_children(VIK_LAYER(vtl)->vt, &(this->track_iter), vtl->track_sort_order);
				} else {
					vik_treeview_sort_children(VIK_LAYER(vtl)->vt, &(this->route_iter), vtl->track_sort_order);
				}
			}
		}

		// Start trying to find same names again...
		track_names = NULL; /* kamilFIXME: this list is not free()d anywhere? */
		track_names = LayerTRW::sorted_track_id_by_name_list(track_table);
		udata.has_same_track_name = false;
		GList * dummy_list2 = g_list_sort_with_data(track_names, check_tracks_for_same_name, &udata);

		// No tracks any more - give up searching
		if (!dummy_list2) {
			udata.has_same_track_name = false;
		}
	}

	// Update
	vik_layers_panel_emit_update(vlp->panel_ref);
}





void LayerTRW::sort_order_specified(unsigned int sublayer_type, vik_layer_sort_order_t order)
{
	VikTrwLayer * vtl = (VikTrwLayer *) this->vl;
	GtkTreeIter * iter = NULL;

	switch (sublayer_type) {
	case VIK_TRW_LAYER_SUBLAYER_TRACKS:
		iter = &(this->track_iter);
		vtl->track_sort_order = order;
		break;
	case VIK_TRW_LAYER_SUBLAYER_ROUTES:
		iter = &(this->route_iter);
		vtl->track_sort_order = order;
		break;
	default: // VIK_TRW_LAYER_SUBLAYER_WAYPOINTS:
		iter = &(this->waypoint_iter);
		vtl->wp_sort_order = order;
		break;
	}

	vik_treeview_sort_children(VIK_LAYER(vtl)->vt, iter, order);
}





static void trw_layer_sort_order_a2z ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  vtl->trw->sort_order_specified(KPOINTER_TO_INT(values[MA_SUBTYPE]), VL_SO_ALPHABETICAL_ASCENDING);
}

static void trw_layer_sort_order_z2a ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  vtl->trw->sort_order_specified(KPOINTER_TO_INT(values[MA_SUBTYPE]), VL_SO_ALPHABETICAL_DESCENDING);
}

static void trw_layer_sort_order_timestamp_ascend ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  vtl->trw->sort_order_specified(KPOINTER_TO_INT(values[MA_SUBTYPE]), VL_SO_DATE_ASCENDING);
}

static void trw_layer_sort_order_timestamp_descend ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  vtl->trw->sort_order_specified(KPOINTER_TO_INT(values[MA_SUBTYPE]), VL_SO_DATE_DESCENDING);
}

/**
 *
 */
static void trw_layer_delete_tracks_from_selection ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);

  // Ensure list of track names offered is unique
  if ( LayerTRW::has_same_track_names ( vtl->trw->tracks ) ) {
    if ( a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
			      _("Multiple entries with the same name exist. This method only works with unique names. Force unique names now?"), NULL ) ) {
      vtl->trw->uniquify_tracks(VIK_LAYERS_PANEL(values[MA_VLP]), vtl->trw->tracks, true);
    }
    else
      return;
  }

  // Sort list alphabetically for better presentation
  GList * all = LayerTRW::sorted_track_id_by_name_list(vtl->trw->tracks);

  if ( ! all ) {
    a_dialog_error_msg (VIK_GTK_WINDOW_FROM_LAYER(vtl),	_("No tracks found"));
    return;
  }

  // Get list of items to delete from the user
  GList *delete_list = a_dialog_select_from_list(VIK_GTK_WINDOW_FROM_LAYER(vtl),
						 all,
						 true,
						 _("Delete Selection"),
						 _("Select tracks to delete"));
  g_list_free(all);

  // Delete requested tracks
  // since specificly requested, IMHO no need for extra confirmation
  if ( delete_list ) {
    GList *l;
    for (l = delete_list; l != NULL; l = g_list_next(l)) {
      // This deletes first trk it finds of that name (but uniqueness is enforced above)
      vtl->trw->delete_track_by_name((const char *) l->data, false);
    }
    g_list_free(delete_list);
    // Reset layer timestamps in case they have now changed
    vik_treeview_item_set_timestamp ( vtl->vl.vt, &vtl->vl.iter, trw_layer_get_timestamp(vtl) );

    vik_layer_emit_update( VIK_LAYER(vtl) );
  }
}

/**
 *
 */
static void trw_layer_delete_routes_from_selection ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);

  // Ensure list of track names offered is unique
  if ( LayerTRW::has_same_track_names ( vtl->trw->routes ) ) {
    if ( a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                              _("Multiple entries with the same name exist. This method only works with unique names. Force unique names now?"), NULL ) ) {
      vtl->trw->uniquify_tracks(VIK_LAYERS_PANEL(values[MA_VLP]), vtl->trw->routes, false);
    }
    else
      return;
  }

  // Sort list alphabetically for better presentation
  GList * all = LayerTRW::sorted_track_id_by_name_list(vtl->trw->routes);

  if ( ! all ) {
    a_dialog_error_msg (VIK_GTK_WINDOW_FROM_LAYER(vtl), _("No routes found"));
    return;
  }

  // Get list of items to delete from the user
  GList *delete_list = a_dialog_select_from_list ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                                   all,
                                                   true,
                                                   _("Delete Selection"),
                                                   _("Select routes to delete") );
  g_list_free(all);

  // Delete requested routes
  // since specificly requested, IMHO no need for extra confirmation
  if ( delete_list ) {
    GList *l;
    for (l = delete_list; l != NULL; l = g_list_next(l)) {
      // This deletes first route it finds of that name (but uniqueness is enforced above)
      vtl->trw->delete_track_by_name((const char *) l->data, true);
    }
    g_list_free(delete_list);
    vik_layer_emit_update( VIK_LAYER(vtl) );
  }
}

typedef struct {
  bool    has_same_waypoint_name;
  const char *same_waypoint_name;
} same_waypoint_name_udata;

static int check_waypoints_for_same_name ( gconstpointer aa, gconstpointer bb, void * udata )
{
  const char* namea = (const char*) aa;
  const char* nameb = (const char*) bb;

  // the test
  int result = strcmp ( namea, nameb );

  if ( result == 0 ) {
    // Found two names the same
    same_waypoint_name_udata *user_data = (same_waypoint_name_udata *) udata;
    user_data->has_same_waypoint_name = true;
    user_data->same_waypoint_name = namea;
  }

  // Leave ordering the same
  return 0;
}





/**
 * Find out if any waypoints have the same name in this layer
 */
bool LayerTRW::has_same_waypoint_names()
{
	// Sort items by name, then compare if any next to each other are the same

	GList * waypoint_names = NULL;
	LayerTRW::sorted_wp_id_by_name_list(this->waypoints, &waypoint_names);

	// No waypoints
	if (!waypoint_names) {
		return false;
	}

	same_waypoint_name_udata udata;
	udata.has_same_waypoint_name = false;

	// Use sort routine to traverse list comparing items
	// Don't care how this list ends up ordered ( doesn't actually change ) - care about the returned status
	GList * dummy_list = g_list_sort_with_data(waypoint_names, check_waypoints_for_same_name, &udata);
	// Still no waypoints...
	if (!dummy_list) {
		return false;
	}

	return udata.has_same_waypoint_name;
}





/**
 * Force unqiue waypoint names for this layer
 * Note the panel is a required parameter to enable the update of the names displayed
 */
void LayerTRW::uniquify_waypoints(VikLayersPanel * vlp)
{
	// . Search list for an instance of repeated name
	// . get waypoint of this name
	// . create new name
	// . rename waypoint & update equiv. treeview iter
	// . repeat until all different

	same_waypoint_name_udata udata;

	GList * waypoint_names = NULL;
	udata.has_same_waypoint_name = false;
	udata.same_waypoint_name = NULL;

	LayerTRW::sorted_wp_id_by_name_list(this->waypoints, &waypoint_names);

	// No waypoints
	if (!waypoint_names) {
		return;
	}

	GList * dummy_list1 = g_list_sort_with_data(waypoint_names, check_waypoints_for_same_name, &udata);
	// Still no waypoints...
	if (!dummy_list1) {
		return;
	}

	VikTrwLayer * vtl = (VikTrwLayer *) this->vl;

	while (udata.has_same_waypoint_name) {

		// Find a waypoint with the same name
		Waypoint * wp = this->get_waypoint((const char *) udata.same_waypoint_name);
		if (!wp) {
			// Broken :(
			fprintf(stderr, "CRITICAL: Houston, we've had a problem.\n");
			vik_statusbar_set_message ( vik_window_get_statusbar (VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl))), VIK_STATUSBAR_INFO,
						    _("Internal Error in uniquify_waypoints") );
			return;
		}

		// Rename it
		char * newname = this->new_unique_sublayer_name(VIK_TRW_LAYER_SUBLAYER_WAYPOINT, udata.same_waypoint_name);

		this->waypoint_rename(wp, newname);

		// Start trying to find same names again...
		waypoint_names = NULL;
		LayerTRW::sorted_wp_id_by_name_list(this->waypoints, &waypoint_names);
		udata.has_same_waypoint_name = false;
		GList * dummy_list2 = g_list_sort_with_data(waypoint_names, check_waypoints_for_same_name, &udata);

		// No waypoints any more - give up searching
		if (!dummy_list2) {
			udata.has_same_waypoint_name = false;
		}
	}

	// Update
	vik_layers_panel_emit_update(vlp->panel_ref);
}




/**
 *
 */
static void trw_layer_delete_waypoints_from_selection ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  GList *all = NULL;

  // Ensure list of waypoint names offered is unique
  if (vtl->trw->has_same_waypoint_names()) {
    if ( a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
			      _("Multiple entries with the same name exist. This method only works with unique names. Force unique names now?"), NULL ) ) {
      vtl->trw->uniquify_waypoints(VIK_LAYERS_PANEL(values[MA_VLP]));
    }
    else
      return;
  }

  // Sort list alphabetically for better presentation
  LayerTRW::sorted_wp_id_by_name_list(vtl->trw->waypoints, &all);
  if ( ! all ) {
	  a_dialog_error_msg(VIK_GTK_WINDOW_FROM_LAYER(vtl), _("No waypoints found"));
	  return;
  }

  all = g_list_sort_with_data(all, sort_alphabetically, NULL);

  // Get list of items to delete from the user
  GList *delete_list = a_dialog_select_from_list(VIK_GTK_WINDOW_FROM_LAYER(vtl),
						 all,
						 true,
						 _("Delete Selection"),
						 _("Select waypoints to delete"));
  g_list_free(all);

  // Delete requested waypoints
  // since specificly requested, IMHO no need for extra confirmation
  if ( delete_list ) {
    GList *l;
    for (l = delete_list; l != NULL; l = g_list_next(l)) {
      // This deletes first waypoint it finds of that name (but uniqueness is enforced above)
      vtl->trw->delete_waypoint_by_name((const char *) l->data);
    }
    g_list_free(delete_list);

    vtl->trw->calculate_bounds_waypoints();
    // Reset layer timestamp in case it has now changed
    vik_treeview_item_set_timestamp ( vtl->vl.vt, &vtl->vl.iter, trw_layer_get_timestamp(vtl) );
    vik_layer_emit_update( VIK_LAYER(vtl) );
  }

}
/**
 *
 */
static void trw_layer_waypoints_visibility_off(menu_array_layer values)
{
	VikTrwLayer * vtl = VIK_TRW_LAYER(values[MA_VTL]);
	LayerTRW::set_iter_visibility(vtl->trw->waypoints_iters, VIK_LAYER(vtl)->vt, false);
	LayerTRW::set_waypoints_visibility(vtl->trw->waypoints, false);
	// Redraw
	vik_layer_emit_update(VIK_LAYER(vtl));
}





/**
 *
 */
static void trw_layer_waypoints_visibility_on(menu_array_layer values)
{
	VikTrwLayer * vtl = VIK_TRW_LAYER(values[MA_VTL]);
	LayerTRW::set_iter_visibility(vtl->trw->waypoints_iters, VIK_LAYER(vtl)->vt, true);
	LayerTRW::set_waypoints_visibility(vtl->trw->waypoints, true);
	// Redraw
	vik_layer_emit_update(VIK_LAYER(vtl));
}





/**
 *
 */
static void trw_layer_waypoints_visibility_toggle(menu_array_layer values)
{
	VikTrwLayer * vtl = VIK_TRW_LAYER(values[MA_VTL]);
	LayerTRW::iter_visibility_toggle(vtl->trw->waypoints_iters, VIK_LAYER(vtl)->vt);
	LayerTRW::waypoints_toggle_visibility(vtl->trw->waypoints);
	// Redraw
	vik_layer_emit_update(VIK_LAYER(vtl));
}





/**
 *
 */
static void trw_layer_tracks_visibility_off(menu_array_layer values)
{
	VikTrwLayer * vtl = VIK_TRW_LAYER(values[MA_VTL]);
	LayerTRW::set_iter_visibility(vtl->trw->tracks_iters, VIK_LAYER(vtl)->vt, false);
	LayerTRW::set_tracks_visibility(vtl->trw->tracks, false);
	// Redraw
	vik_layer_emit_update(VIK_LAYER(vtl));
}





/**
 *
 */
static void trw_layer_tracks_visibility_on(menu_array_layer values)
{
	VikTrwLayer * vtl = VIK_TRW_LAYER(values[MA_VTL]);
	LayerTRW::set_iter_visibility(vtl->trw->tracks_iters, VIK_LAYER(vtl)->vt, true);
	LayerTRW::set_tracks_visibility(vtl->trw->tracks, true);
	// Redraw
	vik_layer_emit_update(VIK_LAYER(vtl));
}





/**
 *
 */
static void trw_layer_tracks_visibility_toggle(menu_array_layer values)
{
	VikTrwLayer * vtl = VIK_TRW_LAYER(values[MA_VTL]);
	LayerTRW::iter_visibility_toggle(vtl->trw->tracks_iters, VIK_LAYER(vtl)->vt);
	LayerTRW::tracks_toggle_visibility(vtl->trw->tracks);
	// Redraw
	vik_layer_emit_update(VIK_LAYER(vtl));
}





/**
 *
 */
static void trw_layer_routes_visibility_off(menu_array_layer values)
{
	VikTrwLayer * vtl = VIK_TRW_LAYER(values[MA_VTL]);
	LayerTRW::set_iter_visibility(vtl->trw->routes_iters, VIK_LAYER(vtl)->vt, false);
	LayerTRW::set_tracks_visibility(vtl->trw->routes, false);
	// Redraw
	vik_layer_emit_update(VIK_LAYER(vtl));
}





/**
 *
 */
static void trw_layer_routes_visibility_on(menu_array_layer values)
{
	VikTrwLayer * vtl = VIK_TRW_LAYER(values[MA_VTL]);
	LayerTRW::set_iter_visibility(vtl->trw->routes_iters, VIK_LAYER(vtl)->vt, true);
	LayerTRW::set_tracks_visibility(vtl->trw->routes, true);
	// Redraw
	vik_layer_emit_update(VIK_LAYER(vtl));
}





/**
 *
 */
static void trw_layer_routes_visibility_toggle(menu_array_layer values)
{
	VikTrwLayer * vtl = VIK_TRW_LAYER(values[MA_VTL]);
	LayerTRW::iter_visibility_toggle(vtl->trw->routes_iters, VIK_LAYER(vtl)->vt );
	LayerTRW::tracks_toggle_visibility(vtl->trw->routes);
	// Redraw
	vik_layer_emit_update(VIK_LAYER(vtl));
}





/**
 * Helper function to construct a list of #vik_trw_waypoint_list_t
 */
GList * LayerTRW::build_waypoint_list_t(GList * waypoints)
{
	GList * waypoints_and_layers = NULL;
	// build waypoints_and_layers list
	while (waypoints) {
		vik_trw_waypoint_list_t * vtdl = (vik_trw_waypoint_list_t *) malloc(sizeof (vik_trw_waypoint_list_t));
		vtdl->wp = (Waypoint *) waypoints->data;
		vtdl->vtl = (VikTrwLayer *) this->vl;
		waypoints_and_layers = g_list_prepend(waypoints_and_layers, vtdl);
		waypoints = g_list_next(waypoints);
	}
	return waypoints_and_layers;
}





/**
 * trw_layer_create_waypoint_list:
 *
 * Create the latest list of waypoints with the associated layer(s)
 *  Although this will always be from a single layer here
 */
static GList* trw_layer_create_waypoint_list ( VikLayer *vl, void * user_data )
{
	VikTrwLayer *vtl = VIK_TRW_LAYER(vl);

	GList * waypoints = NULL;
	int index = 0;
	for (auto i = vtl->trw->waypoints.begin(); i != vtl->trw->waypoints.end(); i++) {
		waypoints = g_list_insert(waypoints, i->second, index++);
	}

	return vtl->trw->build_waypoint_list_t(waypoints);
}





/**
 * trw_layer_analyse_close:
 *
 * Stuff to do on dialog closure
 */
static void trw_layer_analyse_close ( GtkWidget *dialog, int resp, VikLayer* vl )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(vl);
  gtk_widget_destroy ( dialog );
  vtl->tracks_analysis_dialog = NULL;
}





/**
 * Helper function to construct a list of #vik_trw_track_list_t
 */
GList * LayerTRW::build_track_list_t(GList * tracks)
{
	GList * tracks_and_layers = NULL;
	// build tracks_and_layers list
	while (tracks) {
		vik_trw_track_list_t * vtdl = (vik_trw_track_list_t *) malloc(sizeof (vik_trw_track_list_t));
		vtdl->trk = ((Track *) tracks->data);
		vtdl->vtl = (VikTrwLayer *) this->vl;
		tracks_and_layers = g_list_prepend(tracks_and_layers, vtdl);
		tracks = g_list_next(tracks);
	}
	return tracks_and_layers;
}





/**
 * trw_layer_create_track_list:
 *
 * Create the latest list of tracks with the associated layer(s)
 *  Although this will always be from a single layer here
 */
static GList* trw_layer_create_track_list ( VikLayer *vl, void * user_data )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(vl);
  GList *tracks = NULL;
  if ( KPOINTER_TO_INT(user_data) == VIK_TRW_LAYER_SUBLAYER_TRACKS )
    tracks = LayerTRW::get_track_values(&tracks, vtl->trw->get_tracks());
  else
    tracks = LayerTRW::get_track_values(&tracks, vtl->trw->get_routes());

  return vtl->trw->build_track_list_t(tracks);
}

static void trw_layer_tracks_stats ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  // There can only be one!
  if ( vtl->tracks_analysis_dialog )
    return;

  vtl->tracks_analysis_dialog = vik_trw_layer_analyse_this ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                                             VIK_LAYER(vtl)->name,
                                                             VIK_LAYER(vtl),
                                                             KINT_TO_POINTER(VIK_TRW_LAYER_SUBLAYER_TRACKS),
                                                             trw_layer_create_track_list,
                                                             trw_layer_analyse_close );
}

/**
 *
 */
static void trw_layer_routes_stats ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  // There can only be one!
  if ( vtl->tracks_analysis_dialog )
    return;

  vtl->tracks_analysis_dialog = vik_trw_layer_analyse_this ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                                             VIK_LAYER(vtl)->name,
                                                             VIK_LAYER(vtl),
                                                             KINT_TO_POINTER(VIK_TRW_LAYER_SUBLAYER_ROUTES),
                                                             trw_layer_create_track_list,
                                                             trw_layer_analyse_close );
}

static void trw_layer_goto_waypoint ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  sg_uid_t wp_uid = (sg_uid_t) ((long) values[MA_SUBLAYER_ID]);
  Waypoint * wp = vtl->trw->waypoints.at(wp_uid);
  if ( wp )
    goto_coord ( (void **) values[MA_VLP], vtl, values[MA_VVP], &(wp->coord) );
}

static void trw_layer_waypoint_gc_webpage ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  sg_uid_t wp_uid = (sg_uid_t) ((long) values[MA_SUBLAYER_ID]);
  Waypoint * wp = vtl->trw->waypoints.at(wp_uid);
  if ( !wp )
    return;
  char *webpage = g_strdup_printf("http://www.geocaching.com/seek/cache_details.aspx?wp=%s", wp->name );
  open_url(VIK_GTK_WINDOW_FROM_LAYER(VIK_LAYER(vtl)), webpage);
  free( webpage );
}

static void trw_layer_waypoint_webpage ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  sg_uid_t wp_uid = (sg_uid_t) ((long) values[MA_SUBLAYER_ID]);
  Waypoint * wp = vtl->trw->waypoints.at(wp_uid);
  if ( !wp )
    return;
  if ( wp->url ) {
    open_url(VIK_GTK_WINDOW_FROM_LAYER(VIK_LAYER(vtl)), wp->url);
  } else if ( !strncmp(wp->comment, "http", 4) ) {
    open_url(VIK_GTK_WINDOW_FROM_LAYER(VIK_LAYER(vtl)), wp->comment);
  } else if ( !strncmp(wp->description, "http", 4) ) {
    open_url(VIK_GTK_WINDOW_FROM_LAYER(VIK_LAYER(vtl)), wp->description);
  }
}

static const char* trw_layer_sublayer_rename_request ( VikTrwLayer *l, const char *newname, void * vlp, int subtype, void * sublayer, GtkTreeIter *iter )
{
  sg_uid_t uid = (sg_uid_t) ((long) sublayer);
  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT )
  {
    Waypoint * wp = l->trw->waypoints.at(uid);

    // No actual change to the name supplied
    if ( wp->name )
      if (strcmp(newname, wp->name) == 0 )
       return NULL;

    Waypoint * wpf = l->trw->get_waypoint(newname);

    if ( wpf ) {
      // An existing waypoint has been found with the requested name
      if ( ! a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(l),
           _("A waypoint with the name \"%s\" already exists. Really rename to the same name?"),
           newname ) )
        return NULL;
    }

    // Update WP name and refresh the treeview
    wp->set_name(newname);

    vik_treeview_item_set_name ( VIK_LAYER(l)->vt, iter, newname );
    vik_treeview_sort_children ( VIK_LAYER(l)->vt, &(l->trw->waypoint_iter), l->wp_sort_order );

    vik_layers_panel_emit_update ( VIK_LAYERS_PANEL(vlp)->panel_ref );

    return newname;
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK )
  {
    Track * trk = l->trw->tracks.at(uid);

    // No actual change to the name supplied
    if ( trk->name )
      if (strcmp(newname, trk->name) == 0)
	return NULL;

    Track *trkf = l->trw->get_track((const char *) newname);

    if ( trkf ) {
      // An existing track has been found with the requested name
      if ( ! a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(l),
          _("A track with the name \"%s\" already exists. Really rename to the same name?"),
          newname ) )
        return NULL;
    }
    // Update track name and refresh GUI parts
    trk->set_name(newname);

    // Update any subwindows that could be displaying this track which has changed name
    // Only one Track Edit Window
    if ( l->trw->current_tp_track == trk && l->trw->tpwin ) {
      vik_trw_layer_tpwin_set_track_name ( l->trw->tpwin, newname );
    }
    // Property Dialog of the track
    vik_trw_layer_propwin_update ( trk );

    vik_treeview_item_set_name ( VIK_LAYER(l)->vt, iter, newname );
    vik_treeview_sort_children ( VIK_LAYER(l)->vt, &(l->trw->track_iter), l->track_sort_order );

    vik_layers_panel_emit_update ( VIK_LAYERS_PANEL(vlp)->panel_ref );

    return newname;
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE )
  {
    Track * trk = l->trw->routes.at(uid);

    // No actual change to the name supplied
    if ( trk->name )
      if (strcmp(newname, trk->name) == 0)
        return NULL;

    Track * trkf = l->trw->get_route((const char *) newname);

    if ( trkf ) {
      // An existing track has been found with the requested name
      if ( ! a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(l),
          _("A route with the name \"%s\" already exists. Really rename to the same name?"),
          newname ) )
        return NULL;
    }
    // Update track name and refresh GUI parts
    trk->set_name(newname);

    // Update any subwindows that could be displaying this track which has changed name
    // Only one Track Edit Window
    if ( l->trw->current_tp_track == trk && l->trw->tpwin ) {
      vik_trw_layer_tpwin_set_track_name ( l->trw->tpwin, newname );
    }
    // Property Dialog of the track
    vik_trw_layer_propwin_update ( trk );

    vik_treeview_item_set_name ( VIK_LAYER(l)->vt, iter, newname );
    vik_treeview_sort_children ( VIK_LAYER(l)->vt, &(l->trw->track_iter), l->track_sort_order );

    vik_layers_panel_emit_update ( VIK_LAYERS_PANEL(vlp)->panel_ref );

    return newname;
  }
  return NULL;
}

static bool is_valid_geocache_name ( char *str )
{
  int len = strlen ( str );
  return len >= 3 && len <= 7 && str[0] == 'G' && str[1] == 'C' && isalnum(str[2]) && (len < 4 || isalnum(str[3])) && (len < 5 || isalnum(str[4])) && (len < 6 || isalnum(str[5])) && (len < 7 || isalnum(str[6]));
}

#ifndef WINDOWS
static void trw_layer_track_use_with_filter ( menu_array_sublayer values )
{
  sg_uid_t uid = (sg_uid_t) ((long) values[MA_SUBLAYER_ID]);
  Track * trk = VIK_TRW_LAYER(values[MA_VTL])->trw->tracks.at(uid);
  a_acquire_set_filter_track(trk);
}
#endif





#ifdef VIK_CONFIG_GOOGLE


bool LayerTRW::is_valid_google_route(sg_uid_t track_uid)
{
	Track * trk = this->routes.at(track_uid);
	return (trk && trk->comment && strlen(trk->comment) > 7 && !strncmp(trk->comment, "from:", 5));
}





static void trw_layer_google_route_webpage ( menu_array_sublayer values )
{
  sg_uid_t uid = (sg_uid_t) ((long) values[MA_SUBLAYER_ID]);
  Track * trk = VIK_TRW_LAYER(values[MA_VTL])->trw->routes.at(uid);
  if (trk) {
    char *escaped = uri_escape ( trk->comment );
    char *webpage = g_strdup_printf("http://maps.google.com/maps?f=q&hl=en&q=%s", escaped );
    open_url(VIK_GTK_WINDOW_FROM_LAYER(VIK_LAYER(values[MA_VTL])), webpage);
    free( escaped );
    free( webpage );
  }
}


#endif





/* vlp can be NULL if necessary - i.e. right-click from a tool */
/* viewpoint is now available instead */
static bool trw_layer_sublayer_add_menu_items ( VikTrwLayer *l, GtkMenu *menu, void * vlp, int subtype, void * sublayer, GtkTreeIter *iter, VikViewport *vvp )
{
  static menu_array_sublayer pass_along;
  sg_uid_t uid = (sg_uid_t) ((long) sublayer);
  GtkWidget *item;
  bool rv = false;

  pass_along[MA_VTL]         = l;
  pass_along[MA_VLP]         = vlp;
  pass_along[MA_SUBTYPE]     = KINT_TO_POINTER (subtype);
  pass_along[MA_SUBLAYER_ID] = sublayer;
  pass_along[MA_CONFIRM]     = KINT_TO_POINTER (1); // Confirm delete request
  pass_along[MA_VVP]         = vvp;
  pass_along[MA_TV_ITER]     = iter;
  pass_along[MA_MISC]        = NULL; // For misc purposes - maybe track or waypoint

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT || subtype == VIK_TRW_LAYER_SUBLAYER_TRACK || subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE )
  {
    rv = true;

    item = gtk_image_menu_item_new_from_stock ( GTK_STOCK_PROPERTIES, NULL );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_properties_item), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    if (subtype == VIK_TRW_LAYER_SUBLAYER_TRACK) {
      Track * trk = l->trw->tracks.at(uid);
      if (trk && trk->property_dialog)
        gtk_widget_set_sensitive(GTK_WIDGET(item), false );
    }
    if (subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE) {
      Track * trk = l->trw->routes.at(uid);
      if (trk && trk->property_dialog)
        gtk_widget_set_sensitive(GTK_WIDGET(item), false );
    }

    item = gtk_image_menu_item_new_from_stock ( GTK_STOCK_CUT, NULL );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_cut_item_cb), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_from_stock ( GTK_STOCK_COPY, NULL );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_copy_item_cb), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_from_stock ( GTK_STOCK_DELETE, NULL );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_item), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT )
    {
      // Always create separator as now there is always at least the transform menu option
      item = gtk_menu_item_new ();
      gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
      gtk_widget_show ( item );

      /* could be a right-click using the tool */
      if ( vlp != NULL ) {
        item = gtk_image_menu_item_new_with_mnemonic ( _("_Goto") );
        gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU) );
        g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_waypoint), pass_along );
        gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
        gtk_widget_show ( item );
      }

      sg_uid_t wp_uid = (sg_uid_t) ((long) sublayer);
      Waypoint * wp = VIK_TRW_LAYER(l)->trw->waypoints.at(wp_uid);

      if ( wp && wp->name ) {
        if ( is_valid_geocache_name ( wp->name ) ) {
          item = gtk_menu_item_new_with_mnemonic ( _("_Visit Geocache Webpage") );
          g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_waypoint_gc_webpage), pass_along );
          gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
          gtk_widget_show ( item );
        }
#ifdef VIK_CONFIG_GEOTAG
        item = gtk_menu_item_new_with_mnemonic ( _("Geotag _Images...") );
        g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_geotagging_waypoint), pass_along );
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
        gtk_widget_set_tooltip_text (item, _("Geotag multiple images against this waypoint"));
        gtk_widget_show ( item );
#endif
      }

      if ( wp && wp->image )
      {
        // Set up image paramater
        pass_along[MA_MISC] = wp->image;

        item = gtk_image_menu_item_new_with_mnemonic ( _("_Show Picture...") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock ("vik-icon-Show Picture", GTK_ICON_SIZE_MENU) ); // Own icon - see stock_icons in vikwindow.c
        g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_show_picture), pass_along );
        gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
        gtk_widget_show ( item );

#ifdef VIK_CONFIG_GEOTAG
	GtkWidget *geotag_submenu = gtk_menu_new ();
	item = gtk_image_menu_item_new_with_mnemonic ( _("Update Geotag on _Image") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU) );
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show ( item );
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), geotag_submenu );

	item = gtk_menu_item_new_with_mnemonic ( _("_Update") );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_geotagging_waypoint_mtime_update), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (geotag_submenu), item);
	gtk_widget_show ( item );

	item = gtk_menu_item_new_with_mnemonic ( _("Update and _Keep File Timestamp") );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_geotagging_waypoint_mtime_keep), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (geotag_submenu), item);
	gtk_widget_show ( item );
#endif
      }

      if ( wp )
      {
        if ( wp->url ||
             ( wp->comment && !strncmp(wp->comment, "http", 4) ) ||
             ( wp->description && !strncmp(wp->description, "http", 4) )) {
          item = gtk_image_menu_item_new_with_mnemonic ( _("Visit _Webpage") );
          gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_NETWORK, GTK_ICON_SIZE_MENU) );
          g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_waypoint_webpage), pass_along );
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          gtk_widget_show ( item );
        }
      }
    }
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINTS || subtype == VIK_TRW_LAYER_SUBLAYER_TRACKS || subtype == VIK_TRW_LAYER_SUBLAYER_ROUTES ) {
    item = gtk_image_menu_item_new_from_stock ( GTK_STOCK_PASTE, NULL );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_paste_item_cb), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );
    // TODO: only enable if suitable item is in clipboard - want to determine *which* sublayer type
    if ( a_clipboard_type ( ) == VIK_CLIPBOARD_DATA_SUBLAYER )
      gtk_widget_set_sensitive ( item, true );
    else
      gtk_widget_set_sensitive ( item, false );

    // Add separator
    item = gtk_menu_item_new ();
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );
  }

  if ( vlp && (subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINTS || subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT) )
  {
    rv = true;
    item = gtk_image_menu_item_new_with_mnemonic ( _("_New Waypoint...") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_new_wp), pass_along );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINTS )
  {
    item = gtk_image_menu_item_new_with_mnemonic ( _("_View All Waypoints") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_auto_waypoints_view), pass_along );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("Goto _Waypoint...") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_wp), pass_along );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("Delete _All Waypoints") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_all_waypoints), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Delete Waypoints From Selection...") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_waypoints_from_selection), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    GtkWidget *vis_submenu = gtk_menu_new ();
    item = gtk_menu_item_new_with_mnemonic ( _("_Visibility") );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), vis_submenu );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Show All Waypoints") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_APPLY, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_waypoints_visibility_on), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(vis_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Hide All Waypoints") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_waypoints_visibility_off), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(vis_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Toggle") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_waypoints_visibility_toggle), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(vis_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_List Waypoints...") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_waypoint_list_dialog), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACKS )
  {
    rv = true;

    if ( l->current_track && !l->current_track->is_route ) {
      item = gtk_menu_item_new_with_mnemonic ( _("_Finish Track") );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_finish_track), pass_along );
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show ( item );
      // Add separator
      item = gtk_menu_item_new ();
      gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
      gtk_widget_show ( item );
    }

    item = gtk_image_menu_item_new_with_mnemonic ( _("_View All Tracks") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_auto_tracks_view), pass_along );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_New Track") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_new_track), pass_along );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    // Make it available only when a new track *not* already in progress
    gtk_widget_set_sensitive ( item, ! (bool)KPOINTER_TO_INT(l->current_track) );

    item = gtk_image_menu_item_new_with_mnemonic ( _("Delete _All Tracks") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_all_tracks), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Delete Tracks From Selection...") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_tracks_from_selection), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    GtkWidget *vis_submenu = gtk_menu_new ();
    item = gtk_menu_item_new_with_mnemonic ( _("_Visibility") );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), vis_submenu );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Show All Tracks") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_APPLY, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_tracks_visibility_on), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(vis_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Hide All Tracks") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_tracks_visibility_off), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(vis_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Toggle") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_tracks_visibility_toggle), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(vis_submenu), item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_List Tracks...") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_track_list_dialog_single), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_menu_item_new_with_mnemonic ( _("_Statistics") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_tracks_stats), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_ROUTES )
  {
    rv = true;

    if ( l->current_track && l->current_track->is_route ) {
      item = gtk_menu_item_new_with_mnemonic ( _("_Finish Route") );
      // Reuse finish track method
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_finish_track), pass_along );
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show ( item );
      // Add separator
      item = gtk_menu_item_new ();
      gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
      gtk_widget_show ( item );
    }

    item = gtk_image_menu_item_new_with_mnemonic ( _("_View All Routes") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_auto_routes_view), pass_along );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_New Route") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_new_route), pass_along );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    // Make it available only when a new track *not* already in progress
    gtk_widget_set_sensitive ( item, ! (bool)KPOINTER_TO_INT(l->current_track) );

    item = gtk_image_menu_item_new_with_mnemonic ( _("Delete _All Routes") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_all_routes), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Delete Routes From Selection...") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_routes_from_selection), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    GtkWidget *vis_submenu = gtk_menu_new ();
    item = gtk_menu_item_new_with_mnemonic ( _("_Visibility") );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), vis_submenu );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Show All Routes") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_APPLY, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_routes_visibility_on), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(vis_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Hide All Routes") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_routes_visibility_off), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(vis_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Toggle") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_routes_visibility_toggle), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(vis_submenu), item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_List Routes...") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_track_list_dialog_single), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );

    gtk_widget_show ( item );

    item = gtk_menu_item_new_with_mnemonic ( _("_Statistics") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_routes_stats), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );
  }


  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINTS || subtype == VIK_TRW_LAYER_SUBLAYER_TRACKS || subtype == VIK_TRW_LAYER_SUBLAYER_ROUTES ) {
    GtkWidget *submenu_sort = gtk_menu_new ();
    item = gtk_image_menu_item_new_with_mnemonic ( _("_Sort") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU) );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu_sort );

    item = gtk_image_menu_item_new_with_mnemonic ( _("Name _Ascending") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_SORT_ASCENDING, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_sort_order_a2z), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(submenu_sort), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("Name _Descending") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_SORT_DESCENDING, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_sort_order_z2a), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(submenu_sort), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("Date Ascending") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_SORT_ASCENDING, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_sort_order_timestamp_ascend), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(submenu_sort), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("Date Descending") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_SORT_DESCENDING, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_sort_order_timestamp_descend), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(submenu_sort), item );
    gtk_widget_show ( item );
  }

  GtkWidget *upload_submenu = gtk_menu_new ();

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK || subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE )
  {
    item = gtk_menu_item_new ();
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    if ( l->current_track && subtype == VIK_TRW_LAYER_SUBLAYER_TRACK && !l->current_track->is_route )
      item = gtk_menu_item_new_with_mnemonic ( _("_Finish Track") );
    if ( l->current_track && subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE && l->current_track->is_route )
      item = gtk_menu_item_new_with_mnemonic ( _("_Finish Route") );
    if ( l->current_track ) {
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_finish_track), pass_along );
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show ( item );

      // Add separator
      item = gtk_menu_item_new ();
      gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
      gtk_widget_show ( item );
    }

    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK )
      item = gtk_image_menu_item_new_with_mnemonic ( _("_View Track") );
    else
      item = gtk_image_menu_item_new_with_mnemonic ( _("_View Route") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_auto_track_view), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_menu_item_new_with_mnemonic ( _("_Statistics") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_track_statistics), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    GtkWidget *goto_submenu;
    goto_submenu = gtk_menu_new ();
    item = gtk_image_menu_item_new_with_mnemonic ( _("_Goto") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU) );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), goto_submenu );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Startpoint") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GOTO_FIRST, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_track_startpoint), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(goto_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("\"_Center\"") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_track_center), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(goto_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Endpoint") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GOTO_LAST, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_track_endpoint), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(goto_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Highest Altitude") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GOTO_TOP, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_track_max_alt), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(goto_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Lowest Altitude") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GOTO_BOTTOM, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_track_min_alt), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(goto_submenu), item );
    gtk_widget_show ( item );

    // Routes don't have speeds
    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
      item = gtk_image_menu_item_new_with_mnemonic ( _("_Maximum Speed") );
      gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_MEDIA_FORWARD, GTK_ICON_SIZE_MENU) );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_track_max_speed), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(goto_submenu), item );
      gtk_widget_show ( item );
    }

    GtkWidget *combine_submenu;
    combine_submenu = gtk_menu_new ();
    item = gtk_image_menu_item_new_with_mnemonic ( _("Co_mbine") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_CONNECT, GTK_ICON_SIZE_MENU) );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), combine_submenu );

    // Routes don't have times or segments...
    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
      item = gtk_menu_item_new_with_mnemonic ( _("_Merge By Time...") );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_merge_by_timestamp), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(combine_submenu), item );
      gtk_widget_show ( item );

      item = gtk_menu_item_new_with_mnemonic ( _("Merge _Segments") );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_merge_by_segment), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(combine_submenu), item );
      gtk_widget_show ( item );
    }

    item = gtk_menu_item_new_with_mnemonic ( _("Merge _With Other Tracks...") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_merge_with_other), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(combine_submenu), item );
    gtk_widget_show ( item );

    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK )
      item = gtk_menu_item_new_with_mnemonic ( _("_Append Track...") );
    else
      item = gtk_menu_item_new_with_mnemonic ( _("_Append Route...") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_append_track), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(combine_submenu), item );
    gtk_widget_show ( item );

    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK )
      item = gtk_menu_item_new_with_mnemonic ( _("Append _Route...") );
    else
      item = gtk_menu_item_new_with_mnemonic ( _("Append _Track...") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_append_other), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(combine_submenu), item );
    gtk_widget_show ( item );

    GtkWidget *split_submenu;
    split_submenu = gtk_menu_new ();
    item = gtk_image_menu_item_new_with_mnemonic ( _("_Split") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_DISCONNECT, GTK_ICON_SIZE_MENU) );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), split_submenu );

    // Routes don't have times or segments...
    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
      item = gtk_menu_item_new_with_mnemonic ( _("_Split By Time...") );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_split_by_timestamp), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(split_submenu), item );
      gtk_widget_show ( item );

      // ATM always enable this entry - don't want to have to analyse the track before displaying the menu - to keep the menu speedy
      item = gtk_menu_item_new_with_mnemonic ( _("Split Se_gments") );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_split_segments), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(split_submenu), item );
      gtk_widget_show ( item );
    }

    item = gtk_menu_item_new_with_mnemonic ( _("Split By _Number of Points...") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_split_by_n_points), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(split_submenu), item );
    gtk_widget_show ( item );

    item = gtk_menu_item_new_with_mnemonic ( _("Split at _Trackpoint") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_split_at_trackpoint), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(split_submenu), item );
    gtk_widget_show ( item );
    // Make it available only when a trackpoint is selected.
    gtk_widget_set_sensitive ( item, (bool)KPOINTER_TO_INT(l->trw->current_tpl) );

    GtkWidget *insert_submenu = gtk_menu_new ();
    item = gtk_image_menu_item_new_with_mnemonic ( _("_Insert Points") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU) );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), insert_submenu );

    item = gtk_menu_item_new_with_mnemonic ( _("Insert Point _Before Selected Point") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_insert_point_before), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(insert_submenu), item );
    gtk_widget_show ( item );
    // Make it available only when a point is selected
    gtk_widget_set_sensitive ( item, (bool)KPOINTER_TO_INT(l->trw->current_tpl) );

    item = gtk_menu_item_new_with_mnemonic ( _("Insert Point _After Selected Point") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_insert_point_after), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(insert_submenu), item );
    gtk_widget_show ( item );
    // Make it available only when a point is selected
    gtk_widget_set_sensitive ( item, (bool)KPOINTER_TO_INT(l->trw->current_tpl) );

    GtkWidget *delete_submenu;
    delete_submenu = gtk_menu_new ();
    item = gtk_image_menu_item_new_with_mnemonic ( _("Delete Poi_nts") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_DELETE, GTK_ICON_SIZE_MENU) );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), delete_submenu );

    item = gtk_image_menu_item_new_with_mnemonic ( _("Delete _Selected Point") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_DELETE, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_point_selected), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(delete_submenu), item );
    gtk_widget_show ( item );
    // Make it available only when a point is selected
    gtk_widget_set_sensitive ( item, (bool)KPOINTER_TO_INT(l->trw->current_tpl) );

    item = gtk_menu_item_new_with_mnemonic ( _("Delete Points With The Same _Position") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_points_same_position), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(delete_submenu), item );
    gtk_widget_show ( item );

    item = gtk_menu_item_new_with_mnemonic ( _("Delete Points With The Same _Time") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_points_same_time), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(delete_submenu), item );
    gtk_widget_show ( item );

    GtkWidget *transform_submenu;
    transform_submenu = gtk_menu_new ();
    item = gtk_image_menu_item_new_with_mnemonic ( _("_Transform") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_CONVERT, GTK_ICON_SIZE_MENU) );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), transform_submenu );

    GtkWidget *dem_submenu;
    dem_submenu = gtk_menu_new ();
    item = gtk_image_menu_item_new_with_mnemonic ( _("_Apply DEM Data") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock ("vik-icon-DEM Download", GTK_ICON_SIZE_MENU) ); // Own icon - see stock_icons in vikwindow.c
    gtk_menu_shell_append ( GTK_MENU_SHELL(transform_submenu), item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), dem_submenu );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Overwrite") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_apply_dem_data_all), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(dem_submenu), item );
    gtk_widget_set_tooltip_text (item, _("Overwrite any existing elevation values with DEM values"));
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Keep Existing") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_apply_dem_data_only_missing), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(dem_submenu), item );
    gtk_widget_set_tooltip_text (item, _("Keep existing elevation values, only attempt for missing values"));
    gtk_widget_show ( item );

    GtkWidget *smooth_submenu;
    smooth_submenu = gtk_menu_new ();
    item = gtk_menu_item_new_with_mnemonic ( _("_Smooth Missing Elevation Data") );
    gtk_menu_shell_append ( GTK_MENU_SHELL(transform_submenu), item );
    gtk_widget_show ( item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), smooth_submenu );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Interpolated") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_missing_elevation_data_interp), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(smooth_submenu), item );
    gtk_widget_set_tooltip_text (item, _("Interpolate between known elevation values to derive values for the missing elevations"));
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Flat") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_missing_elevation_data_flat), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(smooth_submenu), item );
    gtk_widget_set_tooltip_text (item, _("Set unknown elevation values to the last known value"));
    gtk_widget_show ( item );

    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK )
      item = gtk_image_menu_item_new_with_mnemonic ( _("C_onvert to a Route") );
    else
      item = gtk_image_menu_item_new_with_mnemonic ( _("C_onvert to a Track") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_CONVERT, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_convert_track_route), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(transform_submenu), item );
    gtk_widget_show ( item );

    // Routes don't have timestamps - so these are only available for tracks
    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
      item = gtk_image_menu_item_new_with_mnemonic ( _("_Anonymize Times") );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_anonymize_times), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(transform_submenu), item );
      gtk_widget_set_tooltip_text (item, _("Shift timestamps to a relative offset from 1901-01-01"));
      gtk_widget_show ( item );

      item = gtk_image_menu_item_new_with_mnemonic ( _("_Interpolate Times") );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_interpolate_times), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(transform_submenu), item );
      gtk_widget_set_tooltip_text (item, _("Reset trackpoint timestamps between the first and last points such that track is traveled at equal speed"));
      gtk_widget_show ( item );
    }

    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK )
      item = gtk_image_menu_item_new_with_mnemonic ( _("_Reverse Track") );
    else
      item = gtk_image_menu_item_new_with_mnemonic ( _("_Reverse Route") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GO_BACK, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_reverse), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    if ( subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE ) {
      item = gtk_image_menu_item_new_with_mnemonic ( _("Refine Route...") );
      gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_FIND, GTK_ICON_SIZE_MENU) );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_route_refine), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
      gtk_widget_show ( item );
    }

    /* ATM This function is only available via the layers panel, due to the method in finding out the maps in use */
    if ( vlp ) {
      if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK )
        item = gtk_image_menu_item_new_with_mnemonic ( _("Down_load Maps Along Track...") );
      else
        item = gtk_image_menu_item_new_with_mnemonic ( _("Down_load Maps Along Route...") );
      gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock ("vik-icon-Maps Download", GTK_ICON_SIZE_MENU) ); // Own icon - see stock_icons in vikwindow.c
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_download_map_along_track_cb), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
      gtk_widget_show ( item );
    }

    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK )
      item = gtk_image_menu_item_new_with_mnemonic ( _("_Export Track as GPX...") );
    else
      item = gtk_image_menu_item_new_with_mnemonic ( _("_Export Route as GPX...") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_HARDDISK, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_export_gpx_track), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK )
      item = gtk_image_menu_item_new_with_mnemonic ( _("E_xtend Track End") );
    else
      item = gtk_image_menu_item_new_with_mnemonic ( _("E_xtend Route End") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_extend_track_end), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    if ( subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE ) {
      item = gtk_image_menu_item_new_with_mnemonic ( _("Extend _Using Route Finder") );
      gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock ("vik-icon-Route Finder", GTK_ICON_SIZE_MENU) ); // Own icon - see stock_icons in vikwindow.c
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_extend_track_end_route_finder), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
      gtk_widget_show ( item );
    }

    // ATM can't upload a single waypoint but can do waypoints to a GPS
    if ( subtype != VIK_TRW_LAYER_SUBLAYER_WAYPOINT ) {
      item = gtk_image_menu_item_new_with_mnemonic ( _("_Upload") );
      gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GO_UP, GTK_ICON_SIZE_MENU) );
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show ( item );
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), upload_submenu );

      item = gtk_image_menu_item_new_with_mnemonic ( _("_Upload to GPS...") );
      gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GO_FORWARD, GTK_ICON_SIZE_MENU) );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_gps_upload_any), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(upload_submenu), item );
      gtk_widget_show ( item );
    }
  }

  GtkWidget *external_submenu = create_external_submenu ( menu );

  // These are only made available if a suitable program is installed
  if ( (have_astro_program || have_diary_program) &&
       (subtype == VIK_TRW_LAYER_SUBLAYER_TRACK || subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT) ) {

    if ( have_diary_program ) {
      item = gtk_image_menu_item_new_with_mnemonic ( _("_Diary") );
      gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_SPELL_CHECK, GTK_ICON_SIZE_MENU) );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_diary), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(external_submenu), item );
      gtk_widget_set_tooltip_text (item, _("Open diary program at this date"));
      gtk_widget_show ( item );
    }

    if ( have_astro_program ) {
      item = gtk_image_menu_item_new_with_mnemonic ( _("_Astronomy") );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_astro), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(external_submenu), item );
      gtk_widget_set_tooltip_text (item, _("Open astronomy program at this date and location"));
      gtk_widget_show ( item );
    }
  }

  if ( l->trw->current_tpl || l->trw->current_wp ) {
    // For the selected point
    VikCoord *vc;
    if ( l->trw->current_tpl )
      vc = &(((Trackpoint *) l->trw->current_tpl->data)->coord);
    else
      vc = &(l->trw->current_wp->coord);
    vik_ext_tools_add_menu_items_to_menu ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(l)), GTK_MENU (external_submenu), vc );
  }
  else {
    // Otherwise for the selected sublayer
    // TODO: Should use selected items centre - rather than implicitly using the current viewport
    vik_ext_tools_add_menu_items_to_menu ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(l)), GTK_MENU (external_submenu), NULL );
  }


#ifdef VIK_CONFIG_GOOGLE
  if ( subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE &&  ( l->trw->is_valid_google_route((sg_uid_t) ((long) sublayer)) ))
  {
    item = gtk_image_menu_item_new_with_mnemonic ( _("_View Google Directions") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_NETWORK, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_google_route_webpage), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );
  }
#endif

  // Some things aren't usable with routes
  if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
#ifdef VIK_CONFIG_OPENSTREETMAP
    item = gtk_image_menu_item_new_with_mnemonic ( _("Upload to _OSM...") );
    // Convert internal pointer into track
    sg_uid_t uid = (sg_uid_t) ((long) sublayer);
    pass_along[MA_MISC] = l->trw->tracks.at(uid);
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GO_UP, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_osm_traces_upload_track_cb), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(upload_submenu), item );
    gtk_widget_show ( item );
#endif

    // Currently filter with functions all use shellcommands and thus don't work in Windows
#ifndef WINDOWS
    item = gtk_image_menu_item_new_with_mnemonic ( _("Use with _Filter") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_track_use_with_filter), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );
#endif

    /* ATM This function is only available via the layers panel, due to needing a vlp */
    if ( vlp ) {
      sg_uid_t uid = (sg_uid_t) ((long) sublayer);
      VikLayersPanel * vlp_ = VIK_LAYERS_PANEL(vlp);
      item = a_acquire_track_menu ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(l)), (VikLayersPanel *) vlp,
                                    (VikViewport *) vlp_->panel_ref->get_viewport()->vvp,
                                    l->trw->tracks.at(uid));
      if ( item ) {
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
        gtk_widget_show ( item );
      }
    }

#ifdef VIK_CONFIG_GEOTAG
    item = gtk_menu_item_new_with_mnemonic ( _("Geotag _Images...") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_geotagging_track), pass_along );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
#endif
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK || subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE ) {
    // Only show on viewport popmenu when a trackpoint is selected
    if ( ! vlp && l->trw->current_tpl ) {
      // Add separator
      item = gtk_menu_item_new ();
      gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
      gtk_widget_show ( item );

      item = gtk_image_menu_item_new_with_mnemonic ( _("_Edit Trackpoint") );
      gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_PROPERTIES, GTK_ICON_SIZE_MENU) );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_edit_trackpoint), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
      gtk_widget_show ( item );
    }
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINTS || subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT ) {
    GtkWidget *transform_submenu;
    transform_submenu = gtk_menu_new ();
    item = gtk_image_menu_item_new_with_mnemonic ( _("_Transform") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_CONVERT, GTK_ICON_SIZE_MENU) );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), transform_submenu );

    GtkWidget *dem_submenu;
    dem_submenu = gtk_menu_new ();
    item = gtk_image_menu_item_new_with_mnemonic ( _("_Apply DEM Data") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock ("vik-icon-DEM Download", GTK_ICON_SIZE_MENU) ); // Own icon - see stock_icons in vikwindow.c
    gtk_menu_shell_append ( GTK_MENU_SHELL(transform_submenu), item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), dem_submenu );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Overwrite") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_apply_dem_data_wpt_all), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(dem_submenu), item );
    gtk_widget_set_tooltip_text (item, _("Overwrite any existing elevation values with DEM values"));
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Keep Existing") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_apply_dem_data_wpt_only_missing), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(dem_submenu), item );
    gtk_widget_set_tooltip_text (item, _("Keep existing elevation values, only attempt for missing values"));
    gtk_widget_show ( item );
  }

  gtk_widget_show_all ( GTK_WIDGET(menu) );

  return rv;
}





// TODO: Probably better to rework this track manipulation in viktrack.c
void LayerTRW::insert_tp_beside_current_tp(bool before)
{
	// sanity check
	if (!this->current_tpl) {
		return;
	}

	Trackpoint * tp_current = (Trackpoint *) this->current_tpl->data;
	Trackpoint * tp_other = NULL;

	if (before) {
		if (!this->current_tpl->prev) {
			return;
		}
		tp_other = ((Trackpoint *) this->current_tpl->prev->data);
	} else {
		if (!this->current_tpl->next) {
			return;
		}
		tp_other = ((Trackpoint *) this->current_tpl->next->data);
	}

	// kamil
	// Use current and other trackpoints to form a new track point which is inserted into the tracklist
	if (tp_other) {

		Trackpoint * tp_new = new Trackpoint();
		struct LatLon ll_current, ll_other;
		vik_coord_to_latlon(&tp_current->coord, &ll_current);
		vik_coord_to_latlon(&tp_other->coord, &ll_other);

		/* main positional interpolation */
		struct LatLon ll_new = { (ll_current.lat + ll_other.lat)/2, (ll_current.lon + ll_other.lon)/2 };
		vik_coord_load_from_latlon(&(tp_new->coord), this->coord_mode, &ll_new);

		/* Now other properties that can be interpolated */
		tp_new->altitude = (tp_current->altitude + tp_other->altitude) / 2;

		if (tp_current->has_timestamp && tp_other->has_timestamp) {
			/* Note here the division is applied to each part, then added
			   This is to avoid potential overflow issues with a 32 time_t for dates after midpoint of this Unix time on 2004/01/04 */
			tp_new->timestamp = (tp_current->timestamp/2) + (tp_other->timestamp/2);
			tp_new->has_timestamp = true;
		}

		if (tp_current->speed != NAN && tp_other->speed != NAN)
			tp_new->speed = (tp_current->speed + tp_other->speed)/2;

		/* TODO - improve interpolation of course, as it may not be correct.
		   if courses in degrees are 350 + 020, the mid course more likely to be 005 (not 185)
		   [similar applies if value is in radians] */
		if (tp_current->course != NAN && tp_other->course != NAN) {
			tp_new->course = (tp_current->course + tp_other->course)/2;
		}

		/* DOP / sat values remain at defaults as not they do not seem applicable to a dreamt up point */

		// Insert new point into the appropriate trackpoint list, either before or after the current trackpoint as directed
		Track * trk = this->tracks.at(this->current_tp_uid);
		if (!trk) {
			// Otherwise try routes
			trk = this->routes.at(this->current_tp_uid);
		}

		if (!trk) {
			return;
		}

		int index =  g_list_index(trk->trackpoints, tp_current);
		if (index > -1) {
			if (!before) {
				index = index + 1;
			}
			// NB no recalculation of bounds since it is inserted between points
			trk->trackpoints = g_list_insert(trk->trackpoints, tp_new, index);
		}
	}
}





static void trw_layer_cancel_current_tp ( VikTrwLayer *vtl, bool destroy )
{
  if ( vtl->trw->tpwin )
  {
    if ( destroy)
    {
      gtk_widget_destroy ( GTK_WIDGET(vtl->trw->tpwin) );
      vtl->trw->tpwin = NULL;
		}
    else
      vik_trw_layer_tpwin_set_empty ( vtl->trw->tpwin );
	}
  if ( vtl->trw->current_tpl )
  {
    vtl->trw->current_tpl = NULL;
    vtl->trw->current_tp_track = NULL;
    vtl->trw->current_tp_uid = 0;
    vik_layer_emit_update(VIK_LAYER(vtl));
	}
}





void LayerTRW::my_tpwin_set_tp()
{
	Track * trk = this->current_tp_track;
	VikCoord vc;
	// Notional center of a track is simply an average of the bounding box extremities
	struct LatLon center = { (trk->bbox.north+trk->bbox.south)/2, (trk->bbox.east+trk->bbox.west)/2 };
	vik_coord_load_from_latlon(&vc, this->coord_mode, &center);
	vik_trw_layer_tpwin_set_tp(this->tpwin, this->current_tpl, trk->name, this->current_tp_track->is_route);
}





static void trw_layer_tpwin_response ( VikTrwLayer *vtl, int response )
{
  assert ( vtl->trw->tpwin != NULL );
  if ( response == VIK_TRW_LAYER_TPWIN_CLOSE )
    trw_layer_cancel_current_tp ( vtl, true );

  if ( vtl->trw->current_tpl == NULL )
    return;

  if ( response == VIK_TRW_LAYER_TPWIN_SPLIT && vtl->trw->current_tpl->next && vtl->trw->current_tpl->prev )
  {
    vtl->trw->split_at_selected_trackpoint(vtl->trw->current_tp_track->is_route ? VIK_TRW_LAYER_SUBLAYER_ROUTE : VIK_TRW_LAYER_SUBLAYER_TRACK);
    vtl->trw->my_tpwin_set_tp();
  }
  else if ( response == VIK_TRW_LAYER_TPWIN_DELETE )
  {
    Track *tr = vtl->trw->tracks.at(vtl->trw->current_tp_uid);
    if ( tr == NULL )
      tr = vtl->trw->routes.at(vtl->trw->current_tp_uid);
    if ( tr == NULL )
      return;

    vtl->trw->trackpoint_selected_delete(tr);

    if ( vtl->trw->current_tpl )
      // Reset dialog with the available adjacent trackpoint
      vtl->trw->my_tpwin_set_tp();

    vik_layer_emit_update(VIK_LAYER(vtl));
  }
  else if ( response == VIK_TRW_LAYER_TPWIN_FORWARD && vtl->trw->current_tpl->next )
  {
    if ( vtl->trw->current_tp_track ) {
      vtl->trw->current_tpl = vtl->trw->current_tpl->next;
      vtl->trw->my_tpwin_set_tp();
    }
    vik_layer_emit_update(VIK_LAYER(vtl)); /* TODO longone: either move or only update if tp is inside drawing window */
  }
  else if ( response == VIK_TRW_LAYER_TPWIN_BACK && vtl->trw->current_tpl->prev )
  {
    if ( vtl->trw->current_tp_track ) {
      vtl->trw->current_tpl = vtl->trw->current_tpl->prev;
      vtl->trw->my_tpwin_set_tp();
    }
    vik_layer_emit_update(VIK_LAYER(vtl));
  }
  else if ( response == VIK_TRW_LAYER_TPWIN_INSERT && vtl->trw->current_tpl->next )
  {
    vtl->trw->insert_tp_beside_current_tp(false);
    vik_layer_emit_update(VIK_LAYER(vtl));
  }
  else if ( response == VIK_TRW_LAYER_TPWIN_DATA_CHANGED )
    vik_layer_emit_update(VIK_LAYER(vtl));
}





/**
 * trw_layer_dialog_shift:
 * @vertical: The reposition strategy. If Vertical moves dialog vertically, otherwise moves it horizontally
 *
 * Try to reposition a dialog if it's over the specified coord
 *  so to not obscure the item of interest
 */
void LayerTRW::dialog_shift(GtkWindow * dialog, VikCoord * coord, bool vertical)
{
	GtkWindow * parent = VIK_GTK_WINDOW_FROM_LAYER(this->vl); //i.e. the main window

	VikTrwLayer * vtl = (VikTrwLayer *) this->vl;

	// Attempt force dialog to be shown so we can find out where it is more reliably...
	while (gtk_events_pending()) {
		gtk_main_iteration();
	}

	// get parent window position & size
	int win_pos_x, win_pos_y;
	gtk_window_get_position(parent, &win_pos_x, &win_pos_y);

	int win_size_x, win_size_y;
	gtk_window_get_size(parent, &win_size_x, &win_size_y);

	// get own dialog size
	int dia_size_x, dia_size_y;
	gtk_window_get_size(dialog, &dia_size_x, &dia_size_y);

	// get own dialog position
	int dia_pos_x, dia_pos_y;
	gtk_window_get_position(dialog, &dia_pos_x, &dia_pos_y);

	// Dialog not 'realized'/positioned - so can't really do any repositioning logic
	if (dia_pos_x > 2 && dia_pos_y > 2) {

		VikViewport * vvp = vik_window_viewport(VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)));

		int vp_xx, vp_yy; // In viewport pixels
		vvp->port.coord_to_screen(coord, &vp_xx, &vp_yy);

		// Work out the 'bounding box' in pixel terms of the dialog and only move it when over the position

		int dest_x = 0;
		int dest_y = 0;
		if (gtk_widget_translate_coordinates(GTK_WIDGET(vvp), GTK_WIDGET(parent), 0, 0, &dest_x, &dest_y)) {

			// Transform Viewport pixels into absolute pixels
			int tmp_xx = vp_xx + dest_x + win_pos_x - 10;
			int tmp_yy = vp_yy + dest_y + win_pos_y - 10;

			// Is dialog over the point (to within an  ^^ edge value)
			if ( (tmp_xx > dia_pos_x) && (tmp_xx < (dia_pos_x + dia_size_x)) &&
			     (tmp_yy > dia_pos_y) && (tmp_yy < (dia_pos_y + dia_size_y)) ) {

				if (vertical) {
					// Shift up<->down
					int hh = vvp->port.get_height();

					// Consider the difference in viewport to the full window
					int offset_y = dest_y;
					// Add difference between dialog and window sizes
					offset_y += win_pos_y + (hh/2 - dia_size_y)/2;

					if (vp_yy > hh/2) {
						// Point in bottom half, move window to top half
						gtk_window_move ( dialog, dia_pos_x, offset_y );
					} else {
						// Point in top half, move dialog down
						gtk_window_move(dialog, dia_pos_x, hh/2 + offset_y);
					}
				} else {
					// Shift left<->right
					int ww = vvp->port.get_width();

					// Consider the difference in viewport to the full window
					int offset_x = dest_x;
					// Add difference between dialog and window sizes
					offset_x += win_pos_x + (ww/2 - dia_size_x)/2;

					if (vp_xx > ww/2) {
						// Point on right, move window to left
						gtk_window_move(dialog, offset_x, dia_pos_y);
					} else {
						// Point on left, move right
						gtk_window_move(dialog, ww/2 + offset_x, dia_pos_y);
					}
				}
			}
		}
	}
}





void LayerTRW::tpwin_init()
{
	if (!this->tpwin) {
		VikTrwLayer * vtl = (VikTrwLayer *) this->vl;

		this->tpwin = vik_trw_layer_tpwin_new(VIK_GTK_WINDOW_FROM_LAYER(vtl));
		g_signal_connect_swapped(GTK_DIALOG(this->tpwin), "response", G_CALLBACK(trw_layer_tpwin_response), vtl);
		/* connect signals -- DELETE SIGNAL VERY IMPORTANT TO SET TO NULL */
		g_signal_connect_swapped(this->tpwin, "delete-event", G_CALLBACK(trw_layer_cancel_current_tp), vtl);

		gtk_widget_show_all(GTK_WIDGET(this->tpwin));

		if (this->current_tpl) {
			// get tp pixel position
			Trackpoint * tp = (Trackpoint *) this->current_tpl->data;

			// Shift up<->down to try not to obscure the trackpoint.
			this->dialog_shift(GTK_WINDOW(this->tpwin), &(tp->coord), true);
		}
	}

	if (this->current_tpl) {
		if (this->current_tp_track) {
			this->my_tpwin_set_tp();
		}
	}
	/* set layer name and TP data */
}





/***************************************************************************
 ** Tool code
 ***************************************************************************/

/*** Utility data structures and functions ****/





// ATM: Leave this as 'Track' only.
//  Not overly bothered about having a snap to route trackpoint capability
Trackpoint * LayerTRW::closest_tp_in_five_pixel_interval(Viewport * viewport, int x, int y)
{
	TPSearchParams params;

	params.x = x;
	params.y = y;
	params.viewport = viewport;
	params.closest_track_uid = 0;
	params.closest_tp = NULL;

	params.viewport->get_min_max_lat_lon(&(params.bbox.south), &(params.bbox.north), &(params.bbox.west), &(params.bbox.east) );

	LayerTRW::track_search_closest_tp(this->tracks, &params);

	return params.closest_tp;
}





Waypoint * LayerTRW::closest_wp_in_five_pixel_interval(Viewport * viewport, int x, int y)
{
	WPSearchParams params;

	params.x = x;
	params.y = y;
	params.viewport = viewport;
	params.draw_images = ((VikTrwLayer *) this->vl)->drawimages;
	params.closest_wp = NULL;
	params.closest_wp_uid = 0;

	LayerTRW::waypoint_search_closest_tp(this->waypoints, &params);

	return params.closest_wp;
}





// Some forward declarations
static void marker_begin_move ( tool_ed_t *t, int x, int y );
static void marker_moveto ( tool_ed_t *t, int x, int y );
static void marker_end_move ( tool_ed_t *t );
//

static bool trw_layer_select_move ( VikTrwLayer *vtl, GdkEventMotion *event, Viewport * viewport, tool_ed_t* t )
{
  if ( t->holding ) {
    VikCoord new_coord;
    viewport->screen_to_coord(event->x, event->y, &new_coord);

    // Here always allow snapping back to the original location
    //  this is useful when one decides not to move the thing afterall
    // If one wants to move the item only a little bit then don't hold down the 'snap' key!

    // snap to TP
    if ( event->state & GDK_CONTROL_MASK )
    {
      Trackpoint * tp = vtl->trw->closest_tp_in_five_pixel_interval(viewport, event->x, event->y);
      if ( tp )
        new_coord = tp->coord;
    }

    // snap to WP
    if ( event->state & GDK_SHIFT_MASK )
    {
      Waypoint * wp = vtl->trw->closest_wp_in_five_pixel_interval(viewport, event->x, event->y);
      if ( wp )
        new_coord = wp->coord;
    }

    int x, y;
    viewport->coord_to_screen(&new_coord, &x, &y );

    marker_moveto ( t, x, y );

    return true;
  }
  return false;
}

static bool trw_layer_select_release ( VikTrwLayer *vtl, GdkEventButton *event, Viewport * viewport, tool_ed_t* t )
{
  if ( t->holding && event->button == 1 )
  {
    // Prevent accidental (small) shifts when specific movement has not been requested
    //  (as the click release has occurred within the click object detection area)
    if ( !t->moving )
      return false;

    VikCoord new_coord;
    viewport->screen_to_coord(event->x, event->y, &new_coord );

    // snap to TP
    if ( event->state & GDK_CONTROL_MASK )
    {
      Trackpoint * tp = vtl->trw->closest_tp_in_five_pixel_interval(viewport, event->x, event->y);
      if ( tp )
        new_coord = tp->coord;
    }

    // snap to WP
    if ( event->state & GDK_SHIFT_MASK )
    {
      Waypoint * wp = vtl->trw->closest_wp_in_five_pixel_interval(viewport, event->x, event->y);
      if ( wp )
        new_coord = wp->coord;
    }

    marker_end_move ( t );

    // Determine if working on a waypoint or a trackpoint
    if ( t->is_waypoint ) {
      // Update waypoint position
      vtl->trw->current_wp->coord = new_coord;
      vtl->trw->calculate_bounds_waypoints();
      // Reset waypoint pointer
      vtl->trw->current_wp    = NULL;
      vtl->trw->current_wp_uid = 0;
    }
    else {
      if ( vtl->trw->current_tpl ) {
        ((Trackpoint *) vtl->trw->current_tpl->data)->coord = new_coord;

        if ( vtl->trw->current_tp_track )
          vtl->trw->current_tp_track->calculate_bounds();

        if ( vtl->trw->tpwin )
          if ( vtl->trw->current_tp_track )
            vtl->trw->my_tpwin_set_tp();
        // NB don't reset the selected trackpoint, thus ensuring it's still in the tpwin
      }
    }

    vik_layer_emit_update ( VIK_LAYER(vtl) );
    return true;
  }
  return false;
}





/*
  Returns true if a waypoint or track is found near the requested event position for this particular layer
  The item found is automatically selected
  This is a tool like feature but routed via the layer interface, since it's instigated by a 'global' layer tool in vikwindow.c
 */
static bool trw_layer_select_click(VikTrwLayer * vtl, GdkEventButton * event, Viewport * viewport, tool_ed_t * tet)
{
	if (event->button != 1) {
		return false;
	}

	if (!vtl || vtl->vl.type != VIK_LAYER_TRW) {
		return false;
	}

	if (!vtl->trw->tracks_visible && !vtl->trw->waypoints_visible && !vtl->trw->routes_visible) {
		return false;
	}

	LatLonBBox bbox;
	viewport->get_min_max_lat_lon(&(bbox.south), &(bbox.north), &(bbox.west), &(bbox.east) );

	// Go for waypoints first as these often will be near a track, but it's likely the wp is wanted rather then the track

	if (vtl->trw->waypoints_visible && BBOX_INTERSECT (vtl->waypoints_bbox, bbox)) {
		WPSearchParams wp_params;
		wp_params.viewport = viewport;
		wp_params.x = event->x;
		wp_params.y = event->y;
		wp_params.draw_images = vtl->drawimages;
		wp_params.closest_wp_uid = 0;
		wp_params.closest_wp = NULL;

		LayerTRW::waypoint_search_closest_tp(vtl->trw->waypoints, &wp_params);

		if (wp_params.closest_wp) {

			// Select
			vik_treeview_select_iter(VIK_LAYER(vtl)->vt, vtl->trw->waypoints_iters.at(wp_params.closest_wp_uid), true);

			// Too easy to move it so must be holding shift to start immediately moving it
			//   or otherwise be previously selected but not have an image (otherwise clicking within image bounds (again) moves it)
			if (event->state & GDK_SHIFT_MASK
			    || (vtl->trw->current_wp == wp_params.closest_wp && !vtl->trw->current_wp->image)) {
				// Put into 'move buffer'
				// NB vvp & vw already set in tet
				tet->vtl = (void **) vtl;
				tet->is_waypoint = true;

				marker_begin_move(tet, event->x, event->y);
			}

			vtl->trw->current_wp =     wp_params.closest_wp;
			vtl->trw->current_wp_uid = wp_params.closest_wp_uid;

			if (event->type == GDK_2BUTTON_PRESS) {
				if (vtl->trw->current_wp->image) {
					menu_array_sublayer values;
					values[MA_VTL] = vtl;
					values[MA_MISC] = vtl->trw->current_wp->image;
					trw_layer_show_picture(values);
				}
			}

			vik_layer_emit_update(VIK_LAYER(vtl));

			return true;
		}
	}

	// Used for both track and route lists
	TPSearchParams tp_params;
	tp_params.viewport = viewport;
	tp_params.x = event->x;
	tp_params.y = event->y;
	tp_params.closest_track_uid = 0;
	tp_params.closest_tp = NULL;
	tp_params.closest_tpl = NULL;
	tp_params.bbox = bbox;

	if (vtl->trw->tracks_visible) {
		LayerTRW::track_search_closest_tp(vtl->trw->tracks, &tp_params);

		if (tp_params.closest_tp) {

			// Always select + highlight the track
			vik_treeview_select_iter(VIK_LAYER(vtl)->vt, vtl->trw->tracks_iters.at(tp_params.closest_track_uid), true);

			tet->is_waypoint = false;

			// Select the Trackpoint
			// Can move it immediately when control held or it's the previously selected tp
			if (event->state & GDK_CONTROL_MASK
			     || vtl->trw->current_tpl == tp_params.closest_tpl) {
				// Put into 'move buffer'
				// NB vvp & vw already set in tet
				tet->vtl = (void **) vtl;
				marker_begin_move(tet, event->x, event->y);
			}

			vtl->trw->current_tpl = tp_params.closest_tpl;
			vtl->trw->current_tp_uid = tp_params.closest_track_uid;
			vtl->trw->current_tp_track = vtl->trw->tracks.at(tp_params.closest_track_uid);

			vtl->trw->set_statusbar_msg_info_trkpt(tp_params.closest_tp);

			if (vtl->trw->tpwin) {
				vtl->trw->my_tpwin_set_tp();
			}

			vik_layer_emit_update(VIK_LAYER(vtl));
			return true;
		}
	}

	// Try again for routes
	if (vtl->trw->routes_visible) {
		LayerTRW::track_search_closest_tp(vtl->trw->routes, &tp_params);

		if (tp_params.closest_tp)  {

			// Always select + highlight the track
			vik_treeview_select_iter(VIK_LAYER(vtl)->vt, vtl->trw->routes_iters.at(tp_params.closest_track_uid), true);

			tet->is_waypoint = false;

			// Select the Trackpoint
			// Can move it immediately when control held or it's the previously selected tp
			if (event->state & GDK_CONTROL_MASK ||
			     vtl->trw->current_tpl == tp_params.closest_tpl) {
				// Put into 'move buffer'
				// NB vvp & vw already set in tet
				tet->vtl = (void **) vtl;
				marker_begin_move(tet, event->x, event->y);
			}

			vtl->trw->current_tpl = tp_params.closest_tpl;
			vtl->trw->current_tp_uid = tp_params.closest_track_uid;
			vtl->trw->current_tp_track = vtl->trw->routes.at(tp_params.closest_track_uid);

			vtl->trw->set_statusbar_msg_info_trkpt(tp_params.closest_tp);

			if (vtl->trw->tpwin) {
				vtl->trw->my_tpwin_set_tp();
			}

			vik_layer_emit_update(VIK_LAYER(vtl));
			return true;
		}
	}

	/* these aren't the droids you're looking for */
	vtl->trw->current_wp    = NULL;
	vtl->trw->current_wp_uid = 0;
	trw_layer_cancel_current_tp(vtl, false);

	// Blank info
	vik_statusbar_set_message(vik_window_get_statusbar(VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl))), VIK_STATUSBAR_INFO, "");

	return false;
}

static bool trw_layer_show_selected_viewport_menu ( VikTrwLayer *vtl, GdkEventButton *event, Viewport * viewport)
{
  if ( event->button != 3 )
    return false;

  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return false;

  if ( !vtl->trw->tracks_visible && !vtl->trw->waypoints_visible && !vtl->trw->routes_visible )
    return false;

  /* Post menu for the currently selected item */

  /* See if a track is selected */
  Track * trk = (Track *) vik_window_get_selected_track ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(vtl) );
  if ( trk && trk->visible ) {

    if ( trk->name ) {

      if ( vtl->track_right_click_menu )
        g_object_ref_sink ( G_OBJECT(vtl->track_right_click_menu) );

      vtl->track_right_click_menu = GTK_MENU ( gtk_menu_new () );

      sg_uid_t uid = 0;;
      if ( trk->is_route )
	uid = LayerTRW::find_uid_of_track(vtl->trw->routes, trk);
      else
	uid = LayerTRW::find_uid_of_track(vtl->trw->tracks, trk);

      if (uid) {

        GtkTreeIter *iter;
        if ( trk->is_route )
	  iter = vtl->trw->routes_iters.at(uid);
        else
	  iter = vtl->trw->tracks_iters.at(uid);

        trw_layer_sublayer_add_menu_items ( vtl,
                                            vtl->track_right_click_menu,
                                            NULL,
                                            trk->is_route ? VIK_TRW_LAYER_SUBLAYER_ROUTE : VIK_TRW_LAYER_SUBLAYER_TRACK,
                                            (void *) ((long) uid),
                                            iter,
                                            (VikViewport *) viewport->vvp );
      }

      gtk_menu_popup ( vtl->track_right_click_menu, NULL, NULL, NULL, NULL, event->button, gtk_get_current_event_time() );

      return true;
    }
  }

  /* See if a waypoint is selected */
  Waypoint * waypoint = (Waypoint*)vik_window_get_selected_waypoint ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(vtl) );
  if ( waypoint && waypoint->visible ) {
    if ( waypoint->name ) {

      if ( vtl->wp_right_click_menu )
        g_object_ref_sink ( G_OBJECT(vtl->wp_right_click_menu) );

      vtl->wp_right_click_menu = GTK_MENU ( gtk_menu_new () );

      sg_uid_t wp_uid = LayerTRW::find_uid_of_waypoint(vtl->trw->waypoints, waypoint);
      if (wp_uid) {
	GtkTreeIter * iter = vtl->trw->waypoints_iters.at(wp_uid);

        trw_layer_sublayer_add_menu_items ( vtl,
                                            vtl->wp_right_click_menu,
                                            NULL,
                                            VIK_TRW_LAYER_SUBLAYER_WAYPOINT,
                                            (void *) ((long) wp_uid),
                                            iter,
                                            (VikViewport *) viewport->vvp );
      }
      gtk_menu_popup ( vtl->wp_right_click_menu, NULL, NULL, NULL, NULL, event->button, gtk_get_current_event_time() );

      return true;
    }
  }

  return false;
}

/* background drawing hook, to be passed the viewport */
static bool tool_sync_done = true;

static int tool_sync(void * data)
{
  Viewport * viewport = (Viewport *) data;
  gdk_threads_enter();
  viewport->sync();
  tool_sync_done = true;
  gdk_threads_leave();
  return 0;
}

static void marker_begin_move ( tool_ed_t *t, int x, int y )
{
  t->holding = true;
  t->gc = t->viewport->new_gc("black", 2);
  gdk_gc_set_function ( t->gc, GDK_INVERT );
  t->viewport->draw_rectangle(t->gc, false, x-3, y-3, 6, 6 );
  t->viewport->sync();
  t->oldx = x;
  t->oldy = y;
  t->moving = false;
}

static void marker_moveto ( tool_ed_t *t, int x, int y )
{
  t->viewport->draw_rectangle(t->gc, false, t->oldx-3, t->oldy-3, 6, 6 );
  t->viewport->draw_rectangle(t->gc, false, x-3, y-3, 6, 6 );
  t->oldx = x;
  t->oldy = y;
  t->moving = true;

  if (tool_sync_done) {
    g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, tool_sync, t->viewport, NULL);
    tool_sync_done = false;
  }
}

static void marker_end_move ( tool_ed_t *t )
{
  t->viewport->draw_rectangle(t->gc, false, t->oldx-3, t->oldy-3, 6, 6 );
  g_object_unref ( t->gc );
  t->holding = false;
  t->moving = false;
}

/*** Edit waypoint ****/

static void * tool_edit_waypoint_create ( VikWindow *vw, Viewport * viewport)
{
  tool_ed_t *t = (tool_ed_t *) malloc(1 * sizeof (tool_ed_t));
  t->viewport = viewport;
  t->holding = false;
  return t;
}

static void tool_edit_waypoint_destroy ( tool_ed_t *t )
{
  free( t );
}

static bool tool_edit_waypoint_click ( VikTrwLayer *vtl, GdkEventButton *event, void * data )
{
  WPSearchParams params;
  tool_ed_t *t = (tool_ed_t *) data;

  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return false;

  if ( t->holding ) {
    return true;
  }

  if ( !vtl->vl.visible || !vtl->trw->waypoints_visible )
    return false;

  if ( vtl->trw->current_wp && vtl->trw->current_wp->visible )
  {
    /* first check if current WP is within area (other may be 'closer', but we want to move the current) */
    int x, y;
    t->viewport->coord_to_screen(&(vtl->trw->current_wp->coord), &x, &y );

    if ( abs(x - (int)round(event->x)) <= WAYPOINT_SIZE_APPROX &&
         abs(y - (int)round(event->y)) <= WAYPOINT_SIZE_APPROX )
    {
      if ( event->button == 3 )
        vtl->trw->waypoint_rightclick = true; /* remember that we're clicking; other layers will ignore release signal */
      else {
	marker_begin_move(t, event->x, event->y);
      }
      return true;
    }
  }

  params.viewport = t->viewport;
  params.x = event->x;
  params.y = event->y;
  params.draw_images = vtl->drawimages;
  params.closest_wp_uid = 0;
  params.closest_wp = NULL;
  LayerTRW::waypoint_search_closest_tp(vtl->trw->waypoints, &params);
  if ( vtl->trw->current_wp && (vtl->trw->current_wp == params.closest_wp) )
  {
    if ( event->button == 3 )
      vtl->trw->waypoint_rightclick = true; /* remember that we're clicking; other layers will ignore release signal */
    else
      marker_begin_move(t, event->x, event->y);
    return false;
  }
  else if ( params.closest_wp )
  {
    if ( event->button == 3 )
      vtl->trw->waypoint_rightclick = true; /* remember that we're clicking; other layers will ignore release signal */
    else
      vtl->trw->waypoint_rightclick = false;

    vik_treeview_select_iter ( VIK_LAYER(vtl)->vt, vtl->trw->waypoints_iters.at(params.closest_wp_uid), true );

    vtl->trw->current_wp = params.closest_wp;
    vtl->trw->current_wp_uid = params.closest_wp_uid;

    /* could make it so don't update if old WP is off screen and new is null but oh well */
    vik_layer_emit_update ( VIK_LAYER(vtl) );
    return true;
  }

  vtl->trw->current_wp = NULL;
  vtl->trw->current_wp_uid = 0;
  vtl->trw->waypoint_rightclick = false;
  vik_layer_emit_update ( VIK_LAYER(vtl) );
  return false;
}

static bool tool_edit_waypoint_move ( VikTrwLayer *vtl, GdkEventMotion *event, void * data )
{
  tool_ed_t *t = (tool_ed_t *) data;

  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return false;

  if ( t->holding ) {
    VikCoord new_coord;
    t->viewport->screen_to_coord(event->x, event->y, &new_coord );

    /* snap to TP */
    if ( event->state & GDK_CONTROL_MASK )
    {
      Trackpoint * tp = vtl->trw->closest_tp_in_five_pixel_interval(t->viewport, event->x, event->y);
      if ( tp )
        new_coord = tp->coord;
    }

    /* snap to WP */
    if ( event->state & GDK_SHIFT_MASK )
    {
      Waypoint * wp = vtl->trw->closest_wp_in_five_pixel_interval(t->viewport, event->x, event->y);
      if ( wp && wp != vtl->trw->current_wp )
        new_coord = wp->coord;
    }

    {
      int x, y;
      t->viewport->coord_to_screen(&new_coord, &x, &y );

      marker_moveto ( t, x, y );
    }
    return true;
  }
  return false;
}

static bool tool_edit_waypoint_release ( VikTrwLayer *vtl, GdkEventButton *event, void * data )
{
  tool_ed_t *t = (tool_ed_t *) data;

  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return false;

  if ( t->holding && event->button == 1 )
  {
    VikCoord new_coord;
    t->viewport->screen_to_coord(event->x, event->y, &new_coord );

    /* snap to TP */
    if ( event->state & GDK_CONTROL_MASK )
    {
      Trackpoint * tp = vtl->trw->closest_tp_in_five_pixel_interval(t->viewport, event->x, event->y);
      if ( tp )
        new_coord = tp->coord;
    }

    /* snap to WP */
    if ( event->state & GDK_SHIFT_MASK )
    {
      Waypoint * wp = vtl->trw->closest_wp_in_five_pixel_interval(t->viewport, event->x, event->y);
      if ( wp && wp != vtl->trw->current_wp )
        new_coord = wp->coord;
    }

    marker_end_move ( t );

    vtl->trw->current_wp->coord = new_coord;

    vtl->trw->calculate_bounds_waypoints();
    vik_layer_emit_update ( VIK_LAYER(vtl) );
    return true;
  }
  /* PUT IN RIGHT PLACE!!! */
  if ( event->button == 3 && vtl->trw->waypoint_rightclick )
  {
    if ( vtl->wp_right_click_menu )
      g_object_ref_sink ( G_OBJECT(vtl->wp_right_click_menu) );
    if ( vtl->trw->current_wp ) {
      vtl->wp_right_click_menu = GTK_MENU ( gtk_menu_new () );
      trw_layer_sublayer_add_menu_items ( vtl, vtl->wp_right_click_menu, NULL, VIK_TRW_LAYER_SUBLAYER_WAYPOINT, (void *) ((long) vtl->trw->current_wp_uid), vtl->trw->waypoints_iters.at(vtl->trw->current_wp_uid), (VikViewport *) t->viewport->vvp );
      gtk_menu_popup ( vtl->wp_right_click_menu, NULL, NULL, NULL, NULL, event->button, gtk_get_current_event_time() );
    }
    vtl->trw->waypoint_rightclick = false;
  }
  return false;
}

/*** New track ****/

static void * tool_new_track_create( VikWindow *vw, Viewport * viewport)
{
	return viewport;
}

typedef struct {
  VikTrwLayer *vtl;
  GdkDrawable *drawable;
  GdkGC *gc;
  GdkPixmap *pixmap;
} draw_sync_t;

/*
 * Draw specified pixmap
 */
static int draw_sync ( void * data )
{
  draw_sync_t *ds = (draw_sync_t*) data;
  // Sometimes don't want to draw
  //  normally because another update has taken precedent such as panning the display
  //   which means this pixmap is no longer valid
  if ( ds->vtl->draw_sync_do ) {
    gdk_threads_enter();
    gdk_draw_drawable (ds->drawable,
                       ds->gc,
                       ds->pixmap,
                       0, 0, 0, 0, -1, -1);
    ds->vtl->draw_sync_done = true;
    gdk_threads_leave();
  }
  free( ds );
  return 0;
}

static char* distance_string (double distance)
{
  char str[128];

  /* draw label with distance */
  vik_units_distance_t dist_units = a_vik_get_units_distance ();
  switch (dist_units) {
  case VIK_UNITS_DISTANCE_MILES:
    if (distance >= VIK_MILES_TO_METERS(1) && distance < VIK_MILES_TO_METERS(100)) {
      g_sprintf(str, "%3.2f miles", VIK_METERS_TO_MILES(distance));
    } else if (distance < 1609.4) {
      g_sprintf(str, "%d yards", (int)(distance*1.0936133));
    } else {
      g_sprintf(str, "%d miles", (int)VIK_METERS_TO_MILES(distance));
    }
    break;
  case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
    if (distance >= VIK_NAUTICAL_MILES_TO_METERS(1) && distance < VIK_NAUTICAL_MILES_TO_METERS(100)) {
      g_sprintf(str, "%3.2f NM", VIK_METERS_TO_NAUTICAL_MILES(distance));
    } else if (distance < VIK_NAUTICAL_MILES_TO_METERS(1)) {
      g_sprintf(str, "%d yards", (int)(distance*1.0936133));
    } else {
      g_sprintf(str, "%d NM", (int)VIK_METERS_TO_NAUTICAL_MILES(distance));
    }
    break;
  default:
    // VIK_UNITS_DISTANCE_KILOMETRES
    if (distance >= 1000 && distance < 100000) {
      g_sprintf(str, "%3.2f km", distance/1000.0);
    } else if (distance < 1000) {
      g_sprintf(str, "%d m", (int)distance);
    } else {
      g_sprintf(str, "%d km", (int)distance/1000);
    }
    break;
  }
  return g_strdup(str);
}

/*
 * Actually set the message in statusbar
 */
static void statusbar_write (double distance, double elev_gain, double elev_loss, double last_step, double angle, VikTrwLayer *vtl )
{
  // Only show elevation data when track has some elevation properties
  char str_gain_loss[64];
  str_gain_loss[0] = '\0';
  char str_last_step[64];
  str_last_step[0] = '\0';
  char *str_total = distance_string (distance);

  if ( (elev_gain > 0.1) || (elev_loss > 0.1) ) {
    if ( a_vik_get_units_height () == VIK_UNITS_HEIGHT_METRES )
      g_sprintf(str_gain_loss, _(" - Gain %dm:Loss %dm"), (int)elev_gain, (int)elev_loss);
    else
      g_sprintf(str_gain_loss, _(" - Gain %dft:Loss %dft"), (int)VIK_METERS_TO_FEET(elev_gain), (int)VIK_METERS_TO_FEET(elev_loss));
  }

  if ( last_step > 0 ) {
      char *tmp = distance_string (last_step);
      g_sprintf(str_last_step, _(" - Bearing %3.1f° - Step %s"), RAD2DEG(angle), tmp);
      free( tmp );
  }

  VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl));

  // Write with full gain/loss information
  char *msg = g_strdup_printf ( "Total %s%s%s", str_total, str_last_step, str_gain_loss);
  vik_statusbar_set_message ( vik_window_get_statusbar (vw), VIK_STATUSBAR_INFO, msg );
  free( msg );
  free( str_total );
}

/*
 * Figure out what information should be set in the statusbar and then write it
 */
static void update_statusbar ( VikTrwLayer *vtl )
{
  // Get elevation data
  double elev_gain, elev_loss;
  vtl->current_track->get_total_elevation_gain(&elev_gain, &elev_loss);

  /* Find out actual distance of current track */
  double distance = vtl->current_track->get_length();

  statusbar_write (distance, elev_gain, elev_loss, 0, 0, vtl);
}


static VikLayerToolFuncStatus tool_new_track_move( VikTrwLayer *vtl, GdkEventMotion *event, Viewport * viewport)
{
  VikViewport * vvp = (VikViewport *) viewport->vvp;
  /* if we haven't sync'ed yet, we don't have time to do more. */
  if ( vtl->draw_sync_done && vtl->current_track && vtl->current_track->trackpoints ) {
    Trackpoint * last_tpt = vtl->current_track->get_tp_last();

    static GdkPixmap *pixmap = NULL;
    int w1, h1, w2, h2;
    // Need to check in case window has been resized
    w1 = viewport->get_width();
    h1 = viewport->get_height();
    if (!pixmap) {
      pixmap = gdk_pixmap_new ( gtk_widget_get_window(GTK_WIDGET(vvp)), w1, h1, -1 );
    }
    gdk_drawable_get_size (pixmap, &w2, &h2);
    if (w1 != w2 || h1 != h2) {
      g_object_unref ( G_OBJECT ( pixmap ) );
      pixmap = gdk_pixmap_new ( gtk_widget_get_window(GTK_WIDGET(vvp)), w1, h1, -1 );
    }

    // Reset to background
    gdk_draw_drawable (pixmap,
                       vtl->current_track_newpoint_gc,
                       viewport->get_pixmap(),
                       0, 0, 0, 0, -1, -1);

    draw_sync_t *passalong;
    int x1, y1;

    viewport->coord_to_screen(&(last_tpt->coord), &x1, &y1 );

    // FOR SCREEN OVERLAYS WE MUST DRAW INTO THIS PIXMAP (when using the reset method)
    //  otherwise using vik_viewport_draw_* functions puts the data into the base pixmap,
    //  thus when we come to reset to the background it would include what we have already drawn!!
    gdk_draw_line ( pixmap,
                    vtl->current_track_newpoint_gc,
                    x1, y1, event->x, event->y );
    // Using this reset method is more reliable than trying to undraw previous efforts via the GDK_INVERT method

    /* Find out actual distance of current track */
    double distance = vtl->current_track->get_length();

    // Now add distance to where the pointer is //
    VikCoord coord;
    struct LatLon ll;
    viewport->screen_to_coord((int) event->x, (int) event->y, &coord );
    vik_coord_to_latlon ( &coord, &ll );
    double last_step = vik_coord_diff( &coord, &(last_tpt->coord));
    distance = distance + last_step;

    // Get elevation data
    double elev_gain, elev_loss;
    vtl->current_track->get_total_elevation_gain(&elev_gain, &elev_loss);

    // Adjust elevation data (if available) for the current pointer position
    double elev_new;
    elev_new = (double) a_dems_get_elev_by_coord ( &coord, VIK_DEM_INTERPOL_BEST );
    if ( elev_new != VIK_DEM_INVALID_ELEVATION ) {
      if ( last_tpt->altitude != VIK_DEFAULT_ALTITUDE ) {
	// Adjust elevation of last track point
	if ( elev_new > last_tpt->altitude )
	  // Going up
	  elev_gain += elev_new - last_tpt->altitude;
	else
	  // Going down
	  elev_loss += last_tpt->altitude - elev_new;
      }
    }

    //
    // Display of the distance 'tooltip' during track creation is controlled by a preference
    //
    if ( a_vik_get_create_track_tooltip() ) {

      char *str = distance_string (distance);

      PangoLayout *pl = gtk_widget_create_pango_layout (GTK_WIDGET(vvp), NULL);
      pango_layout_set_font_description (pl, gtk_widget_get_style(GTK_WIDGET(vvp))->font_desc);
      pango_layout_set_text (pl, str, -1);
      int wd, hd;
      pango_layout_get_pixel_size ( pl, &wd, &hd );

      int xd,yd;
      // offset from cursor a bit depending on font size
      xd = event->x + 10;
      yd = event->y - hd;

      // Create a background block to make the text easier to read over the background map
      GdkGC *background_block_gc = viewport->new_gc("#cccccc", 1);
      gdk_draw_rectangle (pixmap, background_block_gc, true, xd-2, yd-2, wd+4, hd+2);
      gdk_draw_layout (pixmap, vtl->current_track_newpoint_gc, xd, yd, pl);

      g_object_unref ( G_OBJECT ( pl ) );
      g_object_unref ( G_OBJECT ( background_block_gc ) );
      free(str);
    }

    passalong = (draw_sync_t *) malloc(1 * sizeof (draw_sync_t)); // freed by draw_sync()
    passalong->vtl = vtl;
    passalong->pixmap = pixmap;
    passalong->drawable = gtk_widget_get_window(GTK_WIDGET(vvp));
    passalong->gc = vtl->current_track_newpoint_gc;

    double angle;
    double baseangle;
    viewport->compute_bearing(x1, y1, event->x, event->y, &angle, &baseangle );

    // Update statusbar with full gain/loss information
    statusbar_write (distance, elev_gain, elev_loss, last_step, angle, vtl);

    // draw pixmap when we have time to
    g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, draw_sync, passalong, NULL);
    vtl->draw_sync_done = false;
    return VIK_LAYER_TOOL_ACK_GRAB_FOCUS;
  }
  return VIK_LAYER_TOOL_ACK;
}

// NB vtl->current_track must be valid
static void undo_trackpoint_add ( VikTrwLayer *vtl )
{
  // 'undo'
  if ( vtl->current_track->trackpoints ) {
    // TODO rework this...
    //vik_trackpoint_free ( vtl->current_track->get_tp_last() );
    GList *last = g_list_last(vtl->current_track->trackpoints);
    free( last->data );
    vtl->current_track->trackpoints = g_list_remove_link ( vtl->current_track->trackpoints, last );

    vtl->current_track->calculate_bounds();
  }
}

static bool tool_new_track_key_press ( VikTrwLayer *vtl, GdkEventKey *event, Viewport * viewport)
{
  if ( vtl->current_track && event->keyval == GDK_Escape ) {
    // Bin track if only one point as it's not very useful
    if ( vtl->current_track->get_tp_count() == 1 ) {
      if ( vtl->current_track->is_route )
	vtl->trw->delete_route(vtl->current_track);
      else
        vtl->trw->delete_track(vtl->current_track);
    }
    vtl->current_track = NULL;
    vik_layer_emit_update ( VIK_LAYER(vtl) );
    return true;
  } else if ( vtl->current_track && event->keyval == GDK_BackSpace ) {
    undo_trackpoint_add ( vtl );
    update_statusbar ( vtl );
    vik_layer_emit_update ( VIK_LAYER(vtl) );
    return true;
  }
  return false;
}

/*
 * Common function to handle trackpoint button requests on either a route or a track
 *  . enables adding a point via normal click
 *  . enables removal of last point via right click
 *  . finishing of the track or route via double clicking
 */
static bool tool_new_track_or_route_click ( VikTrwLayer *vtl, GdkEventButton *event, Viewport * viewport)
{
  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return false;

  if ( event->button == 2 ) {
    // As the display is panning, the new track pixmap is now invalid so don't draw it
    //  otherwise this drawing done results in flickering back to an old image
    vtl->draw_sync_do = false;
    return false;
  }

  if ( event->button == 3 )
  {
    if ( !vtl->current_track )
      return false;
    undo_trackpoint_add ( vtl );
    update_statusbar ( vtl );
    vik_layer_emit_update ( VIK_LAYER(vtl) );
    return true;
  }

  if ( event->type == GDK_2BUTTON_PRESS )
  {
    /* subtract last (duplicate from double click) tp then end */
    if ( vtl->current_track && vtl->current_track->trackpoints && vtl->ct_x1 == vtl->ct_x2 && vtl->ct_y1 == vtl->ct_y2 )
    {
      /* undo last, then end */
      undo_trackpoint_add ( vtl );
      vtl->current_track = NULL;
    }
    vik_layer_emit_update ( VIK_LAYER(vtl) );
    return true;
  }

  Trackpoint * tp = new Trackpoint();
  viewport->screen_to_coord(event->x, event->y, &(tp->coord) );

  /* snap to other TP */
  if ( event->state & GDK_CONTROL_MASK )
  {
    Trackpoint * other_tp = vtl->trw->closest_tp_in_five_pixel_interval(viewport, event->x, event->y);
    if ( other_tp )
      tp->coord = other_tp->coord;
  }

  tp->newsegment = false;
  tp->has_timestamp = false;
  tp->timestamp = 0;

  if ( vtl->current_track ) {
    vtl->current_track->add_trackpoint(tp, true ); // Ensure bounds is updated
    /* Auto attempt to get elevation from DEM data (if it's available) */
    vtl->current_track->apply_dem_data_last_trackpoint();
  }

  vtl->ct_x1 = vtl->ct_x2;
  vtl->ct_y1 = vtl->ct_y2;
  vtl->ct_x2 = event->x;
  vtl->ct_y2 = event->y;

  vik_layer_emit_update ( VIK_LAYER(vtl) );
  return true;
}

static bool tool_new_track_click ( VikTrwLayer *vtl, GdkEventButton *event, Viewport * viewport)
{
  // if we were running the route finder, cancel it
  vtl->route_finder_started = false;

  // ----------------------------------------------------- if current is a route - switch to new track
  if ( event->button == 1 && ( ! vtl->current_track || (vtl->current_track && vtl->current_track->is_route ) ))
  {
    char *name = vtl->trw->new_unique_sublayer_name(VIK_TRW_LAYER_SUBLAYER_TRACK, _("Track"));
    if ( a_vik_get_ask_for_create_track_name() ) {
      name = a_dialog_new_track ( VIK_GTK_WINDOW_FROM_LAYER(vtl), name, false );
      if ( !name )
        return false;
    }
    vtl->trw->new_track_create_common(name);
    free( name );
  }
  return tool_new_track_or_route_click ( vtl, event, viewport);
}

static void tool_new_track_release ( VikTrwLayer *vtl, GdkEventButton *event, Viewport * viewport)
{
  if ( event->button == 2 ) {
    // Pan moving ended - enable potential point drawing again
    vtl->draw_sync_do = true;
    vtl->draw_sync_done = true;
  }
}

/*** New route ****/

static void * tool_new_route_create(VikWindow *vw, Viewport * viewport)
{
  return viewport;
}

static bool tool_new_route_click ( VikTrwLayer *vtl, GdkEventButton *event, Viewport * viewport)
{
  // if we were running the route finder, cancel it
  vtl->route_finder_started = false;

  // -------------------------- if current is a track - switch to new route,
  if ( event->button == 1 && ( ! vtl->current_track ||
                               (vtl->current_track && !vtl->current_track->is_route ) ) )
  {
    char *name = vtl->trw->new_unique_sublayer_name(VIK_TRW_LAYER_SUBLAYER_ROUTE, _("Route"));
    if ( a_vik_get_ask_for_create_track_name() ) {
      name = a_dialog_new_track ( VIK_GTK_WINDOW_FROM_LAYER(vtl), name, true );
      if ( !name )
        return false;
    }
    vtl->trw->new_route_create_common(name);
    free( name );
  }
  return tool_new_track_or_route_click ( vtl, event, viewport);
}

/*** New waypoint ****/

static void * tool_new_waypoint_create(VikWindow *vw, Viewport * viewport)
{
	return viewport;
}

static bool tool_new_waypoint_click( VikTrwLayer *vtl, GdkEventButton *event, Viewport * viewport)
{
  VikCoord coord;
  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return false;
  viewport->screen_to_coord(event->x, event->y, &coord );
  if (vtl->trw->new_waypoint(VIK_GTK_WINDOW_FROM_LAYER(vtl), &coord) ) {
    vtl->trw->calculate_bounds_waypoints();
    if ( VIK_LAYER(vtl)->visible )
      vik_layer_emit_update ( VIK_LAYER(vtl) );
  }
  return true;
}


/*** Edit trackpoint ****/

static void * tool_edit_trackpoint_create ( VikWindow *vw, Viewport * viewport)
{
  tool_ed_t *t = (tool_ed_t *) malloc(1 * sizeof (tool_ed_t));
  t->viewport = viewport;
  t->holding = false;
  return t;
}

static void tool_edit_trackpoint_destroy ( tool_ed_t *t )
{
  free( t );
}

/**
 * tool_edit_trackpoint_click:
 *
 * On 'initial' click: search for the nearest trackpoint or routepoint and store it as the current trackpoint
 * Then update the viewport, statusbar and edit dialog to draw the point as being selected and it's information.
 * On subsequent clicks: (as the current trackpoint is defined) and the click is very near the same point
 *  then initiate the move operation to drag the point to a new destination.
 * NB The current trackpoint will get reset elsewhere.
 */
static bool tool_edit_trackpoint_click ( VikTrwLayer *vtl, GdkEventButton *event, void * data )
{
  tool_ed_t *t = (tool_ed_t *) data;
  TPSearchParams params;
  params.viewport = t->viewport;
  params.x = event->x;
  params.y = event->y;
  params.closest_track_uid = 0;
  params.closest_tp = NULL;
  params.closest_tpl = NULL;
  t->viewport->get_min_max_lat_lon(&(params.bbox.south), &(params.bbox.north), &(params.bbox.west), &(params.bbox.east) );

  if ( event->button != 1 )
    return false;

  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return false;

  if ( !vtl->vl.visible || !(vtl->trw->tracks_visible || vtl->trw->routes_visible) )
    return false;

  if ( vtl->trw->current_tpl )
  {
    /* first check if it is within range of prev. tp. and if current_tp track is shown. (if it is, we are moving that trackpoint.) */
    Trackpoint * tp = ((Trackpoint *) vtl->trw->current_tpl->data);
    Track *current_tr = vtl->trw->tracks.at(vtl->trw->current_tp_uid);
    if ( !current_tr )
      current_tr = vtl->trw->routes.at(vtl->trw->current_tp_uid);
    if ( !current_tr )
      return false;

    int x, y;
    t->viewport->coord_to_screen(&(tp->coord), &x, &y );

    if ( current_tr->visible &&
         abs(x - (int)round(event->x)) < TRACKPOINT_SIZE_APPROX &&
         abs(y - (int)round(event->y)) < TRACKPOINT_SIZE_APPROX ) {
      marker_begin_move ( t, event->x, event->y );
      return true;
    }

  }

  if ( vtl->trw->tracks_visible )
    LayerTRW::track_search_closest_tp(vtl->trw->tracks, &params);

  if ( params.closest_tp )
  {
    vik_treeview_select_iter ( VIK_LAYER(vtl)->vt, vtl->trw->tracks_iters.at(params.closest_track_uid), true );
    vtl->trw->current_tpl = params.closest_tpl;
    vtl->trw->current_tp_uid = params.closest_track_uid;
    vtl->trw->current_tp_track = vtl->trw->tracks.at(params.closest_track_uid);
    vtl->trw->tpwin_init();
    vtl->trw->set_statusbar_msg_info_trkpt(params.closest_tp);
    vik_layer_emit_update ( VIK_LAYER(vtl) );
    return true;
  }

  if ( vtl->trw->routes_visible )
    LayerTRW::track_search_closest_tp(vtl->trw->routes, &params);

  if ( params.closest_tp )
  {
    vik_treeview_select_iter ( VIK_LAYER(vtl)->vt, vtl->trw->routes_iters.at(params.closest_track_uid), true );
    vtl->trw->current_tpl = params.closest_tpl;
    vtl->trw->current_tp_uid = params.closest_track_uid;
    vtl->trw->current_tp_track = vtl->trw->routes.at(params.closest_track_uid);
    vtl->trw->tpwin_init();
    vtl->trw->set_statusbar_msg_info_trkpt(params.closest_tp);
    vik_layer_emit_update ( VIK_LAYER(vtl) );
    return true;
  }

  /* these aren't the droids you're looking for */
  return false;
}

static bool tool_edit_trackpoint_move ( VikTrwLayer *vtl, GdkEventMotion *event, void * data )
{
  tool_ed_t *t = (tool_ed_t *) data;

  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return false;

  if ( t->holding )
  {
    VikCoord new_coord;
    t->viewport->screen_to_coord(event->x, event->y, &new_coord);

    /* snap to TP */
    if ( event->state & GDK_CONTROL_MASK )
    {
      Trackpoint * tp = vtl->trw->closest_tp_in_five_pixel_interval(t->viewport, event->x, event->y);
      if ( tp && tp != vtl->trw->current_tpl->data )
        new_coord = tp->coord;
    }
    //    ((Trackpoint *) vtl->trw->current_tpl->data)->coord = new_coord;
    {
      int x, y;
      t->viewport->coord_to_screen(&new_coord, &x, &y );
      marker_moveto ( t, x, y );
    }

    return true;
  }
  return false;
}

static bool tool_edit_trackpoint_release ( VikTrwLayer *vtl, GdkEventButton *event, void * data )
{
  tool_ed_t *t = (tool_ed_t *) data;

  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return false;
  if ( event->button != 1)
    return false;

  if ( t->holding ) {
    VikCoord new_coord;
    t->viewport->screen_to_coord(event->x, event->y, &new_coord );

    /* snap to TP */
    if ( event->state & GDK_CONTROL_MASK )
    {
      Trackpoint * tp = vtl->trw->closest_tp_in_five_pixel_interval(t->viewport, event->x, event->y );
      if ( tp && tp != vtl->trw->current_tpl->data )
        new_coord = tp->coord;
    }

    ((Trackpoint *) vtl->trw->current_tpl->data)->coord = new_coord;
    if ( vtl->trw->current_tp_track )
      vtl->trw->current_tp_track->calculate_bounds();

    marker_end_move ( t );

    /* diff dist is diff from orig */
    if ( vtl->trw->tpwin )
      if ( vtl->trw->current_tp_track )
        vtl->trw->my_tpwin_set_tp();

    vik_layer_emit_update ( VIK_LAYER(vtl) );
    return true;
  }
  return false;
}


/*** Extended Route Finder ***/

static void * tool_extended_route_finder_create(VikWindow *vw, Viewport * viewport)
{
	return viewport;
}

static void tool_extended_route_finder_undo ( VikTrwLayer *vtl )
{
  VikCoord *new_end;
  new_end = vtl->current_track->cut_back_to_double_point();
  if ( new_end ) {
    free( new_end );
    vik_layer_emit_update ( VIK_LAYER(vtl) );

    /* remove last ' to:...' */
    if ( vtl->current_track->comment ) {
      char *last_to = strrchr ( vtl->current_track->comment, 't' );
      if ( last_to && (last_to - vtl->current_track->comment > 1) ) {
        char *new_comment = g_strndup ( vtl->current_track->comment,
                                         last_to - vtl->current_track->comment - 1);
        vtl->current_track->set_comment_no_copy(new_comment);
      }
    }
  }
}


static bool tool_extended_route_finder_click(VikTrwLayer *vtl, GdkEventButton *event, Viewport * viewport)
{
  VikCoord tmp;
  if ( !vtl ) return false;
  viewport->screen_to_coord(event->x, event->y, &tmp);
  if ( event->button == 3 && vtl->current_track ) {
    tool_extended_route_finder_undo ( vtl );
  }
  else if ( event->button == 2 ) {
     vtl->draw_sync_do = false;
     return false;
  }
  // if we started the track but via undo deleted all the track points, begin again
  else if ( vtl->current_track && vtl->current_track->is_route && ! vtl->current_track->get_tp_first() ) {
    return tool_new_track_or_route_click ( vtl, event, viewport );
  }
  else if ( ( vtl->current_track && vtl->current_track->is_route ) ||
            ( event->state & GDK_CONTROL_MASK && vtl->current_track ) ) {
    struct LatLon start, end;

    Trackpoint * tp_start = vtl->current_track->get_tp_last();
    vik_coord_to_latlon ( &(tp_start->coord), &start );
    vik_coord_to_latlon ( &(tmp), &end );

    vtl->route_finder_started = true;
    vtl->route_finder_append = true;  // merge tracks. keep started true.

    // update UI to let user know what's going on
    VikStatusbar *sb = vik_window_get_statusbar (VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)));
    VikRoutingEngine *engine = vik_routing_default_engine ( );
    if ( ! engine ) {
        vik_statusbar_set_message ( sb, VIK_STATUSBAR_INFO, "Cannot plan route without a default routing engine." );
        return true;
    }
    char *msg = g_strdup_printf ( _("Querying %s for route between (%.3f, %.3f) and (%.3f, %.3f)."),
                                   vik_routing_engine_get_label ( engine ),
                                   start.lat, start.lon, end.lat, end.lon );
    vik_statusbar_set_message ( sb, VIK_STATUSBAR_INFO, msg );
    free( msg );
    vik_window_set_busy_cursor ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)) );


    /* Give GTK a change to display the new status bar before querying the web */
    while ( gtk_events_pending ( ) )
        gtk_main_iteration ( );

    bool find_status = vik_routing_default_find ( vtl, start, end );

    /* Update UI to say we're done */
    vik_window_clear_busy_cursor ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)) );
    msg = ( find_status ) ? g_strdup_printf ( _("%s returned route between (%.3f, %.3f) and (%.3f, %.3f)."),
                            vik_routing_engine_get_label ( engine ),
                            start.lat, start.lon, end.lat, end.lon )
                          : g_strdup_printf ( _("Error getting route from %s."),
                                              vik_routing_engine_get_label ( engine ) );
    vik_statusbar_set_message ( sb, VIK_STATUSBAR_INFO, msg );
    free( msg );

    vik_layer_emit_update ( VIK_LAYER(vtl) );
  } else {
    vtl->current_track = NULL;

    // create a new route where we will add the planned route to
    bool ret = tool_new_route_click( vtl, event, viewport );

    vtl->route_finder_started = true;

    return ret;
  }
  return true;
}

static bool tool_extended_route_finder_key_press ( VikTrwLayer *vtl, GdkEventKey *event, Viewport * viewport)
{
  if ( vtl->current_track && event->keyval == GDK_Escape ) {
    vtl->route_finder_started = false;
    vtl->current_track = NULL;
    vik_layer_emit_update ( VIK_LAYER(vtl) );
    return true;
  } else if ( vtl->current_track && event->keyval == GDK_BackSpace ) {
    tool_extended_route_finder_undo ( vtl );
  }
  return false;
}



/*** Show picture ****/

static void * tool_show_picture_create(VikWindow *vw, Viewport * viewport)
{
	return viewport;
}





static void trw_layer_show_picture ( menu_array_sublayer values )
{
  /* thanks to the Gaim people for showing me ShellExecute and g_spawn_command_line_async */
#ifdef WINDOWS
  ShellExecute(NULL, "open", (char *) values[MA_MISC], NULL, NULL, SW_SHOWNORMAL);
#else /* WINDOWS */
  GError *err = NULL;
  char *quoted_file = g_shell_quote ( (char *) values[MA_MISC] );
  char *cmd = g_strdup_printf ( "%s %s", a_vik_get_image_viewer(), quoted_file );
  free( quoted_file );
  if ( ! g_spawn_command_line_async ( cmd, &err ) )
    {
      a_dialog_error_msg_extra ( VIK_GTK_WINDOW_FROM_LAYER(values[MA_VTL]), _("Could not launch %s to open file."), a_vik_get_image_viewer() );
      g_error_free ( err );
    }
  free( cmd );
#endif /* WINDOWS */
}





static bool tool_show_picture_click(VikTrwLayer * vtl, GdkEventButton * event, Viewport * viewport)
{
	if (!vtl || vtl->vl.type != VIK_LAYER_TRW) {
		return false;
	}

	char * found = LayerTRW::tool_show_picture_wp(vtl->trw->waypoints, event->x, event->y, viewport);
	if (found) {
		static menu_array_sublayer values;
		values[MA_VTL] = vtl;
		values[MA_MISC] = found;
		trw_layer_show_picture(values);
		return true; /* Found a match. */
	} else {
		return false; /* Go through other layers, searching for a match. */
	}
}





/***************************************************************************
 ** End tool code
 ***************************************************************************/





/* Structure for thumbnail creating data used in the background thread */
typedef struct {
  VikTrwLayer *vtl; // Layer needed for redrawing
  GSList *pics;     // Image list
} thumbnail_create_thread_data;

static int create_thumbnails_thread ( thumbnail_create_thread_data *tctd, void * threaddata )
{
  unsigned int total = g_slist_length(tctd->pics), done = 0;
  while ( tctd->pics )
  {
    a_thumbnails_create ( (char *) tctd->pics->data );
    int result = a_background_thread_progress ( threaddata, ((double) ++done) / total );
    if ( result != 0 )
      return -1; /* Abort thread */

    tctd->pics = tctd->pics->next;
  }

  // Redraw to show the thumbnails as they are now created
  if ( IS_VIK_LAYER(tctd->vtl) )
    vik_layer_emit_update ( VIK_LAYER(tctd->vtl) ); // NB update from background thread

  return 0;
}

static void thumbnail_create_thread_free ( thumbnail_create_thread_data *tctd )
{
  while ( tctd->pics )
  {
    free( tctd->pics->data );
    tctd->pics = g_slist_delete_link ( tctd->pics, tctd->pics );
  }
  free( tctd );
}

void trw_layer_verify_thumbnails ( VikTrwLayer *vtl, GtkWidget *vp )
{
  if ( ! vtl->has_verified_thumbnails )
  {
    GSList *pics = vtl->trw->image_wp_make_list(vtl->trw->waypoints);
    if ( pics )
    {
      int len = g_slist_length ( pics );
      char *tmp = g_strdup_printf ( _("Creating %d Image Thumbnails..."), len );
      thumbnail_create_thread_data *tctd = (thumbnail_create_thread_data *) malloc(sizeof (thumbnail_create_thread_data));
      tctd->vtl = vtl;
      tctd->pics = pics;
      a_background_thread ( BACKGROUND_POOL_LOCAL,
                            VIK_GTK_WINDOW_FROM_LAYER(vtl),
			    tmp,
			    (vik_thr_func) create_thumbnails_thread,
			    tctd,
			    (vik_thr_free_func) thumbnail_create_thread_free,
			    NULL,
			    len );
      free( tmp );
    }
  }
}

static const char* my_track_colors ( int ii )
{
  static const char* colors[VIK_TRW_LAYER_TRACK_GCS] = {
    "#2d870a",
    "#135D34",
    "#0a8783",
    "#0e4d87",
    "#05469f",
    "#695CBB",
    "#2d059f",
    "#4a059f",
    "#5A171A",
    "#96059f"
  };
  // Fast and reliable way of returning a colour
  return colors[(ii % VIK_TRW_LAYER_TRACK_GCS)];
}

void LayerTRW::track_alloc_colors()
{
	VikTrwLayer * vtl = (VikTrwLayer *) this->vl;

	// Tracks
	int ii = 0;
	for (auto i = this->tracks.begin(); i != this->tracks.end(); i++) {

		Track * trk = i->second;

		// Tracks get a random spread of colours if not already assigned
		if (!trk->has_color) {
			if (vtl->drawmode == DRAWMODE_ALL_SAME_COLOR) {
				trk->color = vtl->track_color;
			} else {
				gdk_color_parse(my_track_colors(ii), &(trk->color));
			}
			trk->has_color = true;
		}

		this->update_treeview(trk);

		ii++;
		if (ii > VIK_TRW_LAYER_TRACK_GCS) {
			ii = 0;
		}
	}

	// Routes
	ii = 0;
	for (auto i = this->routes.begin(); i != this->routes.end(); i++) {

		Track * trk = i->second;

		// Routes get an intermix of reds
		if (!trk->has_color) {
			if (ii) {
				gdk_color_parse("#FF0000", &trk->color); // Red
			} else {
				gdk_color_parse("#B40916", &trk->color); // Dark Red
			}
			trk->has_color = true;
		}

		this->update_treeview(trk);

		ii = !ii;
	}
}





/*
 * (Re)Calculate the bounds of the waypoints in this layer,
 * This should be called whenever waypoints are changed
 */
void LayerTRW::calculate_bounds_waypoints()
{
	struct LatLon topleft = { 0.0, 0.0 };
	struct LatLon bottomright = { 0.0, 0.0 };
	struct LatLon ll;

	auto i = this->waypoints.begin();
	if (i == this->waypoints.end()) {
		/* E.g. after all waypoints have been removed from trw layer. */
		return;
	}
	Waypoint * wp = i->second;
	// Set bounds to first point
	if (wp) {
		vik_coord_to_latlon(&wp->coord, &topleft);
		vik_coord_to_latlon(&wp->coord, &bottomright);
	}

	// Ensure there is another point...
	if (this->waypoints.size() > 1) {

		while (++i != this->waypoints.end()) { /* kamilTODO: check the conditon. */

			wp = i->second;

			// See if this point increases the bounds.
			vik_coord_to_latlon(&wp->coord, &ll);

			if (ll.lat > topleft.lat) {
				topleft.lat = ll.lat;
			}
			if (ll.lon < topleft.lon) {
				topleft.lon = ll.lon;
			}
			if (ll.lat < bottomright.lat) {
				bottomright.lat = ll.lat;
			}
			if (ll.lon > bottomright.lon){
				bottomright.lon = ll.lon;
			}
		}
	}

	VikTrwLayer * vtl = (VikTrwLayer *) this->vl;

	vtl->waypoints_bbox.north = topleft.lat;
	vtl->waypoints_bbox.east = bottomright.lon;
	vtl->waypoints_bbox.south = bottomright.lat;
	vtl->waypoints_bbox.west = topleft.lon;
}





void LayerTRW::calculate_bounds_track(std::unordered_map<sg_uid_t, Track *> & tracks)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		i->second->calculate_bounds();
	}
}





void LayerTRW::calculate_bounds_tracks()
{
	LayerTRW::calculate_bounds_track(this->tracks);
	LayerTRW::calculate_bounds_track(this->routes);
}





void LayerTRW::sort_all()
{
	VikTrwLayer * vtl = (VikTrwLayer *) this->vl;

	if (!VIK_LAYER(vtl)->vt) {
		return;
	}

	// Obviously need 2 to tango - sorting with only 1 (or less) is a lonely activity!
	if (this->tracks.size() > 1) {
		vik_treeview_sort_children(VIK_LAYER(vtl)->vt, &(this->track_iter), vtl->track_sort_order);
	}

	if (this->routes.size() > 1) {
		vik_treeview_sort_children(VIK_LAYER(vtl)->vt, &(this->route_iter), vtl->track_sort_order);
	}

	if (this->waypoints.size() > 1) {
		vik_treeview_sort_children(VIK_LAYER(vtl)->vt, &(this->waypoint_iter), vtl->wp_sort_order);
	}
}





/**
 * Get the earliest timestamp available from all tracks
 */
time_t LayerTRW::get_timestamp_tracks()
{
	time_t timestamp = 0;
	GList * gl = NULL;
	gl = LayerTRW::get_track_values(&gl, this->tracks);
	gl = g_list_sort(gl, Track::compare_timestamp);
	gl = g_list_first(gl);

	if (gl) {
		// Only need to check the first track as they have been sorted by time
		Track * trk = (Track *) gl->data;
		// Assume trackpoints already sorted by time
		Trackpoint * tpt = trk->get_tp_first();
		if (tpt && tpt->has_timestamp) {
			timestamp = tpt->timestamp;
		}
		g_list_free(gl);
	}
	return timestamp;
}





/**
 * Get the earliest timestamp available from all waypoints
 */
time_t LayerTRW::get_timestamp_waypoints()
{
	time_t timestamp = 0;

	for (auto i = this->waypoints.begin(); i != this->waypoints.end(); i++) {
		Waypoint * wp = i->second;
		if (wp->has_timestamp) {
			// When timestamp not set yet - use the first value encountered
			if (timestamp == 0) {
				timestamp = wp->timestamp;
			} else if (timestamp > wp->timestamp) {
				timestamp = wp->timestamp;
			} else {

			}
		}
	}

	return timestamp;
}





/**
 * Get the earliest timestamp available for this layer
 */
static time_t trw_layer_get_timestamp ( VikTrwLayer *vtl )
{
  time_t timestamp_tracks = vtl->trw->get_timestamp_tracks();
  time_t timestamp_waypoints = vtl->trw->get_timestamp_waypoints();
  // NB routes don't have timestamps - hence they are not considered

  if ( !timestamp_tracks && !timestamp_waypoints ) {
    // Fallback to get time from the metadata when no other timestamps available
    GTimeVal gtv;
    if  ( vtl->metadata && vtl->metadata->timestamp && g_time_val_from_iso8601 ( vtl->metadata->timestamp, &gtv ) )
      return gtv.tv_sec;
  }
  if ( timestamp_tracks && !timestamp_waypoints )
    return timestamp_tracks;
  if ( timestamp_tracks && timestamp_waypoints && (timestamp_tracks < timestamp_waypoints) )
    return timestamp_tracks;
  return timestamp_waypoints;
}

static void trw_layer_post_read ( VikTrwLayer *vtl, GtkWidget *vvp, bool from_file )
{
  if ( VIK_LAYER(vtl)->realized )
    trw_layer_verify_thumbnails ( vtl, vvp );
  vtl->trw->track_alloc_colors();

  vtl->trw->calculate_bounds_waypoints();
  vtl->trw->calculate_bounds_tracks();

  // Apply treeview sort after loading all the tracks for this layer
  //  (rather than sorted insert on each individual track additional)
  //  and after subsequent changes to the properties as the specified order may have changed.
  //  since the sorting of a treeview section is now very quick
  // NB sorting is also performed after every name change as well to maintain the list order
  vtl->trw->sort_all();

  // Setting metadata time if not otherwise set
  if ( vtl->metadata ) {

    bool need_to_set_time = true;
    if ( vtl->metadata->timestamp ) {
      need_to_set_time = false;
      if ( !g_strcmp0(vtl->metadata->timestamp, "" ) )
        need_to_set_time = true;
    }

    if ( need_to_set_time ) {
      GTimeVal timestamp;
      timestamp.tv_usec = 0;
      timestamp.tv_sec = trw_layer_get_timestamp ( vtl );

      // No time found - so use 'now' for the metadata time
      if ( timestamp.tv_sec == 0 ) {
        g_get_current_time ( &timestamp );
      }

      vtl->metadata->timestamp = g_time_val_to_iso8601 ( &timestamp );
    }
  }
}





VikCoordMode LayerTRW::get_coord_mode()
{
	return this->coord_mode;
}





/**
 * Uniquify the whole layer
 * Also requires the layers panel as the names shown there need updating too
 * Returns whether the operation was successful or not
 */
bool LayerTRW::uniquify(VikLayersPanel * vlp)
{
	if (vlp) {
		this->uniquify_tracks(vlp, this->tracks, true);
		this->uniquify_tracks(vlp, this->routes, false);
		this->uniquify_waypoints(vlp);
		return true;
	}
	return false;
}





static void waypoint_convert(Waypoint * wp, VikCoordMode * dest_mode)
{
	vik_coord_convert(&(wp->coord), *dest_mode);
}





static void trw_layer_change_coord_mode(VikTrwLayer *vtl, VikCoordMode dest_mode)
{
	if (vtl->trw->coord_mode != dest_mode) {
		vtl->trw->coord_mode = dest_mode;
		LayerTRW::waypoints_convert(vtl->trw->waypoints, &dest_mode);
		LayerTRW::track_convert(vtl->trw->tracks, &dest_mode);
		LayerTRW::track_convert(vtl->trw->routes, &dest_mode);
	}
}





static void trw_layer_set_menu_selection ( VikTrwLayer *vtl, uint16_t selection )
{
  vtl->menu_selection = (VikStdLayerMenuItem) selection; /* kamil: invalid cast? */
}

static uint16_t trw_layer_get_menu_selection ( VikTrwLayer *vtl )
{
  return (vtl->menu_selection);
}

/* ----------- Downloading maps along tracks --------------- */

static int get_download_area_width(VikViewport *vvp, double zoom_level, struct LatLon *wh)
{
  /* TODO: calculating based on current size of viewport */
  const double w_at_zoom_0_125 = 0.0013;
  const double h_at_zoom_0_125 = 0.0011;
  double zoom_factor = zoom_level/0.125;

  wh->lat = h_at_zoom_0_125 * zoom_factor;
  wh->lon = w_at_zoom_0_125 * zoom_factor;

  return 0;   /* all OK */
}

static VikCoord *get_next_coord(VikCoord *from, VikCoord *to, struct LatLon *dist, double gradient)
{
  if ((dist->lon >= ABS(to->east_west - from->east_west)) &&
      (dist->lat >= ABS(to->north_south - from->north_south)))
    return NULL;

  VikCoord *coord = (VikCoord *) malloc(sizeof (VikCoord));
  coord->mode = VIK_COORD_LATLON;

  if (ABS(gradient) < 1) {
    if (from->east_west > to->east_west)
      coord->east_west = from->east_west - dist->lon;
    else
      coord->east_west = from->east_west + dist->lon;
    coord->north_south = gradient * (coord->east_west - from->east_west) + from->north_south;
  } else {
    if (from->north_south > to->north_south)
      coord->north_south = from->north_south - dist->lat;
    else
      coord->north_south = from->north_south + dist->lat;
    coord->east_west = (1/gradient) * (coord->north_south - from->north_south) + from->north_south;
  }

  return coord;
}

static GList *add_fillins(GList *list, VikCoord *from, VikCoord *to, struct LatLon *dist)
{
  /* TODO: handle virtical track (to->east_west - from->east_west == 0) */
  double gradient = (to->north_south - from->north_south)/(to->east_west - from->east_west);

  VikCoord *next = from;
  while (true) {
    if ((next = get_next_coord(next, to, dist, gradient)) == NULL)
        break;
    list = g_list_prepend(list, next);
  }

  return list;
}

void vik_track_download_map(Track *tr, VikMapsLayer *vml, VikViewport *vvp, double zoom_level)
{
  typedef struct _Rect {
    VikCoord tl;
    VikCoord br;
    VikCoord center;
  } Rect;
#define GLRECT(iter) ((Rect *)((iter)->data))

  struct LatLon wh;
  GList *rects_to_download = NULL;
  GList *rect_iter;

  if (get_download_area_width(vvp, zoom_level, &wh))
    return;

  GList *iter = tr->trackpoints;
  if (!iter)
    return;

  bool new_map = true;
  VikCoord *cur_coord, tl, br;
  Rect *rect;
  while (iter) {
    cur_coord = &(((Trackpoint *) iter->data))->coord;
    if (new_map) {
      vik_coord_set_area(cur_coord, &wh, &tl, &br);
      rect = (Rect *) malloc(sizeof(Rect));
      rect->tl = tl;
      rect->br = br;
      rect->center = *cur_coord;
      rects_to_download = g_list_prepend(rects_to_download, rect);
      new_map = false;
      iter = iter->next;
      continue;
    }
    bool found = false;
    for (rect_iter = rects_to_download; rect_iter; rect_iter = rect_iter->next) {
      if (vik_coord_inside(cur_coord, &GLRECT(rect_iter)->tl, &GLRECT(rect_iter)->br)) {
        found = true;
        break;
      }
    }
    if (found)
        iter = iter->next;
    else
      new_map = true;
  }

  GList *fillins = NULL;
  /* 'fillin' doesn't work in UTM mode - potentially ending up in massive loop continually allocating memory - hence don't do it */
  /* seems that ATM the function get_next_coord works only for LATLON */
  if ( cur_coord->mode == VIK_COORD_LATLON ) {
    /* fill-ins for far apart points */
    GList *cur_rect, *next_rect;
    for (cur_rect = rects_to_download;
	 (next_rect = cur_rect->next) != NULL;
	 cur_rect = cur_rect->next) {
      if ((wh.lon < ABS(GLRECT(cur_rect)->center.east_west - GLRECT(next_rect)->center.east_west)) ||
	  (wh.lat < ABS(GLRECT(cur_rect)->center.north_south - GLRECT(next_rect)->center.north_south))) {
	fillins = add_fillins(fillins, &GLRECT(cur_rect)->center, &GLRECT(next_rect)->center, &wh);
      }
    }
  } else
    fprintf(stderr, "MESSAGE: %s: this feature works only in Mercator mode\n", __FUNCTION__);

  if (fillins) {
    GList *fiter = fillins;
    while (fiter) {
      cur_coord = (VikCoord *)(fiter->data);
      vik_coord_set_area(cur_coord, &wh, &tl, &br);
      rect = (Rect *) malloc(sizeof(Rect));
      rect->tl = tl;
      rect->br = br;
      rect->center = *cur_coord;
      rects_to_download = g_list_prepend(rects_to_download, rect);
      fiter = fiter->next;
    }
  }

  for (rect_iter = rects_to_download; rect_iter; rect_iter = rect_iter->next) {
    vik_maps_layer_download_section (vml, vvp, &(((Rect *)(rect_iter->data))->tl), &(((Rect *)(rect_iter->data))->br), zoom_level);
  }

  if (fillins) {
    for (iter = fillins; iter; iter = iter->next)
      free(iter->data);
    g_list_free(fillins);
  }
  if (rects_to_download) {
    for (rect_iter = rects_to_download; rect_iter; rect_iter = rect_iter->next)
      free(rect_iter->data);
    g_list_free(rects_to_download);
  }
}

static void trw_layer_download_map_along_track_cb ( menu_array_sublayer values )
{
  VikMapsLayer *vml;
  int selected_map;
  char *zoomlist[] = {(char *) "0.125", (char *) "0.25", (char *) "0.5", (char *) "1", (char *) "2", (char *) "4", (char *) "8", (char *) "16", (char *) "32", (char *) "64", (char *) "128", (char *) "256", (char *) "512", (char *) "1024", NULL };
  double zoom_vals[] = {0.125, 0.25, 0.5, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};
  int selected_zoom, default_zoom;

  VikTrwLayer *vtl = (VikTrwLayer *) values[MA_VTL];
  VikLayersPanel *vlp = (VikLayersPanel *) values[MA_VLP];
  Track * trk = trw_layer_get_track_helper(values, vtl);

  if ( !trk )
    return;

  VikViewport *vvp = vik_window_viewport((VikWindow *)(VIK_GTK_WINDOW_FROM_LAYER(vtl)));

  std::list<Layer *> * vmls = vlp->panel_ref->get_all_layers_of_type(VIK_LAYER_MAPS, true); // Includes hidden map layer types
  int num_maps = vmls->size();

  if (!num_maps) {
    a_dialog_error_msg(VIK_GTK_WINDOW_FROM_LAYER(vtl), _("No map layer in use. Create one first") );
    return;
  }

  // Convert from list of vmls to list of names. Allowing the user to select one of them
  char **map_names = (char **) g_malloc_n(1 + num_maps, sizeof (char *));
  VikMapsLayer **map_layers = (VikMapsLayer **) g_malloc_n(1 + num_maps, sizeof(VikMapsLayer *));

  char **np = map_names;
  VikMapsLayer **lp = map_layers;
  for (auto i = vmls->begin(); i != vmls->end(); i++) {
    vml = (VikMapsLayer *) *i;
    *lp++ = vml;
    *np++ = vik_maps_layer_get_map_label(vml);
  }
  // Mark end of the array lists
  *lp = NULL;
  *np = NULL;

  double cur_zoom = vvp->port.get_zoom();
  for (default_zoom = 0; default_zoom < G_N_ELEMENTS(zoom_vals); default_zoom++) {
    if (cur_zoom == zoom_vals[default_zoom])
      break;
  }
  default_zoom = (default_zoom == G_N_ELEMENTS(zoom_vals)) ? G_N_ELEMENTS(zoom_vals) - 1 : default_zoom;

  if (!a_dialog_map_n_zoom(VIK_GTK_WINDOW_FROM_LAYER(vtl), map_names, 0, zoomlist, default_zoom, &selected_map, &selected_zoom))
    goto done;

  vik_track_download_map(trk, map_layers[selected_map], vvp, zoom_vals[selected_zoom]);

done:
  for (int i = 0; i < num_maps; i++)
    free(map_names[i]);
  free(map_names);
  free(map_layers);

  delete vmls;

}

/**** lowest waypoint number calculation ***/
static int highest_wp_number_name_to_number(const char *name) {
  if ( strlen(name) == 3 ) {
    int n = atoi(name);
    if ( n < 100 && name[0] != '0' )
      return -1;
    if ( n < 10 && name[0] != '0' )
      return -1;
    return n;
  }
  return -1;
}





void LayerTRW::highest_wp_number_reset()
{
	this->highest_wp_number = -1;
}





void LayerTRW::highest_wp_number_add_wp(const char * new_wp_name)
{
	/* if is bigger that top, add it */
	int new_wp_num = highest_wp_number_name_to_number(new_wp_name);
	if (new_wp_num > this->highest_wp_number) {
		this->highest_wp_number = new_wp_num;
	}
}





void LayerTRW::highest_wp_number_remove_wp(char const * old_wp_name)
{
	/* if wasn't top, do nothing. if was top, count backwards until we find one used */
	int old_wp_num = highest_wp_number_name_to_number(old_wp_name);
	if (this->highest_wp_number == old_wp_num) {
		char buf[4];
		this->highest_wp_number--;

		snprintf(buf,4,"%03d", this->highest_wp_number);
		/* search down until we find something that *does* exist */

		while (this->highest_wp_number > 0 && !this->get_waypoint((const char *) buf)) {
		       this->highest_wp_number--;
		       snprintf(buf, 4, "%03d", this->highest_wp_number);
		}
	}
}





/* get lowest unused number */
char * LayerTRW::highest_wp_number_get()
{
	char buf[4];
	if (this->highest_wp_number < 0 || this->highest_wp_number >= 999) {
		return NULL;
	}
	snprintf(buf, 4, "%03d", this->highest_wp_number +1);
	return g_strdup(buf);
}





/**
 * trw_layer_create_track_list_both:
 *
 * Create the latest list of tracks and routes
 */
static GList* trw_layer_create_track_list_both ( VikLayer *vl, void * user_data )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(vl);
  GList * tracks = NULL;
  LayerTRW::get_track_values(&tracks, vtl->trw->get_tracks());
  LayerTRW::get_track_values(&tracks, vtl->trw->get_routes());

  return vtl->trw->build_track_list_t(tracks);
}

static void trw_layer_track_list_dialog_single ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);

  char *title = NULL;
  if ( KPOINTER_TO_INT(values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_TRACKS )
    title = g_strdup_printf ( _("%s: Track List"), VIK_LAYER(vtl)->name );
  else
    title = g_strdup_printf ( _("%s: Route List"), VIK_LAYER(vtl)->name );

  vik_trw_layer_track_list_show_dialog ( title, VIK_LAYER(vtl), values[MA_SUBTYPE], trw_layer_create_track_list, false );
  free( title );
}

static void trw_layer_track_list_dialog ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);

  char *title = g_strdup_printf ( _("%s: Track and Route List"), VIK_LAYER(vtl)->name );
  vik_trw_layer_track_list_show_dialog ( title, VIK_LAYER(vtl), NULL, trw_layer_create_track_list_both, false );
  free( title );
}

static void trw_layer_waypoint_list_dialog ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);

  char *title = g_strdup_printf ( _("%s: Waypoint List"), VIK_LAYER(vtl)->name );
  vik_trw_layer_waypoint_list_show_dialog ( title, VIK_LAYER(vtl), NULL, trw_layer_create_waypoint_list, false );
  free( title );
}





Track * trw_layer_get_track_helper(menu_array_sublayer values, VikTrwLayer * vtl)
{
	sg_uid_t uid = (sg_uid_t) ((long) values[MA_SUBLAYER_ID]);
	if (KPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE) {
		return vtl->trw->routes.at(uid);
	} else {
		return vtl->trw->tracks.at(uid);
	}
}


LayerTRW::LayerTRW(VikLayer * vl) : Layer(vl)
{
	current_wp = NULL;
	current_wp_uid = 0;
	moving_wp = false;
	waypoint_rightclick = false;

	current_tpl = NULL;
	current_tp_track = NULL;
	current_tp_uid = 0;
	tpwin = NULL;

	moving_tp = false;

	memset(&coord_mode, 0, sizeof (VikCoordMode));

	highest_wp_number = 0;
}
