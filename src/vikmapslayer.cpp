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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixdata.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include <deque>
#include <cstdlib>
#include <cassert>

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "viking.h"
#include "vikmapsource.h"
#include "vikslippymapsource.h"
#include "vikutils.h"
#include "maputils.h"
#include "mapcache.h"
#include "background.h"
#include "preferences.h"
#include "vikmapslayer.h"
#include "icons/icons.h"
#include "metatile.h"
#include "ui_util.h"
#include "map_ids.h"
#include "viklayer_defaults.h"
#include "vikfileentry.h"
#include "dialog.h"
#include "file.h"
#include "settings.h"
#include "globals.h"
#include "vik_compat.h"
#include "uibuilder.h"

#ifdef HAVE_SQLITE3_H
#include "sqlite3.h"
#include <gio/gio.h>
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


/* List of label for each map type. */
static char ** map_type_labels = NULL;

/* Corresponding IDs. (Cf. field map_type in MapSource class). */
static MapTypeID * map_type_ids = NULL;

/******** MAPZOOMS *********/

static char *params_mapzooms[] = { (char *) N_("Use Viking Zoom Level"), (char *) "0.25", (char *) "0.5", (char *) "1", (char *) "2", (char *) "4", (char *) "8", (char *) "16", (char *) "32", (char *) "64", (char *) "128", (char *) "256", (char *) "512", (char *) "1024", (char *) "USGS 10k", (char *) "USGS 24k", (char *) "USGS 25k", (char *) "USGS 50k", (char *) "USGS 100k", (char *) "USGS 200k", (char *) "USGS 250k", NULL };
static double __mapzooms_x[] = { 0.0, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 256.0, 512.0, 1024.0, 1.016, 2.4384, 2.54, 5.08, 10.16, 20.32, 25.4 };
static double __mapzooms_y[] = { 0.0, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 256.0, 512.0, 1024.0, 1.016, 2.4384, 2.54, 5.08, 10.16, 20.32, 25.4 };

#define NUM_MAPZOOMS (sizeof(params_mapzooms)/sizeof(params_mapzooms[0]) - 1)

/**************************/


static Layer * maps_layer_unmarshall(uint8_t * data, int len, Viewport * viewport);
static void maps_layer_change_param(GtkWidget *widget, ui_change_values * values);
static void start_download_thread(LayerMaps * layer, Viewport * viewport, const VikCoord *ul, const VikCoord *br, int redownload_mode);
static unsigned int map_type_to_map_index(MapTypeID map_type);


static LayerTool * maps_layer_download_create(Window * window, Viewport * viewport);
static bool maps_layer_download_release(Layer * layer, GdkEventButton *event, LayerTool * tool);
static bool maps_layer_download_click(Layer * layer, GdkEventButton *event, LayerTool * tool);




static LayerParamScale params_scales[] = {
	/* min, max, step, digits (decimal places). */
	{ 0, 255, 3, 0 }, /* alpha */
};




static LayerParamData id_default(void)
{
	return VIK_LPD_UINT(MAP_ID_MAPQUEST_OSM);
}




static LayerParamData directory_default(void)
{
	LayerParamData data;
	LayerParamData *pref = a_preferences_get(VIKING_PREFERENCES_NAMESPACE "maplayer_default_dir");
	if (pref) {
		data.s = g_strdup(pref->s);
	} else {
		data.s = "";
	}
	return data;
}




static LayerParamData file_default(void)
{
	LayerParamData data;
	data.s = "";
	return data;
}




static LayerParamData alpha_default(void)
{
	return VIK_LPD_UINT (255);
}




static LayerParamData mapzoom_default(void)
{
	return VIK_LPD_UINT (0);
}




static char * cache_types[] = { (char *) "Viking", (char *) N_("OSM"), NULL };
static MapsCacheLayout cache_layout_default_value = MapsCacheLayout::VIKING;




static LayerParamData cache_layout_default(void)
{
	return VIK_LPD_UINT ((int32_t) cache_layout_default_value);
}




LayerParam maps_layer_params[] = {
	// NB mode => map source type id - But can't break file format just to rename something better
	{ LayerType::MAPS, "mode",           LayerParamType::UINT,    VIK_LAYER_GROUP_NONE, N_("Map Type:"),                            LayerWidgetType::COMBOBOX,    NULL,                               NULL, NULL, id_default, NULL, NULL },
	{ LayerType::MAPS, "directory",      LayerParamType::STRING,  VIK_LAYER_GROUP_NONE, N_("Maps Directory:"),                      LayerWidgetType::FOLDERENTRY, NULL,                               NULL, NULL, directory_default, NULL, NULL },
	{ LayerType::MAPS, "cache_type",     LayerParamType::UINT,    VIK_LAYER_GROUP_NONE, N_("Cache Layout:"),                        LayerWidgetType::COMBOBOX,    cache_types,                        NULL, N_("This determines the tile storage layout on disk"), cache_layout_default, NULL, NULL },
	{ LayerType::MAPS, "mapfile",        LayerParamType::STRING,  VIK_LAYER_GROUP_NONE, N_("Map File:"),                            LayerWidgetType::FILEENTRY,   KINT_TO_POINTER(VF_FILTER_MBTILES), NULL, N_("An MBTiles file. Only applies when the map type method is 'MBTiles'"), file_default, NULL, NULL },
	{ LayerType::MAPS, "alpha",          LayerParamType::UINT,    VIK_LAYER_GROUP_NONE, N_("Alpha:"),                               LayerWidgetType::HSCALE,      params_scales,                      NULL, N_("Control the Alpha value for transparency effects"), alpha_default, NULL, NULL },
	{ LayerType::MAPS, "autodownload",   LayerParamType::BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Autodownload maps:"),                   LayerWidgetType::CHECKBUTTON, NULL,                               NULL, NULL, vik_lpd_true_default, NULL, NULL },
	{ LayerType::MAPS, "adlonlymissing", LayerParamType::BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Autodownload Only Gets Missing Maps:"), LayerWidgetType::CHECKBUTTON, NULL,                               NULL, N_("Using this option avoids attempting to update already acquired tiles. This can be useful if you want to restrict the network usage, without having to resort to manual control. Only applies when 'Autodownload Maps' is on."), vik_lpd_false_default, NULL, NULL },
	{ LayerType::MAPS, "mapzoom",        LayerParamType::UINT,    VIK_LAYER_GROUP_NONE, N_("Zoom Level:"),                          LayerWidgetType::COMBOBOX,    params_mapzooms,                    NULL, N_("Determines the method of displaying map tiles for the current zoom level. 'Viking Zoom Level' uses the best matching level, otherwise setting a fixed value will always use map tiles of the specified value regardless of the actual zoom level."),	  mapzoom_default, NULL, NULL },
};




enum {
	PARAM_MAPTYPE=0,
	PARAM_CACHE_DIR,
	PARAM_CACHE_LAYOUT,
	PARAM_FILE,
	PARAM_ALPHA,
	PARAM_AUTODOWNLOAD,
	PARAM_ONLYMISSING,
	PARAM_MAPZOOM,
	NUM_PARAMS
};




void maps_layer_set_autodownload_default(bool autodownload)
{
	/* Set appropriate function. */
	if(autodownload) {
		maps_layer_params[PARAM_AUTODOWNLOAD].default_value = vik_lpd_true_default;
	} else {
		maps_layer_params[PARAM_AUTODOWNLOAD].default_value = vik_lpd_false_default;
	}
}




void maps_layer_set_cache_default(MapsCacheLayout layout)
{
	/* Override default value returned by the default param function. */
	cache_layout_default_value = layout;
}




static LayerTool * maps_tools[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL } ; /* (VikToolConstructorFunc) */

VikLayerInterface vik_maps_layer_interface = {
	"Map",
	N_("Map"),
	"<control><shift>M",
	&vikmapslayer_pixbuf,

	{ maps_layer_download_create, NULL, NULL, NULL, NULL, NULL, NULL },
	maps_tools,
	1,

	maps_layer_params,
	NUM_PARAMS,
	NULL,
	0,

	VIK_MENU_ITEM_ALL,

	/* (VikLayerFuncUnmarshall) */   maps_layer_unmarshall,
	/* (VikLayerFuncChangeParam) */  maps_layer_change_param,
};




enum { REDOWNLOAD_NONE = 0,    /* Download only missing maps. */
       REDOWNLOAD_BAD,         /* Download missing and bad maps. */
       REDOWNLOAD_NEW,         /* Download missing maps that are newer on server only. */
       REDOWNLOAD_ALL,         /* Download all maps. */
       DOWNLOAD_OR_REFRESH };  /* Download missing maps and refresh cache. */




static LayerParam prefs[] = {
	{ LayerType::NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "maplayer_default_dir", LayerParamType::STRING, VIK_LAYER_GROUP_NONE, N_("Default map layer directory:"), LayerWidgetType::FOLDERENTRY, NULL, NULL, N_("Choose a directory to store cached Map tiles for this layer"), NULL, NULL, NULL },
};




void maps_layer_init()
{
	LayerParamData tmp;
	tmp.s = maps_layer_default_dir();
	a_preferences_register(prefs, tmp, VIKING_PREFERENCES_GROUP_KEY);

	int max_tiles = MAX_TILES;
	if (a_settings_get_integer(VIK_SETTINGS_MAP_MAX_TILES, &max_tiles)) {
		MAX_TILES = max_tiles;
	}

	double gdtmp;
	if (a_settings_get_double(VIK_SETTINGS_MAP_MIN_SHRINKFACTOR, &gdtmp)) {
		MIN_SHRINKFACTOR = gdtmp;
	}

	if (a_settings_get_double(VIK_SETTINGS_MAP_MAX_SHRINKFACTOR, &gdtmp)) {
		MAX_SHRINKFACTOR = gdtmp;
	}

	if (a_settings_get_double(VIK_SETTINGS_MAP_REAL_MIN_SHRINKFACTOR, &gdtmp)) {
		REAL_MIN_SHRINKFACTOR = gdtmp;
	}

	int gitmp = 0;
	if (a_settings_get_integer(VIK_SETTINGS_MAP_SCALE_INC_UP, &gitmp)) {
		SCALE_INC_UP = gitmp;
	}

	if (a_settings_get_integer(VIK_SETTINGS_MAP_SCALE_INC_DOWN, &gitmp)) {
		SCALE_INC_DOWN = gitmp;
	}

	bool gbtmp = true;
	if (a_settings_get_boolean(VIK_SETTINGS_MAP_SCALE_SMALLER_ZOOM_FIRST, &gbtmp)) {
		SCALE_SMALLER_ZOOM_FIRST = gbtmp;
	}

}




/****************************************/
/******** MAPS LAYER TYPES **************/
/****************************************/

void _add_map_source(MapSource * map, const char * label, MapTypeID map_type)
{
	size_t len = 0;
	if (map_type_labels) {
		len = g_strv_length(map_type_labels);
	}
	/* Add the label. */
	map_type_labels = (char **) g_realloc(map_type_labels, (len + 2) * sizeof (char *));
	map_type_labels[len] = g_strdup(label);
	map_type_labels[len+1] = NULL;

	/* Add the id. */
	map_type_ids = (MapTypeID *) g_realloc(map_type_ids, (len + 2) * sizeof (MapTypeID));
	map_type_ids[len] = map_type;
	map_type_ids[len+1] = (MapTypeID) 0; /* Guard. */

	/* We have to clone. */
	MapSource clone = *map; /* FIXME: to clone or not to clone? */
	/* Register the clone in the list */
	map_sources.push_back(map);

	/* Hack.
	   We have to ensure the mode LayerParam references the up-to-date GLists.
	*/
	/*
	  memcpy(&maps_layer_params[0].widget_data, &map_type_labels, sizeof(void *));
	  memcpy(&maps_layer_params[0].extra_widget_data, &map_type_ids, sizeof(void *));
	*/
	maps_layer_params[0].widget_data = map_type_labels;
	maps_layer_params[0].extra_widget_data = map_type_ids;
}




void _update_map_source(MapSource *map, const char *label, unsigned int index)
{
	if (index >= map_sources.size()) {
		return;
	}

	MapSource * map_source = map_sources[index];
	map_sources[index] = map;
	if (map_source) {
		// delete map_source; /* kamilFIXME: free this pointer. */
	}


	/* Change previous data. */
	free(map_type_labels[index]);
	map_type_labels[index] = g_strdup(label);
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

	unsigned int previous = map_type_to_map_index(map_type);
	if (previous != map_sources.size()) {
		_update_map_source(map, label, previous);
	} else {
		_add_map_source(map, label, map_type);
	}
}




#define MAPS_LAYER_NTH_LABEL(n) (map_type_labels[n])
#define MAPS_LAYER_NTH_ID(n) (map_type_ids[n])

/**
 * Returns the actual map id (rather than the internal type index value).
 */
MapTypeID LayerMaps::get_map_type()
{
	return MAPS_LAYER_NTH_ID(this->map_index);
}




void LayerMaps::set_map_type(MapTypeID map_type)
{
	unsigned int map_index = map_type_to_map_index(map_type);
	if (map_index == map_sources.size()) {
		fprintf(stderr, _("WARNING: Unknown map type\n"));
	} else {
		this->map_index = map_index;
	}
}




MapTypeID maps_layer_get_default_map_type()
{
	LayerInterface * vli = Layer::get_interface(LayerType::MAPS);
	LayerParamData vlpd = a_layer_defaults_get(vli->fixed_layer_name, "mode", LayerParamType::UINT);
	if (vlpd.u == 0) {
		vlpd = id_default();
	}
	return (MapTypeID) vlpd.u;
}




char * LayerMaps::get_map_label()
{
	return strdup(MAPS_LAYER_NTH_LABEL(this->map_index));
}




/****************************************/
/******** CACHE DIR STUFF ***************/
/****************************************/

#define DIRECTDIRACCESS "%s%d" G_DIR_SEPARATOR_S "%d" G_DIR_SEPARATOR_S "%d%s"
#define DIRECTDIRACCESS_WITH_NAME "%s%s" G_DIR_SEPARATOR_S "%d" G_DIR_SEPARATOR_S "%d" G_DIR_SEPARATOR_S "%d%s"
#define DIRSTRUCTURE "%st%ds%dz%d" G_DIR_SEPARATOR_S "%d" G_DIR_SEPARATOR_S "%d"
#define MAPS_CACHE_DIR maps_layer_default_dir()

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




char * maps_layer_default_dir()
{
	static char * defaultdir = NULL;
	if (!defaultdir) {
		/* Thanks to Mike Davison for the $VIKING_MAPS usage. */
		const char * mapdir = g_getenv("VIKING_MAPS");
		if (mapdir) {
			defaultdir = g_strdup(mapdir);
		} else if (access(GLOBAL_MAPS_DIR, W_OK) == 0) {
			defaultdir = g_strdup(GLOBAL_MAPS_DIR);
		} else {
			const char *home = g_get_home_dir();
			if (!home || access(home, W_OK)) {
				home = g_get_home_dir();
			}

			if (home) {
				defaultdir = g_build_filename(home, LOCAL_MAPS_DIR, NULL);
			} else {
				defaultdir = g_strdup(LOCAL_MAPS_DIR);
			}
		}
		if (defaultdir && (defaultdir[strlen(defaultdir)-1] != G_DIR_SEPARATOR)) {
			/* Add the separator at the end. */
			char *tmp = defaultdir;
			defaultdir = g_strconcat(tmp, G_DIR_SEPARATOR_S, NULL);
			free(tmp);
		}
		fprintf(stderr, "DEBUG: %s: defaultdir=%s\n", __FUNCTION__, defaultdir);
	}
	return defaultdir;
}




std::string& maps_layer_default_dir_2()
{
	static char * defaultdir = NULL;
	static std::string default_dir;
	if (!defaultdir) {
		defaultdir = maps_layer_default_dir();
		default_dir = std::string(defaultdir);
	}

	return default_dir;
}




void LayerMaps::mkdir_if_default_dir()
{
	if (this->cache_dir && strcmp(this->cache_dir, MAPS_CACHE_DIR) == 0 && g_file_test(this->cache_dir, G_FILE_TEST_EXISTS) == false) {
		if (g_mkdir(this->cache_dir, 0777) != 0) {
			fprintf(stderr, "WARNING: %s: Failed to create directory %s\n", __FUNCTION__, this->cache_dir);
		}
	}
}




void LayerMaps::set_cache_dir(char const * dir)
{
	free(this->cache_dir);
	this->cache_dir = NULL;

	char const * mydir = dir;
	if (dir == NULL || dir[0] == '\0') {
		if (a_preferences_get(VIKING_PREFERENCES_NAMESPACE "maplayer_default_dir")) {
			mydir = a_preferences_get(VIKING_PREFERENCES_NAMESPACE "maplayer_default_dir")->s;
		}
	}

	char * canonical_dir = vu_get_canonical_filename(this, mydir);

	/* Ensure cache_dir always ends with a separator. */
	unsigned int len = strlen(canonical_dir);
	/* Unless the dir is not valid. */
	if (len > 0) {
		if (canonical_dir[len-1] != G_DIR_SEPARATOR) {
			this->cache_dir = g_strconcat(canonical_dir, G_DIR_SEPARATOR_S, NULL);
			free(canonical_dir);
		} else {
			this->cache_dir = canonical_dir;
		}

		this->mkdir_if_default_dir();
	}
}




void LayerMaps::set_file(char const * name)
{
	if (this->filename) {
		free(this->filename);
	}

	this->filename = g_strdup(name);
}




/****************************************/
/************** PARAMETERS **************/
/****************************************/

static MapTypeID map_index_to_map_type(unsigned int index)
{
	assert (index < map_sources.size());
	return map_sources[index]->map_type;
}




static unsigned int map_type_to_map_index(MapTypeID map_type)
{
	for (unsigned int i = 0; i < map_sources.size(); i++) {
		if (map_sources[i]->map_type == map_type) {
			return i;
		}
	}
	return map_sources.size(); /* no such thing */
}




#define VIK_SETTINGS_MAP_LICENSE_SHOWN "map_license_shown"

/**
 * Convenience function to display the license.
 */
static void maps_show_license(GtkWindow *parent, MapSource *map)
{
	a_dialog_license(parent,
			 map->get_label(),
			 map->get_license(),
			 map->get_license_url());
}




bool LayerMaps::set_param_value(uint16_t id, LayerParamData data, Viewport * viewport, bool is_file_operation)
{
	switch (id) {
	case PARAM_CACHE_DIR:
		this->set_cache_dir(data.s);
		break;
	case PARAM_CACHE_LAYOUT:
		if ((MapsCacheLayout) data.u < MapsCacheLayout::NUM) {
			this->cache_layout = (MapsCacheLayout) data.u;
		}
		break;
	case PARAM_FILE:
		this->set_file(data.s);
		break;
	case PARAM_MAPTYPE: {
		unsigned int map_index = map_type_to_map_index((MapTypeID) data.u);
		if (map_index == map_sources.size()) {
			fprintf(stderr, _("WARNING: Unknown map type\n"));
		} else {
			this->map_index = map_index;

			/* When loading from a file don't need the license reminder - ensure it's saved into the 'seen' list. */
			if (is_file_operation) {
				a_settings_set_integer_list_containing(VIK_SETTINGS_MAP_LICENSE_SHOWN, data.u);
			} else {
				MapSource * map = map_sources[this->map_index];
				if (map->get_license() != NULL) {
					/* Check if licence for this map type has been shown before. */
					if (! a_settings_get_integer_list_contains(VIK_SETTINGS_MAP_LICENSE_SHOWN, data.u)) {
						if (viewport) {
							maps_show_license(viewport->get_toolkit_window(), map);
						}
						a_settings_set_integer_list_containing(VIK_SETTINGS_MAP_LICENSE_SHOWN, data.u);
					}
				}
			}
			}
		break;
	}
	case PARAM_ALPHA:
		if (data.u <= 255) {
			this->alpha = data.u;
		}
		break;
	case PARAM_AUTODOWNLOAD:
		this->autodownload = data.b;
		break;
	case PARAM_ONLYMISSING:
		this->adl_only_missing = data.b;
		break;
	case PARAM_MAPZOOM:
		if (data.u < NUM_MAPZOOMS) {
			this->mapzoom_id = data.u;
			this->xmapzoom = __mapzooms_x[data.u];
			this->ymapzoom = __mapzooms_y[data.u];
		} else {
			fprintf(stderr, _("WARNING: Unknown Map Zoom\n"));
		}
		break;
	default:
		break;
	}
	return true;
}




LayerParamData LayerMaps::get_param_value(uint16_t id, bool is_file_operation) const
{
	LayerParamData rv;
	switch (id) {
	case PARAM_CACHE_DIR: {
		bool set = false;
		/* Only save a blank when the map cache location equals the default.
		   On reading in, when it is blank then the default is reconstructed.
		   Since the default changes dependent on the user and OS, it means the resultant file is more portable. */
		if (is_file_operation && this->cache_dir && strcmp(this->cache_dir, MAPS_CACHE_DIR) == 0) {
			rv.s = "";
			set = true;
		} else if (is_file_operation && this->cache_dir) {
			if (a_vik_get_file_ref_format() == VIK_FILE_REF_FORMAT_RELATIVE) {
				char *cwd = g_get_current_dir();
				if (cwd) {
					rv.s = file_GetRelativeFilename(cwd, this->cache_dir);
					if (!rv.s) rv.s = "";
					set = true;
				}
			}
		}

		if (!set) {
			rv.s = this->cache_dir ? this->cache_dir : "";
		}
		break;
	}
	case PARAM_CACHE_LAYOUT:
		rv.u = (int32_t) this->cache_layout;
		break;
	case PARAM_FILE:
		rv.s = this->filename;
		break;
	case PARAM_MAPTYPE:
		rv.u = map_index_to_map_type(this->map_index);
		break;
	case PARAM_ALPHA:
		rv.u = this->alpha;
		break;
	case PARAM_AUTODOWNLOAD:
		rv.u = this->autodownload;
		break;
	case PARAM_ONLYMISSING:
		rv.u = this->adl_only_missing;
		break;
	case PARAM_MAPZOOM:
		rv.u = this->mapzoom_id;
		break;
	default: break;
	}
	return rv;
}




static void maps_layer_change_param(GtkWidget *widget, ui_change_values * values)
{
	switch (values->param_id) {
		/* Alter sensitivity of download option widgets according to the map_index setting. */
	case PARAM_MAPTYPE: {
		/* Get new value. */
		LayerParamData vlpd = a_uibuilder_widget_get_value(widget, values->param);
		/* Is it *not* the OSM On Disk Tile Layout or the MBTiles type or the OSM Metatiles type. */
		bool sensitive = (MAP_ID_OSM_ON_DISK != vlpd.u &&
				  MAP_ID_MBTILES != vlpd.u &&
				  MAP_ID_OSM_METATILES != vlpd.u);
		GtkWidget **ww1 = values->widgets;
		GtkWidget **ww2 = values->labels;
		GtkWidget *w1 = ww1[PARAM_ONLYMISSING];
		GtkWidget *w2 = ww2[PARAM_ONLYMISSING];
		GtkWidget *w3 = ww1[PARAM_AUTODOWNLOAD];
		GtkWidget *w4 = ww2[PARAM_AUTODOWNLOAD];
		/* Depends on autodownload value. */
		LayerMaps * layer = (LayerMaps *) values->layer;
		bool missing_sense = sensitive && layer->autodownload;
		if (w1) {
			gtk_widget_set_sensitive(w1, missing_sense);
		}

		if (w2) {
			gtk_widget_set_sensitive(w2, missing_sense);
		}

		if (w3) {
			gtk_widget_set_sensitive(w3, sensitive);
		}

		if (w4) {
			gtk_widget_set_sensitive(w4, sensitive);
		}

		/* Cache type not applicable either. */
		GtkWidget *w9 = ww1[PARAM_CACHE_LAYOUT];
		GtkWidget *w10 = ww2[PARAM_CACHE_LAYOUT];
		if (w9) {
			gtk_widget_set_sensitive(w9, sensitive);
		}

		if (w10) {
			gtk_widget_set_sensitive(w10, sensitive);
		}

		/* File only applicable for MBTiles type.
		   Directory for all other types. */
		sensitive = (MAP_ID_MBTILES == vlpd.u);
		GtkWidget *w5 = ww1[PARAM_FILE];
		GtkWidget *w6 = ww2[PARAM_FILE];
		GtkWidget *w7 = ww1[PARAM_CACHE_DIR];
		GtkWidget *w8 = ww2[PARAM_CACHE_DIR];
		if (w5) {
			gtk_widget_set_sensitive(w5, sensitive);
		}

		if (w6) {
			gtk_widget_set_sensitive(w6, sensitive);
		}

		if (w7) {
			gtk_widget_set_sensitive(w7, !sensitive);
		}

		if (w8) {
			gtk_widget_set_sensitive(w8, !sensitive);
		}

		break;
	}

		/* Alter sensitivity of 'download only missing' widgets according to the autodownload setting. */
	case PARAM_AUTODOWNLOAD: {
		/* Get new value. */
		LayerParamData vlpd = a_uibuilder_widget_get_value(widget, values->param);
		GtkWidget **ww1 = values->widgets;
		GtkWidget **ww2 = values->labels;
		GtkWidget *w1 = ww1[PARAM_ONLYMISSING];
		GtkWidget *w2 = ww2[PARAM_ONLYMISSING];
		if (w1) {
			gtk_widget_set_sensitive(w1, vlpd.b);
		}

		if (w2) {
			gtk_widget_set_sensitive(w2, vlpd.b);
		}
		break;
	}
	default: break;
	}
}




/****************************************/
/****** CREATING, COPYING, FREEING ******/
/****************************************/
LayerMaps::~LayerMaps()
{
	free(this->cache_dir);
	this->cache_dir = NULL;
	if (this->dl_right_click_menu) {
		g_object_ref_sink(G_OBJECT(this->dl_right_click_menu));
	}

	free(this->last_center);
	this->last_center = NULL;
	free(this->filename);
	this->filename = NULL;

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




void LayerMaps::post_read(Viewport * viewport, bool from_file)
{
	MapSource * map = map_sources[this->map_index];

	if (!from_file) {
		/* If this method is not called in file reading context it is called in GUI context.
		   So, we can check if we have to inform the user about inconsistency. */
		VikViewportDrawMode vp_drawmode;
		vp_drawmode = viewport->get_drawmode();

		if (map->get_drawmode() != vp_drawmode) {
			const char *drawmode_name = viewport->get_drawmode_name(map->get_drawmode());
			char *msg = g_strdup_printf(_("New map cannot be displayed in the current drawmode.\nSelect \"%s\" from View menu to view it."), drawmode_name);
			a_dialog_warning_msg(viewport->get_toolkit_window(), msg);
			free(msg);
		}
	}

	/* Performed in post read as we now know the map type. */
#ifdef HAVE_SQLITE3_H
	/* Do some SQL stuff. */
	if (map->is_mbtiles()) {
		int ans = sqlite3_open_v2(this->filename,
					  &(this->mbtiles),
					  SQLITE_OPEN_READONLY,
					  NULL);
		if (ans != SQLITE_OK) {
			/* That didn't work, so here's why: */
			fprintf(stderr, "WARNING: %s: %s\n", __FUNCTION__, sqlite3_errmsg(this->mbtiles));

			a_dialog_error_msg_extra(viewport->get_toolkit_window(),
						 _("Failed to open MBTiles file: %s"),
						 this->filename);
			this->mbtiles = NULL;
		}
	}
#endif

	/* If the on Disk OSM Tile Layout type. */
	if (map->map_type == MAP_ID_OSM_ON_DISK) {
		/* Copy the directory into filename.
		   Thus the mapcache look up will be unique when using more than one of these map types. */
		free(this->filename);
		this->filename = g_strdup(this->cache_dir);
	}
}




char const * LayerMaps::tooltip()
{
	return this->get_map_label();
}




static Layer * maps_layer_unmarshall(uint8_t * data, int len, Viewport * viewport)
{
	LayerMaps * layer = new LayerMaps();

	layer->unmarshall_params(data, len, viewport);
	layer->post_read(viewport, false);
	return layer;
}




/*********************/
/****** DRAWING ******/
/*********************/

static GdkPixbuf * get_pixbuf_from_file(LayerMaps * layer, char * filename_buf);

static GdkPixbuf *pixbuf_shrink(GdkPixbuf *pixbuf, double xshrinkfactor, double yshrinkfactor)
{
	uint16_t width = gdk_pixbuf_get_width(pixbuf), height = gdk_pixbuf_get_height(pixbuf);
	GdkPixbuf * tmp = gdk_pixbuf_scale_simple(pixbuf, ceil(width * xshrinkfactor), ceil(height * yshrinkfactor), GDK_INTERP_BILINEAR);
	g_object_unref(G_OBJECT(pixbuf));
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




static GdkPixbuf *get_pixbuf_sql_exec(sqlite3 *sql, int xx, int yy, int zoom)
{
	GdkPixbuf *pixbuf = NULL;

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
					/* Convert these blob bytes into a pixbuf via these streaming operations. */
					GInputStream *stream = g_memory_input_stream_new_from_data(data, bytes, NULL);
					GError *error = NULL;
					pixbuf = gdk_pixbuf_new_from_stream(stream, NULL, &error);
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

	return pixbuf;
}
#endif




static GdkPixbuf *get_mbtiles_pixbuf(LayerMaps * layer, int xx, int yy, int zoom)
{
	GdkPixbuf *pixbuf = NULL;

#ifdef HAVE_SQLITE3_H
	if (layer->mbtiles) {
		/*
		  char *statement = g_strdup_printf("SELECT name FROM sqlite_master WHERE type='table';");
		  char *errMsg = NULL;
		  int ans = sqlite3_exec(layer->mbtiles, statement, sql_select_tile_dump_cb, pixbuf, &errMsg);
		  if (ans != SQLITE_OK) {
		  // Only to console for information purposes only
		  fprintf(stderr, "WARNING: SQL problem: %d for %s - error: %s\n", ans, statement, errMsg);
		  sqlite3_free(errMsg);
		  }
		  free(statement);
		*/

		/* Reading BLOBS is a bit more involved and so can't use the simpler sqlite3_exec().
		   Hence this specific function. */
		pixbuf = get_pixbuf_sql_exec(layer->mbtiles, xx, yy, zoom);
	}
#endif

	return pixbuf;
}




static GdkPixbuf *get_pixbuf_from_metatile(LayerMaps * layer, int xx, int yy, int zz)
{
	const int tile_max = METATILE_MAX_SIZE;
	char err_msg[PATH_MAX];
	int compressed;

	char * buf = (char *) malloc(tile_max);
	if (!buf) {
		return NULL;
	}

	err_msg[0] = 0;
	int len = metatile_read(layer->cache_dir, xx, yy, zz, buf, tile_max, &compressed, err_msg);

	if (len > 0) {
		if (compressed) {
			/* Not handled yet - I don't think this is used often - so implement later if necessary. */
			fprintf(stderr, "WARNING: Compressed metatiles not implemented:%s\n", __FUNCTION__);
			free(buf);
			return NULL;
		}

		/* Convert these buf bytes into a pixbuf via these streaming operations. */
		GdkPixbuf *pixbuf = NULL;

		GInputStream *stream = g_memory_input_stream_new_from_data(buf, len, NULL);
		GError *error = NULL;
		pixbuf = gdk_pixbuf_new_from_stream(stream, NULL, &error);
		if (error) {
			fprintf(stderr, "WARNING: %s: %s", __FUNCTION__, error->message);
			g_error_free(error);
		}
		g_input_stream_close(stream, NULL, NULL);

		free(buf);
		return pixbuf;
	} else {
		free(buf);
		fprintf(stderr, "WARNING: FAILED:%s %s", __FUNCTION__, err_msg);
		return NULL;
	}
}




/**
 * Caller has to decrease reference counter of returned.
 * GdkPixbuf, when buffer is no longer needed.
 */
static GdkPixbuf * pixbuf_apply_settings(GdkPixbuf * pixbuf, uint8_t alpha, double xshrinkfactor, double yshrinkfactor)
{
	/* Apply alpha setting. */
	if (pixbuf && alpha < 255) {
		pixbuf = ui_pixbuf_set_alpha(pixbuf, alpha);
	}

	if (pixbuf && (xshrinkfactor != 1.0 || yshrinkfactor != 1.0)) {
		pixbuf = pixbuf_shrink(pixbuf, xshrinkfactor, yshrinkfactor);
	}

	return pixbuf;
}




static void get_cache_filename(const char *cache_dir,
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
			if (g_strcmp0(cache_dir, MAPS_CACHE_DIR)) {
				/* Cache dir not the default - assume it's been directed somewhere specific. */
				snprintf(filename_buf, buf_len, DIRECTDIRACCESS, cache_dir, (17 - coord->scale), coord->x, coord->y, file_extension);
			} else {
				/* Using default cache - so use the map name in the directory path. */
				snprintf(filename_buf, buf_len, DIRECTDIRACCESS_WITH_NAME, cache_dir, name, (17 - coord->scale), coord->x, coord->y, file_extension);
			}
		} else {
			snprintf(filename_buf, buf_len, DIRECTDIRACCESS, cache_dir, (17 - coord->scale), coord->x, coord->y, file_extension);
		}
		break;
	default:
		snprintf(filename_buf, buf_len, DIRSTRUCTURE, cache_dir, id, coord->scale, coord->z, coord->x, coord->y);
		break;
	}
}




/**
 * Caller has to decrease reference counter of returned.
 * GdkPixbuf, when buffer is no longer needed.
 */
static GdkPixbuf * get_pixbuf(LayerMaps * layer, MapTypeID map_type, const char* mapname, TileInfo *mapcoord, char *filename_buf, int buf_len, double xshrinkfactor, double yshrinkfactor)
{
	/* Get the thing. */
	GdkPixbuf * pixbuf = map_cache_get(mapcoord, map_type, layer->alpha, xshrinkfactor, yshrinkfactor, layer->filename);
	if (pixbuf) {
		//fprintf(stderr, "MapsLayer: MAP CACHE HIT\n");
	} else {
		//fprintf(stderr, "MapsLayer: MAP CACHE MISS\n");
		MapSource * map = map_sources[layer->map_index];
		if (map->is_direct_file_access()) {
			/* ATM MBTiles must be 'a direct access type'. */
			if (map->is_mbtiles()) {
				pixbuf = get_mbtiles_pixbuf(layer, mapcoord->x, mapcoord->y, (17 - mapcoord->scale));
			} else if (map->is_osm_meta_tiles()) {
				pixbuf = get_pixbuf_from_metatile(layer, mapcoord->x, mapcoord->y, (17 - mapcoord->scale));
			} else {
				get_cache_filename(layer->cache_dir, MapsCacheLayout::OSM, map_type, NULL,
					     mapcoord, filename_buf, buf_len,
					     map->get_file_extension());
				pixbuf = get_pixbuf_from_file(layer, filename_buf);
			}
		} else {
			get_cache_filename(layer->cache_dir, layer->cache_layout, map_type, mapname,
				     mapcoord, filename_buf, buf_len,
				     map->get_file_extension());
			pixbuf = get_pixbuf_from_file(layer, filename_buf);
		}

		if (pixbuf) {
			pixbuf = pixbuf_apply_settings(pixbuf, layer->alpha, xshrinkfactor, yshrinkfactor);

			map_cache_add(pixbuf, (map_cache_extra_t) {0.0}, mapcoord, map_sources[layer->map_index]->map_type,
				      layer->alpha, xshrinkfactor, yshrinkfactor, layer->filename);
		}
	}
	return pixbuf;
}




static GdkPixbuf * get_pixbuf_from_file(LayerMaps * layer, char * filename_buf)
{
	GdkPixbuf * pixbuf = NULL;

	if (g_file_test(filename_buf, G_FILE_TEST_EXISTS) == true) {
		GError * gx = NULL;
		pixbuf = gdk_pixbuf_new_from_file(filename_buf, &gx);

		/* Free the pixbuf on error. */
		if (gx) {
			if (gx->domain != GDK_PIXBUF_ERROR || gx->code != GDK_PIXBUF_ERROR_CORRUPT_IMAGE) {
				/* Report a warning. */
				Window * w = layer->get_window();
				if (w) {
					char * msg = g_strdup_printf(_("Couldn't open image file: %s"), gx->message);
					w->statusbar_update(msg, VIK_STATUSBAR_INFO);
					free(msg);
				}
			}

			g_error_free(gx);
			if (pixbuf) {
				g_object_unref(G_OBJECT(pixbuf));
			}
			pixbuf = NULL;
		}
	}

	return pixbuf;
}





static bool should_start_autodownload(LayerMaps * layer, Viewport * viewport)
{
	const VikCoord *center = viewport->get_center();

	if (viewport->get_window()->get_pan_move()) {
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
		VikCoord *new_center = (VikCoord *) malloc(sizeof(VikCoord));
		*new_center = *center; /* KamilFIXME: can we do it this way? */
		layer->last_center = new_center;
		layer->last_xmpp = viewport->get_xmpp();
		layer->last_ympp = viewport->get_ympp();
		return true;
	}

	/* TODO: perhaps vik_coord_diff() */
	if (vik_coord_equals(layer->last_center, center)
	    && (layer->last_xmpp == viewport->get_xmpp())
	    && (layer->last_ympp == viewport->get_ympp())) {
		return false;
	}

	*(layer->last_center) = *center;
	layer->last_xmpp = viewport->get_xmpp();
	layer->last_ympp = viewport->get_ympp();
	return true;
}




bool try_draw_scale_down(LayerMaps * layer, Viewport * viewport, TileInfo ulm, int xx, int yy, int tilesize_x_ceil, int tilesize_y_ceil,
			 double xshrinkfactor, double yshrinkfactor, MapTypeID map_type, const char *mapname, char *path_buf, unsigned int max_path_len)
{
	for (unsigned int scale_inc = 1; scale_inc < SCALE_INC_DOWN; scale_inc++) {
		/* Try with smaller zooms. */
		int scale_factor = 1 << scale_inc;  /* 2^scale_inc */
		TileInfo ulm2 = ulm;
		ulm2.x = ulm.x / scale_factor;
		ulm2.y = ulm.y / scale_factor;
		ulm2.scale = ulm.scale + scale_inc;
		GdkPixbuf * pixbuf = get_pixbuf(layer, map_type, mapname, &ulm2, path_buf, max_path_len, xshrinkfactor * scale_factor, yshrinkfactor * scale_factor);
		if (pixbuf) {
			int src_x = (ulm.x % scale_factor) * tilesize_x_ceil;
			int src_y = (ulm.y % scale_factor) * tilesize_y_ceil;
			viewport->draw_pixbuf(pixbuf, src_x, src_y, xx, yy, tilesize_x_ceil, tilesize_y_ceil);
			g_object_unref(pixbuf);
			return true;
		}
	}
	return false;
}




bool try_draw_scale_up(LayerMaps * layer, Viewport * viewport, TileInfo ulm, int xx, int yy, int tilesize_x_ceil, int tilesize_y_ceil,
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
				GdkPixbuf * pixbuf = get_pixbuf(layer, map_type, mapname, &ulm3, path_buf, max_path_len, xshrinkfactor / scale_factor, yshrinkfactor / scale_factor);
				if (pixbuf) {
					int src_x = 0;
					int src_y = 0;
					int dest_x = xx + pict_x * (tilesize_x_ceil / scale_factor);
					int dest_y = yy + pict_y * (tilesize_y_ceil / scale_factor);
					viewport->draw_pixbuf(pixbuf, src_x, src_y, dest_x, dest_y, tilesize_x_ceil / scale_factor, tilesize_y_ceil / scale_factor);
					g_object_unref(pixbuf);
					return true;
				}
			}
		}
	}
	return false;
}




void LayerMaps::draw_section(Viewport * viewport, VikCoord *ul, VikCoord *br)
{
	double xzoom = viewport->get_xmpp();
	double yzoom = viewport->get_ympp();
	double xshrinkfactor = 1.0, yshrinkfactor = 1.0;
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
					char * msg = g_strdup_printf(_("Cowardly refusing to draw tiles or existence of tiles beyond %d zoom out factor"), (int)(1.0/REAL_MIN_SHRINKFACTOR));
					w->statusbar_update(msg, VIK_STATUSBAR_INFO);
					free(msg);
				}
				return;
			}
		}
	}

	/* coord -> ID */
	TileInfo ulm, brm;
	MapSource *map = (MapSource *) map_sources[this->map_index];
	if (map->coord_to_tile(ul, xzoom, yzoom, &ulm) &&
	     map->coord_to_tile(br, xzoom, yzoom, &brm)) {

		/* Loop & draw. */
		//int x, y;
		int xmin = MIN(ulm.x, brm.x), xmax = MAX(ulm.x, brm.x);
		int ymin = MIN(ulm.y, brm.y), ymax = MAX(ulm.y, brm.y);
		MapTypeID map_type = map->map_type;
		const char *mapname = map->get_name();

		VikCoord coord;
		int xx, yy, width, height;
		GdkPixbuf *pixbuf;

		/* Prevent the program grinding to a halt if trying to deal with thousands of tiles
		   which can happen when using a small fixed zoom level and viewing large areas.
		   Also prevents very large number of tile download requests. */
		int tiles = (xmax-xmin) * (ymax-ymin);
		if (tiles > MAX_TILES) {
			fprintf(stderr, "DEBUG: %s: existence_only due to wanting too many tiles (%d)", __FUNCTION__, tiles);
			existence_only = true;
		}

		unsigned int max_path_len = strlen(this->cache_dir) + 40;
		char *path_buf = (char *) malloc(max_path_len * sizeof(char));

		if ((!existence_only) && this->autodownload  && should_start_autodownload(this, viewport)) {
			fprintf(stderr, "DEBUG: %s: Starting autodownload", __FUNCTION__);
			if (!this->adl_only_missing && map->supports_download_only_new()) {
				/* Try to download newer tiles. */
				start_download_thread(this, viewport, ul, br, REDOWNLOAD_NEW);
			} else {
				/* Download only missing tiles. */
				start_download_thread(this, viewport, ul, br, REDOWNLOAD_NONE);
			}
		}

		if (map->get_tilesize_x() == 0 && !existence_only) {
			for (int x = xmin; x <= xmax; x++) {
				for (int y = ymin; y <= ymax; y++) {
					ulm.x = x;
					ulm.y = y;
					pixbuf = get_pixbuf(this, map_type, mapname, &ulm, path_buf, max_path_len, xshrinkfactor, yshrinkfactor);
					if (pixbuf) {
						width = gdk_pixbuf_get_width (pixbuf);
						height = gdk_pixbuf_get_height(pixbuf);

						map->tile_to_center_coord(&ulm, &coord);
						viewport->coord_to_screen(&coord, &xx, &yy);
						xx -= (width/2);
						yy -= (height/2);

						viewport->draw_pixbuf(pixbuf, 0, 0, xx, yy, width, height);
						g_object_unref(pixbuf);
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

			map->tile_to_center_coord(&ulm, &coord);
			viewport->coord_to_screen(&coord, &xx_tmp, &yy_tmp);
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

						if (g_file_test(path_buf, G_FILE_TEST_EXISTS) == true) {
							GdkGC *black_gc = gtk_widget_get_style(viewport->get_toolkit_widget())->black_gc;
							viewport->draw_line(black_gc, xx+tilesize_x_ceil, yy, xx, yy+tilesize_y_ceil);
						}
					} else {
						/* Try correct scale first. */
						int scale_factor = 1;
						pixbuf = get_pixbuf(this, map_type, mapname, &ulm, path_buf, max_path_len, xshrinkfactor * scale_factor, yshrinkfactor * scale_factor);
						if (pixbuf) {
							int src_x = (ulm.x % scale_factor) * tilesize_x_ceil;
							int src_y = (ulm.y % scale_factor) * tilesize_y_ceil;
							viewport->draw_pixbuf(pixbuf, src_x, src_y, xx, yy, tilesize_x_ceil, tilesize_y_ceil);
							g_object_unref(pixbuf);
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
				GdkGC *black_gc = viewport->get_toolkit_widget()->style->black_gc;
				/* Draw single grid lines across the whole screen. */
				int width = viewport->get_width();
				int height = viewport->get_height();
				xx = xx_tmp; yy = yy_tmp;
				int base_xx = xx - (tilesize_x/2);
				base_yy = yy - (tilesize_y/2);

				xx = base_xx;
				for (int x = ((xinc == 1) ? xmin : xmax); x != xend; x+=xinc) {
					viewport->draw_line(black_gc, xx, base_yy, xx, height);
					xx += tilesize_x;
				}

				yy = base_yy;
				for (int y = ((yinc == 1) ? ymin : ymax); y != yend; y+=yinc) {
					viewport->draw_line(black_gc, base_xx, yy, width, yy);
					yy += tilesize_y;
				}
			}

		}
		free(path_buf);
	}
}




void LayerMaps::draw(Viewport * viewport)
{
	if (map_sources[this->map_index]->get_drawmode() == viewport->get_drawmode()) {
		VikCoord ul, br;

		/* Copyright. */
		double level = viewport->get_zoom();
		LatLonBBox bbox;
		viewport->get_bbox(&bbox);
		map_sources[this->map_index]->get_copyright(bbox, level, vik_viewport_add_copyright_cb, viewport);

		/* Logo. */
		const GdkPixbuf *logo = map_sources[this->map_index]->get_logo();
		viewport->add_logo(logo);

		/* Get corner coords. */
		if (viewport->get_coord_mode() == VIK_COORD_UTM && ! viewport->is_one_zone()) {
			/* UTM multi-zone stuff by Kit Transue. */
			char leftmost_zone = viewport->leftmost_zone();
			char rightmost_zone = viewport->rightmost_zone();
			for (char i = leftmost_zone; i <= rightmost_zone; ++i) {
				viewport->corners_for_zonen(i, &ul, &br);
				this->draw_section(viewport, &ul, &br);
			}
		} else {
			viewport->screen_to_coord(0, 0, &ul);
			viewport->screen_to_coord(viewport->get_width(), viewport->get_height(), &br);

			this->draw_section(viewport, &ul, &br);
		}
	}
}




/*************************/
/****** DOWNLOADING ******/
/*************************/

/* Pass along data to thread, exists even if layer is deleted. */
typedef struct {
	char *cache_dir;
	char *filename_buf;
	MapsCacheLayout cache_layout;
	int x0, y0, xf, yf;
	TileInfo mapcoord;
	int map_index;
	int maxlen;
	int mapstoget;
	int redownload_mode;
	bool refresh_display;
	LayerMaps * layer;
	bool map_layer_alive;
	GMutex *mutex;
} MapDownloadInfo;

static MapDownloadInfo * mdi_new(LayerMaps * layer, TileInfo * ulm, TileInfo * brm, bool refresh_display, int redownload_mode);
static char * redownload_mode_message(int redownload_mode, int mapstoget, char * label);
static void mdi_calculate_mapstoget(MapDownloadInfo * mdi, MapSource * map, TileInfo * ulm);
static void mdi_calculate_mapstoget_other(MapDownloadInfo * mdi, MapSource * map, TileInfo * ulm);




static void mdi_free(MapDownloadInfo *mdi)
{
	vik_mutex_free(mdi->mutex);
	free(mdi->cache_dir);
	mdi->cache_dir = NULL;
	free(mdi->filename_buf);
	mdi->filename_buf = NULL;
	free(mdi);
}




void LayerMaps::weak_ref_cb(void * ptr, GObject * dead_vml)
{
	MapDownloadInfo * mdi = (MapDownloadInfo *) ptr;
	g_mutex_lock(mdi->mutex);
	mdi->map_layer_alive = false;
	g_mutex_unlock(mdi->mutex);
}




static bool is_in_area(MapSource * map, TileInfo * mc)
{
	VikCoord vc;
	map->tile_to_center_coord(mc, &vc);

	struct LatLon tl;
	tl.lat = map->get_lat_max();
	tl.lon = map->get_lon_min();
	struct LatLon br;
	br.lat = map->get_lat_min();
	br.lon = map->get_lon_max();
	VikCoord vctl;
	vik_coord_load_from_latlon(&vctl, VIK_COORD_LATLON, &tl);
	VikCoord vcbr;
	vik_coord_load_from_latlon(&vcbr, VIK_COORD_LATLON, &br);

	return vik_coord_inside(&vc, &vctl, &vcbr);
}




static int map_download_thread(MapDownloadInfo *mdi, void * threaddata)
{
	void *handle = map_sources[mdi->map_index]->download_handle_init();
	unsigned int donemaps = 0;
	TileInfo mcoord = mdi->mapcoord;

	for (mcoord.x = mdi->x0; mcoord.x <= mdi->xf; mcoord.x++) {
		for (mcoord.y = mdi->y0; mcoord.y <= mdi->yf; mcoord.y++) {
			/* Only attempt to download a tile from supported areas. */
			if (is_in_area(map_sources[mdi->map_index], &mcoord)) {
				bool remove_mem_cache = false;
				bool need_download = false;

				get_cache_filename(mdi->cache_dir, mdi->cache_layout,
					     map_sources[mdi->map_index]->map_type,
					     map_sources[mdi->map_index]->get_name(),
					     &mcoord, mdi->filename_buf, mdi->maxlen,
					     map_sources[mdi->map_index]->get_file_extension());

				donemaps++;
				int res = a_background_thread_progress(threaddata, ((double)donemaps) / mdi->mapstoget); /* this also calls testcancel */
				if (res != 0) {
					map_sources[mdi->map_index]->download_handle_cleanup(handle);
					return -1;
				}

				if (g_file_test(mdi->filename_buf, G_FILE_TEST_EXISTS) == false) {
					need_download = true;
					remove_mem_cache = true;

				} else {  /* In case map file already exists. */
					switch (mdi->redownload_mode) {
					case REDOWNLOAD_NONE:
						continue;

					case REDOWNLOAD_BAD: {
						/* See if this one is bad or what. */
						GError *gx = NULL;
						GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(mdi->filename_buf, &gx);
						if (gx || (!pixbuf)) {
							if (remove(mdi->filename_buf)) {
								fprintf(stderr, "WARNING: REDOWNLOAD failed to remove: %s", mdi->filename_buf);
							}
							need_download = true;
							remove_mem_cache = true;
							g_error_free(gx);

						} else {
							g_object_unref(pixbuf);
						}
						break;
					}

					case REDOWNLOAD_NEW:
						need_download = true;
						remove_mem_cache = true;
						break;

					case REDOWNLOAD_ALL:
						/* FIXME: need a better way than to erase file in case of server/network problem. */
						if (remove(mdi->filename_buf)) {
							fprintf(stderr, "WARNING: REDOWNLOAD failed to remove: %s", mdi->filename_buf);
						}
						need_download = true;
						remove_mem_cache = true;
						break;

					case DOWNLOAD_OR_REFRESH:
						remove_mem_cache = true;
						break;

					default:
						fprintf(stderr, "WARNING: redownload mode %d unknown\n", mdi->redownload_mode);
					}
				}

				mdi->mapcoord.x = mcoord.x;
				mdi->mapcoord.y = mcoord.y;

				if (need_download) {
					DownloadResult_t dr = map_sources[mdi->map_index]->download(&(mdi->mapcoord), mdi->filename_buf, handle);
					switch (dr) {
					case DOWNLOAD_HTTP_ERROR:
					case DOWNLOAD_CONTENT_ERROR: {
						/* TODO: ?? count up the number of download errors somehow... */
						char* msg = g_strdup_printf("%s: %s", mdi->layer->get_map_label(), _("Failed to download tile"));
						mdi->layer->get_window()->statusbar_update(msg, VIK_STATUSBAR_INFO);
						free(msg);
						break;
					}
					case DOWNLOAD_FILE_WRITE_ERROR: {
						char * msg = g_strdup_printf("%s: %s", mdi->layer->get_map_label(), _("Unable to save tile"));
						mdi->layer->get_window()->statusbar_update(msg, VIK_STATUSBAR_INFO);
						free(msg);
						break;
					}
					case DOWNLOAD_SUCCESS:
					case DOWNLOAD_NOT_REQUIRED:
					default:
						break;
					}
				}

				g_mutex_lock(mdi->mutex);
				if (remove_mem_cache) {
					map_cache_remove_all_shrinkfactors(&mcoord, map_sources[mdi->map_index]->map_type, mdi->layer->filename);
				}

				if (mdi->refresh_display && mdi->map_layer_alive) {
					/* TODO: check if it's on visible area. */
					mdi->layer->emit_update(); /* NB update display from background. */
				}
				g_mutex_unlock(mdi->mutex);
				mdi->mapcoord.x = mdi->mapcoord.y = 0; /* We're temporarily between downloads. */
			}
		}
	}
	map_sources[mdi->map_index]->download_handle_cleanup(handle);
	g_mutex_lock(mdi->mutex);
	if (mdi->map_layer_alive) {
		mdi->layer->weak_unref(LayerMaps::weak_ref_cb, mdi);
	}
	g_mutex_unlock(mdi->mutex);
	return 0;
}




static void mdi_cancel_cleanup(MapDownloadInfo *mdi)
{
	if (mdi->mapcoord.x || mdi->mapcoord.y) {
		get_cache_filename(mdi->cache_dir, mdi->cache_layout,
				   map_sources[mdi->map_index]->map_type,
				   map_sources[mdi->map_index]->get_name(),
				   &mdi->mapcoord, mdi->filename_buf, mdi->maxlen,
				   map_sources[mdi->map_index]->get_file_extension());
		if (g_file_test(mdi->filename_buf, G_FILE_TEST_EXISTS) == true) {
			if (remove(mdi->filename_buf)) {
				fprintf(stderr, "WARNING: Cleanup failed to remove: %s", mdi->filename_buf);
			}
		}
	}
}




static void start_download_thread(LayerMaps * layer, Viewport * viewport, const VikCoord *ul, const VikCoord *br, int redownload_mode)
{
	double xzoom = layer->xmapzoom ? layer->xmapzoom : viewport->get_xmpp();
	double yzoom = layer->ymapzoom ? layer->ymapzoom : viewport->get_ympp();
	TileInfo ulm, brm;
	MapSource *map = map_sources[layer->map_index];

	/* Don't ever attempt download on direct access. */
	if (map->is_direct_file_access()) {
		return;
	}

	if (map->coord_to_tile(ul, xzoom, yzoom, &ulm)
	     && map->coord_to_tile(br, xzoom, yzoom, &brm)) {

		MapDownloadInfo * mdi = mdi_new(layer, &ulm, &brm, true, redownload_mode);

		if (mdi->redownload_mode) {
			mdi->mapstoget = (mdi->xf - mdi->x0 + 1) * (mdi->yf - mdi->y0 + 1);
		} else {
			mdi_calculate_mapstoget(mdi, map, &ulm);
		}

		/* For cleanup - no current map. */
		mdi->mapcoord.x = 0;
		mdi->mapcoord.y = 0;

		if (mdi->mapstoget) {
			char * msg = redownload_mode_message(redownload_mode, mdi->mapstoget, MAPS_LAYER_NTH_LABEL(layer->map_index));

			mdi->layer->weak_ref(LayerMaps::weak_ref_cb, mdi);
			/* Launch the thread */
			a_background_thread(BACKGROUND_POOL_REMOTE,
					    layer->get_toolkit_window(),            /* Parent window. */
					    msg,                                    /* Description string. */
					    (vik_thr_func) map_download_thread,     /* Function to call within thread. */
					    mdi,                                    /* Pass along data. */
					    (vik_thr_free_func) mdi_free,           /* Function to free pass along data. */
					    (vik_thr_free_func) mdi_cancel_cleanup,
					    mdi->mapstoget);
			free(msg);
		} else {
			mdi_free(mdi);
		}
	}
}




void LayerMaps::download_section_sub(VikCoord *ul, VikCoord *br, double zoom, int redownload_mode)
{
	TileInfo ulm, brm;
	MapSource *map = map_sources[this->map_index];

	/* Don't ever attempt download on direct access. */
	if (map->is_direct_file_access()) {
		return;
	}

	if (!map->coord_to_tile(ul, zoom, zoom, &ulm)
	    || !map->coord_to_tile(br, zoom, zoom, &brm)) {
		fprintf(stderr, "WARNING: %s() coord_to_tile() failed", __PRETTY_FUNCTION__);
		return;
	}

	MapDownloadInfo * mdi = mdi_new(this, &ulm, &brm, true, redownload_mode);

	mdi_calculate_mapstoget(mdi, map, &ulm);

	/* For cleanup - no current map. */
	mdi->mapcoord.x = 0;
	mdi->mapcoord.y = 0;

	if (mdi->mapstoget) {
		char * msg = redownload_mode_message(redownload_mode, mdi->mapstoget, MAPS_LAYER_NTH_LABEL(this->map_index));

		mdi->layer->weak_ref(weak_ref_cb, mdi);

		/* Launch the thread. */
		a_background_thread(BACKGROUND_POOL_REMOTE,
				    this->get_toolkit_window(),             /* Parent window. */
				    msg,                                    /* Description string. */
				    (vik_thr_func) map_download_thread,     /* Function to call within thread. */
				    mdi,                                    /* Pass along data. */
				    (vik_thr_free_func) mdi_free,           /* Function to free pass along data. */
				    (vik_thr_free_func) mdi_cancel_cleanup,
				    mdi->mapstoget);
		free(msg);
	} else {
		mdi_free(mdi);
	}
}




/**
 * @ul:   Upper left coordinate of the area to be downloaded
 * @br:   Bottom right coordinate of the area to be downloaded
 * @zoom: The zoom level at which the maps are to be download
 *
 * Download a specified map area at a certain zoom level
 */
void LayerMaps::download_section(VikCoord * ul, VikCoord * br, double zoom)
{
	this->download_section_sub(ul, br, zoom, REDOWNLOAD_NONE);
}




static void maps_layer_redownload_bad(LayerMaps * layer)
{
	start_download_thread(layer, layer->redownload_viewport, &(layer->redownload_ul), &(layer->redownload_br), REDOWNLOAD_BAD);
}




static void maps_layer_redownload_all(LayerMaps * layer)
{
	start_download_thread(layer, layer->redownload_viewport, &(layer->redownload_ul), &(layer->redownload_br), REDOWNLOAD_ALL);
}




static void maps_layer_redownload_new(LayerMaps * layer)
{
	start_download_thread(layer, layer->redownload_viewport, &(layer->redownload_ul), &(layer->redownload_br), REDOWNLOAD_NEW);
}




#if !GLIB_CHECK_VERSION(2,26,0)
typedef struct stat GStatBuf;
#endif

/**
 * Display a simple dialog with information about this particular map tile
 */
static void maps_layer_tile_info(LayerMaps * layer)
{
	MapSource *map = map_sources[layer->map_index];

	double xzoom = layer->xmapzoom ? layer->xmapzoom : layer->redownload_viewport->get_xmpp();
	double yzoom = layer->ymapzoom ? layer->ymapzoom : layer->redownload_viewport->get_ympp();
	TileInfo ulm;

	if (!map->coord_to_tile(&(layer->redownload_ul), xzoom, yzoom, &ulm)) {
		return;
	}

	char *filename = NULL;
	char *source = NULL;

	if (map->is_direct_file_access()) {
		if (map->is_mbtiles()) {
			filename = g_strdup(layer->filename);
#ifdef HAVE_SQLITE3_H
			/* And whether to bother going into the SQL to check it's really there or not... */
			char *exists = NULL;
			int zoom = 17 - ulm.scale;
			if (layer->mbtiles) {
				GdkPixbuf *pixbuf = get_pixbuf_sql_exec(layer->mbtiles, ulm.x, ulm.y, zoom);
				if (pixbuf) {
					exists = strdup(_("YES"));
					g_object_unref(G_OBJECT(pixbuf));
				} else {
					exists = strdup(_("NO"));
				}
			} else {
				exists = strdup(_("NO"));
			}

			int flip_y = (int) pow(2, zoom)-1 - ulm.y;
			/* NB Also handles .jpg automatically due to pixbuf_new_from() support - although just print png for now. */
			source = g_strdup_printf("Source: %s (%d%s%d%s%d.%s %s)", filename, zoom, G_DIR_SEPARATOR_S, ulm.x, G_DIR_SEPARATOR_S, flip_y, "png", exists);
			free(exists);
#else
			source = strdup(_("Source: Not available"));
#endif
		} else if (map->is_osm_meta_tiles()) {
			char path[PATH_MAX];
			xyz_to_meta(path, sizeof(path), layer->cache_dir, ulm.x, ulm.y, 17-ulm.scale);
			source = g_strdup(path);
			filename = g_strdup(path);
		} else {
			unsigned int max_path_len = strlen(layer->cache_dir) + 40;
			filename = (char *) malloc(max_path_len * sizeof(char));
			get_cache_filename(layer->cache_dir, MapsCacheLayout::OSM,
				     map->map_type,
				     NULL,
				     &ulm, filename, max_path_len,
				     map->get_file_extension());
			source = g_strconcat("Source: file://", filename, NULL);
		}
	} else {
		unsigned int max_path_len = strlen(layer->cache_dir) + 40;
		filename = (char *) malloc(max_path_len * sizeof(char));
		get_cache_filename(layer->cache_dir, layer->cache_layout,
			     map->map_type,
			     map->get_name(),
			     &ulm, filename, max_path_len,
			     map->get_file_extension());
		source = g_markup_printf_escaped("Source: http://%s%s",
						   map->get_server_hostname(),
						   map->get_server_path(&ulm));
	}

	GArray *array = g_array_new(false, true, sizeof(char*));
	g_array_append_val(array, source);

	char *filemsg = NULL;
	char *timemsg = NULL;

	if (g_file_test(filename, G_FILE_TEST_EXISTS)) {
		filemsg = g_strconcat("Tile File: ", filename, NULL);
		/* Get some timestamp information of the tile. */
		GStatBuf stat_buf;
		if (g_stat(filename, &stat_buf) == 0) {
			char time_buf[64];
			strftime(time_buf, sizeof(time_buf), "%c", gmtime((const time_t *)&stat_buf.st_mtime));
			timemsg = g_strdup_printf(_("Tile File Timestamp: %s"), time_buf);
		} else {
			timemsg = strdup(_("Tile File Timestamp: Not Available"));
		}
		g_array_append_val(array, filemsg);
		g_array_append_val(array, timemsg);
	} else {
		filemsg = g_strdup_printf("Tile File: %s [Not Available]", filename);
		g_array_append_val(array, filemsg);
	}

	a_dialog_list(layer->get_toolkit_window(), _("Tile Information"), array, 5);
	g_array_free(array, false);

	free(timemsg);
	free(filemsg);
	free(source);
	free(filename);
}




static bool maps_layer_download_release(Layer * _layer, GdkEventButton *event, LayerTool * tool)
{
	if (!_layer || _layer->type != LayerType::MAPS) {
		return false;
	}

	LayerMaps * layer = (LayerMaps *) _layer;

	if (layer->dl_tool_x != -1 && layer->dl_tool_y != -1) {
		if (event->button == MouseButton::LEFT) {
			VikCoord ul, br;
			tool->viewport->screen_to_coord(MAX(0, MIN(event->x, layer->dl_tool_x)), MAX(0, MIN(event->y, layer->dl_tool_y)), &ul);
			tool->viewport->screen_to_coord(MIN(tool->viewport->get_width(), MAX(event->x, layer->dl_tool_x)), MIN(tool->viewport->get_height(), MAX (event->y, layer->dl_tool_y)), &br);
			start_download_thread(layer, tool->viewport, &ul, &br, DOWNLOAD_OR_REFRESH);
			layer->dl_tool_x = layer->dl_tool_y = -1;
			return true;
		} else {
			tool->viewport->screen_to_coord(MAX(0, MIN(event->x, layer->dl_tool_x)), MAX(0, MIN(event->y, layer->dl_tool_y)), &(layer->redownload_ul));
			tool->viewport->screen_to_coord(MIN(tool->viewport->get_width(), MAX(event->x, layer->dl_tool_x)), MIN(tool->viewport->get_height(), MAX (event->y, layer->dl_tool_y)), &(layer->redownload_br));

			layer->redownload_viewport = tool->viewport;

			layer->dl_tool_x = layer->dl_tool_y = -1;

			if (!layer->dl_right_click_menu) {
				GtkWidget *item;
				layer->dl_right_click_menu = GTK_MENU (gtk_menu_new());

				item = gtk_menu_item_new_with_mnemonic(_("Redownload _Bad Map(s)"));
				g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(maps_layer_redownload_bad), layer);
				gtk_menu_shell_append(GTK_MENU_SHELL(layer->dl_right_click_menu), item);

				item = gtk_menu_item_new_with_mnemonic(_("Redownload _New Map(s)"));
				g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(maps_layer_redownload_new), layer);
				gtk_menu_shell_append(GTK_MENU_SHELL(layer->dl_right_click_menu), item);

				item = gtk_menu_item_new_with_mnemonic(_("Redownload _All Map(s)"));
				g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(maps_layer_redownload_all), layer);
				gtk_menu_shell_append(GTK_MENU_SHELL(layer->dl_right_click_menu), item);

				item = gtk_image_menu_item_new_with_mnemonic(_("_Show Tile Information"));
				gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_INFO, GTK_ICON_SIZE_MENU));
				g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(maps_layer_tile_info), layer);
				gtk_menu_shell_append(GTK_MENU_SHELL(layer->dl_right_click_menu), item);
			}

			gtk_menu_popup(layer->dl_right_click_menu, NULL, NULL, NULL, NULL, event->button, event->time);
			gtk_widget_show_all(GTK_WIDGET(layer->dl_right_click_menu));
		}
	}
	return false;
}




static LayerTool * maps_layer_download_create(Window * window, Viewport * viewport)
{
	LayerTool * layer_tool = new LayerTool(window, viewport, LayerType::MAPS);

	maps_tools[0] = layer_tool;

	layer_tool->radioActionEntry.name = strdup("MapsDownload");
	layer_tool->radioActionEntry.stock_id = strdup("vik-icon-Maps Download");
	layer_tool->radioActionEntry.label = strdup(N_("_Maps Download"));
	layer_tool->radioActionEntry.accelerator = NULL;
	layer_tool->radioActionEntry.tooltip = strdup(N_("Maps Download"));
	layer_tool->radioActionEntry.value = 0;

	layer_tool->click = (VikToolMouseFunc) maps_layer_download_click;
	layer_tool->release = (VikToolMouseFunc) maps_layer_download_release;

	layer_tool->cursor_type = GDK_CURSOR_IS_PIXMAP;
	layer_tool->cursor_data = &cursor_mapdl_pixbuf;

	return layer_tool;
}




static bool maps_layer_download_click(Layer * _layer, GdkEventButton *event, LayerTool * tool)
{
	TileInfo tmp;
	if (!_layer || _layer->type != LayerType::MAPS) {
		return false;
	}

	LayerMaps * layer = (LayerMaps *) _layer;

	MapSource *map = map_sources[layer->map_index];
	if (map->get_drawmode() == tool->viewport->get_drawmode()
	    && map->coord_to_tile(tool->viewport->get_center(),
				  layer->xmapzoom ? layer->xmapzoom : tool->viewport->get_xmpp(),
				  layer->ymapzoom ? layer->ymapzoom : tool->viewport->get_ympp(),
				  &tmp)) {

		layer->dl_tool_x = event->x;
		layer->dl_tool_y = event->y;
		return true;
	}
	return false;
}




typedef struct {
	LayerMaps * layer;
	Viewport * viewport;
} menu_array_values;

static void download_onscreen_maps(menu_array_values * values, int redownload_mode)
{
	LayerMaps * layer = values->layer;
	Viewport * viewport = values->viewport;
	VikViewportDrawMode vp_drawmode = viewport->get_drawmode();

	double xzoom = layer->xmapzoom ? layer->xmapzoom : viewport->get_xmpp();
	double yzoom = layer->ymapzoom ? layer->ymapzoom : viewport->get_ympp();

	VikCoord ul, br;
	TileInfo ulm, brm;

	viewport->screen_to_coord(0, 0, &ul);
	viewport->screen_to_coord(viewport->get_width(), viewport->get_height(), &br);

	MapSource *map = map_sources[layer->map_index];
	if (map->get_drawmode() == vp_drawmode
	    && map->coord_to_tile(&ul, xzoom, yzoom, &ulm)
	    && map->coord_to_tile(&br, xzoom, yzoom, &brm)) {

		start_download_thread(layer, viewport, &ul, &br, redownload_mode);

	} else if (map->get_drawmode() != vp_drawmode) {
		const char * drawmode_name = viewport->get_drawmode_name(map->get_drawmode());
		char *err = g_strdup_printf(_("Wrong drawmode for this map.\nSelect \"%s\" from View menu and try again."), _(drawmode_name));
		a_dialog_error_msg(layer->get_toolkit_window(), err);
		free(err);
	} else {
		a_dialog_error_msg(layer->get_toolkit_window(), _("Wrong zoom level for this map."));
	}

}




static void maps_layer_download_missing_onscreen_maps(menu_array_values * values)
{
	download_onscreen_maps(values, REDOWNLOAD_NONE);
}




static void maps_layer_download_new_onscreen_maps(menu_array_values * values)
{
	download_onscreen_maps(values, REDOWNLOAD_NEW);
}




static void maps_layer_redownload_all_onscreen_maps(menu_array_values * values)
{
	download_onscreen_maps(values, REDOWNLOAD_ALL);
}




static void maps_layer_about(menu_array_values * values)
{
	LayerMaps * layer = (LayerMaps *) values->layer;

	MapSource * map = map_sources[layer->map_index];
	if (map->get_license()) {
		maps_show_license(layer->get_toolkit_window(), map);
	} else {
		a_dialog_info_msg(layer->get_toolkit_window(), map->get_label());
	}
}




/**
 * Copied from maps_layer_download_section but without the actual download and this returns a value
 */
int LayerMaps::how_many_maps(VikCoord *ul, VikCoord *br, double zoom, int redownload_mode)
{
	TileInfo ulm, brm;
	MapSource *map = map_sources[this->map_index];

	if (map->is_direct_file_access()) {
		return 0;
	}

	if (!map->coord_to_tile(ul, zoom, zoom, &ulm)
	    || !map->coord_to_tile(br, zoom, zoom, &brm)) {
		fprintf(stderr, "WARNING: %s() coord_to_tile() failed", __PRETTY_FUNCTION__);
		return 0;
	}

	MapDownloadInfo * mdi = mdi_new(this, &ulm, &brm, false, redownload_mode);

	if (mdi->redownload_mode == REDOWNLOAD_ALL) {
		mdi->mapstoget = (mdi->xf - mdi->x0 + 1) * (mdi->yf - mdi->y0 + 1);
	} else {
		mdi_calculate_mapstoget_other(mdi, map, &ulm);
	}

	int rv = mdi->mapstoget;

	mdi_free(mdi);

	return rv;
}




/**
 * This dialog is specific to the map layer, so it's here rather than in dialog.c
 */
bool maps_dialog_zoom_between(GtkWindow *parent,
			      char *title,
			      char *zoom_list[],
			      int default_zoom1,
			      int default_zoom2,
			      int *selected_zoom1,
			      int *selected_zoom2,
			      char *download_list[],
			      int default_download,
			      int *selected_download)
{
	GtkWidget *dialog = gtk_dialog_new_with_buttons(title,
							parent,
							(GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
							GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
							GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
							NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	GtkWidget *response_w = NULL;
#if GTK_CHECK_VERSION (2, 20, 0)
	response_w = gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
#endif
	GtkWidget *zoom_label1 = gtk_label_new(_("Zoom Start:"));
	GtkWidget *zoom_combo1 = vik_combo_box_text_new();
	char **s;
	for (s = zoom_list; *s; s++) {
		vik_combo_box_text_append(zoom_combo1, *s);
	}

	gtk_combo_box_set_active(GTK_COMBO_BOX(zoom_combo1), default_zoom1);

	GtkWidget *zoom_label2 = gtk_label_new(_("Zoom End:"));
	GtkWidget *zoom_combo2 = vik_combo_box_text_new();
	for (s = zoom_list; *s; s++) {
		vik_combo_box_text_append(zoom_combo2, *s);
	}

	gtk_combo_box_set_active(GTK_COMBO_BOX(zoom_combo2), default_zoom2);

	GtkWidget *download_label = gtk_label_new(_("Download Maps Method:"));
	GtkWidget *download_combo = vik_combo_box_text_new();
	for (s = download_list; *s; s++) {
		vik_combo_box_text_append(download_combo, *s);
	}

	gtk_combo_box_set_active(GTK_COMBO_BOX(download_combo), default_download);

	GtkTable *box = GTK_TABLE(gtk_table_new(3, 2, false));
	gtk_table_attach_defaults(box, GTK_WIDGET(zoom_label1), 0, 1, 0, 1);
	gtk_table_attach_defaults(box, GTK_WIDGET(zoom_combo1), 1, 2, 0, 1);
	gtk_table_attach_defaults(box, GTK_WIDGET(zoom_label2), 0, 1, 1, 2);
	gtk_table_attach_defaults(box, GTK_WIDGET(zoom_combo2), 1, 2, 1, 2);
	gtk_table_attach_defaults(box, GTK_WIDGET(download_label), 0, 1, 2, 3);
	gtk_table_attach_defaults(box, GTK_WIDGET(download_combo), 1, 2, 2, 3);

	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), GTK_WIDGET(box), false, false, 5);

	if (response_w) {
		gtk_widget_grab_focus(response_w);
	}

	gtk_widget_show_all(dialog);
	if (gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_ACCEPT) {
		gtk_widget_destroy(dialog);
		return false;
	}

	/* Return selected options. */
	*selected_zoom1 = gtk_combo_box_get_active(GTK_COMBO_BOX(zoom_combo1));
	*selected_zoom2 = gtk_combo_box_get_active(GTK_COMBO_BOX(zoom_combo2));
	*selected_download = gtk_combo_box_get_active(GTK_COMBO_BOX(download_combo));

	gtk_widget_destroy(dialog);
	return true;
}




/* My best guess of sensible limits. */
#define REALLY_LARGE_AMOUNT_OF_TILES 5000
#define CONFIRM_LARGE_AMOUNT_OF_TILES 500

/**
 * Get all maps in the region for zoom levels specified by the user
 * Sort of similar to trw_layer_download_map_along_track_cb function
 */
static void maps_layer_download_all(menu_array_values * values)
{
	LayerMaps * layer = values->layer;
	Viewport * viewport = values->viewport;

	/* I don't think we should allow users to hammer the servers too much...
	   Deliberately not allowing lowest zoom levels.
	   Still can give massive numbers to download.
	   A screen size of 1600x1200 gives around 300,000 tiles between 1..128 when none exist before!! */
	char *zoom_list[] = {(char *) "1", (char *) "2", (char *) "4", (char *) "8", (char *) "16", (char *) "32", (char *) "64", (char *) "128", (char *) "256", (char *) "512", (char *) "1024", NULL };
	double zoom_vals[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};

	int selected_zoom1, selected_zoom2, default_zoom, lower_zoom;
	int selected_download_method;

	double cur_zoom = viewport->get_zoom();

	for (default_zoom = 0; default_zoom < sizeof(zoom_vals)/sizeof(double); default_zoom++) {
		if (cur_zoom == zoom_vals[default_zoom]) {
			break;
		}
	}
	default_zoom = (default_zoom == sizeof(zoom_vals)/sizeof(double)) ? sizeof(zoom_vals)/sizeof(double) - 1 : default_zoom;

	/* Default to only 2 zoom levels below the current one/ */
	if (default_zoom > 1) {
		lower_zoom = default_zoom - 2;
	} else {
		lower_zoom = default_zoom;
	}

	/* Redownload method - needs to align with REDOWNLOAD* macro values. */
	char *download_list[] = { _("Missing"), _("Bad"), _("New"), _("Reload All"), NULL };


	char *title = g_strdup_printf (("%s: %s"), layer->get_map_label(), _("Download for Zoom Levels"));

	if (!maps_dialog_zoom_between(layer->get_toolkit_window(),
				      title,
				      zoom_list,
				      lower_zoom,
				      default_zoom,
				      &selected_zoom1,
				      &selected_zoom2,
				      download_list,
				      REDOWNLOAD_NONE, /* AKA Missing. */
				      &selected_download_method)) {
		/* Cancelled. */
		free(title);
		return;
	}
	free(title);

	/* Find out new current positions. */
	double min_lat, max_lat, min_lon, max_lon;
	VikCoord vc_ul, vc_br;
	viewport->get_min_max_lat_lon(&min_lat, &max_lat, &min_lon, &max_lon);
	struct LatLon ll_ul = { max_lat, min_lon };
	struct LatLon ll_br = { min_lat, max_lon };
	vik_coord_load_from_latlon(&vc_ul, viewport->get_coord_mode(), &ll_ul);
	vik_coord_load_from_latlon(&vc_br, viewport->get_coord_mode(), &ll_br);

	/* Get Maps Count - call for each zoom level (in reverse).
	   With REDOWNLOAD_NEW this is a possible maximum.
	   With REDOWNLOAD_NONE this only missing ones - however still has a server lookup per tile. */
	int map_count = 0;
	for (int zz = selected_zoom2; zz >= selected_zoom1; zz--) {
		map_count = map_count + layer->how_many_maps(&vc_ul, &vc_br, zoom_vals[zz], selected_download_method);
	}

	fprintf(stderr, "DEBUG: vikmapslayer: download request map count %d for method %d", map_count, selected_download_method);

	/* Absolute protection of hammering a map server. */
	if (map_count > REALLY_LARGE_AMOUNT_OF_TILES) {
		char *str = g_strdup_printf(_("You are not allowed to download more than %d tiles in one go (requested %d)"), REALLY_LARGE_AMOUNT_OF_TILES, map_count);
		a_dialog_error_msg(layer->get_toolkit_window(), str);
		free(str);
		return;
	}

	/* Confirm really want to do this. */
	if (map_count > CONFIRM_LARGE_AMOUNT_OF_TILES) {
		char *str = g_strdup_printf(_("Do you really want to download %d tiles?"), map_count);
		bool ans = a_dialog_yes_or_no(layer->get_toolkit_window(), str, NULL);
		free(str);
		if (!ans) {
			return;
		}
	}

	/* Get Maps - call for each zoom level (in reverse). */
	for (int zz = selected_zoom2; zz >= selected_zoom1; zz--) {
		layer->download_section_sub(&vc_ul, &vc_br, zoom_vals[zz], selected_download_method);
	}
}




static void maps_layer_flush(menu_array_values * values)
{
	map_cache_flush_type(map_sources[values->layer->map_index]->map_type);
}




void LayerMaps::add_menu_items(GtkMenu * menu, void * panel_)
{
	LayersPanel * panel = (LayersPanel *) panel_;

	static menu_array_values values;
	values.layer = this;
	values.viewport = panel->get_viewport();

	GtkWidget * item = gtk_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	/* Now with icons. */
	item = gtk_image_menu_item_new_with_mnemonic(_("Download _Missing Onscreen Maps"));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(maps_layer_download_missing_onscreen_maps), &values);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	gtk_widget_show(item);

	if (map_sources[this->map_index]->supports_download_only_new()) {
		item = gtk_image_menu_item_new_with_mnemonic(_("Download _New Onscreen Maps"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_REDO, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(maps_layer_download_new_onscreen_maps), &values);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
	}

	item = gtk_image_menu_item_new_with_mnemonic(_("Reload _All Onscreen Maps"));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(maps_layer_redownload_all_onscreen_maps), &values);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	gtk_widget_show(item);

	item = gtk_image_menu_item_new_with_mnemonic(_("Download Maps in _Zoom Levels..."));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_DND_MULTIPLE, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(maps_layer_download_all), &values);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	gtk_widget_show(item);

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_ABOUT, NULL);
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(maps_layer_about), &values);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	gtk_widget_show(item);

	/* Typical users shouldn't need to use this functionality - so debug only ATM. */
	if (vik_debug) {
		item = gtk_image_menu_item_new_with_mnemonic(_("Flush Map Cache"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(maps_layer_flush), &values);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
	}
}




/**
 * Enable downloading maps of the current screen area either 'new' or 'everything'.
 */
void LayerMaps::download(Viewport * viewport, bool only_new)
{
	if (!viewport) {
		return;
	}

	static menu_array_values values;
	values.layer = this;
	values.viewport = viewport;

	if (only_new) {
		/* Get only new maps. */
		maps_layer_download_new_onscreen_maps(&values);
	} else {
		/* Redownload everything. */
		maps_layer_redownload_all_onscreen_maps(&values);
	}
}




void mdi_calculate_mapstoget(MapDownloadInfo * mdi, MapSource * map, TileInfo * ulm)
{
	TileInfo mcoord = mdi->mapcoord;
	mcoord.z = ulm->z;
	mcoord.scale = ulm->scale;

	for (mcoord.x = mdi->x0; mcoord.x <= mdi->xf; mcoord.x++) {
		for (mcoord.y = mdi->y0; mcoord.y <= mdi->yf; mcoord.y++) {
			/* Only count tiles from supported areas. */
			if (is_in_area(map, &mcoord)) {
				get_cache_filename(mdi->cache_dir, mdi->cache_layout,
						   map->map_type,
						   map->get_name(),
						   &mcoord, mdi->filename_buf, mdi->maxlen,
						   map->get_file_extension());

				if (g_file_test(mdi->filename_buf, G_FILE_TEST_EXISTS) == false) {
					mdi->mapstoget++;
				}
			}
		}
	}

	return;
}




void mdi_calculate_mapstoget_other(MapDownloadInfo * mdi, MapSource * map, TileInfo * ulm)
{
	TileInfo mcoord = mdi->mapcoord;
	mcoord.z = ulm->z;
	mcoord.scale = ulm->scale;

	/* Calculate how many we need. */
	for (mcoord.x = mdi->x0; mcoord.x <= mdi->xf; mcoord.x++) {
		for (mcoord.y = mdi->y0; mcoord.y <= mdi->yf; mcoord.y++) {
			/* Only count tiles from supported areas. */
			if (is_in_area(map, &mcoord)) {
				get_cache_filename(mdi->cache_dir, mdi->cache_layout,
						   map->map_type,
						   map->get_name(),
						   &mcoord, mdi->filename_buf, mdi->maxlen,
						   map->get_file_extension());
				if (mdi->redownload_mode == REDOWNLOAD_NEW) {
					/* Assume the worst - always a new file.
					   Absolute value would require a server lookup - but that is too slow. */
					mdi->mapstoget++;
				} else {
					if (g_file_test(mdi->filename_buf, G_FILE_TEST_EXISTS) == false) {
						/* Missing. */
						mdi->mapstoget++;
					} else {
						if (mdi->redownload_mode == REDOWNLOAD_BAD) {
							/* see if this one is bad or what */
							GError *gx = NULL;
							GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(mdi->filename_buf, &gx);
							if (gx || (!pixbuf)) {
								mdi->mapstoget++;
							}
							break;
							/* Other download cases already considered or just ignored. */
						}
					}
				}
			}
		}
	}

	return;
}




MapDownloadInfo * mdi_new(LayerMaps * layer, TileInfo * ulm, TileInfo * brm, bool refresh_display, int redownload_mode)
{
	MapDownloadInfo * mdi = (MapDownloadInfo *) malloc(sizeof(MapDownloadInfo));

	mdi->layer = layer;
	mdi->map_layer_alive = true;
	mdi->mutex = vik_mutex_new();
	mdi->refresh_display = refresh_display;

	/* cache_dir and buffer for dest filename. */
	mdi->cache_dir = g_strdup(layer->cache_dir);
	mdi->maxlen = strlen(layer->cache_dir) + 40;
	mdi->filename_buf = (char *) malloc(mdi->maxlen * sizeof(char));
	mdi->map_index = layer->map_index;
	mdi->cache_layout = layer->cache_layout;

	/* kamilFIXME: in original code there was an assignment of structures:
	   mdi->mapcoord = ulm; */
	memcpy(&mdi->mapcoord, ulm, sizeof (TileInfo));
	mdi->redownload_mode = redownload_mode;

	mdi->x0 = MIN(ulm->x, brm->x);
	mdi->xf = MAX(ulm->x, brm->x);
	mdi->y0 = MIN(ulm->y, brm->y);
	mdi->yf = MAX(ulm->y, brm->y);

	mdi->mapstoget = 0;

	return mdi;
}




char * redownload_mode_message(int redownload_mode, int mapstoget, char * label)
{
	char * format = NULL;
	if (redownload_mode) {
		if (redownload_mode == REDOWNLOAD_BAD) {
			format = ngettext("Redownloading up to %d %s map...", "Redownloading up to %d %s maps...", mapstoget);
		} else {
			format = ngettext("Redownloading %d %s map...", "Redownloading %d %s maps...", mapstoget);
		}
	} else {
		format = ngettext("Downloading %d %s map...", "Downloading %d %s maps...", mapstoget);
	}

	char * result = g_strdup_printf(format, mapstoget, label);
	return result;
}




LayerMaps::LayerMaps()
{
	fprintf(stderr, "LayerMaps::LayerMaps()\n");

	this->type = LayerType::MAPS;
	strcpy(this->type_string, "MAPS");
	this->interface = &vik_maps_layer_interface;

	memset(&redownload_ul, 0, sizeof (VikCoord));
	memset(&redownload_br, 0, sizeof (VikCoord));
}




LayerMaps::LayerMaps(Viewport * viewport) : LayerMaps()
{
	fprintf(stderr, "LayerMaps::LayerMaps(viewport)\n");

	this->set_defaults(viewport);
}
