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
#include "layer_trw_import.h"
#include "babel.h"
#include "gpx.h"
#include "babel_dialog.h"
#include "util.h"
#include "widget_file_entry.h"




using namespace SlavGPS;




#define SG_MODULE "DataSource File"




DataSourceFile::DataSourceFile()
{
	this->m_window_title = QObject::tr("Import file with GPSBabel");
	this->m_layer_title = QObject::tr("Imported file");
	this->m_layer_mode = TargetLayerMode::AutoLayerManagement;
	this->m_autoview = true;
	this->m_keep_dialog_open_after_success = true;
}




SGObjectTypeID DataSourceFile::get_source_id(void) const
{
	return DataSourceFile::source_id();
}
SGObjectTypeID DataSourceFile::source_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.datasource.file");
	return id;
}




int DataSourceFile::run_config_dialog(AcquireContext & acquire_context)
{
	DataSourceFileDialog config_dialog("");

	const int answer = config_dialog.exec();
	if (answer == QDialog::Accepted) {
		this->m_acquire_options = config_dialog.create_acquire_options(acquire_context);
		this->m_download_options = new DownloadOptions; /* With default values. */
	}

	return answer;
}




DataSourceFileDialog::DataSourceFileDialog(const QString & title, QWidget * parent_widget) : BabelDialog(title, parent_widget)
{
	this->build_ui();

	this->file_selector->setFocus();

	connect(this->button_box, &QDialogButtonBox::accepted, this, &DataSourceFileDialog::accept_cb);
}




DataSourceFileDialog::~DataSourceFileDialog()
{
}




AcquireOptions * DataSourceFileDialog::create_acquire_options(__attribute__((unused)) AcquireContext & acquire_context)
{
	/* Generate the process options. */
	AcquireOptions * acquire_options = new AcquireOptions();

	acquire_options->babel_process = new BabelProcess();
	acquire_options->babel_process->set_input(this->get_file_type_selection()->identifier, this->file_selector->get_selected_file_full_path());

	return acquire_options;
}




void DataSourceFileDialog::accept_cb(void)
{
	const BabelFileType * file_type = this->get_file_type_selection();

	qDebug() << SG_PREFIX_I << "Dialog result: accepted";
	qDebug() << SG_PREFIX_I << "Selected format type identifier:" << file_type->identifier;
	qDebug() << SG_PREFIX_I << "Selected format type label:" << file_type->label;
	qDebug() << SG_PREFIX_I << "Selected file path:" << this->file_selector->get_selected_file_full_path();
}
