/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2008, Hein Ragas <viking@ragas.nl>
 * Copyright (C) 2010-2014, Rob Norris <rw_norris@hotmail.com>
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

#include <QDialog>
#include <QInputDialog>
#include <QDebug>

#include "dialog.h"
#include "ui_util.h"
#include "date_time_dialog.h"
#include "degrees_converters.h"
#include "globals.h"




using namespace SlavGPS;




void Dialog::info(QString const & message, QWidget * parent)
{
	QMessageBox box(parent);
	box.setText(message);
	box.setIcon(QMessageBox::Information);
	box.exec();
	return;
}




void Dialog::warning(QString const & message, QWidget * parent)
{
	QMessageBox box(parent);
	box.setText(message);
	box.setIcon(QMessageBox::Warning);
	box.exec();
	return;
}




void Dialog::error(QString const & message, QWidget * parent)
{
	QMessageBox box(parent);
	box.setText(message);
	box.setIcon(QMessageBox::Critical);
	box.exec();
	return;
}




/**
 * Returns: a date as a string - always in ISO8601 format (YYYY-MM-DD).
 * This string can be NULL (especially when the dialog is cancelled).
 * Free the string after use.
 */
char * a_dialog_get_date(const QString & title, QWidget * parent)
{
	time_t new_timestamp;
	if (!date_dialog(title, time(NULL), new_timestamp, parent)) {
		return NULL;
	}

	struct tm * out = localtime(&new_timestamp);
	size_t size = strlen("YYYY-MM-DD") + 1;
	char * date_str = (char *) malloc(size);
	snprintf(date_str, size, "%04d-%02d-%02d", 1900 + out->tm_year, out->tm_mon + 1, out->tm_mday);
	qDebug() << "II: Dialog: get date:" << date_str;

	return date_str;
}




bool Dialog::yes_or_no(QString const & message, QWidget * parent, QString const & title)
{
	return QMessageBox::Yes == QMessageBox::question(parent, title, message);
}




/**
 * Dialog to return an integer via a spinbox within the supplied limits.
 *
 * Use \param ok to recognize when a dialog was cancelled.
 */
int Dialog::get_int(const QString & title, const QString & label, int default_num, int min, int max, int step, bool * ok, QWidget * parent)
{
	int result = QInputDialog::getInt(parent, title, label, default_num, min, max, step, ok);

	return result;
}




/**
   Creates a dialog with list of text.
   Mostly useful for longer messages that have several lines of information.
*/
void a_dialog_list(const QString & title, const QStringList & items, int padding, QWidget * parent)
{
	QMessageBox box(parent);
	//box.setTitle(title);
	box.setIcon(QMessageBox::Information);

	QVBoxLayout vbox;
	QLayout * old = box.layout();
	delete old;
	box.setLayout(&vbox);

	for (int i = 0; i < items.size(); i++) {
		QLabel * label = ui_label_new_selectable(items.at(i), &box);
		vbox.addWidget(label);
		// gtk_box_pack_start(GTK_BOX(vbox), label, false, true, padding);
	}

	box.exec();
}



/**
 * Display a dialog presenting the license of a map.
 * Allow to read the license by launching a web browser.
 */
void Dialog::license(const char * map, const char * license, const char * url, QWidget * parent)
{
	const QString primary_text = QString(QObject::tr("The map data is licensed: %1.")).arg(license);
	const QString secondary_text = QString(QObject::tr("The data provided by '<b>%1</b>' are licensed under the following license: <b>%1</b>."))
		.arg(map)
		.arg(license);

	QMessageBox box;
	box.setText(primary_text);
	box.setInformativeText(secondary_text);
	box.setStandardButtons(QMessageBox::Ok);

	if (url != NULL) {
		box.addButton(QObject::tr("Open license"), QMessageBox::HelpRole);
	}
	int response;
	do {
		response = box.exec();
		if (response == QMessageBox::Help) {
			open_url(url);
		}
	} while (response != QMessageBox::Cancel && response != QMessageBox::Ok);
}




BasicDialog::BasicDialog(QWidget * parent)
{
	this->vbox = new QVBoxLayout;
	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox);


	this->grid = new QGridLayout();
	this->vbox->addLayout(this->grid);


	this->button_box = new QDialogButtonBox();
	this->button_box->addButton(QDialogButtonBox::Ok);
	this->button_box->addButton(QDialogButtonBox::Cancel);
	connect(this->button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(this->button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
	this->vbox->addWidget(this->button_box);
}




BasicDialog::~BasicDialog()
{
}




BasicMessage::BasicMessage(QWidget * parent)
{
	this->vbox = new QVBoxLayout;
	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox);


	this->grid = new QGridLayout();
	this->vbox->addLayout(this->grid);


	this->button_box = new QDialogButtonBox();
	this->button_box->addButton(QDialogButtonBox::Ok);
	connect(this->button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
	this->vbox->addWidget(this->button_box);
}




BasicMessage::~BasicMessage()
{
}
