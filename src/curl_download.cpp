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
 *
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
#include "globals.h"
#include "curl_download.h"




using namespace SlavGPS;




char * curl_download_user_agent = NULL;




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
	return a_background_testcancel(NULL);
}




/* This should to be called from main() to make sure thread safe. */
void CurlDownload::init(void)
{
	curl_global_init(CURL_GLOBAL_ALL);
	curl_download_user_agent = g_strdup_printf ("%s/%s %s", PACKAGE, VERSION, curl_version());
}




/* This should to be called from main() to make sure thread safe. */
void CurlDownload::uninit(void)
{
	curl_global_cleanup();
}




CurlDownloadStatus CurlDownload::download_uri(const char * uri, FILE * f, const DownloadOptions * dl_options, CurlOptions * curl_options, void * handle)
{
	struct curl_slist * curl_send_headers = NULL;

	qDebug().nospace() << "DD: Curl Download: Download URI '" << uri << "'";

	CURL * curl = handle ? handle : curl_easy_init();
	if (!curl) {
		return CurlDownloadStatus::ERROR;
	}
	if (1 /* vik_verbose */) {
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
	}
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1); /* Yep, we're a multi-threaded program so don't let signals mess it up! */
	if (dl_options != NULL && dl_options->user_pass) {
		curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
		curl_easy_setopt(curl, CURLOPT_USERPWD, dl_options->user_pass);
	}
	curl_easy_setopt(curl, CURLOPT_URL, uri);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_func);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
	curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, NULL);
	curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, curl_progress_func);
	if (dl_options != NULL) {
		if (dl_options->referer != NULL) {
			curl_easy_setopt(curl, CURLOPT_REFERER, dl_options->referer);
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
					curl_send_headers = curl_slist_append(curl_send_headers, str);
					curl_easy_setopt(curl, CURLOPT_HTTPHEADER , curl_send_headers);
				}
				/* Store the new etag from the server in an option value. */
				curl_easy_setopt(curl, CURLOPT_WRITEHEADER, curl_options);
				curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_get_etag_func);
			}
		}
	}
	curl_easy_setopt(curl, CURLOPT_USERAGENT, curl_download_user_agent);

	CurlDownloadStatus status;

	if (0 == curl_easy_perform(curl)) {
		long response;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
		if (response == 304) {         // 304 = Not Modified
			status = CurlDownloadStatus::NO_NEWER_FILE;

		} else if (response == 200        /* http: 200 = Ok */
			   || response == 226) {  /* ftp:  226 = success */
			double size;
			/* Verify if curl sends us any data - this is a workaround on using CURLOPT_TIMECONDITION
			   when the server has a (incorrect) time earlier than the time on the file we already have. */
			curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &size);
			if (size == 0) {
				status = CurlDownloadStatus::ERROR;
			} else {
				status = CurlDownloadStatus::NO_ERROR;
			}
		} else {
			qDebug().nospace() << "WW: Curl Download: Download URI: http response:" << response << "for URI '" << uri << "'";
			status = CurlDownloadStatus::ERROR;
		}
	} else {
		status = CurlDownloadStatus::ERROR;
	}
	if (!handle) {
		curl_easy_cleanup(curl);
	}
	if (curl_send_headers) {
		curl_slist_free_all(curl_send_headers);
		curl_send_headers = NULL;
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER , NULL);
	}
	return status;
}




CurlDownloadStatus CurlDownload::get_url(const char * hostname, const char * uri, FILE * f, const DownloadOptions * dl_options, bool ftp, CurlOptions * curl_options, void * handle)
{
	char * full = NULL;

	if (strstr(hostname, "://") != NULL) {
		/* Already full url. */
		full = (char *) hostname;
	} else if (strstr(uri, "://") != NULL) {
		/* Already full url. */
		full = (char *) uri;
	} else {
		/* Compose the full url. */
		full = g_strdup_printf("%s://%s%s", (ftp?"ftp":"http"), hostname, uri);
	}
	CurlDownloadStatus ret = CurlDownload::download_uri(full, f, dl_options, curl_options, handle);
	/* Free newly allocated memory, but do not free uri. */
	if (hostname != full && uri != full) {
		free(full);
	}
	full = NULL;

	return ret;
}




void * CurlDownload::init_handle(void)
{
	return curl_easy_init();
}




void CurlDownload::uninit_handle(void * handle)
{
	curl_easy_cleanup(handle);
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
