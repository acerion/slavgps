/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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

#include <unordered_map>
#include <string>
#include <list>
#include <iostream>

#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>
#include <stdlib.h>

#include "globals.h"
#include "mapcache.h"
#include "preferences.h"
#include "vik_compat.h"





using namespace SlavGPS;





#define MC_KEY_SIZE 64

#define HASHKEY_FORMAT_STRING "%d-%d-%d-%d-%d-%d-%d-%.3f-%.3f"
#define HASHKEY_FORMAT_STRING_NOSHRINK_NOR_ALPHA "%d-%d-%d-%d-%d-%d-"
#define HASHKEY_FORMAT_STRING_TYPE "%d-"





typedef struct {
	GdkPixbuf * pixbuf;
	map_cache_extra_t extra;
} cache_item_t;





static std::unordered_map<std::string, cache_item_t *> maps_cache;
static std::list<std::string> keys_list;
static size_t cache_size = 0;
static size_t max_cache_size = VIK_CONFIG_MAPCACHE_SIZE * 1024 * 1024;

static GMutex * mc_mutex = NULL;

static ParameterScale params_scales[] = {
	/* min, max, step, digits (decimal places) */
	{ 1, 1024, 1, 0 },
};

static Parameter prefs[] = {
	{ LayerType::NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "mapcache_size", ParameterType::UINT, VIK_LAYER_GROUP_NONE, N_("Map cache memory size (MB):"), WidgetType::HSCALE, params_scales, NULL, NULL, NULL, NULL, NULL },
};





static void cache_add(std::string & key, GdkPixbuf *pixbuf, map_cache_extra_t extra);
static void cache_remove(std::string & key);
static void cache_remove_oldest();
static void flush_matching(std::string & key_part);





static void cache_item_free(cache_item_t * ci)
{
	g_object_unref(ci->pixbuf);
	free(ci);
}





void SlavGPS::map_cache_init()
{
	ParameterValue tmp;
	tmp.u = VIK_CONFIG_MAPCACHE_SIZE;
	a_preferences_register(prefs, tmp, VIKING_PREFERENCES_GROUP_KEY);

	mc_mutex = vik_mutex_new();
}





void cache_add(std::string & key, GdkPixbuf *pixbuf, map_cache_extra_t extra)
{
	cache_item_t * ci = (cache_item_t *) malloc(sizeof (cache_item_t));
	ci->pixbuf = pixbuf;
	ci->extra = extra;

	size_t before = maps_cache.size();

	/* Insert new or overwrite existing. */
	maps_cache[key] = ci;

	size_t after = maps_cache.size();

	if (after != before) {
		cache_size += gdk_pixbuf_get_rowstride(pixbuf) * gdk_pixbuf_get_height(pixbuf);
		/* ATM size of 'extra' data hardly worth trying to count (compared to pixbuf sizes)
		   Not sure what this 100 represents anyway - probably a guess at an average pixbuf metadata size */
		cache_size += 100;
	}

	keys_list.push_back(key);

	if (maps_cache.size() != keys_list.size()) {
		fprintf(stderr, "%s;%d: ERROR: size mismatch: %zd != %zd\n", __FUNCTION__, __LINE__, maps_cache.size(), keys_list.size());
		exit(EXIT_FAILURE);
	}
}





void cache_remove(std::string & key)
{
	auto iter = maps_cache.find(key);
	if (iter != maps_cache.end() && iter->second && iter->second->pixbuf){
		cache_size -= gdk_pixbuf_get_rowstride(iter->second->pixbuf) * gdk_pixbuf_get_height(iter->second->pixbuf);
		cache_size -= 100;
		maps_cache.erase(key);
	}
}





void cache_remove_oldest()
{
	std::string old_key = keys_list.front();
	cache_remove(old_key);
	keys_list.pop_front();

	if (maps_cache.size() != keys_list.size()) {
		fprintf(stderr, "%s;%d: ERROR: size mismatch: %zd != %zd\n", __FUNCTION__, __LINE__, maps_cache.size(), keys_list.size());
		exit(EXIT_FAILURE);
	}
}





/**
 * Function increments reference counter of pixbuf.
 * Caller may (and should) decrease it's reference.
 */
void SlavGPS::map_cache_add(GdkPixbuf * pixbuf, map_cache_extra_t extra, TileInfo * mapcoord, MapTypeID map_type, uint8_t alpha, double xshrinkfactor, double yshrinkfactor, char const * name)
{
	if (!GDK_IS_PIXBUF(pixbuf)) {
		fprintf(stderr, "DEBUG: Not caching corrupt pixbuf for maptype %d at %d %d %d %d\n", map_type, mapcoord->x, mapcoord->y, mapcoord->z, mapcoord->scale);
		return;
	}

	static char key_[MC_KEY_SIZE];

	std::size_t nn = name ? std::hash<std::string>{}(name) : 0;
	snprintf(key_, sizeof(key_), HASHKEY_FORMAT_STRING, map_type, mapcoord->x, mapcoord->y, mapcoord->z, mapcoord->scale, nn, alpha, xshrinkfactor, yshrinkfactor);
	std::string key(key_);

	g_mutex_lock(mc_mutex);
	g_object_ref(pixbuf);

	cache_add(key, pixbuf, extra);

	/* TODO: that should be done on preference change only... */
	max_cache_size = a_preferences_get(VIKING_PREFERENCES_NAMESPACE "mapcache_size")->u * 1024 * 1024;

	while (cache_size > max_cache_size && maps_cache.size()) {
		cache_remove_oldest();
	}
	g_mutex_unlock(mc_mutex);

	static int tmp = 0;
	if ((++tmp == 20)) {
		fprintf(stderr, "DEBUG: keys count = %d, cache count = %d, cache size = %u, max cache size = %u\n", keys_list.size(), maps_cache.size(), cache_size, max_cache_size);
		tmp = 0;
	}
}





/**
 * Function increases reference counter of pixels buffer in behalf of caller.
 * Caller have to decrease references counter, when buffer is no longer needed.
 */
GdkPixbuf * SlavGPS::map_cache_get(TileInfo * mapcoord, MapTypeID map_type, uint8_t alpha, double xshrinkfactor, double yshrinkfactor, char const * name)
{
	static char key_[MC_KEY_SIZE];
	std::size_t nn = name ? std::hash<std::string>{}(name) : 0;
	snprintf(key_, sizeof (key_), HASHKEY_FORMAT_STRING, map_type, mapcoord->x, mapcoord->y, mapcoord->z, mapcoord->scale, nn, alpha, xshrinkfactor, yshrinkfactor);
	std::string key(key_);

	g_mutex_lock(mc_mutex); /* prevent returning pixbuf when cache is being cleared */

	auto iter = maps_cache.find(key);
	if (iter != maps_cache.end()) {
		g_object_ref(iter->second->pixbuf);
		g_mutex_unlock(mc_mutex);
		return iter->second->pixbuf;
	} else {
		g_mutex_unlock(mc_mutex);
		return NULL;
	}
}





map_cache_extra_t SlavGPS::map_cache_get_extra(TileInfo * mapcoord, MapTypeID map_type, uint8_t alpha, double xshrinkfactor, double yshrinkfactor, char const * name)
{
	static char key_[MC_KEY_SIZE];
	std::size_t nn = name ? std::hash<std::string>{}(name) : 0;
	snprintf(key_, sizeof(key_), HASHKEY_FORMAT_STRING, map_type, mapcoord->x, mapcoord->y, mapcoord->z, mapcoord->scale, nn, alpha, xshrinkfactor, yshrinkfactor);
	std::string key(key_);

	auto iter = maps_cache.find(key);
	if (iter != maps_cache.end() && iter->second) {
		return iter->second->extra;
	} else {
		return (map_cache_extra_t) { 0.0 };
	}
}





/**
 * Common function to remove cache items for keys starting with the specified string
 */
void flush_matching(std::string & key_part)
{
	g_mutex_lock(mc_mutex);

	if (keys_list.empty()) {
		g_mutex_unlock(mc_mutex);
		return;
	}

	size_t len = key_part.length();

	int i = 0;
	for (auto iter = keys_list.begin(); iter != keys_list.end(); ) {
		std::string key = *iter;

		auto erase_iter = keys_list.end();
		if (0 == key.compare(0, len, key_part)) {
			cache_remove(key);
			erase_iter = iter;
		}
		iter++;

		if (erase_iter != keys_list.end()) {
			keys_list.erase(erase_iter);
		}

		if (maps_cache.size() != keys_list.size()) {
			fprintf(stderr, "%s;%d: ERROR: size mismatch: %zd != %zd\n", __FUNCTION__, __LINE__, maps_cache.size(), keys_list.size());
			exit(EXIT_FAILURE);
		}
	}

	g_mutex_unlock(mc_mutex);
}





/**
 * Appears this is only used when redownloading tiles (i.e. to invalidate old images)
 */
void SlavGPS::map_cache_remove_all_shrinkfactors(TileInfo * mapcoord, MapTypeID map_type, char const * name)
{
	char key_[MC_KEY_SIZE];
	std::size_t nn = name ? std::hash<std::string>{}(name) : 0;
	snprintf(key_, sizeof(key_), HASHKEY_FORMAT_STRING_NOSHRINK_NOR_ALPHA, map_type, mapcoord->x, mapcoord->y, mapcoord->z, mapcoord->scale, nn);
	std::string key(key_);

	flush_matching(key);
}





void SlavGPS::map_cache_flush()
{
	/* Everything happens within the mutex lock section. */
	g_mutex_lock(mc_mutex);

	for (auto iter = keys_list.begin(); iter != keys_list.end(); iter++) {
		std::string key = *iter;
		cache_remove(key);
		keys_list.erase(iter);
	}

	if (maps_cache.size() != keys_list.size()) {
		fprintf(stderr, "%s;%d: ERROR: size mismatch: %zd != %zd\n", __FUNCTION__, __LINE__, maps_cache.size(), keys_list.size());
		exit(EXIT_FAILURE);
	}

	g_mutex_unlock(mc_mutex);
}





/**
 *  map_cache_flush_type:
 *  @map_type: Specified map type
 *
 * Just remove cache items for the specified map type
 * i.e. all related xyz+zoom+alpha+etc...
 */
void SlavGPS::map_cache_flush_type(MapTypeID map_type)
{
	char key_[MC_KEY_SIZE];
	snprintf(key_, sizeof (key_), HASHKEY_FORMAT_STRING_TYPE, map_type);
	std::string key(key_);
	flush_matching(key);
}





void SlavGPS::map_cache_uninit()
{
	maps_cache.clear();
	/* free list */
	vik_mutex_free(mc_mutex);
}





/* Size of mapcache in memory. */
size_t SlavGPS::map_cache_get_size()
{
	return cache_size;
}





/* Count of items in the mapcache. */
int SlavGPS::map_cache_get_count()
{
	return maps_cache.size();
}
