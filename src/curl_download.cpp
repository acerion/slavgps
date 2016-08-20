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
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>

#include <curl/curl.h>

#include "background.h"
#include "dir.h"
#include "file.h"
#include "globals.h"
#include "curl_download.h"




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
	CurlDownloadOptions *cdo = (CurlDownloadOptions *) stream;
	size_t len = size * nmemb;
	char * str = g_strstr_len((const char *) ptr, len, ETAG_KEYWORD);
	if (str) {
		char * etag_str = str + ETAG_LEN;
		char * end_str = g_strstr_len(etag_str, len - ETAG_LEN, "\r\n");
		if (etag_str && end_str) {
			cdo->new_etag = g_strndup(etag_str, end_str - etag_str);
			fprintf(stderr, "DEBUG: %s: ETAG found: %s\n", __FUNCTION__, cdo->new_etag);
		}
	}
	return nmemb;
}




static int curl_progress_func(void * clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
	return a_background_testcancel(NULL);
}




/* This should to be called from main() to make sure thread safe. */
void curl_download_init()
{
	curl_global_init(CURL_GLOBAL_ALL);
	curl_download_user_agent = g_strdup_printf ("%s/%s %s", PACKAGE, VERSION, curl_version());
}




/* This should to be called from main() to make sure thread safe. */
void curl_download_uninit()
{
	curl_global_cleanup();
}




int curl_download_uri(const char * uri, FILE * f, DownloadFileOptions * options, CurlDownloadOptions * cdo, void * handle)
{
	CURL * curl;
	struct curl_slist * curl_send_headers = NULL;
	CURLcode res = CURLE_FAILED_INIT;

	//fprintf(stderr, "DEBUG: %s: uri=%s\n", __PRETTY_FUNCTION__, uri);

	curl = handle ? handle : curl_easy_init ();
	if (!curl) {
		return CURL_DOWNLOAD_ERROR;
	}
	if (vik_verbose) {
		curl_easy_setopt (curl, CURLOPT_VERBOSE, 1);
	}
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1); /* Yep, we're a multi-threaded program so don't let signals mess it up! */
	if (options != NULL && options->user_pass) {
		curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
		curl_easy_setopt(curl, CURLOPT_USERPWD, options->user_pass);
	}
	curl_easy_setopt(curl, CURLOPT_URL, uri);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_func);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
	curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, NULL);
	curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, curl_progress_func);
	if (options != NULL) {
		if(options->referer != NULL) {
			curl_easy_setopt(curl, CURLOPT_REFERER, options->referer);
		}

		if(options->follow_location != 0) {
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, options->follow_location);
		}
		if (cdo != NULL) {
			if (options->check_file_server_time && cdo->time_condition != 0) {
				/* If file exists, check against server if file is recent enough. */
				curl_easy_setopt(curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
				curl_easy_setopt(curl, CURLOPT_TIMEVALUE, cdo->time_condition);
			}
			if (options->use_etag) {
				if (cdo->etag != NULL) {
					/* Add an header on the HTTP request. */
					char str[60];
					snprintf(str, 60, "If-None-Match: %s", cdo->etag);
					curl_send_headers = curl_slist_append(curl_send_headers, str);
					curl_easy_setopt(curl, CURLOPT_HTTPHEADER , curl_send_headers);
				}
				/* Store the new etag from the server in an option value. */
				curl_easy_setopt(curl, CURLOPT_WRITEHEADER, cdo);
				curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_get_etag_func);
			}
		}
	}
	curl_easy_setopt(curl, CURLOPT_USERAGENT, curl_download_user_agent);
	res = curl_easy_perform(curl);

	if (res == 0) {
		glong response;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
		if (response == 304) {         // 304 = Not Modified
			res = (CURLcode) CURL_DOWNLOAD_NO_NEWER_FILE;

		} else if (response == 200        /* http: 200 = Ok */
			   || response == 226) {  /* ftp:  226 = success */
			double size;
			/* Verify if curl sends us any data - this is a workaround on using CURLOPT_TIMECONDITION
			   when the server has a (incorrect) time earlier than the time on the file we already have. */
			curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &size);
			if (size == 0) {
				res = (CURLcode) CURL_DOWNLOAD_ERROR;
			} else {
				res = (CURLcode) CURL_DOWNLOAD_NO_ERROR;
			}
		} else {
			fprintf(stderr, "WARNING: %s: http response: %ld for uri %s\n", __FUNCTION__, response, uri);
			res = (CURLcode) CURL_DOWNLOAD_ERROR;
		}
	} else {
		res = (CURLcode) CURL_DOWNLOAD_ERROR;
	}
	if (!handle) {
		curl_easy_cleanup(curl);
	}
	if (curl_send_headers) {
		curl_slist_free_all(curl_send_headers);
		curl_send_headers = NULL;
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER , NULL);
	}
	return res;
}




int curl_download_get_url(const char * hostname, const char * uri, FILE * f, DownloadFileOptions * options, bool ftp, CurlDownloadOptions * cdo, void * handle)
{
	int ret;
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
	ret = curl_download_uri(full, f, options, cdo, handle);
	/* Free newly allocated memory, but do not free uri. */
	if (hostname != full && uri != full) {
		free(full);
	}
	full = NULL;

	return ret;
}




void * curl_download_handle_init()
{
	return curl_easy_init();
}




void curl_download_handle_cleanup(void * handle)
{
	curl_easy_cleanup(handle);
}
