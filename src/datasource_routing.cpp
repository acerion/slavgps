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

#include <string.h>
#include <stdlib.h>

#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "viking.h"
#include "babel.h"
#include "gpx.h"
#include "acquire.h"
#include "vikrouting.h"

typedef struct {
	GtkWidget * engines_combo;
	GtkWidget * from_entry, * to_entry;
} datasource_routing_widgets_t;

/* Memory of previous selection */
static int last_engine = 0;
static char * last_from_str = NULL;
static char * last_to_str = NULL;

static void * datasource_routing_init(acq_vik_t * avt);
static void datasource_routing_add_setup_widgets(GtkWidget * dialog, Viewport * viewport, void * user_data);
static void datasource_routing_get_process_options(datasource_routing_widgets_t * widgets, ProcessOptions * po, DownloadFileOptions * options, char const * not_used2, char const * not_used3);
static void datasource_routing_cleanup(void * data);

VikDataSourceInterface vik_datasource_routing_interface = {
	N_("Directions"),
	N_("Directions"),
	VIK_DATASOURCE_AUTO_LAYER_MANAGEMENT,
	VIK_DATASOURCE_INPUTTYPE_NONE,
	true,
	true,
	true,
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
	GtkWidget *engine_label, *from_label, *to_label;

	/* Engine selector */
	engine_label = gtk_label_new(_("Engine:"));
	widgets->engines_combo = vik_routing_ui_selector_new((Predicate)vik_routing_engine_supports_direction, NULL);
	gtk_combo_box_set_active(GTK_COMBO_BOX (widgets->engines_combo), last_engine);

	/* From and To entries */
	from_label = gtk_label_new(_("From:"));
	widgets->from_entry = gtk_entry_new();
	to_label = gtk_label_new(_("To:"));
	widgets->to_entry = gtk_entry_new();
	if (last_from_str) {
		gtk_entry_set_text(GTK_ENTRY(widgets->from_entry), last_from_str);
	}

	if (last_to_str) {
		gtk_entry_set_text(GTK_ENTRY(widgets->to_entry), last_to_str);
	}

	/* Packing all these widgets */
	GtkBox *box = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
	gtk_box_pack_start(box, engine_label, false, false, 5);
	gtk_box_pack_start(box, widgets->engines_combo, false, false, 5);
	gtk_box_pack_start(box, from_label, false, false, 5);
	gtk_box_pack_start(box, widgets->from_entry, false, false, 5);
	gtk_box_pack_start(box, to_label, false, false, 5);
	gtk_box_pack_start(box, widgets->to_entry, false, false, 5);
	gtk_widget_show_all(dialog);
}

static void datasource_routing_get_process_options(datasource_routing_widgets_t * widgets, ProcessOptions * po, DownloadFileOptions * options, char const * not_used2, char const * not_used3)
{
	const char *from, *to;

	/* Retrieve directions */
	from = gtk_entry_get_text(GTK_ENTRY(widgets->from_entry));
	to = gtk_entry_get_text(GTK_ENTRY(widgets->to_entry));

	/* Retrieve engine */
	last_engine = gtk_combo_box_get_active(GTK_COMBO_BOX(widgets->engines_combo));
	VikRoutingEngine *engine = vik_routing_ui_selector_get_nth(widgets->engines_combo, last_engine);
	if (!engine) return;

	po->url = vik_routing_engine_get_url_from_directions(engine, from, to);
	po->input_file_type = g_strdup(vik_routing_engine_get_format(engine));
	options = NULL; // i.e. use the default download settings

	/* Save last selection */
	free(last_from_str);
	free(last_to_str);

	last_from_str = g_strdup(from);
	last_to_str = g_strdup(to);
}

static void datasource_routing_cleanup(void * data)
{
	free(data);
}
