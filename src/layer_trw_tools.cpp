/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2008, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2009, Hein Ragas <viking@ragas.nl>
 * Copyright (c) 2012-2015, Rob Norris <rw_norris@hotmail.com>
 * Copyright (c) 2012-2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <cassert>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "layer_trw.h"
#include "layer_trw_draw.h"
#include "dialog.h"
#include "dems.h"




#define POINTS 1
#define LINES 2

/* This is how it knows when you click if you are clicking close to a trackpoint. */
#define TRACKPOINT_SIZE_APPROX 5
#define WAYPOINT_SIZE_APPROX 5

#define MIN_STOP_LENGTH 15
#define MAX_STOP_LENGTH 86400




using namespace SlavGPS;




extern LayerTool * trw_layer_tools[];




static bool tool_edit_trackpoint_click_cb(Layer * trw, QMouseEvent * event, LayerTool * tool);
static bool tool_edit_trackpoint_move_cb(Layer * trw, QMouseEvent * event, LayerTool * tool);
static bool tool_edit_trackpoint_release_cb(Layer * trw, QMouseEvent * event, LayerTool * tool);


static bool tool_show_picture_click_cb(Layer * trw, QMouseEvent * event, LayerTool * tool);


static bool tool_edit_waypoint_click_cb(Layer * trw, QMouseEvent * event, LayerTool * tool);
static bool tool_edit_waypoint_move_cb(Layer * trw, QMouseEvent * event, LayerTool * tool);
static bool tool_edit_waypoint_release_cb(Layer * trw, QMouseEvent * event, LayerTool * tool);


static bool tool_new_route_click_cb(Layer * trw, QMouseEvent * event, LayerTool * tool);

static bool tool_new_track_click_cb(Layer * trw, QMouseEvent * event, LayerTool * tool);
static bool tool_new_track_double_click_cb(Layer * trw, QMouseEvent * event, LayerTool * tool);
static LayerToolFuncStatus tool_new_track_move_cb(Layer * trw, QMouseEvent * event, LayerTool * tool);
static void tool_new_track_release_cb(Layer * trw, QMouseEvent * event, LayerTool * tool);
static bool tool_new_track_key_press_cb(Layer * trw, GdkEventKey * event, LayerTool * tool);


static bool tool_new_waypoint_click_cb(Layer * trw, QMouseEvent * event, LayerTool * tool);


static bool tool_extended_route_finder_click_cb(Layer * trw, QMouseEvent * event, LayerTool * tool);
static bool tool_extended_route_finder_key_press_cb(Layer * trw, GdkEventKey * event, LayerTool * tool);




// ATM: Leave this as 'Track' only.
//  Not overly bothered about having a snap to route trackpoint capability
Trackpoint * LayerTRW::closest_tp_in_five_pixel_interval(Viewport * viewport, int x, int y)
{
	TrackpointSearch search;
	search.x = x;
	search.y = y;
	search.viewport = viewport;

	search.viewport->get_bbox(&search.bbox);

	LayerTRWc::track_search_closest_tp(this->tracks, &search);

	return search.closest_tp;
}




Waypoint * LayerTRW::closest_wp_in_five_pixel_interval(Viewport * viewport, int x, int y)
{
	WaypointSearch search;
	search.x = x;
	search.y = y;
	search.viewport = viewport;
	search.draw_images = this->drawimages;

	LayerTRWc::waypoint_search_closest_tp(this->waypoints, &search);

	return search.closest_wp;
}




/* Forward declarations. */
static void marker_begin_move(LayerTool * tool, int x, int y);
static void marker_moveto(LayerTool * tool, int x, int y);
static void marker_end_move(LayerTool * tool);


bool LayerTRW::select_move(QMouseEvent * event, Viewport * viewport, LayerTool * tool)
{
	if (tool->ed->holding) {
		VikCoord new_coord;

		viewport->screen_to_coord(event->x(), event->y(), &new_coord);

		/* Here always allow snapping back to the original location.
		   This is useful when one decides not to move the thing after all.
		   If one wants to move the item only a little bit then don't hold down the 'snap' key! */

		/* Snap to trackpoint. */
		if (event->modifiers() & Qt::ControlModifier) {
			Trackpoint * tp = this->closest_tp_in_five_pixel_interval(viewport, event->x(), event->y());
			if (tp) {
				new_coord = tp->coord;
			}
		}

		/* Snap to waypoint. */
		if (event->modifiers() & Qt::ShiftModifier) {
			Waypoint * wp = this->closest_wp_in_five_pixel_interval(viewport, event->x(), event->y());
			if (wp) {
				new_coord = wp->coord;
			}
		}

		int x, y;
		viewport->coord_to_screen(&new_coord, &x, &y);

		marker_moveto(tool, x, y);

		return true;
	}
	return false;
}




bool LayerTRW::select_release(QMouseEvent * event, Viewport * viewport, LayerTool * tool)
{
	if (!tool->ed->holding) {
		return false;
	}

	if (!event->button() == Qt::LeftButton) {
		return false;
	}

	/* Prevent accidental (small) shifts when specific movement has not been requested
	   (as the click release has occurred within the click object detection area). */
	if (!tool->ed->moving) {
		return false;
	}

	VikCoord new_coord;
	viewport->screen_to_coord(event->x(), event->y(), &new_coord);

	/* Snap to trackpoint. */
	if (event->modifiers() & Qt::ControlModifier) {
		Trackpoint * tp = this->closest_tp_in_five_pixel_interval(viewport, event->x(), event->y());
		if (tp) {
			new_coord = tp->coord;
		}
	}

	/* Snap to waypoint. */
	if (event->modifiers() & Qt::ShiftModifier) {
		Waypoint * wp = this->closest_wp_in_five_pixel_interval(viewport, event->x(), event->y());
		if (wp) {
			new_coord = wp->coord;
		}
	}

	fprintf(stderr, "%s:%d: calling marker_end_move\n", __FUNCTION__, __LINE__);
	marker_end_move(tool);

	/* Determine if working on a waypoint or a trackpoint. */
	if (tool->ed->is_waypoint) {
		/* Update waypoint position. */
		this->current_wp->coord = new_coord;
		this->calculate_bounds_waypoints();
		/* Reset waypoint pointer. */
		this->current_wp = NULL;
	} else {
		if (this->selected_tp.valid) {
			(*this->selected_tp.iter)->coord = new_coord;

			if (this->current_trk) {
				this->current_trk->calculate_bounds();
			}

			if (this->tpwin) {
				if (this->current_trk) {
					this->my_tpwin_set_tp();
				}
			}
			/* NB don't reset the selected trackpoint, thus ensuring it's still in the tpwin. */
		}
	}

	this->emit_changed();
	return true;
}




/*
  Returns true if a waypoint or track is found near the requested event position for this particular layer.
  The item found is automatically selected.
  This is a tool like feature but routed via the layer interface, since it's instigated by a 'global' layer tool in window.c
 */
bool LayerTRW::select_click(QMouseEvent * event, Viewport * viewport, LayerTool * tool)
{
	if (event->button() != Qt::LeftButton) {
		return false;
	}

	if (this->type != LayerType::TRW) {
		return false;
	}
#ifdef K
	if (!this->tracks_visible && !this->waypoints_visible && !this->routes_visible) {
		return false;
	}
#endif

	LatLonBBox bbox;
	viewport->get_bbox(&bbox);

	/* Go for waypoints first as these often will be near a track, but it's likely the wp is wanted rather then the track. */

	if (this->waypoints_visible && BBOX_INTERSECT (this->waypoints_bbox, bbox)) {
		WaypointSearch wp_search;
		wp_search.viewport = viewport;
		wp_search.x = event->x();
		wp_search.y = event->y();
		wp_search.draw_images = this->drawimages;

		LayerTRWc::waypoint_search_closest_tp(this->waypoints, &wp_search);

		if (wp_search.closest_wp) {

			/* Select. */
			this->tree_view->select_and_expose(wp_search.closest_wp->index);

			/* Too easy to move it so must be holding shift to start immediately moving it
			   or otherwise be previously selected but not have an image (otherwise clicking within image bounds (again) moves it). */
			if (event->modifiers() & Qt::ShiftModifier
			    || (this->current_wp == wp_search.closest_wp && !this->current_wp->image)) {
				/* Put into 'move buffer'.
				   Viewport & window already set in tool. */
				tool->ed->trw = this;
				tool->ed->is_waypoint = true;

				marker_begin_move(tool, event->x(), event->y());
			}

			this->current_wp = wp_search.closest_wp;

#ifdef K
			if (event->type == GDK_2BUTTON_PRESS) {
				if (this->current_wp->image) {
					trw_menu_sublayer_t data;
					memset(&data, 0, sizeof (trw_menu_sublayer_t));
					data.layer = this;
					data.misc = this->current_wp->image;
					this->show_picture_cb(&data);
				}
			}
#endif

			this->emit_changed();
			return true;
		}
	}

	/* Used for both track and route lists. */
	TrackpointSearch tp_search;
	tp_search.viewport = viewport;
	tp_search.x = event->x();
	tp_search.y = event->y();
	//tp_search.closest_tp_iter = NULL;
	tp_search.bbox = bbox;

	if (this->tracks_visible) {
		LayerTRWc::track_search_closest_tp(this->tracks, &tp_search);

		if (tp_search.closest_tp) {

			/* Always select + highlight the track. */
			this->tree_view->select_and_expose(tp_search.closest_track->index);

			tool->ed->is_waypoint = false;

			/* Select the Trackpoint.
			   Can move it immediately when control held or it's the previously selected tp. */
			if (event->modifiers() & Qt::ControlModifier
			    || this->selected_tp.iter == tp_search.closest_tp_iter) {

				/* Put into 'move buffer'.
				   Viewport & window already set in tool. */
				tool->ed->trw = this;
				marker_begin_move(tool, event->x(), event->y());
			}

			this->selected_tp.iter = tp_search.closest_tp_iter;
			this->selected_tp.valid = true;

			this->current_trk = tp_search.closest_track;

			this->set_statusbar_msg_info_trkpt(tp_search.closest_tp);

			if (this->tpwin) {
				this->my_tpwin_set_tp();
			}

			this->emit_changed();
			return true;
		}
	}

	/* Try again for routes. */
	if (this->routes_visible) {
		LayerTRWc::track_search_closest_tp(this->routes, &tp_search);

		if (tp_search.closest_tp)  {

			/* Always select + highlight the track. */
			this->tree_view->select_and_expose(tp_search.closest_track->index);

			tool->ed->is_waypoint = false;

			/* Select the Trackpoint.
			   Can move it immediately when control held or it's the previously selected tp. */
			if (event->modifiers() & Qt::ControlModifier
			    || this->selected_tp.iter == tp_search.closest_tp_iter) {

				/* Put into 'move buffer'.
				   Viewport & window already set in tool. */
				tool->ed->trw = this;
				marker_begin_move(tool, event->x(), event->y());
			}

			this->selected_tp.iter = tp_search.closest_tp_iter;
			this->selected_tp.valid = true;

			this->current_trk = tp_search.closest_track;

			this->set_statusbar_msg_info_trkpt(tp_search.closest_tp);

			if (this->tpwin) {
				this->my_tpwin_set_tp();
			}

			this->emit_changed();
			return true;
		}
	}

	/* These aren't the droids you're looking for. */
	this->current_wp = NULL;
	this->cancel_current_tp(false);

	/* Blank info. */
	this->get_window()->get_statusbar()->set_message(StatusBarField::INFO, "");

	return false;
}




bool LayerTRW::show_selected_viewport_menu(QMouseEvent * event, Viewport * viewport)
{
	if (event->button() != Qt::RightButton) {
		return false;
	}

	if (this->type != LayerType::TRW) {
		return false;
	}

	if (!this->tracks_visible && !this->waypoints_visible && !this->routes_visible) {
		return false;
	}

	/* Post menu for the currently selected item. */

	/* See if a track is selected */
	Track * trk = this->get_window()->get_selected_track();
	if (trk && trk->visible) {
		if (trk->name) {
			this->menu_data->sublayer = trk;
			this->menu_data->viewport = viewport;

			QMenu menu(viewport);

			this->sublayer_add_menu_items(menu);
			menu.exec(QCursor::pos());
			return true;
		}
	}

	/* See if a waypoint is selected. */
	Waypoint * waypoint = this->get_window()->get_selected_waypoint();
	if (waypoint && waypoint->visible) {
		if (waypoint->name) {

			this->menu_data->sublayer = waypoint;
			this->menu_data->viewport = viewport;

			QMenu menu(viewport);

			this->sublayer_add_menu_items(menu);
			menu.exec(QCursor::pos());
			return true;
		}
	}
	return false;
}




/* Background drawing hook, to be passed the viewport. */
static bool tool_sync_done = true;




static void marker_begin_move(LayerTool * tool, int x, int y)
{
	tool->ed->holding = true;

	tool->ed->ed_pen = new QPen(QColor("black"));
	tool->ed->ed_pen->setWidth(2);

	//gdk_gc_set_function(tool->ed->en_pen, GDK_INVERT);
	tool->viewport->draw_rectangle(*tool->ed->ed_pen, x-3, y-3, 6, 6);
	tool->viewport->sync();
	tool->ed->oldx = x;
	tool->ed->oldy = y;
	tool->ed->moving = false;
}




static void marker_moveto(LayerTool * tool, int x, int y)
{
	tool->viewport->draw_rectangle(*tool->ed->ed_pen, tool->ed->oldx - 3, tool->ed->oldy - 3, 6, 6);
	tool->viewport->draw_rectangle(*tool->ed->ed_pen, x-3, y-3, 6, 6);
	tool->ed->oldx = x;
	tool->ed->oldy = y;
	tool->ed->moving = true;

	if (tool_sync_done) {
		tool->viewport->sync();
		tool_sync_done = true;
	}
}




static void marker_end_move(LayerTool * tool)
{
	tool->viewport->draw_rectangle(*tool->ed->ed_pen, tool->ed->oldx - 3, tool->ed->oldy - 3, 6, 6);
	delete tool->ed->ed_pen;
	tool->ed->holding = false;
	tool->ed->moving = false;
}




/*** Edit waypoint ****/

LayerTool * tool_edit_waypoint_create(Window * window, Viewport * viewport)
{
	LayerTool * layer_tool = new LayerTool(window, viewport, LayerType::TRW);

	trw_layer_tools[4] = layer_tool;

	layer_tool->layer_type = LayerType::TRW;
	layer_tool->id_string = QString("EditWaypoint");

	layer_tool->radioActionEntry.stock_id    = strdup(":/icons/layer_tool/trw_edit_wp_18.png");
	layer_tool->radioActionEntry.label       = strdup(N_("&Edit Waypoint"));
	layer_tool->radioActionEntry.accelerator = strdup("<control><shift>E");
	layer_tool->radioActionEntry.tooltip     = strdup(N_("Edit Waypoint"));
	layer_tool->radioActionEntry.value       = 0;

	layer_tool->click = (ToolMouseFunc) tool_edit_waypoint_click_cb;
	layer_tool->move = (ToolMouseMoveFunc) tool_edit_waypoint_move_cb;
	layer_tool->release = (ToolMouseFunc) tool_edit_waypoint_release_cb;

	layer_tool->cursor_click = new QCursor(QPixmap(":/cursors/trw_edit_wp.png"), 0, 0);
	layer_tool->cursor_release = new QCursor(Qt::ArrowCursor);

	layer_tool->ed = (tool_ed_t *) malloc(1 * sizeof (tool_ed_t));
	memset(layer_tool->ed, 0, sizeof (tool_ed_t));

	return layer_tool;
}




static bool tool_edit_waypoint_click_cb(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (trw->type != LayerType::TRW) {
		return false;
	}
	if (tool->ed->holding) {
		return true;
	}
#ifdef K
	if (!trw->visible || !trw->waypoints_visible) {
		return false;
	}
#endif
	if (trw->current_wp && trw->current_wp->visible) {
		/* First check if current WP is within area (other may be 'closer', but we want to move the current). */
		int x, y;
		tool->viewport->coord_to_screen(&trw->current_wp->coord, &x, &y);

		if (abs(x - (int) round(event->x())) <= WAYPOINT_SIZE_APPROX
		    && abs(y - (int) round(event->y())) <= WAYPOINT_SIZE_APPROX) {
			if (event->button() == Qt::RightButton) {
				trw->waypoint_rightclick = true; /* Remember that we're clicking; other layers will ignore release signal. */
			} else {
				marker_begin_move(tool, event->x(), event->y());
			}
			return true;
		}
	}

	WaypointSearch search;
	search.viewport = tool->viewport;
	search.x = event->x();
	search.y = event->y();
	search.draw_images = trw->drawimages;

	LayerTRWc::waypoint_search_closest_tp(trw->waypoints, &search);
	if (trw->current_wp && (trw->current_wp == search.closest_wp)) {
		if (event->button() == Qt::RightButton) {
			trw->waypoint_rightclick = true; /* Remember that we're clicking; other layers will ignore release signal. */
		} else {
			marker_begin_move(tool, event->x(), event->y());
		}
		return false;
	} else if (search.closest_wp) {
		if (event->button() == Qt::RightButton) {
			trw->waypoint_rightclick = true; /* Remember that we're clicking; other layers will ignore release signal. */
		} else {
			trw->waypoint_rightclick = false;
		}

		trw->tree_view->select_and_expose(search.closest_wp->index);

		trw->current_wp = search.closest_wp;

		/* Could make it so don't update if old WP is off screen and new is null but oh well. */
		trw->emit_changed();
		return true;
	}

	trw->current_wp = NULL;
	trw->waypoint_rightclick = false;
	trw->emit_changed();

	return false;
}




static bool tool_edit_waypoint_move_cb(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (trw->type != LayerType::TRW) {
		return false;
	}

	if (!tool->ed->holding) {
		return false;
	}

	VikCoord new_coord;
	tool->viewport->screen_to_coord(event->x(), event->y(), &new_coord);


	if (event->modifiers() & Qt::ControlModifier) { /* Snap to trackpoint. */
		Trackpoint * tp = trw->closest_tp_in_five_pixel_interval(tool->viewport, event->x(), event->y());
		if (tp) {
			new_coord = tp->coord;
		}
	} else if (event->modifiers() & Qt::ShiftModifier) { /* Snap to waypoint. */
		Waypoint * wp = trw->closest_wp_in_five_pixel_interval(tool->viewport, event->x(), event->y());
		if (wp && wp != trw->current_wp) {
			new_coord = wp->coord;
		}
	} else {
		; /* No modifiers. */
	}

	int x, y;
	tool->viewport->coord_to_screen(&new_coord, &x, &y);
	marker_moveto(tool, x, y);

	return true;
}




static bool tool_edit_waypoint_release_cb(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (trw->type != LayerType::TRW) {
		return false;
	}

#ifdef K
	if (!tool->ed->holding) {
		return false;
	}
#endif

	if (event->button() == Qt::LeftButton) {
		VikCoord new_coord;
		tool->viewport->screen_to_coord(event->x(), event->y(), &new_coord);

		/* Snap to trackpoint. */
		if (event->modifiers() & Qt::ControlModifier) {
			Trackpoint * tp = trw->closest_tp_in_five_pixel_interval(tool->viewport, event->x(), event->y());
			if (tp) {
				new_coord = tp->coord;
			}
		}

		/* Snap to waypoint. */
		if (event->modifiers() & Qt::ShiftModifier) {
			Waypoint * wp = trw->closest_wp_in_five_pixel_interval(tool->viewport, event->x(), event->y());
			if (wp && wp != trw->current_wp) {
				new_coord = wp->coord;
			}
		}

		marker_end_move(tool);

		trw->current_wp->coord = new_coord;

		trw->calculate_bounds_waypoints();
		trw->emit_changed();
		return true;

	} else if (event->button() == Qt::RightButton && trw->waypoint_rightclick) {
		if (trw->current_wp) {
			trw->menu_data->sublayer = trw->current_wp;
			trw->menu_data->viewport = tool->viewport;

			QMenu menu;
			trw->sublayer_add_menu_items(menu);
			menu.exec(QCursor::pos());
		}
		trw->waypoint_rightclick = false;

	} else {
		return false;
	}
}




/*** New track ****/

LayerTool * tool_new_track_create(Window * window, Viewport * viewport)
{
	LayerTool * layer_tool = new LayerTool(window, viewport, LayerType::TRW);

	trw_layer_tools[1] = layer_tool;

	layer_tool->layer_type = LayerType::TRW;
	layer_tool->id_string = QString("CreateTrack");

	layer_tool->radioActionEntry.stock_id    = strdup(":/icons/layer_tool/trw_add_tr_18.png");
	layer_tool->radioActionEntry.label       = strdup(N_("Create &Track"));
	layer_tool->radioActionEntry.accelerator = strdup("<control><shift>T");
	layer_tool->radioActionEntry.tooltip     = strdup(N_("Create Track"));
	layer_tool->radioActionEntry.value       = 0;

	layer_tool->click = (ToolMouseFunc) tool_new_track_click_cb;
	layer_tool->double_click = (ToolMouseFunc) tool_new_track_double_click_cb;
	layer_tool->move = (ToolMouseMoveFunc) tool_new_track_move_cb;
	layer_tool->release = (ToolMouseFunc) tool_new_track_release_cb;
	layer_tool->key_press = tool_new_track_key_press_cb;

	layer_tool->pan_handler = true;  /* Still need to handle clicks when in PAN mode to disable the potential trackpoint drawing. */
	layer_tool->cursor_click = new QCursor(QPixmap(":/cursors/trw_add_tr.png"), 0, 0);
	layer_tool->cursor_release = new QCursor(Qt::ArrowCursor);

	layer_tool->ed = (tool_ed_t *) malloc(sizeof (tool_ed_t));
	memset(layer_tool->ed, 0, sizeof (tool_ed_t));

	return layer_tool;
}




/*
 * Draw specified pixmap.
 */
static int draw_sync(LayerTRW * trw, QPixmap * drawable, QPixmap * pixmap)
{
	/* Sometimes don't want to draw normally because another
	   update has taken precedent such as panning the display
	   which means this pixmap is no longer valid. */
	if (1 /* trw->draw_sync_do*/ ) {
		QPainter painter(drawable);
		painter.drawPixmap(0, 0, *pixmap);
		emit trw->changed();
#if 0
		gdk_draw_drawable(ds->drawable,
				  ds->gc,
				  ds->pixmap,
				  0, 0, 0, 0, -1, -1);
#endif
		trw->draw_sync_done = true;
	}
	return 0;
}




static char* distance_string(double distance)
{
	char str[128];

	/* Draw label with distance. */
	DistanceUnit distance_unit = a_vik_get_units_distance();
	switch (distance_unit) {
	case DistanceUnit::MILES:
		if (distance >= VIK_MILES_TO_METERS(1) && distance < VIK_MILES_TO_METERS(100)) {
			g_sprintf(str, "%3.2f miles", VIK_METERS_TO_MILES(distance));
		} else if (distance < 1609.4) {
			g_sprintf(str, "%d yards", (int)(distance*1.0936133));
		} else {
			g_sprintf(str, "%d miles", (int)VIK_METERS_TO_MILES(distance));
		}
		break;
	case DistanceUnit::NAUTICAL_MILES:
		if (distance >= VIK_NAUTICAL_MILES_TO_METERS(1) && distance < VIK_NAUTICAL_MILES_TO_METERS(100)) {
			g_sprintf(str, "%3.2f NM", VIK_METERS_TO_NAUTICAL_MILES(distance));
		} else if (distance < VIK_NAUTICAL_MILES_TO_METERS(1)) {
			g_sprintf(str, "%d yards", (int)(distance*1.0936133));
		} else {
			g_sprintf(str, "%d NM", (int)VIK_METERS_TO_NAUTICAL_MILES(distance));
		}
		break;
	default:
		/* DistanceUnit::KILOMETRES */
		if (distance >= 1000 && distance < 100000) {
			g_sprintf(str, "%3.2f km", distance/1000.0);
		} else if (distance < 1000) {
			g_sprintf(str, "%d m", (int)distance);
		} else {
			g_sprintf(str, "%d km", (int)distance/1000);
		}
		break;
	}
	return g_strdup(str);
}




/*
 * Actually set the message in statusbar.
 */
static void statusbar_write(double distance, double elev_gain, double elev_loss, double last_step, double angle, LayerTRW * layer)
{
	/* Only show elevation data when track has some elevation properties. */
	char str_gain_loss[64];
	str_gain_loss[0] = '\0';
	char str_last_step[64];
	str_last_step[0] = '\0';
	char *str_total = distance_string(distance);

	if ((elev_gain > 0.1) || (elev_loss > 0.1)) {
		if (a_vik_get_units_height() == HeightUnit::METRES) {
			g_sprintf(str_gain_loss, _(" - Gain %dm:Loss %dm"), (int)elev_gain, (int)elev_loss);
		} else {
			g_sprintf(str_gain_loss, _(" - Gain %dft:Loss %dft"), (int)VIK_METERS_TO_FEET(elev_gain), (int)VIK_METERS_TO_FEET(elev_loss));
		}
	}

	if (last_step > 0) {
		char *tmp = distance_string(last_step);
		g_sprintf(str_last_step, _(" - Bearing %3.1f° - Step %s"), RAD2DEG(angle), tmp);
		free(tmp);
	}

	/* Write with full gain/loss information. */
	char *msg = g_strdup_printf("Total %s%s%s", str_total, str_last_step, str_gain_loss);
	layer->get_window()->get_statusbar()->set_message(StatusBarField::INFO, msg);
	free(msg);
	free(str_total);
}




/*
 * Figure out what information should be set in the statusbar and then write it.
 */
void LayerTRW::update_statusbar()
{
	/* Get elevation data. */
	double elev_gain, elev_loss;
	this->current_trk->get_total_elevation_gain(&elev_gain, &elev_loss);

	/* Find out actual distance of current track. */
	double distance = this->current_trk->get_length();

	statusbar_write(distance, elev_gain, elev_loss, 0, 0, this);
}




static LayerToolFuncStatus tool_new_track_move_cb(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
	LayerTRW * trw = (LayerTRW *) layer;

	qDebug() << "II: Layer TRW: new track's move()" << trw->draw_sync_done;
	qDebug() << "II: Layer TRW: new track's move()" << trw->current_trk;
	qDebug() << "II: Layer TRW: new track's move()" << !trw->current_trk->empty();
	/* If we haven't sync'ed yet, we don't have time to do more. */
	if (/* trw->draw_sync_done && */ trw->current_trk && !trw->current_trk->empty()) {
		Trackpoint * last_tpt = trw->current_trk->get_tp_last();

		static QPixmap * pixmap = NULL;

		/* Need to check in case window has been resized. */
		int w1 = tool->viewport->get_width();
		int h1 = tool->viewport->get_height();

		if (!pixmap) {
			pixmap = new QPixmap(w1, h1);
		}
		int w2, h2;
		if (w1 != pixmap->width() || h1 != pixmap->height()) {
			delete pixmap;
			pixmap = new QPixmap(w1, h1);
		}

		/* Reset to background. */
		//QPainter painter2(ds->drawable);
		//painter2.drawPixmap(0, 0, *ds->pixmap);
		*pixmap = *tool->viewport->get_pixmap();
#if 0
		gdk_draw_drawable(pixmap,
				  trw->current_trk_new_point_pen,
				  tool->viewport->get_pixmap(),
				  0, 0, 0, 0, -1, -1);
#endif

		int x1, y1;

		tool->viewport->coord_to_screen(&last_tpt->coord, &x1, &y1);

		/* FOR SCREEN OVERLAYS WE MUST DRAW INTO THIS PIXMAP (when using the reset method)
		   otherwise using Viewport::draw_* functions puts the data into the base pixmap,
		   thus when we come to reset to the background it would include what we have already drawn!! */
		QPainter painter(pixmap);
		painter.setPen(trw->current_trk_new_point_pen);
		qDebug() << "II: Layer TRW: drawing line" << x1 << y1 << event->x() << event->y();
		painter.drawLine(x1, y1, event->x(), event->y());

		/* Using this reset method is more reliable than trying to undraw previous efforts via the GDK_INVERT method. */

		/* Find out actual distance of current track. */
		double distance = trw->current_trk->get_length();

		/* Now add distance to where the pointer is. */
		VikCoord coord;
		struct LatLon ll;
		tool->viewport->screen_to_coord(event->x(), event->y(), &coord);
		vik_coord_to_latlon(&coord, &ll);
		double last_step = vik_coord_diff(&coord, &last_tpt->coord);
		distance = distance + last_step;

		/* Get elevation data. */
		double elev_gain, elev_loss;
		trw->current_trk->get_total_elevation_gain(&elev_gain, &elev_loss);

		/* Adjust elevation data (if available) for the current pointer position. */
		double elev_new;
		elev_new = (double) dem_cache_get_elev_by_coord(&coord, VIK_DEM_INTERPOL_BEST);
		if (elev_new != VIK_DEM_INVALID_ELEVATION) {
			if (last_tpt->altitude != VIK_DEFAULT_ALTITUDE) {
				/* Adjust elevation of last track point. */
				if (elev_new > last_tpt->altitude) {
					/* Going up. */
					elev_gain += elev_new - last_tpt->altitude;
				} else {
					/* Going down. */
					elev_loss += last_tpt->altitude - elev_new;
				}
			}
		}

		/* Display of the distance 'tooltip' during track creation is controlled by a preference. */
		if (a_vik_get_create_track_tooltip()) {
#ifdef K
			char *str = distance_string(distance);

			PangoLayout *pl = gtk_widget_create_pango_layout(tool->viewport->get_toolkit_widget(), NULL);
			pango_layout_set_font_description(pl, gtk_widget_get_style(tool->viewport->get_toolkit_widget())->font_desc);
			pango_layout_set_text(pl, str, -1);
			int wd, hd;
			pango_layout_get_pixel_size(pl, &wd, &hd);

			int xd,yd;
			/* Offset from cursor a bit depending on font size. */
			xd = event->x + 10;
			yd = event->y - hd;

			/* Create a background block to make the text easier to read over the background map. */
			GdkGC *background_block_gc = tool->viewport->new_pen("#cccccc", 1);
			fill_rectangle(pixmap, background_block_gc, xd-2, yd-2, wd+4, hd+2);
			gdk_draw_layout(pixmap, trw->current_trk_new_point_pen, xd, yd, pl);

			g_object_unref(G_OBJECT (pl));
			g_object_unref(G_OBJECT (background_block_gc));
			free(str);
#endif
		}

		double angle;
		double baseangle;
		tool->viewport->compute_bearing(x1, y1, event->x(), event->y(), &angle, &baseangle);

		/* Update statusbar with full gain/loss information. */
		statusbar_write(distance, elev_gain, elev_loss, last_step, angle, trw);

		//passalong->pen = new QPen(trw->current_trk_new_point_pen);
		draw_sync(trw, tool->viewport->scr_buffer, pixmap);
		trw->draw_sync_done = false;

		return LayerToolFuncStatus::ACK_GRAB_FOCUS;
	}
	return LayerToolFuncStatus::ACK;
}




/* trw->current_trk must be valid. */
void LayerTRW::undo_trackpoint_add()
{
	if (!this->current_trk || this->current_trk->empty()) {
		return;
	}

	auto iter = this->current_trk->get_last();
	this->current_trk->erase_trackpoint(iter);

	this->current_trk->calculate_bounds();
}




static bool tool_new_track_key_press_cb(Layer * layer, GdkEventKey * event, LayerTool * tool)
{
	LayerTRW * trw = (LayerTRW *) layer;
#ifdef K
	if (this->current_trk && event->keyval == GDK_Escape) {
		/* Bin track if only one point as it's not very useful. */
		if (this->current_trk->get_tp_count() == 1) {
			if (this->current_trk->sublayer_type == SublayerType::ROUTE) {
				this->delete_route(this->current_trk);
			} else {
				this->delete_track(this->current_trk);
			}
		}
		this->current_trk = NULL;
		this->emit_changed();
		return true;
	} else if (this->current_trk && event->keyval == GDK_BackSpace) {
		this->undo_trackpoint_add();
		this->update_statusbar();
		this->emit_changed();
		return true;
	}
#endif
	return false;
}




/*
 * Common function to handle trackpoint button requests on either a route or a track
 *  . enables adding a point via normal click
 *  . enables removal of last point via right click
 */
bool LayerTRW::tool_new_track_or_route_click(QMouseEvent * event, Viewport * viewport)
{
	if (this->type != LayerType::TRW) {
		return false;
	}

	if (event->button() == Qt::MiddleButton) {
		/* As the display is panning, the new track pixmap is now invalid so don't draw it
		   otherwise this drawing done results in flickering back to an old image. */
		this->draw_sync_do = false;
		return false;
	}

	if (event->button() == Qt::RightButton) {
		if (!this->current_trk) {
			return false;
		}
		this->undo_trackpoint_add();
		this->update_statusbar();
		this->emit_changed();
		return true;
	}

	Trackpoint * tp = new Trackpoint();
	viewport->screen_to_coord(event->x(), event->y(), &tp->coord);

	/* Snap to other TP. */
	if (event->modifiers() & Qt::ControlModifier) {
		Trackpoint * other_tp = this->closest_tp_in_five_pixel_interval(viewport, event->x(), event->y());
		if (other_tp) {
			tp->coord = other_tp->coord;
		}
	}

	tp->newsegment = false;
	tp->has_timestamp = false;
	tp->timestamp = 0;

	if (this->current_trk) {
		this->current_trk->add_trackpoint(tp, true); /* Ensure bounds is updated. */
		/* Auto attempt to get elevation from DEM data (if it's available). */
		this->current_trk->apply_dem_data_last_trackpoint();
	}

	/* TODO: I think that in current implementation of handling of double click we don't need these fields. */
	this->ct_x1 = this->ct_x2;
	this->ct_y1 = this->ct_y2;
	this->ct_x2 = event->x();
	this->ct_y2 = event->y();

	this->emit_changed();

	return true;
}




static bool tool_new_track_click_cb(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
	LayerTRW * trw = (LayerTRW *) layer;

	/* If we were running the route finder, cancel it. */
	trw->route_finder_started = false;

	/* If current is a route - switch to new track. */
	if (event->button() == Qt::LeftButton) {
		if ((!trw->current_trk || (trw->current_trk && trw->current_trk->sublayer_type == SublayerType::ROUTE))) {
		char *name = trw->new_unique_sublayer_name(SublayerType::TRACK, _("Track"));
		QString new_name(name);
		if (a_vik_get_ask_for_create_track_name()) {
			new_name = a_dialog_new_track(trw->get_window(), QString(name), false);
			if (new_name.isEmpty()) {
				return false;
			}
		}
		trw->new_track_create_common(new_name.toUtf8().data());
		free(name);
		}
	}

	return trw->tool_new_track_or_route_click(event, tool->viewport);
}




static bool tool_new_track_double_click_cb(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
	LayerTRW * trw = (LayerTRW *) layer;
	if (trw->type != LayerType::TRW) {
		return false;
	}

	if (event->button() != Qt::LeftButton) {
		return false;
	}

	/* Subtract last (duplicate from double click) tp then end. */
	if (trw->current_trk && !trw->current_trk->empty() /*  && trw->ct_x1 == trw->ct_x2 && trw->ct_y1 == trw->ct_y2 */) {
		/* Undo last, then end.
		   TODO: I think that in current implementation of handling of double click we don't need the undo. */
		trw->undo_trackpoint_add();
		trw->current_trk = NULL;
	}
	trw->emit_changed();
	return true;
}




static void tool_new_track_release_cb(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (event->button() == Qt::MiddleButton) {
		/* Pan moving ended - enable potential point drawing again. */
		trw->draw_sync_do = true;
		trw->draw_sync_done = true;
	}
}




/*** New route ****/

LayerTool * tool_new_route_create(Window * window, Viewport * viewport)
{
	LayerTool * layer_tool = new LayerTool(window, viewport, LayerType::TRW);

	trw_layer_tools[2] = layer_tool;

	layer_tool->layer_type = LayerType::TRW;
	layer_tool->id_string = QString("CreateRoute");

	layer_tool->radioActionEntry.stock_id    = strdup(":/icons/layer_tool/trw_add_route_18.png");
	layer_tool->radioActionEntry.label       = strdup(N_("Create &Route"));
	layer_tool->radioActionEntry.accelerator = strdup("<control><shift>B");
	layer_tool->radioActionEntry.tooltip     = strdup(N_("Create Route"));
	layer_tool->radioActionEntry.value       = 0;

	layer_tool->click = (ToolMouseFunc) tool_new_route_click_cb;
	layer_tool->move = (ToolMouseMoveFunc) tool_new_track_move_cb;    /* Reuse this track method for a route. */
	layer_tool->release = (ToolMouseFunc) tool_new_track_release_cb;  /* Reuse this track method for a route. */
	layer_tool->key_press = tool_new_track_key_press_cb;                 /* Reuse this track method for a route. */

	layer_tool->pan_handler = true;  /* Still need to handle clicks when in PAN mode to disable the potential trackpoint drawing. */
	layer_tool->cursor_click = new QCursor(QPixmap(":/cursors/trw_add_route.png"), 0, 0);
	layer_tool->cursor_release = new QCursor(Qt::ArrowCursor);

	layer_tool->ed = (tool_ed_t *) malloc(sizeof (tool_ed_t));
	memset(layer_tool->ed, 0, sizeof (tool_ed_t));

	return layer_tool;
}




static bool tool_new_route_click_cb(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
	LayerTRW * trw = (LayerTRW *) layer;

	/* If we were running the route finder, cancel it. */
	trw->route_finder_started = false;

	/* If current is a track - switch to new route */
	if (event->button() == Qt::LeftButton
	    && (!trw->current_trk
		|| (trw->current_trk && trw->current_trk->sublayer_type == SublayerType::ROUTE))) {

		char * name = trw->new_unique_sublayer_name(SublayerType::ROUTE, _("Route"));
		QString new_name(name);
		if (a_vik_get_ask_for_create_track_name()) {
			new_name = a_dialog_new_track(trw->get_window(), new_name, true);
			if (new_name.isEmpty()) {
				return false;
			}
		}

		trw->new_route_create_common(new_name.toUtf8().data());
		free(name);
	}
	return trw->tool_new_track_or_route_click(event, tool->viewport);
}




/*** New waypoint ****/

LayerTool * tool_new_waypoint_create(Window * window, Viewport * viewport)
{
	LayerTool * layer_tool = new LayerTool(window, viewport, LayerType::TRW);

	trw_layer_tools[0] = layer_tool;

	layer_tool->layer_type = LayerType::TRW;
	layer_tool->id_string = QString("CreateWaypoint");

	layer_tool->radioActionEntry.stock_id    = strdup(":/icons/layer_tool/trw_add_wp_18.png");
	layer_tool->radioActionEntry.label       = strdup(N_("Create &Waypoint"));
	layer_tool->radioActionEntry.accelerator = strdup("<control><shift>W");
	layer_tool->radioActionEntry.tooltip     = strdup(N_("Create Waypoint"));
	layer_tool->radioActionEntry.value       = 0;

	layer_tool->click = (ToolMouseFunc) tool_new_waypoint_click_cb;

	layer_tool->cursor_click = new QCursor(QPixmap(":/cursors/trw_add_wp.png"), 0, 0);
	layer_tool->cursor_release = new QCursor(Qt::ArrowCursor);

	layer_tool->ed = (tool_ed_t *) malloc(sizeof (tool_ed_t));;
	memset(layer_tool->ed, 0, sizeof (tool_ed_t));

	return layer_tool;
}




static bool tool_new_waypoint_click_cb(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
	LayerTRW * trw = (LayerTRW *) layer;

	VikCoord coord;
	if (trw->type != LayerType::TRW) {
		return false;
	}

	tool->viewport->screen_to_coord(event->x(), event->y(), &coord);
	if (trw->new_waypoint(trw->get_window(), &coord)) {
		trw->calculate_bounds_waypoints();
		if (trw->visible) {
			qDebug() << "II: Layer TRW: created new waypoint, will emit update";
			trw->emit_changed();
		}
	}
	return true;
}





/*** Edit trackpoint ****/

LayerTool * tool_edit_trackpoint_create(Window * window, Viewport * viewport)
{
	LayerTool * layer_tool = new LayerTool(window, viewport, LayerType::TRW);

	trw_layer_tools[5] = layer_tool;

	layer_tool->layer_type = LayerType::TRW;
	layer_tool->id_string = QString("EditTrackpoint");

	layer_tool->radioActionEntry.stock_id    = strdup(":/icons/layer_tool/trw_edit_tr_18.png");
	layer_tool->radioActionEntry.label       = strdup(N_("Edit Trac&kpoint"));
	layer_tool->radioActionEntry.accelerator = strdup("<control><shift>K");
	layer_tool->radioActionEntry.tooltip     = strdup(N_("Edit Trackpoint"));
	layer_tool->radioActionEntry.value       = 0;

	layer_tool->click = (ToolMouseFunc) tool_edit_trackpoint_click_cb;
	layer_tool->move = (ToolMouseMoveFunc) tool_edit_trackpoint_move_cb;
	layer_tool->release = (ToolMouseFunc) tool_edit_trackpoint_release_cb;

	layer_tool->cursor_click = new QCursor(QPixmap(":/cursors/trw_edit_tr.png"), 0, 0);
	layer_tool->cursor_release = new QCursor(Qt::ArrowCursor);

	layer_tool->ed = (tool_ed_t *) malloc(1 * sizeof (tool_ed_t));
	memset(layer_tool->ed, 0, sizeof (tool_ed_t));

	return layer_tool;
}




/**
 * On 'initial' click: search for the nearest trackpoint or routepoint and store it as the current trackpoint.
 * Then update the viewport, statusbar and edit dialog to draw the point as being selected and it's information.
 * On subsequent clicks: (as the current trackpoint is defined) and the click is very near the same point
 * then initiate the move operation to drag the point to a new destination.
 * NB The current trackpoint will get reset elsewhere.
 */
static bool tool_edit_trackpoint_click_cb(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
	LayerTRW * trw = (LayerTRW *) layer;

	TrackpointSearch search;
	search.viewport = tool->viewport;
	search.x = event->x();
	search.y = event->y();
	//search.closest_tp_iter = NULL;

	tool->viewport->get_bbox(&search.bbox);

	if (event->button() != Qt::LeftButton) {
		return false;
	}

	if (trw->type != LayerType::TRW) {
		return false;
	}
#ifdef K
	if (!trw->visible || !(trw->tracks_visible || trw->routes_visible)) {
		return false;
	}
#endif

	if (trw->selected_tp.valid) {
		/* First check if it is within range of prev. tp. and if current_tp track is shown. (if it is, we are moving that trackpoint). */

		if (!trw->current_trk) {
			return false;
		}

		Trackpoint * tp = *trw->selected_tp.iter;

		int x, y;
		tool->viewport->coord_to_screen(&tp->coord, &x, &y);

		if (trw->current_trk->visible
		    && abs(x - (int)round(event->x())) < TRACKPOINT_SIZE_APPROX
		    && abs(y - (int)round(event->y())) < TRACKPOINT_SIZE_APPROX) {

			marker_begin_move(tool, event->x(), event->y());
			return true;
		}

	}
#ifdef K
	if (trw->tracks_visible) {
#else
	if (1) {
#endif
		LayerTRWc::track_search_closest_tp(trw->tracks, &search);
	}

	if (search.closest_tp) {
		trw->tree_view->select_and_expose(search.closest_track->index);

		trw->selected_tp.iter = search.closest_tp_iter;
		trw->selected_tp.valid = true;

		trw->current_trk = search.closest_track;
		trw->trackpoint_properties_show();
		trw->set_statusbar_msg_info_trkpt(search.closest_tp);
		trw->emit_changed();
		return true;
	}

#ifdef K
	if (trw->routes_visible) {
#else
	if (1) {
#endif
		LayerTRWc::track_search_closest_tp(trw->routes, &search);
	}

	if (search.closest_tp) {
		trw->tree_view->select_and_expose(search.closest_track->index);

		trw->selected_tp.iter = search.closest_tp_iter;
		trw->selected_tp.iter = search.closest_tp_iter;
		trw->selected_tp.valid = true;

		trw->current_trk = search.closest_track;
		trw->trackpoint_properties_show();
		trw->set_statusbar_msg_info_trkpt(search.closest_tp);
		trw->emit_changed();
		return true;
	}
	/* These aren't the droids you're looking for. */
	return false;
}




static bool tool_edit_trackpoint_move_cb(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (trw->type != LayerType::TRW) {
		return false;
	}

	if (!tool->ed->holding) {
		return false;
	}

	VikCoord new_coord;
	tool->viewport->screen_to_coord(event->x(), event->y(), &new_coord);

	/* Snap to trackpoint. */
	if (event->modifiers() & Qt::ControlModifier) {
		Trackpoint * tp = trw->closest_tp_in_five_pixel_interval(tool->viewport, event->x(), event->y());
		if (tp && tp != *trw->selected_tp.iter) {
			new_coord = tp->coord;
		}
	}
	// trw->selected_tp.tp->coord = new_coord;
	int x, y;
	tool->viewport->coord_to_screen(&new_coord, &x, &y);
	marker_moveto(tool, x, y);

	return true;
}




static bool tool_edit_trackpoint_release_cb(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (trw->type != LayerType::TRW) {
		return false;
	}

	if (event->button() != Qt::LeftButton) {
		return false;
	}

	if (!tool->ed->holding) {
		return false;
	}

	VikCoord new_coord;
	tool->viewport->screen_to_coord(event->x(), event->y(), &new_coord);

	/* Snap to trackpoint */
	if (event->modifiers() & Qt::ControlModifier) {
		Trackpoint * tp = trw->closest_tp_in_five_pixel_interval(tool->viewport, event->x(), event->y());
		if (tp && tp != *trw->selected_tp.iter) {
			new_coord = tp->coord;
		}
	}

	(*trw->selected_tp.iter)->coord = new_coord;
	if (trw->current_trk) {
		trw->current_trk->calculate_bounds();
	}

	marker_end_move(tool);

	/* Diff dist is diff from orig. */
	if (trw->tpwin) {
		if (trw->current_trk) {
			trw->my_tpwin_set_tp();
		}
	}

	trw->emit_changed();

	return true;
}




/*** Extended Route Finder ***/

LayerTool * tool_extended_route_finder_create(Window * window, Viewport * viewport)
{
	LayerTool * layer_tool = new LayerTool(window, viewport, LayerType::TRW);

	trw_layer_tools[3] = layer_tool;

	layer_tool->layer_type = LayerType::TRW;
	layer_tool->id_string = QString("ExtendedRouteFinder");

	layer_tool->radioActionEntry.stock_id    = strdup(":/icons/layer_tool/trw_find_route_18.png");
	layer_tool->radioActionEntry.label       = strdup(N_("Route &Finder"));
	layer_tool->radioActionEntry.accelerator = strdup("<control><shift>F");
	layer_tool->radioActionEntry.tooltip     = strdup(N_("Route Finder"));
	layer_tool->radioActionEntry.value       = 0;

	layer_tool->click = (ToolMouseFunc) tool_extended_route_finder_click_cb;
	layer_tool->move = (ToolMouseMoveFunc) tool_new_track_move_cb;   /* Reuse these track methods on a route. */
	layer_tool->release = (ToolMouseFunc) tool_new_track_release_cb; /* Reuse these track methods on a route. */
	layer_tool->key_press = tool_extended_route_finder_key_press_cb;

	layer_tool->pan_handler = true;  /* Still need to handle clicks when in PAN mode to disable the potential trackpoint drawing. */
	layer_tool->cursor_click = new QCursor(QPixmap(":/cursors/trw____.png"), 0, 0);
	layer_tool->cursor_release = new QCursor(Qt::ArrowCursor);

	layer_tool->ed = (tool_ed_t *) malloc(sizeof (tool_ed_t));
	memset(layer_tool->ed, 0, sizeof (tool_ed_t));

	return layer_tool;
}




void LayerTRW::tool_extended_route_finder_undo()
{
	VikCoord * new_end = this->current_trk->cut_back_to_double_point();
	if (!new_end) {
		return;
	}

	free(new_end);
	this->emit_changed();

	/* Remove last ' to:...' */
	if (this->current_trk->comment) {
		char *last_to = strrchr(this->current_trk->comment, 't');
		if (last_to && (last_to - this->current_trk->comment > 1)) {
			char *new_comment = g_strndup(this->current_trk->comment,
						      last_to - this->current_trk->comment - 1);
			this->current_trk->set_comment_no_copy(new_comment);
		}
	}
}




static bool tool_extended_route_finder_click_cb(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
	LayerTRW * trw = (LayerTRW *) layer;

	VikCoord tmp;

	tool->viewport->screen_to_coord(event->x(), event->y(), &tmp);
	if (event->button() == Qt::RightButton && trw->current_trk) {
		trw->tool_extended_route_finder_undo();

	} else if (event->button() == Qt::MiddleButton) {
		trw->draw_sync_do = false;
		return false;
	}
	/* If we started the track but via undo deleted all the track points, begin again. */
	else if (trw->current_trk && trw->current_trk->sublayer_type == SublayerType::ROUTE && !trw->current_trk->get_tp_first()) {
		return trw->tool_new_track_or_route_click(event, tool->viewport);

	} else if ((trw->current_trk && trw->current_trk->sublayer_type == SublayerType::ROUTE)
		   || ((event->modifiers() & Qt::ControlModifier) && trw->current_trk)) {

		struct LatLon start, end;

		Trackpoint * tp_start = trw->current_trk->get_tp_last();
		vik_coord_to_latlon(&tp_start->coord, &start);
		vik_coord_to_latlon(&tmp, &end);

		trw->route_finder_started = true;
		trw->route_finder_append = true;  /* Merge tracks. Keep started true. */

#ifdef K

		/* Update UI to let user know what's going on. */
		VikStatusbar *sb = trw->get_window()->get_statusbar();
		VikRoutingEngine *engine = vik_routing_default_engine();
		if (!engine) {
			trw->get_window()->get_statusbar()->set_message(StatusBarField::INFO, "Cannot plan route without a default routing engine.");
			return true;
		}
		char *msg = g_strdup_printf(_("Querying %s for route between (%.3f, %.3f) and (%.3f, %.3f)."),
					    vik_routing_engine_get_label(engine),
					    start.lat, start.lon, end.lat, end.lon);
		trw->get_window()->get_statusbar()->set_message(StatusBarField::INFO, msg);
		free(msg);
		trw->get_window()->set_busy_cursor();


		/* Give GTK a change to display the new status bar before querying the web. */
		while (gtk_events_pending()) {
			gtk_main_iteration();
		}

		bool find_status = vik_routing_default_find(trw, start, end);

		/* Update UI to say we're done. */
		trw->get_window()->clear_busy_cursor();
		msg = (find_status)
			? g_strdup_printf(_("%s returned route between (%.3f, %.3f) and (%.3f, %.3f)."), vik_routing_engine_get_label(engine), start.lat, start.lon, end.lat, end.lon)
			: g_strdup_printf(_("Error getting route from %s."), vik_routing_engine_get_label(engine));

		trw->get_window()->get_statusbar()->set_message(StatusBarField::INFO, msg);
		free(msg);
#endif

		trw->emit_changed();
	} else {
		trw->current_trk = NULL;

		/* Create a new route where we will add the planned route to. */
		bool ret = tool_new_route_click_cb(trw, event, tool);

		trw->route_finder_started = true;

		return ret;
	}

	return true;
}




static bool tool_extended_route_finder_key_press_cb(Layer * layer, GdkEventKey * event, LayerTool * tool)
{
	LayerTRW * trw = (LayerTRW *) layer;

#ifdef K
	if (trw->current_trk && event->keyval == GDK_Escape) {
		trw->route_finder_started = false;
		trw->current_trk = NULL;
		trw->emit_changed();
		return true;
	} else if (trw->current_trk && event->keyval == GDK_BackSpace) {
		trw->tool_extended_route_finder_undo();
	}
#endif
	return false;
}




/*** Show picture ****/

LayerTool * tool_show_picture_create(Window * window, Viewport * viewport)
{
	LayerTool * layer_tool = new LayerTool(window, viewport, LayerType::TRW);

	trw_layer_tools[6] = layer_tool;

	layer_tool->layer_type = LayerType::TRW;
	layer_tool->id_string = QString("ShowPicture");

	layer_tool->radioActionEntry.stock_id    = strdup(":/icons/layer_tool/trw_show_picture_18.png");
	layer_tool->radioActionEntry.label       = strdup(N_("Show P&icture"));
	layer_tool->radioActionEntry.accelerator = strdup("<control><shift>I");
	layer_tool->radioActionEntry.tooltip     = strdup(N_("Show Picture"));
	layer_tool->radioActionEntry.value       = 0;

	layer_tool->click = (ToolMouseFunc) tool_show_picture_click_cb;

	layer_tool->cursor_click = new QCursor(QPixmap(":/cursors/trw____.png"), 0, 0);
	layer_tool->cursor_release = new QCursor(Qt::ArrowCursor);

	layer_tool->ed = (tool_ed_t *) malloc(sizeof (tool_ed_t));
	memset(layer_tool->ed, 0, sizeof (tool_ed_t));

	return layer_tool;
}




void LayerTRW::show_picture_cb(void) /* Slot. */
{
#ifdef K
	/* Thanks to the Gaim people for showing me ShellExecute and g_spawn_command_line_async. */
#ifdef WINDOWS
	ShellExecute(NULL, "open", (char *) data->misc, NULL, NULL, SW_SHOWNORMAL);
#else /* WINDOWS */
	GError *err = NULL;
	char *quoted_file = g_shell_quote((char *) data->misc);
	char *cmd = g_strdup_printf("%s %s", a_vik_get_image_viewer(), quoted_file);
	free(quoted_file);
	if (!g_spawn_command_line_async(cmd, &err)) {
		dialog_error(QString("Could not launch %1 to open file.").arg(Qstring(a_vik_get_image_viewer())), data->layer->get_window());
		g_error_free(err);
	}
	free(cmd);
#endif /* WINDOWS */
#endif
}




static bool tool_show_picture_click_cb(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (trw->type != LayerType::TRW) {
		return false;
	}

	char * found = LayerTRWc::tool_show_picture_wp(trw->waypoints, event->x(), event->y(), tool->viewport);
	if (found) {
#ifdef K
		trw_menu_sublayer_t data;
		memset(&data, 0, sizeof (trw_menu_sublayer_t));
		data.layer = trw;
		data.misc = found;
		trw->show_picture_cb(&data);
#endif
		return true; /* Found a match. */
	} else {
		return false; /* Go through other layers, searching for a match. */
	}
}
