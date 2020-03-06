/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _SG_LAYER_AGGREGATE_H_
#define _SG_LAYER_AGGREGATE_H_




#include <list>




#include <QDialog>




#include "variant.h"
#include "layer.h"
#include "layer_interface.h"




namespace SlavGPS {




	/* Forward declarations. */
	class GisViewport;
	class Track;
	class Waypoint;




	class LayerAggregateInterface : public LayerInterface {
	public:
		LayerAggregateInterface();
		Layer * unmarshall(Pickle & pickle, GisViewport * gisview);
	};




	class LayerAggregate : public Layer {
		Q_OBJECT
	public:
		LayerAggregate();
		~LayerAggregate();


		void draw_tree_item(GisViewport * gisview, bool highlight_selected, bool parent_is_selected);
		QString get_tooltip(void) const;
		void marshall(Pickle & pickle);
		void change_coord_mode(CoordMode mode);
		sg_ret menu_add_type_specific_operations(QMenu & menu, bool in_tree_view) override;
		sg_ret post_read_2(void) override;

		sg_ret accept_dropped_child(TreeItem * tree_item, int row, int col) override;
		bool dropped_item_is_acceptable(const TreeItem & tree_item) const override;


		sg_ret add_child_item(TreeItem * child_tree_item) override;
		sg_ret delete_child_item(TreeItem * child_tree_item, bool confirm_deleting) override;


		void clear(void);



		sg_ret get_tree_items(std::list<TreeItem *> & list, const std::list<SGObjectTypeID> & wanted_types) const override;

		Layer * get_top_visible_layer_of_type(LayerKind layer_kind);
		void get_all_layers_of_kind(std::list<const Layer *> & layers, LayerKind layer_kind, bool include_invisible) const;
		bool handle_select_tool_click(QMouseEvent * event, GisViewport * gisview, LayerToolSelect * select_tool);
		bool handle_select_tool_double_click(QMouseEvent * event, GisViewport * gisview, LayerToolSelect * select_tool);

		bool is_top_level_layer(void) const;

		int child_rows_count(void) const override;
		std::list<Layer const *> get_child_layers(void) const;

	private slots:
		void children_visibility_on_cb(void);
		void children_visibility_off_cb(void);
		void children_visibility_toggle_cb(void);
		void sort_a2z_cb(void);
		void sort_z2a_cb(void);
		void sort_timestamp_ascend_cb(void);
		void sort_timestamp_descend_cb(void);
		void analyse_cb(void);

		/**
		   @brief Show all TRW Tracks and TRW routes from all
		   TRW layers laying directly and indirectly in this
		   aggregate layer
		*/
		void track_and_route_list_dialog_cb(void);

		void waypoint_list_dialog_cb(void);
		void search_date_cb(void);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_AGGREGATE_H_ */
