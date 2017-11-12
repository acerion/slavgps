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




extern std::map<int, BabelFileType *> a_babel_file_types;

/* The last file format selected. */
static int g_last_type_id = -1;




static DataSourceDialog * datasource_url_create_setup_dialog(Viewport * viewport, void * user_data);
static ProcessOptions * datasource_url_get_process_options(DownloadOptions & dl_options, char const * not_used2, char const * not_used3);




VikDataSourceInterface vik_datasource_url_interface = {
	N_("Acquire from URL"),
	N_("URL"),
	DatasourceMode::AUTO_LAYER_MANAGEMENT,
	DatasourceInputtype::NONE,
	true,
	true,  /* true = keep dialog open after success. */
	true,  /* true = run as thread. */

	(VikDataSourceInitFunc)               NULL,
	(VikDataSourceCheckExistenceFunc)     NULL,
	(DataSourceCreateSetupDialogFunc)     datasource_url_create_setup_dialog,
	(VikDataSourceGetProcessOptionsFunc)  NULL,
	(VikDataSourceProcessFunc)            a_babel_convert_from,
	(VikDataSourceProgressFunc)           NULL,
	(DataSourceCreateProgressDialogFunc)  NULL,
	(VikDataSourceCleanupFunc)            NULL,
	(DataSourceTurnOffFunc)               NULL,
	NULL,
	0,
	NULL,
	NULL,
	0
};




static int find_entry = -1;
static int wanted_entry = -1;




static void find_type(BabelFileType * file_type, const QString & type_name)
{
	find_entry++;
	if (file_type->name == type_name) {
		wanted_entry = find_entry;
	}
}




#define VIK_SETTINGS_URL_FILE_DL_TYPE "url_file_download_type"




static DataSourceDialog * datasource_url_create_setup_dialog(Viewport * viewport, void * user_data)
{
	return new DataSourceURLDialog();
}




DataSourceURLDialog::DataSourceURLDialog()
{
	QLabel * url_label = new QLabel(QObject::tr("URL:"));
	QLabel * type_combo_label = new QLabel(QObject::tr("File type:"));

	if (g_last_type_id < 0) {
		find_entry = -1;
		wanted_entry = -1;
		QString type;
		if (ApplicationState::get_string(VIK_SETTINGS_URL_FILE_DL_TYPE, type)) {
			/* Use setting. */
			if (!type.isEmpty()) {
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
		g_last_type_id = (wanted_entry < 0) ? 0 : wanted_entry;
	}


	if (a_babel_available()) {
		for (auto iter = a_babel_file_types.begin(); iter != a_babel_file_types.end(); iter++) {
			this->type_combo.addItem(iter->second->label);
		}
		if (g_last_type_id >= 0) {
			this->type_combo.setCurrentIndex(g_last_type_id);
		}
	} else {
		/* Only GPX (not using GPSbabel). */
		this->type_combo.addItem(QObject::tr("GPX"));
	}

	this->grid->addWidget(url_label, 0, 0);
	this->grid->addWidget(&this->url_input, 1, 0);
	this->grid->addWidget(type_combo_label, 2, 0);
	this->grid->addWidget(&this->type_combo, 3, 0);
}




DataSourceURLDialog::~DataSourceURLDialog()
{
}




ProcessOptions * DataSourceURLDialog::get_process_options(DownloadOptions & dl_options)
{
	ProcessOptions * po = new ProcessOptions();

	/* TODO: handle situation when there is only one item in the combo (i.e. GPX). */

	g_last_type_id = this->type_combo.currentIndex();

	po->input_file_type = ""; /* Default to gpx. */
	if (a_babel_file_types.size()) {
		po->input_file_type = a_babel_file_types.at(g_last_type_id)->name;
	}

	po->url = this->url_input.text();

	/* Support .zip + bzip2 files directly. */
	dl_options.convert_file = a_try_decompress_file;
	dl_options.follow_location = 5;

	return po;

}
