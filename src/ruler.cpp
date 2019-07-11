
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




Ruler::Ruler(GisViewport * new_gisview, DistanceUnit new_distance_unit)
{
	this->distance_unit = new_distance_unit;
	this->gisview = new_gisview;

	this->line_pen.setColor(QColor("black"));
	this->line_pen.setWidth(1);

	this->compass_pen.setColor(QColor("black"));
	this->compass_pen.setWidth(1);

	this->arc_pen.setColor(QColor("red"));
	this->arc_pen.setWidth(COMPASS_RADIUS_DELTA);
}



void Ruler::set_begin(int begin_x_, int begin_y_)
{
	this->begin_x = begin_x_;
	this->begin_y = begin_y_;

	this->begin_coord = this->gisview->screen_pos_to_coord(this->begin_x, this->begin_y);
}




void Ruler::set_end(int end_x_, int end_y_)
{
	this->end_x = end_x_;
	this->end_y = end_y_;

	this->end_coord = this->gisview->screen_pos_to_coord(this->end_x, this->end_y);

	/* TODO: these calculations are repeated in compute_bearing(). */
	this->len = sqrt((this->begin_x - this->end_x) * (this->begin_x - this->end_x) + (this->begin_y - this->end_y) * (this->begin_y - this->end_y));
	this->dx = (this->end_x - this->begin_x) / len * 10;
	this->dy = (this->end_y - this->begin_y) / len * 10;

	this->c = cos(DEG2RAD(15.0));
	this->s = sin(DEG2RAD(15.0));

	this->gisview->compute_bearing(this->begin_x, this->begin_y, this->end_x, this->end_y, this->angle, this->base_angle);

	this->line_distance = Coord::distance_2(this->end_coord, this->begin_coord);
}




void Ruler::paint_ruler(QPainter & painter, bool paint_tooltips)
{
	painter.setPen(this->line_pen);

	/* Draw line with arrow ends. */
	if (1) {
		int tmp_begin_x = this->begin_x;
		int tmp_begin_y = this->begin_y;
		int tmp_end_x = this->end_x;
		int tmp_end_y = this->end_y;
		GisViewport::clip_line(&tmp_begin_x, &tmp_begin_y, &tmp_end_x, &tmp_end_y);
		painter.drawLine(tmp_begin_x, tmp_begin_y, tmp_end_x, tmp_end_y);


		GisViewport::clip_line(&this->begin_x, &this->begin_y, &this->end_x, &this->end_y);

		painter.drawLine(this->begin_x,            this->begin_y,            this->end_x,                                             this->end_y);
		painter.drawLine(this->begin_x - this->dy, this->begin_y + this->dx, this->begin_x + this->dy,                                  this->begin_y - this->dx);
		painter.drawLine(this->end_x - this->dy, this->end_y + this->dx, this->end_x + this->dy,                                  this->end_y - this->dx);
		painter.drawLine(this->end_x,            this->end_y,            this->end_x - (this->dx * this->c + this->dy * this->s), this->end_y - (this->dy * this->c - this->dx * this->s));
		painter.drawLine(this->end_x,            this->end_y,            this->end_x - (this->dx * this->c - this->dy * this->s), this->end_y - (this->dy * this->c + this->dx * this->s));
		painter.drawLine(this->begin_x,            this->begin_y,            this->begin_x + (this->dx * this->c + this->dy * this->s), this->begin_y + (this->dy * this->c - this->dx * this->s));
		painter.drawLine(this->begin_x,            this->begin_y,            this->begin_x + (this->dx * this->c - this->dy * this->s), this->begin_y + (this->dy * this->c + this->dx * this->s));
	}


	/* Draw compass. */


	const int radius = COMPASS_RADIUS;
	const int radius_delta = COMPASS_RADIUS_DELTA;
	painter.setPen(this->compass_pen);

	if (1) {

		/* Three full circles. */
		painter.drawArc(this->begin_x - radius + radius_delta, this->begin_y - radius + radius_delta, 2 * (radius - radius_delta), 2 * (radius - radius_delta), 0, 16 * 360); /* Innermost. */
		painter.drawArc(this->begin_x - radius,                this->begin_y - radius,                2 * radius,                  2 * radius,                  0, 16 * 360); /* Middle. */
		painter.drawArc(this->begin_x - radius - radius_delta, this->begin_y - radius - radius_delta, 2 * (radius + radius_delta), 2 * (radius + radius_delta), 0, 16 * 360); /* Outermost. */
	}

	/* Fill between middle and innermost circle. */
	if (1) {
		const float start_angle = (90 - RAD2DEG(this->base_angle.get_value())) * 16;
		const float span_angle = -RAD2DEG(this->angle.get_value()) * 16;

		painter.setPen(this->arc_pen);

		painter.drawArc(this->begin_x - radius + radius_delta / 2,
				this->begin_y - radius + radius_delta / 2,
				2 * radius - radius_delta,
				2 * radius - radius_delta,
				start_angle, span_angle);


	}

	painter.setPen(this->compass_pen);

	/* Ticks around circles, every N degrees. */
	if (1) {
		int ticksize = 2 * radius_delta;
		for (int i = 0; i < 180; i += 5) {
			this->c = cos(DEG2RAD(i) * 2 + this->base_angle.get_value());
			this->s = sin(DEG2RAD(i) * 2 + this->base_angle.get_value());
			painter.drawLine(this->begin_x + (radius-radius_delta)*this->c, this->begin_y + (radius-radius_delta)*this->s, this->begin_x + (radius+ticksize)*this->c, this->begin_y + (radius+ticksize)*this->s);
		}
	}

	if (1) {
		/* Two axis inside a compass.
		   Varying angle will rotate the axis. I don't know why you would need this :) */
		//float angle = 0;
		int c2 = (radius + radius_delta * 2) * sin(this->base_angle.get_value());
		int s2 = (radius + radius_delta * 2) * cos(this->base_angle.get_value());
		painter.drawLine(this->begin_x - c2, this->begin_y - s2, this->begin_x + c2, this->begin_y + s2);
		painter.drawLine(this->begin_x + s2, this->begin_y - c2, this->begin_x - s2, this->begin_y + c2);
	}


	/* Draw compass labels. */
	if (1) {
		painter.drawText(this->begin_x - 5, this->begin_y - radius - 3 * radius_delta - 8, "N");
	}



	const int margin = 3;

	/* Draw distance tooltip. */
	if (1 && paint_tooltips) {

		QString distance_label;
		if (this->line_distance.is_valid() && this->total_distance.is_valid()) {
			distance_label = QString("%1\n%2")
				.arg(this->line_distance.convert_to_unit(this->distance_unit).to_nice_string())
				.arg(this->total_distance.convert_to_unit(this->distance_unit).to_nice_string());
		} else if (this->line_distance.is_valid()) {
			distance_label = this->line_distance.convert_to_unit(this->distance_unit).to_nice_string();
		} else if (this->total_distance.is_valid()) {
			distance_label = this->total_distance.convert_to_unit(this->distance_unit).to_nice_string();
		} else {
			; /* NOOP */
		}


		QRectF label1_rect = painter.boundingRect(QRect(0, 0, 0, 0), Qt::AlignHCenter, distance_label);

		int label1_x;
		int label1_y;

		if (this->dy > 0) {
			label1_x = (this->begin_x + this->end_x) / 2 + this->dy;
			label1_y = (this->begin_y + this->end_y) / 2 - label1_rect.height() / 2 - this->dx;
		} else {
			label1_x = (this->begin_x + this->end_x) / 2 - this->dy;
			label1_y = (this->begin_y + this->end_y) / 2 - label1_rect.height() / 2 + this->dx;
		}

		if (label1_x < -5 || label1_y < -5 || label1_x > this->gisview->vpixmap.get_width() + 5 || label1_y > this->gisview->vpixmap.get_height() + 5) {
			label1_x = this->end_x + 10;
			label1_y = this->end_y - 5;
		}

		label1_rect.moveTo(label1_x, label1_y);
		label1_rect.adjust(-margin, -margin, margin, margin);

		painter.fillRect(label1_rect, QColor("gray"));
		painter.drawText(label1_rect, Qt::AlignCenter, distance_label);
	}


	/* Draw bearing tooltip. */
	if (1 && paint_tooltips) {
		const QString bearing_label = this->angle.to_string();

		QRectF label2_rect = painter.boundingRect(QRect(0, 0, 0, 0), Qt::AlignBottom | Qt::AlignLeft, bearing_label);

		const int label2_x = this->begin_x - radius * cos(this->angle.get_value() - M_PI_2) / 2;
		const int label2_y = this->begin_y - radius * sin(this->angle.get_value() - M_PI_2) / 2;

		label2_rect.moveTo(label2_x - label2_rect.width() / 2, label2_y - label2_rect.height() / 2);
		label2_rect.adjust(-margin, -margin, margin, margin);

		painter.fillRect(label2_rect, QColor("pink"));
		painter.drawText(label2_rect, Qt::AlignCenter, bearing_label);
	}
}




QString Ruler::get_msg(void) const
{
	const QString coord_string = this->end_coord.to_string();

	const QString msg = QObject::tr("%1 DIFF %2")
		.arg(coord_string)
		.arg(this->line_distance.convert_to_unit(this->distance_unit).to_string());

	return msg;
}
