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




#define SG_MODULE "Babel Dialog"




/* The last file format selected. */
static int g_last_file_type_index = 0;

/* TODO_2_LATER: verify how to implement "last used directory" here. Look at
   g_last_directory_url in datasource_file.cpp. */




/**
   \brief Create a list of gpsbabel file types

   \param input_mode: the mode to filter the file types

   \return list of file types
*/
QComboBox * BabelDialog::build_file_type_selector(const BabelMode * operating_mode)
{
	BabelMode mode;
	if (operating_mode) {
		mode = *operating_mode;
	} else {
		/* The dialog is in "import" mode. Propose any readable file. */
		BabelMode read_mode = { 1, 0, 1, 0, 1, 0 };
		mode = read_mode;
	}

	QComboBox * combo = new QComboBox();

	/* Add a first label to invite user to select a file type.
	   user data == -1 distinguishes this entry. This entry can be also recognized by index == 0. */
	combo->addItem(tr("Select a file type"), -1);

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
		for (auto iter = Babel::file_types.begin(); iter != Babel::file_types.end(); iter++) {

			BabelFileType * file_type = iter->second;
			/* Call function when any read mode found. */
			if (file_type->mode.waypoints_read
			    || file_type->mode.tracks_read
			    || file_type->mode.routes_read) {

				combo->addItem(file_type->label, iter->first);
			}
		}
	} else {
		/* Run a function on all file types supporting a given mode. */
		for (auto iter = Babel::file_types.begin(); iter != Babel::file_types.end(); iter++) {
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
				combo->addItem(file_type->label, iter->first);
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
	/* File type ID that was used in combo->addItem(<file type>, id);
	   A special item has been added with id == -1.
	   All other items have been added with id >= 0. */
	const int id = this->file_types_combo->currentData().toInt();
	if (id == -1) {
		qDebug() << "II: Babel Dialog: selected file type: NONE";
		return NULL;
	} else {
		BabelFileType * file_type = Babel::file_types.at(id);
		qDebug() << "II: Babel Dialog: selected file type:" << file_type->identifier << ", " << file_type->label;
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





BabelDialog::BabelDialog(const QString & window_title, QWidget * parent_) : DataSourceDialog(window_title)
{
}




BabelDialog::~BabelDialog()
{
	delete this->file_types_combo;
	delete this->mode_box;
}




void BabelDialog::build_ui(const BabelMode * mode)
{
	qDebug() << "II: Babel Dialog: building dialog UI";



	this->grid->addWidget(new QLabel(tr("File:")), 0, 0);



	if (mode && (mode->tracks_write || mode->routes_write || mode->waypoints_write)) {
		this->file_selector = new FileSelectorWidget(QFileDialog::Option(0), QFileDialog::AnyFile, tr("Select Target File File for Export"), NULL);
		this->file_selector->set_accept_mode(QFileDialog::AcceptSave);
	} else {
		this->file_selector = new FileSelectorWidget(QFileDialog::Option(0), QFileDialog::ExistingFile, tr("Select File to Import"), NULL);
	}
	this->grid->addWidget(this->file_selector, 1, 0);

#ifdef K_TODO_MAYBE
	/* We don't do this because we don't have filename here. */
	if (!filename.isEmpty()) {
		this->file_entry->preselect_file(filename);
	}

	/* We don't do this because last_directory_url is kept by classes inheriting from this class. */
	if (last_directory_url.isValid()) {
		this->file_entry->set_directory_url(last_directory_url);
	}
#endif



	this->grid->addWidget(new QLabel(tr("File type:")), 2, 0);



	this->file_types_combo = this->build_file_type_selector(mode);
	this->file_types_combo->setCurrentIndex(g_last_file_type_index);
	this->grid->addWidget(this->file_types_combo, 3, 0);
	QObject::connect(this->file_types_combo, SIGNAL (currentIndexChanged(int)), this, SLOT (file_type_changed_cb(int)));



#define DO_DEBUG 0
#if DO_DEBUG
	BabelMode mode2;
	mode2.tracks_write = true;
	mode2.routes_write = true;
	mode2.waypoints_write = true;
	if (mode2.tracks_write || mode2.routes_write || mode2.waypoints_write) {
#else
	if (mode && (mode->tracks_write || mode->routes_write || mode->waypoints_write)) {
#endif
		/* These checkboxes are only for "export" mode (at least for now). */

		QFrame * line = new QFrame();
		line->setFrameShape(QFrame::HLine);
		line->setFrameShadow(QFrame::Sunken);
		this->grid->addWidget(line, 4, 0);


		this->grid->addWidget(new QLabel(tr("Export these items:")), 5, 0);
#if DO_DEBUG
		this->mode_box = this->build_mode_selector(mode2.tracks_write, mode2.routes_write, mode2.waypoints_write);
#else
		this->mode_box = this->build_mode_selector(mode->tracks_write, mode->routes_write, mode->waypoints_write);
#endif
		this->grid->addLayout(this->mode_box, 6, 0);
	}
#undef DO_DEBUG


	/* Manually call the callback to set state of OK button. */
	this->file_type_changed_cb(g_last_file_type_index);


	/* Blinky cursor in input field will be visible and will bring
	   user's eyes to widget that has a focus. */
	this->file_selector->setFocus();


	connect(this->button_box, SIGNAL (accepted(void)), this, SLOT (on_accept_cb(void)));
}




void BabelDialog::file_type_changed_cb(int index)
{
	qDebug() << "SLOT: Babel Dialog: current index changed to" << index;

	/* Only allow dialog's validation when format selection is done. */
	QPushButton * button = this->button_box->button(QDialogButtonBox::Ok);
	button->setEnabled(index != 0); /* Index is zero. User Data is -1. */



	/* Update file type filters in file selection dialog according
	   to currently selected babel file type. */
	QStringList filters;

	BabelFileType * selection = this->get_file_type_selection();
	if (selection) {
		if (!selection->extension.isEmpty()) {
			const QString selected = selection->label + " (*." + selection->extension + ")";
			qDebug() << SG_PREFIX_I << "using" << selected << "as selected file filter";
			filters << selected;
		}
	}

	filters << tr("All files (*)");

	this->file_selector->set_name_filters(filters);
}




void BabelDialog::on_accept_cb(void)
{
	g_last_file_type_index = this->file_types_combo->currentIndex();
	if (g_last_file_type_index != 0) {
		const BabelFileType * file_type = Babel::file_types.at(g_last_file_type_index);
		qDebug() << "SLOT: Babel Dialog: On Accept: selected file type:" << g_last_file_type_index << file_type->identifier << file_type->label;
	} else {
		qDebug() << "SLOT: Babel Dialog: On Accept: last file type index =" << g_last_file_type_index;
	}
}
