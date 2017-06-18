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
 *
 */
#ifndef _SG_KMZ_H_
#define _SG_KMZ_H_



#include <QPixmap>
#include <glib.h>




namespace SlavGPS {




	class Viewport;
	class LayersPanel;




	int kmz_save_file(QPixmap * pixmap, const char* filename, double north, double east, double south, double west);
	int kmz_open_file(const char* filename, Viewport * viewport, LayersPanel * panel);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_KMZ_H_ */
