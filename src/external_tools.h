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

#ifndef _SG_EXTERNAL_TOOLS_H_
#define _SG_EXTERNAL_TOOLS_H_




#include <QMenu>




#include "external_tool.h"



/*
  A collection of external (remote) tools that perform some action on
  remote server for specific coordinate selected on local machine.

  The tools can be registered with ::register_tool() method.



*/




namespace SlavGPS {




	class Window;



	class ExternalTools {
	public:
		static void register_tool(ExternalTool * ext_tool);
		static void unregister_all(void);
		static void add_action_items(QActionGroup * action_group, Window * window);
		static void add_menu_items(QMenu * menu, Window * window, const Coord * coord);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_EXTERNAL_TOOLS_H_ */
