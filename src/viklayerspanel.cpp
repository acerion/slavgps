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

#include "viking.h"
#include "settings.h"
#include "viklayerspanel.h"
#include "dialog.h"
#include "clipboard.h"
#include "globals.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

enum {
	VLP_UPDATE_SIGNAL,
	VLP_DELETE_LAYER_SIGNAL,
	VLP_LAST_SIGNAL
};

static unsigned int layers_panel_signals[VLP_LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class;



static GtkActionEntry entries[] = {
	{ "Cut",    GTK_STOCK_CUT,    N_("C_ut"),       NULL, NULL, (GCallback) vik_layers_panel_cut_selected },
	{ "Copy",   GTK_STOCK_COPY,   N_("_Copy"),      NULL, NULL, (GCallback) vik_layers_panel_copy_selected },
	{ "Paste",  GTK_STOCK_PASTE,  N_("_Paste"),     NULL, NULL, (GCallback) vik_layers_panel_paste_selected },
	{ "Delete", GTK_STOCK_DELETE, N_("_Delete"),    NULL, NULL, (GCallback) vik_layers_panel_delete_selected },
};

static void layers_item_toggled_cb(VikLayersPanel *vlp, GtkTreeIter *iter);
static void layers_item_toggled(LayersPanel * panel, GtkTreeIter *iter);

static void layers_item_edited_cb(VikLayersPanel *vlp, GtkTreeIter *iter, const char *new_text);
static void layers_item_edited(LayersPanel * panel, GtkTreeIter *iter, const char *new_text);

static void menu_popup_cb(VikLayersPanel *vlp); /* signals-related. */
static void layers_popup_cb(VikLayersPanel *vlp);  /* signals-related. */
static void layers_popup(LayersPanel * panel, GtkTreeIter *iter, int mouse_button);

static bool layers_button_press_cb(VikLayersPanel *vlp, GdkEventButton *event);
static bool layers_button_press(LayersPanel * panel, GdkEventButton *event);

static bool layers_key_press_cb(VikLayersPanel *vlp, GdkEventKey *event);
static bool layers_key_press(LayersPanel * panel, GdkEventKey *event);

static void layers_move_item(LayersPanel * panel, bool up);
static void layers_move_item_up_cb(VikLayersPanel *vlp); /* signals-related. */
static void layers_move_item_down_cb(VikLayersPanel *vlp); /* signals-related. */
static void layers_panel_finalize(GObject *gob);

G_DEFINE_TYPE (VikLayersPanel, vik_layers_panel, GTK_TYPE_VBOX)

static void vik_layers_panel_class_init(VikLayersPanelClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = layers_panel_finalize;

	parent_class = (GObjectClass *) g_type_class_peek_parent(klass);

	layers_panel_signals[VLP_UPDATE_SIGNAL] = g_signal_new("update", G_TYPE_FROM_CLASS (klass), (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION), G_STRUCT_OFFSET (VikLayersPanelClass, update), NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
	layers_panel_signals[VLP_DELETE_LAYER_SIGNAL] = g_signal_new("delete_layer", G_TYPE_FROM_CLASS (klass), (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION), G_STRUCT_OFFSET (VikLayersPanelClass, update), NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

VikLayersPanel *vik_layers_panel_new()
{
	return VIK_LAYERS_PANEL (g_object_new(VIK_LAYERS_PANEL_TYPE, NULL));
}

void vik_layers_panel_set_viewport(LayersPanel * panel, VikViewport *vvp)
{
	panel->vvp = vvp;
	/* TODO: also update GCs (?) */
}

VikViewport *vik_layers_panel_get_viewport(LayersPanel * panel)
{
	return panel->vvp;
}

static bool layers_panel_new_layer(void * lpnl[2])
{
	return vik_layers_panel_new_layer(((VikLayersPanel *) lpnl[0])->gob->panel_ref, (VikLayerTypeEnum) KPOINTER_TO_INT(lpnl[1]));
}

/**
 * Create menu popup on demand
 * @full: offer cut/copy options as well - not just the new layer options
 */
static GtkWidget* layers_panel_create_popup(VikLayersPanel *vlp, bool full)
{
	GtkWidget *menu = gtk_menu_new();
	GtkWidget *menuitem;

	if (full) {
		for (unsigned int ii = 0; ii < G_N_ELEMENTS(entries); ii++) {
			if (entries[ii].stock_id) {
				menuitem = gtk_image_menu_item_new_with_mnemonic(entries[ii].label);
				gtk_image_menu_item_set_image((GtkImageMenuItem*)menuitem, gtk_image_new_from_stock (entries[ii].stock_id, GTK_ICON_SIZE_MENU));
			} else {
				menuitem = gtk_menu_item_new_with_mnemonic(entries[ii].label);
			}

			g_signal_connect_swapped(G_OBJECT(menuitem), "activate", G_CALLBACK(entries[ii].callback), vlp->gob);
			gtk_menu_shell_append(GTK_MENU_SHELL (menu), menuitem);
			gtk_widget_show(menuitem);
		}
	}

	GtkWidget *submenu = gtk_menu_new();
	menuitem = gtk_menu_item_new_with_mnemonic(_("New Layer"));
	gtk_menu_shell_append (GTK_MENU_SHELL(menu), menuitem);
	gtk_widget_show(menuitem);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM (menuitem), submenu);

	// Static: so memory accessible yet not continually allocated
	static void * lpnl[VIK_LAYER_NUM_TYPES][2];

	for (unsigned int ii = 0; ii < VIK_LAYER_NUM_TYPES; ii++) {
		if (vik_layer_get_interface((VikLayerTypeEnum) ii)->icon) {
			menuitem = gtk_image_menu_item_new_with_mnemonic(vik_layer_get_interface((VikLayerTypeEnum) ii)->name);
			gtk_image_menu_item_set_image((GtkImageMenuItem*)menuitem, gtk_image_new_from_pixbuf(vik_layer_load_icon ((VikLayerTypeEnum) ii)));
		} else {
			menuitem = gtk_menu_item_new_with_mnemonic (vik_layer_get_interface((VikLayerTypeEnum) ii)->name);
		}

		lpnl[ii][0] = vlp;
		lpnl[ii][1] = KINT_TO_POINTER(ii);

		g_signal_connect_swapped (G_OBJECT(menuitem), "activate", G_CALLBACK(layers_panel_new_layer), ((VikLayersPanel *) lpnl[ii])->gob);
		gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
		gtk_widget_show (menuitem);
	}

	return menu;
}

static void vik_layers_panel_init(VikLayersPanel * vlp)
{
	vlp->panel.vvp = NULL;
	vlp->gob = vlp;
	vlp->gob->panel_ref = &vlp->panel;
	vlp->gob->panel_ref->gob = vlp;

	GtkWidget * hbox = gtk_hbox_new (true, 2);
	vlp->panel.vt = vik_treeview_new ();

	vlp->panel.toplayer = vik_aggregate_layer_new ();
	vik_layer_rename(VIK_LAYER(vlp->panel.toplayer), _("Top Layer"));
	g_signal_connect_swapped (G_OBJECT(vlp->panel.toplayer), "update", G_CALLBACK(vik_layers_panel_emit_update), vlp->panel_ref);

	vik_treeview_add_layer (vlp->panel.vt, NULL, &(vlp->panel.toplayer_iter), VIK_LAYER(vlp->panel.toplayer)->name, NULL, true, vlp->panel.toplayer, VIK_LAYER_AGGREGATE, VIK_LAYER_AGGREGATE, 0);
	vik_layer_realize (VIK_LAYER(vlp->panel.toplayer), vlp->panel.vt, &(vlp->panel.toplayer_iter));

	g_signal_connect_swapped (vlp->panel.vt, "popup_menu", G_CALLBACK(menu_popup_cb), vlp->gob);
	g_signal_connect_swapped (vlp->panel.vt, "button_press_event", G_CALLBACK(layers_button_press_cb), vlp->gob);
	g_signal_connect_swapped (vlp->panel.vt, "item_toggled", G_CALLBACK(layers_item_toggled_cb), vlp->gob);
	g_signal_connect_swapped (vlp->panel.vt, "item_edited", G_CALLBACK(layers_item_edited_cb), vlp->gob);
	g_signal_connect_swapped (vlp->panel.vt, "key_press_event", G_CALLBACK(layers_key_press_cb), vlp->gob);

	/* Add button */
	GtkWidget * addimage = gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_SMALL_TOOLBAR);
	GtkWidget * addbutton = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER(addbutton), addimage);
	gtk_widget_set_tooltip_text (GTK_WIDGET(addbutton), _("Add new layer"));
	gtk_box_pack_start (GTK_BOX(hbox), addbutton, true, true, 0);
	g_signal_connect_swapped (G_OBJECT(addbutton), "clicked", G_CALLBACK(layers_popup_cb), vlp->gob);
	/* Remove button */
	GtkWidget * removeimage = gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_SMALL_TOOLBAR);
	GtkWidget * removebutton = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER(removebutton), removeimage);
	gtk_widget_set_tooltip_text (GTK_WIDGET(removebutton), _("Remove selected layer"));
	gtk_box_pack_start (GTK_BOX(hbox), removebutton, true, true, 0);
	g_signal_connect_swapped (G_OBJECT(removebutton), "clicked", G_CALLBACK(vik_layers_panel_delete_selected), vlp->gob);
	/* Up button */
	GtkWidget * upimage = gtk_image_new_from_stock (GTK_STOCK_GO_UP, GTK_ICON_SIZE_SMALL_TOOLBAR);
	GtkWidget * upbutton = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER(upbutton), upimage);
	gtk_widget_set_tooltip_text (GTK_WIDGET(upbutton), _("Move selected layer up"));
	gtk_box_pack_start (GTK_BOX(hbox), upbutton, true, true, 0);
	g_signal_connect_swapped (G_OBJECT(upbutton), "clicked", G_CALLBACK(layers_move_item_up_cb), vlp->gob);
	/* Down button */
	GtkWidget * downimage = gtk_image_new_from_stock (GTK_STOCK_GO_DOWN, GTK_ICON_SIZE_SMALL_TOOLBAR);
	GtkWidget * downbutton = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER(downbutton), downimage);
	gtk_widget_set_tooltip_text (GTK_WIDGET(downbutton), _("Move selected layer down"));
	gtk_box_pack_start (GTK_BOX(hbox), downbutton, true, true, 0);
	g_signal_connect_swapped (G_OBJECT(downbutton), "clicked", G_CALLBACK(layers_move_item_down_cb), vlp->gob);
	/* Cut button */
	GtkWidget * cutimage = gtk_image_new_from_stock (GTK_STOCK_CUT, GTK_ICON_SIZE_SMALL_TOOLBAR);
	GtkWidget * cutbutton = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER(cutbutton), cutimage);
	gtk_widget_set_tooltip_text (GTK_WIDGET(cutbutton), _("Cut selected layer"));
	gtk_box_pack_start (GTK_BOX(hbox), cutbutton, true, true, 0);
	g_signal_connect_swapped (G_OBJECT(cutbutton), "clicked", G_CALLBACK(vik_layers_panel_cut_selected), vlp->gob);
	/* Copy button */
	GtkWidget * copyimage = gtk_image_new_from_stock (GTK_STOCK_COPY, GTK_ICON_SIZE_SMALL_TOOLBAR);
	GtkWidget * copybutton = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER(copybutton), copyimage);
	gtk_widget_set_tooltip_text (GTK_WIDGET(copybutton), _("Copy selected layer"));
	gtk_box_pack_start (GTK_BOX(hbox), copybutton, true, true, 0);
	g_signal_connect_swapped (G_OBJECT(copybutton), "clicked", G_CALLBACK(vik_layers_panel_copy_selected), vlp->gob);
	/* Paste button */
	GtkWidget * pasteimage = gtk_image_new_from_stock (GTK_STOCK_PASTE, GTK_ICON_SIZE_SMALL_TOOLBAR);
	GtkWidget * pastebutton = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER(pastebutton),pasteimage);
	gtk_widget_set_tooltip_text (GTK_WIDGET(pastebutton), _("Paste layer into selected container layer or otherwise above selected layer"));
	gtk_box_pack_start (GTK_BOX(hbox), pastebutton, true, true, 0);
	g_signal_connect_swapped (G_OBJECT(pastebutton), "clicked", G_CALLBACK(vik_layers_panel_paste_selected), vlp->gob);

	GtkWidget * scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER(scrolledwindow), GTK_WIDGET(vlp->panel.vt));

	gtk_box_pack_start (GTK_BOX(vlp), scrolledwindow, true, true, 0);
	gtk_box_pack_start (GTK_BOX(vlp), hbox, false, false, 0);
}

/**
 * Invoke the actual drawing via signal method
 */
static bool idle_draw_panel(VikLayersPanel *vlp)
{
	g_signal_emit(G_OBJECT(vlp->gob), layers_panel_signals[VLP_UPDATE_SIGNAL], 0);
	return false; // Nothing else to do
}

void vik_layers_panel_emit_update(LayersPanel * panel)
{
	GThread *thread = vik_window_get_thread(VIK_WINDOW(VIK_GTK_WINDOW_FROM_WIDGET(panel->gob)));
	if (!thread) {
		// Do nothing
		return;
	}

	// Only ever draw when there is time to do so
	if (g_thread_self() != thread) {
		// Drawing requested from another (background) thread, so handle via the gdk thread method
		gdk_threads_add_idle((GSourceFunc) idle_draw_panel, panel->gob);
	} else {
		g_idle_add((GSourceFunc) idle_draw_panel, panel->gob);
	}
}

static void layers_item_toggled_cb(VikLayersPanel *vlp, GtkTreeIter *iter)
{
	layers_item_toggled(vlp->panel_ref, iter);
}

static void layers_item_toggled(LayersPanel * panel, GtkTreeIter *iter)
{
	/* get type and data */
	int type = vik_treeview_item_get_type(panel->vt, iter);
	void * p = vik_treeview_item_get_pointer(panel->vt, iter);

	bool visible;
	switch (type) {
	case VIK_TREEVIEW_TYPE_LAYER:
		visible = (VIK_LAYER(p)->visible ^= 1);
		vik_layer_emit_update_although_invisible(VIK_LAYER(p)); /* set trigger for half-drawn */
		break;
	case VIK_TREEVIEW_TYPE_SUBLAYER:
		visible = vik_layer_sublayer_toggle_visible(VIK_LAYER(vik_treeview_item_get_parent(panel->vt, iter)),
							    vik_treeview_item_get_data(panel->vt, iter), p);
		vik_layer_emit_update_although_invisible(VIK_LAYER(vik_treeview_item_get_parent(panel->vt, iter)));
		break;
	default:
		return;
	}

	vik_treeview_item_set_visible(panel->vt, iter, visible);
}

static void layers_item_edited_cb(VikLayersPanel *vlp, GtkTreeIter *iter, const char *new_text)
{
	layers_item_edited(vlp->panel_ref, iter, new_text);
}

static void layers_item_edited(LayersPanel * panel, GtkTreeIter *iter, const char *new_text)
{
	if (!new_text) {
		return;
	}

	if (new_text[0] == '\0') {
		a_dialog_error_msg(GTK_WINDOW(VIK_WINDOW_FROM_WIDGET(panel->gob)), _("New name can not be blank."));
		return;
	}

	if (vik_treeview_item_get_type(panel->vt, iter) == VIK_TREEVIEW_TYPE_LAYER) {

		/* get iter and layer */
		VikLayer * l = VIK_LAYER (vik_treeview_item_get_pointer(panel->vt, iter));

		if (strcmp(l->name, new_text) != 0) {
			vik_layer_rename(l, new_text);
			vik_treeview_item_set_name(panel->vt, iter, l->name);
		}
	} else {
		const char *name = vik_layer_sublayer_rename_request((VikLayer *) vik_treeview_item_get_parent(panel->vt, iter), new_text, panel->gob, vik_treeview_item_get_data(panel->vt, iter), vik_treeview_item_get_pointer(panel->vt, iter), iter);
		if (name) {
			vik_treeview_item_set_name(panel->vt, iter, name);
		}
	}
}

static bool layers_button_press_cb(VikLayersPanel *vlp, GdkEventButton *event)
{
	return layers_button_press(vlp->panel_ref, event);
}

static bool layers_button_press(LayersPanel * panel, GdkEventButton *event)
{
	if (event->button == 3) {
		static GtkTreeIter iter;
		if (vik_treeview_get_iter_at_pos(panel->vt, &iter, event->x, event->y)) {
			layers_popup(panel, &iter, 3);
			vik_treeview_item_select(panel->vt, &iter);
		} else {
			layers_popup(panel, NULL, 3);
		}
		return true;
	}
	return false;
}

static bool layers_key_press_cb(VikLayersPanel *vlp, GdkEventKey *event)
{
	return layers_key_press(vlp->panel_ref, event);
}

static bool layers_key_press(LayersPanel * panel, GdkEventKey *event)
{
	// Accept all forms of delete keys
	if (event->keyval == GDK_Delete || event->keyval == GDK_KP_Delete || event->keyval == GDK_BackSpace) {
		vik_layers_panel_delete_selected(panel);
		return true;
	}
	return false;
}

static void layers_popup(LayersPanel * panel, GtkTreeIter *iter, int mouse_button)
{
	GtkMenu *menu = NULL;

	if (iter) {
		if (vik_treeview_item_get_type(panel->vt, iter) == VIK_TREEVIEW_TYPE_LAYER) {
			VikLayer *layer = VIK_LAYER(vik_treeview_item_get_pointer(panel->vt, iter));

			if (layer->type == VIK_LAYER_AGGREGATE) {
				menu = GTK_MENU (layers_panel_create_popup(panel->gob, true));
			} else {
				GtkWidget *del, *prop;
				VikStdLayerMenuItem menu_selection = (VikStdLayerMenuItem) vik_layer_get_menu_items_selection(layer);

				menu = GTK_MENU (gtk_menu_new());

				if (menu_selection & VIK_MENU_ITEM_PROPERTY) {
					prop = gtk_image_menu_item_new_from_stock (GTK_STOCK_PROPERTIES, NULL);
					g_signal_connect_swapped (G_OBJECT(prop), "activate", G_CALLBACK(vik_layers_panel_properties), panel->gob);
					gtk_menu_shell_append (GTK_MENU_SHELL (menu), prop);
					gtk_widget_show (prop);
				}

				if (menu_selection & VIK_MENU_ITEM_CUT) {
					del = gtk_image_menu_item_new_from_stock (GTK_STOCK_CUT, NULL);
					g_signal_connect_swapped (G_OBJECT(del), "activate", G_CALLBACK(vik_layers_panel_cut_selected), panel->gob);
					gtk_menu_shell_append (GTK_MENU_SHELL (menu), del);
					gtk_widget_show (del);
				}

				if (menu_selection & VIK_MENU_ITEM_COPY) {
					del = gtk_image_menu_item_new_from_stock (GTK_STOCK_COPY, NULL);
					g_signal_connect_swapped (G_OBJECT(del), "activate", G_CALLBACK(vik_layers_panel_copy_selected), panel->gob);
					gtk_menu_shell_append (GTK_MENU_SHELL (menu), del);
					gtk_widget_show (del);
				}

				if (menu_selection & VIK_MENU_ITEM_PASTE) {
					del = gtk_image_menu_item_new_from_stock (GTK_STOCK_PASTE, NULL);
					g_signal_connect_swapped (G_OBJECT(del), "activate", G_CALLBACK(vik_layers_panel_paste_selected), panel->gob);
					gtk_menu_shell_append (GTK_MENU_SHELL (menu), del);
					gtk_widget_show (del);
				}

				if (menu_selection & VIK_MENU_ITEM_DELETE) {
					del = gtk_image_menu_item_new_from_stock (GTK_STOCK_DELETE, NULL);
					g_signal_connect_swapped (G_OBJECT(del), "activate", G_CALLBACK(vik_layers_panel_delete_selected), panel->gob);
					gtk_menu_shell_append (GTK_MENU_SHELL (menu), del);
					gtk_widget_show (del);
				}
			}
			vik_layer_add_menu_items (layer, menu, panel->gob);
		} else {
			menu = GTK_MENU (gtk_menu_new ());
			if (! vik_layer_sublayer_add_menu_items ((VikLayer *) vik_treeview_item_get_parent(panel->vt, iter), menu, panel->gob, vik_treeview_item_get_data(panel->vt, iter), vik_treeview_item_get_pointer(panel->vt, iter), iter, panel->vvp)) {
				gtk_widget_destroy (GTK_WIDGET(menu));
				return;
			}
			/* TODO: specific things for different types */
		}
	} else {
		menu = GTK_MENU (layers_panel_create_popup(panel->gob, false));
	}
	gtk_menu_popup(menu, NULL, NULL, NULL, NULL, mouse_button, gtk_get_current_event_time());
}

static void menu_popup_cb(VikLayersPanel *vlp)
{
	GtkTreeIter iter;
	layers_popup(vlp->panel_ref, vik_treeview_get_selected_iter (vlp->panel.vt, &iter) ? &iter : NULL, 0);
}

static void layers_popup_cb(VikLayersPanel *vlp)
{
	layers_popup(vlp->panel_ref, NULL, 0);
}

#define VIK_SETTINGS_LAYERS_TRW_CREATE_DEFAULT "layers_create_trw_auto_default"
/**
 * vik_layers_panel_new_layer:
 * @type: type of the new layer
 *
 * Create a new layer and add to panel.
 */
bool vik_layers_panel_new_layer(LayersPanel * panel, VikLayerTypeEnum type)
{
	assert (panel->vvp);
	bool ask_user = false;
	if (type == VIK_LAYER_TRW) {
		(void)a_settings_get_boolean(VIK_SETTINGS_LAYERS_TRW_CREATE_DEFAULT, &ask_user);
	}
	ask_user = !ask_user;
	VikLayer * l = vik_layer_create(type, panel->vvp, ask_user);
	if (l) {
		vik_layers_panel_add_layer(panel, l);
		return true;
	}
	return false;
}

/**
 * vik_layers_panel_add_layer:
 * @l: existing layer
 *
 * Add an existing layer to panel.
 */
void vik_layers_panel_add_layer(LayersPanel * panel, VikLayer *l)
{
	GtkTreeIter iter;
	GtkTreeIter *replace_iter = NULL;

	/* could be something different so we have to do this */
	vik_layer_change_coord_mode(l, panel->vvp->port.get_coord_mode());

	if (! vik_treeview_get_selected_iter(panel->vt, &iter)) {
		vik_aggregate_layer_add_layer(panel->toplayer, l, true);
	} else {
		VikAggregateLayer *addtoagg;
		if (vik_treeview_item_get_type(panel->vt, &iter) == VIK_TREEVIEW_TYPE_LAYER) {
			if (IS_VIK_AGGREGATE_LAYER(vik_treeview_item_get_pointer(panel->vt, &iter))) {
				addtoagg = VIK_AGGREGATE_LAYER(vik_treeview_item_get_pointer(panel->vt, &iter));
			} else {
				VikLayer *vl = VIK_LAYER(vik_treeview_item_get_parent(panel->vt, &iter));
				while (! IS_VIK_AGGREGATE_LAYER(vl)) {
					iter = vl->iter;
					vl = VIK_LAYER(vik_treeview_item_get_parent(panel->vt, &vl->iter));
					assert (vl->realized);
				}
				addtoagg = VIK_AGGREGATE_LAYER(vl);
				replace_iter = &iter;
			}
		} else {
			/* a sublayer is selected, first get its parent (layer), then find the layer's parent (aggr. layer) */
			VikLayer *vl = VIK_LAYER(vik_treeview_item_get_parent(panel->vt, &iter));
			replace_iter = &(vl->iter);
			assert (vl->realized);
			VikLayer *grandpa = (VikLayer *) (vik_treeview_item_get_parent(panel->vt, &(vl->iter)));
			if (IS_VIK_AGGREGATE_LAYER(grandpa)) {
				addtoagg = VIK_AGGREGATE_LAYER(grandpa);
			} else {
				addtoagg = panel->toplayer;
				replace_iter = &grandpa->iter;
			}
		}
		if (replace_iter) {
			vik_aggregate_layer_insert_layer(addtoagg, l, replace_iter);
		} else {
			vik_aggregate_layer_add_layer(addtoagg, l, true);
		}
	}

	vik_layers_panel_emit_update(panel);
}

static void layers_move_item(LayersPanel * panel, bool up)
{
	GtkTreeIter iter;

	/* TODO: deactivate the buttons and stuff */
	if (! vik_treeview_get_selected_iter(panel->vt, &iter)) {
		return;
	}

	vik_treeview_select_iter(panel->vt, &iter, false); /* cancel any layer-name editing going on... */

	if (vik_treeview_item_get_type(panel->vt, &iter) == VIK_TREEVIEW_TYPE_LAYER) {
		VikAggregateLayer * parent = VIK_AGGREGATE_LAYER(vik_treeview_item_get_parent(panel->vt, &iter));
		if (parent) {/* not toplevel */
			vik_aggregate_layer_move_layer(parent, &iter, up);
			vik_layers_panel_emit_update(panel);
		}
	}
}

bool vik_layers_panel_properties(LayersPanel * panel)
{
	GtkTreeIter iter;
	assert (panel->vvp);

	if (vik_treeview_get_selected_iter(panel->vt, &iter) && vik_treeview_item_get_type(panel->vt, &iter) == VIK_TREEVIEW_TYPE_LAYER) {
		if (vik_treeview_item_get_data(panel->vt, &iter) == VIK_LAYER_AGGREGATE) {
			a_dialog_info_msg(VIK_GTK_WINDOW_FROM_WIDGET(panel->gob), _("Aggregate Layers have no settable properties."));
		}
		VikLayer * layer = VIK_LAYER(vik_treeview_item_get_pointer(panel->vt, &iter));
		if (vik_layer_properties(layer, panel->vvp)) {
			vik_layer_emit_update(layer);
		}
		return true;
	} else {
		return false;
	}
}

void vik_layers_panel_draw_all(LayersPanel * panel)
{
	if (panel->vvp && VIK_LAYER(panel->toplayer)->visible) {
		vik_aggregate_layer_draw(panel->toplayer, panel->vvp);
	}
}

void vik_layers_panel_cut_selected(LayersPanel * panel)
{
	GtkTreeIter iter;

	if (! vik_treeview_get_selected_iter(panel->vt, &iter)) {
		/* Nothing to do */
		return;
	}

	int type = vik_treeview_item_get_type(panel->vt, &iter);

	if (type == VIK_TREEVIEW_TYPE_LAYER) {
		VikAggregateLayer *parent = (VikAggregateLayer *) vik_treeview_item_get_parent(panel->vt, &iter);
		if (parent){
			/* reset trigger if trigger deleted */
			if (vik_layers_panel_get_selected(panel) == vik_viewport_get_trigger(panel->vvp)) {
				vik_viewport_set_trigger(panel->vvp, NULL);
			}

			a_clipboard_copy_selected(panel->gob);

			if (IS_VIK_AGGREGATE_LAYER(parent)) {

				g_signal_emit(G_OBJECT(panel->gob), layers_panel_signals[VLP_DELETE_LAYER_SIGNAL], 0);

				if (vik_aggregate_layer_delete(parent, &iter)) {
					vik_layers_panel_emit_update(panel);
				}
			}
		} else {
			a_dialog_info_msg(VIK_GTK_WINDOW_FROM_WIDGET(panel->gob), _("You cannot cut the Top Layer."));
		}
	} else if (type == VIK_TREEVIEW_TYPE_SUBLAYER) {
		VikLayer *sel = vik_layers_panel_get_selected(panel);
		if (vik_layer_get_interface(sel->type)->cut_item) {
			int subtype = vik_treeview_item_get_data(panel->vt, &iter);
			vik_layer_get_interface(sel->type)->cut_item(sel, subtype, vik_treeview_item_get_pointer(sel->vt, &iter));
		}
	}
}

void vik_layers_panel_copy_selected(LayersPanel * panel)
{
	GtkTreeIter iter;
	if (! vik_treeview_get_selected_iter(panel->vt, &iter)) {
		/* Nothing to do */
		return;
	}
	// NB clipboard contains layer vs sublayer logic, so don't need to do it here
	a_clipboard_copy_selected(panel->gob);
}

bool vik_layers_panel_paste_selected(LayersPanel * panel)
{
	GtkTreeIter iter;
	if (! vik_treeview_get_selected_iter(panel->vt, &iter)) {
		/* Nothing to do */
		return false;
	}
	return a_clipboard_paste(panel->gob);
}

void vik_layers_panel_delete_selected(LayersPanel * panel)
{
	GtkTreeIter iter;

	if (! vik_treeview_get_selected_iter(panel->vt, &iter)) {
		/* Nothing to do */
		return;
	}

	int type = vik_treeview_item_get_type(panel->vt, &iter);

	if (type == VIK_TREEVIEW_TYPE_LAYER) {
		// Get confirmation from the user
		if (! a_dialog_yes_or_no(VIK_GTK_WINDOW_FROM_WIDGET(panel->gob),
					 _("Are you sure you want to delete %s?"),
					 vik_layer_get_name(VIK_LAYER(vik_treeview_item_get_pointer(panel->vt, &iter))))) {
			return;
		}

		VikAggregateLayer *parent = (VikAggregateLayer *) vik_treeview_item_get_parent(panel->vt, &iter);
		if (parent) {
			/* reset trigger if trigger deleted */
			if (vik_layers_panel_get_selected(panel) == vik_viewport_get_trigger(panel->vvp)) {
				vik_viewport_set_trigger(panel->vvp, NULL);
			}

			if (IS_VIK_AGGREGATE_LAYER(parent)) {

				g_signal_emit(G_OBJECT(panel->gob), layers_panel_signals[VLP_DELETE_LAYER_SIGNAL], 0);

				if (vik_aggregate_layer_delete(parent, &iter)) {
					vik_layers_panel_emit_update(panel);
				}
			}
		} else {
			a_dialog_info_msg (VIK_GTK_WINDOW_FROM_WIDGET(panel->gob), _("You cannot delete the Top Layer."));
		}
	} else if (type == VIK_TREEVIEW_TYPE_SUBLAYER) {
		VikLayer *sel = vik_layers_panel_get_selected(panel);
		if (vik_layer_get_interface(sel->type)->delete_item) {
			int subtype = vik_treeview_item_get_data(panel->vt, &iter);
			vik_layer_get_interface(sel->type)->delete_item(sel, subtype, vik_treeview_item_get_pointer(sel->vt, &iter));
		}
	}
}

VikLayer *vik_layers_panel_get_selected(LayersPanel * panel)
{
	GtkTreeIter iter, parent;

	if (! vik_treeview_get_selected_iter(panel->vt, &iter)) {
		return NULL;
	}

	int type = vik_treeview_item_get_type(panel->vt, &iter);

	while (type != VIK_TREEVIEW_TYPE_LAYER) {
		if (! vik_treeview_item_get_parent_iter(panel->vt, &iter, &parent)) {
			return NULL;
		}
		iter = parent;
		type = vik_treeview_item_get_type(panel->vt, &iter);
	}

	return VIK_LAYER(vik_treeview_item_get_pointer(panel->vt, &iter));
}

static void layers_move_item_up_cb(VikLayersPanel *vlp)
{
	layers_move_item(vlp->panel_ref, true);
}

static void layers_move_item_down_cb(VikLayersPanel *vlp)
{
	layers_move_item(vlp->panel_ref, false);
}

#if 0
bool vik_layers_panel_tool(VikLayersPanel *vlp, uint16_t layer_type, VikToolInterfaceFunc tool_func, GdkEventButton *event, VikViewport *vvp)
{
	VikLayer *vl = vik_layers_panel_get_selected(vlp);
	if (vl && vl->type == layer_type) {
		tool_func(vl, event, vvp);
		return true;
	} else if (VIK_LAYER(vlp->panel.toplayer)->visible &&
		   vik_aggregate_layer_tool(vlp->panel.toplayer, layer_type, tool_func, event, vvp) != 1) { /* either accepted or rejected, but a layer was found */
		return true;
	}
	return false;
}
#endif

VikLayer *vik_layers_panel_get_layer_of_type(LayersPanel * panel, VikLayerTypeEnum type)
{
	VikLayer *rv = vik_layers_panel_get_selected(panel);
	if (rv == NULL || rv->type != type) {
		if (VIK_LAYER(panel->toplayer)->visible) {
			return vik_aggregate_layer_get_top_visible_layer_of_type(panel->toplayer, type);
		} else {
			return NULL;
		}
	} else {
		return rv;
	}
}

GList *vik_layers_panel_get_all_layers_of_type(LayersPanel * panel, int type, bool include_invisible)
{
	GList *layers = NULL;

	return (vik_aggregate_layer_get_all_layers_of_type(panel->toplayer, layers, (VikLayerTypeEnum) type, include_invisible));
}

VikAggregateLayer *vik_layers_panel_get_top_layer(LayersPanel * panel)
{
	return panel->toplayer;
}

/**
 * Remove all layers
 */
void vik_layers_panel_clear(LayersPanel * panel)
{
	if (! vik_aggregate_layer_is_empty(panel->toplayer)) {
		g_signal_emit(G_OBJECT(panel->gob), layers_panel_signals[VLP_DELETE_LAYER_SIGNAL], 0);
		vik_aggregate_layer_clear(panel->toplayer); /* simply deletes all layers */
	}
}

void vik_layers_panel_change_coord_mode(LayersPanel * panel, VikCoordMode mode)
{
	vik_layer_change_coord_mode(VIK_LAYER(panel->toplayer), mode);
}

static void layers_panel_finalize(GObject *gob)
{
	VikLayersPanel *vlp = VIK_LAYERS_PANEL (gob);
	g_object_unref(VIK_LAYER(vlp->panel.toplayer));
	G_OBJECT_CLASS(parent_class)->finalize(gob);
}

VikTreeview *vik_layers_panel_get_treeview(LayersPanel * panel)
{
	return panel->vt;
}
