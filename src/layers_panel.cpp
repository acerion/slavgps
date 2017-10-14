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

#include "viewport_internal.h"
#include "settings.h"
#include "layers_panel.h"
#include "layer_aggregate.h"
#include "layer_coord.h"
#include "layer_interface.h"
#include "tree_view_internal.h"
#include "dialog.h"
#include "globals.h"
#include "util.h"
#ifdef K
#include "clipboard.h"
#endif




using namespace SlavGPS;




static bool layers_key_press_cb(LayersPanel * panel, QKeyEvent * ev);




LayersPanel::LayersPanel(QWidget * parent_, Window * window_) : QWidget(parent_)
{
	this->panel_box = new QVBoxLayout;
	this->window = window_;

	this->setMaximumWidth(300);
	this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);


	{
		this->tree_view = new TreeView(this);
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


	this->toplayer = new LayerAggregate();
	this->toplayer->set_name(tr("Top Layer"));
	TreeIndex invalid_parent_index; /* Top layer doesn't have any parent index. */
	/* This call sets TreeItem::index and TreeItem::tree_view of added item. */
	this->toplayer_item = this->tree_view->add_tree_item(invalid_parent_index, this->toplayer, this->toplayer->name);


	connect(this->tree_view, SIGNAL(tree_item_needs_redraw(sg_uid_t)), this->window, SLOT(draw_layer_cb(sg_uid_t)));
	connect(this->toplayer, SIGNAL(layer_changed(void)), this, SLOT(emit_update_window_cb(void)));


#ifndef SLAVGPS_QT
	QObject::connect(this->tree_view, this, SIGNAL("button_press_event"), SLOT (button_press_cb));
	QObject::connect(this->tree_view, this, SIGNAL("key_press_event"), SLOT (layers_key_press_cb));
#endif
}




LayersPanel::~LayersPanel()
{
	qDebug() << "II: Layers Panel: ~LayersPanel() called";

#ifdef K
	/* TODO: what to do with the toplayer? */
	this->toplayer->unref();
#endif

	delete this->tree_view;
	delete this->panel_box;
	delete this->tool_bar;
}




void LayersPanel::emit_update_window_cb()
{
	qDebug() << "SLOT?: Layers Panel received 'changed' signal from top level layer?";
	qDebug() << "SIGNAL: Layers Panel emits 'update' signal";
	emit this->update_window();
}




bool LayersPanel::button_press_cb(QMouseEvent * ev)
{
	/* I don't understand what's going on with mouse buttons in this function. */

	if (ev->button() == Qt::RightButton) {
		TreeIndex * index = this->tree_view->get_index_at_pos(ev->x(), ev->y());
#ifdef K
		if (index && index->isValid()) {
			this->popup(index, (MouseButton) ev->button);
			this->tree_view->select(&iter);
		} else {
			this->popup(NULL, (MouseButton) ev->button);
		}
		return true;
#endif
	}

	return false;
}




static bool layers_key_press_cb(LayersPanel * panel, QKeyEvent * ev)
{
	return panel->key_press(ev);
}




bool LayersPanel::key_press(QKeyEvent * ev)
{
	/* Accept all forms of delete keys. */
	if (ev->key() == Qt::Key_Delete || ev->key() == Qt::Key_Backspace) {
		this->delete_selected_cb();
		return true;
	}
	return false;
}



/**
   Create basic part of context menu for a click occurring anywhere in
   tree view (both on an item and outside of item).

   Number and type of menu items is controlled by \param layer_menu_items.
   Returned pointer is owned by caller.

   \param layer_menu_items - bit sum of LayerMenuItem values

   \return freshly created menu with specified items
*/
void LayersPanel::context_menu_add_standard_items(QMenu * menu, uint16_t layer_menu_items)
{
	if (layer_menu_items & (uint16_t) LayerMenuItem::PROPERTIES) {
		menu->addAction(this->window->qa_tree_item_properties);
	}

	if (layer_menu_items & (uint16_t) LayerMenuItem::CUT) {
		menu->addAction(this->qa_layer_cut);
	}

	if (layer_menu_items & (uint16_t) LayerMenuItem::COPY) {
		menu->addAction(this->qa_layer_copy);
	}

	if (layer_menu_items & (uint16_t) LayerMenuItem::PASTE) {
		menu->addAction(this->qa_layer_paste);
	}

	if (layer_menu_items & (uint16_t) LayerMenuItem::DELETE) {
		menu->addAction(this->qa_layer_remove);
	}

	if (layer_menu_items & (uint16_t) LayerMenuItem::NEW) {
		this->context_menu_add_new_layer_submenu(menu);
	}
}




void LayersPanel::context_menu_add_new_layer_submenu(QMenu * menu)
{
	QMenu * layers_submenu = new QMenu("New Layer");
	menu->addMenu(layers_submenu);
	this->window->new_layers_submenu_add_actions(layers_submenu);
}




void LayersPanel::context_menu_show_for_item(TreeItem * item)
{
	if (!item) {
		qDebug() << "EE: Layers Panel: show context menu for item: NULL item";
		return;
	}

	QMenu * menu = new QMenu(this);

	if (item->tree_item_type == TreeItemType::LAYER) {

		qDebug() << "II: Layers Panel: context menu event: menu for layer" << item->type_id << item->name;

		Layer * layer = item->to_layer();

		/* kamilFIXME: this doesn't work for Map in tree view. Why? */
		uint16_t layer_menu_items = (uint16_t) layer->get_menu_items_selection();

#ifdef K
		/* "New layer -> layer types" submenu. */
		layer_menu_items |= (uint16_t) LayerMenuItem::NEW;
#endif


		this->context_menu_add_standard_items(menu, layer_menu_items);

		/* Layer-type-specific menu items. */
		layer->add_menu_items(*menu);
	} else {
		qDebug() << "II: Layers Panel: context menu event: menu for sublayer" << item->type_id << item->name;


		if (!item->add_context_menu_items(*menu, true)) {
			delete menu;
			return;
		}
		/* TODO: specific things for different types. */
	}

	if (menu) {
		menu->exec(QCursor::pos());
		delete menu;
	} else {
		qDebug() << "EE: Layers Panel: show context menu: null menu";
	}

	return;

}




void LayersPanel::context_menu_show_for_new_layer(void)
{
	QMenu * menu = new QMenu(this);
	this->context_menu_add_standard_items(menu, (uint16_t) LayerMenuItem::NEW);
	menu->exec(QCursor::pos());
	delete menu;
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

	qDebug() << "II: Layers Panel: add layer: attempting to add layer" << layer->debug_string;

	/* TODO: move this in some reasonable place. Putting it here is just a workaround. */
	layer->tree_view = this->tree_view;

	TreeItem * selected_item = this->tree_view->get_selected_tree_item();
	if (!selected_item) {
		/* No particular layer is selected in panel, so the
		   layer to be added goes directly under top level
		   aggregate layer. */
		qDebug() << "II: Layers Panel: add layer: no selected layer, adding layer" << layer->debug_string << "under top level aggregate layer";
		this->toplayer->add_layer(layer, true);

	} else {
		/* Some item in tree view is already selected. Let's find a good
		   place for given layer to be added - a first aggregate
		   layer that we meet going up in hierarchy. */

		Layer * selected_layer = selected_item->to_layer(); /* If selected item is layer, then the layer itself is returned here. Otherwise, parent/owning layer of selected sublayer is returned. */
		if (selected_item->tree_item_type == TreeItemType::SUBLAYER) {
			qDebug() << "II: Layers Panel: add layer: capturing parent layer" << selected_layer->debug_string << "as selected layer";
		} else {
			qDebug() << "II: Layers Panel: add layer: capturing selected layer" << selected_layer->debug_string << "as selected layer";
		}
		assert (selected_layer->tree_view);
		TreeIndex sibling_layer_index = selected_layer->index;

		/* A new layer can be inserted only under an Aggregate layer.
		   Find first one in tree hierarchy (going up). */
		TreeIndex aggregate_index = this->go_up_to_layer(selected_layer->index, LayerType::AGGREGATE);
		if (aggregate_index.isValid()) {
			LayerAggregate * aggregate = (LayerAggregate *) this->tree_view->get_tree_item(aggregate_index)->to_layer();
			assert (aggregate->tree_view);

			if (false
#ifdef K
			    sibling_layer_index.isValid()
#endif
			    ) {
				aggregate->insert_layer(layer, sibling_layer_index);
			} else {
				aggregate->add_layer(layer, true);
			}
		} else {
			/* TODO: add error handling? */
			qDebug() << "EE: Layers Panel: add layer: failure, can't find aggregate layer";
		}
	}

	this->emit_update_window_cb();
}




void LayersPanel::move_item(bool up)
{
	TreeItem * selected_item = this->tree_view->get_selected_tree_item();
	if (!selected_item) {
		/* TODO: deactivate the buttons and stuff. */
		return;
	}

	this->tree_view->select(selected_item->index); /* Cancel any layer-name editing going on... */

	if (selected_item->tree_item_type == TreeItemType::LAYER) {
		/* A layer can be owned only by Aggregate layer.
		   TODO: what about TRW layers under GPS layer? */
		LayerAggregate * parent_layer = (LayerAggregate *) selected_item->owning_layer;
		if (parent_layer) { /* Not toplevel. */
			parent_layer->move_layer(selected_item->index, up);
			this->emit_update_window_cb();
		}
	}
}




void LayersPanel::draw_all(Viewport * viewport_)
{
	if (viewport_ && this->toplayer->visible) {
		qDebug() << "II: Layers Panel: calling toplayer->draw()";
		this->toplayer->draw(viewport_);
	}
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
		   TODO: what about TRW layers under GPS layer? */
		LayerAggregate * parent_layer = (LayerAggregate *) selected_item->owning_layer;
		if (parent_layer) {
#ifndef SLAVGPS_QT
			/* Reset trigger if trigger deleted. */
			if (this->get_selected_layer()->the_same_object(g_tree->tree_get_main_viewport()->get_trigger())) {
				g_tree->tree_get_main_viewport()->set_trigger(NULL);
			}

			a_clipboard_copy_selected(this);

			if (parent_layer->type == LayerType::AGGREGATE) {
				g_signal_emit(G_OBJECT(this->panel_box), items_tree_signals[VLP_DELETE_LAYER_SIGNAL], 0);

				if (parent_layer->delete_layer(selected_item->index)) {
					this->emit_update_window_cb();
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
#ifndef SLAVGPS_QT
	/* NB clipboard contains layer vs sublayer logic, so don't need to do it here. */
	a_clipboard_copy_selected(this);
#endif
}




bool LayersPanel::paste_selected_cb(void) /* Slot. */
{
	TreeItem * selected_item = this->tree_view->get_selected_tree_item();
	if (!selected_item) {
		/* Nothing to do. */
		return false;
	}
	bool result = false;
#ifndef SLAVGPS_QT
	result = a_clipboard_paste(this);
#endif
	return result;
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
		if (!Dialog::yes_or_no(tr("Are you sure you want to delete %1?").arg(QString(layer->get_name())), this->window)) {
			return;
		}

		/* A layer can be owned only by Aggregate layer.
		     TODO: what about TRW layers under GPS layer? */
		LayerAggregate * parent_layer = (LayerAggregate *) selected_item->owning_layer;
		if (parent_layer) {
#ifndef SLAVGPS_QT
			/* Reset trigger if trigger deleted. */
			if (this->get_selected_layer()->the_same_object(g_tree->tree_get_main_viewport()->get_trigger())) {
				g_tree->tree_get_main_viewport()->set_trigger(NULL);
			}

			if (parent_layer->type == LayerType::AGGREGATE) {

				g_signal_emit(G_OBJECT(this->panel_box), items_tree_signals[VLP_DELETE_LAYER_SIGNAL], 0);

				if (parent_layer->delete_layer(selected_item->index)) {
					this->emit_update_window_cb();
				}
			}
#endif
		} else {
			Dialog::info(tr("You cannot delete the Top Layer."), this->window);
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




#if 0
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




std::list<Layer const *> * LayersPanel::get_all_layers_of_type(LayerType layer_type, bool include_invisible)
{
	std::list<Layer const *> * layers = new std::list<Layer const *>;
	return this->toplayer->get_all_layers_of_type(layers, layer_type, include_invisible);
}




bool LayersPanel::has_any_layer_of_type(LayerType type)
{
	std::list<Layer const *> * dems = this->get_all_layers_of_type(type, true); /* Includes hidden layers. */
	if (dems->empty()) {
		delete dems;
		Dialog::error(tr("No DEM layers available, thus no DEM values can be applied."), this->window);
		return false;
	}
	delete dems;
	return true;
}




LayerAggregate * LayersPanel::get_top_layer()
{
	return this->toplayer;
}




/**
 * Remove all layers.
 */
void LayersPanel::clear()
{
	if (!this->toplayer->is_empty()) {
#ifndef SLAVGPS_QT
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
	this->window->show_side_panel(visible);
	return;
}




bool LayersPanel::get_visible(void)
{
	return true; /* kamilTODO: improve. */
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
		memset(layer->menu_data, 0, sizeof (trw_menu_sublayer_t));

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
  Go up the tree to find a Layer of given type.

  If @param index already refers to layer of given type, the function
  doesn't really go up, it returns the same index.

  If you want to skip item pointed to by @param index and start from
  its parent, you have to calculate parent by yourself before passing
  an index to this function.
*/
TreeIndex const LayersPanel::go_up_to_layer(TreeIndex const & item_index, LayerType expected_layer_type)
{
        TreeIndex this_index = item_index;
	TreeIndex parent_index;

	while (1) {
		if (!this_index.isValid()) {
			return this_index; /* Returning copy of invalid index. */
		}

		TreeItem * this_item = this->tree_view->get_tree_item(this_index);
		if (this_item->tree_item_type == TreeItemType::LAYER) {

			if (((Layer *) this_item)->type ==  expected_layer_type) {
				return this_index; /* Returning index of matching layer. */
			}
		}

		/* Go one step up to parent. */
		parent_index = this_index.parent();

		/* Parent also may be invalid. */
		if (!parent_index.isValid()) {
			return parent_index; /* Returning copy of invalid index. */
		}

		this_index = parent_index;
	}
}
