/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2006, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2012-2015, Rob Norris <rw_norris@hotmail.com>
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




#include <QPushButton>




#include "datasource.h"
#include "dialog.h"
#include "globals.h"
#include "layer_trw_import.h"
#include "gpx.h"
#include "util.h"
#include "widget_list_selection.h"
#include "viewport_internal.h"
#include "window.h"




using namespace SlavGPS;




#define SG_MODULE "DataSource"




DataSource::~DataSource()
{
	delete this->m_acquire_options;
	delete this->m_download_options;
}




AcquireProgressDialog * DataSource::create_progress_dialog(const QString & title)
{
	AcquireProgressDialog * dialog = new AcquireProgressDialog(title, this->m_keep_dialog_open_after_success);
	dialog->set_headline(QObject::tr("Importing data..."));
	dialog->setMinimumWidth(300);
	dialog->setAttribute(Qt::WA_DeleteOnClose);

	return dialog;
}




AcquireProgressDialog::AcquireProgressDialog(const QString & window_title, bool keep_open_after_success, QWidget * parent) : BasicDialog(parent)
{
	this->setWindowTitle(window_title);

	this->headline = new QLabel(QObject::tr("Working..."));
	this->current_status = new QLabel("");
	this->m_keep_dialog_open_after_success = keep_open_after_success;

	this->grid->addWidget(this->headline, 0, 0);
	this->grid->addWidget(this->current_status, 1, 0);

	/* There will be nothing to confirm with OK button when data
	   source is importing data, so the OK button needs to be
	   blocked. */
	this->button_box->button(QDialogButtonBox::Ok)->setEnabled(false);
}




AcquireProgressDialog::~AcquireProgressDialog()
{
	qDebug() << SG_PREFIX_I;

	if (this->list_selection_widget) {
		qDebug() << SG_PREFIX_I << "Removing list selection widget from Acquire Progress Dialog";
		this->grid->removeWidget(this->list_selection_widget);

		/* By making the widget a child of viewport, we make sure that
		   it won't be deleted by destructor of acquire progress dialog. */
		this->list_selection_widget->setParent(ThisApp::get_main_gis_view());
	}
}




void AcquireProgressDialog::set_headline(const QString & text)
{
	this->headline->setText(text);
}




void AcquireProgressDialog::set_current_status(const QString & text)
{
	this->current_status->setText(text);
}




void AcquireProgressDialog::handle_acquire_completed_with_success_cb(void)
{
	qDebug() << SG_PREFIX_SLOT << "Handling signal about successful completion of acquire";

	if (this->m_keep_dialog_open_after_success) {
		this->button_box->button(QDialogButtonBox::Ok)->setEnabled(true);
		this->button_box->button(QDialogButtonBox::Cancel)->setEnabled(false);
	} else {
		/* Call 'close()' slot to close the dialog. Thanks to
		   Qt::WA_DeleteOnClose flag the dialog will be also
		   deleted. */
		qDebug() << SG_PREFIX_I << "Will close the dialog by calling 'close()'";
		this->close();
	}
}




void AcquireProgressDialog::handle_acquire_completed_with_failure_cb(void)
{
	qDebug() << SG_PREFIX_SLOT << "Handling signal about unsuccessful completion of acquire";

	this->set_headline(QObject::tr("Error: acquisition failed."));
}
