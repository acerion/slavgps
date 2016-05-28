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

#ifndef _VIKING_AGGREGATELAYER_H
#define _VIKING_AGGREGATELAYER_H

#include <glib.h>
#include <stdbool.h>
#include <stdint.h>

#include <list>


#include "viklayer.h"


#ifdef __cplusplus
extern "C" {
#endif


#define VIK_AGGREGATE_LAYER_TYPE            (vik_aggregate_layer_get_type ())
#define VIK_AGGREGATE_LAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_AGGREGATE_LAYER_TYPE, VikAggregateLayer))
#define VIK_AGGREGATE_LAYER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_AGGREGATE_LAYER_TYPE, VikAggregateLayerClass))
#define IS_VIK_AGGREGATE_LAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_AGGREGATE_LAYER_TYPE))
#define IS_VIK_AGGREGATE_LAYER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_AGGREGATE_LAYER_TYPE))

typedef struct _VikAggregateLayerClass VikAggregateLayerClass;
struct _VikAggregateLayerClass
{
	VikLayerClass vik_layer_class;
};

GType vik_aggregate_layer_get_type();

typedef struct {
	VikLayer vl;
} VikAggregateLayer;


VikAggregateLayer *vik_aggregate_layer_new();
void vik_aggregate_layer_free(VikAggregateLayer *val);
VikAggregateLayer *vik_aggregate_layer_create(Viewport * viewport);
//bool vik_aggregate_layer_load_layers(VikAggregateLayer *val, FILE *f, void * vp);


#ifdef __cplusplus
}
#endif



/* Forward declarations. */
struct _VikLayersPanel;
typedef struct _VikLayersPanel VikLayersPanel;


namespace SlavGPS {





	class LayerAggregate : public Layer {

	public:
		LayerAggregate(VikLayer * vl);


		/* Layer interface methods. */

		void draw(Viewport * viewport);
		char const * tooltip();
		void marshall(uint8_t ** data, int * len);
		void change_coord_mode(VikCoordMode mode);
		void drag_drop_request(Layer * src, GtkTreeIter * src_item_iter, GtkTreePath * dest_path);
		void add_menu_items(GtkMenu * menu, void * vlp);
		void realize(VikTreeview * vt, GtkTreeIter * layer_iter);
		void free_();


		void add_layer(Layer * layer, bool allow_reordering);
		void insert_layer(Layer * layer, GtkTreeIter *replace_iter);
		void move_layer(GtkTreeIter * child_iter, bool up);
		bool delete_layer(GtkTreeIter * iter);
		void clear();


		Layer * get_top_visible_layer_of_type(VikLayerTypeEnum type);
		GList * waypoint_create_list();
		GList * track_create_list();
		std::list<Layer *> * get_all_layers_of_type(std::list<Layer *> * layers, VikLayerTypeEnum type, bool include_invisible);
		bool is_empty();
		const std::list<Layer *> * get_children();

		void search_date();

		void child_visible_set(VikLayersPanel * vlp, bool visible);
		void child_visible_toggle(VikLayersPanel * vlp);

		std::list<Layer *> * children;

		// One per layer
		GtkWidget * tracks_analysis_dialog;

	};





}





#endif
