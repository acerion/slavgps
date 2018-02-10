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



 /*
  * Dependencies must be just on Glib
  * see ui_utils for thing that depend on Gtk
  * see vikutils for things that further depend on other Viking types
  */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <gio/gio.h>

#include <list>
#include <cstdlib>
#include <cstring>

#include <QDebug>
#include <QDir>

#include "util.h"
#include "globals.h"

#ifdef WINDOWS
#include <windows.h>
#else
#include <unistd.h>
#endif




#include "util.h"




unsigned int SlavGPS::util_get_number_of_cpus()
{
#if GLIB_CHECK_VERSION (2, 36, 0)
	return g_get_num_processors();
#else
	long nprocs = 1;
#ifdef WINDOWS
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	nprocs = info.dwNumberOfProcessors;
#else
#ifdef _SC_NPROCESSORS_ONLN
	nprocs = sysconf(_SC_NPROCESSORS_ONLN);
	if (nprocs < 1) {
		nprocs = 1;
	}
#endif
#endif
	return nprocs;
#endif
}




char * SlavGPS::uri_escape(const char * str)
{
	char * esc_str = (char *) malloc(3 * strlen(str));
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

	return esc_str;
}




/**
 * @buf: the input string
 * @key: newly allocated string that is before the '='
 * @val: newly allocated string after the '='
 *
 * Designed for file line processing, so it ignores strings beginning with special
 * characters, such as '#'; returns false in such situations.
 * Also returns false if no equals character is found.
 *
 * e.g. if buf = "GPS.parameter=42"
 *   key = "GPS.parameter"
 *   val = "42"
 */
bool SlavGPS::split_string_from_file_on_equals(char const * buf, char ** key, char ** val)
{
	/* comments, special characters in viking file format. */
	if (buf == NULL || buf[0] == '\0' || buf[0] == '~' || buf[0] == '=' || buf[0] == '#') {
		return false;
	}

	if (!strchr(buf, '=')) {
		return false;
	}

	char ** strv = g_strsplit(buf, "=", 2);

	int gi = 0;
	char * str = strv[gi];
	while (str) {
		if (gi == 0) {
			*key = g_strdup(str);
		} else {
			*val = g_strdup(str);
		}
		gi++;
		str = strv[gi];
	}

	g_strfreev(strv);

	/* Remove newline from val and also any other whitespace. */
	*key = g_strstrip(*key);
	*val = g_strstrip(*val);
	return true;
}




static std::list<QString> deletion_list;




/**
 * Add a name of a file into the list that is to be deleted on program exit.
 * Normally this is for files that get used asynchronously,
 * so we don't know when it's time to delete them - other than at this program's end.
 */
void SlavGPS::util_add_to_deletion_list(const QString & file_full_path)
{
	deletion_list.push_back(file_full_path);
}




/**
 * Delete all the files in the deletion list.
 * This should only be called on program exit.
 */
void SlavGPS::util_remove_all_in_deletion_list(void)
{
	for (auto iter = deletion_list.begin(); iter != deletion_list.end(); iter++) {
		if (!QDir::root().remove((*iter))) {
			qDebug() << "WW: Utils: Failed to remove" << *iter;
		}
	}
	deletion_list.clear();
}




/**
 *  Removes characters from a string, in place.
 *
 *  @param str String to search.
 *  @param chars Characters to remove.
 *
 *  @return @a str - return value is only useful when nesting function calls, e.g.:
 *  @code str = utils_str_remove_chars(strdup("f_o_o"), "_"); @endcode
 *
 *  @see @c g_strdelimit.
 **/
char * util_str_remove_chars(char * string, char const * chars)
{
	if (!string) {
		return NULL;
	}

	if ((!(chars) || !*(chars))) {
		return string;
	}

	char * w = string;

	for (const char * r = string; *r; r++) {
		if (!strchr(chars, *r)) {
			*w++ = *r;
		}
	}
	*w = 0x0;
	return string;
}




/**
 * In 'extreme' debug mode don't remove temporary files thus the
 * contents can be inspected if things go wrong, with the trade off
 * the user may need to delete tmp files manually
 *
 * Only use this for 'occasional' downloaded temporary files that need
 * interpretation, rather than large volume items such as Bing
 * attributions.
 */
bool SlavGPS::util_remove(const QString & file_full_path)
{
	if (1 /* vik_debug && vik_verbose */) {
		qDebug() << "WW: Util: Remove: not removing file" << file_full_path;
		return 0;
	} else {
		return QDir::root().remove(file_full_path);
	}
}




/**
 * Stream write buffer to a temporary file (in one go).
 *
 * @param buffer The buffer to write
 * @param count Size of the buffer to write
 *
 * @return the filename of the buffer that was written
 */
QString SlavGPS::util_write_tmp_file_from_bytes(const void * buffer, size_t count)
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
