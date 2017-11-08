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

#include <QDateTime>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "layer_trw.h"
#include "layer_trw_painter.h"
#include "layer_trw_tools.h"
#include "layer_trw_dialogs.h"
#include "layer_trw_waypoint_properties.h"
#include "layer_trw_waypoint_list.h"
#include "layer_trw_track_internal.h"
#include "layer_trw_track_list_dialog.h"
#include "layer_map.h"
#include "tree_view_internal.h"
#include "vikutils.h"
#include "file.h"
#include "dialog.h"
#include "dem.h"
//#include "dem_cache.h"
#include "background.h"
#include "util.h"
#include "application_state.h"
#include "globals.h"
#include "ui_builder.h"
#include "layers_panel.h"
#include "preferences.h"
#include "util.h"
#include "generic_tools.h"
#include "toolbox.h"
#include "thumbnails.h"


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
#include "routing.h"
#include "icons/icons.h"
#endif

#include "clipboard.h"
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




extern Tree * g_tree;

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
	PARAM_WP_IMAGE_DRAW,
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


ParameterSpecification trw_layer_param_specs[] = {
	{ PARAM_TRACKS_VISIBLE,          NULL, "tracks_visible",    SGVariantType::BOOLEAN, PARAMETER_GROUP_HIDDEN,  QObject::tr("Tracks are visible"),      WidgetType::CHECKBUTTON,  NULL,                        sg_variant_true,            NULL, NULL },
	{ PARAM_WAYPOINTS_VISIBLE,       NULL, "waypoints_visible", SGVariantType::BOOLEAN, PARAMETER_GROUP_HIDDEN,  QObject::tr("Waypoints are visible"),                WidgetType::CHECKBUTTON,  NULL,                        sg_variant_true,            NULL, NULL },
	{ PARAM_ROUTES_VISIBLE,          NULL, "routes_visible",    SGVariantType::BOOLEAN, PARAMETER_GROUP_HIDDEN,  QObject::tr("Routes are visible"),                   WidgetType::CHECKBUTTON,  NULL,                        sg_variant_true,            NULL, NULL },

	{ PARAM_DRAW_TRACK_LABELS,       NULL, "trackdrawlabels",   SGVariantType::BOOLEAN, GROUP_TRACKS,            QObject::tr("Draw Labels"),                      WidgetType::CHECKBUTTON,  NULL,                        sg_variant_true,            NULL, N_("Note: the individual track controls what labels may be displayed") },
	{ PARAM_TRACK_LABEL_FONT_SIZE,   NULL, "trackfontsize",     SGVariantType::INT,     GROUP_TRACKS_ADV,        QObject::tr("Size of Track Label's Font:"),      WidgetType::COMBOBOX,     &params_font_sizes,          tnfontsize_default,         NULL, NULL },
	{ PARAM_TRACK_DRAWING_MODE,      NULL, "drawmode",          SGVariantType::INT,     GROUP_TRACKS,            QObject::tr("Track Drawing Mode:"),              WidgetType::COMBOBOX,     &params_track_drawing_modes, track_drawing_mode_default, NULL, NULL },
	{ PARAM_TRACK_COLOR_COMMON,      NULL, "trackcolor",        SGVariantType::COLOR,   GROUP_TRACKS,            QObject::tr("All Tracks Color:"),                WidgetType::COLOR,        NULL,                        black_color_default,        NULL, N_("The color used when 'All Tracks Same Color' drawing mode is selected") },
	{ PARAM_DRAW_TRACK_LINES,        NULL, "drawlines",         SGVariantType::BOOLEAN, GROUP_TRACKS,            QObject::tr("Draw Track Lines"),                 WidgetType::CHECKBUTTON,  NULL,                        sg_variant_true,            NULL, NULL },
	{ PARAM_TRACK_THICKNESS,         NULL, "line_thickness",    SGVariantType::INT,     GROUP_TRACKS_ADV,        QObject::tr("Track Thickness:"),                 WidgetType::SPINBOX_INT,  &scale_track_thickness,      NULL,                       NULL, NULL },
	{ PARAM_DD,                      NULL, "drawdirections",    SGVariantType::BOOLEAN, GROUP_TRACKS,            QObject::tr("Draw Track Direction"),             WidgetType::CHECKBUTTON,  NULL,                        sg_variant_false,           NULL, NULL },
	{ PARAM_TRACK_DIRECTION_SIZE,    NULL, "trkdirectionsize",  SGVariantType::INT,     GROUP_TRACKS_ADV,        QObject::tr("Direction Size:"),                  WidgetType::SPINBOX_INT,  &scale_track_direction_size, NULL,                       NULL, NULL },
	{ PARAM_DRAW_TRACKPOINTS,        NULL, "drawpoints",        SGVariantType::BOOLEAN, GROUP_TRACKS,            QObject::tr("Draw Trackpoints:"),                WidgetType::CHECKBUTTON,  NULL,                        sg_variant_true,            NULL, NULL },
	{ PARAM_TRACKPOINT_SIZE,         NULL, "trkpointsize",      SGVariantType::INT,     GROUP_TRACKS_ADV,        QObject::tr("Trackpoint Size:"),                 WidgetType::SPINBOX_INT,  &scale_trackpoint_size,      NULL,                       NULL, NULL },
	{ PARAM_DE,                      NULL, "drawelevation",     SGVariantType::BOOLEAN, GROUP_TRACKS,            QObject::tr("Draw Elevation"),                   WidgetType::CHECKBUTTON,  NULL,                        sg_variant_false,           NULL, NULL },
	{ PARAM_TRACK_ELEVATION_FACTOR,  NULL, "elevation_factor",  SGVariantType::INT,     GROUP_TRACKS_ADV,        QObject::tr("Draw Elevation Height %:"),         WidgetType::HSCALE,       &scale_track_elevation_factor, NULL,                     NULL, NULL },
	{ PARAM_DRAW_TRACK_STOPS,        NULL, "drawstops",         SGVariantType::BOOLEAN, GROUP_TRACKS,            QObject::tr("Draw Track Stops:"),                WidgetType::CHECKBUTTON,  NULL,                        sg_variant_false,           NULL, N_("Whether to draw a marker when trackpoints are at the same position but over the minimum stop length apart in time") },
	{ PARAM_TRACK_MIN_STOP_LENGTH,   NULL, "stop_length",       SGVariantType::INT,     GROUP_TRACKS_ADV,        QObject::tr("Min Stop Length (seconds):"),       WidgetType::SPINBOX_INT,  &scale_track_min_stop_length,NULL,                       NULL, NULL },

	{ PARAM_TRACK_BG_THICKNESS,      NULL, "bg_line_thickness", SGVariantType::INT,     GROUP_TRACKS_ADV,        QObject::tr("Track Background Thickness:"),      WidgetType::SPINBOX_INT,  &scale_track_bg_thickness,   NULL,                       NULL, NULL },
	{ PARAM_TRK_BG_COLOR,            NULL, "trackbgcolor",      SGVariantType::COLOR,   GROUP_TRACKS_ADV,        QObject::tr("Track Background Color"),           WidgetType::COLOR,        NULL,                        trackbgcolor_default,       NULL, NULL },
	{ PARAM_TRACK_DRAW_SPEED_FACTOR, NULL, "speed_factor",      SGVariantType::DOUBLE,  GROUP_TRACKS_ADV,        QObject::tr("Draw by Speed Factor (%):"),        WidgetType::HSCALE,       &scale_track_draw_speed_factor, NULL,                    NULL, N_("The percentage factor away from the average speed determining the color used") },
	{ PARAM_TRACK_SORT_ORDER,        NULL, "tracksortorder",    SGVariantType::INT,     GROUP_TRACKS_ADV,        QObject::tr("Track Sort Order:"),                WidgetType::COMBOBOX,     &params_sort_order,          sort_order_default,         NULL, NULL },

	{ PARAM_DLA,                     NULL, "drawlabels",        SGVariantType::BOOLEAN, GROUP_WAYPOINTS,         QObject::tr("Draw Labels"),                      WidgetType::CHECKBUTTON,  NULL,                        sg_variant_true,            NULL, NULL },
	{ PARAM_WP_LABEL_FONT_SIZE,      NULL, "wpfontsize",        SGVariantType::INT,     GROUP_WAYPOINTS,         QObject::tr("Font Size of Waypoint's Label:"),   WidgetType::COMBOBOX,     &params_font_sizes,          wpfontsize_default,         NULL, NULL },
	{ PARAM_WP_MARKER_COLOR,         NULL, "wpcolor",           SGVariantType::COLOR,   GROUP_WAYPOINTS,         QObject::tr("Color of Waypoint's Marker:"),      WidgetType::COLOR,        NULL,                        black_color_default,        NULL, NULL },
	{ PARAM_WP_LABEL_FG_COLOR,       NULL, "wptextcolor",       SGVariantType::COLOR,   GROUP_WAYPOINTS,         QObject::tr("Color of Waypoint's Label:"),       WidgetType::COLOR,        NULL,                        wptextcolor_default,        NULL, NULL },
	{ PARAM_WP_LABEL_BG_COLOR,       NULL, "wpbgcolor",         SGVariantType::COLOR,   GROUP_WAYPOINTS,         QObject::tr("Background of Waypoint's Label:"),  WidgetType::COLOR,        NULL,                        wpbgcolor_default,          NULL, NULL },
	{ PARAM_WPBA,                    NULL, "wpbgand",           SGVariantType::BOOLEAN, GROUP_WAYPOINTS,         QObject::tr("Fake BG Color Translucency:"),      WidgetType::CHECKBUTTON,  NULL,                        sg_variant_false,           NULL, NULL },
	{ PARAM_WP_MARKER_TYPE,          NULL, "wpsymbol",          SGVariantType::INT,     GROUP_WAYPOINTS,         QObject::tr("Type of Waypoint's Marker:"),       WidgetType::COMBOBOX,     &params_wpsymbols,           wpsymbol_default,           NULL, NULL },
	{ PARAM_WP_MARKER_SIZE,          NULL, "wpsize",            SGVariantType::INT,     GROUP_WAYPOINTS,         QObject::tr("Size of Waypoint's Marker:"),       WidgetType::SPINBOX_INT,  &scale_wp_marker_size,       NULL,                       NULL, NULL },
	{ PARAM_WPSYMS,                  NULL, "wpsyms",            SGVariantType::BOOLEAN, GROUP_WAYPOINTS,         QObject::tr("Draw Waypoint Symbols:"),           WidgetType::CHECKBUTTON,  NULL,                        sg_variant_true,            NULL, NULL },
	{ PARAM_WP_SORT_ORDER,           NULL, "wpsortorder",       SGVariantType::INT,     GROUP_WAYPOINTS,         QObject::tr("Waypoint Sort Order:"),             WidgetType::COMBOBOX,     &params_sort_order,          sort_order_default,         NULL, NULL },

	{ PARAM_WP_IMAGE_DRAW,           NULL, "drawimages",        SGVariantType::BOOLEAN, GROUP_IMAGES,            QObject::tr("Draw Waypoint Images"),             WidgetType::CHECKBUTTON,  NULL,                        sg_variant_true,            NULL, NULL },
	{ PARAM_WP_IMAGE_SIZE,           NULL, "image_size",        SGVariantType::INT,     GROUP_IMAGES,            QObject::tr("Waypoint Image Size (pixels):"),    WidgetType::HSCALE,       &scale_wp_image_size,        NULL,                       NULL, NULL },
	{ PARAM_WP_IMAGE_ALPHA,          NULL, "image_alpha",       SGVariantType::INT,     GROUP_IMAGES,            QObject::tr("Waypoint Image Alpha:"),            WidgetType::HSCALE,       &scale_wp_image_alpha,       NULL,                       NULL, NULL },
	{ PARAM_WP_IMAGE_CACHE_SIZE,     NULL, "image_cache_size",  SGVariantType::INT,     GROUP_IMAGES,            QObject::tr("Waypoint Images' Memory Cache Size:"),WidgetType::HSCALE,       &scale_wp_image_cache_size,  NULL,                       NULL, NULL },

	{ PARAM_MDDESC,                  NULL, "metadatadesc",      SGVariantType::STRING,  GROUP_METADATA,          QObject::tr("Description"),                      WidgetType::ENTRY,        NULL,                        string_default,             NULL, NULL },
	{ PARAM_MDAUTH,                  NULL, "metadataauthor",    SGVariantType::STRING,  GROUP_METADATA,          QObject::tr("Author"),                           WidgetType::ENTRY,        NULL,                        string_default,             NULL, NULL },
	{ PARAM_MDTIME,                  NULL, "metadatatime",      SGVariantType::STRING,  GROUP_METADATA,          QObject::tr("Creation Time"),                    WidgetType::ENTRY,        NULL,                        string_default,             NULL, NULL },
	{ PARAM_MDKEYS,                  NULL, "metadatakeywords",  SGVariantType::STRING,  GROUP_METADATA,          QObject::tr("Keywords"),                         WidgetType::ENTRY,        NULL,                        string_default,             NULL, NULL },

	{ NUM_PARAMS,                    NULL, NULL,                SGVariantType::EMPTY,   PARAMETER_GROUP_GENERIC, QString(""),                                     WidgetType::NONE,         NULL,                        NULL,                       NULL, NULL }, /* Guard. */
};


/*** TO ADD A PARAM:
 *** 1) Add to trw_layer_param_specs and enumeration
 *** 2) Handle in get_param & set_param (presumably adding on to LayerTRW class)
 ***/

/****** END PARAMETERS ******/




LayerTRWInterface vik_trw_layer_interface;




LayerTRWInterface::LayerTRWInterface()
{
	this->parameters_c = trw_layer_param_specs;
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
static QString diary_program;
#define VIK_SETTINGS_EXTERNAL_DIARY_PROGRAM "external_diary_program"

bool have_geojson_export = false;

bool g_have_astro_program = false;
static QString astro_program;
#define VIK_SETTINGS_EXTERNAL_ASTRO_PROGRAM "external_astro_program"




void SlavGPS::layer_trw_init(void)
{
	if (!ApplicationState::get_string(VIK_SETTINGS_EXTERNAL_DIARY_PROGRAM, diary_program)) {
#ifdef WINDOWS
		//diary_program = "C:\\Program Files\\Rednotebook\\rednotebook.exe";
		diary_program = "C:/Progra~1/Rednotebook/rednotebook.exe";
#else
		diary_program = "rednotebook";
#endif
	} else {
		/* User specified so assume it works. */
		g_have_diary_program = true;
	}

	if (g_find_program_in_path(diary_program.toUtf8().constData())) {
		char *mystdout = NULL;
		char *mystderr = NULL;
		/* Needs RedNotebook 1.7.3+ for support of opening on a specified date. */
		char *cmd = g_strconcat(diary_program.toUtf8().constData(), " --version", NULL); // "rednotebook --version"
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
	if (!ApplicationState::get_string(VIK_SETTINGS_EXTERNAL_ASTRO_PROGRAM, astro_program)) {
#ifdef WINDOWS
		//astro_program = "C:\\Program Files\\Stellarium\\stellarium.exe";
		astro_program = "C:/Progra~1/Stellarium/stellarium.exe";
#else
		astro_program = "stellarium";
#endif
	} else {
		/* User specified so assume it works. */
		g_have_astro_program = true;
	}
	if (g_find_program_in_path(astro_program.toUtf8().constData())) {
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
	Track * trk = this->tracks->find_track_by_date(date_str);
	if (trk && select) {
		struct LatLon maxmin[2] = { {0,0}, {0,0} };
		trk->find_maxmin(maxmin);
		this->zoom_to_show_latlons(viewport, maxmin);
		this->tree_view->select_and_expose(trk->index);
		this->emit_layer_changed();
	}
	return (bool) trk;
}


/**
 * Find a waypoint by date.
 */
bool LayerTRW::find_waypoint_by_date(char const * date_str, Viewport * viewport, bool select)
{
	Waypoint * wp = this->waypoints->find_waypoint_by_date(date_str);
	if (wp && select) {
		viewport->set_center_coord(wp->coord, true);
		this->tree_view->select_and_expose(wp->index);
		this->emit_layer_changed();
	}
	return (bool) wp;
}




void LayerTRW::delete_sublayer(TreeItem * sublayer)
{
	if (!sublayer) {
		qDebug() << "WW: Layer TRW: 'delete sublayer' received NULL sublayer";
		return;
	}

	/* true: confirm delete request. */
	this->delete_sublayer_common(sublayer, true);
}




void LayerTRW::cut_sublayer(TreeItem * sublayer)
{
	if (!sublayer) {
		qDebug() << "WW: Layer TRW: 'cut sublayer' received NULL sublayer";
		return;
	}

	this->copy_sublayer_common(sublayer);
	/* true: confirm delete request. */
	this->cut_sublayer_common(sublayer, true);
}




void LayerTRW::copy_sublayer_common(TreeItem * item)
{
	uint8_t * data_ = NULL;
	unsigned int len;

	this->copy_sublayer(item, &data_, &len);

	if (data_) {
		Clipboard::copy(ClipboardDataType::SUBLAYER, LayerType::TRW, item->type_id, len, item->name, data_);
	}
}




void LayerTRW::cut_sublayer_common(TreeItem * item, bool confirm)
{
	this->copy_sublayer_common(item);
	this->delete_sublayer_common(item, confirm);
}




void LayerTRW::copy_sublayer(TreeItem * item, uint8_t ** data, unsigned int * len)
{
	if (!item) {
		qDebug() << "WW: Layer TRW: 'copy sublayer' received NULL sublayer";
		*data = NULL;
		return;
	}

	uint8_t *id;
	size_t il;

	GByteArray *ba = g_byte_array_new();

	item->marshall(&id, &il);

	g_byte_array_append(ba, id, il);

	std::free(id);

	*len = ba->len;
	*data = ba->data;
}




bool LayerTRW::paste_sublayer(TreeItem * item, uint8_t * data, size_t data_len)
{
	if (!item) {
		qDebug() << "WW: Layer TRW: 'paste sublayer' received NULL item";
		return false;
	}

	if (!data) {
		return false;
	}

	if (item->type_id == "sg.trw.waypoint") {
		Waypoint * wp = Waypoint::unmarshall(data, data_len);
		/* When copying - we'll create a new name based on the original. */
		const QString uniq_name = this->new_unique_element_name("sg.trw.waypoint", wp->name);
		wp->set_name(uniq_name);

		this->add_waypoint(wp);

		wp->convert(this->coord_mode);
		this->waypoints->calculate_bounds();

		/* Consider if redraw necessary for the new item. */
		if (this->visible && this->waypoints->visible && wp->visible) {
			this->emit_layer_changed();
		}
		return true;
	}
	if (item->type_id == "sg.trw.track") {
		Track * trk = Track::unmarshall(data, data_len);

		/* When copying - we'll create a new name based on the original. */
		const QString uniq_name = this->new_unique_element_name("sg.trw.track", trk->name);
		trk->set_name(uniq_name);

		this->add_track(trk);

		trk->convert(this->coord_mode);

		/* Consider if redraw necessary for the new item. */
		if (this->visible && this->tracks->visible && trk->visible) {
			this->emit_layer_changed();
		}
		return true;
	}
	if (item->type_id == "sg.trw.route") {
		Track * trk = Track::unmarshall(data, data_len);
		/* When copying - we'll create a new name based on the original. */
		const QString uniq_name = this->new_unique_element_name("sg.trw.route", trk->name);
		trk->set_name(uniq_name);

		this->add_route(trk);
		trk->convert(this->coord_mode);

		/* Consider if redraw necessary for the new item. */
		if (this->visible && this->routes->visible && trk->visible) {
			this->emit_layer_changed();
		}
		return true;
	}
	return false;
}




void LayerTRW::wp_image_cache_flush()
{
	for (auto iter = this->wp_image_cache.begin(); iter != this->wp_image_cache.end(); iter++) {
		delete *iter;
	}
}




bool LayerTRW::set_param_value(uint16_t id, const SGVariant & data, bool is_file_operation)
{
	switch (id) {
	case PARAM_TRACKS_VISIBLE:
		this->tracks->visible = data.val_bool;
		break;
	case PARAM_WAYPOINTS_VISIBLE:
		this->waypoints->visible = data.val_bool;
		break;
	case PARAM_ROUTES_VISIBLE:
		this->routes->visible = data.val_bool;
		break;
	case PARAM_DRAW_TRACK_LABELS:
		this->track_draw_labels = data.val_bool;
		break;
	case PARAM_TRACK_LABEL_FONT_SIZE:
		if (data.val_int < FS_NUM_SIZES) {
			this->trk_label_font_size = (font_size_t) data.val_int;
		}
		break;
	case PARAM_TRACK_DRAWING_MODE:
		this->track_drawing_mode = data.val_int;
		break;
	case PARAM_TRACK_COLOR_COMMON:
		this->track_color_common = data.val_color;
		this->new_track_pens();
		break;
	case PARAM_DRAW_TRACKPOINTS:
		this->draw_trackpoints = data.val_bool;
		break;
	case PARAM_TRACKPOINT_SIZE:
		if (data.val_int >= scale_trackpoint_size.min && data.val_int <= scale_trackpoint_size.max) {
			this->trackpoint_size = data.val_int;
		}
		break;
	case PARAM_DE:
		this->drawelevation = data.val_bool;
		break;
	case PARAM_DRAW_TRACK_STOPS:
		this->draw_track_stops = data.val_bool;
		break;
	case PARAM_DRAW_TRACK_LINES:
		this->draw_track_lines = data.val_bool;
		break;
	case PARAM_DD:
		this->drawdirections = data.val_bool;
		break;
	case PARAM_TRACK_DIRECTION_SIZE:
		if (data.val_int >= scale_track_direction_size.min && data.val_int <= scale_track_direction_size.max) {
			this->drawdirections_size = data.val_int;
		}
		break;
	case PARAM_TRACK_MIN_STOP_LENGTH:
		if (data.val_int >= scale_track_min_stop_length.min && data.val_int <= scale_track_min_stop_length.max) {
			this->stop_length = data.val_int;
		}
		break;
	case PARAM_TRACK_ELEVATION_FACTOR:
		if (data.val_int >= scale_track_elevation_factor.min && data.val_int <= scale_track_elevation_factor.max) {
			this->elevation_factor = data.val_int;
		}
		break;
	case PARAM_TRACK_THICKNESS:
		if (data.val_int >= scale_track_thickness.min && data.val_int <= scale_track_thickness.max) {
			if (data.val_int != this->track_thickness) {
				this->track_thickness = data.val_int;
				this->new_track_pens();
			}
		}
		break;
	case PARAM_TRACK_BG_THICKNESS:
		if (data.val_int >= scale_track_bg_thickness.min && data.val_int <= scale_track_bg_thickness.max) {
			if (data.val_int != this->track_bg_thickness) {
				this->track_bg_thickness = data.val_int;
				this->new_track_pens();
			}
		}
		break;

	case PARAM_TRK_BG_COLOR:
		this->track_bg_color = data.val_color;
		this->track_bg_pen.setColor(this->track_bg_color);
		break;

	case PARAM_TRACK_DRAW_SPEED_FACTOR:
		if (data.val_double >= scale_track_draw_speed_factor.min && data.val_double <= scale_track_draw_speed_factor.max) {
			this->track_draw_speed_factor = data.val_double;
		}
		break;
	case PARAM_TRACK_SORT_ORDER:
		if (data.val_int < VL_SO_LAST) {
			this->track_sort_order = (sort_order_t) data.val_int;
		}
		break;
	case PARAM_DLA:
		this->drawlabels = data.val_bool;
		break;
	case PARAM_WP_IMAGE_DRAW:
		this->wp_image_draw = data.val_bool;
		break;
	case PARAM_WP_IMAGE_SIZE:
		if (data.val_int >= scale_wp_image_size.min && data.val_int <= scale_wp_image_size.max) {
			if (data.val_int != this->wp_image_size) {
				this->wp_image_size = data.val_int;
				this->wp_image_cache_flush();
			}
		}
		break;
	case PARAM_WP_IMAGE_ALPHA:
		if (data.val_int >= scale_wp_image_alpha.min && data.val_int <= scale_wp_image_alpha.max) {
			if (data.val_int != this->wp_image_alpha) {
				this->wp_image_alpha = data.val_int;
				this->wp_image_cache_flush();
			}
		}
		break;

	case PARAM_WP_IMAGE_CACHE_SIZE:
		if (data.val_int >= scale_wp_image_cache_size.min && data.val_int <= scale_wp_image_cache_size.max) {
			this->wp_image_cache_size = data.val_int;
			while (this->wp_image_cache.size() > this->wp_image_cache_size) { /* If shrinking cache_size, free pixbuf ASAP. */
				this->wp_image_cache.pop_front(); /* Calling .pop_front() removes oldest element and calls its destructor. */
			}
		}
		break;

	case PARAM_WP_MARKER_COLOR:
		this->wp_marker_color = data.val_color;
		this->wp_marker_pen.setColor(this->wp_marker_color);
		break;

	case PARAM_WP_LABEL_FG_COLOR:
		this->wp_label_fg_color = data.val_color;
		this->wp_label_fg_pen.setColor(this->wp_label_fg_color);
		break;

	case PARAM_WP_LABEL_BG_COLOR:
		this->wp_label_bg_color = data.val_color;
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
		if (data.val_int < SYMBOL_NUM_SYMBOLS) {
			this->wp_marker_type = data.val_int;
		}
		break;
	case PARAM_WP_MARKER_SIZE:
		if (data.val_int >= scale_wp_marker_size.min && data.val_int <= scale_wp_marker_size.max) {
			this->wp_marker_size = data.val_int;
		}
		break;
	case PARAM_WPSYMS:
		this->wp_draw_symbols = data.val_bool;
		break;
	case PARAM_WP_LABEL_FONT_SIZE:
		if (data.val_int < FS_NUM_SIZES) {
			this->wp_label_font_size = (font_size_t) data.val_int;
		}
		break;
	case PARAM_WP_SORT_ORDER:
		if (data.val_int < VL_SO_LAST) {
			this->wp_sort_order = (sort_order_t) data.val_int;
		}
		break;
		// Metadata
	case PARAM_MDDESC:
		if (!data.val_string.isEmpty() && this->metadata) {
			this->metadata->set_description(data.val_string);
		}
		break;
	case PARAM_MDAUTH:
		if (!data.val_string.isEmpty() && this->metadata) {
			this->metadata->set_author(data.val_string);
		}
		break;
	case PARAM_MDTIME:
		if (!data.val_string.isEmpty() && this->metadata) {
			this->metadata->set_timestamp(data.val_string);
		}
		break;
	case PARAM_MDKEYS:
		if (!data.val_string.isEmpty() && this->metadata) {
			this->metadata->set_keywords(data.val_string);
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
	case PARAM_TRACKS_VISIBLE:          rv = SGVariant(this->tracks->visible);               break;
	case PARAM_WAYPOINTS_VISIBLE:       rv = SGVariant(this->waypoints->visible);            break;
	case PARAM_ROUTES_VISIBLE:          rv = SGVariant(this->routes->visible);               break;
	case PARAM_DRAW_TRACK_LABELS:       rv = SGVariant(this->track_draw_labels);             break;
	case PARAM_TRACK_LABEL_FONT_SIZE:   rv = SGVariant((int32_t) this->trk_label_font_size); break;
	case PARAM_TRACK_DRAWING_MODE:      rv = SGVariant((int32_t) this->track_drawing_mode);  break;
	case PARAM_TRACK_COLOR_COMMON:      rv = SGVariant(this->track_color_common);            break;
	case PARAM_DRAW_TRACKPOINTS:        rv = SGVariant(this->draw_trackpoints);              break;
	case PARAM_TRACKPOINT_SIZE:         rv = SGVariant((int32_t) this->trackpoint_size);     break;
	case PARAM_DE:                      rv = SGVariant(this->drawelevation);                 break;
	case PARAM_TRACK_ELEVATION_FACTOR:  rv = SGVariant((int32_t) this->elevation_factor);    break;
	case PARAM_DRAW_TRACK_STOPS:        rv = SGVariant(this->draw_track_stops);              break;
	case PARAM_TRACK_MIN_STOP_LENGTH:   rv = SGVariant((int32_t) this->stop_length);         break;
	case PARAM_DRAW_TRACK_LINES:        rv = SGVariant(this->draw_track_lines);              break;
	case PARAM_DD:                      rv = SGVariant(this->drawdirections);                break;
	case PARAM_TRACK_DIRECTION_SIZE:    rv = SGVariant((int32_t) this->drawdirections_size); break;
	case PARAM_TRACK_THICKNESS:         rv = SGVariant((int32_t) this->track_thickness);     break;
	case PARAM_TRACK_BG_THICKNESS:      rv = SGVariant((int32_t) this->track_bg_thickness);  break;
	case PARAM_DLA:                     rv = SGVariant(this->drawlabels);                    break;
	case PARAM_TRK_BG_COLOR:            rv = SGVariant(this->track_bg_color);                break;
	case PARAM_TRACK_DRAW_SPEED_FACTOR: rv = SGVariant(this->track_draw_speed_factor);       break;
	case PARAM_TRACK_SORT_ORDER:        rv = SGVariant((int32_t) this->track_sort_order);    break;

	case PARAM_WP_IMAGE_DRAW:           rv = SGVariant(this->wp_image_draw);                 break;
	case PARAM_WP_IMAGE_SIZE:           rv = SGVariant((int32_t) this->wp_image_size);       break;
	case PARAM_WP_IMAGE_ALPHA:          rv = SGVariant((int32_t) this->wp_image_alpha);      break;
	case PARAM_WP_IMAGE_CACHE_SIZE:     rv = SGVariant((int32_t) this->wp_image_cache_size); break;

	case PARAM_WP_MARKER_COLOR:         rv = SGVariant(this->wp_marker_color);               break;
	case PARAM_WP_LABEL_FG_COLOR:       rv = SGVariant(this->wp_label_fg_color);             break;
	case PARAM_WP_LABEL_BG_COLOR:       rv = SGVariant(this->wp_label_bg_color);             break;
	case PARAM_WPBA:                    rv = SGVariant(this->wpbgand);                       break;
	case PARAM_WP_MARKER_TYPE:          rv = SGVariant((int32_t) this->wp_marker_type);      break;
	case PARAM_WP_MARKER_SIZE:          rv = SGVariant((int32_t) this->wp_marker_size);      break;
	case PARAM_WPSYMS:                  rv = SGVariant(this->wp_draw_symbols);               break;
	case PARAM_WP_LABEL_FONT_SIZE:      rv = SGVariant((int32_t) this->wp_label_font_size);  break;
	case PARAM_WP_SORT_ORDER:           rv = SGVariant((int32_t) this->wp_sort_order);       break;

	/* Metadata. */
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
	default:
		break;
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
	case PARAM_WP_IMAGE_DRAW: {
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




void LayerTRW::marshall(uint8_t ** data, size_t * data_len)
{
	uint8_t *pd;
	size_t pl;

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


	for (auto i = this->waypoints->items.begin(); i != this->waypoints->items.end(); i++) {
		i->second->marshall(&sl_data, &sl_len);
		tlm_append(sl_data, sl_len, SublayerType::WAYPOINT);
		std::free(sl_data);
	}


	for (auto i = this->tracks->items.begin(); i != this->tracks->items.end(); i++) {
		i->second->marshall(&sl_data, &sl_len);
		tlm_append(sl_data, sl_len, SublayerType::TRACK);
		std::free(sl_data);
	}


	for (auto i = this->routes->items.begin(); i != this->routes->items.end(); i++) {
		i->second->marshall(&sl_data, &sl_len);
		tlm_append(sl_data, sl_len, SublayerType::ROUTE);
		std::free(sl_data);
	}


#undef tlm_append

	*data = ba->data;
	*data_len = ba->len;
}




Layer * LayerTRWInterface::unmarshall(uint8_t * data, size_t data_len, Viewport * viewport)
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

	while (*data && consumed_length < data_len) {
		// Normally four extra bytes at the end of the datastream
		//  (since it's a GByteArray and that's where it's length is stored)
		//  So only attempt read when there's an actual block of sublayer data
		if (consumed_length + tlm_size < data_len) {

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
	//fprintf(stderr, "DEBUG: consumed_length %d vs len %d\n", consumed_length, data_len);

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




void LayerTRW::draw_tree_item(Viewport * viewport, bool hl_is_allowed, bool hl_is_required)
{

#ifdef K
	/* TODO: re-implement/re-enable this feature. */

	/* If this layer is to be highlighted - then don't draw now - as it will be drawn later on in the specific highlight draw stage
	   This may seem slightly inefficient to test each time for every layer
	   but for a layer with *lots* of tracks & waypoints this can save some effort by not drawing the items twice. */
	if (viewport->get_highlight_usage()
	    && g_tree->selected_tree_item && *g_tree->selected_tree_item == this) { /* TODO: use UID to compare tree items. */

		return;
	}
#endif




	/* kamilFIXME: enabling this code and then compiling it with -O0 results in crash when selecting trackpoint in viewport. */
#if 1
	/* Check the layer for visibility (including all the parents' visibilities). */
	if (!this->tree_view->get_tree_item_visibility_with_parents(this->index)) {
		return;
	}
#endif

	const bool allowed = hl_is_allowed;
	const bool required = allowed
		&& (hl_is_required /* Parent code requires us to do highlight. */
		    || (g_tree->selected_tree_item && g_tree->selected_tree_item == this)); /* This item discovers that it is selected and decides to be highlighted. */ /* TODO: use UID to compare tree items. */

	if (this->tracks->visible) {
		qDebug() << "II: Layer TRW: calling function to draw tracks, highlight:" << allowed << required;
		this->tracks->draw_tree_item(viewport, allowed, required);
	}

	if (this->routes->visible) {
		qDebug() << "II: Layer TRW: calling function to draw routes, highlight:" << allowed << required;
		this->routes->draw_tree_item(viewport, allowed, required);
	}

	if (this->waypoints->visible) { /* TODO: fix condition. */
		qDebug() << "II: Layer TRW: calling function to draw waypoints, highlight:" << allowed << required;
		this->waypoints->draw_tree_item(viewport, allowed, required);
	}

	return;

}




void LayerTRW::draw(Viewport * viewport)
{
	this->draw_tree_item(viewport, false, false);
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




void LayerTRW::add_children_to_tree(void)
{
	qDebug() << "DD: Layer TRW: Add Children to Tree";

	if (this->tracks->items.size() > 0) {
		qDebug() << "DD: Layer TRW: Add Children to Tree: Tracks";
		/* TODO: verify that the node is not connected to tree yet, before calling this method. */
		this->tree_view->add_tree_item(this->index, this->tracks, tr("Tracks"));
		this->tracks->add_children_to_tree();
	}

	if (this->routes->items.size() > 0) {
		qDebug() << "DD: Layer TRW: Add Children to Tree: Routes";
		/* TODO: verify that the node is not connected to tree yet, before calling this method. */
		this->tree_view->add_tree_item(this->index, this->routes, tr("Routes"));
		this->routes->add_children_to_tree();
	}

	if (this->waypoints->items.size() > 0) {
		qDebug() << "DD: Layer TRW: Add Children to Tree: Waypoints";
		/* TODO: verify that the node is not connected to tree yet, before calling this method. */
		this->tree_view->add_tree_item(this->index, this->waypoints, tr("Waypoints"));
		this->waypoints->add_children_to_tree();
	}

	this->verify_thumbnails();

	this->sort_all();
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
QString LayerTRW::get_tooltip()
{
	char tbuf1[64] = { 0 };
	char tbuf2[64] = { 0 };
	QString tracks_duration;
	char tbuf4[10] = { 0 };

	static char tmp_buf[128] = { 0 };

	/* For compact date format I'm using '%x'     [The preferred date representation for the current locale without the time.] */

	if (!this->tracks->items.empty()) {
		tooltip_tracks tt = { 0.0, 0, 0, 0 };
		trw_layer_tracks_tooltip(this->tracks->items, &tt);

		QDateTime date_start;
		date_start.setTime_t(tt.start_time);

		QDateTime date_end;
		date_end.setTime_t(tt.end_time);

		if (date_start != date_end) { /* TODO: should we compare dates/times, or only dates? */
			/* Dates differ so print range on separate line. */
			tracks_duration = QObject::tr("%1 to %2\n").arg(date_start.toString(Qt::SystemLocaleLongDate)).arg(date_end.toString(Qt::SystemLocaleLongDate));
		} else {
			/* Same date so just show it and keep rest of text on the same line - provided it's a valid time! */
			if (tt.start_time != 0) {
				tracks_duration = date_start.toString(Qt::SystemLocaleLongDate);
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
				 tracks_duration.toUtf8().constData(), len_in_units, tbuf4, tbuf1);
		}

		tbuf1[0] = '\0';
		double rlength = 0.0;
		trw_layer_routes_tooltip(this->routes->items, &rlength);
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
			 this->tracks->items.size(), this->waypoints->items.size(), this->routes->items.size(), tbuf2, tbuf1);

	}
	return QString(tmp_buf);
}




#define VIK_SETTINGS_TRKPT_SELECTED_STATUSBAR_FORMAT "trkpt_selected_statusbar_format"

/**
   Function to show track point information on the statusbar.
   Items displayed is controlled by the settings format code.
*/
void LayerTRW::set_statusbar_msg_info_tp(TrackPoints::iterator & tp_iter, Track * track)
{
	Trackpoint * tp_prev = NULL;

	QString statusbar_format_code;
	if (!ApplicationState::get_string(VIK_SETTINGS_TRKPT_SELECTED_STATUSBAR_FORMAT, statusbar_format_code)) {
		/* Otherwise use default. */
		statusbar_format_code = "KEATDN";
	} else {
		/* Format code may want to show speed - so may need previous trkpt to work it out. */
		auto iter = std::prev(tp_iter);
		tp_prev = iter == track->end() ? NULL : *iter;
	}

	Trackpoint * tp = tp_iter == track->end() ? NULL : *tp_iter;
	const QString msg = vu_trackpoint_formatted_message(statusbar_format_code.toUtf8().constData(), tp, tp_prev, track, NAN);
	this->get_window()->get_statusbar()->set_message(StatusBarField::INFO, QString(msg));
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




void LayerTRW::reset_internal_selections(void)
{
	this->reset_edited_wp();
	this->cancel_current_tp(false);
}




Tracks & LayerTRW::get_track_items()
{
	return this->tracks->items;
}




Tracks & LayerTRW::get_route_items()
{
	return this->routes->items;
}




Waypoints & LayerTRW::get_waypoint_items()
{
	return this->waypoints->items;
}




LayerTRWTracks & LayerTRW::get_tracks_node(void)
{
	return *this->tracks;
}




LayerTRWTracks & LayerTRW::get_routes_node(void)
{
	return *this->routes;
}




LayerTRWWaypoints & LayerTRW::get_waypoints_node(void)
{
	return *this->waypoints;
}




bool LayerTRW::is_empty()
{
	return ! (this->tracks->items.size() || this->routes->items.size() || this->waypoints->items.size());
}




bool LayerTRW::get_tracks_visibility()
{
	return this->tracks->visible;
}




bool LayerTRW::get_routes_visibility()
{
	return this->routes->visible;
}




bool LayerTRW::get_waypoints_visibility()
{
	return this->waypoints->visible;
}




void LayerTRW::find_maxmin(struct LatLon maxmin[2])
{
	/* Continually reuse maxmin to find the latest maximum and minimum values.
	   First set to waypoints bounds. */

	this->waypoints->find_maxmin(maxmin);
	this->tracks->find_maxmin(maxmin);
	this->routes->find_maxmin(maxmin);
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
		Viewport * viewport = g_tree->tree_get_main_viewport();
		this->goto_coord(viewport, coord);
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
	if (this->auto_set_view(g_tree->tree_get_main_viewport())) {
		g_tree->tree_get_items_tree()->emit_update_window_cb();
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




void LayerTRW::find_waypoint_dialog_cb(void)
{
	QInputDialog dialog(this->get_window());
	dialog.setWindowTitle(tr("Find"));
	dialog.setLabelText(tr("Waypoint Name:"));
	dialog.setInputMode(QInputDialog::TextInput);


	while (dialog.exec() == QDialog::Accepted) {
		QString name_ = dialog.textValue();

		/* Find *first* wp with the given name. */
		Waypoint * wp = this->waypoints->find_waypoint_by_name(name_);

		if (!wp) {
			Dialog::error(tr("Waypoint not found in this layer."), this->get_window());
		} else {
			g_tree->tree_get_main_viewport()->set_center_coord(wp->coord, true);
			g_tree->emit_update_window();
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

	const QString returned_name = waypoint_properties_dialog(parent_window, default_name, wp, this->coord_mode, true, &updated);

	if (returned_name.size()) {
		wp->visible = true;
		wp->set_name(returned_name);
		this->add_waypoint(wp);
		return true;
	} else {
		qDebug() << "II: LayerTRW:" << __FUNCTION__ << "empty name of new Waypoint, rejecting";
		delete wp;
		return false;
	}
}




void LayerTRW::acquire_from_wikipedia_waypoints_viewport_cb(void) /* Slot. */
{
	struct LatLon maxmin[2] = { {0.0,0.0}, {0.0,0.0} };
	Viewport * viewport = g_tree->tree_get_main_viewport();

	/* Note the order is max part first then min part - thus reverse order of use in min_max function: */
	viewport->get_min_max_lat_lon(&maxmin[1].lat, &maxmin[0].lat, &maxmin[1].lon, &maxmin[0].lon);

	a_geonames_wikipedia_box(this->get_window(), this, maxmin);
	this->waypoints->calculate_bounds();
	g_tree->emit_update_window();
}




void LayerTRW::acquire_from_wikipedia_waypoints_layer_cb(void) /* Slot. */
{
	struct LatLon maxmin[2] = { {0.0,0.0}, {0.0,0.0} };

	this->find_maxmin(maxmin);

	a_geonames_wikipedia_box(this->get_window(), this, maxmin);
	this->waypoints->calculate_bounds();
	g_tree->emit_update_window();
}




#ifdef VIK_CONFIG_GEOTAG
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
	LayersPanel * panel = g_tree->tree_get_items_tree();
	Viewport * viewport =  g_tree->tree_get_main_viewport();

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




/**
 * If data->tree is defined that this will upload just that track.
 */
void LayerTRW::upload_to_gps_cb()
{
	this->upload_to_gps(NULL);
}




/**
 * If @param sublayer is defined then this function will upload just that sublayer.
 */
void LayerTRW::upload_to_gps(TreeItem * sublayer)
{
	LayersPanel * panel = g_tree->tree_get_items_tree();
	Track * trk = NULL;
	GPSTransferType xfer_type = GPSTransferType::TRK; /* "sg.trw.tracks" = 0 so hard to test different from NULL! */
	bool xfer_all = false;

	if (sublayer) {
		/* Upload only specified sublayer, not whole layer. */
		xfer_all = false;
		if (sublayer->type_id == "sg.trw.track") {
			trk = (Track *) sublayer;
			xfer_type = GPSTransferType::TRK;

		} else if (sublayer->type_id == "sg.trw.route") {
			trk = (Track *) sublayer;
			xfer_type = GPSTransferType::RTE;

		} else if (sublayer->type_id == "sg.trw.waypoints") {
			xfer_type = GPSTransferType::WPT;

		} else if (sublayer->type_id == "sg.trw.routes") {
			xfer_type = GPSTransferType::RTE;
		}
	} else {
		/* No specific sublayer, so upload whole layer. */
		xfer_all = true;
	}

	if (trk && !trk->visible) {
		Dialog::error(tr("Can not upload invisible track."), this->get_window());
		return;
	}

#ifdef K
	GtkWidget * dialog = gtk_dialog_new_with_buttons(_("GPS Upload"),
							this->get_window(),
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

	/* Get info from reused datasource dialog widgets. */
	QString protocol = datasource_gps_get_protocol(dgs);
	QString port = datasource_gps_get_descriptor(dgs);
	/* Don't free the above strings as they're references to values held elsewhere. */
	bool do_tracks = datasource_gps_get_do_tracks(dgs);
	bool do_routes = datasource_gps_get_do_routes(dgs);
	bool do_waypoints = datasource_gps_get_do_waypoints(dgs);
	bool turn_off = datasource_gps_get_off(dgs);

	gtk_widget_destroy(dialog);

	/* When called from the viewport - work the corresponding layerspanel: */
	if (!panel) {
		panel = g_tree->tree_get_items_tree();
	}

	/* Apply settings to transfer to the GPS device. */
	vik_gps_comm(this,
		     trk,
		     GPSDirection::UP,
		     protocol,
		     port,
		     false,
		     g_tree->tree_get_main_viewport(),
		     panel,
		     do_tracks,
		     do_routes,
		     do_waypoints,
		     turn_off);
#endif
}




void LayerTRW::new_waypoint_cb(void) /* Slot. */
{
	/* TODO longone: okay, if layer above (aggregate) is invisible but this->visible is true, this redraws for no reason.
	   Instead return true if you want to update. */
	if (this->new_waypoint(this->get_window(), g_tree->tree_get_main_viewport()->get_center())) {
		this->waypoints->calculate_bounds();
		if (this->visible) {
			g_tree->emit_update_window();
		}
	}
}




Track * LayerTRW::new_track_create_common(const QString & new_name)
{
	qDebug() << "II: Layer TRW: new track create common, track name" << new_name;

	Track * track = new Track(false);
	track->set_defaults();
	track->visible = true; /* TODO: verify if this is necessary. Tracks are visible by default. */

	if (this->track_drawing_mode == DRAWMODE_ALL_SAME_COLOR) {
		/* Create track with the preferred color from the layer properties. */
		track->color = this->track_color_common;
	} else {
		track->color = QColor("#aa22dd"); //QColor("#000000");
	}

	track->has_color = true;
	track->set_name(new_name);

	this->set_edited_track(track);

	this->add_track(track);

	return track;
}




void LayerTRW::new_track_cb() /* Slot. */
{
	if (!this->get_edited_track()) {

		/* This QAction for this slot shouldn't even be active when a track/route is already being created. */

		const QString uniq_name = this->new_unique_element_name("sg.trw.track", tr("Track")) ;
		this->new_track_create_common(uniq_name);

		this->get_window()->activate_tool(LAYER_TRW_TOOL_CREATE_TRACK);
	}
}




Track * LayerTRW::new_route_create_common(const QString & new_name)
{
	Track * route = new Track(true);
	route->set_defaults();
	route->visible = true; /* TODO: verify if this is necessary. Routes are visible by default. */

	/* By default make all routes red. */
	route->has_color = true;
	route->color = QColor("red");

	route->set_name(new_name);

	this->set_edited_track(route);

	this->add_route(route);

	return route;
}




void LayerTRW::new_route_cb(void) /* Slot. */
{
	if (!this->get_edited_track()) {

		/* This QAction for this slot shouldn't even be active when a track/route is already being created. */

		const QString uniq_name = this->new_unique_element_name("sg.trw.route", tr("Route")) ;
		this->new_route_create_common(uniq_name);

		this->get_window()->activate_tool(LAYER_TRW_TOOL_CREATE_ROUTE);
	}
}





void LayerTRW::finish_track_cb(void) /* Slot. */
{
	this->reset_track_creation_in_progress();
	this->reset_edited_track();
	this->route_finder_started = false;
	this->emit_layer_changed();
}




void LayerTRW::finish_route_cb(void) /* Slot. */
{
	this->reset_route_creation_in_progress();
	this->reset_edited_track();
	this->route_finder_started = false;
	this->emit_layer_changed();
}




void LayerTRW::upload_to_osm_traces_cb(void) /* Slot. */
{
	osm_traces_upload_viktrwlayer(this, NULL);
}




void LayerTRW::add_waypoint(Waypoint * wp)
{
	if (!this->tree_view) {
		qDebug() << "EE: Layer TRW: add waypoint: layer not connected to tree";
		return;
	}

	if (!this->waypoints->tree_view) {
		/* "Waypoints" node has not been shown in tree view.
		   That's because we start showing the node only after first waypoint is added. */

		if (0 != this->waypoints->items.size()) {
			qDebug() << "EE: Layer TRW: add waypoint: 'Waypoints' node not connected, but has non-zero items already.";
			return;
		}

		this->tree_view->add_tree_item(this->index, this->waypoints, tr("Waypoints"));
	}

	/* Now we can proceed with adding a waypoint to Waypoints node. */

	wp->owning_layer = this;

	this->waypoints->items.insert({{ wp->uid, wp }});

	time_t timestamp = 0;
	if (wp->has_timestamp) {
		timestamp = wp->timestamp;
	}

	this->tree_view->add_tree_item(this->waypoints->index, wp, wp->name);

	this->tree_view->set_tree_item_timestamp(wp->index, timestamp);

	/* Sort now as post_read is not called on a waypoint connected to tree. */
	this->tree_view->sort_children(this->waypoints->get_index(), this->wp_sort_order);

	this->highest_wp_number_add_wp(wp->name);
}




void LayerTRW::add_track(Track * trk)
{
	if (!this->tree_view) {
		qDebug() << "EE: Layer TRW: add track: layer not connected to tree";
		return;
	}

	if (!this->tracks->tree_view) {
		/* "Tracks" node has not been shown in tree view.
		   That's because we start showing the node only after first track is added. */

		if (0 != this->tracks->items.size()) {
			qDebug() << "EE: Layer TRW: add track: 'Tracks' node not connected, but has non-zero items already.";
			return;
		}

		this->tree_view->add_tree_item(this->index, this->tracks, tr("Tracks"));
	}

	/* Now we can proceed with adding a track to Tracks node. */

	trk->owning_layer = this;

	this->tracks->items.insert({{ trk->uid, trk }});

	time_t timestamp = 0;
	Trackpoint * tp = trk->get_tp_first();
	if (tp && tp->has_timestamp) {
		timestamp = tp->timestamp;
	}

	this->tree_view->add_tree_item(this->tracks->index, trk, trk->name);
	this->tree_view->set_tree_item_timestamp(trk->index, timestamp);

	/* Sort now as post_read is not called on a track connected to tree. */
	this->tree_view->sort_children(this->tracks->get_index(), this->track_sort_order);

	this->tracks->update_tree_view(trk);
}




void LayerTRW::add_route(Track * trk)
{
	if (!this->tree_view) {
		qDebug() << "EE: Layer TRW: add route: layer not connected to tree";
		return;
	}

	if (!this->routes->tree_view) {
		/* "Routes" node has not been shown in tree view.
		   That's because we start showing the node only after first route is added. */

		if (0 != this->routes->items.size()) {
			qDebug() << "EE: Layer TRW: add route: 'Routes' node not connected, but has non-zero items already.";
			return;
		}

		this->tree_view->add_tree_item(this->index, this->routes, tr("Routes"));
	}

	/* Now we can proceed with adding a route to Routes node. */

	trk->owning_layer = this;

	this->routes->items.insert({{ trk->uid, trk }});

	this->tree_view->add_tree_item(this->routes->index, trk, trk->name);

	/* Sort now as post_read is not called on a route connected to tree. */
	this->tree_view->sort_children(this->routes->get_index(), this->track_sort_order);

	this->routes->update_tree_view(trk);
}




/* to be called whenever a track has been deleted or may have been changed. */
void LayerTRW::cancel_tps_of_track(Track * trk)
{
	if (this->get_edited_track() == trk) {
		this->cancel_current_tp(false);
	}
}




/**
 * Normally this is done to due the waypoint size preference having changed.
 */
void LayerTRW::reset_waypoints()
{
	for (auto i = this->waypoints->items.begin(); i != this->waypoints->items.end(); i++) {
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
		return this->tracks->new_unique_element_name(old_name);
	} else if (item_type_id == "sg.trw.waypoint") {
		return this->waypoints->new_unique_element_name(old_name);
	} else {
		return this->routes->new_unique_element_name(old_name);
	}
}




void LayerTRW::add_waypoint_from_file(Waypoint * wp, const QString & wp_name)
{
	/* No more uniqueness of name forced when loading from a file.
	   This now makes this function a little redunant as we just flow the parameters through. */
	wp->set_name(wp_name);
	this->add_waypoint(wp);
}




void LayerTRW::add_waypoint_to_data_structure(Waypoint * wp, const QString & wp_name)
{
	/* No more uniqueness of name forced when loading from a file.
	   This now makes this function a little redunant as we just flow the parameters through. */
	wp->set_name(wp_name);

	wp->owning_layer = this;

	this->waypoints->items.insert({{ wp->uid, wp }});

	this->highest_wp_number_add_wp(wp->name);
}




void LayerTRW::add_track_to_data_structure(Track * trk)
{
	trk->owning_layer = this;
	this->tracks->items.insert({{ trk->uid, trk }});
}



void LayerTRW::add_route_to_data_structure(Track * trk)
{
	trk->owning_layer = this;
	this->routes->items.insert({{ trk->uid, trk }});
}




void LayerTRW::add_track_from_file(Track * incoming_track, const QString & incoming_track_name)
{
	Track * curr_track = this->get_edited_track();

	if (this->route_finder_append && curr_track) {
		incoming_track->remove_dup_points(); /* Make "double point" track work to undo. */

		/* Enforce end of current track equal to start of tr. */
		Trackpoint * cur_end = curr_track->get_tp_last();
		Trackpoint * new_start = incoming_track->get_tp_first();
		if (cur_end && new_start) {
			if (cur_end->coord != new_start->coord) {
				curr_track->add_trackpoint(new Trackpoint(*cur_end), false);
			}
		}

		curr_track->steal_and_append_trackpoints(incoming_track);
		incoming_track->free();
		this->route_finder_append = false; /* This means we have added it. */
	} else {
		incoming_track->set_name(incoming_track_name);
		/* No more uniqueness of name forced when loading from a file. */
		if (incoming_track->type_id == "sg.trw.route") {
			this->add_route(incoming_track);
		} else {
			this->add_track(incoming_track);
		}

		if (this->route_finder_check_added_track) {
			incoming_track->remove_dup_points(); /* Make "double point" track work to undo. */
			this->route_finder_added_track = incoming_track;
		}
	}
}




void LayerTRW::add_track_from_file2(Track * incoming_track, const QString & incoming_track_name)
{
	Track * curr_track = this->get_edited_track();

	if (this->route_finder_append && curr_track) {
		incoming_track->remove_dup_points(); /* Make "double point" track work to undo. */

		/* Enforce end of current track equal to start of tr. */
		Trackpoint * cur_end = curr_track->get_tp_last();
		Trackpoint * new_start = incoming_track->get_tp_first();
		if (cur_end && new_start) {
			if (cur_end->coord != new_start->coord) {
				curr_track->add_trackpoint(new Trackpoint(*cur_end), false);
			}
		}

		curr_track->steal_and_append_trackpoints(incoming_track);
		incoming_track->free();
		this->route_finder_append = false; /* This means we have added it. */
	} else {
		incoming_track->set_name(incoming_track_name);
		/* No more uniqueness of name forced when loading from a file. */
		if (incoming_track->type_id == "sg.trw.route") {
			this->add_route_to_data_structure(incoming_track);
		} else {
			this->add_track_to_data_structure(incoming_track);
		}

		if (this->route_finder_check_added_track) {
			incoming_track->remove_dup_points(); /* Make "double point" track work to undo. */
			this->route_finder_added_track = incoming_track;
		}
	}
}




/*
 * Move an item from one TRW layer to another TRW layer.
 */
void LayerTRW::move_item(LayerTRW * trw_dest, sg_uid_t sublayer_uid, const QString & item_type_id)
{
	LayerTRW * trw_src = this;
	/* When an item is moved the name is checked to see if it clashes with an existing name
	   in the destination layer and if so then it is allocated a new name. */

	/* TODO reconsider strategy when moving within layer (if anything...). */
	if (trw_src == trw_dest) {
		return;
	}

	if (item_type_id == "sg.trw.track") {
		Track * trk = this->tracks->items.at(sublayer_uid);
		Track * trk2 = new Track(*trk);

		const QString uniq_name = trw_dest->new_unique_element_name(item_type_id, trk->name);
		trk2->set_name(uniq_name);
		/* kamilFIXME: in C application did we free this unique name anywhere? */

		trw_dest->add_track(trk2);

		this->delete_track(trk);
		/* Reset layer timestamps in case they have now changed. */
		trw_dest->tree_view->set_tree_item_timestamp(trw_dest->index, trw_dest->get_timestamp());
		trw_src->tree_view->set_tree_item_timestamp(trw_src->index, trw_src->get_timestamp());
	}

	if (item_type_id == "sg.trw.route") {
		Track * trk = this->routes->items.at(sublayer_uid);
		Track * trk2 = new Track(*trk);

		const QString uniq_name = trw_dest->new_unique_element_name(item_type_id, trk->name);
		trk2->set_name(uniq_name);

		trw_dest->add_route(trk2);

		this->delete_route(trk);
	}

	if (item_type_id == "sg.trw.waypoint") {
		Waypoint * wp = this->waypoints->items.at(sublayer_uid);
		Waypoint * wp2 = new Waypoint(*wp);

		const QString uniq_name = trw_dest->new_unique_element_name(item_type_id, wp->name);
		wp2->set_name(uniq_name);

		trw_dest->add_waypoint(wp2);

		this->delete_waypoint(wp);

		/* Recalculate bounds even if not renamed as maybe dragged between layers. */
		trw_dest->waypoints->calculate_bounds();
		trw_src->waypoints->calculate_bounds();
		/* Reset layer timestamps in case they have now changed. */
		trw_dest->tree_view->set_tree_item_timestamp(trw_dest->index, trw_dest->get_timestamp());
		trw_src->tree_view->set_tree_item_timestamp(trw_src->index, trw_src->get_timestamp());
	}
}




void LayerTRW::drag_drop_request(Layer * src, TreeIndex & src_item_index, void * GtkTreePath_dest_path)
{
	LayerTRW * trw_dest = this;
	LayerTRW * trw_src = (LayerTRW *) src;

	TreeItem * sublayer = trw_src->tree_view->get_tree_item(src_item_index);


	if (sublayer->name.isEmpty()) {
		std::list<sg_uid_t> items;

		if (sublayer->type_id == "sg.trw.tracks") {
			trw_src->tracks->list_trk_uids(items);
		}
		if (sublayer->type_id == "sg.trw.waypoints") {
			trw_src->waypoints->list_wp_uids(items);
		}
		if (sublayer->type_id == "sg.trw.routes") {
			trw_src->routes->list_trk_uids(items);
		}

		for (auto iter = items.begin(); iter != items.end(); iter++) {
			if (sublayer->type_id == "sg.trw.tracks") {
				trw_src->move_item(trw_dest, *iter, "sg.trw.track");
			} else if (sublayer->type_id == "sg.trw.routes") {
				trw_src->move_item(trw_dest, *iter, "sg.trw.route");
			} else {
				trw_src->move_item(trw_dest, *iter, "sg.trw.waypoint");
			}
		}
	} else {
#ifdef K
		trw_src->move_item(trw_dest, sublayer->name, sublayer->type_id);
#endif
	}
}




bool LayerTRW::delete_track(Track * trk)
{
	/* TODO: why check for trk->name here? */
	if (!trk || trk->name.isEmpty()) {
		return false;
	}

	if (trk == this->get_edited_track()) {
		this->reset_edited_track();
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
	this->tracks->items.erase(trk->uid); /* kamilTODO: should this line be inside of "if (it)"? */

	/* If last sublayer, then remove sublayer container. */
	if (this->tracks->items.size() == 0) {
		this->tree_view->erase(this->tracks->get_index());
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

	if (trk == this->get_edited_track()) {
		this->reset_edited_track();
		this->moving_tp = false;
	}

	bool was_visible = trk->visible;

	if (trk == this->route_finder_added_track) {
		this->route_finder_added_track = NULL;
	}

	/* Could be current_tp, so we have to check. */
	this->cancel_tps_of_track(trk);

	this->tree_view->erase(trk->index);
	this->routes->items.erase(trk->uid); /* kamilTODO: should this line be inside of "if (it)"? */

	/* If last sublayer, then remove sublayer container. */
	if (this->routes->items.size() == 0) {
		this->tree_view->erase(this->routes->get_index());
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

	if (wp == this->get_edited_wp()) {
		this->reset_edited_wp();
		this->moving_wp = false;
	}

	bool was_visible = wp->visible;

	this->tree_view->erase(wp->index);

	this->highest_wp_number_remove_wp(wp->name);

	/* kamilTODO: should this line be inside of "if (it)"? */
	this->waypoints->items.erase(wp->uid); /* Last because this frees the name. */

	/* If last sublayer, then remove sublayer container. */
	if (this->waypoints->items.size() == 0) {
		this->tree_view->erase(this->waypoints->get_index());
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
	Waypoint * wp = this->waypoints->find_waypoint_by_name(wp_name);
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
		Track * trk = this->routes->find_track_by_name(trk_name);
		if (trk) {
			return delete_route(trk);
		}
	} else {
		Track * trk = this->tracks->find_track_by_name(trk_name);
		if (trk) {
			return delete_track(trk);
		}
	}

	return false;
}




void LayerTRW::delete_all_routes()
{
	this->route_finder_added_track = NULL;
	if (this->get_edited_track()) {
		this->cancel_current_tp(false);
		this->reset_edited_track();
	}

	for (auto i = this->routes->items.begin(); i != this->routes->items.end(); i++) {
		tree_view->erase(i->second->index);
	}

	this->routes->items.clear(); /* kamilTODO: call destructors of routes. */

	this->tree_view->erase(this->routes->get_index());

	this->emit_layer_changed();
}




void LayerTRW::delete_all_tracks()
{
	this->route_finder_added_track = NULL;
	if (this->get_edited_track()) {
		this->cancel_current_tp(false);
		this->reset_edited_track();
	}

	for (auto i = this->tracks->items.begin(); i != this->tracks->items.end(); i++) {
		tree_view->erase(i->second->index);
	}

	this->tracks->items.clear(); /* kamilTODO: call destructors of tracks. */

	this->tree_view->erase(this->tracks->get_index());

	this->emit_layer_changed();
}




void LayerTRW::delete_all_waypoints()
{
	this->reset_edited_wp();
	this->moving_wp = false;

	this->highest_wp_number_reset();

	for (auto i = this->waypoints->items.begin(); i != this->waypoints->items.end(); i++) {
		tree_view->erase(i->second->index);
	}

	this->waypoints->items.clear(); /* kamilTODO: does this really call destructors of Waypoints? */

	this->tree_view->erase(this->waypoints->get_index());

	this->emit_layer_changed();
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




void LayerTRW::delete_sublayer_common(TreeItem * item, bool confirm)
{
	bool was_visible = false;

	if (item->type_id == "sg.trw.waypoint") {
		Waypoint * wp = (Waypoint *) item;
		wp->delete_sublayer(confirm);
	} else if (item->type_id == "sg.trw.track" || item->type_id == "sg.trw.route") {
		Track * trk = (Track *) item;
		trk->delete_sublayer(confirm);
	} else {
		qDebug() << "EE: Layer TRW: unknown sublayer type" << item->type_id;
	}
}




void LayerTRW::goto_coord(Viewport * viewport, const Coord & coord)
{
	if (viewport) {
		viewport->set_center_coord(coord, true);
		this->emit_layer_changed();
	}
}




void LayerTRW::extend_track_end_cb(void)
{
	Track * track = this->get_edited_track();
	if (!track) {
		return;
	}

	this->get_window()->activate_tool(track->type_id == "sg.trw.route" ? LAYER_TRW_TOOL_CREATE_ROUTE : LAYER_TRW_TOOL_CREATE_TRACK);

	if (!track->empty()) {
		Viewport * viewport = g_tree->tree_get_main_viewport();
		this->goto_coord(viewport, track->get_tp_last()->coord);
	}
}




/**
 * Extend a track using route finder.
 */
void LayerTRW::extend_track_end_route_finder_cb(void)
{
	Track * track = this->get_edited_track();
	if (!track) {
		return;
	}

	this->get_window()->activate_tool(LAYER_TRW_TOOL_ROUTE_FINDER);

	this->route_finder_started = true;

	if (!track->empty()) {
		Viewport * viewport = g_tree->tree_get_main_viewport();
		this->goto_coord(viewport, track->get_tp_last()->coord);
	}
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
		/* Same sort method as used in the vik_tree_view_*_alphabetize functions. */
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
	Track * track = this->get_edited_track();
	if (!track) {
		qDebug() << "EE: Layer TRW: can't get edited track in track-related function" << __FUNCTION__;
		return;
	}
	if (track->empty()) {
		return;
	}

	const bool is_route = track->type_id == "sg.trw.route";

	LayerTRWTracks * ght_tracks = is_route ? this->routes : this->tracks;

	/* with_timestamps: allow merging with 'similar' time type time tracks
	   i.e. either those times, or those without */
	bool with_timestamps = track->get_tp_first()->has_timestamp;
	std::list<sg_uid_t> other_tracks = ght_tracks->find_tracks_with_timestamp_type(with_timestamps, track);

	if (other_tracks.empty()) {
		if (with_timestamps) {
			Dialog::error(tr("Failed. No other tracks with timestamps in this layer found"), this->get_window());
		} else {
			Dialog::error(tr("Failed. No other tracks without timestamps in this layer found"), this->get_window());
		}
		return;
	}
	other_tracks.reverse();

	/* Convert into list of names for usage with dialog function.
	   TODO: Need to consider how to work best when we can have multiple tracks the same name... */
	std::list<QString> other_tracks_names;
	for (auto iter = other_tracks.begin(); iter != other_tracks.end(); iter++) {
		other_tracks_names.push_back(ght_tracks->items.at(*iter)->name);
	}

	/* Sort alphabetically for user presentation. */
	other_tracks_names.sort();

	const QStringList headers = { is_route ? tr("Select route to merge with") : tr("Select track to merge with") };
	std::list<QString> merge_list = a_dialog_select_from_list(other_tracks_names,
								  true,
								  tr("Merge with..."),
								  headers,
								  this->get_window());

	if (merge_list.empty()) {
		qDebug() << "II: Layer TRW: merge track is empty";
		return;
	}

	for (auto iter = merge_list.begin(); iter != merge_list.end(); iter++) {
		Track * merge_track = NULL;
		if (is_route) {
			merge_track = this->routes->find_track_by_name(*iter);
		} else {
			merge_track = this->tracks->find_track_by_name(*iter);
		}

		if (merge_track) {
			qDebug() << "II: Layer TRW: we have a merge track";
			track->steal_and_append_trackpoints(merge_track);
			if (is_route) {
				this->delete_route(merge_track);
			} else {
				this->delete_track(merge_track);
			}
			track->sort(Trackpoint::compare_timestamps);
		}
	}
	this->emit_layer_changed();
}




/**
 * Join - this allows combining 'tracks' and 'track routes'
 *  i.e. doesn't care about whether tracks have consistent timestamps
 * ATM can only append one track at a time to the currently selected track
 */
void LayerTRW::append_track_cb(void)
{
	Track * track = this->get_edited_track();
	if (!track) {
		qDebug() << "EE: Layer TRW: can't get edited track in track-related function" << __FUNCTION__;
		return;
	}

	const bool is_route = track->type_id == "sg.trw.route";

	LayerTRWTracks * ght_tracks = is_route ? this->routes : this->tracks;

	/* Convert into list of names for usage with dialog function.
	   TODO: Need to consider how to work best when we can have multiple tracks the same name... */

	std::list<QString> other_tracks_names = ght_tracks->get_sorted_track_name_list_exclude_self(track);

	/* Note the limit to selecting one track only.
	   This is to control the ordering of appending tracks, i.e. the selected track always goes after the current track
	   (otherwise with multiple select the ordering would not be controllable by the user - automatically being alphabetically). */
	const QStringList headers = { is_route ? tr("Select the route to append after the current route") : tr("Select the track to append after the current track") };
	std::list<QString> append_list = a_dialog_select_from_list(other_tracks_names,
								   false,
								   is_route ? tr("Append Route") : tr("Append Track"),
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
		if (is_route) {
			append_track = this->routes->find_track_by_name(*iter);
		} else {
			append_track = this->tracks->find_track_by_name(*iter);
		}

		if (append_track) {
			track->steal_and_append_trackpoints(append_track);
			if (is_route) {
				this->delete_route(append_track);
			} else {
				this->delete_track(append_track);
			}
		}
	}

	this->emit_layer_changed();
}




/**
 * Very similar to append_track_cb() for joining
 * but this allows selection from the 'other' list
 * If a track is selected, then is shows routes and joins the selected one
 * If a route is selected, then is shows tracks and joins the selected one
 */
void LayerTRW::append_other_cb(void)
{
	Track * track = this->get_edited_track();
	if (!track) {
		qDebug() << "EE: Layer TRW: can't get edited track in track-related function" << __FUNCTION__;
		return;
	}

	const bool is_route = track->type_id == "sg.trw.route";

	LayerTRWTracks * ght_mykind = is_route ? this->routes : this->tracks;
	LayerTRWTracks * ght_others = is_route ? this->tracks : this->routes;

	/* Convert into list of names for usage with dialog function.
	   TODO: Need to consider how to work best when we can have multiple tracks the same name... */

	std::list<QString> const other_tracks_names = ght_others->get_sorted_track_name_list_exclude_self(track);

	/* Note the limit to selecting one track only.
	   this is to control the ordering of appending tracks, i.e. the selected track always goes after the current track
	   (otherwise with multiple select the ordering would not be controllable by the user - automatically being alphabetically). */
	const QStringList headers = { is_route ? tr("Select the track to append after the current route") : tr("Select the route to append after the current track") };
	std::list<QString> append_list = a_dialog_select_from_list(other_tracks_names,
								   false,
								   is_route ? tr("Append Track") : tr("Append Route"),
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
		if (is_route) {
			append_track = this->tracks->find_track_by_name(*iter);
		} else {
			append_track = this->routes->find_track_by_name(*iter);
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

			track->steal_and_append_trackpoints(append_track);

			/* Delete copied which is FROM THE OTHER TYPE list. */
			if (is_route) {
				this->delete_track(append_track);
			} else {
				this->delete_route(append_track);
			}
		}
	}

	this->emit_layer_changed();
}




/* Merge by segments. */
void LayerTRW::merge_by_segment_cb(void)
{
	Track * track = this->get_edited_track();
	if (!track) {
		qDebug() << "EE: Layer TRW: can't get edited track in track-related function" << __FUNCTION__;
		return;
	}

	unsigned int segments = track->merge_segments();
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
	Track * orig_track = this->get_edited_track();
	if (!orig_track) {
		qDebug() << "EE: Layer TRW: can't get edited track in track-related function" << __FUNCTION__;
		return;
	}

	if (!orig_track->empty()
	    && !orig_track->get_tp_first()->has_timestamp) {
		Dialog::error(tr("Failed. This track does not have timestamp"), this->get_window());
		return;
	}

	std::list<sg_uid_t> tracks_with_timestamp = this->tracks->find_tracks_with_timestamp_type(true, orig_track);
	if (tracks_with_timestamp.empty()) {
		Dialog::error(tr("Failed. No other track in this layer has timestamp"), this->get_window());
		return;
	}

	static uint32_t threshold_in_minutes = 1;
	if (!a_dialog_time_threshold(tr("Merge Threshold..."),
				     tr("Merge when time between tracks less than:"),
				     &threshold_in_minutes,
				     this->get_window())) {
		return;
	}

	/* Keep attempting to merge all tracks until no merges within the time specified is possible. */
	bool attempt_merge = true;

	while (attempt_merge) {

		/* Don't try again unless tracks have changed. */
		attempt_merge = false;

		/* kamilTODO: why call this here? Shouldn't we call this way earlier? */
		if (orig_track->empty()) {
			return;
		}

		/* Get a list of adjacent-in-time tracks. */
		std::list<Track *> nearby_tracks = this->tracks->find_nearby_tracks_by_time(orig_track, (threshold_in_minutes * 60));

		/* Merge them. */

		for (auto iter = nearby_tracks.begin(); iter != nearby_tracks.end(); iter++) {
			/* Remove trackpoints from merged track, delete track. */
			orig_track->steal_and_append_trackpoints(*iter);
			this->delete_track(*iter);

			/* Tracks have changed, therefore retry again against all the remaining tracks. */
			attempt_merge = true;
		}

		orig_track->sort(Trackpoint::compare_timestamps);
	}

	this->emit_layer_changed();
}



/*
  orig - original track
  points- list of trackpoint lists
*/
bool LayerTRW::create_new_tracks(Track * orig, std::list<TrackPoints *> * points)
{
	for (auto iter = points->begin(); iter != points->end(); iter++) {

		Track * copy = new Track(*orig, (*iter)->begin(), (*iter)->end());

		const QString new_name = this->new_unique_element_name(orig->type_id, orig->name);
		copy->set_name(new_name);

		if (orig->type_id == "sg.trw.route") {
			this->add_route(copy);
		} else {
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
	this->emit_layer_changed();

	return true;
}




/**
 * Split a track at the currently selected trackpoint
 */
void LayerTRW::split_at_trackpoint_cb(void)
{
	Track * selected_track = this->get_edited_track();
	if (!selected_track) {
		return;
	}

	Track * new_track = selected_track->split_at_trackpoint(selected_track->selected_tp);
	if (new_track) {
		this->set_edited_track(new_track, new_track->begin());
		this->add_track(new_track);
		this->emit_layer_changed();
	}
}




/* end of split/merge routines */




void LayerTRW::delete_selected_tp(Track * track)
{
	TrackPoints::iterator new_tp_iter = track->delete_trackpoint(track->selected_tp.iter);

	if (new_tp_iter != track->end()) {
		/* Set to current to the available adjacent trackpoint. */
		track->selected_tp.iter = new_tp_iter;
		track->calculate_bounds();
	} else {
		this->cancel_current_tp(false);
	}
}




/**
 * Delete the selected point
 */
void LayerTRW::delete_point_selected_cb(void)
{
	Track * selected_track = this->get_edited_track();
	if (!selected_track) {
		qDebug() << "EE: LayerTRW: can't get selected track in track callback" << __FUNCTION__;
		return;
	}


	if (!selected_track->selected_tp.valid) {
		return;
	}

	this->delete_selected_tp(selected_track);

	/* Track has been updated so update tps: */
	this->cancel_tps_of_track(selected_track);

	this->emit_layer_changed();
}




/**
 * Delete adjacent track points at the same position
 * AKA Delete Dulplicates on the Properties Window
 */
void LayerTRW::delete_points_same_position_cb(void)
{
	Track * track = this->get_edited_track();
	if (!track) {
		return;
	}

	unsigned long removed = track->remove_dup_points();

	/* Track has been updated so update tps: */
	this->cancel_tps_of_track(track);

	/* Inform user how much was deleted as it's not obvious from the normal view. */
	char str[64];
	const char *tmp_str = ngettext("Deleted %ld point", "Deleted %ld points", removed);
	snprintf(str, 64, tmp_str, removed);
	Dialog::info(str, this->get_window());

	this->emit_layer_changed();
}




/**
 * Delete adjacent track points with the same timestamp
 * Normally new tracks that are 'routes' won't have any timestamps so should be OK to clean up the track
 */
void LayerTRW::delete_points_same_time_cb(void)
{
	Track * track = this->get_edited_track();
	if (!track) {
		return;
	}

	unsigned long removed = track->remove_same_time_points();

	/* Track has been updated so update tps: */
	this->cancel_tps_of_track(track);

	/* Inform user how much was deleted as it's not obvious from the normal view. */
	char str[64];
	const char *tmp_str = ngettext("Deleted %ld point", "Deleted %ld points", removed);
	snprintf(str, 64, tmp_str, removed);
	Dialog::info(str, this->get_window());

	this->emit_layer_changed();
}




/**
 * Insert a point
 */
void LayerTRW::insert_point_after_cb(void)
{
	Track * selected_track = this->get_edited_track();
	if (!selected_track) {
		qDebug() << "EE: LayerTRW: can't get selected track in track callback" << __FUNCTION__;
		return;
	}

	selected_track->create_tp_next_to_reference_tp(&selected_track->selected_tp, false);

	this->emit_layer_changed();
}




void LayerTRW::insert_point_before_cb(void)
{
	Track * selected_track = this->get_edited_track();
	if (!selected_track) {
		qDebug() << "EE: LayerTRW: can't get selected track in track callback" << __FUNCTION__;
		return;
	}

	selected_track->create_tp_next_to_reference_tp(&selected_track->selected_tp, true);

	this->emit_layer_changed();
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
	char * cmd = g_strdup_printf("%s %s%s", diary_program.toUtf8().constData(), "--date=", date_str);
	if (!g_spawn_command_line_async(cmd, &err)) {
		Dialog::error(tr("Could not launch %1 to open file.").arg(diary_program), this->get_window());
		g_error_free(err);
	}
	free(cmd);
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
				    astro_program.toUtf8().constData(), "-c", tmp, "--full-screen no", "--sky-date", date_str, "--sky-time", time_str, "--latitude", lat_str, "--longitude", lon_str, "--altitude", alt_str);
	fprintf(stderr, "WARNING: %s\n", cmd);
	if (!g_spawn_command_line_async(cmd, &err)) {
		Dialog::error(tr("Could not launch %1").arg(astro_program), this->get_window());
		fprintf(stderr, "WARNING: %s\n", err->message);
		g_error_free(err);
	}
	util_add_to_deletion_list(tmp);
	free(tmp);
	free(cmd);
}




/*
  TODO: move to common file, e.g. to astro.cpp.

  Format of stellarium lat & lon seems designed to be particularly awkward
  who uses ' & " in the parameters for the command line?!
  -1d4'27.48"
  +53d58'16.65"
*/
char * SlavGPS::convert_to_dms(double dec)
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




void LayerTRW::delete_selected_tracks_cb(void) /* Slot. */
{
	/* Ensure list of track names offered is unique. */
	QString duplicate_name = this->tracks->has_duplicate_track_names();
	if (duplicate_name != "") {
		if (Dialog::yes_or_no(tr("Multiple entries with the same name exist. This method only works with unique names. Force unique names now?")), this->get_window()) {
			this->tracks->uniquify(this->track_sort_order);

			/* Update. */
			g_tree->emit_update_window();
		} else {
			return;
		}
	}

	/* Sort list alphabetically for better presentation. */
	std::list<QString> const all = this->tracks->get_sorted_track_name_list();

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
	this->tree_view->set_tree_item_timestamp(this->index, this->get_timestamp());

	this->emit_layer_changed();
}




void LayerTRW::delete_selected_routes_cb(void) /* Slot. */
{
	/* Ensure list of track names offered is unique. */
	QString duplicate_name = this->routes->has_duplicate_track_names();
	if (duplicate_name != "") {
		if (Dialog::yes_or_no(tr("Multiple entries with the same name exist. This method only works with unique names. Force unique names now?"), this->get_window())) {
			this->routes->uniquify(this->track_sort_order);

			/* Update. */
			g_tree->emit_update_window();
		} else {
			return;
		}
	}

	/* Sort list alphabetically for better presentation. */
	std::list<QString> all = this->routes->get_sorted_track_name_list();

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

	this->emit_layer_changed();
}




void LayerTRW::delete_selected_waypoints_cb(void)
{
	/* Ensure list of waypoint names offered is unique. */
	QString duplicate_name = this->waypoints->has_duplicate_waypoint_names();
	if (duplicate_name != "") {
		if (Dialog::yes_or_no(tr("Multiple entries with the same name exist. This method only works with unique names. Force unique names now?"), this->get_window())) {
			this->waypoints->uniquify(this->wp_sort_order);

			/* Update. */
			g_tree->emit_update_window();
		} else {
			return;
		}
	}

	/* Sort list alphabetically for better presentation. */
	std::list<QString> all = this->waypoints->get_sorted_wp_name_list();
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

	this->waypoints->calculate_bounds();
	/* Reset layer timestamp in case it has now changed. */
	this->tree_view->set_tree_item_timestamp(this->index, this->get_timestamp());
	this->emit_layer_changed();

}




/**
   \brief Create the latest list of waypoints in this layer

   Function returns freshly allocated container. The container itself is owned by caller. Elements in container are not.
*/
std::list<SlavGPS::Waypoint *> * LayerTRW::create_waypoints_list()
{
	std::list<Waypoint *> * wps = new std::list<Waypoint *>;

	for (auto iter = this->waypoints->items.begin(); iter != this->waypoints->items.end(); iter++) {
		wps->push_back((*iter).second);
	}

	return wps;
}




/**
 * Create the latest list of tracks with the associated layer(s).
 * Although this will always be from a single layer here.
 */
std::list<Track *> * LayerTRW::create_tracks_list(const QString & item_type_id)
{
	std::list<Track *> * tracks_ = new std::list<Track *>;
	if (item_type_id == "sg.trw.tracks") {
		tracks_ = this->tracks->get_track_values(tracks_);
	} else {
		tracks_ = this->routes->get_track_values(tracks_);
	}

	return tracks_;
}




void LayerTRW::tracks_stats_cb(void)
{
	layer_trw_show_stats(this->name, this, "sg.trw.tracks", this->get_window());
}




void LayerTRW::routes_stats_cb(void)
{
	layer_trw_show_stats(this->name, this, "sg.trw.routes", this->get_window());
}




bool is_valid_geocache_name(const char *str)
{
	int len = strlen(str);
	return len >= 3 && len <= 7 && str[0] == 'G' && str[1] == 'C' && isalnum(str[2]) && (len < 4 || isalnum(str[3])) && (len < 5 || isalnum(str[4])) && (len < 6 || isalnum(str[5])) && (len < 7 || isalnum(str[6]));
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
			this->tpwin->reset_dialog_data();
		}
	}

	Track * track = this->get_edited_track();
	if (track && track->selected_tp.valid) {
		track->selected_tp.valid = false;
		this->reset_edited_track();

		this->emit_layer_changed();
	}
}




void LayerTRW::tpwin_update_dialog_data()
{
	Track * track = this->get_edited_track();
	if (track) {
		/* Notional center of a track is simply an average of the bounding box extremities. */
		struct LatLon ll_center = { (track->bbox.north + track->bbox.south)/2, (track->bbox.east + track->bbox.west)/2 };
		Coord coord(ll_center, this->coord_mode);  /* kamilTODO: this variable is unused. */
		this->tpwin->set_dialog_data(track, track->selected_tp.iter, track->type_id == "sg.trw.route");
	}
}




void LayerTRW::trackpoint_properties_cb(int response) /* Slot. */
{
	assert (this->tpwin != NULL);

	Track * track = this->get_edited_track();

	switch (response) {
	case SG_TRACK_CLOSE_DIALOG:
		this->cancel_current_tp(true);
		//this->tpwin->reject();
		break;

	case SG_TRACK_SPLIT_TRACK_AT_CURRENT_TP: {
		if (!track) {
			return;
		}
		if (!track->selected_tp.valid) {
			return;
		}
		if (track->selected_tp.iter == track->begin()) {
			/* Can't split track at first trackpoint in track. */
			break;
		}
		if (std::next(track->selected_tp.iter) == track->end()) {
			/* Can't split track at last trackpoint in track. */
			break;
		}

		Track * new_track = track->split_at_trackpoint(track->selected_tp);
		if (new_track) {
			this->set_edited_track(new_track, new_track->begin());
			this->add_track(new_track);
			this->emit_layer_changed();
		}

		this->tpwin_update_dialog_data();
		}
		break;

	case SG_TRACK_DELETE_CURRENT_TP:
		if (!track) {
			return;
		}
		if (!track->selected_tp.valid) {
			return;
		}
		this->delete_selected_tp(track);

		if (track->selected_tp.valid) {
			/* Update Trackpoint Properties with the available adjacent trackpoint. */
			this->tpwin_update_dialog_data();
		}

		this->emit_layer_changed();
		break;

	case SG_TRACK_GO_FORWARD:
		if (!track) {
			break;
		}
		if (!track->selected_tp.valid) {
			return;
		}
		if (std::next(track->selected_tp.iter) == track->end()) {
			/* Can't go forward if we are already at the end. */
			break;
		}

		track->selected_tp.iter++;
		this->tpwin_update_dialog_data();
		this->emit_layer_changed(); /* TODO longone: either move or only update if tp is inside drawing window */

		break;

	case SG_TRACK_GO_BACK:
		if (!track) {
			break;
		}
		if (!track->selected_tp.valid) {
			return;
		}
		if (track->selected_tp.iter == track->begin()) {
			/* Can't go back if we are already at the beginning. */
			break;
		}

		track->selected_tp.iter--;
		this->tpwin_update_dialog_data();
		this->emit_layer_changed();

		break;

	case SG_TRACK_INSERT_TP_AFTER:
		if (!track) {
			break;
		}
		if (!track->selected_tp.valid) {
			return;
		}
		if (std::next(track->selected_tp.iter) == track->end()) {
			/* Can't inset trackpoint after last
			   trackpoint in track.  This is because the
			   algorithm for inserting a new trackpoint
			   uses two existing trackpoints and adds the
			   new one between the two existing tps. */
			break;
		}

		track->create_tp_next_to_reference_tp(&track->selected_tp, false);
		this->emit_layer_changed();
		break;

	case SG_TRACK_CHANGED:
		this->emit_layer_changed();
		break;

	default:
		qDebug() << "EE: LayerTRW: Trackpoint Properties Dialog: unhandled dialog response" << response;
	}
}




/**
   @vertical: The reposition strategy. If Vertical moves dialog vertically, otherwise moves it horizontally

   Try to reposition a dialog if it's over the specified coord
   so to not obscure the item of interest
*/
void LayerTRW::dialog_shift(QDialog * dialog, Coord * coord, bool vertical)
{
	Window * parent_window = this->get_window(); /* i.e. the main window. */
	Viewport * viewport = g_tree->tree_get_main_viewport();

	/* TODO: improve this code, it barely works. */

	int win_pos_x = parent_window->x();
	int win_pos_y = parent_window->y();
	int win_size_x = parent_window->width();
	int win_size_y = parent_window->height();

	const QPoint global_dialog_pos = dialog->mapToGlobal(QPoint(0, 0));
	int dialog_width = dialog->width();
	int dialog_height = dialog->height();

	/* Viewport's position relative to parent widget. */
	int viewport_pos_x = viewport->x();
	int viewport_pos_y = viewport->y();


	/* Dialog not 'realized'/positioned - so can't really do any repositioning logic. */
	if (dialog_width <= 2 || dialog_height <= 2) {
		qDebug() << "WW: Layer TRW: can't position dialog window";
		return;
	}


	int in_viewport_x, in_viewport_y; /* In viewport pixels. */
	viewport->coord_to_screen(coord, &in_viewport_x, &in_viewport_y);
	const QPoint global_coord_pos = viewport->mapToGlobal(QPoint(in_viewport_x, in_viewport_y));

	if (global_coord_pos.x() < global_dialog_pos.x()) {
		/* Point visible, on left side of dialog. */
		return;
	}
	if (global_coord_pos.y() < global_dialog_pos.y()) {
		/* Point visible, above dialog. */
		return;
	}
	if (global_coord_pos.x() > (global_dialog_pos.x() + dialog_width)) {
		/* Point visible, on right side of dialog. */
		return;
	}
	if (global_coord_pos.y() > (global_dialog_pos.y() + dialog_height)) {
		/* Point visible, below dialog. */
		return;
	}

	const QPoint in_window_coord_pos = parent_window->mapFromGlobal(global_coord_pos);

	if (vertical) {
		/* Shift up or down. */
		const int viewport_height = viewport->get_height();
		if (in_viewport_y < viewport_height / 2) {
			/* Point's coordinate is in upper half of viewport.
			   Move dialog below the point. Don't change x coordinate of dialog. */
			dialog->move(dialog->x(), in_window_coord_pos.y() + 10);
		} else {
			/* Point's coordinate is in lower half of viewport.
			   Move dialog above the point. Don't change x coordinate of dialog. */
			int dest_y = in_window_coord_pos.y() - dialog_height - 10;
			if (dest_y < 0) {
				/* TODO: rewrite it to check that dialog's title bar is still visible. */
				dest_y = 0;
			}
			dialog->move(dialog->x(), dest_y);
		}
	} else {
		/* Shift left or right. */
		const int viewport_width = viewport->get_width();
		if (in_viewport_x < viewport_width / 2) {
			/* Point's coordinate is in left half of viewport.
			   Move dialog to the right of point. Don't change y coordinate of dialog. */
			dialog->move(400, dialog->y());
		} else {
			/* Point's coordinate is in right half of viewport.
			   Move dialog to the left of point. Don't change y coordinate of dialog. */
			dialog->move(0, dialog->y());
		}
	}

#if 0
	int dest_x = 0;
	int dest_y = 0;

	/* Work out the 'bounding box' in pixel terms of the dialog and only move it when over the position. */
	if (!gtk_widget_translate_coordinates(viewport->get_widget(), GTK_WIDGET(parent_window), 0, 0, &dest_x, &dest_y)) {
		return;
	}

	/* Transform Viewport pixels into absolute pixels. */
	int tmp_xx = in_viewport_x + dest_x + win_pos_x - 10;
	int tmp_yy = in_viewport_y + dest_y + win_pos_y - 10;

	/* Is dialog over the point (to within an  ^^ edge value). */
	if ((tmp_xx > global_dialog_pos.x()) && (tmp_xx < (global_dialog_pos.x() + dialog_width))
	    && (tmp_yy > global_dialog_pos.y()) && (tmp_yy < (global_dialog_pos.y() + dialog_height))) {

		if (vertical) {
			/* Shift up<->down. */
			int hh = viewport->get_height();

			/* Consider the difference in viewport to the full window. */
			int offset_y = dest_y;
			/* Add difference between dialog and window sizes. */
			offset_y += win_pos_y + (hh/2 - dialog_height)/2;

			if (in_viewport_y > hh/2) {
				/* Point in bottom half, move window to top half. */
				dialog->move(global_dialog_pos.x(), offset_y);
			} else {
				/* Point in top half, move dialog down. */
				dialog->move(global_dialog_pos.x(), hh/2 + offset_y);
			}
		} else {
			/* Shift left<->right. */
			int ww = viewport->get_width();

			/* Consider the difference in viewport to the full window. */
			int offset_x = dest_x;
			/* Add difference between dialog and window sizes. */
			offset_x += win_pos_x + (ww/2 - dialog_width)/2;

			if (in_viewport_x > ww/2) {
				/* Point on right, move window to left. */
				dialog->move(offset_x, global_dialog_pos.y());
			} else {
				/* Point on left, move right. */
				dialog->move(ww/2 + offset_x, global_dialog_pos.y());
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

		connect(this->tpwin->signal_mapper, SIGNAL (mapped(int)), this, SLOT (trackpoint_properties_cb(int)));

		//QObject::connect(this->tpwin, SIGNAL("delete-event"), this, SLOT (trw_layer_cancel_current_tp_cb));
	}
	this->tpwin->show();


	Track * track = this->get_edited_track();
	if (track && track->selected_tp.valid) {
		/* Get tp pixel position. */
		Trackpoint * tp = *track->selected_tp.iter;

		/* Shift up/down to try not to obscure the trackpoint. */
		this->dialog_shift(this->tpwin, &tp->coord, true);

		this->tpwin_update_dialog_data();
	}
	/* Set layer name and TP data. */
}





static int create_thumbnails_thread(BackgroundJob * bg_job);



/* Structure for thumbnail creating data used in the background thread. */
class ThumbnailCreator : public BackgroundJob {
public:
	ThumbnailCreator(LayerTRW * layer, const QStringList & pictures);

	LayerTRW * layer = NULL;  /* Layer needed for redrawing. */
	QStringList pictures_list;
};




ThumbnailCreator::ThumbnailCreator(LayerTRW * layer_, const QStringList & pictures)
{
	this->thread_fn = create_thumbnails_thread;
	this->n_items = pictures.size();

	this->layer = layer_;
	this->pictures_list = pictures;
}




static int create_thumbnails_thread(BackgroundJob * bg_job)
{
	ThumbnailCreator * creator = (ThumbnailCreator *) bg_job;

	const int n = creator->pictures_list.size();
	for (int i = 0; i < n; i++) {
		const QString path = creator->pictures_list.at(i);

		Thumbnails::generate_thumbnail(path);
		if (0 != a_background_thread_progress(bg_job, (i + 1.0) / n)) {
			return -1; /* Abort thread. */
		}
	}

	/* Redraw to show the thumbnails as they are now created. */
	if (creator->layer) {
		creator->layer->emit_layer_changed(); /* NB update from background thread. */
	}

	return 0;
}




void LayerTRW::verify_thumbnails(void)
{
	if (this->has_verified_thumbnails) {
		return;
	}

	QStringList * pics = this->waypoints->image_wp_make_list();
	int len = pics->size();
	if (!len) {
		delete pics;
		return;
	}

	const QString job_description = tr("Creating %1 Image Thumbnails...").arg(len);
	ThumbnailCreator * creator = new ThumbnailCreator(this, *pics);
	a_background_thread(creator, ThreadPoolType::LOCAL, job_description);

	delete pics; /* The list of pictures has been copied to Thumbnail creator, so it's safe to delete it here. */
}




void LayerTRW::sort_all()
{
	if (!this->tree_view) {
		return;
	}

	/* Obviously need 2 to tango - sorting with only 1 (or less) is a lonely activity! */
	if (this->tracks->items.size() > 1) {
		this->tree_view->sort_children(this->tracks->get_index(), this->track_sort_order);
	}

	if (this->routes->items.size() > 1) {
		this->tree_view->sort_children(this->routes->get_index(), this->track_sort_order);
	}

	if (this->waypoints->items.size() > 1) {
		this->tree_view->sort_children(this->waypoints->get_index(), this->wp_sort_order);
	}
}




/**
 * Get the earliest timestamp available for this layer.
 */
time_t LayerTRW::get_timestamp()
{
	time_t timestamp_tracks = this->tracks->get_earliest_timestamp();
	time_t timestamp_waypoints = this->waypoints->get_earliest_timestamp();
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
	if (this->tree_view) {
		this->verify_thumbnails();
	}
	this->tracks->assign_colors(this->track_drawing_mode, this->track_color_common);
	this->routes->assign_colors(-1, this->track_color_common);

	this->waypoints->calculate_bounds();
	this->tracks->calculate_bounds();
	this->routes->calculate_bounds();


	/*
	  Apply tree view sort after loading all the tracks for this
	  layer (rather than sorted insert on each individual track
	  additional) and after subsequent changes to the properties
	  as the specified order may have changed.  since the sorting
	  of a tree view section is now very quick.  NB sorting is also
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
			QDateTime meta_time;
			const time_t timestamp = this->get_timestamp();
			if (timestamp == 0) {
				/* No time found - so use 'now' for the metadata time. */
				meta_time = QDateTime::currentDateTime(); /* The method returns time in local time zone. */
			} else {
				meta_time.setMSecsSinceEpoch(timestamp * 1000); /* TODO: replace with setSecsSinceEpoch() in future. */
			}

			this->metadata->timestamp = meta_time.toString(Qt::ISODate);
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
		this->tracks->uniquify(this->track_sort_order);
		this->routes->uniquify(this->track_sort_order);
		this->waypoints->uniquify(this->wp_sort_order);

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
		this->waypoints->change_coord_mode(dest_mode);
		this->tracks->change_coord_mode(dest_mode);
		this->routes->change_coord_mode(dest_mode);
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

void vik_track_download_map(Track * trk, LayerMap * layer_map, double zoom_level)
{
	std::list<Rect *> * rects_to_download = trk->get_map_rectangles(zoom_level);
	if (!rects_to_download) {
		return;
	}

	for (auto rect_iter = rects_to_download->begin(); rect_iter != rects_to_download->end(); rect_iter++) {
		layer_map->download_section(&(*rect_iter)->tl, &(*rect_iter)->br, zoom_level);
	}

	if (rects_to_download) {
		for (auto rect_iter = rects_to_download->begin(); rect_iter != rects_to_download->end(); rect_iter++) {
			free(*rect_iter);
		}
		delete rects_to_download;
	}
}




void LayerTRW::download_map_along_track_cb(void)
{
	const QStringList zoom_labels = { "0.125", "0.25", "0.5", "1", "2", "4", "8", "16", "32", "64", "128", "256", "512", "1024" };
	std::vector<double> zoom_values = { 0.125, 0.25, 0.5, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024 };

	LayersPanel * panel = g_tree->tree_get_items_tree();
	const Viewport * viewport = g_tree->tree_get_main_viewport();

	Track * track = this->get_edited_track();
	if (!track) {
		return;
	}

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

	vik_track_download_map(track, *iter, zoom_values[selected_zoom_idx]);

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

		while (this->highest_wp_number > 0 && !this->waypoints->find_waypoint_by_name(buf)) {
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
std::list<Track *> * LayerTRW::create_tracks_list()
{
	std::list<Track *> * tracks_ = new std::list<Track *>;
	tracks_ = this->tracks->get_track_values(tracks_);
	tracks_ = this->routes->get_track_values(tracks_);
	return tracks_;
}




void LayerTRW::track_list_dialog_cb(void)
{
	QString title = tr("%1: Track and Route List").arg(this->name);
	track_list_dialog(title, this, "");
}




void LayerTRW::waypoint_list_dialog_cb(void) /* Slot. */
{
	QString title = tr("%1: Waypoint List").arg(this->name);
	waypoint_list_dialog(title, this);
}




int LayerTRW::read_layer_data(FILE * file, char const * dirpath)
{
	qDebug() << "DD: Layer TRW: Read Layer Data from File: will call gpspoint_read_file() to read Layer Data";
	return (int) a_gpspoint_read_file(file, this, dirpath);
}




void LayerTRW::write_layer_data(FILE * file) const
{
	fprintf(file, "\n\n~LayerData\n");
	a_gpspoint_write_file(file, this);
	fprintf(file, "~EndLayerData\n");
}




LayerTRW::LayerTRW() : Layer()
{
	this->type = LayerType::TRW;
	strcpy(this->debug_string, "TRW");
	this->interface = &vik_trw_layer_interface;

	this->tracks = new LayerTRWTracks(false);
	this->routes = new LayerTRWTracks(true);
	this->waypoints = new LayerTRWWaypoints();

	this->tracks->owning_layer = this;
	this->routes->owning_layer = this;
	this->waypoints->owning_layer = this;

	memset(&coord_mode, 0, sizeof (CoordMode));

	this->set_initial_parameter_values();

	/* Param settings that are not available via the GUI. */
	/* Force to on after processing params (which defaults them to off with a zero value). */
	this->waypoints->visible = this->tracks->visible = this->routes->visible = true;

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
}




LayerTRW::~LayerTRW()
{
	this->wp_image_cache_flush();

	delete this->tpwin;

	delete this->tracks;
	delete this->routes;
	delete this->waypoints;

}




/* To be called right after constructor. */
void LayerTRW::set_coord_mode(CoordMode mode)
{
	this->coord_mode = mode;
}




bool LayerTRW::handle_selection_in_tree()
{
	qDebug() << "II: LayerTRW: handle selection in tree: top";

	/* Reset info about old selection. */
	this->reset_internal_selections();

	/* Set info about current selection. */
	g_tree->selected_tree_item = this;

	/* Set highlight thickness. */
	g_tree->tree_get_main_viewport()->set_highlight_thickness(this->get_property_track_thickness());

	/* Mark for redraw. */
	return true;
}




bool LayerTRW::clear_highlight()
{
#ifdef K
	/* TODO: this is not used anymore. What to do with this code? When to return true and when false? */
	if (this->selected_sublayer_index) {
		this->selected_sublayer_index = NULL;
		return true;
	} else {
		return false;
	}
#endif
	return true;
}




/**
   \brief Get layer's selected track or route

   Returns track or route selected in this layer (if any track or route is selected in this layer)
   Returns NULL otherwise.
*/
Track * LayerTRW::get_edited_track()
{
	return this->current_track_;
}




void LayerTRW::set_edited_track(Track * track, const TrackPoints::iterator & tp_iter)
{
	if (track) {
		this->current_track_ = track;
		this->current_track_->selected_tp.iter = tp_iter;
		this->current_track_->selected_tp.valid = true;
	} else {
		qDebug() << "EE: Layer TRW: Set Current Track (1): NULL track";
	}
}





void LayerTRW::set_edited_track(Track * track)
{
	if (track) {
		this->current_track_ = track;
		this->current_track_->selected_tp.valid = false;
	} else {
		qDebug() << "EE: Layer TRW: Set Current Track (2): NULL track";
	}
}





void LayerTRW::reset_edited_track(void)
{
	this->current_track_ = NULL;
}




/**
   \brief Get layer's selected waypoint

   Returns waypoint selected in this layer (if any waypoint is selected in this layer)
   Returns NULL otherwise.
*/
Waypoint * LayerTRW::get_edited_wp()
{
	return this->current_wp_;
}




void LayerTRW::set_edited_wp(Waypoint * wp)
{
	if (wp) {
		this->current_wp_ = wp;
	} else {
		qDebug() << "EE: Layer TRW: Set Current Waypoint: null waypoint";
	}
}




void LayerTRW::reset_edited_wp(void)
{
	this->current_wp_ = NULL;
}




bool LayerTRW::get_track_creation_in_progress() const
{
	LayerToolTRWNewTrack * new_track_tool = (LayerToolTRWNewTrack *) g_tree->tree_get_main_window()->get_toolbox()->get_tool(LAYER_TRW_TOOL_CREATE_TRACK);
	return new_track_tool->creation_in_progress == this;
}




void LayerTRW::reset_track_creation_in_progress()
{
	LayerToolTRWNewTrack * new_track_tool = (LayerToolTRWNewTrack *) g_tree->tree_get_main_window()->get_toolbox()->get_tool(LAYER_TRW_TOOL_CREATE_TRACK);
	if (new_track_tool->creation_in_progress == this) {
		new_track_tool->creation_in_progress = NULL;
	}
}




bool LayerTRW::get_route_creation_in_progress() const
{
	LayerToolTRWNewRoute * new_route_tool = (LayerToolTRWNewRoute *) g_tree->tree_get_main_window()->get_toolbox()->get_tool(LAYER_TRW_TOOL_CREATE_ROUTE);
	return new_route_tool->creation_in_progress == this;
}




void LayerTRW::reset_route_creation_in_progress()
{
	LayerToolTRWNewRoute * new_route_tool = (LayerToolTRWNewRoute *) g_tree->tree_get_main_window()->get_toolbox()->get_tool(LAYER_TRW_TOOL_CREATE_ROUTE);
	if (new_route_tool->creation_in_progress == this) {
		new_route_tool->creation_in_progress = NULL;
	}
}




void LayerTRW::show_wp_picture_cb(void) /* Slot. */
{
	Waypoint * wp = this->get_edited_wp();
	if (!wp) {
		qDebug() << "EE: Layer TRW: no waypoint in waypoint-related callback" << __FUNCTION__;
		return;
	}

	const QString program = Preferences::get_image_viewer();
	const QString image_path = this->get_edited_wp()->image;
#ifdef K
	char * quoted_file = g_shell_quote((char *) image_path);
#endif
	QStringList args;
	args << image_path;

	qDebug() << "II: Layer TRW: Show WP picture:" << program << image_path;

	/* "Fire and forget". The viewer will run detached from this application. */
	QProcess::startDetached(program, args);

	/* TODO: add handling of errors from process. */
}
