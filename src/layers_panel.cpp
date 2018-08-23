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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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




#define PREFIX ": Layers Panel:" << __FUNCTION__ << __LINE__ << ">"




extern Tree * g_tree;




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


		this->qa_layer_add = new QAction("Add", this);
		this->qa_layer_add->setToolTip("Add new layer");
		this->qa_layer_add->setIcon(QIcon::fromTheme("list-add"));
		connect(this->qa_layer_add, SIGNAL (triggered(bool)), this, SLOT (add_layer_cb()));

		this->qa_layer_remove = new QAction("Remove", this);
		this->qa_layer_remove->setToolTip("Remove selected layer");
		this->qa_layer_remove->setIcon(QIcon::fromTheme("list-remove"));
		connect(this->qa_layer_remove, SIGNAL (triggered(bool)), this, SLOT (delete_selected_cb()));

		this->qa_layer_move_up = new QAction("Up", this);
		this->qa_layer_move_up->setToolTip("Move selected layer up");
		this->qa_layer_move_up->setIcon(QIcon::fromTheme("go-up"));
		connect(this->qa_layer_move_up, SIGNAL (triggered(bool)), this, SLOT (move_item_up_cb()));

		this->qa_layer_move_down = new QAction("Down", this);
		this->qa_layer_move_down->setToolTip("Move selected layer down");
		this->qa_layer_move_down->setIcon(QIcon::fromTheme("go-down"));
		connect(this->qa_layer_move_down, SIGNAL (triggered(bool)), this, SLOT (move_item_down_cb()));

		this->qa_layer_cut = new QAction("Cut", this);
		this->qa_layer_cut->setToolTip("Cut selected layer");
		this->qa_layer_cut->setIcon(QIcon::fromTheme("edit-cut"));
		connect(this->qa_layer_cut, SIGNAL (triggered(bool)), this, SLOT (cut_selected_cb()));

		this->qa_layer_copy = new QAction("Copy", this);
		this->qa_layer_copy->setToolTip("Copy selected layer");
		this->qa_layer_copy->setIcon(QIcon::fromTheme("edit-copy"));
		connect(this->qa_layer_copy, SIGNAL (triggered(bool)), this, SLOT (copy_selected_cb()));

		this->qa_layer_paste = new QAction("Paste", this);
		this->qa_layer_paste->setToolTip("Paste layer into selected container layer or otherwise above selected layer");
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
	connect(this->toplayer, SIGNAL(layer_changed(const QString &)), this, SLOT(emit_items_tree_updated_cb(const QString &)));
}




LayersPanel::~LayersPanel()
{
	qDebug() << "II: Layers Panel: ~LayersPanel() called";

	delete this->toplayer;
	delete this->tree_view;
	delete this->panel_box;
	delete this->tool_bar;
}




void LayersPanel::emit_items_tree_updated_cb(const QString & trigger_name)
{
	qDebug() << "SLOT?: Layers Panel received 'changed' signal from top level layer?" << trigger_name;
	qDebug() << "SIGNAL: Layers Panel emits 'update' signal";
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
void LayersPanel::context_menu_add_standard_items(QMenu * menu, TreeItem::MenuOperation menu_operations)
{
	if (operator_bit_and(menu_operations, TreeItem::MenuOperation::Properties)) {
		menu->addAction(this->window->qa_tree_item_properties);
	}

	if (operator_bit_and(menu_operations, TreeItem::MenuOperation::Cut)) {
		menu->addAction(this->qa_layer_cut);
	}

	if (operator_bit_and(menu_operations, TreeItem::MenuOperation::Copy)) {
		menu->addAction(this->qa_layer_copy);
	}

	if (operator_bit_and(menu_operations, TreeItem::MenuOperation::Paste)) {
		menu->addAction(this->qa_layer_paste);
	}

	if (operator_bit_and(menu_operations, TreeItem::MenuOperation::Delete)) {
		menu->addAction(this->qa_layer_remove);
	}

	if (operator_bit_and(menu_operations, TreeItem::MenuOperation::New)) {
		this->context_menu_add_new_layer_submenu(menu);
	}
}




void LayersPanel::context_menu_add_new_layer_submenu(QMenu * menu)
{
	QMenu * layers_submenu = new QMenu(tr("New Layer"));
	menu->addMenu(layers_submenu);
	this->window->new_layers_submenu_add_actions(layers_submenu);
}




void LayersPanel::context_menu_show_for_item(TreeItem * item)
{
	if (!item) {
		qDebug() << "EE: Layers Panel: show context menu for item: NULL item";
		return;
	}

	QMenu menu;

	if (item->tree_item_type == TreeItemType::LAYER) {

		qDebug() << "II: Layers Panel: context menu event: menu for layer" << item->type_id << item->name;

		Layer * layer = item->to_layer();

		/* "New layer -> layer types" submenu. */
		TreeItem::MenuOperation operations = layer->get_menu_operation_ids();
		operations = operator_bit_or(operations, TreeItem::MenuOperation::New);
		this->context_menu_add_standard_items(&menu, operations);

		/* Layer-type-specific menu items. */
		layer->add_menu_items(menu);
	} else {
		qDebug() << "II: Layers Panel: context menu event: menu for sublayer" << item->type_id << item->name;


		if (!item->add_context_menu_items(menu, true)) {
			return;
		}
		/* TODO_LATER: specific things for different types. */
	}

	menu.exec(QCursor::pos());

	return;

}




void LayersPanel::context_menu_show_for_new_layer(void)
{
	QMenu menu(this);
	/* Put only "New" item in the context menu. */
	this->context_menu_add_standard_items(&menu, TreeItem::MenuOperation::New);
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

	qDebug() << "II" PREFIX << "attempting to add layer named" << layer->get_name();


	TreeItem * selected_item = this->tree_view->get_selected_tree_item();
	if (!selected_item) {
		/* No particular layer is selected in panel, so the
		   layer to be added goes directly under top level
		   aggregate layer. */
		qDebug() << "II" PREFIX << "No selected layer, adding layer named" << layer->get_name() << "under Top Level Layer";
		this->toplayer->add_layer(layer, true);

		qDebug() << "SIGNAL" PREFIX << "Will call 'emit_items_tree_updated_cb()' after adding layer named" << layer->get_name();
		this->emit_items_tree_updated_cb(layer->get_name());

		return;
	}


	/* If selected item is layer, then the layer itself is
	   returned here. Otherwise, parent/owning layer of selected
	   sublayer is returned. */
	Layer * selected_layer = selected_item->to_layer();
	assert (selected_layer->tree_view);
	assert (selected_layer->index.isValid());
	TreeIndex selected_layer_index = selected_layer->index;
	qDebug() << "II" PREFIX << "Selected layer is named" << selected_layer->get_name();


	if (selected_layer->type == LayerType::Aggregate) {
		/* If selected layer is Aggregate layer, we want new
		   layer to go into this selected Aggregate layer.

		   Notice that this case also covers situation when
		   selected Aggregate layer is Top Level Layer. */

		qDebug() << "II" PREFIX << "Selected layer is Aggregate layer named" << selected_layer->get_name() << ", adding layer named" << layer->get_name() << "under that Aggregate layer";

		((LayerAggregate *) selected_layer)->add_layer(layer, true);

		qDebug() << "SIGNAL" PREFIX << "Will call 'emit_items_tree_updated_cb()' after adding layer named" << layer->get_name();
		this->emit_items_tree_updated_cb(layer->get_name());

		return;
	}


	/* Some non-Aggregate layer is selected. Since we can insert
	   layers only under Aggregate layer, let's find the Aggregate
	   layer by going up in hierarchy. */
	qDebug() << "II" PREFIX << "Selected layer is non-Aggregate layer named" << selected_layer->get_name() << ", looking for Aggregate layer";
	Layer * aggregate_candidate = this->go_up_to_layer(selected_layer, LayerType::Aggregate);
	if (aggregate_candidate) {
		LayerAggregate * aggregate = (LayerAggregate *) aggregate_candidate;
		assert (aggregate->tree_view);

		qDebug() << "II" PREFIX << "Found closest Aggregate layer named" << aggregate->get_name() << ", adding layer named" << layer->get_name() << "under that Aggregate layer";

		aggregate->insert_layer(layer, selected_layer); /* Insert layer next to selected layer. */

		qDebug() << "SIGNAL" PREFIX << "Will call 'emit_items_tree_updated_cb()' after adding layer named" << layer->get_name();
		this->emit_items_tree_updated_cb(layer->get_name());

		return;
	}


	/* TODO_LATER: add error handling? */
	qDebug() << "EE" PREFIX << "Can't find place for new layer";

	return;
}




void LayersPanel::move_item(bool up)
{
	TreeItem * selected_item = this->tree_view->get_selected_tree_item();
	if (!selected_item) {
		/* TODO_LATER: deactivate the buttons and stuff. */
		return;
	}

	this->tree_view->select_tree_item(selected_item); /* Cancel any layer-name editing going on... */

	if (selected_item->tree_item_type == TreeItemType::LAYER) {
		qDebug() << "II" PREFIX << "Move layer" << selected_item->name << (up ? "up" : "down");
		/* A layer can be owned only by Aggregate layer.
		   TODO_LATER: what about TRW layers under GPS layer? */
		LayerAggregate * parent_layer = (LayerAggregate *) selected_item->owning_layer;
		if (parent_layer) { /* Selected item is not top level layer. */
			qDebug() << "-----" PREFIX "step one";
			if (parent_layer->change_child_item_position(selected_item, up)) {
				qDebug() << "-----" PREFIX "step two";
				this->tree_view->change_tree_item_position(selected_item, up);
				qDebug() << "-----" PREFIX "step tree";

				qDebug() << "SIGNAL" PREFIX << "will call 'emit_items_tree_updated_cb()' for" << parent_layer->get_name();
				this->emit_items_tree_updated_cb(parent_layer->get_name());
			}
		}
	} else {
		qDebug() << "II" PREFIX << "Move sublayer" << selected_item->name << (up ? "up" : "down");
	}
}




void LayersPanel::draw_tree_items(Viewport * viewport, bool highlight_selected, bool parent_is_selected)
{
	if (!viewport || !this->toplayer->visible) {
		return;
	}

	/* We call ::get_highlight_usage() here, on top of call chain,
	   so that all layer items can use this information and don't
	   have to call ::get_highlight_usage() themselves. */
	highlight_selected = highlight_selected && viewport->get_highlight_usage();
	qDebug() << "II" PREFIX "calling toplayer->draw_tree_item(highlight_selected =" << highlight_selected << "parent_is_selected =" << parent_is_selected << ")";
	this->toplayer->draw_tree_item(viewport, highlight_selected, parent_is_selected);

	/* K_TODO_LATER: layers panel or tree view or aggregate layer should
	   recognize which layer lays under non-transparent layers,
	   and don't draw the layer hidden under non-transparent
	   layer. */
}




void LayersPanel::cut_selected_cb(void) /* Slot. */
{
	TreeItem * selected_item = this->tree_view->get_selected_tree_item();
	if (!selected_item) {
		/* Nothing to do. */
		return;
	}

	if (selected_item->tree_item_type == TreeItemType::LAYER) {
		/* A layer can be owned only by Aggregate layer.
		   TODO_LATER: what about TRW layers under GPS layer? */
		LayerAggregate * parent_layer = (LayerAggregate *) selected_item->owning_layer;
		if (parent_layer) {
#ifdef K_FIXME_RESTORE
			/* Reset trigger if trigger deleted. */
			if (TreeItem::the_same_object(this->get_selected_layer(), g_tree->tree_get_main_viewport()->get_trigger())) {
				g_tree->tree_get_main_viewport()->set_trigger(NULL);
			}

			Clipboard::copy_selected(this);

			if (parent_layer->type == LayerType::AGGREGATE) {
				g_signal_emit(G_OBJECT(this->panel_box), items_tree_signals[VLP_DELETE_LAYER_SIGNAL], 0);

				if (parent_layer->delete_layer(selected_item)) {
					qDebug() << "SIGNAL" PREFIX << "will call 'emit_items_tree_updated_cb()' for" << parent_layer->get_name();
					this->emit_items_tree_updated_cb(parent_layer->get_name());
				}
			}
#endif
		} else {
			Dialog::info(tr("You cannot cut the Top Layer."), this->window);
		}
	} else if (selected_item->tree_item_type == TreeItemType::SUBLAYER) {
		Layer * parent_layer = this->get_selected_layer();
		parent_layer->cut_sublayer(selected_item);
	}
}




void LayersPanel::copy_selected_cb(void) /* Slot. */
{
	TreeItem * selected_item = this->tree_view->get_selected_tree_item();
	if (!selected_item) {
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
		/* Nothing to do. */
		return false;
	}

	return Clipboard::paste(this);
}




void LayersPanel::add_layer_cb(void)
{
	this->context_menu_show_for_new_layer();
}




void LayersPanel::delete_selected_cb(void) /* Slot. */
{
	TreeItem * selected_item = this->tree_view->get_selected_tree_item();
	if (!selected_item) {
		/* Nothing to do. */
		return;
	}

	if (selected_item->tree_item_type == TreeItemType::LAYER) {
		Layer * layer = selected_item->to_layer();


		/* Get confirmation from the user. */
		if (!Dialog::yes_or_no(tr("Are you sure you want to delete %1?").arg(layer->get_name()), this->window)) {
			return;
		}

		/* A layer can be owned only by Aggregate layer.
		   TODO_LATER: what about TRW layers under GPS layer? */
		LayerAggregate * parent_layer = (LayerAggregate *) selected_item->owning_layer;
		if (parent_layer) {
#ifdef K_FIXME_RESTORE
			/* Reset trigger if trigger deleted. */
			if (TreeItem::the_same_object(this->get_selected_layer(), g_tree->tree_get_main_viewport()->get_trigger())) {
				g_tree->tree_get_main_viewport()->set_trigger(NULL);
			}

			if (parent_layer->type == LayerType::AGGREGATE) {

				g_signal_emit(G_OBJECT(this->panel_box), items_tree_signals[VLP_DELETE_LAYER_SIGNAL], 0);

				if (parent_layer->delete_layer(selected_item)) {
					qDebug() << "SIGNAL" PREFIX << "will call 'emit_items_tree_updated_cb()' for" << layer->get_name();
					this->emit_items_tree_updated_cb(parent_layer->get_name());
				}
			}
#endif
		} else {
			/* We can't delete top-level aggregate layer. */
			Dialog::info(tr("You cannot delete the %1.").arg(layer->get_name()), this->window);
		}
	} else if (selected_item->tree_item_type == TreeItemType::SUBLAYER) {
		Layer * parent_layer = this->get_selected_layer();
		parent_layer->delete_sublayer(selected_item);
	}
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
	return selected_item->to_layer();
}




#ifdef K_TODO_MAYBE
bool LayersPanel::tool(LayerType layer_type, VikToolInterfaceFunc tool_func, GdkEventButton * ev, Viewport * viewport)
{
	Layer * layer = this->get_selected_layer();
	if (layer && layer->type == layer_type) {
		tool_func(layer, ev, viewport);
		return true;
	} else if (this->toplayer->visible &&
		   this->toplayer->layer_tool(layer_type, tool_func, ev, viewport) != 1) { /* either accepted or rejected, but a layer was found */
		return true;
	}
	return false;
}
#endif




Layer * LayersPanel::get_layer_of_type(LayerType layer_type)
{
	Layer * layer = this->get_selected_layer();
	if (layer && layer->type == layer_type) {
		return layer;
	}

	if (this->toplayer->visible) {
		return this->toplayer->get_top_visible_layer_of_type(layer_type);
	}

	return NULL;
}




std::list<const Layer *> LayersPanel::get_all_layers_of_type(LayerType layer_type, bool include_invisible)
{
	std::list<const Layer *> layers;
	this->toplayer->get_all_layers_of_type(layers, layer_type, include_invisible);
	return layers;
}




bool LayersPanel::has_any_layer_of_type(LayerType type)
{
	std::list<const Layer *> dems = this->get_all_layers_of_type(type, true); /* Includes hidden layers. */
	if (dems.empty()) {
		Dialog::error(tr("No DEM layers available, thus no DEM values can be applied."), this->window);
		return false;
	}
	return true;
}




LayerAggregate * LayersPanel::get_top_layer()
{
	if (NULL == this->toplayer) {
		qDebug() << "EE" PREFIX << "Top layer is NULL";
	}

	return this->toplayer;
}




/**
 * Remove all layers.
 */
void LayersPanel::clear()
{
	if (0 != this->toplayer->get_child_layers_count()) {
#ifdef K_FIXME_RESTORE
		g_signal_emit(G_OBJECT(this->panel_box), items_tree_signals[VLP_DELETE_LAYER_SIGNAL], 0);
#endif
		this->toplayer->clear(); /* simply deletes all layers */
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
		qDebug() << "II: Layers Panel: context menu event outside tree view";
		/* We want to handle only events that happen inside of tree view. */
		return;
	}

	qDebug() << "II: Layers Panel: context menu event inside tree view";

	QPoint orig = ev->pos();
	QPoint v = this->tree_view->pos();
	QPoint t = this->tree_view->viewport()->pos();

	qDebug() << "DD: Layers Panel: context menu event: event @" << orig.x() << orig.y();
	qDebug() << "DD: Layers Panel: context menu event: viewport @" << v;
	qDebug() << "DD: Layers Panel: context menu event: tree view @" << t;

	QPoint point;
	point.setX(orig.x() - v.x() - t.x());
	point.setY(orig.y() - v.y() - t.y());

	QModelIndex ind = this->tree_view->indexAt(point);

	if (ind.isValid()) {
		/* We have clicked on some valid item in tree view. */

		qDebug() << "II: Layers Panel: context menu event: valid tree view index, row =" << ind.row();
		TreeIndex index = QPersistentModelIndex(ind);
		TreeItem * item = this->tree_view->get_tree_item(index);

		Layer * layer = item->to_layer();

		this->context_menu_show_for_item(item);
	} else {
		/* We have clicked on empty space, not on tree item.  */

		qDebug() << "II: Layers Panel: context menu event: tree view not hit";
		if (!this->tree_view->viewport()->geometry().contains(ev->pos())) {
			qDebug() << "II: Layers Panel: context menu event outside of tree view's viewport";
			return;
		} else {
			qDebug() << "II: Layers Panel: context menu event inside of tree view's viewport";
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
Layer * LayersPanel::go_up_to_layer(const TreeItem * tree_item, LayerType expected_layer_type)
{
	TreeIndex const & item_index = tree_item->index;
        TreeIndex this_index = item_index;
	TreeIndex parent_index;

	while (1) {
		if (!this_index.isValid()) {
			return NULL; /* Returning invalid layer. */
		}

		TreeItem * this_item = this->tree_view->get_tree_item(this_index);
		if (this_item->tree_item_type == TreeItemType::LAYER) {

			if (((Layer *) this_item)->type == expected_layer_type) {
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
