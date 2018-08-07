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
#include <QRadioButton>
#include <QVBoxLayout>
#include <QButtonGroup>

#include "widget_radio_group.h"




using namespace SlavGPS;




RadioGroupWidget::RadioGroupWidget(const QString & title_, const std::vector<SGLabelID> * items, QWidget * parent_widget) : QGroupBox(parent_widget)
{
	this->vbox = new QVBoxLayout;
	this->group = new QButtonGroup;

	bool first_checked = false;

	for (auto iter = items->begin(); iter != items->end(); iter++) {
		QRadioButton * radio = new QRadioButton((*iter).label);
		this->group->addButton(radio, (*iter).id);
		this->vbox->addWidget(radio);
		if (!first_checked) {
			radio->setChecked(true);
			first_checked = true;
		}
	}

	this->setLayout(this->vbox);
}




RadioGroupWidget::~RadioGroupWidget()
{
	qDebug() << "RadioGroupWidget destructor called";
	delete this->vbox;
	delete this->group;
}




int RadioGroupWidget::get_id_of_selected(void)
{
	return this->group->checkedId();
}




void RadioGroupWidget::set_id_of_selected(int id)
{
	this->group->button(id)->setChecked(true);
}
