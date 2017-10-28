/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright(C) 2013, Rob Norris <rw_norris@hotmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 *(at your option) any later version.
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

#ifndef _SG_LAYER_DEFAULTS_H_
#define _SG_LAYER_DEFAULTS_H_




#include <cstdint>

#include <QWidget>

#include "ui_builder.h"
#include "layer.h"




namespace SlavGPS {




	class LayerDefaults {

	public:
		static void init(void);
		static void uninit(void);

		static void set(LayerType layer_type, ParameterSpecification * layer_param_spec, const SGVariant & default_value);
		static SGVariant get(LayerType layer_type, const char * param_name, SGVariantType param_type);

		static bool show_window(LayerType layer_type, QWidget * parent);

		static bool save(void);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_DEFAULTS_H_ */
