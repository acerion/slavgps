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

#include <cassert>

#include <QHeaderView>
#include <QDebug>
#include <QDateTime>

#include "layer.h"
#include "layer_trw.h"
#include "layer_trw_track_list_dialog.h"
#include "layer_trw_track_internal.h"
#include "layer_trw_track_properties_dialog.h"
#include "application_state.h"
#include "globals.h"
#include "window.h"
#include "vikutils.h"
#include "util.h"
#include "preferences.h"
#include "tree_view_internal.h"
#include "layer_aggregate.h"
#include "clipboard.h"




using namespace SlavGPS;




/* Long formatted date+basic time - listing this way ensures the string comparison sort works - so no local type format %x or %c here! */
#define TRACK_LIST_DATE_FORMAT "%Y-%m-%d %H:%M"




enum {
	LAYER_NAME_COLUMN,
	TRACK_COLUMN,
	DATE_COLUMN,
	VISIBLE_COLUMN,
	COMMENT_COLUMN,
	LENGTH_COLUMN,
	DURATION_COLUMN,
	AVERAGE_SPEED_COLUMN,
	MAXIMUM_SPEED_COLUMN,
	MAXIMUM_HEIGHT_COLUMN,
};





/* Instead of hooking automatically on tree view item selection,
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
	if (!this->selected_track) {
		qDebug() << "EE: Track List: encountered NULL Track in callback" << __FUNCTION__;
		return;
	}

	Track * trk = this->selected_track;
	LayerTRW * trw = trk->get_parent_layer_trw();

	if (!trk->name.isEmpty()) {

		/* Kill off this dialog to allow interaction with properties window.
		   Since the properties also allows track manipulations it won't cause conflicts here. */
		this->accept();
		track_properties_dialog(trw->get_window(), trk, true);
	}
}




void TrackListDialog::track_view_cb(void)
{
	if (!this->selected_track) {
		qDebug() << "EE: Track List: encountered NULL Track in callback" << __FUNCTION__;
		return;
	}

	Track * trk = this->selected_track;
	LayerTRW * trw = trk->get_parent_layer_trw();
	Viewport * viewport = trw->get_window()->get_viewport();

	// TODO create common function to convert between LatLon[2] and LatLonBBox or even change LatLonBBox to be 2 LatLons!
	LatLon maxmin[2];
	maxmin[0].lat = trk->bbox.north;
	maxmin[1].lat = trk->bbox.south;
	maxmin[0].lon = trk->bbox.east;
	maxmin[1].lon = trk->bbox.west;

	trw->zoom_to_show_latlons(viewport, maxmin);

	this->track_select(trw, trk);
}



typedef struct {
	bool has_layer_names;
	QString str;
} copy_data_t;



#ifdef K




static void copy_selection(QStandardItemModel * model, GtkTreePath * path, GtkTreeIter * iter,  void * data)
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
	copy_data_t cd;
#ifdef K
	GtkTreeSelection * selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
	// NB GTK3 has gtk_tree_view_get_n_columns() but we're GTK2 ATM
	GList * gl = gtk_tree_view_get_columns(GTK_TREE_VIEW(tree_view));
	unsigned int count = g_list_length(gl);
	g_list_free(gl);
	cd.has_layer_names = (count > TRK_LIST_COLS - 3);

	gtk_tree_selection_selected_foreach(selection, copy_selection, &cd);
#endif
	Clipboard::copy(ClipboardDataType::TEXT, LayerType::AGGREGATE, "", 0, cd.str, NULL);
}




void TrackListDialog::add_copy_menu_item(QMenu & menu)
{
	QAction * qa = menu.addAction(QIcon::fromTheme("edit-copy"), tr("&Copy Data"));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (copy_selected_cb()));
}




void TrackListDialog::add_menu_items(QMenu & menu)
{
	QAction * qa = NULL;

#if 0   /* OLD COMMENT: ATM view auto selects, so don't bother with separate select menu entry. */
	qa = menu.addAction(QIcon::fromTheme("edit-find"), tr("&Select"));
	/* The callback worked by exposing selected item in tree view. */
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_select_cb()));
#endif

	qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), tr("&View"));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_view_cb()));

	qa = menu.addAction(tr("&Statistics"));
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


	QStandardItem * child = parent_item->child(index.row(), TRACK_COLUMN);
	qDebug() << "II: Track List: selected track" << child->text();

	child = parent_item->child(index.row(), TRACK_COLUMN);
	Track * trk = child->data(RoleLayerData).value<Track *>();
	if (!trk) {
		qDebug() << "EE: Track List Dialog: null track in context menu handler";
		return;
	}

	/* If we were able to get list of Tracks, all of them need to have associated parent layer. */
	LayerTRW * trw = trk->get_parent_layer_trw();
	if (!trw) {
		qDebug() << "EE: Track List: failed to get non-NULL parent layer @" << __FUNCTION__ << __LINE__;
		return;
	}

	this->selected_track = trk;

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




/*
 * Foreach entry we copy the various individual track properties into the tree store
 * formatting & converting the internal values into something for display.
 */
void TrackListDialog::add_row(Track * trk, DistanceUnit distance_unit, SpeedUnit speed_units, HeightUnit height_units, const QString & date_format) /* TODO: verify that date_format is suitable for QDateTime class. */
{
	double trk_dist = trk->get_length();
	/* Store unit converted value. */
	trk_dist = convert_distance_meters_to(trk_dist, distance_unit);

	/* Get start date. */
	QString start_date;
	if (!trk->empty()
	    && (*trk->trackpoints.begin())->has_timestamp) {

		QDateTime date_start;
		date_start.setTime_t((*trk->trackpoints.begin())->timestamp);
		start_date = date_start.toString(date_format);
	}

	LayerTRW * trw = trk->get_parent_layer_trw();

	/* 'visible' doesn't include aggegrate visibility. */
	bool visible = trw->visible && trk->visible;
	visible = visible && (trk->type_id == "sg.trw.route" ? trw->get_routes_visibility() : trw->get_tracks_visibility());

	unsigned int trk_duration = 0; /* In minutes. */
	if (!trk->empty()) {
		time_t t1 = (*trk->trackpoints.begin())->timestamp;
		time_t t2 = (*std::prev(trk->trackpoints.end()))->timestamp;
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
	if (!trk->comment.isEmpty()) {
		tooltip = trk->comment;
	} else if (!trk->description.isEmpty()) {
		tooltip = trk->description;
	} else {
		;
	}

	/* TODO: Add reordering of columns? */


	/* LAYER_NAME_COLUMN */
	item = new QStandardItem(trw->name);
	item->setToolTip(tooltip);
	item->setEditable(false); /* This dialog is not a good place to edit layer name. */
	items << item;

	/* TRACK_COLUMN */
	item = new QStandardItem(trk->name);
	item->setToolTip(tooltip);
	variant = QVariant::fromValue(trk);
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
	item = new QStandardItem(trk->comment);
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

	this->model->invisibleRootItem()->appendRow(items);
}




void TrackListDialog::build_model(bool hide_layer_names)
{
	DistanceUnit distance_units = Preferences::get_unit_distance();
	SpeedUnit speed_units = Preferences::get_unit_speed();
	HeightUnit height_units = Preferences::get_unit_height();


	this->model = new QStandardItemModel();
	this->model->setHorizontalHeaderItem(LAYER_NAME_COLUMN, new QStandardItem("Layer"));
	this->model->setHorizontalHeaderItem(TRACK_COLUMN, new QStandardItem("Track Name"));
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
	this->model->setHorizontalHeaderItem(AVERAGE_SPEED_COLUMN, new QStandardItem(tr("Average Speed\n(%1)").arg(speed_units_string))); /* Viking was using %.1f printf() format. */
	this->model->setHorizontalHeaderItem(MAXIMUM_SPEED_COLUMN, new QStandardItem(tr("Maximum Speed\n(%1)").arg(speed_units_string))); /* Viking was using %.1f printf() format. */

	if (height_units == HeightUnit::FEET) {
		this->model->setHorizontalHeaderItem(MAXIMUM_HEIGHT_COLUMN, new QStandardItem("Maximum Height\n(Feet)"));
	} else {
		this->model->setHorizontalHeaderItem(MAXIMUM_HEIGHT_COLUMN, new QStandardItem("Maximum Height\n(Metres)"));
	}


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

	this->view->horizontalHeader()->setSectionHidden(TRACK_COLUMN, false);
	this->view->horizontalHeader()->setSectionResizeMode(TRACK_COLUMN, QHeaderView::Interactive);

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

	this->view->horizontalHeader()->setStretchLastSection(true);

	this->vbox->addWidget(this->view);
	this->vbox->addWidget(this->button_box);

	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox);

	connect(this->button_box, SIGNAL(accepted()), this, SLOT(accept()));



	QString date_format;
	if (!ApplicationState::get_string(VIK_SETTINGS_LIST_DATE_FORMAT, date_format)) {
		date_format = TRACK_LIST_DATE_FORMAT;
	}

	for (auto iter = this->tracks->begin(); iter != this->tracks->end(); iter++) {
		this->add_row(*iter, distance_units, speed_units, height_units, date_format);
	}


	/* Notice that we enable and perform sorting after adding all items in the for() loop. */
	this->view->setSortingEnabled(true);
	if (hide_layer_names) {
		this->view->sortByColumn(TRACK_COLUMN, Qt::AscendingOrder);
	} else {
		this->view->sortByColumn(LAYER_NAME_COLUMN, Qt::AscendingOrder);
	}

	this->setMinimumSize(750, 400);

#ifdef K
	/* TODO: The callback worked by exposing selected item in tree view. */
	QObject::connect(gtk_tree_view_get_selection (GTK_TREE_VIEW(view)), SIGNAL("changed"), view, SLOT (track_select_cb));

	/* TODO: Maybe add full menu of a Track class in the table view too. */
#endif
}




/**
   @title: the title for the dialog
   @layer: The layer, from which a list of tracks should be extracted
   @type_id: TreeItem type to be show in list (empty string for both tracks and layers)

  Common method for showing a list of tracks with extended information
*/
void SlavGPS::track_list_dialog(QString const & title, Layer * layer, const QString & type_id)
{
	TrackListDialog dialog(title, layer->get_window());


	if (layer->type == LayerType::AGGREGATE) {
		if (type_id == "") { /* No particular sublayer type means both tracks and routes. */
			dialog.tracks = ((LayerAggregate *) layer)->create_tracks_list();
		} else {
			dialog.tracks = ((LayerAggregate *) layer)->create_tracks_list(type_id);
		}
	} else if (layer->type == LayerType::TRW) {
		if (type_id == "") { /* No particular sublayer type means both tracks and routes. */
			dialog.tracks = ((LayerTRW *) layer)->create_tracks_list();
		} else {
			dialog.tracks = ((LayerTRW *) layer)->create_tracks_list(type_id);
		}
	} else {
		assert (0);
	}

	dialog.build_model(layer->type != LayerType::AGGREGATE);
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
	delete this->tracks;
}




void TrackListDialog::accept_cb(void) /* Slot. */
{
	/* FIXME: check and make sure the track still exists before doing anything to it. */
#ifdef K

	/* Here we save in track objects changes made in the dialog. */

	trk->set_comment(this->w_comment->text());

	this->trw->update_tree_view(this->trk);
	this->trw->emit_layer_changed();
#endif

	this->accept();
}
