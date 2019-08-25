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
 */




#include <cstdlib>
#include <cassert>
#include <cctype>
#include <cstring>
#include <cmath>




#include <QDebug>
#include <QDir>




#include "gpspoint.h"
#include "layer_trw_track_internal.h"
#include "layer_trw_waypoint.h"
#include "layer_trw.h"
#include "file.h"
#include "preferences.h"




using namespace SlavGPS;




#define SG_MODULE "GPSPoint"




static void a_gpspoint_write_tracks(FILE * file, const std::list<Track *> & tracks);
static void a_gpspoint_write_trackpoint(FILE * file, const Trackpoint * tp, bool is_route);
static void a_gpspoint_write_waypoints(FILE * file, const std::list<Waypoint *> & waypoints);




#define GPSPOINT_TYPE_NONE 0
#define GPSPOINT_TYPE_WAYPOINT 1
#define GPSPOINT_TYPE_TRACKPOINT 2
#define GPSPOINT_TYPE_ROUTEPOINT 3
#define GPSPOINT_TYPE_TRACK 4
#define GPSPOINT_TYPE_ROUTE 5




class GPSPointParser {
public:
	void reset(void);
	void process_tag(char const * tag, int len);
	void process_key_and_value(const char * key, int key_len, const char * value, int value_len);

	Track * create_track(LayerTRW * trw);
	Waypoint * create_waypoint(CoordMode coordinate_mode, const QString & dirpath);
	Trackpoint * create_trackpoint(CoordMode coordinate_mode);

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
	QString line_image;
	char * line_symbol = NULL;

	bool line_visible = true;

	bool line_newsegment = false;
	Time line_timestamp; /* Invalid by default. */
	Altitude line_altitude; /* Invalid by default. */

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




static char * slashdup(const QString & input)
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
	this->line_type = GPSPOINT_TYPE_NONE;
	this->line_latlon.invalidate();

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

	this->line_name_label = 0;
	this->line_dist_label = 0;

	this->line_image = "";

	if (this->line_symbol) {
		free(this->line_symbol);
		this->line_symbol = NULL;
	}

	this->line_visible = true;

	this->line_newsegment = false;
	this->line_timestamp.invalidate();
	this->line_altitude.invalidate();

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

	return;
}




/*
 * Returns whether file read was a success.
 * No obvious way to test for a 'gpspoint' file,
 * thus set a flag if any actual tag found during processing of the file.
 */
LayerDataReadStatus GPSPoint::read_layer_from_file(QFile & file, LayerTRW * trw, const QString & dirpath)
{
	assert (trw != NULL);

	const CoordMode coord_mode = trw->get_coord_mode();

	GPSPointParser point_parser;
	Track * current_track = NULL;

	bool have_read_something = false;

	while (0 < file.readLine(line_buffer, sizeof (line_buffer))) {
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

			const int len = tag_end - tag_start;
			point_parser.process_tag(tag_start, len);

			if (*tag_end == '\0') {
				break;
			} else {
				tag_start = tag_end + 1;
			}
		}


		have_read_something = true;

		if (point_parser.line_type == GPSPOINT_TYPE_WAYPOINT && point_parser.line_name) {
			Waypoint * wp = point_parser.create_waypoint(coord_mode, dirpath);
			trw->add_waypoint(wp);

		} else if (point_parser.line_type == GPSPOINT_TYPE_TRACK && point_parser.line_name) {
			current_track = point_parser.create_track(trw);
			trw->add_track(current_track);

		} else if (point_parser.line_type == GPSPOINT_TYPE_ROUTE && point_parser.line_name) {
			current_track = point_parser.create_track(trw);
			trw->add_route(current_track);

		} else if ((point_parser.line_type == GPSPOINT_TYPE_TRACKPOINT || point_parser.line_type == GPSPOINT_TYPE_ROUTEPOINT) && current_track) {
			Trackpoint * tp = point_parser.create_trackpoint(coord_mode);
			current_track->trackpoints.push_back(tp);

		} else {
			have_read_something = false;
		}

		point_parser.reset();
	}

	if (have_read_something) {
		return LayerDataReadStatus::Success;
	} else {
		return LayerDataReadStatus::Error;
	}
}




Waypoint * GPSPointParser::create_waypoint(CoordMode coordinate_mode, const QString & dirpath)
{
	Waypoint * wp = new Waypoint();
	wp->set_visible(this->line_visible);
	wp->altitude = this->line_altitude;
	wp->set_name(this->line_name);
	wp->set_timestamp(this->line_timestamp);

	wp->coord = Coord(this->line_latlon, coordinate_mode);

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

	if (!this->line_image.isEmpty()) {
		/* Ensure the filename is absolute. */
		if (QDir::isAbsolutePath(this->line_image)) {
			wp->set_image_full_path(this->line_image);
		} else {
			/* Otherwise create the absolute filename from the directory of the .vik file & and the relative filename. */
			const QString full_path = QString("%1%2%3").arg(dirpath).arg(QDir::separator()).arg(this->line_image);
			wp->set_image_full_path(SGUtils::get_canonical_path(full_path));
		}
	}

	if (this->line_symbol) {
		wp->set_symbol(this->line_symbol);
	}

	return wp;
}



Track * GPSPointParser::create_track(LayerTRW * trw)
{
	Track * trk = new Track(this->line_type == GPSPOINT_TYPE_ROUTE);
	/* Don't set defaults here as all properties are stored in the GPS_POINT format. */
	// vik_track_set_defaults(trk);

	/* Thanks to Peter Jones for this Fix. */
	if (!this->line_name) {
		this->line_name = strdup("UNK");
	}

	trk->set_visible(this->line_visible);
	trk->name = this->line_name;

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


	return trk;
}




/* Create trackpoint from current parser data. */
Trackpoint * GPSPointParser::create_trackpoint(CoordMode coordinate_mode)
{
	Trackpoint * tp = new Trackpoint();
	tp->coord = Coord(this->line_latlon, coordinate_mode);
	tp->newsegment = this->line_newsegment;
	tp->altitude = this->line_altitude;
	tp->set_name(this->line_name);
	tp->timestamp = this->line_timestamp;

	/* Trackpoint's extended attributes. */
	if (this->line_extended) {
		tp->gps_speed = this->line_speed;
		tp->course = Angle(this->line_course, AngleUnit::Degrees);  /* TODO: verify unit read from file. */
		tp->nsats = this->line_sat;
		tp->fix_mode = (GPSFixMode) this->line_fix_mode;
		tp->hdop = this->line_hdop;
		tp->vdop = this->line_vdop;
		tp->pdop = this->line_pdop;
	}

	qDebug() << SG_PREFIX_I << "new trackpoint at" << tp->coord << "(" << this->line_latlon << ")";

	return tp;
}




/* Tag will be of a few defined forms:
   ^[:alpha:]*=".*"$
   ^[:alpha:]*=.*$

   <invalid tag>

   So we must determine end of tag name, start of value, end of value.
*/
void GPSPointParser::process_tag(char const * tag, int len)
{
	char const * value_start = NULL;
	char const * value_end = NULL;

	/* Search for end of key. */
	int key_len = 0;
	while (key_len < len) {
		if (*(tag + key_len) == '=') {
			break;
		}
		key_len++;
	}

	if (key_len == len) {
		return; /* No good. */
	}

	if (key_len == len + 1) {
		value_end = value_start; /* value's size = 0 */
	} else {
		value_start = tag + key_len + 1; /* equal_sign plus one. */

		if (*value_start == '"') {
			value_start++;
			if (*value_start == '"') {
				value_end = value_start = 0; /* value's size = 0 */
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


		const int value_len = value_end - value_start;

		/* Detect broken lines which end without any text or the enclosing ". i.e. like: comment=" */
		if (value_len < 0) {
			return;
		}

		if (key_len >= VIKING_LINE_SIZE || value_len >= VIKING_LINE_SIZE) {
			return;
		}

		char key[VIKING_LINE_SIZE];
		memcpy(key, tag, key_len);
		key[key_len] = '\0';

		char value[VIKING_LINE_SIZE];
		memcpy(value, value_start, value_len);
		value[value_len] = '\0';

		this->process_key_and_value(key, key_len, value, value_len);
	}
}




void GPSPointParser::process_key_and_value(const char * key, int key_len, const char * value, int value_len)
{
	switch (key_len) {
	case 3:
		if (0 == strncasecmp(key, "sat", key_len)) { /* Trackpoint's extended attribute. */
			this->line_sat = atoi(value);

		} else if (0 == strncasecmp(key, "fix", key_len)) { /* Trackpoint's extended attribute. */
			this->line_fix_mode = atoi(value);
		}
		break;

	case 4:
		if (0 == strncasecmp(key, "type", key_len)) {
			if (value_len == 0) {
				this->line_type = GPSPOINT_TYPE_NONE;

			} else if (value_len == 5 && 0 == strncasecmp(value, "track", value_len)) {
				this->line_type = GPSPOINT_TYPE_TRACK;

			} else if (value_len == 5 && 0 == strncasecmp(value, "route", value_len)) {
				this->line_type = GPSPOINT_TYPE_ROUTE;

			} else if (value_len == 8 && 0 == strncasecmp(value, "waypoint", value_len)) {
				this->line_type = GPSPOINT_TYPE_WAYPOINT;

			} else if (value_len == 10 && 0 == strncasecmp(value, "trackpoint", value_len)) {
				this->line_type = GPSPOINT_TYPE_TRACKPOINT;

			} else if (value_len == 10 && 0 == strncasecmp(value, "routepoint", value_len)) {
				this->line_type = GPSPOINT_TYPE_ROUTEPOINT;

			} else {
				/* All others are ignored. */
				this->line_type = GPSPOINT_TYPE_NONE;
			}

		} else if (0 == strncasecmp(key, "hdop", key_len)) { /* Trackpoint's extended attribute. */
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
			if (this->line_image.isEmpty()) {
				char * line = deslashndup(value, value_len);
				this->line_image = QString(line);
				free(line);
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
			this->line_altitude.set_value_from_char_string(value);

		} else if (0 == strncasecmp(key, "unixtime", key_len)) {
			this->line_timestamp.set_value_from_char_string(value);

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
		qDebug() << SG_PREFIX_E << "Process key and value: unhandled key" << key << "of length" << key_len;
		break;
	}
}




static void a_gpspoint_write_waypoints(FILE * file, const std::list<Waypoint *> & waypoints)
{
	for (auto iter = waypoints.begin(); iter != waypoints.end(); iter++) {

		Waypoint * wp = *iter;

		/* Sanity clauses. */
		if (!wp) {
			continue;
		}

		if (wp->name.isEmpty()) { /* TODO_LATER: will this condition ever be true? */
			continue;
		}

		const LatLon lat_lon = wp->coord.get_lat_lon();
		char * tmp_name = slashdup(wp->name);
		fprintf(file, "type=\"waypoint\" latitude=\"%s\" longitude=\"%s\" name=\"%s\"", SGUtils::double_to_c(lat_lon.lat).toUtf8().constData(), SGUtils::double_to_c(lat_lon.lon).toUtf8().constData(), tmp_name);
		free(tmp_name);

		if (wp->altitude.is_valid()) {
			fprintf(file, " altitude=\"%s\"", wp->altitude.value_to_string_for_file().toUtf8().constData());
		}
		if (wp->get_timestamp().is_valid()) {
			fprintf(file, " unixtime=\"%ld\"", wp->get_timestamp().get_ll_value());
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
			if (Preferences::get_file_path_format() == FilePathFormat::Relative) {
				cwd = QDir::currentPath();
				if (!cwd.isEmpty()) {
					tmp_image_full_path = file_GetRelativeFilename(cwd, wp->image_full_path);
				}
			}

			/* If cwd not available - use image filename as is.
			   This should be an absolute path as set in thumbnails. */
			if (cwd.isEmpty()) {
				char * tmp = slashdup(wp->image_full_path);
				tmp_image_full_path = QString(tmp);
				free(tmp);
			}

			if (!tmp_image_full_path.isEmpty()) {
				fprintf(file, " image=\"%s\"", tmp_image_full_path.toUtf8().constData());
			}
		}
		if (!wp->symbol_name.isEmpty()) {
			/* Due to changes in GarminSymbols - the symbol name is now in Title Case.
			   However to keep newly generated .vik files better compatible with older Viking versions.
			   The symbol names will always be lowercase. */
			fprintf(file, " symbol=\"%s\"", wp->symbol_name.toLower().toUtf8().constData());
		}
		if (!wp->is_visible()) {
			fprintf(file, " visible=\"n\"");
		}
		fprintf(file, "\n");
	}
}




static void a_gpspoint_write_trackpoint(FILE * file, const Trackpoint * tp, bool is_route)
{
	const LatLon lat_lon = tp->coord.get_lat_lon();

	fprintf(file, "type=\"%spoint\" latitude=\"%s\" longitude=\"%s\"", is_route ? "route" : "track", SGUtils::double_to_c(lat_lon.lat).toUtf8().constData(), SGUtils::double_to_c(lat_lon.lon).toUtf8().constData());

	if (!tp->name.isEmpty()) {
		char * name = slashdup(tp->name);
		fprintf(file, " name=\"%s\"", name);
		free(name);
	}

	if (tp->altitude.is_valid()) {
		fprintf(file, " altitude=\"%s\"", tp->altitude.value_to_string_for_file().toUtf8().constData());
	}
	if (tp->timestamp.is_valid()) {
		fprintf(file, " unixtime=\"%ld\"", tp->timestamp.get_ll_value());
	}
	if (tp->newsegment) {
		fprintf(file, " newsegment=\"yes\"");
	}

	if (!std::isnan(tp->gps_speed) || tp->course.is_valid() || tp->nsats > 0) {
		fprintf(file, " extended=\"yes\"");
		if (!std::isnan(tp->gps_speed)) {
			fprintf(file, " speed=\"%s\"", SGUtils::double_to_c(tp->gps_speed).toUtf8().constData());
		}
		if (tp->course.is_valid()) {
			fprintf(file, " course=\"%s\"", tp->course.value_to_string_for_file(SG_PRECISION_COURSE).toUtf8().constData());
		}
		if (tp->nsats > 0) {
			fprintf(file, " sat=\"%d\"", tp->nsats);
		}

		if (((int) tp->fix_mode) > 0) {
			fprintf(file, " fix=\"%d\"", (int) tp->fix_mode);
		}

		if (tp->hdop != VIK_DEFAULT_DOP) {
			fprintf(file, " hdop=\"%s\"", SGUtils::double_to_c(tp->hdop).toUtf8().constData());
		}
		if (tp->vdop != VIK_DEFAULT_DOP) {
			fprintf(file, " vdop=\"%s\"", SGUtils::double_to_c(tp->vdop).toUtf8().constData());
		}
		if (tp->pdop != VIK_DEFAULT_DOP) {
			fprintf(file, " pdop=\"%s\"", SGUtils::double_to_c(tp->pdop).toUtf8().constData());
		}
	}
	fprintf(file, "\n");
}




static void a_gpspoint_write_tracks(FILE * file, const std::list<Track *> & tracks)
{
	for (auto tracks_iter = tracks.begin(); tracks_iter != tracks.end(); tracks_iter++) {

		Track * trk = *tracks_iter;

		/* Sanity clauses. */
		if (!trk) {
			continue;
		}

		if (trk->name.isEmpty()) { /* TODO_LATER: will this condition ever be true? */
			continue;
		}

		char * tmp_name = slashdup(trk->name);
		fprintf(file, "type=\"%s\" name=\"%s\"", trk->is_route() ? "route" : "track", tmp_name);
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

		if (trk->draw_name_mode != TrackDrawNameMode::None) {
			fprintf(file, " draw_name_mode=\"%d\"", (int) trk->draw_name_mode);
		}

		if (trk->max_number_dist_labels > 0) {
			fprintf(file, " number_dist_labels=\"%d\"", trk->max_number_dist_labels);
		}

		if (!trk->is_visible()) {
			fprintf(file, " visible=\"n\"");
		}
		fprintf(file, "\n");

		const bool is_route = trk->is_route();
		for (auto tp_iter = trk->trackpoints.begin(); tp_iter != trk->trackpoints.end(); tp_iter++) {
			a_gpspoint_write_trackpoint(file, *tp_iter, is_route);
		}
		fprintf(file, "type=\"%send\"\n", is_route ? "route" : "track");
	}
}




SaveStatus GPSPoint::write_layer_to_file(FILE * file, const LayerTRW * trw)
{
	const std::list<Track *> & tracks = trw->get_tracks();
	const std::list<Track *> & routes = trw->get_routes();
	const std::list<Waypoint *> & waypoints = trw->get_waypoints();

	fprintf(file, "type=\"waypointlist\"\n");
	a_gpspoint_write_waypoints(file, waypoints);
	fprintf(file, "type=\"waypointlistend\"\n");

	a_gpspoint_write_tracks(file, tracks);
	a_gpspoint_write_tracks(file, routes);

	return SaveStatus::Code::Success;
}
