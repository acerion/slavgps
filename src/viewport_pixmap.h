/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2019, Kamil Ignacak <acerion@wp.pl>
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




	enum class TextOffset : uint8_t {
		None  = 0x00,
		Left  = 0x01,
		Right = 0x02,
		Up    = 0x04,
		Down  = 0x08
	};




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

		/**
		   @brief Reconfigure a pixmap so that it uses new
		   total width and new total height

		   The function can be called when parent widget
		   (e.g. main window) is being resized by user.

		   Margin sizes are not affected.

		   @param total_width and @param total_height refer to
		   pixmap size with margins.
		*/
		void apply_total_sizes(int total_width, int total_height);


		/* ViewportPixmap buffer management/drawing to screen. */
		const QPixmap & get_pixmap(void) const;   /* Get contents of drawing buffer. */
		void set_pixmap(const QPixmap & pixmap);
		void render_to_screen(void);              /* Draw buffer to window. */
		void pan_sync(int x_off, int y_off);

		void snapshot_save(void);
		void snapshot_restore(void);

		/* Run this before drawing a line. ViewportPixmap::draw_line() runs it for you. */
		static void clip_line(fpixel * x1, fpixel * y1, fpixel * x2, fpixel * y2);


		/* Returned pixels are in Qt's coordinate system,
		   where beginning (0,0 point) is in top-left
		   corner. */
		int central_get_leftmost_pixel(void) const;
		int central_get_rightmost_pixel(void) const;
		int central_get_topmost_pixel(void) const;
		int central_get_bottommost_pixel(void) const;

		/* Get coordinate of pixel (starting with zero) that
		   is in center of central part of viewport pixmap,
		   either at the center of horizontal line (x), or at
		   the center of vertical line (y).
		*/
		fpixel central_get_x_center_pixel(void) const;
		fpixel central_get_y_center_pixel(void) const;
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


		void resizeEvent(QResizeEvent * event);


		/* Drawing primitives. */
		void draw_line(const QPen & pen, fpixel begin_x, fpixel begin_y, fpixel end_x, fpixel end_y);
		void draw_line(const QPen & pen, const ScreenPos & begin, const ScreenPos & end);
		void draw_rectangle(const QPen & pen, fpixel upper_left_x, fpixel upper_left_y, fpixel width, fpixel height);
		void draw_rectangle(const QPen & pen, const QRect & rect);
		void draw_rectangle(const QPen & pen, const QRectF & rect);
		void fill_rectangle(const QColor & color, fpixel x, fpixel y, fpixel width, fpixel height);

		void draw_text(const QFont & font, const QPen & pen, fpixel x, fpixel y, const QString & text);
		QRectF draw_text(const QFont & font, const QPen & pen, const QRectF & bounding_rect, int flags, const QString & text, TextOffset text_offset = TextOffset::None);
		QRectF draw_text(const QFont & text_font, const QPen & pen, const QColor & bg_color, const QRectF & bounding_rect, int flags, const QString & text, TextOffset text_offset = TextOffset::None);
		void draw_outlined_text(const QFont & text_font, const QPen & outline_pen, const QColor & fill_color, const ScreenPos & base_point, const QString & text);
		void offset_text_bounding_rect(QRectF & text_rect, TextOffset text_offset) const;

		void draw_arc(const QPen & pen, fpixel center_x, fpixel center_y, fpixel width, fpixel height, int start_angle, int span_angle);
		void draw_ellipse(const QPen & pen, const ScreenPos & center, fpixel radius_x, fpixel radius_y);
		void fill_ellipse(const QColor & color, const ScreenPos & center, fpixel radius_x, fpixel radius_y);
		void draw_polygon(const QPen & pen, const ScreenPos * points, int npoints, bool filled);

		void draw_pixmap(const QPixmap & pixmap, fpixel viewport_x, fpixel viewport_y, fpixel pixmap_x, fpixel pixmap_y, fpixel pixmap_width, fpixel pixmap_height);
		void draw_pixmap(const QPixmap & pixmap, fpixel viewport_x, fpixel viewport_y);
		void draw_pixmap(const QPixmap & pixmap, const QRect & viewport_rect, const QRect & pixmap_rect);


		void margin_draw_text(ViewportPixmap::MarginPosition pos, const QFont & text_font, const QPen & pen, const QRectF & bounding_rect, int flags, const QString & text, TextOffset text_offset);

		/* Draw a line in central part of viewport.  x/y
		   coordinates should be in "beginning is in
		   bottom-left corner" coordinates system. */
		void central_draw_line(const QPen & pen, fpixel begin_x, fpixel begin_y, fpixel end_x, fpixel end_y);

		/* Draw a crosshair in central part of viewport.  x/y
		   coordinates should be in "beginning is in bottom
		   left corner" coordinates system.

		   "Simple" means one horizontal and one vertical line
		   crossing at given position. */
		void central_draw_simple_crosshair(const ScreenPos & pos);

		/**
		   Draw a rectangle around central area, but only if
		   there is any margin around the central area.

		   The rectangle will be drawn just outside of central
		   area.
		*/
		sg_ret central_draw_outside_boundary_rect(void);


		bool line_is_outside(fpixel begin_x, fpixel begin_y, fpixel end_x, fpixel end_y);


		QPainter & get_painter(void) { return this->painter; }


		void set_highlight_usage(bool new_state);
		bool get_highlight_usage(void) const;

		const QPen & get_highlight_pen(void) const;
		void set_highlight_thickness(int thickness);

		void set_highlight_color(const QString & color_name);
		void set_highlight_color(const QColor & color);
		const QColor & get_highlight_color(void) const;



		/* Color/graphics context management. */
		void set_background_color(const QColor & color);
		void set_background_color(const QString & color_name);
		const QColor & get_background_color(void) const;


		/**
		   Target device in which we want to print pixmap may
		   have different proportions than the pixmap.
		   Calculate size of scaled pixmap so that it:
		   a) fits into and maximally fills target device and,
		   b) keeps proportions of original pixmap

		   @param scale_factor is a scale used to re-scale sizes of margins
		*/
		sg_ret calculate_scaled_sizes(int target_width, int target_height, int & scaled_width, int & scaled_height, double & scale_factor);



		/**
		   @bried Debug function: draw misc elements to Viewport Pixmap for debugging purposes
		*/
		void debug_pixmap_draw(void);

		/**
		   @brief Print out selected parameters of an object
		*/
		virtual void debug_print_info(void) const;


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

		/* Full width/height, i.e. including margins. */
		int total_width = 0;
		int total_height = 0;

		int left_margin_width = 0;
		int right_margin_width = 0;
		int top_margin_height = 0;
		int bottom_margin_height = 0;

		QPen highlight_pen;
		QColor highlight_color;

		QPen central_outside_boundary_rect_pen;

		bool highlight_usage = true;


		QPainter painter;
		QPixmap vpixmap;
		QPixmap vpixmap_snapshot;

	private:
		void draw_text_debug(const QRectF & text_rect);


	signals:
		void size_changed(ViewportPixmap * vpixmap);
		void cursor_moved(ViewportPixmap * vpixmap, QMouseEvent * event);
		void button_released(ViewportPixmap * vpixmap, QMouseEvent * event);

	};
	QDebug operator<<(QDebug debug, const ViewportPixmap & vpixmap);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_VIEWPORT_PIXMAP_H_ */
