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

#include "datasource_file.h"
#include "babel.h"
#include "gpx.h"
#include "babel_ui.h"
#include "acquire.h"
#include "util.h"




using namespace SlavGPS;



extern std::map<int, BabelFileType *> a_babel_file_types;

/* The last used directory. */
static char * last_folder_uri = NULL;

/* The last used file filter. */
/* Nb: we use a complex strategy for this because the UI is rebuild each
   time, so it is not possible to reuse directly the GtkFileFilter as they are
   differents. */
static BabelFileType * last_file_type = NULL;

/* The last file format selected. */
static int last_type = 0;




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




DataSourceFileDialog::DataSourceFileDialog(QString const & title, QWidget * parent_) : QDialog(parent_)
{
	this->setWindowTitle(title);
}




DataSourceFileDialog::~DataSourceFileDialog()
{
	delete this->file_types_combo;
}




void DataSourceFileDialog::build_ui(void)
{
	qDebug() << "II: Data Source File: building dialog UI";

	this->vbox = new QVBoxLayout;
	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox);



	QLabel * label = new QLabel("File");
	this->vbox->addWidget(label);


	QString a_title = "File to Import";
	this->file_entry = new SGFileEntry(QFileDialog::Option(0), QFileDialog::ExistingFile, a_title, NULL);
	/*
	if (filename) {
		QString filename(filename);
		this->file_entry->set_filename(filename);
	}

	if (last_folder_uri) {
		gtk_file_chooser_set_current_folder_uri(GTK_FILE_CHOOSER(widgets->file), last_folder_uri);
	}

	*/
	/* Add filters. */


	QStringList filter;
	filter << _("All files (*)");

	this->file_entry->file_selector->setNameFilters(filter);

	for (auto iter = a_babel_file_types.begin(); iter != a_babel_file_types.end(); iter++) {

		QString a = QString((iter->second)->label) + "(" + QString((iter->second)->ext) + ")";
		//qDebug() << "II: Data Source File: adding file filter " << a;
		filter << a;

		char const * ext = (iter->second)->ext;
		if (ext == NULL || ext[0] == '\0') {
			/* No file extension => no filter. */
			continue;
		}
		//char * pattern = g_strdup_printf("*.%s", ext);

#ifdef K
		GtkFileFilter * filter = gtk_file_filter_new();
		gtk_file_filter_add_pattern(filter, pattern);
		if (strstr(label, pattern+1)) {
			gtk_file_filter_set_name(filter, label);
		} else {
			/* Ensure displayed label contains file pattern. */
			/* NB: we skip the '*' in the pattern. */
			char * name = g_strdup_printf("%s (%s)", label, pattern+1);
			gtk_file_filter_set_name(filter, name);
			free(name);
#endif
	}

#ifdef K
	g_object_set_data(G_OBJECT(filter), "Babel", file_type);
	gtk_file_chooser_add_filter(this->file_entry, filter);
	if (last_file_type == file_type) {
		/* Previous selection used this filter. */
		gtk_file_chooser_set_filter(this->file_entry, filter);
	}

	free(pattern);
#endif
	this->vbox->addWidget(this->file_entry);



	label = new QLabel("File type:");
	this->vbox->addWidget(label);



#ifdef K
	GtkFileFilter *all_filter = gtk_file_filter_new();
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(data_source_file_dialog->file_entry), all_filter);
	if (last_file_type == NULL) {
		/* No previously selected filter or 'All files' selected. */
		gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(data_source_file_dialog->file_entry), all_filter);
	}
#endif
	/* The file format selector. */
	/* Propose any readable file. */
	BabelMode mode = { 1, 0, 1, 0, 1, 0 };
	this->file_types_combo = a_babel_ui_file_type_selector_new(mode);
	this->vbox->addWidget(this->file_types_combo);

	QObject::connect(this->file_types_combo, SIGNAL (currentIndexChanged(int)), this, SLOT (file_type_changed_cb(int)));
	this->file_types_combo->setCurrentIndex(last_type);


	this->button_box = new QDialogButtonBox();
	this->button_box->addButton(QDialogButtonBox::Ok);
	this->button_box->addButton(QDialogButtonBox::Cancel);
	connect(this->button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(this->button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
	this->vbox->addWidget(this->button_box);


	/* Manually call the callback to set state of OK button. */
	this->file_type_changed_cb(last_type);


	/* Blinky cursor in input field will be visible and will bring
	   user's eyes to widget that has a focus. */
	this->file_entry->setFocus();
}




void DataSourceFileDialog::file_type_changed_cb(int index)
{
	qDebug() << "SLOT: Datasource File: current index changed to" << index;
	QPushButton * button = this->button_box->button(QDialogButtonBox::Ok);
	button->setEnabled(index != 0); /* Index is zero. User Data is -1. */
}




DataSourceFileDialog * data_source_file_dialog = NULL;




void DataSourceFileDialog::add_file_type_filter(BabelFileType * file_type)
{
	char const * label = file_type->label;
	char const * ext = file_type->ext;
	if (ext == NULL || ext[0] == '\0') {
		/* No file extension => no filter. */
		return;
	}
	char * pattern = g_strdup_printf("*.%s", ext);

#ifdef K
	GtkFileFilter * filter = gtk_file_filter_new();
	gtk_file_filter_add_pattern(filter, pattern);
	if (strstr(label, pattern+1)) {
		gtk_file_filter_set_name(filter, label);
	} else {
		/* Ensure displayed label contains file pattern. */
		/* NB: we skip the '*' in the pattern. */
		char * name = g_strdup_printf("%s (%s)", label, pattern+1);
		gtk_file_filter_set_name(filter, name);
		free(name);
	}

	g_object_set_data(G_OBJECT(filter), "Babel", file_type);
	gtk_file_chooser_add_filter(this->file_entry, filter);
	if (last_file_type == file_type) {
		/* Previous selection used this filter. */
		gtk_file_chooser_set_filter(this->file_entry, filter);
	}

	free(pattern);
#endif
}




/* See VikDataSourceInterface. */
static int datasource_file_internal_dialog(QWidget * parent)
{
	if (!data_source_file_dialog) {
		data_source_file_dialog = new DataSourceFileDialog(QObject::tr(vik_datasource_file_interface.window_title), parent);
		data_source_file_dialog->build_ui();
	}

	data_source_file_dialog->file_entry->setFocus();

	return data_source_file_dialog->exec();
}




/* See VikDataSourceInterface/ */
static ProcessOptions * datasource_file_get_process_options(void * unused, void * not_used, char const * not_used2, char const * not_used3)
{
	ProcessOptions * po = new ProcessOptions();



#ifdef K
	/* Memorize the directory for later use. */
	free(last_folder_uri);
	last_folder_uri = gtk_file_chooser_get_current_folder_uri(GTK_FILE_CHOOSER(data_source_file_dialog->file_entry));

	/* Memorize the file filter for later use. */
	GtkFileFilter *filter = gtk_file_chooser_get_filter(GTK_FILE_CHOOSER(data_source_file_dialog->file_entry));
	last_file_type = (BabelFileType *) g_object_get_data(G_OBJECT(filter), "Babel");
#endif
	/* Retrieve and memorize file format selected. */
	last_type = data_source_file_dialog->file_types_combo->currentIndex();

	const char * selected = (a_babel_ui_file_type_selector_get(data_source_file_dialog->file_types_combo))->name;

	/* Generate the process options. */
	po->babelargs = g_strdup_printf("-i %s", selected);
	po->filename = strdup(data_source_file_dialog->file_entry->get_filename().toUtf8().data());

	qDebug() << "II: Datasource File: using Babel args" << po->babelargs << "and file" << po->filename;

	return po;
}
