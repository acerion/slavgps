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

#ifndef _SG_LAYER_TRW_TRACKS_H_
#define _SG_LAYER_TRW_TRACKS_H_




#include <unordered_map>




#include <QMenu>




#include "measurements.h"
#include "layer_trw_definitions.h"
#include "layer_trw_track.h"
#include "tree_view.h"
#include "bbox.h"
#include "coord.h"
#include "viewport.h"




namespace SlavGPS {




	class Track;
	class GisViewport;
	class LayerTRW;
	class Window;




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
	typedef std::unordered_map<sg_uid_t, Track *> TracksMap;




	class TrackpointSearch {
	public:
		TrackpointSearch(int ev_x, int ev_y, GisViewport * gisview);

		/* Input. */
		int x = 0;
		int y = 0;
		GisViewport * gisview = NULL;
		LatLonBBox bbox;
		/* A trackpoint that we want to ignore during
		   search. Used by code searching for snap coordinates
		   to avoid snapping to ourselves. */
		Trackpoint * skip_tp = NULL;

		/* Output. */
		ScreenPos closest_pos;
		Track * closest_track = NULL;
		Trackpoint * closest_tp = NULL;

		TrackPoints::iterator closest_tp_iter;
	};




	class LayerTRWTracks : public TreeItem {
		Q_OBJECT

		friend class LayerTRW;
	public:
		LayerTRWTracks(bool is_routes);
		LayerTRWTracks(TreeView * ref_tree_view);
		LayerTRWTracks(bool is_routes, TreeView * ref_tree_view);
		~LayerTRWTracks();

		QString get_tooltip(void) const;

		sg_ret attach_children_to_tree(void);

		virtual TreeItemType get_tree_item_type(void) const override { return TreeItemType::Sublayer; }


		/* Get track by name - not guaranteed to be unique. Finds the first one matching the name. */
		Track * find_track_by_name(const QString & trk_name);
		std::list<TreeItem *> get_tracks_by_date(const QDate & search_date) const;

		Track * find_child_by_uid(sg_uid_t uid) const;

		void uniquify(TreeViewSortOrder sort_order);
		QString new_unique_element_name(const QString & existing_name);

		void assign_colors(LayerTRWTrackDrawingMode track_drawing_mode, const QColor & track_color_common);


		Time get_earliest_timestamp(void) const;



		void list_trk_uids(std::list<sg_uid_t> & list);

		std::list<Track *> find_tracks_with_timestamp_type(bool with_timestamps, Track * exclude);
		std::list<Track *> find_nearby_tracks_by_time(Track * orig_trk, const Duration & threshold);

		/* Get list of pointers to tracks, sorted by name.  If
		   @param exclude is not NULL, track specified by
		   @exclude won't be included in returned list. */
		std::list<Track *> get_sorted_by_name(const Track * exclude = NULL) const;

		Track * find_track_with_duplicate_name(void) const;


		void set_items_visibility(bool on_off);
		void toggle_items_visibility();

		/* Get tree items (direct and indirect children of the
		   layer) of given type @param type_id (for now only
		   tracks/routes). */
		void get_tree_items(std::list<Track *> & list) const;

		void track_search_closest_tp(TrackpointSearch & search) const;

 		void change_coord_mode(CoordMode dest_mode);



		bool add_context_menu_items(QMenu & menu, bool tree_view_context_menu);
		void sublayer_menu_tracks_misc(LayerTRW * parent_layer_, QMenu & menu);
		void sublayer_menu_routes_misc(LayerTRW * parent_layer_, QMenu & menu);
		void sublayer_menu_sort(QMenu & menu);

		sg_ret drag_drop_request(TreeItem * tree_item, int row, int col);
		sg_ret dropped_item_is_acceptable(TreeItem * tree_item, bool * result) const;

		bool handle_selection_in_tree(void);

		void draw_tree_item(GisViewport * gisview, bool highlight_selected, bool parent_is_selected);

		bool move_child(TreeItem & child_tree_item, bool up) override;

		/* Similar to C++ container's ::clear() method: call
		   destructor for all elements of this container,
		   remove the elements, leaving zero elements in the
		   container. */
		void clear(void);

		/* Similar to C++ container's ::size() method. */
		size_t size(void) const;

		/* Similar to C++ container's ::empty() method. */
		bool empty(void) const;


		sg_ret attach_to_container(Track * trk);

		/* Delete a single track from container (but not from main items tree).
		   Return value of track's "visible" property before it was deleted. */
		sg_ret detach_from_container(Track * trk, bool * was_visible);

		void recalculate_bbox(void);
		LatLonBBox get_bbox(void) const { return this->bbox; };


	public slots:
		void move_viewport_to_show_all_cb(void);
		void items_visibility_on_cb(void);
		void items_visibility_off_cb(void);
		void items_visibility_toggle_cb(void);

		void track_list_dialog_cb(void);

		void paste_sublayer_cb(void);

		void sort_order_a2z_cb(void);
		void sort_order_z2a_cb(void);
		void sort_order_timestamp_ascend_cb(void);
		void sort_order_timestamp_descend_cb(void);

	private:
		LatLonBBox bbox;

		std::list<Track *> children_list;
		TracksMap children_map;

		void init_item(void);
	};




}




#endif /* #ifndef _SG_LAYER_TRW_TRACKS_H_ */
