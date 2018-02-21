/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2012, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2012-2013, Rob Norris <rw_norris@hotmail.com>
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




#include <QDebug>




#include "file_utils.h"




#ifdef WINDOWS
#define FILE_SEP '\\'
#else
#define FILE_SEP '/'
#endif




using namespace SlavGPS;




QString FileUtils::get_base_name(const QString & file_name)
{
	int len = 0;

	for (int i = file_name.size() - 1; i >=0; i--) {
		if (file_name[i] == FILE_SEP) {
			qDebug() << "DD: File Utils: get base name of" << file_name << "is" << file_name.right(len);
			return file_name.right(len);
		}
		len++;
	}

	qDebug() << "DD: File Utils: get base name of" << file_name << "is" << file_name;
	return file_name;
}




/*
  Example:
  bool is_gpx = FileUtils::has_extension("a/b/c.gpx", ".gpx");
*/
bool FileUtils::has_extension(const QString & file_name, const QString & file_extension)
{
	if (file_name.isEmpty()) {
		return false;
	}

	if (file_extension.isEmpty() || file_extension[0] != '.') {
		return false;
	}

	const QString base_name = FileUtils::get_base_name(file_name);
	if (base_name.isEmpty()) {
		return false;
	}

	const bool result = base_name.right(file_extension.size()) == file_extension;
	qDebug() << "DD: File: has extension:" << file_extension << "in" << base_name << ":" << result;
	return result;
}
