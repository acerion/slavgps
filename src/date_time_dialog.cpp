/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
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
 *
 */
#include <cstdlib>

#include <QDebug>

#include "date_time_dialog.h"




using namespace SlavGPS;




/**
   @title: title to use for the dialog
   @initial_timestamp: the initial date/time to be shown in dialog
   @result_timestamp: timestamp entered in dialog if user accepted dialog; not changed if user cancelled the dialog;
   @parent: the parent widget

   @return true if user accepted the dialog (pressed OK/Enter); @result_timestamp is then modified
   @return false if user cancelled the dialog (pressed Cancel/Escape); @result_timestamp is then not modified
*/
bool SlavGPS::date_time_dialog(QString const & title, time_t initial_timestamp, time_t & result_timestamp, QWidget * parent)
{
	SGDateTimeDialog * dialog = new SGDateTimeDialog(parent, QDateTime::fromTime_t(initial_timestamp));
	dialog->setWindowTitle(title);

	if (QDialog::Accepted == dialog->exec()) {
		result_timestamp = dialog->get_timestamp();
		qDebug() << "II Date Time Dialog: accepted, returning timestamp" << result_timestamp;
		return true;
	} else {
		qDebug() << "II Date Time Dialog: cancelled";
		return false;
	}
}



bool SlavGPS::date_dialog(QString const & title, time_t initial_timestamp, time_t & result_timestamp, QWidget * parent)
{
	SGDateTimeDialog * dialog = new SGDateTimeDialog(parent, QDateTime::fromTime_t(initial_timestamp));
	dialog->setWindowTitle(title);
	dialog->show_clock(false);

	if (QDialog::Accepted == dialog->exec()) {
		result_timestamp = dialog->get_timestamp();
		qDebug() << "II Date Dialog: accepted, returning timestamp" << result_timestamp;
		return true;
	} else {
		qDebug() << "II Date Dialog: cancelled";
		return false;
	}
}




SGDateTimeDialog::SGDateTimeDialog(QWidget * parent_widget, QDateTime const & date_time) : QDialog(parent_widget)
{
	this->vbox = new QVBoxLayout;
	this->calendar = new QCalendarWidget(this);
	this->calendar->setSelectedDate(date_time.date());
	this->clock = new QTimeEdit(this);
	this->clock->setTime(date_time.time());
	this->clock->setDisplayFormat(QString("h:mm:ss t"));

	this->button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(this->button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(this->button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);


	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox);

	this->vbox->addWidget(this->calendar);
	this->vbox->addWidget(this->clock);
	this->vbox->addWidget(this->button_box);
}




SGDateTimeDialog::~SGDateTimeDialog()
{
}




time_t SGDateTimeDialog::get_timestamp()
{
	QDateTime date_time;
	date_time.setDate(this->calendar->selectedDate());
	if (this->clock->isVisible()) {
		date_time.setTime(this->clock->time());
	}

	return date_time.toTime_t();
}




void SGDateTimeDialog::show_clock(bool do_show)
{
	this->clock->setVisible(do_show);
}





SGDateTime::SGDateTime(time_t date_time, QWidget * parent_widget) : QPushButton(parent_widget)
{
	this->dialog = new SGDateTimeDialog(parent_widget, QDateTime::fromTime_t(date_time));
	this->dialog->setWindowTitle(QString("Edit Date/Time"));
	connect(this, SIGNAL (released(void)), this, SLOT (open_dialog_cb(void)));
}




SGDateTime::~SGDateTime()
{

}




void SGDateTime::open_dialog_cb(void) /* Slot. */
{
	int dialog_code = dialog->exec();

	if (dialog_code == QDialog::Accepted) {
		this->timestamp = this->dialog->get_timestamp();
		qDebug() << "II DateTime: returning timestamp" << this->timestamp;
	} else {
		qDebug() << "II DateTime: returning zero";
		this->timestamp = 0;
	}
}




time_t SGDateTime::value(void)
{
	return this->timestamp;
}
