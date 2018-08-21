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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <mutex>
#include <map>
#include <cstdlib>
#include <cassert>
#include <cmath>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <QDebug>
#include <QDir>

#include "window.h"
#include "map_source.h"
#include "map_source_slippy.h"
#include "map_source_mbtiles.h"
#include "vikutils.h"
#include "map_utils.h"
#include "map_cache.h"
#include "preferences.h"
#include "layer_map.h"
#include "layer_map_download.h"
#include "metatile.h"
#include "ui_util.h"
#include "layer_defaults.h"
#include "widget_file_entry.h"
#include "dialog.h"
#include "file.h"
#include "application_state.h"
#include "ui_builder.h"
#include "util.h"
#include "statusbar.h"
#include "viewport_internal.h"




#ifdef HAVE_SQLITE3_H
//#undef HAVE_SQLITE3_H
#endif

#ifdef HAVE_SQLITE3_H
#include "sqlite3.h"
#endif




using namespace SlavGPS;




#define SG_MODULE "Map Layer"




LayerMapInterface vik_map_layer_interface;




extern bool vik_debug;




#define LAYER_MAP_GRID_COLOR "#E6202E" /* Red-ish. */

#define VIK_SETTINGS_MAP_MAX_TILES "maps_max_tiles"
static int MAX_TILES = 1000;

#define VIK_SETTINGS_MAP_MIN_SHRINKFACTOR "maps_min_shrinkfactor"
#define VIK_SETTINGS_MAP_MAX_SHRINKFACTOR "maps_max_shrinkfactor"
static double MAX_SHRINKFACTOR = 8.0000001; /* zoom 1 viewing 8-tiles. */
static double MIN_SHRINKFACTOR = 0.0312499; /* zoom 32 viewing 1-tiles. */

#define VIK_SETTINGS_MAP_REAL_MIN_SHRINKFACTOR "maps_real_min_shrinkfactor"
static double REAL_MIN_SHRINKFACTOR = 0.0039062499; /* If shrinkfactor is between MAX and REAL_MAX, will only check for existence. */

#define VIK_SETTINGS_MAP_SCALE_INC_UP "maps_scale_inc_up"
static unsigned int SCALE_INC_UP = 2;
#define VIK_SETTINGS_MAP_SCALE_INC_DOWN "maps_scale_inc_down"
static unsigned int SCALE_INC_DOWN = 4;
#define VIK_SETTINGS_MAP_SCALE_SMALLER_ZOOM_FIRST "maps_scale_smaller_zoom_first"
static bool SCALE_SMALLER_ZOOM_FIRST = true;

/****** MAP TYPES ******/

static std::map<MapTypeID, MapSource *> map_source_interfaces;


/* List of labels (string) and IDs (MapTypeID) for each map type. */
static std::vector<SGLabelID> map_types;


static std::vector<SGLabelID> params_mapzooms = {
	SGLabelID(QObject::tr("Use Viking Zoom Level"), 0),
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
};

static double __mapzooms_x[] = { 0.0, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 256.0, 512.0, 1024.0, 1.016, 2.4384, 2.54, 5.08, 10.16, 20.32, 25.4 };
static double __mapzooms_y[] = { 0.0, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 256.0, 512.0, 1024.0, 1.016, 2.4384, 2.54, 5.08, 10.16, 20.32, 25.4 };




static void draw_grid(Viewport * viewport, int viewport_x, int viewport_y, int x_begin, int delta_x, int x_end, int y_begin, int delta_y, int y_end, int tilesize_x, int tilesize_y);




static ParameterScale<int> scale_alpha(0, 255, SGVariant((int32_t) 255), 3, 0);




static SGVariant id_default(void)
{
	return SGVariant((int32_t) MapTypeID::MapQuestOSM);
}




static SGVariant directory_default(void)
{
	const SGVariant pref_value = Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL ".maplayer_default_dir");
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




static SGVariant mapzoom_default(void)
{
	return SGVariant((int32_t) 0);
}




static std::vector<SGLabelID> cache_types = {
	SGLabelID("Viking", (int) MapCacheLayout::Viking),
	SGLabelID("OSM",    (int) MapCacheLayout::OSM),
};

static MapCacheLayout cache_layout_default_value = MapCacheLayout::Viking;




static SGVariant cache_layout_default(void)
{
	return SGVariant((int32_t) cache_layout_default_value);
}




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

ParameterSpecification maps_layer_param_specs[] = {
	/* 'mode' is really map source type id, but can't break file format just to rename the parameter name to something better. */
	{ PARAM_MAP_TYPE_ID,   NULL, "mode",           SGVariantType::Int,     PARAMETER_GROUP_GENERIC, QObject::tr("Map Type:"),                            WidgetType::ComboBox,      &map_types,       id_default,           NULL, NULL },
	{ PARAM_CACHE_DIR,     NULL, "directory",      SGVariantType::String,  PARAMETER_GROUP_GENERIC, QObject::tr("Maps Directory:"),                      WidgetType::FolderEntry,   NULL,             directory_default,    NULL, NULL },
	{ PARAM_CACHE_LAYOUT,  NULL, "cache_type",     SGVariantType::Int,     PARAMETER_GROUP_GENERIC, QObject::tr("Cache Layout:"),                        WidgetType::ComboBox,      &cache_types,     cache_layout_default, NULL, N_("This determines the tile storage layout on disk") },
	{ PARAM_FILE,          NULL, "mapfile",        SGVariantType::String,  PARAMETER_GROUP_GENERIC, QObject::tr("Raster MBTiles Map File:"),             WidgetType::FileSelector,  map_file_type,    file_default,         NULL, N_("A raster MBTiles file. Only applies when the map type method is 'MBTiles'") },
	{ PARAM_ALPHA,         NULL, "alpha",          SGVariantType::Int,     PARAMETER_GROUP_GENERIC, QObject::tr("Alpha:"),                               WidgetType::HScale,        &scale_alpha,     NULL,                 NULL, N_("Control the Alpha value for transparency effects") },
	{ PARAM_AUTO_DOWNLOAD, NULL, "autodownload",   SGVariantType::Boolean, PARAMETER_GROUP_GENERIC, QObject::tr("Autodownload maps:"),                   WidgetType::CheckButton,   NULL,             sg_variant_true,      NULL, NULL },
	{ PARAM_ONLY_MISSING,  NULL, "adlonlymissing", SGVariantType::Boolean, PARAMETER_GROUP_GENERIC, QObject::tr("Autodownload Only Gets Missing Maps:"), WidgetType::CheckButton,   NULL,             sg_variant_false,     NULL, N_("Using this option avoids attempting to update already acquired tiles. This can be useful if you want to restrict the network usage, without having to resort to manual control. Only applies when 'Autodownload Maps' is on.") },
	{ PARAM_MAP_ZOOM,      NULL, "mapzoom",        SGVariantType::Int,     PARAMETER_GROUP_GENERIC, QObject::tr("Zoom Level:"),                          WidgetType::ComboBox,      &params_mapzooms, mapzoom_default,      NULL, N_("Determines the method of displaying map tiles for the current zoom level. 'Viking Zoom Level' uses the best matching level, otherwise setting a fixed value will always use map tiles of the specified value regardless of the actual zoom level.") },

	{ NUM_PARAMS,          NULL, NULL,             SGVariantType::Empty,   PARAMETER_GROUP_GENERIC, QString(""),                                         WidgetType::None,          NULL,             NULL,                 NULL, NULL }, /* Guard. */
};




void LayerMap::set_autodownload_default(bool autodownload)
{
	maps_layer_param_specs[PARAM_AUTO_DOWNLOAD].hardcoded_default_value = autodownload ? sg_variant_true : sg_variant_false;
}




void LayerMap::set_cache_default(MapCacheLayout layout)
{
	/* Override default value returned by the default param function. */
	cache_layout_default_value = layout;
}




LayerMapInterface::LayerMapInterface()
{
	this->parameters_c = maps_layer_param_specs; /* Parameters. */

	this->fixed_layer_type_string = "Map"; /* Non-translatable. */

	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_M;
	// this->action_icon = ...; /* Set elsewhere. */

	this->ui_labels.new_layer = QObject::tr("New Map Layer");
	this->ui_labels.layer_type = QObject::tr("Map");
	this->ui_labels.layer_defaults = QObject::tr("Default Settings of Map Layer");
}




LayerToolContainer * LayerMapInterface::create_tools(Window * window, Viewport * viewport)
{
	/* This method should be called only once. */
	static bool created = false;
	if (created) {
		return NULL;
	}

	auto tools = new LayerToolContainer;

	LayerTool * tool = new LayerToolMapsDownload(window, viewport);
	tools->insert({{ tool->id_string, tool }});

	created = true;

	return tools;
}



static ParameterSpecification prefs[] = {
	{ (param_id_t) LayerType::Max, PREFERENCES_NAMESPACE_GENERAL, "maplayer_default_dir", SGVariantType::String, PARAMETER_GROUP_GENERIC, QObject::tr("Default map layer directory:"), WidgetType::FolderEntry, NULL, NULL, NULL, N_("Choose a directory to store cached Map tiles for this layer") },
};




void LayerMap::init(void)
{
	Preferences::register_parameter(prefs[0], SGVariant(MapCache::get_default_maps_dir(), prefs[0].type_id));

	int max_tiles = MAX_TILES;
	if (ApplicationState::get_integer(VIK_SETTINGS_MAP_MAX_TILES, &max_tiles)) {
		MAX_TILES = max_tiles;
	}

	double gdtmp;
	if (ApplicationState::get_double(VIK_SETTINGS_MAP_MIN_SHRINKFACTOR, &gdtmp)) {
		MIN_SHRINKFACTOR = gdtmp;
	}

	if (ApplicationState::get_double(VIK_SETTINGS_MAP_MAX_SHRINKFACTOR, &gdtmp)) {
		MAX_SHRINKFACTOR = gdtmp;
	}

	if (ApplicationState::get_double(VIK_SETTINGS_MAP_REAL_MIN_SHRINKFACTOR, &gdtmp)) {
		REAL_MIN_SHRINKFACTOR = gdtmp;
	}

	int gitmp = 0;
	if (ApplicationState::get_integer(VIK_SETTINGS_MAP_SCALE_INC_UP, &gitmp)) {
		SCALE_INC_UP = gitmp;
	}

	if (ApplicationState::get_integer(VIK_SETTINGS_MAP_SCALE_INC_DOWN, &gitmp)) {
		SCALE_INC_DOWN = gitmp;
	}

	bool gbtmp = true;
	if (ApplicationState::get_boolean(VIK_SETTINGS_MAP_SCALE_SMALLER_ZOOM_FIRST, &gbtmp)) {
		SCALE_SMALLER_ZOOM_FIRST = gbtmp;
	}
}




/***************************************/
/******** MAP LAYER TYPES **************/
/***************************************/




/**
   \brief Register a new MapSource

   Override existing one (equality of id).
*/
void MapSources::register_map_source(MapSource * map_source)
{
	assert (map_source != NULL);

	const MapTypeID map_type_id = map_source->map_type_id;
	const QString label = map_source->get_label();
	assert (!label.isEmpty());

	auto iter = map_source_interfaces.find(map_type_id);
	if (iter == map_source_interfaces.end()) {
		/* Add the map source as new. */

		map_types.push_back(SGLabelID(label, (int) map_type_id));
		map_source_interfaces[map_type_id] = map_source;
		/* TODO: verify in application that properties dialog sees updated list of map types. */
	} else {

		/* Update (overwrite) existing entry. */

		MapSource * old_map_source = map_source_interfaces[map_type_id];
		map_source_interfaces[map_type_id] = map_source;
		if (old_map_source) {
			delete old_map_source;
		} else {
			qDebug() << SG_PREFIX_E << "Old map source was NULL";
		}

		/* TODO: the concept of map types, and of updating it,
		   could be implemented better. */
		for (int i = 0; i < (int) map_types.size(); i++) {
			if (map_types[i].id == (int) map_type_id) {
				map_types[i].label = label;
				break;
			}
		}

		/* TODO: verify in application that properties dialog sees updated entry. */
	}
}




/**
   Returns map type id
*/
MapTypeID LayerMap::get_map_type_id(void) const
{
	return this->map_type_id;
}




bool LayerMap::set_map_type_id(MapTypeID new_map_type_id)
{
	if (!MapSource::is_map_type_id_registered(new_map_type_id)) {
		qDebug() << SG_PREFIX_E << "Unknown map type" << (int) new_map_type_id;
		return false;
	}

	this->map_type_id = new_map_type_id;
	return true;
}




MapTypeID LayerMap::get_default_map_type_id(void)
{
	/* TODO: verify that this function call works as expected. */
	SGVariant var = LayerDefaults::get(LayerType::Map, "mode", SGVariantType::Int); /* kamilTODO: get the default value from LayerInterface. */
	if (var.u.val_int == 0) {
		var = id_default();
	}
	return (MapTypeID) var.u.val_int;
}




QString LayerMap::get_map_label(void) const
{
	if (this->map_type_id == MapTypeID::Initial) {
		return QObject::tr("Map Layer");
	} else {
		return map_source_interfaces[this->map_type_id]->get_label();
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
		SGVariant var = Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL ".maplayer_default_dir");
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
	return map_source_interfaces.end() != map_source_interfaces.find(map_type_id);
}




#define VIK_SETTINGS_MAP_LICENSE_SHOWN "map_license_shown"

/**
   Convenience function to display the license.
*/
static void maps_show_license(Window * parent, const MapSource * map_source)
{
	Dialog::map_license(map_source->get_label(), map_source->get_license(), map_source->get_license_url(), parent);
}




bool LayerMap::set_param_value(param_id_t param_id, const SGVariant & data, bool is_file_operation)
{
	switch (param_id) {
	case PARAM_CACHE_DIR:
		this->set_cache_dir(data.val_string);
		break;
	case PARAM_CACHE_LAYOUT:
		if ((MapCacheLayout) data.u.val_int < MapCacheLayout::Num) {
			this->cache_layout = (MapCacheLayout) data.u.val_int;
		}
		break;
	case PARAM_FILE:
		this->set_file_full_path(data.val_string);
		break;
	case PARAM_MAP_TYPE_ID:
		if (!MapSource::is_map_type_id_registered((MapTypeID) data.u.val_int)) {
			qDebug() << SG_PREFIX_E << "Unknown map type" << data.u.val_int;
		} else {
			this->map_type_id = (MapTypeID) data.u.val_int;

			/* When loading from a file don't need the license reminder - ensure it's saved into the 'seen' list. */
			if (is_file_operation) {
				ApplicationState::set_integer_list_containing(VIK_SETTINGS_MAP_LICENSE_SHOWN, (int) this->map_type_id);
			} else {
				/* Call to MapSource::is_map_type_id_registered()
				   above guarantees that this map
				   lookup is successful. */
				const MapSource * map_source = map_source_interfaces[this->map_type_id];
				if (map_source->get_license() != NULL) {
					/* Check if licence for this map type has been shown before. */
					if (!ApplicationState::get_integer_list_contains(VIK_SETTINGS_MAP_LICENSE_SHOWN, (int) this->map_type_id)) {
						maps_show_license(this->get_window(), map_source);
						ApplicationState::set_integer_list_containing(VIK_SETTINGS_MAP_LICENSE_SHOWN, (int) this->map_type_id);
					}
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
		if (data.u.val_int < (int) params_mapzooms.size()) {
			this->mapzoom_id = data.u.val_int;
			this->xmapzoom = __mapzooms_x[data.u.val_int];
			this->ymapzoom = __mapzooms_y[data.u.val_int];
		} else {
			qDebug() << SG_PREFIX_W << "Unknown Map Zoom" << data.u.val_int;
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
		rv = SGVariant((int32_t) this->cache_layout);
		break;
	case PARAM_FILE:
		rv = SGVariant(this->file_full_path);
		break;
	case PARAM_MAP_TYPE_ID:
		rv = SGVariant((int32_t) this->map_type_id);
		break;
	case PARAM_ALPHA:
		rv = SGVariant((int32_t) this->alpha);
		break;
	case PARAM_AUTO_DOWNLOAD:
		rv = SGVariant(this->autodownload); /* kamilkamil: in viking code there is a type mismatch. */
		break;
	case PARAM_ONLY_MISSING:
		rv = SGVariant(this->adl_only_missing); /* kamilkamil: in viking code there is a type mismatch. */
		break;
	case PARAM_MAP_ZOOM:
		rv = SGVariant((int32_t) this->mapzoom_id);
		break;
	default: break;
	}
	return rv;
}



#ifdef K_TODO
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



/****************************************/
/****** CREATING, COPYING, FREEING ******/
/****************************************/
LayerMap::~LayerMap()
{
	if (this->dl_right_click_menu) {
		delete this->dl_right_click_menu;
	}

	free(this->last_center);
	this->last_center = NULL;


	MapSourceArgs args;
	args.sqlite_handle = &this->sqlite_handle;

	map_source_interfaces[this->map_type_id]->close_map_source(args);
}




void LayerMap::post_read(Viewport * viewport, bool from_file)
{
	MapSource * map_source = map_source_interfaces[this->map_type_id];

	if (!from_file) {
		/* If this method is not called in file reading context it is called in GUI context.
		   So, we can check if we have to inform the user about inconsistency. */
		if (map_source->get_drawmode() != viewport->get_drawmode()) {
			const QString drawmode_name = ViewportDrawModes::get_name(map_source->get_drawmode());
			const QString msg = QString(QObject::tr("New map cannot be displayed in the current drawmode.\nSelect \"%1\" from View menu to view it.")).arg(drawmode_name);
			Dialog::warning(msg, this->get_window());
		}
	}

	/* Performed in post read as we now know the map type. */

	MapSourceArgs args;
	args.tile_file_full_path = this->file_full_path;
	args.sqlite_handle = &this->sqlite_handle;
	args.parent_window = this->get_window();

	map_source->post_read(args);

	/* If the on Disk OSM Tile Layout type. */
	if (map_source->map_type_id == MapTypeID::OSMOnDisk) {
		/* Copy the directory into filename.
		   Thus the map cache look up will be unique when using more than one of these map types. */
		this->file_full_path = this->cache_dir;
	}
}




QString LayerMap::get_tooltip(void) const
{
	return this->get_map_label();
}




Layer * LayerMapInterface::unmarshall(Pickle & pickle, Viewport * viewport)
{
	LayerMap * layer = new LayerMap();

	layer->unmarshall_params(pickle);
	layer->post_read(viewport, false);
	return layer;
}




/*********************/
/****** DRAWING ******/
/*********************/




static void pixmap_apply_settings(QPixmap & pixmap, int alpha, double scale_x, double scale_y)
{
	/* Apply alpha setting. */
	if (scale_alpha.is_in_range(alpha)) {
		ui_pixmap_set_alpha(pixmap, alpha);
	}

	if (scale_x != 1.0 || scale_y != 1.0) {
		ui_pixmap_scale_size_by(pixmap, scale_x, scale_y);
	}

	return;
}




QPixmap LayerMap::get_tile_pixmap(const QString & map_type_string, TileInfo & tile_info, double scale_x, double scale_y)
{
	/* Get the thing. */
	QPixmap pixmap = MapCache::get_tile_pixmap(tile_info, this->map_type_id, this->alpha, scale_x, scale_y, this->file_full_path);
	if (!pixmap.isNull()) {
		qDebug() << SG_PREFIX_I << "CACHE HIT";
		return pixmap;
	}

	/* Not an error, simply the pixmap was not in a cache. Let's generate the pixmap. */
	qDebug() << SG_PREFIX_I << "CACHE MISS";

	MapSource * map_source = map_source_interfaces[this->map_type_id]; /* TODO: this variable should be const. */

	MapSourceArgs args;
	args.sqlite_handle = &this->sqlite_handle;
	args.map_type_string = map_type_string;
	const MapCacheObj map_cache_obj(this->cache_layout, this->cache_dir);
	pixmap = map_source->get_tile_pixmap(map_cache_obj, tile_info, args);

	if (!pixmap.isNull()) {
		pixmap_apply_settings(pixmap, this->alpha, scale_x, scale_y);

		MapCache::add_tile_pixmap(pixmap, MapCacheItemProperties(0.0), tile_info, map_source->map_type_id,
					  this->alpha, scale_x, scale_y, this->file_full_path);
	}

	return pixmap;
}




bool LayerMap::should_start_autodownload(Viewport * viewport)
{
	const Coord center = viewport->get_center2();

	if (this->get_window()->get_pan_move()) {
		/* D'n'D pan in action: do not download. */
		return false;
	}

	/* Don't attempt to download unsupported zoom levels. */
	double xzoom = viewport->get_map_zoom().get_x();
	const MapSource * map_source = map_source_interfaces[this->map_type_id];
	int zl = map_utils_mpp_to_zoom_level(xzoom);
	if (zl < map_source->get_zoom_min() || zl > map_source->get_zoom_max()) {
		return false;
	}

	if (this->last_center == NULL) {
		Coord * new_center = new Coord();
		*new_center = center;
		this->last_center = new_center;
		this->last_map_zoom = viewport->get_map_zoom();
		return true;
	}


	if ((*this->last_center == center) /* TODO: perhaps Coord::distance()? */
	    && this->last_map_zoom == viewport->get_map_zoom()) {

		return false;
	}

	*this->last_center = center;
	this->last_map_zoom = viewport->get_map_zoom();
	return true;
}




bool LayerMap::try_draw_scale_down(Viewport * viewport, TileInfo ulm,
				   int viewport_x, int viewport_y,
				   int tilesize_x_ceil, int tilesize_y_ceil,
				   double scale_x, double scale_y,
				   const QString & map_type_string)
{
	for (unsigned int scale_inc = 1; scale_inc < SCALE_INC_DOWN; scale_inc++) {
		/* Try with smaller zooms. */
		int scale_factor = 1 << scale_inc;  /* 2^scale_inc */
		TileInfo ulm2 = ulm;
		ulm2.x = ulm.x / scale_factor;
		ulm2.y = ulm.y / scale_factor;
		ulm2.scale = ulm.scale + scale_inc;

		const QPixmap pixmap = this->get_tile_pixmap(map_type_string, ulm2, scale_x * scale_factor, scale_y * scale_factor);
		if (!pixmap.isNull()) {
			qDebug() << SG_PREFIX_I << "Pixmap found";
			const int pixmap_x = (ulm.x % scale_factor) * tilesize_x_ceil;
			const int pixmap_y = (ulm.y % scale_factor) * tilesize_y_ceil;
			qDebug() << SG_PREFIX_I << "Calling draw_pixmap()";
			viewport->draw_pixmap(pixmap, viewport_x, viewport_y, pixmap_x, pixmap_y, tilesize_x_ceil, tilesize_y_ceil);
			return true;
		} else {
			qDebug() << SG_PREFIX_I << "Pixmap not found";
		}
	}
	return false;
}




bool LayerMap::try_draw_scale_up(Viewport * viewport, TileInfo ulm,
				 int viewport_x, int viewport_y,
				 int tilesize_x_ceil, int tilesize_y_ceil,
				 double scale_x, double scale_y,
				 const QString & map_type_string)
{
	/* Try with bigger zooms. */
	for (unsigned int scale_dec = 1; scale_dec < SCALE_INC_UP; scale_dec++) {
		int scale_factor = 1 << scale_dec;  /* 2^scale_dec */
		TileInfo ulm2 = ulm;
		ulm2.x = ulm.x * scale_factor;
		ulm2.y = ulm.y * scale_factor;
		ulm2.scale = ulm.scale - scale_dec;
		for (int pict_x = 0; pict_x < scale_factor; pict_x ++) {
			for (int pict_y = 0; pict_y < scale_factor; pict_y ++) {
				TileInfo ulm3 = ulm2;
				ulm3.x += pict_x;
				ulm3.y += pict_y;

				const QPixmap pixmap = this->get_tile_pixmap(map_type_string, ulm3, scale_x / scale_factor, scale_y / scale_factor);
				if (!pixmap.isNull()) {
					qDebug() << SG_PREFIX_I << "Pixmap found";
					int pixmap_x = 0;
					int pixmap_y = 0;
					int dest_x = viewport_x + pict_x * (tilesize_x_ceil / scale_factor);
					int dest_y = viewport_y + pict_y * (tilesize_y_ceil / scale_factor);
					qDebug() << SG_PREFIX_I << "Calling draw_pixmap";
					viewport->draw_pixmap(pixmap, dest_x, dest_y, pixmap_x, pixmap_y, tilesize_x_ceil / scale_factor, tilesize_y_ceil / scale_factor);
					return true;
				} else {
					qDebug() << SG_PREFIX_I << "Pixmap not found";
				}
			}
		}
	}
	return false;
}




void LayerMap::draw_section(Viewport * viewport, const Coord & coord_ul, const Coord & coord_br)
{
	double xzoom = viewport->get_map_zoom().get_x();
	double yzoom = viewport->get_map_zoom().get_y();

	double scale_x = 1.0;
	double scale_y = 1.0;
	bool existence_only = false;

	if (this->xmapzoom && (this->xmapzoom != xzoom || this->ymapzoom != yzoom)) {
		scale_x = this->xmapzoom / xzoom;
		scale_y = this->ymapzoom / yzoom;
		xzoom = this->xmapzoom;
		yzoom = this->xmapzoom;
		if (! (scale_x > MIN_SHRINKFACTOR && scale_x < MAX_SHRINKFACTOR &&
			scale_y > MIN_SHRINKFACTOR && scale_y < MAX_SHRINKFACTOR)) {

			if (scale_x > REAL_MIN_SHRINKFACTOR && scale_y > REAL_MIN_SHRINKFACTOR) {
				qDebug() << SG_PREFIX_D << "existence_only due to SHRINKFACTORS";
				existence_only = true;
			} else {
				/* Report the reason for not drawing. */
				Window * window = this->get_window();
				if (window) {
					QString msg = QString("Refusing to draw tiles or existence of tiles beyond %1 zoom out factor").arg((int)(1.0/REAL_MIN_SHRINKFACTOR));
					window->statusbar_update(StatusBarField::INFO, msg);
				}
				return;
			}
		}
	}

	/* coord -> ID */
	TileInfo ulm, brm;
	const MapSource * map_source = map_source_interfaces[this->map_type_id];
	if (!map_source->coord_to_tile(coord_ul, xzoom, yzoom, ulm)
	    || !map_source->coord_to_tile(coord_br, xzoom, yzoom, brm)) {

		return;
	}


	/* Loop & draw. */
	const TilesRange primary_range = TileInfo::get_tiles_range(ulm, brm);
	const QString map_type_string = map_source->get_map_type_string();

	Coord coord;

	/* Prevent the program grinding to a halt if trying to deal with thousands of tiles
	   which can happen when using a small fixed zoom level and viewing large areas.
	   Also prevents very large number of tile download requests. */
	int tiles = (primary_range.x_end - primary_range.x_begin) * (primary_range.y_end - primary_range.y_begin);
	if (tiles > MAX_TILES) {
		qDebug() << SG_PREFIX_D << "existence_only due to wanting too many tiles:" << tiles;
		existence_only = true;
	}

	if ((!existence_only) && this->autodownload  && this->should_start_autodownload(viewport)) {
		qDebug() << SG_PREFIX_D << "Starting autodownload";
		if (!this->adl_only_missing && map_source->supports_download_only_new()) {
			/* Try to download newer tiles. */
			this->start_download_thread(viewport, coord_ul, coord_br, MapDownloadMode::New);
		} else {
			/* Download only missing tiles. */
			this->start_download_thread(viewport, coord_ul, coord_br, MapDownloadMode::MissingOnly);
		}
	}

	if (map_source->get_tilesize_x() == 0 && !existence_only) {

		/* The purpose of this assignment is to set fields in tile_iter
		   other than x and y.  x and y will be set and incremented in
		   loops below, but other fields of the iter also need to have
		   some valid values. These valid values are set here. */
		TileInfo tile_iter = ulm;

		for (tile_iter.x = primary_range.x_begin; tile_iter.x <= primary_range.x_end; tile_iter.x++) {
			for (tile_iter.y = primary_range.y_begin; tile_iter.y <= primary_range.y_end; tile_iter.y++) {

				const QPixmap pixmap = this->get_tile_pixmap(map_type_string, tile_iter, scale_x, scale_y);
				if (pixmap.isNull()) {
					qDebug() << SG_PREFIX_W << "Pixmap not found";
					continue;
				}

				qDebug() << SG_PREFIX_I << "Pixmap found";
				const int width = pixmap.width();
				const int height = pixmap.height();
				int viewport_x;
				int viewport_y;

				map_source->tile_to_center_coord(tile_iter, coord);
				viewport->coord_to_screen_pos(coord, &viewport_x, &viewport_y);
				viewport_x -= (width/2);
				viewport_y -= (height/2);

				qDebug() << SG_PREFIX_I << "Calling draw_pixmap()";
				viewport->draw_pixmap(pixmap, viewport_x, viewport_y, 0, 0, width, height);
			}
		}
	} else { /* tilesize is known, don't have to keep converting coords. */
		const double tilesize_x = map_source->get_tilesize_x() * scale_x;
		const double tilesize_y = map_source->get_tilesize_y() * scale_y;
		/* ceiled so tiles will be maximum size in the case of funky shrinkfactor. */
		const int tilesize_x_ceil = ceil(tilesize_x);
		const int tilesize_y_ceil = ceil(tilesize_y);

		const int delta_x = (ulm.x == primary_range.x_begin) ? 1 : -1;
		const int delta_y = (ulm.y == primary_range.y_begin) ? 1 : -1;


		TilesRange secondary_range;
		secondary_range.x_begin = (delta_x == 1) ? primary_range.x_begin : primary_range.x_end;
		secondary_range.y_begin = (delta_y == 1) ? primary_range.y_begin : primary_range.y_end;
		secondary_range.x_end   = (delta_x == 1) ? (primary_range.x_end + 1) : (primary_range.x_begin - 1);
		secondary_range.y_end   = (delta_y == 1) ? (primary_range.y_end + 1) : (primary_range.y_begin - 1);

		int viewport_x;
		int viewport_y;
		map_source->tile_to_center_coord(ulm, coord);
		viewport->coord_to_screen_pos(coord, &viewport_x, &viewport_y);

		const int viewport_x_grid = viewport_x;
		const int viewport_y_grid = viewport_y;

		/* Above trick so viewport_x,viewport_y doubles. this is so shrinkfactors aren't rounded off
		   e.g. if tile size 128, shrinkfactor 0.333. */
		viewport_x -= (tilesize_x/2);
		int base_viewport_y = viewport_y - (tilesize_y/2);

		const MapCacheObj map_cache_obj(map_source->is_direct_file_access() ? MapCacheLayout::OSM : this->cache_layout, this->cache_dir);

		/* The purpose of this assignment is to set fields in tile_iter
		   other than x and y.  x and y will be set and incremented in
		   loops below, but other fields of the iter also need to have
		   some valid values. These valid values are set here. */
		TileInfo tile_iter = ulm;

		for (tile_iter.x = secondary_range.x_begin; tile_iter.x != secondary_range.x_end; tile_iter.x += delta_x) {
			viewport_y = base_viewport_y;
			for (tile_iter.y = secondary_range.y_begin; tile_iter.y != secondary_range.y_end; tile_iter.y += delta_y) {

				if (existence_only) {
					const QString path_buf = map_cache_obj.get_cache_file_full_path(tile_iter,
													map_source->map_type_id,
													map_source->get_map_type_string(),
													map_source->get_file_extension());

					if (0 == access(path_buf.toUtf8().constData(), F_OK)) {
						const QPen pen(QColor(LAYER_MAP_GRID_COLOR));
						viewport->draw_line(pen, viewport_x + tilesize_x_ceil, viewport_y, viewport_x, viewport_y + tilesize_y_ceil);
					}
				} else {
					/* Try correct scale first. */
					int scale_factor = 1;
					const QPixmap pixmap = this->get_tile_pixmap(map_type_string, tile_iter, scale_x * scale_factor, scale_y * scale_factor);
					if (!pixmap.isNull()) {
						qDebug() << SG_PREFIX_I << "Pixmap found";
						const int pixmap_x = (tile_iter.x % scale_factor) * tilesize_x_ceil;
						const int pixmap_y = (tile_iter.y % scale_factor) * tilesize_y_ceil;
						qDebug() << SG_PREFIX_I << "Calling draw_pixmap, pixmap_x =" << pixmap_x << "pixmap_y =" << pixmap_y << "viewport_x =" << viewport_x << "viewport_y =" << viewport_y;
						viewport->draw_pixmap(pixmap, viewport_x, viewport_y, pixmap_x, pixmap_y, tilesize_x_ceil, tilesize_y_ceil);
					} else {
						qDebug() << SG_PREFIX_I << "Pixmap not found";
						/* Otherwise try different scales. */
						if (SCALE_SMALLER_ZOOM_FIRST) {
							if (!this->try_draw_scale_down(viewport, tile_iter, viewport_x, viewport_y, tilesize_x_ceil, tilesize_y_ceil, scale_x, scale_y, map_type_string)) {
								this->try_draw_scale_up(viewport, tile_iter, viewport_x, viewport_y, tilesize_x_ceil, tilesize_y_ceil, scale_x, scale_y, map_type_string);
							}
						} else {
							if (!this->try_draw_scale_up(viewport, tile_iter, viewport_x, viewport_y, tilesize_x_ceil, tilesize_y_ceil, scale_x, scale_y, map_type_string)) {
								this->try_draw_scale_down(viewport, tile_iter, viewport_x, viewport_y, tilesize_x_ceil, tilesize_y_ceil, scale_x, scale_y, map_type_string);
							}
						}
					}
				}

				viewport_y += tilesize_y;
			}
			viewport_x += tilesize_x;
		}

		/* ATM Only show tile grid lines in extreme debug mode. */
		if (
#if 1
		    true
#else
		    vik_debug && vik_verbose
#endif
		    ) {
			/* Grid drawing here so it gets drawn on top of the map.
			   Thus loop around x & y again, but this time separately.
			   Only showing grid for the current scale */
			draw_grid(viewport, viewport_x_grid, viewport_y_grid, secondary_range.x_begin, delta_x, secondary_range.x_end, secondary_range.y_begin, delta_y, secondary_range.y_end, tilesize_x, tilesize_y);
		}
	}
}




void draw_grid(Viewport * viewport, int viewport_x, int viewport_y, int x_begin, int delta_x, int x_end, int y_begin, int delta_y, int y_end, int tilesize_x, int tilesize_y)
{
	const QPen pen(QColor(LAYER_MAP_GRID_COLOR));

	/* Draw single grid lines across the whole screen. */
	const int width = viewport->get_width();
	const int height = viewport->get_height();
	const int base_viewport_x = viewport_x - (tilesize_x / 2);
	const int base_viewport_y = viewport_y - (tilesize_y / 2);

	viewport_x = base_viewport_x;
	for (int x = x_begin; x != x_end; x += delta_x) {
		viewport->draw_line(pen, viewport_x, base_viewport_y, viewport_x, height);
		viewport_x += tilesize_x;
	}

	viewport_y = base_viewport_y;
	for (int y = y_begin; y != y_end; y += delta_y) {
		viewport->draw_line(pen, base_viewport_x, viewport_y, width, viewport_y);
		viewport_y += tilesize_y;
	}
}




void LayerMap::draw_tree_item(Viewport * viewport, bool highlight_selected, bool parent_is_selected)
{
	MapSource * map_source = map_source_interfaces[this->map_type_id];

	if (map_source->get_drawmode() != viewport->get_drawmode()) {
		return;
	}

	/* Copyright. */
	double level = viewport->get_zoom();
	const LatLonBBox bbox = viewport->get_bbox();
	map_source->add_copyright(viewport, bbox, level);

	viewport->add_logo(map_source->get_logo());

	/* Get corner coords. */
	if (viewport->get_coord_mode() == CoordMode::UTM && ! viewport->get_is_one_utm_zone()) {
		/* UTM multi-zone stuff by Kit Transue. */
		const int leftmost_zone = viewport->get_leftmost_zone();
		const int rightmost_zone = viewport->get_rightmost_zone();
		Coord coord_ul;
		Coord coord_br;
		for (int zone = leftmost_zone; zone <= rightmost_zone; ++zone) {
			viewport->get_corners_for_zone(coord_ul, coord_br, zone);
			this->draw_section(viewport, coord_ul, coord_br);
		}
	} else {
		const Coord coord_ul = viewport->screen_pos_to_coord(0, 0);
		const Coord coord_br = viewport->screen_pos_to_coord(viewport->get_width(), viewport->get_height());

		this->draw_section(viewport, coord_ul, coord_br);
	}
}




/*************************/
/****** DOWNLOADING ******/
/*************************/




void LayerMap::weak_ref_cb(void * ptr, void * dead_vml)
{
	MapDownloadJob * mdj = (MapDownloadJob *) ptr;
	mdj->mutex.lock();
	mdj->map_layer_alive = false;
	mdj->mutex.unlock();
}




void LayerMap::start_download_thread(Viewport * viewport, const Coord & coord_ul, const Coord & coord_br, MapDownloadMode map_download_mode)
{
	double xzoom = this->xmapzoom ? this->xmapzoom : viewport->get_map_zoom().get_x();
	double yzoom = this->ymapzoom ? this->ymapzoom : viewport->get_map_zoom().get_y();
	TileInfo ulm, brm;
	const MapSource * map_source = map_source_interfaces[this->map_type_id];

	qDebug() << SG_PREFIX_I << "Map:" << (quintptr) map_source << "map index" << (int) this->map_type_id;

	/* Don't ever attempt download on direct access. */
	if (map_source->is_direct_file_access()) {
		qDebug() << SG_PREFIX_I << "Not downloading, direct access";
		return;
	}

	if (!map_source->coord_to_tile(coord_ul, xzoom, yzoom, ulm)
	     || !map_source->coord_to_tile(coord_br, xzoom, yzoom, brm)) {

		qDebug() << SG_PREFIX_E << "Conversion coord to tile failed";
		return;
	}


	MapDownloadJob * mdj = new MapDownloadJob(this, map_source, ulm, brm, true, map_download_mode);

	if (mdj->map_download_mode == MapDownloadMode::MissingOnly) {
		mdj->n_items = mdj->calculate_tile_count_to_download();
	} else {
		mdj->n_items = mdj->calculate_total_tile_count_to_download();
	}

	if (mdj->n_items) {
		mdj->layer->weak_ref(LayerMap::weak_ref_cb, mdj);
		mdj->set_description(map_download_mode, mdj->n_items, map_source->get_label());
		mdj->run_in_background(ThreadPoolType::Remote);
	} else {
		delete mdj;
	}
}




void LayerMap::download_section_sub(const Coord & coord_ul, const Coord & coord_br, double zoom, MapDownloadMode map_download_mode)
{
	TileInfo ulm, brm;
	const MapSource * map_source = map_source_interfaces[this->map_type_id];

	/* Don't ever attempt download on direct access. */
	if (map_source->is_direct_file_access()) {
		return;
	}

	if (!map_source->coord_to_tile(coord_ul, zoom, zoom, ulm)
	    || !map_source->coord_to_tile(coord_br, zoom, zoom, brm)) {
		qDebug() << SG_PREFIX_W << "coord_to_tile() failed";
		return;
	}

	MapDownloadJob * mdj = new MapDownloadJob(this, map_source, ulm, brm, true, map_download_mode);

	mdj->n_items = mdj->calculate_tile_count_to_download();
	if (mdj->n_items) {
		mdj->layer->weak_ref(weak_ref_cb, mdj);
		mdj->set_description(map_download_mode, mdj->n_items, map_source->get_label());
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
void LayerMap::download_section(const Coord & coord_ul, const Coord & coord_br, double zoom)
{
	this->download_section_sub(coord_ul, coord_br, zoom, MapDownloadMode::MissingOnly);
}




void LayerMap::redownload_bad_cb(void)
{
	this->start_download_thread(this->redownload_viewport, this->redownload_ul, this->redownload_br, MapDownloadMode::MissingAndBad);
}




void LayerMap::redownload_all_cb(void)
{
	this->start_download_thread(this->redownload_viewport, this->redownload_ul, this->redownload_br, MapDownloadMode::All);
}




void LayerMap::redownload_new_cb(void)
{
	this->start_download_thread(this->redownload_viewport, this->redownload_ul, this->redownload_br, MapDownloadMode::New);
}




/**
 * Display a simple dialog with information about this particular map tile
 */
void LayerMap::tile_info_cb(void)
{
	const MapSource * map_source = map_source_interfaces[this->map_type_id];

	double xzoom = this->xmapzoom ? this->xmapzoom : this->redownload_viewport->get_map_zoom().get_x();
	double yzoom = this->ymapzoom ? this->ymapzoom : this->redownload_viewport->get_map_zoom().get_y();
	TileInfo ulm;

	if (!map_source->coord_to_tile(this->redownload_ul, xzoom, yzoom, ulm)) {
		return;
	}

	MapSourceArgs args;
	args.sqlite_handle = &this->sqlite_handle;
	args.tile_file_full_path = this->file_full_path;

	const MapCacheObj map_cache_obj(this->cache_layout, this->cache_dir);

	const QStringList tile_info_strings = map_source->get_tile_description(map_cache_obj, ulm, args);

	a_dialog_list(tr("Tile Information"), tile_info_strings, 5, this->get_window());
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
			char time_buf[64];
			strftime(time_buf, sizeof (time_buf), "%c", gmtime((const time_t *) &stat_buf.st_mtime));
			timestamp_info = QObject::tr("Tile File Timestamp: %1").arg(time_buf);
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




ToolStatus LayerToolMapsDownload::handle_mouse_release(Layer * _layer, QMouseEvent * event)
{
	if (!_layer || _layer->type != LayerType::Map) {
		return ToolStatus::Ignored;
	}

	LayerMap * layer = (LayerMap *) _layer;

	if (layer->dl_tool_x != -1 && layer->dl_tool_y != -1) {
		if (event->button() == Qt::LeftButton) {
			const Coord coord_ul = this->viewport->screen_pos_to_coord(std::max(0, std::min(event->x(), layer->dl_tool_x)), std::max(0, std::min(event->y(), layer->dl_tool_y)));
			const Coord coord_br = this->viewport->screen_pos_to_coord(std::min(this->viewport->get_width(), std::max(event->x(), layer->dl_tool_x)), std::min(this->viewport->get_height(), std::max(event->y(), layer->dl_tool_y)));
			layer->start_download_thread(this->viewport, coord_ul, coord_br, MapDownloadMode::DownloadAndRefresh);
			layer->dl_tool_x = layer->dl_tool_y = -1;
			return ToolStatus::Ack;
		} else {
			layer->redownload_ul = this->viewport->screen_pos_to_coord(std::max(0, std::min(event->x(), layer->dl_tool_x)), std::max(0, std::min(event->y(), layer->dl_tool_y)));
			layer->redownload_br = this->viewport->screen_pos_to_coord(std::min(this->viewport->get_width(), std::max(event->x(), layer->dl_tool_x)), std::min(this->viewport->get_height(), std::max(event->y(), layer->dl_tool_y)));

			layer->redownload_viewport = this->viewport;

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
		}
	}
	return ToolStatus::Ignored;
}




LayerToolMapsDownload::LayerToolMapsDownload(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::Map)
{
	this->id_string = "sg.tool.layer_map.maps_download";

	this->action_icon_path   = "vik-icon-Maps Download";
	this->action_label       = QObject::tr("Maps Download");
	this->action_tooltip     = QObject::tr("Maps Download");
	// this->action_accelerator = ...; /* Empty accelerator. */

	this->cursor_click = new QCursor(QPixmap(":cursors/maps_download.png"), 0, 0);
	this->cursor_release = new QCursor(QPixmap(":cursors/maps_download.png"), 0, 0);
#ifdef K_FIXME_RESTORE
	this->cursor_shape = Qt::BitmapCursor;
	this->cursor_data = &cursor_mapdl_pixmap;
#endif
}




ToolStatus LayerToolMapsDownload::handle_mouse_click(Layer * _layer, QMouseEvent * event)
{
	TileInfo tmp;
	if (!_layer || _layer->type != LayerType::Map) {
		return ToolStatus::Ignored;
	}

	LayerMap * layer = (LayerMap *) _layer;

	const MapSource * map_source = map_source_interfaces[layer->map_type_id];
	if (map_source->get_drawmode() == this->viewport->get_drawmode()
	    && map_source->coord_to_tile(this->viewport->get_center2(),
				  layer->xmapzoom ? layer->xmapzoom : this->viewport->get_map_zoom().get_x(),
				  layer->ymapzoom ? layer->ymapzoom : this->viewport->get_map_zoom().get_y(),
				  tmp)) {

		layer->dl_tool_x = event->x();
		layer->dl_tool_y = event->y();
		return ToolStatus::Ack;
	}
	return ToolStatus::Ignored;
}




void LayerMap::download_onscreen_maps(MapDownloadMode map_download_mode)
{
	Viewport * viewport = this->get_window()->get_viewport();

	double xzoom = this->xmapzoom ? this->xmapzoom : viewport->get_map_zoom().get_x();
	double yzoom = this->ymapzoom ? this->ymapzoom : viewport->get_map_zoom().get_y();

	TileInfo ulm, brm;

	const Coord coord_ul = viewport->screen_pos_to_coord(0, 0);
	const Coord coord_br = viewport->screen_pos_to_coord(viewport->get_width(), viewport->get_height());

	const MapSource * map_source = map_source_interfaces[this->map_type_id];

	const ViewportDrawMode map_draw_mode = map_source->get_drawmode();
	const ViewportDrawMode vp_draw_mode = viewport->get_drawmode();

	if (map_draw_mode == vp_draw_mode
	    && map_source->coord_to_tile(coord_ul, xzoom, yzoom, ulm)
	    && map_source->coord_to_tile(coord_br, xzoom, yzoom, brm)) {

		this->start_download_thread(viewport, coord_ul, coord_br, map_download_mode);

	} else if (map_draw_mode != vp_draw_mode) {
		const QString drawmode_name = ViewportDrawModes::get_name(map_draw_mode);
		const QString err = QString(QObject::tr("Wrong drawmode for this map.\nSelect \"%1\" from View menu and try again.")).arg(drawmode_name);
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
	const MapSource * map_source = map_source_interfaces[this->map_type_id];
	if (map_source->get_license().isEmpty()) {
		Dialog::info(map_source->get_label(), this->get_window());
	} else {
		maps_show_license(this->get_window(), map_source);
	}
}




/**
 * Copied from maps_layer_download_section but without the actual download and this returns a value
 */
int LayerMap::how_many_maps(const Coord & coord_ul, const Coord & coord_br, double zoom, MapDownloadMode map_download_mode)
{
	const MapSource * map_source = map_source_interfaces[this->map_type_id];

	if (map_source->is_direct_file_access()) {
		return 0;
	}

	TileInfo ulm, brm;
	if (!map_source->coord_to_tile(coord_ul, zoom, zoom, ulm)
	    || !map_source->coord_to_tile(coord_br, zoom, zoom, brm)) {
		qDebug() << SG_PREFIX_W << "coord_to_tile() failed";
		return 0;
	}

	MapDownloadJob * mdj = new MapDownloadJob(this, map_source, ulm, brm, false, map_download_mode);
	int n_items = 0;

	if (mdj->map_download_mode == MapDownloadMode::All) {
		n_items = mdj->calculate_total_tile_count_to_download();
	} else {
		n_items = mdj->calculate_tile_count_to_download();
	}

	delete mdj;

	return n_items;
}




/**
 * This dialog is specific to the map layer, so it's here rather than in dialog.c
 */
bool maps_dialog_zoom_between(Window * parent,
			      const QString & title,
			      const QStringList & zoom_list,
			      const QStringList & download_list,
			      int default_zoom1,
			      int default_zoom2,
			      MapDownloadMode default_download_mode,
			      int * selected_zoom1,
			      int * selected_zoom2,
			      MapDownloadMode * selected_download_mode)
{

	BasicDialog dialog(parent);
	dialog.setWindowTitle(title);


	int row = 0;


	dialog.grid->addWidget(new QLabel(QObject::tr("Zoom Start:")), row, 0);
	QComboBox zoom_combo1;
	for (int i = 0; i < zoom_list.size(); i++) {
		zoom_combo1.addItem(zoom_list.at(i), i);
	}
	zoom_combo1.setCurrentIndex(default_zoom1);
	dialog.grid->addWidget(&zoom_combo1, row, 1);
	row++;


	dialog.grid->addWidget(new QLabel(QObject::tr("Zoom End:")), row, 0);
	QComboBox zoom_combo2;
	for (int i = 0; i < zoom_list.size(); i++) {
		zoom_combo2.addItem(zoom_list.at(i), i);
	}
	zoom_combo2.setCurrentIndex(default_zoom2);
	dialog.grid->addWidget(&zoom_combo2, row, 1);
	row++;


	dialog.grid->addWidget(new QLabel(QObject::tr("Download Maps Method:")), row, 0);
	QComboBox download_combo;
	for (int i = 0; i < download_list.size(); i++) {
		download_combo.addItem(download_list.at(i), i);
	}
	download_combo.setCurrentIndex((int) default_download_mode);
	dialog.grid->addWidget(&download_combo, row, 1);


	if (dialog.exec() != QDialog::Accepted) {
		return false;
	}


	/* Return selected options. */
	*selected_zoom1 = zoom_combo1.currentIndex();
	*selected_zoom2 = zoom_combo2.currentIndex();
	*selected_download_mode = (MapDownloadMode) download_combo.currentIndex();


	return true;
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
	Viewport * viewport = this->get_window()->get_viewport();

	/* I don't think we should allow users to hammer the servers too much...
	   Deliberately not allowing lowest zoom levels.
	   Still can give massive numbers to download.
	   A screen size of 1600x1200 gives around 300,000 tiles between 1..128 when none exist before!! */

	double zoom_vals[] = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024 };
	int n_zoom_vals = (int) (sizeof (zoom_vals) / sizeof (zoom_vals[0]));
	QStringList zoom_list;
	for (int i = 0; i < n_zoom_vals; i++) {
		char buffer[6];
		snprintf(buffer, sizeof (buffer), "%d", (int) zoom_vals[i]);
		zoom_list << buffer;
	}

	/* Redownload method - needs to align with REDOWNLOAD* macro values. */
	QStringList download_list;
	download_list << QObject::tr("Only Missing") << QObject::tr("Missing and Bad") << QObject::tr("New") << QObject::tr("Re-download All");

	int selected_zoom1, selected_zoom2, default_zoom, lower_zoom;
	MapDownloadMode selected_download_mode;

	double cur_zoom = viewport->get_zoom();

	for (default_zoom = 0; default_zoom < n_zoom_vals; default_zoom++) {
		if (cur_zoom == zoom_vals[default_zoom]) {
			break;
		}
	}
	default_zoom = (default_zoom == n_zoom_vals) ? n_zoom_vals - 1 : default_zoom;

	/* Default to only 2 zoom levels below the current one/ */
	if (default_zoom > 1) {
		lower_zoom = default_zoom - 2;
	} else {
		lower_zoom = default_zoom;
	}


	const QString title = QString(QObject::tr("%1: %2")).arg(this->get_map_label()).arg(QObject::tr("Download for Zoom Levels"));
	if (!maps_dialog_zoom_between(this->get_window(),
				      title,
				      zoom_list,
				      download_list,
				      lower_zoom,
				      default_zoom,
				      MapDownloadMode::MissingOnly,
				      &selected_zoom1,
				      &selected_zoom2,
				      &selected_download_mode)) {
		/* Cancelled. */
		return;
	}

	/* Find out new current positions. */
	const LatLonMinMax min_max = viewport->get_min_max_lat_lon();
	const Coord coord_ul(LatLon(min_max.max.lat, min_max.min.lon), viewport->get_coord_mode());
	const Coord coord_br(LatLon(min_max.min.lat, min_max.max.lon), viewport->get_coord_mode());

	/* Get Maps Count - call for each zoom level (in reverse).
	   With MapDownloadMode::New this is a possible maximum.
	   With MapDownloadMode::MisingOnly this only missing ones - however still has a server lookup per tile. */
	int map_count = 0;
	for (int zz = selected_zoom2; zz >= selected_zoom1; zz--) {
		map_count = map_count + this->how_many_maps(coord_ul, coord_br, zoom_vals[zz], selected_download_mode);
	}

	qDebug() << SG_PREFIX_D << "Download request map count" << map_count << "for method" << (int) selected_download_mode;

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
	for (int zz = selected_zoom2; zz >= selected_zoom1; zz--) {
		this->download_section_sub(coord_ul, coord_br, zoom_vals[zz], selected_download_mode);
	}
}




void LayerMap::flush_cb(void)
{
	MapCache::flush_type(map_source_interfaces[this->map_type_id]->map_type_id);
}




void LayerMap::add_menu_items(QMenu & menu)
{
	QAction * qa = NULL;

	qa = new QAction(QObject::tr("Download &Missing Onscreen Maps"), this);
	qa->setIcon(QIcon::fromTheme("list-add"));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (download_missing_onscreen_maps_cb(void)));
	menu.addAction(qa);

	if (map_source_interfaces[this->map_type_id]->supports_download_only_new()) {
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
}




/**
 * Enable downloading maps of the current screen area either 'new' or 'everything'.
 */
void LayerMap::download(Viewport * viewport, bool only_new)
{
	if (!viewport) {
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

	this->type = LayerType::Map;
	strcpy(this->debug_string, "MAP");
	this->interface = &vik_map_layer_interface;

	this->set_initial_parameter_values();
	this->set_name(Layer::get_type_ui_label(this->type));
}
