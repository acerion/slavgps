/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#include <QComboBox>
#include <QLineEdit>

#include <glib/gprintf.h>

#include "babel.h"
#include "gpx.h"
#include "acquire.h"
#include "routing.h"
#include "util.h"



using namespace SlavGPS;




class datasource_routing_widgets_t {
public:
	QComboBox * engines_combo;
	QLineEdit from_entry;
	QLineEdit to_entry;
};




/* Memory of previous selection */
static int last_engine = 0;
static QString last_from_str;
static QString last_to_str;

static void * datasource_routing_init(acq_vik_t * avt);
static void datasource_routing_add_setup_widgets(GtkWidget * dialog, Viewport * viewport, void * user_data);
static ProcessOptions * datasource_routing_get_process_options(datasource_routing_widgets_t * widgets, DownloadOptions * dl_options, char const * not_used2, char const * not_used3);
static void datasource_routing_cleanup(void * data);




VikDataSourceInterface vik_datasource_routing_interface = {
	N_("Directions"),
	N_("Directions"),
	DatasourceMode::AUTO_LAYER_MANAGEMENT,
	DatasourceInputtype::NONE,
	true,
	true,
	true,

	(DataSourceInternalDialog)              NULL,
	(VikDataSourceInitFunc)		datasource_routing_init,
	(VikDataSourceCheckExistenceFunc)	NULL,
	(VikDataSourceAddSetupWidgetsFunc)	datasource_routing_add_setup_widgets,
	(VikDataSourceGetProcessOptionsFunc)  datasource_routing_get_process_options,
	(VikDataSourceProcessFunc)            a_babel_convert_from,
	(VikDataSourceProgressFunc)		NULL,
	(VikDataSourceAddProgressWidgetsFunc)	NULL,
	(VikDataSourceCleanupFunc)		datasource_routing_cleanup,
	(VikDataSourceOffFunc)                NULL,

	NULL,
	0,
	NULL,
	NULL,
	0
};




static void * datasource_routing_init(acq_vik_t * avt)
{
	datasource_routing_widgets_t * widgets = (datasource_routing_widgets_t *) malloc(sizeof (datasource_routing_widgets_t));
	return widgets;
}




static void datasource_routing_add_setup_widgets(GtkWidget * dialog, Viewport * viewport, void * user_data)
{
	datasource_routing_widgets_t *widgets = (datasource_routing_widgets_t *)user_data;

	/* Engine selector. */
	QLabel * engine_label = new QLabel(QObject::tr("Engine:"));
#ifdef K
	widgets->engines_combo = routing_ui_selector_new((Predicate)vik_routing_engine_supports_direction, NULL);
	widgets->engines_combo->setCurrentIndex(last_engine);
#endif

	/* From and To entries. */
	QLabel * from_label = new QLabel(QObject::tr("From:"));
	QLabel * to_label = new QLabel(QObject::tr("To:"));

	if (!last_from_str.isEmpty()) {
		widgets->from_entry.setText(last_from_str);
	}

	if (!last_to_str.isEmpty()) {
		widgets->to_entry.setText(last_to_str);
	}
#ifdef K
	/* Packing all these widgets. */
	GtkBox *box = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
	box->addWidget(engine_label);
	box->addWidget(widgets->engines_combo);
	box->addWidget(from_label);
	box->addWidget(&widgets->from_entry);
	box->addWidget(to_label);
	box->addWidget(&widgets->to_entry);
	gtk_widget_show_all(dialog);
#endif
}




static ProcessOptions * datasource_routing_get_process_options(datasource_routing_widgets_t * widgets, DownloadOptions * dl_options, char const * not_used2, char const * not_used3)
{
	ProcessOptions * po = new ProcessOptions();

	/* Retrieve directions. */
	const QString from = widgets->from_entry.text();
	const QString to = widgets->to_entry.text();

	/* Retrieve engine. */
	last_engine = widgets->engines_combo->currentIndex();

	RoutingEngine * engine = routing_ui_selector_get_nth(widgets->engines_combo, last_engine);
	if (!engine) {
		return NULL; /* kamil FIXME: this needs to be handled in caller. */
	}

	po->url = engine->get_url_from_directions(from.toUtf8().constData(), to.toUtf8().constData());
	po->input_file_type = g_strdup(engine->get_format());
	dl_options = NULL; /* i.e. use the default download settings. */


	/* Save last selection. */
	last_from_str = from;
	last_to_str = to;

	return po;
}




static void datasource_routing_cleanup(void * data)
{
	free(data);
}
