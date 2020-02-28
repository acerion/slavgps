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

	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {
		TreeItem * child = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &child)) {
			qDebug() << SG_PREFIX_E << "Failed to get child item in row" << row << "/" << rows;
			continue;
		}

		Pickle helper_pickle;
		child->marshall(helper_pickle);
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
			aggregate->non_attached_children.push_front(child_layer);
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

	layer->set_parent_and_owner_tree_item(this);

	if (this->tree_view) {
		this->tree_view->debug_print_tree();

		/* This call sets TreeItem::index and TreeItem::tree_view of added item. */
		qDebug() << SG_PREFIX_I << "Attaching item" << layer->get_name() << "to tree under" << this->get_name();
		layer->attach_to_tree_under_parent(this, attach_mode, sibling_layer);

		QObject::connect(layer, SIGNAL (tree_item_changed(const QString &)), this, SLOT (child_tree_item_changed_cb(const QString &)));

		/* Update our own tooltip in tree view. */
		this->update_tree_item_tooltip();

#if K_TODO_LATER
		if (!this->non_attached_children->empty()) {
			this->tree_view->expand(this->index);
		}
#endif
	} else {
		if (sibling_layer->index().isValid()) {

			auto sibling_iter = this->non_attached_children.end();
			for (auto iter = this->non_attached_children.begin(); iter != this->non_attached_children.end(); iter++) {
				if (TreeItem::the_same_object(sibling_layer, *iter)) {
					sibling_iter = iter;
				}
			}

			qDebug() << SG_PREFIX_I << "Aggregate::children: adding" << layer->get_name();
			/* ::insert() inserts before given iterator. */
			if (TreeViewAttachMode::Before == attach_mode) {
				this->non_attached_children.insert(sibling_iter, layer);
			} else {
				this->non_attached_children.insert(std::next(sibling_iter), layer);
			}
		} else {
			qDebug() << SG_PREFIX_I << "Aggregate::children: adding" << layer->get_name();
			/* Effectively insert at 'end' of the list to match how displayed in the tree view
			   - but since it is drawn from 'bottom first' it is actually the first in the child list.
			   This ordering is especially important if it is a map or similar type,
			   which needs be drawn first for the layering draw method to work properly.
			   ATM this only happens when a layer is drag/dropped to the end of an aggregate layer. */
			this->non_attached_children.push_back(layer);
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

	Layer * layer = item->immediate_layer();

	/* By default layers go to the top. */
	bool put_above = true;

	if (allow_reordering && is_base_type(layer->m_kind)) {
		put_above = false;
	}

	layer->set_parent_and_owner_tree_item(this);

	qDebug() << SG_PREFIX_I << "Aggregate::children: adding" << layer->get_name();
	if (put_above) {
		/* This call sets TreeItem::index and TreeItem::tree_view of added item. */
		qDebug() << SG_PREFIX_I << "Attaching item" << layer->get_name() << "to tree under" << this->get_name();
		layer->attach_to_tree_under_parent(this, TreeViewAttachMode::Front);
	} else {
		/* This call sets TreeItem::index and TreeItem::tree_view of added item. */
		qDebug() << SG_PREFIX_I << "Attaching item" << layer->get_name() << "to tree under" << this->get_name();
		layer->attach_to_tree_under_parent(this);
	}
	this->tree_view->apply_tree_item_timestamp(layer);
	this->tree_view->debug_print_tree();

	QObject::connect(layer, SIGNAL (tree_item_changed(const QString &)), this, SLOT (child_tree_item_changed_cb(const QString &)));

	/* Update our own tooltip in tree view. */
	this->update_tree_item_tooltip();

#if 0
	if (!this->non_attached_children.empty()) {
		this->tree_view->expand(this->index);
	}
#endif

	return sg_ret::ok;
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
	__attribute__((unused)) Layer * trigger = gisview->get_trigger();

	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {
		TreeItem * child = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &child)) {
			qDebug() << SG_PREFIX_E << "Failed to get child item in row" << row << "/" << rows;
			return;
		}

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
		qDebug() << SG_PREFIX_I << "Calling draw_tree_item(" << highlight_selected << parent_is_selected << ") for" << child->get_name();
		child->draw_tree_item(gisview, highlight_selected, parent_is_selected);
#endif
	}
}




void LayerAggregate::change_coord_mode(CoordMode mode)
{
	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {
		TreeItem * child = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &child)) {
			qDebug() << SG_PREFIX_E << "Failed to get child item in row" << row << "/" << rows;
			continue;
		}
		Layer * layer = (Layer *) child;
		layer->change_coord_mode(mode);
	}
}




void LayerAggregate::children_visibility_toggle_cb(void) /* Slot. */
{
	const int changed = this->toggle_direct_children_only_visibility_flag();
	if (changed) {
		/* Redraw as view may have changed. */
		this->emit_tree_item_changed("Requesting redrawing of children of Aggregate layer after visibility flag was toggled");
	}
}




void LayerAggregate::children_visibility_on_cb(void) /* Slot. */
{
	const int changed = this->set_direct_children_only_visibility_flag(true);
	if (changed) {
		/* Redraw as view may have changed. */
		this->emit_tree_item_changed("Requesting redrawing of children of Aggregate layer after visibility flag was set to true");
	}
}




void LayerAggregate::children_visibility_off_cb(void) /* Slot. */
{
	const int changed = this->set_direct_children_only_visibility_flag(false);
	if (changed) {
		/* Redraw as view may have changed. */
		this->emit_tree_item_changed("Requesting redrawing of children of Aggregate layer after visibility flag was set to false");
	}
}




void LayerAggregate::sort_a2z_cb(void) /* Slot. */
{
	this->tree_view->sort_children(this, TreeViewSortOrder::AlphabeticalAscending);
}




void LayerAggregate::sort_z2a_cb(void) /* Slot. */
{
	this->tree_view->sort_children(this, TreeViewSortOrder::AlphabeticalDescending);
}




void LayerAggregate::sort_timestamp_ascend_cb(void) /* Slot. */
{
	this->tree_view->sort_children(this, TreeViewSortOrder::DateAscending);
}




void LayerAggregate::sort_timestamp_descend_cb(void) /* Slot. */
{
	this->tree_view->sort_children(this, TreeViewSortOrder::DateDescending);
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
	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {
		TreeItem * child = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &child)) {
			qDebug() << SG_PREFIX_E << "Failed to get child item in row" << row << "/" << rows;
			continue;
		}

		if (sg_ret::ok != child->get_tree_items(list, wanted_types)) {
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




sg_ret LayerAggregate::menu_add_type_specific_operations(QMenu & menu, __attribute__((unused)) bool in_tree_view)
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
	this->clear();
}




void LayerAggregate::clear()
{
	unsigned int deleted = 0;

	const int rows = this->child_rows_count();
	for (int row = rows - 1; row >= 0; row--) {
		TreeItem * child = nullptr;
		if (sg_ret::ok == this->child_from_row(row, &child)) {
			Layer * layer = (Layer *) child;
			if (layer->is_in_tree()) {
				this->tree_view->detach_tree_item(layer);
			}
			delete layer;
			deleted++;
		} else {
			qDebug() << SG_PREFIX_E << "Failed to get child item in row" << row << "/" << rows;
		}
	}


	if (deleted) {
		/* Update our own tooltip in tree view. */
		this->update_tree_item_tooltip();
	}
}




sg_ret LayerAggregate::delete_child_item(TreeItem * item, __attribute__((unused)) bool confirm_deleting)
{
	const bool was_visible = item->is_visible();

	if (item->is_in_tree()) {
		this->tree_view->detach_tree_item(item);
	} else {
		qDebug() << SG_PREFIX_E << "Tree item" << item->get_name() << "is not in tree";
		/* Don't return yet, try to find the item in list of children. */
	}
	qDebug() << SG_PREFIX_I << "Calling destructor for removed item - begin =================";
	delete item;
	qDebug() << SG_PREFIX_I << "Calling destructor for removed item - end   =================";

	/* Update our own tooltip in tree view. */
	this->update_tree_item_tooltip();

	if (was_visible) {
		qDebug() << SG_PREFIX_SIGNAL << "Layer" << this->get_name() << "emits 'changed' signal";
		emit this->tree_item_changed(this->get_name());
	}

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
	const int rows = this->child_rows_count();
	if (rows <= 0) {
		return nullptr;
	}

	for (int row = rows - 1; row >= 0; row--) {
		TreeItem * child = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &child)) {
			qDebug() << SG_PREFIX_E << "Failed to get child item in row" << row << "/" << rows;
			continue;
		}

		Layer * layer = (Layer *) child;
		if (layer->is_visible() && layer->m_kind == layer_kind) {
			return layer;
		} else if (layer->is_visible() && layer->m_kind == LayerKind::Aggregate) {
			Layer * rv = ((LayerAggregate *) layer)->get_top_visible_layer_of_type(layer_kind);
			if (rv) {
				return rv;
			}
		}
	}

	return nullptr;
}




void LayerAggregate::get_all_layers_of_kind(std::list<Layer const *> & layers, LayerKind expected_layer_kind, bool include_invisible) const
{
	/* Where appropriate *don't* include non-visible layers. */
	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {
		TreeItem * child = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &child)) {
			qDebug() << SG_PREFIX_E << "Failed to get child item in row" << row << "/" << rows;
			continue;
		}

		Layer * layer = (Layer *) child;
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
	}
	return;
}




bool LayerAggregate::handle_select_tool_click(QMouseEvent * event, GisViewport * gisview, LayerToolSelect * select_tool)
{
	if (!this->is_visible()) {
		/* In practice this condition will be checked for
		   top-level aggregate layer only. For child aggregate
		   layers the visibility condition in a loop below
		   will be tested first, before a call to the child's
		   handle_select_tool_click(). */
		return false;
	}

	bool has_been_handled = false;

	/* For each layer keep adding the specified tree items
	   to build a list of all of them. */
	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {
		TreeItem * child = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &child)) {
			qDebug() << SG_PREFIX_E << "Failed to get child item in row" << row << "/" << rows;
			continue;
		}
		Layer * layer = (Layer *) child;

		if (!child->is_visible()) {
			continue;
		}

		if (event->flags() & Qt::MouseEventCreatedDoubleClick) {
			has_been_handled = layer->handle_select_tool_double_click(event, gisview, select_tool);
		} else {
			has_been_handled = layer->handle_select_tool_click(event, gisview, select_tool);
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




sg_ret LayerAggregate::post_read_2(void)
{
	if (this->non_attached_children.empty()) {
		return sg_ret::ok;
	}

	for (auto iter = this->non_attached_children.begin(); iter != this->non_attached_children.end(); iter++) {
		Layer * layer = *iter;

		/* This call sets TreeItem::index and TreeItem::tree_view of added item. */
		qDebug() << SG_PREFIX_I << "Attaching item" << layer->get_name() << "to tree under" << this->get_name();
		layer->attach_to_tree_under_parent(this);
	}
	this->non_attached_children.clear();


	/* Update our own tooltip in tree view. */
	this->update_tree_item_tooltip();

	return sg_ret::ok;
}




std::list<Layer const *> LayerAggregate::get_child_layers(void) const
{
	std::list<Layer const *> result;

	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {
		TreeItem * child = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &child)) {
			qDebug() << SG_PREFIX_E << "Failed to get child item in row" << row << "/" << rows;
			continue;
		}
		result.push_back((Layer *) child);
	}

	qDebug() << SG_PREFIX_I << "Returning" << result.size() << "children";
	return result;
}




int LayerAggregate::get_child_layers_count(void) const
{
	const int rows = this->child_rows_count();
	if (rows <= 0) {
		return 0;
	} else {
		return rows;
	}
}




sg_ret LayerAggregate::drag_drop_request(TreeItem * tree_item, __attribute__((unused)) int row, __attribute__((unused)) int col)
{
#if K_TODO_LATER
	/* Handle item in old location. */
	{
		/* TODO_LATER: implement detaching from parent tree
		   item's container where tree item is of any kind. */

		assert (layer->is_in_tree());
		assert (TreeItem::the_same_object(this->tree_view->get_tree_item(layer->index)->immediate_layer(), layer));

		if (NULL != was_visible) {
			*was_visible = layer->is_visible();
		}

		for (auto iter = this->children->begin(); iter != this->children->end(); iter++) {
			if (TreeItem::the_same_object(layer, *iter)) {
				this->children->erase(iter);
				break;
			}
		}

		/* Detaching of tree item from tree view will be handled by QT. */
	}

	/* Handle item in new location. */
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

	}
#endif
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

	int rows = this->child_rows_count();
	if (rows < 0) {
		qDebug() << SG_PREFIX_E << "Can't get count of children of Aggregate layer" << this->get_name();
		rows = 0;
	}

	return tr("%n immediate child layer(s)", "", rows);
}




LayerAggregate::LayerAggregate()
{
	qDebug() << SG_PREFIX_I;

	this->m_kind = LayerKind::Aggregate;
	strcpy(this->debug_string, "LayerKind::Aggregate");

	this->interface = &vik_aggregate_layer_interface;
	this->set_name(Layer::get_translated_layer_kind_string(this->m_kind));
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
	return this == ThisApp::layers_panel()->top_layer();
}
