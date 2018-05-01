/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vector>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <glib.h>

#include <QDebug>
#include <QLineEdit>
#include <QDir>

#include "window.h"
#include "goto_tool.h"
#include "goto.h"
#include "dialog.h"
#include "application_state.h"
#include "util.h"
#include "degrees_converters.h"
#include "viewport_internal.h"
#include "vikutils.h"
#include "widget_utm_entry.h"
#include "widget_lat_lon_entry.h"




using namespace SlavGPS;




#define PREFIX ": GoTo:" << __FUNCTION__ << __LINE__ << ">"
#define VIK_SETTINGS_GOTO_PROVIDER "goto_provider"




static int last_goto_idx = -1;
static QString last_location;
static std::vector<GotoTool *> goto_tools;

static bool goto_latlon_dialog(LatLon & new_lat_lon, const LatLon & initial_lat_lon, Window * parent);
static bool goto_utm_dialog(UTM & new_utm, const UTM & initial_utm, Window * parent);
static QString goto_location_dialog(Window * window);



void GoTo::register_tool(GotoTool * goto_tool)
{
	goto_tools.push_back(goto_tool);
}




void GoTo::unregister_all_tools()
{
	for (auto iter = goto_tools.begin(); iter != goto_tools.end(); iter++) {
		delete *iter;
	}
}




static bool prompt_try_again(QString const & msg, Window * parent)
{
	return QMessageBox::Yes == QMessageBox::question(parent, QObject::tr("goto"), msg,
							 QMessageBox::No | QMessageBox::Yes,
							 QMessageBox::Yes);
}




/**
   Get last_goto_idx value.

   Use setting for the provider if necessary and available.
*/
static int get_last_provider_index(void)
{
	if (last_goto_idx >= 0) {
		return last_goto_idx;
	}

	int found_entry = -1;
	QString provider;
	if (ApplicationState::get_string(VIK_SETTINGS_GOTO_PROVIDER, provider)) {
		/* Use setting. */
		if (!provider.isEmpty()) {
			int i = 0;
			for (auto iter = goto_tools.begin(); iter != goto_tools.end(); iter++) {
				GotoTool * goto_tool = *iter;
				if (goto_tool->get_label() == provider) {
					found_entry = i;
					break;
				}
				i++;
			}
		}
		/* If not found set it to the first entry, otherwise use the entry. */
		last_goto_idx = (found_entry < 0) ? 0 : found_entry;
	} else {
		last_goto_idx = 0;
	}

	return last_goto_idx;
}




void GotoDialog::text_changed_cb(const QString & text)
{
	QPushButton * ok_button = this->button_box->button(QDialogButtonBox::Ok);
	ok_button->setEnabled(!text.isEmpty());
}




GotoDialog::GotoDialog(QWidget * parent) : BasicDialog(parent)
{
	this->setWindowTitle(tr("goto"));

	QLabel * tool_label = new QLabel(tr("goto provider:"), this);
	this->grid->addWidget(tool_label, 0, 0);


	int i = 0;
	for (auto iter = goto_tools.begin(); iter != goto_tools.end(); iter++) {
		GotoTool * goto_tool = *iter;
		QString label(goto_tool->get_label());
		this->providers_combo.addItem(label, i);
		i++;
	}
	last_goto_idx = get_last_provider_index();
	this->providers_combo.setCurrentIndex(last_goto_idx);
	this->grid->addWidget(&this->providers_combo, 1, 0);


	QLabel * prompt_label = new QLabel(tr("Enter address or location name:"), this);
	this->grid->addWidget(prompt_label, 2, 0);


	QObject::connect(&this->input_field, SIGNAL (returnPressed(void)), this, SLOT(accept()));
	QObject::connect(&this->input_field, SIGNAL (textChanged(const QString &)), this, SLOT (text_changed_cb(const QString &)));
	if (!last_location.isEmpty()) {
		/* Notice that this may be not a *successful* location. */
		this->input_field.setText(last_location);
	}
	/* Set initial 'enabled = X' state of button. */
	this->text_changed_cb(last_location);
	this->grid->addWidget(&this->input_field, 3, 0);


	/* Ensure the text field has focus so we can start typing straight away. */
	this->input_field.setFocus(Qt::OtherFocusReason);
}




GotoDialog::~GotoDialog()
{
}




/**
   @brief Get name of a location to go to

   @return NULL on errors
   @return NULL if empty string has been entered
   @return non-NULL string with location entered in dialog on success
*/
QString goto_location_dialog(Window * window)
{
	QString empty_string("");


	GotoDialog dialog;
	if (0 == dialog.providers_combo.count()) {
		Dialog::error(QObject::tr("There are no GoTo engines available."), window);
		return empty_string;
	}


	if (dialog.exec() != QDialog::Accepted) {
		return empty_string;
	}


	last_goto_idx = dialog.providers_combo.currentIndex();
	ApplicationState::set_string(VIK_SETTINGS_GOTO_PROVIDER, goto_tools[last_goto_idx]->get_label());
	const QString location = dialog.input_field.text();
	if (location.isEmpty()) {
		qDebug() << "EE: goto: can't get string" << dialog.input_field.text();
		return empty_string;
	}

	last_location = location;

	return location;
}




/**
   \brief Get a coordinate of specified name

   Returns: %true if a successful lookup
*/
static bool get_coordinate_of(Viewport * viewport, const QString & name, /* in/out */ Coord * coord)
{
	if (goto_tools.empty()) {
		return false;
	}

	/* Ensure last_goto_idx is given a value. */
	last_goto_idx = get_last_provider_index();
	if (last_goto_idx < 0) {
		return false;
	}

	GotoTool * goto_tool = goto_tools[last_goto_idx];
	if (!goto_tool) {
		return false;
	}

	if (goto_tool->get_coord(viewport, name, coord) != GotoToolResult::Found) {
		return false;
	}

	return true;
}




void GoTo::goto_location(Window * window, Viewport * viewport)
{
	if (goto_tools.empty()) {
		Dialog::warning(QObject::tr("No goto tool available."), window);
		return;
	}

	bool more = true;
	do {
		const QString location = goto_location_dialog(window);
		if (location.isEmpty()) {
			more = false;
		} else {
			Coord location_coord;
			GotoToolResult ans = goto_tools[last_goto_idx]->get_coord(viewport, location.toUtf8().data(), &location_coord);
			switch (ans) {
			case GotoToolResult::Found:
				viewport->set_center_from_coord(location_coord, true);
				more = false;
				break;
			case GotoToolResult::NotFound:
				more = prompt_try_again(QObject::tr("I don't know that location. Do you want another goto?"), window);
				break;
			case GotoToolResult::Error:
			default:
				more = prompt_try_again(QObject::tr("Service request failure. Do you want another goto?"), window);
				break;
			}
		}
	} while (more);
}




#define HOSTIP_LATITUDE_PATTERN "\"lat\":\""
#define HOSTIP_LONGITUDE_PATTERN "\"lng\":\""
#define HOSTIP_CITY_PATTERN "\"city\":\""
#define HOSTIP_COUNTRY_PATTERN "\"country_name\":\""




/**
 * Automatic attempt to find out where you are using:
 *   1. http://www.hostip.info ++
 *   2. if not specific enough fallback to using the default goto tool with a country name
 * ++ Using returned JSON information
 *  c.f. with googlesearch.c - similar implementation is used here
 *
 * returns:
 *   0 if failed to locate anything
 *   1 if exact latitude/longitude found
 *   2 if position only as precise as a city
 *   3 if position only as precise as a country
 * @name: Contains the name of location found. Free this string after use.
 */
int GoTo::where_am_i(Viewport * viewport, LatLon & lat_lon, QString & name)
{
	name = "";

	DownloadHandle dl_handle;
	QTemporaryFile tmp_file;
	//char *tmpname = strdup("../test/hostip2.json");
	if (!dl_handle.download_to_tmp_file(tmp_file, "http://api.hostip.info/get_json.php?position=true")) {
		return 0;
	}

	lat_lon.invalidate();

	if (!tmp_file.open()) {
		qDebug() << "EE" PREFIX << "Can't open file" << tmp_file.fileName() << tmp_file.error();
		tmp_file.remove();
		return 0;
	}

	off_t file_size = tmp_file.size();
	unsigned char * file_contents = tmp_file.map(0, file_size, QFileDevice::MapPrivateOption);
	if (!file_contents) {
		qDebug() << "EE" PREFIX << "Can't map file" << tmp_file.fileName() << tmp_file.error();
		tmp_file.remove();
		return 0;
	}


	size_t len = file_size;
	char * text = (char *) file_contents;

	char *pat;
	char *ss;
	int fragment_len;
	QString country;

	if ((pat = g_strstr_len(text, len, HOSTIP_COUNTRY_PATTERN))) {
		pat += strlen(HOSTIP_COUNTRY_PATTERN);
		fragment_len = 0;
		ss = pat;
		while (*pat != '"') {
			fragment_len++;
			pat++;
		}
		country = QString(ss).left(fragment_len);
	}

	QString city;
	if ((pat = g_strstr_len(text, len, HOSTIP_CITY_PATTERN))) {
		pat += strlen(HOSTIP_CITY_PATTERN);
		fragment_len = 0;
		ss = pat;
		while (*pat != '"') {
			fragment_len++;
			pat++;
		}
		city = QString(ss).left(fragment_len);
	}

	char lat_buf[32] = { 0 };
	char lon_buf[32] = { 0 };

	if ((pat = g_strstr_len(text, len, HOSTIP_LATITUDE_PATTERN))) {
		pat += strlen(HOSTIP_LATITUDE_PATTERN);
		ss = lat_buf;
		if (*pat == '-') {
			*ss++ = *pat++;
		}

		while ((ss < (lat_buf + sizeof(lat_buf))) && (pat < (text + len)) &&
		       (g_ascii_isdigit(*pat) || (*pat == '.'))) {

			*ss++ = *pat++;
		}
		*ss = '\0';
		lat_lon.lat = SGUtils::c_to_double(lat_buf);
	}

	if ((pat = g_strstr_len(text, len, HOSTIP_LONGITUDE_PATTERN))) {
		pat += strlen(HOSTIP_LONGITUDE_PATTERN);
		ss = lon_buf;
		if (*pat == '-') {
			*ss++ = *pat++;
		}

		while ((ss < (lon_buf + sizeof(lon_buf))) && (pat < (text + len)) &&
		       (g_ascii_isdigit(*pat) || (*pat == '.'))) {

			*ss++ = *pat++;
		}
		*ss = '\0';
		lat_lon.lon = SGUtils::c_to_double(lon_buf);
	}

	int result = 0;
	if (lat_lon.is_valid()) {
		if (lat_lon.lat > -90.0 && lat_lon.lat < 90.0 && lat_lon.lon > -180.0 && lat_lon.lon < 180.0) {
			// Found a 'sensible' & 'precise' location
			result = 1;
			name = QObject::tr("Locality"); //Albeit maybe not known by an actual name!
		}
	} else {
		/* Hopefully city name is unique enough to lookup position on.
		   Maybe for American locations where hostip appends the State code on the end.
		   But if the country code is not appended if could easily get confused.
		   e.g. 'Portsmouth' could be at least
		   Portsmouth, Hampshire, UK or
		   Portsmouth, Viginia, USA. */

		/* Try city name lookup. */
		if (!city.isEmpty()) {
			qDebug() << "DD" PREFIX << "found city" << city;
			if (city != "(Unknown city)") {
				Coord new_center;
				if (get_coordinate_of(viewport, city, &new_center)) {
					/* Got something. */
					lat_lon = new_center.get_latlon();
					result = 2;
					name = city;
					goto tidy;
				}
			}
		}

		/* Try country name lookup. */
		if (!country.isEmpty()) {
			qDebug() << "DD" PREFIX << "found country" << country;
			if (country != "(Unknown Country)") {
				Coord new_center;
				if (get_coordinate_of(viewport, country, &new_center)) {
					/* Finally got something. */
					lat_lon = new_center.get_latlon();
					result = 3;
					name = country;
					goto tidy;
				}
			}
		}
	}

 tidy:
	tmp_file.unmap(file_contents);
	tmp_file.remove();
	return result;
}




void GoTo::goto_latlon(Window * window, Viewport * viewport)
{
	LatLon new_lat_lon;
	const LatLon initial_lat_lon = viewport->get_center()->get_latlon();

	if (!goto_latlon_dialog(new_lat_lon, initial_lat_lon, window)) {
		return;
	}

	const Coord new_center = Coord(new_lat_lon, viewport->get_coord_mode());
	viewport->set_center_from_coord(new_center, true);

	return;
}



bool goto_latlon_dialog(LatLon & new_lat_lon, const LatLon & initial_lat_lon, Window * parent)
{
	BasicDialog dialog(parent);
	dialog.setWindowTitle(QObject::tr("Go to Lat/Lon"));

	SGLatLonEntry entry;
	entry.set_value(initial_lat_lon);
	dialog.grid->addWidget(&entry, 0, 0);
	entry.setFocus(); /* This will set keyboard focus in first field of entry widget. */

	if (dialog.exec() == QDialog::Accepted) {
#if 1
		new_lat_lon = entry.get_value();
#else
		/* kamilTODO: what's going on here? Why we use these functions? */
		new_lat_lon.lat = convert_dms_to_dec(lat_input.text().toUtf8().constData());
		new_lat_lon.lon = convert_dms_to_dec(lon_input.text().toUtf8().constData());
#endif
		return true;
	} else {
		return false;
	}
}




void GoTo::goto_utm(Window * window, Viewport * viewport)
{
	UTM new_utm;
	const UTM initial_utm = viewport->get_center()->get_utm();

	if (!goto_utm_dialog(new_utm, initial_utm, window)) {
		return;
	}

	const Coord new_center = Coord(new_utm, viewport->get_coord_mode());
	viewport->set_center_from_coord(new_center, true);

	return;
}




bool goto_utm_dialog(UTM & new_utm, const UTM & initial_utm, Window * parent)
{
	BasicDialog dialog(parent);
	dialog.setWindowTitle(QObject::tr("Go to UTM"));

	SGUTMEntry entry;
	entry.set_value(initial_utm);
	dialog.grid->addWidget(&entry, 0, 0);
	entry.setFocus(); /* This will set keyboard focus in first field of entry widget. */

	if (dialog.exec() == QDialog::Accepted) {
		new_utm = entry.get_value();
		return true;
	} else {
		return false;
	}
}
