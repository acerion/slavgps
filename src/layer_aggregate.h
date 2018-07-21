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
 */

#ifndef _SG_LAYER_AGGREGATE_H_
#define _SG_LAYER_AGGREGATE_H_




#include <list>

#include <QDialog>

#include "variant.h"
#include "layer.h"
#include "layer_interface.h"




namespace SlavGPS {




	/* Forward declarations. */
	class Viewport;
	class LayersPanel;
	class Track;
	class Waypoint;




	class LayerAggregateInterface : public LayerInterface {
	public:
		LayerAggregateInterface();
		Layer * unmarshall(Pickle & pickle, Viewport * viewport);
	};




	class LayerAggregate : public Layer {
		Q_OBJECT
	public:
		LayerAggregate();
		~LayerAggregate();


		/* Layer interface methods. */

		void draw_tree_item(Viewport * viewport, bool highlight_selected, bool parent_is_selected);
		QString get_tooltip(void) const;
		void marshall(Pickle & pickle);
		void change_coord_mode(CoordMode mode);
		void drag_drop_request(Layer * src, TreeIndex & src_item_index, void * GtkTreePath_dest_path);
		void add_menu_items(QMenu & menu);
		void add_children_to_tree(void);


		void add_layer(Layer * layer, bool allow_reordering);
		void insert_layer(Layer * layer, const Layer * sibling_layer);

		/* Move child item (direct/immediate child of given
		   aggregate layer) up or down in list of the child
		   item's peers. This change is made only to aggregate
		   layer's container of child items. */
		bool change_child_item_position(TreeItem * child_tree_index, bool up);

		bool delete_layer(Layer * layer);
		void clear();


		void get_waypoints_list(std::list<Waypoint *> & list);
		void get_tracks_list(std::list<Track *> & list, const QString & type_id);

		Layer * get_top_visible_layer_of_type(LayerType layer_type);
		void get_all_layers_of_type(std::list<const Layer *> & layers, LayerType layer_type, bool include_invisible);
		bool handle_select_tool_click(QMouseEvent * event, Viewport * viewport, LayerTool * tool);
		bool handle_select_tool_double_click(QMouseEvent * event, Viewport * viewport, LayerTool * tool);

		int get_child_layers_count(void) const;
		std::list<Layer const *> get_child_layers(void) const;

		std::list<Layer *> * children = NULL;

	private:
		void child_visible_set(LayersPanel * panel, bool visible);

	public slots:
		/* Aggregate Layer can contain other layers and should be notified about changes in them. */
		void child_layer_changed_cb(const QString & child_layer_name);

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
