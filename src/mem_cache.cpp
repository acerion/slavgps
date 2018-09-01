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




/* In-memory cache. */




#include <QFileInfo>
#include <QDebug>




#include "mem_cache.h"
#include "globals.h"




using namespace SlavGPS;




#define SG_MODULE "Mem Cache"




CachedPixmap::CachedPixmap(const QPixmap & new_pixmap, const QString & new_full_path)
{
	if (!new_pixmap.isNull()) {
		this->pixmap = new_pixmap;
		this->image_file_full_path = new_full_path;

		this->size_bytes = this->pixmap.width() * this->pixmap.height() * (this->pixmap.depth() / 8);
		this->size_bytes += sizeof (QPixmap);

		/* Remember that a valid pixmap may be non-null, but
		   path may be empty.  This is true e.g. when cached
		   pixmap is created from
		   Thumbnails::get_default_thumbnail() pixmap which
		   doesn't exist on disc. */

#if 1           /* Only for tests and comparison of size of disc file and size of memory object size. */
		if (!this->image_file_full_path.isEmpty()) {
			QFileInfo file_info(this->image_file_full_path);
			qDebug () << SG_PREFIX_I << "In-memory size = " << this->size_bytes << ", on-disc size =" << file_info.size();
		}
#endif
	}
}




size_t CachedPixmap::get_size_bytes(void) const
{
	return this->size_bytes;
}




bool CachedPixmap::is_valid(void) const
{
	/* Remember that a valid pixmap may be non-null even if path
	   is empty.  This is true e.g. when cached pixmap is created
	   from Thumbnails::get_default_thumbnail() pixmap which
	   doesn't exist on disc. */
	return !this->pixmap.isNull();
}
