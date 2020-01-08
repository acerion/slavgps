/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2019, Kamil Ignacak <acerion@wp.pl>
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





/**
   @reviewed-on tbd
*/
ViewportPixmap::ViewportPixmap(int left, int right, int top, int bottom, QWidget * parent) : QWidget(parent)
{
	this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

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


	/* Use non-alarming color. Red color should be reserved for error indications. */
	this->central_outside_boundary_rect_pen.setColor("gray");
	this->central_outside_boundary_rect_pen.setWidth(1);

	/* A valid (non-null) initial pixmap for painter, otherwise
	   the painter will complain to console. */
	this->vpixmap = QPixmap(10, 10);
	/* ::apply_total_sizes() first calls painter.end(), so in
	   constructor we have to start with painter.begin(). */
	this->painter.begin(&this->vpixmap);

	/* We want to constantly update cursor position in status bar
	   and/or draw crosshair. For this we need cursor tracking in
	   viewport. */
	this->setMouseTracking(true);
}




/**
   @reviewed-on tbd
*/
ViewportPixmap::~ViewportPixmap()
{
	/* Call ::end() before deleting paint device
	   (ViewportPixmap::pixmap), otherwise destructor of the paint
	   device will complain: "QPaintDevice: Cannot destroy paint
	   device that is being painted". */
	this->painter.end();
}




/**
   @reviewed-on tbd
*/
void ViewportPixmap::draw_line(QPen const & pen, const ScreenPos & begin, const ScreenPos & end)
{
	this->draw_line(pen, begin.x(), begin.y(), end.x(), end.y());
}




/**
   @reviewed-on tbd
*/
void ViewportPixmap::draw_line(const QPen & pen, fpixel begin_x, fpixel begin_y, fpixel end_x, fpixel end_y)
{
	//qDebug() << SG_PREFIX_I << "Attempt to draw line between points" << begin_x << begin_y << "and" << end_x << end_y;
	if (this->line_is_outside(begin_x, begin_y, end_x, end_y)) {
		//qDebug() << SG_PREFIX_I << "Line" << begin_x << begin_y << end_x << end_y << "is outside of viewport";
		return;
	}

	/*** Clipping, yeah! ***/
	GisViewport::clip_line(&begin_x, &begin_y, &end_x, &end_y);

	this->painter.setPen(pen);
	this->painter.drawLine(begin_x, begin_y, end_x, end_y);
}




/**
   @reviewed-on 2019-07-19
*/
void ViewportPixmap::draw_rectangle(const QPen & pen, fpixel upper_left_x, fpixel upper_left_y, fpixel rect_width, fpixel rect_height)
{
	this->painter.setPen(pen);
	this->painter.drawRect(upper_left_x, upper_left_y, rect_width, rect_height);
}




/**
   @reviewed-on 2019-07-19
*/
void ViewportPixmap::draw_rectangle(const QPen & pen, const QRect & rect)
{
	this->painter.setPen(pen);
	this->painter.drawRect(rect);
}




/**
   @reviewed-on 2019-07-21
*/
void ViewportPixmap::draw_rectangle(const QPen & pen, const QRectF & rect)
{
	this->painter.setPen(pen);
	this->painter.drawRect(rect);
}




/**
   @reviewed-on 2019-07-19
*/
void ViewportPixmap::fill_rectangle(const QColor & color, fpixel pos_x, fpixel pos_y, fpixel rect_width, fpixel rect_height)
{
	this->painter.fillRect(pos_x, pos_y, rect_width, rect_height, color);
}




/**
   @reviewed-on 2019-07-19
*/
void ViewportPixmap::draw_text(QFont const & text_font, QPen const & pen, fpixel pos_x, fpixel pos_y, QString const & text)
{
	this->painter.setPen(pen);
	this->painter.setFont(text_font);
	this->painter.drawText(pos_x, pos_y, text);
}




/**
   @reviewed-on tbd
*/
QRectF ViewportPixmap::draw_text(const QFont & text_font, const QPen & pen, const QRectF & bounding_rect, int flags, const QString & text, TextOffset text_offset)
{
	this->painter.setFont(text_font);
	this->painter.setPen(pen);

	/* Get bounding rect of text drawn with font set above with .setFont(). */
	QRectF text_rect = this->painter.boundingRect(bounding_rect, flags, text);

	if (TextOffset::None != text_offset) {
		this->offset_text_bounding_rect(text_rect, text_offset);
	}

	this->painter.drawText(text_rect, flags, text, NULL);

	if (1) { /* Debug. */
		this->draw_text_debug(text_rect);
	}

	return text_rect;
}




/**
   @reviewed-on tbd
*/
void ViewportPixmap::draw_outlined_text(QFont const & text_font, QPen const & outline_pen, const QColor & fill_color, const ScreenPos & base_point, QString const & text)
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




/**
   @reviewed-on tbd
*/
QRectF ViewportPixmap::draw_text(QFont const & text_font, QPen const & pen, const QColor & bg_color, const QRectF & bounding_rect, int flags, const QString & text, TextOffset text_offset)
{
	this->painter.setFont(text_font);

	/* Get bounding rect of text drawn with font set above with .setFont(). */
	QRectF text_rect = this->painter.boundingRect(bounding_rect, flags, text);

	if (TextOffset::None != text_offset) {
		this->offset_text_bounding_rect(text_rect, text_offset);
	}

	/* A highlight of drawn text, must be executed before .drawText(). */
	this->painter.fillRect(text_rect, bg_color);

	this->painter.setPen(pen);
	this->painter.drawText(text_rect, flags, text, NULL);


	if (1) { /* Debug. */
		this->draw_text_debug(text_rect);
	}

	return text_rect;
}




/**
   @reviewed-on tbd
*/
void ViewportPixmap::draw_text_debug(const QRectF & text_rect)
{
	this->painter.setPen(QColor("red"));

	this->painter.drawEllipse(QPoint(text_rect.left(), text_rect.top()), 6, 6);
	this->painter.drawRect(text_rect);
}




/**
   @reviewed-on tbd
*/
void ViewportPixmap::offset_text_bounding_rect(QRectF & text_rect, TextOffset text_offset) const
{
	if ((uint8_t) text_offset & (uint8_t) TextOffset::Up) {
		/* Move box up. */
		const fpixel new_top = text_rect.top() - (text_rect.height() / 2);
		text_rect.moveTop(new_top);
	}

	if ((uint8_t) text_offset & (uint8_t) TextOffset::Left) {
		/* Move box to the left. */
		const fpixel new_left = text_rect.left() - (text_rect.width() / 2);
		text_rect.moveLeft(new_left);
	}
}




/**
   @reviewed-on tbd
*/
void ViewportPixmap::draw_pixmap(QPixmap const & pixmap, fpixel viewport_x, fpixel viewport_y, fpixel pixmap_x, fpixel pixmap_y, fpixel pixmap_width, fpixel pixmap_height)
{
	this->painter.drawPixmap(viewport_x, viewport_y, pixmap, pixmap_x, pixmap_y, pixmap_width, pixmap_height);
}




/**
   @reviewed-on tbd
*/
void ViewportPixmap::draw_pixmap(QPixmap const & pixmap, fpixel viewport_x, fpixel viewport_y)
{
	this->painter.drawPixmap(viewport_x, viewport_y, pixmap);
}




/**
   @reviewed-on tbd
*/
void ViewportPixmap::draw_pixmap(QPixmap const & pixmap, const QRect & viewport_rect, const QRect & pixmap_rect)
{
	this->painter.drawPixmap(viewport_rect, pixmap, pixmap_rect);
}




/**
   @reviewed-on tbd
*/
void ViewportPixmap::draw_arc(QPen const & pen, fpixel center_x, fpixel center_y, fpixel size_w, fpixel size_h, int start_angle, int span_angle)
{
	this->painter.setPen(pen);
	this->painter.drawArc(center_x, center_y, size_w, size_h, start_angle, span_angle * 16);
}




/**
   @reviewed-on 2019-07-19
*/
void ViewportPixmap::draw_ellipse(QPen const & pen, const ScreenPos & ellipse_center, fpixel radius_x, fpixel radius_y)
{
	/* If pen has width 1, draw_ellipse() and fill_ellipse() will
	   draw ellipsis with the same outer size. */
	this->painter.setPen(pen);
	this->painter.drawEllipse(ellipse_center, radius_x, radius_y);
}




/**
   @reviewed-on 2019-07-19
*/
void ViewportPixmap::fill_ellipse(const QColor & color, const ScreenPos & ellipse_center, fpixel radius_x, fpixel radius_y)
{
	/* If pen has width 1, draw_ellipse() and fill_ellipse() will
	   draw ellipsis with the same outer size. */
	QPen pen(color);
	pen.setWidth(1);

	this->painter.setBrush(QBrush(color));
	this->painter.setPen(pen);
	this->painter.drawEllipse(ellipse_center, radius_x, radius_y);
	this->painter.setBrush(QBrush()); /* Reset brush. */
}




/**
   @reviewed-on tbd
*/
void ViewportPixmap::draw_polygon(QPen const & pen, ScreenPos const * points, int npoints, bool filled)
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




/**
   @reviewed-on tbd
*/
void ViewportPixmap::apply_total_sizes(int new_total_width, int new_total_height)
{
	/*
	  Regenerate new pixmap members with given total sizes. Notice
	  that margin sizes are not affected - they are constant. But
	  if total size changes and margins stay the same, then
	  central area also changes (in the same direction).
	*/

	qDebug() << SG_PREFIX_N << "Attempting to apply new total sizes to" << this->debug << ": width =" << new_total_width << ", height =" << new_total_height;

	if (this->total_width == new_total_width && this->total_height == new_total_height) {
		qDebug() << SG_PREFIX_I << "Not applying new total sizes to" << this->debug << ": size didn't change";
		return;
	}

	this->total_width = new_total_width;
	this->total_height = new_total_height;

	qDebug() << SG_PREFIX_I << this->debug << "Will regenerate pixmaps with total width =" << this->total_width << ", total height =" << this->total_height;
	/*
	  Call ::end() before deleting paint device
	  (ViewportPixmap::pixmap), otherwise destructor of the paint
	  device will complain: "QPaintDevice: Cannot destroy paint
	  device that is being painted".

	  This comment may not be applicable because we don't really
	  delete the pixmap (it is no longer a pointer variable), but
	  we still need to call .end().
	*/
	this->painter.end();
	this->vpixmap = QPixmap(this->total_width, this->total_height); /* Reset pixmap with new size */
	this->vpixmap.fill();
	this->painter.begin(&this->vpixmap);


	qDebug() << SG_PREFIX_I << this->debug << "Will regenerate snapshot buffer with size" << this->total_width << this->total_height;
	/* TODO_LATER trigger: only if this is enabled!!! */
	this->vpixmap_snapshot = QPixmap(this->total_width, this->total_height); /* Reset snapshot buffer with new size */


	qDebug() << SG_PREFIX_SIGNAL << this->debug << "Sending \"size changed\" signal";
	emit this->size_changed(this);
}




/**
   @reviewed-on tbd
*/
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
   @reviewed-on 2019-07-21
*/
int ViewportPixmap::central_get_rightmost_pixel(void) const
{
	/*
	  The following code:
	      QRectF rect(0, 0, 10, 11);
	      qDebug() << "left =" << rect.left() << ", right =" << rect.right() << ", top =" << rect.top() << ", bottom =" << rect.bottom();
	  will print
	      left = 0 , right = 10 , top = 0 , bottom = 11
	  This code is written in a way that follows this logic.
	*/
	return this->total_width - this->right_margin_width;
}




/**
   @reviewed-on 2019-07-15
*/
int ViewportPixmap::central_get_topmost_pixel(void) const
{
	return this->top_margin_height;
}




/**
   @reviewed-on 2019-07-21
*/
int ViewportPixmap::central_get_bottommost_pixel(void) const
{
	/*
	  The following code:
	      QRectF rect(0, 0, 10, 11);
	      qDebug() << "left =" << rect.left() << ", right =" << rect.right() << ", top =" << rect.top() << ", bottom =" << rect.bottom();
	  will print
	      left = 0 , right = 10 , top = 0 , bottom = 11
	  This code is written in a way that follows this logic.
	*/
	return this->total_height - this->bottom_margin_height;
}




/**
   @reviewed-on 2019-07-17
*/
fpixel ViewportPixmap::central_get_x_center_pixel(void) const
{
	/* If left and right margins were zero, this would be
	   horizontal position of center pixel. */
	/* To understand how this calculation work, just draw 4-by-5
	   rectangle in grid notebook, mark indices of pixels on x and
	   y axis (starting with zero index) and mark center position
	   in the rectangle. Then look at corresponding indices of the
	   mark on x and y axis and try to derive formula from
	   that. */
	const fpixel x_center_without_margins = (this->total_width - this->left_margin_width - this->right_margin_width) / 2.0;

	/* Move the pixel's position left by how much place the left
	   margin takes. */
	const fpixel x_center_shifted_by_left_margin = x_center_without_margins + this->left_margin_width;

	return x_center_shifted_by_left_margin;
}

/**
   @reviewed-on 2019-07-17
*/
fpixel ViewportPixmap::central_get_y_center_pixel(void) const
{
	/* If top and bottom margins were zero, this would be vertical
	   position of center pixel. */
	/* To understand how this calculation work, just draw 4-by-5
	   rectangle in grid notebook, mark indices of pixels on x and
	   y axis (starting with zero index) and mark center position
	   in the rectangle. Then look at corresponding indices of the
	   mark on x and y axis and try to derive formula from
	   that. */
	const fpixel y_center_without_margins = (this->total_height - this->top_margin_height - this->bottom_margin_height) / 2.0;

	/* Move the pixel's position down by how much place the top
	   margin takes. */
	const fpixel y_center_shifted_by_top_margin = y_center_without_margins + this->top_margin_height;

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




int ViewportPixmap::central_get_n_columns(void) const
{
	return this->central_get_rightmost_pixel() - this->central_get_leftmost_pixel() + 1;
}




int ViewportPixmap::central_get_n_rows(void) const
{
	return this->central_get_bottommost_pixel() - this->central_get_topmost_pixel() + 1;
}




/**
   @reviewed-on 2019-07-14
*/
int ViewportPixmap::left_get_width(void) const
{
	return this->left_margin_width;
}



/**
   @reviewed-on tbd
*/
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




/**
   @reviewed-on tbd
*/
int ViewportPixmap::right_get_height(void) const
{
	return this->total_height;
}




/**
   @reviewed-on tbd
*/
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




/**
   @reviewed-on tbd
*/
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




/**
   @reviewed-on 2019-12-07
*/
void ViewportPixmap::clear(void)
{
	/* This clears ViewportPixmap::vpixmap. */
	this->painter.eraseRect(0, 0, this->total_width, this->total_height);

	/* This clears ViewportPixmap::vpixmap_snapshot */
	this->vpixmap_snapshot = this->vpixmap;
}




/**
   @reviewed-on tbd
*/
bool ViewportPixmap::line_is_outside(fpixel begin_x, fpixel begin_y, fpixel end_x, fpixel end_y)
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




/**
   @reviewed-on tbd
*/
void ViewportPixmap::central_draw_line(const QPen & pen, fpixel begin_x, fpixel begin_y, fpixel end_x, fpixel end_y)
{
	//qDebug() << SG_PREFIX_I << "Attempt to draw line between points" << begin_x << begin_y << "and" << end_x << end_y;
	if (this->line_is_outside(begin_x, begin_y, end_x, end_y)) {
		qDebug() << SG_PREFIX_I << "Line" << begin_x << begin_y << end_x << end_y << "is outside of viewport";
		return;
	}

	/*** Clipping, yeah! ***/
	//GisViewport::clip_line(&begin_x, &begin_y, &end_x, &end_y);

	/* x/y coordinates are converted here from "beginning in
	   bottom-left corner" to Qt's "beginning in top-left corner"
	   coordinate system. */
	this->painter.setPen(pen);
	this->painter.drawLine(begin_x, begin_y,
			       end_x,   end_y);
}




/*
  @pos is a position in viewport's pixmap.
  The function makes sure that the crosshair is drawn only inside of graph area.

  @reviewed-on tbd
*/
void ViewportPixmap::central_draw_simple_crosshair(const Crosshair2D & crosshair)
{
	const int leftmost_px = this->central_get_leftmost_pixel();
	const int rigthmost_px = this->central_get_rightmost_pixel();
	const int topmost_px = this->central_get_topmost_pixel();
	const int bottommost_px = this->central_get_bottommost_pixel();

	if (!crosshair.valid) {
		qDebug() << SG_PREFIX_E << "Crosshair" << crosshair.debug << "is invalid";
		/* Position outside of graph area. */
		return;
	}

	//qDebug() << SG_PREFIX_I << "Crosshair" << crosshair.debug << "at coord" << crosshair.x_px << crosshair.y_px << "(central cbl =" << crosshair.central_cbl_x_px << crosshair.central_cbl_y_px << ")";

	if (crosshair.x_px > rigthmost_px || crosshair.x_px < leftmost_px) {
		qDebug() << SG_PREFIX_E << "Crosshair has bad x";
		/* Position outside of graph area. */
		return;
	}
	if (crosshair.y_px > bottommost_px || crosshair.y_px < topmost_px) {
		qDebug() << SG_PREFIX_E << "Crosshair has bad y";
		/* Position outside of graph area. */
		return;
	}

	this->painter.setPen(this->marker_pen);

	this->painter.drawLine(leftmost_px, crosshair.y_px, rigthmost_px, crosshair.y_px); /* Horizontal line. */
	this->painter.drawLine(crosshair.x_px, topmost_px, crosshair.x_px, bottommost_px); /* Vertical line. */
}





/**
   @reviewed-on tbd
*/
void ViewportPixmap::set_background_color(const QString & color_name)
{
	this->background_color.setNamedColor(color_name);
	this->background_pen.setColor(this->background_color);
}




/**
   @reviewed-on tbd
*/
void ViewportPixmap::set_background_color(const QColor & color)
{
	this->background_color = color;
	this->background_pen.setColor(color);
}




/**
   @reviewed-on 2019-07-19
*/
const QColor & ViewportPixmap::get_background_color(void) const
{
	return this->background_color;
}




/**
   @reviewed-on 2019-07-19
*/
void ViewportPixmap::set_highlight_usage(bool new_state)
{
	this->highlight_usage = new_state;
}




/**
   @reviewed-on 2019-07-19
*/
bool ViewportPixmap::get_highlight_usage(void) const
{
	return this->highlight_usage;
}




/**
   @reviewed-on 2019-07-19
*/
const QColor & ViewportPixmap::get_highlight_color(void) const
{
	return this->highlight_color;
}




/**
   @reviewed-on tbd
*/
void ViewportPixmap::set_highlight_color(const QString & color_name)
{
	this->highlight_color.setNamedColor(color_name);
	this->highlight_pen.setColor(this->highlight_color);
}




/**
   @reviewed-on tbd
*/
void ViewportPixmap::set_highlight_color(const QColor & color)
{
	this->highlight_color = color;
	this->highlight_pen.setColor(color);
}




/**
   @reviewed-on 2019-07-19
*/
const QPen & ViewportPixmap::get_highlight_pen(void) const
{
	return this->highlight_pen;
}




/**
   @reviewed-on tbd
*/
void ViewportPixmap::set_highlight_thickness(int w)
{
	this->highlight_pen.setWidth(w);

	this->highlight_pen.setCapStyle(Qt::RoundCap);
	this->highlight_pen.setJoinStyle(Qt::RoundJoin);

#if 0   /* This line style is used by default. */
	this->highlight_pen.setStyle(Qt::SolidLine);
#endif
}




/**
   @reviewed-on tbd
*/
QRect ViewportPixmap::central_get_rect(void) const
{
	const int begin_x = this->central_get_leftmost_pixel();
	const int begin_y = this->central_get_topmost_pixel();

	return QRect(begin_x, begin_y, begin_x + this->central_get_width(), begin_y + this->central_get_height());
}




/**
   @reviewed-on tbd
*/
void ViewportPixmap::paintEvent(QPaintEvent * ev)
{
	//qDebug() << SG_PREFIX_I;

	QPainter event_painter(this);
	event_painter.drawPixmap(0, 0, this->vpixmap);

	return;
}




/**
   @reviewed-on 2019-07-19
*/
const QPixmap & ViewportPixmap::get_pixmap(void) const
{
	return this->vpixmap;
}




/**
   @reviewed-on tbd
*/
void ViewportPixmap::set_pixmap(const QPixmap & new_pixmap)
{
	if (this->vpixmap.size() != new_pixmap.size()) {
		qDebug() << SG_PREFIX_E << "Pixmap size mismatch: existing pixmap =" << this->vpixmap.size() << ", new pixmap =" << new_pixmap.size();
	} else {
		this->painter.drawPixmap(0, 0, new_pixmap, 0, 0, 0, 0);
	}
}




/**
   @reviewed-on tbd
*/
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





/*
  Clip functions continually reduce the value by a factor until it is in the acceptable range
  whilst also scaling the other coordinate value.

   @reviewed-on tbd
*/
static void clip_x(fpixel * x1, fpixel * y1, fpixel * x2, fpixel * y2)
{
	while (std::fabs(*x1) > 32768) {
		*x1 = *x2 + (0.5 * (*x1 - *x2));
		*y1 = *y2 + (0.5 * (*y1 - *y2));
	}
}




/**
   @reviewed-on tbd
*/
static void clip_y(fpixel * x1, fpixel * y1, fpixel * x2, fpixel * y2)
{
	while (std::fabs(*y1) > 32767) {
		*x1 = *x2 + (0.5 * (*x1 - *x2));
		*y1 = *y2 + (0.5 * (*y1 - *y2));
	}
}




/**
   @x1: screen coord
   @y1: screen coord
   @x2: screen coord
   @y2: screen coord

   Due to the seemingly undocumented behaviour of gdk_draw_line(), we need to restrict the range of values passed in.
   So despite it accepting ints, the effective range seems to be the actually the minimum C int range (2^16).
   This seems to be limitations coming from the X Window System.

   See http://www.rahul.net/kenton/40errs.html
   ERROR 7. Boundary conditions.
   "The X coordinate space is not infinite.
   Most drawing functions limit position, width, and height to 16 bit integers (sometimes signed, sometimes unsigned) of accuracy.
   Because most C compilers use 32 bit integers, Xlib will not complain if you exceed the 16 bit limit, but your results will usually not be what you expected.
   You should be especially careful of this if you are implementing higher level scalable graphics packages."

   This function should be called before calling gdk_draw_line().

  @reviewed-on tbd
*/
void ViewportPixmap::clip_line(fpixel * x1, fpixel * y1, fpixel * x2, fpixel * y2)
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




/**
   @reviewed-on 2019-07-19
*/
void ViewportPixmap::snapshot_save(void)
{
	qDebug() << SG_PREFIX_I << "Save snapshot";
	this->vpixmap_snapshot = this->vpixmap;
}




/**
   @reviewed-on 2019-07-19
*/
void ViewportPixmap::snapshot_restore(void)
{
	qDebug() << SG_PREFIX_I << "Restore snapshot";
	this->vpixmap = this->vpixmap_snapshot;
}




/**
   @reviewed-on: 2019-07-21
*/
void ViewportPixmap::debug_pixmap_draw(void)
{
	const int leftmost_px   = this->central_get_leftmost_pixel();
	const int rightmost_px  = this->central_get_rightmost_pixel();
	const int topmost_px    = this->central_get_topmost_pixel();
	const int bottommost_px = this->central_get_bottommost_pixel();

	if (1) {
		/*
		  Small circles with centers in corners of central
		  area of viewport.
		*/
		const int radius = 8;
		this->draw_ellipse(QColor("red"),    ScreenPos(leftmost_px, topmost_px), radius, radius);
		this->draw_ellipse(QColor("green"),  ScreenPos(rightmost_px, topmost_px), radius, radius);
		this->draw_ellipse(QColor("blue"),   ScreenPos(leftmost_px, bottommost_px), radius, radius);
		this->draw_ellipse(QColor("orange"), ScreenPos(rightmost_px, bottommost_px), radius, radius);
	}

	if (1) {
		/*
		  Boundaries of whole pixmap.
		*/
		QPen pen(QColor("blue"));
		pen.setWidth(2);
		this->draw_rectangle(pen, 0, 0, this->total_get_width(), this->total_get_height());
	}

	if (1) {
		/*
		  The largest rectangle that fits into central part of pixmap.

		  When looking at right and bottom border of the
		  rectangle, remember about how Qt draws these borders
		  of rectangles:
		  https://doc.qt.io/qt-5/qrect.html#rendering
		*/
		QPen pen(QColor("red"));
		pen.setWidth(1);
		const int begin_x = this->central_get_leftmost_pixel();
		const int begin_y = this->central_get_topmost_pixel();
		const int width = this->central_get_width();
		const int height = this->central_get_height();
		this->draw_rectangle(pen, begin_x, begin_y, width, height);
	}

	if (1) {
		/*
		  The largest ellipsis that fits into central part of
		  pixmap. The outermost parts of ellipsis must overlap
		  with the sides of the largest rectangle drawn above.

		  See also this info about how Qt paints rectangles
		  (I'm guessing that similar approach applies to
		  ellipsis too):
		  https://doc.qt.io/qt-5/qrect.html#rendering
		*/

		/*
		  Either of the two ways to get center should give the same
		  result.  Keep them both in code to be able to test all three
		  GisViewport methods.
		*/
#if 0
		const ScreenPos center = this->central_get_center_screen_pos();
#else
		const ScreenPos center(this->central_get_x_center_pixel(),
				       this->central_get_y_center_pixel());
#endif

		QPen pen(QColor("blue"));
		pen.setWidth(1);
		this->draw_ellipse(pen,
				   center,
				   this->central_get_width() / 2.0,
				   this->central_get_height() / 2.0);
	}

	if (1) {
		/* Draw checkered pixmap in four corners of central
		   area of viewport. This pixmap is rectangular, so I
		   can use the same sizes when scaling it down. */
		const int size = 40;
		const QPixmap pixmap = QPixmap(":/test_data/pixmap_checkered_black_alpha.png").scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

		fpixel viewport_x;
		fpixel viewport_y;

		/* Top left. */
		viewport_x = leftmost_px;
		viewport_y = topmost_px;
		this->draw_pixmap(pixmap, viewport_x, viewport_y, 0, 0, size, size);
		//this->draw_pixmap(pixmap, viewport_x, viewport_y);

		/* Top right. */
		viewport_x = rightmost_px - size;
		viewport_y = topmost_px;
		this->draw_pixmap(pixmap, viewport_x, viewport_y, 0, 0, size, size);

		/* Bottom left. */
		viewport_x = leftmost_px;
		viewport_y = bottommost_px - size;
		this->draw_pixmap(pixmap, viewport_x, viewport_y, 0, 0, size, size);

		/* Bottom right. */
		viewport_x = rightmost_px - size;
		viewport_y = bottommost_px - size;
		this->draw_pixmap(pixmap, viewport_x, viewport_y, 0, 0, size, size);
	}
}




/**
   @reviewed-on tbd
*/
void ViewportPixmap::debug_print_info(void) const
{
	qDebug() << SG_PREFIX_I << this->debug;


	qDebug() << SG_PREFIX_I << "total width    =" << this->total_get_width();
	qDebug() << SG_PREFIX_I << "total height   =" << this->total_get_height();

	qDebug() << SG_PREFIX_I << "central width  =" << this->central_get_width();
	qDebug() << SG_PREFIX_I << "central height =" << this->central_get_height();
	qDebug() << SG_PREFIX_I << "left width     =" << this->left_get_width();
	qDebug() << SG_PREFIX_I << "left height    =" << this->left_get_height();
	qDebug() << SG_PREFIX_I << "right width    =" << this->right_get_width();
	qDebug() << SG_PREFIX_I << "right height   =" << this->right_get_height();
	qDebug() << SG_PREFIX_I << "top width      =" << this->top_get_width();
	qDebug() << SG_PREFIX_I << "top height     =" << this->top_get_height();
	qDebug() << SG_PREFIX_I << "bottom width   =" << this->bottom_get_width();
	qDebug() << SG_PREFIX_I << "bottom height  =" << this->bottom_get_height();
}




/**
   @reviewed-on tbd
*/
sg_ret ViewportPixmap::calculate_scaled_sizes(int target_width, int target_height, int & scaled_width, int & scaled_height, double & scale_factor)
{
	/*
	  We always want to print original pixmap to target device in
	  the pixmap's fullness. If necessary, we should scale the
	  pixmap in a way that ensures covering the most of target
	  device, but we want to keep proportions of original
	  pixmap. The question is how much should we scale our pixmap?

	  We need to look at how shapes of original pixmap and target
	  device match. In the example below the original pixmap is
	  more "height-ish" than target device. We will want to scale
	  the original pixmap by factor of ~2 (target device height /
	  original height) before drawing it to target device. Scaling
	  original pixmap in this example to match width of target
	  device would make the resulting image too tall.

	   +-------------------------------+
	   |                               |
	   |   +----+                      |
	   |   |    |original pixmap       |
	   |   |    |                      |
	   |   |    |                      |target device
	   |   +----+                      |
	   |                               |
	   |                               |
	   +-------------------------------+

	   TODO_HARD: make sure that this works also for target device
	   smaller than original pixmap.
	*/

	qDebug() << SG_PREFIX_I << "Attempt to create scaled pixmap with size no larger than width =" << target_width << ", height =" << target_height;


	const int orig_width = this->central_get_width();
	const int orig_height = this->central_get_height();

	const double orig_factor = 1.0 * orig_width / orig_height;
	const double target_factor = 1.0 * target_width / target_height;

	if (orig_factor > target_factor) {
		/*
		  Original pixmap is more "wide-ish" than target
		  device.

		  +--------------+
		  |              |
		  | +---------+  |
		  | |         |original pixmap
		  | |         |  |
		  | +---------+  |target device
		  |              |
		  |              |
		  |              |
		  |              |
		  +--------------+

		  Width of scaled pixmap will then simply be copied,
		  but height needs to be calculated.
		*/
		qDebug() << SG_PREFIX_I << "Target device is more height-ish than pixmap, copying width, calculating height";
		qDebug() << SG_PREFIX_I << "Calculating height, floor(" << (scale_factor * orig_height) << ") =" << floor(scale_factor * orig_height);

		scale_factor = 1.0 * target_width / orig_width;
		scaled_width = target_width;
		scaled_height = floor(scale_factor * orig_height);
	} else {
		/*
		  Original pixmap is more "height-ish" than target
		  device.

		  +-------------------------------+
		  |                               |
		  |   +----+                      |
		  |   |    |original pixmap       |
		  |   |    |                      |
		  |   |    |                      |target device
		  |   +----+                      |
		  |                               |
		  |                               |
		  +-------------------------------+

		  Height of scaled pixmap can be simply copied, but
		  width needs to be calculated.
		*/
		qDebug() << SG_PREFIX_I << "Target device is more wide-ish than pixmap, calculating width, copying height";
		qDebug() << SG_PREFIX_I << "Calculating width, floor(" << (scale_factor * orig_width) << ") =" << floor(scale_factor * orig_width);

		scale_factor = 1.0 * target_height / orig_height;
		scaled_width = floor(scale_factor * orig_width);
		scaled_height = target_height;
	}
	qDebug() << SG_PREFIX_I << "Scale factor =" << scale_factor << ", scaled width =" << scaled_width << ", scaled height =" << scaled_height;

	return sg_ret::ok;
}




/**
   @reviewed-on 2019-07-28
*/
void ViewportPixmap::resizeEvent(QResizeEvent * ev)
{
	qDebug() << SG_PREFIX_I << "Reacting to resize event, new total size is width =" << this->geometry().width() << ", height =" << this->geometry().height();

	/* This will emit "sizes changed" signal. Clients that
	   subscribe to this signal will re-draw their contents to
	   resized viewport pixmap widget. */
	this->apply_total_sizes(this->geometry().width(), this->geometry().height());

	return;
}




sg_ret ViewportPixmap::central_draw_outside_boundary_rect(void)
{
	if (this->left_margin_width > 0
	    || this->right_margin_width > 0
	    || this->top_margin_height > 0
	    || this->bottom_margin_height > 0) {

		const int x = this->central_get_leftmost_pixel() - 1;
		const int y = this->central_get_topmost_pixel() - 1;
		const int width = this->central_get_width() + 2;
		const int height = this->central_get_height() + 2;

		this->draw_rectangle(this->central_outside_boundary_rect_pen, x, y, width, height);
	}

	return sg_ret::ok;
}
