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
#include <QColorDialog>

#include "widget_color_button.h"




using namespace SlavGPS;




SGColorButton::SGColorButton(QColor const & color_, QWidget * parent_widget = NULL) : QToolButton (parent_widget)
{
	this->set_style(color_);

	this->color = color_;

	connect(this, SIGNAL(clicked()), this, SLOT(open_dialog(void)));
}




SGColorButton::~SGColorButton()
{
}




void SGColorButton::set_style(QColor const & color_)
{
	QString style_ = "background-color: rgb(%1, %2, %3);";
	style_ = style_.arg(color_.red()).arg(color_.green()).arg(color_.blue());
	style_ += "border: none;";
	this->setStyleSheet(style_);

}




void SGColorButton::open_dialog(void)
{
	QColor new_color = QColorDialog::getColor(this->color, NULL, "Get color", 0);
	if (new_color.isValid()) {
		this->color = new_color;
		this->set_style(this->color);
	}
}




QColor SGColorButton::get_color(void)
{
	return this->color;
}
