/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
 *
 * Lat/Lon plotting functions calcxy* are from GPSDrive
 * GPSDrive Copyright (C) 2001-2004 Fritz Ganter <ganter@ganter.at>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif




#include <cmath>




#include <QDebug>
#include <QPainter>




#include "preferences.h"
#include "viewport.h"
#include "viewport_internal.h"
#include "coords.h"
#include "globals.h"




using namespace SlavGPS;




#define PREFIX " Viewport Decorations: "




static int rescale_unit(double * base_distance, double * scale_unit, int maximum_width);
static int PAD = 10;




ViewportDecorations::ViewportDecorations()
{
	this->pen_marks_fg.setColor(QColor("grey"));
	this->pen_marks_fg.setWidth(2);
	this->pen_marks_bg.setColor(QColor("pink"));
	this->pen_marks_bg.setWidth(6);
}




void ViewportDecorations::draw(Viewport * viewport)
{
	this->draw_scale(viewport);
	this->draw_copyrights(viewport);
	this->draw_center_mark(viewport);
	this->draw_logos(viewport);
}




void ViewportDecorations::draw_scale(Viewport * viewport)
{
	if (!viewport->scale_visibility) {
		return;
	}

	double base_distance;       /* Physical (real world) distance corresponding to full width of drawn scale. Physical units (miles, meters). */
	int HEIGHT = 20;            /* Height of scale in pixels. */
	float RELATIVE_WIDTH = 0.5; /* Width of scale, relative to width of viewport. */
	int MAXIMUM_WIDTH = viewport->size_width * RELATIVE_WIDTH;

	const Coord left =  viewport->screen_pos_to_coord(0,                                     viewport->size_height / 2);
	const Coord right = viewport->screen_pos_to_coord(viewport->size_width * RELATIVE_WIDTH, viewport->size_height / 2);

	DistanceUnit distance_unit = Preferences::get_unit_distance();
	switch (distance_unit) {
	case DistanceUnit::KILOMETRES:
		base_distance = Coord::distance(left, right); /* In meters. */
		break;
	case DistanceUnit::MILES:
		/* In 0.1 miles (copes better when zoomed in as 1 mile can be too big). */
		base_distance = VIK_METERS_TO_MILES (Coord::distance(left, right)) * 10.0;
		break;
	case DistanceUnit::NAUTICAL_MILES:
		/* In 0.1 NM (copes better when zoomed in as 1 NM can be too big). */
		base_distance = VIK_METERS_TO_NAUTICAL_MILES (Coord::distance(left, right)) * 10.0;
		break;
	default:
		base_distance = 1; /* Keep the compiler happy. */
		qDebug() << "EE:" PREFIX << __FUNCTION__ << __LINE__ << "failed to get correct units of distance, got" << (int) distance_unit;
	}

	/* At this point "base_distance" is a distance between "left" and "right" in physical units.
	   But a scale can't have an arbitrary length (e.g. 3.07 miles, or 23.2 km),
	   it should be something like 1.00 mile or 10.00 km - a unit. */
	double scale_unit = 1; /* [km, miles, nautical miles] */

	//fprintf(stderr, "%s:%d: base_distance = %g, scale_unit = %g, MAXIMUM_WIDTH = %d\n", __FUNCTION__, __LINE__, base_distance, scale_unit, MAXIMUM_WIDTH);
	int len = rescale_unit(&base_distance, &scale_unit, MAXIMUM_WIDTH);
	//fprintf(stderr, "resolved len = %d\n", len);


	const QPen & pen_fg = this->pen_marks_fg;
	const QPen & pen_bg = this->pen_marks_bg;

	this->draw_scale_helper_scale(viewport, pen_bg, len, HEIGHT); /* Bright background. */
	this->draw_scale_helper_scale(viewport, pen_fg, len, HEIGHT); /* Darker scale on the bright background. */

	const QString scale_value = this->draw_scale_helper_value(viewport, distance_unit, scale_unit);


	QPointF scale_start(PAD, viewport->size_height - PAD); /* Bottom-left corner of scale. */
	QPointF value_start = QPointF(scale_start.x() + len + PAD, scale_start.y()); /* Bottom-left corner of value. */
	QRectF bounding_rect = QRectF((int) value_start.x(), 0, (int) value_start.x() + 300, (int) value_start.y());
	viewport->draw_text(QFont("Helvetica", 40), pen_fg, bounding_rect, Qt::AlignBottom | Qt::AlignLeft, scale_value, 0);
	/* TODO: we need to draw background of the text in some color,
	   so that it's more visible on a map that will be present in the background. */

#if 1
	/* Debug. */
	QPainter painter(viewport->scr_buffer);
	painter.setPen(QColor("red"));
	painter.drawEllipse(scale_start, 3, 3);
	painter.setPen(QColor("blue"));
	painter.drawEllipse(value_start, 3, 3);
#endif

	viewport->repaint();
}




void ViewportDecorations::draw_scale_helper_scale(Viewport * viewport, const QPen & pen, int scale_len, int h)
{
	/* Black scale. */
	viewport->draw_line(pen, PAD,             viewport->size_height - PAD, PAD + scale_len, viewport->size_height - PAD);
	viewport->draw_line(pen, PAD,             viewport->size_height - PAD, PAD,             viewport->size_height - PAD - h);
	viewport->draw_line(pen, PAD + scale_len, viewport->size_height - PAD, PAD + scale_len, viewport->size_height - PAD - h);

	int y1 = viewport->size_height - PAD;
	for (int i = 1; i < 10; i++) {
		int x1 = PAD + i * scale_len / 10;
		int diff = ((i == 5) ? (2 * h / 3) : (1 * h / 3));
		viewport->draw_line(pen, x1, y1, x1, y1 - diff);
	}
}




QString ViewportDecorations::draw_scale_helper_value(Viewport * viewport, DistanceUnit distance_unit, double scale_unit)
{
	QString scale_value;

	switch (distance_unit) {
	case DistanceUnit::KILOMETRES:
		if (scale_unit >= 1000) {
			scale_value = QObject::tr("y%1 km").arg((int) scale_unit / 1000);
		} else {
			scale_value = QObject::tr("y%1 m").arg((int) scale_unit);
		}
		break;
	case DistanceUnit::MILES:
		/* Handle units in 0.1 miles. */
		if (scale_unit < 10.0) {
			scale_value = QObject::tr("%1 miles").arg(scale_unit / 10.0, 0, 'f', 1); /* "%0.1f" */
		} else if ((int) scale_unit == 10.0) {
			scale_value = QObject::tr("1 mile");
		} else {
			scale_value = QObject::tr("%1 miles").arg((int) (scale_unit / 10.0));
		}
		break;
	case DistanceUnit::NAUTICAL_MILES:
		/* Handle units in 0.1 NM. */
		if (scale_unit < 10.0) {
			scale_value = QObject::tr("%1 NM").arg(scale_unit / 10.0, 0, 'f', 1); /* "%0.1f" */
		} else if ((int) scale_unit == 10.0) {
			scale_value = QObject::tr("1 NM");
		} else {
			scale_value = QObject::tr("%1 NMs").arg((int) (scale_unit / 10.0));
		}
		break;
	default:
		qDebug() << "EE:" PREFIX << __FUNCTION__ << __LINE__ << "failed to get correct units of distance, got" << (int) distance_unit;
	}

	return scale_value;
}




void ViewportDecorations::draw_copyrights(Viewport * viewport)
{
	/* TODO: how to ensure that these 128 chars fit into bounding rectangle used below? */
#define MAX_COPYRIGHTS_LEN 128
	QString result;
	int free_space = MAX_COPYRIGHTS_LEN;

#if 1
	this->copyrights << "test copyright 1";
	this->copyrights << "another test copyright";
#endif

	/* Compute copyrights string. */

	for (int i = 0; i < this->copyrights.size(); i++) {

		if (free_space < 0) {
			break;
		}

		QString const & copyright = copyrights[i];

		/* Only use part of this copyright that fits in the available space left,
		   remembering 1 character is left available for the appended space. */

		result.append(copyright.left(free_space));
		free_space -= copyright.size();

		result.append(" ");
		free_space--;
	}

	/* Copyright text will be in bottom-right corner. */
	/* Use no more than half of width of viewport. */
	int x_size = 0.5 * viewport->size_width;
	int y_size = 0.7 * viewport->size_height;
	int w_size = viewport->size_width - x_size - PAD;
	int h_size = viewport->size_height - y_size - PAD;

	QPointF box_start = QPointF(viewport->size_width - PAD, viewport->size_height - PAD); /* Anchor in bottom-right corner. */
	QRectF bounding_rect = QRectF(box_start.x(), box_start.y(), -w_size, -h_size);
	viewport->draw_text(QFont("Helvetica", 12), this->pen_marks_fg, bounding_rect, Qt::AlignBottom | Qt::AlignRight, result, 0);

#undef MAX_COPYRIGHTS_LEN
}




void ViewportDecorations::draw_center_mark(Viewport * viewport)
{
	qDebug() << "II:" PREFIX << __FUNCTION__ << __LINE__ << "draw center mark:" << viewport->center_mark_visibility;

	if (!viewport->center_mark_visibility) {
		return;
	}

	const int len = 30;
	const int gap = 4;
	int center_x = viewport->size_width / 2;
	int center_y = viewport->size_height / 2;

	const QPen & pen_fg = this->pen_marks_fg;
	const QPen & pen_bg = this->pen_marks_bg;

	/* White background. */
	viewport->draw_line(pen_bg, center_x - len, center_y,       center_x - gap, center_y);
	viewport->draw_line(pen_bg, center_x + gap, center_y,       center_x + len, center_y);
	viewport->draw_line(pen_bg, center_x,       center_y - len, center_x,       center_y - gap);
	viewport->draw_line(pen_bg, center_x,       center_y + gap, center_x,       center_y + len);

	/* Black foreground. */
	viewport->draw_line(pen_fg, center_x - len, center_y,        center_x - gap, center_y);
	viewport->draw_line(pen_fg, center_x + gap, center_y,        center_x + len, center_y);
	viewport->draw_line(pen_fg, center_x,       center_y - len,  center_x,       center_y - gap);
	viewport->draw_line(pen_fg, center_x,       center_y + gap,  center_x,       center_y + len);

	viewport->update();
}




void ViewportDecorations::draw_logos(Viewport * viewport)
{
	int x_pos = viewport->size_width - PAD;
	int y_pos = PAD;
	for (auto iter = this->logos.begin(); iter != this->logos.end(); iter++) {
		QPixmap const * logo = *iter;
		const int logo_width = logo->width();
		const int logo_height = logo->height();
		viewport->draw_pixmap(*logo, 0, 0, x_pos - logo_width, y_pos, logo_width, logo_height);
		x_pos = x_pos - logo_width - PAD;
	}
}



void ViewportDecorations::reset_data(void)
{
	this->reset_copyrights();
	this->reset_logos();
}



void ViewportDecorations::reset_copyrights(void)
{
	this->copyrights.clear();
}




void ViewportDecorations::reset_logos(void)
{
	/* Do not free pointers, they are owned by someone else.
	   TODO: this is potentially a source of problems - if owner deletes pointer, it becomes invalid in viewport, right?. */
	this->logos.clear();
}




/* Return length of scale bar, in pixels. */
static int rescale_unit(double * base_distance, double * scale_unit, int maximum_width)
{
	double ratio = *base_distance / *scale_unit;
	//fprintf(stderr, "%s:%d: %d / %d / %d\n", __FUNCTION__, __LINE__, (int) *base_distance, (int) *scale_unit, maximum_width);

	int n = 0;
	if (ratio > 1) {
		n = (int) floor(log10(ratio));
	} else {
		n = (int) floor(log10(1.0 / ratio));
	}

	//fprintf(stderr, "%s:%d: ratio = %f, n = %d\n", __FUNCTION__, __LINE__, ratio, n);

	*scale_unit = pow(10.0, n); /* scale_unit is still a unit (1 km, 10 miles, 100 km, etc. ), only 10^n times larger. */
	ratio = *base_distance / *scale_unit;
	double len = maximum_width / ratio; /* [px] */


	//fprintf(stderr, "%s:%d: len = %f\n", __FUNCTION__, __LINE__, len);
	/* I don't want the scale unit to be always 10^n.

	   Let's say that at this point we have a scale of length 10km
	   = 344 px. Let's see what actually happens as we zoom out:
	   zoom  0: 10 km / 344 px
	   zoom -1: 10 km / 172 px
	   zoom -2: 10 km /  86 px
	   zoom -3: 10 km /  43 px

	   At zoom -3 the scale is small and not very useful. With the code
	   below enabled we get:

	   zoom  0: 10 km / 345 px
	   zoom -1: 20 km / 345 px
	   zoom -2: 20 km / 172 px
	   zoom -3: 50 km / 216 px

	   We can see that the scale doesn't become very short, and keeps being usable. */

	if (maximum_width / len > 5) {
		*scale_unit *= 5;
		ratio = *base_distance / *scale_unit;
		len = maximum_width / ratio;

	} else if (maximum_width / len > 2) {
		*scale_unit *= 2;
		ratio = *base_distance / *scale_unit;
		len = maximum_width / ratio;

	} else {
		;
	}
	//fprintf(stderr, "rescale unit len = %g\n", len);

	return (int) len;
}
