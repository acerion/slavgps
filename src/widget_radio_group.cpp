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




SGRadioGroup::SGRadioGroup(const QString & title_, const QStringList & labels, QWidget * parent_widget) : QGroupBox(parent_widget)
{
	this->vbox = new QVBoxLayout;
	this->group = new QButtonGroup;

	for (int i = 0; i < labels.size(); i++) {
		QRadioButton * radio = new QRadioButton(labels.at(i));
		this->group->addButton(radio, i);
		this->vbox->addWidget(radio);
		if (i == 0) {
			radio->setChecked(true);
		}
	}

	this->setLayout(this->vbox);
}




SGRadioGroup::~SGRadioGroup()
{
	delete this->vbox;
	delete this->group;
}




uint32_t SGRadioGroup::get_selected(void)
{
	return (uint32_t) this->group->checkedId();
}




void SGRadioGroup::set_selected(uint32_t id)
{
	this->group->button(id)->setChecked(true);
}
