/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (c) 2011-2013, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_LAYER_TRW_WAYPOINTS_H_
#define _SG_LAYER_TRW_WAYPOINTS_H_




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

//#include <cstdint>
//#include <list>
//#include <unordered_map>

//#include <QStandardItem>
//#include <QDialog>

//#include "layer.h"
//#include "layer_tool.h"
//#include "layer_interface.h"
//#include "layer_trw_containers.h"
//#include "layer_trw_dialogs.h"
//#include "viewport.h"
//#include "waypoint.h"
//#include "trackpoint_properties.h"
//#include "file.h"
#include "tree_view.h"
#include "bbox.h"
#include "coord.h"
#include "layer_trw_containers.h"




namespace SlavGPS {




	class Waypoint;




	class WaypointSearch {
	public:
		int x = 0;
		int y = 0;
		int closest_x = 0;
		int closest_y = 0;
		bool draw_images = false;
		Waypoint * closest_wp = NULL;
		Viewport * viewport = NULL;
	};





	class LayerTRWWaypoints : public TreeItem {
		Q_OBJECT
	public:
		LayerTRWWaypoints();
		LayerTRWWaypoints(TreeView * ref_tree_view);
		~LayerTRWWaypoints();

		QString get_tooltip();

		Waypoint * find_waypoint_by_name(const QString & wp_name);
		Waypoint * find_waypoint_by_date(char const * date);

		void find_maxmin(struct LatLon maxmin[2]);


		void single_waypoint_jump(Viewport * viewport);
		void list_wp_uids(GList ** l);
		std::list<QString> get_sorted_wp_name_list();
		QString has_duplicate_waypoint_names();
		void set_waypoints_visibility(bool on_off);
		void waypoints_toggle_visibility();
		void search_closest_wp(WaypointSearch * search);
		QString tool_show_picture_wp(int event_x, int event_y, Viewport * viewport);
		QStringList * image_wp_make_list();
		void change_coord_mode(CoordMode new_mode);




		Waypoints waypoints;
		LatLonBBox bbox;
	};




}




#endif /* #ifndef _SG_LAYER_TRW_WAYPOINTS_H_ */
