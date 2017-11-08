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

#include <vector>
#include <cstring>
#include <cstdlib>

#include <glib/gprintf.h>

#include <QDebug>

#include "babel.h"
#include "gpx.h"
#include "babel_dialog.h"
#include "acquire.h"
#include "util.h"
#include "widget_file_entry.h"




using namespace SlavGPS;



extern std::map<int, BabelFileType *> a_babel_file_types;

/* The last used directory. */
static QUrl last_directory_url;


/* The last used file filter. */
/* Nb: we use a complex strategy for this because the UI is rebuild each
   time, so it is not possible to reuse directly the GtkFileFilter as they are
   differents. */
static BabelFileType * last_file_type = NULL;





static int datasource_file_internal_dialog(QWidget * parent);
static ProcessOptions * datasource_file_get_process_options(void * widgets, void * not_used, char const * not_used2, char const * not_used3);




VikDataSourceInterface vik_datasource_file_interface = {
	N_("Import file with GPSBabel"),
	N_("Imported file"),
	DatasourceMode::AUTO_LAYER_MANAGEMENT,
	DatasourceInputtype::NONE,
	true,
	true,
	true,

	(DataSourceInternalDialog)              datasource_file_internal_dialog,
	(VikDataSourceInitFunc)	                NULL,
	(VikDataSourceCheckExistenceFunc)       NULL,
	(VikDataSourceAddSetupWidgetsFunc)      NULL,
	(VikDataSourceGetProcessOptionsFunc)    datasource_file_get_process_options,
	(VikDataSourceProcessFunc)              a_babel_convert_from,
	(VikDataSourceProgressFunc)             NULL,
	(VikDataSourceAddProgressWidgetsFunc)   NULL,
	(VikDataSourceCleanupFunc)              NULL,
	(VikDataSourceOffFunc)                  NULL,

	NULL,
	0,
	NULL,
	NULL,
	0
};




BabelDialog * data_source_file_dialog = NULL;




/* See VikDataSourceInterface. */
static int datasource_file_internal_dialog(QWidget * parent)
{
	if (!data_source_file_dialog) {
		data_source_file_dialog = new BabelDialog(QObject::tr(vik_datasource_file_interface.window_title), parent);
		data_source_file_dialog->build_ui();
	}

	if (last_directory_url.isValid()) {
		data_source_file_dialog->file_entry->file_selector->setDirectoryUrl(last_directory_url);
	}

	data_source_file_dialog->file_entry->setFocus();

	int rv = data_source_file_dialog->exec();

	if (rv == QDialog::Accepted) {
		const BabelFileType * file_type = data_source_file_dialog->get_file_type_selection();

		qDebug() << "II: Datasource File: dialog result: accepted";
		qDebug() << "II: Datasource File: selected format type name: '" << file_type->name  << "'";
		qDebug() << "II: Datasource File: selected format type label: '" << file_type->label << "'";
		qDebug() << "II: Datasource File: selected file path: '" << data_source_file_dialog->file_entry->get_filename() << "'";

	} else if (rv == QDialog::Rejected) {
		qDebug() << "II: Datasource File: dialog result: rejected";
	} else {
		qDebug() << "EE: Datasource File: dialog result: unknown:" << rv;
	}

	return rv;
}




/* See VikDataSourceInterface/ */
static ProcessOptions * datasource_file_get_process_options(void * unused, void * not_used, char const * not_used2, char const * not_used3)
{
	ProcessOptions * po = new ProcessOptions();

	/* Memorize the directory for later use. */
	last_directory_url = data_source_file_dialog->file_entry->file_selector->directoryUrl();

#ifdef K
	/* Memorize the file filter for later use. */
	GtkFileFilter *filter = gtk_file_chooser_get_filter(GTK_FILE_CHOOSER(data_source_file_dialog->file_entry));
	last_file_type = (BabelFileType *) g_object_get_data(G_OBJECT(filter), "Babel");
#endif


	const QString selected = (data_source_file_dialog->get_file_type_selection())->name;

	/* Generate the process options. */
	po->babel_args = QString("-i %1").arg(selected);
	po->input_file_name = data_source_file_dialog->file_entry->get_filename();

	qDebug() << "II: Datasource File: using Babel args" << po->babel_args << "and input file" << po->input_file_name;

	return po;
}
