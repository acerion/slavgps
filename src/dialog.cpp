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




#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>




#include <QDialog>
#include <QInputDialog>
#include <QDebug>
#include <QDesktopWidget>
#include <QThread>




#include "dialog.h"
#include "ui_util.h"
#include "date_time_dialog.h"
#include "lat_lon.h"
#include "viewport_internal.h"




using namespace SlavGPS;




#define SG_MODULE "Dialog"




void Dialog::info(QString const & message, QWidget * parent)
{
	QMessageBox box(parent);
	box.setText(message);
	box.setIcon(QMessageBox::Information);
	box.exec();
	return;
}



void Dialog::info(const QString & header, const QStringList & message, QWidget * parent)
{
	QMessageBox box(parent);
	box.setTextFormat(Qt::RichText);

	const QString formatted_message = QString("<big>%1</big>"
						  "<br/><br/>"
						  "%2").arg(header).arg(message.join("<br/>"));
	box.setText(formatted_message);

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
 * Display a dialog presenting the license of a map.
 * Allow to read the license by launching a web browser.
 */
void Dialog::map_license(const QString & map_name, const QString & map_license, const QString & map_license_url, QWidget * parent)
{
	const QString primary_text = QObject::tr("The map data is licensed: %1.").arg(map_license);
	const QString secondary_text = QObject::tr("The data provided by '<b>%1</b>' are licensed under the following license: <b>%1</b>.")
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

   @dialog - dialog to move
   @point_to_expose - x/y coordinates of point to be exposed by movement of the dialog
   @move_vertically: The reposition strategy. move_vertically==true moves dialog vertically, otherwise moves it horizontally
*/
void Dialog::move_dialog(QDialog * dialog, const ScreenPos & point_to_expose, bool move_vertically)
{
	/* http://doc.qt.io/qt-5/application-windows.html#window-geometry */

	/* TODO_LATER: there is one more improvement to be made: take into
	   consideration that available geometry (as returned by
	   ::availableGeometry()) may be smaller than geometry of full
	   screen. */

	const int dialog_width = dialog->frameGeometry().width(); /* Including window frame. */
	const int dialog_height = dialog->frameGeometry().height(); /* Including window frame. */
	const QPoint dialog_pos = dialog->pos();


	if (1) { /* Debug. */
		const int primary_screen = QApplication::desktop()->primaryScreen();
		qDebug() << SG_PREFIX_D << "Primary screen:" << primary_screen << "dialog begin:" << dialog_pos << "coord pos:" << point_to_expose;
	}


	/* Dialog not 'realized'/positioned - so can't really do any repositioning logic. */
	if (dialog_width <= 2 || dialog_height <= 2) {
		qDebug() << SG_PREFIX_W << "Can't re-position dialog window";
		return;
	}


	if (point_to_expose.x() < dialog_pos.x()) {
		/* Point visible, on left side of dialog. */
		qDebug() << SG_PREFIX_D << "Point visible on left";
		return;
	}
	if (point_to_expose.y() < dialog_pos.y()) {
		/* Point visible, above dialog. */
		qDebug() << SG_PREFIX_D << "Point visible above";
		return;
	}
	if (point_to_expose.x() > (dialog_pos.x() + dialog_width)) {
		/* Point visible, on right side of dialog. */
		qDebug() << SG_PREFIX_D << "Point visible on right";
		return;
	}
	if (point_to_expose.y() > (dialog_pos.y() + dialog_height)) {
		/* Point visible, below dialog. */
		qDebug() << SG_PREFIX_D << "Point visible below";
		return;
	}


	QPoint new_position;
	if (move_vertically) {
		/* Move dialog up or down. */
		if (point_to_expose.y() > dialog_height + 10) {
			/* Move above given screen position. */
			qDebug() << SG_PREFIX_D << "Move up";
			new_position = QPoint(dialog_pos.x(), point_to_expose.y() - dialog_height - 10);
		} else {
			/* Move below given screen position. */
			qDebug() << SG_PREFIX_D << "Move down";
			new_position = QPoint(dialog_pos.x(), point_to_expose.y() + 10);
		}
	} else {
		/* Move dialog left or right. */
		if (point_to_expose.x() > dialog_width + 10) {
			/* Move to the left of given screen position. */
			qDebug() << SG_PREFIX_D << "Move to the left";
			new_position = QPoint(point_to_expose.x() - dialog_width - 10, dialog_pos.y());
		} else {
			/* Move to the right of given screen position. */
			qDebug() << SG_PREFIX_D << "Move to the right";
			new_position = QPoint(point_to_expose.x() + 10, dialog_pos.y());
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

	connect(this, SIGNAL (set_central_widget(QWidget *)), this, SLOT (set_central_widget_cb(QWidget *)));
}




BasicDialog::~BasicDialog()
{
	qDebug() << SG_PREFIX_I;
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




BasicDialog::BasicDialog(const QString & title, QWidget * parent) : BasicDialog(parent)
{
	this->setWindowTitle(title);
}




void BasicDialog::set_central_widget_cb(QWidget * widget)
{
	qDebug() << SG_PREFIX_D;
	QLayoutItem * child;
	while ((child = this->grid->takeAt(0)) != 0) {
		delete child->widget();
		delete child;
	}

	qDebug() << SG_PREFIX_I << "Current thread    =" << QThread::currentThread();
	qDebug() << SG_PREFIX_I << "Main thread       =" << QApplication::instance()->thread();
	qDebug() << SG_PREFIX_I << "Widget thread     =" << widget->thread();
	qDebug() << SG_PREFIX_I << "this-> thread     =" << this->thread();
	qDebug() << SG_PREFIX_I << "this->grid thread =" << this->grid->thread();

	//widget->moveToThread(QApplication::instance()->thread());
	//this->moveToThread(QApplication::instance()->thread());
	//this->grid->moveToThread(QApplication::instance()->thread());
	this->grid->addWidget(widget, 0, 0);

	return;
}




BasicMessage::~BasicMessage()
{
}
