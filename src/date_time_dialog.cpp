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
 */




#include <cstdlib>




#include <QDebug>
#include <QMenu>
#include <QString>




#include "date_time_dialog.h"
#include "clipboard.h"
#include "vikutils.h"
#include "layer.h"
#include "globals.h"




using namespace SlavGPS;




#define SG_MODULE "DateTime Dialog"




QDate SGDateTimeDialog::date_dialog(QString const & title, const QDate & initial_date, QWidget * parent)
{
	QDate result;

	SGDateTimeDialog * dialog = new SGDateTimeDialog(QDateTime(initial_date), false, parent);
	dialog->setWindowTitle(title);

	if (QDialog::Accepted == dialog->exec()) {
		const QDateTime selected = dialog->get_date_time();
		result = selected.date();
		qDebug() << SG_PREFIX_I  << "Accepted, returning date" << result;
	} else {
		result = QDate(); /* Invalid date. */
		qDebug() << SG_PREFIX_I << "Cancelled";
	}

	return result;
}




SGDateTimeDialog::SGDateTimeDialog(QDateTime const & date_time, bool show_clock, QWidget * parent_widget) : QDialog(parent_widget)
{
	this->vbox = new QVBoxLayout;
	this->calendar = new QCalendarWidget(this);
	if (show_clock) {
		this->clock = new QTimeEdit(this);
		this->clock->setDisplayFormat(QString("h:mm:ss t"));
	}
	this->set_date_time(date_time);

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




Time SGDateTimeDialog::get_timestamp(void) const
{
	QDateTime date_time;
	date_time.setDate(this->calendar->selectedDate());
	qDebug() << SG_PREFIX_D << "Extracted date timestamp:" << date_time << date_time.toTime_t();

	if (this->clock) {
		date_time.setTime(this->clock->time());
	}
	qDebug() << SG_PREFIX_D << "Extracted combined timestamp:" << date_time << date_time.toTime_t();

	return Time(date_time.toTime_t(), TimeType::Unit::internal_unit());
}




QDateTime SGDateTimeDialog::get_date_time(void) const
{
	QDateTime date_time;
	date_time.setDate(this->calendar->selectedDate());
	qDebug() << SG_PREFIX_D << "Extracted date:" << date_time;

	if (this->clock) {
		date_time.setTime(this->clock->time());
	}
	qDebug() << SG_PREFIX_D << "Extracted combined date+time:" << date_time;

	return date_time;
}




void SGDateTimeDialog::set_date_time(const QDateTime & date_time)
{
	this->calendar->setSelectedDate(date_time.date());
	if (this->clock) {
		this->clock->setTime(date_time.time());
	}
}




/* Use constructor delegation to pass invalid Time value to other constructor. */
SGDateTimeButton::SGDateTimeButton(QWidget * parent_widget) : SGDateTimeButton(Time(), parent_widget)
{
}




SGDateTimeButton::SGDateTimeButton(const Time & date_time, QWidget * parent_widget) : QPushButton(parent_widget)
{
	if (date_time.is_valid()) {
		this->timestamp = date_time;
	} else {
		this->timestamp = Time(0, TimeType::Unit::internal_unit()); /* Initialize with default, valid value. */

		this->setIcon(QIcon::fromTheme("list-add"));
		this->setText("");
	}

	this->dialog = new SGDateTimeDialog(QDateTime::fromTime_t(this->timestamp.ll_value()), true, parent_widget);
	this->dialog->setWindowTitle(tr("Edit Date/Time"));

	connect(this, SIGNAL (released(void)), this, SLOT (open_dialog_cb(void)));
}




SGDateTimeButton::~SGDateTimeButton()
{

}




void SGDateTimeButton::open_dialog_cb(void) /* Slot. */
{
	qDebug() << SG_PREFIX_SLOT << "Called";

	/* Make sure that the dialog shows the correct date/time - the
	   value that was last retrieved from the date time dialog. */
	dialog->set_date_time(QDateTime::fromTime_t(this->timestamp.ll_value()));

	if (QDialog::Accepted == dialog->exec()) {
		this->timestamp = this->dialog->get_timestamp();
		this->set_label(this->timestamp, this->coord);
		qDebug() << SG_PREFIX_I << "Timestamp selected in dialog =" << this->timestamp;

		qDebug() << SG_PREFIX_SIGNAL << "Will emit 'value_is_set' signal for timestamp =" << this->timestamp;
		emit this->value_is_set(this->timestamp);
	} else {
		qDebug() << SG_PREFIX_I << "Returning zero timestamp";
		this->timestamp = Time(0, TimeType::Unit::internal_unit());
	}
}




Time SGDateTimeButton::get_value(void)
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
#if 0
	Pickle dummy;
	Clipboard::copy(ClipboardDataType::Text, LayerKind::Aggregate, SGObjectTypeID::any(), dummy, this->text());
#endif
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




void SGDateTimeButton::set_label(const Time & value, const Coord & new_coord)
{
	const QString msg = value.get_time_string(this->date_time_format, new_coord);

	this->setIcon(QIcon()); /* Invalid/null button icon will indicate that a timestamp is set, and is displayed as button label. */
	this->setText(msg);
}




void SGDateTimeButton::clear_label(void)
{
	this->setText("");
	this->setIcon(QIcon::fromTheme("list-add")); /* Non-empty/non-null button icon will indicate that no timestamp is set. */
}




void SGDateTimeButton::set_coord(const Coord & new_coord)
{
	this->coord = new_coord;
	this->set_label(this->timestamp, this->coord);
}
