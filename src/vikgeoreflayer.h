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

#ifndef _SG_GEOREFLAYER_H_
#define _SG_GEOREFLAYER_H_




#include <cstdint>

#include "layer.h"




namespace SlavGPS {




	class Viewport;




	void vik_georef_layer_init(void);




	typedef struct {
		GtkWidget * x_spin;
		GtkWidget * y_spin;
		/* UTM widgets. */
		GtkWidget * ce_spin; /* Top left. */
		GtkWidget * cn_spin;
		GtkWidget * utm_zone_spin;
		GtkWidget * utm_letter_entry;

		GtkWidget * lat_tl_spin;
		GtkWidget * lon_tl_spin;
		GtkWidget * lat_br_spin;
		GtkWidget * lon_br_spin;

		GtkWidget * tabs;
		GtkWidget * imageentry;
	} changeable_widgets;





	class LayerGeoref : public Layer {
	public:
		LayerGeoref();
		LayerGeoref(Viewport * viewport);
		~LayerGeoref();


		/* Layer interface methods. */
		void post_read(Viewport * viewport, bool from_file);
		void draw(Viewport * viewport);
		char const * tooltip();
		void add_menu_items(QMenu & menu, void * panel);
		bool properties_dialog(Viewport * viewport);
		bool set_param_value(uint16_t id, LayerParamValue param_value, Viewport * viewport, bool is_file_operation);
		LayerParamValue get_param_value(layer_param_id_t id, bool is_file_operation) const;





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
		bool move_release(GdkEventButton * event, LayerTool * tool);
		bool zoom_press(GdkEventButton * event, LayerTool * tool);
		bool move_press(GdkEventButton * event, LayerTool * tool);





		char * image = NULL;
		GdkPixbuf * pixbuf = NULL;
		uint8_t alpha = 255;

		struct UTM corner; /* Top Left. */
		double mpp_easting = 0.0;
		double mpp_northing = 0.0;
		struct LatLon ll_br = { 0.0, 0.0 }; /* Bottom Right. */
		unsigned int width = 0;
		unsigned int height = 0;

		GdkPixbuf * scaled = NULL;
		uint32_t scaled_width = 0;
		uint32_t scaled_height = 0;

		int click_x = -1;
		int click_y = -1;
		changeable_widgets cw;
	};




	LayerGeoref * vik_georef_layer_create(Viewport * viewport,
					      const char *name,
					      GdkPixbuf *pibxbuf,
					      VikCoord *coord_tr,
					      VikCoord *coord_br );




}




#endif /* #ifndef _SG_GEOREFLAYER_H_ */
