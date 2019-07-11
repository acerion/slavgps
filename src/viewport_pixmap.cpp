/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
 *
 * Multiple UTM zone patch by Kit Transue <notlostyet@didactek.com>
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




#include "viewport_pixmap.h"
#include "viewport_internal.h"
#include "globals.h"




using namespace SlavGPS;




#define SG_MODULE "ViewportPixmap"




ViewportPixmap::ViewportPixmap(QWidget * parent) : QWidget(parent)
{
}




ViewportPixmap::~ViewportPixmap()
{
	/* Painter must be deleted before paint device, otherwise
	   destructor of the paint device will complain. */
	delete this->painter;
	delete this->pixmap;
}




void ViewportPixmap::draw_line(QPen const & pen, const ScreenPos & begin, const ScreenPos & end)
{
	this->draw_line(pen, begin.x, begin.y, end.x, end.y);
}




void ViewportPixmap::draw_line(const QPen & pen, int begin_x, int begin_y, int end_x, int end_y)
{
	//qDebug() << SG_PREFIX_I << "Attempt to draw line between points" << begin_x << begin_y << "and" << end_x << end_y;
	if (this->line_is_outside(begin_x, begin_y, end_x, end_y)) {
		qDebug() << SG_PREFIX_I << "Line" << begin_x << begin_y << end_x << end_y << "is outside of viewport";
		return;
	}

	/*** Clipping, yeah! ***/
	GisViewport::clip_line(&begin_x, &begin_y, &end_x, &end_y);

	//QPainter painter(this->vpixmap.pixmap);
	this->painter->setPen(pen);
	this->painter->drawLine(begin_x, begin_y,
				end_x,   end_y);
}




void ViewportPixmap::draw_rectangle(const QPen & pen, int upper_left_x, int upper_left_y, int rect_width, int rect_height)
{
	/* Using 32 as half the default waypoint image size, so this
	   draws ensures the highlight gets done. */
	const int border = 32;

	/* TODO: review this condition. */
	if (upper_left_x > -border
	    && upper_left_y > -border
	    && upper_left_x < this->get_width() + border
	    && upper_left_y < this->get_height() + border) {

		//QPainter painter(this->vpixmap.pixmap);
		this->painter->setPen(pen);
		this->painter->drawRect(upper_left_x, upper_left_y, rect_width, rect_height);
	}
}




void ViewportPixmap::draw_rectangle(const QPen & pen, const QRect & rect)
{
	/* Using 32 as half the default waypoint image size, so this
	   draws ensures the highlight gets done. */
	const int border = 32;

	/* TODO: review this condition. */
	if (rect.x() > -border
	    && rect.y() > -border
	    && rect.x() < this->get_width() + border
	    && rect.y() < this->get_height() + border) {

		//QPainter painter(this->pixmap);
		this->painter->setPen(pen);
		this->painter->drawRect(rect);
	}
}




void ViewportPixmap::fill_rectangle(const QColor & color, int pos_x, int pos_y, int rect_width, int rect_height)
{
	/* Using 32 as half the default waypoint image size, so this
	   draws ensures the highlight gets done. */
	const int border = 32;

	/* TODO: review this condition. */
	if (pos_x > -border
	    && pos_y > -border
	    && pos_x < this->get_width() + border
	    && pos_y < this->get_height() + border) {

		//QPainter painter(this->pixmap);
		this->painter->fillRect(pos_x, pos_y, rect_width, rect_height, color);
	}
}




void ViewportPixmap::draw_text(QFont const & text_font, QPen const & pen, int pos_x, int pos_y, QString const & text)
{
	const int border = 100;

	/* TODO: review this condition. */
	if (pos_x > -border
	    && pos_y > -border
	    && pos_x < this->get_width() + border
	    && pos_y < this->get_height() + border) {

		//QPainter painter(this->pixmap);
		this->painter->setPen(pen);
		this->painter->setFont(text_font);
		this->painter->drawText(pos_x, pos_y, text);
	}
}




void ViewportPixmap::draw_text(const QFont & text_font, const QPen & pen, const QRectF & bounding_rect, int flags, const QString & text, int text_offset)
{
	//QPainter painter(this->pixmap);
	this->painter->setFont(text_font);

	/* "Normalize" bounding rectangles that have negative width or height.
	   Otherwise the text will be outside of the bounding rectangle. */
	QRectF final_bounding_rect = bounding_rect.united(bounding_rect);

	QRectF text_rect = this->painter->boundingRect(final_bounding_rect, flags, text);
	if (text_offset & SG_TEXT_OFFSET_UP) {
		/* Move boxes a bit up, so that text is right against grid line, not below it. */
		qreal new_top = text_rect.top() - (text_rect.height() / 2);
		final_bounding_rect.moveTop(new_top);
		text_rect.moveTop(new_top);
	}

	if (text_offset & SG_TEXT_OFFSET_LEFT) {
		/* Move boxes a bit left, so that text is right below grid line, not to the right of it. */
		qreal new_left = text_rect.left() - (text_rect.width() / 2);
		final_bounding_rect.moveLeft(new_left);
		text_rect.moveLeft(new_left);
	}


#if 1
	/* Debug. */
	this->painter->setPen(QColor("red"));
	this->painter->drawEllipse(final_bounding_rect.left(), final_bounding_rect.top(), 5, 5);

	this->painter->setPen(QColor("darkgreen"));
	this->painter->drawRect(final_bounding_rect);

	this->painter->setPen(QColor("red"));
	this->painter->drawRect(text_rect);
#endif

	this->painter->setPen(pen);
	this->painter->drawText(text_rect, flags, text, NULL);
}




void ViewportPixmap::draw_outlined_text(QFont const & text_font, QPen const & outline_pen, const QColor & fill_color, const QPointF & base_point, QString const & text)
{
	/* http://doc.qt.io/qt-5/qpainterpath.html#addText */

	this->painter->setPen(outline_pen);
	this->painter->setBrush(QBrush(fill_color));

	QPainterPath path;
	path.addText(base_point, text_font, text);

	this->painter->drawPath(path);

	/* Reset painter. */
	this->painter->setPen(QPen());
	this->painter->setBrush(QBrush());
}




void ViewportPixmap::draw_text(QFont const & text_font, QPen const & pen, const QColor & bg_color, const QRectF & bounding_rect, int flags, const QString & text, int text_offset)
{
	//QPainter painter(this->pixmap);
	this->painter->setFont(text_font);

	/* "Normalize" bounding rectangles that have negative width or height.
	   Otherwise the text will be outside of the bounding rectangle. */
	QRectF final_bounding_rect = bounding_rect.united(bounding_rect);

	QRectF text_rect = this->painter->boundingRect(final_bounding_rect, flags, text);
	if (text_offset & SG_TEXT_OFFSET_UP) {
		/* Move boxes a bit up, so that text is right against grid line, not below it. */
		qreal new_top = text_rect.top() - (text_rect.height() / 2);
		final_bounding_rect.moveTop(new_top);
		text_rect.moveTop(new_top);
	}

	if (text_offset & SG_TEXT_OFFSET_LEFT) {
		/* Move boxes a bit left, so that text is right below grid line, not to the right of it. */
		qreal new_left = text_rect.left() - (text_rect.width() / 2);
		final_bounding_rect.moveLeft(new_left);
		text_rect.moveLeft(new_left);
	}


#if 1
	/* Debug. */
	this->painter->setPen(QColor("red"));
	this->painter->drawEllipse(bounding_rect.left(), bounding_rect.top(), 3, 3);

	this->painter->setPen(QColor("darkgreen"));
	this->painter->drawRect(bounding_rect);
#endif

	/* A highlight of drawn text, must be executed before .drawText(). */
	this->painter->fillRect(text_rect, bg_color);

	this->painter->setPen(pen);
	this->painter->drawText(text_rect, flags, text, NULL);
}




void ViewportPixmap::draw_pixmap(QPixmap const & a_pixmap, int viewport_x, int viewport_y, int pixmap_x, int pixmap_y, int pixmap_width, int pixmap_height)
{
	//QPainter painter(this->pixmap);
	this->painter->drawPixmap(viewport_x, viewport_y, a_pixmap, pixmap_x, pixmap_y, pixmap_width, pixmap_height);
}




void ViewportPixmap::draw_pixmap(QPixmap const & a_pixmap, int viewport_x, int viewport_y)
{
	//QPainter painter(this->pixmap);
	this->painter->drawPixmap(viewport_x, viewport_y, a_pixmap);
}




void ViewportPixmap::draw_pixmap(QPixmap const & a_pixmap, const QRect & viewport_rect, const QRect & pixmap_rect)
{
	//QPainter painter(this->pixmap);
	this->painter->drawPixmap(viewport_rect, a_pixmap, pixmap_rect);
}




void ViewportPixmap::draw_arc(QPen const & pen, int center_x, int center_y, int size_w, int size_h, int start_angle, int span_angle)
{
	this->painter->setPen(pen);
	this->painter->drawArc(center_x, center_y, size_w, size_h, start_angle, span_angle * 16);
}




void ViewportPixmap::draw_ellipse(QPen const & pen, const QPoint & ellipse_center, int radius_x, int radius_y, bool filled)
{
	/* TODO_LATER: see if there is a difference in outer size of ellipse drawn with and without a filling. */

	if (filled) {
		/* Set brush to fill ellipse. */
		this->painter->setBrush(QBrush(pen.color()));
	} else {
		this->painter->setPen(pen);
	}

	this->painter->drawEllipse(ellipse_center, radius_x, radius_y);

	if (filled) {
		/* Reset brush. */
		this->painter->setBrush(QBrush());
	}
}




void ViewportPixmap::draw_polygon(QPen const & pen, QPoint const * points, int npoints, bool filled)
{
	//QPainter painter(this->pixmap);

	if (filled) {
		QPainterPath path;
		path.moveTo(points[0]);
		for (int i = 1; i < npoints; i++) {
			path.lineTo(points[i]);
		}

		this->painter->setPen(Qt::NoPen);
		this->painter->fillPath(path, QBrush(QColor(pen.color())));
	} else {
		this->painter->setPen(pen);
		this->painter->drawPolygon(points, npoints);
	}
}




void ViewportPixmap::reconfigure(int new_width, int new_height)
{
	/* We don't handle situation when size of the pixmap doesn't
	   change.  Such situation will occur rarely, and adding a
	   code path to handle this special case would bring little
	   gain. */

	qDebug() << SG_PREFIX_N << this->debug << "vpixmap is being reconfigured with size" << new_width << new_height;

	if (this->width == new_width && this->height == new_height) {
		qDebug() << SG_PREFIX_I << this->debug << "vpixmap not reconfigured: size didn't change";
		return;
	}

	this->width = new_width;
	this->height = new_height;

	if (this->pixmap) {
		qDebug() << SG_PREFIX_I << this->debug << "Deleting old vpixmap";
		/* Painter must be deleted before paint device, otherwise
		   destructor of the paint device will complain. */
		delete this->painter;
		delete this->pixmap;
	}

	qDebug() << SG_PREFIX_I << this->debug << "Creating new vpixmap with size" << this->width << this->height;
	this->pixmap = new QPixmap(this->width, this->height);
	this->pixmap->fill();

	this->painter = new QPainter(this->pixmap);


	/* TODO_UNKNOWN trigger: only if this is enabled!!! */
	if (this->snapshot_buffer) {
		qDebug() << SG_PREFIX_D << this->debug << "Deleting old snapshot buffer";
		delete this->snapshot_buffer;
	}
	qDebug() << SG_PREFIX_I << this->debug << "Creating new snapshot buffer with size" << this->width << this->height;
	this->snapshot_buffer = new QPixmap(this->width, this->height);

	qDebug() << SG_PREFIX_SIGNAL << this->debug << "Sending \"reconfigured\" signal";
	emit this->reconfigured(this->parent_viewport); /* TODO: maybe emit->this->parent_viewport->some_signal()? */
}




QDebug SlavGPS::operator<<(QDebug debug, const ViewportPixmap & vpixmap)
{
	debug << "ViewportPixmap:" << vpixmap.debug << "width=" << vpixmap.get_width() << "height=" << vpixmap.get_height();
	return debug;
}




int ViewportPixmap::get_leftmost_pixel(void) const
{
	return 0;
}
int ViewportPixmap::get_rightmost_pixel(void) const
{
	return this->width - 1;
}
int ViewportPixmap::get_upmost_pixel(void) const
{
	return 0;
}
int ViewportPixmap::get_bottommost_pixel(void) const
{
	return this->height - 1;
}
int ViewportPixmap::get_vert_center_pixel(void) const
{
	return (this->height - 1) / 2;
}
int ViewportPixmap::get_horiz_center_pixel(void) const
{
	return (this->width - 1) / 2;
}
int ViewportPixmap::get_width(void) const
{
	return this->width;
}
int ViewportPixmap::get_height(void) const
{
	return this->height;
}




ScreenPos ViewportPixmap::get_center_screen_pos(void) const
{
	return ScreenPos(this->get_horiz_center_pixel(), this->get_vert_center_pixel());
}




void ViewportPixmap::clear(void)
{
	this->painter->eraseRect(0, 0, this->get_width(), this->get_height());
}




bool ViewportPixmap::line_is_outside(int begin_x, int begin_y, int end_x, int end_y)
{
	/*
	  Here we follow Qt's coordinate system:
	  begin in top-left corner, pixel numbers increase as we go down and right.
	  "q_" prefix means just that: that values are in Qt's coordinate system.
	*/

	const int leftmost   = this->get_leftmost_pixel();
	const int rightmost  = this->get_rightmost_pixel();
	const int upmost     = this->get_upmost_pixel();
	const int bottommost = this->get_bottommost_pixel();

	if (begin_x < leftmost && end_x < leftmost) {
		/* Line begins and ends on left side of viewport pixmap. */
		//qDebug() << SG_PREFIX_D << "cond 1, leftmost =" << q_leftmost << ", begin_x = " << begin_x << ", end_x =" << end_x;
		return true;
	}
	if (begin_y < upmost && end_y < upmost) {
		/* Line begins and ends above viewport pixmap. */
		//qDebug() << SG_PREFIX_D << "cond 2, upmost =" << q_upmost << ", begin_y = " << begin_y << ", end_y =" << end_y;
		return true;
	}
	if (begin_x > rightmost && end_x > rightmost) {
		/* Line begins and ends on right side of viewport pixmap. */
		//qDebug() << SG_PREFIX_D << "cond 3, rightmost =" << q_rightmost << ", begin_x = " << begin_x << ", end_x =" << end_x;
		return true;
	}
	if (begin_y > bottommost && end_y > bottommost) {
		/* Line begins and ends below viewport pixmap. */
		//qDebug() << SG_PREFIX_D << "cond 4, bottommost =" << q_bottommost << ", begin_y = " << begin_y << ", end_y =" << end_y;
		return true;
	}

	return false;
}
