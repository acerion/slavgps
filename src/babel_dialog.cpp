/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#include <QCheckBox>
#include <QDebug>
#include <QLabel>

#include "babel_dialog.h"
#include "widget_file_entry.h"



using namespace SlavGPS;




extern std::map<int, BabelFileType *> a_babel_file_types;




/* The last file format selected. */
static int last_type = 0;




/**
   \brief Create a list of gpsbabel file types

   \param mode: the mode to filter the file types

   \return list of file types
*/
QComboBox * BabelDialog::build_file_type_selector(const BabelMode & mode)
{
	QComboBox * combo = new QComboBox();

	/* Add a first label to invite user to select a file type.
	   user data == -1 distinguishes this entry. This entry can be also recognized by index == 0. */
	combo->addItem("Select a file type", -1);

	/* Add all known and compatible file types */
	if (mode.tracks_read
	    && mode.routes_read
	    && mode.waypoints_read
	    && !mode.tracks_write
	    && !mode.routes_write
	    && !mode.waypoints_write) {

		/* Run a function on all file types with any kind of read method
		   (which is almost all but not quite - e.g. with GPSBabel v1.4.4
		   - PalmDoc is write only waypoints). */
		for (auto iter = a_babel_file_types.begin(); iter != a_babel_file_types.end(); iter++) {

			BabelFileType * file_type = iter->second;
			/* Call function when any read mode found. */
			if (file_type->mode.waypoints_read
			    || file_type->mode.tracks_read
			    || file_type->mode.routes_read) {

				combo->addItem(QString(file_type->label), iter->first);
			}
		}
	} else {
		/* Run a function on all file types supporting a given mode. */
		for (auto iter = a_babel_file_types.begin(); iter != a_babel_file_types.end(); iter++) {
			BabelFileType * file_type = iter->second;
			/* Check compatibility of modes. */
			bool compat = true;
			if (mode.waypoints_read  && !file_type->mode.waypoints_read)  compat = false;
			if (mode.waypoints_write && !file_type->mode.waypoints_write) compat = false;
			if (mode.tracks_read     && !file_type->mode.tracks_read)     compat = false;
			if (mode.tracks_write    && !file_type->mode.tracks_write)    compat = false;
			if (mode.routes_read     && !file_type->mode.routes_read)     compat = false;
			if (mode.routes_write    && !file_type->mode.routes_write)    compat = false;
			/* Do call. */
			if (compat) {
				combo->addItem(QString(file_type->label), iter->first);
			}
		}
	}

	/* Initialize the selection with the really first entry */
	combo->setCurrentIndex(0);

	return combo;
}




/**
   \brief Retrieve the selected file type

   \param combo: list with gpsbabel file types

   \return the selected BabelFileType or NULL
*/
BabelFileType * BabelDialog::get_file_type_selection(void)
{
	/* ID that was used in combo->addItem(<file type>, id);
	   A special item has been added with id == -1.
	   All other items have been added with id >= 0. */
	int i = this->file_types_combo->currentData().toInt();
	if (i == -1) {
		qDebug() << "II: Babel Dialog: selected file type: NONE";
		return NULL;
	} else {
		BabelFileType * file_type = a_babel_file_types.at(i);
		qDebug().nospace() << "II: Babel Dialog: selected file type: '" << file_type->name << "', '" << file_type->label << "'";
		return file_type;
	}
}




/**
   \brief Create a selector for babel modes. This selector is based on 3 checkboxes.

   \param tracks:
   \param routes:
   \param waypoints:

   \return a layout widget packing all checkboxes
*/
QHBoxLayout * BabelDialog::build_mode_selector(bool tracks, bool routes, bool waypoints)
{
	QHBoxLayout * hbox = new QHBoxLayout();
	QCheckBox * checkbox = NULL;

	const QString tooltip = tr("Select the information to process.\n"
				   "Warning: the behavior of these switches is highly dependent of the file format selected.\n"
				   "Please, refer to GPSBabel documentation if unsure.");

	checkbox = new QCheckBox(QObject::tr("Tracks"));
	checkbox->setChecked(tracks);
	checkbox->setToolTip(tooltip);
	hbox->addWidget(checkbox);

	checkbox = new QCheckBox(QObject::tr("Routes"));
	checkbox->setChecked(routes);
	checkbox->setToolTip(tooltip);
	hbox->addWidget(checkbox);

	checkbox = new QCheckBox(QObject::tr("Waypoints"));
	checkbox->setChecked(waypoints);
	checkbox->setToolTip(tooltip);
	hbox->addWidget(checkbox);

	return hbox;
}




/**
   \brief Retrieve state of checkboxes

   \param hbox:
   \param tracks: return value
   \param routes: return value
   \param waypoints: return value
*/
void BabelDialog::get_write_mode(BabelMode & mode)
{
	if (!this->mode_box) {
		qDebug() << "EE: Babel Dialog: calling get write mode for object with NULL mode box";
		return;
	}

	QWidget * widget = NULL;
	QCheckBox * checkbox = NULL;

	widget = this->mode_box->itemAt(0)->widget();
	if (!widget) {
		qDebug() << "EE: Babel Dialog: failed to get checkbox 0";
		return;
	}
	mode.tracks_write = ((QCheckBox *) widget)->isChecked();

	widget = this->mode_box->itemAt(1)->widget();
	if (!widget) {
		qDebug() << "EE: Babel Dialog: failed to get checkbox 1";
		return;
	}
	mode.routes_write = ((QCheckBox *) widget)->isChecked();

	widget = this->mode_box->itemAt(2)->widget();
	if (!widget) {
		qDebug() << "EE: Babel Dialog: failed to get checkbox 2";
		return;
	}
	mode.waypoints_write = ((QCheckBox *) widget)->isChecked();

	return;
}





BabelDialog::BabelDialog(QString const & title, QWidget * parent_) : QDialog(parent_)
{
	this->setWindowTitle(title);
}




BabelDialog::~BabelDialog()
{
	delete this->file_types_combo;
	delete this->mode_box;
}




void BabelDialog::build_ui(const BabelMode * mode)
{
	qDebug() << "II: Babel Dialog: building dialog UI";

	this->vbox = new QVBoxLayout;
	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox);


	QLabel * label = new QLabel("File");
	this->vbox->addWidget(label);


	if (mode && (mode->tracks_write || mode->routes_write || mode->waypoints_write)) {
		this->file_entry = new SGFileEntry(QFileDialog::Option(0), QFileDialog::AnyFile, SGFileTypeFilter::ANY, tr("Select Target File File for Export"), NULL);
		this->file_entry->file_selector->setAcceptMode(QFileDialog::AcceptSave);
	} else {
		this->file_entry = new SGFileEntry(QFileDialog::Option(0), QFileDialog::ExistingFile, SGFileTypeFilter::ANY, tr("Select File to Import"), NULL);
	}


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
	filter << tr("All files (*)");

	this->file_entry->file_selector->setNameFilters(filter);

	for (auto iter = a_babel_file_types.begin(); iter != a_babel_file_types.end(); iter++) {

		QString a = QString((iter->second)->label) + "(" + QString((iter->second)->ext) + ")";
		//qDebug() << "II: Babel Dialog: adding file filter " << a;
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
	if (mode) {
		this->file_types_combo = build_file_type_selector(*mode);
	} else {
		/* The dialog is in "import" mode. Propose any readable file. */
		const BabelMode read_mode = { 1, 0, 1, 0, 1, 0 };
		this->file_types_combo = build_file_type_selector(read_mode);
	}
	this->vbox->addWidget(this->file_types_combo);

	QObject::connect(this->file_types_combo, SIGNAL (currentIndexChanged(int)), this, SLOT (file_type_changed_cb(int)));
	this->file_types_combo->setCurrentIndex(last_type);


	if (mode && (mode->tracks_write || mode->routes_write || mode->waypoints_write)) {

		/* These checkboxes are only for "export" mode (at least for now). */

		QFrame * line = new QFrame();
		line->setFrameShape(QFrame::HLine);
		line->setFrameShadow(QFrame::Sunken);
		this->vbox->addWidget(line);

		label = new QLabel(tr("Export these items:"));
		this->vbox->addWidget(label);

		this->mode_box = this->build_mode_selector(mode->tracks_write, mode->routes_write, mode->waypoints_write);
		this->vbox->addLayout(this->mode_box);
	}


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




void BabelDialog::file_type_changed_cb(int index)
{
	qDebug() << "SLOT: Babel Dialog: current index changed to" << index;

	/* Only allow dialog's validation when format selection is done. */
	QPushButton * button = this->button_box->button(QDialogButtonBox::Ok);
	button->setEnabled(index != 0); /* Index is zero. User Data is -1. */
}




void BabelDialog::add_file_type_filter(BabelFileType * file_type)
{
	char const * label = file_type->label;
	char const * ext = file_type->ext;
	if (ext == NULL || ext[0] == '\0') {
		/* No file extension => no filter. */
		return;
	}
	const QString pattern = QString("*.%1").arg(ext);

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
