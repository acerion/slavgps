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
#include <QMenu>




#include "tree_view.h"
#include "bbox.h"
#include "coord.h"




namespace SlavGPS {




	class Waypoint;
	class Viewport;
	class LayerTRW;
	class Window;
	class LayersPanel;
	class LayerTRWWaypoints;




	class DefaultNameGenerator {
	public:
		QString try_new_name(void) const;
		void add_name(const QString & item_name);
		void remove_name(const QString & item_name);
		void reset();

		void set_parent_sublayer(LayerTRWWaypoints * parent_sublayer) { this->parent = parent_sublayer; };

	private:
		int name_to_number(const QString & item_name) const;
		QString number_to_name(int number) const;

		int highest_item_number = 0;
		LayerTRWWaypoints * parent = NULL;
	};




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
	typedef std::unordered_map<sg_uid_t, Waypoint *> WaypointsContainer;




	class WaypointSearch {
	public:
		WaypointSearch(int ev_x, int ev_y, Viewport * viewport_, bool draw_images_) :
			x(ev_x),
			y(ev_y),
			draw_images(draw_images_),
			viewport(viewport_) {};

		/* Input. */
		int x = 0;
		int y = 0;
		bool draw_images = false;
		Viewport * viewport = NULL;

		/* Output. */
		int closest_x = 0;
		int closest_y = 0;
		Waypoint * closest_wp = NULL;
	};





	class LayerTRWWaypoints : public TreeItem {
		Q_OBJECT
	public:
		LayerTRWWaypoints();
		LayerTRWWaypoints(TreeView * ref_tree_view);
		~LayerTRWWaypoints();

		QString get_tooltip(void) const;


		void add_children_to_tree(void);


		void uniquify(TreeViewSortOrder sort_order);
		QString new_unique_element_name(const QString & old_name);

		void update_tree_view(const Waypoint * wp);
		void propagate_new_waypoint_name(const Waypoint * wp);
		void set_new_waypoint_icon(const Waypoint * wp);

		/* Uses a case sensitive find. Finds the first waypoint matching given name. */
		Waypoint * find_waypoint_by_name(const QString & wp_name);

		Waypoint * find_waypoint_by_date(char const * date);




		time_t get_earliest_timestamp();

		void apply_dem_data_common(bool skip_existing_elevations);


		void list_wp_uids(std::list<sg_uid_t> & list);
		std::list<Waypoint *> get_sorted_by_name(void) const;
		Waypoint * find_waypoint_with_duplicate_name(void) const;
		void set_items_visibility(bool on_off);
		void toggle_items_visibility();
		void search_closest_wp(WaypointSearch * search);
		QString tool_show_picture_wp(int event_x, int event_y, Viewport * viewport);
		QStringList get_list_of_missing_thumbnails(void) const;
		void change_coord_mode(CoordMode new_mode);


		bool add_context_menu_items(QMenu & menu, bool tree_view_context_menu);
		void sublayer_menu_waypoints_misc(LayerTRW * parent_layer_, QMenu & menu);
		void sublayer_menu_sort(QMenu & menu);


		bool handle_selection_in_tree(void);

		void draw_tree_item(Viewport * viewport, bool highlight_selected, bool parent_is_selected);

		/* Similar to C++ container's ::clear() method: call
		   destructor for all elements of this container,
		   remove the elements, leaving zero elements in the
		   container. */
		void clear(void);


		void add_waypoint(Waypoint * wp);

		/*
		  Delete a single waypoint from container and from main items tree.
		  Delete the passed object as well.
		  Return value of waypoint's "visible" property before it was deleted.
		*/
		bool delete_waypoint(Waypoint * wp);

		void recalculate_bbox(void);
		LatLonBBox get_bbox(void) const { return this->bbox; };

		WaypointsContainer items;


		DefaultNameGenerator name_generator;



	public slots:
		void move_viewport_to_show_all_cb(void);

		void items_visibility_on_cb(void);
		void items_visibility_off_cb(void);
		void items_visibility_toggle_cb(void);

		void apply_dem_data_all_cb(void);
		void apply_dem_data_only_missing_cb(void);

		void paste_sublayer_cb(void);

		void sort_order_a2z_cb(void);
		void sort_order_z2a_cb(void);
		void sort_order_timestamp_ascend_cb(void);
		void sort_order_timestamp_descend_cb(void);

	private:
		LatLonBBox bbox;
	};




	QIcon get_wp_icon_small(const QString & symbol_name);




}




#endif /* #ifndef _SG_LAYER_TRW_WAYPOINTS_H_ */
