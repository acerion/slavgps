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
 *
 */
#ifndef _SG_LAYER_TRW_PROPWIN_H_
#define _SG_LAYER_TRW_PROPWIN_H_




#include <cstdint>

#include <QObject>
#include <QDialog>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QCheckBox>
#include <QMouseEvent>

#include <glib.h>

#include "layer_trw.h"
#include "viewport.h"
#include "track.h"
#include "layers_panel.h"
#include "window.h"




#define VIK_TRW_LAYER_PROPWIN_SPLIT 1
#define VIK_TRW_LAYER_PROPWIN_REVERSE 2
#define VIK_TRW_LAYER_PROPWIN_DEL_DUP 3
#define VIK_TRW_LAYER_PROPWIN_SPLIT_MARKER 4




namespace SlavGPS {




	typedef struct _propsaved {
		bool saved;
		GdkImage * img;
	} PropSaved;




	typedef enum {
		SG_TRACK_PROFILE_TYPE_ELEVATION_DISTANCE,
		SG_TRACK_PROFILE_TYPE_GRADIENT_DISTANCE,
		SG_TRACK_PROFILE_TYPE_SPEED_TIME,
		SG_TRACK_PROFILE_TYPE_DISTANCE_TIME,
		SG_TRACK_PROFILE_TYPE_ELEVATION_TIME,
		SG_TRACK_PROFILE_TYPE_SPEED_DISTANCE,
		SG_TRACK_PROFILE_TYPE_END,
	} TrackProfileType;




	class TrackProfileDialog : public QDialog {
		Q_OBJECT
	public:
		TrackProfileDialog() {};
		TrackProfileDialog(QString const & title, LayerTRW * a_layer, Track * a_trk, LayersPanel * a_panel, Viewport * a_viewport, Window * a_parent = NULL);
		~TrackProfileDialog();

	private slots:
		void checkbutton_toggle_cb(void);
		void dialog_response_cb(int resp);
		void destroy_cb(void);


		void track_profile_move_cb(Viewport * viewport, QMouseEvent * event);
		void track_gradient_move_cb(Viewport * viewport, QMouseEvent * event);
		void track_vt_move_cb(Viewport * viewport, QMouseEvent * event);
		void track_dt_move_cb(Viewport * viewport, QMouseEvent * event);
		void track_et_move_cb(Viewport * viewport, QMouseEvent * event);
		void track_sd_move_cb(Viewport * viewport, QMouseEvent * event);

		bool track_profile_release_cb(Viewport * viewport, QMouseEvent * event);
		bool track_gradient_release_cb(Viewport * viewport, QMouseEvent * event);
		bool track_vt_release_cb(Viewport * viewport, QMouseEvent * event);
		bool track_dt_release_cb(Viewport * viewport, QMouseEvent * event);
		bool track_et_release_cb(Viewport * viewport, QMouseEvent * event);
		bool track_sd_release_cb(Viewport * viewport, QMouseEvent * event);


	public:

		Viewport * create_profile(double * min_alt, double * max_alt);
		Viewport * create_gradient(void);
		Viewport * create_vtdiag(void);
		Viewport * create_dtdiag(void);
		Viewport * create_etdiag(void);
		Viewport * create_sddiag(void);

		void save_image_and_draw_graph_marks(Viewport * viewport,
						     QPen & pen,
						     double selected_pos_x,
						     double selected_pos_y,
						     double current_pos_x,
						     double current_pox_y,
						     PropSaved *saved_img,
						     unsigned int graph_width,
						     unsigned int graph_height);

		void track_graph_release(Viewport * viewport, QMouseEvent * event, TrackProfileType graph_type);

		/* "Draw" functions. */
		void draw_elevations(Viewport * viewport, Track * trk);
		void draw_gradients(Viewport * viewport, Track * trk);
		void draw_vt(Viewport * viewport, Track * trk);
		void draw_dt(Viewport * viewport, Track * trk);
		void draw_et(Viewport * viewport, Track * trk);
		void draw_sd(Viewport * viewport, Track * trk);

		/* "get_pos_y" functions. */
		double get_pos_y_speed(double pos_x, int width, int height);
		double get_pos_y_speed_dist(double pos_x, int width, int height);
		double get_pos_y_altitude(double pos_x, int width, int height);
		double get_pos_y_altitude_time(double pos_x, int width, int height);
		double get_pos_y_gradient(double pos_x, int width, int height);
		double get_pos_y_distance(double pos_x, int width, int height);

		void clear_image(QPixmap * pix);

		void draw_all_graphs(bool resized);
		QWidget * create_graph_page(QWidget * graph,
					    const char * text1,
					    QLabel * value1,
					    const char * text2,
					    QLabel * value2,
					    const char * text3,
					    QLabel * value3,
					    QCheckBox * checkbutton1,
					    bool checkbutton1_default,
					    QCheckBox * checkbutton2,
					    bool checkbutton2_default);

		void draw_vertical_grid_distance(Viewport * viewport, unsigned int ii, double dd, unsigned int xx, DistanceUnit distance_unit);
		void draw_vertical_grid_time(Viewport * viewport, unsigned int ii, unsigned int tt, unsigned int xx);
		void draw_horizontal_grid(Viewport * viewport, QPen & fg_pen, QPen & dark_pen, char * ss, int i);
		void draw_time_lines(Viewport * viewport);
		void draw_distance_divisions(Viewport * viewport, DistanceUnit distance_unit);

		void save_values(void);

		void draw_single_graph(Viewport * viewport, bool resized, void (TrackProfileDialog::*draw_graph)(Viewport *, Track *), double (TrackProfileDialog::*get_pos_y)(double, int, int), bool by_time, PropSaved * saved_img);


		Window * parent = NULL;
		LayerTRW * trw = NULL;
		Track * trk = NULL;
		LayersPanel * panel = NULL;
		Viewport * main_viewport = NULL;

		QTabWidget * tabs = NULL;


		bool  configure_dialog;

		int      profile_width;
		int      profile_height;
		int      profile_width_old;
		int      profile_height_old;
		int      profile_width_offset;
		int      profile_height_offset;

		QLabel * w_cur_dist = NULL; /*< Current distance. */
		QLabel * w_cur_elevation = NULL;
		QLabel * w_cur_gradient_dist = NULL; /*< Current distance on gradient graph. */
		QLabel * w_cur_gradient_gradient = NULL; /*< Current gradient on gradient graph. */
		QLabel * w_cur_time = NULL; /*< Current track time. */
		QLabel * w_cur_time_real = NULL; /*< Actual time as on a clock. */
		QLabel * w_cur_speed = NULL;
		QLabel * w_cur_dist_dist = NULL; /*< Current distance on distance graph. */
		QLabel * w_cur_dist_time = NULL; /*< Current track time on distance graph. */
		QLabel * w_cur_dist_time_real = NULL; /* Clock time. */
		QLabel * w_cur_elev_elev = NULL;
		QLabel * w_cur_elev_time = NULL; /* Track time. */
		QLabel * w_cur_elev_time_real = NULL; /* Clock time. */
		QLabel * w_cur_speed_dist = NULL;
		QLabel * w_cur_speed_speed = NULL;
		QCheckBox * w_show_dem = NULL;
		QCheckBox * w_show_alt_gps_speed = NULL;
		QCheckBox * w_show_gps_speed = NULL;
		QCheckBox * w_show_gradient_gps_speed = NULL;
		QCheckBox * w_show_dist_speed = NULL;
		QCheckBox * w_show_elev_speed = NULL;
		QCheckBox * w_show_elev_dem = NULL;
		QCheckBox * w_show_sd_gps_speed = NULL;
		double   track_length;
		double   track_length_inc_gaps;
		PropSaved elev_graph_saved_img;
		PropSaved gradient_graph_saved_img;
		PropSaved speed_graph_saved_img;
		PropSaved dist_graph_saved_img;
		PropSaved elev_time_graph_saved_img;
		PropSaved speed_dist_graph_saved_img;
		Viewport * elev_viewport = NULL;
		Viewport * gradient_viewport = NULL;
		Viewport * speed_viewport = NULL;
		Viewport * dist_viewport = NULL;
		Viewport * elev_time_viewport = NULL;
		Viewport * speed_dist_viewport = NULL;
		double   * altitudes = NULL;
		double   * ats = NULL; /* Altitudes in time. */
		double   min_altitude = 0.0;
		double   max_altitude = 0.0;
		double   draw_min_altitude = 0.0;
		double   draw_min_altitude_time = 0.0;
		int      cia = 0; /* Chunk size Index into Altitudes. */
		int      ciat = 0; /* Chunk size Index into Altitudes / Time. */
		/* NB cia & ciat are normally same value but sometimes not due to differing methods of altitude array creation.
		   thus also have draw_min_altitude for each altitude graph type. */
		double   *gradients = NULL;
		double   min_gradient = 0.0;
		double   max_gradient = 0.0;
		double   draw_min_gradient = 0.0;
		int      cig; /* Chunk size Index into Gradients. */
		double   * speeds = NULL;
		double   * speeds_dist = NULL;
		double   min_speed = 0.0;
		double   max_speed = 0.0;
		double   draw_min_speed = 0.0;
		double   max_speed_dist = 0.0;
		int      cis = 0; /* Chunk size Index into Speeds. */
		int      cisd = 0; /* Chunk size Index into Speed/Distance. */
		double   * distances = NULL;
		int      cid = 0; /* Chunk size Index into Distance. */

		Trackpoint * selected_tp = NULL; /* Trackpoint selected by clicking in chart. Will be marked in a viewport by non-moving crosshair. */
		bool  is_selected_drawn = false;
		Trackpoint * current_tp = NULL; /* Trackpoint that is closest to current position of cursor. */
		bool  is_current_drawn = false;

		time_t    duration = 0;
		//char     * tz = NULL; /* TimeZone at track's location. */
	};




	void vik_trw_layer_propwin_run(Window * parent,
				       LayerTRW * layer,
				       Track * trk,
				       LayersPanel * panel,
				       Viewport * viewport,
				       bool start_on_stats);

	/**
	 * Update this property dialog e.g. if the track has been renamed
	 */
	void vik_trw_layer_propwin_update(Track * trk);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_TRW_PROPWIN_H_ */
