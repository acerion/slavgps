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

#ifndef _SG_THUMBNAILS_H_
#define _SG_THUMBNAILS_H_




#include <cstdint>

#include <QPixmap>




namespace SlavGPS {




	bool a_thumbnails_exists(const char * filename);
	void a_thumbnails_create(const char * filename);
	QPixmap * a_thumbnails_get(const char * filename);
	QPixmap * a_thumbnails_get_default();
	QPixmap * a_thumbnails_scale_pixbuf(QPixmap * src, int max_w, int max_h);




} /* namespace SlavGPS */




#endif
