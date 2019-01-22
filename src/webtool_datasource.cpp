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
#include "webtool_datasource.h"
#include "webtool_datasource.h"
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




DataSourceOnlineServiceDialog::DataSourceOnlineServiceDialog(const QString & window_title, Viewport * new_viewport, OnlineService_query * new_online_query_service) : DataSourceDialog(window_title)
{
	this->viewport = new_viewport;
	this->online_query_service = new_online_query_service;


	QLabel * user_string_label = new QLabel(tr("%1:").arg(this->online_query_service->input_field_label_text), this);

	this->input_field.setText(this->online_query_service->get_last_user_string());

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
	if (this->online_query_service->tool_needs_user_string()) {
		this->online_query_service->user_string = this->input_field.text();

		if (this->online_query_service->user_string[0] != '\0') {
			const QString tool_label = this->online_query_service->get_label();
			dso_last_user_strings.insert(tool_label, this->online_query_service->user_string);
		}
	}


	AcquireOptions * babel_options = new AcquireOptions(AcquireOptions::Mode::FromURL);
	babel_options->source_url = this->online_query_service->get_url_for_viewport(this->viewport);
	qDebug() << SG_PREFIX_D << "Source URL =" << babel_options->source_url;

	/* Only use first section of the file_type string.
	   One can't use values like 'kml -x transform,rte=wpt' in order to do fancy things
	   since it won't be in the right order for the overall GPSBabel command.
	   So prevent any potentially dangerous behaviour. */
	if (!this->online_query_service->file_type.isEmpty()) {
		QStringList parts = this->online_query_service->file_type.split(" ");
		if (parts.size()) {
			babel_options->input_data_format = parts.at(0);
		}
	}


	return babel_options;

}




void DataSourceOnlineService::cleanup(void * data)
{
	free(data);
}



DataSourceOnlineService::DataSourceOnlineService(bool new_search, const QString & new_window_title, const QString & new_layer_title, Viewport * new_viewport, OnlineService_query * new_online_query_service)
{
	this->viewport = new_viewport;
	this->online_query_service = new_online_query_service;

	this->window_title = new_window_title;
	this->layer_title = new_layer_title;
	this->mode = DataSourceMode::AddToLayer;
	this->input_type = DataSourceInputType::None;
	this->autoview = false; /* false = maintain current view rather than setting it to the acquired points. */
	this->keep_dialog_open = true; /* true = keep dialog open after success. */

	this->search = new_search;
}




int DataSourceOnlineService::run_config_dialog(AcquireContext * acquire_context)
{
	int answer;

	if (this->search) {
		DataSourceOnlineServiceDialog config_dialog(this->window_title, this->viewport, this->online_query_service);
		answer = config_dialog.exec();

		if (answer == QDialog::Accepted) {
			this->acquire_options = config_dialog.create_acquire_options(acquire_context);
			this->download_options = new DownloadOptions; /* With default values. */
		}
	} else {
		answer = QDialog::Rejected;
	}

	return answer;
}
