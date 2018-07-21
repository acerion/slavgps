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




#include <QPixmap>




#define PIXMAP_THUMB_SIZE  128




namespace SlavGPS {




	class Thumbnails {
	public:
		static bool thumbnail_exists(const QString & original_file_full_path);

		/* Generate a thumbnail, but only if it doesn't exist yet. */
		static void generate_thumbnail_if_missing(const QString & original_file_full_path);

		/* Returns freshly allocated pixmap. Caller must delete the pointer. */
		static QPixmap get_thumbnail(const QString & original_file_full_path);

		static QPixmap get_default_thumbnail(void);

		static QPixmap scale_pixmap(const QPixmap & src, int max_w, int max_h);

	private:
		/* Unconditionally generate a thumbnail. */
		static bool generate_thumbnail(const QString & original_file_full_path);
	};




} /* namespace SlavGPS */




#endif
