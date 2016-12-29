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

#include "layer.h"
#include "layer_trw.h"
#include "viewport.h"




namespace SlavGPS {




	/* Forward declarations. */
	class LayersPanel;
	class Track;
	struct track_layer_t;
	struct waypoint_layer_t;




	class LayerAggregate : public Layer {
		Q_OBJECT
	public:
		LayerAggregate();
		LayerAggregate(Viewport * viewport);
		~LayerAggregate();


		/* Layer interface methods. */

		void draw(Viewport * viewport);
		QString tooltip();
		void marshall(uint8_t ** data, int * len);
		void change_coord_mode(VikCoordMode mode);
		void drag_drop_request(Layer * src, GtkTreeIter * src_item_iter, GtkTreePath * dest_path);
		void add_menu_items(QMenu & menu);
		void connect_to_tree(TreeView * tree_view, TreeIndex const & layer_index);


		void add_layer(Layer * layer, bool allow_reordering);
		void insert_layer(Layer * layer, TreeIndex const & replace_index);
		void move_layer(GtkTreeIter * child_iter, bool up);
		bool delete_layer(TreeIndex const & index);
		void clear();


		Layer * get_top_visible_layer_of_type(LayerType layer_type);
		std::list<waypoint_layer_t *> * create_waypoints_and_layers_list();
		std::list<track_layer_t *> * create_tracks_and_layers_list();
		std::list<track_layer_t *> * create_tracks_and_layers_list(SublayerType sublayer_type);
		std::list<Layer *> * get_all_layers_of_type(std::list<Layer *> * layers, LayerType layer_type, bool include_invisible);
		bool is_empty();
		std::list<Layer const *> * get_children();

		std::list<Layer *> * children = NULL;

	private:
		void child_visible_set(LayersPanel * panel, bool visible);

		/* One per layer. */
		GtkWidget * tracks_analysis_dialog = NULL;

	private slots:
		void child_visible_on_cb(void);
		void child_visible_off_cb(void);
		void child_visible_toggle_cb(void);
		void sort_a2z_cb(void);
		void sort_z2a_cb(void);
		void sort_timestamp_ascend_cb(void);
		void sort_timestamp_descend_cb(void);
		void analyse_cb(void);
		void track_list_dialog_cb(void);
		void waypoint_list_dialog_cb(void);
		void search_date_cb(void);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_AGGREGATE_H_ */
