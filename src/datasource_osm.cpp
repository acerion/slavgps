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
#include <cstring>
#include <cstdlib>

#include <glib/gprintf.h>

#include "viewport_internal.h"
#include "babel.h"
#include "gpx.h"
#include "acquire.h"
#include "util.h"




using namespace SlavGPS;




/**
 * See http://wiki.openstreetmap.org/wiki/API_v0.6#GPS_Traces
 */
#define DOWNLOAD_URL_FMT "api.openstreetmap.org/api/0.6/trackpoints?bbox=%s,%s,%s,%s&page=%d"




typedef struct {
	QSpinBox * page_number = NULL;
	Viewport * viewport = NULL;
} datasource_osm_widgets_t;




static double last_page_number = 0;

static void * datasource_osm_init(acq_vik_t * avt);
static void datasource_osm_add_setup_widgets(GtkWidget * dialog, Viewport * viewport, void * user_data);
static ProcessOptions * datasource_osm_get_process_options(datasource_osm_widgets_t * widgets, DownloadFileOptions * options, char const * notused1, char const * notused2);
static void datasource_osm_cleanup(void * data);




VikDataSourceInterface vik_datasource_osm_interface = {
	N_("OSM traces"),
	N_("OSM traces"),
	DatasourceMode::AUTO_LAYER_MANAGEMENT,
	DatasourceInputtype::NONE,
	true,
	true,
	true,

	(DataSourceInternalDialog)              NULL,
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
	/* Keep reference to viewport. */
	widgets->viewport = avt->viewport;
	return widgets;
}




static void datasource_osm_add_setup_widgets(GtkWidget * dialog, Viewport * viewport, void * user_data)
{
	datasource_osm_widgets_t * widgets = (datasource_osm_widgets_t *) user_data;
#ifdef K
	QLabel * page_number_label = new QLabel(QObject::tr("Page number:"));
	widgets->page_number = new QSpinBox(0, 100, 1);
	widgets->page_number->setValue(last_page_number);

	/* Packing all widgets */
	GtkBox * box = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
	box->addWidget(page_number_label);
	box->addWidget(widgets->page_number);
	gtk_widget_show_all(dialog);
#endif
}




static ProcessOptions * datasource_osm_get_process_options(datasource_osm_widgets_t * widgets, DownloadFileOptions * options, char const * notused1, char const * notused2)
{
	ProcessOptions * po = new ProcessOptions();

	int page = 0;

	LatLonBBoxStrings bbox_strings;
	widgets->viewport->get_bbox_strings(&bbox_strings);

	/* Retrieve the specified page number. */
	last_page_number = widgets->page_number->value();
	page = last_page_number;

	/* NB Download is of GPX type. */
	po->url = g_strdup_printf(DOWNLOAD_URL_FMT, bbox_strings.sminlon, bbox_strings.sminlat, bbox_strings.smaxlon, bbox_strings.smaxlat, page);
	options = NULL; /* i.e. use the default download settings. */

	return po;
}




static void datasource_osm_cleanup(void * data)
{
	free(data);
}
