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

#include <QDebug>

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
#include "layer_trw_track_internal.h"
#include "layer_trw_waypoint.h"
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



#define GPSPOINT_TYPE_NONE 0
#define GPSPOINT_TYPE_WAYPOINT 1
#define GPSPOINT_TYPE_TRACKPOINT 2
#define GPSPOINT_TYPE_ROUTEPOINT 3
#define GPSPOINT_TYPE_TRACK 4
#define GPSPOINT_TYPE_ROUTE 5




class GPSPointParser {
public:
	void reset(void);
	void process_tag(char const * tag, unsigned int len);
	void process_key_and_value(char const * key, unsigned int key_len, char const * value, unsigned int value_len);
	Track * add_track(LayerTRW * trw);
	void add_waypoint(LayerTRW * trw, CoordMode coordinate_mode, const char * dirpath);
	Trackpoint * add_trackpoint(CoordMode coordinate_mode);

	int line_type = GPSPOINT_TYPE_NONE;
	LatLon line_latlon;
	char * line_name = NULL;
	char * line_comment = NULL;
	char * line_description = NULL;
	char * line_source = NULL;
	char * line_xtype = NULL;
	char * line_color = NULL;
	int line_name_label = 0;
	int line_dist_label = 0;
	char * line_image = NULL;
	char * line_symbol = NULL;

	bool line_visible = true;

	bool line_newsegment = false;
	bool line_has_timestamp = false;
	time_t line_timestamp = 0;
	double line_altitude = VIK_DEFAULT_ALTITUDE;

	/* Trackpoint's extended attributes. */
	bool line_extended = false;
	double line_speed = NAN;
	double line_course = NAN;
	int line_sat = 0;
	int line_fix_mode = 0;
	double line_hdop = VIK_DEFAULT_DOP;
	double line_vdop = VIK_DEFAULT_DOP;
	double line_pdop = VIK_DEFAULT_DOP;

	/* Other possible properties go here. */
};





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

static Track * current_track;


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




void GPSPointParser::reset()
{
	if (this->line_name) {
		free(this->line_name);
		this->line_name = NULL;
	}

	if (this->line_comment) {
		free(this->line_comment);
		this->line_comment = NULL;
	}

	if (this->line_description) {
		free(this->line_description);
		this->line_description = NULL;
	}

	if (this->line_source) {
		free(this->line_source);
		this->line_source = NULL;
	}

	if (this->line_xtype) {
		free(this->line_xtype);
		this->line_xtype = NULL;
	}

	if (this->line_color) {
		free(this->line_color);
		this->line_color = NULL;
	}

	if (this->line_image) {
		free(this->line_image);
		this->line_image = NULL;
	}

	if (this->line_symbol) {
		free(this->line_symbol);
		this->line_symbol = NULL;
	}

	/* TODO: add resetting line_latlon. */

	this->line_type = GPSPOINT_TYPE_NONE;
	this->line_visible = true;

	this->line_newsegment = false;
	this->line_has_timestamp = false;
	this->line_timestamp = 0;
	this->line_altitude = VIK_DEFAULT_ALTITUDE;

	this->line_symbol = NULL;


	/* Trackpoint's extended attributes. */
	this->line_extended = false;
	this->line_speed = NAN;
	this->line_course = NAN;
	this->line_sat = 0;
	this->line_fix_mode = 0;
	this->line_hdop = VIK_DEFAULT_DOP;
	this->line_vdop = VIK_DEFAULT_DOP;
	this->line_pdop = VIK_DEFAULT_DOP;


	this->line_name_label = 0;
	this->line_dist_label = 0;

	return;
}




/*
 * Returns whether file read was a success.
 * No obvious way to test for a 'gpspoint' file,
 * thus set a flag if any actual tag found during processing of the file.
 */
bool SlavGPS::a_gpspoint_read_file(FILE * file, LayerTRW * trw, char const * dirpath)
{
	assert (file != NULL && trw != NULL);

	const CoordMode coord_mode = trw->get_coord_mode();

	GPSPointParser point_parser;
	current_track = NULL;

	bool have_read_something = false;

	while (fgets(line_buffer, sizeof (line_buffer), file)) {
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
		char * tag_start = line_buffer;
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

			char * tag_end = tag_start;
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
			point_parser.process_tag(tag_start, len);

			if (*tag_end == '\0') {
				break;
			} else {
				tag_start = tag_end + 1;
			}
		}


		if (point_parser.line_type == GPSPOINT_TYPE_WAYPOINT && point_parser.line_name) {
			have_read_something = true;
			point_parser.add_waypoint(trw, coord_mode, dirpath);

		} else if ((point_parser.line_type == GPSPOINT_TYPE_TRACK || point_parser.line_type == GPSPOINT_TYPE_ROUTE) && point_parser.line_name) {
			have_read_something = true;
			current_track = point_parser.add_track(trw);

		} else if ((point_parser.line_type == GPSPOINT_TYPE_TRACKPOINT || point_parser.line_type == GPSPOINT_TYPE_ROUTEPOINT) && current_track) {
			have_read_something = true;
			Trackpoint * tp = point_parser.add_trackpoint(coord_mode);
			current_track->trackpoints.push_back(tp);
		}

		point_parser.reset();
	}

	return have_read_something;
}




void GPSPointParser::add_waypoint(LayerTRW * trw, CoordMode coordinate_mode, const char * dirpath)
{
	Waypoint * wp = new Waypoint();
	wp->visible = this->line_visible;
	wp->altitude = this->line_altitude;
	wp->has_timestamp = this->line_has_timestamp;
	wp->timestamp = this->line_timestamp;

	wp->coord = Coord(this->line_latlon, coordinate_mode);

	trw->add_waypoint_to_data_structure(wp, this->line_name);
	free(this->line_name);
	this->line_name = NULL;

	if (this->line_comment) {
		wp->set_comment(QString(this->line_comment));
	}

	if (this->line_description) {
		wp->set_description(this->line_description);
	}

	if (this->line_source) {
		wp->set_source(this->line_source);
	}

	if (this->line_xtype) {
		wp->set_type(this->line_xtype);
	}

	if (this->line_image) {
		/* Ensure the filename is absolute. */
		if (g_path_is_absolute(this->line_image)) {
			wp->set_image_full_path(this->line_image);
		} else {
			/* Otherwise create the absolute filename from the directory of the .vik file & and the relative filename. */
			const QString full_path = QString("%1%2%3").arg(dirpath).arg(QDir::separator()).arg(this->line_image);
			wp->set_image_full_path(SGUtils::get_canonical_path(full_path));
		}
	}

	if (this->line_symbol) {
		wp->set_symbol_name(this->line_symbol);
	}
}



Track * GPSPointParser::add_track(LayerTRW * trw)
{
	Track * trk = new Track(this->line_type == GPSPOINT_TYPE_ROUTE);
	/* NB don't set defaults here as all properties are stored in the GPS_POINT format. */
	// vik_track_set_defaults (pl);

	/* Thanks to Peter Jones for this Fix. */
	if (!this->line_name) {
		this->line_name = strdup("UNK");
	}

	trk->visible = this->line_visible;

	if (this->line_comment) {
		trk->set_comment(QString(this->line_comment));
	}

	if (this->line_description) {
		trk->set_description(QString(this->line_description));
	}

	if (this->line_source) {
		trk->set_source(this->line_source);
	}

	if (this->line_xtype) {
		trk->set_type(this->line_xtype);
	}

	if (this->line_color) {
		trk->color.setNamedColor(this->line_color);
		trk->has_color = trk->color.isValid();
	}


	trk->draw_name_mode = (TrackDrawNameMode) this->line_name_label;
	trk->max_number_dist_labels = this->line_dist_label;

	/* trk->trackpoints = NULL; */ /* kamilTODO: why it was here? */
	trw->add_track_from_file2(trk, this->line_name);
	free(this->line_name);
	this->line_name = NULL;

	return trk;
}




Trackpoint * GPSPointParser::add_trackpoint(CoordMode coordinate_mode)
{
	Trackpoint * tp = new Trackpoint();
	tp->coord = Coord(this->line_latlon, coordinate_mode);
	tp->newsegment = this->line_newsegment;
	tp->has_timestamp = this->line_has_timestamp;
	tp->timestamp = this->line_timestamp;
	tp->altitude = this->line_altitude;
	tp->set_name(this->line_name);

	/* Trackpoint's extended attributes. */
	if (this->line_extended) {
		tp->speed = this->line_speed;
		tp->course = this->line_course;
		tp->nsats = this->line_sat;
		tp->fix_mode = (GPSFixMode) this->line_fix_mode;
		tp->hdop = this->line_hdop;
		tp->vdop = this->line_vdop;
		tp->pdop = this->line_pdop;
	}

	return tp;
}




/* Tag will be of a few defined forms:
   ^[:alpha:]*=".*"$
   ^[:alpha:]*=.*$

   <invalid tag>

   So we must determine end of tag name, start of value, end of value.
*/
void GPSPointParser::process_tag(char const * tag, unsigned int len)
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

		this->process_key_and_value(tag, key_end - tag, value_start, value_end - value_start);
	}
}




/*
  value = NULL for none.
*/
void GPSPointParser::process_key_and_value(char const * key, unsigned int key_len, char const * value, unsigned int value_len)
{
	if (key_len == 4 && 0 == strncasecmp(key, "type", key_len)) {
		if (value == NULL) {
			this->line_type = GPSPOINT_TYPE_NONE;
		} else if (value_len == 5 && 0 == strncasecmp(value, "track", value_len)) {
			this->line_type = GPSPOINT_TYPE_TRACK;
		} else if (value_len == 10 && 0 == strncasecmp(value, "trackpoint", value_len)) {
			this->line_type = GPSPOINT_TYPE_TRACKPOINT;
		} else if (value_len == 8 && 0 == strncasecmp(value, "waypoint", value_len)) {
			this->line_type = GPSPOINT_TYPE_WAYPOINT;
		} else if (value_len == 5 && 0 == strncasecmp(value, "route", value_len)) {
			this->line_type = GPSPOINT_TYPE_ROUTE;
		} else if (value_len == 10 && 0 == strncasecmp(value, "routepoint", value_len)) {
			this->line_type = GPSPOINT_TYPE_ROUTEPOINT;
		} else {
			/* All others are ignored. */
			this->line_type = GPSPOINT_TYPE_NONE;
		}

		return;
	}

	if (!value) {
		return;
	}

	switch (key_len) {
	case 3:
		if (0 == strncasecmp(key, "sat", key_len)) { /* Trackpoint's extended attribute. */
			this->line_sat = atoi(value);

		} else if (0 == strncasecmp(key, "fix", key_len)) { /* Trackpoint's extended attribute. */
			this->line_fix_mode = atoi(value);
		}
		break;

	case 4:
		if (0 == strncasecmp(key, "hdop", key_len)) { /* Trackpoint's extended attribute. */
			this->line_hdop = SGUtils::c_to_double(value);

		} else if (0 == strncasecmp(key, "vdop", key_len)) { /* Trackpoint's extended attribute. */
			this->line_vdop = SGUtils::c_to_double(value);

		} else if (0 == strncasecmp(key, "pdop", key_len)) { /* Trackpoint's extended attribute. */
			this->line_pdop = SGUtils::c_to_double(value);

		} else if (0 == strncasecmp(key, "name", key_len)) {
			if (this->line_name == NULL) {
				this->line_name = deslashndup(value, value_len);
			}
		}
		break;

	case 5:

		if (0 == strncasecmp(key, "speed", key_len)) { /* Trackpoint's extended attribute. */
			this->line_speed = SGUtils::c_to_double(value);

		} else if (0 == strncasecmp(key, "xtype", key_len)) {
			/* NB using 'xtype' to differentiate from our own 'type' key. */
			if (this->line_xtype == NULL) {
				this->line_xtype = deslashndup(value, value_len);
			}
		} else if (0 == strncasecmp(key, "color", key_len)) {
			if (this->line_color == NULL) {
				this->line_color = deslashndup(value, value_len);
			}

		} else if (0 == strncasecmp(key, "image", key_len)) {
			if (this->line_image == NULL) {
				this->line_image = deslashndup(value, value_len);
			}
		}
		break;

	case 6:
		if (0 == strncasecmp(key, "course", key_len)) { /* Trackpoint's extended attribute. */
			this->line_course = SGUtils::c_to_double(value);

		} else if (0 == strncasecmp(key, "symbol", key_len)) {
			this->line_symbol = strndup(value, value_len);

		} else if (0 == strncasecmp(key, "source", key_len)) {
			if (this->line_source == NULL) {
				this->line_source = deslashndup(value, value_len);
			}
		}
		break;

	case 7:
		if (0 == strncasecmp(key, "visible", key_len)) {
			this->line_visible = value[0] == 'y' || value[0] == 'Y' || value[0] == 't' || value[0] == 'T';

		} else if (0 == strncasecmp(key, "comment", key_len)) {
			if (this->line_comment == NULL) {
				this->line_comment = deslashndup(value, value_len);
			}
		}
		break;

	case 8:
		if (0 == strncasecmp(key, "latitude", key_len)) {
			this->line_latlon.lat = SGUtils::c_to_double(value);

		} else if (0 == strncasecmp(key, "altitude", key_len)) {
			this->line_altitude = SGUtils::c_to_double(value);

		} else if (0 == strncasecmp(key, "unixtime", key_len)) {
			this->line_timestamp  = g_ascii_strtod(value, NULL); /* TODO: replace with non-glib function. */
			if (this->line_timestamp != 0x80000000) {
				this->line_has_timestamp = true;
			}

		} else if (0 == strncasecmp(key, "extended", key_len)) { /* Trackpoint's extended attribute. */
			this->line_extended = true;
		}
		break;

	case 9:
		if (key_len == 9 && 0 == strncasecmp(key, "longitude", key_len)) {
			this->line_latlon.lon = SGUtils::c_to_double(value);
		}
		break;

	case 10:
		if (key_len == 10 && 0 == strncasecmp(key, "newsegment", key_len)) {
			this->line_newsegment = true;
		}
		break;

	case 11:
		if (0 == strncasecmp(key, "description", key_len)) {
			if (this->line_description == NULL) {
				this->line_description = deslashndup(value, value_len);
			}
		}
		break;

	case 14:
		if (0 == strncasecmp(key, "draw_name_mode", key_len)) {
			this->line_name_label = atoi(value);
		}
		break;

	case 18:
		if (0 == strncasecmp(key, "number_dist_labels", key_len)) {
			this->line_dist_label = atoi(value);
		}
		break;

	default:
		qDebug() << "EE: GPSPoint: process key and value: unhandled key" << key << "of length" << key_len;
		break;
	}
}




static void a_gpspoint_write_waypoints(FILE * file, Waypoints & data)
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

		static LatLon ll = wp->coord.get_latlon();
		char * s_lat = a_coords_dtostr(ll.lat);
		char * s_lon = a_coords_dtostr(ll.lon);
		char * tmp_name = slashdup(wp->name);
		fprintf(file, "type=\"waypoint\" latitude=\"%s\" longitude=\"%s\" name=\"%s\"", s_lat, s_lon, tmp_name);
		free(tmp_name);
		free(s_lat);
		free(s_lon);

		if (wp->altitude != VIK_DEFAULT_ALTITUDE) {
			char * s_alt = a_coords_dtostr(wp->altitude);
			fprintf(file, " altitude=\"%s\"", s_alt);
			free(s_alt);
		}
		if (wp->has_timestamp) {
			fprintf(file, " unixtime=\"%ld\"", wp->timestamp);
		}

		if (!wp->comment.isEmpty()) {
			char * tmp_comment = slashdup(wp->comment);
			fprintf(file, " comment=\"%s\"", tmp_comment);
			free(tmp_comment);
		}
		if (!wp->description.isEmpty()) {
			char * tmp_description = slashdup(wp->description);
			fprintf(file, " description=\"%s\"", tmp_description);
			free(tmp_description);
		}
		if (!wp->source.isEmpty()) {
			char * tmp_source = slashdup(wp->source);
			fprintf(file, " source=\"%s\"", tmp_source);
			free(tmp_source);
		}
		if (!wp->type.isEmpty()) {
			char * tmp_type = slashdup(wp->type);
			fprintf(file, " xtype=\"%s\"", tmp_type);
			free(tmp_type);
		}
		if (!wp->image_full_path.isEmpty()) {
			QString tmp_image_full_path;
			QString cwd;
			if (Preferences::get_file_ref_format() == VIK_FILE_REF_FORMAT_RELATIVE) {
				cwd = QDir::currentPath();
				if (!cwd.isEmpty()) {
					tmp_image_full_path = file_GetRelativeFilename(cwd, wp->image_full_path);
				}
			}

			/* If cwd not available - use image filename as is.
			   This should be an absolute path as set in thumbnails. */
			if (cwd.isEmpty()) {
				tmp_image_full_path = QString(slashdup(wp->image_full_path));
			}

			if (!tmp_image_full_path.isEmpty()) {
				fprintf(file, " image=\"%s\"", tmp_image_full_path.toUtf8().constData());
			}
		}
		if (!wp->symbol_name.isEmpty()) {
			/* Due to changes in garminsymbols - the symbol name is now in Title Case.
			   However to keep newly generated .vik files better compatible with older Viking versions.
			   The symbol names will always be lowercase. */
			char * tmp_symbol = g_utf8_strdown(wp->symbol_name.toUtf8().constData(), -1);
			fprintf(file, " symbol=\"%s\"", tmp_symbol);
			free(tmp_symbol);
		}
		if (!wp->visible) {
			fprintf(file, " visible=\"n\"");
		}
		fprintf(file, "\n");
	}
}




static void a_gpspoint_write_trackpoint(Trackpoint * tp, TP_write_info_type * write_info)
{
	static LatLon ll = tp->coord.get_latlon();

	FILE * file = write_info->f;

	/* TODO: modify a_coords_dtostr() to accept (optional) buffer
	   instead of doing malloc/free every time. */
	char * s_lat = a_coords_dtostr(ll.lat);
	char * s_lon = a_coords_dtostr(ll.lon);
	fprintf(file, "type=\"%spoint\" latitude=\"%s\" longitude=\"%s\"", write_info->is_route ? "route" : "track", s_lat, s_lon);
	free(s_lat);
	free(s_lon);

	if (!tp->name.isEmpty()) {
		char * name = slashdup(tp->name);
		fprintf(file, " name=\"%s\"", name);
		free(name);
	}

	if (tp->altitude != VIK_DEFAULT_ALTITUDE) {
		char * s_alt = a_coords_dtostr(tp->altitude);
		fprintf(file, " altitude=\"%s\"", s_alt);
		free(s_alt);
	}
	if (tp->has_timestamp) {
		fprintf(file, " unixtime=\"%ld\"", tp->timestamp);
	}

	if (tp->newsegment) {
		fprintf(file, " newsegment=\"yes\"");
	}

	if (!std::isnan(tp->speed) || !std::isnan(tp->course) || tp->nsats > 0) {
		fprintf(file, " extended=\"yes\"");
		if (!std::isnan(tp->speed)) {
			char * s_speed = a_coords_dtostr(tp->speed);
			fprintf(file, " speed=\"%s\"", s_speed);
			free(s_speed);
		}
		if (!std::isnan(tp->course)) {
			char * s_course = a_coords_dtostr(tp->course);
			fprintf(file, " course=\"%s\"", s_course);
			free(s_course);
		}
		if (tp->nsats > 0) {
			fprintf(file, " sat=\"%d\"", tp->nsats);
		}

		if (((int) tp->fix_mode) > 0) {
			fprintf(file, " fix=\"%d\"", (int) tp->fix_mode);
		}

		if (tp->hdop != VIK_DEFAULT_DOP) {
			char * ss = a_coords_dtostr(tp->hdop);
			fprintf(file, " hdop=\"%s\"", ss);
			free(ss);
		}
		if (tp->vdop != VIK_DEFAULT_DOP) {
			char * ss = a_coords_dtostr(tp->vdop);
			fprintf(file, " vdop=\"%s\"", ss);
			free(ss);
		}
		if (tp->pdop != VIK_DEFAULT_DOP) {
			char * ss = a_coords_dtostr(tp->pdop);
			fprintf(file, " pdop=\"%s\"", ss);
			free(ss);
		}
	}
	fprintf(file, "\n");
}




static void a_gpspoint_write_track(FILE * file, Tracks & tracks)
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
		fprintf(file, "type=\"%s\" name=\"%s\"", trk->type_id == "sg.trw.route" ? "route" : "track", tmp_name);
		free(tmp_name);

		if (!trk->comment.isEmpty()) {
			char * tmp = slashdup(trk->comment);
			fprintf(file, " comment=\"%s\"", tmp);
			free(tmp);
		}

		if (!trk->description.isEmpty()) {
			char * tmp = slashdup(trk->description);
			fprintf(file, " description=\"%s\"", tmp);
			free(tmp);
		}

		if (!trk->source.isEmpty()) {
			char * tmp = slashdup(trk->source);
			fprintf(file, " source=\"%s\"", tmp);
			free(tmp);
		}

		if (!trk->type.isEmpty()) {
			char * tmp = slashdup(trk->type);
			fprintf(file, " xtype=\"%s\"", tmp);
			free(tmp);
		}

		if (trk->has_color) {
			fprintf(file, " color=#%.2x%.2x%.2x", (int)(trk->color.red()),(int)(trk->color.green()),(int)(trk->color.blue()));
		}

		if (trk->draw_name_mode != TrackDrawNameMode::NONE) {
			fprintf(file, " draw_name_mode=\"%d\"", (int) trk->draw_name_mode);
		}

		if (trk->max_number_dist_labels > 0) {
			fprintf(file, " number_dist_labels=\"%d\"", trk->max_number_dist_labels);
		}

		if (!trk->visible) {
			fprintf(file, " visible=\"n\"");
		}
		fprintf(file, "\n");

		TP_write_info_type tp_write_info = { file, trk->type_id == "sg.trw.route" };
		for (auto iter = trk->trackpoints.begin(); iter != trk->trackpoints.end(); iter++) {
			a_gpspoint_write_trackpoint(*iter, &tp_write_info);
		}
		fprintf(file, "type=\"%send\"\n", trk->type_id == "sg.trw.route" ? "route" : "track");
	}
}




void SlavGPS::a_gpspoint_write_file(FILE * file, LayerTRW const * trw)
{
	Tracks & tracks = ((LayerTRW *) trw)->get_track_items();
	Tracks & routes = ((LayerTRW *) trw)->get_route_items();
	Waypoints & waypoints = ((LayerTRW *) trw)->get_waypoint_items();

	fprintf(file, "type=\"waypointlist\"\n");
	a_gpspoint_write_waypoints(file, waypoints);
	fprintf(file, "type=\"waypointlistend\"\n");

	a_gpspoint_write_track(file, tracks);
	a_gpspoint_write_track(file, routes);
}
