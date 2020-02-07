/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2005, Evan Battaglia <viking@greentorch.org>
 * Copyright (C) 2010, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (c) 2013, Rob Norris <rw_norris@hotmail.com>
 * UTM multi-zone stuff by Kit Transue <notlostyet@didactek.com>
 * Dynamic map type by Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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




#include <mutex>
#include <map>
#include <cstdlib>
#include <cassert>
#include <cmath>




#include <sys/types.h>
#include <sys/stat.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif




#include <QDebug>
#include <QDir>




#include "application_state.h"
#include "dialog.h"
#include "file.h"
#include "map_cache.h"
#include "layer_map_source.h"
#include "layer_map_source_slippy.h"
#include "map_utils.h"
#include "layer_defaults.h"
#include "layer_map.h"
#include "layer_map_download.h"
#include "osm_metatile.h"
#include "preferences.h"
#include "statusbar.h"
#include "ui_util.h"
#include "ui_builder.h"
#include "util.h"
#include "viewport_internal.h"
#include "vikutils.h"
#include "widget_file_entry.h"
#include "window.h"




using namespace SlavGPS;




#define SG_MODULE "Map Layer"




LayerMapInterface vik_map_layer_interface;




extern bool vik_debug;
extern bool vik_verbose;




#define LAYER_MAP_GRID_COLOR "#E6202E" /* Red-ish. */

#define VIK_SETTINGS_MAP_MAX_TILES "maps_max_tiles"
static int g_max_tiles = 1000;

#define VIK_SETTINGS_MAP_MIN_SHRINKFACTOR "maps_min_shrinkfactor"
#define VIK_SETTINGS_MAP_MAX_SHRINKFACTOR "maps_max_shrinkfactor"
static double g_max_shrinkfactor = 8.0000001; /* zoom 1 viewing 8-tiles. */
static double g_min_shrinkfactor = 0.0312499; /* zoom 32 viewing 1-tiles. */

#define VIK_SETTINGS_MAP_REAL_MIN_SHRINKFACTOR "maps_real_min_shrinkfactor"
static double g_real_min_shrinkfactor = 0.0039062499; /* If shrinkfactor is between MAX and REAL_MAX, will only check for existence. */

#define VIK_SETTINGS_MAP_SCALE_INC_UP "maps_scale_inc_up"
static unsigned int SCALE_INC_UP = 2;
#define VIK_SETTINGS_MAP_SCALE_INC_DOWN "maps_scale_inc_down"
static unsigned int SCALE_INC_DOWN = 4;
#define VIK_SETTINGS_MAP_SCALE_SMALLER_ZOOM_FIRST "maps_scale_smaller_zoom_first"
static bool g_scale_smaller_zoom_first = true;




static std::map<MapTypeID, MapSourceMaker> map_source_makers;




/* List of labels (string) and IDs (MapTypeID) for each map type. */
static WidgetIntEnumerationData map_types_enum = {
	{},
	(int) MapTypeID::OSMCycle,
};
static SGVariant map_type_default(void) { return SGVariant(map_types_enum.default_id, SGVariantType::Enumeration); }




static WidgetIntEnumerationData map_zooms_enum = {
	{
		SGLabelID(QObject::tr("Follow Zoom Level of Viewport"), LAYER_MAP_ZOOM_ID_FOLLOW_VIEWPORT_ZOOM_LEVEL),
		SGLabelID(QObject::tr("0.25"),       1),
		SGLabelID(QObject::tr("0.5"),        2),
		SGLabelID(QObject::tr("1"),          3),
		SGLabelID(QObject::tr("2"),          4),
		SGLabelID(QObject::tr("4"),          5),
		SGLabelID(QObject::tr("8"),          6),
		SGLabelID(QObject::tr("16"),         7),
		SGLabelID(QObject::tr("32"),         8),
		SGLabelID(QObject::tr("64"),         9),
		SGLabelID(QObject::tr("128"),       10),
		SGLabelID(QObject::tr("256"),       11),
		SGLabelID(QObject::tr("512"),       12),
		SGLabelID(QObject::tr("1024"),      13),
		SGLabelID(QObject::tr("USGS 10k"),  14),
		SGLabelID(QObject::tr("USGS 24k"),  15),
		SGLabelID(QObject::tr("USGS 25k"),  16),
		SGLabelID(QObject::tr("USGS 50k"),  17),
		SGLabelID(QObject::tr("USGS 100k"), 18),
		SGLabelID(QObject::tr("USGS 200k"), 19),
		SGLabelID(QObject::tr("USGS 250k"), 20),
	},
	LAYER_MAP_ZOOM_ID_FOLLOW_VIEWPORT_ZOOM_LEVEL,
};




/* Count of elements in each of these two arrays should be the same as
   count of elements in map_zooms_enum[]. */
static double map_zooms_x[] = { 0.0, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 256.0, 512.0, 1024.0, 1.016, 2.4384, 2.54, 5.08, 10.16, 20.32, 25.4 };
static double map_zooms_y[] = { 0.0, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 256.0, 512.0, 1024.0, 1.016, 2.4384, 2.54, 5.08, 10.16, 20.32, 25.4 };




static ParameterScale<int> scale_alpha(0, 255, SGVariant((int32_t) 255, SGVariantType::Int), 3, 0);




static SGVariant directory_default(void)
{
	const SGVariant pref_value = Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL "maplayer_default_dir");
	if (pref_value.type_id == SGVariantType::Empty) {
		return SGVariant("");
	} else {
		return pref_value;
	}
}




static SGVariant file_default(void)
{
	return SGVariant("");
}




static WidgetIntEnumerationData cache_layout_enum = {
	{
		SGLabelID("Viking", (int) MapCacheLayout::Viking),
		SGLabelID("OSM",    (int) MapCacheLayout::OSM),
	},
	(int) MapCacheLayout::Viking,
};




enum {
	PARAM_MAP_TYPE_ID = 0,
	PARAM_CACHE_DIR,
	PARAM_CACHE_LAYOUT,
	PARAM_FILE,
	PARAM_ALPHA,
	PARAM_AUTO_DOWNLOAD,
	PARAM_ONLY_MISSING,
	PARAM_MAP_ZOOM,
	NUM_PARAMS
};


FileSelectorWidget::FileTypeFilter map_file_type[1] = { FileSelectorWidget::FileTypeFilter::MBTILES };

static ParameterSpecification maps_layer_param_specs[] = {
	/* 'mode' is really map source type id, but can't break file format just to rename the parameter name to something better. */
	{ PARAM_MAP_TYPE_ID,   "mode",           SGVariantType::Enumeration,  PARAMETER_GROUP_GENERIC, QObject::tr("Map Type:"),                            WidgetType::IntEnumeration,   &map_types_enum,     NULL,                 "" },
	{ PARAM_CACHE_DIR,     "directory",      SGVariantType::String,       PARAMETER_GROUP_GENERIC, QObject::tr("Maps Directory:"),                      WidgetType::FolderEntry,      NULL,                directory_default,    "" },
	{ PARAM_CACHE_LAYOUT,  "cache_type",     SGVariantType::Enumeration,  PARAMETER_GROUP_GENERIC, QObject::tr("Cache Layout:"),                        WidgetType::IntEnumeration,   &cache_layout_enum,  NULL,                 QObject::tr("This determines the tile storage layout on disk") },
	{ PARAM_FILE,          "mapfile",        SGVariantType::String,       PARAMETER_GROUP_GENERIC, QObject::tr("Raster MBTiles Map File:"),             WidgetType::FileSelector,     map_file_type,       file_default,         QObject::tr("A raster MBTiles file. Only applies when the map type method is 'MBTiles'") },
	{ PARAM_ALPHA,         "alpha",          SGVariantType::Int,          PARAMETER_GROUP_GENERIC, QObject::tr("Alpha:"),                               WidgetType::HScale,           &scale_alpha,        NULL,                 QObject::tr("Control the Alpha value for transparency effects") },
	{ PARAM_AUTO_DOWNLOAD, "autodownload",   SGVariantType::Boolean,      PARAMETER_GROUP_GENERIC, QObject::tr("Autodownload maps:"),                   WidgetType::CheckButton,      NULL,                sg_variant_true,      "" },
	{ PARAM_ONLY_MISSING,  "adlonlymissing", SGVariantType::Boolean,      PARAMETER_GROUP_GENERIC, QObject::tr("Autodownload Only Gets Missing Maps:"), WidgetType::CheckButton,      NULL,                sg_variant_false,     QObject::tr("Using this option avoids attempting to update already acquired tiles. This can be useful if you want to restrict the network usage, without having to resort to manual control. Only applies when 'Autodownload Maps' is on.") },
	{ PARAM_MAP_ZOOM,      "mapzoom",        SGVariantType::Enumeration,  PARAMETER_GROUP_GENERIC, QObject::tr("Zoom Level:"),                          WidgetType::IntEnumeration,   &map_zooms_enum,     NULL,                 QObject::tr("Determines the method of displaying map tiles for the current zoom level.\n'Follow Zoom Level of Viewport' uses the best matching level, otherwise setting a fixed value will always use map tiles of the specified value regardless of the actual zoom level.") },

	{ NUM_PARAMS,          "",               SGVariantType::Empty,        PARAMETER_GROUP_GENERIC, "",                                                  WidgetType::None,             NULL,                NULL,                 "" }, /* Guard. */
};




void LayerMap::set_autodownload_default(bool autodownload)
{
	maps_layer_param_specs[PARAM_AUTO_DOWNLOAD].hardcoded_default_value = autodownload ? sg_variant_true : sg_variant_false;
}




void LayerMap::set_cache_default(MapCacheLayout layout)
{
	/* Override default value returned by the default param function. */
	cache_layout_enum.default_id = (int) layout;
}




LayerMapInterface::LayerMapInterface()
{
	this->parameters_c = maps_layer_param_specs; /* Parameters. */

	this->fixed_layer_kind_string = "Map"; /* Non-translatable. */

	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_M;
	// this->action_icon = ...; /* Set elsewhere. */

	this->ui_labels.new_layer = QObject::tr("New Map Layer");
	this->ui_labels.translated_layer_kind = QObject::tr("Map");
	this->ui_labels.layer_defaults = QObject::tr("Default Settings of Map Layer");
}




/**
   @reviewed-on 2020-02-05
*/
LayerToolContainer LayerMapInterface::create_tools(Window * window, GisViewport * gisview)
{
	static LayerToolContainer tools;

	/* This method should be called only once. */
	static bool created = false;
	if (created) {
		qDebug() << SG_PREFIX_E << "Called the method for (at least) second time";
		return tools;
	}

	LayerTool * tool = new LayerToolMapsDownload(window, gisview);
	tools.insert({ tool->get_tool_id(), tool });

	created = true;

	return tools;
}



static ParameterSpecification prefs[] = {
	{ (param_id_t) LayerKind::Max, PREFERENCES_NAMESPACE_GENERAL "maplayer_default_dir", SGVariantType::String, PARAMETER_GROUP_GENERIC, QObject::tr("Default map layer directory:"), WidgetType::FolderEntry, NULL, NULL, QObject::tr("Choose a directory to store cached Map tiles for this layer") },
};




void LayerMap::init(void)
{
	Preferences::register_parameter_instance(prefs[0], SGVariant(MapCache::get_default_maps_dir(), prefs[0].type_id));

	int int_val = 0;
	double double_val = 0.0;
	bool bool_val = true;

	if (ApplicationState::get_integer(VIK_SETTINGS_MAP_MAX_TILES, &int_val)) {
		g_max_tiles = int_val;
	}
	if (ApplicationState::get_double(VIK_SETTINGS_MAP_MIN_SHRINKFACTOR, &double_val)) {
		g_min_shrinkfactor = double_val;
	}
	if (ApplicationState::get_double(VIK_SETTINGS_MAP_MAX_SHRINKFACTOR, &double_val)) {
		g_max_shrinkfactor = double_val;
	}
	if (ApplicationState::get_double(VIK_SETTINGS_MAP_REAL_MIN_SHRINKFACTOR, &double_val)) {
		g_real_min_shrinkfactor = double_val;
	}
	if (ApplicationState::get_integer(VIK_SETTINGS_MAP_SCALE_INC_UP, &int_val)) {
		SCALE_INC_UP = int_val;
	}
	if (ApplicationState::get_integer(VIK_SETTINGS_MAP_SCALE_INC_DOWN, &int_val)) {
		SCALE_INC_DOWN = int_val;
	}
	if (ApplicationState::get_boolean(VIK_SETTINGS_MAP_SCALE_SMALLER_ZOOM_FIRST, &bool_val)) {
		g_scale_smaller_zoom_first = bool_val;
	}
}




/**
   \brief Register a new MapSource

   Override existing one (recognizing existing instance of map source
   is done by comparing map type IDs).
*/
void MapSources::register_map_source_maker(MapSourceMaker map_source_maker_fn, MapTypeID map_type_id, const QString & map_type_ui_label)
{
	assert (nullptr != map_source_maker_fn);
	assert (!map_type_ui_label.isEmpty());

	auto iter = map_source_makers.find(map_type_id);
	if (iter == map_source_makers.end()) {
		/* Add the map source as new. */

		map_types_enum.values.push_back(SGLabelID(map_type_ui_label, (int) map_type_id));
		map_source_makers[map_type_id] = map_source_maker_fn;
		/* TODO_LATER: verify in application that properties dialog sees updated list of map types. */
	} else {

		/* Update (overwrite) existing entry. */

		MapSourceMaker old = map_source_makers[map_type_id];
		map_source_makers[map_type_id] = map_source_maker_fn;
		if (nullptr == old) {
			qDebug() << SG_PREFIX_E << "Previous map source maker was NULL";
		}

		/* TODO_MAYBE: the concept of map types, and of updating it,
		   could be implemented better. */
		for (int i = 0; i < (int) map_types_enum.values.size(); i++) {
			if (map_types_enum.values[i].id == (int) map_type_id) {
				map_types_enum.values[i].label = map_type_ui_label;
				break;
			}
		}

		/* TODO_LATER: verify in application that properties dialog sees updated entry. */
	}
}




sg_ret LayerMap::set_map_type_id(MapTypeID map_type_id)
{
	if (!MapSource::is_map_type_id_registered(map_type_id)) {
		qDebug() << SG_PREFIX_E << "Map type" << (int) map_type_id << "has not been registered in the program";
		return sg_ret::err;
	}

	this->m_map_type_id = map_type_id;
	delete this->m_map_source;
	this->m_map_source = map_source_makers[this->m_map_type_id]();
	return sg_ret::ok;
}




MapTypeID LayerMap::get_default_map_type_id(void)
{
	SGVariant var = LayerDefaults::get(LayerKind::Map, maps_layer_param_specs[PARAM_MAP_TYPE_ID]);
	if (!var.is_valid() || var.u.val_enumeration == (int) MapTypeID::Default) {
		var = map_type_default();
	}
	return (MapTypeID) var.u.val_enumeration;
}




QString LayerMap::get_map_type_ui_label(void) const
{
	if (this->m_map_type_id == MapTypeID::Initial) {
		return QObject::tr("Map Layer");
	} else {
		return this->m_map_source->ui_label();
	}
}




/****************************************/
/******** CACHE DIR STUFF ***************/
/****************************************/



void LayerMap::mkdir_if_default_dir()
{
	if (!this->cache_dir.isEmpty()
	    && this->cache_dir == MapCache::get_dir()
	    && 0 != access(this->cache_dir.toUtf8().constData(), F_OK)) {

		if (mkdir(this->cache_dir.toUtf8().constData(), 0777) != 0) {
			qDebug() << SG_PREFIX_W << "Failed to create directory" << this->cache_dir;
		}
	}
}




void LayerMap::set_cache_dir(const QString & dir)
{
	this->cache_dir = "";

	QString mydir = dir;
	if (dir.isEmpty()) {
		SGVariant var = Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL "maplayer_default_dir");
		if (var.type_id != SGVariantType::Empty) {
			mydir = var.val_string;
		}
	}

	const QString canonical_dir = vu_get_canonical_filename(this, mydir, this->get_window()->get_current_document_full_path());

	/* Ensure cache_dir always ends with a separator. */
	const int len = canonical_dir.length();
	/* Unless the dir is not valid. */
	if (len > 0) {
		if (canonical_dir.at(len - 1) != QDir::separator()) {
			this->cache_dir = canonical_dir + QDir::separator();
		} else {
			this->cache_dir = canonical_dir;
		}

		this->mkdir_if_default_dir();
	}
}




void LayerMap::set_file_full_path(const QString & new_file_full_path)
{
	this->file_full_path = new_file_full_path;
}




/**
   \brief See if given map type id describes a map source that has been registered in application

   \return true or false
*/
bool MapSource::is_map_type_id_registered(MapTypeID map_type_id)
{
	return map_source_makers.end() != map_source_makers.find(map_type_id);
}




#define VIK_SETTINGS_MAP_LICENSE_SHOWN "map_license_shown"

/**
   Convenience function to display the license.
*/
static void maps_show_license(Window * parent, const MapSource * map_source)
{
	Dialog::map_license(map_source->ui_label(), map_source->get_license(), map_source->get_license_url(), parent);
}




bool LayerMap::set_param_value(param_id_t param_id, const SGVariant & data, bool is_file_operation)
{
	switch (param_id) {
	case PARAM_CACHE_DIR:
		this->set_cache_dir(data.val_string);
		break;
	case PARAM_CACHE_LAYOUT:
		if ((MapCacheLayout) data.u.val_enumeration < MapCacheLayout::Num) {
			this->cache_layout = (MapCacheLayout) data.u.val_enumeration;
		}
		break;
	case PARAM_FILE:
		this->set_file_full_path(data.val_string);
		break;
	case PARAM_MAP_TYPE_ID:
		if (sg_ret::ok != this->set_map_type_id((MapTypeID) data.u.val_int)) {
			/* TODO_LATER: handle this better. */
			qDebug() << SG_PREFIX_E << "Unknown map type" << data.u.val_int;
			return false;
		}

		/* When loading from a file don't need the license reminder - ensure it's saved into the 'seen' list. */
		if (is_file_operation) {
			ApplicationState::set_integer_list_containing(VIK_SETTINGS_MAP_LICENSE_SHOWN, (int) this->m_map_type_id);
		} else {
			/* Call to MapSource::is_map_type_id_registered()
			   above guarantees that this map
			   lookup is successful. */
			if (this->m_map_source->get_license() != NULL) {
				/* Check if licence for this map type has been shown before. */
				if (!ApplicationState::get_integer_list_contains(VIK_SETTINGS_MAP_LICENSE_SHOWN, (int) this->m_map_type_id)) {
					maps_show_license(this->get_window(), this->m_map_source);
					ApplicationState::set_integer_list_containing(VIK_SETTINGS_MAP_LICENSE_SHOWN, (int) this->m_map_type_id);
				}
			}
		}
		break;
	case PARAM_ALPHA:
		if (data.u.val_int >= scale_alpha.min && data.u.val_int <= scale_alpha.max) {
			this->alpha = data.u.val_int;
		}
		break;
	case PARAM_AUTO_DOWNLOAD:
		this->autodownload = data.u.val_bool;
		break;
	case PARAM_ONLY_MISSING:
		this->adl_only_missing = data.u.val_bool;
		break;
	case PARAM_MAP_ZOOM:
		if (data.u.val_enumeration < (int) map_zooms_enum.values.size()) { /* TODO_LATER: don't compare integers, try to find enum ID in map_zooms_enum */
			this->map_zoom_id = data.u.val_enumeration;
			this->map_zoom_x = map_zooms_x[this->map_zoom_id];
			this->map_zoom_y = map_zooms_y[this->map_zoom_id];
		} else {
			qDebug() << SG_PREFIX_W << "Unknown Map Zoom" << data.u.val_enumeration;
			this->map_zoom_id = map_zooms_enum.default_id;
		}
		break;
	default:
		break;
	}
	return true;
}




SGVariant LayerMap::get_param_value(param_id_t param_id, bool is_file_operation) const
{
	SGVariant rv;
	switch (param_id) {
	case PARAM_CACHE_DIR: {
		bool set = false;
		/* Only save a blank when the map cache location equals the default.
		   On reading in, when it is blank then the default is reconstructed.
		   Since the default changes dependent on the user and OS, it means the resultant file is more portable. */
		if (is_file_operation
		    && !this->cache_dir.isEmpty()
		    && this->cache_dir == MapCache::get_dir()) {

			rv = SGVariant("");
			set = true;
		} else if (is_file_operation && !this->cache_dir.isEmpty()) {
			if (Preferences::get_file_path_format() == FilePathFormat::Relative) {
				const QString cwd = QDir::currentPath();
				if (!cwd.isEmpty()) {
					rv = SGVariant(file_GetRelativeFilename(cwd, this->cache_dir));
					set = true;
				}
			}
		}

		if (!set) {
			rv = SGVariant(this->cache_dir);
		}
		break;
	}
	case PARAM_CACHE_LAYOUT:
		rv = SGVariant((int32_t) this->cache_layout, maps_layer_param_specs[PARAM_CACHE_LAYOUT].type_id);
		break;
	case PARAM_FILE:
		rv = SGVariant(this->file_full_path);
		break;
	case PARAM_MAP_TYPE_ID:
		rv = SGVariant((int32_t) this->m_map_type_id, maps_layer_param_specs[PARAM_MAP_TYPE_ID].type_id);
		break;
	case PARAM_ALPHA:
		rv = SGVariant((int32_t) this->alpha, maps_layer_param_specs[PARAM_ALPHA].type_id);
		break;
	case PARAM_AUTO_DOWNLOAD:
		rv = SGVariant(this->autodownload); /* kamilkamil: in viking code there is a type mismatch. */
		break;
	case PARAM_ONLY_MISSING:
		rv = SGVariant(this->adl_only_missing); /* kamilkamil: in viking code there is a type mismatch. */
		break;
	case PARAM_MAP_ZOOM:
		rv = SGVariant((int) this->map_zoom_id, maps_layer_param_specs[PARAM_MAP_ZOOM].type_id);
		break;
	default: break;
	}
	return rv;
}



#ifdef TODO_LATER
void LayerMapInterface::change_param(void * gtk_widget, void * ui_change_values)
{
	switch (values->param_id) {
		/* Alter sensitivity of download option widgets according to the map_index setting. */
	case PARAM_MAP_TYPE_ID: {
		/* Get new value. */
		SGVariant var = a_uibuilder_widget_get_value(gtk_widget, values->param);
		/* Is it *not* the OSM On Disk Tile Layout or the MBTiles type or the OSM Metatiles type. */
		bool sensitive = (MapTypeID::OSMOnDisk != var.i &&
				  MapTypeID::MBTiles != var.i &&
				  MapTypeID::OSMMetatiles != var.i);
		GtkWidget **ww1 = values->widgets;
		GtkWidget **ww2 = values->labels;
		GtkWidget *w1 = ww1[PARAM_ONLY_MISSING];
		GtkWidget *w2 = ww2[PARAM_ONLY_MISSING];
		GtkWidget *w3 = ww1[PARAM_AUTO_DOWNLOAD];
		GtkWidget *w4 = ww2[PARAM_AUTO_DOWNLOAD];
		/* Depends on autodownload value. */
		LayerMap * layer = (LayerMap *) values->layer;
		bool missing_sense = sensitive && layer->autodownload;
		if (w1) {
			w1->setEnabled(missing_sense);
		}

		if (w2) {
			w2->setEnabled(missing_sense);
		}

		if (w3) {
			w3->setEnabled(sensitive);
		}

		if (w4) {
			w4->setEnabled(sensitive);
		}

		/* Cache type not applicable either. */
		GtkWidget *w9 = ww1[PARAM_CACHE_LAYOUT];
		GtkWidget *w10 = ww2[PARAM_CACHE_LAYOUT];
		if (w9) {
			w9->setEnabled(sensitive);
		}

		if (w10) {
			w10->setEnabled(sensitive);
		}

		/* File only applicable for MBTiles type.
		   Directory for all other types. */
		sensitive = (MapTypeID::MBTiles == var.i);
		GtkWidget *w5 = ww1[PARAM_FILE];
		GtkWidget *w6 = ww2[PARAM_FILE];
		GtkWidget *w7 = ww1[PARAM_CACHE_DIR];
		GtkWidget *w8 = ww2[PARAM_CACHE_DIR];
		if (w5) {
			w5->setEnabled(sensitive);
		}

		if (w6) {
			w6->setEnabled(sensitive);
		}

		if (w7) {
			w7->setEnabled(!sensitive);
		}

		if (w8) {
			w8->setEnabled(!sensitive);
		}

		break;
	}

		/* Alter sensitivity of 'download only missing' widgets according to the autodownload setting. */
	case PARAM_AUTO_DOWNLOAD: {
		/* Get new value. */
		SGVariant var = a_uibuilder_widget_get_value(gtk_widget, values->param);
		GtkWidget **ww1 = values->widgets;
		GtkWidget **ww2 = values->labels;
		GtkWidget *w1 = ww1[PARAM_ONLY_MISSING];
		GtkWidget *w2 = ww2[PARAM_ONLY_MISSING];
		if (w1) {
			w1->setEnabled(var.b);
		}

		if (w2) {
			w2->setEnabled(var.b);
		}
		break;
	}
	default: break;
	}
}
#endif




LayerMap::~LayerMap()
{
	if (this->dl_right_click_menu) {
		delete this->dl_right_click_menu;
	}

	free(this->last_center);
	this->last_center = nullptr;

	delete this->m_map_source;
	this->m_map_source = nullptr;
}




sg_ret LayerMap::post_read(GisViewport * gisview, bool from_file)
{
	if (!from_file) {
		/* If this method is not called in file reading context it is called in GUI context.
		   So, we can check if we have to inform the user about inconsistency. */
		if (this->m_map_source->get_drawmode() != gisview->get_draw_mode()) {
			const QString drawmode_name = GisViewportDrawModes::get_label_with_accelerator(this->m_map_source->get_drawmode());
			const QString msg = QObject::tr("New map cannot be displayed in the current drawmode.\nSelect \"%1\" from View menu to view it.").arg(drawmode_name);
			Dialog::warning(msg, this->get_window());
		}
	}

	/* Performed in post read as we now know the map type. */

	MapSourceParameters source_params;
	source_params.full_path = this->file_full_path;

	QString open_err;
	if (sg_ret::ok != this->m_map_source->open_map_source(source_params, open_err)) {
		Dialog::error(open_err, this->get_window());
		/* TODO_LATER: now what? How to handle failure of MapSource::post_read()? */
		return sg_ret::err;
	}

	/* If the on Disk OSM Tile Layout type. */
	if (this->m_map_source->map_type_id() == MapTypeID::OSMOnDisk) {
		/* Copy the directory into filename.
		   Thus the map cache look up will be unique when using more than one of these map types. */
		this->file_full_path = this->cache_dir;
	}
	return sg_ret::ok;
}




QString LayerMap::get_tooltip(void) const
{
	return this->get_map_type_ui_label();
}




Layer * LayerMapInterface::unmarshall(Pickle & pickle, GisViewport * gisview)
{
	LayerMap * layer = new LayerMap();

	layer->unmarshall_params(pickle);
	layer->post_read(gisview, false);
	return layer;
}




/*********************/
/****** DRAWING ******/
/*********************/




static void pixmap_apply_settings(QPixmap & pixmap, int alpha, const TilePixmapResize & tile_pixmap_resize)
{
	/* Apply alpha setting. */
	if (scale_alpha.is_in_range(alpha)) {
		ui_pixmap_set_alpha(pixmap, alpha);
	}

	if (tile_pixmap_resize.x != 1.0 || tile_pixmap_resize.y != 1.0) {
		ui_pixmap_scale_size_by(pixmap, tile_pixmap_resize.x, tile_pixmap_resize.y);
	}

	return;
}




QPixmap LayerMap::get_tile_pixmap_with_stretch(const TileInfo & tile_info, const TilePixmapResize & tile_pixmap_resize)
{
	/* Get the thing. */
	QPixmap pixmap = MapCache::get_tile_pixmap_with_stretch(tile_info, this->m_map_type_id, this->alpha, tile_pixmap_resize, this->file_full_path);
	if (!pixmap.isNull()) {
		qDebug() << SG_PREFIX_I << "CACHE HIT";
		return pixmap;
	}

	/* Not an error, simply the pixmap was not in a cache. Let's generate the pixmap. */
	qDebug() << SG_PREFIX_I << "CACHE MISS";

	const MapCacheObj map_cache_obj(this->cache_layout, this->cache_dir);
	pixmap = this->m_map_source->create_tile_pixmap(map_cache_obj, tile_info);

	if (!pixmap.isNull()) {
		pixmap_apply_settings(pixmap, this->alpha, tile_pixmap_resize);

		MapCache::add_tile_pixmap(pixmap, MapCacheItemProperties(SG_RENDER_TIME_NO_RENDER), tile_info, this->m_map_source->map_type_id(),
					  this->alpha, tile_pixmap_resize, this->file_full_path);
	}

	return pixmap;
}




bool LayerMap::should_start_autodownload(const GisViewport * gisview)
{
	const Coord center = gisview->get_center_coord();

	if (this->get_window()->get_pan_move_in_progress()) {
		/* Panning in progress: do not download. */
		return false;
	}

	/* Don't attempt to download unsupported zoom levels. */
	const TileZoomLevel tile_zoom_level = gisview->get_viking_scale().to_tile_zoom_level();
	if (!this->m_map_source->is_supported_tile_zoom_level(tile_zoom_level)) {
		return false;
	}

	if (this->last_center == NULL) {
		Coord * new_center = new Coord();
		*new_center = center;
		this->last_center = new_center;
		this->last_map_scale = gisview->get_viking_scale();
		return true;
	}


	if ((*this->last_center == center) /* TODO_MAYBE: perhaps Coord::distance()? */
	    && this->last_map_scale == gisview->get_viking_scale()) {

		return false;
	}

	*this->last_center = center;
	this->last_map_scale = gisview->get_viking_scale();
	return true;
}




TileGeometry LayerMap::find_resized_down_tile(const TileInfo & tile_iter,
					      const TileGeometry & tile_geometry,
					      const TilePixmapResize & tile_pixmap_resize)
{
	TileGeometry result;

	for (unsigned int scale_inc = 1; scale_inc < SCALE_INC_DOWN; scale_inc++) {
		/* Try with smaller zooms. */
		const int resize_times = 1 << scale_inc;  /* 2^scale_inc */

		TileInfo scaled_tile_iter = tile_iter;
		scaled_tile_iter.resize_down(scale_inc, resize_times);

		TilePixmapResize scaled_tile_pixmap_resize = tile_pixmap_resize;
		scaled_tile_pixmap_resize.resize_down(resize_times);

		result.pixmap = this->get_tile_pixmap_with_stretch(scaled_tile_iter, scaled_tile_pixmap_resize);
		if (!result.pixmap.isNull()) {
			qDebug() << SG_PREFIX_I << "Found scaled-down tile pixmap";

			result.dest_x = tile_geometry.dest_x;
			result.dest_y = tile_geometry.dest_y;
			result.begin_x = (tile_iter.x % resize_times) * tile_geometry.width;
			result.begin_y = (tile_iter.y % resize_times) * tile_geometry.height;
			result.width = tile_geometry.width;
			result.height = tile_geometry.height;

			return result;
		} else {
			qDebug() << SG_PREFIX_I << "Pixmap not found";
		}
	}
	return result;
}




TileGeometry LayerMap::find_resized_up_tile(const TileInfo & tile_iter,
					    const TileGeometry & tile_geometry,
					    const TilePixmapResize & tile_pixmap_resize)
{
	TileGeometry result;

	/* Try with bigger zooms. */
	for (unsigned int scale_dec = 1; scale_dec < SCALE_INC_UP; scale_dec++) {
		const int resize_times = 1 << scale_dec;  /* 2^scale_dec */

		TileInfo scaled_tile_iter = tile_iter;
		scaled_tile_iter.resize_up(scale_dec, resize_times);

		TilePixmapResize scaled_tile_pixmap_resize = tile_pixmap_resize;
		scaled_tile_pixmap_resize.resize_up(resize_times);

		TileGeometry scaled_tile_geometry = tile_geometry;
		scaled_tile_geometry.resize_up(resize_times);

		for (int pict_x = 0; pict_x < resize_times; pict_x++) {
			for (int pict_y = 0; pict_y < resize_times; pict_y++) {
				TileInfo ulm3 = scaled_tile_iter;
				ulm3.x += pict_x;
				ulm3.y += pict_y;

				result.pixmap = this->get_tile_pixmap_with_stretch(ulm3, scaled_tile_pixmap_resize);
				if (!result.pixmap.isNull()) {
					qDebug() << SG_PREFIX_I << "Found scaled-up tile pixmap";

					result.dest_x = tile_geometry.dest_x + pict_x * scaled_tile_geometry.width;
					result.dest_y = tile_geometry.dest_y + pict_y * scaled_tile_geometry.height;
					result.begin_x = 0;
					result.begin_y = 0;
					result.width = scaled_tile_geometry.width;
					result.height = scaled_tile_geometry.height;

					return result;
				} else {
					qDebug() << SG_PREFIX_I << "Pixmap not found";
				}
			}
		}
	}
	return result;
}




TilePixmapResize LayerMap::get_desired_pixmap_resize(const GisViewport & gisview) const
{
	TilePixmapResize result(1.0, 1.0);

	if (this->map_zoom_id == LAYER_MAP_ZOOM_ID_FOLLOW_VIEWPORT_ZOOM_LEVEL) {
		/* The tiles that we will get from Map Source will
		   require no resizing (the resizing factor will be
		   equal to 1.0). Nothing more to do */

	} else {
		/* User wants to use tile pixmaps with specific scale. If the
		   pixmaps that we get from source don't match our viewport,
		   we will have to resize them by this much. */
		const double xmpp = gisview.get_viking_scale().get_x();
		const double ympp = gisview.get_viking_scale().get_y();
		result = TilePixmapResize(this->map_zoom_x / xmpp, this->map_zoom_y / ympp);
	}

	return result;
}




bool LayerMap::validate_tile_pixmap_resize(const TilePixmapResize & tile_pixmap_resize, bool & existence_only) const
{
	/* This part I don't understand yet. */

	if (!tile_pixmap_resize.resize_factors_in_allowed_range()) {
		if (tile_pixmap_resize.resize_factors_in_existence_only_range()) {
			qDebug() << SG_PREFIX_D << "existence_only due to SHRINKFACTORS";
			existence_only = true;
		} else {
			/* Report the reason for not drawing. */
			Window * window = this->get_window();
			if (window) {
				QString msg = tr("Refusing to draw tiles or existence of tiles beyond %1 zoom out factor").arg((int)(1.0 / g_real_min_shrinkfactor));
				window->statusbar_update(StatusBarField::Info, msg);
			}
			return false;
		}
	}

	return true;
}




sg_ret LayerMap::draw_section(GisViewport * gisview, const Coord & coord_ul, const Coord & coord_br)
{
	const TilePixmapResize tile_pixmap_resize = this->get_desired_pixmap_resize(*gisview);
	bool existence_only = false;
	const bool can_continue = this->validate_tile_pixmap_resize(tile_pixmap_resize, existence_only);
	if (!can_continue) {
		return sg_ret::ok;
	}


	/* GisViewport's corner coordinates -> tiles info. */
	TileInfo tile_ul;
	TileInfo tile_br;
	const VikingScale viking_scale = this->get_desired_viking_scale(*gisview);
	if (!this->m_map_source->coord_to_tile_info(coord_ul, viking_scale, tile_ul)
	    || !this->m_map_source->coord_to_tile_info(coord_br, viking_scale, tile_br)) {

		qDebug() << SG_PREFIX_E << "Failed to get tile info";
		return sg_ret::err;
	}


	/* "unordered" meaning that the tiles' x and y ranges may
	   either increase or decrease. */
	const TilesRange unordered_tiles_range = TileInfo::get_tiles_range(tile_ul, tile_br);


	/* Prevent the program grinding to a halt if trying to deal with thousands of tiles
	   which can happen when using a small fixed zoom level and viewing large areas.
	   Also prevents very large number of tile download requests. */
	const int n_tiles = unordered_tiles_range.get_tiles_count();
	if (n_tiles > g_max_tiles) {
		qDebug() << SG_PREFIX_D << "existence_only due to wanting too many tiles:" << n_tiles;
		existence_only = true;
	}

	if ((!existence_only) && this->autodownload  && this->should_start_autodownload(gisview)) {
		qDebug() << SG_PREFIX_D << "Starting autodownload";
		if (!this->adl_only_missing && this->m_map_source->supports_download_only_new()) {
			/* Try to download newer tiles. */
			this->start_download_thread(gisview, coord_ul, coord_br, MapDownloadMode::New);
		} else {
			/* Download only missing tiles. */
			this->start_download_thread(gisview, coord_ul, coord_br, MapDownloadMode::MissingOnly);
		}
	}

	/* The purpose of this assignment is to set fields in
	   tile_iter other than x and y.  x and y will be set and
	   incremented in loops below, but other fields of the iter
	   also need to have some valid values. These valid values are
	   set here. */
	TileInfo tile_iter = tile_ul;

	Coord coord;
	if (this->m_map_source->tilesize_x() == 0 && !existence_only) {

		for (tile_iter.x = unordered_tiles_range.x_first; tile_iter.x <= unordered_tiles_range.x_last; tile_iter.x++) {
			for (tile_iter.y = unordered_tiles_range.y_first; tile_iter.y <= unordered_tiles_range.y_last; tile_iter.y++) {

				TileGeometry tile_geometry;

				tile_geometry.pixmap = this->get_tile_pixmap_with_stretch(tile_iter, tile_pixmap_resize);
				if (tile_geometry.pixmap.isNull()) {
					qDebug() << SG_PREFIX_W << "Pixmap not found";
					continue;
				}
				qDebug() << SG_PREFIX_I << "Pixmap found";

				tile_geometry.begin_x = 0;
				tile_geometry.begin_y = 0;
				tile_geometry.width = tile_geometry.pixmap.width();
				tile_geometry.height = tile_geometry.pixmap.height();

				if (sg_ret::ok != this->m_map_source->tile_info_to_center_coord(tile_iter, coord)) {
					qDebug() << SG_PREFIX_E << "Can't get coord, continue";
					continue;
				}
				gisview->coord_to_screen_pos(coord, &tile_geometry.dest_x, &tile_geometry.dest_y);

				tile_geometry.dest_x -= (tile_geometry.width / 2);
				tile_geometry.dest_y -= (tile_geometry.height / 2);

				qDebug() << SG_PREFIX_I << "Calling draw_pixmap()";
				gisview->draw_pixmap(tile_geometry.pixmap, tile_geometry.dest_x, tile_geometry.dest_y, tile_geometry.begin_x, tile_geometry.begin_y, tile_geometry.width, tile_geometry.height);
			}
		}
	} else {
		const MapCacheObj map_cache_obj(this->m_map_source->is_direct_file_access() ? MapCacheLayout::OSM : this->cache_layout, this->cache_dir);



		/* Tile size is known, don't have to keep converting coords. */
		TileGeometry tile_geometry;
		const double tile_width_f = this->m_map_source->tilesize_x() * tile_pixmap_resize.x;
		const double tile_height_f = this->m_map_source->tilesize_y() * tile_pixmap_resize.y;
		/* ceiled so tiles will be maximum size in the case of funky shrinkfactor. */
		tile_geometry.width = ceil(tile_width_f);
		tile_geometry.height = ceil(tile_height_f);

		if (sg_ret::ok != this->m_map_source->tile_info_to_center_coord(tile_ul, coord)) {
			qDebug() << SG_PREFIX_E << "Can't get coord, return";
			return sg_ret::err;
		}
		gisview->coord_to_screen_pos(coord, &tile_geometry.dest_x, &tile_geometry.dest_y);

		const int viewport_x_grid = tile_geometry.dest_x;
		const int viewport_y_grid = tile_geometry.dest_y;

		/* Using *_f  so tile_geometry.dest_x,tile_geometry.dest_y doubles. this is so shrinkfactors aren't rounded off
		   e.g. if tile size 128, shrinkfactor 0.333. */
		tile_geometry.dest_x -= (tile_width_f / 2);
		const int base_viewport_y = tile_geometry.dest_y - (tile_height_f / 2);



		const int delta_x = (tile_ul.x == unordered_tiles_range.x_first) ? 1 : -1;
		const int delta_y = (tile_ul.y == unordered_tiles_range.y_first) ? 1 : -1;
		TilesRange ordered_tiles_range;
		ordered_tiles_range.x_first = (delta_x == 1) ?  unordered_tiles_range.x_first     :  unordered_tiles_range.x_last;
		ordered_tiles_range.y_first = (delta_y == 1) ?  unordered_tiles_range.y_first     :  unordered_tiles_range.y_last;
		ordered_tiles_range.x_last  = (delta_x == 1) ? (unordered_tiles_range.x_last + 1) : (unordered_tiles_range.x_first - 1);
		ordered_tiles_range.y_last  = (delta_y == 1) ? (unordered_tiles_range.y_last + 1) : (unordered_tiles_range.y_first - 1);



		//existence_only = true;

		for (tile_iter.x = ordered_tiles_range.x_first; tile_iter.x != ordered_tiles_range.x_last; tile_iter.x += delta_x) {
			tile_geometry.dest_y = base_viewport_y;
			for (tile_iter.y = ordered_tiles_range.y_first; tile_iter.y != ordered_tiles_range.y_last; tile_iter.y += delta_y) {

				if (existence_only) {
					this->draw_existence(gisview, tile_iter, tile_geometry, map_cache_obj);
				} else {
					/* Try correct scale first. */
					const int scale_factor = 1;

					/* Since scale_factor == 1, this is not necessary: */
					/*
					   TilePixmapResize scaled_tile_pixmap_resize = scale;
					   scaled_tile_pixmap_resize = scale.resize_down(scale_factor);
					*/

					const TileGeometry found_tile = this->find_tile(tile_iter, tile_geometry, tile_pixmap_resize, scale_factor);
					if (!found_tile.pixmap.isNull()) {
						qDebug() << SG_PREFIX_I << "Calling draw_pixmap to draw found tile";
						gisview->draw_pixmap(found_tile.pixmap, found_tile.dest_x, found_tile.dest_y, found_tile.begin_x, found_tile.begin_y, found_tile.width, found_tile.height);
					}
				}

				tile_geometry.dest_y += tile_height_f;
			}
			tile_geometry.dest_x += tile_width_f;
		}

		/* ATM Only show tile grid lines in extreme debug mode. */
		if (true || (vik_debug && vik_verbose)) {
			/* Grid drawing here so it gets drawn on top of the map.
			   Thus loop around x & y again, but this time separately.
			   Only showing grid for the current scale */
			const QPen pen(QColor(LAYER_MAP_GRID_COLOR));
			LayerMap::draw_grid(gisview, pen, viewport_x_grid, viewport_y_grid, ordered_tiles_range.x_first, delta_x, ordered_tiles_range.x_last + 1, ordered_tiles_range.y_first, delta_y, ordered_tiles_range.y_last + 1, tile_width_f, tile_height_f);
		}
	}

	return sg_ret::ok;
}




void LayerMap::draw_grid(GisViewport * gisview, const QPen & pen, fpixel viewport_x, fpixel viewport_y, fpixel x_begin, fpixel delta_x, fpixel x_end, fpixel y_begin, fpixel delta_y, fpixel y_end, double tile_width, double tile_height)
{
	/* Draw single grid lines across the whole screen. */
	const int center_width = gisview->central_get_width();
	const int center_height = gisview->central_get_height();
	const int base_viewport_x = viewport_x - (tile_width / 2);
	const int base_viewport_y = viewport_y - (tile_height / 2);

	viewport_x = base_viewport_x;
	for (int x = x_begin; x != x_end; x += delta_x) {
		/* Using 'base_viewport_y as a third arg,
		   instead of zero, causes drawing only whole
		   tiles on top of a map. */
		gisview->draw_line(pen, viewport_x, base_viewport_y, viewport_x, center_height);
		viewport_x += tile_width;
	}

	viewport_y = base_viewport_y;
	for (int y = y_begin; y != y_end; y += delta_y) {
		/* Using 'base_viewport_x as a second arg,
		   instead of zero, causes drawing only whole
		   tiles on left size of a map. */
		gisview->draw_line(pen, base_viewport_x, viewport_y, center_width, viewport_y);
		viewport_y += tile_height;
	}
}




void LayerMap::draw_tree_item(GisViewport * gisview, __attribute__((unused)) bool highlight_selected, __attribute__((unused)) bool parent_is_selected)
{
	if (this->m_map_source->get_drawmode() != gisview->get_draw_mode()) {
		return;
	}

	/* Copyright. */
	const LatLonBBox bbox = gisview->get_bbox();
	this->m_map_source->add_copyright(gisview, bbox, gisview->get_viking_scale());

	gisview->add_logo(this->m_map_source->get_logo());

	/* Get corner coords. */
	if (gisview->get_coord_mode() == CoordMode::UTM && ! gisview->get_is_one_utm_zone()) {
		/* UTM multi-zone stuff by Kit Transue. */
		const int leftmost_zone = gisview->get_leftmost_zone();
		const int rightmost_zone = gisview->get_rightmost_zone();
		Coord coord_ul;
		Coord coord_br;
		for (int zone = leftmost_zone; zone <= rightmost_zone; ++zone) {
			gisview->get_corners_for_zone(coord_ul, coord_br, zone);
			this->draw_section(gisview, coord_ul, coord_br);
		}
	} else {
		const Coord coord_ul = gisview->screen_pos_to_coord(ScreenPosition::UpperLeft);
		const Coord coord_br = gisview->screen_pos_to_coord(ScreenPosition::BottomRight);

		this->draw_section(gisview, coord_ul, coord_br);
	}
}




/*************************/
/****** DOWNLOADING ******/
/*************************/




void LayerMap::start_download_thread(GisViewport * gisview, const Coord & coord_ul, const Coord & coord_br, MapDownloadMode map_download_mode)
{
	qDebug() << SG_PREFIX_I << "Map:" << (quintptr) this->m_map_source << "map index" << (int) this->m_map_type_id;

	/* Don't ever attempt download on direct access. */
	if (this->m_map_source->is_direct_file_access()) {
		qDebug() << SG_PREFIX_I << "Not downloading, direct access";
		return;
	}

	TileInfo tile_ul;
	TileInfo tile_br;
	const VikingScale viking_scale = this->get_desired_viking_scale(*gisview);
	if (!this->m_map_source->coord_to_tile_info(coord_ul, viking_scale, tile_ul)
	    || !this->m_map_source->coord_to_tile_info(coord_br, viking_scale, tile_br)) {

		qDebug() << SG_PREFIX_E << "Conversion coord to tile failed";
		return;
	}


	MapDownloadJob * mdj = new MapDownloadJob(this, tile_ul, tile_br, true, map_download_mode);

	if (mdj->m_map_download_mode == MapDownloadMode::MissingOnly) {
		mdj->n_items = mdj->calculate_tile_count_to_download();
	} else {
		mdj->n_items = mdj->calculate_total_tile_count_to_download();
	}

	if (mdj->n_items) {
		mdj->set_description(map_download_mode, mdj->n_items, this->m_map_source->ui_label());
		mdj->run_in_background(ThreadPoolType::Remote);
	} else {
		delete mdj;
	}
}




void LayerMap::download_section_sub(const Coord & coord_ul, const Coord & coord_br, const VikingScale & viking_scale, MapDownloadMode map_download_mode)
{
	/* Don't ever attempt download on direct access. */
	if (this->m_map_source->is_direct_file_access()) {
		return;
	}

	if (!viking_scale.x_y_is_equal()) {
		qDebug() << SG_PREFIX_E << "Unequal zoom levels";
		return;
	}

	TileInfo tile_ul;
	TileInfo tile_br;

	if (!this->m_map_source->coord_to_tile_info(coord_ul, viking_scale, tile_ul)
	    || !this->m_map_source->coord_to_tile_info(coord_br, viking_scale, tile_br)) {
		qDebug() << SG_PREFIX_W << "coord_to_tile_info() failed";
		return;
	}

	MapDownloadJob * mdj = new MapDownloadJob(this, tile_ul, tile_br, true, map_download_mode);

	mdj->n_items = mdj->calculate_tile_count_to_download();
	if (mdj->n_items) {
		mdj->set_description(map_download_mode, mdj->n_items, this->m_map_source->ui_label());
		mdj->run_in_background(ThreadPoolType::Remote);
	} else {
		delete mdj;
	}
}




/**
 * @ul:   Upper left coordinate of the area to be downloaded
 * @br:   Bottom right coordinate of the area to be downloaded
 * @zoom: The zoom level at which the maps are to be download
 *
 * Download a specified map area at a certain zoom level
 */
void LayerMap::download_section(const Coord & coord_ul, const Coord & coord_br, const VikingScale & viking_scale)
{
	this->download_section_sub(coord_ul, coord_br, viking_scale, MapDownloadMode::MissingOnly);
}




void LayerMap::redownload_bad_cb(void)
{
	this->start_download_thread(this->redownload_gisview, this->redownload_ul, this->redownload_br, MapDownloadMode::MissingAndBad);
}




void LayerMap::redownload_all_cb(void)
{
	this->start_download_thread(this->redownload_gisview, this->redownload_ul, this->redownload_br, MapDownloadMode::All);
}




void LayerMap::redownload_new_cb(void)
{
	this->start_download_thread(this->redownload_gisview, this->redownload_ul, this->redownload_br, MapDownloadMode::New);
}




/**
 * Display a simple dialog with information about this particular map tile
 */
void LayerMap::tile_info_cb(void)
{
        const VikingScale viking_scale = this->get_desired_viking_scale(*this->redownload_gisview);
	TileInfo tile_info;
	if (!this->m_map_source->coord_to_tile_info(this->redownload_ul, viking_scale, tile_info)) {
		return;
	}

	const MapCacheObj map_cache_obj(this->cache_layout, this->cache_dir);

	const QStringList tile_info_strings = this->m_map_source->get_tile_description(map_cache_obj, tile_info);

	Dialog::info(tr("Tile Information"), tile_info_strings, this->get_window());
}




void SlavGPS::tile_info_add_file_info_strings(QStringList & items, const QString & tile_file_full_path)
{
	QString file_info;
	QString timestamp_info;

	if (0 == access(tile_file_full_path.toUtf8().constData(), F_OK)) {
		file_info = QObject::tr("Tile File: %1").arg(tile_file_full_path);
		/* Get timestamp information about the tile. */
		struct stat stat_buf;
		if (0 == stat(tile_file_full_path.toUtf8().constData(), &stat_buf)) {
			const Time timestamp(stat_buf.st_mtime, TimeType::Unit::internal_unit());
			timestamp_info = QObject::tr("Tile File Timestamp: %1").arg(timestamp.strftime_utc("%c"));
		} else {
			timestamp_info = QObject::tr("Tile File Timestamp: Not Available");
		}
	} else {
		file_info = QObject::tr("Tile File: %1 [Not Available]").arg(tile_file_full_path);
		timestamp_info = "";
	}

	items.push_back(file_info);
	items.push_back(timestamp_info);
}




LayerTool::Status LayerToolMapsDownload::handle_mouse_release(Layer * _layer, QMouseEvent * event)
{
	if (nullptr == _layer) {
		qDebug() << SG_PREFIX_E << "NULL pointer argument";
		return LayerTool::Status::Error;
	}
	if (_layer->m_kind != LayerKind::Map) {
		qDebug() << SG_PREFIX_E << "Invalid layer kind" << _layer->m_kind;
		return LayerTool::Status::Error;
	}

	LayerMap * layer = (LayerMap *) _layer;

	if (layer->dl_tool_x == -1 || layer->dl_tool_y == -1) {
		return LayerTool::Status::Ignored;
	}

	const int leftmost_pixel   = this->gisview->central_get_leftmost_pixel();
	const int rightmost_pixel  = this->gisview->central_get_rightmost_pixel();
	const int topmost_pixel    = this->gisview->central_get_topmost_pixel();
	const int bottommost_pixel = this->gisview->central_get_bottommost_pixel();

	if (event->button() == Qt::LeftButton) {
		const int ul_x = std::max(rightmost_pixel, std::min(event->x(), layer->dl_tool_x));
		const int ul_y = std::max(topmost_pixel, std::min(event->y(), layer->dl_tool_y));
		const Coord coord_ul = this->gisview->screen_pos_to_coord(ul_x, ul_y);

		const int br_x = std::min(leftmost_pixel, std::max(event->x(), layer->dl_tool_x));
		const int br_y = std::min(bottommost_pixel, std::max(event->y(), layer->dl_tool_y));
		const Coord coord_br = this->gisview->screen_pos_to_coord(br_x, br_y);

		layer->start_download_thread(this->gisview, coord_ul, coord_br, MapDownloadMode::DownloadAndRefresh);
		layer->dl_tool_x = -1;
		layer->dl_tool_y = -1;

		return LayerTool::Status::Handled;
	} else {
		const int ul_x = std::max(rightmost_pixel, std::min(event->x(), layer->dl_tool_x));
		const int ul_y = std::max(topmost_pixel, std::min(event->y(), layer->dl_tool_y));
		layer->redownload_ul = this->gisview->screen_pos_to_coord(ul_x, ul_y);

		const int br_x = std::min(leftmost_pixel, std::max(event->x(), layer->dl_tool_x));
		const int br_y = std::min(bottommost_pixel, std::max(event->y(), layer->dl_tool_y));
		layer->redownload_br = this->gisview->screen_pos_to_coord(br_x, br_y);

		layer->redownload_gisview = this->gisview;

		layer->dl_tool_x = -1;
		layer->dl_tool_y = -1;

		if (!layer->dl_right_click_menu) {
			QAction * action = NULL;
			layer->dl_right_click_menu = new QMenu();

			action = new QAction(QObject::tr("Redownload &Bad Map(s)"), layer);
			layer->dl_right_click_menu->addAction(action);
			QObject::connect(action, SIGNAL (triggered(bool)), layer, SLOT (redownload_bad_cb));

			action = new QAction(QObject::tr("Redownload &New Map(s)"), layer);
			layer->dl_right_click_menu->addAction(action);
			QObject::connect(action, SIGNAL (triggered(bool)), layer, SLOT (redownload_new_cb(void)));

			action = new QAction(QObject::tr("Redownload &All Map(s)"), layer);
			layer->dl_right_click_menu->addAction(action);
			QObject::connect(action, SIGNAL (triggered(bool)), layer, SLOT (redownload_all_cb(void)));

			action = new QAction(QObject::tr("&Show Tile Information"), layer);
			layer->dl_right_click_menu->addAction(action);
			action->setIcon(QIcon::fromTheme("help-about"));
			QObject::connect(action, SIGNAL (triggered(bool)), layer, SLOT (tile_info_cb(void)));
		}
		layer->dl_right_click_menu->exec(QCursor::pos());

		return LayerTool::Status::Handled;
	}
}




LayerToolMapsDownload::LayerToolMapsDownload(Window * window_, GisViewport * gisview_) : LayerTool(window_, gisview_, LayerKind::Map)
{
	this->action_icon_path   = ":/icons/layer_tool/map_download_18.png";
	this->action_label       = QObject::tr("Maps Download");
	this->action_tooltip     = QObject::tr("Maps Download");
	// this->action_accelerator = ...; /* Empty accelerator. */

	this->cursor_click = QCursor(QPixmap(":cursors/maps_download.png"), 0, 0);
	this->cursor_release = QCursor(QPixmap(":cursors/maps_download.png"), 0, 0);
}




SGObjectTypeID LayerToolMapsDownload::get_tool_id(void) const
{
	return LayerToolMapsDownload::tool_id();
}
SGObjectTypeID LayerToolMapsDownload::tool_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.tool.layer_map.maps_download");
	return id;
}




LayerTool::Status LayerToolMapsDownload::handle_mouse_click(Layer * _layer, QMouseEvent * event)
{
	TileInfo tmp;
	if (nullptr == _layer) {
		qDebug() << SG_PREFIX_E << "NULL pointer argument";
		return LayerTool::Status::Error;
	}
	if (_layer->m_kind != LayerKind::Map) {
		qDebug() << SG_PREFIX_E << "Invalid layer kind" << _layer->m_kind;
		return LayerTool::Status::Error;
	}

	LayerMap * layer = (LayerMap *) _layer;

	const VikingScale viking_scale = layer->get_desired_viking_scale(*this->gisview);
	if (layer->map_source()->get_drawmode() == this->gisview->get_draw_mode()
	    && layer->map_source()->coord_to_tile_info(this->gisview->get_center_coord(),
						       viking_scale,
						       tmp)) {

		layer->dl_tool_x = event->x();
		layer->dl_tool_y = event->y();
		return LayerTool::Status::Handled;
	}
	return LayerTool::Status::Ignored;
}




void LayerMap::download_onscreen_maps(MapDownloadMode map_download_mode)
{
	GisViewport * gisview = this->get_window()->get_main_gis_view();

	const Coord coord_ul = gisview->screen_pos_to_coord(ScreenPosition::UpperLeft);
	const Coord coord_br = gisview->screen_pos_to_coord(ScreenPosition::BottomRight);

	const GisViewportDrawMode map_draw_mode = this->m_map_source->get_drawmode();
	const GisViewportDrawMode vp_draw_mode = gisview->get_draw_mode();

	const VikingScale viking_scale = this->get_desired_viking_scale(*gisview);

	TileInfo tile_ul;
	TileInfo tile_br;
	if (map_draw_mode == vp_draw_mode
	    && this->m_map_source->coord_to_tile_info(coord_ul, viking_scale, tile_ul)
	    && this->m_map_source->coord_to_tile_info(coord_br, viking_scale, tile_br)) {

		this->start_download_thread(gisview, coord_ul, coord_br, map_download_mode);

	} else if (map_draw_mode != vp_draw_mode) {
		const QString drawmode_name = GisViewportDrawModes::get_label_with_accelerator(map_draw_mode);
		const QString err = QObject::tr("Wrong drawmode for this map.\nSelect \"%1\" from View menu and try again.").arg(drawmode_name);
		Dialog::error(err, this->get_window());
	} else {
		Dialog::error(QObject::tr("Wrong zoom level for this map."), this->get_window());
	}

}




void LayerMap::download_missing_onscreen_maps_cb(void)
{
	this->download_onscreen_maps(MapDownloadMode::MissingOnly);
}




void LayerMap::download_new_onscreen_maps_cb(void)
{
	this->download_onscreen_maps(MapDownloadMode::New);
}




void LayerMap::redownload_all_onscreen_maps_cb(void)
{
	this->download_onscreen_maps(MapDownloadMode::All);
}




void LayerMap::about_cb(void)
{
	if (this->m_map_source->get_license().isEmpty()) {
		Dialog::info(this->m_map_source->ui_label(), this->get_window());
	} else {
		maps_show_license(this->get_window(), this->m_map_source);
	}
}




/**
 * Copied from maps_layer_download_section but without the actual download and this returns a value
 */
int LayerMap::how_many_maps(const Coord & coord_ul, const Coord & coord_br, const VikingScale & viking_scale, MapDownloadMode map_download_mode)
{
	if (this->m_map_source->is_direct_file_access()) {
		return 0;
	}

	if (!viking_scale.x_y_is_equal()) {
		qDebug() << SG_PREFIX_E << "Unequal zoom levels";
		return 0;
	}

	TileInfo tile_ul;
	TileInfo tile_br;
	if (!this->m_map_source->coord_to_tile_info(coord_ul, viking_scale, tile_ul)
	    || !this->m_map_source->coord_to_tile_info(coord_br, viking_scale, tile_br)) {
		qDebug() << SG_PREFIX_W << "coord_to_tile_info() failed";
		return 0;
	}

	MapDownloadJob * mdj = new MapDownloadJob(this, tile_ul, tile_br, false, map_download_mode);
	int n_items = 0;

	if (mdj->m_map_download_mode == MapDownloadMode::All) {
		n_items = mdj->calculate_total_tile_count_to_download();
	} else {
		n_items = mdj->calculate_tile_count_to_download();
	}

	delete mdj;

	return n_items;
}




DownloadMethodsAndZoomsDialog::DownloadMethodsAndZoomsDialog(const QString & title,
							     const std::vector<VikingScale> & viking_scales,
							     const std::vector<MapDownloadMode> & download_modes,
							     QWidget * parent) : BasicDialog(parent)
{
	this->setWindowTitle(title);


	int row = 0;


	this->grid->addWidget(new QLabel(QObject::tr("Zoom Start:")), row, 0);
	this->smaller_zoom_combo = new QComboBox();
	for (unsigned int i = 0; i < viking_scales.size(); i++) {
		this->smaller_zoom_combo->addItem(viking_scales.at(i).to_string(), i);
	}
	this->grid->addWidget(this->smaller_zoom_combo, row, 1);
	row++;


	this->grid->addWidget(new QLabel(QObject::tr("Zoom End:")), row, 0);
	this->larger_zoom_combo = new QComboBox();
	for (unsigned int i = 0; i < viking_scales.size(); i++) {
		this->larger_zoom_combo->addItem(viking_scales.at(i).to_string(), i);
	}
	this->grid->addWidget(this->larger_zoom_combo, row, 1);
	row++;


	this->grid->addWidget(new QLabel(QObject::tr("Download Maps Mode:")), row, 0);
	this->download_mode_combo = new QComboBox();
	for (unsigned int i = 0; i < download_modes.size(); i++) {
		this->download_mode_combo->addItem(to_string(download_modes[i]), i);
	}
	this->grid->addWidget(this->download_mode_combo, row, 1);
	row++;
}




void DownloadMethodsAndZoomsDialog::preselect(int smaller_zoom_idx, int larger_zoom_idx, int download_mode_idx)
{
	this->smaller_zoom_combo->setCurrentIndex(smaller_zoom_idx);
	this->larger_zoom_combo->setCurrentIndex(larger_zoom_idx);
	this->download_mode_combo->setCurrentIndex(download_mode_idx);
}




int DownloadMethodsAndZoomsDialog::get_download_mode_idx(void) const
{
	return this->download_mode_combo->currentIndex();
}




int DownloadMethodsAndZoomsDialog::get_smaller_zoom_idx(void) const
{
	return this->smaller_zoom_combo->currentIndex();
}




int DownloadMethodsAndZoomsDialog::get_larger_zoom_idx(void) const
{
	return this->larger_zoom_combo->currentIndex();
}




/* My best guess of sensible limits. */
#define REALLY_LARGE_AMOUNT_OF_TILES 5000
#define CONFIRM_LARGE_AMOUNT_OF_TILES 500

/**
 * Get all maps in the region for zoom levels specified by the user
 * Sort of similar to LayerTRW::download_map_along_track_cb().
 */
void LayerMap::download_all_cb(void)
{
	GisViewport * gisview = this->get_window()->get_main_gis_view();

	/* I don't think we should allow users to hammer the servers too much...
	   Deliberately not allowing lowest zoom levels.
	   Still can give massive numbers to download.
	   A screen size of 1600x1200 gives around 300,000 tiles between 1..128 when none exist before!! */

	const std::vector<VikingScale> viking_scales = {
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

	/* Redownload method - needs to align with REDOWNLOAD* macro values. */
	const std::vector<MapDownloadMode> download_modes = {
		MapDownloadMode::MissingOnly,
		MapDownloadMode::MissingAndBad,
		MapDownloadMode::New,
		MapDownloadMode::All,
		/* MapDownloadMode::DownloadAndRefresh TODO_MAYBE: do we add this mode here? It wasn't present in Viking. */
	};
	const int download_mode_idx = 0; /* MapDownloadMode::MissingOnly */


	const VikingScale current_viking_scale = gisview->get_viking_scale().get_x();
	int larger_zoom_idx = 0;
	int smaller_zoom_idx = 0;
	if (sg_ret::ok != VikingScale::get_closest_index(larger_zoom_idx, viking_scales, current_viking_scale)) {
		qDebug() << SG_PREFIX_W << "Failed to get the closest viking scale";
		larger_zoom_idx = viking_scales.size() - 1;
	}
	/* Default to only 2 zoom levels below the current one. */
	smaller_zoom_idx = (larger_zoom_idx >= 2) ? larger_zoom_idx - 2 : larger_zoom_idx;


	const QString title = QObject::tr("%1: Download for Zoom Levels").arg(this->get_map_type_ui_label());
	DownloadMethodsAndZoomsDialog dialog(title, viking_scales, download_modes, this->get_window());
	dialog.preselect(smaller_zoom_idx, larger_zoom_idx, download_mode_idx);


	const int selected_smaller_zoom_idx = dialog.get_smaller_zoom_idx();
	const int selected_larger_zoom_idx = dialog.get_larger_zoom_idx();
	const int selected_download_mode_idx = dialog.get_download_mode_idx();
	const MapDownloadMode selected_download_mode = download_modes[selected_download_mode_idx];


	if (dialog.exec() != QDialog::Accepted) {
		/* Cancelled. */
		return;
	}


	/* Find out new current positions. */
	const LatLonBBox bbox = gisview->get_bbox();
	const Coord coord_ul(LatLon(bbox.north, bbox.west), gisview->get_coord_mode());
	const Coord coord_br(LatLon(bbox.south, bbox.east), gisview->get_coord_mode());

	/* Get Maps Count - call for each zoom level (in reverse).
	   With MapDownloadMode::New this is a possible maximum.
	   With MapDownloadMode::MisingOnly this only missing ones - however still has a server lookup per tile. */
	int map_count = 0;
	for (int zz = selected_larger_zoom_idx; zz >= selected_smaller_zoom_idx; zz--) {
		map_count = map_count + this->how_many_maps(coord_ul, coord_br, viking_scales[zz], selected_download_mode);
	}

	qDebug() << SG_PREFIX_D << "Download request map count" << map_count << "for method" << to_string(selected_download_mode);

	/* Absolute protection of hammering a map server. */
	if (map_count > REALLY_LARGE_AMOUNT_OF_TILES) {
		const QString msg = QObject::tr("You are not allowed to download more than %1 tiles in one go (requested %2)").arg(REALLY_LARGE_AMOUNT_OF_TILES).arg(map_count);
		Dialog::error(msg, this->get_window());
		return;
	}

	/* Confirm really want to do this. */
	if (map_count > CONFIRM_LARGE_AMOUNT_OF_TILES) {
		const QString msg = QObject::tr("Do you really want to download %1 tiles?").arg(map_count);
		if (false == Dialog::yes_or_no(msg, this->get_window())) {
			return;
		}
	}

	/* Get Maps - call for each zoom level (in reverse). */
	for (int zz = selected_larger_zoom_idx; zz >= selected_smaller_zoom_idx; zz--) {
		this->download_section_sub(coord_ul, coord_br, viking_scales[zz], selected_download_mode);
	}
}




void LayerMap::flush_cb(void)
{
	MapCache::flush_type(this->m_map_source->map_type_id());
}




sg_ret LayerMap::menu_add_type_specific_operations(QMenu & menu, __attribute__((unused)) bool in_tree_view)
{
	QAction * qa = NULL;

	qa = new QAction(QObject::tr("Download &Missing Onscreen Maps"), this);
	qa->setIcon(QIcon::fromTheme("list-add"));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (download_missing_onscreen_maps_cb(void)));
	menu.addAction(qa);

	if (this->m_map_source->supports_download_only_new()) {
		qa = new QAction(QObject::tr("Download &New Onscreen Maps"), this);
		qa->setIcon(QIcon::fromTheme("edit-redo"));
		QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (download_new_onscreen_maps_cb(void)));
		menu.addAction(qa);
	}

	qa = new QAction(QObject::tr("Reload &All Onscreen Maps"), this);
	qa->setIcon(QIcon::fromTheme("view-refresh"));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (redownload_all_onscreen_maps_cb(void)));
	menu.addAction(qa);

	qa = new QAction(QObject::tr("Download Maps in &Zoom Levels..."), this);
	qa->setIcon(QIcon::fromTheme("list-add"));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (download_all_cb(void)));
	menu.addAction(qa);

	qa = new QAction(QObject::tr("About"), this);
	qa->setIcon(QIcon::fromTheme("help-about"));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (about_cb(void)));
	menu.addAction(qa);

	/* Typical users shouldn't need to use this functionality - so debug only ATM. */
	if (vik_debug) {
		qa = new QAction(QObject::tr("Flush Map Cache"), this);
		qa->setIcon(QIcon::fromTheme("edit-clear"));
		QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (flush_cb(void)));
		menu.addAction(qa);
	}

	return sg_ret::ok;
}




/**
 * Enable downloading maps of the current screen area either 'new' or 'everything'.
 */
void LayerMap::download(GisViewport * gisview, bool only_new)
{
	if (!gisview) {
		return;
	}

	if (only_new) {
		/* Get only new maps. */
		this->download_new_onscreen_maps_cb();
	} else {
		/* Redownload everything. */
		this->redownload_all_onscreen_maps_cb();
	}
}



LayerMap::LayerMap()
{
	qDebug() << SG_PREFIX_D << "Constructor called";

	this->m_kind = LayerKind::Map;
	strcpy(this->debug_string, "MAP");
	this->interface = &vik_map_layer_interface;

	this->set_initial_parameter_values();
	this->set_name(Layer::get_translated_layer_kind_string(this->m_kind));
}




QString SlavGPS::to_string(MapDownloadMode download_mode)
{
	QString result;

	switch (download_mode) {
	case MapDownloadMode::MissingOnly:
		result = QObject::tr("Only Missing Maps");
		break;
	case MapDownloadMode::MissingAndBad:
		result = QObject::tr("Missing and Bad Maps");
		break;
	case MapDownloadMode::New:
		result = QObject::tr("New Maps");
		break;
	case MapDownloadMode::All:
		result = QObject::tr("Re-download All Maps");
		break;
	case MapDownloadMode::DownloadAndRefresh:
		result = QObject::tr("Download Missing and Refresh Cache");
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected download mode" << (int) download_mode;
		result = QObject::tr("Unknown Mode");
		break;
	}

	return result;
}





sg_ret LayerMap::handle_downloaded_tile_cb(void)
{
	this->emit_tree_item_changed("Indicating change to layer in response to downloading new map tile");
	return sg_ret::ok;
}




bool LayerMap::is_tile_visible(__attribute__((unused)) const TileInfo & tile_info)
{
	/* TODO_LATER: implement. */
	return true;
}




VikingScale LayerMap::get_desired_viking_scale(const GisViewport & gisview) const
{
	VikingScale result;

	if (this->map_zoom_id == LAYER_MAP_ZOOM_ID_FOLLOW_VIEWPORT_ZOOM_LEVEL) {
		/*
		  User wants to get from Map Source the tiles for zoom
		  level that matches current zoom level of viewport.
		  So just get the scale from viewport.
		*/
		result = gisview.get_viking_scale();
	} else {
		/*
		  If we get here, then this probably means "Don't look
		  at what zoom level Z is now currently selected by
		  user in main viewport (e.g. with mouse scroll
		  up/down) when trying to find a suitable tile pixmap.
		  Look at zoom level L selected in layer's properties
		  instead."

		  This probably also mean "when getting tiles from map
		  source, don't ask source for tiles with the zoom
		  level Z, but with the zoom level L".

		  If what we have from map source is a tile pixmap
		  corresponding to zoom level L, but currently
		  viewport is at level Z, then in order to display
		  that pixmap correctly (in correct place and with
		  correct size) then we will probably have to resize
		  it (stretch it).
		*/
		const double xmpp = this->map_zoom_x;
		const double ympp = this->map_zoom_y;
		result = VikingScale(xmpp, ympp);
	}

	return result;
}




TilePixmapResize::TilePixmapResize(double new_x, double new_y)
{
	this->x = new_x;
	this->y = new_y;
}




bool TilePixmapResize::resize_factors_in_allowed_range(void) const
{
	return (this->x > g_min_shrinkfactor && this->x < g_max_shrinkfactor &&
		this->y > g_min_shrinkfactor && this->y < g_max_shrinkfactor);
}




bool TilePixmapResize::resize_factors_in_existence_only_range(void) const
{
	return this->x > g_real_min_shrinkfactor && this->y > g_real_min_shrinkfactor;
}




void TilePixmapResize::resize_down(int resize_times)
{
	this->x *= resize_times;
	this->y *= resize_times;
}



void TilePixmapResize::resize_up(int resize_times)
{
	this->x /= resize_times;
	this->y /= resize_times;
}




void TileGeometry::resize_up(int scale_factor)
{
	this->width /= scale_factor;
	this->height /= scale_factor;
}




TileGeometry LayerMap::find_tile(const TileInfo & tile_info, const TileGeometry & tile_geometry, const TilePixmapResize & tile_pixmap_resize, int scale_factor)
{
	TileGeometry result;

	result.pixmap = this->get_tile_pixmap_with_stretch(tile_info, tile_pixmap_resize);
	if (!result.pixmap.isNull()) {
		qDebug() << SG_PREFIX_I << "Non-re-scaled pixmap found";

		result.dest_x = tile_geometry.dest_x;
		result.dest_y = tile_geometry.dest_y;
		result.begin_x = (tile_info.x % scale_factor) * tile_geometry.width;
		result.begin_y = (tile_info.y % scale_factor) * tile_geometry.height;
		result.width = tile_geometry.width;
		result.height = tile_geometry.height;

	} else {
		qDebug() << SG_PREFIX_I << "Non-re-scaled pixmap not found, will look for re-scaled pixmap";

		/* Otherwise try different scales. */
		if (g_scale_smaller_zoom_first) {
			result = this->find_resized_down_tile(tile_info, tile_geometry, tile_pixmap_resize);
			if (result.pixmap.isNull()) {
				result = this->find_resized_up_tile(tile_info, tile_geometry, tile_pixmap_resize);
			}
		} else {
			result = this->find_resized_up_tile(tile_info, tile_geometry, tile_pixmap_resize);
			if (result.pixmap.isNull()) {
				result = this->find_resized_down_tile(tile_info, tile_geometry, tile_pixmap_resize);
			}
		}
	}

	return result;
}




void LayerMap::draw_existence(GisViewport * gisview, const TileInfo & tile_info, const TileGeometry & tile_geometry, const MapCacheObj & map_cache_obj)
{
	const QString path_buf = map_cache_obj.get_cache_file_full_path(tile_info,
									this->m_map_source->map_type_id(),
									this->m_map_source->map_type_string(),
									this->m_map_source->get_file_extension());

	if (0 == access(path_buf.toUtf8().constData(), F_OK)) {
		const QPen pen(QColor(LAYER_MAP_GRID_COLOR));
		gisview->draw_line(pen, tile_geometry.dest_x + tile_geometry.width,
				   tile_geometry.dest_y,
				   tile_geometry.dest_x,
				   tile_geometry.dest_y + tile_geometry.height);
	}
}
