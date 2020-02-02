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




#include <QDebug>
#include <QLineEdit>
#include <QHash>
#include <QPushButton>




#include "layer.h"
#include "layer_aggregate.h"
#include "window.h"
#include "layers_panel.h"
#include "datasource.h"
#include "datasource_webtool.h"
#include "viewport_internal.h"
#include "webtool_query.h"




using namespace SlavGPS;




#define SG_MODULE "Online Service with Query"
#define MAX_NUMBER_CODES 7




extern QHash<QString, QString> dso_last_user_strings;




OnlineService_query::OnlineService_query(const QString & new_tool_name,
					 const QString & new_url_format,
					 const QString & new_url_format_code,
					 const QString & new_file_type,
					 const QString & new_input_field_label_text) : OnlineService(new_tool_name)
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




OnlineService_query::~OnlineService_query()
{
	qDebug() << SG_PREFIX_I << "Delete tool with label" << this->label;
}




/**
 * Calculate individual elements (similarly to the Online Service BBox & Center) for *all* potential values.
 * Then only values specified by the URL format are used in parameterizing the URL.
 */
QString OnlineService_query::get_url_for_viewport(GisViewport * a_gisview)
{
	/* Center values. */
	const LatLon lat_lon = a_gisview->get_center_coord().get_lat_lon();

	QString center_lat;
	QString center_lon;
	lat_lon.to_strings_raw(center_lat, center_lon);


	/* Zoom - ideally x & y factors need to be the same otherwise use the default. */
	TileZoomLevel tile_zoom_level(TileZoomLevel::Level::Default); /* Zoomed in by default. */
	if (a_gisview->get_viking_scale().x_y_is_equal()) {
		tile_zoom_level = a_gisview->get_viking_scale().to_tile_zoom_level();
	}

	int len = this->url_format_code.size();
	if (len == 0) {
		qDebug() << SG_PREFIX_E << "url format code is empty";
		return QString("");
	} else if (len > MAX_NUMBER_CODES) {
		qDebug() << SG_PREFIX_E << "url format code too long:" << len << MAX_NUMBER_CODES << this->url_format_code;
		return QString("");
	} else {
		;
	}

	const LatLonBBoxStrings bbox_strings = a_gisview->get_bbox().values_to_c_strings();

	QString url = this->url_format;

	/* Evaluate+replace each consecutive format specifier %1, %2,
	   %3 etc. in url=='format string' with a value. */
	for (int i = 0; i < len; i++) {
		switch (this->url_format_code[i].toUpper().toLatin1()) {
		case 'L': url = url.arg(bbox_strings.west);  break;
		case 'R': url = url.arg(bbox_strings.east);  break;
		case 'B': url = url.arg(bbox_strings.south); break;
		case 'T': url = url.arg(bbox_strings.north); break;
		case 'A': url = url.arg(center_lat); break;
		case 'O': url = url.arg(center_lon); break;
		case 'Z': url = url.arg(tile_zoom_level.to_string()); break;
		case 'S': url = url.arg(this->user_string); break;
		default:
			qDebug() << SG_PREFIX_E << "Invalid URL format code" << this->url_format_code[i] << "at position" << i;
			return QString("");
		}
	}

	qDebug() << SG_PREFIX_I << "URL at current position is" << url;

	return url;
}




QString OnlineService_query::get_url_at_position(GisViewport * a_gisview, const __attribute__((unused)) Coord * a_coord)
{
	return this->get_url_for_viewport(a_gisview);
}




/**
   Returns true if the URL format contains 'S' -- that is, a search
   term entry box needs to be displayed.
*/
bool OnlineService_query::tool_needs_user_string(void) const
{
	return this->url_format_code.contains("S", Qt::CaseInsensitive);
}




QString OnlineService_query::get_last_user_string(void) const
{
	auto iter = dso_last_user_strings.find(this->get_label());
	if (iter == dso_last_user_strings.end()) {
		return "";
	} else {
		return *iter;
	}
}




void OnlineService_query::run_at_current_position(GisViewport * gisview)
{
	DataSource * data_source = new DataSourceOnlineService(this->get_label(),
							       this->get_label(),
							       gisview,
							       this);


	Layer * parent = nullptr;
	Layer * existing_layer = ThisApp::get_layers_panel()->get_selected_layer();
	if (existing_layer) {
		parent = existing_layer->get_owning_layer(); /* Maybe Aggregate layer, or maybe GPS layer. */
	} else {
		parent = ThisApp::get_layers_panel()->get_top_layer();
	}

	switch (existing_layer->m_kind) {
	case LayerKind::TRW:
		if (parent->m_kind == LayerKind::Aggregate || parent->m_kind == LayerKind::GPS) {
			AcquireContext acquire_context(gisview->get_window(), gisview, parent, (LayerTRW *) existing_layer, nullptr);
			Acquire::acquire_from_source(data_source, data_source->m_layer_mode, acquire_context);
		}
		break;
	default:
		/* Not an error, we just don't support acquiring to
		   non-TRW layers. */
		break;
	}
}
