/*
 * viking
 * Copyright (C) 2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 *
 * viking is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * viking is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _SG_MAP_TYPE_H_
#define _SG_MAP_TYPE_H_




#include "layer_map_source.h"
#include "vikmapslayer_compat.h"




namespace SlavGPS {




	class VikMapType : public MapSource {
	public:
		VikMapType();
		VikMapType(VikMapsLayer_MapType map_type, char const * label);
		~VikMapType();


		QString get_name(void) const;
		uint16_t get_uniq_id();
		QString map_type_get_label(void) const;
		uint16_t get_tilesize_x(void) const;
		uint16_t get_tilesize_y(void) const;

		GisViewportDrawMode get_drawmode(void) const;


		QString label;
		QString name;
		VikMapsLayer_MapType map_type;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_MAP_TYPE_H_ */
