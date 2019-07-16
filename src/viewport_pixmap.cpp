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

#define DEFAULT_BACKGROUND_COLOR "#CCCCCC"
#define DEFAULT_HIGHLIGHT_COLOR "#EEA500" /* Orange. */





ViewportPixmap::ViewportPixmap(int left, int right, int top, int bottom, QWidget * parent) : QWidget(parent)
{
	this->left_margin_width = left;
	this->right_margin_width = right;
	this->top_margin_height = top;
	this->bottom_margin_height = bottom;

	this->marker_pen.setColor(QColor("brown"));
	this->marker_pen.setWidth(1);

	this->grid_pen.setColor(QColor("dimgray"));
	this->grid_pen.setWidth(1);

	this->labels_pen.setColor("black");
	this->labels_font.setFamily("Helvetica");
	this->labels_font.setPointSize(11);

	this->background_pen.setColor(QString(DEFAULT_BACKGROUND_COLOR));
	this->background_pen.setWidth(1);
	this->set_background_color(QString(DEFAULT_BACKGROUND_COLOR));

	this->highlight_pen.setColor(DEFAULT_HIGHLIGHT_COLOR);
	this->highlight_pen.setWidth(1);
	this->set_highlight_color(QString(DEFAULT_HIGHLIGHT_COLOR));
}




ViewportPixmap::~ViewportPixmap()
{
	/* Call ::end() before deleting paint device, otherwise
	   destructor of the paint device will complain:
	   "QPaintDevice: Cannot destroy paint device that is being
	   painted". */
	this->painter.end();
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

	this->painter.setPen(pen);
	this->painter.drawLine(begin_x, begin_y,
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
	    && upper_left_x < this->central_get_width() + border
	    && upper_left_y < this->central_get_height() + border) {

		this->painter.setPen(pen);
		this->painter.drawRect(upper_left_x, upper_left_y, rect_width, rect_height);
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
	    && rect.x() < this->central_get_width() + border
	    && rect.y() < this->central_get_height() + border) {

		this->painter.setPen(pen);
		this->painter.drawRect(rect);
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
	    && pos_x < this->central_get_width() + border
	    && pos_y < this->central_get_height() + border) {

		this->painter.fillRect(pos_x, pos_y, rect_width, rect_height, color);
	}
}




void ViewportPixmap::draw_text(QFont const & text_font, QPen const & pen, int pos_x, int pos_y, QString const & text)
{
	const int border = 100;

	/* TODO: review this condition. */
	if (pos_x > -border
	    && pos_y > -border
	    && pos_x < this->central_get_width() + border
	    && pos_y < this->central_get_height() + border) {

		this->painter.setPen(pen);
		this->painter.setFont(text_font);
		this->painter.drawText(pos_x, pos_y, text);
	}
}




void ViewportPixmap::draw_text(const QFont & text_font, const QPen & pen, const QRectF & bounding_rect, int flags, const QString & text, int text_offset)
{
	this->painter.setFont(text_font);

	/* "Normalize" bounding rectangles that have negative width or height.
	   Otherwise the text will be outside of the bounding rectangle. */
	QRectF final_bounding_rect = bounding_rect.united(bounding_rect);

	QRectF text_rect = this->painter.boundingRect(final_bounding_rect, flags, text);
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
	this->painter.setPen(QColor("red"));
	this->painter.drawEllipse(final_bounding_rect.left(), final_bounding_rect.top(), 5, 5);

	this->painter.setPen(QColor("darkgreen"));
	this->painter.drawRect(final_bounding_rect);

	this->painter.setPen(QColor("red"));
	this->painter.drawRect(text_rect);
#endif

	this->painter.setPen(pen);
	this->painter.drawText(text_rect, flags, text, NULL);
}




void ViewportPixmap::draw_outlined_text(QFont const & text_font, QPen const & outline_pen, const QColor & fill_color, const QPointF & base_point, QString const & text)
{
	/* http://doc.qt.io/qt-5/qpainterpath.html#addText */

	this->painter.setPen(outline_pen);
	this->painter.setBrush(QBrush(fill_color));

	QPainterPath path;
	path.addText(base_point, text_font, text);

	this->painter.drawPath(path);

	/* Reset painter. */
	this->painter.setPen(QPen());
	this->painter.setBrush(QBrush());
}




void ViewportPixmap::draw_text(QFont const & text_font, QPen const & pen, const QColor & bg_color, const QRectF & bounding_rect, int flags, const QString & text, int text_offset)
{
	this->painter.setFont(text_font);

	/* "Normalize" bounding rectangles that have negative width or height.
	   Otherwise the text will be outside of the bounding rectangle. */
	QRectF final_bounding_rect = bounding_rect.united(bounding_rect);

	QRectF text_rect = this->painter.boundingRect(final_bounding_rect, flags, text);
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
	this->painter.setPen(QColor("red"));
	this->painter.drawEllipse(bounding_rect.left(), bounding_rect.top(), 3, 3);

	this->painter.setPen(QColor("darkgreen"));
	this->painter.drawRect(bounding_rect);
#endif

	/* A highlight of drawn text, must be executed before .drawText(). */
	this->painter.fillRect(text_rect, bg_color);

	this->painter.setPen(pen);
	this->painter.drawText(text_rect, flags, text, NULL);
}




void ViewportPixmap::draw_pixmap(QPixmap const & a_pixmap, int viewport_x, int viewport_y, int pixmap_x, int pixmap_y, int pixmap_width, int pixmap_height)
{
	this->painter.drawPixmap(viewport_x, viewport_y, a_pixmap, pixmap_x, pixmap_y, pixmap_width, pixmap_height);
}




void ViewportPixmap::draw_pixmap(QPixmap const & a_pixmap, int viewport_x, int viewport_y)
{
	this->painter.drawPixmap(viewport_x, viewport_y, a_pixmap);
}




void ViewportPixmap::draw_pixmap(QPixmap const & a_pixmap, const QRect & viewport_rect, const QRect & pixmap_rect)
{
	this->painter.drawPixmap(viewport_rect, a_pixmap, pixmap_rect);
}




void ViewportPixmap::draw_arc(QPen const & pen, int center_x, int center_y, int size_w, int size_h, int start_angle, int span_angle)
{
	this->painter.setPen(pen);
	this->painter.drawArc(center_x, center_y, size_w, size_h, start_angle, span_angle * 16);
}




void ViewportPixmap::draw_ellipse(QPen const & pen, const QPoint & ellipse_center, int radius_x, int radius_y, bool filled)
{
	/* TODO_LATER: see if there is a difference in outer size of ellipse drawn with and without a filling. */

	if (filled) {
		/* Set brush to fill ellipse. */
		this->painter.setBrush(QBrush(pen.color()));
	} else {
		this->painter.setPen(pen);
	}

	this->painter.drawEllipse(ellipse_center, radius_x, radius_y);

	if (filled) {
		/* Reset brush. */
		this->painter.setBrush(QBrush());
	}
}




void ViewportPixmap::draw_polygon(QPen const & pen, QPoint const * points, int npoints, bool filled)
{
	if (filled) {
		QPainterPath path;
		path.moveTo(points[0]);
		for (int i = 1; i < npoints; i++) {
			path.lineTo(points[i]);
		}

		this->painter.setPen(Qt::NoPen);
		this->painter.fillPath(path, QBrush(QColor(pen.color())));
	} else {
		this->painter.setPen(pen);
		this->painter.drawPolygon(points, npoints);
	}
}




void ViewportPixmap::reconfigure(int new_width, int new_height)
{
	/* TODO: where do we reconfigure margin sizes? */

	/* We don't handle situation when size of the pixmap doesn't
	   change.  Such situation will occur rarely, and adding a
	   code path to handle this special case would bring little
	   gain. */

	qDebug() << SG_PREFIX_N << this->debug << "vpixmap is being reconfigured with size" << new_width << new_height;

	if (this->total_width == new_width && this->total_height == new_height) {
		qDebug() << SG_PREFIX_I << this->debug << "vpixmap not reconfigured: size didn't change";
		return;
	}

	this->total_width = new_width;
	this->total_height = new_height;

	if (this->pixmap) {
		qDebug() << SG_PREFIX_I << this->debug << "Deleting old vpixmap";
		/* Call ::end() before deleting paint device,
		   otherwise destructor of the paint device will
		   complain: "QPaintDevice: Cannot destroy paint
		   device that is being painted". */
		this->painter.end();
		delete this->pixmap;
	}

	qDebug() << SG_PREFIX_I << this->debug << "Creating new vpixmap with size" << this->total_width << this->total_height;
	this->pixmap = new QPixmap(this->total_width, this->total_height);
	this->pixmap->fill();

	this->painter.begin(this->pixmap);


	/* TODO_UNKNOWN trigger: only if this is enabled!!! */
	if (this->snapshot_buffer) {
		qDebug() << SG_PREFIX_D << this->debug << "Deleting old snapshot buffer";
		delete this->snapshot_buffer;
	}
	qDebug() << SG_PREFIX_I << this->debug << "Creating new snapshot buffer with size" << this->total_width << this->total_height;
	this->snapshot_buffer = new QPixmap(this->total_width, this->total_height);

	qDebug() << SG_PREFIX_SIGNAL << this->debug << "Sending \"reconfigured\" signal";
	emit this->reconfigured(this);
}




QDebug SlavGPS::operator<<(QDebug debug, const ViewportPixmap & vpixmap)
{
	debug << "ViewportPixmap:" << vpixmap.debug << "central width=" << vpixmap.central_get_width() << "central height=" << vpixmap.central_get_height();
	return debug;
}




/**
   @reviewed-on 2019-07-15
*/
int ViewportPixmap::central_get_leftmost_pixel(void) const
{
	return this->left_margin_width;
}

/**
   @reviewed-on 2019-07-15
*/
int ViewportPixmap::central_get_rightmost_pixel(void) const
{
	return this->total_width - this->right_margin_width - 1;
}

/**
   @reviewed-on 2019-07-15
*/
int ViewportPixmap::central_get_topmost_pixel(void) const
{
	return this->top_margin_height;
}

/**
   @reviewed-on 2019-07-15
*/
int ViewportPixmap::central_get_bottommost_pixel(void) const
{
	return this->total_height - this->bottom_margin_height - 1;
}

/**
   @reviewed-on 2019-07-15
*/
int ViewportPixmap::central_get_x_center_pixel(void) const
{
	/* If left and right margins were zero, this would be
	   horizontal position of center pixel. */
	const int x_center_without_margins = (this->total_width - this->left_margin_width - this->right_margin_width - 1) / 2;

	/* Move the pixel's position left by how much place takes the
	   left margin. */
	const int x_center_shifted_by_left_margin = x_center_without_margins + this->left_margin_width;

	return x_center_shifted_by_left_margin;
}

/**
   @reviewed-on 2019-07-15
*/
int ViewportPixmap::central_get_y_center_pixel(void) const
{
	/* If top and bottom margins were zero, this would be vertical
	   position of center pixel. */
	const int y_center_without_margins = (this->total_height - this->top_margin_height - this->bottom_margin_height - 1) / 2;

	/* Move the pixel's position down by how much place takes the
	   top margin. */
	const int y_center_shifted_by_top_margin = y_center_without_margins + this->top_margin_height;

	return y_center_shifted_by_top_margin;
}

/**
   @reviewed-on 2019-07-15
*/
int ViewportPixmap::total_get_width(void) const
{
	return this->total_width;
}

/**
   @reviewed-on 2019-07-15
*/
int ViewportPixmap::total_get_height(void) const
{
	return this->total_height;
}

/**
   @reviewed-on 2019-07-14
*/
int ViewportPixmap::central_get_width(void) const
{
	return this->total_width - this->left_margin_width - this->right_margin_width;
}

/**
   @reviewed-on 2019-07-14
*/
int ViewportPixmap::central_get_height(void) const
{
	return this->total_height - this->top_margin_height - this->bottom_margin_height;
}

/**
   @reviewed-on 2019-07-14
*/
int ViewportPixmap::left_get_width(void) const
{
	return this->left_margin_width;
}
int ViewportPixmap::left_get_height(void) const
{
	return this->total_height;
}

/**
   @reviewed-on 2019-07-14
*/
int ViewportPixmap::right_get_width(void) const
{
	return this->right_margin_width;
}

int ViewportPixmap::right_get_height(void) const
{
	return this->total_height;
}

int ViewportPixmap::top_get_width(void) const
{
	return this->total_width;
}

/**
   @reviewed-on 2019-07-14
*/
int ViewportPixmap::top_get_height(void) const
{
	return this->top_margin_height;
}

int ViewportPixmap::bottom_get_width(void) const
{
	return this->total_width;
}

/**
   @reviewed-on 2019-07-14
*/
int ViewportPixmap::bottom_get_height(void) const
{
	return this->bottom_margin_height;
}




/**
   @reviewed-on 2019-07-15
*/
ScreenPos ViewportPixmap::central_get_center_screen_pos(void) const
{
	return ScreenPos(this->central_get_x_center_pixel(), this->central_get_y_center_pixel());
}




void ViewportPixmap::clear(void)
{
	this->painter.eraseRect(0, 0, this->total_width, this->total_height); /* TODO: use Qt's method that clears whole widget? */
}




bool ViewportPixmap::line_is_outside(int begin_x, int begin_y, int end_x, int end_y)
{
	/*
	  Here we follow Qt's coordinate system:
	  begin in top-left corner, pixel numbers increase as we go down and right.
	  "q_" prefix means just that: that values are in Qt's coordinate system.
	*/

	const int leftmost   = this->central_get_leftmost_pixel();
	const int rightmost  = this->central_get_rightmost_pixel();
	const int topmost    = this->central_get_topmost_pixel();
	const int bottommost = this->central_get_bottommost_pixel();

	if (begin_x < leftmost && end_x < leftmost) {
		/* Line begins and ends on left side of viewport pixmap. */
		//qDebug() << SG_PREFIX_D << "cond 1, leftmost =" << leftmost << ", begin_x = " << begin_x << ", end_x =" << end_x;
		return true;
	}
	if (begin_y < topmost && end_y < topmost) {
		/* Line begins and ends above viewport pixmap. */
		//qDebug() << SG_PREFIX_D << "cond 2, topmost =" << topmost << ", begin_y = " << begin_y << ", end_y =" << end_y;
		return true;
	}
	if (begin_x > rightmost && end_x > rightmost) {
		/* Line begins and ends on right side of viewport pixmap. */
		//qDebug() << SG_PREFIX_D << "cond 3, rightmost =" << rightmost << ", begin_x = " << begin_x << ", end_x =" << end_x;
		return true;
	}
	if (begin_y > bottommost && end_y > bottommost) {
		/* Line begins and ends below viewport pixmap. */
		//qDebug() << SG_PREFIX_D << "cond 4, bottommost =" << bottommost << ", begin_y = " << begin_y << ", end_y =" << end_y;
		return true;
	}

	return false;
}




void ViewportPixmap::margin_draw_text(ViewportPixmap::MarginPosition pos, const QFont & text_font, const QPen & pen, const QRectF & bounding_rect, int flags, const QString & text, int text_offset)
{
#ifdef K_TODO_RESTORE
	qDebug() << SG_PREFIX_I << "Will draw label" << text;
	ViewportPixmap * vpixmap = NULL;

	switch (pos) {
	case ViewportPixmap::MarginPosition::Left:
		vpixmap = this->left;
		break;
	case ViewportPixmap::MarginPosition::Right:
		vpixmap = this->right;
		break;
	case ViewportPixmap::MarginPosition::Top:
		vpixmap = this->top;
		break;
	case ViewportPixmap::MarginPosition::Bottom:
		vpixmap = this->bottom;
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled margin position";
		break;
	}

	if (!vpixmap) {
		qDebug() << SG_PREFIX_E << "No margin vpixmap selected";
		return;
	}


	vpixmap->painter.setFont(text_font);

	/* "Normalize" bounding rectangles that have negative width or height.
	   Otherwise the text will be outside of the bounding rectangle. */
	QRectF final_bounding_rect = bounding_rect.united(bounding_rect);

	QRectF text_rect = vpixmap->painter.boundingRect(final_bounding_rect, flags, text);
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
	vpixmap->painter.setPen(QColor("red"));
	vpixmap->painter.drawEllipse(bounding_rect.left(), bounding_rect.top(), 5, 5);

	vpixmap->painter.setPen(QColor("darkgreen"));
	vpixmap->painter.drawRect(bounding_rect);

	vpixmap->painter.setPen(QColor("red"));
	vpixmap->painter.drawRect(text_rect);
#endif

	vpixmap->painter.setPen(pen);
	vpixmap->painter.drawText(text_rect, flags, text, NULL);

#endif
}




void ViewportPixmap::central_draw_line(const QPen & pen, int begin_x, int begin_y, int end_x, int end_y)
{
	//qDebug() << SG_PREFIX_I << "Attempt to draw line between points" << begin_x << begin_y << "and" << end_x << end_y;
	if (this->line_is_outside(begin_x, begin_y, end_x, end_y)) {
		qDebug() << SG_PREFIX_I << "Line" << begin_x << begin_y << end_x << end_y << "is outside of viewport";
		return;
	}

	/*** Clipping, yeah! ***/
	//GisViewport::clip_line(&begin_x, &begin_y, &end_x, &end_y);

	const int bottommost_pixel = this->central_get_bottommost_pixel();

	/* x/y coordinates are converted here from "beginning in
	   bottom-left corner" to Qt's "beginning in top-left corner"
	   coordinate system. */
	this->painter.setPen(pen);
	this->painter.drawLine(begin_x, bottommost_pixel - begin_y,
			       end_x,   bottommost_pixel - end_y);
}




/*
  @pos is a position in viewport's pixmap.
  The function makes sure that the crosshair is drawn only inside of graph area.
*/
void ViewportPixmap::central_draw_simple_crosshair(const ScreenPos & pos)
{
	const int rigthmost_pixel = this->central_get_rightmost_pixel();
	const int bottommost_pixel = this->central_get_bottommost_pixel();

	/* Convert from "beginning in bottom-left corner" to Qt's
	   "beginning in top-left corner" coordinate system. "q_"
	   prefix means "Qt's coordinate system". */
	const int x = pos.x;
	const int y = bottommost_pixel - pos.y;

	qDebug() << SG_PREFIX_I << "Crosshair at coord" << x << y;

	if (x > rigthmost_pixel || y > bottommost_pixel) {
		/* Position outside of graph area. */
		return;
	}

	this->painter.setPen(this->marker_pen);

	/* Small optimization: use QT's drawing primitives directly.
	   Remember that (0,0) screen position is in upper-left corner of viewport. */

	this->painter.drawLine(0, y, rigthmost_pixel, y); /* Horizontal line. */
	this->painter.drawLine(x, 0, x, bottommost_pixel); /* Vertical line. */
}




void ViewportPixmap::set_background_color(const QString & color_name)
{
	this->background_color.setNamedColor(color_name);
	this->background_pen.setColor(this->background_color);
}




void ViewportPixmap::set_background_color(const QColor & color)
{
	this->background_color = color;
	this->background_pen.setColor(color);
}




const QColor & ViewportPixmap::get_background_color(void) const
{
	return this->background_color;
}




void ViewportPixmap::set_highlight_usage(bool new_state)
{
	this->highlight_usage = new_state;
}




bool ViewportPixmap::get_highlight_usage(void) const
{
	return this->highlight_usage;
}




const QColor & ViewportPixmap::get_highlight_color(void) const
{
	return this->highlight_color;
}




void ViewportPixmap::set_highlight_color(const QString & color_name)
{
	this->highlight_color.setNamedColor(color_name);
	this->highlight_pen.setColor(this->highlight_color);
}




void ViewportPixmap::set_highlight_color(const QColor & color)
{
	this->highlight_color = color;
	this->highlight_pen.setColor(color);
}




QPen ViewportPixmap::get_highlight_pen(void) const
{
	return this->highlight_pen;
}




void ViewportPixmap::set_highlight_thickness(int w)
{
	this->highlight_pen.setWidth(w);

	this->highlight_pen.setCapStyle(Qt::RoundCap);
	this->highlight_pen.setJoinStyle(Qt::RoundJoin);

#if 0   /* This line style is used by default. */
	this->highlight_pen.setStyle(Qt::SolidLine);
#endif
}




QRect ViewportPixmap::central_get_rect(void) const
{
	const int begin_x = this->central_get_leftmost_pixel();
	const int begin_y = this->central_get_topmost_pixel();

	return QRect(begin_x, begin_y, begin_x + this->central_get_width(), begin_y + this->central_get_height());
}




void ViewportPixmap::paintEvent(QPaintEvent * ev)
{
	qDebug() << SG_PREFIX_I;

	QPainter event_painter(this);
	event_painter.drawPixmap(0, 0, *this->pixmap);

	return;
}




QPixmap ViewportPixmap::get_pixmap(void) const
{
	return *this->pixmap;
}




void ViewportPixmap::set_pixmap(const QPixmap & new_pixmap)
{
	if (this->pixmap->size() != new_pixmap.size()) {
		qDebug() << SG_PREFIX_E << "Pixmap size mismatch: existing pixmap =" << this->pixmap->size() << ", new pixmap =" << new_pixmap.size();
	} else {
		this->painter.drawPixmap(0, 0, new_pixmap, 0, 0, 0, 0);
	}
}





void ViewportPixmap::reconfigure_drawing_area(int new_width, int new_height)
{
	if (new_width == 0 && new_height == 0) {
		qDebug() << SG_PREFIX_I << "Will reconfigure viewport with geometry sizes" << this->geometry().width() << this->geometry().height();
		this->reconfigure(this->geometry().width(), this->geometry().height());
	} else {
		qDebug() << SG_PREFIX_I << "Will reconfigure viewport with specified sizes" << new_width << new_height;
		this->reconfigure(new_width, new_height);
	}
}




void ViewportPixmap::sync(void)
{
	qDebug() << SG_PREFIX_I << "sync (will call ->render())";
	this->render(this->pixmap);
}




void ViewportPixmap::pan_sync(int x_off, int y_off)
{
	qDebug() << SG_PREFIX_I;
#ifdef K_FIXME_RESTORE
	int x, y, wid, hei;

	gdk_draw_drawable(gtk_widget_get_window(GTK_WIDGET(this)), gtk_widget_get_style(GTK_WIDGET(this))->bg_gc[0], GDK_DRAWABLE(this->pixmap), 0, 0, x_off, y_off, this->width, this->vpixmap.height);

	if (x_off >= 0) {
		x = 0;
		wid = x_off;
	} else {
		x = this->width + x_off;
		wid = -x_off;
	}

	if (y_off >= 0) {
		y = 0;
		hei = y_off;
	} else {
		y = this->height + y_off;
		hei = -y_off;
	}
	gtk_widget_queue_draw_area(GTK_WIDGET(this), x, 0, wid, this->height);
	gtk_widget_queue_draw_area(GTK_WIDGET(this), 0, y, this->width, hei);
#endif
}





/* Clip functions continually reduce the value by a factor until it is in the acceptable range
   whilst also scaling the other coordinate value. */
static void clip_x(int * x1, int * y1, int * x2, int * y2)
{
	while (std::abs(*x1) > 32768) {
		*x1 = *x2 + (0.5 * (*x1 - *x2));
		*y1 = *y2 + (0.5 * (*y1 - *y2));
	}
}




static void clip_y(int * x1, int * y1, int * x2, int * y2)
{
	while (std::abs(*y1) > 32767) {
		*x1 = *x2 + (0.5 * (*x1 - *x2));
		*y1 = *y2 + (0.5 * (*y1 - *y2));
	}
}




/**
 * @x1: screen coord
 * @y1: screen coord
 * @x2: screen coord
 * @y2: screen coord
 *
 * Due to the seemingly undocumented behaviour of gdk_draw_line(), we need to restrict the range of values passed in.
 * So despite it accepting ints, the effective range seems to be the actually the minimum C int range (2^16).
 * This seems to be limitations coming from the X Window System.
 *
 * See http://www.rahul.net/kenton/40errs.html
 * ERROR 7. Boundary conditions.
 * "The X coordinate space is not infinite.
 *  Most drawing functions limit position, width, and height to 16 bit integers (sometimes signed, sometimes unsigned) of accuracy.
 *  Because most C compilers use 32 bit integers, Xlib will not complain if you exceed the 16 bit limit, but your results will usually not be what you expected.
 *  You should be especially careful of this if you are implementing higher level scalable graphics packages."
 *
 * This function should be called before calling gdk_draw_line().
 */
void ViewportPixmap::clip_line(int * x1, int * y1, int * x2, int * y2)
{
	if (*x1 > 32768 || *x1 < -32767) {
		clip_x(x1, y1, x2, y2);
	}

	if (*y1 > 32768 || *y1 < -32767) {
		clip_y(x1, y1, x2, y2);
	}

	if (*x2 > 32768 || *x2 < -32767) {
		clip_x(x2, y2, x1, y1);
	}

	if (*y2 > 32768 || *y2 < -32767) {
		clip_y(x2, y2, x1, y1);
	}
}



void ViewportPixmap::snapshot_save(void)
{
	qDebug() << SG_PREFIX_I << "Save snapshot";
	*this->snapshot_buffer = *this->pixmap;
}




void ViewportPixmap::snapshot_load(void)
{
	qDebug() << SG_PREFIX_I << "Load snapshot";
	*this->pixmap = *this->snapshot_buffer;
}




bool ViewportPixmap::is_ready(void) const
{
	return this->pixmap != NULL;
}
