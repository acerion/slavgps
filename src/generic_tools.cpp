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

#include <QDebug>

#include "window.h"
#include "generic_tools.h"
#include "layer_trw.h"
#include "layer_aggregate.h"
#include "coords.h"
#include "tree_view_internal.h"
#include "vikutils.h"
#include "preferences.h"
#include "viewport_internal.h"
#include "viewport_zoom.h"
#include "layers_panel.h"




using namespace SlavGPS;




#ifdef WINDOWS
#define SG_MOVE_MODIFIER Qt::AltModifier
#else
/* Alt+mouse on Linux desktops tends to be used by the desktop manager.
   Thus use an alternate modifier - you may need to set something into this group.
   Viking used GDK_MOD5_MASK. */
#define SG_MOVE_MODIFIER Qt::ControlModifier
#endif




LayerToolContainer * GenericTools::create_tools(Window * window, Viewport * viewport)
{
	/* This method should be called only once. */
	static bool created = false;
	if (created) {
		return NULL;
	}

	auto tools = new LayerToolContainer;

	LayerTool * tool = NULL;

	tool = new LayerToolSelect(window, viewport);
	tools->insert({{ tool->id_string, tool }});

	tool = new GenericToolRuler(window, viewport);
	tools->insert({{ tool->id_string, tool }});

	tool = new GenericToolZoom(window, viewport);
	tools->insert({{ tool->id_string, tool }});

	tool = new LayerToolPan(window, viewport);
	tools->insert({{ tool->id_string, tool }});

	created = true;

	return tools;
}




/**
   @param x1, y1 - coordinates of beginning of ruler (start coordinates, where cursor was pressed down)
   @param x2, y2 - coordinates of end of ruler (end coordinates, where cursor currently is)
*/
void GenericToolRuler::draw(QPainter & painter, int x1, int y1, int x2, int y2, double distance)
{
	qDebug() << "DD: Generic Layer Tool: Ruler: draw";
#if 0
	PangoLayout *pl;
	QPen * lab_pen = this->viewport->new_pen("#cccccc", 1);
	QPen * thick_pen = gdk_gc_new(d);
#endif

	double len = sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
	double dx = (x2 - x1) / len * 10;
	double dy = (y2 - y1) / len * 10;
	double c = cos(DEG2RAD(15.0));
	double s = sin(DEG2RAD(15.0));
	double angle;
	double baseangle = 0;

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

		this->viewport->compute_bearing(x1, y1, x2, y2, &angle, &baseangle);
		float start_angle = (90 - RAD2DEG(baseangle)) * 16;
		float span_angle = -RAD2DEG(angle) * 16;
		fprintf(stderr, "DD: Generic Tool Ruler: draw in rectangle %d %d %d %d / %f / %f\n", x1-CR+dist/2, y1-CR+dist/2, 2*CR-dist, 2*CR-dist, start_angle, span_angle);
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
		//float angle = 0;
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
	// fill_rectangle(d, lab_pen, (x)-2, (y)-1, (w)+4, (h)+1);
	// draw_rectangle(d, gc, (x)-2, (y)-1, (w)+4, (h)+1);
	#define LABEL(x, y, w, h, text) {				\
		painter.drawText((x), (y), text); }
#endif

	{
		int wd, hd, xd, yd;
		int wb, hb, xb, yb;
		char str[128];

		/* Draw label with distance. */
		DistanceUnit distance_unit = Preferences::get_unit_distance();
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

			if (xd < -5 || yd < -5 || xd > this->viewport->get_width() + 5 || yd > this->viewport->get_height() + 5) {
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

			if (xb < -5 || yb < -5 || xb > this->viewport->get_width() + 5 || yb > this->viewport->get_height() + 5) {
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
	g_object_unref(G_OBJECT (lab_pen));
	g_object_unref(G_OBJECT (thick_pen));
#endif
}




GenericToolRuler::GenericToolRuler(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::NUM_TYPES)
{
	this->id_string = "sg.tool.generic.ruler";

	this->action_icon_path   = ":/icons/layer_tool/ruler_18.png";
	this->action_label       = QObject::tr("&Ruler");
	this->action_tooltip     = QObject::tr("Ruler Tool");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_U; /* Ctrl+Shift+R is used for Refresh (deemed more important), so use 'U' instead. */

	this->cursor_click = new QCursor(Qt::ArrowCursor);
	this->cursor_release = new QCursor(Qt::ArrowCursor);
	//shape = Qt::BitmapCursor;
	//this->cursor_data = &cursor_ruler_pixbuf;

	//gtk_widget_get_style(this->viewport)->black_gc,
	this->pen.setColor("black");
	this->pen.setWidth(1);
}




GenericToolRuler::~GenericToolRuler()
{
	delete this->cursor_click;
	delete this->cursor_release;
}




ToolStatus GenericToolRuler::handle_mouse_click(Layer * layer, QMouseEvent * event)
{
	qDebug() << "DD: Generic Tool Ruler: ->handle_mouse_click() called";

	if (event->button() == Qt::LeftButton) {

		QString msg;

		const Coord cursor_coord = this->viewport->screen_to_coord(event->x(), event->y());

		QString lat;
		QString lon;
		CoordUtils::to_strings(lat, lon, cursor_coord.get_latlon());

		if (this->has_start_coord) {
			DistanceUnit distance_unit = Preferences::get_unit_distance();
			switch (distance_unit) {
			case DistanceUnit::KILOMETRES:
				msg = QObject::tr("%1 %2 DIFF %3 meters").arg(lat).arg(lon).arg(Coord::distance(cursor_coord, this->start_coord));
				break;
			case DistanceUnit::MILES:
				msg = QObject::tr("%1 %2 DIFF %3 miles").arg(lat).arg(lon).arg(VIK_METERS_TO_MILES(Coord::distance(cursor_coord, this->start_coord)));
				break;
			case DistanceUnit::NAUTICAL_MILES:
				msg = QObject::tr("%1 %2 DIFF %3 NM").arg(lat).arg(lon).arg(VIK_METERS_TO_NAUTICAL_MILES(Coord::distance(cursor_coord, this->start_coord)));
				break;
			default:
				qDebug() << "EE: Generic Tool Ruler: handle mouse click: invalid distance unit:" << (int) distance_unit;
			}

			qDebug() << "II: Generic Tool Ruler: second click, dropping start coordinates";



			this->has_start_coord = false;

			/* Restore clean viewport (clean == without ruler drawn on top of it). */
			this->viewport->set_pixmap(this->orig_viewport_pixmap);
			this->viewport->update();
		} else {
			msg = QObject::tr("%1 %2").arg(lat).arg(lon);
			qDebug() << "II: Generic Tool Ruler: first click, saving start coordinates";

			/* Save clean viewport (clean == without ruler drawn on top of it). */
			this->orig_viewport_pixmap = *this->viewport->get_pixmap();

			this->has_start_coord = true;
		}

		this->window->get_statusbar()->set_message(StatusBarField::INFO, msg);
		this->start_coord = cursor_coord;
	} else {
		this->viewport->set_center_screen(event->x(), event->y());
		this->window->draw_update_cb();
	}

	return ToolStatus::ACK;
}




ToolStatus GenericToolRuler::handle_mouse_move(Layer * layer, QMouseEvent * event)
{
	qDebug() << "DD: Generic Tool Ruler: ->handle_mouse_move() called";

	if (!this->has_start_coord) {
		qDebug() << "II: Generic Tool Ruler: not drawing, we don't have start coordinates";
		return ToolStatus::ACK;
	}

	const Coord cursor_coord = this->viewport->screen_to_coord(event->x(), event->y());

	int start_x;
	int start_y;
	this->viewport->coord_to_screen(&this->start_coord, &start_x, &start_y);

	QPixmap marked_pixmap = this->orig_viewport_pixmap;
	//marked_pixmap.fill(QColor("transparent"));

	QPainter painter(&marked_pixmap);
	//painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

	this->draw(painter, start_x, start_y, event->x(), event->y(), Coord::distance(cursor_coord, this->start_coord));
	this->viewport->set_pixmap(marked_pixmap);
	this->viewport->update();

	QString lat;
	QString lon;
	CoordUtils::to_strings(lat, lon, cursor_coord.get_latlon());

	QString msg;
	DistanceUnit distance_unit = Preferences::get_unit_distance();
	switch (distance_unit) {
	case DistanceUnit::KILOMETRES:
		msg = QObject::tr("%1 %2 DIFF %3 meters").arg(lat).arg(lon).arg(Coord::distance(cursor_coord, this->start_coord));
		break;
	case DistanceUnit::MILES:
		msg = QObject::tr("%1 %2 DIFF %3 miles").arg(lat).arg(lon).arg(VIK_METERS_TO_MILES (Coord::distance(cursor_coord, this->start_coord)));
		break;
	case DistanceUnit::NAUTICAL_MILES:
		msg = QObject::tr("%1 %2 DIFF %3 NM").arg(lat).arg(lon).arg(VIK_METERS_TO_NAUTICAL_MILES (Coord::distance(cursor_coord, this->start_coord)));
		break;
	default:
		qDebug() << "EE: Generic Tool Ruler: handle mouse move: unknown distance unit:" << (int) distance_unit;
	}

	this->window->get_statusbar()->set_message(StatusBarField::INFO, msg);

	/* We have used the start coordinate to draw a ruler. The coordinate should be discarded on LMB release. */
	this->invalidate_start_coord = true;

	return ToolStatus::ACK;
}




ToolStatus GenericToolRuler::handle_mouse_release(Layer * layer, QMouseEvent * event)
{
	qDebug() << "II: Generic Tool Ruler: ->handle_mouse_release() called";
	if (this->invalidate_start_coord) {
		/* In ->move() we have been using ->start_coord to draw a ruler.
		   Now the ->start_coord is unnecessary and should be discarded. */
		this->invalidate_start_coord = false;
		this->has_start_coord = false;
	}
	return ToolStatus::ACK;
}




void GenericToolRuler::deactivate_tool(Layer * layer)
{
	qDebug() << "II: Generic Tool Ruler: ->deactivate_tool() called";
	this->window->draw_update_cb();
}




ToolStatus GenericToolRuler::handle_key_press(Layer * layer, QKeyEvent * event)
{
	if (event->key() == Qt::Key_Escape) {
		this->invalidate_start_coord = false;
		this->has_start_coord = false;

		/* Restore clean viewport (clean == without ruler drawn on top of it). */
		this->viewport->set_pixmap(this->orig_viewport_pixmap);
		this->viewport->update();

		this->deactivate_tool(layer);
		return ToolStatus::ACK;
	}

	/* Regardless of whether we used it, return false so other GTK things may use it. */
	return ToolStatus::IGNORED;
}




GenericToolZoom::GenericToolZoom(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::NUM_TYPES)
{
	this->id_string = "sg.tool.generic.zoom";

	this->action_icon_path   = ":/icons/layer_tool/zoom_18.png";
	this->action_label       = QObject::tr("&Zoom");
	this->action_tooltip     = QObject::tr("Zoom Tool");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_Z;

	this->cursor_click = new QCursor(Qt::ArrowCursor);
	this->cursor_release = new QCursor(Qt::ArrowCursor);
	//this->cursor_shape = Qt::BitmapCursor;
	//this->cursor_data = &cursor_zoom_pixbuf;
}




GenericToolZoom::~GenericToolZoom()
{
	delete this->cursor_click;
	delete this->cursor_release;
}




ToolStatus GenericToolZoom::handle_mouse_click(Layer * layer, QMouseEvent * event)
{
	qDebug() << "DD: Generic Tool Zoom: ->handle_mouse_click() called";

	const unsigned int modifiers = event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier);

	int center_x = this->viewport->get_width() / 2;
	int center_y = this->viewport->get_height() / 2;

	/* Did the zoom operation affect viewport? */
	bool redraw_viewport = false;

	this->ztr_is_active = false;

	if (modifiers == (Qt::ControlModifier | Qt::ShiftModifier)) {

		/* Location at the center of viewport will be
		   preserved (coordinate at the center before the zoom
		   and coordinate at the center after the zoom will be
		   the same). */

		if (event->button() == Qt::LeftButton) {
			this->viewport->set_center_screen(center_x, center_y);
			this->viewport->zoom_in();
			this->window->contents_modified = true;
			redraw_viewport = true;

		} else if (event->button() == Qt::RightButton) {
			this->viewport->set_center_screen(center_x, center_y);
			this->viewport->zoom_out();
			this->window->contents_modified = true;
			redraw_viewport = true;
		} else {
			/* Ignore other buttons. */
		}
	} else if (modifiers == Qt::ControlModifier) {

		/* Clicked location will be put at the center of
		   viewport (coordinate of a place under cursor before
		   zoom will be placed at the center of viewport after
		   zoom). */

		if (event->button() == Qt::LeftButton) {
			this->viewport->set_center_screen(event->x(), event->y());
			this->viewport->zoom_in();
			this->window->contents_modified = true;
			redraw_viewport = true;

		} else if (event->button() == Qt::RightButton) {
			this->viewport->set_center_screen(event->x(), event->y());
			this->viewport->zoom_out();
			this->window->contents_modified = true;
			redraw_viewport = true;
		} else {
			/* Ignore other buttons. */
		}
	} else if (modifiers == Qt::ShiftModifier) {

		/* Beginning of "zoom in to rectangle" operation.
		   Notice that there is no "zoom out to rectangle"
		   operation. Get start position of zoom bounds. */

		if (event->button() == Qt::LeftButton) {
			this->ztr_is_active = true;
			this->ztr_start_x = event->x();
			this->ztr_start_y = event->y();
			this->ztr_orig_viewport_pixmap = *this->viewport->get_pixmap();
		}

		/* No zoom action (yet), so no redrawing of viewport. */

	} else {
		/* Clicked location will be put after zoom at the same
		   position in viewport as before zoom.  Before zoom
		   the location was under cursor, and after zoom it
		   will be still under cursor. */

		int x, y;
		if (event->button() == Qt::LeftButton) {
			const Coord cursor_coord = this->viewport->screen_to_coord(event->x(), event->y());
			this->viewport->zoom_in();
			this->viewport->coord_to_screen(&cursor_coord, &x, &y);
			this->viewport->set_center_screen(center_x + (x - event->x()), center_y + (y - event->y()));
			this->window->contents_modified = true;
			redraw_viewport = true;

		} else if (event->button() == Qt::RightButton) {
			const Coord cursor_coord = this->viewport->screen_to_coord(event->x(), event->y());
			this->viewport->zoom_out();
			this->viewport->coord_to_screen(&cursor_coord, &x, &y);
			this->viewport->set_center_screen(center_x + (x - event->x()), center_y + (y - event->y()));
			this->window->contents_modified = true;
			redraw_viewport = true;

		} else {
			/* Ignore other buttons. */
		}
	}

	if (redraw_viewport) {
		this->window->draw_update();
	}


	return ToolStatus::ACK;
}




ToolStatus GenericToolZoom::handle_mouse_move(Layer * layer, QMouseEvent * event)
{
	qDebug() << "DD: Generic Tool Zoom: ->handle_mouse_move() called";

	const unsigned int modifiers = event->modifiers() & Qt::ShiftModifier;

	if (this->ztr_is_active && modifiers != Qt::ShiftModifier) {

		/* When user pressed left mouse button, he also held
		   down Shift key.  This initiated drawing a "zoom to
		   rectangle" box/bound.

		   Right now Shift key is released, so stop drawing
		   the box and abort "zoom to rectangle" procedure. */

		this->ztr_is_active = false;
		return ToolStatus::ACK;
	}

	/* Update shape and size of "zoom to rectangle" box. The box
	   may grow, shrink, change proportions, but at least one of
	   its corners is always fixed.

	   Calculate new starting point & size of the box, in
	   pixels. */

	int xx, yy, width, height;
	if (event->y() > this->ztr_start_y) {
		yy = this->ztr_start_y;
		height = event->y() - this->ztr_start_y;
	} else {
		yy = event->y();
		height = this->ztr_start_y - event->y();
	}
	if (event->x() > this->ztr_start_x) {
		xx = this->ztr_start_x;
		width = event->x() - this->ztr_start_x;
	} else {
		xx = event->x();
		width = this->ztr_start_x - event->x();
	}


	/* Draw the box on saved state of viewport and tell viewport to display it. */
	QPixmap marked_pixmap = this->ztr_orig_viewport_pixmap;
	QPainter painter(&marked_pixmap);
	QPen new_pen(QColor("red"));
	new_pen.setWidth(1);
	painter.setPen(new_pen);
	painter.drawRect(xx, yy, width, height);
	this->viewport->set_pixmap(marked_pixmap);
	this->viewport->update();

	return ToolStatus::ACK;
}




ToolStatus GenericToolZoom::handle_mouse_release(Layer * layer, QMouseEvent * event)
{
	qDebug() << "DD: Generic Tool Zoom: ->handle_mouse_release() called";

	if (event->button() != Qt::LeftButton && event->button() != Qt::RightButton) {
		return ToolStatus::IGNORED;
	}

	const unsigned int modifiers = event->modifiers() & Qt::ShiftModifier;

	/* Did the zoom operation affect viewport? */
	bool redraw_viewport = false;

	if (this->ztr_is_active) {
		/* Ensure that we haven't just released mouse button
		   on the exact same position i.e. probably haven't
		   moved the mouse at all. */
		if (modifiers == Qt::ShiftModifier && (abs(event->x() - this->ztr_start_x) >= 5) && (abs(event->y() - this->ztr_start_y) >= 5)) {

			const Coord start_coord = this->viewport->screen_to_coord(this->ztr_start_x, this->ztr_start_y);
			const Coord cursor_coord = this->viewport->screen_to_coord(event->x(), event->y());

			/* From the extend of the bounds pick the best zoom level
			   c.f. trw_layer_zoom_to_show_latlons().
			   Maybe refactor... */
			struct LatLon maxmin[2];
			maxmin[0] = start_coord.get_latlon();
			maxmin[1] = cursor_coord.get_latlon();

			vu_zoom_to_show_latlons_common(this->viewport->get_coord_mode(), this->viewport, maxmin, SG_VIEWPORT_ZOOM_MIN, false);
			redraw_viewport = true;
		}
	} else {
		/* When pressing shift and clicking for zoom, then
		   change zoom by three levels (zoom in * 3, or zoom
		   out * 3). */
		if (modifiers == Qt::ShiftModifier) {
			/* Zoom in/out by three if possible. */

			if (event->button() == Qt::LeftButton) {
				this->viewport->set_center_screen(event->x(), event->y());
				this->viewport->zoom_in();
				this->viewport->zoom_in();
				this->viewport->zoom_in();
			} else { /* Qt::RightButton */
				this->viewport->set_center_screen(event->x(), event->y());
				this->viewport->zoom_out();
				this->viewport->zoom_out();
				this->viewport->zoom_out();
			}
			redraw_viewport = true;
		}
	}

	if (redraw_viewport) {
		this->window->draw_update();
	}

	/* Reset "zoom to rectangle" tool.
	   If there was any rectangle drawn in viewport, it has
	   already been erased when zoomed-in or zoomed-out viewport
	   has been redrawn from scratch. */
	this->ztr_is_active = false;

	return ToolStatus::ACK;
}




LayerToolPan::LayerToolPan(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::NUM_TYPES)
{
	this->id_string = "sg.tool.generic.pan";

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




ToolStatus LayerToolPan::handle_mouse_click(Layer * layer, QMouseEvent * event)
{
	qDebug() << "DD: Layer Tools: Pan: ->handle_mouse_click() called";
	this->window->contents_modified = true;

	/* Standard pan click. */
	if (event->button() == Qt::LeftButton) {
		qDebug() << "DD: Layer Tools: Pan click: window->pan_click()";
		this->window->pan_click(event);
	}

	return ToolStatus::ACK;
}


ToolStatus LayerToolPan::handle_mouse_double_click(Layer * layer, QMouseEvent * event)
{
	qDebug() << "DD: Layer Tools: Pan: ->handle_mouse_double_click() called";
	this->window->contents_modified = true;

	/* Zoom in / out on double click.
	   No need to change the center as that has already occurred in the first click of a double click occurrence. */
	if (event->button() == Qt::LeftButton) {
		if (event->modifiers() & Qt::ShiftModifier) {
			this->window->viewport->zoom_out();
		} else {
			this->window->viewport->zoom_in();
		}
	} else if (event->button() == Qt::RightButton) {
		this->window->viewport->zoom_out();
	} else {
		/* Ignore other mouse buttons. */
	}

	this->window->draw_update();

	return ToolStatus::ACK;
}




ToolStatus LayerToolPan::handle_mouse_move(Layer * layer, QMouseEvent * event)
{
	qDebug() << "DD: Layer Tools: Pan: calling window->pan_move()";
	this->window->pan_move(event);

	return ToolStatus::ACK;
}




ToolStatus LayerToolPan::handle_mouse_release(Layer * layer, QMouseEvent * event)
{
	if (event->button() == Qt::LeftButton) {
		this->window->pan_release(event);
	}
	return ToolStatus::ACK;
}




LayerToolSelect::LayerToolSelect(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::NUM_TYPES)
{
	this->id_string = "sg.tool.generic.select";

	this->action_icon_path   = ":/icons/layer_tool/select_18.png";
	this->action_label       = QObject::tr("&Select");
	this->action_tooltip     = QObject::tr("Select Tool");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_S;

	this->cursor_click = new QCursor(Qt::ArrowCursor);
	this->cursor_release = new QCursor(Qt::ArrowCursor);

	this->sublayer_edit = new SublayerEdit;
}




LayerToolSelect::~LayerToolSelect()
{
	delete this->cursor_click;
	delete this->cursor_release;

	delete this->sublayer_edit;
}




ToolStatus LayerToolSelect::handle_mouse_click(Layer * layer, QMouseEvent * event)
{
	qDebug() << "DD: Layer Tools:" << this->id_string << "->handle_mouse_click() called";

	this->window->select_move = false;

	/* Only allow selection on primary button. */
	if (event->button() != Qt::LeftButton) {
		return ToolStatus::IGNORED;
	}


	if (event->modifiers() & SG_MOVE_MODIFIER) {
		this->window->pan_click(event);
	} else {
		/* TODO: the code in this branch visits (in one way or
		   the other) whole tree of layers, starting with top
		   level aggregate layer.  Should we really visit all
		   layers? Shouldn't we visit only selected items and
		   its children? */

		const bool handled = this->window->layers_panel->get_top_layer()->select_click(event, this->window->viewport, this);
		if (!handled) {
			/* Deselect & redraw screen if necessary to remove the highlight. */

			TreeView * tree_view = this->window->layers_panel->get_treeview();
			TreeItem * selected_item = tree_view->get_selected_item();
			if (selected_item) {
				/* Only clear if selected thing is a TrackWaypoint layer or a sublayer. TODO: improve this condition. */
				if (selected_item->tree_item_type == TreeItemType::SUBLAYER
				    || selected_item->to_layer()->type == LayerType::TRW) {

					tree_view->unselect(selected_item->index);
					if (this->window->clear_highlight()) {
						this->window->draw_update();
					}
				}
			}
		} else {
			/* Some layer has handled the click - so enable movement. */
			this->window->select_move = true;
		}
	}

	return ToolStatus::ACK;
}




ToolStatus LayerToolSelect::handle_mouse_move(Layer * layer, QMouseEvent * event)
{
	if (this->window->select_move) {
		/* Don't care about trw here. */
		if (this->sublayer_edit->trw) {
			layer->select_move(event, this->viewport, this); /* kamilFIXME: layer->select_move or trw->select_move? */
		}
	} else {
		/* Optional Panning. */
		if (event->modifiers() & SG_MOVE_MODIFIER) {
			this->window->pan_move(event);
		}
	}
	return ToolStatus::ACK;
}




ToolStatus LayerToolSelect::handle_mouse_release(Layer * layer, QMouseEvent * event)
{
	if (this->window->select_move) {
		/* Don't care about trw here. */
		if (this->sublayer_edit->trw) {
			((LayerTRW *) this->sublayer_edit->trw)->select_release(event, this->viewport, this);
		}
	}

	if (event->button() == Qt::LeftButton && (event->modifiers() & SG_MOVE_MODIFIER)) {
		this->window->pan_release(event);
	}

	/* Force pan off in case it was on. */
	this->window->pan_off();

	/* End of this select movement. */
	this->window->select_move = false;

	if (event->button() == Qt::RightButton) {
		if (layer && layer->type == LayerType::TRW && layer->visible) {
			/* See if a TRW item is selected, and show menu for the item. */
			layer->select_tool_context_menu(event, this->window->viewport);
		}
	}

	return ToolStatus::ACK;
}
