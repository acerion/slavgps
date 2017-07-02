/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _SG_CURL_DOWNLOAD_H_
#define _SG_CURL_DOWNLOAD_H_




#include <cstdio>
#include <cstdint>

#include <QString>

#include "download.h"




namespace SlavGPS {




	/* Error messages returned by download functions. */
	enum class CurlDownloadStatus {
		NO_ERROR = 0,
		NO_NEWER_FILE,
		ERROR
	};




	class CurlOptions {
	public:
		CurlOptions() {};
		~CurlOptions();

		/* Time sent to server on header If-Modified-Since. */
		time_t time_condition = 0;

		/* Etag sent by server on previous download. */
		char * etag = NULL;

		/* Etag sent by server on this download. */
		char * new_etag = NULL;
	} ;




	class CurlDownload {
	public:
		static void init(void);
		static void uninit(void);

		static void * init_handle(void);
		static void uninit_handle(void * handle);

		static CurlDownloadStatus get_url(const QString & hostname, const QString & uri, FILE * f, const DownloadOptions * dl_options, bool ftp, CurlOptions * curl_options, void * handle);
		static CurlDownloadStatus download_uri(const QString & full_url, FILE * f, const DownloadOptions * dl_options, CurlOptions * curl_options, void * handle);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_CURL_DOWNLOAD_H_ */
