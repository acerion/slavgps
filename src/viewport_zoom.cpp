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




//#include <cstdlib>
//#include <cstring>
//#include <cctype>

//#include <QDialog>
//#include <QInputDialog>
#include <QObject>
#include <QDebug>

#include "viewport.h"
#include "viewport_zoom.h"




using namespace SlavGPS;




bool a_dialog_custom_zoom(double * xmpp, double * ympp, QWidget * parent)
{
	ViewportZoomDialog dialog(xmpp, ympp, parent);
	if (QDialog::Accepted == dialog.exec()) {
		dialog.get_values(xmpp, ympp);
		/* There is something strange about argument to qSetRealNumberPrecision().  The precision for
		   fractional part is not enough, I had to add few places for leading digits and decimal dot. */
		qDebug() << qSetRealNumberPrecision(5 + 1 + SG_VIEWPORT_ZOOM_PRECISION) << "DD: Dialog: Saving custom zoom as" << *xmpp << *ympp;
		return true;
	} else {
		return false;
	}
}




ViewportZoomDialog::ViewportZoomDialog(double * xmpp, double * ympp, QWidget * parent)
{
	this->setWindowTitle(QObject::tr("Zoom Factors..."));

	int row = 0;

	QLabel * main_label = new QLabel(QObject::tr("Zoom factor (in meters per pixel):"), this);
	this->grid->addWidget(main_label, row, 0, 1, 2); /* Row span = 1, Column span = 2. */
	row++;


	QLabel * xlabel = new QLabel(QObject::tr("X (easting):"), this);

	/* TODO: add some kind of validation and indication for values out of range. */
	this->xspin.setMinimum(SG_VIEWPORT_ZOOM_MIN);
	this->xspin.setMaximum(SG_VIEWPORT_ZOOM_MAX);
	this->xspin.setSingleStep(1);
	this->xspin.setDecimals(SG_VIEWPORT_ZOOM_PRECISION);
	this->xspin.setValue(*xmpp);

	this->grid->addWidget(xlabel, row, 0);
	this->grid->addWidget(&this->xspin, row, 1);
	row++;


	QLabel * ylabel = new QLabel(QObject::tr("Y (northing):"), this);

	/* TODO: add some kind of validation and indication for values out of range. */
	this->yspin.setMinimum(SG_VIEWPORT_ZOOM_MIN);
	this->yspin.setMaximum(SG_VIEWPORT_ZOOM_MAX);
	this->yspin.setSingleStep(1);
	this->yspin.setDecimals(SG_VIEWPORT_ZOOM_PRECISION);
	this->yspin.setValue(*ympp);

	this->grid->addWidget(ylabel, row, 0);
	this->grid->addWidget(&this->yspin, row, 1);
	row++;


	this->checkbox.setText(QObject::tr("X and Y zoom factors must be equal"));
	if (*xmpp == *ympp) {
		this->checkbox.setChecked(true);
	}
	this->grid->addWidget(&this->checkbox, row, 0, 1, 2); /* Row span = 1, Column span = 2. */
	row++;


	QObject::connect(&this->xspin, SIGNAL (valueChanged(double)), this, SLOT (spin_changed_cb(double)));
	QObject::connect(&this->yspin, SIGNAL (valueChanged(double)), this, SLOT (spin_changed_cb(double)));
}




void ViewportZoomDialog::get_values(double * xmpp, double * ympp)
{
	*xmpp = this->xspin.value();
	*ympp = this->yspin.value();
}




void ViewportZoomDialog::spin_changed_cb(double new_value)
{
	if (this->checkbox.isChecked()) {
		if (new_value == this->xspin.value()) {
			this->yspin.setValue(new_value);
		} else {
			this->xspin.setValue(new_value);
		}
	}
}
