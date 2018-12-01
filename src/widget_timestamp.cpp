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




#include <QLabel>
#include <QDebug>




#include <cstdint>




#include "widget_timestamp.h"
#include "globals.h"




using namespace SlavGPS;




#define SG_MODULE "Timestamp Widget"
#define PREFIX " Widget Timestamp: "




TimestampWidget::TimestampWidget(QWidget * parent)
{
	this->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);

	this->grid = new QGridLayout();

	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->grid);

	this->timestamp_entry = new QSpinBox(this);
	this->timestamp_entry->setMinimum(0);
	this->timestamp_entry->setMaximum(INT32_MAX); /* TODO_MAYBE: Adjust this limit to support 64-bit integers. */
	this->timestamp_entry->setSingleStep(1);
	this->grid->addWidget(new QLabel(tr("Raw Timestamp:")), 0, 0);
	this->grid->addWidget(this->timestamp_entry, 0, 1);
	connect(this->timestamp_entry, SIGNAL (valueChanged(int)), this, SLOT (on_timestamp_entry_value_set_cb(void)));


	this->timestamp_button = new SGDateTimeButton(this);
	this->grid->addWidget(new QLabel(tr("Formatted Time:")), 1, 0);
	this->grid->addWidget(this->timestamp_button, 1, 1);
	connect(this->timestamp_button, SIGNAL (value_is_set(time_t)), this, SLOT (on_timestamp_button_value_set_cb(void)));
	connect(this->timestamp_button, SIGNAL (value_is_reset(void)), this, SLOT (on_timestamp_button_value_reset_cb(void)));
}




void TimestampWidget::set_timestamp(const Time & timestamp, const Coord & new_coord)
{
	if (!timestamp.is_valid()) {
		qDebug() << SG_PREFIX_E << "Trying to set invalid timestamp";
		return;
	}

	this->timestamp_entry->setValue(timestamp.get_value());
	this->timestamp_button->set_label(timestamp, new_coord);
}




void TimestampWidget::reset_timestamp(void)
{
	this->timestamp_entry->setValue(0);
	this->timestamp_button->clear_label();
}




Time TimestampWidget::get_timestamp(void) const
{
	return Time((time_t) this->timestamp_entry->value());
}




void TimestampWidget::on_timestamp_entry_value_set_cb(void)
{
	const time_t new_value = (time_t) this->timestamp_entry->value();
	qDebug() << SG_PREFIX_SLOT << "New value of timestamp =" << new_value;

	qDebug() << SG_PREFIX_SIGNAL << "Timestamp value in entry field changed to" << new_value << ", emitting signal 'TimestampWidget::value_is_set()";
	emit this->value_is_set(new_value);

	this->timestamp_button->set_label(new_value, this->coord);
}




void TimestampWidget::on_timestamp_button_value_set_cb(void)
{
	const Time new_value = this->timestamp_button->get_value();
	qDebug() << SG_PREFIX_SLOT << "New value =" << new_value;
	this->timestamp_entry->setValue(new_value.get_value());
}




void TimestampWidget::on_timestamp_button_value_reset_cb(void)
{
	qDebug() << SG_PREFIX_SLOT;
	this->clear();
}




void TimestampWidget::clear(void)
{
	this->timestamp_entry->setValue(0);
	this->timestamp_button->clear_label();
}




void TimestampWidget::set_coord(const Coord & new_coord)
{
	this->coord = new_coord;

	const time_t current_value = (time_t) this->timestamp_entry->value();

	this->timestamp_button->set_label(current_value, this->coord);
}
