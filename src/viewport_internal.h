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
#include "viewport_utils.h"
#include "viewport_decorations.h"
#include "coord.h"
#include "bbox.h"
#include "globals.h"




namespace SlavGPS {




	class Window;
	class Layer;




	class Viewport : public QWidget {
	Q_OBJECT

	public:
		Viewport(Window * parent);
		~Viewport();

		/* If a viewport widget already has some non-zero geometry,
		   you can call this method without arguments. */
		void reconfigure_drawing_area(int width = 0, int height = 0);

		void paintEvent(QPaintEvent * event);
		void resizeEvent(QResizeEvent * event);
		void mousePressEvent(QMouseEvent * event); /* Double click is handled through event filter. */
		void mouseMoveEvent(QMouseEvent * event);
		void mouseReleaseEvent(QMouseEvent * event);
		void wheelEvent(QWheelEvent * event);

		void draw_mouse_motion_cb(QMouseEvent * event);



		/* Drawing primitives. */
		void draw_line(QPen const & pen, int begin_x, int begin_y, int end_x, int end_y);
		void draw_rectangle(QPen const & pen, int upper_left_x, int upper_left_y, int width, int height);
		void fill_rectangle(QColor const & color, int x, int y, int width, int height);
		void draw_text(QFont const & font, QPen const & pen, int x, int y, QString const & text);
		void draw_text(QFont const & font, QPen const & pen, QRectF & bounding_rect, int flags, QString const & text, int text_offset);
		void draw_text(QFont const & text_font, QPen const & pen, const QColor & bg_color, const QRectF & bounding_rect, int flags, QString const & text, int text_offset);
		void draw_arc(QPen const & pen, int x, int y, int width, int height, int angle1, int angle2, bool filled);
		void draw_polygon(QPen const & pen, QPoint const * points, int npoints, bool filled);
		void draw_pixmap(QPixmap const & pixmap, int src_x, int src_y, int dest_x, int dest_y, int dest_width, int dest_height);

		/* Run this before drawing a line. Viewport::draw_line() runs it for you. */
		static void clip_line(int * x1, int * y1, int * x2, int * y2);

		CoordMode get_coord_mode(void) const;
		void set_coord_mode(CoordMode mode);


		bool go_back(void);
		bool go_forward(void);
		bool back_available(void) const;
		bool forward_available(void) const;


		const Coord * get_center() const;
		std::list<QString> get_centers_list(void) const;
		void show_centers(Window * parent) const;
		void print_centers(const QString & label) const;
		void update_centers(void);



		/* Viewport position. */
		void set_center_from_coord(const Coord & coord, bool save_position);
		void set_center_from_utm(const UTM & utm, bool save_position);
		void set_center_from_latlon(const LatLon & lat_lon, bool save_position);

		void set_center_from_screen_pos(int x, int y);
		void set_center_from_screen_pos(const ScreenPos & pos);

		void get_center_for_zone(UTM & center, int zone);
		void get_corners_for_zone(Coord & coord_ul, Coord & coord_br, int zone);

		char get_leftmost_zone(void) const;
		char get_rightmost_zone(void) const;


		LatLonMinMax get_min_max_lat_lon(void) const;
		LatLonBBox get_bbox(void) const;

		int get_width(void) const;
		int get_height(void) const;

		/* Coordinate transformations. */
		Coord screen_pos_to_coord(int x, int y) const;
		Coord screen_pos_to_coord(const ScreenPos & pos) const;
		void coord_to_screen_pos(const Coord & coord, int * x, int * y);
		ScreenPos coord_to_screen_pos(const Coord & coord);

		/* Viewport scale. */
		void set_xmpp(double new_xmpp);
		void set_ympp(double new_ympp);
		double get_xmpp(void) const;
		double get_ympp(void) const;
		void set_zoom(double new_mpp);
		double get_zoom(void) const;
		void zoom_in();
		void zoom_out();


		void add_copyright(QString const & copyright);
		void add_logo(QPixmap const * logo);



		QPen get_highlight_pen(void) const;
		void set_highlight_thickness(int thickness);

		void set_highlight_color(const QString & color_name);
		void set_highlight_color(const QColor & color);
		const QColor & get_highlight_color(void) const;



		/* Color/graphics context management. */
		void set_background_color(const QColor & color);
		void set_background_color(const QString & color_name);
		const QColor & get_background_color(void) const;


		bool is_one_zone(void) const;


		void draw_decorations(void);
		void set_center_mark_visibility(bool new_state);
		bool get_center_mark_visibility(void) const;
		void set_scale_visibility(bool new_state);
		bool get_scale_visibility(void) const;


		void set_highlight_usage(bool new_state);
		bool get_highlight_usage(void) const;



		void clear(void);


		void set_drawmode(ViewportDrawMode drawmode);
		ViewportDrawMode get_drawmode(void) const;



		/* Viewport buffer management/drawing to screen. */
		QPixmap * get_pixmap(void) const;   /* Get pointer to drawing buffer. */
		void set_pixmap(QPixmap & pixmap);
		void sync(void);              /* Draw buffer to window. */
		void pan_sync(int x_off, int y_off);



		/* Utilities. */
		void compute_bearing(int x1, int y1, int x2, int y2, double *angle, double *baseangle);


		void set_margin(int top, int bottom, int left, int right);
		void draw_border(void);



		/* Trigger stuff. */
		void set_trigger(Layer * trigger);
		Layer * get_trigger(void) const;

		void snapshot_save(void);
		void snapshot_load(void);

		void set_half_drawn(bool half_drawn);
		bool get_half_drawn(void) const;


		/* Whether or not to display some decorations. */
		bool scale_visibility = true;
		bool center_mark_visibility = true;

		double xmpp, ympp;
		double xmfactor, ymfactor;


		CoordMode coord_mode;
		Coord center;

		/* centers_iter++ means moving forward in history. Thus prev(centers->end()) is the newest item.
		   centers_iter-- means moving backward in history. Thus centers->begin() is the oldest item (in the beginning of history). */
		std::list<Coord *> * centers;  /* The history of requested positions. */
		std::list<Coord *>::iterator centers_iter; /* Current position within the history list. */

		int centers_max;      /* configurable maximum size of the history list. */
		int centers_radius;   /* Metres. */

		QPixmap * scr_buffer = NULL;

		int size_width = 0;
		int size_height = 0;
		/* Half of the normal width and height. */
		int size_width_2 = 0;
		int size_height_2 = 0;


		double utm_zone_width;
		bool one_utm_zone;

		/* Subset of coord types. lat lon can be plotted in 2 ways, google or exp. */
		ViewportDrawMode drawmode;


		QPen background_pen;
		QColor background_color;

		QPen highlight_pen;
		QColor highlight_color;

		bool highlight_usage = true;

		/* The border around main area of viewport. It's specified by margin sizes. */
		QPen border_pen;
		/* x/y mark lines indicating e.g. current position of cursor in viewport (sort of a crosshair indicator). */
		QPen marker_pen;
		/* Generic pen for a generic (other than geographical coordinates) grid. */
		QPen grid_pen;


		/* Trigger stuff. */
		Layer * trigger = NULL;
		QPixmap * snapshot_buffer = NULL;
		bool half_drawn = false;

		char type_string[100] = { 0 };

		int margin_top = 0;
		int margin_bottom = 0;
		int margin_left = 0;
		int margin_right = 0;

	private:
		void free_center(std::list<Coord *>::iterator iter);
		void init_drawing_area(void);

		double calculate_utm_zone_width(void) const;
		void utm_zone_check(void);


		Window * window = NULL;
		ViewportDecorations decorations;

	signals:
		void updated_center(void);
		void cursor_moved(Viewport * viewport, QMouseEvent * event);
		void button_released(Viewport * viewport, QMouseEvent * event);
		void reconfigured(Viewport * viewport);


	public slots:
		bool reconfigure_drawing_area_cb(void);
		bool print_cb(QPrinter *);

	protected:
		bool eventFilter(QObject * object, QEvent * event);
	};




	void viewport_init(void);




} /* namespace SlavGPS */




#endif /* _SG_VIEWPORT_INTERNAL_H_ */
