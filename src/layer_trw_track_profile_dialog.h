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




	/*
	  Crosshair that knows its position in 2D graph and in Qt
	  widget. See comments on x/y members for details.
	*/
	class Crosshair2D {
	public:
		/*
		  Coordinates valid only in central area of 2d graph,
		  and following 'beginning in bottom-left corner'
		  coordinate system.

		  So if the two values are zero, they indicate
		  bottom-left corner of central area of 2d graph.
		*/
		int central_cbl_x = 0;
		int central_cbl_y = 0;

		/*
		  Global coordinates in Qt widget, following
		  'beginning in top-left corner' convention.
		*/
		int x = 0;
		int y = 0;

		QString debug;

		bool valid = false;
	};





	class Graph2D : public ViewportPixmap {
		Q_OBJECT
	public:
		Graph2D(int left = 0, int right = 0, int top = 0, int bottom = 0, QWidget * parent = NULL);

		/* Get cursor position of a mouse event.  Returned
		   position is in "beginning is in bottom-left corner"
		   coordinate system. */
		sg_ret cbl_get_cursor_pos(QMouseEvent * ev, ScreenPos & screen_pos) const;

		void central_draw_simple_crosshair(const Crosshair2D & crosshair);

		/* How many rows/columns are there to draw? */
		int central_get_n_columns(void) const;
		int central_get_n_rows(void) const;

		void mousePressEvent(QMouseEvent * event); /* Double click is handled through event filter. */
		void mouseMoveEvent(QMouseEvent * event);
		void mouseReleaseEvent(QMouseEvent * event);

		GisViewportDomain x_domain = GisViewportDomain::Max;
		GisViewportDomain y_domain = GisViewportDomain::Max;

		HeightUnit height_unit;
		DistanceUnit distance_unit;
		SpeedUnit speed_unit;

		/* Calculated on every resize event. Having cached
		   values saves a minimal amount of time when we move
		   cursor back and forth across the graph and
		   calculate cursor location on a track. */
		int cached_central_n_columns = 0;
		int cached_central_n_rows = 0;
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
		sg_ret handle_graph_resize_cb(ViewportPixmap * pixmap);

		void handle_cursor_move_cb(ViewportPixmap * vpixmap, QMouseEvent * ev);
		void handle_mouse_button_release_cb(ViewportPixmap * vpixmap, QMouseEvent * event);

	private:
		sg_ret set_center_at_selected_tp(QMouseEvent * ev, const Graph2D * graph_2d, int graph_2d_central_n_columns);


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


		int get_central_n_columns(void) const;
		int get_central_n_rows(void) const;


		sg_ret draw_track_and_crosshairs(Track * trk);

		void create_graph_2d(void);
		void configure_labels(void);
		void create_widgets_layout(void);

		void configure_title(void);
		const QString & get_title(void) const;

		/**
		   Find y position of argument that matches x position of argument.
		   In other words: find y = f(x), where f() is current graph.

		   Returned cursor position is in "beginning of
		   coordinate system (position 0,0) is in bottom-left
		   corner".
		   cbl = coordinate-bottom-left.
		*/
		sg_ret cbl_find_y_on_graph_line(const int cbl_x, int & cbl_y);

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
		Crosshair2D get_cursor_pos_on_line(QMouseEvent * ev);

		sg_ret set_initial_visible_range_x_distance(void);
		sg_ret set_initial_visible_range_x_time(void);
		sg_ret set_initial_visible_range_y(void);

		/**
		   Calculate y position for crosshair on y=f(x) graph.
		   The position will be in "beginning of coordinates system is in bottom-left corner".
		   cbl = coordinate-bottom-left.
		*/
		Crosshair2D get_position_of_tp(Track * trk, tp_idx tp_idx);

		sg_ret regenerate_data(Track * trk);

		sg_ret draw_graph_without_crosshairs(Track * trk);
		sg_ret draw_crosshairs(const Crosshair2D & selected_tp, const Crosshair2D & cursor_pos);

		sg_ret draw_function_values(Track * trk);

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
		/*
		  For some graphs these values will be "padded" with
		  some margin so that there is some space between
		  top/bottom border of graph area and graph lines.

		  That way the graph won't touch top/bottom border
		  lines.
		*/
		double y_visible_min = 0.0;
		double y_visible_max = 0.0;
		/*
		  Difference between max and min (with margins
		  mentioned above). Calculated once, used many times.
		*/
		double y_visible_range_uu = 0.0;



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
