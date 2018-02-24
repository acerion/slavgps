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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdlib>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <QDebug>

#include <glib.h>
#include <glib/gstdio.h>

#include "dir.h"




using namespace SlavGPS;




#define PREFIX ": Dir:" << __FUNCTION__ << __LINE__ << ">"




static QString g_viking_dir;




/**
 * For external use. Free the result.
 * Made externally available primarily to detect when Viking is first run.
 */
QString SlavGPS::get_viking_dir_no_create(void)
{
	/* TODO: use g_get_user_config_dir? */

	QString home = QString::fromLocal8Bit(("HOME"));
	if (home.isEmpty() || access(home.toUtf8().constData(), W_OK)) {
		home = QString(g_get_home_dir());
	}

#ifdef HAVE_MKDTEMP
	if (home.isEmpty() || access(home.toUtf8().constData(), W_OK)) {
		static char temp[] = {"/tmp/vikXXXXXX"};
		home = QString(mkdtemp(temp)); /* TODO: memory leak? */
	}
#endif
	if (home.isEmpty() || access(home.toUtf8().constData(), W_OK)) {
		/* Fatal error. */
		qDebug() << "EE" PREFIX << "Unable to find a base directory";
	}

	/* Build the name of the directory. */
#ifdef __APPLE__
	return home + "/Library/Application Support/Viking";
#else
	return home + "/.viking";
#endif
}




QString SlavGPS::get_viking_dir(void)
{
	if (g_viking_dir.isEmpty()) {
		g_viking_dir = get_viking_dir_no_create();
		if (0 != access(g_viking_dir.toUtf8().constData(), F_OK)) {
			if (g_mkdir(g_viking_dir.toUtf8().constData(), 0755) != 0) {
				qDebug() << "WW" PREFIX << "Failed to create directory" << g_viking_dir;
			}
		}
	}
	return QDir::toNativeSeparators(g_viking_dir);
}




QString SlavGPS::get_viking_data_home()
{
	const QString xdg_data_home = QString::fromLocal8Bit(qgetenv("XDG_DATA_HOME"));
	if (xdg_data_home.isEmpty()) {
		return QString("");
	} else {
		return QDir::toNativeSeparators(xdg_data_home) + QDir::separator() + PACKAGE;
	}
}




/**
 * get_viking_data_path:
 *
 * Retrieves the configuration path.
 *
 * Returns: list of directories to scan for data. Should be freed with g_strfreev.
 */
char ** SlavGPS::get_viking_data_path()
{
#ifdef WINDOWS
	/* Try to use from the install directory - normally the working directory of Viking is where ever it's install location is. */
	char const * xdg_data_dirs = "./data";
	//const char *xdg_data_dirs = g_strdup( "%s/%s/data", qgetenv("ProgramFiles"), PACKAGE);
#else
	QString xdg_data_dirs = qgetenv("XDG_DATA_DIRS");
#endif
	if (xdg_data_dirs.isEmpty()) {
		/* Default value specified in
		   http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html
		*/
		xdg_data_dirs = "/usr/local/share/:/usr/share/";
	}

	char ** data_path = g_strsplit(xdg_data_dirs.toUtf8().constData(), G_SEARCHPATH_SEPARATOR_S, 0);

#ifndef WINDOWS
	/* Append the viking dir. */
	for (char ** path = data_path ; *path != NULL ; path++) {
		char * dir = *path;
		*path = g_build_filename(dir, PACKAGE, NULL);
		free(dir);
	}
#endif
	return data_path;
}
