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
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

static DataSourceDialog * datasource_routing_create_setup_dialog(Viewport * viewport, void * user_data);




VikDataSourceInterface vik_datasource_routing_interface = {
	N_("Directions"),
	N_("Directions"),
	DatasourceMode::AUTO_LAYER_MANAGEMENT,
	DatasourceInputtype::NONE,
	true,
	true,
	true,

	(DataSourceInternalDialog)            NULL,
	(VikDataSourceInitFunc)               NULL,
	(VikDataSourceCheckExistenceFunc)     NULL,
	(DataSourceCreateSetupDialogFunc)     datasource_routing_create_setup_dialog,
	(VikDataSourceGetProcessOptionsFunc)  NULL,
	(VikDataSourceProcessFunc)            a_babel_convert_from,
	(VikDataSourceProgressFunc)           NULL,
	(DataSourceCreateProgressDialogFunc)  NULL,
	(VikDataSourceCleanupFunc)            NULL,
	(DataSourceTurnOffFunc)               NULL,

	NULL,
	0,
	NULL,
	NULL,
	0
};




static DataSourceDialog * datasource_routing_create_setup_dialog(Viewport * viewport, void * user_data)
{
	return new DataSourceRoutingDialog();
}




DataSourceRoutingDialog::DataSourceRoutingDialog()
{
	/* Engine selector. */
	QLabel * engine_label = new QLabel(tr("Engine:"));
#ifdef K
	this->engines_combo = routing_ui_selector_new(RoutingEngine::supports_refine(void), NULL);
#else
	this->engines_combo = routing_ui_selector_new(NULL, NULL);
#endif
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




ProcessOptions * DataSourceRoutingDialog::get_process_options(DownloadOptions & dl_options)
{
	ProcessOptions * po = new ProcessOptions();

	/* Retrieve directions. */
	const QString from = this->from_entry.text();
	const QString to = this->to_entry.text();

	/* Retrieve engine. */
	last_engine = this->engines_combo->currentIndex();

	RoutingEngine * engine = routing_ui_selector_get_nth(this->engines_combo, last_engine);
	if (!engine) {
		return NULL; /* kamil FIXME: this needs to be handled in caller. */
	}

	po->url = engine->get_url_from_directions(from.toUtf8().constData(), to.toUtf8().constData());
	po->input_file_type = strdup(engine->get_format());
	/* Don't modify dl_options, i.e. use the default download settings. */

	/* Save last selection. */
	last_from_str = from;
	last_to_str = to;

	return po;
}




DataSourceRoutingDialog::~DataSourceRoutingDialog()
{
}
