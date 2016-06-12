/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2011, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
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
#include <string.h>
#include <stdlib.h>

#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "viking.h"
#include "babel.h"
#include "gpx.h"
#include "acquire.h"



using namespace SlavGPS;




/**
 * See http://wiki.openstreetmap.org/wiki/API_v0.6#GPS_Traces
 */
#define DOWNLOAD_URL_FMT "api.openstreetmap.org/api/0.6/trackpoints?bbox=%s,%s,%s,%s&page=%d"

typedef struct {
	GtkWidget * page_number;
	Viewport * viewport;
} datasource_osm_widgets_t;

static double last_page_number = 0;

static void * datasource_osm_init(acq_vik_t * avt);
static void datasource_osm_add_setup_widgets(GtkWidget * dialog, Viewport * viewport, void * user_data);
static void datasource_osm_get_process_options(datasource_osm_widgets_t * widgets, ProcessOptions * po, DownloadFileOptions * options, char const * notused1, char const * notused2);
static void datasource_osm_cleanup(void * data);

VikDataSourceInterface vik_datasource_osm_interface = {
	N_("OSM traces"),
	N_("OSM traces"),
	VIK_DATASOURCE_AUTO_LAYER_MANAGEMENT,
	VIK_DATASOURCE_INPUTTYPE_NONE,
	true,
	true,
	true,
	(VikDataSourceInitFunc)		        datasource_osm_init,
	(VikDataSourceCheckExistenceFunc)	NULL,
	(VikDataSourceAddSetupWidgetsFunc)	datasource_osm_add_setup_widgets,
	(VikDataSourceGetProcessOptionsFunc)    datasource_osm_get_process_options,
	(VikDataSourceProcessFunc)              a_babel_convert_from,
	(VikDataSourceProgressFunc)		NULL,
	(VikDataSourceAddProgressWidgetsFunc)	NULL,
	(VikDataSourceCleanupFunc)		datasource_osm_cleanup,
	(VikDataSourceOffFunc)                  NULL,

	NULL,
	0,
	NULL,
	NULL,
	0
};

static void * datasource_osm_init(acq_vik_t * avt)
{
	datasource_osm_widgets_t *widgets = (datasource_osm_widgets_t *) malloc(sizeof (datasource_osm_widgets_t));
	/* Keep reference to viewport */
	widgets->viewport = avt->viewport;
	return widgets;
}

static void datasource_osm_add_setup_widgets(GtkWidget * dialog, Viewport * viewport, void * user_data)
{
	datasource_osm_widgets_t *widgets = (datasource_osm_widgets_t *)user_data;
	GtkWidget *page_number_label;
	page_number_label = gtk_label_new (_("Page number:"));
	widgets->page_number = gtk_spin_button_new_with_range(0, 100, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(widgets->page_number), last_page_number);

	/* Packing all widgets */
	GtkBox * box = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
	gtk_box_pack_start (box, page_number_label, false, false, 5);
	gtk_box_pack_start (box, widgets->page_number, false, false, 5);
	gtk_widget_show_all(dialog);
}

static void datasource_osm_get_process_options(datasource_osm_widgets_t * widgets, ProcessOptions * po, DownloadFileOptions * options, char const * notused1, char const * notused2)
{
	int page = 0;
	double min_lat, max_lat, min_lon, max_lon;
	char sminlon[G_ASCII_DTOSTR_BUF_SIZE];
	char smaxlon[G_ASCII_DTOSTR_BUF_SIZE];
	char sminlat[G_ASCII_DTOSTR_BUF_SIZE];
	char smaxlat[G_ASCII_DTOSTR_BUF_SIZE];

	/* get Viewport bounding box */
	widgets->viewport->get_min_max_lat_lon(&min_lat, &max_lat, &min_lon, &max_lon);

	/* Convert as LANG=C double representation */
	g_ascii_dtostr (sminlon, G_ASCII_DTOSTR_BUF_SIZE, min_lon);
	g_ascii_dtostr (smaxlon, G_ASCII_DTOSTR_BUF_SIZE, max_lon);
	g_ascii_dtostr (sminlat, G_ASCII_DTOSTR_BUF_SIZE, min_lat);
	g_ascii_dtostr (smaxlat, G_ASCII_DTOSTR_BUF_SIZE, max_lat);

	/* Retrieve the specified page number */
	last_page_number = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widgets->page_number));
	page = last_page_number;

	// NB Download is of GPX type
	po->url = g_strdup_printf(DOWNLOAD_URL_FMT, sminlon, sminlat, smaxlon, smaxlat, page);
	options = NULL; // i.e. use the default download settings
}

static void datasource_osm_cleanup(void * data)
{
	free(data);
}
