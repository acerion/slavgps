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




#ifndef _SG_LAYER_TRW_MENU_H_
#define _SG_LAYER_TRW_MENU_H_




#include <QMenu>




namespace SlavGPS {




	class LayerTRW;




	void layer_trw_sublayer_menu_waypoint_track_route_properties(LayerTRW * parent_layer, QMenu & menu);
	void layer_trw_sublayer_menu_track_route_profile(LayerTRW * parent_layer, QMenu & menu);
	void layer_trw_sublayer_menu_waypoint_track_route_edit(LayerTRW * parent_layer, QMenu & menu);
	void layer_trw_sublayer_menu_waypoint_misc(LayerTRW * parent_layer, QMenu & menu);
	void layer_trw_sublayer_menu_waypoints_tracks_routes_paste(LayerTRW * parent_layer, QMenu & menu);
	void layer_trw_sublayer_menu_waypoints_waypoint_new(LayerTRW * parent_layer, QMenu & menu);
	void layer_trw_sublayer_menu_waypoints_A(LayerTRW * parent_layer, QMenu & menu);
	void layer_trw_sublayer_menu_tracks_A(LayerTRW * parent_layer, QMenu & menu);
	void layer_trw_sublayer_menu_routes_A(LayerTRW * parent_layer, QMenu & menu);
	void layer_trw_sublayer_menu_tracks_routes_waypoints_sort(LayerTRW * parent_layer, QMenu & menu);
	void layer_trw_sublayer_menu_track_route_misc(LayerTRW * parent_layer, QMenu & menu, QMenu * upload_submenu);
	void layer_trw_sublayer_menu_track_waypoint_diary_astro(LayerTRW * parent_layer, QMenu & menu, QMenu * external_submenu);
	void layer_trw_sublayer_menu_all_add_external_tools(LayerTRW * parent_layer, QMenu & menu, QMenu * external_submenu);
	void layer_trw_sublayer_menu_route_google_directions(LayerTRW * parent_layer, QMenu & menu);
	void layer_trw_sublayer_menu_track_misc(LayerTRW * parent_layer, QMenu & menu, QMenu * upload_submenu);
	void layer_trw_sublayer_menu_track_route_edit_trackpoint(LayerTRW * parent_layer, QMenu & menu);
	void layer_trw_sublayer_menu_waypoints_waypoint_transform(LayerTRW * parent_layer, QMenu & menu);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_TRW_MENU_H_ */
