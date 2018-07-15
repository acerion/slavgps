/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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




#include <cstring>




#include <QDebug>




#include "widget_file_entry.h"




using namespace SlavGPS;




/* TODO: add copying of text edited in QLineEdit into this->current_file_full_path and this->file_dialog. */




SGFileEntry::SGFileEntry(enum QFileDialog::Option options, enum QFileDialog::FileMode mode, SGFileTypeFilter file_type_filter, const QString & title, QWidget * parent_widget) : QWidget(parent_widget)
{
	this->file_dialog = new QFileDialog();
	this->file_dialog->setFileMode(mode);
	this->file_dialog->setOptions(options);
	this->file_dialog->setWindowTitle(title);

	this->add_file_type_filters(file_type_filter);

	this->line = new QLineEdit(this);
	this->browse = new QPushButton("Browse", this);


	this->hbox = new QHBoxLayout;
	this->hbox->addWidget(this->line);
	this->hbox->addWidget(this->browse);
	this->hbox->setContentsMargins(0, 0, 0, 0);


	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->hbox);

	connect(this->browse, SIGNAL(clicked()), this, SLOT(open_browser_cb()));

	/* Input line should be primary "focus receiver" because cursor
	   blinking in input line has a good visibility. */
	this->setFocusProxy(this->line);
}




SGFileEntry::~SGFileEntry()
{
}




void SGFileEntry::add_file_type_filters(SGFileTypeFilter file_type_filter)
{
	/* Always have an catch all filter at the end. */

	QStringList mime;

	switch (file_type_filter) {
	case SGFileTypeFilter::IMAGE: {
		mime << "image/jpeg";
		mime << "image/png";
		mime << "image/tiff";
		mime << "application/octet-stream"; /* "All files (*)" */

		break;
	}
	case SGFileTypeFilter::MBTILES: {
		mime << tr("MBTiles (*.sqlite, *.mbtiles, *.db3)");
		mime << tr("All files (*)");
		break;
	}
	case SGFileTypeFilter::XML: {
		mime << tr("XML (*.xml)");
		mime << tr("All files (*)");
		break;
	}
	case SGFileTypeFilter::CARTO: {
		mime << tr("MML (.*.mml)");
		mime << tr("All files (*)");
		break;
	}
	default:
		mime << tr("All files (*)");
		break;
	}

	this->file_dialog->setMimeTypeFilters(mime);
}




void SGFileEntry::open_browser_cb(void) /* Slot. */
{

	if (this->file_dialog->exec()) {
		this->current_file_full_path = this->file_dialog->selectedFiles().at(0);
		this->line->insert(this->current_file_full_path);
		qDebug() << "II: Widget File Entry: clicking OK results in this file:" << this->current_file_full_path;

	}
}




void SGFileEntry::preselect_file_full_path(const QString & filename)
{
	this->current_file_full_path = filename;
	if (this->file_dialog) {
		this->file_dialog->selectFile(this->current_file_full_path);
	}
	this->line->insert(this->current_file_full_path);
}




QString SGFileEntry::get_selected_file_full_path(void) const
{
	QStringList selection = this->file_dialog->selectedFiles();
	static QString empty("");

	if (selection.size()) {
		qDebug() << "II: Widget File Entry: will return" << selection.at(0) << "to caller";
		return selection.at(0);
	} else {
		qDebug() << "II: Widget File Entry: will return empty string";
		return empty;
	}
}




QStringList SGFileEntry::get_selected_files_full_paths(void) const
{
	return this->file_dialog->selectedFiles();
}




QUrl SGFileEntry::get_directory_url(void) const
{
	return this->file_dialog->directoryUrl();
}




void SGFileEntry::set_directory_url(const QUrl & dir_url)
{
	this->file_dialog->setDirectoryUrl(dir_url);
}




void SGFileEntry::set_name_filters(const QStringList & name_filters)
{
	this->file_dialog->setNameFilters(name_filters);
}




void SGFileEntry::select_name_filter(const QString & name_filter)
{
	this->file_dialog->selectNameFilter(name_filter);
}




QString SGFileEntry::get_selected_name_filter(void) const
{
	return this->file_dialog->selectedNameFilter();
}




void SGFileEntry::set_accept_mode(QFileDialog::AcceptMode accept_mode)
{
	this->file_dialog->setAcceptMode(accept_mode);
}
