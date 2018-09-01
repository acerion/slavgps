/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (c) 2011-2013, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_CACHE_H_
#define _SG_CACHE_H_




#include <deque>




#include <QPixmap>
#include <QString>




#include "globals.h"




#define SG_MODULE_MEM_CACHE "Mem Cache  >  "




namespace SlavGPS {




	template <class T>
	class MemCache {

	public:
		void add(const T & item);
		void remove_oldest(void);
		void clear(void) { this->items.clear(); this->current_size_bytes = 0; };

		void set_capacity_megabytes(int capacity_megabytes);
		int get_capacity_megabytes(void) const { return this->capacity_bytes / (1024.0 * 1024.0); };

		class std::deque<T>::iterator begin() { return this->items.begin(); };
		class std::deque<T>::iterator end()   { return this->items.end(); };

	private:
		/* Deque is used to easily find the oldest elements in cache. */
		std::deque<T> items;

	        int current_size_bytes = 0;
		int capacity_bytes = 0;
	};

	template <class T>
	void MemCache<T>::add(const T & item)
	{
		this->items.push_back(item);
		this->current_size_bytes += item.get_size_bytes();

		/* Keep size of queue under a limit. */
		if (this->current_size_bytes > this->capacity_bytes) {
			if (this->items.size()) {
				qDebug() << "II   " << SG_MODULE_MEM_CACHE << __FUNCTION__ << __LINE__ << "Removing oldest items from cache to fit into a limit";
				qDebug() << "II   " << SG_MODULE_MEM_CACHE << __FUNCTION__ << __LINE__ << "Current size (before removal) =" << this->current_size_bytes / (1024.0 * 1024.0) << "megabytes, capacity =" << this->capacity_bytes / (1024.0 * 1024.0) << "megabytes";
				this->remove_oldest();
				qDebug() << "II   " << SG_MODULE_MEM_CACHE << __FUNCTION__ << __LINE__ << "Current size (after removal) =" << this->current_size_bytes / (1024.0 * 1024.0) << "megabytes, capacity =" << this->capacity_bytes / (1024.0 * 1024.0) << "megabytes";
			} else {
				/* A bit paranoid because we have just added an object. */
				qDebug() << "EE   " << SG_MODULE_MEM_CACHE << __FUNCTION__ << __LINE__ << "Limit of cache reached, but cache data structure is empty";
			}
		}
	}

	template <class T>
	void MemCache<T>::remove_oldest(void)
	{
		while (this->current_size_bytes > this->capacity_bytes) {
			if (this->items.size()) {
				qDebug() << "II   " << SG_MODULE_MEM_CACHE << __FUNCTION__ << __LINE__ << "Removing oldest item from cache, item size =" << this->items.front().get_size_bytes();

				this->current_size_bytes -= ((int) this->items.front().get_size_bytes());
				this->items.pop_front(); /* Calling .pop_front() removes oldest element and calls its destructor. */

				if (this->current_size_bytes < 0) {
					qDebug() << "EE   " << SG_MODULE_MEM_CACHE << __FUNCTION__ << __LINE__ << "Reached negative cache size:" << this->current_size_bytes;
				}
			} else {
				qDebug() << "EE   " << SG_MODULE_MEM_CACHE << __FUNCTION__ << __LINE__ << "Cache size still over limit, but cache data structure is empty";

				/* If we get into this branch of
				   if/else, we may have infinite
				   while() loop. Break it. */

				/* TODO_MAYBE: perhaps we should call MemCache::clear() in case of such error? */

				break;
			}
		}
	}

	template <class T>
	void MemCache<T>::set_capacity_megabytes(int new_capacity_megabytes)
	{
		this->capacity_bytes = new_capacity_megabytes * 1024 * 1024;

		/* Size of memory already used by cached objects may
		   be larger than new capacity. */
		if (this->current_size_bytes < this->capacity_bytes) {
			this->remove_oldest();
		}
	}




	/* An image cached in RAM. */
	class CachedPixmap {
	public:
		CachedPixmap() {};
		CachedPixmap(const QPixmap & pixmap, const QString & full_path);
		~CachedPixmap();

		/* This method returns size of CachedPixmap. Most of
		   the size is of course in pixmap. The size of pixmap
		   is a size of object in memory, not a size of file
		   on disc, for two reasons:

		   1. the image in memory is probably uncompressed for
		   performance reasons, so its size in memory may be
		   larger than size of file on disc (if the image's
		   dimensions are kept).

		   2. the image in memory may be scaled (usually
		   scaled down), so its size in memory may be smaller
		   than size of file on disc.
		*/
		size_t get_size_bytes(void) const;

		bool is_valid(void) const;

		QPixmap pixmap;
		QString image_file_full_path;

	private:
		size_t size_bytes = 0;
	};




	/* Binary predicate for searching a pixmap (CachedPixmap) in pixmap cache container. */
	struct CachedPixmapCompareByPath {
	CachedPixmapCompareByPath(const QString & new_searched_full_path) : searched_full_path(new_searched_full_path) { }
		bool operator()(CachedPixmap & item) const { return item.image_file_full_path == this->searched_full_path; }
	private:
		const QString searched_full_path;
	};




}




#endif /* #ifndef _SG_CACHE_H_ */
