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
#include "layer_aggregate.h"
#include "layers_panel.h"
#ifndef SLAVGPS_QT
#include "vikgpslayer.h"
#include "viktrwlayer_analysis.h"
#include "viktrwlayer_tracklist.h"
#include "viktrwlayer_waypointlist.h"
#include "dialog.h"
#endif
#include "icons/icons.h"
#include "globals.h"

#include <list>
#include <cstring>
#include <cstdlib>
#include <cassert>

#include <glib/gi18n.h>




using namespace SlavGPS;




static Layer * aggregate_layer_unmarshall(uint8_t * data, int len, Viewport * viewport);




LayerInterface vik_aggregate_layer_interface = {
	"Aggregate",
	N_("Aggregate"),
	"<control><shift>A",
#ifdef SLAVGPS_QT
	NULL,
#else
	&vikaggregatelayer_pixbuf,
#endif

	{ NULL, NULL, NULL, NULL, NULL, NULL, NULL },
	NULL,
	0,

	NULL,
	0,
	NULL,
	0,

	VIK_MENU_ITEM_ALL,

	/* (VikLayerFuncUnmarshall) */   aggregate_layer_unmarshall,
	/* (VikLayerFuncChangeParam) */  NULL,
	NULL,
};




void LayerAggregate::marshall(uint8_t **data, int *datalen)
{
	uint8_t *ld;
	int ll;
	GByteArray* b = g_byte_array_new();
	int len;

#define alm_append(obj, sz)					\
	len = (sz);						\
	g_byte_array_append(b, (uint8_t *)&len, sizeof(len));	\
	g_byte_array_append(b, (uint8_t *)(obj), len);

	this->marshall_params(&ld, &ll);
	alm_append(ld, ll);
	free(ld);

	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		Layer::marshall((*child), &ld, &ll);
		if (ld) {
			alm_append(ld, ll);
			free(ld);
		}
	}
	*data = b->data;
	*datalen = b->len;
	g_byte_array_free(b, false);
#undef alm_append
}




static Layer * aggregate_layer_unmarshall(uint8_t *data, int len, Viewport * viewport)
{
#define alm_size (*(int *)data)
#define alm_next		 \
	len -= sizeof(int) + alm_size;		\
	data += sizeof(int) + alm_size;

	LayerAggregate * aggregate = new LayerAggregate();

	aggregate->unmarshall_params(data + sizeof (int), alm_size, viewport);
	alm_next;

	while (len > 0) {
		Layer * child_layer = Layer::unmarshall(data + sizeof (int), alm_size, viewport);
		if (child_layer) {
			aggregate->children->push_front(child_layer);
			QObject::connect(child_layer, SIGNAL(Layer::changed(void)), (Layer *) aggregate, SLOT(child_layer_changed_cb(void)));
		}
		alm_next;
	}
	// fprintf(stdout, "aggregate_layer_unmarshall ended with len=%d\n", len);
	return aggregate;
#undef alm_size
#undef alm_next
}




void LayerAggregate::insert_layer(Layer * layer, TreeIndex * replace_index)
{
	/* By default layers are inserted above the selected layer. */
	bool put_above = true;

#if 0
	/* These types are 'base' types in that you what other information on top. */
	if (layer->type == LayerType::MAPS || layer->type == LayerType::DEM || layer->type == LayerType::GEOREF) {
		put_above = false;
	}
#endif

	if (this->realized) {
		TreeIndex * new_index = this->tree_view->insert_layer(layer, this, this->index, put_above, (int) layer->type, layer->get_timestamp(), replace_index);
		if (!layer->visible) {
			this->tree_view->set_visibility(new_index, false);
		}

		layer->realize(this->tree_view, new_index);

		if (this->children->empty()) { /* kamilTODO: empty() or !empty()? */
			this->tree_view->expand(this->index);
		}
	}

	if (replace_index) {
		Layer * existing_layer = this->tree_view->get_layer(replace_index);

		auto theone = this->children->end();
		for (auto i = this->children->begin(); i != this->children->end(); i++) {
			if (existing_layer->the_same_object(*i)) {
				theone = i;
			}
		}

		if (put_above) {
			this->children->insert(std::next(theone), layer);
		} else {
			/* Thus insert 'here' (so don't add 1). */
			this->children->insert(theone, layer);
		}
	} else {
		/* Effectively insert at 'end' of the list to match how displayed in the treeview
		   - but since it is drawn from 'bottom first' it is actually the first in the child list.
		   This ordering is especially important if it is a map or similar type,
		   which needs be drawn first for the layering draw method to work properly.
		   ATM this only happens when a layer is drag/dropped to the end of an aggregate layer. */
		this->children->push_back(layer);
	}

	QObject::connect(layer, SIGNAL(changed(void)), (Layer *) this, SLOT(child_layer_changed_cb(void)));
}




/**
 * @allow_reordering: should be set for GUI interactions,
 *                    whereas loading from a file needs strict ordering and so should be false
 */
void LayerAggregate::add_layer(Layer * layer, bool allow_reordering)
{
	/* By default layers go to the top. */
	bool put_above = true;

#ifndef SLAVGPS_QT
	if (allow_reordering) {
		/* These types are 'base' types in that you what other information on top. */
		if (layer->type == LayerType::MAPS || layer->type == LayerType::DEM || layer->type == LayerType::GEOREF) {
			put_above = false;
		}
	}
#endif

	if (this->realized) {
		TreeIndex * new_index = this->tree_view->add_layer(layer, this, this->index, false, 0, layer->get_timestamp());
		if (!layer->visible) {
			this->tree_view->set_visibility(new_index, false);
		}

		layer->realize(this->tree_view, new_index);

		if (this->children->empty()) {
			this->tree_view->expand(this->index);
		}
	} else {
		qDebug() << "Not realized";
	}

	if (put_above) {
		this->children->push_back(layer);
	} else {
		this->children->push_front(layer);
	}

	QObject::connect(layer, SIGNAL(changed(void)), this, SLOT(child_layer_changed_cb(void)));
}




void LayerAggregate::move_layer(GtkTreeIter *child_iter, bool up)
{
#ifndef SLAVGPS_QT
	auto theone = this->children->end();

	this->tree_view->move(child_iter, up);

	Layer * layer = this->tree_view->get_layer(child_iter);

	for (auto i = this->children->begin(); i != this->children->end(); i++) {
		if (layer->the_same_object(*i)) {
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
	Layer * trigger = viewport->get_trigger();

	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		Layer * layer = *child;
		if (layer->the_same_object(trigger)) {
			if (viewport->get_half_drawn()) {
				viewport->set_half_drawn(false);
				viewport->snapshot_load();
			} else {
				viewport->snapshot_save();
			}
		}

		if (layer->type == LayerType::AGGREGATE
#ifndef SLAVGPS_QT
		    || layer->type == LayerType::GPS
#endif
		    || !viewport->get_half_drawn()) {

			qDebug() << "II: Layer Aggregate: calling draw_visible() for" << layer->name;
			layer->draw_visible(viewport);
		}
	}
}




void LayerAggregate::change_coord_mode(VikCoordMode mode)
{
	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		Layer * layer = *child;
		layer->change_coord_mode(mode);
	}
}




typedef struct {
	LayerAggregate * aggregate;
	LayersPanel * panel;
} menu_array_values;




static void aggregate_layer_child_visible_toggle(menu_array_values * values)
{
	LayerAggregate * aggregate = (LayerAggregate *) values->aggregate;
	LayersPanel * panel = values->panel;

	aggregate->child_visible_toggle(panel);
}




void LayerAggregate::child_visible_toggle(LayersPanel * panel)
{
	TreeView * tree = panel->get_treeview();

	/* Loop around all (child) layers applying visibility setting.
	   This does not descend the tree if there are aggregates within aggregrate - just the first level of layers held. */
	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		Layer * layer = *child;
		layer->visible = !layer->visible;
		/* Also set checkbox on/off. */
		tree->toggle_visibility(layer->index);
	}
	/* Redraw as view may have changed. */
	this->emit_changed();
}




void LayerAggregate::child_visible_set(LayersPanel * panel, bool on_off)
{
	/* Loop around all (child) layers applying visibility setting.
	   This does not descend the tree if there are aggregates within aggregrate - just the first level of layers held. */
	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		Layer * layer = *child;
		layer->visible = on_off;
		/* Also set checkbox on_off. */
		panel->get_treeview()->set_visibility(layer->index, on_off);
	}

	/* Redraw as view may have changed. */
	this->emit_changed();
}




static void aggregate_layer_child_visible_on(menu_array_values * values)
{
	LayerAggregate * aggregate = values->aggregate;
	LayersPanel * panel = values->panel;

	aggregate->child_visible_set(panel, true);
}




static void aggregate_layer_child_visible_off(menu_array_values * values)
{
	LayerAggregate * aggregate = values->aggregate;
	LayersPanel * panel = values->panel;

	aggregate->child_visible_set(panel, false);
}




static void aggregate_layer_sort_a2z(menu_array_values * values)
{
	LayerAggregate * aggregate = values->aggregate;

#ifndef SLAVGPS_QT
	aggregate->tree_view->sort_children(&aggregate->iter, VL_SO_ALPHABETICAL_ASCENDING);
	aggregate->children->sort(Layer::compare_name_ascending);
#endif
}




static void aggregate_layer_sort_z2a(menu_array_values * values)
{
	LayerAggregate * aggregate = values->aggregate;

#ifndef SLAVGPS_QT
	aggregate->tree_view->sort_children(&aggregate->iter, VL_SO_ALPHABETICAL_DESCENDING);
	aggregate->children->sort(Layer::compare_name_descending);
#endif
}




static void aggregate_layer_sort_timestamp_ascend(menu_array_values * values)
{
	LayerAggregate * aggregate = values->aggregate;

#ifndef SLAVGPS_QT
	aggregate->tree_view->sort_children(&aggregate->iter, VL_SO_DATE_ASCENDING);
	aggregate->children->sort(Layer::compare_timestamp_ascending);
#endif
}




static void aggregate_layer_sort_timestamp_descend(menu_array_values * values)
{
	LayerAggregate * aggregate = values->aggregate;

#ifndef SLAVGPS_QT
	aggregate->tree_view->sort_children(&aggregate->iter, VL_SO_DATE_DESCENDING);
	aggregate->children->sort(Layer::compare_timestamp_descending);
#endif
}




std::list<waypoint_layer_t *> * LayerAggregate::create_waypoints_and_layers_list()
{
#ifndef SLAVGPS_QT
	std::list<Layer *> * layers = new std::list<Layer *>;
	layers = this->get_all_layers_of_type(layers, LayerType::TRW, true);

	/* For each TRW layers keep adding the waypoints to build a list of all of them. */
	std::list<waypoint_layer_t *> * waypoints_and_layers = new std::list<waypoint_layer_t *>;
	for (auto iter = layers->begin(); iter != layers->end(); iter++) {
		std::list<Waypoint *> * waypoints = new std::list<Waypoint *>;

		std::unordered_map<sg_uid_t, Waypoint *> & wps = ((LayerTRW *) (*iter))->get_waypoints();
		for (auto i = wps.begin(); i != wps.end(); i++) {
			waypoints->push_back(i->second);
		}

		waypoints_and_layers->splice(waypoints_and_layers->begin(), *((LayerTRW *) (*iter))->create_waypoints_and_layers_list_helper(waypoints));
	}
	delete layers;

	return waypoints_and_layers;
#endif
}




static void aggregate_layer_waypoint_list_dialog(menu_array_values * values)
{
#ifndef SLAVGPS_QT
	LayerAggregate * aggregate = values->aggregate;

	char *title = g_strdup_printf(_("%s: Waypoint List"), aggregate->name);
	vik_trw_layer_waypoint_list_show_dialog(title, aggregate, true);
	free(title);
#endif
}




static void aggregate_layer_search_date(menu_array_values * values)
{
	LayerAggregate * aggregate = values->aggregate;

	aggregate->search_date();
}




/**
 * Search all TrackWaypoint layers in this aggregate layer for an item on the user specified date
 */
void LayerAggregate::search_date()
{
#ifndef SLAVGPS_QT
	VikCoord position;
	char * date_str = a_dialog_get_date(this->get_toolkit_window(), _("Search by Date"));

	if (!date_str) {
		return;
	}

	Viewport * viewport = this->get_window()->get_viewport();

	bool found = false;
	std::list<Layer *> * layers = new std::list<Layer *>;
	layers = this->get_all_layers_of_type(layers, LayerType::TRW, true);


	/* Search tracks first. */
	for (auto iter = layers->begin(); iter != layers->end(); iter++) {
		/* Make it auto select the item if found. */
		found = ((LayerTRW *) (*iter))->find_by_date(date_str, &position, viewport, true, true);
		if (found) {
			break;
		}
	}
	delete layers;

	if (!found) {
		/* Reset and try on Waypoints. */ /* kamilTODO: do we need to reset the list? Did it change? */
		layers = new std::list<Layer *>;
		layers = this->get_all_layers_of_type(layers, LayerType::TRW, true);

		for (auto iter = layers->begin(); iter != layers->end(); iter++) {
			/* Make it auto select the item if found. */
			found = ((LayerTRW *) (*iter))->find_by_date(date_str, &position, viewport, false, true);
			if (found) {
				break;
			}
		}
		delete layers;
	}

	if (!found) {
		a_dialog_info_msg(this->get_toolkit_window(), _("No items found with the requested date."));
	}
	free(date_str);
#endif
}




std::list<SlavGPS::track_layer_t *> * aggregate_layer_create_tracks_and_layers_list(Layer * layer)
{
#ifndef SLAVGPS_QT
	return ((LayerAggregate *) layer)->create_tracks_and_layers_list();
#endif
}




std::list<SlavGPS::track_layer_t *> * LayerAggregate::create_tracks_and_layers_list(SublayerType sublayer_type)
{
#ifndef SLAVGPS_QT
	return this->create_tracks_and_layers_list();
#endif
}




std::list<track_layer_t *> * LayerAggregate::create_tracks_and_layers_list()
{
#ifndef SLAVGPS_QT
	std::list<Layer *> * layers = new std::list<Layer *>;
	layers = this->get_all_layers_of_type(layers, LayerType::TRW, true);

	/* For each TRW layers keep adding the tracks and routes to build a list of all of them. */
	std::list<track_layer_t *> * tracks_and_layers = new std::list<track_layer_t *>;

	std::list<Track *> * tracks = new std::list<Track *>;
	for (auto iter = layers->begin(); iter != layers->end(); iter++) {

		LayerTRWc::get_track_values(tracks, ((LayerTRW *) (*iter))->get_tracks());
		LayerTRWc::get_track_values(tracks, ((LayerTRW *) (*iter))->get_routes());

		tracks_and_layers->splice(tracks_and_layers->begin(), *((LayerTRW *) (*iter))->create_tracks_and_layers_list_helper(tracks));

		tracks->clear();
	}
	delete tracks;
	delete layers;

	return tracks_and_layers;
#endif
}




static void aggregate_layer_track_list_dialog(menu_array_values * values)
{
#ifndef SLAVGPS_QT
	LayerAggregate * aggregate = values->aggregate;

	char *title = g_strdup_printf(_("%s: Track and Route List"), aggregate->name);
	vik_trw_layer_track_list_show_dialog(title, aggregate, SublayerType::NONE, true);
	free(title);
#endif
}




/**
 * Stuff to do on dialog closure
 */
static void aggregate_layer_analyse_close(GtkWidget *dialog, int resp, Layer * layer)
{
#ifndef SLAVGPS_QT
	LayerAggregate * aggregate = (LayerAggregate *) layer;
	gtk_widget_destroy(dialog);
	aggregate->tracks_analysis_dialog = NULL;
#endif
}




static void aggregate_layer_analyse(menu_array_values * values)
{
#ifndef SLAVGPS_QT
	LayerAggregate * aggregate = values->aggregate;

	/* There can only be one! */
	if (aggregate->tracks_analysis_dialog) {
		return;
	}

	aggregate->tracks_analysis_dialog = vik_trw_layer_analyse_this(aggregate->get_toolkit_window(),
								       aggregate->name,
								       aggregate,
								       SublayerType::NONE,
								       aggregate_layer_analyse_close);
#endif
}




void LayerAggregate::add_menu_items(GtkMenu * menu, void * panel)
{
#ifndef SLAVGPS_QT
	/* Data to pass on in menu functions. */
	static menu_array_values values { .aggregate = this, .panel = (LayersPanel *) panel };

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
	g_signal_connect_swapped (G_OBJECT(item), "activate", G_CALLBACK(aggregate_layer_child_visible_on), &values);
	gtk_menu_shell_append (GTK_MENU_SHELL (vis_submenu), item);
	gtk_widget_show (item);

	item = gtk_image_menu_item_new_with_mnemonic (_("_Hide All"));
	gtk_image_menu_item_set_image ((GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped (G_OBJECT(item), "activate", G_CALLBACK(aggregate_layer_child_visible_off), &values);
	gtk_menu_shell_append (GTK_MENU_SHELL (vis_submenu), item);
	gtk_widget_show (item);

	item = gtk_image_menu_item_new_with_mnemonic (_("_Toggle"));
	gtk_image_menu_item_set_image ((GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped (G_OBJECT(item), "activate", G_CALLBACK(aggregate_layer_child_visible_toggle), &values);
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
	g_signal_connect_swapped (G_OBJECT(item), "activate", G_CALLBACK(aggregate_layer_sort_a2z), &values);
	gtk_menu_shell_append (GTK_MENU_SHELL(submenu_sort), item);
	gtk_widget_show (item);

	item = gtk_image_menu_item_new_with_mnemonic (_("Name _Descending"));
	gtk_image_menu_item_set_image ((GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_SORT_DESCENDING, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped (G_OBJECT(item), "activate", G_CALLBACK(aggregate_layer_sort_z2a), &values);
	gtk_menu_shell_append (GTK_MENU_SHELL(submenu_sort), item);
	gtk_widget_show (item);

	item = gtk_image_menu_item_new_with_mnemonic (_("Date Ascending"));
	gtk_image_menu_item_set_image ((GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_SORT_ASCENDING, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped (G_OBJECT(item), "activate", G_CALLBACK(aggregate_layer_sort_timestamp_ascend), &values);
	gtk_menu_shell_append (GTK_MENU_SHELL(submenu_sort), item);
	gtk_widget_show (item);

	item = gtk_image_menu_item_new_with_mnemonic (_("Date Descending"));
	gtk_image_menu_item_set_image ((GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_SORT_DESCENDING, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped (G_OBJECT(item), "activate", G_CALLBACK(aggregate_layer_sort_timestamp_descend), &values);
	gtk_menu_shell_append (GTK_MENU_SHELL(submenu_sort), item);
	gtk_widget_show (item);

	item = gtk_menu_item_new_with_mnemonic (_("_Statistics"));
	g_signal_connect_swapped (G_OBJECT(item), "activate", G_CALLBACK(aggregate_layer_analyse), &values);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	item = gtk_image_menu_item_new_with_mnemonic (_("Track _List..."));
	gtk_image_menu_item_set_image ((GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped (G_OBJECT(item), "activate", G_CALLBACK(aggregate_layer_track_list_dialog), &values);
	gtk_menu_shell_append (GTK_MENU_SHELL(menu), item);
	gtk_widget_show (item);

	item = gtk_image_menu_item_new_with_mnemonic (_("_Waypoint List..."));
	gtk_image_menu_item_set_image ((GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped (G_OBJECT(item), "activate", G_CALLBACK(aggregate_layer_waypoint_list_dialog), &values);
	gtk_menu_shell_append (GTK_MENU_SHELL(menu), item);
	gtk_widget_show (item);

	GtkWidget *search_submenu = gtk_menu_new ();
	item = gtk_image_menu_item_new_with_mnemonic (_("Searc_h"));
	gtk_image_menu_item_set_image ((GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), search_submenu);

	item = gtk_menu_item_new_with_mnemonic (_("By _Date..."));
	g_signal_connect_swapped (G_OBJECT(item), "activate", G_CALLBACK(aggregate_layer_search_date), &values);
	gtk_menu_shell_append (GTK_MENU_SHELL(search_submenu), item);
	gtk_widget_set_tooltip_text (item, _("Find the first item with a specified date"));
	gtk_widget_show (item);
#endif
}




LayerAggregate::~LayerAggregate()
{
#ifndef SLAVGPS_QT
	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		this->disconnect_layer_signal(*child);
		(*child)->unref();
	}
	// g_list_free(val->children); // kamilFIXME: clean up the list. */

	if (this->tracks_analysis_dialog) {
		gtk_widget_destroy(this->tracks_analysis_dialog);
	}
#endif
}




static void delete_layer_iter(Layer * layer)
{
#ifndef SLAVGPS_QT
	if (layer->realized) {
		layer->tree_view->erase(&layer->iter);
	}
#endif
}




void LayerAggregate::clear()
{
	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		this->disconnect_layer_signal(*child);
		delete_layer_iter(*child);
		(*child)->unref();
	}
	// g_list_free(val->children); // kamilFIXME: clean up the list
}




/* Delete a layer specified by \p iter. */
bool LayerAggregate::delete_layer(GtkTreeIter * iter)
{
#ifndef SLAVGPS_QT
	Layer * layer = this->tree_view->get_layer(iter);
	bool was_visible = layer->visible;

	this->tree_view->erase(iter);

	for (auto i = this->children->begin(); i != this->children->end(); i++) {
		if (layer->the_same_object(*i)) {
			this->children->erase(i);
			break;
		}
	}
	this->disconnect_layer_signal(layer);
	layer->unref();

	return was_visible;
#endif
}




#if 0
/* returns: 0 = success, 1 = none appl. found, 2 = found but rejected */
unsigned int LayerAggregate::layer_tool(LayerType layer_type, VikToolInterfaceFunc tool_func, GdkEventButton * event, Viewport * viewport)
{
	if (this->children->empty()) {
		return false;
	}

	bool found_rej = false;

	auto iter = std::prev(this->children->end());
	while (1) {

		Layer * layer = *iter;

		/* If this layer "accepts" the tool call. */
		if (layer->visible && layer->type == layer_type) {
			if (tool_func(layer, event, viewport)) {
				return 0;
			} else {
				found_rej = true;
			}
		}

		/* Recursive -- try the same for the child aggregate layer. */
		else if (layer->visible && layer->type == LayerType::AGGREGATE) {
			int rv = ((LayerAggregate *) layer)->layer_tool(layer_type, tool_func, event, viewport);
			if (rv == 0) {
				return 0;
			} else if (rv == 2) {
				found_rej = true;
			}
		}

		if (iter == this->children->begin()) {
			break;
		} else {
			iter--;
		}
	}

	return found_rej ? 2 : 1; /* No one wanted to accept the tool call in this layer. */
}
#endif




Layer * LayerAggregate::get_top_visible_layer_of_type(LayerType layer_type)
{
	if (this->children->empty()) {
		return NULL;
	}

	auto child = this->children->end();
	do {
		child--;
		Layer * layer = *child;
		if (layer->visible && layer->type == layer_type) {
			return layer;
		} else if (layer->visible && layer->type == LayerType::AGGREGATE) {
			Layer * rv = ((LayerAggregate *) layer)->get_top_visible_layer_of_type(layer_type);
			if (rv) {
				return rv;
			}
		}
	} while (child != this->children->begin());

	return NULL;
}




std::list<Layer *> * LayerAggregate::get_all_layers_of_type(std::list<Layer *> * layers, LayerType layer_type, bool include_invisible)
{
	if (this->children->empty()) {
		return layers;
	}

	auto child = this->children->begin();
	/* Where appropriate *don't* include non-visible layers. */
	while (child != this->children->end()) {
		Layer * layer = *child;
		if (layer->type == LayerType::AGGREGATE) {
			/* Don't even consider invisible aggregrates, unless told to. */
			if (layer->visible || include_invisible) {
				LayerAggregate * aggregate = (LayerAggregate *) layer;
				layers = aggregate->get_all_layers_of_type(layers, type, include_invisible);
			}
		} else if (layer->type == layer_type) {
			if (layer->visible || include_invisible) {
				layers->push_back(layer); /* now in top down order */
			}
#ifndef SLAVGPS_QT
		} else if (layer_type == LayerType::TRW) {
			/* GPS layers contain TRW layers. cf with usage in file.c */
			if (layer->type == LayerType::GPS) {
				if (layer->visible || include_invisible) {
					if (!((LayerGPS *) layer)->is_empty()) {
						/*
						  can not use g_list_concat due to wrong copy method - crashes if used a couple times !!
						  l = g_list_concat (l, (*child)->get_children();
						*/
						/* create own copy method instead :(*/
						GList *gps_trw_layers = (GList *) ((LayerGPS *) layer)->get_children();
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
#endif
		}
		child++;
	}
	return layers;
}




void LayerAggregate::realize(TreeView * tree_view_, TreeIndex * layer_index)
{
	this->tree_view = tree_view_;
	this->index = layer_index;
	this->realized = true;

	if (this->children->empty()) {
		return;
	}

	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		Layer * layer = *child;
		//QStandardItem * item = this->tree_view->add_layer(layer_item, &iter, layer->name, this, true, layer, (int) layer->type, layer->type, layer->get_timestamp());
		TreeIndex * new_index = this->tree_view->add_layer(layer, this, this->index, false, 0, layer->get_timestamp());
		if (!layer->visible) {
			this->tree_view->set_visibility(new_index, false);
		}
		layer->realize(this->tree_view, new_index);
	}
}




std::list<Layer const *> * LayerAggregate::get_children()
{
	std::list<Layer const *> * result = new std::list<Layer const *>;
	for (auto iter = this->children->begin(); iter != this->children->end(); iter++) {
		fprintf(stderr, "pushing child from aggregate\n");
		result->push_back(*iter);
	}
	fprintf(stderr, "returning %ld items in %p\n", result->size(), result);
	return result;
}




bool LayerAggregate::is_empty()
{
	return this->children->empty();
}




void LayerAggregate::drag_drop_request(Layer * src, GtkTreeIter *src_item_iter, GtkTreePath *dest_path)
{
#ifndef SLAVGPS_QT
	Layer * layer = src->tree_view->get_layer(src_item_iter);
	GtkTreeIter dest_iter;
	char *dp;
	bool target_exists;

	dp = gtk_tree_path_to_string(dest_path);
	target_exists = src->tree_view->get_iter_from_path_str(&dest_iter, dp);

	/* LayerAggregate::delete_layer unrefs, but we don't want that here.
	   We're still using the layer. */
	layer->ref();
	((LayerAggregate *) src)->delete_layer(src_item_iter);

	if (target_exists) {
		this->insert_layer(layer, &dest_iter);
	} else {
		this->insert_layer(layer, NULL); /* Append. */
	}
	free(dp);
#endif
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
		/* Could have a more complicated tooltip that numbers each
		   type of layers, but for now a simple overall count. */
		snprintf(tmp_buf, sizeof(tmp_buf), ngettext("One layer", "%ld layers", size), size);
	}
	return tmp_buf;
}




LayerAggregate::LayerAggregate()
{
	qDebug() << "II: LayerAggregate::LayerAggregate()";

	this->type = LayerType::AGGREGATE;
	strcpy(this->type_string, "LayerType::AGGREGATE");
	this->configure_interface(&vik_aggregate_layer_interface, NULL);

	this->rename(vik_aggregate_layer_interface.name);
	this->children = new std::list<Layer *>;
	this->tracks_analysis_dialog = NULL;
}




LayerAggregate::LayerAggregate(Viewport * viewport) : LayerAggregate()
{
}
