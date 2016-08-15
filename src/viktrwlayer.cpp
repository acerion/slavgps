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




static void goto_coord(LayersPanel * panel, Layer * layer, Viewport * viewport, const VikCoord * coord);

static void trw_layer_cancel_current_tp_cb(LayerTRW * layer, bool destroy);
static void trw_layer_tpwin_response_cb(LayerTRW * layer, int response);

static LayerTool * tool_edit_trackpoint_create(Window * window, Viewport * viewport);
static bool   tool_edit_trackpoint_click_cb(Layer * trw, GdkEventButton *event, LayerTool * tool);
static bool   tool_edit_trackpoint_move_cb(Layer * trw, GdkEventMotion *event, LayerTool * tool);
static bool   tool_edit_trackpoint_release_cb(Layer * trw, GdkEventButton *event, LayerTool * tool);

static LayerTool * tool_show_picture_create(Window * window, Viewport * viewport);
static bool   tool_show_picture_click_cb(Layer * trw, GdkEventButton *event, LayerTool * tool);

static LayerTool * tool_edit_waypoint_create(Window * window, Viewport * viewport);
static bool   tool_edit_waypoint_click_cb(Layer * trw, GdkEventButton *event, LayerTool * tool);
static bool   tool_edit_waypoint_move_cb(Layer * trw, GdkEventMotion *event, LayerTool * tool);
static bool   tool_edit_waypoint_release_cb(Layer * trw, GdkEventButton *event, LayerTool * tool);

static LayerTool * tool_new_route_create(Window * window, Viewport * viewport);
static bool   tool_new_route_click_cb(Layer * trw, GdkEventButton *event, LayerTool * tool);

static LayerTool * tool_new_track_create(Window * window, Viewport * viewport);
static bool   tool_new_track_click_cb(Layer * trw, GdkEventButton *event, LayerTool * tool);
static VikLayerToolFuncStatus tool_new_track_move_cb(Layer * trw, GdkEventMotion *event, LayerTool * tool);
static void   tool_new_track_release_cb(Layer * trw, GdkEventButton *event, LayerTool * tool);
static bool   tool_new_track_key_press_cb(Layer * trw, GdkEventKey *event, LayerTool * tool);

static LayerTool * tool_new_waypoint_create(Window * window, Viewport * viewport);
static bool   tool_new_waypoint_click_cb(Layer * trw, GdkEventButton *event, LayerTool * tool);

static LayerTool * tool_extended_route_finder_create(Window * window, Viewport * viewport);
static bool   tool_extended_route_finder_click_cb(Layer * trw, GdkEventButton *event, LayerTool * tool);
static bool   tool_extended_route_finder_key_press_cb(Layer * trw, GdkEventKey *event, LayerTool * tool);

static void waypoint_convert(Waypoint * wp, VikCoordMode * dest_mode);

static char * font_size_to_string(int font_size);




// Note for the following tool GtkRadioActionEntry texts:
//  the very first text value is an internal name not displayed anywhere
//  the first N_ text value is the name used for menu entries - hence has an underscore for the keyboard accelerator
//    * remember not to clash with the values used for VikWindow level tools (Pan, Zoom, Ruler + Select)
//  the second N_ text value is used for the button tooltip (i.e. generally don't want an underscore here)
//  the value is always set to 0 and the tool loader in VikWindow will set the actual appropriate value used
static LayerTool * trw_layer_tools[] = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

enum {
	TOOL_CREATE_WAYPOINT = 0,
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

static VikLayerParamData black_color_default(void) {
	VikLayerParamData data; gdk_color_parse("#000000", &data.c); return data; // Black
}
static VikLayerParamData drawmode_default(void) { return VIK_LPD_UINT (DRAWMODE_BY_TRACK); }
static VikLayerParamData line_thickness_default(void) { return VIK_LPD_UINT (1); }
static VikLayerParamData trkpointsize_default(void) { return VIK_LPD_UINT (MIN_POINT_SIZE); }
static VikLayerParamData trkdirectionsize_default(void) { return VIK_LPD_UINT (5); }
static VikLayerParamData bg_line_thickness_default(void) { return VIK_LPD_UINT (0); }
static VikLayerParamData trackbgcolor_default(void) {
	VikLayerParamData data; gdk_color_parse("#FFFFFF", &data.c); return data; // White
}
static VikLayerParamData elevation_factor_default(void) { return VIK_LPD_UINT (30); }
static VikLayerParamData stop_length_default(void) { return VIK_LPD_UINT (60); }
static VikLayerParamData speed_factor_default(void) { return VIK_LPD_DOUBLE (30.0); }

static VikLayerParamData tnfontsize_default(void) { return VIK_LPD_UINT (FS_MEDIUM); }
static VikLayerParamData wpfontsize_default(void) { return VIK_LPD_UINT (FS_MEDIUM); }
static VikLayerParamData wptextcolor_default(void) {
	VikLayerParamData data; gdk_color_parse("#FFFFFF", &data.c); return data; // White
}
static VikLayerParamData wpbgcolor_default(void) {
	VikLayerParamData data; gdk_color_parse("#8383C4", &data.c); return data; // Kind of Blue
}
static VikLayerParamData wpsize_default(void) { return VIK_LPD_UINT(4); }
static VikLayerParamData wpsymbol_default(void) { return VIK_LPD_UINT (WP_SYMBOL_FILLED_SQUARE); }

static VikLayerParamData image_size_default(void) { return VIK_LPD_UINT (64); }
static VikLayerParamData image_alpha_default(void) { return VIK_LPD_UINT (255); }
static VikLayerParamData image_cache_size_default(void) { return VIK_LPD_UINT (300); }

static VikLayerParamData sort_order_default(void) { return VIK_LPD_UINT (0); }

static VikLayerParamData string_default(void)
{
	VikLayerParamData data;
	data.s = "";
	return data;
}

VikLayerParam trw_layer_params[] = {
	{ LayerType::TRW, "tracks_visible",    VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_NOT_IN_PROPERTIES, NULL,                              (VikLayerWidgetType) 0,        NULL,               NULL, NULL, vik_lpd_true_default,       NULL, NULL },
	{ LayerType::TRW, "waypoints_visible", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_NOT_IN_PROPERTIES, NULL,                              (VikLayerWidgetType) 0,        NULL,               NULL, NULL, vik_lpd_true_default,       NULL, NULL },
	{ LayerType::TRW, "routes_visible",    VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_NOT_IN_PROPERTIES, NULL,                              (VikLayerWidgetType) 0,        NULL,               NULL, NULL, vik_lpd_true_default,       NULL, NULL },

	{ LayerType::TRW, "trackdrawlabels",   VIK_LAYER_PARAM_BOOLEAN, GROUP_TRACKS,                N_("Draw Labels"),                 VIK_LAYER_WIDGET_CHECKBUTTON,  NULL,               NULL, N_("Note: the individual track controls what labels may be displayed"), vik_lpd_true_default, NULL, NULL },
	{ LayerType::TRW, "trackfontsize",     VIK_LAYER_PARAM_UINT,    GROUP_TRACKS_ADV,            N_("Track Labels Font Size:"),     VIK_LAYER_WIDGET_COMBOBOX,     params_font_sizes,  NULL, NULL, tnfontsize_default,         NULL, NULL },
	{ LayerType::TRW, "drawmode",          VIK_LAYER_PARAM_UINT,    GROUP_TRACKS,                N_("Track Drawing Mode:"),         VIK_LAYER_WIDGET_COMBOBOX,     params_drawmodes,   NULL, NULL, drawmode_default,           NULL, NULL },
	{ LayerType::TRW, "trackcolor",        VIK_LAYER_PARAM_COLOR,   GROUP_TRACKS,                N_("All Tracks Color:"),           VIK_LAYER_WIDGET_COLOR,        NULL,               NULL, N_("The color used when 'All Tracks Same Color' drawing mode is selected"), black_color_default, NULL, NULL },
	{ LayerType::TRW, "drawlines",         VIK_LAYER_PARAM_BOOLEAN, GROUP_TRACKS,                N_("Draw Track Lines"),            VIK_LAYER_WIDGET_CHECKBUTTON,  NULL,               NULL, NULL, vik_lpd_true_default,       NULL, NULL },
	{ LayerType::TRW, "line_thickness",    VIK_LAYER_PARAM_UINT,    GROUP_TRACKS_ADV,            N_("Track Thickness:"),            VIK_LAYER_WIDGET_SPINBUTTON,   &params_scales[0],  NULL, NULL, line_thickness_default,     NULL, NULL },
	{ LayerType::TRW, "drawdirections",    VIK_LAYER_PARAM_BOOLEAN, GROUP_TRACKS,                N_("Draw Track Direction"),        VIK_LAYER_WIDGET_CHECKBUTTON,  NULL,               NULL, NULL, vik_lpd_false_default,      NULL, NULL },
	{ LayerType::TRW, "trkdirectionsize",  VIK_LAYER_PARAM_UINT,    GROUP_TRACKS_ADV,            N_("Direction Size:"),             VIK_LAYER_WIDGET_SPINBUTTON,   &params_scales[11], NULL, NULL, trkdirectionsize_default,   NULL, NULL },
	{ LayerType::TRW, "drawpoints",        VIK_LAYER_PARAM_BOOLEAN, GROUP_TRACKS,                N_("Draw Trackpoints"),            VIK_LAYER_WIDGET_CHECKBUTTON,  NULL,               NULL, NULL, vik_lpd_true_default,       NULL, NULL },
	{ LayerType::TRW, "trkpointsize",      VIK_LAYER_PARAM_UINT,    GROUP_TRACKS_ADV,            N_("Trackpoint Size:"),            VIK_LAYER_WIDGET_SPINBUTTON,   &params_scales[10], NULL, NULL, trkpointsize_default,       NULL, NULL },
	{ LayerType::TRW, "drawelevation",     VIK_LAYER_PARAM_BOOLEAN, GROUP_TRACKS,                N_("Draw Elevation"),              VIK_LAYER_WIDGET_CHECKBUTTON,  NULL,               NULL, NULL, vik_lpd_false_default,      NULL, NULL },
	{ LayerType::TRW, "elevation_factor",  VIK_LAYER_PARAM_UINT,    GROUP_TRACKS_ADV,            N_("Draw Elevation Height %:"),    VIK_LAYER_WIDGET_HSCALE,       &params_scales[9],  NULL, NULL, elevation_factor_default,   NULL, NULL },
	{ LayerType::TRW, "drawstops",         VIK_LAYER_PARAM_BOOLEAN, GROUP_TRACKS,                N_("Draw Stops"),                  VIK_LAYER_WIDGET_CHECKBUTTON,  NULL,               NULL, N_("Whether to draw a marker when trackpoints are at the same position but over the minimum stop length apart in time"), vik_lpd_false_default, NULL, NULL },
	{ LayerType::TRW, "stop_length",       VIK_LAYER_PARAM_UINT,    GROUP_TRACKS_ADV,            N_("Min Stop Length (seconds):"),  VIK_LAYER_WIDGET_SPINBUTTON,   &params_scales[8],  NULL, NULL, stop_length_default,        NULL, NULL },

	{ LayerType::TRW, "bg_line_thickness", VIK_LAYER_PARAM_UINT,    GROUP_TRACKS_ADV,            N_("Track BG Thickness:"),         VIK_LAYER_WIDGET_SPINBUTTON,   &params_scales[6],  NULL, NULL, bg_line_thickness_default,  NULL, NULL },
	{ LayerType::TRW, "trackbgcolor",      VIK_LAYER_PARAM_COLOR,   GROUP_TRACKS_ADV,            N_("Track Background Color"),      VIK_LAYER_WIDGET_COLOR,        NULL,               NULL, NULL, trackbgcolor_default,       NULL, NULL },
	{ LayerType::TRW, "speed_factor",      VIK_LAYER_PARAM_DOUBLE,  GROUP_TRACKS_ADV,            N_("Draw by Speed Factor (%):"),   VIK_LAYER_WIDGET_HSCALE,       &params_scales[1],  NULL, N_("The percentage factor away from the average speed determining the color used"), speed_factor_default, NULL, NULL },
	{ LayerType::TRW, "tracksortorder",    VIK_LAYER_PARAM_UINT,    GROUP_TRACKS_ADV,            N_("Track Sort Order:"),           VIK_LAYER_WIDGET_COMBOBOX,     params_sort_order,  NULL, NULL, sort_order_default,         NULL, NULL },

	{ LayerType::TRW, "drawlabels",        VIK_LAYER_PARAM_BOOLEAN, GROUP_WAYPOINTS,             N_("Draw Labels"),                 VIK_LAYER_WIDGET_CHECKBUTTON,  NULL,               NULL, NULL, vik_lpd_true_default,       NULL, NULL },
	{ LayerType::TRW, "wpfontsize",        VIK_LAYER_PARAM_UINT,    GROUP_WAYPOINTS,             N_("Waypoint Font Size:"),         VIK_LAYER_WIDGET_COMBOBOX,     params_font_sizes,  NULL, NULL, wpfontsize_default,         NULL, NULL },
	{ LayerType::TRW, "wpcolor",           VIK_LAYER_PARAM_COLOR,   GROUP_WAYPOINTS,             N_("Waypoint Color:"),             VIK_LAYER_WIDGET_COLOR,        NULL,               NULL, NULL, black_color_default,        NULL, NULL },
	{ LayerType::TRW, "wptextcolor",       VIK_LAYER_PARAM_COLOR,   GROUP_WAYPOINTS,             N_("Waypoint Text:"),              VIK_LAYER_WIDGET_COLOR,        NULL,               NULL, NULL, wptextcolor_default,        NULL, NULL },
	{ LayerType::TRW, "wpbgcolor",         VIK_LAYER_PARAM_COLOR,   GROUP_WAYPOINTS,             N_("Background:"),                 VIK_LAYER_WIDGET_COLOR,        NULL,               NULL, NULL, wpbgcolor_default,          NULL, NULL },
	{ LayerType::TRW, "wpbgand",           VIK_LAYER_PARAM_BOOLEAN, GROUP_WAYPOINTS,             N_("Fake BG Color Translucency:"), VIK_LAYER_WIDGET_CHECKBUTTON,  NULL,               NULL, NULL, vik_lpd_false_default,      NULL, NULL },
	{ LayerType::TRW, "wpsymbol",          VIK_LAYER_PARAM_UINT,    GROUP_WAYPOINTS,             N_("Waypoint marker:"),            VIK_LAYER_WIDGET_COMBOBOX,     params_wpsymbols,   NULL, NULL, wpsymbol_default,           NULL, NULL },
	{ LayerType::TRW, "wpsize",            VIK_LAYER_PARAM_UINT,    GROUP_WAYPOINTS,             N_("Waypoint size:"),              VIK_LAYER_WIDGET_SPINBUTTON,   &params_scales[7],  NULL, NULL, wpsize_default,             NULL, NULL },
	{ LayerType::TRW, "wpsyms",            VIK_LAYER_PARAM_BOOLEAN, GROUP_WAYPOINTS,             N_("Draw Waypoint Symbols:"),      VIK_LAYER_WIDGET_CHECKBUTTON,  NULL,               NULL, NULL, vik_lpd_true_default,       NULL, NULL },
	{ LayerType::TRW, "wpsortorder",       VIK_LAYER_PARAM_UINT,    GROUP_WAYPOINTS,             N_("Waypoint Sort Order:"),        VIK_LAYER_WIDGET_COMBOBOX,     params_sort_order,  NULL, NULL, sort_order_default,         NULL, NULL },

	{ LayerType::TRW, "drawimages",        VIK_LAYER_PARAM_BOOLEAN, GROUP_IMAGES,                N_("Draw Waypoint Images"),        VIK_LAYER_WIDGET_CHECKBUTTON,  NULL,               NULL, NULL, vik_lpd_true_default,       NULL, NULL },
	{ LayerType::TRW, "image_size",        VIK_LAYER_PARAM_UINT,    GROUP_IMAGES,                N_("Image Size (pixels):"),        VIK_LAYER_WIDGET_HSCALE,       &params_scales[3],  NULL, NULL, image_size_default,         NULL, NULL },
	{ LayerType::TRW, "image_alpha",       VIK_LAYER_PARAM_UINT,    GROUP_IMAGES,                N_("Image Alpha:"),                VIK_LAYER_WIDGET_HSCALE,       &params_scales[4],  NULL, NULL, image_alpha_default,        NULL, NULL },
	{ LayerType::TRW, "image_cache_size",  VIK_LAYER_PARAM_UINT,    GROUP_IMAGES,                N_("Image Memory Cache Size:"),    VIK_LAYER_WIDGET_HSCALE,       &params_scales[5],  NULL, NULL, image_cache_size_default,   NULL, NULL },

	{ LayerType::TRW, "metadatadesc",      VIK_LAYER_PARAM_STRING,  GROUP_METADATA,              N_("Description"),                 VIK_LAYER_WIDGET_ENTRY,        NULL,               NULL, NULL, string_default,             NULL, NULL },
	{ LayerType::TRW, "metadataauthor",    VIK_LAYER_PARAM_STRING,  GROUP_METADATA,              N_("Author"),                      VIK_LAYER_WIDGET_ENTRY,        NULL,               NULL, NULL, string_default,             NULL, NULL },
	{ LayerType::TRW, "metadatatime",      VIK_LAYER_PARAM_STRING,  GROUP_METADATA,              N_("Creation Time"),               VIK_LAYER_WIDGET_ENTRY,        NULL,               NULL, NULL, string_default,             NULL, NULL },
	{ LayerType::TRW, "metadatakeywords",  VIK_LAYER_PARAM_STRING,  GROUP_METADATA,              N_("Keywords"),                    VIK_LAYER_WIDGET_ENTRY,        NULL,               NULL, NULL, string_default,             NULL, NULL },
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
 *** 2) Handle in get_param & set_param (presumably adding on to LayerTRW class)
 ***/

/****** END PARAMETERS ******/

/* Layer Interface function definitions */
static Layer * trw_layer_unmarshall(uint8_t * data, int len, Viewport * viewport);
static void trw_layer_change_param(GtkWidget * widget, ui_change_values * values);
/* End Layer Interface function definitions */

VikLayerInterface vik_trw_layer_interface = {
	"TrackWaypoint",
	N_("TrackWaypoint"),
	"<control><shift>Y",
	&viktrwlayer_pixbuf,

	{ tool_new_waypoint_create,          /* (VikToolConstructorFunc) */
	  tool_new_track_create,             /* (VikToolConstructorFunc) */
	  tool_new_route_create,             /* (VikToolConstructorFunc) */
	  tool_extended_route_finder_create, /* (VikToolConstructorFunc) */
	  tool_edit_waypoint_create,         /* (VikToolConstructorFunc) */
	  tool_edit_trackpoint_create,       /* (VikToolConstructorFunc) */
	  tool_show_picture_create },        /* (VikToolConstructorFunc) */

	trw_layer_tools,
	7,

	trw_layer_params,
	NUM_PARAMS,
	params_groups, /* params_groups */
	sizeof(params_groups)/sizeof(params_groups[0]),    /* number of groups */

	VIK_MENU_ITEM_ALL,

	/* (VikLayerFuncUnmarshall) */   trw_layer_unmarshall,

	/* (VikLayerFuncSetParam) */     layer_set_param,
	/* (VikLayerFuncGetParam) */     layer_get_param,
	/* (VikLayerFuncChangeParam) */  trw_layer_change_param,
};




bool have_diary_program = false;
char *diary_program = NULL;
#define VIK_SETTINGS_EXTERNAL_DIARY_PROGRAM "external_diary_program"

bool have_geojson_export = false;

bool have_astro_program = false;
char *astro_program = NULL;
#define VIK_SETTINGS_EXTERNAL_ASTRO_PROGRAM "external_astro_program"




// NB Only performed once per program run
static void vik_trwlayer_class_init(VikTrwLayerClass *klass)
{
	if (!a_settings_get_string(VIK_SETTINGS_EXTERNAL_DIARY_PROGRAM, &diary_program)) {
#ifdef WINDOWS
		//diary_program = strdup("C:\\Program Files\\Rednotebook\\rednotebook.exe");
		diary_program = strdup("C:/Progra~1/Rednotebook/rednotebook.exe");
#else
		diary_program = strdup("rednotebook");
#endif
	} else {
		// User specified so assume it works
		have_diary_program = true;
	}

	if (g_find_program_in_path(diary_program)) {
		char *mystdout = NULL;
		char *mystderr = NULL;
		// Needs RedNotebook 1.7.3+ for support of opening on a specified date
		char *cmd = g_strconcat(diary_program, " --version", NULL); // "rednotebook --version"
		if (g_spawn_command_line_sync(cmd, &mystdout, &mystderr, NULL, NULL)) {
			// Annoyingly 1.7.1|2|3 versions of RedNotebook prints the version to stderr!!
			if (mystdout) {
				fprintf(stderr, "DEBUG: Diary: %s\n", mystdout); // Should be something like 'RedNotebook 1.4'
			}
			if (mystderr) {
				fprintf(stderr, "WARNING: Diary: stderr: %s\n", mystderr);
			}

			char **tokens = NULL;
			if (mystdout && strcmp(mystdout, "")) {
				tokens = g_strsplit(mystdout, " ", 0);
			} else if (mystderr) {
				tokens = g_strsplit(mystderr, " ", 0);
			}

			if (tokens) {
				int num = 0;
				char *token = tokens[num];
				while (token && num < 2) {
					if (num == 1) {
						if (viking_version_to_number(token) >= viking_version_to_number((char *) "1.7.3")) {
							have_diary_program = true;
						}
					}
					num++;
					token = tokens[num];
				}
			}
			g_strfreev(tokens);
		}
		free(mystdout);
		free(mystderr);
		free(cmd);
	}

	if (g_find_program_in_path(a_geojson_program_export())) {
		have_geojson_export = true;
	}

	// Astronomy Domain
	if (!a_settings_get_string(VIK_SETTINGS_EXTERNAL_ASTRO_PROGRAM, &astro_program)) {
#ifdef WINDOWS
		//astro_program = strdup("C:\\Program Files\\Stellarium\\stellarium.exe");
		astro_program = strdup("C:/Progra~1/Stellarium/stellarium.exe");
#else
		astro_program = strdup("stellarium");
#endif
	} else {
		// User specified so assume it works
		have_astro_program = true;
	}
	if (g_find_program_in_path(astro_program)) {
		have_astro_program = true;
	}
}




GType vik_trw_layer_get_type()
{
	static GType vtl_type = 0;

	if (!vtl_type) {
		static const GTypeInfo vtl_info = {
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
		vtl_type = g_type_register_static(VIK_LAYER_TYPE, "VikTrwLayer", &vtl_info, (GTypeFlags) 0);
	}
	return vtl_type;
}




TRWMetadata * LayerTRW::metadata_new()
{
	TRWMetadata * data = (TRWMetadata *) malloc(sizeof (TRWMetadata));
	memset(data, 0, sizeof (TRWMetadata));

	return data;
}




void LayerTRW::metadata_free(TRWMetadata *metadata)
{
	free(metadata);
}




TRWMetadata * LayerTRW::get_metadata()
{
	return this->metadata;
}




void LayerTRW::set_metadata(TRWMetadata * metadata)
{
	if (this->metadata) {
		LayerTRW::metadata_free(this->metadata);
	}
	this->metadata = metadata;
}




void TRWMetadata::set_author(char const * new_author)
{
	if (this->author) {
		free(this->author);
		this->author = NULL;
	}
	if (new_author) {
		this->author = strdup(new_author);
	}
	return;
}




void TRWMetadata::set_description(char const * new_description)
{
	if (this->description) {
		free(this->description);
		this->description = NULL;
	}
	if (new_description) {
		this->description = strdup(new_description);
	}
	return;
}




void TRWMetadata::set_keywords(char const * new_keywords)
{
	if (this->keywords) {
		free(this->keywords);
		this->keywords = NULL;
	}
	if (new_keywords) {
		this->keywords = strdup(new_keywords);
	}
	return;
}




void TRWMetadata::set_timestamp(char const * new_timestamp)
{
	if (this->timestamp) {
		free(this->timestamp);
		this->timestamp = NULL;
	}
	if (new_timestamp) {
		this->timestamp = strdup(new_timestamp);
	}
	return;
}




/**
 * Find an item by date
 */
bool LayerTRW::find_by_date(char const * date_str, VikCoord * position, Viewport * viewport, bool do_tracks, bool select)
{
	date_finder_type df;
	df.found = false;
	df.date_str = date_str;
	df.trk = NULL;
	df.wp = NULL;
	// Only tracks ATM
	if (do_tracks) {
		LayerTRWc::find_track_by_date(tracks, &df);
	} else {
		LayerTRWc::find_waypoint_by_date(waypoints, &df);
	}

	if (select && df.found) {
		if (do_tracks && df.trk) {
			struct LatLon maxmin[2] = { {0,0}, {0,0} };
			LayerTRW::find_maxmin_in_track(df.trk, maxmin);
			this->zoom_to_show_latlons(viewport, maxmin);
			this->tree_view->select_and_expose(tracks_iters.at(df.trk_uid));
		} else if (df.wp) {
			viewport->set_center_coord(&df.wp->coord, true);
			this->tree_view->select_and_expose(waypoints_iters.at(df.wp_uid));
		}
		this->emit_update();
	}
	return df.found;
}





void LayerTRW::delete_sublayer(SublayerType sublayer_type, sg_uid_t sublayer_uid)
{
	if (sublayer_uid == SG_UID_NONE) {
		return;
	}

	static trw_menu_sublayer_t data;
	memset(&data, 0, sizeof (trw_menu_sublayer_t));

	data.layer       = this;
	data.sublayer_type = sublayer_type;
	data.sublayer_uid = sublayer_uid;
	data.confirm     = true;  // Confirm delete request

	trw_layer_delete_item(&data);
}





void LayerTRW::cut_sublayer(SublayerType sublayer_type, sg_uid_t sublayer_uid)
{
	if (sublayer_uid == SG_UID_NONE) {
		return;
	}

	static trw_menu_sublayer_t data;
	memset(&data, 0, sizeof (trw_menu_sublayer_t));

	data.layer       = this;
	data.sublayer_type = sublayer_type;
	data.sublayer_uid = sublayer_uid;
	data.confirm     = true; // Confirm delete request

	trw_layer_copy_item_cb(&data);
	trw_layer_cut_item_cb(&data);
}





void trw_layer_copy_item_cb(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	SublayerType sublayer_type = data->sublayer_type;
	sg_uid_t sublayer_uid = data->sublayer_uid;

	uint8_t *data_ = NULL;
	unsigned int len;

	data->layer->copy_sublayer(sublayer_type, sublayer_uid, &data_, &len);

	if (data_) {
		const char* name = NULL;
		if (sublayer_type == SublayerType::WAYPOINT) {
			Waypoint * wp = layer->waypoints.at(sublayer_uid);
			if (wp && wp->name) {
				name = wp->name;
			} else {
				name = NULL; // Broken :(
			}
		} else if (sublayer_type == SublayerType::TRACK) {
			Track * trk = layer->tracks.at(sublayer_uid);
			if (trk && trk->name) {
				name = trk->name;
			} else {
				name = NULL; // Broken :(
			}
		} else {
			Track * trk = layer->routes.at(sublayer_uid);
			if (trk && trk->name) {
				name = trk->name;
			} else {
				name = NULL; // Broken :(
			}
		}

		a_clipboard_copy(VIK_CLIPBOARD_DATA_SUBLAYER, LayerType::TRW,
				 sublayer_type, len, name, data_);
	}
}




void trw_layer_cut_item_cb(trw_menu_sublayer_t * data)
{
	trw_layer_copy_item_cb(data);
	data->confirm = false; // Never need to confirm automatic delete
	trw_layer_delete_item(data);
}




void trw_layer_paste_item_cb(trw_menu_sublayer_t * data)
{
	// Slightly cheating method, routing via the panels capability
	a_clipboard_paste(data->panel);
}




void LayerTRW::copy_sublayer(SublayerType sublayer_type, sg_uid_t sublayer_uid, uint8_t **item, unsigned int *len)
{
	if (sublayer_uid == SG_UID_NONE) {
		*item = NULL;
		return;
	}

	uint8_t *id;
	size_t il;

	GByteArray *ba = g_byte_array_new();
	sg_uid_t uid = sublayer_uid;

	if (sublayer_type == SublayerType::WAYPOINT) {
		this->waypoints.at(uid)->marshall(&id, &il);
	} else if (sublayer_type == SublayerType::TRACK) {
		this->tracks.at(uid)->marshall(&id, &il);
	} else {
		this->routes.at(uid)->marshall(&id, &il);
	}

	g_byte_array_append(ba, id, il);

	std::free(id);

	*len = ba->len;
	*item = ba->data;
}




bool LayerTRW::paste_sublayer(SublayerType sublayer_type, uint8_t * item, size_t len)
{
	if (!item) {
		return false;
	}

	char *name;

	if (sublayer_type == SublayerType::WAYPOINT) {
		Waypoint * wp = Waypoint::unmarshall(item, len);
		// When copying - we'll create a new name based on the original
		name = this->new_unique_sublayer_name(SublayerType::WAYPOINT, wp->name);
		this->add_waypoint(wp, name);
		waypoint_convert(wp, &this->coord_mode);
		std::free(name);

		this->calculate_bounds_waypoints();

		// Consider if redraw necessary for the new item
		if (this->visible && this->waypoints_visible && wp->visible) {
			this->emit_update();
		}
		return true;
	}
	if (sublayer_type == SublayerType::TRACK) {
		Track * trk = Track::unmarshall(item, len);
		// When copying - we'll create a new name based on the original
		name = this->new_unique_sublayer_name(SublayerType::TRACK, trk->name);
		this->add_track(trk, name);
		trk->convert(this->coord_mode);
		std::free(name);

		// Consider if redraw necessary for the new item
		if (this->visible && this->tracks_visible && trk->visible) {
			this->emit_update();
		}
		return true;
	}
	if (sublayer_type == SublayerType::ROUTE) {
		Track * trk = Track::unmarshall(item, len);
		// When copying - we'll create a new name based on the original
		name = this->new_unique_sublayer_name(SublayerType::ROUTE, trk->name);
		this->add_route(trk, name);
		trk->convert(this->coord_mode);
		std::free(name);

		// Consider if redraw necessary for the new item
		if (this->visible && this->routes_visible && trk->visible) {
			this->emit_update();
		}
		return true;
	}
	return false;
}




#if 0
void trw_layer_free_copied_item(int subtype, void * item)
{
	if (item) {
		free(item);
	}
}
#endif




void LayerTRW::image_cache_free()
{
	g_list_foreach(this->image_cache->head, (GFunc) cached_pixbuf_free, NULL);
	g_queue_free(this->image_cache);
}




char * font_size_to_string(int font_size)
{
	char * result = NULL;
	switch (font_size) {
	case FS_XX_SMALL:
		result = strdup("xx-small");
		break;
	case FS_X_SMALL:
		result = strdup("x-small");
		break;
	case FS_SMALL:
		result = strdup("small");
		break;
	case FS_LARGE:
		result = strdup("large");
		break;
	case FS_X_LARGE:
		result = strdup("x-large");
		break;
	case FS_XX_LARGE:
		result = strdup("xx-large");
		break;
	default:
		result = strdup("medium");
		break;
	}

	return result;
}




bool LayerTRW::set_param(uint16_t id, VikLayerParamData data, Viewport * viewport, bool is_file_operation)
{
	switch (id) {
	case PARAM_TV:
		this->tracks_visible = data.b;
		break;
	case PARAM_WV:
		this->waypoints_visible = data.b;
		break;
	case PARAM_RV:
		this->routes_visible = data.b;
		break;
	case PARAM_TDL:
		this->track_draw_labels = data.b;
		break;
	case PARAM_TLFONTSIZE:
		if (data.u < FS_NUM_SIZES) {
			this->track_font_size = (font_size_t) data.u;
			free(this->track_fsize_str);
			this->track_fsize_str = font_size_to_string(this->track_font_size);
		}
		break;
	case PARAM_DM:
		this->drawmode = data.u;
		break;
	case PARAM_TC:
		this->track_color = data.c;
		if (viewport) {
			this->new_track_gcs(viewport);
		}
		break;
	case PARAM_DP:
		this->drawpoints = data.b;
		break;
	case PARAM_DPS:
		if (data.u >= MIN_POINT_SIZE && data.u <= MAX_POINT_SIZE) {
			this->drawpoints_size = data.u;
		}
		break;
	case PARAM_DE:
		this->drawelevation = data.b;
		break;
	case PARAM_DS:
		this->drawstops = data.b;
		break;
	case PARAM_DL:
		this->drawlines = data.b;
		break;
	case PARAM_DD:
		this->drawdirections = data.b;
		break;
	case PARAM_DDS:
		if (data.u >= MIN_ARROW_SIZE && data.u <= MAX_ARROW_SIZE) {
			this->drawdirections_size = data.u;
		}
		break;
	case PARAM_SL:
		if (data.u >= MIN_STOP_LENGTH && data.u <= MAX_STOP_LENGTH) {
			this->stop_length = data.u;
		}
		break;
	case PARAM_EF:
		if (data.u >= 1 && data.u <= 100) {
			this->elevation_factor = data.u;
		}
		break;
	case PARAM_LT:
		if (data.u > 0 && data.u < 15 && data.u != this->line_thickness) {
			this->line_thickness = data.u;
			if (viewport) {
				this->new_track_gcs(viewport);
			}
		}
		break;
	case PARAM_BLT:
		if (data.u <= 8 && data.u != this->bg_line_thickness) {
			this->bg_line_thickness = data.u;
			if (viewport) {
				this->new_track_gcs(viewport);
			}
		}
		break;
	case PARAM_TBGC:
		this->track_bg_color = data.c;
		if (this->track_bg_gc) {
			gdk_gc_set_rgb_fg_color(this->track_bg_gc, &(this->track_bg_color));
		}
		break;
	case PARAM_TDSF:
		this->track_draw_speed_factor = data.d;
		break;
	case PARAM_TSO:
		if (data.u < VL_SO_LAST) {
			this->track_sort_order = (vik_layer_sort_order_t) data.u;
		}
		break;
	case PARAM_DLA:
		this->drawlabels = data.b;
		break;
	case PARAM_DI:
		this->drawimages = data.b;
		break;
	case PARAM_IS:
		if (data.u != this->image_size) {
			this->image_size = data.u;
			this->image_cache_free();
			this->image_cache = g_queue_new();
		}
		break;
	case PARAM_IA:
		if (data.u != this->image_alpha) {
			this->image_alpha = data.u;
			this->image_cache_free();
			this->image_cache = g_queue_new();
		}
		break;
	case PARAM_ICS:
		this->image_cache_size = data.u;
		while (this->image_cache->length > this->image_cache_size) {/* if shrinking cache_size, free pixbuf ASAP */
			cached_pixbuf_free((CachedPixbuf *) g_queue_pop_tail(this->image_cache));
		}
		break;
	case PARAM_WPC:
		this->waypoint_color = data.c;
		if (this->waypoint_gc) {
			gdk_gc_set_rgb_fg_color(this->waypoint_gc, &(this->waypoint_color));
		}
		break;
	case PARAM_WPTC:
		this->waypoint_text_color = data.c;
		if (this->waypoint_text_gc) {
			gdk_gc_set_rgb_fg_color(this->waypoint_text_gc, &(this->waypoint_text_color));
		}
		break;
	case PARAM_WPBC:
		this->waypoint_bg_color = data.c;
		if (this->waypoint_bg_gc) {
			gdk_gc_set_rgb_fg_color(this->waypoint_bg_gc, &(this->waypoint_bg_color));
		}
		break;
	case PARAM_WPBA:
		this->wpbgand = (GdkFunction) data.b;
		if (this->waypoint_bg_gc) {
			gdk_gc_set_function(this->waypoint_bg_gc, data.b ? GDK_AND : GDK_COPY);
		}
		break;
	case PARAM_WPSYM:
		if (data.u < WP_NUM_SYMBOLS) {
			this->wp_symbol = data.u;
		}
		break;
	case PARAM_WPSIZE:
		if (data.u > 0 && data.u <= 64) {
			this->wp_size = data.u;
		}
		break;
	case PARAM_WPSYMS:
		this->wp_draw_symbols = data.b;
		break;
	case PARAM_WPFONTSIZE:
		if (data.u < FS_NUM_SIZES) {
			this->wp_font_size = (font_size_t) data.u;
			free(this->wp_fsize_str);
			this->wp_fsize_str = font_size_to_string(this->wp_font_size);
		}
		break;
	case PARAM_WPSO:
		if (data.u < VL_SO_LAST) {
			this->wp_sort_order = (vik_layer_sort_order_t) data.u;
		}
		break;
		// Metadata
	case PARAM_MDDESC:
		if (data.s && this->metadata) {
			this->metadata->set_description(data.s);
		}
		break;
	case PARAM_MDAUTH:
		if (data.s && this->metadata) {
			this->metadata->set_author(data.s);
		}
		break;
	case PARAM_MDTIME:
		if (data.s && this->metadata) {
			this->metadata->set_timestamp(data.s);
		}
		break;
	case PARAM_MDKEYS:
		if (data.s && this->metadata) {
			this->metadata->set_keywords(data.s);
		}
		break;
	default: break;
	}
	return true;
}




VikLayerParamData LayerTRW::get_param(uint16_t id, bool is_file_operation) const
{
	VikLayerParamData rv;
	switch (id) {
	case PARAM_TV: rv.b = this->tracks_visible; break;
	case PARAM_WV: rv.b = this->waypoints_visible; break;
	case PARAM_RV: rv.b = this->routes_visible; break;
	case PARAM_TDL: rv.b = this->track_draw_labels; break;
	case PARAM_TLFONTSIZE: rv.u = this->track_font_size; break;
	case PARAM_DM: rv.u = this->drawmode; break;
	case PARAM_TC: rv.c = this->track_color; break;
	case PARAM_DP: rv.b = this->drawpoints; break;
	case PARAM_DPS: rv.u = this->drawpoints_size; break;
	case PARAM_DE: rv.b = this->drawelevation; break;
	case PARAM_EF: rv.u = this->elevation_factor; break;
	case PARAM_DS: rv.b = this->drawstops; break;
	case PARAM_SL: rv.u = this->stop_length; break;
	case PARAM_DL: rv.b = this->drawlines; break;
	case PARAM_DD: rv.b = this->drawdirections; break;
	case PARAM_DDS: rv.u = this->drawdirections_size; break;
	case PARAM_LT: rv.u = this->line_thickness; break;
	case PARAM_BLT: rv.u = this->bg_line_thickness; break;
	case PARAM_DLA: rv.b = this->drawlabels; break;
	case PARAM_DI: rv.b = this->drawimages; break;
	case PARAM_TBGC: rv.c = this->track_bg_color; break;
	case PARAM_TDSF: rv.d = this->track_draw_speed_factor; break;
	case PARAM_TSO: rv.u = this->track_sort_order; break;
	case PARAM_IS: rv.u = this->image_size; break;
	case PARAM_IA: rv.u = this->image_alpha; break;
	case PARAM_ICS: rv.u = this->image_cache_size; break;
	case PARAM_WPC: rv.c = this->waypoint_color; break;
	case PARAM_WPTC: rv.c = this->waypoint_text_color; break;
	case PARAM_WPBC: rv.c = this->waypoint_bg_color; break;
	case PARAM_WPBA: rv.b = this->wpbgand; break;
	case PARAM_WPSYM: rv.u = this->wp_symbol; break;
	case PARAM_WPSIZE: rv.u = this->wp_size; break;
	case PARAM_WPSYMS: rv.b = this->wp_draw_symbols; break;
	case PARAM_WPFONTSIZE: rv.u = this->wp_font_size; break;
	case PARAM_WPSO: rv.u = this->wp_sort_order; break;
		// Metadata
	case PARAM_MDDESC:
		if (this->metadata) {
			rv.s = this->metadata->description;
		}
		break;
	case PARAM_MDAUTH:
		if (this->metadata) {
			rv.s = this->metadata->author;
		}
		break;
	case PARAM_MDTIME:
		if (this->metadata) {
			rv.s = this->metadata->timestamp;
		}
		break;
	case PARAM_MDKEYS:
		if (this->metadata) {
			rv.s = this->metadata->keywords;
		}
		break;
	default: break;
	}
	return rv;
}




static void trw_layer_change_param(GtkWidget * widget, ui_change_values * values)
{
	// This '-3' is to account for the first few parameters not in the properties
	const int OFFSET = -3;

	switch (values->param_id) {
		// Alter sensitivity of waypoint draw image related widgets according to the draw image setting.
	case PARAM_DI: {
		// Get new value
		VikLayerParamData vlpd = a_uibuilder_widget_get_value(widget, values->param);
		GtkWidget **ww1 = values->widgets;
		GtkWidget **ww2 = values->labels;
		GtkWidget *w1 = ww1[OFFSET + PARAM_IS];
		GtkWidget *w2 = ww2[OFFSET + PARAM_IS];
		GtkWidget *w3 = ww1[OFFSET + PARAM_IA];
		GtkWidget *w4 = ww2[OFFSET + PARAM_IA];
		GtkWidget *w5 = ww1[OFFSET + PARAM_ICS];
		GtkWidget *w6 = ww2[OFFSET + PARAM_ICS];
		if (w1) gtk_widget_set_sensitive(w1, vlpd.b);
		if (w2) gtk_widget_set_sensitive(w2, vlpd.b);
		if (w3) gtk_widget_set_sensitive(w3, vlpd.b);
		if (w4) gtk_widget_set_sensitive(w4, vlpd.b);
		if (w5) gtk_widget_set_sensitive(w5, vlpd.b);
		if (w6) gtk_widget_set_sensitive(w6, vlpd.b);
		break;
	}
		// Alter sensitivity of waypoint label related widgets according to the draw label setting.
	case PARAM_DLA: {
		// Get new value
		VikLayerParamData vlpd = a_uibuilder_widget_get_value(widget, values->param);
		GtkWidget **ww1 = values->widgets;
		GtkWidget **ww2 = values->labels;
		GtkWidget *w1 = ww1[OFFSET + PARAM_WPTC];
		GtkWidget *w2 = ww2[OFFSET + PARAM_WPTC];
		GtkWidget *w3 = ww1[OFFSET + PARAM_WPBC];
		GtkWidget *w4 = ww2[OFFSET + PARAM_WPBC];
		GtkWidget *w5 = ww1[OFFSET + PARAM_WPBA];
		GtkWidget *w6 = ww2[OFFSET + PARAM_WPBA];
		GtkWidget *w7 = ww1[OFFSET + PARAM_WPFONTSIZE];
		GtkWidget *w8 = ww2[OFFSET + PARAM_WPFONTSIZE];
		if (w1) gtk_widget_set_sensitive(w1, vlpd.b);
		if (w2) gtk_widget_set_sensitive(w2, vlpd.b);
		if (w3) gtk_widget_set_sensitive(w3, vlpd.b);
		if (w4) gtk_widget_set_sensitive(w4, vlpd.b);
		if (w5) gtk_widget_set_sensitive(w5, vlpd.b);
		if (w6) gtk_widget_set_sensitive(w6, vlpd.b);
		if (w7) gtk_widget_set_sensitive(w7, vlpd.b);
		if (w8) gtk_widget_set_sensitive(w8, vlpd.b);
		break;
	}
		// Alter sensitivity of all track colours according to the draw track mode.
	case PARAM_DM: {
		// Get new value
		VikLayerParamData vlpd = a_uibuilder_widget_get_value(widget, values->param);
		bool sensitive = (vlpd.u == DRAWMODE_ALL_SAME_COLOR);
		GtkWidget **ww1 = values->widgets;
		GtkWidget **ww2 = values->labels;
		GtkWidget *w1 = ww1[OFFSET + PARAM_TC];
		GtkWidget *w2 = ww2[OFFSET + PARAM_TC];
		if (w1) gtk_widget_set_sensitive(w1, sensitive);
		if (w2) gtk_widget_set_sensitive(w2, sensitive);
		break;
	}
	case PARAM_MDTIME: {
		// Force metadata->timestamp to be always read-only for now.
		GtkWidget **ww = values->widgets;
		GtkWidget *w1 = ww[OFFSET + PARAM_MDTIME];
		if (w1) gtk_widget_set_sensitive(w1, false);
	}
		// NB Since other track settings have been split across tabs,
		// I don't think it's useful to set sensitivities on widgets you can't immediately see
	default: break;
	}
}




void LayerTRW::marshall(uint8_t **data, int *len)
{
	uint8_t *pd;
	int pl;

	*data = NULL;

	// Use byte arrays to store sublayer data
	// much like done elsewhere e.g. Layer::marshall_params()
	GByteArray *ba = g_byte_array_new();

	uint8_t *sl_data;
	size_t sl_len;

	unsigned int object_length;
	unsigned int subtype;
	// store:
	// the length of the item
	// the sublayer type of item
	// the the actual item
#define tlm_append(object_pointer, size, type)				\
	subtype = (int) (type);						\
	object_length = (size);						\
	g_byte_array_append(ba, (uint8_t *)&object_length, sizeof(object_length)); \
	g_byte_array_append(ba, (uint8_t *)&subtype, sizeof(subtype));	\
	g_byte_array_append(ba, (object_pointer), object_length);

	// Layer parameters first
	this->marshall_params(&pd, &pl);
	g_byte_array_append(ba, (uint8_t *)&pl, sizeof(pl));
	g_byte_array_append(ba, pd, pl);
	std::free(pd);

	// Waypoints
	for (auto i = this->waypoints.begin(); i != this->waypoints.end(); i++) {
		i->second->marshall(&sl_data, &sl_len);
		tlm_append(sl_data, sl_len, SublayerType::WAYPOINT);
		std::free(sl_data);
	}

	// Tracks
	for (auto i = this->tracks.begin(); i != this->tracks.end(); i++) {
		i->second->marshall(&sl_data, &sl_len);
		tlm_append(sl_data, sl_len, SublayerType::TRACK);
		std::free(sl_data);
	}

	// Routes
	for (auto i = this->routes.begin(); i != this->routes.end(); i++) {
		i->second->marshall(&sl_data, &sl_len);
		tlm_append(sl_data, sl_len, SublayerType::ROUTE);
		std::free(sl_data);
	}

#undef tlm_append

	*data = ba->data;
	*len = ba->len;
}




static Layer * trw_layer_unmarshall(uint8_t * data, int len, Viewport * viewport)
{
	LayerTRW * trw = new LayerTRW(viewport);

	int pl;

	// First the overall layer parameters
	memcpy(&pl, data, sizeof(pl));
	data += sizeof(pl);
	trw->unmarshall_params(data, pl, viewport);
	data += pl;

	int consumed_length = pl;
	const int sizeof_len_and_subtype = sizeof(int) + sizeof(int);

#define tlm_size (*(int *)data)
	// See marshalling above for order of how this is written
#define tlm_next				\
	data += sizeof_len_and_subtype + tlm_size;

	// Now the individual sublayers:

	while (*data && consumed_length < len) {
		// Normally four extra bytes at the end of the datastream
		//  (since it's a GByteArray and that's where it's length is stored)
		//  So only attempt read when there's an actual block of sublayer data
		if (consumed_length + tlm_size < len) {

			// Reuse pl to read the subtype from the data stream
			memcpy(&pl, data+sizeof(int), sizeof(pl));

			SublayerType sublayer_type = (SublayerType) pl;

			// Also remember to (attempt to) convert each coordinate in case this is pasted into a different drawmode
			if (sublayer_type == SublayerType::TRACK) {
				Track * trk = Track::unmarshall(data + sizeof_len_and_subtype, 0);
				char *name = g_strdup(trk->name);
				trw->add_track(trk, name);
				std::free(name);
				trk->convert(trw->coord_mode);
			}
			if (sublayer_type == SublayerType::WAYPOINT) {
				Waypoint * wp = Waypoint::unmarshall(data + sizeof_len_and_subtype, 0);
				char *name = g_strdup(wp->name);
				trw->add_waypoint(wp, name);
				std::free(name);
				waypoint_convert(wp, &trw->coord_mode);
			}
			if (sublayer_type == SublayerType::ROUTE) {
				Track * trk = Track::unmarshall(data + sizeof_len_and_subtype, 0);
				char *name = g_strdup(trk->name);
				trw->add_route(trk, name);
				std::free(name);
				trk->convert(trw->coord_mode);
			}
		}
		consumed_length += tlm_size + sizeof_len_and_subtype;
		tlm_next;
	}
	//fprintf(stderr, "DEBUG: consumed_length %d vs len %d\n", consumed_length, len);

	// Not stored anywhere else so need to regenerate
	trw->calculate_bounds_waypoints();

	return trw;
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




LayerTRW::~LayerTRW()
{
	/* kamilTODO: call destructors of objects in these maps. */
	this->waypoints.clear();
	this->waypoints_iters.clear();
	this->tracks.clear();
	this->tracks_iters.clear();
	this->routes.clear();
	this->routes_iters.clear();

	/* ODC: replace with GArray */
	this->free_track_gcs();

	if (this->wp_right_click_menu) {
		g_object_ref_sink(G_OBJECT(this->wp_right_click_menu));
	}

	if (this->track_right_click_menu) {
		g_object_ref_sink(G_OBJECT(this->track_right_click_menu));
	}

	if (this->tracklabellayout != NULL) {
		g_object_unref(G_OBJECT (this->tracklabellayout));
	}

	if (this->wplabellayout != NULL) {
		g_object_unref(G_OBJECT (this->wplabellayout));
	}

	if (this->waypoint_gc != NULL) {
		g_object_unref(G_OBJECT (this->waypoint_gc));
	}

	if (this->waypoint_text_gc != NULL) {
		g_object_unref(G_OBJECT (this->waypoint_text_gc));
	}

	if (this->waypoint_bg_gc != NULL) {
		g_object_unref(G_OBJECT (this->waypoint_bg_gc));
	}

	free(this->wp_fsize_str);
	free(this->track_fsize_str);

	if (this->tpwin != NULL)
		gtk_widget_destroy(GTK_WIDGET(this->tpwin));

	if (this->tracks_analysis_dialog != NULL) {
		gtk_widget_destroy(GTK_WIDGET(this->tracks_analysis_dialog));
	}

	this->image_cache_free();
}




void LayerTRW::draw_with_highlight(Viewport * viewport, bool highlight)
{
	static DrawingParams dp;
	init_drawing_params(&dp, this, viewport, highlight);

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




void LayerTRW::draw(Viewport * viewport)
{
	// If this layer is to be highlighted - then don't draw now - as it will be drawn later on in the specific highlight draw stage
	// This may seem slightly inefficient to test each time for every layer
	//  but for a layer with *lots* of tracks & waypoints this can save some effort by not drawing the items twice
	if (viewport->get_draw_highlight()
	    && window_from_layer(this)->get_selected_trw_layer() == this) {

		return;
	}

	this->draw_with_highlight(viewport, false);
}




void LayerTRW::draw_highlight(Viewport * viewport)
{
	/* kamilFIXME: enabling this code and then compiling it with -O0 results in crash when selecting trackpoint in viewport. */
#if 0
	// Check the layer for visibility (including all the parents visibilities)
	if (!this->tree_view->is_visible_in_tree(&this->iter)) {
		return;
	}
#endif
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
	/* kamilFIXME: enabling this code and then compiling it with -O0 results in crash when selecting trackpoint in viewport. */
#if 0
	// Check the layer for visibility (including all the parents visibilities)
	if (!this->tree_view->is_visible_in_tree(&this->iter)) {
		return;
	}
#endif

	static DrawingParams dp;
	init_drawing_params(&dp, this, viewport, true);

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
void LayerTRW::draw_highlight_items(std::unordered_map<sg_uid_t, Track *> * tracks, std::unordered_map<sg_uid_t, Waypoint *> * selected_waypoints, Viewport * viewport)
{
	/* kamilFIXME: enabling this code and then compiling it with -O0 results in crash when selecting trackpoint in viewport. */
#if 0
	// Check the layer for visibility (including all the parents visibilities)
	if (!this->tree_view->is_visible_in_tree(&this->iter)) {
		return;
	}
#endif

	static DrawingParams dp;
	init_drawing_params(&dp, this, viewport, true);

	if (tracks) {
		bool is_routes = (tracks == &routes);
		bool draw = (is_routes && routes_visible) || (!is_routes && tracks_visible);
		if (draw) {
			trw_layer_draw_track_cb(*tracks, &dp);
		}
	}

	if (waypoints_visible && selected_waypoints) {
		trw_layer_draw_waypoints_cb(selected_waypoints, &dp);
	}
}




void LayerTRW::free_track_gcs()
{
	if (this->track_bg_gc) {
		g_object_unref(this->track_bg_gc);
		this->track_bg_gc = NULL;
	}
	if (this->track_1color_gc) {
		g_object_unref(this->track_1color_gc);
		this->track_1color_gc = NULL;
	}
	if (this->current_track_gc) {
		g_object_unref(this->current_track_gc);
		this->current_track_gc = NULL;
	}
	if (this->current_track_newpoint_gc) {
		g_object_unref(this->current_track_newpoint_gc);
		this->current_track_newpoint_gc = NULL;
	}

	if (!this->track_gc) {
		return;
	}

	for (int i = this->track_gc->len - 1; i >= 0; i--) {
		g_object_unref(g_array_index(this->track_gc, GObject *, i));
	}
	g_array_free(this->track_gc, true);
	this->track_gc = NULL;
}




void LayerTRW::new_track_gcs(Viewport * viewport)
{
	if (this->track_gc) {
		this->free_track_gcs();
	}

	int width = this->line_thickness;

	if (this->track_bg_gc) {
		g_object_unref(this->track_bg_gc);
	}

	this->track_bg_gc = viewport->new_gc_from_color(&this->track_bg_color, width + this->bg_line_thickness);

	// Ensure new track drawing heeds line thickness setting
	//  however always have a minium of 2, as 1 pixel is really narrow
	int new_track_width = (this->line_thickness < 2) ? 2 : this->line_thickness;

	if (this->current_track_gc) {
		g_object_unref(this->current_track_gc);
	}
	this->current_track_gc = viewport->new_gc("#FF0000", new_track_width);
	gdk_gc_set_line_attributes(this->current_track_gc, new_track_width, GDK_LINE_ON_OFF_DASH, GDK_CAP_ROUND, GDK_JOIN_ROUND);

	// 'newpoint' gc is exactly the same as the current track gc
	if (this->current_track_newpoint_gc) {
		g_object_unref(this->current_track_newpoint_gc);
	}
	this->current_track_newpoint_gc = viewport->new_gc("#FF0000", new_track_width);
	gdk_gc_set_line_attributes(this->current_track_newpoint_gc, new_track_width, GDK_LINE_ON_OFF_DASH, GDK_CAP_ROUND, GDK_JOIN_ROUND);

	this->track_gc = g_array_sized_new(false, false, sizeof (GdkGC *), VIK_TRW_LAYER_TRACK_GC);

	GdkGC * gc[VIK_TRW_LAYER_TRACK_GC];

	gc[VIK_TRW_LAYER_TRACK_GC_STOP] = viewport->new_gc("#874200", width);
	gc[VIK_TRW_LAYER_TRACK_GC_BLACK] = viewport->new_gc("#000000", width); // black

	gc[VIK_TRW_LAYER_TRACK_GC_SLOW] = viewport->new_gc("#E6202E", width); // red-ish
	gc[VIK_TRW_LAYER_TRACK_GC_AVER] = viewport->new_gc("#D2CD26", width); // yellow-ish
	gc[VIK_TRW_LAYER_TRACK_GC_FAST] = viewport->new_gc("#2B8700", width); // green-ish

	gc[VIK_TRW_LAYER_TRACK_GC_SINGLE] = viewport->new_gc_from_color(&(this->track_color), width);

	g_array_append_vals(this->track_gc, gc, VIK_TRW_LAYER_TRACK_GC);
}




#define SMALL_ICON_SIZE 18
/*
 * Can accept a null symbol, and may return null value
 */
GdkPixbuf* get_wp_sym_small(char *symbol)
{
	GdkPixbuf* wp_icon = a_get_wp_sym(symbol);
	// ATM a_get_wp_sym returns a cached icon, with the size dependent on the preferences.
	//  So needing a small icon for the treeview may need some resizing:
	if (wp_icon && gdk_pixbuf_get_width(wp_icon) != SMALL_ICON_SIZE) {
		wp_icon = gdk_pixbuf_scale_simple(wp_icon, SMALL_ICON_SIZE, SMALL_ICON_SIZE, GDK_INTERP_BILINEAR);
	}
	return wp_icon;
}




void LayerTRW::realize_track(std::unordered_map<sg_uid_t, Track *> & tracks, trw_data4_t * pass_along, SublayerType sublayer_type)
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

		Layer * parent = pass_along->layer;
		TreeView * tree_view = pass_along->tree_view;
		tree_view->add_sublayer(pass_along->path_iter, pass_along->iter2, trk->name, parent, i->first, sublayer_type, pixbuf, true, timestamp);

		if (pixbuf) {
			g_object_unref(pixbuf);
		}

		*new_iter = *(pass_along->iter2);
		if (trk->is_route) {
			this->routes_iters.insert({{ i->first, new_iter }});
		} else {
			this->tracks_iters.insert({{ i->first, new_iter }});
		}

		if (!trk->visible) {
			pass_along->tree_view->set_visibility(pass_along->iter2, false);
		}
	}
}




void LayerTRW::realize_waypoints(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, trw_data4_t * pass_along, SublayerType sublayer_type)
{
	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {
		GtkTreeIter *new_iter = (GtkTreeIter *) malloc(sizeof (GtkTreeIter));

		time_t timestamp = 0;
		if (i->second->has_timestamp) {
			timestamp = i->second->timestamp;
		}

		Layer * parent = pass_along->layer;

		pass_along->tree_view->add_sublayer(pass_along->path_iter, pass_along->iter2, i->second->name, parent, i->first, sublayer_type, get_wp_sym_small(i->second->symbol), true, timestamp);

		*new_iter = *(pass_along->iter2);
		this->waypoints_iters.insert({{ i->first, new_iter }});

		if (!i->second->visible) {
			pass_along->tree_view->set_visibility(pass_along->iter2, false);
		}
	}
}




void LayerTRW::add_sublayer_tracks(TreeView * tree_view_, GtkTreeIter * layer_iter)
{
	tree_view_->add_sublayer(layer_iter, &(track_iter), _("Tracks"), this, SG_UID_NONE, SublayerType::TRACKS, NULL, false, 0);
}




void LayerTRW::add_sublayer_waypoints(TreeView * tree_view_, GtkTreeIter * layer_iter)
{
	tree_view_->add_sublayer(layer_iter, &(waypoint_iter), _("Waypoints"), this, SG_UID_NONE, SublayerType::WAYPOINTS, NULL, false, 0);
}




void LayerTRW::add_sublayer_routes(TreeView * tree_view_, GtkTreeIter * layer_iter)
{
	tree_view_->add_sublayer(layer_iter, &(route_iter), _("Routes"), this, SG_UID_NONE, SublayerType::ROUTES, NULL, false, 0);
}




void LayerTRW::realize(TreeView * tree_view_, GtkTreeIter * layer_iter)
{
	GtkTreeIter iter2;
	trw_data4_t pass_along;
	pass_along.path_iter = &this->track_iter;
	pass_along.iter2 = &iter2;
	pass_along.layer = this;
	pass_along.tree_view = tree_view_;

	this->tree_view = tree_view_;
	this->iter = *layer_iter;
	this->realized = true;

	if (this->tracks.size() > 0) {
		this->add_sublayer_tracks(this->tree_view, layer_iter);
		this->realize_track(this->tracks, &pass_along, SublayerType::TRACK);
		this->tree_view->set_visibility(&(this->track_iter), this->tracks_visible);
	}

	if (this->routes.size() > 0) {
		pass_along.path_iter = &(this->route_iter);

		this->add_sublayer_routes(this->tree_view, layer_iter);
		this->realize_track(this->routes, &pass_along, SublayerType::ROUTE);
		this->tree_view->set_visibility(&(this->route_iter), this->routes_visible);
	}

	if (this->waypoints.size() > 0) {
		pass_along.path_iter = &(this->waypoint_iter);

		this->add_sublayer_waypoints(this->tree_view, layer_iter);
		this->realize_waypoints(this->waypoints, &pass_along, SublayerType::WAYPOINT);
		this->tree_view->set_visibility(&(this->waypoint_iter), this->waypoints_visible);
	}

	this->verify_thumbnails(NULL);

	this->sort_all();
}




bool LayerTRW::sublayer_toggle_visible(SublayerType sublayer_type, sg_uid_t sublayer_uid)
{
	switch (sublayer_type) {
	case SublayerType::TRACKS:
		return (this->tracks_visible ^= 1);
	case SublayerType::WAYPOINTS:
		return (this->waypoints_visible ^= 1);
	case SublayerType::ROUTES:
		return (this->routes_visible ^= 1);
	case SublayerType::TRACK:
		{
			Track * trk = this->tracks.at(sublayer_uid);
			if (trk) {
				return (trk->visible ^= 1);
			} else {
				return true;
			}
		}
	case SublayerType::WAYPOINT:
		{

			Waypoint * wp = this->waypoints.at(sublayer_uid);
			if (wp) {
				return (wp->visible ^= 1);
			} else {
				return true;
			}
		}
	case SublayerType::ROUTE:
		{
			Track * trk = this->routes.at(sublayer_uid);
			if (trk) {
				return (trk->visible ^= 1);
			} else {
				return true;
			}
		}
	default:
		break;
	}
	return true;
}




/*
 * Return a property about tracks for this layer
 */
int LayerTRW::get_property_tracks_line_thickness()
{
	return this->line_thickness;
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
		if (!trk->empty() && trk->get_tp_first()->has_timestamp) {
			// Get trkpt only once - as using get_tp_last() iterates whole track each time
			Trackpoint * trkpt_last = trk->get_tp_last();
			if (trkpt_last->has_timestamp) {

				time_t t1 = trk->get_tp_first()->timestamp;
				time_t t2 = trkpt_last->timestamp;

				// Assume never actually have a track with a time of 0 (1st Jan 1970)
				// Hence initialize to the first 'proper' value
				if (tt->start_time == 0) {
					tt->start_time = t1;
				}
				if (tt->end_time == 0) {
					tt->end_time = t2;
				}

				// Update find the earliest / last times
				if (t1 < tt->start_time) {
					tt->start_time = t1;
				}

				if (t2 > tt->end_time) {
					tt->end_time = t2;
				}

				// Keep track of total time
				//  there maybe gaps within a track (eg segments)
				//  but this should be generally good enough for a simple indicator
				tt->duration = tt->duration + (int) (t2 - t1);
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
char const * LayerTRW::tooltip()
{
	char tbuf1[64] = { 0 };
	char tbuf2[64] = { 0 };
	char tbuf3[64] = { 0 };
	char tbuf4[10] = { 0 };

	static char tmp_buf[128] = { 0 };

	// For compact date format I'm using '%x'     [The preferred date representation for the current locale without the time.]

	if (!this->tracks.empty()) {
		tooltip_tracks tt = { 0.0, 0, 0, 0 };
		trw_layer_tracks_tooltip(this->tracks, &tt);

		GDate* gdate_start = g_date_new();
		g_date_set_time_t(gdate_start, tt.start_time);

		GDate* gdate_end = g_date_new();
		g_date_set_time_t(gdate_end, tt.end_time);

		if (g_date_compare(gdate_start, gdate_end)) {
			// Dates differ so print range on separate line
			g_date_strftime(tbuf1, sizeof(tbuf1), "%x", gdate_start);
			g_date_strftime(tbuf2, sizeof(tbuf2), "%x", gdate_end);
			snprintf(tbuf3, sizeof(tbuf3), "%s to %s\n", tbuf1, tbuf2);
		} else {
			// Same date so just show it and keep rest of text on the same line - provided it's a valid time!
			if (tt.start_time != 0) {
				g_date_strftime(tbuf3, sizeof(tbuf3), "%x: ", gdate_start);
			}
		}

		tbuf2[0] = '\0';
		if (tt.length > 0.0) {

			/* Setup info dependent on distance units. */
			DistanceUnit distance_unit = a_vik_get_units_distance();
			get_distance_unit_string(tbuf4, sizeof (tbuf4), distance_unit);
			double len_in_units = convert_distance_meters_to(distance_unit, tt.length);

			// Timing information if available
			tbuf1[0] = '\0';
			if (tt.duration > 0) {
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
		trw_layer_routes_tooltip(this->routes, &rlength);
		if (rlength > 0.0) {



			/* Setup info dependent on distance units. */
			DistanceUnit distance_unit = a_vik_get_units_distance();
			get_distance_unit_string(tbuf4, sizeof (tbuf4), distance_unit);
			double len_in_units = convert_distance_meters_to(distance_unit, rlength);
			snprintf(tbuf1, sizeof(tbuf1), _("\nTotal route length %.1f %s"), len_in_units, tbuf4);
		}

		// Put together all the elements to form compact tooltip text
		snprintf(tmp_buf, sizeof(tmp_buf),
			 _("Tracks: %d - Waypoints: %d - Routes: %d%s%s"),
			 this->tracks.size(), this->waypoints.size(), this->routes.size(), tbuf2, tbuf1);

		g_date_free(gdate_start);
		g_date_free(gdate_end);
	}

	return tmp_buf;
}




char const * LayerTRW::sublayer_tooltip(SublayerType sublayer_type, sg_uid_t sublayer_uid)
{
	switch (sublayer_type) {
	case SublayerType::TRACKS:
		{
			// Very simple tooltip - may expand detail in the future...
			static char tmp_buf[32];
			snprintf(tmp_buf, sizeof(tmp_buf), _("Tracks: %d"), this->tracks.size());
			return tmp_buf;
		}
		break;
	case SublayerType::ROUTES:
		{
			// Very simple tooltip - may expand detail in the future...
			static char tmp_buf[32];
			snprintf(tmp_buf, sizeof(tmp_buf), _("Routes: %d"), this->routes.size());
			return tmp_buf;
		}
		break;

	/* Same tooltip for route and track. */
	case SublayerType::ROUTE:
	case SublayerType::TRACK:
		{
			Track * trk = NULL;
			if (sublayer_type == SublayerType::TRACK) {
				trk = this->tracks.at(sublayer_uid);
			} else {
				trk = this->routes.at(sublayer_uid);
			}

			if (trk) {
				// Could be a better way of handling strings - but this works...
				char time_buf1[20] = { 0 };
				char time_buf2[20] = { 0 };

				static char tmp_buf[100];
				// Compact info: Short date eg (11/20/99), duration and length
				// Hopefully these are the things that are most useful and so promoted into the tooltip
				if (!trk->empty() && trk->get_tp_first()->has_timestamp) {
					// %x     The preferred date representation for the current locale without the time.
					strftime(time_buf1, sizeof(time_buf1), "%x: ", gmtime(&(trk->get_tp_first()->timestamp)));
					time_t dur = trk->get_duration(true);
					if (dur > 0) {
						snprintf(time_buf2, sizeof(time_buf2), _("- %d:%02d hrs:mins"), (int)(dur/3600), (int)round(dur/60.0)%60);
					}
				}
				// Get length and consider the appropriate distance units
				double tr_len = trk->get_length();
				DistanceUnit distance_unit = a_vik_get_units_distance();
				switch (distance_unit) {
				case DistanceUnit::KILOMETRES:
					snprintf(tmp_buf, sizeof(tmp_buf), _("%s%.1f km %s"), time_buf1, tr_len/1000.0, time_buf2);
					break;
				case DistanceUnit::MILES:
					snprintf(tmp_buf, sizeof(tmp_buf), _("%s%.1f miles %s"), time_buf1, VIK_METERS_TO_MILES(tr_len), time_buf2);
					break;
				case DistanceUnit::NAUTICAL_MILES:
					snprintf(tmp_buf, sizeof(tmp_buf), _("%s%.1f NM %s"), time_buf1, VIK_METERS_TO_NAUTICAL_MILES(tr_len), time_buf2);
					break;
				default:
					break;
				}
				return tmp_buf;
			}
		}
		break;
	case SublayerType::WAYPOINTS:
		{
			// Very simple tooltip - may expand detail in the future...
			static char tmp_buf[32];
			snprintf(tmp_buf, sizeof(tmp_buf), _("Waypoints: %d"), this->waypoints.size());
			return tmp_buf;
		}
		break;
	case SublayerType::WAYPOINT:
		{
			Waypoint * wp = this->waypoints.at(sublayer_uid);
			// NB It's OK to return NULL
			if (wp) {
				if (wp->comment) {
					return wp->comment;
				} else {
					return wp->description;
				}
			}
		}
		break;
	default:
		break;
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
		tp_prev = selected_track->get_tp_prev(tp);
	}

	char * msg = vu_trackpoint_formatted_message(statusbar_format_code, tp, tp_prev, selected_track, NAN);
	vik_statusbar_set_message(window_from_layer(this)->get_statusbar(), VIK_STATUSBAR_INFO, msg);
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
	case HeightUnit::FEET:
		snprintf(tmp_buf1, sizeof(tmp_buf1), _("Wpt: Alt %dft"), (int) round(VIK_METERS_TO_FEET(wp->altitude)));
		break;
	default:
		//HeightUnit::METRES:
		snprintf(tmp_buf1, sizeof(tmp_buf1), _("Wpt: Alt %dm"), (int) round(wp->altitude));
	}

	// Position part
	// Position is put last, as this bit is most likely not to be seen if the display is not big enough,
	//   one can easily use the current pointer position to see this if needed
	char *lat = NULL, *lon = NULL;
	static struct LatLon ll;
	vik_coord_to_latlon(&wp->coord, &ll);
	a_coords_latlon_to_string(&ll, &lat, &lon);

	// Combine parts to make overall message
	char * msg;
	if (wp->comment) {
		// Add comment if available
		msg = g_strdup_printf(_("%s | %s %s | Comment: %s"), tmp_buf1, lat, lon, wp->comment);
	} else {
		msg = g_strdup_printf(_("%s | %s %s"), tmp_buf1, lat, lon);
	}
	vik_statusbar_set_message(window_from_layer(this)->get_statusbar(), VIK_STATUSBAR_INFO, msg);
	free(lat);
	free(lon);
	free(msg);
}




/**
 * General layer selection function, find out which bit is selected and take appropriate action
 */
bool LayerTRW::selected(SublayerType sublayer_type, sg_uid_t sublayer_uid, TreeItemType type, void * panel)
{
	// Reset
	this->current_wp = NULL;
	this->current_wp_uid = 0;
	this->cancel_current_tp(false);

	// Clear statusbar
	vik_statusbar_set_message(window_from_layer(this)->get_statusbar(), VIK_STATUSBAR_INFO, "");

	switch (type)	{
	case TreeItemType::LAYER:
		{
			window_from_layer(this)->set_selected_trw_layer(this);
			/* Mark for redraw */
			return true;
		}
		break;

	case TreeItemType::SUBLAYER:
		{
			switch (sublayer_type) {
			case SublayerType::TRACKS:
				{
					window_from_layer(this)->set_selected_tracks(&this->tracks, this);
					/* Mark for redraw */
					return true;
				}
				break;
			case SublayerType::TRACK:
				{
					Track * trk = this->tracks.at(sublayer_uid);
					window_from_layer(this)->set_selected_track(trk, this);
					/* Mark for redraw */
					return true;
				}
				break;
			case SublayerType::ROUTES:
				{
					window_from_layer(this)->set_selected_tracks(&this->routes, this);
					/* Mark for redraw */
					return true;
				}
				break;
			case SublayerType::ROUTE:
				{
					Track * trk = this->routes.at(sublayer_uid);
					window_from_layer(this)->set_selected_track(trk, this);
					/* Mark for redraw */
					return true;
				}
				break;
			case SublayerType::WAYPOINTS:
				{
					window_from_layer(this)->set_selected_waypoints(&this->waypoints, this);
					/* Mark for redraw */
					return true;
				}
				break;
			case SublayerType::WAYPOINT:
				{
					Waypoint * wp = this->waypoints.at(sublayer_uid);
					if (wp) {
						window_from_layer(this)->set_selected_waypoint(wp, this);
						// Show some waypoint info
						this->set_statusbar_msg_info_wpt(wp);
						/* Mark for redraw */
						return true;
					}
				}
				break;
			default:
				{
					return window_from_layer(this)->clear_highlight();
				}
				break;
			}
			return false;
		}
		break;

	default:
		return window_from_layer(this)->clear_highlight();
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
	return LayerTRWc::find_waypoint_by_name(waypoints, name);
}




/*
 * Get track by name - not guaranteed to be unique
 * Finds the first one
 */
Track * LayerTRW::get_track(const char * name)
{
	return LayerTRWc::find_track_by_name(tracks, name);
}




/*
 * Get route by name - not guaranteed to be unique
 * Finds the first one
 */
Track * LayerTRW::get_route(const char * name)
{
	return LayerTRWc::find_track_by_name(routes, name);
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
	// Continually reuse maxmin to find the latest maximum and minimum values
	// First set to waypoints bounds
	maxmin[0].lat = this->waypoints_bbox.north;
	maxmin[1].lat = this->waypoints_bbox.south;
	maxmin[0].lon = this->waypoints_bbox.east;
	maxmin[1].lon = this->waypoints_bbox.west;

	LayerTRWc::find_maxmin_in_tracks(tracks, maxmin);
	LayerTRWc::find_maxmin_in_tracks(routes, maxmin);
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




void trw_layer_centerize(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	VikCoord coord;
	if (layer->find_center(&coord)) {
		goto_coord(data->panel, NULL, NULL, &coord);
	} else {
		a_dialog_info_msg(gtk_window_from_layer(layer), _("This layer has no waypoints or trackpoints."));
	}
}




void LayerTRW::zoom_to_show_latlons(Viewport * viewport, struct LatLon maxmin[2])
{
	vu_zoom_to_show_latlons(coord_mode, viewport, maxmin);
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




void trw_layer_auto_view(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	LayersPanel * panel = data->panel;
	if (layer->auto_set_view(panel->get_viewport())) {
		panel->emit_update();
	} else {
		a_dialog_info_msg(gtk_window_from_layer(layer), _("This layer has no waypoints or trackpoints."));
	}
}




void trw_layer_export_gpspoint(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	char *auto_save_name = append_file_ext(layer->get_name(), FILE_TYPE_GPSPOINT);

	vik_trw_layer_export(layer, _("Export Layer"), auto_save_name, NULL, FILE_TYPE_GPSPOINT);

	free(auto_save_name);
}




void trw_layer_export_gpsmapper(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	char *auto_save_name = append_file_ext(layer->get_name(), FILE_TYPE_GPSMAPPER);

	vik_trw_layer_export(layer, _("Export Layer"), auto_save_name, NULL, FILE_TYPE_GPSMAPPER);

	free(auto_save_name);
}




void trw_layer_export_gpx(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	char *auto_save_name = append_file_ext(layer->get_name(), FILE_TYPE_GPX);

	vik_trw_layer_export(layer, _("Export Layer"), auto_save_name, NULL, FILE_TYPE_GPX);

	free(auto_save_name);
}




void trw_layer_export_kml(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	char *auto_save_name = append_file_ext(layer->get_name(), FILE_TYPE_KML);

	vik_trw_layer_export(layer, _("Export Layer"), auto_save_name, NULL, FILE_TYPE_KML);

	free(auto_save_name);
}




void trw_layer_export_geojson(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	char *auto_save_name = append_file_ext(layer->get_name(), FILE_TYPE_GEOJSON);

	vik_trw_layer_export(layer, _("Export Layer"), auto_save_name, NULL, FILE_TYPE_GEOJSON);

	free(auto_save_name);
}




void trw_layer_export_babel(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	const char *auto_save_name = layer->get_name();
	vik_trw_layer_export_gpsbabel(layer, _("Export Layer"), auto_save_name);
}




void trw_layer_export_external_gpx_1(trw_menu_layer_t * data)
{
	vik_trw_layer_export_external_gpx(data->layer, a_vik_get_external_gpx_program_1());
}




void trw_layer_export_external_gpx_2(trw_menu_layer_t * data)
{
	vik_trw_layer_export_external_gpx(data->layer, a_vik_get_external_gpx_program_2());
}




void trw_layer_export_gpx_track(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	Track * trk = layer->get_track_helper(data);

	if (!trk || !trk->name) {
		return;
	}

	char * auto_save_name = append_file_ext(trk->name, FILE_TYPE_GPX);

	char * label = NULL;
	if (data->sublayer_type == SublayerType::ROUTE) {
		label = _("Export Route as GPX");
	} else {
		label = _("Export Track as GPX");
	}
	vik_trw_layer_export(data->layer, label, auto_save_name, trk, FILE_TYPE_GPX);

	free(auto_save_name);
}




void trw_layer_goto_wp(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	LayersPanel * panel = data->panel;
	GtkWidget *dia = gtk_dialog_new_with_buttons(_("Find"),
						     gtk_window_from_layer(layer),
						     (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
						     GTK_STOCK_CANCEL,
						     GTK_RESPONSE_REJECT,
						     GTK_STOCK_OK,
						     GTK_RESPONSE_ACCEPT,
						     NULL);

	GtkWidget * label = gtk_label_new(_("Waypoint Name:"));
	GtkWidget * entry = gtk_entry_new();

	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), label, false, false, 0);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), entry, false, false, 0);
	gtk_widget_show_all(dia);
	// 'ok' when press return in the entry
	g_signal_connect_swapped(entry, "activate", G_CALLBACK(a_dialog_response_accept), dia);
	gtk_dialog_set_default_response(GTK_DIALOG(dia), GTK_RESPONSE_ACCEPT);

	while (gtk_dialog_run(GTK_DIALOG(dia)) == GTK_RESPONSE_ACCEPT) {
		char *name = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
		// Find *first* wp with the given name
		Waypoint * wp = layer->get_waypoint((const char *) name);

		if (!wp) {
			a_dialog_error_msg(gtk_window_from_layer(layer), _("Waypoint not found in this layer."));
		} else {
			panel->get_viewport()->set_center_coord(&wp->coord, true);
			panel->emit_update();

			// Find and select on the side panel
			sg_uid_t wp_uid = LayerTRWc::find_uid_of_waypoint(layer->waypoints, wp);
			if (wp_uid) {
				GtkTreeIter * it = layer->waypoints_iters.at(wp_uid);
				layer->tree_view->select_and_expose(it);
			}

			break;
		}

		free(name);

	}
	gtk_widget_destroy(dia);
}





bool LayerTRW::new_waypoint(GtkWindow * w, const VikCoord * def_coord)
{
	char * default_name = this->highest_wp_number_get();
	Waypoint * wp = new Waypoint();
	bool updated;
	wp->coord = *def_coord;

	// Attempt to auto set height if DEM data is available
	wp->apply_dem_data(true);

	char * returned_name = a_dialog_waypoint(w, default_name, this, wp, this->coord_mode, true, &updated);

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




void trw_layer_new_wikipedia_wp_viewport(trw_menu_layer_t * data)
{
	struct LatLon maxmin[2] = { {0.0,0.0}, {0.0,0.0} };
	LayerTRW * layer = data->layer;
	LayersPanel * panel = data->panel;
	Viewport * viewport =  window_from_layer(layer)->get_viewport();

	// Note the order is max part first then min part - thus reverse order of use in min_max function:
	viewport->get_min_max_lat_lon(&maxmin[1].lat, &maxmin[0].lat, &maxmin[1].lon, &maxmin[0].lon);
	a_geonames_wikipedia_box(window_from_layer(layer), layer, maxmin);
	layer->calculate_bounds_waypoints();
	panel->emit_update();
}




void trw_layer_new_wikipedia_wp_layer(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	LayersPanel * panel = data->panel;
	struct LatLon maxmin[2] = { {0.0,0.0}, {0.0,0.0} };

	layer->find_maxmin(maxmin);
	a_geonames_wikipedia_box(window_from_layer(layer), layer, maxmin);
	layer->calculate_bounds_waypoints();
	panel->emit_update();
}




#ifdef VIK_CONFIG_GEOTAG
void trw_layer_geotagging_waypoint_mtime_keep(trw_menu_sublayer_t * data)
{
	sg_uid_t wp_uid = data->sublayer_uid;
	Waypoint * wp = data->layer->waypoints.at(wp_uid);
	if (wp) {
		// Update directly - not changing the mtime
		a_geotag_write_exif_gps(wp->image, wp->coord, wp->altitude, true);
	}
}




void trw_layer_geotagging_waypoint_mtime_update(trw_menu_sublayer_t * data)
{
	sg_uid_t wp_uid = data->sublayer_uid;
	Waypoint * wp = data->layer->waypoints.at(wp_uid);
	if (wp) {
		// Update directly
		a_geotag_write_exif_gps(wp->image, wp->coord, wp->altitude, false);
	}
}




/*
 * Use code in separate file for this feature as reasonably complex
 */
void trw_layer_geotagging_track(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	sg_uid_t uid = data->sublayer_uid;
	Track * trk = layer->tracks.at(uid);
	// Unset so can be reverified later if necessary
	layer->has_verified_thumbnails = false;

	trw_layer_geotag_dialog(gtk_window_from_layer(layer),
				layer,
				NULL,
				trk);
}




void trw_layer_geotagging_waypoint(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	sg_uid_t wp_uid = data->sublayer_uid;
	Waypoint * wp = layer->waypoints.at(wp_uid);

	trw_layer_geotag_dialog(gtk_window_from_layer(layer),
				layer,
				wp,
				NULL);
}




void trw_layer_geotagging(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	// Unset so can be reverified later if necessary
	layer->has_verified_thumbnails = false;

	trw_layer_geotag_dialog(gtk_window_from_layer(layer),
				layer,
				NULL,
				NULL);
}
#endif




// 'Acquires' - Same as in File Menu -> Acquire - applies into the selected TRW Layer //

static void trw_layer_acquire(trw_menu_layer_t * data, VikDataSourceInterface *datasource)
{
	LayerTRW * layer = data->layer;
	LayersPanel * panel = (LayersPanel *) data->panel;
	Window * window = window_from_layer(layer);
	Viewport * viewport =  window->get_viewport();

	vik_datasource_mode_t mode = datasource->mode;
	if (mode == VIK_DATASOURCE_AUTO_LAYER_MANAGEMENT) {
		mode = VIK_DATASOURCE_ADDTOLAYER;
	}
	a_acquire(window, panel, viewport, mode, datasource, NULL, NULL);
}




/*
 * Acquire into this TRW Layer straight from GPS Device
 */
void trw_layer_acquire_gps_cb(trw_menu_layer_t * data)
{
	trw_layer_acquire(data, &vik_datasource_gps_interface);
}




/*
 * Acquire into this TRW Layer from Directions
 */
void trw_layer_acquire_routing_cb(trw_menu_layer_t * data)
{
	trw_layer_acquire(data, &vik_datasource_routing_interface);
}




/*
 * Acquire into this TRW Layer from an entered URL
 */
void trw_layer_acquire_url_cb(trw_menu_layer_t * data)
{
	trw_layer_acquire(data, &vik_datasource_url_interface);
}




#ifdef VIK_CONFIG_OPENSTREETMAP
/*
 * Acquire into this TRW Layer from OSM
 */
void trw_layer_acquire_osm_cb(trw_menu_layer_t * data)
{
	trw_layer_acquire(data, &vik_datasource_osm_interface);
}




/**
 * Acquire into this TRW Layer from OSM for 'My' Traces
 */
void trw_layer_acquire_osm_my_traces_cb(trw_menu_layer_t * data)
{
	trw_layer_acquire(data, &vik_datasource_osm_my_traces_interface);
}
#endif




#ifdef VIK_CONFIG_GEOCACHES
/*
 * Acquire into this TRW Layer from Geocaching.com
 */
void trw_layer_acquire_geocache_cb(trw_menu_layer_t * data)
{
	trw_layer_acquire(values, &vik_datasource_gc_interface);
}
#endif




#ifdef VIK_CONFIG_GEOTAG
/*
 * Acquire into this TRW Layer from images
 */
void trw_layer_acquire_geotagged_cb(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;

	trw_layer_acquire(data, &vik_datasource_geotag_interface);

	// Reverify thumbnails as they may have changed
	layer->has_verified_thumbnails = false;
	layer->verify_thumbnails(NULL);
}
#endif




/*
 * Acquire into this TRW Layer from any GPS Babel supported file
 */
void trw_layer_acquire_file_cb(trw_menu_layer_t * data)
{
	trw_layer_acquire(data, &vik_datasource_file_interface);
}

void trw_layer_gps_upload(trw_menu_layer_t * data)
{
	trw_menu_sublayer_t data2;
	memset(&data2, 0, sizeof (trw_menu_sublayer_t));

	data2.layer = data->layer;
	data2.panel = data->panel;

	trw_layer_gps_upload_any(&data2);
}




/**
 * If pass_along->tree is defined that this will upload just that track
 */
void trw_layer_gps_upload_any(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	LayersPanel * panel = data->panel;
	sg_uid_t uid = data->sublayer_uid;

	// May not actually get a track here as values[2&3] can be null
	Track * trk = NULL;
	GPSTransferType xfer_type = GPSTransferType::TRK; // SublayerType::TRACKS = 0 so hard to test different from NULL!
	bool xfer_all = false;

	if ((bool) data->sublayer_type) { /* kamilFIXME: don't cast. */
		xfer_all = false;
		if (data->sublayer_type == SublayerType::ROUTE) {
			trk = layer->routes.at(uid);
			xfer_type = GPSTransferType::RTE;
		} else if (data->sublayer_type == SublayerType::TRACK) {
			trk = layer->tracks.at(uid);
			xfer_type = GPSTransferType::TRK;
		} else if (data->sublayer_type == SublayerType::WAYPOINTS) {
			xfer_type = GPSTransferType::WPT;
		} else if (data->sublayer_type == SublayerType::ROUTES) {
			xfer_type = GPSTransferType::RTE;
		}
	} else if (!data->confirm) {
		xfer_all = true; // i.e. whole layer
	}

	if (trk && !trk->visible) {
		a_dialog_error_msg(gtk_window_from_layer(layer), _("Can not upload invisible track."));
		return;
	}

	GtkWidget *dialog = gtk_dialog_new_with_buttons(_("GPS Upload"),
							gtk_window_from_layer(layer),
							GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_STOCK_OK,
							GTK_RESPONSE_ACCEPT,
							GTK_STOCK_CANCEL,
							GTK_RESPONSE_REJECT,
							NULL);

	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	GtkWidget *response_w = NULL;
#if GTK_CHECK_VERSION (2, 20, 0)
	response_w = gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
#endif

	if (response_w) {
		gtk_widget_grab_focus(response_w);
	}

	void * dgs = datasource_gps_setup(dialog, xfer_type, xfer_all);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_ACCEPT) {
		datasource_gps_clean_up(dgs);
		gtk_widget_destroy(dialog);
		return;
	}

	// Get info from reused datasource dialog widgets
	char* protocol = datasource_gps_get_protocol(dgs);
	char* port = datasource_gps_get_descriptor(dgs);
	// NB don't free the above strings as they're references to values held elsewhere
	bool do_tracks = datasource_gps_get_do_tracks(dgs);
	bool do_routes = datasource_gps_get_do_routes(dgs);
	bool do_waypoints = datasource_gps_get_do_waypoints(dgs);
	bool turn_off = datasource_gps_get_off(dgs);

	gtk_widget_destroy(dialog);

	// When called from the viewport - work the corresponding layerspanel:
	if (!panel) {
		panel = window_from_layer(layer)->get_layers_panel();
	}

	// Apply settings to transfer to the GPS device
	vik_gps_comm(layer,
		     trk,
		     GPSDirection::UP,
		     protocol,
		     port,
		     false,
		     panel->get_viewport(),
		     panel,
		     do_tracks,
		     do_routes,
		     do_waypoints,
		     turn_off);
}




void trw_layer_new_wp(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	LayersPanel * panel = data->panel;
	/* TODO longone: okay, if layer above (aggregate) is invisible but vtl->visible is true, this redraws for no reason.
	   instead return true if you want to update. */
	if (layer->new_waypoint(gtk_window_from_layer(layer), panel->get_viewport()->get_center())) {
		layer->calculate_bounds_waypoints();
		if (layer->visible) {
			panel->emit_update();
		}
	}
}




void LayerTRW::new_track_create_common(char * name)
{
	this->current_track = new Track();
	this->current_track->set_defaults();
	this->current_track->visible = true;

	if (this->drawmode == DRAWMODE_ALL_SAME_COLOR) {
		// Create track with the preferred colour from the layer properties
		this->current_track->color = this->track_color;
	} else {
		gdk_color_parse("#000000", &(this->current_track->color));
	}

	this->current_track->has_color = true;
	this->add_track(this->current_track, name);
}




void trw_layer_new_track(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;

	if (!layer->current_track) {
		char *name = layer->new_unique_sublayer_name(SublayerType::TRACK, _("Track")) ;
		layer->new_track_create_common(name);
		free(name);

		window_from_layer(layer)->enable_layer_tool(LayerType::TRW, TOOL_CREATE_TRACK);
	}
}




void LayerTRW::new_route_create_common(char * name)
{
	this->current_track = new Track();
	this->current_track->set_defaults();
	this->current_track->visible = true;
	this->current_track->is_route = true;
	// By default make all routes red
	this->current_track->has_color = true;
	gdk_color_parse ("red", &this->current_track->color);
	this->add_route(this->current_track, name);
}




void trw_layer_new_route(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;

	if (!layer->current_track) {
		char *name = layer->new_unique_sublayer_name(SublayerType::ROUTE, _("Route")) ;
		layer->new_route_create_common(name);
		free(name);
		window_from_layer(layer)->enable_layer_tool(LayerType::TRW, TOOL_CREATE_ROUTE);
	}
}




void trw_layer_auto_routes_view(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	LayersPanel * panel = data->panel;

	if (layer->routes.size() > 0) {
		struct LatLon maxmin[2] = { {0,0}, {0,0} };
		LayerTRWc::find_maxmin_in_tracks(layer->routes, maxmin);
		layer->zoom_to_show_latlons(panel->get_viewport(), maxmin);
		panel->emit_update();
	}
}




void trw_layer_finish_track(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	layer->current_track = NULL;
	layer->route_finder_started = false;
	layer->emit_update();
}




void trw_layer_auto_tracks_view(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	LayersPanel * panel = data->panel;

	if (layer->tracks.size() > 0) {
		struct LatLon maxmin[2] = { {0,0}, {0,0} };
		LayerTRWc::find_maxmin_in_tracks(layer->tracks, maxmin);
		layer->zoom_to_show_latlons(panel->get_viewport(), maxmin);
		panel->emit_update();
	}
}




void trw_layer_auto_waypoints_view(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	LayersPanel * panel = data->panel;

	/* Only 1 waypoint - jump straight to it */
	if (layer->waypoints.size() == 1) {
		Viewport * viewport = panel->get_viewport();
		LayerTRWc::single_waypoint_jump(layer->waypoints, viewport);
	}
	/* If at least 2 waypoints - find center and then zoom to fit */
	else if (layer->waypoints.size() > 1) {
		struct LatLon maxmin[2] = { {0,0}, {0,0} };
		maxmin[0].lat = layer->waypoints_bbox.north;
		maxmin[1].lat = layer->waypoints_bbox.south;
		maxmin[0].lon = layer->waypoints_bbox.east;
		maxmin[1].lon = layer->waypoints_bbox.west;
		layer->zoom_to_show_latlons(panel->get_viewport(), maxmin);
	}

	panel->emit_update();
}




void trw_layer_osm_traces_upload_cb(trw_menu_layer_t * data)
{
	osm_traces_upload_viktrwlayer(data->layer, NULL);
}




void trw_layer_osm_traces_upload_track_cb(trw_menu_sublayer_t * data)
{
	if (data->misc) {
		Track * trk = ((Track *) data->misc);
		osm_traces_upload_viktrwlayer(data->layer, trk);
	}
}




GtkWidget* create_external_submenu(GtkMenu *menu)
{
	GtkWidget *external_submenu = gtk_menu_new();
	GtkWidget *item = gtk_image_menu_item_new_with_mnemonic(_("Externa_l"));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_EXECUTE, GTK_ICON_SIZE_MENU));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM (item), external_submenu);
	return external_submenu;
}




// Fake Waypoint UUIDs vith simple increasing integer
static sg_uid_t global_wp_uid = SG_UID_INITIAL;

void LayerTRW::add_waypoint(Waypoint * wp, char const * name)
{
	global_wp_uid++;

	wp->set_name(name);

	if (this->realized) {
		// Do we need to create the sublayer:
		if (waypoints.size() == 0) {
			this->add_sublayer_waypoints(this->tree_view, &this->iter);
		}

		GtkTreeIter *iter = (GtkTreeIter *) malloc(sizeof(GtkTreeIter));

		time_t timestamp = 0;
		if (wp->has_timestamp) {
			timestamp = wp->timestamp;
		}

		// Visibility column always needed for waypoints
		this->tree_view->add_sublayer(&waypoint_iter, iter, name, this, global_wp_uid, SublayerType::WAYPOINT, get_wp_sym_small(wp->symbol), true, timestamp);

		// Actual setting of visibility dependent on the waypoint
		this->tree_view->set_visibility(iter, wp->visible);

		waypoints_iters.insert({{ global_wp_uid, iter }});

		// Sort now as post_read is not called on a realized waypoint
		this->tree_view->sort_children(&(waypoint_iter), this->wp_sort_order);
	}

	this->highest_wp_number_add_wp(name);
	waypoints.insert({{ global_wp_uid, wp }});
}




// Fake Track UUIDs vi simple increasing integer
static sg_uid_t global_tr_uuid = SG_UID_INITIAL;

void LayerTRW::add_track(Track * trk, char const * name)
{
	global_tr_uuid++;

	trk->set_name(name);

	if (this->realized) {
		// Do we need to create the sublayer:
		if (tracks.size() == 0) {
			this->add_sublayer_tracks(this->tree_view, &this->iter);
		}

		GtkTreeIter *iter = (GtkTreeIter *) malloc(sizeof(GtkTreeIter));

		time_t timestamp = 0;
		Trackpoint * tp = trk->get_tp_first();
		if (tp && tp->has_timestamp) {
			timestamp = tp->timestamp;
		}

		// Visibility column always needed for tracks
		this->tree_view->add_sublayer(&track_iter, iter, name, this, global_tr_uuid, SublayerType::TRACK, NULL, true, timestamp);

		// Actual setting of visibility dependent on the track
		this->tree_view->set_visibility(iter, trk->visible);

		tracks_iters.insert({{ global_tr_uuid, iter }});

		// Sort now as post_read is not called on a realized track
		this->tree_view->sort_children(&(track_iter), this->track_sort_order);
	}

	tracks.insert({{ global_tr_uuid, trk }});

	this->update_treeview(trk);
}





// Fake Route UUIDs vi simple increasing integer
static sg_uid_t global_rt_uuid = SG_UID_INITIAL;

void LayerTRW::add_route(Track * trk, char const * name)
{
	global_rt_uuid++;

	trk->set_name(name);

	if (this->realized) {
		// Do we need to create the sublayer:
		if (routes.size() == 0) {
			this->add_sublayer_routes(this->tree_view, &this->iter);
		}

		GtkTreeIter *iter = (GtkTreeIter *) malloc(sizeof(GtkTreeIter));
		// Visibility column always needed for routes
		this->tree_view->add_sublayer(&route_iter, iter, name, this, global_rt_uuid, SublayerType::ROUTE, NULL, true, 0); // Routes don't have times
		// Actual setting of visibility dependent on the route
		this->tree_view->set_visibility(iter, trk->visible);

		routes_iters.insert({{ global_rt_uuid, iter }});

		// Sort now as post_read is not called on a realized route
		this->tree_view->sort_children(&(route_iter), this->track_sort_order);
	}

	routes.insert({{ global_rt_uuid, trk }});

	this->update_treeview(trk);
}




/* to be called whenever a track has been deleted or may have been changed. */
void LayerTRW::cancel_tps_of_track(Track * trk)
{
	if (this->selected_track == trk) {
		this->cancel_current_tp(false);
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
char * LayerTRW::new_unique_sublayer_name(SublayerType sublayer_type, const char * name)
{
	int i = 2; /* kamilTODO: static? */
	char * newname = g_strdup(name);

	void * id = NULL;
	do {
		id = NULL;
		switch (sublayer_type) {
		case SublayerType::TRACK:
			id = (void *) this->get_track((const char *) newname);
			break;
		case SublayerType::WAYPOINT:
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
	if (this->route_finder_append && this->current_track) {
		trk->remove_dup_points(); /* make "double point" track work to undo */

		// enforce end of current track equal to start of tr
		Trackpoint * cur_end = this->current_track->get_tp_last();
		Trackpoint * new_start = trk->get_tp_first();
		if (cur_end && new_start) {
			if (!vik_coord_equals(&cur_end->coord, &new_start->coord)) {
				this->current_track->add_trackpoint(new Trackpoint(*cur_end), false);
			}
		}

		this->current_track->steal_and_append_trackpoints(trk);
		trk->free();
		this->route_finder_append = false; /* this means we have added it */
	} else {

		// No more uniqueness of name forced when loading from a file
		if (trk->is_route) {
			this->add_route(trk, name);
		} else {
			this->add_track(trk, name);
		}

		if (this->route_finder_check_added_track) {
			trk->remove_dup_points(); /* make "double point" track work to undo */
			this->route_finder_added_track = trk;
		}
	}
}




#if 0
static void trw_layer_enum_item(void * id, GList **tr, GList **l)
{
	*l = g_list_append(*l, id);
}
#endif




/*
 * Move an item from one TRW layer to another TRW layer
 */
void LayerTRW::move_item(LayerTRW * trw_dest, void * id, SublayerType sublayer_type)
{
	LayerTRW * trw_src = this;
	// When an item is moved the name is checked to see if it clashes with an existing name
	//  in the destination layer and if so then it is allocated a new name

	// TODO reconsider strategy when moving within layer (if anything...)
	if (trw_src == trw_dest) {
		return;
	}

	sg_uid_t uid = (sg_uid_t) ((long) id);
	if (sublayer_type == SublayerType::TRACK) {
		Track * trk = this->tracks.at(uid);

		char * newname = trw_dest->new_unique_sublayer_name(sublayer_type, trk->name);

		Track * trk2 = new Track(*trk);
		trw_dest->add_track(trk2, newname);
		free(newname);
		this->delete_track(trk);
		// Reset layer timestamps in case they have now changed
		trw_dest->tree_view->set_timestamp(&trw_dest->iter, trw_dest->get_timestamp());
		trw_src->tree_view->set_timestamp(&trw_src->iter, trw_src->get_timestamp());
	}

	if (sublayer_type == SublayerType::ROUTE) {
		Track * trk = this->routes.at(uid);

		char * newname = trw_dest->new_unique_sublayer_name(sublayer_type, trk->name);

		Track * trk2 = new Track(*trk);
		trw_dest->add_route(trk2, newname);
		free(newname);
		this->delete_route(trk);
	}

	if (sublayer_type == SublayerType::WAYPOINT) {
		Waypoint * wp = this->waypoints.at(uid);

		char *newname = trw_dest->new_unique_sublayer_name(sublayer_type, wp->name);

		Waypoint * wp2 = new Waypoint(*wp);
		trw_dest->add_waypoint(wp2, newname);
		free(newname);
		this->delete_waypoint(wp);

		// Recalculate bounds even if not renamed as maybe dragged between layers
		trw_dest->calculate_bounds_waypoints();
		trw_src->calculate_bounds_waypoints();
		// Reset layer timestamps in case they have now changed
		trw_dest->tree_view->set_timestamp(&trw_dest->iter, trw_dest->get_timestamp());
		trw_src->tree_view->set_timestamp(&trw_src->iter, trw_src->get_timestamp());
	}
}




void LayerTRW::drag_drop_request(Layer * src, GtkTreeIter * src_item_iter, GtkTreePath * dest_path)
{
	LayerTRW * trw_dest = this;
	LayerTRW * trw_src = (LayerTRW *) src;

	SublayerType sublayer_type = trw_src->tree_view->get_sublayer_type(src_item_iter);

	if (!trw_src->tree_view->get_name(src_item_iter)) {
		GList *items = NULL;

		if (sublayer_type == SublayerType::TRACKS) {
			LayerTRWc::list_trk_uids(trw_src->tracks, &items);
		}
		if (sublayer_type == SublayerType::WAYPOINTS) {
			LayerTRWc::list_wp_uids(trw_src->waypoints, &items);
		}
		if (sublayer_type == SublayerType::ROUTES) {
			LayerTRWc::list_trk_uids(trw_src->routes, &items);
		}

		GList * iter = items;
		while (iter) {
			if (sublayer_type == SublayerType::TRACKS) {
				trw_src->move_item(trw_dest, iter->data, SublayerType::TRACK);
			} else if (sublayer_type == SublayerType::ROUTES) {
				trw_src->move_item(trw_dest, iter->data, SublayerType::ROUTE);
			} else {
				trw_src->move_item(trw_dest, iter->data, SublayerType::WAYPOINT);
			}
			iter = iter->next;
		}
		if (items) {
			g_list_free(items);
		}
	} else {
		char * name = trw_src->tree_view->get_name(src_item_iter);
		trw_src->move_item(trw_dest, name, sublayer_type);
	}
}




bool LayerTRW::delete_track(Track * trk)
{
	bool was_visible = false;

	/* kamilTODO: why check for trk->name here? */
	if (trk && trk->name) {

		if (trk == this->current_track) {
			this->current_track = NULL;
			selected_track = NULL;
			current_tp_uid = 0;
			moving_tp = false;
			this->route_finder_started = false;
		}

		was_visible = trk->visible;

		if (trk == this->route_finder_added_track) {
			this->route_finder_added_track = NULL;
		}

		sg_uid_t uid = LayerTRWc::find_uid_of_track(tracks, trk);
		if (uid) {
			/* could be current_tp, so we have to check */
			this->cancel_tps_of_track(trk);

			TreeIndex * it = tracks_iters.at(uid);
			if (it) {
				this->tree_view->erase(it);
				tracks_iters.erase(uid);
				tracks.erase(uid); /* kamilTODO: should this line be inside of "if (it)"? */

				// If last sublayer, then remove sublayer container
				if (tracks.size() == 0) {
					this->tree_view->erase(&track_iter);
				}
			}
			// In case it was selected (no item delete signal ATM)
			window_from_layer(this)->clear_highlight();
		}
	}
	return was_visible;
}




bool LayerTRW::delete_route(Track * trk)
{
	bool was_visible = false;

	/* kamilTODO: why check for trk->name here? */
	if (trk && trk->name) {

		if (trk == this->current_track) {
			this->current_track = NULL;
			selected_track = NULL;
			current_tp_uid = 0;
			moving_tp = false;
		}

		was_visible = trk->visible;

		if (trk == this->route_finder_added_track) {
			this->route_finder_added_track = NULL;
		}

		// Hmmm, want key of it
		sg_uid_t uid = LayerTRWc::find_uid_of_track(routes, trk);
		if (uid) {
			/* could be current_tp, so we have to check */
			this->cancel_tps_of_track(trk);

			GtkTreeIter * it = routes_iters.at(uid);

			if (it) {
				this->tree_view->erase(it);
				routes_iters.erase(uid);
				routes.erase(uid); /* kamilTODO: should this line be inside of "if (it)"? */

				// If last sublayer, then remove sublayer container
				if (routes.size() == 0) {
					this->tree_view->erase(&route_iter);
				}
			}
			// Incase it was selected (no item delete signal ATM)
			window_from_layer(this)->clear_highlight();
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

		sg_uid_t uid = LayerTRWc::find_uid_of_waypoint(waypoints, wp);
		if (uid) {
			TreeIndex * it = waypoints_iters.at(uid);

			if (it) {
				this->tree_view->erase(it);
				waypoints_iters.erase(uid);

				this->highest_wp_number_remove_wp(wp->name);

				/* kamilTODO: should this line be inside of "if (it)"? */
				waypoints.erase(uid); // last because this frees the name

				// If last sublayer, then remove sublayer container
				if (waypoints.size() == 0) {
					this->tree_view->erase(&waypoint_iter);
				}
			}
			// Incase it was selected (no item delete signal ATM)
			window_from_layer(this)->clear_highlight();
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
	sg_uid_t uid = LayerTRWc::find_uid_of_waypoint_by_name(waypoints, name);
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
		Track * trk = LayerTRWc::find_track_by_name(routes, name);
		if (trk) {
			return delete_route(trk);
		}
	} else {
		Track * trk = LayerTRWc::find_track_by_name(tracks, name);
		if (trk) {
			return delete_track(trk);
		}
	}

	return false;
}




void LayerTRW::delete_all_routes()
{
	this->current_track = NULL;
	this->route_finder_added_track = NULL;
	if (this->selected_track) {
		this->cancel_current_tp(false);
	}

	LayerTRWc::remove_item_from_treeview(this->routes_iters, this->tree_view);
	this->routes_iters.clear(); /* kamilTODO: call destructors of route iters. */
	this->routes.clear(); /* kamilTODO: call destructors of routes. */

	this->tree_view->erase(&this->route_iter);

	this->emit_update();
}




void LayerTRW::delete_all_tracks()
{
	this->current_track = NULL;
	this->route_finder_added_track = NULL;
	if (this->selected_track) {
		this->cancel_current_tp(false);
	}

	LayerTRWc::remove_item_from_treeview(this->tracks_iters, this->tree_view);
	this->tracks_iters.clear();
	this->tracks.clear(); /* kamilTODO: call destructors of tracks. */

	this->tree_view->erase(&this->track_iter);

	this->emit_update();
}




void LayerTRW::delete_all_waypoints()
{
	this->current_wp = NULL;
	this->current_wp_uid = 0;
	this->moving_wp = false;

	this->highest_wp_number_reset();

	LayerTRWc::remove_item_from_treeview(this->waypoints_iters, this->tree_view);
	this->waypoints_iters.clear();
	this->waypoints.clear(); /* kamilTODO: does this really call destructors of Waypoints? */

	this->tree_view->erase(&this->waypoint_iter);

	this->emit_update();
}




void trw_layer_delete_all_tracks(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	// Get confirmation from the user
	if (a_dialog_yes_or_no(gtk_window_from_layer(layer),
			       _("Are you sure you want to delete all tracks in %s?"),
			       layer->get_name())) {

		layer->delete_all_tracks();
	}
}




void trw_layer_delete_all_routes(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	// Get confirmation from the user
	if (a_dialog_yes_or_no(gtk_window_from_layer(layer),
			       _("Are you sure you want to delete all routes in %s?"),
			       layer->get_name())) {

		layer->delete_all_routes();
	}
}




void trw_layer_delete_all_waypoints(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	// Get confirmation from the user
	if (a_dialog_yes_or_no(gtk_window_from_layer(layer),
			       _("Are you sure you want to delete all waypoints in %s?"),
			       layer->get_name())) {

		layer->delete_all_waypoints();
	}
}




void trw_layer_delete_item(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	sg_uid_t uid = data->sublayer_uid;
	bool was_visible = false;

	if (data->sublayer_type == SublayerType::WAYPOINT) {
		Waypoint * wp = layer->waypoints.at(uid);
		if (wp && wp->name) {
			if (data->confirm) {
				// Get confirmation from the user
				// Maybe this Waypoint Delete should be optional as is it could get annoying...
				if (!a_dialog_yes_or_no(gtk_window_from_layer(layer),
							 _("Are you sure you want to delete the waypoint \"%s\"?"),
							 wp->name)) {
					return;
				}
			}

			was_visible = layer->delete_waypoint(wp);
			layer->calculate_bounds_waypoints();
			// Reset layer timestamp in case it has now changed
			layer->tree_view->set_timestamp(&layer->iter, layer->get_timestamp());
		}
	} else if (data->sublayer_type == SublayerType::TRACK) {
		Track * trk = layer->tracks.at(uid);
		if (trk && trk->name) {
			if (data->confirm) {
				// Get confirmation from the user
				if (!a_dialog_yes_or_no(gtk_window_from_layer(layer),
							 _("Are you sure you want to delete the track \"%s\"?"),
							 trk->name)) {
					return;
				}
			}

			was_visible = layer->delete_track(trk);
			// Reset layer timestamp in case it has now changed
			layer->tree_view->set_timestamp(&layer->iter, layer->get_timestamp());
		}
	} else {
		Track * trk = layer->routes.at(uid);
		if (trk && trk->name) {
			if (data->confirm) {
				// Get confirmation from the user
				if (!a_dialog_yes_or_no(gtk_window_from_layer(layer),
							 _("Are you sure you want to delete the route \"%s\"?"),
							 trk->name)) {
					return;
				}
			}
			was_visible = layer->delete_route(trk);
		}
	}
	if (was_visible) {
		layer->emit_update();
	}
}




/**
 *  Rename waypoint and maintain corresponding name of waypoint in the treeview
 */
void LayerTRW::waypoint_rename(Waypoint * wp, char const * new_name)
{
	wp->set_name(new_name);

	// Now update the treeview as well
	// Need key of it for treeview update
	sg_uid_t uid = LayerTRWc::find_uid_of_waypoint(this->waypoints, wp);
	if (uid) {
		GtkTreeIter * it = this->waypoints_iters.at(uid);
		if (it) {
			this->tree_view->set_name(it, new_name);
			this->tree_view->sort_children(&(this->waypoint_iter), this->wp_sort_order);
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
	sg_uid_t uid = LayerTRWc::find_uid_of_waypoint(this->waypoints, wp);
	if (uid) {
		GtkTreeIter * it = this->waypoints_iters.at(uid);
		if (it) {
			this->tree_view->set_icon(it, get_wp_sym_small(wp->symbol));
		}
	}
}




void trw_layer_properties_item(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;

	if (data->sublayer_type == SublayerType::WAYPOINT) {
		sg_uid_t wp_uid = data->sublayer_uid;
		Waypoint * wp = layer->waypoints.at(wp_uid);

		if (wp && wp->name) {
			bool updated = false;
			char *new_name = a_dialog_waypoint(gtk_window_from_layer(layer), wp->name, layer, wp, layer->coord_mode, false, &updated);
			if (new_name) {
				layer->waypoint_rename(wp, new_name);
			}

			if (updated && data->tv_iter) {
				layer->tree_view->set_icon(data->tv_iter, get_wp_sym_small(wp->symbol));
			}

			if (updated && layer->visible) {
				layer->emit_update();
			}
		}
	} else {
		Track * trk = layer->get_track_helper(data);

		if (trk && trk->name) {
			vik_trw_layer_propwin_run(gtk_window_from_layer(layer),
						  layer,
						  trk,
						  data->panel ? data->panel : NULL,
						  data->viewport,
						  false);
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
void trw_layer_track_statistics(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	Track * trk = layer->get_track_helper(data);

	if (trk && trk->name) {
		vik_trw_layer_propwin_run(gtk_window_from_layer(layer),
					  layer,
					  trk,
					  data->panel,
					  data->viewport,
					  true);
	}
}




/*
 * Update the treeview of the track id - primarily to update the icon
 */
void LayerTRW::update_treeview(Track * trk)
{
	sg_uid_t uid = 0;
	if (trk->is_route) {
		uid = LayerTRWc::find_uid_of_track(this->routes, trk);
	} else {
		uid = LayerTRWc::find_uid_of_track(this->tracks, trk);
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

			this->tree_view->set_icon(iter, pixbuf);
			g_object_unref(pixbuf);
		}
	}
}




static void goto_coord(LayersPanel * panel, Layer * layer, Viewport * viewport, const VikCoord * coord)
{
	if (panel) {
		panel->get_viewport()->set_center_coord(coord, true);
		panel->emit_update();
	} else {
		/* Since panel not set, layer & viewport should be valid instead! */
		if (layer && viewport) {
			viewport->set_center_coord(coord, true);
			layer->emit_update();
		}
	}
}




void trw_layer_goto_track_startpoint(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	Track * trk = layer->get_track_helper(data);

	if (trk && !trk->empty()) {
		goto_coord(data->panel, data->layer, data->viewport, &trk->get_tp_first()->coord);
	}
}




void trw_layer_goto_track_center(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	Track * trk = layer->get_track_helper(data);

	if (trk && !trk->empty()) {
		struct LatLon average, maxmin[2] = { {0,0}, {0,0} };
		VikCoord coord;
		LayerTRW::find_maxmin_in_track(trk, maxmin);
		average.lat = (maxmin[0].lat+maxmin[1].lat)/2;
		average.lon = (maxmin[0].lon+maxmin[1].lon)/2;
		vik_coord_load_from_latlon(&coord, layer->coord_mode, &average);
		goto_coord(data->panel, data->layer, data->viewport, &coord);
	}
}




void trw_layer_convert_track_route(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	Track * trk = layer->get_track_helper(data);

	if (!trk) {
		return;
	}

	// Converting a track to a route can be a bit more complicated,
	//  so give a chance to change our minds:
	if (!trk->is_route
	    && ((trk->get_segment_count() > 1)
		|| (trk->get_average_speed() > 0.0))) {

		if (!a_dialog_yes_or_no(gtk_window_from_layer(layer),
					_("Converting a track to a route removes extra track data such as segments, timestamps, etc...\nDo you want to continue?"), NULL)) {
			return;
		}
	}

	// Copy it
	Track * trk_copy = new Track(*trk);

	// Convert
	trk_copy->is_route = !trk_copy->is_route;

	// ATM can't set name to self - so must create temporary copy
	char *name = g_strdup(trk_copy->name);

	// Delete old one and then add new one
	if (trk->is_route) {
		layer->delete_route(trk);
		layer->add_track(trk_copy, name);
	} else {
		// Extra route conversion bits...
		trk_copy->merge_segments();
		trk_copy->to_routepoints();

		layer->delete_track(trk);
		layer->add_route(trk_copy, name);
	}
	free(name);

	// Update in case color of track / route changes when moving between sublayers
	layer->emit_update();
}




void trw_layer_anonymize_times(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	Track * trk = layer->get_track_helper(data);

	if (trk) {
		trk->anonymize_times();
	}
}




void trw_layer_interpolate_times(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	Track * trk = layer->get_track_helper(data);

	if (trk) {
		trk->interpolate_times();
	}
}




void trw_layer_extend_track_end(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	Track * trk = layer->get_track_helper(data);

	if (!trk) {
		return;
	}

	layer->current_track = trk;
	window_from_layer(layer)->enable_layer_tool(LayerType::TRW, trk->is_route ? TOOL_CREATE_ROUTE : TOOL_CREATE_TRACK);

	if (!trk->empty()) {
		goto_coord(data->panel, data->layer, data->viewport, &trk->get_tp_last()->coord);
	}
}




/**
 * extend a track using route finder
 */
void trw_layer_extend_track_end_route_finder(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	sg_uid_t uid = data->sublayer_uid;
	Track * trk = layer->routes.at(uid);
	if (!trk) {
		return;
	}

	window_from_layer(layer)->enable_layer_tool(LayerType::TRW, TOOL_ROUTE_FINDER);
	layer->current_track = trk;
	layer->route_finder_started = true;

	if (!trk->empty()) {
		goto_coord(data->panel, data->layer, data->viewport, &trk->get_tp_last()->coord);
	}
}




/**
 *
 */
bool LayerTRW::dem_test(LayersPanel * panel)
{
	// If have a panel then perform a basic test to see if any DEM info available...
	if (panel) {
		std::list<Layer *> * dems = panel->get_all_layers_of_type(LayerType::DEM, true); // Includes hidden DEM layer types
		if (dems->empty()) {
			a_dialog_error_msg(gtk_window_from_layer(this), _("No DEM layers available, thus no DEM values can be applied."));
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
void LayerTRW::apply_dem_data_common(LayersPanel * panel, Track * trk, bool skip_existing_elevations)
{
	if (!this->dem_test(panel)) {
		return;
	}

	unsigned long changed = trk->apply_dem_data(skip_existing_elevations);
	// Inform user how much was changed
	char str[64];
	const char * tmp_str = ngettext("%ld point adjusted", "%ld points adjusted", changed);
	snprintf(str, 64, tmp_str, changed);
	a_dialog_info_msg(gtk_window_from_layer(this), str);
}




void trw_layer_apply_dem_data_all(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	Track * trk = layer->get_track_helper(data);

	if (trk) {
		layer->apply_dem_data_common(data->panel, trk, false);
	}
}




void trw_layer_apply_dem_data_only_missing(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	Track * trk = layer->get_track_helper(data);

	if (trk) {
		layer->apply_dem_data_common(data->panel, trk, true);
	}
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
	a_dialog_info_msg(gtk_window_from_layer(this), str);
}




/**
 *
 */
void trw_layer_missing_elevation_data_interp(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	Track * trk = layer->get_track_helper(data);

	if (!trk) {
		return;
	}

	layer->smooth_it(trk, false);
}




void trw_layer_missing_elevation_data_flat(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	Track * trk = layer->get_track_helper(data);

	if (!trk) {
		return;
	}

	layer->smooth_it(trk, true);
}




/**
 * Commonal helper function
 */
void LayerTRW::wp_changed_message(int changed)
{
	char str[64];
	const char * tmp_str = ngettext("%ld waypoint changed", "%ld waypoints changed", changed);
	snprintf(str, 64, tmp_str, changed);
	a_dialog_info_msg(gtk_window_from_layer(this), str);
}




void trw_layer_apply_dem_data_wpt_all(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	LayersPanel * panel = data->panel;

	if (!layer->dem_test(panel)) {
		return;
	}

	int changed = 0;
	if (data->sublayer_type == SublayerType::WAYPOINT) {
		// Single Waypoint
		sg_uid_t wp_uid = data->sublayer_uid;
		Waypoint * wp = layer->waypoints.at(wp_uid);
		if (wp) {
			changed = (int) wp->apply_dem_data(false);
		}
	} else {
		// All waypoints
		for (auto i = layer->waypoints.begin(); i != layer->waypoints.end(); i++) {
			changed = changed + (int) i->second->apply_dem_data(false);
		}
	}
	layer->wp_changed_message(changed);
}




void trw_layer_apply_dem_data_wpt_only_missing(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	LayersPanel * panel = data->panel;

	if (!layer->dem_test(panel)) {
		return;
	}

	int changed = 0;
	if (data->sublayer_type == SublayerType::WAYPOINT) {
		// Single Waypoint
		sg_uid_t wp_uid = data->sublayer_uid;
		Waypoint * wp = layer->waypoints.at(wp_uid);
		if (wp) {
			changed = (int) wp->apply_dem_data(true);
		}
	} else {
		// All waypoints
		for (auto i = layer->waypoints.begin(); i != layer->waypoints.end(); i++) {
			changed = changed + (int) i->second->apply_dem_data(true);
		}
	}
	layer->wp_changed_message(changed);
}




void trw_layer_goto_track_endpoint(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	Track * trk = layer->get_track_helper(data);

	if (!trk) {
		return;
	}

	if (trk->empty()) {
		return;
	}
	goto_coord(data->panel, data->layer, data->viewport, &trk->get_tp_last()->coord);
}




void trw_layer_goto_track_max_speed(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	Track * trk = layer->get_track_helper(data);

	if (!trk) {
		return;
	}

	Trackpoint * vtp = trk->get_tp_by_max_speed();
	if (!vtp) {
		return;
	}
	goto_coord(data->panel, data->layer, data->viewport, &vtp->coord);
}




void trw_layer_goto_track_max_alt(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	Track * trk = layer->get_track_helper(data);

	if (!trk) {
		return;
	}

	Trackpoint* vtp = trk->get_tp_by_max_alt();
	if (!vtp) {
		return;
	}
	goto_coord(data->panel, data->layer, data->viewport, &vtp->coord);
}




void trw_layer_goto_track_min_alt(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	Track * trk = layer->get_track_helper(data);

	if (!trk) {
		return;
	}

	Trackpoint* vtp = trk->get_tp_by_min_alt();
	if (!vtp) {
		return;
	}
	goto_coord(data->panel, data->layer, data->viewport, &vtp->coord);
}




/*
 * Automatically change the viewport to center on the track and zoom to see the extent of the track
 */
void trw_layer_auto_track_view(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	Track * trk = layer->get_track_helper(data);

	if (trk && !trk->empty()) {
		struct LatLon maxmin[2] = { {0,0}, {0,0} };
		LayerTRW::find_maxmin_in_track(trk, maxmin);
		layer->zoom_to_show_latlons(data->viewport, maxmin);
		if (data->panel) {
			data->panel->emit_update();
		} else {
			layer->emit_update();
		}
	}
}




/*
 * Refine the selected track/route with a routing engine.
 * The routing engine is selected by the user, when requestiong the job.
 */
void trw_layer_route_refine(trw_menu_sublayer_t * data)
{
	static int last_engine = 0;
	LayerTRW * layer = data->layer;
	Track * trk = layer->get_track_helper(data);

	if (trk && !trk->empty()) {
		/* Check size of the route */
		int nb = trk->get_tp_count();
		if (nb > 100) {
			GtkWidget *dialog = gtk_message_dialog_new(gtk_window_from_layer(layer),
								   (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
								   GTK_MESSAGE_WARNING,
								   GTK_BUTTONS_OK_CANCEL,
								   _("Refining a track with many points (%d) is unlikely to yield sensible results. Do you want to Continue?"),
								   nb);
			int response = gtk_dialog_run(GTK_DIALOG(dialog));
			gtk_widget_destroy(dialog);
			if (response != GTK_RESPONSE_OK) {
				return;
			}

		}
		/* Select engine from dialog */
		GtkWidget *dialog = gtk_dialog_new_with_buttons(_("Refine Route with Routing Engine..."),
								gtk_window_from_layer(layer),
								(GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
								GTK_STOCK_CANCEL,
								GTK_RESPONSE_REJECT,
								GTK_STOCK_OK,
								GTK_RESPONSE_ACCEPT,
								NULL);
		GtkWidget *label = gtk_label_new(_("Select routing engine"));
		gtk_widget_show_all(label);

		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), label, true, true, 0);

		GtkWidget * combo = vik_routing_ui_selector_new((Predicate)vik_routing_engine_supports_refine, NULL);
		gtk_combo_box_set_active(GTK_COMBO_BOX (combo), last_engine);
		gtk_widget_show_all(combo);

		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), combo, true, true, 0);

		gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

		if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
			/* Dialog validated: retrieve selected engine and do the job */
			last_engine = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
			VikRoutingEngine *routing = vik_routing_ui_selector_get_nth(combo, last_engine);

			/* Change cursor */
			window_from_layer(layer)->set_busy_cursor();

			/* Force saving track */
			/* FIXME: remove or rename this hack */
			layer->route_finder_check_added_track = true;

			/* the job */
			vik_routing_engine_refine(routing, layer->vl, trk);

			/* FIXME: remove or rename this hack */
			if (layer->route_finder_added_track) {
				layer->route_finder_added_track->calculate_bounds();
			}

			layer->route_finder_added_track = NULL;
			layer->route_finder_check_added_track = false;

			layer->emit_update();

			/* Restore cursor */
			window_from_layer(layer)->clear_busy_cursor();
		}
		gtk_widget_destroy(dialog);
	}
}




void trw_layer_edit_trackpoint(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	layer->tpwin_init();
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




/**
 * comparison function which can be used to sort tracks or waypoints by name
 */
int sort_alphabetically(gconstpointer a, gconstpointer b, void * user_data)
{
	const char* namea = (const char*) a;
	const char* nameb = (const char*) b;
	if (namea == NULL || nameb == NULL) {
		return 0;
	} else {
		// Same sort method as used in the vik_treeview_*_alphabetize functions
		return strcmp(namea, nameb);
	}
}




/**
 * Attempt to merge selected track with other tracks specified by the user
 * Tracks to merge with must be of the same 'type' as the selected track -
 *  either all with timestamps, or all without timestamps
 */
void trw_layer_merge_with_other(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	sg_uid_t uid = data->sublayer_uid;
	std::unordered_map<sg_uid_t, SlavGPS::Track*> * ght_tracks = NULL;
	if (data->sublayer_type == SublayerType::ROUTE) {
		ght_tracks = &layer->routes;
	} else {
		ght_tracks = &layer->tracks;
	}

	Track * trk = ght_tracks->at(uid);

	if (!trk) {
		return;
	}

	if (trk->empty()) {
		return;
	}

	/* with_timestamps: allow merging with 'similar' time type time tracks
	   i.e. either those times, or those without */
	bool with_timestamps = trk->get_tp_first()->has_timestamp;
	GList * other_tracks = LayerTRWc::find_tracks_with_timestamp_type(ght_tracks, with_timestamps, trk);
	other_tracks = g_list_reverse(other_tracks);

	if (!other_tracks) {
		if (with_timestamps) {
			a_dialog_error_msg(gtk_window_from_layer(layer), _("Failed. No other tracks with timestamps in this layer found"));
		} else {
			a_dialog_error_msg(gtk_window_from_layer(layer), _("Failed. No other tracks without timestamps in this layer found"));
		}
		return;
	}

	// Sort alphabetically for user presentation
	// Convert into list of names for usage with dialog function
	// TODO: Need to consider how to work best when we can have multiple tracks the same name...
	GList *other_tracks_names = NULL;
	GList *iter = g_list_first(other_tracks);
	while (iter) {
		other_tracks_names = g_list_append(other_tracks_names, ght_tracks->at((sg_uid_t) ((long) iter->data))->name);
		iter = g_list_next(iter);
	}

	other_tracks_names = g_list_sort_with_data(other_tracks_names, sort_alphabetically, NULL);

	GList *merge_list = a_dialog_select_from_list(gtk_window_from_layer(layer),
						      other_tracks_names,
						      true,
						      _("Merge with..."),
						      trk->is_route ? _("Select route to merge with") : _("Select track to merge with"));
	g_list_free(other_tracks);
	g_list_free(other_tracks_names);

	if (merge_list) {
		GList *l;
		for (l = merge_list; l != NULL; l = g_list_next(l)) {
			Track *merge_track;
			if (trk->is_route) {
				merge_track = layer->get_route((const char *) l->data);
			} else {
				merge_track = layer->get_track((const char *) l->data);
			}

			if (merge_track) {
				trk->steal_and_append_trackpoints(merge_track);
				if (trk->is_route) {
					layer->delete_route(merge_track);
				} else {
					layer->delete_track(merge_track);
				}
				trk->sort(Trackpoint::compare_timestamps);
			}
		}
		for (l = merge_list; l != NULL; l = g_list_next(l)) {
			free(l->data);
		}
		g_list_free(merge_list);

		layer->emit_update();
	}
}




/**
 * Join - this allows combining 'tracks' and 'track routes'
 *  i.e. doesn't care about whether tracks have consistent timestamps
 * ATM can only append one track at a time to the currently selected track
 */
void trw_layer_append_track(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	Track *trk;
	std::unordered_map<sg_uid_t, SlavGPS::Track*> * ght_tracks = NULL;
	if (data->sublayer_type == SublayerType::ROUTE) {
		ght_tracks = &layer->routes;
	} else {
		ght_tracks = &layer->tracks;
	}

	sg_uid_t uid = data->sublayer_uid;
	trk = ght_tracks->at(uid);

	if (!trk) {
		return;
	}

	GList *other_tracks_names = NULL;

	// Sort alphabetically for user presentation
	// Convert into list of names for usage with dialog function
	// TODO: Need to consider how to work best when we can have multiple tracks the same name...
	twt_udata udata;
	udata.result = &other_tracks_names;
	udata.exclude = trk;

	LayerTRWc::sorted_track_id_by_name_list_exclude_self(ght_tracks, &udata);

	// Note the limit to selecting one track only
	//  this is to control the ordering of appending tracks, i.e. the selected track always goes after the current track
	//  (otherwise with multiple select the ordering would not be controllable by the user - automatically being alphabetically)
	GList *append_list = a_dialog_select_from_list(gtk_window_from_layer(layer),
						       other_tracks_names,
						       false,
						       trk->is_route ? _("Append Route"): _("Append Track"),
						       trk->is_route ? _("Select the route to append after the current route") :
						       _("Select the track to append after the current track"));

	g_list_free(other_tracks_names);

	// It's a list, but shouldn't contain more than one other track!
	if (append_list) {

		for (GList * l = append_list; l != NULL; l = g_list_next(l)) {
			// TODO: at present this uses the first track found by name,
			//  which with potential multiple same named tracks may not be the one selected...
			Track *append_track;
			if (trk->is_route) {
				append_track = layer->get_route((const char *) l->data);
			} else {
				append_track = layer->get_track((const char *) l->data);
			}

			if (append_track) {
				trk->steal_and_append_trackpoints(append_track);
				if (trk->is_route) {
					layer->delete_route(append_track);
				} else {
					layer->delete_track(append_track);
				}
			}
		}
		for (GList * l = append_list; l != NULL; l = g_list_next(l)) {
			free(l->data);
		}
		g_list_free(append_list);

		layer->emit_update();
	}
}




/**
 * Very similar to trw_layer_append_track for joining
 * but this allows selection from the 'other' list
 * If a track is selected, then is shows routes and joins the selected one
 * If a route is selected, then is shows tracks and joins the selected one
 */
void trw_layer_append_other(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	sg_uid_t uid = data->sublayer_uid;

	std::unordered_map<sg_uid_t, SlavGPS::Track*> * ght_mykind;
	std::unordered_map<sg_uid_t, SlavGPS::Track*> * ght_others;
	if (data->sublayer_type == SublayerType::ROUTE) {
		ght_mykind = &layer->routes;
		ght_others = &layer->tracks;
	} else {
		ght_mykind = &layer->tracks;
		ght_others = &layer->routes;
	}

	Track * trk = ght_mykind->at(uid);

	if (!trk) {
		return;
	}

	GList *other_tracks_names = NULL;

	// Sort alphabetically for user presentation
	// Convert into list of names for usage with dialog function
	// TODO: Need to consider how to work best when we can have multiple tracks the same name...
	twt_udata udata;
	udata.result = &other_tracks_names;
	udata.exclude = trk;

	LayerTRWc::sorted_track_id_by_name_list_exclude_self(ght_others, &udata);

	// Note the limit to selecting one track only
	//  this is to control the ordering of appending tracks, i.e. the selected track always goes after the current track
	//  (otherwise with multiple select the ordering would not be controllable by the user - automatically being alphabetically)
	GList *append_list = a_dialog_select_from_list(gtk_window_from_layer(layer),
						       other_tracks_names,
						       false,
						       trk->is_route ? _("Append Track"): _("Append Route"),
						       trk->is_route ? _("Select the track to append after the current route") :
						       _("Select the route to append after the current track"));

	g_list_free(other_tracks_names);

	// It's a list, but shouldn't contain more than one other track!
	if (append_list) {
		for (GList * l = append_list; l != NULL; l = g_list_next(l)) {
			// TODO: at present this uses the first track found by name,
			//  which with potential multiple same named tracks may not be the one selected...

			// Get FROM THE OTHER TYPE list
			Track *append_track;
			if (trk->is_route) {
				append_track = layer->get_track((const char *) l->data);
			} else {
				append_track = layer->get_route((const char *) l->data);
			}

			if (append_track) {

				if (!append_track->is_route
				    && ((append_track->get_segment_count() > 1)
					|| (append_track->get_average_speed() > 0.0))) {

					if (a_dialog_yes_or_no(gtk_window_from_layer(layer),
							       _("Converting a track to a route removes extra track data such as segments, timestamps, etc...\nDo you want to continue?"), NULL)) {
						append_track->merge_segments();
						append_track->to_routepoints();
					} else {
						break;
					}
				}

				trk->steal_and_append_trackpoints(append_track);

				// Delete copied which is FROM THE OTHER TYPE list
				if (trk->is_route) {
					layer->delete_track(append_track);
				} else {
					layer->delete_route(append_track);
				}
			}
		}
		for (GList * l = append_list; l != NULL; l = g_list_next(l)) {
			free(l->data);
		}
		g_list_free(append_list);
		layer->emit_update();
	}
}




/* merge by segments */
void trw_layer_merge_by_segment(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	sg_uid_t uid = data->sublayer_uid;
	Track * trk = layer->tracks.at(uid);
	unsigned int segments = trk->merge_segments();
	// NB currently no need to redraw as segments not actually shown on the display
	// However inform the user of what happened:
	char str[64];
	const char *tmp_str = ngettext("%d segment merged", "%d segments merged", segments);
	snprintf(str, 64, tmp_str, segments);
	a_dialog_info_msg(gtk_window_from_layer(layer), str);
}




/* merge by time routine */
void trw_layer_merge_by_timestamp(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	sg_uid_t uid = data->sublayer_uid;

	//time_t t1, t2;

	Track *orig_trk = layer->tracks.at(uid);
	if (!orig_trk->empty()
	    && !orig_trk->get_tp_first()->has_timestamp) {
		a_dialog_error_msg(gtk_window_from_layer(layer), _("Failed. This track does not have timestamp"));
		return;
	}

	GList * tracks_with_timestamp = LayerTRWc::find_tracks_with_timestamp_type(&layer->tracks, true, orig_trk);
	tracks_with_timestamp = g_list_reverse(tracks_with_timestamp);

	if (!tracks_with_timestamp) {
		a_dialog_error_msg(gtk_window_from_layer(layer), _("Failed. No other track in this layer has timestamp"));
		return;
	}
	g_list_free(tracks_with_timestamp);

	static unsigned int threshold_in_minutes = 1;
	if (!a_dialog_time_threshold(gtk_window_from_layer(layer),
				     _("Merge Threshold..."),
				     _("Merge when time between tracks less than:"),
				     &threshold_in_minutes)) {
		return;
	}

	// keep attempting to merge all tracks until no merges within the time specified is possible
	bool attempt_merge = true;
	GList *nearby_tracks = NULL;
	GList *trps;

	while (attempt_merge) {

		// Don't try again unless tracks have changed
		attempt_merge = false;

		/* kamilTODO: why call this here? Shouldn't we call this way earlier? */
		if (orig_trk->empty()) {
			return;
		}

		if (nearby_tracks) {
			g_list_free(nearby_tracks);
			nearby_tracks = NULL;
		}

		/* get a list of adjacent-in-time tracks */
		nearby_tracks = LayerTRWc::find_nearby_tracks_by_time(layer->tracks, orig_trk, (threshold_in_minutes * 60));

		/* merge them */

		for (GList *l = nearby_tracks; l; l = g_list_next(l)) {
			/* remove trackpoints from merged track, delete track */
			orig_trk->steal_and_append_trackpoints(((Track *) l->data));
			layer->delete_track(((Track *) l->data));

			// Tracks have changed, therefore retry again against all the remaining tracks
			attempt_merge = true;
		}

		orig_trk->sort(Trackpoint::compare_timestamps);
	}

	g_list_free(nearby_tracks);

	layer->emit_update();
}




/**
 * Split a track at the currently selected trackpoint
 */
void LayerTRW::split_at_selected_trackpoint(SublayerType sublayer_type)
{
	if (!this->selected_tp.valid) {
		return;
	}

	if (this->selected_tp.iter != this->selected_track->begin()
	    && this->selected_tp.iter != std::prev(this->selected_track->end())) {

		char * name = this->new_unique_sublayer_name(sublayer_type, this->selected_track->name);
		if (name) {

			/* Selected Trackpoint stays in old track, but its copy goes to new track too. */
			Trackpoint * selected = new Trackpoint(**this->selected_tp.iter);

			Track * new_track = new Track(*this->selected_track, std::next(this->selected_tp.iter), this->selected_track->end());
			new_track->push_front(selected);

			this->selected_track->erase(std::next(this->selected_tp.iter), this->selected_track->end());
			this->selected_track->calculate_bounds(); /* Bounds of the selected track changed due to the split. */

			this->selected_tp.iter = new_track->begin();
			this->selected_track = new_track;
			this->selected_track->calculate_bounds();

			sg_uid_t uid = 0;
			if (new_track->is_route) {
				this->add_route(new_track, name);
				uid = LayerTRWc::find_uid_of_track(this->routes, new_track);
			} else {
				this->add_track(new_track, name);
				uid = LayerTRWc::find_uid_of_track(this->tracks, new_track);
			}
			/* kamilTODO: how it's possible that a new track will already have an uid? */
			fprintf(stderr, "uid of new track is %u\n", uid);

			this->current_tp_uid = uid;

			this->emit_update();
			free(name);
		}
	}
}




/* split by time routine */
void trw_layer_split_by_timestamp(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	sg_uid_t uid = data->sublayer_uid;
	Track * trk = layer->tracks.at(uid);

	static unsigned int thr = 1;

	if (trk->empty()) {
		return;
	}

	if (!a_dialog_time_threshold(gtk_window_from_layer(layer),
				     _("Split Threshold..."),
				     _("Split when time between trackpoints exceeds:"),
				     &thr)) {
		return;
	}

	/* iterate through trackpoints, and copy them into new lists without touching original list */
	auto iter = trk->trackpointsB->begin();
	time_t prev_ts = (*iter)->timestamp;

	TrackPoints * newtps = new TrackPoints;
	std::list<TrackPoints *> points;

	for (; iter != trk->trackpointsB->end(); iter++) {
		time_t ts = (*iter)->timestamp;

		// Check for unordered time points - this is quite a rare occurence - unless one has reversed a track.
		if (ts < prev_ts) {
			char tmp_str[64];
			strftime(tmp_str, sizeof(tmp_str), "%c", localtime(&ts));
			if (a_dialog_yes_or_no(gtk_window_from_layer(layer),
					       _("Can not split track due to trackpoints not ordered in time - such as at %s.\n\nGoto this trackpoint?"),
					       tmp_str)) {
				goto_coord(data->panel, data->layer, data->viewport, &(*iter)->coord);
			}
			return;
		}

		if (ts - prev_ts > thr * 60) {
			/* flush accumulated trackpoints into new list */
			points.push_back(newtps);
			newtps = new TrackPoints;
		}

		/* Accumulate trackpoint copies in newtps. */
		newtps->push_back(new Trackpoint(**iter));
		prev_ts = ts;
	}
	if (!newtps->empty()) {
		points.push_back(newtps);
	}

	/* Only bother updating if the split results in new tracks. */
	if (points.size() > 1) {
		layer->create_new_tracks(trk, &points);
	}

	/* Trackpoints are copied to new tracks, but lists of the Trackpoints need to be deallocated. */
	for (auto iter = points.begin(); iter != points.end(); iter++) {
		delete *iter;
	}

	return;
}




/**
 * Split a track by the number of points as specified by the user
 */
void trw_layer_split_by_n_points(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	Track * trk = layer->get_track_helper(data);

	if (!trk || trk->empty()) {
		return;
	}

	int n_points = a_dialog_get_positive_number(gtk_window_from_layer(layer),
						    _("Split Every Nth Point"),
						    _("Split on every Nth point:"),
						    250,   // Default value as per typical limited track capacity of various GPS devices
						    2,     // Min
						    65536, // Max
						    5);    // Step
	// Was a valid number returned?
	if (!n_points) {
		return;
	}

	// Now split...
	TrackPoints * newtps = new TrackPoints;
	std::list<TrackPoints *> points;

	int count = 0;

	for (auto iter = trk->trackpointsB->begin(); iter != trk->trackpointsB->end(); iter++) {
		/* Accumulate trackpoint copies in newtps, in reverse order */
		newtps->push_back(new Trackpoint(**iter));
		count++;
		if (count >= n_points) {
			/* flush accumulated trackpoints into new list */
			points.push_back(newtps);
			newtps = new TrackPoints;
			count = 0;
		}
	}

	/* If there is a remaining chunk put that into the new split list.
	   This may well be the whole track if no split points were encountered. */
	if (newtps->size()) {
		points.push_back(newtps);
	}

	/* Only bother updating if the split results in new tracks. */
	if (points.size() > 1) {
		layer->create_new_tracks(trk, &points);
	}

	/* Trackpoints are copied to new tracks, but lists of the Trackpoints need to be deallocated. */
	for (auto iter = points.begin(); iter != points.end(); iter++) {
		delete *iter;
	}
}




/*
  orig - original track
  points- list of trackpoint lists
*/
bool LayerTRW::create_new_tracks(Track * orig, std::list<TrackPoints *> * points)
{
	for (auto iter = points->begin(); iter != points->end(); iter++) {

		Track * copy = new Track(*orig, (*iter)->begin(), (*iter)->end());

		char * new_tr_name = NULL;
		if (orig->is_route) {
			new_tr_name = this->new_unique_sublayer_name(SublayerType::ROUTE, orig->name);
			this->add_route(copy, new_tr_name);
		} else {
			new_tr_name = this->new_unique_sublayer_name(SublayerType::TRACK, orig->name);
			this->add_track(copy, new_tr_name);
		}
		free(new_tr_name);
		copy->calculate_bounds();
	}

	/* Remove original track and then update the display. */
	if (orig->is_route) {
		this->delete_route(orig);
	} else {
		this->delete_track(orig);
	}
	this->emit_update();

	return true;
}




/**
 * Split a track at the currently selected trackpoint
 */
void trw_layer_split_at_trackpoint(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	layer->split_at_selected_trackpoint(data->sublayer_type);
}




/**
 * Split a track by its segments
 * Routes do not have segments so don't call this for routes
 */
void trw_layer_split_segments(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	sg_uid_t uid = data->sublayer_uid;
	Track *trk = layer->tracks.at(uid);

	if (!trk) {
		return;
	}

	std::list<Track *> * tracks = trk->split_into_segments();
	for (auto iter = tracks->begin(); iter != tracks->end(); iter++) {
		if (*iter) {
			char * new_tr_name = layer->new_unique_sublayer_name(SublayerType::TRACK, trk->name);
			layer->add_track(*iter, new_tr_name);
			free(new_tr_name);
		}
	}
	if (tracks) {
		delete tracks;
		// Remove original track
		layer->delete_track(trk);
		layer->emit_update();
	} else {
		a_dialog_error_msg(gtk_window_from_layer(layer), _("Can not split track as it has no segments"));
	}
}
/* end of split/merge routines */




void LayerTRW::trackpoint_selected_delete(Track * trk)
{
	TrackPoints::iterator new_tp_iter = trk->delete_trackpoint(this->selected_tp.iter);

	if (new_tp_iter != trk->end()) {
		/* Set to current to the available adjacent trackpoint. */
		this->selected_tp.iter = new_tp_iter;

		if (this->selected_track) {
			this->selected_track->calculate_bounds();
		}
	} else {
		this->cancel_current_tp(false);
	}
}




/**
 * Delete the selected point
 */
void trw_layer_delete_point_selected(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	Track * trk = layer->get_track_helper(data);

	if (!trk) {
		return;
	}

	if (!layer->selected_tp.valid) {
		return;
	}

	layer->trackpoint_selected_delete(trk);

	// Track has been updated so update tps:
	layer->cancel_tps_of_track(trk);

	layer->emit_update();
}




/**
 * Delete adjacent track points at the same position
 * AKA Delete Dulplicates on the Properties Window
 */
void trw_layer_delete_points_same_position(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	Track * trk = layer->get_track_helper(data);

	if (!trk) {
		return;
	}

	unsigned long removed = trk->remove_dup_points();

	// Track has been updated so update tps:
	layer->cancel_tps_of_track(trk);

	// Inform user how much was deleted as it's not obvious from the normal view
	char str[64];
	const char *tmp_str = ngettext("Deleted %ld point", "Deleted %ld points", removed);
	snprintf(str, 64, tmp_str, removed);
	a_dialog_info_msg(gtk_window_from_layer(layer), str);

	layer->emit_update();
}




/**
 * Delete adjacent track points with the same timestamp
 * Normally new tracks that are 'routes' won't have any timestamps so should be OK to clean up the track
 */
void trw_layer_delete_points_same_time(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	Track * trk = layer->get_track_helper(data);

	if (!trk) {
		return;
	}

	unsigned long removed = trk->remove_same_time_points();

	// Track has been updated so update tps:
	layer->cancel_tps_of_track(trk);

	// Inform user how much was deleted as it's not obvious from the normal view
	char str[64];
	const char *tmp_str = ngettext("Deleted %ld point", "Deleted %ld points", removed);
	snprintf(str, 64, tmp_str, removed);
	a_dialog_info_msg(gtk_window_from_layer(layer), str);

	layer->emit_update();
}




/**
 * Insert a point
 */
void trw_layer_insert_point_after(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	Track * trk = layer->get_track_helper(data);

	if (!trk) {
		return;
	}

	layer->insert_tp_beside_current_tp(false);

	layer->emit_update();
}




void trw_layer_insert_point_before(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	Track * trk = layer->get_track_helper(data);

	if (!trk) {
		return;
	}

	layer->insert_tp_beside_current_tp(true);

	layer->emit_update();
}




/**
 * Reverse a track
 */
void trw_layer_reverse(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	Track * trk = layer->get_track_helper(data);

	if (!trk) {
		return;
	}

	trk->reverse();

	layer->emit_update();
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
		a_dialog_error_msg_extra(gtk_window_from_layer(this), _("Could not launch %s to open file."), diary_program);
		g_error_free(err);
	}
	free(cmd);
}




/**
 * Open a diary at the date of the track or waypoint
 */
void trw_layer_diary(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	sg_uid_t uid = data->sublayer_uid;

	if (data->sublayer_type == SublayerType::TRACK) {
		Track * trk = layer->tracks.at(uid);
		if (!trk) {
			return;
		}

		char date_buf[20];
		date_buf[0] = '\0';
		if (!trk->empty() && (*trk->trackpointsB->begin())->has_timestamp) {
			strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", gmtime(&(*trk->trackpointsB->begin())->timestamp));
			layer->diary_open(date_buf);
		} else {
			a_dialog_info_msg(gtk_window_from_layer(layer), _("This track has no date information."));
		}
	} else if (data->sublayer_type == SublayerType::WAYPOINT) {
		Waypoint * wp = layer->waypoints.at(uid);
		if (!wp) {
			return;
		}

		char date_buf[20];
		date_buf[0] = '\0';
		if (wp->has_timestamp) {
			strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", gmtime(&(wp->timestamp)));
			layer->diary_open(date_buf);
		} else {
			a_dialog_info_msg(gtk_window_from_layer(layer), _("This waypoint has no date information."));
		}
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
		a_dialog_error_msg_extra(gtk_window_from_layer(this), _("Could not launch %s"), astro_program);
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
static char *convert_to_dms(double dec)
{
	char sign_c = ' ';
	if (dec > 0) {
		sign_c = '+';
	} else if (dec < 0) {
		sign_c = '-';
	} else { // Nul value
		sign_c = ' ';
	}

	// Degrees
	double tmp = fabs(dec);
	int val_d = (int)tmp;

	// Minutes
	tmp = (tmp - val_d) * 60;
	int val_m = (int)tmp;

	// Seconds
	double val_s = (tmp - val_m) * 60;

	// Format
	char * result = g_strdup_printf("%c%dd%d\\\'%.4f\\\"", sign_c, val_d, val_m, val_s);
	return result;
}




/**
 * Open an astronomy program at the date & position of the track center, trackpoint or waypoint
 */
void trw_layer_astro(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	sg_uid_t uid = data->sublayer_uid;

	if (data->sublayer_type == SublayerType::TRACK) {
		Track * trk = layer->tracks.at(uid);
		if (!trk) {
			return;
		}

		Trackpoint * tp = NULL;
		if (layer->selected_tp.valid) {
			/* Current trackpoint. */
			tp = *layer->selected_tp.iter;

		} else if (!trk->empty()) {
			/* Otherwise first trackpoint. */
			tp = *trk->begin();
		} else {
			/* Give up. */
			return;
		}

		if (tp->has_timestamp) {
			char date_buf[20];
			strftime(date_buf, sizeof(date_buf), "%Y%m%d", gmtime(&(tp->timestamp)));
			char time_buf[20];
			strftime(time_buf, sizeof(time_buf), "%H:%M:%S", gmtime(&(tp->timestamp)));
			struct LatLon ll;
			vik_coord_to_latlon(&tp->coord, &ll);
			char *lat_str = convert_to_dms(ll.lat);
			char *lon_str = convert_to_dms(ll.lon);
			char alt_buf[20];
			snprintf(alt_buf, sizeof(alt_buf), "%d", (int)round(tp->altitude));
			layer->astro_open(date_buf, time_buf, lat_str, lon_str, alt_buf);
			free(lat_str);
			free(lon_str);
		} else {
			a_dialog_info_msg(gtk_window_from_layer(layer), _("This track has no date information."));
		}
	} else if (data->sublayer_type == SublayerType::WAYPOINT) {
		sg_uid_t wp_uid = data->sublayer_uid;
		Waypoint * wp = layer->waypoints.at(wp_uid);
		if (!wp) {
			return;
		}

		if (wp->has_timestamp) {
			char date_buf[20];
			strftime(date_buf, sizeof(date_buf), "%Y%m%d", gmtime(&(wp->timestamp)));
			char time_buf[20];
			strftime(time_buf, sizeof(time_buf), "%H:%M:%S", gmtime(&(wp->timestamp)));
			struct LatLon ll;
			vik_coord_to_latlon(&wp->coord, &ll);
			char *lat_str = convert_to_dms(ll.lat);
			char *lon_str = convert_to_dms(ll.lon);
			char alt_buf[20];
			snprintf(alt_buf, sizeof(alt_buf), "%d", (int)round(wp->altitude));
			layer->astro_open(date_buf, time_buf, lat_str, lon_str, alt_buf);
			free(lat_str);
			free(lon_str);
		} else {
			a_dialog_info_msg(gtk_window_from_layer(layer), _("This waypoint has no date information."));
		}
	}
}




int check_tracks_for_same_name(gconstpointer aa, gconstpointer bb, void * udata)
{
	const char* namea = (const char*) aa;
	const char* nameb = (const char*) bb;

	// the test
	int result = strcmp(namea, nameb);

	if (result == 0) {
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
void LayerTRW::uniquify_tracks(LayersPanel * panel, std::unordered_map<sg_uid_t, Track *> & track_table, bool ontrack)
{
	// . Search list for an instance of repeated name
	// . get track of this name
	// . create new name
	// . rename track & update equiv. treeview iter
	// . repeat until all different

	same_track_name_udata udata;


	udata.has_same_track_name = false;
	udata.same_track_name = NULL;

	GList * track_names = LayerTRWc::sorted_track_id_by_name_list(track_table);

	// No tracks
	if (!track_names) {
		return;
	}

	GList * dummy_list1 = g_list_sort_with_data(track_names, check_tracks_for_same_name, &udata);

	// Still no tracks...
	if (!dummy_list1) {
		return;
	}

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
			vik_statusbar_set_message(window_from_layer(this)->get_statusbar(), VIK_STATUSBAR_INFO,
						    _("Internal Error in LayerTRW::uniquify_tracks"));
			return;
		}

		// Rename it
		char * newname = this->new_unique_sublayer_name(SublayerType::TRACK, udata.same_track_name);
		trk->set_name(newname);

		// Need want key of it for treeview update
		sg_uid_t uid = LayerTRWc::find_uid_of_track(track_table, trk);
		if (uid) {

			GtkTreeIter *it;
			if (ontrack) {
				it = this->tracks_iters.at(uid);
			} else {
				it = this->routes_iters.at(uid);
			}

			if (it) {
				this->tree_view->set_name(it, newname);
				if (ontrack) {
					this->tree_view->sort_children(&(this->track_iter), this->track_sort_order);
				} else {
					this->tree_view->sort_children(&(this->route_iter), this->track_sort_order);
				}
			}
		}

		// Start trying to find same names again...
		track_names = NULL; /* kamilFIXME: this list is not free()d anywhere? */
		track_names = LayerTRWc::sorted_track_id_by_name_list(track_table);
		udata.has_same_track_name = false;
		GList * dummy_list2 = g_list_sort_with_data(track_names, check_tracks_for_same_name, &udata);

		// No tracks any more - give up searching
		if (!dummy_list2) {
			udata.has_same_track_name = false;
		}
	}

	// Update
	panel->emit_update();
}




void LayerTRW::sort_order_specified(SublayerType sublayer_type, vik_layer_sort_order_t order)
{
	GtkTreeIter * iter = NULL;

	switch (sublayer_type) {
	case SublayerType::TRACKS:
		iter = &(this->track_iter);
		this->track_sort_order = order;
		break;
	case SublayerType::ROUTES:
		iter = &(this->route_iter);
		this->track_sort_order = order;
		break;
	default: // SublayerType::WAYPOINTS:
		iter = &(this->waypoint_iter);
		this->wp_sort_order = order;
		break;
	}

	this->tree_view->sort_children(iter, order);
}




void trw_layer_sort_order_a2z(trw_menu_sublayer_t * data)
{
	data->layer->sort_order_specified(data->sublayer_type, VL_SO_ALPHABETICAL_ASCENDING);
}




void trw_layer_sort_order_z2a(trw_menu_sublayer_t * data)
{
	data->layer->sort_order_specified(data->sublayer_type, VL_SO_ALPHABETICAL_DESCENDING);
}




void trw_layer_sort_order_timestamp_ascend(trw_menu_sublayer_t * data)
{
	data->layer->sort_order_specified(data->sublayer_type, VL_SO_DATE_ASCENDING);
}




void trw_layer_sort_order_timestamp_descend(trw_menu_sublayer_t * data)
{
	data->layer->sort_order_specified(data->sublayer_type, VL_SO_DATE_DESCENDING);
}




/**
 *
 */
void trw_layer_delete_tracks_from_selection(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;

	// Ensure list of track names offered is unique
	if (LayerTRWc::has_same_track_names(layer->tracks)) {
		if (a_dialog_yes_or_no(gtk_window_from_layer(layer),
				       _("Multiple entries with the same name exist. This method only works with unique names. Force unique names now?"), NULL)) {
			layer->uniquify_tracks(data->panel, layer->tracks, true);
		} else {
			return;
		}
	}

	// Sort list alphabetically for better presentation
	GList * all = LayerTRWc::sorted_track_id_by_name_list(layer->tracks);

	if (!all) {
		a_dialog_error_msg(gtk_window_from_layer(layer), _("No tracks found"));
		return;
	}

	// Get list of items to delete from the user
	GList *delete_list = a_dialog_select_from_list(gtk_window_from_layer(layer),
						       all,
						       true,
						       _("Delete Selection"),
						       _("Select tracks to delete"));
	g_list_free(all);

	// Delete requested tracks
	// since specificly requested, IMHO no need for extra confirmation
	if (delete_list) {
		for (GList * l = delete_list; l != NULL; l = g_list_next(l)) {
			// This deletes first trk it finds of that name (but uniqueness is enforced above)
			layer->delete_track_by_name((const char *) l->data, false);
		}
		g_list_free(delete_list);
		// Reset layer timestamps in case they have now changed
		layer->tree_view->set_timestamp(&layer->iter, layer->get_timestamp());

		layer->emit_update();
	}
}




/**
 *
 */
void trw_layer_delete_routes_from_selection(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;

	// Ensure list of track names offered is unique
	if (LayerTRWc::has_same_track_names(layer->routes)) {
		if (a_dialog_yes_or_no(gtk_window_from_layer(layer),
				       _("Multiple entries with the same name exist. This method only works with unique names. Force unique names now?"), NULL)) {
			layer->uniquify_tracks(data->panel, layer->routes, false);
		} else {
			return;
		}
	}

	// Sort list alphabetically for better presentation
	GList * all = LayerTRWc::sorted_track_id_by_name_list(layer->routes);

	if (!all) {
		a_dialog_error_msg(gtk_window_from_layer(layer), _("No routes found"));
		return;
	}

	// Get list of items to delete from the user
	GList *delete_list = a_dialog_select_from_list(gtk_window_from_layer(layer),
						       all,
						       true,
						       _("Delete Selection"),
						       _("Select routes to delete"));
	g_list_free(all);

	// Delete requested routes
	// since specificly requested, IMHO no need for extra confirmation
	if (delete_list) {
		for (GList * l = delete_list; l != NULL; l = g_list_next(l)) {
			// This deletes first route it finds of that name (but uniqueness is enforced above)
			layer->delete_track_by_name((const char *) l->data, true);
		}
		g_list_free(delete_list);
		layer->emit_update();
	}
}




typedef struct {
	bool    has_same_waypoint_name;
	const char *same_waypoint_name;
} same_waypoint_name_udata;

static int check_waypoints_for_same_name(gconstpointer aa, gconstpointer bb, void * udata)
{
	const char* namea = (const char*) aa;
	const char* nameb = (const char*) bb;

	// the test
	int result = strcmp(namea, nameb);

	if (result == 0) {
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
	LayerTRWc::sorted_wp_id_by_name_list(this->waypoints, &waypoint_names);

	// No waypoints
	if (!waypoint_names) {
		return false;
	}

	same_waypoint_name_udata udata;
	udata.has_same_waypoint_name = false;

	// Use sort routine to traverse list comparing items
	// Don't care how this list ends up ordered (doesn't actually change) - care about the returned status
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
void LayerTRW::uniquify_waypoints(LayersPanel * panel)
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

	LayerTRWc::sorted_wp_id_by_name_list(this->waypoints, &waypoint_names);

	// No waypoints
	if (!waypoint_names) {
		return;
	}

	GList * dummy_list1 = g_list_sort_with_data(waypoint_names, check_waypoints_for_same_name, &udata);
	// Still no waypoints...
	if (!dummy_list1) {
		return;
	}

	while (udata.has_same_waypoint_name) {

		// Find a waypoint with the same name
		Waypoint * wp = this->get_waypoint((const char *) udata.same_waypoint_name);
		if (!wp) {
			// Broken :(
			fprintf(stderr, "CRITICAL: Houston, we've had a problem.\n");
			vik_statusbar_set_message(window_from_layer(this)->get_statusbar(), VIK_STATUSBAR_INFO,
						  _("Internal Error in uniquify_waypoints"));
			return;
		}

		// Rename it
		char * newname = this->new_unique_sublayer_name(SublayerType::WAYPOINT, udata.same_waypoint_name);

		this->waypoint_rename(wp, newname);

		// Start trying to find same names again...
		waypoint_names = NULL;
		LayerTRWc::sorted_wp_id_by_name_list(this->waypoints, &waypoint_names);
		udata.has_same_waypoint_name = false;
		GList * dummy_list2 = g_list_sort_with_data(waypoint_names, check_waypoints_for_same_name, &udata);

		// No waypoints any more - give up searching
		if (!dummy_list2) {
			udata.has_same_waypoint_name = false;
		}
	}

	// Update
	panel->emit_update();
}




/**
 *
 */
void trw_layer_delete_waypoints_from_selection(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	GList *all = NULL;

	// Ensure list of waypoint names offered is unique
	if (layer->has_same_waypoint_names()) {
		if (a_dialog_yes_or_no(gtk_window_from_layer(layer),
				       _("Multiple entries with the same name exist. This method only works with unique names. Force unique names now?"), NULL)) {
			layer->uniquify_waypoints(data->panel);
		} else {
			return;
		}
	}

	// Sort list alphabetically for better presentation
	LayerTRWc::sorted_wp_id_by_name_list(layer->waypoints, &all);
	if (!all) {
		a_dialog_error_msg(gtk_window_from_layer(layer), _("No waypoints found"));
		return;
	}

	all = g_list_sort_with_data(all, sort_alphabetically, NULL);

	// Get list of items to delete from the user
	GList *delete_list = a_dialog_select_from_list(gtk_window_from_layer(layer),
						       all,
						       true,
						       _("Delete Selection"),
						       _("Select waypoints to delete"));
	g_list_free(all);

	// Delete requested waypoints
	// since specificly requested, IMHO no need for extra confirmation
	if (delete_list) {
		for (GList * l = delete_list; l != NULL; l = g_list_next(l)) {
			// This deletes first waypoint it finds of that name (but uniqueness is enforced above)
			layer->delete_waypoint_by_name((const char *) l->data);
		}
		g_list_free(delete_list);

		layer->calculate_bounds_waypoints();
		// Reset layer timestamp in case it has now changed
		layer->tree_view->set_timestamp(&layer->iter, layer->get_timestamp());
		layer->emit_update();
	}

}




/**
 *
 */
void trw_layer_waypoints_visibility_off(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	LayerTRWc::set_iter_visibility(layer->waypoints_iters, layer->tree_view, false);
	LayerTRWc::set_waypoints_visibility(layer->waypoints, false);
	// Redraw
	layer->emit_update();
}




/**
 *
 */
void trw_layer_waypoints_visibility_on(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	LayerTRWc::set_iter_visibility(layer->waypoints_iters, layer->tree_view, true);
	LayerTRWc::set_waypoints_visibility(layer->waypoints, true);
	// Redraw
	layer->emit_update();
}




/**
 *
 */
void trw_layer_waypoints_visibility_toggle(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	LayerTRWc::iter_visibility_toggle(layer->waypoints_iters, layer->tree_view);
	LayerTRWc::waypoints_toggle_visibility(layer->waypoints);
	// Redraw
	layer->emit_update();
}




/**
 *
 */
void trw_layer_tracks_visibility_off(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	LayerTRWc::set_iter_visibility(layer->tracks_iters, layer->tree_view, false);
	LayerTRWc::set_tracks_visibility(layer->tracks, false);
	// Redraw
	layer->emit_update();
}




/**
 *
 */
void trw_layer_tracks_visibility_on(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	LayerTRWc::set_iter_visibility(layer->tracks_iters, layer->tree_view, true);
	LayerTRWc::set_tracks_visibility(layer->tracks, true);
	// Redraw
	layer->emit_update();
}




/**
 *
 */
void trw_layer_tracks_visibility_toggle(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	LayerTRWc::iter_visibility_toggle(layer->tracks_iters, layer->tree_view);
	LayerTRWc::tracks_toggle_visibility(layer->tracks);
	// Redraw
	layer->emit_update();
}




/**
 *
 */
void trw_layer_routes_visibility_off(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	LayerTRWc::set_iter_visibility(layer->routes_iters, layer->tree_view, false);
	LayerTRWc::set_tracks_visibility(layer->routes, false);
	// Redraw
	layer->emit_update();
}




/**
 *
 */
void trw_layer_routes_visibility_on(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	LayerTRWc::set_iter_visibility(layer->routes_iters, layer->tree_view, true);
	LayerTRWc::set_tracks_visibility(layer->routes, true);
	// Redraw
	layer->emit_update();
}




/**
 *
 */
void trw_layer_routes_visibility_toggle(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	LayerTRWc::iter_visibility_toggle(layer->routes_iters, layer->tree_view);
	LayerTRWc::tracks_toggle_visibility(layer->routes);
	// Redraw
	layer->emit_update();
}




/**
 * Helper function to construct a list of #waypoint_layer_t
 */
std::list<waypoint_layer_t *> * LayerTRW::create_waypoints_and_layers_list_helper(std::list<Waypoint *> * waypoints)
{
	std::list<waypoint_layer_t *> * waypoints_and_layers = new std::list<waypoint_layer_t *>;
	// build waypoints_and_layers list
	for (auto iter = waypoints->begin(); iter != waypoints->end(); iter++) {
		waypoint_layer_t * element = (waypoint_layer_t *) malloc(sizeof (waypoint_layer_t));
		element->wp = *iter;
		element->trw = this;
		waypoints_and_layers->push_back(element);
	}

	return waypoints_and_layers;
}




/**
 * trw_layer_create_waypoints_and_layers_list:
 *
 * Create the latest list of waypoints with the associated layer(s)
 *  Although this will always be from a single layer here
 */
std::list<SlavGPS::waypoint_layer_t *> * LayerTRW::create_waypoints_and_layers_list()
{
	std::list<Waypoint *> pure_waypoints;

	for (auto iter = this->waypoints.begin(); iter != this->waypoints.end(); iter++) {
		pure_waypoints.push_back((*iter).second);
	}

	return this->create_waypoints_and_layers_list_helper(&pure_waypoints);
}




/**
 * trw_layer_analyse_close:
 *
 * Stuff to do on dialog closure
 */
static void trw_layer_analyse_close(GtkWidget *dialog, int resp, Layer * layer)
{
	gtk_widget_destroy(dialog);
	((LayerTRW *) layer)->tracks_analysis_dialog = NULL;
}




/**
 * Helper function to construct a list of #track_layer_t
 */
std::list<track_layer_t *> * LayerTRW::create_tracks_and_layers_list_helper(std::list<Track *> * tracks)
{
	// build tracks_and_layers list
	std::list<track_layer_t *> * tracks_and_layers = new std::list<track_layer_t *>;
	for (auto iter = tracks->begin(); iter != tracks->end(); iter++) {
		track_layer_t * element = (track_layer_t *) malloc(sizeof (track_layer_t));
		element->trk = *iter;
		element->trw = this;
		tracks_and_layers->push_back(element);
	}
	return tracks_and_layers;
}




/**
 * trw_layer_create_track_list:
 *
 * Create the latest list of tracks with the associated layer(s)
 *  Although this will always be from a single layer here
 */
static std::list<track_layer_t *> * trw_layer_create_tracks_and_layers_list(Layer * layer, SublayerType sublayer_type)
{
	std::list<Track *> * tracks = new std::list<Track *>;
	if (sublayer_type == SublayerType::TRACKS) {
		tracks = LayerTRWc::get_track_values(tracks, ((LayerTRW *) layer)->get_tracks());
	} else {
		tracks = LayerTRWc::get_track_values(tracks, ((LayerTRW *) layer)->get_routes());
	}

	return ((LayerTRW *) layer)->create_tracks_and_layers_list_helper(tracks);
}



/**
 * trw_layer_create_track_list:
 *
 * Create the latest list of tracks with the associated layer(s)
 *  Although this will always be from a single layer here
 */
std::list<track_layer_t *> * LayerTRW::create_tracks_and_layers_list(SublayerType sublayer_type)
{
	std::list<Track *> * tracks = new std::list<Track *>;
	if (sublayer_type == SublayerType::TRACKS) {
		tracks = LayerTRWc::get_track_values(tracks, this->get_tracks());
	} else {
		tracks = LayerTRWc::get_track_values(tracks, this->get_routes());
	}

	return this->create_tracks_and_layers_list_helper(tracks);
}




void trw_layer_tracks_stats(trw_menu_layer_t * data)
{
	LayerTRW * trw = data->layer;
	// There can only be one!
	if (trw->tracks_analysis_dialog) {
		return;
	}

	trw->tracks_analysis_dialog = vik_trw_layer_analyse_this(gtk_window_from_layer(trw),
								 trw->name,
								 trw,
								 SublayerType::TRACKS,
								 trw_layer_analyse_close);
}




/**
 *
 */
void trw_layer_routes_stats(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;
	// There can only be one!
	if (layer->tracks_analysis_dialog) {
		return;
	}

	layer->tracks_analysis_dialog = vik_trw_layer_analyse_this(gtk_window_from_layer(layer),
								   layer->name,
								   layer,
								   SublayerType::ROUTES,
								   trw_layer_analyse_close);
}




void trw_layer_goto_waypoint(trw_menu_sublayer_t * data)
{
	sg_uid_t wp_uid = data->sublayer_uid;
	Waypoint * wp = data->layer->waypoints.at(wp_uid);
	if (wp) {
		goto_coord(data->panel, data->layer, data->viewport, &wp->coord);
	}
}




void trw_layer_waypoint_gc_webpage(trw_menu_sublayer_t * data)
{
	sg_uid_t wp_uid = data->sublayer_uid;
	Waypoint * wp = data->layer->waypoints.at(wp_uid);
	if (!wp) {
		return;
	}
	char *webpage = g_strdup_printf("http://www.geocaching.com/seek/cache_details.aspx?wp=%s", wp->name);
	open_url(gtk_window_from_layer(data->layer), webpage);
	free(webpage);
}




void trw_layer_waypoint_webpage(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;
	sg_uid_t wp_uid = data->sublayer_uid;
	Waypoint * wp = layer->waypoints.at(wp_uid);
	if (!wp) {
		return;
	}

	if (wp->url) {
		open_url(gtk_window_from_layer(layer), wp->url);
	} else if (!strncmp(wp->comment, "http", 4)) {
		open_url(gtk_window_from_layer(layer), wp->comment);
	} else if (!strncmp(wp->description, "http", 4)) {
		open_url(gtk_window_from_layer(layer), wp->description);
	}
}




char const * LayerTRW::sublayer_rename_request(const char * newname, void * panel, SublayerType sublayer_type, sg_uid_t sublayer_uid, GtkTreeIter * iter)
{
	if (sublayer_type == SublayerType::WAYPOINT) {
		Waypoint * wp = this->waypoints.at(sublayer_uid);

		// No actual change to the name supplied
		if (wp->name) {
			if (strcmp(newname, wp->name) == 0) {
				return NULL;
			}
		}

		Waypoint * wpf = this->get_waypoint(newname);

		if (wpf) {
			// An existing waypoint has been found with the requested name
			if (!a_dialog_yes_or_no(gtk_window_from_layer(this),
						_("A waypoint with the name \"%s\" already exists. Really rename to the same name?"),
						newname)) {
				return NULL;
			}
		}

		// Update WP name and refresh the treeview
		wp->set_name(newname);

		this->tree_view->set_name(iter, newname);
		this->tree_view->sort_children(&(this->waypoint_iter), this->wp_sort_order);

		((LayersPanel *) panel)->emit_update();

		return newname;
	}

	if (sublayer_type == SublayerType::TRACK) {
		Track * trk = this->tracks.at(sublayer_uid);

		// No actual change to the name supplied
		if (trk->name) {
			if (strcmp(newname, trk->name) == 0) {
				return NULL;
			}
		}

		Track *trkf = this->get_track((const char *) newname);

		if (trkf) {
			// An existing track has been found with the requested name
			if (!a_dialog_yes_or_no(gtk_window_from_layer(this),
						_("A track with the name \"%s\" already exists. Really rename to the same name?"),
						newname)) {
				return NULL;
			}
		}
		// Update track name and refresh GUI parts
		trk->set_name(newname);

		// Update any subwindows that could be displaying this track which has changed name
		// Only one Track Edit Window
		if (this->selected_track == trk && this->tpwin) {
			vik_trw_layer_tpwin_set_track_name(this->tpwin, newname);
		}
		// Property Dialog of the track
		vik_trw_layer_propwin_update(trk);

		this->tree_view->set_name(iter, newname);
		this->tree_view->sort_children(&(this->track_iter), this->track_sort_order);

		((LayersPanel *) panel)->emit_update();

		return newname;
	}

	if (sublayer_type == SublayerType::ROUTE) {
		Track * trk = this->routes.at(sublayer_uid);

		// No actual change to the name supplied
		if (trk->name) {
			if (strcmp(newname, trk->name) == 0) {
				return NULL;
			}
		}

		Track * trkf = this->get_route((const char *) newname);

		if (trkf) {
			// An existing track has been found with the requested name
			if (!a_dialog_yes_or_no(gtk_window_from_layer(this),
						_("A route with the name \"%s\" already exists. Really rename to the same name?"),
						newname)) {
				return NULL;
			}
		}
		// Update track name and refresh GUI parts
		trk->set_name(newname);

		// Update any subwindows that could be displaying this track which has changed name
		// Only one Track Edit Window
		if (this->selected_track == trk && this->tpwin) {
			vik_trw_layer_tpwin_set_track_name(this->tpwin, newname);
		}
		// Property Dialog of the track
		vik_trw_layer_propwin_update(trk);

		this->tree_view->set_name(iter, newname);
		this->tree_view->sort_children(&(this->track_iter), this->track_sort_order);

		((LayersPanel *) panel)->emit_update();

		return newname;
	}
	return NULL;
}




bool is_valid_geocache_name(char *str)
{
	int len = strlen(str);
	return len >= 3 && len <= 7 && str[0] == 'G' && str[1] == 'C' && isalnum(str[2]) && (len < 4 || isalnum(str[3])) && (len < 5 || isalnum(str[4])) && (len < 6 || isalnum(str[5])) && (len < 7 || isalnum(str[6]));
}




#ifndef WINDOWS
void trw_layer_track_use_with_filter(trw_menu_sublayer_t * data)
{
	sg_uid_t uid = data->sublayer_uid;
	Track * trk = data->layer->tracks.at(uid);
	a_acquire_set_filter_track(trk);
}
#endif




#ifdef VIK_CONFIG_GOOGLE


bool LayerTRW::is_valid_google_route(sg_uid_t track_uid)
{
	Track * trk = this->routes.at(track_uid);
	return (trk && trk->comment && strlen(trk->comment) > 7 && !strncmp(trk->comment, "from:", 5));
}




void trw_layer_google_route_webpage(trw_menu_sublayer_t * data)
{
	sg_uid_t uid = data->sublayer_uid;
	Track * trk = data->layer->routes.at(uid);
	if (trk) {
		char *escaped = uri_escape(trk->comment);
		char *webpage = g_strdup_printf("http://maps.google.com/maps?f=q&hl=en&q=%s", escaped);
		open_url(gtk_window_from_layer(data->layer), webpage);
		free(escaped);
		free(webpage);
	}
}

#endif




// TODO: Probably better to rework this track manipulation in viktrack.c
void LayerTRW::insert_tp_beside_current_tp(bool before)
{
	/* Sanity check. */
	if (!this->selected_tp.valid) {
		return;
	}

	Trackpoint * tp_current = *this->selected_tp.iter;
	Trackpoint * tp_other = NULL;

	if (before) {
		if (this->selected_tp.iter == this->selected_track->begin()) {
			return;
		}
		tp_other = *std::prev(this->selected_tp.iter);
	} else {
		if (std::next(this->selected_tp.iter) == this->selected_track->end()) {
			return;
		}
		tp_other = *std::next(this->selected_tp.iter);
	}

	/* Use current and other trackpoints to form a new
	   track point which is inserted into the tracklist. */
	if (tp_other) {

		Trackpoint * tp_new = new Trackpoint(*tp_current, *tp_other, this->coord_mode);
		/* Insert new point into the appropriate trackpoint list,
		   either before or after the current trackpoint as directed. */

		Track * trk = this->tracks.at(this->current_tp_uid);
		if (!trk) {
			/* Otherwise try routes. */
			trk = this->routes.at(this->current_tp_uid);
		}

		if (!trk) {
			return;
		}

		trk->insert(tp_current, tp_new, before);
	}
}




static void trw_layer_cancel_current_tp_cb(LayerTRW * layer, bool destroy)
{
	layer->cancel_current_tp(destroy);
}




void LayerTRW::cancel_current_tp(bool destroy)
{
	if (this->tpwin) {
		if (destroy) {
			gtk_widget_destroy(GTK_WIDGET(this->tpwin));
			this->tpwin = NULL;
		} else {
			vik_trw_layer_tpwin_set_empty(this->tpwin);
		}
	}

	if (this->selected_tp.valid) {
		this->selected_tp.valid = false;

		this->selected_track = NULL;
		this->current_tp_uid = 0;
		this->emit_update();
	}
}




void LayerTRW::my_tpwin_set_tp()
{
	Track * trk = this->selected_track;
	VikCoord vc;
	// Notional center of a track is simply an average of the bounding box extremities
	struct LatLon center = { (trk->bbox.north+trk->bbox.south)/2, (trk->bbox.east+trk->bbox.west)/2 };
	vik_coord_load_from_latlon(&vc, this->coord_mode, &center);
	vik_trw_layer_tpwin_set_tp(this->tpwin, this->selected_track, &this->selected_tp.iter, trk->name, this->selected_track->is_route);
}




static void trw_layer_tpwin_response_cb(LayerTRW * layer, int response)
{
	layer->tpwin_response(response);
}




void LayerTRW::tpwin_response(int response)
{
	assert (this->tpwin != NULL);
	if (response == VIK_TRW_LAYER_TPWIN_CLOSE) {
		this->cancel_current_tp(true);
	}

	if (!this->selected_tp.valid) {
		return;
	}

	if (response == VIK_TRW_LAYER_TPWIN_SPLIT
	    && this->selected_tp.iter != this->selected_track->begin()
	    && std::next(this->selected_tp.iter) != this->selected_track->end()) {

		this->split_at_selected_trackpoint(this->selected_track->is_route ? SublayerType::ROUTE : SublayerType::TRACK);
		this->my_tpwin_set_tp();

	} else if (response == VIK_TRW_LAYER_TPWIN_DELETE) {

		Track * tr = this->tracks.at(this->current_tp_uid);
		if (tr == NULL) {
			tr = this->routes.at(this->current_tp_uid);
		}
		if (tr == NULL) {
			return;
		}

		this->trackpoint_selected_delete(tr);

		if (this->selected_tp.valid) {
			/* Reset dialog with the available adjacent trackpoint. */
			this->my_tpwin_set_tp();
		}

		this->emit_update();

	} else if (response == VIK_TRW_LAYER_TPWIN_FORWARD
		   && this->selected_track
		   && std::next(this->selected_tp.iter) != this->selected_track->end()) {

		this->selected_tp.iter++;
		this->my_tpwin_set_tp();
		this->emit_update(); /* TODO longone: either move or only update if tp is inside drawing window */

	} else if (response == VIK_TRW_LAYER_TPWIN_BACK
		   && this->selected_track
		   && this->selected_tp.iter != this->selected_track->begin()) {

		this->selected_tp.iter--;
		this->my_tpwin_set_tp();
		this->emit_update();

	} else if (response == VIK_TRW_LAYER_TPWIN_INSERT
		   && this->selected_track
		   && std::next(this->selected_tp.iter) != this->selected_track->end()) {

		this->insert_tp_beside_current_tp(false);
		this->emit_update();

	} else if (response == VIK_TRW_LAYER_TPWIN_DATA_CHANGED) {
		this->emit_update();
	}
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
	GtkWindow * parent = gtk_window_from_layer(this); //i.e. the main window

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
	if (dia_pos_x <= 2 || dia_pos_y <= 2) {
		return;
	}

	Viewport * viewport = window_from_layer(this)->get_viewport();

	int vp_xx, vp_yy; // In viewport pixels
	viewport->coord_to_screen(coord, &vp_xx, &vp_yy);

	// Work out the 'bounding box' in pixel terms of the dialog and only move it when over the position

	int dest_x = 0;
	int dest_y = 0;
	if (!gtk_widget_translate_coordinates(GTK_WIDGET(viewport->vvp), GTK_WIDGET(parent), 0, 0, &dest_x, &dest_y)) {
		return;
	}

	// Transform Viewport pixels into absolute pixels
	int tmp_xx = vp_xx + dest_x + win_pos_x - 10;
	int tmp_yy = vp_yy + dest_y + win_pos_y - 10;

	// Is dialog over the point (to within an  ^^ edge value)
	if ((tmp_xx > dia_pos_x) && (tmp_xx < (dia_pos_x + dia_size_x))
	    && (tmp_yy > dia_pos_y) && (tmp_yy < (dia_pos_y + dia_size_y))) {

		if (vertical) {
			// Shift up<->down
			int hh = viewport->get_height();

			// Consider the difference in viewport to the full window
			int offset_y = dest_y;
			// Add difference between dialog and window sizes
			offset_y += win_pos_y + (hh/2 - dia_size_y)/2;

			if (vp_yy > hh/2) {
				// Point in bottom half, move window to top half
				gtk_window_move(dialog, dia_pos_x, offset_y);
			} else {
				// Point in top half, move dialog down
				gtk_window_move(dialog, dia_pos_x, hh/2 + offset_y);
			}
		} else {
			// Shift left<->right
			int ww = viewport->get_width();

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
	return;
}




void LayerTRW::tpwin_init()
{
	if (!this->tpwin) {
		this->tpwin = vik_trw_layer_tpwin_new(gtk_window_from_layer(this));
		g_signal_connect_swapped(GTK_DIALOG(this->tpwin), "response", G_CALLBACK(trw_layer_tpwin_response_cb), this);
		/* connect signals -- DELETE SIGNAL VERY IMPORTANT TO SET TO NULL */
		g_signal_connect_swapped(this->tpwin, "delete-event", G_CALLBACK(trw_layer_cancel_current_tp_cb), this);

		gtk_widget_show_all(GTK_WIDGET(this->tpwin));

		if (this->selected_tp.valid) {
			/* Get tp pixel position. */
			Trackpoint * tp = *this->selected_tp.iter;

			// Shift up<->down to try not to obscure the trackpoint.
			this->dialog_shift(GTK_WINDOW(this->tpwin), &tp->coord, true);
		}
	}

	if (this->selected_tp.valid) {
		if (this->selected_track) {
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

	params.viewport->get_bbox(&params.bbox);

	LayerTRWc::track_search_closest_tp(this->tracks, &params);

	return params.closest_tp;
}




Waypoint * LayerTRW::closest_wp_in_five_pixel_interval(Viewport * viewport, int x, int y)
{
	WPSearchParams params;

	params.x = x;
	params.y = y;
	params.viewport = viewport;
	params.draw_images = this->drawimages;
	params.closest_wp = NULL;
	params.closest_wp_uid = 0;

	LayerTRWc::waypoint_search_closest_tp(this->waypoints, &params);

	return params.closest_wp;
}




// Some forward declarations
static void marker_begin_move(LayerTool * tool, int x, int y);
static void marker_moveto(LayerTool * tool, int x, int y);
static void marker_end_move(LayerTool * tool);
//

bool LayerTRW::select_move(GdkEventMotion * event, Viewport * viewport, LayerTool * tool)
{
	if (tool->ed->holding) {
		VikCoord new_coord;
		viewport->screen_to_coord(event->x, event->y, &new_coord);

		// Here always allow snapping back to the original location
		//  this is useful when one decides not to move the thing afterall
		// If one wants to move the item only a little bit then don't hold down the 'snap' key!

		// snap to TP
		if (event->state & GDK_CONTROL_MASK) {
			Trackpoint * tp = this->closest_tp_in_five_pixel_interval(viewport, event->x, event->y);
			if (tp) {
				new_coord = tp->coord;
			}
		}

		// snap to WP
		if (event->state & GDK_SHIFT_MASK) {
			Waypoint * wp = this->closest_wp_in_five_pixel_interval(viewport, event->x, event->y);
			if (wp) {
				new_coord = wp->coord;
			}
		}

		int x, y;
		viewport->coord_to_screen(&new_coord, &x, &y);

		marker_moveto(tool, x, y);

		return true;
	}
	return false;
}




bool LayerTRW::select_release(GdkEventButton * event, Viewport * viewport, LayerTool * tool)
{
	if (tool->ed->holding && event->button == MouseButton::LEFT) {
		// Prevent accidental (small) shifts when specific movement has not been requested
		//  (as the click release has occurred within the click object detection area)
		if (!tool->ed->moving) {
			return false;
		}

		VikCoord new_coord;
		viewport->screen_to_coord(event->x, event->y, &new_coord);

		// snap to TP
		if (event->state & GDK_CONTROL_MASK) {
			Trackpoint * tp = this->closest_tp_in_five_pixel_interval(viewport, event->x, event->y);
			if (tp) {
				new_coord = tp->coord;
			}
		}

		// snap to WP
		if (event->state & GDK_SHIFT_MASK) {
			Waypoint * wp = this->closest_wp_in_five_pixel_interval(viewport, event->x, event->y);
			if (wp) {
				new_coord = wp->coord;
			}
		}

		fprintf(stderr, "%s:%d: calling marker_end_move\n", __FUNCTION__, __LINE__);
		marker_end_move(tool);

		// Determine if working on a waypoint or a trackpoint
		if (tool->ed->is_waypoint) {
			// Update waypoint position
			this->current_wp->coord = new_coord;
			this->calculate_bounds_waypoints();
			// Reset waypoint pointer
			this->current_wp    = NULL;
			this->current_wp_uid = 0;
		} else {
			if (this->selected_tp.valid) {
				(*this->selected_tp.iter)->coord = new_coord;

				if (this->selected_track) {
					this->selected_track->calculate_bounds();
				}

				if (this->tpwin) {
					if (this->selected_track) {
						this->my_tpwin_set_tp();
					}
				}
				// NB don't reset the selected trackpoint, thus ensuring it's still in the tpwin
			}
		}

		this->emit_update();
		return true;
	}
	return false;
}




/*
  Returns true if a waypoint or track is found near the requested event position for this particular layer
  The item found is automatically selected
  This is a tool like feature but routed via the layer interface, since it's instigated by a 'global' layer tool in vikwindow.c
 */
bool LayerTRW::select_click(GdkEventButton * event, Viewport * viewport, LayerTool * tool)
{
	if (event->button != MouseButton::LEFT) {
		return false;
	}

	if (this->type != LayerType::TRW) {
		return false;
	}

	if (!this->tracks_visible && !this->waypoints_visible && !this->routes_visible) {
		return false;
	}

	LatLonBBox bbox;
	viewport->get_bbox(&bbox);

	// Go for waypoints first as these often will be near a track, but it's likely the wp is wanted rather then the track

	if (this->waypoints_visible && BBOX_INTERSECT (this->waypoints_bbox, bbox)) {
		WPSearchParams wp_params;
		wp_params.viewport = viewport;
		wp_params.x = event->x;
		wp_params.y = event->y;
		wp_params.draw_images = this->drawimages;
		wp_params.closest_wp_uid = 0;
		wp_params.closest_wp = NULL;

		LayerTRWc::waypoint_search_closest_tp(this->waypoints, &wp_params);

		if (wp_params.closest_wp) {

			// Select
			this->tree_view->select_and_expose(this->waypoints_iters.at(wp_params.closest_wp_uid));

			// Too easy to move it so must be holding shift to start immediately moving it
			//   or otherwise be previously selected but not have an image (otherwise clicking within image bounds (again) moves it)
			if (event->state & GDK_SHIFT_MASK
			    || (this->current_wp == wp_params.closest_wp && !this->current_wp->image)) {
				// Put into 'move buffer'
				// NB viewport & window already set in tool
				tool->ed->trw = this;
				tool->ed->is_waypoint = true;

				marker_begin_move(tool, event->x, event->y);
			}

			this->current_wp =     wp_params.closest_wp;
			this->current_wp_uid = wp_params.closest_wp_uid;

			if (event->type == GDK_2BUTTON_PRESS) {
				if (this->current_wp->image) {
					trw_menu_sublayer_t data;
					memset(&data, 0, sizeof (trw_menu_sublayer_t));
					data.layer = this;
					data.misc = this->current_wp->image;
					trw_layer_show_picture(&data);
				}
			}

			this->emit_update();
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
	//tp_params.closest_tp_iter = NULL;
	tp_params.bbox = bbox;

	if (this->tracks_visible) {
		LayerTRWc::track_search_closest_tp(this->tracks, &tp_params);

		if (tp_params.closest_tp) {

			// Always select + highlight the track
			this->tree_view->select_and_expose(this->tracks_iters.at(tp_params.closest_track_uid));

			tool->ed->is_waypoint = false;

			// Select the Trackpoint
			// Can move it immediately when control held or it's the previously selected tp
			if (event->state & GDK_CONTROL_MASK
			    || this->selected_tp.iter == tp_params.closest_tp_iter) {

				// Put into 'move buffer'
				// NB viewport & window already set in tool
				tool->ed->trw = this;
				marker_begin_move(tool, event->x, event->y);
			}

			this->selected_tp.iter = tp_params.closest_tp_iter;
			this->selected_tp.valid = true;

			this->current_tp_uid = tp_params.closest_track_uid;
			this->selected_track = this->tracks.at(tp_params.closest_track_uid);

			this->set_statusbar_msg_info_trkpt(tp_params.closest_tp);

			if (this->tpwin) {
				this->my_tpwin_set_tp();
			}

			this->emit_update();
			return true;
		}
	}

	// Try again for routes
	if (this->routes_visible) {
		LayerTRWc::track_search_closest_tp(this->routes, &tp_params);

		if (tp_params.closest_tp)  {

			// Always select + highlight the track
			this->tree_view->select_and_expose(this->routes_iters.at(tp_params.closest_track_uid));

			tool->ed->is_waypoint = false;

			// Select the Trackpoint
			// Can move it immediately when control held or it's the previously selected tp
			if (event->state & GDK_CONTROL_MASK
			    || this->selected_tp.iter == tp_params.closest_tp_iter) {

				// Put into 'move buffer'
				// NB viewport & window already set in tool
				tool->ed->trw = this;
				marker_begin_move(tool, event->x, event->y);
			}

			this->selected_tp.iter = tp_params.closest_tp_iter;
			this->selected_tp.valid = true;

			this->current_tp_uid = tp_params.closest_track_uid;
			this->selected_track = this->routes.at(tp_params.closest_track_uid);

			this->set_statusbar_msg_info_trkpt(tp_params.closest_tp);

			if (this->tpwin) {
				this->my_tpwin_set_tp();
			}

			this->emit_update();
			return true;
		}
	}

	/* these aren't the droids you're looking for */
	this->current_wp    = NULL;
	this->current_wp_uid = 0;
	this->cancel_current_tp(false);

	// Blank info
	vik_statusbar_set_message(window_from_layer(this)->get_statusbar(), VIK_STATUSBAR_INFO, "");

	return false;
}




bool LayerTRW::show_selected_viewport_menu(GdkEventButton * event, Viewport * viewport)
{
	if (event->button != MouseButton::RIGHT) {
		return false;
	}

	if (this->type != LayerType::TRW) {
		return false;
	}

	if (!this->tracks_visible && !this->waypoints_visible && !this->routes_visible) {
		return false;
	}

	/* Post menu for the currently selected item */

	/* See if a track is selected */
	Track * trk = window_from_layer(this)->get_selected_track();
	if (trk && trk->visible) {

		if (trk->name) {

			if (this->track_right_click_menu) {
				g_object_ref_sink(G_OBJECT(this->track_right_click_menu));
			}

			this->track_right_click_menu = GTK_MENU (gtk_menu_new());

			sg_uid_t uid = 0;;
			if (trk->is_route) {
				uid = LayerTRWc::find_uid_of_track(this->routes, trk);
			} else {
				uid = LayerTRWc::find_uid_of_track(this->tracks, trk);
			}

			if (uid) {

				GtkTreeIter *iter;
				if (trk->is_route) {
					iter = this->routes_iters.at(uid);
				} else {
					iter = this->tracks_iters.at(uid);
				}

				this->sublayer_add_menu_items(this->track_right_click_menu,
							      NULL,
							      trk->is_route ? SublayerType::ROUTE : SublayerType::TRACK,
							      uid,
							      iter,
							      viewport);
			}

			gtk_menu_popup(this->track_right_click_menu, NULL, NULL, NULL, NULL, event->button, gtk_get_current_event_time());

			return true;
		}
	}

	/* See if a waypoint is selected */
	Waypoint * waypoint = window_from_layer(this)->get_selected_waypoint();
	if (waypoint && waypoint->visible) {
		if (waypoint->name) {

			if (this->wp_right_click_menu) {
				g_object_ref_sink(G_OBJECT(this->wp_right_click_menu));
			}

			this->wp_right_click_menu = GTK_MENU (gtk_menu_new());

			sg_uid_t wp_uid = LayerTRWc::find_uid_of_waypoint(this->waypoints, waypoint);
			if (wp_uid) {
				GtkTreeIter * iter = this->waypoints_iters.at(wp_uid);

				this->sublayer_add_menu_items(this->wp_right_click_menu,
							      NULL,
							      SublayerType::WAYPOINT,
							      wp_uid,
							      iter,
							      viewport);
			}
			gtk_menu_popup(this->wp_right_click_menu, NULL, NULL, NULL, NULL, event->button, gtk_get_current_event_time());

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




static void marker_begin_move(LayerTool * tool, int x, int y)
{
	tool->ed->holding = true;
	tool->ed->gc = tool->viewport->new_gc("black", 2);
	gdk_gc_set_function(tool->ed->gc, GDK_INVERT);
	tool->viewport->draw_rectangle(tool->ed->gc, false, x-3, y-3, 6, 6);
	tool->viewport->sync();
	tool->ed->oldx = x;
	tool->ed->oldy = y;
	tool->ed->moving = false;
}




static void marker_moveto(LayerTool * tool, int x, int y)
{
	tool->viewport->draw_rectangle(tool->ed->gc, false, tool->ed->oldx - 3, tool->ed->oldy - 3, 6, 6);
	tool->viewport->draw_rectangle(tool->ed->gc, false, x-3, y-3, 6, 6);
	tool->ed->oldx = x;
	tool->ed->oldy = y;
	tool->ed->moving = true;

	if (tool_sync_done) {
		g_idle_add_full(G_PRIORITY_HIGH_IDLE + 10, tool_sync, tool->viewport, NULL);
		tool_sync_done = false;
	}
}




static void marker_end_move(LayerTool * tool)
{
	tool->viewport->draw_rectangle(tool->ed->gc, false, tool->ed->oldx - 3, tool->ed->oldy - 3, 6, 6);
	g_object_unref(tool->ed->gc);
	tool->ed->holding = false;
	tool->ed->moving = false;
}




/*** Edit waypoint ****/

static LayerTool * tool_edit_waypoint_create(Window * window, Viewport * viewport)
{
	LayerTool * layer_tool = new LayerTool(window, viewport, LayerType::TRW);

	trw_layer_tools[4] = layer_tool;

	layer_tool->radioActionEntry.name = strdup("EditWaypoint");
	layer_tool->radioActionEntry.stock_id = strdup("vik-icon-Edit Waypoint");
	layer_tool->radioActionEntry.label = strdup(N_("_Edit Waypoint"));
	layer_tool->radioActionEntry.accelerator = strdup("<control><shift>E");
	layer_tool->radioActionEntry.tooltip = strdup(N_("Edit Waypoint"));
	layer_tool->radioActionEntry.value = 0;

	layer_tool->click = (VikToolMouseFunc) tool_edit_waypoint_click_cb;
	layer_tool->move = (VikToolMouseMoveFunc) tool_edit_waypoint_move_cb;
	layer_tool->release = (VikToolMouseFunc) tool_edit_waypoint_release_cb;

	layer_tool->cursor_type = GDK_CURSOR_IS_PIXMAP;
	layer_tool->cursor_data = &cursor_edwp_pixbuf;

	layer_tool->ed = (tool_ed_t *) malloc(1 * sizeof (tool_ed_t));
	memset(layer_tool->ed, 0, sizeof (tool_ed_t));

	return layer_tool;
}




static bool tool_edit_waypoint_click_cb(Layer * trw, GdkEventButton *event, LayerTool * tool)
{
	 return ((LayerTRW *) trw)->tool_edit_waypoint_click(event, tool);
}




bool LayerTRW::tool_edit_waypoint_click(GdkEventButton * event, LayerTool * tool)
{
	if (this->type != LayerType::TRW) {
		return false;
	}

	if (tool->ed->holding) {
		return true;
	}

	if (!this->visible || !this->waypoints_visible) {
		return false;
	}

	if (this->current_wp && this->current_wp->visible) {
		/* first check if current WP is within area (other may be 'closer', but we want to move the current) */
		int x, y;
		tool->viewport->coord_to_screen(&this->current_wp->coord, &x, &y);

		if (abs(x - (int)round(event->x)) <= WAYPOINT_SIZE_APPROX
		    && abs(y - (int)round(event->y)) <= WAYPOINT_SIZE_APPROX) {
			if (event->button == MouseButton::RIGHT) {
				this->waypoint_rightclick = true; /* remember that we're clicking; other layers will ignore release signal */
			} else {
				marker_begin_move(tool, event->x, event->y);
			}
			return true;
		}
	}

	WPSearchParams params;
	params.viewport = tool->viewport;
	params.x = event->x;
	params.y = event->y;
	params.draw_images = this->drawimages;
	params.closest_wp_uid = 0;
	params.closest_wp = NULL;
	LayerTRWc::waypoint_search_closest_tp(this->waypoints, &params);
	if (this->current_wp && (this->current_wp == params.closest_wp)) {
		if (event->button == MouseButton::RIGHT) {
			this->waypoint_rightclick = true; /* remember that we're clicking; other layers will ignore release signal */
		} else {
			marker_begin_move(tool, event->x, event->y);
		}
		return false;
	} else if (params.closest_wp) {
		if (event->button == MouseButton::RIGHT) {
			this->waypoint_rightclick = true; /* remember that we're clicking; other layers will ignore release signal */
		} else {
			this->waypoint_rightclick = false;
		}

		this->tree_view->select_and_expose(this->waypoints_iters.at(params.closest_wp_uid));

		this->current_wp = params.closest_wp;
		this->current_wp_uid = params.closest_wp_uid;

		/* could make it so don't update if old WP is off screen and new is null but oh well */
		this->emit_update();
		return true;
	}

	this->current_wp = NULL;
	this->current_wp_uid = 0;
	this->waypoint_rightclick = false;
	this->emit_update();
	return false;
}




static bool tool_edit_waypoint_move_cb(Layer * trw, GdkEventMotion *event, LayerTool * tool)
{
	return ((LayerTRW *) trw)->tool_edit_waypoint_move(event, tool);
}




bool LayerTRW::tool_edit_waypoint_move(GdkEventMotion * event, LayerTool * tool)
{
	if (this->type != LayerType::TRW) {
		return false;
	}

	if (tool->ed->holding) {
		VikCoord new_coord;
		tool->viewport->screen_to_coord(event->x, event->y, &new_coord);

		/* snap to TP */
		if (event->state & GDK_CONTROL_MASK) {
			Trackpoint * tp = this->closest_tp_in_five_pixel_interval(tool->viewport, event->x, event->y);
			if (tp) {
				new_coord = tp->coord;
			}
		}

		/* snap to WP */
		if (event->state & GDK_SHIFT_MASK) {
			Waypoint * wp = this->closest_wp_in_five_pixel_interval(tool->viewport, event->x, event->y);
			if (wp && wp != this->current_wp) {
				new_coord = wp->coord;
			}
		}

		{
			int x, y;
			tool->viewport->coord_to_screen(&new_coord, &x, &y);

			marker_moveto(tool, x, y);
		}
		return true;
	}
	return false;
}




static bool tool_edit_waypoint_release_cb(Layer * trw, GdkEventButton * event, LayerTool * tool)
{
	return ((LayerTRW *) trw)->tool_edit_waypoint_release(event, tool);
}




bool LayerTRW::tool_edit_waypoint_release(GdkEventButton * event, LayerTool * tool)
{
	if (this->type != LayerType::TRW) {
		return false;
	}

	if (tool->ed->holding && event->button == MouseButton::LEFT) {
		VikCoord new_coord;
		tool->viewport->screen_to_coord(event->x, event->y, &new_coord);

		/* snap to TP */
		if (event->state & GDK_CONTROL_MASK) {
			Trackpoint * tp = this->closest_tp_in_five_pixel_interval(tool->viewport, event->x, event->y);
			if (tp) {
				new_coord = tp->coord;
			}
		}

		/* snap to WP */
		if (event->state & GDK_SHIFT_MASK) {
			Waypoint * wp = this->closest_wp_in_five_pixel_interval(tool->viewport, event->x, event->y);
			if (wp && wp != this->current_wp) {
				new_coord = wp->coord;
			}
		}

		marker_end_move(tool);

		this->current_wp->coord = new_coord;

		this->calculate_bounds_waypoints();
		this->emit_update();
		return true;
	}
	/* PUT IN RIGHT PLACE!!! */
	if (event->button == MouseButton::RIGHT && this->waypoint_rightclick) {
		if (this->wp_right_click_menu) {
			g_object_ref_sink(G_OBJECT(this->wp_right_click_menu));
		}
		if (this->current_wp) {
			this->wp_right_click_menu = GTK_MENU (gtk_menu_new());
			this->sublayer_add_menu_items(this->wp_right_click_menu, NULL, SublayerType::WAYPOINT, this->current_wp_uid, this->waypoints_iters.at(this->current_wp_uid), tool->viewport);
			gtk_menu_popup(this->wp_right_click_menu, NULL, NULL, NULL, NULL, event->button, gtk_get_current_event_time());
		}
		this->waypoint_rightclick = false;
	}
	return false;
}




/*** New track ****/

static LayerTool * tool_new_track_create(Window * window, Viewport * viewport)
{
	LayerTool * layer_tool = new LayerTool(window, viewport, LayerType::TRW);

	trw_layer_tools[1] = layer_tool;

	layer_tool->radioActionEntry.name = strdup("CreateTrack");
	layer_tool->radioActionEntry.stock_id = strdup("vik-icon-Create Track");
	layer_tool->radioActionEntry.label = strdup(N_("Create _Track"));
	layer_tool->radioActionEntry.accelerator = strdup("<control><shift>T");
	layer_tool->radioActionEntry.tooltip = strdup(N_("Create Track"));
	layer_tool->radioActionEntry.value = 0;

	layer_tool->click = (VikToolMouseFunc) tool_new_track_click_cb;
	layer_tool->move = (VikToolMouseMoveFunc) tool_new_track_move_cb;
	layer_tool->release = (VikToolMouseFunc) tool_new_track_release_cb;
	layer_tool->key_press = tool_new_track_key_press_cb;

	layer_tool->pan_handler = true;  /* Still need to handle clicks when in PAN mode to disable the potential trackpoint drawing. */
	layer_tool->cursor_type = GDK_CURSOR_IS_PIXMAP;
	layer_tool->cursor_data = &cursor_addtr_pixbuf;

	layer_tool->ed = (tool_ed_t *) malloc(sizeof (tool_ed_t));
	memset(layer_tool->ed, 0, sizeof (tool_ed_t));

	return layer_tool;
}




typedef struct {
	LayerTRW * layer;
	GdkDrawable *drawable;
	GdkGC *gc;
	GdkPixmap *pixmap;
} draw_sync_t;




/*
 * Draw specified pixmap
 */
static int draw_sync(void * data)
{
	draw_sync_t * ds = (draw_sync_t *) data;
	// Sometimes don't want to draw
	//  normally because another update has taken precedent such as panning the display
	//   which means this pixmap is no longer valid
	if (ds->layer->draw_sync_do) {
		gdk_threads_enter();
		gdk_draw_drawable(ds->drawable,
				  ds->gc,
				  ds->pixmap,
				  0, 0, 0, 0, -1, -1);
		ds->layer->draw_sync_done = true;
		gdk_threads_leave();
	}
	free(ds);
	return 0;
}




static char* distance_string(double distance)
{
	char str[128];

	/* draw label with distance */
	DistanceUnit distance_unit = a_vik_get_units_distance();
	switch (distance_unit) {
	case DistanceUnit::MILES:
		if (distance >= VIK_MILES_TO_METERS(1) && distance < VIK_MILES_TO_METERS(100)) {
			g_sprintf(str, "%3.2f miles", VIK_METERS_TO_MILES(distance));
		} else if (distance < 1609.4) {
			g_sprintf(str, "%d yards", (int)(distance*1.0936133));
		} else {
			g_sprintf(str, "%d miles", (int)VIK_METERS_TO_MILES(distance));
		}
		break;
	case DistanceUnit::NAUTICAL_MILES:
		if (distance >= VIK_NAUTICAL_MILES_TO_METERS(1) && distance < VIK_NAUTICAL_MILES_TO_METERS(100)) {
			g_sprintf(str, "%3.2f NM", VIK_METERS_TO_NAUTICAL_MILES(distance));
		} else if (distance < VIK_NAUTICAL_MILES_TO_METERS(1)) {
			g_sprintf(str, "%d yards", (int)(distance*1.0936133));
		} else {
			g_sprintf(str, "%d NM", (int)VIK_METERS_TO_NAUTICAL_MILES(distance));
		}
		break;
	default:
		// DistanceUnit::KILOMETRES
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
static void statusbar_write(double distance, double elev_gain, double elev_loss, double last_step, double angle, LayerTRW * layer)
{
	// Only show elevation data when track has some elevation properties
	char str_gain_loss[64];
	str_gain_loss[0] = '\0';
	char str_last_step[64];
	str_last_step[0] = '\0';
	char *str_total = distance_string(distance);

	if ((elev_gain > 0.1) || (elev_loss > 0.1)) {
		if (a_vik_get_units_height() == HeightUnit::METRES) {
			g_sprintf(str_gain_loss, _(" - Gain %dm:Loss %dm"), (int)elev_gain, (int)elev_loss);
		} else {
			g_sprintf(str_gain_loss, _(" - Gain %dft:Loss %dft"), (int)VIK_METERS_TO_FEET(elev_gain), (int)VIK_METERS_TO_FEET(elev_loss));
		}
	}

	if (last_step > 0) {
		char *tmp = distance_string(last_step);
		g_sprintf(str_last_step, _(" - Bearing %3.1f° - Step %s"), RAD2DEG(angle), tmp);
		free(tmp);
	}

	// Write with full gain/loss information
	char *msg = g_strdup_printf("Total %s%s%s", str_total, str_last_step, str_gain_loss);
	vik_statusbar_set_message(window_from_layer(layer)->get_statusbar(), VIK_STATUSBAR_INFO, msg);
	free(msg);
	free(str_total);
}




/*
 * Figure out what information should be set in the statusbar and then write it
 */
void LayerTRW::update_statusbar()
{
	// Get elevation data
	double elev_gain, elev_loss;
	this->current_track->get_total_elevation_gain(&elev_gain, &elev_loss);

	/* Find out actual distance of current track */
	double distance = this->current_track->get_length();

	statusbar_write(distance, elev_gain, elev_loss, 0, 0, this);
}




static VikLayerToolFuncStatus tool_new_track_move_cb(Layer * trw, GdkEventMotion *event, LayerTool * tool)
{
	return ((LayerTRW *) trw)->tool_new_track_move(event, tool);
}




VikLayerToolFuncStatus LayerTRW::tool_new_track_move(GdkEventMotion * event, LayerTool * tool)
{
	/* if we haven't sync'ed yet, we don't have time to do more. */
	if (this->draw_sync_done && this->current_track && !this->current_track->empty()) {
		Trackpoint * last_tpt = this->current_track->get_tp_last();

		static GdkPixmap *pixmap = NULL;
		int w1, h1, w2, h2;
		// Need to check in case window has been resized
		w1 = tool->viewport->get_width();
		h1 = tool->viewport->get_height();
		if (!pixmap) {
			pixmap = gdk_pixmap_new(gtk_widget_get_window(GTK_WIDGET(tool->viewport->vvp)), w1, h1, -1);
		}
		gdk_drawable_get_size(pixmap, &w2, &h2);
		if (w1 != w2 || h1 != h2) {
			g_object_unref(G_OBJECT (pixmap));
			pixmap = gdk_pixmap_new(gtk_widget_get_window(GTK_WIDGET(tool->viewport->vvp)), w1, h1, -1);
		}

		// Reset to background
		gdk_draw_drawable(pixmap,
				  this->current_track_newpoint_gc,
				  tool->viewport->get_pixmap(),
				  0, 0, 0, 0, -1, -1);

		draw_sync_t * passalong;
		int x1, y1;

		tool->viewport->coord_to_screen(&last_tpt->coord, &x1, &y1);

		// FOR SCREEN OVERLAYS WE MUST DRAW INTO THIS PIXMAP (when using the reset method)
		//  otherwise using Viewport::draw_* functions puts the data into the base pixmap,
		//  thus when we come to reset to the background it would include what we have already drawn!!
		gdk_draw_line(pixmap,
			      this->current_track_newpoint_gc,
			      x1, y1, event->x, event->y);
		// Using this reset method is more reliable than trying to undraw previous efforts via the GDK_INVERT method

		/* Find out actual distance of current track */
		double distance = this->current_track->get_length();

		// Now add distance to where the pointer is //
		VikCoord coord;
		struct LatLon ll;
		tool->viewport->screen_to_coord((int) event->x, (int) event->y, &coord);
		vik_coord_to_latlon(&coord, &ll);
		double last_step = vik_coord_diff(&coord, &last_tpt->coord);
		distance = distance + last_step;

		// Get elevation data
		double elev_gain, elev_loss;
		this->current_track->get_total_elevation_gain(&elev_gain, &elev_loss);

		// Adjust elevation data (if available) for the current pointer position
		double elev_new;
		elev_new = (double) dem_cache_get_elev_by_coord(&coord, VIK_DEM_INTERPOL_BEST);
		if (elev_new != VIK_DEM_INVALID_ELEVATION) {
			if (last_tpt->altitude != VIK_DEFAULT_ALTITUDE) {
				// Adjust elevation of last track point
				if (elev_new > last_tpt->altitude) {
					// Going up
					elev_gain += elev_new - last_tpt->altitude;
				} else {
					// Going down
					elev_loss += last_tpt->altitude - elev_new;
				}
			}
		}

		//
		// Display of the distance 'tooltip' during track creation is controlled by a preference
		//
		if (a_vik_get_create_track_tooltip()) {

			char *str = distance_string(distance);

			PangoLayout *pl = gtk_widget_create_pango_layout(GTK_WIDGET(tool->viewport->vvp), NULL);
			pango_layout_set_font_description(pl, gtk_widget_get_style(GTK_WIDGET(tool->viewport->vvp))->font_desc);
			pango_layout_set_text(pl, str, -1);
			int wd, hd;
			pango_layout_get_pixel_size(pl, &wd, &hd);

			int xd,yd;
			// offset from cursor a bit depending on font size
			xd = event->x + 10;
			yd = event->y - hd;

			// Create a background block to make the text easier to read over the background map
			GdkGC *background_block_gc = tool->viewport->new_gc("#cccccc", 1);
			gdk_draw_rectangle(pixmap, background_block_gc, true, xd-2, yd-2, wd+4, hd+2);
			gdk_draw_layout(pixmap, this->current_track_newpoint_gc, xd, yd, pl);

			g_object_unref(G_OBJECT (pl));
			g_object_unref(G_OBJECT (background_block_gc));
			free(str);
		}

		passalong = (draw_sync_t *) malloc(1 * sizeof (draw_sync_t)); // freed by draw_sync()
		passalong->layer = this;
		passalong->pixmap = pixmap;
		passalong->drawable = gtk_widget_get_window(GTK_WIDGET(tool->viewport->vvp));
		passalong->gc = this->current_track_newpoint_gc;

		double angle;
		double baseangle;
		tool->viewport->compute_bearing(x1, y1, event->x, event->y, &angle, &baseangle);

		// Update statusbar with full gain/loss information
		statusbar_write(distance, elev_gain, elev_loss, last_step, angle, this);

		// draw pixmap when we have time to
		g_idle_add_full(G_PRIORITY_HIGH_IDLE + 10, draw_sync, passalong, NULL);
		this->draw_sync_done = false;
		return VIK_LAYER_TOOL_ACK_GRAB_FOCUS;
	}
	return VIK_LAYER_TOOL_ACK;
}




/* trw->current_track must be valid. */
void LayerTRW::undo_trackpoint_add()
{
	if (!this->current_track || this->current_track->empty()) {
		return;
	}

	auto iter = this->current_track->get_last();
	this->current_track->erase_trackpoint(iter);

	this->current_track->calculate_bounds();
}




static bool tool_new_track_key_press_cb(Layer * trw, GdkEventKey *event, LayerTool * tool)
{
	return ((LayerTRW *) trw)->tool_new_track_key_press(event, tool);
}




bool LayerTRW::tool_new_track_key_press(GdkEventKey *event, LayerTool * tool)
{
	if (this->current_track && event->keyval == GDK_Escape) {
		// Bin track if only one point as it's not very useful
		if (this->current_track->get_tp_count() == 1) {
			if (this->current_track->is_route) {
				this->delete_route(this->current_track);
			} else {
				this->delete_track(this->current_track);
			}
		}
		this->current_track = NULL;
		this->emit_update();
		return true;
	} else if (this->current_track && event->keyval == GDK_BackSpace) {
		this->undo_trackpoint_add();
		this->update_statusbar();
		this->emit_update();
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
bool LayerTRW::tool_new_track_or_route_click(GdkEventButton * event, Viewport * viewport)
{
	if (this->type != LayerType::TRW) {
		return false;
	}

	if (event->button == MouseButton::MIDDLE) {
		// As the display is panning, the new track pixmap is now invalid so don't draw it
		//  otherwise this drawing done results in flickering back to an old image
		this->draw_sync_do = false;
		return false;
	}

	if (event->button == MouseButton::RIGHT) {
		if (!this->current_track) {
			return false;
		}
		this->undo_trackpoint_add();
		this->update_statusbar();
		this->emit_update();
		return true;
	}

	if (event->type == GDK_2BUTTON_PRESS) {
		/* subtract last (duplicate from double click) tp then end */
		if (this->current_track && !this->current_track->empty() && this->ct_x1 == this->ct_x2 && this->ct_y1 == this->ct_y2) {
			/* undo last, then end */
			this->undo_trackpoint_add();
			this->current_track = NULL;
		}
		this->emit_update();
		return true;
	}

	Trackpoint * tp = new Trackpoint();
	viewport->screen_to_coord(event->x, event->y, &tp->coord);

	/* snap to other TP */
	if (event->state & GDK_CONTROL_MASK) {
		Trackpoint * other_tp = this->closest_tp_in_five_pixel_interval(viewport, event->x, event->y);
		if (other_tp) {
			tp->coord = other_tp->coord;
		}
	}

	tp->newsegment = false;
	tp->has_timestamp = false;
	tp->timestamp = 0;

	if (this->current_track) {
		this->current_track->add_trackpoint(tp, true); // Ensure bounds is updated
		/* Auto attempt to get elevation from DEM data (if it's available) */
		this->current_track->apply_dem_data_last_trackpoint();
	}

	this->ct_x1 = this->ct_x2;
	this->ct_y1 = this->ct_y2;
	this->ct_x2 = event->x;
	this->ct_y2 = event->y;

	this->emit_update();
	return true;
}




static bool tool_new_track_click_cb(Layer * trw, GdkEventButton *event, LayerTool * tool)
{
	return ((LayerTRW *) trw)->tool_new_track_click(event, tool);
}




bool LayerTRW::tool_new_track_click(GdkEventButton * event, LayerTool * tool)
{
	// if we were running the route finder, cancel it
	this->route_finder_started = false;

	// ----------------------------------------------------- if current is a route - switch to new track
	if (event->button == MouseButton::LEFT && (!this->current_track || (this->current_track && this->current_track->is_route))) {
		char *name = this->new_unique_sublayer_name(SublayerType::TRACK, _("Track"));
		if (a_vik_get_ask_for_create_track_name()) {
			name = a_dialog_new_track(gtk_window_from_layer(this), name, false);
			if (!name) {
				return false;
			}
		}
		this->new_track_create_common(name);
		free(name);
	}
	return this->tool_new_track_or_route_click(event, tool->viewport);
}




static void tool_new_track_release_cb(Layer * trw, GdkEventButton *event, LayerTool * tool)
{
	((LayerTRW *) trw)->tool_new_track_release(event, tool);
}




void LayerTRW::tool_new_track_release(GdkEventButton *event, LayerTool * tool)
{
	if (event->button == MouseButton::MIDDLE) {
		// Pan moving ended - enable potential point drawing again
		this->draw_sync_do = true;
		this->draw_sync_done = true;
	}
}




/*** New route ****/

static LayerTool * tool_new_route_create(Window * window, Viewport * viewport)
{
	LayerTool * layer_tool = new LayerTool(window, viewport, LayerType::TRW);

	trw_layer_tools[2] = layer_tool;

	layer_tool->radioActionEntry.name = strdup("CreateRoute");
	layer_tool->radioActionEntry.stock_id = strdup("vik-icon-Create Route");
	layer_tool->radioActionEntry.label = strdup(N_("Create _Route"));
	layer_tool->radioActionEntry.accelerator = strdup("<control><shift>B");
	layer_tool->radioActionEntry.tooltip = strdup(N_("Create Route"));
	layer_tool->radioActionEntry.value = 0;

	layer_tool->click = (VikToolMouseFunc) tool_new_route_click_cb;
	layer_tool->move = (VikToolMouseMoveFunc) tool_new_track_move_cb;    /* Reuse this track method for a route. */
	layer_tool->release = (VikToolMouseFunc) tool_new_track_release_cb;  /* Reuse this track method for a route. */
	layer_tool->key_press = tool_new_track_key_press_cb;                 /* Reuse this track method for a route. */

	layer_tool->pan_handler = true;  /* Still need to handle clicks when in PAN mode to disable the potential trackpoint drawing. */
	layer_tool->cursor_type = GDK_CURSOR_IS_PIXMAP;
	layer_tool->cursor_data = &cursor_new_route_pixbuf;
	layer_tool->cursor = NULL;

	layer_tool->ed = (tool_ed_t *) malloc(sizeof (tool_ed_t));
	memset(layer_tool->ed, 0, sizeof (tool_ed_t));

	return layer_tool;
}




static bool tool_new_route_click_cb(Layer * trw, GdkEventButton *event, LayerTool * tool)
{
	return ((LayerTRW *) trw)->tool_new_route_click(event, tool);
}




bool LayerTRW::tool_new_route_click(GdkEventButton * event, LayerTool * tool)
{
	// if we were running the route finder, cancel it
	this->route_finder_started = false;

	// -------------------------- if current is a track - switch to new route,
	if (event->button == MouseButton::LEFT
	    && (!this->current_track
		|| (this->current_track && !this->current_track->is_route))) {

		char *name = this->new_unique_sublayer_name(SublayerType::ROUTE, _("Route"));
		if (a_vik_get_ask_for_create_track_name()) {
			name = a_dialog_new_track(gtk_window_from_layer(this), name, true);
			if (!name) {
				return false;
			}
		}
		this->new_route_create_common(name);
		free(name);
	}
	return this->tool_new_track_or_route_click(event, tool->viewport);
}




/*** New waypoint ****/

static LayerTool * tool_new_waypoint_create(Window * window, Viewport * viewport)
{
	LayerTool * layer_tool = new LayerTool(window, viewport, LayerType::TRW);

	trw_layer_tools[0] = layer_tool;

	layer_tool->radioActionEntry.name = strdup("CreateWaypoint");
	layer_tool->radioActionEntry.stock_id = strdup("vik-icon-Create Waypoint");
	layer_tool->radioActionEntry.label = strdup(N_("Create _Waypoint"));
	layer_tool->radioActionEntry.accelerator = strdup("<control><shift>W");
	layer_tool->radioActionEntry.tooltip = strdup(N_("Create Waypoint"));
	layer_tool->radioActionEntry.value = 0;

	layer_tool->click = (VikToolMouseFunc) tool_new_waypoint_click_cb;

	layer_tool->cursor_type = GDK_CURSOR_IS_PIXMAP;
	layer_tool->cursor_data = &cursor_addwp_pixbuf;

	layer_tool->ed = (tool_ed_t *) malloc(sizeof (tool_ed_t));;
	memset(layer_tool->ed, 0, sizeof (tool_ed_t));

	return layer_tool;
}




static bool tool_new_waypoint_click_cb(Layer * trw, GdkEventButton *event, LayerTool * tool)
{
	return ((LayerTRW *) trw)->tool_new_waypoint_click(event, tool);
}




bool LayerTRW::tool_new_waypoint_click(GdkEventButton * event, LayerTool * tool)
{
	VikCoord coord;
	if (this->type != LayerType::TRW) {
		return false;
	}

	tool->viewport->screen_to_coord(event->x, event->y, &coord);
	if (this->new_waypoint(gtk_window_from_layer(this), &coord)) {
		this->calculate_bounds_waypoints();
		if (this->visible) {
			this->emit_update();
		}
	}
	return true;
}





/*** Edit trackpoint ****/

static LayerTool * tool_edit_trackpoint_create(Window * window, Viewport * viewport)
{
	LayerTool * layer_tool = new LayerTool(window, viewport, LayerType::TRW);

	trw_layer_tools[5] = layer_tool;

	layer_tool->radioActionEntry.name = strdup("EditTrackpoint");
	layer_tool->radioActionEntry.stock_id = strdup("vik-icon-Edit Trackpoint");
	layer_tool->radioActionEntry.label = strdup(N_("Edit Trac_kpoint"));
	layer_tool->radioActionEntry.accelerator = strdup("<control><shift>K");
	layer_tool->radioActionEntry.tooltip = strdup(N_("Edit Trackpoint"));
	layer_tool->radioActionEntry.value = 0;

	layer_tool->click = (VikToolMouseFunc) tool_edit_trackpoint_click_cb;
	layer_tool->move = (VikToolMouseMoveFunc) tool_edit_trackpoint_move_cb;
	layer_tool->release = (VikToolMouseFunc) tool_edit_trackpoint_release_cb;

	layer_tool->cursor_type = GDK_CURSOR_IS_PIXMAP;
	layer_tool->cursor_data = &cursor_edtr_pixbuf;

	layer_tool->ed = (tool_ed_t *) malloc(1 * sizeof (tool_ed_t));
	memset(layer_tool->ed, 0, sizeof (tool_ed_t));

	return layer_tool;
}




static bool tool_edit_trackpoint_click_cb(Layer * trw, GdkEventButton * event, LayerTool * tool)
{
	return ((LayerTRW *) trw)->tool_edit_trackpoint_click(event, tool);
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
bool LayerTRW::tool_edit_trackpoint_click(GdkEventButton * event, LayerTool * tool)
{
	TPSearchParams params;
	params.viewport = tool->viewport;
	params.x = event->x;
	params.y = event->y;
	params.closest_track_uid = 0;
	params.closest_tp = NULL;
	//params.closest_tp_iter = NULL;

	tool->viewport->get_bbox(&params.bbox);

	if (event->button != MouseButton::LEFT) {
		return false;
	}

	if (this->type != LayerType::TRW) {
		return false;
	}

	if (!this->visible || !(this->tracks_visible || this->routes_visible)) {
		return false;
	}

	if (this->selected_tp.valid) {
		/* First check if it is within range of prev. tp. and if current_tp track is shown. (if it is, we are moving that trackpoint.) */
		Trackpoint * tp = *this->selected_tp.iter;
		Track *current_tr = this->tracks.at(this->current_tp_uid);
		if (!current_tr) {
			current_tr = this->routes.at(this->current_tp_uid);
		}

		if (!current_tr) {
			return false;
		}

		int x, y;
		tool->viewport->coord_to_screen(&tp->coord, &x, &y);

		if (current_tr->visible
		    && abs(x - (int)round(event->x)) < TRACKPOINT_SIZE_APPROX
		    && abs(y - (int)round(event->y)) < TRACKPOINT_SIZE_APPROX) {

			marker_begin_move(tool, event->x, event->y);
			return true;
		}

	}

	if (this->tracks_visible) {
		LayerTRWc::track_search_closest_tp(this->tracks, &params);
	}

	if (params.closest_tp) {
		this->tree_view->select_and_expose(this->tracks_iters.at(params.closest_track_uid));

		this->selected_tp.iter = params.closest_tp_iter;
		this->selected_tp.valid = true;

		this->current_tp_uid = params.closest_track_uid;

		this->selected_track = this->tracks.at(params.closest_track_uid);
		this->tpwin_init();
		this->set_statusbar_msg_info_trkpt(params.closest_tp);
		this->emit_update();
		return true;
	}

	if (this->routes_visible) {
		LayerTRWc::track_search_closest_tp(this->routes, &params);
	}

	if (params.closest_tp) {
		this->tree_view->select_and_expose(this->routes_iters.at(params.closest_track_uid));

		this->selected_tp.iter = params.closest_tp_iter;
		this->selected_tp.iter = params.closest_tp_iter;
		this->selected_tp.valid = true;

		this->current_tp_uid = params.closest_track_uid;
		this->selected_track = this->routes.at(params.closest_track_uid);
		this->tpwin_init();
		this->set_statusbar_msg_info_trkpt(params.closest_tp);
		this->emit_update();
		return true;
	}

	/* these aren't the droids you're looking for */
	return false;
}




static bool tool_edit_trackpoint_move_cb(Layer * trw, GdkEventMotion *event, LayerTool * tool)
{
	return ((LayerTRW *) trw)->tool_edit_trackpoint_move(event, tool);
}




bool LayerTRW::tool_edit_trackpoint_move(GdkEventMotion *event, LayerTool * tool)
{
	if (this->type != LayerType::TRW) {
		return false;
	}

	if (tool->ed->holding) {
		VikCoord new_coord;
		tool->viewport->screen_to_coord(event->x, event->y, &new_coord);

		/* snap to TP */
		if (event->state & GDK_CONTROL_MASK) {
			Trackpoint * tp = this->closest_tp_in_five_pixel_interval(tool->viewport, event->x, event->y);
			if (tp && tp != *this->selected_tp.iter) {
				new_coord = tp->coord;
			}
		}
		// this->selected_tp.tp->coord = new_coord;
		{
			int x, y;
			tool->viewport->coord_to_screen(&new_coord, &x, &y);
			marker_moveto(tool, x, y);
		}

		return true;
	}
	return false;
}




static bool tool_edit_trackpoint_release_cb(Layer * trw, GdkEventButton *event, LayerTool * tool)
{
	return ((LayerTRW *) trw)->tool_edit_trackpoint_release(event, tool);
}




bool LayerTRW::tool_edit_trackpoint_release(GdkEventButton * event, LayerTool * tool)
{
	if (this->type != LayerType::TRW) {
		return false;
	}

	if (event->button != MouseButton::LEFT) {
		return false;
	}

	if (tool->ed->holding) {
		VikCoord new_coord;
		tool->viewport->screen_to_coord(event->x, event->y, &new_coord);

		/* snap to TP */
		if (event->state & GDK_CONTROL_MASK) {
			Trackpoint * tp = this->closest_tp_in_five_pixel_interval(tool->viewport, event->x, event->y);
			if (tp && tp != *this->selected_tp.iter) {
				new_coord = tp->coord;
			}
		}

		(*this->selected_tp.iter)->coord = new_coord;
		if (this->selected_track) {
			this->selected_track->calculate_bounds();
		}

		marker_end_move(tool);

		/* diff dist is diff from orig */
		if (this->tpwin) {
			if (this->selected_track) {
				this->my_tpwin_set_tp();
			}
		}

		this->emit_update();
		return true;
	}
	return false;
}




/*** Extended Route Finder ***/

static LayerTool * tool_extended_route_finder_create(Window * window, Viewport * viewport)
{
	LayerTool * layer_tool = new LayerTool(window, viewport, LayerType::TRW);

	trw_layer_tools[3] = layer_tool;

	layer_tool->radioActionEntry.name = strdup("ExtendedRouteFinder");
	layer_tool->radioActionEntry.stock_id = strdup("vik-icon-Route Finder");
	layer_tool->radioActionEntry.label = strdup(N_("Route _Finder"));
	layer_tool->radioActionEntry.accelerator = strdup("<control><shift>F");
	layer_tool->radioActionEntry.tooltip = strdup(N_("Route Finder"));
	layer_tool->radioActionEntry.value = 0;

	layer_tool->click = (VikToolMouseFunc) tool_extended_route_finder_click_cb;
	layer_tool->move = (VikToolMouseMoveFunc) tool_new_track_move_cb;   /* Reuse these track methods on a route. */
	layer_tool->release = (VikToolMouseFunc) tool_new_track_release_cb; /* Reuse these track methods on a route. */
	layer_tool->key_press = tool_extended_route_finder_key_press_cb;

	layer_tool->pan_handler = true;  /* Still need to handle clicks when in PAN mode to disable the potential trackpoint drawing. */
	layer_tool->cursor_type = GDK_CURSOR_IS_PIXMAP;
	layer_tool->cursor_data = &cursor_route_finder_pixbuf;

	layer_tool->ed = (tool_ed_t *) malloc(sizeof (tool_ed_t));
	memset(layer_tool->ed, 0, sizeof (tool_ed_t));

	return layer_tool;
}




void LayerTRW::tool_extended_route_finder_undo()
{
	VikCoord * new_end = this->current_track->cut_back_to_double_point();
	if (new_end) {
		free(new_end);
		this->emit_update();

		/* remove last ' to:...' */
		if (this->current_track->comment) {
			char *last_to = strrchr(this->current_track->comment, 't');
			if (last_to && (last_to - this->current_track->comment > 1)) {
				char *new_comment = g_strndup(this->current_track->comment,
							      last_to - this->current_track->comment - 1);
				this->current_track->set_comment_no_copy(new_comment);
			}
		}
	}
}




static bool tool_extended_route_finder_click_cb(Layer * trw, GdkEventButton *event, LayerTool * tool)
{
	return ((LayerTRW *) trw)->tool_extended_route_finder_click(event, tool);
}




bool LayerTRW::tool_extended_route_finder_click(GdkEventButton * event, LayerTool * tool)
{
	VikCoord tmp;

	tool->viewport->screen_to_coord(event->x, event->y, &tmp);
	if (event->button == MouseButton::RIGHT && this->current_track) {
		this->tool_extended_route_finder_undo();

	} else if (event->button == MouseButton::MIDDLE) {
		this->draw_sync_do = false;
		return false;
	}
	// if we started the track but via undo deleted all the track points, begin again
	else if (this->current_track && this->current_track->is_route && ! this->current_track->get_tp_first()) {
		return this->tool_new_track_or_route_click(event, tool->viewport);

	} else if ((this->current_track && this->current_track->is_route)
		   || (event->state & GDK_CONTROL_MASK && this->current_track)) {

		struct LatLon start, end;

		Trackpoint * tp_start = this->current_track->get_tp_last();
		vik_coord_to_latlon(&tp_start->coord, &start);
		vik_coord_to_latlon(&tmp, &end);

		this->route_finder_started = true;
		this->route_finder_append = true;  // merge tracks. keep started true.

		// update UI to let user know what's going on
		VikStatusbar *sb = window_from_layer(this)->get_statusbar();
		VikRoutingEngine *engine = vik_routing_default_engine();
		if (!engine) {
			vik_statusbar_set_message(sb, VIK_STATUSBAR_INFO, "Cannot plan route without a default routing engine.");
			return true;
		}
		char *msg = g_strdup_printf(_("Querying %s for route between (%.3f, %.3f) and (%.3f, %.3f)."),
					    vik_routing_engine_get_label(engine),
					    start.lat, start.lon, end.lat, end.lon);
		vik_statusbar_set_message(sb, VIK_STATUSBAR_INFO, msg);
		free(msg);
		window_from_layer(this)->set_busy_cursor();


		/* Give GTK a change to display the new status bar before querying the web */
		while (gtk_events_pending()) {
			gtk_main_iteration();
		}

		bool find_status = vik_routing_default_find(this->vl, start, end);

		/* Update UI to say we're done */
		window_from_layer(this)->clear_busy_cursor();
		msg = (find_status)
			? g_strdup_printf(_("%s returned route between (%.3f, %.3f) and (%.3f, %.3f)."), vik_routing_engine_get_label(engine), start.lat, start.lon, end.lat, end.lon)
			: g_strdup_printf(_("Error getting route from %s."), vik_routing_engine_get_label(engine));

		vik_statusbar_set_message(sb, VIK_STATUSBAR_INFO, msg);
		free(msg);

		this->emit_update();
	} else {
		this->current_track = NULL;

		// create a new route where we will add the planned route to
		bool ret = this->tool_new_route_click(event, tool);

		this->route_finder_started = true;

		return ret;
	}
	return true;
}




static bool tool_extended_route_finder_key_press_cb(Layer * trw, GdkEventKey *event, LayerTool * tool)
{
	return ((LayerTRW *) trw)->tool_extended_route_finder_key_press(event, tool);
}




bool LayerTRW::tool_extended_route_finder_key_press(GdkEventKey * event, LayerTool * tool)
{
	if (this->current_track && event->keyval == GDK_Escape) {
		this->route_finder_started = false;
		this->current_track = NULL;
		this->emit_update();
		return true;
	} else if (this->current_track && event->keyval == GDK_BackSpace) {
		this->tool_extended_route_finder_undo();
	}
	return false;
}




/*** Show picture ****/

static LayerTool * tool_show_picture_create(Window * window, Viewport * viewport)
{
	LayerTool * layer_tool = new LayerTool(window, viewport, LayerType::TRW);

	trw_layer_tools[6] = layer_tool;

	layer_tool->radioActionEntry.name = strdup("ShowPicture");
	layer_tool->radioActionEntry.stock_id = strdup("vik-icon-Show Picture");
	layer_tool->radioActionEntry.label = strdup(N_("Show P_icture"));
	layer_tool->radioActionEntry.accelerator = strdup("<control><shift>I");
	layer_tool->radioActionEntry.tooltip = strdup(N_("Show Picture"));
	layer_tool->radioActionEntry.value = 0;

	layer_tool->click = (VikToolMouseFunc) tool_show_picture_click_cb;

	layer_tool->cursor_type = GDK_CURSOR_IS_PIXMAP;
	layer_tool->cursor_data = &cursor_showpic_pixbuf;

	layer_tool->ed = (tool_ed_t *) malloc(sizeof (tool_ed_t));
	memset(layer_tool->ed, 0, sizeof (tool_ed_t));

	return layer_tool;
}




void trw_layer_show_picture(trw_menu_sublayer_t * data)
{
	/* thanks to the Gaim people for showing me ShellExecute and g_spawn_command_line_async */
#ifdef WINDOWS
	ShellExecute(NULL, "open", (char *) data->misc, NULL, NULL, SW_SHOWNORMAL);
#else /* WINDOWS */
	GError *err = NULL;
	char *quoted_file = g_shell_quote((char *) data->misc);
	char *cmd = g_strdup_printf("%s %s", a_vik_get_image_viewer(), quoted_file);
	free(quoted_file);
	if (!g_spawn_command_line_async(cmd, &err)) {
		a_dialog_error_msg_extra(gtk_window_from_layer(data->layer), _("Could not launch %s to open file."), a_vik_get_image_viewer());
		g_error_free(err);
	}
	free(cmd);
#endif /* WINDOWS */
}




static bool tool_show_picture_click_cb(Layer * trw, GdkEventButton * event, LayerTool * tool)
{
	return ((LayerTRW *) trw)->tool_show_picture_click(event, tool);
}




bool LayerTRW::tool_show_picture_click(GdkEventButton * event, LayerTool * tool)
{
	if (this->type != LayerType::TRW) {
		return false;
	}

	char * found = LayerTRWc::tool_show_picture_wp(this->waypoints, event->x, event->y, tool->viewport);
	if (found) {
		trw_menu_sublayer_t data;
		memset(&data, 0, sizeof (trw_menu_sublayer_t));
		data.layer = this;
		data.misc = found;
		trw_layer_show_picture(&data);
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
	LayerTRW * layer;  /* Layer needed for redrawing. */
	GSList * pics;     /* Image list. */
} thumbnail_create_thread_data;

static int create_thumbnails_thread(thumbnail_create_thread_data * tctd, void * threaddata)
{
	unsigned int total = g_slist_length(tctd->pics), done = 0;
	while (tctd->pics) {
		a_thumbnails_create((char *) tctd->pics->data);
		int result = a_background_thread_progress(threaddata, ((double) ++done) / total);
		if (result != 0)
			return -1; /* Abort thread */

		tctd->pics = tctd->pics->next;
	}

	// Redraw to show the thumbnails as they are now created
	if (IS_VIK_LAYER(tctd->layer->vl)) {
		tctd->layer->emit_update(); // NB update from background thread
	}

	return 0;
}




static void thumbnail_create_thread_free(thumbnail_create_thread_data * tctd)
{
	while (tctd->pics) {
		free(tctd->pics->data);
		tctd->pics = g_slist_delete_link(tctd->pics, tctd->pics);
	}
	free(tctd);
}




void LayerTRW::verify_thumbnails(Viewport * viewport)
{
	if (!this->has_verified_thumbnails) {
		GSList *pics = LayerTRWc::image_wp_make_list(this->waypoints);
		if (pics) {
			int len = g_slist_length(pics);
			char *tmp = g_strdup_printf(_("Creating %d Image Thumbnails..."), len);
			thumbnail_create_thread_data * tctd = (thumbnail_create_thread_data *) malloc(sizeof (thumbnail_create_thread_data));
			tctd->layer = this;
			tctd->pics = pics;
			a_background_thread(BACKGROUND_POOL_LOCAL,
					    gtk_window_from_layer(this),
					    tmp,
					    (vik_thr_func) create_thumbnails_thread,
					    tctd,
					    (vik_thr_free_func) thumbnail_create_thread_free,
					    NULL,
					    len);
			free(tmp);
		}
	}
}




static const char* my_track_colors(int ii)
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
	// Tracks
	int ii = 0;
	for (auto i = this->tracks.begin(); i != this->tracks.end(); i++) {

		Track * trk = i->second;

		// Tracks get a random spread of colours if not already assigned
		if (!trk->has_color) {
			if (this->drawmode == DRAWMODE_ALL_SAME_COLOR) {
				trk->color = this->track_color;
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

	this->waypoints_bbox.north = topleft.lat;
	this->waypoints_bbox.east = bottomright.lon;
	this->waypoints_bbox.south = bottomright.lat;
	this->waypoints_bbox.west = topleft.lon;
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
	if (!this->tree_view) {
		return;
	}

	// Obviously need 2 to tango - sorting with only 1 (or less) is a lonely activity!
	if (this->tracks.size() > 1) {
		this->tree_view->sort_children(&(this->track_iter), this->track_sort_order);
	}

	if (this->routes.size() > 1) {
		this->tree_view->sort_children(&(this->route_iter), this->track_sort_order);
	}

	if (this->waypoints.size() > 1) {
		this->tree_view->sort_children(&(this->waypoint_iter), this->wp_sort_order);
	}
}




/**
 * Get the earliest timestamp available from all tracks
 */
time_t LayerTRW::get_timestamp_tracks()
{
	time_t timestamp = 0;
	std::list<Track *> * tracks = new std::list<Track *>;
	tracks = LayerTRWc::get_track_values(tracks, this->tracks);

	if (!tracks->empty()) {
		tracks->sort(Track::compare_timestamp);

		// Only need to check the first track as they have been sorted by time
		Track * trk = *(tracks->begin());
		// Assume trackpoints already sorted by time
		Trackpoint * tpt = trk->get_tp_first();
		if (tpt && tpt->has_timestamp) {
			timestamp = tpt->timestamp;
		}
	}
	delete tracks;
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
time_t LayerTRW::get_timestamp()
{
	time_t timestamp_tracks = this->get_timestamp_tracks();
	time_t timestamp_waypoints = this->get_timestamp_waypoints();
	// NB routes don't have timestamps - hence they are not considered

	if (!timestamp_tracks && !timestamp_waypoints) {
		// Fallback to get time from the metadata when no other timestamps available
		GTimeVal gtv;
		if (this->metadata && this->metadata->timestamp && g_time_val_from_iso8601(this->metadata->timestamp, &gtv)) {
			return gtv.tv_sec;
		}
	}
	if (timestamp_tracks && !timestamp_waypoints) {
		return timestamp_tracks;
	}
	if (timestamp_tracks && timestamp_waypoints && (timestamp_tracks < timestamp_waypoints)) {
		return timestamp_tracks;
	}
	return timestamp_waypoints;
}




void LayerTRW::post_read(Viewport * viewport, bool from_file)
{
	if (this->realized) {
		this->verify_thumbnails(viewport);
	}
	this->track_alloc_colors();

	this->calculate_bounds_waypoints();
	this->calculate_bounds_tracks();

	// Apply treeview sort after loading all the tracks for this layer
	//  (rather than sorted insert on each individual track additional)
	//  and after subsequent changes to the properties as the specified order may have changed.
	//  since the sorting of a treeview section is now very quick
	// NB sorting is also performed after every name change as well to maintain the list order
	this->sort_all();

	// Setting metadata time if not otherwise set
	if (this->metadata) {

		bool need_to_set_time = true;
		if (this->metadata->timestamp) {
			need_to_set_time = false;
			if (!strcmp(this->metadata->timestamp, "")) {
				need_to_set_time = true;
			}
		}

		if (need_to_set_time) {
			GTimeVal timestamp;
			timestamp.tv_usec = 0;
			timestamp.tv_sec = this->get_timestamp();

			// No time found - so use 'now' for the metadata time
			if (timestamp.tv_sec == 0) {
				g_get_current_time(&timestamp);
			}

			this->metadata->timestamp = g_time_val_to_iso8601(&timestamp);
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
bool LayerTRW::uniquify(LayersPanel * panel)
{
	if (panel) {
		this->uniquify_tracks(panel, this->tracks, true);
		this->uniquify_tracks(panel, this->routes, false);
		this->uniquify_waypoints(panel);
		return true;
	}
	return false;
}




static void waypoint_convert(Waypoint * wp, VikCoordMode * dest_mode)
{
	vik_coord_convert(&wp->coord, *dest_mode);
}




void LayerTRW::change_coord_mode(VikCoordMode dest_mode)
{
	if (this->coord_mode != dest_mode) {
		this->coord_mode = dest_mode;
		LayerTRWc::waypoints_convert(this->waypoints, &dest_mode);
		LayerTRWc::track_convert(this->tracks, &dest_mode);
		LayerTRWc::track_convert(this->routes, &dest_mode);
	}
}




void LayerTRW::set_menu_selection(uint16_t selection)
{
	//fprintf(stderr, "=============== set menu selection\n");
	this->menu_selection = (VikStdLayerMenuItem) selection; /* kamil: invalid cast? */
}




uint16_t LayerTRW::get_menu_selection()
{
	//fprintf(stderr, "=============== get menu selection\n");
	return this->menu_selection;
}




/* ----------- Downloading maps along tracks --------------- */

static int get_download_area_width(Viewport * viewport, double zoom_level, struct LatLon *wh) /* kamilFIXME: viewport is unused, why? */
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
	if ((dist->lon >= ABS(to->east_west - from->east_west))
	    && (dist->lat >= ABS(to->north_south - from->north_south))) {

		return NULL;
	}

	VikCoord *coord = (VikCoord *) malloc(sizeof (VikCoord));
	coord->mode = VIK_COORD_LATLON;

	if (ABS(gradient) < 1) {
		if (from->east_west > to->east_west) {
			coord->east_west = from->east_west - dist->lon;
		} else {
			coord->east_west = from->east_west + dist->lon;
		}
		coord->north_south = gradient * (coord->east_west - from->east_west) + from->north_south;
	} else {
		if (from->north_south > to->north_south) {
			coord->north_south = from->north_south - dist->lat;
		} else {
			coord->north_south = from->north_south + dist->lat;
		}
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
		if ((next = get_next_coord(next, to, dist, gradient)) == NULL) {
			break;
		}
		list = g_list_prepend(list, next);
	}

	return list;
}




void vik_track_download_map(Track *tr, VikLayer *vml, Viewport * viewport, double zoom_level)
{
	struct LatLon wh;
	if (get_download_area_width(viewport, zoom_level, &wh)) {
		return;
	}

	if (tr->empty()) {
		return;
	}

	std::list<Rect *> * rects_to_download = tr->get_rectangles(&wh);

	GList * fillins = NULL;

	/* 'fillin' doesn't work in UTM mode - potentially ending up in massive loop continually allocating memory - hence don't do it */
	/* seems that ATM the function get_next_coord works only for LATLON */
	if (tr->get_coord_mode() == VIK_COORD_LATLON) {

		/* fill-ins for far apart points */
		std::list<Rect *>::iterator cur_rect;
		std::list<Rect *>::iterator next_rect;

		for (cur_rect = rects_to_download->begin();
		     (next_rect = std::next(cur_rect)) != rects_to_download->end();
		     cur_rect++) {

			if ((wh.lon < ABS ((*cur_rect)->center.east_west - (*next_rect)->center.east_west)) ||
			    (wh.lat < ABS ((*cur_rect)->center.north_south - (*next_rect)->center.north_south))) {

				fillins = add_fillins(fillins, &(*cur_rect)->center, &(*next_rect)->center, &wh);
			}
		}
	} else {
		fprintf(stderr, "MESSAGE: %s: this feature works only in Mercator mode\n", __FUNCTION__);
	}

	if (fillins) {
		VikCoord tl, br;
		for (GList * fiter = fillins; fiter; fiter = fiter->next) {
			VikCoord * cur_coord = (VikCoord *)(fiter->data);
			vik_coord_set_area(cur_coord, &wh, &tl, &br);
			Rect * rect = (Rect *) malloc(sizeof (Rect));
			rect->tl = tl;
			rect->br = br;
			rect->center = *cur_coord;
			rects_to_download->push_front(rect);
		}
	}

	for (auto rect_iter = rects_to_download->begin(); rect_iter != rects_to_download->end(); rect_iter++) {
		((LayerMaps *) vml->layer)->download_section(&(*rect_iter)->tl, &(*rect_iter)->br, zoom_level);
	}

	if (fillins) {
		for (GList * iter = fillins; iter; iter = iter->next) {
			free(iter->data);
		}
		g_list_free(fillins);
	}
	if (rects_to_download) {
		for (auto rect_iter = rects_to_download->begin(); rect_iter != rects_to_download->end(); rect_iter++) {
			free(*rect_iter);
		}
		delete rects_to_download;
	}
}




void trw_layer_download_map_along_track_cb(trw_menu_sublayer_t * data)
{
	VikLayer *vml;
	int selected_map;
	char *zoomlist[] = {(char *) "0.125", (char *) "0.25", (char *) "0.5", (char *) "1", (char *) "2", (char *) "4", (char *) "8", (char *) "16", (char *) "32", (char *) "64", (char *) "128", (char *) "256", (char *) "512", (char *) "1024", NULL };
	double zoom_vals[] = {0.125, 0.25, 0.5, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};
	int selected_zoom, default_zoom;

	LayerTRW * layer = data->layer;
	LayersPanel * panel = data->panel;
	Track * trk = layer->get_track_helper(data);

	if (!trk) {
		return;
	}

	Viewport * viewport = window_from_layer(layer)->get_viewport();

	std::list<Layer *> * vmls = panel->get_all_layers_of_type(LayerType::MAPS, true); // Includes hidden map layer types
	int num_maps = vmls->size();

	if (!num_maps) {
		a_dialog_error_msg(gtk_window_from_layer(layer), _("No map layer in use. Create one first"));
		return;
	}

	// Convert from list of vmls to list of names. Allowing the user to select one of them
	char **map_names = (char **) g_malloc_n(1 + num_maps, sizeof (char *));
	VikLayer **map_layers = (VikLayer **) g_malloc_n(1 + num_maps, sizeof(VikLayer *));

	char **np = map_names;
	VikLayer **lp = map_layers;
	for (auto i = vmls->begin(); i != vmls->end(); i++) {
		vml = (VikLayer *) *i;
		*lp++ = vml;
		LayerMaps * lm = (LayerMaps *) vml->layer;
		*np++ = lm->get_map_label();
	}
	// Mark end of the array lists
	*lp = NULL;
	*np = NULL;

	double cur_zoom = viewport->get_zoom();
	for (default_zoom = 0; default_zoom < G_N_ELEMENTS(zoom_vals); default_zoom++) {
		if (cur_zoom == zoom_vals[default_zoom]) {
			break;
		}
	}
	default_zoom = (default_zoom == G_N_ELEMENTS(zoom_vals)) ? G_N_ELEMENTS(zoom_vals) - 1 : default_zoom;

	if (!a_dialog_map_n_zoom(gtk_window_from_layer(layer), map_names, 0, zoomlist, default_zoom, &selected_map, &selected_zoom)) {
		goto done;
	}

	vik_track_download_map(trk, map_layers[selected_map], viewport, zoom_vals[selected_zoom]);

 done:
	for (int i = 0; i < num_maps; i++) {
		free(map_names[i]);
	}
	free(map_names);
	free(map_layers);

	delete vmls;

}




/**** lowest waypoint number calculation ***/
static int highest_wp_number_name_to_number(const char *name)
{
	if (strlen(name) == 3) {
		int n = atoi(name);
		if (n < 100 && name[0] != '0') {
			return -1;
		}

		if (n < 10 && name[0] != '0') {
			return -1;
		}
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
static std::list<track_layer_t *> * trw_layer_create_tracks_and_layers_list_both(Layer * layer, SublayerType sublayer_type)
{
	std::list<Track *> * tracks = new std::list<Track *>;
	tracks = LayerTRWc::get_track_values(tracks, ((LayerTRW *) layer)->get_tracks());
	tracks = LayerTRWc::get_track_values(tracks, ((LayerTRW *) layer)->get_routes());

	return ((LayerTRW *) layer)->create_tracks_and_layers_list_helper(tracks);
}




/**
 * trw_layer_create_track_list_both:
 *
 * Create the latest list of tracks and routes
 */
std::list<track_layer_t *> * LayerTRW::create_tracks_and_layers_list()
{
	std::list<Track *> * tracks = new std::list<Track *>;
	tracks = LayerTRWc::get_track_values(tracks, this->get_tracks());
	tracks = LayerTRWc::get_track_values(tracks, this->get_routes());

	return this->create_tracks_and_layers_list_helper(tracks);
}




void trw_layer_track_list_dialog_single(trw_menu_sublayer_t * data)
{
	LayerTRW * layer = data->layer;

	char *title = NULL;
	if (data->sublayer_type == SublayerType::TRACKS) {
		title = g_strdup_printf(_("%s: Track List"), layer->name);
	} else {
		title = g_strdup_printf(_("%s: Route List"), layer->name);
	}

	vik_trw_layer_track_list_show_dialog(title, layer, data->sublayer_type, false);
	free(title);
}




void trw_layer_track_list_dialog(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;

	char *title = g_strdup_printf(_("%s: Track and Route List"), layer->name);
	vik_trw_layer_track_list_show_dialog(title, layer, SublayerType::NONE, false);
	free(title);
}




void trw_layer_waypoint_list_dialog(trw_menu_layer_t * data)
{
	LayerTRW * layer = data->layer;

	char * title = g_strdup_printf(_("%s: Waypoint List"), layer->name);
	vik_trw_layer_waypoint_list_show_dialog(title, layer, false);
	free(title);
}




Track * LayerTRW::get_track_helper(trw_menu_sublayer_t * data)
{
	sg_uid_t uid = data->sublayer_uid;
	if (data->sublayer_type == SublayerType::ROUTE) {
		return this->routes.at(uid);
	} else {
		return this->tracks.at(uid);
	}
}




int LayerTRW::read_file(FILE * f, char const * dirpath)
{
	return (int) a_gpspoint_read_file(this, f, dirpath);
}




void LayerTRW::write_file(FILE * f) const
{
	fprintf(f, "\n\n~LayerData\n");
	a_gpspoint_write_file(this, f);
	fprintf(f, "~EndLayerData\n");
}




LayerTRW::LayerTRW() : Layer()
{
	this->type = LayerType::TRW;

	strcpy(this->type_string, "TRW");

	memset(&coord_mode, 0, sizeof (VikCoordMode));
}




LayerTRW::LayerTRW(Viewport * viewport) : Layer()
{
	this->type = LayerType::TRW;

	strcpy(this->type_string, "TRW");

	memset(&coord_mode, 0, sizeof (VikCoordMode));

	// It's not entirely clear the benefits of hash tables usage here - possibly the simplicity of first implementation for unique names
	// Now with the name of the item stored as part of the item - these tables are effectively straightforward lists

	// For this reworking I've choosen to keep the use of hash tables since for the expected data sizes
	// - even many hundreds of waypoints and tracks is quite small in the grand scheme of things,
	//  and with normal PC processing capabilities - it has negligibile performance impact
	// This also minimized the amount of rework - as the management of the hash tables already exists.

	// The hash tables are indexed by simple integers acting as a UUID hash, which again shouldn't affect performance much
	//   we have to maintain a uniqueness (as before when multiple names where not allowed),
	//   this is to ensure it refers to the same item in the data structures used on the viewport and on the layers panel

	//rv->waypoints = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) Waypoint::delete_waypoint);
	//rv->waypoints_iters = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
	//rv->tracks = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) Track::delete_track);
	//rv->tracks_iters = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
	//rv->routes = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) Track::delete_track);
	//rv->routes_iters = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);

	this->image_cache = g_queue_new(); // Must be performed before set_params via set_defaults

	this->set_defaults(viewport);

	// Param settings that are not available via the GUI
	// Force to on after processing params (which defaults them to off with a zero value)
	this->waypoints_visible = this->tracks_visible = this->routes_visible = true;

	this->metadata = new TRWMetadata();
	this->draw_sync_done = true;
	this->draw_sync_do = true;
	// Everything else is 0, false or NULL



	this->rename(vik_trw_layer_interface.name);

	if (viewport == NULL || gtk_widget_get_window(GTK_WIDGET(viewport->vvp)) == NULL) {
		;
	} else {

		this->wplabellayout = gtk_widget_create_pango_layout(GTK_WIDGET(viewport->vvp), NULL);
		pango_layout_set_font_description(this->wplabellayout, gtk_widget_get_style(GTK_WIDGET(viewport->vvp))->font_desc);

		this->tracklabellayout = gtk_widget_create_pango_layout(GTK_WIDGET(viewport->vvp), NULL);
		pango_layout_set_font_description(this->tracklabellayout, gtk_widget_get_style(GTK_WIDGET(viewport->vvp))->font_desc);

		this->new_track_gcs(viewport);

		this->waypoint_gc = viewport->new_gc_from_color(&(this->waypoint_color), 2);
		this->waypoint_text_gc = viewport->new_gc_from_color(&(this->waypoint_text_color), 1);
		this->waypoint_bg_gc = viewport->new_gc_from_color(&(this->waypoint_bg_color), 1);
		gdk_gc_set_function(this->waypoint_bg_gc, this->wpbgand);

		this->coord_mode = viewport->get_coord_mode();

		this->menu_selection = vik_layer_get_interface(this->type)->menu_items_selection;
	}
}
