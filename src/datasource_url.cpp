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
 */




#include <vector>
#include <cstdlib>
#include <cassert>




#include "datasource_url.h"
#include "layer_trw_import.h"
#include "babel.h"
#include "application_state.h"
#include "util.h"




using namespace SlavGPS;




#define VIK_SETTINGS_URL_FILE_DL_TYPE "url_file_download_type"




/*
  Initially was just going to be a URL and always in GPX format.
  But might as well specify the file type as per datasource_file.c.
  However in this version we'll cope with no GPSBabel available and in this case just try GPX.
*/

#define INVALID_ENTRY_INDEX -1

/* Index of the last file format selected. */
static int g_last_file_type_index = INVALID_ENTRY_INDEX;




static int find_initial_file_type_index(void);




DataSourceURL::DataSourceURL()
{
	this->m_window_title = QObject::tr("Acquire data from URL");
	this->m_layer_title = QObject::tr("From URL");
	this->m_layer_mode = TargetLayerMode::AutoLayerManagement;
	this->m_autoview = true;
	this->m_keep_dialog_open_after_success = true;
}




SGObjectTypeID DataSourceURL::get_source_id(void) const
{
	return DataSourceURL::source_id();
}
SGObjectTypeID DataSourceURL::source_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.datasource.url");
	return id;
}




int DataSourceURL::run_config_dialog(AcquireContext & acquire_context)
{
	DataSourceURLDialog config_dialog(this->m_window_title);

	const int answer = config_dialog.exec();
	if (answer == QDialog::Accepted) {
		this->m_acquire_options = config_dialog.create_acquire_options(acquire_context);

		this->m_download_options = new DownloadOptions; /* With default values. */
		/* Support .zip + bzip2 files directly. */
		this->m_download_options->convert_file = a_try_decompress_file;
		this->m_download_options->follow_location = 5;
	}

	return answer;
}




DataSourceURLDialog::DataSourceURLDialog(const QString & window_title) : DataSourceDialog(window_title)
{
	if (g_last_file_type_index == INVALID_ENTRY_INDEX) {
		g_last_file_type_index = find_initial_file_type_index();
	}
	/* After this the index is valid. */

	if (Babel::is_available()) {
		for (auto iter = Babel::file_types.begin(); iter != Babel::file_types.end(); iter++) {
			this->file_type_combo.addItem(iter->second->label);
		}
		this->file_type_combo.setCurrentIndex(g_last_file_type_index);
	} else {
		/* Only GPX (not using GPSbabel). */
		this->file_type_combo.addItem(QObject::tr("GPX"));
	}


	this->grid->addWidget(new QLabel(QObject::tr("URL:")), 0, 0);
	this->grid->addWidget(&this->url_input, 1, 0);
	this->grid->addWidget(new QLabel(QObject::tr("File type:")), 2, 0);
	this->grid->addWidget(&this->file_type_combo, 3, 0);

	this->url_input.setFocus();
}




DataSourceURLDialog::~DataSourceURLDialog()
{
}




AcquireOptions * DataSourceURLDialog::create_acquire_options(__attribute__((unused)) AcquireContext & acquire_context)
{
	AcquireOptions * babel_options = new AcquireOptions(AcquireOptions::Mode::FromURL);

	g_last_file_type_index = this->file_type_combo.currentIndex();

	if (Babel::is_available()) {
		babel_options->input_data_format = Babel::file_types.at(g_last_file_type_index)->identifier;
	} else {
		babel_options->input_data_format = ""; /* Default to gpx. */
	}

	babel_options->source_url = this->url_input.text();

	return babel_options;
}




int find_initial_file_type_index(void)
{
	QString type_identifier;
	if (ApplicationState::get_string(VIK_SETTINGS_URL_FILE_DL_TYPE, type_identifier)) {
		/* Use setting. */
	} else {
		/* Default to this value if necessary. */
		type_identifier = "gpx";
	}

	int entry_index = INVALID_ENTRY_INDEX;
	bool found = false;
	if (!type_identifier.isEmpty()) {
		for (auto iter = Babel::file_types.begin(); iter != Babel::file_types.end(); iter++) {
			entry_index++;
			if (iter->second->identifier == type_identifier) {
				found = true;
				break;
			}
		}
	}

	if (!found) {
		/* First entry in Babel::file_types. */
		entry_index = 0;
	}

	return entry_index;
}
