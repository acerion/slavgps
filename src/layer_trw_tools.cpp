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

#include <glib/gstdio.h>

#include <QDebug>
#include <QPainter>

#include "layer_trw.h"
#include "layer_trw_painter.h"
#include "layer_trw_tools.h"
#include "layer_trw_dialogs.h"
#include "tree_view_internal.h"
#include "viewport_internal.h"
#include "track_internal.h"
#include "dialog.h"
#include "dem_cache.h"
#include "util.h"
#include "preferences.h"
#include "routing.h"




/* This is how it knows when you click if you are clicking close to a trackpoint. */
#define TRACKPOINT_SIZE_APPROX 5
#define WAYPOINT_SIZE_APPROX 5




using namespace SlavGPS;




extern LayerTool * trw_layer_tools[];

static bool tool_new_track_key_press(LayerTool * tool, LayerTRW * trw, QKeyEvent * ev);
static LayerToolFuncStatus tool_new_track_move(LayerTool * tool, LayerTRW * trw, QMouseEvent * ev);
static LayerToolFuncStatus tool_new_track_release(LayerTool * tool, LayerTRW * trw, QMouseEvent * ev);




/*
  ATM: Leave this as 'Track' only.
  Not overly bothered about having a snap to route trackpoint capability.
*/
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




bool LayerTRW::select_move(QMouseEvent * ev, Viewport * viewport, LayerTool * tool)
{
	if (!tool->sublayer_edit->holding) {
		return false;
	}

	Coord new_coord = viewport->screen_to_coord(ev->x(), ev->y());

	/* Here always allow snapping back to the original location.
	   This is useful when one decides not to move the thing after all.
	   If one wants to move the item only a little bit then don't hold down the 'snap' key! */

	/* Snap to trackpoint. */
	if (ev->modifiers() & Qt::ControlModifier) {
		Trackpoint * tp = this->closest_tp_in_five_pixel_interval(viewport, ev->x(), ev->y());
		if (tp) {
			new_coord = tp->coord;
		}
	}

	/* Snap to waypoint. */
	if (ev->modifiers() & Qt::ShiftModifier) {
		Waypoint * wp = this->closest_wp_in_five_pixel_interval(viewport, ev->x(), ev->y());
		if (wp) {
			new_coord = wp->coord;
		}
	}

	int x, y;
	viewport->coord_to_screen(&new_coord, &x, &y);

	tool->sublayer_edit_move(x, y);

	return true;
}




bool LayerTRW::select_release(QMouseEvent * ev, Viewport * viewport, LayerTool * tool)
{
	if (!tool->sublayer_edit->holding) {
		return false;
	}

	if (ev->button() != Qt::LeftButton) {
		return false;
	}

	/* Prevent accidental (small) shifts when specific movement has not been requested
	   (as the click release has occurred within the click object detection area). */
	if (!tool->sublayer_edit->moving
	    || tool->sublayer_edit->sublayer_type == SublayerType::NONE) {
		return false;
	}

	Coord new_coord = viewport->screen_to_coord(ev->x(), ev->y());

	/* Snap to trackpoint. */
	if (ev->modifiers() & Qt::ControlModifier) {
		Trackpoint * tp = this->closest_tp_in_five_pixel_interval(viewport, ev->x(), ev->y());
		if (tp) {
			new_coord = tp->coord;
		}
	}

	/* Snap to waypoint. */
	if (ev->modifiers() & Qt::ShiftModifier) {
		Waypoint * wp = this->closest_wp_in_five_pixel_interval(viewport, ev->x(), ev->y());
		if (wp) {
			new_coord = wp->coord;
		}
	}

	tool->sublayer_edit_release();

	/* Determine if working on a waypoint or a trackpoint. */
	if (tool->sublayer_edit->sublayer_type == SublayerType::WAYPOINT) {
		/* Update waypoint position. */
		this->current_wp->coord = new_coord;
		this->calculate_bounds_waypoints();
		/* Reset waypoint pointer. */
		this->current_wp = NULL;

	} else if (tool->sublayer_edit->sublayer_type == SublayerType::TRACK
		   || tool->sublayer_edit->sublayer_type == SublayerType::ROUTE) {

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
	} else {
		assert(0);
	}

	this->emit_changed();
	return true;
}




/*
  Returns true if a waypoint or track is found near the requested event position for this particular layer.
  The item found is automatically selected.
  This is a tool like feature but routed via the layer interface, since it's instigated by a 'global' layer tool in window.c
 */
bool LayerTRW::select_click(QMouseEvent * ev, Viewport * viewport, LayerTool * tool)
{
	if (ev->button() != Qt::LeftButton) {
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
		wp_search.x = ev->x();
		wp_search.y = ev->y();
		wp_search.draw_images = this->drawimages;

		LayerTRWc::waypoint_search_closest_tp(this->waypoints, &wp_search);

		if (wp_search.closest_wp) {

			/* Select. */
			this->tree_view->select_and_expose(wp_search.closest_wp->index);

			/* Too easy to move it so must be holding shift to start immediately moving it
			   or otherwise be previously selected but not have an image (otherwise clicking within image bounds (again) moves it). */
			if (ev->modifiers() & Qt::ShiftModifier
			    || (this->current_wp == wp_search.closest_wp && !this->current_wp->image)) {
				/* Put into 'move buffer'.
				   Viewport & window already set in tool. */
				tool->sublayer_edit->trw = this;
				tool->sublayer_edit->sublayer_type = SublayerType::WAYPOINT;

				tool->sublayer_edit_click(ev->x(), ev->y());
			}

			this->current_wp = wp_search.closest_wp;

#ifdef K
			if (ev->type == GDK_2BUTTON_PRESS) {
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
	tp_search.x = ev->x();
	tp_search.y = ev->y();
	tp_search.bbox = bbox;

	if (this->tracks_visible) {
		LayerTRWc::track_search_closest_tp(this->tracks, &tp_search);

		if (tp_search.closest_tp) {

			/* Always select + highlight the track. */
			this->tree_view->select_and_expose(tp_search.closest_track->index);

			tool->sublayer_edit->sublayer_type = SublayerType::TRACK;

			/* Select the Trackpoint.
			   Can move it immediately when control held or it's the previously selected tp. */
			if (ev->modifiers() & Qt::ControlModifier
			    || this->selected_tp.iter == tp_search.closest_tp_iter) {

				/* Put into 'move buffer'.
				   Viewport & window already set in tool. */
				tool->sublayer_edit->trw = this;
				tool->sublayer_edit_click(ev->x(), ev->y());
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

			tool->sublayer_edit->sublayer_type = SublayerType::ROUTE;

			/* Select the Trackpoint.
			   Can move it immediately when control held or it's the previously selected tp. */
			if (ev->modifiers() & Qt::ControlModifier
			    || this->selected_tp.iter == tp_search.closest_tp_iter) {

				/* Put into 'move buffer'.
				   Viewport & window already set in tool. */
				tool->sublayer_edit->trw = this;
				tool->sublayer_edit_click(ev->x(), ev->y());
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




/**
   Show context menu for the currently selected item.

   This function is called when generic Select tool is selected from tool bar.
   It would be nice to somehow merge this function with code used when "edit track/route/waypoint" tool is selected.
*/
bool LayerTRW::select_tool_context_menu(QMouseEvent * ev, Viewport * viewport)
{
	if (ev->button() != Qt::RightButton) {
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

	if (this->current_trk && this->current_trk->visible) { /* Track or Route. */
		if (!this->current_trk->name.isEmpty()) {
			this->menu_data->sublayer = this->current_trk;
			this->menu_data->viewport = viewport;

			QMenu menu(viewport);

			this->sublayer_add_menu_items(menu);
			menu.exec(QCursor::pos());
			return true;
		}
	} else if (this->current_wp && this->current_wp->visible) {
		if (!this->current_wp->name.isEmpty()) {
			this->menu_data->sublayer = this->current_wp;
			this->menu_data->viewport = viewport;

			QMenu menu(viewport);

			this->sublayer_add_menu_items(menu);
			menu.exec(QCursor::pos());
			return true;
		}
	} else {
		; /* No Track/Route/Waypoint selected. */
	}
	return false;
}




LayerTool * tool_edit_waypoint_create(Window * window, Viewport * viewport)
{
	return new LayerToolTRWEditWaypoint(window, viewport);
}




LayerToolTRWEditWaypoint::LayerToolTRWEditWaypoint(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::TRW)
{
	this->id_string = QString("trw.edit_waypoint");

	this->action_icon_path   = ":/icons/layer_tool/trw_edit_wp_18.png";
	this->action_label       = QObject::tr("&Edit Waypoint");
	this->action_tooltip     = QObject::tr("Edit Waypoint");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_E;

	this->cursor_click = new QCursor(QPixmap(":/cursors/trw_edit_wp.png"), 0, 0);
	this->cursor_release = new QCursor(Qt::ArrowCursor);

	this->sublayer_edit = new SublayerEdit;

	Layer::get_interface(LayerType::TRW)->layer_tools.insert({{ LAYER_TRW_TOOL_EDIT_WAYPOINT, this }});
}




LayerToolFuncStatus LayerToolTRWEditWaypoint::click_(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (trw->type != LayerType::TRW) {
		return LayerToolFuncStatus::IGNORE;
	}
	if (this->sublayer_edit->holding) {
		return LayerToolFuncStatus::ACK;
	}
#ifdef K
	if (!trw->visible || !trw->waypoints_visible) {
		return LayerToolFuncStatus::IGNORE;
	}
#endif
	if (trw->current_wp && trw->current_wp->visible) {
		/* First check if current WP is within area (other may be 'closer', but we want to move the current). */
		int x, y;
		this->viewport->coord_to_screen(&trw->current_wp->coord, &x, &y);

		if (abs(x - (int) round(ev->x())) <= WAYPOINT_SIZE_APPROX
		    && abs(y - (int) round(ev->y())) <= WAYPOINT_SIZE_APPROX) {
			if (ev->button() == Qt::RightButton) {
				trw->waypoint_rightclick = true; /* Remember that we're clicking; other layers will ignore release signal. */
			} else {
				this->sublayer_edit_click(ev->x(), ev->y());
			}
			return LayerToolFuncStatus::ACK;
		}
	}

	WaypointSearch search;
	search.viewport = this->viewport;
	search.x = ev->x();
	search.y = ev->y();
	search.draw_images = trw->drawimages;

	LayerTRWc::waypoint_search_closest_tp(trw->waypoints, &search);
	if (trw->current_wp && (trw->current_wp == search.closest_wp)) {
		if (ev->button() == Qt::RightButton) {
			trw->waypoint_rightclick = true; /* Remember that we're clicking; other layers will ignore release signal. */
		} else {
			this->sublayer_edit_click(ev->x(), ev->y());
		}
		return LayerToolFuncStatus::IGNORE;
	} else if (search.closest_wp) {
		if (ev->button() == Qt::RightButton) {
			trw->waypoint_rightclick = true; /* Remember that we're clicking; other layers will ignore release signal. */
		} else {
			trw->waypoint_rightclick = false;
		}

		trw->tree_view->select_and_expose(search.closest_wp->index);

		trw->current_wp = search.closest_wp;

		/* Could make it so don't update if old WP is off screen and new is null but oh well. */
		trw->emit_changed();
		return LayerToolFuncStatus::ACK;
	}

	trw->current_wp = NULL;
	trw->waypoint_rightclick = false;
	trw->emit_changed();

	return LayerToolFuncStatus::IGNORE;
}




LayerToolFuncStatus LayerToolTRWEditWaypoint::move_(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (trw->type != LayerType::TRW) {
		return LayerToolFuncStatus::IGNORE;
	}

	if (!this->sublayer_edit->holding) {
		return LayerToolFuncStatus::IGNORE;
	}

	Coord new_coord = this->viewport->screen_to_coord(ev->x(), ev->y());

	if (ev->modifiers() & Qt::ControlModifier) { /* Snap to trackpoint. */
		Trackpoint * tp = trw->closest_tp_in_five_pixel_interval(this->viewport, ev->x(), ev->y());
		if (tp) {
			new_coord = tp->coord;
		}
	} else if (ev->modifiers() & Qt::ShiftModifier) { /* Snap to waypoint. */
		Waypoint * wp = trw->closest_wp_in_five_pixel_interval(this->viewport, ev->x(), ev->y());
		if (wp && wp != trw->current_wp) {
			new_coord = wp->coord;
		}
	} else {
		; /* No modifiers. */
	}

	int x, y;
	this->viewport->coord_to_screen(&new_coord, &x, &y);
	this->sublayer_edit_move(x, y);

	return LayerToolFuncStatus::ACK;
}




LayerToolFuncStatus LayerToolTRWEditWaypoint::release_(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (trw->type != LayerType::TRW) {
		return LayerToolFuncStatus::IGNORE;
	}

	if (!this->sublayer_edit->holding) {
		return LayerToolFuncStatus::IGNORE;
	}

	if (ev->button() == Qt::LeftButton) {
		Coord new_coord = this->viewport->screen_to_coord(ev->x(), ev->y());

		/* Snap to trackpoint. */
		if (ev->modifiers() & Qt::ControlModifier) {
			Trackpoint * tp = trw->closest_tp_in_five_pixel_interval(this->viewport, ev->x(), ev->y());
			if (tp) {
				new_coord = tp->coord;
			}
		}

		/* Snap to waypoint. */
		if (ev->modifiers() & Qt::ShiftModifier) {
			Waypoint * wp = trw->closest_wp_in_five_pixel_interval(this->viewport, ev->x(), ev->y());
			if (wp && wp != trw->current_wp) {
				new_coord = wp->coord;
			}
		}

		this->sublayer_edit_release();

		trw->current_wp->coord = new_coord;

		trw->calculate_bounds_waypoints();
		trw->emit_changed();
		return LayerToolFuncStatus::ACK;

	} else if (ev->button() == Qt::RightButton && trw->waypoint_rightclick) {
		if (trw->current_wp) {
			trw->menu_data->sublayer = trw->current_wp;
			trw->menu_data->viewport = this->viewport;

			QMenu menu;
			trw->sublayer_add_menu_items(menu);
			menu.exec(QCursor::pos());
		}
		trw->waypoint_rightclick = false;

		return LayerToolFuncStatus::IGNORE;
	} else {
		return LayerToolFuncStatus::IGNORE;
	}
}




LayerTool * tool_new_track_create(Window * window, Viewport * viewport)
{
	return new LayerToolTRWNewTrack(window, viewport);
}




LayerToolTRWNewTrack::LayerToolTRWNewTrack(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::TRW)
{
	this->id_string = QString("trw.create_track");

	this->action_icon_path   = ":/icons/layer_tool/trw_add_tr_18.png";
	this->action_label       = QObject::tr("Create &Track");
	this->action_tooltip     = QObject::tr("Create Track");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_T;

	this->pan_handler = true;  /* Still need to handle clicks when in PAN mode to disable the potential trackpoint drawing. */
	this->cursor_click = new QCursor(QPixmap(":/cursors/trw_add_tr.png"), 0, 0);
	this->cursor_release = new QCursor(Qt::ArrowCursor);

	this->sublayer_edit = new SublayerEdit;

	Layer::get_interface(LayerType::TRW)->layer_tools.insert({{ LAYER_TRW_TOOL_CREATE_TRACK, this }});
}




bool LayerToolTRWNewTrack::key_press_(Layer * layer, QKeyEvent * ev)
{
	return tool_new_track_key_press(this, (LayerTRW *) layer, ev);
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




static QString distance_string(double distance)
{
	QString result;
	char str[128];

	/* Draw label with distance. */
	DistanceUnit distance_unit = Preferences::get_unit_distance();
	switch (distance_unit) {
	case DistanceUnit::MILES:
		if (distance >= VIK_MILES_TO_METERS(1) && distance < VIK_MILES_TO_METERS(100)) {
			result = QObject::tr("%1 miles").arg(VIK_METERS_TO_MILES(distance), 6, 'f', 2); /* "%3.2f" */
		} else if (distance < 1609.4) {
			result = QObject::tr("%1 yards").arg((int) (distance * 1.0936133));
		} else {
			result = QObject::tr("%1 miles").arg((int) VIK_METERS_TO_MILES(distance));
		}
		break;
	case DistanceUnit::NAUTICAL_MILES:
		if (distance >= VIK_NAUTICAL_MILES_TO_METERS(1) && distance < VIK_NAUTICAL_MILES_TO_METERS(100)) {
			result = QObject::tr("%1 NM").arg(VIK_METERS_TO_NAUTICAL_MILES(distance), 6, 'f', 2); /* "%3.2f" */
		} else if (distance < VIK_NAUTICAL_MILES_TO_METERS(1)) {
			result = QObject::tr("%1 yards").arg((int) (distance * 1.0936133));
		} else {
			result = QObject::tr("%1 NM").arg((int) VIK_METERS_TO_NAUTICAL_MILES(distance));
		}
		break;
	default:
		/* DistanceUnit::KILOMETRES */
		if (distance >= 1000 && distance < 100000) {
			result = QObject::tr("1 km").arg(distance/1000.0, 6, 'f', 2); /* ""%3.2f" */
		} else if (distance < 1000) {
			result = QObject::tr("%1 m").arg((int) distance);
		} else {
			result = QObject::tr("%1 km").arg((int) distance/1000);
		}
		break;
	}
	return result;
}




/*
 * Actually set the message in statusbar.
 */
static void statusbar_write(double distance, double elev_gain, double elev_loss, double last_step, double angle, LayerTRW * layer)
{
	/* Only show elevation data when track has some elevation properties. */
	QString str_gain_loss;
	QString str_last_step;
	const QString str_total = distance_string(distance);

	if ((elev_gain > 0.1) || (elev_loss > 0.1)) {
		if (Preferences::get_unit_height() == HeightUnit::METRES) {
			str_gain_loss = QObject::tr(" - Gain %1m:Loss %2m").arg((int) elev_gain).arg((int) elev_loss);
		} else {
			str_gain_loss = QObject::tr(" - Gain %1ft:Loss %2ft").arg((int) VIK_METERS_TO_FEET(elev_gain)).arg((int)VIK_METERS_TO_FEET(elev_loss));
		}
	}

	if (last_step > 0) {
		const QString dist = distance_string(last_step);
		str_last_step = QObject::tr(" - Bearing %1° - Step %2").arg(RAD2DEG(angle), 4, 'f', 1).arg(dist); /* "%3.1f" */
	}

	/* Write with full gain/loss information. */
	const QString msg = QObject::tr("Total %1%2%3").arg(str_total).arg(str_last_step).arg(str_gain_loss);
	layer->get_window()->get_statusbar()->set_message(StatusBarField::INFO, msg);
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




LayerToolFuncStatus LayerToolTRWNewTrack::move_(Layer * layer, QMouseEvent * ev)
{
	return tool_new_track_move(this, (LayerTRW *) layer, ev);
}




static LayerToolFuncStatus tool_new_track_move(LayerTool * tool, LayerTRW * trw, QMouseEvent * ev)
{
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
		qDebug() << "II: Layer TRW: drawing line" << x1 << y1 << ev->x() << ev->y();
		painter.drawLine(x1, y1, ev->x(), ev->y());

		/* Using this reset method is more reliable than trying to undraw previous efforts via the GDK_INVERT method. */

		/* Find out actual distance of current track. */
		double distance = trw->current_trk->get_length();

		/* Now add distance to where the pointer is. */
		Coord coord = tool->viewport->screen_to_coord(ev->x(), ev->y());
		struct LatLon ll = coord.get_latlon();
		double last_step = Coord::distance(coord, last_tpt->coord);
		distance = distance + last_step;

		/* Get elevation data. */
		double elev_gain, elev_loss;
		trw->current_trk->get_total_elevation_gain(&elev_gain, &elev_loss);

		/* Adjust elevation data (if available) for the current pointer position. */
		double elev_new = (double) DEMCache::get_elev_by_coord(&coord, DemInterpolation::BEST);
		if (elev_new != DEM_INVALID_ELEVATION) {
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
		if (Preferences::get_create_track_tooltip()) {
			QString str = distance_string(distance);
#ifdef K
			PangoLayout *pl = gtk_widget_create_pango_layout(tool->viewport), NULL);
			pango_layout_set_font_description(pl, gtk_widget_get_style(tool->viewport)->font_desc);
			pango_layout_set_text(pl, str, -1);
			int wd, hd;
			pango_layout_get_pixel_size(pl, &wd, &hd);

			int xd,yd;
			/* Offset from cursor a bit depending on font size. */
			xd = ev->x + 10;
			yd = ev->y - hd;

			/* Create a background block to make the text easier to read over the background map. */
			QPen * background_block_pen = tool->viewport->new_pen("#cccccc", 1);
			fill_rectangle(pixmap, background_block_pen, xd-2, yd-2, wd+4, hd+2);
			gdk_draw_layout(pixmap, trw->current_trk_new_point_pen, xd, yd, pl);

			g_object_unref(G_OBJECT (pl));
			g_object_unref(G_OBJECT (background_block_pen));
#endif
		}

		double angle;
		double baseangle;
		tool->viewport->compute_bearing(x1, y1, ev->x(), ev->y(), &angle, &baseangle);

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




static bool tool_new_track_key_press(LayerTool * tool, LayerTRW * trw, QKeyEvent * ev)
{
	if (trw->current_trk && ev->key() == Qt::Key_Escape) {
		/* Bin track if only one point as it's not very useful. */
		if (trw->current_trk->get_tp_count() == 1) {
			if (trw->current_trk->sublayer_type == SublayerType::ROUTE) {
				trw->delete_route(trw->current_trk);
			} else {
				trw->delete_track(trw->current_trk);
			}
		}
		trw->current_trk = NULL;
		trw->emit_changed();
		return true;
	} else if (trw->current_trk && ev->key() == Qt::Key_Backspace) {
		trw->undo_trackpoint_add();
		trw->update_statusbar();
		trw->emit_changed();
		return true;
	} else {
		return false;
	}
}




/*
 * Common function to handle trackpoint button requests on either a route or a track
 *  . enables adding a point via normal click
 *  . enables removal of last point via right click
 */
LayerToolFuncStatus LayerTRW::tool_new_track_or_route_click(QMouseEvent * ev, Viewport * viewport)
{
	if (this->type != LayerType::TRW) {
		return LayerToolFuncStatus::IGNORE;
	}

	if (ev->button() == Qt::MiddleButton) {
		/* As the display is panning, the new track pixmap is now invalid so don't draw it
		   otherwise this drawing done results in flickering back to an old image. */
		this->draw_sync_do = false;
		return LayerToolFuncStatus::IGNORE;
	}

	if (ev->button() == Qt::RightButton) {
		if (!this->current_trk) {
			return LayerToolFuncStatus::IGNORE;
		}
		this->undo_trackpoint_add();
		this->update_statusbar();
		this->emit_changed();
		return LayerToolFuncStatus::ACK;
	}

	Trackpoint * tp = new Trackpoint();
	tp->coord = viewport->screen_to_coord(ev->x(), ev->y());

	/* Snap to other TP. */
	if (ev->modifiers() & Qt::ControlModifier) {
		Trackpoint * other_tp = this->closest_tp_in_five_pixel_interval(viewport, ev->x(), ev->y());
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
	this->ct_x2 = ev->x();
	this->ct_y2 = ev->y();

	this->emit_changed();

	return LayerToolFuncStatus::ACK;
}




LayerToolFuncStatus LayerToolTRWNewTrack::click_(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	/* If we were running the route finder, cancel it. */
	trw->route_finder_started = false;

	/* If current is a route - switch to new track. */
	if (ev->button() != Qt::LeftButton) {
		/* TODO: this shouldn't even happen. */
		return LayerToolFuncStatus::IGNORE;
	}

	/* If either no track/route was being created
	   or we were in the middle of creating a route... */
	if (!trw->current_trk
	    || (trw->current_trk && trw->current_trk->sublayer_type == SublayerType::ROUTE)) {

		QString new_name = trw->new_unique_sublayer_name(SublayerType::TRACK, QObject::tr("Track"));
		if (Preferences::get_ask_for_create_track_name()) {
			new_name = a_dialog_new_track(new_name, false, trw->get_window());
			if (new_name.isEmpty()) {
				return LayerToolFuncStatus::IGNORE;
			}
		}
		trw->new_track_create_common(new_name);
	}

	return trw->tool_new_track_or_route_click(ev, this->viewport);
}




LayerToolFuncStatus LayerToolTRWNewTrack::double_click_(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;
	if (trw->type != LayerType::TRW) {
		return LayerToolFuncStatus::IGNORE;
	}

	if (ev->button() != Qt::LeftButton) {
		return LayerToolFuncStatus::IGNORE;
	}

	/* Subtract last (duplicate from double click) tp then end. */
	if (trw->current_trk && !trw->current_trk->empty() /*  && trw->ct_x1 == trw->ct_x2 && trw->ct_y1 == trw->ct_y2 */) {
		/* Undo last, then end.
		   TODO: I think that in current implementation of handling of double click we don't need the undo. */
		trw->undo_trackpoint_add();
		trw->current_trk = NULL;
	}
	trw->emit_changed();
	return LayerToolFuncStatus::ACK;
}




LayerToolFuncStatus LayerToolTRWNewTrack::release_(Layer * layer, QMouseEvent * ev)
{
	return tool_new_track_release(this, (LayerTRW *) layer, ev);
}




static LayerToolFuncStatus tool_new_track_release(LayerTool * tool, LayerTRW * trw, QMouseEvent * ev)
{
	if (ev->button() == Qt::MiddleButton) {
		/* Pan moving ended - enable potential point drawing again. */
		trw->draw_sync_do = true;
		trw->draw_sync_done = true;
	}

	return LayerToolFuncStatus::ACK;
}




/*** New route ****/

LayerTool * tool_new_route_create(Window * window, Viewport * viewport)
{
	return new LayerToolTRWNewRoute(window, viewport);
}


LayerToolTRWNewRoute::LayerToolTRWNewRoute(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::TRW)
{
	this->id_string = QString("trw.create_route");

	this->action_icon_path   = ":/icons/layer_tool/trw_add_route_18.png";
	this->action_label       = QObject::tr("Create &Route");
	this->action_tooltip     = QObject::tr("Create Route");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_B;

	this->pan_handler = true;  /* Still need to handle clicks when in PAN mode to disable the potential trackpoint drawing. */
	this->cursor_click = new QCursor(QPixmap(":/cursors/trw_add_route.png"), 0, 0);
	this->cursor_release = new QCursor(Qt::ArrowCursor);

	this->sublayer_edit = new SublayerEdit;

	Layer::get_interface(LayerType::TRW)->layer_tools.insert({{ LAYER_TRW_TOOL_CREATE_ROUTE, this }});
}




LayerToolFuncStatus LayerToolTRWNewRoute::click_(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	/* If we were running the route finder, cancel it. */
	trw->route_finder_started = false;

	if (ev->button() != Qt::LeftButton) {
		/* TODO: this shouldn't even happen. */
		return LayerToolFuncStatus::IGNORE;
	}

	/* If either no track/route was being created
	   or we were in the middle of creating a track.... */
	if (!trw->current_trk
	    || (trw->current_trk && trw->current_trk->sublayer_type == SublayerType::TRACK)) {

		QString new_name = trw->new_unique_sublayer_name(SublayerType::ROUTE, QObject::tr("Route"));
		if (Preferences::get_ask_for_create_track_name()) {
			new_name = a_dialog_new_track(new_name, true, trw->get_window());
			if (new_name.isEmpty()) {
				return LayerToolFuncStatus::IGNORE;
			}
		}

		trw->new_route_create_common(new_name);
	}
	return trw->tool_new_track_or_route_click(ev, this->viewport);
}




LayerToolFuncStatus LayerToolTRWNewRoute::move_(Layer * layer, QMouseEvent * ev)
{
	return tool_new_track_move(this, (LayerTRW *) layer, ev);
}




LayerToolFuncStatus LayerToolTRWNewRoute::release_(Layer * layer, QMouseEvent * ev)
{
	return tool_new_track_release(this, (LayerTRW *) layer, ev);
}




bool LayerToolTRWNewRoute::key_press_(Layer * layer, QKeyEvent * ev)
{
	return tool_new_track_key_press(this, (LayerTRW *) layer, ev);
}




/*** New waypoint ****/

LayerTool * tool_new_waypoint_create(Window * window, Viewport * viewport)
{
	return new LayerToolTRWNewWaypoint(window, viewport);
}



LayerToolTRWNewWaypoint::LayerToolTRWNewWaypoint(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::TRW)
{
	this->id_string = QString("trw.create_waypoint");

	this->action_icon_path   = ":/icons/layer_tool/trw_add_wp_18.png";
	this->action_label       = QObject::tr("Create &Waypoint");
	this->action_tooltip     = QObject::tr("Create Waypoint");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_W;

	this->cursor_click = new QCursor(QPixmap(":/cursors/trw_add_wp.png"), 0, 0);
	this->cursor_release = new QCursor(Qt::ArrowCursor);

	this->sublayer_edit = new SublayerEdit;

	Layer::get_interface(LayerType::TRW)->layer_tools.insert({{ LAYER_TRW_TOOL_CREATE_WAYPOINT, this }});
}




LayerToolFuncStatus LayerToolTRWNewWaypoint::click_(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (trw->type != LayerType::TRW) {
		return LayerToolFuncStatus::IGNORE;
	}

	Coord coord = this->viewport->screen_to_coord(ev->x(), ev->y());
	if (trw->new_waypoint(trw->get_window(), &coord)) {
		trw->calculate_bounds_waypoints();
		if (trw->visible) {
			qDebug() << "II: Layer TRW: created new waypoint, will emit update";
			trw->emit_changed();
		}
	}
	return LayerToolFuncStatus::ACK;
}





/*** Edit trackpoint ****/

LayerTool * tool_edit_trackpoint_create(Window * window, Viewport * viewport)
{
	return new LayerToolTRWEditTrackpoint(window, viewport);
}




LayerToolTRWEditTrackpoint::LayerToolTRWEditTrackpoint(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::TRW)
{
	this->id_string = QString("trw.edit_trackpoint");

	this->action_icon_path   = ":/icons/layer_tool/trw_edit_tr_18.png";
	this->action_label       = QObject::tr("Edit Trac&kpoint");
	this->action_tooltip     = QObject::tr("Edit Trackpoint");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_K;

	this->cursor_click = new QCursor(QPixmap(":/cursors/trw_edit_tr.png"), 0, 0);
	this->cursor_release = new QCursor(Qt::ArrowCursor);

	this->sublayer_edit = new SublayerEdit;

	Layer::get_interface(LayerType::TRW)->layer_tools.insert({{ LAYER_TRW_TOOL_EDIT_TRACKPOINT, this }});
}




/**
 * On 'initial' click: search for the nearest trackpoint or routepoint and store it as the current trackpoint.
 * Then update the viewport, statusbar and edit dialog to draw the point as being selected and it's information.
 * On subsequent clicks: (as the current trackpoint is defined) and the click is very near the same point
 * then initiate the move operation to drag the point to a new destination.
 * NB The current trackpoint will get reset elsewhere.
 */
LayerToolFuncStatus LayerToolTRWEditTrackpoint::click_(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	TrackpointSearch search;
	search.viewport = this->viewport;
	search.x = ev->x();
	search.y = ev->y();
	//search.closest_tp_iter = NULL;

	this->viewport->get_bbox(&search.bbox);

	if (ev->button() != Qt::LeftButton) {
		return LayerToolFuncStatus::IGNORE;
	}

	if (trw->type != LayerType::TRW) {
		return LayerToolFuncStatus::IGNORE;
	}
#ifdef K
	if (!trw->visible || !(trw->tracks_visible || trw->routes_visible)) {
		return LayerToolFuncStatus::IGNORE;
	}
#endif

	if (trw->selected_tp.valid) {
		/* First check if it is within range of prev. tp. and if current_tp track is shown. (if it is, we are moving that trackpoint). */

		if (!trw->current_trk) {
			return LayerToolFuncStatus::IGNORE;
		}

		Trackpoint * tp = *trw->selected_tp.iter;

		int x, y;
		this->viewport->coord_to_screen(&tp->coord, &x, &y);

		if (trw->current_trk->visible
		    && abs(x - (int) round(ev->x())) < TRACKPOINT_SIZE_APPROX
		    && abs(y - (int) round(ev->y())) < TRACKPOINT_SIZE_APPROX) {

			this->sublayer_edit_click(ev->x(), ev->y());
			return LayerToolFuncStatus::ACK;
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
		return LayerToolFuncStatus::ACK;
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
		return LayerToolFuncStatus::ACK;
	}
	/* These aren't the droids you're looking for. */
	return LayerToolFuncStatus::IGNORE;
}




LayerToolFuncStatus LayerToolTRWEditTrackpoint::move_(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (trw->type != LayerType::TRW) {
		return LayerToolFuncStatus::IGNORE;
	}

	if (!this->sublayer_edit->holding) {
		return LayerToolFuncStatus::IGNORE;
	}

	Coord new_coord = this->viewport->screen_to_coord(ev->x(), ev->y());

	/* Snap to trackpoint. */
	if (ev->modifiers() & Qt::ControlModifier) {
		Trackpoint * tp = trw->closest_tp_in_five_pixel_interval(this->viewport, ev->x(), ev->y());
		if (tp && tp != *trw->selected_tp.iter) {
			new_coord = tp->coord;
		}
	}
	// trw->selected_tp.tp->coord = new_coord;
	int x, y;
	this->viewport->coord_to_screen(&new_coord, &x, &y);
	this->sublayer_edit_move(x, y);

	return LayerToolFuncStatus::ACK;
}




LayerToolFuncStatus LayerToolTRWEditTrackpoint::release_(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (trw->type != LayerType::TRW) {
		return LayerToolFuncStatus::IGNORE;
	}

	if (ev->button() != Qt::LeftButton) {
		return LayerToolFuncStatus::IGNORE;
	}

	if (!this->sublayer_edit->holding) {
		return LayerToolFuncStatus::IGNORE;
	}

	Coord new_coord = this->viewport->screen_to_coord(ev->x(), ev->y());

	/* Snap to trackpoint */
	if (ev->modifiers() & Qt::ControlModifier) {
		Trackpoint * tp = trw->closest_tp_in_five_pixel_interval(this->viewport, ev->x(), ev->y());
		if (tp && tp != *trw->selected_tp.iter) {
			new_coord = tp->coord;
		}
	}

	(*trw->selected_tp.iter)->coord = new_coord;
	if (trw->current_trk) {
		trw->current_trk->calculate_bounds();
	}

	this->sublayer_edit_release();

	/* Diff dist is diff from orig. */
	if (trw->tpwin) {
		if (trw->current_trk) {
			trw->my_tpwin_set_tp();
		}
	}

	trw->emit_changed();

	return LayerToolFuncStatus::ACK;
}




/*** Extended Route Finder ***/

LayerTool * tool_extended_route_finder_create(Window * window, Viewport * viewport)
{
	return new LayerToolTRWExtendedRouteFinder(window, viewport);
}




LayerToolTRWExtendedRouteFinder::LayerToolTRWExtendedRouteFinder(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::TRW)
{
	this->id_string = QString("trw.extended_route_finder");

	this->action_icon_path   = ":/icons/layer_tool/trw_find_route_18.png";
	this->action_label       = QObject::tr("Route &Finder");
	this->action_tooltip     = QObject::tr("Route Finder");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_F;

	this->pan_handler = true;  /* Still need to handle clicks when in PAN mode to disable the potential trackpoint drawing. */
	this->cursor_click = new QCursor(QPixmap(":/cursors/trw____.png"), 0, 0);
	this->cursor_release = new QCursor(Qt::ArrowCursor);

	this->sublayer_edit = new SublayerEdit;

	Layer::get_interface(LayerType::TRW)->layer_tools.insert({{ LAYER_TRW_TOOL_ROUTE_FINDER, this }});
}




LayerToolFuncStatus LayerToolTRWExtendedRouteFinder::move_(Layer * layer, QMouseEvent * ev)
{
	return tool_new_track_move(this, (LayerTRW *) layer, ev);
}




LayerToolFuncStatus LayerToolTRWExtendedRouteFinder::release_(Layer * layer, QMouseEvent * ev)
{
	return tool_new_track_release(this, (LayerTRW *) layer, ev);
}



void LayerTRW::tool_extended_route_finder_undo()
{
	Coord * new_end = this->current_trk->cut_back_to_double_point();
	if (!new_end) {
		return;
	}
	delete new_end;

	this->emit_changed();
#ifdef K
	/* Remove last ' to:...' */
	if (!this->current_trk->comment_.isEmpty()) {
		char *last_to = strrchr(this->current_trk->comment_, 't');
		if (last_to && (last_to - this->current_trk->comment_ > 1)) {
			char *new_comment = g_strndup(this->current_trk->comment_,
						      last_to - this->current_trk->comment_ - 1); /* FIXME: memory leak. */
			this->current_trk->set_comment(QString(new_comment));
		}
	}
#endif
}




LayerToolFuncStatus LayerToolTRWExtendedRouteFinder::click_(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	Coord tmp = this->viewport->screen_to_coord(ev->x(), ev->y());
	if (ev->button() == Qt::RightButton && trw->current_trk) {
		trw->tool_extended_route_finder_undo();

	} else if (ev->button() == Qt::MiddleButton) {
		trw->draw_sync_do = false;
		return LayerToolFuncStatus::IGNORE;
	}
	/* If we started the track but via undo deleted all the track points, begin again. */
	else if (trw->current_trk && trw->current_trk->sublayer_type == SublayerType::ROUTE && !trw->current_trk->get_tp_first()) {
		return trw->tool_new_track_or_route_click(ev, this->viewport);

	} else if ((trw->current_trk && trw->current_trk->sublayer_type == SublayerType::ROUTE)
		   || ((ev->modifiers() & Qt::ControlModifier) && trw->current_trk)) {

		Trackpoint * tp_start = trw->current_trk->get_tp_last();
		struct LatLon start = tp_start->coord.get_latlon();
		struct LatLon end = tmp.get_latlon();

		trw->route_finder_started = true;
		trw->route_finder_append = true;  /* Merge tracks. Keep started true. */



		/* Update UI to let user know what's going on. */
		StatusBar * sb = trw->get_window()->get_statusbar();
		RoutingEngine * engine = routing_default_engine();
		if (!engine) {
			trw->get_window()->get_statusbar()->set_message(StatusBarField::INFO, "Cannot plan route without a default routing engine.");
			return LayerToolFuncStatus::ACK;
		}
		const QString msg1 = QObject::tr("Querying %1 for route between (%2, %3) and (%4, %5).")
			.arg(engine->get_label())
			.arg(start.lat, 0, 'f', 3) /* ".3f" */
			.arg(start.lon, 0, 'f', 3)
			.arg(end.lat, 0, 'f', 3)
			.arg(end.lon, 0, 'f', 3);
		trw->get_window()->get_statusbar()->set_message(StatusBarField::INFO, msg1);
		trw->get_window()->set_busy_cursor();

 #ifdef K
		/* Give GTK a change to display the new status bar before querying the web. */
		while (gtk_events_pending()) {
			gtk_main_iteration();
		}
#endif
		bool find_status = routing_default_find(trw, start, end);

		/* Update UI to say we're done. */
		trw->get_window()->clear_busy_cursor();
		const QString msg2 = (find_status)
			? QObject::tr("%1 returned route between (%2, %3) and (%4, %5).").arg(engine->get_label()).arg(start.lat, 0, 'f', 3).arg(start.lon, 0, 'f', 3).arg(end.lat, 0, 'f', 3).arg(end.lon, 0, 'f', 3) /* ".3f" */
			: QObject::tr("Error getting route from %1.").arg(engine->get_label());

		trw->get_window()->get_statusbar()->set_message(StatusBarField::INFO, msg2);


		trw->emit_changed();
	} else {
		trw->current_trk = NULL;

		/* Create a new route where we will add the planned route to. */
		LayerToolFuncStatus ret = this->click_(trw, ev);

		trw->route_finder_started = true;

		return ret;
	}

	return LayerToolFuncStatus::ACK;
}




bool LayerToolTRWExtendedRouteFinder::key_press_(Layer * layer, QKeyEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (trw->current_trk && ev->key() == Qt::Key_Escape) {
		trw->route_finder_started = false;
		trw->current_trk = NULL;
		trw->emit_changed();
		return true;
	} else if (trw->current_trk && ev->key() == Qt::Key_Backspace) {
		trw->tool_extended_route_finder_undo();
		return true;
	} else {
		return false;
	}
}




LayerTool * tool_show_picture_create(Window * window, Viewport * viewport)
{
	return new LayerToolTRWShowPicture(window, viewport);
}




LayerToolTRWShowPicture::LayerToolTRWShowPicture(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::TRW)
{
	this->id_string = QString("trw.show_picture");

	this->action_icon_path   = ":/icons/layer_tool/trw_show_picture_18.png";
	this->action_label       = QObject::tr("Show P&icture");
	this->action_tooltip     = QObject::tr("Show Picture");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_I;

	this->cursor_click = new QCursor(QPixmap(":/cursors/trw____.png"), 0, 0);
	this->cursor_release = new QCursor(Qt::ArrowCursor);

	this->sublayer_edit = new SublayerEdit;

	Layer::get_interface(LayerType::TRW)->layer_tools.insert({{ LAYER_TRW_TOOL_SHOW_PICTURE, this }});
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
	char *cmd = g_strdup_printf("%s %s", Preferences::get_image_viewer(), quoted_file);
	free(quoted_file);
	if (!g_spawn_command_line_async(cmd, &err)) {
		Dialog::error(tr("Could not launch %1 to open file.").arg(Preferences::get_image_viewer()), data->layer->get_window());
		g_error_free(err);
	}
	free(cmd);
#endif /* WINDOWS */
#endif
}




LayerToolFuncStatus LayerToolTRWShowPicture::click_(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (trw->type != LayerType::TRW) {
		return LayerToolFuncStatus::IGNORE;
	}

	char * found = LayerTRWc::tool_show_picture_wp(trw->waypoints, ev->x(), ev->y(), this->viewport);
	if (found) {
#ifdef K
		trw_menu_sublayer_t data;
		memset(&data, 0, sizeof (trw_menu_sublayer_t));
		data.layer = trw;
		data.misc = found;
		trw->show_picture_cb(&data);
#endif
		return LayerToolFuncStatus::ACK; /* Found a match. */
	} else {
		return LayerToolFuncStatus::IGNORE; /* Go through other layers, searching for a match. */
	}
}
