/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2006, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2012-2015, Rob Norris <rw_norris@hotmail.com>
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




#include <cassert>
#include <cstring>

#include "window.h"
#include "window_layer_tools.h"
#include "coords.h"
//#include "vikutils.h"
#include "icons/icons.h"




using namespace SlavGPS;




#ifdef WINDOWS
/* Hopefully Alt key by default. */
#define SG_MOVE_MODIFIER Qt::AltModifier
#else
/* Alt+mouse on Linux desktops tends to be used by the desktop manager.
   Thus use an alternate modifier - you may need to set something into this group.
   Viking used GDK_MOD5_MASK.
*/
#define SG_MOVE_MODIFIER Qt::ControlModifier
#endif




static bool draw_buf_done = true;

typedef struct {
	QWindow * window;
	//GdkGC * gdk_style;
	QPixmap * pixmap;
} draw_buf_data_t;




static int draw_buf(draw_buf_data_t * data)
{
#if 0
	gdk_threads_enter();
	gdk_draw_drawable(data->window, data->gdk_style, data->pixmap,
			  0, 0, 0, 0, -1, -1);
	draw_buf_done = true;
	gdk_threads_leave();
#endif
	return false;
}




/**
   @param x1, y1 - coordinates of beginning of ruler (start coordinates, where cursor was pressed down)
   @param x2, y2 - coordinates of end of ruler (end coordinates, where cursor currently is)
*/
void LayerToolRuler::draw(Viewport * viewport, QPixmap * pixmap, QPen & pen, int x1, int y1, int x2, int y2, double distance)
{
	qDebug() << "DD: Generic Layer Tool: Ruler: draw";
#if 0
	PangoLayout *pl;
	GdkGC *labgc = viewport->new_pen("#cccccc", 1);
	GdkGC *thickgc = gdk_gc_new(d);
#endif

	double len = sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
	double dx = (x2 - x1) / len * 10;
	double dy = (y2 - y1) / len * 10;
	double c = cos(DEG2RAD(15.0));
	double s = sin(DEG2RAD(15.0));
	double angle;
	double baseangle = 0;

	QPainter painter(pixmap);
	painter.setPen(pen);

	/* Draw line with arrow ends. */
	{
		int tmp_x1 = x1, tmp_y1 = y1, tmp_x2 = x2, tmp_y2 = y2;
		Viewport::clip_line(&tmp_x1, &tmp_y1, &tmp_x2, &tmp_y2);
		painter.drawLine(tmp_x1, tmp_y1, tmp_x2, tmp_y2);


		Viewport::clip_line(&x1, &y1, &x2, &y2);
		painter.drawLine(x1,      y1,      x2,                     y2);
		painter.drawLine(x1 - dy, y1 + dx, x1 + dy,                y1 - dx);
		painter.drawLine(x2 - dy, y2 + dx, x2 + dy,                y2 - dx);
		painter.drawLine(x2,      y2,      x2 - (dx * c + dy * s), y2 - (dy * c - dx * s));
		painter.drawLine(x2,      y2,      x2 - (dx * c - dy * s), y2 - (dy * c + dx * s));
		painter.drawLine(x1,      y1,      x1 + (dx * c + dy * s), y1 + (dy * c - dx * s));
		painter.drawLine(x1,      y1,      x1 + (dx * c - dy * s), y1 + (dy * c + dx * s));

	}


	/* Draw compass. */

#define CR 80

	/* Distance between circles. */
	const int dist = 4;

#if 1
	/* Three full circles. */
	painter.drawArc(x1 - CR + dist, y1 - CR + dist, 2 * (CR - dist), 2 * (CR - dist), 0, 16 * 360); /* Innermost. */
	painter.drawArc(x1 - CR,        y1 - CR,        2 * CR,          2 * CR,          0, 16 * 360); /* Middle. */
	painter.drawArc(x1 - CR - dist, y1 - CR - dist, 2 * (CR + dist), 2 * (CR + dist), 0, 16 * 360); /* Outermost. */
#endif




#if 1
	/* Fill between middle and innermost circle. */
	{
		// "#2255cc", GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER;

		viewport->compute_bearing(x1, y1, x2, y2, &angle, &baseangle);
		float start_angle = (90 - RAD2DEG(baseangle)) * 16;
		float span_angle = -RAD2DEG(angle) * 16;
		fprintf(stderr, "DD: Layer Tools: Ruler: draw in rectangle %d %d %d %d / %f / %f\n", x1-CR+dist/2, y1-CR+dist/2, 2*CR-dist, 2*CR-dist, start_angle, span_angle);
		QPen new_pen(QColor("red"));
		new_pen.setWidth(dist);
		painter.setPen(new_pen);
		painter.drawArc(x1 - CR + dist / 2, y1 - CR + dist / 2, 2 * CR - dist, 2 * CR - dist, start_angle, span_angle);

		painter.setPen(pen);
	}
#endif

#if 1
	/* Ticks around circles, every N degrees. */
	{
		int ticksize = 2 * dist;
		for (int i = 0; i < 180; i += 5) {
			c = cos(DEG2RAD(i) * 2 + baseangle);
			s = sin(DEG2RAD(i) * 2 + baseangle);
			painter.drawLine(x1 + (CR-dist)*c, y1 + (CR-dist)*s, x1 + (CR+ticksize)*c, y1 + (CR+ticksize)*s);
		}
	}
#endif


#if 1
	{
		/* Two axis inside a compass.
		   Varying angle will rotate the axis. I don't know why you would need this :) */
		float angle = 0;
		int c2 = (CR + dist * 2) * sin(baseangle);
		int s2 = (CR + dist * 2) * cos(baseangle);
		painter.drawLine(x1 - c2, y1 - s2, x1 + c2, y1 + s2);
		painter.drawLine(x1 + s2, y1 - c2, x1 - s2, y1 + c2);
	}
#endif




	/* Draw labels. */
	{
		painter.drawText(x1-5, y1-CR-3*dist-8, "N");
	}
#if 1
	// fill_rectangle(d, labgc, (x)-2, (y)-1, (w)+4, (h)+1);
	// draw_rectangle(d, gc, (x)-2, (y)-1, (w)+4, (h)+1);
	#define LABEL(x, y, w, h, text) {				\
		painter.drawText((x), (y), text); }
#endif

	{
		int wd, hd, xd, yd;
		int wb, hb, xb, yb;
		char str[128];

		/* Draw label with distance. */
		DistanceUnit distance_unit = a_vik_get_units_distance();
		switch (distance_unit) {
		case DistanceUnit::KILOMETRES:
			if (distance >= 1000 && distance < 100000) {
				sprintf(str, "%3.2f km", distance/1000.0);
			} else if (distance < 1000) {
				sprintf(str, "%d m", (int)distance);
			} else {
				sprintf(str, "%d km", (int)distance/1000);
			}
			break;
		case DistanceUnit::MILES:
			if (distance >= VIK_MILES_TO_METERS(1) && distance < VIK_MILES_TO_METERS(100)) {
				sprintf(str, "%3.2f miles", VIK_METERS_TO_MILES(distance));
			} else if (distance < VIK_MILES_TO_METERS(1)) {
				sprintf(str, "%d yards", (int)(distance*1.0936133));
			} else {
				sprintf(str, "%d miles", (int)VIK_METERS_TO_MILES(distance));
			}
			break;
		case DistanceUnit::NAUTICAL_MILES:
			if (distance >= VIK_NAUTICAL_MILES_TO_METERS(1) && distance < VIK_NAUTICAL_MILES_TO_METERS(100)) {
				sprintf(str, "%3.2f NM", VIK_METERS_TO_NAUTICAL_MILES(distance));
			} else if (distance < VIK_NAUTICAL_MILES_TO_METERS(1)) {
				sprintf(str, "%d yards", (int)(distance*1.0936133));
			} else {
				sprintf(str, "%d NM", (int)VIK_METERS_TO_NAUTICAL_MILES(distance));
			}
			break;
		default:
			qDebug() << "EE: Layer Tools: draw ruler: invalid distance unit" << (int) distance_unit;
		}

		/* Draw distance label. */
		{
			QRectF r1 = painter.boundingRect(QRect(0, 0, 0, 0), Qt::AlignHCenter, QString(str));
			wd = r1.width();
			hd = r1.height();
			if (dy>0) {
				xd = (x1 + x2) / 2 + dy;
				yd = (y1 + y2) / 2 - hd / 2 - dx;
			} else {
				xd = (x1 + x2) / 2 - dy;
				yd = (y1 + y2) / 2 - hd / 2 + dx;
			}

			if (xd < -5 || yd < -5 || xd > viewport->get_width() + 5 || yd > viewport->get_height() + 5) {
				xd = x2 + 10;
				yd = y2 - 5;
			}

			LABEL(xd, yd, wd, hd, QString(str));
		}

#if 1
		/* Draw bearing label. */
		{

			sprintf(str, "%3.1f°", RAD2DEG(angle));
			QRectF box = painter.boundingRect(QRect(0, 0, 0, 0), Qt::AlignHCenter, QString(str));
			wb = box.width();
			hb = box.height();

			xb = x1 + CR * cos(angle - M_PI_2);
			yb = y1 + CR * sin(angle - M_PI_2);

			if (xb < -5 || yb < -5 || xb > viewport->get_width() + 5 || yb > viewport->get_height() + 5) {
				xb = x2 + 10;
				yb = y2 + 10;
			}

			{
				QRect r1(xd - 2, yd - 1, wd + 4, hd + 1);
				QRect r2(xb - 2, yb - 1, wb + 4, hb + 1);
				if (r1.intersects(r2)) {
					xb = xd + wd + 5;
				}
			}
			LABEL(xb, yb, wb, hb, QString(str));
		}
#endif
	}

#undef LABEL

#if 0
	g_object_unref(G_OBJECT (pl));
	g_object_unref(G_OBJECT (labgc));
	g_object_unref(G_OBJECT (thickgc));
#endif
}




LayerTool * SlavGPS::ruler_create(Window * window, Viewport * viewport)
{
	return new LayerToolRuler(window, viewport);
}




LayerToolRuler::LayerToolRuler(Window * window, Viewport * viewport) : LayerTool(window, viewport, LayerType::NUM_TYPES)
{
	this->id_string = "generic.ruler";

	this->action_icon_path   = ":/icons/layer_tool/ruler_18.png";
	this->action_label       = QObject::tr("&Ruler");
	this->action_tooltip     = QObject::tr("Ruler Tool");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_U; /* Ctrl+Shift+R is used for Refresh (deemed more important), so use 'U' instead. */

	this->cursor_click = new QCursor(Qt::ArrowCursor);
	this->cursor_release = new QCursor(Qt::ArrowCursor);
	//shape = Qt::BitmapCursor;
	//this->cursor_data = &cursor_ruler_pixbuf;

	this->ruler = new ruler_tool_state_t;
}




LayerToolRuler::~LayerToolRuler()
{
	delete this->cursor_click;
	delete this->cursor_release;
	delete this->ruler;
}




LayerToolFuncStatus LayerToolRuler::click_(Layer * layer, QMouseEvent * event)
{
	qDebug() << "DD: Layer Tools: Ruler: ->click()";

	struct LatLon ll;
	VikCoord coord;
	char temp[128] = { 0 };

	if (event->button() == Qt::LeftButton) {
		char * lat = NULL;
		char * lon = NULL;
		this->viewport->screen_to_coord(event->x(), event->y(), &coord);
		vik_coord_to_latlon(&coord, &ll);
		a_coords_latlon_to_string(&ll, &lat, &lon);
		if (this->ruler->has_start_coord) {
			DistanceUnit distance_unit = a_vik_get_units_distance();
			switch (distance_unit) {
			case DistanceUnit::KILOMETRES:
				sprintf(temp, "%s %s DIFF %f meters", lat, lon, vik_coord_diff(&coord, &this->ruler->start_coord));
				break;
			case DistanceUnit::MILES:
				sprintf(temp, "%s %s DIFF %f miles", lat, lon, VIK_METERS_TO_MILES(vik_coord_diff(&coord, &this->ruler->start_coord)));
				break;
			case DistanceUnit::NAUTICAL_MILES:
				sprintf(temp, "%s %s DIFF %f NM", lat, lon, VIK_METERS_TO_NAUTICAL_MILES(vik_coord_diff(&coord, &this->ruler->start_coord)));
				break;
			default:
				sprintf(temp, "Just to keep the compiler happy");
				qDebug() << "EE: Layer Tools: Ruler click: invalid distance unit:" << (int) distance_unit;
			}

			qDebug() << "II: Layer Tools: Ruler: second click, dropping start coordinates";
			this->ruler->has_start_coord = false;
		} else {
			sprintf(temp, "%s %s", lat, lon);
			qDebug() << "II: Layer Tools: Ruler: first click, saving start coordinates";
			this->ruler->has_start_coord = true;
		}

		QString message(temp);
		this->window->get_statusbar()->set_message(StatusBarField::INFO, message);
		this->ruler->start_coord = coord;
	} else {
		this->viewport->set_center_screen(event->x(), event->y());
		this->window->draw_update_cb();
	}

	return LayerToolFuncStatus::ACK;
}




LayerToolFuncStatus LayerToolRuler::move_(Layer * layer, QMouseEvent * event)
{
	qDebug() << "DD: Layer Tools: Ruler: ->move()";

	struct LatLon ll;
	VikCoord coord;
	char temp[128] = { 0 };

	if (!this->ruler->has_start_coord) {
		qDebug() << "II: Layer Tools: Ruler: not drawing, we don't have start coordinates";
		return LayerToolFuncStatus::ACK;
	}

	static QPixmap * buf = NULL;
	char * lat = NULL;
	char * lon = NULL;
	int w1 = this->viewport->get_width();
	int h1 = this->viewport->get_height();
	if (!buf) {
		qDebug() << "II: Layer Tools: Ruler: creating new pixmap of size" << w1 << h1;
		buf = new QPixmap(w1, h1);
	} else {
		if (w1 != buf->width() || h1 != buf->height()) {
			qDebug() << "EE: Layer Tools: Ruler: discarding old pixmap, creating new pixmap of size" << w1 << h1;
			delete buf;
			buf = new QPixmap(w1, h1);
		}
	}
	buf->fill(QColor("transparent"));
	//buf->fill();

	this->viewport->screen_to_coord(event->x(), event->y(), &coord);
	vik_coord_to_latlon(&coord, &ll);

	int start_x;
	int start_y;
	this->viewport->coord_to_screen(&this->ruler->start_coord, &start_x, &start_y);

	//gdk_draw_drawable(buf, gtk_widget_get_style(this->viewport)->black_gc,
	//		  this->viewport->get_pixmap(), 0, 0, 0, 0, -1, -1);

	QPen pen("black");
	pen.setWidth(1);
	LayerToolRuler::draw(this->viewport, buf, pen, start_x, start_y, event->x(), event->y(), vik_coord_diff(&coord, &this->ruler->start_coord));

	if (draw_buf_done) {
#if 0
		static draw_buf_data_t pass_along;
		pass_along.window = gtk_widget_get_window(this->viewport);
		pass_along.gdk_style = gtk_widget_get_style(this->viewport)->black_gc;
		pass_along.pixmap = buf;
		g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, (GSourceFunc) draw_buf, (void *) &pass_along, NULL);
		draw_buf_done = false;

#else
		QPainter painter(this->viewport->scr_buffer);
		//painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
		painter.drawPixmap(0, 0, *buf);
		this->viewport->update();
		draw_buf_done = true;
#endif

	}


	a_coords_latlon_to_string(&ll, &lat, &lon);
	DistanceUnit distance_unit = a_vik_get_units_distance();
	switch (distance_unit) {
	case DistanceUnit::KILOMETRES:
		sprintf(temp, "%s %s DIFF %f meters", lat, lon, vik_coord_diff(&coord, &this->ruler->start_coord));
		break;
	case DistanceUnit::MILES:
		sprintf(temp, "%s %s DIFF %f miles", lat, lon, VIK_METERS_TO_MILES (vik_coord_diff(&coord, &this->ruler->start_coord)));
		break;
	case DistanceUnit::NAUTICAL_MILES:
		sprintf(temp, "%s %s DIFF %f NM", lat, lon, VIK_METERS_TO_NAUTICAL_MILES (vik_coord_diff(&coord, &this->ruler->start_coord)));
		break;
	default:
		sprintf(temp, "Just to keep the compiler happy");
		qDebug() << "EE: Layer Tools: Ruler move: unknown distance unit:" << (int) distance_unit;
	}

	this->window->get_statusbar()->set_message(StatusBarField::INFO, QString(temp));

	/* We have used the start coordinate to draw a ruler. The coordinate should be discarded on LMB release. */
	this->ruler->invalidate_start_coord = true;

	return LayerToolFuncStatus::ACK;
}




LayerToolFuncStatus LayerToolRuler::release_(Layer * layer, QMouseEvent * event)
{
	qDebug() << "II: Layer Tools: Ruler: ->release()";
	if (this->ruler->invalidate_start_coord) {
		/* In ->move() we have been using ->start_coord to draw a ruler.
		   Now the ->start_coord is unnecessary and should be discarded. */
		this->ruler->invalidate_start_coord = false;
		this->ruler->has_start_coord = false;
	}
	return LayerToolFuncStatus::ACK;
}




void LayerToolRuler::deactivate_(Layer * layer)
{
	qDebug() << "II: Layer Tools: Ruler: ->deactivate() called";
	this->window->draw_update_cb();
}




bool LayerToolRuler::key_press_(Layer * layer, QKeyEvent * event)
{
	if (event->key() == Qt::Key_Escape) {
#if 0
		this->ruler->invalidate_start_coord = false;
		this->ruler->has_start_coord = false;
		this->deactivate_(layer);
		return true;
#endif
	}

	/* Regardless of whether we used it, return false so other GTK things may use it. */
	return false;
}




/*
 * In case the screen size has changed
 */
void LayerToolZoom::resize_pixmap(void)
{
	/* Allocate a drawing area the size of the viewport. */
	int w1 = this->window->viewport->get_width();
	int h1 = this->window->viewport->get_height();

	if (!this->zoom->pixmap) {
		/* Totally new. */
		this->zoom->pixmap = new QPixmap(w1, h1); /* TODO: Where do we delete this? */
	} else {

		int w2 = this->zoom->pixmap->width();
		int h2 = this->zoom->pixmap->height();

		if (w1 != w2 || h1 != h2) {
			/* Has changed - delete and recreate with new values. */
			delete this->zoom->pixmap;
			this->zoom->pixmap = new QPixmap(w1, h1); /* TODO: Where do we delete this? */
		}
	}
}




LayerTool * SlavGPS::zoomtool_create(Window * window, Viewport * viewport)
{
	return new LayerToolZoom(window, viewport);
}




LayerToolZoom::LayerToolZoom(Window * window, Viewport * viewport) : LayerTool(window, viewport, LayerType::NUM_TYPES)
{
	this->id_string = "generic.zoom";

	this->action_icon_path   = ":/icons/layer_tool/zoom_18.png";
	this->action_label       = QObject::tr("&Zoom");
	this->action_tooltip     = QObject::tr("Zoom Tool");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_Z;

	this->cursor_click = new QCursor(Qt::ArrowCursor);
	this->cursor_release = new QCursor(Qt::ArrowCursor);
	//this->cursor_shape = Qt::BitmapCursor;
	//this->cursor_data = &cursor_zoom_pixbuf;

	this->zoom = new zoom_tool_state_t;
}




LayerToolZoom::~LayerToolZoom()
{
	delete this->cursor_click;
	delete this->cursor_release;
	delete this->zoom;
}




LayerToolFuncStatus LayerToolZoom::click_(Layer * layer, QMouseEvent * event)
{
	qDebug() << "DD: Layer Tools: Zoom: ->click() called";

	unsigned int modifiers = event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier);
#if 0
	this->window->modified = true;

	VikCoord coord;
	int center_x = this->window->viewport->get_width() / 2;
	int center_y = this->window->viewport->get_height() / 2;

	bool skip_update = false;

	this->zoom->bounds_active = false;

	if (modifiers == (Qt::ControlModifier | Qt::ShiftModifier)) {
		/* This zoom is on the center position. */
		this->window->viewport->set_center_screen(center_x, center_y);
		if (event->button() == Qt::LeftButton) {
			this->window->viewport->zoom_in();
		} else if (event->button() == Qt::RigthButton) {
			this->window->viewport->zoom_out();
		}
	} else if (modifiers == Qt::ControlModifier) {
		/* This zoom is to recenter on the mouse position. */
		this->window->viewport->set_center_screen(event->x(), event->y());
		if (event->button() == Qt::LeftButton) {
			this->window->viewport->zoom_in();
		} else if (event->button() == Qt::RightButton) {
			this->window->viewport->zoom_out();
		}
	} else if (modifiers == Qt::ShiftModifier) {
		/* Get start of new zoom bounds. */
		if (event->button() == Qt::LeftButton) {
			this->zoom->bounds_active = true;
			this->zoom->start_x = event->x();
			this->zoom->start_y = event->y();
			skip_update = true;
		}
	} else {
		/* Make sure mouse is still over the same point on the map when we zoom. */
		this->window->viewport->screen_to_coord(event->x(), event->y(), &coord);
		if (event->button() == Qt::LeftButton) {
			this->window->viewport->zoom_in();
		} else if (event->button() == Qt::RightButton) {
			this->window->viewport->zoom_out();
		}
		int x, y;
		this->window->viewport->coord_to_screen(&coord, &x, &y);
		this->window->viewport->set_center_screen(center_x + (x - event->x()),
							  center_y + (y - event->y()));
	}

	if (!skip_update) {
		this->window->draw_update();
	}

#endif

	return LayerToolFuncStatus::ACK;
}




LayerToolFuncStatus LayerToolZoom::move_(Layer * layer, QMouseEvent * event)
{
	unsigned int modifiers = event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier);
#if 0

	if (this->zoom->bounds_active && modifiers == Qt::ShiftModifier) {
		this->resize_pixmap();

		/* Blank out currently drawn area. */
		gdk_draw_drawable(this->zoom->pixmap,
				  gtk_widget_get_style(this->window->viewport)->black_gc,
				  this->window->viewport->get_pixmap(),
				  0, 0, 0, 0, -1, -1);

		/* Calculate new box starting point & size in pixels. */
		int xx, yy, width, height;
		if (event->y() > this->zoom->start_y) {
			yy = this->zoom->start_y;
			height = event->y() - this->zoom->start_y;
		} else {
			yy = event->y();
			height = this->zoom->start_y - event->y();
		}
		if (event->x() > this->zoom->start_x) {
			xx = this->zoom->start_x;
			width = event->x() - this->zoom->start_x;
		} else {
			xx = event->x();
			width = this->zoom->start_x - event->x();
		}

		/* Draw the box. */
		draw_rectangle(this->zoom->pixmap, gtk_widget_get_style(this->window->viewport)->black_gc, xx, yy, width, height);

		/* Only actually draw when there's time to do so. */
		if (draw_buf_done) {
			static draw_buf_data_t pass_along;
			pass_along.window = gtk_widget_get_window(this->window->viewport);
			pass_along.gdk_style = gtk_widget_get_style(this->window->viewport)->black_gc;
			pass_along.pixmap = this->zoom->pixmap;
			g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, (GSourceFunc) draw_buf, &pass_along, NULL);
			draw_buf_done = false;
		}
	} else {
		this->zoom->bounds_active = false;
	}
#endif
	return LayerToolFuncStatus::ACK;
}




LayerToolFuncStatus LayerToolZoom::release_(Layer * layer, QMouseEvent * event)
{
	unsigned int modifiers = event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier);

#if 0
	/* Ensure haven't just released on the exact same position
	   i.e. probably haven't moved the mouse at all. */
	if (this->zoom->bounds_active && modifiers == Qt::ShiftModifier
	    && (event->x() < this->zoom->start_x - 5 || event->x() > this->zoom->start_x + 5)
	    && (event->y() < this->zoom->start_y - 5 || event->y() > this->zoom->start_y + 5)) {

		VikCoord coord1, coord2;
		this->window->viewport->screen_to_coord(this->zoom->start_x, this->zoom->start_y, &coord1);
		this->window->viewport->screen_to_coord(event->x(), event->y(), &coord2);

		/* From the extend of the bounds pick the best zoom level
		   c.f. trw_layer_zoom_to_show_latlons().
		   Maybe refactor... */
		struct LatLon maxmin[2];
		vik_coord_to_latlon(&coord1, &maxmin[0]);
		vik_coord_to_latlon(&coord2, &maxmin[1]);

		vu_zoom_to_show_latlons_common(this->window->viewport->get_coord_mode(), this->window->viewport, maxmin, VIK_VIEWPORT_MIN_ZOOM, false);
	} else {
		/* When pressing shift and clicking for zoom, then jump three levels. */
		if (modifiers == Qt::ShiftModifier) {
			/* Zoom in/out by three if possible. */
			this->window->viewport->set_center_screen(event->x(), event->y());
			if (event->button() == Qt::LeftButton) {
				this->window->viewport->zoom_in();
				this->window->viewport->zoom_in();
				this->window->viewport->zoom_in();
			} else if (event->button() == Qt::RightButton) {
				this->window->viewport->zoom_out();
				this->window->viewport->zoom_out();
				this->window->viewport->zoom_out();
			}
		}
	}

	this->window->draw_update();

	/* Reset. */
	this->zoom->bounds_active = false;
#endif
	return LayerToolFuncStatus::ACK;
}




LayerTool * SlavGPS::pantool_create(Window * window, Viewport * viewport)
{
	return new LayerToolPan(window, viewport);
}




LayerToolPan::LayerToolPan(Window * window, Viewport * viewport) : LayerTool(window, viewport, LayerType::NUM_TYPES)
{
	this->id_string = "generic.pan";

	this->action_icon_path   = ":/icons/layer_tool/pan_22.png";
	this->action_label       = QObject::tr("&Pan");
	this->action_tooltip     = QObject::tr("Pan Tool");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_P;

	this->cursor_click = new QCursor(Qt::ClosedHandCursor);
	this->cursor_release = new QCursor(Qt::OpenHandCursor);
}




LayerToolPan::~LayerToolPan()
{
	delete this->cursor_click;
	delete this->cursor_release;
}




/* NB Double clicking means this gets called THREE times!!! */
LayerToolFuncStatus LayerToolPan::click_(Layer * layer, QMouseEvent * event)
{
	qDebug() << "DD: Layer Tools: Pan: ->click() called";
	this->window->modified = true;
#if 0
	if (event->type == GDK_2BUTTON_PRESS) {
		/* Zoom in / out on double click.
		   No need to change the center as that has already occurred in the first click of a double click occurrence. */
		if (event->button() == Qt::LeftButton) {
			unsigned int modifier = event->modifiers() & Qt::ShiftModifier;
			if (modifier) {
				this->window->viewport->zoom_out();
			} else {
				this->window->viewport->zoom_in();
			}
		} else if (event->button() == Qt::RightButton) {
			this->window->viewport->zoom_out();
		}

		this->window->draw_update();
	} else {
#endif

		qDebug() << "DD: Layer Tools: Pan: ->click() called, checking button";
		/* Standard pan click. */
		if (event->button() == Qt::LeftButton) {
			qDebug() << "DD: Layer Tools: Pan click: window->pan_click()";
			this->window->pan_click(event);
		}
#if 0
	}
#endif
	return LayerToolFuncStatus::ACK;
}




LayerToolFuncStatus LayerToolPan::move_(Layer * layer, QMouseEvent * event)
{
	qDebug() << "DD: Layer Tools: Pan: calling window->pan_move()";
	this->window->pan_move(event);

	return LayerToolFuncStatus::ACK;
}




LayerToolFuncStatus LayerToolPan::release_(Layer * layer, QMouseEvent * event)
{
	if (event->button() == Qt::LeftButton) {
		this->window->pan_release(event);
	}
	return LayerToolFuncStatus::ACK;
}




LayerTool * SlavGPS::selecttool_create(Window * window, Viewport * viewport)
{
	return new LayerToolSelect(window, viewport);
}




LayerToolSelect::LayerToolSelect(Window * window, Viewport * viewport) : LayerTool(window, viewport, LayerType::NUM_TYPES)
{
	this->id_string = "generic.select";

	this->action_icon_path   = ":/icons/layer_tool/select_18.png";
	this->action_label       = QObject::tr("&Select");
	this->action_tooltip     = QObject::tr("Select Tool");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_S;

	this->cursor_click = new QCursor(Qt::ArrowCursor);
	this->cursor_release = new QCursor(Qt::ArrowCursor);

	this->ed = new tool_ed_t;
}




LayerToolSelect::~LayerToolSelect()
{
	delete this->cursor_click;
	delete this->cursor_release;
	delete this->ed;
}




LayerToolFuncStatus LayerToolSelect::click_(Layer * layer, QMouseEvent * event)
{
	qDebug() << "DD: Layer Tools:" << this->id_string << "->click() called";

	this->window->select_move = false;

	/* Only allow selection on primary button. */
	if (event->button() != Qt::LeftButton) {
		return LayerToolFuncStatus::IGNORE;
	}


	if (event->modifiers() & SG_MOVE_MODIFIER) {
		this->window->pan_click(event);
	} else {
		/* Enable click to apply callback to potentially all track/waypoint layers. */
		/* Useful as we can find things that aren't necessarily in the currently selected layer. */
		std::list<Layer *> * layers = this->window->layers_panel->get_all_layers_of_type(LayerType::TRW, false); /* Don't get invisible layers. */

		bool found = false;
		for (auto iter = layers->begin(); iter != layers->end(); iter++) {
			/* Stop on first layer that reports "we clicked on some object in this layer". */
			if (layer->visible) {
				if (layer->select_click(event, this->window->viewport, this)) {
					found = true;
					break;
				}
			}
		}
		delete layers;

		if (!found) {
			/* Deselect & redraw screen if necessary to remove the highlight. */

			TreeView * tree_view = this->window->layers_panel->get_treeview();
			TreeIndex const & index = tree_view->get_selected_item();

			if (index.isValid()) {
				/* Only clear if selected thing is a TrackWaypoint layer or a sublayer. */
				TreeItemType type = tree_view->get_item_type(index);
				if (type == TreeItemType::SUBLAYER
				    || tree_view->get_layer(index)->type == LayerType::TRW) {

					tree_view->unselect(index);
					if (this->window->clear_highlight()) {
						this->window->draw_update();
					}
				}
			}
		} else {
			/* Something found - so enable movement. */
			this->window->select_move = true;
		}
	}

	return LayerToolFuncStatus::ACK;
}




LayerToolFuncStatus LayerToolSelect::move_(Layer * layer, QMouseEvent * event)
{
#if 0
	if (this->window->select_move) {
		/* Don't care about trw here. */
		if (this->ed->trw) {
			layer->select_move(event, this->viewport, tool); /* kamilFIXME: layer->select_move or trw->select_move? */
		}
	} else {
		/* Optional Panning. */
		if (event->modifiers() & SG_MOVE_MODIFIER) {
			this->window->pan_move(event);
		}
	}
#endif
	return LayerToolFuncStatus::ACK;
}




LayerToolFuncStatus LayerToolSelect::release_(Layer * layer, QMouseEvent * event)
{
#if 0
	if (this->window->select_move) {
		/* Don't care about trw here. */
		if (this->ed->trw) {
			((LayerTRW *) this->ed->trw)->select_release(event, this->viewport, tool);
		}
	}

	if (event->button() == Qt::LeftButton && (event->modifiers() & SG_MOVE_MODIFIER)) {
		this->window->pan_release(event);
	}

	/* Force pan off in case it was on. */
	this->window->pan_move_flag = false;
	this->window->pan_x = -1;
	this->window->pan_y = -1;

	/* End of this select movement. */
	this->window->select_move = false;
#endif

	if (event->button() == Qt::RightButton) {
		if (layer && layer->type == LayerType::TRW && layer->visible) {
			/* See if a TRW item is selected, and show menu for the item. */
			layer->select_tool_context_menu(event, this->window->viewport);
		}
	}

	return LayerToolFuncStatus::ACK;
}
