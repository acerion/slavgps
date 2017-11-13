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
 */




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cctype>
#include <cstring>
#include <cstdlib>
#include <vector>

#include <QDebug>
#include <QLineEdit>

#include <glib.h>

#include "viewport_internal.h"
#include "vikwebtool_datasource.h"
#include "globals.h"
#include "acquire.h"
#include "map_utils.h"
#include "dialog.h"
#include "util.h"




using namespace SlavGPS;




#define MAX_NUMBER_CODES 7




extern Tree * g_tree;




static GHashTable *last_user_strings = NULL;




typedef struct {
	WebToolDatasource * web_tool_datasource;
	Window * window;
	Viewport * viewport;
	QLineEdit user_string;
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
	QString label = source->web_tool_datasource->get_label();
	char *last_str = (char *) g_hash_table_lookup(last_user_strings, label.toUtf8().constData());
	return last_str;
}




static void set_last_user_string(const datasource_t *source, const char *s)
{
	QString label = source->web_tool_datasource->get_label();
	ensure_last_user_strings_hash();
	g_hash_table_insert(last_user_strings,
			    label.toUtf8().data(),
			    g_strdup(s));
}




static void * datasource_init(acq_vik_t *avt)
{
	datasource_t *data = (datasource_t *) malloc(sizeof(*data));
	data->web_tool_datasource = (WebToolDatasource *) avt->userdata;
	data->window = avt->window;
	data->viewport = avt->viewport;
	return data;
}




static DataSourceDialog * datasource_create_setup_dialog(Viewport * viewport, void * user_data)
{
	DataSourceDialog * setup_dialog = NULL;
	GtkWidget *dialog;

	datasource_t *widgets = (datasource_t *)user_data;
	WebToolDatasource * ext_tool = widgets->web_tool_datasource;
	char *label = g_strdup_printf("%s:", ext_tool->input_label);
	QLabel * user_string_label = new QLabel(label);
#ifdef K
	char *last_str = get_last_user_string(widgets);
	if (last_str) {
		widgets->user_string.setText(last_str);
	}

	/* 'ok' when press return in the entry. */
	QObject::connect(&widgets->user_string, SIGNAL("activate"), dialog, SLOT (accept));

	/* Packing all widgets. */
	GtkBox *box = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
	box->addWidget(user_string_label);
	box->addWidget(&widgets->user_string);
	gtk_widget_show_all(dialog);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	/* NB presently the focus is overridden later on by the acquire.c code. */
	gtk_widget_grab_focus(widgets->user_string);

	free(label);
#endif

	return setup_dialog;
}




static ProcessOptions * datasource_get_process_options(void * user_data, DownloadOptions * dl_options, const char * unused1, const char * unused2)
{
	ProcessOptions * po = new ProcessOptions();

	datasource_t *data = (datasource_t*) user_data;

	WebToolDatasource * web_tool_datasource = (WebToolDatasource *) data->web_tool_datasource;

#ifdef K
	if (web_tool_datasource->webtool_needs_user_string()) {
		web_tool_datasource->user_string = data->user_string.text();

		if (web_tool_datasource->user_string[0] != '\0') {
			set_last_user_string(data, web_tool_datasource->user_string);
		}
	}
#endif

	po->url = web_tool_datasource->get_url_at_current_position(data->viewport);
	qDebug() << "DD: Web Tool Datasource: url =" << po->url;

	/* Only use first section of the file_type string.
	   One can't use values like 'kml -x transform,rte=wpt' in order to do fancy things
	   since it won't be in the right order for the overall GPSBabel command.
	   So prevent any potentially dangerous behaviour. */
	char **parts = NULL;
	if (web_tool_datasource->file_type) {
		parts = g_strsplit(web_tool_datasource->file_type, " ", 0);
	}

	if (parts) {
		po->input_file_type = QString(parts[0]);
	} else {
		po->input_file_type = "";
	}
	g_strfreev(parts);


	dl_options = NULL;
	po->babel_filters = web_tool_datasource->babel_filter_args;

	return po;

}




static void cleanup(void * data)
{
	free(data);
}




void WebToolDatasource::run_at_current_position(Window * a_window)
{
	bool search = this->webtool_needs_user_string();

	/* Use DataSourceInterface to give thready goodness controls of downloading stuff (i.e. can cancel the request). */

	/* Can now create a 'DataSourceInterface' on the fly... */
	DataSourceInterface * datasource_interface = (DataSourceInterface *) malloc(sizeof(DataSourceInterface));

	/* An 'easy' way of assigning values. */
	DataSourceInterface data = {
		this->get_label(),
		this->get_label(),
		DataSourceMode::ADD_TO_LAYER,
		DatasourceInputtype::NONE,
		false, /* Maintain current view - rather than setting it to the acquired points. */
		true,  /* true = keep dialog open after success. */
		true,  /* true = run as thread. */

		(DataSourceInitFunc)                  datasource_init,
		(DataSourceCheckExistenceFunc)        NULL,
		(DataSourceCreateSetupDialogFunc)     (search ? datasource_create_setup_dialog : NULL),
		(DataSourceGetProcessOptionsFunc)     datasource_get_process_options,
		(DataSourceProcessFunc)               a_babel_convert_from,
		(DataSourceProgressFunc)              NULL,
		(DataSourceCreateProgressDialogFunc)  NULL,
		(DataSourceCleanupFunc)               cleanup,
		(DataSourceTurnOffFunc)               NULL,
		NULL,
		0,
		NULL,
		NULL,
		0
	};
	memcpy(datasource_interface, &data, sizeof(DataSourceInterface));

	Acquire::acquire_from_source(a_window, g_tree->tree_get_items_tree(), a_window->get_viewport(), data.mode, datasource_interface, this, cleanup);
}



#if 0
WebToolDatasource::WebToolDatasource()
{
	qDebug() << "II: Web Tool Datasource created";

	this->url_format_code = strdup("LRBT");
	this->input_label = strdup(_("Search Term"));
}
#endif



WebToolDatasource::WebToolDatasource(const QString & new_label,
				     const QString & new_url_format,
				     const QString & new_url_format_code,
				     const char * new_file_type,
				     const char * new_babel_filter_args,
				     const char * new_input_label) : WebTool(new_label)
{
	qDebug() << "II: Web Tool Datasource created with label" << new_label;

	this->label = new_label;
	this->q_url_format = new_url_format;
	this->url_format_code = new_url_format_code;

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
	qDebug() << "II: Web Tool Datasource: delete tool with label" << this->label;

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
 * Calculate individual elements (similarly to the VikWebtool Bounds & Center) for *all* potential values.
 * Then only values specified by the URL format are used in parameterizing the URL.
 */
QString WebToolDatasource::get_url_at_current_position(Viewport * a_viewport)
{
	/* Center values. */
	struct LatLon ll = a_viewport->get_center()->get_latlon();

	QString center_lat;
	QString center_lon;
	CoordUtils::to_strings(center_lat, center_lon, ll);

	uint8_t zoom_level = 17; /* A zoomed in default. */
	/* Zoom - ideally x & y factors need to be the same otherwise use the default. */
	if (a_viewport->get_xmpp() == a_viewport->get_ympp()) {
		zoom_level = map_utils_mpp_to_zoom_level(a_viewport->get_zoom());
	}

	QString zoom((int) zoom_level);

	int len = this->url_format_code.size();
	if (len == 0) {
		qDebug() << "EE: Web Tool Datasource: url format code is empty";
		return QString("");
	} else if (len > MAX_NUMBER_CODES) {
		qDebug() << "WW: Web Tool Datasource: url format code too long:" << len << MAX_NUMBER_CODES << this->url_format_code;
		len = MAX_NUMBER_CODES;
	} else {
		;
	}

	std::vector<QString> values;

	LatLonBBoxStrings bbox_strings;
	a_viewport->get_bbox_strings(bbox_strings);

	for (int i = 0; i < len; i++) {
		switch (this->url_format_code[i].toUpper().toLatin1()) {
		case 'L': values[i] = bbox_strings.min_lon; break;
		case 'R': values[i] = bbox_strings.max_lon; break;
		case 'B': values[i] = bbox_strings.min_lat; break;
		case 'T': values[i] = bbox_strings.max_lat; break;
		case 'A': values[i] = center_lat; break;
		case 'O': values[i] = center_lon; break;
		case 'Z': values[i] = zoom; break;
		case 'S': values[i] = this->user_string; break;
		default:
			qDebug() << "EE: Web Tool Datasource: invalid URL format code" << this->url_format_code[i];
			return QString("");
		}
	}

	QString url = QString(this->q_url_format)
		.arg(values[0])
		.arg(values[1])
		.arg(values[2])
		.arg(values[3])
		.arg(values[4])
		.arg(values[5])
		.arg(values[6]);

	qDebug() << "II: Web Tool Datasource: url at current position is" << url;

	return url;
}




QString WebToolDatasource::get_url_at_position(Viewport * a_viewport, const Coord * a_coord)
{
	return this->get_url_at_current_position(a_viewport);
}




/* NB Only works for ascii strings. */
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
 * box needs to be displayed.
 */
bool WebToolDatasource::webtool_needs_user_string()
{
	return this->url_format_code.contains("S", Qt::CaseInsensitive);
}
