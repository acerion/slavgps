/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _SG_DATASOURCE_GEOCACHE_H_
#define _SG_DATASOURCE_GEOCACHE_H_




#include <QSpinBox>
#include <QLineEdit>
#include <QDoubleSpinBox>




#include "datasource.h"
#include "babel.h"




namespace SlavGPS {




	class DataSourceGeoCache : public DataSourceBabel {
	public:
		DataSourceGeoCache() {};
		DataSourceGeoCache(Viewport * viewport);
		~DataSourceGeoCache() {};

		static void init(void);
		static bool have_programs(void); /* Check if programs necessary for using GeoCaches data source are available. */

		int run_config_dialog(void);

		Viewport * viewport = NULL;
	};




	class DataSourceGCDialog : public DataSourceDialog {
		Q_OBJECT
	public:
		DataSourceGCDialog(const QString & window_title, Viewport * viewport);
		~DataSourceGCDialog();

		ProcessOptions * get_process_options_none(void);

		QSpinBox num_spin;
		QLineEdit center_entry; /* TODO make separate widgets for lat/lon. */
		QDoubleSpinBox miles_radius_spin;

		QPen circle_pen;
		Viewport * viewport = NULL;
		bool circle_onscreen;
		int circle_x;
		int circle_y;
		int circle_width;

	private slots:
		void draw_circle_cb(void);

	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_DATASOURCE_GEOCACHE_H_ */
