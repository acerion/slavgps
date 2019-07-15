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

#ifndef _SG_VIEWPORT_PIXMAP_H_
#define _SG_VIEWPORT_PIXMAP_H_




#include <QObject>
#include <QWidget>
#include <QPainter>
#include <QPixmap>
#include <QDebug>




#include "viewport.h"




namespace SlavGPS {




	/* Wrapper around pixmap and a painter that paints to the
	   pixmap.  Simple class providing paint primitives. */
	class ViewportPixmap : public QWidget {
		Q_OBJECT

		friend class GisViewportDecorations;
	public:
		ViewportPixmap(int left = 0, int right = 0, int top = 0, int bottom = 0, QWidget * parent = NULL);
		~ViewportPixmap();

		enum class MarginPosition {
			Left,
			Right,
			Top,
			Bottom
		};

		void paintEvent(QPaintEvent * event);

		void reconfigure(int width, int height);

		/* If a viewport widget already has some non-zero geometry,
		   you can call this method without arguments. */
		void reconfigure_drawing_area(int width = 0, int height = 0);

		/* ViewportPixmap buffer management/drawing to screen. */
		QPixmap get_pixmap(void) const;   /* Get contents of drawing buffer. */
		void set_pixmap(const QPixmap & pixmap);
		void sync(void);              /* Draw buffer to window. */
		void pan_sync(int x_off, int y_off);

		void snapshot_save(void);
		void snapshot_load(void);

		bool is_ready(void) const;


		/* Run this before drawing a line. ViewportPixmap::draw_line() runs it for you. */
		static void clip_line(int * x1, int * y1, int * x2, int * y2);


		/* Returned pixels are in Qt's coordinate system,
		   where beginning (0,0 point) is in top-left
		   corner. */
		int central_get_leftmost_pixel(void) const;
		int central_get_rightmost_pixel(void) const;
		int central_get_upmost_pixel(void) const;
		int central_get_bottommost_pixel(void) const;

		/* Get number of pixel (starting with zero) that is in
		   center of viewport pixmap, either at the center of
		   vertical line, or at the center of horizontal
		   line. */
		int central_get_y_center_pixel(void) const;
		int central_get_x_center_pixel(void) const;
		ScreenPos central_get_center_screen_pos(void) const;

		int total_get_width(void) const;
		int total_get_height(void) const;

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

		QRect central_get_rect(void) const;

		void clear(void);



		/* Drawing primitives. */
		void draw_line(QPen const & pen, int begin_x, int begin_y, int end_x, int end_y);
		void draw_line(QPen const & pen, const ScreenPos & begin, const ScreenPos & end);
		void draw_rectangle(QPen const & pen, int upper_left_x, int upper_left_y, int width, int height);
		void draw_rectangle(QPen const & pen, const QRect & rect);
		void fill_rectangle(QColor const & color, int x, int y, int width, int height);

		void draw_text(QFont const & font, QPen const & pen, int x, int y, QString const & text);
		void draw_text(const QFont & font, const QPen & pen, const QRectF & bounding_rect, int flags, const QString & text, int text_offset);
		void draw_text(QFont const & text_font, QPen const & pen, const QColor & bg_color, const QRectF & bounding_rect, int flags, QString const & text, int text_offset);
		void draw_outlined_text(QFont const & text_font, QPen const & outline_pen, const QColor & fill_color, const QPointF & base_point, QString const & text);

		void draw_arc(QPen const & pen, int x, int y, int width, int height, int start_angle, int span_angle);
		void draw_ellipse(QPen const & pen, const QPoint & center, int radius_x, int radius_y, bool filled);
		void draw_polygon(QPen const & pen, QPoint const * points, int npoints, bool filled);

		void draw_pixmap(const QPixmap & pixmap, int viewport_x, int viewport_y, int pixmap_x, int pixmap_y, int pixmap_width, int pixmap_height);
		void draw_pixmap(const QPixmap & pixmap, int viewport_x, int viewport_y);
		void draw_pixmap(const QPixmap & pixmap, const QRect & viewport_rect, const QRect & pixmap_rect);


		void margin_draw_text(ViewportPixmap::MarginPosition pos, QFont const & text_font, QPen const & pen, const QRectF & bounding_rect, int flags, QString const & text, int text_offset);

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


		bool line_is_outside(int begin_x, int begin_y, int end_x, int end_y);


		QPainter & get_painter(void) { return this->painter; }


		void set_highlight_usage(bool new_state);
		bool get_highlight_usage(void) const;

		QPen get_highlight_pen(void) const;
		void set_highlight_thickness(int thickness);

		void set_highlight_color(const QString & color_name);
		void set_highlight_color(const QColor & color);
		const QColor & get_highlight_color(void) const;



		/* Color/graphics context management. */
		void set_background_color(const QColor & color);
		void set_background_color(const QString & color_name);
		const QColor & get_background_color(void) const;




		char debug[100] = { 0 };

		/* TODO: move to 'protected'. */
		QPixmap saved_pixmap;
		bool saved_pixmap_valid;


		/* x/y mark lines indicating e.g. current position of cursor in viewport (sort of a crosshair indicator). */
		QPen marker_pen;
		/* Generic pen for a generic (other than geographical coordinates) grid. */
		QPen grid_pen;


		/* Properties of text labels drawn on margins of charts (next to each horizontal/vertical grid line). */
		QPen labels_pen;
		QFont labels_font;

		QPen background_pen;
		QColor background_color;

	protected:
		//Viewport2D * parent_viewport = NULL;

		/* Full width/height, i.e. including margins. */
		int total_width = 0;
		int total_height = 0;

		int left_margin = 0;
		int right_margin = 0;
		int top_margin = 0;
		int bottom_margin = 0;

		QPen highlight_pen;
		QColor highlight_color;

		bool highlight_usage = true;


		QPainter painter;
		QPixmap * pixmap = NULL;
		QPixmap * snapshot_buffer = NULL;


	signals:
		void reconfigured(ViewportPixmap * vpixmap);
	};
	QDebug operator<<(QDebug debug, const ViewportPixmap & vpixmap);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_VIEWPORT_PIXMAP_H_ */
