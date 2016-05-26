/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013-2015, Rob Norris <rw_norris@hotmail.com>
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

#include "viking.h"
#include "vikgpslayer.h"
#include "viktrwlayer_analysis.h"
#include "viktrwlayer_tracklist.h"
#include "viktrwlayer_waypointlist.h"
#include "icons/icons.h"
#include "dialog.h"
#include "globals.h"

#include <list>

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <glib/gi18n.h>


static VikAggregateLayer *aggregate_layer_unmarshall(uint8_t *data, int len, Viewport * viewport);
static void vik_aggregate_layer_realize(VikAggregateLayer *val, VikTreeview *vt, GtkTreeIter *layer_iter);

VikLayerInterface vik_aggregate_layer_interface = {
	"Aggregate",
	N_("Aggregate"),
	"<control><shift>A",
	&vikaggregatelayer_pixbuf,

	NULL,
	0,

	NULL,
	0,
	NULL,
	0,

	VIK_MENU_ITEM_ALL,

	(VikLayerFuncCreate)                  vik_aggregate_layer_create,
	(VikLayerFuncRealize)                 vik_aggregate_layer_realize,
	(VikLayerFuncFree)                    vik_aggregate_layer_free,

	(VikLayerFuncUnmarshall)	      aggregate_layer_unmarshall,

	(VikLayerFuncSetParam)                NULL,
	(VikLayerFuncGetParam)                NULL,
	(VikLayerFuncChangeParam)             NULL,
};

GType vik_aggregate_layer_get_type()
{
	static GType val_type = 0;

	if (!val_type) {
		static const GTypeInfo val_info = {
			sizeof (VikAggregateLayerClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			NULL, /* class init */
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (VikAggregateLayer),
			0,
			NULL /* instance init */
		};
		val_type = g_type_register_static (VIK_LAYER_TYPE, "VikAggregateLayer", &val_info, (GTypeFlags) 0);
	}

	return val_type;
}

VikAggregateLayer * vik_aggregate_layer_create(Viewport * viewport)
{
	VikAggregateLayer *rv = vik_aggregate_layer_new();
	VikLayer * vl = (VikLayer *) rv;
	vik_layer_rename(vl, vik_aggregate_layer_interface.name);

	return rv;
}

void LayerAggregate::marshall(uint8_t **data, int *datalen)
{
#if 1

	uint8_t *ld;
	int ll;
	GByteArray* b = g_byte_array_new();
	int len;

#define alm_append(obj, sz) 	\
	len = (sz);						\
	g_byte_array_append(b, (uint8_t *)&len, sizeof(len));	\
	g_byte_array_append(b, (uint8_t *)(obj), len);

	VikLayer * vl = (VikLayer *) this->vl;
	vik_layer_marshall_params(vl, &ld, &ll);
	alm_append(ld, ll);
	free(ld);

	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		vik_layer_marshall((*child)->vl, &ld, &ll);
		if (ld) {
			alm_append(ld, ll);
			free(ld);
		}
	}
	*data = b->data;
	*datalen = b->len;
	g_byte_array_free(b, false);
#undef alm_append

#endif
}

static VikAggregateLayer * aggregate_layer_unmarshall(uint8_t *data, int len, Viewport * viewport)
{
#if 1

#define alm_size (*(int *)data)
#define alm_next		 \
	len -= sizeof(int) + alm_size;		\
	data += sizeof(int) + alm_size;

	VikAggregateLayer *rv = vik_aggregate_layer_new();
	LayerAggregate * aggregate = (LayerAggregate *) ((VikLayer *) rv)->layer;

	VikLayer * vl = (VikLayer *) rv;
	vik_layer_unmarshall_params(vl, data+sizeof(int), alm_size, viewport);
	alm_next;

	while (len>0) {
		VikLayer * child_layer = vik_layer_unmarshall(data + sizeof(int), alm_size, viewport);
		if (child_layer) {
			/* kamilFIXME: shouldn't we put "new LayerXYZ" in every _unmarshall() function? */
			Layer * new_layer = new Layer(child_layer);
			aggregate->children->push_front(new_layer);
			g_signal_connect_swapped(G_OBJECT(child_layer), "update", G_CALLBACK(vik_layer_emit_update_secondary), rv);
		}
		alm_next;
	}
	//  fprintf(stdout, "aggregate_layer_unmarshall ended with len=%d\n", len);
	return rv;
#undef alm_size
#undef alm_next

#endif
}

VikAggregateLayer *vik_aggregate_layer_new()
{
	VikAggregateLayer *val = (VikAggregateLayer *) g_object_new(VIK_AGGREGATE_LAYER_TYPE, NULL);

	VikLayer * vl = (VikLayer *) val;
	vik_layer_set_type(vl, VIK_LAYER_AGGREGATE);

	vl->layer = new LayerAggregate(vl);

	return val;
}

void LayerAggregate::insert_layer(Layer * layer, GtkTreeIter *replace_iter)
{
	GtkTreeIter iter;
	VikLayer * vl = (VikLayer *) this->vl;

	// By default layers are inserted above the selected layer
	bool put_above = true;

	// These types are 'base' types in that you what other information on top
	if (layer->vl->type == VIK_LAYER_MAPS || layer->vl->type == VIK_LAYER_DEM || layer->vl->type == VIK_LAYER_GEOREF) {
		put_above = false;
	}

	if (vl->realized) {
		vik_treeview_insert_layer(vl->vt, &(vl->iter), &iter, layer->vl->name, this->vl, put_above, layer, layer->vl->type, layer->vl->type, replace_iter, layer->get_timestamp());
		if (! layer->vl->visible) {
			vik_treeview_item_set_visible(vl->vt, &iter, false);
		}

		vik_layer_realize(layer->vl, vl->vt, &iter);

		if (this->children->empty()) {
			vik_treeview_expand(vl->vt, &(vl->iter));
		}
	}

	if (replace_iter) {
		Layer * existing_layer = (Layer *) vik_treeview_item_get_layer(vl->vt, replace_iter);

		auto theone = this->children->end();
		for (auto i = this->children->begin(); i != this->children->end(); i++) {
			if ((*i)->vl == existing_layer->vl) {
				theone = i;
			}
		}

		if (put_above) {
			this->children->insert(std::next(theone), layer);
		} else {
			// Thus insert 'here' (so don't add 1)
			this->children->insert(theone, layer);
		}
	} else {
		// Effectively insert at 'end' of the list to match how displayed in the treeview
		//  - but since it is drawn from 'bottom first' it is actually the first in the child list
		// This ordering is especially important if it is a map or similar type,
		//  which needs be drawn first for the layering draw method to work properly.
		// ATM this only happens when a layer is drag/dropped to the end of an aggregate layer
		this->children->push_back(layer);
	}

	g_signal_connect_swapped(G_OBJECT(layer->vl), "update", G_CALLBACK(vik_layer_emit_update_secondary), this->vl);
}

/**
 * vik_aggregate_layer_add_layer:
 * @allow_reordering: should be set for GUI interactions,
 *                    whereas loading from a file needs strict ordering and so should be false
 */
void LayerAggregate::add_layer(Layer * layer, bool allow_reordering)
{
	GtkTreeIter iter;
	VikLayer *vl = (VikLayer *) this->vl;

	// By default layers go to the top
	bool put_above = true;

	if (allow_reordering) {
		// These types are 'base' types in that you what other information on top
		if (layer->vl->type == VIK_LAYER_MAPS || layer->vl->type == VIK_LAYER_DEM || layer->vl->type == VIK_LAYER_GEOREF) {
			put_above = false;
		}
	}

	if (vl->realized) {
		vik_treeview_add_layer(vl->vt, &(vl->iter), &iter, layer->vl->name, this->vl, put_above, layer, layer->vl->type, layer->vl->type, layer->get_timestamp());
		if (! layer->vl->visible) {
			vik_treeview_item_set_visible(vl->vt, &iter, false);
		}

		vik_layer_realize(layer->vl, vl->vt, &iter);

		if (this->children->empty()) {
			vik_treeview_expand(vl->vt, &(vl->iter));
		}
	}

	if (put_above) {
		this->children->push_back(layer);
	} else {
		this->children->push_front(layer);
	}

	g_signal_connect_swapped(G_OBJECT(layer->vl), "update", G_CALLBACK(vik_layer_emit_update_secondary), this->vl);
}

void LayerAggregate::move_layer(GtkTreeIter *child_iter, bool up)
{
	auto theone = this->children->end();

	vik_treeview_move_item(this->vl->vt, child_iter, up);

	Layer * layer = (Layer *) vik_treeview_item_get_layer(this->vl->vt, child_iter);

	for (auto i = this->children->begin(); i != this->children->end(); i++) {
		if ((*i)->vl == layer->vl) {
			theone = i;
		}
	}

	if (up) {
		std::swap(*theone, *std::next(theone));
	} else {
		std::swap(*theone, *std::prev(theone));
	}

#if 0
	/* the old switcheroo */
	if (up && theone + 1 != val->children->end()) {

		VikLayer * tmp = *(theone + 1);
		*(theone + 1) = *(theone);
		*(theone) = tmp;
	} else if (!up && theone - 1 != val->children->end()) {

		VikLayer * tmp = *(theone - 1);
		*(theone - 1) = *(theone);

		first = theone->prev;
		second = theone;
	} else {
		return;
	}
#endif
}

/* Draw the aggregate layer. If vik viewport is in half_drawn mode, this means we are only
 * to draw the layers above and including the trigger layer.
 * To do this we don't draw any layers if in half drawn mode, unless we find the
 * trigger layer, in which case we pull up the saved pixmap, turn off half drawn mode and
 * start drawing layers.
 * Also, if we were never in half drawn mode, we save a snapshot
 * of the pixmap before drawing the trigger layer so we can use it again
 * later.
 */
void LayerAggregate::draw(Viewport * viewport)
{
	VikLayer * trigger = (VikLayer *) viewport->get_trigger();

	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		Layer * layer = *child;
		if (layer->vl == trigger) {
			if (viewport->get_half_drawn()) {
				viewport->set_half_drawn(false);
				viewport->snapshot_load();
			} else {
				viewport->snapshot_save();
			}
		}
		if (layer->vl->type == VIK_LAYER_AGGREGATE || layer->vl->type == VIK_LAYER_GPS || ! viewport->get_half_drawn()) {
			vik_layer_draw(layer->vl, viewport);
		}
	}


	fprintf(stderr, "%s:%d: children:\n", __FILE__, __LINE__);
	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		Layer * layer = *child;
		fprintf(stderr, "    name = '%s', ", layer->vl->name);
		if (layer->vl->type == VIK_LAYER_AGGREGATE) {
			fprintf(stderr, "type = LAYER AGGREGATE\n");

		} else if (layer->vl->type == VIK_LAYER_TRW) {
			fprintf(stderr, "type = LAYER TRW\n");
		} else if (layer->vl->type == VIK_LAYER_COORD) {
			fprintf(stderr, "type = LAYER COORD\n");
		} else if (layer->vl->type == VIK_LAYER_GEOREF) {
			fprintf(stderr, "type = LAYER GEOREF\n");
		} else if (layer->vl->type == VIK_LAYER_MAPS) {
			fprintf(stderr, "type = LAYER MAPS\n");
		} else if (layer->vl->type == VIK_LAYER_DEM) {
			fprintf(stderr, "type = LAYER DEM\n");
		} else if (layer->vl->type == VIK_LAYER_GPS) {
			fprintf(stderr, "type = LAYER GPS\n");
#ifdef HAVE_LIBMAPNIK
		} else if (layer->vl->type == VIK_LAYER_MAPNIK) {
			fprintf(stderr, "type = LAYER MAPNIK\n");
#endif
		} else if (layer->vl->type == VIK_LAYER_NUM_TYPES) {
			fprintf(stderr, "type = LAYER NUM_TYPES !!!!! \n");

		} else {
			fprintf(stderr, "type = %d !!!!!! \n", layer->vl->type);
		}
	}
}

void LayerAggregate::change_coord_mode(VikCoordMode mode)
{
	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		Layer * layer = *child;
		vik_layer_change_coord_mode(layer->vl, mode);
	}
}

// A slightly better way of defining the menu callback information
// This should be easier to extend/rework compared to previously
typedef enum {
	MA_VAL = 0,
	MA_VLP,
	MA_LAST
} menu_array_index;

typedef void * menu_array_values[MA_LAST];

static void aggregate_layer_child_visible_toggle(menu_array_values values)
{
	LayerAggregate * aggregate = (LayerAggregate *) values[MA_VAL];
	VikLayersPanel *vlp = VIK_LAYERS_PANEL (values[MA_VLP]);

	aggregate->child_visible_toggle(vlp);
}


void LayerAggregate::child_visible_toggle(VikLayersPanel * vlp)
{
	// Loop around all (child) layers applying visibility setting
	// This does not descend the tree if there are aggregates within aggregrate - just the first level of layers held
	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		Layer * layer = *child;
		layer->vl->visible = !layer->vl->visible;
		// Also set checkbox on/off
		vik_treeview_item_toggle_visible(vlp->panel_ref->get_treeview(), &(layer->vl->iter));
	}
	// Redraw as view may have changed
	vik_layer_emit_update(this->vl);
}

void LayerAggregate::child_visible_set(VikLayersPanel * vlp, bool on_off)
{
	// Loop around all (child) layers applying visibility setting
	// This does not descend the tree if there are aggregates within aggregrate - just the first level of layers held
	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		Layer * layer = *child;
		layer->vl->visible = on_off;
		// Also set checkbox on_off
		vik_treeview_item_set_visible(vlp->panel_ref->get_treeview(), &(layer->vl->iter), on_off);
	}

	// Redraw as view may have changed
	vik_layer_emit_update(this->vl);
}

static void aggregate_layer_child_visible_on(menu_array_values values)
{
	LayerAggregate * aggregate = (LayerAggregate *) values[MA_VAL];
	VikLayersPanel * vlp = VIK_LAYERS_PANEL (values[MA_VLP]);

	aggregate->child_visible_set(vlp, true);
}

static void aggregate_layer_child_visible_off(menu_array_values values)
{
	LayerAggregate * aggregate = (LayerAggregate *) values[MA_VAL];
	VikLayersPanel * vlp = VIK_LAYERS_PANEL (values[MA_VLP]);

	aggregate->child_visible_set(vlp, false);
}

/**
 * If order is true sort ascending, otherwise a descending sort
 */
static int sort_layer_compare(gconstpointer a, gconstpointer b, void * order)
{
	VikLayer *sa = (VikLayer *)a;
	VikLayer *sb = (VikLayer *)b;

	// Default ascending order
	int answer = g_strcmp0(sa->name, sb->name);

	if (KPOINTER_TO_INT(order)) {
		// Invert sort order for ascending order
		answer = -answer;
	}

	return answer;
}

static void aggregate_layer_sort_a2z(menu_array_values values)
{
#if 0
	LayerAggregate * aggregate = (LayerAggregate *) values[MA_VAL];
	VikLayer * vl = (VikLayer *) aggregate->vl;
	vik_treeview_sort_children(vl->vt, &vl->iter, VL_SO_ALPHABETICAL_ASCENDING);
	g_list_sort_with_data(aggregate->children, sort_layer_compare, KINT_TO_POINTER(true));
#endif
}

static void aggregate_layer_sort_z2a(menu_array_values values)
{
#if 0
	LayerAggregate * aggregate = (LayerAggregate *) values[MA_VAL];
	VikLayer * vl = (VikLayer *) aggregate->vl;
	vik_treeview_sort_children(vl->vt, &vl->iter, VL_SO_ALPHABETICAL_DESCENDING);
	g_list_sort_with_data(aggregate->children, sort_layer_compare, KINT_TO_POINTER(false));
#endif
}

/**
 * If order is true sort ascending, otherwise a descending sort
 */
static int sort_layer_compare_timestamp(gconstpointer a, gconstpointer b, void * order)
{
	VikLayer *sa = (VikLayer *)a;
	VikLayer *sb = (VikLayer *)b;

	// Default ascending order
	// NB This might be relatively slow...
	int answer = (((Layer *) sa->layer)->get_timestamp() > ((Layer *) sb->layer)->get_timestamp());

	if (KPOINTER_TO_INT(order)) {
		// Invert sort order for ascending order
		answer = !answer;
	}

	return answer;
}

static void aggregate_layer_sort_timestamp_ascend(menu_array_values values)
{
#if 0
	LayerAggregate * aggregate = (LayerAggregate *) values[MA_VAL];
	VikLayer * vl = (VikLayer *) aggregate->vl;
	vik_treeview_sort_children(vl->vt, &vl->iter, VL_SO_DATE_ASCENDING);
	g_list_sort_with_data(aggregate->children, sort_layer_compare_timestamp, KINT_TO_POINTER(true));
#endif
}

static void aggregate_layer_sort_timestamp_descend(menu_array_values values)
{
#if 0
	LayerAggregate * aggregate = (LayerAggregate *) values[MA_VAL];
	VikLayer * vl = (VikLayer *) aggregate->vl;
	vik_treeview_sort_children(vl->vt, &vl->iter, VL_SO_DATE_DESCENDING);
	g_list_sort_with_data(aggregate->children, sort_layer_compare_timestamp, KINT_TO_POINTER(false));
#endif
}

/**
 * aggregate_layer_waypoint_create_list:
 * @vl:        The layer that should create the waypoint and layers list
 * @user_data: Not used in this function
 *
 * Returns: A list of #vik_trw_waypoint_list_t
 */
static GList * aggregate_layer_waypoint_create_list(VikLayer *vl, void * user_data)
{
	LayerAggregate * layer = (LayerAggregate *) (vl->layer);
	return layer->waypoint_create_list();
}

GList * LayerAggregate::waypoint_create_list()
{
	// Get all TRW layers
	std::list<Layer *> * layers = new std::list<Layer *>;
	layers = this->get_all_layers_of_type(layers, VIK_LAYER_TRW, true);

	// For each TRW layers keep adding the waypoints to build a list of all of them
	GList *waypoints_and_layers = NULL;
	int index = 0;
	for (auto iter = layers->begin(); iter != layers->end(); iter++) {
		GList * waypoints = NULL;

		std::unordered_map<sg_uid_t, Waypoint *> & wps = VIK_TRW_LAYER(*iter)->trw->get_waypoints();
		for (auto i = wps.begin(); i != wps.end(); i++) {
			waypoints = g_list_insert(waypoints, i->second, index++);
		}

		waypoints_and_layers = g_list_concat(waypoints_and_layers, VIK_TRW_LAYER(*iter)->trw->build_waypoint_list_t(waypoints));
	}
	delete layers;

	return waypoints_and_layers;
}

static void aggregate_layer_waypoint_list_dialog(menu_array_values values)
{
	LayerAggregate * aggregate = (LayerAggregate *) values[MA_VAL];
	VikLayer * vl = (VikLayer *) aggregate->vl;
	char *title = g_strdup_printf(_("%s: Waypoint List"), vl->name);
	vik_trw_layer_waypoint_list_show_dialog(title, vl, NULL, aggregate_layer_waypoint_create_list, true);
	free(title);
}


static void aggregate_layer_search_date(menu_array_values values)
{
	LayerAggregate * aggregate = (LayerAggregate *) values[MA_VAL];
	aggregate->search_date();
}

/**
 * Search all TrackWaypoint layers in this aggregate layer for an item on the user specified date
 */
void LayerAggregate::search_date()
{
	VikCoord position;
	char * date_str = a_dialog_get_date(VIK_GTK_WINDOW_FROM_LAYER(this->vl), _("Search by Date"));

	if (!date_str) {
		return;
	}

	Viewport * viewport = vik_window_viewport(VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(this->vl)));

	bool found = false;
	std::list<Layer *> * layers = new std::list<Layer *>;
	layers = this->get_all_layers_of_type(layers, VIK_LAYER_TRW, true);


	// Search tracks first
	for (auto iter = layers->begin(); iter != layers->end(); iter++) {
		// Make it auto select the item if found
		found = VIK_TRW_LAYER(*iter)->trw->find_by_date(date_str, &position, viewport, true, true);
		if (found) {
			break;
		}
	}
	delete layers;

	if (!found) {
		// Reset and try on Waypoints; /* kamilTODO: do we need to reset the list? Did it change? */
		layers = new std::list<Layer *>;
		layers = this->get_all_layers_of_type(layers, VIK_LAYER_TRW, true);

		for (auto iter = layers->begin(); iter != layers->end(); iter++) {
			// Make it auto select the item if found
			found = VIK_TRW_LAYER(*iter)->trw->find_by_date(date_str, &position, viewport, false, true);
			if (found) {
				break;
			}
		}
		delete layers;
	}

	if (!found) {
		a_dialog_info_msg(VIK_GTK_WINDOW_FROM_LAYER(this->vl), _("No items found with the requested date."));
	}
	free(date_str);
}

static GList* aggregate_layer_track_create_list(VikLayer *vl, void * user_data)
{
	LayerAggregate * aggregate  = (LayerAggregate *) vl->layer;
	return aggregate->track_create_list();
}


/**
 * Returns: A list of #vik_trw_track_list_t
 */
GList * LayerAggregate::track_create_list()
{
	// Get all TRW layers
	std::list<Layer *> * layers = new std::list<Layer *>;
	layers = this->get_all_layers_of_type(layers, VIK_LAYER_TRW, true);

	// For each TRW layers keep adding the tracks and routes to build a list of all of them
	GList *tracks_and_layers = NULL;

	for (auto iter = layers->begin(); iter != layers->end(); iter++) {
		GList *tracks = NULL;
		LayerTRWc::get_track_values(&tracks, VIK_TRW_LAYER(*iter)->trw->get_tracks());
		LayerTRWc::get_track_values(&tracks, VIK_TRW_LAYER(*iter)->trw->get_routes());

		tracks_and_layers = g_list_concat(tracks_and_layers, VIK_TRW_LAYER(*iter)->trw->build_track_list_t(tracks));
	}
	delete layers;

	return tracks_and_layers;
}

static void aggregate_layer_track_list_dialog(menu_array_values values)
{
	LayerAggregate * aggregate = (LayerAggregate *) values[MA_VAL];
	VikLayer * vl = (VikLayer *) aggregate->vl;
	char *title = g_strdup_printf(_("%s: Track and Route List"), vl->name);
	vik_trw_layer_track_list_show_dialog(title, vl, NULL, aggregate_layer_track_create_list, true);
	free(title);
}

/**
 * aggregate_layer_analyse_close:
 *
 * Stuff to do on dialog closure
 */
static void aggregate_layer_analyse_close(GtkWidget *dialog, int resp, VikLayer* vl)
{
	VikAggregateLayer *val = (VikAggregateLayer *) vl;
	LayerAggregate * aggregate = (LayerAggregate *) ((VikLayer *) val)->layer;
	gtk_widget_destroy(dialog);
	aggregate->tracks_analysis_dialog = NULL;
}

static void aggregate_layer_analyse(menu_array_values values)
{
	LayerAggregate * aggregate = (LayerAggregate *) values[MA_VAL];

	// There can only be one!
	if (aggregate->tracks_analysis_dialog) {
		return;
	}

	aggregate->tracks_analysis_dialog = vik_trw_layer_analyse_this(VIK_GTK_WINDOW_FROM_LAYER(aggregate->vl),
								       aggregate->vl->name,
								       aggregate->vl,
								       NULL,
								       aggregate_layer_track_create_list,
								       aggregate_layer_analyse_close);
}

void LayerAggregate::add_menu_items(GtkMenu * menu, void * vlp)
{
	// Data to pass on in menu functions
	static menu_array_values values;
	values[MA_VAL] = this;
	values[MA_VLP] = vlp;

	GtkWidget *item = gtk_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	GtkWidget *vis_submenu = gtk_menu_new();
	item = gtk_menu_item_new_with_mnemonic(_("_Visibility"));
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	gtk_widget_show(item);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM (item), vis_submenu);

	item = gtk_image_menu_item_new_with_mnemonic(_("_Show All"));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_APPLY, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped (G_OBJECT(item), "activate", G_CALLBACK(aggregate_layer_child_visible_on), values);
	gtk_menu_shell_append (GTK_MENU_SHELL (vis_submenu), item);
	gtk_widget_show (item);

	item = gtk_image_menu_item_new_with_mnemonic (_("_Hide All"));
	gtk_image_menu_item_set_image ((GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped (G_OBJECT(item), "activate", G_CALLBACK(aggregate_layer_child_visible_off), values);
	gtk_menu_shell_append (GTK_MENU_SHELL (vis_submenu), item);
	gtk_widget_show (item);

	item = gtk_image_menu_item_new_with_mnemonic (_("_Toggle"));
	gtk_image_menu_item_set_image ((GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped (G_OBJECT(item), "activate", G_CALLBACK(aggregate_layer_child_visible_toggle), values);
	gtk_menu_shell_append (GTK_MENU_SHELL (vis_submenu), item);
	gtk_widget_show (item);

	GtkWidget *submenu_sort = gtk_menu_new ();
	item = gtk_image_menu_item_new_with_mnemonic (_("_Sort"));
	gtk_image_menu_item_set_image ((GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu_sort);

	item = gtk_image_menu_item_new_with_mnemonic (_("Name _Ascending"));
	gtk_image_menu_item_set_image ((GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_SORT_ASCENDING, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped (G_OBJECT(item), "activate", G_CALLBACK(aggregate_layer_sort_a2z), values);
	gtk_menu_shell_append (GTK_MENU_SHELL(submenu_sort), item);
	gtk_widget_show (item);

	item = gtk_image_menu_item_new_with_mnemonic (_("Name _Descending"));
	gtk_image_menu_item_set_image ((GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_SORT_DESCENDING, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped (G_OBJECT(item), "activate", G_CALLBACK(aggregate_layer_sort_z2a), values);
	gtk_menu_shell_append (GTK_MENU_SHELL(submenu_sort), item);
	gtk_widget_show (item);

	item = gtk_image_menu_item_new_with_mnemonic (_("Date Ascending"));
	gtk_image_menu_item_set_image ((GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_SORT_ASCENDING, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped (G_OBJECT(item), "activate", G_CALLBACK(aggregate_layer_sort_timestamp_ascend), values);
	gtk_menu_shell_append (GTK_MENU_SHELL(submenu_sort), item);
	gtk_widget_show (item);

	item = gtk_image_menu_item_new_with_mnemonic (_("Date Descending"));
	gtk_image_menu_item_set_image ((GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_SORT_DESCENDING, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped (G_OBJECT(item), "activate", G_CALLBACK(aggregate_layer_sort_timestamp_descend), values);
	gtk_menu_shell_append (GTK_MENU_SHELL(submenu_sort), item);
	gtk_widget_show (item);

	item = gtk_menu_item_new_with_mnemonic (_("_Statistics"));
	g_signal_connect_swapped (G_OBJECT(item), "activate", G_CALLBACK(aggregate_layer_analyse), values);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	item = gtk_image_menu_item_new_with_mnemonic (_("Track _List..."));
	gtk_image_menu_item_set_image ((GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped (G_OBJECT(item), "activate", G_CALLBACK(aggregate_layer_track_list_dialog), values);
	gtk_menu_shell_append (GTK_MENU_SHELL(menu), item);
	gtk_widget_show (item);

	item = gtk_image_menu_item_new_with_mnemonic (_("_Waypoint List..."));
	gtk_image_menu_item_set_image ((GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped (G_OBJECT(item), "activate", G_CALLBACK(aggregate_layer_waypoint_list_dialog), values);
	gtk_menu_shell_append (GTK_MENU_SHELL(menu), item);
	gtk_widget_show (item);

	GtkWidget *search_submenu = gtk_menu_new ();
	item = gtk_image_menu_item_new_with_mnemonic (_("Searc_h"));
	gtk_image_menu_item_set_image ((GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), search_submenu);

	item = gtk_menu_item_new_with_mnemonic (_("By _Date..."));
	g_signal_connect_swapped (G_OBJECT(item), "activate", G_CALLBACK(aggregate_layer_search_date), values);
	gtk_menu_shell_append (GTK_MENU_SHELL(search_submenu), item);
	gtk_widget_set_tooltip_text (item, _("Find the first item with a specified date"));
	gtk_widget_show (item);
}

static void disconnect_layer_signal(VikLayer *vl, VikAggregateLayer *val)
{
	unsigned int number_handlers = g_signal_handlers_disconnect_matched(vl, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, val);
	if (number_handlers != 1) {
		fprintf(stderr, "CRITICAL: %s: Unexpected number of disconnect handlers: %d\n", __FUNCTION__, number_handlers);
	}
}

void vik_aggregate_layer_free(VikAggregateLayer *val)
{
	LayerAggregate * aggregate = (LayerAggregate *) ((VikLayer *) val)->layer;
	for (auto child = aggregate->children->begin(); child != aggregate->children->end(); child++) {
		disconnect_layer_signal((*child)->vl, val);
		g_object_unref((*child)->vl);
	}
	// g_list_free(val->children); // kamilFIXME: clean up the list. */

	if (aggregate->tracks_analysis_dialog) {
		gtk_widget_destroy(aggregate->tracks_analysis_dialog);
	}
}

static void delete_layer_iter(VikLayer *vl)
{
	if (vl->realized) {
		vik_treeview_item_delete(vl->vt, &(vl->iter));
	}
}

void LayerAggregate::clear()
{
	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		disconnect_layer_signal((*child)->vl, (VikAggregateLayer *) this->vl);
		delete_layer_iter((*child)->vl);
		g_object_unref((*child)->vl);
	}
	// g_list_free(val->children); // kamilFIXME: clean up the list
}

bool vik_aggregate_layer_delete(VikAggregateLayer *val, GtkTreeIter *iter)
{
	VikLayer * vl_A = (VikLayer *) val;
	LayerAggregate * aggregate = (LayerAggregate *) ((VikLayer *) val)->layer;

	Layer * layer = (Layer *) vik_treeview_item_get_layer(vl_A->vt, iter);
	bool was_visible = layer->vl->visible;

	vik_treeview_item_delete(vl_A->vt, iter);

	for (auto i = aggregate->children->begin(); i != aggregate->children->end(); i++) {
		if ((*i)->vl = layer->vl) {
			aggregate->children->erase(i);
			break;
		}
	}
	disconnect_layer_signal(layer->vl, val);
	g_object_unref(layer->vl);

	return was_visible;
}

#if 0
/* returns: 0 = success, 1 = none appl. found, 2 = found but rejected */
unsigned int vik_aggregate_layer_tool(VikAggregateLayer *val, VikLayerTypeEnum layer_type, VikToolInterfaceFunc tool_func, GdkEventButton *event, VikViewport *vvp)
{
	GList *iter = val->children;
	bool found_rej = false;
	if (!iter) {
		return false;
	}
	while (iter->next) {
		iter = iter->next;
	}

	while (iter) {
		/* if this layer "accepts" the tool call */
		VikLayer * vl = (VikLayer *) iter->data;
		if (vl->visible && vl->type == layer_type) {
			if (tool_func(vl, event, vvp)) {
				return 0;
			} else {
				found_rej = true;
			}
		}

		/* recursive -- try the same for the child aggregate layer. */
		else if (vl->visible && vl->type == VIK_LAYER_AGGREGATE) {
			int rv = vik_aggregate_layer_tool((VikAggregateLayer *) iter->data, layer_type, tool_func, event, vvp);
			if (rv == 0) {
				return 0;
			} else if (rv == 2) {
				found_rej = true;
			}
		}
		iter = iter->prev;
	}
	return found_rej ? 2 : 1; /* no one wanted to accept the tool call in this layer */
}
#endif

Layer * LayerAggregate::get_top_visible_layer_of_type(VikLayerTypeEnum type)
{
	if (this->children->empty()) {
		return NULL;
	}

	auto child = this->children->end();
	do {
		child--;
		Layer * layer = *child;
		if (layer->vl->visible && layer->vl->type == type) {
			return layer;
		} else if (layer->vl->visible && layer->vl->type == VIK_LAYER_AGGREGATE) {
			Layer * rv = ((LayerAggregate *) layer)->get_top_visible_layer_of_type(type);
			if (rv) {
				return rv;
			}
		}
	} while (child != this->children->begin());

	return NULL;
}

std::list<Layer *> * LayerAggregate::get_all_layers_of_type(std::list<Layer *> * layers, VikLayerTypeEnum type, bool include_invisible)
{
	if (this->children->empty()) {
		return layers;
	}

	auto child = this->children->begin();
	// Where appropriate *don't* include non-visible layers
	while (child != this->children->end()) {
		Layer * layer = *child;
		if (layer->vl->type == VIK_LAYER_AGGREGATE) {
			// Don't even consider invisible aggregrates, unless told to
			if (layer->vl->visible || include_invisible) {
				LayerAggregate * aggregate = (LayerAggregate *) layer;
				layers = aggregate->get_all_layers_of_type(layers, type, include_invisible);
			}
		} else if (layer->vl->type == type) {
			if (layer->vl->visible || include_invisible) {
				layers->push_back(layer); /* now in top down order */
			}
		} else if (type == VIK_LAYER_TRW) {
			/* GPS layers contain TRW layers. cf with usage in file.c */
			if (layer->vl->type == VIK_LAYER_GPS) {
				if (layer->vl->visible || include_invisible) {
					if (!vik_gps_layer_is_empty(VIK_GPS_LAYER(layer->vl))) {
						/*
						  can not use g_list_concat due to wrong copy method - crashes if used a couple times !!
						  l = g_list_concat (l, vik_gps_layer_get_children (VIK_GPS_LAYER((*child)->vl)));
						*/
						/* create own copy method instead :(*/
						GList *gps_trw_layers = (GList *) vik_gps_layer_get_children(VIK_GPS_LAYER(layer->vl));
						int n_layers = g_list_length(gps_trw_layers);
						int lay = 0;
						for (lay = 0; lay < n_layers; lay++) {
							layers->push_front((Layer *) gps_trw_layers->data); /* kamilFIXME: gps_trw_layers->data is not Layer! */
							gps_trw_layers = gps_trw_layers->next;
						}
						g_list_free(gps_trw_layers);
					}
				}
			}
		}
		child++;
	}
	return layers;
}

void vik_aggregate_layer_realize(VikAggregateLayer *val, VikTreeview *vt, GtkTreeIter *layer_iter)
{
	LayerAggregate * aggregate = (LayerAggregate *) ((VikLayer *) val)->layer;
	aggregate->realize(vt, layer_iter);

	return;
}


void LayerAggregate::realize(VikTreeview *vt, GtkTreeIter *layer_iter)
{
	if (this->children->empty()) {
		return;
	}

	GtkTreeIter iter;

	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		Layer * layer = *child;
		vik_treeview_add_layer(this->vl->vt, layer_iter, &iter, layer->vl->name, this->vl, true,
				       layer, layer->vl->type, layer->vl->type, layer->get_timestamp());
		if (! layer->vl->visible) {
			vik_treeview_item_set_visible(vl->vt, &iter, false);
		}
		vik_layer_realize(layer->vl, this->vl->vt, &iter);
	}
}

const std::list<Layer *> * LayerAggregate::get_children()
{
	return this->children;
}

bool LayerAggregate::is_empty()
{
	return this->children->empty();
}

void LayerAggregate::drag_drop_request(Layer * src, GtkTreeIter *src_item_iter, GtkTreePath *dest_path)
{
	VikTreeview * vt = src->vl->vt;
	Layer * layer = (Layer *) vik_treeview_item_get_layer(vt, src_item_iter);
	GtkTreeIter dest_iter;
	char *dp;
	bool target_exists;

	dp = gtk_tree_path_to_string(dest_path);
	target_exists = vik_treeview_get_iter_from_path_str(vt, &dest_iter, dp);

	/* vik_aggregate_layer_delete unrefs, but we don't want that here.
	 * we're still using the layer. */
	g_object_ref(layer->vl);
	vik_aggregate_layer_delete((VikAggregateLayer *) src->vl, src_item_iter);

	if (target_exists) {
		this->insert_layer(layer, &dest_iter);
	} else {
		this->insert_layer(layer, NULL); /* append */
	}
	free(dp);
}

/**
 * Generate tooltip text for the layer.
 */
char const * LayerAggregate::tooltip()
{
	static char tmp_buf[128];
	tmp_buf[0] = '\0';

	size_t size = this->children->size();
	if (size) {
		// Could have a more complicated tooltip that numbers each type of layers,
		//  but for now a simple overall count
		snprintf(tmp_buf, sizeof(tmp_buf), ngettext("One layer", "%d layers", size), size);
	}
	return tmp_buf;
}





LayerAggregate::LayerAggregate(VikLayer * vl) : Layer(vl)
{
	this->children = new std::list<Layer *>;
	this->tracks_analysis_dialog = NULL;
}
