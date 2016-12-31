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

#include "settings.h"
#include "layers_panel.h"
#include "layer_aggregate.h"
#include "layer_coord.h"
#include "dialog.h"
#include "globals.h"
#include "window.h"
#include "util.h"
#ifdef K
#include "clipboard.h"
#endif




using namespace SlavGPS;




typedef struct {
	LayersPanel * panel;
	LayerType layer_type;
} new_layer_data_t;




static void layers_item_edited_cb(LayersPanel * panel, TreeIndex const & index, char const * new_text);
static bool layers_key_press_cb(LayersPanel * panel, GdkEventKey * event);
static void layers_move_item_up_cb(LayersPanel * panel);
static void layers_move_item_down_cb(LayersPanel * panel);




LayersPanel::LayersPanel(Window * parent) : QWidget((QWidget *) parent)
{
	this->window = parent;
	this->panel_box = new QVBoxLayout;

	{
		this->tree_view = new TreeView(this);
		this->panel_box->addWidget(this->tree_view);
		this->tree_view->show();
	}

	{
		for (int i = 0; i < QIcon::themeSearchPaths().size(); i++) {
			qDebug() << "II: Layers Panel: XDG DATA FOLDER: " << QIcon::themeSearchPaths().at(i);
		}


		qDebug() << "II: Layers Panel: Using icon theme " << QIcon::themeName();

		this->tool_bar = new QToolBar();
		this->panel_box->addWidget(this->tool_bar);


		this->qa_layer_add = new QAction("Add", this);
		this->qa_layer_add->setToolTip("Add new layer");
		this->qa_layer_add->setIcon(QIcon::fromTheme("list-add"));
		// g_signal_connect_swapped (G_OBJECT(addbutton), "clicked", G_CALLBACK(layers_popup_cb), this);

		this->qa_layer_remove = new QAction("Remove", this);
		this->qa_layer_remove->setToolTip("Remove selected layer");
		this->qa_layer_remove->setIcon(QIcon::fromTheme("list-remove"));
		connect(this->qa_layer_remove, SIGNAL (triggered(bool)), this, SLOT (delete_selected_cb()));

		this->qa_layer_move_up = new QAction("Up", this);
		this->qa_layer_move_up->setToolTip("Move selected layer up");
		this->qa_layer_move_up->setIcon(QIcon::fromTheme("go-up"));
		// connect(this->qa_layer_move_up, SIGNAL (triggered(bool)), this, SLOT (layers_move_item_up_cb()));

		this->qa_layer_move_down = new QAction("Down", this);
		this->qa_layer_move_down->setToolTip("Move selected layer down");
		this->qa_layer_move_down->setIcon(QIcon::fromTheme("go-down"));
		// connect(this->qa_layer_move_down, SIGNAL (triggered(bool)), this, SLOT (layers_move_item_down_cb()));

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
	this->toplayer->rename(_("Top Layer"));
	TreeIndex invalid_parent_index; /* Top layer doesn't have any parent index. */
	this->toplayer_item = this->tree_view->add_layer(this->toplayer, NULL, invalid_parent_index, false, 0);
	this->toplayer->connect_to_tree(this->tree_view, this->toplayer_item);


	connect(this->tree_view, SIGNAL(layer_needs_redraw(sg_uid_t)), this->window, SLOT(draw_layer_cb(sg_uid_t)));
	connect(this->toplayer, SIGNAL(changed(void)), this, SLOT(emit_update_cb(void)));
	//connect(this->tree_view, "item_toggled", this, SLOT(item_toggled));


#ifndef SLAVGPS_QT
	g_signal_connect_swapped (this->tree_view->get_toolkit_widget(), "button_press_event", G_CALLBACK(button_press_cb), this);
	g_signal_connect_swapped (this->tree_view->get_toolkit_widget(), "item_edited", G_CALLBACK(layers_item_edited_cb), this);
	g_signal_connect_swapped (this->tree_view->get_toolkit_widget(), "key_press_event", G_CALLBACK(layers_key_press_cb), this);
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




void LayersPanel::set_viewport(Viewport * viewport)
{
	this->viewport = viewport;
	/* TODO: also update GCs (?) */
}




Viewport * LayersPanel::get_viewport()
{
	return this->viewport;
}




static bool layers_panel_new_layer(void * data)
{
	new_layer_data_t * layer_data = (new_layer_data_t *) data;

	return layer_data->panel->new_layer(layer_data->layer_type);
}




QMenu * LayersPanel::create_context_menu(bool full)
{
	QMenu * menu = new QMenu(this);
	if (full) {
		menu->addAction(this->qa_layer_cut);
		menu->addAction(this->qa_layer_copy);
		menu->addAction(this->qa_layer_paste);
		menu->addAction(this->qa_layer_remove);
	}

	QMenu * layers_submenu = new QMenu("New Layer");
	menu->addMenu(layers_submenu);
	this->window->new_layers_submenu_add_actions(layers_submenu);

	return menu;

}




void LayersPanel::emit_update_cb()
{
	qDebug() << "SLOT?: Layers Panel received 'changed' signal from top level layer?";
	qDebug() << "SIGNAL: Layers Panel emits 'update' signal";
	emit this->update();
}




/* Why do we have this function? Isn't TreeView::data_changed_cb() enough? */
void LayersPanel::item_toggled(TreeIndex const & index)
{
	/* Get type and data. */
	TreeItemType type = this->tree_view->get_item_type(index);

	bool visible;
	switch (type) {
	case TreeItemType::LAYER: {
		Layer * layer = this->tree_view->get_layer(index);
		visible = (layer->visible ^= 1);
		layer->emit_changed_although_invisible(); /* Set trigger for half-drawn. */
		break;
		}
	case TreeItemType::SUBLAYER: {
		Layer * parent = this->tree_view->get_parent_layer(index);
		visible = parent->sublayer_toggle_visible(this->tree_view->get_sublayer(index));
		parent->emit_changed_although_invisible();
		break;
	}
	default:
		return;
	}

	this->tree_view->set_visibility(index, visible);
}




static void layers_item_edited_cb(LayersPanel * panel, TreeIndex const & index, char const * new_text)
{
	panel->item_edited(index, new_text);
}




/* Why do we have this function? Isn't TreeView::data_changed_cb() enough? */
void LayersPanel::item_edited(TreeIndex const & index, char const * new_text)
{
	if (!new_text) {
		return;
	}

	if (new_text[0] == '\0') {
		dialog_error(_("New name can not be blank."), this->window);
		return;
	}
#ifdef K

	if (this->tree_view->get_item_type(index) == TreeItemType::LAYER) {

		/* Get index and layer. */
		Layer * layer = this->tree_view->get_layer(index);

		if (strcmp(layer->name, new_text) != 0) {
			layer->rename(new_text);
			this->tree_view->set_name(index, layer->name);
		}
	} else {
		Layer * parent = this->tree_view->get_parent_layer(index);
		const char *name = parent->sublayer_rename_request(this->tree_view->get_sublayer(index), new_text, this);
		if (name) {
			this->tree_view->set_name(index, name);
		}
	}
#endif
}




bool LayersPanel::button_press_cb(QMouseEvent * event)
{
	/* I don't understand what's going on with mouse buttons in this function. */

	if (event->button() == Qt::RightButton) {
		TreeIndex * index = this->tree_view->get_index_at_pos(event->x(), event->y());
#ifdef K
		if (index && index->isValid()) {
			this->popup(index, (MouseButton) event->button);
			this->tree_view->select(&iter);
		} else {
			this->popup(NULL, (MouseButton) event->button);
		}
		return true;
#endif
	}

	return false;
}




static bool layers_key_press_cb(LayersPanel * panel, GdkEventKey * event)
{
	return panel->key_press(event);
}




bool LayersPanel::key_press(GdkEventKey * event)
{
#ifndef SLAVGPS_QT
	/* Accept all forms of delete keys. */
	if (event->keyval == GDK_Delete || event->keyval == GDK_KP_Delete || event->keyval == GDK_BackSpace) {
		this->delete_selected();
		return true;
	}
#endif
	return false;
}




void LayersPanel::show_context_menu(TreeIndex const & index, Layer * layer)
{
	QMenu * menu = NULL;

	if (index.isValid()) {
		if (this->tree_view->get_item_type(index) == TreeItemType::LAYER) {
			if (layer->type == LayerType::AGGREGATE) {
				menu = this->create_context_menu(true);
			} else {
				/* kamilFIXME: this doesn't work for Map in treeview. Why?*/
				qDebug() << "II: Layers Panel: will call get_menu_items_selection";
				LayerMenuItem menu_selection = (LayerMenuItem) vik_layer_get_menu_items_selection(layer);

				menu = new QMenu(this);

				if (menu_selection & VIK_MENU_ITEM_PROPERTY) {
#if 0
					menu->addAction(qa_layer_properties);
#endif
				}

				if (menu_selection & VIK_MENU_ITEM_CUT) {
					menu->addAction(this->qa_layer_cut);
				}

				if (menu_selection & VIK_MENU_ITEM_COPY) {
					menu->addAction(this->qa_layer_copy);
				}

				if (menu_selection & VIK_MENU_ITEM_PASTE) {
					menu->addAction(this->qa_layer_paste);
				}

				if (menu_selection & VIK_MENU_ITEM_DELETE) {
					menu->addAction(this->qa_layer_remove);
				}
			}
			layer->add_menu_items(*menu);
		} else {
			menu = new QMenu(this);
			if (!layer->sublayer_add_menu_items(*menu)) { /* Here 'layer' is a parent layer. */
				delete menu;
				return;
			}
			/* TODO: specific things for different types. */
		}
	} else {
		menu = this->create_context_menu(false);
	}
	menu->exec(QCursor::pos());
	delete menu;
}




#define VIK_SETTINGS_LAYERS_TRW_CREATE_DEFAULT "layers_create_trw_auto_default"
/**
 * @type: type of the new layer.
 *
 * Create a new layer and add to panel.
 */
bool LayersPanel::new_layer(LayerType layer_type)
{
	assert (this->viewport);
	bool ask_user = false;
	if (layer_type == LayerType::TRW) {
		(void)a_settings_get_boolean(VIK_SETTINGS_LAYERS_TRW_CREATE_DEFAULT, &ask_user);
	}
	ask_user = !ask_user;

	assert (layer_type != LayerType::NUM_TYPES);

	Layer * layer = Layer::new_(layer_type, this->viewport);
	if (!layer) {
		return false;
	}

	LayerInterface * interface = Layer::get_interface(layer_type);

	if (ask_user && interface->params_count != 0) {
		if (!layer->properties_dialog(viewport)) {
			delete layer;
			return false;
		}

		/* We translate the name here in order to avoid translating name set by user. */
		layer->rename(_(interface->layer_name.toUtf8().constData()));
	}

	this->add_layer(layer);

	this->viewport->configure();
	qDebug() << "II: Layers Panel: calling layer->draw() for new layer" << interface->layer_type_string;
	layer->draw(this->viewport);

	return true;
}




/**
 * @layer: existing layer
 *
 * Add an existing layer to panel.
 */
void LayersPanel::add_layer(Layer * layer)
{
	/* Could be something different so we have to do this. */
	layer->change_coord_mode(this->viewport->get_coord_mode());
	qDebug() << "II: Layers Panel: add layer: attempting to add layer" << layer->debug_string;

	TreeIndex const & selected_index = this->tree_view->get_selected_item();
	if (true) { /* kamilFIXME: "if (!selected_index) { */
		/* No particular layer is selected in panel, so the
		   layer to be added goes directly under top level
		   aggregate layer. */
		qDebug() << "II: Layers Panel: add layer: no selected layer, adding layer" << layer->debug_string << "under top level aggregate layer";
		this->toplayer->add_layer(layer, true);

	} else {
		/* Some item in tree view is already selected. Let's find a good
		   place for given layer to be added - a first aggregate
		   layer that we meet going up in hierarchy. */

		TreeIndex replace_index;
		Layer * current = NULL;

		if (this->tree_view->get_item_type(selected_index) == TreeItemType::SUBLAYER) {
			current = this->tree_view->get_parent_layer(selected_index);
			qDebug() << "II: Layers Panel: add layer: capturing parent layer" << current->debug_string << "as current layer";
		} else {
			current = this->tree_view->get_layer(selected_index);
			qDebug() << "II: Layers Panel: add layer: capturing selected layer" << current->debug_string << "as current layer";
		}
		assert(current->connected_to_tree);
		replace_index = current->index;

		/* Go further up until you find first aggregate layer. */
		while (current->type != LayerType::AGGREGATE) {
			Layer * tmp_layer = this->tree_view->get_parent_layer(current->index);
			current = tmp_layer;
			assert(current->connected_to_tree);
		}


		LayerAggregate * aggregate = (LayerAggregate *) current;
#ifndef SLAVGPS_QT
		if (replace_index.isValid()) {
			aggregate->insert_layer(layer, replace_index);
		} else {
#endif
			aggregate->add_layer(layer, true);
#ifndef SLAVGPS_QT
		}
#endif
	}

	this->emit_update_cb();
}




void LayersPanel::move_item(bool up)
{
	TreeIndex const & selected_index = this->tree_view->get_selected_item();
	/* TODO: deactivate the buttons and stuff. */
	if (!selected_index.isValid()) {
		return;
	}

	this->tree_view->select(selected_index); /* Cancel any layer-name editing going on... */
	if (this->tree_view->get_item_type(selected_index) == TreeItemType::LAYER) {
		LayerAggregate * parent = (LayerAggregate *) this->tree_view->get_parent_layer(selected_index);

		if (parent) { /* Not toplevel. */
#ifndef SLAVGPS_QT
			parent->move_layer(selected_index, up);
			this->emit_update_cb();
#endif
		}
	}
}




bool LayersPanel::properties_cb(void) /* Slot. */
{
	assert (this->viewport);

	TreeIndex const & index = this->tree_view->get_selected_item();
	if (this->tree_view->get_item_type(index) == TreeItemType::LAYER) {
		LayerType layer_type = this->tree_view->get_layer(index)->type;
		if (Layer::get_interface(layer_type)->params_count == 0) {
			dialog_info(tr("This layer type has no configurable properties."), this->window);
		} else {
			Layer * layer = this->tree_view->get_layer(index);
			if (layer->properties_dialog(this->viewport)) {
				layer->emit_changed();
			}
		}
		return true;
	} else {
		return false;
	}
}




void LayersPanel::draw_all()
{
	if (this->viewport && this->toplayer->visible) {
		qDebug() << "II: Layers Panel: calling toplayer->draw()";
		this->toplayer->draw(this->viewport);
	}
}




void LayersPanel::cut_selected_cb(void) /* Slot. */
{
        TreeIndex const & index = this->tree_view->get_selected_item();
	if (!index.isValid()) {
		/* Nothing to do. */
		return;
	}

	TreeItemType type = this->tree_view->get_item_type(index);

	if (type == TreeItemType::LAYER) {
		LayerAggregate * parent = (LayerAggregate *) this->tree_view->get_parent_layer(index);

		if (parent) {
#ifndef SLAVGPS_QT
			/* Reset trigger if trigger deleted. */
			if (this->get_selected_layer()->the_same_object(this->viewport->get_trigger())) {
				this->viewport->set_trigger(NULL);
			}

			a_clipboard_copy_selected(this);

			if (parent->type == LayerType::AGGREGATE) {
				g_signal_emit(G_OBJECT(this->panel_box), layers_panel_signals[VLP_DELETE_LAYER_SIGNAL], 0);

				if (parent->delete_layer(index)) {
					this->emit_update_cb();
				}
			}
#endif
		} else {
			dialog_info("You cannot cut the Top Layer.", this->window);
		}
	} else if (type == TreeItemType::SUBLAYER) {
		Layer * selected = this->get_selected_layer();
		selected->cut_sublayer(this->tree_view->get_sublayer(index));
	}
}




void LayersPanel::copy_selected_cb(void) /* Slot. */
{
	TreeIndex const & selected_index = this->tree_view->get_selected_item();
	if (!selected_index.isValid()) {
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
	TreeIndex const & selected_index = this->tree_view->get_selected_item();
	if (!selected_index.isValid()) {
		/* Nothing to do. */
		return false;
	}
#ifndef SLAVGPS_QT
	return a_clipboard_paste(this);
#endif
}




void LayersPanel::delete_selected_cb(void) /* Slot. */
{
	TreeIndex const & index = this->tree_view->get_selected_item();
	if (!index.isValid()) {
		/* Nothing to do. */
		return;
	}

	TreeItemType type = this->tree_view->get_item_type(index);
	if (type == TreeItemType::LAYER) {
		Layer * layer = this->tree_view->get_layer(index);


		/* Get confirmation from the user. */
		if (!dialog_yes_or_no(QString("Are you sure you want to delete %1?").arg(QString(layer->get_name())), this->window)) {
			return;
		}

		LayerAggregate * parent = (LayerAggregate *) this->tree_view->get_parent_layer(index);
		if (parent) {
#ifndef SLAVGPS_QT
			/* Reset trigger if trigger deleted. */
			if (this->get_selected_layer()->the_same_object(this->viewport->get_trigger())) {
				this->viewport->set_trigger(NULL);
			}

			if (parent->type == LayerType::AGGREGATE) {

				g_signal_emit(G_OBJECT(this->panel_box), layers_panel_signals[VLP_DELETE_LAYER_SIGNAL], 0);

				if (parent->delete_layer(index)) {
					this->emit_update_cb();
				}
			}
#endif
		} else {
			dialog_info("You cannot delete the Top Layer.", this->window);
		}
	} else if (type == TreeItemType::SUBLAYER) {
		Layer * selected = this->get_selected_layer();
		selected->delete_sublayer(this->tree_view->get_sublayer(index));
	}
}




Layer * LayersPanel::get_selected_layer()
{
	TreeIndex const & index = this->tree_view->get_selected_item();
	if (!index.isValid()) {
		return NULL;
	}

#if 1
	TreeIndex const layer_index = this->tree_view->go_up_to_layer(index);
	if (layer_index.isValid()) {
		return this->tree_view->get_layer(layer_index);
	} else {
		return NULL;
	}
#else
	TreeIndex layer_index = index;
	TreeItemType type = this->tree_view->get_item_type(layer_index);
	while (type != TreeItemType::LAYER) {
		TreeIndex parent = layer_index.parent();
		if (!parent.isValid()) {
			return NULL;
		}
		type = this->tree_view->get_item_type(parent);
		layer_index = parent;
	}
	return this->tree_view->get_layer(layer_index);
#endif
}




static void layers_move_item_up_cb(LayersPanel * panel)
{
	panel->move_item(true);
}




static void layers_move_item_down_cb(LayersPanel * panel)
{
	panel->move_item(false);
}



#if 0
bool vik_layers_panel_tool(LayersPanel * panel, LayerType layer_type, VikToolInterfaceFunc tool_func, GdkEventButton * event, Viewport * viewport)
{
	Layer * layer = panel->get_selected_layer();
	if (layer && layer->type == layer_type) {
		tool_func(layer, event, viewport);
		return true;
	} else if (panel->toplayer->visible &&
		   panel->toplayer->layer_tool(layer_type, tool_func, event, viewport) != 1) { /* either accepted or rejected, but a layer was found */
		return true;
	}
	return false;
}
#endif




Layer * LayersPanel::get_layer_of_type(LayerType layer_type)
{
	Layer * layer = this->get_selected_layer();
	if (layer == NULL || layer->type != layer_type) {
		if (this->toplayer->visible) {
			return this->toplayer->get_top_visible_layer_of_type(layer_type);
		} else {
			return NULL;
		}
	} else {
		return (Layer *) layer;
	}
}




std::list<Layer *> * LayersPanel::get_all_layers_of_type(LayerType layer_type, bool include_invisible)
{
	std::list<Layer *> * layers = new std::list<Layer *>;
	return this->toplayer->get_all_layers_of_type(layers, layer_type, include_invisible);
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
		g_signal_emit(G_OBJECT(this->panel_box), layers_panel_signals[VLP_DELETE_LAYER_SIGNAL], 0);
#endif
		this->toplayer->clear(); /* simply deletes all layers */
	}
}




void LayersPanel::change_coord_mode(VikCoordMode mode)
{
	this->toplayer->change_coord_mode(mode);
}




void LayersPanel::set_visible(bool visible)
{
#ifndef SLAVGPS_QT
	if (visible) {
		gtk_widget_show(GTK_WIDGET (this->panel_box));
	} else {
		gtk_widget_hide(GTK_WIDGET (this->panel_box));
	}
#endif
	return;
}




bool LayersPanel::get_visible(void)
{
	return true; /* kamilTODO: improve. */
}




TreeView * LayersPanel::get_treeview()
{
	return this->tree_view;
}




Window * LayersPanel::get_window()
{
	return this->window;
}




void LayersPanel::contextMenuEvent(QContextMenuEvent * event)
{
	if (!this->tree_view->geometry().contains(event->pos())) {
		qDebug() << "II: Layers Panel: context menu event outside tree view";
		/* We want to handle only events that happen inside of tree view. */
		return;
	}

	qDebug() << "II: Layers Panel: context menu event inside tree view";

	QPoint orig = event->pos();
	QPoint v = this->tree_view->pos();
	QPoint t = this->tree_view->viewport()->pos();

	qDebug() << "DD: Layers Panel: context menu event: event @" << orig.x() << orig.y();
	qDebug() << "DD: Layers Panel: context menu event: viewport @" << v;
	qDebug() << "DD: Layers Panel: context menu event: treeview @" << t;

	orig.setX(orig.x() - v.x() - t.x());
	orig.setY(orig.y() - v.y() - t.y());

	QPoint point = orig;
	QModelIndex ind = this->tree_view->indexAt(point);
	TreeIndex index;
	Layer * layer = NULL;

	if (ind.isValid()) {
		qDebug() << "II: Layers Panel: context menu event: valid tree view index, row =" << ind.row();
		index = QPersistentModelIndex(ind);

		if (TreeItemType::LAYER == this->tree_view->get_item_type(index)) {
			qDebug() << "II: Layers Panel: creating context menu for TreeItemType::LAYER";

			layer = this->tree_view->get_layer(index);
			qDebug() << "II: Layers Panel: context menu event: layer type is" << (layer ? layer->debug_string : "NULL");

			QModelIndex parent = index.parent();
			if (parent.isValid()) {
				QModelIndex index2 = parent.child(ind.row(), 0);
				qDebug() << "II: Layers Panel: context menu event: item" << index << "index2" << index2;
				if (index.isValid()) {
					qDebug() << "II: Layers Panel: context menu event: index.row =" << index.row() << "index.column =" << index.column() << "text =" << this->tree_view->model->itemFromIndex(index2)->text();
				}
				index = index2;
			}
			memset(layer->menu_data, 0, sizeof (trw_menu_sublayer_t));
			layer->menu_data->layers_panel = this;
		} else {
			qDebug() << "II: Layers Panel: creating context menu for TreeItemType::SUBLAYER";

			layer = this->tree_view->get_parent_layer(index);
			qDebug() << "II: Layers Panel: context menu event: layer type is" << (layer ? layer->debug_string : "NULL");

			memset(layer->menu_data, 0, sizeof (trw_menu_sublayer_t));
			layer->menu_data->sublayer = this->tree_view->get_sublayer(index);
			layer->menu_data->viewport = this->get_viewport();
			layer->menu_data->layers_panel = this;
			layer->menu_data->confirm = true; /* Confirm delete request. */
		}

		this->show_context_menu(index, layer);
		memset(layer->menu_data, 0, sizeof (trw_menu_sublayer_t));

	} else {
		qDebug() << "II: Layers Panel: context menu event: tree view not hit";
		if (!this->tree_view->viewport()->geometry().contains(event->pos())) {
			qDebug() << "II: Layers Panel: context menu event outside of tree view's viewport";
			return;
		}
		qDebug() << "II: Layers Panel: context menu event inside of tree view's viewport";

		/* Invalid index and NULL layer. */
		this->show_context_menu(index, layer);
	}
	return;
}
