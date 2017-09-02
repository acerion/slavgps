/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2007, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdio>
#include <cstdlib>
//#include <cctype>
//#include <cstring>
#include <mutex>
#include <algorithm>
#include <string>

#include <errno.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_UTIME_H
#include <utime.h>
#endif
#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include <QDebug>

#if 1 //#ifdef HAVE_MAGIC_H // kamilFIXME: check dependency during configuration
#include <magic.h>
#endif

#include "compression.h"
#include "download.h"
#include "curl_download.h"
#include "ui_builder.h"
#include "preferences.h"
#include "globals.h"
#include "util.h"




using namespace SlavGPS;




static bool check_file_first_line(FILE * f, const char * patterns[])
{
	fpos_t pos;
	char buf[33];

	memset(buf, 0, sizeof (buf));
	if (!fgetpos(f, &pos)) {
		return false;
	}
	rewind(f);
	size_t nr = fread(buf, 1, sizeof(buf) - 1, f);
	if (!fgetpos(f, &pos)) {
		return false;
	}

	char *bp;
	for (bp = buf; (bp < (buf + sizeof(buf) - 1)) && (nr > (bp - buf)); bp++) {
		if (!(isspace(*bp))) {
			break;
		}
	}
	if ((bp >= (buf + sizeof(buf) -1)) || ((bp - buf) >= nr)) {
		return false;
	}
	for (const char ** s = patterns; *s; s++) {
		if (strncasecmp(*s, bp, strlen(*s)) == 0) {
			return true;
		}
	}
	return false;
}




bool SlavGPS::a_check_html_file(FILE * f)
{
	const char * html_str[] = {
		"<html",
		"<!DOCTYPE html",
		"<head",
		"<title",
		NULL
	};

	return check_file_first_line(f, html_str);
}




bool SlavGPS::a_check_map_file(FILE * f)
{
	/* FIXME no more true since a_check_kml_file. */
	return !a_check_html_file(f);
}




bool a_check_kml_file(FILE * f)
{
	const char * kml_str[] = {
		"<?xml",
		NULL
	};

	return check_file_first_line(f, kml_str);
}




static QList<QString> dem_files;
static std::mutex dem_files_mutex;




/* Spin button scales. */
static ParameterScale params_scales[] = {
	{1, 365, 1, 0},		/* download_tile_age */
};




static SGVariant convert_to_display(SGVariant value)
{
	/* From seconds into days. */
	return SGVariant((uint32_t) (value.u / 86400));
}




static SGVariant convert_to_internal(SGVariant value)
{
	/* From days into seconds. */
	return SGVariant((uint32_t) (86400 * value.u));
}



static ParameterExtra prefs_extra = { convert_to_display, convert_to_internal };
static Parameter prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_GENERAL "download_tile_age", SGVariantType::UINT, PARAMETER_GROUP_GENERIC, N_("Tile age (days):"), WidgetType::SPINBUTTON, &params_scales[0], NULL, &prefs_extra, NULL },
};




void Download::init(void)
{
	SGVariant tmp((uint32_t) (VIK_CONFIG_DEFAULT_TILE_AGE / 86400)); /* Now in days. */
	Preferences::register_parameter(prefs, tmp, PREFERENCES_GROUP_KEY_GENERAL);
}




static bool lock_file(const QString & file_path)
{
	bool locked = false;

	dem_files_mutex.lock();
	if (!dem_files.contains(file_path)) {
		/* The file path is not yet locked. */
		dem_files.push_back(file_path);
		locked = true;
	}
	dem_files_mutex.unlock();

	return locked;
}




static void unlock_file(const QString & file_path)
{
	dem_files_mutex.lock();
	dem_files.removeAll(file_path);
	dem_files_mutex.unlock();
}




/**
 * Unzip a file - replacing the file with the unzipped contents of the self.
 */
static void uncompress_zip(const QString & file_path)
{
	GError * error = NULL;
	GMappedFile * mf;

	if ((mf = g_mapped_file_new(file_path.toUtf8().constData(), false, &error)) == NULL) {
		qDebug() << "EE: Download: Couldn't map file" << file_path << ":" << error->message;
		g_error_free(error);
		return;
	}
	char * file_contents = g_mapped_file_get_contents(mf);

	void * unzip_mem = NULL;
	unsigned long ucsize;

	if ((unzip_mem = unzip_file(file_contents, &ucsize)) == NULL) {
		g_mapped_file_unref(mf);
		return;
	}

	/* This overwrites any previous file contents. */
	if (!g_file_set_contents(file_path.toUtf8().constData(), (char const *) unzip_mem, ucsize, &error)) {
		qDebug() << "EE: Download: Couldn't write file" << file_path << "because of" << error->message;
		g_error_free(error);
	}
}



/**
 * @file_path:  The potentially compressed filename
 *
 * Perform magic to decide how which type of decompression to attempt.
 */
void SlavGPS::a_try_decompress_file(const QString & file_path)
{
#ifdef HAVE_MAGIC_H
#ifdef MAGIC_VERSION
	/* Or magic_version() if available - probably need libmagic 5.18 or so
	   (can't determine exactly which version the versioning became available). */
	qDebug() << "DD: Download: magic version:" << MAGIC_VERSION;
#endif
	magic_t myt = magic_open(MAGIC_CONTINUE|MAGIC_ERROR|MAGIC_MIME);
	bool zip = false;
	bool bzip2 = false;
	if (myt) {
#ifdef WINDOWS
		/* We have to 'package' the magic database ourselves :(
		   --> %PROGRAM FILES%\Viking\magic.mgc */
		int ml = magic_load(myt, ".\\magic.mgc");
#else
		/* Use system default. */
		int ml = magic_load(myt, NULL);
#endif
		if (ml == 0) {
			char const * magic = magic_file(myt, file_path.toUtf8().constData());
			qDebug() << "DD: Download: magic output:" << magic;

			if (g_ascii_strncasecmp(magic, "application/zip", 15) == 0) {
				zip = true;
			}

			if (g_ascii_strncasecmp(magic, "application/x-bzip2", 19) == 0) {
				bzip2 = true;
			}
		} else {
			qDebug() << "EE: Download: magic load database failure";
		}

		magic_close(myt);
	}

	if (!(zip || bzip2)) {
		return;
	}

	if (zip) {
		uncompress_zip(file_path.toUtf8().constData());
	} else if (bzip2) {
		char* bz2_name = uncompress_bzip2(file_path);
		if (bz2_name) {
			if (remove(file_path.toUtf8().constData())) {
				qDebug() << "EE: Download: remove file failed (" << file_path << ")";
			}
			if (g_rename (bz2_name, file_path.toUtf8().constData())) {
				qDebug() << "EE: Download: file rename failed [" << bz2_name << "] to [" << file_path << "]";
			}
		}
	}

	return;
#endif
}




#define VIKING_ETAG_XATTR "xattr::viking.etag"




static bool get_etag_xattr(const QString & file_path, CurlOptions * curl_options)
{
	bool result = false;

	GFile * file = g_file_new_for_path(file_path.toUtf8().constData());
	GFileInfo * fileinfo = g_file_query_info(file, VIKING_ETAG_XATTR, G_FILE_QUERY_INFO_NONE, NULL, NULL);
	if (fileinfo) {
		char const * etag = g_file_info_get_attribute_string(fileinfo, VIKING_ETAG_XATTR);
		if (etag) {
			curl_options->etag = g_strdup(etag);
			result = !!curl_options->etag;
		}
		g_object_unref(fileinfo);
	}
	g_object_unref(file);

	if (result) {
		qDebug() << "DD: Download: Get etag (xattr) from" << file_path << ":" << curl_options->etag;
	}

	return result;
}




static bool get_etag_file(const QString & file_path, CurlOptions * curl_options)
{
	bool result = false;

	char * etag_filename = g_strdup_printf("%s.etag", file_path.toUtf8().constData());
	if (etag_filename) {
		result = g_file_get_contents(etag_filename, &curl_options->etag, NULL, NULL);
		free(etag_filename);
	}

	if (result) {
		qDebug() << "DD: Download: Get etag (file) from" << file_path << ":" << curl_options->etag;
	}

	return result;
}




static void get_etag(const QString & file_path, CurlOptions * curl_options)
{
	/* First try to get etag from xattr, then fall back to plain file. */
	if (!get_etag_xattr(file_path, curl_options) && !get_etag_file(file_path, curl_options)) {
		qDebug() << "DD: Download: Failed to get etag from" << file_path;
		return;
	}

	/* Check if etag is short enough. */
	if (strlen(curl_options->etag) > 100) {
		free(curl_options->etag);
		curl_options->etag = NULL;
	}

	/* TODO: should check that etag is a valid string. */
}




static bool set_etag_xattr(const QString & file_path, CurlOptions * curl_options)
{
	GFile * file = g_file_new_for_path(file_path.toUtf8().constData());
	bool result = g_file_set_attribute_string(file, VIKING_ETAG_XATTR, curl_options->new_etag, G_FILE_QUERY_INFO_NONE, NULL, NULL);
	g_object_unref(file);

	if (result) {
		qDebug() << "DD: Download: Set etag (xattr) on" << file_path << ":" << curl_options->new_etag;
	}

	return result;
}




static bool set_etag_file(const QString & file_path, CurlOptions * curl_options)
{
	const QString etag_filename = file_path + ".etag";
	if (g_file_set_contents(etag_filename.toUtf8().constData(), curl_options->new_etag, -1, NULL)) {
		qDebug() << "DD: Download: Set etag (file) on" << file_path << ":" << curl_options->new_etag;
		return true;
	} else {
		return false;
	}
}




static void set_etag(const QString & file_path, const QString & tmp_file_path, CurlOptions * curl_options)
{
	/* First try to store etag in extended attribute, then fall back to plain file. */
	if (!set_etag_xattr(tmp_file_path, curl_options) && !set_etag_file(file_path, curl_options)) {
		qDebug() << "DD: Download: Failed to set etag on" << file_path;
	}
}




static DownloadResult download(const QString & hostname, const QString & uri, const QString & dest_file_path, const DownloadOptions * dl_options, bool ftp, void * handle)
{
	bool failure = false;
	CurlOptions curl_options;

	/* Check file. */
	if (0 == access(dest_file_path.toUtf8().constData(), F_OK)) {
		if (dl_options == NULL
		    || (!dl_options->check_file_server_time
			&& !dl_options->use_etag)) {
			/* Nothing to do as file already exists and we don't want to check server. */
			return DownloadResult::NOT_REQUIRED;
		}

		time_t tile_age = 365; //a_preferences_get(PREFERENCES_NAMESPACE_GENERAL "download_tile_age")->u;
		/* Get the modified time of this file. */
		struct stat buf;
		(void) stat(dest_file_path.toUtf8().constData(), &buf);
		time_t file_time = buf.st_mtime;
		if ((time(NULL) - file_time) < tile_age) {
			/* File cache is too recent, so return. */
			return DownloadResult::NOT_REQUIRED;
		}

		if (dl_options != NULL && dl_options->check_file_server_time) {
			curl_options.time_condition = file_time;
		}
		if (dl_options != NULL && dl_options->use_etag) {
			get_etag(dest_file_path, &curl_options);
		}

	} else {
		char *dir = g_path_get_dirname(dest_file_path.toUtf8().constData());
		if (g_mkdir_with_parents(dir , 0777) != 0) {
			qDebug() << "WW: Download: Failed to mkdir" << dir;
		}
		free(dir);
	}

	const QString tmp_file_path = dest_file_path + ".tmp";
	if (!lock_file(tmp_file_path)) {
		qDebug() << "WW: Download: Couldn't take lock on temporary file" << tmp_file_path;
		return DownloadResult::FILE_WRITE_ERROR;
	}

	FILE * f = fopen(tmp_file_path.toUtf8().constData(), "w+b");  /* Truncate file and open it. */
	int e = errno;
	if (!f) {
		qDebug() << "WW: Download: Couldn't open temporary file" << tmp_file_path << ":" << strerror(e);
		return DownloadResult::FILE_WRITE_ERROR;
	}

	/* Call the backend function */
	CurlDownloadStatus ret = CurlDownload::get_url(hostname, uri, f, dl_options, ftp, &curl_options, handle);

	DownloadResult result = DownloadResult::SUCCESS;

	if (ret != CurlDownloadStatus::NO_ERROR && ret != CurlDownloadStatus::NO_NEWER_FILE) {
		qDebug() << "WW: Download: failed: CurlDownload::get_url = " << (int) ret;
		failure = true;
		result = DownloadResult::HTTP_ERROR;
	}

	if (!failure && dl_options != NULL && dl_options->check_file != NULL && ! dl_options->check_file(f)) {
		qDebug() << "DD: Download: file content checking failed";
		failure = true;
		result = DownloadResult::CONTENT_ERROR;
	}

	fclose(f);
	f = NULL;

	if (failure) {
		qDebug() << "WW: Download: Download error for file:" << dest_file_path;
		if (remove(tmp_file_path.toUtf8().constData()) != 0) {
			qDebug() << "WW: Download: Failed to remove" << tmp_file_path;
		}
		unlock_file(tmp_file_path);
		return result;
	}

	if (ret == CurlDownloadStatus::NO_NEWER_FILE)  {
		(void) remove(tmp_file_path.toUtf8().constData());
		/* Wpdate mtime of local copy.
		   Not security critical, thus potential Time of Check Time of Use race condition is not bad.
		   coverity[toctou] */
		if (g_utime(dest_file_path.toUtf8().constData(), NULL) != 0)
			qDebug() << "WW: Download: couldn't set time on" << dest_file_path;
	} else {
		if (dl_options != NULL && dl_options->convert_file) {
			dl_options->convert_file(tmp_file_path);
		}

		if (dl_options != NULL && dl_options->use_etag) {
			if (curl_options.new_etag) {
				/* Server returned an etag value. */
				set_etag(dest_file_path, tmp_file_path, &curl_options);
			}
		}

		/* Move completely-downloaded file to permanent location. */
		if (g_rename(tmp_file_path.toUtf8().constData(), dest_file_path.toUtf8().constData())) {
			qDebug() << "WW: Download: file rename failed" << tmp_file_path << "to" << dest_file_path;
		}
	}
	unlock_file(tmp_file_path);

	return DownloadResult::SUCCESS;
}




/**
 * uri: like "/uri.html?whatever"
 * Only reason for the "wrapper" is so we can do redirects.
 */
DownloadResult Download::get_url_http(const QString & hostname, const QString & uri, const QString & dest_file_path, const DownloadOptions * dl_options, void * handle)
{
	return download(hostname, uri, dest_file_path, dl_options, false, handle);
}




DownloadResult Download::get_url_ftp(const QString & hostname, const QString & uri, const QString & dest_file_path, const DownloadOptions * dl_options, void * handle)
{
	return download(hostname, uri, dest_file_path, dl_options, true, handle);
}




void * Download::init_handle()
{
	return CurlDownload::init_handle();
}




void Download::uninit_handle(void * handle)
{
	CurlDownload::uninit_handle(handle);
}




/**
 * @uri:         The URI (Uniform Resource Identifier)
 * @options:     Download options (maybe NULL)
 *
 * Returns name of the temporary file created - NULL if unsuccessful.
 * This string needs to be freed once used.
 * The file needs to be removed once used.
 */
char * Download::get_uri_to_tmp_file(const QString & uri, const DownloadOptions * dl_options)
{
	int tmp_fd;
	char * tmpname;

	if ((tmp_fd = g_file_open_tmp("viking-download.XXXXXX", &tmpname, NULL)) == -1) {
		qDebug() << "EE: Download: couldn't open temp file";
		return NULL;
	}

	FILE * tmp_file = fdopen(tmp_fd, "r+");
	if (!tmp_file) {
		return NULL;
	}

	if (CurlDownloadStatus::NO_ERROR != CurlDownload::download_uri(uri, tmp_file, dl_options, NULL, NULL)) {
		fclose (tmp_file);
		(void) remove(tmpname);
		free(tmpname);
		return NULL;
	}
	fclose (tmp_file);

	return tmpname;
}
