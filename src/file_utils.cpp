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




#include <unistd.h>




#include <QDebug>
#include <QDir>




#include "file_utils.h"
#include "jpg.h"




#ifdef WINDOWS
#define FILE_SEP '\\'
#else
#define FILE_SEP '/'
#endif




using namespace SlavGPS;




#define SG_MODULE "File Utils"




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
	if (result) {
		qDebug() << SG_PREFIX_I << "File name" << base_name << "has expected extension:" << file_extension << "=" << base_name.right(file_extension.size());
	} else {
		qDebug() << SG_PREFIX_I << "File name" << base_name << "doesn't have expected extension:" << file_extension << "!=" << base_name.right(file_extension.size());
	}
	return result;
}




bool FileUtils::file_has_magic(QFile & file, char const * magic, size_t size)
{
	char buffer[16] = { 0 }; /* There should be no magic longer than few (3-4) characters. */
	if (size > sizeof (buffer)) {
		qDebug() << SG_PREFIX_E << "Expected magic length too large:" << size;
		return false;
	}

	if ((int) size != file.peek(buffer, size)) {
		/* Too little data in file to read magic. */
		return false;
	}

	if (0 != strncmp(buffer, magic, size)) {
		return false;
	}

	return true;
}




QString FileUtils::path_get_dirname(const QString & file_full_path)
{
	const QFileInfo file_info(file_full_path);
	const QDir dir = file_info.dir();

	/* I think that we can't use ::canonicalPath() or
	   ::absolutePath() because the path may not exist yet so QT
	   can't "calculate" canonical/absolute path. ::path() seems
	   to be the only available option here. */
	const QString dir_path = dir.path();

	qDebug() << SG_PREFIX_D << "File full path =" << file_full_path << "----> dir full path =" << dir << dir_path;

	return dir_path;
}




sg_ret FileUtils::create_directory_for_file(const QString & file_full_path)
{
	const QString dir_path = FileUtils::path_get_dirname(file_full_path);

	if (0 == access(dir_path.toUtf8().constData(), F_OK)) {
		return sg_ret::ok;
	}

	QDir dir(QDir::root());
	if (dir.mkpath(dir_path)) {
		qDebug() << SG_PREFIX_I << "Created path:" << file_full_path << "->" << dir_path;
		return sg_ret::ok;
	} else {
		qDebug() << SG_PREFIX_E << "Not created path:" << file_full_path << "->" << dir_path;
		return sg_ret::err;
	}
}





FileUtils::FileType FileUtils::discover_file_type(QFile & file, const QString & full_path)
{
	if (FileUtils::file_has_magic(file, VIK_MAGIC, VIK_MAGIC_LEN)) {
		return FileUtils::FileType::Vik;
	}

	if (jpg_magic_check(full_path)) {
		return FileUtils::FileType::JPEG;
	}

	if (FileUtils::has_extension(full_path, ".kml") && FileUtils::file_has_magic(file, GPX_MAGIC, GPX_MAGIC_LEN)) {
		return FileUtils::FileType::KML;
	}

	/* Use a extension check first, as a GPX file header may have
	   a Byte Order Mark (BOM) in it - which currently confuses
	   our FileUtils::file_has_magic() function. */
	if (FileUtils::has_extension(full_path, ".gpx") || FileUtils::file_has_magic(file, GPX_MAGIC, GPX_MAGIC_LEN)) {
		return FileUtils::FileType::GPX;
	}

	return FileUtils::FileType::Unknown;
}
