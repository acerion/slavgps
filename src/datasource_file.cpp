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
/* TODO: verify how this overlaps with babel_dialog.cpp:g_last_file_type_index. */
static QString g_last_filter;




DataSourceFile::DataSourceFile()
{
	this->window_title = QObject::tr("Import file with GPSBabel");
	this->layer_title = QObject::tr("Imported file");
	this->mode = DataSourceMode::AUTO_LAYER_MANAGEMENT;
	this->input_type = DataSourceInputType::None;
	this->autoview = true;
	this->keep_dialog_open = true; /* true = keep dialog open after success. */
	this->is_thread = true;
}




DataSourceDialog * DataSourceFile::create_setup_dialog(Viewport * viewport, void * user_data)
{
	return new DataSourceFileDialog("");
}



DataSourceFileDialog::DataSourceFileDialog(const QString & title) : BabelDialog(title)
{
	this->build_ui();
	if (g_last_directory_url.isValid()) {
		this->file_entry->file_selector->setDirectoryUrl(g_last_directory_url);
	}
	if (!g_last_filter.isEmpty()) {
		this->file_entry->file_selector->selectNameFilter(g_last_filter);
	}

	this->file_entry->setFocus();

	connect(this->button_box, &QDialogButtonBox::accepted, this, &DataSourceFileDialog::accept_cb);
}




DataSourceFileDialog::~DataSourceFileDialog()
{
}




ProcessOptions * DataSourceFileDialog::get_process_options(void)
{
	ProcessOptions * po = new ProcessOptions();

	g_last_directory_url = this->file_entry->file_selector->directoryUrl();
	g_last_filter = this->file_entry->file_selector->selectedNameFilter();


	const QString selected = this->get_file_type_selection()->identifier;

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
	qDebug() << "II: Datasource File: selected format type identifier:" << file_type->identifier;
	qDebug() << "II: Datasource File: selected format type label:" << file_type->label;
	qDebug() << "II: Datasource File: selected file path:" << this->file_entry->get_filename();
}
