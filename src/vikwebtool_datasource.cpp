/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013-2015, Rob Norris <rw_norris@hotmail.com>
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

#include "vikwebtool_datasource.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "globals.h"
#include "acquire.h"
#include "maputils.h"
#include "dialog.h"





using namespace SlavGPS;





#define MAX_NUMBER_CODES 7





static GHashTable *last_user_strings = NULL;





typedef struct {
	WebToolDatasource * web_tool_datasource;
	Window * window;
	Viewport * viewport;
	GtkWidget * user_string;
} datasource_t;





static void ensure_last_user_strings_hash()
{
	if (last_user_strings == NULL) {
		last_user_strings = g_hash_table_new_full(g_str_hash,
							  g_str_equal,
							  g_free,
							  g_free);
	}
}





static char * get_last_user_string(const datasource_t *source)
{
	ensure_last_user_strings_hash();
	char * label = source->web_tool_datasource->get_label();
	char *last_str = (char *) g_hash_table_lookup(last_user_strings, label);
	free(label);
	return last_str;
}





static void set_last_user_string(const datasource_t *source, const char *s)
{
	ensure_last_user_strings_hash();
	g_hash_table_insert(last_user_strings,
			    source->web_tool_datasource->get_label(),
			    g_strdup(s));
}





static void * datasource_init(acq_vik_t *avt)
{
	datasource_t *data = (datasource_t *) malloc(sizeof(*data));
	data->web_tool_datasource = (WebToolDatasource *) avt->userdata;
	data->window = avt->window;
	data->viewport = avt->viewport;
	data->user_string = NULL;
	return data;
}





static void datasource_add_setup_widgets(GtkWidget *dialog, Viewport * viewport, void * user_data)
{
	datasource_t *widgets = (datasource_t *)user_data;
	WebToolDatasource * ext_tool = widgets->web_tool_datasource;
	GtkWidget *user_string_label;
	char *label = g_strdup_printf("%s:", ext_tool->input_label);
	user_string_label = gtk_label_new (label);
	widgets->user_string = gtk_entry_new();

	char *last_str = get_last_user_string(widgets);
	if (last_str) {
		gtk_entry_set_text(GTK_ENTRY(widgets->user_string), last_str);
	}

	// 'ok' when press return in the entry
	g_signal_connect_swapped(widgets->user_string, "activate", G_CALLBACK(a_dialog_response_accept), dialog);

	/* Packing all widgets */
	GtkBox *box = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
	gtk_box_pack_start(box, user_string_label, false, false, 5);
	gtk_box_pack_start(box, widgets->user_string, false, false, 5);
	gtk_widget_show_all(dialog);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	// NB presently the focus is overridden later on by the acquire.c code.
	gtk_widget_grab_focus(widgets->user_string);

	free(label);
}





static void datasource_get_process_options(void * user_data, ProcessOptions *po, DownloadFileOptions *options, const char *notused1, const char *notused2)
{
	datasource_t *data = (datasource_t*) user_data;

	WebToolDatasource * web_tool_datasource = (WebToolDatasource *) data->web_tool_datasource;

	if (web_tool_datasource->webtool_needs_user_string()) {
		web_tool_datasource->user_string = g_strdup(gtk_entry_get_text (GTK_ENTRY (data->user_string)));

		if (web_tool_datasource->user_string[0] != '\0') {
			set_last_user_string(data, web_tool_datasource->user_string);
		}
	}

	char *url = web_tool_datasource->get_url(data->window);
	fprintf(stderr, "DEBUG: %s: %s\n", __PRETTY_FUNCTION__, url);

	po->url = g_strdup(url);

	// Only use first section of the file_type string
	// One can't use values like 'kml -x transform,rte=wpt' in order to do fancy things
	//  since it won't be in the right order for the overall GPSBabel command
	// So prevent any potentially dangerous behaviour
	char **parts = NULL;
	if (web_tool_datasource->file_type) {
		parts = g_strsplit(web_tool_datasource->file_type, " ", 0);
	}

	if (parts) {
		po->input_file_type = g_strdup(parts[0]);
	} else {
		po->input_file_type = NULL;
	}
	g_strfreev(parts);

	options = NULL;
	po->babel_filters = web_tool_datasource->babel_filter_args;
}





static void cleanup(void * data)
{
	free(data);
}





void WebToolDatasource::open(Window * window)
{
	bool search = this->webtool_needs_user_string();

	// Use VikDataSourceInterface to give thready goodness controls of downloading stuff (i.e. can cancel the request)

	// Can now create a 'VikDataSourceInterface' on the fly...
	VikDataSourceInterface *vik_datasource_interface = (VikDataSourceInterface *) malloc(sizeof(VikDataSourceInterface));

	// An 'easy' way of assigning values
	VikDataSourceInterface data = {
		this->get_label(),
		this->get_label(),
		VIK_DATASOURCE_ADDTOLAYER,
		VIK_DATASOURCE_INPUTTYPE_NONE,
		false, // Maintain current view - rather than setting it to the acquired points
		true,
		true,
		(VikDataSourceInitFunc)               datasource_init,
		(VikDataSourceCheckExistenceFunc)     NULL,
		(VikDataSourceAddSetupWidgetsFunc)    (search ? datasource_add_setup_widgets : NULL),
		(VikDataSourceGetProcessOptionsFunc)  datasource_get_process_options,
		(VikDataSourceProcessFunc)            a_babel_convert_from,
		(VikDataSourceProgressFunc)           NULL,
		(VikDataSourceAddProgressWidgetsFunc) NULL,
		(VikDataSourceCleanupFunc)            cleanup,
		(VikDataSourceOffFunc)                NULL,
		NULL,
		0,
		NULL,
		NULL,
		0
	};
	memcpy(vik_datasource_interface, &data, sizeof(VikDataSourceInterface));

	a_acquire(window, window->get_layers_panel(), window->get_viewport(), data.mode, vik_datasource_interface, this, cleanup);
}





WebToolDatasource::WebToolDatasource()
{
	fprintf(stderr, "%s:%d\n", __PRETTY_FUNCTION__, __LINE__);

	this->url_format_code = strdup("LRBT");
	this->file_type = NULL;
	this->babel_filter_args = NULL;
	this->input_label = strdup(_("Search Term"));
	this->user_string = NULL;
}





WebToolDatasource::WebToolDatasource(const char * new_label,
				     const char * new_url_format,
				     const char * new_url_format_code,
				     const char * new_file_type,
				     const char * new_babel_filter_args,
				     const char * new_input_label)
{
	fprintf(stderr, "%s:%d, label = \n", __PRETTY_FUNCTION__, __LINE__, new_label);

	if (new_label) {
		this->label = strdup(new_label);
	}
	if (new_url_format) {
		this->url_format = strdup(new_url_format);
	}
	if (new_url_format_code) {
		this->url_format_code = strdup(new_url_format_code);
	}
	if (new_file_type) {
		this->file_type = strdup(new_file_type);
	}
	if (new_babel_filter_args) {
		this->babel_filter_args = strdup(new_babel_filter_args);
	}
	if (new_input_label) {
		this->input_label = strdup(new_input_label);
	}
}





WebToolDatasource::~WebToolDatasource()
{
	fprintf(stderr, "%s:%d, label = %s\n", __PRETTY_FUNCTION__, __LINE__, this->label);

	if (this->url_format_code) {
		free(this->url_format_code);
		this->url_format_code = NULL;
	}

	if (this->file_type) {
		free(this->file_type);
		this->file_type = NULL;
	}

	if (this->babel_filter_args) {
		free(this->babel_filter_args);
		this->babel_filter_args = NULL;
	}

	if (this->input_label) {
		free(this->input_label);
		this->input_label = NULL;
	}

	if (this->user_string) {
		free(this->user_string);
		this->user_string = NULL;
	}
}





/**
 * Calculate individual elements (similarly to the VikWebtool Bounds & Center) for *all* potential values
 * Then only values specified by the URL format are used in parameterizing the URL
 */
char * WebToolDatasource::get_url(Window * window)
{
	Viewport * viewport = window->get_viewport();

	// Center values
	const VikCoord *coord = viewport->get_center();
	struct LatLon ll;
	vik_coord_to_latlon(coord, &ll);

	char scenterlat[G_ASCII_DTOSTR_BUF_SIZE];
	char scenterlon[G_ASCII_DTOSTR_BUF_SIZE];
	g_ascii_dtostr(scenterlat, G_ASCII_DTOSTR_BUF_SIZE, ll.lat);
	g_ascii_dtostr(scenterlon, G_ASCII_DTOSTR_BUF_SIZE, ll.lon);

	uint8_t zoom = 17; // A zoomed in default
	// zoom - ideally x & y factors need to be the same otherwise use the default
	if (viewport->get_xmpp() == viewport->get_ympp()) {
		zoom = map_utils_mpp_to_zoom_level(viewport->get_zoom());
	}

	char szoom[G_ASCII_DTOSTR_BUF_SIZE];
	snprintf(szoom, G_ASCII_DTOSTR_BUF_SIZE, "%d", zoom);

	int len = 0;
	if (this->url_format_code) {
		len = strlen(this->url_format_code);
	}

	if (len > MAX_NUMBER_CODES) {
		len = MAX_NUMBER_CODES;
	}

	char* values[MAX_NUMBER_CODES];
	for (int i = 0; i < MAX_NUMBER_CODES; i++) {
		values[i] = '\0';
	}

	LatLonBBoxStrings bbox_strings;
	viewport->get_bbox_strings(&bbox_strings);

	for (int i = 0; i < len; i++) {
		switch (g_ascii_toupper (this->url_format_code[i])) {
		case 'L': values[i] = g_strdup(bbox_strings.sminlon); break;
		case 'R': values[i] = g_strdup(bbox_strings.smaxlon); break;
		case 'B': values[i] = g_strdup(bbox_strings.sminlat); break;
		case 'T': values[i] = g_strdup(bbox_strings.smaxlat); break;
		case 'A': values[i] = g_strdup(scenterlat); break;
		case 'O': values[i] = g_strdup(scenterlon); break;
		case 'Z': values[i] = g_strdup(szoom); break;
		case 'S': values[i] = g_strdup(this->user_string); break;
		default: break;
		}
	}

	char * url = g_strdup_printf(this->url_format, values[0], values[1], values[2], values[3], values[4], values[5], values[6]);

	for (int i = 0; i < MAX_NUMBER_CODES; i++) {
		if (values[i] != '\0') {
			free(values[i]);
		}
	}

	return url;
}





char * WebToolDatasource::get_url_at_position(Window * window, VikCoord * vc)
{
	return this->get_url(window);
}





// NB Only works for ascii strings
char* strcasestr2(const char *dst, const char *src)
{
	if (!dst || !src) {
		return NULL;
	}

	if (src[0] == '\0') {
		return (char*)dst;
	}

	int len = strlen(src) - 1;
	char sc = tolower(src[0]);
	for (char dc = *dst; (dc = *dst); dst++) {
		dc = tolower(dc);
		if (sc == dc && (len == 0 || !strncasecmp(dst+1, src+1, len))) {
			return (char *) dst;
		}
	}

	return NULL;
}





/**
 * Returns true if the URL format contains 'S' -- that is, a search term entry
 * box needs to be displayed
 */
bool WebToolDatasource::webtool_needs_user_string()
{
	// For some reason (my) Windows build gets built with -D_GNU_SOURCE
#if (_GNU_SOURCE && !WINDOWS)
	return (strcasestr(this->url_format_code, "S") != NULL);
#else
	return (strcasestr2(this->url_format_code, "S") != NULL);
#endif
}
