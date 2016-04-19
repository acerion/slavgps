/*
 * viking
 * Copyright (C) 2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 *
 * viking is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
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

 /**
  * SECTION:vikmaptype
  * @short_description: the adapter class to support old map source declaration
  *
  * The #VikMapType class handles is an adapter to allow to reuse
  * old map source (see #VikMapsLayer_MapType).
  */

#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "vikmaptype.h"
#include "vikmapslayer_compat.h"
#include "download.h"

using namespace SlavGPS;

VikMapType::VikMapType()
{
	label = NULL;
	name = NULL;
}

VikMapType::VikMapType(VikMapsLayer_MapType map_type, const char *label)
{
	map_type = map_type;
	label = g_strdup(label);
	name = NULL;
}


VikMapType::~VikMapType()
{
	free(label);
	label = NULL;

	free(name);
	name = NULL;
}

const char * VikMapType::get_name()
{
	return name;
}

uint16_t VikMapType::get_uniq_id()
{
	return map_type.uniq_id;
}

char * VikMapType::map_type_get_label()
{
	return label;
}

uint16_t VikMapType::get_tilesize_x()
{
	return tilesize_x;
}

uint16_t VikMapType::get_tilesize_y()
{
	return tilesize_y;
}

VikViewportDrawMode VikMapType::get_drawmode()
{
	return (VikViewportDrawMode) map_type.drawmode;
}
