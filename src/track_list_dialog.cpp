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
#include <cassert>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "viking.h"
#include "track_list_dialog.h"
#include "track_profile_dialog.h"
#include "clipboard.h"
#include "settings.h"
#include "globals.h"
#include "window.h"
#include "vikutils.h"




using namespace SlavGPS;




// Long formatted date+basic time - listing this way ensures the string comparison sort works - so no local type format %x or %c here!
#define TRACK_LIST_DATE_FORMAT "%Y-%m-%d %H:%M"


enum {
	LAYER_NAME_COLUMN,
	TRACK_NAME_COLUMN,
	DATE_COLUMN,
	VISIBLE_COLUMN,
	COMMENT_COLUMN,
	LENGTH_COLUMN,
	DURATION_COLUMN,
	AVERAGE_SPEED_COLUMN,
	MAXIMUM_SPEED_COLUMN,
	MAXIMUM_HEIGHT_COLUMN,
	LAYER_POINTER_COLUMN,
	TRACK_POINTER_COLUMN,
};




/**
 * track_close_cb:
 *
 */
static void track_close_cb(GtkWidget * dialog, int resp, std::list<track_layer_t *> * tracks_and_layers)
{
	/* kamilTODO: should we delete tracks_and_layers here? */
#ifdef K
	gtk_widget_destroy(dialog);
#endif
}




/**
 * format_1f_cell_data_func:
 *
 * General purpose column double formatting
 *
 */
static void format_1f_cell_data_func(GtkTreeViewColumn * col,
				     GtkCellRenderer   * renderer,
				     GtkTreeModel      * model,
				     GtkTreeIter       * iter,
				     void              * user_data)
{
	double value;
	char buf[20];
	int column = KPOINTER_TO_INT (user_data);
#ifdef K
	gtk_tree_model_get(model, iter, column, &value, -1);
	snprintf(buf, sizeof(buf), "%.1f", value);
	g_object_set(renderer, "text", buf, NULL);
#endif
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
				       int                x,
				       int                y,
				       bool               keyboard_tip,
				       GtkTooltip       * tooltip,
				       void             * data)
{
	GtkTreeIter iter;
	GtkTreePath * path = NULL;
#ifdef K
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
#endif
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
	gtk_tree_model_get(model, &iter, TRW_COL_NUM, &trw, -1);
	if (trw->type != LayerType::TRW) {
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
#ifdef K
		GtkTreeIter *iter = NULL;
		if (trk->is_route) {
			iter = trw->get_routes_iters().at(uid);
		} else {
			iter = trw->get_tracks_iters().at(uid);
		}

		if (iter) {
			trw->tree_view->select_and_expose(iter);
		}
#endif
	}
}



static void trw_layer_track_stats_cb(tracklist_data_t * values)
{
	LayerTRW * trw = values->trw;
	Track * trk = values->track;
	Viewport * viewport = values->viewport;

	if (trk && trk->name) {
#ifdef K
		// Kill off this dialog to allow interaction with properties window
		//  since the properties also allows track manipulations it won't cause conflicts here.
		GtkWidget * gw = gtk_widget_get_toplevel(values->gtk_tree_view);
		track_close_cb(gw, 0, values->tracks_and_layers);

		track_properties_dialog(trw->get_window(),
					trw,
					trk,
					true);
#endif
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
#ifdef K
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
#endif
}




static void trw_layer_copy_selected(GtkWidget * tree_view)
{
#ifdef K
	GtkTreeSelection * selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
	// NB GTK3 has gtk_tree_view_get_n_columns() but we're GTK2 ATM
	GList * gl = gtk_tree_view_get_columns(GTK_TREE_VIEW(tree_view));
	unsigned int count = g_list_length(gl);
	g_list_free(gl);
	copy_data_t cd;
	cd.has_layer_names = (count > TRK_LIST_COLS - 3);
	// Or use gtk_tree_view_column_get_visible()?
	cd.str = g_string_new(NULL);
	gtk_tree_selection_selected_foreach(selection, copy_selection, &cd);

	a_clipboard_copy(VIK_CLIPBOARD_DATA_TEXT, LayerType::AGGREGATE, SublayerType::NONE, 0, cd.str->str, NULL);

	g_string_free(cd.str, true);
#endif
}




static void add_copy_menu_item(GtkMenu * menu, GtkWidget * tree_view)
{
#ifdef K
	GtkWidget * item = gtk_image_menu_item_new_with_mnemonic(_("_Copy Data"));
	gtk_image_menu_item_set_image((GtkImageMenuItem *) item, gtk_image_new_from_stock(GTK_STOCK_COPY, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(trw_layer_copy_selected), tree_view);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	gtk_widget_show(item);
#endif
}




static bool add_menu_items(QMenu & menu, LayerTRW * trw, Track * trk, sg_uid_t track_uid, Viewport * viewport, GtkWidget * gtk_tree_view, std::list<track_layer_t *> * tracks_and_layers)
{
#ifdef K
	static tracklist_data_t values;

	values.trw               = trw;
	values.track             = trk;
	values.track_uid         = track_uid;
	values.viewport          = viewport;
	values.gtk_tree_view     = gtk_tree_view;
	values.tracks_and_layers = tracks_and_layers;

	GtkWidget *item;

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
#endif
	return true;
}




static bool trw_layer_track_menu_popup_multi(GtkWidget * gtk_tree_view,
					     GdkEventButton * event,
					     void * data)
{
#ifdef K
	GtkWidget * menu = gtk_menu_new();

	add_copy_menu_item(GTK_MENU(menu), gtk_tree_view);

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, gtk_get_current_event_time());
#endif

	return true;
}




static bool trw_layer_track_menu_popup(GtkWidget * tree_view,
				       GdkEventButton * event,
				       void * tracks_and_layers)
{
#ifdef K
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

	LayerTRW * trw;
	gtk_tree_model_get(model, &iter, TRW_COL_NUM, &trw, -1);

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
		Viewport * viewport = trw->get_window()->get_viewport();

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
#endif
	return false;
}




static bool trw_layer_track_button_pressed_cb(GtkWidget * tree_view,
					      GdkEventButton * event,
					      void * tracks_and_layers)
{
#ifdef K
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
#endif
}




/*
 * Foreach entry we copy the various individual track properties into the tree store
 *  formatting & converting the internal values into something for display
 */
static void trw_layer_track_list_add(track_layer_t * element,
				     GtkTreeStore * store,
				     DistanceUnit distance_unit,
				     SpeedUnit speed_units,
				     HeightUnit height_units,
				     const char * date_format)
{
#ifdef K
	GtkTreeIter t_iter;
	Track * trk = element->trk;
	LayerTRW * trw = element->trw;

	double trk_dist = trk->get_length();
	// Store unit converted value
	trk_dist = convert_distance_meters_to(distance_unit, trk_dist);

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
		time_t t1 = (*trk->trackpointsB->begin())->timestamp;
		time_t t2 = (*std::prev(trk->trackpointsB->end()))->timestamp;
		trk_len_time = (int) round(labs(t2 - t1) / 60.0);
	}

	double av_speed = trk->get_average_speed();
	av_speed = convert_speed_mps_to(speed_units, av_speed);

	double max_speed = trk->get_max_speed();
	max_speed = convert_speed_mps_to(speed_units, max_speed);

	double max_alt = 0.0;
	// TODO - make this a function to get min / max values?
	double * altitudes = trk->make_elevation_map(500);
	if (altitudes) {
		max_alt = -1000;
		for (unsigned int i = 0; i < 500; i++) {
			if (altitudes[i] != VIK_DEFAULT_ALTITUDE) {
				if (altitudes[i] > max_alt) {
					max_alt = altitudes[i];
				}
			}
		}
	}
	free(altitudes);

	switch (height_units) {
	case HeightUnit::FEET:
		max_alt = VIK_METERS_TO_FEET(max_alt);
		break;
	default:
		// HeightUnit::METRES: no need to convert
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
			   TRW_COL_NUM, trw,
			   TRK_COL_NUM, trk,
			   -1);
#endif
}




static GtkTreeViewColumn * my_new_column_text(const char * title, GtkCellRenderer * renderer, GtkWidget *view, int column_runner)
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
 * vik_trw_layer_track_list_internal:
 * @dialog:            The dialog to create the widgets in
 * @tracks_and_layers: The list of tracks (and it's layer) to be shown
 * @show_layer_names:  Show the layer names that each track belongs to
 *
 * Create a table of tracks with corresponding track information
 * This table does not support being actively updated
 */
void TrackListDialog::build_model(bool hide_layer_names)
{
	if (!this->tracks_and_layers || this->tracks_and_layers->empty()) {
		return;
	}

	DistanceUnit distance_units = a_vik_get_units_distance();
	SpeedUnit speed_units = a_vik_get_units_speed();
	HeightUnit height_units = a_vik_get_units_height();


	this->model = new QStandardItemModel();
	this->model->setHorizontalHeaderItem(LAYER_NAME_COLUMN, new QStandardItem("Layer"));
	this->model->setHorizontalHeaderItem(TRACK_NAME_COLUMN, new QStandardItem("Track Name")); /* TODO: add sorting. */
	this->model->setHorizontalHeaderItem(DATE_COLUMN, new QStandardItem("Date"));
	this->model->setHorizontalHeaderItem(VISIBLE_COLUMN, new QStandardItem("Visible"));
	this->model->setHorizontalHeaderItem(COMMENT_COLUMN, new QStandardItem("Comment"));

	if (distance_units == DistanceUnit::MILES) {
		this->model->setHorizontalHeaderItem(LENGTH_COLUMN, new QStandardItem("Length\n(miles)"));
	} else if (distance_units == DistanceUnit::NAUTICAL_MILES) {
		this->model->setHorizontalHeaderItem(LENGTH_COLUMN, new QStandardItem("Length\n(nautical miles)"));
	} else {
		this->model->setHorizontalHeaderItem(LENGTH_COLUMN, new QStandardItem("Length\n(km)"));
	}

	this->model->setHorizontalHeaderItem(DURATION_COLUMN, new QStandardItem("Duration\n(minutes)"));

	char * speed_units_string = get_speed_unit_string(speed_units);
	this->model->setHorizontalHeaderItem(AVERAGE_SPEED_COLUMN, new QStandardItem(QString("Average Speed\n(%1)").arg(speed_units_string))); // format_1f_cell_data_func()  Apply own formatting of the data
	this->model->setHorizontalHeaderItem(MAXIMUM_SPEED_COLUMN, new QStandardItem(QString("Maximum Speed\n(%1)").arg(speed_units_string))); // format_1f_cell_data_func()  Apply own formatting of the data
	free(speed_units_string);

	if (height_units == HeightUnit::FEET) {
		this->model->setHorizontalHeaderItem(MAXIMUM_HEIGHT_COLUMN, new QStandardItem("Maximum Height\n(Feet)"));
	} else {
		this->model->setHorizontalHeaderItem(MAXIMUM_HEIGHT_COLUMN, new QStandardItem("Maximum Height\n(Metres)"));
	}

	this->model->setHorizontalHeaderItem(LAYER_POINTER_COLUMN, new QStandardItem("Layer Pointer"));
	this->model->setHorizontalHeaderItem(TRACK_POINTER_COLUMN, new QStandardItem("Track Pointer"));


	this->view = new QTableView();
	this->view->horizontalHeader()->setStretchLastSection(false);
	this->view->verticalHeader()->setVisible(false);
	this->view->setWordWrap(false);
	this->view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	this->view->setTextElideMode(Qt::ElideRight);
	this->view->setSelectionMode(QAbstractItemView::ExtendedSelection);
	this->view->setShowGrid(false);
	this->view->setModel(this->model);
	this->view->show();
	this->view->setVisible(false);
	this->view->resizeRowsToContents();
	this->view->resizeColumnsToContents();
	this->view->setVisible(true);


	this->view->horizontalHeader()->setSectionHidden(LAYER_NAME_COLUMN, hide_layer_names);
	this->view->horizontalHeader()->setSectionResizeMode(LAYER_NAME_COLUMN, QHeaderView::Interactive);

	this->view->horizontalHeader()->setSectionHidden(TRACK_NAME_COLUMN, false);
	this->view->horizontalHeader()->setSectionResizeMode(TRACK_NAME_COLUMN, QHeaderView::Interactive);

	this->view->horizontalHeader()->setSectionHidden(DATE_COLUMN, false);
	this->view->horizontalHeader()->setSectionResizeMode(DATE_COLUMN, QHeaderView::ResizeToContents);

	this->view->horizontalHeader()->setSectionHidden(VISIBLE_COLUMN, false);
	this->view->horizontalHeader()->setSectionResizeMode(VISIBLE_COLUMN, QHeaderView::ResizeToContents);

	this->view->horizontalHeader()->setSectionHidden(COMMENT_COLUMN, false);
	this->view->horizontalHeader()->setSectionResizeMode(COMMENT_COLUMN, QHeaderView::Stretch);

	this->view->horizontalHeader()->setSectionHidden(LENGTH_COLUMN, false);
	this->view->horizontalHeader()->setSectionResizeMode(LENGTH_COLUMN, QHeaderView::ResizeToContents);

	this->view->horizontalHeader()->setSectionHidden(DURATION_COLUMN, false);
	this->view->horizontalHeader()->setSectionResizeMode(DURATION_COLUMN, QHeaderView::ResizeToContents);

	this->view->horizontalHeader()->setSectionHidden(AVERAGE_SPEED_COLUMN, false);
	this->view->horizontalHeader()->setSectionResizeMode(AVERAGE_SPEED_COLUMN, QHeaderView::ResizeToContents);

	this->view->horizontalHeader()->setSectionHidden(MAXIMUM_SPEED_COLUMN, false);
	this->view->horizontalHeader()->setSectionResizeMode(MAXIMUM_SPEED_COLUMN, QHeaderView::ResizeToContents);

	this->view->horizontalHeader()->setSectionHidden(MAXIMUM_HEIGHT_COLUMN, false);
	this->view->horizontalHeader()->setSectionResizeMode(MAXIMUM_HEIGHT_COLUMN, QHeaderView::ResizeToContents);

	this->view->horizontalHeader()->setSectionHidden(LAYER_POINTER_COLUMN, true);
	this->view->horizontalHeader()->setSectionHidden(TRACK_POINTER_COLUMN, true);


	this->vbox->addWidget(this->view);
	this->vbox->addWidget(this->button_box);

	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox);

	connect(this->button_box, SIGNAL(accepted()), this, SLOT(accept()));



	char * date_format = NULL;
	if (
#ifdef K
	    !a_settings_get_string(VIK_SETTINGS_LIST_DATE_FORMAT, &date_format)
#else
	    false
#endif
	    ) {
		date_format = g_strdup(TRACK_LIST_DATE_FORMAT);
	}

	for (auto iter = tracks_and_layers->begin(); iter != tracks_and_layers->end(); iter++) {
#ifdef K
		this->add((*iter)->wp, (*iter)->trw, height_units, date_format);

		trw_layer_track_list_add(*iter, store, distance_units, speed_units, height_units, date_format);
#endif
	}
	free(date_format);

	/* TODO: add initial sorting by layer name or waypoint name. */
#ifdef K
	if (hide_layer_name) {
		sort by waypoint name;
	} else {
		sort by layer name;
	}
#endif


	/* TODO: add initial sorting by layer name or waypoint name. */
#ifdef K
	if (hide_layer_name) {
		sort by waypoint name;
	} else {
		sort by layer name;
	}
#endif

	this->setMinimumSize(hide_layer_names ? 500 : 700, 400);


#ifdef K
	g_signal_connect(view, "query-tooltip", G_CALLBACK (trw_layer_track_tooltip_cb), NULL);
	//g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW(view)), "changed", G_CALLBACK(trw_layer_track_select_cb), view);
	g_signal_connect(view, "popup-menu", G_CALLBACK(trw_layer_track_menu_popup), tracks_and_layers);
	g_signal_connect(view, "button-press-event", G_CALLBACK(trw_layer_track_button_pressed_cb), tracks_and_layers);

	gtk_tree_view_column_clicked(sort_by_column);
#endif
}




/**
 * track_list_dialog:
 * @title:               The title for the dialog
 * @layer:               The #Layer passed on into get_tracks_and_layers_cb()
 * @sublayer_typea:      Sublayer type to be show in list (NONE for both TRACKS and LAYER)
 * @show_layer_names:    Normally only set when called from an aggregate level
 *
 * Common method for showing a list of tracks with extended information
 *
 */
void SlavGPS::track_list_dialog(QString const & title,
				Layer * layer,
				SublayerType sublayer_type,
				bool show_layer_names)
{
	TrackListDialog dialog(title, layer->get_window());


	if (layer->type == LayerType::AGGREGATE) {
		if (sublayer_type == SublayerType::NONE) { /* No particular sublayer type means both tracks and layers. */
			dialog.tracks_and_layers = ((LayerAggregate *) layer)->create_tracks_and_layers_list();
		} else {
			dialog.tracks_and_layers = ((LayerAggregate *) layer)->create_tracks_and_layers_list(sublayer_type);
		}
	} else if (layer->type == LayerType::TRW) {
		if (sublayer_type == SublayerType::NONE) { /* No particular sublayer type means both tracks and layers. */
			dialog.tracks_and_layers = ((LayerTRW *) layer)->create_tracks_and_layers_list();
		} else {
			dialog.tracks_and_layers = ((LayerTRW *) layer)->create_tracks_and_layers_list(sublayer_type);
		}
	} else {
		assert (0);
	}

	dialog.build_model(!show_layer_names);
	dialog.exec();
}




TrackListDialog::TrackListDialog(QString const & title, QWidget * parent) : QDialog(parent)
{
	this->setWindowTitle(title);

	this->button_box = new QDialogButtonBox();
	this->parent = parent;
	this->button_box->addButton("&Close", QDialogButtonBox::ActionRole);
	this->vbox = new QVBoxLayout;
}




TrackListDialog::~TrackListDialog()
{
	delete this->tracks_and_layers;
}
