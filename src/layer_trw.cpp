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
#include <utility>




#include <QDateTime>
#include <QInputDialog>
#include <QStandardPaths>




#include "layer_aggregate.h"
#include "layer_trw.h"
#include "layer_trw_painter.h"
#include "layer_trw_tools.h"
#include "layer_trw_dialogs.h"
#include "layer_trw_waypoint_properties.h"
#include "layer_trw_track_internal.h"
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
#include "astro.h"
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




extern SelectedTreeItems g_selected;




/****** PARAMETERS ******/

enum {
	PARAMETER_GROUP_WAYPOINTS,
	PARAMETER_GROUP_TRACKS,
	PARAMETER_GROUP_IMAGES,
	PARAMETER_GROUP_TRACKS_ADV,
	PARAMETER_GROUP_METADATA
};




static WidgetEnumerationData track_drawing_modes_enum = {
	{
		SGLabelID(QObject::tr("Draw by Track"),                  (int) LayerTRWTrackDrawingMode::ByTrack),
		SGLabelID(QObject::tr("Draw by Speed"),                  (int) LayerTRWTrackDrawingMode::BySpeed),
		SGLabelID(QObject::tr("All Tracks Have The Same Color"), (int) LayerTRWTrackDrawingMode::AllSameColor),
	},
	(int) LayerTRWTrackDrawingMode::ByTrack
};
static SGVariant track_drawing_modes_default(void) { return SGVariant(track_drawing_modes_enum.default_value, SGVariantType::Enumeration); }




static WidgetEnumerationData wp_symbol_enum = {
	{
		SGLabelID(QObject::tr("Filled Square"), (int) GraphicMarker::FilledSquare),
		SGLabelID(QObject::tr("Square"),        (int) GraphicMarker::Square),
		SGLabelID(QObject::tr("Circle"),        (int) GraphicMarker::Circle),
		SGLabelID(QObject::tr("X"),             (int) GraphicMarker::X),
	},
	(int) GraphicMarker::FilledSquare
};
static SGVariant wp_symbol_default(void) { return SGVariant(wp_symbol_enum.default_value, SGVariantType::Enumeration); }




#define MIN_POINT_SIZE 2
#define MAX_POINT_SIZE 10

#define MIN_ARROW_SIZE 3
#define MAX_ARROW_SIZE 20

                                                        /*            min,              max,                     hardcoded default,    step,   digits */
static ParameterScale<int> scale_track_thickness              (              1,               10,                SGVariant((int32_t) 1, SGVariantType::Int),       1,        0); /* PARAM_TRACK_THICKNESS */
static ParameterScale<int> scale_track_draw_speed_factor      (              0,              100,                      SGVariant(30.0),                            1,        0); /* PARAM_TRACK_DRAW_SPEED_FACTOR */
static ParameterScale<int> scale_wp_image_size                (             16,              128,               SGVariant((int32_t) 64, SGVariantType::Int),       4,        0); /* PARAM_WP_IMAGE_SIZE */
static ParameterScale<int> scale_wp_image_alpha               (              0,              255,              SGVariant((int32_t) 255, SGVariantType::Int),       5,        0); /* PARAM_WP_IMAGE_ALPHA */
static ParameterScale<int> scale_wp_image_cache_capacity      (              5,              500,              SGVariant((int32_t) 300, SGVariantType::Int),       5,        0); /* PARAM_WP_IMAGE_CACHE_CAPACITY, megabytes */
static ParameterScale<int> scale_track_bg_thickness           (              0,                8,                SGVariant((int32_t) 0, SGVariantType::Int),       1,        0); /* PARAM_TRACK_BG_THICKNESS */
static ParameterScale<int> scale_wp_marker_size               (              1,               64,                SGVariant((int32_t) 4, SGVariantType::Int),       1,        0); /* PARAM_WP_MARKER_SIZE */
static ParameterScale<int> scale_track_elevation_factor       (              1,              100,               SGVariant((int32_t) 30, SGVariantType::Int),       1,        0); /* PARAM_TRACK_ELEVATION_FACTOR */
static ParameterScale<int> scale_trackpoint_size              ( MIN_POINT_SIZE,   MAX_POINT_SIZE,   SGVariant((int32_t) MIN_POINT_SIZE, SGVariantType::Int),       1,        0); /* PARAM_TRACKPOINT_SIZE */
static ParameterScale<int> scale_track_direction_size         ( MIN_ARROW_SIZE,   MAX_ARROW_SIZE,                SGVariant((int32_t) 5, SGVariantType::Int),       1,        0); /* PARAM_TRACK_DIRECTION_SIZE */

static MeasurementScale<Duration, Duration_ll, DurationUnit> scale_track_min_stop_duration(MIN_STOP_LENGTH, MAX_STOP_LENGTH, 60, 1, DurationUnit::Seconds, 0); /* PARAM_TRACK_MIN_STOP_LENGTH */



static WidgetEnumerationData font_size_enum = {
	{
		SGLabelID("Extra Extra Small",   FS_XX_SMALL),
		SGLabelID("Extra Small",         FS_X_SMALL),
		SGLabelID("Small",               FS_SMALL),
		SGLabelID("Medium",              FS_MEDIUM),
		SGLabelID("Large",               FS_LARGE),
		SGLabelID("Extra Large",         FS_X_LARGE),
		SGLabelID("Extra Extra Large",   FS_XX_LARGE),
	},
	FS_MEDIUM
};
static SGVariant font_size_default(void) { return SGVariant(font_size_enum.default_value, SGVariantType::Enumeration); }




static WidgetEnumerationData sort_order_trk_enum = {
	{
		SGLabelID("None",            (int) TreeViewSortOrder::None),
		SGLabelID("Name Ascending",  (int) TreeViewSortOrder::AlphabeticalAscending),
		SGLabelID("Name Descending", (int) TreeViewSortOrder::AlphabeticalDescending),
		SGLabelID("Date Ascending",  (int) TreeViewSortOrder::DateAscending),
		SGLabelID("Date Descending", (int) TreeViewSortOrder::DateDescending),
	},
	(int) TreeViewSortOrder::None
};
static SGVariant sort_order_trk_default(void) { return SGVariant(sort_order_trk_enum.default_value, SGVariantType::Enumeration); }




static WidgetEnumerationData sort_order_wp_enum = {
	{
		SGLabelID("None",            (int) TreeViewSortOrder::None),
		SGLabelID("Name Ascending",  (int) TreeViewSortOrder::AlphabeticalAscending),
		SGLabelID("Name Descending", (int) TreeViewSortOrder::AlphabeticalDescending),
		SGLabelID("Date Ascending",  (int) TreeViewSortOrder::DateAscending),
		SGLabelID("Date Descending", (int) TreeViewSortOrder::DateDescending),
	},
	(int) TreeViewSortOrder::None
};
static SGVariant sort_order_wp_default(void) { return SGVariant(sort_order_wp_enum.default_value, SGVariantType::Enumeration); }




static SGVariant black_color_default(void)           { return SGVariant(0, 0, 0, 100); } /* Black. */
static SGVariant trackbgcolor_default(void)          { return SGVariant(255, 255, 255, 100); }  /* White. */
static SGVariant wptextcolor_default(void)           { return SGVariant(255, 255, 255, 100); } /* White. */
static SGVariant wpbgcolor_default(void)             { return SGVariant(0x83, 0x83, 0xc4, 100); } /* Kind of Blue. */
static SGVariant string_default(void)                { return SGVariant(""); }




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
	{ PARAM_TRACKS_VISIBLE,          "tracks_visible",    SGVariantType::Boolean,      PARAMETER_GROUP_HIDDEN,            QObject::tr("Tracks are visible"),               WidgetType::CheckButton,  NULL,                        sg_variant_true,             "" },
	{ PARAM_WAYPOINTS_VISIBLE,       "waypoints_visible", SGVariantType::Boolean,      PARAMETER_GROUP_HIDDEN,            QObject::tr("Waypoints are visible"),            WidgetType::CheckButton,  NULL,                        sg_variant_true,             "" },
	{ PARAM_ROUTES_VISIBLE,          "routes_visible",    SGVariantType::Boolean,      PARAMETER_GROUP_HIDDEN,            QObject::tr("Routes are visible"),               WidgetType::CheckButton,  NULL,                        sg_variant_true,             "" },

	{ PARAM_DRAW_TRACK_LABELS,       "trackdrawlabels",   SGVariantType::Boolean,      PARAMETER_GROUP_TRACKS,            QObject::tr("Draw Labels"),                      WidgetType::CheckButton,  NULL,                        sg_variant_true,             QObject::tr("Note: the individual track controls what labels may be displayed") },
	{ PARAM_TRACK_LABEL_FONT_SIZE,   "trackfontsize",     SGVariantType::Enumeration,  PARAMETER_GROUP_TRACKS_ADV,        QObject::tr("Size of Track Label's Font:"),      WidgetType::Enumeration,  &font_size_enum,             font_size_default,           "" },
	{ PARAM_TRACK_DRAWING_MODE,      "drawmode",          SGVariantType::Enumeration,  PARAMETER_GROUP_TRACKS,            QObject::tr("Track Drawing Mode:"),              WidgetType::Enumeration,  &track_drawing_modes_enum,   track_drawing_modes_default, "" },
	{ PARAM_TRACK_COLOR_COMMON,      "trackcolor",        SGVariantType::Color,        PARAMETER_GROUP_TRACKS,            QObject::tr("All Tracks Color:"),                WidgetType::Color,        NULL,                        black_color_default,         QObject::tr("The color used when 'All Tracks Same Color' drawing mode is selected") },
	{ PARAM_DRAW_TRACK_LINES,        "drawlines",         SGVariantType::Boolean,      PARAMETER_GROUP_TRACKS,            QObject::tr("Draw Track Lines"),                 WidgetType::CheckButton,  NULL,                        sg_variant_true,             "" },
	{ PARAM_TRACK_THICKNESS,         "line_thickness",    SGVariantType::Int,          PARAMETER_GROUP_TRACKS_ADV,        QObject::tr("Track Thickness:"),                 WidgetType::SpinBoxInt,   &scale_track_thickness,      NULL,                        "" },
	{ PARAM_DRAW_TRACK_DIRECTIONS,   "drawdirections",    SGVariantType::Boolean,      PARAMETER_GROUP_TRACKS,            QObject::tr("Draw Track Directions:"),           WidgetType::CheckButton,  NULL,                        sg_variant_false,            "" },
	{ PARAM_TRACK_DIRECTION_SIZE,    "trkdirectionsize",  SGVariantType::Int,          PARAMETER_GROUP_TRACKS_ADV,        QObject::tr("Direction Size:"),                  WidgetType::SpinBoxInt,   &scale_track_direction_size, NULL,                        "" },
	{ PARAM_DRAW_TRACKPOINTS,        "drawpoints",        SGVariantType::Boolean,      PARAMETER_GROUP_TRACKS,            QObject::tr("Draw Trackpoints:"),                WidgetType::CheckButton,  NULL,                        sg_variant_true,             "" },
	{ PARAM_TRACKPOINT_SIZE,         "trkpointsize",      SGVariantType::Int,          PARAMETER_GROUP_TRACKS_ADV,        QObject::tr("Trackpoint Size:"),                 WidgetType::SpinBoxInt,   &scale_trackpoint_size,      NULL,                        "" },
	{ PARAM_DRAW_TRACK_ELEVATION,    "drawelevation",     SGVariantType::Boolean,      PARAMETER_GROUP_TRACKS,            QObject::tr("Draw Track Elevation"),             WidgetType::CheckButton,  NULL,                        sg_variant_false,            "" },
	{ PARAM_TRACK_ELEVATION_FACTOR,  "elevation_factor",  SGVariantType::Int,          PARAMETER_GROUP_TRACKS_ADV,        QObject::tr("Draw Elevation Height %:"),         WidgetType::HScale,       &scale_track_elevation_factor, NULL,                      "" },
	{ PARAM_DRAW_TRACK_STOPS,        "drawstops",         SGVariantType::Boolean,      PARAMETER_GROUP_TRACKS,            QObject::tr("Draw Track Stops:"),                WidgetType::CheckButton,  NULL,                        sg_variant_false,            QObject::tr("Whether to draw a marker when trackpoints are at the same position but over the minimum stop length apart in time") },
	{ PARAM_TRACK_MIN_STOP_LENGTH,   "stop_length",       SGVariantType::DurationType, PARAMETER_GROUP_TRACKS_ADV,        QObject::tr("Min Stop Length:"),                 WidgetType::DurationType, &scale_track_min_stop_duration,NULL,                        "" }, // KKAMIL

	{ PARAM_TRACK_BG_THICKNESS,      "bg_line_thickness", SGVariantType::Int,          PARAMETER_GROUP_TRACKS_ADV,        QObject::tr("Track Background Thickness:"),      WidgetType::SpinBoxInt,   &scale_track_bg_thickness,   NULL,                        "" },
	{ PARAM_TRK_BG_COLOR,            "trackbgcolor",      SGVariantType::Color,        PARAMETER_GROUP_TRACKS_ADV,        QObject::tr("Track Background Color"),           WidgetType::Color,        NULL,                        trackbgcolor_default,        "" },
	{ PARAM_TRACK_DRAW_SPEED_FACTOR, "speed_factor",      SGVariantType::Double,       PARAMETER_GROUP_TRACKS_ADV,        QObject::tr("Draw by Speed Factor (%):"),        WidgetType::HScale,       &scale_track_draw_speed_factor, NULL,                     QObject::tr("The percentage factor away from the average speed determining the color used") },
	{ PARAM_TRACK_SORT_ORDER,        "tracksortorder",    SGVariantType::Enumeration,  PARAMETER_GROUP_TRACKS_ADV,        QObject::tr("Track Sort Order:"),                WidgetType::Enumeration,  &sort_order_trk_enum,        sort_order_trk_default,      "" },

	{ PARAM_WP_LABELS,               "drawlabels",        SGVariantType::Boolean,      PARAMETER_GROUP_WAYPOINTS,         QObject::tr("Draw Waypoint Labels"),             WidgetType::CheckButton,  NULL,                        sg_variant_true,             "" },
	{ PARAM_WP_LABEL_FONT_SIZE,      "wpfontsize",        SGVariantType::Enumeration,  PARAMETER_GROUP_WAYPOINTS,         QObject::tr("Font Size of Waypoint's Label:"),   WidgetType::Enumeration,  &font_size_enum,             font_size_default,           "" },
	{ PARAM_WP_MARKER_COLOR,         "wpcolor",           SGVariantType::Color,        PARAMETER_GROUP_WAYPOINTS,         QObject::tr("Color of Waypoint's Marker:"),      WidgetType::Color,        NULL,                        black_color_default,         "" },
	{ PARAM_WP_LABEL_FG_COLOR,       "wptextcolor",       SGVariantType::Color,        PARAMETER_GROUP_WAYPOINTS,         QObject::tr("Color of Waypoint's Label:"),       WidgetType::Color,        NULL,                        wptextcolor_default,         "" },
	{ PARAM_WP_LABEL_BG_COLOR,       "wpbgcolor",         SGVariantType::Color,        PARAMETER_GROUP_WAYPOINTS,         QObject::tr("Background of Waypoint's Label:"),  WidgetType::Color,        NULL,                        wpbgcolor_default,           "" },
	{ PARAM_WPBA,                    "wpbgand",           SGVariantType::Boolean,      PARAMETER_GROUP_WAYPOINTS,         QObject::tr("Fake BG Color Translucency:"),      WidgetType::CheckButton,  NULL,                        sg_variant_false,            "" },
	{ PARAM_WP_MARKER_TYPE,          "wpsymbol",          SGVariantType::Enumeration,  PARAMETER_GROUP_WAYPOINTS,         QObject::tr("Type of Waypoint's Marker:"),       WidgetType::Enumeration,  &wp_symbol_enum,             wp_symbol_default,           "" },
	{ PARAM_WP_MARKER_SIZE,          "wpsize",            SGVariantType::Int,          PARAMETER_GROUP_WAYPOINTS,         QObject::tr("Size of Waypoint's Marker:"),       WidgetType::SpinBoxInt,   &scale_wp_marker_size,       NULL,                        "" },
	{ PARAM_DRAW_WP_SYMBOLS,         "wpsyms",            SGVariantType::Boolean,      PARAMETER_GROUP_WAYPOINTS,         QObject::tr("Draw Waypoint Symbols:"),           WidgetType::CheckButton,  NULL,                        sg_variant_true,             "" },
	{ PARAM_WP_SORT_ORDER,           "wpsortorder",       SGVariantType::Enumeration,  PARAMETER_GROUP_WAYPOINTS,         QObject::tr("Waypoint Sort Order:"),             WidgetType::Enumeration,  &sort_order_wp_enum,         sort_order_wp_default,       "" },

	{ PARAM_DRAW_WP_IMAGES,          "drawimages",        SGVariantType::Boolean,      PARAMETER_GROUP_IMAGES,            QObject::tr("Draw Waypoint Images"),             WidgetType::CheckButton,  NULL,                        sg_variant_true,             "" },
	{ PARAM_WP_IMAGE_SIZE,           "image_size",        SGVariantType::Int,          PARAMETER_GROUP_IMAGES,            QObject::tr("Waypoint Image Size (pixels):"),    WidgetType::HScale,       &scale_wp_image_size,        NULL,                        "" },
	{ PARAM_WP_IMAGE_ALPHA,          "image_alpha",       SGVariantType::Int,          PARAMETER_GROUP_IMAGES,            QObject::tr("Waypoint Image Alpha:"),            WidgetType::HScale,       &scale_wp_image_alpha,       NULL,                        "" },
	{ PARAM_WP_IMAGE_CACHE_CAPACITY, "image_cache_size",  SGVariantType::Int,          PARAMETER_GROUP_IMAGES,            QObject::tr("Waypoint Images' Memory Cache Size (MB):"),WidgetType::HScale,&scale_wp_image_cache_capacity,  NULL,                    "" },

	{ PARAM_MDDESC,                  "metadatadesc",      SGVariantType::String,       PARAMETER_GROUP_METADATA,          QObject::tr("Description"),                      WidgetType::Entry,        NULL,                        string_default,              "" },
	{ PARAM_MDAUTH,                  "metadataauthor",    SGVariantType::String,       PARAMETER_GROUP_METADATA,          QObject::tr("Author"),                           WidgetType::Entry,        NULL,                        string_default,              "" },
	{ PARAM_MDTIME,                  "metadatatime",      SGVariantType::String,       PARAMETER_GROUP_METADATA,          QObject::tr("Creation Time"),                    WidgetType::Entry,        NULL,                        string_default,              "" },
	{ PARAM_MDKEYS,                  "metadatakeywords",  SGVariantType::String,       PARAMETER_GROUP_METADATA,          QObject::tr("Keywords"),                         WidgetType::Entry,        NULL,                        string_default,              "" },

	{ NUM_PARAMS,                    "",                  SGVariantType::Empty,        PARAMETER_GROUP_GENERIC,           "",                                              WidgetType::None,         NULL,                        NULL,                        "" }, /* Guard. */
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

	this->fixed_layer_kind_string = "TrackWaypoint"; /* Non-translatable. */

	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_Y;
	// this->action_icon = ...; /* Set elsewhere. */

	this->ui_labels.new_layer = QObject::tr("New Track/Route/Waypoint Layer");
	this->ui_labels.translated_layer_kind = QObject::tr("TrackWaypoint");
	this->ui_labels.layer_defaults = QObject::tr("Default Settings of Track/Route/Waypoint Layer");
}




/**
   @reviewed-on 2019-12-01
*/
LayerToolContainer LayerTRWInterface::create_tools(Window * window, GisViewport * gisview)
{
	LayerToolContainer tools;

	/* This method should be called only once. */
	static bool created = false;
	if (created) {
		return tools;
	}

	LayerTool * tool = nullptr;

	/* I'm using assertions to verify that tools have unique IDs
	   (at least unique within a group of tools). */

	tool = new LayerToolTRWNewWaypoint(window, gisview);
	auto status = tools.insert({ tool->get_tool_id(), tool });
	assert(status.second);

	tool = new LayerToolTRWNewTrack(window, gisview);
	status = tools.insert({ tool->get_tool_id(), tool });
	assert(status.second);

	tool = new LayerToolTRWNewRoute(window, gisview);
	status = tools.insert({ tool->get_tool_id(), tool });
	assert(status.second);

	tool = new LayerToolTRWExtendedRouteFinder(window, gisview);
	status = tools.insert({ tool->get_tool_id(), tool });
	assert(status.second);

	tool = new LayerToolTRWEditWaypoint(window, gisview);
	status = tools.insert({ tool->get_tool_id(), tool });
	assert(status.second);

	tool = new LayerToolTRWEditTrackpoint(window, gisview);
	status = tools.insert({ tool->get_tool_id(), tool });
	assert(status.second);

	tool = new LayerToolTRWShowPicture(window, gisview);
	status = tools.insert({ tool->get_tool_id(), tool });
	assert(status.second);

	created = true;

	return tools;
}




bool g_have_diary_program = false;
static QString diary_program;
#define VIK_SETTINGS_EXTERNAL_DIARY_PROGRAM "external_diary_program"

bool have_geojson_export = false;




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

	Astro::init();
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




sg_ret TRWMetadata::set_iso8601_timestamp(const QString & value)
{
	/* QDateTime::fromString():
	   "Returns the QDateTime represented by the string, using the format given, or an invalid datetime if this is not possible."
	   So we only have to check .isValid(). We don't need to check .isNull() of the timestamp. */

	this->iso8601_timestamp = QDateTime::fromString(value, Qt::ISODate);
	if (this->iso8601_timestamp.isValid()) {
		return sg_ret::ok;
	} else {
		qDebug() << SG_PREFIX_E << "Failed to convert ISO8601 metadata timestamp" << value;
		return sg_ret::err;
	}
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
		Clipboard::copy(ClipboardDataType::Sublayer, LayerKind::TRW, item->m_type_id, pickle, item->name);
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

	if (item->get_type_id() == Waypoint::type_id()) {
		Waypoint * wp = Waypoint::unmarshall(pickle);
		/* When copying - we'll create a new name based on the original. */
		const QString uniq_name = this->new_unique_element_name(item->m_type_id, wp->name);
		wp->set_name(uniq_name);

		this->add_waypoint(wp);

		wp->convert(this->coord_mode);
		this->waypoints.recalculate_bbox();

		/* Consider if redraw necessary for the new item. */
		if (this->is_visible() && this->waypoints.is_visible() && wp->is_visible()) {
			this->emit_tree_item_changed("TRW - paste waypoint");
		}
		return true;
	} else if (item->get_type_id() == Track::type_id()) {
		Track * trk = Track::unmarshall(pickle);

		/* When copying - we'll create a new name based on the original. */
		const QString uniq_name = this->new_unique_element_name(item->m_type_id, trk->name);
		trk->set_name(uniq_name);

		this->add_track(trk);

		trk->change_coord_mode(this->coord_mode);

		/* Consider if redraw necessary for the new item. */
		if (this->is_visible() && this->tracks.is_visible() && trk->is_visible()) {
			this->emit_tree_item_changed("TRW - paste track");
		}
		return true;
	} else if (item->get_type_id() == Route::type_id()) {
		Track * trk = Track::unmarshall(pickle);
		/* When copying - we'll create a new name based on the original. */
		const QString uniq_name = this->new_unique_element_name(item->m_type_id, trk->name);
		trk->set_name(uniq_name);

		this->add_route(trk);
		trk->change_coord_mode(this->coord_mode);

		/* Consider if redraw necessary for the new item. */
		if (this->is_visible() && this->routes.is_visible() && trk->is_visible()) {
			this->emit_tree_item_changed("TRW - paste route");
		}
		return true;
	} else {
		qDebug() << SG_PREFIX_E << "Unhandled object type id" << item->m_type_id;
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
		this->tracks.set_visible(data.u.val_bool);
		break;
	case PARAM_WAYPOINTS_VISIBLE:
		this->waypoints.set_visible(data.u.val_bool);
		break;
	case PARAM_ROUTES_VISIBLE:
		this->routes.set_visible(data.u.val_bool);
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
		if (data.get_duration() >= scale_track_min_stop_duration.m_min && data.get_duration() <= scale_track_min_stop_duration.m_max) { /* FIXME: m_min and m_max have to be converted to internal units. */
			this->painter->track_min_stop_duration = data.get_duration();
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
	case PARAM_TRACKS_VISIBLE:          rv = SGVariant(this->tracks.is_visible());                       break;
	case PARAM_WAYPOINTS_VISIBLE:       rv = SGVariant(this->waypoints.is_visible());                    break;
	case PARAM_ROUTES_VISIBLE:          rv = SGVariant(this->routes.is_visible());                       break;
	case PARAM_DRAW_TRACK_LABELS:       rv = SGVariant(this->painter->draw_track_labels);                break;
	case PARAM_TRACK_LABEL_FONT_SIZE:   rv = SGVariant((int32_t) this->painter->track_label_font_size, trw_layer_param_specs[param_id].type_id);  break;
	case PARAM_TRACK_DRAWING_MODE:      rv = SGVariant((int32_t) this->painter->track_drawing_mode, trw_layer_param_specs[param_id].type_id);     break;
	case PARAM_TRACK_COLOR_COMMON:      rv = SGVariant(this->painter->track_color_common);               break;
	case PARAM_DRAW_TRACKPOINTS:        rv = SGVariant(this->painter->draw_trackpoints);                 break;
	case PARAM_TRACKPOINT_SIZE:         rv = SGVariant((int32_t) this->painter->trackpoint_size, trw_layer_param_specs[param_id].type_id);        break;
	case PARAM_DRAW_TRACK_ELEVATION:    rv = SGVariant(this->painter->draw_track_elevation);             break;
	case PARAM_TRACK_ELEVATION_FACTOR:  rv = SGVariant((int32_t) this->painter->track_elevation_factor, trw_layer_param_specs[param_id].type_id); break;
	case PARAM_DRAW_TRACK_STOPS:        rv = SGVariant(this->painter->draw_track_stops);                 break;
	case PARAM_TRACK_MIN_STOP_LENGTH:   rv = SGVariant(this->painter->track_min_stop_duration, trw_layer_param_specs[param_id].type_id);  break;
	case PARAM_DRAW_TRACK_LINES:        rv = SGVariant(this->painter->draw_track_lines);                 break;
	case PARAM_DRAW_TRACK_DIRECTIONS:   rv = SGVariant(this->painter->draw_track_directions);            break;
	case PARAM_TRACK_DIRECTION_SIZE:    rv = SGVariant((int32_t) this->painter->draw_track_directions_size, trw_layer_param_specs[param_id].type_id); break;
	case PARAM_TRACK_THICKNESS:         rv = SGVariant((int32_t) this->painter->track_thickness, trw_layer_param_specs[param_id].type_id);        break;
	case PARAM_TRACK_BG_THICKNESS:      rv = SGVariant((int32_t) this->painter->track_bg_thickness, trw_layer_param_specs[param_id].type_id);     break;
	case PARAM_WP_LABELS:               rv = SGVariant(this->painter->draw_wp_labels);                   break;
	case PARAM_TRK_BG_COLOR:            rv = SGVariant(this->painter->track_bg_color);                   break;
	case PARAM_TRACK_DRAW_SPEED_FACTOR: rv = SGVariant(this->painter->track_draw_speed_factor);          break;
	case PARAM_TRACK_SORT_ORDER:        rv = SGVariant((int32_t) this->track_sort_order, trw_layer_param_specs[param_id].type_id);                break;

	case PARAM_DRAW_WP_IMAGES:          rv = SGVariant(this->painter->draw_wp_images);       break;
	case PARAM_WP_IMAGE_SIZE:           rv = SGVariant((int32_t) this->painter->wp_image_size, trw_layer_param_specs[param_id].type_id);       break;
	case PARAM_WP_IMAGE_ALPHA:          rv = SGVariant((int32_t) this->painter->wp_image_alpha, trw_layer_param_specs[param_id].type_id);      break;
	case PARAM_WP_IMAGE_CACHE_CAPACITY: rv = SGVariant((int32_t) this->wp_image_cache.get_capacity_megabytes(), trw_layer_param_specs[param_id].type_id); break;

	case PARAM_WP_MARKER_COLOR:         rv = SGVariant(this->painter->wp_marker_color);      break;
	case PARAM_WP_LABEL_FG_COLOR:       rv = SGVariant(this->painter->wp_label_fg_color);    break;
	case PARAM_WP_LABEL_BG_COLOR:       rv = SGVariant(this->painter->wp_label_bg_color);    break;
	case PARAM_WPBA:                    rv = SGVariant(this->painter->wpbgand);              break;
	case PARAM_WP_MARKER_TYPE:          rv = SGVariant((int32_t) this->painter->wp_marker_type,     trw_layer_param_specs[param_id].type_id);  break;
	case PARAM_WP_MARKER_SIZE:          rv = SGVariant((int32_t) this->painter->wp_marker_size,     trw_layer_param_specs[param_id].type_id);  break;
	case PARAM_DRAW_WP_SYMBOLS:         rv = SGVariant(this->painter->draw_wp_symbols);               break;
	case PARAM_WP_LABEL_FONT_SIZE:      rv = SGVariant((int32_t) this->painter->wp_label_font_size, trw_layer_param_specs[param_id].type_id);  break;
	case PARAM_WP_SORT_ORDER:           rv = SGVariant((int32_t) this->wp_sort_order,               trw_layer_param_specs[param_id].type_id);  break;

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
			rv = SGVariant(this->metadata->iso8601_timestamp.toString(Qt::ISODate));
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
		(*iter)->marshall(helper_pickle); /* TODO_LATER: the marshall() function needs to put sublayer type into helper_pickle. */
		if (helper_pickle.data_size() > 0) {
			pickle.put_pickle(helper_pickle);
		}
		helper_pickle.clear();
	}


	for (auto iter = this->tracks.children_list.begin(); iter != this->tracks.children_list.end(); iter++) {
		(*iter)->marshall(helper_pickle); /* TODO_LATER: the marshall() function needs to put sublayer type into helper_pickle. */
		if (helper_pickle.data_size() > 0) {
			pickle.put_pickle(helper_pickle);
		}
		helper_pickle.clear();
	}


	for (auto iter = this->routes.children_list.begin(); iter != this->routes.children_list.end(); iter++) {
		(*iter)->marshall(helper_pickle); /* TODO_LATER: the marshall() function needs to put sublayer type into helper_pickle. */
		if (helper_pickle.data_size() > 0) {
			pickle.put_pickle(helper_pickle);
		}
		helper_pickle.clear();
	}
}




Layer * LayerTRWInterface::unmarshall(Pickle & pickle, GisViewport * gisview)
{
	LayerTRW * trw = new LayerTRW();
	trw->set_coord_mode(gisview->get_coord_mode());

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

			const QString type_id_string = pickle.peek_string(sizeof (pickle_size_t)); /* Look at type id string that is after object size. */
			SGObjectTypeID type_id = SGObjectTypeID(type_id_string.toUtf8().constData());

			/* Also remember to (attempt to) convert each
			   coordinate in case this is pasted into a
			   different track_drawing_mode. */
			if (type_id == Track::type_id()) {
				Track * trk = Track::unmarshall(pickle);
				/* Unmarshalling already sets track name, so we don't have to do it here. */
				trw->add_track(trk);
				trk->change_coord_mode(trw->coord_mode);
			} else if (type_id == Waypoint::type_id()) {
				Waypoint * wp = Waypoint::unmarshall(pickle);
				/* Unmarshalling already sets waypoint name, so we don't have to do it here. */
				trw->add_waypoint(wp);
				wp->convert(trw->coord_mode);
			} else if (type_id == Route::type_id()) {
				Track * trk = Track::unmarshall(pickle);
				/* Unmarshalling already sets route name, so we don't have to do it here. */
				trw->add_route(trk);
				trk->change_coord_mode(trw->coord_mode);
			} else {
				qDebug() << SG_PREFIX_E << "Invalid sublayer type id" << type_id;
			}
		}
		consumed_length += pickle.peek_size() + sizeof_len_and_subtype;
#ifdef TODO_LATER
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




void LayerTRW::draw_tree_item(GisViewport * gisview, bool highlight_selected, bool parent_is_selected)
{
	/* Check the layer for visibility (including all the parents' visibilities). */
	if (!this->tree_view->get_tree_item_visibility_with_parents(this)) {
		return;
	}

	const bool item_is_selected = parent_is_selected || g_selected.is_in_set(this);

	/* This will copy viewport's parameters (size, coords, etc.)
	   to painter, so that painter knows whether, what and how to
	   paint. */
	this->painter->set_viewport(gisview);

	if (this->tracks.is_visible()) {
		qDebug() << SG_PREFIX_I << "Calling function to draw tracks, highlight:" << highlight_selected << item_is_selected;
		this->tracks.draw_tree_item(gisview, highlight_selected, item_is_selected);
	}

	if (this->routes.is_visible()) {
		qDebug() << SG_PREFIX_I << "Calling function to draw routes, highlight:" << highlight_selected << item_is_selected;
		this->routes.draw_tree_item(gisview, highlight_selected, item_is_selected);
	}

	if (this->waypoints.is_visible()) {
		qDebug() << SG_PREFIX_I << "Calling function to draw waypoints, highlight:" << highlight_selected << item_is_selected;
		this->waypoints.draw_tree_item(gisview, highlight_selected, item_is_selected);
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

		Time ts_first;
		Time ts_last;
		if (sg_ret::ok == trk->get_timestamps(ts_first, ts_last)) {

			/* Update the earliest / the latest timestamps
			   (initialize if necessary). */
			if ((!result.start_time.is_valid()) || ts_first < result.start_time) {
				result.start_time = ts_first;
			}

			if ((!result.end_time.is_valid()) || ts_last > result.end_time) {
				result.end_time = ts_last;
			}

			/* Keep track of total time.
			   There maybe gaps within a track (eg segments)
			   but this should be generally good enough for a simple indicator. */
			result.duration += Time::get_abs_duration(ts_last, ts_first);
		}
	}

	return result;
}




/*
  Generate tooltip text for the layer
*/
QString LayerTRW::get_tooltip(void) const
{
	QString result = QObject::tr("Number of tracks: %1\n"
				     "Number of waypoints: %2\n"
				     "Number of routes: %3")
		.arg(this->tracks.size())
		.arg(this->waypoints.size())
		.arg(this->routes.size());

	QString tracks_info;
	if (!this->tracks.empty()) {
		TracksTooltipData ttd = this->get_tracks_tooltip_data();

		QDateTime date_start;
		date_start.setTime_t(ttd.start_time.get_ll_value());

		QDateTime date_end;
		date_end.setTime_t(ttd.end_time.get_ll_value());

		result += QObject::tr("\n\n"
				      "Tracks time span: %1 to %2").arg(date_start.toString(Qt::SystemLocaleLongDate)).arg(date_end.toString(Qt::SystemLocaleLongDate));

		if (ttd.duration.is_valid() && !ttd.duration.is_zero()) {
			result += QObject::tr("\n"
					      "Tracks total duration: %1").arg(ttd.duration.to_string());
		}
		if (ttd.length.is_valid() && !ttd.length.is_zero()) {
			result += QObject::tr("\n"
					      "Tracks total length: %1").arg(ttd.length.convert_to_unit(Preferences::get_unit_distance()).to_nice_string());
		}
	}


	QString routes_info;
	if (!this->routes.empty()) {
		const Distance rlength = this->get_routes_tooltip_data(); /* [meters] */
		if (rlength.is_valid()) {
			/* Prepare track info dependent on distance units. */
			const QString distance_string = rlength.convert_to_unit(Preferences::get_unit_distance()).to_nice_string();
			result += QObject::tr("\n\n"
					      "Routes total length: %1").arg(distance_string);
		}
	}

	return result;
}




#define VIK_SETTINGS_TRKPT_SELECTED_STATUSBAR_FORMAT "trkpt_selected_statusbar_format"

/**
   Function to show track point information on the statusbar.
   Items displayed is controlled by the settings format code.
*/
void LayerTRW::set_statusbar_msg_info_tp(const TrackpointReference & tp_ref, Track * track)
{
	if (!tp_ref.m_iter_valid) {
		qDebug() << SG_PREFIX_W << "Trying to set info about invalid tp";
		return;
	}

	Trackpoint * tp_prev = NULL;

	QString statusbar_format_code;
	if (!ApplicationState::get_string(VIK_SETTINGS_TRKPT_SELECTED_STATUSBAR_FORMAT, statusbar_format_code)) {
		/* Otherwise use default. */
		statusbar_format_code = "KEATDN";
	}

	/* Format code may want to show speed - so may need previous trkpt to work it out. */
	auto iter = std::prev(tp_ref.m_iter);
	tp_prev = iter == track->end() ? NULL : *iter;

	Trackpoint * tp = tp_ref.m_iter == track->end() ? NULL : *tp_ref.m_iter;
	const QString msg = vu_trackpoint_formatted_message(statusbar_format_code, tp, tp_prev, track, NAN);
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
	const QString coord_string = wp->get_coord().to_string();

	/* Combine parts to make overall message. */
	QString msg;
	if (!wp->comment.isEmpty()) {
		/* Add comment if available. */
		msg = tr("%1 | %2 | Comment: %3").arg(alti_string_uu).arg(coord_string).arg(wp->comment);
	} else {
		msg = tr("%1 | %2").arg(alti_string_uu).arg(coord_string);
	}
	this->get_window()->get_statusbar()->set_message(StatusBarField::Info, msg);
}




void LayerTRW::reset_internal_selections(void)
{
	qDebug() << SG_PREFIX_I << "Will reset in-layer selection info";

	bool changed = false;

	changed = changed || this->selected_track_reset();
	changed = changed || this->selected_wp_reset();

	if (changed) {
		this->emit_tree_item_changed("Selections changed when resetting TRW layer's internal selections");
	}

}




bool LayerTRW::is_empty(void) const
{
	return this->tracks.empty() && this->routes.empty() && this->waypoints.empty();
}




bool LayerTRW::get_tracks_visibility(void) const
{
	return this->tracks.is_visible();
}




bool LayerTRW::get_routes_visibility(void) const
{
	return this->routes.is_visible();
}




bool LayerTRW::get_waypoints_visibility(void) const
{
	return this->waypoints.is_visible();
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
	result.expand_with_bbox(intermediate);

	intermediate = this->routes.get_bbox();
	result.expand_with_bbox(intermediate);

	intermediate = this->waypoints.get_bbox();
	result.expand_with_bbox(intermediate);

	return result;
}




bool LayerTRW::find_center(Coord * dest)
{
	if (this->get_bbox().is_valid()) {
		*dest = Coord(this->get_bbox().get_center_lat_lon(), this->coord_mode);
		return true;
	} else {
		return false;
	}
}




void LayerTRW::centerize_cb(void)
{
	Coord coord;
	if (this->find_center(&coord)) {
		this->request_new_viewport_center(ThisApp::get_main_gis_view(), coord);
	} else {
		Dialog::info(tr("This layer has no waypoints or trackpoints."), this->get_window());
	}
}




bool LayerTRW::move_viewport_to_show_all(GisViewport * gisview)
{
	if (this->get_bbox().is_valid()) {
		gisview->set_bbox(this->get_bbox());
		gisview->request_redraw("Re-align viewport to show whole contents of TRW Layer");
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

	GisViewport * gisview = ThisApp::get_main_gis_view();
	if (this->move_viewport_to_show_all(gisview)) {
		gisview->request_redraw("Redrawing viewport after re-aligning it to show all of TRW Layer");
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
			ThisApp::get_main_gis_view()->set_center_coord(wp->get_coord());
			this->tree_view->select_and_expose_tree_item(wp);
			ThisApp::get_main_gis_view()->request_redraw("Redrawing items after setting new center in viewport");

			break;
		}

	}
}




bool LayerTRW::new_waypoint(const Coord & default_coord, bool & visible_with_parents, Window * parent_window)
{
	visible_with_parents = false;

	/* Notice that we don't handle situation when returned default
	   name is invalid. The new name in properties dialog will
	   simply be empty. */
	const QString default_name = this->waypoints.name_generator.try_new_name();

	Waypoint * wp = new Waypoint(default_coord);

	/* Attempt to auto set height if DEM data is available. */
	wp->apply_dem_data(true);

	this->add_waypoint(wp);
	visible_with_parents = wp->is_visible_with_parents();

	qDebug() << SG_PREFIX_I << "Will show properties dialog for wp with coord" << wp->get_coord();
	return wp->show_properties_dialog();
}




void LayerTRW::acquire_from_wikipedia_waypoints_viewport_cb(void) /* Slot. */
{
	GisViewport * gisview = ThisApp::get_main_gis_view();

	Geonames::create_wikipedia_waypoints(this, gisview->get_bbox(), this->get_window());
	this->waypoints.recalculate_bbox();

	this->emit_tree_item_changed("Redrawing items after adding wikipedia waypoints");
}




void LayerTRW::acquire_from_wikipedia_waypoints_layer_cb(void) /* Slot. */
{
	Geonames::create_wikipedia_waypoints(this, this->get_bbox(), this->get_window());
	this->waypoints.recalculate_bbox();

	this->emit_tree_item_changed("Redrawing items after adding wikipedia waypoints");
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
	this->acquire_handler(new DataSourceGeoCache(ThisApp::get_main_gis_view()));
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
	Track * trk = NULL;
	GPSTransferType xfer_type = GPSTransferType::TRK; /* "sg.trw.tracks" = 0 so hard to test different from NULL! */
	bool xfer_all = false;

	if (sublayer) {
		/* Upload only specified sublayer, not whole layer. */
		xfer_all = false;
		if (sublayer->m_type_id == Track::type_id()) {
			trk = (Track *) sublayer;
			xfer_type = GPSTransferType::TRK;

		} else if (sublayer->get_type_id() == Route::type_id()) {
			trk = (Track *) sublayer;
			xfer_type = GPSTransferType::RTE;

		} else if (sublayer->get_type_id() == Waypoint::type_id()) {
			xfer_type = GPSTransferType::WPT;

		} else if (sublayer->get_type_id() == LayerTRWRoutes::type_id()) {
			xfer_type = GPSTransferType::RTE;
		}
	} else {
		/* No specific sublayer, so upload whole layer. */
		xfer_all = true;
	}

	if (trk && !trk->is_visible()) {
		Dialog::error(tr("Can not upload invisible track."), this->get_window());
		return;
	}


	DataSourceGPSDialog gps_upload_config(QObject::tr("Upload"), xfer_type, xfer_all);
	if (gps_upload_config.exec() != QDialog::Accepted) {
		return;
	}
	gps_upload_config.save_transfer_options(); /* TODO_LATER: hide this call inside of DataSourceGPSDialog::exec() */


	/* Apply settings to transfer to the GPS device. */
	gps_upload_config.transfer.run_transfer(this, trk, ThisApp::get_main_gis_view(), false);
}




void LayerTRW::new_waypoint_cb(void) /* Slot. */
{
	bool visible_with_parents = false;

	if (this->new_waypoint(ThisApp::get_main_gis_view()->get_center_coord(), visible_with_parents, this->get_window())) {
		this->waypoints.recalculate_bbox();
		/* We don't have direct access to added waypoint, so
		   we can't call ::emit_tree_item_changed(). But we
		   have access to 'waypoints' node, so let's use
		   that. */
		if (visible_with_parents) {
			this->waypoints.emit_tree_item_changed("Redrawing 'waypoints' node after adding waypoint");
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

	this->selected_track_set(track);

	this->add_track(track);

	return track;
}




void LayerTRW::new_track_cb() /* Slot. */
{
	if (!this->selected_track_get()) {

		/* This QAction for this slot shouldn't even be active when a track/route is already being created. */

		const QString uniq_name = this->new_unique_element_name(Track::type_id(), tr("Track")) ;
		this->new_track_create_common(uniq_name);

		this->get_window()->activate_tool_by_id(LayerToolTRWNewTrack::tool_id());
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

	this->selected_track_set(route);

	this->add_route(route);

	return route;
}




void LayerTRW::new_route_cb(void) /* Slot. */
{
	if (!this->selected_track_get()) {

		/* This QAction for this slot shouldn't even be active when a track/route is already being created. */

		const QString uniq_name = this->new_unique_element_name(Route::type_id(), tr("Route")) ;
		this->new_route_create_common(uniq_name);

		this->get_window()->activate_tool_by_id(LayerToolTRWNewRoute::tool_id());
	}
}





void LayerTRW::finish_track_cb(void) /* Slot. */
{
	this->reset_track_creation_in_progress();
	this->selected_track_reset();
	this->route_finder_started = false;
	this->emit_tree_item_changed("TRW - finish track");
}




void LayerTRW::finish_route_cb(void) /* Slot. */
{
	this->reset_route_creation_in_progress();
	this->selected_track_reset();
	this->route_finder_started = false;
	this->emit_tree_item_changed("TRW - finish route");
}




void LayerTRW::upload_to_osm_traces_cb(void) /* Slot. */
{
	OSMTraces::upload_trw_layer(this, NULL);
}




sg_ret LayerTRW::attach_to_container(Track * trk)
{
	if (trk->is_route()) {
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
	/* TODO_LATER: when adding multiple waypoints (e.g. during acquire), sorting children here will make acquire take more time. */
	this->tree_view->sort_children(&this->waypoints, this->wp_sort_order);


	return sg_ret::ok;
}




sg_ret LayerTRW::add_waypoint(Waypoint * wp)
{
	this->attach_to_container(wp);
	this->attach_to_tree(wp);

	QObject::connect(wp, SIGNAL (tree_item_changed(const QString &)), this, SLOT (child_tree_item_changed_cb(const QString &)));

	return sg_ret::ok;
}




sg_ret LayerTRW::attach_to_tree(Track * trk)
{
	if (!this->is_in_tree()) {
		qDebug() << SG_PREFIX_W << "This layer" << this->name << "is not connected to tree, will now connect it";
		ThisApp::get_layers_panel()->get_top_layer()->add_layer(this, true);
	}

	if (trk->is_route()) {
		if (!this->routes.is_in_tree()) {
			qDebug() << SG_PREFIX_I << "Attaching to tree item" << this->routes.name << "under" << this->name;
			this->tree_view->attach_to_tree(this, &this->routes);
		}

		/* Now we can proceed with adding a route to Routes node. */

		qDebug() << SG_PREFIX_I << "Attaching to tree item" << trk->name << "under" << this->routes.name;
		this->tree_view->attach_to_tree(&this->routes, trk);

		/* Update tree item properties of this item before sorting of all sibling tree items. */
		trk->update_tree_item_properties();

		/* Sort now as post_read is not called on a route connected to tree. */
		this->tree_view->sort_children(&this->routes, this->track_sort_order);
	} else {
		if (!this->tracks.is_in_tree()) {
			qDebug() << SG_PREFIX_I << "Attaching to tree item" << this->tracks.name << "under" << this->name;
			this->tree_view->attach_to_tree(this, &this->tracks);
		}


		Trackpoint * tp = trk->get_tp_first();
		if (tp && tp->timestamp.is_valid()) {
			trk->set_timestamp(tp->timestamp);
		}

		qDebug() << SG_PREFIX_I << "Attaching to tree item" << trk->name << "under" << this->tracks.name;
		this->tree_view->attach_to_tree(&this->tracks, trk); /* push item to the end of parent nodes. */

		/* Update tree item properties of this item before sorting of all sibling tree items. */
		trk->update_tree_item_properties();

		/* Sort now as post_read is not called on a track connected to tree. */
		this->tree_view->sort_children(&this->tracks, this->track_sort_order);
	}

	return sg_ret::ok;
}




sg_ret LayerTRW::add_track(Track * trk)
{
	this->attach_to_container(trk);
	this->attach_to_tree(trk);

	QObject::connect(trk, SIGNAL (tree_item_changed(const QString &)), this, SLOT (child_tree_item_changed_cb(const QString &)));

	return sg_ret::ok;
}




sg_ret LayerTRW::add_route(Track * trk)
{
	this->attach_to_container(trk);
	this->attach_to_tree(trk);

	QObject::connect(trk, SIGNAL (tree_item_changed(const QString &)), this, SLOT (child_tree_item_changed_cb(const QString &)));

	return sg_ret::ok;
}




/*
  To be called whenever a track has been deleted or may have been changed.
  Deselect current trackpoint on given track, but don't deselect the track itself.
*/
void LayerTRW::deselect_current_trackpoint(Track * trk)
{
	Track * selected_track = this->selected_track_get();
	if (NULL == selected_track) {
		return;
	}

	if (selected_track != trk) {
		return;
	}

	const bool was_set = selected_track->selected_tp_reset();
	if (was_set) {
		this->emit_tree_item_changed("TRW layer has changed after deselecting a trackpoint");
	}

	return;
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
		wp->set_symbol_name(tmp_symbol_name);
	}
}




/**
   \brief Get a a unique new name for element of type \param item_type_id
*/
QString LayerTRW::new_unique_element_name(const SGObjectTypeID & item_type_id, const QString & old_name)
{
	if (item_type_id == Track::type_id()) {
		return this->tracks.new_unique_element_name(old_name);

	} else if (item_type_id == Waypoint::type_id()) {
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

	QObject::connect(wp, SIGNAL (tree_item_changed(const QString &)), this, SLOT (child_tree_item_changed_cb(const QString &)));
}




void LayerTRW::add_track_from_file(Track * trk)
{
	Track * curr_track = this->selected_track_get();

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

		curr_track->move_trackpoints_from(*trk, trk->begin(), trk->end());
		trk->free();
		this->route_finder_append = false; /* This means we have added it. */
	} else {
		/* No more uniqueness of name forced when loading from a file. */
		if (trk->is_route()) {
			this->routes.attach_to_container(trk);
		} else {
			this->tracks.attach_to_container(trk);
		}
		QObject::connect(trk, SIGNAL (tree_item_changed(const QString &)), this, SLOT (child_tree_item_changed_cb(const QString &)));

		if (this->route_finder_check_added_track) {
			trk->remove_dup_points(); /* Make "double point" track work to undo. */
			this->route_finder_added_track = trk;
		}
	}
}




sg_ret LayerTRW::drag_drop_request(TreeItem * tree_item, int row, int col)
{
	LayerTRW * source_trw = (LayerTRW *) tree_item->get_owning_layer();
	const bool the_same_parent = TreeItem::the_same_object(this, source_trw);

	/* Handle item in old location. */
	{
		if (tree_item->get_type_id() == Track::type_id()) {
			source_trw->detach_from_container((Track *) tree_item);

		} else if (tree_item->get_type_id() == Route::type_id()) {
			source_trw->detach_from_container((Track *) tree_item);

		} else if (tree_item->get_type_id() == Waypoint::type_id()) {
			source_trw->detach_from_container((Waypoint *) tree_item);
		} else {
			qDebug() << SG_PREFIX_E << "Unexpected type id" << tree_item->m_type_id << "of item" << tree_item->name;
			return sg_ret::err;
		}

		tree_item->disconnect(); /* Disconnect everything connected to object's signals. */

		/* Detaching of tree item from tree view will be handled by QT. */
	}

	/* Handle item in new location. */
	{
		/* We are using
		   LayerTRW::add_waypoint()/add_track()/add_route()
		   here so that the layer can create and attach
		   Tracks/Routes/Waypoints nodes if necessary. */

		if (tree_item->get_type_id() == Track::type_id()) {

			this->add_track((Track *) tree_item);

			this->tracks.recalculate_bbox();
			if (!the_same_parent) {
				source_trw->tracks.recalculate_bbox();
			}

		} else if (tree_item->get_type_id() == Route::type_id()) {
			this->add_route((Track *) tree_item);

			this->routes.recalculate_bbox();
			if (!the_same_parent) {
				source_trw->routes.recalculate_bbox();
			}

		} else if (tree_item->get_type_id() == Waypoint::type_id()) {
			this->add_waypoint((Waypoint *) tree_item);

			this->waypoints.recalculate_bbox();
			if (!the_same_parent) {
				source_trw->waypoints.recalculate_bbox();
			}
		} else {
			qDebug() << SG_PREFIX_E << "Unexpected type id" << tree_item->m_type_id << "of item" << tree_item->name;
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

	if (TreeItemType::Sublayer != tree_item->get_tree_item_type()) {
		*result = false;
		return sg_ret::ok;
	}

	if (tree_item->get_type_id() == Track::type_id()
	    || tree_item->get_type_id() == Route::type_id()
	    || tree_item->get_type_id() == Waypoint::type_id()) {

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

	if (trk->is_route()) {
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
	   TODO_LATER: this sometimes doesn't work. */
	if (tree_item->get_type_id() == Track::type_id()) {
		if (this->tracks.size() == 0) {
			this->tree_view->detach_tree_item(&this->tracks);
		}
	} else if (tree_item->get_type_id() == Route::type_id()) {
		if (this->routes.size() == 0) {
			this->tree_view->detach_tree_item(&this->routes);
		}
	} else if (tree_item->get_type_id() == Waypoint::type_id()) {
		if (this->waypoints.size() == 0) {
			this->tree_view->detach_tree_item(&this->waypoints);
		}
	} else {
		qDebug() << SG_PREFIX_E << "Unexpected tree item type" << tree_item->m_type_id;
		return sg_ret::err;
	}

	return sg_ret::ok;
}




void LayerTRW::delete_all_routes()
{
	this->route_finder_added_track = NULL;

	Track * track = this->selected_track_get();
	if (NULL != track && track->is_route()) { /* Be careful not to reset current track. */
		this->selected_track_reset();
	}

	for (auto iter = this->routes.children_list.begin(); iter != this->routes.children_list.end(); iter++) {
		this->tree_view->detach_tree_item(*iter);
	}
	this->tree_view->detach_tree_item(&this->routes);
	this->routes.set_visible(false); /* There is no such item in tree anymore. */

	/* Don't try (for now) to verify if ->selected_tree_item was
	   set to this item or any of its children, or to anything
	   else. Just clear the selection when deleting any item from
	   the tree. */
	g_selected.clear();

	this->routes.clear();

	this->emit_tree_item_changed("TRW - delete all tracks");
}




void LayerTRW::delete_all_tracks()
{
	this->route_finder_added_track = NULL;

	Track * track = this->selected_track_get();
	if (NULL != track && track->is_track()) { /* Be careful not to reset current route. */
		this->selected_track_reset();
	}

	for (auto iter = this->tracks.children_list.begin(); iter != this->tracks.children_list.end(); iter++) {
		this->tree_view->detach_tree_item(*iter);
	}
	this->tree_view->detach_tree_item(&this->tracks);
	this->tracks.set_visible(false); /* There is no such item in tree anymore. */

	/* Don't try (for now) to verify if ->selected_tree_item was
	   set to this item or any of its children, or to anything
	   else. Just clear the selection when deleting any item from
	   the tree. */
	g_selected.clear();

	this->tracks.clear();

	this->emit_tree_item_changed("TRW - delete all routes");
}




void LayerTRW::delete_all_waypoints()
{
	this->selected_wp_reset();
	this->moving_wp = false;

	this->waypoints.name_generator.reset();

	for (auto iter = this->waypoints.children_list.begin(); iter != this->waypoints.children_list.end(); iter++) {
		this->tree_view->detach_tree_item(*iter);
	}
	this->tree_view->detach_tree_item(&this->waypoints);
	this->waypoints.set_visible(false); /* There is no such item in tree anymore. */

	/* Don't try (for now) to verify if ->selected_tree_item was
	   set to this item or any of its children, or to anything
	   else. Just clear the selection when deleting any item from
	   the tree. */
	g_selected.clear();

	this->waypoints.clear();

	this->emit_tree_item_changed("TRW - delete all waypoints");
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
	if (item->get_type_id() == Waypoint::type_id()) {
		Waypoint * wp = (Waypoint *) item;
		this->delete_waypoint(wp, confirm);

	} else if (item->get_type_id() == Track::type_id()
		   || item->get_type_id() == Route::type_id()) {
		Track * trk = (Track *) item;
		this->delete_track(trk, confirm);
	} else {
		qDebug() << SG_PREFIX_E << "Unexpected sublayer type" << item->m_type_id;
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
	this->tp_show_properties_dialog();
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





/**
 * Attempt to merge selected track with other tracks specified by the user
 * Tracks to merge with must be of the same 'type' as the selected track -
 *  either all with timestamps, or all without timestamps
 */
void LayerTRW::merge_with_other_cb(void)
{
	Track * track = this->selected_track_get();
	if (!track) {
		qDebug() << SG_PREFIX_E << "Can't get edited track in track-related function" << __FUNCTION__;
		return;
	}
	if (track->empty()) {
		return;
	}

	const bool is_route = track->is_route();

	LayerTRWTracks & source_sublayer = is_route ? this->routes : this->tracks;

	/* with_timestamps: allow merging with 'similar' time type time tracks
	   i.e. either those times, or those without */
	const bool with_timestamps = track->get_tp_first()->timestamp.is_valid();
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
			track->move_trackpoints_from(*source_track, source_track->begin(), source_track->end());

			this->detach_from_container(source_track);
			this->detach_from_tree(source_track);
			delete source_track;

			track->sort(Trackpoint::compare_timestamps);
		}
	}
	this->emit_tree_item_changed("TRW - merge with other");
}




/**
 * Join - this allows combining 'tracks' and 'track routes'
 *  i.e. doesn't care about whether tracks have consistent timestamps
 * ATM can only append one track at a time to the currently selected track
 */
void LayerTRW::append_track_cb(void)
{
	Track * track = this->selected_track_get();
	if (!track) {
		qDebug() << SG_PREFIX_E << "Can't get edited track in track-related function";
		return;
	}

	const bool is_route = track->is_route();

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


	track->move_trackpoints_from(*source_track, source_track->begin(), source_track->end());


	/* All trackpoints have been moved from source_track to
	   target_track. We don't need source_track anymore. */
	this->detach_from_container(source_track);
	this->detach_from_tree(source_track);
	delete source_track;


	this->emit_tree_item_changed("TRW - append track");
}




/**
 * Very similar to append_track_cb() for joining
 * but this allows selection from the 'other' list
 * If a track is selected, then is shows routes and joins the selected one
 * If a route is selected, then is shows tracks and joins the selected one
 */
void LayerTRW::append_other_cb(void)
{
	Track * track = this->selected_track_get();
	if (!track) {
		qDebug() << SG_PREFIX_E << "Can't get edited track in track-related function" << __FUNCTION__;
		return;
	}

	const bool target_is_route = track->is_route();

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


	if (source_track->is_track()) {
		const Speed avg = source_track->get_average_speed();
		if (source_track->get_segment_count() > 1
		    || (avg.is_valid() && avg.is_positive())) {

			if (Dialog::yes_or_no(tr("Converting a track to a route removes extra track data such as segments, timestamps, etc...\nDo you want to continue?"), this->get_window())) {
				source_track->merge_segments();
				source_track->to_routepoints();
			} else {
				return;
			}
		}
	}


	track->move_trackpoints_from(*source_track, source_track->begin(), source_track->end());


	/* All trackpoints have been moved from
	   source_track to target_track. We don't need
	   source_track anymore. */
	this->detach_from_container(source_track);
	this->detach_from_tree(source_track);
	delete source_track;


	this->emit_tree_item_changed("TRW - append other");
}




/* Merge by segments. */
void LayerTRW::merge_by_segment_cb(void)
{
	Track * track = this->selected_track_get();
	if (!track) {
		qDebug() << SG_PREFIX_E << "Can't get edited track in track-related function" << __FUNCTION__;
		return;
	}

	/* Currently no need to redraw as segments not actually shown on the display.
	   However inform the user of what happened. */
	const unsigned int n_segments = track->merge_segments();
	const QString msg = QObject::tr("%n segments merged", "", n_segments); /* TODO_MAYBE: verify that "%n" format correctly handles unsigned int. */
	Dialog::info(msg, this->get_window());
}




/* merge by time routine */
void LayerTRW::merge_by_timestamp_cb(void)
{
	Track * orig_track = this->selected_track_get();
	if (!orig_track) {
		qDebug() << SG_PREFIX_E << "Can't get edited track in track-related function" << __FUNCTION__;
		return;
	}

	if (!orig_track->empty()
	    && !orig_track->get_tp_first()->timestamp.is_valid()) {
		Dialog::error(tr("Failed. This track does not have timestamp"), this->get_window());
		return;
	}

	std::list<Track *> tracks_with_timestamp = this->tracks.find_tracks_with_timestamp_type(true, orig_track);
	if (tracks_with_timestamp.empty()) {
		Dialog::error(tr("Failed. No other track in this layer has timestamp"), this->get_window());
		return;
	}

	Duration threshold(60, DurationUnit::Seconds);
	if (false == Dialog::duration(tr("Merge Threshold..."),
				      tr("Merge when time between tracks is less than:"),
				      threshold,
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
		std::list<Track *> nearby_tracks = this->tracks.find_nearby_tracks_by_time(orig_track, threshold);

		/* Merge them. */

		for (auto iter = nearby_tracks.begin(); iter != nearby_tracks.end(); iter++) {
			/* Remove trackpoints from merged track, delete track. */
			orig_track->move_trackpoints_from(**iter, (*iter)->begin(), (*iter)->end());

			this->detach_from_container(*iter);
			this->detach_from_tree(*iter);
			delete *iter;

			/* Tracks have changed, therefore retry again against all the remaining tracks. */
			attempt_merge = true;
		}

		orig_track->sort(Trackpoint::compare_timestamps);
	}

	this->emit_tree_item_changed("TRW merge by timestamp");
}




/* end of split/merge routines */




/**
 * Open a program at the specified date
 * Mainly for RedNotebook - http://rednotebook.sourceforge.net/
 * But could work with any program that accepts a command line of --date=<date>
 * FUTURE: Allow configuring of command line options + date format
 */
void LayerTRW::diary_open(const QString & date_str)
{
	GError *err = NULL;
	const QString cmd = QString("%1 %2%3").arg(diary_program).arg("--date=").arg(date_str);
	if (!g_spawn_command_line_async(cmd.toUtf8().constData(), &err)) {
		Dialog::error(tr("Could not launch %1 to open file.").arg(diary_program), this->get_window());
		g_error_free(err);
	}
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

	this->emit_tree_item_changed("TRW - delete selected tracks");
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

	this->emit_tree_item_changed("TRW - delete selected routes");
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
	this->emit_tree_item_changed("TRW - delete selected waypoints");

}





/**
   @reviewed-on 2019-12-01
*/
sg_ret LayerTRW::get_tree_items(std::list<TreeItem *> & list, const std::list<SGObjectTypeID> & wanted_types) const
{
	for (auto iter = wanted_types.begin(); iter != wanted_types.end(); ++iter) {
		if (*iter ==  Track::type_id()) {
			this->tracks.get_tree_items(list);

		} else if (*iter == Route::type_id()) {
			this->routes.get_tree_items(list);

		} else if (*iter == Waypoint::type_id()) {
			this->waypoints.get_tree_items(list);

		} else {
			qDebug() << SG_PREFIX_E << "Unexpected type id" << *iter;
			return sg_ret::err;
		}
	}

	return sg_ret::ok;
}




/**
   @reviewed on 2019-12-01
*/
void LayerTRW::tracks_stats_cb(void)
{
	const std::list<SGObjectTypeID> wanted_types = { Track::type_id() };
	layer_trw_show_stats(this->name, this, wanted_types, this->get_window());
}




/**
   @reviewed on 2019-12-01
*/
void LayerTRW::routes_stats_cb(void)
{
	const std::list<SGObjectTypeID> wanted_types = { Route::type_id() };
	layer_trw_show_stats(this->name, this, wanted_types, this->get_window());
}




bool SlavGPS::is_valid_geocache_name(const char * str)
{
	const size_t len = strlen(str);
	return len >= 3 && len <= 7 && str[0] == 'G' && str[1] == 'C' && isalnum(str[2]) && (len < 4 || isalnum(str[3])) && (len < 5 || isalnum(str[4])) && (len < 6 || isalnum(str[5])) && (len < 7 || isalnum(str[6]));
}




void LayerTRW::on_tp_properties_dialog_tp_coordinates_changed_cb(void)
{
	this->emit_tree_item_changed("Indicating change of edited trackpoint's coordinates");
}




void LayerTRW::on_wp_properties_dialog_wp_coordinates_changed_cb(void)
{
	this->emit_tree_item_changed("Indicating change of edited waypoint's coordinates");
}



void LayerTRW::tp_show_properties_dialog()
{
	LayerToolTRWEditTrackpoint * tool = (LayerToolTRWEditTrackpoint *) ThisApp::get_main_window()->get_toolbox()->get_tool(LayerToolTRWEditTrackpoint::tool_id());
	Window * window = ThisApp::get_main_window();

	/* Signals. */
	{
		/* Disconnect all old connections that may have been
		   made from this global dialog to other TRW layer. */
		/* TODO_LATER: also disconnect the signals in dialog code when the dialog is closed? */
		tool->point_properties_dialog->disconnect();

		/* Make new connections to current TRW layer. */
		connect(tool->point_properties_dialog, SIGNAL (point_coordinates_changed()), this, SLOT (on_tp_properties_dialog_tp_coordinates_changed_cb()));
	}


	/* Show properties dialog. */
	{
		const CoordMode coord_mode = this->get_coord_mode();
		tool->point_properties_dialog->set_coord_mode(coord_mode);
		window->get_tools_dock()->setWidget(tool->point_properties_dialog);
		window->set_tools_dock_visibility_cb(true);
	}


	/* Fill properties dialog with current point. */
	{
		Track * track = this->selected_track_get();
		if (nullptr == track) {
			qDebug() << SG_PREFIX_W << "Parent layer doesn't have any 'edited' track set";
			Track::tp_properties_dialog_reset();
			return;
		}
		const size_t sel_tp_count = track->get_selected_children().get_count();
		if (1 != sel_tp_count) {
			qDebug() << SG_PREFIX_W << "Will reset trackpoint dialog data: selected tp count is not 1:" << sel_tp_count;
			Track::tp_properties_dialog_reset();
			return;
		}
		track->tp_properties_dialog_set();
		return;
	}
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
		this->layer->emit_tree_item_changed("TRW - thumbnail creator"); /* NB update from background thread. */
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
Time LayerTRW::get_timestamp(void) const
{
	const Time timestamp_tracks = this->tracks.get_earliest_timestamp();
	const Time timestamp_waypoints = this->waypoints.get_earliest_timestamp();
	/* NB routes don't have timestamps - hence they are not considered. */

	if (!timestamp_tracks.is_valid() && !timestamp_waypoints.is_valid()) {
		/* Fallback to get time from the metadata when no other timestamps available. */
		if (this->metadata && this->metadata->iso8601_timestamp.isValid()) {

			/* TODO_MAYBE: use toSecsSinceEpoch() when new version of QT library becomes more available. */
			return Time(this->metadata->iso8601_timestamp.toMSecsSinceEpoch() / MSECS_PER_SEC, Time::get_internal_unit());
		}
	}
	if (timestamp_tracks.is_valid() && !timestamp_waypoints.is_valid()) {
		return timestamp_tracks;
	}
	if (timestamp_tracks.is_valid() && timestamp_waypoints.is_valid() && (timestamp_tracks < timestamp_waypoints)) {
		return timestamp_tracks;
	}
	return timestamp_waypoints;
}




void LayerTRW::post_read(GisViewport * gisview, bool from_file)
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
	if (this->metadata && !this->metadata->iso8601_timestamp.isValid()) {
		const Time local_timestamp = this->get_timestamp();
		if (local_timestamp.is_valid()) {
			this->metadata->iso8601_timestamp.setMSecsSinceEpoch(local_timestamp.get_ll_value() * MSECS_PER_SEC); /* TODO_MAYBE: replace with setSecsSinceEpoch() in future. */
		} else {
			/* No time found - so use 'now' for the metadata time. */
			this->metadata->iso8601_timestamp = QDateTime::currentDateTime(); /* The method returns time in local time zone. */
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
bool LayerTRW::uniquify(void)
{
	this->tracks.uniquify(this->track_sort_order);
	this->routes.uniquify(this->track_sort_order);
	this->waypoints.uniquify(this->wp_sort_order);

	/* Update. */
	this->emit_tree_item_changed("Indicating change in Layer TRW after uniquifying");

	return true;
}




void LayerTRW::change_coord_mode(CoordMode dest_mode)
{
	if (this->coord_mode != dest_mode) {
		this->coord_mode = dest_mode;
		this->waypoints.change_coord_mode(dest_mode);
		this->tracks.change_coord_mode(dest_mode);
		this->routes.change_coord_mode(dest_mode);
	}


	Window * window = this->get_window();
	/* Some properties dialogs need to change their "coordinates"
	   widgets. Implementation of code changing the mode ensures
	   that the mode is changed only when current mode is
	   different than expected. */
	{
		LayerToolTRWEditWaypoint * tool = (LayerToolTRWEditWaypoint *) window->get_toolbox()->get_tool(LayerToolTRWEditWaypoint::tool_id());
		tool->change_coord_mode(dest_mode);
	}
	{
		LayerToolTRWEditTrackpoint * tool = (LayerToolTRWEditTrackpoint *) window->get_toolbox()->get_tool(LayerToolTRWEditTrackpoint::tool_id());
		tool->change_coord_mode(dest_mode);
	}
}




/* ----------- Downloading maps along tracks --------------- */

void vik_track_download_map(Track * trk, LayerMap * layer_map, const VikingScale & viking_scale)
{
	std::list<Rect *> rectangles_to_download = trk->get_map_rectangles(viking_scale);
	if (rectangles_to_download.empty()) {
		return;
	}

	for (auto iter = rectangles_to_download.begin(); iter != rectangles_to_download.end(); iter++) {
		layer_map->download_section((*iter)->tl, (*iter)->br, viking_scale);
	}

	for (auto iter = rectangles_to_download.begin(); iter != rectangles_to_download.end(); iter++) {
		delete *iter;
	}
}




void LayerTRW::download_map_along_track_cb(void)
{
	std::vector<VikingScale> viking_scales = {
		VikingScale(0.125),
		VikingScale(0.25),
		VikingScale(0.5),
		VikingScale(1),
		VikingScale(2),
		VikingScale(4),
		VikingScale(8),
		VikingScale(16),
		VikingScale(32),
		VikingScale(64),
		VikingScale(128),
		VikingScale(256),
		VikingScale(512),
		VikingScale(1024) };

	LayersPanel * panel = ThisApp::get_layers_panel();
	const GisViewport * gisview = ThisApp::get_main_gis_view();

	Track * track = this->selected_track_get();
	if (!track) {
		return;
	}

	const std::list<Layer const *> layers = panel->get_all_layers_of_kind(LayerKind::Map, true); /* Includes hidden map layer types. */
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



	const VikingScale current_viking_scale(gisview->get_viking_scale().get_x());
	int default_zoom_idx = 0;
	if (sg_ret::ok != VikingScale::get_closest_index(default_zoom_idx, viking_scales, current_viking_scale)) {
		qDebug() << SG_PREFIX_W << "Failed to get the closest Viking scale";
		default_zoom_idx = viking_scales.size() - 1;
	}



	MapAndZoomDialog dialog(QObject::tr("Download along track"), map_labels, viking_scales, this->get_window());
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

	vik_track_download_map(track, *iter, viking_scales[selected_zoom_idx]);

	return;
}




/**
   @reviewed on 2019-12-01
*/
void LayerTRW::track_and_route_list_dialog_cb(void)
{
	const std::list<SGObjectTypeID> wanted_types = { Track::type_id(), Route::type_id() };
	const QString title = tr("%1: Tracks and Routes List").arg(this->name);
	Track::list_dialog(title, this, wanted_types);
}




/**
   @reviewed on 2019-12-01
*/
void LayerTRW::waypoint_list_dialog_cb(void) /* Slot. */
{
	const QString title = tr("%1: Waypoints List").arg(this->name);
	Waypoint::list_dialog(title, this);
}




LayerDataReadStatus LayerTRW::read_layer_data(QFile & file, const QString & dirpath)
{
	qDebug() << SG_PREFIX_D << "Will call gpspoint_read_file() to read Layer Data";
	return GPSPoint::read_layer_from_file(file, this, dirpath);
}




SaveStatus LayerTRW::write_layer_data(FILE * file) const
{
	fprintf(file, "\n\n~LayerData\n");
	const SaveStatus rv = GPSPoint::write_layer_to_file(file, this);
	fprintf(file, "~EndLayerData\n");

	return rv;
}




LayerTRW::LayerTRW() : Layer()
{
	this->m_kind = LayerKind::TRW;
	strcpy(this->debug_string, "TRW");
	this->interface = &vik_trw_layer_interface;

	this->tracks.owning_layer = this;
	this->routes.owning_layer = this;
	this->waypoints.owning_layer = this;

	this->painter = new LayerTRWPainter(this);

	this->set_initial_parameter_values();
	this->set_name(Layer::get_translated_layer_kind_string(this->m_kind));

	/* Param settings that are not available via the GUI. */
	/* Force to on after processing params (which defaults them to off with a zero value). */
	this->tracks.set_visible(true);
	this->routes.set_visible(true);
	this->waypoints.set_visible(true);

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

	g_selected.add_to_set(this);

	/* Set highlight thickness. */
	ThisApp::get_main_gis_view()->set_highlight_thickness(this->get_track_thickness());

	/* Mark for redraw. */
	return true;
}




/**
   \brief Get layer's selected track or route

   Returns track or route selected in this layer (if any track or route is selected in this layer)
   Returns NULL otherwise.
*/
Track * LayerTRW::selected_track_get()
{
	return this->m_selected_track;
}




void LayerTRW::selected_track_set(Track * track, const TrackpointReference & tp_ref)
{
	if (!track) {
		qDebug() << SG_PREFIX_E << "NULL track";
		return;
	}

	this->m_selected_track = track;
	this->m_selected_track->selected_tp_set(tp_ref);
}





void LayerTRW::selected_track_set(Track * track)
{
	if (!track) {
		qDebug() << SG_PREFIX_E << "NULL track";
		return;
	}

	this->m_selected_track = track;
	this->m_selected_track->selected_tp_reset();
}





bool LayerTRW::selected_track_reset(void)
{
	const bool was_set = NULL != this->m_selected_track;

	if (was_set) {
		qDebug() << SG_PREFIX_I << "Will reset trackpoint properties dialog data";

		if (0 != this->m_selected_track->get_selected_children().get_count()) {
			this->m_selected_track->selected_tp_reset();
		}

		this->m_selected_track = NULL;
	}

	return was_set;
}




/**
   \brief Get layer's selected waypoint

   Returns waypoint selected in this layer (if any waypoint is selected in this layer)
   Returns NULL otherwise.
*/
Waypoint * LayerTRW::selected_wp_get()
{
	return this->m_selected_wp;
}




void LayerTRW::selected_wp_set(Waypoint * wp)
{
	if (nullptr == wp) {
		qDebug() << SG_PREFIX_E << "NULL waypoint";
		return;
	}

	this->m_selected_wp = wp;
	this->m_selected_wp->properties_dialog_set();
}




bool LayerTRW::selected_wp_reset(void)
{
	const bool was_selected = nullptr != this->m_selected_wp;

	this->m_selected_wp = nullptr;

	Waypoint::properties_dialog_reset();

	return was_selected;
}




bool LayerTRW::get_track_creation_in_progress() const
{
	LayerToolTRWNewTrack * new_track_tool = (LayerToolTRWNewTrack *) ThisApp::get_main_window()->get_toolbox()->get_tool(LayerToolTRWNewTrack::tool_id());
	if (NULL == new_track_tool) {
		qDebug() << SG_PREFIX_E << "Failed to get tool with id =" << LayerToolTRWNewTrack::tool_id();
		return false;
	}
	return new_track_tool->creation_in_progress == this;
}




void LayerTRW::reset_track_creation_in_progress()
{
	LayerToolTRWNewTrack * new_track_tool = (LayerToolTRWNewTrack *) ThisApp::get_main_window()->get_toolbox()->get_tool(LayerToolTRWNewTrack::tool_id());
	if (NULL == new_track_tool) {
		qDebug() << SG_PREFIX_E << "Failed to get tool with id =" << LayerToolTRWNewTrack::tool_id();
		return;
	}
	if (new_track_tool->creation_in_progress == this) {
		new_track_tool->creation_in_progress = NULL;
	}
}




bool LayerTRW::get_route_creation_in_progress() const
{
	LayerToolTRWNewRoute * new_route_tool = (LayerToolTRWNewRoute *) ThisApp::get_main_window()->get_toolbox()->get_tool(LayerToolTRWNewRoute::tool_id());
	if (NULL == new_route_tool) {
		qDebug() << SG_PREFIX_E << "Failed to get tool with id =" << LayerToolTRWNewRoute::tool_id();
		return false;
	}
	return new_route_tool->creation_in_progress == this;
}




void LayerTRW::reset_route_creation_in_progress()
{
	LayerToolTRWNewRoute * new_route_tool = (LayerToolTRWNewRoute *) ThisApp::get_main_window()->get_toolbox()->get_tool(LayerToolTRWNewRoute::tool_id());
	if (NULL == new_route_tool) {
		qDebug() << SG_PREFIX_E << "Failed to get tool with id =" << LayerToolTRWNewRoute::tool_id();
		return;
	}
	if (new_route_tool->creation_in_progress == this) {
		new_route_tool->creation_in_progress = NULL;
	}
}




void LayerTRW::show_wp_picture_cb(void) /* Slot. */
{
	Waypoint * wp = this->selected_wp_get();
	if (!wp) {
		qDebug() << SG_PREFIX_E << "No waypoint in waypoint-related callback" << __FUNCTION__;
		return;
	}

	const QString program = Preferences::get_image_viewer();
	const QString image_full_path = this->selected_wp_get()->image_full_path;
	const QString quoted_image_full_path = Util::shell_quote(image_full_path);

	QStringList args;
	args << quoted_image_full_path;

	qDebug() << SG_PREFIX_I << program << quoted_image_full_path;

	/* "Fire and forget". The viewer will run detached from this application. */
	QProcess::startDetached(program, args);

	/* TODO_LATER: add handling of errors from process. */
}




void LayerTRW::delete_track_cb(void)
{
	QAction * qa = (QAction *) QObject::sender();
	const sg_uid_t child_uid = qa->data().toUInt();

	Track * trk = this->tracks.find_child_by_uid(child_uid);
	if (trk) {
		/* false: don't require confirmation in callbacks. */
		this->delete_track(trk, false);
	}
}




void LayerTRW::delete_route_cb(void)
{
	QAction * qa = (QAction *) QObject::sender();
	const sg_uid_t child_uid = qa->data().toUInt();

	Track * trk = this->routes.find_child_by_uid(child_uid);
	if (trk) {
		/* false: don't require confirmation in callbacks. */
		this->delete_track(trk, false);
	}
}




void LayerTRW::delete_waypoint_cb(void)
{
	QAction * qa = (QAction *) QObject::sender();
	const sg_uid_t child_uid = qa->data().toUInt();

	Waypoint * wp = this->waypoints.find_child_by_uid(child_uid);
	if (wp) {
		/* false: don't require confirmation in callbacks. */
		this->delete_waypoint(wp, false);
	}
}




sg_ret LayerTRW::delete_track(Track * trk, bool confirm)
{
	const bool is_track = trk->is_track();

	if (confirm) {
		/* Get confirmation from the user. */
		if (!Dialog::yes_or_no(is_track
				       ? tr("Are you sure you want to delete the track \"%1\"?")
				       : tr("Are you sure you want to delete the route \"%1\"?")
				       .arg(trk->name)), ThisApp::get_main_window()) {
			return sg_ret::ok;
		}
	}


	bool was_visible = false;
	this->detach_from_container(trk, &was_visible);
	this->detach_from_tree(trk);
	delete trk;


	if (is_track) {
		/* Reset layer timestamp in case it has now changed. */
		this->tree_view->apply_tree_item_timestamp(this);
	}

	if (was_visible) {
		this->emit_tree_item_changed("Indicating change in Layer TRW after deleting Track or Route");
	}

	return sg_ret::ok;
}




sg_ret LayerTRW::delete_waypoint(Waypoint * wp, bool confirm)
{
	if (confirm) {
		/* Get confirmation from the user. */
		/* Maybe this Waypoint Delete should be optional as is it could get annoying... */
		if (!Dialog::yes_or_no(tr("Are you sure you want to delete the waypoint \"%1\"?").arg(wp->name)), ThisApp::get_main_window()) {
			return sg_ret::ok;
		}
	}

	bool was_visible;
	this->detach_from_container(wp, &was_visible);
	this->detach_from_tree(wp);
	delete wp;

	this->waypoints.recalculate_bbox();

	/* Reset layer timestamp in case it has now changed. */
	this->tree_view->apply_tree_item_timestamp(this);

	if (was_visible) {
		this->emit_tree_item_changed("Indicating change in Layer TRW after deleting Waypoint");
	}

	return sg_ret::ok;
}





sg_ret LayerTRW::has_child(const Track * trk, bool * result) const
{
	if (NULL == trk) {
		qDebug() << SG_PREFIX_E << "Invalid argument 1";
		return sg_ret::err;
	}
	if (NULL == result) {
		qDebug() << SG_PREFIX_E << "Invalid argument 2";
		return sg_ret::err;
	}


	Track * found = NULL;
	if (trk->is_track()) {
		found = this->tracks.find_child_by_uid(trk->get_uid());
	} else {
		found = this->routes.find_child_by_uid(trk->get_uid());
	}

	*result = (NULL != found);

	return sg_ret::ok;
}




sg_ret LayerTRW::has_child(const Waypoint * wp, bool * result) const
{
	if (NULL == wp) {
		qDebug() << SG_PREFIX_E << "Invalid argument 1";
		return sg_ret::err;
	}
	if (NULL == result) {
		qDebug() << SG_PREFIX_E << "Invalid argument 2";
		return sg_ret::err;
	}


	Waypoint * found = this->waypoints.find_child_by_uid(wp->get_uid());

	*result = NULL != found;

	return sg_ret::ok;
}




void LayerTRW::lock_remove(void)
{
	qDebug() << SG_PREFIX_D << "Lock - before";
	this->remove_mutex.lock();
	qDebug() << SG_PREFIX_D << "Lock - after";
}




void LayerTRW::unlock_remove(void)
{
	qDebug() << SG_PREFIX_D << "Unlock - before";
	this->remove_mutex.unlock();
	qDebug() << SG_PREFIX_D << "Unlock - after";
}




bool LayerTRW::move_child(TreeItem & child_tree_item, bool up)
{
	/* Let's not allow moving Tracks/Routes/Waypoints nodes. */
	return false;
}




/* Doesn't set the trigger. Should be done by aggregate layer when child emits 'changed' signal. */
void LayerTRW::child_tree_item_changed_cb(const QString & child_tree_item_name) /* Slot. */
{
	qDebug() << SG_PREFIX_SLOT << "Layer" << this->name << "received 'child tree item changed' signal from" << child_tree_item_name;
	if (this->is_visible()) {
		/* TODO_LATER: this can used from the background - e.g. in acquire
		   so will need to flow background update status through too. */
		qDebug() << SG_PREFIX_SIGNAL << "Layer" << this->name << "emits 'changed' signal";
		emit this->tree_item_changed(this->get_name());
	}
}




LayerTRW::TracksTooltipData::TracksTooltipData()
{
	/* Track uses internal distance and time units, so this also uses internal unit. */
	this->length = Distance(0, Distance::get_internal_unit());
	this->duration = Duration(0, Duration::get_internal_unit());

}
