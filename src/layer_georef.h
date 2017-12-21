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
#include "layer_interface.h"
#include "layer_tool.h"




namespace SlavGPS {




	class Viewport;
	class SGFileEntry;




	void vik_georef_layer_init(void);




	class widgets_group {
	public:
		QDoubleSpinBox * x_scale_spin = NULL;
		QDoubleSpinBox * y_scale_spin = NULL;
		/* UTM widgets. */
		QDoubleSpinBox * easting_spin = NULL;
		QDoubleSpinBox * northing_spin = NULL;

		QSpinBox * utm_zone_spin = NULL;
		QLineEdit * utm_letter_entry = NULL;

		QDoubleSpinBox * lat_tl_spin = NULL;
		QDoubleSpinBox * lon_tl_spin = NULL;
		QDoubleSpinBox * lat_br_spin = NULL;
		QDoubleSpinBox * lon_br_spin = NULL;

		//GtkWidget * tabs = NULL;
		SGFileEntry * map_image_file_entry = NULL;
		SGFileEntry * world_file_entry = NULL;
	};




	class LayerGeorefInterface : public LayerInterface {
	public:
		LayerGeorefInterface();
		Layer * unmarshall(uint8_t * data, size_t data_len, Viewport * viewport);
		LayerToolContainer * create_tools(Window * window, Viewport * viewport);
	};




	class LayerGeoref : public Layer {
		Q_OBJECT
	public:
		LayerGeoref();
		LayerGeoref(Viewport * viewport);
		~LayerGeoref();


		/* Layer interface methods. */
		void post_read(Viewport * viewport, bool from_file);
		void draw(Viewport * viewport);
		QString get_tooltip();
		void add_menu_items(QMenu & menu);
		bool properties_dialog(Viewport * viewport);
		bool set_param_value(uint16_t id, const SGVariant & param_value, bool is_file_operation);
		SGVariant get_param_value(param_id_t id, bool is_file_operation) const;



		void configure_from_viewport(Viewport const * viewport);


		void create_image_file();
		void set_image(const QString & image);
		LatLon get_ll_tl();
		LatLon get_ll_br();
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
		void zoom_to_fit_cb(void);
		void goto_center_cb(void);
		void export_params_cb(void);


	public:

		QString image;
		QPixmap * pixmap = NULL;
		uint8_t alpha = 255;

		UTM corner; /* Top Left. */
		double mpp_easting = 0.0;
		double mpp_northing = 0.0;
		LatLon ll_br; /* Bottom Right. */
		unsigned int width = 0;
		unsigned int height = 0;

		QPixmap * scaled = NULL;
		int scaled_width = 0;
		int scaled_height = 0;

		int click_x = -1;
		int click_y = -1;
		widgets_group cw;
	};




	LayerGeoref * georef_layer_create(Viewport * viewport, const QString & name, QPixmap * pixmap, const Coord & coord_tr, const Coord & coord_br);




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
