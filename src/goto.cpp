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

#include "goto_tool.h"
#include "goto.h"
#include "dialog.h"
#include "settings.h"
#include "util.h"
#include "degrees_converters.h"




using namespace SlavGPS;




static int last_goto_idx = -1;
static char * last_location = NULL;
static VikCoord * last_coord = NULL;
static char * last_successful_location = NULL;

std::vector<GotoTool *> goto_tools;

#define VIK_SETTINGS_GOTO_PROVIDER "goto_provider"



static bool goto_latlon_dialog(Window * parent, struct LatLon * ll, const struct LatLon * old);
static bool goto_utm_dialog(Window * parent, struct UTM * utm, const struct UTM * old);
static char * goto_location_dialog(Window * window);



void SlavGPS::vik_goto_register(GotoTool * goto_tool)
{
	goto_tools.push_back(goto_tool);
}




void SlavGPS::vik_goto_unregister_all()
{
	for (auto iter = goto_tools.begin(); iter != goto_tools.end(); iter++) {
		/* kamilFIXME: delete objects? */
	}
}




char * SlavGPS::a_vik_goto_get_search_string_for_this_location(Window * window)
{
	if (!last_coord) {
		return NULL;
	}

	const VikCoord * cur_center = window->get_viewport()->get_center();
	if (vik_coord_equals(cur_center, last_coord)) {
		return(last_successful_location);
	} else {
		return NULL;
	}
}




static bool prompt_try_again(Window * parent, QString const & msg)
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
	char * provider = NULL;
	if (a_settings_get_string(VIK_SETTINGS_GOTO_PROVIDER, &provider)) {
		/* Use setting. */
		if (provider) {
			int i = 0;
			for (auto iter = goto_tools.begin(); iter != goto_tools.end(); iter++) {
				GotoTool * goto_tool = *iter;
				if (0 == strcmp(goto_tool->get_label(), provider)) {
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




static void text_changed_cb(GtkEntry * entry, GParamSpec * pspec, GtkWidget * button)
{
#ifdef K
	bool has_text = gtk_entry_get_text_length(entry) > 0;
	gtk_entry_set_icon_sensitive(entry, GTK_ENTRY_ICON_SECONDARY, has_text);
	gtk_widget_set_sensitive(button, has_text);
#endif
}




/**
   @brief Get name of a location to go to

   Returned pointer is owned by caller.

   @return NULL on errors
   @return NULL if empty string has been entered
   @return non-NULL string with location entered in dialog on success
*/
char * goto_location_dialog(Window * window)
{
	QDialog dialog(window);
	dialog.setWindowTitle(QObject::tr("goto"));

	QVBoxLayout vbox;
	QLayout * old = dialog.layout();
	delete old;
	dialog.setLayout(&vbox);


	QLabel tool_label(QObject::tr("goto provider:"));
	vbox.addWidget(&tool_label);


	QComboBox providers_combo;
	int i = 0;
	for (auto iter = goto_tools.begin(); iter != goto_tools.end(); iter++) {
		GotoTool * goto_tool = *iter;
		QString label(goto_tool->get_label());
		providers_combo.addItem(label, i);
		i++;
	}
	last_goto_idx = get_last_provider_index();
	providers_combo.setCurrentIndex(last_goto_idx);
	vbox.addWidget(&providers_combo);


	QLabel prompt_label(QObject::tr("Enter address or location name:"));
	vbox.addWidget(&prompt_label);


	QLineEdit input_field;
	QObject::connect(&input_field, SIGNAL (returnPressed(void)), &dialog, SLOT(accept()));
	if (last_location) {
		/* Notice that this may be not a *successful* location. */
		input_field.setText(last_location);
	}
#ifdef K
#if GTK_CHECK_VERSION (2,20,0)
	GtkWidget *ok_button = gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	text_changed_cb(GTK_ENTRY(goto_entry), NULL, ok_button);
	QObject::connect(goto_entry, SIGNAL("notify::text"), ok_button, SLOT (text_changed_cb));
#endif
#endif
	vbox.addWidget(&input_field);


	QDialogButtonBox button_box;
	button_box.addButton(QDialogButtonBox::Ok);
	button_box.addButton(QDialogButtonBox::Cancel);
	QObject::connect(&button_box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	QObject::connect(&button_box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	vbox.addWidget(&button_box);


	/* Ensure the text field has focus so we can start typing straight away. */
	input_field.setFocus(Qt::OtherFocusReason);


	if (dialog.exec() != QDialog::Accepted) {
		return NULL;
	}


	/* TODO check if list is empty. */
	last_goto_idx = providers_combo.currentIndex();
	char * provider = goto_tools[last_goto_idx]->get_label();
	a_settings_set_string(VIK_SETTINGS_GOTO_PROVIDER, provider);
	char * location = strdup(input_field.text().toUtf8().constData());
	if (!location) {
		qDebug() << "EE: goto: can't strdup string" << input_field.text();
		return NULL;
	}

	if (location[0] == '\0') {
		free(location);
		location = NULL;
	} else {
		if (last_location) {
			free(last_location);
		}
		last_location = strdup(location);
	}

	return location;
}




/**
 * Goto a location when we already have a string to search on.
 *
 * Returns: %true if a successful lookup
 */
static bool vik_goto_location(Viewport * viewport, char* name, VikCoord *vcoord)
{
	/* Ensure last_goto_idx is given a value. */
	last_goto_idx = get_last_provider_index();

	if (!goto_tools.empty() && last_goto_idx >= 0) {
		GotoTool * goto_tool = goto_tools[last_goto_idx];
		if (goto_tool) {
			if (goto_tool->get_coord(viewport, name, vcoord) == 0) {
				return true;
			}
		}
	}
	return false;
}




void SlavGPS::goto_location(Window * window, Viewport * viewport)
{
	if (goto_tools.empty()) {
		dialog_warning(QObject::tr("No goto tool available."), window);
		return;
	}

	bool more = true;
	do {
		char * location = goto_location_dialog(window);
		if (!location) {
			more = false;
		} else {
			VikCoord location_coord;
			int ans = goto_tools[last_goto_idx]->get_coord(viewport, location, &location_coord);
			if (ans == 0) {
				if (last_coord) {
					free(last_coord);
				}
				last_coord = (VikCoord *) malloc(sizeof(VikCoord));
				*last_coord = location_coord;
				if (last_successful_location) {
					free(last_successful_location);
				}
				last_successful_location = g_strdup(last_location);
				viewport->set_center_coord(&location_coord, true);
				more = false;
			} else if (ans == -1) {
				if (!prompt_try_again(window, QObject::tr("I don't know that location. Do you want another goto?"))) {
					more = false;
				}
			} else if (!prompt_try_again(window, QObject::tr("Service request failure. Do you want another goto?"))) {
				more = false;
			}
			free(location);
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
int SlavGPS::a_vik_goto_where_am_i(Viewport * viewport, struct LatLon *ll, char **name)
{
	*name = NULL;

	char * tmpname = a_download_uri_to_tmp_file("http://api.hostip.info/get_json.php?position=true", NULL);
	//char *tmpname = strdup("../test/hostip2.json");
	if (!tmpname) {
		return 0;
	}

	ll->lat = 0.0;
	ll->lon = 0.0;

	GMappedFile *mf;
	if ((mf = g_mapped_file_new(tmpname, false, NULL)) == NULL) {
		qCritical() << QObject::tr("CRITICAL: couldn't map temp file\n");

		g_mapped_file_unref(mf);
		(void) remove(tmpname);
		free(tmpname);
		return 0;
	}

	size_t len = g_mapped_file_get_length(mf);
	char *text = g_mapped_file_get_contents(mf);

	char *pat;
	char *ss;
	int fragment_len;
	char *country = NULL;
	if ((pat = g_strstr_len(text, len, HOSTIP_COUNTRY_PATTERN))) {
		pat += strlen(HOSTIP_COUNTRY_PATTERN);
		fragment_len = 0;
		ss = pat;
		while (*pat != '"') {
			fragment_len++;
			pat++;
		}
		country = g_strndup(ss, fragment_len);
	}

	char *city = NULL;
	if ((pat = g_strstr_len(text, len, HOSTIP_CITY_PATTERN))) {
		pat += strlen(HOSTIP_CITY_PATTERN);
		fragment_len = 0;
		ss = pat;
		while (*pat != '"') {
			fragment_len++;
			pat++;
		}
		city = g_strndup(ss, fragment_len);
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
		ll->lat = g_ascii_strtod(lat_buf, NULL);
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
		ll->lon = g_ascii_strtod(lon_buf, NULL);
	}

	int result = 0;
	if (ll->lat != 0.0 && ll->lon != 0.0) {
		if (ll->lat > -90.0 && ll->lat < 90.0 && ll->lon > -180.0 && ll->lon < 180.0) {
			// Found a 'sensible' & 'precise' location
			result = 1;
			*name = strdup(_("Locality")); //Albeit maybe not known by an actual name!
		}
	} else {
		/* Hopefully city name is unique enough to lookup position on.
		   Maybe for American locations where hostip appends the State code on the end.
		   But if the country code is not appended if could easily get confused.
		   e.g. 'Portsmouth' could be at least
		   Portsmouth, Hampshire, UK or
		   Portsmouth, Viginia, USA. */

		/* Try city name lookup. */
		if (city) {
			fprintf(stderr, "DEBUG: %s: found city %s\n", __FUNCTION__, city);
			if (strcmp(city, "(Unknown city)") != 0) {
				VikCoord new_center;
				if (vik_goto_location(viewport, city, &new_center)) {
					/* Got something. */
					vik_coord_to_latlon(&new_center, ll);
					result = 2;
					*name = city;
					goto tidy;
				}
			}
		}

		/* Try country name lookup. */
		if (country) {
			fprintf(stderr, "DEBUG: %s: found country %s\n", __FUNCTION__, country);
			if (strcmp(country, "(Unknown Country)") != 0) {
				VikCoord new_center;
				if (vik_goto_location(viewport, country, &new_center)) {
					/* Finally got something. */
					vik_coord_to_latlon(&new_center, ll);
					result = 3;
					*name = country;
					goto tidy;
				}
			}
		}
	}

 tidy:
	g_mapped_file_unref(mf);
	(void) remove(tmpname);
	free(tmpname);
	return result;
}


void SlavGPS::goto_latlon(Window * window, Viewport * viewport)
{
	VikCoord new_center;

	struct LatLon ll, llold;
	vik_coord_to_latlon(viewport->get_center(), &llold);

	if (goto_latlon_dialog(window, &ll, &llold)) {
		vik_coord_load_from_latlon(&new_center, viewport->get_coord_mode(), &ll);
	} else {
		return;
	}

	viewport->set_center_coord(&new_center, true);

	return;
}



bool goto_latlon_dialog(SlavGPS::Window * parent, struct LatLon * ll, const struct LatLon * old)
{
	char buffer[64] = { 0 };


	QDialog dialog(parent);
	dialog.setWindowTitle(QObject::tr("Go to Lat/Lon"));


	QVBoxLayout vbox;
	QLayout * old_layout = dialog.layout();
	delete old_layout;
	dialog.setLayout(&vbox);


	QLabel lat_label(QObject::tr("Latitude:"));
	vbox.addWidget(&lat_label);


	QLineEdit lat_input;
	QObject::connect(&lat_input, SIGNAL (returnPressed(void)), &dialog, SLOT(accept()));
	snprintf(buffer, sizeof (buffer), "%f", old->lat);
	lat_input.setText(buffer);
	vbox.addWidget(&lat_input);


	QLabel lon_label(QObject::tr("Longitude:"));
	vbox.addWidget(&lon_label);


	QLineEdit lon_input;
	QObject::connect(&lon_input, SIGNAL (returnPressed(void)), &dialog, SLOT(accept()));
	snprintf(buffer, sizeof (buffer), "%f", old->lon);
	lon_input.setText(buffer);
	vbox.addWidget(&lon_input);


	QDialogButtonBox button_box;
	button_box.addButton(QDialogButtonBox::Ok);
	button_box.addButton(QDialogButtonBox::Cancel);
	QObject::connect(&button_box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	QObject::connect(&button_box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	vbox.addWidget(&button_box);


	/* Ensure the first text field has focus so we can start typing straight away. */
	lat_input.setFocus(Qt::OtherFocusReason);


	if (dialog.exec() == QDialog::Accepted) {
		/* kamilTODO: what's going on here? Why we use these functions? */
		ll->lat = convert_dms_to_dec(lat_input.text().toUtf8().constData());
		ll->lon = convert_dms_to_dec(lon_input.text().toUtf8().constData());
		return true;
	} else {
		return false;
	}
}




void SlavGPS::goto_utm(Window * window, Viewport * viewport)
{
	VikCoord new_center;

	struct UTM utm, utmold;
	vik_coord_to_utm(viewport->get_center(), &utmold);

	if (goto_utm_dialog(window, &utm, &utmold)) {
		vik_coord_load_from_utm(&new_center, viewport->get_coord_mode(), &utm);
	} else {
		return;
	}

	viewport->set_center_coord(&new_center, true);

	return;
}




bool goto_utm_dialog(SlavGPS::Window * parent, struct UTM * utm, const struct UTM * old)
{
	char buffer[64] = { 0 };


	QDialog dialog(parent);
	dialog.setWindowTitle(QObject::tr("Go to UTM"));


	QVBoxLayout vbox;
	QLayout * old_layout = dialog.layout();
	delete old_layout;
	dialog.setLayout(&vbox);


	QLabel northing_label(QObject::tr("Northing:"));
	vbox.addWidget(&northing_label);


	QLineEdit northing_input;
	QObject::connect(&northing_input, SIGNAL (returnPressed(void)), &dialog, SLOT(accept()));
	snprintf(buffer, sizeof (buffer), "%f", old->northing);
	northing_input.setText(buffer);
	vbox.addWidget(&northing_input);


	QLabel easting_label(QObject::tr("Easting:"));
	vbox.addWidget(&easting_label);


	QLineEdit easting_input;
	QObject::connect(&easting_input, SIGNAL (returnPressed(void)), &dialog, SLOT(accept()));
	snprintf(buffer, sizeof (buffer), "%f", old->easting);
	easting_input.setText(buffer);
	vbox.addWidget(&easting_input);


	QLabel zone_label(QObject::tr("Zone:"));
	vbox.addWidget(&zone_label);


	QSpinBox zone_spinbox;
	QObject::connect(&zone_spinbox, SIGNAL (returnPressed(void)), &dialog, SLOT(accept()));
	zone_spinbox.setValue(old->zone);
	zone_spinbox.setMinimum(1);
	zone_spinbox.setMaximum(60);
	zone_spinbox.setSingleStep(1);
	vbox.addWidget(&zone_spinbox);


	QLabel letter_label(QObject::tr("Letter:"));
	vbox.addWidget(&letter_label);


	QLineEdit letter_input;
	QObject::connect(&letter_input, SIGNAL (returnPressed(void)), &dialog, SLOT(accept()));
	buffer[0] = old->letter;
	buffer[1] = '\0';
	letter_input.setText(buffer);
	letter_input.setMaxLength(1);
	letter_input.setInputMask("A");
	vbox.addWidget(&letter_input);


	QDialogButtonBox button_box;
	button_box.addButton(QDialogButtonBox::Ok);
	button_box.addButton(QDialogButtonBox::Cancel);
	QObject::connect(&button_box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	QObject::connect(&button_box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	vbox.addWidget(&button_box);


	/* Ensure the first text field has focus so we can start typing straight away. */
	northing_input.setFocus(Qt::OtherFocusReason);


	if (dialog.exec() == QDialog::Accepted) {
		utm->northing = atof(northing_input.text().toUtf8().constData()); /* kamilTODO: why atof()? */
		utm->easting = atof(easting_input.text().toUtf8().constData());
		utm->zone = zone_spinbox.value();
		const char * letter = letter_input.text().toUtf8().constData();
		if (*letter) {
			utm->letter = toupper(*letter);
		}
		return true;
	} else {
		return false;
	}
}
