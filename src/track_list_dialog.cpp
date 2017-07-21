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
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cassert>

#include "track_list_dialog.h"
#include "track_properties_dialog.h"
#include "clipboard.h"
#include "settings.h"
#include "globals.h"
#include "window.h"
#include "vikutils.h"
#include "util.h"
#include "preferences.h"
#include "tree_view_internal.h"
#include "layer.h"
#include "layer_trw.h"
#include "layer_aggregate.h"




using namespace SlavGPS;




/* Long formatted date+basic time - listing this way ensures the string comparison sort works - so no local type format %x or %c here! */
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




/*
static void track_select_cb(GtkTreeSelection * selection, void * data)
{
	GtkTreeIter iter;
	if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		return;
	}

	GtkTreeView * tree_view = GTK_TREE_VIEW (data);
	QStandardItemModel * model = gtk_tree_view_get_model(tree_view);

	Track * trk;
	gtk_tree_model_get(model, &iter, TRACK_POINTER_COLUMN, &trk, -1);
	if (!trk) {
		return;
	}

	LayerTRW * trw;
	gtk_tree_model_get(model, &iter, LAYER_POINTER_COLUMN, &trw, -1);
	if (trw->type != LayerType::TRW) {
		return;
	}

	//vik_treeview_select_iter(trw->vt, g_hash_table_lookup(trw->track_iters, uuid), true);
}
*/




/* Instead of hooking automatically on treeview item selection,
   this is performed on demand via the specific menu request. */
void TrackListDialog::track_select(LayerTRW * trw, Track * trk)
{
	if (!trw || !trk) {
		qDebug() << "EE: Track List Dialog: layer or track is NULL:" << (qintptr) trw << (qintptr) trk;
		return;
	}

	trw->tree_view->select_and_expose(trk->index);
}




void TrackListDialog::track_stats_cb(void)
{
	LayerTRW * trw = this->menu_data.trw;
	Track * trk = this->menu_data.trk;
	Viewport * viewport = this->menu_data.viewport;

	if (trk && !trk->name.isEmpty()) {

		/* Kill off this dialog to allow interaction with properties window.
		   Since the properties also allows track manipulations it won't cause conflicts here. */
		this->accept();
		track_properties_dialog(trw->get_window(), trw, trk, true);
	}
}




void TrackListDialog::track_view_cb(void)
{
	LayerTRW * trw = this->menu_data.trw;
	Track * trk = this->menu_data.trk;
	Viewport * viewport = this->menu_data.viewport;


	// TODO create common function to convert between LatLon[2] and LatLonBBox or even change LatLonBBox to be 2 LatLons!
	struct LatLon maxmin[2];
	maxmin[0].lat = trk->bbox.north;
	maxmin[1].lat = trk->bbox.south;
	maxmin[0].lon = trk->bbox.east;
	maxmin[1].lon = trk->bbox.west;

	trw->zoom_to_show_latlons(viewport, maxmin);

	this->track_select(trw, trk);
}




#ifdef K
typedef struct {
	bool has_layer_names;
	GString * str;
} copy_data_t;




static void copy_selection(QStandardItemModel * model,
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
#endif




void TrackListDialog::copy_selected_cb(void)
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




void TrackListDialog::add_copy_menu_item(QMenu & menu)
{
	QAction * qa = menu.addAction(QIcon::fromTheme("edit-copy"), QString(_("&Copy Data")));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (copy_selected_cb()));
}




void TrackListDialog::add_menu_items(QMenu & menu)
{
	QAction * qa = NULL;

#if 0   /* OLD COMMENT: ATM view auto selects, so don't bother with separate select menu entry. */
	qa = menu.addAction(QIcon::fromTheme("edit-find"), QString("&Select"));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_select_cb()));
#endif

	qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), QString(_("&View")));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_view_cb()));

	qa = menu.addAction(QString(_("&Statistics")));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_stats_cb()));

	this->add_copy_menu_item(menu);

	return;
}




void TrackListDialog::contextMenuEvent(QContextMenuEvent * ev)
{
	QPoint orig = ev->pos();
	QPoint v = this->view->pos();
	QPoint t = this->view->viewport()->pos();

	orig.setX(orig.x() - v.x() - t.x());
	orig.setY(orig.y() - v.y() - t.y());

	QPoint point = orig;
	QModelIndex index = this->view->indexAt(point);
	if (!index.isValid()) {
		qDebug() << "II: Track List: context menu event: INvalid index";
		return;
	} else {
		qDebug() << "II: Track List: context menu event: on index.row =" << index.row() << "index.column =" << index.column();
	}


	QStandardItem * parent_item = this->model->invisibleRootItem();


	QStandardItem * child = parent_item->child(index.row(), TRACK_NAME_COLUMN);
	qDebug() << "II: Track List: selected track" << child->text();

	child = parent_item->child(index.row(), TRACK_POINTER_COLUMN);
	Track * trk = child->data(RoleLayerData).value<Track *>();
	if (!trk) {
		qDebug() << "EE: Track List Dialog: null track in context menu handler";
		return;
	}

	child = parent_item->child(index.row(), LAYER_POINTER_COLUMN);
	LayerTRW * trw = (LayerTRW *) child->data(RoleLayerData).value<Layer *>();
	if (trw->type != LayerType::TRW) {
		qDebug() << "EE: Track List: layer type is not TRW:" << (int) trw->type;
		return;
	}


	this->menu_data.trw = trw;
	this->menu_data.trk = trk;
	this->menu_data.viewport = trw->get_window()->get_viewport();

	QMenu menu(this);
#if 0
	/* When multiple rows are selected, the number of applicable operation is lower. */
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW (tree_view));
	if (gtk_tree_selection_count_selected_rows (selection) != 1) {
		this->add_copy_menu_items(QMenu & menu);
	}

	this->add_copy_menu_item(menu);
#else
	this->add_menu_items(menu);
#endif
	menu.exec(QCursor::pos());
	return;
}




#if 0
static bool trw_layer_track_button_pressed_cb(GtkWidget * tree_view,
					      GdkEventButton * ev,
					      void * tracks_and_layers)
{
	/* Only on right clicks... */
	if (!(event->type == GDK_BUTTON_PRESS && event->button == MouseButton::RIGHT)) {
		return false;
	}

	/* ATM Force a selection... */
	GtkTreeSelection * selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
	if (gtk_tree_selection_count_selected_rows(selection) <= 1) {
		GtkTreePath * path;
		/* Get tree path for row that was clicked. */
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
#endif




/*
 * Foreach entry we copy the various individual track properties into the tree store
 * formatting & converting the internal values into something for display.
 */
void TrackListDialog::add(Track * trk, LayerTRW * trw, DistanceUnit distance_unit, SpeedUnit speed_units, HeightUnit height_units, char const * date_format)
{
	double trk_dist = trk->get_length();
	/* Store unit converted value. */
	trk_dist = convert_distance_meters_to(trk_dist, distance_unit);

	/* Get start date. */
	char time_buf[32];
	time_buf[0] = '\0';
	if (!trk->empty()
	    && (*trk->trackpointsB->begin())->has_timestamp) {
#ifdef K
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
#endif
	}

	/* 'visible' doesn't include aggegrate visibility. */
	bool visible = trw->visible && trk->visible;
	visible = visible && (trk->sublayer_type == SublayerType::ROUTE ? trw->get_routes_visibility() : trw->get_tracks_visibility());

	unsigned int trk_duration = 0; /* In minutes. */
	if (!trk->empty()) {
		time_t t1 = (*trk->trackpointsB->begin())->timestamp;
		time_t t2 = (*std::prev(trk->trackpointsB->end()))->timestamp;
		trk_duration = (int) round(labs(t2 - t1) / 60.0);
	}

	double av_speed = trk->get_average_speed();
	av_speed = convert_speed_mps_to(av_speed, speed_units);

	double max_speed = trk->get_max_speed();
	max_speed = convert_speed_mps_to(max_speed, speed_units);

	double max_alt = 0.0;
	/* TODO - make this a function to get min / max values? */
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
		/* HeightUnit::METRES: no need to convert. */
		break;
	}

	QList<QStandardItem *> items;
	QStandardItem * item = NULL;
	QVariant variant;

	QString tooltip("");
	if (trk->comment) {
		tooltip = QString(trk->comment);
	} else if (trk->description) {
		tooltip = QString(trk->description);
	} else {
		;
	}

	/* TODO: Add reordering of columns? */


	/* LAYER_NAME_COLUMN */
	item = new QStandardItem(trw->name);
	item->setToolTip(tooltip);
	item->setEditable(false); /* This dialog is not a good place to edit layer name. */
	items << item;

	/* TRACK_NAME_COLUMN */
	item = new QStandardItem(trk->name);
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
	item = new QStandardItem(QString(trk->comment));
	item->setToolTip(tooltip);
	items << item;

	/* LENGTH_COLUMN */
	item = new QStandardItem();
	item->setToolTip(tooltip);
	variant = QVariant::fromValue(trk_dist);
	item->setData(variant, Qt::DisplayRole);
	item->setEditable(false); /* This dialog is not a good place to edit track length. */
	items << item;

	/* DURATION_COLUMN */
	item = new QStandardItem();
	item->setToolTip(tooltip);
	variant = QVariant::fromValue(trk_duration);
	item->setData(variant, Qt::DisplayRole);
	item->setEditable(false); /* This dialog is not a good place to edit track duration. */
	items << item;

	/* AVERAGE_SPEED_COLUMN */
	item = new QStandardItem();
	item->setToolTip(tooltip);
	variant = QVariant::fromValue(av_speed);
	item->setData(variant, Qt::DisplayRole);
	item->setEditable(false); /* 'Average speed' is not an editable parameter. */
	items << item;

	/* MAXIMUM_SPEED_COLUMN */
	item = new QStandardItem();
	item->setToolTip(tooltip);
	variant = QVariant::fromValue(max_speed);
	item->setData(variant, Qt::DisplayRole);
	item->setEditable(false); /* 'Maximum speed' is not an editable parameter. */
	items << item;

	/* MAXIMUM_HEIGHT_COLUMN */
	item = new QStandardItem();
	item->setToolTip(tooltip);
	variant = QVariant::fromValue(max_alt);
	item->setData(variant, Qt::DisplayRole);
	item->setEditable(false); /* 'Maximum height' is not an editable parameter. */
	items << item;

	/* LAYER_POINTER_COLUMN */
	item = new QStandardItem();
	variant = QVariant::fromValue((Layer *) trw);
	item->setData(variant, RoleLayerData);
	items << item;

	/* TRACK_POINTER_COLUMN */
	item = new QStandardItem();
	variant = QVariant::fromValue(trk);
	item->setData(variant, RoleLayerData);
	items << item;

	this->model->invisibleRootItem()->appendRow(items);
}




void TrackListDialog::build_model(bool hide_layer_names)
{
	DistanceUnit distance_units = Preferences::get_unit_distance();
	SpeedUnit speed_units = Preferences::get_unit_speed();
	HeightUnit height_units = Preferences::get_unit_height();


	this->model = new QStandardItemModel();
	this->model->setHorizontalHeaderItem(LAYER_NAME_COLUMN, new QStandardItem("Layer"));
	this->model->setHorizontalHeaderItem(TRACK_NAME_COLUMN, new QStandardItem("Track Name"));
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

	const QString speed_units_string = get_speed_unit_string(speed_units);
	this->model->setHorizontalHeaderItem(AVERAGE_SPEED_COLUMN, new QStandardItem(QString("Average Speed\n(%1)").arg(speed_units_string))); /* Viking was using %.1f printf() format. */
	this->model->setHorizontalHeaderItem(MAXIMUM_SPEED_COLUMN, new QStandardItem(QString("Maximum Speed\n(%1)").arg(speed_units_string))); /* Viking was using %.1f printf() format. */

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
	this->view->horizontalHeader()->setSectionResizeMode(COMMENT_COLUMN, QHeaderView::Interactive);

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

	this->view->horizontalHeader()->setStretchLastSection(true);

	this->vbox->addWidget(this->view);
	this->vbox->addWidget(this->button_box);

	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox);

	connect(this->button_box, SIGNAL(accepted()), this, SLOT(accept()));



	char * date_format = NULL;
	if (!a_settings_get_string(VIK_SETTINGS_LIST_DATE_FORMAT, &date_format)) {
		date_format = g_strdup(TRACK_LIST_DATE_FORMAT);
	}

	for (auto iter = tracks_and_layers->begin(); iter != tracks_and_layers->end(); iter++) {
		this->add((*iter)->trk, (*iter)->trw, distance_units, speed_units, height_units, date_format);
	}
	free(date_format);


	/* Notice that we enable and perform sorting after adding all items in the for() loop. */
	this->view->setSortingEnabled(true);
	if (hide_layer_names) {
		this->view->sortByColumn(TRACK_NAME_COLUMN, Qt::AscendingOrder);
	} else {
		this->view->sortByColumn(LAYER_NAME_COLUMN, Qt::AscendingOrder);
	}

	this->setMinimumSize(750, 400);

#ifdef K
	//QObject::connect(gtk_tree_view_get_selection (GTK_TREE_VIEW(view)), SIGNAL("changed"), view, SLOT (track_select_cb));
	QObject::connect(view, SIGNAL("popup-menu"), tracks_and_layers, SLOT (trw_layer_track_menu_popup));
	QObject::connect(view, SIGNAL("button-press-event"), tracks_and_layers, SLOT (trw_layer_track_button_pressed_cb));
#endif
}




/**
 * @title:               The title for the dialog
 * @layer:               The #Layer passed on into get_tracks_and_layers_cb()
 * @sublayer_typea:      Sublayer type to be show in list (NONE for both TRACKS and LAYER)
 * @show_layer_names:    Normally only set when called from an aggregate level
 *
 * Common method for showing a list of tracks with extended information
 */
void SlavGPS::track_list_dialog(QString const & title, Layer * layer, SublayerType sublayer_type, bool show_layer_names)
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




TrackListDialog::TrackListDialog(QString const & title, QWidget * parent_widget) : QDialog(parent_widget)
{
	this->setWindowTitle(title);

	this->button_box = new QDialogButtonBox();
	this->parent = parent_widget;

	this->button_box->addButton("&Save and Close", QDialogButtonBox::AcceptRole);
	this->button_box->addButton("&Cancel", QDialogButtonBox::RejectRole);
	connect(this->button_box, &QDialogButtonBox::accepted, this, &TrackListDialog::accept_cb);
	connect(this->button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

	this->vbox = new QVBoxLayout;
}




TrackListDialog::~TrackListDialog()
{
	delete this->tracks_and_layers;
}




void TrackListDialog::accept_cb(void) /* Slot. */
{
	/* FIXME: check and make sure the track still exists before doing anything to it. */
#ifdef K

	/* Here we save in track objects changes made in the dialog. */

	trk->set_comment(this->w_comment->text().toUtf8().data());

	this->trw->update_treeview(this->trk);
	this->trw->emit_changed();
#endif

	this->accept();
}
