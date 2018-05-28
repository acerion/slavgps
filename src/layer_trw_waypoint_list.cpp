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
 */




#include <cmath>
#include <cassert>




#include <QMenu>
#include <QDebug>
#include <QHeaderView>
#include <QDateTime>




#include "layer.h"
#include "layer_trw.h"
#include "layer_trw_waypoint.h"
#include "layer_trw_waypoints.h"
#include "layer_trw_waypoint_list.h"
#include "layer_trw_waypoint_properties.h"
#include "layer_aggregate.h"
#include "window.h"
#include "viewport_internal.h"
#include "util.h"
#include "application_state.h"
#include "preferences.h"
#include "tree_view_internal.h"
#include "dialog.h"
#include "clipboard.h"




using namespace SlavGPS;




#define PREFIX ": Layer TRW Waypoint List:" << __FUNCTION__ << __LINE__ << ">"




extern Tree * g_tree;




/* Long formatted date+basic time - listing this way ensures the string
   comparison sort works - so no local type format %x or %c here! */
#define WAYPOINT_LIST_DATE_FORMAT "%Y-%m-%d %H:%M"




enum {
	LAYER_NAME_COLUMN = 0,    /* Layer Name (string). May not be displayed. */
	WAYPOINT_COLUMN,          /* Waypoint Name (string) + pointer to waypoint */
	DATE_COLUMN,              /* Date (string). */
	VISIBLE_COLUMN,           /* Visibility (boolean). */
	COMMENT_COLUMN,           /* Comment (string). */
	HEIGHT_COLUMN,            /* Height (integer). */
	SYMBOL_COLUMN,            /* Symbol icon (pixmap). */
};




/* Instead of hooking automatically on table item selection,
   this is performed on demand via the specific context menu request. */
void WaypointListDialog::waypoint_select(LayerTRW * layer)
{
	if (!this->selected_wp) {
		qDebug() << "EE: Waypoint List: encountered NULL Waypoint in callback" << __FUNCTION__;
		return;
	}

	Waypoint * wp = this->selected_wp;
	LayerTRW * trw = wp->get_parent_layer_trw();

	if (wp && trw) {
		trw->tree_view->select_and_expose(wp->index);
	} else {
		qDebug() << "EE: Waypoint List Dialog: selecting either NULL layer or NULL wp:" << (qintptr) trw << (qintptr) wp;
	}
}




void WaypointListDialog::waypoint_properties_cb(void) /* Slot. */
{
	if (!this->selected_wp) {
		qDebug() << "EE: Waypoint List: encountered NULL Waypoint in callback" << __FUNCTION__;
		return;
	}

	Waypoint * wp = this->selected_wp;
	LayerTRW * trw = wp->get_parent_layer_trw();

	if (wp->name.isEmpty()) {
		return;
	}

	/* Close this dialog to allow interaction with properties window.
	   Since the properties also allows waypoint manipulations it won't cause conflicts here. */
	this->accept();

	const std::tuple<bool, bool> result = waypoint_properties_dialog(wp, wp->name, trw->get_coord_mode(), g_tree->tree_get_main_window());
	if (std::get<SG_WP_DIALOG_OK>(result)) { /* "OK" pressed in dialog, waypoint's parameters entered in the dialog are valid. */

		if (std::get<SG_WP_DIALOG_NAME>(result)) {
			/* Waypoint's name has been changed. */
			trw->get_waypoints_node().propagate_new_waypoint_name(wp);
		}

		trw->get_waypoints_node().set_new_waypoint_icon(wp);

		if (trw->visible) {
			trw->emit_layer_changed();
		}
	}
}




void WaypointListDialog::waypoint_view_cb(void) /* Slot. */
{
	if (!this->selected_wp) {
		qDebug() << "EE: Waypoint List: encountered NULL Waypoint in callback" << __FUNCTION__;
		return;
	}

	Waypoint * wp = this->selected_wp;
	LayerTRW * trw = wp->get_parent_layer_trw();
	Viewport * viewport = g_tree->tree_get_main_viewport();

	viewport->set_center_from_coord(wp->coord, true);
	this->waypoint_select(trw);
	trw->emit_layer_changed();
}




void WaypointListDialog::show_picture_waypoint_cb(void) /* Slot. */
{
	if (!this->selected_wp) {
		qDebug() << "EE: Waypoint List: encountered NULL Waypoint in callback" << __FUNCTION__;
		return;
	}

	Waypoint * wp = this->selected_wp;
	LayerTRW * trw = wp->get_parent_layer_trw();

	const QString viewer = Preferences::get_image_viewer();
	const QString quoted_path = Util::shell_quote(wp->image_full_path);
	const QString command = QString("%1 %2").arg(viewer).arg(quoted_path);

	if (!QProcess::startDetached(command)) {
		Dialog::error(QObject::QObject::tr("Could not launch viewer program '%1' to view file '%2'.").arg(viewer).arg(quoted_path), trw->get_window());
	}
}




typedef struct {
	bool has_layer_names;
	bool include_positions;
	QString str;
} copy_data_t;




#ifdef K_FIXME_RESTORE



/**
 * At the moment allow copying the data displayed** with or without the positions
 * (since the position data is not shown in the list but is useful in copying to external apps).
 *
 * ATM The visibility flag is not copied and neither is a text representation of the waypoint symbol.
 */
static void copy_selection(QStandardItemModel * model, GtkTreePath * path, GtkTreeIter * iter, void * data)
{
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
        gtk_tree_model_get(model, iter, WAYPOINT_COLUMN, &wp, -1);
	LatLon ll;
	if (wp) {
		ll = wp->coord.get_latlon();
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
}
#endif



void WaypointListDialog::copy_selected(bool include_positions)
{
	copy_data_t cd;

#ifdef K_FIXME_RESTORE
	QItemSelectionModel * selection = this->view.selectionModel();
	// NB GTK3 has gtk_tree_view_get_n_columns() but we're GTK2 ATM
	GList * gl = gtk_tree_view_get_columns(this->view);
	unsigned int count = g_list_length(gl);
	g_list_free(gl);
	cd.has_layer_names = (count > N_COLUMNS-3);
	cd.include_positions = include_positions;
	gtk_tree_selection_selected_foreach(selection, copy_selection, &cd);
#endif

	Pickle dummy;
	Clipboard::copy(ClipboardDataType::TEXT, LayerType::AGGREGATE, "", dummy, cd.str);
}





void WaypointListDialog::copy_selected_only_visible_columns_cb(void) /* Slot. */
{
	this->copy_selected(false);
}




void WaypointListDialog::copy_selected_with_position_cb(void) /* Slot. */
{
	this->copy_selected(true);
}




void WaypointListDialog::contextMenuEvent(QContextMenuEvent * ev)
{
	QPoint orig = ev->pos();
	QPoint v = this->view->pos();
	QPoint t = this->view->viewport()->pos();

	orig.setX(orig.x() - v.x() - t.x());
	orig.setY(orig.y() - v.y() - t.y());

	QPoint point = orig;
	QModelIndex index = this->view->indexAt(point);
	if (!index.isValid()) {
		qDebug() << "II" PREFIX << "context menu event: INvalid index";
		return;
	} else {
		qDebug() << "II" PREFIX << "context menu event: on index.row =" << index.row() << "index.column =" << index.column();
	}


	QStandardItem * parent_item = this->model->invisibleRootItem();


	QStandardItem * child = parent_item->child(index.row(), WAYPOINT_COLUMN);
	qDebug() << "II" PREFIX << "selected waypoint" << child->text();

	child = parent_item->child(index.row(), WAYPOINT_COLUMN);
	Waypoint * wp = child->data(RoleLayerData).value<Waypoint *>();
	if (!wp) {
		qDebug() << "EE" PREFIX << "failed to get non-NULL Waypoint from table";
		return;
	}

	/* If we were able to get list of Waypoints, all of them need to have associated parent layer. */
	LayerTRW * trw = wp->get_parent_layer_trw();
	if (!trw) {
		qDebug() << "EE" PREFIX << "failed to get non-NULL parent layer";
		return;
	}

	this->selected_wp = wp;



	QAction * qa = NULL;
	QMenu menu(this);
	/* When multiple rows are selected, the number of applicable operation is lower. */
	QItemSelectionModel * selection = this->view->selectionModel();
	if (selection->selectedRows(0).size() == 1) {

		qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), tr("&Zoom onto"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (waypoint_view_cb()));

		qa = menu.addAction(QIcon::fromTheme("document-properties"), tr("&Properties"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (waypoint_properties_cb()));

		qa = menu.addAction(QIcon::fromTheme("vik-icon-Show Picture"), tr("&Show Picture..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (show_picture_waypoint_cb()));
		qa->setEnabled(!wp->image_full_path.isEmpty());
	}

	qa = menu.addAction(QIcon::fromTheme("edit-copy"), tr("&Copy Data"));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (copy_selected_only_visible_columns_cb()));

	qa = menu.addAction(QIcon::fromTheme("edit-copy"), tr("Copy Data (with &positions)"));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (copy_selected_with_position_cb()));



	menu.exec(QCursor::pos());
	return;
}




/*
 * For each entry we copy the various individual waypoint properties into the table,
 * formatting & converting the internal values into something for display.
 */
void WaypointListDialog::add_row(Waypoint * wp, HeightUnit height_unit, const QString & date_format) /* TODO: verify that date_format is suitable for QDateTime class. */
{
	/* Get start date. */
	QString start_date;
	if (wp->has_timestamp) {
		QDateTime date_start;
		date_start.setTime_t(wp->timestamp);
		start_date = date_start.toString(date_format);
	}

	LayerTRW * trw = wp->get_parent_layer_trw();

	/* This parameter doesn't include aggegrate visibility. */
	bool visible = trw->visible && wp->visible;
	visible = visible && trw->get_waypoints_visibility();

	double alt = wp->altitude;
	switch (height_unit) {
	case HeightUnit::Metres: /* No need to convert. */
		break;
	case HeightUnit::Feet:
		alt = VIK_METERS_TO_FEET(alt);
		break;
	default:
		qDebug() << "EE" PREFIX << "invalid height unit" << (int) height_unit;
		break;
	}


	QList<QStandardItem *> items;
	QStandardItem * item = NULL;
	QVariant variant;
	QString tooltip(wp->description);

	/* TODO: add sorting by columns. Add reordering of columns. */


	/* LAYER_NAME_COLUMN */
	item = new QStandardItem(trw->name);
	item->setToolTip(tooltip);
	item->setEditable(false); /* This dialog is not a good place to edit layer name. */
	items << item;

	/* WAYPOINT_COLUMN */
	item = new QStandardItem(wp->name);
	item->setToolTip(tooltip);
	variant = QVariant::fromValue(wp);
	item->setData(variant, RoleLayerData);
	items << item;

	/* DATE_COLUMN */
	item = new QStandardItem(start_date);
	item->setToolTip(tooltip);
	items << item;

	/* VISIBLE_COLUMN */
	item = new QStandardItem();
	item->setToolTip(tooltip);
	item->setCheckable(true);
	item->setCheckState(visible ? Qt::Checked : Qt::Unchecked);
	items << item;

	/* COMMENT_COLUMN */
	item = new QStandardItem(wp->comment);
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
	item = new QStandardItem();
	item->setToolTip(tooltip);
	item->setIcon(get_wp_icon_small(wp->symbol_name));
	item->setEditable(false);
	items << item;

	this->model->invisibleRootItem()->appendRow(items);
}




/**
 * @hide_layer_names:  Don't show the layer names (first column of list) that each waypoint belongs to
 *
 * Create a table of waypoints with corresponding waypoint information.
 * This table does not support being actively updated.
 */
void WaypointListDialog::build_model(bool hide_layer_names)
{
	if (this->waypoints.empty()) {
		return;
	}

	const HeightUnit height_unit = Preferences::get_unit_height();

	this->model = new QStandardItemModel();
	this->model->setHorizontalHeaderItem(LAYER_NAME_COLUMN, new QStandardItem("Layer"));
	this->model->setHorizontalHeaderItem(WAYPOINT_COLUMN, new QStandardItem("Name"));
	this->model->setHorizontalHeaderItem(DATE_COLUMN, new QStandardItem("Date"));
	this->model->setHorizontalHeaderItem(VISIBLE_COLUMN, new QStandardItem("Visible"));
	this->model->setHorizontalHeaderItem(COMMENT_COLUMN, new QStandardItem("Comment"));
	switch (height_unit) {
	case HeightUnit::Metres:
		this->model->setHorizontalHeaderItem(HEIGHT_COLUMN, new QStandardItem("Height\n(Metres)"));
		break;
	case HeightUnit::Feet:
		this->model->setHorizontalHeaderItem(HEIGHT_COLUMN, new QStandardItem("Height\n(Feet)"));
		break;
	default:
		qDebug() << "EE" PREFIX << "invalid height unit" << (int) height_unit;
		break;
	}
	this->model->setHorizontalHeaderItem(SYMBOL_COLUMN, new QStandardItem("Symbol"));


	this->view = new QTableView();
	this->view->horizontalHeader()->setStretchLastSection(false);
	this->view->verticalHeader()->setVisible(false);
	this->view->setWordWrap(false);
	this->view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	this->view->setTextElideMode(Qt::ElideRight);
	this->view->setSelectionMode(QAbstractItemView::ExtendedSelection);
	this->view->setSelectionBehavior(QAbstractItemView::SelectRows);
	this->view->setShowGrid(false);
	this->view->setModel(this->model);
	this->view->show();
	this->view->setVisible(false);
	this->view->resizeRowsToContents();
	this->view->resizeColumnsToContents();
	this->view->setVisible(true);


	this->view->horizontalHeader()->setSectionHidden(LAYER_NAME_COLUMN, hide_layer_names);
	this->view->horizontalHeader()->setSectionResizeMode(LAYER_NAME_COLUMN, QHeaderView::Interactive);

	this->view->horizontalHeader()->setSectionHidden(WAYPOINT_COLUMN, false);
	this->view->horizontalHeader()->setSectionResizeMode(WAYPOINT_COLUMN, QHeaderView::Interactive);

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

	this->vbox->addWidget(this->view);
	this->vbox->addWidget(this->button_box);

	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox);

	connect(this->button_box, SIGNAL(accepted()), this, SLOT(accept()));



	QString date_format;
	if (!ApplicationState::get_string(VIK_SETTINGS_LIST_DATE_FORMAT, date_format)) {
		date_format = WAYPOINT_LIST_DATE_FORMAT;
	}

	for (auto iter = waypoints.begin(); iter != waypoints.end(); iter++) {
		this->add_row(*iter, height_unit, date_format);
	}

	/* TODO: add initial sorting by layer name or waypoint name. */
#ifdef K_TODO
	if (hide_layer_name) {
		sort by waypoint name;
	} else {
		sort by layer name;
	}
#endif

	this->setMinimumSize(700, 400);
}




/**
   @title: the title for the dialog
   @layer: the layer, from which a list of waypoints should be extracted

   Common method for showing a list of waypoints with extended information
*/
void SlavGPS::waypoint_list_dialog(QString const & title, Layer * layer)
{
	WaypointListDialog dialog(title, layer->get_window());

	dialog.waypoints.clear();

	if (layer->type == LayerType::TRW) {
		((LayerTRW *) layer)->get_waypoints_list(dialog.waypoints);
	} else if (layer->type == LayerType::AGGREGATE) {
		((LayerAggregate *) layer)->get_waypoints_list(dialog.waypoints);
	} else {
		assert (0);
	}

	dialog.build_model(layer->type != LayerType::AGGREGATE);
	dialog.exec();
}




WaypointListDialog::WaypointListDialog(QString const & title, QWidget * parent_widget) : QDialog(parent_widget)
{
	this->setWindowTitle(title);

	this->button_box = new QDialogButtonBox();
	this->parent = parent_widget;
	this->button_box->addButton("&Close", QDialogButtonBox::AcceptRole);
	connect(this->button_box, &QDialogButtonBox::accepted, this, &WaypointListDialog::accept_cb);
	this->vbox = new QVBoxLayout;
}




WaypointListDialog::~WaypointListDialog()
{
}




/* Here we save in track objects changes made in the dialog. */
void WaypointListDialog::accept_cb(void) /* Slot. */
{
	/* FIXME: check and make sure the waypoint still exists before doing anything to it. */

	if (this->selected_wp) {
		LayerTRW * trw = this->selected_wp->get_parent_layer_trw();
		trw->waypoints->update_tree_view(this->selected_wp);
		trw->emit_layer_changed();
	}

	this->accept();
}
