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
#include <QProcess>




#include "layer.h"
#include "tree_item_list.h"
#include "layer_aggregate.h"
#include "window.h"
#include "viewport_internal.h"
#include "util.h"
#include "application_state.h"
#include "preferences.h"
#include "tree_view_internal.h"
#include "dialog.h"





using namespace SlavGPS;




#define SG_MODULE "TreeItem List"




void show_context_menu(TreeItem * item, const QPoint & cursor_position)
{
	if (!item) {
		qDebug() << SG_PREFIX_E << "Show context menu for item: NULL item";
		return;
	}

	QMenu menu;

	switch (item->get_tree_item_type()) {
	case TreeItemType::Layer: {

		qDebug() << SG_PREFIX_I << "Menu for layer tree item" << item->type_id << item->name;

		/* We don't want a parent layer here. We want item
		   cast to layer if the item is layer, or item's
		   parent layer otherwise. */
		Layer * layer = item->to_layer();

		/* "New layer -> layer types" submenu. */
		MenuOperation ops = layer->get_menu_operation_ids();
		ops = (MenuOperation) (ops | MenuOperationNew);
		//this->context_menu_add_standard_items(&menu, ops);

		/* Layer-type-specific menu items. */
		layer->add_menu_items(menu);
		}
		break;

	case TreeItemType::Sublayer:
		qDebug() << SG_PREFIX_I << "Menu for non-layer tree item" << item->type_id << item->name;

		if (!item->add_context_menu_items(menu, true)) {
			return;
		}
		/* TODO_LATER: specific things for different types. */
		break;

	default:
		qDebug() << SG_PREFIX_E << "Unexpected value of tree item type:" << (int) item->get_tree_item_type() << item->name;
		return;
	}

	menu.exec(QPoint(cursor_position.x(), cursor_position.y()));

	return;
}



void TreeItemListDialog::contextMenuEvent(QContextMenuEvent * ev)
{
	if (!this->view->geometry().contains(ev->pos())) {
		qDebug() << SG_PREFIX_W << "context menu event outside list view";
		/* We want to handle only events that happen inside of list view. */
		return;
	}
	qDebug() << SG_PREFIX_I << "context menu event inside list view";

	QPoint orig = ev->pos();
	QPoint v = this->view->pos();
	QPoint t = this->view->viewport()->pos();

	qDebug() << SG_PREFIX_D << "Event @" << orig.x() << orig.y();
	qDebug() << SG_PREFIX_D << "GisViewport @" << v;
	qDebug() << SG_PREFIX_D << "Tree view @" << t;

	QPoint point;
	point.setX(orig.x() - v.x() - t.x());
	point.setY(orig.y() - v.y() - t.y());

	QModelIndex item_index = this->view->indexAt(point);

	if (!item_index.isValid()) {
		/* We have clicked on empty space, not on tree
		   item. Not an error, user simply missed a row. */
		qDebug() << SG_PREFIX_I << "INvalid index";
		return;
	}

	/* We have clicked on some valid tree item. */
	qDebug() << SG_PREFIX_I << "Item index row =" << item_index.row() << ", item index column =" << item_index.column();


	QStandardItem * root_item = this->model->invisibleRootItem();
	if (!root_item) {
		qDebug() << SG_PREFIX_E << "Failed to get root item";
		return;
	}

	const int column_idx = this->column_id_to_column_idx(TreeItemPropertyID::TheItem);
	if (column_idx < 0) {
		qDebug() << SG_PREFIX_E << "Calculated column index is out of range:" << column_idx;
		return;
	}


	QStandardItem * child_item = root_item->child(item_index.row(), column_idx);
	if (!child_item) {
		qDebug() << SG_PREFIX_E << "Failed to get child item from column no." << column_idx;
		return;
	}

	qDebug() << SG_PREFIX_I << "Selected cell" << child_item->text();

	TreeItem * tree_item = child_item->data(RoleLayerData).value<TreeItem *>();
	if (!tree_item) {
		qDebug() << SG_PREFIX_E << "Failed to get non-NULL TreeItem from item" << child_item->text() << "at column id" << TreeItemPropertyID::TheItem;
		return;
	}

	show_context_menu(tree_item, QCursor::pos());

	return;
}




int TreeItemListDialog::column_id_to_column_idx(TreeItemPropertyID column_id)
{
	for (unsigned int i = 0; i < this->view_format.columns.size(); i++) {
		if (this->view_format.columns[i].id == column_id) {
			return (int) i;
		}
	}
	return -1;
}




TreeItemListDialog::TreeItemListDialog(QString const & title, QWidget * parent_widget) : QDialog(parent_widget)
{
	this->setWindowTitle(title);

	this->button_box = new QDialogButtonBox();
	this->parent = parent_widget;
	this->button_box->addButton("&Close", QDialogButtonBox::AcceptRole);
	connect(this->button_box, &QDialogButtonBox::accepted, this, &TreeItemListDialog::accept_cb);
	this->vbox = new QVBoxLayout;
}




TreeItemListDialog::~TreeItemListDialog()
{
}




/* Here we save in track objects changes made in the dialog. */
void TreeItemListDialog::accept_cb(void) /* Slot. */
{
	/* FIXME: check and make sure the tree_item still exists before doing anything to it. */
#ifdef TODO_LATER
	if (this->selected_tree_item) {
		Layer * parent_layer = this->selected_tree_item->get_parent_layer();
		if (parent_layer) {
			this->selected_tree_item->update_tree_item_properties();
			parent_layer->emit_tree_item_changed("TRW - TreeItem List Dialog - Accept");
		}
	}
#endif

	this->accept();
}




void TreeItemListModel::sort(int column, Qt::SortOrder order)
{
	if (column == TreeItemPropertyID::Icon) {
		return;
	}

	QStandardItemModel::sort(column, order);
}




TreeItemViewFormat & TreeItemViewFormat::operator=(const TreeItemViewFormat & other)
{
	if (&other == this) {
		return *this;
	}

	for (unsigned int i = 0; i < other.columns.size(); i++) {
		this->columns.push_back(TreeItemViewColumn(other.columns[i].id,
							   other.columns[i].visible,
							   other.columns[i].header_label));
	}

	return *this;
}




#ifdef K_FIXME_RESTORE




typedef struct {
	bool has_layer_names;
	bool include_positions;
	QString str;
} copy_data_t;




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




void WaypointListDialog::copy_selected(bool include_positions)
{
	copy_data_t cd;

	QItemSelectionModel * selection = this->view.selectionModel();
	// NB GTK3 has gtk_tree_view_get_n_columns() but we're GTK2 ATM
	GList * gl = gtk_tree_view_get_columns(this->view);
	unsigned int count = g_list_length(gl);
	g_list_free(gl);
	cd.has_layer_names = (count > N_COLUMNS-3);
	cd.include_positions = include_positions;
	gtk_tree_selection_selected_foreach(selection, copy_selection, &cd);

	Pickle dummy;
	Clipboard::copy(ClipboardDataType::Text, LayerType::Aggregate, "", dummy, cd.str);
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


#endif
