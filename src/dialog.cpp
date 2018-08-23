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
#include "viewport_internal.h"




using namespace SlavGPS;




#define PREFIX ": Dialog:" << __FUNCTION__ << __LINE__ << ">"




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
bool Dialog::get_date(const QString & title, char * buffer, size_t buffer_size, QWidget * parent)
{
	time_t new_timestamp;
	if (!date_dialog(title, time(NULL), new_timestamp, parent)) {
		return false;
	}

	struct tm * out = localtime(&new_timestamp);
	snprintf(buffer, buffer_size, "%04d-%02d-%02d", 1900 + out->tm_year, out->tm_mon + 1, out->tm_mday);

	qDebug() << "II" PREFIX << "entered date:" << buffer;

	return true;
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
	}

	box.exec();
}



/**
 * Display a dialog presenting the license of a map.
 * Allow to read the license by launching a web browser.
 */
void Dialog::map_license(const QString & map_name, const QString & map_license, const QString & map_license_url, QWidget * parent)
{
	const QString primary_text = QString(QObject::tr("The map data is licensed: %1.")).arg(map_license);
	const QString secondary_text = QString(QObject::tr("The data provided by '<b>%1</b>' are licensed under the following license: <b>%1</b>."))
		.arg(map_name)
		.arg(map_license);

	QMessageBox box;
	box.setText(primary_text);
	box.setInformativeText(secondary_text);
	box.setStandardButtons(QMessageBox::Ok);

	if (!map_license_url.isEmpty()) {
		box.addButton(QObject::tr("Open license"), QMessageBox::HelpRole);
	}
	int response;
	do {
		response = box.exec();
		if (response == QMessageBox::Help) {
			open_url(map_license_url);
		}
	} while (response != QMessageBox::Cancel && response != QMessageBox::Ok);
}




/**
   @brief Move a dialog to expose given coordinate

   Try to reposition a dialog if it's over the specified coord so to
   not obscure the item of interest.

   TODO_REALLY: the function should accept something else than a Coord variable. Perhaps a screen number and a pixel coordinates.

   @dialog - dialog to move
   @viewport - viewport in which the coordinate exists
   @exposed_coord - coordinate to be exposed by movement of the dialog
   @move_vertically: The reposition strategy. move_vertically==true moves dialog vertically, otherwise moves it horizontally
*/
void Dialog::move_dialog(QDialog * dialog, Viewport * viewport, const Coord & exposed_coord, bool move_vertically)
{
	/* http://doc.qt.io/qt-5/application-windows.html#window-geometry */

	/* TODO_LATER: there is one more improvement to be made: take into
	   consideration that available geometry (as returned by
	   ::availableGeometry()) may be smaller than geometry of full
	   screen. */

	const int dialog_width = dialog->frameGeometry().width(); /* Including window frame. */
	const int dialog_height = dialog->frameGeometry().height(); /* Including window frame. */
	const QPoint dialog_pos = dialog->pos();


	/* Dialog not 'realized'/positioned - so can't really do any repositioning logic. */
	if (dialog_width <= 2 || dialog_height <= 2) {
		qDebug() << "WW: Layer TRW: can't position dialog window";
		return;
	}


	const ScreenPos exposed_pos = viewport->coord_to_screen_pos(exposed_coord); /* In viewport's x/y coordinate system. */
	const QPoint global_point_pos = viewport->mapToGlobal(QPoint(exposed_pos.x, exposed_pos.y)); /* In screen's x/y coordinate system. */


#if 0   /* Debug. */
	const int primary_screen = QApplication::desktop()->primaryScreen();
	const QRect primary_screen_geo = QApplication::desktop()->availableGeometry(primary_screen);
	const QRect containing_screen_geo = QApplication::desktop()->availableGeometry(viewport);

	qDebug() << "DD" PREFIX "primary screen:" << primary_screen << "dialog begin:" << dialog_pos << "coord pos:" << global_point_pos;
	qDebug() << "DD" PREFIX "available geometry of primary screen:" << primary_screen_geo;
	qDebug() << "DD" PREFIX "available geometry of screen containing widget:" << containing_screen_geo;
#endif


	if (global_point_pos.x() < dialog_pos.x()) {
		/* Point visible, on left side of dialog. */
		qDebug() << "DD" PREFIX "point visible on left";
		return;
	}
	if (global_point_pos.y() < dialog_pos.y()) {
		/* Point visible, above dialog. */
		qDebug() << "DD" PREFIX "point visible above";
		return;
	}
	if (global_point_pos.x() > (dialog_pos.x() + dialog_width)) {
		/* Point visible, on right side of dialog. */
		qDebug() << "DD" PREFIX "point visible on right";
		return;
	}
	if (global_point_pos.y() > (dialog_pos.y() + dialog_height)) {
		/* Point visible, below dialog. */
		qDebug() << "DD" PREFIX "point visible below";
		return;
	}


	QPoint new_position;
	if (move_vertically) {
		/* Move dialog up or down. */
		if (global_point_pos.y() > dialog_height + 10) {
			/* Move above given screen position. */
			qDebug() << "DD" PREFIX "move up";
			new_position = QPoint(dialog_pos.x(), global_point_pos.y() - dialog_height - 10);
		} else {
			/* Move below given screen position. */
			qDebug() << "DD" PREFIX "move down";
			new_position = QPoint(dialog_pos.x(), global_point_pos.y() + 10);
		}
	} else {
		/* Move dialog left or right. */
		if (global_point_pos.x() > dialog_width + 10) {
			/* Move to the left of given screen position. */
			qDebug() << "DD" PREFIX "move to the left";
			new_position = QPoint(global_point_pos.x() - dialog_width - 10, dialog_pos.y());
		} else {
			/* Move to the right of given screen position. */
			qDebug() << "DD" PREFIX "move to the right";
			new_position = QPoint(global_point_pos.x() + 10, dialog_pos.y());
		}
	}
	dialog->move(new_position);

	return;
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
