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




FileSelectorWidget::FileSelectorWidget(enum QFileDialog::Option options, enum QFileDialog::FileMode mode, const QString & title, QWidget * parent_widget) : QWidget(parent_widget)
{
	this->file_dialog = new QFileDialog();
	this->file_dialog->setFileMode(mode);
	this->file_dialog->setOptions(options);
	this->file_dialog->setWindowTitle(title);

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




FileSelectorWidget::~FileSelectorWidget()
{
}




void FileSelectorWidget::set_file_type_filter(FileTypeFilter new_file_type_filter)
{
	this->file_type_filter = new_file_type_filter;

	QStringList filter_list;
	const bool is_mime = FileSelectorWidget::get_file_filter_string(this->file_type_filter, filter_list);

	if (is_mime) {
		this->file_dialog->setMimeTypeFilters(filter_list);
	} else {
		this->file_dialog->setNameFilters(filter_list);
	}
}




bool FileSelectorWidget::get_file_filter_string(FileTypeFilter file_type_filter, QStringList & filter_list)
{
	bool is_mime = true;

	switch (file_type_filter) {
	case FileTypeFilter::Image:
		filter_list << "image/jpeg";
		filter_list << "image/png";
		filter_list << "image/tiff";
		filter_list << "application/octet-stream"; /* "All files (*)" */
		is_mime = true;
		break;

	case FileTypeFilter::MBTILES:
		filter_list << tr("MBTiles (*.sqlite, *.mbtiles, *.db3)");
		filter_list << tr("All files (*)");
		is_mime = false;
		break;

	case FileTypeFilter::XML:
		filter_list << tr("XML (*.xml)");
		filter_list << tr("All files (*)");
		is_mime = false;
		break;

	case FileTypeFilter::Carto:
		filter_list << tr("MML (.*.mml)");
		filter_list << tr("All files (*)");
		is_mime = false;
		break;

	case FileTypeFilter::JPEG:
		filter_list << "image/jpeg";
		filter_list << "application/octet-stream"; /* "All files (*)" */
		is_mime = true;
		break;

	case FileTypeFilter::GeoJSON:
		filter_list << tr("GeoJSON (*.geojson)");
		filter_list << tr("All files (*)");
		is_mime = false;
		break;

	default:
		/* Always have an catch all filter at the end. */
		filter_list << "application/octet-stream"; /* "All files (*)" */
		is_mime = true;
		break;
	}

	return is_mime;
}




void FileSelectorWidget::open_browser_cb(void) /* Slot. */
{
	if (QDialog::Accepted == this->file_dialog->exec()) {
		this->current_file_full_path = this->file_dialog->selectedFiles().at(0);
		this->line->insert(this->current_file_full_path);
		qDebug() << "II: Widget File Entry: clicking OK results in this file:" << this->current_file_full_path;

	}
}




void FileSelectorWidget::preselect_file_full_path(const QString & filename)
{
	this->current_file_full_path = filename;
	if (this->file_dialog) {
		this->file_dialog->selectFile(this->current_file_full_path);
	}
	this->line->insert(this->current_file_full_path);
}




QString FileSelectorWidget::get_selected_file_full_path(void) const
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




QStringList FileSelectorWidget::get_selected_files_full_paths(void) const
{
	return this->file_dialog->selectedFiles();
}




QUrl FileSelectorWidget::get_directory_url(void) const
{
	return this->file_dialog->directoryUrl();
}




void FileSelectorWidget::set_directory_url(const QUrl & dir_url)
{
	this->file_dialog->setDirectoryUrl(dir_url);
}




void FileSelectorWidget::set_name_filters(const QStringList & name_filters)
{
	this->file_dialog->setNameFilters(name_filters);
}




void FileSelectorWidget::select_name_filter(const QString & name_filter)
{
	this->file_dialog->selectNameFilter(name_filter);
}




QString FileSelectorWidget::get_selected_name_filter(void) const
{
	return this->file_dialog->selectedNameFilter();
}




void FileSelectorWidget::set_accept_mode(QFileDialog::AcceptMode accept_mode)
{
	this->file_dialog->setAcceptMode(accept_mode);
}
