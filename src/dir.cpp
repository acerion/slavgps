/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright(C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright(C) 2012, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 *(at your option) any later version.
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




#if HAVE_UNISTD_H
#include <unistd.h>
#endif

/* mkdir() */
#include <sys/stat.h>
#include <sys/types.h>




#include <QDebug>
#include <QDir>
#include <QTemporaryDir>




#include "dir.h"




using namespace SlavGPS;




#define PREFIX ": SlavGPS Locations:" << __FUNCTION__ << __LINE__ << ">"




/* Definitions of static data members from SlavGPSLocation class. */
QString SlavGPSLocations::config_dir;




bool SlavGPSLocations::config_dir_exists(void)
{
	const QString dir_path = SlavGPSLocations::get_config_dir_no_create();

	if (dir_path.isEmpty()) {
		/* This is an error situation, but we have to treat it as if the directory does not exist. */
		return false;
	}
	if (0 != access(dir_path.toUtf8().constData(), F_OK)) {
		return false;
	}

	return true;
}




/* Small utility function.
   Build the name of the slavgps directory. */
QString SlavGPSLocations::build_final_name(const QString & base_dir)
{
#ifdef __APPLE__
	QString full_dir_path = QDir::toNativeSeparators(base_dir + "/Library/Application Support/Viking");
#else
	QString full_dir_path = QDir::toNativeSeparators(base_dir + "/.viking");
#endif

	qDebug() << "II" PREFIX << "Returning newly constructed directory path" << full_dir_path;
	return full_dir_path;
}




/**
   @Brief Return a path to viking directory

   The function does not create the directory itself, but may create
   parent directory/directories, in which the viking directory should
   be located.
*/
QString SlavGPSLocations::get_config_dir_no_create(void)
{
	if (!SlavGPSLocations::config_dir.isEmpty()) {
		qDebug() << "II" PREFIX << "Returning cached directory path" << SlavGPSLocations::config_dir;
		return SlavGPSLocations::config_dir;
	}

	QString base_dir;


	base_dir = QDir::homePath();
	if (!base_dir.isEmpty() && base_dir != QDir::rootPath() && 0 == access(base_dir.toUtf8().constData(), W_OK)) {
		SlavGPSLocations::config_dir = SlavGPSLocations::build_final_name(base_dir);
		return SlavGPSLocations::config_dir;
	}


	/* QDir::homePath() already uses $HOME, but let's try using qgetenv() directly anyway. */
	base_dir = QString::fromLocal8Bit(qgetenv("HOME"));
	if (!base_dir.isEmpty() && 0 == access(base_dir.toUtf8().constData(), W_OK)) {
		SlavGPSLocations::config_dir = SlavGPSLocations::build_final_name(base_dir);
		return SlavGPSLocations::config_dir;
	}


	QTemporaryDir tmp_dir("slavgpsXXXXXX");
	tmp_dir.setAutoRemove(false);
	if (tmp_dir.isValid() && 0 == access(base_dir.toUtf8().constData(), W_OK)) {
		SlavGPSLocations::config_dir = SlavGPSLocations::build_final_name(tmp_dir.path());
		return SlavGPSLocations::config_dir;
	}


	/* Fatal error. */
	qDebug() << "EE" PREFIX << "Unable to find/create a base directory for .viking dir";
	SlavGPSLocations::config_dir = "";
	return SlavGPSLocations::config_dir;
}




QString SlavGPSLocations::get_config_dir(void)
{
	QString dir_path = SlavGPSLocations::get_config_dir_no_create();
	if (dir_path.isEmpty()) {
		qDebug() << "EE" PREFIX << "Returning empty directory path";
		return dir_path;
	}

	if (0 != access(dir_path.toUtf8().constData(), F_OK)) {
		qDebug() << "II" PREFIX << "Directory" << dir_path << "does not exist, will create one.";
		if (0 != mkdir(dir_path.toUtf8().constData(), 0755)) {
			qDebug() << "WW" PREFIX << "Failed to create directory" << dir_path;
		}
	}
	return dir_path;
}




QString SlavGPSLocations::get_data_home(void)
{
	const QString xdg_data_home = QString::fromLocal8Bit(qgetenv("XDG_DATA_HOME"));
	if (xdg_data_home.isEmpty()) {
		return xdg_data_home;
	} else {
		return QDir::toNativeSeparators(xdg_data_home) + QDir::separator() + PACKAGE;
	}
}




QString SlavGPSLocations::get_file_full_path(const QString & file_name)
{
	return SlavGPSLocations::get_config_dir() + QDir::separator() + file_name;
}




/**
   Get list of directories to scan for application data.
*/
QStringList SlavGPSLocations::get_data_dirs(void)
{
#ifdef WINDOWS
	/* Try to use from the install directory - normally the working directory of Viking is where ever it's install location is. */
	QString xdg_data_dirs = "./data";
	//QString xdg_data_dirs = QString("%1/%2/data").arg(QString::fromLocal8Bit(qgetenv("ProgramFiles"))).arg(PACKAGE);
#else
	QString xdg_data_dirs = QString::fromLocal8Bit(qgetenv("XDG_DATA_DIRS"));
#endif
	if (xdg_data_dirs.isEmpty()) {
		/* Use default value specified in
		   http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html */
		xdg_data_dirs = "/usr/local/share/:/usr/share/";
	}

	QStringList data_dirs = xdg_data_dirs.split(":", QString::SkipEmptyParts);

#ifndef WINDOWS
	/* Append the viking dir. */
	for (int i = 0; i < data_dirs.size(); i++) {
		const QString tmp = data_dirs[i] + QDir::separator() + PACKAGE;
		data_dirs[i] = tmp;
	}
#endif
	return data_dirs;
}
