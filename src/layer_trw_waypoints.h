/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (c) 2011-2013, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2020, Kamil Ignacak <acerion@wp.pl>
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




#include <unordered_map>
#include <list>




#include <QIcon>
#include <QMenu>




#include "measurements.h"
#include "tree_view.h"
#include "bbox.h"
#include "coord.h"
#include "viewport.h"




namespace SlavGPS {




	class Waypoint;
	class GisViewport;
	class LayerTRW;
	class Window;
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




	class WaypointSearch {
	public:
		WaypointSearch(const ScreenPos & event_pos, GisViewport * gisview) :
			m_event_pos(event_pos),
			m_gisview(gisview) {};

		/* Input. */
		const ScreenPos m_event_pos;
		GisViewport * m_gisview = nullptr;
		/* A waypoint that we want to ignore during
		   search. Used by code searching for snap coordinates
		   to avoid snapping to ourselves. */
		Waypoint * skip_wp = nullptr;

		/* Output. */
		ScreenPos closest_pos;
		Waypoint * closest_wp = nullptr;
	};





	class LayerTRWWaypoints : public TreeItem {
		Q_OBJECT

		friend class LayerTRW;
	public:
		LayerTRWWaypoints();
		LayerTRWWaypoints(TreeView * ref_tree_view);
		~LayerTRWWaypoints();

		QString get_tooltip(void) const;

		sg_ret attach_unattached_children(void) override;


		void uniquify(TreeViewSortOrder sort_order);
		QString new_unique_element_name(const QString & existing_name);

		/* Uses a case sensitive find. Finds the first waypoint matching given name. */
		Waypoint * find_waypoint_by_name(const QString & wp_name);


		std::list<TreeItem *> find_children_by_date(const QDate & search_date) const;


		Time get_earliest_timestamp(void) const;

		void apply_dem_data_common(bool skip_existing_elevations);

		std::list<Waypoint *> children_list(const Waypoint * exclude = nullptr) const;
		std::list<Waypoint *> children_list_sorted_by_name(const Waypoint * exclude = nullptr) const;

		Waypoint * find_waypoint_with_duplicate_name(void) const;

		void search_closest_wp(WaypointSearch & search);
		QString tool_show_picture_wp(const ScreenPos & event_pos, GisViewport * gisview);
		QStringList get_list_of_missing_thumbnails(void) const;
		void change_coord_mode(CoordMode new_mode);

		sg_ret menu_add_type_specific_operations(QMenu & menu, bool in_tree_view) override;
		void sublayer_menu_waypoints_misc(LayerTRW * parent_layer_, QMenu & menu);
		void sublayer_menu_sort(QMenu & menu);

		sg_ret update_properties(void) override;

		sg_ret accept_dropped_child(TreeItem * tree_item, int row) override;

		bool handle_selection_in_tree(void);

		void draw_tree_item(GisViewport * gisview, bool highlight_selected, bool parent_is_selected);

		void recalculate_bbox(void);
		LatLonBBox get_bbox(void) const { return this->bbox; };

		SGObjectTypeID get_type_id(void) const override;
		static SGObjectTypeID type_id(void);

		DefaultNameGenerator name_generator;


		/**
		   @brief Deselect current waypoint, select next trackpoint

		   If it's impossible to change selection, return
		   value other than sg_ret::ok.
		*/
		sg_ret move_selection_to_next_child(void);

		/**
		   @brief Deselect current waypoint, select previous trackpoint

		   If it's impossible to change selection, return
		   value other than sg_ret::ok.
		*/
		sg_ret move_selection_to_previous_child(void);

		/**
		   @brief Simple accessors
		*/
		LayerTRW * owner_trw_layer(void) const;
		Layer * parent_layer(void) const override;

	public slots:
		void move_viewport_to_show_all_cb(void);

		void children_visibility_on_cb(void);
		void children_visibility_off_cb(void);
		void children_visibility_toggle_cb(void);

		void apply_dem_data_all_cb(void);
		void apply_dem_data_only_missing_cb(void);

		sg_ret paste_child_tree_item_cb(void) override;

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
