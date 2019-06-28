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




namespace SlavGPS {




	class Viewport2D;




	/* Wrapper around pixmap and a painter that paints to the
	   pixmap.  Simple class providing paint primitives. */
	class ViewportPixmap : public QWidget {
		Q_OBJECT

		friend class Viewport;
		friend class Viewport2D;
		friend class ViewportDecorations;
		friend class ViewportMargin;
	public:
		ViewportPixmap(QWidget * parent = NULL);
		~ViewportPixmap();

		void reconfigure(int width, int height);

		int get_leftmost_pixel(void) const;
		int get_rightmost_pixel(void) const;
		int get_topmost_pixel(void) const;
		int get_bottommost_pixel(void) const;

		/* Get number of pixel (starting with zero) that is in
		   center of viewport pixmap, either at the center of
		   vertical line, or at the center of horizontal
		   line. */
		int get_vert_center_pixel(void) const;
		int get_horiz_center_pixel(void) const;

		int get_width(void) const;
		int get_height(void) const;

		void clear(void);



		/* Drawing primitives. */
		void draw_line(QPen const & pen, int begin_x, int begin_y, int end_x, int end_y);
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



		bool line_is_outside(int begin_x, int begin_y, int end_x, int end_y);



		char debug[100] = { 0 };

		/* TODO: move to 'protected'. */
		QPixmap saved_pixmap;
		bool saved_pixmap_valid;


	protected:
		Viewport2D * parent_viewport = NULL;

		int width = 0;
		int height = 0;

		QPainter * painter = NULL;
		QPixmap * pixmap = NULL;
		QPixmap * snapshot_buffer = NULL;


	signals:
		void reconfigured(Viewport2D * viewport);
	};
	QDebug operator<<(QDebug debug, const ViewportPixmap & vpixmap);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_VIEWPORT_PIXMAP_H_ */
