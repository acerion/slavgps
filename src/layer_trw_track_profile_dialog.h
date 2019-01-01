/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2016-2018, Kamil Ignacak <acerion@wp.pl>
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

#ifndef _SG_TRACK_PROFILE_DIALOG_H_
#define _SG_TRACK_PROFILE_DIALOG_H_




#include <cstdint>
#include <vector>




#include <QObject>
#include <QDialog>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QCheckBox>
#include <QMouseEvent>
#include <QPen>
#include <QSignalMapper>
#include <QDialogButtonBox>
#include <QGridLayout>




#include "layer_trw_track_internal.h"
#include "measurements.h"




namespace SlavGPS {




	class Window;
	class Viewport;
	class LayerTRW;
	class Track;
	class Trackpoint;
	class ProfileGraph;
	class ScreenPos;




	class PropSaved {
	public:
		bool valid;
		QPixmap img;
	};




	enum class GeoCanvasDomain {
		Time = 0,
		Elevation,
		Distance,
		Speed,
		Gradient,
		Max
	};




	class GeoCanvas {
	public:
		GeoCanvas();

		GeoCanvasDomain x_domain;
		GeoCanvasDomain y_domain;

		HeightUnit height_unit;
		DistanceUnit distance_unit;
		SpeedUnit speed_unit;
	};




	class GeoCanvasLabels {
	public:
		QLabel * x_value = NULL;
		QLabel * y_value = NULL;
		QLabel * t_value = NULL; /* Actual clock time, only for time-based graphs. */

		QLabel * x_label = NULL;
		QLabel * y_label = NULL;
		QLabel * t_label = NULL;
	};




	class TrackProfileDialog : public QDialog {
		Q_OBJECT
	public:
		TrackProfileDialog() {};
		TrackProfileDialog(QString const & title, Track * trk, Viewport * main_viewport, QWidget * parent = NULL);
		~TrackProfileDialog();

		void clear_image(QPixmap * pix);

		void draw_all_graphs(bool resized);

		void save_values(void);

		sg_ret draw_single_graph(ProfileGraph * graph);

		ProfileGraph * get_current_graph(void) const;


		LayerTRW * trw = NULL;
		Viewport * main_viewport = NULL;
		Track * trk = NULL;

		/* 1D array to be used as (sparse) 2D array.  With 2D
		   array, indexed by x/y domains, we can have all
		   kinds of graphs. */
		std::vector<ProfileGraph *> graphs;

	private slots:
		void checkbutton_toggle_cb(void);
		void dialog_response_cb(int resp);
		void destroy_cb(void);
		bool paint_to_viewport_cb(Viewport * viewport);

		void handle_cursor_move_cb(Viewport * viewport, QMouseEvent * ev);
		void handle_mouse_button_release_cb(Viewport * viewport, QMouseEvent * event);

	private:
		sg_ret get_cursor_pos_by_distance(QMouseEvent * ev, ProfileGraph * graph, double & meters_from_start, ScreenPos & selected_pos, ScreenPos & current_pos);
		sg_ret get_cursor_pos_by_time(QMouseEvent * ev, ProfileGraph * graph, ScreenPos & selected_pos, ScreenPos & current_pos);



		/* Trackpoint selected by clicking in chart. Will be marked in a viewport by non-moving crosshair. */
		bool is_selected_drawn = false;
		/* Trackpoint that is closest to current position of *hovering* cursor. */
		bool is_current_drawn = false;

		//char * tz = NULL; /* TimeZone at track's location. */

		/* Pen used to draw main parts of graphs (i.e. the values of functions y = f(x)). */
		QPen main_pen;

		QTabWidget * tabs = NULL;

		QDialogButtonBox * button_box = NULL;
		QPushButton * button_cancel = NULL;
		QPushButton * button_split_at_marker = NULL;
		QPushButton * button_split_segments = NULL;
		QPushButton * button_reverse = NULL;
		QPushButton * button_ok = NULL;

		int profile_width;
		int profile_height;

		QSignalMapper * signal_mapper = NULL;
	};




	class ProfileGraph {
	public:
		ProfileGraph(GeoCanvasDomain x_domain, GeoCanvasDomain y_domain, TrackProfileDialog * dialog);
		virtual ~ProfileGraph();

		virtual void draw_additional_indicators(Track * trk) {};
		virtual void configure_controls(TrackProfileDialog * dialog) {};
		virtual void save_values(void) {};

		void configure_labels(TrackProfileDialog * dialog);
		QWidget * create_widgets_layout(TrackProfileDialog * dialog);

		void create_viewport(TrackProfileDialog * dialog);
		QString get_graph_title(void) const;

		double get_pos_y(double pos_x);

		sg_ret set_initial_visible_range_x_distance(void);
		sg_ret set_initial_visible_range_x_time(void);
		sg_ret set_initial_visible_range_y(void);

		int get_cursor_pos_x(QMouseEvent * ev) const;

		QPointF get_position_of_tp(Track * trk, int idx);

		sg_ret regenerate_data(Track * trk);
		sg_ret regenerate_sizes(void);

		sg_ret draw_graph(Track * trk);
		sg_ret draw_marks(const ScreenPos & selected_pos, const ScreenPos & current_pos, bool & is_selected_drawn, bool & is_current_drawn);

		void draw_function_values(void);

		void draw_dem_alt_speed_dist(Track * trk, bool do_dem, bool do_speed);
		void draw_speed_dist(Track * trk);

		void draw_grid_horizontal_line(int pos_y, const QString & label);
		void draw_grid_vertical_line(int pos_x, const QString & label);

		bool has_supported_domains(void) const;


		void draw_x_grid(const Track * trk);
		void draw_y_grid(void);

		void draw_x_grid_sub_d(void);
		void draw_x_grid_sub_t(void);

		QString get_y_grid_label(float value);

		Viewport * viewport = NULL;
		PropSaved saved_img;

		int width = 0;
		int height = 0;
		int bottom_edge = 0;
		int left_edge = 0;

		/* For distance-based graphs. */
		Distance x_interval_d = { 0.0 };
		Distance x_min_visible_d = { 0.0 };
		Distance x_max_visible_d = { 0.0 };

		/* For time-based graphs. */
		Time x_interval_t = { 0 };
		Time x_min_visible_t = { 0 };
		Time x_max_visible_t = { 0 };

		double y_interval = 0.0;
		double y_min_visible = 0.0;
		double y_max_visible = 0.0;

		TrackData track_data;     /* Compressed. */
		TrackData track_data_raw; /* Raw = uncompressed. */


		GeoCanvas geocanvas;

		QPen main_pen;
		QPen gps_speed_pen;
		QPen dem_alt_pen;
		QPen no_alt_info_pen;

		GeoCanvasLabels labels;

		QGridLayout * labels_grid = NULL;
		QVBoxLayout * main_vbox = NULL;
		QVBoxLayout * controls_vbox = NULL;

	private:
		sg_ret regenerate_data_from_scratch(Track * trk);
	};




	/* ET = elevation as a function of time. */
	class ProfileGraphET : public ProfileGraph {
	public:
		ProfileGraphET(TrackProfileDialog * dialog);
		~ProfileGraphET() {};

		void draw_additional_indicators(Track * trk) override;
		void configure_controls(TrackProfileDialog * dialog) override;
		void save_values(void) override;
	private:
		QCheckBox * show_dem_cb = NULL;
		QCheckBox * show_speed_cb = NULL;
	};


	/* SD = speed as a function of distance. */
	class ProfileGraphSD : public ProfileGraph {
	public:
		ProfileGraphSD(TrackProfileDialog * dialog);
		~ProfileGraphSD() {};

		void draw_additional_indicators(Track * trk) override;
		void configure_controls(TrackProfileDialog * dialog) override;
		void save_values(void) override;
	private:
		QCheckBox * show_gps_speed_cb = NULL;
	};


	/* ED = elevation as a function of distance. */
	class ProfileGraphED : public ProfileGraph {
	public:
		ProfileGraphED(TrackProfileDialog * dialog);
		~ProfileGraphED() {};

		void draw_additional_indicators(Track * trk) override;
		void configure_controls(TrackProfileDialog * dialog) override;
		void save_values(void) override;
	private:
		QCheckBox * show_dem_cb = NULL;
		QCheckBox * show_gps_speed_cb = NULL;
	};


	/* GD = gradient as a function of distance. */
	class ProfileGraphGD : public ProfileGraph {
	public:
		ProfileGraphGD(TrackProfileDialog * dialog);
		~ProfileGraphGD() {};

		void draw_additional_indicators(Track * trk) override;
		void configure_controls(TrackProfileDialog * dialog) override;
		void save_values(void) override;
	private:
		QCheckBox * show_gps_speed_cb = NULL;
	};


	/* ST = speed as a function of time. */
	class ProfileGraphST : public ProfileGraph {
	public:
		ProfileGraphST(TrackProfileDialog * dialog);
		~ProfileGraphST() {};

		void draw_additional_indicators(Track * trk) override;
		void configure_controls(TrackProfileDialog * dialog) override;
		void save_values(void) override;
	private:
		QCheckBox * show_gps_speed_cb = NULL;
	};


	/* DT = distance as a function of time. */
	class ProfileGraphDT : public ProfileGraph {
	public:
		ProfileGraphDT(TrackProfileDialog * dialog);
		~ProfileGraphDT() {};

		void draw_additional_indicators(Track * trk) override;
		void configure_controls(TrackProfileDialog * dialog) override;
		void save_values(void) override;
	private:
		QCheckBox * show_speed_cb = NULL;
	};




	void track_profile_dialog(Track * trk, Viewport * main_viewport, QWidget * parent);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TRACK_PROFILE_DIALOG_H_ */
