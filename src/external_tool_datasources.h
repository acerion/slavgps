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

#ifndef _SG_EXTERNAL_DATASOURCES_H
#define _SG_EXTERNAL_DATASOURCES_H




#include "external_tool.h"




namespace SlavGPS {




	class ExternalToolDataSource {
	public:
		static void register_tool(ExternalTool * ext_tool);
		static void unregister_all(void);
		static void add_menu_items(QMenu * menu, Window * window);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_EXTERNAL_DATASOURCES_H */
