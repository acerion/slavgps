/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_LAYER_MAPNIK_WRAPPER_H_
#define _SG_LAYER_MAPNIK_WRAPPER_H_





#include <QString>




#include <mapnik/map.hpp>




#include "globals.h"




namespace SlavGPS {




	class MapnikWrapper {
	public:
		MapnikWrapper();
		~MapnikWrapper();

		QString get_copyright(void) const;
		void set_copyright(void);

		QStringList get_parameters(void) const;

		sg_ret load_map_file(const QString & map_file_full_path, unsigned int width, unsigned int height, QString & msg);

		QPixmap render_map(double lat_tl, double lon_tl, double lat_br, double lon_br);

		static void initialize(const QString & plugins_dir, const QString & font_dir, bool font_dir_recurse);
		static QString about(void);

		mapnik::Map map;

	private:
		QString copyright; /* Cached Mapnik parameter to save looking it up each time. */
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_MAPNIK_WRAPPER_H_ */
