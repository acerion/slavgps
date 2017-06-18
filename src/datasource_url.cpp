/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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
#include <glib/gi18n.h>

#include <vector>
#include <cstdlib>

#include <QComboBox>

#include "acquire.h"
#include "babel.h"
#include "settings.h"




using namespace SlavGPS;




/*
  Initially was just going to be a URL and always in GPX format.
  But might as well specify the file type as per datasource_file.c.
  However in this version we'll cope with no GPSBabel available and in this case just try GPX.
*/




typedef struct {
	QLineEdit url;
	QComboBox * type = NULL;
} datasource_url_widgets_t;




extern std::map<int, BabelFileType *> a_babel_file_types;

/* The last file format selected. */
static int last_type_id = -1;




static void * datasource_url_init(acq_vik_t * avt);
static void datasource_url_add_setup_widgets(GtkWidget * dialog, Viewport * viewport, void * user_data);
static ProcessOptions * datasource_url_get_process_options(datasource_url_widgets_t * widgets, DownloadFileOptions * download_options, char const * not_used2, char const * not_used3);
static void datasource_url_cleanup(void * data);




VikDataSourceInterface vik_datasource_url_interface = {
	N_("Acquire from URL"),
	N_("URL"),
	DatasourceMode::AUTO_LAYER_MANAGEMENT,
	DatasourceInputtype::NONE,
	true,
	true,
	true,

	(DataSourceInternalDialog)            NULL,
	(VikDataSourceInitFunc)               datasource_url_init,
	(VikDataSourceCheckExistenceFunc)     NULL,
	(VikDataSourceAddSetupWidgetsFunc)    datasource_url_add_setup_widgets,
	(VikDataSourceGetProcessOptionsFunc)  datasource_url_get_process_options,
	(VikDataSourceProcessFunc)            a_babel_convert_from,
	(VikDataSourceProgressFunc)           NULL,
	(VikDataSourceAddProgressWidgetsFunc) NULL,
	(VikDataSourceCleanupFunc)            datasource_url_cleanup,
	(VikDataSourceOffFunc)                NULL,
	NULL,
	0,
	NULL,
	NULL,
	0
};




static void * datasource_url_init(acq_vik_t * avt)
{
	datasource_url_widgets_t * widgets = (datasource_url_widgets_t *) malloc(sizeof (datasource_url_widgets_t));
	return widgets;
}




static void fill_combo_box(void * data, void * user_data)
{
	char const * label = ((BabelFileType *) data)->label;
#ifdef K
	vik_combo_box_text_append (GTK_WIDGET(user_data), label);
#endif
}




static int find_entry = -1;
static int wanted_entry = -1;




static void find_type(BabelFileType * file_type, char const * type_name)
{
	find_entry++;
	if (!g_strcmp0(file_type->name, type_name)) {
		wanted_entry = find_entry;
	}
}




#define VIK_SETTINGS_URL_FILE_DL_TYPE "url_file_download_type"




static void datasource_url_add_setup_widgets(GtkWidget * dialog, Viewport * viewport, void * user_data)
{
	datasource_url_widgets_t * widgets = (datasource_url_widgets_t *) user_data;
	QLabel * label = new QLabel(QObject::tr("URL:"));
#ifdef K

	QLabel * type_label = new QLabel(QObject::tr("File type:"));

	if (last_type_id < 0) {
		find_entry = -1;
		wanted_entry = -1;
		char *type = NULL;
		if (a_settings_get_string(VIK_SETTINGS_URL_FILE_DL_TYPE, &type)) {
			/* Use setting. */
			if (type) {
				for (auto iter = a_babel_file_types.begin(); iter != a_babel_file_types.end(); iter++) {
					find_type(iter->second, type);
				}
			}
		} else {
			/* Default to GPX if possible. */
			for (auto iter = a_babel_file_types.begin(); iter != a_babel_file_types.end(); iter++) {
				find_type(iter->second, "gpx");
			}
		}
		/* If not found set it to the first entry, otherwise use the entry. */
		last_type_id = (wanted_entry < 0) ? 0 : wanted_entry;
	}

	if (a_babel_available()) {
		widgets->type = new QComboBox();
		for (auto iter = a_babel_file_types.begin(); iter != a_babel_file_types.end(); iter++) {
			fill_combo_box(iter->second, widgets->type);
		}
		widgets->type->setCurrentIndex(last_type_id);
	} else {
		/* Only GPX (not using GPSbabel). */
		widgets->type = new QLabel(QObject::tr("GPX"));
	}

	/* Packing all widgets. */
	GtkBox * box = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
	box->addWidget(label);
	box->addWidget(&widgets->url);
	box->addWidget(type_label);
	box->addWidget(widgets->type);

	gtk_widget_show_all(dialog);
#endif
}




static ProcessOptions * datasource_url_get_process_options(datasource_url_widgets_t * widgets, DownloadFileOptions * download_options, char const * not_used2, char const * not_used3)
{
	ProcessOptions * po = new ProcessOptions();
#ifdef K
	/* Retrieve the user entered value. */
	char const * value = widgets->url.text();

	if (GTK_IS_COMBO_BOX (widgets->type)) {
		last_type_id = widgets->type->currentIndex();
	}

	po->input_file_type = NULL; /* Default to gpx. */
	if (a_babel_file_types.size()) {
		po->input_file_type = g_strdup(a_babel_file_types.at(last_type_id)->name);
	}

	po->url = g_strdup(value);

	/* Support .zip + bzip2 files directly. */
	download_options->convert_file = a_try_decompress_file;
	download_options->follow_location = 5;

	return po;
#endif
}




static void datasource_url_cleanup(void * data)
{
	free(data);
}
