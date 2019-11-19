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




DurationWidget::DurationWidget(const ParameterScale<Time_ll> & scale, QWidget * parent)
{
	this->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);

	this->m_hbox = new QHBoxLayout();

	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->m_hbox);


	/* Always show "seconds" spinbox. */
	this->m_seconds = new QSpinBox();
	this->m_seconds->setMinimum(0);
	this->m_seconds->setMaximum(59);
	this->m_seconds->setSingleStep(1);
	this->m_seconds->setValue(0);
	this->m_seconds->setSuffix(tr("s"));
	this->m_seconds->setToolTip(tr("Seconds"));

	if (scale.max >= 60) { /* More than 59 seconds, so we need "minutes" spinbox. */
		this->m_minutes = new QSpinBox();
		this->m_minutes->setMinimum(0);
		this->m_minutes->setMaximum(59);
		this->m_minutes->setSingleStep(1);
		this->m_minutes->setValue(0);
		this->m_minutes->setSuffix(tr("m"));
		this->m_minutes->setToolTip(tr("Minutes"));

		if (scale.max >= 60 * 60) { /* More than 59 minutes and 59 seconds, so we need "hours" spinbox. */
			this->m_hours = new QSpinBox();
			this->m_hours->setMinimum(0);
			this->m_hours->setMaximum(23);
			this->m_hours->setSingleStep(1);
			this->m_hours->setValue(0);
			this->m_hours->setSuffix(tr("h"));
			this->m_hours->setToolTip(tr("Hours"));

			if (scale.max >= 24 * 60 * 60) { /* More than 23h 59m 59s, so we need "days" spinbox. */
				this->m_days = new QSpinBox();
				this->m_days->setMinimum(0);
				this->m_days->setMaximum(365);
				this->m_days->setSingleStep(1);
				this->m_days->setValue(0);
				this->m_days->setSuffix(tr("d"));
				this->m_days->setToolTip(tr("Days"));
			}
		}
	}

	if (this->m_days) {
		this->m_hbox->addWidget(this->m_days);
	}
	if (this->m_hours) {
		if ((24 * 60 * 60) == scale.step) {
			/* Size of step indicates that only widgets of "higher order" should be manipulated. */
			this->m_hours->setDisabled(true);
		}
		this->m_hbox->addWidget(this->m_hours);
	}
	if (this->m_minutes) {
		if ((60 * 60) == scale.step || (24 * 60 * 60) == scale.step) {
			/* Size of step indicates that only widgets of "higher order" should be manipulated. */
			this->m_minutes->setDisabled(true);
		}
		this->m_hbox->addWidget(this->m_minutes);
	}
	if (this->m_seconds) {
		if (60 == scale.step || (60 * 60) == scale.step || (24 * 60 * 60) == scale.step) {
			/* Size of step indicates that only widgets of "higher order" should be manipulated. */
			this->m_seconds->setDisabled(true);
		}
		this->m_hbox->addWidget(this->m_seconds);
	}


	/* Ensure the first entry field has focus so we can start
	   typing straight away. */
	this->setFocusProxy(this->m_seconds);

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
