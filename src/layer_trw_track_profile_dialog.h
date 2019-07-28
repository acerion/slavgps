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




#include "viewport_pixmap.h"
#include "layer_trw_track_internal.h"
#include "measurements.h"




namespace SlavGPS {




	class Window;
	class GisViewport;
	class LayerTRW;
	class Track;
	class Trackpoint;
	class ProfileView;
	class ScreenPos;




	class TrackViewLabels {
	public:
		QLabel * x_value = NULL;
		QLabel * y_value = NULL;
		QLabel * t_value = NULL; /* Actual clock time, only for time-based graphs. */

		QLabel * x_label = NULL;
		QLabel * y_label = NULL;
		QLabel * t_label = NULL;
	};




	class Graph2D : public ViewportPixmap {
		Q_OBJECT
	public:
		Graph2D(int left = 0, int right = 0, int top = 0, int bottom = 0, QWidget * parent = NULL);

		/* Get cursor position of a mouse event.  Returned
		   position is in "beginning is in bottom-left corner"
		   coordinate system. */
		sg_ret cbl_get_cursor_pos(QMouseEvent * ev, ScreenPos & screen_pos) const;


		void mousePressEvent(QMouseEvent * event); /* Double click is handled through event filter. */
		void mouseMoveEvent(QMouseEvent * event);
		void mouseReleaseEvent(QMouseEvent * event);

		GisViewportDomain x_domain = GisViewportDomain::Max;
		GisViewportDomain y_domain = GisViewportDomain::Max;

		HeightUnit height_unit;
		DistanceUnit distance_unit;
		SpeedUnit speed_unit;
	};




	class TrackProfileDialog : public QDialog {
		Q_OBJECT
	public:
		TrackProfileDialog() {};
		TrackProfileDialog(QString const & title, Track * trk, GisViewport * main_gisview, QWidget * parent = NULL);
		~TrackProfileDialog();

		void clear_image(QPixmap * pix);

		void draw_all_views(bool resized);

		void save_values(void);

		ProfileView * find_view(Graph2D * graph_2d) const;
		ProfileView * get_current_view(void) const;


		LayerTRW * trw = NULL;
		GisViewport * main_gisview = NULL;
		Track * trk = NULL;

		std::vector<ProfileView *> views;

	private slots:
		void checkbutton_toggle_cb(void);
		void dialog_response_cb(int resp);
		void destroy_cb(void);
		sg_ret paint_graph_cb(ViewportPixmap * pixmap);

		void handle_cursor_move_cb(ViewportPixmap * vpixmap, QMouseEvent * ev);
		void handle_mouse_button_release_cb(ViewportPixmap * vpixmap, QMouseEvent * event);

	private:
		/* Pen used to draw main parts of views (i.e. the values of functions y = f(x)). */
		QPen main_pen;

		QTabWidget * tabs = NULL;

		QDialogButtonBox * button_box = NULL;
		QPushButton * button_cancel = NULL;
		QPushButton * button_split_at_marker = NULL;
		QPushButton * button_ok = NULL;

		int profile_width;
		int profile_height;

		QSignalMapper * signal_mapper = NULL;
	};




	class ProfileView : public QWidget {
		Q_OBJECT
	public:
		ProfileView(GisViewportDomain x_domain, GisViewportDomain y_domain, TrackProfileDialog * dialog, QWidget * parent = NULL);
		virtual ~ProfileView();

		virtual void draw_additional_indicators(Track * trk) {};
		virtual void configure_controls(void) {};
		virtual void save_values(void) {};


		int get_central_width_px(void) const;
		int get_central_height_px(void) const;


		sg_ret draw_track_and_crosshairs(Track * trk);

		void create_graph_2d(void);
		void configure_labels(void);
		void create_widgets_layout(void);

		void configure_title(void);
		const QString & get_title(void) const;

		/**
		   Set y position of argument that matches x position of argument.
		   In other words: find y = f(x), where f() is current graph.

		   Returned cursor position is in "beginning of
		   coordinate system (position 0,0) is in bottom-left
		   corner".
		   cbl = coordinate-bottom-left.
		*/
		sg_ret set_pos_y_cbl(ScreenPos & screen_pos);

		/**
		   Get position of cursor on a 2d graph. 'x'
		   coordinate will match current 'x' position of
		   cursor, and 'y' coordinate will be on a 2d graph
		   line that corresponds with the 'x' position.

		   Returned cursor position is in "beginning of
		   coordinate system (position 0,0) is in bottom-left
		   corner".
		   cbl = coordinate-bottom-left.
		*/
		sg_ret cbl_get_cursor_pos_on_line(QMouseEvent * ev, ScreenPos & screen_pos);

		sg_ret set_initial_visible_range_x_distance(void);
		sg_ret set_initial_visible_range_x_time(void);
		sg_ret set_initial_visible_range_y(void);

		/**
		   Calculate y position for crosshair on y=f(x) graph.
		   The position will be in "beginning of coordinates system is in bottom-left corner".
		   cbl = coordinate-bottom-left.
		*/
		sg_ret get_position_cbl_of_tp(Track * trk, tp_idx tp_idx, ScreenPos & screen_pos);

		sg_ret regenerate_data(Track * trk);

		sg_ret draw_graph_without_crosshairs(Track * trk);
		sg_ret draw_crosshairs(const ScreenPos & selected_tp_pos, const ScreenPos & cursor_pos);

		void draw_function_values(void);

		void draw_dem_alt_speed_dist(Track * trk, bool do_dem, bool do_speed);
		void draw_speed_dist(Track * trk);

		/* Check whether given combination of x/y domains is supported by ProfileView. */
		static bool supported_domains(GisViewportDomain x_domain, GisViewportDomain y_domain);

		void draw_x_grid(const Track * trk);
		void draw_y_grid(void);

		void draw_x_grid_d_domain(void);
		void draw_x_grid_t_domain(void);

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

		GisViewportDomain x_domain = GisViewportDomain::Max;
		GisViewportDomain y_domain = GisViewportDomain::Max;

		Graph2D * graph_2d = NULL;
		TrackViewLabels labels;

		QGridLayout * labels_grid = NULL;
		QVBoxLayout * main_vbox = NULL;
		QVBoxLayout * controls_vbox = NULL;

	private:
		sg_ret regenerate_data_from_scratch(Track * trk);
		QString title;
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




	void track_profile_dialog(Track * trk, GisViewport * main_gisview, QWidget * parent);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TRACK_PROFILE_DIALOG_H_ */
