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

#include "viewport.h"
#include "layer_trw_track_internal.h"
#include "measurements.h"




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
		SG_TRACK_PROFILE_TYPE_ED, /* ed = elevation as a function of distance. */
		SG_TRACK_PROFILE_TYPE_GD, /* gd = gradient as a function of distance. */
		SG_TRACK_PROFILE_TYPE_ST, /* st = speed as a function of time. */
		SG_TRACK_PROFILE_TYPE_DT, /* dt = distance as a function of time. */
		SG_TRACK_PROFILE_TYPE_ET, /* et = elevation as a function of time. */
		SG_TRACK_PROFILE_TYPE_SD, /* sd = speed as a function of distance. */

		SG_TRACK_PROFILE_TYPE_MAX,
	} TrackProfileType;




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

		QLabel * x_label = NULL;
		QLabel * y_label = NULL;
		QLabel * t_label = NULL;
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
		void handle_mouse_button_release(Viewport * viewport, QMouseEvent * event, ProfileGraph * graph);

		void clear_image(QPixmap * pix);

		void draw_all_graphs(bool resized);

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

		std::vector<ProfileGraph *> graphs;

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

		void handle_cursor_move(ProfileGraph * graph, QMouseEvent * ev);
	};




	class ProfileGraph {
	public:
		ProfileGraph(GeoCanvasDomain x_domain, GeoCanvasDomain y_domain, int index, TrackProfileDialog * dialog);
		virtual ~ProfileGraph();

		virtual void draw_additional_indicators(TrackInfo & track_info) {};
		virtual void configure_controls(TrackProfileDialog * dialog) {};
		virtual void save_values(void) {};

		void configure_labels(TrackProfileDialog * dialog);
		QWidget * create_widgets_layout(TrackProfileDialog * dialog);

		void create_viewport(int index, TrackProfileDialog * dialog);
		QString get_graph_title(void) const;

		double get_pos_y(double pos_x);

		void set_initial_visible_range_x_distance(void);
		void set_initial_visible_range_x_time(void);
		void set_initial_visible_range_y(void);

		int get_cursor_pos_x(QMouseEvent * ev) const;

		QPointF get_position_of_tp(TrackInfo & track_info, Trackpoint * tp);

		bool regenerate_data(Track * trk);
		void regenerate_sizes(void);

		void draw_graph(TrackInfo & track_info);
		void draw_marks(const ScreenPos & selected_pos, const ScreenPos & current_pos, bool & is_selected_drawn, bool & is_current_drawn);

		void draw_function_values(void);

		void draw_dem_alt_speed_dist(Track * trk, double max_speed_in, bool do_dem, bool do_speed);
		void draw_speed_dist(Track * trk, double max_speed_in, bool do_speed);

		void draw_grid_horizontal_line(int pos_y, const QString & label);
		void draw_grid_vertical_line(int pos_x, const QString & label);


		void draw_x_grid(const TrackInfo & track_info);
		void draw_y_grid(void);

		void draw_x_grid_sub_d(void);
		void draw_x_grid_sub_t(void);

		QString get_y_grid_label(float value);

		TrackProfileType type = SG_TRACK_PROFILE_TYPE_MAX;
		Viewport * viewport = NULL;
		PropSaved saved_img;

		int width = 0;
		int height = 0;
		int bottom_edge = 0;
		int left_edge = 0;

		/* For distance-based graphs. */
		double x_interval_d = 0.0;
		double x_min_visible_d = 0.0;
		double x_max_visible_d = 0.0;

		/* For time-based graphs. */
		time_t x_interval_t = 0;
		time_t x_min_visible_t = 0;
		time_t x_max_visible_t = 0;

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
		bool regenerate_data_from_scratch(Track * trk);
	};




	class ProfileGraphET : public ProfileGraph {
	public:
		ProfileGraphET(TrackProfileDialog * dialog);
		~ProfileGraphET() {};

		void draw_additional_indicators(TrackInfo & track_info);
		void configure_controls(TrackProfileDialog * dialog);
		void save_values(void);
	private:
		QCheckBox * show_dem_cb = NULL;
		QCheckBox * show_speed_cb = NULL;
	};


	class ProfileGraphSD : public ProfileGraph {
	public:
		ProfileGraphSD(TrackProfileDialog * dialog);
		~ProfileGraphSD() {};

		void draw_additional_indicators(TrackInfo & track_info);
		void configure_controls(TrackProfileDialog * dialog);
		void save_values(void);
	private:
		QCheckBox * show_gps_speed_cb = NULL;
	};


	class ProfileGraphED : public ProfileGraph {
	public:
		ProfileGraphED(TrackProfileDialog * dialog);
		~ProfileGraphED() {};

		void draw_additional_indicators(TrackInfo & track_info);
		void configure_controls(TrackProfileDialog * dialog);
		void save_values(void);
	private:
		QCheckBox * show_dem_cb = NULL;
		QCheckBox * show_gps_speed_cb = NULL;
	};


	class ProfileGraphGD : public ProfileGraph {
	public:
		ProfileGraphGD(TrackProfileDialog * dialog);
		~ProfileGraphGD() {};

		void draw_additional_indicators(TrackInfo & track_info);
		void configure_controls(TrackProfileDialog * dialog);
		void save_values(void);
	private:
		QCheckBox * show_gps_speed_cb = NULL;
	};


	class ProfileGraphST : public ProfileGraph {
	public:
		ProfileGraphST(TrackProfileDialog * dialog);
		~ProfileGraphST() {};

		void draw_additional_indicators(TrackInfo & track_info);
		void configure_controls(TrackProfileDialog * dialog);
		void save_values(void);
	private:
		QCheckBox * show_gps_speed_cb = NULL;
	};


	class ProfileGraphDT : public ProfileGraph {
	public:
		ProfileGraphDT(TrackProfileDialog * dialog);
		~ProfileGraphDT() {};

		void draw_additional_indicators(TrackInfo & track_info);
		void configure_controls(TrackProfileDialog * dialog);
		void save_values(void);
	private:
		QCheckBox * show_speed_cb = NULL;
	};




	void track_profile_dialog(Window * parent, Track * trk, Viewport * main_viewport);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TRACK_PROFILE_DIALOG_H_ */
