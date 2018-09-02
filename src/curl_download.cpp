/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2007, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2009-2010, Jocelyn Jaubert <jocelyn.jaubert@gmail.com>
 * Copyright (C) 2010, Sven Wegener <sven.wegener@stealer.net>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdio>
#include <cstring>
#include <cstdlib>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>

#include <curl/curl.h>

#include <QDebug>

#include "background.h"
#include "dir.h"
#include "file.h"
#include "curl_download.h"




using namespace SlavGPS;




#define SG_MODULE "Curl Download"




static QString curl_download_user_agent;
static CurlDownloadStatus report_post_download_status(CURL * curl, CURLcode ret, const QString & full_url);
static void apply_dl_options(CURL * curl, const DownloadOptions * dl_options, const CurlOptions * curl_options, struct curl_slist ** curl_send_headers);




/*
 * Even if writing to FILE* is supported by libcurl by default,
 * it seems that it is non-portable (win32 DLL specific).
 *
 * So, we provide our own trivial CURLOPT_WRITEFUNCTION.
 */
static size_t curl_write_func(void * ptr, size_t size, size_t nmemb, FILE * stream)
{
	return fwrite(ptr, size, nmemb, stream);
}




static size_t curl_write_tmp_func(void * ptr, size_t size, size_t nmemb, FILE * stream)
{
	QTemporaryFile * file = (QTemporaryFile *) stream;
	return file->write((const char *) ptr, size * nmemb);
}




static size_t curl_get_etag_func(void * ptr, size_t size, size_t nmemb, void * stream)
{
#define ETAG_KEYWORD "ETag: "
#define ETAG_LEN (sizeof(ETAG_KEYWORD)-1)
	CurlOptions * curl_options = (CurlOptions *) stream;
	size_t len = size * nmemb;
	char * str = g_strstr_len((const char *) ptr, len, ETAG_KEYWORD);
	if (str) {
		char * etag_str = str + ETAG_LEN;
		char * end_str = g_strstr_len(etag_str, len - ETAG_LEN, "\r\n");
		if (etag_str && end_str) {
			curl_options->new_etag = g_strndup(etag_str, end_str - etag_str);
			qDebug().nospace() << "DD: Curl Download: Get Etag: ETAG found: '" << curl_options->new_etag << "'";
		}
	}
	return nmemb;
}




static int curl_progress_func(void * clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
	return (int) Background::test_global_termination_condition();
}




/* This should to be called from main() to make sure thread safe. */
void CurlDownload::init(void)
{
	curl_global_init(CURL_GLOBAL_ALL);
	curl_download_user_agent = QString("%1/%2 %3").arg(PACKAGE).arg(VERSION).arg(curl_version());
}




/* This should to be called from main() to make sure thread safe. */
void CurlDownload::uninit(void)
{
	curl_global_cleanup();
}




CurlDownloadStatus CurlHandle::download_uri(const QString & full_url, FILE * file, const DownloadOptions * dl_options, CurlOptions * curl_options)
{
	struct curl_slist * curl_send_headers = NULL;

	qDebug() << SG_PREFIX_D << "Download URL" << full_url;

	/* Allocate curl handle locally if necessary. */
	CURL * curl = (this->curl_handle) ? this->curl_handle : curl_easy_init();
	if (!curl) {
		return CurlDownloadStatus::Error;
	}
	if (1 /* vik_verbose */) {
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
	}
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1); /* Yep, we're a multi-threaded program so don't let signals mess it up! */
	if (dl_options != NULL && !dl_options->user_pass.isEmpty()) {
		curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
		/* "Strings passed to libcurl as 'char *' arguments, are copied by the library;" */
		curl_easy_setopt(curl, CURLOPT_USERPWD, dl_options->user_pass.toUtf8().constData());
	}

	/* "Strings passed to libcurl as 'char *' arguments, are copied by the library;" */
	curl_easy_setopt(curl, CURLOPT_URL, full_url.toUtf8().constData());
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_func);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
	curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, NULL);
	curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, curl_progress_func);
	apply_dl_options(curl, dl_options, curl_options, &curl_send_headers);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, curl_download_user_agent.toUtf8().constData());

	const CURLcode ret = curl_easy_perform(curl);
	const CurlDownloadStatus status = report_post_download_status(curl, ret, full_url);

	if (!this->curl_handle) {
		/* Deallocate curl handle, but only the one that we have allocated locally. */
		curl_easy_cleanup(curl);
	}
	if (curl_send_headers) {
		curl_slist_free_all(curl_send_headers);
		curl_send_headers = NULL;
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, NULL); /* TODO_LATER: we may be setting opt on curl handler that we may have cleaned up above. */
	}

	return status;
}




CurlDownloadStatus CurlHandle::download_uri(const QString & full_url, QTemporaryFile * file, const DownloadOptions * dl_options, const CurlOptions * curl_options)
{
	struct curl_slist * curl_send_headers = NULL;

	qDebug() << SG_PREFIX_D << "Download URL" << full_url;

	/* Allocate curl handle locally if necessary. */
	CURL * curl = (this->curl_handle) ? this->curl_handle : curl_easy_init();
	if (!curl) {
		return CurlDownloadStatus::Error;
	}
	if (1 /* vik_verbose */) {
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
	}
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1); /* Yep, we're a multi-threaded program so don't let signals mess it up! */
	if (dl_options != NULL && !dl_options->user_pass.isEmpty()) {
		curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
		/* "Strings passed to libcurl as 'char *' arguments, are copied by the library;" */
		curl_easy_setopt(curl, CURLOPT_USERPWD, dl_options->user_pass.toUtf8().constData());
	}

	/* "Strings passed to libcurl as 'char *' arguments, are copied by the library;" */
	curl_easy_setopt(curl, CURLOPT_URL, full_url.toUtf8().constData());
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_tmp_func);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
	curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, NULL);
	curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, curl_progress_func);
	apply_dl_options(curl, dl_options, curl_options, &curl_send_headers);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, curl_download_user_agent.toUtf8().constData());

	const CURLcode ret = curl_easy_perform(curl);
	const CurlDownloadStatus status = report_post_download_status(curl, ret, full_url);

	file->reset(); /* Reset file position. */
	file->unsetError();

	if (!this->curl_handle) {
		/* Deallocate curl handle, but only the one that we have allocated locally. */
		curl_easy_cleanup(curl);
	}
	if (curl_send_headers) {
		curl_slist_free_all(curl_send_headers);
		curl_send_headers = NULL;
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, NULL); /* TODO_LATER: we may be setting opt on curl handler that we may have cleaned up above. */
	}

	return status;
}




static CurlDownloadStatus report_post_download_status(CURL * curl, CURLcode ret, const QString & full_url)
{
	CurlDownloadStatus status;

	long response_code = 0;
	double content_length = 0.0;
	double size_download = 0.0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
	curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &content_length);
	curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &size_download);
	qDebug() << SG_PREFIX_I << "Full URL =" << full_url;
	qDebug() << SG_PREFIX_I << "Response code =" << response_code;
	qDebug() << SG_PREFIX_I << "Content-length =" << content_length;
	qDebug() << SG_PREFIX_I << "Size download =" << size_download;

	if (CURLE_OK == ret) {
		qDebug() << SG_PREFIX_I << "Curl operation successful";

		switch (response_code) {
		case 304: /* Not Modified */
			status = CurlDownloadStatus::NoNewerFile;
			break;
		case 200: /* http OK */
		case 226: /* ftp Success */
			/* Verify if curl sends us any data - this is a workaround on using CURLOPT_TIMECONDITION
			   when the server has a (incorrect) time earlier than the time on the file we already have. */
			if (content_length < 0.1 && size_download < 0.1) {
				status = CurlDownloadStatus::Error;
			} else {
				status = CurlDownloadStatus::NoError;
			}
			break;
		default:
			status = CurlDownloadStatus::Error;
			break;
		}
	} else {
		qDebug() << SG_PREFIX_I << "Curl operation failed";
		status = CurlDownloadStatus::Error;
	}

	return status;
}




static void apply_dl_options(CURL * curl, const DownloadOptions * dl_options, const CurlOptions * curl_options, struct curl_slist ** curl_send_headers)
{
	if (dl_options == NULL) {
		return;
	}

	if (!dl_options->referer.isEmpty()) {
		curl_easy_setopt(curl, CURLOPT_REFERER, dl_options->referer.toUtf8().constData());
	}

	if (dl_options->follow_location != 0) {
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
		curl_easy_setopt(curl, CURLOPT_MAXREDIRS, dl_options->follow_location);
	}
	if (curl_options != NULL) {
		if (dl_options->check_file_server_time && curl_options->time_condition != 0) {
			/* If file exists, check against server if file is recent enough. */
			curl_easy_setopt(curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
			curl_easy_setopt(curl, CURLOPT_TIMEVALUE, curl_options->time_condition);
		}
		if (dl_options->use_etag) {
			if (curl_options->etag != NULL) {
				/* Add an header on the HTTP request. */
				char str[60];
				snprintf(str, 60, "If-None-Match: %s", curl_options->etag);
				*curl_send_headers = curl_slist_append(*curl_send_headers, str);
				curl_easy_setopt(curl, CURLOPT_HTTPHEADER, *curl_send_headers);
			}
			/* Store the new etag from the server in an option value. */
			curl_easy_setopt(curl, CURLOPT_WRITEHEADER, curl_options);
			curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_get_etag_func);
		}
	}

	return;
}




CurlDownloadStatus CurlHandle::get_url(const QString & hostname, const QString & uri, FILE * file, const DownloadOptions * dl_options, bool ftp, CurlOptions * curl_options)
{
	QString full_url;

	if (hostname.contains("://")) {
		/* Already full url. */
		full_url = hostname;
	} else if (uri.contains("://")) {
		/* Already full url. */
		full_url = uri;
	} else {
		/* Compose the full url. */
		full_url = QString("%1://%2%3").arg(ftp ? "ftp" : "http").arg(hostname).arg(uri);
	}

	return this->download_uri(full_url, file, dl_options, curl_options);
}




CurlHandle::CurlHandle()
{
	this->curl_handle = curl_easy_init();
	qDebug() << SG_PREFIX_D << "Initialized curl handle" << QString("%1").arg((quintptr) this->curl_handle, 0, 16);
}




CurlHandle::~CurlHandle()
{
	qDebug() << SG_PREFIX_D << "Cleaning curl handle" << QString("%1").arg((quintptr) this->curl_handle, 0, 16);
	curl_easy_cleanup(this->curl_handle);
}




CurlOptions::~CurlOptions()
{
	if (this->etag) {
		free(this->etag);
		this->etag = NULL;
	}
	if (this->new_etag) {
		free(this->new_etag);
		this->etag = NULL;
	}
}
