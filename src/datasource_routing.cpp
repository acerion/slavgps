/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
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




#include <cassert>




#include <QComboBox>
#include <QLineEdit>




#include "datasource_routing.h"
#include "babel.h"
#include "gpx.h"
#include "acquire.h"
#include "routing.h"
#include "util.h"




using namespace SlavGPS;




/* Memory of previous selection */
static int last_engine = 0;
static QString last_from_str;
static QString last_to_str;




DataSourceRouting::DataSourceRouting()
{
	this->window_title = QObject::tr("Directions");
	this->layer_title = QObject::tr("Directions");
	this->mode = DataSourceMode::AutoLayerManagement;
	this->input_type = DataSourceInputType::None;
	this->autoview = true;
	this->keep_dialog_open = true; /* true = keep dialog open after success. */
}




int DataSourceRouting::run_config_dialog(AcquireContext * acquire_context)
{
	DataSourceRoutingDialog config_dialog(this->window_title);

	const int answer = config_dialog.exec();
	if (answer == QDialog::Accepted) {
		this->acquire_options = config_dialog.create_acquire_options(acquire_context);
		this->download_options = new DownloadOptions; /* With default values. */
	}

	return answer;
}




DataSourceRoutingDialog::DataSourceRoutingDialog(const QString & window_title) : DataSourceDialog(window_title)
{
	/* Engine selector. */
	QLabel * engine_label = new QLabel(tr("Engine:"));
	this->engines_combo = Routing::create_engines_combo(routing_engine_supports_refine);
	this->engines_combo->setCurrentIndex(last_engine);

	/* From and To entries. */
	QLabel * from_label = new QLabel(tr("From:"));
	QLabel * to_label = new QLabel(tr("To:"));

	if (!last_from_str.isEmpty()) {
		this->from_entry.setText(last_from_str);
	}

	if (!last_to_str.isEmpty()) {
		this->to_entry.setText(last_to_str);
	}


	/* Packing all these widgets. */
	this->grid->addWidget(engine_label, 0, 0);
	this->grid->addWidget(this->engines_combo, 0, 1);

	this->grid->addWidget(from_label, 1, 0);
	this->grid->addWidget(&this->from_entry, 1, 1);

	this->grid->addWidget(to_label, 2, 0);
	this->grid->addWidget(&this->to_entry, 2, 1);
}




AcquireOptions * DataSourceRoutingDialog::create_acquire_options(AcquireContext * acquire_context)
{
	/* Retrieve directions. */
	const QString from = this->from_entry.text();
	const QString to = this->to_entry.text();

	/* Retrieve engine. */
	last_engine = this->engines_combo->currentIndex();

	RoutingEngine * engine = Routing::get_engine_by_index(this->engines_combo, last_engine);
	if (!engine) {
		return NULL; /* FIXME: this needs to be handled in caller. */
	}

	AcquireOptions * babel_options = new AcquireOptions(AcquireOptions::Mode::FromURL);

	babel_options->source_url = engine->get_url_from_directions(from, to);
	babel_options->input_data_format = QString(engine->get_format());
	/* Don't modify dl_options, i.e. use the default download settings. */

	/* Save last selection. */
	last_from_str = from;
	last_to_str = to;

	return babel_options;
}




DataSourceRoutingDialog::~DataSourceRoutingDialog()
{
}
