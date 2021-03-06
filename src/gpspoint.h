/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _SG_GPSPOINT_H_
#define _SG_GPSPOINT_H_




#include <cstdint>
#include <cstdio>




#include "globals.h"





namespace SlavGPS {




	class LayerTRW;
	enum class LayerDataReadStatus;




	class GPSPoint {
	public:
		static LayerDataReadStatus read_layer_from_file(QFile & file, LayerTRW * trw, const QString & dirpath);
		static SaveStatus write_layer_to_file(FILE * file, const LayerTRW * trw);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_GPSPOINT_H_ */
