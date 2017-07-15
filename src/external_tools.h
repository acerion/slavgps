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

#ifndef _SG_EXTERNAL_TOOLS_H
#define _SG_EXTERNAL_TOOLS_H




#include <cstdint>

#include <QMenu>

#include "external_tool.h"
#include "slav_qt.h"




namespace SlavGPS {




	class Window;




	void vik_ext_tools_register(ExternalTool * ext_tool);
	void vik_ext_tools_unregister_all();
	void vik_ext_tools_add_action_items(QActionGroup * action_group, Window * window);
	void vik_ext_tools_add_menu_items_to_menu(Window * window, QMenu * menu, Coord * coord);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_EXTERNAL_TOOLS_H */
