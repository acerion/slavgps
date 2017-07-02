/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2007, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#ifndef _SG_DOWNLOAD_H_
#define _SG_DOWNLOAD_H_




#include <cstdio>
#include <cstdint>
#include <string>

#include <QString>




namespace SlavGPS {




	enum class DownloadResult {
		FILE_WRITE_ERROR = -4, /* Can't write downloaded file :( */
		HTTP_ERROR       = -2,
		CONTENT_ERROR    = -1,
		SUCCESS          =  0,
		NOT_REQUIRED     =  1, /* Also 'successful'. e.g. because file already exists and no time checks used. */
	};




	/* File content check. */
	typedef bool (* VikFileContentCheckerFunc) (FILE *);
	bool a_check_map_file(FILE *);
	bool a_check_html_file(FILE *);
	bool a_check_kml_file(FILE *);

	/* Convert. */
	void a_try_decompress_file(char * name);
	typedef void (* VikFileContentConvertFunc) (char *); /* filename (temporary). */




	class DownloadOptions {

	public:
		DownloadOptions() {};
		DownloadOptions(long follows) { follow_location = follows; };

		/* Check if the server has a more recent file than the
		   one we have before downloading it. This uses http
		   header If-Modified-Since. */
		bool check_file_server_time = false;

		/* Set if the server handle ETag. */
		bool use_etag = false;

		/* The REFERER string to use. Could be NULL. */
		char * referer = NULL;

		/* follow_location specifies the number of retries to
		   follow a redirect while downloading a page. */
		long follow_location = 0;

		/* File content checker. */
		VikFileContentCheckerFunc check_file = NULL;

		/* If need to authenticate on download. Format: 'username:password' */
		char * user_pass = NULL;

		/* File manipulation if necessary such as
		   uncompressing the downloaded file. */
		VikFileContentConvertFunc convert_file = NULL;
	};




	class Download {
	public:
		static void init(void);
		/* There is no uninit() function (kamilTODO: perhaps there should be one, for the handle). */

		static void * init_handle(void);
		static void uninit_handle(void * handle);

		static DownloadResult get_url_http(const QString & hostname, const QString & uri, const std::string & fn, const DownloadOptions * dl_options, void * handle);
		static DownloadResult get_url_ftp(const QString & hostname, const QString & uri, const std::string & fn, const DownloadOptions * dl_options, void * handle);

		static char * get_uri_to_tmp_file(const QString & uri, const DownloadOptions * dl_options);
	};






} /* namespace SlavGPS */




#endif /* #ifndef _SG_DOWNLOAD_H_ */
