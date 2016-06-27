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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "viking.h"
#include "vikgototool.h"
#include "vikgoto.h"
#include "dialog.h"
#include "vik_compat.h"
#include "settings.h"

using namespace SlavGPS;



/* Compatibility */
#if ! GLIB_CHECK_VERSION(2,22,0)
#define g_mapped_file_unref g_mapped_file_free
#endif

static char *last_goto_str = NULL;
static VikCoord *last_coord = NULL;
static char *last_successful_goto_str = NULL;

std::vector<GotoTool *> goto_tools;

#define VIK_SETTINGS_GOTO_PROVIDER "goto_provider"
int last_goto_tool = -1;

void vik_goto_register(GotoTool * goto_tool)
{
	goto_tools.push_back(goto_tool); /* , g_object_ref(tool); */
}

void vik_goto_unregister_all()
{
	for (auto iter = goto_tools.begin(); iter != goto_tools.end(); iter++) {
		/* kamilFIXME: delete objects? */
	}
}

char * a_vik_goto_get_search_string_for_this_place(Window * window)
{
	if (!last_coord) {
		return NULL;
	}

	const VikCoord * cur_center = window->get_viewport()->get_center();
	if (vik_coord_equals(cur_center, last_coord)) {
		return(last_successful_goto_str);
	} else {
		return NULL;
	}
}

static void display_no_tool(Window * window)
{
	GtkWidget *dialog = NULL;

	dialog = gtk_message_dialog_new(GTK_WINDOW(window->vw), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, _("No goto tool available."));

	gtk_dialog_run(GTK_DIALOG(dialog));

	gtk_widget_destroy(dialog);
}

static bool prompt_try_again(Window * window, const char *msg)
{
	GtkWidget *dialog = NULL;
	bool ret = true;

	dialog = gtk_dialog_new_with_buttons("", GTK_WINDOW(window->vw), (GtkDialogFlags) 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);
	gtk_window_set_title(GTK_WINDOW(dialog), _("goto"));

	GtkWidget *goto_label = gtk_label_new(msg);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), goto_label, false, false, 5);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	gtk_widget_show_all(dialog);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_ACCEPT) {
		ret = false;
	}

	gtk_widget_destroy(dialog);
	return ret;
}

static int find_entry = -1;
static int wanted_entry = -1;

static void find_provider(GotoTool * goto_tool, char * provider)
{
	find_entry++;
	if (0 == strcmp(goto_tool->get_label(), provider)) {
		wanted_entry = find_entry;
	}
}

/**
 * Setup last_goto_tool value
 */
static void get_provider()
{
	// Use setting for the provider if available
	if (last_goto_tool < 0) {
		find_entry = -1;
		wanted_entry = -1;
		char *provider = NULL;
		if (a_settings_get_string(VIK_SETTINGS_GOTO_PROVIDER, &provider)) {
			// Use setting
			if (provider) {
				for (auto iter = goto_tools.begin(); iter != goto_tools.end(); iter++) {
					find_provider(*iter, provider);
				}

			}
			// If not found set it to the first entry, otherwise use the entry
			last_goto_tool = (wanted_entry < 0) ? 0 : wanted_entry;
		} else {
			last_goto_tool = 0;
		}
	}
}

static void text_changed_cb(GtkEntry   *entry,
			    GParamSpec *pspec,
			    GtkWidget  *button)
{
	bool has_text = gtk_entry_get_text_length(entry) > 0;
	gtk_entry_set_icon_sensitive(entry, GTK_ENTRY_ICON_SECONDARY, has_text);
	gtk_widget_set_sensitive(button, has_text);
}

static char *a_prompt_for_goto_string(Window * window)
{
	GtkWidget *dialog = NULL;

	dialog = gtk_dialog_new_with_buttons("", GTK_WINDOW(window->vw), (GtkDialogFlags) 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);
	gtk_window_set_title(GTK_WINDOW(dialog), _("goto"));

	GtkWidget *tool_label = gtk_label_new(_("goto provider:"));
	GtkWidget *tool_list = vik_combo_box_text_new();
	for (auto iter = goto_tools.begin(); iter != goto_tools.end(); iter++) {
		GotoTool * goto_tool = *iter;
		char * label = goto_tool->get_label();
		vik_combo_box_text_append(tool_list, label);
	}

	get_provider();
	gtk_combo_box_set_active(GTK_COMBO_BOX(tool_list), last_goto_tool);

	GtkWidget *goto_label = gtk_label_new(_("Enter address or place name:"));
	GtkWidget *goto_entry = gtk_entry_new();
	if (last_goto_str) {
		gtk_entry_set_text(GTK_ENTRY(goto_entry), last_goto_str);
	}

	// 'ok' when press return in the entry
	g_signal_connect_swapped(goto_entry, "activate", G_CALLBACK(a_dialog_response_accept), dialog);

#if GTK_CHECK_VERSION (2,20,0)
	GtkWidget *ok_button = gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	text_changed_cb(GTK_ENTRY(goto_entry), NULL, ok_button);
	g_signal_connect(goto_entry, "notify::text", G_CALLBACK (text_changed_cb), ok_button);
#endif
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), tool_label, false, false, 5);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), tool_list, false, false, 5);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), goto_label, false, false, 5);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), goto_entry, false, false, 5);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	gtk_widget_show_all(dialog);

	// Ensure the text field has focus so we can start typing straight away
	gtk_widget_grab_focus(goto_entry);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_ACCEPT) {
		gtk_widget_destroy(dialog);
		return NULL;
	}

	// TODO check if list is empty
	last_goto_tool = gtk_combo_box_get_active(GTK_COMBO_BOX(tool_list));
	char * provider = goto_tools[last_goto_tool]->get_label();
	a_settings_set_string(VIK_SETTINGS_GOTO_PROVIDER, provider);

	char *goto_str = g_strdup(gtk_entry_get_text(GTK_ENTRY(goto_entry)));

	gtk_widget_destroy(dialog);

	if (goto_str[0] != '\0') {
		if (last_goto_str) {
			free(last_goto_str);
		}
		last_goto_str = g_strdup(goto_str);
	}

	return(goto_str);   /* goto_str needs to be freed by caller */
}

/**
 * Goto a place when we already have a string to search on
 *
 * Returns: %true if a successful lookup
 */
static bool vik_goto_place(Window * window, Viewport * viewport, char* name, VikCoord *vcoord)
{
	// Ensure last_goto_tool is given a value
	get_provider();

	if (!goto_tools.empty() && last_goto_tool >= 0) {
		GotoTool * goto_tool = goto_tools[last_goto_tool];
		if (goto_tool) {
			if (goto_tool->get_coord(window, viewport, name, vcoord) == 0) {
				return true;
			}
		}
	}
	return false;
}

void a_vik_goto(Window * window, Viewport * viewport)
{
	VikCoord new_center;
	char *s_str;
	bool more = true;

	if (goto_tools.empty()) {
		display_no_tool(window);
		return;
	}

	do {
		s_str = a_prompt_for_goto_string(window);
		if ((!s_str) || (s_str[0] == 0)) {
			more = false;
		} else {
			int ans = goto_tools[last_goto_tool]->get_coord(window, viewport, s_str, &new_center);
			if (ans == 0) {
				if (last_coord) {
					free(last_coord);
				}
				last_coord = (VikCoord *) malloc(sizeof(VikCoord));
				*last_coord = new_center;
				if (last_successful_goto_str) {
					free(last_successful_goto_str);
				}
				last_successful_goto_str = g_strdup(last_goto_str);
				viewport->set_center_coord(&new_center, true);
				more = false;
			} else if (ans == -1) {
				if (!prompt_try_again(window, _("I don't know that place. Do you want another goto?"))) {
					more = false;
				}
			} else if (!prompt_try_again(window, _("Service request failure. Do you want another goto?"))) {
				more = false;
			}
		}
		free(s_str);
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
 * @name: Contains the name of place found. Free this string after use.
 */
int a_vik_goto_where_am_i(Viewport * viewport, struct LatLon *ll, char **name)
{
	int result = 0;
	*name = NULL;

	char *tmpname = a_download_uri_to_tmp_file("http://api.hostip.info/get_json.php?position=true", NULL);
	//char *tmpname = strdup("../test/hostip2.json");
	if (!tmpname) {
		return result;
	}

	ll->lat = 0.0;
	ll->lon = 0.0;

	char *pat;
	GMappedFile *mf;
	char *ss;
	int fragment_len;

	char lat_buf[32], lon_buf[32];
	lat_buf[0] = lon_buf[0] = '\0';
	char *country = NULL;
	char *city = NULL;

	if ((mf = g_mapped_file_new(tmpname, false, NULL)) == NULL) {
		fprintf(stderr, _("CRITICAL: couldn't map temp file\n"));

		g_mapped_file_unref(mf);
		(void) remove(tmpname);
		free(tmpname);
		return result;
	}

	size_t len = g_mapped_file_get_length(mf);
	char *text = g_mapped_file_get_contents(mf);

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

	if (ll->lat != 0.0 && ll->lon != 0.0) {
		if (ll->lat > -90.0 && ll->lat < 90.0 && ll->lon > -180.0 && ll->lon < 180.0) {
			// Found a 'sensible' & 'precise' location
			result = 1;
			*name = strdup(_("Locality")); //Albeit maybe not known by an actual name!
		}
	} else {
		// Hopefully city name is unique enough to lookup position on
		// Maybe for American places where hostip appends the State code on the end
		// But if the country code is not appended if could easily get confused
		//  e.g. 'Portsmouth' could be at least
		//   Portsmouth, Hampshire, UK or
		//   Portsmouth, Viginia, USA.

		// Try city name lookup
		if (city) {
			fprintf(stderr, "DEBUG: %s: found city %s\n", __FUNCTION__, city);
			if (strcmp(city, "(Unknown city)") != 0) {
				VikCoord new_center;
				if (vik_goto_place(NULL, viewport, city, &new_center)) {
					// Got something
					vik_coord_to_latlon(&new_center, ll);
					result = 2;
					*name = city;
					goto tidy;
				}
			}
		}

		// Try country name lookup
		if (country) {
			fprintf(stderr, "DEBUG: %s: found country %s\n", __FUNCTION__, country);
			if (strcmp(country, "(Unknown Country)") != 0) {
				VikCoord new_center;
				if (vik_goto_place(NULL, viewport, country, &new_center)) {
					// Finally got something
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
