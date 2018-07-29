/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2006, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2012-2015, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2018, Kamil Ignacak <acerion@wp.pl>
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




#include <cmath>




#include "viewport_internal.h"
#include "ruler.h"
#include "globals.h"




using namespace SlavGPS;




#define SG_MODULE "Generic Tools"


#define COMPASS_RADIUS         80  /* Radius of middle circle of compass. */
#define COMPASS_RADIUS_DELTA    4  /* Distance between compass' circles. */




Ruler::Ruler(Viewport * new_viewport, int begin_x, int begin_y, DistanceUnit new_distance_unit)
{
	this->x1 = begin_x;
	this->y1 = begin_y;
	this->distance_unit = new_distance_unit;
	this->viewport = new_viewport;

	this->line_pen.setColor(QColor("black"));
	this->line_pen.setWidth(1);
	this->arc_pen.setColor(QColor("red"));
	this->arc_pen.setWidth(COMPASS_RADIUS_DELTA);
}





void Ruler::end_moved_to(int new_x2, int new_y2, QPainter & painter, double new_distance1, double new_distance2)
{
	this->x2 = new_x2;
	this->y2 = new_y2;
	this->distance1 = new_distance1;
	this->distance2 = new_distance2;

	this->len = sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
	this->dx = (x2 - x1) / len * 10;
	this->dy = (y2 - y1) / len * 10;
	this->c = cos(DEG2RAD(15.0));
	this->s = sin(DEG2RAD(15.0));

	this->viewport->compute_bearing(this->x1, this->y1, this->x2, this->y2, &this->angle, &this->baseangle);

	this->draw_line(painter);
}




/**
   @param x1, y1 - coordinates of beginning of ruler (start coordinates, where cursor was pressed down)
   @param x2, y2 - coordinates of end of ruler (end coordinates, where cursor currently is)
*/
void Ruler::draw_line(QPainter & painter)
{
	const QString bearing_label = QObject::tr("%1°").arg(RAD2DEG(this->angle), 3, 'f', 2);
	QString distance_label;
	if (this->distance1 > -0.5 && this->distance2 > -0.5) {
		distance_label = QString("%1\n%2")
			.arg(Measurements::get_distance_string_for_ruler(this->distance1, this->distance_unit))
			.arg(Measurements::get_distance_string_for_ruler(this->distance2, this->distance_unit));
	} else if (this->distance1 > -0.5) {
		distance_label = Measurements::get_distance_string_for_ruler(this->distance1, this->distance_unit);
	} else if (this->distance2 > -0.5) {
		distance_label = Measurements::get_distance_string_for_ruler(this->distance2, this->distance_unit);
	} else {
		; /* NOOP */
	}


	QPen main_pen;
	main_pen.setColor(QColor("black"));
	main_pen.setWidth(1);
	painter.setPen(main_pen);


	/* Draw line with arrow ends. */
	if (1) {
		int tmp_x1 = this->x1;
		int tmp_y1 = this->y1;
		int tmp_x2 = this->x2;
		int tmp_y2 = this->y2;
		Viewport::clip_line(&tmp_x1, &tmp_y1, &tmp_x2, &tmp_y2);
		painter.drawLine(tmp_x1, tmp_y1, tmp_x2, tmp_y2);


		Viewport::clip_line(&this->x1, &this->y1, &this->x2, &this->y2);

		painter.drawLine(this->x1,            this->y1,            this->x2,                                             this->y2);
		painter.drawLine(this->x1 - this->dy, this->y1 + this->dx, this->x1 + this->dy,                                  this->y1 - this->dx);
		painter.drawLine(this->x2 - this->dy, this->y2 + this->dx, this->x2 + this->dy,                                  this->y2 - this->dx);
		painter.drawLine(this->x2,            this->y2,            this->x2 - (this->dx * this->c + this->dy * this->s), this->y2 - (this->dy * this->c - this->dx * this->s));
		painter.drawLine(this->x2,            this->y2,            this->x2 - (this->dx * this->c - this->dy * this->s), this->y2 - (this->dy * this->c + this->dx * this->s));
		painter.drawLine(this->x1,            this->y1,            this->x1 + (this->dx * this->c + this->dy * this->s), this->y1 + (this->dy * this->c - this->dx * this->s));
		painter.drawLine(this->x1,            this->y1,            this->x1 + (this->dx * this->c - this->dy * this->s), this->y1 + (this->dy * this->c + this->dx * this->s));
	}


	/* Draw compass. */


	const int radius = COMPASS_RADIUS;
	const int radius_delta = COMPASS_RADIUS_DELTA;


	if (1) {
		/* Three full circles. */
		painter.drawArc(this->x1 - radius + radius_delta, this->y1 - radius + radius_delta, 2 * (radius - radius_delta), 2 * (radius - radius_delta), 0, 16 * 360); /* Innermost. */
		painter.drawArc(this->x1 - radius,                this->y1 - radius,                2 * radius,                  2 * radius,                  0, 16 * 360); /* Middle. */
		painter.drawArc(this->x1 - radius - radius_delta, this->y1 - radius - radius_delta, 2 * (radius + radius_delta), 2 * (radius + radius_delta), 0, 16 * 360); /* Outermost. */
	}

	/* Fill between middle and innermost circle. */
	if (1) {
		const float start_angle = (90 - RAD2DEG(this->baseangle)) * 16;
		const float span_angle = -RAD2DEG(this->angle) * 16;

		QPen fill_pen;

		fill_pen.setColor(QColor("red"));
		fill_pen.setWidth(radius_delta);
		painter.setPen(fill_pen);

		painter.drawArc(this->x1 - radius + radius_delta / 2,
				this->y1 - radius + radius_delta / 2,
				2 * radius - radius_delta,
				2 * radius - radius_delta,
				start_angle, span_angle);


	}

	painter.setPen(main_pen);

	/* Ticks around circles, every N degrees. */
	if (1) {
		int ticksize = 2 * radius_delta;
		for (int i = 0; i < 180; i += 5) {
			this->c = cos(DEG2RAD(i) * 2 + this->baseangle);
			this->s = sin(DEG2RAD(i) * 2 + this->baseangle);
			painter.drawLine(this->x1 + (radius-radius_delta)*this->c, this->y1 + (radius-radius_delta)*this->s, this->x1 + (radius+ticksize)*this->c, this->y1 + (radius+ticksize)*this->s);
		}
	}

	if (1) {
		/* Two axis inside a compass.
		   Varying angle will rotate the axis. I don't know why you would need this :) */
		//float angle = 0;
		int c2 = (radius + radius_delta * 2) * sin(this->baseangle);
		int s2 = (radius + radius_delta * 2) * cos(this->baseangle);
		painter.drawLine(this->x1 - c2, this->y1 - s2, this->x1 + c2, this->y1 + s2);
		painter.drawLine(this->x1 + s2, this->y1 - c2, this->x1 - s2, this->y1 + c2);
	}


	/* Draw compass labels. */
	if (1) {
		painter.drawText(this->x1 - 5, this->y1 - radius - 3 * radius_delta - 8, "N");
	}


	const int margin = 3;

	/* Draw distance label. */
	if (1) {
		QRectF label1_rect = painter.boundingRect(QRect(0, 0, 0, 0), Qt::AlignHCenter, distance_label);

		int label1_x;
		int label1_y;

		if (this->dy > 0) {
			label1_x = (this->x1 + this->x2) / 2 + this->dy;
			label1_y = (this->y1 + this->y2) / 2 - label1_rect.height() / 2 - this->dx;
		} else {
			label1_x = (this->x1 + this->x2) / 2 - this->dy;
			label1_y = (this->y1 + this->y2) / 2 - label1_rect.height() / 2 + this->dx;
		}

		if (label1_x < -5 || label1_y < -5 || label1_x > this->viewport->get_width() + 5 || label1_y > this->viewport->get_height() + 5) {
			label1_x = this->x2 + 10;
			label1_y = this->y2 - 5;
		}

		label1_rect.moveTo(label1_x, label1_y);
		label1_rect.adjust(-margin, -margin, margin, margin);

		painter.fillRect(label1_rect, QColor("gray"));
		painter.drawText(label1_rect, Qt::AlignCenter, distance_label);
	}


	/* Draw bearing label. */
	if (1) {
		QRectF label2_rect = painter.boundingRect(QRect(0, 0, 0, 0), Qt::AlignBottom | Qt::AlignLeft, bearing_label);

		const int label2_x = this->x1 - radius * cos(this->angle - M_PI_2) / 2;
		const int label2_y = this->y1 - radius * sin(this->angle - M_PI_2) / 2;

		label2_rect.moveTo(label2_x - label2_rect.width() / 2, label2_y - label2_rect.height() / 2);
		label2_rect.adjust(-margin, -margin, margin, margin);

		painter.fillRect(label2_rect, QColor("pink"));
		painter.drawText(label2_rect, Qt::AlignCenter, bearing_label);
	}
}
