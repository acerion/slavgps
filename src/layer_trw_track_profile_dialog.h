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
	class Viewport2D;
	class ViewportCanvas;
	class LayerTRW;
	class Track;
	class Trackpoint;
	class ProfileView;
	class ScreenPos;




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

		sg_ret draw_center(ProfileView * graph);
		sg_ret draw_left(ProfileView * graph);
		sg_ret draw_bottom(ProfileView * graph);

		ProfileView * find_view(Viewport2D * viewport) const;
		ProfileView * get_current_view(void) const;


		LayerTRW * trw = NULL;
		Viewport * main_viewport = NULL;
		Track * trk = NULL;

		std::vector<ProfileView *> graphs;

	private slots:
		void checkbutton_toggle_cb(void);
		void dialog_response_cb(int resp);
		void destroy_cb(void);

		sg_ret paint_center_cb(Viewport2D * viewport);
		sg_ret paint_left_cb(Viewport2D * viewport);
		sg_ret paint_bottom_cb(Viewport2D * viewport);

		void handle_cursor_move_cb(Viewport * viewport, QMouseEvent * ev);
		void handle_mouse_button_release_cb(Viewport * viewport, QMouseEvent * event);

	private:
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




	class ProfileView : public QWidget {
		Q_OBJECT
	public:
		ProfileView(GeoCanvasDomain x_domain, GeoCanvasDomain y_domain, TrackProfileDialog * dialog, QWidget * parent = NULL);
		virtual ~ProfileView();

		virtual void draw_additional_indicators(Track * trk) {};
		virtual void configure_controls(void) {};
		virtual void save_values(void) {};

		void configure_labels(void);
		void create_widgets_layout(void);

		void create_viewport(TrackProfileDialog * dialog, GeoCanvasDomain x_domain, GeoCanvasDomain y_domain);
		QString get_graph_title(void) const;

		sg_ret set_pos_y(ScreenPos & screen_pos);

		/* Get position of cursor on a graph. 'x' coordinate
		   will match current 'x' position of cursor, and 'y'
		   coordinate will be on a graph line that corresponds
		   with the 'x' position. */
		sg_ret get_cursor_pos_on_line(QMouseEvent * ev, ScreenPos & screen_pos);

		sg_ret set_initial_visible_range_x_distance(void);
		sg_ret set_initial_visible_range_x_time(void);
		sg_ret set_initial_visible_range_y(void);

		/* Get position of a cursor within a graph. This
		   function doesn't care if 'x' or 'y' coordinate is
		   on a graph line. It just reports cursor
		   position. */
		sg_ret get_cursor_pos(QMouseEvent * ev, ScreenPos & screen_pos) const;

		sg_ret get_position_of_tp(Track * trk, int idx, ScreenPos & screen_pos);

		sg_ret regenerate_data(Track * trk);
		sg_ret regenerate_sizes(void);

		sg_ret draw_graph(Track * trk);
		sg_ret draw_marks(const ScreenPos & selected_pos, const ScreenPos & current_pos, bool & is_selected_drawn, bool & is_current_drawn);

		void draw_function_values(void);

		void draw_dem_alt_speed_dist(Track * trk, bool do_dem, bool do_speed);
		void draw_speed_dist(Track * trk);

		void draw_grid_horizontal_line(int pos_y, const QString & label);
		void draw_grid_vertical_line(int pos_x, const QString & label);

		/* Check whether given combination of x/y domains is supported by ProfileView. */
		static bool supported_domains(GeoCanvasDomain x_domain, GeoCanvasDomain y_domain);

		void draw_x_grid_inside(const Track * trk);
		void draw_x_grid_outside(const Track * trk);
		void draw_y_grid_inside(void);
		void draw_y_grid_outside(void);

		void draw_x_grid_sub_d_inside(void);
		void draw_x_grid_sub_d_outside(void);
		void draw_x_grid_sub_t_inside(void);
		void draw_x_grid_sub_t_outside(void);

		QString get_y_grid_label(float value);


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

		TrackProfileDialog * dialog = NULL;


		QPen main_pen;
		QPen gps_speed_pen;
		QPen dem_alt_pen;
		QPen no_alt_info_pen;

		Viewport2D * viewport2d = NULL;
		GeoCanvasLabels labels;

		QGridLayout * labels_grid = NULL;
		QVBoxLayout * main_vbox = NULL;
		QVBoxLayout * controls_vbox = NULL;

	private:
		sg_ret regenerate_data_from_scratch(Track * trk);
	};




	/* ET = elevation as a function of time. */
	class ProfileViewET : public ProfileView {
	public:
		ProfileViewET(TrackProfileDialog * dialog);
		~ProfileViewET() {};

		void draw_additional_indicators(Track * trk) override;
		void configure_controls(void) override;
		void save_values(void) override;
	private:
		QCheckBox * show_dem_cb = NULL;
		QCheckBox * show_speed_cb = NULL;
	};


	/* SD = speed as a function of distance. */
	class ProfileViewSD : public ProfileView {
	public:
		ProfileViewSD(TrackProfileDialog * dialog);
		~ProfileViewSD() {};

		void draw_additional_indicators(Track * trk) override;
		void configure_controls(void) override;
		void save_values(void) override;
	private:
		QCheckBox * show_gps_speed_cb = NULL;
	};


	/* ED = elevation as a function of distance. */
	class ProfileViewED : public ProfileView {
	public:
		ProfileViewED(TrackProfileDialog * dialog);
		~ProfileViewED() {};

		void draw_additional_indicators(Track * trk) override;
		void configure_controls(void) override;
		void save_values(void) override;
	private:
		QCheckBox * show_dem_cb = NULL;
		QCheckBox * show_gps_speed_cb = NULL;
	};


	/* GD = gradient as a function of distance. */
	class ProfileViewGD : public ProfileView {
	public:
		ProfileViewGD(TrackProfileDialog * dialog);
		~ProfileViewGD() {};

		void draw_additional_indicators(Track * trk) override;
		void configure_controls(void) override;
		void save_values(void) override;
	private:
		QCheckBox * show_gps_speed_cb = NULL;
	};


	/* ST = speed as a function of time. */
	class ProfileViewST : public ProfileView {
	public:
		ProfileViewST(TrackProfileDialog * dialog);
		~ProfileViewST() {};

		void draw_additional_indicators(Track * trk) override;
		void configure_controls(void) override;
		void save_values(void) override;
	private:
		QCheckBox * show_gps_speed_cb = NULL;
	};


	/* DT = distance as a function of time. */
	class ProfileViewDT : public ProfileView {
	public:
		ProfileViewDT(TrackProfileDialog * dialog);
		~ProfileViewDT() {};

		void draw_additional_indicators(Track * trk) override;
		void configure_controls(void) override;
		void save_values(void) override;
	private:
		QCheckBox * show_speed_cb = NULL;
	};




	void track_profile_dialog(Track * trk, Viewport * main_viewport, QWidget * parent);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TRACK_PROFILE_DIALOG_H_ */
