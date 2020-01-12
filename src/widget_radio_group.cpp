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
 */




#include <cstring>
#include <cassert>




#include <QDebug>
#include <QHeaderView>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QButtonGroup>




#include "widget_radio_group.h"




using namespace SlavGPS;



#define SG_MODULE "Widget Radio Group"




RadioGroupWidget::RadioGroupWidget(__attribute__((unused)) const QString & title_, const WidgetIntEnumerationData & items, QWidget * parent_widget) : QGroupBox(parent_widget)
{
	this->vbox = new QVBoxLayout;
	this->group = new QButtonGroup;

	bool any_checked = false;

	for (auto iter = items.values.begin(); iter != items.values.end(); iter++) {
		QRadioButton * button = new QRadioButton((*iter).label);
		this->group->addButton(button, (*iter).id);
		if (iter->id == items.default_id) {
			button->setChecked(true);
			any_checked = true;
		}
		this->vbox->addWidget(button);
	}

	if (false == any_checked) {
		qDebug() << SG_PREFIX_E << "No button is initially checked: couldn't find default ID in @param items";
	}

	this->setLayout(this->vbox);
}




RadioGroupWidget::~RadioGroupWidget()
{
	delete this->vbox;
	delete this->group;
}




int RadioGroupWidget::get_selected_id(void)
{
	return this->group->checkedId();
}




/**
   @reviewed-on 2019-12-07
*/
void RadioGroupWidget::set_selected_id(int id)
{
	QAbstractButton * button = this->group->button(id);
	if (nullptr == button) {
		qDebug() << SG_PREFIX_E << "Failed to look up button with id" << id;
	} else {
		button->setChecked(true);
	}
}
