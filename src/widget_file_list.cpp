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




#include <QDebug>
#include <QHeaderView>

#include "widget_file_list.h"




using namespace SlavGPS;




SGFileList::SGFileList(char const * title, std::list<char *> * sl, QWidget * parent) : QWidget(parent)
{
#if 1
	this->button_box = new QDialogButtonBox();
	this->add = this->button_box->addButton("Add", QDialogButtonBox::ActionRole);
	this->del = this->button_box->addButton("Delete", QDialogButtonBox::ActionRole);

	this->vbox = new QVBoxLayout;


	this->model = new QStandardItemModel();
	this->model->setHorizontalHeaderItem(0, new QStandardItem("Tree Item Type"));


	this->view = new QTableView();
	//this->view->horizontalHeader()->showSection(0);
	//this->view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	this->view->horizontalHeader()->setStretchLastSection(true);
	this->view->verticalHeader()->setVisible(false);
	this->view->setWordWrap(false);
	//this->view->setShowGrid(false);
	this->view->setModel(this->model);

	QStandardItem * item = new QStandardItem(" this is path to file");
	this->model->appendRow(item);

	this->view->resizeRowsToContents();


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
}




std::list<char *> * SGFileList::get_list(void)
{
	return NULL;
}




void SGFileList::add_file()
{
	qDebug() << "Add file";
}




void SGFileList::del_file()
{
	qDebug() << "Delete file";
}
