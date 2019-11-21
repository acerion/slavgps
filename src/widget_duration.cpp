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




DurationWidget::DurationWidget(const MeasurementScale<Duration, Time_ll, TimeUnit> & scale, QWidget * parent)
{
	this->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);

	this->m_hbox = new QHBoxLayout();

	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->m_hbox);


	this->m_widget = new QSpinBox(parent);
	this->m_widget->setMinimum(0);
	this->m_widget->setMaximum(59);
	this->m_widget->setSingleStep(1);
	this->m_widget->setValue(0);
	this->m_widget->setSuffix(tr("s"));
	this->m_widget->setToolTip(tr("Widget"));

	this->m_hbox->addWidget(this->m_widget);


	/* Ensure entry field has focus so we can start typing
	   straight away. */
	this->setFocusProxy(this->m_widget);

	this->m_hbox->setContentsMargins(0, 0, 0, 0);
}





sg_ret DurationWidget::set_value(const Duration & duration)
{
	return sg_ret::ok;
}

Duration DurationWidget::get_value(void) const
{
	Duration result;

	return result;
}

void DurationWidget::clear_widget(void)
{
}
