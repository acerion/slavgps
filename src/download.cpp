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
#include <cctype>
#include <cstring>

#include <errno.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_UTIME_H
#include <utime.h>
#endif
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#if 1 //#ifdef HAVE_MAGIC_H // kamilFIXME: check dependency during configuration
#include <magic.h>
#endif

#include "compression.h"
#include "download.h"
#include "curl_download.h"
#include "uibuilder.h"
//#include "preferences.h"
#include "globals.h"
//#include "vik_compat.h"




using namespace SlavGPS;




static bool check_file_first_line(FILE * f, char * patterns[])
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
	for (char ** s = patterns; *s; s++) {
		if (strncasecmp(*s, bp, strlen(*s)) == 0) {
			return true;
		}
	}
	return false;
}




bool a_check_html_file(FILE * f)
{
	char * html_str[] = {
		(char *) "<html",
		(char *) "<!DOCTYPE html",
		(char *) "<head",
		(char *) "<title",
		NULL
	};

	return check_file_first_line(f, html_str);
}




bool a_check_map_file(FILE * f)
{
	/* FIXME no more true since a_check_kml_file. */
	return !a_check_html_file(f);
}




bool a_check_kml_file(FILE* f)
{
	char * kml_str[] = {
		(char *) "<?xml",
		NULL
	};

	return check_file_first_line(f, kml_str);
}




static GList *file_list = NULL;
static GMutex *file_list_mutex = NULL;




/* Spin button scales. */
static LayerParamScale params_scales[] = {
	{1, 365, 1, 0},		/* download_tile_age */
};




static LayerParamData convert_to_display(LayerParamData value)
{
	/* From seconds into days. */
	return VIK_LPD_UINT (value.u / 86400);
}




static LayerParamData convert_to_internal(LayerParamData value)
{
	/* From days into seconds. */
	return VIK_LPD_UINT (86400 * value.u);
}




static LayerParam prefs[] = {
	{ LayerType::NUM_TYPES, 0, VIKING_PREFERENCES_NAMESPACE "download_tile_age", LayerParamType::UINT, VIK_LAYER_GROUP_NONE, N_("Tile age (days):"), LayerWidgetType::SPINBUTTON, &params_scales[0], NULL, NULL, NULL, convert_to_display, convert_to_internal },
};




void a_download_init(void)
{
	LayerParamData tmp;
	tmp.u = VIK_CONFIG_DEFAULT_TILE_AGE / 86400; /* Now in days. */
#if 0
	a_preferences_register(prefs, tmp, VIKING_PREFERENCES_GROUP_KEY);
	file_list_mutex = vik_mutex_new();
#endif
}




void a_download_uninit(void)
{
#if 0
	vik_mutex_free(file_list_mutex);
#endif
}




static bool lock_file(char const * fn)
{
	bool locked = false;
#if 0
	g_mutex_lock(file_list_mutex);
#endif
	if (g_list_find_custom(file_list, fn, (GCompareFunc)g_strcmp0) == NULL) {
		/* The filename is not yet locked. */
		file_list = g_list_append(file_list, (void *)fn),
			locked = true;
	}
#if 0
	g_mutex_unlock(file_list_mutex);
#endif
	return locked;
}




static void unlock_file(char const * fn)
{
	g_mutex_lock(file_list_mutex);
	file_list = g_list_remove(file_list, (gconstpointer)fn);
	g_mutex_unlock(file_list_mutex);
}




/**
 * Unzip a file - replacing the file with the unzipped contents of the self.
 */
static void uncompress_zip(char * name)
{
	GError * error = NULL;
	GMappedFile * mf;

	if ((mf = g_mapped_file_new(name, false, &error)) == NULL) {
		fprintf(stderr, _("CRITICAL: Couldn't map file %s: %s\n"), name, error->message);
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
	if (!g_file_set_contents(name, (char const *) unzip_mem, ucsize, &error)) {
		fprintf(stderr, "CRITICAL: Couldn't write file '%s', because of %s\n", name, error->message);
		g_error_free(error);
	}
}



/**
 * @name:  The potentially compressed filename
 *
 * Perform magic to decide how which type of decompression to attempt.
 */
void a_try_decompress_file(char * name)
{
#ifdef HAVE_MAGIC_H
#ifdef MAGIC_VERSION
	/* Or magic_version() if available - probably need libmagic 5.18 or so
	   (can't determine exactly which version the versioning became available). */
	fprintf(stderr, "DEBUG: %s: magic version: %d\n", __FUNCTION__, MAGIC_VERSION);
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
			char const * magic = magic_file(myt, name);
			fprintf(stderr, "DEBUG: %s: magic output: %s\n", __FUNCTION__, magic);

			if (g_ascii_strncasecmp(magic, "application/zip", 15) == 0) {
				zip = true;
			}

			if (g_ascii_strncasecmp(magic, "application/x-bzip2", 19) == 0) {
				bzip2 = true;
			}
		} else {
			fprintf(stderr, "CRITICAL: %s: magic load database failure\n", __FUNCTION__);
		}

		magic_close(myt);
	}

	if (!(zip || bzip2)) {
		return;
	}

	if (zip) {
		uncompress_zip (name);
	} else if (bzip2) {
		char* bz2_name = uncompress_bzip2 (name);
		if (bz2_name) {
			if (remove(name)) {
				fprintf(stderr, "CRITICAL: %s: remove file failed [%s]\n", __FUNCTION__, name);
			}
			if (g_rename (bz2_name, name)) {
				fprintf(stderr, "CRITICAL: %s: file rename failed [%s] to [%s]\n", __FUNCTION__, bz2_name, name);
			}
		}
	}

	return;
#endif
}




#define VIKING_ETAG_XATTR "xattr::viking.etag"




static bool get_etag_xattr(char const * fn, CurlDownloadOptions * cdo)
{
	bool result = false;

	GFile * file = g_file_new_for_path(fn);
	GFileInfo * fileinfo = g_file_query_info(file, VIKING_ETAG_XATTR, G_FILE_QUERY_INFO_NONE, NULL, NULL);
	if (fileinfo) {
		char const * etag = g_file_info_get_attribute_string(fileinfo, VIKING_ETAG_XATTR);
		if (etag) {
			cdo->etag = g_strdup(etag);
			result = !!cdo->etag;
		}
		g_object_unref(fileinfo);
	}
	g_object_unref(file);

	if (result) {
		fprintf(stderr, "DEBUG: %s: Get etag (xattr) from %s: %s\n", __FUNCTION__, fn, cdo->etag);
	}

	return result;
}




static bool get_etag_file(char const * fn, CurlDownloadOptions * cdo)
{
	bool result = false;

	char * etag_filename = g_strdup_printf("%s.etag", fn);
	if (etag_filename) {
		result = g_file_get_contents(etag_filename, &cdo->etag, NULL, NULL);
		free(etag_filename);
	}

	if (result) {
		fprintf(stderr, "DEBUG: %s: Get etag (file) from %s: %s\n", __FUNCTION__, fn, cdo->etag);
	}

	return result;
}




static void get_etag(char const * fn, CurlDownloadOptions * cdo)
{
	/* First try to get etag from xattr, then fall back to plain file. */
	if (!get_etag_xattr(fn, cdo) && !get_etag_file(fn, cdo)) {
		fprintf(stderr, "DEBUG: %s: Failed to get etag from %s\n", __FUNCTION__, fn);
		return;
	}

	/* Check if etag is short enough. */
	if (strlen(cdo->etag) > 100) {
		free(cdo->etag);
		cdo->etag = NULL;
	}

	/* TODO: should check that etag is a valid string. */
}




static bool set_etag_xattr(char const * fn, CurlDownloadOptions * cdo)
{
	bool result = false;
	GFile *file;

	file = g_file_new_for_path(fn);
	result = g_file_set_attribute_string(file, VIKING_ETAG_XATTR, cdo->new_etag, G_FILE_QUERY_INFO_NONE, NULL, NULL);
	g_object_unref(file);

	if (result) {
		fprintf(stderr, "DEBUG: %s: Set etag (xattr) on %s: %s\n", __FUNCTION__, fn, cdo->new_etag);
	}

	return result;
}




static bool set_etag_file(char const * fn, CurlDownloadOptions * cdo)
{
	bool result = false;
	char * etag_filename;

	etag_filename = g_strdup_printf("%s.etag", fn);
	if (etag_filename) {
		result = g_file_set_contents(etag_filename, cdo->new_etag, -1, NULL);
		free(etag_filename);
	}

	if (result) {
		fprintf(stderr, "DEBUG: %s: Set etag (file) on %s: %s\n", __FUNCTION__, fn, cdo->new_etag);
	}

	return result;
}




static void set_etag(char const * fn, char const * fntmp, CurlDownloadOptions * cdo)
{
	/* First try to store etag in extended attribute, then fall back to plain file. */
	if (!set_etag_xattr(fntmp, cdo) && !set_etag_file(fn, cdo)) {
		fprintf(stderr, "DEBUG: %s: Failed to set etag on %s\n", __FUNCTION__, fn);
	}
}




static DownloadResult_t download(char const * hostname, char const * uri, char const * fn, DownloadFileOptions * options, bool ftp, void * handle)
{
	int ret;
	bool failure = false;
	CurlDownloadOptions cdo = {0, NULL, NULL};

	/* Check file. */
	if (g_file_test(fn, G_FILE_TEST_EXISTS) == true) {
		if (options == NULL
		    || (!options->check_file_server_time
			&& !options->use_etag)) {
			/* Nothing to do as file already exists and we don't want to check server. */
			return DOWNLOAD_NOT_REQUIRED;
		}

		time_t tile_age = 365; //a_preferences_get(VIKING_PREFERENCES_NAMESPACE "download_tile_age")->u;
		/* Get the modified time of this file. */
		GStatBuf buf;
		(void)g_stat(fn, &buf);
		time_t file_time = buf.st_mtime;
		if ((time(NULL) - file_time) < tile_age) {
			/* File cache is too recent, so return. */
			return DOWNLOAD_NOT_REQUIRED;
		}

		if (options != NULL && options->check_file_server_time) {
			cdo.time_condition = file_time;
		}
		if (options != NULL && options->use_etag) {
			get_etag(fn, &cdo);
		}

	} else {
		char *dir = g_path_get_dirname(fn);
		if (g_mkdir_with_parents(dir , 0777) != 0) {
			fprintf(stderr, "WARNING: %s: Failed to mkdir %s\n", __FUNCTION__, dir);
		}
		free(dir);
	}

	char * tmpfilename = g_strdup_printf("%s.tmp", fn);
	if (!lock_file(tmpfilename)) {
		fprintf(stderr, "DEBUG: %s: Couldn't take lock on temporary file \"%s\"\n", __FUNCTION__, tmpfilename);
		free(tmpfilename);
		if (options->use_etag) {
			free(cdo.etag);
		}
		return DOWNLOAD_FILE_WRITE_ERROR;
	}

	FILE * f = fopen(tmpfilename, "w+b");  /* Truncate file and open it. */
	if (!f) {
		fprintf(stderr, "WARNING: Couldn't open temporary file \"%s\": %s\n", tmpfilename, g_strerror(errno));
		free(tmpfilename);
		if (options->use_etag) {
			free(cdo.etag);
		}
		return DOWNLOAD_FILE_WRITE_ERROR;
	}

	/* Call the backend function */
	ret = curl_download_get_url(hostname, uri, f, options, ftp, &cdo, handle);

	DownloadResult_t result = DOWNLOAD_SUCCESS;

	if (ret != CURL_DOWNLOAD_NO_ERROR && ret != CURL_DOWNLOAD_NO_NEWER_FILE) {
		fprintf(stderr, "DEBUG: %s: download failed: curl_download_get_url=%d\n", __FUNCTION__, ret);
		failure = true;
		result = DOWNLOAD_HTTP_ERROR;
	}

	if (!failure && options != NULL && options->check_file != NULL && ! options->check_file(f)) {
		fprintf(stderr, "DEBUG: %s: file content checking failed\n", __FUNCTION__);
		failure = true;
		result = DOWNLOAD_CONTENT_ERROR;
	}

	fclose (f);
	f = NULL;

	if (failure) {
		fprintf(stderr, _("WARNING: Download error: %s\n"), fn);
		if (remove(tmpfilename) != 0) {
			fprintf(stderr, _("WARNING: Failed to remove: %s\n"), tmpfilename);
		}
		unlock_file (tmpfilename);
		free(tmpfilename);
		if (options != NULL && options->use_etag) {
			free(cdo.etag);
			free(cdo.new_etag);
		}
		return result;
	}

	if (ret == CURL_DOWNLOAD_NO_NEWER_FILE)  {
		(void) remove(tmpfilename);
		/* Wpdate mtime of local copy.
		   Not security critical, thus potential Time of Check Time of Use race condition is not bad.
		   coverity[toctou] */
		if (g_utime (fn, NULL) != 0)
			fprintf(stderr, "WARNING: %s couldn't set time on: %s\n", __FUNCTION__, fn);
	} else {
		if (options != NULL && options->convert_file) {
			options->convert_file (tmpfilename);
		}

		if (options != NULL && options->use_etag) {
			if (cdo.new_etag) {
				/* Server returned an etag value. */
				set_etag(fn, tmpfilename, &cdo);
			}
		}

		/* Move completely-downloaded file to permanent location. */
		if (g_rename(tmpfilename, fn)) {
			fprintf(stderr, "WARNING: %s: file rename failed [%s] to [%s]\n", __FUNCTION__, tmpfilename, fn);
		}
	}
	unlock_file (tmpfilename);
	free(tmpfilename);

	if (options != NULL && options->use_etag) {
		free(cdo.etag);
		free(cdo.new_etag);
	}
	return DOWNLOAD_SUCCESS;
}




/**
 * uri: like "/uri.html?whatever"
 * Only reason for the "wrapper" is so we can do redirects.
 */
DownloadResult_t a_http_download_get_url(char const * hostname, char const * uri, char const * fn, DownloadFileOptions * opt, void * handle)
{
	return download(hostname, uri, fn, opt, false, handle);
}




DownloadResult_t a_ftp_download_get_url(char const * hostname, char const * uri, char const * fn, DownloadFileOptions * opt, void * handle)
{
	return download(hostname, uri, fn, opt, true, handle);
}




void * a_download_handle_init()
{
	return curl_download_handle_init();
}




void a_download_handle_cleanup(void * handle)
{
	curl_download_handle_cleanup(handle);
}




/**
 * @uri:         The URI (Uniform Resource Identifier)
 * @options:     Download options (maybe NULL)
 *
 * Returns name of the temporary file created - NULL if unsuccessful.
 * This string needs to be freed once used.
 * The file needs to be removed once used.
 */
char * a_download_uri_to_tmp_file(char const * uri, DownloadFileOptions * options)
{
	int tmp_fd;
	char * tmpname;

	if ((tmp_fd = g_file_open_tmp("viking-download.XXXXXX", &tmpname, NULL)) == -1) {
		fprintf(stderr, _("CRITICAL: couldn't open temp file\n"));
		return NULL;
	}

	FILE * tmp_file = fdopen(tmp_fd, "r+");
	if (!tmp_file) {
		return NULL;
	}

	if (curl_download_uri(uri, tmp_file, options, NULL, NULL)) {
		/* Error. */
		fclose (tmp_file);
		(void) remove(tmpname);
		free(tmpname);
		return NULL;
	}
	fclose (tmp_file);

	return tmpname;
}
