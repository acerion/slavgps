/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#ifndef _SG_LAYER_TRW_EXPORT_H_
#define _SG_LAYER_TRW_EXPORT_H_




#include "layer_trw.h"
#include "file.h"




namespace SlavGPS {




	void vik_trw_layer_export(LayerTRW * layer, char const * title, char const * default_name, Track * trk, VikFileType_t file_type);
	void vik_trw_layer_export_external_gpx(LayerTRW * trw, char const * external_program);
	void vik_trw_layer_export_gpsbabel(LayerTRW * trw, char const * title, char const * default_name);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_TRW_EXPORT_H_ */
