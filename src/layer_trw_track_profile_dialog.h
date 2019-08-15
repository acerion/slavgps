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
#include "layer_trw_track_data.h"
#include "measurements.h"




namespace SlavGPS {




	class Window;
	class GisViewport;
	class LayerTRW;
	class Track;
	class Trackpoint;
	class ProfileViewBase;
	class ScreenPos;




	class TrackViewLabels {
	public:
		QLabel * x_label = NULL;
		QLabel * x_value = NULL;

		QLabel * y_label = NULL;
		QLabel * y_value = NULL;

		/* Actual timestamp read directly from trackpoint. */
		QLabel * tp_timestamp_label = NULL;
		QLabel * tp_timestamp_value = NULL;
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

		ProfileViewBase * find_view(Graph2D * graph_2d) const;
		ProfileViewBase * get_current_view(void) const;


		LayerTRW * trw = NULL;
		GisViewport * main_gisview = NULL;
		Track * trk = NULL;

		std::vector<ProfileViewBase *> views;

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




	class TPInfo {
	public:
		int found_x_px = -1;
		int found_y_px = -1;
		size_t found_tp_idx = (size_t) -1; /* Index of trackpoint in "track data" data structure that is the closest to mouse event. */
		bool valid = false;
		Trackpoint * found_tp = NULL;
	};




	class ProfileViewBase {
	public:
		ProfileViewBase(GisViewportDomain x_domain, GisViewportDomain y_domain, TrackProfileDialog * dialog, QWidget * parent = NULL);
		virtual ~ProfileViewBase();

		int get_central_n_columns(void) const;
		int get_central_n_rows(void) const;

		sg_ret draw_track_and_crosshairs(Track * trk);
		virtual sg_ret draw_graph_without_crosshairs(Track * trk) = 0;
		sg_ret draw_crosshairs(const Crosshair2D & selection_ch, const Crosshair2D & hover_ch);

		virtual sg_ret generate_initial_track_data(Track * trk) = 0;

		virtual void configure_controls(void) = 0;


		virtual void save_values(void) {};

		void configure_labels(void);
		void create_widgets_layout(void);


		/* Check whether given combination of x/y domains is supported by ProfileView. */
		static bool domains_are_supported(GisViewportDomain x_domain, GisViewportDomain y_domain);

		void configure_title(void);
		const QString & get_title(void) const;

		void create_graph_2d(void);

		/**
		   Calculate y position for crosshair on y=f(x) graph.
		   The position will be in "beginning of coordinates system is in bottom-left corner".
		   cbl = coordinate-bottom-left.
		*/
		Crosshair2D get_position_of_tp(Track * trk, tp_idx tp_idx);


		/**
		   Find y position of argument that matches x position of argument.
		   In other words: find y = f(x), where f() is current graph.

		   Returned cursor position is in "beginning of
		   coordinate system (position 0,0) is in bottom-left
		   corner".
		   cbl = coordinate-bottom-left.

		   Arguments are x/y coordinates in central area of
		   graph. They are equal to 0/0 in bottom-left corner
		   of central area of graph.
		*/
		virtual sg_ret cbl_find_y_on_graph_line(const int central_cbl_x, int & central_cbl_y) = 0;

		/**
		   Get a crosshair that is "sticky" to drawn track
		   profile line: its 'x' pixel coordinate will match
		   'x' pixel coordinate of mouse event, and its 'y'
		   pixel coordinate will be calculated so that
		   crosshair is positioned on track profile line drawn
		   in track profile view.
		*/
		virtual Crosshair2D get_crosshair_under_cursor(QMouseEvent * ev) = 0;

		virtual TPInfo get_tp_info_under_cursor(QMouseEvent * ev) = 0;

		virtual bool track_data_is_valid(void) const = 0;

		virtual sg_ret on_cursor_move(Track * trk, QMouseEvent * ev) = 0;

		Crosshair2D tpinfo_to_crosshair(const TPInfo & tp_info);

		QPen main_pen;
		QPen gps_speed_pen;
		QPen dem_alt_pen;
		QPen no_alt_info_pen;

		GisViewportDomain x_domain = GisViewportDomain::Max;
		GisViewportDomain y_domain = GisViewportDomain::Max;

		Graph2D * graph_2d = NULL;
		TrackViewLabels labels;

		TrackProfileDialog * dialog = NULL;
		QWidget * widget = NULL;
		QGridLayout * labels_grid = NULL;
		QVBoxLayout * main_vbox = NULL;
		QVBoxLayout * controls_vbox = NULL;

		/*
		  Place where selection crosshair should be drawn in
		  given graph.

		  This class doesn't have ::hover_ch member because
		  the hover crosshair is more volatile and temporary
		  so it shouldn't be semi-permanently stored.
		*/
		Crosshair2D m_selection_ch;

	private:
		virtual sg_ret update_x_labels(const TPInfo & tp_info) = 0;
		virtual sg_ret update_y_labels(const TPInfo & tp_info) = 0;

		QString title;
	};



	template <typename Tx, typename Tx_ll>
	class ProfileView : public ProfileViewBase {
	public:
		ProfileView(GisViewportDomain new_x_domain, GisViewportDomain new_y_domain, TrackProfileDialog * new_dialog, QWidget * parent = NULL)
			: ProfileViewBase(new_x_domain, new_y_domain, new_dialog, parent) {};
		virtual ~ProfileView();

		virtual void draw_additional_indicators(Track * trk) {};
		void configure_controls(void) override {};


		sg_ret draw_graph_without_crosshairs(Track * trk) override;

		TPInfo get_tp_info_under_cursor(QMouseEvent * ev) override;


		bool track_data_is_valid(void) const override;


		sg_ret set_initial_visible_range_x(void);
		sg_ret set_initial_visible_range_y(const TrackDataBase & track_data);


		sg_ret cbl_find_y_on_graph_line(const int central_cbl_x, int & central_cbl_y) override;
		Crosshair2D get_crosshair_under_cursor(QMouseEvent * ev) override;

		sg_ret on_cursor_move(Track * trk, QMouseEvent * ev) override;

		sg_ret generate_initial_track_data(Track * trk) override;


		sg_ret draw_function_values(Track * trk);

		void draw_dem_alt_speed_dist(Track * trk, bool do_dem, bool do_speed);
		void draw_speed_dist(Track * trk);



		void draw_x_grid(void);
		void draw_y_grid(void);


		/* There can be two x-domains: Time or Distance. They
		   are chosen by Tx (type of x-domain) template
		   parameter. */
		Tx x_interval = { 0 };
		Tx x_visible_min = { 0 };
		Tx x_visible_max = { 0 };


		/*
		  Difference between maximal and minimal value of x
		  parameter.
		*/
		Tx x_visible_range_uu;



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



		/*
		  Track data collected from track at the beginning,
		  and then used to create a processed/compressed copy
		  (track_data_to_draw) that will be drawn to graph.

		  This variable is a cache variable, so that we don't
		  have to collect data from track every time user
		  resizes the graph.
		*/
		TrackData<Tx, Tx_ll> initial_track_data;

		/*
		  Data structure with data from initial_track_data,
		  but processed and prepared for painting
		  (e.g. compressed).
		*/
		TrackData<Tx, Tx_ll> track_data_to_draw;

	private:
		/*
		  Use ::initial_track_data to generate a new version
		  of ::track_data_to_draw, e.g. after resizing Profile
		  View window.
		*/
		sg_ret regenerate_track_data_to_draw(Track * trk);

		sg_ret update_x_labels(const TPInfo & tp_info) override;
		sg_ret update_y_labels(const TPInfo & tp_info) override;

		QString get_y_grid_label(double value);
		QString get_x_grid_label(const Tx & value_uu);
	};




	/* ET = elevation as a function of time. */
	class ProfileViewET : public ProfileView<Time, Time_ll> {
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
	class ProfileViewSD : public ProfileView<Distance, Distance_ll> {
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
	class ProfileViewED : public ProfileView<Distance, Distance_ll> {
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
	class ProfileViewGD : public ProfileView<Distance, Distance_ll> {
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
	class ProfileViewST : public ProfileView<Time, Time_ll> {
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
	class ProfileViewDT : public ProfileView<Time, Time_ll> {
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
