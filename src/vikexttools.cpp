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

#include "vikexttools.h"





using namespace SlavGPS;





#define VIK_TOOL_DATA_KEY "vik-tool-data"
#define VIK_TOOL_WIN_KEY "vik-tool-win"

static std::list<External *> ext_tools;





void SlavGPS::vik_ext_tools_register(External * ext_tool)
{
	ext_tools.push_back(ext_tool);

}





void vik_ext_tools_unregister_all()
{
	for (auto iter = ext_tools.begin(); iter != ext_tools.end(); iter++) {
		/* kamilFIXME: do something here. */
		//g_object_unref(*iter);
	}
}





static void ext_tools_open_cb(GtkWidget * widget, Window * window)
{
#ifdef K
	void * ptr = g_object_get_data(G_OBJECT(widget), VIK_TOOL_DATA_KEY);
	External * ext_tool = (External *) ptr;
	ext_tool->open(window);
#endif
}





void SlavGPS::vik_ext_tools_add_action_items(Window * window, GtkUIManager * uim, GtkActionGroup * action_group, unsigned int mid)
{
	for (auto iter = ext_tools.begin(); iter != ext_tools.end(); iter++) {
		External * ext_tool = *iter;
		char * label = ext_tool->get_label();
		if (label) {
#ifdef K
			gtk_ui_manager_add_ui(uim, mid, "/ui/MainMenu/Tools/Exttools",
					      _(label),
					      label,
					      GTK_UI_MANAGER_MENUITEM, false);

			QAction * action = new QActionw(QString(label), this);
			g_object_set_data(action, VIK_TOOL_DATA_KEY, ext_tool);
			QObject::connect(action, SIGNAL (triggered(bool)), window, SLOT (ext_tools_open_cb));

			gtk_action_group_add_action(action_group, action);

			g_object_unref(action);
#endif
			free(label);
			label = NULL;
		}
	}
}





static void ext_tool_open_at_position_cb(GtkWidget * widget, Coord * vc)
{
#ifdef K
	void * ptr = g_object_get_data(G_OBJECT(widget), VIK_TOOL_DATA_KEY);
	External * ext_tool = (External *) ptr;

	void * wptr = g_object_get_data(G_OBJECT(widget), VIK_TOOL_WIN_KEY);
	Window * window = (Window *) wptr;

	ext_tool->open_at_position(window, vc);
#endif
}





/**
 * Add to any menu
 *  mostly for allowing to assign for TrackWaypoint layer menus
 */
void SlavGPS::vik_ext_tools_add_menu_items_to_menu(Window * window, QMenu * menu, Coord * vc)
{
	for (auto iter = ext_tools.begin(); iter != ext_tools.end(); iter++)  {
		External * ext_tool = *iter;
		char * label = ext_tool->get_label();
		if (label) {
#ifdef K
			QAction * action = QAction(QObject::tr(label), this);
			free(label);
			label = NULL;
			// Store some data into the menu entry
			g_object_set_data(G_OBJECT(item), VIK_TOOL_DATA_KEY, ext_tool);
			g_object_set_data(G_OBJECT(item), VIK_TOOL_WIN_KEY, window);
			if (vc) {
				QObject::connect(action, SIGNAL (triggered(bool)), vc, SLOT (ext_tool_open_at_position_cb));
			} else {
				QObject::connect(action, SIGNAL (triggered(bool)), window, SLOT (ext_tools_open_cb));
			}
			menu->addAction(action);
			gtk_widget_show(item);
#endif
		}
	}
}
