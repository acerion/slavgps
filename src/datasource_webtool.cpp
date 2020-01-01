/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013-2015, Rob Norris <rw_norris@hotmail.com>
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




#include <QDebug>
#include <QLineEdit>
#include <QHash>
#include <QPushButton>




#include "window.h"
#include "layers_panel.h"
#include "viewport_internal.h"
#include "datasource_webtool.h"
#include "webtool_query.h"
#include "acquire.h"
#include "map_utils.h"
#include "dialog.h"
#include "util.h"
#include "tree_view.h"
#include "layer_trw.h"




using namespace SlavGPS;




#define SG_MODULE "DataSource OnlineService"




QHash<QString, QString> dso_last_user_strings;




DataSourceOnlineServiceDialog::DataSourceOnlineServiceDialog(const QString & window_title, GisViewport * new_gisview, OnlineService_query * new_online_service) : DataSourceDialog(window_title)
{
	this->gisview = new_gisview;
	this->online_service = new_online_service;


	QLabel * user_string_label = new QLabel(QString("%1:").arg(this->online_service->input_field_label_text), this);

	this->input_field.setText(this->online_service->get_last_user_string());

#ifdef K_FIXME_RESTORE
	/* 'ok' when press return in the entry. */
	QObject::connect(&this->input_field, SIGNAL("activate"), dialog, SLOT (accept));
#endif


	this->grid->addWidget(user_string_label, 0, 0);
	this->grid->addWidget(&this->input_field, 0, 1);

	this->button_box->button(QDialogButtonBox::Ok)->setDefault(true);
	/* NB presently the focus is overridden later on by the acquire.c code. */
	this->input_field.setFocus();
}




AcquireOptions * DataSourceOnlineServiceDialog::create_acquire_options(AcquireContext * acquire_context)
{
	if (this->online_service->tool_needs_user_string()) {
		this->online_service->user_string = this->input_field.text();

		if (this->online_service->user_string[0] != '\0') {
			const QString tool_label = this->online_service->get_label();
			dso_last_user_strings.insert(tool_label, this->online_service->user_string);
		}
	}


	AcquireOptions * acquire_options = new AcquireOptions(AcquireOptions::Mode::FromURL);
	acquire_options->source_url = this->online_service->get_url_for_viewport(this->gisview);
	qDebug() << SG_PREFIX_D << "Source URL =" << acquire_options->source_url;

	/* Only use first section of the file_type string.
	   One can't use values like 'kml -x transform,rte=wpt' in order to do fancy things
	   since it won't be in the right order for the overall GPSBabel command.
	   So prevent any potentially dangerous behaviour. */
	if (!this->online_service->file_type.isEmpty()) {
		QStringList parts = this->online_service->file_type.split(" ");
		if (parts.size()) {
			acquire_options->input_data_format = parts.at(0);
		}
	}


	return acquire_options;
}




void DataSourceOnlineService::cleanup(void * data)
{
	free(data);
}



DataSourceOnlineService::DataSourceOnlineService(const QString & new_window_title, const QString & new_layer_title, GisViewport * new_gisview, OnlineService_query * new_online_service)
{
	this->gisview = new_gisview;
	this->online_service = new_online_service;

	this->window_title = new_window_title;
	this->layer_title = new_layer_title;
	//this->mode = DataSourceMode::AddToLayer; /* TODO_LATER: restore? investigate? */
	this->mode = DataSourceMode::CreateNewLayer;
	this->input_type = DataSourceInputType::None;
	this->autoview = false; /* false = maintain current view rather than setting it to the acquired points. */
	this->keep_dialog_open = true; /* true = keep dialog open after success. */
}




SGObjectTypeID DataSourceOnlineService::get_source_id(void) const
{
	return DataSourceOnlineService::source_id();
}
SGObjectTypeID DataSourceOnlineService::source_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.datasource.online_service");
	return id;
}




int DataSourceOnlineService::run_config_dialog(AcquireContext * acquire_context)
{
	int answer;

	DataSourceOnlineServiceDialog config_dialog(this->window_title, this->gisview, this->online_service);
	if (this->online_service->tool_needs_user_string()) {
		answer = config_dialog.exec();
	} else {
		/* Simple resolution for online services that don't
		   require any extra user strings/query terms (which
		   means that we don't really need to display the
		   config dialog, but we still need config dialog's
		   ::create_acquire_options()). */
		answer = QDialog::Accepted;
	}

	if (answer == QDialog::Accepted) {
		this->acquire_options = config_dialog.create_acquire_options(acquire_context);
		this->download_options = new DownloadOptions; /* With default values. */
		this->download_options->follow_location = 1; /* http -> https */
	}

	return answer;
}
