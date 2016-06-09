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

VikLayer *vik_georef_layer_create(Viewport * viewport,
					LayersPanel * panel,
					const char *name,
					GdkPixbuf *pibxbuf,
					VikCoord *coord_tr,
					VikCoord *coord_br );

#ifdef __cplusplus
}
#endif





namespace SlavGPS {




	typedef struct {
		GtkWidget * x_spin;
		GtkWidget * y_spin;
		// UTM widgets
		GtkWidget * ce_spin; // top left
		GtkWidget * cn_spin; //    "
		GtkWidget * utm_zone_spin;
		GtkWidget * utm_letter_entry;

		GtkWidget * lat_tl_spin;
		GtkWidget * lon_tl_spin;
		GtkWidget * lat_br_spin;
		GtkWidget * lon_br_spin;
		//
		GtkWidget * tabs;
		GtkWidget * imageentry;
	} changeable_widgets;





	class LayerGeoref : public Layer {
	public:
		LayerGeoref();
		LayerGeoref(VikLayer * vl);
		LayerGeoref(Viewport * viewport);


		/* Layer interface methods. */
		void load_image(Viewport * viewport, bool from_file);
		void draw(Viewport * viewport);
		char const * tooltip();
		void marshall(uint8_t ** data, int * len);
		void add_menu_items(GtkMenu * menu, void * panel);
		bool properties(void * vp);
		void free_();
		bool set_param(uint16_t id, VikLayerParamData data, Viewport * viewport, bool is_file_operation);
		VikLayerParamData get_param(uint16_t id, bool is_file_operation);





		void create_image_file();
		void set_image(char const * image);
		struct LatLon get_ll_tl();
		struct LatLon get_ll_br();
		void align_utm2ll();
		void align_ll2utm();
		void align_coords();
		void check_br_is_good_or_msg_user();
		void calculate_mpp_from_coords(GtkWidget * ww);
		bool dialog(Viewport * viewport, GtkWindow * w);
		bool move_release(GdkEventButton * event, Viewport * viewport);
		bool zoom_press(GdkEventButton * event, Viewport * viewport);
		bool move_press(GdkEventButton * event, Viewport * viewport);





		char * image;
		GdkPixbuf * pixbuf;
		uint8_t alpha;

		struct UTM corner; // Top Left
		double mpp_easting;
		double mpp_northing;
		struct LatLon ll_br; // Bottom Right
		unsigned int width;
		unsigned int height;

		GdkPixbuf * scaled;
		uint32_t scaled_width;
		uint32_t scaled_height;

		int click_x;
		int click_y;
		changeable_widgets cw;

	};





}





#endif
