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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <cassert>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "layer_trw.h"
#include "layer_trw_painter.h"
#include "layer_trw_tools.h"
#include "layer_trw_dialogs.h"
#include "layer_map.h"
#include "tree_view_internal.h"
#include "vikutils.h"
#include "track_properties_dialog.h"
#include "waypoint_properties.h"
#include "waypoint_list.h"
#include "track_internal.h"
#include "track_profile_dialog.h"
#include "track_list_dialog.h"
#include "file.h"
#include "dialog.h"
#include "dem.h"
//#include "dem_cache.h"
#include "background.h"
#include "util.h"
#include "settings.h"
#include "globals.h"
#include "ui_builder.h"
#include "layers_panel.h"
#include "preferences.h"
#include "util.h"
#include "generic_tools.h"


#include "layer_gps.h"
#include "layer_trw_stats.h"
#include "geonamessearch.h"
#ifdef VIK_CONFIG_GEOTAG
#include "layer_trw_geotag.h"
#include "geotag_exif.h"
#endif

#ifdef VIK_CONFIG_OPENSTREETMAP
#include "osm-traces.h"
#endif

#ifdef K
#include "garminsymbols.h"
#include "thumbnails.h"
#include "background.h"
#include "gpx.h"
#include "geojson.h"
#include "babel.h"


#include "acquire.h"
#include "datasources.h"
#include "datasource_gps.h"
#include "external_tools.h"
#include "vikexttool_datasources.h"
#include "ui_util.h"
#include "clipboard.h"
#include "routing.h"
#include "icons/icons.h"
#endif

#include "gpspoint.h"

#include "widget_list_selection.h"



#define POINTS 1
#define LINES 2

/* This is how it knows when you click if you are clicking close to a trackpoint. */
#define TRACKPOINT_SIZE_APPROX 5
#define WAYPOINT_SIZE_APPROX 5

#define MIN_STOP_LENGTH 15
#define MAX_STOP_LENGTH 86400




using namespace SlavGPS;



extern VikDataSourceInterface vik_datasource_gps_interface;
extern VikDataSourceInterface vik_datasource_file_interface;
extern VikDataSourceInterface vik_datasource_routing_interface;
#ifdef VIK_CONFIG_OPENSTREETMAP
extern VikDataSourceInterface vik_datasource_osm_interface;
extern VikDataSourceInterface vik_datasource_osm_my_traces_interface;
#endif
#ifdef VIK_CONFIG_GEOCACHES
extern VikDataSourceInterface vik_datasource_gc_interface;
#endif
#ifdef VIK_CONFIG_GEOTAG
extern VikDataSourceInterface vik_datasource_geotag_interface;
#endif
#ifdef VIK_CONFIG_GEONAMES
extern VikDataSourceInterface vik_datasource_wikipedia_interface;
#endif
extern VikDataSourceInterface vik_datasource_url_interface;
extern VikDataSourceInterface vik_datasource_geojson_interface;



static void goto_coord(LayersPanel * panel, Layer * layer, Viewport * viewport, const Coord & coord);

static void trw_layer_cancel_current_tp_cb(LayerTRW * layer, bool destroy);






/****** PARAMETERS ******/

static const char * g_params_groups[] = { "Waypoints", "Tracks", "Waypoint Images", "Tracks Advanced", "Metadata" };
enum { GROUP_WAYPOINTS, GROUP_TRACKS, GROUP_IMAGES, GROUP_TRACKS_ADV, GROUP_METADATA };


static std::vector<SGLabelID> params_track_drawing_modes = {
	SGLabelID("Draw by Track",                  DRAWMODE_BY_TRACK),
	SGLabelID("Draw by Speed",                  DRAWMODE_BY_SPEED),
	SGLabelID("All Tracks Have The Same Color", DRAWMODE_ALL_SAME_COLOR),
};

static std::vector<SGLabelID> params_wpsymbols = {
	SGLabelID("Filled Square", SYMBOL_FILLED_SQUARE),
	SGLabelID("Square",        SYMBOL_SQUARE),
	SGLabelID("Circle",        SYMBOL_CIRCLE),
	SGLabelID("X",             SYMBOL_X),
};

#define MIN_POINT_SIZE 2
#define MAX_POINT_SIZE 10

#define MIN_ARROW_SIZE 3
#define MAX_ARROW_SIZE 20

                                                      /*            min,              max,                     hardwired default,    step,   digits */
static ParameterScale scale_track_thickness         = {               1,               10,                SGVariant((int32_t) 1),       1,        0 }; /* PARAM_TRACK_THICKNESS */
static ParameterScale scale_track_draw_speed_factor = {               0,              100,                       SGVariant(30.0),       1,        0 }; /* PARAM_TRACK_DRAW_SPEED_FACTOR */
static ParameterScale scale_wp_image_size           = {              16,              128,               SGVariant((int32_t) 64),       4,        0 }; /* PARAM_WP_IMAGE_SIZE */
static ParameterScale scale_wp_image_alpha          = {               0,              255,              SGVariant((int32_t) 255),       5,        0 }; /* PARAM_WP_IMAGE_ALPHA */
static ParameterScale scale_wp_image_cache_size     = {               5,              500,              SGVariant((int32_t) 300),       5,        0 }; /* PARAM_WP_IMAGE_CACHE_SIZE */
static ParameterScale scale_track_bg_thickness      = {               0,                8,                SGVariant((int32_t) 0),       1,        0 }; /* PARAM_TRACK_BG_THICKNESS */
static ParameterScale scale_wp_marker_size          = {               1,               64,                SGVariant((int32_t) 4),       1,        0 }; /* PARAM_WP_MARKER_SIZE */
static ParameterScale scale_track_min_stop_length   = { MIN_STOP_LENGTH,  MAX_STOP_LENGTH,               SGVariant((int32_t) 60),       1,        0 }; /* PARAM_TRACK_MIN_STOP_LENGTH */
static ParameterScale scale_track_elevation_factor  = {               1,              100,               SGVariant((int32_t) 30),       1,        0 }; /* PARAM_TRACK_ELEVATION_FACTOR */
static ParameterScale scale_trackpoint_size         = {  MIN_POINT_SIZE,   MAX_POINT_SIZE,   SGVariant((int32_t) MIN_POINT_SIZE),       1,        0 }; /* PARAM_TRACKPOINT_SIZE */
static ParameterScale scale_track_direction_size    = {  MIN_ARROW_SIZE,   MAX_ARROW_SIZE,                SGVariant((int32_t) 5),       1,        0 }; /* PARAM_TRACK_DIRECTION_SIZE */




static std::vector<SGLabelID> params_font_sizes = {
	SGLabelID("Extra Extra Small",   FS_XX_SMALL),
	SGLabelID("Extra Small",         FS_X_SMALL),
	SGLabelID("Small",               FS_SMALL),
	SGLabelID("Medium",              FS_MEDIUM),
	SGLabelID("Large",               FS_LARGE),
	SGLabelID("Extra Large",         FS_X_LARGE),
 	SGLabelID("Extra Extra Large",   FS_XX_LARGE),
};



static std::vector<SGLabelID> params_sort_order = {
	SGLabelID("None",            VL_SO_NONE),
	SGLabelID("Name Ascending",  VL_SO_ALPHABETICAL_ASCENDING),
	SGLabelID("Name Descending", VL_SO_ALPHABETICAL_DESCENDING),
	SGLabelID("Date Ascending",  VL_SO_DATE_ASCENDING),
	SGLabelID("Date Descending", VL_SO_DATE_DESCENDING),
};

static SGVariant black_color_default(void)       { return SGVariant(0, 0, 0, 100); } /* Black. */
static SGVariant track_drawing_mode_default(void){ return SGVariant((int32_t) DRAWMODE_BY_TRACK); }
static SGVariant trackbgcolor_default(void)      { return SGVariant(255, 255, 255, 100); }  /* White. */
static SGVariant tnfontsize_default(void)        { return SGVariant((int32_t) FS_MEDIUM); }
static SGVariant wpfontsize_default(void)        { return SGVariant((int32_t) FS_MEDIUM); }
static SGVariant wptextcolor_default(void)       { return SGVariant(255, 255, 255, 100); } /* White. */
static SGVariant wpbgcolor_default(void)         { return SGVariant(0x83, 0x83, 0xc4, 100); } /* Kind of Blue. kamilTODO: verify the hex values. */
static SGVariant wpsymbol_default(void)          { return SGVariant((int32_t) SYMBOL_FILLED_SQUARE); }
static SGVariant sort_order_default(void)        { return SGVariant((int32_t) 0); }
static SGVariant string_default(void)            { return SGVariant(""); }




/* ENUMERATION MUST BE IN THE SAME ORDER AS THE NAMED PARAMS ABOVE. */
enum {
	// Sublayer visibilities
	PARAM_TRACKS_VISIBLE,
	PARAM_WAYPOINTS_VISIBLE,
	PARAM_ROUTES_VISIBLE,
	// Tracks
	PARAM_DRAW_TRACK_LABELS,
	PARAM_TRACK_LABEL_FONT_SIZE,
	PARAM_TRACK_DRAWING_MODE,
	PARAM_TRACK_COLOR_COMMON,
	PARAM_DRAW_TRACK_LINES,
	PARAM_TRACK_THICKNESS,
	PARAM_DD,
	PARAM_TRACK_DIRECTION_SIZE,
	PARAM_DRAW_TRACKPOINTS,
	PARAM_TRACKPOINT_SIZE,
	PARAM_DE,
	PARAM_TRACK_ELEVATION_FACTOR,
	PARAM_DRAW_TRACK_STOPS,
	PARAM_TRACK_MIN_STOP_LENGTH,
	PARAM_TRACK_BG_THICKNESS,
	PARAM_TRK_BG_COLOR,
	PARAM_TRACK_DRAW_SPEED_FACTOR,
	PARAM_TRACK_SORT_ORDER,
	// Waypoints
	PARAM_DLA,
	PARAM_WP_LABEL_FONT_SIZE,
	PARAM_WP_MARKER_COLOR,
	PARAM_WP_LABEL_FG_COLOR,
	PARAM_WP_LABEL_BG_COLOR,
	PARAM_WPBA,
	PARAM_WP_MARKER_TYPE,
	PARAM_WP_MARKER_SIZE,
	PARAM_WPSYMS,
	PARAM_WP_SORT_ORDER,
	// WP images
	PARAM_DI,
	PARAM_WP_IMAGE_SIZE,
	PARAM_WP_IMAGE_ALPHA,
	PARAM_WP_IMAGE_CACHE_SIZE,
	// Metadata
	PARAM_MDDESC,
	PARAM_MDAUTH,
	PARAM_MDTIME,
	PARAM_MDKEYS,
	NUM_PARAMS
};


Parameter trw_layer_params[] = {
	{ PARAM_TRACKS_VISIBLE,        "tracks_visible",    SGVariantType::BOOLEAN, PARAMETER_GROUP_HIDDEN,  "Tracks are visible",                   WidgetType::CHECKBUTTON,  NULL,                        sg_variant_true,            NULL, NULL },
	{ PARAM_WAYPOINTS_VISIBLE,     "waypoints_visible", SGVariantType::BOOLEAN, PARAMETER_GROUP_HIDDEN,  "Waypoints are visible",                WidgetType::CHECKBUTTON,  NULL,                        sg_variant_true,            NULL, NULL },
	{ PARAM_ROUTES_VISIBLE,        "routes_visible",    SGVariantType::BOOLEAN, PARAMETER_GROUP_HIDDEN,  "Routes are visible",                   WidgetType::CHECKBUTTON,  NULL,                        sg_variant_true,            NULL, NULL },

	{ PARAM_DRAW_TRACK_LABELS,     "trackdrawlabels",   SGVariantType::BOOLEAN, GROUP_TRACKS,            N_("Draw Labels"),                      WidgetType::CHECKBUTTON,  NULL,                        sg_variant_true,            NULL, N_("Note: the individual track controls what labels may be displayed") },
	{ PARAM_TRACK_LABEL_FONT_SIZE, "trackfontsize",     SGVariantType::INT,     GROUP_TRACKS_ADV,        N_("Size of Track Label's Font:"),      WidgetType::COMBOBOX,     &params_font_sizes,          tnfontsize_default,         NULL, NULL },
	{ PARAM_TRACK_DRAWING_MODE,    "drawmode",          SGVariantType::INT,     GROUP_TRACKS,            N_("Track Drawing Mode:"),              WidgetType::COMBOBOX,     &params_track_drawing_modes, track_drawing_mode_default, NULL, NULL },
	{ PARAM_TRACK_COLOR_COMMON,    "trackcolor",        SGVariantType::COLOR,   GROUP_TRACKS,            N_("All Tracks Color:"),                WidgetType::COLOR,        NULL,                        black_color_default,        NULL, N_("The color used when 'All Tracks Same Color' drawing mode is selected") },
	{ PARAM_DRAW_TRACK_LINES,      "drawlines",         SGVariantType::BOOLEAN, GROUP_TRACKS,            N_("Draw Track Lines"),                 WidgetType::CHECKBUTTON,  NULL,                        sg_variant_true,            NULL, NULL },
	{ PARAM_TRACK_THICKNESS,       "line_thickness",    SGVariantType::INT,     GROUP_TRACKS_ADV,        N_("Track Thickness:"),                 WidgetType::SPINBOX_INT,  &scale_track_thickness,      NULL,                       NULL, NULL },
	{ PARAM_DD,                    "drawdirections",    SGVariantType::BOOLEAN, GROUP_TRACKS,            N_("Draw Track Direction"),             WidgetType::CHECKBUTTON,  NULL,                        sg_variant_false,           NULL, NULL },
	{ PARAM_TRACK_DIRECTION_SIZE,  "trkdirectionsize",  SGVariantType::INT,     GROUP_TRACKS_ADV,        N_("Direction Size:"),                  WidgetType::SPINBOX_INT,  &scale_track_direction_size, NULL,                       NULL, NULL },
	{ PARAM_DRAW_TRACKPOINTS,      "drawpoints",        SGVariantType::BOOLEAN, GROUP_TRACKS,            N_("Draw Trackpoints:"),                WidgetType::CHECKBUTTON,  NULL,                        sg_variant_true,            NULL, NULL },
	{ PARAM_TRACKPOINT_SIZE,       "trkpointsize",      SGVariantType::INT,     GROUP_TRACKS_ADV,        N_("Trackpoint Size:"),                 WidgetType::SPINBOX_INT,  &scale_trackpoint_size,      NULL,                       NULL, NULL },
	{ PARAM_DE,                    "drawelevation",     SGVariantType::BOOLEAN, GROUP_TRACKS,            N_("Draw Elevation"),                   WidgetType::CHECKBUTTON,  NULL,                        sg_variant_false,           NULL, NULL },
	{ PARAM_TRACK_ELEVATION_FACTOR,"elevation_factor",  SGVariantType::INT,     GROUP_TRACKS_ADV,        N_("Draw Elevation Height %:"),         WidgetType::HSCALE,       &scale_track_elevation_factor, NULL,                     NULL, NULL },
	{ PARAM_DRAW_TRACK_STOPS,      "drawstops",         SGVariantType::BOOLEAN, GROUP_TRACKS,            N_("Draw Track Stops:"),                WidgetType::CHECKBUTTON,  NULL,                        sg_variant_false,           NULL, N_("Whether to draw a marker when trackpoints are at the same position but over the minimum stop length apart in time") },
	{ PARAM_TRACK_MIN_STOP_LENGTH, "stop_length",       SGVariantType::INT,     GROUP_TRACKS_ADV,        N_("Min Stop Length (seconds):"),       WidgetType::SPINBOX_INT,  &scale_track_min_stop_length,NULL,                       NULL, NULL },

	{ PARAM_TRACK_BG_THICKNESS,    "bg_line_thickness", SGVariantType::INT,     GROUP_TRACKS_ADV,        N_("Track Background Thickness:"),      WidgetType::SPINBOX_INT,  &scale_track_bg_thickness,   NULL,                       NULL, NULL },
	{ PARAM_TRK_BG_COLOR,          "trackbgcolor",      SGVariantType::COLOR,   GROUP_TRACKS_ADV,        N_("Track Background Color"),           WidgetType::COLOR,        NULL,                        trackbgcolor_default,       NULL, NULL },
	{ PARAM_TRACK_DRAW_SPEED_FACTOR, "speed_factor",    SGVariantType::DOUBLE,  GROUP_TRACKS_ADV,        N_("Draw by Speed Factor (%):"),        WidgetType::HSCALE,       &scale_track_draw_speed_factor, NULL,                    NULL, N_("The percentage factor away from the average speed determining the color used") },
	{ PARAM_TRACK_SORT_ORDER,      "tracksortorder",    SGVariantType::INT,     GROUP_TRACKS_ADV,        N_("Track Sort Order:"),                WidgetType::COMBOBOX,     &params_sort_order,          sort_order_default,         NULL, NULL },

	{ PARAM_DLA,                   "drawlabels",        SGVariantType::BOOLEAN, GROUP_WAYPOINTS,         N_("Draw Labels"),                      WidgetType::CHECKBUTTON,  NULL,                        sg_variant_true,            NULL, NULL },
	{ PARAM_WP_LABEL_FONT_SIZE,    "wpfontsize",        SGVariantType::INT,     GROUP_WAYPOINTS,         N_("Font Size of Waypoint's Label:"),   WidgetType::COMBOBOX,     &params_font_sizes,          wpfontsize_default,         NULL, NULL },
	{ PARAM_WP_MARKER_COLOR,       "wpcolor",           SGVariantType::COLOR,   GROUP_WAYPOINTS,         N_("Color of Waypoint's Marker:"),      WidgetType::COLOR,        NULL,                        black_color_default,        NULL, NULL },
	{ PARAM_WP_LABEL_FG_COLOR,     "wptextcolor",       SGVariantType::COLOR,   GROUP_WAYPOINTS,         N_("Color of Waypoint's Label:"),       WidgetType::COLOR,        NULL,                        wptextcolor_default,        NULL, NULL },
	{ PARAM_WP_LABEL_BG_COLOR,     "wpbgcolor",         SGVariantType::COLOR,   GROUP_WAYPOINTS,         N_("Background of Waypoint's Label:"),  WidgetType::COLOR,        NULL,                        wpbgcolor_default,          NULL, NULL },
	{ PARAM_WPBA,                  "wpbgand",           SGVariantType::BOOLEAN, GROUP_WAYPOINTS,         N_("Fake BG Color Translucency:"),      WidgetType::CHECKBUTTON,  NULL,                        sg_variant_false,           NULL, NULL },
	{ PARAM_WP_MARKER_TYPE,        "wpsymbol",          SGVariantType::INT,     GROUP_WAYPOINTS,         N_("Type of Waypoint's Marker:"),       WidgetType::COMBOBOX,     &params_wpsymbols,           wpsymbol_default,           NULL, NULL },
	{ PARAM_WP_MARKER_SIZE,        "wpsize",            SGVariantType::INT,     GROUP_WAYPOINTS,         N_("Size of Waypoint's Marker:"),       WidgetType::SPINBOX_INT,  &scale_wp_marker_size,       NULL,                       NULL, NULL },
	{ PARAM_WPSYMS,                "wpsyms",            SGVariantType::BOOLEAN, GROUP_WAYPOINTS,         N_("Draw Waypoint Symbols:"),           WidgetType::CHECKBUTTON,  NULL,                        sg_variant_true,            NULL, NULL },
	{ PARAM_WP_SORT_ORDER,         "wpsortorder",       SGVariantType::INT,     GROUP_WAYPOINTS,         N_("Waypoint Sort Order:"),             WidgetType::COMBOBOX,     &params_sort_order,          sort_order_default,         NULL, NULL },

	{ PARAM_DI,                    "drawimages",        SGVariantType::BOOLEAN, GROUP_IMAGES,            N_("Draw Waypoint Images"),             WidgetType::CHECKBUTTON,  NULL,                        sg_variant_true,            NULL, NULL },
	{ PARAM_WP_IMAGE_SIZE,         "image_size",        SGVariantType::INT,     GROUP_IMAGES,            N_("Image Size (pixels):"),             WidgetType::HSCALE,       &scale_wp_image_size,        NULL,                       NULL, NULL },
	{ PARAM_WP_IMAGE_ALPHA,        "image_alpha",       SGVariantType::INT,     GROUP_IMAGES,            N_("Image Alpha:"),                     WidgetType::HSCALE,       &scale_wp_image_alpha,       NULL,                       NULL, NULL },
	{ PARAM_WP_IMAGE_CACHE_SIZE,   "image_cache_size",  SGVariantType::INT,     GROUP_IMAGES,            N_("Image Memory Cache Size:"),         WidgetType::HSCALE,       &scale_wp_image_cache_size,  NULL,                       NULL, NULL },

	{ PARAM_MDDESC,                "metadatadesc",      SGVariantType::STRING,  GROUP_METADATA,          N_("Description"),                      WidgetType::ENTRY,        NULL,                        string_default,             NULL, NULL },
	{ PARAM_MDAUTH,                "metadataauthor",    SGVariantType::STRING,  GROUP_METADATA,          N_("Author"),                           WidgetType::ENTRY,        NULL,                        string_default,             NULL, NULL },
	{ PARAM_MDTIME,                "metadatatime",      SGVariantType::STRING,  GROUP_METADATA,          N_("Creation Time"),                    WidgetType::ENTRY,        NULL,                        string_default,             NULL, NULL },
	{ PARAM_MDKEYS,                "metadatakeywords",  SGVariantType::STRING,  GROUP_METADATA,          N_("Keywords"),                         WidgetType::ENTRY,        NULL,                        string_default,             NULL, NULL },

	{ NUM_PARAMS,                  NULL,                SGVariantType::EMPTY,   PARAMETER_GROUP_GENERIC, NULL,                                   WidgetType::NONE,         NULL,                        NULL,                       NULL, NULL }, /* Guard. */
};


/*** TO ADD A PARAM:
 *** 1) Add to trw_layer_params and enumeration
 *** 2) Handle in get_param & set_param (presumably adding on to LayerTRW class)
 ***/

/****** END PARAMETERS ******/




LayerTRWInterface vik_trw_layer_interface;




LayerTRWInterface::LayerTRWInterface()
{
	this->parameters_c = trw_layer_params;
	this->parameter_groups = g_params_groups;

	this->fixed_layer_type_string = "TrackWaypoint"; /* Non-translatable. */

	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_Y;
	// this->action_icon = ...; /* Set elsewhere. */

	this->menu_items_selection = LayerMenuItem::ALL;

	this->ui_labels.new_layer = QObject::tr("New Track/Route/Waypoint Layer");
	this->ui_labels.layer_type = QObject::tr("TrackWaypoint");
	this->ui_labels.layer_defaults = QObject::tr("Default Settings of Track/Route/Waypoint Layer");
}




LayerToolContainer * LayerTRWInterface::create_tools(Window * window, Viewport * viewport)
{
	/* This method should be called only once. */
	static bool created = false;
	if (created) {
		return NULL;
	}

	auto tools = new LayerToolContainer;

	LayerTool * tool = NULL;

	tool = new LayerToolTRWNewWaypoint(window, viewport);
	tools->insert({{ tool->id_string, tool }});

	tool = new LayerToolTRWNewTrack(window, viewport);
	tools->insert({{ tool->id_string, tool }});

	tool = new LayerToolTRWNewRoute(window, viewport);
	tools->insert({{ tool->id_string, tool }});

	tool = new LayerToolTRWExtendedRouteFinder(window, viewport);
	tools->insert({{ tool->id_string, tool }});

	tool = new LayerToolTRWEditWaypoint(window, viewport);
	tools->insert({{ tool->id_string, tool }});

	tool = new LayerToolTRWEditTrackpoint(window, viewport);
	tools->insert({{ tool->id_string, tool }});

	tool = new LayerToolTRWShowPicture(window, viewport);
	tools->insert({{ tool->id_string, tool }});

	created = true;

	return tools;
}




bool g_have_diary_program = false;
char *diary_program = NULL;
#define VIK_SETTINGS_EXTERNAL_DIARY_PROGRAM "external_diary_program"

bool have_geojson_export = false;

bool g_have_astro_program = false;
char *astro_program = NULL;
#define VIK_SETTINGS_EXTERNAL_ASTRO_PROGRAM "external_astro_program"




void SlavGPS::layer_trw_init(void)
{
	if (!a_settings_get_string(VIK_SETTINGS_EXTERNAL_DIARY_PROGRAM, &diary_program)) {
#ifdef WINDOWS
		//diary_program = strdup("C:\\Program Files\\Rednotebook\\rednotebook.exe");
		diary_program = strdup("C:/Progra~1/Rednotebook/rednotebook.exe");
#else
		diary_program = strdup("rednotebook");
#endif
	} else {
		/* User specified so assume it works. */
		g_have_diary_program = true;
	}

	if (g_find_program_in_path(diary_program)) {
		char *mystdout = NULL;
		char *mystderr = NULL;
		/* Needs RedNotebook 1.7.3+ for support of opening on a specified date. */
		char *cmd = g_strconcat(diary_program, " --version", NULL); // "rednotebook --version"
		if (g_spawn_command_line_sync(cmd, &mystdout, &mystderr, NULL, NULL)) {
			/* Annoyingly 1.7.1|2|3 versions of RedNotebook prints the version to stderr!! */
			if (mystdout) {
				fprintf(stderr, "DEBUG: Diary: %s\n", mystdout); /* Should be something like 'RedNotebook 1.4'. */
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
							g_have_diary_program = true;
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
#ifdef K
	if (g_find_program_in_path(geojson_program_export())) {
		have_geojson_export = true;
	}
#endif

	/* Astronomy Domain. */
	if (!a_settings_get_string(VIK_SETTINGS_EXTERNAL_ASTRO_PROGRAM, &astro_program)) {
#ifdef WINDOWS
		//astro_program = strdup("C:\\Program Files\\Stellarium\\stellarium.exe");
		astro_program = strdup("C:/Progra~1/Stellarium/stellarium.exe");
#else
		astro_program = strdup("stellarium");
#endif
	} else {
		/* User specified so assume it works. */
		g_have_astro_program = true;
	}
	if (g_find_program_in_path(astro_program)) {
		g_have_astro_program = true;
	}
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




void LayerTRW::set_metadata(TRWMetadata * meta_data)
{
	if (this->metadata) {
		LayerTRW::metadata_free(this->metadata);
	}
	this->metadata = meta_data;
}




void TRWMetadata::set_author(const QString & new_author)
{
	this->author = new_author;
}




void TRWMetadata::set_description(const QString & new_description)
{
	this->description = new_description;
}




void TRWMetadata::set_keywords(const QString & new_keywords)
{
	this->keywords = new_keywords;
}




void TRWMetadata::set_timestamp(const QString & new_timestamp)
{
	this->timestamp = new_timestamp;
	this->has_timestamp = true;
}




/**
 * Find a track by date.
 */
bool LayerTRW::find_track_by_date(char const * date_str, Viewport * viewport, bool select)
{
	Track * trk = this->tracks.find_track_by_date(date_str);
	if (trk && select) {
		struct LatLon maxmin[2] = { {0,0}, {0,0} };
		trk->find_maxmin(maxmin);
		this->zoom_to_show_latlons(viewport, maxmin);
		this->tree_view->select_and_expose(trk->index);
		this->emit_changed();
	}
	return (bool) trk;
}


/**
 * Find a waypoint by date.
 */
bool LayerTRW::find_waypoint_by_date(char const * date_str, Viewport * viewport, bool select)
{
	Waypoint * wp = this->waypoints.find_waypoint_by_date(date_str);
	if (wp && select) {
		viewport->set_center_coord(wp->coord, true);
		this->tree_view->select_and_expose(wp->index);
		this->emit_changed();
	}
	return (bool) wp;
}




void LayerTRW::delete_sublayer(TreeItem * sublayer)
{
	if (!sublayer) {
		qDebug() << "WW: Layer TRW: 'delete sublayer' received NULL sublayer";
		return;
	}

	static trw_menu_sublayer_t data;
	memset(&data, 0, sizeof (trw_menu_sublayer_t));

	data.sublayer = sublayer;
	data.confirm  = true;  /* Confirm delete request. */
#ifdef K
	this->delete_sublayer_cb(&data);
#endif
}




void LayerTRW::cut_sublayer(TreeItem * sublayer)
{
	if (!sublayer) {
		qDebug() << "WW: Layer TRW: 'cut sublayer' received NULL sublayer";
		return;
	}

	static trw_menu_sublayer_t data;
	memset(&data, 0, sizeof (trw_menu_sublayer_t));

	data.sublayer = sublayer;
	data.confirm  = true; /* Confirm delete request. */
#ifdef K
	this->copy_sublayer_cb(&data);
	this->cut_sublayer_cb(&data);
#endif
}




void LayerTRW::copy_sublayer_cb(void)
{
	TreeItem * sublayer = this->menu_data->sublayer;
	uint8_t *data_ = NULL;
	unsigned int len;

	this->copy_sublayer(sublayer, &data_, &len);

#ifdef K
	if (data_) {
		const char* name = NULL;
		if (sublayer->type == "sg.trw.waypoint") {
			Waypoint * wp = this->waypoints.items.at(sublayer->uid);
			if (wp && wp->name) {
				name = wp->name;
			} else {
				name = NULL; // Broken :(
			}
		} else if (sublayer->type == "sg.trw.track") {
			Track * trk = this->tracks.at(sublayer->uid);
			if (trk && trk->name) {
				name = trk->name;
			} else {
				name = NULL; // Broken :(
			}
		} else {
			Track * trk = this->routes.at(sublayer->uid);
			if (trk && trk->name) {
				name = trk->name;
			} else {
				name = NULL; // Broken :(
			}
		}

#ifdef K
		a_clipboard_copy(VIK_CLIPBOARD_DATA_SUBLAYER, LayerType::TRW,
				 sublayer->type, len, name, data_);
#endif
	}
#endif
}




void LayerTRW::cut_sublayer_cb(void) /* Slot. */
{
#ifdef K
	this->copy_sublayer_cb(data);
	data->confirm = false; /* Never need to confirm automatic delete. */
	this->delete_sublayer_cb(data);
#endif
}




void LayerTRW::paste_sublayer_cb(void)
{
	/* Slightly cheating method, routing via the panels capability. */
#ifdef K
	a_clipboard_paste(this->menu_data->layers_panel);
#endif
}




void LayerTRW::copy_sublayer(TreeItem * sublayer, uint8_t **item, unsigned int *len)
{
	if (!sublayer) {
		qDebug() << "WW: Layer TRW: 'copy sublayer' received NULL sublayer";
		*item = NULL;
		return;
	}

	uint8_t *id;
	size_t il;

	GByteArray *ba = g_byte_array_new();

	if (sublayer->type_id == "sg.trw.waypoint") {
		this->waypoints.items.at(sublayer->uid)->marshall(&id, &il);
	} else if (sublayer->type_id == "sg.trw.track") {
		this->tracks.items.at(sublayer->uid)->marshall(&id, &il);
	} else {
		this->routes.items.at(sublayer->uid)->marshall(&id, &il);
	}

	g_byte_array_append(ba, id, il);

	std::free(id);

	*len = ba->len;
	*item = ba->data;
}




bool LayerTRW::paste_sublayer(TreeItem * sublayer, uint8_t * item, size_t len)
{
	if (!sublayer) {
		qDebug() << "WW: Layer TRW: 'paste sublayer' received NULL sublayer";
		return false;
	}

	if (!item) {
		return false;
	}

	if (sublayer->type_id == "sg.trw.waypoint") {
		Waypoint * wp = Waypoint::unmarshall(item, len);
		/* When copying - we'll create a new name based on the original. */
		const QString uniq_name = this->new_unique_element_name("sg.trw.waypoint", wp->name);
		wp->set_name(uniq_name);

		this->add_waypoint(wp);

		wp->convert(this->coord_mode);
		this->waypoints.calculate_bounds();

		/* Consider if redraw necessary for the new item. */
		if (this->visible && this->waypoints.visible && wp->visible) {
			this->emit_changed();
		}
		return true;
	}
	if (sublayer->type_id == "sg.trw.track") {
		Track * trk = Track::unmarshall(item, len);

		/* When copying - we'll create a new name based on the original. */
		const QString uniq_name = this->new_unique_element_name("sg.trw.track", trk->name);
		trk->set_name(uniq_name);

		this->add_track(trk);

		trk->convert(this->coord_mode);

		/* Consider if redraw necessary for the new item. */
		if (this->visible && this->tracks.visible && trk->visible) {
			this->emit_changed();
		}
		return true;
	}
	if (sublayer->type_id == "sg.trw.route") {
		Track * trk = Track::unmarshall(item, len);
		/* When copying - we'll create a new name based on the original. */
		const QString uniq_name = this->new_unique_element_name("sg.trw.route", trk->name);
		trk->set_name(uniq_name);

		this->add_route(trk);
		trk->convert(this->coord_mode);

		/* Consider if redraw necessary for the new item. */
		if (this->visible && this->routes.visible && trk->visible) {
			this->emit_changed();
		}
		return true;
	}
	return false;
}




void LayerTRW::image_cache_free()
{
#ifdef K
	g_list_foreach(this->image_cache->head, (GFunc) cached_pixmap_free, NULL);
	g_queue_free(this->image_cache);
#endif
}




bool LayerTRW::set_param_value(uint16_t id, const SGVariant & data, bool is_file_operation)
{
	switch (id) {
	case PARAM_TRACKS_VISIBLE:
		this->tracks.visible = data.b;
		break;
	case PARAM_WAYPOINTS_VISIBLE:
		this->waypoints.visible = data.b;
		break;
	case PARAM_ROUTES_VISIBLE:
		this->routes.visible = data.b;
		break;
	case PARAM_DRAW_TRACK_LABELS:
		this->track_draw_labels = data.b;
		break;
	case PARAM_TRACK_LABEL_FONT_SIZE:
		if (data.i < FS_NUM_SIZES) {
			this->trk_label_font_size = (font_size_t) data.i;
		}
		break;
	case PARAM_TRACK_DRAWING_MODE:
		this->track_drawing_mode = data.i;
		break;
	case PARAM_TRACK_COLOR_COMMON:
		this->track_color_common = QColor(data.c.r, data.c.g, data.c.b, data.c.a);
		this->new_track_pens();
		break;
	case PARAM_DRAW_TRACKPOINTS:
		this->draw_trackpoints = data.b;
		break;
	case PARAM_TRACKPOINT_SIZE:
		if (data.i >= scale_trackpoint_size.min && data.i <= scale_trackpoint_size.max) {
			this->trackpoint_size = data.i;
		}
		break;
	case PARAM_DE:
		this->drawelevation = data.b;
		break;
	case PARAM_DRAW_TRACK_STOPS:
		this->draw_track_stops = data.b;
		break;
	case PARAM_DRAW_TRACK_LINES:
		this->draw_track_lines = data.b;
		break;
	case PARAM_DD:
		this->drawdirections = data.b;
		break;
	case PARAM_TRACK_DIRECTION_SIZE:
		if (data.i >= scale_track_direction_size.min && data.i <= scale_track_direction_size.max) {
			this->drawdirections_size = data.i;
		}
		break;
	case PARAM_TRACK_MIN_STOP_LENGTH:
		if (data.i >= scale_track_min_stop_length.min && data.i <= scale_track_min_stop_length.max) {
			this->stop_length = data.i;
		}
		break;
	case PARAM_TRACK_ELEVATION_FACTOR:
		if (data.i >= scale_track_elevation_factor.min && data.i <= scale_track_elevation_factor.max) {
			this->elevation_factor = data.i;
		}
		break;
	case PARAM_TRACK_THICKNESS:
		if (data.i >= scale_track_thickness.min && data.i <= scale_track_thickness.max) {
			if (data.i != this->track_thickness) {
				this->track_thickness = data.i;
				this->new_track_pens();
			}
		}
		break;
	case PARAM_TRACK_BG_THICKNESS:
		if (data.i >= scale_track_bg_thickness.min && data.i <= scale_track_bg_thickness.max) {
			if (data.i != this->track_bg_thickness) {
				this->track_bg_thickness = data.i;
				this->new_track_pens();
			}
		}
		break;

	case PARAM_TRK_BG_COLOR:
		data.to_qcolor(this->track_bg_color);
		this->track_bg_pen.setColor(this->track_bg_color);
		break;

	case PARAM_TRACK_DRAW_SPEED_FACTOR:
		if (data.d >= scale_track_draw_speed_factor.min && data.d <= scale_track_draw_speed_factor.max) {
			this->track_draw_speed_factor = data.d;
		}
		break;
	case PARAM_TRACK_SORT_ORDER:
		if (data.i < VL_SO_LAST) {
			this->track_sort_order = (sort_order_t) data.i;
		}
		break;
	case PARAM_DLA:
		this->drawlabels = data.b;
		break;
	case PARAM_DI:
		this->drawimages = data.b;
		break;
	case PARAM_WP_IMAGE_SIZE:
		if (data.i >= scale_wp_image_size.min && data.i <= scale_wp_image_size.max) {
			if (data.i != this->wp_image_size) {
				this->wp_image_size = data.i;
				this->image_cache_free();
				this->image_cache = g_queue_new();
			}
		}
		break;
	case PARAM_WP_IMAGE_ALPHA:
		if (data.i >= scale_wp_image_alpha.min && data.i <= scale_wp_image_alpha.max) {
			if (data.i != this->wp_image_alpha) {
				this->wp_image_alpha = data.i;
				this->image_cache_free();
				this->image_cache = g_queue_new();
			}
		}
		break;

	case PARAM_WP_IMAGE_CACHE_SIZE:
		if (data.i >= scale_wp_image_cache_size.min && data.i <= scale_wp_image_cache_size.max) {
			this->wp_image_cache_size = data.i;
			while (this->image_cache->length > this->wp_image_cache_size) { /* If shrinking cache_size, free pixbuf ASAP. */
				CachedPixmap * pixmap = (CachedPixmap *) g_queue_pop_tail(this->image_cache);
				delete pixmap;
			}
		}
		break;

	case PARAM_WP_MARKER_COLOR:
		data.to_qcolor(this->wp_marker_color);
		this->wp_marker_pen.setColor(this->wp_marker_color);
		break;

	case PARAM_WP_LABEL_FG_COLOR:
		data.to_qcolor(this->wp_label_fg_color);
		this->wp_label_fg_pen.setColor(this->wp_label_fg_color);
		break;

	case PARAM_WP_LABEL_BG_COLOR:
		data.to_qcolor(this->wp_label_bg_color);
		this->wp_label_bg_pen.setColor(this->wp_label_bg_color);
		break;

	case PARAM_WPBA:
#ifdef K
		this->wpbgand = (GdkFunction) data.b;
		if (this->waypoint_bg_gc) {
			gdk_gc_set_function(this->waypoint_bg_gc, data.b ? GDK_AND : GDK_COPY);
		}
#endif
		break;
	case PARAM_WP_MARKER_TYPE:
		if (data.i < SYMBOL_NUM_SYMBOLS) {
			this->wp_marker_type = data.i;
		}
		break;
	case PARAM_WP_MARKER_SIZE:
		if (data.i >= scale_wp_marker_size.min && data.i <= scale_wp_marker_size.max) {
			this->wp_marker_size = data.i;
		}
		break;
	case PARAM_WPSYMS:
		this->wp_draw_symbols = data.b;
		break;
	case PARAM_WP_LABEL_FONT_SIZE:
		if (data.i < FS_NUM_SIZES) {
			this->wp_label_font_size = (font_size_t) data.i;
		}
		break;
	case PARAM_WP_SORT_ORDER:
		if (data.i < VL_SO_LAST) {
			this->wp_sort_order = (sort_order_t) data.i;
		}
		break;
		// Metadata
	case PARAM_MDDESC:
		if (!data.s.isEmpty() && this->metadata) {
			this->metadata->set_description(data.s);
		}
		break;
	case PARAM_MDAUTH:
		if (!data.s.isEmpty() && this->metadata) {
			this->metadata->set_author(data.s);
		}
		break;
	case PARAM_MDTIME:
		if (!data.s.isEmpty() && this->metadata) {
			this->metadata->set_timestamp(data.s);
		}
		break;
	case PARAM_MDKEYS:
		if (!data.s.isEmpty() && this->metadata) {
			this->metadata->set_keywords(data.s);
		}
		break;
	default: break;
	}
	return true;
}




SGVariant LayerTRW::get_param_value(param_id_t id, bool is_file_operation) const
{
	SGVariant rv;
	switch (id) {
	case PARAM_TRACKS_VISIBLE:    rv.b = this->tracks.visible; break;
	case PARAM_WAYPOINTS_VISIBLE: rv.b = this->waypoints.visible; break;
	case PARAM_ROUTES_VISIBLE:    rv.b = this->routes.visible; break;
	case PARAM_DRAW_TRACK_LABELS: rv.b = this->track_draw_labels; break;
	case PARAM_TRACK_LABEL_FONT_SIZE: rv.i = this->trk_label_font_size; break;
	case PARAM_TRACK_DRAWING_MODE: rv.i = this->track_drawing_mode; break;
	case PARAM_TRACK_COLOR_COMMON: rv = SGVariant(this->track_color_common); break;
	case PARAM_DRAW_TRACKPOINTS: rv.b = this->draw_trackpoints; break;
	case PARAM_TRACKPOINT_SIZE: rv.i = this->trackpoint_size; break;
	case PARAM_DE: rv.b = this->drawelevation; break;
	case PARAM_TRACK_ELEVATION_FACTOR: rv.i = this->elevation_factor; break;
	case PARAM_DRAW_TRACK_STOPS: rv.b = this->draw_track_stops; break;
	case PARAM_TRACK_MIN_STOP_LENGTH: rv.i = this->stop_length; break;
	case PARAM_DRAW_TRACK_LINES: rv.b = this->draw_track_lines; break;
	case PARAM_DD: rv.b = this->drawdirections; break;
	case PARAM_TRACK_DIRECTION_SIZE: rv.i = this->drawdirections_size; break;
	case PARAM_TRACK_THICKNESS: rv.i = this->track_thickness; break;
	case PARAM_TRACK_BG_THICKNESS: rv.i = this->track_bg_thickness; break;
	case PARAM_DLA: rv.b = this->drawlabels; break;
	case PARAM_DI: rv.b = this->drawimages; break;
	case PARAM_TRK_BG_COLOR: rv = SGVariant(this->track_bg_color); break;
	case PARAM_TRACK_DRAW_SPEED_FACTOR: rv.d = this->track_draw_speed_factor; break;
	case PARAM_TRACK_SORT_ORDER: rv.i = this->track_sort_order; break;
	case PARAM_WP_IMAGE_SIZE: rv.i = this->wp_image_size; break;
	case PARAM_WP_IMAGE_ALPHA: rv.i = this->wp_image_alpha; break;
	case PARAM_WP_IMAGE_CACHE_SIZE: rv.i = this->wp_image_cache_size; break;
	case PARAM_WP_MARKER_COLOR:   rv = SGVariant(this->wp_marker_color); break;
	case PARAM_WP_LABEL_FG_COLOR: rv = SGVariant(this->wp_label_fg_color); break;
	case PARAM_WP_LABEL_BG_COLOR: rv = SGVariant(this->wp_label_bg_color); break;
	case PARAM_WPBA: rv.b = this->wpbgand; break;
	case PARAM_WP_MARKER_TYPE: rv.i = this->wp_marker_type; break;
	case PARAM_WP_MARKER_SIZE: rv.i = this->wp_marker_size; break;
	case PARAM_WPSYMS: rv.b = this->wp_draw_symbols; break;
	case PARAM_WP_LABEL_FONT_SIZE: rv.i = this->wp_label_font_size; break;
	case PARAM_WP_SORT_ORDER: rv.i = this->wp_sort_order; break;
		// Metadata
	case PARAM_MDDESC:
		if (this->metadata) {
			rv = SGVariant(this->metadata->description);
		}
		break;
	case PARAM_MDAUTH:
		if (this->metadata) {
			rv = SGVariant(this->metadata->author);
		}
		break;
	case PARAM_MDTIME:
		if (this->metadata) {
			rv = SGVariant(this->metadata->timestamp);
		}
		break;
	case PARAM_MDKEYS:
		if (this->metadata) {
			rv = SGVariant(this->metadata->keywords);
		}
		break;
	default: break;
	}

	return rv;
}




void LayerTRWInterface::change_param(GtkWidget * widget, ui_change_values * values)
{
	// This '-3' is to account for the first few parameters not in the properties
	const int OFFSET = -3;
#ifdef K
	switch (values->param_id) {
		// Alter sensitivity of waypoint draw image related widgets according to the draw image setting.
	case PARAM_DI: {
		// Get new value
		SGVariant var = a_uibuilder_widget_get_value(widget, values->param);
		GtkWidget **ww1 = values->widgets;
		GtkWidget **ww2 = values->labels;
		GtkWidget *w1 = ww1[OFFSET + PARAM_WP_IMAGE_SIZE];
		GtkWidget *w2 = ww2[OFFSET + PARAM_WP_IMAGE_SIZE];
		GtkWidget *w3 = ww1[OFFSET + PARAM_WP_IMAGE_ALPHA];
		GtkWidget *w4 = ww2[OFFSET + PARAM_WP_IMAGE_ALPHA];
		GtkWidget *w5 = ww1[OFFSET + PARAM_WP_IMAGE_CACHE_SIZE];
		GtkWidget *w6 = ww2[OFFSET + PARAM_WP_IMAGE_CACHE_SIZE];
		if (w1) gtk_widget_set_sensitive(w1, var.b);
		if (w2) gtk_widget_set_sensitive(w2, var.b);
		if (w3) gtk_widget_set_sensitive(w3, var.b);
		if (w4) gtk_widget_set_sensitive(w4, var.b);
		if (w5) gtk_widget_set_sensitive(w5, var.b);
		if (w6) gtk_widget_set_sensitive(w6, var.b);
		break;
	}
		// Alter sensitivity of waypoint label related widgets according to the draw label setting.
	case PARAM_DLA: {
		// Get new value
		SGVariant var = a_uibuilder_widget_get_value(widget, values->param);
		GtkWidget **ww1 = values->widgets;
		GtkWidget **ww2 = values->labels;
		GtkWidget *w1 = ww1[OFFSET + PARAM_WP_LABEL_FG_COLOR];
		GtkWidget *w2 = ww2[OFFSET + PARAM_WP_LABEL_FG_COLOR];
		GtkWidget *w3 = ww1[OFFSET + PARAM_WP_LABEL_BG_COLOR];
		GtkWidget *w4 = ww2[OFFSET + PARAM_WP_LABEL_BG_COLOR];
		GtkWidget *w5 = ww1[OFFSET + PARAM_WPBA];
		GtkWidget *w6 = ww2[OFFSET + PARAM_WPBA];
		GtkWidget *w7 = ww1[OFFSET + PARAM_WP_LABEL_FONT_SIZE];
		GtkWidget *w8 = ww2[OFFSET + PARAM_WP_LABEL_FONT_SIZE];
		if (w1) gtk_widget_set_sensitive(w1, var.b);
		if (w2) gtk_widget_set_sensitive(w2, var.b);
		if (w3) gtk_widget_set_sensitive(w3, var.b);
		if (w4) gtk_widget_set_sensitive(w4, var.b);
		if (w5) gtk_widget_set_sensitive(w5, var.b);
		if (w6) gtk_widget_set_sensitive(w6, var.b);
		if (w7) gtk_widget_set_sensitive(w7, var.b);
		if (w8) gtk_widget_set_sensitive(w8, var.b);
		break;
	}
		// Alter sensitivity of all track colors according to the draw track mode.
	case PARAM_TRACK_DRAWING_MODE: {
		// Get new value
		SGVariant var = a_uibuilder_widget_get_value(widget, values->param);
		bool sensitive = (var.i == DRAWMODE_ALL_SAME_COLOR);
		GtkWidget **ww1 = values->widgets;
		GtkWidget **ww2 = values->labels;
		GtkWidget *w1 = ww1[OFFSET + PARAM_TRACK_COLOR_COMMON];
		GtkWidget *w2 = ww2[OFFSET + PARAM_TRACK_COLOR_COMMON];
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
#endif
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

#ifdef K
	// Waypoints
	for (auto i = this->waypoints.items.begin(); i != this->waypoints.items.end(); i++) {
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
#endif

#undef tlm_append

	*data = ba->data;
	*len = ba->len;
}




Layer * LayerTRWInterface::unmarshall(uint8_t * data, int len, Viewport * viewport)
{
	LayerTRW * trw = new LayerTRW();
	trw->set_coord_mode(viewport->get_coord_mode());

	int pl;

	// First the overall layer parameters
	memcpy(&pl, data, sizeof(pl));
	data += sizeof(pl);
	trw->unmarshall_params(data, pl);
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

			const QString & type_id = (const QString &) pl;

			// Also remember to (attempt to) convert each coordinate in case this is pasted into a different track_drawing_mode
			if (type_id == "sg.trw.track") {
				Track * trk = Track::unmarshall(data + sizeof_len_and_subtype, 0);
				/* Unmarshalling already sets track name, so we don't have to do it here. */
				trw->add_track(trk);
				trk->convert(trw->coord_mode);
			}
			if (type_id == "sg.trw.waypoint") {
				Waypoint * wp = Waypoint::unmarshall(data + sizeof_len_and_subtype, 0);
				/* Unmarshalling already sets waypoint name, so we don't have to do it here. */
				trw->add_waypoint(wp);
				wp->convert(trw->coord_mode);
			}
			if (type_id == "sg.trw.route") {
				Track * trk = Track::unmarshall(data + sizeof_len_and_subtype, 0);
				/* Unmarshalling already sets route name, so we don't have to do it here. */
				trw->add_route(trk);
				trk->convert(trw->coord_mode);
			}
		}
		consumed_length += tlm_size + sizeof_len_and_subtype;
		tlm_next;
	}
	//fprintf(stderr, "DEBUG: consumed_length %d vs len %d\n", consumed_length, len);

	// Not stored anywhere else so need to regenerate
	trw->get_waypoints_node().calculate_bounds();

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
	this->image_cache_free();

	delete this->tpwin;
}




void LayerTRW::draw_with_highlight_sub(Viewport * viewport, bool do_highlight)
{
	static TRWPainter painter(this, viewport);

	if (true /* this->tracks_node.visible */) { /* TODO: fix condition. */
		qDebug() << "II: Layer TRW: calling function to draw tracks";
		painter.draw_tracks(this->tracks.items, do_highlight);
	}

	if (true /* this->routes_node.visible */) { /* TODO: fix condition. */
		qDebug() << "II: Layer TRW: calling function to draw routes";
		painter.draw_tracks(this->routes.items, do_highlight);
	}

	if (true /* this->waypoints.visible */) { /* TODO: fix condition. */
		qDebug() << "II: Layer TRW: calling function to draw waypoints";
		painter.draw_waypoints(this->waypoints.items, do_highlight);
	}
}




void LayerTRW::draw(Viewport * viewport)
{
	/* If this layer is to be highlighted - then don't draw now - as it will be drawn later on in the specific highlight draw stage
	   This may seem slightly inefficient to test each time for every layer
	   but for a layer with *lots* of tracks & waypoints this can save some effort by not drawing the items twice. */
#ifdef K
	if (viewport->get_draw_with_highlight()
	    && this->get_window()->get_selected_trw_layer() == this) {

		return;
	}
#endif

	this->draw_with_highlight_sub(viewport, false);
}




void LayerTRW::draw_with_highlight(Viewport * viewport)
{
	/* kamilFIXME: enabling this code and then compiling it with -O0 results in crash when selecting trackpoint in viewport. */
#if 0
	/* Check the layer for visibility (including all the parents visibilities). */
	if (!this->tree_view->is_visible_in_tree(&this->iter)) {
		return;
	}
#endif
	this->draw_with_highlight_sub(viewport, true);
}




/**
 * Only handles a single track
 * It assumes the track belongs to the TRW Layer (it doesn't check this is the case)
 */
void LayerTRW::draw_with_highlight(Viewport * viewport, Track * trk, bool do_highlight)
{
	/* kamilFIXME: enabling this code and then compiling it with -O0 results in crash when selecting trackpoint in viewport. */
#if 0
	/* Check the layer for visibility (including all the parents visibilities). */
	if (!this->tree_view->is_visible_in_tree(&this->iter)) {
		return;
	}
#endif

	if (!trk) {
		return;
	}

	bool do_draw = (trk->type_id == "sg.trw.route" && this->routes.visible) || (trk->type_id == "sg.trw.track" && this->tracks.visible);
	if (!do_draw) {
		return;
	}

	static TRWPainter painter(this, viewport);
	painter.draw_track(trk, do_highlight);
}


/**
 * Only handles a single waypoint
 * It assumes the waypoint belongs to the TRW Layer (it doesn't check this is the case)
 */
void LayerTRW::draw_with_highlight(Viewport * viewport, Waypoint * wp, bool do_highlight)
{
	/* kamilFIXME: enabling this code and then compiling it with -O0 results in crash when selecting trackpoint in viewport. */
#if 0
	/* Check the layer for visibility (including all the parents visibilities). */
	if (!this->tree_view->is_visible_in_tree(&this->iter)) {
		return;
	}
#endif

	if (!this->waypoints.visible) {
		return;
	}

	if (!wp) {
		return;
	}

	static TRWPainter painter(this, viewport);
	painter.draw_waypoint(wp, do_highlight);
}




/**
 * Generally for drawing all tracks or routes or waypoints
 * tracks may be actually routes
 * It assumes they belong to the TRW Layer (it doesn't check this is the case)
 */
void LayerTRW::draw_with_highlight(Viewport * viewport, Tracks & tracks_, bool do_highlight)
{
	/* kamilFIXME: enabling this code and then compiling it with -O0 results in crash when selecting trackpoint in viewport. */
#if 0
	/* Check the layer for visibility (including all the parents visibilities). */
	if (!this->tree_view->is_visible_in_tree(&this->iter)) {
		return;
	}
#endif

	if (tracks_.empty()) {
		return;
	}

	bool is_routes = (*tracks_.begin()).second->type_id == "sg.trw.route";
	bool is_visible = (is_routes && this->routes.visible) || (!is_routes && this->tracks.visible);
	if (!is_visible) {
		return;
	}

	static TRWPainter painter(this, viewport);
	painter.draw_tracks(tracks_, do_highlight);
}




/**
 * Generally for drawing all tracks or routes or waypoints
 * tracks may be actually routes
 * It assumes they belong to the TRW Layer (it doesn't check this is the case)
 */
void LayerTRW::draw_with_highlight(Viewport * viewport, Waypoints & waypoints_, bool do_highlight)
{
	/* kamilFIXME: enabling this code and then compiling it with -O0 results in crash when selecting trackpoint in viewport. */
#if 0
	/* Check the layer for visibility (including all the parents visibilities). */
	if (!this->tree_view->is_visible_in_tree(&this->iter)) {
		return;
	}
#endif

	if (!this->waypoints.visible) {
		return;
	}

	if (waypoints_.empty()) {
		return;
	}

	static TRWPainter painter(this, viewport);
	painter.draw_waypoints(waypoints_, do_highlight);
}




void LayerTRW::new_track_pens(void)
{
	int32_t width = this->track_thickness;


	this->track_bg_pen = QPen(this->track_bg_color);
	this->track_bg_pen.setWidth(width + this->track_bg_thickness);


	/* Ensure new track drawing heeds line thickness setting,
	   however always have a minium of 2, as 1 pixel is really narrow. */
	int new_track_width = (this->track_thickness < 2) ? 2 : this->track_thickness;
	this->current_trk_pen = QPen(QColor("#FF0000"));
	this->current_trk_pen.setWidth(new_track_width);
	//gdk_gc_set_line_attributes(this->current_trk_gc, new_track_width, GDK_LINE_ON_OFF_DASH, GDK_CAP_ROUND, GDK_JOIN_ROUND);


	/* 'new_point' pen is exactly the same as the current track pen. */
	this->current_trk_new_point_pen = QPen(QColor("#FF0000"));
	this->current_trk_new_point_pen.setWidth(new_track_width);
	//gdk_gc_set_line_attributes(this->current_trk_new_point_gc, new_track_width, GDK_LINE_ON_OFF_DASH, GDK_CAP_ROUND, GDK_JOIN_ROUND);

	this->track_pens.clear();
	this->track_pens.resize(VIK_TRW_LAYER_TRACK_GC);

	this->track_pens[VIK_TRW_LAYER_TRACK_GC_STOP] = QPen(QColor("#874200"));
	this->track_pens[VIK_TRW_LAYER_TRACK_GC_STOP].setWidth(width);

	this->track_pens[VIK_TRW_LAYER_TRACK_GC_BLACK] = QPen(QColor("#000000")); /* Black. */
	this->track_pens[VIK_TRW_LAYER_TRACK_GC_BLACK].setWidth(width);

	this->track_pens[VIK_TRW_LAYER_TRACK_GC_SLOW] = QPen(QColor("#E6202E")); /* Red-ish. */
	this->track_pens[VIK_TRW_LAYER_TRACK_GC_SLOW].setWidth(width);

	this->track_pens[VIK_TRW_LAYER_TRACK_GC_AVER] = QPen(QColor("#D2CD26")); /* Yellow-ish. */
	this->track_pens[VIK_TRW_LAYER_TRACK_GC_AVER].setWidth(width);

	this->track_pens[VIK_TRW_LAYER_TRACK_GC_FAST] = QPen(QColor("#2B8700")); /* Green-ish. */
	this->track_pens[VIK_TRW_LAYER_TRACK_GC_FAST].setWidth(width);

	this->track_pens[VIK_TRW_LAYER_TRACK_GC_SINGLE] = QPen(QColor(this->track_color_common));
	this->track_pens[VIK_TRW_LAYER_TRACK_GC_SINGLE].setWidth(width);
}




void LayerTRW::add_tracks_node(void)
{
	assert(this->connected_to_tree);

	this->tracks.tree_item_type = TreeItemType::SUBLAYER;

	this->tracks.type_id = "sg.trw.tracks";
	this->tracks.accepted_child_type_ids << "sg.trw.track";
	this->tracks.tree_view = this->tree_view;
	this->tracks.window = this->get_window();
	this->tracks.parent_layer = this;


	this->tree_view->add_sublayer(&this->tracks, this, this->index, tr("Tracks"), NULL, false, 0);
}




void LayerTRW::add_waypoints_node(void)
{
	assert(this->connected_to_tree);

	this->waypoints.tree_view = this->tree_view;
	this->waypoints.window = this->get_window();
	this->waypoints.parent_layer = this;

	this->tree_view->add_sublayer(&this->waypoints, this, this->index, tr("Waypoints"), NULL, false, 0);
}




void LayerTRW::add_routes_node(void)
{
	assert(this->connected_to_tree);

	this->routes.tree_item_type = TreeItemType::SUBLAYER;

	this->routes.type_id = "sg.trw.routes";
	this->routes.accepted_child_type_ids << "sg.trw.route";
	this->routes.tree_view = this->tree_view;
	this->routes.window = this->get_window();
	this->routes.parent_layer = this;

	this->tree_view->add_sublayer(&this->routes, this, this->index, tr("Routes"), NULL, false, 0);
}




void LayerTRW::connect_to_tree(TreeView * tree_view_, TreeIndex const & layer_index)
{
	this->tree_view = tree_view_;
	this->index = layer_index;
	this->connected_to_tree = true;

	if (this->tracks.items.size() > 0) {
		this->add_tracks_node();
		this->tracks.add_items_as_children(this->tracks.items, this);
		this->tree_view->set_visibility(this->tracks.get_index(), this->tracks.visible);
	}

	if (this->routes.items.size() > 0) {
		this->add_routes_node();
		this->routes.add_items_as_children(this->routes.items, this);
		this->tree_view->set_visibility(this->routes.get_index(), this->routes.visible);
	}

	if (this->waypoints.items.size() > 0) {
		this->add_waypoints_node();
		this->waypoints.add_items_as_children(this->waypoints.items, this);
		this->tree_view->set_visibility(this->waypoints.get_index(), this->waypoints.visible);
	}

	this->verify_thumbnails();

	this->sort_all();
}




bool LayerTRW::sublayer_toggle_visible(TreeItem * sublayer)
{
	if (sublayer->type_id == "sg.trw.tracks") {
		return this->tracks.toggle_visible();

	} else if (sublayer->type_id == "sg.trw.waypoints") {
		return this->waypoints.toggle_visible();

	} else if (sublayer->type_id == "sg.trw.routes") {
		return this->routes.toggle_visible();

	} else if (sublayer->type_id == "sg.trw.track") {
		Track * trk = this->tracks.items.at(sublayer->uid);
		if (trk) {
			return trk->toggle_visible();
		} else {
			return true;
		}

	} else if (sublayer->type_id == "sg.trw.waypoint") {
		Waypoint * wp = this->waypoints.items.at(sublayer->uid);
		if (wp) {
			return wp->toggle_visible();
		} else {
			return true;
		}

	} else if (sublayer->type_id == "sg.trw.route") {
		Track * trk = this->routes.items.at(sublayer->uid);
		if (trk) {
			return trk->toggle_visible();
		} else {
			return true;
		}
	} else {
		;
	}

	return true;
}




/*
 * Return a property about tracks for this layer.
 */
int32_t LayerTRW::get_property_track_thickness()
{
	return this->track_thickness;
}




/*
 * Build up multiple routes information.
 */
static void trw_layer_routes_tooltip(Tracks & tracks, double * length)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		*length = *length + i->second->get_length();
	}
}




/* Structure to hold multiple track information for a layer. */
typedef struct {
	double length;
	time_t  start_time;
	time_t  end_time;
	int    duration;
} tooltip_tracks;

/*
 * Build up layer multiple track information via updating the tooltip_tracks structure.
 */
static void trw_layer_tracks_tooltip(Tracks & tracks, tooltip_tracks * tt)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {

		Track * trk = i->second;

		tt->length = tt->length + trk->get_length();

		/* Ensure times are available. */
		if (trk->empty() || !trk->get_tp_first()->has_timestamp) {
			continue;
		}

		/* Get trkpt only once - as using get_tp_last() iterates whole track each time. */
		Trackpoint * trkpt_last = trk->get_tp_last();
		if (!trkpt_last->has_timestamp) {
			continue;
		}


		time_t t1 = trk->get_tp_first()->timestamp;
		time_t t2 = trkpt_last->timestamp;

		/* Assume never actually have a track with a time of 0 (1st Jan 1970).
		   Hence initialize to the first 'proper' value. */
		if (tt->start_time == 0) {
			tt->start_time = t1;
		}
		if (tt->end_time == 0) {
			tt->end_time = t2;
		}

		/* Update find the earliest / last times. */
		if (t1 < tt->start_time) {
			tt->start_time = t1;
		}

		if (t2 > tt->end_time) {
			tt->end_time = t2;
		}

		/* Keep track of total time.
		   There maybe gaps within a track (eg segments)
		   but this should be generally good enough for a simple indicator. */
		tt->duration = tt->duration + (int) (t2 - t1);
	}
}




/*
  Generate tooltip text for the layer.
  This is relatively complicated as it considers information for
  no tracks, a single track or multiple tracks
  (which may or may not have timing information)
*/
QString LayerTRW::tooltip()
{
	char tbuf1[64] = { 0 };
	char tbuf2[64] = { 0 };
	char tbuf3[64] = { 0 };
	char tbuf4[10] = { 0 };

	static char tmp_buf[128] = { 0 };

	/* For compact date format I'm using '%x'     [The preferred date representation for the current locale without the time.] */

	if (!this->tracks.items.empty()) {
		tooltip_tracks tt = { 0.0, 0, 0, 0 };
		trw_layer_tracks_tooltip(this->tracks.items, &tt);

		GDate* gdate_start = g_date_new();
		g_date_set_time_t(gdate_start, tt.start_time);

		GDate* gdate_end = g_date_new();
		g_date_set_time_t(gdate_end, tt.end_time);

		if (g_date_compare(gdate_start, gdate_end)) {
			/* Dates differ so print range on separate line. */
			g_date_strftime(tbuf1, sizeof(tbuf1), "%x", gdate_start);
			g_date_strftime(tbuf2, sizeof(tbuf2), "%x", gdate_end);
			snprintf(tbuf3, sizeof(tbuf3), "%s to %s\n", tbuf1, tbuf2);
		} else {
			/* Same date so just show it and keep rest of text on the same line - provided it's a valid time! */
			if (tt.start_time != 0) {
				g_date_strftime(tbuf3, sizeof(tbuf3), "%x: ", gdate_start);
			}
		}

		tbuf2[0] = '\0';
		if (tt.length > 0.0) {
			/* Setup info dependent on distance units. */
			DistanceUnit distance_unit = Preferences::get_unit_distance();
			get_distance_unit_string(tbuf4, sizeof (tbuf4), distance_unit);
			double len_in_units = convert_distance_meters_to(tt.length, distance_unit);

			/* Timing information if available. */
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
		trw_layer_routes_tooltip(this->routes.items, &rlength);
		if (rlength > 0.0) {

			/* Setup info dependent on distance units. */
			DistanceUnit distance_unit = Preferences::get_unit_distance();
			get_distance_unit_string(tbuf4, sizeof (tbuf4), distance_unit);
			double len_in_units = convert_distance_meters_to(rlength, distance_unit);
			snprintf(tbuf1, sizeof(tbuf1), _("\nTotal route length %.1f %s"), len_in_units, tbuf4);
		}

		/* Put together all the elements to form compact tooltip text. */
		snprintf(tmp_buf, sizeof(tmp_buf),
			 _("Tracks: %ld - Waypoints: %ld - Routes: %ld%s%s"),
			 this->tracks.items.size(), this->waypoints.items.size(), this->routes.items.size(), tbuf2, tbuf1);

		g_date_free(gdate_start);
		g_date_free(gdate_end);
	}
	return QString(tmp_buf);
}




QString LayerTRW::sublayer_tooltip(TreeItem * sublayer)
{
	if (!sublayer) {
		qDebug() << "WW: Layer TRW: NULL sublayer in sublayer_tooltip()";
		return QString("");
	}

	if (sublayer->type_id == "sg.trw.tracks"
	    || sublayer->type_id == "sg.trw.routes") {

		return sublayer->get_tooltip();

	} else if (sublayer->type_id == "sg.trw.route" || sublayer->type_id == "sg.trw.track") {
		/* Same tooltip for route and track. */

		assert (sublayer->uid != SG_UID_INITIAL);

		Track * trk = NULL;
		if (sublayer->type_id == "sg.trw.track") {
			trk = this->tracks.items.at(sublayer->uid);
		} else {
			trk = this->routes.items.at(sublayer->uid);
		}

		if (trk) {
			/* Could be a better way of handling strings - but this works. */
			char time_buf1[20] = { 0 };
			char time_buf2[20] = { 0 };

			static char tmp_buf[100];
			/* Compact info: Short date eg (11/20/99), duration and length.
			   Hopefully these are the things that are most useful and so promoted into the tooltip. */
			if (!trk->empty() && trk->get_tp_first()->has_timestamp) {
				/* %x     The preferred date representation for the current locale without the time. */
				strftime(time_buf1, sizeof(time_buf1), "%x: ", gmtime(&(trk->get_tp_first()->timestamp)));
				time_t dur = trk->get_duration(true);
				if (dur > 0) {
					snprintf(time_buf2, sizeof(time_buf2), _("- %d:%02d hrs:mins"), (int)(dur/3600), (int)round(dur/60.0)%60);
				}
			}
			/* Get length and consider the appropriate distance units. */
			double tr_len = trk->get_length();
			DistanceUnit distance_unit = Preferences::get_unit_distance();
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
			return QString(tmp_buf);
		}
	} else if (sublayer->type_id == "sg.trw.waypoints") {
		return sublayer->get_tooltip();

	} else if (sublayer->type_id == "sg.trw.waypoint") {
		assert (sublayer->uid != SG_UID_INITIAL);

		Waypoint * wp = this->waypoints.items.at(sublayer->uid);
		/* It's OK to return NULL. */
		if (wp) {
			if (!wp->comment.isEmpty()) {
				return wp->comment;
			} else {
				return wp->description;
			}
		}
	} else {
		;
	}

	return QString("");
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
		/* Otherwise use default. */
		statusbar_format_code = strdup("KEATDN");
		need2free = true;
	} else {
		/* Format code may want to show speed - so may need previous trkpt to work it out. */
		tp_prev = this->current_trk->get_tp_prev(tp);
	}
	const QString msg = vu_trackpoint_formatted_message(statusbar_format_code, tp, tp_prev, this->current_trk, NAN);
	this->get_window()->get_statusbar()->set_message(StatusBarField::INFO, QString(msg));

	if (need2free) {
		free(statusbar_format_code);
	}
}




/*
 * Function to show basic waypoint information on the statusbar.
 */
void LayerTRW::set_statusbar_msg_info_wpt(Waypoint * wp)
{
	char tmp_buf1[64];
	switch (Preferences::get_unit_height()) {
	case HeightUnit::FEET:
		snprintf(tmp_buf1, sizeof(tmp_buf1), _("Wpt: Alt %dft"), (int) round(VIK_METERS_TO_FEET(wp->altitude)));
		break;
	default:
		// HeightUnit::METRES:
		snprintf(tmp_buf1, sizeof(tmp_buf1), _("Wpt: Alt %dm"), (int) round(wp->altitude));
	}

	/* Position part.
	   Position is put last, as this bit is most likely not to be seen if the display is not big enough,
	   one can easily use the current pointer position to see this if needed. */
	char * lat = NULL;
	char * lon = NULL;
	static struct LatLon ll = wp->coord.get_latlon();
	a_coords_latlon_to_string(&ll, &lat, &lon);

	/* Combine parts to make overall message. */
	QString msg;
	if (!wp->comment.isEmpty()) {
		/* Add comment if available. */
		msg = tr("%1 | %2 %3 | Comment: %4").arg(tmp_buf1).arg(lat).arg(lon).arg(wp->comment);
	} else {
		msg = tr("%1 | %2 %3").arg(tmp_buf1).arg(lat).arg(lon);
	}
	this->get_window()->get_statusbar()->set_message(StatusBarField::INFO, msg);
	free(lat);
	free(lon);
}




/**
 * General layer selection function, find out which bit is selected and take appropriate action.
 */
bool LayerTRW::kamil_selected(TreeItemType item_type, TreeItem * sublayer)
{
	/* Reset. */
	this->current_wp = NULL;
	this->cancel_current_tp(false);

	/* Clear statusbar. */
	this->get_window()->get_statusbar()->set_message(StatusBarField::INFO, "");

	qDebug() << "II: LayerTRW:   selection: set selected: top";

	switch (item_type) {
	case TreeItemType::LAYER:
		{
			qDebug() << "II: LayerTRW:   selection: set selected: layer" << this->name;
			this->get_window()->set_selected_trw_layer(this);
			/* Mark for redraw. */
			return true;
		}
		break;

	case TreeItemType::SUBLAYER:
		{
			if (sublayer->type_id == "sg.trw.tracks") {
				qDebug() << "II: LayerTRW:   selection: set selected: tracks";
				this->get_window()->set_selected_tracks(&this->tracks.items, this);
				/* Mark for redraw. */
				return true;
			} else if (sublayer->type_id == "sg.trw.track") {
				Track * trk = this->tracks.items.at(sublayer->uid);
				qDebug() << "II: LayerTRW:   selection: set selected: track" << trk->name;
				this->get_window()->set_selected_track(trk, this);
				/* Mark for redraw. */
				return true;

			} else if (sublayer->type_id == "sg.trw.routes") {
				qDebug() << "II: LayerTRW:   selection: set selected: routes";
				this->get_window()->set_selected_tracks(&this->routes.items, this);
				/* Mark for redraw. */
				return true;

			} else if (sublayer->type_id == "sg.trw.route") {
				Track * trk = this->routes.items.at(sublayer->uid);
				qDebug() << "II: LayerTRW:   selection: set selected: route" << trk->name;
				this->get_window()->set_selected_track(trk, this);
				/* Mark for redraw. */
				return true;

			} else if (sublayer->type_id == "sg.trw.waypoints") {
				qDebug() << "II: LayerTRW:   selection: set selected: waypoints";
				this->get_window()->set_selected_waypoints(&this->waypoints.items, this);
				/* Mark for redraw. */
				return true;

			} else if (sublayer->type_id == "sg.trw.waypoint") {
				Waypoint * wp = this->waypoints.items.at(sublayer->uid);
				qDebug() << "II: LayerTRW:   selection: set selected: waypoint" << wp->name;
				if (wp) {
					this->get_window()->set_selected_waypoint(wp, this);
					/* Show some waypoint info. */
					this->set_statusbar_msg_info_wpt(wp);
					/* Mark for redraw. */
					return true;
				}
			} else {
				qDebug() << "II: LayerTRW:   selection: set selected: clear highlight";
				return this->get_window()->clear_highlight();
			}
			return false;
		}
		break;

	default:
		return this->get_window()->clear_highlight();
		break;
	}
}




Tracks & LayerTRW::get_track_items()
{
	return this->tracks.items;
}




Tracks & LayerTRW::get_route_items()
{
	return this->routes.items;
}




Waypoints & LayerTRW::get_waypoint_items()
{
	return this->waypoints.items;
}




LayerTRWTracks & LayerTRW::get_tracks_node(void)
{
	return this->tracks;
}




LayerTRWTracks & LayerTRW::get_routes_node(void)
{
	return this->routes;
}




LayerTRWWaypoints & LayerTRW::get_waypoints_node(void)
{
	return this->waypoints;
}




bool LayerTRW::is_empty()
{
	return ! (this->tracks.items.size() || this->routes.items.size() || this->waypoints.items.size());
}




bool LayerTRW::get_tracks_visibility()
{
	return this->tracks.visible;
}




bool LayerTRW::get_routes_visibility()
{
	return this->routes.visible;
}




bool LayerTRW::get_waypoints_visibility()
{
	return this->waypoints.visible;
}




void LayerTRW::find_maxmin(struct LatLon maxmin[2])
{
	/* Continually reuse maxmin to find the latest maximum and minimum values.
	   First set to waypoints bounds. */

	this->waypoints.find_maxmin(maxmin);
	this->tracks.find_maxmin(maxmin);
	this->routes.find_maxmin(maxmin);
}




bool LayerTRW::find_center(Coord * dest)
{
	/* TODO: what if there's only one waypoint @ 0,0, it will think nothing found. like I don't have more important things to worry about... */
	struct LatLon maxmin[2] = { {0.0,0.0}, {0.0,0.0} };
	this->find_maxmin(maxmin);
	if (maxmin[0].lat == 0.0 && maxmin[0].lon == 0.0 && maxmin[1].lat == 0.0 && maxmin[1].lon == 0.0) {
		return false;
	} else {
		struct LatLon average = { (maxmin[0].lat+maxmin[1].lat)/2, (maxmin[0].lon+maxmin[1].lon)/2 };
		*dest = Coord(average, this->coord_mode);
		return true;
	}
}




void LayerTRW::centerize_cb(void)
{
	Coord coord;
	if (this->find_center(&coord)) {
		goto_coord(this->get_window()->get_layers_panel(), NULL, NULL, coord);
	} else {
		Dialog::info(tr("This layer has no waypoints or trackpoints."), this->get_window());
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




void LayerTRW::full_view_cb(void) /* Slot. */
{
	if (this->auto_set_view(this->menu_data->viewport)) {
		this->get_window()->get_layers_panel()->emit_update_window_cb();
	} else {
		Dialog::info(tr("This layer has no waypoints or trackpoints."), this->get_window());
	}
}




void LayerTRW::export_as_gpspoint_cb(void) /* Slot. */
{
	const QString auto_save_name = append_file_ext(this->get_name(), SGFileType::GPSPOINT);
	this->export_layer(tr("Export Layer"), auto_save_name, NULL, SGFileType::GPSPOINT);
}




void LayerTRW::export_as_gpsmapper_cb(void) /* Slot. */
{
	const QString auto_save_name = append_file_ext(this->get_name(), SGFileType::GPSMAPPER);
	this->export_layer(tr("Export Layer"), auto_save_name, NULL, SGFileType::GPSMAPPER);
}




void LayerTRW::export_as_gpx_cb(void) /* Slot. */
{
	const QString & auto_save_name = append_file_ext(this->get_name(), SGFileType::GPX);
	this->export_layer(tr("Export Layer"), auto_save_name, NULL, SGFileType::GPX);
}




void LayerTRW::export_as_kml_cb(void) /* Slot. */
{
	const QString & auto_save_name = append_file_ext(this->get_name(), SGFileType::KML);
	this->export_layer(tr("Export Layer"), auto_save_name, NULL, SGFileType::KML);
}




void LayerTRW::export_as_geojson_cb(void) /* Slot. */
{
	const QString auto_save_name = append_file_ext(this->get_name(), SGFileType::GEOJSON);
	this->export_layer(tr("Export Layer"), auto_save_name, NULL, SGFileType::GEOJSON);
}




void LayerTRW::export_via_babel_cb(void) /* Slot. */
{
	this->export_layer_with_gpsbabel(tr("Export Layer with GPSBabel"), this->get_name());
}




void LayerTRW::open_with_external_gpx_1_cb(void) /* Slot. */
{
	this->open_layer_with_external_program(Preferences::get_external_gpx_program_1());
}




void LayerTRW::open_with_external_gpx_2_cb(void) /* Slot. */
{
	this->open_layer_with_external_program(Preferences::get_external_gpx_program_2());
}




void LayerTRW::export_gpx_track_cb(void)
{
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (!trk || trk->name.isEmpty()) { /* TODO: will track's name be ever empty? */
		return;
	}

	const QString auto_save_name = append_file_ext(trk->name, SGFileType::GPX);

	QString title;
	if (this->menu_data->sublayer->type_id == "sg.trw.route") {
		title = tr("Export Route as GPX");
	} else {
		title = tr("Export Track as GPX");
	}
	this->export_layer(title, auto_save_name, trk, SGFileType::GPX);
}




void LayerTRW::find_waypoint_dialog_cb(void)
{
	LayersPanel * panel = this->get_window()->get_layers_panel();

	QInputDialog dialog(this->get_window());
	dialog.setWindowTitle(tr("Find"));
	dialog.setLabelText(tr("Waypoint Name:"));
	dialog.setInputMode(QInputDialog::TextInput);


	while (dialog.exec() == QDialog::Accepted) {
		QString name_ = dialog.textValue();

		/* Find *first* wp with the given name. */
		Waypoint * wp = this->waypoints.find_waypoint_by_name(name_);

		if (!wp) {
			Dialog::error(tr("Waypoint not found in this layer."), this->get_window());
		} else {
			panel->get_viewport()->set_center_coord(wp->coord, true);
			panel->emit_update_window_cb();
			this->tree_view->select_and_expose(wp->index);

			break;
		}

	}
}




bool LayerTRW::new_waypoint(Window * parent_window, const Coord * def_coord)
{
	const QString default_name = this->highest_wp_number_get();
	Waypoint * wp = new Waypoint();
	bool updated;
	wp->coord = *def_coord;

	/* Attempt to auto set height if DEM data is available. */
	wp->apply_dem_data(true);

	const QString returned_name = waypoint_properties_dialog(parent_window, default_name, this, wp, this->coord_mode, true, &updated);

	if (returned_name.size()) {
		wp->visible = true;
		wp->set_name(returned_name);
		this->add_waypoint(wp);
		return true;
	} else {
		delete wp;
		return false;
	}
}




void LayerTRW::acquire_from_wikipedia_waypoints_viewport_cb(void) /* Slot. */
{
	struct LatLon maxmin[2] = { {0.0,0.0}, {0.0,0.0} };
	LayersPanel * panel = this->get_window()->get_layers_panel();
	Viewport * viewport = this->get_window()->get_viewport();

	/* Note the order is max part first then min part - thus reverse order of use in min_max function: */
	viewport->get_min_max_lat_lon(&maxmin[1].lat, &maxmin[0].lat, &maxmin[1].lon, &maxmin[0].lon);

	a_geonames_wikipedia_box(this->get_window(), this, maxmin);
	this->waypoints.calculate_bounds();
	panel->emit_update_window_cb();
}




void LayerTRW::acquire_from_wikipedia_waypoints_layer_cb(void) /* Slot. */
{
	LayersPanel * panel = this->get_window()->get_layers_panel();
	struct LatLon maxmin[2] = { {0.0,0.0}, {0.0,0.0} };

	this->find_maxmin(maxmin);

	a_geonames_wikipedia_box(this->get_window(), this, maxmin);
	this->waypoints.calculate_bounds();
	panel->emit_update_window_cb();
}




#ifdef VIK_CONFIG_GEOTAG
void LayerTRW::geotagging_waypoint_mtime_keep_cb(void)
{
	sg_uid_t wp_uid = this->menu_data->sublayer->uid;
	Waypoint * wp = this->waypoints.items.at(wp_uid);
	if (wp) {
#ifdef K
		/* Update directly - not changing the mtime. */
		SlavGPS::a_geotag_write_exif_gps(wp->image, wp->coord, wp->altitude, true);
#endif
	}
}




void LayerTRW::geotagging_waypoint_mtime_update_cb(void)
{
	sg_uid_t wp_uid = this->menu_data->sublayer->uid;
	Waypoint * wp = this->waypoints.items.at(wp_uid);
	if (wp) {
#ifdef K
		/* Update directly. */
		a_geotag_write_exif_gps(wp->image, wp->coord, wp->altitude, false);
#endif
	}
}




/*
 * Use code in separate file for this feature as reasonably complex.
 */
void LayerTRW::geotagging_track_cb(void)
{
	sg_uid_t child_uid = this->menu_data->sublayer->uid;
	Track * trk = this->tracks.items.at(child_uid);
	/* Unset so can be reverified later if necessary. */
	this->has_verified_thumbnails = false;
	trw_layer_geotag_dialog(this->get_window(), this, NULL, trk);
}




void LayerTRW::geotagging_waypoint_cb(void)
{
	sg_uid_t wp_uid = this->menu_data->sublayer->uid;
	Waypoint * wp = this->waypoints.items.at(wp_uid);
	trw_layer_geotag_dialog(this->get_window(), this, wp, NULL);
}




void LayerTRW::geotag_images_cb(void) /* Slot. */
{
	/* Unset so can be reverified later if necessary. */
	this->has_verified_thumbnails = false;
	trw_layer_geotag_dialog(this->get_window(), this, NULL, NULL);
}
#endif




/* 'Acquires' - Same as in File Menu -> Acquire - applies into the selected TRW Layer */

void LayerTRW::acquire(VikDataSourceInterface *datasource)
{
	Window * window_ = this->get_window();
	LayersPanel * panel = window_->get_layers_panel();
	Viewport * viewport =  window_->get_viewport();

	DatasourceMode mode = datasource->mode;
	if (mode == DatasourceMode::AUTO_LAYER_MANAGEMENT) {
		mode = DatasourceMode::ADDTOLAYER;
	}
	a_acquire(window_, panel, viewport, mode, datasource, NULL, NULL);
}




/*
 * Acquire into this TRW Layer straight from GPS Device.
 */
void LayerTRW::acquire_from_gps_cb(void)
{
	this->acquire(&vik_datasource_gps_interface);
}




/*
 * Acquire into this TRW Layer from Directions.
 */
void LayerTRW::acquire_from_routing_cb(void) /* Slot. */
{
	this->acquire(&vik_datasource_routing_interface);
}




/*
 * Acquire into this TRW Layer from an entered URL.
 */
void LayerTRW::acquire_from_url_cb(void) /* Slot. */
{
	this->acquire(&vik_datasource_url_interface);
}




#ifdef VIK_CONFIG_OPENSTREETMAP
/*
 * Acquire into this TRW Layer from OSM.
 */
void LayerTRW::acquire_from_osm_cb(void) /* Slot. */
{
	this->acquire(&vik_datasource_osm_interface);
}




/**
 * Acquire into this TRW Layer from OSM for 'My' Traces.
 */
void LayerTRW::acquire_from_osm_my_traces_cb(void) /* Slot. */
{
	this->acquire(&vik_datasource_osm_my_traces_interface);
}
#endif




#ifdef VIK_CONFIG_GEOCACHES
/*
 * Acquire into this TRW Layer from Geocaching.com
 */
void LayerTRW::acquire_from_geocache_cb(void) /* Slot. */
{
	this->acquire(&vik_datasource_gc_interface);
}
#endif




#ifdef VIK_CONFIG_GEOTAG
/*
 * Acquire into this TRW Layer from images.
 */
void LayerTRW::acquire_from_geotagged_images_cb(void) /* Slot. */
{
	this->acquire(&vik_datasource_geotag_interface);

	/* Re-verify thumbnails as they may have changed. */
	this->has_verified_thumbnails = false;
	this->verify_thumbnails();
}
#endif




/*
 * Acquire into this TRW Layer from any GPS Babel supported file.
 */
void LayerTRW::acquire_from_file_cb(void) /* Slot. */
{
	this->acquire(&vik_datasource_file_interface);
}




void LayerTRW::upload_to_gps_cb(void) /* Slot. */
{
#ifdef K
	trw_menu_sublayer_t data2;
	memset(&data2, 0, sizeof (trw_menu_sublayer_t));

	data2.layer = this;
	data2.panel = this->menu_data->layers_panel;

	this->gps_upload_any_cb(&data2);
#endif
}




/**
 * If data->tree is defined that this will upload just that track.
 */
void LayerTRW::gps_upload_any_cb()
{
	LayersPanel * panel = this->get_window()->get_layers_panel();
	sg_uid_t child_uid = this->menu_data->sublayer->uid;

	/* May not actually get a track here as values[2&3] can be null. */
	Track * trk = NULL;
	GPSTransferType xfer_type = GPSTransferType::TRK; /* "sg.trw.tracks" = 0 so hard to test different from NULL! */
	bool xfer_all = false;

#ifdef K

	if ((bool) data->type_id) { /* kamilFIXME: don't cast. */
		xfer_all = false;
		if (this->menu_data->sublayer->type == "sg.trw.route") {
			trk = this->routes.at(child_uid);
			xfer_type = GPSTransferType::RTE;
		} else if (this->menu_data->sublayer->type == "sg.trw.track") {
			trk = this->tracks.at(child_uid);
			xfer_type = GPSTransferType::TRK;
		} else if (this->menu_data->sublayer->type == "sg.trw.waypoints") {
			xfer_type = GPSTransferType::WPT;
		} else if (this->menu_data->sublayer->type == "sg.trw.routes") {
			xfer_type = GPSTransferType::RTE;
		}
	} else if (!data->confirm) {
		xfer_all = true; /* i.e. whole layer. */
	}

	if (trk && !trk->visible) {
		Dialog::error(tr("Can not upload invisible track."), this->get_window());
		return;
	}

	GtkWidget *dialog = gtk_dialog_new_with_buttons(_("GPS Upload"),
							this->get_window(),
							GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_STOCK_OK,
							GTK_RESPONSE_ACCEPT,
							GTK_STOCK_CANCEL,
							GTK_RESPONSE_REJECT,
							NULL);

	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	GtkWidget *response_w = NULL;
#ifdef K
#if GTK_CHECK_VERSION (2, 20, 0)
	response_w = gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
#endif
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

	/* Get info from reused datasource dialog widgets. */
	char* protocol = datasource_gps_get_protocol(dgs);
	QString port = datasource_gps_get_descriptor(dgs);
	/* Don't free the above strings as they're references to values held elsewhere. */
	bool do_tracks = datasource_gps_get_do_tracks(dgs);
	bool do_routes = datasource_gps_get_do_routes(dgs);
	bool do_waypoints = datasource_gps_get_do_waypoints(dgs);
	bool turn_off = datasource_gps_get_off(dgs);

	gtk_widget_destroy(dialog);

	/* When called from the viewport - work the corresponding layerspanel: */
	if (!panel) {
		panel = this->get_window()->get_layers_panel();
	}

	/* Apply settings to transfer to the GPS device. */
	vik_gps_comm(this,
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
#endif
}




void LayerTRW::new_waypoint_cb(void) /* Slot. */
{
	LayersPanel * panel = this->get_window()->get_layers_panel();
	/* TODO longone: okay, if layer above (aggregate) is invisible but vtl->visible is true, this redraws for no reason.
	   Instead return true if you want to update. */
	if (this->new_waypoint(this->get_window(), panel->get_viewport()->get_center())) {
		this->waypoints.calculate_bounds();
		if (this->visible) {
			panel->emit_update_window_cb();
		}
	}
}




void LayerTRW::new_track_create_common(const QString & new_name)
{
	qDebug() << "II: Layer TRW: new track create common, track name" << new_name;

	this->current_trk = new Track(false);
	this->current_trk->set_defaults();
	this->current_trk->visible = true;

	if (this->track_drawing_mode == DRAWMODE_ALL_SAME_COLOR) {
		/* Create track with the preferred color from the layer properties. */
		this->current_trk->color = this->track_color_common;
	} else {
		this->current_trk->color = QColor("#aa22dd"); //QColor("#000000");
	}

	this->current_trk->has_color = true;
	this->current_trk->set_name(new_name);
	this->add_track(this->current_trk);
}




void LayerTRW::new_track_cb() /* Slot. */
{
	if (!this->current_trk) {
		const QString uniq_name = this->new_unique_element_name("sg.trw.track", tr("Track")) ;
		this->new_track_create_common(uniq_name);

		this->get_window()->activate_tool(LAYER_TRW_TOOL_CREATE_TRACK);
	}
}




void LayerTRW::new_route_create_common(const QString & new_name)
{
	this->current_trk = new Track(true);
	this->current_trk->set_defaults();
	this->current_trk->visible = true;
	/* By default make all routes red. */
	this->current_trk->has_color = true;
	this->current_trk->color = QColor("red");
	this->current_trk->set_name(new_name);

	this->add_route(this->current_trk);
}




void LayerTRW::new_route_cb(void) /* Slot. */
{
	if (!this->current_trk) {
		const QString uniq_name = this->new_unique_element_name("sg.trw.route", tr("Route")) ;
		this->new_route_create_common(uniq_name);

		this->get_window()->activate_tool(LAYER_TRW_TOOL_CREATE_ROUTE);
	}
}




void LayerTRW::full_view_routes_cb(void) /* Slot. */
{
	LayersPanel * panel = this->get_window()->get_layers_panel();

	if (this->routes.items.size() > 0) {
		struct LatLon maxmin[2] = { {0,0}, {0,0} };
		this->routes.find_maxmin(maxmin);
		this->zoom_to_show_latlons(panel->get_viewport(), maxmin);
		panel->emit_update_window_cb();
	}
}




void LayerTRW::finish_track_cb(void) /* Slot. */
{
	this->current_trk = NULL;
	this->route_finder_started = false;
	this->emit_changed();
}




void LayerTRW::full_view_tracks_cb(void) /* Slot. */
{
	LayersPanel * panel = this->get_window()->get_layers_panel();

	if (this->tracks.items.size() > 0) {
		struct LatLon maxmin[2] = { {0,0}, {0,0} };
		this->tracks.find_maxmin(maxmin);
		this->zoom_to_show_latlons(panel->get_viewport(), maxmin);
		panel->emit_update_window_cb();
	}
}




void LayerTRW::upload_to_osm_traces_cb(void) /* Slot. */
{
	osm_traces_upload_viktrwlayer(this, NULL);
}




void LayerTRW::osm_traces_upload_track_cb(void)
{
	if (this->menu_data->misc) {
		Track * trk = ((Track *) this->menu_data->misc);
		osm_traces_upload_viktrwlayer(this, trk);
	}
}




void LayerTRW::add_waypoint(Waypoint * wp)
{
	wp->window = this->get_window();
	wp->parent_layer = this;

	this->waypoints.items.insert({{ wp->uid, wp }});

	if (this->connected_to_tree) {
		if (this->waypoints.items.size() == 1) { /* We compare against '1' because we already added a first wp to ::waypoints() at the beginning of this function. */
			this->add_waypoints_node();
		}

		time_t timestamp = 0;
		if (wp->has_timestamp) {
			timestamp = wp->timestamp;
		}

		/* Visibility column always needed for waypoints. */
		this->waypoints.add_child(wp, this, wp->name, NULL /* wp->symbol */, timestamp);

		/* Actual setting of visibility dependent on the waypoint. */
		this->tree_view->set_visibility(wp->index, wp->visible);

		/* Sort now as post_read is not called on a waypoint connected to tree. */
		this->tree_view->sort_children(this->waypoints.get_index(), this->wp_sort_order);
	}

	this->highest_wp_number_add_wp(wp->name);
}




void LayerTRW::add_track(Track * trk)
{
	trk->window = this->get_window();
	trk->parent_layer = this;

	this->tracks.items.insert({{ trk->uid, trk }});

	if (this->connected_to_tree) {
		if (this->tracks.items.size() == 1) { /* We compare against '1' because we already added a first trk to ::tracks() at the beginning of this function. */
			this->add_tracks_node();
		}

		time_t timestamp = 0;
		Trackpoint * tp = trk->get_tp_first();
		if (tp && tp->has_timestamp) {
			timestamp = tp->timestamp;
		}

		/* Visibility column always needed for tracks. */
		this->tracks.add_child(trk, this, trk->name, NULL, timestamp);

		/* Actual setting of visibility dependent on the track. */
		this->tree_view->set_visibility(trk->index, trk->visible);

		/* Sort now as post_read is not called on a track connected to tree. */
		this->tree_view->sort_children(this->tracks.get_index(), this->track_sort_order);
	}

	this->tracks.update_treeview(trk);
}




void LayerTRW::add_route(Track * trk)
{
	trk->window = this->get_window();
	trk->parent_layer = this;

	this->routes.items.insert({{ trk->uid, trk }});

	if (this->connected_to_tree) {
		if (this->routes.items.size() == 1) { /* We compare against '1' because we already added a first trk to ::routes() at the beginning of this function. */
			this->add_routes_node();
		}

		/* Visibility column always needed for routes. */
		this->routes.add_child(trk, this, trk->name, NULL, 0); /* Routes don't have times. */

		/* Actual setting of visibility dependent on the route. */
		this->tree_view->set_visibility(trk->index, trk->visible);

		/* Sort now as post_read is not called on a route connected to tree. */
		this->tree_view->sort_children(this->routes.get_index(), this->track_sort_order);
	}

	this->routes.update_treeview(trk);
}




/* to be called whenever a track has been deleted or may have been changed. */
void LayerTRW::cancel_tps_of_track(Track * trk)
{
	if (this->current_trk == trk) {
		this->cancel_current_tp(false);
	}
}




/**
 * Normally this is done to due the waypoint size preference having changed.
 */
void LayerTRW::reset_waypoints()
{
	for (auto i = this->waypoints.items.begin(); i != this->waypoints.items.end(); i++) {
		Waypoint * wp = i->second;
		if (!wp->symbol_name.isEmpty()) {
			/* Reapply symbol setting to update the pixbuf. TODO: what's does it really do? */
			const QString tmp_symbol_name = wp->symbol_name;
			wp->set_symbol_name(tmp_symbol_name);
		}
	}
}




/**
   \brief Get a a unique new name for element of type \param item_type_id
*/
QString LayerTRW::new_unique_element_name(const QString & item_type_id, const QString & old_name)
{
	if (item_type_id == "sg.trw.track") {
		return this->tracks.new_unique_element_name(old_name);
	} else if (item_type_id == "sg.trw.waypoint") {
		return this->waypoints.new_unique_element_name(old_name);
	} else {
		return this->routes.new_unique_element_name(old_name);
	}
}




void LayerTRW::filein_add_waypoint(Waypoint * wp, const QString & wp_name)
{
	/* No more uniqueness of name forced when loading from a file.
	   This now makes this function a little redunant as we just flow the parameters through. */
	wp->set_name(wp_name);
	this->add_waypoint(wp);
}




void LayerTRW::filein_add_track(Track * trk, const QString & trk_name)
{
	if (this->route_finder_append && this->current_trk) {
		trk->remove_dup_points(); /* Make "double point" track work to undo. */

		/* Enforce end of current track equal to start of tr. */
		Trackpoint * cur_end = this->current_trk->get_tp_last();
		Trackpoint * new_start = trk->get_tp_first();
		if (cur_end && new_start) {
			if (cur_end->coord != new_start->coord) {
				this->current_trk->add_trackpoint(new Trackpoint(*cur_end), false);
			}
		}

		this->current_trk->steal_and_append_trackpoints(trk);
		trk->free();
		this->route_finder_append = false; /* This means we have added it. */
	} else {
		trk->set_name(trk_name);
		/* No more uniqueness of name forced when loading from a file. */
		if (trk->type_id == "sg.trw.route") {
			this->add_route(trk);
		} else {
			this->add_track(trk);
		}

		if (this->route_finder_check_added_track) {
			trk->remove_dup_points(); /* Make "double point" track work to undo. */
			this->route_finder_added_track = trk;
		}
	}
}




/*
 * Move an item from one TRW layer to another TRW layer.
 */
void LayerTRW::move_item(LayerTRW * trw_dest, sg_uid_t sublayer_uid, const QString & item_type_id)
{
#ifdef K
	LayerTRW * trw_src = this;
	/* When an item is moved the name is checked to see if it clashes with an existing name
	   in the destination layer and if so then it is allocated a new name. */

	/* TODO reconsider strategy when moving within layer (if anything...). */
	if (trw_src == trw_dest) {
		return;
	}

	if (item_type_id == "sg.trw.track") {
		Track * trk = this->tracks.at(sublayer_uid);
		Track * trk2 = new Track(*trk);

		const QString uniq_name = trw_dest->new_unique_element_name(item_type_id, trk->name_);
		trk2->set_name(uniq_name);
		/* kamilFIXME: in C application did we free this unique name anywhere? */

		trw_dest->add_track(trk2);

		this->delete_track(trk);
		/* Reset layer timestamps in case they have now changed. */
		trw_dest->tree_view->set_timestamp(trw_dest->index, trw_dest->get_timestamp());
		trw_src->tree_view->set_timestamp(trw_src->index, trw_src->get_timestamp());
	}

	if (item_type_id == "sg.trw.route") {
		Track * trk = this->routes.at(sublayer_uid);
		Track * trk2 = new Track(*trk);

		const QString uniq_name = trw_dest->new_unique_element_name(item_type_id, trk->name_);
		trk2->set_name(uniq_name);

		trw_dest->add_route(trk2);

		this->delete_route(trk);
	}

	if (item_type_id == "sg.trw.waypoint") {
		Waypoint * wp = this->waypoints.items.at(sublayer_uid);
		Waypoint * wp2 = new Waypoint(*wp);

		const QString uniq_name = trw_dest->new_unique_element_name(item_type_id, wp->name_);
		wp2->set_name(uniq_name);

		trw_dest->add_waypoint(wp2);

		this->delete_waypoint(wp);

		/* Recalculate bounds even if not renamed as maybe dragged between layers. */
		trw_dest->waypoints.calculate_bounds();
		trw_src->waypoints.calculate_bounds();
		/* Reset layer timestamps in case they have now changed. */
		trw_dest->tree_view->set_timestamp(trw_dest->index, trw_dest->get_timestamp());
		trw_src->tree_view->set_timestamp(trw_src->index, trw_src->get_timestamp());
	}
#endif
}




void LayerTRW::drag_drop_request(Layer * src, TreeIndex * src_item_iter, void * GtkTreePath_dest_path)
{
#ifdef K
	LayerTRW * trw_dest = this;
	LayerTRW * trw_src = (LayerTRW *) src;

	TreeItem * sublayer = trw_src->tree_view->get_sublayer(src_item_iter);

	if (!trw_src->tree_view->get_name(src_item_iter)) {
		GList *items = NULL;

		if (sublayer->type == "sg.trw.tracks") {
			trw_src->tracks.list_trk_uids(&items);
		}
		if (sublayer->type == "sg.trw.waypoints") {
			trw_src->waypoints.list_wp_uids(&items);
		}
		if (sublayer->type == "sg.trw.routes") {
			trw_src->routes.list_trk_uids(&items);
		}

		GList * iter = items;
		while (iter) {
			if (sublayer->type == "sg.trw.tracks") {
				trw_src->move_item(trw_dest, iter->data, "sg.trw.track");
			} else if (sublayer->type == "sg.trw.routes") {
				trw_src->move_item(trw_dest, iter->data, "sg.trw.route");
			} else {
				trw_src->move_item(trw_dest, iter->data, "sg.trw.waypoint");
			}
			iter = iter->next;
		}
		if (items) {
			g_list_free(items);
		}
	} else {
		char * name = trw_src->tree_view->get_name(src_item_iter);
		trw_src->move_item(trw_dest, name, sublayer->type);
	}
#endif
}




bool LayerTRW::delete_track(Track * trk)
{
	/* TODO: why check for trk->name here? */
	if (!trk || trk->name.isEmpty()) {
		return false;
	}

	if (trk == this->current_trk) {
		this->current_trk = NULL;
		this->moving_tp = false;
		this->route_finder_started = false;
	}

	bool was_visible = trk->visible;

	if (trk == this->route_finder_added_track) {
		this->route_finder_added_track = NULL;
	}

	/* Could be current_tp, so we have to check. */
	this->cancel_tps_of_track(trk);

	qDebug() << "II: Layer TRW: erasing track" << trk->name << "from tree view";
	this->tree_view->erase(trk->index);
	this->tracks.items.erase(trk->uid); /* kamilTODO: should this line be inside of "if (it)"? */

	/* If last sublayer, then remove sublayer container. */
	if (this->tracks.items.size() == 0) {
		this->tree_view->erase(this->tracks.get_index());
	}
	/* In case it was selected (no item delete signal ATM). */
	this->get_window()->clear_highlight();

	return was_visible;
}




bool LayerTRW::delete_route(Track * trk)
{
	/* kamilTODO: why check for trk->name here? */
	if (!trk || trk->name.isEmpty()) {
		return false;
	}

	if (trk == this->current_trk) {
		this->current_trk = NULL;
		this->moving_tp = false;
	}

	bool was_visible = trk->visible;

	if (trk == this->route_finder_added_track) {
		this->route_finder_added_track = NULL;
	}

	/* Could be current_tp, so we have to check. */
	this->cancel_tps_of_track(trk);

	this->tree_view->erase(trk->index);
	this->routes.items.erase(trk->uid); /* kamilTODO: should this line be inside of "if (it)"? */

	/* If last sublayer, then remove sublayer container. */
	if (this->routes.items.size() == 0) {
		this->tree_view->erase(this->routes.get_index());
	}

	/* In case it was selected (no item delete signal ATM). */
	this->get_window()->clear_highlight();

	return was_visible;
}




bool LayerTRW::delete_waypoint(Waypoint * wp)
{
	/* TODO: why check for wp->name here? */
	if (!wp || wp->name.isEmpty()) {
		return false;
	}

	if (wp == current_wp) {
		this->current_wp = NULL;
		this->moving_wp = false;
	}

	bool was_visible = wp->visible;

	this->tree_view->erase(wp->index);

	this->highest_wp_number_remove_wp(wp->name);

	/* kamilTODO: should this line be inside of "if (it)"? */
	this->waypoints.items.erase(wp->uid); /* Last because this frees the name. */

	/* If last sublayer, then remove sublayer container. */
	if (this->waypoints.items.size() == 0) {
		this->tree_view->erase(this->waypoints.get_index());
	}
	/* In case it was selected (no item delete signal ATM). */
	this->get_window()->clear_highlight();

	return was_visible;
}




/*
 * Delete a waypoint by the given name
 * NOTE: ATM this will delete the first encountered Waypoint with the specified name
 *   as there be multiple waypoints with the same name
 */
bool LayerTRW::delete_waypoint_by_name(const QString & wp_name)
{
	/* Currently only the name is used in this waypoint find function. */
	Waypoint * wp = this->waypoints.find_waypoint_by_name(wp_name);
	if (wp) {
		return delete_waypoint(wp);
	} else {
		return false;
	}
}




/*
 * Delete a track by the given name
 * NOTE: ATM this will delete the first encountered Track with the specified name
 *   as there may be multiple tracks with the same name within the specified hash table
 */
bool LayerTRW::delete_track_by_name(const QString & trk_name, bool is_route)
{
	if (is_route) {
		Track * trk = this->routes.find_track_by_name(trk_name);
		if (trk) {
			return delete_route(trk);
		}
	} else {
		Track * trk = this->tracks.find_track_by_name(trk_name);
		if (trk) {
			return delete_track(trk);
		}
	}

	return false;
}




void LayerTRW::delete_all_routes()
{
	this->current_trk = NULL;
	this->route_finder_added_track = NULL;
	if (this->current_trk) { /* FIXME: we have just set this to NULL! */
		this->cancel_current_tp(false);
	}

	for (auto i = this->routes.items.begin(); i != this->routes.items.end(); i++) {
		tree_view->erase(i->second->index);
	}

	this->routes.items.clear(); /* kamilTODO: call destructors of routes. */

	this->tree_view->erase(this->routes.get_index());

	this->emit_changed();
}




void LayerTRW::delete_all_tracks()
{
	this->current_trk = NULL;
	this->route_finder_added_track = NULL;
	if (this->current_trk) { /* FIXME: we have just set this to NULL! */
		this->cancel_current_tp(false);
	}

	for (auto i = this->tracks.items.begin(); i != this->tracks.items.end(); i++) {
		tree_view->erase(i->second->index);
	}

	this->tracks.items.clear(); /* kamilTODO: call destructors of tracks. */

	this->tree_view->erase(this->tracks.get_index());

	this->emit_changed();
}




void LayerTRW::delete_all_waypoints()
{
	this->current_wp = NULL;
	this->moving_wp = false;

	this->highest_wp_number_reset();

	for (auto i = this->waypoints.items.begin(); i != this->waypoints.items.end(); i++) {
		tree_view->erase(i->second->index);
	}

	this->waypoints.items.clear(); /* kamilTODO: does this really call destructors of Waypoints? */

	this->tree_view->erase(this->waypoints.get_index());

	this->emit_changed();
}




void LayerTRW::delete_all_tracks_cb(void) /* Slot. */
{
	/* Get confirmation from the user. */
	if (Dialog::yes_or_no(tr("Are you sure you want to delete all tracks in \"%1\"?").arg(QString(this->get_name())), this->get_window())) {
		this->delete_all_tracks();
	}
}




void LayerTRW::delete_all_routes_cb(void) /* Slot. */
{
	/* Get confirmation from the user. */
	if (Dialog::yes_or_no(tr("Are you sure you want to delete all routes in \"%1\"?").arg(QString(this->get_name())), this->get_window())) {
		this->delete_all_routes();
	}
}




void LayerTRW::delete_all_waypoints_cb(void) /* Slot. */
{
	/* Get confirmation from the user. */
	if (Dialog::yes_or_no(tr("Are you sure you want to delete all waypoints in \"%1\"?").arg(QString(this->get_name())), this->get_window())) {
		this->delete_all_waypoints();
	}
}




void LayerTRW::delete_sublayer_cb(void)
{
	sg_uid_t child_uid = this->menu_data->sublayer->uid;
	bool was_visible = false;

	if (this->menu_data->sublayer->type_id == "sg.trw.waypoint") {
		Waypoint * wp = this->waypoints.items.at(child_uid);
		if (wp && !wp->name.isEmpty()) {
			if (this->menu_data->confirm) {
				/* Get confirmation from the user. */
				/* Maybe this Waypoint Delete should be optional as is it could get annoying... */
				if (!Dialog::yes_or_no(tr("Are you sure you want to delete the waypoint \"%1\"?").arg(wp->name)), this->get_window()) {
					return;
				}
			}

			was_visible = this->delete_waypoint(wp);
			this->waypoints.calculate_bounds();
			/* Reset layer timestamp in case it has now changed. */
			this->tree_view->set_timestamp(this->index, this->get_timestamp());
		}
	} else if (this->menu_data->sublayer->type_id == "sg.trw.track") {
		Track * trk = this->tracks.items.at(child_uid);
		if (trk && !trk->name.isEmpty()) {
			if (this->menu_data->confirm) {
				/* Get confirmation from the user. */
				if (!Dialog::yes_or_no(tr("Are you sure you want to delete the track \"%1\"?").arg(trk->name)), this->get_window()) {
					return;
				}
			}

			was_visible = this->delete_track(trk);
			/* Reset layer timestamp in case it has now changed. */
			this->tree_view->set_timestamp(this->index, this->get_timestamp());
		}
	} else {
		Track * trk = this->routes.items.at(child_uid);
		if (trk && !trk->name.isEmpty()) {
			if (this->menu_data->confirm) {
				/* Get confirmation from the user. */
				if (!Dialog::yes_or_no(tr("Are you sure you want to delete the route \"%1\"?").arg(trk->name)), this->get_window()) {
					return;
				}
			}
			was_visible = this->delete_route(trk);
		}
	}
	if (was_visible) {
		this->emit_changed();
	}
}




void LayerTRW::properties_item_cb(void)
{
	if (this->menu_data->sublayer->type_id == "sg.trw.waypoint") {
		sg_uid_t wp_uid = this->menu_data->sublayer->uid;
		Waypoint * wp = this->waypoints.items.at(wp_uid);

		if (wp && !wp->name.isEmpty()) {
			bool updated = false;

			const QString new_name = waypoint_properties_dialog(this->get_window(), wp->name, this, wp, this->coord_mode, false, &updated);
			if (new_name.size()) {
				this->waypoints.rename_waypoint(wp, new_name);
			}

			if (updated && this->menu_data->sublayer->index.isValid()) {
				this->tree_view->set_icon(this->menu_data->sublayer->index, get_wp_sym_small(wp->symbol_name));
			}

			if (updated && this->visible) {
				this->emit_changed();
			}
		}
	} else {
		Track * trk = this->get_track_helper(this->menu_data->sublayer);
		if (trk && !trk->name.isEmpty()) {
			track_properties_dialog(this->get_window(), this, trk);
		}
	}
}




void LayerTRW::profile_item_cb(void)
{
	if (this->menu_data->sublayer->type_id != "sg.trw.waypoint") {
		Track * trk = this->get_track_helper(this->menu_data->sublayer);
		if (trk && !trk->name.isEmpty()) {
			track_profile_dialog(this->get_window(),
					     this,
					     trk,
					     this->get_window()->get_layers_panel(),
					     this->menu_data->viewport);
		}
	}
}




/**
 * Show track statistics.
 * ATM jump to the stats page in the properties
 * TODO: consider separating the stats into an individual dialog?
 */
void LayerTRW::track_statistics_cb(void)
{
	Track * trk = this->get_track_helper(this->menu_data->sublayer);
	if (trk && !trk->name.isEmpty()) {
		track_properties_dialog(this->get_window(), this, trk, true);
	}
}




static void goto_coord(LayersPanel * panel, Layer * layer, Viewport * viewport, const Coord & coord)
{
	if (panel) {
		panel->get_viewport()->set_center_coord(coord, true);
		panel->emit_update_window_cb();
	} else {
		/* Since panel not set, layer & viewport should be valid instead! */
		if (layer && viewport) {
			viewport->set_center_coord(coord, true);
			layer->emit_changed();
		}
	}
}




void LayerTRW::goto_track_startpoint_cb(void)
{
	LayersPanel * panel = this->get_window()->get_layers_panel();
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (trk && !trk->empty()) {
		goto_coord(panel, this, this->menu_data->viewport, trk->get_tp_first()->coord);
	}
}




void LayerTRW::goto_track_center_cb(void)
{
	LayersPanel * panel = this->get_window()->get_layers_panel();
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (trk && !trk->empty()) {
		struct LatLon average, maxmin[2] = { {0,0}, {0,0} };
		trk->find_maxmin(maxmin);
		average.lat = (maxmin[0].lat+maxmin[1].lat)/2;
		average.lon = (maxmin[0].lon+maxmin[1].lon)/2;
		Coord coord(average, this->coord_mode);
		goto_coord(panel, this, this->menu_data->viewport, coord);
	}
}




void LayerTRW::convert_track_route_cb(void)
{
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (!trk) {
		return;
	}

	/* Converting a track to a route can be a bit more complicated,
	   so give a chance to change our minds: */
	if (trk->type_id == "sg.trw.track"
	    && ((trk->get_segment_count() > 1)
		|| (trk->get_average_speed() > 0.0))) {

		if (!Dialog::yes_or_no(tr("Converting a track to a route removes extra track data such as segments, timestamps, etc...\nDo you want to continue?"), this->get_window())) {
			return;
		}
	}

	/* Copy it. */
	Track * trk_copy = new Track(*trk);

	/* Convert. */
	trk_copy->type_id = trk_copy->type_id == "sg.trw.route" ? "sg.trw.track": "sg.trw.route";

	/* ATM can't set name to self - so must create temporary copy. TODO: verify this comment. */
	const QString copy_name = trk_copy->name;

	/* Delete old one and then add new one. */
	if (trk->type_id == "sg.trw.route") {
		this->delete_route(trk);
		trk_copy->set_name(copy_name);
		this->add_track(trk_copy);
	} else {
		/* Extra route conversion bits... */
		trk_copy->merge_segments();
		trk_copy->to_routepoints();

		this->delete_track(trk);
		trk_copy->set_name(copy_name);
		this->add_route(trk_copy);
	}

	/* Update in case color of track / route changes when moving between sublayers. */
	this->emit_changed();
}




void LayerTRW::anonymize_times_cb(void)
{
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (!trk) {
		return;
	}

	trk->anonymize_times();
}




void LayerTRW::interpolate_times_cb(void)
{
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (!trk) {
		return;
	}

	trk->interpolate_times();
}




void LayerTRW::extend_track_end_cb(void)
{
	LayersPanel * panel = this->get_window()->get_layers_panel();
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (!trk) {
		return;
	}

	this->current_trk = trk;

	this->get_window()->activate_tool(trk->type_id == "sg.trw.route" ? LAYER_TRW_TOOL_CREATE_ROUTE : LAYER_TRW_TOOL_CREATE_TRACK);

	if (!trk->empty()) {
		goto_coord(panel, this, this->menu_data->viewport, trk->get_tp_last()->coord);
	}
}




/**
 * Extend a track using route finder.
 */
void LayerTRW::extend_track_end_route_finder_cb(void)
{
	sg_uid_t child_uid = this->menu_data->sublayer->uid;
	Track * trk = this->routes.items.at(child_uid);
	if (!trk) {
		return;
	}

	this->get_window()->activate_tool(LAYER_TRW_TOOL_ROUTE_FINDER);

	this->current_trk = trk;
	this->route_finder_started = true;

	if (!trk->empty()) {
		goto_coord(this->get_window()->get_layers_panel(), this, this->menu_data->viewport, trk->get_tp_last()->coord);
	}
}




bool LayerTRW::dem_test(LayersPanel * panel)
{
	/* If have a panel then perform a basic test to see if any DEM info available... */
	if (panel) {
		std::list<Layer const *> * dems = panel->get_all_layers_of_type(LayerType::DEM, true); /* Includes hidden DEM layer types. */
		if (dems->empty()) {
			Dialog::error(tr("No DEM layers available, thus no DEM values can be applied."), this->get_window());
			return false;
		}
	}
	return true;
}




/**
 * A common function for applying the DEM values and reporting the results.
 */
void LayerTRW::apply_dem_data_common(LayersPanel * panel, Track * trk, bool skip_existing_elevations)
{
	if (!this->dem_test(panel)) {
		return;
	}

	unsigned long changed_ = trk->apply_dem_data(skip_existing_elevations);
	/* Inform user how much was changed. */
	char str[64];
	const char * tmp_str = ngettext("%ld point adjusted", "%ld points adjusted", changed_);
	snprintf(str, 64, tmp_str, changed_);
	Dialog::info(str, this->get_window());
}




void LayerTRW::apply_dem_data_all_cb(void)
{
	LayersPanel * panel = this->get_window()->get_layers_panel();
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (!trk) {
		return;
	}

	this->apply_dem_data_common(panel, trk, false);
}




void LayerTRW::apply_dem_data_only_missing_cb(void)
{
	LayersPanel * panel = this->get_window()->get_layers_panel();
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (!trk) {
		return;
	}

	this->apply_dem_data_common(panel, trk, true);
}




/**
 * A common function for applying the elevation smoothing and reporting the results.
 */
void LayerTRW::smooth_it(Track * trk, bool flat)
{
	unsigned long changed_ = trk->smooth_missing_elevation_data(flat);
	/* Inform user how much was changed. */
	char str[64];
	const char * tmp_str = ngettext("%ld point adjusted", "%ld points adjusted", changed_);
	snprintf(str, 64, tmp_str, changed_);
	Dialog::info(str, this->get_window());
}




void LayerTRW::missing_elevation_data_interp_cb(void)
{
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (!trk) {
		return;
	}

	this->smooth_it(trk, false);
}




void LayerTRW::missing_elevation_data_flat_cb(void)
{
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (!trk) {
		return;
	}

	this->smooth_it(trk, true);
}




/**
 * Commonal helper function.
 */
void LayerTRW::wp_changed_message(int changed_)
{
	char str[64];
	const char * tmp_str = ngettext("%ld waypoint changed", "%ld waypoints changed", changed_);
	snprintf(str, 64, tmp_str, changed_);
	Dialog::info(str, this->get_window());
}




void LayerTRW::apply_dem_data_wpt_all_cb(void)
{
	LayersPanel * panel = this->get_window()->get_layers_panel();

	if (!this->dem_test(panel)) {
		return;
	}

	int changed_ = 0;
	if (this->menu_data->sublayer->type_id == "sg.trw.waypoint") {
		/* Single Waypoint. */
		sg_uid_t wp_uid = this->menu_data->sublayer->uid;
		Waypoint * wp = this->waypoints.items.at(wp_uid);
		if (wp) {
			changed_ = (int) wp->apply_dem_data(false);
		}
	} else {
		/* All waypoints. */
		for (auto i = this->waypoints.items.begin(); i != this->waypoints.items.end(); i++) {
			changed_ = changed_ + (int) i->second->apply_dem_data(false);
		}
	}
	this->wp_changed_message(changed_);
}




void LayerTRW::apply_dem_data_wpt_only_missing_cb(void)
{
	LayersPanel * panel = this->get_window()->get_layers_panel();

	if (!this->dem_test(panel)) {
		return;
	}

	int changed_ = 0;
	if (this->menu_data->sublayer->type_id == "sg.trw.waypoint") {
		/* Single Waypoint. */
		sg_uid_t wp_uid = this->menu_data->sublayer->uid;
		Waypoint * wp = this->waypoints.items.at(wp_uid);
		if (wp) {
			changed_ = (int) wp->apply_dem_data(true);
		}
	} else {
		/* All waypoints. */
		for (auto i = this->waypoints.items.begin(); i != this->waypoints.items.end(); i++) {
			changed_ = changed_ + (int) i->second->apply_dem_data(true);
		}
	}
	this->wp_changed_message(changed_);
}




void LayerTRW::goto_track_endpoint_cb(void)
{
	LayersPanel * panel = this->get_window()->get_layers_panel();
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (!trk) {
		return;
	}

	if (trk->empty()) {
		return;
	}
	goto_coord(panel, this, this->menu_data->viewport, trk->get_tp_last()->coord);
}




void LayerTRW::goto_track_max_speed_cb()
{
	LayersPanel * panel = this->get_window()->get_layers_panel();
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (!trk) {
		return;
	}

	Trackpoint * vtp = trk->get_tp_by_max_speed();
	if (!vtp) {
		return;
	}
	goto_coord(panel, this, this->menu_data->viewport, vtp->coord);
}




void LayerTRW::goto_track_max_alt_cb(void)
{
	LayersPanel * panel = this->get_window()->get_layers_panel();
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (!trk) {
		return;
	}

	Trackpoint* vtp = trk->get_tp_by_max_alt();
	if (!vtp) {
		return;
	}
	goto_coord(panel, this, this->menu_data->viewport, vtp->coord);
}




void LayerTRW::goto_track_min_alt_cb(void)
{
	LayersPanel * panel = this->get_window()->get_layers_panel();
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (!trk) {
		return;
	}

	Trackpoint* vtp = trk->get_tp_by_min_alt();
	if (!vtp) {
		return;
	}
	goto_coord(panel, this, this->menu_data->viewport, vtp->coord);
}




/*
 * Automatically change the viewport to center on the track and zoom to see the extent of the track.
 */
void LayerTRW::auto_track_view_cb(void)
{
	LayersPanel * panel = this->get_window()->get_layers_panel();
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (trk && !trk->empty()) {
		struct LatLon maxmin[2] = { {0,0}, {0,0} };
		trk->find_maxmin(maxmin);
		this->zoom_to_show_latlons(this->get_window()->get_viewport(), maxmin);
		if (panel) {
			panel->emit_update_window_cb();
		} else {
			this->emit_changed();
		}
	}
}




/*
 * Refine the selected track/route with a routing engine.
 * The routing engine is selected by the user, when requestiong the job.
 */
void LayerTRW::route_refine_cb(void)
{
	static int last_engine = 0;
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (trk && !trk->empty()) {
		/* Check size of the route */
		int nb = trk->get_tp_count();
#ifdef K
		if (nb > 100) {
			GtkWidget *dialog = gtk_message_dialog_new(this->get_window(),
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
								this->get_window(),
								(GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
								GTK_STOCK_CANCEL,
								GTK_RESPONSE_REJECT,
								GTK_STOCK_OK,
								GTK_RESPONSE_ACCEPT,
								NULL);
		QLabel * label = new QLabel(QObject::tr("Select routing engine"));
		gtk_widget_show_all(label);

		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), label, true, true, 0);

		QComboBox * combo = routing_ui_selector_new((Predicate)vik_routing_engine_supports_refine, NULL);
		combo->setCurrentIndex(last_engine);
		gtk_widget_show_all(combo);

		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), combo, true, true, 0);

		gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

		if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
			/* Dialog validated: retrieve selected engine and do the job */
			last_engine = combo->currentIndex();
			RoutingEngine *routing = routing_ui_selector_get_nth(combo, last_engine);

			/* Change cursor */
			this->get_window()->set_busy_cursor();

			/* Force saving track */
			/* FIXME: remove or rename this hack */
			this->route_finder_check_added_track = true;

			/* the job */
			routing->refine(this, trk);

			/* FIXME: remove or rename this hack */
			if (this->route_finder_added_track) {
				this->route_finder_added_track->calculate_bounds();
			}

			this->route_finder_added_track = NULL;
			this->route_finder_check_added_track = false;

			this->emit_changed();

			/* Restore cursor */
			this->get_window()->clear_busy_cursor();
		}
		gtk_widget_destroy(dialog);
#endif
	}
}




void LayerTRW::edit_trackpoint_cb(void)
{
	this->trackpoint_properties_show();
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
/* Not actively used - can be restored if needed. */
#if 0
static int track_compare(gconstpointer a, gconstpointer b, void * user_data)
{
	GHashTable *tracks = user_data;

	time_t t1 = ((Trackpoint *) ((Track *) g_hash_table_lookup(tracks, a))->trackpoints->data)->timestamp;
	time_t t2 = ((Trackpoint *) ((Track *) g_hash_table_lookup(tracks, b))->trackpoints->data)->timestamp;

	if (t1 < t2) return -1;
	if (t1 > t2) return 1;
	return 0;
}
#endif




/**
 * Comparison function which can be used to sort tracks or waypoints by name.
 */
int sort_alphabetically(gconstpointer a, gconstpointer b, void * user_data)
{
	const char* namea = (const char*) a;
	const char* nameb = (const char*) b;
	if (namea == NULL || nameb == NULL) {
		return 0;
	} else {
		/* Same sort method as used in the vik_treeview_*_alphabetize functions. */
		return strcmp(namea, nameb);
	}
}




/**
 * Attempt to merge selected track with other tracks specified by the user
 * Tracks to merge with must be of the same 'type' as the selected track -
 *  either all with timestamps, or all without timestamps
 */
void LayerTRW::merge_with_other_cb(void)
{
	sg_uid_t child_uid = this->menu_data->sublayer->uid;
	LayerTRWTracks * ght_tracks = NULL;
	if (this->menu_data->sublayer->type_id == "sg.trw.route") {
		ght_tracks = &this->routes;
	} else {
		ght_tracks = &this->tracks;
	}

	Track * trk = ght_tracks->items.at(child_uid);

	if (!trk) {
		return;
	}

	if (trk->empty()) {
		return;
	}

	/* with_timestamps: allow merging with 'similar' time type time tracks
	   i.e. either those times, or those without */
	bool with_timestamps = trk->get_tp_first()->has_timestamp;
	std::list<sg_uid_t> * other_tracks = ght_tracks->find_tracks_with_timestamp_type(with_timestamps, trk);

	if (other_tracks->empty()) {
		if (with_timestamps) {
			Dialog::error(tr("Failed. No other tracks with timestamps in this layer found"), this->get_window());
		} else {
			Dialog::error(tr("Failed. No other tracks without timestamps in this layer found"), this->get_window());
		}
		delete other_tracks;
		return;
	}
	other_tracks->reverse();

	/* Convert into list of names for usage with dialog function.
	   TODO: Need to consider how to work best when we can have multiple tracks the same name... */
	std::list<QString> other_tracks_names;
	for (auto iter = other_tracks->begin(); iter != other_tracks->end(); iter++) {
		other_tracks_names.push_back(ght_tracks->items.at(*iter)->name);
	}

	/* Sort alphabetically for user presentation. */
	other_tracks_names.sort();

	const QStringList headers = { trk->type_id == "sg.trw.route" ? tr("Select route to merge with") : tr("Select track to merge with") };
	std::list<QString> merge_list = a_dialog_select_from_list(other_tracks_names,
								  true,
								  tr("Merge with..."),
								  headers,
								  this->get_window());
	delete other_tracks;

	if (merge_list.empty()) {
		qDebug() << "II: Layer TRW: merge track is empty";
		return;
	}

	for (auto iter = merge_list.begin(); iter != merge_list.end(); iter++) {
		Track * merge_track = NULL;
		if (trk->type_id == "sg.trw.route") {
			merge_track = this->routes.find_track_by_name(*iter);
		} else {
			merge_track = this->tracks.find_track_by_name(*iter);
		}

		if (merge_track) {
			qDebug() << "II: Layer TRW: we have a merge track";
			trk->steal_and_append_trackpoints(merge_track);
			if (trk->type_id == "sg.trw.route") {
				this->delete_route(merge_track);
			} else {
				this->delete_track(merge_track);
			}
			trk->sort(Trackpoint::compare_timestamps);
		}
	}
	this->emit_changed();
}




/**
 * Join - this allows combining 'tracks' and 'track routes'
 *  i.e. doesn't care about whether tracks have consistent timestamps
 * ATM can only append one track at a time to the currently selected track
 */
void LayerTRW::append_track_cb(void)
{
	Track * trk = NULL;
	LayerTRWTracks * ght_tracks = NULL;
	if (this->menu_data->sublayer->type_id == "sg.trw.route") {
		ght_tracks = &this->routes;
	} else {
		ght_tracks = &this->tracks;
	}

	sg_uid_t child_uid = this->menu_data->sublayer->uid;
	trk = ght_tracks->items.at(child_uid);

	if (!trk) {
		return;
	}

	/* Convert into list of names for usage with dialog function.
	   TODO: Need to consider how to work best when we can have multiple tracks the same name... */

	std::list<QString> other_tracks_names = ght_tracks->get_sorted_track_name_list_exclude_self(trk);

	/* Note the limit to selecting one track only.
	   This is to control the ordering of appending tracks, i.e. the selected track always goes after the current track
	   (otherwise with multiple select the ordering would not be controllable by the user - automatically being alphabetically). */
	const QStringList headers = { trk->type_id == "sg.trw.route" ? tr("Select the route to append after the current route") : tr("Select the track to append after the current track") };
	std::list<QString> append_list = a_dialog_select_from_list(other_tracks_names,
								   false,

								   trk->type_id == "sg.trw.route"
								   ? tr("Append Route")
								   : tr("Append Track"),
								   headers,
								   this->get_window());

	/* It's a list, but shouldn't contain more than one other track! */
	if (append_list.empty()) {
		return;
	}

	for (auto iter = append_list.begin(); iter != append_list.end(); iter++) {
		/* TODO: at present this uses the first track found by name,
		   which with potential multiple same named tracks may not be the one selected... */
		Track * append_track = NULL;
		if (trk->type_id == "sg.trw.route") {
			append_track = this->routes.find_track_by_name(*iter);
		} else {
			append_track = this->tracks.find_track_by_name(*iter);
		}

		if (append_track) {
			trk->steal_and_append_trackpoints(append_track);
			if (trk->type_id == "sg.trw.route") {
				this->delete_route(append_track);
			} else {
				this->delete_track(append_track);
			}
		}
	}

	this->emit_changed();
}




/**
 * Very similar to append_track_cb() for joining
 * but this allows selection from the 'other' list
 * If a track is selected, then is shows routes and joins the selected one
 * If a route is selected, then is shows tracks and joins the selected one
 */
void LayerTRW::append_other_cb(void)
{
	sg_uid_t child_uid = this->menu_data->sublayer->uid;

	LayerTRWTracks * ght_mykind = NULL;
	LayerTRWTracks * ght_others = NULL;
	if (this->menu_data->sublayer->type_id == "sg.trw.route") {
		ght_mykind = &this->routes;
		ght_others = &this->tracks;
	} else {
		ght_mykind = &this->tracks;
		ght_others = &this->routes;
	}

	Track * trk = ght_mykind->items.at(child_uid);

	if (!trk) {
		return;
	}

	/* Convert into list of names for usage with dialog function.
	   TODO: Need to consider how to work best when we can have multiple tracks the same name... */

	std::list<QString> const other_tracks_names = ght_others->get_sorted_track_name_list_exclude_self(trk);

	/* Note the limit to selecting one track only.
	   this is to control the ordering of appending tracks, i.e. the selected track always goes after the current track
	   (otherwise with multiple select the ordering would not be controllable by the user - automatically being alphabetically). */
	const QStringList headers = { trk->type_id == "sg.trw.route" ? tr("Select the track to append after the current route") : tr("Select the route to append after the current track") };
	std::list<QString> append_list = a_dialog_select_from_list(other_tracks_names,
								   false,

								   trk->type_id == "sg.trw.route"
								   ? tr("Append Track")
								   : tr("Append Route"),

								   headers,
								   this->get_window());

	if (append_list.empty()) {
		return;
	}

	/* It's a list, but shouldn't contain more than one other track!
	   TODO: verify that the list has only one member. The loop below would not be necessary anymore. */

	for (auto iter = append_list.begin(); iter != append_list.end(); iter++) {
		/* TODO: at present this uses the first track found by name,
		   which with potential multiple same named tracks may not be the one selected... */

		/* Get FROM THE OTHER TYPE list. */
		Track * append_track = NULL;
		if (trk->type_id == "sg.trw.route") {
			append_track = this->tracks.find_track_by_name(*iter);
		} else {
			append_track = this->routes.find_track_by_name(*iter);
		}

		if (append_track) {

			if (append_track->type_id != "sg.trw.route"
			    && ((append_track->get_segment_count() > 1)
				|| (append_track->get_average_speed() > 0.0))) {

				if (Dialog::yes_or_no(tr("Converting a track to a route removes extra track data such as segments, timestamps, etc...\nDo you want to continue?"), this->get_window())) {
					append_track->merge_segments();
					append_track->to_routepoints();
				} else {
					break;
				}
			}

			trk->steal_and_append_trackpoints(append_track);

			/* Delete copied which is FROM THE OTHER TYPE list. */
			if (trk->type_id == "sg.trw.route") {
				this->delete_track(append_track);
			} else {
				this->delete_route(append_track);
			}
		}
	}

	this->emit_changed();
}




/* Merge by segments. */
void LayerTRW::merge_by_segment_cb(void)
{
	sg_uid_t child_uid = this->menu_data->sublayer->uid;
	Track * trk = this->tracks.items.at(child_uid);
	unsigned int segments = trk->merge_segments();
	/* NB currently no need to redraw as segments not actually shown on the display.
	   However inform the user of what happened: */
	char str[64];
	const char *tmp_str = ngettext("%d segment merged", "%d segments merged", segments);
	snprintf(str, 64, tmp_str, segments);
	Dialog::info(str, this->get_window());
}




/* merge by time routine */
void LayerTRW::merge_by_timestamp_cb(void)
{
	sg_uid_t child_uid = this->menu_data->sublayer->uid;

	//time_t t1, t2;

	Track * orig_trk = this->tracks.items.at(child_uid);

	if (!orig_trk->empty()
	    && !orig_trk->get_tp_first()->has_timestamp) {
		Dialog::error(tr("Failed. This track does not have timestamp"), this->get_window());
		return;
	}

#ifdef K

	std::list<sg_uid_t> * tracks_with_timestamp = this->tracks.find_tracks_with_timestamp_type(true, orig_trk);
	tracks_with_timestamp = g_list_reverse(tracks_with_timestamp);

	if (!tracks_with_timestamp) {
		Dialog::error(tr("Failed. No other track in this layer has timestamp"), this->get_window());
		return;
	}
	g_list_free(tracks_with_timestamp);
#endif
	static uint32_t threshold_in_minutes = 1;
	if (!a_dialog_time_threshold(tr("Merge Threshold..."),
				     tr("Merge when time between tracks less than:"),
				     &threshold_in_minutes,
				     this->get_window())) {
		return;
	}
#ifdef K
	/* Keep attempting to merge all tracks until no merges within the time specified is possible. */
	bool attempt_merge = true;
	GList *nearby_tracks = NULL;

	while (attempt_merge) {

		/* Don't try again unless tracks have changed. */
		attempt_merge = false;

		/* kamilTODO: why call this here? Shouldn't we call this way earlier? */
		if (orig_trk->empty()) {
			return;
		}

		if (nearby_tracks) {
			g_list_free(nearby_tracks);
			nearby_tracks = NULL;
		}

		/* Get a list of adjacent-in-time tracks. */
		nearby_tracks = this->tracks.find_nearby_tracks_by_time(orig_trk, (threshold_in_minutes * 60));

		/* Merge them. */

		for (GList *l = nearby_tracks; l; l = g_list_next(l)) {
			/* remove trackpoints from merged track, delete track */
			orig_trk->steal_and_append_trackpoints(((Track *) l->data));
			this->delete_track(((Track *) l->data));

			/* Tracks have changed, therefore retry again against all the remaining tracks. */
			attempt_merge = true;
		}

		orig_trk->sort(Trackpoint::compare_timestamps);
	}

	g_list_free(nearby_tracks);

	this->emit_changed();
#endif
}




/**
 * Split a track at the currently selected trackpoint
 */
void LayerTRW::split_at_selected_trackpoint(const QString & item_type_id)
{
	if (!this->selected_tp.valid) {
		return;
	}

	if (this->selected_tp.iter == this->current_trk->begin()) {
		/* First TP in track. Don't split. This function shouldn't be called at all. */
		qDebug() << "WW: Layer TRW: attempting to split track on first tp";
		return;
	}

	if (this->selected_tp.iter == std::prev(this->current_trk->end())) {
		/* Last TP in track. Don't split. This function shouldn't be called at all. */
		qDebug() << "WW: Layer TRW: attempting to split track on last tp";
		return;
	}

	const QString uniq_name = this->new_unique_element_name(item_type_id, this->current_trk->name);
	if (!uniq_name.size()) {
		qDebug() << "EE: Layer TRW: failed to get unique track name when splitting" << this->current_trk->name;
		return;
	}

	/* Selected Trackpoint stays in old track, but its copy goes to new track too. */
	Trackpoint * selected_ = new Trackpoint(**this->selected_tp.iter);

	Track * new_track = new Track(*this->current_trk, std::next(this->selected_tp.iter), this->current_trk->end());
	new_track->push_front(selected_);

	this->current_trk->erase(std::next(this->selected_tp.iter), this->current_trk->end());
	this->current_trk->calculate_bounds(); /* Bounds of the selected track changed due to the split. */

	this->selected_tp.iter = new_track->begin();
	this->current_trk = new_track;
	this->current_trk->calculate_bounds();

	/* kamilTODO: how it's possible that a new track will already have an uid? */
	qDebug() << "II: Layer TRW: split track: uid of new track is" << new_track->uid;

	this->current_trk = new_track;

	new_track->set_name(uniq_name);

	this->add_track(new_track);

	this->emit_changed();
}




/* split by time routine */
void LayerTRW::split_by_timestamp_cb(void)
{
	LayersPanel * panel = this->get_window()->get_layers_panel();
	sg_uid_t child_uid = this->menu_data->sublayer->uid;
	Track * trk = this->tracks.items.at(child_uid);

	static uint32_t thr = 1;

	if (trk->empty()) {
		return;
	}

	if (!a_dialog_time_threshold(tr("Split Threshold..."),
				     tr("Split when time between trackpoints exceeds:"),
				     &thr,
				     this->get_window())) {
		return;
	}

	/* Iterate through trackpoints, and copy them into new lists without touching original list. */
	auto iter = trk->trackpoints.begin();
	time_t prev_ts = (*iter)->timestamp;

	TrackPoints * newtps = new TrackPoints;
	std::list<TrackPoints *> points;

	for (; iter != trk->trackpoints.end(); iter++) {
		time_t ts = (*iter)->timestamp;

		/* Check for unordered time points - this is quite a rare occurence - unless one has reversed a track. */
		if (ts < prev_ts) {
			char tmp_str[64];
			strftime(tmp_str, sizeof(tmp_str), "%c", localtime(&ts));

			if (Dialog::yes_or_no(tr("Can not split track due to trackpoints not ordered in time - such as at %1.\n\nGoto this trackpoint?").arg(QString(tmp_str))), this->get_window()) {
				goto_coord(panel, this, this->menu_data->viewport, (*iter)->coord);
			}
			return;
		}

		if (ts - prev_ts > thr * 60) {
			/* Flush accumulated trackpoints into new list. */
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
		this->create_new_tracks(trk, &points);
	}

	/* Trackpoints are copied to new tracks, but lists of the Trackpoints need to be deallocated. */
	for (auto iter2 = points.begin(); iter2 != points.end(); iter2++) {
		delete *iter2;
	}

	return;
}




/**
 * Split a track by the number of points as specified by the user
 */
void LayerTRW::split_by_n_points_cb(void)
{
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (!trk || trk->empty()) {
		return;
	}

	int n_points = Dialog::get_int(tr("Split Every Nth Point"),
				       tr("Split on every Nth point:"),
				       250,   /* Default value as per typical limited track capacity of various GPS devices. */
				       2,     /* Min */
				       65536, /* Max */
				       5,     /* Step */
				       NULL,  /* ok */
				       this->get_window());
	/* Was a valid number returned? */
	if (!n_points) {
		return;
	}

	/* Now split. */
	TrackPoints * newtps = new TrackPoints;
	std::list<TrackPoints *> points;

	int count = 0;

	for (auto iter = trk->trackpoints.begin(); iter != trk->trackpoints.end(); iter++) {
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
		this->create_new_tracks(trk, &points);
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
	QString new_tr_name;
	for (auto iter = points->begin(); iter != points->end(); iter++) {

		Track * copy = new Track(*orig, (*iter)->begin(), (*iter)->end());

		if (orig->type_id == "sg.trw.route") {
			new_tr_name = this->new_unique_element_name("sg.trw.route", orig->name);
			copy->set_name(new_tr_name);
			this->add_route(copy);
		} else {
			new_tr_name = this->new_unique_element_name("sg.trw.track", orig->name);
			copy->set_name(new_tr_name);
			this->add_track(copy);
		}
		copy->calculate_bounds();
	}

	/* Remove original track and then update the display. */
	if (orig->type_id == "sg.trw.route") {
		this->delete_route(orig);
	} else {
		this->delete_track(orig);
	}
	this->emit_changed();

	return true;
}




/**
 * Split a track at the currently selected trackpoint
 */
void LayerTRW::split_at_trackpoint_cb(void)
{
	this->split_at_selected_trackpoint(this->menu_data->sublayer->type_id);
}




/**
 * Split a track by its segments
 * Routes do not have segments so don't call this for routes
 */
void LayerTRW::split_segments_cb(void)
{
	sg_uid_t child_uid = this->menu_data->sublayer->uid;
	Track *trk = this->tracks.items.at(child_uid);

	if (!trk) {
		return;
	}

	QString new_tr_name;
	std::list<Track *> * tracks_ = trk->split_into_segments();
	for (auto iter = tracks_->begin(); iter != tracks_->end(); iter++) {
		if (*iter) {
			new_tr_name = this->new_unique_element_name("sg.trw.track", trk->name);
			(*iter)->set_name(new_tr_name);

			this->add_track(*iter);
		}
	}
	if (tracks_) {
		delete tracks_;
		/* Remove original track. */
		this->delete_track(trk);
		this->emit_changed();
	} else {
		Dialog::error(tr("Can not split track as it has no segments"), this->get_window());
	}
}
/* end of split/merge routines */




void LayerTRW::trackpoint_selected_delete(Track * trk)
{
	TrackPoints::iterator new_tp_iter = trk->delete_trackpoint(this->selected_tp.iter);

	if (new_tp_iter != trk->end()) {
		/* Set to current to the available adjacent trackpoint. */
		this->selected_tp.iter = new_tp_iter;

		if (this->current_trk) {
			this->current_trk->calculate_bounds();
		}
	} else {
		this->cancel_current_tp(false);
	}
}




/**
 * Delete the selected point
 */
void LayerTRW::delete_point_selected_cb(void)
{
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (!trk) {
		return;
	}

	if (!this->selected_tp.valid) {
		return;
	}

	this->trackpoint_selected_delete(trk);

	/* Track has been updated so update tps: */
	this->cancel_tps_of_track(trk);

	this->emit_changed();
}




/**
 * Delete adjacent track points at the same position
 * AKA Delete Dulplicates on the Properties Window
 */
void LayerTRW::delete_points_same_position_cb(void)
{
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (!trk) {
		return;
	}

	unsigned long removed = trk->remove_dup_points();

	/* Track has been updated so update tps: */
	this->cancel_tps_of_track(trk);

	/* Inform user how much was deleted as it's not obvious from the normal view. */
	char str[64];
	const char *tmp_str = ngettext("Deleted %ld point", "Deleted %ld points", removed);
	snprintf(str, 64, tmp_str, removed);
	Dialog::info(str, this->get_window());

	this->emit_changed();
}




/**
 * Delete adjacent track points with the same timestamp
 * Normally new tracks that are 'routes' won't have any timestamps so should be OK to clean up the track
 */
void LayerTRW::delete_points_same_time_cb(void)
{
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (!trk) {
		return;
	}

	unsigned long removed = trk->remove_same_time_points();

	/* Track has been updated so update tps: */
	this->cancel_tps_of_track(trk);

	/* Inform user how much was deleted as it's not obvious from the normal view. */
	char str[64];
	const char *tmp_str = ngettext("Deleted %ld point", "Deleted %ld points", removed);
	snprintf(str, 64, tmp_str, removed);
	Dialog::info(str, this->get_window());

	this->emit_changed();
}




/**
 * Insert a point
 */
void LayerTRW::insert_point_after_cb(void)
{
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (!trk) {
		return;
	}

	this->insert_tp_beside_current_tp(false);

	this->emit_changed();
}




void LayerTRW::insert_point_before_cb(void)
{
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (!trk) {
		return;
	}

	this->insert_tp_beside_current_tp(true);

	this->emit_changed();
}




/**
 * Reverse a track
 */
void LayerTRW::reverse_cb(void)
{
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (!trk) {
		return;
	}

	trk->reverse();

	this->emit_changed();
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
		Dialog::error(tr("Could not launch %1 to open file.").arg(QString(diary_program)), this->get_window());
		g_error_free(err);
	}
	free(cmd);
}




/**
 * Open a diary at the date of the track or waypoint
 */
void LayerTRW::diary_cb(void)
{
	sg_uid_t child_uid = this->menu_data->sublayer->uid;

	if (this->menu_data->sublayer->type_id == "sg.trw.track") {
		Track * trk = this->tracks.items.at(child_uid);
		if (!trk) {
			return;
		}

		char date_buf[20];
		date_buf[0] = '\0';
		if (!trk->empty() && (*trk->trackpoints.begin())->has_timestamp) {
			strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", gmtime(&(*trk->trackpoints.begin())->timestamp));
			this->diary_open(date_buf);
		} else {
			Dialog::info(tr("This track has no date information."), this->get_window());
		}
	} else if (this->menu_data->sublayer->type_id == "sg.trw.waypoint") {
		Waypoint * wp = this->waypoints.items.at(uid);
		if (!wp) {
			return;
		}

		char date_buf[20];
		date_buf[0] = '\0';
		if (wp->has_timestamp) {
			strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", gmtime(&(wp->timestamp)));
			this->diary_open(date_buf);
		} else {
			Dialog::info(tr("This waypoint has no date information."), this->get_window());
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
		Dialog::error(tr("Could not launch %1").arg(QString(astro_program)), this->get_window());
		fprintf(stderr, "WARNING: %s\n", err->message);
		g_error_free(err);
	}
	util_add_to_deletion_list(tmp);
	free(tmp);
	free(cmd);
}




/*
  Format of stellarium lat & lon seems designed to be particularly awkward
  who uses ' & " in the parameters for the command line?!
  -1d4'27.48"
  +53d58'16.65"
*/
static char *convert_to_dms(double dec)
{
	char sign_c = ' ';
	if (dec > 0) {
		sign_c = '+';
	} else if (dec < 0) {
		sign_c = '-';
	} else { /* Nul value. */
		sign_c = ' ';
	}

	/* Degrees. */
	double tmp = fabs(dec);
	int val_d = (int)tmp;

	/* Minutes. */
	tmp = (tmp - val_d) * 60;
	int val_m = (int)tmp;

	/* Seconds. */
	double val_s = (tmp - val_m) * 60;

	/* Format. */
	char * result = g_strdup_printf("%c%dd%d\\\'%.4f\\\"", sign_c, val_d, val_m, val_s);
	return result;
}




/**
 * Open an astronomy program at the date & position of the track center, trackpoint or waypoint
 */
void LayerTRW::astro_cb(void)
{
	sg_uid_t child_uid = this->menu_data->sublayer->uid;

	if (this->menu_data->sublayer->type_id == "sg.trw.track") {
		Track * trk = this->tracks.items.at(child_uid);
		if (!trk) {
			return;
		}

		Trackpoint * tp = NULL;
		if (this->selected_tp.valid) {
			/* Current trackpoint. */
			tp = *this->selected_tp.iter;

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
			struct LatLon ll = tp->coord.get_latlon();
			char *lat_str = convert_to_dms(ll.lat);
			char *lon_str = convert_to_dms(ll.lon);
			char alt_buf[20];
			snprintf(alt_buf, sizeof(alt_buf), "%d", (int)round(tp->altitude));
			this->astro_open(date_buf, time_buf, lat_str, lon_str, alt_buf);
			free(lat_str);
			free(lon_str);
		} else {
			Dialog::info(tr("This track has no date information."), this->get_window());
		}
	} else if (this->menu_data->sublayer->type_id == "sg.trw.waypoint") {
		sg_uid_t wp_uid = this->menu_data->sublayer->uid;
		Waypoint * wp = this->waypoints.items.at(wp_uid);
		if (!wp) {
			return;
		}

		if (wp->has_timestamp) {
			char date_buf[20];
			strftime(date_buf, sizeof(date_buf), "%Y%m%d", gmtime(&(wp->timestamp)));
			char time_buf[20];
			strftime(time_buf, sizeof(time_buf), "%H:%M:%S", gmtime(&(wp->timestamp)));
			struct LatLon ll = wp->coord.get_latlon();
			char *lat_str = convert_to_dms(ll.lat);
			char *lon_str = convert_to_dms(ll.lon);
			char alt_buf[20];
			snprintf(alt_buf, sizeof(alt_buf), "%d", (int)round(wp->altitude));
			this->astro_open(date_buf, time_buf, lat_str, lon_str, alt_buf);
			free(lat_str);
			free(lon_str);
		} else {
			Dialog::info(tr("This waypoint has no date information."), this->get_window());
		}
	}
}




void LayerTRW::sort_order_specified(const QString & item_type_id, sort_order_t order)
{
	TreeIndex tree_index;

	if (item_type_id == "sg.trw.tracks") {
		tree_index = this->tracks.get_index();
		this->track_sort_order = order;

	} else if (item_type_id == "sg.trw.routes") {
		tree_index = this->routes.get_index();
		this->track_sort_order = order;

	} else { /* "sg.trw.waypoints" */
		tree_index = this->waypoints.get_index();
		this->wp_sort_order = order;
	}

	this->tree_view->sort_children(tree_index, order);
}




void LayerTRW::sort_order_a2z_cb(void)
{
	this->sort_order_specified(this->menu_data->sublayer->type_id, VL_SO_ALPHABETICAL_ASCENDING);
}




void LayerTRW::sort_order_z2a_cb(void)
{
	this->sort_order_specified(this->menu_data->sublayer->type_id, VL_SO_ALPHABETICAL_DESCENDING);
}




void LayerTRW::sort_order_timestamp_ascend_cb(void)
{
	this->sort_order_specified(this->menu_data->sublayer->type_id, VL_SO_DATE_ASCENDING);
}




void LayerTRW::sort_order_timestamp_descend_cb(void)
{
	this->sort_order_specified(this->menu_data->sublayer->type_id, VL_SO_DATE_DESCENDING);
}




void LayerTRW::delete_selected_tracks_cb(void) /* Slot. */
{
	LayersPanel * panel = this->get_window()->get_layers_panel();

	/* Ensure list of track names offered is unique. */
	QString duplicate_name = this->tracks.has_duplicate_track_names();
	if (duplicate_name != "") {
		if (Dialog::yes_or_no(tr("Multiple entries with the same name exist. This method only works with unique names. Force unique names now?")), this->get_window()) {
			this->tracks.uniquify(this->track_sort_order);

			/* Update. */
			panel->emit_update_window_cb();
		} else {
			return;
		}
	}

	/* Sort list alphabetically for better presentation. */
	std::list<QString> const all = this->tracks.get_sorted_track_name_list();

	if (all.empty()) {
		Dialog::error(tr("No tracks found"), this->get_window());
		return;
	}

	/* Get list of items to delete from the user. */
	const QStringList headers = { tr("Select tracks to delete") };
	std::list<QString> delete_list = a_dialog_select_from_list(all,
								   true,
								   tr("Delete Selection"),
								   headers,
								   this->get_window());

	if (delete_list.empty()) {
		return;
	}

	/* Delete requested tracks.
	   Since specificly requested, IMHO no need for extra confirmation. */

	for (auto iter = delete_list.begin(); iter != delete_list.end(); iter++) {
		/* This deletes first trk it finds of that name (but uniqueness is enforced above). */
		this->delete_track_by_name(*iter, false);
	}

	/* Reset layer timestamps in case they have now changed. */
	this->tree_view->set_timestamp(this->index, this->get_timestamp());

	this->emit_changed();
}




void LayerTRW::delete_selected_routes_cb(void) /* Slot. */
{
	LayersPanel * panel = this->get_window()->get_layers_panel();

	/* Ensure list of track names offered is unique. */
	QString duplicate_name = this->routes.has_duplicate_track_names();
	if (duplicate_name != "") {
		if (Dialog::yes_or_no(tr("Multiple entries with the same name exist. This method only works with unique names. Force unique names now?"), this->get_window())) {
			this->routes.uniquify(this->track_sort_order);

			/* Update. */
			panel->emit_update_window_cb();
		} else {
			return;
		}
	}

	/* Sort list alphabetically for better presentation. */
	std::list<QString> all = this->routes.get_sorted_track_name_list();

	if (all.empty()) {
		Dialog::error(tr("No routes found"), this->get_window());
		return;
	}

	/* Get list of items to delete from the user. */
	const QStringList headers = { tr("Select routes to delete") };
	std::list<QString> delete_list = a_dialog_select_from_list(all,
								   true,
								   tr("Delete Selection"),
								   headers,
								   this->get_window());

	/* Delete requested routes.
	   Since specifically requested, IMHO no need for extra confirmation. */
	if (delete_list.empty()) {
		return;
	}

	for (auto iter = delete_list.begin(); iter != delete_list.end(); iter++) {
		/* This deletes first route it finds of that name (but uniqueness is enforced above). */
		this->delete_track_by_name(*iter, true);
	}

	this->emit_changed();
}




void LayerTRW::delete_selected_waypoints_cb(void)
{
	/* Ensure list of waypoint names offered is unique. */
	QString duplicate_name = this->waypoints.has_duplicate_waypoint_names();
	if (duplicate_name != "") {
		if (Dialog::yes_or_no(tr("Multiple entries with the same name exist. This method only works with unique names. Force unique names now?"), this->get_window())) {
			this->waypoints.uniquify(this->wp_sort_order);

			LayersPanel * panel = this->get_window()->get_layers_panel();
			/* Update. */
			panel->emit_update_window_cb();
		} else {
			return;
		}
	}

	/* Sort list alphabetically for better presentation. */
	std::list<QString> all = this->waypoints.get_sorted_wp_name_list();
	if (all.empty()) {
		Dialog::error(tr("No waypoints found"), this->get_window());
		return;
	}

	/* Get list of items to delete from the user. */
	const QStringList headers = { tr("Select waypoints to delete") };
	std::list<QString> delete_list = a_dialog_select_from_list(all,
								   true,
								   tr("Delete Selection"),
								   headers,
								   this->get_window());

	if (delete_list.empty()) {
		return;
	}

	/* Delete requested waypoints.
	   Since specifically requested, IMHO no need for extra confirmation. */
	for (auto iter = delete_list.begin(); iter != delete_list.end(); iter++) {
		/* This deletes first waypoint it finds of that name (but uniqueness is enforced above). */
		this->delete_waypoint_by_name(*iter);
	}

	this->waypoints.calculate_bounds();
	/* Reset layer timestamp in case it has now changed. */
	this->tree_view->set_timestamp(this->index, this->get_timestamp());
	this->emit_changed();

}




void LayerTRW::waypoints_visibility_off_cb(void) /* Slot. */
{
	this->waypoints.set_items_visibility(false);
	/* Redraw. */
	this->emit_changed();
}




void LayerTRW::waypoints_visibility_on_cb(void) /* Slot. */
{
	this->waypoints.set_items_visibility(true);
	/* Redraw. */
	this->emit_changed();
}




void LayerTRW::waypoints_visibility_toggle_cb(void) /* Slot. */
{
	this->waypoints.toggle_items_visibility();
	/* Redraw. */
	this->emit_changed();
}




void LayerTRW::tracks_visibility_off_cb(void) /* Slot. */
{
	this->tracks.set_items_visibility(false);
	/* Redraw. */
	this->emit_changed();
}




void LayerTRW::tracks_visibility_on_cb(void) /* Slot. */
{
	this->tracks.set_items_visibility(true);
	/* Redraw. */
	this->emit_changed();
}




void LayerTRW::tracks_visibility_toggle_cb(void) /* Slot. */
{
	this->tracks.toggle_items_visibility();
	/* Redraw. */
	this->emit_changed();
}




void LayerTRW::routes_visibility_off_cb(void) /* Slot. */
{
	this->routes.set_items_visibility(false);
	/* Redraw. */
	this->emit_changed();
}




void LayerTRW::routes_visibility_on_cb() /* Slot. */
{
	this->routes.set_items_visibility(true);
	/* Redraw. */
	this->emit_changed();
}




void LayerTRW::routes_visibility_toggle_cb(void) /* Slot. */
{
	this->routes.toggle_items_visibility();
	/* Redraw. */
	this->emit_changed();
}




/**
 * Helper function to construct a list of #waypoint_layer_t.
 */
std::list<waypoint_layer_t *> * LayerTRW::create_waypoints_and_layers_list_helper(std::list<Waypoint *> * waypoints_)
{
	std::list<waypoint_layer_t *> * waypoints_and_layers = new std::list<waypoint_layer_t *>;
	/* Build waypoints_and_layers list. */
	for (auto iter = waypoints_->begin(); iter != waypoints_->end(); iter++) {
		waypoint_layer_t * element = (waypoint_layer_t *) malloc(sizeof (waypoint_layer_t));
		element->wp = *iter;
		element->trw = this;
		waypoints_and_layers->push_back(element);
	}

	return waypoints_and_layers;
}




/**
 * Create the latest list of waypoints with the associated layer(s).
 * Although this will always be from a single layer here.
 */
std::list<SlavGPS::waypoint_layer_t *> * LayerTRW::create_waypoints_and_layers_list()
{
	std::list<Waypoint *> pure_waypoints;

	for (auto iter = this->waypoints.items.begin(); iter != this->waypoints.items.end(); iter++) {
		pure_waypoints.push_back((*iter).second);
	}

	return this->create_waypoints_and_layers_list_helper(&pure_waypoints);
}




/**
 * Helper function to construct a list of #track_layer_t.
 */
std::list<track_layer_t *> * LayerTRW::create_tracks_and_layers_list_helper(std::list<Track *> * tracks_)
{
	/* Build tracks_and_layers list. */
	std::list<track_layer_t *> * tracks_and_layers = new std::list<track_layer_t *>;
	for (auto iter = tracks_->begin(); iter != tracks_->end(); iter++) {
		track_layer_t * element = (track_layer_t *) malloc(sizeof (track_layer_t));
		element->trk = *iter;
		element->trw = this;
		tracks_and_layers->push_back(element);
	}
	return tracks_and_layers;
}




/**
 * Create the latest list of tracks with the associated layer(s).
 * Although this will always be from a single layer here.
 */
std::list<track_layer_t *> * LayerTRW::create_tracks_and_layers_list(const QString & item_type_id)
{
	std::list<Track *> * tracks_ = new std::list<Track *>;
	if (item_type_id == "sg.trw.tracks") {
		tracks_ = this->tracks.get_track_values(tracks_);
	} else {
		tracks_ = this->routes.get_track_values(tracks_);
	}

	return this->create_tracks_and_layers_list_helper(tracks_);
}




void LayerTRW::tracks_stats_cb(void)
{
	layer_trw_show_stats(this->get_window(), this->name, this, "sg.trw.tracks");
}




void LayerTRW::routes_stats_cb(void)
{
	layer_trw_show_stats(this->get_window(), this->name, this, "sg.trw.routes");
}




void LayerTRW::go_to_selected_waypoint_cb(void)
{
	LayersPanel * panel = this->get_window()->get_layers_panel();
	sg_uid_t wp_uid = this->menu_data->sublayer->uid;
	Waypoint * wp = this->waypoints.items.at(wp_uid);
	if (wp) {
		goto_coord(panel, this, this->menu_data->viewport, wp->coord);
	}
}




void LayerTRW::waypoint_geocache_webpage_cb(void)
{
	sg_uid_t wp_uid = this->menu_data->sublayer->uid;
	Waypoint * wp = this->waypoints.items.at(wp_uid);
	if (!wp) {
		return;
	}
	const QString webpage = QString("http://www.geocaching.com/seek/cache_details.aspx?wp=%1").arg(wp->name);
	open_url(webpage);
}




void LayerTRW::waypoint_webpage_cb(void)
{
	sg_uid_t wp_uid = this->menu_data->sublayer->uid;
	Waypoint * wp = this->waypoints.items.at(wp_uid);
	if (!wp || !wp->has_any_url()) {
		return;
	}

	open_url(wp->get_any_url());
}




QString LayerTRW::sublayer_rename_request(TreeItem * sublayer, const QString & new_name, LayersPanel * panel)
{
	QString empty_string("");

	if (sublayer->type_id == "sg.trw.waypoint") {
		Waypoint * wp = this->waypoints.items.at(sublayer->uid);

		/* No actual change to the name supplied. */
		if (!wp->name.isEmpty()) {
			if (new_name == wp->name) {
				return empty_string;
			}
		}

		Waypoint * wpf = this->waypoints.find_waypoint_by_name(new_name);

		if (wpf) {
			/* An existing waypoint has been found with the requested name. */
			if (!Dialog::yes_or_no(tr("A waypoint with the name \"%1\" already exists. Really rename to the same name?").arg(new_name), this->get_window())) {
				return empty_string;
			}
		}

		/* Update WP name and refresh the treeview. */
		wp->set_name(new_name);

		this->tree_view->set_name(sublayer->index, new_name);
		this->tree_view->sort_children(this->waypoints.get_index(), this->wp_sort_order);

		panel->emit_update_window_cb();

		return new_name;
	}

	if (sublayer->type_id == "sg.trw.track") {
		Track * trk = this->tracks.items.at(sublayer->uid);

		/* No actual change to the name supplied. */
		if (trk->name.size()) {
			if (new_name == trk->name) {
				return empty_string;
			}
		}

		Track * trkf = this->tracks.find_track_by_name(new_name);
		if (trkf) {
			/* An existing track has been found with the requested name. */
			if (!Dialog::yes_or_no(tr("A track with the name \"%1\" already exists. Really rename to the same name?").arg(new_name), this->get_window())) {
				return empty_string;
			}
		}
		/* Update track name and refresh GUI parts. */
		trk->set_name(new_name);

		/* Update any subwindows that could be displaying this track which has changed name.
		   Only one Track Edit Window. */
		if (this->current_trk == trk && this->tpwin) {
			this->tpwin->set_track_name(new_name);
		}

		/* Update the dialog windows if any of them is visible. */
		trk->update_properties_dialog();
		trk->update_profile_dialog();

		this->tree_view->set_name(sublayer->index, new_name);
		this->tree_view->sort_children(this->tracks.get_index(), this->track_sort_order);

		panel->emit_update_window_cb();

		return new_name;
	}

	if (sublayer->type_id == "sg.trw.route") {
		Track * trk = this->routes.items.at(sublayer->uid);

		/* No actual change to the name supplied. */
		if (trk->name.size()) {
			if (new_name == trk->name) {
				return empty_string;
			}
		}

		Track * trkf = this->routes.find_track_by_name(new_name);
		if (trkf) {
			/* An existing track has been found with the requested name. */
			if (!Dialog::yes_or_no(tr("A route with the name \"%1\" already exists. Really rename to the same name?").arg(new_name), this->get_window())) {
				return empty_string;
			}
		}
		/* Update track name and refresh GUI parts. */
		trk->set_name(new_name);

		/* Update any subwindows that could be displaying this track which has changed name.
		   Only one Track Edit Window. */
		if (this->current_trk == trk && this->tpwin) {
			this->tpwin->set_track_name(new_name);
		}

		/* Update the dialog windows if any of them is visible. */
		trk->update_properties_dialog();
		trk->update_profile_dialog();

		this->tree_view->set_name(sublayer->index, new_name);
		this->tree_view->sort_children(this->tracks.get_index(), this->track_sort_order);

		panel->emit_update_window_cb();

		return new_name;
	}

	return empty_string;
}




bool is_valid_geocache_name(const char *str)
{
	int len = strlen(str);
	return len >= 3 && len <= 7 && str[0] == 'G' && str[1] == 'C' && isalnum(str[2]) && (len < 4 || isalnum(str[3])) && (len < 5 || isalnum(str[4])) && (len < 6 || isalnum(str[5])) && (len < 7 || isalnum(str[6]));
}




#ifndef WINDOWS
void LayerTRW::track_use_with_filter_cb(void)
{
	sg_uid_t child_uid = this->menu_data->sublayer->uid;
	Track * trk = this->tracks.items.at(child_uid);
	a_acquire_set_filter_track(trk);
}
#endif




#ifdef VIK_CONFIG_GOOGLE


bool LayerTRW::is_valid_google_route(sg_uid_t track_uid)
{
	Track * trk = this->routes.items.at(track_uid);
	return (trk && trk->comment.size() > 7 && !strncmp(trk->comment.toUtf8().constData(), "from:", 5));
}




void LayerTRW::google_route_webpage_cb(void)
{
	sg_uid_t child_uid = this->menu_data->sublayer->uid;
	Track * trk = this->routes.items.at(child_uid);
	if (trk) {
		char *escaped = uri_escape(trk->comment.toUtf8().data());
		QString webpage = QString("http://maps.google.com/maps?f=q&hl=en&q=%1").arg(escaped);
		open_url(webpage);
		free(escaped);
	}
}

#endif




/* TODO: Probably better to rework this track manipulation in viktrack.c. */
void LayerTRW::insert_tp_beside_current_tp(bool before)
{
	/* Sanity check. */
	if (!this->selected_tp.valid) {
		return;
	}

	Trackpoint * tp_current = *this->selected_tp.iter;
	Trackpoint * tp_other = NULL;

	if (before) {
		if (this->selected_tp.iter == this->current_trk->begin()) {
			return;
		}
		tp_other = *std::prev(this->selected_tp.iter);
	} else {
		if (std::next(this->selected_tp.iter) == this->current_trk->end()) {
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

		if (this->current_trk) {
			this->current_trk->insert(tp_current, tp_new, before);
		} else {
			/* TODO: under which conditions this ::insert_tp_beside_current_tp() would be called and ->current_trk would be NULL? */
		}
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
			delete this->tpwin;
			this->tpwin = NULL;
		} else {
			this->tpwin->set_empty();
		}
	}

	if (this->selected_tp.valid) {
		this->selected_tp.valid = false;

		this->current_trk = NULL;
		this->emit_changed();
	}
}




void LayerTRW::my_tpwin_set_tp()
{
	Track * trk = this->current_trk;
	/* Notional center of a track is simply an average of the bounding box extremities. */
	struct LatLon ll_center = { (trk->bbox.north+trk->bbox.south)/2, (trk->bbox.east+trk->bbox.west)/2 };
	Coord coord(ll_center, this->coord_mode);  /* kamilTODO: this variable is unused. */
	this->tpwin->set_tp(trk, &this->selected_tp.iter, trk->name, trk->type_id == "sg.trw.route");
}




void LayerTRW::trackpoint_properties_cb(int response) /* Slot. */
{
	assert (this->tpwin != NULL);
	if (response == SG_TRACK_CLOSE) {
		this->cancel_current_tp(true);
		//this->tpwin->reject();
	}

	if (!this->selected_tp.valid) {
		return;
	}

	if (response == SG_TRACK_SPLIT
	    && this->selected_tp.iter != this->current_trk->begin()
	    && std::next(this->selected_tp.iter) != this->current_trk->end()) {

		this->split_at_selected_trackpoint(this->current_trk->type_id);
		this->my_tpwin_set_tp();

	} else if (response == SG_TRACK_DELETE) {

		if (!this->current_trk) {
			return;
		}
		this->trackpoint_selected_delete(this->current_trk);

		if (this->selected_tp.valid) {
			/* Reset dialog with the available adjacent trackpoint. */
			this->my_tpwin_set_tp();
		}

		this->emit_changed();

	} else if (response == SG_TRACK_FORWARD
		   && this->current_trk
		   && std::next(this->selected_tp.iter) != this->current_trk->end()) {

		this->selected_tp.iter++;
		this->my_tpwin_set_tp();
		this->emit_changed(); /* TODO longone: either move or only update if tp is inside drawing window */

	} else if (response == SG_TRACK_BACK
		   && this->current_trk
		   && this->selected_tp.iter != this->current_trk->begin()) {

		this->selected_tp.iter--;
		this->my_tpwin_set_tp();
		this->emit_changed();

	} else if (response == SG_TRACK_INSERT
		   && this->current_trk
		   && std::next(this->selected_tp.iter) != this->current_trk->end()) {

		this->insert_tp_beside_current_tp(false);
		this->emit_changed();

	} else if (response == SG_TRACK_CHANGED) {
		this->emit_changed();
	}
}




/**
 * @vertical: The reposition strategy. If Vertical moves dialog vertically, otherwise moves it horizontally
 *
 * Try to reposition a dialog if it's over the specified coord
 *  so to not obscure the item of interest
 */
void LayerTRW::dialog_shift(QDialog * dialog, Coord * coord, bool vertical)
{
	Window * parent_window = this->get_window(); /* i.e. the main window. */

	int win_pos_x = parent_window->x();
	int win_pos_y = parent_window->y();
	int win_size_x = parent_window->width();
	int win_size_y = parent_window->height();

	int dia_pos_x = dialog->x();
	int dia_pos_y = dialog->y();
	int dia_size_x = dialog->width();
	int dia_size_y = dialog->height();


	/* Dialog not 'realized'/positioned - so can't really do any repositioning logic. */
	if (dia_pos_x <= 2 || dia_pos_y <= 2) {
		qDebug() << "WW: Layer TRW: can't position dialog window";
		return;
	}

	Viewport * viewport = this->get_window()->get_viewport();

	int vp_xx, vp_yy; /* In viewport pixels. */
	viewport->coord_to_screen(coord, &vp_xx, &vp_yy);

#ifdef K
	/* Work out the 'bounding box' in pixel terms of the dialog and only move it when over the position. */

	int dest_x = 0;
	int dest_y = 0;
	if (!gtk_widget_translate_coordinates(viewport->get_widget(), GTK_WIDGET(parent_window), 0, 0, &dest_x, &dest_y)) {
		return;
	}

	/* Transform Viewport pixels into absolute pixels. */
	int tmp_xx = vp_xx + dest_x + win_pos_x - 10;
	int tmp_yy = vp_yy + dest_y + win_pos_y - 10;

	/* Is dialog over the point (to within an  ^^ edge value). */
	if ((tmp_xx > dia_pos_x) && (tmp_xx < (dia_pos_x + dia_size_x))
	    && (tmp_yy > dia_pos_y) && (tmp_yy < (dia_pos_y + dia_size_y))) {

		if (vertical) {
			/* Shift up<->down. */
			int hh = viewport->get_height();

			/* Consider the difference in viewport to the full window. */
			int offset_y = dest_y;
			/* Add difference between dialog and window sizes. */
			offset_y += win_pos_y + (hh/2 - dia_size_y)/2;

			if (vp_yy > hh/2) {
				/* Point in bottom half, move window to top half. */
				gtk_window_move(dialog, dia_pos_x, offset_y);
			} else {
				/* Point in top half, move dialog down. */
				gtk_window_move(dialog, dia_pos_x, hh/2 + offset_y);
			}
		} else {
			/* Shift left<->right. */
			int ww = viewport->get_width();

			/* Consider the difference in viewport to the full window. */
			int offset_x = dest_x;
			/* Add difference between dialog and window sizes. */
			offset_x += win_pos_x + (ww/2 - dia_size_x)/2;

			if (vp_xx > ww/2) {
				/* Point on right, move window to left. */
				gtk_window_move(dialog, offset_x, dia_pos_y);
			} else {
				/* Point on left, move right. */
				gtk_window_move(dialog, ww/2 + offset_x, dia_pos_y);
			}
		}
	}
#endif
	return;
}




void LayerTRW::trackpoint_properties_show()
{
	if (!this->tpwin) {
		this->tpwin = new PropertiesDialogTP(this->get_window());
		//connect(this->tpwin, SIGNAL (changed(void)), this, SLOT (trackpoint_properties_cb(void)));

		connect(this->tpwin->signalMapper, SIGNAL (mapped(int)), this, SLOT (trackpoint_properties_cb(int)));

		//QObject::connect(this->tpwin, SIGNAL("delete-event"), this, SLOT (trw_layer_cancel_current_tp_cb));
	}
	this->tpwin->show();


	if (this->selected_tp.valid) {
		/* Get tp pixel position. */
		Trackpoint * tp = *this->selected_tp.iter;
		/* Shift up/down to try not to obscure the trackpoint. */
		this->dialog_shift(this->tpwin, &tp->coord, true);
	}


	if (this->selected_tp.valid) {
		if (this->current_trk) {
			this->my_tpwin_set_tp();
		}
	}
	/* Set layer name and TP data. */
}





static int create_thumbnails_thread(BackgroundJob * bg_job);



/* Structure for thumbnail creating data used in the background thread. */
class ThumbnailCreator : public BackgroundJob {
public:
	ThumbnailCreator(LayerTRW * layer, QStringList * pics_);
	~ThumbnailCreator();

	LayerTRW * layer = NULL;  /* Layer needed for redrawing. */
	QStringList * pics = NULL;     /* Image list. */
};




ThumbnailCreator::ThumbnailCreator(LayerTRW * layer_, QStringList * pics_)
{
	this->thread_fn = create_thumbnails_thread;
	this->n_items = pics_->size();

	layer = layer_;
	pics = pics_;
}




static int create_thumbnails_thread(BackgroundJob * bg_job)
{
	ThumbnailCreator * creator = (ThumbnailCreator *) bg_job;

	unsigned int total = creator->pics->size();
	unsigned int done = 0;
	while (creator->pics) {
#ifdef K
		a_thumbnails_create((char *) creator->pics->data);
		int result = a_background_thread_progress(threaddata, ((double) ++done) / total);
		if (result != 0) {
			return -1; /* Abort thread. */
		}

		creator->pics = creator->pics->next;
#endif
	}

	/* Redraw to show the thumbnails as they are now created. */
	if (creator->layer) {
		creator->layer->emit_changed(); /* NB update from background thread. */
	}

	return 0;
}




ThumbnailCreator::~ThumbnailCreator()
{
	delete this->pics;
}




void LayerTRW::verify_thumbnails(void)
{
	if (this->has_verified_thumbnails) {
		return;
	}

	QStringList * pics = this->waypoints.image_wp_make_list();
	int len = pics->size();
	if (!len) {
		delete pics;
		return;
	}

	const QString job_description = QString(tr("Creating %1 Image Thumbnails...")).arg(len);
	ThumbnailCreator * creator = new ThumbnailCreator(this, pics);
	a_background_thread(creator, ThreadPoolType::LOCAL, job_description);
}




void LayerTRW::sort_all()
{
	if (!this->tree_view) {
		return;
	}

	/* Obviously need 2 to tango - sorting with only 1 (or less) is a lonely activity! */
	if (this->tracks.items.size() > 1) {
		this->tree_view->sort_children(this->tracks.get_index(), this->track_sort_order);
	}

	if (this->routes.items.size() > 1) {
		this->tree_view->sort_children(this->routes.get_index(), this->track_sort_order);
	}

	if (this->waypoints.items.size() > 1) {
		this->tree_view->sort_children(this->waypoints.get_index(), this->wp_sort_order);
	}
}




/**
 * Get the earliest timestamp available for this layer.
 */
time_t LayerTRW::get_timestamp()
{
	time_t timestamp_tracks = this->tracks.get_earliest_timestamp();
	time_t timestamp_waypoints = this->waypoints.get_earliest_timestamp();
	/* NB routes don't have timestamps - hence they are not considered. */

	if (!timestamp_tracks && !timestamp_waypoints) {
		/* Fallback to get time from the metadata when no other timestamps available. */
		GTimeVal gtv;
		if (this->metadata
		    && this->metadata->has_timestamp
		    && !this->metadata->timestamp.isEmpty()
		    && g_time_val_from_iso8601(this->metadata->timestamp.toUtf8().constData(), &gtv)) {
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
	if (this->connected_to_tree) {
		this->verify_thumbnails();
	}
	this->tracks.assign_colors(this->track_drawing_mode, this->track_color_common);
	this->routes.assign_colors(-1, this->track_color_common);

	this->waypoints.calculate_bounds();
	this->tracks.calculate_bounds();
	this->routes.calculate_bounds();


	/*
	  Apply treeview sort after loading all the tracks for this
	  layer (rather than sorted insert on each individual track
	  additional) and after subsequent changes to the properties
	  as the specified order may have changed.  since the sorting
	  of a treeview section is now very quick.  NB sorting is also
	  performed after every name change as well to maintain the
	  list order.
	*/
	this->sort_all();

	/* Setting metadata time if not otherwise set. */
	if (this->metadata) {

		bool need_to_set_time = true;
		if (this->metadata->has_timestamp) {
			need_to_set_time = false;
			if (!this->metadata->timestamp.isEmpty()) {
				need_to_set_time = true;
			}
		}

		if (need_to_set_time) {
			GTimeVal timestamp;
			timestamp.tv_usec = 0;
			timestamp.tv_sec = this->get_timestamp();

			/* No time found - so use 'now' for the metadata time. */
			if (timestamp.tv_sec == 0) {
				g_get_current_time(&timestamp);
			}

			this->metadata->timestamp = QString(g_time_val_to_iso8601(&timestamp));
		}
	}
}




CoordMode LayerTRW::get_coord_mode()
{
	return this->coord_mode;
}




/**
 * Uniquify the whole layer.
 * Also requires the layers panel as the names shown there need updating too.
 * Returns whether the operation was successful or not.
 */
bool LayerTRW::uniquify(LayersPanel * panel)
{
	if (panel) {
		this->tracks.uniquify(this->track_sort_order);
		this->routes.uniquify(this->track_sort_order);
		this->waypoints.uniquify(this->wp_sort_order);

		/* Update. */
		panel->emit_update_window_cb();

		return true;
	}
	return false;
}




void LayerTRW::change_coord_mode(CoordMode dest_mode)
{
	if (this->coord_mode != dest_mode) {
		this->coord_mode = dest_mode;
		this->waypoints.change_coord_mode(dest_mode);
		this->tracks.change_coord_mode(dest_mode);
		this->routes.change_coord_mode(dest_mode);
	}
}




void LayerTRW::set_menu_selection(LayerMenuItem selection)
{
	this->menu_selection = selection;
}




LayerMenuItem LayerTRW::get_menu_selection()
{
	return this->menu_selection;
}




/* ----------- Downloading maps along tracks --------------- */

static int get_download_area_width(double zoom_level, struct LatLon *wh) /* kamilFIXME: viewport is unused, why? */
{
	/* TODO: calculating based on current size of viewport. */
	const double w_at_zoom_0_125 = 0.0013;
	const double h_at_zoom_0_125 = 0.0011;
	double zoom_factor = zoom_level/0.125;

	wh->lat = h_at_zoom_0_125 * zoom_factor;
	wh->lon = w_at_zoom_0_125 * zoom_factor;

	return 0;   /* All OK. */
}




static Coord * get_next_coord(Coord *from, Coord *to, struct LatLon *dist, double gradient)
{
	if ((dist->lon >= ABS(to->ll.lon - from->ll.lon))
	    && (dist->lat >= ABS(to->ll.lat - from->ll.lat))) {

		return NULL;
	}

	Coord * coord = new Coord();
	coord->mode = CoordMode::LATLON;

	if (ABS(gradient) < 1) {
		if (from->ll.lon > to->ll.lon) {
			coord->ll.lon = from->ll.lon - dist->lon;
		} else {
			coord->ll.lon = from->ll.lon + dist->lon;
		}
		coord->ll.lat = gradient * (coord->ll.lon - from->ll.lon) + from->ll.lat;
	} else {
		if (from->ll.lat > to->ll.lat) {
			coord->ll.lat = from->ll.lat - dist->lat;
		} else {
			coord->ll.lat = from->ll.lat + dist->lat;
		}
		coord->ll.lon = (1/gradient) * (coord->ll.lat - from->ll.lat) + from->ll.lat;
	}

	return coord;
}




static GList * add_fillins(GList *list, Coord * from, Coord * to, struct LatLon *dist)
{
	/* TODO: handle vertical track (to->ll.lon - from->ll.lon == 0). */
	double gradient = (to->ll.lat - from->ll.lat)/(to->ll.lon - from->ll.lon);

	Coord * next = from;
	while (true) {
		if ((next = get_next_coord(next, to, dist, gradient)) == NULL) {
			break;
		}
		list = g_list_prepend(list, next);
	}

	return list;
}




void vik_track_download_map(Track *tr, LayerMap * layer_map, double zoom_level)
{
	struct LatLon wh;
	if (get_download_area_width(zoom_level, &wh)) {
		return;
	}

	if (tr->empty()) {
		return;
	}
#ifdef K
	std::list<Rect *> * rects_to_download = tr->get_rectangles(&wh);

	GList * fillins = NULL;

	/* 'fillin' doesn't work in UTM mode - potentially ending up in massive loop continually allocating memory - hence don't do it. */
	/* Seems that ATM the function get_next_coord works only for LATLON. */
	if (tr->get_coord_mode() == CoordMode::LATLON) {

		/* Fill-ins for far apart points. */
		std::list<Rect *>::iterator cur_rect;
		std::list<Rect *>::iterator next_rect;

		for (cur_rect = rects_to_download->begin();
		     (next_rect = std::next(cur_rect)) != rects_to_download->end();
		     cur_rect++) {

			if ((wh.lon < ABS ((*cur_rect)->center.ll.lon - (*next_rect)->center.ll.lon))
			    || (wh.lat < ABS ((*cur_rect)->center.ll.lat - (*next_rect)->center.ll.lat))) {

				fillins = add_fillins(fillins, &(*cur_rect)->center, &(*next_rect)->center, &wh);
			}
		}
	} else {
		qDebug() << "WW: Layer TRW: 'download map' feature works only in Mercator mode";
	}

	if (fillins) {
		Coord tl, br;
		for (GList * fiter = fillins; fiter; fiter = fiter->next) {
			Coord * cur_coord = (Coord *)(fiter->data);
			cur_coord->set_area(&wh, &tl, &br);
			Rect * rect = (Rect *) malloc(sizeof (Rect));
			rect->tl = tl;
			rect->br = br;
			rect->center = *cur_coord;
			rects_to_download->push_front(rect);
		}
	}

	for (auto rect_iter = rects_to_download->begin(); rect_iter != rects_to_download->end(); rect_iter++) {
		layer_map->download_section(&(*rect_iter)->tl, &(*rect_iter)->br, zoom_level);
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
#endif
}




void LayerTRW::download_map_along_track_cb(void)
{
	const QStringList zoom_labels = { "0.125", "0.25", "0.5", "1", "2", "4", "8", "16", "32", "64", "128", "256", "512", "1024" };
	std::vector<double> zoom_values = { 0.125, 0.25, 0.5, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024 };

	LayersPanel * panel = this->get_window()->get_layers_panel();

	Track * trk = this->get_track_helper(this->menu_data->sublayer);
	if (!trk) {
		return;
	}

	const Viewport * viewport = this->get_window()->get_viewport();

	std::list<Layer const *> * layers = panel->get_all_layers_of_type(LayerType::MAP, true); /* Includes hidden map layer types. */
	int num_maps = layers->size();
	if (!num_maps) {
		Dialog::error(tr("No map layer in use. Create one first"), this->get_window());
		return;
	}

	/* Convert from list of layers to list of names. Allowing the user to select one of them. */

	std::list<LayerMap *> map_layers;
	QStringList map_labels; /* List of names of map layers that are currently used. */

	for (auto iter = layers->begin(); iter != layers->end(); iter++) {
		map_layers.push_back((LayerMap *) *iter); /* kamilFIXME: casting const pointer to non-const. */
		map_labels << ((LayerMap *) *iter)->get_map_label();
	}

	double cur_zoom = viewport->get_zoom();
	unsigned int default_zoom_idx;
	for (default_zoom_idx = 0; default_zoom_idx < zoom_values.size(); default_zoom_idx++) {
		if (cur_zoom == zoom_values[default_zoom_idx]) {
			break;
		}
	}
	default_zoom_idx = default_zoom_idx == zoom_values.size() ? (zoom_values.size() - 1) : default_zoom_idx;

	unsigned int selected_map_idx = 0;
	unsigned int selected_zoom_idx = 0;
	if (!a_dialog_map_and_zoom(map_labels, 0, zoom_labels, default_zoom_idx, &selected_map_idx, &selected_zoom_idx, this->get_window())) {
		delete layers;
		return;
	}

	auto iter = map_layers.begin();
	for (unsigned int i = 0; i < selected_map_idx; i++) {
		iter++;
	}

	vik_track_download_map(trk, *iter, zoom_values[selected_zoom_idx]);

	delete layers;
	return;
}




/* Lowest waypoint number calculation. */
static int highest_wp_number_name_to_number(const QString & name)
{
	if (name.size() == 3) {
		int n = atoi(name.toUtf8().constData()); /* TODO: Use QString method. */
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




void LayerTRW::highest_wp_number_add_wp(const QString & new_wp_name)
{
	/* If is bigger that top, add it. */
	int new_wp_num = highest_wp_number_name_to_number(new_wp_name);
	if (new_wp_num > this->highest_wp_number) {
		this->highest_wp_number = new_wp_num;
	}
}




void LayerTRW::highest_wp_number_remove_wp(const QString & old_wp_name)
{
	/* If wasn't top, do nothing. if was top, count backwards until we find one used. */
	int old_wp_num = highest_wp_number_name_to_number(old_wp_name);
	if (this->highest_wp_number == old_wp_num) {
		char buf[4];
		this->highest_wp_number--;

		snprintf(buf,4,"%03d", this->highest_wp_number);
		/* Search down until we find something that *does* exist. */

		while (this->highest_wp_number > 0 && !this->waypoints.find_waypoint_by_name(buf)) {
		       this->highest_wp_number--;
		       snprintf(buf, 4, "%03d", this->highest_wp_number);
		}
	}
}




/* Get lowest unused number. */
QString LayerTRW::highest_wp_number_get()
{
	QString result;
	if (this->highest_wp_number < 0 || this->highest_wp_number >= 999) { /* TODO: that's rather limiting, isn't it? */
		return result;
	}
	result = QString("%1").arg((int) (this->highest_wp_number + 1), 3, 10, (QChar) '0');
	return result;
}




/**
 * Create the latest list of tracks and routes.
 */
std::list<track_layer_t *> * LayerTRW::create_tracks_and_layers_list()
{
	std::list<Track *> * tracks_ = new std::list<Track *>;
	tracks_ = this->tracks.get_track_values(tracks_);
	tracks_ = this->routes.get_track_values(tracks_);
	return this->create_tracks_and_layers_list_helper(tracks_);
}




void LayerTRW::track_list_dialog_single_cb(void) /* Slot. */
{
	QString title;
	if (this->menu_data->sublayer->type_id == "sg.trw.tracks") {
		title = tr("%1: Track List").arg(this->name);
	} else {
		title = tr("%1: Route List").arg(this->name);
	}
	track_list_dialog(title, this, this->menu_data->sublayer->type_id, false);
}




void LayerTRW::track_list_dialog_cb(void)
{
	QString title = tr("%1: Track and Route List").arg(this->name);
	track_list_dialog(title, this, "", false);
}




void LayerTRW::waypoint_list_dialog_cb(void) /* Slot. */
{
	QString title = tr("%1: Waypoint List").arg(this->name);
	waypoint_list_dialog(title, this, false);
}




Track * LayerTRW::get_track_helper(TreeItem * sublayer)
{
	/* TODO: either get rid of this method, or reduce it to checking
	   of consistency between function argument and contents of this->table.at(). */
	if (sublayer->type_id == "sg.trw.route") {
		return this->routes.items.at(sublayer->uid);
	} else {
		return this->tracks.items.at(sublayer->uid);
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
	strcpy(this->debug_string, "TRW");
	this->interface = &vik_trw_layer_interface;

	memset(&coord_mode, 0, sizeof (CoordMode));

	this->image_cache = g_queue_new(); /* Must be performed before set_params via set_initial_parameter_values. */

	this->set_initial_parameter_values();

	/* Param settings that are not available via the GUI. */
	/* Force to on after processing params (which defaults them to off with a zero value). */
	this->waypoints.visible = this->tracks.visible = this->routes.visible = true;

	this->metadata = new TRWMetadata();
	this->draw_sync_done = true;
	this->draw_sync_do = true;
	/* Everything else is 0, false or NULL. */

	this->set_name(Layer::get_type_ui_label(this->type));


#ifdef K
	pango_layout_set_font_description(this->wplabellayout, gtk_widget_get_style(viewport->font_desc));
	pango_layout_set_font_description(this->tracklabellayout, gtk_widget_get_style(viewport->font_desc));
#endif

	this->new_track_pens();

	this->wp_marker_pen = QPen(this->wp_marker_color);
	this->wp_marker_pen.setWidth(2);

	this->wp_label_fg_pen = QPen(this->wp_label_fg_color);
	this->wp_label_fg_pen.setWidth(1);

	this->wp_label_bg_pen = QPen(this->wp_label_bg_color);
	this->wp_label_bg_pen.setWidth(1);
#ifdef K
	gdk_gc_set_function(this->waypoint_bg_gc, this->wpbgand);
#endif


	this->menu_selection = this->interface->menu_items_selection;

#if 0
	rv->waypoints = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) Waypoint::delete_waypoint);
	rv->tracks = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) Track::delete_track);
	rv->routes = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) Track::delete_track);
#endif

}




/* To be called right after constructor. */
void LayerTRW::set_coord_mode(CoordMode mode)
{
	this->coord_mode = mode;
}
