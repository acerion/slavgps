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
#include "widget_image_alpha.h"




namespace SlavGPS {




	class GisViewport;
	class FileSelectorWidget;
	class LayerGeoref;
	class WorldFile;




	void layer_georef_init(void);




	class GeorefConfigDialog : public BasicDialog {
		Q_OBJECT
	public:
		GeorefConfigDialog(LayerGeoref * the_layer, QWidget * parent = NULL);

		void sync_coords_in_entries(void);
		LatLon get_lat_lon_tl(void) const;
		LatLon get_lat_lon_br(void) const;
		void check_br_is_good_or_msg_user(void);
		void set_widget_values(const WorldFile & wfile);

		FileSelectorWidget * map_image_file_selector = NULL;
		FileSelectorWidget * world_file_selector = NULL;

		QDoubleSpinBox * x_scale_spin = NULL;
		QDoubleSpinBox * y_scale_spin = NULL;


		QComboBox * coord_mode_combo = NULL;

		/* Because of how I switch between two coordinate
		   modes, and how I display them depending on state of
		   "coord_mode_combo", I have to have equal number of
		   widgets for UTM mode and for LatLon mode. */
		UTMEntryWidget * utm_entry = NULL;
		QWidget * dummy_entry1 = NULL;
		QWidget * dummy_entry2 = NULL;

		LatLonEntryWidget * lat_lon_tl_entry = NULL;
		LatLonEntryWidget * lat_lon_br_entry = NULL;
		QPushButton * calc_mpp_button = NULL;


		ImageAlphaWidget * alpha_slider = nullptr;

	public slots:
		void load_world_file_cb(void);
		void coord_mode_changed_cb(int combo_index);
		void calculate_mpp_from_coords_cb(void);

	private:
		void sync_from_utm_to_lat_lon(void);
		void sync_from_lat_lon_to_utm(void);


		LayerGeoref * layer = NULL; /* Only a reference. */
	};




	class LayerGeorefInterface : public LayerInterface {
	public:
		LayerGeorefInterface();
		Layer * unmarshall(Pickle & pickle, GisViewport * gisview);
		LayerToolContainer create_tools(Window * window, GisViewport * gisview);
	};




	class LayerGeoref : public Layer {
		Q_OBJECT
	public:
		LayerGeoref();
		LayerGeoref(GisViewport * gisview);
		~LayerGeoref();


		/* Layer interface methods. */
		sg_ret post_read(GisViewport * gisview, bool from_file) override;
		void draw_tree_item(GisViewport * gisview, bool highlight_selected, bool parent_is_selected);
		QString get_tooltip(void) const;
		sg_ret menu_add_type_specific_operations(QMenu & menu, bool in_tree_view) override;
		bool show_properties_dialog(GisViewport * gisview);
		bool set_param_value(param_id_t param_id, const SGVariant & param_value, bool is_file_operation);
		SGVariant get_param_value(param_id_t param_id, bool is_file_operation) const override;

		sg_ret get_values_from_dialog(const GeorefConfigDialog & dialog);
		void reset_pixmaps(void);


		void configure_from_viewport(const GisViewport * gisview);


		void create_image_file(void);
		void set_image_file_full_path(const QString & full_path);

		bool run_dialog(GisViewport * gisview, QWidget * parent);

		LayerTool::Status move_release(QMouseEvent * event, LayerTool * tool);
		LayerTool::Status zoom_press(QMouseEvent * event, LayerTool * tool);
		LayerTool::Status move_press(QMouseEvent * event, LayerTool * tool);


	public slots:
		void zoom_to_fit_cb(void);
		void goto_center_cb(void);
		void export_params_cb(void);

	public:
		QPixmap image;
		int image_width = 0;
		int image_height = 0;
		QString image_file_full_path;

		QString world_file_full_path;

		ImageAlpha alpha;

		UTM utm_tl; /* Top Left. */
		double mpp_easting = 0.0;
		double mpp_northing = 0.0;

		LatLon lat_lon_tl; /* Top Left. */
		LatLon lat_lon_br; /* Bottom Right. */


		QPixmap scaled_image;
		int scaled_image_width = 0;
		int scaled_image_height = 0;

		int click_x = -1;
		int click_y = -1;
	};




	LayerGeoref * georef_layer_create(GisViewport * gisview, const QString & name, QPixmap * pixmap, const Coord & coord_tr, const Coord & coord_br);




	class LayerToolGeorefMove : public LayerTool {
	public:
		LayerToolGeorefMove(Window * window, GisViewport * gisview);

		SGObjectTypeID get_tool_id(void) const override;
		static SGObjectTypeID tool_id(void);

	private:
		LayerTool::Status handle_mouse_click(Layer * layer, QMouseEvent * event) override;
		LayerTool::Status handle_mouse_release(Layer * layer, QMouseEvent * event) override;
	};

	class LayerToolGeorefZoom : public LayerTool {
	public:
		LayerToolGeorefZoom(Window * window, GisViewport * gisview);

		SGObjectTypeID get_tool_id(void) const override;
		static SGObjectTypeID tool_id(void);

	private:
		LayerTool::Status handle_mouse_click(Layer * layer, QMouseEvent * event) override;
	};




}




#endif /* #ifndef _SG_LAYER_GEOREF_H_ */
