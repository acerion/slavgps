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

#include "window.h"
#include "variant.h"
#include "layer_aggregate.h"
#include "layer_trw.h"
#include "layer_trw_stats.h"
#include "layer_trw_waypoint_list.h"
#include "layer_trw_track_list_dialog.h"
#include "layer_gps.h"
#include "layers_panel.h"
#include "tree_view_internal.h"
#include "viewport_internal.h"
#include "util.h"
#include "dialog.h"
#include "date_time_dialog.h"
#include "tree_item_list.h"
#include "preferences.h"




using namespace SlavGPS;




extern Tree * g_tree;




#define SG_MODULE "Layer Aggregate"
#define PREFIX ": Layer Aggregate:" << __FUNCTION__ << __LINE__ << ">"




LayerAggregateInterface vik_aggregate_layer_interface;




LayerAggregateInterface::LayerAggregateInterface()
{
	this->fixed_layer_type_string = "Aggregate"; /* Non-translatable. */

	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_A;
	// this->action_icon = ...; /* Set elsewhere. */

	this->ui_labels.new_layer = QObject::tr("New Aggregate Layer");
	this->ui_labels.layer_type = QObject::tr("Aggregate");
	this->ui_labels.layer_defaults = QObject::tr("Default Settings of Aggregate Layer");
}




void LayerAggregate::marshall(Pickle & pickle)
{
	this->marshall_params(pickle);

	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		Pickle helper_pickle;
		(*child)->marshall(helper_pickle);
		if (helper_pickle.data_size() > 0) {
			pickle.put_pickle(helper_pickle);
		}
	}
}




Layer * LayerAggregateInterface::unmarshall(Pickle & pickle, Viewport * viewport)
{
	LayerAggregate * aggregate = new LayerAggregate();

	aggregate->unmarshall_params(pickle);

	while (pickle.data_size() > 0) {
		Layer * child_layer = Layer::unmarshall(pickle, viewport);
		if (child_layer) {
			aggregate->children->push_front(child_layer);
			QObject::connect(child_layer, SIGNAL (layer_changed(const QString &)), aggregate, SLOT(child_layer_changed_cb(const QString &)));
		}
	}
	// qDebug() << "II: Layer Aggregate: unmarshall() ended with len =" << pickle.data_size;
	return aggregate;
}




bool is_base_type(LayerType layer_type)
{
	/* These types are 'base' types in that you what other information on top. */
	return layer_type == LayerType::DEM
		|| layer_type == LayerType::Map
		|| layer_type == LayerType::Georef;
}




void LayerAggregate::insert_layer(Layer * layer, const Layer * sibling_layer)
{
	/* By default layers are inserted above the selected layer. */
	bool put_above = true;

	/* These types are 'base' types in that you what other information on top. */
	if (is_base_type(layer->type)) {
		put_above = false;
	}

	if (this->tree_view) {
		/* This call sets TreeItem::index and TreeItem::tree_view of added item. */
		this->tree_view->insert_tree_item(this, sibling_layer, layer, put_above);
		this->tree_view->apply_tree_item_timestamp(layer, layer->get_timestamp());
	}

	layer->owning_layer = this;

	if (sibling_layer->index.isValid()) {

		auto theone = this->children->end();
		for (auto i = this->children->begin(); i != this->children->end(); i++) {
			if (TreeItem::the_same_object(sibling_layer, *i)) {
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
		/* Effectively insert at 'end' of the list to match how displayed in the tree view
		   - but since it is drawn from 'bottom first' it is actually the first in the child list.
		   This ordering is especially important if it is a map or similar type,
		   which needs be drawn first for the layering draw method to work properly.
		   ATM this only happens when a layer is drag/dropped to the end of an aggregate layer. */
		this->children->push_back(layer);
	}

	QObject::connect(layer, SIGNAL (layer_changed(const QString &)), this, SLOT (child_layer_changed_cb(const QString &)));

	/* Update our own tooltip. */
	this->tree_view->apply_tree_item_tooltip(this);

	if (!this->children->empty()) {
		this->tree_view->expand(this->index);
	}
}




/**
 * @allow_reordering: should be set for GUI interactions,
 *                    whereas loading from a file needs strict ordering and so should be false
 */
void LayerAggregate::add_layer(Layer * layer, bool allow_reordering)
{
	if (!this->is_in_tree()) {
		qDebug() << "EE" PREFIX "Aggregate Layer" << this->name << "is not connected to tree";
		return;
	}


	/* By default layers go to the top. */
	bool put_above = true;

	if (allow_reordering && is_base_type(layer->type)) {
		put_above = false;
	}

	layer->owning_layer = this;

	if (put_above) {
		/* This call sets TreeItem::index and TreeItem::tree_view of added item. */
		this->tree_view->push_tree_item_front(this, layer);

		this->children->push_front(layer);
	} else {
		/* This call sets TreeItem::index and TreeItem::tree_view of added item. */
		this->tree_view->push_tree_item_back(this, layer);

		this->children->push_back(layer);
	}
	this->tree_view->apply_tree_item_timestamp(layer, layer->get_timestamp());

	if (layer->type == LayerType::GPS) {
		/* TODO_LATER: move this in some reasonable place. Putting it here is just a workaround. */
		layer->add_children_to_tree();
	}

	QObject::connect(layer, SIGNAL (layer_changed(const QString &)), this, SLOT (child_layer_changed_cb(const QString &)));

	/* Update our own tooltip. */
	this->tree_view->apply_tree_item_tooltip(this);

	if (!this->children->empty()) {
		this->tree_view->expand(this->index);
	}
}




bool LayerAggregate::change_child_item_position(TreeItem * child_item, bool up)
{
	auto child_iter = this->children->end();

	//this->tree_view->change_tree_item_position(child_item->index, up);

	/* We are in aggregate layer, so the child must be a layer as well. */
	assert (child_item->tree_item_type == TreeItemType::LAYER);
	Layer * child_layer = child_item->to_layer();

	/* Find container iterator of given tree item.
	   This is not the same as tree item index. */
	for (auto iter = this->children->begin(); iter != this->children->end(); iter++) {
		if (TreeItem::the_same_object(child_layer, *iter)) {
			child_iter = iter;
		}
	}

	if (child_iter == this->children->end()) {
		qDebug() << "EE" PREFIX << "Failed to find iterator of child item";
		return false;
	}

	bool result = false;
	if (up) {
		if (child_iter == this->children->begin()) {
			qDebug() << "WW" PREFIX << "Not moving child" << child_layer->name << "up, already at the beginning";
		} else {
			qDebug() << "II" PREFIX << "Moving child" << child_layer->name << "up in list of" << this->name << "children";
			std::swap(*child_iter, *std::prev(child_iter));
			result = true;
		}
	} else {
		if (std::next(child_iter) == this->children->end()) {
			qDebug() << "WW" PREFIX << "Not moving child" << child_layer->name << "down, already at the end";
		} else {
			qDebug() << "II" PREFIX << "Moving child" << child_layer->name << "down in list of" << this->name << "children";
			std::swap(*child_iter, *std::next(child_iter));
			result = true;
		}
	}

	/* In this function we only move children in aggregate's container.
	   Movement in tree widget is handled elsewhere. */

	return result;

#ifdef K_OLD_IMPLEMENTATION
	/* the old switcheroo */
	if (up && child_iter + 1 != val->children->end()) {

		Layer * tmp = *(child_iter + 1);
		*(child_iter + 1) = *(child_iter);
		*(child_iter) = tmp;
	} else if (!up && child_iter - 1 != val->children->end()) {

		Layer * tmp = *(child_iter - 1);
		*(child_iter - 1) = *(child_iter);

		first = child_iter->prev;
		second = child_iter;
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
void LayerAggregate::draw_tree_item(Viewport * viewport, bool highlight_selected, bool parent_is_selected)
{
	Layer * trigger = viewport->get_trigger();

	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		Layer * layer = *child;
#ifdef K_FIXME_RESTORE
		if (TreeItem::the_same_object(layer, trigger)) {
			if (viewport->get_half_drawn()) {
				viewport->set_half_drawn(false);
				viewport->snapshot_load();
			} else {
				viewport->snapshot_save();
			}
		}

		if (layer->type == LayerType::AGGREGATE
		    || layer->type == LayerType::GPS
		    || !viewport->get_half_drawn()) {

			qDebug() << "II: Layer Aggregate: calling draw_if_visible() for" << layer->name;
			layer->draw_tree_item(viewport, false, false);
		}
#else
		qDebug() << "II" PREFIX "calling draw_tree_item(" << highlight_selected << parent_is_selected << ") for" << layer->name;
		layer->draw_tree_item(viewport, highlight_selected, parent_is_selected);
#endif
	}
}




void LayerAggregate::change_coord_mode(CoordMode mode)
{
	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		Layer * layer = *child;
		layer->change_coord_mode(mode);
	}
}




void LayerAggregate::child_visible_toggle_cb(void) /* Slot. */
{
	TreeView * t_view = g_tree->tree_get_items_tree()->get_tree_view();

	/* Loop around all (child) layers applying visibility setting.
	   This does not descend the tree if there are aggregates within aggregrate - just the first level of layers held. */
	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		Layer * layer = *child;
		layer->visible = !layer->visible;
		/* Also set checkbox on/off in tree view. */
		t_view->apply_tree_item_visibility(layer);
	}
	/* Redraw as view may have changed. */
	this->emit_layer_changed("Aggregate - child visible toggle");
}




void LayerAggregate::child_visible_set(LayersPanel * panel, bool on_off)
{
	/* Loop around all (child) layers applying visibility setting.
	   This does not descend the tree if there are aggregates within aggregrate - just the first level of layers held. */
	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		Layer * layer = *child;
		layer->visible = on_off;
		/* Also set checkbox on_off in tree view. */
		panel->get_tree_view()->apply_tree_item_visibility(layer);
	}

	/* Redraw as view may have changed. */
	this->emit_layer_changed("Aggregate - child visible set");
}




void LayerAggregate::child_visible_on_cb(void) /* Slot. */
{
	this->child_visible_set(g_tree->tree_get_items_tree(), true);
}




void LayerAggregate::child_visible_off_cb(void) /* Slot. */
{
	this->child_visible_set(g_tree->tree_get_items_tree(), false);
}




void LayerAggregate::sort_a2z_cb(void) /* Slot. */
{
	TreeView * t_view = g_tree->tree_get_items_tree()->get_tree_view();

	this->blockSignals(true);
	t_view->blockSignals(true);

	for (auto iter = this->children->begin(); iter != this->children->end(); iter++) {
		t_view->detach_tree_item(*iter);
	}
	this->children->sort(TreeItem::compare_name_descending);
	this->add_children_to_tree();

	this->blockSignals(false);
	t_view->blockSignals(false);
}




void LayerAggregate::sort_z2a_cb(void) /* Slot. */
{
	TreeView * t_view = g_tree->tree_get_items_tree()->get_tree_view();

	this->blockSignals(true);
	t_view->blockSignals(true);

	for (auto iter = this->children->begin(); iter != this->children->end(); iter++) {
		t_view->detach_tree_item(*iter);
	}
	this->children->sort(TreeItem::compare_name_ascending);
	this->add_children_to_tree();

	this->blockSignals(false);
	t_view->blockSignals(false);
}




void LayerAggregate::sort_timestamp_ascend_cb(void) /* Slot. */
{
	this->tree_view->sort_children(this, TreeViewSortOrder::DateAscending);
	this->children->sort(Layer::compare_timestamp_ascending);
}




void LayerAggregate::sort_timestamp_descend_cb(void) /* Slot. */
{
	this->tree_view->sort_children(this, TreeViewSortOrder::DateDescending);
	this->children->sort(Layer::compare_timestamp_descending);
}




void LayerAggregate::get_waypoints_list(std::list<Waypoint *> & list)
{
	std::list<const Layer *> layers;
	this->get_all_layers_of_type(layers, LayerType::TRW, true);

	/* For each TRW layers keep adding the waypoints to build a list of all of them. */
	for (auto iter = layers.begin(); iter != layers.end(); iter++) {
		((LayerTRW *) (*iter))->get_waypoints_list(list);
	}
}




void LayerAggregate::waypoint_list_dialog_cb(void) /* Slot. */
{
	QString title = tr("%1: Waypoint List").arg(this->name);
	waypoint_list_dialog(title, this);
}




/**
   Search all TrackWaypoint layers in this aggregate layer for items with user-specified date
*/
void LayerAggregate::search_date_cb(void) /* Slot. */
{
	static QDate initial_date = QDate::currentDate();
	QDate search_date = SGDateTimeDialog::date_dialog(tr("Search by Date"), initial_date, this->get_window());
	if (!search_date.isValid()) {
		return;
	}
	initial_date = search_date;


	std::list<const Layer *> layers;
	this->get_all_layers_of_type(layers, LayerType::TRW, true);


	std::list<TreeItem *> items_by_date;
	for (auto iter = layers.begin(); iter != layers.end(); iter++) {
		items_by_date.splice(items_by_date.end(), (*iter)->get_items_by_date(search_date)); /* Move items from one list to another. */
	}


	if (items_by_date.empty()) {
		Dialog::info(tr("No items found with the requested date."), this->get_window());
	} else {
		const HeightUnit height_unit = Preferences::get_unit_height();
		TreeItemListFormat list_format;
		list_format.columns.push_back(TreeItemListColumn(TreeItemListColumnID::TheItem,  true, tr("Tree Item")));
		list_format.columns.push_back(TreeItemListColumn(TreeItemListColumnID::Timestamp, true, tr("Timestamp")));
		switch (height_unit) {
		case HeightUnit::Metres:
			list_format.columns.push_back(TreeItemListColumn(TreeItemListColumnID::Elevation,  true, tr("Height\n(Metres)")));
			break;
		case HeightUnit::Feet:
			list_format.columns.push_back(TreeItemListColumn(TreeItemListColumnID::Elevation,  true, tr("Height\n(Feet)")));
			break;
		default:
			qDebug() << SG_PREFIX_E << "Invalid height unit" << (int) height_unit;
			break;
		}


		TreeItemListDialog::show_dialog(tr("List of matching items"), list_format, items_by_date, this);
	}
}




void LayerAggregate::get_tracks_list(std::list<Track *> & list, const QString & type_id_string)
{
	std::list<Layer const *> layers;
	this->get_all_layers_of_type(layers, LayerType::TRW, true);

	/* For each TRW layers keep adding the tracks and/or routes to build a list of all of them. */
	for (auto iter = layers.begin(); iter != layers.end(); iter++) {
		((LayerTRW *) (*iter))->get_tracks_list(list, type_id_string);
	}
}




void LayerAggregate::track_list_dialog_cb(void) /* Slot. */
{
	QString title = tr("%1: Track and Route List").arg(this->name);
	track_list_dialog(title, this, "");
}




void LayerAggregate::analyse_cb(void) /* Slot. */
{
	layer_trw_show_stats(this->name, this, "", this->get_window());
}




void LayerAggregate::add_menu_items(QMenu & menu)
{
	QAction * qa = NULL;
	menu.addSeparator();

	{
		QMenu * vis_submenu = menu.addMenu(tr("&Visibility"));

		qa = vis_submenu->addAction(QIcon::fromTheme("APPLY"), tr("&Show All Layers"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (child_visible_on_cb()));

		qa = vis_submenu->addAction(QIcon::fromTheme("CLEAR"), tr("&Hide All Layers"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (child_visible_off_cb()));

		qa = vis_submenu->addAction(QIcon::fromTheme("REFRESH"), tr("&Toggle Visibility of All Layers"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (child_visible_toggle_cb()));
	}


	{
		QMenu * sort_submenu = menu.addMenu(QIcon::fromTheme("REFRESH"), tr("&Sort"));

		qa = sort_submenu->addAction(QIcon::fromTheme("view-sort-ascending"), tr("Name &Ascending"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (sort_a2z_cb()));

		qa = sort_submenu->addAction(QIcon::fromTheme("view-sort-descending"), tr("Name &Descending"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (sort_z2a_cb()));

		qa = sort_submenu->addAction(QIcon::fromTheme("view-sort-ascending"), tr("Date Ascending"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (sort_timestamp_ascend_cb()));

		qa = sort_submenu->addAction(QIcon::fromTheme("view-sort-descending"), tr("Date Descending"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (sort_timestamp_descend_cb()));
	}

	qa = menu.addAction(tr("&Statistics"));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (analyse_cb()));

	qa = menu.addAction(QIcon::fromTheme("INDEX"), tr("&Tracks List..."));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_list_dialog_cb()));

	qa = menu.addAction(QIcon::fromTheme("INDEX"), tr("&Waypoints List..."));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (waypoint_list_dialog_cb()));

	{
		QMenu * search_submenu = menu.addMenu(QIcon::fromTheme("go-jump"), tr("Searc&h"));

		qa = search_submenu->addAction(tr("By &Date..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (search_date_cb()));
		qa->setToolTip(tr("Find the first item with a specified date"));
	}
}




LayerAggregate::~LayerAggregate()
{
	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		delete *child;
	}
	delete this->children;
}




void LayerAggregate::clear()
{
	TreeView * tree = this->tree_view;

	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		Layer * layer = *child;
		if (layer->is_in_tree()) {
			tree->detach_tree_item(layer);
		}
		delete layer;
	}
	this->children->clear();

	/* Update our own tooltip. */
	this->tree_view->apply_tree_item_tooltip(this);
}




/**
   @brief Delete a layer specified by \p index

   This method also calls destructor of \p layer.

   @return true if layer was visible before being deleted
   @return false otherwise
*/
bool LayerAggregate::delete_layer(Layer * layer)
{
	assert (layer->is_in_tree());
	assert (TreeItem::the_same_object(this->tree_view->get_tree_item(layer->index)->to_layer(), layer));

	const bool was_visible = layer->visible;

	this->tree_view->detach_tree_item(layer);

	for (auto iter = this->children->begin(); iter != this->children->end(); iter++) {
		if (TreeItem::the_same_object(layer, *iter)) {
			this->children->erase(iter);
			break;
		}
	}
	delete layer;

	/* Update our own tooltip. */
	this->tree_view->apply_tree_item_tooltip(this);

	return was_visible;
}




#ifdef K_TODO_MAYBE
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
		} else if (layer->visible && layer->type == LayerType::Aggregate) {
			Layer * rv = ((LayerAggregate *) layer)->get_top_visible_layer_of_type(layer_type);
			if (rv) {
				return rv;
			}
		}
	} while (child != this->children->begin());

	return NULL;
}




void LayerAggregate::get_all_layers_of_type(std::list<Layer const *> & layers, LayerType expected_layer_type, bool include_invisible)
{
	if (this->children->empty()) {
		return;
	}

	auto child = this->children->begin();
	/* Where appropriate *don't* include non-visible layers. */
	while (child != this->children->end()) {
		Layer * layer = *child;
		if (layer->type == LayerType::Aggregate) {
			/* Don't even consider invisible aggregrates, unless told to. */
			if (layer->visible || include_invisible) {
				LayerAggregate * aggregate = (LayerAggregate *) layer;
				aggregate->get_all_layers_of_type(layers, type, include_invisible);
			}
		} else if (expected_layer_type == layer->type) {
			if (layer->visible || include_invisible) {
				layers.push_back(layer); /* now in top down order */
			}
		} else if (expected_layer_type == LayerType::TRW) {
			if (layer->type != LayerType::GPS) {
				continue;
			}

			/* GPS layers contain TRW layers. cf with usage in file.c */
			if (!(layer->visible || include_invisible)) {
				continue;
			}

			if (0 == layer->get_child_layers_count()) {
				continue;
			}

			std::list<Layer const * > gps_children = layer->get_child_layers();
			for (auto iter = gps_children.begin(); iter != gps_children.end(); iter++) {
				layers.push_front(*iter);
			}
		}
		child++;
	}
	return;
}




bool LayerAggregate::handle_select_tool_click(QMouseEvent * event, Viewport * viewport, LayerTool * layer_tool)
{
	if (this->children->empty()) {
		return false;
	}

	if (!this->visible) {
		/* In practice this condition will be checked for
		   top-level aggregate layer only. For child aggregate
		   layers the visibility condition in a loop below
		   will be tested first, before a call to the child's
		   handle_select_tool_click(). */
		return false;
	}

	bool has_been_handled = false;

	for (auto iter = this->children->begin(); iter != this->children->end(); iter++) {
		if (!(*iter)->visible) {
			continue;
		}

		if (event->flags() & Qt::MouseEventCreatedDoubleClick) {
			has_been_handled = (*iter)->handle_select_tool_double_click(event, viewport, layer_tool);
		} else {
			has_been_handled = (*iter)->handle_select_tool_click(event, viewport, layer_tool);
		}
		if (has_been_handled) {
			/* A Layer has handled the event. */
			break;
		}
	}

	return has_been_handled;

#ifdef K_OLD_IMPLEMENTATION
	/* Leaving the code here for future reference, to see how GPS layer has been handled. */

	auto child = this->children->begin();
	/* Where appropriate *don't* include non-visible layers. */
	while (child != this->children->end()) {

		if (layer->type == LayerType::AGGREGATE) {

				LayerAggregate * aggregate = (LayerAggregate *) layer;

			}
		} else if (expected_layer_type == layer->type) {
			if (layer->visible) {
				layers->push_back(layer); /* now in top down order */
			}
		} else if (expected_layer_type == LayerType::TRW) {
			if (layer->type != LayerType::GPS) {
				continue;
			}

			/* GPS layers contain TRW layers. cf with usage in file.c */
			if (!(layer->visible)) {
				continue;
			}

			if (0 == layer->get_child_layers_count()) {
				continue;
			}

			std::list<Layer const * > gps_children = layer->get_child_layers();
			for (auto iter = gps_children.begin(); iter != gps_children.end(); iter++) {
				layers->push_front(*iter);
			}
		}
		child++;
	}
#endif
}




bool LayerAggregate::handle_select_tool_double_click(QMouseEvent * event, Viewport * viewport, LayerTool * layer_tool)
{
	/* Double-click will be handled by checking event->flags() in the function below, and calling proper handling method. */
	return this->handle_select_tool_click(event, viewport, layer_tool);
}




void LayerAggregate::add_children_to_tree(void)
{
	if (this->children->empty()) {
		return;
	}

	for (auto iter = this->children->begin(); iter != this->children->end(); iter++) {
		Layer * layer = *iter;

		/* This call sets TreeItem::index and TreeItem::tree_view of added item. */
		this->tree_view->push_tree_item_back(this, layer);
		this->tree_view->apply_tree_item_timestamp(layer, layer->get_timestamp());
	}

	/* Update our own tooltip. */
	this->tree_view->apply_tree_item_tooltip(this);
}




std::list<Layer const *> LayerAggregate::get_child_layers(void) const
{
	std::list<Layer const *> result;
	for (auto iter = this->children->begin(); iter != this->children->end(); iter++) {
		result.push_back(*iter);
	}
	qDebug() << "II: Layer Aggregate: returning" << result.size() << "children";
	return result;
}




int LayerAggregate::get_child_layers_count(void) const
{
	return this->children->size();
}




void LayerAggregate::drag_drop_request(Layer * src, TreeIndex & src_item_index, void *GtkTreePath_dest_path)
{

	Layer * layer = src->tree_view->get_tree_item(src_item_index)->to_layer();

#ifdef K_FIXME_RESTORE
	char * dp = gtk_tree_path_to_string(dest_path);
	TreeIndex * dest_index = src->tree_view->get_index_from_path_str(dp);

	/* LayerAggregate::delete_layer unrefs, but we don't want that here.
	   We're still using the layer. */
	layer->ref();
	((LayerAggregate *) src)->delete_layer(src_item_index);

	if (dest_index && dest_index->isValid()) {
		this->insert_layer(layer, dest_index);
	} else {
		this->insert_layer(layer, NULL); /* Append. */
	}
	free(dp);
#endif
}




/**
   @brief Generate tooltip text for the layer
*/
QString LayerAggregate::get_tooltip(void) const
{
	/* We could have a more complicated tooltip that numbers each
	   type of layers, but for now a simple overall count should be enough. */

	return tr("%n immediate child layer(s)", "", this->children->size());
}




LayerAggregate::LayerAggregate()
{
	qDebug() << "II: LayerAggregate::LayerAggregate()";

	this->type = LayerType::Aggregate;
	strcpy(this->debug_string, "LayerType::AGGREGATE");

	this->interface = &vik_aggregate_layer_interface;
	this->set_name(Layer::get_type_ui_label(this->type));

	this->children = new std::list<Layer *>;
}




void LayerAggregate::child_layer_changed_cb(const QString & child_layer_name) /* Slot. */
{
	qDebug() << "SLOT" PREFIX << this->name << "received 'child layer changed' signal from" << child_layer_name;
	if (this->visible) {
		/* TODO_LATER: this can used from the background - e.g. in acquire
		   so will need to flow background update status through too. */
		qDebug() << "SIGNAL" PREFIX << "layer" << this->name << "emits 'changed' signal";
		emit this->layer_changed(this->get_name());
	}
}
