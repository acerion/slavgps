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
 * @parent:     The parent window
 * @title:      The title to use for the dialog
 * @date_time:  The inital date/time to be shown
 *
 * Returns: A time selected by the user via this dialog.
 *          Even though a time of zero is notionally valid - consider it unlikely to be actually wanted!
 *          Thus if the time is zero then the dialog was cancelled or somehow an invalid date was encountered.
 */
time_t SlavGPS::date_time_dialog(QWidget * parent, QString const & title, time_t date_time)
{
	SGDateTimeDialog * dialog = new SGDateTimeDialog(parent, QDateTime::fromTime_t(date_time));
	dialog->setWindowTitle(title);
	int dialog_code = dialog->exec();

	time_t ret = date_time;

	if (dialog_code == QDialog::Accepted) {
		ret = dialog->get_timestamp();
		qDebug() << "II DateTime: returning timestamp" << ret;
		return ret;
	} else {
		qDebug() << "II DateTime: returning zero";
		return 0;
	}
}




SGDateTimeDialog::SGDateTimeDialog(QWidget * parent, QDateTime const & date_time) : QDialog(parent)
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
	date_time.setTime(this->clock->time());

	return date_time.toTime_t();
}




SGDateTime::SGDateTime(time_t date_time, QWidget * parent) : QPushButton(parent)
{
	this->dialog = new SGDateTimeDialog(parent, QDateTime::fromTime_t(date_time));
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
