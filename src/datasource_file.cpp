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
#include <glib/gi18n.h>

#include "datasource_file.h"
#include "babel.h"
#include "gpx.h"
#include "babel_ui.h"
#include "acquire.h"




using namespace SlavGPS;




typedef struct {
	GtkWidget * file;
	GtkWidget * type;
} datasource_file_widgets_t;




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




static void * datasource_file_init(acq_vik_t * avt);
static void datasource_file_add_setup_widgets(GtkWidget * dialog, Viewport * viewport, void * user_data);
static ProcessOptions * datasource_file_get_process_options(datasource_file_widgets_t * widgets, void * not_used, char const * not_used2, char const * not_used3);
static void datasource_file_cleanup(void * data);




VikDataSourceInterface vik_datasource_file_interface = {
	N_("Import file with GPSBabel"),
	N_("Imported file"),
	VIK_DATASOURCE_AUTO_LAYER_MANAGEMENT,
	VIK_DATASOURCE_INPUTTYPE_NONE,
	true,
	true,
	true,
	(VikDataSourceInitFunc)	                datasource_file_init,
	(VikDataSourceCheckExistenceFunc)       NULL,
	(VikDataSourceAddSetupWidgetsFunc)      datasource_file_add_setup_widgets,
	(VikDataSourceGetProcessOptionsFunc)    datasource_file_get_process_options,
	(VikDataSourceProcessFunc)              a_babel_convert_from,
	(VikDataSourceProgressFunc)             NULL,
	(VikDataSourceAddProgressWidgetsFunc)   NULL,
	(VikDataSourceCleanupFunc)              datasource_file_cleanup,
	(VikDataSourceOffFunc)                  NULL,

	NULL,
	0,
	NULL,
	NULL,
	0
};




DataSourceFileDialog::DataSourceFileDialog(QString const & title, QWidget * parent_widget) : QDialog(NULL)
{
	this->setWindowTitle(title);
	this->parent = parent_widget;
}




DataSourceFileDialog::~DataSourceFileDialog()
{
	delete this->file_types;
}




void DataSourceFileDialog::accept_cb(void) /* Slot. */
{
	this->accept();
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
		qDebug() << "II: -------- Data Source File: adding file filter " << a;
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
	this->file_types = a_babel_ui_file_type_selector_new(mode);
#ifdef K
	QObject::connect(data_source_file_dialog->file_types, SIGNAL("changed"), dialog, SLOT (a_babel_ui_type_selector_dialog_sensitivity_cb));
	gtk_combo_box_set_active(data_source_file_dialog->file_types, last_type);
	/* Manually call the callback to fix the state. */
	a_babel_ui_type_selector_dialog_sensitivity_cb(data_source_file_dialog->file_types, dialog);
#endif
	this->vbox->addWidget(this->file_types);



	this->button_box = new QDialogButtonBox();
	this->button_box->addButton("&Ok", QDialogButtonBox::AcceptRole);
	this->button_box->addButton("&Cancel", QDialogButtonBox::RejectRole);
	connect(this->button_box, &QDialogButtonBox::accepted, this, &DataSourceFileDialog::accept_cb);
	connect(this->button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
	this->vbox->addWidget(this->button_box);



	this->accept();
}




DataSourceFileDialog * data_source_file_dialog = NULL;

/* See VikDataSourceInterface. */
static void * datasource_file_init(acq_vik_t * avt)
{
	if (!data_source_file_dialog) {
		data_source_file_dialog = new DataSourceFileDialog("Import File with GPSBabel", avt->window);
	}

	datasource_file_widgets_t *widgets = (datasource_file_widgets_t *) malloc(sizeof (datasource_file_widgets_t));
	return widgets;
}




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
static void datasource_file_add_setup_widgets(GtkWidget * dialog, Viewport * viewport, void * user_data)
{
	data_source_file_dialog->build_ui();

	QStringList filter;
	filter << _("All files (*)");

	data_source_file_dialog->file_entry->file_selector->setNameFilters(filter);
	data_source_file_dialog->exec();

}




/* See VikDataSourceInterface/ */
static ProcessOptions * datasource_file_get_process_options(datasource_file_widgets_t * unused, void * not_used, char const * not_used2, char const * not_used3)
{
	ProcessOptions * po = new ProcessOptions();



#ifdef K
	/* Memorize the directory for later use. */
	free(last_folder_uri);
	last_folder_uri = gtk_file_chooser_get_current_folder_uri(GTK_FILE_CHOOSER(data_source_file_dialog->file_entry));

	/* Memorize the file filter for later use. */
	GtkFileFilter *filter = gtk_file_chooser_get_filter(GTK_FILE_CHOOSER(data_source_file_dialog->file_entry));
	last_file_type = (BabelFileType *) g_object_get_data(G_OBJECT(filter), "Babel");

	/* Retrieve and memorize file format selected. */
	last_type = gtk_combo_box_get_active(data_source_file_dialog->file_types);

#endif
	const char * selected = (a_babel_ui_file_type_selector_get(data_source_file_dialog->file_types))->name;

	/* Generate the process options. */
	po->babelargs = g_strdup_printf("-i %s", selected);
	po->filename = strdup(data_source_file_dialog->file_entry->get_filename().toUtf8().data());

	qDebug() << "II: Datasource File: using Babel args" << po->babelargs << "and file" << po->filename;

	return po;
}




/* See VikDataSourceInterface. */
static void datasource_file_cleanup(void * data)
{
	free(data);
}
