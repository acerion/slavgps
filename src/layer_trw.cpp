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




#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <cassert>




#include <QDateTime>
#include <QInputDialog>
#include <QStandardPaths>




#include "layer_aggregate.h"
#include "layer_trw.h"
#include "layer_trw_painter.h"
#include "layer_trw_tools.h"
#include "layer_trw_dialogs.h"
#include "layer_trw_waypoint_properties.h"
#include "layer_trw_waypoint_list.h"
#include "layer_trw_track_internal.h"
#include "layer_trw_track_list_dialog.h"
#include "layer_trw_trackpoint_properties.h"
#include "layer_map.h"
#include "tree_view_internal.h"
#include "vikutils.h"
#include "file.h"
#include "dialog.h"
#include "dem.h"
#include "background.h"
#include "util.h"
#include "application_state.h"
#include "ui_builder.h"
#include "layers_panel.h"
#include "preferences.h"
#include "util.h"
#include "generic_tools.h"
#include "toolbox.h"
#include "thumbnails.h"
#include "datasources.h"
#include "statusbar.h"
#include "osm_traces.h"
#include "layer_gps.h"
#include "layer_trw_stats.h"
#include "geonames_search.h"
#ifdef VIK_CONFIG_GEOTAG
#include "layer_trw_geotag.h"
#include "geotag_exif.h"
#endif


#ifdef K_INCLUDES
#include "garmin_symbols.h"
#include "gpx.h"
#include "dem_cache.h"
#include "babel.h"
#include "acquire.h"
#include "external_tools.h"
#include "external_tool_datasources.h"
#include "ui_util.h"
#include "routing.h"
#endif

#include "geojson.h"
#include "clipboard.h"
#include "gpspoint.h"
#include "widget_list_selection.h"
#include "viewport_internal.h"




#define POINTS 1
#define LINES 2

/* This is how it knows when you click if you are clicking close to a trackpoint. */
#define TRACKPOINT_SIZE_APPROX 5
#define WAYPOINT_SIZE_APPROX 5

#define MIN_STOP_LENGTH 15
#define MAX_STOP_LENGTH 86400




using namespace SlavGPS;




#define SG_MODULE "Layer TRW"




extern Tree * g_tree;




static void trw_layer_cancel_current_tp_cb(LayerTRW * layer, bool destroy);




/****** PARAMETERS ******/

enum {
	PARAMETER_GROUP_WAYPOINTS,
	PARAMETER_GROUP_TRACKS,
	PARAMETER_GROUP_IMAGES,
	PARAMETER_GROUP_TRACKS_ADV,
	PARAMETER_GROUP_METADATA
};


static std::vector<SGLabelID> params_track_drawing_modes = {
	SGLabelID(QObject::tr("Draw by Track"),                  (int) LayerTRWTrackDrawingMode::ByTrack),
	SGLabelID(QObject::tr("Draw by Speed"),                  (int) LayerTRWTrackDrawingMode::BySpeed),
	SGLabelID(QObject::tr("All Tracks Have The Same Color"), (int) LayerTRWTrackDrawingMode::AllSameColor),
};

static std::vector<SGLabelID> params_wpsymbols = {
	SGLabelID(QObject::tr("Filled Square"), (int) GraphicMarker::FilledSquare),
	SGLabelID(QObject::tr("Square"),        (int) GraphicMarker::Square),
	SGLabelID(QObject::tr("Circle"),        (int) GraphicMarker::Circle),
	SGLabelID(QObject::tr("X"),             (int) GraphicMarker::X),
};

#define MIN_POINT_SIZE 2
#define MAX_POINT_SIZE 10

#define MIN_ARROW_SIZE 3
#define MAX_ARROW_SIZE 20

                                                        /*            min,              max,                     hardcoded default,    step,   digits */
static ParameterScale<int> scale_track_thickness         (              1,               10,                SGVariant((int32_t) 1),       1,        0); /* PARAM_TRACK_THICKNESS */
static ParameterScale<int> scale_track_draw_speed_factor (              0,              100,                       SGVariant(30.0),       1,        0); /* PARAM_TRACK_DRAW_SPEED_FACTOR */
static ParameterScale<int> scale_wp_image_size           (             16,              128,               SGVariant((int32_t) 64),       4,        0); /* PARAM_WP_IMAGE_SIZE */
static ParameterScale<int> scale_wp_image_alpha          (              0,              255,              SGVariant((int32_t) 255),       5,        0); /* PARAM_WP_IMAGE_ALPHA */
static ParameterScale<int> scale_wp_image_cache_capacity (              5,              500,              SGVariant((int32_t) 300),       5,        0); /* PARAM_WP_IMAGE_CACHE_CAPACITY, megabytes */
static ParameterScale<int> scale_track_bg_thickness      (              0,                8,                SGVariant((int32_t) 0),       1,        0); /* PARAM_TRACK_BG_THICKNESS */
static ParameterScale<int> scale_wp_marker_size          (              1,               64,                SGVariant((int32_t) 4),       1,        0); /* PARAM_WP_MARKER_SIZE */
static ParameterScale<int> scale_track_min_stop_length   (MIN_STOP_LENGTH,  MAX_STOP_LENGTH,               SGVariant((int32_t) 60),       1,        0); /* PARAM_TRACK_MIN_STOP_LENGTH */
static ParameterScale<int> scale_track_elevation_factor  (              1,              100,               SGVariant((int32_t) 30),       1,        0); /* PARAM_TRACK_ELEVATION_FACTOR */
static ParameterScale<int> scale_trackpoint_size         ( MIN_POINT_SIZE,   MAX_POINT_SIZE,   SGVariant((int32_t) MIN_POINT_SIZE),       1,        0); /* PARAM_TRACKPOINT_SIZE */
static ParameterScale<int> scale_track_direction_size    ( MIN_ARROW_SIZE,   MAX_ARROW_SIZE,                SGVariant((int32_t) 5),       1,        0); /* PARAM_TRACK_DIRECTION_SIZE */




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
	SGLabelID("None",            (int) TreeViewSortOrder::None),
	SGLabelID("Name Ascending",  (int) TreeViewSortOrder::AlphabeticalAscending),
	SGLabelID("Name Descending", (int) TreeViewSortOrder::AlphabeticalDescending),
	SGLabelID("Date Ascending",  (int) TreeViewSortOrder::DateAscending),
	SGLabelID("Date Descending", (int) TreeViewSortOrder::DateDescending),
};

static SGVariant black_color_default(void)       { return SGVariant(0, 0, 0, 100); } /* Black. */
static SGVariant track_drawing_mode_default(void){ return SGVariant((int32_t) LayerTRWTrackDrawingMode::ByTrack); }
static SGVariant trackbgcolor_default(void)      { return SGVariant(255, 255, 255, 100); }  /* White. */
static SGVariant tnfontsize_default(void)        { return SGVariant((int32_t) FS_MEDIUM); }
static SGVariant wpfontsize_default(void)        { return SGVariant((int32_t) FS_MEDIUM); }
static SGVariant wptextcolor_default(void)       { return SGVariant(255, 255, 255, 100); } /* White. */
static SGVariant wpbgcolor_default(void)         { return SGVariant(0x83, 0x83, 0xc4, 100); } /* Kind of Blue. */
static SGVariant wpsymbol_default(void)          { return SGVariant((int32_t) (int) GraphicMarker::FilledSquare); }
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
	PARAM_DRAW_TRACK_DIRECTIONS,
	PARAM_TRACK_DIRECTION_SIZE,
	PARAM_DRAW_TRACKPOINTS,
	PARAM_TRACKPOINT_SIZE,
	PARAM_DRAW_TRACK_ELEVATION,
	PARAM_TRACK_ELEVATION_FACTOR,
	PARAM_DRAW_TRACK_STOPS,
	PARAM_TRACK_MIN_STOP_LENGTH,
	PARAM_TRACK_BG_THICKNESS,
	PARAM_TRK_BG_COLOR,
	PARAM_TRACK_DRAW_SPEED_FACTOR,
	PARAM_TRACK_SORT_ORDER,
	// Waypoints
	PARAM_WP_LABELS,
	PARAM_WP_LABEL_FONT_SIZE,
	PARAM_WP_MARKER_COLOR,
	PARAM_WP_LABEL_FG_COLOR,
	PARAM_WP_LABEL_BG_COLOR,
	PARAM_WPBA,
	PARAM_WP_MARKER_TYPE,
	PARAM_WP_MARKER_SIZE,
	PARAM_DRAW_WP_SYMBOLS,
	PARAM_WP_SORT_ORDER,
	// WP images
	PARAM_DRAW_WP_IMAGES,
	PARAM_WP_IMAGE_SIZE,
	PARAM_WP_IMAGE_ALPHA,
	PARAM_WP_IMAGE_CACHE_CAPACITY,
	// Metadata
	PARAM_MDDESC,
	PARAM_MDAUTH,
	PARAM_MDTIME,
	PARAM_MDKEYS,
	NUM_PARAMS
};


static ParameterSpecification trw_layer_param_specs[] = {
	{ PARAM_TRACKS_VISIBLE,          "tracks_visible",    SGVariantType::Boolean, PARAMETER_GROUP_HIDDEN,            QObject::tr("Tracks are visible"),               WidgetType::CheckButton,  NULL,                        sg_variant_true,            NULL, NULL },
	{ PARAM_WAYPOINTS_VISIBLE,       "waypoints_visible", SGVariantType::Boolean, PARAMETER_GROUP_HIDDEN,            QObject::tr("Waypoints are visible"),            WidgetType::CheckButton,  NULL,                        sg_variant_true,            NULL, NULL },
	{ PARAM_ROUTES_VISIBLE,          "routes_visible",    SGVariantType::Boolean, PARAMETER_GROUP_HIDDEN,            QObject::tr("Routes are visible"),               WidgetType::CheckButton,  NULL,                        sg_variant_true,            NULL, NULL },

	{ PARAM_DRAW_TRACK_LABELS,       "trackdrawlabels",   SGVariantType::Boolean, PARAMETER_GROUP_TRACKS,            QObject::tr("Draw Labels"),                      WidgetType::CheckButton,  NULL,                        sg_variant_true,            NULL, N_("Note: the individual track controls what labels may be displayed") },
	{ PARAM_TRACK_LABEL_FONT_SIZE,   "trackfontsize",     SGVariantType::Int,     PARAMETER_GROUP_TRACKS_ADV,        QObject::tr("Size of Track Label's Font:"),      WidgetType::ComboBox,     &params_font_sizes,          tnfontsize_default,         NULL, NULL },
	{ PARAM_TRACK_DRAWING_MODE,      "drawmode",          SGVariantType::Int,     PARAMETER_GROUP_TRACKS,            QObject::tr("Track Drawing Mode:"),              WidgetType::ComboBox,     &params_track_drawing_modes, track_drawing_mode_default, NULL, NULL },
	{ PARAM_TRACK_COLOR_COMMON,      "trackcolor",        SGVariantType::Color,   PARAMETER_GROUP_TRACKS,            QObject::tr("All Tracks Color:"),                WidgetType::Color,        NULL,                        black_color_default,        NULL, N_("The color used when 'All Tracks Same Color' drawing mode is selected") },
	{ PARAM_DRAW_TRACK_LINES,        "drawlines",         SGVariantType::Boolean, PARAMETER_GROUP_TRACKS,            QObject::tr("Draw Track Lines"),                 WidgetType::CheckButton,  NULL,                        sg_variant_true,            NULL, NULL },
	{ PARAM_TRACK_THICKNESS,         "line_thickness",    SGVariantType::Int,     PARAMETER_GROUP_TRACKS_ADV,        QObject::tr("Track Thickness:"),                 WidgetType::SpinBoxInt,   &scale_track_thickness,      NULL,                       NULL, NULL },
	{ PARAM_DRAW_TRACK_DIRECTIONS,   "drawdirections",    SGVariantType::Boolean, PARAMETER_GROUP_TRACKS,            QObject::tr("Draw Track Directions:"),           WidgetType::CheckButton,  NULL,                        sg_variant_false,           NULL, NULL },
	{ PARAM_TRACK_DIRECTION_SIZE,    "trkdirectionsize",  SGVariantType::Int,     PARAMETER_GROUP_TRACKS_ADV,        QObject::tr("Direction Size:"),                  WidgetType::SpinBoxInt,   &scale_track_direction_size, NULL,                       NULL, NULL },
	{ PARAM_DRAW_TRACKPOINTS,        "drawpoints",        SGVariantType::Boolean, PARAMETER_GROUP_TRACKS,            QObject::tr("Draw Trackpoints:"),                WidgetType::CheckButton,  NULL,                        sg_variant_true,            NULL, NULL },
	{ PARAM_TRACKPOINT_SIZE,         "trkpointsize",      SGVariantType::Int,     PARAMETER_GROUP_TRACKS_ADV,        QObject::tr("Trackpoint Size:"),                 WidgetType::SpinBoxInt,   &scale_trackpoint_size,      NULL,                       NULL, NULL },
	{ PARAM_DRAW_TRACK_ELEVATION,    "drawelevation",     SGVariantType::Boolean, PARAMETER_GROUP_TRACKS,            QObject::tr("Draw Track Elevation"),             WidgetType::CheckButton,  NULL,                        sg_variant_false,           NULL, NULL },
	{ PARAM_TRACK_ELEVATION_FACTOR,  "elevation_factor",  SGVariantType::Int,     PARAMETER_GROUP_TRACKS_ADV,        QObject::tr("Draw Elevation Height %:"),         WidgetType::HScale,       &scale_track_elevation_factor, NULL,                     NULL, NULL },
	{ PARAM_DRAW_TRACK_STOPS,        "drawstops",         SGVariantType::Boolean, PARAMETER_GROUP_TRACKS,            QObject::tr("Draw Track Stops:"),                WidgetType::CheckButton,  NULL,                        sg_variant_false,           NULL, N_("Whether to draw a marker when trackpoints are at the same position but over the minimum stop length apart in time") },
	{ PARAM_TRACK_MIN_STOP_LENGTH,   "stop_length",       SGVariantType::Int,     PARAMETER_GROUP_TRACKS_ADV,        QObject::tr("Min Stop Length (seconds):"),       WidgetType::SpinBoxInt,   &scale_track_min_stop_length,NULL,                       NULL, NULL },

	{ PARAM_TRACK_BG_THICKNESS,      "bg_line_thickness", SGVariantType::Int,     PARAMETER_GROUP_TRACKS_ADV,        QObject::tr("Track Background Thickness:"),      WidgetType::SpinBoxInt,   &scale_track_bg_thickness,   NULL,                       NULL, NULL },
	{ PARAM_TRK_BG_COLOR,            "trackbgcolor",      SGVariantType::Color,   PARAMETER_GROUP_TRACKS_ADV,        QObject::tr("Track Background Color"),           WidgetType::Color,        NULL,                        trackbgcolor_default,       NULL, NULL },
	{ PARAM_TRACK_DRAW_SPEED_FACTOR, "speed_factor",      SGVariantType::Double,  PARAMETER_GROUP_TRACKS_ADV,        QObject::tr("Draw by Speed Factor (%):"),        WidgetType::HScale,       &scale_track_draw_speed_factor, NULL,                    NULL, N_("The percentage factor away from the average speed determining the color used") },
	{ PARAM_TRACK_SORT_ORDER,        "tracksortorder",    SGVariantType::Int,     PARAMETER_GROUP_TRACKS_ADV,        QObject::tr("Track Sort Order:"),                WidgetType::ComboBox,     &params_sort_order,          sort_order_default,         NULL, NULL },

	{ PARAM_WP_LABELS,               "drawlabels",        SGVariantType::Boolean, PARAMETER_GROUP_WAYPOINTS,         QObject::tr("Draw Waypoint Labels"),             WidgetType::CheckButton,  NULL,                        sg_variant_true,            NULL, NULL },
	{ PARAM_WP_LABEL_FONT_SIZE,      "wpfontsize",        SGVariantType::Int,     PARAMETER_GROUP_WAYPOINTS,         QObject::tr("Font Size of Waypoint's Label:"),   WidgetType::ComboBox,     &params_font_sizes,          wpfontsize_default,         NULL, NULL },
	{ PARAM_WP_MARKER_COLOR,         "wpcolor",           SGVariantType::Color,   PARAMETER_GROUP_WAYPOINTS,         QObject::tr("Color of Waypoint's Marker:"),      WidgetType::Color,        NULL,                        black_color_default,        NULL, NULL },
	{ PARAM_WP_LABEL_FG_COLOR,       "wptextcolor",       SGVariantType::Color,   PARAMETER_GROUP_WAYPOINTS,         QObject::tr("Color of Waypoint's Label:"),       WidgetType::Color,        NULL,                        wptextcolor_default,        NULL, NULL },
	{ PARAM_WP_LABEL_BG_COLOR,       "wpbgcolor",         SGVariantType::Color,   PARAMETER_GROUP_WAYPOINTS,         QObject::tr("Background of Waypoint's Label:"),  WidgetType::Color,        NULL,                        wpbgcolor_default,          NULL, NULL },
	{ PARAM_WPBA,                    "wpbgand",           SGVariantType::Boolean, PARAMETER_GROUP_WAYPOINTS,         QObject::tr("Fake BG Color Translucency:"),      WidgetType::CheckButton,  NULL,                        sg_variant_false,           NULL, NULL },
	{ PARAM_WP_MARKER_TYPE,          "wpsymbol",          SGVariantType::Int,     PARAMETER_GROUP_WAYPOINTS,         QObject::tr("Type of Waypoint's Marker:"),       WidgetType::ComboBox,     &params_wpsymbols,           wpsymbol_default,           NULL, NULL },
	{ PARAM_WP_MARKER_SIZE,          "wpsize",            SGVariantType::Int,     PARAMETER_GROUP_WAYPOINTS,         QObject::tr("Size of Waypoint's Marker:"),       WidgetType::SpinBoxInt,   &scale_wp_marker_size,       NULL,                       NULL, NULL },
	{ PARAM_DRAW_WP_SYMBOLS,         "wpsyms",            SGVariantType::Boolean, PARAMETER_GROUP_WAYPOINTS,         QObject::tr("Draw Waypoint Symbols:"),           WidgetType::CheckButton,  NULL,                        sg_variant_true,            NULL, NULL },
	{ PARAM_WP_SORT_ORDER,           "wpsortorder",       SGVariantType::Int,     PARAMETER_GROUP_WAYPOINTS,         QObject::tr("Waypoint Sort Order:"),             WidgetType::ComboBox,     &params_sort_order,          sort_order_default,         NULL, NULL },

	{ PARAM_DRAW_WP_IMAGES,          "drawimages",        SGVariantType::Boolean, PARAMETER_GROUP_IMAGES,            QObject::tr("Draw Waypoint Images"),             WidgetType::CheckButton,  NULL,                        sg_variant_true,            NULL, NULL },
	{ PARAM_WP_IMAGE_SIZE,           "image_size",        SGVariantType::Int,     PARAMETER_GROUP_IMAGES,            QObject::tr("Waypoint Image Size (pixels):"),    WidgetType::HScale,       &scale_wp_image_size,        NULL,                       NULL, NULL },
	{ PARAM_WP_IMAGE_ALPHA,          "image_alpha",       SGVariantType::Int,     PARAMETER_GROUP_IMAGES,            QObject::tr("Waypoint Image Alpha:"),            WidgetType::HScale,       &scale_wp_image_alpha,       NULL,                       NULL, NULL },
	{ PARAM_WP_IMAGE_CACHE_CAPACITY, "image_cache_size",  SGVariantType::Int,     PARAMETER_GROUP_IMAGES,            QObject::tr("Waypoint Images' Memory Cache Size (MB):"),WidgetType::HScale,&scale_wp_image_cache_capacity,  NULL,                       NULL, NULL },

	{ PARAM_MDDESC,                  "metadatadesc",      SGVariantType::String,  PARAMETER_GROUP_METADATA,          QObject::tr("Description"),                      WidgetType::Entry,        NULL,                        string_default,             NULL, NULL },
	{ PARAM_MDAUTH,                  "metadataauthor",    SGVariantType::String,  PARAMETER_GROUP_METADATA,          QObject::tr("Author"),                           WidgetType::Entry,        NULL,                        string_default,             NULL, NULL },
	{ PARAM_MDTIME,                  "metadatatime",      SGVariantType::String,  PARAMETER_GROUP_METADATA,          QObject::tr("Creation Time"),                    WidgetType::Entry,        NULL,                        string_default,             NULL, NULL },
	{ PARAM_MDKEYS,                  "metadatakeywords",  SGVariantType::String,  PARAMETER_GROUP_METADATA,          QObject::tr("Keywords"),                         WidgetType::Entry,        NULL,                        string_default,             NULL, NULL },

	{ NUM_PARAMS,                    "",                  SGVariantType::Empty,   PARAMETER_GROUP_GENERIC,           "",                                              WidgetType::None,         NULL,                        NULL,                       NULL, NULL }, /* Guard. */
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

	this->parameter_groups.emplace_back(SGLabelID(QObject::tr("Waypoints"),       PARAMETER_GROUP_WAYPOINTS));
	this->parameter_groups.emplace_back(SGLabelID(QObject::tr("Tracks"),          PARAMETER_GROUP_TRACKS));
	this->parameter_groups.emplace_back(SGLabelID(QObject::tr("Waypoint Images"), PARAMETER_GROUP_IMAGES));
	this->parameter_groups.emplace_back(SGLabelID(QObject::tr("Tracks Advanced"), PARAMETER_GROUP_TRACKS_ADV));
	this->parameter_groups.emplace_back(SGLabelID(QObject::tr("Metadata"),        PARAMETER_GROUP_METADATA));

	this->fixed_layer_type_string = "TrackWaypoint"; /* Non-translatable. */

	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_Y;
	// this->action_icon = ...; /* Set elsewhere. */

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

	tool = new LayerToolTRWNewTrack(window, viewport, false);
	tools->insert({{ tool->id_string, tool }});

	tool = new LayerToolTRWNewTrack(window, viewport, true);
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




void LayerTRW::init(void)
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


	if (!QStandardPaths::findExecutable(diary_program).isEmpty()) {
		char *mystdout = NULL;
		char *mystderr = NULL;
		/* Needs RedNotebook 1.7.3+ for support of opening on a specified date. */
		const QString cmd = diary_program + " --version"; // "rednotebook --version"
		if (g_spawn_command_line_sync(cmd.toUtf8().constData(), &mystdout, &mystderr, NULL, NULL)) {
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
						if (SGUtils::version_to_number(token) >= SGUtils::version_to_number("1.7.3")) {
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
	}

	if (!QStandardPaths::findExecutable(geojson_program_export()).isEmpty()) {
		have_geojson_export = true;
	}

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
	if (!QStandardPaths::findExecutable(astro_program).isEmpty()) {
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




void TRWMetadata::set_iso8601_timestamp(const QString & new_timestamp)
{
	this->iso8601_timestamp = new_timestamp;
	this->has_timestamp = true;
}





std::list<TreeItem *> LayerTRW::get_items_by_date(const QDate & search_date) const
{
	std::list<TreeItem *> result;

	/* Routes don't have date, so we don't search for routes by date. */

	result.splice(result.end(), this->get_tracks_by_date(search_date)); /* Move items from one list to another. */
	result.splice(result.end(), this->get_waypoints_by_date(search_date)); /* Move items from one list to another. */

	return result;
}




/**
   Return list of tracks meeting date criterion
*/
std::list<TreeItem *> LayerTRW::get_tracks_by_date(const QDate & search_date) const
{
	return this->tracks.get_tracks_by_date(search_date);
}




/**
   Return list of waypoints meeting date criterion
*/
std::list<TreeItem *> LayerTRW::get_waypoints_by_date(const QDate & search_date) const
{
	return this->waypoints.get_waypoints_by_date(search_date);
}




void LayerTRW::delete_sublayer(TreeItem * sublayer)
{
	if (!sublayer) {
		qDebug() << SG_PREFIX_W << "Argument is NULL";
		return;
	}

	/* true: confirm delete request. */
	this->delete_sublayer_common(sublayer, true);
}




void LayerTRW::cut_sublayer(TreeItem * sublayer)
{
	if (!sublayer) {
		qDebug() << SG_PREFIX_W << "Argument is NULL";
		return;
	}

	this->copy_sublayer_common(sublayer);
	/* true: confirm delete request. */
	this->cut_sublayer_common(sublayer, true);
}




void LayerTRW::copy_sublayer_common(TreeItem * item)
{
	Pickle pickle;

	this->copy_sublayer(item, pickle);

	if (pickle.data_size() > 0) {
#ifdef K_TODO_2_LATER
		Clipboard::copy(ClipboardDataType::Sublayer, LayerType::TRW, item->type_id, pickle, item->name);
#endif
	}
}




void LayerTRW::cut_sublayer_common(TreeItem * item, bool confirm)
{
	this->copy_sublayer_common(item);
	this->delete_sublayer_common(item, confirm);
}




void LayerTRW::copy_sublayer(TreeItem * item, Pickle & pickle)
{
	if (!item) {
		qDebug() << SG_PREFIX_W << "Argument is NULL";
		return;
	}

	Pickle helper_pickle;
	item->marshall(helper_pickle);
	if (helper_pickle.data_size() > 0) {
		pickle.put_pickle(helper_pickle);
	}
}




bool LayerTRW::paste_sublayer(TreeItem * item, Pickle & pickle)
{
	if (!item) {
		qDebug() << SG_PREFIX_W << "Argument is NULL";
		return false;
	}

	if (pickle.data_size() <= 0) {
		return false;
	}

	if (item->type_id == "sg.trw.waypoint") {
		Waypoint * wp = Waypoint::unmarshall(pickle);
		/* When copying - we'll create a new name based on the original. */
		const QString uniq_name = this->new_unique_element_name("sg.trw.waypoint", wp->name);
		wp->set_name(uniq_name);

		this->add_waypoint(wp);

		wp->convert(this->coord_mode);
		this->waypoints.recalculate_bbox();

		/* Consider if redraw necessary for the new item. */
		if (this->visible && this->waypoints.visible && wp->visible) {
			this->emit_layer_changed("TRW - paste waypoint");
		}
		return true;
	}
	if (item->type_id == "sg.trw.track") {
		Track * trk = Track::unmarshall(pickle);

		/* When copying - we'll create a new name based on the original. */
		const QString uniq_name = this->new_unique_element_name("sg.trw.track", trk->name);
		trk->set_name(uniq_name);

		this->add_track(trk);

		trk->convert(this->coord_mode);

		/* Consider if redraw necessary for the new item. */
		if (this->visible && this->tracks.visible && trk->visible) {
			this->emit_layer_changed("TRW - paste track");
		}
		return true;
	}
	if (item->type_id == "sg.trw.route") {
		Track * trk = Track::unmarshall(pickle);
		/* When copying - we'll create a new name based on the original. */
		const QString uniq_name = this->new_unique_element_name("sg.trw.route", trk->name);
		trk->set_name(uniq_name);

		this->add_route(trk);
		trk->convert(this->coord_mode);

		/* Consider if redraw necessary for the new item. */
		if (this->visible && this->routes.visible && trk->visible) {
			this->emit_layer_changed("TRW - paste route");
		}
		return true;
	}
	return false;
}




void LayerTRW::wp_image_cache_flush(void)
{
	this->wp_image_cache.clear();
}




void LayerTRW::wp_image_cache_add(const CachedPixmap & cached_pixmap)
{
	this->wp_image_cache.add(cached_pixmap);
}




bool LayerTRW::set_param_value(param_id_t param_id, const SGVariant & data, bool is_file_operation)
{
	switch (param_id) {
	case PARAM_TRACKS_VISIBLE:
		this->tracks.visible = data.u.val_bool;
		break;
	case PARAM_WAYPOINTS_VISIBLE:
		this->waypoints.visible = data.u.val_bool;
		break;
	case PARAM_ROUTES_VISIBLE:
		this->routes.visible = data.u.val_bool;
		break;
	case PARAM_DRAW_TRACK_LABELS:
		this->painter->draw_track_labels = data.u.val_bool;
		break;
	case PARAM_TRACK_LABEL_FONT_SIZE:
		if (data.u.val_int < FS_NUM_SIZES) {
			this->painter->track_label_font_size = (font_size_t) data.u.val_int;
		}
		break;
	case PARAM_TRACK_DRAWING_MODE:
		this->painter->track_drawing_mode = (LayerTRWTrackDrawingMode) data.u.val_int;
		break;
	case PARAM_TRACK_COLOR_COMMON:
		this->painter->track_color_common = data.val_color;
		this->painter->make_track_pens();
		break;
	case PARAM_DRAW_TRACKPOINTS:
		this->painter->draw_trackpoints = data.u.val_bool;
		break;
	case PARAM_TRACKPOINT_SIZE:
		if (data.u.val_int >= scale_trackpoint_size.min && data.u.val_int <= scale_trackpoint_size.max) {
			this->painter->trackpoint_size = data.u.val_int;
		}
		break;
	case PARAM_DRAW_TRACK_ELEVATION:
		this->painter->draw_track_elevation = data.u.val_bool;
		break;
	case PARAM_DRAW_TRACK_STOPS:
		this->painter->draw_track_stops = data.u.val_bool;
		break;
	case PARAM_DRAW_TRACK_LINES:
		this->painter->draw_track_lines = data.u.val_bool;
		break;
	case PARAM_DRAW_TRACK_DIRECTIONS:
		this->painter->draw_track_directions = data.u.val_bool;
		break;
	case PARAM_TRACK_DIRECTION_SIZE:
		if (data.u.val_int >= scale_track_direction_size.min && data.u.val_int <= scale_track_direction_size.max) {
			this->painter->draw_track_directions_size = data.u.val_int;
		}
		break;
	case PARAM_TRACK_MIN_STOP_LENGTH:
		if (data.u.val_int >= scale_track_min_stop_length.min && data.u.val_int <= scale_track_min_stop_length.max) {
			this->painter->track_min_stop_length = data.u.val_int;
		}
		break;
	case PARAM_TRACK_ELEVATION_FACTOR:
		if (data.u.val_int >= scale_track_elevation_factor.min && data.u.val_int <= scale_track_elevation_factor.max) {
			this->painter->track_elevation_factor = data.u.val_int;
		}
		break;
	case PARAM_TRACK_THICKNESS:
		if (data.u.val_int >= scale_track_thickness.min && data.u.val_int <= scale_track_thickness.max) {
			if (data.u.val_int != this->painter->track_thickness) {
				this->painter->track_thickness = data.u.val_int;
				this->painter->make_track_pens();
			}
		}
		break;
	case PARAM_TRACK_BG_THICKNESS:
		if (data.u.val_int >= scale_track_bg_thickness.min && data.u.val_int <= scale_track_bg_thickness.max) {
			if (data.u.val_int != this->painter->track_bg_thickness) {
				this->painter->track_bg_thickness = data.u.val_int;
				this->painter->make_track_pens();
			}
		}
		break;

	case PARAM_TRK_BG_COLOR:
		this->painter->track_bg_color = data.val_color;
		this->painter->track_bg_pen.setColor(this->painter->track_bg_color);
		break;

	case PARAM_TRACK_DRAW_SPEED_FACTOR:
		if (data.u.val_double >= scale_track_draw_speed_factor.min && data.u.val_double <= scale_track_draw_speed_factor.max) {
			this->painter->track_draw_speed_factor = data.u.val_double;
		}
		break;
	case PARAM_TRACK_SORT_ORDER:
		if (data.u.val_int < (int) TreeViewSortOrder::Last) {
			this->track_sort_order = (TreeViewSortOrder) data.u.val_int;
		} else {
			qDebug() << SG_PREFIX_E << "Invalid Track Sort Order" << data.u.val_int;
		}
		break;
	case PARAM_WP_LABELS:
		this->painter->draw_wp_labels = data.u.val_bool;
		break;
	case PARAM_DRAW_WP_IMAGES:
		this->painter->draw_wp_images = data.u.val_bool;
		break;
	case PARAM_WP_IMAGE_SIZE:
		if (data.u.val_int >= scale_wp_image_size.min && data.u.val_int <= scale_wp_image_size.max) {
			if (data.u.val_int != this->painter->wp_image_size) {
				this->painter->wp_image_size = data.u.val_int;
				this->wp_image_cache_flush();
			}
		}
		break;
	case PARAM_WP_IMAGE_ALPHA:
		if (data.u.val_int >= scale_wp_image_alpha.min && data.u.val_int <= scale_wp_image_alpha.max) {
			if (data.u.val_int != this->painter->wp_image_alpha) {
				this->painter->wp_image_alpha = data.u.val_int;
				this->wp_image_cache_flush();
			}
		}
		break;

	case PARAM_WP_IMAGE_CACHE_CAPACITY:
		if (scale_wp_image_cache_capacity.is_in_range(data.u.val_int)) {
			qDebug() << SG_PREFIX_I << "Setting new capacity of in-memory waypoint image cache:" << data.u.val_int << "megabytes";
			this->wp_image_cache.set_capacity_megabytes(data.u.val_int);
		}
		break;

	case PARAM_WP_MARKER_COLOR:
		this->painter->wp_marker_color = data.val_color;
		this->painter->wp_marker_pen.setColor(this->painter->wp_marker_color);
		break;

	case PARAM_WP_LABEL_FG_COLOR:
		this->painter->wp_label_fg_color = data.val_color;
		this->painter->wp_label_fg_pen.setColor(this->painter->wp_label_fg_color);
		break;

	case PARAM_WP_LABEL_BG_COLOR:
		this->painter->wp_label_bg_color = data.val_color;
		this->painter->wp_label_bg_pen.setColor(this->painter->wp_label_bg_color);
		break;

	case PARAM_WPBA:
#ifdef K_FIXME_RESTORE
		this->wpbgand = (GdkFunction) data.b;
		if (this->waypoint_bg_gc) {
			gdk_gc_set_function(this->waypoint_bg_gc, data.b ? GDK_AND : GDK_COPY);
		}
#endif
		break;
	case PARAM_WP_MARKER_TYPE:
		if (data.u.val_int < (int) GraphicMarker::Max) {
			this->painter->wp_marker_type = (GraphicMarker) data.u.val_int;
		}
		break;
	case PARAM_WP_MARKER_SIZE:
		if (data.u.val_int >= scale_wp_marker_size.min && data.u.val_int <= scale_wp_marker_size.max) {
			this->painter->wp_marker_size = data.u.val_int;
		}
		break;
	case PARAM_DRAW_WP_SYMBOLS:
		this->painter->draw_wp_symbols = data.u.val_bool;
		break;
	case PARAM_WP_LABEL_FONT_SIZE:
		if (data.u.val_int < FS_NUM_SIZES) {
			this->painter->wp_label_font_size = (font_size_t) data.u.val_int;
		}
		break;
	case PARAM_WP_SORT_ORDER:
		if (data.u.val_int < (int) TreeViewSortOrder::Last) {
			this->wp_sort_order = (TreeViewSortOrder) data.u.val_int;
		} else {
			qDebug() << SG_PREFIX_E << "Invalid Waypoint Sort Order" << data.u.val_int;
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
			this->metadata->set_iso8601_timestamp(data.val_string);
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




SGVariant LayerTRW::get_param_value(param_id_t param_id, bool is_file_operation) const
{
	SGVariant rv;
	switch (param_id) {
	case PARAM_TRACKS_VISIBLE:          rv = SGVariant(this->tracks.visible);                            break;
	case PARAM_WAYPOINTS_VISIBLE:       rv = SGVariant(this->waypoints.visible);                         break;
	case PARAM_ROUTES_VISIBLE:          rv = SGVariant(this->routes.visible);                            break;
	case PARAM_DRAW_TRACK_LABELS:       rv = SGVariant(this->painter->draw_track_labels);                break;
	case PARAM_TRACK_LABEL_FONT_SIZE:   rv = SGVariant((int32_t) this->painter->track_label_font_size);  break;
	case PARAM_TRACK_DRAWING_MODE:      rv = SGVariant((int32_t) this->painter->track_drawing_mode);     break;
	case PARAM_TRACK_COLOR_COMMON:      rv = SGVariant(this->painter->track_color_common);               break;
	case PARAM_DRAW_TRACKPOINTS:        rv = SGVariant(this->painter->draw_trackpoints);                 break;
	case PARAM_TRACKPOINT_SIZE:         rv = SGVariant((int32_t) this->painter->trackpoint_size);        break;
	case PARAM_DRAW_TRACK_ELEVATION:    rv = SGVariant(this->painter->draw_track_elevation);             break;
	case PARAM_TRACK_ELEVATION_FACTOR:  rv = SGVariant((int32_t) this->painter->track_elevation_factor); break;
	case PARAM_DRAW_TRACK_STOPS:        rv = SGVariant(this->painter->draw_track_stops);                 break;
	case PARAM_TRACK_MIN_STOP_LENGTH:   rv = SGVariant((int32_t) this->painter->track_min_stop_length);  break;
	case PARAM_DRAW_TRACK_LINES:        rv = SGVariant(this->painter->draw_track_lines);                 break;
	case PARAM_DRAW_TRACK_DIRECTIONS:   rv = SGVariant(this->painter->draw_track_directions);            break;
	case PARAM_TRACK_DIRECTION_SIZE:    rv = SGVariant((int32_t) this->painter->draw_track_directions_size); break;
	case PARAM_TRACK_THICKNESS:         rv = SGVariant((int32_t) this->painter->track_thickness);        break;
	case PARAM_TRACK_BG_THICKNESS:      rv = SGVariant((int32_t) this->painter->track_bg_thickness);     break;
	case PARAM_WP_LABELS:               rv = SGVariant(this->painter->draw_wp_labels);                   break;
	case PARAM_TRK_BG_COLOR:            rv = SGVariant(this->painter->track_bg_color);                   break;
	case PARAM_TRACK_DRAW_SPEED_FACTOR: rv = SGVariant(this->painter->track_draw_speed_factor);          break;
	case PARAM_TRACK_SORT_ORDER:        rv = SGVariant((int32_t) this->track_sort_order);                break;

	case PARAM_DRAW_WP_IMAGES:          rv = SGVariant(this->painter->draw_wp_images);       break;
	case PARAM_WP_IMAGE_SIZE:           rv = SGVariant((int32_t) this->painter->wp_image_size);       break;
	case PARAM_WP_IMAGE_ALPHA:          rv = SGVariant((int32_t) this->painter->wp_image_alpha);      break;
	case PARAM_WP_IMAGE_CACHE_CAPACITY: rv = SGVariant((int32_t) this->wp_image_cache.get_capacity_megabytes()); break;

	case PARAM_WP_MARKER_COLOR:         rv = SGVariant(this->painter->wp_marker_color);      break;
	case PARAM_WP_LABEL_FG_COLOR:       rv = SGVariant(this->painter->wp_label_fg_color);    break;
	case PARAM_WP_LABEL_BG_COLOR:       rv = SGVariant(this->painter->wp_label_bg_color);    break;
	case PARAM_WPBA:                    rv = SGVariant(this->painter->wpbgand);              break;
	case PARAM_WP_MARKER_TYPE:          rv = SGVariant((int32_t) this->painter->wp_marker_type);      break;
	case PARAM_WP_MARKER_SIZE:          rv = SGVariant((int32_t) this->painter->wp_marker_size);      break;
	case PARAM_DRAW_WP_SYMBOLS:         rv = SGVariant(this->painter->draw_wp_symbols);               break;
	case PARAM_WP_LABEL_FONT_SIZE:      rv = SGVariant((int32_t) this->painter->wp_label_font_size);  break;
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
			rv = SGVariant(this->metadata->iso8601_timestamp);
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




#ifdef K_OLD_IMPLEMENTATION
void LayerTRWInterface::change_param(void * gtk_widget, void * ui_change_values)
{
	// This '-3' is to account for the first few parameters not in the properties
	const int OFFSET = -3;

	switch (values->param_id) {
		// Alter sensitivity of waypoint draw image related widgets according to the draw image setting.
	case PARAM_DRAW_WP_IMAGES: {
		// Get new value
		SGVariant var = a_uibuilder_widget_get_value(gtk_widget, values->param);
		GtkWidget **ww1 = values->widgets;
		GtkWidget **ww2 = values->labels;
		GtkWidget *w1 = ww1[OFFSET + PARAM_WP_IMAGE_SIZE];
		GtkWidget *w2 = ww2[OFFSET + PARAM_WP_IMAGE_SIZE];
		GtkWidget *w3 = ww1[OFFSET + PARAM_WP_IMAGE_ALPHA];
		GtkWidget *w4 = ww2[OFFSET + PARAM_WP_IMAGE_ALPHA];
		GtkWidget *w5 = ww1[OFFSET + PARAM_WP_IMAGE_CACHE_CAPACITY];
		GtkWidget *w6 = ww2[OFFSET + PARAM_WP_IMAGE_CACHE_CAPACITY];
		if (w1) w1->setEnabled(var.b);
		if (w2) w2->setEnabled(var.b);
		if (w3) w3->setEnabled(var.b);
		if (w4) w4->setEnabled(var.b);
		if (w5) w5->setEnabled(var.b);
		if (w6) w6->setEnabled(var.b);
		break;
	}
		// Alter sensitivity of waypoint label related widgets according to the draw label setting.
	case PARAM_WP_LABELS: {
		// Get new value
		SGVariant var = a_uibuilder_widget_get_value(gtk_widget, values->param);
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
		if (w1) w1->setEnabled(var.b);
		if (w2) w2->setEnabled(var.b);
		if (w3) w3->setEnabled(var.b);
		if (w4) w4->setEnabled(var.b);
		if (w5) w5->setEnabled(var.b);
		if (w6) w6->setEnabled(var.b);
		if (w7) w7->setEnabled(var.b);
		if (w8) w8->setEnabled(var.b);
		break;
	}
		// Alter sensitivity of all track colors according to the draw track mode.
	case PARAM_TRACK_DRAWING_MODE: {
		// Get new value
		SGVariant var = a_uibuilder_widget_get_value(gtk_widget, values->param);
		bool sensitive = (var.i == LayerTRWTrackDrawingMode::AllSameColor);
		GtkWidget **ww1 = values->widgets;
		GtkWidget **ww2 = values->labels;
		GtkWidget *w1 = ww1[OFFSET + PARAM_TRACK_COLOR_COMMON];
		GtkWidget *w2 = ww2[OFFSET + PARAM_TRACK_COLOR_COMMON];
		if (w1) w1->setEnabled(sensitive);
		if (w2) w2->setEnabled(sensitive);
		break;
	}
	case PARAM_MDTIME: {
		// Force metadata->timestamp to be always read-only for now.
		GtkWidget **ww = values->widgets;
		GtkWidget *w1 = ww[OFFSET + PARAM_MDTIME];
		if (w1) w1->setEnabled(false);
	}
		// NB Since other track settings have been split across tabs,
		// I don't think it's useful to set sensitivities on widgets you can't immediately see
	default: break;
	}
}
#endif






void LayerTRW::marshall(Pickle & pickle)
{
	/* Layer parameters go first. */
	this->marshall_params(pickle);


	Pickle helper_pickle;

	for (auto iter = this->waypoints.children_list.begin(); iter != this->waypoints.children_list.end(); iter++) {
		(*iter)->marshall(helper_pickle); /* TODO_2_LATER: the marshall() function needs to put sublayer type into helper_pickle. */
		if (helper_pickle.data_size() > 0) {
			pickle.put_pickle(helper_pickle);
		}
		helper_pickle.clear();
	}


	for (auto iter = this->tracks.children_list.begin(); iter != this->tracks.children_list.end(); iter++) {
		(*iter)->marshall(helper_pickle); /* TODO_2_LATER: the marshall() function needs to put sublayer type into helper_pickle. */
		if (helper_pickle.data_size() > 0) {
			pickle.put_pickle(helper_pickle);
		}
		helper_pickle.clear();
	}


	for (auto iter = this->routes.children_list.begin(); iter != this->routes.children_list.end(); iter++) {
		(*iter)->marshall(helper_pickle); /* TODO_2_LATER: the marshall() function needs to put sublayer type into helper_pickle. */
		if (helper_pickle.data_size() > 0) {
			pickle.put_pickle(helper_pickle);
		}
		helper_pickle.clear();
	}
}




Layer * LayerTRWInterface::unmarshall(Pickle & pickle, Viewport * viewport)
{
	LayerTRW * trw = new LayerTRW();
	trw->set_coord_mode(viewport->get_coord_mode());

	const pickle_size_t original_data_size = pickle.data_size();


	/* First the overall layer parameters. */
	pickle_size_t data_size = pickle.peek_size();
	trw->unmarshall_params(pickle);

	pickle_size_t consumed_length = data_size;
	const pickle_size_t sizeof_len_and_subtype = sizeof (pickle_size_t) + sizeof (int); /* Object size + object type. */

	// Now the individual sublayers:
	while (pickle.data_size() > 0 && consumed_length < original_data_size) {
		// Normally four extra bytes at the end of the datastream
		//  (since it's a GByteArray and that's where it's length is stored)
		//  So only attempt read when there's an actual block of sublayer data
		if (consumed_length + pickle.peek_size() < original_data_size) {

			const QString type_id = pickle.peek_string(sizeof (pickle_size_t)); /* Look at type id string that is after object size. */

			/* Also remember to (attempt to) convert each
			   coordinate in case this is pasted into a
			   different track_drawing_mode. */
			if (type_id == "sg.trw.track") {
				Track * trk = Track::unmarshall(pickle);
				/* Unmarshalling already sets track name, so we don't have to do it here. */
				trw->add_track(trk);
				trk->convert(trw->coord_mode);
			} else if (type_id == "sg.trw.waypoint") {
				Waypoint * wp = Waypoint::unmarshall(pickle);
				/* Unmarshalling already sets waypoint name, so we don't have to do it here. */
				trw->add_waypoint(wp);
				wp->convert(trw->coord_mode);
			} else if (type_id == "sg.trw.route") {
				Track * trk = Track::unmarshall(pickle);
				/* Unmarshalling already sets route name, so we don't have to do it here. */
				trw->add_route(trk);
				trk->convert(trw->coord_mode);
			} else {
				qDebug() << SG_PREFIX_E << "Invalid sublayer type id" << type_id;
			}
		}
		consumed_length += pickle.peek_size() + sizeof_len_and_subtype;
#ifdef TODO_2_LATER
		// See marshalling above for order of how this is written  // kamilkamil
		pickle.data += sizeof_len_and_subtype + pickle.peek_size();
#endif
	}
	//fprintf(stderr, "DEBUG: consumed_length %d vs len %d\n", consumed_length, original_data_size);

	// Not stored anywhere else so need to regenerate
	trw->get_waypoints_node().recalculate_bbox();

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




void LayerTRW::draw_tree_item(Viewport * viewport, bool highlight_selected, bool parent_is_selected)
{
	/* Check the layer for visibility (including all the parents' visibilities). */
	if (!this->tree_view->get_tree_item_visibility_with_parents(this)) {
		return;
	}

	const bool item_is_selected = parent_is_selected || g_tree->is_in_set_of_selected(this);

	/* This will copy viewport's parameters (size, coords, etc.)
	   to painter, so that painter knows whether, what and how to
	   paint. */
	this->painter->set_viewport(viewport);

	if (this->tracks.visible) {
		qDebug() << SG_PREFIX_I << "Calling function to draw tracks, highlight:" << highlight_selected << item_is_selected;
		this->tracks.draw_tree_item(viewport, highlight_selected, item_is_selected);
	}

	if (this->routes.visible) {
		qDebug() << SG_PREFIX_I << "Calling function to draw routes, highlight:" << highlight_selected << item_is_selected;
		this->routes.draw_tree_item(viewport, highlight_selected, item_is_selected);
	}

	if (this->waypoints.visible) {
		qDebug() << SG_PREFIX_I << "Calling function to draw waypoints, highlight:" << highlight_selected << item_is_selected;
		this->waypoints.draw_tree_item(viewport, highlight_selected, item_is_selected);
	}

	return;

}




sg_ret LayerTRW::attach_children_to_tree(void)
{
	qDebug() << SG_PREFIX_D;

	if (!this->is_in_tree()) {
		qDebug() << SG_PREFIX_E << "TRW Layer" << this->name << "is not connected to tree";
		return sg_ret::err;
	}

	if (this->tracks.size() > 0) {
		qDebug() << SG_PREFIX_D << "Handling Tracks node";

		if (!this->tracks.is_in_tree()) {
			qDebug() << SG_PREFIX_I << "Attaching to tree item" << this->tracks.name << "under" << this->name;
			this->tree_view->attach_to_tree(this, &this->tracks);
		}
		this->tracks.attach_children_to_tree();
	}

	if (this->routes.size() > 0) {
		qDebug() << SG_PREFIX_D << "Handling Routes node";

		if (!this->routes.is_in_tree()) {
			qDebug() << SG_PREFIX_I << "Attaching to tree item" << this->routes.name << "under" << this->name;
			this->tree_view->attach_to_tree(this, &this->routes);
		}

		this->routes.attach_children_to_tree();
	}

	if (this->waypoints.size() > 0) {
		qDebug() << SG_PREFIX_D << "Handling Waypoints node";

		if (!this->waypoints.is_in_tree()) {
			qDebug() << SG_PREFIX_I << "Attaching to tree item" << this->waypoints.name << "under" << this->name;
			this->tree_view->attach_to_tree(this, &this->waypoints);
			qDebug() << SG_PREFIX_I;
		}

		this->waypoints.attach_children_to_tree();
	}

	this->generate_missing_thumbnails();

	this->sort_all();

	return sg_ret::ok;
}




/*
 * Return a property about tracks for this layer.
 */
int LayerTRW::get_track_thickness()
{
	return this->painter->track_thickness;
}




/*
  Build up multiple routes information
*/
Distance LayerTRW::get_routes_tooltip_data(void) const
{
	Distance result;
	for (auto iter = this->routes.children_list.begin(); iter != this->routes.children_list.end(); iter++) {
		result += (*iter)->get_length();
	}

	return result;
}




/*
  Get information about layer's tracks
*/
LayerTRW::TracksTooltipData LayerTRW::get_tracks_tooltip_data(void) const
{
	TracksTooltipData result;

	for (auto iter = this->tracks.children_list.begin(); iter != this->tracks.children_list.end(); iter++) {

		Track * trk = *iter;

		result.length += trk->get_length();

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
		if (result.start_time == 0) {
			result.start_time = t1;
		}
		if (result.end_time == 0) {
			result.end_time = t2;
		}

		/* Update find the earliest / last times. */
		if (t1 < result.start_time) {
			result.start_time = t1;
		}

		if (t2 > result.end_time) {
			result.end_time = t2;
		}

		/* Keep track of total time.
		   There maybe gaps within a track (eg segments)
		   but this should be generally good enough for a simple indicator. */
		result.duration = result.duration + (int) (t2 - t1);
	}

	return result;
}




/*
  Generate tooltip text for the layer
*/
QString LayerTRW::get_tooltip(void) const
{
	QString tracks_info;
	if (!this->tracks.empty()) {
		TracksTooltipData ttd = this->get_tracks_tooltip_data();

		QDateTime date_start;
		date_start.setTime_t(ttd.start_time);

		QDateTime date_end;
		date_end.setTime_t(ttd.end_time);

		QString duration_string;

		if (date_start != date_end) { /* TODO_MAYBE: should we compare dates/times, or only dates? */
			/* Dates differ so print range on separate line. */
			duration_string = QObject::tr("%1 to %2\n").arg(date_start.toString(Qt::SystemLocaleLongDate)).arg(date_end.toString(Qt::SystemLocaleLongDate));
		} else {
			/* Same date so just show it and keep rest of text on the same line - provided it's a valid time! */
			if (ttd.start_time != 0) {
				duration_string = date_start.toString(Qt::SystemLocaleLongDate);
			}
		}

		if (ttd.length.is_valid()) {
			/* Prepare info string dependent on distance units. */
			const QString distance_string = ttd.length.convert_to_unit(Preferences::get_unit_distance()).to_nice_string();

			/* Use timing information if available. */
			if (ttd.duration > 0) {
				tracks_info = QObject::tr("\n%1Total Length %2 in %3 %4")
					.arg(duration_string)
					.arg(distance_string)
					.arg((int)(ttd.duration/3600))
					.arg((int) round(ttd.duration / 60.0) % 60, 2, 10, (QChar) '0');
			} else {
				tracks_info = QObject::tr("\n%1Total Length %2")
					.arg(duration_string)
					.arg(distance_string);
			}
		}
	}


	QString routes_info;
	if (!this->routes.empty()) {
		const Distance rlength = this->get_routes_tooltip_data(); /* [meters] */
		if (rlength.is_valid()) {
			/* Prepare track info dependent on distance units. */
			const QString distance_string = rlength.convert_to_unit(Preferences::get_unit_distance()).to_nice_string();
			routes_info = QObject::tr("\nTotal route length %1").arg(distance_string);
		}
	}


	/* Put together all the elements to form compact tooltip text. */
	const QString result = QObject::tr("Tracks: %1 - Waypoints: %2 - Routes: %3"
					   "%4"
					   "%5")
		.arg(this->tracks.size())
		.arg(this->waypoints.size())
		.arg(this->routes.size())
		.arg(tracks_info)
		.arg(routes_info);

	return result;
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
	this->get_window()->get_statusbar()->set_message(StatusBarField::Info, msg);
}




/*
 * Function to show basic waypoint information on the statusbar.
 */
void LayerTRW::set_statusbar_msg_info_wpt(Waypoint * wp)
{
	const HeightUnit height_unit = Preferences::get_unit_height();
	const QString alti_string_uu = QObject::tr("Wpt: Alt %1").arg(wp->altitude.convert_to_unit(height_unit).to_string());

	/* Position part.
	   Position is put last, as this bit is most likely not to be seen if the display is not big enough,
	   one can easily use the current pointer position to see this if needed. */
	const LatLon lat_lon = wp->coord.get_latlon();
	QString lat;
	QString lon;
	LatLon::to_strings(lat_lon, lat, lon);

	/* Combine parts to make overall message. */
	QString msg;
	if (!wp->comment.isEmpty()) {
		/* Add comment if available. */
		msg = tr("%1 | %2 %3 | Comment: %4").arg(alti_string_uu).arg(lat).arg(lon).arg(wp->comment);
	} else {
		msg = tr("%1 | %2 %3").arg(alti_string_uu).arg(lat).arg(lon);
	}
	this->get_window()->get_statusbar()->set_message(StatusBarField::Info, msg);
}




void LayerTRW::reset_internal_selections(void)
{
	this->reset_edited_track();
	this->reset_edited_wp();
	this->cancel_current_tp(false);
}




bool LayerTRW::is_empty(void) const
{
	return this->tracks.empty() && this->routes.empty() && this->waypoints.empty();
}




bool LayerTRW::get_tracks_visibility(void) const
{
	return this->tracks.visible;
}




bool LayerTRW::get_routes_visibility(void) const
{
	return this->routes.visible;
}




bool LayerTRW::get_waypoints_visibility(void) const
{
	return this->waypoints.visible;
}




void LayerTRW::recalculate_bbox(void)
{
	this->tracks.recalculate_bbox();
	this->routes.recalculate_bbox();
	this->waypoints.recalculate_bbox();
}




LatLonBBox LayerTRW::get_bbox(void)
{
	/* Continually expand result bbox to find union of bboxes of
	   three sublayers. */

	LatLonBBox result;
	LatLonBBox intermediate;

	intermediate = this->tracks.get_bbox();
	BBOX_EXPAND_WITH_BBOX(result, intermediate);

	intermediate = this->routes.get_bbox();
	BBOX_EXPAND_WITH_BBOX(result, intermediate);

	intermediate = this->waypoints.get_bbox();
	BBOX_EXPAND_WITH_BBOX(result, intermediate);

	return result;
}




bool LayerTRW::find_center(Coord * dest)
{
	if (this->get_bbox().is_valid()) {
		*dest = Coord(this->get_bbox().get_center(), this->coord_mode);
		return true;
	} else {
		return false;
	}
}




void LayerTRW::centerize_cb(void)
{
	Coord coord;
	if (this->find_center(&coord)) {
		Viewport * viewport = ThisApp::get_main_viewport();
		this->goto_coord(viewport, coord);
	} else {
		Dialog::info(tr("This layer has no waypoints or trackpoints."), this->get_window());
	}
}




bool LayerTRW::move_viewport_to_show_all(Viewport * viewport)
{
	if (this->get_bbox().is_valid()) {
		viewport->show_bbox(this->get_bbox());
		return true;
	} else {
		return false;
	}
}




void LayerTRW::move_viewport_to_show_all_cb(void) /* Slot. */
{
	if (this->is_empty()) {
		Dialog::info(tr("This layer has no tracks, routes or waypoints."), this->get_window());
		return;
	}

	if (this->move_viewport_to_show_all(ThisApp::get_main_viewport())) {
		qDebug() << SG_PREFIX_SIGNAL << "Will call 'emit_items_tree_updated_cb() for" << this->get_name();
		ThisApp::get_layers_panel()->emit_items_tree_updated_cb(this->get_name());
	}
}




void LayerTRW::export_as_gpspoint_cb(void) /* Slot. */
{
	const QString auto_save_name = append_file_ext(this->get_name(), SGFileType::GPSPoint);
	this->export_layer(tr("Export Layer"), auto_save_name, NULL, SGFileType::GPSPoint);
}




void LayerTRW::export_as_gpsmapper_cb(void) /* Slot. */
{
	const QString auto_save_name = append_file_ext(this->get_name(), SGFileType::GPSMapper);
	this->export_layer(tr("Export Layer"), auto_save_name, NULL, SGFileType::GPSMapper);
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
	const QString auto_save_name = append_file_ext(this->get_name(), SGFileType::GeoJSON);
	this->export_layer(tr("Export Layer"), auto_save_name, NULL, SGFileType::GeoJSON);
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
		Waypoint * wp = this->waypoints.find_waypoint_by_name(name_);

		if (!wp) {
			Dialog::error(tr("Waypoint not found in this layer."), this->get_window());
		} else {
			ThisApp::get_main_viewport()->set_center_from_coord(wp->coord, true);
			g_tree->emit_items_tree_updated();
			this->tree_view->select_and_expose_tree_item(wp);

			break;
		}

	}
}




bool LayerTRW::new_waypoint(const Coord & default_coord, Window * parent_window)
{
	/* Notice that we don't handle situation when returned default
	   name is invalid. The new name in properties dialog will
	   simply be empty. */
	const QString default_name = this->waypoints.name_generator.try_new_name();

	Waypoint * wp = new Waypoint();
	bool updated;
	wp->coord = default_coord;

	/* Attempt to auto set height if DEM data is available. */
	wp->apply_dem_data(true);

	const std::tuple<bool, bool> result = waypoint_properties_dialog(wp, default_name, this->coord_mode, parent_window);
	if (std::get<SG_WP_DIALOG_OK>(result)) {
		/* "OK" pressed in dialog, waypoint's parameters entered in the dialog are valid. */
		wp->visible = true;
		this->add_waypoint(wp);
		return true;
	} else {
		qDebug() << SG_PREFIX_I << "Failed to create new waypoint in dialog, rejecting";
		delete wp;
		return false;
	}
}




void LayerTRW::acquire_from_wikipedia_waypoints_viewport_cb(void) /* Slot. */
{
	Viewport * viewport = ThisApp::get_main_viewport();

	Geonames::create_wikipedia_waypoints(this, viewport->get_bbox(), this->get_window());
	this->waypoints.recalculate_bbox();
	g_tree->emit_items_tree_updated();
}




void LayerTRW::acquire_from_wikipedia_waypoints_layer_cb(void) /* Slot. */
{
	Geonames::create_wikipedia_waypoints(this, this->get_bbox(), this->get_window());
	this->waypoints.recalculate_bbox();
	g_tree->emit_items_tree_updated();
}




#ifdef VIK_CONFIG_GEOTAG
void LayerTRW::geotag_images_cb(void) /* Slot. */
{
	/* Set to true so that thumbnails are generate later if necessary. */
	this->has_missing_thumbnails = true;
	trw_layer_geotag_dialog(this->get_window(), this, NULL, NULL);
}
#endif




/* 'Acquires' - Same as in File Menu -> Acquire - applies into the selected TRW Layer */
void LayerTRW::acquire_handler(DataSource * data_source)
{
	this->get_window()->acquire_handler(data_source);
}




/*
 * Acquire into this TRW Layer straight from GPS Device.
 */
void LayerTRW::acquire_from_gps_cb(void)
{
	this->acquire_handler(new DataSourceGPS());
}




/*
 * Acquire into this TRW Layer from Directions.
 */
void LayerTRW::acquire_from_routing_cb(void) /* Slot. */
{
	this->acquire_handler(new DataSourceRouting());
}




/*
 * Acquire into this TRW Layer from an entered URL.
 */
void LayerTRW::acquire_from_url_cb(void) /* Slot. */
{
	this->acquire_handler(new DataSourceURL());
}




/*
 * Acquire into this TRW Layer from OSM.
 */
void LayerTRW::acquire_from_osm_cb(void) /* Slot. */
{
	this->acquire_handler(new DataSourceOSMTraces());
}




/**
 * Acquire into this TRW Layer from OSM for 'My' Traces.
 */
void LayerTRW::acquire_from_osm_my_traces_cb(void) /* Slot. */
{
	this->acquire_handler(new DataSourceOSMMyTraces());
}




#ifdef VIK_CONFIG_GEOCACHES
/*
 * Acquire into this TRW Layer from Geocaching.com
 */
void LayerTRW::acquire_from_geocache_cb(void) /* Slot. */
{
	this->acquire_handler(new DataSourceGeoCache(ThisApp::get_main_viewport()));
}
#endif




#ifdef VIK_CONFIG_GEOTAG
/*
 * Acquire into this TRW Layer from images.
 */
void LayerTRW::acquire_from_geotagged_images_cb(void) /* Slot. */
{
	this->acquire_handler(new DataSourceGeoTag());

	/* Re-generate thumbnails as they may have changed.
	   TODO_MAYBE: move this somewhere else, where we are sure that the acquisition has been completed? */
	this->has_missing_thumbnails = true;
	this->generate_missing_thumbnails();
}
#endif




/*
 * Acquire into this TRW Layer from any GPS Babel supported file.
 */
void LayerTRW::acquire_from_file_cb(void) /* Slot. */
{
	this->acquire_handler(new DataSourceFile());
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
	LayersPanel * panel = ThisApp::get_layers_panel();
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


	DatasourceGPSSetup gps_upload_setup(QObject::tr("Upload"), xfer_type, xfer_all);
	if (gps_upload_setup.exec() != QDialog::Accepted) {
		return;
	}

	/* When called from the viewport - work the corresponding layers panel: */
	if (!panel) {
		panel = ThisApp::get_layers_panel();
	}

	/* Apply settings to transfer to the GPS device. */
	vik_gps_comm(this,
		     trk,
		     GPSDirection::Up,
		     gps_upload_setup.get_protocol(),
		     gps_upload_setup.get_port(),
		     false,
		     ThisApp::get_main_viewport(),
		     panel,
		     gps_upload_setup.get_do_tracks(),
		     gps_upload_setup.get_do_routes(),
		     gps_upload_setup.get_do_waypoints(),
		     gps_upload_setup.get_do_turn_off());
}




void LayerTRW::new_waypoint_cb(void) /* Slot. */
{
	/* TODO_LATER longone: okay, if layer above (aggregate) is invisible but this->visible is true, this redraws for no reason.
	   Instead return true if you want to update. */
	if (this->new_waypoint(ThisApp::get_main_viewport()->get_center2()), this->get_window()) {
		this->waypoints.recalculate_bbox();
		if (this->visible) {
			g_tree->emit_items_tree_updated();
		}
	}
}




Track * LayerTRW::new_track_create_common(const QString & new_name)
{
	qDebug() << SG_PREFIX_I << "Track name" << new_name;

	Track * track = new Track(false);
	track->set_defaults();

	if (this->painter->track_drawing_mode == LayerTRWTrackDrawingMode::AllSameColor) {
		/* Create track with the preferred color from the layer properties. */
		track->color = this->painter->track_color_common;
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

		this->get_window()->activate_tool_by_id(LAYER_TRW_TOOL_CREATE_TRACK);
	}
}




Track * LayerTRW::new_route_create_common(const QString & new_name)
{
	Track * route = new Track(true);
	route->set_defaults();

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

		this->get_window()->activate_tool_by_id(LAYER_TRW_TOOL_CREATE_ROUTE);
	}
}





void LayerTRW::finish_track_cb(void) /* Slot. */
{
	this->reset_track_creation_in_progress();
	this->reset_edited_track();
	this->route_finder_started = false;
	this->emit_layer_changed("TRW - finish track");
}




void LayerTRW::finish_route_cb(void) /* Slot. */
{
	this->reset_route_creation_in_progress();
	this->reset_edited_track();
	this->route_finder_started = false;
	this->emit_layer_changed("TRW - finish route");
}




void LayerTRW::upload_to_osm_traces_cb(void) /* Slot. */
{
	OSMTraces::upload_trw_layer(this, NULL);
}




sg_ret LayerTRW::attach_to_container(Track * trk)
{
	if (trk->type_id == "sg.trw.route") {
		this->routes.attach_to_container(trk);
	} else {
		this->tracks.attach_to_container(trk);
	}
	return sg_ret::ok;
}




sg_ret LayerTRW::attach_to_container(Waypoint * wp)
{
	this->waypoints.attach_to_container(wp);
	return sg_ret::ok;
}




sg_ret LayerTRW::attach_to_tree(Waypoint * wp)
{
	if (!this->is_in_tree()) {
		qDebug() << SG_PREFIX_W << "This layer" << this->name << "is not connected to tree, will now connect it";
		ThisApp::get_layers_panel()->get_top_layer()->add_layer(this, true);
	}

	if (!this->waypoints.is_in_tree()) {
		qDebug() << SG_PREFIX_I << "Attaching to tree item" << this->waypoints.name << "under" << this->name;
		this->tree_view->attach_to_tree(this, &this->waypoints);
	}


	qDebug() << SG_PREFIX_I << "Attaching to tree item" << wp->name << "under" << this->waypoints.name;
	this->tree_view->attach_to_tree(&this->waypoints, wp); /* push item to the end of parent nodes. */

	/* Sort now as post_read is not called on a waypoint connected to tree. */
	/* TODO: when adding multiple waypoints (e.g. during acquire), sorting children here will make acquire take more time. */
	this->tree_view->sort_children(&this->waypoints, this->wp_sort_order);


	return sg_ret::ok;
}




sg_ret LayerTRW::add_waypoint(Waypoint * wp)
{
	this->attach_to_container(wp);
	this->attach_to_tree(wp);

	return sg_ret::ok;
}




sg_ret LayerTRW::attach_to_tree(Track * trk)
{
	if (!this->is_in_tree()) {
		qDebug() << SG_PREFIX_W << "This layer" << this->name << "is not connected to tree, will now connect it";
		ThisApp::get_layers_panel()->get_top_layer()->add_layer(this, true);
	}

	if (trk->type_id == "sg.trw.route") {
		if (!this->routes.is_in_tree()) {
			qDebug() << SG_PREFIX_I << "Attaching to tree item" << this->routes.name << "under" << this->name;
			this->tree_view->attach_to_tree(this, &this->routes);
		}

		/* Now we can proceed with adding a route to Routes node. */

		qDebug() << SG_PREFIX_I << "Attaching to tree item" << trk->name << "under" << this->routes.name;
		this->tree_view->attach_to_tree(&this->routes, trk);

		/* Sort now as post_read is not called on a route connected to tree. */
		this->tree_view->sort_children(&this->routes, this->track_sort_order);

		this->routes.update_tree_view(trk);

	} else {
		if (!this->tracks.is_in_tree()) {
			qDebug() << SG_PREFIX_I << "Attaching to tree item" << this->tracks.name << "under" << this->name;
			this->tree_view->attach_to_tree(this, &this->tracks);
		}


		Trackpoint * tp = trk->get_tp_first();
		if (tp && tp->has_timestamp) {
			trk->timestamp = tp->timestamp;
			trk->has_timestamp = true;
		}

		qDebug() << SG_PREFIX_I << "Attaching to tree item" << trk->name << "under" << this->tracks.name;
		this->tree_view->attach_to_tree(&this->tracks, trk); /* push item to the end of parent nodes. */

		/* Sort now as post_read is not called on a track connected to tree. */
		this->tree_view->sort_children(&this->tracks, this->track_sort_order);
		this->tracks.update_tree_view(trk);
	}

	return sg_ret::ok;
}




sg_ret LayerTRW::add_track(Track * trk)
{
	this->attach_to_container(trk);
	this->attach_to_tree(trk);

	return sg_ret::ok;
}




sg_ret LayerTRW::add_route(Track * trk)
{
	this->attach_to_container(trk);
	this->attach_to_tree(trk);

	return sg_ret::ok;
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
	for (auto iter = this->waypoints.children_list.begin(); iter != this->waypoints.children_list.end(); iter++) {
		Waypoint * wp = *iter;
		if (wp->symbol_name.isEmpty()) {
			continue;
		}

		/* This function is called to re-get waypoint's symbol
		   from GarminSymbols, with current "waypoint's symbol
		   size" setting.  The symbol is shown in viewport */
		const QString tmp_symbol_name = wp->symbol_name;
		wp->set_symbol(tmp_symbol_name);
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




void LayerTRW::add_waypoint_from_file(Waypoint * wp)
{
	/* No more uniqueness of name forced when loading from a file.
	   This now makes this function a little redunant as we just flow the parameters through. */

	this->add_waypoint(wp);
}




void LayerTRW::add_track_from_file(Track * trk)
{
	Track * curr_track = this->get_edited_track();

	if (this->route_finder_append && curr_track) {
		trk->remove_dup_points(); /* Make "double point" track work to undo. */

		/* Enforce end of current track equal to start of tr. */
		Trackpoint * cur_end = curr_track->get_tp_last();
		Trackpoint * new_start = trk->get_tp_first();
		if (cur_end && new_start) {
			if (cur_end->coord != new_start->coord) {
				curr_track->add_trackpoint(new Trackpoint(*cur_end), false);
			}
		}

		curr_track->steal_and_append_trackpoints(trk);
		trk->free();
		this->route_finder_append = false; /* This means we have added it. */
	} else {
		/* No more uniqueness of name forced when loading from a file. */
		if (trk->type_id == "sg.trw.route") {
			this->routes.attach_to_container(trk);
		} else {
			this->tracks.attach_to_container(trk);
		}

		if (this->route_finder_check_added_track) {
			trk->remove_dup_points(); /* Make "double point" track work to undo. */
			this->route_finder_added_track = trk;
		}
	}
}




sg_ret LayerTRW::drag_drop_request(TreeItem * tree_item, int row, int col)
{
	/* TODO: Reset layer timestamps in case they have now changed. */
	/* TODO: Recalculate bounds. */

	/* Handle item in old location. */
	{
		LayerTRW * trw = (LayerTRW *) tree_item->get_owning_layer();

		if (tree_item->type_id == "sg.trw.track") {
			trw->detach_from_container((Track *) tree_item);
		} else if (tree_item->type_id == "sg.trw.route") {
			trw->detach_from_container((Track *) tree_item);
		} else if (tree_item->type_id == "sg.trw.waypoint") {
			trw->detach_from_container((Waypoint *) tree_item);
		} else {
			qDebug() << SG_PREFIX_E << "Unexpected type id" << tree_item->type_id << "of item" << tree_item->name;
			return sg_ret::err;
		}

		/* Detaching of tree item from tree view will be handled by QT. */
	}

	/* Handle item in new location. */
	{
		/* We are using LayerTRW::attach_to_tree() here so
		   that the layer can create and attach
		   Tracks/Routes/Waypoints nodes if necessary. */

		if (tree_item->type_id == "sg.trw.track") {
			this->attach_to_container((Track *) tree_item);
			this->attach_to_tree((Track *) tree_item);

		} else if (tree_item->type_id == "sg.trw.route") {
			this->attach_to_container((Track *) tree_item);
			this->attach_to_tree((Track *) tree_item);

		} else if (tree_item->type_id == "sg.trw.waypoint") {
			this->attach_to_container((Waypoint *) tree_item);
			this->attach_to_tree((Waypoint *) tree_item);
		} else {
			qDebug() << SG_PREFIX_E << "Unexpected type id" << tree_item->type_id << "of item" << tree_item->name;
			return sg_ret::err;
		}
	}

	return sg_ret::ok;
}




sg_ret LayerTRW::dropped_item_is_acceptable(TreeItem * tree_item, bool * result) const
{
	if (NULL == result) {
		return sg_ret::err;
	}

	if (TreeItemType::Sublayer != tree_item->tree_item_type) {
		*result = false;
		return sg_ret::ok;
	}

	if ("sg.trw.track" == tree_item->type_id
	    || "sg.trw.route" == tree_item->type_id
	    || "sg.trw.waypoint" == tree_item->type_id) {
		*result = true;
		return sg_ret::ok;
	}

	*result = false;
	return sg_ret::ok;
}




sg_ret LayerTRW::detach_from_container(Track * trk, bool * was_visible)
{
	if (!trk) {
		qDebug() << SG_PREFIX_E << "NULL pointer to track";
		return sg_ret::err;
	}

	if (trk->type_id == "sg.trw.route") {
		if (sg_ret::ok != this->routes.detach_from_container(trk, was_visible)) {
			return sg_ret::err;
		}
	} else {
		if (sg_ret::ok != this->tracks.detach_from_container(trk, was_visible)) {
			return sg_ret::err;
		}
	}

	/* In case it was selected (no item delete signal ATM). */
	this->get_window()->clear_highlight();

	return sg_ret::ok;
}




sg_ret LayerTRW::detach_from_container(Waypoint * wp, bool * was_visible)
{
	if (!wp) {
		qDebug() << SG_PREFIX_E << "NULL pointer to waypoint";
		return sg_ret::err;
	}

	if (sg_ret::ok != this->waypoints.detach_from_container(wp, was_visible)) {
		return sg_ret::err;
	}

	/* In case it was selected (no item delete signal ATM). */
	this->get_window()->clear_highlight();

	return sg_ret::ok;
}




sg_ret LayerTRW::detach_from_tree(TreeItem * tree_item)
{
	if (!tree_item) {
		qDebug() << SG_PREFIX_E << "NULL pointer to tree item";
		return sg_ret::err;
	}

	this->tree_view->detach_tree_item(tree_item);

	/* If last sublayer of given type, then remove sublayer container.
	   TODO: this sometimes doesn't work. */
	if (tree_item->type_id == "sg.trw.track") {
		if (this->tracks.size() == 0) {
			this->tree_view->detach_tree_item(&this->tracks);
		}
	} else if (tree_item->type_id == "sg.trw.route") {
		if (this->routes.size() == 0) {
			this->tree_view->detach_tree_item(&this->routes);
		}
	} else if (tree_item->type_id == "sg.trw.waypoint") {
		if (this->waypoints.size() == 0) {
			this->tree_view->detach_tree_item(&this->waypoints);
		}
	} else {
		qDebug() << SG_PREFIX_E << "Unexpected tree item type" << tree_item->type_id;
		return sg_ret::err;
	}

	return sg_ret::ok;
}




void LayerTRW::delete_all_routes()
{
	this->route_finder_added_track = NULL;
	if (this->get_edited_track()) {
		this->cancel_current_tp(false);
		this->reset_edited_track();
	}

	for (auto iter = this->routes.children_list.begin(); iter != this->routes.children_list.end(); iter++) {
		this->tree_view->detach_tree_item(*iter);
	}
	this->tree_view->detach_tree_item(&this->routes);
	this->routes.visible = false; /* There is no such item in tree anymore. */

	/* Don't try (for now) to verify if ->selected_tree_item was
	   set to this item or any of its children, or to anything
	   else. Just clear the selection when deleting any item from
	   the tree. */
	g_tree->selected_tree_items.clear();

	this->routes.clear();

	this->emit_layer_changed("TRW - delete all tracks");
}




void LayerTRW::delete_all_tracks()
{
	this->route_finder_added_track = NULL;
	if (this->get_edited_track()) {
		this->cancel_current_tp(false);
		this->reset_edited_track();
	}

	for (auto iter = this->tracks.children_list.begin(); iter != this->tracks.children_list.end(); iter++) {
		this->tree_view->detach_tree_item(*iter);
	}
	this->tree_view->detach_tree_item(&this->tracks);
	this->tracks.visible = false; /* There is no such item in tree anymore. */

	/* Don't try (for now) to verify if ->selected_tree_item was
	   set to this item or any of its children, or to anything
	   else. Just clear the selection when deleting any item from
	   the tree. */
	g_tree->selected_tree_items.clear();

	this->tracks.clear();

	this->emit_layer_changed("TRW - delete all routes");
}




void LayerTRW::delete_all_waypoints()
{
	this->reset_edited_wp();
	this->moving_wp = false;

	this->waypoints.name_generator.reset();

	for (auto iter = this->waypoints.children_list.begin(); iter != this->waypoints.children_list.end(); iter++) {
		this->tree_view->detach_tree_item(*iter);
	}
	this->tree_view->detach_tree_item(&this->waypoints);
	this->waypoints.visible = false; /* There is no such item in tree anymore. */

	/* Don't try (for now) to verify if ->selected_tree_item was
	   set to this item or any of its children, or to anything
	   else. Just clear the selection when deleting any item from
	   the tree. */
	g_tree->selected_tree_items.clear();

	this->waypoints.clear();

	this->emit_layer_changed("TRW - delete all waypoints");
}




void LayerTRW::delete_all_tracks_cb(void) /* Slot. */
{
	/* Get confirmation from the user. */
	if (Dialog::yes_or_no(tr("Are you sure you want to delete all tracks in \"%1\"?").arg(this->get_name()), this->get_window())) {
		this->delete_all_tracks();
	}
}




void LayerTRW::delete_all_routes_cb(void) /* Slot. */
{
	/* Get confirmation from the user. */
	if (Dialog::yes_or_no(tr("Are you sure you want to delete all routes in \"%1\"?").arg(this->get_name()), this->get_window())) {
		this->delete_all_routes();
	}
}




void LayerTRW::delete_all_waypoints_cb(void) /* Slot. */
{
	/* Get confirmation from the user. */
	if (Dialog::yes_or_no(tr("Are you sure you want to delete all waypoints in \"%1\"?").arg(this->get_name()), this->get_window())) {
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
		qDebug() << SG_PREFIX_E << "Unexpected sublayer type" << item->type_id;
	}
}




void LayerTRW::goto_coord(Viewport * viewport, const Coord & coord)
{
	if (viewport) {
		viewport->set_center_from_coord(coord, true);
		this->emit_layer_changed("TRW - goto coord");
	}
}




void LayerTRW::extend_track_end_cb(void)
{
	Track * track = this->get_edited_track();
	if (!track) {
		return;
	}

	this->get_window()->activate_tool_by_id(track->type_id == "sg.trw.route" ? LAYER_TRW_TOOL_CREATE_ROUTE : LAYER_TRW_TOOL_CREATE_TRACK);

	if (!track->empty()) {
		Viewport * viewport = ThisApp::get_main_viewport();
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

	this->get_window()->activate_tool_by_id(LAYER_TRW_TOOL_ROUTE_FINDER);

	this->route_finder_started = true;

	if (!track->empty()) {
		Viewport * viewport = ThisApp::get_main_viewport();
		this->goto_coord(viewport, track->get_tp_last()->coord);
	}
}




/**
 * Commonal helper function.
 */
void LayerTRW::wp_changed_message(int n_changed)
{
	const QString msg = QObject::tr("%n waypoints changed", "", n_changed);
	Dialog::info(msg, this->get_window());
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
#ifdef K_OLD_IMPLEMENTATION
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
 * Attempt to merge selected track with other tracks specified by the user
 * Tracks to merge with must be of the same 'type' as the selected track -
 *  either all with timestamps, or all without timestamps
 */
void LayerTRW::merge_with_other_cb(void)
{
	Track * track = this->get_edited_track();
	if (!track) {
		qDebug() << SG_PREFIX_E << "Can't get edited track in track-related function" << __FUNCTION__;
		return;
	}
	if (track->empty()) {
		return;
	}

	const bool is_route = track->type_id == "sg.trw.route";

	LayerTRWTracks & source_sublayer = is_route ? this->routes : this->tracks;

	/* with_timestamps: allow merging with 'similar' time type time tracks
	   i.e. either those times, or those without */
	bool with_timestamps = track->get_tp_first()->has_timestamp;
	std::list<Track *> merge_candidates = source_sublayer.find_tracks_with_timestamp_type(with_timestamps, track);

	if (merge_candidates.empty()) {
		if (with_timestamps) {
			Dialog::error(tr("Failed. No other tracks with timestamps in this layer found"), this->get_window());
		} else {
			Dialog::error(tr("Failed. No other tracks without timestamps in this layer found"), this->get_window());
		}
		return;
	}
	/* Sort alphabetically for user presentation. */
	merge_candidates.sort(TreeItem::compare_name_ascending);

	BasicDialog dialog(is_route ? tr("Select route to merge with") : tr("Select track to merge with"), this->get_window());
	std::list<Track *> merge_list = a_dialog_select_from_list(dialog, merge_candidates, ListSelectionMode::MultipleItems, ListSelectionWidget::get_headers_for_track());

	if (merge_list.empty()) {
		qDebug() << SG_PREFIX_I << "Merge track is empty";
		return;
	}

	for (auto iter = merge_list.begin(); iter != merge_list.end(); iter++) {
		Track * source_track = *iter;

		if (source_track) {
			qDebug() << SG_PREFIX_I << "We have a merge track";
			track->steal_and_append_trackpoints(source_track);

			this->detach_from_container(source_track);
			this->detach_from_tree(source_track);
			delete source_track;

			track->sort(Trackpoint::compare_timestamps);
		}
	}
	this->emit_layer_changed("TRW - merge with other");
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
		qDebug() << SG_PREFIX_E << "Can't get edited track in track-related function";
		return;
	}

	const bool is_route = track->type_id == "sg.trw.route";

	LayerTRWTracks & source_sublayer = is_route ? this->routes : this->tracks;

	/* Get list of tracks for usage with list selection dialog function.
	   The dialog function will present tracks in a manner allowing differentiating between tracks with the same name. */
	const std::list<Track *> source_tracks = source_sublayer.get_sorted_by_name(track);

	/* Note the limit to selecting one track only.
	   This is to control the ordering of appending tracks, i.e. the selected track always goes after the current track
	   (otherwise with multiple select the ordering would not be controllable by the user - automatically being alphabetically). */
	BasicDialog dialog(is_route ? tr("Select the route to append after the current route") : tr("Select the track to append after the current track"), this->get_window());
	std::list<Track *> sources_list = a_dialog_select_from_list(dialog, source_tracks, ListSelectionMode::SingleItem, ListSelectionWidget::get_headers_for_track());
	/* It's a list, but shouldn't contain more than one other track! */
	if (sources_list.empty()) {
		return;
	}
	/* Because we have used ListSelectionMode::SingleItem selection
	   mode, this list can't have more than one element. */
	assert (sources_list.size() <= 1);


	Track * source_track = *sources_list.begin();
	if (!source_track) {
		qDebug() << SG_PREFIX_E << "Pointer to source track from tracks container is NULL";
		return;
	}


	track->steal_and_append_trackpoints(source_track);


	/* All trackpoints have been moved from source_track to
	   target_track. We don't need source_track anymore. */
	this->detach_from_container(source_track);
	this->detach_from_tree(source_track);
	delete source_track;


	this->emit_layer_changed("TRW - append track");
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
		qDebug() << SG_PREFIX_E << "Can't get edited track in track-related function" << __FUNCTION__;
		return;
	}

	const bool target_is_route = track->type_id == "sg.trw.route";

	/* We want to append a track of the *other* type, so use appropriate sublayer for this. */
	LayerTRWTracks & source_sublayer = target_is_route ? this->tracks : this->routes;

	/* Get list of names for usage with list selection dialog function.
	   The dialog function will present tracks in a manner allowing differentiating between tracks with the same name. */
	const std::list<Track *> source_tracks = source_sublayer.get_sorted_by_name(track);

	/* Note the limit to selecting one track only.
	   this is to control the ordering of appending tracks, i.e. the selected track always goes after the current track
	   (otherwise with multiple select the ordering would not be controllable by the user - automatically being alphabetically). */
	BasicDialog dialog(target_is_route ? tr("Select the track to append after the current route") : tr("Select the route to append after the current track"), this->get_window());
	std::list<Track *> sources_list = a_dialog_select_from_list(dialog, source_tracks, ListSelectionMode::SingleItem, ListSelectionWidget::get_headers_for_track());
	if (sources_list.empty()) {
		return;
	}
	/* Because we have used ListSelectionMode::SingleItem selection
	   mode, this list can't have more than one element. */
	assert (sources_list.size() <= 1);


	Track * source_track = *sources_list.begin();
	if (!source_track) {
		/* Very unlikely, but to be sure... */
		qDebug() << SG_PREFIX_E << "Pointer to source track from tracks container is NULL";
		return;
	}


	if (source_track->type_id != "sg.trw.route") {
		const Speed avg = source_track->get_average_speed();
		if (source_track->get_segment_count() > 1
		    || (avg.is_valid() && avg.get_value() > 0.0)) {

			if (Dialog::yes_or_no(tr("Converting a track to a route removes extra track data such as segments, timestamps, etc...\nDo you want to continue?"), this->get_window())) {
				source_track->merge_segments();
				source_track->to_routepoints();
			} else {
				return;
			}
		}
	}


	track->steal_and_append_trackpoints(source_track);


	/* All trackpoints have been moved from
	   source_track to target_track. We don't need
	   source_track anymore. */
	this->detach_from_container(source_track);
	this->detach_from_tree(source_track);
	delete source_track;


	this->emit_layer_changed("TRW - append other");
}




/* Merge by segments. */
void LayerTRW::merge_by_segment_cb(void)
{
	Track * track = this->get_edited_track();
	if (!track) {
		qDebug() << SG_PREFIX_E << "Can't get edited track in track-related function" << __FUNCTION__;
		return;
	}

	/* Currently no need to redraw as segments not actually shown on the display.
	   However inform the user of what happened. */
	const unsigned int n_segments = track->merge_segments();
	const QString msg = QObject::tr("%n segments merged", "", n_segments); /* TODO_2_LATER: verify that "%n" format correctly handles unsigned int. */
	Dialog::info(msg, this->get_window());
}




/* merge by time routine */
void LayerTRW::merge_by_timestamp_cb(void)
{
	Track * orig_track = this->get_edited_track();
	if (!orig_track) {
		qDebug() << SG_PREFIX_E << "Can't get edited track in track-related function" << __FUNCTION__;
		return;
	}

	if (!orig_track->empty()
	    && !orig_track->get_tp_first()->has_timestamp) {
		Dialog::error(tr("Failed. This track does not have timestamp"), this->get_window());
		return;
	}

	std::list<Track *> tracks_with_timestamp = this->tracks.find_tracks_with_timestamp_type(true, orig_track);
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

		/* TODO_LATER: why call this here? Shouldn't we call this way earlier? */
		if (orig_track->empty()) {
			return;
		}

		/* Get a list of adjacent-in-time tracks. */
		std::list<Track *> nearby_tracks = this->tracks.find_nearby_tracks_by_time(orig_track, (threshold_in_minutes * 60));

		/* Merge them. */

		for (auto iter = nearby_tracks.begin(); iter != nearby_tracks.end(); iter++) {
			/* Remove trackpoints from merged track, delete track. */
			orig_track->steal_and_append_trackpoints(*iter);

			this->detach_from_container(*iter);
			this->detach_from_tree(*iter);
			delete *iter;

			/* Tracks have changed, therefore retry again against all the remaining tracks. */
			attempt_merge = true;
		}

		orig_track->sort(Trackpoint::compare_timestamps);
	}

	this->emit_layer_changed("TRW merge by timestamp");
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
	}

	/* Remove original track and then update the display. */
	this->detach_from_container(orig);
	this->detach_from_tree(orig);
	delete orig;

	this->emit_layer_changed("TRW create new tracks");

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

	Track * new_track = selected_track->split_at_trackpoint(selected_track->selected_tp_iter);
	if (new_track) {
		this->set_edited_track(new_track, new_track->begin());
		this->add_track(new_track);
		this->emit_layer_changed("TRW - split at trackpoint");
	}
}




/* end of split/merge routines */




void LayerTRW::delete_selected_tp(Track * track)
{
	TrackPoints::iterator new_tp_iter = track->delete_trackpoint(track->selected_tp_iter.iter);

	if (new_tp_iter != track->end()) {
		/* Set to current to the available adjacent trackpoint. */
		track->selected_tp_iter.iter = new_tp_iter;
		track->recalculate_bbox();
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
		qDebug() << SG_PREFIX_E << "Can't get selected track in track callback" << __FUNCTION__;
		return;
	}


	if (!selected_track->selected_tp_iter.valid) {
		return;
	}

	this->delete_selected_tp(selected_track);

	/* Track has been updated so update tps: */
	this->cancel_tps_of_track(selected_track);

	this->emit_layer_changed("TRW - delete point selected");
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

	unsigned long n_removed = track->remove_dup_points();

	/* Track has been updated so update tps: */
	this->cancel_tps_of_track(track);

	/* Inform user how much was deleted as it's not obvious from the normal view. */
	const QString msg = QObject::tr("Deleted %n points", "", n_removed); /* TODO_2_LATER: verify that "%n" format correctly handles unsigned long. */
	Dialog::info(msg, this->get_window());

	this->emit_layer_changed("TRW - delete points same pos");
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

	unsigned long n_removed = track->remove_same_time_points();

	/* Track has been updated so update tps: */
	this->cancel_tps_of_track(track);

	/* Inform user how much was deleted as it's not obvious from the normal view. */
	const QString msg = QObject::tr("Deleted %n points", "", n_removed); /* TODO_2_LATER: verify that "%n" format correctly handles unsigned long. */
	Dialog::info(msg, this->get_window());

	this->emit_layer_changed("TRW - delete points same time");
}




/**
 * Insert a point
 */
void LayerTRW::insert_point_after_cb(void)
{
	Track * selected_track = this->get_edited_track();
	if (!selected_track) {
		qDebug() << SG_PREFIX_E << "Can't get selected track in track callback" << __FUNCTION__;
		return;
	}

	selected_track->create_tp_next_to_reference_tp(&selected_track->selected_tp_iter, false);

	this->emit_layer_changed("TRW - insert point after");
}




void LayerTRW::insert_point_before_cb(void)
{
	Track * selected_track = this->get_edited_track();
	if (!selected_track) {
		qDebug() << SG_PREFIX_E << "Can't get selected track in track callback" << __FUNCTION__;
		return;
	}

	selected_track->create_tp_next_to_reference_tp(&selected_track->selected_tp_iter, true);

	this->emit_layer_changed("TRW - insert point before");
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
	const QString cmd = QString("%1 %2%3").arg(diary_program).arg("--date=").arg(date_str);
	if (!g_spawn_command_line_async(cmd.toUtf8().constData(), &err)) {
		Dialog::error(tr("Could not launch %1 to open file.").arg(diary_program), this->get_window());
		g_error_free(err);
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
	char * ini_file_path;
	int fd = g_file_open_tmp("vik-astro-XXXXXX.ini", &ini_file_path, &err);
	if (fd < 0) {
		fprintf(stderr, "WARNING: %s: Failed to open temporary file: %s\n", __FUNCTION__, err->message);
		g_clear_error(&err);
		return;
	}
	const QString cmd = QString("%1 -c %2 --full-screen no --sky-date %3 --sky-time %4 --latitude %5 --longitude %6 --altitude %7")
		.arg(astro_program)
		.arg(ini_file_path)
		.arg(date_str)
		.arg(time_str)
		.arg(lat_str)
		.arg(lon_str)
		.arg(alt_str);

	qDebug() << SG_PREFIX_I << "Command is " << cmd;

	if (!g_spawn_command_line_async(cmd.toUtf8().constData(), &err)) {
		Dialog::error(tr("Could not launch %1").arg(astro_program), this->get_window());
		fprintf(stderr, "WARNING: %s\n", err->message);
		g_error_free(err);
	}
	Util::add_to_deletion_list(QString(ini_file_path));
	free(ini_file_path);
}




/*
  TODO_MAYBE: move to common file, e.g. to astro.cpp.

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
	/* Usage of a_dialog_select_from_list() will ensure that even
	   if each item has the same name, amount of information about
	   the item displayed in the list dialog will be enough to
	   uniquely identify and distinguish each item. */

	/* Sort list alphabetically for better presentation. */
	const std::list<Track *> all_tracks = this->tracks.get_sorted_by_name();

	if (all_tracks.empty()) {
		Dialog::error(tr("No tracks found"), this->get_window());
		return;
	}

	/* Get list of items to delete from the user. */
	BasicDialog dialog(tr("Select tracks to delete"), this->get_window());
	std::list<Track *> delete_list = a_dialog_select_from_list(dialog, all_tracks, ListSelectionMode::MultipleItems, ListSelectionWidget::get_headers_for_track());

	if (delete_list.empty()) {
		return;
	}

	/* Delete requested tracks.
	   Since specificly requested, IMHO no need for extra confirmation. */

	for (auto iter = delete_list.begin(); iter != delete_list.end(); iter++) {
		this->detach_from_container(*iter);
		this->detach_from_tree(*iter);
		delete *iter;
	}

	/* Reset layer timestamps in case they have now changed. */
	this->tree_view->apply_tree_item_timestamp(this);

	this->emit_layer_changed("TRW - delete selected tracks");
}




void LayerTRW::delete_selected_routes_cb(void) /* Slot. */
{
	/* Usage of a_dialog_select_from_list() will ensure that even
	   if each item has the same name, amount of information about
	   the item displayed in the list dialog will be enough to
	   uniquely identify and distinguish each item. */

	/* Sort list alphabetically for better presentation. */
	const std::list<Track *> all_routes = this->routes.get_sorted_by_name();

	if (all_routes.empty()) {
		Dialog::error(tr("No routes found"), this->get_window());
		return;
	}

	/* Get list of items to delete from the user. */

	BasicDialog dialog(tr("Select routes to delete"), this->get_window());
	std::list<Track *> delete_list = a_dialog_select_from_list(dialog, all_routes, ListSelectionMode::MultipleItems, ListSelectionWidget::get_headers_for_track());

	/* Delete requested routes.
	   Since specifically requested, IMHO no need for extra confirmation. */
	if (delete_list.empty()) {
		return;
	}

	for (auto iter = delete_list.begin(); iter != delete_list.end(); iter++) {
		this->detach_from_container(*iter);
		this->detach_from_tree(*iter);
		delete *iter;
	}

	this->emit_layer_changed("TRW - delete selected routes");
}




void LayerTRW::delete_selected_waypoints_cb(void)
{
	/* Usage of a_dialog_select_from_list() will ensure that even
	   if each item has the same name, amount of information about
	   the item displayed in the list dialog will be enough to
	   uniquely identify and distinguish each item. */

	/* Sort list alphabetically for better presentation. */
	std::list<Waypoint *> all_waypoints = this->waypoints.get_sorted_by_name();
	if (all_waypoints.empty()) {
		Dialog::error(tr("No waypoints found"), this->get_window());
		return;
	}

	/* Get list of items to delete from the user. */
	BasicDialog dialog(tr("Select waypoints to delete"), this->get_window());
	std::list<Waypoint *> delete_list = a_dialog_select_from_list(dialog, all_waypoints, ListSelectionMode::MultipleItems, ListSelectionWidget::get_headers_for_waypoint());

	if (delete_list.empty()) {
		return;
	}

	/* Delete requested waypoints.
	   Since specifically requested, IMHO no need for extra confirmation. */
	for (auto iter = delete_list.begin(); iter != delete_list.end(); iter++) {
		/* This deletes first waypoint it finds of that name (but uniqueness is enforced above). */
		this->detach_from_container(*iter);
		this->detach_from_tree(*iter);
		delete *iter;
	}

	this->waypoints.recalculate_bbox();
	/* Reset layer timestamp in case it has now changed. */
	this->tree_view->apply_tree_item_timestamp(this);
	this->emit_layer_changed("TRW - delete selected waypoints");

}




/**
   \brief Create the latest list of waypoints in this layer

   Function returns freshly allocated container. The container itself is owned by caller. Elements in container are not.
*/
void LayerTRW::get_waypoints_list(std::list<SlavGPS::Waypoint *> & list)
{
	for (auto iter = this->waypoints.children_list.begin(); iter != this->waypoints.children_list.end(); iter++) {
		list.push_back(*iter);
	}
}




/**
   Fill the list with tracks and/or routes from the layer.
*/
void LayerTRW::get_tracks_list(std::list<Track *> & list, const QString & type_id_string)
{
	if (type_id_string == "" || type_id_string == "sg.trw.tracks") {
		this->tracks.get_tracks_list(list);
	}

	if (type_id_string == "" || type_id_string == "sg.trw.routes") {
		this->routes.get_tracks_list(list);
	}
}




void LayerTRW::tracks_stats_cb(void)
{
	layer_trw_show_stats(this->name, this, "sg.trw.tracks", this->get_window());
}




void LayerTRW::routes_stats_cb(void)
{
	layer_trw_show_stats(this->name, this, "sg.trw.routes", this->get_window());
}




bool SlavGPS::is_valid_geocache_name(const char * str)
{
	const size_t len = strlen(str);
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
	if (track && track->selected_tp_iter.valid) {
		track->selected_tp_iter.valid = false;
		this->reset_edited_track();

		this->emit_layer_changed("TRW - cancel current tp");
	}
}




void LayerTRW::tpwin_update_dialog_data()
{
	Track * track = this->get_edited_track();
	if (track) {
		/* Notional center of a track is simply an average of its bounding box extremities. */
		const LatLon ll_center = track->bbox.get_center_coordinate(); /* TODO_MAYBE: this variable is unused. */
		this->tpwin->set_dialog_data(track, track->selected_tp_iter.iter, track->type_id == "sg.trw.route");
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
		if (!track->selected_tp_iter.valid) {
			return;
		}
		if (track->selected_tp_iter.iter == track->begin()) {
			/* Can't split track at first trackpoint in track. */
			break;
		}
		if (std::next(track->selected_tp_iter.iter) == track->end()) {
			/* Can't split track at last trackpoint in track. */
			break;
		}

		Track * new_track = track->split_at_trackpoint(track->selected_tp_iter);
		if (new_track) {
			this->set_edited_track(new_track, new_track->begin());
			this->add_track(new_track);
			this->emit_layer_changed("TRW - trackpoint properties - split");
		}

		this->tpwin_update_dialog_data();
		}
		break;

	case SG_TRACK_DELETE_CURRENT_TP:
		if (!track) {
			return;
		}
		if (!track->selected_tp_iter.valid) {
			return;
		}
		this->delete_selected_tp(track);

		if (track->selected_tp_iter.valid) {
			/* Update Trackpoint Properties with the available adjacent trackpoint. */
			this->tpwin_update_dialog_data();
		}

		this->emit_layer_changed("TRW - trackpoint properties - delete current");
		break;

	case SG_TRACK_GO_FORWARD:
		if (!track) {
			break;
		}
		if (!track->selected_tp_iter.valid) {
			return;
		}
		if (std::next(track->selected_tp_iter.iter) == track->end()) {
			/* Can't go forward if we are already at the end. */
			break;
		}

		track->selected_tp_iter.iter++;
		this->tpwin_update_dialog_data();
		this->emit_layer_changed("TRW - trackpoint properties - go forward"); /* TODO_LATER longone: either move or only update if tp is inside drawing window */

		break;

	case SG_TRACK_GO_BACK:
		if (!track) {
			break;
		}
		if (!track->selected_tp_iter.valid) {
			return;
		}
		if (track->selected_tp_iter.iter == track->begin()) {
			/* Can't go back if we are already at the beginning. */
			break;
		}

		track->selected_tp_iter.iter--;
		this->tpwin_update_dialog_data();
		this->emit_layer_changed("TRW - trackpoint properties - go back");

		break;

	case SG_TRACK_INSERT_TP_AFTER:
		if (!track) {
			break;
		}
		if (!track->selected_tp_iter.valid) {
			return;
		}
		if (std::next(track->selected_tp_iter.iter) == track->end()) {
			/* Can't inset trackpoint after last
			   trackpoint in track.  This is because the
			   algorithm for inserting a new trackpoint
			   uses two existing trackpoints and adds the
			   new one between the two existing tps. */
			break;
		}

		track->create_tp_next_to_reference_tp(&track->selected_tp_iter, false);
		this->emit_layer_changed("TRW - trackpoint properties - insert after");
		break;

	case SG_TRACK_CHANGED:
		this->emit_layer_changed("TRW - trackpoint properties - track changed");
		break;

	default:
		qDebug() << SG_PREFIX_E << "Unexpected dialog response" << response;
	}
}




void LayerTRW::trackpoint_properties_show()
{
	if (!this->tpwin) {
		this->tpwin = new PropertiesDialogTP(this->get_window());
		connect(this->tpwin, SIGNAL (trackpoint_coordinates_changed(int)), this, SLOT (trackpoint_properties_cb(int)));
		connect(this->tpwin->signal_mapper, SIGNAL (mapped(int)), this, SLOT (trackpoint_properties_cb(int)));

		//QObject::connect(this->tpwin, SIGNAL("delete-event"), this, SLOT (trw_layer_cancel_current_tp_cb));
	}
	this->tpwin->show();


	Track * track = this->get_edited_track();
	if (track && track->selected_tp_iter.valid) {
		/* Get tp pixel position. */
		Trackpoint * tp = *track->selected_tp_iter.iter;

		/* Shift up/down to try not to obscure the trackpoint. */
		const GlobalPoint point_to_expose = SGUtils::coord_to_global_point(tp->coord, ThisApp::get_main_viewport());
		Dialog::move_dialog(this->tpwin, point_to_expose, true);

		this->tpwin_update_dialog_data();
	}
	/* Set layer name and TP data. */
}




/* Structure for thumbnail creating data used in the background thread. */
class ThumbnailCreator : public BackgroundJob {
public:
	ThumbnailCreator(LayerTRW * layer, const QStringList & original_image_files_paths);

	void run(void);

	LayerTRW * layer = NULL;  /* Layer needed for redrawing. */
	QStringList original_image_files_paths;
};




ThumbnailCreator::ThumbnailCreator(LayerTRW * new_layer, const QStringList & new_original_image_files_paths)
{
	this->n_items = new_original_image_files_paths.size();

	this->layer = new_layer;
	this->original_image_files_paths = new_original_image_files_paths;
}




void ThumbnailCreator::run(void)
{
	const int n = this->original_image_files_paths.size();
	for (int i = 0; i < n; i++) {
		Thumbnails::generate_thumbnail_if_missing(this->original_image_files_paths.at(i));
		const bool end_job = this->set_progress_state((i + 1.0) / n);
		if (end_job) {
			return; /* Abort thread. */
		}
	}

	/* Redraw to show the thumbnails as they are now created. */
	if (this->layer) {
		this->layer->emit_layer_changed("TRW - thumbnail creator"); /* NB update from background thread. */
	}

	return;
}




void LayerTRW::generate_missing_thumbnails(void)
{
	if (!this->has_missing_thumbnails) {
		return;
	}

	QStringList original_image_files_paths = this->waypoints.get_list_of_missing_thumbnails();
	const int n_images = original_image_files_paths.size();
	if (0 == n_images) {
		return;
	}

	ThumbnailCreator * creator = new ThumbnailCreator(this, original_image_files_paths);
	creator->set_description(tr("Creating %1 Image Thumbnails...").arg(n_images));
	creator->run_in_background(ThreadPoolType::Local);
}




void LayerTRW::sort_all()
{
	if (!this->is_in_tree()) {
		return;
	}

	/* Obviously need 2 to tango - sorting with only 1 (or less) is a lonely activity! */
	if (this->tracks.size() > 1) {
		this->tree_view->sort_children(&this->tracks, this->track_sort_order);
	}

	if (this->routes.size() > 1) {
		this->tree_view->sort_children(&this->routes, this->track_sort_order);
	}

	if (this->waypoints.size() > 1) {
		this->tree_view->sort_children(&this->waypoints, this->wp_sort_order);
	}
}




/**
 * Get the earliest timestamp available for this layer.
 */
time_t LayerTRW::get_timestamp(void)
{
	const time_t timestamp_tracks = this->tracks.get_earliest_timestamp();
	const time_t timestamp_waypoints = this->waypoints.get_earliest_timestamp();
	/* NB routes don't have timestamps - hence they are not considered. */

	if (!timestamp_tracks && !timestamp_waypoints) {
		/* Fallback to get time from the metadata when no other timestamps available. */
		if (this->metadata
		    && this->metadata->has_timestamp
		    && !this->metadata->iso8601_timestamp.isEmpty()) {

			const QDateTime ts = QDateTime::fromString(this->metadata->iso8601_timestamp, Qt::ISODate);
			if (!ts.isNull() && ts.isValid()) {
				return ts.toMSecsSinceEpoch() / MSECS_PER_SEC; /* TODO_2_LATER: use toSecsSinceEpoch() when new version of QT library becomes more available. */
			} else {
				qDebug() << SG_PREFIX_E << "Failed to convert ISO8601 metadata timestamp" << this->metadata->iso8601_timestamp;
			}
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
		this->generate_missing_thumbnails();
	}
	this->tracks.assign_colors(this->painter->track_drawing_mode, this->painter->track_color_common);
	this->routes.assign_colors(this->painter->track_drawing_mode, this->painter->track_color_common); /* For routes the first argument is ignored. */

	this->waypoints.recalculate_bbox();
	this->tracks.recalculate_bbox();
	this->routes.recalculate_bbox();


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
			if (!this->metadata->iso8601_timestamp.isEmpty()) {
				need_to_set_time = true;
			}
		}

		if (need_to_set_time) {
			QDateTime meta_time;
			const time_t local_timestamp = this->get_timestamp();
			if (local_timestamp == 0) {
				/* No time found - so use 'now' for the metadata time. */
				meta_time = QDateTime::currentDateTime(); /* The method returns time in local time zone. */
			} else {
				meta_time.setMSecsSinceEpoch(local_timestamp * MSECS_PER_SEC); /* TODO_MAYBE: replace with setSecsSinceEpoch() in future. */
			}

			this->metadata->iso8601_timestamp = meta_time.toString(Qt::ISODate);
		}
	}
}




CoordMode LayerTRW::get_coord_mode(void) const
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
		qDebug() << SG_PREFIX_SIGNAL << "Will call 'emit_items_tree_updated_cb() for" << this->get_name();
		panel->emit_items_tree_updated_cb(this->get_name());

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




/* ----------- Downloading maps along tracks --------------- */

void vik_track_download_map(Track * trk, LayerMap * layer_map, const VikingZoomLevel & viking_zoom_level)
{
	std::list<Rect *> * rects_to_download = trk->get_map_rectangles(viking_zoom_level);
	if (!rects_to_download) {
		return;
	}

	for (auto rect_iter = rects_to_download->begin(); rect_iter != rects_to_download->end(); rect_iter++) {
		layer_map->download_section((*rect_iter)->tl, (*rect_iter)->br, viking_zoom_level);
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
	std::vector<VikingZoomLevel> viking_zooms = {
		VikingZoomLevel(0.125),
		VikingZoomLevel(0.25),
		VikingZoomLevel(0.5),
		VikingZoomLevel(1),
		VikingZoomLevel(2),
		VikingZoomLevel(4),
		VikingZoomLevel(8),
		VikingZoomLevel(16),
		VikingZoomLevel(32),
		VikingZoomLevel(64),
		VikingZoomLevel(128),
		VikingZoomLevel(256),
		VikingZoomLevel(512),
		VikingZoomLevel(1024) };

	LayersPanel * panel = ThisApp::get_layers_panel();
	const Viewport * viewport = ThisApp::get_main_viewport();

	Track * track = this->get_edited_track();
	if (!track) {
		return;
	}

	const std::list<Layer const *> layers = panel->get_all_layers_of_type(LayerType::Map, true); /* Includes hidden map layer types. */
	int num_maps = layers.size();
	if (!num_maps) {
		Dialog::error(tr("No map layer in use. Create one first"), this->get_window());
		return;
	}

	/* Convert from list of layers to list of names. Allowing the user to select one of them. */

	std::list<LayerMap *> map_layers;
	QStringList map_labels; /* List of names of map layers that are currently used. */

	for (auto iter = layers.begin(); iter != layers.end(); iter++) {
		map_layers.push_back((LayerMap *) *iter); /* kamilFIXME: casting const pointer to non-const. */
		map_labels << ((LayerMap *) *iter)->get_map_label();
	}



	const VikingZoomLevel current_viking_zoom(viewport->get_viking_zoom_level().get_x());
	int default_zoom_idx = 0;
	if (0 != VikingZoomLevel::get_closest_index(default_zoom_idx, viking_zooms, current_viking_zoom)) {
		qDebug() << SG_PREFIX_W << "Failed to get the closest viking zoom level";
		default_zoom_idx = viking_zooms.size() - 1;
	}



	MapAndZoomDialog dialog(QObject::tr("Download along track"), map_labels, viking_zooms, this->get_window());
	dialog.preselect(0, default_zoom_idx);
	if (QDialog::Accepted != dialog.exec()) {
		return;
	}
	const int selected_map_idx = dialog.get_map_idx();
	const int selected_zoom_idx = dialog.get_zoom_idx();



	auto iter = map_layers.begin();
	for (int i = 0; i < selected_map_idx; i++) {
		iter++;
	}

	vik_track_download_map(track, *iter, viking_zooms[selected_zoom_idx]);

	return;
}




void LayerTRW::track_list_dialog_cb(void)
{
	const QString title = tr("%1: Track and Route List").arg(this->name);
	track_list_dialog(title, this, "");
}




void LayerTRW::waypoint_list_dialog_cb(void) /* Slot. */
{
	const QString title = tr("%1: Waypoint List").arg(this->name);
	waypoint_list_dialog(title, this);
}




LayerDataReadStatus LayerTRW::read_layer_data(QFile & file, const QString & dirpath)
{
	qDebug() << SG_PREFIX_D << "Will call gpspoint_read_file() to read Layer Data";
	return GPSPoint::read_layer_from_file(file, this, dirpath);
}




sg_ret LayerTRW::write_layer_data(FILE * file) const
{
	fprintf(file, "\n\n~LayerData\n");
	const sg_ret rv = GPSPoint::write_layer_to_file(file, this);
	fprintf(file, "~EndLayerData\n");

	return rv;
}




LayerTRW::LayerTRW() : Layer()
{
	this->type = LayerType::TRW;
	strcpy(this->debug_string, "TRW");
	this->interface = &vik_trw_layer_interface;

	this->tracks.owning_layer = this;
	this->routes.owning_layer = this;
	this->waypoints.owning_layer = this;

	this->painter = new LayerTRWPainter(this);

	this->set_initial_parameter_values();
	this->set_name(Layer::get_type_ui_label(this->type));

	/* Param settings that are not available via the GUI. */
	/* Force to on after processing params (which defaults them to off with a zero value). */
	this->tracks.visible = true;
	this->routes.visible = true;
	this->waypoints.visible = true;

	this->metadata = new TRWMetadata();
	this->draw_sync_done = true;
	this->draw_sync_do = true;
	/* Everything else is 0, false or NULL. */

	/* This should be called after set_initial_parameter_values().
	   Notice that call to make_wp_pens() is done only once, but
	   make_track_pens() is used more than once in the code
	   base. */
	this->painter->make_track_pens();
	this->painter->make_wp_pens();

	const SGVariant limit = interface->parameter_default_values[PARAM_WP_IMAGE_CACHE_CAPACITY];
	if (limit.is_valid()) {
		this->wp_image_cache.set_capacity_megabytes(limit.u.val_int);
	}
}




LayerTRW::~LayerTRW()
{
	delete this->tpwin;

	delete this->painter;
}




/* To be called right after constructor. */
void LayerTRW::set_coord_mode(CoordMode new_mode)
{
	this->coord_mode = new_mode;
}




bool LayerTRW::handle_selection_in_tree(void)
{
	qDebug() << SG_PREFIX_I << "Tree item" << this->name << "becomes selected tree item";

	this->reset_internal_selections(); /* No other tree item (that is a sublayer of this layer) is selected... */

	/* Set info about current selection. */

	g_tree->add_to_set_of_selected(this);

	/* Set highlight thickness. */
	ThisApp::get_main_viewport()->set_highlight_thickness(this->get_track_thickness());

	/* Mark for redraw. */
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
	if (!track) {
		qDebug() << SG_PREFIX_E << "NULL track";
		return;
	}

	this->current_track_ = track;
	this->current_track_->selected_tp_iter.iter = tp_iter;
	this->current_track_->selected_tp_iter.valid = true;
}





void LayerTRW::set_edited_track(Track * track)
{
	if (!track) {
		qDebug() << SG_PREFIX_E << "NULL track";
		return;
	}

	this->current_track_ = track;
	this->current_track_->selected_tp_iter.valid = false;
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
	if (!wp) {
		qDebug() << SG_PREFIX_E << "NULL waypoint";
		return;
	}

	this->current_wp_ = wp;
}




bool LayerTRW::reset_edited_wp(void)
{
	const bool was_selected = NULL != this->current_wp_;

	this->current_wp_ = NULL;

	return was_selected;
}




bool LayerTRW::get_track_creation_in_progress() const
{
	LayerToolTRWNewTrack * new_track_tool = (LayerToolTRWNewTrack *) ThisApp::get_main_window()->get_toolbox()->get_tool(LAYER_TRW_TOOL_CREATE_TRACK);
	return new_track_tool->creation_in_progress == this;
}




void LayerTRW::reset_track_creation_in_progress()
{
	LayerToolTRWNewTrack * new_track_tool = (LayerToolTRWNewTrack *) ThisApp::get_main_window()->get_toolbox()->get_tool(LAYER_TRW_TOOL_CREATE_TRACK);
	if (new_track_tool->creation_in_progress == this) {
		new_track_tool->creation_in_progress = NULL;
	}
}




bool LayerTRW::get_route_creation_in_progress() const
{
	LayerToolTRWNewTrack * new_route_tool = (LayerToolTRWNewTrack *) ThisApp::get_main_window()->get_toolbox()->get_tool(LAYER_TRW_TOOL_CREATE_ROUTE);
	return new_route_tool->creation_in_progress == this;
}




void LayerTRW::reset_route_creation_in_progress()
{
	LayerToolTRWNewTrack * new_route_tool = (LayerToolTRWNewTrack *) ThisApp::get_main_window()->get_toolbox()->get_tool(LAYER_TRW_TOOL_CREATE_ROUTE);
	if (new_route_tool->creation_in_progress == this) {
		new_route_tool->creation_in_progress = NULL;
	}
}




void LayerTRW::show_wp_picture_cb(void) /* Slot. */
{
	Waypoint * wp = this->get_edited_wp();
	if (!wp) {
		qDebug() << SG_PREFIX_E << "No waypoint in waypoint-related callback" << __FUNCTION__;
		return;
	}

	const QString program = Preferences::get_image_viewer();
	const QString image_full_path = this->get_edited_wp()->image_full_path;
	const QString quoted_image_full_path = Util::shell_quote(image_full_path);

	QStringList args;
	args << quoted_image_full_path;

	qDebug() << SG_PREFIX_I << program << quoted_image_full_path;

	/* "Fire and forget". The viewer will run detached from this application. */
	QProcess::startDetached(program, args);

	/* TODO_LATER: add handling of errors from process. */
}
