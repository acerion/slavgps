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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <list>

#include <cstring>
#include <cstdlib>

#include "external_tools.h"
#include "slav_qt.h"




using namespace SlavGPS;




static std::list<ExternalTool *> ext_tools;




void SlavGPS::external_tools_register(ExternalTool * ext_tool)
{
	ext_tools.push_back(ext_tool);
}




void external_tools_unregister_all()
{
	for (auto iter = ext_tools.begin(); iter != ext_tools.end(); iter++) {
		delete *iter;
	}
}




void SlavGPS::external_tools_add_action_items(QActionGroup * action_group, Window * window)
{
	for (auto iter = ext_tools.begin(); iter != ext_tools.end(); iter++) {

		ExternalTool * ext_tool = *iter;
		QAction * qa = new QAction(ext_tool->get_label(), NULL);

		ext_tool->set_window(window);

		QObject::connect(qa, SIGNAL (triggered(bool)), ext_tool, SLOT (run_at_current_position_cb(void)));
		action_group->addAction(qa);
	}
}




/**
 * Add to any menu
 *  mostly for allowing to assign for TrackWaypoint layer menus
 */
void SlavGPS::external_tools_add_menu_items_to_menu(Window * window, QMenu * menu, const Coord * coord)
{
	for (auto iter = ext_tools.begin(); iter != ext_tools.end(); iter++)  {
		ExternalTool * ext_tool = *iter;
		QAction * qa = new QAction(ext_tool->get_label(), NULL);

		ext_tool->set_window(window);
		if (coord) {
			ext_tool->set_coord(coord);
			QObject::connect(qa, SIGNAL (triggered(bool)), ext_tool, SLOT (run_at_position_cb(void)));
		} else {
			QObject::connect(qa, SIGNAL (triggered(bool)), ext_tool, SLOT (run_at_current_position_cb(void)));
		}

		menu->addAction(qa);
	}
}
