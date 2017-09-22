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




	class Track;




	class TrackpointSearch {
	public:
		int x = 0;
		int y = 0;
		int closest_x = 0;
		int closest_y = 0;
		Track * closest_track = NULL;
		Trackpoint * closest_tp = NULL;
		Viewport * viewport = NULL;
		TrackPoints::iterator closest_tp_iter;
		LatLonBBox bbox;
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


		void calculate_bounds_tracks(void);


		Track * find_track_by_name(const QString & trk_name);
		Track * find_track_by_date(char const * date_str);

		void find_maxmin_in_tracks(struct LatLon maxmin[2]);

		void list_trk_uids(GList ** l);

		std::list<sg_uid_t> * find_tracks_with_timestamp_type(bool with_timestamps, Track * exclude);
		GList * find_nearby_tracks_by_time(Track * orig_trk, unsigned int threshold);
		std::list<QString> get_sorted_track_name_list();
		std::list<QString> get_sorted_track_name_list_exclude_self(Track const * self);

		QString has_duplicate_track_names(void);


		void set_tracks_visibility(bool on_off);
		void tracks_toggle_visibility();

		std::list<Track *> * get_track_values(std::list<Track *> * target);
		void track_search_closest_tp(TrackpointSearch * search);

 		void change_coord_mode(CoordMode dest_mode);




		Tracks tracks;
		LatLonBBox bbox;
	};




}




#endif /* #ifndef _SG_LAYER_TRW_TRACKS_H_ */
