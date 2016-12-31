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

#include <list>
#include <cstring>
#include <cstdlib>
#include <cassert>

#include "layer_aggregate.h"
#include "layers_panel.h"
#include "waypoint_list.h"
#include "track_list_dialog.h"
#include "util.h"
#ifdef K
#include "vikgpslayer.h"
#include "viktrwlayer_analysis.h"
#endif
#include "dialog.h"
#include "icons/icons.h"
#include "globals.h"




using namespace SlavGPS;




LayerAggregateInterface vik_aggregate_layer_interface;




LayerAggregateInterface::LayerAggregateInterface()
{
	strncpy(this->layer_type_string, "Aggregate", sizeof (this->layer_type_string) - 1); /* Non-translatable. */
	this->layer_type_string[sizeof (this->layer_type_string) - 1] = '\0';

	this->layer_name = QObject::tr("Aggregate");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_A;
	// this->action_icon = ...; /* Set elsewhere. */

	this->menu_items_selection = VIK_MENU_ITEM_ALL;
}




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




Layer * LayerAggregateInterface::unmarshall(uint8_t *data, int len, Viewport * viewport)
{
#define alm_size (*(int *)data)
#define alm_next		 \
	len -= sizeof(int) + alm_size;		\
	data += sizeof(int) + alm_size;

	LayerAggregate * aggregate = new LayerAggregate();

	aggregate->unmarshall_params(data + sizeof (int), alm_size);
	alm_next;

	while (len > 0) {
		Layer * child_layer = Layer::unmarshall(data + sizeof (int), alm_size, viewport);
		if (child_layer) {
			aggregate->children->push_front(child_layer);
			QObject::connect(child_layer, SIGNAL(Layer::changed(void)), (Layer *) aggregate, SLOT(child_layer_changed_cb(void)));
		}
		alm_next;
	}
	// qDebug() << "II: Layer Aggregate: unmarshall() ended with len =" << len;
	return aggregate;
#undef alm_size
#undef alm_next
}




void LayerAggregate::insert_layer(Layer * layer, TreeIndex const & replace_index)
{
	/* By default layers are inserted above the selected layer. */
	bool put_above = true;

	/* These types are 'base' types in that you what other information on top. */
	if (layer->type == LayerType::DEM
#ifdef K
	    || layer->type == LayerType::MAPS
	    || layer->type == LayerType::GEOREF
#endif
	    ) {

		put_above = false;
	}

	if (this->connected_to_tree) {
		TreeIndex const & new_index = this->tree_view->insert_layer(layer, this, this->index, put_above, layer->get_timestamp(), replace_index);
		if (!layer->visible) {
			this->tree_view->set_visibility(new_index, false);
		}

		layer->connect_to_tree(this->tree_view, new_index);

		if (this->children->empty()) { /* kamilTODO: empty() or !empty()? */
			this->tree_view->expand(this->index);
		}
	}

	if (replace_index.isValid()) {
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

	if (allow_reordering) {
		/* These types are 'base' types in that you what other information on top. */
		if (layer->type == LayerType::DEM
#ifdef K
		    || layer->type == LayerType::MAPS
		    || layer->type == LayerType::GEOREF
#endif
		    ) {
			put_above = false;
		}
	}


	if (this->connected_to_tree) {
		TreeIndex const & new_index = this->tree_view->add_layer(layer, this, this->index, false, layer->get_timestamp());
		if (!layer->visible) {
			this->tree_view->set_visibility(new_index, false);
		}

		layer->connect_to_tree(this->tree_view, new_index);

		if (this->children->empty()) {
			this->tree_view->expand(this->index);
		}
	} else {
		qDebug() << "EE: Layer Aggregate: Aggregate Layer not connected to tree";
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
#ifdef K
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
#ifdef K
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




void LayerAggregate::child_visible_toggle_cb(void) /* Slot. */
{
	LayersPanel * panel = this->menu_data->layers_panel;
	TreeView * tree_view = panel->get_treeview();

	/* Loop around all (child) layers applying visibility setting.
	   This does not descend the tree if there are aggregates within aggregrate - just the first level of layers held. */
	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		Layer * layer = *child;
		layer->visible = !layer->visible;
		/* Also set checkbox on/off. */
		tree_view->toggle_visibility(layer->index);
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




void LayerAggregate::child_visible_on_cb(void) /* Slot. */
{
	this->child_visible_set(this->menu_data->layers_panel, true);
}




void LayerAggregate::child_visible_off_cb(void) /* Slot. */
{
	this->child_visible_set(this->menu_data->layers_panel, false);
}




void LayerAggregate::sort_a2z_cb(void) /* Slot. */
{
	this->tree_view->sort_children(this->index, VL_SO_ALPHABETICAL_ASCENDING);
	this->children->sort(Layer::compare_name_ascending);
}




void LayerAggregate::sort_z2a_cb(void) /* Slot. */
{
	this->tree_view->sort_children(this->index, VL_SO_ALPHABETICAL_DESCENDING);
	this->children->sort(Layer::compare_name_descending);
}




void LayerAggregate::sort_timestamp_ascend_cb(void) /* Slot. */
{
	this->tree_view->sort_children(this->index, VL_SO_DATE_ASCENDING);
	this->children->sort(Layer::compare_timestamp_ascending);
}




void LayerAggregate::sort_timestamp_descend_cb(void) /* Slot. */
{
	this->tree_view->sort_children(this->index, VL_SO_DATE_DESCENDING);
	this->children->sort(Layer::compare_timestamp_descending);
}




std::list<waypoint_layer_t *> * LayerAggregate::create_waypoints_and_layers_list()
{
	std::list<Layer *> * layers = new std::list<Layer *>;
	layers = this->get_all_layers_of_type(layers, LayerType::TRW, true);

	/* For each TRW layers keep adding the waypoints to build a list of all of them. */
	std::list<waypoint_layer_t *> * waypoints_and_layers = new std::list<waypoint_layer_t *>;
	for (auto iter = layers->begin(); iter != layers->end(); iter++) {
		std::list<Waypoint *> * waypoints = new std::list<Waypoint *>;

		/* TODO: move this to layer trw containers. */
		Waypoints & wps = ((LayerTRW *) (*iter))->get_waypoints();
		for (auto i = wps.begin(); i != wps.end(); i++) {
			waypoints->push_back(i->second);
		}

		waypoints_and_layers->splice(waypoints_and_layers->begin(), *((LayerTRW *) (*iter))->create_waypoints_and_layers_list_helper(waypoints));
	}
	delete layers;

	return waypoints_and_layers;
}




void LayerAggregate::waypoint_list_dialog_cb(void) /* Slot. */
{
	QString title = QString(_("%1: Waypoint List")).arg(QString(this->name));
	waypoint_list_dialog(title, this, true);
}




/**
 * Search all TrackWaypoint layers in this aggregate layer for an item on the user specified date
 */
void LayerAggregate::search_date_cb(void) /* Slot. */
{
	VikCoord position;
	char * date_str = a_dialog_get_date(this->get_window(), _("Search by Date"));
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
		found = ((LayerTRW *) (*iter))->find_track_by_date(date_str, &position, viewport, true);
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
			found = ((LayerTRW *) (*iter))->find_waypoint_by_date(date_str, &position, viewport, true);
			if (found) {
				break;
			}
		}
		delete layers;
	}

	if (!found) {
		dialog_info("No items found with the requested date.", this->get_window());
	}
	free(date_str);
}




std::list<SlavGPS::track_layer_t *> * aggregate_layer_create_tracks_and_layers_list(Layer * layer)
{
	return ((LayerAggregate *) layer)->create_tracks_and_layers_list();
}




std::list<SlavGPS::track_layer_t *> * LayerAggregate::create_tracks_and_layers_list(SublayerType sublayer_type)
{
	return this->create_tracks_and_layers_list();
}




std::list<track_layer_t *> * LayerAggregate::create_tracks_and_layers_list()
{
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
}




void LayerAggregate::track_list_dialog_cb(void) /* Slot. */
{
	QString title = QString(_("%1: Track and Route List")).arg(QString(this->name));
	track_list_dialog(title, this, SublayerType::NONE, true);
}




/**
 * Stuff to do on dialog closure
 */
static void aggregate_layer_analyse_close(GtkWidget *dialog, int resp, Layer * layer)
{
#ifdef K
	LayerAggregate * aggregate = (LayerAggregate *) layer;
	gtk_widget_destroy(dialog);
	aggregate->tracks_analysis_dialog = NULL;
#endif
}




void LayerAggregate::analyse_cb(void) /* Slot. */
{
	/* There can only be one! */
	if (this->tracks_analysis_dialog) {
		return;
	}

#ifdef K
	this->tracks_analysis_dialog = vik_trw_layer_analyse_this(this->get_window(),
								  this->name,
								  this,
								  SublayerType::NONE,
								  aggregate_layer_analyse_close);
#endif
}




void LayerAggregate::add_menu_items(QMenu & menu)
{
	QAction * qa = NULL;
	menu.addSeparator();

	{
		QMenu * vis_submenu = menu.addMenu(QString("&Visibility"));

		qa = vis_submenu->addAction(QIcon::fromTheme("APPLY"), QString(_("&Show All")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (child_visible_on_cb()));

		qa = vis_submenu->addAction(QIcon::fromTheme("CLEAR"), QString(_("&Hide All")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (child_visible_off_cb()));

		qa = vis_submenu->addAction(QIcon::fromTheme("REFRESH"), QString(_("&Toggle")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (child_visible_toggle_cb()));
	}


	{
		QMenu * sort_submenu = menu.addMenu(QIcon::fromTheme("REFRESH"), QString("&Sort"));

		qa = sort_submenu->addAction(QIcon::fromTheme("view-sort-ascending"), QString(_("Name &Ascending")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (sort_a2z_cb()));

		qa = sort_submenu->addAction(QIcon::fromTheme("view-sort-descending"), QString(_("Name &Descending")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (sort_z2a_cb()));

		qa = sort_submenu->addAction(QIcon::fromTheme("view-sort-ascending"), QString(_("Date Ascending")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (sort_timestamp_ascend_cb()));

		qa = sort_submenu->addAction(QIcon::fromTheme("view-sort-descending"), QString(_("Date Descending")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (sort_timestamp_descend_cb()));
	}

	qa = menu.addAction(QString(_("&Statistics")));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (analyse_cb()));

	qa = menu.addAction(QIcon::fromTheme("INDEX"), tr("&Tracks List..."));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_list_dialog_cb()));

	qa = menu.addAction(QIcon::fromTheme("INDEX"), tr("&Waypoints List..."));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (waypoint_list_dialog_cb()));

	{
		QMenu * search_submenu = menu.addMenu(QIcon::fromTheme("go-jump"), QString("Searc&h"));

		qa = search_submenu->addAction(QString(_("By &Date...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (search_date_cb()));
		qa->setToolTip(QString(_("Find the first item with a specified date")));
	}
}




LayerAggregate::~LayerAggregate()
{
#ifdef K
	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		this->disconnect_layer_signal(*child);
		(*child)->unref();
	}
	// g_list_free(val->children); /* kamilFIXME: clean up the list. */

	if (this->tracks_analysis_dialog) {
		gtk_widget_destroy(this->tracks_analysis_dialog);
	}
#endif
}




void LayerAggregate::clear()
{
	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		Layer * layer = *child;
		this->disconnect_layer_signal(layer);
		if (layer->connected_to_tree) {
			layer->tree_view->erase(layer->index);
		}
		delete layer;
	}
	// g_list_free(val->children); /* kamilFIXME: clean up the list
}




/* Delete a layer specified by \p index. */
bool LayerAggregate::delete_layer(TreeIndex const & index)
{
	assert(index.isValid());

	Layer * layer = this->tree_view->get_layer(index);
	bool was_visible = layer->visible;

	this->tree_view->erase(index);

	for (auto i = this->children->begin(); i != this->children->end(); i++) {
		if (layer->the_same_object(*i)) {
			this->children->erase(i);
			break;
		}
	}
	this->disconnect_layer_signal(layer);
	delete layer;

	return was_visible;
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
		} else if (layer_type == LayerType::TRW) {
			/* GPS layers contain TRW layers. cf with usage in file.c */
#ifdef K
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




void LayerAggregate::connect_to_tree(TreeView * tree_view_, TreeIndex const & layer_index)
{
	this->tree_view = tree_view_;
	this->index = layer_index;
	this->connected_to_tree = true;

	if (this->children->empty()) {
		return;
	}

	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		Layer * layer = *child;
		TreeIndex const & new_index = this->tree_view->add_layer(layer, this, this->index, false, layer->get_timestamp());
		if (!layer->visible) {
			this->tree_view->set_visibility(new_index, false);
		}
		layer->connect_to_tree(this->tree_view, new_index);
	}
}




std::list<Layer const *> * LayerAggregate::get_children()
{
	std::list<Layer const *> * result = new std::list<Layer const *>;
	for (auto iter = this->children->begin(); iter != this->children->end(); iter++) {
		result->push_back(*iter);
	}
	qDebug() << "II: Layer Aggregate: returning" << result->size() << "children";
	return result;
}




bool LayerAggregate::is_empty()
{
	return this->children->empty();
}




void LayerAggregate::drag_drop_request(Layer * src, GtkTreeIter *src_item_iter, GtkTreePath *dest_path)
{
#ifdef K
	Layer * layer = src->tree_view->get_layer(src_item_iter);

	char * dp = gtk_tree_path_to_string(dest_path);
	TreeIndex * dest_index = src->tree_view->get_index_from_path_str(dp);

	/* LayerAggregate::delete_layer unrefs, but we don't want that here.
	   We're still using the layer. */
	layer->ref();
	((LayerAggregate *) src)->delete_layer(src_item_iter);

	if (dest_index && dest_index->isValid()) {
		this->insert_layer(layer, dest_index);
	} else {
		this->insert_layer(layer, NULL); /* Append. */
	}
	free(dp);
#endif
}




/**
 * Generate tooltip text for the layer.
 */
QString LayerAggregate::tooltip()
{
	QString tooltip;

	size_t size = this->children->size();
	if (size) {
		/* Could have a more complicated tooltip that numbers each
		   type of layers, but for now a simple overall count. */
		tooltip = QString(tr("%n layer(s)", "", size));
	}
	return tooltip;
}




LayerAggregate::LayerAggregate()
{
	qDebug() << "II: LayerAggregate::LayerAggregate()";

	this->type = LayerType::AGGREGATE;
	strcpy(this->debug_string, "LayerType::AGGREGATE");
	this->interface = &vik_aggregate_layer_interface;

	this->rename(vik_aggregate_layer_interface.layer_name.toUtf8().constData());
	this->children = new std::list<Layer *>;
	this->tracks_analysis_dialog = NULL;
}
