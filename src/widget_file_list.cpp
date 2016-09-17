/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
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




#include <cstring>

#include <QDebug>
#include <QHeaderView>

#include "widget_file_list.h"




using namespace SlavGPS;




SGFileList::SGFileList(char const * title, std::list<char *> * fl, QWidget * parent) : QWidget(parent)
{

#if 1
	this->button_box = new QDialogButtonBox();
	this->add = this->button_box->addButton("Add", QDialogButtonBox::ActionRole);
	this->del = this->button_box->addButton("Delete", QDialogButtonBox::ActionRole);

	this->vbox = new QVBoxLayout;


	this->model = new QStandardItemModel();
	this->model->setHorizontalHeaderItem(0, new QStandardItem("DEM files"));


	this->view = new QTableView();
	//this->view->horizontalHeader()->show();
	//this->view->horizontalHeader()->showSection(0);
	//this->view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	this->view->horizontalHeader()->setStretchLastSection(true);
	this->view->verticalHeader()->setVisible(false);
	this->view->setWordWrap(false);
	this->view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	this->view->setTextElideMode(Qt::ElideNone);
	this->view->setShowGrid(false);
	this->view->setModel(this->model);
	this->view->show();



	this->file_list = fl;
	for (auto iter = this->file_list->begin(); iter != this->file_list->end(); ++iter) {
		qDebug() << "SGFileList: adding to initial file list:" << (*iter);

		QStandardItem * item = new QStandardItem(*iter);
		item->setToolTip(*iter);
		this->model->appendRow(item);
	}
	this->view->setVisible(false);
	this->view->resizeRowsToContents();
	this->view->resizeColumnsToContents();
	this->view->setVisible(true);



	this->vbox->addWidget(this->view);
	this->vbox->addWidget(this->button_box);

	QLayout * old = this->layout();
	delete old;

	this->setLayout(this->vbox);

	connect(this->add, SIGNAL(clicked()), this, SLOT(add_file()));
	connect(this->del, SIGNAL(clicked()), this, SLOT(del_file()));
#endif
}




SGFileList::~SGFileList()
{
	for (auto iter = this->file_list->begin(); iter != this->file_list->end(); iter++) {
		qDebug() << "File on list: " << QString(*iter);
	}
}




std::list<char *> * SGFileList::get_list(void)
{
	this->file_list->clear();

	QStandardItem * root = this->model->invisibleRootItem();
	for (int i = 0; i < root->rowCount(); i++) {
		QStandardItem * item = root->child(i);
		QByteArray array = item->text().toLocal8Bit();
		char * buffer = array.data();
		this->file_list->push_back(strdup(buffer));
	}

	return this->file_list;
}




void SGFileList::add_file()
{
	qDebug() << "Add file";

	if (!this->file_selector) {
		this->file_selector = new QFileDialog();
		this->file_selector->setFileMode(QFileDialog::ExistingFiles);
	}

	if (this->file_selector->exec()) {
		QStringList selection = this->file_selector->selectedFiles();
		for (QStringList::const_iterator iter = selection.constBegin(); iter != selection.constEnd(); ++iter) {
			qDebug() << (*iter);

			QStandardItem * item = new QStandardItem(*iter);
			item->setToolTip(*iter);
			this->model->appendRow(item);
		}
		this->view->setVisible(false);
		this->view->resizeRowsToContents();
		this->view->resizeColumnsToContents();
		this->view->setVisible(true);
	}
}




void SGFileList::del_file()
{
	qDebug() << "Delete file";

	QModelIndex index = this->view->currentIndex();
	if (!index.isValid()) {
		return;
	}

	this->model->removeRow(index.row());

}
