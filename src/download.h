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
#include <QFile>
#include <QTemporaryFile>




namespace SlavGPS {




	class CurlHandle;




	enum class DownloadResult {
		FileWriteError      = -4, /* Can't write downloaded file. */
		HTTPError           = -2,
		ContentError        = -1,
		Success             =  0,
		DownloadNotRequired =  1, /* Also 'successful'. e.g. because file already exists and no time checks used. */
	};

	QDebug operator<<(QDebug debug, const DownloadResult result);




	/* File content check. */
	typedef bool (* VikFileContentCheckerFunc) (FILE *);
	bool a_check_map_file(FILE *);
	bool a_check_html_file(FILE *);
	bool a_check_kml_file(FILE *);

	/* Convert. */
	void a_try_decompress_file(const QString & file_path);
	typedef void (* VikFileContentConvertFunc) (const QString &); /* Function's argument is path to (temporary) file. */




	class DownloadOptions {

	public:
		DownloadOptions() {};
		DownloadOptions(long follows) { follow_location = follows; };
		DownloadOptions(const DownloadOptions& dl_options);

		/* Check if the server has a more recent file than the
		   one we have before downloading it. This uses http
		   header If-Modified-Since. */
		bool check_file_server_time = false;

		/* Set if the server handle ETag. */
		bool use_etag = false;

		/* The REFERER string to use. Could be NULL. */
		QString referer;

		/* follow_location specifies the number of retries to
		   follow a redirect while downloading a page. */
		long follow_location = 0;

		/* File content checker. */
		VikFileContentCheckerFunc check_file = NULL;

		/* If need to authenticate on download. Format: 'username:password' */
		QString user_pass;

		/* File manipulation if necessary such as
		   uncompressing the downloaded file. */
		VikFileContentConvertFunc convert_file = NULL;
	};




	class DownloadHandle {
	public:
		DownloadHandle();
		DownloadHandle(const DownloadOptions * dl_options);
		~DownloadHandle();

		bool is_valid(void) const;
		void set_options(const DownloadOptions & dl_options);

		DownloadResult get_url_http(const QString & hostname, const QString & uri, const QString & dest_file_path);
		DownloadResult get_url_ftp(const QString & hostname, const QString & uri, const QString & dest_file_path);

		bool download_to_tmp_file(QTemporaryFile & tmp_file, const QString & uri);

		DownloadOptions dl_options;

	private:
		DownloadResult download(const QString & hostname, const QString & uri, const QString & dest_file_path, bool ftp);

		CurlHandle * curl_handle = NULL;
	};




	class Download {
	public:
		static void init(void);
		static void uninit();
	};






} /* namespace SlavGPS */




#endif /* #ifndef _SG_DOWNLOAD_H_ */
