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
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cassert>

#include <QMenu>

#ifdef K
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#endif

#include "waypoint_list.h"
#include "waypoint_properties.h"
#include "util.h"
#ifdef K
#include "clipboard.h"
#include "dialog.h"
#include "settings.h"
#include "globals.h"
#include "window.h"
#endif




using namespace SlavGPS;




/* Long formatted date+basic time - listing this way ensures the string
   comparison sort works - so no local type format %x or %c here! */
#define WAYPOINT_LIST_DATE_FORMAT "%Y-%m-%d %H:%M"




enum {
	LAYER_NAME_COLUMN = 0,    /* Layer Name (string). */
	WAYPOINT_NAME_COLUMN,     /* Waypoint Name (string). */
	DATE_COLUMN,              /* Date (string). */
	VISIBLE_COLUMN,           /* Visibility (boolean). */
	COMMENT_COLUMN,           /* Comment (string). */
	HEIGHT_COLUMN,            /* Height (int). */
	SYMBOL_COLUMN,            /* Symbol icon (pixmap). */
	LAYER_POINTER_COLUMN,     /* Pointer to TRW layer (pointer). */
	WAYPOINT_POINTER_COLUMN,  /* Pointer to waypoint (pointer). */
	N_COLUMNS
};




#if 0
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
	gtk_tree_model_get (model, iter, column, &value, -1);
	snprintf(buf, sizeof(buf), "%.1f", value);
	g_object_set (renderer, "text", buf, NULL);
}
#endif




#if 0
void WaypointListDialog::waypoint_select_cb(void) /* Slot. */
{
	GtkTreeIter iter;
	if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		return;
	}

	GtkTreeView *tree_view = GTK_TREE_VIEW (data);
	GtkTreeModel *model = gtk_tree_view_get_model(tree_view);

	Waypoint *wp;
	gtk_tree_model_get(model, &iter, WAYPOINT_POINTER_COLUMN, &wp, -1);
	if (!wp) {
		return;
	}

	LayerTRW * trw;
	gtk_tree_model_get(model, &iter, LAYER_POINTER_COLUMN, &trw-, -1);
	if (trw->type != LayerTRW::TRW) {
		return;
	}

	//vik_treeview_select_iter(trw->vt, g_hash_table_lookup(trw->waypoint_iters, uuid), true);
}
#endif





/* Instead of hooking automatically on treeview item selection.
   This is performed on demand via the specific menu request. */
void WaypointListDialog::waypoint_select(LayerTRW * layer)
{
	LayerTRW * trw = this->menu_data.trw;
	sg_uid_t uid = this->menu_data.waypoint_uid;

	if (uid) {
		TreeIndex * index = trw->get_waypoints_iters().at(uid);

		if (index && index->isValid()) {
			trw->tree_view->select_and_expose(index);
		}
	}
}




void WaypointListDialog::waypoint_properties_cb(void) /* Slot. */
{
	LayerTRW * trw = this->menu_data.trw;
	Waypoint * wp = this->menu_data.waypoint;

	if (!wp || !wp->name) {
		return;
	}

	/* Close this dialog to allow interaction with properties window.
	   Since the properties also allows waypoint manipulations it won't cause conflicts here. */
	this->accept();

	bool updated = false;
	char * new_name = waypoint_properties_dialog(trw->get_window(), wp->name, trw, wp, trw->get_coord_mode(), false, &updated);
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




void WaypointListDialog::waypoint_view_cb(void) /* Slot. */
{
	this->menu_data.viewport->set_center_coord(&this->menu_data.waypoint->coord, true);
	this->waypoint_select(this->menu_data.trw);
	this->menu_data.trw->emit_changed();
}




void WaypointListDialog::show_picture_waypoint_cb(void) /* Slot. */
{
	Waypoint * wp = this->menu_data.waypoint;
	LayerTRW * trw = this->menu_data.trw;
#ifdef K
#ifdef WINDOWS
	ShellExecute(NULL, "open", wp->image, NULL, NULL, SW_SHOWNORMAL);
#else

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
 * (since the position data is not shown in the list but is useful in copying to external apps).
 *
 * ATM The visibility flag is not copied and neither is a text representation of the waypoint symbol.
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
        gtk_tree_model_get(model, iter, WAYPOINT_POINTER_COLUMN, &wp, -1);
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
	cd.has_layer_names = (count > N_COLUMNS-3);
	cd.str = g_string_new(NULL);
	cd.include_positions = include_positions;
	gtk_tree_selection_selected_foreach(selection, copy_selection, &cd);

	a_clipboard_copy(VIK_CLIPBOARD_DATA_TEXT, LayerType::AGGREGATE, SublayerType::NONE, 0, cd.str->str, NULL);

	g_string_free(cd.str, true);
#endif
}




void WaypointListDialog::copy_selected_only_visible_columns_cb(void) /* Slot. */
{
#ifdef K
	trw_layer_copy_selected(tree_view, false);
#endif
}




void WaypointListDialog::copy_selected_with_position_cb(void) /* Slot. */
{
#ifdef K
	trw_layer_copy_selected(tree_view, true);
#endif
}




void WaypointListDialog::add_copy_menu_items(QMenu & menu)
{
	QAction * qa = NULL;

	qa = menu.addAction(QIcon::fromTheme("edit-copy"), QString(_("&Copy Data")));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (copy_selected_only_visible_columns_cb()));

	qa = menu.addAction(QIcon::fromTheme("edit-copy"), QString(_("Copy Data (with &positions)")));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (copy_selected_with_position_cb()));
}




void WaypointListDialog::add_menu_items(QMenu & menu)
{
	QAction * qa = NULL;

#if 0
	qa = menu.addAction(QIcon::fromTheme("edit-find"), QString(_("&Select")));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (waypoint_select_cb()));
#endif

	qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), QString(_("&Zoom onto")));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (waypoint_view_cb()));

	qa = menu.addAction(QIcon::fromTheme("document-properties"), QString(_("&Properties")));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (waypoint_properties_cb()));

	qa = menu.addAction(QIcon::fromTheme("vik-icon-Show Picture"), QString(_("&Show Picture...")));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (show_picture_waypoint_cb()));
	qa->setEnabled(this->menu_data.waypoint->image);

	return;
}




void WaypointListDialog::contextMenuEvent(QContextMenuEvent * event)
{
	QPoint orig = event->pos();
	QPoint v = this->view->pos();
	QPoint t = this->view->viewport()->pos();

	orig.setX(orig.x() - v.x() - t.x());
	orig.setY(orig.y() - v.y() - t.y());

	QPoint point = orig;
	QModelIndex index = this->view->indexAt(point);
	if (!index.isValid()) {
		qDebug() << "II: Waypoint List: context menu event: INvalid index";
		return;
	} else {
		qDebug() << "II: Waypoint List: context menu event: on index.row =" << index.row() << "index.column =" << index.column();
	}


	QStandardItem * parent_item = this->model->invisibleRootItem();


	QStandardItem * child = parent_item->child(index.row(), WAYPOINT_NAME_COLUMN);
	qDebug() << "II: Waypoint List: selected waypoint" << child->text();

	child = parent_item->child(index.row(), WAYPOINT_POINTER_COLUMN);
	QVariant variant = child->data(RoleLayerData);
	Waypoint * wp = (Waypoint *) variant.toULongLong();


	child = parent_item->child(index.row(), LAYER_POINTER_COLUMN);
	variant = child->data(RoleLayerData);
	LayerTRW * trw = (LayerTRW *) variant.value<Layer *>();
	if (trw->type != LayerType::TRW) {
		qDebug() << "EE: Waypoint List: layer type is not TRW:" << (int) trw->type;
		return;
	}


	sg_uid_t wp_uuid = LayerTRWc::find_uid_of_waypoint(trw->get_waypoints(), wp);
	if (wp_uuid) {
		this->menu_data.trw = trw;
		this->menu_data.waypoint = wp;
		this->menu_data.waypoint_uid = wp_uuid;
		this->menu_data.viewport = trw->get_window()->get_viewport();

		QMenu menu(this);
#if 0
		/* When multiple rows are selected, the number of applicable operation is lower. */
		GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW (tree_view));
		if (gtk_tree_selection_count_selected_rows (selection) != 1) {
			this->add_copy_menu_items(QMenu & menu);
		}
#else
		this->add_menu_items(menu);
#endif

		menu.exec(QCursor::pos());
		return;
	}
	return;
}




/*
 * For each entry we copy the various individual waypoint properties into the tree store
 * formatting & converting the internal values into something for display.
 */
void WaypointListDialog::add(Waypoint * wp, LayerTRW * trw, HeightUnit height_units, const char * date_format)
{
	/* Get start date. */
	char time_buf[32] = { 0 };
	if (wp->has_timestamp) {

#ifdef K
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
#endif
	}

	/* This parameter doesn't include aggegrate visibility. */
	bool visible = trw->visible && wp->visible;
	visible = visible && trw->get_waypoints_visibility();

	double alt = wp->altitude;
	switch (height_units) {
	case HeightUnit::FEET:
		alt = VIK_METERS_TO_FEET(alt);
		break;
	default:
		/* HeightUnit::METRES: no need to convert. */
		break;
	}


	QList<QStandardItem *> items;
	QStandardItem * item = NULL;
	QVariant variant;
	QString tooltip(wp->description ? wp->description : "");

	/* TODO: add sorting by columns. Add reordering of columns. */


	/* LAYER_NAME_COLUMN */
	item = new QStandardItem(QString(trw->name));
	item->setToolTip(tooltip);
	items << item;

	/* WAYPOINT_NAME_COLUMN */
	item = new QStandardItem(QString(wp->name));
	item->setToolTip(tooltip);
	items << item;

	/* DATE_COLUMN */
	item = new QStandardItem(QString(time_buf));
	item->setToolTip(tooltip);
	items << item;

	/* VISIBLE_COLUMN */
	item = new QStandardItem();
	item->setToolTip(tooltip);
	item->setCheckable(true);
	item->setCheckState(visible ? Qt::Checked : Qt::Unchecked);
	items << item;

	/* COMMENT_COLUMN */
	item = new QStandardItem(QString(wp->comment));
	item->setToolTip(tooltip);
	items << item;

	/* HEIGHT_COLUMN */
	item = new QStandardItem();
	item->setToolTip(tooltip);
	variant = QVariant::fromValue((int) round(alt));
	item->setData(variant, RoleLayerData);
	items << item;

	/* SYMBOL_COLUMN */
	/* TODO: table should be sortable by this column. */
#ifdef K
	get_wp_sym_small(wp->symbol);
#endif
	item = new QStandardItem();
	item->setToolTip(tooltip);
	item->setIcon(QIcon::fromTheme("list-add"));
	item->setEditable(false);
	items << item;

	/* LAYER_POINTER_COLUMN */
	item = new QStandardItem();
	variant = QVariant::fromValue((Layer *) trw);
	item->setData(variant, RoleLayerData);
	items << item;

	/* WAYPOINT_POINTER_COLUMN */
	item = new QStandardItem();
	variant = QVariant::fromValue((qulonglong) wp);
	item->setData(variant, RoleLayerData);
	items << item;

	this->model->invisibleRootItem()->appendRow(items);
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
	gtk_tree_model_get(model, a, WAYPOINT_POINTER_COLUMN, &wp1, -1);
	if (!wp1) {
		return 0;
	}
	gtk_tree_model_get(model, b, WAYPOINT_POINTER_COLUMN, &wp2, -1);
	if (!wp2) {
		return 0;
	}

	return g_strcmp0(wp1->symbol, wp2->symbol);
#endif
}




/**
 * vik_trw_layer_waypoint_list_internal:
 * @dialog:            The dialog to create the widgets in
 * @waypoints_and_layers: The list of waypoints (and it's layer) to be shown
 * @hide_layer_names:  Don't show the layer names that each waypoint belongs to
 *
 * Create a table of waypoints with corresponding waypoint information
 * This table does not support being actively updated
 */
void WaypointListDialog::build_model(bool hide_layer_names)
{
	if (!this->waypoints_and_layers || this->waypoints_and_layers->empty()) {
		return;
	}

	HeightUnit height_units = a_vik_get_units_height();

	this->model = new QStandardItemModel();
	this->model->setHorizontalHeaderItem(LAYER_NAME_COLUMN, new QStandardItem("Layer"));
	this->model->setHorizontalHeaderItem(WAYPOINT_NAME_COLUMN, new QStandardItem("Name"));
	this->model->setHorizontalHeaderItem(DATE_COLUMN, new QStandardItem("Date"));
	this->model->setHorizontalHeaderItem(VISIBLE_COLUMN, new QStandardItem("Visible"));
	this->model->setHorizontalHeaderItem(COMMENT_COLUMN, new QStandardItem("Comment"));
	if (height_units == HeightUnit::FEET) {
		this->model->setHorizontalHeaderItem(HEIGHT_COLUMN, new QStandardItem("Max Height\n(Feet)"));
	} else {
		this->model->setHorizontalHeaderItem(HEIGHT_COLUMN, new QStandardItem("Max Height\n(Metres)"));
	}
	this->model->setHorizontalHeaderItem(SYMBOL_COLUMN, new QStandardItem("Symbol"));
	this->model->setHorizontalHeaderItem(LAYER_POINTER_COLUMN, new QStandardItem("Layer Pointer"));
	this->model->setHorizontalHeaderItem(WAYPOINT_POINTER_COLUMN, new QStandardItem("Waypoint Pointer"));


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

	this->view->horizontalHeader()->setSectionHidden(WAYPOINT_NAME_COLUMN, false);
	this->view->horizontalHeader()->setSectionResizeMode(WAYPOINT_NAME_COLUMN, QHeaderView::Interactive);

	this->view->horizontalHeader()->setSectionHidden(DATE_COLUMN, false);
	this->view->horizontalHeader()->setSectionResizeMode(DATE_COLUMN, QHeaderView::ResizeToContents);

	this->view->horizontalHeader()->setSectionHidden(VISIBLE_COLUMN, false);
	this->view->horizontalHeader()->setSectionResizeMode(VISIBLE_COLUMN, QHeaderView::ResizeToContents);

	this->view->horizontalHeader()->setSectionHidden(COMMENT_COLUMN, false);
	this->view->horizontalHeader()->setSectionResizeMode(COMMENT_COLUMN, QHeaderView::Stretch);

	this->view->horizontalHeader()->setSectionHidden(HEIGHT_COLUMN, false);
	this->view->horizontalHeader()->setSectionResizeMode(HEIGHT_COLUMN, QHeaderView::ResizeToContents);

	this->view->horizontalHeader()->setSectionHidden(SYMBOL_COLUMN, false);
	this->view->horizontalHeader()->setSectionResizeMode(SYMBOL_COLUMN, QHeaderView::ResizeToContents);

	this->view->horizontalHeader()->setSectionHidden(LAYER_POINTER_COLUMN, true);
	this->view->horizontalHeader()->setSectionHidden(WAYPOINT_POINTER_COLUMN, true);


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
		date_format = g_strdup(WAYPOINT_LIST_DATE_FORMAT);
	}

	for (auto iter = waypoints_and_layers->begin(); iter != waypoints_and_layers->end(); iter++) {
		this->add((*iter)->wp, (*iter)->trw, height_units, date_format);
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

	this->setMinimumSize(hide_layer_names ? 700 : 500, 400);
}




/**
 * vik_trw_layer_waypoint_list_show_dialog:
 * @title:                    The title for the dialog
 * @vl:                       The #VikLayer passed on into create_waypoints_and_layers_cb()
 * @is_aggregate_layer:
 *
 * Common method for showing a list of waypoints with extended information
 */
void SlavGPS::vik_trw_layer_waypoint_list_show_dialog(QString const & title,
						      Layer * layer,
						      bool is_aggregate_layer)
{
	WaypointListDialog dialog(title, layer->get_window());


	if (layer->type == LayerType::TRW) {
		dialog.waypoints_and_layers = ((LayerTRW *) layer)->create_waypoints_and_layers_list();
	} else if (layer->type == LayerType::AGGREGATE) {
		dialog.waypoints_and_layers = ((LayerAggregate *) layer)->create_waypoints_and_layers_list();
	} else {
		assert (0);
	}

	dialog.build_model(!is_aggregate_layer);
	dialog.exec();
}




WaypointListDialog::WaypointListDialog(QString const & title, QWidget * parent) : QDialog(parent)
{
	this->setWindowTitle(title);

	this->button_box = new QDialogButtonBox();
	this->parent = parent;
	this->button_box->addButton("&Close", QDialogButtonBox::ActionRole);
	this->vbox = new QVBoxLayout;
}




WaypointListDialog::~WaypointListDialog()
{
	delete this->waypoints_and_layers;
}
