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
#include <QMenu>
#include <QString>

#include "date_time_dialog.h"
#include "clipboard.h"
#include "vikutils.h"
#include "layer.h"




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
	SGDateTimeDialog * dialog = new SGDateTimeDialog(QDateTime::fromTime_t(initial_timestamp), true, parent);
	dialog->setWindowTitle(title);

	if (QDialog::Accepted == dialog->exec()) {
		result_timestamp = dialog->get_timestamp();
		qDebug() << "II: Date Time Dialog: accepted, returning timestamp" << result_timestamp;
		return true;
	} else {
		qDebug() << "II: Date Time Dialog: cancelled";
		return false;
	}
}



bool SlavGPS::date_dialog(QString const & title, time_t initial_timestamp, time_t & result_timestamp, QWidget * parent)
{
	SGDateTimeDialog * dialog = new SGDateTimeDialog(QDateTime::fromTime_t(initial_timestamp), false, parent);
	dialog->setWindowTitle(title);

	if (QDialog::Accepted == dialog->exec()) {
		result_timestamp = dialog->get_timestamp();
		qDebug() << "II: Date Dialog: accepted, returning timestamp" << result_timestamp;
		return true;
	} else {
		qDebug() << "II: Date Dialog: cancelled";
		return false;
	}
}




SGDateTimeDialog::SGDateTimeDialog(QDateTime const & date_time, bool show_clock, QWidget * parent_widget) : QDialog(parent_widget)
{
	this->vbox = new QVBoxLayout;
	this->calendar = new QCalendarWidget(this);
	this->calendar->setSelectedDate(date_time.date());
	if (show_clock) {
		this->clock = new QTimeEdit(this);
		this->clock->setTime(date_time.time());
		this->clock->setDisplayFormat(QString("h:mm:ss t"));
	}

	this->button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(this->button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(this->button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);


	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox);

	this->vbox->addWidget(this->calendar);
	if (this->clock) {
		this->vbox->addWidget(this->clock);
	}
	this->vbox->addWidget(this->button_box);
}




SGDateTimeDialog::~SGDateTimeDialog()
{
}




time_t SGDateTimeDialog::get_timestamp()
{
	QDateTime date_time;
	date_time.setDate(this->calendar->selectedDate());
	if (this->clock) {
		date_time.setTime(this->clock->time());
	}

	qDebug() << "DD: Date Time Dialog: get timestamp:" << date_time << date_time.toTime_t();

	return date_time.toTime_t();
}




SGDateTimeButton::SGDateTimeButton(QWidget * parent_widget) : QPushButton(parent_widget)
{
	this->setIcon(QIcon::fromTheme("list-add"));
	this->setText("");

	this->dialog = new SGDateTimeDialog(QDateTime::fromTime_t(0), true, parent_widget);
	this->dialog->setWindowTitle(tr("Edit Date/Time"));
	connect(this, SIGNAL (released(void)), this, SLOT (open_dialog_cb(void)));
}





SGDateTimeButton::SGDateTimeButton(time_t date_time, QWidget * parent_widget) : QPushButton(parent_widget)
{
	this->dialog = new SGDateTimeDialog(QDateTime::fromTime_t(date_time), true, parent_widget);
	this->dialog->setWindowTitle(tr("Edit Date/Time"));
	connect(this, SIGNAL (released(void)), this, SLOT (open_dialog_cb(void)));
}




SGDateTimeButton::~SGDateTimeButton()
{

}




void SGDateTimeButton::open_dialog_cb(void) /* Slot. */
{
	qDebug() << "SLOT: Date Time Dialog: 'Open Dialog' slot";

	int dialog_code = dialog->exec();

	if (dialog_code == QDialog::Accepted) {
		this->timestamp = this->dialog->get_timestamp();
		qDebug() << "II: DateTime: returning timestamp" << this->timestamp;

		qDebug() << "SIGNAL: Date Time Dialog: will emit 'Value is Set' signal" << this->timestamp;
		emit this->value_is_set(this->timestamp);
	} else {
		qDebug() << "II: DateTime: returning zero";
		this->timestamp = 0;
	}
}




time_t SGDateTimeButton::get_value(void)
{
	return this->timestamp;
}




void SGDateTimeButton::mousePressEvent(QMouseEvent * ev)
{
	switch (ev->button()) {
	case Qt::RightButton: {

		QMenu menu;
		QAction * qa = NULL;
		/* If button's icon has been replaced with text representing a time, we can copy or clear (reset) the time. */
		const bool has_timestamp = this->icon().isNull();


		qa = new QAction(QObject::tr("&Copy formatted time string"), &menu);
		qa->setEnabled(has_timestamp);
		QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (copy_formatted_time_string_cb(void)));
		menu.addAction(qa);


		qa = new QAction(QObject::tr("Clea&r time"), &menu);
		qa->setEnabled(has_timestamp);
		QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (clear_time_cb(void)));
		menu.addAction(qa);

		menu.exec(QCursor::pos());
		}
		break;

	case Qt::LeftButton:
		this->open_dialog_cb();
		break;
	default:
		break;
	}
}




void SGDateTimeButton::copy_formatted_time_string_cb(void)
{
	qDebug() << "SLOT: Date Time Button: copy formatted time string";

	Pickle dummy;
	Clipboard::copy(ClipboardDataType::TEXT, LayerType::AGGREGATE, "", dummy, this->text());
}




void SGDateTimeButton::clear_time_cb(void)
{
	qDebug() << "SLOT: Date Time Button: clear time";

	this->clear_label();

	QDateTime beginning;
	beginning.setMSecsSinceEpoch(0); /* Zero time. */
	this->dialog->calendar->setSelectedDate(beginning.date());
	if (this->dialog->clock) {
		this->dialog->clock->setTime(beginning.time());
	}

	/* Inform client code that uses the button that "clear time"
	   has been selected from context menu for the button, and
	   that calendar and clock have been reset. */
	qDebug() << "SIGNAL: Date Time Button: will emit 'Value is Reset' signal";
	emit this->value_is_reset();
}




/* TODO: perhaps "format" should be a member of this class, passed to constructor. */
void SGDateTimeButton::set_label(time_t timestamp_value, Qt::DateFormat format, const Coord & coord, const QTimeZone * tz)
{
	const QString msg = SGUtils::get_time_string(timestamp_value, format, coord, tz);

	this->setText(msg);
	this->setIcon(QIcon()); /* Invalid/null button icon will indicate that a timestamp is set, and is displayed as button label. */
}



void SGDateTimeButton::clear_label(void)
{
	this->setText("");
	this->setIcon(QIcon::fromTheme("list-add")); /* Non-empty/non-null button icon will indicate that no timestamp is set. */
}
