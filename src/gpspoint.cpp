/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2012-2013, Rob Norris <rw_norris@hotmail.com>
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

#include <cstdlib>
#include <cassert>
#include <cctype>

#ifdef HAVE_MATH_H
#include <cmath>
#endif

#ifdef HAVE_STRING_H
#include <cstring>
#endif

#include <QDir>

#include "gpspoint.h"
#include "track_internal.h"
#include "waypoint.h"
#include "layer_trw.h"
#include "file.h"
#include "globals.h"
#include "preferences.h"




using namespace SlavGPS;




typedef struct {
	FILE * f;
	bool is_route;
} TP_write_info_type;

//static void a_gpspoint_write_track(const void * id, const Track * trk, FILE * f);
static void a_gpspoint_write_trackpoint(Trackpoint * tp, TP_write_info_type * write_info);
//static void a_gpspoint_write_waypoint(const void * id, const Waypoint * wp, FILE * f);




/* Outline for file gpspoint.c

Reading file:

take a line.
get first tag, if not type, skip it.
if type, record type.  if waypoint list, etc move on. if track, make a new track, make it current track, add it, etc.
if waypoint, read on and store to the waypoint.
if trackpoint, make trackpoint, store to current track (error / skip if none)

*/




/* Thanks to etrex-cache's gpsbabel's gpspoint.c for starting me off! */
#define VIKING_LINE_SIZE 4096
static char line_buffer[VIKING_LINE_SIZE];

#define GPSPOINT_TYPE_NONE 0
#define GPSPOINT_TYPE_WAYPOINT 1
#define GPSPOINT_TYPE_TRACKPOINT 2
#define GPSPOINT_TYPE_ROUTEPOINT 3
#define GPSPOINT_TYPE_TRACK 4
#define GPSPOINT_TYPE_ROUTE 5

static Track * current_track;

static int line_type = GPSPOINT_TYPE_NONE;
static struct LatLon line_latlon;
static char *line_name;
static char *line_comment;
static char *line_description;
static char *line_source;
static char *line_xtype;
static char *line_color;
static int line_name_label = 0;
static int line_dist_label = 0;
static char *line_image;
static char *line_symbol;

static bool line_visible = true;

static bool line_newsegment = false;
static bool line_has_timestamp = false;
static time_t line_timestamp = 0;
static double line_altitude = VIK_DEFAULT_ALTITUDE;



/* Trackpoint's extended attributes. */
static bool line_extended = false;
static double line_speed = NAN;
static double line_course = NAN;
static int line_sat = 0;
static int line_fix_mode = 0;
static double line_hdop = VIK_DEFAULT_DOP;
static double line_vdop = VIK_DEFAULT_DOP;
static double line_pdop = VIK_DEFAULT_DOP;

/* Other possible properties go here. */


static void gpspoint_process_tag(const char *tag, unsigned int len);
static void gpspoint_process_key_and_value(const char *key, unsigned int key_len, const char *value, unsigned int value_len);




static char * slashdup(const QString input)
{
	char * str = strdup(input.toUtf8().constData());

	size_t len = strlen(str);
	size_t need_bs_count = 0;
	for (size_t i = 0; i < len; i++) {
		if (str[i] == '\\' || str[i] == '"') {
			need_bs_count++;
		}
	}

	size_t i, j;
	char * rv = (char *) malloc((len + need_bs_count + 1) * sizeof (char));
	for (i = 0, j = 0; i < len; i++, j++) {
		if (str[i] == '\\' || str[i] == '"') {
			rv[j++] = '\\';
		}
		rv[j] = str[i];
		/* Basic normalization of strings - replace Linefeed and Carriage returns as blanks.
		   Although allowed in GPX Spec - Viking file format can't handle multi-line strings yet... */
		if (str[i] == '\n' || str[i] == '\r') {
			rv[j] = ' ';
		}
	}
	rv[j] = '\0';

	free(str);

	return rv;
}




static char * deslashndup(char const * str, uint16_t len)
{
	uint16_t i, j, bs_count;

	if (len < 1) {
		return NULL;
	}

	for (i = 0, bs_count = 0; i < len; i++) {
		if (str[i] == '\\') {
			bs_count++;
			i++;
		}
	}

	if (str[i - 1] == '\\' && (len == 1 || str[i - 2] != '\\')) {
		bs_count--;
	}

	bool backslash = false;
	uint16_t new_len = len - bs_count;
	char * rv = (char *) malloc((new_len+1) * sizeof (char));
	for (i = 0, j = 0; i < len && j < new_len; i++) {
		if (str[i] == '\\' && !backslash) {
			backslash = true;
		} else {
			rv[j++] = str[i];
			backslash = false;
		}
	}

	rv[new_len] = '\0';
	return rv;
}




static void reset_line(void)
{
	if (line_name) {
		free(line_name);
		line_name = NULL;
	}

	if (line_comment) {
		free(line_comment);
		line_comment = NULL;
	}

	if (line_description) {
		free(line_description);
		line_description = NULL;
	}

	if (line_source) {
		free(line_source);
		line_source = NULL;
	}

	if (line_xtype) {
		free(line_xtype);
		line_xtype = NULL;
	}

	if (line_color) {
		free(line_color);
		line_color = NULL;
	}

	if (line_image) {
		free(line_image);
		line_image = NULL;
	}

	if (line_symbol) {
		free(line_symbol);
		line_symbol = NULL;
	}

	line_type = GPSPOINT_TYPE_NONE;
	line_visible = true;

	line_newsegment = false;
	line_has_timestamp = false;
	line_timestamp = 0;
	line_altitude = VIK_DEFAULT_ALTITUDE;

	line_symbol = NULL;


	/* Trackpoint's extended attributes. */
	line_extended = false;
	line_speed = NAN;
	line_course = NAN;
	line_sat = 0;
	line_fix_mode = 0;
	line_hdop = VIK_DEFAULT_DOP;
	line_vdop = VIK_DEFAULT_DOP;
	line_pdop = VIK_DEFAULT_DOP;


	line_name_label = 0;
	line_dist_label = 0;

	return;
}




/*
 * Returns whether file read was a success.
 * No obvious way to test for a 'gpspoint' file,
 * thus set a flag if any actual tag found during processing of the file.
 */
bool SlavGPS::a_gpspoint_read_file(LayerTRW * trw, FILE * f, char const * dirpath)
{
	CoordMode coord_mode = trw->get_coord_mode();
	char *tag_start, *tag_end;
	assert (f != NULL && trw != NULL);
	line_type = 0;
	line_timestamp = 0;
	line_newsegment = false;
	line_image = NULL;
	line_symbol = NULL;
	current_track = NULL;
	bool have_read_something = false;

	while (fgets(line_buffer, VIKING_LINE_SIZE, f)) {
		bool inside_quote = 0;
		bool backslash = 0;

		line_buffer[strlen(line_buffer) - 1] = '\0'; /* Chop off newline. */

		/* For gpspoint files wrapped inside. */
		if (strlen(line_buffer) >= 13 && strncmp(line_buffer, "~EndLayerData", 13) == 0) {
			/* Even just a blank TRW is ok when in a .vik file. */
			have_read_something = true;
			break;
		}

		/* Each line: nullify stuff, make thing if nes, free name if necessary. */
		tag_start = line_buffer;
		for (;;) {
			/* My addition: find first non-whitespace character. if the null, skip line. */
			while (*tag_start != '\0' && isspace(*tag_start)) {
				tag_start++;
			}

			if (*tag_start == '\0') {
				break;
			}

			if (*tag_start == '#') {
				break;
			}

			tag_end = tag_start;
			if (*tag_end == '"') {
				inside_quote = !inside_quote;
			}
			while (*tag_end != '\0' && (!isspace(*tag_end) || inside_quote)) {
				tag_end++;
				if (*tag_end == '\\' && !backslash) {
					backslash = true;
				} else if (backslash) {
					backslash = false;
				} else if (*tag_end == '"') {
					inside_quote = !inside_quote;
				}
			}

			/* Won't have super massively long strings, so potential truncation in cast is acceptable. */
			unsigned int len = (unsigned int)(tag_end - tag_start);
			gpspoint_process_tag(tag_start, len);

			if (*tag_end == '\0') {
				break;
			} else {
				tag_start = tag_end + 1;
			}
		}


		if (line_type == GPSPOINT_TYPE_WAYPOINT && line_name) {
			have_read_something = true;
			Waypoint * wp = new Waypoint();
			wp->visible = line_visible;
			wp->altitude = line_altitude;
			wp->has_timestamp = line_has_timestamp;
			wp->timestamp = line_timestamp;

			wp->coord = Coord(line_latlon, coord_mode);

			trw->filein_add_waypoint(wp, line_name);
			free(line_name);
			line_name = NULL;

			if (line_comment) {
				wp->set_comment(QString(line_comment));
			}

			if (line_description) {
				wp->set_description(line_description);
			}

			if (line_source) {
				wp->set_source(line_source);
			}

			if (line_xtype) {
				wp->set_type(line_xtype);
			}

			if (line_image) {
				/* Ensure the filename is absolute. */
				if (g_path_is_absolute(line_image)) {
					wp->set_image(line_image);
				} else {
					/* Otherwise create the absolute filename from the directory of the .vik file & and the relative filename. */
					char *full = g_strconcat(dirpath, G_DIR_SEPARATOR_S, line_image, NULL);
					char *absolute = file_realpath_dup(full); // resolved into the canonical name
					wp->set_image(absolute);
					free(absolute);
					free(full);
				}
			}

			if (line_symbol) {
				wp->set_symbol_name(line_symbol);
			}
		} else if ((line_type == GPSPOINT_TYPE_TRACK || line_type == GPSPOINT_TYPE_ROUTE) && line_name) {
			have_read_something = true;
			Track * trk = new Track(line_type == GPSPOINT_TYPE_ROUTE);
			/* NB don't set defaults here as all properties are stored in the GPS_POINT format. */
			// vik_track_set_defaults (pl);

			/* Thanks to Peter Jones for this Fix. */
			if (!line_name) {
				line_name = strdup("UNK");
			}

			trk->visible = line_visible;

			if (line_comment) {
				trk->set_comment(QString(line_comment));
			}

			if (line_description) {
				trk->set_description(QString(line_description));
			}

			if (line_source) {
				trk->set_source(line_source);
			}

			if (line_xtype) {
				trk->set_type(line_xtype);
			}
#ifdef K
			if (line_color) {
				if (gdk_color_parse(line_color, &(trk->color))) {
					trk->has_color = true;
				}
			}
#endif

			trk->draw_name_mode = (TrackDrawNameMode) line_name_label;
			trk->max_number_dist_labels = line_dist_label;

			/* trk->trackpoints = NULL; */ /* kamilTODO: why it was here? */
			trw->filein_add_track(trk, line_name);
			free(line_name);
			line_name = NULL;

			current_track = trk;
		} else if ((line_type == GPSPOINT_TYPE_TRACKPOINT || line_type == GPSPOINT_TYPE_ROUTEPOINT) && current_track) {
			have_read_something = true;

			Trackpoint * tp = new Trackpoint();
			tp->coord = Coord(line_latlon, coord_mode);
			tp->newsegment = line_newsegment;
			tp->has_timestamp = line_has_timestamp;
			tp->timestamp = line_timestamp;
			tp->altitude = line_altitude;
			tp->set_name(line_name);

			/* Trackpoint's extended attributes. */
			if (line_extended) {
				tp->speed = line_speed;
				tp->course = line_course;
				tp->nsats = line_sat;
				tp->fix_mode = (GPSFixMode) line_fix_mode;
				tp->hdop = line_hdop;
				tp->vdop = line_vdop;
				tp->pdop = line_pdop;
			}

			current_track->trackpoints.push_back(tp);
		}

		reset_line();
	}

	return have_read_something;
}




/* Tag will be of a few defined forms:
   ^[:alpha:]*=".*"$
   ^[:alpha:]*=.*$

   <invalid tag>

   So we must determine end of tag name, start of value, end of value.
*/
static void gpspoint_process_tag(char const * tag, unsigned int len)
{
	char const * value_start = NULL;
	char const * value_end = NULL;

	/* Searching for key end. */
	char const * key_end = tag;

	while (++key_end - tag < len) {
		if (*key_end == '=') {
			break;
		}
	}

	if (key_end - tag == len) {
		return; /* No good. */
	}

	if (key_end - tag == len + 1) {
		value_start = value_end = 0; /* size = 0 */
	} else {
		value_start = key_end + 1; /* equal_sign plus one. */

		if (*value_start == '"') {
			value_start++;
			if (*value_start == '"') {
				value_start = value_end = 0; /* size = 0 */
			} else {
				if (*(tag+len-1) == '"') {
					value_end = tag + len - 1;
				} else {
					return; /* Bogus. */
				}
			}
		} else {
			value_end = tag + len; /* Value start really IS value start. */
		}

		/* Detect broken lines which end without any text or the enclosing ". i.e. like: comment=" */
		if ((value_end - value_start) < 0) {
			return;
		}

		gpspoint_process_key_and_value(tag, key_end - tag, value_start, value_end - value_start);
	}
}




/*
  value = NULL for none.
*/
static void gpspoint_process_key_and_value(char const * key, unsigned int key_len, char const * value, unsigned int value_len)
{
	if (key_len == 4 && strncasecmp(key, "type", key_len) == 0) {
		if (value == NULL) {
			line_type = GPSPOINT_TYPE_NONE;
		} else if (value_len == 5 && strncasecmp(value, "track", value_len) == 0) {
			line_type = GPSPOINT_TYPE_TRACK;
		} else if (value_len == 10 && strncasecmp(value, "trackpoint", value_len) == 0) {
			line_type = GPSPOINT_TYPE_TRACKPOINT;
		} else if (value_len == 8 && strncasecmp(value, "waypoint", value_len) == 0) {
			line_type = GPSPOINT_TYPE_WAYPOINT;
		} else if (value_len == 5 && strncasecmp(value, "route", value_len) == 0) {
			line_type = GPSPOINT_TYPE_ROUTE;
		} else if (value_len == 10 && strncasecmp(value, "routepoint", value_len) == 0) {
			line_type = GPSPOINT_TYPE_ROUTEPOINT;
		} else {
			/* All others are ignored. */
			line_type = GPSPOINT_TYPE_NONE;
		}

		return;
	}

	if (!value) {
		return;
	}

	if (key_len == 4 && strncasecmp(key, "name", key_len) == 0) {
		if (line_name == NULL) {
			line_name = deslashndup(value, value_len);
		}
	} else if (key_len == 7 && strncasecmp(key, "comment", key_len) == 0) {
		if (line_comment == NULL) {
			line_comment = deslashndup(value, value_len);
		}
	} else if (key_len == 11 && strncasecmp(key, "description", key_len) == 0) {
		if (line_description == NULL) {
			line_description = deslashndup(value, value_len);
		}
	} else if (key_len == 6 && strncasecmp(key, "source", key_len) == 0) {
		if (line_source == NULL) {
			line_source = deslashndup(value, value_len);
		}
	} else if (key_len == 5 && strncasecmp(key, "xtype", key_len) == 0) {
		/* NB using 'xtype' to differentiate from our own 'type' key. */
		if (line_xtype == NULL) {
			line_xtype = deslashndup(value, value_len);
		}
	} else if (key_len == 5 && strncasecmp(key, "color", key_len) == 0) {
		if (line_color == NULL) {
			line_color = deslashndup(value, value_len);
		}
	} else if (key_len == 14 && strncasecmp(key, "draw_name_mode", key_len) == 0) {
		line_name_label = atoi(value);
	} else if (key_len == 18 && strncasecmp(key, "number_dist_labels", key_len) == 0) {
		line_dist_label = atoi(value);
	} else if (key_len == 5 && strncasecmp(key, "image", key_len) == 0) {
		if (line_image == NULL) {
			line_image = deslashndup(value, value_len);
		}
	} else if (key_len == 8 && strncasecmp(key, "latitude", key_len) == 0) {
		line_latlon.lat = g_ascii_strtod(value, NULL);
	} else if (key_len == 9 && strncasecmp(key, "longitude", key_len) == 0) {
		line_latlon.lon = g_ascii_strtod(value, NULL);
	} else if (key_len == 8 && strncasecmp(key, "altitude", key_len) == 0) {
		line_altitude = g_ascii_strtod(value, NULL);
	} else if (key_len == 7 && strncasecmp(key, "visible", key_len) == 0) {
		line_visible = value[0] == 'y' || value[0] == 'Y' || value[0] == 't' || value[0] == 'T';
	} else if (key_len == 6 && strncasecmp(key, "symbol", key_len) == 0) {
		line_symbol = strndup(value, value_len);
	} else if (key_len == 8 && strncasecmp(key, "unixtime", key_len) == 0) {
		line_timestamp = g_ascii_strtod(value, NULL);
		if (line_timestamp != 0x80000000) {
			line_has_timestamp = true;
		}
	} else if (key_len == 10 && strncasecmp(key, "newsegment", key_len) == 0) {
		line_newsegment = true;


	/* Trackpoint's extended attributes. */

	} else if (key_len == 8 && strncasecmp(key, "extended", key_len) == 0) {
		line_extended = true;
	} else if (key_len == 5 && strncasecmp(key, "speed", key_len) == 0){
		line_speed = g_ascii_strtod(value, NULL);
	} else if (key_len == 6 && strncasecmp(key, "course", key_len) == 0) {
		line_course = g_ascii_strtod(value, NULL);
	} else if (key_len == 3 && strncasecmp(key, "sat", key_len) == 0) {
		line_sat = atoi(value);
	} else if (key_len == 3 && strncasecmp(key, "fix", key_len) == 0) {
		line_fix_mode = atoi(value);
	} else if (key_len == 4 && strncasecmp(key, "hdop", key_len) == 0) {
		line_hdop = g_ascii_strtod(value, NULL);
	} else if (key_len == 4 && strncasecmp(key, "vdop", key_len) == 0) {
		line_vdop = g_ascii_strtod(value, NULL);
	} else if (key_len == 4 && strncasecmp(key, "pdop", key_len) == 0) {
		line_pdop = g_ascii_strtod(value, NULL);
	}
}




static void a_gpspoint_write_waypoints(FILE * f, Waypoints & data)
{
	for (auto i = data.begin(); i != data.end(); i++) {

		Waypoint * wp = i->second;

		/* Sanity clauses. */
		if (!wp) {
			continue;
		}

		if (wp->name.isEmpty()) { /* TODO: will this condition ever be true? */
			continue;
		}

		static struct LatLon ll = wp->coord.get_latlon();
		char * s_lat = a_coords_dtostr(ll.lat);
		char * s_lon = a_coords_dtostr(ll.lon);
		char * tmp_name = slashdup(wp->name);
		fprintf(f, "type=\"waypoint\" latitude=\"%s\" longitude=\"%s\" name=\"%s\"", s_lat, s_lon, tmp_name);
		free(tmp_name);
		free(s_lat);
		free(s_lon);

		if (wp->altitude != VIK_DEFAULT_ALTITUDE) {
			char * s_alt = a_coords_dtostr(wp->altitude);
			fprintf(f, " altitude=\"%s\"", s_alt);
			free(s_alt);
		}
		if (wp->has_timestamp) {
			fprintf(f, " unixtime=\"%ld\"", wp->timestamp);
		}

		if (!wp->comment.isEmpty()) {
			char * tmp_comment = slashdup(wp->comment);
			fprintf(f, " comment=\"%s\"", tmp_comment);
			free(tmp_comment);
		}
		if (!wp->description.isEmpty()) {
			char * tmp_description = slashdup(wp->description);
			fprintf(f, " description=\"%s\"", tmp_description);
			free(tmp_description);
		}
		if (!wp->source.isEmpty()) {
			char * tmp_source = slashdup(wp->source);
			fprintf(f, " source=\"%s\"", tmp_source);
			free(tmp_source);
		}
		if (!wp->type.isEmpty()) {
			char * tmp_type = slashdup(wp->type);
			fprintf(f, " xtype=\"%s\"", tmp_type);
			free(tmp_type);
		}
		if (!wp->image.isEmpty()) {
			QString tmp_image;
			QString cwd;
			if (Preferences::get_file_ref_format() == VIK_FILE_REF_FORMAT_RELATIVE) {
				cwd = QDir::currentPath();
				if (!cwd.isEmpty()) {
					tmp_image = file_GetRelativeFilename(cwd, wp->image);
				}
			}

			/* If cwd not available - use image filename as is.
			   This should be an absolute path as set in thumbnails. */
			if (cwd.isEmpty()) {
				tmp_image = QString(slashdup(wp->image));
			}

			if (!tmp_image.isEmpty()) {
				fprintf(f, " image=\"%s\"", tmp_image.toUtf8().constData());
			}
		}
		if (!wp->symbol_name.isEmpty()) {
			/* Due to changes in garminsymbols - the symbol name is now in Title Case.
			   However to keep newly generated .vik files better compatible with older Viking versions.
			   The symbol names will always be lowercase. */
			char * tmp_symbol = g_utf8_strdown(wp->symbol_name.toUtf8().constData(), -1);
			fprintf(f, " symbol=\"%s\"", tmp_symbol);
			free(tmp_symbol);
		}
		if (!wp->visible) {
			fprintf(f, " visible=\"n\"");
		}
		fprintf(f, "\n");
	}
}




static void a_gpspoint_write_trackpoint(Trackpoint * tp, TP_write_info_type * write_info)
{
	static struct LatLon ll = tp->coord.get_latlon();

	FILE * f = write_info->f;

	/* TODO: modify a_coords_dtostr() to accept (optional) buffer
	   instead of doing malloc/free every time. */
	char * s_lat = a_coords_dtostr(ll.lat);
	char * s_lon = a_coords_dtostr(ll.lon);
	fprintf(f, "type=\"%spoint\" latitude=\"%s\" longitude=\"%s\"", write_info->is_route ? "route" : "track", s_lat, s_lon);
	free(s_lat);
	free(s_lon);

	if (!tp->name.isEmpty()) {
		char * name = slashdup(tp->name);
		fprintf(f, " name=\"%s\"", name);
		free(name);
	}

	if (tp->altitude != VIK_DEFAULT_ALTITUDE) {
		char * s_alt = a_coords_dtostr(tp->altitude);
		fprintf(f, " altitude=\"%s\"", s_alt);
		free(s_alt);
	}
	if (tp->has_timestamp) {
		fprintf(f, " unixtime=\"%ld\"", tp->timestamp);
	}

	if (tp->newsegment) {
		fprintf(f, " newsegment=\"yes\"");
	}

	if (!std::isnan(tp->speed) || !std::isnan(tp->course) || tp->nsats > 0) {
		fprintf(f, " extended=\"yes\"");
		if (!std::isnan(tp->speed)) {
			char * s_speed = a_coords_dtostr(tp->speed);
			fprintf(f, " speed=\"%s\"", s_speed);
			free(s_speed);
		}
		if (!std::isnan(tp->course)) {
			char * s_course = a_coords_dtostr(tp->course);
			fprintf(f, " course=\"%s\"", s_course);
			free(s_course);
		}
		if (tp->nsats > 0) {
			fprintf(f, " sat=\"%d\"", tp->nsats);
		}

		if (((int) tp->fix_mode) > 0) {
			fprintf(f, " fix=\"%d\"", (int) tp->fix_mode);
		}

		if (tp->hdop != VIK_DEFAULT_DOP) {
			char * ss = a_coords_dtostr(tp->hdop);
			fprintf(f, " hdop=\"%s\"", ss);
			free(ss);
		}
		if (tp->vdop != VIK_DEFAULT_DOP) {
			char * ss = a_coords_dtostr(tp->vdop);
			fprintf(f, " vdop=\"%s\"", ss);
			free(ss);
		}
		if (tp->pdop != VIK_DEFAULT_DOP) {
			char * ss = a_coords_dtostr(tp->pdop);
			fprintf(f, " pdop=\"%s\"", ss);
			free(ss);
		}
	}
	fprintf(f, "\n");
}




static void a_gpspoint_write_track(FILE * f, Tracks & tracks)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {

		Track * trk = i->second;

		/* Sanity clauses. */
		if (!trk) {
			continue;
		}

		if (trk->name.isEmpty()) { /* TODO: will this condition ever be true? */
			continue;
		}

		char * tmp_name = slashdup(trk->name);
		fprintf(f, "type=\"%s\" name=\"%s\"", trk->type_id == "sg.trw.route" ? "route" : "track", tmp_name);
		free(tmp_name);

		if (!trk->comment.isEmpty()) {
			char * tmp = slashdup(trk->comment);
			fprintf(f, " comment=\"%s\"", tmp);
			free(tmp);
		}

		if (!trk->description.isEmpty()) {
			char * tmp = slashdup(trk->description);
			fprintf(f, " description=\"%s\"", tmp);
			free(tmp);
		}

		if (!trk->source.isEmpty()) {
			char * tmp = slashdup(trk->source);
			fprintf(f, " source=\"%s\"", tmp);
			free(tmp);
		}

		if (!trk->type.isEmpty()) {
			char * tmp = slashdup(trk->type);
			fprintf(f, " xtype=\"%s\"", tmp);
			free(tmp);
		}
#ifdef K
		if (trk->has_color) {
			fprintf(f, " color=#%.2x%.2x%.2x", (int)(trk->color.red/256),(int)(trk->color.green/256),(int)(trk->color.blue/256));
		}

		if (trk->draw_name_mode > 0) {
			fprintf(f, " draw_name_mode=\"%d\"", trk->draw_name_mode);
		}
#endif

		if (trk->max_number_dist_labels > 0) {
			fprintf(f, " number_dist_labels=\"%d\"", trk->max_number_dist_labels);
		}

		if (!trk->visible) {
			fprintf(f, " visible=\"n\"");
		}
		fprintf(f, "\n");

		TP_write_info_type tp_write_info = { f, trk->type_id == "sg.trw.route" };
		for (auto iter = trk->trackpoints.begin(); iter != trk->trackpoints.end(); iter++) {
			a_gpspoint_write_trackpoint(*iter, &tp_write_info);
		}
		fprintf(f, "type=\"%send\"\n", trk->type_id == "sg.trw.route" ? "route" : "track");
	}
}




void SlavGPS::a_gpspoint_write_file(LayerTRW const * trw, FILE *f)
{
	Tracks & tracks = ((LayerTRW *) trw)->get_track_items();
	Tracks & routes = ((LayerTRW *) trw)->get_route_items();
	Waypoints & waypoints = ((LayerTRW *) trw)->get_waypoint_items();

	fprintf(f, "type=\"waypointlist\"\n");
	a_gpspoint_write_waypoints(f, waypoints);

	fprintf(f, "type=\"waypointlistend\"\n");
	a_gpspoint_write_track(f, tracks);
	a_gpspoint_write_track(f, routes);
}
