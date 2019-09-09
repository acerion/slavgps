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
 */
#ifndef _SG_LAYER_TRW_WAYPOINT_PROPERTIES_H_
#define _SG_LAYER_TRW_WAYPOINT_PROPERTIES_H_




#include <QWidget>
#include <QComboBox>
#include <QSignalMapper>




#include "coord.h"
#include "ui_builder.h"
#include "widget_measurement_entry.h"
#include "layer_trw_point_properties.h"




namespace SlavGPS {




	class Waypoint;
	class SGDateTimeButton;
	class CoordEntryWidget;
	class TimestampWidget;
	class FileSelectorWidget;




	class WpPropertiesWidget : public PointPropertiesWidget {
		Q_OBJECT
	public:
		WpPropertiesWidget(QWidget * parent = NULL);

		sg_ret build_widgets(QWidget * parent_widget);

		/* Erase all contents from widgets, as if nothing was
		   presented by the widgets. */
		void clear_widgets(void);

		sg_ret build_buttons(QWidget * parent_widget);


		QLineEdit * comment_entry = NULL;
		QLineEdit * description_entry = NULL;
		FileSelectorWidget * file_selector = NULL;
		QComboBox * symbol_combo = NULL;

		QSignalMapper * signal_mapper = NULL;

		QPushButton * button_delete_current_point = NULL;
		QPushButton * button_previous_point = NULL;
		QPushButton * button_next_point = NULL;
		QPushButton * button_close_dialog = NULL;
	};




	class WpPropertiesDialog : public WpPropertiesWidget {
		Q_OBJECT
	public:
		WpPropertiesDialog(CoordMode coord_mode, QWidget * parent_widget);
		~WpPropertiesDialog();

		sg_ret dialog_data_set(Waypoint * wp);
		void dialog_data_reset(void);
		void set_dialog_title(const QString & title);

		void set_coord_mode(CoordMode coord_mode);

		/* Dialog action codes. */
		enum class Action {
			DeleteSelectedPoint,
			PreviousPoint,
			NextPoint,
		};

	public slots:
		void clicked_cb(int response);

		/* Called to inform the dialog that a change of
		   selection in main tree view has been made. */
		void tree_view_selection_changed_cb(void);

	private slots:
		bool sync_name_entry_to_current_point_cb(const QString & name);
		void sync_coord_widget_to_current_point_cb(void);
		void sync_altitude_widget_to_current_point_cb(void);
		bool sync_timestamp_widget_to_current_point_cb(const Time & timestamp);
		bool sync_empty_timestamp_widget_to_current_point_cb(void);


		void sync_comment_entry_to_current_point_cb(const QString &);
		void sync_description_entry_to_current_point_cb(const QString &);
		void sync_file_selector_to_current_point_cb(void);
		void sync_symbol_combo_to_current_point_cb(int index_in_combo);

	signals:
		/* Coordinates of edited object have changed. */
		void object_coordinates_changed(void);

		/* Coordinates of one of track's trackpoints has changed its coordinates. */
		void point_coordinates_changed(void);


	public:
		void update_timestamp_widget(const Waypoint * wp);


	private:
		Waypoint * current_point = NULL;

		/* Don't pass values currently set/entered in widgets
		   to currently edited point, because the entering is
		   being done as part of initializing properties
		   dialog. */
		bool skip_syncing_to_current_point = false;
	};




#if 0
	class WpPropertiesDialog : public QDialog {
		Q_OBJECT

	private slots:
		void symbol_entry_changed_cb(int);

	public:
		void update_timestamp_widget(const Waypoint * wp);


		void save_from_dialog(Waypoint * saved_object);

	private:

		QLineEdit * comment_entry = NULL;
		QLineEdit * description_entry = NULL;
		FileSelectorWidget * file_selector = NULL;
		QComboBox * symbol_combo = NULL;
	};
#endif




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_TRW_WAYPOINT_PROPERTIES_H_ */
