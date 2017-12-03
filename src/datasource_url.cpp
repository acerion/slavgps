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

#include <vector>
#include <cstdlib>

#include "datasource_url.h"
#include "acquire.h"
#include "babel.h"
#include "application_state.h"
#include "util.h"




using namespace SlavGPS;




/*
  Initially was just going to be a URL and always in GPX format.
  But might as well specify the file type as per datasource_file.c.
  However in this version we'll cope with no GPSBabel available and in this case just try GPX.
*/




#define INVALID_ENTRY_INDEX -1

/* Index of the last file format selected. */
static int g_last_file_type_index = INVALID_ENTRY_INDEX;




static DataSourceDialog * datasource_url_create_setup_dialog(Viewport * viewport, void * user_data);
static ProcessOptions * datasource_url_get_process_options(DownloadOptions & dl_options, char const * not_used2, char const * not_used3);
static int find_initial_file_type_index(void);




DataSourceInterface datasource_url_interface = {
	N_("Acquire from URL"),
	N_("URL"),
	DataSourceMode::AUTO_LAYER_MANAGEMENT,
	DatasourceInputtype::NONE,
	true,
	true,  /* true = keep dialog open after success. */
	true,  /* true = run as thread. */

	(DataSourceInitFunc)                  NULL,
	(DataSourceCheckExistenceFunc)        NULL,
	(DataSourceCreateSetupDialogFunc)     datasource_url_create_setup_dialog,
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




#define VIK_SETTINGS_URL_FILE_DL_TYPE "url_file_download_type"




static DataSourceDialog * datasource_url_create_setup_dialog(Viewport * viewport, void * user_data)
{
	return new DataSourceURLDialog();
}




DataSourceURLDialog::DataSourceURLDialog()
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
}




DataSourceURLDialog::~DataSourceURLDialog()
{
}




ProcessOptions * DataSourceURLDialog::get_process_options(DownloadOptions & dl_options)
{
	ProcessOptions * po = new ProcessOptions();

	/* TODO: handle situation when there is only one item in the combo (i.e. GPX). */

	g_last_file_type_index = this->file_type_combo.currentIndex();

	po->input_file_type = ""; /* Default to gpx. */
	if (Babel::file_types.size()) {
		po->input_file_type = Babel::file_types.at(g_last_file_type_index)->identifier;
	}

	po->url = this->url_input.text();

	/* Support .zip + bzip2 files directly. */
	dl_options.convert_file = a_try_decompress_file;
	dl_options.follow_location = 5;

	return po;

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
