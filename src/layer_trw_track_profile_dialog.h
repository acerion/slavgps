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
		SG_TRACK_PROFILE_TYPE_ED, /* ed = elevation-distance. */
		SG_TRACK_PROFILE_TYPE_GD, /* gd = gradient-distance. */
		SG_TRACK_PROFILE_TYPE_ST, /* st = speed-time. */
		SG_TRACK_PROFILE_TYPE_DT, /* dt = distance-time. */
		SG_TRACK_PROFILE_TYPE_ET, /* et = elevation-time. */
		SG_TRACK_PROFILE_TYPE_SD, /* sd = speed-distance. */
		SG_TRACK_PROFILE_TYPE_END,
	} TrackProfileType;







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


		void track_ed_move_cb(Viewport * viewport, QMouseEvent * event);
		void track_gd_move_cb(Viewport * viewport, QMouseEvent * event);
		void track_st_move_cb(Viewport * viewport, QMouseEvent * event);
		void track_dt_move_cb(Viewport * viewport, QMouseEvent * event);
		void track_et_move_cb(Viewport * viewport, QMouseEvent * event);
		void track_sd_move_cb(Viewport * viewport, QMouseEvent * event);

		bool track_ed_release_cb(Viewport * viewport, QMouseEvent * event);
		bool track_gd_release_cb(Viewport * viewport, QMouseEvent * event);
		bool track_st_release_cb(Viewport * viewport, QMouseEvent * event);
		bool track_dt_release_cb(Viewport * viewport, QMouseEvent * event);
		bool track_et_release_cb(Viewport * viewport, QMouseEvent * event);
		bool track_sd_release_cb(Viewport * viewport, QMouseEvent * event);


	public:

		Viewport * create_viewport(const char * debug_label);

		void track_graph_release(Viewport * viewport, QMouseEvent * event, ProfileGraph * graph);



		/* "Draw" functions. */
		void draw_ed(ProfileGraph * graph, Track * trk);
		void draw_gd(ProfileGraph * graph, Track * trk);
		void draw_st(ProfileGraph * graph, Track * trk);
		void draw_dt(ProfileGraph * graph, Track * trk);
		void draw_et(ProfileGraph * graph, Track * trk);
		void draw_sd(ProfileGraph * graph, Track * trk);



		void clear_image(QPixmap * pix);

		void draw_all_graphs(bool resized);
		QWidget * create_graph_page(Viewport * viewport,
					    const QString & text1,
					    QLabel * value1,
					    const QString & text2,
					    QLabel * value2,
					    const QString & text3,
					    QLabel * value3,
					    QCheckBox * checkbutton1,
					    bool checkbutton1_default,
					    QCheckBox * checkbutton2,
					    bool checkbutton2_default);


		void draw_grid_horizontal_line(ProfileGraph * graph, const QString & label, int pos_y);
		void draw_grid_vertical_line(ProfileGraph * graph, const QString & label, int pos_x);

		void draw_time_grid(ProfileGraph * graph, int n_intervals);
		void draw_distance_grid(ProfileGraph * graph, DistanceUnit distance_unit, int n_intervals);

		void draw_st_grid(ProfileGraph * graph, SpeedUnit speed_unit);
		void draw_dt_grid(ProfileGraph * graph, DistanceUnit distance_unit);
		void draw_et_grid(ProfileGraph * graph, HeightUnit height_unit);
		void draw_sd_grid(ProfileGraph * graph, SpeedUnit speed_unit);
		void draw_ed_grid(ProfileGraph * graph, HeightUnit height_unit);
		void draw_gd_grid(ProfileGraph * graph);

		void save_values(void);

		void draw_single_graph(ProfileGraph * graph, const double * interval_values);


		Window * parent = NULL;
		LayerTRW * trw = NULL;
		Track * trk = NULL;
		Viewport * main_viewport = NULL;

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

		QLabel * w_ed_current_distance = NULL; /*< Current distance. */
		QLabel * w_ed_current_elevation = NULL;
		QLabel * w_gd_current_distance = NULL; /*< Current distance on gradient graph. */
		QLabel * w_gd_current_gradient = NULL; /*< Current gradient on gradient graph. */
		QLabel * w_st_current_time = NULL; /*< Current track time. */
		QLabel * w_st_current_time_real = NULL; /*< Actual time as on a clock. */
		QLabel * w_st_current_speed = NULL;
		QLabel * w_dt_curent_distance = NULL; /*< Current distance on distance graph. */
		QLabel * w_dt_current_time = NULL; /*< Current track time on distance graph. */
		QLabel * w_dt_current_time_real = NULL; /* Clock time. */
		QLabel * w_et_current_elevation = NULL;
		QLabel * w_et_current_time = NULL; /* Track time. */
		QLabel * w_et_current_time_real = NULL; /* Clock time. */
		QLabel * w_sd_current_distance = NULL;
		QLabel * w_sd_current_speed = NULL;

		QCheckBox * w_ed_show_dem = NULL;
		QCheckBox * w_ed_show_gps_speed = NULL;
		QCheckBox * w_gd_show_gps_speed = NULL;
		QCheckBox * w_st_show_gps_speed = NULL;
		QCheckBox * w_dt_show_speed = NULL;
		QCheckBox * w_et_show_speed = NULL;
		QCheckBox * w_et_show_dem = NULL;
		QCheckBox * w_sd_show_gps_speed = NULL;

		double   track_length;
		double   track_length_inc_gaps;


		ProfileGraph * graph_ed = NULL;
		ProfileGraph * graph_gd = NULL;
		ProfileGraph * graph_st = NULL;
		ProfileGraph * graph_dt = NULL;
		ProfileGraph * graph_et = NULL;
		ProfileGraph * graph_sd = NULL;


		double   max_speed = 0.0;



		Trackpoint * selected_tp = NULL; /* Trackpoint selected by clicking in chart. Will be marked in a viewport by non-moving crosshair. */
		bool  is_selected_drawn = false;
		Trackpoint * current_tp = NULL; /* Trackpoint that is closest to current position of cursor. */
		bool  is_current_drawn = false;

		time_t    duration = 0;
		//char     * tz = NULL; /* TimeZone at track's location. */

		/* Pen used to draw main parts of graphs (i.e. the values of functions y = f(x)). */
		QPen main_pen;

		/* Properties of text labels drawn on margins of charts (next to each horizontal/vertical grid line). */
		QPen labels_pen;
		QFont labels_font;

		QSignalMapper * signal_mapper = NULL;

	private:
		void draw_marks(ProfileGraph * graph, const ScreenPos & selected_pos, const ScreenPos & current_pos);
	};




	class ProfileGraph {
	public:
		ProfileGraph(bool time_graph, void (TrackProfileDialog::*draw_graph)(ProfileGraph *, Track *), void (*representation_creator)(TrackData &, Track *, int));
		~ProfileGraph();

		double get_pos_y(double pos_x, const double * interval_values);
		void set_y_range_min_drawable(int interval_index, const double * interval_values, int n_interval_values, int n_intervals);

		bool regenerate_y_values(Track * trk);
		void regenerate_sizes(void);

		TrackProfileType type = SG_TRACK_PROFILE_TYPE_END;
		Viewport * viewport = NULL;
		PropSaved saved_img;
		bool is_time_graph = false;

		void (TrackProfileDialog::*draw_graph_fn)(ProfileGraph *, Track *) = NULL;

		int width = 0;
		int height = 0;
		int bottom_edge = 0;
		int left_edge = 0;

		/* Number of intervals on y-axis. */
		int n_intervals_y = 0;

		double y_interval = 0.0;

		double y_range_min = 0.0;
		double y_range_max = 0.0;
		double y_range_min_drawable = 0.0;

		TrackData rep;
		void (*representation_creator_fn)(TrackData &, Track *, int);
	};





	void track_profile_dialog(Window * parent, Track * trk, Viewport * main_viewport);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TRACK_PROFILE_DIALOG_H_ */
