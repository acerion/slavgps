/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
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
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <cassert>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "viking.h"
#include "viktrwlayer_waypointlist.h"
#include "waypoint_properties.h"
#include "clipboard.h"
#include "dialog.h"
#include "settings.h"
#include "globals.h"
#include "window.h"




using namespace SlavGPS;




// Long formatted date+basic time - listing this way ensures the string comparison sort works - so no local type format %x or %c here!
#define WAYPOINT_LIST_DATE_FORMAT "%Y-%m-%d %H:%M"




/**
 * waypoint_close_cb:
 *
 */
static void waypoint_close_cb(GtkWidget * dialog, int resp, std::list<waypoint_layer_t *> * waypoints_and_layers)
{
#ifdef K
	/* kamilTODO: free waypoints_and_layers */

	gtk_widget_destroy(dialog);
#endif
}




/**
 * format_1f_cell_data_func:
 *
 * General purpose column double formatting
 *
static void format_1f_cell_data_func(GtkTreeViewColumn * col,
                                     GtkCellRenderer   * renderer,
                                     GtkTreeModel      * model,
                                     GtkTreeIter       * iter,
                                     void              * user_data)
{
	double value;
	char buf[20];
	int column = KPOINTER_TO_INT (user_data);
	gtk_tree_model_get (model, iter, column, &value, -1);
	snprintf(buf, sizeof(buf), "%.1f", value);
	g_object_set (renderer, "text", buf, NULL);
}
 */




#define WPT_LIST_COLS 9
#define WPT_COL_NUM WPT_LIST_COLS-1
#define TRW_COL_NUM WPT_COL_NUM-1




/*
 * trw_layer_waypoint_tooltip_cb:
 *
 * Show a tooltip when the mouse is over a waypoint list entry.
 * The tooltip contains the description.
 */
static bool trw_layer_waypoint_tooltip_cb(GtkWidget  * widget,
					  int          x,
					  int          y,
					  bool         keyboard_tip,
					  GtkTooltip * tooltip,
					  void       * data)
{
#ifdef K
	GtkTreeIter iter;
	GtkTreePath * path = NULL;
	GtkTreeView * tree_view = GTK_TREE_VIEW (widget);
	GtkTreeModel * model = gtk_tree_view_get_model(tree_view);

	if (!gtk_tree_view_get_tooltip_context(tree_view, &x, &y,
					       keyboard_tip,
					       &model, &path, &iter)) {

		return false;
	}

	Waypoint * wp;
	gtk_tree_model_get(model, &iter, WPT_COL_NUM, &wp, -1);
	if (!wp) {
		return false;
	}

	bool tooltip_set = true;
	if (wp->description) {
		gtk_tooltip_set_text(tooltip, wp->description);
	} else {
		tooltip_set = false;
	}

	if (tooltip_set) {
		gtk_tree_view_set_tooltip_row(tree_view, tooltip, path);
	}

	gtk_tree_path_free(path);

	return tooltip_set;
#endif
}




/*
static void trw_layer_waypoint_select_cb(GtkTreeSelection * selection, void * data)
{
	GtkTreeIter iter;
	if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		return;
	}

	GtkTreeView *tree_view = GTK_TREE_VIEW (data);
	GtkTreeModel *model = gtk_tree_view_get_model(tree_view);

	Waypoint *wp;
	gtk_tree_model_get(model, &iter, WPT_COL_NUM, &wp, -1);
	if (!wp) {
		return;
	}

	LayerTRW * trw;
	gtk_tree_model_get(model, &iter, TRW_COL_NUM, &trw-, -1);
	if (trw->type != LayerTRW::TRW) {
		return;
	}

	//vik_treeview_select_iter(trw->vt, g_hash_table_lookup(trw->waypoint_iters, uuid), true);
}
*/




typedef struct {
	LayerTRW * trw;
	Waypoint * waypoint;
	sg_uid_t waypoint_uid;
	Viewport * viewport;
	GtkWidget * gtk_tree_view;
	std::list<waypoint_layer_t *> * waypoints_and_layers;
} waypointlist_data_t;




// Instead of hooking automatically on treeview item selection
// This is performed on demand via the specific menu request
static void trw_layer_waypoint_select(waypointlist_data_t * values)
{
#ifdef K
	LayerTRW * trw = values->trw;

	if (values->waypoint_uid) {
		sg_uid_t uid = values->waypoint_uid;
		GtkTreeIter * iter = trw->get_waypoints_iters().at(uid);

		if (iter) {
			trw->tree_view->select_and_expose(iter);
		}
	}
#endif
}




static void trw_layer_waypoint_properties(waypointlist_data_t * values)
{
#ifdef K
	LayerTRW * trw = values->trw;
	Waypoint * wp = values->waypoint;

	if (wp && wp->name) {
		// Kill off this dialog to allow interaction with properties window
		//  since the properties also allows waypoint manipulations it won't cause conflicts here.
		GtkWidget * gw = gtk_widget_get_toplevel(values->gtk_tree_view);
		waypoint_close_cb(gw, 0, values->waypoints_and_layers);

		bool updated = false;
		char * new_name = waypoint_properties_dialog(trw->get_toolkit_window(), wp->name, trw, wp, trw->get_coord_mode(), false, &updated);
		if (new_name) {
			trw->waypoint_rename(wp, new_name);
		}

		if (updated) {
			trw->waypoint_reset_icon(wp);
		}

		if (updated && trw->visible) {
			trw->emit_changed();
		}
	}
#endif
}




static void trw_layer_waypoint_view(waypointlist_data_t * values)
{
	LayerTRW * trw = values->trw;
	Waypoint * wp = values->waypoint;
	Viewport * viewport = values->viewport;

	viewport->set_center_coord(&(wp->coord), true);

	trw_layer_waypoint_select(values);

	trw->emit_changed();
}




static void trw_layer_show_picture_wp(waypointlist_data_t * values)
{
#ifdef K
	Waypoint * wp = values->waypoint;
#ifdef WINDOWS
	ShellExecute(NULL, "open", wp->image, NULL, NULL, SW_SHOWNORMAL);
#else
	LayerTRW * trw = values->trw;
	GError * err = NULL;
	char * quoted_file = g_shell_quote(wp->image);
	char * cmd = g_strdup_printf("%s %s", a_vik_get_image_viewer(), quoted_file);
	free(quoted_file);
	if (! g_spawn_command_line_async(cmd, &err)) {
		a_dialog_error_msg_extra(trw->get_toolkit_window(), _("Could not launch %s to open file."), a_vik_get_image_viewer());
		g_error_free(err);
	}
	free(cmd);
#endif
#endif
}




typedef struct {
	bool has_layer_names;
	bool include_positions;
	GString * str;
} copy_data_t;




/**
 * At the moment allow copying the data displayed** with or without the positions
 *  (since the position data is not shown in the list but is useful in copying to external apps)
 *
 * ** ATM The visibility flag is not copied and neither is a text representation of the waypoint symbol
 */
static void copy_selection(GtkTreeModel * model,
			   GtkTreePath  * path,
			   GtkTreeIter  * iter,
			   void         * data)
{
#ifdef K
	copy_data_t * cd = (copy_data_t *) data;

	char * layername;
	gtk_tree_model_get(model, iter, 0, &layername, -1);

	char * name;
	gtk_tree_model_get(model, iter, 1, &name, -1);

	char * date;
	gtk_tree_model_get(model, iter, 2, &date, -1);

	char * comment;
	gtk_tree_model_get(model, iter, 4, &comment, -1);
	if (comment == NULL) {
		comment = strdup("");
	}

	int hh;
	gtk_tree_model_get(model, iter, 5, &hh, -1);

	Waypoint * wp;
        gtk_tree_model_get(model, iter, WPT_COL_NUM, &wp, -1);
	struct LatLon ll;
	if (wp) {
		vik_coord_to_latlon(&wp->coord, &ll);
	}
	char sep = '\t'; // Could make this configurable - but simply always make it a tab character for now
	// NB Even if the columns have been reordered - this copies it out only in the original default order
	// if col 0 is displayed then also copy the layername
	// Note that the lat/lon data copy is using the users locale
	if (cd->has_layer_names) {
		if (cd->include_positions) {
			g_string_append_printf(cd->str, "%s%c%s%c%s%c%s%c%d%c%.6f%c%.6f\n", layername, sep, name, sep, date, sep, comment, sep, hh, sep, ll.lat, sep, ll.lon);
		} else {
			g_string_append_printf(cd->str, "%s%c%s%c%s%c%s%c%d\n", layername, sep, name, sep, date, sep, comment, sep, hh);
		}
	} else {
		if (cd->include_positions) {
			g_string_append_printf(cd->str, "%s%c%s%c%s%c%d%c%.6f%c%.6f\n", name, sep, date, sep, comment, sep, hh, sep, ll.lat, sep, ll.lon);
		} else {
			g_string_append_printf(cd->str, "%s%c%s%c%s%c%d\n", name, sep, date, sep, comment, sep, hh);
		}
	}
	free(layername);
	free(name);
	free(date);
	free(comment);
#endif
}




static void trw_layer_copy_selected(GtkWidget * tree_view, bool include_positions)
{
#ifdef K
	GtkTreeSelection * selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
	// NB GTK3 has gtk_tree_view_get_n_columns() but we're GTK2 ATM
	GList * gl = gtk_tree_view_get_columns(GTK_TREE_VIEW(tree_view));
	unsigned int count = g_list_length(gl);
	g_list_free(gl);
	copy_data_t cd;
	cd.has_layer_names = (count > WPT_LIST_COLS-3);
	cd.str = g_string_new(NULL);
	cd.include_positions = include_positions;
	gtk_tree_selection_selected_foreach(selection, copy_selection, &cd);

	a_clipboard_copy(VIK_CLIPBOARD_DATA_TEXT, LayerType::AGGREGATE, SublayerType::NONE, 0, cd.str->str, NULL);

	g_string_free(cd.str, true);
#endif
}




static void trw_layer_copy_selected_only_visible_columns(GtkWidget * tree_view)
{
#ifdef K
	trw_layer_copy_selected(tree_view, false);
#endif
}




static void trw_layer_copy_selected_with_position(GtkWidget * tree_view)
{
#ifdef K
	trw_layer_copy_selected(tree_view, true);
#endif
}




static void add_copy_menu_items(GtkMenu * menu, GtkWidget * tree_view)
{
#ifdef K
	GtkWidget * item = gtk_image_menu_item_new_with_mnemonic(_("_Copy Data"));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_COPY, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(trw_layer_copy_selected_only_visible_columns), tree_view);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	gtk_widget_show(item);

	item = gtk_image_menu_item_new_with_mnemonic(_("Copy Data (with _positions)"));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_COPY, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(trw_layer_copy_selected_with_position), tree_view);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	gtk_widget_show(item);
#endif
}




static bool add_menu_items(QMenu & menu, LayerTRW * trw, Waypoint * wp, sg_uid_t wp_uid, Viewport * viewport, GtkWidget * gtk_tree_view, std::list<waypoint_layer_t *> * waypoints_and_layers)
{
#ifdef K
	GtkWidget * item;

	static waypointlist_data_t values;
	values.trw                  = trw;
	values.waypoint             = wp;
	values.waypoint_uid         = wp_uid;
	values.viewport             = viewport;
	values.gtk_tree_view        = gtk_tree_view;
	values.waypoints_and_layers = waypoints_and_layers;

	/*
	item = gtk_image_menu_item_new_with_mnemonic(_("_Select"));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_FIND, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(trw_layer_waypoint_select), &values);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	gtk_widget_show(item);
	*/

	// AUTO SELECT NOT true YET...
	// ATM view auto selects, so don't bother with separate select menu entry
	item = gtk_image_menu_item_new_with_mnemonic(_("_View"));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(trw_layer_waypoint_view), &values);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	gtk_widget_show(item);

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_PROPERTIES, NULL);
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(trw_layer_waypoint_properties), &values);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	item = gtk_image_menu_item_new_with_mnemonic(_("_Show Picture..."));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock ("vik-icon-Show Picture", GTK_ICON_SIZE_MENU)); // Own icon - see stock_icons in vikwindow.c
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(trw_layer_show_picture_wp), &values);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);
	gtk_widget_set_sensitive(item, KPOINTER_TO_INT(wp->image));

	add_copy_menu_items(menu, gtk_tree_view);
#endif

	return true;
}




static bool trw_layer_waypoint_menu_popup_multi(GtkWidget * tree_view,
						GdkEventButton * event,
						void * waypoints_and_layers)
{
#ifdef K
	GtkWidget * menu = gtk_menu_new();

	add_copy_menu_items(GTK_MENU(menu), tree_view);

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, gtk_get_current_event_time());
#endif

	return true;
}




static bool trw_layer_waypoint_menu_popup(GtkWidget * tree_view,
					  GdkEventButton * event,
					  void * waypoints_and_layers)
{
#ifdef K
	static GtkTreeIter iter;

	// Use selected item to get a single iterator ref
	// This relies on an row being selected as part of the right click
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
	if (gtk_tree_selection_count_selected_rows(selection) != 1) {
		return trw_layer_waypoint_menu_popup_multi(tree_view, event, waypoints_and_layers);
	}

	GtkTreePath * path;
	GtkTreeModel * model = gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view));

	// All this just to get the iter
	if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tree_view),
					  (int) event->x,
					  (int) event->y,
					  &path, NULL, NULL, NULL)) {
		gtk_tree_model_get_iter_from_string(model, &iter, gtk_tree_path_to_string (path));
		gtk_tree_path_free(path);
	} else {
		return false;
	}

	Waypoint * wp;
	gtk_tree_model_get(model, &iter, WPT_COL_NUM, &wp, -1);
	if (!wp) {
		return false;
	}

	LayerTRW * trw;
	gtk_tree_model_get(model, &iter, TRW_COL_NUM, &trw, -1);
	if (trw->type != LayerType::TRW) {
		return false;
	}

	sg_uid_t wp_uuid = LayerTRWc::find_uid_of_waypoint(trw->get_waypoints(), wp);
	if (wp_uuid) {
		Viewport * viewport = trw->get_window()->get_viewport();

		GtkWidget * menu = gtk_menu_new();

		// Originally started to reuse the trw_layer menu items
		//  however these offer too many ways to edit the waypoint data
		//  so without an easy way to distinguish read only operations,
		//  create a very minimal new set of operations
		add_menu_items(GTK_MENU(menu),
			       trw,
			       wp,
			       wp_uuid,
			       viewport,
			       tree_view,
			       (std::list<SlavGPS::waypoint_layer_t*> *) waypoints_and_layers);

		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, gtk_get_current_event_time());
		return true;
	}
#endif
	return false;
}




static bool trw_layer_waypoint_button_pressed(GtkWidget * tree_view,
					      GdkEventButton * event,
					      void * waypoints_and_layers)
{
#ifdef K
	// Only on right clicks...
	if (! (event->type == GDK_BUTTON_PRESS && event->button == MouseButton::RIGHT)) {
		return false;
	}

	// ATM Force a selection...
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
	if (gtk_tree_selection_count_selected_rows(selection) <= 1) {
		GtkTreePath *path;
		/* Get tree path for row that was clicked */
		if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tree_view),
						  (int) event->x,
						  (int) event->y,
						  &path, NULL, NULL, NULL)) {

			gtk_tree_selection_unselect_all(selection);
			gtk_tree_selection_select_path(selection, path);
			gtk_tree_path_free(path);
		}
	}
	return trw_layer_waypoint_menu_popup(tree_view, event, waypoints_and_layers);
#endif
}




/*
 * Foreach entry we copy the various individual waypoint properties into the tree store
 *  formatting & converting the internal values into something for display
 */
static void trw_layer_waypoint_list_add(waypoint_layer_t * element,
					GtkTreeStore * store,
					HeightUnit height_units,
					const char * date_format)
{
#ifdef K
	GtkTreeIter t_iter;
	Waypoint * wp = element->wp;
	LayerTRW * trw = element->trw;

	// Get start date
	char time_buf[32];
	time_buf[0] = '\0';
	if (wp->has_timestamp) {

#if GLIB_CHECK_VERSION(2,26,0)
		GDateTime * gdt = g_date_time_new_from_unix_utc(wp->timestamp);
		char * time = g_date_time_format(gdt, date_format);
		g_strlcpy(time_buf, time, sizeof(time_buf));
		free(time);
		g_date_time_unref(gdt);
#else
		GDate * gdate_start = g_date_new();
		g_date_set_time_t(gdate_start, wp->timestamp);
		g_date_strftime(time_buf, sizeof(time_buf), date_format, gdate_start);
		g_date_free(gdate_start);
#endif
	}

	// NB: doesn't include aggegrate visibility
	bool visible = trw->visible && wp->visible;
	visible = visible && trw->get_waypoints_visibility();

	double alt = wp->altitude;
	switch (height_units) {
	case HeightUnit::FEET:
		alt = VIK_METERS_TO_FEET(alt);
		break;
	default:
		// HeightUnit::METRES: no need to convert
		break;
	}

	gtk_tree_store_append(store, &t_iter, NULL);
	gtk_tree_store_set(store, &t_iter,
			   0, trw->name,
			   1, wp->name,
			   2, time_buf,
			   3, visible,
			   4, wp->comment,
			   5, (int) round(alt),
			   6, get_wp_sym_small(wp->symbol),
			   TRW_COL_NUM, trw,
			   WPT_COL_NUM, wp,
			   -1);
#endif
}




/*
 * Instead of comparing the pixbufs,
 *  look at the waypoint data and compare the symbol(as text).
 */
int sort_pixbuf_compare_func(GtkTreeModel * model,
			     GtkTreeIter  * a,
			     GtkTreeIter  * b,
			     void         * userdata)
{
#ifdef K
	Waypoint * wp1, * wp2;
	gtk_tree_model_get(model, a, WPT_COL_NUM, &wp1, -1);
	if (!wp1) {
		return 0;
	}
	gtk_tree_model_get(model, b, WPT_COL_NUM, &wp2, -1);
	if (!wp2) {
		return 0;
	}

	return g_strcmp0(wp1->symbol, wp2->symbol);
#endif
}




static GtkTreeViewColumn * my_new_column_text(const char * title, GtkCellRenderer * renderer, GtkWidget * view, int column_runner)
{
#ifdef K
	GtkTreeViewColumn * column = gtk_tree_view_column_new_with_attributes(title, renderer, "text", column_runner, NULL);
	gtk_tree_view_column_set_sort_column_id(column, column_runner);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
	gtk_tree_view_column_set_reorderable(column, true);
	gtk_tree_view_column_set_resizable(column, true);
	return column;
#endif
}




/**
 * vik_trw_layer_waypoint_list_internal:
 * @dialog:            The dialog to create the widgets in
 * @waypoints_and_layers: The list of waypoints (and it's layer) to be shown
 * @show_layer_names:  Show the layer names that each waypoint belongs to
 *
 * Create a table of waypoints with corresponding waypoint information
 * This table does not support being actively updated
 */
static void vik_trw_layer_waypoint_list_internal(GtkWidget * dialog,
						 std::list<waypoint_layer_t *> * waypoints_and_layers,
						 bool show_layer_names)
{
#ifdef K
	if (!waypoints_and_layers || waypoints_and_layers->empty()) {
		return;
	}

	// It's simple storing the double values in the tree store as the sort works automatically
	// Then apply specific cell data formatting (rather default double is to 6 decimal places!)
	// However not storing any doubles for waypoints ATM
	// TODO: Consider adding the waypoint icon into this store for display in the list
	GtkTreeStore * store = gtk_tree_store_new(WPT_LIST_COLS,
						  G_TYPE_STRING,    // 0: Layer Name
						  G_TYPE_STRING,    // 1: Waypoint Name
						  G_TYPE_STRING,    // 2: Date
						  G_TYPE_BOOLEAN,   // 3: Visible
						  G_TYPE_STRING,    // 4: Comment
						  G_TYPE_INT,       // 5: Height
						  GDK_TYPE_PIXBUF,  // 6: Symbol Icon
						  G_TYPE_POINTER,   // 7: TrackWaypoint Layer pointer
						  G_TYPE_POINTER);  // 8: Waypoint pointer

	//gtk_tree_selection_set_select_function(gtk_tree_view_get_selection (GTK_TREE_VIEW(vt)), vik_treeview_selection_filter, vt, NULL);

	HeightUnit height_units = a_vik_get_units_height();

	//GList *gl = create_waypoints_and_layers_cb(vl, user_data);
	//g_list_foreach(waypoints_and_layers, (GFunc) trw_layer_waypoint_list_add, store);
	char *date_format = NULL;
	if (!a_settings_get_string(VIK_SETTINGS_LIST_DATE_FORMAT, &date_format)) {
		date_format = g_strdup(WAYPOINT_LIST_DATE_FORMAT);
	}

	for (auto iter = waypoints_and_layers->begin(); iter != waypoints_and_layers->end(); iter++) {
		trw_layer_waypoint_list_add(*iter, store, height_units, date_format);
	}
	free(date_format);

	GtkWidget * view = gtk_tree_view_new();
	GtkCellRenderer * renderer = gtk_cell_renderer_text_new();
	g_object_set(G_OBJECT (renderer), "xalign", 0.0, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	GtkTreeViewColumn * column;
	GtkTreeViewColumn * sort_by_column;

	int column_runner = 0;
	if (show_layer_names) {
		// Insert column for the layer name when viewing multi layers
		column = my_new_column_text(_("Layer"), renderer, view, column_runner++);
		g_object_set(G_OBJECT (renderer), "xalign", 0.0, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
		gtk_tree_view_column_set_expand(column, true);
		// remember the layer column so we can sort by it later
		sort_by_column = column;
	} else {
		column_runner++;
	}

	column = my_new_column_text(_("Name"), renderer, view, column_runner++);
	gtk_tree_view_column_set_expand(column, true);
	if (!show_layer_names) {
		// remember the name column so we can sort by it later
		sort_by_column = column;
	}

	column = my_new_column_text(_("Date"), renderer, view, column_runner++);
	gtk_tree_view_column_set_resizable(column, true);

	GtkCellRenderer *renderer_toggle = gtk_cell_renderer_toggle_new();
	column = gtk_tree_view_column_new_with_attributes(_("Visible"), renderer_toggle, "active", column_runner, NULL);
	gtk_tree_view_column_set_sort_column_id(column, column_runner);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
	column_runner++;

	column = my_new_column_text(_("Comment"), renderer, view, column_runner++);
	gtk_tree_view_column_set_expand(column, true);

	if (height_units == HeightUnit::FEET) {
		(void) my_new_column_text(_("Max Height\n(Feet)"), renderer, view, column_runner++);
	} else {
		(void) my_new_column_text(_("Max Height\n(Metres)"), renderer, view, column_runner++);
	}

	GtkCellRenderer *renderer_pixbuf = gtk_cell_renderer_pixbuf_new();
	g_object_set(G_OBJECT (renderer_pixbuf), "xalign", 0.5, NULL);
	column = gtk_tree_view_column_new_with_attributes(_("Symbol"), renderer_pixbuf, "pixbuf", column_runner++, NULL);
	// Special sort required for pixbufs
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store), column_runner, sort_pixbuf_compare_func, NULL, NULL);
	gtk_tree_view_column_set_sort_column_id(column, column_runner);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

	gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(store));
	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), GTK_SELECTION_MULTIPLE);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(view), true);

	g_object_unref(store);

	GtkWidget * scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolledwindow), view);

	g_object_set(view, "has-tooltip", true, NULL);

	g_signal_connect(view, "query-tooltip", G_CALLBACK (trw_layer_waypoint_tooltip_cb), NULL);
	//g_signal_connect(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), "changed", G_CALLBACK(trw_layer_waypoint_select_cb), view);

	g_signal_connect(view, "popup-menu", G_CALLBACK(trw_layer_waypoint_menu_popup), waypoints_and_layers);
	g_signal_connect(view, "button-press-event", G_CALLBACK(trw_layer_waypoint_button_pressed), waypoints_and_layers);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), scrolledwindow, true, true, 0);

	// Set ordering of the initial view by one of the name columns
	gtk_tree_view_column_clicked(sort_by_column);

	// Ensure a reasonable number of items are shown
	//  TODO: may be save window size, column order, sorted by between invocations.
	gtk_window_set_default_size(GTK_WINDOW(dialog), show_layer_names ? 700 : 500, 400);
#endif
}




/**
 * vik_trw_layer_waypoint_list_show_dialog:
 * @title:                    The title for the dialog
 * @vl:                       The #VikLayer passed on into create_waypoints_and_layers_cb()
 * @show_layer_names:         Normally only set when called from an aggregate level
 *
 * Common method for showing a list of waypoints with extended information
 *
 */
void SlavGPS::vik_trw_layer_waypoint_list_show_dialog(char * title,
						      Layer * layer,
						      bool show_layer_names)
{
#ifdef K
	GtkWidget * dialog = gtk_dialog_new_with_buttons(title,
							 layer->get_toolkit_window(),
							 GTK_DIALOG_DESTROY_WITH_PARENT,
							 GTK_STOCK_CLOSE,
							 GTK_RESPONSE_CLOSE,
							 NULL);

	std::list<waypoint_layer_t *> * waypoints_and_layers = NULL;
	if (layer->type == LayerType::TRW) {
		waypoints_and_layers = ((LayerTRW *) layer)->create_waypoints_and_layers_list();
	} else if (layer->type == LayerType::AGGREGATE) {
		waypoints_and_layers = ((LayerAggregate *) layer)->create_waypoints_and_layers_list();
	} else {
		assert (0);
	}

	vik_trw_layer_waypoint_list_internal(dialog, waypoints_and_layers, show_layer_names);

	// Use response to close the dialog with tidy up
	g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(waypoint_close_cb), waypoints_and_layers);

	gtk_widget_show_all(dialog);
	// Yes - set the size *AGAIN* - this time widgets are expanded nicely
	gtk_window_resize(GTK_WINDOW(dialog), show_layer_names ? 800 : 600, 400);

	// ATM lock out on dialog run - to prevent list contents being manipulated in other parts of the GUI whilst shown here.
	gtk_dialog_run(GTK_DIALOG (dialog));
	// Unfortunately seems subsequently opening the Waypoint Properties we can't interact with it until this dialog is closed
	// Thus this dialog is then forcibly closed when opening the properties.

	// Occassionally the 'View' doesn't update the viewport properly
	//  viewport center + zoom is changed but the viewport isn't updated
	// not sure why yet..
#endif
}