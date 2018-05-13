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




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif




#include <unordered_map>




#include <QMenu>




#include "layer_trw_definitions.h"
#include "layer_trw_track.h"
#include "tree_view.h"
#include "bbox.h"
#include "coord.h"




namespace SlavGPS {




	class Track;
	class Viewport;
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
	typedef std::unordered_map<sg_uid_t, Track *> Tracks;




	class TrackpointSearch {
	public:
		TrackpointSearch(int ev_x, int ev_y, Viewport * viewport);

		/* Input. */
		int x = 0;
		int y = 0;
		Viewport * viewport = NULL;
		LatLonBBox bbox;

		/* Output. */
		int closest_x = 0;
		int closest_y = 0;
		Track * closest_track = NULL;
		Trackpoint * closest_tp = NULL;

		TrackPoints::iterator closest_tp_iter;
	};




	class LayerTRWTracks : public TreeItem {
		Q_OBJECT
	public:
		LayerTRWTracks();
		LayerTRWTracks(bool is_routes);
		LayerTRWTracks(TreeView * ref_tree_view);
		LayerTRWTracks(bool is_routes, TreeView * ref_tree_view);
		~LayerTRWTracks();

		QString get_tooltip();

		void add_children_to_tree(void);


		/* Get track by name - not guaranteed to be unique. Finds the first one matching the name. */
		Track * find_track_by_name(const QString & trk_name);
		Track * find_track_by_date(char const * date_str);

		void uniquify(TreeViewSortOrder sort_order);
		QString new_unique_element_name(const QString & old_name);

		/* Update the tree view of the track id - primarily to update the icon. */
		void update_tree_view(Track * trk);

		void assign_colors(LayerTRWTrackDrawingMode track_drawing_mode, const QColor & track_color_common);


		time_t get_earliest_timestamp();



		void list_trk_uids(std::list<sg_uid_t> & list);

		std::list<sg_uid_t> find_tracks_with_timestamp_type(bool with_timestamps, Track * exclude);
		std::list<Track *> find_nearby_tracks_by_time(Track * orig_trk, unsigned int threshold);

		/* Get list of pointers to tracks, sorted by name.  If
		   @param exclude is not NULL, track specified by
		   @exclude won't be included in returned list. */
		std::list<Track *> get_sorted_by_name(const Track * exclude = NULL) const;

		Track * find_track_with_duplicate_name(void) const;


		void set_items_visibility(bool on_off);
		void toggle_items_visibility();

		void get_tracks_list(std::list<Track *> & list);
		void track_search_closest_tp(TrackpointSearch * search);

 		void change_coord_mode(CoordMode dest_mode);



		bool add_context_menu_items(QMenu & menu, bool tree_view_context_menu);
		void sublayer_menu_tracks_misc(LayerTRW * parent_layer_, QMenu & menu);
		void sublayer_menu_routes_misc(LayerTRW * parent_layer_, QMenu & menu);
		void sublayer_menu_sort(QMenu & menu);


		bool handle_selection_in_tree(void);

		void draw_tree_item(Viewport * viewport, bool highlight_selected, bool parent_is_selected);

		/* Similar to C++ container's ::clear() method: call
		   destructor for all elements of this container,
		   remove the elements, leaving zero elements in the
		   container. */
		void clear(void);


		void add_track(Track * trk);
		void add_track_to_data_structure_only(Track * trk);

		/*
		  Delete a single track from container and from main items tree.
		  Delete the passed object as well.
		  Return value of track's "visible" property before it was deleted.
		*/
		bool delete_track(Track * trk);

		void recalculate_bbox(void);
		LatLonBBox get_bbox(void) const { return this->bbox; };

		Tracks items;


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
	};




}




#endif /* #ifndef _SG_LAYER_TRW_TRACKS_H_ */
