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




#include <QWidget>
#include <QComboBox>




#include "coord.h"
#include "ui_builder.h"
#include "widget_measurement_entry.h"




namespace SlavGPS {




	class Waypoint;
	class SGDateTimeButton;
	class CoordEntryWidget;
	class TimestampWidget;
	class FileSelectorWidget;




	/* Edit properties of new waypoint (waypoint that is being created). */
	bool waypoint_new_dialog(Waypoint * wp, const QString & default_wp_name, CoordMode coord_mode, QWidget * parent = NULL);

	/* Edit properties of existing waypoint. */
	bool waypoint_edit_dialog(Waypoint * wp, CoordMode coord_mode, QWidget * parent = NULL);



	class WpPropertiesDialog : public QDialog {
		Q_OBJECT
	public:
		WpPropertiesDialog(Waypoint * wp, QWidget * parent = NULL);
		~WpPropertiesDialog();

		sg_ret set_dialog_data(Waypoint * wp, const QString & name);
		sg_ret reset_dialog_data(void);
		void set_title(const QString & title);


		QLineEdit * name_entry = NULL;


	private slots:
		bool sync_name_entry_to_current_object_cb(const QString &);
		void sync_coord_entry_to_current_object_cb(void);
		void sync_empty_timestamp_entry_to_current_object_cb(void);
		void sync_timestamp_entry_to_current_object_cb(const Time & timestamp);
		void sync_altitude_entry_to_current_object_cb(void);
		void sync_comment_entry_to_current_object_cb(const QString &);
		void sync_description_entry_to_current_object_cb(const QString &);
		void sync_file_selector_to_current_object_cb(void);
		void symbol_entry_changed_cb(int);

	signals:
		/* Coordinates of edited object have changed. */
		void object_coordinates_changed(void);

	public:
		void update_timestamp_widget(const Waypoint * wp);

		bool set_timestamp_of_current_object(const Time & timestamp);

		void save_from_dialog(Waypoint * saved_object);

	private:
		sg_ret build_widgets(QWidget * parent_widget);

		Waypoint * current_object = NULL;
		bool sync_to_current_object_block = false;

		QDialogButtonBox * button_box = NULL;
		QGridLayout * grid = NULL;
		QVBoxLayout * vbox = NULL;


		CoordEntryWidget * coord_entry = NULL;
		TimestampWidget * timestamp_widget = NULL;
		MeasurementEntry_2<Altitude, HeightUnit> * altitude_entry = NULL;
		QLineEdit * comment_entry = NULL;
		QLineEdit * description_entry = NULL;
		FileSelectorWidget * file_selector = NULL;
		QComboBox * symbol_combo = NULL;
	};





} /* namespace SlavGPS */




#endif /* #ifndef _SG_WAYPOINT_PARAMETERS_H_ */
