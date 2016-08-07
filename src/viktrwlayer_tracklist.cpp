/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "viking.h"
#include "viktrwlayer_tracklist.h"
#include "viktrwlayer_propwin.h"
#include "clipboard.h"
#include "settings.h"
#include "globals.h"
#include "vikwindow.h"


using namespace SlavGPS;


// Long formatted date+basic time - listing this way ensures the string comparison sort works - so no local type format %x or %c here!
#define TRACK_LIST_DATE_FORMAT "%Y-%m-%d %H:%M"

/**
 * track_close_cb:
 *
 */
static void track_close_cb(GtkWidget * dialog, int resp, std::list<track_layer_t *> * tracks_and_layers)
{
	/* kamilTODO: should we delete tracks_and_layers here? */

	gtk_widget_destroy(dialog);
}

/**
 * format_1f_cell_data_func:
 *
 * General purpose column double formatting
 *
 */
static void format_1f_cell_data_func(GtkTreeViewColumn * col,
                                       GtkCellRenderer * renderer,
                                       GtkTreeModel    * model,
                                       GtkTreeIter     * iter,
                                       void            * user_data)
{
	double value;
	char buf[20];
	int column = KPOINTER_TO_INT (user_data);
	gtk_tree_model_get(model, iter, column, &value, -1);
	snprintf(buf, sizeof(buf), "%.1f", value);
	g_object_set(renderer, "text", buf, NULL);
}

#define TRK_LIST_COLS 11
#define TRK_COL_NUM TRK_LIST_COLS-1
#define TRW_COL_NUM TRK_COL_NUM-1

/*
 * trw_layer_track_tooltip_cb:
 *
 * Show a tooltip when the mouse is over a track list entry.
 * The tooltip contains the comment or description.
 */
static bool trw_layer_track_tooltip_cb(GtkWidget        * widget,
                                             int          x,
                                             int          y,
                                             bool         keyboard_tip,
                                             GtkTooltip * tooltip,
                                             void       * data)
{
	GtkTreeIter iter;
	GtkTreePath * path = NULL;
	GtkTreeView * tree_view = GTK_TREE_VIEW (widget);
	GtkTreeModel * model = gtk_tree_view_get_model(tree_view);

	if (!gtk_tree_view_get_tooltip_context(tree_view, &x, &y,
					       keyboard_tip,
					       &model, &path, &iter)) {
		return false;
	}

	Track * trk;
	gtk_tree_model_get(model, &iter, TRK_COL_NUM, &trk, -1);
	if (!trk) {
		return false;
	}

	bool tooltip_set = true;
	if (trk->comment) {
		gtk_tooltip_set_text(tooltip, trk->comment);
	} else if (trk->description) {
		gtk_tooltip_set_text(tooltip, trk->description);
	} else {
		tooltip_set = false;
	}

	if (tooltip_set) {
		gtk_tree_view_set_tooltip_row(tree_view, tooltip, path);
	}

	gtk_tree_path_free(path);

	return tooltip_set;
}

/*
static void trw_layer_track_select_cb(GtkTreeSelection * selection, void * data)
{
	GtkTreeIter iter;
	if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		return;
	}

	GtkTreeView * tree_view = GTK_TREE_VIEW (data);
	GtkTreeModel * model = gtk_tree_view_get_model(tree_view);

	Track * trk;
	gtk_tree_model_get(model, &iter, TRK_COL_NUM, &trk, -1);
	if (!trk) {
		return;
	}

	LayerTRW * trw;
	gtk_tree_model_get(model, &iter, TRW_COL_NUM, &trw->vl, -1);
	if (!IS_VIK_TRW_LAYER(trw->vl)) {
		return;
	}

	//vik_treeview_select_iter(trw->vt, g_hash_table_lookup(trw->track_iters, uuid), true);
}
*/


typedef struct {
	LayerTRW * trw;
	Track * track;
	sg_uid_t track_uid;
	Viewport * viewport;
	GtkWidget * gtk_tree_view;
	std::list<track_layer_t *> * tracks_and_layers;
} tracklist_data_t;


// Instead of hooking automatically on treeview item selection
// This is performed on demand via the specific menu request
static void trw_layer_track_select(tracklist_data_t * values)
{
	LayerTRW * trw = values->trw;
	Track * trk = values->track;
	sg_uid_t uid = values->track_uid;

	if (uid) {
		GtkTreeIter *iter = NULL;
		if (trk->is_route) {
			iter = trw->get_routes_iters().at(uid);
		} else {
			iter = trw->get_tracks_iters().at(uid);
		}

		if (iter) {
			trw->tree_view->select_and_expose(iter);
		}
	}
}

static void trw_layer_track_stats_cb(tracklist_data_t * values)
{
	LayerTRW * trw = values->trw;
	Track * trk = values->track;
	Viewport * viewport = values->viewport;

	if (trk && trk->name) {
		// Kill off this dialog to allow interaction with properties window
		//  since the properties also allows track manipulations it won't cause conflicts here.
		GtkWidget * gw = gtk_widget_get_toplevel(values->gtk_tree_view);
		track_close_cb(gw, 0, values->tracks_and_layers);

		vik_trw_layer_propwin_run(gtk_window_from_layer(trw),
					  trw,
					  trk,
					  NULL, // panel
					  viewport,
					  true);
	}
}

static void trw_layer_track_view_cb(tracklist_data_t * values)
{
	LayerTRW * trw = values->trw;
	Track * trk = values->track;
	Viewport * viewport = values->viewport;

	// TODO create common function to convert between LatLon[2] and LatLonBBox or even change LatLonBBox to be 2 LatLons!
	struct LatLon maxmin[2];
	maxmin[0].lat = trk->bbox.north;
	maxmin[1].lat = trk->bbox.south;
	maxmin[0].lon = trk->bbox.east;
	maxmin[1].lon = trk->bbox.west;

	trw->zoom_to_show_latlons(viewport, maxmin);

	trw_layer_track_select(values);
}

typedef struct {
	bool has_layer_names;
	GString * str;
} copy_data_t;

static void copy_selection(GtkTreeModel * model,
			   GtkTreePath  * path,
			   GtkTreeIter  * iter,
			   void         * data)
{
	copy_data_t * cd = (copy_data_t *) data;

	char * layername;
	gtk_tree_model_get(model, iter, 0, &layername, -1);

	char * name;
	gtk_tree_model_get(model, iter, 1, &name, -1);

	char * date;
	gtk_tree_model_get(model, iter, 2, &date, -1);

	double d1;
	gtk_tree_model_get(model, iter, 4, &d1, -1);

	unsigned int d2;
	gtk_tree_model_get(model, iter, 5, &d2, -1);

	double d3;
	gtk_tree_model_get(model, iter, 6, &d3, -1);

	double d4;
	gtk_tree_model_get(model, iter, 7, &d4, -1);

	int d5;
	gtk_tree_model_get(model, iter, 8, &d5, -1);

	char sep = '\t'; // Could make this configurable - but simply always make it a tab character for now
	// NB Even if the columns have been reordered - this copies it out only in the original default order
	// if col 0 is displayed then also copy the layername
	if (cd->has_layer_names) {
		g_string_append_printf(cd->str, "%s%c%s%c%s%c%.1f%c%d%c%.1f%c%.1f%c%d\n", layername, sep, name, sep, date, sep, d1, sep, d2, sep, d3, sep, d4, sep, d5);
	} else {
		g_string_append_printf(cd->str, "%s%c%s%c%.1f%c%d%c%.1f%c%.1f%c%d\n", name, sep, date, sep, d1, sep, d2, sep, d3, sep, d4, sep, d5);
	}
	free(layername);
	free(name);
	free(date);
}

static void trw_layer_copy_selected(GtkWidget * tree_view)
{
	GtkTreeSelection * selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
	// NB GTK3 has gtk_tree_view_get_n_columns() but we're GTK2 ATM
	GList * gl = gtk_tree_view_get_columns(GTK_TREE_VIEW(tree_view));
	unsigned int count = g_list_length(gl);
	g_list_free(gl);
	copy_data_t cd;
	cd.has_layer_names = (count > TRK_LIST_COLS-3);
	// Or use gtk_tree_view_column_get_visible()?
	cd.str = g_string_new(NULL);
	gtk_tree_selection_selected_foreach(selection, copy_selection, &cd);

	a_clipboard_copy(VIK_CLIPBOARD_DATA_TEXT, LayerType::AGGREGATE, SublayerType::NONE, 0, cd.str->str, NULL);

	g_string_free(cd.str, true);
}

static void add_copy_menu_item(GtkMenu * menu, GtkWidget * tree_view)
{
	GtkWidget * item = gtk_image_menu_item_new_with_mnemonic(_("_Copy Data"));
	gtk_image_menu_item_set_image((GtkImageMenuItem *) item, gtk_image_new_from_stock(GTK_STOCK_COPY, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(trw_layer_copy_selected), tree_view);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	gtk_widget_show(item);
}

static bool add_menu_items(GtkMenu * menu, LayerTRW * trw, Track * trk, sg_uid_t track_uid, Viewport * viewport, GtkWidget * gtk_tree_view, std::list<track_layer_t *> * tracks_and_layers)
{
	static tracklist_data_t values;
	GtkWidget *item;

	values.trw               = trw;
	values.track             = trk;
	values.track_uid         = track_uid;
	values.viewport          = viewport;
	values.gtk_tree_view     = gtk_tree_view;
	values.tracks_and_layers = tracks_and_layers;

	/*
	item = gtk_image_menu_item_new_with_mnemonic(_("_Select"));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_FIND, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(trw_layer_track_select), values);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	gtk_widget_show(item);
	*/

	// ATM view auto selects, so don't bother with separate select menu entry
	item = gtk_image_menu_item_new_with_mnemonic(_("_View"));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK (trw_layer_track_view_cb), &values);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	gtk_widget_show(item);

	item = gtk_menu_item_new_with_mnemonic(_("_Statistics"));
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK (trw_layer_track_stats_cb), &values);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	add_copy_menu_item(menu, gtk_tree_view);

	return true;
}

static bool trw_layer_track_menu_popup_multi(GtkWidget * gtk_tree_view,
					     GdkEventButton * event,
					     void * data)
{
	GtkWidget * menu = gtk_menu_new();

	add_copy_menu_item(GTK_MENU(menu), gtk_tree_view);

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, gtk_get_current_event_time());

	return true;
}

static bool trw_layer_track_menu_popup(GtkWidget * tree_view,
				       GdkEventButton * event,
				       void * tracks_and_layers)
{
	static GtkTreeIter iter;

	// Use selected item to get a single iterator ref
	// This relies on an row being selected as part of the right click
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
	if (gtk_tree_selection_count_selected_rows(selection) != 1) {
		return trw_layer_track_menu_popup_multi(tree_view, event, tracks_and_layers);
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

	Track * trk;
	gtk_tree_model_get(model, &iter, TRK_COL_NUM, &trk, -1);
	if (!trk) {
		return false;
	}

	VikLayer * vtl;
	gtk_tree_model_get(model, &iter, TRW_COL_NUM, &vtl, -1);
	LayerTRW * trw = (LayerTRW *) vtl->layer;
	if (trw->type != LayerType::TRW) {
		return false;
	}

	sg_uid_t uid = 0;;
	if (trk->is_route) {
		uid = LayerTRWc::find_uid_of_track(trw->get_routes(), trk);
	} else {
		uid = LayerTRWc::find_uid_of_track(trw->get_tracks(), trk);
	}

	if (uid) {
		Viewport * viewport = window_from_layer(trw)->get_viewport();

		GtkWidget * menu = gtk_menu_new();

		// Originally started to reuse the trw_layer menu items
		//  however these offer too many ways to edit the track data
		//  so without an easy way to distinguish read only operations,
		//  create a very minimal new set of operations
		add_menu_items(GTK_MENU(menu),
			       trw,
			       trk,
			       uid,
			       viewport,
			       tree_view,
			       (std::list<track_layer_t *> * ) tracks_and_layers);

		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, gtk_get_current_event_time());
		return true;
	}
	return false;
}

static bool trw_layer_track_button_pressed_cb(GtkWidget * tree_view,
					      GdkEventButton * event,
					      void * tracks_and_layers)
{
	// Only on right clicks...
	if (!(event->type == GDK_BUTTON_PRESS && event->button == MouseButton::RIGHT)) {
		return false;
	}

	// ATM Force a selection...
	GtkTreeSelection * selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
	if (gtk_tree_selection_count_selected_rows(selection) <= 1) {
		GtkTreePath * path;
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
	return trw_layer_track_menu_popup(tree_view, event, tracks_and_layers);
}

/*
 * Foreach entry we copy the various individual track properties into the tree store
 *  formatting & converting the internal values into something for display
 */
static void trw_layer_track_list_add(track_layer_t * element,
				     GtkTreeStore * store,
				     vik_units_distance_t dist_units,
				     vik_units_speed_t speed_units,
				     vik_units_height_t height_units,
				     const char * date_format)
{
	GtkTreeIter t_iter;
	Track * trk = element->trk;
	LayerTRW * trw = element->trw;

	double trk_dist = trk->get_length();
	// Store unit converted value
	switch (dist_units) {
	case VIK_UNITS_DISTANCE_MILES:
		trk_dist = VIK_METERS_TO_MILES(trk_dist);
		break;
	default:
		trk_dist = trk_dist/1000.0;
		break;
	}

	// Get start date
	char time_buf[32];
	time_buf[0] = '\0';
	if (!trk->empty()
	    && (*trk->trackpointsB->begin())->has_timestamp) {

#if GLIB_CHECK_VERSION(2,26,0)
		GDateTime * gdt = g_date_time_new_from_unix_utc((*trk->trackpointsB->begin())->timestamp);
		char * time = g_date_time_format(gdt, date_format);
		g_strlcpy(time_buf, time, sizeof(time_buf));
		free(time);
		g_date_time_unref(gdt);
#else
		GDate * gdate_start = g_date_new();
		g_date_set_time_t(gdate_start, (*trk->trackpointsB->begin())->timestamp);
		g_date_strftime(time_buf, sizeof(time_buf), date_format, gdate_start);
		g_date_free(gdate_start);
#endif
	}

	// NB: doesn't include aggegrate visibility
	bool visible = trw->visible && trk->visible;
	visible = visible && (trk->is_route ? trw->get_routes_visibility() : trw->get_tracks_visibility());

	unsigned int trk_len_time = 0; // In minutes
	if (!trk->empty()) {
		time_t t1, t2;
		t1 = (*trk->trackpointsB->begin())->timestamp;
		t2 = (*std::prev(trk->trackpointsB->end()))->timestamp;
		trk_len_time = (int) round(labs(t2 - t1) / 60.0);
	}

	double av_speed = 0.0;
	double max_speed = 0.0;
	double max_alt = 0.0;

	av_speed = trk->get_average_speed();
	switch (speed_units) {
	case VIK_UNITS_SPEED_KILOMETRES_PER_HOUR: av_speed = VIK_MPS_TO_KPH(av_speed); break;
	case VIK_UNITS_SPEED_MILES_PER_HOUR:      av_speed = VIK_MPS_TO_MPH(av_speed); break;
	case VIK_UNITS_SPEED_KNOTS:               av_speed = VIK_MPS_TO_KNOTS(av_speed); break;
	default: // VIK_UNITS_SPEED_METRES_PER_SECOND therefore no change
		break;
	}

	max_speed = trk->get_max_speed();
	switch (speed_units) {
	case VIK_UNITS_SPEED_KILOMETRES_PER_HOUR: max_speed = VIK_MPS_TO_KPH(max_speed); break;
	case VIK_UNITS_SPEED_MILES_PER_HOUR:      max_speed = VIK_MPS_TO_MPH(max_speed); break;
	case VIK_UNITS_SPEED_KNOTS:               max_speed = VIK_MPS_TO_KNOTS(max_speed); break;
	default: // VIK_UNITS_SPEED_METRES_PER_SECOND therefore no change
		break;
	}

	// TODO - make this a function to get min / max values?
	double * altitudes = NULL;
	altitudes = trk->make_elevation_map(500);
	if (altitudes) {
		max_alt = -1000;
		unsigned int i;
		for (i = 0; i < 500; i++) {
			if (altitudes[i] != VIK_DEFAULT_ALTITUDE) {
				if (altitudes[i] > max_alt) {
					max_alt = altitudes[i];
				}
			}
		}
	}
	free(altitudes);

	switch (height_units) {
	case VIK_UNITS_HEIGHT_FEET: max_alt = VIK_METERS_TO_FEET(max_alt); break;
	default:
		// VIK_UNITS_HEIGHT_METRES: no need to convert
		break;
	}

	gtk_tree_store_append(store, &t_iter, NULL);
	gtk_tree_store_set(store, &t_iter,
			   0, trw->name,
			   1, trk->name,
			   2, time_buf,
			   3, visible,
			   4, trk_dist,
			   5, trk_len_time,
			   6, av_speed,
			   7, max_speed,
			   8, (int) round(max_alt),
			   TRW_COL_NUM, trw->vl,
			   TRK_COL_NUM, trk,
			   -1);
}

static GtkTreeViewColumn * my_new_column_text(const char * title, GtkCellRenderer * renderer, GtkWidget *view, int column_runner)
{
	GtkTreeViewColumn * column = gtk_tree_view_column_new_with_attributes(title, renderer, "text", column_runner, NULL);
	gtk_tree_view_column_set_sort_column_id(column, column_runner);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
	gtk_tree_view_column_set_reorderable(column, true);
	gtk_tree_view_column_set_resizable(column, true);
	return column;
}

/**
 * vik_trw_layer_track_list_internal:
 * @dialog:            The dialog to create the widgets in
 * @tracks_and_layers: The list of tracks (and it's layer) to be shown
 * @show_layer_names:  Show the layer names that each track belongs to
 *
 * Create a table of tracks with corresponding track information
 * This table does not support being actively updated
 */
static void vik_trw_layer_track_list_internal(GtkWidget * dialog,
					      std::list<track_layer_t *> * tracks_and_layers,
					      bool show_layer_names)
{
	if (!tracks_and_layers || tracks_and_layers->empty()) {
		return;
	}

	// It's simple storing the double values in the tree store as the sort works automatically
	// Then apply specific cell data formatting(rather default double is to 6 decimal places!)
	GtkTreeStore *store = gtk_tree_store_new(TRK_LIST_COLS,
						 G_TYPE_STRING,    // 0: Layer Name
						 G_TYPE_STRING,    // 1: Track Name
						 G_TYPE_STRING,    // 2: Date
						 G_TYPE_BOOLEAN,   // 3: Visible
						 G_TYPE_DOUBLE,    // 4: Distance
						 G_TYPE_UINT,      // 5: Length in time
						 G_TYPE_DOUBLE,    // 6: Av. Speed
						 G_TYPE_DOUBLE,    // 7: Max Speed
						 G_TYPE_INT,       // 8: Max Height
						 G_TYPE_POINTER,   // 9: TrackWaypoint Layer pointer
						 G_TYPE_POINTER);  // 10: Track pointer

	//gtk_tree_selection_set_select_function(gtk_tree_view_get_selection (GTK_TREE_VIEW(vt)), vik_treeview_selection_filter, vt, NULL);

	vik_units_distance_t dist_units = a_vik_get_units_distance();
	vik_units_speed_t speed_units = a_vik_get_units_speed();
	vik_units_height_t height_units = a_vik_get_units_height();

	//GList *gl = get_tracks_and_layers_cb(vl, user_data);
	//g_list_foreach (tracks_and_layers, (GFunc) trw_layer_track_list_add, store);
	char *date_format = NULL;
	if (!a_settings_get_string(VIK_SETTINGS_LIST_DATE_FORMAT, &date_format)) {
		date_format = g_strdup(TRACK_LIST_DATE_FORMAT);
	}

	for (auto iter = tracks_and_layers->begin(); iter != tracks_and_layers->end(); iter++) {
		trw_layer_track_list_add(*iter, store, dist_units, speed_units, height_units, date_format);
	}
	free(date_format);

	GtkWidget * view = gtk_tree_view_new();
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
	g_object_set(G_OBJECT (renderer),
		     "xalign", 0.0,
		     "ellipsize", PANGO_ELLIPSIZE_END,
		     NULL);

	GtkTreeViewColumn * column;
	GtkTreeViewColumn * sort_by_column;

	int column_runner = 0;
	if (show_layer_names) {
		// Insert column for the layer name when viewing multi layers
		column = my_new_column_text(_("Layer"), renderer, view, column_runner++);
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
	gtk_tree_view_column_set_expand(column, true);

	GtkCellRenderer *renderer_toggle = gtk_cell_renderer_toggle_new();
	column = gtk_tree_view_column_new_with_attributes(_("Visible"), renderer_toggle, "active", column_runner, NULL);
	gtk_tree_view_column_set_reorderable(column, true);
	gtk_tree_view_column_set_sort_column_id(column, column_runner);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
	column_runner++;

	switch (dist_units) {
	case VIK_UNITS_DISTANCE_MILES:
		column = my_new_column_text(_("Distance\n(miles)"), renderer, view, column_runner++);
		break;
	default:
		column = my_new_column_text(_("Distance\n(km)"), renderer, view, column_runner++);
		break;
	}
	// Apply own formatting of the data
	gtk_tree_view_column_set_cell_data_func(column, renderer, format_1f_cell_data_func, KINT_TO_POINTER(column_runner-1), NULL);

	(void) my_new_column_text(_("Length\n(minutes)"), renderer, view, column_runner++);

	char *spd_units = NULL;
	switch (speed_units) {
	case VIK_UNITS_SPEED_KILOMETRES_PER_HOUR: spd_units = strdup(_("km/h")); break;
	case VIK_UNITS_SPEED_MILES_PER_HOUR:      spd_units = strdup(_("mph")); break;
	case VIK_UNITS_SPEED_KNOTS:               spd_units = strdup(_("knots")); break;
	// VIK_UNITS_SPEED_METRES_PER_SECOND:
	default:                                  spd_units = strdup(_("m/s")); break;
	}

	char * title = g_strdup_printf(_("Av. Speed\n(%s)"), spd_units);
	column = my_new_column_text(title, renderer, view, column_runner++);
	free(title);
	gtk_tree_view_column_set_cell_data_func(column, renderer, format_1f_cell_data_func, KINT_TO_POINTER(column_runner-1), NULL); // Apply own formatting of the data

	title = g_strdup_printf(_("Max Speed\n(%s)"), spd_units);
	column = my_new_column_text(title, renderer, view, column_runner++);
	gtk_tree_view_column_set_cell_data_func(column, renderer, format_1f_cell_data_func, KINT_TO_POINTER(column_runner-1), NULL); // Apply own formatting of the data

	free(title);
	free(spd_units);

	if (height_units == VIK_UNITS_HEIGHT_FEET) {
		(void) my_new_column_text(_("Max Height\n(Feet)"), renderer, view, column_runner++);
	} else {
		(void) my_new_column_text(_("Max Height\n(Metres)"), renderer, view, column_runner++);
	}

	gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(store));
	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), GTK_SELECTION_MULTIPLE);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(view), true);

	g_object_unref(store);

	GtkWidget *scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolledwindow), view);

	g_object_set(view, "has-tooltip", true, NULL);

	g_signal_connect(view, "query-tooltip", G_CALLBACK (trw_layer_track_tooltip_cb), NULL);
	//g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW(view)), "changed", G_CALLBACK(trw_layer_track_select_cb), view);

	g_signal_connect(view, "popup-menu", G_CALLBACK(trw_layer_track_menu_popup), tracks_and_layers);
	g_signal_connect(view, "button-press-event", G_CALLBACK(trw_layer_track_button_pressed_cb), tracks_and_layers);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), scrolledwindow, true, true, 0);

	// Set ordering of the initial view by one of the name columns
	gtk_tree_view_column_clicked(sort_by_column);

	// Ensure a reasonable number of items are shown
	//  TODO: may be save window size, column order, sorted by between invocations.
	// Gtk too stupid to work out best size so need to tell it.
	gtk_window_set_default_size(GTK_WINDOW(dialog), show_layer_names ? 900 : 700, 400);
}


/**
 * vik_trw_layer_track_list_show_dialog:
 * @title:                    The title for the dialog
 * @vl:                       The #VikLayer passed on into get_tracks_and_layers_cb()
 * @user_data:                Data passed on into get_tracks_and_layers_cb()
 * @get_tracks_and_layers_cb: The function to call to construct items to be analysed
 * @show_layer_names:         Normally only set when called from an aggregate level
 *
 * Common method for showing a list of tracks with extended information
 *
 */
void vik_trw_layer_track_list_show_dialog(char * title,
					  Layer * layer,
					  void * user_data,
					  VikTrwlayerGetTracksAndLayersFunc get_tracks_and_layers_cb,
					  bool show_layer_names)
{
	GtkWidget * dialog = gtk_dialog_new_with_buttons(title,
							 gtk_window_from_layer(layer),
							 GTK_DIALOG_DESTROY_WITH_PARENT,
							 GTK_STOCK_CLOSE,
							 GTK_RESPONSE_CLOSE,
							 NULL);


	std::list<track_layer_t *> * tracks_and_layers = get_tracks_and_layers_cb(layer, user_data);

	vik_trw_layer_track_list_internal(dialog, tracks_and_layers, show_layer_names);

	// Use response to close the dialog with tidy up
	g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(track_close_cb), tracks_and_layers);

	gtk_widget_show_all(dialog);
	// Yes - set the size *AGAIN* - this time widgets are expanded nicely
	gtk_window_resize(GTK_WINDOW(dialog), show_layer_names ? 1000 : 800, 400);

	// ATM lock out on dialog run - to prevent list contents being manipulated in other parts of the GUI whilst shown here.
	gtk_dialog_run(GTK_DIALOG (dialog));
	// Unfortunately seems subsequently opening the Track Properties we can't interact with it until this dialog is closed
	// Thus this dialog is then forcibly closed when opening the properties.

	// Occassionally the 'View' doesn't update the viewport properly
	//  viewport center + zoom is changed but the viewport isn't updated
	// not sure why yet..
}
