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




#include <cmath>




#include <QDebug>
#include <QPainter>




#include "preferences.h"
#include "viewport.h"
#include "viewport_internal.h"
#include "coords.h"




using namespace SlavGPS;




#define SG_MODULE "Viewport Decorations"




static int rescale_unit(double * base_distance, double * scale_unit, int maximum_width);
static int PAD = 10;




ViewportDecorations::ViewportDecorations()
{
	this->pen_marks_fg.setColor(QColor("grey"));
	this->pen_marks_fg.setWidth(2);
	this->pen_marks_bg.setColor(QColor("pink"));
	this->pen_marks_bg.setWidth(6);
}




void ViewportDecorations::draw(Viewport * viewport) const
{
	this->draw_scale(viewport);
	this->draw_attributions(viewport);
	this->draw_center_mark(viewport);
	this->draw_logos(viewport);
	this->draw_viewport_data(viewport); /* Viewport bbox coordinates, viewport width and height. */
}




void ViewportDecorations::draw_scale(Viewport * viewport) const
{
	if (!viewport->scale_visibility) {
		return;
	}

	const int canvas_width = viewport->canvas.get_width();
	const int canvas_height = viewport->canvas.get_height();

	double base_distance;       /* Physical (real world) distance corresponding to full width of drawn scale. Physical units (miles, meters). */
	int HEIGHT = 20;            /* Height of scale in pixels. */
	float RELATIVE_WIDTH = 0.5; /* Width of scale, relative to width of viewport. */
	int MAXIMUM_WIDTH = canvas_width * RELATIVE_WIDTH;

	const Coord left =  viewport->screen_pos_to_coord(0,                             canvas_height / 2);
	const Coord right = viewport->screen_pos_to_coord(canvas_width * RELATIVE_WIDTH, canvas_height / 2);

	const DistanceUnit distance_unit = Preferences::get_unit_distance();
	const double l2r = Coord::distance(left, right);
	switch (distance_unit) {
	case DistanceUnit::Kilometres:
		base_distance = l2r; /* In meters. */
		break;
	case DistanceUnit::Miles:
		/* In 0.1 miles (copes better when zoomed in as 1 mile can be too big). */
		base_distance = VIK_METERS_TO_MILES (l2r) * 10.0;
		break;
	case DistanceUnit::NauticalMiles:
		/* In 0.1 NM (copes better when zoomed in as 1 NM can be too big). */
		base_distance = VIK_METERS_TO_NAUTICAL_MILES (l2r) * 10.0;
		break;
	default:
		base_distance = 1; /* Keep the compiler happy. */
		qDebug() << SG_PREFIX_E << "Invalid distance unit" << (int) distance_unit;
		break;
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


	/* Draw scale value.  We need to draw the text with outline,
	   so that it's more visible on a map that will be present in
	   the background. */
	{
		const QString scale_value = this->draw_scale_helper_value(viewport, distance_unit, scale_unit);

		const QPointF scale_start(PAD, canvas_height - PAD); /* Bottom-left corner of scale. */
		const QPointF value_start = QPointF(scale_start.x() + len + PAD, scale_start.y()); /* Bottom-left corner of value. */
		const QRectF bounding_rect = QRectF(value_start.x(), 0, value_start.x() + 300, value_start.y());

		const QFont scale_font = QFont("Helvetica", 40);

#if 1           /* Text with outline. */
		QPen outline_pen;
		outline_pen.setWidth(1);
		outline_pen.setColor(pen_bg.color());
		const QColor fill_color(pen_fg.color());
		viewport->draw_outlined_text(scale_font, outline_pen, fill_color, value_start, scale_value);

#else           /* Text without outline (old version). */
		viewport->draw_text(scale_font, pen_fg, bounding_rect, Qt::AlignBottom | Qt::AlignLeft, scale_value, 0);
#endif


#if 1
		/* Debug. */
		//QPainter painter(viewport->scr_buffer);
		viewport->canvas.painter->setPen(QColor("red"));
		viewport->canvas.painter->drawEllipse(scale_start, 3, 3);
		viewport->canvas.painter->setPen(QColor("blue"));
		viewport->canvas.painter->drawEllipse(value_start, 3, 3);
#endif
	}
}




void ViewportDecorations::draw_scale_helper_scale(Viewport * viewport, const QPen & pen, int scale_len, int h) const
{
	const int canvas_width = viewport->canvas.get_width();
	const int canvas_height = viewport->canvas.get_height();

	/* Black scale. */
	viewport->draw_line(pen, PAD,             canvas_height - PAD, PAD + scale_len, canvas_height - PAD);
	viewport->draw_line(pen, PAD,             canvas_height - PAD, PAD,             canvas_height - PAD - h);
	viewport->draw_line(pen, PAD + scale_len, canvas_height - PAD, PAD + scale_len, canvas_height - PAD - h);

	const int y1 = canvas_height - PAD;
	for (int i = 1; i < 10; i++) {
		int x1 = PAD + i * scale_len / 10;
		int diff = ((i == 5) ? (2 * h / 3) : (1 * h / 3));
		viewport->draw_line(pen, x1, y1, x1, y1 - diff);
	}
}




QString ViewportDecorations::draw_scale_helper_value(Viewport * viewport, DistanceUnit distance_unit, double scale_unit) const
{
	QString scale_value;

	switch (distance_unit) {
	case DistanceUnit::Kilometres:
		if (scale_unit >= 1000) {
			scale_value = QObject::tr("%1 km").arg((int) scale_unit / 1000);
		} else {
			scale_value = QObject::tr("%1 m").arg((int) scale_unit);
		}
		break;
	case DistanceUnit::Miles:
		/* Handle units in 0.1 miles. */
		if (scale_unit < 10.0) {
			scale_value = QObject::tr("%1 miles").arg(scale_unit / 10.0, 0, 'f', 1); /* "%0.1f" */
		} else if ((int) scale_unit == 10.0) {
			scale_value = QObject::tr("1 mile");
		} else {
			scale_value = QObject::tr("%1 miles").arg((int) (scale_unit / 10.0));
		}
		break;
	case DistanceUnit::NauticalMiles:
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
		qDebug() << SG_PREFIX_E << "Invalid distance unit" << (int) distance_unit;
		break;
	}

	return scale_value;
}




/* Draw list of attribution strings, aligning them to bottom-right corner. */
void ViewportDecorations::draw_attributions(Viewport * viewport) const
{
	const QFont font("Helvetica", 12);
	const QPen & pen = this->pen_marks_fg;

	const int font_height = viewport->fontMetrics().boundingRect("© Copyright").height();
	const int single_row_height = 1.2 * font_height;

	const int base_rect_width = viewport->canvas.get_width() - (2 * PAD);     /* The actual width will be the same for all attribution label rectangles. */
	const int base_rect_height = viewport->canvas.get_height() - (2 * PAD);   /* The actual height will be smaller and smaller for each consecutive attribution. */
	const int base_anchor_x = viewport->canvas.get_width() - (1 * PAD);       /* x coordinate of actual anchor of every rectangle will be in the same place. */
	const int base_anchor_y = viewport->canvas.get_height() - (1 * PAD);      /* y coordinate of actual anchor of every rectangle will be higher for each consecutive attribution. */

	for (int i = 0; i < this->attributions.size(); i++) {
		const int delta = (i * single_row_height);

		const int rect_width = base_rect_width;
		const int rect_height = base_rect_height - delta;
		const int anchor_x = base_anchor_x;
		const int anchor_y = base_anchor_y - delta;

		const QRectF bounding_rect = QRectF(anchor_x, anchor_y, -rect_width, -rect_height);

		viewport->draw_text(font, pen, bounding_rect, Qt::AlignBottom | Qt::AlignRight, this->attributions[i], 0);
	}
}




void ViewportDecorations::draw_center_mark(Viewport * viewport) const
{
	//qDebug() << SG_PREFIX_I << "Center mark visibility =" << viewport->center_mark_visibility;

	if (!viewport->center_mark_visibility) {
		return;
	}

	const int len = 30;
	const int gap = 4;
	const int center_x = viewport->canvas.get_width() / 2;
	const int center_y = viewport->canvas.get_height() / 2;

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
}




void ViewportDecorations::draw_logos(Viewport * viewport) const
{
	int x_pos = viewport->canvas.get_width() - PAD;
	const int y_pos = PAD;
	for (auto iter = this->logos.begin(); iter != this->logos.end(); iter++) {
		const QPixmap & logo_pixmap = iter->logo_pixmap;
		const int logo_width = logo_pixmap.width();
		const int logo_height = logo_pixmap.height();
		viewport->draw_pixmap(logo_pixmap, 0, 0, x_pos - logo_width, y_pos, logo_width, logo_height);
		x_pos = x_pos - logo_width - PAD;
	}
}



void ViewportDecorations::clear(void)
{
	this->attributions.clear();

	/* There are no pointers involved in logos, so we can safely
	   do ::clear() here. */
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




/**
   \brief Add an attribution/copyright to display on viewport

   @attribution: new attribution/copyright to display.
*/
sg_ret ViewportDecorations::add_attribution(QString const & attribution)
{
	if (!this->attributions.contains(attribution)) {
		this->attributions.push_front(attribution);
	}

	return sg_ret::ok;
}




sg_ret ViewportDecorations::add_logo(const ViewportLogo & logo)
{
	if (logo.logo_id == "") {
		qDebug() << SG_PREFIX_W << "Trying to add empty logo";
		return sg_ret::ok;
	}

	bool found = false;
	for (auto iter = this->logos.begin(); iter != this->logos.end(); iter++) {
		if (iter->logo_id == logo.logo_id) {
			found = true;
		}
	}

	if (!found) {
		this->logos.push_front(logo);
	}

	return sg_ret::ok;
}




void ViewportDecorations::draw_viewport_data(Viewport * viewport) const
{
	const LatLonBBox bbox = viewport->get_bbox();

	const QString north = "N " + bbox.north.to_string();
	const QString west =  "W " + bbox.west.to_string();
	const QString east =  "E " + bbox.east.to_string();
	const QString south = "S " + bbox.south.to_string();
	const QString size = QString("w = %1, h = %2").arg(viewport->get_width()).arg(viewport->get_height());

	const QPointF data_start(10, 10); /* Top-right corner of viewport. */
	const QRectF bounding_rect = QRectF(data_start.x(), data_start.y(), data_start.x() + 400, data_start.y() + 400);

	const QFont font = QFont("Helvetica", 10);
	const QPen & pen_fg = this->pen_marks_fg;

	viewport->draw_text(font, pen_fg, bounding_rect, Qt::AlignTop | Qt::AlignHCenter, north, 0);
	viewport->draw_text(font, pen_fg, bounding_rect, Qt::AlignVCenter | Qt::AlignRight, east, 0);
	viewport->draw_text(font, pen_fg, bounding_rect, Qt::AlignVCenter | Qt::AlignLeft, west, 0);
	viewport->draw_text(font, pen_fg, bounding_rect, Qt::AlignBottom | Qt::AlignHCenter, south, 0);

	viewport->draw_text(font, pen_fg, bounding_rect, Qt::AlignVCenter | Qt::AlignHCenter, size, 0);
}
