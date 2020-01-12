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
 */




#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <algorithm>
#include <string>
#include <cerrno>

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_UTIME_H
#include <utime.h>
#endif




#include <glib/gstdio.h>
#include <gio/gio.h>




#include <QDebug>
#include <QDir>
#include <QTemporaryFile>




#ifdef HAVE_MAGIC_H
#include <magic.h>
#endif




#include "compression.h"
#include "download.h"
#include "curl_download.h"
#include "ui_builder.h"
#include "preferences.h"
#include "util.h"
#include "vikutils.h"
#include "file_utils.h"




using namespace SlavGPS;




#define SG_MODULE "Download"
#define ETAG_VALUE_LEN_MAX 100




static bool file_first_line_matches(FILE * file, const char * patterns[])
{
	fpos_t pos;
	char buf[33];

	memset(buf, 0, sizeof (buf));
	if (!fgetpos(file, &pos)) {
		return false;
	}
	rewind(file);
	size_t nr = fread(buf, 1, sizeof(buf) - 1, file);
	if (!fgetpos(file, &pos)) {
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




sg_ret SlavGPS::html_file_validator_fn(FILE * file, bool * is_valid)
{
	const char * html_str[] = {
		"<html",
		"<!DOCTYPE html",
		"<head",
		"<title",
		NULL
	};

	*is_valid = file_first_line_matches(file, html_str);

	return sg_ret::ok;
}




sg_ret SlavGPS::map_file_validator_fn(FILE * file, bool * is_valid)
{
	bool is_html = false;
	if (sg_ret::ok != SlavGPS::html_file_validator_fn(file, &is_html)) {
		return sg_ret::err;
	}
	if (is_html) {
		*is_valid = false;
		return sg_ret::ok;
	}


	bool is_kml = false;
	if (sg_ret::ok != SlavGPS::kml_file_validator_fn(file, &is_kml)) {
		return sg_ret::err;
	}
	if (is_kml) {
		*is_valid = false;
		return sg_ret::ok;
	}


	*is_valid = true;
	return sg_ret::ok;
}




sg_ret SlavGPS::kml_file_validator_fn(FILE * file, bool * is_valid)
{
	const char * kml_str[] = {
		"<?xml",
		NULL
	};

	*is_valid = file_first_line_matches(file, kml_str);

	return sg_ret::ok;
}




static QList<QString> dem_files;
static std::mutex dem_files_mutex;




/* Spin button's scale. */
static MeasurementScale<Duration> scale_download_tile_age(1, 365, 7, 1, DurationType::Unit::E::Days, 0);




#if 0
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
#endif



static ParameterSpecification prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_GENERAL "download_tile_age", SGVariantType::DurationType, PARAMETER_GROUP_GENERIC, QObject::tr("Tile age:"), WidgetType::DurationType, &scale_download_tile_age, NULL, "" }, // KKAMIL
};




void Download::init(void)
{
	CurlDownload::init();
	Preferences::register_parameter_instance(prefs[0], SGVariant(scale_download_tile_age.m_initial, prefs[0].type_id));
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
static sg_ret uncompress_zip(const QString & file_full_path)
{
	QFile file(file_full_path);
	if (!file.open(QIODevice::ReadOnly)) {
		qDebug() << SG_PREFIX_E << "Can't open file" << file_full_path << file.error();
		return sg_ret::err_arg;
	}

	off_t file_size = file.size();
	unsigned char * file_contents = file.map(0, file_size, QFileDevice::MapPrivateOption);
	if (!file_contents) {
		qDebug() << SG_PREFIX_E << "Can't map file" << file_full_path << file.error();
		return sg_ret::err_algo;
	}


	size_t unzip_size;
	void * unzip_mem = unzip_file((char *) file_contents, &unzip_size);
	file.unmap(file_contents);

	if (unzip_mem == NULL) {
		return sg_ret::err_algo;
	}

	/* Overwrite any previous file contents. */
	if (!file.seek(0)) {
		qDebug() << SG_PREFIX_E << "seek(0) failed" << file.error();
		return sg_ret::err_algo;
	}
	const size_t written = file.write((const char *) unzip_mem, unzip_size);
	if (written != unzip_size) {
		qDebug() << SG_PREFIX_E << "Short write for" << file.fileName() << "expected bytes:" << unzip_size << "written bytes:" << written << file.error();;
		return sg_ret::err_algo;
	}

	file.close();

	return sg_ret::ok;
}



/**
 * @file_path:  The potentially compressed filename
 *
 * Perform magic to decide how which type of decompression to attempt.
 */
void SlavGPS::a_try_decompress_file(const QString & archive_file_full_path)
{
#ifdef HAVE_MAGIC_H
#ifdef MAGIC_VERSION
	/* Or magic_version() if available - probably need libmagic 5.18 or so
	   (can't determine exactly which version the versioning became available). */
	qDebug() << SG_PREFIX_D << "Magic version:" << MAGIC_VERSION;
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
			char const * magic = magic_file(myt, archive_file_full_path.toUtf8().constData());
			if (NULL == magic || '\0' == magic[0]) {
				qDebug() << SG_PREFIX_W << "Empty magic in file" << archive_file_full_path;
			} else {
				qDebug() << SG_PREFIX_D << "Magic in file:" << magic;

				if (0 == strncasecmp(magic, "application/zip", strlen("application/zip"))) {
					zip = true;
				}

				if (0 == strncasecmp(magic, "application/x-bzip2", strlen("application/x-bzip2"))) {
					bzip2 = true;
				}
			}
		} else {
			qDebug() << SG_PREFIX_E << "Failed to load magic database";
		}

		magic_close(myt);
	}

	if (!(zip || bzip2)) {
		return;
	}

	if (zip) {
		uncompress_zip(archive_file_full_path);
	} else if (bzip2) {
		QString uncompressed_file_full_path;
		if (sg_ret::ok == uncompress_bzip2(uncompressed_file_full_path, archive_file_full_path)) {
			if (!QDir::root().remove(archive_file_full_path)) {
				qDebug() << SG_PREFIX_E << "Remove file failed (" << archive_file_full_path << ")";
			}
			if (!QDir::root().rename(uncompressed_file_full_path, archive_file_full_path)) {
				qDebug() << SG_PREFIX_E << "File rename failed [" << uncompressed_file_full_path << "] to [" << archive_file_full_path << "]";
			}
		} else {
			qDebug() << SG_PREFIX_E << "Failed to uncompress bz2 file" << archive_file_full_path;
		}
	}

	return;
#endif
}




/* https://developer.gnome.org/gio/stable/GFile.html, "Entity Tags" */
#define VIKING_ETAG_XATTR "xattr::viking.etag"




static sg_ret get_etag_via_xattr(const QString & file_path, QString & etag)
{
	sg_ret retv = sg_ret::err;

	GFile * file = g_file_new_for_path(file_path.toUtf8().constData());
	GFileInfo * fileinfo = g_file_query_info(file, VIKING_ETAG_XATTR, G_FILE_QUERY_INFO_NONE, NULL, NULL);
	if (fileinfo) {
		char const * attr_value = g_file_info_get_attribute_string(fileinfo, VIKING_ETAG_XATTR);
		if (attr_value) {
			char * tag = g_strdup(attr_value);
			retv = tag ? sg_ret::ok : sg_ret::err;
			etag = QString(tag);
			free(tag);

		}
		g_object_unref(fileinfo);
	}
	g_object_unref(file);

	if (retv == sg_ret::ok) {
		qDebug() << SG_PREFIX_D << "etag value for file" << file_path << ":" << etag;
	}

	return retv;
}




static sg_ret get_etag_via_file(const QString & file_full_path, QString & etag)
{
	const QString etag_file_full_path = QString("%1.etag").arg(file_full_path);

	QFile file(etag_file_full_path);
	if (!file.open(QIODevice::ReadOnly)) {
		qDebug() << SG_PREFIX_E << "Failed to open etag file" << etag_file_full_path << ":" << file.error();
		return sg_ret::err;
	}

	char buf[ETAG_VALUE_LEN_MAX + 1] = { 0 };
	const qint64 r = file.readLine(buf, sizeof (buf));
	if (r <= 0) {
		qDebug() << SG_PREFIX_W << "Failed to read etag value from file" << etag_file_full_path << file.error();
		return sg_ret::err;
	}

	etag = QString(buf);
	qDebug() << SG_PREFIX_D << "etag value for file" << file_full_path << ":" << etag;

	return sg_ret::ok;
}




static sg_ret get_etag(const QString & file_full_path, QString & etag)
{
	/* First try to get etag from xattr, then fall back to plain file. */
	if (sg_ret::ok != get_etag_via_xattr(file_full_path, etag)
	    && sg_ret::ok != get_etag_via_file(file_full_path, etag)) {

		qDebug() << SG_PREFIX_W << "Failed to get etag for" << file_full_path;
		return sg_ret::err;
	}

	/* Check if etag is short enough. */
	if (etag.length() > ETAG_VALUE_LEN_MAX) {
		qDebug() << SG_PREFIX_W << "Failed to get etag for" << file_full_path << ": value too long:" << etag.length();
		etag = "";
		return sg_ret::err;
	}

	/* TODO_LATER: validate the value of etag (do more checks than just test of length) */

	return sg_ret::ok;
}




static sg_ret set_etag_xattr(const QString & file_full_path, const QString & etag)
{
	GFile * file = g_file_new_for_path(file_full_path.toUtf8().constData());
	const sg_ret retv = g_file_set_attribute_string(file, VIKING_ETAG_XATTR, etag.toUtf8().constData(), G_FILE_QUERY_INFO_NONE, NULL, NULL) ? sg_ret::ok : sg_ret::err;
	g_object_unref(file);

	if (sg_ret::ok == retv) {
		qDebug() << SG_PREFIX_D << "Set etag" << etag << "for file" << file_full_path;
	} else {
		qDebug() << SG_PREFIX_W << "Failed to set etag" << etag << "for file" << file_full_path;
	}

	return retv;
}




static sg_ret set_etag_file(const QString & file_full_path, const QString & etag)
{
	const QString etag_filename = file_full_path + ".etag";

	QFile file(file_full_path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		qDebug() << SG_PREFIX_E << "Can't open file" << file_full_path << file.error();
		return sg_ret::err;
	}

	if (-1 == file.write(etag.toUtf8().constData())) {
		qDebug() << SG_PREFIX_E << "Failed to write etag" << etag << "to file" << file_full_path << ":" << file.error();
		return sg_ret::err;
	}

	qDebug() << SG_PREFIX_D << "Set etag for" << file_full_path << ":" << etag;
	return sg_ret::ok;
}




static sg_ret set_etag(const QString & file_full_path, const QString & tmp_file_path, const QString & etag)
{
	/* First try to store etag in extended attribute, then fall back to plain file. */
	if (sg_ret::ok != set_etag_xattr(tmp_file_path, etag)
	    && sg_ret::ok != set_etag_file(file_full_path, etag)) {

		qDebug() << SG_PREFIX_W << "Failed to set etag for" << file_full_path;
		return sg_ret::err;
	}
	return sg_ret::ok;
}




DownloadStatus DownloadHandle::perform_download(const QString & hostname, const QString & uri, const QString & dest_file_path, DownloadProtocol protocol)
{
	bool failure = false;
	CurlOptions curl_options;

	/* Check file. */
	if (0 == access(dest_file_path.toUtf8().constData(), F_OK)) {
		if ((!this->dl_options.check_file_server_time && !this->dl_options.use_etag)) {
			/* Nothing to do as file already exists and we don't want to check server. */
			return DownloadStatus::DownloadNotRequired;
		}

		DurationType::LL tile_age = Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL "download_tile_age").get_duration().convert_to_unit(DurationType::Unit::E::Seconds).ll_value();
		/* Get the modified time of this file. */
		struct stat buf;
		(void) stat(dest_file_path.toUtf8().constData(), &buf);
		const time_t now = time(NULL);
		const time_t file_time = buf.st_mtime;
		if ((now - file_time) < tile_age) {
			/* File cache is too recent, so return. */
			return DownloadStatus::DownloadNotRequired;
		}

		if (this->dl_options.check_file_server_time) {
			curl_options.time_condition = file_time;
		}
		if (this->dl_options.use_etag) {
			get_etag(dest_file_path, curl_options.etag);
		}

	} else {
		if (sg_ret::ok != FileUtils::create_directory_for_file(dest_file_path)) {
			qDebug() << SG_PREFIX_E << "Failed to create directory for file" << dest_file_path;
			return DownloadStatus::FileWriteError;
		}
	}

	const QString tmp_file_path = dest_file_path + ".tmp";
	if (!lock_file(tmp_file_path)) {
		qDebug() << SG_PREFIX_W << "Couldn't take lock on temporary file" << tmp_file_path;
		return DownloadStatus::FileWriteError;
	}

	FILE * file = fopen(tmp_file_path.toUtf8().constData(), "w+b");  /* Truncate file and open it. */
	int e = errno;
	if (!file) {
		qDebug() << SG_PREFIX_W << "Couldn't open temporary file" << tmp_file_path << ":" << strerror(e);
		return DownloadStatus::FileWriteError;
	}

	/* Call the backend function */
	CurlDownloadStatus ret = this->curl_handle->get_url(hostname, uri, file, &this->dl_options, protocol, &curl_options);

	DownloadStatus result = DownloadStatus::Success;

	if (ret != CurlDownloadStatus::NoError && ret != CurlDownloadStatus::NoNewerFile) {
		qDebug() << SG_PREFIX_E << "Failed: CurlDownload::get_url = " << (int) ret;
		failure = true;
		result = DownloadStatus::HTTPError;
	}

	if (!failure && this->dl_options.file_validator_fn) {
		bool file_is_valid = false;
		this->dl_options.file_validator_fn(file, &file_is_valid);
		if (!file_is_valid) {
			qDebug() << SG_PREFIX_E << "File content checking failed";
			failure = true;
			result = DownloadStatus::ContentError;
		}
	}

	fclose(file);
	file = NULL;

	if (failure) {
		qDebug() << SG_PREFIX_W << "Download error for file:" << dest_file_path;
		if (!QDir::root().remove(tmp_file_path)) {
			qDebug() << SG_PREFIX_W << "Failed to remove" << tmp_file_path;
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
			qDebug() << SG_PREFIX_W << "Couldn't set time on" << dest_file_path;
	} else {
		if (this->dl_options.convert_file) {
			this->dl_options.convert_file(tmp_file_path);
		}

		if (this->dl_options.use_etag) {
			if (!curl_options.new_etag.isEmpty()) {
				/* Server returned an etag value. */
				set_etag(dest_file_path, tmp_file_path, curl_options.new_etag);
			}
		}

		/* Move completely-downloaded file to permanent location. */
		if (!QDir::root().rename(tmp_file_path, dest_file_path)) {
			qDebug() << SG_PREFIX_W << "File rename failed" << tmp_file_path << "to" << dest_file_path;
		}
	}
	unlock_file(tmp_file_path);

	return DownloadStatus::Success;
}




DownloadStatus DownloadHandle::perform_download(const QString & url, const QString & dest_file_path)
{
	const DownloadProtocol protocol = SlavGPS::from_url(url);
	if (protocol == DownloadProtocol::Unknown) {
		qDebug() << SG_PREFIX_W << "Unsupported protocol in" << url;
	}

	return DownloadHandle::perform_download(url, "", dest_file_path, protocol);
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
	this->file_validator_fn      = dl_options.file_validator_fn;
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
		qDebug() << SG_PREFIX_E << "Invalid download result" << (int) result;
		break;
	};

	return debug;
}





QString SlavGPS::to_string(DownloadProtocol protocol)
{
	QString result;

	switch (protocol) {
	case DownloadProtocol::FTP:
		result = "ftp";
		break;
	case DownloadProtocol::HTTP:
		result = "http";
		break;
	case DownloadProtocol::HTTPS:
		result = "https";
		break;
	case DownloadProtocol::File:
		result = "file";
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected download protocol" << (int) protocol;
		result = "";
	}

	return result;
}





DownloadProtocol SlavGPS::from_url(const QString & url)
{
	DownloadProtocol protocol;

	if (url.left(strlen("http://")) == "http://") {
		protocol = DownloadProtocol::HTTP;

	} else if (url.left(strlen("https://")) == "https://") {
		protocol = DownloadProtocol::HTTPS;

	} else if (url.left(strlen("ftp://")) == "ftp://") {
		protocol = DownloadProtocol::FTP;

	} else if (url.left(strlen("file://")) == "file://") {
		protocol = DownloadProtocol::File;

	} else {
		qDebug() << SG_PREFIX_E << "Unsupported protocol in" << url;
		protocol = DownloadProtocol::Unknown;
	}

	return protocol;
}
