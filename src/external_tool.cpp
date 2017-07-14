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
 *
 */


#include <cstring>
#include <cstdlib>
#include <cstdio>

#include <QDebug>

#include <glib.h>

#include "external_tool.h"





using namespace SlavGPS;





ExternalTool::ExternalTool(const QString & new_label)
{
	this->label = new_label;

	qDebug() << "II: External Tool: new external tool" << this->label;
}





ExternalTool::~ExternalTool()
{
	qDebug() << "II: External Toll: delete external tool" << this->label;
}





const QString & ExternalTool::get_label(void)
{
	return this->label;
}
