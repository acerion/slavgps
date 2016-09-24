/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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
 *
 */

#ifndef _SG_LAYER_AGGREGATE_H_
#define _SG_LAYER_AGGREGATE_H_




#include <cstdint>
#include <list>

#include "viklayer.h"
#include "vikviewport.h"



namespace SlavGPS {




	/* Forward declarations. */
	class LayersPanel;
#ifndef SLAVGPS_QT
	class Track;
	struct track_layer_t;
	struct waypoint_layer_t;

#endif



	class LayerAggregate : public Layer {

	public:
		LayerAggregate();
		LayerAggregate(Viewport * viewport);
		~LayerAggregate();


		/* Layer interface methods. */

		void draw(Viewport * viewport);
		char const * tooltip();
		void marshall(uint8_t ** data, int * len);
		void change_coord_mode(VikCoordMode mode);
		void drag_drop_request(Layer * src, GtkTreeIter * src_item_iter, GtkTreePath * dest_path);
		void add_menu_items(GtkMenu * menu, void * panel);
		void realize(TreeView * tree_view, QStandardItem * layer_item);


		void add_layer(Layer * layer, bool allow_reordering);
		void insert_layer(Layer * layer, QStandardItem * replace_item);
		void move_layer(GtkTreeIter * child_iter, bool up);
		bool delete_layer(GtkTreeIter * iter);
		void clear();


		Layer * get_top_visible_layer_of_type(LayerType layer_type);
		std::list<waypoint_layer_t *> * create_waypoints_and_layers_list();
		std::list<track_layer_t *> * create_tracks_and_layers_list();
		std::list<track_layer_t *> * create_tracks_and_layers_list(SublayerType sublayer_type);
		std::list<Layer *> * get_all_layers_of_type(std::list<Layer *> * layers, LayerType layer_type, bool include_invisible);
		bool is_empty();
		std::list<Layer const *> * get_children();


		void search_date();

		void child_visible_set(LayersPanel * panel, bool visible);
		void child_visible_toggle(LayersPanel * panel);

		std::list<Layer *> * children;

		/* One per layer. */
		GtkWidget * tracks_analysis_dialog;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_AGGREGATE_H_ */