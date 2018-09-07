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

#ifndef _SG_TREE_ITEM_LIST_H_
#define _SG_TREE_ITEM_LIST_H_




#include <list>
#include <vector>




#include <QWidget>
#include <QDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QStandardItemModel>
#include <QTableView>
#include <QContextMenuEvent>




#include "measurements.h"
#include "tree_item.h"




namespace SlavGPS {




	class TreeItem;
	class Viewport;
	class TreeItem;





	class TreeItemListColumn {
	public:
		TreeItemListColumn(enum TreeItemPropertyID new_id, bool new_visible, const QString & new_header_label) :
			id(new_id),
			visible(new_visible),
			header_label(new_header_label) {};
		const TreeItemPropertyID id;
		const bool visible;            /* Is the column visible? */
		const QString header_label;    /* If the column is visible, this is the label of column header. */
	};

	class TreeItemListFormat {
	public:
		std::vector<TreeItemListColumn> columns;
		TreeItemListFormat & operator=(const TreeItemListFormat & other);
	};





	class TreeItemListModel : public QStandardItemModel {
		Q_OBJECT

	public:
		TreeItemListModel(QObject * parent = NULL) : QStandardItemModel(parent) {};
		void sort(int column, Qt::SortOrder order = Qt::AscendingOrder);
	};




	class TreeItemListDialog : public QDialog {
		Q_OBJECT
	public:
		TreeItemListDialog(QString const & title, QWidget * parent = NULL);
		~TreeItemListDialog();
		void build_model(const TreeItemListFormat & new_list_format);

		static void show_dialog(QString const & title, const TreeItemListFormat & new_list_format, const std::list<TreeItem *> & new_tree_items, QWidget * parent = NULL);

		std::list<TreeItem *> tree_items;

	private slots:
		void accept_cb(void);

	private:
		void contextMenuEvent(QContextMenuEvent * event);

		int column_id_to_column_idx(TreeItemPropertyID column_id);

		QWidget * parent = NULL;
		QDialogButtonBox * button_box = NULL;
		QVBoxLayout * vbox = NULL;

		QStandardItemModel * model = NULL;
		QTableView * view = NULL;

		/* TreeItem selected in list. */
		TreeItem * selected_tree_item = NULL;

		TreeItemListFormat list_format;

		Qt::DateFormat date_time_format = Qt::ISODate;
	};





} /* namespace SlavGPS */




#endif /* #ifndef _SG_TREE_ITEM_LIST_H_ */
