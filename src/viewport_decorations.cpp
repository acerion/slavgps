/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2019, Kamil Ignacak <acerion@wp.pl>
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




#define SG_MODULE "GisViewport Decorations"

/* All logo pixmaps will be scaled to the same height. */
#define MAX_LOGO_HEIGHT 24




static int rescale_unit(double * base_distance, double * scale_unit, int maximum_width);




/**
   @reviewed-on 2019-07-21
*/
GisViewportDecorations::GisViewportDecorations()
{
	this->pen_marks_fg.setColor(QColor("dimgray")); /* gray, darkgray, dimgray, slategray, lightslategray */
	this->pen_marks_fg.setWidth(2);
	this->pen_marks_bg.setColor(QColor("pink"));
	this->pen_marks_bg.setWidth(6);
}




/**
   @reviewed-on 2019-07-21
*/
void GisViewportDecorations::draw(GisViewport & gisview)
{
	this->draw_attributions(gisview);
	this->draw_logos(gisview);
	this->draw_scale(gisview);
	this->draw_center_mark(gisview);
}




/**
   @reviewed-on tbd
*/
void GisViewportDecorations::draw_scale(GisViewport & gisview) const
{
	if (!gisview.scale_visibility) {
		return;
	}

	const int central_width  = gisview.central_get_width();
	const int central_height = gisview.central_get_height();
	const int leftmost_pixel = gisview.central_get_leftmost_pixel();
	const fpixel y_center_pixel = gisview.central_get_y_center_pixel();

	double base_distance;       /* Physical (real world) distance corresponding to full width of drawn scale. Physical units (miles, meters). */
	int HEIGHT = 20;            /* Height of scale in pixels. */
	float RELATIVE_WIDTH = 0.5; /* Width of scale, relative to width of viewport. */
	int MAXIMUM_WIDTH = central_width * RELATIVE_WIDTH;

	const Coord left  = gisview.screen_pos_to_coord(leftmost_pixel,                                  y_center_pixel);
	const Coord right = gisview.screen_pos_to_coord(leftmost_pixel + central_width * RELATIVE_WIDTH, y_center_pixel);

	const DistanceType::Unit distance_unit = Preferences::get_unit_distance();
	const double l2r = Coord::distance(left, right);
	switch (distance_unit.u) {
	case DistanceType::Unit::E::Kilometres:
		base_distance = l2r; /* In meters. */
		break;
	case DistanceType::Unit::E::Miles:
		/* In 0.1 miles (copes better when zoomed in as 1 mile can be too big). */
		base_distance = VIK_METERS_TO_MILES (l2r) * 10.0;
		break;
	case DistanceType::Unit::E::NauticalMiles:
		/* In 0.1 NM (copes better when zoomed in as 1 NM can be too big). */
		base_distance = VIK_METERS_TO_NAUTICAL_MILES (l2r) * 10.0;
		break;
	default:
		base_distance = 1; /* Keep the compiler happy. */
		qDebug() << SG_PREFIX_E << "Unhandled distance unit" << distance_unit;
		break;
	}

	/* At this point "base_distance" is a distance between "left" and "right" in physical units.
	   But a scale can't have an arbitrary length (e.g. 3.07 miles, or 23.2 km),
	   it should be something like 1.00 mile or 10.00 km - a unit. */
	double scale_unit = 1; /* [km, miles, nautical miles] */

	//fprintf(stderr, "%s:%d: base_distance = %g, scale_unit = %g, MAXIMUM_WIDTH = %d\n", __FUNCTION__, __LINE__, base_distance, scale_unit, MAXIMUM_WIDTH);
	const int scale_len_px = rescale_unit(&base_distance, &scale_unit, MAXIMUM_WIDTH);
	//fprintf(stderr, "resolved scale len = %d pixels\n", scale_len_px);


	const QPen & pen_fg = this->pen_marks_fg;
	const QPen & pen_bg = this->pen_marks_bg;
	ScreenPos begin(gisview.central_get_leftmost_pixel() + this->base_padding, gisview.central_get_bottommost_pixel() - this->base_padding);

	this->draw_scale_helper_draw_scale(gisview, begin, pen_bg, scale_len_px, HEIGHT); /* Bright background. */
	this->draw_scale_helper_draw_scale(gisview, begin, pen_fg, scale_len_px, HEIGHT); /* Darker scale on the bright background. */
	if (1) { /* Debug. */
		gisview.draw_ellipse(QColor("red"), begin, 3, 3);
	}


	/* Draw scale value.  We need to draw the text with outline,
	   so that it's more visible on a map that will be present in
	   the background. */
	{
		begin.rx() += (scale_len_px + this->base_padding); /* Scale value is to the right of scale bar. */
		const QRectF bounding_rect = QRectF(begin.x(), 0, begin.x() + 300, begin.y());

		const QString scale_value = this->draw_scale_helper_get_value_string(gisview, distance_unit, scale_unit);
		const QFont scale_font = QFont("Helvetica", 40);

#if 1           /* Text with outline. */
		QPen outline_pen;
		outline_pen.setWidth(1);
		outline_pen.setColor(pen_bg.color());
		const QColor fill_color(pen_fg.color());
		gisview.draw_outlined_text(scale_font, outline_pen, fill_color, begin, scale_value);

#else           /* Text without outline (old version). */
		gisview.draw_text(scale_font, pen_fg, bounding_rect, Qt::AlignBottom | Qt::AlignLeft, scale_value, 0);
#endif


		if (1) { /* Debug. */
			gisview.draw_ellipse(QColor("blue"), begin, 3, 3);
		}
	}
}




/**
   @reviewed-on 2019-07-16
*/
void GisViewportDecorations::draw_scale_helper_draw_scale(GisViewport & gisview, const ScreenPos & begin, const QPen & pen, int scale_len_px, int bar_height_max_px) const
{
	/* Main horizontal bar. */
	gisview.draw_line(pen, begin.x(),  begin.y(), begin.x() + scale_len_px, begin.y());

	/* Vertical bars. */
	const int n = 10; /* Scale bar is divided into N parts. */
	const int y = begin.y();
	for (int i = 0; i <= n; i++) {
		const int x = begin.x() + i * scale_len_px / 10;

		int bar_height = 0;
		switch (i) {
		case 0:   /* Vertical bar at left end. */
		case n:  /* Vertical bar at right end. */
			bar_height = bar_height_max_px;
			break;
		case n/2:   /* Vertical bar in the center. */
			bar_height = 0.666 * bar_height_max_px;
			break;
		default:
			bar_height = 0.333 * bar_height_max_px;
			break;
		}

		gisview.draw_line(pen, x, y, x, y - bar_height);
	}
}




/**
   @reviewed-on tbd
*/
QString GisViewportDecorations::draw_scale_helper_get_value_string(GisViewport & gisview, const DistanceType::Unit & distance_unit, double scale_unit) const
{
	QString scale_value;

	switch (distance_unit.u) {
	case DistanceType::Unit::E::Kilometres:
		if (scale_unit >= 1000) {
			scale_value = QObject::tr("%1 km").arg((int) scale_unit / 1000);
		} else {
			scale_value = QObject::tr("%1 m").arg((int) scale_unit);
		}
		break;
	case DistanceType::Unit::E::Miles:
		/* Handle units in 0.1 miles. */
		if (scale_unit < 10.0) {
			scale_value = QObject::tr("%1 miles").arg(scale_unit / 10.0, 0, 'f', 1); /* "%0.1f" */
		} else if ((int) scale_unit == 10.0) {
			scale_value = QObject::tr("1 mile");
		} else {
			scale_value = QObject::tr("%1 miles").arg((int) (scale_unit / 10.0));
		}
		break;
	case DistanceType::Unit::E::NauticalMiles:
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
		qDebug() << SG_PREFIX_E << "Unhandled distance unit" << distance_unit;
		break;
	}

	return scale_value;
}




/**
   Draw list of attribution strings, aligning them to bottom-right corner

   @reviewed-on tbd
*/
void GisViewportDecorations::draw_attributions(GisViewport & gisview)
{
	const QFont font("Helvetica", 12);
	const QPen & pen = this->pen_marks_fg;

	const int font_height = gisview.fontMetrics().boundingRect("© Copyright").height();
	const int single_row_height = 1.2 * font_height;

	const int base_rect_width  = gisview.central_get_width() - (2 * this->base_padding);    /* The actual width will be the same for all attribution label rectangles. */
	const int base_rect_height = gisview.central_get_height() - (2 * this->base_padding);   /* The actual height will be smaller and smaller for each consecutive attribution. */
	const int base_anchor_x    = gisview.central_get_rightmost_pixel() - (1 * this->base_padding);    /* x coordinate of actual anchor of every rectangle will be in the same place. */
	const int base_anchor_y    = gisview.central_get_bottommost_pixel() - (1 * this->base_padding);   /* y coordinate of actual anchor of every rectangle will be higher for each consecutive attribution. */


	for (int i = 0; i < this->attributions.size(); i++) {
		const int delta = (i * single_row_height);

		const int rect_width = base_rect_width;
		const int rect_height = base_rect_height - delta;
		const int anchor_x = base_anchor_x;
		const int anchor_y = base_anchor_y - delta;

		QRectF bounding_rect = QRectF(anchor_x, anchor_y, -rect_width, -rect_height);
		/* "Normalize" bounding rectangle that has negative
		   width or height.  Otherwise the text will be
		   outside of the bounding rectangle. */
		bounding_rect = bounding_rect.united(bounding_rect);

		const QRectF text_rect = gisview.draw_text(font, pen, bounding_rect, Qt::AlignBottom | Qt::AlignRight, this->attributions[i]);

		if (0 == i) {
			/* Initialize. */
			this->attributions_rect = text_rect;
		} else {
			/* Expand. */
			this->attributions_rect = this->attributions_rect.united(text_rect);
		}
	}

	gisview.draw_rectangle(QPen("orange"), this->attributions_rect);
}




/**
   @reviewed-on 2019-07-18
*/
void GisViewportDecorations::draw_center_mark(GisViewport & gisview) const
{
	if (!gisview.center_mark_visibility) {
		return;
	}

	const int len = 30;
	const int gap = 5; /* Lines of center mark don't actually cross. There is a gap in lines at the intersection. */
	const fpixel center_x = gisview.central_get_x_center_pixel();
	const fpixel center_y = gisview.central_get_y_center_pixel();

	QList<const QPen *> pens = { &this->pen_marks_bg, &this->pen_marks_fg }; /* Background pen should be first. */
	for (int i = 0; i < pens.size(); i++) {
		gisview.draw_line(*pens[i], center_x - len, center_y,       center_x - gap, center_y);
		gisview.draw_line(*pens[i], center_x + gap, center_y,       center_x + len, center_y);
		gisview.draw_line(*pens[i], center_x,       center_y - len, center_x,       center_y - gap);
		gisview.draw_line(*pens[i], center_x,       center_y + gap, center_x,       center_y + len);
	}
}



/**
   @reviewed-on 2019-07-21
*/
void GisViewportDecorations::draw_logos(GisViewport & gisview) const
{
	const int padding = 2; /* Padding between logos themselves, and between logos and attributions. */

	const fpixel begin_y = 0
		+ this->attributions_rect.y() /* Logos are drawn above attributions. */
		- padding                     /* The same padding as between logos should be also kept between logos and attribution. */
		- MAX_LOGO_HEIGHT;            /* Ensure that begin_y is correctly located in Qt's "beginning in bottom-left" coordinate system. */

	/*
	  I could calculate starting value of begin_x in some other
	  way, but I decided that it will look good if group of logos
	  is right-aligned with attributions, so I chose this way.

	  begin_x is now at maximum right position, and we will move
	  it to left for each drawn logo.
	*/
	fpixel begin_x = this->attributions_rect.x() + this->attributions_rect.width();

	for (auto iter = this->logos.begin(); iter != this->logos.end(); iter++) {
		const QPixmap & logo_pixmap = iter->logo_pixmap;
		const QRect logo_rect = logo_pixmap.rect();

		begin_x -= (logo_rect.width() + padding); /* Each new logo is to the left of previous one. */

		const QRect viewport_rect(begin_x,
					  /* begin_y is adjusted for
					     small logos, so that all
					     of them (small and
					     regular) are aligned to
					     bottom line. */
					  begin_y + (MAX_LOGO_HEIGHT - logo_rect.height()),
					  logo_rect.width(),
					  logo_rect.height());

		gisview.draw_pixmap(logo_pixmap, viewport_rect, logo_rect);
	}
}




/**
   @reviewed-on tbd
*/
void GisViewportDecorations::clear(void)
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

   @reviewed-on 2019-07-18
*/
sg_ret GisViewportDecorations::add_attribution(QString const & attribution)
{
	if (!this->attributions.contains(attribution)) {
		this->attributions.push_front(attribution);
	}

	return sg_ret::ok;
}




/**
   @reviewed-on 2019-07-21
*/
sg_ret GisViewportDecorations::add_logo(const GisViewportLogo & logo)
{
	if (logo.logo_id.isEmpty()) {
		qDebug() << SG_PREFIX_E << "Trying to add logo with empty id";
		return sg_ret::err;
	}

	bool found = false;
	for (auto iter = this->logos.begin(); iter != this->logos.end(); iter++) {
		if (iter->logo_id == logo.logo_id) {
			found = true;
		}
	}

	if (!found) {
		/* We copy to be able to scale down. Never scale up. */
		GisViewportLogo copy = logo;
		if (copy.logo_pixmap.height() > MAX_LOGO_HEIGHT) {
			copy.logo_pixmap = logo.logo_pixmap.scaledToHeight(MAX_LOGO_HEIGHT, Qt::SmoothTransformation);
		}
		this->logos.push_back(copy);
	}

	return sg_ret::ok;
}
