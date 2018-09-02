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
#include <mutex>
#include <algorithm>
#include <string>
#include <cerrno>

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
#include <QDir>
#include <QTemporaryFile>

#if 1 //#ifdef HAVE_MAGIC_H // FIXME: check dependency during configuration
#include <magic.h>
#endif

#include "compression.h"
#include "download.h"
#include "curl_download.h"
#include "ui_builder.h"
#include "preferences.h"
#include "util.h"
#include "vikutils.h"




using namespace SlavGPS;




#define SG_MODULE "Download"
#define PREFIX ": Download:" << __FUNCTION__ << __LINE__ << ">"




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




/* Spin button scale. */
static ParameterScale<int> scale_age(1, 365, SGVariant((int32_t) (VIK_CONFIG_DEFAULT_TILE_AGE / 86400)), 1, 0); /* download_tile_age; hardcoded default value in days. */




static SGVariant convert_to_display(SGVariant value)
{
	/* From seconds into days. */
	return SGVariant((int32_t) (value.u.val_int / 86400));
}




static SGVariant convert_to_internal(SGVariant value)
{
	/* From days into seconds. */
	return SGVariant((int32_t) (86400 * value.u.val_int));
}


/* FIXME: verify that users of the two functions operate on signed int. */
static ParameterExtra prefs_extra = { convert_to_display, convert_to_internal, NULL };
static ParameterSpecification prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_GENERAL, "download_tile_age", SGVariantType::Int, PARAMETER_GROUP_GENERIC, QObject::tr("Tile age (days):"), WidgetType::SpinBoxInt, &scale_age, NULL, &prefs_extra, NULL },
};




void Download::init(void)
{
	CurlDownload::init();
	Preferences::register_parameter(prefs[0], scale_age.initial);
}




void Download::uninit()
{
	CurlDownload::uninit();
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
static void uncompress_zip(const QString & file_full_path)
{
	GError * error = NULL;
	GMappedFile * mf;

	QFile file(file_full_path);
	if (!file.open(QIODevice::ReadOnly)) {
		qDebug() << "EE" PREFIX << "Can't open file" << file_full_path << file.error();
		return;
	}

	off_t file_size = file.size();
	unsigned char * file_contents = file.map(0, file_size, QFileDevice::MapPrivateOption);
	if (!file_contents) {
		qDebug() << "EE" PREFIX << "Can't map file" << file_full_path << file.error();
		return;
	}


	unsigned long ucsize;
	void * unzip_mem = unzip_file((char *) file_contents, &ucsize);
	file.unmap(file_contents);
	file.close();

	if (unzip_mem == NULL) {
		return;
	}

	/* This overwrites any previous file contents. */
	if (!g_file_set_contents(file.fileName().toUtf8().constData(), (char const *) unzip_mem, ucsize, &error)) {
		qDebug() << "EE: Download: Couldn't write file" << file.fileName() << "because of" << error->message;
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
			if (!QDir::root().remove(file_path)) {
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
	const QString etag_filename = QString("%1.etag").arg(file_path);
	bool result = g_file_get_contents(etag_filename.toUtf8().constData(), &curl_options->etag, NULL, NULL);

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

	/* TODO_LATER: should check that etag is a valid string. */
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




DownloadStatus DownloadHandle::download(const QString & hostname, const QString & uri, const QString & dest_file_path, bool ftp)
{
	bool failure = false;
	CurlOptions curl_options;

	/* Check file. */
	if (0 == access(dest_file_path.toUtf8().constData(), F_OK)) {
		if ((!this->dl_options.check_file_server_time && !this->dl_options.use_etag)) {
			/* Nothing to do as file already exists and we don't want to check server. */
			return DownloadStatus::DownloadNotRequired;
		}

		time_t tile_age = 365; //Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL ".download_tile_age").u;
		/* Get the modified time of this file. */
		struct stat buf;
		(void) stat(dest_file_path.toUtf8().constData(), &buf);
		time_t file_time = buf.st_mtime;
		if ((time(NULL) - file_time) < tile_age) {
			/* File cache is too recent, so return. */
			return DownloadStatus::DownloadNotRequired;
		}

		if (this->dl_options.check_file_server_time) {
			curl_options.time_condition = file_time;
		}
		if (this->dl_options.use_etag) {
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
		return DownloadStatus::FileWriteError;
	}

	FILE * file = fopen(tmp_file_path.toUtf8().constData(), "w+b");  /* Truncate file and open it. */
	int e = errno;
	if (!file) {
		qDebug() << "WW: Download: Couldn't open temporary file" << tmp_file_path << ":" << strerror(e);
		return DownloadStatus::FileWriteError;
	}

	/* Call the backend function */
	CurlDownloadStatus ret = this->curl_handle->get_url(hostname, uri, file, &this->dl_options, ftp, &curl_options);

	DownloadStatus result = DownloadStatus::Success;

	if (ret != CurlDownloadStatus::NoError && ret != CurlDownloadStatus::NoNewerFile) {
		qDebug() << "WW: Download: failed: CurlDownload::get_url = " << (int) ret;
		failure = true;
		result = DownloadStatus::HTTPError;
	}

	if (!failure && this->dl_options.check_file != NULL && ! this->dl_options.check_file(file)) {
		qDebug() << "DD: Download: file content checking failed";
		failure = true;
		result = DownloadStatus::ContentError;
	}

	fclose(file);
	file = NULL;

	if (failure) {
		qDebug() << "WW: Download: Download error for file:" << dest_file_path;
		if (!QDir::root().remove(tmp_file_path)) {
			qDebug() << "WW: Download: Failed to remove" << tmp_file_path;
		}
		unlock_file(tmp_file_path);
		return result;
	}

	if (ret == CurlDownloadStatus::NoNewerFile)  {
		QDir::root().remove(tmp_file_path);
		/* Wpdate mtime of local copy.
		   Not security critical, thus potential Time of Check Time of Use race condition is not bad.
		   coverity[toctou] */
		if (g_utime(dest_file_path.toUtf8().constData(), NULL) != 0)
			qDebug() << "WW: Download: couldn't set time on" << dest_file_path;
	} else {
		if (this->dl_options.convert_file) {
			this->dl_options.convert_file(tmp_file_path);
		}

		if (this->dl_options.use_etag) {
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

	return DownloadStatus::Success;
}




/**
 * uri: like "/uri.html?whatever"
 * Only reason for the "wrapper" is so we can do redirects.
 */
DownloadStatus DownloadHandle::get_url_http(const QString & hostname, const QString & uri, const QString & dest_file_path)
{
	return this->download(hostname, uri, dest_file_path, false);
}




DownloadStatus DownloadHandle::get_url_ftp(const QString & hostname, const QString & uri, const QString & dest_file_path)
{
	return this->download(hostname, uri, dest_file_path, true);
}




/**
   @brief Download URI to temporary file, return opened temporary file

   @tmp_file - uninitialized (unopened) file object
   @uri - the URI (Uniform Resource Identifier) to download

   @return true on success (@tmp_file will be returned open)
   @return false otherwise (@tmp_file will be returned closed)
*/
bool DownloadHandle::download_to_tmp_file(QTemporaryFile & tmp_file, const QString & uri)
{
	tmp_file.setFileTemplate("viking-download.XXXXXX");
	qDebug() << SG_PREFIX_I << "Created temporary file" << tmp_file.fileName();

	if (!tmp_file.open()) {
		qDebug() << SG_PREFIX_E << "Failed to open temporary file, error =" << tmp_file.error();
		return false;
	}

	const CurlDownloadStatus curl_status = this->curl_handle->download_uri(uri, &tmp_file, &this->dl_options, NULL);

	if (CurlDownloadStatus::NoError != curl_status) {
		tmp_file.close();
		QDir::root().remove(tmp_file.fileName());
		qDebug() << SG_PREFIX_E << "Downloading failed";
		return false;
	}

	return true;
}




DownloadHandle::DownloadHandle()
{
	this->curl_handle = new CurlHandle;
}




DownloadHandle::DownloadHandle(const DownloadOptions * new_dl_options)
{
	if (new_dl_options) {
		this->dl_options = *new_dl_options;
	}
	this->curl_handle = new CurlHandle;
}




DownloadHandle::~DownloadHandle()
{
	delete this->curl_handle;
	this->curl_handle = NULL;
}




bool DownloadHandle::is_valid(void) const
{
	return NULL != this->curl_handle;
}




void DownloadHandle::set_options(const DownloadOptions & new_dl_options)
{
	this->dl_options = new_dl_options;
}




DownloadOptions::DownloadOptions(const DownloadOptions & dl_options)
{
	this->check_file_server_time = dl_options.check_file_server_time;
	this->use_etag               = dl_options.use_etag;
	this->referer                = dl_options.referer;
	this->follow_location        = dl_options.follow_location;
	this->check_file             = dl_options.check_file;
	this->user_pass              = dl_options.user_pass;
	this->convert_file           = dl_options.convert_file;
}




QDebug SlavGPS::operator<<(QDebug debug, const DownloadStatus result)
{
	switch (result) {
	case DownloadStatus::FileWriteError:
		debug << "FileWriteError";
		break;
	case DownloadStatus::HTTPError:
		debug << "HTTPError";
		break;
	case DownloadStatus::ContentError:
		debug << "ContentError";
		break;
	case DownloadStatus::Success:
		debug << "Success";
		break;
	case DownloadStatus::DownloadNotRequired:
		debug << "DownloadNotRequired";
		break;
	default:
		debug << "Unknown";
		qDebug() << "EE" PREFIX << "invalid download result" << (int) result;
		break;
	};

	return debug;
}
