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




namespace SlavGPS {




	template <class T>
	class MemCache {

	public:
		void add(const T & item);
		void remove_oldest(void);
		void clear(void) { this->items.clear(); };

		void set_size_limit(int size_limit);
		int get_size_limit(void) const { return this->size_limit_bytes; };

		class std::deque<T>::iterator begin() { return this->items.begin(); };
		class std::deque<T>::iterator end()   { return this->items.end(); };

	private:
		std::deque<T> items;
		int current_size_bytes = 0;
		int size_limit_bytes = 0;
	};

	template <class T>
	void MemCache<T>::add(const T & item)
	{
		this->items.push_back(item);
		/* TODO_REALLY: update current size of cache after adding new object. */

		/* Keep size of queue under a limit. */
		if (this->current_size_bytes < this->size_limit_bytes) {
			this->remove_oldest();
		}
	}

	template <class T>
	void MemCache<T>::remove_oldest(void)
	{
		while (this->current_size_bytes > this->size_limit_bytes) {
			this->items.pop_front(); /* Calling .pop_front() removes oldest element and calls its destructor. */
			/* TODO_REALLY: update current size of cache after removing object. */
		}
	}

	template <class T>
	void MemCache<T>::set_size_limit(int new_size_limit)
	{
		this->size_limit_bytes = new_size_limit;

		/* Size of memory already used by cached
		   objects may be larger than new limit. */
		if (this->current_size_bytes < this->size_limit_bytes) {
			this->remove_oldest();
		}
	}




	/* A cached image. */
	class CachedPixmap {
	public:
		CachedPixmap() {};
		~CachedPixmap();
		QPixmap pixmap;
		QString image_file_full_path;
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
