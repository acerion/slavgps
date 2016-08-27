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

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "settings.h"
#include "viklayerspanel.h"
#include "dialog.h"
#include "clipboard.h"
#include "globals.h"
#include "vikwindow.h"




using namespace SlavGPS;




typedef struct {
	LayersPanel * panel;
	LayerType layer_type;
} new_layer_data_t;

enum {
	VLP_UPDATE_SIGNAL,
	VLP_DELETE_LAYER_SIGNAL,
	VLP_LAST_SIGNAL
};

static unsigned int layers_panel_signals[VLP_LAST_SIGNAL] = { 0 };




static void vik_layers_panel_emit_update_cb(LayersPanel * panel);
static void vik_layers_panel_cut_selected_cb(LayersPanel * panel);
static void vik_layers_panel_copy_selected_cb(LayersPanel * panel);
static bool vik_layers_panel_paste_selected_cb(LayersPanel * panel);
static void vik_layers_panel_delete_selected_cb(LayersPanel * panel);
static bool vik_layers_panel_properties_cb(LayersPanel * panel);

static GtkActionEntry entries[] = {
	{ "Cut",    GTK_STOCK_CUT,    N_("C_ut"),       NULL, NULL, (GCallback) vik_layers_panel_cut_selected_cb    },
	{ "Copy",   GTK_STOCK_COPY,   N_("_Copy"),      NULL, NULL, (GCallback) vik_layers_panel_copy_selected_cb   },
	{ "Paste",  GTK_STOCK_PASTE,  N_("_Paste"),     NULL, NULL, (GCallback) vik_layers_panel_paste_selected_cb  },
	{ "Delete", GTK_STOCK_DELETE, N_("_Delete"),    NULL, NULL, (GCallback) vik_layers_panel_delete_selected_cb },
};




static void layers_item_toggled_cb(LayersPanel * panel, GtkTreeIter * iter);
static void layers_item_edited_cb(LayersPanel * panel, GtkTreeIter * iter, char const * new_text);
static void menu_popup_cb(LayersPanel * panel);
static void layers_popup_cb(LayersPanel * panel);
static bool layers_button_press_cb(LayersPanel * panel, GdkEventButton * event);
static bool layers_key_press_cb(LayersPanel * panel, GdkEventKey * event);
static void layers_move_item_up_cb(LayersPanel * panel);
static void layers_move_item_down_cb(LayersPanel * panel);
static void layers_panel_finalize(GObject * gob);




void SlavGPS::layers_panel_init(void)
{
	layers_panel_signals[VLP_UPDATE_SIGNAL] = g_signal_new("update", G_TYPE_OBJECT, (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION), 0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
	layers_panel_signals[VLP_DELETE_LAYER_SIGNAL] = g_signal_new("delete_layer", G_TYPE_OBJECT, (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION), 0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}




LayersPanel::LayersPanel()
{
	this->toplayer = NULL;
	memset(&this->toplayer_iter, 0, sizeof (GtkTreeIter));
	this->viewport = NULL; /* reference */

	this->panel_box_ = (GtkVBox *) gtk_vbox_new(false, 2);

	GtkWidget * hbox = gtk_hbox_new(true, 2);
	this->tree_view = new TreeView();


	/* All this stuff has been moved here from
	   vik_layers_panel_init(), because leaving it in _init()
	   caused problems during execution time. */

	Viewport * viewport = NULL;
	this->toplayer = new LayerAggregate(viewport);
	this->toplayer->rename(_("Top Layer"));
	int a = g_signal_connect_swapped(G_OBJECT(this->toplayer->vl), "update", G_CALLBACK(vik_layers_panel_emit_update_cb), this);

	this->tree_view->add_layer(NULL, &(this->toplayer_iter), this->toplayer->name, NULL, true, this->toplayer, (int) LayerType::AGGREGATE, LayerType::AGGREGATE, 0);
	this->toplayer->realize(this->tree_view, &(this->toplayer_iter));

	a = g_signal_connect_swapped (this->tree_view->get_toolkit_widget(), "popup_menu", G_CALLBACK(menu_popup_cb), this);
	a = g_signal_connect_swapped (this->tree_view->get_toolkit_widget(), "button_press_event", G_CALLBACK(layers_button_press_cb), this);
	a = g_signal_connect_swapped (this->tree_view->get_toolkit_widget(), "item_toggled", G_CALLBACK(layers_item_toggled_cb), this);
	a = g_signal_connect_swapped (this->tree_view->get_toolkit_widget(), "item_edited", G_CALLBACK(layers_item_edited_cb), this);
	a = g_signal_connect_swapped (this->tree_view->get_toolkit_widget(), "key_press_event", G_CALLBACK(layers_key_press_cb), this);

	/* Add button. */
	GtkWidget * addimage = gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_SMALL_TOOLBAR);
	GtkWidget * addbutton = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER(addbutton), addimage);
	gtk_widget_set_tooltip_text (GTK_WIDGET(addbutton), _("Add new layer"));
	gtk_box_pack_start (GTK_BOX(hbox), addbutton, true, true, 0);
	g_signal_connect_swapped (G_OBJECT(addbutton), "clicked", G_CALLBACK(layers_popup_cb), this);
	/* Remove button. */
	GtkWidget * removeimage = gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_SMALL_TOOLBAR);
	GtkWidget * removebutton = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER(removebutton), removeimage);
	gtk_widget_set_tooltip_text (GTK_WIDGET(removebutton), _("Remove selected layer"));
	gtk_box_pack_start (GTK_BOX(hbox), removebutton, true, true, 0);
	g_signal_connect_swapped (G_OBJECT(removebutton), "clicked", G_CALLBACK(vik_layers_panel_delete_selected_cb), this);
	/* Up button. */
	GtkWidget * upimage = gtk_image_new_from_stock (GTK_STOCK_GO_UP, GTK_ICON_SIZE_SMALL_TOOLBAR);
	GtkWidget * upbutton = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER(upbutton), upimage);
	gtk_widget_set_tooltip_text (GTK_WIDGET(upbutton), _("Move selected layer up"));
	gtk_box_pack_start (GTK_BOX(hbox), upbutton, true, true, 0);
	g_signal_connect_swapped (G_OBJECT(upbutton), "clicked", G_CALLBACK(layers_move_item_up_cb), this);
	/* Down button. */
	GtkWidget * downimage = gtk_image_new_from_stock (GTK_STOCK_GO_DOWN, GTK_ICON_SIZE_SMALL_TOOLBAR);
	GtkWidget * downbutton = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER(downbutton), downimage);
	gtk_widget_set_tooltip_text (GTK_WIDGET(downbutton), _("Move selected layer down"));
	gtk_box_pack_start (GTK_BOX(hbox), downbutton, true, true, 0);
	g_signal_connect_swapped (G_OBJECT(downbutton), "clicked", G_CALLBACK(layers_move_item_down_cb), this);
	/* Cut button. */
	GtkWidget * cutimage = gtk_image_new_from_stock (GTK_STOCK_CUT, GTK_ICON_SIZE_SMALL_TOOLBAR);
	GtkWidget * cutbutton = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER(cutbutton), cutimage);
	gtk_widget_set_tooltip_text (GTK_WIDGET(cutbutton), _("Cut selected layer"));
	gtk_box_pack_start (GTK_BOX(hbox), cutbutton, true, true, 0);
	g_signal_connect_swapped (G_OBJECT(cutbutton), "clicked", G_CALLBACK(vik_layers_panel_cut_selected_cb), this);
	/* Copy button. */
	GtkWidget * copyimage = gtk_image_new_from_stock (GTK_STOCK_COPY, GTK_ICON_SIZE_SMALL_TOOLBAR);
	GtkWidget * copybutton = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER(copybutton), copyimage);
	gtk_widget_set_tooltip_text (GTK_WIDGET(copybutton), _("Copy selected layer"));
	gtk_box_pack_start (GTK_BOX(hbox), copybutton, true, true, 0);
	g_signal_connect_swapped (G_OBJECT(copybutton), "clicked", G_CALLBACK(vik_layers_panel_copy_selected_cb), this);
	/* Paste button. */
	GtkWidget * pasteimage = gtk_image_new_from_stock (GTK_STOCK_PASTE, GTK_ICON_SIZE_SMALL_TOOLBAR);
	GtkWidget * pastebutton = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER(pastebutton),pasteimage);
	gtk_widget_set_tooltip_text (GTK_WIDGET(pastebutton), _("Paste layer into selected container layer or otherwise above selected layer"));
	gtk_box_pack_start (GTK_BOX(hbox), pastebutton, true, true, 0);
	g_signal_connect_swapped (G_OBJECT(pastebutton), "clicked", G_CALLBACK(vik_layers_panel_paste_selected_cb), this);

	GtkWidget * scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER (scrolledwindow), this->tree_view->get_toolkit_widget());

	gtk_box_pack_start(GTK_BOX(this->panel_box_), scrolledwindow, true, true, 0);
	gtk_box_pack_start(GTK_BOX(this->panel_box_), hbox, false, false, 0);
}




LayersPanel::~LayersPanel()
{
	/* kamilTODO: improve the destructor. */

	fprintf(stderr, "~LayersPanel() called\n");

	this->toplayer->unref();

	/* kamilFIXME: free this pointer. */
	this->panel_box_ = NULL;
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




/**
 * Create menu popup on demand.
 * @full: offer cut/copy options as well - not just the new layer options
 */
static GtkWidget* layers_panel_create_popup(LayersPanel * panel, bool full)
{
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
		if (vik_layer_get_interface(ii)->icon) {
			menuitem = gtk_image_menu_item_new_with_mnemonic(vik_layer_get_interface(ii)->name);
			gtk_image_menu_item_set_image((GtkImageMenuItem*)menuitem, gtk_image_new_from_pixbuf(vik_layer_load_icon(ii)));
		} else {
			menuitem = gtk_menu_item_new_with_mnemonic (vik_layer_get_interface(ii)->name);
		}

		lpnl[(int) ii].panel = panel;
		lpnl[(int) ii].layer_type = ii;

		g_signal_connect_swapped (G_OBJECT(menuitem), "activate", G_CALLBACK(layers_panel_new_layer), &(lpnl[(int) ii]));
		gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
		gtk_widget_show(menuitem);
	}

	return menu;
}




/**
 * Invoke the actual drawing via signal method.
 */
static bool idle_draw_panel(LayersPanel * panel)
{
	g_signal_emit(G_OBJECT(panel->panel_box_), layers_panel_signals[VLP_UPDATE_SIGNAL], 0);
	return false; /* Nothing else to do. */
}




void vik_layers_panel_emit_update_cb(LayersPanel * panel)
{
	panel->emit_update();
}




void LayersPanel::emit_update()
{
	GThread * thread = window_from_widget(this->panel_box_)->get_thread();
	if (!thread) {
		/* Do nothing. */
		return;
	}

	/* Only ever draw when there is time to do so. */
	if (g_thread_self() != thread) {
		/* Drawing requested from another (background) thread, so handle via the gdk thread method. */
		gdk_threads_add_idle((GSourceFunc) idle_draw_panel, this);
	} else {
		g_idle_add((GSourceFunc) idle_draw_panel, this);
	}
}




static void layers_item_toggled_cb(LayersPanel * panel, GtkTreeIter * iter)
{
	panel->item_toggled(iter);
}




void LayersPanel::item_toggled(GtkTreeIter * iter)
{
	/* Get type and data. */
	TreeItemType type = this->tree_view->get_item_type(iter);

	bool visible;
	switch (type) {
	case TreeItemType::LAYER: {
		Layer * layer = this->tree_view->get_layer(iter);;
		visible = (layer->visible ^= 1);
		vik_layer_emit_update_although_invisible(layer); /* Set trigger for half-drawn. */
		break;
		}
	case TreeItemType::SUBLAYER: {
		sg_uid_t sublayer_uid = this->tree_view->get_sublayer_uid(iter);
		Layer * parent = this->tree_view->get_parent_layer(iter);

		visible = parent->sublayer_toggle_visible(this->tree_view->get_sublayer_type(iter), sublayer_uid);
		vik_layer_emit_update_although_invisible(parent);
		break;
	}
	default:
		return;
	}

	this->tree_view->set_visibility(iter, visible);
}




static void layers_item_edited_cb(LayersPanel * panel, GtkTreeIter * iter, char const * new_text)
{
	panel->item_edited(iter, new_text);
}




void LayersPanel::item_edited(GtkTreeIter * iter, char const * new_text)
{
	if (!new_text) {
		return;
	}

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
}




static bool layers_button_press_cb(LayersPanel * panel, GdkEventButton * event)
{
	return panel->button_press(event);
}




bool LayersPanel::button_press(GdkEventButton * event)
{
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
	return false;
}




static bool layers_key_press_cb(LayersPanel * panel, GdkEventKey * event)
{
	return panel->key_press(event);
}




bool LayersPanel::key_press(GdkEventKey * event)
{
	/* Accept all forms of delete keys. */
	if (event->keyval == GDK_Delete || event->keyval == GDK_KP_Delete || event->keyval == GDK_BackSpace) {
		this->delete_selected();
		return true;
	}
	return false;
}




void LayersPanel::popup(GtkTreeIter * iter, MouseButton mouse_button)
{
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
}




static void menu_popup_cb(LayersPanel * panel)
{
	GtkTreeIter iter;
	panel->popup(panel->tree_view->get_selected_iter(&iter) ? &iter : NULL, MouseButton::OTHER);
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
	GtkTreeIter iter;

	/* Could be something different so we have to do this. */
	layer->change_coord_mode(this->viewport->get_coord_mode());
	fprintf(stderr, "INFO: %s:%d: attempting to add layer '%s'\n", __FUNCTION__, __LINE__, layer->type_string);

	if (!this->tree_view->get_selected_iter(&iter)) {

		/* No particular layer is selected in panel, so the
		   layer to be added goes directly under top level
		   aggregate layer. */
		fprintf(stderr, "INFO: %s:%d: No selected layer, adding layer '%s' under top level aggregate layer\n", __FUNCTION__, __LINE__, layer->type_string);
		this->toplayer->add_layer(layer, true);

	} else {
		/* Some item in tree view is already selected. Let's find a good
		   place for given layer to be added - a first aggregate
		   layer that we meet going up in hierarchy. */

		GtkTreeIter * replace_iter = NULL;
		Layer * current = NULL;

		if (this->tree_view->get_item_type(&iter) == TreeItemType::SUBLAYER) {
			current = this->tree_view->get_parent_layer(&iter);
			fprintf(stderr, "INFO: %s:%d: Capturing parent layer '%s' as current layer\n",
				__FUNCTION__, __LINE__, current->type_string);
		} else {
			current = this->tree_view->get_layer(&iter);
			fprintf(stderr, "INFO: %s:%d: Capturing selected layer '%s' as current layer\n",
				__FUNCTION__, __LINE__, current->type_string);
		}
		assert (current->realized);

		/* Go further up until you find first aggregate layer. */
		while (current->type != LayerType::AGGREGATE) {
			current = this->tree_view->get_parent_layer(&iter);
			iter = current->iter;
			assert (current->realized);
		}

		LayerAggregate * aggregate = (LayerAggregate *) current;
		if (replace_iter) {
			aggregate->insert_layer(layer, replace_iter);
		} else {
			aggregate->add_layer(layer, true);
		}
	}

	this->emit_update();
}




void LayersPanel::move_item(bool up)
{
	GtkTreeIter iter;

	/* TODO: deactivate the buttons and stuff. */
	if (!this->tree_view->get_selected_iter(&iter)) {
		return;
	}

	this->tree_view->select(&iter); /* Cancel any layer-name editing going on... */

	if (this->tree_view->get_item_type(&iter) == TreeItemType::LAYER) {
		LayerAggregate * parent = (LayerAggregate *) this->tree_view->get_parent_layer(&iter);
		if (parent) { /* Not toplevel. */
			parent->move_layer(&iter, up);
			this->emit_update();
		}
	}
}




bool vik_layers_panel_properties_cb(LayersPanel * panel)
{
	return panel->properties();
}




bool LayersPanel::properties()
{
	GtkTreeIter iter;
	assert (this->viewport);

	if (this->tree_view->get_selected_iter(&iter) && this->tree_view->get_item_type(&iter) == TreeItemType::LAYER) {
		if (this->tree_view->get_layer(&iter)->type == LayerType::AGGREGATE) {
			a_dialog_info_msg(VIK_GTK_WINDOW_FROM_WIDGET(this->panel_box_), _("Aggregate Layers have no settable properties."));
		}
		Layer * layer = this->tree_view->get_layer(&iter);
		if (vik_layer_properties(layer, this->viewport)) {
			layer->emit_update();
		}
		return true;
	} else {
		return false;
	}
}




void LayersPanel::draw_all()
{
	if (this->viewport && this->toplayer->visible) {
		Layer * layer = (Layer *) this->toplayer;
		layer->draw(this->viewport);
	}
}




void vik_layers_panel_cut_selected_cb(LayersPanel * panel)
{
	panel->cut_selected();
}




void LayersPanel::cut_selected()
{
	GtkTreeIter iter;

	if (!this->tree_view->get_selected_iter(&iter)) {
		/* Nothing to do. */
		return;
	}

	TreeItemType type = this->tree_view->get_item_type(&iter);

	if (type == TreeItemType::LAYER) {
		LayerAggregate * parent = (LayerAggregate *) this->tree_view->get_parent_layer(&iter);
		if (parent){
			/* Reset trigger if trigger deleted. */
			if (this->get_selected()->the_same_object(this->viewport->get_trigger())) {
				this->viewport->set_trigger(NULL);
			}

			a_clipboard_copy_selected(this);

			if (parent->type == LayerType::AGGREGATE) {

				g_signal_emit(G_OBJECT(this->panel_box_), layers_panel_signals[VLP_DELETE_LAYER_SIGNAL], 0);

				if (parent->delete_layer(&iter)) {
					this->emit_update();
				}
			}
		} else {
			a_dialog_info_msg(this->get_toolkit_window(), _("You cannot cut the Top Layer."));
		}
	} else if (type == TreeItemType::SUBLAYER) {
		Layer * selected = this->get_selected();
		SublayerType sublayer_type = this->tree_view->get_sublayer_type(&iter);
		selected->cut_sublayer(sublayer_type, selected->tree_view->get_sublayer_uid(&iter));
	}
}




void vik_layers_panel_copy_selected_cb(LayersPanel * panel)
{
	panel->copy_selected();
}




void LayersPanel::copy_selected()
{
	GtkTreeIter iter;
	if (!this->tree_view->get_selected_iter(&iter)) {
		/* Nothing to do. */
		return;
	}
	/* NB clipboard contains layer vs sublayer logic, so don't need to do it here. */
	a_clipboard_copy_selected(this);
}




bool vik_layers_panel_paste_selected_cb(LayersPanel * panel)
{
	return panel->paste_selected();
}




bool LayersPanel::paste_selected()
{
	GtkTreeIter iter;
	if (!this->tree_view->get_selected_iter(&iter)) {
		/* Nothing to do. */
		return false;
	}
	return a_clipboard_paste(this);
}




void vik_layers_panel_delete_selected_cb(LayersPanel * panel)
{
	panel->delete_selected();
}




void LayersPanel::delete_selected()
{
	GtkTreeIter iter;

	if (!this->tree_view->get_selected_iter(&iter)) {
		/* Nothing to do. */
		return;
	}

	TreeItemType type = this->tree_view->get_item_type(&iter);

	if (type == TreeItemType::LAYER) {
		Layer * layer = this->tree_view->get_layer(&iter);
		/* Get confirmation from the user. */
		if (! a_dialog_yes_or_no(this->get_toolkit_window(),
					 _("Are you sure you want to delete %s?"),
					 layer->get_name())) {
			return;
		}

		LayerAggregate * parent = (LayerAggregate *) this->tree_view->get_parent_layer(&iter);
		if (parent) {
			/* Reset trigger if trigger deleted. */
			if (this->get_selected()->the_same_object(this->viewport->get_trigger())) {
				this->viewport->set_trigger(NULL);
			}

			if (parent->type == LayerType::AGGREGATE) {

				g_signal_emit(G_OBJECT(this->panel_box_), layers_panel_signals[VLP_DELETE_LAYER_SIGNAL], 0);

				if (parent->delete_layer(&iter)) {
					this->emit_update();
				}
			}
		} else {
			a_dialog_info_msg(this->get_toolkit_window(), _("You cannot delete the Top Layer."));
		}
	} else if (type == TreeItemType::SUBLAYER) {
		Layer * selected = this->get_selected();
		SublayerType sublayer_type = this->tree_view->get_sublayer_type(&iter);
		selected->delete_sublayer(sublayer_type, selected->tree_view->get_sublayer_uid(&iter));
	}
}




Layer * LayersPanel::get_selected()
{
	GtkTreeIter iter, parent;
	memset(&iter, 0, sizeof (GtkTreeIter));

	if (!this->tree_view->get_selected_iter(&iter)) {
		return NULL;
	}

	TreeItemType type = this->tree_view->get_item_type(&iter);

	while (type != TreeItemType::LAYER) {
		if (!this->tree_view->get_parent_iter(&iter, &parent)) {
			return NULL;
		}
		iter = parent;
		type = this->tree_view->get_item_type(&iter);
	}

	return this->tree_view->get_layer(&iter);
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
	Layer * layer = panel->get_selected();
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
	Layer * layer = this->get_selected();
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
		g_signal_emit(G_OBJECT(this->panel_box_), layers_panel_signals[VLP_DELETE_LAYER_SIGNAL], 0);
		this->toplayer->clear(); /* simply deletes all layers */
	}
}




void LayersPanel::change_coord_mode(VikCoordMode mode)
{
	this->toplayer->change_coord_mode(mode);
}




void LayersPanel::set_visible(bool visible)
{
	if (visible) {
		gtk_widget_show(GTK_WIDGET (this->panel_box_));
	} else {
		gtk_widget_hide(GTK_WIDGET (this->panel_box_));
	}
	return;
}




bool LayersPanel::get_visible(void)
{
	return GTK_WIDGET_VISIBLE (GTK_WIDGET (this->panel_box_));
}




TreeView * LayersPanel::get_treeview()
{
	return this->tree_view;
}




GtkWindow * LayersPanel::get_toolkit_window(void)
{
	return VIK_GTK_WINDOW_FROM_WIDGET(GTK_WIDGET (this->panel_box_));
}




GtkWidget * LayersPanel::get_toolkit_widget(void)
{
	return GTK_WIDGET (this->panel_box_);
}
