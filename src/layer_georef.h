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

#ifndef _SG_GEOREFLAYER_H_
#define _SG_GEOREFLAYER_H_




#include <cstdint>

#include <QPixmap>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>

#include "layer.h"




namespace SlavGPS {




	class Viewport;
	class SGFileEntry;




	void vik_georef_layer_init(void);




	typedef struct {
		QDoubleSpinBox * x_spin = NULL;
		QDoubleSpinBox * y_spin = NULL;
		/* UTM widgets. */
		QDoubleSpinBox * ce_spin = NULL; /* Top left. */
		QDoubleSpinBox * cn_spin = NULL;
		QSpinBox utm_zone_spin;
		QLineEdit utm_letter_entry;

		QDoubleSpinBox lat_tl_spin;
		QDoubleSpinBox lon_tl_spin;
		QDoubleSpinBox lat_br_spin;
		QDoubleSpinBox lon_br_spin;

		GtkWidget * tabs = NULL;
		SGFileEntry * imageentry = NULL;
	} changeable_widgets;




	class LayerGeorefInterface : public LayerInterface {
	public:
		LayerGeorefInterface();
		Layer * unmarshall(uint8_t * data, int len, Viewport * viewport);
		LayerToolContainer * create_tools(Window * window, Viewport * viewport);
	};




	class LayerGeoref : public Layer {
	public:
		LayerGeoref();
		LayerGeoref(Viewport * viewport);
		~LayerGeoref();


		/* Layer interface methods. */
		void post_read(Viewport * viewport, bool from_file);
		void draw(Viewport * viewport);
		QString tooltip();
		void add_menu_items(QMenu & menu);
		bool properties_dialog(Viewport * viewport);
		bool set_param_value(uint16_t id, const SGVariant & param_value, bool is_file_operation);
		SGVariant get_param_value(param_id_t id, bool is_file_operation) const;



		void configure_from_viewport(Viewport const * viewport);


		void create_image_file();
		void set_image(const QString & image);
		struct LatLon get_ll_tl();
		struct LatLon get_ll_br();
		void align_utm2ll();
		void align_ll2utm();
		void align_coords();
		void check_br_is_good_or_msg_user();
		bool dialog(Viewport * viewport, Window * window);
		bool move_release(QMouseEvent * event, LayerTool * tool);
		bool zoom_press(QMouseEvent * event, LayerTool * tool);
		bool move_press(QMouseEvent * event, LayerTool * tool);


	public slots:
		void calculate_mpp_from_coords_cb(void);


	public:

		QString image;
		QPixmap * pixmap = NULL;
		uint8_t alpha = 255;

		struct UTM corner; /* Top Left. */
		double mpp_easting = 0.0;
		double mpp_northing = 0.0;
		struct LatLon ll_br = { 0.0, 0.0 }; /* Bottom Right. */
		unsigned int width = 0;
		unsigned int height = 0;

		QPixmap * scaled = NULL;
		uint32_t scaled_width = 0;
		uint32_t scaled_height = 0;

		int click_x = -1;
		int click_y = -1;
		changeable_widgets cw;
	};




	LayerGeoref * georef_layer_create(Viewport * viewport, const QString & name, QPixmap * pixmap, const Coord * coord_tr, const Coord * coord_br);




	class LayerToolGeorefMove : public LayerTool {
	public:
		LayerToolGeorefMove(Window * window, Viewport * viewport);

		ToolStatus handle_mouse_click(Layer * layer, QMouseEvent * event);
		ToolStatus handle_mouse_release(Layer * layer, QMouseEvent * event);
	};

	class LayerToolGeorefZoom : public LayerTool {
	public:
		LayerToolGeorefZoom(Window * window, Viewport * viewport);

		ToolStatus handle_mouse_click(Layer * layer, QMouseEvent * event);
	};




}




#endif /* #ifndef _SG_GEOREFLAYER_H_ */
