/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2009, Hein Ragas
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

#include <QDebug>

#include "window.h"
#include "dialog.h"
#include "geonamessearch.h"
#include "download.h"
#include "util.h"
#include "globals.h"
#include "util.h"
#include "widget_list_selection.h"
#include "layer_trw.h"




using namespace SlavGPS;




/**
 * See http://www.geonames.org/export/wikipedia-webservice.html#wikipediaBoundingBox
 */
/* Translators may wish to change this setting as appropriate to get Wikipedia articles in that language. */
#define GEONAMES_LANG QObject::tr("en")
/* TODO - offer configuration of this value somewhere.
   ATM decided it's not essential enough to warrant putting in the preferences. */
#define GEONAMES_MAX_ENTRIES 20

//#define GEONAMES_WIKIPEDIA_URL_FMT "http://api.geonames.org/wikipediaBoundingBoxJSON?formatted=true&north=%s&south=%s&east=%s&west=%s&lang=%s&maxRows=%d&username=viking"
#define GEONAMES_WIKIPEDIA_URL_FMT "http://api.geonames.org/wikipediaBoundingBoxJSON?formatted=true&north=%1&south=%2&east=%3&west=%4&lang=%5&maxRows=%6&username=viking"

#define GEONAMES_FEATURE_PATTERN "\"feature\": \""
#define GEONAMES_LONGITUDE_PATTERN "\"lng\": "
#define GEONAMES_NAME_PATTERN "\"name\": \""
#define GEONAMES_LATITUDE_PATTERN "\"lat\": "
#define GEONAMES_ELEVATION_PATTERN "\"elevation\": "
#define GEONAMES_TITLE_PATTERN "\"title\": \""
#define GEONAMES_WIKIPEDIAURL_PATTERN "\"wikipediaUrl\": \""
#define GEONAMES_THUMBNAILIMG_PATTERN "\"thumbnailImg\": \""
#define GEONAMES_SEARCH_NOT_FOUND "not understand the location"




static Geoname * copy_geoname(Geoname * src)
{
	Geoname * dest = new Geoname();
	dest->name = src->name;
	dest->feature = src->feature;
	dest->ll.lat = src->ll.lat;
	dest->ll.lon = src->ll.lon;
	dest->elevation = src->elevation;
	dest->comment = src->comment;
	dest->desc = src->desc;
	return dest;
}




static void free_geoname_list(std::list<Geoname *> & found_places)
{
	for (auto iter = found_places.begin(); iter != found_places.end(); iter++) {
		qDebug() << "DD: deallocating geoname" << (*iter)->name;
		delete *iter;
	}
}




/*
  TODO: this function builds a table with three columns, but only one
  of them (Name) is filled with details from geonames.  Extend/improve
  list selection widget so that it can display properties of items in
  N columns.
*/
std::list<Geoname *> a_select_geoname_from_list(const QString & title, const QStringList & headers, std::list<Geoname *> & geonames, bool multiple_selection, Window * parent)
{
	QStringList headers2;
	headers2 << QObject::tr("Name") << QObject::tr("Feature") << QObject::tr("Lat/Lon");

	std::list<Geoname *> selected_geonames = a_dialog_select_from_list(geonames, multiple_selection, title, headers2, parent);

	if (!selected_geonames.size()) {
		Dialog::error(QObject::tr("Nothing was selected"), parent);
	}

	return selected_geonames; /* Hopefully Named Return Value Optimization will work here. */
}




static std::list<Geoname *> get_entries_from_file(const QString & file_full_path)
{
	std::list<Geoname *> found_places;

	char lat_buf[32] = { 0 };
	char lon_buf[32] = { 0 };
	char elev_buf[32] = { 0 };
	char * wikipedia_url = NULL;
	char * thumbnail_url = NULL;

	GMappedFile * mf = NULL;
	if ((mf = g_mapped_file_new(file_full_path.toUtf8().constData(), false, NULL)) == NULL) {
		qDebug() << QObject::tr("CRITICAL: couldn't map temp file");
		return found_places;
	}
	size_t len = g_mapped_file_get_length(mf);
	char * text = g_mapped_file_get_contents(mf);

	bool more = true;
	if (g_strstr_len(text, len, GEONAMES_SEARCH_NOT_FOUND) != NULL) {
		more = false;
	}

	char ** found_entries = g_strsplit(text, "},", 0);
	int entry_runner = 0;
	char * entry = found_entries[entry_runner];
	char * pat = NULL;
	char * s = NULL;
	int fragment_len;
	while (entry) {
		more = true;
		Geoname * geoname = new Geoname();
		if ((pat = g_strstr_len(entry, strlen(entry), GEONAMES_FEATURE_PATTERN))) {
			pat += strlen(GEONAMES_FEATURE_PATTERN);
			fragment_len = 0;
			s = pat;
			while (*pat != '"') {
				fragment_len++;
				pat++;
			}
			geoname->feature = g_strndup(s, fragment_len);
		}
		if ((pat = g_strstr_len(entry, strlen(entry), GEONAMES_LONGITUDE_PATTERN)) == NULL) {
			more = false;
		} else {
			pat += strlen(GEONAMES_LONGITUDE_PATTERN);
			s = lon_buf;
			if (*pat == '-')
				*s++ = *pat++;
			while ((s < (lon_buf + sizeof(lon_buf))) && (pat < (text + len)) &&
			       (g_ascii_isdigit(*pat) || (*pat == '.'))) {
				*s++ = *pat++;
			}
			*s = '\0';
			if ((pat >= (text + len)) || (lon_buf[0] == '\0')) {
				more = false;
			}
			geoname->ll.lon = SGUtils::c_to_double(lon_buf);
		}
		if ((pat = g_strstr_len(entry, strlen(entry), GEONAMES_ELEVATION_PATTERN))) {
			pat += strlen(GEONAMES_ELEVATION_PATTERN);
			s = elev_buf;
			if (*pat == '-') {
				*s++ = *pat++;
			}
			while ((s < (elev_buf + sizeof(elev_buf))) && (pat < (text + len)) &&
			       (g_ascii_isdigit(*pat) || (*pat == '.'))) {
				*s++ = *pat++;
			}
			*s = '\0';
			geoname->elevation = SGUtils::c_to_double(elev_buf);
		}
		if ((pat = g_strstr_len(entry, strlen(entry), GEONAMES_NAME_PATTERN))) {
			pat += strlen(GEONAMES_NAME_PATTERN);
			fragment_len = 0;
			s = pat;
			while (*pat != '"') {
				fragment_len++;
				pat++;
			}
			geoname->name = g_strndup(s, fragment_len);
		}
		if ((pat = g_strstr_len(entry, strlen(entry), GEONAMES_TITLE_PATTERN))) {
			pat += strlen(GEONAMES_TITLE_PATTERN);
			fragment_len = 0;
			s = pat;
			while (*pat != '"') {
				fragment_len++;
				pat++;
			}
			geoname->name = g_strndup(s, fragment_len);
		}
		if ((pat = g_strstr_len(entry, strlen(entry), GEONAMES_WIKIPEDIAURL_PATTERN))) {
			pat += strlen(GEONAMES_WIKIPEDIAURL_PATTERN);
			fragment_len = 0;
			s = pat;
			while (*pat != '"') {
				fragment_len++;
				pat++;
			}
			wikipedia_url = g_strndup(s, fragment_len);
		}
		if ((pat = g_strstr_len(entry, strlen(entry), GEONAMES_THUMBNAILIMG_PATTERN))) {
			pat += strlen(GEONAMES_THUMBNAILIMG_PATTERN);
			fragment_len = 0;
			s = pat;
			while (*pat != '"') {
				fragment_len++;
				pat++;
			}
			thumbnail_url = g_strndup(s, fragment_len);
		}
		if ((pat = g_strstr_len(entry, strlen(entry), GEONAMES_LATITUDE_PATTERN)) == NULL) {
			more = false;
		} else {
			pat += strlen(GEONAMES_LATITUDE_PATTERN);
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
				more = false;
			}
			geoname->ll.lat = SGUtils::c_to_double(lat_buf);
		}
		if (!more) {
			if (geoname) {
				free(geoname);
			}
		} else {
			if (wikipedia_url) {
				/* Really we should support the GPX URL tag and then put that in there... */
				geoname->comment = QString("http://%1").arg(wikipedia_url);
				if (thumbnail_url) {
					geoname->desc = QString("<a href=\"http://%1\" target=\"_blank\"><img src=\"%2\" border=\"0\"/></a>").arg(wikipedia_url).arg(thumbnail_url);
				} else {
					geoname->desc = QString("<a href=\"http://%1\" target=\"_blank\">%2</a>").arg(wikipedia_url).arg(geoname->name);
				}
			}
			if (wikipedia_url) {
				free(wikipedia_url);
				wikipedia_url = NULL;
			}
			if (thumbnail_url) {
				free(thumbnail_url);
				thumbnail_url = NULL;
			}
			found_places.push_front(geoname);
		}
		entry_runner++;
		entry = found_entries[entry_runner];
	}
	g_strfreev(found_entries);
	found_places.reverse();
	g_mapped_file_unref(mf);

	return found_places; /* Hopefully Named Return Value Optimization will work here. */
}




void SlavGPS::a_geonames_wikipedia_box(Window * window, LayerTRW * trw, const LatLonMinMax & min_max)
{
	QString north;
	QString south;
	QString east;
	QString west;
	CoordUtils::to_string(north, min_max.max.lat);
	CoordUtils::to_string(south, min_max.min.lat);
	CoordUtils::to_string(east, min_max.max.lon);
	CoordUtils::to_string(west, min_max.min.lon);

	/* Encode doubles in a C locale; kamilTODO: see LatLonBBox::to_strings(). */
	const QString uri = QString(GEONAMES_WIKIPEDIA_URL_FMT).arg(north).arg(south).arg(east).arg(west).arg(GEONAMES_LANG).arg(GEONAMES_MAX_ENTRIES);

	const QString tmp_file_full_path = Download::get_uri_to_tmp_file(uri, NULL);
	if (tmp_file_full_path.isEmpty()) {
		Dialog::info(QObject::tr("No entries found!"), window);
		return;
	}

	std::list<Geoname *> wiki_places = get_entries_from_file(tmp_file_full_path);
	Util::remove(tmp_file_full_path);

	if (wiki_places.size() == 0) {
		Dialog::info(QObject::tr("No entries found!"), window);
		return;
	}

	const QStringList headers = { QObject::tr("Select the articles you want to add.") };
	std::list<Geoname *> selected = a_select_geoname_from_list(QObject::tr("Select articles"), headers, wiki_places, true, window);

	for (auto iter = selected.begin(); iter != selected.end(); iter++) {
		const Geoname * wiki_geoname = *iter;

		Waypoint * wiki_wp = new Waypoint();
		wiki_wp->visible = true;
		wiki_wp->coord = Coord(wiki_geoname->ll, trw->get_coord_mode());
		wiki_wp->altitude = wiki_geoname->elevation;
		wiki_wp->set_comment(wiki_geoname->comment);
		wiki_wp->set_description(wiki_geoname->desc);

		/* Use the featue type to generate a suitable waypoint icon
		   http://www.geonames.org/wikipedia/wikipedia_features.html
		   Only a few values supported as only a few symbols make sense. */
		if (!wiki_geoname->feature.isEmpty()) {
			if (wiki_geoname->feature == "city") {
				wiki_wp->set_symbol_name("city (medium)");
			}

			if (wiki_geoname->feature == "edu") {
				wiki_wp->set_symbol_name("school");
			}

			if (wiki_geoname->feature == "airport") {
				wiki_wp->set_symbol_name("airport");
			}

			if (wiki_geoname->feature == "mountain") {
				wiki_wp->set_symbol_name("summit");
			}

			if (wiki_geoname->feature == "forest") {
				wiki_wp->set_symbol_name("forest");
			}
		}
		trw->add_waypoint_from_file(wiki_wp, wiki_geoname->name);
	}

	free_geoname_list(wiki_places);
#if 0
	/* 'selected' contains pointer to geonames that were
	   present on 'wiki_places'.  Freeing 'wiki_places' freed also
	   pointers stored in 'selected', so there is no need to call
	   free_geoname_list(selected). */
	free_geoname_list(selected);
#endif

	return;
}
