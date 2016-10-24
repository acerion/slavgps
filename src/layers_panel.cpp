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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstring>
#include <cstdlib>
#include <cassert>
#include <future> /* std::async */

#include <glib/gi18n.h>
#include <QPushButton>

#include "settings.h"
#include "layers_panel.h"
#include "layer_aggregate.h"
#include "layer_coord.h"
#include "dialog.h"
#ifndef SLAVGPS_QT
#include "clipboard.h"
#endif
#include "globals.h"
#include "window.h"




using namespace SlavGPS;




typedef struct {
	LayersPanel * panel;
	LayerType layer_type;
} new_layer_data_t;

enum {
	VLP_DELETE_LAYER_SIGNAL,
	VLP_LAST_SIGNAL
};

static unsigned int layers_panel_signals[VLP_LAST_SIGNAL] = { 0 };




static void vik_layers_panel_cut_selected_cb(LayersPanel * panel);
static void vik_layers_panel_copy_selected_cb(LayersPanel * panel);
static bool vik_layers_panel_paste_selected_cb(LayersPanel * panel);
static void vik_layers_panel_delete_selected_cb(LayersPanel * panel);
static bool vik_layers_panel_properties_cb(LayersPanel * panel);

#ifndef SLAVGPS_QT
static GtkActionEntry entries[] = {
	{ "Cut",    GTK_STOCK_CUT,    N_("C_ut"),       NULL, NULL, (GCallback) vik_layers_panel_cut_selected_cb    },
	{ "Copy",   GTK_STOCK_COPY,   N_("_Copy"),      NULL, NULL, (GCallback) vik_layers_panel_copy_selected_cb   },
	{ "Paste",  GTK_STOCK_PASTE,  N_("_Paste"),     NULL, NULL, (GCallback) vik_layers_panel_paste_selected_cb  },
	{ "Delete", GTK_STOCK_DELETE, N_("_Delete"),    NULL, NULL, (GCallback) vik_layers_panel_delete_selected_cb },
};
#endif




static void layers_item_edited_cb(LayersPanel * panel, GtkTreeIter * iter, char const * new_text);
static void menu_popup_cb(LayersPanel * panel);
static void layers_popup_cb(LayersPanel * panel);
static bool layers_button_press_cb(LayersPanel * panel, GdkEventButton * event);
static bool layers_key_press_cb(LayersPanel * panel, GdkEventKey * event);
static void layers_move_item_up_cb(LayersPanel * panel);
static void layers_move_item_down_cb(LayersPanel * panel);




void SlavGPS::layers_panel_init(void)
{
#ifndef SLAVGPS_QT
	layers_panel_signals[VLP_DELETE_LAYER_SIGNAL] = g_signal_new("delete_layer", G_TYPE_OBJECT, (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION), 0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
#endif
}




LayersPanel::LayersPanel(Window * parent) : QWidget((QWidget *) parent)
{
	this->window = parent;
	this->panel_box_ = new QVBoxLayout;

	{
		this->tree_view = new TreeView(this);
		this->panel_box_->addWidget(this->tree_view);
		this->tree_view->show();
	}

	{
		for (int i = 0; i < QIcon::themeSearchPaths().size(); i++) {
			qDebug() << "XDG DATA FOLDER: " << QIcon::themeSearchPaths().at(i);
		}


		qDebug() << "Using icon theme " << QIcon::themeName();

		this->tool_bar_ = new QToolBar();
		this->panel_box_->addWidget(this->tool_bar_);


		this->qa_layer_add = new QAction("Add", this);
		this->qa_layer_add->setToolTip("Add new layer");
		this->qa_layer_add->setIcon(QIcon::fromTheme("list-add"));
		// g_signal_connect_swapped (G_OBJECT(addbutton), "clicked", G_CALLBACK(layers_popup_cb), this);

		this->qa_layer_remove = new QAction("Remove", this);
		this->qa_layer_remove->setToolTip("Remove selected layer");
		this->qa_layer_remove->setIcon(QIcon::fromTheme("list-remove"));
		// g_signal_connect_swapped (G_OBJECT(removebutton), "clicked", G_CALLBACK(vik_layers_panel_delete_selected_cb), this);

		this->qa_layer_move_up = new QAction("Up", this);
		this->qa_layer_move_up->setToolTip("Move selected layer up");
		this->qa_layer_move_up->setIcon(QIcon::fromTheme("go-up"));
		// g_signal_connect_swapped (G_OBJECT(upbutton), "clicked", G_CALLBACK(layers_move_item_up_cb), this);

		this->qa_layer_move_down = new QAction("Down", this);
		this->qa_layer_move_down->setToolTip("Move selected layer down");
		this->qa_layer_move_down->setIcon(QIcon::fromTheme("go-down"));
		// g_signal_connect_swapped (G_OBJECT(downbutton), "clicked", G_CALLBACK(layers_move_item_down_cb), this);

		this->qa_layer_cut = new QAction("Cut", this);
		this->qa_layer_cut->setToolTip("Cut selected layer");
		this->qa_layer_cut->setIcon(QIcon::fromTheme("edit-cut"));
		// g_signal_connect_swapped (G_OBJECT(cutbutton), "clicked", G_CALLBACK(vik_layers_panel_cut_selected_cb), this);

		this->qa_layer_copy = new QAction("Copy", this);
		this->qa_layer_copy->setToolTip("Copy selected layer");
		this->qa_layer_copy->setIcon(QIcon::fromTheme("edit-copy"));
		// g_signal_connect_swapped (G_OBJECT(copybutton), "clicked", G_CALLBACK(vik_layers_panel_copy_selected_cb), this);

		this->qa_layer_paste = new QAction("Paste", this);
		this->qa_layer_paste->setToolTip("Paste layer into selected container layer or otherwise above selected layer");
		this->qa_layer_paste->setIcon(QIcon::fromTheme("edit-paste"));
		// g_signal_connect_swapped (G_OBJECT(pastebutton), "clicked", G_CALLBACK(vik_layers_panel_paste_selected_cb), this);

		this->tool_bar_->addAction(qa_layer_add);
		this->tool_bar_->addAction(qa_layer_remove);
		this->tool_bar_->addAction(qa_layer_move_up);
		this->tool_bar_->addAction(qa_layer_move_down);
		this->tool_bar_->addAction(qa_layer_cut);
		this->tool_bar_->addAction(qa_layer_copy);
		this->tool_bar_->addAction(qa_layer_paste);
	}



	this->setLayout(this->panel_box_);



	/* All this stuff has been moved here from
	   vik_layers_panel_init(), because leaving it in _init()
	   caused problems during execution time. */

	this->toplayer = new LayerAggregate(this->window->get_viewport());
	this->toplayer->rename(_("Top Layer"));
	///this->tree_view->add_layer(NULL, &(this->toplayer_iter), this->toplayer->name, NULL, true, this->toplayer, (int) LayerType::AGGREGATE, LayerType::AGGREGATE, 0);
	this->toplayer_item = this->tree_view->add_layer(this->toplayer, NULL, NULL, false, 0, 0);
	this->toplayer->realize(this->tree_view, this->toplayer_item);


	//Layer * layer = new LayerCoord();
	//layer->rename("a coord layer");
	//QStandardItem * coord = this->tree_view->add_layer(layer, this->toplayer, this->toplayer_item, false, 0, 0);

	connect(this->tree_view, SIGNAL(layer_needs_redraw(sg_uid_t)), this->window, SLOT(draw_layer_cb(sg_uid_t)));
	connect(this->toplayer, SIGNAL(update(void)), this, SLOT(emit_update_cb(void)));
	//connect(this->tree_view, "item_toggled", this, SLOT(item_toggled));


#ifndef SLAVGPS_QT


	g_signal_connect_swapped (this->tree_view->get_toolkit_widget(), "popup_menu", G_CALLBACK(menu_popup_cb), this);
	g_signal_connect_swapped (this->tree_view->get_toolkit_widget(), "button_press_event", G_CALLBACK(layers_button_press_cb), this);
	g_signal_connect_swapped (this->tree_view->get_toolkit_widget(), "item_edited", G_CALLBACK(layers_item_edited_cb), this);
	g_signal_connect_swapped (this->tree_view->get_toolkit_widget(), "key_press_event", G_CALLBACK(layers_key_press_cb), this);
#endif
}




LayersPanel::~LayersPanel()
{
	/* kamilTODO: improve the destructor. */

	fprintf(stderr, "~LayersPanel() called\n");

	this->toplayer->unref();

#ifndef SLAVGPS_QT
	/* kamilFIXME: free this pointer. */
	this->panel_box_ = NULL;
#endif
}




void LayersPanel::set_viewport(Viewport * viewport)
{
	this->viewport = viewport;
	this->toplayer->viewport = viewport;
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




/**
 * Create menu popup on demand.
 * @full: offer cut/copy options as well - not just the new layer options
 */
static GtkWidget* layers_panel_create_popup(LayersPanel * panel, bool full)
{
#ifndef SLAVGPS_QT
	GtkWidget * menu = gtk_menu_new();
	GtkWidget * menuitem;

	if (full) {
		for (unsigned int ii = 0; ii < G_N_ELEMENTS(entries); ii++) {
			if (entries[ii].stock_id) {
				menuitem = gtk_image_menu_item_new_with_mnemonic(entries[ii].label);
				gtk_image_menu_item_set_image((GtkImageMenuItem*)menuitem, gtk_image_new_from_stock (entries[ii].stock_id, GTK_ICON_SIZE_MENU));
			} else {
				menuitem = gtk_menu_item_new_with_mnemonic(entries[ii].label);
			}

			g_signal_connect_swapped(G_OBJECT(menuitem), "activate", G_CALLBACK(entries[ii].callback), panel->panel_box_);
			gtk_menu_shell_append(GTK_MENU_SHELL (menu), menuitem);
			gtk_widget_show(menuitem);
		}
	}

	GtkWidget * submenu = gtk_menu_new();
	menuitem = gtk_menu_item_new_with_mnemonic(_("New Layer"));
	gtk_menu_shell_append (GTK_MENU_SHELL(menu), menuitem);
	gtk_widget_show(menuitem);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM (menuitem), submenu);

	/* Static: so memory accessible yet not continually allocated. */
	static new_layer_data_t lpnl[(int) LayerType::NUM_TYPES];

	for (LayerType ii = LayerType::AGGREGATE; ii < LayerType::NUM_TYPES; ++ii) {
		if (Layer::get_interface(ii)->icon) {
			menuitem = gtk_image_menu_item_new_with_mnemonic(Layer::get_interface(ii)->name);
			gtk_image_menu_item_set_image((GtkImageMenuItem*)menuitem, gtk_image_new_from_pixbuf(vik_layer_load_icon(ii)));
		} else {
			menuitem = gtk_menu_item_new_with_mnemonic (Layer::get_interface(ii)->name);
		}

		lpnl[(int) ii].panel = panel;
		lpnl[(int) ii].layer_type = ii;

		g_signal_connect_swapped (G_OBJECT(menuitem), "activate", G_CALLBACK(layers_panel_new_layer), &(lpnl[(int) ii]));
		gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
		gtk_widget_show(menuitem);
	}

	return menu;
#endif
}




/**
 * Invoke the actual drawing via signal method.
 */
void LayersPanel::idle_draw_panel(LayersPanel * panel)
{
	qDebug() << "SIGNAL: LayersPanel::update()";
	emit panel->update();
	return;
}




void LayersPanel::emit_update_cb()
{
	qDebug() << "II: Layers Panel: scheduling idle draw panel in response to update signal";
	std::async(std::launch::async, LayersPanel::idle_draw_panel, this);
}




/* Why do we have this function? Isn't TreeView::data_changed_cb() enough? */
void LayersPanel::item_toggled(TreeIndex * index)
{
	/* Get type and data. */
	TreeItemType type = this->tree_view->get_item_type(index);

	bool visible;
	switch (type) {
	case TreeItemType::LAYER: {
		Layer * layer = this->tree_view->get_layer(index);
		visible = (layer->visible ^= 1);
		layer->emit_update_although_invisible(); /* Set trigger for half-drawn. */
		break;
		}
	case TreeItemType::SUBLAYER: {
		sg_uid_t sublayer_uid = this->tree_view->get_sublayer_uid(index);
		Layer * parent = this->tree_view->get_parent_layer(index);
		visible = parent->sublayer_toggle_visible(this->tree_view->get_sublayer_type(index), sublayer_uid);
		parent->emit_update_although_invisible();
		break;
	}
	default:
		return;
	}

	this->tree_view->set_visibility(index, visible);
}




static void layers_item_edited_cb(LayersPanel * panel, GtkTreeIter * iter, char const * new_text)
{
	panel->item_edited(iter, new_text);
}




/* Why do we have this function? Isn't TreeView::data_changed_cb() enough? */
void LayersPanel::item_edited(GtkTreeIter * iter, char const * new_text)
{
	if (!new_text) {
		return;
	}

#ifndef SLAVGPS_QT
	if (new_text[0] == '\0') {
		a_dialog_error_msg(this->toplayer->get_window()->get_toolkit_window(), _("New name can not be blank."));
		return;
	}

	if (this->tree_view->get_item_type(iter) == TreeItemType::LAYER) {

		/* Get iter and layer. */
		Layer * layer = this->tree_view->get_layer(iter);

		if (strcmp(layer->name, new_text) != 0) {
			layer->rename(new_text);
			this->tree_view->set_name(iter, layer->name);
		}
	} else {
		Layer * parent = this->tree_view->get_parent_layer(iter);
		const char *name = parent->sublayer_rename_request(new_text, this, this->tree_view->get_sublayer_type(iter), this->tree_view->get_sublayer_uid(iter), iter);
		if (name) {
			this->tree_view->set_name(iter, name);
		}
	}
#endif
}




static bool layers_button_press_cb(LayersPanel * panel, GdkEventButton * event)
{
	return panel->button_press(event);
}




bool LayersPanel::button_press(GdkEventButton * event)
{
#ifndef SLAVGPS_QT
	/* I don't understand what's going on with mouse buttons in this function. */

	if (event->button == 3) {
		static GtkTreeIter iter;
		if (this->tree_view->get_iter_at_pos(&iter, event->x, event->y)) {
			this->popup(&iter, (MouseButton) event->button);
			this->tree_view->select(&iter);
		} else {
			this->popup(NULL, (MouseButton) event->button);
		}
		return true;
	}
#endif
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




void LayersPanel::popup(GtkTreeIter * iter, MouseButton mouse_button)
{
#ifndef SLAVGPS_QT
	GtkMenu * menu = NULL;

	if (iter) {
		if (this->tree_view->get_item_type(iter) == TreeItemType::LAYER) {
			Layer * layer = this->tree_view->get_layer(iter);

			if (layer->type == LayerType::AGGREGATE) {
				menu = GTK_MENU (layers_panel_create_popup(this, true));
			} else {
				GtkWidget *del, *prop;
				/* kamilFIXME: this doesn't work for Map in treeview. Why?*/
				fprintf(stderr, "will call get_menu_items_selection.\n");
				VikStdLayerMenuItem menu_selection = (VikStdLayerMenuItem) vik_layer_get_menu_items_selection(layer);

				menu = GTK_MENU (gtk_menu_new());

				if (menu_selection & VIK_MENU_ITEM_PROPERTY) {
					prop = gtk_image_menu_item_new_from_stock (GTK_STOCK_PROPERTIES, NULL);
					g_signal_connect_swapped (G_OBJECT(prop), "activate", G_CALLBACK(vik_layers_panel_properties_cb), this);
					gtk_menu_shell_append (GTK_MENU_SHELL (menu), prop);
					gtk_widget_show (prop);
				}

				if (menu_selection & VIK_MENU_ITEM_CUT) {
					del = gtk_image_menu_item_new_from_stock (GTK_STOCK_CUT, NULL);
					g_signal_connect_swapped (G_OBJECT(del), "activate", G_CALLBACK(vik_layers_panel_cut_selected_cb), this);
					gtk_menu_shell_append (GTK_MENU_SHELL (menu), del);
					gtk_widget_show (del);
				}

				if (menu_selection & VIK_MENU_ITEM_COPY) {
					del = gtk_image_menu_item_new_from_stock (GTK_STOCK_COPY, NULL);
					g_signal_connect_swapped (G_OBJECT(del), "activate", G_CALLBACK(vik_layers_panel_copy_selected_cb), this);
					gtk_menu_shell_append (GTK_MENU_SHELL (menu), del);
					gtk_widget_show (del);
				}

				if (menu_selection & VIK_MENU_ITEM_PASTE) {
					del = gtk_image_menu_item_new_from_stock (GTK_STOCK_PASTE, NULL);
					g_signal_connect_swapped (G_OBJECT(del), "activate", G_CALLBACK(vik_layers_panel_paste_selected_cb), this);
					gtk_menu_shell_append (GTK_MENU_SHELL (menu), del);
					gtk_widget_show (del);
				}

				if (menu_selection & VIK_MENU_ITEM_DELETE) {
					del = gtk_image_menu_item_new_from_stock (GTK_STOCK_DELETE, NULL);
					g_signal_connect_swapped (G_OBJECT(del), "activate", G_CALLBACK(vik_layers_panel_delete_selected_cb), this);
					gtk_menu_shell_append (GTK_MENU_SHELL (menu), del);
					gtk_widget_show (del);
				}
			}
			layer->add_menu_items(menu, this);
		} else {
			menu = GTK_MENU (gtk_menu_new());
			Layer * parent = this->tree_view->get_parent_layer(iter);
			if (!parent->sublayer_add_menu_items(menu, this, this->tree_view->get_sublayer_type(iter), this->tree_view->get_sublayer_uid(iter), iter, this->viewport)) { // kamil
				gtk_widget_destroy (GTK_WIDGET(menu));
				return;
			}
			/* TODO: specific things for different types. */
		}
	} else {
		menu = GTK_MENU (layers_panel_create_popup(this, false));
	}
	gtk_menu_popup(menu, NULL, NULL, NULL, NULL, (unsigned int) mouse_button, gtk_get_current_event_time());
#endif
}




static void menu_popup_cb(LayersPanel * panel)
{
#ifndef SLAVGPS_QT
	GtkTreeIter iter;
	panel->popup(panel->tree_view->get_selected_item(&iter) ? &iter : NULL, MouseButton::OTHER);
#endif
}




static void layers_popup_cb(LayersPanel * panel)
{
	panel->popup(NULL, MouseButton::OTHER);
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

	Layer * layer = Layer::new_(layer_type, this->viewport, ask_user);
	if (layer) {
		this->add_layer(layer);

		this->viewport->configure();
		qDebug() << "II: Layers Panel: calling layer->draw() for" << layer->get_interface(layer->type)->fixed_layer_name << __FUNCTION__ << __LINE__;
		layer->draw(this->viewport);

		return true;
	}

	return false;
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
	fprintf(stderr, "INFO: %s:%d: attempting to add layer '%s'\n", __FUNCTION__, __LINE__, layer->type_string);

	TreeIndex * selected_index = this->tree_view->get_selected_item();
	if (true) { /* kamilFIXME: "if (!selected_index) { */
		/* No particular layer is selected in panel, so the
		   layer to be added goes directly under top level
		   aggregate layer. */
		fprintf(stderr, "INFO: %s:%d: No selected layer, adding layer '%s' under top level aggregate layer\n", __FUNCTION__, __LINE__, layer->type_string);
		this->toplayer->add_layer(layer, true);

	} else {
		/* Some item in tree view is already selected. Let's find a good
		   place for given layer to be added - a first aggregate
		   layer that we meet going up in hierarchy. */

		TreeIndex * replace_index = NULL;
		Layer * current = NULL;

		if (this->tree_view->get_item_type(selected_index) == TreeItemType::SUBLAYER) {
			current = this->tree_view->get_parent_layer(selected_index);
			fprintf(stderr, "INFO: %s:%d: Capturing parent layer '%s' as current layer\n",
				__FUNCTION__, __LINE__, current->type_string);
		} else {
			current = this->tree_view->get_layer(selected_index);
			fprintf(stderr, "INFO: %s:%d: Capturing selected layer '%s' as current layer\n",
				__FUNCTION__, __LINE__, current->type_string);
		}
		assert (current->realized);
		replace_index = current->index;

		/* Go further up until you find first aggregate layer. */
		while (current->type != LayerType::AGGREGATE) {
			current = this->tree_view->get_parent_layer(selected_index);
			selected_index = current->index;
			assert (current->realized);
		}


		LayerAggregate * aggregate = (LayerAggregate *) current;
#ifndef SLAVGPS_QT
		if (replace_index) {
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
	TreeIndex * selected_index = this->tree_view->get_selected_item();
	/* TODO: deactivate the buttons and stuff. */
	if (!selected_index) {
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




bool vik_layers_panel_properties_cb(LayersPanel * panel)
{
	return panel->properties();
}




bool LayersPanel::properties()
{
	assert (this->viewport);

	TreeIndex * index = this->tree_view->get_selected_item();
	if (this->tree_view->get_item_type(index) == TreeItemType::LAYER) {
		if (this->tree_view->get_layer(index)->type == LayerType::AGGREGATE) {
			a_dialog_info_msg("Aggregate Layer has no settable properties.", "Properties");
		} else {
			Layer * layer = this->tree_view->get_layer(index);
			if (layer->properties_dialog(this->viewport)) {
				layer->emit_update();
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
		this->toplayer->draw(this->viewport);
	}
}




void vik_layers_panel_cut_selected_cb(LayersPanel * panel)
{
	panel->cut_selected();
}




void LayersPanel::cut_selected()
{
        TreeIndex * index = this->tree_view->get_selected_item();
	if (!index) {
		/* Nothing to do. */
		return;
	}

	TreeItemType type = this->tree_view->get_item_type(index);



	if (type == TreeItemType::LAYER) {
		LayerAggregate * parent = (LayerAggregate *) this->tree_view->get_parent_layer(index);
#ifndef SLAVGPS_QT
		if (parent){
			/* Reset trigger if trigger deleted. */
			if (this->get_selected_layer()->the_same_object(this->viewport->get_trigger())) {
				this->viewport->set_trigger(NULL);
			}

			a_clipboard_copy_selected(this);

			if (parent->type == LayerType::AGGREGATE) {
				g_signal_emit(G_OBJECT(this->panel_box_), layers_panel_signals[VLP_DELETE_LAYER_SIGNAL], 0);

				if (parent->delete_layer(index)) {
					this->emit_update_cb();
				}
			}
		} else {
			a_dialog_info_msg(this->get_toolkit_window(), _("You cannot cut the Top Layer."));
		}
#endif
	} else if (type == TreeItemType::SUBLAYER) {
		Layer * selected = this->get_selected_layer();
		SublayerType sublayer_type = this->tree_view->get_sublayer_type(index);
		selected->cut_sublayer(sublayer_type, selected->tree_view->get_sublayer_uid(index));
	}
}




void vik_layers_panel_copy_selected_cb(LayersPanel * panel)
{
	panel->copy_selected();
}




void LayersPanel::copy_selected()
{
	if (!this->tree_view->get_selected_item()) {
		/* Nothing to do. */
		return;
	}
#ifndef SLAVGPS_QT
	/* NB clipboard contains layer vs sublayer logic, so don't need to do it here. */
	a_clipboard_copy_selected(this);
#endif
}




bool vik_layers_panel_paste_selected_cb(LayersPanel * panel)
{
	return panel->paste_selected();
}




bool LayersPanel::paste_selected()
{
	if (!this->tree_view->get_selected_item()) {
		/* Nothing to do. */
		return false;
	}
#ifndef SLAVGPS_QT
	return a_clipboard_paste(this);
#endif
}




void vik_layers_panel_delete_selected_cb(LayersPanel * panel)
{
	panel->delete_selected();
}




void LayersPanel::delete_selected()
{
	TreeIndex * index = this->tree_view->get_selected_item();
	if (!index) {
		/* Nothing to do. */
		return;
	}

	TreeItemType type = this->tree_view->get_item_type(index);
	if (type == TreeItemType::LAYER) {
		Layer * layer = this->tree_view->get_layer(index);

#ifndef SLAVGPS_QT
		/* Get confirmation from the user. */
		if (!a_dialog_yes_or_no(this->get_toolkit_window(),
					 _("Are you sure you want to delete %s?"),
					 layer->get_name())) {
			return;
		}

		LayerAggregate * parent = (LayerAggregate *) this->tree_view->get_parent_layer(index);
		if (parent) {
			/* Reset trigger if trigger deleted. */
			if (this->get_selected_layer()->the_same_object(this->viewport->get_trigger())) {
				this->viewport->set_trigger(NULL);
			}

			if (parent->type == LayerType::AGGREGATE) {

				g_signal_emit(G_OBJECT(this->panel_box_), layers_panel_signals[VLP_DELETE_LAYER_SIGNAL], 0);

				if (parent->delete_layer(index)) {
					this->emit_update_cb();
				}
			}
		} else {
			a_dialog_info_msg(this->get_toolkit_window(), _("You cannot delete the Top Layer."));
		}
#endif
	} else if (type == TreeItemType::SUBLAYER) {
		Layer * selected = this->get_selected_layer();
		SublayerType sublayer_type = this->tree_view->get_sublayer_type(index);
		selected->delete_sublayer(sublayer_type, selected->tree_view->get_sublayer_uid(index));
	}
}




Layer * LayersPanel::get_selected_layer()
{
	TreeIndex * index = this->tree_view->get_selected_item();
	if (!index) {
		return NULL;
	}

	TreeItemType type = this->tree_view->get_item_type(index);
	TreeIndex * parent = NULL;
	while (type != TreeItemType::LAYER) {
		if (NULL == (parent = this->tree_view->get_parent_index(index))) {
			return NULL;
		}
		index = parent;
		type = this->tree_view->get_item_type(index);
	}

	return this->tree_view->get_layer(index);
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
#ifndef SLAVGPS_QT
	std::list<Layer *> * layers = new std::list<Layer *>;
	return this->toplayer->get_all_layers_of_type(layers, layer_type, include_invisible);
#endif
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
		g_signal_emit(G_OBJECT(this->panel_box_), layers_panel_signals[VLP_DELETE_LAYER_SIGNAL], 0);
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
		gtk_widget_show(GTK_WIDGET (this->panel_box_));
	} else {
		gtk_widget_hide(GTK_WIDGET (this->panel_box_));
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




GtkWindow * LayersPanel::get_toolkit_window(void)
{
#ifndef SLAVGPS_QT
	return VIK_GTK_WINDOW_FROM_WIDGET(GTK_WIDGET (this->panel_box_));
#endif
}




GtkWidget * LayersPanel::get_toolkit_widget(void)
{
#ifndef SLAVGPS_QT
	return GTK_WIDGET (this->panel_box_);
#endif
}





void LayersPanel::contextMenuEvent(QContextMenuEvent * event)
{
	//TreeView * tree_view = this->layers_panel->get_treeview();

	QPoint orig = event->pos();
	qDebug() << "II: Layers Panel: context menu event: event @" << orig.x() << orig.y();

	QPoint v = this->tree_view->pos();
	qDebug() << "II: Layers Panel: context menu event: viewport @" << v;
	QPoint t = this->tree_view->viewport()->pos();
	qDebug() << "II: Layers Panel: context menu event: treeview @" << t;

	orig.setX(orig.x() - v.x() - t.x());
	orig.setY(orig.y() - v.y() - t.y());


	QPoint point = orig;//this->tree_view->viewport()->mapToGlobal();
	//QPoint point = event->pos();
	QModelIndex ind = this->tree_view->indexAt(point);

	if (ind.isValid()) {
		qDebug() << "II: Layers Panel: context menu event: valid index";
		qDebug() << "II: Layers Panel: context menu event: row = " << ind.row();
		TreeIndex index = QPersistentModelIndex(ind);

		Layer * layer = this->tree_view->get_layer(&index);
		if (layer) {
			qDebug() << "II: Layers Panel: context menu event: layer type =" << layer->type_string;
		} else {
			qDebug() << "II: Layers Panel: context menu event: layer type is NULL";
		}

		QModelIndex parent = index.parent();
		if (parent.isValid()) {
			QModelIndex index2 = parent.child(ind.row(), 0);
			qDebug() << "II: Layers Panel: context menu event: item" << index << "index2" << index2;
			if (index.isValid()) {
				qDebug() << "II: Layers Panel: context menu event: index.row =" << index.row() << "index.column =" << index.column() << "text =" << this->tree_view->model->itemFromIndex(index2)->text();
			}
			index = index2;
		}
		QMenu menu(this);
		this->window->get_layer_menu(&menu);
		menu.addAction(this->qa_layer_cut);
		menu.addAction(this->qa_layer_copy);
		menu.addAction(this->qa_layer_paste);
		menu.addAction(this->qa_layer_remove);
		menu.exec(QCursor::pos());
	} else {
		qDebug() << "II: Layers Panel: context menu event: INvalid index";
	}
}
