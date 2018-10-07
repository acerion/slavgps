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
#include "acquire.h"
#include "gpx.h"
#include "util.h"




using namespace SlavGPS;




#define SG_MODULE "DataSource"




DataSource::~DataSource()
{
	delete this->acquire_options;
	delete this->download_options;
}




AcquireProgressDialog * DataSource::create_progress_dialog(const QString & title)
{
	AcquireProgressDialog * dialog = new AcquireProgressDialog(title);

	return dialog;
}




AcquireOptions * DataSourceDialog::create_acquire_options_layer(LayerTRW * trw)
{
	qDebug() << SG_PREFIX_I << "Input type: TRWLayer";

	AcquireOptions * process_options = NULL;

	const QString layer_file_full_path = GPX::write_tmp_file(trw, NULL);
	process_options = this->get_acquire_options_layer(layer_file_full_path);
	Util::add_to_deletion_list(layer_file_full_path);

	return process_options;
}




AcquireOptions * DataSourceDialog::create_acquire_options_layer_track(LayerTRW * trw, Track * trk)
{
	qDebug() << SG_PREFIX_I << "Input type: TRWLayerTrack";

	const QString layer_file_full_path = GPX::write_tmp_file(trw, NULL);
	const QString track_file_full_path = GPX::write_track_tmp_file(trk, NULL);

	AcquireOptions * process_options = this->get_acquire_options_layer_track(layer_file_full_path, track_file_full_path);

	Util::add_to_deletion_list(layer_file_full_path);
	Util::add_to_deletion_list(track_file_full_path);

	return process_options;
}




AcquireOptions * DataSourceDialog::create_acquire_options_track(Track * trk)
{
	qDebug() << SG_PREFIX_I << "Input type: Track";

	const QString track_file_full_path = GPX::write_track_tmp_file(trk, NULL);
	AcquireOptions * process_options = this->get_acquire_options_layer_track("", track_file_full_path);

	return process_options;
}




AcquireOptions * DataSourceDialog::create_acquire_options_none(void)
{
	qDebug() << SG_PREFIX_I << "Input type: None";

	AcquireOptions * process_options = this->get_acquire_options_none();

	return process_options;
}




AcquireProgressDialog::AcquireProgressDialog(const QString & window_title, QWidget * parent) : BasicDialog(parent)
{
	this->setWindowTitle(window_title);

	this->headline = new QLabel(QObject::tr("Working..."));
	this->current_status = new QLabel("");

	this->grid->addWidget(this->headline, 0, 0);
	this->grid->addWidget(this->current_status, 1, 0);

	/* There will be nothing to confirm with OK button when data
	   source is importing data, so the OK button needs to be
	   blocked. */
	this->button_box->button(QDialogButtonBox::Ok)->setEnabled(false);
}




void AcquireProgressDialog::set_headline(const QString & text)
{
	this->headline->setText(text);
}



void AcquireProgressDialog::set_current_status(const QString & text)
{
	this->current_status->setText(text);
}
