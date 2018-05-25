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

#ifndef _SG_LAYER_GEOREF_H_
#define _SG_LAYER_GEOREF_H_




#include <cstdint>

#include <QPixmap>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QComboBox>

#include "dialog.h"
#include "layer.h"
#include "layer_interface.h"
#include "layer_tool.h"
#include "widget_utm_entry.h"
#include "widget_lat_lon_entry.h"
#include "widget_slider.h"




namespace SlavGPS {




	class Viewport;
	class SGFileEntry;
	class LayerGeoref;




	void layer_georef_init(void);




	class GeorefConfigDialog : public BasicDialog {
		Q_OBJECT
	public:
		GeorefConfigDialog(LayerGeoref * the_layer, QWidget * parent = NULL);

		void sync_coords_in_entries(void);
		LatLon get_ll_tl(void) const;
		LatLon get_ll_br(void) const;
		void check_br_is_good_or_msg_user(void);

		SGFileEntry * map_image_file_entry = NULL;
		SGFileEntry * world_file_entry = NULL;

		QDoubleSpinBox * x_scale_spin = NULL;
		QDoubleSpinBox * y_scale_spin = NULL;


		QComboBox * coord_mode_combo = NULL;

		/* Because of how I switch between two coordinate
		   modes, and how I display them depending on state of
		   "coord_mode_combo", I have to have equal number of
		   widgets for UTM mode and for LatLon mode. */
		SGUTMEntry * utm_entry = NULL;
		QWidget * dummy_entry1 = NULL;
		QWidget * dummy_entry2 = NULL;

		SGLatLonEntry * lat_lon_tl_entry = NULL;
		SGLatLonEntry * lat_lon_br_entry = NULL;
		QPushButton * calc_mpp_button = NULL;


		SGSlider * alpha_slider = NULL;

	public slots:
		void load_cb(void);
		void coord_mode_changed_cb(int combo_index);
		void calculate_mpp_from_coords_cb(void);

	private:
		void set_widget_values(double values[4]);
		void sync_from_utm_to_lat_lon(void);
		void sync_from_lat_lon_to_utm(void);


		LayerGeoref * layer = NULL; /* Only a reference. */
	};




	class LayerGeorefInterface : public LayerInterface {
	public:
		LayerGeorefInterface();
		Layer * unmarshall(Pickle & pickle, Viewport * viewport);
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
		void draw_tree_item(Viewport * viewport, bool highlight_selected, bool parent_is_selected);
		QString get_tooltip();
		void add_menu_items(QMenu & menu);
		bool properties_dialog(Viewport * viewport);
		bool set_param_value(uint16_t id, const SGVariant & param_value, bool is_file_operation);
		SGVariant get_param_value(param_id_t id, bool is_file_operation) const;



		void configure_from_viewport(Viewport const * viewport);


		void create_image_file();
		void set_image_full_path(const QString & full_path);

		bool dialog(Viewport * viewport, Window * window);

		ToolStatus move_release(QMouseEvent * event, LayerTool * tool);
		ToolStatus zoom_press(QMouseEvent * event, LayerTool * tool);
		ToolStatus move_press(QMouseEvent * event, LayerTool * tool);


	public slots:
		void zoom_to_fit_cb(void);
		void goto_center_cb(void);
		void export_params_cb(void);

	public:
		QPixmap image;
		int image_width = 0;
		int image_height = 0;
		QString image_full_path;

		uint8_t alpha = 255;

		UTM utm_tl; /* Top Left. */
		double mpp_easting = 0.0;
		double mpp_northing = 0.0;
		LatLon ll_br; /* Bottom Right. */


		QPixmap scaled_image;
		int scaled_image_width = 0;
		int scaled_image_height = 0;

		int click_x = -1;
		int click_y = -1;
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




#endif /* #ifndef _SG_LAYER_GEOREF_H_ */
