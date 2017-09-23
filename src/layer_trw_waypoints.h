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




#include <unordered_map>




#include <QIcon>




#include "tree_view.h"
#include "bbox.h"
#include "coord.h"
#include "globals.h"




namespace SlavGPS {




	class Waypoint;
	class Viewport;




	/* Comment from viking, perhaps still applies:

	   It's not entirely clear the benefits of hash tables usage
	   here - possibly the simplicity of first implementation for
	   unique names.  Now with the name of the item stored as part
	   of the item - these tables are effectively straightforward
	   lists.

	   For this reworking I've choosen to keep the use of hash
	   tables since for the expected data sizes - even many hundreds
	   of waypoints and tracks is quite small in the grand scheme of
	   things, and with normal PC processing capabilities - it has
	   negligible performance impact.  This also minimized the
	   amount of rework - as the management of the hash tables
	   already exists.

	   The hash tables are indexed by simple integers acting as a
	   UUID hash, which again shouldn't affect performance much we
	   have to maintain a uniqueness (as before when multiple names
	   where not allowed), this is to ensure it refers to the same
	   item in the data structures used on the viewport and on the
	   layers panel.
	*/
	typedef std::unordered_map<sg_uid_t, Waypoint *> Waypoints;




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



		void find_maxmin(struct LatLon maxmin[2]);
		void uniquify(sort_order_t sort_order);
		QString new_unique_element_name(const QString & old_name);

		void rename_waypoint(Waypoint * wp, const QString & new_name);
		void reset_waypoint_icon(Waypoint * wp);

		/* Uses a case sensitive find. Finds the first waypoint matching given name. */
		Waypoint * find_waypoint_by_name(const QString & wp_name);

		Waypoint * find_waypoint_by_date(char const * date);

		void calculate_bounds();


		time_t get_earliest_timestamp();


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




	QIcon * get_wp_sym_small(const QString & symbol_name);




}




#endif /* #ifndef _SG_LAYER_TRW_WAYPOINTS_H_ */
