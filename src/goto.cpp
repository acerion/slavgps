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
 */




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




#define SG_MODULE "GoTo"
#define PREFIX ": GoTo:" << __FUNCTION__ << __LINE__ << ">"
#define VIK_SETTINGS_GOTO_PROVIDER "goto_provider"




static int last_goto_idx = -1;
static QString g_last_location;
static std::vector<GotoTool *> goto_tools;

static LatLon goto_latlon_dialog(const LatLon & initial_lat_lon, Window * parent = NULL);
static int goto_utm_dialog(UTM & new_utm, const UTM & initial_utm, Window * parent = NULL);
static int goto_location_dialog(QString & new_location, const QString & initial_location, Window * parent = NULL);



void GoTo::register_tool(GotoTool * goto_tool)
{
	goto_tools.push_back(goto_tool);
}




void GoTo::uninit(void)
{
	/* Unregister all tools. */
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




GotoDialog::GotoDialog(const QString & initial_location, QWidget * parent) : BasicDialog(parent)
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
	if (!initial_location.isEmpty()) {
		/* Notice that this may be not a *successful* location. */
		this->input_field.setText(initial_location);
	}
	/* Set initial 'enabled = X' state of button. */
	this->text_changed_cb(initial_location);
	this->grid->addWidget(&this->input_field, 3, 0);


	/* Ensure the text field has focus so we can start typing straight away. */
	this->input_field.setFocus(Qt::OtherFocusReason);
}




GotoDialog::~GotoDialog()
{
}




/**
   @brief Get name of a location to go to

   Name of location is returned through @location argument.

   @return QDialog::Accepted if user entered a string
   @return QDialog::Rejected otherwise
*/
int goto_location_dialog(QString & new_location, const QString & initial_location, Window * parent)
{
	GotoDialog dialog(initial_location);
	if (0 == dialog.providers_combo.count()) {
		Dialog::error(QObject::tr("There are no GoTo engines available."), parent);
		new_location = "";
		return QDialog::Rejected;
	}


	if (dialog.exec() != QDialog::Accepted) {
		new_location = "";
		return QDialog::Rejected;
	}


	last_goto_idx = dialog.providers_combo.currentIndex();
	ApplicationState::set_string(VIK_SETTINGS_GOTO_PROVIDER, goto_tools[last_goto_idx]->get_label());

	QString user_input = dialog.input_field.text();
	if (user_input.isEmpty()) {
		qDebug() << "EE: goto: can't get string" << dialog.input_field.text();
		new_location = "";
		return QDialog::Rejected;
	} else {
		new_location = user_input;
	}

	return QDialog::Accepted;
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




bool GoTo::goto_location(Window * window, Viewport * viewport)
{
	bool moved_to_new_position = false;

	if (goto_tools.empty()) {
		Dialog::warning(QObject::tr("No goto tool available."), window);
		return moved_to_new_position;
	}

	bool ask_again = true;
	do {
		QString location;
		if (QDialog::Rejected == goto_location_dialog(location, g_last_location, window)) {
			/* User has cancelled dialog. */
			ask_again = false;
		} else {
			Coord location_coord;
			GotoToolResult ans = goto_tools[last_goto_idx]->get_coord(viewport, location.toUtf8().data(), &location_coord);
			switch (ans) {
			case GotoToolResult::Found:
				viewport->set_center_from_coord(location_coord, true);
				ask_again = false;
				moved_to_new_position = true;
				g_last_location = location;
				break;
			case GotoToolResult::NotFound:
				ask_again = prompt_try_again(QObject::tr("I don't know that location. Do you want another goto?"), window);
				break;
			case GotoToolResult::Error:
			default:
				ask_again = prompt_try_again(QObject::tr("Service request failure. Do you want another goto?"), window);
				break;
			}
		}
	} while (ask_again);

	return moved_to_new_position;
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
	lat_lon.invalidate();

	DownloadHandle dl_handle;
	QTemporaryFile tmp_file;
	//char *tmpname = strdup("../test/hostip2.json");
	if (!dl_handle.download_to_tmp_file(tmp_file, "http://api.hostip.info/get_json.php?position=true")) {
		return 0;
	}

	off_t file_size = tmp_file.size();
	unsigned char * file_contents = tmp_file.map(0, file_size, QFileDevice::MapPrivateOption);
	if (!file_contents) {
		qDebug() << SG_PREFIX_E << "Can't map file" << tmp_file.fileName() << tmp_file.error();
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




bool GoTo::goto_latlon(Window * window, Viewport * viewport)
{
	const LatLon initial_lat_lon = viewport->get_center()->get_latlon();
	const LatLon new_lat_lon = goto_latlon_dialog(initial_lat_lon, window);

	if (!new_lat_lon.is_valid()) {
		return false;
	}

	const Coord new_center = Coord(new_lat_lon, viewport->get_coord_mode());
	viewport->set_center_from_coord(new_center, true);

	return true;
}




/**
   @return valid LatLon if user entered new lat/lon
   @return invalid LatLon otherwise
*/
LatLon goto_latlon_dialog(const LatLon & initial_lat_lon, Window * parent)
{
	LatLon result;

	BasicDialog dialog(parent);
	dialog.setWindowTitle(QObject::tr("Go to Lat/Lon"));

	LatLonEntryWidget entry;
	entry.set_value(initial_lat_lon);
	dialog.grid->addWidget(&entry, 0, 0);
	entry.setFocus(); /* This will set keyboard focus in first field of entry widget. */

	if (dialog.exec() == QDialog::Accepted) {
		result = entry.get_value();
	} else {
		result.invalidate();
	}
	return result;
}




bool GoTo::goto_utm(Window * window, Viewport * viewport)
{
	UTM new_utm;
	const UTM initial_utm = viewport->get_center()->get_utm();

	if (QDialog::Rejected == goto_utm_dialog(new_utm, initial_utm, window)) {
		return false;
	}

	const Coord new_center = Coord(new_utm, viewport->get_coord_mode());
	viewport->set_center_from_coord(new_center, true);

	return true;
}




int goto_utm_dialog(UTM & new_utm, const UTM & initial_utm, Window * parent)
{
	BasicDialog dialog(parent);
	dialog.setWindowTitle(QObject::tr("Go to UTM"));

	UTMEntryWidget entry;
	entry.set_value(initial_utm);
	dialog.grid->addWidget(&entry, 0, 0);
	entry.setFocus(); /* This will set keyboard focus in first field of entry widget. */

	if (dialog.exec() == QDialog::Accepted) {
		new_utm = entry.get_value();
		return QDialog::Accepted;
	} else {
		return QDialog::Rejected;
	}
}
