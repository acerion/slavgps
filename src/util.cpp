/*
 * Viking - GPS data editor
 * Copyright (C) 2007, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
 * Based on:
 * Copyright (C) 2003-2007, Leandro A. F. Pereira <leandro@linuxmag.com.br>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif




#include <list>




#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <gio/gio.h>




#include <QDebug>
#include <QDir>
#include <QLocale>
#include <QThread>




#include "util.h"




using namespace SlavGPS;




#define PREFIX ": Utils:" << __FUNCTION__ << __LINE__ << ">"




static std::list<QString> deletion_list;




int Util::get_number_of_threads(void)
{
	const int n_threads = QThread::idealThreadCount();
	qDebug() << "DD" PREFIX << "there can be up to" << n_threads << "threads on this machine";
	return n_threads;
}




QString Util::uri_escape(const QString & buffer)
{
	const char * str = buffer.toUtf8().constData();

	char * esc_str = (char *) malloc(3 * strlen(str)); /* FIXME: The size should be +1. */
	char * dst = esc_str;

	for (const char * src = str; *src; src++) {
		if (*src == ' ') {
			*dst++ = '+';
		} else if (g_ascii_isalnum(*src)) {
			*dst++ = *src;
		} else {
			g_sprintf(dst, "%%%02hhX", *src);
			dst += 3;
		}
	}
	*dst = '\0';

	QString result(esc_str);
	free(esc_str);

	return result;
}




/**
   @brief Split line into two substrings, treating '=' as separator

   @param line line to split
   @param output argument, substring on left side of '='
   @param output argument substring on right side of '='

   Designed for file line processing, so it ignores strings beginning with special
   characters, such as '#'; returns false in such situations.
   Also returns false if no equals character is found.

   e.g. if buf = "GPS.parameter=42"
   key = "GPS.parameter"
   val = "42"

   @return true on success
   @return false otherwise
*/
bool Util::split_string_from_file_on_equals(const QString & line, QString & key, QString & val)
{
	/* comments, special characters in viking file format. */
	if (line.isEmpty() || line.startsWith('~') || line.startsWith('=') || line.startsWith('#')) {
		return false;
	}

	if (!line.contains('=')) {
		return false;
	}

	const QStringList elems = line.split('=', QString::SkipEmptyParts);
	if (elems.size() != 2) {
		return false;
	}

	/* Remove white spaces. */
	key = elems.at(0).trimmed();
	val = elems.at(1).trimmed();

	return true;
}




/**
   Add a name of a file into the list that is to be deleted on program exit.
   Normally this is for files that get used asynchronously,
   so we don't know when it's time to delete them - other than at this program's end.
*/
void Util::add_to_deletion_list(const QString & file_full_path)
{
	deletion_list.push_back(file_full_path);
}




/**
   Delete all the files in the deletion list.
   This should only be called on program exit.
*/
void Util::remove_all_in_deletion_list(void)
{
	for (auto iter = deletion_list.begin(); iter != deletion_list.end(); iter++) {
		if (!QDir::root().remove((*iter))) {
			qDebug() << "WW: Utils: Failed to remove" << *iter;
		}
	}
	deletion_list.clear();
}




/**
   In 'extreme' debug mode don't remove temporary files thus the
   contents can be inspected if things go wrong, with the trade off
   the user may need to delete tmp files manually

   Only use this for 'occasional' downloaded temporary files that need
   interpretation, rather than large volume items such as Bing
   attributions.
*/
bool Util::remove(const QString & file_full_path)
{
	if (1 /* vik_debug && vik_verbose */) {
		qDebug() << "WW: Util: Remove: not removing file" << file_full_path;
		return 0;
	} else {
		return QDir::root().remove(file_full_path);
	}
}




bool Util::remove(QFile & file)
{
	if (1 /* vik_debug && vik_verbose */) {
		qDebug() << "WW: Util: Remove: not removing file" << file.fileName();
		return 0;
	} else {
		return file.remove();
	}
}




/**
   Stream write buffer to a temporary file (in one go).

   @param buffer The buffer to write
   @param count Size of the buffer to write

   @return the filename of the buffer that was written
*/
QString Util::write_tmp_file_from_bytes(const void * buffer, size_t count)
{
	QString result;

	GFileIOStream * gios;
	GError * error = NULL;
	char * tmpname = NULL;

#if GLIB_CHECK_VERSION(2,32,0)
	GFile * gf = g_file_new_tmp("vik-tmp.XXXXXX", &gios, &error);
	tmpname = g_file_get_path(gf);
#else
	int fd = g_file_open_tmp("vik-tmp.XXXXXX", &tmpname, &error);
	if (error) {
		fprintf(stderr, "WARNING: %s\n", error->message);
		g_error_free(error);
		return result;
	}
	gios = g_file_open_readwrite(g_file_new_for_path(tmpname), NULL, &error);
	if (error) {
		fprintf(stderr, "WARNING: %s\n", error->message);
		g_error_free(error);
		return result;
	}
#endif

	gios = g_file_open_readwrite(g_file_new_for_path(tmpname), NULL, &error);
	if (error) {
		fprintf(stderr, "WARNING: %s\n", error->message);
		g_error_free(error);
		return result;
	}

	GOutputStream * gos = g_io_stream_get_output_stream(G_IO_STREAM(gios));
	if (g_output_stream_write(gos, buffer, count, NULL, &error) < 0) {
		fprintf(stderr, "CRITICAL: Couldn't write tmp %s file due to %s\n", tmpname, error->message);
		free(tmpname);
		tmpname = NULL;
	}

	g_output_stream_close(gos, NULL, &error);
	g_object_unref(gios);

	result = QString(tmpname);
	free(tmpname);

	return result;
}




QString Util::shell_quote(const QString & string)
{
#ifdef K_FIXME
	/* Implement shell quoting. */
#else
	char * quoted = g_shell_quote(string.toUtf8().constData());
	const QString result(quoted);
	g_free(quoted);
	return result;
#endif
}
