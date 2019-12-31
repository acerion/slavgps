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




#include <cstring>
#include <cstdlib>
#include <cassert>




#include <QPushButton>




#include "window.h"
#include "viewport_internal.h"
#include "application_state.h"
#include "layers_panel.h"
#include "layer_aggregate.h"
#include "layer_coord.h"
#include "layer_interface.h"
#include "tree_view_internal.h"
#include "dialog.h"
#include "util.h"
#include "clipboard.h"




using namespace SlavGPS;




#define SG_MODULE "Layers Panel"




LayersPanel::LayersPanel(QWidget * parent_, Window * window_) : QWidget(parent_)
{
	this->panel_box = new QVBoxLayout;
	this->window = window_;

	this->setMaximumWidth(300);
	this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);


	{
		this->toplayer = new LayerAggregate();
		this->toplayer->set_name(tr("Top Layer"));

		this->tree_view = new TreeView(this->toplayer, this);
		this->panel_box->addWidget(this->tree_view);
		this->tree_view->show();
	}

	{
		this->tool_bar = new QToolBar();
		this->panel_box->addWidget(this->tool_bar);


		this->qa_layer_add = new QAction(tr("Add"), this);
		this->qa_layer_add->setToolTip(tr("Add new layer"));
		this->qa_layer_add->setIcon(QIcon::fromTheme("list-add"));
		connect(this->qa_layer_add, SIGNAL (triggered(bool)), this, SLOT (add_layer_cb()));

		this->qa_layer_remove = new QAction(tr("Remove"), this);
		this->qa_layer_remove->setToolTip(tr("Remove selected item"));
		this->qa_layer_remove->setIcon(QIcon::fromTheme("list-remove"));
		connect(this->qa_layer_remove, SIGNAL (triggered(bool)), this, SLOT (delete_selected_cb()));

		this->qa_layer_move_up = new QAction(tr("Up"), this);
		this->qa_layer_move_up->setToolTip(tr("Move selected item up"));
		this->qa_layer_move_up->setIcon(QIcon::fromTheme("go-up"));
		connect(this->qa_layer_move_up, SIGNAL (triggered(bool)), this, SLOT (move_item_up_cb()));

		this->qa_layer_move_down = new QAction(tr("Down"), this);
		this->qa_layer_move_down->setToolTip(tr("Move selected item down"));
		this->qa_layer_move_down->setIcon(QIcon::fromTheme("go-down"));
		connect(this->qa_layer_move_down, SIGNAL (triggered(bool)), this, SLOT (move_item_down_cb()));

		this->qa_layer_cut = new QAction(tr("Cut"), this);
		this->qa_layer_cut->setToolTip(tr("Cut selected item"));
		this->qa_layer_cut->setIcon(QIcon::fromTheme("edit-cut"));
		connect(this->qa_layer_cut, SIGNAL (triggered(bool)), this, SLOT (cut_selected_cb()));

		this->qa_layer_copy = new QAction(tr("Copy"), this);
		this->qa_layer_copy->setToolTip(tr("Copy selected item"));
		this->qa_layer_copy->setIcon(QIcon::fromTheme("edit-copy"));
		connect(this->qa_layer_copy, SIGNAL (triggered(bool)), this, SLOT (copy_selected_cb()));

		this->qa_layer_paste = new QAction(tr("Paste"), this);
		this->qa_layer_paste->setToolTip(tr("Paste item into selected container"));
		this->qa_layer_paste->setIcon(QIcon::fromTheme("edit-paste"));
		connect(this->qa_layer_paste, SIGNAL (triggered(bool)), this, SLOT (paste_selected_cb()));

		this->tool_bar->addAction(qa_layer_add);
		this->tool_bar->addAction(qa_layer_remove);
		this->tool_bar->addAction(qa_layer_move_up);
		this->tool_bar->addAction(qa_layer_move_down);
		this->tool_bar->addAction(qa_layer_cut);
		this->tool_bar->addAction(qa_layer_copy);
		this->tool_bar->addAction(qa_layer_paste);
	}



	this->setLayout(this->panel_box);


	connect(this->tree_view, SIGNAL(tree_item_needs_redraw(sg_uid_t)), this->window, SLOT(draw_layer_cb(sg_uid_t)));
	connect(this->toplayer, SIGNAL(tree_item_changed(const QString &)), this, SLOT(emit_items_tree_updated_cb(const QString &)));
	connect(this->tree_view, SIGNAL (tree_item_selected(void)), this, SLOT (activate_buttons_cb(void)));


	this->activate_buttons_cb();
}




LayersPanel::~LayersPanel()
{
	qDebug() << SG_PREFIX_I;

	delete this->toplayer;
	delete this->tree_view;
	delete this->panel_box;
	delete this->tool_bar;
}




void LayersPanel::emit_items_tree_updated_cb(const QString & trigger_name)
{
	qDebug() << "SLOT?: Layers Panel received 'changed' signal from top level layer?" << trigger_name;
	qDebug() << SG_PREFIX_SIGNAL << "Will emit 'items_tree_updated' signal";
	emit this->items_tree_updated();
}




void LayersPanel::keyPressEvent(QKeyEvent * ev)
{
	/* Accept all forms of delete keys. */
	if (ev->key() == Qt::Key_Delete || ev->key() == Qt::Key_Backspace) {
		this->delete_selected_cb();
		ev->accept();
		return;
	}

	/* Let base class handle unhandled events. */
	QWidget::keyPressEvent(ev);
}




/**
   Create basic part of context menu for a click occurring anywhere in
   tree view (both on an item and outside of item).

   Number and type of menu items is controlled by \param layer_menu_items.
   Returned pointer is owned by caller.

   \param layer_menu_items - bit sum of TreeItemOperation values

   \return freshly created menu with specified items
*/
void LayersPanel::context_menu_add_standard_operations(QMenu & menu, const StandardMenuOperations & ops)
{
	if (ops.is_member(StandardMenuOperation::Properties)) {
		menu.addAction(this->window->qa_tree_item_properties);
	}

	if (ops.is_member(StandardMenuOperation::Cut)) {
		menu.addAction(this->qa_layer_cut);
	}

	if (ops.is_member(StandardMenuOperation::Copy)) {
		menu.addAction(this->qa_layer_copy);
	}

	if (ops.is_member(StandardMenuOperation::Paste)) {
		menu.addAction(this->qa_layer_paste);
	}

	if (ops.is_member(StandardMenuOperation::Delete)) {
		menu.addAction(this->qa_layer_remove);
	}

	if (ops.is_member(StandardMenuOperation::New)) {
		this->context_menu_add_new_layer_submenu(menu);
	}
}




void LayersPanel::context_menu_add_new_layer_submenu(QMenu & menu)
{
	QMenu * layers_submenu = new QMenu(tr("New Layer"), &menu);
	menu.addMenu(layers_submenu);
	this->window->new_layers_submenu_add_actions(layers_submenu);
}




void LayersPanel::context_menu_show_for_item(TreeItem & item) // KAMIL TODO
{
	qDebug() << SG_PREFIX_I << "Context menu event for" << item.m_type_id << item.get_name();
	QMenu menu;
	if (!item.menu_add_tree_item_operations(menu, true)) {
		return;
	}
	menu.exec(QCursor::pos());

	return;

}




void LayersPanel::context_menu_show_for_new_layer(void)
{
	QMenu menu(this);

	/* Put only "New" item in the context menu. */
	StandardMenuOperations ops;
	ops.push_back(StandardMenuOperation::New);

	this->context_menu_add_standard_operations(menu, ops);
	menu.exec(QCursor::pos());
}




/**
 * @layer: existing layer
 *
 * Add an existing layer to panel.
 */
void LayersPanel::add_layer(Layer * layer, const CoordMode & viewport_coord_mode)
{
	/* Could be something different so we have to do this. */
	layer->change_coord_mode(viewport_coord_mode);

	qDebug() << SG_PREFIX_I << "attempting to add layer named" << layer->get_name();


	TreeItem * selected_item = this->tree_view->get_selected_tree_item();
	if (!selected_item) {
		/* No particular layer is selected in panel, so the
		   layer to be added goes directly under top level
		   aggregate layer. */
		qDebug() << SG_PREFIX_I << "No selected layer, adding layer named" << layer->get_name() << "under Top Level Layer";
		this->toplayer->add_child_item(layer, true);

		qDebug() << SG_PREFIX_SIGNAL << "Will call 'emit_items_tree_updated_cb()' after adding layer named" << layer->get_name();
		this->emit_items_tree_updated_cb(layer->get_name());

		return;
	}


	/* If selected item is layer, then the layer itself is
	   returned here. Otherwise, parent/owning layer of selected
	   sublayer is returned. */
	Layer * selected_layer = selected_item->get_immediate_layer();
	assert (selected_layer->tree_view);
	assert (selected_layer->index.isValid());
	TreeIndex selected_layer_index = selected_layer->index;
	qDebug() << SG_PREFIX_I << "Selected layer is named" << selected_layer->get_name();


	if (selected_layer->m_kind == LayerKind::Aggregate) {
		/* If selected layer is Aggregate layer, we want new
		   layer to go into this selected Aggregate layer.

		   Notice that this case also covers situation when
		   selected Aggregate layer is Top Level Layer. */

		qDebug() << SG_PREFIX_I << "Selected layer is Aggregate layer named" << selected_layer->get_name() << ", adding layer named" << layer->get_name() << "under that Aggregate layer";

		selected_layer->add_child_item(layer, true);

		qDebug() << SG_PREFIX_SIGNAL << "Will call 'emit_items_tree_updated_cb()' after adding layer named" << layer->get_name();
		this->emit_items_tree_updated_cb(layer->get_name());

		return;
	}


	/* Some non-Aggregate layer is selected. Since we can insert
	   layers only under Aggregate layer, let's find the Aggregate
	   layer by going up in hierarchy. */
	qDebug() << SG_PREFIX_I << "Selected layer is non-Aggregate layer named" << selected_layer->get_name() << ", looking for Aggregate layer";
	Layer * aggregate_candidate = this->go_up_to_layer(selected_layer, LayerKind::Aggregate);
	if (aggregate_candidate) {
		LayerAggregate * aggregate = (LayerAggregate *) aggregate_candidate;
		assert (aggregate->tree_view);

		qDebug() << SG_PREFIX_I << "Found closest Aggregate layer named" << aggregate->get_name() << ", adding layer named" << layer->get_name() << "under that Aggregate layer";

		aggregate->insert_layer(layer, selected_layer); /* Insert layer next to selected layer. */

		qDebug() << SG_PREFIX_SIGNAL << "Will call 'emit_items_tree_updated_cb()' after adding layer named" << layer->get_name();
		this->emit_items_tree_updated_cb(layer->get_name());

		return;
	}


	/* TODO_LATER: add error handling? */
	qDebug() << SG_PREFIX_E << "Can't find place for new layer";

	return;
}




void LayersPanel::move_item(bool up)
{
	TreeItem * selected_item = this->tree_view->get_selected_tree_item();
	if (!selected_item) {
		this->activate_buttons_cb();
		return;
	}

	if (TreeItem::the_same_object(selected_item, this->toplayer)) {
		/* We shouldn't even get here. "Move up/down" buttons
		   should be disabled for Top Level Layer. */
		qDebug() << SG_PREFIX_W << "Ignoring attempt to move Top Level Layer";
		return;
	}

	this->tree_view->select_tree_item(selected_item); /* Cancel any layer-name editing going on... */

	if (selected_item->get_direct_parent_tree_item()->move_child(*selected_item, up)) {          /* This moves child item in parent item's container. */
		this->tree_view->change_tree_item_position(selected_item, up);      /* This moves child item in tree. */

		qDebug() << SG_PREFIX_SIGNAL << "Will call 'emit_items_tree_updated_cb()' for" << selected_item->get_direct_parent_tree_item()->get_name();
		this->emit_items_tree_updated_cb(selected_item->get_direct_parent_tree_item()->get_name());
	}
}




void LayersPanel::draw_tree_items(GisViewport * gisview, bool highlight_selected, bool parent_is_selected)
{
	if (!gisview || !this->toplayer->is_visible()) {
		return;
	}

	/* We call ::get_highlight_usage() here, on top of call chain,
	   so that all layer items can use this information and don't
	   have to call ::get_highlight_usage() themselves. */
	highlight_selected = highlight_selected && gisview->get_highlight_usage();
	qDebug() << SG_PREFIX_I << "Calling toplayer->draw_tree_item(highlight_selected =" << highlight_selected << "parent_is_selected =" << parent_is_selected << ")";
	this->toplayer->draw_tree_item(gisview, highlight_selected, parent_is_selected);

	/* TODO_LATER: layers panel or tree view or aggregate layer should
	   recognize which layer lays under non-transparent layers,
	   and don't draw the layer hidden under non-transparent
	   layer. */
}




void LayersPanel::cut_selected_cb(void) /* Slot. */
{
	TreeItem * selected_item = this->tree_view->get_selected_tree_item();
	if (nullptr == selected_item) {
		this->activate_buttons_cb();
		/* Nothing to do. */
		return;
	}


	/* Special case for top-level Aggregate layer. */
	if (selected_item->is_layer()) {
		Layer * layer = selected_item->get_immediate_layer();
		if (layer->m_kind == LayerKind::Aggregate) {
			if (((LayerAggregate *) selected_item)->is_top_level_layer()) {
				Dialog::info(tr("You cannot cut the Top Layer."), this->window);
				return;
			}
		}
	}

	Layer * owning_layer = selected_item->get_owning_layer();
	owning_layer->cut_child_item(selected_item);

	return;
}




void LayersPanel::copy_selected_cb(void) /* Slot. */
{
	TreeItem * selected_item = this->tree_view->get_selected_tree_item();
	if (!selected_item) {
		this->activate_buttons_cb();
		/* Nothing to do. */
		return;
	}

	/* Clipboard contains layer vs sublayer logic, so don't need to do it here. */
	Clipboard::copy_selected(this);
}




bool LayersPanel::paste_selected_cb(void) /* Slot. */
{
	TreeItem * selected_item = this->tree_view->get_selected_tree_item();
	if (!selected_item) {
		this->activate_buttons_cb();
		/* Nothing to do. */
		return false;
	}

	bool pasted = false;
	if (sg_ret::ok != Clipboard::paste(this, pasted)) {
		return false;
	}

	return pasted;
}




void LayersPanel::add_layer_cb(void)
{
	this->context_menu_show_for_new_layer();
}




void LayersPanel::delete_selected_cb(void) /* Slot. */
{
	TreeItem * selected_item = this->tree_view->get_selected_tree_item();
	if (nullptr == selected_item) {
		this->activate_buttons_cb();
		/* Nothing to do. */
		return;
	}

	/* Special case for top-level Aggregate layer. */
	if (selected_item->is_layer()) {
		Layer * layer = selected_item->get_immediate_layer();
		if (layer->m_kind == LayerKind::Aggregate) {
			if (((LayerAggregate *) selected_item)->is_top_level_layer()) {
				Dialog::info(tr("You cannot delete the Top Layer."), this->window);
				return;
			}
		}
	}

	Layer * owning_layer = selected_item->get_owning_layer();
	/* true: confirm delete request. */
	owning_layer->delete_child_item(selected_item, true);

	// TODO_LATER? this->activate_buttons_cb();
}




void LayersPanel::move_item_up_cb(void)
{
	this->move_item(true);
}




void LayersPanel::move_item_down_cb(void)
{
	this->move_item(false);
}




Layer * LayersPanel::get_selected_layer()
{
	TreeItem * selected_item = this->tree_view->get_selected_tree_item();
	if (!selected_item) {
		return NULL;
	}

	/* If a layer is selected, return the layer itself.
	   If a sublayer is selected, return its parent/owning layer. */
	return selected_item->get_immediate_layer();
}




Layer * LayersPanel::get_layer_of_kind(LayerKind layer_kind)
{
	Layer * layer = this->get_selected_layer();
	if (layer && layer->m_kind == layer_kind) {
		return layer;
	}

	if (this->toplayer->is_visible()) {
		return this->toplayer->get_top_visible_layer_of_type(layer_kind);
	}

	return NULL;
}




std::list<const Layer *> LayersPanel::get_all_layers_of_kind(LayerKind layer_kind, bool include_invisible) const
{
	std::list<const Layer *> layers;
	this->toplayer->get_all_layers_of_kind(layers, layer_kind, include_invisible);
	return layers;
}




bool LayersPanel::has_any_layer_of_kind(LayerKind layer_kind)
{
	std::list<const Layer *> layers = this->get_all_layers_of_kind(layer_kind, true); /* Includes hidden layers. */
	return layers.size () > 0;
}




LayerAggregate * LayersPanel::get_top_layer()
{
	if (NULL == this->toplayer) {
		qDebug() << SG_PREFIX_E << "Top layer is NULL";
	}

	return this->toplayer;
}




void LayersPanel::clear(void)
{
	if (0 != this->toplayer->get_child_layers_count()) {
		this->toplayer->clear(); /* Delete all layers. */
		this->emit_items_tree_updated_cb("Delete all layers through layers panel");
	}
}




void LayersPanel::change_coord_mode(CoordMode mode)
{
	this->toplayer->change_coord_mode(mode);
}




void LayersPanel::set_visible(bool visible)
{
	this->window->set_side_panel_visibility_cb(visible);
	return;
}




bool LayersPanel::get_visible(void) const
{
	return this->window->get_side_panel_visibility();
}




TreeView * LayersPanel::get_tree_view()
{
	return this->tree_view;
}




void LayersPanel::contextMenuEvent(QContextMenuEvent * ev)
{
	if (!this->tree_view->geometry().contains(ev->pos())) {
		qDebug() << SG_PREFIX_I << "Context menu event outside tree view";
		/* We want to handle only events that happen inside of tree view. */
		return;
	}

	qDebug() << SG_PREFIX_I << "Context menu event inside tree view";

	QPoint orig = ev->pos();
	QPoint v = this->tree_view->pos();
	QPoint t = this->tree_view->viewport()->pos();

	qDebug() << SG_PREFIX_D << "Context menu event: event @" << orig.x() << orig.y();
	qDebug() << SG_PREFIX_D << "Context menu event: viewport @" << v;
	qDebug() << SG_PREFIX_D << "Context menu event: tree view @" << t;

	QPoint point;
	point.setX(orig.x() - v.x() - t.x());
	point.setY(orig.y() - v.y() - t.y());

	QModelIndex ind = this->tree_view->indexAt(point);

	if (ind.isValid()) {
		/* We have clicked on some valid item in tree view. */

		qDebug() << SG_PREFIX_I << "Context menu event: valid tree view index, row =" << ind.row();
		TreeItem * item = this->tree_view->get_tree_item(QPersistentModelIndex(ind));
		if (nullptr == item) {
			qDebug() << SG_PREFIX_E << "Tree item is NULL";
		} else {
			this->context_menu_show_for_item(*item);
		}
	} else {
		/* We have clicked on empty space, not on tree item.  */

		qDebug() << SG_PREFIX_I << "Context menu event: tree view not hit";
		if (!this->tree_view->viewport()->geometry().contains(ev->pos())) {
			qDebug() << SG_PREFIX_I << "Context menu event outside of tree view's viewport";
			return;
		} else {
			qDebug() << SG_PREFIX_I << "Context menu event inside of tree view's viewport";
		}

		this->context_menu_show_for_new_layer();
	}
	return;
}




/*
  Go up the tree to find a Layer of given type

  If @param tree_item already refers to layer of given type, the
  function doesn't really go up, it returns the same tree item.

  If you want to skip tree item pointed to by @param tree_item and
  start from its parent, you have to calculate parent by yourself and
  pass the parent to this function.
*/
Layer * LayersPanel::go_up_to_layer(const TreeItem * tree_item, LayerKind expected_layer_kind)
{
	TreeIndex const & item_index = tree_item->index;
        TreeIndex this_index = item_index;
	TreeIndex parent_index;

	while (1) {
		if (!this_index.isValid()) {
			return NULL; /* Returning invalid layer. */
		}

		TreeItem * this_item = this->tree_view->get_tree_item(this_index);
		if (this_item->is_layer()) {
			if (((Layer *) this_item)->m_kind == expected_layer_kind) {
				return (Layer *) this_item; /* Returning matching layer. */
			}
		}

		/* Go one step up to parent. */
		parent_index = this_index.parent();

		/* Parent also may be invalid. */
		if (!parent_index.isValid()) {
			return NULL; /* Returning invalid layer */
		}

		this_index = parent_index;
	}
}




void LayersPanel::activate_buttons_cb(void)
{
	/* Deactivate all buttons now and activate them only if necessary below. */
	this->qa_layer_add->setEnabled(false);
	this->qa_layer_remove->setEnabled(false);
	this->qa_layer_move_up->setEnabled(false);
	this->qa_layer_move_down->setEnabled(false);
	this->qa_layer_cut->setEnabled(false);
	this->qa_layer_copy->setEnabled(false);
	this->qa_layer_paste->setEnabled(false);



	TreeItem * selected_item = this->tree_view->get_selected_tree_item();
	if (NULL == selected_item) {
		qDebug() << SG_PREFIX_I << "Leaving all buttons disabled.";
		return;
	}


	if (TreeItem::the_same_object(selected_item, this->toplayer)) {
		/* Not an error. Simply don't enable buttons for Top Level Layer. */
		return;
	}


	/* Get position among selected item's siblings */
	bool is_first = false;
	bool is_last = false;
	if (sg_ret::ok != this->tree_view->get_position(*selected_item, is_first, is_last)) {
		qDebug() << SG_PREFIX_E << "Failed to get position of tree item" << selected_item->get_name();
		return;
	}


	/* OK, we have some item selected and we've got its position,
	   so now we can enable its 'edit' buttons. */
	this->qa_layer_add->setEnabled(true);
	this->qa_layer_remove->setEnabled(true);
	this->qa_layer_cut->setEnabled(true);
	this->qa_layer_copy->setEnabled(true);
	this->qa_layer_paste->setEnabled(true);


	/* Now see what we can do to "move up"/"move down" buttons. */


	if (is_first && is_last) {
		/* This may happen if the item doesn't have any
		   siblings. We can't move such item up nor down. */
		return;
	}

	if (is_first) {
		this->qa_layer_move_down->setEnabled(true);
	} else if (is_last) {
		this->qa_layer_move_up->setEnabled(true);
	} else {
		this->qa_layer_move_up->setEnabled(true);
		this->qa_layer_move_down->setEnabled(true);
	}

	return;
}
