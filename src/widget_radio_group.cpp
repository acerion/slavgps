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

#include "widget_radio_group.h"




using namespace SlavGPS;




SGRadioGroup::SGRadioGroup(QString & title, std::list<QString> & labels, QWidget * parent) : QGroupBox(parent)
{
	this->vbox = new QVBoxLayout;
	this->group = new QButtonGroup;

        uint32_t i = 0;
	for (auto iter = labels.begin(); iter != labels.end(); iter++) {
		QRadioButton * radio = new QRadioButton(*iter);
		this->group->addButton(radio, i);
		this->vbox->addWidget(radio);
		if (i == 0) {
			radio->setChecked(true);
		}
		i++;
	}

	this->setLayout(this->vbox);
}




SGRadioGroup::~SGRadioGroup()
{
	delete this->vbox;
	delete this->group;
}




uint32_t SGRadioGroup::value(void)
{
	return (uint32_t) this->group->checkedId();
}
