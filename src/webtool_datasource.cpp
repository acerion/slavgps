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




#include <vector>
#include <cassert>




#include <QDebug>
#include <QLineEdit>
#include <QHash>




#include "window.h"
#include "layers_panel.h"
#include "viewport_internal.h"
#include "webtool_datasource.h"
#include "acquire.h"
#include "map_utils.h"
#include "dialog.h"
#include "util.h"
#include "tree_view.h"
#include "layer_trw.h"




using namespace SlavGPS;




#define SG_MODULE "DataSource Webtool"

#define MAX_NUMBER_CODES 7




extern Tree * g_tree;




static QHash<QString, QString> last_user_strings;




QString WebToolDatasource::get_last_user_string(void) const
{
	auto iter = last_user_strings.find(this->get_label());
	if (iter == last_user_strings.end()) {
		return "";
	} else {
		return *iter;
	}
}




DataSourceWebToolDialog::DataSourceWebToolDialog(const QString & window_title, Viewport * new_viewport, WebToolDatasource * new_web_tool_data_source) : DataSourceDialog(window_title)
{
	this->viewport = new_viewport;
	this->web_tool_data_source = new_web_tool_data_source;


	QLabel * user_string_label = new QLabel(tr("%1:").arg(this->web_tool_data_source->input_field_label_text), this);

	this->input_field.setText(this->web_tool_data_source->get_last_user_string());

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




AcquireOptions * DataSourceWebToolDialog::create_acquire_options(AcquireContext * acquire_context)
{
	if (this->web_tool_data_source->webtool_needs_user_string()) {
		this->web_tool_data_source->user_string = this->input_field.text();

		if (this->web_tool_data_source->user_string[0] != '\0') {
			const QString tool_label = this->web_tool_data_source->get_label();
			last_user_strings.insert(tool_label, this->web_tool_data_source->user_string);
		}
	}


	AcquireOptions * babel_options = new AcquireOptions(AcquireOptions::Mode::FromURL);
	babel_options->source_url = this->web_tool_data_source->get_url_for_viewport(this->viewport);
	qDebug() << SG_PREFIX_D << "Source URL =" << babel_options->source_url;

	/* Only use first section of the file_type string.
	   One can't use values like 'kml -x transform,rte=wpt' in order to do fancy things
	   since it won't be in the right order for the overall GPSBabel command.
	   So prevent any potentially dangerous behaviour. */
	QStringList parts;
	if (!this->web_tool_data_source->file_type.isEmpty()) {
		parts = this->web_tool_data_source->file_type.split(" ");
		if (parts.size()) {
			babel_options->input_data_format = parts.at(0);
		}
	}


	return babel_options;

}




void DataSourceWebTool::cleanup(void * data)
{
	free(data);
}



DataSourceWebTool::DataSourceWebTool(bool new_search, const QString & new_window_title, const QString & new_layer_title, Viewport * new_viewport, WebToolDatasource * new_web_tool_data_source)
{
	this->viewport = new_viewport;
	this->web_tool_data_source = new_web_tool_data_source;

	this->window_title = new_window_title;
	this->layer_title = new_layer_title;
	this->mode = DataSourceMode::AddToLayer;
	this->input_type = DataSourceInputType::None;
	this->autoview = false; /* false = maintain current view rather than setting it to the acquired points. */
	this->keep_dialog_open = true; /* true = keep dialog open after success. */

	this->search = new_search;
}




int DataSourceWebTool::run_config_dialog(AcquireContext * acquire_context)
{
	int answer;

	if (this->search) {
		DataSourceWebToolDialog config_dialog(this->window_title, this->viewport, this->web_tool_data_source);
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




void WebToolDatasource::run_at_current_position(Viewport * a_viewport)
{
	const bool search = this->webtool_needs_user_string();

	DataSource * data_source = new DataSourceWebTool(search, this->get_label(), this->get_label(), a_viewport, this);

	AcquireContext acquire_context(a_viewport->get_window(), a_viewport, g_tree->tree_get_items_tree()->get_top_layer(), g_tree->tree_get_items_tree()->get_selected_layer());
	Acquire::acquire_from_source(data_source, data_source->mode, &acquire_context);

#ifdef K_TODO
	/* TODO: I think that this is already done in AcquireWorker::on_complete_process() */
	if (acquire_context.trw) {
		acquire_context.trw->attach_children_to_tree();
	}
#endif
}




WebToolDatasource::WebToolDatasource(const QString & new_tool_name,
				     const QString & new_url_format,
				     const QString & new_url_format_code,
				     const QString & new_file_type,
				     const QString & new_input_field_label_text) : WebTool(new_tool_name)
{
	qDebug() << SG_PREFIX_I << "Created with tool name" << new_tool_name;

	this->label = new_tool_name;
	this->url_format = new_url_format;
	this->url_format_code = new_url_format_code; /* A default value would be "LRBT". */

	if (!new_file_type.isEmpty()) {
		this->file_type = new_file_type;
	}
	if (!new_input_field_label_text.isEmpty()) {
		this->input_field_label_text = new_input_field_label_text;
	}
}




WebToolDatasource::~WebToolDatasource()
{
	qDebug() << SG_PREFIX_I << "Delete tool with label" << this->label;
}




/**
 * Calculate individual elements (similarly to the VikWebtool Bounds & Center) for *all* potential values.
 * Then only values specified by the URL format are used in parameterizing the URL.
 */
QString WebToolDatasource::get_url_for_viewport(Viewport * a_viewport)
{
	/* Center values. */
	const LatLon lat_lon = a_viewport->get_center()->get_latlon();

	QString center_lat;
	QString center_lon;
	lat_lon.to_strings_raw(center_lat, center_lon);

	/* Zoom - ideally x & y factors need to be the same otherwise use the default. */
	TileZoomLevel tile_zoom_level(TileZoomLevels::Default); /* Zoomed in by default. */
	if (a_viewport->get_viking_zoom_level().x_y_is_equal()) {
		tile_zoom_level = a_viewport->get_viking_zoom_level().to_tile_zoom_level();
	}

	int len = this->url_format_code.size();
	if (len == 0) {
		qDebug() << SG_PREFIX_E << "url format code is empty";
		return QString("");
	} else if (len > MAX_NUMBER_CODES) {
		qDebug() << SG_PREFIX_W << "url format code too long:" << len << MAX_NUMBER_CODES << this->url_format_code;
		len = MAX_NUMBER_CODES;
	} else {
		;
	}

	std::vector<QString> values;

	const LatLonBBoxStrings bbox_strings = a_viewport->get_bbox().to_strings();

	for (int i = 0; i < len; i++) {
		switch (this->url_format_code[i].toUpper().toLatin1()) {
		case 'L': values[i] = bbox_strings.west;  break;
		case 'R': values[i] = bbox_strings.east;  break;
		case 'B': values[i] = bbox_strings.south; break;
		case 'T': values[i] = bbox_strings.north; break;
		case 'A': values[i] = center_lat; break;
		case 'O': values[i] = center_lon; break;
		case 'Z': values[i] = tile_zoom_level.to_string(); break;
		case 'S': values[i] = this->user_string; break;
		default:
			qDebug() << SG_PREFIX_E << "Invalid URL format code" << this->url_format_code[i];
			return QString("");
		}
	}

	QString url = QString(this->url_format)
		.arg(values[0])
		.arg(values[1])
		.arg(values[2])
		.arg(values[3])
		.arg(values[4])
		.arg(values[5])
		.arg(values[6]);

	qDebug() << SG_PREFIX_I << "url at current position is" << url;

	return url;
}




QString WebToolDatasource::get_url_at_position(Viewport * a_viewport, const Coord * a_coord)
{
	return this->get_url_for_viewport(a_viewport);
}




/**
   Returns true if the URL format contains 'S' -- that is, a search
   term entry box needs to be displayed.
*/
bool WebToolDatasource::webtool_needs_user_string()
{
	return this->url_format_code.contains("S", Qt::CaseInsensitive);
}
