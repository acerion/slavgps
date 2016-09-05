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




#include <cstring>

#include <gdk/gdkkeysyms.h>

#include "window.h"
#include "window_layer_tools.h"
#include "coords.h"
#include "vikutils.h"
#include "icons/icons.h"




#ifndef N_
#define N_(s) s
#endif




using namespace SlavGPS;




LayerToolsBox::~LayerToolsBox()
{
	for (int tt = 0; tt < this->n_tools; tt++) {
		delete this->tools[tt];
	}
}




int LayerToolsBox::add_tool(LayerTool * layer_tool)
{
	toolbar_action_tool_entry_register(this->window->viking_vtb, &layer_tool->radioActionEntry);

	this->tools.push_back(layer_tool);
	this->n_tools++;

	return this->n_tools;
}




LayerTool * LayerToolsBox::get_tool(char const *tool_name)
{
	for (int i = 0; i < this->n_tools; i++) {
		if (0 == strcmp(tool_name, this->tools[i]->radioActionEntry.name)) {
			return this->tools[i];
		}
	}
	return NULL;
}




void LayerToolsBox::activate(char const *tool_name)
{
	LayerTool * tool = this->get_tool(tool_name);
	Layer * layer = this->window->layers_panel->get_selected_layer();
#if 0
	if (!layer) {
		return;
	}
#endif

	if (!tool) {
		fprintf(stderr, "CRITICAL: trying to activate a non-existent tool...\n");
		return;
	}
	/* Is the tool already active? */
	if (this->active_tool == tool) {
		return;
	}

	if (this->active_tool) {
		if (this->active_tool->deactivate) {
			this->active_tool->deactivate(NULL, this->active_tool);
		}
	}
	if (tool->activate) {
		tool->activate(layer, tool);
	}
	this->active_tool = tool;
}




const GdkCursor * LayerToolsBox::get_cursor(char const *tool_name)
{
	LayerTool * tool = this->get_tool(tool_name);
	if (tool->cursor == NULL) {
		if (tool->cursor_type == GDK_CURSOR_IS_PIXMAP && tool->cursor_data != NULL) {
			GdkPixbuf *cursor_pixbuf = gdk_pixbuf_from_pixdata(tool->cursor_data, false, NULL);
			/* TODO: settable offeset. */
			tool->cursor = gdk_cursor_new_from_pixbuf(gdk_display_get_default(), cursor_pixbuf, 3, 3);
			g_object_unref(G_OBJECT(cursor_pixbuf));
		} else {
			tool->cursor = gdk_cursor_new(tool->cursor_type);
		}
	}
	return tool->cursor;
}




void LayerToolsBox::click(GdkEventButton * event)
{
	Layer * layer = this->window->layers_panel->get_selected_layer();
#if 1
	if (!layer) {
		return;
	}
#endif

	if (this->active_tool && this->active_tool->click) {
		LayerType ltype = this->active_tool->layer_type;
		if (ltype == LayerType::NUM_TYPES || (layer && ltype == layer->type)) {
			this->active_tool->click(layer, event, this->active_tool);
		}
	}
}




void LayerToolsBox::move(GdkEventMotion * event)
{
	Layer * layer = this->window->layers_panel->get_selected_layer();
#if 1
	if (!layer) {
		return;
	}
#endif

	if (this->active_tool && this->active_tool->move) {
		LayerType ltype = this->active_tool->layer_type;
		if (ltype == LayerType::NUM_TYPES || (layer && ltype == layer->type)) {
			if (VIK_LAYER_TOOL_ACK_GRAB_FOCUS == this->active_tool->move(layer, event, this->active_tool)) {
				gtk_widget_grab_focus(this->window->viewport->get_toolkit_widget());
			}
		}
	}
}




void LayerToolsBox::release(GdkEventButton * event)
{
	Layer * layer = this->window->layers_panel->get_selected_layer();
#if 1
	if (!layer) {
		return;
	}
#endif

	if (this->active_tool && this->active_tool->release) {
		LayerType ltype = this->active_tool->layer_type;
		if (ltype == LayerType::NUM_TYPES || (layer && ltype == layer->type)) {
			this->active_tool->release(layer, event, this->active_tool);
		}
	}
}




/********************************************************************************
 ** Ruler tool code
 ********************************************************************************/




static bool draw_buf_done = true;

typedef struct {
	GdkDrawable * gdk_window;
	GdkGC * gdk_style;
	GdkPixmap * pixmap;
} draw_buf_data_t;




static int draw_buf(draw_buf_data_t * data)
{
	gdk_threads_enter();
	gdk_draw_drawable(data->gdk_window, data->gdk_style, data->pixmap,
			  0, 0, 0, 0, -1, -1);
	draw_buf_done = true;
	gdk_threads_leave();
	return false;
}




static VikLayerToolFuncStatus ruler_click(Layer * layer, GdkEventButton * event, LayerTool * tool);
static VikLayerToolFuncStatus ruler_move(Layer * layer, GdkEventMotion * event, LayerTool * tool);
static VikLayerToolFuncStatus ruler_release(Layer * layer, GdkEventButton * event, LayerTool * tool);
static void ruler_deactivate(Layer * layer, LayerTool * tool);
static bool ruler_key_press(Layer * layer, GdkEventKey *event, LayerTool * tool);




static void draw_ruler(Viewport * viewport, GdkDrawable *d, GdkGC *gc, int x1, int y1, int x2, int y2, double distance)
{
	PangoLayout *pl;
	char str[128];
	GdkGC *labgc = viewport->new_gc("#cccccc", 1);
	GdkGC *thickgc = gdk_gc_new(d);

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
		gdk_draw_line(d, gc, tmp_x1, tmp_y1, tmp_x2, tmp_y2);
	}

	Viewport::clip_line(&x1, &y1, &x2, &y2);
	gdk_draw_line(d, gc, x1,      y1,      x2,                     y2);
	gdk_draw_line(d, gc, x1 - dy, y1 + dx, x1 + dy,                y1 - dx);
	gdk_draw_line(d, gc, x2 - dy, y2 + dx, x2 + dy,                y2 - dx);
	gdk_draw_line(d, gc, x2,      y2,      x2 - (dx * c + dy * s), y2 - (dy * c - dx * s));
	gdk_draw_line(d, gc, x2,      y2,      x2 - (dx * c - dy * s), y2 - (dy * c + dx * s));
	gdk_draw_line(d, gc, x1,      y1,      x1 + (dx * c + dy * s), y1 + (dy * c - dx * s));
	gdk_draw_line(d, gc, x1,      y1,      x1 + (dx * c - dy * s), y1 + (dy * c + dx * s));

	/* Draw compass. */
#define CR 80
#define CW 4

	viewport->compute_bearing(x1, y1, x2, y2, &angle, &baseangle);

	{
		GdkColor color;
		gdk_gc_copy(thickgc, gc);
		gdk_gc_set_line_attributes(thickgc, CW, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER);
		gdk_color_parse("#2255cc", &color);
		gdk_gc_set_rgb_fg_color(thickgc, &color);
	}
	gdk_draw_arc(d, thickgc, false, x1-CR+CW/2, y1-CR+CW/2, 2*CR-CW, 2*CR-CW, (90 - RAD2DEG(baseangle))*64, -RAD2DEG(angle)*64);


	gdk_gc_copy(thickgc, gc);
	gdk_gc_set_line_attributes(thickgc, 2, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER);
	for (int i = 0; i < 180; i++) {
		c = cos(DEG2RAD(i)*2 + baseangle);
		s = sin(DEG2RAD(i)*2 + baseangle);

		if (i % 5) {
			gdk_draw_line(d, gc, x1 + CR*c, y1 + CR*s, x1 + (CR+CW)*c, y1 + (CR+CW)*s);
		} else {
			double ticksize = 2*CW;
			gdk_draw_line(d, thickgc, x1 + (CR-CW)*c, y1 + (CR-CW)*s, x1 + (CR+ticksize)*c, y1 + (CR+ticksize)*s);
		}
	}

	gdk_draw_arc(d, gc, false, x1-CR, y1-CR, 2*CR, 2*CR, 0, 64*360);
	gdk_draw_arc(d, gc, false, x1-CR-CW, y1-CR-CW, 2*(CR+CW), 2*(CR+CW), 0, 64*360);
	gdk_draw_arc(d, gc, false, x1-CR+CW, y1-CR+CW, 2*(CR-CW), 2*(CR-CW), 0, 64*360);
	c = (CR+CW*2)*cos(baseangle);
	s = (CR+CW*2)*sin(baseangle);
	gdk_draw_line(d, gc, x1-c, y1-s, x1+c, y1+s);
	gdk_draw_line(d, gc, x1+s, y1-c, x1-s, y1+c);

	/* Draw labels. */
#define LABEL(x, y, w, h) {						\
		gdk_draw_rectangle(d, labgc, true, (x)-2, (y)-1, (w)+4, (h)+1); \
		gdk_draw_rectangle(d, gc, false, (x)-2, (y)-1, (w)+4, (h)+1); \
		gdk_draw_layout(d, gc, (x), (y), pl); }

	{
		int wd, hd, xd, yd;
		int wb, hb, xb, yb;

		pl = gtk_widget_create_pango_layout(viewport->get_toolkit_widget(), NULL);
		pango_layout_set_font_description(pl, gtk_widget_get_style(viewport->get_toolkit_widget())->font_desc);
		pango_layout_set_text(pl, "N", -1);
		gdk_draw_layout(d, gc, x1-5, y1-CR-3*CW-8, pl);

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
			fprintf(stderr, "CRITICAL: invalid distance unit %d\n", distance_unit);
		}

		pango_layout_set_text(pl, str, -1);

		pango_layout_get_pixel_size(pl, &wd, &hd);
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

		LABEL(xd, yd, wd, hd);

		/* Draw label with bearing. */
		sprintf(str, "%3.1f°", RAD2DEG(angle));
		pango_layout_set_text(pl, str, -1);
		pango_layout_get_pixel_size(pl, &wb, &hb);
		xb = x1 + CR * cos(angle - M_PI_2);
		yb = y1 + CR * sin(angle - M_PI_2);

		if (xb < -5 || yb < -5 || xb > viewport->get_width() + 5 || yb > viewport->get_height() + 5) {
			xb = x2 + 10;
			yb = y2 + 10;
		}

		{
			GdkRectangle r1 = {xd - 2, yd - 1, wd + 4, hd + 1}, r2 = {xb - 2, yb - 1, wb + 4, hb + 1};
			if (gdk_rectangle_intersect(&r1, &r2, &r2)) {
				xb = xd + wd + 5;
			}
		}
		LABEL(xb, yb, wb, hb);
	}
#undef LABEL

	g_object_unref(G_OBJECT (pl));
	g_object_unref(G_OBJECT (labgc));
	g_object_unref(G_OBJECT (thickgc));
}




LayerTool * SlavGPS::ruler_create(Window * window, Viewport * viewport)
{
	LayerTool * layer_tool = new LayerTool(window, viewport, LayerType::NUM_TYPES);

	layer_tool->layer_type = LayerType::NUM_TYPES;

	layer_tool->radioActionEntry.name = strdup("Ruler");
	layer_tool->radioActionEntry.stock_id = strdup("vik-icon-ruler");
	layer_tool->radioActionEntry.label = strdup(N_("_Ruler"));
	layer_tool->radioActionEntry.accelerator = strdup("<control><shift>U"); /* Ctrl+Shift+R is used for Refresh (deemed more important), so use 'U' instead. */
	layer_tool->radioActionEntry.tooltip = strdup(N_("Ruler Tool"));
	layer_tool->radioActionEntry.value = 2;

	layer_tool->deactivate = ruler_deactivate;
	layer_tool->click = (VikToolMouseFunc) ruler_click;
	layer_tool->move = (VikToolMouseMoveFunc) ruler_move;
	layer_tool->release = (VikToolMouseFunc) ruler_release;
	layer_tool->key_press = ruler_key_press;

	layer_tool->cursor_type = GDK_CURSOR_IS_PIXMAP;
	layer_tool->cursor_data = &cursor_ruler_pixbuf;

	layer_tool->ruler = (ruler_tool_state_t *) malloc(1 * sizeof (ruler_tool_state_t));
	memset(layer_tool->ruler, 0, sizeof (ruler_tool_state_t));

	return layer_tool;
}




static VikLayerToolFuncStatus ruler_click(Layer * layer, GdkEventButton * event, LayerTool * tool)
{
	struct LatLon ll;
	VikCoord coord;
	char temp[128] = { 0 };
	if (event->button == MouseButton::LEFT) {
		char *lat = NULL, *lon = NULL;
		tool->viewport->screen_to_coord((int) event->x, (int) event->y, &coord);
		vik_coord_to_latlon(&coord, &ll);
		a_coords_latlon_to_string(&ll, &lat, &lon);
		if (tool->ruler->has_oldcoord) {
			DistanceUnit distance_unit = a_vik_get_units_distance();
			switch (distance_unit) {
			case DistanceUnit::KILOMETRES:
				sprintf(temp, "%s %s DIFF %f meters", lat, lon, vik_coord_diff(&coord, &(tool->ruler->oldcoord)));
				break;
			case DistanceUnit::MILES:
				sprintf(temp, "%s %s DIFF %f miles", lat, lon, VIK_METERS_TO_MILES(vik_coord_diff(&coord, &(tool->ruler->oldcoord))));
				break;
			case DistanceUnit::NAUTICAL_MILES:
				sprintf(temp, "%s %s DIFF %f NM", lat, lon, VIK_METERS_TO_NAUTICAL_MILES(vik_coord_diff(&coord, &(tool->ruler->oldcoord))));
				break;
			default:
				sprintf(temp, "Just to keep the compiler happy");
				fprintf(stderr, "CRITICAL: Houston, we've had a problem. distance=%d\n", distance_unit);
			}

			tool->ruler->has_oldcoord = false;
		} else {
			sprintf(temp, "%s %s", lat, lon);
			tool->ruler->has_oldcoord = true;
		}

		vik_statusbar_set_message(tool->window->viking_vs, VIK_STATUSBAR_INFO, temp);

		tool->ruler->oldcoord = coord;
	} else {
		tool->viewport->set_center_screen((int) event->x, (int) event->y);
		tool->window->draw_update();
	}
	return VIK_LAYER_TOOL_ACK;
}




static VikLayerToolFuncStatus ruler_move(Layer * layer, GdkEventMotion * event, LayerTool * tool)
{
	Window * window = tool->window;

	struct LatLon ll;
	VikCoord coord;
	char temp[128] = { 0 };

	if (tool->ruler->has_oldcoord) {
		static GdkPixmap *buf = NULL;
		char *lat = NULL, *lon = NULL;
		int w1 = tool->viewport->get_width();
		int h1 = tool->viewport->get_height();
		if (!buf) {
			buf = gdk_pixmap_new(gtk_widget_get_window(tool->viewport->get_toolkit_widget()), w1, h1, -1);
		}

		int w2, h2;
		gdk_drawable_get_size(buf, &w2, &h2);
		if (w1 != w2 || h1 != h2) {
			g_object_unref(G_OBJECT (buf));
			buf = gdk_pixmap_new(gtk_widget_get_window(tool->viewport->get_toolkit_widget()), w1, h1, -1);
		}

		tool->viewport->screen_to_coord((int) event->x, (int) event->y, &coord);
		vik_coord_to_latlon(&coord, &ll);
		int oldx, oldy;
		tool->viewport->coord_to_screen(&tool->ruler->oldcoord, &oldx, &oldy);

		gdk_draw_drawable(buf, gtk_widget_get_style(tool->viewport->get_toolkit_widget())->black_gc,
				  tool->viewport->get_pixmap(), 0, 0, 0, 0, -1, -1);
		draw_ruler(tool->viewport, buf, gtk_widget_get_style(tool->viewport->get_toolkit_widget())->black_gc, oldx, oldy, event->x, event->y, vik_coord_diff(&coord, &(tool->ruler->oldcoord)));
		if (draw_buf_done) {
			static draw_buf_data_t pass_along;
			pass_along.gdk_window = gtk_widget_get_window(tool->viewport->get_toolkit_widget());
			pass_along.gdk_style = gtk_widget_get_style(tool->viewport->get_toolkit_widget())->black_gc;
			pass_along.pixmap = buf;
			g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, (GSourceFunc) draw_buf, (void *) &pass_along, NULL);
			draw_buf_done = false;
		}
		a_coords_latlon_to_string(&ll, &lat, &lon);
		DistanceUnit distance_unit = a_vik_get_units_distance();
		switch (distance_unit) {
		case DistanceUnit::KILOMETRES:
			sprintf(temp, "%s %s DIFF %f meters", lat, lon, vik_coord_diff(&coord, &(tool->ruler->oldcoord)));
			break;
		case DistanceUnit::MILES:
			sprintf(temp, "%s %s DIFF %f miles", lat, lon, VIK_METERS_TO_MILES (vik_coord_diff(&coord, &(tool->ruler->oldcoord))));
			break;
		case DistanceUnit::NAUTICAL_MILES:
			sprintf(temp, "%s %s DIFF %f NM", lat, lon, VIK_METERS_TO_NAUTICAL_MILES (vik_coord_diff(&coord, &(tool->ruler->oldcoord))));
			break;
		default:
			sprintf(temp, "Just to keep the compiler happy");
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. distance=%d\n", distance_unit);
		}
		vik_statusbar_set_message(window->viking_vs, VIK_STATUSBAR_INFO, temp);
	}
	return VIK_LAYER_TOOL_ACK;
}




static VikLayerToolFuncStatus ruler_release(Layer * layer, GdkEventButton * event, LayerTool * tool)
{
	return VIK_LAYER_TOOL_ACK;
}




static void ruler_deactivate(Layer * layer, LayerTool * tool)
{
	tool->window->draw_update();
}




static bool ruler_key_press(Layer * layer, GdkEventKey *event, LayerTool * tool)
{
	if (event->keyval == GDK_Escape) {
		tool->ruler->has_oldcoord = false;
		ruler_deactivate(layer, tool);
		return true;
	}
	/* Regardless of whether we used it, return false so other GTK things may use it. */
	return false;
}

/*** End ruler code. ********************************************************/




/********************************************************************************
 ** Zoom tool code
 ********************************************************************************/




static VikLayerToolFuncStatus zoomtool_click(Layer * layer, GdkEventButton * event, LayerTool * tool);
static VikLayerToolFuncStatus zoomtool_move(Layer * layer, GdkEventMotion * event, LayerTool * tool);
static VikLayerToolFuncStatus zoomtool_release(Layer * layer, GdkEventButton * event, LayerTool * tool);




/*
 * In case the screen size has changed
 */
static void zoomtool_resize_pixmap(LayerTool * tool)
{
	/* Allocate a drawing area the size of the viewport. */
	int w1 = tool->window->viewport->get_width();
	int h1 = tool->window->viewport->get_height();

	if (!tool->zoom->pixmap) {
		/* Totally new. */
		tool->zoom->pixmap = gdk_pixmap_new(gtk_widget_get_window(tool->window->viewport->get_toolkit_widget()), w1, h1, -1);
	}

	int w2, h2;
	gdk_drawable_get_size(tool->zoom->pixmap, &w2, &h2);

	if (w1 != w2 || h1 != h2) {
		/* Has changed - delete and recreate with new values. */
		g_object_unref(G_OBJECT (tool->zoom->pixmap));
		tool->zoom->pixmap = gdk_pixmap_new(gtk_widget_get_window(tool->window->viewport->get_toolkit_widget()), w1, h1, -1);
	}
}




LayerTool * SlavGPS::zoomtool_create(Window * window, Viewport * viewport)
{
	LayerTool * layer_tool = new LayerTool(window, viewport, LayerType::NUM_TYPES);

	layer_tool->layer_type = LayerType::NUM_TYPES;

	layer_tool->radioActionEntry.name = strdup("Zoom");
	layer_tool->radioActionEntry.stock_id = strdup("vik-icon-zoom");
	layer_tool->radioActionEntry.label = strdup(N_("_Zoom"));
	layer_tool->radioActionEntry.accelerator = strdup("<control><shift>Z");
	layer_tool->radioActionEntry.tooltip = strdup(N_("Zoom Tool"));
	layer_tool->radioActionEntry.value = 1;

	layer_tool->click = (VikToolMouseFunc) zoomtool_click;
	layer_tool->move = (VikToolMouseMoveFunc) zoomtool_move;
	layer_tool->release = (VikToolMouseFunc) zoomtool_release;

	layer_tool->cursor_type = GDK_CURSOR_IS_PIXMAP;
	layer_tool->cursor_data = &cursor_zoom_pixbuf;

	layer_tool->zoom = (zoom_tool_state_t *) malloc(1 * sizeof (zoom_tool_state_t));
	memset(layer_tool->zoom, 0, sizeof (zoom_tool_state_t));

	return layer_tool;
}




static VikLayerToolFuncStatus zoomtool_click(Layer * layer, GdkEventButton * event, LayerTool * tool)
{
	tool->window->modified = true;
	unsigned int modifiers = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK);

	VikCoord coord;
	int center_x = tool->window->viewport->get_width() / 2;
	int center_y = tool->window->viewport->get_height() / 2;

	bool skip_update = false;

	tool->zoom->bounds_active = false;

	if (modifiers == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) {
		/* This zoom is on the center position. */
		tool->window->viewport->set_center_screen(center_x, center_y);
		if (event->button == MouseButton::LEFT) {
			tool->window->viewport->zoom_in();
		} else if (event->button == MouseButton::RIGHT) {
			tool->window->viewport->zoom_out();
		}
	} else if (modifiers == GDK_CONTROL_MASK) {
		/* This zoom is to recenter on the mouse position. */
		tool->window->viewport->set_center_screen((int) event->x, (int) event->y);
		if (event->button == MouseButton::LEFT) {
			tool->window->viewport->zoom_in();
		} else if (event->button == MouseButton::RIGHT) {
			tool->window->viewport->zoom_out();
		}
	} else if (modifiers == GDK_SHIFT_MASK) {
		/* Get start of new zoom bounds. */
		if (event->button == MouseButton::LEFT) {
			tool->zoom->bounds_active = true;
			tool->zoom->start_x = (int) event->x;
			tool->zoom->start_y = (int) event->y;
			skip_update = true;
		}
	} else {
		/* Make sure mouse is still over the same point on the map when we zoom. */
		tool->window->viewport->screen_to_coord(event->x, event->y, &coord);
		if (event->button == MouseButton::LEFT) {
			tool->window->viewport->zoom_in();
		} else if (event->button == MouseButton::RIGHT) {
			tool->window->viewport->zoom_out();
		}
		int x, y;
		tool->window->viewport->coord_to_screen(&coord, &x, &y);
		tool->window->viewport->set_center_screen(center_x + (x - event->x),
							  center_y + (y - event->y));
	}

	if (!skip_update) {
		tool->window->draw_update();
	}

	return VIK_LAYER_TOOL_ACK;
}




static VikLayerToolFuncStatus zoomtool_move(Layer * layer, GdkEventMotion * event, LayerTool * tool)
{
	unsigned int modifiers = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK);

	if (tool->zoom->bounds_active && modifiers == GDK_SHIFT_MASK) {
		zoomtool_resize_pixmap(tool);

		/* Blank out currently drawn area. */
		gdk_draw_drawable(tool->zoom->pixmap,
				  gtk_widget_get_style(tool->window->viewport->get_toolkit_widget())->black_gc,
				  tool->window->viewport->get_pixmap(),
				  0, 0, 0, 0, -1, -1);

		/* Calculate new box starting point & size in pixels. */
		int xx, yy, width, height;
		if (event->y > tool->zoom->start_y) {
			yy = tool->zoom->start_y;
			height = event->y - tool->zoom->start_y;
		} else {
			yy = event->y;
			height = tool->zoom->start_y - event->y;
		}
		if (event->x > tool->zoom->start_x) {
			xx = tool->zoom->start_x;
			width = event->x - tool->zoom->start_x;
		} else {
			xx = event->x;
			width = tool->zoom->start_x - event->x;
		}

		/* Draw the box. */
		gdk_draw_rectangle(tool->zoom->pixmap, gtk_widget_get_style(tool->window->viewport->get_toolkit_widget())->black_gc, false, xx, yy, width, height);

		/* Only actually draw when there's time to do so. */
		if (draw_buf_done) {
			static draw_buf_data_t pass_along;
			pass_along.gdk_window = gtk_widget_get_window(tool->window->viewport->get_toolkit_widget());
			pass_along.gdk_style = gtk_widget_get_style(tool->window->viewport->get_toolkit_widget())->black_gc;
			pass_along.pixmap = tool->zoom->pixmap;
			g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, (GSourceFunc) draw_buf, &pass_along, NULL);
			draw_buf_done = false;
		}
	} else {
		tool->zoom->bounds_active = false;
	}

	return VIK_LAYER_TOOL_ACK;
}




static VikLayerToolFuncStatus zoomtool_release(Layer * layer, GdkEventButton * event, LayerTool * tool)
{
	unsigned int modifiers = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK);

	/* Ensure haven't just released on the exact same position
	   i.e. probably haven't moved the mouse at all. */
	if (tool->zoom->bounds_active && modifiers == GDK_SHIFT_MASK
	    && (event->x < tool->zoom->start_x-5 || event->x > tool->zoom->start_x+5)
	    && (event->y < tool->zoom->start_y-5 || event->y > tool->zoom->start_y+5)) {

		VikCoord coord1, coord2;
		tool->window->viewport->screen_to_coord(tool->zoom->start_x, tool->zoom->start_y, &coord1);
		tool->window->viewport->screen_to_coord(event->x, event->y, &coord2);

		/* From the extend of the bounds pick the best zoom level
		   c.f. trw_layer_zoom_to_show_latlons().
		   Maybe refactor... */
		struct LatLon maxmin[2];
		vik_coord_to_latlon(&coord1, &maxmin[0]);
		vik_coord_to_latlon(&coord2, &maxmin[1]);

		vu_zoom_to_show_latlons_common(tool->window->viewport->get_coord_mode(), tool->window->viewport, maxmin, VIK_VIEWPORT_MIN_ZOOM, false);
	} else {
		/* When pressing shift and clicking for zoom, then jump three levels. */
		if (modifiers == GDK_SHIFT_MASK) {
			/* Zoom in/out by three if possible. */
			tool->window->viewport->set_center_screen(event->x, event->y);
			if (event->button == MouseButton::LEFT) {
				tool->window->viewport->zoom_in();
				tool->window->viewport->zoom_in();
				tool->window->viewport->zoom_in();
			} else if (event->button == MouseButton::RIGHT) {
				tool->window->viewport->zoom_out();
				tool->window->viewport->zoom_out();
				tool->window->viewport->zoom_out();
			}
		}
	}

	tool->window->draw_update();

	/* Reset. */
	tool->zoom->bounds_active = false;

	return VIK_LAYER_TOOL_ACK;
}


/*** End zoom code. ********************************************************/




/********************************************************************************
 ** Pan tool code
 ********************************************************************************/




static VikLayerToolFuncStatus pantool_click(Layer * layer, GdkEventButton * event, LayerTool * tool);
static VikLayerToolFuncStatus pantool_move(Layer * layer, GdkEventMotion * event, LayerTool * tool);
static VikLayerToolFuncStatus pantool_release(Layer * layer, GdkEventButton * event, LayerTool * tool);




LayerTool * SlavGPS::pantool_create(Window * window, Viewport * viewport)
{
	LayerTool * layer_tool = new LayerTool(window, viewport, LayerType::NUM_TYPES);

	layer_tool->layer_type = LayerType::NUM_TYPES;

	layer_tool->radioActionEntry.name = strdup("Pan");
	layer_tool->radioActionEntry.stock_id = strdup("vik-icon-pan");
	layer_tool->radioActionEntry.label = strdup(N_("_Pan"));
	layer_tool->radioActionEntry.accelerator = strdup("<control><shift>P");
	layer_tool->radioActionEntry.tooltip = strdup(N_("Pan Tool"));
	layer_tool->radioActionEntry.value = 0;

	layer_tool->click = (VikToolMouseFunc) pantool_click;
	layer_tool->move = (VikToolMouseMoveFunc) pantool_move;
	layer_tool->release = (VikToolMouseFunc) pantool_release;

	layer_tool->cursor_type = GDK_FLEUR;
	layer_tool->cursor_data = NULL;

	return layer_tool;
}




/* NB Double clicking means this gets called THREE times!!! */
static VikLayerToolFuncStatus pantool_click(Layer * layer, GdkEventButton * event, LayerTool * tool)
{
	tool->window->modified = true;

	if (event->type == GDK_2BUTTON_PRESS) {
		/* Zoom in / out on double click.
		   No need to change the center as that has already occurred in the first click of a double click occurrence. */
		if (event->button == MouseButton::LEFT) {
			unsigned int modifier = event->state & GDK_SHIFT_MASK;
			if (modifier) {
				tool->window->viewport->zoom_out();
			} else {
				tool->window->viewport->zoom_in();
			}
		} else if (event->button == MouseButton::RIGHT) {
			tool->window->viewport->zoom_out();
		}

		tool->window->draw_update();
	} else {
		/* Standard pan click. */
		if (event->button == MouseButton::LEFT) {
			tool->window->pan_click(event);
		}
	}

	return VIK_LAYER_TOOL_ACK;
}




static VikLayerToolFuncStatus pantool_move(Layer * layer, GdkEventMotion * event, LayerTool * tool)
{
	tool->window->pan_move(event);
	return VIK_LAYER_TOOL_ACK;
}




static VikLayerToolFuncStatus pantool_release(Layer * layer, GdkEventButton * event, LayerTool * tool)
{
	if (event->button == MouseButton::LEFT) {
		tool->window->pan_release(event);
	}
	return VIK_LAYER_TOOL_ACK;
}


/*** End pan code. ********************************************************/




/********************************************************************************
 ** Select tool code
 ********************************************************************************/




static VikLayerToolFuncStatus selecttool_click(Layer * layer, GdkEventButton * event, LayerTool * tool);
static VikLayerToolFuncStatus selecttool_move(Layer * layer, GdkEventMotion * event, LayerTool * tool);
static VikLayerToolFuncStatus selecttool_release(Layer * layer, GdkEventButton * event, LayerTool * tool);




LayerTool * SlavGPS::selecttool_create(Window * window, Viewport * viewport)
{
	LayerTool * layer_tool = new LayerTool(window, viewport, LayerType::NUM_TYPES);

	layer_tool->layer_type = LayerType::NUM_TYPES;

	layer_tool->radioActionEntry.name = strdup("Select");
	layer_tool->radioActionEntry.stock_id = strdup("vik-icon-select");
	layer_tool->radioActionEntry.label = strdup(N_("_Select"));
	layer_tool->radioActionEntry.accelerator = strdup("<control><shift>S");
	layer_tool->radioActionEntry.tooltip = strdup(N_("Select Tool"));
	layer_tool->radioActionEntry.value = 3;

	layer_tool->click = (VikToolMouseFunc) selecttool_click;
	layer_tool->move = (VikToolMouseMoveFunc) selecttool_move;
	layer_tool->release = (VikToolMouseFunc) selecttool_release;

	layer_tool->cursor_type = GDK_LEFT_PTR;
	layer_tool->cursor_data = NULL;

	layer_tool->ed  = (tool_ed_t *) malloc(1 * sizeof (tool_ed_t));
	memset(layer_tool->ed, 0, sizeof (tool_ed_t));

	return layer_tool;
}




typedef struct {
	bool cont;
	Viewport * viewport;
	GdkEventButton * event;
	LayerTool * tool;
} clicker;




static void click_layer_selected(Layer * layer, clicker * ck)
{
	/* Do nothing when function call returns true,
	   i.e. stop on first found item. */
	if (ck->cont) {
		if (layer->visible) {
			ck->cont = !layer->select_click(ck->event, ck->viewport, ck->tool);
		}
	}
}




#ifdef WINDOWS
/* Hopefully Alt keys by default. */
#define VIK_MOVE_MODIFIER GDK_MOD1_MASK
#else
/* Alt+mouse on Linux desktops tend to be used by the desktop manager.
   Thus use an alternate modifier - you may need to set something into this group. */
#define VIK_MOVE_MODIFIER GDK_MOD5_MASK
#endif




static VikLayerToolFuncStatus selecttool_click(Layer * layer, GdkEventButton * event, LayerTool * tool)
{
	tool->window->select_move = false;
	/* Only allow selection on primary button. */
	if (event->button == MouseButton::LEFT) {

		if (event->state & VIK_MOVE_MODIFIER) {
			tool->window->pan_click(event);
		} else {
			/* Enable click to apply callback to potentially all track/waypoint layers. */
			/* Useful as we can find things that aren't necessarily in the currently selected layer. */
			std::list<Layer *> * layers = tool->window->layers_panel->get_all_layers_of_type(LayerType::TRW, false); /* Don't get invisible layers. */
			clicker ck;
			ck.cont = true;
			ck.viewport = tool->window->viewport;
			fprintf(stderr, "%s:%d: %p\n", __FUNCTION__, __LINE__, ck.viewport);
			ck.event = event;
			ck.tool = tool;
			for (auto iter = layers->begin(); iter != layers->end(); iter++) {
				click_layer_selected(*iter, &ck);
			}
			delete layers;

			/* If nothing found then deselect & redraw screen if necessary to remove the highlight. */
			if (ck.cont) {
				GtkTreeIter iter;
				TreeView * tree_view = tool->window->layers_panel->get_treeview();

				if (tree_view->get_selected_iter(&iter)) {
					/* Only clear if selected thing is a TrackWaypoint layer or a sublayer. */
					TreeItemType type = tree_view->get_item_type(&iter);
					if (type == TreeItemType::SUBLAYER
					    || tree_view->get_layer(&iter)->type == LayerType::TRW) {

						tree_view->unselect(&iter);
						if (tool->window->clear_highlight()) {
							tool->window->draw_update();
						}
					}
				}
			} else {
				/* Something found - so enable movement. */
				tool->window->select_move = true;
			}
		}
	} else if ((event->button == MouseButton::RIGHT) && (layer && layer->type == LayerType::TRW)) {
		if (layer->visible) {
			/* Act on currently selected item to show menu. */
			if (tool->window->selected_track || tool->window->selected_waypoint) {
				layer->show_selected_viewport_menu(event, tool->window->viewport);
			}
		}
	}

	return VIK_LAYER_TOOL_ACK;
}




static VikLayerToolFuncStatus selecttool_move(Layer * layer, GdkEventMotion * event, LayerTool * tool)
{
	if (tool->window->select_move) {
		/* Don't care about trw here. */
		if (tool->ed->trw) {
			layer->select_move(event, tool->viewport, tool); /* kamilFIXME: layer->select_move or trw->select_move? */
		}
	} else {
		/* Optional Panning. */
		if (event->state & VIK_MOVE_MODIFIER) {
			tool->window->pan_move(event);
		}
	}

	return VIK_LAYER_TOOL_ACK;
}




static VikLayerToolFuncStatus selecttool_release(Layer * layer, GdkEventButton * event, LayerTool * tool)
{
	if (tool->window->select_move) {
		/* Don't care about trw here. */
		if (tool->ed->trw) {
			((LayerTRW *) tool->ed->trw)->select_release(event, tool->viewport, tool);
		}
	}

	if (event->button == MouseButton::LEFT && (event->state & VIK_MOVE_MODIFIER)) {
		tool->window->pan_release(event);
	}

	/* Force pan off in case it was on. */
	tool->window->pan_move_flag = false;
	tool->window->pan_x = tool->window->pan_y = -1;

	/* End of this select movement. */
	tool->window->select_move = false;

	return VIK_LAYER_TOOL_ACK;
}

/*** End select tool code. ********************************************************/
