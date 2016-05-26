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
 *
 */

#ifndef _VIKING_GEOREFLAYER_H
#define _VIKING_GEOREFLAYER_H

#include <stdbool.h>
#include <stdint.h>

#include "viklayer.h"

#ifdef __cplusplus
extern "C" {
#endif


#define VIK_GEOREF_LAYER_TYPE            (vik_georef_layer_get_type())
#define VIK_GEOREF_LAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_GEOREF_LAYER_TYPE, VikGeorefLayer))
#define VIK_GEOREF_LAYER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_GEOREF_LAYER_TYPE, VikGeorefLayerClass))
#define IS_VIK_GEOREF_LAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_GEOREF_LAYER_TYPE))
#define IS_VIK_GEOREF_LAYER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_GEOREF_LAYER_TYPE))

typedef struct _VikGeorefLayerClass VikGeorefLayerClass;
struct _VikGeorefLayerClass
{
	VikLayerClass object_class;
};

GType vik_georef_layer_get_type();

typedef struct _VikGeorefLayer VikGeorefLayer;

void vik_georef_layer_init(void);

VikGeorefLayer *vik_georef_layer_create(VikViewport *vp,
					VikLayersPanel *vlp,
					const char *name,
					GdkPixbuf *pibxbuf,
					VikCoord *coord_tr,
					VikCoord *coord_br );

#ifdef __cplusplus
}
#endif





namespace SlavGPS {





	class LayerGeoref : public Layer {
	public:
		LayerGeoref(VikLayer * vl) : Layer(vl) { };


		/* Layer interface methods. */
		void load_image(Viewport * viewport, bool from_file);
		void draw(Viewport * viewport);
		char const * tooltip();
		void marshall(uint8_t ** data, int * len);
		void add_menu_items(GtkMenu * menu, void * vlp);
		bool properties(void * vp);
		void free_();
	};





}





#endif
