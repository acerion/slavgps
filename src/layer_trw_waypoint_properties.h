/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (c) 2014, Rob Norris <rw_norris@hotmail.com>
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
#ifndef _SG_WAYPOINT_PARAMETERS_H_
#define _SG_WAYPOINT_PARAMETERS_H_




#include <tuple>




#include <QWidget>
#include <QComboBox>




#include "coord.h"
#include "ui_builder.h"




namespace SlavGPS {




	class Waypoint;
	class SGDateTimeButton;




	enum {
		SG_WP_PARAM_NAME,
		SG_WP_PARAM_LAT,
		SG_WP_PARAM_LON,
		SG_WP_PARAM_TIME,
		SG_WP_PARAM_ALT,
		SG_WP_PARAM_COMMENT,
		SG_WP_PARAM_DESC,
		SG_WP_PARAM_IMAGE,
		SG_WP_PARAM_SYMBOL,
		SG_WP_PARAM_MAX
	};




	enum {
		SG_WP_DIALOG_OK   = 0,
		SG_WP_DIALOG_NAME = 1
	};

	std::tuple<bool, bool> waypoint_properties_dialog(Waypoint * wp, const QString & default_name, CoordMode coord_mode, QWidget * parent = NULL);




	class PropertiesDialogWaypoint : public PropertiesDialog {
		Q_OBJECT
	public:
		PropertiesDialogWaypoint(Waypoint * wp, QString const & title = "Properties", QWidget * parent = NULL);

		Waypoint * wp = NULL; /* Reference. */
		SGDateTimeButton * date_time_button = NULL; /* Reference. */
		QComboBox * symbol_combo = NULL; /* Reference. */

	public slots:
		void set_timestamp_cb(const Time & timestamp);
		void clear_timestamp_cb(void);
		void symbol_entry_changed_cb(int index);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WAYPOINT_PARAMETERS_H_ */
