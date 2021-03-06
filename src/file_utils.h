/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2012, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2012-2013, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2020, Kamil Ignacak <acerion@wp.pl>
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

#ifndef _SG_FILE_UTILS_H_
#define _SG_FILE_UTILS_H_




/*
  Code that is independent from any data types specific to
  Viking/SlavGPS, and is not tied to internals of Viking file format.

  Otherwise see file.c
*/




#include <QString>
#include <QFile>




#include "globals.h"




#define VIK_MAGIC "#VIK"
#define GPX_MAGIC "<?xm"
#define VIK_MAGIC_LEN 4
#define GPX_MAGIC_LEN 4




namespace SlavGPS {



	class FileUtils {
	public:
		enum class FileType {
			Vik,
			JPEG,
			KML,
			GPX,
			Unknown
		};

		static FileType discover_file_type(QFile & file, const QString & full_path);

		static QString get_base_name(const QString & file_name);
		static sg_ret create_directory_for_file(const QString & file_full_path);
		static QString path_get_dirname(const QString & file_full_path);
		static bool has_extension(const QString & file_name, const QString & file_extension);
		static bool file_has_magic(QFile &, char const * magic, size_t size);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_FILE_UTILS_H_ */
