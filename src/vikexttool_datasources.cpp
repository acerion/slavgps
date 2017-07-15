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




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <list>
#include <cstdlib>

#include "external_tools.h"
#include "vikexttool_datasources.h"




using namespace SlavGPS;




#define VIK_TOOL_DATASOURCE_KEY "vik-datasource-tool"

static std::list<ExternalTool *> ext_tool_datasources;




void SlavGPS::vik_ext_tool_datasources_register(ExternalTool * ext_tool)
{
	ext_tool_datasources.push_back(ext_tool);
}




void vik_ext_tool_datasources_unregister_all()
{
	for (auto iter = ext_tool_datasources.begin(); iter != ext_tool_datasources.end(); iter++) {
		delete *iter;
	}
}




/**
 * Add to any menu.
 * Mostly for allowing to assign for TrackWaypoint layer menus.
 */
void SlavGPS::vik_ext_tool_datasources_add_menu_items(QMenu * menu, Window * window)
{
	for (auto iter = ext_tool_datasources.begin(); iter != ext_tool_datasources.end(); iter++) {
		ExternalTool * ext_tool = *iter;
		QAction * qa = new QAction(ext_tool->get_label(), NULL);

		ext_tool->set_window(window);

		QObject::connect(qa, SIGNAL (triggered(bool)), ext_tool, SLOT (run_at_current_position_cb(void)));
		menu->addAction(qa);
	}
}
