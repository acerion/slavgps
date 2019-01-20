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
#include "window.h"
#include "vikutils.h"
#include "util.h"
#include "preferences.h"
#include "tree_view_internal.h"
#include "layer_aggregate.h"
#include "clipboard.h"
#include "viewport_internal.h"




using namespace SlavGPS;




#define SG_MODULE "Layer TRW Track List Dialog"




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





void TrackListDialog::track_properties_cb(void)
{
	if (!this->selected_track) {
		qDebug() << SG_PREFIX_E << "Encountered NULL Track in callback";
		return;
	}

	Track * trk = this->selected_track;
	LayerTRW * trw = trk->get_parent_layer_trw();

	if (!trk->name.isEmpty()) {

		/* Kill off this dialog to allow interaction with properties window.
		   Since the properties also allows track manipulations it won't cause conflicts here. */
		this->accept();
		track_properties_dialog(trk, trw->get_window());
	}
}




void TrackListDialog::track_statistics_cb(void)
{
	if (!this->selected_track) {
		qDebug() << SG_PREFIX_E << "Encountered NULL Track in callback";
		return;
	}

	Track * trk = this->selected_track;
	LayerTRW * trw = trk->get_parent_layer_trw();

	if (!trk->name.isEmpty()) {

		/* Kill off this dialog to allow interaction with properties window.
		   Since the properties also allows track manipulations it won't cause conflicts here. */
		this->accept();
		track_statistics_dialog(trk, trw->get_window());
	}
}




void TrackListDialog::track_view_cb(void)
{
	if (!this->selected_track) {
		qDebug() << SG_PREFIX_E << "Encountered NULL selected Track in callback";
		return;
	}

	Track * trk = this->selected_track;
	LayerTRW * trw = trk->get_parent_layer_trw();
	Viewport * viewport = trw->get_window()->get_viewport();

	viewport->set_bbox(trk->get_bbox());
	trw->tree_view->select_and_expose_tree_item(trk);
	viewport->request_redraw("Re-align viewport to show whole contents of Track");
}



typedef struct {
	bool has_layer_names;
	QString str;
} copy_data_t;



#ifdef K_FIXME_RESTORE




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

#ifdef K_FIXME_RESTORE
	QItemSelectionModel * selection = tree_view.selectionModel();
	// NB GTK3 has gtk_tree_view_get_n_columns() but we're GTK2 ATM
	GList * gl = gtk_tree_view_get_columns(GTK_TREE_VIEW(tree_view));
	unsigned int count = g_list_length(gl);
	g_list_free(gl);
	cd.has_layer_names = (count > TRK_LIST_COLS - 3);

	gtk_tree_selection_selected_foreach(selection, copy_selection, &cd);
#endif
	Pickle dummy;
	Clipboard::copy(ClipboardDataType::Text, LayerType::Aggregate, "", dummy, cd.str);
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
		qDebug() << SG_PREFIX_I << "context menu event: INvalid index";
		return;
	} else {
		qDebug() << SG_PREFIX_I << "context menu event: on index.row =" << index.row() << "index.column =" << index.column();
	}


	QStandardItem * parent_item = this->model->invisibleRootItem();


	QStandardItem * child = parent_item->child(index.row(), TRACK_COLUMN);
	qDebug() << SG_PREFIX_I << "selected track" << child->text();

	Track * trk = child->data(RoleLayerData).value<Track *>();
	if (!trk) {
		qDebug() << SG_PREFIX_E << "NULL track in context menu handler";
		return;
	}

	/* If we were able to get list of Tracks, all of them need to have associated parent layer. */
	LayerTRW * trw = trk->get_parent_layer_trw();
	if (!trw) {
		qDebug() << SG_PREFIX_E << "Failed to get non-NULL parent layer @" << __FUNCTION__ << __LINE__;
		return;
	}

	this->selected_track = trk;



	QMenu menu(this);

	/* When multiple rows are selected, the number of applicable operation is lower. */
	QAction * qa = NULL;
	QItemSelectionModel * selection = this->view->selectionModel();
	if (selection->selectedRows(0).size() == 1) {

		qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), tr("&Show in Tree View"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_view_cb()));

		qa = menu.addAction(tr("&Statistics"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_statistics_cb()));

		qa = menu.addAction(tr("&Properties"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_properties_cb()));
	}

	qa = menu.addAction(QIcon::fromTheme("edit-copy"), tr("&Copy Data"));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (copy_selected_cb()));

	menu.exec(QCursor::pos());
	return;
}




/*
 * Foreach entry we copy the various individual track properties into the tree store
 * formatting & converting the internal values into something for display.
 */
void TrackListDialog::add_row(Track * trk, DistanceUnit distance_unit, SpeedUnit speed_unit, HeightUnit height_unit)
{
	const Distance trk_dist = trk->get_length().convert_to_unit(distance_unit);

	/* Get start date. */
	QString start_date;
	if (!trk->empty()
	    && (*trk->trackpoints.begin())->timestamp.is_valid()) {

		start_date = (*trk->trackpoints.begin())->timestamp.get_time_string(this->date_time_format);
	}

	LayerTRW * trw = trk->get_parent_layer_trw();

	/* 'visible' doesn't include aggegrate visibility. */
	bool visible = trw->visible && trk->visible;
	visible = visible && (trk->is_route() ? trw->get_routes_visibility() : trw->get_tracks_visibility());


	const Time trk_duration = trk->get_duration();


	Altitude max_alt(0.0, HeightUnit::Metres);
	TrackData altitudes = trk->make_track_data_altitude_over_distance(500);
	if (altitudes.valid) {
		altitudes.calculate_min_max();
		max_alt.set_value(altitudes.y_max);
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
	variant = QVariant::fromValue(trk_dist.value);
	item->setData(variant, Qt::DisplayRole);
	item->setEditable(false); /* This dialog is not a good place to edit track length. */
	items << item;

	/* DURATION_COLUMN */
	item = new QStandardItem();
	item->setToolTip(tooltip);
	variant = QVariant::fromValue(trk_duration.to_duration_string());
	item->setData(variant, Qt::DisplayRole);
	item->setEditable(false); /* This dialog is not a good place to edit track duration. */
	items << item;

	/* AVERAGE_SPEED_COLUMN */
	item = new QStandardItem();
	item->setToolTip(tooltip);
	variant = QVariant::fromValue(trk->get_average_speed().convert_to_unit(speed_unit).to_string());
	item->setData(variant, Qt::DisplayRole);
	item->setEditable(false); /* 'Average speed' is not an editable parameter. */
	items << item;

	/* MAXIMUM_SPEED_COLUMN */
	item = new QStandardItem();
	item->setToolTip(tooltip);
	variant = QVariant::fromValue(trk->get_max_speed().convert_to_unit(speed_unit).to_string());
	item->setData(variant, Qt::DisplayRole);
	item->setEditable(false); /* 'Maximum speed' is not an editable parameter. */
	items << item;

	/* MAXIMUM_HEIGHT_COLUMN */
	item = new QStandardItem();
	item->setToolTip(tooltip);
	variant = QVariant::fromValue(max_alt.convert_to_unit(height_unit).to_string());
	item->setData(variant, Qt::DisplayRole);
	item->setEditable(false); /* 'Maximum height' is not an editable parameter. */
	items << item;

	this->model->invisibleRootItem()->appendRow(items);
}




void TrackListDialog::build_model(bool hide_layer_names)
{
	const DistanceUnit distance_unit = Preferences::get_unit_distance();
	const SpeedUnit speed_unit = Preferences::get_unit_speed();
	const HeightUnit height_unit = Preferences::get_unit_height();


	this->model = new QStandardItemModel();
	this->model->setHorizontalHeaderItem(LAYER_NAME_COLUMN,     new QStandardItem(tr("Layer")));
	this->model->setHorizontalHeaderItem(TRACK_COLUMN,          new QStandardItem(tr("Track Name")));
	this->model->setHorizontalHeaderItem(DATE_COLUMN,           new QStandardItem(tr("Date")));
	this->model->setHorizontalHeaderItem(VISIBLE_COLUMN,        new QStandardItem(tr("Visible")));
	this->model->setHorizontalHeaderItem(COMMENT_COLUMN,        new QStandardItem(tr("Comment")));
	this->model->setHorizontalHeaderItem(LENGTH_COLUMN,         new QStandardItem(tr("Length\n(%1)").arg(Distance::get_unit_full_string(distance_unit))));
	this->model->setHorizontalHeaderItem(DURATION_COLUMN,       new QStandardItem(tr("Duration\n(minutes)")));
	this->model->setHorizontalHeaderItem(AVERAGE_SPEED_COLUMN,  new QStandardItem(tr("Average Speed\n(%1)").arg(Speed::get_unit_string(speed_unit))));
	this->model->setHorizontalHeaderItem(MAXIMUM_SPEED_COLUMN,  new QStandardItem(tr("Maximum Speed\n(%1)").arg(Speed::get_unit_string(speed_unit)))); /* Viking was using %.1f printf() format. */
	this->model->setHorizontalHeaderItem(MAXIMUM_HEIGHT_COLUMN, new QStandardItem(tr("Maximum Height\n(%1)").arg(Altitude::get_unit_full_string(height_unit))));


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
	this->view->setSortingEnabled(true);


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



	/* Set this member before adding rows to the table. */
	Qt::DateFormat dt_format = Qt::ISODate;
	if (ApplicationState::get_integer(VIK_SETTINGS_SORTABLE_DATE_TIME_FORMAT, (int *) &dt_format)) {
		this->date_time_format = dt_format;
	}

	for (auto iter = this->tracks.begin(); iter != this->tracks.end(); iter++) {
		this->add_row(*iter, distance_unit, speed_unit, height_unit);
	}


	/* Notice that we enable and perform sorting after adding all items in the for() loop. */

	if (hide_layer_names) {
		this->view->sortByColumn(TRACK_COLUMN, Qt::AscendingOrder);
	} else {
		this->view->sortByColumn(LAYER_NAME_COLUMN, Qt::AscendingOrder);
	}

	this->setMinimumSize(750, 400);


	this->view->show();

	this->view->setVisible(false);
	this->view->resizeRowsToContents();
	this->view->resizeColumnsToContents();
	this->view->setVisible(true);

}




/**
   @title: the title for the dialog
   @layer: The layer, from which a list of tracks should be extracted
   @type_id_string: TreeItem type to be show in list (empty string for both tracks and routes)

  Common method for showing a list of tracks with extended information
*/
void SlavGPS::track_list_dialog(QString const & title, Layer * layer, const QString & type_id_string)
{
	TrackListDialog dialog(title, layer->get_window());

	dialog.tracks.clear();

	if (layer->type == LayerType::Aggregate) {
		((LayerAggregate *) layer)->get_tracks_list(dialog.tracks, type_id_string);
	} else if (layer->type == LayerType::TRW) {
		((LayerTRW *) layer)->get_tracks_list(dialog.tracks, type_id_string);
	} else {
		assert (0);
	}

	dialog.build_model(layer->type != LayerType::Aggregate);
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
}




void TrackListDialog::accept_cb(void) /* Slot. */
{
	/* Here we are iterating over all rows in the table, saving
	   all tracks. We access the tracks through this->model, so
	   don't even think about using this->selected_track. */


	bool changed = false;


	const int n_rows = this->model->rowCount(this->model->invisibleRootItem()->index());
	for (int row = 0; row < n_rows; row++) {
		QStandardItem * item = NULL;

		item = this->model->item(row, TRACK_COLUMN);
		assert (NULL != item);
		Track * trk = item->data(RoleLayerData).value<Track *>();
		if (!trk) {
			qDebug() << SG_PREFIX_E << "NULL track in 'accept' handler";
			return;
		}


		LayerTRW * parent_layer = (LayerTRW *) trk->get_owning_layer();
		parent_layer->lock_remove();


		/* Make sure that the track really is in parent layer. */
		bool has_child = false;
		if (sg_ret::ok != parent_layer->has_child(trk, &has_child)) {
			parent_layer->unlock_remove();
			return;
		}
		if (!has_child) {
			qDebug() << SG_PREFIX_W << "Can't find edited Track in TRW layer";
			parent_layer->unlock_remove();
			return;
		}


		/* Save all edited properties of given track. */

		item = this->model->item(row, COMMENT_COLUMN);
		assert (NULL != item);
		QString comment = item->text();
		if (trk->comment != comment) {
			trk->set_comment(comment);
			changed = true;
		}


		parent_layer->unlock_remove();
	}

#if K_TODO  /* The dialog may be invoked from LayerAggregate's context
	       menu, which means that there may be multiple TRW
	       layers.  For each of them we need to call
	       trw->udpate_tree_item(), but we must be sure that this
	       doesn't trigger few redraws in a row. There must be
	       only one redraw, final one. Most probably triggered
	       from a layer (either TRW layer or Aggregate layer), for
	       which a context menu item has been triggered. */
	if (changed) {
		trk->update_tree_item_properties();
		parent_layer->emit_tree_item_changed("Indicating change to TRW Layer after changing properties of Track in Track list dialog");
	}
#endif


	this->accept();
}
