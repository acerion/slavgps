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




#include <QDebug>




#include "widget_file_entry.h"
#include "globals.h"




using namespace SlavGPS;




#define SG_MODULE "Widget File Entry"



/*
  https://www.qtcentre.org/threads/49434-QFileDialog-set-default-name
  QFileDialog::selectFile(const QString &) only works with Qt's non-native file selection dialog.
*/
#define SG_SUPPORT_SELECT_FILE 1




FileSelectorWidget::FileSelectorWidget(enum QFileDialog::Option options, enum QFileDialog::FileMode mode, const QString & title, QWidget * parent) : QWidget(parent)
{
	this->file_dialog = new QFileDialog();
	this->file_dialog->setFileMode(mode);
	this->file_dialog->setOptions(options);
	this->file_dialog->setWindowTitle(title);
#if SG_SUPPORT_SELECT_FILE
	this->file_dialog->setOptions(QFileDialog::DontUseNativeDialog);
#endif

	this->line = new QLineEdit(this);
	this->browse_button = new QPushButton(tr("Browse"), this);


	this->hbox = new QHBoxLayout;
	this->hbox->addWidget(this->line);
	this->hbox->addWidget(this->browse_button);
	this->hbox->setContentsMargins(0, 0, 0, 0);


	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->hbox);

	connect(this->browse_button, SIGNAL(clicked()), this, SLOT(open_browser_cb()));
	connect(this->line, SIGNAL (textEdited(const QString&)), this, SLOT (handle_user_edit_in_input_line_cb(void))); /* textEdited() - text edited only manually, not programmatically. */

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
		const QString selected_full_path = this->file_dialog->selectedFiles().at(0);
		this->line->setText(selected_full_path);
		qDebug() << SG_PREFIX_I << "Clicking OK results in this file:" << selected_full_path;
		emit this->selection_is_made();
	}
}




void FileSelectorWidget::handle_user_edit_in_input_line_cb(void)
{
	const QString new_text = this->line->text();
	qDebug() << SG_PREFIX_SLOT << "Handling new text edited by user:" << new_text;

	if (this->file_dialog) {
		this->file_dialog->selectFile(new_text);
	} else {
		qDebug() << SG_PREFIX_W << "No file dialog at this point!";
	}
}




void FileSelectorWidget::preselect_file_full_path(const QString & full_path)
{
	qDebug() << SG_PREFIX_SLOT << "Preselecting path" << full_path;
	if (this->file_dialog) {
		this->file_dialog->selectFile(full_path);
	} else {
		qDebug() << SG_PREFIX_W << "No file dialog at this point!";
	}
	this->line->setText(full_path);
}




QString FileSelectorWidget::get_selected_file_full_path(void) const
{
	const QString result = this->line->text();
	if (result.size()) {
		qDebug() << SG_PREFIX_I << "Will return" << result << "to caller";
	} else {
		qDebug() << SG_PREFIX_I << "Will return empty string";
	}
	return result;
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




void FileSelectorWidget::clear_widget(void)
{
	this->line->setText("");

}




void FileSelectorWidget::set_enabled(bool enabled)
{
	this->line->setEnabled(enabled);
	this->browse_button->setEnabled(enabled);
}
