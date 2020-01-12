/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2016-2020, Kamil Ignacak <acerion@wp.pl>
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
#include <cassert>




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
#include "graph_intervals.h"
#include "preferences.h"
#include "dem_cache.h"




namespace SlavGPS {




#define GRAPH_X_INTERVALS 5
#define GRAPH_Y_INTERVALS 5




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




	class Graph2D : public ViewportPixmap {
		Q_OBJECT
	public:
		Graph2D(int left = 0, int right = 0, int top = 0, int bottom = 0, QWidget * parent = nullptr)
			: ViewportPixmap(left, right, top, bottom, parent) {}

		void mousePressEvent(QMouseEvent * event); /* Double click is handled through event filter. */
		void mouseMoveEvent(QMouseEvent * event);
		void mouseReleaseEvent(QMouseEvent * event);


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
		TrackProfileDialog(Track * trk, GisViewport * main_gisview, QWidget * parent = NULL);
		~TrackProfileDialog();

		void clear_image(QPixmap * pix);

		void draw_all_views(bool resized);

		void save_settings(void);

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
		sg_ret set_center_at_selected_tp(const ProfileViewBase * view, QMouseEvent * ev);


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

		sg_ret draw_track_and_crosshairs(Track & trk);
		virtual sg_ret draw_graph_without_crosshairs(Track & trk) = 0;
		sg_ret draw_crosshairs(const Crosshair2D & selection_ch, const Crosshair2D & hover_ch);

		virtual sg_ret generate_initial_track_data_wrapper(Track * trk) = 0;

		virtual void configure_controls(void) {};


		virtual void save_settings(void) {};

		void configure_labels_x_distance(void);
		void configure_labels_x_time(void);
		void configure_labels_y_altitude(void);
		void configure_labels_y_gradient(void);
		void configure_labels_y_speed(void);
		void configure_labels_y_distance(void);
		void configure_labels_post(void);

		void create_widgets_layout(void);


		const QString & get_title(void) const;

		void create_graph_2d(void);

		/**
		   Get a crosshair that is "sticky" to drawn track
		   profile line: its 'x_px' pixel coordinate will
		   match 'x_px' pixel coordinate of mouse event, and
		   its 'y_px' pixel coordinate will be calculated so
		   that crosshair is positioned on track profile line
		   drawn in track profile view.
		*/
		virtual Crosshair2D get_crosshair_under_cursor(QMouseEvent * ev) const = 0;

		virtual TPInfo get_tp_info_under_cursor(QMouseEvent * ev) const = 0;

		virtual bool track_data_is_valid(void) const = 0;

		virtual sg_ret on_cursor_move(Track * trk, QMouseEvent * ev) = 0;

		virtual sg_ret draw_additional_indicators(Track & trk) = 0;

		Crosshair2D tpinfo_to_crosshair(const TPInfo & tp_info) const;

		QPen main_values_invalid_pen;
		QPen main_values_valid_pen;
		QPen aux_values_1_pen;
		QPen aux_values_2_pen;

		GisViewportDomain x_domain = GisViewportDomain::MaxDomain;
		GisViewportDomain y_domain = GisViewportDomain::MaxDomain;

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

	protected:
		QCheckBox * show_gps_speed_cb = NULL;
		QCheckBox * show_dem_cb = NULL;

		QString title;

	private:
		virtual sg_ret update_x_labels(const TPInfo & tp_info) = 0;
		virtual sg_ret update_y_labels(const TPInfo & tp_info) = 0;
	};




	template <typename Tx, typename Ty>
	class ProfileView : public ProfileViewBase {
	public:
		ProfileView(GisViewportDomain new_x_domain, GisViewportDomain new_y_domain, TrackProfileDialog * new_dialog, QWidget * parent = NULL)
			: ProfileViewBase(new_x_domain, new_y_domain, new_dialog, parent) {};
		virtual ~ProfileView();

		void configure_controls(void) override {};


		sg_ret draw_graph_without_crosshairs(Track & trk) override;

		TPInfo get_tp_info_under_cursor(QMouseEvent * ev) const override;


		bool track_data_is_valid(void) const override;


		sg_ret set_initial_visible_range_x(void);
		sg_ret set_initial_visible_range_y(void);
		sg_ret set_grid_intervals(void);


		Crosshair2D get_crosshair_under_cursor(QMouseEvent * ev) const override;

		sg_ret on_cursor_move(Track * trk, QMouseEvent * ev) override;

		sg_ret generate_initial_track_data_wrapper(Track * trk) override;


		sg_ret draw_function_values(Track & trk);

		sg_ret draw_additional_indicators(__attribute__((unused)) Track & trk) override { return sg_ret::ok; };

		sg_ret draw_gps_speeds_relative(Track & trk);

		sg_ret draw_gps_speeds(void) { return sg_ret::ok; };

		/**
		   Draws DEM points if DEM layer is present.

		   A specialization that really does something should
		   be provided only for those views that have
		   'Altitude' as Ty.
		*/
		sg_ret draw_dem_elevation(void) { return sg_ret::ok; };

		/**
		   @brief Get a specific value of trackpoint from auxiliary source.

		   For speed, the auxiliary source is "GPS speed"
		   field of trackpoint.

		   For altitude, the auxiliary source is DEM cache (if
		   DEM layer is loaded for area where a track is
		   present).
		*/
		Ty get_tp_aux_value_uu(const Trackpoint & tp) { return Ty(); }

		void draw_x_grid(void);
		void draw_y_grid(void);

		void get_pixels_per_unit(double & x_pixels_per_unit, double & y_pixels_per_unit) const;



		/* There can be two x-domains: Time or Distance. They
		   are chosen by Tx (type of x-domain) template
		   parameter. */
		Tx x_interval;
		Tx x_visible_min;
		Tx x_visible_max;


		/*
		  Difference between maximal and minimal value of x
		  parameter.
		*/
		Tx x_visible_range_uu;


		GraphIntervals<Tx> x_intervals_calculator;
		GraphIntervals<Ty> y_intervals_calculator;


		Ty y_interval;
		/*
		  For some graphs these values will be "padded" with
		  some margin so that there is some space between
		  top/bottom border of graph area and graph lines.

		  That way the graph won't touch top/bottom border
		  lines.
		*/
		Ty y_visible_min;
		Ty y_visible_max;
		/*
		  Difference between max and min (with margins
		  mentioned above). Calculated once, used many times.
		*/
		Ty y_visible_range_uu;



		/*
		  Track data collected from track at the beginning,
		  and then used to create a processed/compressed copy
		  (track_data_to_draw) that will be drawn to graph.

		  This variable is a cache variable, so that we don't
		  have to collect data from track every time user
		  resizes the graph.
		*/
		TrackData<Tx, Ty> initial_track_data;

		/*
		  Data structure with data from initial_track_data,
		  but processed and prepared for painting
		  (e.g. compressed).
		*/
		TrackData<Tx, Ty> track_data_to_draw;

	private:
		/*
		  Use ::initial_track_data to generate a new version
		  of ::track_data_to_draw, e.g. after resizing Profile
		  View window.
		*/
		sg_ret regenerate_track_data_to_draw(Track & trk);

		sg_ret update_x_labels(const TPInfo & tp_info) override;
		sg_ret update_y_labels(const TPInfo & tp_info) override;

		/* Draw additional markers for either Speed or
		   Altitude (in real values, not in
		   relative/percentage values) on views that have
		   Speed or Altitude as Ty. */
		sg_ret draw_parameter_from_auxiliary_source(void);
	};




	template <typename Tx, typename Ty>
	sg_ret ProfileView<Tx, Ty>::regenerate_track_data_to_draw(__attribute__((unused)) Track & trk)
	{
		this->track_data_to_draw.clear();



		const bool precise_drawing = true;
		if (precise_drawing) {
			this->track_data_to_draw = this->initial_track_data;
		} else {
			/*
			  ::initial_track_data has been generated
			  once, when the dialog has been opened. Now
			  compress it to limit number of drawn points.
			*/
			const int compressed_n_points = this->get_central_n_columns();
			this->initial_track_data.compress_into(track_data_to_draw, compressed_n_points);
		}
		if (!this->track_data_to_draw.is_valid()) {
			qDebug() << "EE   ProfileView" << __func__ << __LINE__ << "Failed to regenerate valid compressed track data for" << this->get_title();
			return sg_ret::err;
		}
		qDebug() << "II   ProfileView" << __func__ << __LINE__ << "Regenerated valid compressed track data for" << this->get_title();
		qDebug() << "II   ProfileView" << __func__ << __LINE__ << "Initial track data" << this->get_title() << this->initial_track_data;
		qDebug() << "II   ProfileView" << __func__ << __LINE__ << "Track data to draw" << this->get_title() << this->track_data_to_draw;



		if (sg_ret::ok != this->set_initial_visible_range_x()) {
			qDebug() << "EE   ProfileView" << __func__ << __LINE__ << "Failed to set initial visible x range";
			return sg_ret::err;
		}
		if (sg_ret::ok != this->set_initial_visible_range_y()) {
			qDebug() << "EE   ProfileView" << __func__ << __LINE__ << "Failed to set initial visible y range";
			return sg_ret::err;
		}
		if (sg_ret::ok != this->set_grid_intervals()) {
			qDebug() << "EE   ProfileView" << __func__ << __LINE__ << "Failed to set grid intervals";
			return sg_ret::err;
		}



		if (!this->track_data_to_draw.is_valid()) {
			qDebug() << "EE   ProfileView" << __func__ << __LINE__ << "Final test of track data: failure";
			return sg_ret::err;
		} else {
			qDebug() << "II   ProfileView" << __func__ << __LINE__ << "Final test of track data: success";
			return sg_ret::ok;
		}
	}




	template <typename Tx, typename Ty>
	sg_ret ProfileView<Tx, Ty>::set_initial_visible_range_x(void)
	{
		/* We won't display any x values outside of
		   track_data.x_min/max. We will never be able to zoom out to
		   show e.g. negative values of 'x'. */
		this->x_visible_min = this->track_data_to_draw.x_min();
		this->x_visible_max = this->track_data_to_draw.x_max();


		this->x_visible_range_uu = this->x_visible_max - this->x_visible_min;
		if (this->x_visible_range_uu.is_zero()) {
			qDebug() << "EE   ProfileView" << __func__ << __LINE__ << "Zero span of 'x': min/max x = " << this->x_visible_min << this->x_visible_max << this->get_title();
			return sg_ret::err;
		}

		return sg_ret::ok;
	}




	template <typename Tx, typename Ty>
	sg_ret ProfileView<Tx, Ty>::set_initial_visible_range_y(void)
	{
		/* When user will be zooming in and out, and (in particular)
		   moving graph up and down, the y_min/max_visible values will
		   be non-rounded (i.e. random).  Make them non-rounded from
		   the start, and be prepared to handle non-rounded
		   y_min/max_visible from the start. */
		const double y_margin = 0.05; /* There is no deep reasoning behind this particular value. */

		/* This is not exactly the same range as this->y_visible_range_uu
		   calculated below. TODO: ensure that y_min and y_max are valid. */
		const auto y_data_range = this->track_data_to_draw.y_max() - this->track_data_to_draw.y_min(); /* TODO_LATER: replace 'auto' with proper type. */

		switch (this->y_domain) {

		case GisViewportDomain::DistanceDomain:
			/* Distance will be always non-negative, and it will
			   be zero only at one point in the beginning. So it's
			   ok to configure this value as below. */
			this->y_visible_min = this->track_data_to_draw.y_min();
			break;

		case GisViewportDomain::SpeedDomain: /* Speed will never be negative, but since it can drop
							to zero many many times, visually it will be better
							if there will be some margin between bottom of
							graph area and zero values of speed. */
		case GisViewportDomain::ElevationDomain:
		case GisViewportDomain::GradientDomain:
			this->y_visible_min = this->track_data_to_draw.y_min() - y_data_range * y_margin;
			break;
		default:
			qDebug() << "EE   ProfileView" << __func__ << __LINE__ << "Unhandled y domain" << (int) this->y_domain;
			return sg_ret::err;
		}
		this->y_visible_max = this->track_data_to_draw.y_max() + y_data_range * y_margin;
		this->y_visible_range_uu = this->y_visible_max - this->y_visible_min;

		qDebug() << "II   ProfileView" << __func__ << __LINE__ << this->get_title()
			 << "y min =" << this->track_data_to_draw.y_min()
			 << "y max =" << this->track_data_to_draw.y_max()
			 << "y data range =" << y_data_range
			 << "y data range * y_margin =" << (y_data_range * y_margin)
			 << "y visible min =" << this->y_visible_min
			 << "y visible max =" << this->y_visible_max
			 << "y visible range uu =" << this->y_visible_range_uu;

		return sg_ret::ok;
	}




	/**
	   @reviewed-on: 2019-08-24
	*/
	template <typename Tx, typename Ty>
	sg_ret ProfileView<Tx, Ty>::set_grid_intervals(void)
	{
		/* Find a suitable interval index and value for graph grid
		   lines (x grid and y grid) that will nicely cover visible
		   range of data. */

		{
			const int x_n_intervals = GRAPH_X_INTERVALS;
			this->x_interval = this->x_intervals_calculator.get_interval(this->x_visible_min, this->x_visible_max, x_n_intervals);
		}


		{
			const int y_n_intervals = GRAPH_Y_INTERVALS;
			this->y_interval = this->y_intervals_calculator.get_interval(this->y_visible_min, this->y_visible_max, y_n_intervals);
		}

		return sg_ret::ok;
	}




template <typename Tx, typename Ty>
TPInfo ProfileView<Tx, Ty>::get_tp_info_under_cursor(QMouseEvent * ev) const
{
	TPInfo result;

	const size_t n_values = this->track_data_to_draw.size();
	if (0 == n_values) {
		qDebug() << "NN   ProfileView" << __func__ << __LINE__ << "There were zero values in" << graph_2d->debug;
		return result;
	}

	const int leftmost_px = this->graph_2d->central_get_leftmost_pixel();
	const int bottommost_px = this->graph_2d->central_get_bottommost_pixel();

	const int event_x = ev->x();

	/*
	  We will be minimizing this value and stop when x_px_diff is
	  the smallest.
	  It's integer, because event_x is integer. Making it double
	  wouldn't give us anything.
	*/
	int x_px_diff = this->graph_2d->central_get_n_columns();;

	double x_pixels_per_unit;
	double y_pixels_per_unit;
	this->get_pixels_per_unit(x_pixels_per_unit, y_pixels_per_unit);

	for (size_t i = 0; i < n_values; i++) {

		const double y_current_value_uu = this->track_data_to_draw.y_ll(i);
		const bool y_value_valid = !std::isnan(y_current_value_uu);
		if (!y_value_valid) {
			continue;
		}

		const typename Tx::LL x_current_value_uu = this->track_data_to_draw.x_ll(i);
		const int x_px = leftmost_px + x_pixels_per_unit * (x_current_value_uu - this->x_visible_min.ll_value());

		/* See if x coordinate of this trackpoint on a pixmap
		   is closer to cursor than the previous x
		   coordinate. */
		int x_px_diff_current = std::abs(x_px - event_x);
		if (x_px_diff_current < x_px_diff) {
			/* Found a trackpoint painted at position 'x' that is closer to cursor event position on x axis. */
			x_px_diff = x_px_diff_current;

			const int y_px = bottommost_px - (y_current_value_uu - this->y_visible_min.ll_value()) * y_pixels_per_unit;

			result.found_x_px = x_px;
			result.found_y_px = y_px;
			result.found_tp_idx = i;
			result.found_tp = this->track_data_to_draw.tp(i);
			result.valid = true;

			// qDebug() << "II   ProfileView" << __func__ << __LINE__ << "Found new position closer to cursor event at index" << found_tp_idx << ":" << found_x_px << found_y_px;
		}
	}

	return result;
}




/*
  If x values in track data were always separated by the same amount,
  this function could have been a simple array indexing code based on
  'x' coordinate of mouse event.

  But since sometimes there can be gaps in graphs where x-domain is
  Time, and especially because distance values (in graphs where
  x-domain is Distance) can greatly vary, things are more
  complicated.

  The fact that there can be fewer values in track data than there are
  columns in pixmap (i.e. distance between each drawn trackpoints can
  be two or more pixels) also doesn't help.

  We have to use more sophisticated algorithm for
  conversion of 'x' coordinate of mouse event to position of crosshair.

  We have to try to find a trackpoint in track data arrays that is
  drawn the closest to 'x' coordinate of mouse event.
*/
template <typename Tx, typename Ty>
Crosshair2D ProfileView<Tx, Ty>::get_crosshair_under_cursor(QMouseEvent * ev) const
{
	Crosshair2D crosshair;

	const TPInfo tp_info = this->get_tp_info_under_cursor(ev);
	if (!tp_info.valid) {
		qDebug() << "NN   ProfileView" << __func__ << __LINE__ << "Could not find valid tp info in" << this->get_title();
		return crosshair;
	}

	crosshair = this->tpinfo_to_crosshair(tp_info);

	return crosshair;
}




template <typename Tx, typename Ty>
sg_ret ProfileView<Tx, Ty>::on_cursor_move(__attribute__((unused)) Track * trk, QMouseEvent * ev)
{
	const TPInfo tp_info = this->get_tp_info_under_cursor(ev);
	if (!tp_info.valid || NULL == tp_info.found_tp) {
		qDebug() << "NN   ProfileView" << __func__ << __LINE__ << "Could not find valid tp info for" << this->get_title();
		return sg_ret::err;
	}

	Crosshair2D hover_ch = this->tpinfo_to_crosshair(tp_info);
	hover_ch.debug = "hover crosshair for " + this->get_title();
	if (!hover_ch.valid) {
		qDebug() << "EE   ProfileView" << __func__ << __LINE__ << "Failed to get hover crosshair for" << this->get_title();
		return sg_ret::err;
	}

	this->draw_crosshairs(this->m_selection_ch, hover_ch);

	if (sg_ret::ok != this->update_x_labels(tp_info)) {
		qDebug() << "EE   ProfileView" << __func__ << __LINE__ << "Failed to update x labels in view" << this->get_title();
		return sg_ret::err;
	}

	if (sg_ret::ok != this->update_y_labels(tp_info)) {
		qDebug() << "EE   ProfileView" << __func__ << __LINE__ << "Failed to update y labels in view" << this->get_title();
		return sg_ret::err;
	}

	return sg_ret::ok;
}




template <typename Tx, typename Ty>
sg_ret ProfileView<Tx, Ty>::update_x_labels(const TPInfo & tp_info)
{
	/* This is a private method, so we assume that tp_info, and in
	   particular tp_info.found_tp are valid. */
	assert(tp_info.valid);
	assert(NULL != tp_info.found_tp);

	/* Relative time from start of track. */
	if (this->labels.x_value) {
		/* Values in x[] are already re-calculated to user units. */
		const typename Tx::LL x_ll_uu = this->track_data_to_draw.x_ll(tp_info.found_tp_idx);
		const Tx x_uu = Tx(x_ll_uu, Tx::Unit::user_unit());

		/* TODO: we should use x-value of first valid tp. Make
		   sure that x_min is an x-value of first valid tp. */
		const Tx x_uu_from_start = x_uu - this->x_visible_min;

		this->labels.x_value->setText(x_uu_from_start.to_string());
	}

	/* Absolute time stamp. */
	this->labels.tp_timestamp_value->setText(tp_info.found_tp->timestamp.to_timestamp_string(Qt::LocalTime));

	return sg_ret::ok;
}




template <typename Tx, typename Ty>
sg_ret ProfileView<Tx, Ty>::update_y_labels(const TPInfo & tp_info)
{
	/* This is a private method, so we assume that tp_info, and in
	   particular tp_info.found_tp are valid. */
	assert(tp_info.valid);
	assert(NULL != tp_info.found_tp);

	if (this->labels.y_value) {
		/* Values in y[] are already re-calculated to user units. */
		const typename Ty::LL y_uu = this->track_data_to_draw.y_ll(tp_info.found_tp_idx);
		this->labels.y_value->setText(Ty::ll_value_to_string(y_uu, Ty::Unit::user_unit()));
	}

	return sg_ret::ok;
}




template <typename Tx, typename Ty>
sg_ret ProfileView<Tx, Ty>::draw_parameter_from_auxiliary_source(void)
{
	const int leftmost_px = this->graph_2d->central_get_leftmost_pixel();
	const int bottommost_px = this->graph_2d->central_get_bottommost_pixel();
	const size_t n_values = this->track_data_to_draw.size();

	const QColor & color = this->aux_values_2_pen.color();

	double x_pixels_per_unit;
	double y_pixels_per_unit;
	this->get_pixels_per_unit(x_pixels_per_unit, y_pixels_per_unit);

	const int marker_size = 4; /* Size of rectangle to draw to mark our parameter. */
	const int marker_shift = 2; /* When drawing the rectangle, we have to offset its origin from trackpoint's position on pixmap. */

	for (size_t i = 0; i < n_values; i++) {

		const Trackpoint * tp = this->track_data_to_draw.tp(i);
		if (nullptr == tp) {
			qDebug() << "EE   ProfileView" << __func__ << __LINE__ << "Can't get trackpoint";
			continue;
		}

		const typename Tx::LL x_value_uu = this->track_data_to_draw.x_ll(i);

		Ty y_value_uu = this->get_tp_aux_value_uu(*tp);
		if (!y_value_uu.is_valid()) {
			qDebug() << "EE   ProfileView" << __func__ << __LINE__ << "Elevation data from cache is invalid";
			continue;
		}
		y_value_uu -= this->y_visible_min;

		const int x_px = leftmost_px + x_pixels_per_unit * (x_value_uu - this->x_visible_min.ll_value());
		const int y_px = bottommost_px - y_pixels_per_unit * y_value_uu.ll_value();

		this->graph_2d->fill_rectangle(color, x_px - marker_shift, y_px - marker_shift, marker_size, marker_size);
	}

	return sg_ret::ok;
}




/**
   @reviewed-on tbd
*/
template <typename Tx, typename Ty>
sg_ret ProfileView<Tx, Ty>::draw_function_values(__attribute__((unused)) Track & trk)
{
	const size_t n_values = this->track_data_to_draw.size();
	if (0 == n_values) {
		qDebug() << "NN   ProfileView" << __func__ << __LINE__ << "There were zero values in" << graph_2d->debug;
		return sg_ret::err;
	}
	/*
	  It would be tempting to add "assert (n_columns ==
	  n_values)", but this assertion would fail most of the
	  time. We may have uncompressed track data set, we may have
	  zoomed in to the point where there will be only few track
	  data points (trackpoints) visible.
	*/

	const int n_rows = this->graph_2d->central_get_n_rows();
	const int n_columns = this->graph_2d->central_get_n_columns();
	const int leftmost_px = this->graph_2d->central_get_leftmost_pixel();
	const int bottommost_px = this->graph_2d->central_get_bottommost_pixel();


	qDebug() << "II   ProfileView" << __func__ << __LINE__
		 << "Will draw graph" << this->graph_2d->debug
		 << "with n values =" << n_values
		 << "into n columns =" << n_columns;

	double x_pixels_per_unit;
	double y_pixels_per_unit;
	this->get_pixels_per_unit(x_pixels_per_unit, y_pixels_per_unit);


	ScreenPos cur_valid_pos;
	ScreenPos last_valid_pos(leftmost_px, bottommost_px);

	for (size_t i = 0; i < n_values; i++) {

		const typename Tx::LL x_current_value_uu = this->track_data_to_draw.x_ll(i);
		/*
		  This line creates x coordinate that is
		  proportionally as far from left border, as
		  track_data_to_draw.x[i] is from track_data_to_draw.x_min.

		  It works equally well for x vectors with constant
		  intervals (e.g. the same time interval == 1s between
		  consecutive measurements of y), as for x vector with
		  varying intervals (e.g. different values of
		  distances between consecutive measurements of y).
		*/
		const int x_px = leftmost_px + x_pixels_per_unit * (x_current_value_uu - this->x_visible_min.ll_value());

		const bool y_value_valid = !std::isnan(this->track_data_to_draw.y_ll(i));

		if (y_value_valid) {
			const double y_value_uu = this->track_data_to_draw.y_ll(i);

			cur_valid_pos.rx() = x_px;
			cur_valid_pos.ry() = bottommost_px - (y_value_uu - this->y_visible_min.ll_value()) * y_pixels_per_unit;

			graph_2d->draw_line(this->main_values_valid_pen, last_valid_pos, cur_valid_pos);

			last_valid_pos = cur_valid_pos;
		} else {
			/*
			  Draw vertical line from bottom of graph to
			  top to indicate invalid y value.
			*/

			const int begin_y_px = bottommost_px;
			const int end_y_px = bottommost_px - n_rows;

			graph_2d->draw_line(this->main_values_invalid_pen, x_px, begin_y_px, x_px, end_y_px);
		}
	}

	return sg_ret::ok;
}




template <typename Tx, typename Ty>
bool ProfileView<Tx, Ty>::track_data_is_valid(void) const
{
	return this->track_data_to_draw.is_valid();
}




template <typename Tx, typename Ty>
sg_ret ProfileView<Tx, Ty>::draw_gps_speeds_relative(Track & trk)
{
	const int leftmost_px = this->graph_2d->central_get_leftmost_pixel();
	const int bottommost_px = this->graph_2d->central_get_bottommost_pixel();

	const size_t n_values = this->track_data_to_draw.size();

	const QColor & speed_color = this->aux_values_1_pen.color();

	const Speed max_speed = trk.get_max_speed();
	qDebug() << "II   ProfileView" << __func__ << __LINE__ << "Max speed is" << max_speed;

	if (max_speed.is_zero()) {
		qDebug() << "NN   ProfileView" << __func__ << __LINE__ << "Not drawing gps speeds because reference speed is zero";
		return sg_ret::ok;
	}

	const double speed_max = max_speed.ll_value();

	double x_pixels_per_unit;
	double y_pixels_per_unit;
	this->get_pixels_per_unit(x_pixels_per_unit, y_pixels_per_unit);

	for (size_t i = 0; i < n_values; i++) {

		const Trackpoint * tp = this->track_data_to_draw.tp(i);
		if (NULL == tp) {
			qDebug() << "NN   ProfileView" << __func__ << __LINE__ << "NULL trackpoint when drawing GPS speed for" << this->get_title();
			continue;
		}

		const double gps_speed = tp->gps_speed;
		if (std::isnan(gps_speed)) {
			qDebug() << "NN   ProfileView" << __func__ << __LINE__ << "NAN GPS speed for trackpoint when drawing GPS speed for" << this->get_title();
			continue;
		}

		const double x_value = this->track_data_to_draw.x_ll(i);
		const double y_value = 100 * gps_speed / speed_max; /* Percentage of maximum speed. */

		const int x_px = leftmost_px + x_pixels_per_unit * (x_value - this->x_visible_min.ll_value());
		const int y_px = bottommost_px - y_pixels_per_unit * y_value;

		/* This is just a speed indicator - no actual values can be inferred by user. */
		this->graph_2d->fill_rectangle(speed_color, x_px - 1, y_px - 1, 2, 2);
	}

	return sg_ret::ok;
}




/**
   \brief Draw the y = f(x) graph
*/
template <typename Tx, typename Ty>
sg_ret ProfileView<Tx, Ty>::draw_graph_without_crosshairs(Track & trk)
{
	qDebug() << "II   ProfileView" << __func__ << __LINE__;
	QTime draw_time;
	draw_time.start();


	/* Clear before redrawing. */
	this->graph_2d->clear();


	if (sg_ret::ok != this->regenerate_track_data_to_draw(trk)) {
		qDebug() << "EE   ProfileView" << __func__ << __LINE__ << "return 2";
		return sg_ret::err;
	}
	this->draw_function_values(trk);


	/* Draw grid on top of graph of values. */
	this->draw_x_grid();
	this->draw_y_grid();

	this->graph_2d->central_draw_outside_boundary_rect();

	this->draw_additional_indicators(trk);

	/* This will call ::paintEvent(), triggering final render to screen. */
	this->graph_2d->update();

	/* The pixmap = margin + graph area. */
	qDebug() << "II   ProfileView" << __func__ << __LINE__ << "before saving pixmap:" << QTime::currentTime();
	qDebug() << "II   ProfileView" << __func__ << __LINE__ << "Saving graph" << this->graph_2d->debug << "took" << draw_time.elapsed() << "ms";
	this->graph_2d->saved_pixmap = this->graph_2d->get_pixmap();
	this->graph_2d->saved_pixmap_valid = true;

	return sg_ret::ok;
}




template <typename Tx, typename Ty>
ProfileView<Tx, Ty>::~ProfileView()
{
	delete this->graph_2d;
}




template <typename Tx, typename Ty>
sg_ret ProfileView<Tx, Ty>::generate_initial_track_data_wrapper(Track * trk)
{
	this->initial_track_data.clear();

	/* Re-generate initial track data from track. */
	if (sg_ret::ok != this->initial_track_data.make_track_data_x_over_y(trk)) {
		qDebug() << "EE   ProfileView" << __func__ << __LINE__ << "Failed to generate valid initial track data for" << this->get_title();
		return sg_ret::err;
	}

	/*
	  It may be time consuming to convert units on whole long,
	  uncompressed ::initial_track_data. We could decide to do
	  this on compressed (and thus much smaller)
	  ::track_data_to_draw.

	  But in this place we do it only once, when a dialog is being
	  opened. Once it is done, we don't have to re-do it on every
	  resizing of dialog window.
	*/
	this->initial_track_data.apply_unit_conversions_xy(Tx::Unit::user_unit(), Ty::Unit::user_unit());

	qDebug() << "II   ProfileView" << __func__ << __LINE__ << "Generated valid initial track data for" << this->get_title();
	return sg_ret::ok;
}




template <typename Tx, typename Ty>
void ProfileView<Tx, Ty>::draw_y_grid(void)
{
	if (this->y_visible_range_uu.is_zero()) {
		qDebug() << "EE   ProfileView" << __func__ << __LINE__ << "Zero visible range:" << this->y_visible_range_uu;
		return;
	}

	const int n_columns      = this->get_central_n_columns();
	const int n_rows         = this->get_central_n_rows();
	const int left_width     = this->graph_2d->left_get_width();
	const int left_height    = this->graph_2d->left_get_height();
	const int leftmost_px    = this->graph_2d->central_get_leftmost_pixel();
	const int topmost_px     = this->graph_2d->central_get_topmost_pixel();
	const int bottommost_px  = this->graph_2d->central_get_bottommost_pixel();


	double x_pixels_per_unit;
	double y_pixels_per_unit;
	this->get_pixels_per_unit(x_pixels_per_unit, y_pixels_per_unit);

	Ty first_multiple_uu(0.0, Ty::Unit::user_unit());
	Ty last_multiple_uu(0.0, Ty::Unit::user_unit());
	GraphIntervals<Ty>::find_multiples_of_interval(this->y_visible_min, this->y_visible_max, this->y_interval, first_multiple_uu, last_multiple_uu);

#if 1   /* Debug. */
	qDebug() << "DD   ProfileView" << __func__ << __LINE__ << "      graph:" << this->get_title();
	qDebug() << "DD   ProfileView" << __func__ << __LINE__ << "      visible range =" << this->y_visible_range_uu;
	qDebug() << "DD   ProfileView" << __func__ << __LINE__ << "      n rows =" << n_rows << ", n cols =" << n_columns;
	qDebug() << "DD   ProfileView" << __func__ << __LINE__ << "      leftmost px =" << leftmost_px << ", bottommost px =" << bottommost_px;
	qDebug() << "DD   ProfileView" << __func__ << __LINE__ << "      y visible min =" << this->y_visible_min << ", y visible max =" << this->y_visible_max;
	qDebug() << "DD   ProfileView" << __func__ << __LINE__ << "      interval =" << this->y_interval << ", first_multiple_uu =" << first_multiple_uu << ", last_multiple_uu = " << last_multiple_uu;
	qDebug() << "DD   ProfileView" << __func__ << __LINE__ << "      y pixels per unit =" << y_pixels_per_unit;
#endif



	/* Be sure to keep type of y_value_uu as floating-point
	   compatible, otherwise for intervals smaller than 1.0 you
	   will get forever loop. */
	for (Ty y_value_uu = first_multiple_uu; y_value_uu <= last_multiple_uu; y_value_uu += this->y_interval) {
		/* 'y_px' is in "beginning in top-left corner" coordinate system. */
		const int y_px = bottommost_px - y_pixels_per_unit * (y_value_uu - this->y_visible_min).ll_value();

		if (y_px >= topmost_px && y_px <= bottommost_px) {
			qDebug() << "DD   ProfileView" << __func__ << __LINE__ << "      value (inside) =" << y_value_uu << ", y_px =" << y_px;

			/* Graph line. From left edge of central area to right edge of central area. */
			this->graph_2d->central_draw_line(this->graph_2d->grid_pen,
							  leftmost_px,             y_px,
							  leftmost_px + n_columns, y_px);

			/* Text label in left margin. */
			const QRectF bounding_rect = QRectF(2, y_px, left_width - 4, left_height);
			this->graph_2d->draw_text(this->graph_2d->labels_font,
						  this->graph_2d->labels_pen,
						  bounding_rect,
						  Qt::AlignRight | Qt::AlignTop,
						  y_value_uu.to_string(),
						  TextOffset::Up);
		} else {
			qDebug() << "NN   ProfileView" << __func__ << __LINE__ << "      value (outside) =" << y_value_uu << ", y_px =" << y_px;
		}
	}
}




template <typename Tx, typename Ty>
void ProfileView<Tx, Ty>::draw_x_grid(void)
{
	const int n_rows         = this->get_central_n_rows();
	const int n_columns      = this->get_central_n_columns();
	const int bottom_width   = this->graph_2d->bottom_get_width();
	const int bottom_height  = this->graph_2d->bottom_get_height();
	const int leftmost_px    = this->graph_2d->central_get_leftmost_pixel();
	const int rightmost_px   = this->graph_2d->central_get_rightmost_pixel();
	const int topmost_px     = this->graph_2d->central_get_topmost_pixel();
	const int bottommost_px  = this->graph_2d->central_get_bottommost_pixel();

	if (this->x_visible_range_uu.is_zero()) {
		qDebug() << "EE   ProfileView" << __func__ << __LINE__ << "Zero visible range:" << this->x_visible_min << this->x_visible_max;
		return;
	}

	double x_pixels_per_unit;
	double y_pixels_per_unit;
	this->get_pixels_per_unit(x_pixels_per_unit, y_pixels_per_unit);

	Tx first_multiple_uu(0, Tx::Unit::user_unit());
	Tx last_multiple_uu(0, Tx::Unit::user_unit());
	GraphIntervals<Tx>::find_multiples_of_interval(this->x_visible_min, this->x_visible_max, this->x_interval, first_multiple_uu, last_multiple_uu);

#if 1   /* Debug. */
	qDebug() << "DD   ProfileView" << __func__ << __LINE__ << "      graph:" << this->get_title();
	qDebug() << "DD   ProfileView" << __func__ << __LINE__ << "      visible range =" << this->x_visible_range_uu;
	qDebug() << "DD   ProfileView" << __func__ << __LINE__ << "      n rows =" << n_rows << ", n cols =" << n_columns;
	qDebug() << "DD   ProfileView" << __func__ << __LINE__ << "      leftmost px =" << leftmost_px << ", bottommost px =" << bottommost_px;
	qDebug() << "DD   ProfileView" << __func__ << __LINE__ << "      x visible min =" << this->x_visible_min << ", x visible max =" << this->x_visible_max;
	qDebug() << "DD   ProfileView" << __func__ << __LINE__ << "      x interval =" << this->x_interval << ", first_multiple_uu =" << first_multiple_uu << ", last_multiple_uu = " << last_multiple_uu;
	qDebug() << "DD   ProfileView" << __func__ << __LINE__ << "      x pixels per unit =" << x_pixels_per_unit;
#endif

	for (Tx x_value_uu = first_multiple_uu; x_value_uu <= last_multiple_uu; x_value_uu += this->x_interval) {
		/* 'x_px' is in "beginning in top-left corner" coordinate system. */
		const int x_px = leftmost_px + (x_value_uu.ll_value() - this->x_visible_min.ll_value()) * x_pixels_per_unit;

		if (x_px >= leftmost_px && x_px <= rightmost_px) {
			qDebug() << "DD   ProfileView" << __func__ << __LINE__ << "      value (inside) =" << x_value_uu << ", x_px =" << x_px;

			/* Graph line. From bottom of central area to top of central area. */
			this->graph_2d->central_draw_line(this->graph_2d->grid_pen,
							  x_px, topmost_px,
							  x_px, topmost_px + n_rows);

			/* Text label in bottom margin. */
			const QRectF bounding_rect = QRectF(x_px, bottommost_px + 1, bottom_width - 3, bottom_height - 3);
			const QString label = x_value_uu.to_string();
			this->graph_2d->draw_text(this->graph_2d->labels_font, this->graph_2d->labels_pen, bounding_rect, Qt::AlignLeft | Qt::AlignTop, label, TextOffset::Left);
		} else {
			qDebug() << "NN   ProfileView" << __func__ << __LINE__ << "      value (outside) =" << x_value_uu << ", x_px =" << x_px;
		}
	}
}




template <typename Tx, typename Ty>
void ProfileView<Tx, Ty>::get_pixels_per_unit(double & x_pixels_per_unit, double & y_pixels_per_unit) const
{
	const int n_columns = this->graph_2d->central_get_n_columns();
	const int n_rows = this->graph_2d->central_get_n_rows();

	x_pixels_per_unit = (1.0 * n_columns) / this->x_visible_range_uu.ll_value();
	y_pixels_per_unit = (1.0 * n_rows) / this->y_visible_range_uu.ll_value();
}




	/* ET = elevation as a function of time. */
	class ProfileViewET : public ProfileView<Time, Altitude> {
	public:
		ProfileViewET(TrackProfileDialog * dialog);
		~ProfileViewET() {};


		void configure_controls(void) override;
		void save_settings(void) override;
		sg_ret draw_additional_indicators(Track & trk) override;
	};


	/* SD = speed as a function of distance. */
	class ProfileViewSD : public ProfileView<Distance, Speed> {
	public:
		ProfileViewSD(TrackProfileDialog * dialog);
		~ProfileViewSD() {};

		void configure_controls(void) override;
		void save_settings(void) override;
		sg_ret draw_additional_indicators(Track & trk) override;

	};


	/* ED = elevation as a function of distance. */
	class ProfileViewED : public ProfileView<Distance, Altitude> {
	public:
		ProfileViewED(TrackProfileDialog * dialog);
		~ProfileViewED() {};

		void configure_controls(void) override;
		void save_settings(void) override;
		sg_ret draw_additional_indicators(Track & trk) override;
	private:

	};


	/* GD = gradient as a function of distance. */
	class ProfileViewGD : public ProfileView<Distance, Gradient> {
	public:
		ProfileViewGD(TrackProfileDialog * dialog);
		~ProfileViewGD() {};

		void configure_controls(void) override;
		void save_settings(void) override;
		sg_ret draw_additional_indicators(Track & trk) override;
	};


	/* ST = speed as a function of time. */
	class ProfileViewST : public ProfileView<Time, Speed> {
	public:
		ProfileViewST(TrackProfileDialog * dialog);
		~ProfileViewST() {};

		void configure_controls(void) override;
		void save_settings(void) override;
		sg_ret draw_additional_indicators(Track & trk) override;
	};


	/* DT = distance as a function of time. */
	class ProfileViewDT : public ProfileView<Time, Distance> {
	public:
		ProfileViewDT(TrackProfileDialog * dialog);
		~ProfileViewDT() {};

		void configure_controls(void) override;
		void save_settings(void) override;
		sg_ret draw_additional_indicators(Track & trk) override;
	};




	void track_profile_dialog(Track * trk, GisViewport * main_gisview, QWidget * parent);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TRACK_PROFILE_DIALOG_H_ */
