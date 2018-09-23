/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2018, Kamil Ignacak <acerion@wp.pl>
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

#ifndef _SG_METATILE_H_
#define _SG_METATILE_H_




#include <QString>




/* MAX_SIZE is the biggest file which we will return to the user. */
#define METATILE_MAX_SIZE (1 * 1024 * 1024)




namespace SlavGPS {




	class TileInfo;




	class Metatile {
	public:
		Metatile(const QString & dir, const TileInfo & tile_info);

		int read_metatile(QString & log_msg);

		QString file_full_path;
		unsigned char offset = 0;
		bool is_compressed = false;

		unsigned char buffer[METATILE_MAX_SIZE];
		size_t read_bytes = 0;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_METATILE_H_ */
