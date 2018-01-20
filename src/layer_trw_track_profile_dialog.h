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

#ifndef _SG_TRACK_PROFILE_DIALOG_H_
#define _SG_TRACK_PROFILE_DIALOG_H_




#include <cstdint>

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

#include "globals.h"
#include "viewport.h"
#include "layer_trw_track_internal.h"




namespace SlavGPS {




	class Window;
	class Viewport;
	class LayerTRW;
	class Track;
	class Trackpoint;
	class ProfileGraph;




	typedef struct _propsaved {
		bool valid;
		QPixmap img;
	} PropSaved;




	typedef enum {
		SG_TRACK_PROFILE_TYPE_ED, /* ed = elevation as a function of -distance. */
		SG_TRACK_PROFILE_TYPE_GD, /* gd = gradient as a function of distance. */
		SG_TRACK_PROFILE_TYPE_ST, /* st = speed as a function of time. */
		SG_TRACK_PROFILE_TYPE_DT, /* dt = distance as a function of time. */
		SG_TRACK_PROFILE_TYPE_ET, /* et = elevation as a function of time. */
		SG_TRACK_PROFILE_TYPE_SD, /* sd = speed as a function of distance. */

		SG_TRACK_PROFILE_TYPE_MAX,
	} TrackProfileType;




	template <class T>
	class Intervals {
	public:
		Intervals(const T * interval_values, int n_interval_values) : values(interval_values), n_values(n_interval_values) {};
		int get_interval_index(T min, T max, int n_intervals);
		T get_interval_value(int index);

		const T * values = NULL;
		int n_values = 0;
	};



	enum class GeoCanvasDomain {
		Time,
		Elevation,
		Distance,
		Speed,
		Gradient,
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

		QString x_label;
		QString y_label;
		QString t_label;
	};




	class GeoCanvasControls {
	public:
		QCheckBox * show_dem = NULL;
		QCheckBox * show_gps_speed = NULL;
		QCheckBox * show_speed = NULL;
	};




	class TrackInfo {
	public:
		Track * trk = NULL;
		double max_speed = 0.0;
		double track_length_including_gaps = 0.0;
		time_t duration = 0;
	};




	class TrackProfileDialog : public QDialog {
		Q_OBJECT
	public:
		TrackProfileDialog() {};
		TrackProfileDialog(QString const & title, Track * a_trk, Viewport * main_viewport_, Window * a_parent = NULL);
		~TrackProfileDialog();

	private slots:
		void checkbutton_toggle_cb(void);
		void dialog_response_cb(int resp);
		void destroy_cb(void);
		bool paint_to_viewport_cb(Viewport * viewport);

		void handle_cursor_move_ed_cb(Viewport * viewport, QMouseEvent * event);
		void handle_cursor_move_gd_cb(Viewport * viewport, QMouseEvent * event);
		void handle_cursor_move_st_cb(Viewport * viewport, QMouseEvent * event);
		void handle_cursor_move_dt_cb(Viewport * viewport, QMouseEvent * event);
		void handle_cursor_move_et_cb(Viewport * viewport, QMouseEvent * event);
		void handle_cursor_move_sd_cb(Viewport * viewport, QMouseEvent * event);

		bool track_ed_release_cb(Viewport * viewport, QMouseEvent * event);
		bool track_gd_release_cb(Viewport * viewport, QMouseEvent * event);
		bool track_st_release_cb(Viewport * viewport, QMouseEvent * event);
		bool track_dt_release_cb(Viewport * viewport, QMouseEvent * event);
		bool track_et_release_cb(Viewport * viewport, QMouseEvent * event);
		bool track_sd_release_cb(Viewport * viewport, QMouseEvent * event);


	public:
		void track_graph_release(Viewport * viewport, QMouseEvent * event, ProfileGraph * graph);

		void clear_image(QPixmap * pix);

		void draw_all_graphs(bool resized);
		void configure_widgets(int index);
		QWidget * create_graph_page(ProfileGraph * graph);

		void save_values(void);

		void draw_single_graph(ProfileGraph * graph);


		Window * parent = NULL;
		LayerTRW * trw = NULL;
		Viewport * main_viewport = NULL;
		TrackInfo track_info;

		QTabWidget * tabs = NULL;


		QDialogButtonBox * button_box = NULL;
		QPushButton * button_cancel = NULL;
		QPushButton * button_split_at_marker = NULL;
		QPushButton * button_split_segments = NULL;
		QPushButton * button_reverse = NULL;
		QPushButton * button_ok = NULL;


		bool  configure_dialog;

		int profile_width;
		int profile_height;
		int profile_width_old;
		int profile_height_old;
		int profile_width_offset;
		int profile_height_offset;

		ProfileGraph * graphs[SG_TRACK_PROFILE_TYPE_MAX] = { NULL };

		Trackpoint * selected_tp = NULL; /* Trackpoint selected by clicking in chart. Will be marked in a viewport by non-moving crosshair. */
		bool  is_selected_drawn = false;
		Trackpoint * current_tp = NULL; /* Trackpoint that is closest to current position of *hovering* cursor. */
		bool  is_current_drawn = false;

		//char     * tz = NULL; /* TimeZone at track's location. */

		/* Pen used to draw main parts of graphs (i.e. the values of functions y = f(x)). */
		QPen main_pen;

		QSignalMapper * signal_mapper = NULL;

	private:
		bool draw_cursor_by_distance(QMouseEvent * ev, ProfileGraph * graph, double & meters_from_start, int & current_pos_x);
		bool draw_cursor_by_time(QMouseEvent * ev, ProfileGraph * graph, time_t & seconds_from_start, int & current_pos_x);
		void draw_marks(ProfileGraph * graph, const ScreenPos & selected_pos, const ScreenPos & current_pos);


		void handle_cursor_move(ProfileGraph * graph, QMouseEvent * ev);
	};




	class ProfileGraph {
	public:
		ProfileGraph(GeoCanvasDomain x_domain, GeoCanvasDomain y_domain, int index, TrackProfileDialog * dialog);
		~ProfileGraph();

		void create_viewport(int index, TrackProfileDialog * dialog);

		double get_pos_y(double pos_x);
		void set_visible_range(int interval_index, const double * interval_values, int n_interval_values, int n_intervals);

		int get_cursor_pos_x(QMouseEvent * ev) const;

		QPointF get_position_of_tp(TrackInfo & track_info, Trackpoint * tp);

		bool regenerate_data(Track * trk);
		void regenerate_sizes(void);

		void draw_graph(TrackInfo & track_info);

		void draw_function_values(void);

		void draw_dem_alt_speed_dist(Track * trk, double max_speed_in, bool do_dem, bool do_speed);
		void draw_speed_dist(Track * trk, double max_speed_in, bool do_speed);

		void draw_grid_horizontal_line(int pos_y, const QString & label);
		void draw_grid_vertical_line(int pos_x, const QString & label);


		void draw_x_grid(const TrackInfo & track_info);
		void draw_y_grid(void);

		void draw_x_grid_time(time_t visible_begin, time_t visible_end);
		void draw_x_grid_distance(double visible_begin, double visible_end);

		void draw_y_grid_sub(void);
#if 0
		void draw_y_grid_elevation(void);
		void draw_y_grid_speed(void);
		void draw_y_grid_gradient(void);
		void draw_y_grid_distance(void);
#endif

		QString get_y_grid_label(float value);

		TrackProfileType type = SG_TRACK_PROFILE_TYPE_MAX;
		Viewport * viewport = NULL;
		PropSaved saved_img;

		void (*draw_additional_indicators_fn)(ProfileGraph *, TrackInfo &) = NULL;

		int width = 0;
		int height = 0;
		int bottom_edge = 0;
		int left_edge = 0;

		/* Number of intervals on y-axis. */
		int n_intervals_y = 0;

		double y_interval = 0.0;

		//double y_range_min = 0.0;
		//double y_range_max = 0.0;

		double y_min_visible = 0.0;
		double y_max_visible = 0.0;

		TrackData track_data;
		bool (*track_data_creator_fn)(ProfileGraph *, Track *) = NULL;

		GeoCanvas geocanvas;

		QPen main_pen;
		QPen gps_speed_pen;
		QPen dem_alt_pen;
		QPen no_alt_info_pen;

		GeoCanvasControls controls;
		GeoCanvasLabels labels;

		QGridLayout * labels_grid = NULL;
		QVBoxLayout * controls_vbox = NULL;
	};





	void track_profile_dialog(Window * parent, Track * trk, Viewport * main_viewport);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TRACK_PROFILE_DIALOG_H_ */
