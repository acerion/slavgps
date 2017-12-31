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

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include <mutex>
#include <deque>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <cmath>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <QDebug>
#include <QDir>

#include "window.h"
#include "map_source.h"
#include "map_source_slippy.h"
#include "vikutils.h"
#include "map_utils.h"
#include "map_cache.h"
#include "background.h"
#include "preferences.h"
#include "layer_map.h"
#include "metatile.h"
#include "ui_util.h"
#include "map_ids.h"
#include "layer_defaults.h"
#include "widget_file_entry.h"
#include "dialog.h"
#include "file.h"
#include "application_state.h"
#include "globals.h"
#include "ui_builder.h"
#include "util.h"




#ifdef HAVE_SQLITE3_H
#undef HAVE_SQLITE3_H
#endif

#ifdef HAVE_SQLITE3_H
#include "sqlite3.h"
#endif




using namespace SlavGPS;




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

static std::deque<MapSource *> map_sources;


/* List of labels (string) and IDs (MapTypeID) for each map type. */
static std::vector<SGLabelID> map_types;


static std::vector<SGLabelID> params_mapzooms = {
	SGLabelID(N_("Use Viking Zoom Level"), 0),
	SGLabelID("0.25", 1),
	SGLabelID("0.5", 2),
	SGLabelID("1", 3),
	SGLabelID("2", 4),
	SGLabelID("4", 5),
	SGLabelID("8", 6),
	SGLabelID("16", 7),
	SGLabelID("32", 8),
	SGLabelID("64", 9),
	SGLabelID("128", 10),
	SGLabelID("256", 11),
	SGLabelID("512", 12),
	SGLabelID("1024", 13),
	SGLabelID("USGS 10k", 14),
	SGLabelID("USGS 24k", 15),
	SGLabelID("USGS 25k", 16),
	SGLabelID("USGS 50k", 17),
	SGLabelID("USGS 100k", 18),
	SGLabelID("USGS 200k", 19),
	SGLabelID("USGS 250k", 20),
};

static double __mapzooms_x[] = { 0.0, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 256.0, 512.0, 1024.0, 1.016, 2.4384, 2.54, 5.08, 10.16, 20.32, 25.4 };
static double __mapzooms_y[] = { 0.0, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 256.0, 512.0, 1024.0, 1.016, 2.4384, 2.54, 5.08, 10.16, 20.32, 25.4 };


/**************************/



static int map_type_to_map_index(MapTypeID map_type);




static ParameterScale scale_alpha = { 0, 255, SGVariant((int32_t) 255), 3, 0 };




static SGVariant id_default(void)
{
	return SGVariant((int32_t) MAP_ID_MAPQUEST_OSM);
}




static SGVariant directory_default(void)
{
	const SGVariant * pref_value = Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL ".maplayer_default_dir");
	if (pref_value) {
		return SGVariant(pref_value->val_string); /* TODO: why we can't just return *pref_value? */
	} else {
		return SGVariant("");
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
	SGLabelID("Viking", (int) MapsCacheLayout::VIKING),
	SGLabelID("OSM",    (int) MapsCacheLayout::OSM),
};

static MapsCacheLayout cache_layout_default_value = MapsCacheLayout::VIKING;




static SGVariant cache_layout_default(void)
{
	return SGVariant((int32_t) cache_layout_default_value);
}




enum {
	PARAM_MAPTYPE = 0,
	PARAM_CACHE_DIR,
	PARAM_CACHE_LAYOUT,
	PARAM_FILE,
	PARAM_ALPHA,
	PARAM_AUTODOWNLOAD,
	PARAM_ONLYMISSING,
	PARAM_MAPZOOM,
	NUM_PARAMS
};


SGFileTypeFilter map_file_type[1] = { SGFileTypeFilter::MBTILES };

ParameterSpecification maps_layer_param_specs[] = {
	/* 'mode' is really map source type id, but can't break file format just to rename the parameter name to something better. */
	{ PARAM_MAPTYPE,       NULL, "mode",           SGVariantType::INT,     PARAMETER_GROUP_GENERIC, QObject::tr("Map Type:"),                            WidgetType::COMBOBOX,    &map_types,       id_default,           NULL, NULL },
	{ PARAM_CACHE_DIR,     NULL, "directory",      SGVariantType::STRING,  PARAMETER_GROUP_GENERIC, QObject::tr("Maps Directory:"),                      WidgetType::FOLDERENTRY, NULL,             directory_default,    NULL, NULL },
	{ PARAM_CACHE_LAYOUT,  NULL, "cache_type",     SGVariantType::INT,     PARAMETER_GROUP_GENERIC, QObject::tr("Cache Layout:"),                        WidgetType::COMBOBOX,    &cache_types,     cache_layout_default, NULL, N_("This determines the tile storage layout on disk") },
	{ PARAM_FILE,          NULL, "mapfile",        SGVariantType::STRING,  PARAMETER_GROUP_GENERIC, QObject::tr("Map File:"),                            WidgetType::FILEENTRY,   map_file_type,    file_default,         NULL, N_("An MBTiles file. Only applies when the map type method is 'MBTiles'") },
	{ PARAM_ALPHA,         NULL, "alpha",          SGVariantType::INT,     PARAMETER_GROUP_GENERIC, QObject::tr("Alpha:"),                               WidgetType::HSCALE,      &scale_alpha,     NULL,                 NULL, N_("Control the Alpha value for transparency effects") },
	{ PARAM_AUTODOWNLOAD,  NULL, "autodownload",   SGVariantType::BOOLEAN, PARAMETER_GROUP_GENERIC, QObject::tr("Autodownload maps:"),                   WidgetType::CHECKBUTTON, NULL,             sg_variant_true,      NULL, NULL },
	{ PARAM_ONLYMISSING,   NULL, "adlonlymissing", SGVariantType::BOOLEAN, PARAMETER_GROUP_GENERIC, QObject::tr("Autodownload Only Gets Missing Maps:"), WidgetType::CHECKBUTTON, NULL,             sg_variant_false,     NULL, N_("Using this option avoids attempting to update already acquired tiles. This can be useful if you want to restrict the network usage, without having to resort to manual control. Only applies when 'Autodownload Maps' is on.") },
	{ PARAM_MAPZOOM,       NULL, "mapzoom",        SGVariantType::INT,     PARAMETER_GROUP_GENERIC, QObject::tr("Zoom Level:"),                          WidgetType::COMBOBOX,    &params_mapzooms, mapzoom_default,      NULL, N_("Determines the method of displaying map tiles for the current zoom level. 'Viking Zoom Level' uses the best matching level, otherwise setting a fixed value will always use map tiles of the specified value regardless of the actual zoom level.") },

	{ NUM_PARAMS,          NULL, NULL,             SGVariantType::EMPTY,   PARAMETER_GROUP_GENERIC, QString(""),                                         WidgetType::NONE,        NULL,             NULL,                 NULL, NULL }, /* Guard. */
};




void maps_layer_set_autodownload_default(bool autodownload)
{
	/* Set appropriate function. */
	if(autodownload) {
		maps_layer_param_specs[PARAM_AUTODOWNLOAD].hardwired_default_value = sg_variant_true;
	} else {
		maps_layer_param_specs[PARAM_AUTODOWNLOAD].hardwired_default_value = sg_variant_false;
	}
}




void maps_layer_set_cache_default(MapsCacheLayout layout)
{
	/* Override default value returned by the default param function. */
	cache_layout_default_value = layout;
}




LayerMapInterface vik_map_layer_interface;




LayerMapInterface::LayerMapInterface()
{
	this->parameters_c = maps_layer_param_specs; /* Parameters. */

	this->fixed_layer_type_string = "Map"; /* Non-translatable. kamilTODO: verify this line. */

	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_M;
	// this->action_icon = ...; /* Set elsewhere. */

	this->menu_items_selection = LayerMenuItem::ALL;

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



enum { REDOWNLOAD_NONE = 0,    /* Download only missing maps. */
       REDOWNLOAD_BAD,         /* Download missing and bad maps. */
       REDOWNLOAD_NEW,         /* Download missing maps that are newer on server only. */
       REDOWNLOAD_ALL,         /* Download all maps. */
       DOWNLOAD_OR_REFRESH };  /* Download missing maps and refresh cache. */




static ParameterSpecification prefs[] = {
	{ (param_id_t) LayerType::NUM_TYPES, PREFERENCES_NAMESPACE_GENERAL, "maplayer_default_dir", SGVariantType::STRING, PARAMETER_GROUP_GENERIC, QObject::tr("Default map layer directory:"), WidgetType::FOLDERENTRY, NULL, NULL, NULL, N_("Choose a directory to store cached Map tiles for this layer") },
};




void layer_map_init(void)
{
	Preferences::register_parameter(prefs, SGVariant(maps_layer_default_dir()));

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

void _add_map_source(MapSource * map, const char * label, MapTypeID map_type)
{
	/* Add the label and ID of map source. */
	map_types.push_back(SGLabelID(label, map_type));

	/* We have to clone. */
	MapSource clone = *map; /* FIXME: to clone or not to clone? */
	/* Register the clone in the list */
	map_sources.push_back(map);

	/* TODO: verify in application that properties dialog sees updated list of map types. */
}




void _update_map_source(MapSource *map, const char *label, int index)
{
	if (index == -1) {
		qDebug() << "EE: Layer Map: Update Map Source: caller passed index == -1";
		return;
	}

	MapSource * map_source = map_sources[index];
	map_sources[index] = map;
	if (map_source) {
		// delete map_source; /* kamilFIXME: free this pointer. */
	}

	/* Change previous data. */
	map_types[index].label = QString(label);
	/* TODO: why we don't update ID of the map type? */
	/* TODO: verify in application that properties dialog sees updated entry. */
}




/**
 * @map: the new MapSource
 *
 * Register a new MapSource.
 * Override existing one (equality of id).
 */
void maps_layer_register_map_source(MapSource * map)
{
	assert (map != NULL);

	MapTypeID map_type = map->map_type;
	const char * label = map->get_label();
	assert (label);

	int previous = map_type_to_map_index(map_type);
	if (previous != -1) {
		_update_map_source(map, label, previous);
	} else {
		_add_map_source(map, label, map_type);
	}
}




#define LAYER_MAP_NTH_LABEL(n) (map_types[n].label)
#define LAYER_MAP_NTH_ID(n) ((MapTypeID) map_types[n].id)

/**
 * Returns the actual map id (rather than the internal type index value).
 */
MapTypeID LayerMap::get_map_type()
{
	return LAYER_MAP_NTH_ID(this->map_index);
}




void LayerMap::set_map_type(MapTypeID map_type)
{
	int map_index_ = map_type_to_map_index(map_type);
	if (map_index_ == -1) {
		qDebug() << QObject::tr("WARNING: Unknown map type");
	} else {
		this->map_index = map_index_;
	}
}




MapTypeID LayerMap::get_default_map_type()
{
	/* TODO: verify that this function call works as expected. */
	SGVariant var = LayerDefaults::get(LayerType::MAP, "mode", SGVariantType::INT); /* kamilTODO: get the default value from LayerInterface. */
	if (var.val_int == 0) {
		var = id_default();
	}
	return (MapTypeID) var.val_int;
}




QString LayerMap::get_map_label(void) const
{
	return LAYER_MAP_NTH_LABEL(this->map_index);
}




/****************************************/
/******** CACHE DIR STUFF ***************/
/****************************************/

#define DIRECTDIRACCESS "%s%d" G_DIR_SEPARATOR_S "%d" G_DIR_SEPARATOR_S "%d%s"
#define DIRECTDIRACCESS_WITH_NAME "%s%s" G_DIR_SEPARATOR_S "%d" G_DIR_SEPARATOR_S "%d" G_DIR_SEPARATOR_S "%d%s"
#define DIRSTRUCTURE "%st%ds%dz%d" G_DIR_SEPARATOR_S "%d" G_DIR_SEPARATOR_S "%d"

#ifdef WINDOWS
#include <io.h>
#define GLOBAL_MAPS_DIR "C:\\VIKING-MAPS\\"
#define LOCAL_MAPS_DIR "VIKING-MAPS"
#elif defined __APPLE__
#include <stdlib.h>
#define GLOBAL_MAPS_DIR "/Library/cache/Viking/maps/"
#define LOCAL_MAPS_DIR "/Library/Application Support/Viking/viking-maps"
#else /* POSIX */
#include <stdlib.h>
#define GLOBAL_MAPS_DIR "/var/cache/maps/"
#define LOCAL_MAPS_DIR ".viking-maps"
#endif




const QString & maps_layer_default_dir(void)
{
	static QString default_dir;
	if (default_dir.isEmpty()) {

		/* Thanks to Mike Davison for the $VIKING_MAPS usage. */
		const char * mapdir = g_getenv("VIKING_MAPS");
		if (mapdir) {
			default_dir = QString(mapdir);
		} else if (0 == access(GLOBAL_MAPS_DIR, W_OK)) {
			default_dir = QString(GLOBAL_MAPS_DIR);
		} else {
			const QString home_full_path = QDir::homePath();
			if (!home_full_path.isEmpty() && 0 == access(home_full_path.toUtf8().constData(), W_OK)) {
				default_dir = home_full_path + QDir::separator() + LOCAL_MAPS_DIR;
			} else {
				default_dir = QString(LOCAL_MAPS_DIR);
			}
		}
		if (!default_dir.isEmpty() && !default_dir.endsWith(QDir::separator())) {
			/* Add the separator at the end. */
			default_dir += QDir::separator();
		}
		qDebug() << "DD: Layer Map: get default dir: dir is" << default_dir;
	}
	return default_dir;
}




void LayerMap::mkdir_if_default_dir()
{
	if (!this->cache_dir.isEmpty()
	    && this->cache_dir == map_cache_dir()
	    && 0 != access(this->cache_dir.toUtf8().constData(), F_OK)) {

		if (g_mkdir(this->cache_dir.toUtf8().constData(), 0777) != 0) {
			qDebug() << "WW: Layer Map:: Failed to create directory" << this->cache_dir;
		}
	}
}




void LayerMap::set_cache_dir(const QString & dir)
{
	this->cache_dir = "";

	QString mydir = dir;
	if (dir.isEmpty()) {
		if (Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL ".maplayer_default_dir")) {
			mydir = Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL ".maplayer_default_dir")->val_string;
		}
	}

	char * canonical_dir = vu_get_canonical_filename(this, mydir, this->get_window()->get_current_document_full_path());

	/* Ensure cache_dir always ends with a separator. */
	unsigned int len = strlen(canonical_dir);
	/* Unless the dir is not valid. */
	if (len > 0) {
		if (canonical_dir[len-1] != G_DIR_SEPARATOR) {
			this->cache_dir = QString(canonical_dir) + QString(G_DIR_SEPARATOR_S);
			free(canonical_dir);
		} else {
			this->cache_dir = canonical_dir;
		}

		this->mkdir_if_default_dir();
	}
}




void LayerMap::set_file(const QString & name_)
{
	this->filename = name_;
}




/****************************************/
/************** PARAMETERS **************/
/****************************************/

static MapTypeID map_index_to_map_type(unsigned int index)
{
	assert (index < map_sources.size());
	return map_sources[index]->map_type;
}




/**
   \brief Get index for indexing map_sources table

   \return valid index if map type was found
   \return -1 if given map type was not found
*/
static int map_type_to_map_index(MapTypeID map_type)
{
	for (unsigned int i = 0; i < map_sources.size(); i++) {
		if (map_sources[i]->map_type == map_type) {
			return (int) i;
		}
	}
	return -1; /* no such thing */
}




#define VIK_SETTINGS_MAP_LICENSE_SHOWN "map_license_shown"

/**
 * Convenience function to display the license.
 */
static void maps_show_license(Window * parent, MapSource * map)
{
	Dialog::license(map->get_label(), map->get_license(), map->get_license_url(), parent);
}




bool LayerMap::set_param_value(uint16_t id, const SGVariant & data, bool is_file_operation)
{
	switch (id) {
	case PARAM_CACHE_DIR:
		this->set_cache_dir(data.val_string);
		break;
	case PARAM_CACHE_LAYOUT:
		if ((MapsCacheLayout) data.val_int < MapsCacheLayout::NUM) {
			this->cache_layout = (MapsCacheLayout) data.val_int;
		}
		break;
	case PARAM_FILE:
		this->set_file(data.val_string);
		break;
	case PARAM_MAPTYPE: {
		int map_index_ = map_type_to_map_index((MapTypeID) data.val_int);
		if (map_index_ == -1) {
			qDebug() << QObject::tr("WARNING: Unknown map type");
		} else {
			this->map_index = (unsigned int) map_index_;

			/* When loading from a file don't need the license reminder - ensure it's saved into the 'seen' list. */
			if (is_file_operation) {
				ApplicationState::set_integer_list_containing(VIK_SETTINGS_MAP_LICENSE_SHOWN, data.val_int);
			} else {
				MapSource * map = map_sources[this->map_index];
				if (map->get_license() != NULL) {
					/* Check if licence for this map type has been shown before. */
					if (!ApplicationState::get_integer_list_contains(VIK_SETTINGS_MAP_LICENSE_SHOWN, data.val_int)) {
						maps_show_license(this->get_window(), map);
						ApplicationState::set_integer_list_containing(VIK_SETTINGS_MAP_LICENSE_SHOWN, data.val_int);
					}
				}
			}
		}
		break;
	}
	case PARAM_ALPHA:
		if (data.val_int >= scale_alpha.min && data.val_int <= scale_alpha.max) {
			this->alpha = data.val_int;
		}
		break;
	case PARAM_AUTODOWNLOAD:
		this->autodownload = data.val_bool;
		break;
	case PARAM_ONLYMISSING:
		this->adl_only_missing = data.val_bool;
		break;
	case PARAM_MAPZOOM:
		if (data.val_int < params_mapzooms.size()) {
			this->mapzoom_id = data.val_int;
			this->xmapzoom = __mapzooms_x[data.val_int];
			this->ymapzoom = __mapzooms_y[data.val_int];
		} else {
			qDebug() << QObject::tr("WARNING: Unknown Map Zoom");
		}
		break;
	default:
		break;
	}
	return true;
}




SGVariant LayerMap::get_param_value(param_id_t id, bool is_file_operation) const
{
	SGVariant rv;
	switch (id) {
	case PARAM_CACHE_DIR: {
		bool set = false;
		/* Only save a blank when the map cache location equals the default.
		   On reading in, when it is blank then the default is reconstructed.
		   Since the default changes dependent on the user and OS, it means the resultant file is more portable. */
		if (is_file_operation
		    && !this->cache_dir.isEmpty()
		    && this->cache_dir == map_cache_dir()) {

			rv = SGVariant("");
			set = true;
		} else if (is_file_operation && !this->cache_dir.isEmpty()) {
			if (Preferences::get_file_ref_format() == VIK_FILE_REF_FORMAT_RELATIVE) {
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
		rv = SGVariant(this->filename);
		break;
	case PARAM_MAPTYPE:
		rv = SGVariant((int32_t) map_index_to_map_type(this->map_index));
		break;
	case PARAM_ALPHA:
		rv = SGVariant((int32_t) this->alpha);
		break;
	case PARAM_AUTODOWNLOAD:
		rv = SGVariant(this->autodownload); /* kamilkamil: in viking code there is a type mismatch. */
		break;
	case PARAM_ONLYMISSING:
		rv = SGVariant(this->adl_only_missing); /* kamilkamil: in viking code there is a type mismatch. */
		break;
	case PARAM_MAPZOOM:
		rv = SGVariant((int32_t) this->mapzoom_id);
		break;
	default: break;
	}
	return rv;
}



#ifdef K
void LayerMapInterface::change_param(void * gtk_widget, ui_change_values * values)
{
	switch (values->param_id) {
		/* Alter sensitivity of download option widgets according to the map_index setting. */
	case PARAM_MAPTYPE: {
		/* Get new value. */
		SGVariant var = a_uibuilder_widget_get_value(gtk_widget, values->param);
		/* Is it *not* the OSM On Disk Tile Layout or the MBTiles type or the OSM Metatiles type. */
		bool sensitive = (MAP_ID_OSM_ON_DISK != var.i &&
				  MAP_ID_MBTILES != var.i &&
				  MAP_ID_OSM_METATILES != var.i);
		GtkWidget **ww1 = values->widgets;
		GtkWidget **ww2 = values->labels;
		GtkWidget *w1 = ww1[PARAM_ONLYMISSING];
		GtkWidget *w2 = ww2[PARAM_ONLYMISSING];
		GtkWidget *w3 = ww1[PARAM_AUTODOWNLOAD];
		GtkWidget *w4 = ww2[PARAM_AUTODOWNLOAD];
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
		sensitive = (MAP_ID_MBTILES == var.i);
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
	case PARAM_AUTODOWNLOAD: {
		/* Get new value. */
		SGVariant var = a_uibuilder_widget_get_value(gtk_widget, values->param);
		GtkWidget **ww1 = values->widgets;
		GtkWidget **ww2 = values->labels;
		GtkWidget *w1 = ww1[PARAM_ONLYMISSING];
		GtkWidget *w2 = ww2[PARAM_ONLYMISSING];
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

#ifdef HAVE_SQLITE3_H
	MapSource * map = map_sources[this->map_index];
	if (map->is_mbtiles()) {
		if (this->mbtiles) {
			int ans = sqlite3_close(this->mbtiles);
			if (ans != SQLITE_OK) {
				/* Only to console for information purposes only. */
				fprintf(stderr, "WARNING: SQL Close problem: %d\n", ans);
			}
		}
	}
#endif
}




void LayerMap::post_read(Viewport * viewport, bool from_file)
{
	MapSource * map = map_sources[this->map_index];

	if (!from_file) {
		/* If this method is not called in file reading context it is called in GUI context.
		   So, we can check if we have to inform the user about inconsistency. */
		if (map->get_drawmode() != viewport->get_drawmode()) {
			const QString drawmode_name = ViewportDrawModes::get_name(map->get_drawmode());
			const QString msg = QString(QObject::tr("New map cannot be displayed in the current drawmode.\nSelect \"%1\" from View menu to view it.")).arg(drawmode_name);
			Dialog::warning(msg, this->get_window());
		}
	}

	/* Performed in post read as we now know the map type. */
#ifdef HAVE_SQLITE3_H
	/* Do some SQL stuff. */
	if (map->is_mbtiles()) {
		int ans = sqlite3_open_v2(this->filename.toUtf8().constData(),
					  &(this->mbtiles),
					  SQLITE_OPEN_READONLY,
					  NULL);
		if (ans != SQLITE_OK) {
			/* That didn't work, so here's why: */
			fprintf(stderr, "WARNING: %s: %s\n", __FUNCTION__, sqlite3_errmsg(this->mbtiles));

			Dialog::error(tr("Failed to open MBTiles file: %1").arg(this->filename), this->get_window());
			this->mbtiles = NULL;
		}
	}
#endif

	/* If the on Disk OSM Tile Layout type. */
	if (map->map_type == MAP_ID_OSM_ON_DISK) {
		/* Copy the directory into filename.
		   Thus the map cache look up will be unique when using more than one of these map types. */
		this->filename = this->cache_dir;
	}
}




QString LayerMap::get_tooltip(void)
{
	return this->get_map_label();
}




Layer * LayerMapInterface::unmarshall(uint8_t * data, size_t data_len, Viewport * viewport)
{
	LayerMap * layer = new LayerMap();

	layer->unmarshall_params(data, data_len);
	layer->post_read(viewport, false);
	return layer;
}




/*********************/
/****** DRAWING ******/
/*********************/

static QPixmap * get_pixmap_from_file(LayerMap * layer, const char * full_path);

static QPixmap * pixmap_shrink(QPixmap *pixmap, double xshrinkfactor, double yshrinkfactor)
{
	int width = pixmap->width();
	int height = pixmap->height();

	QPixmap * tmp = NULL;
#ifdef K
	tmp = pixmap->scaled(ceil(width * xshrinkfactor), ceil(height * yshrinkfactor), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	g_object_unref(G_OBJECT(pixmap));
#endif
	return tmp;
}




#ifdef HAVE_SQLITE3_H
#if 0
static int sql_select_tile_dump_cb(void *data, int cols, char **fields, char **col_names)
{
	fprintf(stderr, "WARNING: Found %d columns\n", cols);
	for (int i = 0; i < cols; i++) {
		fprintf(stderr, "WARNING: SQL processing %s = %s\n", col_names[i], fields[i]);
	}
	return 0;
}
#endif




static QPixmap *get_pixmap_sql_exec(sqlite3 *sql, int xx, int yy, int zoom)
{
	QPixmap *pixmap = NULL;

	/* MBTiles stored internally with the flipping y thingy (i.e. TMS scheme). */
	int flip_y = (int) pow(2, zoom)-1 - yy;
	char *statement = g_strdup_printf("SELECT tile_data FROM tiles WHERE zoom_level=%d AND tile_column=%d AND tile_row=%d;", zoom, xx, flip_y);

	bool finished = false;

	sqlite3_stmt *sql_stmt = NULL;
	int ans = sqlite3_prepare_v2(sql, statement, -1, &sql_stmt, NULL);
	if (ans != SQLITE_OK) {
		fprintf(stderr, "WARNING: %s: %s - %d: %s\n", __FUNCTION__, "prepare failure", ans, statement);
		finished = true;
	}

	while (!finished) {
		ans = sqlite3_step(sql_stmt);
		switch (ans) {
		case SQLITE_ROW: {
			/* Get tile_data blob. */
			int count = sqlite3_column_count(sql_stmt);
			if (count != 1)  {
				fprintf(stderr, "WARNING: %s: %s - %d\n", __FUNCTION__, "count not one", count);
				finished = true;
			} else {
				const void *data = sqlite3_column_blob(sql_stmt, 0);
				int bytes = sqlite3_column_bytes(sql_stmt, 0);
				if (bytes < 1)  {
					fprintf(stderr, "WARNING: %s: %s (%d)\n", __FUNCTION__, "not enough bytes", bytes);
					finished = true;
				} else {
					/* Convert these blob bytes into a pixmap via these streaming operations. */
					GInputStream *stream = g_memory_input_stream_new_from_data(data, bytes, NULL);
					GError *error = NULL;
					pixmap = gdk_pixbuf_new_from_stream(stream, NULL, &error);
					if (error) {
						fprintf(stderr, "WARNING: %s: %s\n", __FUNCTION__, error->message);
						g_error_free(error);
					}
					g_input_stream_close(stream, NULL, NULL);
				}
			}
			break;
		}
		default:
			/* e.g. SQLITE_DONE | SQLITE_ERROR | SQLITE_MISUSE | etc...
			   Finished normally and give up on any errors. */
			if (ans != SQLITE_DONE) {
				fprintf(stderr, "WARNING: %s: %s - %d\n", __FUNCTION__, "step issue", ans);
			}
			finished = true;
			break;
		}
	}
	(void)sqlite3_finalize(sql_stmt);

	free(statement);

	return pixmap;
}
#endif




static QPixmap *get_mbtiles_pixmap(LayerMap * layer, int xx, int yy, int zoom)
{
	QPixmap *pixmap = NULL;

#ifdef HAVE_SQLITE3_H
	if (layer->mbtiles) {
		/*
		  char *statement = g_strdup_printf("SELECT name FROM sqlite_master WHERE type='table';");
		  char *errMsg = NULL;
		  int ans = sqlite3_exec(layer->mbtiles, statement, sql_select_tile_dump_cb, pixmap, &errMsg);
		  if (ans != SQLITE_OK) {
		  // Only to console for information purposes only
		  fprintf(stderr, "WARNING: SQL problem: %d for %s - error: %s\n", ans, statement, errMsg);
		  sqlite3_free(errMsg);
		  }
		  free(statement);
		*/

		/* Reading BLOBS is a bit more involved and so can't use the simpler sqlite3_exec().
		   Hence this specific function. */
		pixmap = get_pixmap_sql_exec(layer->mbtiles, xx, yy, zoom);
	}
#endif

	return pixmap;
}




static QPixmap * get_pixmap_from_metatile(LayerMap * layer, int xx, int yy, int zz)
{
	const int tile_max = METATILE_MAX_SIZE;
	char err_msg[PATH_MAX] = { 0 };
	int compressed;

	char * buf = (char *) malloc(tile_max);
	if (!buf) {
		return NULL;
	}

	int len = metatile_read(layer->cache_dir.toUtf8().constData(), xx, yy, zz, buf, tile_max, &compressed, err_msg);
	if (len > 0) {
		if (compressed) {
			/* Not handled yet - I don't think this is used often - so implement later if necessary. */
			qDebug() << "EE: Layer Map: ge pixmap from metafile: compressed metatiles not implemented";
			free(buf);
			return NULL;
		}

		/* Convert these buf bytes into a pixmap via these streaming operations. */
		QPixmap * pixmap = NULL;

#ifdef K
		GInputStream *stream = g_memory_input_stream_new_from_data(buf, len, NULL);
		GError *error = NULL;
		pixmap = gdk_pixbuf_new_from_stream(stream, NULL, &error);
		if (error) {
			fprintf(stderr, "WARNING: %s: %s", __FUNCTION__, error->message);
			g_error_free(error);
		}
		g_input_stream_close(stream, NULL, NULL);
#endif
		free(buf);
		return pixmap;
	} else {
		free(buf);
		qDebug() << "EE: Layer Map: get pixmap from metafile: failed:" << err_msg;
		return NULL;
	}
}




/**
 * Caller has to decrease reference counter of returned.
 * QPixmap, when buffer is no longer needed.
 */
static QPixmap * pixmap_apply_settings(QPixmap * pixmap, uint8_t alpha, double xshrinkfactor, double yshrinkfactor)
{
	/* Apply alpha setting. */
	if (pixmap && alpha < 255) {
		pixmap = ui_pixmap_set_alpha(pixmap, alpha);
	}

	if (pixmap && (xshrinkfactor != 1.0 || yshrinkfactor != 1.0)) {
		pixmap = pixmap_shrink(pixmap, xshrinkfactor, yshrinkfactor);
	}

	return pixmap;
}




static void get_cache_filename(const QString & cache_dir,
			       MapsCacheLayout cl,
			       uint16_t id,
			       const char *name,
			       TileInfo const * coord,
			       char *filename_buf,
			       int buf_len,
			       const char* file_extension)
{
	switch (cl) {
	case MapsCacheLayout::OSM:
		if (name) {
			if (cache_dir != map_cache_dir()) {
				/* Cache dir not the default - assume it's been directed somewhere specific. */
				snprintf(filename_buf, buf_len, DIRECTDIRACCESS, cache_dir.toUtf8().constData(), (17 - coord->scale), coord->x, coord->y, file_extension);
			} else {
				/* Using default cache - so use the map name in the directory path. */
				snprintf(filename_buf, buf_len, DIRECTDIRACCESS_WITH_NAME, cache_dir.toUtf8().constData(), name, (17 - coord->scale), coord->x, coord->y, file_extension);
			}
		} else {
			snprintf(filename_buf, buf_len, DIRECTDIRACCESS, cache_dir.toUtf8().constData(), (17 - coord->scale), coord->x, coord->y, file_extension);
		}
		break;
	default:
		snprintf(filename_buf, buf_len, DIRSTRUCTURE, cache_dir.toUtf8().constData(), id, coord->scale, coord->z, coord->x, coord->y);
		break;
	}
}




/**
 * Caller has to decrease reference counter of returned.
 * QPixmap, when buffer is no longer needed.
 */
static QPixmap * get_pixmap(LayerMap * layer, MapTypeID map_type, const char* mapname, TileInfo *mapcoord, char *filename_buf, int buf_len, double xshrinkfactor, double yshrinkfactor)
{
	/* Get the thing. */
	QPixmap * pixmap = map_cache_get(mapcoord, map_type, layer->alpha, xshrinkfactor, yshrinkfactor, layer->filename.toUtf8().constData());
	if (pixmap) {
		//fprintf(stderr, "Layer Map: MAP CACHE HIT\n");
	} else {
		//fprintf(stderr, "Layer Map: MAP CACHE MISS\n");
		MapSource * map = map_sources[layer->map_index];
		if (map->is_direct_file_access()) {
			/* ATM MBTiles must be 'a direct access type'. */
			if (map->is_mbtiles()) {
				pixmap = get_mbtiles_pixmap(layer, mapcoord->x, mapcoord->y, (17 - mapcoord->scale));
			} else if (map->is_osm_meta_tiles()) {
				pixmap = get_pixmap_from_metatile(layer, mapcoord->x, mapcoord->y, (17 - mapcoord->scale));
			} else {
				get_cache_filename(layer->cache_dir, MapsCacheLayout::OSM, map_type, NULL,
					     mapcoord, filename_buf, buf_len,
					     map->get_file_extension());
				pixmap = get_pixmap_from_file(layer, filename_buf);
			}
		} else {
			get_cache_filename(layer->cache_dir, layer->cache_layout, map_type, mapname,
				     mapcoord, filename_buf, buf_len,
				     map->get_file_extension());
			pixmap = get_pixmap_from_file(layer, filename_buf);
		}

		if (pixmap) {
			pixmap = pixmap_apply_settings(pixmap, layer->alpha, xshrinkfactor, yshrinkfactor);

			map_cache_extra_t arg;
			arg.duration = 0.0;
			map_cache_add(pixmap, arg, mapcoord, map_sources[layer->map_index]->map_type,
				      layer->alpha, xshrinkfactor, yshrinkfactor, layer->filename.toUtf8().constData());
		}
	}
	return pixmap;
}




static QPixmap * get_pixmap_from_file(LayerMap * layer, const char * full_path)
{
	if (0 != access(full_path, F_OK | R_OK)) {
		qDebug() << "EE: Layer Map: can't access file" << full_path;
		return NULL;
	}

	QPixmap * pixmap = new QPixmap;

	if (!pixmap->load(QString(full_path))) {
		Window * window = layer->get_window();
		if (window) {
			window->statusbar_update(StatusBarField::INFO, QString("Couldn't open image file"));
		}
		delete pixmap;
		pixmap = NULL;
	}
	return pixmap;
}




static bool should_start_autodownload(LayerMap * layer, Viewport * viewport)
{
	const Coord * center = viewport->get_center();

	if (layer->get_window()->get_pan_move()) {
		/* D'n'D pan in action: do not download. */
		return false;
	}

	/* Don't attempt to download unsupported zoom levels. */
	double xzoom = viewport->get_xmpp();
	MapSource *map = map_sources[layer->map_index];
	uint8_t zl = map_utils_mpp_to_zoom_level(xzoom);
	if (zl < map->get_zoom_min() || zl > map->get_zoom_max()) {
		return false;
	}

	if (layer->last_center == NULL) {
		Coord * new_center = new Coord();
		*new_center = *center; /* KamilFIXME: can we do it this way? */
		layer->last_center = new_center;
		layer->last_xmpp = viewport->get_xmpp();
		layer->last_ympp = viewport->get_ympp();
		return true;
	}

	/* TODO: perhaps Coord::distance() */
	if ((*layer->last_center == *center)
	    && (layer->last_xmpp == viewport->get_xmpp())
	    && (layer->last_ympp == viewport->get_ympp())) {
		return false;
	}

	*(layer->last_center) = *center;
	layer->last_xmpp = viewport->get_xmpp();
	layer->last_ympp = viewport->get_ympp();
	return true;
}




bool try_draw_scale_down(LayerMap * layer, Viewport * viewport, TileInfo ulm, int xx, int yy, int tilesize_x_ceil, int tilesize_y_ceil,
			 double xshrinkfactor, double yshrinkfactor, MapTypeID map_type, const char *mapname, char *path_buf, unsigned int max_path_len)
{
	for (unsigned int scale_inc = 1; scale_inc < SCALE_INC_DOWN; scale_inc++) {
		/* Try with smaller zooms. */
		int scale_factor = 1 << scale_inc;  /* 2^scale_inc */
		TileInfo ulm2 = ulm;
		ulm2.x = ulm.x / scale_factor;
		ulm2.y = ulm.y / scale_factor;
		ulm2.scale = ulm.scale + scale_inc;
		QPixmap * pixmap = get_pixmap(layer, map_type, mapname, &ulm2, path_buf, max_path_len, xshrinkfactor * scale_factor, yshrinkfactor * scale_factor);
		if (pixmap) {
			int src_x = (ulm.x % scale_factor) * tilesize_x_ceil;
			int src_y = (ulm.y % scale_factor) * tilesize_y_ceil;
			viewport->draw_pixmap(*pixmap, src_x, src_y, xx, yy, tilesize_x_ceil, tilesize_y_ceil);
#ifdef K
			g_object_unref(pixmap);
#endif
			return true;
		}
	}
	return false;
}




bool try_draw_scale_up(LayerMap * layer, Viewport * viewport, TileInfo ulm, int xx, int yy, int tilesize_x_ceil, int tilesize_y_ceil,
		       double xshrinkfactor, double yshrinkfactor, MapTypeID map_type, const char *mapname, char *path_buf, unsigned int max_path_len)
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
				QPixmap * pixmap = get_pixmap(layer, map_type, mapname, &ulm3, path_buf, max_path_len, xshrinkfactor / scale_factor, yshrinkfactor / scale_factor);
				if (pixmap) {
					int src_x = 0;
					int src_y = 0;
					int dest_x = xx + pict_x * (tilesize_x_ceil / scale_factor);
					int dest_y = yy + pict_y * (tilesize_y_ceil / scale_factor);
					viewport->draw_pixmap(*pixmap, src_x, src_y, dest_x, dest_y, tilesize_x_ceil / scale_factor, tilesize_y_ceil / scale_factor);
#ifdef K
					g_object_unref(pixmap);
#endif
					return true;
				}
			}
		}
	}
	return false;
}




void LayerMap::draw_section(Viewport * viewport, const Coord & coord_ul, const Coord & coord_br)
{
	double xzoom = viewport->get_xmpp();
	double yzoom = viewport->get_ympp();
	double xshrinkfactor = 1.0;
	double yshrinkfactor = 1.0;
	bool existence_only = false;

	if (this->xmapzoom && (this->xmapzoom != xzoom || this->ymapzoom != yzoom)) {
		xshrinkfactor = this->xmapzoom / xzoom;
		yshrinkfactor = this->ymapzoom / yzoom;
		xzoom = this->xmapzoom;
		yzoom = this->xmapzoom;
		if (! (xshrinkfactor > MIN_SHRINKFACTOR && xshrinkfactor < MAX_SHRINKFACTOR &&
			yshrinkfactor > MIN_SHRINKFACTOR && yshrinkfactor < MAX_SHRINKFACTOR)) {
			if (xshrinkfactor > REAL_MIN_SHRINKFACTOR && yshrinkfactor > REAL_MIN_SHRINKFACTOR) {
				fprintf(stderr, "DEBUG: %s: existence_only due to SHRINKFACTORS", __FUNCTION__);
				existence_only = true;
			} else {
				/* Report the reason for not drawing. */
				Window * w = this->get_window();
				if (w) {
					QString msg = QString("Refusing to draw tiles or existence of tiles beyond %1 zoom out factor").arg((int)(1.0/REAL_MIN_SHRINKFACTOR));
					w->statusbar_update(StatusBarField::INFO, msg);
				}
				return;
			}
		}
	}

	/* coord -> ID */
	TileInfo ulm, brm;
	MapSource *map = (MapSource *) map_sources[this->map_index];
	if (map->coord_to_tile(coord_ul, xzoom, yzoom, &ulm) &&
	     map->coord_to_tile(coord_br, xzoom, yzoom, &brm)) {

		/* Loop & draw. */
		//int x, y;
		int xmin = MIN(ulm.x, brm.x), xmax = MAX(ulm.x, brm.x);
		int ymin = MIN(ulm.y, brm.y), ymax = MAX(ulm.y, brm.y);
		MapTypeID map_type = map->map_type;
		const char *mapname = map->get_name();

		Coord coord;
		int xx, yy;
		QPixmap *pixmap;

		/* Prevent the program grinding to a halt if trying to deal with thousands of tiles
		   which can happen when using a small fixed zoom level and viewing large areas.
		   Also prevents very large number of tile download requests. */
		int tiles = (xmax-xmin) * (ymax-ymin);
		if (tiles > MAX_TILES) {
			fprintf(stderr, "DEBUG: %s: existence_only due to wanting too many tiles (%d)", __FUNCTION__, tiles);
			existence_only = true;
		}

		unsigned int max_path_len = strlen(this->cache_dir.toUtf8().constData()) + 40;
		char *path_buf = (char *) malloc(max_path_len * sizeof(char));

		if ((!existence_only) && this->autodownload  && should_start_autodownload(this, viewport)) {
			fprintf(stderr, "DEBUG: %s: Starting autodownload", __FUNCTION__);
			if (!this->adl_only_missing && map->supports_download_only_new()) {
				/* Try to download newer tiles. */
				this->start_download_thread(viewport, coord_ul, coord_br, REDOWNLOAD_NEW);
			} else {
				/* Download only missing tiles. */
				this->start_download_thread(viewport, coord_ul, coord_br, REDOWNLOAD_NONE);
			}
		}

		if (map->get_tilesize_x() == 0 && !existence_only) {
			for (int x = xmin; x <= xmax; x++) {
				for (int y = ymin; y <= ymax; y++) {
					ulm.x = x;
					ulm.y = y;
					pixmap = get_pixmap(this, map_type, mapname, &ulm, path_buf, max_path_len, xshrinkfactor, yshrinkfactor);
					if (pixmap) {
						int width = pixmap->width();
						int height = pixmap->height();

						map->tile_to_center_coord(&ulm, coord);
						viewport->coord_to_screen_pos(coord, &xx, &yy);
						xx -= (width/2);
						yy -= (height/2);

						viewport->draw_pixmap(*pixmap, 0, 0, xx, yy, width, height);
#ifdef K
						g_object_unref(pixmap);
#endif
					}
				}
			}
		} else { /* tilesize is known, don't have to keep converting coords. */
			double tilesize_x = map->get_tilesize_x() * xshrinkfactor;
			double tilesize_y = map->get_tilesize_y() * yshrinkfactor;
			/* ceiled so tiles will be maximum size in the case of funky shrinkfactor. */
			int tilesize_x_ceil = ceil (tilesize_x);
			int tilesize_y_ceil = ceil(tilesize_y);
			int8_t xinc = (ulm.x == xmin) ? 1 : -1;
			int8_t yinc = (ulm.y == ymin) ? 1 : -1;
			int xx_tmp, yy_tmp;

			int xend = (xinc == 1) ? (xmax+1) : (xmin-1);
			int yend = (yinc == 1) ? (ymax+1) : (ymin-1);

			map->tile_to_center_coord(&ulm, coord);
			viewport->coord_to_screen_pos(coord, &xx_tmp, &yy_tmp);
			xx = xx_tmp; yy = yy_tmp;
			/* Above trick so xx,yy doubles. this is so shrinkfactors aren't rounded off
			   e.g. if tile size 128, shrinkfactor 0.333. */
			xx -= (tilesize_x/2);
			int base_yy = yy - (tilesize_y/2);

			for (int x = ((xinc == 1) ? xmin : xmax); x != xend; x+=xinc) {
				yy = base_yy;
				for (int y = ((yinc == 1) ? ymin : ymax); y != yend; y+=yinc) {
					ulm.x = x;
					ulm.y = y;

					if (existence_only) {
						if (map_sources[this->map_index]->is_direct_file_access()) {
							get_cache_filename(this->cache_dir, MapsCacheLayout::OSM, map_type, map->get_name(),
								     &ulm, path_buf, max_path_len, map->get_file_extension());
						} else {
							get_cache_filename(this->cache_dir, this->cache_layout, map_type, map->get_name(),
								     &ulm, path_buf, max_path_len, map->get_file_extension());
						}

						if (0 == access(path_buf, F_OK)) {
							const QPen pen (QColor("#E6202E")); /* kamilTODO: This should be black. */
							viewport->draw_line(pen, xx+tilesize_x_ceil, yy, xx, yy+tilesize_y_ceil);
						}
					} else {
						/* Try correct scale first. */
						int scale_factor = 1;
						pixmap = get_pixmap(this, map_type, mapname, &ulm, path_buf, max_path_len, xshrinkfactor * scale_factor, yshrinkfactor * scale_factor);
						if (pixmap) {
							int src_x = (ulm.x % scale_factor) * tilesize_x_ceil;
							int src_y = (ulm.y % scale_factor) * tilesize_y_ceil;
							viewport->draw_pixmap(*pixmap, src_x, src_y, xx, yy, tilesize_x_ceil, tilesize_y_ceil);
#ifdef K
							g_object_unref(pixmap);
#endif
						} else {
							/* Otherwise try different scales. */
							if (SCALE_SMALLER_ZOOM_FIRST) {
								if (!try_draw_scale_down(this, viewport ,ulm,xx,yy,tilesize_x_ceil,tilesize_y_ceil,xshrinkfactor,yshrinkfactor, map_type, mapname,path_buf,max_path_len)) {
									try_draw_scale_up(this, viewport ,ulm,xx,yy,tilesize_x_ceil,tilesize_y_ceil,xshrinkfactor,yshrinkfactor, map_type, mapname,path_buf,max_path_len);
								}
							} else {
								if (!try_draw_scale_up(this, viewport, ulm,xx,yy,tilesize_x_ceil,tilesize_y_ceil,xshrinkfactor,yshrinkfactor, map_type, mapname,path_buf,max_path_len)) {
									try_draw_scale_down(this, viewport ,ulm,xx,yy,tilesize_x_ceil,tilesize_y_ceil,xshrinkfactor,yshrinkfactor, map_type, mapname,path_buf,max_path_len);
								}
							}
						}
					}

					yy += tilesize_y;
				}
				xx += tilesize_x;
			}

			/* ATM Only show tile grid lines in extreme debug mode. */
			if (vik_debug && vik_verbose) {
				/* Grid drawing here so it gets drawn on top of the map.
				   Thus loop around x & y again, but this time separately.
				   Only showing grid for the current scale */

				const QPen pen (QColor("#E6202E")); /* kamilTODO: This should be black. */

				/* Draw single grid lines across the whole screen. */
				int width = viewport->get_width();
				int height = viewport->get_height();
				xx = xx_tmp; yy = yy_tmp;
				int base_xx = xx - (tilesize_x/2);
				base_yy = yy - (tilesize_y/2);

				xx = base_xx;
				for (int x = ((xinc == 1) ? xmin : xmax); x != xend; x+=xinc) {
					viewport->draw_line(pen, xx, base_yy, xx, height);
					xx += tilesize_x;
				}

				yy = base_yy;
				for (int y = ((yinc == 1) ? ymin : ymax); y != yend; y+=yinc) {
					viewport->draw_line(pen, base_xx, yy, width, yy);
					yy += tilesize_y;
				}
			}

		}
		free(path_buf);
	}
}




void LayerMap::draw(Viewport * viewport)
{
	if (map_sources[this->map_index]->get_drawmode() == viewport->get_drawmode()) {

		/* Copyright. */
		double level = viewport->get_zoom();
		const LatLonBBox bbox = viewport->get_bbox();
#ifdef K /* linkage problem. */
		map_sources[this->map_index]->get_copyright(bbox, level, SlavGPS::vik_viewport_add_copyright_cb, viewport);
#endif

		/* Logo. */

		const QPixmap * logo = map_sources[this->map_index]->get_logo();
		viewport->add_logo(logo);

		/* Get corner coords. */
		if (viewport->get_coord_mode() == CoordMode::UTM && ! viewport->is_one_zone()) {
			/* UTM multi-zone stuff by Kit Transue. */
			const char leftmost_zone = viewport->get_leftmost_zone();
			const char rightmost_zone = viewport->get_rightmost_zone();
			Coord coord_ul;
			Coord coord_br;
			for (char zone = leftmost_zone; zone <= rightmost_zone; ++zone) {
				viewport->get_corners_for_zone(coord_ul, coord_br, zone);
				this->draw_section(viewport, coord_ul, coord_br);
			}
		} else {
			const Coord coord_ul = viewport->screen_pos_to_coord(0, 0);
			const Coord coord_br = viewport->screen_pos_to_coord(viewport->get_width(), viewport->get_height());

			this->draw_section(viewport, coord_ul, coord_br);
		}
	}
}




/*************************/
/****** DOWNLOADING ******/
/*************************/

/* Pass along data to thread, exists even if layer is deleted. */
class MapDownloadJob : public BackgroundJob {
public:
	MapDownloadJob() {};
	MapDownloadJob(LayerMap * layer, TileInfo * ulm, TileInfo * brm, bool refresh_display, int redownload_mode);
	~MapDownloadJob();

	void cleanup_on_cancel(void);

	QString cache_dir;
	char * filename_buf = NULL;
	MapsCacheLayout cache_layout;
	int x0 = 0;
	int y0 = 0;
	int xf = 0;
	int yf = 0;
	TileInfo mapcoord;
	int map_index = 0;
	int maxlen = 0;
	int mapstoget = 0;
	int redownload_mode = 0;
	bool refresh_display = false;
	LayerMap * layer = NULL;
	bool map_layer_alive = true;
	std::mutex mutex;
};

static QString redownload_mode_message(int redownload_mode, int mapstoget, const QString & label);
static void mdj_calculate_mapstoget(MapDownloadJob * mdj, MapSource * map, TileInfo * ulm);
static void mdj_calculate_mapstoget_other(MapDownloadJob * mdj, MapSource * map, TileInfo * ulm);




void LayerMap::weak_ref_cb(void * ptr, void * dead_vml)
{
	MapDownloadJob * mdj = (MapDownloadJob *) ptr;
	mdj->mutex.lock();
	mdj->map_layer_alive = false;
	mdj->mutex.unlock();
}




static bool is_in_area(MapSource * map, TileInfo * mc)
{
	Coord center_coord;
	map->tile_to_center_coord(mc, center_coord);

	const Coord coord_tl(LatLon(map->get_lat_max(), map->get_lon_min()), CoordMode::LATLON);
	const Coord coord_br(LatLon(map->get_lat_min(), map->get_lon_max()), CoordMode::LATLON);

	return center_coord.is_inside(&coord_tl, &coord_br);
}




static int map_download_thread(BackgroundJob * bg_job)
{
	MapDownloadJob * mdj = (MapDownloadJob *) bg_job;

	void *handle = map_sources[mdj->map_index]->download_handle_init();
	unsigned int donemaps = 0;
	TileInfo mcoord = mdj->mapcoord;

	for (mcoord.x = mdj->x0; mcoord.x <= mdj->xf; mcoord.x++) {
		for (mcoord.y = mdj->y0; mcoord.y <= mdj->yf; mcoord.y++) {
			/* Only attempt to download a tile from supported areas. */
			if (is_in_area(map_sources[mdj->map_index], &mcoord)) {
				bool remove_mem_cache = false;
				bool need_download = false;

				get_cache_filename(mdj->cache_dir, mdj->cache_layout,
					     map_sources[mdj->map_index]->map_type,
					     map_sources[mdj->map_index]->get_name(),
					     &mcoord, mdj->filename_buf, mdj->maxlen,
					     map_sources[mdj->map_index]->get_file_extension());

				donemaps++;

				int res = a_background_thread_progress(bg_job, ((double)donemaps) / mdj->mapstoget); /* this also calls testcancel */
				if (res != 0) {
					map_sources[mdj->map_index]->download_handle_cleanup(handle);
					return -1;
				}

				if (0 != access(mdj->filename_buf, F_OK)) {
					need_download = true;
					remove_mem_cache = true;

				} else {  /* In case map file already exists. */
					switch (mdj->redownload_mode) {
					case REDOWNLOAD_NONE:
						continue;

					case REDOWNLOAD_BAD: {
						/* See if this one is bad or what. */
						QPixmap tmp_pixmap; /* Apparently this will pixmap is only for test of some kind. */
						if (!tmp_pixmap.load(mdj->filename_buf)) {
							if (remove(mdj->filename_buf)) {
								qDebug() << "WW: Layer Map: Redownload Bad failed to remove", mdj->filename_buf;
							}
							need_download = true;
							remove_mem_cache = true;
						}
						break;
					}

					case REDOWNLOAD_NEW:
						need_download = true;
						remove_mem_cache = true;
						break;

					case REDOWNLOAD_ALL:
						/* FIXME: need a better way than to erase file in case of server/network problem. */
						if (remove(mdj->filename_buf)) {
							qDebug() << "WW: Layer Map: Redownload All failed to remove", mdj->filename_buf;
						}
						need_download = true;
						remove_mem_cache = true;
						break;

					case DOWNLOAD_OR_REFRESH:
						remove_mem_cache = true;
						break;

					default:
						fprintf(stderr, "WARNING: redownload mode %d unknown\n", mdj->redownload_mode);
					}
				}

				mdj->mapcoord.x = mcoord.x;
				mdj->mapcoord.y = mcoord.y;

				if (need_download) {
					DownloadResult dr = map_sources[mdj->map_index]->download(&(mdj->mapcoord), mdj->filename_buf, handle);
					switch (dr) {
					case DownloadResult::HTTP_ERROR:
					case DownloadResult::CONTENT_ERROR: {
						/* TODO: ?? count up the number of download errors somehow... */
						QString msg = QString("%1: %2").arg(mdj->layer->get_map_label()).arg("Failed to download tile");
						mdj->layer->get_window()->statusbar_update(StatusBarField::INFO, msg);
						break;
					}
					case DownloadResult::FILE_WRITE_ERROR: {
						QString msg = QString("%1: %2").arg(mdj->layer->get_map_label()).arg("Unable to save tile");
						mdj->layer->get_window()->statusbar_update(StatusBarField::INFO, msg);
						break;
					}
					case DownloadResult::SUCCESS:
					case DownloadResult::NOT_REQUIRED:
					default:
						break;
					}
				}

				mdj->mutex.lock();
				if (remove_mem_cache) {
					map_cache_remove_all_shrinkfactors(&mcoord, map_sources[mdj->map_index]->map_type, mdj->layer->filename.toUtf8().constData());
				}

				if (mdj->refresh_display && mdj->map_layer_alive) {
					/* TODO: check if it's on visible area. */
					mdj->layer->emit_layer_changed(); /* NB update display from background. */
				}
				mdj->mutex.unlock();

				/* We're temporarily between downloads. */
				mdj->mapcoord.x = 0;
				mdj->mapcoord.y = 0;
			}
		}
	}
	map_sources[mdj->map_index]->download_handle_cleanup(handle);
	mdj->mutex.lock();
	if (mdj->map_layer_alive) {
		mdj->layer->weak_unref(LayerMap::weak_ref_cb, mdj);
	}
	mdj->mutex.unlock();
	return 0;
}




void MapDownloadJob::cleanup_on_cancel(void)
{
	if (this->mapcoord.x || this->mapcoord.y) {
		get_cache_filename(this->cache_dir, this->cache_layout,
				   map_sources[this->map_index]->map_type,
				   map_sources[this->map_index]->get_name(),
				   &this->mapcoord, this->filename_buf, this->maxlen,
				   map_sources[this->map_index]->get_file_extension());
		if (0 == access(this->filename_buf, F_OK)) {
			if (remove(this->filename_buf)) {
				fprintf(stderr, "WARNING: Cleanup failed to remove: %s", this->filename_buf);
			}
		}
	}
}




void LayerMap::start_download_thread(Viewport * viewport, const Coord & coord_ul, const Coord & coord_br, int redownload_mode)
{
	double xzoom = this->xmapzoom ? this->xmapzoom : viewport->get_xmpp();
	double yzoom = this->ymapzoom ? this->ymapzoom : viewport->get_ympp();
	TileInfo ulm, brm;
	MapSource *map = map_sources[this->map_index];

	/* Don't ever attempt download on direct access. */
	if (map->is_direct_file_access()) {
		return;
	}

	if (map->coord_to_tile(coord_ul, xzoom, yzoom, &ulm)
	     && map->coord_to_tile(coord_br, xzoom, yzoom, &brm)) {

		MapDownloadJob * mdj = new MapDownloadJob(this, &ulm, &brm, true, redownload_mode);

		if (mdj->redownload_mode) {
			mdj->mapstoget = (mdj->xf - mdj->x0 + 1) * (mdj->yf - mdj->y0 + 1);
		} else {
			mdj_calculate_mapstoget(mdj, map, &ulm);
		}

		/* For cleanup - no current map. */
		mdj->mapcoord.x = 0;
		mdj->mapcoord.y = 0;

		if (mdj->mapstoget) {
			const QString job_description = redownload_mode_message(redownload_mode, mdj->mapstoget, LAYER_MAP_NTH_LABEL(this->map_index));

			mdj->layer->weak_ref(LayerMap::weak_ref_cb, mdj);
			mdj->n_items = mdj->mapstoget; /* kamilTODO: Hide in constructor or in mdj_calculate_mapstoget(). */
			a_background_thread(mdj, ThreadPoolType::REMOTE, job_description);
		} else {
			delete mdj;
		}
	}
}




void LayerMap::download_section_sub(const Coord & coord_ul, const Coord & coord_br, double zoom, int redownload_mode)
{
	TileInfo ulm, brm;
	MapSource *map = map_sources[this->map_index];

	/* Don't ever attempt download on direct access. */
	if (map->is_direct_file_access()) {
		return;
	}

	if (!map->coord_to_tile(coord_ul, zoom, zoom, &ulm)
	    || !map->coord_to_tile(coord_br, zoom, zoom, &brm)) {
		fprintf(stderr, "WARNING: %s() coord_to_tile() failed", __PRETTY_FUNCTION__);
		return;
	}

	MapDownloadJob * mdj = new MapDownloadJob(this, &ulm, &brm, true, redownload_mode);

	mdj_calculate_mapstoget(mdj, map, &ulm);

	/* For cleanup - no current map. */
	mdj->mapcoord.x = 0;
	mdj->mapcoord.y = 0;

	if (mdj->mapstoget) {
		const QString job_description = redownload_mode_message(redownload_mode, mdj->mapstoget, LAYER_MAP_NTH_LABEL(this->map_index));

		mdj->layer->weak_ref(weak_ref_cb, mdj);
		mdj->n_items = mdj->mapstoget; /* kamilTODO: Hide in constructor or in mdj_calculate_mapstoget(). */

		a_background_thread(mdj, ThreadPoolType::REMOTE, job_description);
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
	this->download_section_sub(coord_ul, coord_br, zoom, REDOWNLOAD_NONE);
}




void LayerMap::redownload_bad_cb(void)
{
	this->start_download_thread(this->redownload_viewport, this->redownload_ul, this->redownload_br, REDOWNLOAD_BAD);
}




void LayerMap::redownload_all_cb(void)
{
	this->start_download_thread(this->redownload_viewport, this->redownload_ul, this->redownload_br, REDOWNLOAD_ALL);
}




void LayerMap::redownload_new_cb(void)
{
	this->start_download_thread(this->redownload_viewport, this->redownload_ul, this->redownload_br, REDOWNLOAD_NEW);
}




/**
 * Display a simple dialog with information about this particular map tile
 */
void LayerMap::tile_info_cb(void)
{
	MapSource * map = map_sources[this->map_index];

	double xzoom = this->xmapzoom ? this->xmapzoom : this->redownload_viewport->get_xmpp();
	double yzoom = this->ymapzoom ? this->ymapzoom : this->redownload_viewport->get_ympp();
	TileInfo ulm;

	if (!map->coord_to_tile(this->redownload_ul, xzoom, yzoom, &ulm)) {
		return;
	}

	char * tile_filename = NULL;
	QString source;

	if (map->is_direct_file_access()) {
		if (map->is_mbtiles()) {
			tile_filename = g_strdup(this->filename.toUtf8().constData());
#ifdef HAVE_SQLITE3_H
			/* And whether to bother going into the SQL to check it's really there or not... */
			QString exists;
			int zoom = 17 - ulm.scale;
			if (this->mbtiles) {
				QPixmap *pixmap = get_pixmap_sql_exec(this->mbtiles, ulm.x, ulm.y, zoom);
				if (pixmap) {
					exists = QObject::tr("YES");
					g_object_unref(G_OBJECT(pixmap));
				} else {
					exists = QObject::tr("NO");
				}
			} else {
				exists = QObject::tr("NO");
			}

			int flip_y = (int) pow(2, zoom)-1 - ulm.y;
			/* NB Also handles .jpg automatically due to pixmap_new_from() support - although just print png for now. */
			source = QObject::tr("Source: %1 (%2%3%4%5%6.%7 %8)")
				.arg(tile_filename)
				.arg(zoom)
				.arg(G_DIR_SEPARATOR_S)
				.arg(ulm.x)
				.arg(G_DIR_SEPARATOR_S)
				.arg(flip_y)
				.arg("png")
				.arg(exists);
#else
			source = QObject::tr("Source: Not available");
#endif
		} else if (map->is_osm_meta_tiles()) {
			char path[PATH_MAX];
			xyz_to_meta(path, sizeof (path), this->cache_dir.toUtf8().constData(), ulm.x, ulm.y, 17-ulm.scale);
			source = path;
			tile_filename = g_strdup(path);
		} else {
			unsigned int max_path_len = strlen(this->cache_dir.toUtf8().constData()) + 40;
			tile_filename = (char *) malloc(max_path_len * sizeof(char));
			get_cache_filename(this->cache_dir, MapsCacheLayout::OSM,
				     map->map_type,
				     NULL,
				     &ulm, tile_filename, max_path_len,
				     map->get_file_extension());
			source = QObject::tr("Source: file://%1").arg(tile_filename);
		}
	} else {
		unsigned int max_path_len = strlen(this->cache_dir.toUtf8().constData()) + 40;
		tile_filename = (char *) malloc(max_path_len * sizeof(char));
		get_cache_filename(this->cache_dir, this->cache_layout,
			     map->map_type,
			     map->get_name(),
			     &ulm, tile_filename, max_path_len,
			     map->get_file_extension());
		source = QObject::tr("Source: http://%1%2").arg(map->get_server_hostname()).arg(map->get_server_path(&ulm));
	}

	QStringList items;
	items.push_back(source);

	/* kamilTODO: you have very similar code in LayerMapnik::tile_info. */

	QString file_message;
	QString time_message;

	if (0 == access(tile_filename, F_OK)) {
		file_message = QString(tr("Tile File: %1")).arg(tile_filename);
		/* Get some timestamp information of the tile. */
		struct stat stat_buf;
		if (stat(tile_filename, &stat_buf) == 0) {
			char time_buf[64];
			strftime(time_buf, sizeof (time_buf), "%c", gmtime((const time_t *) &stat_buf.st_mtime));
			time_message = tr("Tile File Timestamp: %1").arg(time_buf);
		} else {
			time_message = tr("Tile File Timestamp: Not Available");
		}

	} else {
		file_message = tr("Tile File: %1 [Not Available]").arg(tile_filename);
		time_message = "";
	}

	items.push_back(file_message);
	items.push_back(time_message);

	a_dialog_list(tr("Tile Information"), items, 5, this->get_window());

	free(tile_filename);
}




ToolStatus LayerToolMapsDownload::handle_mouse_release(Layer * _layer, QMouseEvent * event)
{
	if (!_layer || _layer->type != LayerType::MAP) {
		return ToolStatus::IGNORED;
	}

	LayerMap * layer = (LayerMap *) _layer;

	if (layer->dl_tool_x != -1 && layer->dl_tool_y != -1) {
		if (event->button() == Qt::LeftButton) {
			const Coord coord_ul = this->viewport->screen_pos_to_coord(MAX(0, MIN(event->x(), layer->dl_tool_x)), MAX(0, MIN(event->y(), layer->dl_tool_y)));
			const Coord coord_br = this->viewport->screen_pos_to_coord(MIN(this->viewport->get_width(), MAX(event->x(), layer->dl_tool_x)), MIN(this->viewport->get_height(), MAX (event->y(), layer->dl_tool_y)));
			layer->start_download_thread(this->viewport, coord_ul, coord_br, DOWNLOAD_OR_REFRESH);
			layer->dl_tool_x = layer->dl_tool_y = -1;
			return ToolStatus::ACK;
		} else {
			layer->redownload_ul = this->viewport->screen_pos_to_coord(MAX(0, MIN(event->x(), layer->dl_tool_x)), MAX(0, MIN(event->y(), layer->dl_tool_y)));
			layer->redownload_br = this->viewport->screen_pos_to_coord(MIN(this->viewport->get_width(), MAX(event->x(), layer->dl_tool_x)), MIN(this->viewport->get_height(), MAX (event->y(), layer->dl_tool_y)));

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
	return ToolStatus::IGNORED;
}




LayerToolMapsDownload::LayerToolMapsDownload(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::MAP)
{
	this->id_string = "sg.tool.layer_map.maps_download";

	this->action_icon_path   = "vik-icon-Maps Download";
	this->action_label       = QObject::tr("_Maps Download");
	this->action_tooltip     = QObject::tr("Maps Download");
	// this->action_accelerator = ...; /* Empty accelerator. */

	/* kamilTODO: use correct values for these cursors. */
	this->cursor_click = new QCursor(QPixmap(":/cursors/trw_edit_wp.png"), 0, 0);
	this->cursor_release = new QCursor(Qt::ArrowCursor);
#ifdef K
	this->cursor_shape = Qt::BitmapCursor;
	this->cursor_data = &cursor_mapdl_pixmap;
#endif
}




ToolStatus LayerToolMapsDownload::handle_mouse_click(Layer * _layer, QMouseEvent * event)
{
	TileInfo tmp;
	if (!_layer || _layer->type != LayerType::MAP) {
		return ToolStatus::IGNORED;
	}

	LayerMap * layer = (LayerMap *) _layer;

	MapSource *map = map_sources[layer->map_index];
	if (map->get_drawmode() == this->viewport->get_drawmode()
	    && map->coord_to_tile(*this->viewport->get_center(),
				  layer->xmapzoom ? layer->xmapzoom : this->viewport->get_xmpp(),
				  layer->ymapzoom ? layer->ymapzoom : this->viewport->get_ympp(),
				  &tmp)) {

		layer->dl_tool_x = event->x();
		layer->dl_tool_y = event->y();
		return ToolStatus::ACK;
	}
	return ToolStatus::IGNORED;
}




void LayerMap::download_onscreen_maps(int redownload_mode)
{
	Viewport * viewport = this->get_window()->get_viewport();

	double xzoom = this->xmapzoom ? this->xmapzoom : viewport->get_xmpp();
	double yzoom = this->ymapzoom ? this->ymapzoom : viewport->get_ympp();

	TileInfo ulm, brm;

	const Coord coord_ul = viewport->screen_pos_to_coord(0, 0);
	const Coord coord_br = viewport->screen_pos_to_coord(viewport->get_width(), viewport->get_height());

	MapSource *map = map_sources[this->map_index];

	const ViewportDrawMode map_draw_mode = map->get_drawmode();
	const ViewportDrawMode vp_draw_mode = viewport->get_drawmode();

	if (map_draw_mode == vp_draw_mode
	    && map->coord_to_tile(coord_ul, xzoom, yzoom, &ulm)
	    && map->coord_to_tile(coord_br, xzoom, yzoom, &brm)) {

		this->start_download_thread(viewport, coord_ul, coord_br, redownload_mode);

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
	this->download_onscreen_maps(REDOWNLOAD_NONE);
}




void LayerMap::download_new_onscreen_maps_cb(void)
{
	this->download_onscreen_maps(REDOWNLOAD_NEW);
}




void LayerMap::redownload_all_onscreen_maps_cb(void)
{
	this->download_onscreen_maps(REDOWNLOAD_ALL);
}




void LayerMap::about_cb(void)
{
	MapSource * map = map_sources[this->map_index];
	if (map->get_license()) {
		maps_show_license(this->get_window(), map);
	} else {
		Dialog::info(map->get_label(), this->get_window());
	}
}




/**
 * Copied from maps_layer_download_section but without the actual download and this returns a value
 */
int LayerMap::how_many_maps(const Coord & coord_ul, const Coord & coord_br, double zoom, int redownload_mode)
{
	TileInfo ulm, brm;
	MapSource *map = map_sources[this->map_index];

	if (map->is_direct_file_access()) {
		return 0;
	}

	if (!map->coord_to_tile(coord_ul, zoom, zoom, &ulm)
	    || !map->coord_to_tile(coord_br, zoom, zoom, &brm)) {
		fprintf(stderr, "WARNING: %s() coord_to_tile() failed", __PRETTY_FUNCTION__);
		return 0;
	}

	MapDownloadJob * mdj = new MapDownloadJob(this, &ulm, &brm, false, redownload_mode);

	if (mdj->redownload_mode == REDOWNLOAD_ALL) {
		mdj->mapstoget = (mdj->xf - mdj->x0 + 1) * (mdj->yf - mdj->y0 + 1);
	} else {
		mdj_calculate_mapstoget_other(mdj, map, &ulm);
	}

	int rv = mdj->mapstoget;

	delete mdj;

	return rv;
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
			      int default_download,
			      int * selected_zoom1,
			      int * selected_zoom2,
			      int * selected_download)
{

	QDialog dialog(parent);
	dialog.setWindowTitle(title);


	QVBoxLayout vbox;
	QLayout * old = dialog.layout();
	delete old;
	dialog.setLayout(&vbox);


	QLabel zoom_label1(QObject::tr("Zoom Start:"));
	vbox.addWidget(&zoom_label1);


	QComboBox zoom_combo1;
	for (int i = 0; i < zoom_list.size(); i++) {
		zoom_combo1.addItem(zoom_list.at(i), i);
	}
	zoom_combo1.setCurrentIndex(default_zoom1);
	vbox.addWidget(&zoom_combo1);


	QLabel zoom_label2(QObject::tr("Zoom End:"));
	vbox.addWidget(&zoom_label2);


	QComboBox zoom_combo2;
	for (int i = 0; i < zoom_list.size(); i++) {
		zoom_combo2.addItem(zoom_list.at(i), i);
	}
	zoom_combo2.setCurrentIndex(default_zoom2);
	vbox.addWidget(&zoom_combo2);


	QLabel download_label(QObject::tr("Download Maps Method:"));
	vbox.addWidget(&download_label);


	QComboBox download_combo;
	for (int i = 0; i < download_list.size(); i++) {
		download_combo.addItem(download_list.at(i), i);
	}
	download_combo.setCurrentIndex(default_download);
	vbox.addWidget(&download_combo);


	/* kamilTODO: Set focus on Ok button. */


	QDialogButtonBox button_box;
	button_box.addButton(QDialogButtonBox::Ok);
	button_box.addButton(QDialogButtonBox::Cancel);
	QObject::connect(&button_box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	QObject::connect(&button_box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	vbox.addWidget(&button_box);


	if (dialog.exec() != QDialog::Accepted) {
		return false;
	}


	/* Return selected options. */
	*selected_zoom1 = zoom_combo1.currentIndex();
	*selected_zoom2 = zoom_combo2.currentIndex();
	*selected_download = download_combo.currentIndex();


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
	download_list << QObject::tr("Missing") << QObject::tr("Bad") << QObject::tr("New") << QObject::tr("Reload All");

	int selected_zoom1, selected_zoom2, default_zoom, lower_zoom;
	int selected_download_method;

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
				      REDOWNLOAD_NONE, /* AKA Missing. */
				      &selected_zoom1,
				      &selected_zoom2,
				      &selected_download_method)) {
		/* Cancelled. */
		return;
	}

	/* Find out new current positions. */
	const LatLonMinMax min_max = viewport->get_min_max_lat_lon();
	const Coord coord_ul(LatLon(min_max.max.lat, min_max.min.lon), viewport->get_coord_mode());
	const Coord coord_br(LatLon(min_max.min.lat, min_max.max.lon), viewport->get_coord_mode());

	/* Get Maps Count - call for each zoom level (in reverse).
	   With REDOWNLOAD_NEW this is a possible maximum.
	   With REDOWNLOAD_NONE this only missing ones - however still has a server lookup per tile. */
	int map_count = 0;
	for (int zz = selected_zoom2; zz >= selected_zoom1; zz--) {
		map_count = map_count + this->how_many_maps(coord_ul, coord_br, zoom_vals[zz], selected_download_method);
	}

	fprintf(stderr, "DEBUG: Layer Map: download request map count %d for method %d", map_count, selected_download_method);

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
		this->download_section_sub(coord_ul, coord_br, zoom_vals[zz], selected_download_method);
	}
}




void LayerMap::flush_cb(void)
{
	map_cache_flush_type(map_sources[this->map_index]->map_type);
}




void LayerMap::add_menu_items(QMenu & menu)
{
	QAction * qa = NULL;

	qa = new QAction(QObject::tr("Download &Missing Onscreen Maps"), this);
	qa->setIcon(QIcon::fromTheme("list-add"));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (download_missing_onscreen_maps_cb(void)));
	menu.addAction(qa);

	if (map_sources[this->map_index]->supports_download_only_new()) {
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




void mdj_calculate_mapstoget(MapDownloadJob * mdj, MapSource * map, TileInfo * ulm)
{
	TileInfo mcoord = mdj->mapcoord;
	mcoord.z = ulm->z;
	mcoord.scale = ulm->scale;

	for (mcoord.x = mdj->x0; mcoord.x <= mdj->xf; mcoord.x++) {
		for (mcoord.y = mdj->y0; mcoord.y <= mdj->yf; mcoord.y++) {
			/* Only count tiles from supported areas. */
			if (is_in_area(map, &mcoord)) {
				get_cache_filename(mdj->cache_dir, mdj->cache_layout,
						   map->map_type,
						   map->get_name(),
						   &mcoord, mdj->filename_buf, mdj->maxlen,
						   map->get_file_extension());

				if (0 != access(mdj->filename_buf, F_OK)) {
					mdj->mapstoget++;
				}
			}
		}
	}

	return;
}




void mdj_calculate_mapstoget_other(MapDownloadJob * mdj, MapSource * map, TileInfo * ulm)
{
	TileInfo mcoord = mdj->mapcoord;
	mcoord.z = ulm->z;
	mcoord.scale = ulm->scale;

	/* Calculate how many we need. */
	for (mcoord.x = mdj->x0; mcoord.x <= mdj->xf; mcoord.x++) {
		for (mcoord.y = mdj->y0; mcoord.y <= mdj->yf; mcoord.y++) {
			/* Only count tiles from supported areas. */
			if (!is_in_area(map, &mcoord)) {
				continue;
			}

			get_cache_filename(mdj->cache_dir, mdj->cache_layout,
					   map->map_type,
					   map->get_name(),
					   &mcoord, mdj->filename_buf, mdj->maxlen,
					   map->get_file_extension());
			if (mdj->redownload_mode == REDOWNLOAD_NEW) {
				/* Assume the worst - always a new file.
				   Absolute value would require a server lookup - but that is too slow. */
				mdj->mapstoget++;
			} else {
				if (0 != access(mdj->filename_buf, F_OK)) {
					/* Missing. */
					mdj->mapstoget++;
				} else {
					if (mdj->redownload_mode == REDOWNLOAD_BAD) {
						/* see if this one is bad or what */
						QPixmap * pixmap = new QPixmap();
						if (!pixmap->load(mdj->filename_buf)) {
							delete pixmap;
							pixmap = NULL;
							mdj->mapstoget++;
						} else {
							/* kamilFIXME: where do we free the pixmap? */
						}
						break;
						/* Other download cases already considered or just ignored. */
					}
				}
			}
		}
	}

	return;
}




MapDownloadJob::MapDownloadJob(LayerMap * layer_, TileInfo * ulm_, TileInfo * brm_, bool refresh_display_, int redownload_mode_)
{
	this->thread_fn = map_download_thread;

	this->layer = layer_;
	this->refresh_display = refresh_display_;

	/* cache_dir and buffer for dest filename. */
	this->cache_dir = layer->cache_dir;
	this->maxlen = strlen(layer->cache_dir.toUtf8().constData()) + 40;
	this->filename_buf = (char *) malloc(this->maxlen * sizeof(char));
	this->map_index = layer->map_index;
	this->cache_layout = layer->cache_layout;

	/* kamilFIXME: in original code there was an assignment of structures:
	   this->mapcoord = ulm_; */
	memcpy(&this->mapcoord, ulm_, sizeof (TileInfo));
	this->redownload_mode = redownload_mode_;

	this->x0 = MIN(ulm_->x, brm_->x);
	this->xf = MAX(ulm_->x, brm_->x);
	this->y0 = MIN(ulm_->y, brm_->y);
	this->yf = MAX(ulm_->y, brm_->y);
}




MapDownloadJob::~MapDownloadJob()
{
	free(this->filename_buf);
	this->filename_buf = NULL;
}




QString redownload_mode_message(int redownload_mode, int mapstoget, const QString & label)
{
	QString fmt;
	if (redownload_mode) {
		if (redownload_mode == REDOWNLOAD_BAD) {
			fmt = QObject::tr("Redownloading up to %n %s maps...", "", mapstoget);
		} else {
			fmt = QObject::tr("Redownloading %n %s maps...", "", mapstoget);
		}
	} else {
		fmt = QObject::tr("Downloading %n %s maps...", "", mapstoget);
	}

	return QString(fmt).arg(label);
}




LayerMap::LayerMap()
{
	fprintf(stderr, "LayerMap::LayerMap()\n");

	this->type = LayerType::MAP;
	strcpy(this->debug_string, "MAP");
	this->interface = &vik_map_layer_interface;

	this->set_initial_parameter_values();
	this->set_name(Layer::get_type_ui_label(this->type));
}
