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
 */

#include <unordered_map>
#include <mutex>
#include <string>
#include <list>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>

#include <glib.h>

#include <QDebug>
#include <QDir>
#include <QPixmap>

#include "ui_builder.h"
#include "map_cache.h"
#include "preferences.h"
#include "util.h"




using namespace SlavGPS;




#define PREFIX ": Map Cache:" << __FUNCTION__ << __LINE__ << ">"




class MapCacheItem {
public:
	MapCacheItem(const QPixmap & new_pixmap, const MapCacheItemExtra & new_extra);
	~MapCacheItem() {};

	size_t get_size(void) const;

	QPixmap pixmap;
	MapCacheItemExtra extra;
};




#define MC_KEY_SIZE 64

#define HASHKEY_FORMAT_STRING "%d-%d-%d-%d-%d-%lu-%d-%.3f-%.3f"
#define HASHKEY_FORMAT_STRING_NOSHRINK_NOR_ALPHA "%d-%d-%d-%d-%d-%lu-"
#define HASHKEY_FORMAT_STRING_TYPE "%d-"



static std::unordered_map<std::string, MapCacheItem *> maps_cache;
static std::list<std::string> keys_list;
static size_t cache_size = 0;
static size_t max_cache_size = VIK_CONFIG_MAPCACHE_SIZE * 1024 * 1024;

static std::mutex mc_mutex;

static ParameterScale scale_cache_size = { 1, 1024, SGVariant((int32_t) VIK_CONFIG_MAPCACHE_SIZE), 1, 0 };

static ParameterSpecification prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_GENERAL, "mapcache_size", SGVariantType::Int, PARAMETER_GROUP_GENERIC, QObject::tr("Map cache memory size (MB):"), WidgetType::HScale, &scale_cache_size, NULL, NULL, NULL },
};




static void cache_add(std::string & key, const QPixmap pixmap, MapCacheItemExtra & extra);
static void cache_remove(std::string & key);
static void cache_remove_oldest();
static void flush_matching(std::string & key_part);




MapCacheItem::MapCacheItem(const QPixmap & new_pixmap, const MapCacheItemExtra & new_extra)
{
	this->pixmap = new_pixmap;
	this->extra = new_extra;
}




size_t MapCacheItem::get_size(void) const
{
	size_t size = 0;

	if (!this->pixmap.isNull()) {
		size += this->pixmap.width() * this->pixmap.height() * (this->pixmap.depth() / 8);
		size += sizeof (MapCacheItemExtra);
		size += 100; /* Not sure what this value represents, probably a guess at an average pixmap metadata size. This value existed in Viking. */
	}

	return size;
}




void MapCache::init(void)
{
	Preferences::register_parameter(prefs[0], scale_cache_size.initial);
}




void cache_add(std::string & key, const QPixmap pixmap, MapCacheItemExtra & extra)
{
	MapCacheItem * ci = new MapCacheItem(pixmap, extra);

	size_t before = maps_cache.size();

	/* Insert new or overwrite existing. */
	maps_cache[key] = ci;

	size_t after = maps_cache.size();

	if (after != before) {
		/* An item has been added, not replaced/updated. */
		cache_size += ci->get_size();
	}

	keys_list.push_back(key);

	if (maps_cache.size() != keys_list.size()) {
		qDebug() << "EE" PREFIX << "ERROR: size mismatch:" << maps_cache.size() << "!=" << keys_list.size();
		exit(EXIT_FAILURE);
	}
}




void cache_remove(std::string & key)
{
	auto iter = maps_cache.find(key);
	if (iter != maps_cache.end()) {
		MapCacheItem * ci = iter->second;
		cache_size -= ci->get_size();
		maps_cache.erase(key);
		delete ci;
	}
}




void cache_remove_oldest()
{
	std::string old_key = keys_list.front();
	cache_remove(old_key);
	keys_list.pop_front();

	if (maps_cache.size() != keys_list.size()) {
		qDebug() << "EE" PREFIX << "ERROR: size mismatch:" << maps_cache.size() << "!=" << keys_list.size();
		exit(EXIT_FAILURE);
	}
}




/**
 * Function increments reference counter of pixmap.
 * Caller may (and should) decrease it's reference.
 */
void MapCache::add(const QPixmap & pixmap, MapCacheItemExtra & extra, TileInfo * mapcoord, MapTypeID map_type_id, uint8_t alpha, double xshrinkfactor, double yshrinkfactor, const QString & file_name)
{
	if (pixmap.isNull()) {
		qDebug("EE: Map Cache: not caching corrupt pixmap for maptype %d at %d %d %d %d\n", (int) map_type_id, mapcoord->x, mapcoord->y, mapcoord->z, mapcoord->scale);
		return;
	}

	static char key_[MC_KEY_SIZE];

	std::size_t nn = file_name.isEmpty() ? 0 : std::hash<std::string>{}(file_name.toUtf8().constData());
	snprintf(key_, sizeof(key_), HASHKEY_FORMAT_STRING, (int) map_type_id, mapcoord->x, mapcoord->y, mapcoord->z, mapcoord->scale, nn, alpha, xshrinkfactor, yshrinkfactor);
	std::string key(key_);

	mc_mutex.lock();

	cache_add(key, pixmap, extra);

	/* TODO: that should be done on preference change only... */
	max_cache_size = Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL ".mapcache_size").val_uint * 1024 * 1024;

	while (cache_size > max_cache_size && maps_cache.size()) {
		cache_remove_oldest();
	}
	mc_mutex.unlock();

	static int tmp = 0;
	if ((++tmp == 20)) {
		qDebug() << QString("DD: Map Cache: keys count = %1, cache count = %2, cache size = %3, max cache size = %4").arg(keys_list.size()).arg(maps_cache.size()).arg(cache_size).arg(max_cache_size);
		tmp = 0;
	}
}




/**
 * Function increases reference counter of pixels buffer in behalf of caller.
 * Caller have to decrease references counter, when buffer is no longer needed.
 */
QPixmap MapCache::get_pixmap(TileInfo * mapcoord, MapTypeID map_type_id, uint8_t alpha, double xshrinkfactor, double yshrinkfactor, const QString & file_name)
{
	QPixmap result;

	static char key_[MC_KEY_SIZE];
	std::size_t nn = file_name.isEmpty() ? 0 : std::hash<std::string>{}(file_name.toUtf8().constData());
	snprintf(key_, sizeof (key_), HASHKEY_FORMAT_STRING, (int) map_type_id, mapcoord->x, mapcoord->y, mapcoord->z, mapcoord->scale, nn, alpha, xshrinkfactor, yshrinkfactor);
	std::string key(key_);

	mc_mutex.lock(); /* Prevent returning pixmap when cache is being cleared */
	auto iter = maps_cache.find(key);
	if (iter != maps_cache.end()) {
		result = iter->second->pixmap;
	}
	mc_mutex.unlock();

	return result;
}




MapCacheItemExtra MapCache::get_extra(TileInfo * mapcoord, MapTypeID map_type_id, uint8_t alpha, double xshrinkfactor, double yshrinkfactor, const QString & file_name)
{
	MapCacheItemExtra extra;

	static char key_[MC_KEY_SIZE];
	std::size_t nn = file_name.isEmpty() ? 0 : std::hash<std::string>{}(file_name.toUtf8().constData());
	snprintf(key_, sizeof(key_), HASHKEY_FORMAT_STRING, (int) map_type_id, mapcoord->x, mapcoord->y, mapcoord->z, mapcoord->scale, nn, alpha, xshrinkfactor, yshrinkfactor);
	std::string key(key_);

	auto iter = maps_cache.find(key);
	if (iter != maps_cache.end() && iter->second) {
		extra = iter->second->extra;
	} else {
		extra.duration = 0.0;
	}

	return extra;
}




/**
 * Common function to remove cache items for keys starting with the specified string
 */
void flush_matching(std::string & key_part)
{
	mc_mutex.lock();

	if (keys_list.empty()) {
		mc_mutex.unlock();
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
			qDebug() << "EE" PREFIX << "size mismatch:" << maps_cache.size() << "!=" << keys_list.size();
			exit(EXIT_FAILURE);
		}
	}

	mc_mutex.unlock();
}




/**
 * Appears this is only used when redownloading tiles (i.e. to invalidate old images)
 */
void MapCache::remove_all_shrinkfactors(TileInfo * mapcoord, MapTypeID map_type_id, const QString & file_name)
{
	char key_[MC_KEY_SIZE];
	std::size_t nn = file_name.isEmpty() ? 0 : std::hash<std::string>{}(file_name.toUtf8().constData());
	snprintf(key_, sizeof(key_), HASHKEY_FORMAT_STRING_NOSHRINK_NOR_ALPHA, (int) map_type_id, mapcoord->x, mapcoord->y, mapcoord->z, mapcoord->scale, nn);
	std::string key(key_);

	flush_matching(key);
}




void MapCache::flush(void)
{
	/* Everything happens within the mutex lock section. */
	mc_mutex.lock();

	for (auto iter = keys_list.begin(); iter != keys_list.end(); iter++) {
		std::string key = *iter;
		cache_remove(key);
		keys_list.erase(iter);
	}

	if (maps_cache.size() != keys_list.size()) {
		qDebug() << "EE" PREFIX << "size mismatch:" << maps_cache.size() << "!=" << keys_list.size();
		exit(EXIT_FAILURE);
	}

	mc_mutex.unlock();
}




/**
   @map_type: Specified map type

   Just remove cache items for the specified map type
   i.e. all related xyz+zoom+alpha+etc...
*/
void MapCache::flush_type(MapTypeID map_type_id)
{
	char key_[MC_KEY_SIZE];
	snprintf(key_, sizeof (key_), HASHKEY_FORMAT_STRING_TYPE, (int) map_type_id);
	std::string key(key_);
	flush_matching(key);
}




void MapCache::uninit(void)
{
	maps_cache.clear();
	/* free list */
}




/* Size of map cache in memory. */
size_t MapCache::get_size()
{
	return cache_size;
}




/* Count of items in the map cache. */
int MapCache::get_count()
{
	return maps_cache.size();
}




const QString & MapCache::get_dir()
{
	return MapCache::get_default_maps_dir();
}




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




const QString & MapCache::get_default_maps_dir(void)
{
	static QString default_dir;
	if (default_dir.isEmpty()) {

		/* Thanks to Mike Davison for the $VIKING_MAPS usage. */
		const QString mapdir = qgetenv("VIKING_MAPS");
		if (!mapdir.isEmpty()) {
			default_dir = mapdir;
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
