/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_WAYPOINT_LIST_H_
#define _SG_WAYPOINT_LIST_H_



#include <list>

#include <QWidget>
#include <QDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QStandardItemModel>
#include <QTableView>
#include <QContextMenuEvent>

#include "globals.h"
#include "measurements.h"




namespace SlavGPS {




	class Layer;
	class LayerTRW;
	class Waypoint;
	class Viewport;




	void waypoint_list_dialog(QString const & title, Layer * layer);




	class WaypointListDialog : public QDialog {
		Q_OBJECT
	public:
		WaypointListDialog(QString const & title, QWidget * parent = NULL);
		~WaypointListDialog();
		void build_model(bool hide_layer_names);

		std::list<Waypoint *> * waypoints = NULL;

	private slots:
		void waypoint_view_cb(void);
		// void waypoint_select_cb(void);
		void waypoint_properties_cb(void);
		void show_picture_waypoint_cb(void);

		void copy_selected_only_visible_columns_cb(void);
		void copy_selected_with_position_cb(void);
		void accept_cb(void);

	private:
		void add_row(Waypoint * wp, HeightUnit height_units, const QString & date_format);
		void contextMenuEvent(QContextMenuEvent * event);
		void add_menu_items(QMenu & menu, bool wp_has_image);
		void add_copy_menu_items(QMenu & menu);
		void waypoint_select(LayerTRW * layer);

		QWidget * parent = NULL;
		QDialogButtonBox * button_box = NULL;
		QVBoxLayout * vbox = NULL;

		QStandardItemModel * model = NULL;
		QTableView * view = NULL;

		/* Waypoint selected in list. */
		Waypoint * selected_wp = NULL;
	};





} /* namespace SlavGPS */




#endif /* #ifndef _SG_WAYPOINT_LIST_H_ */
