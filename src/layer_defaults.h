/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright(C) 2013, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_LAYER_DEFAULTS_H_
#define _SG_LAYER_DEFAULTS_H_




#include <QWidget>
#include <QSettings>




#include "variant.h"




namespace SlavGPS {




	class ParameterSpecification;
	enum class LayerType;




	class LayerDefaults {

	public:
		static bool init(void);
		static void uninit(void);

		static void set(LayerType layer_type, const ParameterSpecification & layer_param_spec, const SGVariant & default_value);
		static SGVariant get(LayerType layer_type, const char * param_name, SGVariantType param_type);

		static bool show_window(LayerType layer_type, QWidget * parent);

		static bool save(void);

		static bool is_initialized(void) { return LayerDefaults::loaded; };

	private:
		static bool save_to_file(void);

		static void fill_missing_from_hardcoded_defaults(LayerType layer_type);

		/* Check value of ::is_valid() of returned variant to
		   see whether lookup of parameter has succeeded. */
		static SGVariant get_parameter_value(LayerType layer_type, const char * name, SGVariantType type_id);

		static void save_parameter_value(const SGVariant & value, LayerType layer_type, const char * name, SGVariantType ptype);

		static QSettings * keyfile;
		static bool loaded;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_DEFAULTS_H_ */
