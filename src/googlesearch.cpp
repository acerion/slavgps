/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>

#include "googlesearch.h"
#include "util.h"




using namespace SlavGPS;




#define GOOGLE_GOTO_URL_FMT "http://maps.google.com/maps?q=%s&output=js"
#define GOOGLE_GOTO_PATTERN_1 "{center:{lat:"
#define GOOGLE_GOTO_PATTERN_2 ",lng:"
#define GOOGLE_GOTO_NOT_FOUND "not understand the location"




GotoToolGoogle::GotoToolGoogle() : GotoTool("Google")
{
	this->dl_options.referer = (char *) "http://maps.google.com/";
	this->dl_options.follow_location = 2;
	this->dl_options.check_file = a_check_map_file;
}




GotoToolGoogle::~GotoToolGoogle()
{
}




bool GotoToolGoogle::parse_file_for_latlon(char * file_name, LatLon & lat_lon)
{
	char * s = NULL;
	char lat_buf[32] = { 0 };
	char lon_buf[32] = { 0 };

	GMappedFile *mf;
	if ((mf = g_mapped_file_new(file_name, false, NULL)) == NULL) {
		fprintf(stderr, _("CRITICAL: couldn't map temp file\n"));
		return false;
	}
	size_t len = g_mapped_file_get_length(mf);
	char * text = g_mapped_file_get_contents(mf);

	bool found = true;
	if (g_strstr_len(text, len, GOOGLE_GOTO_NOT_FOUND) != NULL) {
		found = false;
		goto done;
	}

	char * pat;
	if ((pat = g_strstr_len(text, len, GOOGLE_GOTO_PATTERN_1)) == NULL) {
		found = false;
		goto done;
	}
	pat += strlen(GOOGLE_GOTO_PATTERN_1);
	s = lat_buf;
	if (*pat == '-') {
		*s++ = *pat++;
	}
	while ((s < (lat_buf + sizeof(lat_buf))) && (pat < (text + len)) &&
	       (g_ascii_isdigit(*pat) || (*pat == '.'))) {
		*s++ = *pat++;
	}

	*s = '\0';
	if ((pat >= (text + len)) || (lat_buf[0] == '\0')) {
		found = false;
		goto done;
	}

	if (strncmp(pat, GOOGLE_GOTO_PATTERN_2, strlen(GOOGLE_GOTO_PATTERN_2))) {
		found = false;
		goto done;
	}

	pat += strlen(GOOGLE_GOTO_PATTERN_2);
	s = lon_buf;

	if (*pat == '-') {
		*s++ = *pat++;
	}

	while ((s < (lon_buf + sizeof(lon_buf))) && (pat < (text + len)) &&
	       (g_ascii_isdigit(*pat) || (*pat == '.'))) {

		*s++ = *pat++;
	}

	*s = '\0';
	if ((pat >= (text + len)) || (lon_buf[0] == '\0')) {
		found = false;
		goto done;
	}

	lat_lon.lat = g_ascii_strtod(lat_buf, NULL);
	lat_lon.lon = g_ascii_strtod(lon_buf, NULL);

 done:
	g_mapped_file_unref(mf);
	return (found);

}




char * GotoToolGoogle::get_url_format()
{
	return (char *) GOOGLE_GOTO_URL_FMT;
}




const DownloadOptions * GotoToolGoogle::get_download_options(void) const
{
	return &this->dl_options;
}
