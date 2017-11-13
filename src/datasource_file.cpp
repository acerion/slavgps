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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vector>
#include <cstring>
#include <cstdlib>

#include <glib/gprintf.h>

#include <QDebug>

#include "datasource_file.h"
#include "acquire.h"
#include "babel.h"
#include "gpx.h"
#include "babel_dialog.h"
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




static DataSourceDialog * datasource_create_setup_dialog(Viewport * viewport, void * user_data);




DataSourceInterface datasource_file_interface = {
	N_("Import file with GPSBabel"),
	N_("Imported file"),
	DataSourceMode::AUTO_LAYER_MANAGEMENT,
	DatasourceInputtype::NONE,
	true,
	true,  /* true = keep dialog open after success. */
	true,  /* true = run as thread. */

	(DataSourceInitFunc)	              NULL,
	(DataSourceCheckExistenceFunc)        NULL,
	(DataSourceCreateSetupDialogFunc)     datasource_create_setup_dialog,
	(DataSourceGetProcessOptionsFunc)     NULL,
	(DataSourceProcessFunc)               a_babel_convert_from,
	(DataSourceProgressFunc)              NULL,
	(DataSourceCreateProgressDialogFunc)  NULL,
	(DataSourceCleanupFunc)               NULL,
	(DataSourceTurnOffFunc)               NULL,

	NULL,
	0,
	NULL,
	NULL,
	0
};




static DataSourceDialog * datasource_create_setup_dialog(Viewport * viewport, void * user_data)
{
	return new DataSourceFileDialog("");
}




DataSourceFileDialog::DataSourceFileDialog(const QString & title) : BabelDialog(title)
{
	this->build_ui();
	if (last_directory_url.isValid()) {
		this->file_entry->file_selector->setDirectoryUrl(last_directory_url);
	}

	this->file_entry->setFocus();

	connect(this->button_box, &QDialogButtonBox::accepted, this, &DataSourceFileDialog::accept_cb);
}




DataSourceFileDialog::~DataSourceFileDialog()
{
}




ProcessOptions * DataSourceFileDialog::get_process_options(DownloadOptions & dl_options)
{
	ProcessOptions * po = new ProcessOptions();

	/* Memorize the directory for later use. */
	last_directory_url = this->file_entry->file_selector->directoryUrl();

#ifdef K
	/* Memorize the file filter for later use. */
	GtkFileFilter *filter = gtk_file_chooser_get_filter(GTK_FILE_CHOOSER(this->file_entry));
	last_file_type = (BabelFileType *) g_object_get_data(G_OBJECT(filter), "Babel");
#endif


	const QString selected = this->get_file_type_selection()->name;

	/* Generate the process options. */
	po->babel_args = QString("-i %1").arg(selected);
	po->input_file_name = this->file_entry->get_filename();

	qDebug() << "II: Datasource File: using Babel args" << po->babel_args << "and input file" << po->input_file_name;

	return po;
}




void DataSourceFileDialog::accept_cb(void)
{
	const BabelFileType * file_type = this->get_file_type_selection();

	qDebug() << "II: Datasource File: dialog result: accepted";
	qDebug() << "II: Datasource File: selected format type name: '" << file_type->name  << "'";
	qDebug() << "II: Datasource File: selected format type label: '" << file_type->label << "'";
	qDebug() << "II: Datasource File: selected file path: '" << this->file_entry->get_filename() << "'";
}
