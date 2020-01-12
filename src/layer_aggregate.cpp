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
#include "layer_trw_track_internal.h"
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




#define SG_MODULE "Layer Aggregate"




LayerAggregateInterface vik_aggregate_layer_interface;




LayerAggregateInterface::LayerAggregateInterface()
{
	this->fixed_layer_kind_string = "Aggregate"; /* Non-translatable. */

	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_A;
	// this->action_icon = ...; /* Set elsewhere. */

	this->ui_labels.new_layer = QObject::tr("New Aggregate Layer");
	this->ui_labels.translated_layer_kind = QObject::tr("Aggregate");
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




Layer * LayerAggregateInterface::unmarshall(Pickle & pickle, GisViewport * gisview)
{
	LayerAggregate * aggregate = new LayerAggregate();

	aggregate->unmarshall_params(pickle);

	while (pickle.data_size() > 0) {
		Layer * child_layer = Layer::unmarshall(pickle, gisview);
		if (child_layer) {
			aggregate->children->push_front(child_layer);
			QObject::connect(child_layer, SIGNAL (tree_item_changed(const QString &)), aggregate, SLOT (child_tree_item_changed_cb(const QString &)));
		}
	}
	// qDebug() << SG_PREFIX_I << "unmarshall() ended with len =" << pickle.data_size;
	return aggregate;
}




bool is_base_type(LayerKind layer_kind)
{
	/* These kind are 'base' kinds in that you what other information on top. */
	return layer_kind == LayerKind::DEM
		|| layer_kind == LayerKind::Map
		|| layer_kind == LayerKind::Georef;
}




void LayerAggregate::insert_layer(Layer * layer, const Layer * sibling_layer)
{
	/* By default layers are inserted before the selected layer. */
	TreeViewAttachMode attach_mode = TreeViewAttachMode::Before;

	/* These types are 'base' types in that you what other information on top. */
	if (is_base_type(layer->m_kind)) {
		attach_mode = TreeViewAttachMode::After;
	}


	layer->set_owning_layer(this);

	if (sibling_layer->index.isValid()) {

		auto sibling_iter = this->children->end();
		for (auto iter = this->children->begin(); iter != this->children->end(); iter++) {
			if (TreeItem::the_same_object(sibling_layer, *iter)) {
				sibling_iter = iter;
			}
		}

		/* ::insert() inserts before given iterator. */
		if (TreeViewAttachMode::Before == attach_mode) {
			this->children->insert(sibling_iter, layer);
		} else {
			this->children->insert(std::next(sibling_iter), layer);
		}
	} else {
		/* Effectively insert at 'end' of the list to match how displayed in the tree view
		   - but since it is drawn from 'bottom first' it is actually the first in the child list.
		   This ordering is especially important if it is a map or similar type,
		   which needs be drawn first for the layering draw method to work properly.
		   ATM this only happens when a layer is drag/dropped to the end of an aggregate layer. */
		this->children->push_back(layer);
	}

	if (this->tree_view) {
		/* This call sets TreeItem::index and TreeItem::tree_view of added item. */
		qDebug() << SG_PREFIX_I << "Attaching item" << layer->get_name() << "to tree under" << this->get_name();
		layer->attach_to_tree_under_parent(this, attach_mode, sibling_layer);

		QObject::connect(layer, SIGNAL (tree_item_changed(const QString &)), this, SLOT (child_tree_item_changed_cb(const QString &)));

		/* Update our own tooltip in tree view. */
		this->update_tree_item_tooltip();

		if (!this->children->empty()) {
			this->tree_view->expand(this->index);
		}
	}
}




sg_ret LayerAggregate::add_child_item(TreeItem * item, bool allow_reordering)
{
	if (!this->is_in_tree()) {
		qDebug() << SG_PREFIX_E << "Aggregate Layer" << this->get_name() << "is not connected to tree";
		return sg_ret::err;
	}
	if (!item->is_layer()) {
		qDebug() << SG_PREFIX_E << "Tree item" << item->get_name() << "is not a layer";
		return sg_ret::err;
	}

	Layer * layer = item->get_immediate_layer();

	/* By default layers go to the top. */
	bool put_above = true;

	if (allow_reordering && is_base_type(layer->m_kind)) {
		put_above = false;
	}

	layer->set_owning_layer(this);


	if (put_above) {
		/* This call sets TreeItem::index and TreeItem::tree_view of added item. */
		qDebug() << SG_PREFIX_I << "Attaching item" << layer->get_name() << "to tree under" << this->get_name();
		layer->attach_to_tree_under_parent(this, TreeViewAttachMode::Front);
		this->children->push_front(layer);
	} else {
		/* This call sets TreeItem::index and TreeItem::tree_view of added item. */
		qDebug() << SG_PREFIX_I << "Attaching item" << layer->get_name() << "to tree under" << this->get_name();
		layer->attach_to_tree_under_parent(this);
		this->children->push_back(layer);
	}
	this->tree_view->apply_tree_item_timestamp(layer);

	QObject::connect(layer, SIGNAL (tree_item_changed(const QString &)), this, SLOT (child_tree_item_changed_cb(const QString &)));

	/* Update our own tooltip in tree view. */
	this->update_tree_item_tooltip();

#if 0
	if (!this->children->empty()) {
		this->tree_view->expand(this->index);
	}
#endif

	return sg_ret::ok;
}




sg_ret LayerAggregate::attach_to_container(Layer * layer)
{
	layer->set_owning_layer(this);
	this->children->push_back(layer);

	return sg_ret::ok;
}




sg_ret LayerAggregate::attach_to_tree(Layer * layer)
{
	if (!this->is_in_tree()) {
		qDebug() << SG_PREFIX_E << "Aggregate Layer" << this->get_name() << "is not connected to tree";
		return sg_ret::err;
	}

	/* This call sets TreeItem::index and TreeItem::tree_view of added item. */
	qDebug() << SG_PREFIX_I << "Attaching item" << layer->get_name() << "to tree under" << this->get_name();
	layer->attach_to_tree_under_parent(this);


	QObject::connect(layer, SIGNAL (tree_item_changed(const QString &)), this, SLOT (child_tree_item_changed_cb(const QString &)));


	this->tree_view->apply_tree_item_timestamp(layer);

	/* Update our own tooltip in tree view. */
	this->update_tree_item_tooltip();


#if 0
	if (!this->children->empty()) {
		this->tree_view->expand(this->index);
	}
#endif


	return sg_ret::ok;
}




bool LayerAggregate::move_child(TreeItem & child_tree_item, bool up)
{
	/* We are in aggregate layer, so the child must be a layer as well. */
	if (!child_tree_item.is_layer()) {
		qDebug() << SG_PREFIX_E << "Attempting to move non-layer child" << child_tree_item.get_name();
		return false;
	}
	if (NULL == this->children) {
		qDebug() << SG_PREFIX_E << "Attempting to move child when children container is NULL";
		return false;
	}

	Layer * layer = child_tree_item.get_immediate_layer();

	qDebug() << SG_PREFIX_I << "Will now try to move child item of" << this->get_name() << (up ? "up" : "down");
	const bool result = move_tree_item_child_algo(*this->children, layer, up);
	qDebug() << SG_PREFIX_I << "Result of attempt to move child item" << (up ? "up" : "down") << ":" << (result ? "success" : "failure");

	/* In this function we only move children in container of tree items.
	   Movement in tree widget is handled elsewhere. */

	return result;
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
void LayerAggregate::draw_tree_item(GisViewport * gisview, bool highlight_selected, bool parent_is_selected)
{
	Layer * trigger = gisview->get_trigger();

	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		Layer * layer = *child;
#ifdef K_FIXME_RESTORE
		if (TreeItem::the_same_object(layer, trigger)) {
			if (gisview->get_half_drawn()) {
				gisview->set_half_drawn(false);
				gisview->snapshot_load();
			} else {
				gisview->snapshot_save();
			}
		}

		if (layer->m_kind == LayerKind::AGGREGATE
		    || layer->m_kind == LayerKind::GPS
		    || !gisview->get_half_drawn()) {

			qDebug() << SG_PREFIX_I << "Calling draw_if_visible() for" << layer->name;
			layer->draw_tree_item(gisview, false, false);
		}
#else
		qDebug() << SG_PREFIX_I << "Calling draw_tree_item(" << highlight_selected << parent_is_selected << ") for" << layer->get_name();
		layer->draw_tree_item(gisview, highlight_selected, parent_is_selected);
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




void LayerAggregate::children_visibility_toggle_cb(void) /* Slot. */
{
	/* Loop around all (child) layers applying visibility setting.
	   This does not descend the tree if there are aggregates within aggregrate - just the first level of layers held. */
	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		Layer * layer = *child;
		layer->toggle_visible();
		/* Also set checkbox on/off in tree view. */
		this->tree_view->apply_tree_item_visibility(layer);
	}
	/* Redraw as view may have changed. */
	this->emit_tree_item_changed("Aggregate - child visible toggle");
}




void LayerAggregate::children_visibility_set(bool on_off)
{
	/* Loop around all (child) layers applying visibility setting.
	   This does not descend the tree if there are aggregates within aggregrate - just the first level of layers held. */
	for (auto child = this->children->begin(); child != this->children->end(); child++) {
		Layer * layer = *child;
		layer->set_visible(on_off);
		/* Also set checkbox on_off in tree view. */
		this->tree_view->apply_tree_item_visibility(layer);
	}

	/* Redraw as view may have changed. */
	this->emit_tree_item_changed("Aggregate - child visible set");
}




void LayerAggregate::children_visibility_on_cb(void) /* Slot. */
{
	this->children_visibility_set(true);
}




void LayerAggregate::children_visibility_off_cb(void) /* Slot. */
{
	this->children_visibility_set(false);
}




void LayerAggregate::sort_a2z_cb(void) /* Slot. */
{
	this->blockSignals(true);
	this->tree_view->blockSignals(true);

	for (auto iter = this->children->begin(); iter != this->children->end(); iter++) {
		this->tree_view->detach_tree_item(*iter);
	}
	this->children->sort(TreeItem::compare_name_ascending);
	this->attach_children_to_tree();

	this->blockSignals(false);
	this->tree_view->blockSignals(false);
}




void LayerAggregate::sort_z2a_cb(void) /* Slot. */
{
	this->blockSignals(true);
	this->tree_view->blockSignals(true);

	for (auto iter = this->children->begin(); iter != this->children->end(); iter++) {
		this->tree_view->detach_tree_item(*iter);
	}
	this->children->sort(TreeItem::compare_name_descending);
	this->attach_children_to_tree();

	this->blockSignals(false);
	this->tree_view->blockSignals(false);
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




void LayerAggregate::waypoint_list_dialog_cb(void) /* Slot. */
{
	QString title = tr("%1: Waypoint List").arg(this->get_name());
	Waypoint::list_dialog(title, this);
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
	this->get_all_layers_of_kind(layers, LayerKind::TRW, true);


	std::list<TreeItem *> items_by_date;
	for (auto iter = layers.begin(); iter != layers.end(); iter++) {
		items_by_date.splice(items_by_date.end(), (*iter)->get_items_by_date(search_date)); /* Move items from one list to another. */
	}


	if (items_by_date.empty()) {
		Dialog::info(tr("No items found with the requested date."), this->get_window());
	} else {
		const AltitudeType::Unit height_unit = Preferences::get_unit_height();
		TreeItemViewFormat view_format;
		view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::TheItem,  true, tr("Tree Item")));
		view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::Timestamp, true, tr("Timestamp")));
		switch (height_unit.u) {
		case AltitudeType::Unit::E::Metres:
			view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::Elevation,  true, tr("Height\n(Metres)")));
			break;
		case AltitudeType::Unit::E::Feet:
			view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::Elevation,  true, tr("Height\n(Feet)")));
			break;
		default:
			qDebug() << SG_PREFIX_E << "Unhandled height unit" << height_unit;
			break;
		}


		TreeItemListDialogHelper<TreeItem *> dialog_helper;
		dialog_helper.show_dialog(tr("List of matching items"), view_format, items_by_date, this->get_window());
	}
}




/**
   @reviewed-on 2019-12-01
*/
sg_ret LayerAggregate::get_tree_items(std::list<TreeItem *> & list, const std::list<SGObjectTypeID> & wanted_types) const
{
	sg_ret result = sg_ret::ok;

	/* For each layer keep adding the specified tree items
	   to build a list of all of them. */
	for (auto iter = this->children->begin(); iter != this->children->end(); ++iter) {
		if (sg_ret::ok != (*iter)->get_tree_items(list, wanted_types)) {
			result = sg_ret::err;
		}
	}

	return result;
}




/**
   @reviewed-on 2019-12-01
*/
void LayerAggregate::track_and_route_list_dialog_cb(void) /* Slot. */
{
	const std::list<SGObjectTypeID> wanted_types = { Track::type_id(), Route::type_id() };
	const QString title = tr("%1: Tracks and Routes List").arg(this->get_name());
	Track::list_dialog(title, this, wanted_types);
}




void LayerAggregate::analyse_cb(void) /* Slot. */
{
	const std::list<SGObjectTypeID> wanted_types = { Track::type_id(), Route::type_id() };
	layer_trw_show_stats(this->get_name(), this, wanted_types, this->get_window());
}




sg_ret LayerAggregate::menu_add_type_specific_operations(QMenu & menu, bool in_tree_view)
{
	QAction * qa = NULL;
	menu.addSeparator();

	{
		QMenu * vis_submenu = menu.addMenu(tr("&Visibility"));

		qa = vis_submenu->addAction(QIcon::fromTheme("APPLY"), tr("&Show All Layers"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (children_visibility_on_cb()));

		qa = vis_submenu->addAction(QIcon::fromTheme("CLEAR"), tr("&Hide All Layers"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (children_visibility_off_cb()));

		qa = vis_submenu->addAction(QIcon::fromTheme("REFRESH"), tr("&Toggle Visibility of All Layers"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (children_visibility_toggle_cb()));
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

	qa = menu.addAction(QIcon::fromTheme("INDEX"), tr("&Tracks and Routes List..."));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_and_route_list_dialog_cb()));

	qa = menu.addAction(QIcon::fromTheme("INDEX"), tr("&Waypoints List..."));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (waypoint_list_dialog_cb()));

	{
		QMenu * search_submenu = menu.addMenu(QIcon::fromTheme("go-jump"), tr("Searc&h"));

		qa = search_submenu->addAction(tr("By &Date..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (search_date_cb()));
		qa->setToolTip(tr("Find the first item with a specified date"));
	}

	return sg_ret::ok;
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

	/* Update our own tooltip in tree view. */
	this->update_tree_item_tooltip();
}




/**
   @brief Delete a layer specified by \p index

   This method also calls destructor of \p layer.

   @return true if layer was visible before being deleted
   @return false otherwise
*/
sg_ret LayerAggregate::detach_from_container(Layer * layer, bool * was_visible)
{
	assert (layer->is_in_tree());
	assert (TreeItem::the_same_object(this->tree_view->get_tree_item(layer->index)->get_immediate_layer(), layer));

	if (NULL != was_visible) {
		*was_visible = layer->is_visible();
	}

	for (auto iter = this->children->begin(); iter != this->children->end(); iter++) {
		if (TreeItem::the_same_object(layer, *iter)) {
			this->children->erase(iter);
			break;
		}
	}

	return sg_ret::ok;
}




sg_ret LayerAggregate::detach_from_tree(Layer * layer)
{
	this->tree_view->detach_tree_item(layer);

	/* Update our own tooltip in tree view. */
	this->update_tree_item_tooltip();

	return sg_ret::ok;
}




sg_ret LayerAggregate::delete_child_item(TreeItem * item, bool confirm_deleting)
{
	if (!item->is_in_tree()) {
		qDebug() << SG_PREFIX_E << "Tree item" << item->get_name() << "is not in tree";
		return sg_ret::err;
	}

	/* Children of Aggregate layer can be only other layers. */
	if (!item->is_layer()) {
		qDebug() << SG_PREFIX_E << "Tree item" << item->get_name() << "is not a layer";
		return sg_ret::err;
	}

	Layer * layer = item->get_immediate_layer();

	if (!TreeItem::the_same_object(this->tree_view->get_tree_item(layer->index)->get_immediate_layer(), layer)) {
		qDebug() << SG_PREFIX_E << "Tree item" << item->get_name() << "is not in tree";
		return sg_ret::err;
	}

	const bool was_visible = layer->is_visible();

	this->tree_view->detach_tree_item(layer);

	for (auto iter = this->children->begin(); iter != this->children->end(); iter++) {
		if (TreeItem::the_same_object(layer, *iter)) {
			this->children->erase(iter);
			break;
		}
	}
	delete layer;

	/* Update our own tooltip in tree view. */
	this->update_tree_item_tooltip();

#ifdef TODO_LATER
	if (was_visible) {
		qDebug() << SG_PREFIX_SIGNAL << "Will call 'emit_items_tree_updated_cb()' for" << parent_layer->get_name();
		this->emit_items_tree_updated_cb(parent_layer->get_name());
	}
#endif

	return sg_ret::ok;;
}




#ifdef TODO_MAYBE
/* returns: 0 = success, 1 = none appl. found, 2 = found but rejected */
unsigned int LayerAggregate::layer_tool(LayerKind layer_kind, VikToolInterfaceFunc tool_func, GdkEventButton * event, GisViewport * gisview)
{
	if (this->children->empty()) {
		return false;
	}

	bool found_rej = false;

	auto iter = std::prev(this->children->end());
	while (1) {

		Layer * layer = *iter;

		/* If this layer "accepts" the tool call. */
		if (layer->visible && layer->m_kind == layer_kind) {
			if (tool_func(layer, event, gisview)) {
				return 0;
			} else {
				found_rej = true;
			}
		}

		/* Recursive -- try the same for the child aggregate layer. */
		else if (layer->visible && layer->m_kind == LayerKind::Aggregate) {
			int rv = ((LayerAggregate *) layer)->layer_tool(layer_kind, tool_func, event, gisview);
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




Layer * LayerAggregate::get_top_visible_layer_of_type(LayerKind layer_kind)
{
	if (this->children->empty()) {
		return NULL;
	}

	auto child = this->children->end();
	do {
		child--;
		Layer * layer = *child;
		if (layer->is_visible() && layer->m_kind == layer_kind) {
			return layer;
		} else if (layer->is_visible() && layer->m_kind == LayerKind::Aggregate) {
			Layer * rv = ((LayerAggregate *) layer)->get_top_visible_layer_of_type(layer_kind);
			if (rv) {
				return rv;
			}
		}
	} while (child != this->children->begin());

	return NULL;
}




void LayerAggregate::get_all_layers_of_kind(std::list<Layer const *> & layers, LayerKind expected_layer_kind, bool include_invisible) const
{
	if (this->children->empty()) {
		return;
	}

	auto child = this->children->begin();
	/* Where appropriate *don't* include non-visible layers. */
	while (child != this->children->end()) {
		Layer * layer = *child;
		if (layer->m_kind == LayerKind::Aggregate) {
			/* Don't even consider invisible aggregrates, unless told to. */
			if (layer->is_visible() || include_invisible) {
				LayerAggregate * aggregate = (LayerAggregate *) layer;
				aggregate->get_all_layers_of_kind(layers, expected_layer_kind, include_invisible);
			}
		} else if (expected_layer_kind == layer->m_kind) {
			if (layer->is_visible() || include_invisible) {
				layers.push_back(layer); /* now in top down order */
			}
		} else if (expected_layer_kind == LayerKind::TRW) {
			if (layer->m_kind != LayerKind::GPS) {
				continue;
			}

			/* GPS layers contain TRW layers. cf with usage in file.c */
			if (!(layer->is_visible() || include_invisible)) {
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




bool LayerAggregate::handle_select_tool_click(QMouseEvent * event, GisViewport * gisview, LayerToolSelect * select_tool)
{
	if (this->children->empty()) {
		return false;
	}

	if (!this->is_visible()) {
		/* In practice this condition will be checked for
		   top-level aggregate layer only. For child aggregate
		   layers the visibility condition in a loop below
		   will be tested first, before a call to the child's
		   handle_select_tool_click(). */
		return false;
	}

	bool has_been_handled = false;

	for (auto iter = this->children->begin(); iter != this->children->end(); iter++) {
		if (!(*iter)->is_visible()) {
			continue;
		}

		if (event->flags() & Qt::MouseEventCreatedDoubleClick) {
			has_been_handled = (*iter)->handle_select_tool_double_click(event, gisview, select_tool);
		} else {
			has_been_handled = (*iter)->handle_select_tool_click(event, gisview, select_tool);
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

		if (layer->m_kind == LayerKind::Aggregate) {

			LayerAggregate * aggregate = (LayerAggregate *) layer;

		}
		} else if (expected_layer_kind == layer->m_kind) {
			if (layer->visible) {
				layers->push_back(layer); /* now in top down order */
			}
		} else if (expected_layer_kind == LayerKind::TRW) {
			if (layer->m_kind != LayerKind::GPS) {
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




bool LayerAggregate::handle_select_tool_double_click(QMouseEvent * event, GisViewport * gisview, LayerToolSelect * select_tool)
{
	/* Double-click will be handled by checking event->flags() in the function below, and calling proper handling method. */
	return this->handle_select_tool_click(event, gisview, select_tool);
}




sg_ret LayerAggregate::attach_children_to_tree(void)
{
	if (this->children->empty()) {
		return sg_ret::ok;
	}

	for (auto iter = this->children->begin(); iter != this->children->end(); iter++) {
		Layer * layer = *iter;

		/* This call sets TreeItem::index and TreeItem::tree_view of added item. */
		qDebug() << SG_PREFIX_I << "Attaching item" << layer->get_name() << "to tree under" << this->get_name();
		layer->attach_to_tree_under_parent(this);
	}

	/* Update our own tooltip in tree view. */
	this->update_tree_item_tooltip();

	return sg_ret::ok;
}




std::list<Layer const *> LayerAggregate::get_child_layers(void) const
{
	std::list<Layer const *> result;
	for (auto iter = this->children->begin(); iter != this->children->end(); iter++) {
		result.push_back(*iter);
	}
	qDebug() << SG_PREFIX_I << "Returning" << result.size() << "children";
	return result;
}




int LayerAggregate::get_child_layers_count(void) const
{
	return this->children->size();
}




sg_ret LayerAggregate::drag_drop_request(TreeItem * tree_item, int row, int col)
{
	/* Handle item in old location. */
	{
		Layer * layer = tree_item->get_owning_layer();
		if (layer->m_kind != LayerKind::Aggregate) {
			qDebug() << SG_PREFIX_E << "Moving item from layer owned by layer kind" << layer->m_kind;
			/* TODO_LATER: what about drag and drop of TRW layers from GPS layer? */
			return sg_ret::err;
		}

		((LayerAggregate *) layer)->detach_from_container((LayerAggregate *) tree_item, NULL);
		/* Detaching of tree item from tree view will be handled by QT. */
	}

	/* Handle item in new location. */
	{
		this->attach_to_container((Layer *) tree_item);
		this->attach_to_tree((Layer *) tree_item);
	}

	return sg_ret::ok;
}




/**
   @reviewed-on 2019-12-22
*/
bool LayerAggregate::dropped_item_is_acceptable(const TreeItem & tree_item) const
{
	/* Aggregate layer can contain only other layers, nothing more
	   (at least at this time). */
	return tree_item.is_layer();
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
	qDebug() << SG_PREFIX_I;

	this->m_kind = LayerKind::Aggregate;
	strcpy(this->debug_string, "LayerKind::Aggregate");

	this->interface = &vik_aggregate_layer_interface;
	this->set_name(Layer::get_translated_layer_kind_string(this->m_kind));

	this->children = new std::list<Layer *>;
}




void LayerAggregate::child_tree_item_changed_cb(const QString & child_tree_item_name) /* Slot. */
{
	qDebug() << SG_PREFIX_SLOT << "Layer" << this->get_name() << "received 'child tree item changed' signal from" << child_tree_item_name;
	if (this->is_visible()) {
		/* TODO_LATER: this can used from the background - e.g. in acquire
		   so will need to flow background update status through too. */
		qDebug() << SG_PREFIX_SIGNAL << "Layer" << this->get_name() << "emits 'changed' signal";
		emit this->tree_item_changed(this->get_name());
	}
}




bool LayerAggregate::is_top_level_layer(void) const
{
	return this == ThisApp::get_layers_panel()->get_top_layer();
}
