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

#ifndef _SG_VIEWPORT_INTERNAL_H_
#define _SG_VIEWPORT_INTERNAL_H_




#include <list>
#include <cstdint>




#include <QPen>
#include <QWidget>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QPrinter>




#include "viewport.h"
#include "viewport_decorations.h"
#include "viewport_zoom.h"
#include "viewport_pixmap.h"
#include "coord.h"
#include "bbox.h"




namespace SlavGPS {




	class Window;
	class Layer;
	class Viewport2D;
	class ViewportMargin;
	class ViewportPixmap;







	/* GIS-aware viewport. Viewport that knows something about
	   coordinates etc. */
	class GisViewport : public QWidget {
		Q_OBJECT

		friend class GisViewportDecorations;
		friend class Viewport2D;
	public:

		/* ******** Methods that definitely should be in this class. ******** */

		GisViewport(QWidget * parent = NULL);
		~GisViewport();


		void draw_bbox(const LatLonBBox & bbox, const QPen & pen);
		sg_ret set_bbox(const LatLonBBox & bbox);
		/* Positive value of a margin mean that we want to shrink bbox from specified side. */
		LatLonBBox get_bbox(int margin_left = 0, int margin_right = 0, int margin_top = 0, int margin_bottom = 0) const;

		CoordMode get_coord_mode(void) const;
		void set_coord_mode(CoordMode mode);

		bool go_back(void);
		bool go_forward(void);
		bool back_available(void) const;
		bool forward_available(void) const;

		const Coord & get_center_coord(void) const;
		std::list<QString> get_center_coords_list(void) const;
		void show_center_coords(Window * parent) const;
		void print_center_coords(const QString & label) const;


		/* Coordinate transformations.
		   Pixel coordinates passed to the functions are in
		   Qt's coordinate system, where beginning (0,0 point)
		   is in top-left corner. */
		Coord screen_pos_to_coord(ScreenPosition screen_pos) const;
		Coord screen_pos_to_coord(int x, int y) const;
		Coord screen_pos_to_coord(const ScreenPos & pos) const;

		/* Coordinate transformations. */
		sg_ret coord_to_screen_pos(const Coord & coord, int * x, int * y) const;
		ScreenPos coord_to_screen_pos(const Coord & coord) const;


		/* GisViewport's zoom. */
		void zoom_in(void);
		void zoom_out(void);

		sg_ret set_viking_scale(double new_value);
		sg_ret set_viking_scale_x(double new_value);
		sg_ret set_viking_scale_y(double new_value);
		sg_ret set_viking_scale(const VikingScale & new_value);
		const VikingScale & get_viking_scale(void) const;

		void set_draw_mode(GisViewportDrawMode drawmode);
		GisViewportDrawMode get_draw_mode(void) const;

		sg_ret utm_recalculate_current_center_coord_for_other_zone(UTM & center_in_other_zone, int zone);
		sg_ret get_corners_for_zone(Coord & coord_ul, Coord & coord_br, int zone);

		int get_leftmost_zone(void) const;
		int get_rightmost_zone(void) const;
		bool get_is_one_utm_zone(void) const;



		sg_ret set_center_coord(const Coord & coord, bool save_position = true);
		sg_ret set_center_coord(const UTM & utm, bool save_position = true);
		sg_ret set_center_coord(const LatLon & lat_lon, bool save_position = true);

		/* These pixel coordinates should be in Qt's
		   coordinate system, where beginning (point 0,0) is
		   in upper-left corner. */
		sg_ret set_center_coord(int x, int y);
		sg_ret set_center_coord(const ScreenPos & pos);



		void emit_center_coord_or_zoom_changed(const QString & trigger_name);



		/* ******** Other methods. ******** */


		/* GisViewport module initialization function. */
		static void init(void);

		/* If a viewport widget already has some non-zero geometry,
		   you can call this method without arguments. */
		void reconfigure_drawing_area(int width = 0, int height = 0);

		void paintEvent(QPaintEvent * event);
		void resizeEvent(QResizeEvent * event);
		void mousePressEvent(QMouseEvent * event); /* Double click is handled through event filter. */
		void mouseMoveEvent(QMouseEvent * event);
		void mouseReleaseEvent(QMouseEvent * event);
		void wheelEvent(QWheelEvent * event);

		void dragEnterEvent(QDragEnterEvent * event);
		void dropEvent(QDropEvent * event);

		void draw_mouse_motion_cb(QMouseEvent * event);



		/* Run this before drawing a line. ViewportPixmap::draw_line() runs it for you. */
		static void clip_line(int * x1, int * y1, int * x2, int * y2);

		/* Get cursor position of a mouse event.  Returned
		   position is in "beginning is in bottom-left corner"
		   coordinate system. */
		sg_ret get_cursor_pos_cbl(QMouseEvent * ev, ScreenPos & screen_pos) const;




		QRect get_rect(void) const;

		double get_vpixmap_height_m(void) const;
		double get_vpixmap_width_m(void) const;





		/*
		  Add given @attribution to list of attributions displayed in viewport.
		*/
		sg_ret add_attribution(const QString & attribution);
		sg_ret add_logo(const GisViewportLogo & logo);



		QPen get_highlight_pen(void) const;
		void set_highlight_thickness(int thickness);

		void set_highlight_color(const QString & color_name);
		void set_highlight_color(const QColor & color);
		const QColor & get_highlight_color(void) const;



		/* Color/graphics context management. */
		void set_background_color(const QColor & color);
		void set_background_color(const QString & color_name);
		const QColor & get_background_color(void) const;




		void draw_decorations(void);
		void set_center_mark_visibility(bool new_state);
		bool get_center_mark_visibility(void) const;
		void set_scale_visibility(bool new_state);
		bool get_scale_visibility(void) const;

		void set_highlight_usage(bool new_state);
		bool get_highlight_usage(void) const;

		void clear(void);

		void request_redraw(const QString & trigger_descr);

		/* GisViewport buffer management/drawing to screen. */
		QPixmap get_pixmap(void) const;   /* Get contents of drawing buffer. */
		void set_pixmap(const QPixmap & pixmap);
		void sync(void);              /* Draw buffer to window. */
		void pan_sync(int x_off, int y_off);


		/* Pixel coordinates passed to this function should be
		   in Qt's coordinate system, where beginning (pixel
		   0,0) is in upper-left corner. */
		void compute_bearing(int begin_x, int begin_y, int end_x, int end_y, Angle & angle, Angle & base_angle);

		/* Trigger stuff. */
		void set_trigger(Layer * trigger);
		Layer * get_trigger(void) const;

		void snapshot_save(void);
		void snapshot_load(void);

		void set_half_drawn(bool half_drawn);
		bool get_half_drawn(void) const;

		bool is_ready(void) const;


		/* Last arg may be invalid (::is_valid() returns
		   false) - the function will then use its standard
		   algorithm for determining expected zoom of scaled
		   viewport. */
		GisViewport * create_scaled_viewport(Window * window, int target_width, int target_height, const VikingScale & expected_viking_scale);



		Window * get_window(void) const;






		/* ******** Class variables that definitely should be in this class. ******** */



		Coord center_coord;
		/* center_coords_iter++ means moving forward in history. Thus prev(center_coords->end()) is the newest item.
		   center_coords_iter-- means moving backward in history. Thus center_coords->begin() is the oldest item (in the beginning of history). */
		std::list<Coord> * center_coords;  /* The history of requested positions. */
		std::list<Coord>::iterator center_coords_iter; /* Current position within the history list. */
		int center_coords_max;      /* Configurable maximum size of the history list. */
		int center_coords_radius;   /* Metres. */


		CoordMode coord_mode = CoordMode::LatLon;
		GisViewportDrawMode draw_mode = GisViewportDrawMode::Mercator;


		double utm_zone_width = 0.0;
		bool is_one_utm_zone;




		/* ******** Other class variables. ******** */


		/* Whether or not to display some decorations. */
		bool scale_visibility = true;
		bool center_mark_visibility = true;







		/* Properties of text labels drawn on margins of charts (next to each horizontal/vertical grid line). */
		QPen labels_pen;
		QFont labels_font;

		QPen background_pen;
		QColor background_color;

		QPen highlight_pen;
		QColor highlight_color;

		bool highlight_usage = true;

		/* x/y mark lines indicating e.g. current position of cursor in viewport (sort of a crosshair indicator). */
		QPen marker_pen;
		/* Generic pen for a generic (other than geographical coordinates) grid. */
		QPen grid_pen;


		/* Trigger stuff. */
		Layer * trigger = NULL;
		bool half_drawn = false;

		char debug[100] = { 0 };

		ViewportPixmap vpixmap;

	private:
		/* ******** Methods that definitely should be in this class. ******** */

		/* Store the current center coordinate into the history list
		   and emit a signal to notify clients the list has been updated. */
		void save_current_center_coord(void);

		void free_center_coords(std::list<Coord>::iterator iter);


		double calculate_utm_zone_width(void) const;
		void utm_zone_check(void);



		/* ******** Other methods. ******** */



		/* ******** Class variables that definitely should be in this class. ******** */

		VikingScale viking_scale;
		double xmfactor = 0.0f;
		double ymfactor = 0.0f;

		GisViewportDecorations decorations;


		/* ******** Other class variables. ******** */

		Window * window = NULL;


	signals:
		/* ******** Signals that definitely should be in this class. ******** */


		/* ******** Other signals. ******** */
		void cursor_moved(GisViewport * gisview, QMouseEvent * event);
		void button_released(GisViewport * gisview, QMouseEvent * event);

		/* TODO: do we need to have two separate signals? */
		void center_coord_updated(void);
		void center_coord_or_zoom_changed(void);


	public slots:
		bool print_cb(QPrinter *);

	protected:
		bool eventFilter(QObject * object, QEvent * event);
	};




	class ViewportMargin : public ViewportPixmap {
		Q_OBJECT
	public:

		enum class Position {
			Left,
			Right,
			Top,
			Bottom
		};

		ViewportMargin(ViewportMargin::Position pos, int main_size, QWidget * parent = NULL);

		void paintEvent(QPaintEvent * event);
		void resizeEvent(QResizeEvent * event);

		ViewportMargin::Position position;

		/* The border around main area of viewport. It's specified by margin sizes. */
		QPen border_pen;

		/* For left/right margin this is width.
		   For top/bottom margin this is height. */
		int size = 0;
	};




	class Viewport2D : public QWidget {
		Q_OBJECT
	public:
		Viewport2D(int l, int r, int t, int b, QWidget * parent = NULL);

		/* (Re-)create central widget. */
		void create_central(void);
		/* (Re-)create margins with given sizes. */
		void create_margins(int l, int r, int t, int b);

		int central_get_width(void) const;
		int central_get_height(void) const;
		int left_get_width(void) const;
		int left_get_height(void) const;
		int right_get_width(void) const;
		int right_get_height(void) const;
		int top_get_width(void) const;
		int top_get_height(void) const;
		int bottom_get_width(void) const;
		int bottom_get_height(void) const;


		/* Draw a line in central part of viewport.  x/y
		   coordinates should be in "beginning is in
		   bottom-left corner" coordinates system. */
		void central_draw_line(const QPen & pen, int begin_x, int begin_y, int end_x, int end_y);

		/* Draw a crosshair in central part of viewport.  x/y
		   coordinates should be in "beginning is in bottom
		   left corner" coordinates system.

		   "Simple" means one horizontal and one vertical line
		   crossing at given position. */
		void central_draw_simple_crosshair(const ScreenPos & pos);

		void margin_draw_text(ViewportMargin::Position pos, QFont const & text_font, QPen const & pen, const QRectF & bounding_rect, int flags, QString const & text, int text_offset);


		ViewportMargin * left = NULL;
		ViewportMargin * right = NULL;
		ViewportMargin * top = NULL;
		ViewportMargin * bottom = NULL;
		GisViewport * central = NULL;

		GisViewportDomain x_domain = GisViewportDomain::Max;
		GisViewportDomain y_domain = GisViewportDomain::Max;

		HeightUnit height_unit;
		DistanceUnit distance_unit;
		SpeedUnit speed_unit;

		QGridLayout * grid = NULL;
	};




	class ArrowSymbol {
	public:
		/**
		   @param blades_width_degrees - how widely the arrow blades will be spread?
		*/
		ArrowSymbol(double blades_width_degrees = 15.0, int size_factor = 1);

		/**
		   @param direction decides in which direction the arrow head will be pointing. -1 or +1.
		*/
		void set_arrow_tip(int x, int y, int direction = 1);

		sg_ret paint(QPainter & painter, double dx, double dy);

	private:
		double cosine_factor = 0.0;
		double sine_factor = 0.0;

		int tip_x = 0;
		int tip_y = 0;
		int direction = 1;
	};




} /* namespace SlavGPS */




#endif /* _SG_VIEWPORT_INTERNAL_H_ */
