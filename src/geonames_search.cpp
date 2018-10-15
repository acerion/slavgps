/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2009, Hein Ragas
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2018, Kamil Ignacak <acerion@wp.pl>
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




#include <QDebug>




#include "window.h"
#include "dialog.h"
#include "geonames_search.h"
#include "download.h"
#include "util.h"
#include "widget_list_selection.h"
#include "layer_trw.h"




using namespace SlavGPS;




#define SG_MODULE "GeoNames Search"




/**
 * See http://www.geonames.org/export/wikipedia-webservice.html#wikipediaBoundingBox
 */
/* Translators may wish to change this setting as appropriate to get Wikipedia articles in that language. */
#define GEONAMES_LANG QObject::tr("en")
/* TODO_MAYBE - offer configuration of this value somewhere.
   ATM decided it's not essential enough to warrant putting in the preferences. */
#define GEONAMES_MAX_ENTRIES 20

/* TODO_2_LATER: update username in the query string. */
#define GEONAMES_WIKIPEDIA_URL_FMT "http://api.geonames.org/wikipediaBoundingBoxJSON?formatted=true&north=%1&south=%2&east=%3&west=%4&lang=%5&maxRows=%6&username=viking"

#define GEONAMES_FEATURE_PATTERN      "\"feature\""
#define GEONAMES_LONGITUDE_PATTERN    "\"lng\""
#define GEONAMES_NAME_PATTERN         "\"name\""
#define GEONAMES_LATITUDE_PATTERN     "\"lat\""
#define GEONAMES_ELEVATION_PATTERN    "\"elevation\""
#define GEONAMES_TITLE_PATTERN        "\"title\""
#define GEONAMES_WIKIPEDIAURL_PATTERN "\"wikipediaUrl\""
#define GEONAMES_THUMBNAILIMG_PATTERN "\"thumbnailImg\""
#define GEONAMES_SEARCH_NOT_FOUND     "not understand the location"




Geoname::Geoname(const Geoname & geoname) : Geoname()
{
	this->name      = geoname.name;
	this->feature   = geoname.feature;
	this->lat_lon   = geoname.lat_lon;
	this->elevation = geoname.elevation;
	this->comment   = geoname.comment;
	this->desc      = geoname.desc;
}




Waypoint * Geoname::create_waypoint(CoordMode coord_mode) const
{
	Waypoint * wp = new Waypoint();

	wp->visible = true;
	wp->coord = Coord(this->lat_lon, coord_mode);
	wp->altitude = Altitude(this->elevation, HeightUnit::Metres);
	wp->set_comment(this->comment);
	wp->set_description(this->desc);
	wp->set_name(this->name);

	/* Use the featue type to generate a suitable waypoint icon
	   http://www.geonames.org/wikipedia/wikipedia_features.html
	   Only a few values supported as only a few symbols make sense. */
	if (!this->feature.isEmpty()) {
		if (this->feature == "city") {
			wp->set_symbol("city (medium)");
		} else if (this->feature == "edu") {
			wp->set_symbol("school");
		} else if (this->feature == "airport") {
			wp->set_symbol("airport");
		} else if (this->feature == "mountain") {
			wp->set_symbol("summit");
		} else if (this->feature == "forest") {
			wp->set_symbol("forest");
		} else {
			; /* Pass. Feature type for which we can't find suitable symbol. */
		}
	}

	return wp;
}




static void free_geoname_list(std::list<Geoname *> & found_places)
{
	for (auto iter = found_places.begin(); iter != found_places.end(); iter++) {
		qDebug() << SG_PREFIX_D << "Deallocating geoname" << (*iter)->name;
		delete *iter;
	}
}




std::list<Geoname *> Geonames::select_from_list(const QString & title, const QStringList & headers, std::list<Geoname *> & geonames, Window * parent)
{
	BasicDialog dialog(title, parent);
	std::list<Geoname *> selected_geonames = a_dialog_select_from_list(dialog, geonames, ListSelectionMode::MultipleItems, ListSelectionWidget::get_headers_for_geoname());

	if (!selected_geonames.size()) {
		Dialog::error(QObject::tr("Nothing was selected"), parent);
	} else {
		for (auto iter = selected_geonames.begin(); iter != selected_geonames.end(); iter++) {
			qDebug() << SG_PREFIX_I << "Selected geoname" << (*iter)->name << (*iter)->lat_lon;
		}
	}

	return selected_geonames; /* Hopefully Named Return Value Optimization will work here. */
}




/**
   Get value of this pair from given @entry:
   "key": "value",

   "quoted" in function name means that we expect the value in the
   @entry to be in quotes.

   TODO_LATER: add code that ensures that we don't search past newline
   character.

   @return value string on success (the string won't contain quotes)
   @return empty string on failure
*/
QString get_quoted_value(const QString & entry, const QString & key)
{
	QString value;

	int key_pos = entry.indexOf(key);
	if (key_pos < 0) {
		/* Can't find key in given JSON entry. */
		return value;
	}
	key_pos += key.length();

	int begin = entry.indexOf("\"", key_pos + 1);
	if (begin < 0) {
		/* Can't find opening quote of value string in given JSON entry. */
		return value;
	}
	begin++; /* Don't include opening quote. */

	int end = entry.indexOf("\"", begin + 1);
	if (end < 0) {
		/* Can't find closing quote of value string in given JSON entry. */
		return value;
	}
	end--; /* Don't include ending quote. */

	value = entry.mid(begin, end - begin + 1);

	qDebug() << SG_PREFIX_I << key << "=" << value;

	return value;
}




/**
   Get value of this pair from given @entry:
   "key": value,

   "unquoted" in function name means that we expect the value in the
   @entry to be without quotes.

   TODO_LATER: add code that ensures that we don't search past newline
   character.

   @return value string on success
   @return empty string on failure
*/
QString get_unquoted_value(const QString & entry, const QString & key)
{
	QString value;

	int key_pos = entry.indexOf(key);
	if (key_pos < 0) {
		/* Can't find key in given JSON entry. */
		return value;
	}
	key_pos += key.length();

	int begin = entry.indexOf(":", key_pos);
	if (begin < 0) {
		/* Can't find separating ":". */
		return value;
	}
	begin++; /* Don't include the ":" character. */

	while (entry[begin].isSpace()) {
		begin++;
	}

	while (!entry[begin].isSpace() && entry[begin] != ',') {
		value.append(entry[begin++]);
	}

	qDebug() << SG_PREFIX_I << key << "=" << value;

	return value;
}




/*
  @file passed to this function is an opened QTemporaryFile.
*/
static std::list<Geoname *> get_entries_from_file(QTemporaryFile & file)
{
	std::list<Geoname *> found_places;

	QString wikipedia_url;
	QString thumbnail_url;

	off_t file_size = file.size();
	unsigned char * file_contents = file.map(0, file_size, QFileDevice::MapPrivateOption);
	if (!file_contents) {
		/* File errors: http://doc.qt.io/qt-5/qfiledevice.html#FileError-enum */
		qDebug() << SG_PREFIX_E << "Can't map file" << file.fileName() << "with size" << file_size << ", error:" << file.error();
		return found_places;
	}

	const QString text = QString((char *) file_contents);

	if (text.contains(GEONAMES_SEARCH_NOT_FOUND)) {
		/* This is probably programmer's error. */
		qDebug() << SG_PREFIX_E << "Don't understand the search term";
		return found_places;
	}

	QStringList entries = text.split("},");

	for (int i = 0; i < entries.size(); i++) {
		Geoname * geoname = new Geoname();
		QString value;
		double latitude = NAN;
		double longitude = NAN;

		value = get_quoted_value(entries[i], GEONAMES_FEATURE_PATTERN);
		if (!value.isEmpty()) {
			geoname->feature = value;
		}

		value = get_unquoted_value(entries[i], GEONAMES_LATITUDE_PATTERN);
		if (!value.isEmpty()) {
			latitude = SGUtils::c_to_double(value.toUtf8().constData());
		}

		value = get_unquoted_value(entries[i], GEONAMES_LONGITUDE_PATTERN);
		if (!value.isEmpty()) {
			longitude = SGUtils::c_to_double(value.toUtf8().constData());
		}

		value = get_unquoted_value(entries[i], GEONAMES_ELEVATION_PATTERN);
		if (!value.isEmpty()) {
			geoname->elevation = SGUtils::c_to_double(value.toUtf8().constData());
		}

		value = get_quoted_value(entries[i], GEONAMES_NAME_PATTERN);
		if (!value.isEmpty()) {
			geoname->name = value;
		}

		value = get_quoted_value(entries[i], GEONAMES_TITLE_PATTERN);
		if (!value.isEmpty()) {
			geoname->name = value;
		}

		value = get_quoted_value(entries[i], GEONAMES_WIKIPEDIAURL_PATTERN);
		if (!value.isEmpty()) {
			wikipedia_url = value;
		}

		value = get_quoted_value(entries[i], GEONAMES_THUMBNAILIMG_PATTERN);
		if (!value.isEmpty()) {
			thumbnail_url = value;
		}

		geoname->lat_lon = LatLon(latitude, longitude);
		if (!geoname->lat_lon.is_valid()) {
			qDebug() << SG_PREFIX_I << "Can't create valid coordinates from" << latitude << longitude << ", skipping the geoname";
			delete geoname;
		} else {
			qDebug() << SG_PREFIX_I << "Created geoname with lat/lon" << geoname->lat_lon;
			if (!wikipedia_url.isEmpty()) {
				/* Really we should support the GPX URL tag and then put that in there... */
				geoname->comment = QString("http://%1").arg(wikipedia_url);
				if (!thumbnail_url.isEmpty()) {
					geoname->desc = QString("<a href=\"http://%1\" target=\"_blank\"><img src=\"%2\" border=\"0\"/></a>").arg(wikipedia_url).arg(thumbnail_url);
				} else {
					geoname->desc = QString("<a href=\"http://%1\" target=\"_blank\">%2</a>").arg(wikipedia_url).arg(geoname->name);
				}
			}

			/* Reset. */
			wikipedia_url = "";
			thumbnail_url = "";

			found_places.push_front(geoname);
		}
	}

	found_places.reverse();
	file.unmap(file_contents);

	return found_places; /* Hopefully Named Return Value Optimization will work here. */
}




void Geonames::create_wikipedia_waypoints(LayerTRW * trw, const LatLonBBox & bbox, Window * window)
{
	const QString north = SGUtils::double_to_c(bbox.north);
	const QString south = SGUtils::double_to_c(bbox.south);
	const QString east = SGUtils::double_to_c(bbox.east);
	const QString west = SGUtils::double_to_c(bbox.west);

	const QString uri = QString(GEONAMES_WIKIPEDIA_URL_FMT).arg(north).arg(south).arg(east).arg(west).arg(GEONAMES_LANG).arg(GEONAMES_MAX_ENTRIES);

	DownloadHandle dl_handle;
	QTemporaryFile tmp_file;
	if (!dl_handle.download_to_tmp_file(tmp_file, uri)) {
		Dialog::info(QObject::tr("Can't download information"), window);
		return;
	}

	std::list<Geoname *> wiki_places = get_entries_from_file(tmp_file);
	tmp_file.close();
	Util::remove(tmp_file);

	if (wiki_places.size() == 0) {
		Dialog::info(QObject::tr("No entries found!"), window);
		return;
	}

	const QStringList headers = { QObject::tr("Select the articles you want to add.") };
	std::list<Geoname *> selected = Geonames::select_from_list(QObject::tr("Select articles"), headers, wiki_places, window);

	for (auto iter = selected.begin(); iter != selected.end(); iter++) {
		const Geoname * geoname = *iter;
		Waypoint * wp = geoname->create_waypoint(trw->get_coord_mode());
		trw->add_waypoint_from_file(wp);
	}

	free_geoname_list(wiki_places);

#if 0
	/* This section of code is not necessary. It is left in place
	   for explanation: 'selected' contains pointer to geonames
	   that were present on 'wiki_places'. Deallocating
	   'wiki_places' also deallocated pointers stored in
	   'selected', so there is no need to call
	   free_geoname_list(selected). */
	free_geoname_list(selected);
#endif

	return;
}
