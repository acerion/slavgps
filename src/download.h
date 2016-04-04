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
 *
 */

#ifndef _VIKING_DOWNLOAD_H
#define _VIKING_DOWNLOAD_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


/* File content check */
typedef bool (*VikFileContentCheckerFunc) (FILE*);
bool a_check_map_file(FILE*);
bool a_check_html_file(FILE*);
bool a_check_kml_file(FILE*);
// Convert
void a_try_decompress_file (char *name);
typedef void (*VikFileContentConvertFunc) (char*); // filename (temporary)

typedef struct {
  /**
   * Check if the server has a more recent file than the one we have before downloading it
   * This uses http header If-Modified-Since
   */
  bool check_file_server_time;

  /**
   * Set if the server handle ETag
   */
  bool use_etag;

  /**
   * The REFERER string to use.
   * Could be NULL.
   */
  char *referer;

  /**
   * follow_location specifies the number of retries
   * to follow a redirect while downloading a page.
   */
  glong follow_location;
  
  /**
   * File content checker.
   */
  VikFileContentCheckerFunc check_file;

  /**
   * If need to authenticate on download
   *  format: 'username:password'
   */
  char *user_pass;

  /**
   * File manipulation if necessary such as uncompressing the downloaded file.
   */
  VikFileContentConvertFunc convert_file;

} DownloadFileOptions;

void a_download_init(void);
void a_download_uninit(void);

typedef enum {
  DOWNLOAD_FILE_WRITE_ERROR = -4, // Can't write downloaded file :(
  DOWNLOAD_HTTP_ERROR = -2,
  DOWNLOAD_CONTENT_ERROR = -1,
  DOWNLOAD_SUCCESS = 0,
  DOWNLOAD_NOT_REQUIRED = 1, // Also 'successful'. e.g. Because file already exists and no time checks used
} DownloadResult_t;

/* TODO: convert to Glib */
DownloadResult_t a_http_download_get_url ( const char *hostname, const char *uri, const char *fn, DownloadFileOptions *opt, void *handle );
DownloadResult_t a_ftp_download_get_url ( const char *hostname, const char *uri, const char *fn, DownloadFileOptions *opt, void *handle );
void *a_download_handle_init ();
void a_download_handle_cleanup ( void *handle );

char *a_download_uri_to_tmp_file ( const char *uri, DownloadFileOptions *options );

#ifdef __cplusplus
}
#endif

#endif
