/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2019, Kamil Ignacak <acerion@wp.pl>
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




#include "widget_duration.h"




using namespace SlavGPS;




#define SG_MODULE "Widget Duration"




void DurationWidget::build_widget(const MeasurementScale<Duration, Time_ll, TimeUnit> & scale, QWidget * parent)
{
	this->m_hbox = new QHBoxLayout();

	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->m_hbox);


	this->m_widget = new QSpinBox(parent);
	this->m_widget->setMinimum(scale.m_min.get_ll_value());
	this->m_widget->setMaximum(scale.m_max.get_ll_value());
	this->m_widget->setSingleStep(scale.m_step.get_ll_value());
	this->m_widget->setValue(scale.m_initial.get_ll_value());
	switch (scale.m_unit) {
	case TimeUnit::Seconds:
		this->m_widget->setSuffix(tr("s"));
		this->m_widget->setToolTip(tr("Duration in seconds"));
		break;
	case TimeUnit::Minutes:
		this->m_widget->setSuffix(tr("m"));
		this->m_widget->setToolTip(tr("Duration in minutes"));
		break;
	case TimeUnit::Hours:
		this->m_widget->setSuffix(tr("h"));
		this->m_widget->setToolTip(tr("Duration in hours"));
		break;
	case TimeUnit::Days:
		this->m_widget->setSuffix(tr("d"));
		this->m_widget->setToolTip(tr("Duration in days"));
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled duration unit" << (int) scale.m_unit;
		break;
	}


	this->m_hbox->addWidget(this->m_widget);


	/* Ensure entry field has focus so we can start typing
	   straight away. */
	this->setFocusProxy(this->m_widget);

	this->m_hbox->setContentsMargins(0, 0, 0, 0);
}




/**
   @reviewed-on 2019-11-25
*/
sg_ret DurationWidget::set_value(const Duration & duration)
{
	if (duration.get_unit() != this->m_unit) {
		qDebug() << SG_PREFIX_E << "Unit mismatch: widget unit =" << (int) this->m_unit << ", new unit =" << (int) duration.get_unit();
		return sg_ret::err;
	}

	this->m_widget->setValue(duration.get_ll_value());
	return sg_ret::ok;
}




/**
   @reviewed-on 2019-11-25
*/
Duration DurationWidget::get_value(void) const
{
	Duration result(this->m_widget->value(), this->m_unit);
	return result;
}




/**
   @reviewed-on 2019-11-25
*/
void DurationWidget::clear_widget(void)
{
	this->m_widget->clear();
}
