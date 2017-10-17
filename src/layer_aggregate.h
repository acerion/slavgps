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

#include "layer.h"
#include "layer_interface.h"




namespace SlavGPS {




	/* Forward declarations. */
	class Viewport;
	class LayersPanel;
	class Track;




	class LayerAggregateInterface : public LayerInterface {
	public:
		LayerAggregateInterface();
		Layer * unmarshall(uint8_t * data, size_t data_len, Viewport * viewport);
	};




	class LayerAggregate : public Layer {
		Q_OBJECT
	public:
		LayerAggregate();
		~LayerAggregate();


		/* Layer interface methods. */

		void draw(Viewport * viewport);
		QString get_tooltip();
		void marshall(uint8_t ** data, size_t * data_len);
		void change_coord_mode(CoordMode mode);
		void drag_drop_request(Layer * src, TreeIndex & src_item_index, void * GtkTreePath_dest_path);
		void add_menu_items(QMenu & menu);
		void add_children_to_tree(void);


		void add_layer(Layer * layer, bool allow_reordering);
		void insert_layer(Layer * layer, TreeIndex const & replace_index);
		void move_layer(TreeIndex & child_index, bool up);
		bool delete_layer(TreeIndex const & index);
		void clear();


		Layer * get_top_visible_layer_of_type(LayerType layer_type);
		std::list<Waypoint *> * create_waypoints_list();
		std::list<Track *> * create_tracks_list();
		std::list<Track *> * create_tracks_list(const QString & type_id);
		std::list<Layer const *> * get_all_layers_of_type(std::list<Layer const *> * layers, LayerType layer_type, bool include_invisible);
		bool handle_select_tool_click(QMouseEvent * event, Viewport * viewport, LayerTool * tool);
		bool handle_select_tool_double_click(QMouseEvent * event, Viewport * viewport, LayerTool * tool);
		bool is_empty();
		std::list<Layer const *> * get_children();

		std::list<Layer *> * children = NULL;

	private:
		void child_visible_set(LayersPanel * panel, bool visible);


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
