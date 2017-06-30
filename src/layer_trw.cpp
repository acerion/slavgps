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
#include "layer_trw_draw.h"
#include "layer_trw_tools.h"
#include "layer_map.h"
#include "tree_view_internal.h"
#include "vikutils.h"
#include "track_properties_dialog.h"
#include "waypoint_properties.h"
#include "waypoint_list.h"
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
#include "uibuilder.h"
#include "layers_panel.h"
#include "preferences.h"
#include "util.h"


#include "layer_gps.h"
#include "layer_trw_export.h"
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
#include "vikexttools.h"
#include "vikexttool_datasources.h"
#include "ui_util.h"
#include "clipboard.h"
#include "routing.h"
#include "icons/icons.h"
#endif

#include "gpspoint.h"



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



static void goto_coord(LayersPanel * panel, Layer * layer, Viewport * viewport, const VikCoord * coord);

static void trw_layer_cancel_current_tp_cb(LayerTRW * layer, bool destroy);

static char * font_size_to_string(int font_size);




/****** PARAMETERS ******/

static char *params_groups[] = { (char *) N_("Waypoints"), (char *) N_("Tracks"), (char *) N_("Waypoint Images"), (char *) N_("Tracks Advanced"), (char *) N_("Metadata") };
enum { GROUP_WAYPOINTS, GROUP_TRACKS, GROUP_IMAGES, GROUP_TRACKS_ADV, GROUP_METADATA };

static char *params_drawmodes[] = { (char *) N_("Draw by Track"), (char *) N_("Draw by Speed"), (char *) N_("All Tracks Same Color"), NULL };
static char *params_wpsymbols[] = { (char *) N_("Filled Square"), (char *) N_("Square"), (char *) N_("Circle"), (char *) N_("X"), 0 };

#define MIN_POINT_SIZE 2
#define MAX_POINT_SIZE 10

#define MIN_ARROW_SIZE 3
#define MAX_ARROW_SIZE 20

static ParameterScale params_scales[] = {
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

/* Needs to align with vik_layer_sort_order_t. */
static char* params_sort_order[] = {
	(char *) N_("None"),
	(char *) N_("Name Ascending"),
	(char *) N_("Name Descending"),
	(char *) N_("Date Ascending"),
	(char *) N_("Date Descending"),
	NULL
};

static ParameterValue black_color_default(void)       { return ParameterValue(0, 0, 0, 100); } /* Black. */
static ParameterValue drawmode_default(void)          { return ParameterValue((uint32_t) DRAWMODE_BY_TRACK); }
static ParameterValue line_thickness_default(void)    { return ParameterValue((uint32_t) 1); }
static ParameterValue trkpointsize_default(void)      { return ParameterValue((uint32_t) MIN_POINT_SIZE); }
static ParameterValue trkdirectionsize_default(void)  { return ParameterValue((uint32_t) 5); }
static ParameterValue bg_line_thickness_default(void) { return ParameterValue((uint32_t) 0); }
static ParameterValue trackbgcolor_default(void)      { return ParameterValue(255, 255, 255, 100); }  /* White. */
static ParameterValue elevation_factor_default(void)  { return ParameterValue((uint32_t) 30); }
static ParameterValue stop_length_default(void)       { return ParameterValue((uint32_t) 60); }
static ParameterValue speed_factor_default(void)      { return ParameterValue(30.0); }
static ParameterValue tnfontsize_default(void)        { return ParameterValue((uint32_t) FS_MEDIUM); }
static ParameterValue wpfontsize_default(void)        { return ParameterValue((uint32_t) FS_MEDIUM); }
static ParameterValue wptextcolor_default(void)       { return ParameterValue(255, 255, 255, 100); } /* White. */
static ParameterValue wpbgcolor_default(void)         { return ParameterValue(0x83, 0x83, 0xc4, 100); } /* Kind of Blue. kamilTODO: verify the hex values. */
static ParameterValue wpsize_default(void)            { return ParameterValue((uint32_t) 4); }
static ParameterValue wpsymbol_default(void)          { return ParameterValue((uint32_t) WP_SYMBOL_FILLED_SQUARE); }
static ParameterValue image_size_default(void)        { return ParameterValue((uint32_t) 64); }
static ParameterValue image_alpha_default(void)       { return ParameterValue((uint32_t) 255); }
static ParameterValue image_cache_size_default(void)  { return ParameterValue((uint32_t) 300); }
static ParameterValue sort_order_default(void)        { return ParameterValue((uint32_t) 0); }
static ParameterValue string_default(void)            { return ParameterValue(""); }




/* ENUMERATION MUST BE IN THE SAME ORDER AS THE NAMED PARAMS ABOVE. */
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


Parameter trw_layer_params[] = {
	{ PARAM_TV,         "tracks_visible",    ParameterType::BOOLEAN, VIK_LAYER_NOT_IN_PROPERTIES, NULL,                              WidgetType::NONE,         NULL,               NULL, NULL, vik_lpd_true_default,       NULL, NULL },
	{ PARAM_WV,         "waypoints_visible", ParameterType::BOOLEAN, VIK_LAYER_NOT_IN_PROPERTIES, NULL,                              WidgetType::NONE,         NULL,               NULL, NULL, vik_lpd_true_default,       NULL, NULL },
	{ PARAM_RV,         "routes_visible",    ParameterType::BOOLEAN, VIK_LAYER_NOT_IN_PROPERTIES, NULL,                              WidgetType::NONE,         NULL,               NULL, NULL, vik_lpd_true_default,       NULL, NULL },

	{ PARAM_TDL,        "trackdrawlabels",   ParameterType::BOOLEAN, GROUP_TRACKS,                N_("Draw Labels"),                 WidgetType::CHECKBUTTON,  NULL,               NULL, N_("Note: the individual track controls what labels may be displayed"), vik_lpd_true_default, NULL, NULL },
	{ PARAM_TLFONTSIZE, "trackfontsize",     ParameterType::UINT,    GROUP_TRACKS_ADV,            N_("Track Labels Font Size:"),     WidgetType::COMBOBOX,     params_font_sizes,  NULL, NULL, tnfontsize_default,         NULL, NULL },
	{ PARAM_DM,         "drawmode",          ParameterType::UINT,    GROUP_TRACKS,                N_("Track Drawing Mode:"),         WidgetType::COMBOBOX,     params_drawmodes,   NULL, NULL, drawmode_default,           NULL, NULL },
	{ PARAM_TC,         "trackcolor",        ParameterType::COLOR,   GROUP_TRACKS,                N_("All Tracks Color:"),           WidgetType::COLOR,        NULL,               NULL, N_("The color used when 'All Tracks Same Color' drawing mode is selected"), black_color_default, NULL, NULL },
	{ PARAM_DL,         "drawlines",         ParameterType::BOOLEAN, GROUP_TRACKS,                N_("Draw Track Lines"),            WidgetType::CHECKBUTTON,  NULL,               NULL, NULL, vik_lpd_true_default,       NULL, NULL },
	{ PARAM_LT,         "line_thickness",    ParameterType::UINT,    GROUP_TRACKS_ADV,            N_("Track Thickness:"),            WidgetType::SPINBUTTON,   &params_scales[0],  NULL, NULL, line_thickness_default,     NULL, NULL },
	{ PARAM_DD,         "drawdirections",    ParameterType::BOOLEAN, GROUP_TRACKS,                N_("Draw Track Direction"),        WidgetType::CHECKBUTTON,  NULL,               NULL, NULL, vik_lpd_false_default,      NULL, NULL },
	{ PARAM_DDS,        "trkdirectionsize",  ParameterType::UINT,    GROUP_TRACKS_ADV,            N_("Direction Size:"),             WidgetType::SPINBUTTON,   &params_scales[11], NULL, NULL, trkdirectionsize_default,   NULL, NULL },
	{ PARAM_DP,         "drawpoints",        ParameterType::BOOLEAN, GROUP_TRACKS,                N_("Draw Trackpoints"),            WidgetType::CHECKBUTTON,  NULL,               NULL, NULL, vik_lpd_true_default,       NULL, NULL },
	{ PARAM_DPS,        "trkpointsize",      ParameterType::UINT,    GROUP_TRACKS_ADV,            N_("Trackpoint Size:"),            WidgetType::SPINBUTTON,   &params_scales[10], NULL, NULL, trkpointsize_default,       NULL, NULL },
	{ PARAM_DE,         "drawelevation",     ParameterType::BOOLEAN, GROUP_TRACKS,                N_("Draw Elevation"),              WidgetType::CHECKBUTTON,  NULL,               NULL, NULL, vik_lpd_false_default,      NULL, NULL },
	{ PARAM_EF,         "elevation_factor",  ParameterType::UINT,    GROUP_TRACKS_ADV,            N_("Draw Elevation Height %:"),    WidgetType::HSCALE,       &params_scales[9],  NULL, NULL, elevation_factor_default,   NULL, NULL },
	{ PARAM_DS,         "drawstops",         ParameterType::BOOLEAN, GROUP_TRACKS,                N_("Draw Stops"),                  WidgetType::CHECKBUTTON,  NULL,               NULL, N_("Whether to draw a marker when trackpoints are at the same position but over the minimum stop length apart in time"), vik_lpd_false_default, NULL, NULL },
	{ PARAM_SL,         "stop_length",       ParameterType::UINT,    GROUP_TRACKS_ADV,            N_("Min Stop Length (seconds):"),  WidgetType::SPINBUTTON,   &params_scales[8],  NULL, NULL, stop_length_default,        NULL, NULL },

	{ PARAM_BLT,        "bg_line_thickness", ParameterType::UINT,    GROUP_TRACKS_ADV,            N_("Track BG Thickness:"),         WidgetType::SPINBUTTON,   &params_scales[6],  NULL, NULL, bg_line_thickness_default,  NULL, NULL },
	{ PARAM_TBGC,       "trackbgcolor",      ParameterType::COLOR,   GROUP_TRACKS_ADV,            N_("Track Background Color"),      WidgetType::COLOR,        NULL,               NULL, NULL, trackbgcolor_default,       NULL, NULL },
	{ PARAM_TDSF,       "speed_factor",      ParameterType::DOUBLE,  GROUP_TRACKS_ADV,            N_("Draw by Speed Factor (%):"),   WidgetType::HSCALE,       &params_scales[1],  NULL, N_("The percentage factor away from the average speed determining the color used"), speed_factor_default, NULL, NULL },
	{ PARAM_TSO,        "tracksortorder",    ParameterType::UINT,    GROUP_TRACKS_ADV,            N_("Track Sort Order:"),           WidgetType::COMBOBOX,     params_sort_order,  NULL, NULL, sort_order_default,         NULL, NULL },

	{ PARAM_DLA,        "drawlabels",        ParameterType::BOOLEAN, GROUP_WAYPOINTS,             N_("Draw Labels"),                 WidgetType::CHECKBUTTON,  NULL,               NULL, NULL, vik_lpd_true_default,       NULL, NULL },
	{ PARAM_WPFONTSIZE, "wpfontsize",        ParameterType::UINT,    GROUP_WAYPOINTS,             N_("Waypoint Font Size:"),         WidgetType::COMBOBOX,     params_font_sizes,  NULL, NULL, wpfontsize_default,         NULL, NULL },
	{ PARAM_WPC,        "wpcolor",           ParameterType::COLOR,   GROUP_WAYPOINTS,             N_("Waypoint Color:"),             WidgetType::COLOR,        NULL,               NULL, NULL, black_color_default,        NULL, NULL },
	{ PARAM_WPTC,       "wptextcolor",       ParameterType::COLOR,   GROUP_WAYPOINTS,             N_("Waypoint Text:"),              WidgetType::COLOR,        NULL,               NULL, NULL, wptextcolor_default,        NULL, NULL },
	{ PARAM_WPBC,       "wpbgcolor",         ParameterType::COLOR,   GROUP_WAYPOINTS,             N_("Background:"),                 WidgetType::COLOR,        NULL,               NULL, NULL, wpbgcolor_default,          NULL, NULL },
	{ PARAM_WPBA,       "wpbgand",           ParameterType::BOOLEAN, GROUP_WAYPOINTS,             N_("Fake BG Color Translucency:"), WidgetType::CHECKBUTTON,  NULL,               NULL, NULL, vik_lpd_false_default,      NULL, NULL },
	{ PARAM_WPSYM,      "wpsymbol",          ParameterType::UINT,    GROUP_WAYPOINTS,             N_("Waypoint marker:"),            WidgetType::COMBOBOX,     params_wpsymbols,   NULL, NULL, wpsymbol_default,           NULL, NULL },
	{ PARAM_WPSIZE,     "wpsize",            ParameterType::UINT,    GROUP_WAYPOINTS,             N_("Waypoint size:"),              WidgetType::SPINBUTTON,   &params_scales[7],  NULL, NULL, wpsize_default,             NULL, NULL },
	{ PARAM_WPSYMS,     "wpsyms",            ParameterType::BOOLEAN, GROUP_WAYPOINTS,             N_("Draw Waypoint Symbols:"),      WidgetType::CHECKBUTTON,  NULL,               NULL, NULL, vik_lpd_true_default,       NULL, NULL },
	{ PARAM_WPSO,       "wpsortorder",       ParameterType::UINT,    GROUP_WAYPOINTS,             N_("Waypoint Sort Order:"),        WidgetType::COMBOBOX,     params_sort_order,  NULL, NULL, sort_order_default,         NULL, NULL },

	{ PARAM_DI,         "drawimages",        ParameterType::BOOLEAN, GROUP_IMAGES,                N_("Draw Waypoint Images"),        WidgetType::CHECKBUTTON,  NULL,               NULL, NULL, vik_lpd_true_default,       NULL, NULL },
	{ PARAM_IS,         "image_size",        ParameterType::UINT,    GROUP_IMAGES,                N_("Image Size (pixels):"),        WidgetType::HSCALE,       &params_scales[3],  NULL, NULL, image_size_default,         NULL, NULL },
	{ PARAM_IA,         "image_alpha",       ParameterType::UINT,    GROUP_IMAGES,                N_("Image Alpha:"),                WidgetType::HSCALE,       &params_scales[4],  NULL, NULL, image_alpha_default,        NULL, NULL },
	{ PARAM_ICS,        "image_cache_size",  ParameterType::UINT,    GROUP_IMAGES,                N_("Image Memory Cache Size:"),    WidgetType::HSCALE,       &params_scales[5],  NULL, NULL, image_cache_size_default,   NULL, NULL },

	{ PARAM_MDDESC,     "metadatadesc",      ParameterType::STRING,  GROUP_METADATA,              N_("Description"),                 WidgetType::ENTRY,        NULL,               NULL, NULL, string_default,             NULL, NULL },
	{ PARAM_MDAUTH,     "metadataauthor",    ParameterType::STRING,  GROUP_METADATA,              N_("Author"),                      WidgetType::ENTRY,        NULL,               NULL, NULL, string_default,             NULL, NULL },
	{ PARAM_MDTIME,     "metadatatime",      ParameterType::STRING,  GROUP_METADATA,              N_("Creation Time"),               WidgetType::ENTRY,        NULL,               NULL, NULL, string_default,             NULL, NULL },
	{ PARAM_MDKEYS,     "metadatakeywords",  ParameterType::STRING,  GROUP_METADATA,              N_("Keywords"),                    WidgetType::ENTRY,        NULL,               NULL, NULL, string_default,             NULL, NULL },

	{ NUM_PARAMS,       NULL,                ParameterType::PTR,     VIK_LAYER_GROUP_NONE,        NULL,                              WidgetType::NONE,         NULL,               NULL, NULL, NULL,               NULL, NULL }, /* Guard. */
};


/*** TO ADD A PARAM:
 *** 1) Add to trw_layer_params and enumeration
 *** 2) Handle in get_param & set_param (presumably adding on to LayerTRW class)
 ***/

/****** END PARAMETERS ******/




LayerTRWInterface vik_trw_layer_interface;




LayerTRWInterface::LayerTRWInterface()
{
	this->params = trw_layer_params;        /* Parameters. */
	this->params_count = NUM_PARAMS;
	this->params_groups = params_groups;    /* Parameter groups. */

	strncpy(this->layer_type_string, "TrackWaypoint", sizeof (this->layer_type_string) - 1); /* Non-translatable. */
	this->layer_type_string[sizeof (this->layer_type_string) - 1] = '\0';

	this->layer_name = QObject::tr("TrackWaypoint");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_Y;
	// this->action_icon = ...; /* Set elsewhere. */

	this->layer_tool_constructors.insert({{ LAYER_TRW_TOOL_CREATE_WAYPOINT, tool_new_waypoint_create          }});
	this->layer_tool_constructors.insert({{ LAYER_TRW_TOOL_CREATE_TRACK,    tool_new_track_create             }});
	this->layer_tool_constructors.insert({{ LAYER_TRW_TOOL_CREATE_ROUTE,    tool_new_route_create             }});
	this->layer_tool_constructors.insert({{ LAYER_TRW_TOOL_ROUTE_FINDER,    tool_extended_route_finder_create }});
	this->layer_tool_constructors.insert({{ LAYER_TRW_TOOL_EDIT_WAYPOINT,   tool_edit_waypoint_create         }});
	this->layer_tool_constructors.insert({{ LAYER_TRW_TOOL_EDIT_TRACKPOINT, tool_edit_trackpoint_create       }});
	this->layer_tool_constructors.insert({{ LAYER_TRW_TOOL_SHOW_PICTURE,    tool_show_picture_create          }});

	this->menu_items_selection = VIK_MENU_ITEM_ALL;
}




bool have_diary_program = false;
char *diary_program = NULL;
#define VIK_SETTINGS_EXTERNAL_DIARY_PROGRAM "external_diary_program"

bool have_geojson_export = false;

bool have_astro_program = false;
char *astro_program = NULL;
#define VIK_SETTINGS_EXTERNAL_ASTRO_PROGRAM "external_astro_program"




void color_to_param(ParameterValue & value, QColor const & color)
{
	value.c.r = color.red();
	value.c.g = color.green();
	value.c.b = color.blue();
	value.c.a = color.alpha();
}



void param_to_color(QColor & color, ParameterValue const & value)
{
	color = QColor(value.c.r, value.c.g, value.c.b, value.c.a);
}




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
		have_diary_program = true;
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
		have_astro_program = true;
	}
	if (g_find_program_in_path(astro_program)) {
		have_astro_program = true;
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
 * Find a track by date.
 */
bool LayerTRW::find_track_by_date(char const * date_str, VikCoord * position, Viewport * viewport, bool select)
{
	Track * trk = LayerTRWc::find_track_by_date(this->tracks, date_str);
	if (trk && select) {
		struct LatLon maxmin[2] = { {0,0}, {0,0} };
		LayerTRW::find_maxmin_in_track(trk, maxmin);
		this->zoom_to_show_latlons(viewport, maxmin);
		this->tree_view->select_and_expose(trk->index);
		this->emit_changed();
	}
	return (bool) trk;
}


/**
 * Find a waypoint by date.
 */
bool LayerTRW::find_waypoint_by_date(char const * date_str, VikCoord * position, Viewport * viewport, bool select)
{
	Waypoint * wp = LayerTRWc::find_waypoint_by_date(this->waypoints, date_str);
	if (wp && select) {
		viewport->set_center_coord(&wp->coord, true);
		this->tree_view->select_and_expose(wp->index);
		this->emit_changed();
	}
	return (bool) wp;
}




void LayerTRW::delete_sublayer(Sublayer * sublayer)
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




void LayerTRW::cut_sublayer(Sublayer * sublayer)
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
	Sublayer * sublayer = this->menu_data->sublayer;
	uint8_t *data_ = NULL;
	unsigned int len;

	this->copy_sublayer(sublayer, &data_, &len);

#ifdef K
	if (data_) {
		const char* name = NULL;
		if (sublayer->type == SublayerType::WAYPOINT) {
			Waypoint * wp = this->waypoints.at(sublayer->uid);
			if (wp && wp->name) {
				name = wp->name;
			} else {
				name = NULL; // Broken :(
			}
		} else if (sublayer->type == SublayerType::TRACK) {
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




void LayerTRW::copy_sublayer(Sublayer * sublayer, uint8_t **item, unsigned int *len)
{
	if (!sublayer) {
		qDebug() << "WW: Layer TRW: 'copy sublayer' received NULL sublayer";
		*item = NULL;
		return;
	}

	uint8_t *id;
	size_t il;

	GByteArray *ba = g_byte_array_new();

	if (sublayer->sublayer_type == SublayerType::WAYPOINT) {
		this->waypoints.at(sublayer->uid)->marshall(&id, &il);
	} else if (sublayer->sublayer_type == SublayerType::TRACK) {
		this->tracks.at(sublayer->uid)->marshall(&id, &il);
	} else {
		this->routes.at(sublayer->uid)->marshall(&id, &il);
	}

	g_byte_array_append(ba, id, il);

	std::free(id);

	*len = ba->len;
	*item = ba->data;
}




bool LayerTRW::paste_sublayer(Sublayer * sublayer, uint8_t * item, size_t len)
{
	if (!sublayer) {
		qDebug() << "WW: Layer TRW: 'paste sublayer' received NULL sublayer";
		return false;
	}

	if (!item) {
		return false;
	}

	if (sublayer->sublayer_type == SublayerType::WAYPOINT) {
		Waypoint * wp = Waypoint::unmarshall(item, len);
		/* When copying - we'll create a new name based on the original. */
		char * uniq_name = this->new_unique_sublayer_name(SublayerType::WAYPOINT, wp->name);
		wp->set_name(uniq_name);
		std::free(uniq_name);

		this->add_waypoint(wp);

		wp->convert(this->coord_mode);
		this->calculate_bounds_waypoints();

		/* Consider if redraw necessary for the new item. */
		if (this->visible && this->waypoints_visible && wp->visible) {
			this->emit_changed();
		}
		return true;
	}
	if (sublayer->sublayer_type == SublayerType::TRACK) {
		Track * trk = Track::unmarshall(item, len);

		/* When copying - we'll create a new name based on the original. */
		char * uniq_name = this->new_unique_sublayer_name(SublayerType::TRACK, trk->name);
		trk->set_name(uniq_name);
		std::free(uniq_name);

		this->add_track(trk);

		trk->convert(this->coord_mode);

		/* Consider if redraw necessary for the new item. */
		if (this->visible && this->tracks_visible && trk->visible) {
			this->emit_changed();
		}
		return true;
	}
	if (sublayer->sublayer_type == SublayerType::ROUTE) {
		Track * trk = Track::unmarshall(item, len);
		/* When copying - we'll create a new name based on the original. */
		char * uniq_name = this->new_unique_sublayer_name(SublayerType::ROUTE, trk->name);
		trk->set_name(uniq_name);
		free(uniq_name);

		this->add_route(trk);
		trk->convert(this->coord_mode);

		/* Consider if redraw necessary for the new item. */
		if (this->visible && this->routes_visible && trk->visible) {
			this->emit_changed();
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
	g_list_foreach(this->image_cache->head, (GFunc) cached_pixmap_free, NULL);
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




bool LayerTRW::set_param_value(uint16_t id, ParameterValue data, bool is_file_operation)
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
		this->track_color = QColor(data.c.r, data.c.g, data.c.b, data.c.a);
		this->new_track_pens();
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
			this->new_track_pens();
		}
		break;
	case PARAM_BLT:
		if (data.u <= 8 && data.u != this->bg_line_thickness) {
			this->bg_line_thickness = data.u;
			this->new_track_pens();
		}
		break;

	case PARAM_TBGC:
		param_to_color(this->track_bg_color, data);
		this->track_bg_pen.setColor(this->track_bg_color);
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
			cached_pixmap_free((CachedPixmap *) g_queue_pop_tail(this->image_cache));
		}
		break;

	case PARAM_WPC:
		param_to_color(this->waypoint_color, data);
		this->waypoint_pen.setColor(this->waypoint_color);
		break;

	case PARAM_WPTC:
		param_to_color(this->waypoint_text_color, data);
		this->waypoint_pen.setColor(this->waypoint_text_color);
		break;

	case PARAM_WPBC:
		param_to_color(this->waypoint_bg_color, data);
		this->waypoint_bg_pen.setColor(this->waypoint_bg_color);
		break;

	case PARAM_WPBA:
#ifdef K
		this->wpbgand = (GdkFunction) data.b;
		if (this->waypoint_bg_gc) {
			gdk_gc_set_function(this->waypoint_bg_gc, data.b ? GDK_AND : GDK_COPY);
		}
#endif
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




ParameterValue LayerTRW::get_param_value(param_id_t id, bool is_file_operation) const
{
	ParameterValue rv;
	switch (id) {
	case PARAM_TV: rv.b = this->tracks_visible; break;
	case PARAM_WV: rv.b = this->waypoints_visible; break;
	case PARAM_RV: rv.b = this->routes_visible; break;
	case PARAM_TDL: rv.b = this->track_draw_labels; break;
	case PARAM_TLFONTSIZE: rv.u = this->track_font_size; break;
	case PARAM_DM: rv.u = this->drawmode; break;
	case PARAM_TC: color_to_param(rv, this->track_color); break;
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
	case PARAM_TBGC: color_to_param(rv, this->track_bg_color); break;
	case PARAM_TDSF: rv.d = this->track_draw_speed_factor; break;
	case PARAM_TSO: rv.u = this->track_sort_order; break;
	case PARAM_IS: rv.u = this->image_size; break;
	case PARAM_IA: rv.u = this->image_alpha; break;
	case PARAM_ICS: rv.u = this->image_cache_size; break;
	case PARAM_WPC:	color_to_param(rv, this->waypoint_color); break;
	case PARAM_WPTC: color_to_param(rv, this->waypoint_text_color); break;
	case PARAM_WPBC: color_to_param(rv, this->waypoint_bg_color); break;
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




void LayerTRWInterface::change_param(GtkWidget * widget, ui_change_values * values)
{
	// This '-3' is to account for the first few parameters not in the properties
	const int OFFSET = -3;
#ifdef K
	switch (values->param_id) {
		// Alter sensitivity of waypoint draw image related widgets according to the draw image setting.
	case PARAM_DI: {
		// Get new value
		ParameterValue vlpd = a_uibuilder_widget_get_value(widget, values->param);
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
		ParameterValue vlpd = a_uibuilder_widget_get_value(widget, values->param);
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
		ParameterValue vlpd = a_uibuilder_widget_get_value(widget, values->param);
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

			SublayerType sublayer_type = (SublayerType) pl;

			// Also remember to (attempt to) convert each coordinate in case this is pasted into a different drawmode
			if (sublayer_type == SublayerType::TRACK) {
				Track * trk = Track::unmarshall(data + sizeof_len_and_subtype, 0);
				/* Unmarshalling already sets track name, so we don't have to do it here. */
				trw->add_track(trk);
				trk->convert(trw->coord_mode);
			}
			if (sublayer_type == SublayerType::WAYPOINT) {
				Waypoint * wp = Waypoint::unmarshall(data + sizeof_len_and_subtype, 0);
				/* Unmarshalling already sets waypoint name, so we don't have to do it here. */
				trw->add_waypoint(wp);
				wp->convert(trw->coord_mode);
			}
			if (sublayer_type == SublayerType::ROUTE) {
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
	delete this->tracks_node;
	delete this->routes_node;
	delete this->waypoints_node;

	/* kamilTODO: call destructors of objects in these maps. */
	this->waypoints.clear();
	this->tracks.clear();
	this->routes.clear();

#ifdef K
	if (this->tracklabellayout != NULL) {
		g_object_unref(G_OBJECT (this->tracklabellayout));
	}

	if (this->wplabellayout != NULL) {
		g_object_unref(G_OBJECT (this->wplabellayout));
	}


	free(this->wp_fsize_str);
	free(this->track_fsize_str);

#endif

	this->image_cache_free();

	delete this->tpwin;
}




void LayerTRW::draw_with_highlight(Viewport * viewport, bool highlight)
{
	static DrawingParams dp;
	init_drawing_params(&dp, this, viewport, highlight);

	if (true /* this->tracks_visible */) { /* TODO: fix condition. */
		qDebug() << "II: Layer TRW: calling function to draw tracks";
		trw_layer_draw_track_cb(tracks, &dp);
	}

	if (true /* this->routes_visible */) { /* TODO: fix condition. */
		qDebug() << "II: Layer TRW: calling function to draw routes";
		trw_layer_draw_track_cb(routes, &dp);
	}

	if (true /* this->waypoints_visible */) { /* TODO: fix condition. */
		qDebug() << "II: Layer TRW: calling function to draw waypoints";
		trw_layer_draw_waypoints_cb(&waypoints, &dp);
	}
}




void LayerTRW::draw(Viewport * viewport)
{
	/* If this layer is to be highlighted - then don't draw now - as it will be drawn later on in the specific highlight draw stage
	   This may seem slightly inefficient to test each time for every layer
	   but for a layer with *lots* of tracks & waypoints this can save some effort by not drawing the items twice. */
#ifdef K
	if (viewport->get_draw_highlight()
	    && this->get_window()->get_selected_trw_layer() == this) {

		return;
	}
#endif

	qDebug() << "II: Layer TRW: calling draw_with_highlight()";
	this->draw_with_highlight(viewport, false);
}




void LayerTRW::draw_highlight(Viewport * viewport)
{
	/* kamilFIXME: enabling this code and then compiling it with -O0 results in crash when selecting trackpoint in viewport. */
#if 0
	/* Check the layer for visibility (including all the parents visibilities). */
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
	/* Check the layer for visibility (including all the parents visibilities). */
	if (!this->tree_view->is_visible_in_tree(&this->iter)) {
		return;
	}
#endif

	static DrawingParams dp;
	init_drawing_params(&dp, this, viewport, true);

	if (trk) {
		bool do_draw = (trk->sublayer_type == SublayerType::ROUTE && this->routes_visible)
			|| (trk->sublayer_type == SublayerType::TRACK && this->tracks_visible);

		if (do_draw) {
			trw_layer_draw_track_cb(NULL, trk, &dp);
		}
	}
	if (this->waypoints_visible && wp) {
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
void LayerTRW::draw_highlight_items(Tracks * tracks_, Waypoints * selected_waypoints, Viewport * viewport)
{
	/* kamilFIXME: enabling this code and then compiling it with -O0 results in crash when selecting trackpoint in viewport. */
#if 0
	/* Check the layer for visibility (including all the parents visibilities). */
	if (!this->tree_view->is_visible_in_tree(&this->iter)) {
		return;
	}
#endif

	static DrawingParams dp;
	init_drawing_params(&dp, this, viewport, true);

	if (tracks_) {
		bool is_routes = (tracks_ == &routes);
		bool do_draw = (is_routes && this->routes_visible) || (!is_routes && this->tracks_visible);
		if (do_draw) {
			trw_layer_draw_track_cb(*tracks_, &dp);
		}
	}

	if (this->waypoints_visible && selected_waypoints) {
		trw_layer_draw_waypoints_cb(selected_waypoints, &dp);
	}
}




void LayerTRW::new_track_pens(void)
{
	uint32_t width = this->line_thickness;


	this->track_bg_pen = QPen(this->track_bg_color);
	this->track_bg_pen.setWidth(width + this->bg_line_thickness);


	/* Ensure new track drawing heeds line thickness setting,
	   however always have a minium of 2, as 1 pixel is really narrow. */
	int new_track_width = (this->line_thickness < 2) ? 2 : this->line_thickness;
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

	this->track_pens[VIK_TRW_LAYER_TRACK_GC_SINGLE] = QPen(QColor(this->track_color));
	this->track_pens[VIK_TRW_LAYER_TRACK_GC_SINGLE].setWidth(width);
}




#define SMALL_ICON_SIZE 18
/*
 * Can accept a null symbol, and may return null value
 */
QIcon * get_wp_sym_small(char *symbol)
{
#ifdef K
	QPixmap * wp_icon = a_get_wp_sym(symbol);
	/* ATM a_get_wp_sym returns a cached icon, with the size dependent on the preferences.
	   So needing a small icon for the treeview may need some resizing: */
	if (wp_icon && wp_icon->width() != SMALL_ICON_SIZE) {
		wp_icon = wp_icon->scaled(SMALL_ICON_SIZE, SMALL_ICON_SIZE, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	}
	return wp_icon;
#endif
}




void LayerTRW::realize_tracks(Tracks & tracks_, Layer * parent_layer, TreeIndex const & a_parent_index, TreeView * a_tree_view)
{
	for (auto i = tracks_.begin(); i != tracks_.end(); i++) {
		Track * trk = i->second;

		QIcon * icon = NULL;
		if (trk->has_color) {
			QPixmap pixmap(SMALL_ICON_SIZE, SMALL_ICON_SIZE);
			pixmap.fill(trk->color);
			icon = new QIcon(pixmap);
		}

		time_t timestamp = 0;
		Trackpoint * tpt = trk->get_tp_first();
		if (tpt && tpt->has_timestamp) {
			timestamp = tpt->timestamp;
		}

		a_tree_view->add_sublayer(trk, parent_layer, a_parent_index, trk->name, icon, true, timestamp);

		delete icon;

		if (!trk->visible) {
			a_tree_view->set_visibility(trk->index, false);
		}
	}
}




void LayerTRW::realize_waypoints(Waypoints & waypoints_, Layer * parent_layer, TreeIndex const & a_parent_index, TreeView * a_tree_view)
{
	for (auto i = waypoints_.begin(); i != waypoints_.end(); i++) {
		time_t timestamp = 0;
		if (i->second->has_timestamp) {
			timestamp = i->second->timestamp;
		}

		a_tree_view->add_sublayer(i->second, parent_layer, a_parent_index, i->second->name, NULL /* i->second->symbol */, true, timestamp);

		if (!i->second->visible) {
			a_tree_view->set_visibility(i->second->index, false);
		}
	}
}




void LayerTRW::add_tracks_node(void)
{
	assert(this->connected_to_tree);

	this->tracks_node = new Sublayer(SublayerType::TRACKS);
	this->tree_view->add_sublayer(this->tracks_node, this, this->index, _("Tracks"), NULL, false, 0);
}




void LayerTRW::add_waypoints_node(void)
{
	assert(this->connected_to_tree);

	this->waypoints_node = new Sublayer(SublayerType::WAYPOINTS);
	this->tree_view->add_sublayer(this->waypoints_node, this, this->index, _("Waypoints"), NULL, false, 0);
}




void LayerTRW::add_routes_node(void)
{
	assert(this->connected_to_tree);

	this->routes_node = new Sublayer(SublayerType::ROUTES);
	this->tree_view->add_sublayer(this->routes_node, this, this->index, _("Routes"), NULL, false, 0);
}




void LayerTRW::connect_to_tree(TreeView * tree_view_, TreeIndex const & layer_index)
{
	this->tree_view = tree_view_;
	this->index = layer_index;
	this->connected_to_tree = true;

	if (this->tracks.size() > 0) {
		this->add_tracks_node();
		/* Notice that parent layer is "this", but index of direct parent node is index of "tracks" node. */
		this->realize_tracks(this->tracks, this, this->tracks_node->get_index(), tree_view_);
		this->tree_view->set_visibility(this->tracks_node->get_index(), this->tracks_visible);
	}

	if (this->routes.size() > 0) {
		this->add_routes_node();
		/* Notice that parent layer is "this", but index of direct parent node is index of "routes" node. */
		this->realize_tracks(this->routes, this, this->routes_node->get_index(), tree_view_);
		this->tree_view->set_visibility(this->routes_node->get_index(), this->routes_visible);
	}

	if (this->waypoints.size() > 0) {
		this->add_waypoints_node();
		/* Notice that parent layer is "this", but index of direct parent node is index of "waypoints" node. */
		this->realize_waypoints(this->waypoints, this, this->waypoints_node->get_index(), tree_view_);
		this->tree_view->set_visibility(this->waypoints_node->get_index(), this->waypoints_visible);
	}

	this->verify_thumbnails();

	this->sort_all();
}




bool LayerTRW::sublayer_toggle_visible(Sublayer * sublayer)
{
	switch (sublayer->sublayer_type) {
	case SublayerType::TRACKS:
		return (this->tracks_visible ^= 1);
	case SublayerType::WAYPOINTS:
		return (this->waypoints_visible ^= 1);
	case SublayerType::ROUTES:
		return (this->routes_visible ^= 1);
	case SublayerType::TRACK:
		{
			Track * trk = this->tracks.at(sublayer->uid);
			if (trk) {
				return (trk->visible ^= 1);
			} else {
				return true;
			}
		}
	case SublayerType::WAYPOINT:
		{

			Waypoint * wp = this->waypoints.at(sublayer->uid);
			if (wp) {
				return (wp->visible ^= 1);
			} else {
				return true;
			}
		}
	case SublayerType::ROUTE:
		{
			Track * trk = this->routes.at(sublayer->uid);
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
 * Return a property about tracks for this layer.
 */
uint32_t LayerTRW::get_property_tracks_line_thickness()
{
	return this->line_thickness;
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

	if (!this->tracks.empty()) {
		tooltip_tracks tt = { 0.0, 0, 0, 0 };
		trw_layer_tracks_tooltip(this->tracks, &tt);

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
			double len_in_units = convert_distance_meters_to(distance_unit, tt.length);

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
		trw_layer_routes_tooltip(this->routes, &rlength);
		if (rlength > 0.0) {

			/* Setup info dependent on distance units. */
			DistanceUnit distance_unit = Preferences::get_unit_distance();
			get_distance_unit_string(tbuf4, sizeof (tbuf4), distance_unit);
			double len_in_units = convert_distance_meters_to(distance_unit, rlength);
			snprintf(tbuf1, sizeof(tbuf1), _("\nTotal route length %.1f %s"), len_in_units, tbuf4);
		}

		/* Put together all the elements to form compact tooltip text. */
		snprintf(tmp_buf, sizeof(tmp_buf),
			 _("Tracks: %ld - Waypoints: %ld - Routes: %ld%s%s"),
			 this->tracks.size(), this->waypoints.size(), this->routes.size(), tbuf2, tbuf1);

		g_date_free(gdate_start);
		g_date_free(gdate_end);
	}
	return QString(tmp_buf);
}




QString LayerTRW::sublayer_tooltip(Sublayer * sublayer)
{
	if (!sublayer) {
		qDebug() << "WW: Layer TRW: NULL sublayer in sublayer_tooltip()";
		return QString("");
	}
	switch (sublayer->sublayer_type) {
	case SublayerType::TRACKS:
		{
			/* Very simple tooltip - may expand detail in the future. */
			return QString("Tracks: %1").arg(this->tracks.size());
		}
		break;
	case SublayerType::ROUTES:
		{
			/* Very simple tooltip - may expand detail in the future. */
			return QString("Routes: %1").arg(this->routes.size());
		}
		break;

	/* Same tooltip for route and track. */
	case SublayerType::ROUTE:
	case SublayerType::TRACK:
		{
			assert (sublayer->uid != SG_UID_INITIAL);

			Track * trk = NULL;
			if (sublayer->sublayer_type == SublayerType::TRACK) {
				trk = this->tracks.at(sublayer->uid);
			} else {
				trk = this->routes.at(sublayer->uid);
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
		}
		break;
	case SublayerType::WAYPOINTS:
		{
			/* Very simple tooltip - may expand detail in the future. */
			return QString("Waypoints: %1").arg(this->waypoints.size());
		}
		break;
	case SublayerType::WAYPOINT:
		{
			assert (sublayer->uid != SG_UID_INITIAL);

			Waypoint * wp = this->waypoints.at(sublayer->uid);
			/* It's OK to return NULL. */
			if (wp) {
				if (wp->comment) {
					return QString(wp->comment);
				} else {
					return QString(wp->description);
				}
			}
		}
		break;
	default:
		break;
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
	char * msg = vu_trackpoint_formatted_message(statusbar_format_code, tp, tp_prev, this->current_trk, NAN);
	this->get_window()->get_statusbar()->set_message(StatusBarField::INFO, QString(msg));
	free(msg);

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
	static struct LatLon ll;
	vik_coord_to_latlon(&wp->coord, &ll);
	a_coords_latlon_to_string(&ll, &lat, &lon);

	/* Combine parts to make overall message. */
	QString msg;
	if (wp->comment) {
		/* Add comment if available. */
		msg = QString(_("%1 | %2 %3 | Comment: %4")).arg(tmp_buf1).arg(lat).arg(lon).arg(wp->comment);
	} else {
		msg = QString(_("%1 | %2 %3")).arg(tmp_buf1).arg(lat).arg(lon);
	}
	this->get_window()->get_statusbar()->set_message(StatusBarField::INFO, msg);
	free(lat);
	free(lon);
}




/**
 * General layer selection function, find out which bit is selected and take appropriate action.
 */
bool LayerTRW::selected(TreeItemType item_type, Sublayer * sublayer)
{
	/* Reset. */
	this->current_wp = NULL;
	this->cancel_current_tp(false);

	/* Clear statusbar. */
	this->get_window()->get_statusbar()->set_message(StatusBarField::INFO, "");

	switch (item_type) {
	case TreeItemType::LAYER:
		{
			this->get_window()->set_selected_trw_layer(this);
			/* Mark for redraw. */
			return true;
		}
		break;

	case TreeItemType::SUBLAYER:
		{
			switch (sublayer->sublayer_type) {
			case SublayerType::TRACKS:
				{
					this->get_window()->set_selected_tracks(&this->tracks, this);
					/* Mark for redraw. */
					return true;
				}
				break;
			case SublayerType::TRACK:
				{
					Track * trk = this->tracks.at(sublayer->uid);
					this->get_window()->set_selected_track(trk, this);
					/* Mark for redraw. */
					return true;
				}
				break;
			case SublayerType::ROUTES:
				{
					this->get_window()->set_selected_tracks(&this->routes, this);
					/* Mark for redraw. */
					return true;
				}
				break;
			case SublayerType::ROUTE:
				{
					Track * trk = this->routes.at(sublayer->uid);
					this->get_window()->set_selected_track(trk, this);
					/* Mark for redraw. */
					return true;
				}
				break;
			case SublayerType::WAYPOINTS:
				{
					this->get_window()->set_selected_waypoints(&this->waypoints, this);
					/* Mark for redraw. */
					return true;
				}
				break;
			case SublayerType::WAYPOINT:
				{
					Waypoint * wp = this->waypoints.at(sublayer->uid);
					if (wp) {
						this->get_window()->set_selected_waypoint(wp, this);
						/* Show some waypoint info. */
						this->set_statusbar_msg_info_wpt(wp);
						/* Mark for redraw. */
						return true;
					}
				}
				break;
			default:
				{
					return this->get_window()->clear_highlight();
				}
				break;
			}
			return false;
		}
		break;

	default:
		return this->get_window()->clear_highlight();
		break;
	}
}




Tracks & LayerTRW::get_tracks()
{
	return tracks;
}




Tracks & LayerTRW::get_routes()
{
	return routes;
}




Waypoints & LayerTRW::get_waypoints()
{
	return waypoints;
}




bool LayerTRW::is_empty()
{
	return ! (tracks.size() || routes.size() || waypoints.size());
}




bool LayerTRW::get_tracks_visibility()
{
	return this->tracks_visible;
}




bool LayerTRW::get_routes_visibility()
{
	return this->routes_visible;
}




bool LayerTRW::get_waypoints_visibility()
{
	return this->waypoints_visible;
}




/*
 * Get waypoint by name - not guaranteed to be unique
 * Finds the first one
 */
Waypoint * LayerTRW::get_waypoint(const char * wp_name)
{
	return LayerTRWc::find_waypoint_by_name(waypoints, wp_name);
}




/*
 * Get track by name - not guaranteed to be unique
 * Finds the first one
 */
Track * LayerTRW::get_track(const char * name_)
{
	return LayerTRWc::find_track_by_name(tracks, name_);
}




/*
 * Get route by name - not guaranteed to be unique
 * Finds the first one
 */
Track * LayerTRW::get_route(const char * name_)
{
	return LayerTRWc::find_track_by_name(routes, name_);
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
	/* Continually reuse maxmin to find the latest maximum and minimum values.
	   First set to waypoints bounds. */
	if (this->waypoints.size() > 1) { /* kamil TODO this condition may have to be improved. */
		maxmin[0].lat = this->waypoints_bbox.north;
		maxmin[1].lat = this->waypoints_bbox.south;
		maxmin[0].lon = this->waypoints_bbox.east;
		maxmin[1].lon = this->waypoints_bbox.west;
	}

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




void LayerTRW::centerize_cb(void)
{
	VikCoord coord;
	if (this->find_center(&coord)) {
		goto_coord(this->get_window()->get_layers_panel(), NULL, NULL, &coord);
	} else {
		dialog_info("This layer has no waypoints or trackpoints.", this->get_window());
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
		this->get_window()->get_layers_panel()->emit_update_cb();
	} else {
		dialog_info("This layer has no waypoints or trackpoints.", this->get_window());
	}
}




void LayerTRW::export_as_gpspoint_cb(void) /* Slot. */
{
#ifdef K
	char * auto_save_name = append_file_ext(this->get_name(), FILE_TYPE_GPSPOINT);
	vik_trw_layer_export(this, _("Export Layer"), auto_save_name, NULL, FILE_TYPE_GPSPOINT);
	free(auto_save_name);
#endif
}




void LayerTRW::export_as_gpsmapper_cb(void) /* Slot. */
{
#ifdef K
	char * auto_save_name = append_file_ext(this->get_name(), FILE_TYPE_GPSMAPPER);
	vik_trw_layer_export(this, _("Export Layer"), auto_save_name, NULL, FILE_TYPE_GPSMAPPER);
	free(auto_save_name);
#endif
}




void LayerTRW::export_as_gpx_cb(void) /* Slot. */
{
#ifdef K
	char * auto_save_name = append_file_ext(this->get_name(), FILE_TYPE_GPX);
	vik_trw_layer_export(this, _("Export Layer"), auto_save_name, NULL, FILE_TYPE_GPX);
	free(auto_save_name);
#endif
}




void LayerTRW::export_as_kml_cb(void) /* Slot. */
{
#ifdef K
	char * auto_save_name = append_file_ext(this->get_name(), FILE_TYPE_KML);
	vik_trw_layer_export(this, _("Export Layer"), auto_save_name, NULL, FILE_TYPE_KML);
	free(auto_save_name);
#endif
}




void LayerTRW::export_as_geojson_cb(void) /* Slot. */
{
#ifdef K
	char * auto_save_name = append_file_ext(this->get_name(), FILE_TYPE_GEOJSON);
	vik_trw_layer_export(this, _("Export Layer"), auto_save_name, NULL, FILE_TYPE_GEOJSON);
	free(auto_save_name);
#endif
}




void LayerTRW::export_via_babel_cb(void) /* Slot. */
{
	vik_trw_layer_export_gpsbabel(this, _("Export Layer"), this->get_name());
}




void LayerTRW::open_with_external_gpx_1_cb(void) /* Slot. */
{
	vik_trw_layer_export_external_gpx(this, Preferences::get_external_gpx_program_1());
}




void LayerTRW::open_with_external_gpx_2_cb(void) /* Slot. */
{
	vik_trw_layer_export_external_gpx(this, Preferences::get_external_gpx_program_2());
}




void LayerTRW::export_gpx_track_cb(void)
{
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (!trk || !trk->name) {
		return;
	}
#ifdef K

	char * auto_save_name = append_file_ext(trk->name, FILE_TYPE_GPX);

	char * label = NULL;
	if (this->menu_data->sublayer->type == SublayerType::ROUTE) {
		label = _("Export Route as GPX");
	} else {
		label = _("Export Track as GPX");
	}
	vik_trw_layer_export(this, label, auto_save_name, trk, FILE_TYPE_GPX);

	free(auto_save_name);
#endif
}




void LayerTRW::find_waypoint_dialog_cb(void)
{
	LayersPanel * panel = this->get_window()->get_layers_panel();

	QInputDialog dialog(this->get_window());
	dialog.setWindowTitle(_("Find"));
	dialog.setLabelText(_("Waypoint Name:"));
	dialog.setInputMode(QInputDialog::TextInput);


	while (dialog.exec() == QDialog::Accepted) {
		QString name_ = dialog.textValue();

		/* Find *first* wp with the given name. */
		Waypoint * wp = this->get_waypoint(name_.toUtf8().data());

		if (!wp) {
			dialog_error(_("Waypoint not found in this layer."), this->get_window());
		} else {
			panel->get_viewport()->set_center_coord(&wp->coord, true);
			panel->emit_update_cb();
			this->tree_view->select_and_expose(wp->index);

			break;
		}

	}
}




bool LayerTRW::new_waypoint(Window * parent_window, const VikCoord * def_coord)
{
	char * default_name = this->highest_wp_number_get();
	Waypoint * wp = new Waypoint();
	bool updated;
	wp->coord = *def_coord;

	/* Attempt to auto set height if DEM data is available. */
	wp->apply_dem_data(true);

	char * returned_name = waypoint_properties_dialog(parent_window, default_name, this, wp, this->coord_mode, true, &updated);

	if (returned_name) {
		wp->visible = true;
		wp->set_name(returned_name);
		this->add_waypoint(wp);
		free(default_name);
		free(returned_name);
		return true;
	} else {
		free(default_name);
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
	this->calculate_bounds_waypoints();
	panel->emit_update_cb();
}




void LayerTRW::acquire_from_wikipedia_waypoints_layer_cb(void) /* Slot. */
{
	LayersPanel * panel = this->get_window()->get_layers_panel();
	struct LatLon maxmin[2] = { {0.0,0.0}, {0.0,0.0} };

	this->find_maxmin(maxmin);

	a_geonames_wikipedia_box(this->get_window(), this, maxmin);
	this->calculate_bounds_waypoints();
	panel->emit_update_cb();
}




#ifdef VIK_CONFIG_GEOTAG
void LayerTRW::geotagging_waypoint_mtime_keep_cb(void)
{
	sg_uid_t wp_uid = this->menu_data->sublayer->uid;
	Waypoint * wp = this->waypoints.at(wp_uid);
	if (wp) {
#ifdef K
		/* Update directly - not changing the mtime. */
		a_geotag_write_exif_gps(wp->image, wp->coord, wp->altitude, true);
#endif
	}
}




void LayerTRW::geotagging_waypoint_mtime_update_cb(void)
{
	sg_uid_t wp_uid = this->menu_data->sublayer->uid;
	Waypoint * wp = this->waypoints.at(wp_uid);
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
	sg_uid_t uid = this->menu_data->sublayer->uid;
	Track * trk = this->tracks.at(uid);
	/* Unset so can be reverified later if necessary. */
	this->has_verified_thumbnails = false;
	trw_layer_geotag_dialog(this->get_window(), this, NULL, trk);
}




void LayerTRW::geotagging_waypoint_cb(void)
{
	sg_uid_t wp_uid = this->menu_data->sublayer->uid;
	Waypoint * wp = this->waypoints.at(wp_uid);
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
	Window * window = this->get_window();
	LayersPanel * panel = window->get_layers_panel();
	Viewport * viewport =  window->get_viewport();

	DatasourceMode mode = datasource->mode;
	if (mode == DatasourceMode::AUTO_LAYER_MANAGEMENT) {
		mode = DatasourceMode::ADDTOLAYER;
	}
	a_acquire(window, panel, viewport, mode, datasource, NULL, NULL);
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
	LayersPanel * panel = this->menu_data->layers_panel;
	sg_uid_t uid = this->menu_data->sublayer->uid;
#ifdef K

	/* May not actually get a track here as values[2&3] can be null. */
	Track * trk = NULL;
	GPSTransferType xfer_type = GPSTransferType::TRK; /* SublayerType::TRACKS = 0 so hard to test different from NULL! */
	bool xfer_all = false;

	if ((bool) data->sublayer_type) { /* kamilFIXME: don't cast. */
		xfer_all = false;
		if (this->menu_data->sublayer->type == SublayerType::ROUTE) {
			trk = this->routes.at(uid);
			xfer_type = GPSTransferType::RTE;
		} else if (this->menu_data->sublayer->type == SublayerType::TRACK) {
			trk = this->tracks.at(uid);
			xfer_type = GPSTransferType::TRK;
		} else if (this->menu_data->sublayer->type == SublayerType::WAYPOINTS) {
			xfer_type = GPSTransferType::WPT;
		} else if (this->menu_data->sublayer->type == SublayerType::ROUTES) {
			xfer_type = GPSTransferType::RTE;
		}
	} else if (!data->confirm) {
		xfer_all = true; /* i.e. whole layer. */
	}

	if (trk && !trk->visible) {
		dialog_error(_("Can not upload invisible track."), this->get_window());
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
	char* port = datasource_gps_get_descriptor(dgs);
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
		this->calculate_bounds_waypoints();
		if (this->visible) {
			panel->emit_update_cb();
		}
	}
}




void LayerTRW::new_track_create_common(char * name_)
{
	qDebug() << "II: Layer TRW: new track create common, track name" << name_;

	this->current_trk = new Track(false);
	this->current_trk->set_defaults();
	this->current_trk->visible = true;

	if (this->drawmode == DRAWMODE_ALL_SAME_COLOR) {
		/* Create track with the preferred colour from the layer properties. */
		this->current_trk->color = this->track_color;
	} else {
		this->current_trk->color = QColor("#aa22dd"); //QColor("#000000");
	}

	this->current_trk->has_color = true;
	this->current_trk->set_name(name_);
	this->add_track(this->current_trk);
}




void LayerTRW::new_track_cb() /* Slot. */
{
	if (!this->current_trk) {
		char * name_ = this->new_unique_sublayer_name(SublayerType::TRACK, _("Track")) ;
		this->new_track_create_common(name_);
		free(name_);

		this->get_window()->activate_layer_tool(LayerType::TRW, LAYER_TRW_TOOL_CREATE_TRACK);
	}
}




void LayerTRW::new_route_create_common(char * name_)
{
	this->current_trk = new Track(true);
	this->current_trk->set_defaults();
	this->current_trk->visible = true;
	/* By default make all routes red. */
	this->current_trk->has_color = true;
	this->current_trk->color = QColor("red");
	this->current_trk->set_name(name_);

	this->add_route(this->current_trk);
}




void LayerTRW::new_route_cb(void) /* Slot. */
{
	if (!this->current_trk) {
		char * name_ = this->new_unique_sublayer_name(SublayerType::ROUTE, _("Route")) ;
		this->new_route_create_common(name_);
		free(name_);

		this->get_window()->activate_layer_tool(LayerType::TRW, LAYER_TRW_TOOL_CREATE_ROUTE);
	}
}




void LayerTRW::full_view_routes_cb(void) /* Slot. */
{
	LayersPanel * panel = this->get_window()->get_layers_panel();

	if (this->routes.size() > 0) {
		struct LatLon maxmin[2] = { {0,0}, {0,0} };
		LayerTRWc::find_maxmin_in_tracks(this->routes, maxmin);
		this->zoom_to_show_latlons(panel->get_viewport(), maxmin);
		panel->emit_update_cb();
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

	if (this->tracks.size() > 0) {
		struct LatLon maxmin[2] = { {0,0}, {0,0} };
		LayerTRWc::find_maxmin_in_tracks(this->tracks, maxmin);
		this->zoom_to_show_latlons(panel->get_viewport(), maxmin);
		panel->emit_update_cb();
	}
}




void LayerTRW::full_view_waypoints_cb(void) /* Slot. */
{
	LayersPanel * panel = this->get_window()->get_layers_panel();

	/* Only 1 waypoint - jump straight to it */
	if (this->waypoints.size() == 1) {
		Viewport * viewport = panel->get_viewport();
		LayerTRWc::single_waypoint_jump(this->waypoints, viewport);
	}
	/* If at least 2 waypoints - find center and then zoom to fit */
	else if (this->waypoints.size() > 1) {
		struct LatLon maxmin[2] = { {0,0}, {0,0} };
		maxmin[0].lat = this->waypoints_bbox.north;
		maxmin[1].lat = this->waypoints_bbox.south;
		maxmin[0].lon = this->waypoints_bbox.east;
		maxmin[1].lon = this->waypoints_bbox.west;
		this->zoom_to_show_latlons(panel->get_viewport(), maxmin);
	}

	panel->emit_update_cb();
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
	this->waypoints.insert({{ wp->uid, wp }});

	if (this->connected_to_tree) {
		if (waypoints.size() == 1) { /* We compare against '1' because we already added a first wp to ::waypoints() at the beginning of this function. */
			this->add_waypoints_node();
		}

		time_t timestamp = 0;
		if (wp->has_timestamp) {
			timestamp = wp->timestamp;
		}

		/* Visibility column always needed for waypoints. */
		this->tree_view->add_sublayer(wp, this, this->waypoints_node->get_index(), wp->name, NULL /* wp->symbol */, true, timestamp);

		/* Actual setting of visibility dependent on the waypoint. */
		this->tree_view->set_visibility(wp->index, wp->visible);

		/* Sort now as post_read is not called on a waypoint connected to tree. */
		this->tree_view->sort_children(this->waypoints_node->get_index(), this->wp_sort_order);
	}

	this->highest_wp_number_add_wp(wp->name);
}




void LayerTRW::add_track(Track * trk)
{
	this->tracks.insert({{ trk->uid, trk }});

	if (this->connected_to_tree) {
		if (tracks.size() == 1) { /* We compare against '1' because we already added a first trk to ::tracks() at the beginning of this function. */
			this->add_tracks_node();
		}

		time_t timestamp = 0;
		Trackpoint * tp = trk->get_tp_first();
		if (tp && tp->has_timestamp) {
			timestamp = tp->timestamp;
		}

		/* Visibility column always needed for tracks. */
		this->tree_view->add_sublayer(trk, this, this->tracks_node->get_index(), trk->name, NULL, true, timestamp);

		/* Actual setting of visibility dependent on the track. */
		this->tree_view->set_visibility(trk->index, trk->visible);

		/* Sort now as post_read is not called on a track connected to tree. */
		this->tree_view->sort_children(this->tracks_node->get_index(), this->track_sort_order);
	}

	this->update_treeview(trk);
}




void LayerTRW::add_route(Track * trk)
{
	this->routes.insert({{ trk->uid, trk }});

	if (this->connected_to_tree) {
		if (routes.size() == 1) { /* We compare against '1' because we already added a first trk to ::routes() at the beginning of this function. */
			this->add_routes_node();
		}

		/* Visibility column always needed for routes. */
		this->tree_view->add_sublayer(trk, this, this->routes_node->get_index(), trk->name, NULL, true, 0); /* Routes don't have times. */

		/* Actual setting of visibility dependent on the route. */
		this->tree_view->set_visibility(trk->index, trk->visible);

		/* Sort now as post_read is not called on a route connected to tree. */
		this->tree_view->sort_children(this->routes_node->get_index(), this->track_sort_order);
	}

	this->update_treeview(trk);
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
	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {
		Waypoint * wp = i->second;
		if (wp->symbol) {
			/* Reapply symbol setting to update the pixbuf. */
			char * tmp_symbol = g_strdup(wp->symbol);
			wp->set_symbol(tmp_symbol);
			free(tmp_symbol);
		}
	}
}




/**
 * Allocates a unique new name.
 */
char * LayerTRW::new_unique_sublayer_name(SublayerType sublayer_type, const char * name_)
{
	int i = 2; /* kamilTODO: static? */
	char * newname = g_strdup(name_);

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
		/* If found a name already in use try adding 1 to it and we try again. */
		if (id) {
			char * new_newname = g_strdup_printf("%s#%d", name_, i);
			free(newname);
			newname = new_newname;
			i++;
		}
	} while (id != NULL);

	return newname;
}




void LayerTRW::filein_add_waypoint(Waypoint * wp, char const * name_)
{
	/* No more uniqueness of name forced when loading from a file.
	   This now makes this function a little redunant as we just flow the parameters through. */
	wp->set_name(name_);
	this->add_waypoint(wp);
}




void LayerTRW::filein_add_track(Track * trk, char const * name_)
{
	if (this->route_finder_append && this->current_trk) {
		trk->remove_dup_points(); /* Make "double point" track work to undo. */

		/* Enforce end of current track equal to start of tr. */
		Trackpoint * cur_end = this->current_trk->get_tp_last();
		Trackpoint * new_start = trk->get_tp_first();
		if (cur_end && new_start) {
			if (!vik_coord_equals(&cur_end->coord, &new_start->coord)) {
				this->current_trk->add_trackpoint(new Trackpoint(*cur_end), false);
			}
		}

		this->current_trk->steal_and_append_trackpoints(trk);
		trk->free();
		this->route_finder_append = false; /* This means we have added it. */
	} else {
		trk->set_name(name_);
		/* No more uniqueness of name forced when loading from a file. */
		if (trk->sublayer_type == SublayerType::ROUTE) {
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
void LayerTRW::move_item(LayerTRW * trw_dest, sg_uid_t sublayer_uid, SublayerType sublayer_type)
{
#ifdef K
	LayerTRW * trw_src = this;
	/* When an item is moved the name is checked to see if it clashes with an existing name
	   in the destination layer and if so then it is allocated a new name. */

	/* TODO reconsider strategy when moving within layer (if anything...). */
	if (trw_src == trw_dest) {
		return;
	}

	if (sublayer_type == SublayerType::TRACK) {
		Track * trk = this->tracks.at(sublayer_uid);
		Track * trk2 = new Track(*trk);

		char * newname = trw_dest->new_unique_sublayer_name(sublayer_type, trk->name);
		trk2->set_name(newname);
		free(newname);

		trw_dest->add_track(trk2);

		this->delete_track(trk);
		/* Reset layer timestamps in case they have now changed. */
		trw_dest->tree_view->set_timestamp(trw_dest->index, trw_dest->get_timestamp());
		trw_src->tree_view->set_timestamp(trw_src->index, trw_src->get_timestamp());
	}

	if (sublayer_type == SublayerType::ROUTE) {
		Track * trk = this->routes.at(sublayer_uid);
		Track * trk2 = new Track(*trk);

		char * newname = trw_dest->new_unique_sublayer_name(sublayer_type, trk->name);
		trk2->set_name(newname);
		free(newname);

		trw_dest->add_route(trk2);

		this->delete_route(trk);
	}

	if (sublayer_type == SublayerType::WAYPOINT) {
		Waypoint * wp = this->waypoints.at(sublayer_uid);
		Waypoint * wp2 = new Waypoint(*wp);

		char * newname = trw_dest->new_unique_sublayer_name(sublayer_type, wp->name);
		wp2->set_name(newname);
		free(newname);

		trw_dest->add_waypoint(wp2);

		this->delete_waypoint(wp);

		/* Recalculate bounds even if not renamed as maybe dragged between layers. */
		trw_dest->calculate_bounds_waypoints();
		trw_src->calculate_bounds_waypoints();
		/* Reset layer timestamps in case they have now changed. */
		trw_dest->tree_view->set_timestamp(trw_dest->index, trw_dest->get_timestamp());
		trw_src->tree_view->set_timestamp(trw_src->index, trw_src->get_timestamp());
	}
#endif
}




void LayerTRW::drag_drop_request(Layer * src, GtkTreeIter * src_item_iter, GtkTreePath * dest_path)
{
#ifdef K
	LayerTRW * trw_dest = this;
	LayerTRW * trw_src = (LayerTRW *) src;

	Sublayer * sublayer = trw_src->tree_view->get_sublayer(src_item_iter);

	if (!trw_src->tree_view->get_name(src_item_iter)) {
		GList *items = NULL;

		if (sublayer->type == SublayerType::TRACKS) {
			LayerTRWc::list_trk_uids(trw_src->tracks, &items);
		}
		if (sublayer->type == SublayerType::WAYPOINTS) {
			LayerTRWc::list_wp_uids(trw_src->waypoints, &items);
		}
		if (sublayer->type == SublayerType::ROUTES) {
			LayerTRWc::list_trk_uids(trw_src->routes, &items);
		}

		GList * iter = items;
		while (iter) {
			if (sublayer->type == SublayerType::TRACKS) {
				trw_src->move_item(trw_dest, iter->data, SublayerType::TRACK);
			} else if (sublayer->type == SublayerType::ROUTES) {
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
		trw_src->move_item(trw_dest, name, sublayer->type);
	}
#endif
}




bool LayerTRW::delete_track(Track * trk)
{
	/* kamilTODO: why check for trk->name here? */
	if (!trk || !trk->name) {
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
	tracks.erase(trk->uid); /* kamilTODO: should this line be inside of "if (it)"? */

	/* If last sublayer, then remove sublayer container. */
	if (tracks.size() == 0) {
		this->tree_view->erase(this->tracks_node->get_index());
	}
	/* In case it was selected (no item delete signal ATM). */
	this->get_window()->clear_highlight();

	return was_visible;
}




bool LayerTRW::delete_route(Track * trk)
{
	/* kamilTODO: why check for trk->name here? */
	if (!trk || !trk->name) {
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
	routes.erase(trk->uid); /* kamilTODO: should this line be inside of "if (it)"? */

	/* If last sublayer, then remove sublayer container. */
	if (routes.size() == 0) {
		this->tree_view->erase(this->routes_node->get_index());
	}

	/* In case it was selected (no item delete signal ATM). */
	this->get_window()->clear_highlight();

	return was_visible;
}




bool LayerTRW::delete_waypoint(Waypoint * wp)
{
	/* kamilTODO: why check for wp->name here? */
	if (!wp || !wp->name) {
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
	waypoints.erase(wp->uid); /* Last because this frees the name. */

	/* If last sublayer, then remove sublayer container. */
	if (waypoints.size() == 0) {
		this->tree_view->erase(this->waypoints_node->get_index());
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
bool LayerTRW::delete_waypoint_by_name(char const * name_)
{
	/* Currently only the name is used in this waypoint find function. */
	Waypoint * wp = LayerTRWc::find_waypoint_by_name(waypoints, name_);
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
bool LayerTRW::delete_track_by_name(const char * name_, bool is_route)
{
	if (is_route) {
		Track * trk = LayerTRWc::find_track_by_name(routes, name_);
		if (trk) {
			return delete_route(trk);
		}
	} else {
		Track * trk = LayerTRWc::find_track_by_name(tracks, name_);
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
	if (this->current_trk) {
		this->cancel_current_tp(false);
	}

	for (auto i = this->routes.begin(); i != this->routes.end(); i++) {
		tree_view->erase(i->second->index);
	}

	this->routes.clear(); /* kamilTODO: call destructors of routes. */

	this->tree_view->erase(this->routes_node->get_index());

	this->emit_changed();
}




void LayerTRW::delete_all_tracks()
{
	this->current_trk = NULL;
	this->route_finder_added_track = NULL;
	if (this->current_trk) {
		this->cancel_current_tp(false);
	}

	for (auto i = this->tracks.begin(); i != this->tracks.end(); i++) {
		tree_view->erase(i->second->index);
	}

	this->tracks.clear(); /* kamilTODO: call destructors of tracks. */

	this->tree_view->erase(this->tracks_node->get_index());

	this->emit_changed();
}




void LayerTRW::delete_all_waypoints()
{
	this->current_wp = NULL;
	this->moving_wp = false;

	this->highest_wp_number_reset();

	for (auto i = this->waypoints.begin(); i != this->waypoints.end(); i++) {
		tree_view->erase(i->second->index);
	}

	this->waypoints.clear(); /* kamilTODO: does this really call destructors of Waypoints? */

	this->tree_view->erase(this->waypoints_node->get_index());

	this->emit_changed();
}




void LayerTRW::delete_all_tracks_cb(void) /* Slot. */
{
	/* Get confirmation from the user. */
	if (dialog_yes_or_no(QString("Are you sure you want to delete all tracks in \"%1\"?").arg(QString(this->get_name())), this->get_window())) {
		    this->delete_all_tracks();
	}
}




void LayerTRW::delete_all_routes_cb(void) /* Slot. */
{
	/* Get confirmation from the user. */
	if (dialog_yes_or_no(QString("Are you sure you want to delete all routes in \"%1\"?").arg(QString(this->get_name())), this->get_window())) {

		    this->delete_all_routes();
	}
}




void LayerTRW::delete_all_waypoints_cb(void) /* Slot. */
{
	/* Get confirmation from the user. */
	if (dialog_yes_or_no(QString("Are you sure you want to delete all waypoints in \"%1\"?").arg(QString(this->get_name())), this->get_window())) {

		    this->delete_all_waypoints();
	}
}




void LayerTRW::delete_sublayer_cb(void)
{
	sg_uid_t uid = this->menu_data->sublayer->uid;
	bool was_visible = false;

	if (this->menu_data->sublayer->sublayer_type == SublayerType::WAYPOINT) {
		Waypoint * wp = this->waypoints.at(uid);
		if (wp && wp->name) {
			if (this->menu_data->confirm) {
				/* Get confirmation from the user. */
				/* Maybe this Waypoint Delete should be optional as is it could get annoying... */
				if (!dialog_yes_or_no(QString("Are you sure you want to delete the waypoint \"%1\"?").arg(QString(wp->name))), this->get_window()) {
					return;
				}
			}

			was_visible = this->delete_waypoint(wp);
			this->calculate_bounds_waypoints();
			/* Reset layer timestamp in case it has now changed. */
			this->tree_view->set_timestamp(this->index, this->get_timestamp());
		}
	} else if (this->menu_data->sublayer->sublayer_type == SublayerType::TRACK) {
		Track * trk = this->tracks.at(uid);
		if (trk && trk->name) {
			if (this->menu_data->confirm) {
				/* Get confirmation from the user. */
				if (!dialog_yes_or_no(QString("Are you sure you want to delete the track \"%1\"?").arg(QString(trk->name))), this->get_window()) {
					return;
				}
			}

			was_visible = this->delete_track(trk);
			/* Reset layer timestamp in case it has now changed. */
			this->tree_view->set_timestamp(this->index, this->get_timestamp());
		}
	} else {
		Track * trk = this->routes.at(uid);
		if (trk && trk->name) {
			if (this->menu_data->confirm) {
				/* Get confirmation from the user. */
				if (!dialog_yes_or_no(QString("Are you sure you want to delete the route \"%1\"?").arg(QString(trk->name))), this->get_window()) {
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




/**
 *  Rename waypoint and maintain corresponding name of waypoint in the treeview.
 */
void LayerTRW::waypoint_rename(Waypoint * wp, char const * new_name)
{
	wp->set_name(new_name);

	/* Update the treeview as well. */
	if (wp->index.isValid()) {
		this->tree_view->set_name(wp->index, new_name);
		this->tree_view->sort_children(this->waypoints_node->get_index(), this->wp_sort_order);
	} else {
		qDebug() << "EE: Layer TRW: trying to rename waypoint with invalid index";
	}
}




/**
 *  Maintain icon of waypoint in the treeview.
 */
void LayerTRW::waypoint_reset_icon(Waypoint * wp)
{
	/* Update the treeview. */
	if (wp->index.isValid()) {
		this->tree_view->set_icon(wp->index, get_wp_sym_small(wp->symbol));
	} else {
		qDebug() << "EE: Layer TRW: trying to reset icon of waypoint with invalid index";
	}
}




void LayerTRW::properties_item_cb(void)
{
	if (this->menu_data->sublayer->sublayer_type == SublayerType::WAYPOINT) {
		sg_uid_t wp_uid = this->menu_data->sublayer->uid;
		Waypoint * wp = this->waypoints.at(wp_uid);

		if (wp && wp->name) {
			bool updated = false;

			char * new_name = waypoint_properties_dialog(this->get_window(), wp->name, this, wp, this->coord_mode, false, &updated);
			if (new_name) { /* TODO: memory management. */
				this->waypoint_rename(wp, new_name);
			}

			if (updated && this->menu_data->sublayer->index.isValid()) {
				this->tree_view->set_icon(this->menu_data->sublayer->index, get_wp_sym_small(wp->symbol));
			}

			if (updated && this->visible) {
				this->emit_changed();
			}
		}
	} else {
		Track * trk = this->get_track_helper(this->menu_data->sublayer);
		if (trk && trk->name) {
			track_properties_dialog(this->get_window(), this, trk);
		}
	}
}




void LayerTRW::profile_item_cb(void)
{
	if (this->menu_data->sublayer->sublayer_type != SublayerType::WAYPOINT) {
		Track * trk = this->get_track_helper(this->menu_data->sublayer);
		if (trk && trk->name) {
			track_profile_dialog(this->get_window(),
					     this,
					     trk,
					     this->menu_data->layers_panel ? this->menu_data->layers_panel : NULL,
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
	if (trk && trk->name) {
		track_properties_dialog(this->get_window(), this, trk, true);
	}
}




/*
 * Update the treeview of the track id - primarily to update the icon.
 */
void LayerTRW::update_treeview(Track * trk)
{
	if (trk->index.isValid()) {
		QPixmap pixmap(SMALL_ICON_SIZE, SMALL_ICON_SIZE);
		pixmap.fill(trk->color);
		QIcon icon(pixmap);
		this->tree_view->set_icon(trk->index, &icon);
	}
}




static void goto_coord(LayersPanel * panel, Layer * layer, Viewport * viewport, const VikCoord * coord)
{
	if (panel) {
		panel->get_viewport()->set_center_coord(coord, true);
		panel->emit_update_cb();
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
		goto_coord(panel, this, this->menu_data->viewport, &trk->get_tp_first()->coord);
	}
}




void LayerTRW::goto_track_center_cb(void)
{
	LayersPanel * panel = this->get_window()->get_layers_panel();
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (trk && !trk->empty()) {
		struct LatLon average, maxmin[2] = { {0,0}, {0,0} };
		VikCoord coord;
		LayerTRW::find_maxmin_in_track(trk, maxmin);
		average.lat = (maxmin[0].lat+maxmin[1].lat)/2;
		average.lon = (maxmin[0].lon+maxmin[1].lon)/2;
		vik_coord_load_from_latlon(&coord, this->coord_mode, &average);
		goto_coord(panel, this, this->menu_data->viewport, &coord);
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
	if (trk->sublayer_type == SublayerType::TRACK
	    && ((trk->get_segment_count() > 1)
		|| (trk->get_average_speed() > 0.0))) {

		if (!dialog_yes_or_no("Converting a track to a route removes extra track data such as segments, timestamps, etc...\nDo you want to continue?", this->get_window())) {
			return;
		}
	}

	/* Copy it. */
	Track * trk_copy = new Track(*trk);

	/* Convert. */
	trk_copy->sublayer_type = trk_copy->sublayer_type == SublayerType::ROUTE ? SublayerType::TRACK : SublayerType::ROUTE;

	/* ATM can't set name to self - so must create temporary copy. */
	char * name_ = g_strdup(trk_copy->name);

	/* Delete old one and then add new one. */
	if (trk->sublayer_type == SublayerType::ROUTE) {
		this->delete_route(trk);
		trk_copy->set_name(name_);
		this->add_track(trk_copy);
	} else {
		/* Extra route conversion bits... */
		trk_copy->merge_segments();
		trk_copy->to_routepoints();

		this->delete_track(trk);
		trk_copy->set_name(name_);
		this->add_route(trk_copy);
	}
	free(name_);

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

	this->get_window()->activate_layer_tool(LayerType::TRW, trk->sublayer_type == SublayerType::ROUTE ? LAYER_TRW_TOOL_CREATE_ROUTE : LAYER_TRW_TOOL_CREATE_TRACK);

	if (!trk->empty()) {
		goto_coord(panel, this, this->menu_data->viewport, &trk->get_tp_last()->coord);
	}
}




/**
 * Extend a track using route finder.
 */
void LayerTRW::extend_track_end_route_finder_cb(void)
{
	sg_uid_t uid = this->menu_data->sublayer->uid;
	Track * trk = this->routes.at(uid);
	if (!trk) {
		return;
	}

	this->get_window()->activate_layer_tool(LayerType::TRW, LAYER_TRW_TOOL_ROUTE_FINDER);

	this->current_trk = trk;
	this->route_finder_started = true;

	if (!trk->empty()) {
		goto_coord(this->menu_data->layers_panel, this, this->menu_data->viewport, &trk->get_tp_last()->coord);
	}
}




bool LayerTRW::dem_test(LayersPanel * panel)
{
	/* If have a panel then perform a basic test to see if any DEM info available... */
	if (panel) {
		std::list<Layer *> * dems = panel->get_all_layers_of_type(LayerType::DEM, true); /* Includes hidden DEM layer types. */
		if (dems->empty()) {
			dialog_error("No DEM layers available, thus no DEM values can be applied.", this->get_window());
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
	dialog_info(str, this->get_window());
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
	dialog_info(str, this->get_window());
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
	dialog_info(str, this->get_window());
}




void LayerTRW::apply_dem_data_wpt_all_cb(void)
{
	LayersPanel * panel = this->get_window()->get_layers_panel();

	if (!this->dem_test(panel)) {
		return;
	}

	int changed_ = 0;
	if (this->menu_data->sublayer->sublayer_type == SublayerType::WAYPOINT) {
		/* Single Waypoint. */
		sg_uid_t wp_uid = this->menu_data->sublayer->uid;
		Waypoint * wp = this->waypoints.at(wp_uid);
		if (wp) {
			changed_ = (int) wp->apply_dem_data(false);
		}
	} else {
		/* All waypoints. */
		for (auto i = this->waypoints.begin(); i != this->waypoints.end(); i++) {
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
	if (this->menu_data->sublayer->sublayer_type == SublayerType::WAYPOINT) {
		/* Single Waypoint. */
		sg_uid_t wp_uid = this->menu_data->sublayer->uid;
		Waypoint * wp = this->waypoints.at(wp_uid);
		if (wp) {
			changed_ = (int) wp->apply_dem_data(true);
		}
	} else {
		/* All waypoints. */
		for (auto i = this->waypoints.begin(); i != this->waypoints.end(); i++) {
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
	goto_coord(panel, this, this->menu_data->viewport, &trk->get_tp_last()->coord);
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
	goto_coord(panel, this, this->menu_data->viewport, &vtp->coord);
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
	goto_coord(panel, this, this->menu_data->viewport, &vtp->coord);
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
	goto_coord(panel, this, this->menu_data->viewport, &vtp->coord);
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
		LayerTRW::find_maxmin_in_track(trk, maxmin);
		this->zoom_to_show_latlons(this->menu_data->viewport, maxmin);
		if (panel) {
			panel->emit_update_cb();
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

		QComboBox * combo = vik_routing_ui_selector_new((Predicate)vik_routing_engine_supports_refine, NULL);
		combo->setCurrentIndex(last_engine);
		gtk_widget_show_all(combo);

		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), combo, true, true, 0);

		gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

		if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
			/* Dialog validated: retrieve selected engine and do the job */
			last_engine = combo->currentIndex();
			RoutingEngine *routing = vik_routing_ui_selector_get_nth(combo, last_engine);

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
	sg_uid_t uid = this->menu_data->sublayer->uid;
	std::unordered_map<sg_uid_t, SlavGPS::Track*> * ght_tracks = NULL;
	if (this->menu_data->sublayer->sublayer_type == SublayerType::ROUTE) {
		ght_tracks = &this->routes;
	} else {
		ght_tracks = &this->tracks;
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
	std::list<sg_uid_t> * other_tracks = LayerTRWc::find_tracks_with_timestamp_type(ght_tracks, with_timestamps, trk);

	if (other_tracks->empty()) {
		if (with_timestamps) {
			dialog_error("Failed. No other tracks with timestamps in this layer found", this->get_window());
		} else {
			dialog_error("Failed. No other tracks without timestamps in this layer found", this->get_window());
		}
		delete other_tracks;
		return;
	}
	other_tracks->reverse();

	/* Convert into list of names for usage with dialog function.
	   TODO: Need to consider how to work best when we can have multiple tracks the same name... */
	std::list<QString> other_tracks_names;
	for (auto iter = other_tracks->begin(); iter != other_tracks->end(); iter++) {
		other_tracks_names.push_back(ght_tracks->at(*iter)->name);
	}

	/* Sort alphabetically for user presentation. */
	other_tracks_names.sort();

	std::list<QString> merge_list = a_dialog_select_from_list(this->get_window(),
								  other_tracks_names,
								  true,
								  QString(_("Merge with...")),

								  trk->sublayer_type == SublayerType::ROUTE
								  ? QString(_("Select route to merge with"))
								  : QString(_("Select track to merge with")));
	delete other_tracks;

	if (merge_list.empty()) {
		qDebug() << "II: Layer TRW: merge track is empty";
		return;
	}

	for (auto iter = merge_list.begin(); iter != merge_list.end(); iter++) {
		Track * merge_track = NULL;
		if (trk->sublayer_type == SublayerType::ROUTE) {
			merge_track = this->get_route(iter->toUtf8().data());
		} else {
			merge_track = this->get_track(iter->toUtf8().data());
		}

		if (merge_track) {
			qDebug() << "II: Layer TRW: we have a merge track";
			trk->steal_and_append_trackpoints(merge_track);
			if (trk->sublayer_type == SublayerType::ROUTE) {
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
	Track *trk;
	std::unordered_map<sg_uid_t, SlavGPS::Track*> * ght_tracks = NULL;
	if (this->menu_data->sublayer->sublayer_type == SublayerType::ROUTE) {
		ght_tracks = &this->routes;
	} else {
		ght_tracks = &this->tracks;
	}

	sg_uid_t uid = this->menu_data->sublayer->uid;
	trk = ght_tracks->at(uid);

	if (!trk) {
		return;
	}

	/* Convert into list of names for usage with dialog function.
	   TODO: Need to consider how to work best when we can have multiple tracks the same name... */

	std::list<QString> other_tracks_names = LayerTRWc::get_sorted_track_name_list_exclude_self(ght_tracks, trk);

	/* Note the limit to selecting one track only.
	   This is to control the ordering of appending tracks, i.e. the selected track always goes after the current track
	   (otherwise with multiple select the ordering would not be controllable by the user - automatically being alphabetically). */
	std::list<QString> append_list = a_dialog_select_from_list(this->get_window(),
								   other_tracks_names,
								   false,

								   trk->sublayer_type == SublayerType::ROUTE
								   ? _("Append Route")
								   : _("Append Track"),

								   trk->sublayer_type == SublayerType::ROUTE
								   ? _("Select the route to append after the current route")
								   : _("Select the track to append after the current track"));

	/* It's a list, but shouldn't contain more than one other track! */
	if (append_list.empty()) {
		return;
	}

	for (auto iter = append_list.begin(); iter != append_list.end(); iter++) {
		/* TODO: at present this uses the first track found by name,
		   which with potential multiple same named tracks may not be the one selected... */
		Track * append_track = NULL;
		if (trk->sublayer_type == SublayerType::ROUTE) {
			append_track = this->get_route(iter->toUtf8().data());
		} else {
			append_track = this->get_track(iter->toUtf8().data());
		}

		if (append_track) {
			trk->steal_and_append_trackpoints(append_track);
			if (trk->sublayer_type == SublayerType::ROUTE) {
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
	sg_uid_t uid = this->menu_data->sublayer->uid;

	std::unordered_map<sg_uid_t, SlavGPS::Track*> * ght_mykind;
	std::unordered_map<sg_uid_t, SlavGPS::Track*> * ght_others;
	if (this->menu_data->sublayer->sublayer_type == SublayerType::ROUTE) {
		ght_mykind = &this->routes;
		ght_others = &this->tracks;
	} else {
		ght_mykind = &this->tracks;
		ght_others = &this->routes;
	}

	Track * trk = ght_mykind->at(uid);

	if (!trk) {
		return;
	}

	/* Convert into list of names for usage with dialog function.
	   TODO: Need to consider how to work best when we can have multiple tracks the same name... */

	std::list<QString> const other_tracks_names = LayerTRWc::get_sorted_track_name_list_exclude_self(ght_others, trk);

	/* Note the limit to selecting one track only.
	   this is to control the ordering of appending tracks, i.e. the selected track always goes after the current track
	   (otherwise with multiple select the ordering would not be controllable by the user - automatically being alphabetically). */
	std::list<QString> append_list = a_dialog_select_from_list(this->get_window(),
								   other_tracks_names,
								   false,

								   trk->sublayer_type == SublayerType::ROUTE
								   ? QString(_("Append Track"))
								   : QString(_("Append Route")),

								   trk->sublayer_type == SublayerType::ROUTE
								   ? QString(_("Select the track to append after the current route"))
								   : QString(_("Select the route to append after the current track")));

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
		if (trk->sublayer_type == SublayerType::ROUTE) {
			append_track = this->get_track(iter->toUtf8().data());
		} else {
			append_track = this->get_route(iter->toUtf8().data());
		}

		if (append_track) {

			if (append_track->sublayer_type != SublayerType::ROUTE
			    && ((append_track->get_segment_count() > 1)
				|| (append_track->get_average_speed() > 0.0))) {

				if (dialog_yes_or_no("Converting a track to a route removes extra track data such as segments, timestamps, etc...\nDo you want to continue?", this->get_window())) {
					append_track->merge_segments();
					append_track->to_routepoints();
				} else {
					break;
				}
			}

			trk->steal_and_append_trackpoints(append_track);

			/* Delete copied which is FROM THE OTHER TYPE list. */
			if (trk->sublayer_type == SublayerType::ROUTE) {
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
	sg_uid_t uid = this->menu_data->sublayer->uid;
	Track * trk = this->tracks.at(uid);
	unsigned int segments = trk->merge_segments();
	/* NB currently no need to redraw as segments not actually shown on the display.
	   However inform the user of what happened: */
	char str[64];
	const char *tmp_str = ngettext("%d segment merged", "%d segments merged", segments);
	snprintf(str, 64, tmp_str, segments);
	dialog_info(str, this->get_window());
}




/* merge by time routine */
void LayerTRW::merge_by_timestamp_cb(void)
{
	sg_uid_t uid = this->menu_data->sublayer->uid;

	//time_t t1, t2;

	Track * orig_trk = this->tracks.at(uid);

	if (!orig_trk->empty()
	    && !orig_trk->get_tp_first()->has_timestamp) {
		dialog_error("Failed. This track does not have timestamp", this->get_window());
		return;
	}

#ifdef K

	std::list<sg_uid_t> * tracks_with_timestamp = LayerTRWc::find_tracks_with_timestamp_type(&this->tracks, true, orig_trk);
	tracks_with_timestamp = g_list_reverse(tracks_with_timestamp);

	if (!tracks_with_timestamp) {
		dialog_error("Failed. No other track in this layer has timestamp", this->get_window());
		return;
	}
	g_list_free(tracks_with_timestamp);

	static unsigned int threshold_in_minutes = 1;
	if (!a_dialog_time_threshold(this->get_window(),
				     _("Merge Threshold..."),
				     _("Merge when time between tracks less than:"),
				     &threshold_in_minutes)) {
		return;
	}

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
		nearby_tracks = LayerTRWc::find_nearby_tracks_by_time(this->tracks, orig_trk, (threshold_in_minutes * 60));

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
void LayerTRW::split_at_selected_trackpoint(SublayerType sublayer_type)
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

	char * name_ = this->new_unique_sublayer_name(sublayer_type, this->current_trk->name);
	if (!name_) {
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

	new_track->set_name(name_);
	free(name_);

	this->add_track(new_track);

	this->emit_changed();
}




/* split by time routine */
void LayerTRW::split_by_timestamp_cb(void)
{
	LayersPanel * panel = this->get_window()->get_layers_panel();
	sg_uid_t uid = this->menu_data->sublayer->uid;
	Track * trk = this->tracks.at(uid);

	static unsigned int thr = 1;

	if (trk->empty()) {
		return;
	}
#ifdef K
	if (!a_dialog_time_threshold(this->get_window(),
				     _("Split Threshold..."),
				     _("Split when time between trackpoints exceeds:"),
				     &thr)) {
		return;
	}
#endif

	/* Iterate through trackpoints, and copy them into new lists without touching original list. */
	auto iter = trk->trackpointsB->begin();
	time_t prev_ts = (*iter)->timestamp;

	TrackPoints * newtps = new TrackPoints;
	std::list<TrackPoints *> points;

	for (; iter != trk->trackpointsB->end(); iter++) {
		time_t ts = (*iter)->timestamp;

		/* Check for unordered time points - this is quite a rare occurence - unless one has reversed a track. */
		if (ts < prev_ts) {
			char tmp_str[64];
			strftime(tmp_str, sizeof(tmp_str), "%c", localtime(&ts));

			if (dialog_yes_or_no(QString("Can not split track due to trackpoints not ordered in time - such as at %1.\n\nGoto this trackpoint?").arg(QString(tmp_str))), this->get_window()) {
				goto_coord(panel, this, this->menu_data->viewport, &(*iter)->coord);
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

	int n_points = a_dialog_get_positive_number(this->get_window(),
						    QString(_("Split Every Nth Point")),
						    QString(_("Split on every Nth point:")),
						    250,   /* Default value as per typical limited track capacity of various GPS devices. */
						    2,     /* Min */
						    65536, /* Max */
						    5);    /* Step */
	/* Was a valid number returned? */
	if (!n_points) {
		return;
	}

	/* Now split. */
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
	for (auto iter = points->begin(); iter != points->end(); iter++) {

		Track * copy = new Track(*orig, (*iter)->begin(), (*iter)->end());

		char * new_tr_name = NULL;
		if (orig->sublayer_type == SublayerType::ROUTE) {
			new_tr_name = this->new_unique_sublayer_name(SublayerType::ROUTE, orig->name);
			copy->set_name(new_tr_name);
			this->add_route(copy);
		} else {
			new_tr_name = this->new_unique_sublayer_name(SublayerType::TRACK, orig->name);
			copy->set_name(new_tr_name);
			this->add_track(copy);
		}
		free(new_tr_name);
		copy->calculate_bounds();
	}

	/* Remove original track and then update the display. */
	if (orig->sublayer_type == SublayerType::ROUTE) {
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
	this->split_at_selected_trackpoint(this->menu_data->sublayer->sublayer_type);
}




/**
 * Split a track by its segments
 * Routes do not have segments so don't call this for routes
 */
void LayerTRW::split_segments_cb(void)
{
	sg_uid_t uid = this->menu_data->sublayer->uid;
	Track *trk = this->tracks.at(uid);

	if (!trk) {
		return;
	}

	std::list<Track *> * tracks_ = trk->split_into_segments();
	for (auto iter = tracks_->begin(); iter != tracks_->end(); iter++) {
		if (*iter) {
			char * new_tr_name = this->new_unique_sublayer_name(SublayerType::TRACK, trk->name);
			(*iter)->set_name(new_tr_name);
			free(new_tr_name);

			this->add_track(*iter);
		}
	}
	if (tracks_) {
		delete tracks_;
		/* Remove original track. */
		this->delete_track(trk);
		this->emit_changed();
	} else {
		dialog_error("Can not split track as it has no segments", this->get_window());
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
	dialog_info(str, this->get_window());

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
	dialog_info(str, this->get_window());

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
		dialog_error(QString("Could not launch %1 to open file.").arg(QString(diary_program)), this->get_window());
		g_error_free(err);
	}
	free(cmd);
}




/**
 * Open a diary at the date of the track or waypoint
 */
void LayerTRW::diary_cb(void)
{
	sg_uid_t uid = this->menu_data->sublayer->uid;

	if (this->menu_data->sublayer->sublayer_type == SublayerType::TRACK) {
		Track * trk = this->tracks.at(uid);
		if (!trk) {
			return;
		}

		char date_buf[20];
		date_buf[0] = '\0';
		if (!trk->empty() && (*trk->trackpointsB->begin())->has_timestamp) {
			strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", gmtime(&(*trk->trackpointsB->begin())->timestamp));
			this->diary_open(date_buf);
		} else {
			dialog_info("This track has no date information.", this->get_window());
		}
	} else if (this->menu_data->sublayer->sublayer_type == SublayerType::WAYPOINT) {
		Waypoint * wp = this->waypoints.at(uid);
		if (!wp) {
			return;
		}

		char date_buf[20];
		date_buf[0] = '\0';
		if (wp->has_timestamp) {
			strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", gmtime(&(wp->timestamp)));
			this->diary_open(date_buf);
		} else {
			dialog_info("This waypoint has no date information.", this->get_window());
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
		dialog_error(QString("Could not launch %1").arg(QString(astro_program)), this->get_window());
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
	sg_uid_t uid = this->menu_data->sublayer->uid;

	if (this->menu_data->sublayer->sublayer_type == SublayerType::TRACK) {
		Track * trk = this->tracks.at(uid);
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
			struct LatLon ll;
			vik_coord_to_latlon(&tp->coord, &ll);
			char *lat_str = convert_to_dms(ll.lat);
			char *lon_str = convert_to_dms(ll.lon);
			char alt_buf[20];
			snprintf(alt_buf, sizeof(alt_buf), "%d", (int)round(tp->altitude));
			this->astro_open(date_buf, time_buf, lat_str, lon_str, alt_buf);
			free(lat_str);
			free(lon_str);
		} else {
			dialog_info("This track has no date information.", this->get_window());
		}
	} else if (this->menu_data->sublayer->sublayer_type == SublayerType::WAYPOINT) {
		sg_uid_t wp_uid = this->menu_data->sublayer->uid;
		Waypoint * wp = this->waypoints.at(wp_uid);
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
			this->astro_open(date_buf, time_buf, lat_str, lon_str, alt_buf);
			free(lat_str);
			free(lon_str);
		} else {
			dialog_info("This waypoint has no date information.", this->get_window());
		}
	}
}




/**
 * Force unqiue track names for the track table specified
 * Note the panel is a required parameter to enable the update of the names displayed
 * Specify if on tracks or else on routes
 */
void LayerTRW::uniquify_tracks(LayersPanel * panel, Tracks & tracks_table, bool ontrack)
{
	if (tracks_table.empty()) {
		qDebug() << "EE: Layer TRW: ::uniquify() called for empty tracks/routes set";
		return;
	}

	/*
	  - Search tracks set for an instance of repeated name
	  - get track with this name
	  - create new name
	  - rename track & update equiv. treeview iter
	  - repeat until all different
	*/

	/* TODO: make the ::has_duplicate_track_names() return the track/route itself (or NULL). */
	QString duplicate_name = LayerTRWc::has_duplicate_track_names(tracks_table);
	while (duplicate_name != "") {

		/* Get the track with duplicate name. */
		Track * trk;
		if (ontrack) {
			trk = this->get_track(duplicate_name.toUtf8().data());
		} else {
			trk = this->get_route(duplicate_name.toUtf8().data());
		}

		if (!trk) {
			/* Broken :( */
			qDebug() << "EE: Layer TRW: can't retrieve track/route with duplicate name" << duplicate_name;
			this->get_window()->get_statusbar()->set_message(StatusBarField::INFO, _("Internal Error during making tracks/routes unique"));
			return;
		}

		/* Rename it. */
		char * newname = this->new_unique_sublayer_name(SublayerType::TRACK, trk->name);
		trk->set_name(newname);

		/* TODO: do we really need to do this? Isn't the name in tree view auto-updated? */
		if (trk->index.isValid()) {
			this->tree_view->set_name(trk->index, newname);
			if (ontrack) {
				this->tree_view->sort_children(this->tracks_node->get_index(), this->track_sort_order);
			} else {
				this->tree_view->sort_children(this->routes_node->get_index(), this->track_sort_order);
			}
		}
		free(newname);
		newname = NULL;

		/* Try to find duplicate names again in the updated set of tracks. */
		QString duplicate_name_ = LayerTRWc::has_duplicate_track_names(tracks_table); /* kamilTODO: there is a variable in this class with this name. */
	}

	/* Update. */
	panel->emit_update_cb();
}




void LayerTRW::sort_order_specified(SublayerType sublayer_type, vik_layer_sort_order_t order)
{
	TreeIndex tree_index;

	switch (sublayer_type) {
	case SublayerType::TRACKS:
		tree_index = this->tracks_node->get_index();
		this->track_sort_order = order;
		break;
	case SublayerType::ROUTES:
		tree_index = this->routes_node->get_index();
		this->track_sort_order = order;
		break;
	default: // SublayerType::WAYPOINTS:
		tree_index = this->waypoints_node->get_index();
		this->wp_sort_order = order;
		break;
	}

	this->tree_view->sort_children(tree_index, order);
}




void LayerTRW::sort_order_a2z_cb(void)
{
	this->sort_order_specified(this->menu_data->sublayer->sublayer_type, VL_SO_ALPHABETICAL_ASCENDING);
}




void LayerTRW::sort_order_z2a_cb(void)
{
	this->sort_order_specified(this->menu_data->sublayer->sublayer_type, VL_SO_ALPHABETICAL_DESCENDING);
}




void LayerTRW::sort_order_timestamp_ascend_cb(void)
{
	this->sort_order_specified(this->menu_data->sublayer->sublayer_type, VL_SO_DATE_ASCENDING);
}




void LayerTRW::sort_order_timestamp_descend_cb(void)
{
	this->sort_order_specified(this->menu_data->sublayer->sublayer_type, VL_SO_DATE_DESCENDING);
}




void LayerTRW::delete_selected_tracks_cb(void) /* Slot. */
{
	LayersPanel * panel = this->get_window()->get_layers_panel();

	/* Ensure list of track names offered is unique. */
	QString duplicate_name = LayerTRWc::has_duplicate_track_names(this->tracks);
	if (duplicate_name != "") {
		if (dialog_yes_or_no(QString("Multiple entries with the same name exist. This method only works with unique names. Force unique names now?")), this->get_window()) {
			this->uniquify_tracks(panel, this->tracks, true);
		} else {
			return;
		}
	}

	/* Sort list alphabetically for better presentation. */
	std::list<QString> const all = LayerTRWc::get_sorted_track_name_list(this->tracks);

	if (all.empty()) {
		dialog_error("No tracks found", this->get_window());
		return;
	}

	/* Get list of items to delete from the user. */
	std::list<QString> delete_list = a_dialog_select_from_list(this->get_window(),
								   all,
								   true,
								   QString(_("Delete Selection")),
								   QString(_("Select tracks to delete")));

	if (delete_list.empty()) {
		return;
	}

	/* Delete requested tracks.
	   Since specificly requested, IMHO no need for extra confirmation. */

	for (auto iter = delete_list.begin(); iter != delete_list.end(); iter++) {
		/* This deletes first trk it finds of that name (but uniqueness is enforced above). */
		this->delete_track_by_name(iter->toUtf8().data(), false);
	}

	/* Reset layer timestamps in case they have now changed. */
	this->tree_view->set_timestamp(this->index, this->get_timestamp());

	this->emit_changed();
}




void LayerTRW::delete_selected_routes_cb(void) /* Slot. */
{
	LayersPanel * panel = this->get_window()->get_layers_panel();

	/* Ensure list of track names offered is unique. */
	QString duplicate_name = LayerTRWc::has_duplicate_track_names(this->routes);
	if (duplicate_name != "") {
		if (dialog_yes_or_no("Multiple entries with the same name exist. This method only works with unique names. Force unique names now?", this->get_window())) {
			this->uniquify_tracks(panel, this->routes, false);
		} else {
			return;
		}
	}

	/* Sort list alphabetically for better presentation. */
	std::list<QString> all = LayerTRWc::get_sorted_track_name_list(this->routes);

	if (all.empty()) {
		dialog_error("No routes found", this->get_window());
		return;
	}

	/* Get list of items to delete from the user. */
	std::list<QString> delete_list = a_dialog_select_from_list(this->get_window(),
								   all,
								   true,
								   QString(_("Delete Selection")),
								   QString(_("Select routes to delete")));

	/* Delete requested routes.
	   Since specifically requested, IMHO no need for extra confirmation. */
	if (delete_list.empty()) {
		return;
	}

	for (auto iter = delete_list.begin(); iter != delete_list.end(); iter++) {
		/* This deletes first route it finds of that name (but uniqueness is enforced above). */
		this->delete_track_by_name(iter->toUtf8().data(), true);
	}

	this->emit_changed();
}




/**
 * Force unqiue waypoint names for this layer.
 * Note the panel is a required parameter to enable the update of the names displayed.
 */
void LayerTRW::uniquify_waypoints(LayersPanel * panel)
{
	if (this->waypoints.empty()) {
		qDebug() << "EE: Layer TRW: ::uniquify() called for empty waypoints set";
		return;
	}

	/*
	  - Search waypoints set for an instance of repeated name
	  - get waypoint with this name
	  - create new name
	  - rename waypoint
	  - repeat until there are no waypoints with duplicate names
	*/

	/* TODO: make the ::has_duplicate_waypoint_names() return the waypoint itself (or NULL). */
	QString duplicate_name = LayerTRWc::has_duplicate_waypoint_names(this->waypoints);
	while (duplicate_name != "") {

		/* Get that waypoint that has duplicate name. */
		Waypoint * wp = this->get_waypoint(duplicate_name.toUtf8().data());
		if (!wp) {
			/* Broken :( */
			qDebug() << "EE: Layer TRW: can't retrieve waypoint with duplicate name" << duplicate_name;
			this->get_window()->get_statusbar()->set_message(StatusBarField::INFO, QString(_("Internal Error during making waypoints unique")));
			return;
		}

		/* Rename it. */
		char * newname = this->new_unique_sublayer_name(SublayerType::WAYPOINT, wp->name);
		this->waypoint_rename(wp, newname);
		free(newname);
		newname = NULL;

		/* Try to find duplicate names again in the updated set of waypoints. */
		duplicate_name = LayerTRWc::has_duplicate_waypoint_names(this->waypoints);
	}

	/* Update. */
	panel->emit_update_cb();
}




void LayerTRW::delete_selected_waypoints_cb(void)
{
	/* Ensure list of waypoint names offered is unique. */
	QString duplicate_name = LayerTRWc::has_duplicate_waypoint_names(this->waypoints);
	if (duplicate_name != "") {
		if (dialog_yes_or_no("Multiple entries with the same name exist. This method only works with unique names. Force unique names now?", this->get_window())) {
			this->uniquify_waypoints(this->get_window()->get_layers_panel());
		} else {
			return;
		}
	}

	/* Sort list alphabetically for better presentation. */
	std::list<QString> all = LayerTRWc::get_sorted_wp_name_list(this->waypoints);
	if (all.empty()) {
		dialog_error("No waypoints found", this->get_window());
		return;
	}

	/* Get list of items to delete from the user. */
	std::list<QString> delete_list = a_dialog_select_from_list(this->get_window(),
								   all,
								   true,
								   QString(_("Delete Selection")),
								   QString(_("Select waypoints to delete")));

	if (delete_list.empty()) {
		return;
	}

	/* Delete requested waypoints.
	   Since specifically requested, IMHO no need for extra confirmation. */
	for (auto iter = delete_list.begin(); iter != delete_list.end(); iter++) {
		/* This deletes first waypoint it finds of that name (but uniqueness is enforced above). */
		this->delete_waypoint_by_name(iter->toUtf8().data());
	}

	this->calculate_bounds_waypoints();
	/* Reset layer timestamp in case it has now changed. */
	this->tree_view->set_timestamp(this->index, this->get_timestamp());
	this->emit_changed();

}




void LayerTRW::waypoints_visibility_off_cb(void) /* Slot. */
{
	LayerTRWc::set_waypoints_visibility(this->waypoints, false);
	/* Redraw. */
	this->emit_changed();
}




void LayerTRW::waypoints_visibility_on_cb(void) /* Slot. */
{
	LayerTRWc::set_waypoints_visibility(this->waypoints, true);
	/* Redraw. */
	this->emit_changed();
}




void LayerTRW::waypoints_visibility_toggle_cb(void) /* Slot. */
{
	LayerTRWc::waypoints_toggle_visibility(this->waypoints);
	/* Redraw. */
	this->emit_changed();
}




void LayerTRW::tracks_visibility_off_cb(void) /* Slot. */
{
	LayerTRWc::set_tracks_visibility(this->tracks, false);
	/* Redraw. */
	this->emit_changed();
}




void LayerTRW::tracks_visibility_on_cb(void) /* Slot. */
{
	LayerTRWc::set_tracks_visibility(this->tracks, true);
	/* Redraw. */
	this->emit_changed();
}




void LayerTRW::tracks_visibility_toggle_cb(void) /* Slot. */
{
	LayerTRWc::tracks_toggle_visibility(this->tracks);
	/* Redraw. */
	this->emit_changed();
}




void LayerTRW::routes_visibility_off_cb(void) /* Slot. */
{
	LayerTRWc::set_tracks_visibility(this->routes, false);
	/* Redraw. */
	this->emit_changed();
}




void LayerTRW::routes_visibility_on_cb() /* Slot. */
{
	LayerTRWc::set_tracks_visibility(this->routes, true);
	/* Redraw. */
	this->emit_changed();
}




void LayerTRW::routes_visibility_toggle_cb(void) /* Slot. */
{
	LayerTRWc::tracks_toggle_visibility(this->routes);
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

	for (auto iter = this->waypoints.begin(); iter != this->waypoints.end(); iter++) {
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
 * Create the latest list of tracks with the associated layer(s).
 * Although this will always be from a single layer here.
 */
std::list<track_layer_t *> * LayerTRW::create_tracks_and_layers_list(SublayerType sublayer_type)
{
	std::list<Track *> * tracks_ = new std::list<Track *>;
	if (sublayer_type == SublayerType::TRACKS) {
		tracks_ = LayerTRWc::get_track_values(tracks_, this->get_tracks());
	} else {
		tracks_ = LayerTRWc::get_track_values(tracks_, this->get_routes());
	}

	return this->create_tracks_and_layers_list_helper(tracks_);
}




void LayerTRW::tracks_stats_cb(void)
{
	layer_trw_show_stats(this->get_window(), QString(this->name), this, SublayerType::TRACKS);
}




void LayerTRW::routes_stats_cb(void)
{
	layer_trw_show_stats(this->get_window(), QString(this->name), this, SublayerType::ROUTES);
}




void LayerTRW::go_to_selected_waypoint_cb(void)
{
	LayersPanel * panel = this->get_window()->get_layers_panel();
	sg_uid_t wp_uid = this->menu_data->sublayer->uid;
	Waypoint * wp = this->waypoints.at(wp_uid);
	if (wp) {
		goto_coord(panel, this, this->menu_data->viewport, &wp->coord);
	}
}




void LayerTRW::waypoint_geocache_webpage_cb(void)
{
	sg_uid_t wp_uid = this->menu_data->sublayer->uid;
	Waypoint * wp = this->waypoints.at(wp_uid);
	if (!wp) {
		return;
	}
	char *webpage = g_strdup_printf("http://www.geocaching.com/seek/cache_details.aspx?wp=%s", wp->name);
	open_url(this->get_window(), webpage);
	free(webpage);
}




void LayerTRW::waypoint_webpage_cb(void)
{
	sg_uid_t wp_uid = this->menu_data->sublayer->uid;
	Waypoint * wp = this->waypoints.at(wp_uid);
	if (!wp) {
		return;
	}
	if (wp->url) {
		open_url(this->get_window(), wp->url);
	} else if (!strncmp(wp->comment, "http", 4)) {
		open_url(this->get_window(), wp->comment);
	} else if (!strncmp(wp->description, "http", 4)) {
		open_url(this->get_window(), wp->description);
	}
}




char const * LayerTRW::sublayer_rename_request(Sublayer * sublayer, const char * newname, LayersPanel * panel)
{
	if (sublayer->sublayer_type == SublayerType::WAYPOINT) {
		Waypoint * wp = this->waypoints.at(sublayer->uid);

		/* No actual change to the name supplied. */
		if (wp->name) {
			if (strcmp(newname, wp->name) == 0) {
				return NULL;
			}
		}

		Waypoint * wpf = this->get_waypoint(newname);

		if (wpf) {
			/* An existing waypoint has been found with the requested name. */
			if (!dialog_yes_or_no(QString("A waypoint with the name \"%1\" already exists. Really rename to the same name?").arg(QString(newname)), this->get_window())) {
				return NULL;
			}
		}

		/* Update WP name and refresh the treeview. */
		wp->set_name(newname);

		this->tree_view->set_name(sublayer->index, newname);
		this->tree_view->sort_children(this->waypoints_node->get_index(), this->wp_sort_order);

		panel->emit_update_cb();

		return newname;
	}

	if (sublayer->sublayer_type == SublayerType::TRACK) {
		Track * trk = this->tracks.at(sublayer->uid);

		/* No actual change to the name supplied. */
		if (trk->name) {
			if (strcmp(newname, trk->name) == 0) {
				return NULL;
			}
		}

		Track *trkf = this->get_track((const char *) newname);

		if (trkf) {
			/* An existing track has been found with the requested name. */
			if (!dialog_yes_or_no(QString("A track with the name \"%1\" already exists. Really rename to the same name?").arg(QString(newname)), this->get_window())) {
				return NULL;
			}
		}
		/* Update track name and refresh GUI parts. */
		trk->set_name(newname);

		/* Update any subwindows that could be displaying this track which has changed name.
		   Only one Track Edit Window. */
		if (this->current_trk == trk && this->tpwin) {
			this->tpwin->set_track_name(newname);
		}

		/* Update the dialog windows if any of them is visible. */
		trk->update_properties_dialog();
		trk->update_profile_dialog();

		this->tree_view->set_name(sublayer->index, newname);
		this->tree_view->sort_children(this->tracks_node->get_index(), this->track_sort_order);

		panel->emit_update_cb();

		return newname;
	}

	if (sublayer->sublayer_type == SublayerType::ROUTE) {
		Track * trk = this->routes.at(sublayer->uid);

		/* No actual change to the name supplied. */
		if (trk->name) {
			if (strcmp(newname, trk->name) == 0) {
				return NULL;
			}
		}

		Track * trkf = this->get_route((const char *) newname);

		if (trkf) {
			/* An existing track has been found with the requested name. */
			if (!dialog_yes_or_no(QString("A route with the name \"%1\" already exists. Really rename to the same name?").arg(QString(newname)), this->get_window())) {
				return NULL;
			}
		}
		/* Update track name and refresh GUI parts. */
		trk->set_name(newname);

		/* Update any subwindows that could be displaying this track which has changed name.
		   Only one Track Edit Window. */
		if (this->current_trk == trk && this->tpwin) {
			this->tpwin->set_track_name(newname);
		}

		/* Update the dialog windows if any of them is visible. */
		trk->update_properties_dialog();
		trk->update_profile_dialog();

		this->tree_view->set_name(sublayer->index, newname);
		this->tree_view->sort_children(this->tracks_node->get_index(), this->track_sort_order);

		panel->emit_update_cb();

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
void LayerTRW::track_use_with_filter_cb(void)
{
	sg_uid_t uid = this->menu_data->sublayer->uid;
	Track * trk = this->tracks.at(uid);
	a_acquire_set_filter_track(trk);
}
#endif




#ifdef VIK_CONFIG_GOOGLE


bool LayerTRW::is_valid_google_route(sg_uid_t track_uid)
{
	Track * trk = this->routes.at(track_uid);
	return (trk && trk->comment && strlen(trk->comment) > 7 && !strncmp(trk->comment, "from:", 5));
}




void LayerTRW::google_route_webpage_cb(void)
{
	sg_uid_t uid = this->menu_data->sublayer->uid;
	Track * trk = this->routes.at(uid);
	if (trk) {
		char *escaped = uri_escape(trk->comment);
		char *webpage = g_strdup_printf("http://maps.google.com/maps?f=q&hl=en&q=%s", escaped);
		open_url(this->get_window(), webpage);
		free(escaped);
		free(webpage);
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
	VikCoord vc;
	/* Notional center of a track is simply an average of the bounding box extremities. */
	struct LatLon center = { (trk->bbox.north+trk->bbox.south)/2, (trk->bbox.east+trk->bbox.west)/2 };
	vik_coord_load_from_latlon(&vc, this->coord_mode, &center);
	this->tpwin->set_tp(trk, &this->selected_tp.iter, trk->name, trk->sublayer_type == SublayerType::ROUTE);
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

		this->split_at_selected_trackpoint(this->current_trk->sublayer_type);
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
void LayerTRW::dialog_shift(QDialog * dialog, VikCoord * coord, bool vertical)
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
	ThumbnailCreator(LayerTRW * layer, GSList * pics_);
	~ThumbnailCreator();

	LayerTRW * layer = NULL;  /* Layer needed for redrawing. */
	GSList * pics = NULL;     /* Image list. */
};




ThumbnailCreator::ThumbnailCreator(LayerTRW * layer_, GSList * pics_)
{
	this->thread_fn = create_thumbnails_thread;
	this->n_items = g_slist_length(pics_);

	layer = layer_;
	pics = pics_;
}




static int create_thumbnails_thread(BackgroundJob * bg_job)
{
	ThumbnailCreator * creator = (ThumbnailCreator *) bg_job;

	unsigned int total = g_slist_length(creator->pics), done = 0;
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
	while (this->pics) {
		free(this->pics->data);
		this->pics = g_slist_delete_link(this->pics, this->pics);
	}
}




void LayerTRW::verify_thumbnails(void)
{
	if (this->has_verified_thumbnails) {
		return;
	}

	GSList * pics = LayerTRWc::image_wp_make_list(this->waypoints);
	if (!pics) {
		return;
	}

	int len = g_slist_length(pics);
	const QString job_description = QString(tr("Creating %1 Image Thumbnails...")).arg(len);
	ThumbnailCreator * creator = new ThumbnailCreator(this, pics);
	a_background_thread(creator, ThreadPoolType::LOCAL, job_description);
}




static const char * my_track_colors(int ii)
{
	static const char * colors[TRW_LAYER_TRACK_COLORS_MAX] = {
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
	/* Fast and reliable way of returning a colour. */
	return colors[(ii % TRW_LAYER_TRACK_COLORS_MAX)];
}




void LayerTRW::track_alloc_colors()
{
	/* Tracks. */
	int ii = 0;
	for (auto i = this->tracks.begin(); i != this->tracks.end(); i++) {

		Track * trk = i->second;

		/* Tracks get a random spread of colours if not already assigned. */
		if (!trk->has_color) {
			if (this->drawmode == DRAWMODE_ALL_SAME_COLOR) {
				trk->color = this->track_color;
			} else {
				trk->color.setNamedColor(QString(my_track_colors(ii)));
			}
			trk->has_color = true;
		}

		this->update_treeview(trk);

		ii++;
		if (ii > TRW_LAYER_TRACK_COLORS_MAX) {
			ii = 0;
		}
	}

	/* Routes. */
	ii = 0;
	for (auto i = this->routes.begin(); i != this->routes.end(); i++) {

		Track * trk = i->second;

		/* Routes get an intermix of reds. */
		if (!trk->has_color) {
			if (ii) {
				trk->color.setNamedColor("#FF0000"); /* Red. */
			} else {
				trk->color.setNamedColor("#B40916"); /* Dark Red. */
			}
			trk->has_color = true;
		}

		this->update_treeview(trk);

		ii = !ii;
	}
}




/*
 * (Re)Calculate the bounds of the waypoints in this layer.
 * This should be called whenever waypoints are changed.
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
	/* Set bounds to first point. */
	if (wp) {
		vik_coord_to_latlon(&wp->coord, &topleft);
		vik_coord_to_latlon(&wp->coord, &bottomright);
	}

	/* Ensure there is another point... */
	if (this->waypoints.size() > 1) {

		while (++i != this->waypoints.end()) { /* kamilTODO: check the conditon. */

			wp = i->second;

			/* See if this point increases the bounds. */
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




void LayerTRW::calculate_bounds_track(Tracks & tracks)
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

	/* Obviously need 2 to tango - sorting with only 1 (or less) is a lonely activity! */
	if (this->tracks.size() > 1) {
		this->tree_view->sort_children(this->tracks_node->get_index(), this->track_sort_order);
	}

	if (this->routes.size() > 1) {
		this->tree_view->sort_children(this->routes_node->get_index(), this->track_sort_order);
	}

	if (this->waypoints.size() > 1) {
		this->tree_view->sort_children(this->waypoints_node->get_index(), this->wp_sort_order);
	}
}




/**
 * Get the earliest timestamp available from all tracks.
 */
time_t LayerTRW::get_timestamp_tracks()
{
	time_t timestamp = 0;
	std::list<Track *> * tracks_ = new std::list<Track *>;
	tracks_ = LayerTRWc::get_track_values(tracks_, this->tracks);

	if (!tracks_->empty()) {
		tracks_->sort(Track::compare_timestamp);

		/* Only need to check the first track as they have been sorted by time. */
		Track * trk = *(tracks_->begin());
		/* Assume trackpoints already sorted by time. */
		Trackpoint * tpt = trk->get_tp_first();
		if (tpt && tpt->has_timestamp) {
			timestamp = tpt->timestamp;
		}
	}
	delete tracks_;
	return timestamp;
}




/**
 * Get the earliest timestamp available from all waypoints.
 */
time_t LayerTRW::get_timestamp_waypoints()
{
	time_t timestamp = 0;

	for (auto i = this->waypoints.begin(); i != this->waypoints.end(); i++) {
		Waypoint * wp = i->second;
		if (wp->has_timestamp) {
			/* When timestamp not set yet - use the first value encountered. */
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
 * Get the earliest timestamp available for this layer.
 */
time_t LayerTRW::get_timestamp()
{
	time_t timestamp_tracks = this->get_timestamp_tracks();
	time_t timestamp_waypoints = this->get_timestamp_waypoints();
	/* NB routes don't have timestamps - hence they are not considered. */

	if (!timestamp_tracks && !timestamp_waypoints) {
		/* Fallback to get time from the metadata when no other timestamps available. */
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
	if (this->connected_to_tree) {
		this->verify_thumbnails();
	}
	this->track_alloc_colors();

	this->calculate_bounds_waypoints();
	this->calculate_bounds_tracks();

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

			/* No time found - so use 'now' for the metadata time. */
			if (timestamp.tv_sec == 0) {
				g_get_current_time(&timestamp);
			}

			this->metadata->timestamp = g_time_val_to_iso8601(&timestamp);
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
		this->uniquify_tracks(panel, this->tracks, true);
		this->uniquify_tracks(panel, this->routes, false);
		this->uniquify_waypoints(panel);
		return true;
	}
	return false;
}




void LayerTRW::change_coord_mode(CoordMode dest_mode)
{
	if (this->coord_mode != dest_mode) {
		this->coord_mode = dest_mode;
		LayerTRWc::waypoints_convert(this->waypoints, dest_mode);
		LayerTRWc::tracks_convert(this->tracks, dest_mode);
		LayerTRWc::tracks_convert(this->routes, dest_mode);
	}
}




void LayerTRW::set_menu_selection(uint16_t selection)
{
	this->menu_selection = (LayerMenuItem) selection; /* kamil: invalid cast? */
}




uint16_t LayerTRW::get_menu_selection()
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




static VikCoord *get_next_coord(VikCoord *from, VikCoord *to, struct LatLon *dist, double gradient)
{
	if ((dist->lon >= ABS(to->east_west - from->east_west))
	    && (dist->lat >= ABS(to->north_south - from->north_south))) {

		return NULL;
	}

	VikCoord *coord = (VikCoord *) malloc(sizeof (VikCoord));
	coord->mode = CoordMode::LATLON;

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
	/* TODO: handle virtical track (to->east_west - from->east_west == 0). */
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




void vik_track_download_map(Track *tr, Layer * vml, double zoom_level)
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

			if ((wh.lon < ABS ((*cur_rect)->center.east_west - (*next_rect)->center.east_west))
			    || (wh.lat < ABS ((*cur_rect)->center.north_south - (*next_rect)->center.north_south))) {

				fillins = add_fillins(fillins, &(*cur_rect)->center, &(*next_rect)->center, &wh);
			}
		}
	} else {
		qDebug() << "WW: Layer TRW: 'download map' feature works only in Mercator mode";
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
		((LayerMap *) vml)->download_section(&(*rect_iter)->tl, &(*rect_iter)->br, zoom_level);
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
	Layer *vml;
	int selected_map;
	char *zoomlist[] = {(char *) "0.125", (char *) "0.25", (char *) "0.5", (char *) "1", (char *) "2", (char *) "4", (char *) "8", (char *) "16", (char *) "32", (char *) "64", (char *) "128", (char *) "256", (char *) "512", (char *) "1024", NULL };
	double zoom_vals[] = {0.125, 0.25, 0.5, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};
	int selected_zoom, default_zoom;

	LayersPanel * panel = this->menu_data->layers_panel;
	Track * trk = this->get_track_helper(this->menu_data->sublayer);

	if (!trk) {
		return;
	}

	Viewport * viewport = this->get_window()->get_viewport();

#ifdef K
	std::list<Layer *> * vmls = panel->get_all_layers_of_type(LayerType::MAP, true); /* Includes hidden map layer types. */
	int num_maps = vmls->size();
	if (!num_maps) {
		dialog_error("No map layer in use. Create one first", this->get_window());
		return;
	}

	/* Convert from list of vmls to list of names. Allowing the user to select one of them. */
	char **map_names = (char **) g_malloc_n(1 + num_maps, sizeof (char *));
	Layer ** map_layers = (Layer **) g_malloc_n(1 + num_maps, sizeof(Layer *));

	char **np = map_names;
	Layer **lp = map_layers;
	for (auto i = vmls->begin(); i != vmls->end(); i++) {
		vml = (Layer *) *i;
		*lp++ = vml;
		LayerMap * lm = (LayerMap *) vml;
		*np++ = lm->get_map_label();
	}
	/* Mark end of the array lists. */
	*lp = NULL;
	*np = NULL;

	double cur_zoom = viewport->get_zoom();
	for (default_zoom = 0; default_zoom < G_N_ELEMENTS(zoom_vals); default_zoom++) {
		if (cur_zoom == zoom_vals[default_zoom]) {
			break;
		}
	}
	default_zoom = (default_zoom == G_N_ELEMENTS(zoom_vals)) ? G_N_ELEMENTS(zoom_vals) - 1 : default_zoom;

	if (!a_dialog_map_n_zoom(this->get_window(), map_names, 0, zoomlist, default_zoom, &selected_map, &selected_zoom)) {
		goto done;
	}

	vik_track_download_map(trk, map_layers[selected_map], zoom_vals[selected_zoom]);

 done:
	for (int i = 0; i < num_maps; i++) {
		free(map_names[i]);
	}
	free(map_names);
	free(map_layers);

	delete vmls;
#endif
}




/* Lowest waypoint number calculation. */
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
	/* If is bigger that top, add it. */
	int new_wp_num = highest_wp_number_name_to_number(new_wp_name);
	if (new_wp_num > this->highest_wp_number) {
		this->highest_wp_number = new_wp_num;
	}
}




void LayerTRW::highest_wp_number_remove_wp(char const * old_wp_name)
{
	/* If wasn't top, do nothing. if was top, count backwards until we find one used. */
	int old_wp_num = highest_wp_number_name_to_number(old_wp_name);
	if (this->highest_wp_number == old_wp_num) {
		char buf[4];
		this->highest_wp_number--;

		snprintf(buf,4,"%03d", this->highest_wp_number);
		/* Search down until we find something that *does* exist. */

		while (this->highest_wp_number > 0 && !this->get_waypoint((const char *) buf)) {
		       this->highest_wp_number--;
		       snprintf(buf, 4, "%03d", this->highest_wp_number);
		}
	}
}




/* Get lowest unused number. */
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
 * Create the latest list of tracks and routes.
 */
static std::list<track_layer_t *> * trw_layer_create_tracks_and_layers_list_both(Layer * layer)
{
	std::list<Track *> * tracks = new std::list<Track *>;
	tracks = LayerTRWc::get_track_values(tracks, ((LayerTRW *) layer)->get_tracks());
	tracks = LayerTRWc::get_track_values(tracks, ((LayerTRW *) layer)->get_routes());
	return ((LayerTRW *) layer)->create_tracks_and_layers_list_helper(tracks);
}




/**
 * Create the latest list of tracks and routes.
 */
std::list<track_layer_t *> * LayerTRW::create_tracks_and_layers_list()
{
	std::list<Track *> * tracks_ = new std::list<Track *>;
	tracks_ = LayerTRWc::get_track_values(tracks_, this->get_tracks());
	tracks_ = LayerTRWc::get_track_values(tracks_, this->get_routes());
	return this->create_tracks_and_layers_list_helper(tracks_);
}




void LayerTRW::track_list_dialog_single_cb(void) /* Slot. */
{
	QString title;
	if (this->menu_data->sublayer->sublayer_type == SublayerType::TRACKS) {
		title = QString(_("%1: Track List")).arg(this->name);
	} else {
		title = QString(_("%1: Route List")).arg(this->name);
	}
	track_list_dialog(title, this, this->menu_data->sublayer->sublayer_type, false);
}




void LayerTRW::track_list_dialog_cb(void)
{
	QString title = QString(_("%1: Track and Route List")).arg(this->name);
	track_list_dialog(title, this, SublayerType::NONE, false);
}




void LayerTRW::waypoint_list_dialog_cb(void) /* Slot. */
{
	QString title = QString(_("%1: Waypoint List")).arg(QString(this->name));
	waypoint_list_dialog(title, this, false);
}




Track * LayerTRW::get_track_helper(Sublayer * sublayer)
{
	/* TODO: either get rid of this method, or reduce it to checking
	   of consistency between function argument and contents of this->table.at(). */
	if (sublayer->sublayer_type == SublayerType::ROUTE) {
		return this->routes.at(sublayer->uid);
	} else {
		return this->tracks.at(sublayer->uid);
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
	this->waypoints_visible = this->tracks_visible = this->routes_visible = true;

	this->metadata = new TRWMetadata();
	this->draw_sync_done = true;
	this->draw_sync_do = true;
	/* Everything else is 0, false or NULL. */

	this->rename(vik_trw_layer_interface.layer_name.toUtf8().constData());


#ifdef K
	this->wplabellayout = gtk_widget_create_pango_layout(viewport, NULL);
	pango_layout_set_font_description(this->wplabellayout, gtk_widget_get_style(viewport->font_desc));

	this->tracklabellayout = gtk_widget_create_pango_layout(viewport, NULL);
	pango_layout_set_font_description(this->tracklabellayout, gtk_widget_get_style(viewport->font_desc));
#endif

	this->new_track_pens();

	this->waypoint_pen = QPen(this->waypoint_color);
	this->waypoint_pen.setWidth(2);

	this->waypoint_text_pen = QPen(this->waypoint_text_color);
	this->waypoint_text_pen.setWidth(1);

	this->waypoint_bg_pen = QPen(this->waypoint_bg_color);
	this->waypoint_bg_pen.setWidth(1);
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
