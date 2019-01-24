/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2008, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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




#include <QDebug>




#include "external_tool.h"
#include "globals.h"




using namespace SlavGPS;




#define SG_MODULE "External Tool"




ExternalTool::ExternalTool(const QString & new_label)
{
	this->label = new_label;

	qDebug() << SG_PREFIX_I << "New external tool" << this->label;
}




ExternalTool::~ExternalTool()
{
	qDebug() << SG_PREFIX_I << "Delete external tool" << this->label;
}




const QString & ExternalTool::get_label(void) const
{
	return this->label;
}




void ExternalTool::run_at_current_position_cb(void)
{
	this->run_at_current_position(this->m_viewport);
}




void ExternalTool::run_at_position_cb(void)
{
	this->run_at_position(this->m_viewport, this->m_coord);
}




void ExternalTool::set_viewport(Viewport * viewport)
{
	this->m_viewport = viewport;
}




void ExternalTool::set_coord(const Coord & coord)
{
	this->m_coord = coord;
}
