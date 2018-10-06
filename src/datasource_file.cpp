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




#include <vector>
#include <cstring>
#include <cstdlib>
#include <cassert>




#include <QDebug>




#include "datasource_file.h"
#include "acquire.h"
#include "babel.h"
#include "gpx.h"
#include "babel_dialog.h"
#include "util.h"
#include "widget_file_entry.h"




using namespace SlavGPS;




/* The last used directory. */
static QUrl g_last_directory_url;


/* The last used file filter. */
/* TODO_2_LATER: verify how this overlaps with babel_dialog.cpp:g_last_file_type_index. */
static QString g_last_filter;




DataSourceFile::DataSourceFile()
{
	this->window_title = QObject::tr("Import file with GPSBabel");
	this->layer_title = QObject::tr("Imported file");
	this->mode = DataSourceMode::AutoLayerManagement;
	this->input_type = DataSourceInputType::None;
	this->autoview = true;
	this->keep_dialog_open = true; /* true = keep dialog open after success. */
	this->is_thread = true;
}




int DataSourceFile::run_config_dialog(AcquireContext & acquire_context)
{
	DataSourceFileDialog config_dialog("");

	const int answer = config_dialog.exec();
	if (answer == QDialog::Accepted) {
		this->acquire_options = config_dialog.create_acquire_options_none();
		this->download_options = new DownloadOptions; /* With default values. */
	}

	return answer;
}




DataSourceFileDialog::DataSourceFileDialog(const QString & title) : BabelDialog(title)
{
	this->build_ui();
	if (g_last_directory_url.isValid()) {
		this->file_selector->set_directory_url(g_last_directory_url);
	}
	if (!g_last_filter.isEmpty()) {
		this->file_selector->select_name_filter(g_last_filter);
	}

	this->file_selector->setFocus();

	connect(this->button_box, &QDialogButtonBox::accepted, this, &DataSourceFileDialog::accept_cb);
}




DataSourceFileDialog::~DataSourceFileDialog()
{
}




BabelOptions * DataSourceFileDialog::get_acquire_options_none(void)
{
	g_last_directory_url = this->file_selector->get_directory_url();
	g_last_filter = this->file_selector->get_selected_name_filter();


	/* Generate the process options. */
	BabelOptions * babel_options = new BabelOptions(BabelOptionsMode::None);

	/* TODO_LATER: this needs to be deleted. */
	babel_options->importer = new BabelLocalFileImporter(this->file_selector->get_selected_file_full_path(),
							     this->get_file_type_selection()->identifier);

	return babel_options;
}




void DataSourceFileDialog::accept_cb(void)
{
	const BabelFileType * file_type = this->get_file_type_selection();

	qDebug() << "II: Datasource File: dialog result: accepted";
	qDebug() << "II: Datasource File: selected format type identifier:" << file_type->identifier;
	qDebug() << "II: Datasource File: selected format type label:" << file_type->label;
	qDebug() << "II: Datasource File: selected file path:" << this->file_selector->get_selected_file_full_path();
}
