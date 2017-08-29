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




/* TODO: add copying of text edited in QLineEdit into this->selector_filename and this->file_selector. */




SGFileEntry::SGFileEntry(enum QFileDialog::Option options, enum QFileDialog::FileMode mode, const QString & title, QWidget * parent_widget) : QWidget(parent_widget)
{
	this->file_selector = new QFileDialog();
	this->file_selector->setFileMode(mode);
	this->file_selector->setOptions(options);
	this->file_selector->setWindowTitle(title);


	this->line = new QLineEdit(this);
	this->browse = new QPushButton("Browse", this);


	this->hbox = new QHBoxLayout;
	this->hbox->addWidget(this->line);
	this->hbox->addWidget(this->browse);


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




void SGFileEntry::open_browser_cb(void) /* Slot. */
{

	if (this->file_selector->exec()) {
		this->selector_filename = this->file_selector->selectedFiles().at(0);
		this->line->insert(this->selector_filename);
		qDebug() << "II: Widget File Entry: clicking OK results in this file:" << this->selector_filename;

	}
}




void SGFileEntry::set_filename(QString & filename)
{
	this->selector_filename = filename;
	if (this->file_selector) {
		this->file_selector->selectFile(this->selector_filename);
	}
	this->line->insert(this->selector_filename);
}




QString SGFileEntry::get_filename(void)
{
	QStringList selection = this->file_selector->selectedFiles();
	static QString empty("");

	if (selection.size()) {
		qDebug() << "II: Widget File Entry: will return" << selection.at(0) << "to caller";
		return selection.at(0);
	} else {
		qDebug() << "II: Widget File Entry: will return empty string";
		return empty;
	}
}
