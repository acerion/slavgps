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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <list>
#include <cstring>
#include <cstdlib>

#include <glib/gi18n.h>

#include "vikexttools.h"
#include "vikexttool_datasources.h"




using namespace SlavGPS;




#define VIK_TOOL_DATASOURCE_KEY "vik-datasource-tool"

static std::list<External *> ext_tool_datasources;





void SlavGPS::vik_ext_tool_datasources_register(External * ext_tool)
{
	ext_tool_datasources.push_back(ext_tool);
}




void vik_ext_tool_datasources_unregister_all()
{
	for (auto iter = ext_tool_datasources.begin(); iter != ext_tool_datasources.end(); iter++) {
		/* kamilFIXME: do something here. */
		// g_object_unref(*iter);
	}
}




static void ext_tool_datasources_open_cb(GtkWidget * widget, Window * window)
{
	void * ptr = g_object_get_data(G_OBJECT (widget), VIK_TOOL_DATASOURCE_KEY);
	External * ext_tool = (External *) ptr;
	ext_tool->open(window);
}




/**
 * Add to any menu.
 * Mostly for allowing to assign for TrackWaypoint layer menus.
 */
void SlavGPS::vik_ext_tool_datasources_add_menu_items_to_menu(Window * window, GtkMenu * menu)
{
	for (auto iter = ext_tool_datasources.begin(); iter != ext_tool_datasources.end(); iter++) {
		External * ext_tool = *iter;
		char * label = ext_tool->get_label();
		if (label) {
			GtkWidget * item = gtk_menu_item_new_with_label(_(label));
			free(label);
			label = NULL;
			/* Store tool's ref into the menu entry. */
			g_object_set_data(G_OBJECT(item), VIK_TOOL_DATASOURCE_KEY, ext_tool);
			g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(ext_tool_datasources_open_cb), window);
			gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
			gtk_widget_show(item);
		}
	}
}




/**
 * Adds to the File->Acquire menu only.
 */
void SlavGPS::vik_ext_tool_datasources_add_menu_items(Window * window, GtkUIManager * uim)
{
	GtkWidget * widget = gtk_ui_manager_get_widget(uim, "/MainMenu/File/Acquire/");
	GtkMenu * menu = GTK_MENU (gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget)));
	SlavGPS::vik_ext_tool_datasources_add_menu_items_to_menu(window, menu);
	gtk_widget_show(widget);
}
