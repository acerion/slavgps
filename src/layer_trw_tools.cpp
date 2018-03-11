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
#include "layer_trw_track_internal.h"
#include "tree_view_internal.h"
#include "viewport_internal.h"
#include "dialog.h"
#include "dem_cache.h"
#include "util.h"
#include "preferences.h"
#include "routing.h"




/* This is how it knows when you click if you are clicking close to a trackpoint. */
#define TRACKPOINT_SIZE_APPROX 5
#define WAYPOINT_SIZE_APPROX 5




using namespace SlavGPS;




#define PREFIX " Layer TRW Tools:" << __FUNCTION__ << __LINE__ << ">"




extern LayerTool * trw_layer_tools[];

static ToolStatus tool_new_track_handle_key_press(LayerTool * tool, LayerTRW * trw, QKeyEvent * ev);
static ToolStatus tool_new_track_move(LayerTool * tool, LayerTRW * trw, QMouseEvent * ev);
static ToolStatus tool_new_track_release(LayerTool * tool, LayerTRW * trw, QMouseEvent * ev);

static ToolStatus extend_track_with_mouse_click(LayerTRW * trw, Track * track, QMouseEvent * ev, Viewport * viewport);




/*
  ATM: Leave this as 'Track' only.
  Not overly bothered about having a snap to route trackpoint capability.
*/
Trackpoint * LayerTRW::search_nearby_tp(Viewport * viewport, int x, int y)
{
	TrackpointSearch search(x, y, viewport);

	this->tracks->track_search_closest_tp(&search);

	return search.closest_tp;
}




Waypoint * LayerTRW::search_nearby_wp(Viewport * viewport, int x, int y)
{
	WaypointSearch search(x, y, viewport, this->painter->draw_wp_images);

	this->waypoints->search_closest_wp(&search);

	return search.closest_wp;
}




/*
  This method is for handling "move" event coming from generic Select tool.
  The generic Select tool doesn't know how to implement layer-specific movement, so the layer has to implement the behaviour itself.
*/
bool LayerTRW::handle_select_tool_move(QMouseEvent * ev, Viewport * viewport, LayerTool * tool)
{
	if (!tool->layer_edit_info->holding) {
		return false;
	}

	Coord new_coord = viewport->screen_pos_to_coord(ev->x(), ev->y());

	/* Here always allow snapping back to the original location.
	   This is useful when one decides not to move the thing after all.
	   If one wants to move the item only a little bit then don't hold down the 'snap' key! */

	/* See if the coordinates of the new "move" position should be snapped to existing nearby Trackpoint or Waypoint. */
	this->get_nearby_snap_coordinates(new_coord, ev, viewport);

	/* The selected item is being moved to new position. */
	tool->perform_move(viewport->coord_to_screen_pos(new_coord));

	return true;
}




/*
  This method is for handling "release" event coming from generic Select tool.
  The generic Select tool doesn't know how to implement layer-specific release, so the layer has to implement the behaviour itself.
*/
bool LayerTRW::handle_select_tool_release(QMouseEvent * ev, Viewport * viewport, LayerTool * tool)
{
	if (!tool->layer_edit_info->holding) {
		/* The tool wasn't holding any item, so there is nothing to do on mouse release. */
		return false;
	}

	if (ev->button() != Qt::LeftButton) {
		/* If we are still holding something, but we didn't release this with left button, then we aren't interested. */
		return false;
	}

	/* Prevent accidental (small) shifts when specific movement has not been requested
	   (as the click release has occurred within the click object detection area). */
	if (!tool->layer_edit_info->moving
	    || tool->layer_edit_info->type_id == "") {
		return false;
	}

	Coord new_coord = viewport->screen_pos_to_coord(ev->x(), ev->y());

	/* See if the coordinates of the new "release" position should be snapped to existing nearby Trackpoint or Waypoint. */
	this->get_nearby_snap_coordinates(new_coord, ev, viewport);

	tool->perform_release();

	/* Update information about new position of Waypoint/Trackpoint. */
	if (tool->layer_edit_info->type_id == "sg.trw.waypoint") {
		this->get_edited_wp()->coord = new_coord;
		this->waypoints->calculate_bounds();
		this->reset_edited_wp();

	} else if (tool->layer_edit_info->type_id == "sg.trw.track" || tool->layer_edit_info->type_id == "sg.trw.route") {

		Track * track = this->get_edited_track();
		if (track && track->selected_tp.valid) {
			/* Don't reset the selected trackpoint, thus ensuring it's still presented in Trackpoint Properties window. */
			(*track->selected_tp.iter)->coord = new_coord;

			track->calculate_bounds();

			if (this->tpwin) {
				this->tpwin_update_dialog_data();
			}
		}
	} else {
		assert(0);
	}

	this->emit_layer_changed();
	return true;
}




/*
  This method is for handling "click" event coming from generic Select tool.
  The generic Select tool doesn't know how to implement layer-specific selection, so the layer has to implement the behaviour itself.

  Returns true if a waypoint or track is found near the requested event position for this particular layer.
  The item found is automatically selected.
  This is a tool like feature but routed via the layer interface, since it's instigated by a 'global' layer tool in window.c
*/
bool LayerTRW::handle_select_tool_click(QMouseEvent * ev, Viewport * viewport, LayerTool * tool)
{
	if (ev->button() != Qt::LeftButton) {
		qDebug() << "DD: Layer TRW:" << __FUNCTION__ << __LINE__;
		return false;
	}
	if (this->type != LayerType::TRW) {
		qDebug() << "DD: Layer TRW:" << __FUNCTION__ << __LINE__;
		return false;
	}
	if (!this->tracks->visible && !this->waypoints->visible && !this->routes->visible) {
		qDebug() << "DD: Layer TRW:" << __FUNCTION__ << __LINE__;
		return false;
	}

	const LatLonBBox viewport_bbox = viewport->get_bbox();

	/* Go for waypoints first as these often will be near a track, but it's likely the wp is wanted rather then the track. */

	if (this->waypoints->visible && BBOX_INTERSECT (this->waypoints->bbox, viewport_bbox)) {
		qDebug() << "DD: Layer TRW:" << __FUNCTION__ << __LINE__;
		WaypointSearch wp_search(ev->x(), ev->y(), viewport, this->painter->draw_wp_images);
		this->waypoints->search_closest_wp(&wp_search);

		if (wp_search.closest_wp) {
			qDebug() << "DD: Layer TRW:" << __FUNCTION__ << __LINE__;
			/* We have found a Trackpoint. Select Tool needs to know to which layer it belongs to. */
			tool->layer_edit_info->edited_layer = this;
			tool->layer_edit_info->type_id = wp_search.closest_wp->type_id;

			if (ev->type() == QEvent::MouseButtonDblClick
			    /* flags() & Qt::MouseEventCreatedDoubleClick */) {
				qDebug() << "DD: Layer TRW: double" << __FUNCTION__ << __LINE__;
				this->handle_select_tool_double_click_do_waypoint_selection(ev, tool, wp_search.closest_wp);
			} else {
				qDebug() << "DD: Layer TRW: single" << __FUNCTION__ << __LINE__;
				this->handle_select_tool_click_do_waypoint_selection(ev, tool, wp_search.closest_wp);
			}

			qDebug() << "DD: Layer TRW:" << __FUNCTION__ << __LINE__;
			this->emit_layer_changed();
			return true;
		}
	}
	qDebug() << "DD: Layer TRW:" << __FUNCTION__ << __LINE__;

	/* Used for both track and route lists. */
	TrackpointSearch tp_search(ev->x(), ev->y(), viewport);

	if (this->tracks->visible && BBOX_INTERSECT (this->tracks->bbox, viewport_bbox)) {
		this->tracks->track_search_closest_tp(&tp_search);
		if (tp_search.closest_tp) {
			/* We have found a Trackpoint. Select Tool needs to know to which layer it belongs to. */
			tool->layer_edit_info->edited_layer = this;
			tool->layer_edit_info->type_id = tp_search.closest_track->type_id;

			this->handle_select_tool_click_do_track_selection(ev, tool, tp_search.closest_track, tp_search.closest_tp_iter);

			if (this->tpwin) {
				this->tpwin_update_dialog_data();
			}

			this->emit_layer_changed();
			return true;
		}
	}

	/* Try again for routes. */
	if (this->routes->visible && BBOX_INTERSECT (this->routes->bbox, viewport_bbox)) {
		this->routes->track_search_closest_tp(&tp_search);
		if (tp_search.closest_tp) {
			/* We have found a Trackpoint. Select Tool needs to know to which layer it belongs to. */
			tool->layer_edit_info->edited_layer = this;
			tool->layer_edit_info->type_id = tp_search.closest_track->type_id;

			this->handle_select_tool_click_do_track_selection(ev, tool, tp_search.closest_track, tp_search.closest_tp_iter);

			if (this->tpwin) {
				this->tpwin_update_dialog_data();
			}

			this->emit_layer_changed();
			return true;
		}
	}

	/* Mouse click didn't happen anywhere near a Trackpoint or Waypoint from this layer.
	   So unmark/deselect all "current"/"edited" elements of this layer. */
	this->reset_edited_wp();
	this->reset_edited_track();
	this->cancel_current_tp(false);

	/* Blank info. */
	this->get_window()->get_statusbar()->set_message(StatusBarField::INFO, "");

	return false;
}



/*
  This method is for handling "double click" event coming from generic Select tool.
  The generic Select tool doesn't know how to implement layer-specific response to double-click, so the layer has to implement the behaviour itself.

  Returns true if a waypoint or track is found near the requested event position for this particular layer.
  The item found is automatically selected.
  This is a tool like feature but routed via the layer interface, since it's instigated by a 'global' layer tool in window.c
*/
bool LayerTRW::handle_select_tool_double_click(QMouseEvent * ev, Viewport * viewport, LayerTool * tool)
{
	/* Double-click will be handled by checking event->flags() in the function below, and calling proper handling method. */
	qDebug() << "DD: Layer TRW:" << __FUNCTION__;
	return this->handle_select_tool_click(ev, viewport, tool);
}




/**
   Given @param track and @param tp_iter have been selected by clicking with mouse cursor.
   Propagate the information about selection to all important places.
*/
void LayerTRW::handle_select_tool_click_do_track_selection(QMouseEvent * ev, LayerTool * tool, Track * track, TrackPoints::iterator & tp_iter)
{
	/* Always select + highlight the track. */
	this->tree_view->select_and_expose(track->index);
	const Track * current_selected_track = this->get_edited_track();

	/* Select the Trackpoint.
	   Can move it immediately when control held or it's the previously selected tp. */
	if (ev->modifiers() & Qt::ControlModifier
	    || (current_selected_track && current_selected_track->selected_tp.iter == tp_iter)) {

		/* Remember position at which selection occurred. */
		tool->perform_selection(ScreenPos(ev->x(), ev->y()));
	}

	this->set_edited_track(track, tp_iter);

	this->set_statusbar_msg_info_tp(tp_iter, track);
}



/**
   Given @param wp has been selected by clicking with mouse cursor.
   Propagate the information about selection to all important places.
*/
void LayerTRW::handle_select_tool_click_do_waypoint_selection(QMouseEvent * ev, LayerTool * tool, Waypoint * wp)
{
	/* Select. */
	this->tree_view->select_and_expose(wp->index);

	/* Too easy to move it so must be holding shift to start immediately moving it
	   or otherwise be previously selected but not have an image (otherwise clicking within image bounds (again) moves it). */
	if (ev->modifiers() & Qt::ShiftModifier
	    || (this->get_edited_wp() == wp && this->get_edited_wp()->image_full_path.isEmpty())) {

		/* Remember position at which selection occurred. */
		tool->perform_selection(ScreenPos(ev->x(), ev->y()));
	}

	this->set_edited_wp(wp);
}



/**
   Given @param wp has been selected by clicking with mouse cursor.
   Propagate the information about selection to all important places.
*/
void LayerTRW::handle_select_tool_double_click_do_waypoint_selection(QMouseEvent * ev, LayerTool * tool, Waypoint * wp)
{
	this->handle_select_tool_click_do_waypoint_selection(ev, tool, wp);

	qDebug() << "DD: Layer TRW: handle double wp:" << (long) wp << __FUNCTION__ << __LINE__;

	if (wp) {
		qDebug() << "DD: Layer TRW: handle double wp:" << (long) wp << wp->image_full_path.isEmpty() << __FUNCTION__ << __LINE__;
	}


	if (wp && !wp->image_full_path.isEmpty()) {
		qDebug() << "DD: Layer TRW: call call back" << __FUNCTION__ << __LINE__;
		this->show_wp_picture_cb();
	}
}




/**
   Show context menu for the currently selected item.

   This function is called when generic Select tool is selected from tool bar.
   It would be nice to somehow merge this function with code used when "edit track/route/waypoint" tool is selected.
*/
bool LayerTRW::handle_select_tool_context_menu(QMouseEvent * ev, Viewport * viewport)
{
	if (ev->button() != Qt::RightButton) {
		return false;
	}

	if (this->type != LayerType::TRW) {
		return false;
	}

	if (!this->tracks->visible && !this->waypoints->visible && !this->routes->visible) {
		return false;
	}

	Track * track = this->get_edited_track(); /* Track or route that is currently being selected/edited. */
	if (track && track->visible) { /* Track or Route. */
		if (!track->name.isEmpty()) {

			QMenu menu(viewport);

			track->add_context_menu_items(menu, false);
			menu.exec(QCursor::pos());
			return true;
		}
	}

	Waypoint * wp = this->get_edited_wp(); /* Waypoint that is currently being selected/edited. */
	if (wp && wp->visible) {
		if (!wp->name.isEmpty()) {
			QMenu menu(viewport);

			wp->add_context_menu_items(menu, false);
			menu.exec(QCursor::pos());
			return true;
		}
	}

	/* No Track/Route/Waypoint selected. */
	return false;
}




LayerToolTRWEditWaypoint::LayerToolTRWEditWaypoint(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::TRW)
{
	this->id_string = LAYER_TRW_TOOL_EDIT_WAYPOINT;

	this->action_icon_path   = ":/icons/layer_tool/trw_edit_wp_18.png";
	this->action_label       = QObject::tr("&Edit Waypoint");
	this->action_tooltip     = QObject::tr("Edit Waypoint");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_E;

	this->cursor_click = new QCursor(QPixmap(":/cursors/trw_edit_wp.png"), 0, 0);
	this->cursor_release = new QCursor(Qt::ArrowCursor);

	this->layer_edit_info = new LayerEditInfo;
}




ToolStatus LayerToolTRWEditWaypoint::handle_mouse_click(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (trw->type != LayerType::TRW) {
		return ToolStatus::IGNORED;
	}
	if (this->layer_edit_info->holding) {
		return ToolStatus::ACK;
	}
	if (!trw->visible || !trw->waypoints->visible) {
		return ToolStatus::IGNORED;
	}

	/* Does this tool have a waypoint, on which it can operate? */
	Waypoint * our_waypoint = NULL;

	Waypoint * current_wp = trw->get_edited_wp();

	if (current_wp && current_wp->visible) {
		/* Some waypoint has already been activated before the
		   click happened, e.g. by selecting it in items tree.

		   First check if that waypoint is close enough to
		   click coordinates to keep that waypoint as selected.

		   Other waypoints (not selected) may be even closer
		   to click coordinates, but the pre-selected waypoint
		   has priority. */

		const ScreenPos wp_pos = this->viewport->coord_to_screen_pos(current_wp->coord);
		const ScreenPos event_pos = ScreenPos(ev->x(), ev->y());

		if (ScreenPos::is_close_enough(wp_pos, event_pos, WAYPOINT_SIZE_APPROX)) {

			/* A waypoint has been selected in some way
			   (e.g. by selecting it in items tree), and
			   now it is also selected by this tool. */
			this->perform_selection(event_pos);

			/* Global "edited waypoint" now became tool's edited waypoint. */
			our_waypoint = current_wp;
		}
	}

	if (!our_waypoint) {
		/* Either there is no globally selected waypoint, or it
		   was too far from click. Either way the tool doesn't
		   have any waypoint to operate on - yet. Try to find
		   one close to click coordinates. */

		WaypointSearch wp_search(ev->x(), ev->y(), viewport, trw->painter->draw_wp_images);
		trw->get_waypoints_node().search_closest_wp(&wp_search);

		if (wp_search.closest_wp) {

			/* We have right-clicked a waypoint. Make it
			   globally-selected waypoint, and waypoint
			   selected by this tool.

			   TODO: do we need to verify that
			   wp_search.closest_wp != current_wp? */

			trw->tree_view->select_and_expose(wp_search.closest_wp->index);
			trw->set_edited_wp(wp_search.closest_wp);

			/* Could make it so don't update if old WP is off screen and new is null but oh well. */
			trw->emit_layer_changed();

			this->perform_selection(ScreenPos(ev->x(), ev->y()));

		        our_waypoint = wp_search.closest_wp;
		}
	}

	if (!our_waypoint) {
		/* No luck. No waypoint to operate on.

		   We have clicked on some empty space, so make sure
		   that no waypoint in this layer is globally
		   selected, no waypoint is selected by this tool, and
		   no waypoint is drawn as selected. */

		trw->reset_edited_wp();

		this->perform_release();

		/* TODO: do we need to emit this signal every time a right-click fails? */
		trw->emit_layer_changed();

		return ToolStatus::IGNORED;
	}

	/* Finally, a waypoint that this tool can operate on. Not too much operation, though. */
	switch (ev->button()) {
	case Qt::RightButton: {
		QMenu menu;
		our_waypoint->add_context_menu_items(menu, false);
		menu.exec(QCursor::pos());
		}
		break;
	case Qt::LeftButton:
		/* All that was supposed to happen on left-mouse click
		   has already happened when a waypoint was selected
		   either globally, or for this tool. */
		break;
	default:
		/* Ignore any other button. */
		break;
	}

	return ToolStatus::ACK;
}




ToolStatus LayerToolTRWEditWaypoint::handle_mouse_move(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (trw->type != LayerType::TRW) {
		return ToolStatus::IGNORED;
	}

	if (!this->layer_edit_info->holding) {
		return ToolStatus::IGNORED;
	}

	Coord new_coord = this->viewport->screen_pos_to_coord(ev->x(), ev->y());

	/* See if the coordinates of the new "move" position should be snapped to existing nearby Trackpoint or Waypoint. */
	trw->get_nearby_snap_coordinates(new_coord, ev, viewport);

	/* Selected item is being moved to new position. */
	this->perform_move(this->viewport->coord_to_screen_pos(new_coord));

	return ToolStatus::ACK;
}




ToolStatus LayerToolTRWEditWaypoint::handle_mouse_release(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (trw->type != LayerType::TRW) {
		return ToolStatus::IGNORED;
	}

	if (!this->layer_edit_info->holding) {
		/* ::handle_mouse_press() probably never happened. */
		return ToolStatus::IGNORED;
	}


	switch (ev->button()) {
	case Qt::LeftButton: {
		Coord new_coord = this->viewport->screen_pos_to_coord(ev->x(), ev->y());

		/* See if the coordinates of the release position should be snapped to existing nearby Trackpoint or Waypoint. */
		trw->get_nearby_snap_coordinates(new_coord, ev, this->viewport);

		this->perform_release();

		trw->get_edited_wp()->coord = new_coord;

		trw->get_waypoints_node().calculate_bounds();
		trw->emit_layer_changed();
		return ToolStatus::ACK;
		}

	case Qt::RightButton:
	default:
		this->perform_release();
		return ToolStatus::IGNORED;
	}
}




/*
  See if mouse event @param ev happened close to a Trackpoint or a
  Waypoint and if keyboard modifier specific for Trackpoint or
  Waypoint was used. If both conditions are true, put coordinates of
  such Trackpoint or Waypoint in @param point_coord.
*/
bool LayerTRW::get_nearby_snap_coordinates(Coord & point_coord, QMouseEvent * ev, Viewport * viewport)
{
	bool snapped = false;

	/* Snap to Trackpoint. */
	if (this->get_nearby_snap_coordinates_tp(point_coord, ev, viewport)) {
		snapped = true;
	}

	/* Snap to waypoint. */
	if (ev->modifiers() & Qt::ShiftModifier) {
		const Waypoint * wp = this->search_nearby_wp(viewport, ev->x(), ev->y());
		if (wp && wp != this->get_edited_wp()) {
			point_coord = wp->coord;
			snapped = true;
		}
	}

	return snapped;
}




/*
  See if mouse event @param ev happened close to a Trackpoint and if
  keyboard modifier specific for a Trackpoint was used. If both
  conditions are true, put coordinates of such Trackpoint in @param
  point_coord.
*/
bool LayerTRW::get_nearby_snap_coordinates_tp(Coord & point_coord, QMouseEvent * ev, Viewport * viewport)
{
	bool snapped = false;
	if (ev->modifiers() & Qt::ControlModifier) {
		const Trackpoint * tp = this->search_nearby_tp(viewport, ev->x(), ev->y());
		if (tp && tp != *this->get_edited_track()->selected_tp.iter) {
			point_coord = tp->coord;
			snapped = true;
		}
	}

	return snapped;
}




LayerToolTRWNewTrack::LayerToolTRWNewTrack(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::TRW)
{
	this->id_string = LAYER_TRW_TOOL_CREATE_TRACK;

	this->action_icon_path   = ":/icons/layer_tool/trw_add_tr_18.png";
	this->action_label       = QObject::tr("Create &Track");
	this->action_tooltip     = QObject::tr("Create Track");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_T;

	this->pan_handler = true;  /* Still need to handle clicks when in PAN mode to disable the potential trackpoint drawing. */
	this->cursor_click = new QCursor(QPixmap(":/cursors/trw_add_tr.png"), 0, 0);
	this->cursor_release = new QCursor(Qt::ArrowCursor);

	this->layer_edit_info = new LayerEditInfo;
}




ToolStatus LayerToolTRWNewTrack::handle_key_press(Layer * layer, QKeyEvent * ev)
{
	return tool_new_track_handle_key_press(this, (LayerTRW *) layer, ev);
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
		qDebug() << "SIGNAL:" PREFIX << "will emit 'layer_changed()' signal for" << trw->get_name();
		emit trw->layer_changed(trw->get_name());
#ifdef K_OLD_IMPLEMENTATION
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
	this->get_edited_track()->get_total_elevation_gain(&elev_gain, &elev_loss);

	/* Find out actual distance of current track. */
	double distance = this->get_edited_track()->get_length();

	statusbar_write(distance, elev_gain, elev_loss, 0, 0, this);
}




ToolStatus LayerToolTRWNewTrack::handle_mouse_move(Layer * layer, QMouseEvent * ev)
{
	return tool_new_track_move(this, (LayerTRW *) layer, ev);
}




static ToolStatus tool_new_track_move(LayerTool * tool, LayerTRW * trw, QMouseEvent * ev)
{
	Track * track = trw->get_edited_track();
	qDebug() << "II: Layer TRW: new track's move(): draw_sync_done" << (int) trw->draw_sync_done << ", current track:" << (long) track;

	/* If we haven't sync'ed yet, we don't have time to do more. */
	if (/* trw->draw_sync_done && */ track && !track->empty()) {
		Trackpoint * last_tpt = track->get_tp_last();

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
#ifdef K_OLD_IMPLEMENTATION
		gdk_draw_drawable(pixmap,
				  trw->painter->current_track_new_point_pen,
				  tool->viewport->get_pixmap(),
				  0, 0, 0, 0, -1, -1);
#endif

		int x1, y1;

		tool->viewport->coord_to_screen_pos(last_tpt->coord, &x1, &y1);

		/* FOR SCREEN OVERLAYS WE MUST DRAW INTO THIS PIXMAP (when using the reset method)
		   otherwise using Viewport::draw_* functions puts the data into the base pixmap,
		   thus when we come to reset to the background it would include what we have already drawn!! */
		QPainter painter(pixmap);
		painter.setPen(trw->painter->current_track_new_point_pen);
		qDebug() << "II: Layer TRW: drawing line" << x1 << y1 << ev->x() << ev->y();
		painter.drawLine(x1, y1, ev->x(), ev->y());

		/* Using this reset method is more reliable than trying to undraw previous efforts via the GDK_INVERT method. */

		/* Find out actual distance of current track. */
		double distance = track->get_length();

		/* Now add distance to where the pointer is. */
		const Coord screen_coord = tool->viewport->screen_pos_to_coord(ev->x(), ev->y());
		LatLon ll = screen_coord.get_latlon(); /* kamilFIXME: unused variable. */
		double last_step = Coord::distance(screen_coord, last_tpt->coord);
		distance = distance + last_step;

		/* Get elevation data. */
		double elev_gain, elev_loss;
		track->get_total_elevation_gain(&elev_gain, &elev_loss);

		/* Adjust elevation data (if available) for the current pointer position. */
		double elev_new = (double) DEMCache::get_elev_by_coord(&screen_coord, DemInterpolation::BEST);
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
			const QString distance_label = distance_string(distance);
#ifdef K_TODO
			PangoLayout *pl = gtk_widget_create_pango_layout(tool->viewport), NULL);
			pango_layout_set_font_description(pl, gtk_widget_get_style(tool->viewport)->font_desc);
			pango_layout_set_text(pl, distance_label, -1);
			int wd, hd;
			pango_layout_get_pixel_size(pl, &wd, &hd);

			int xd,yd;
			/* Offset from cursor a bit depending on font size. */
			xd = ev->x + 10;
			yd = ev->y - hd;

			/* Create a background block to make the text easier to read over the background map. */
			QPen background_block_pen = SGUtils::new_pen(QColor("#cccccc"), 1);
			fill_rectangle(pixmap, background_block_pen, xd-2, yd-2, wd+4, hd+2);
			gdk_draw_layout(pixmap, trw->painter->current_track_new_point_pen, xd, yd, pl);

			g_object_unref(G_OBJECT (pl));
#endif
		}

		double angle;
		double baseangle;
		tool->viewport->compute_bearing(x1, y1, ev->x(), ev->y(), &angle, &baseangle);

		/* Update statusbar with full gain/loss information. */
		statusbar_write(distance, elev_gain, elev_loss, last_step, angle, trw);

		//passalong->pen = new QPen(trw->painter->current_track_new_point_pen);
		draw_sync(trw, tool->viewport->scr_buffer, pixmap);
		trw->draw_sync_done = false;

		return ToolStatus::ACK_GRAB_FOCUS;
	}
	return ToolStatus::ACK;
}




static ToolStatus tool_new_track_handle_key_press(LayerTool * tool, LayerTRW * trw, QKeyEvent * ev)
{
	if (tool->id_string == LAYER_TRW_TOOL_CREATE_TRACK && !((LayerToolTRWNewTrack *) tool)->creation_in_progress) {
		/* Track is not being created at the moment, so a Key can't affect work of this tool. */
		return ToolStatus::IGNORED;
	}
	if (tool->id_string == LAYER_TRW_TOOL_CREATE_ROUTE && !((LayerToolTRWNewRoute *) tool)->creation_in_progress) {
		/* Route is not being created at the moment, so a Key can't affect work of this tool. */
		return ToolStatus::IGNORED;
	}

	Track * track = trw->get_edited_track();

#if 1
	/* Check of consistency between LayerTRW and the tool. */
	if (!track) {
		qDebug() << "EE: Layer TRW Tools: new track handle key press: creation-in-progress=true, but no track selected in layer";
		return ToolStatus::IGNORED;
	}
#endif

	switch (ev->key()) {
	case Qt::Key_Escape:
		if (track->type_id == "sg.trw.route") {
			((LayerToolTRWNewRoute *) tool)->creation_in_progress = NULL;
			if (track->get_tp_count() == 1) {
				/* Bin track if only one point as it's not very useful. */
				trw->delete_route(track);
			}
		} else {
			((LayerToolTRWNewTrack *) tool)->creation_in_progress = NULL;
			if (track->get_tp_count() == 1) {
				/* Bin track if only one point as it's not very useful. */
				trw->delete_track(track);
			}
		}

		trw->reset_edited_track();
		trw->emit_layer_changed();
		return ToolStatus::ACK;

	case Qt::Key_Backspace:
		track->remove_last_trackpoint();
		trw->update_statusbar();
		trw->emit_layer_changed();
		return ToolStatus::ACK;

	default:
		return ToolStatus::IGNORED;
	}
}




/**
   \brief Edit given track/route in response to given mouse click

   Function can add a point to track if a normal mouse click occurred.
   Function can remove last point from track if right-click occurred.

   Call this function only for single clicks.

   \param trw - parent layer of track
   \param track - track to modify
   \param ev - mouse event
   \param viewport - viewport, in which a click occurred
*/
ToolStatus extend_track_with_mouse_click(LayerTRW * trw, Track * track, QMouseEvent * ev, Viewport * viewport)
{
	if (!track) {
		qDebug() << "EE: LayerTRW: Extend Track: NULL track argument";
		return ToolStatus::IGNORED;
	}

	if (ev->button() == Qt::MiddleButton) {
		/* As the display is panning, the new track pixmap is now invalid so don't draw it
		   otherwise this drawing done results in flickering back to an old image. */
		trw->draw_sync_do = false;
		return ToolStatus::IGNORED;
	}

	if (ev->button() == Qt::RightButton) {
		track->remove_last_trackpoint();
		trw->update_statusbar();
		trw->emit_layer_changed();
		return ToolStatus::ACK;
	}

	Trackpoint * tp = new Trackpoint();
	tp->coord = viewport->screen_pos_to_coord(ev->x(), ev->y());

	/* Snap to other Trackpoint. */
	trw->get_nearby_snap_coordinates_tp(tp->coord, ev, viewport);

	tp->newsegment = false;
	tp->has_timestamp = false;
	tp->timestamp = 0;

	track->add_trackpoint(tp, true); /* Ensure bounds is updated. */
	/* Auto attempt to get elevation from DEM data (if it's available). */
	track->apply_dem_data_last_trackpoint();


	/* TODO: I think that in current implementation of handling of double click we don't need these fields. */
	trw->ct_x1 = trw->ct_x2;
	trw->ct_y1 = trw->ct_y2;
	trw->ct_x2 = ev->x();
	trw->ct_y2 = ev->y();

	trw->emit_layer_changed();

	return ToolStatus::ACK;
}




ToolStatus LayerToolTRWNewTrack::handle_mouse_click(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	/* If we were running the route finder, cancel it. */
	trw->route_finder_started = false;

	if (ev->button() != Qt::LeftButton) {
		/* TODO: this shouldn't even happen. */
		/* TODO: this function calls extend_track_with_mouse_click() which handles right mouse button. Why we don't allow it here? */
		return ToolStatus::IGNORED;
	}

	Track * track = NULL;
	if (this->creation_in_progress) {
#if 1
		/* Check of consistency between LayerTRW and the tool. */
		if (!trw->get_edited_track()) {
			qDebug() << "EE: Layer TRW Tools: New Track: handle mouse click: mismatch 1";
		}
#endif

		track = trw->get_edited_track();
	} else {
#if 1
		/* Check of consistency between LayerTRW and the tool. */
		if (trw->get_edited_track()) {
			qDebug() << "EE: Layer TRW Tools: New Track: handle mouse click: mismatch 1";
		}
#endif

		/* FIXME: how to handle a situation, when a route is being created right now? */
		QString new_name = trw->new_unique_element_name("sg.trw.track", QObject::tr("Track"));
		if (Preferences::get_ask_for_create_track_name()) {
			new_name = a_dialog_new_track(new_name, false, trw->get_window());
			if (new_name.isEmpty()) {
				return ToolStatus::IGNORED;
			}
		}
		track = trw->new_track_create_common(new_name);
		this->creation_in_progress = trw;
	}

	return extend_track_with_mouse_click(trw, track, ev, this->viewport);
}




ToolStatus LayerToolTRWNewTrack::handle_mouse_double_click(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;
	if (trw->type != LayerType::TRW) {
		return ToolStatus::IGNORED;
	}

	if (ev->button() != Qt::LeftButton) {
		return ToolStatus::IGNORED;
	}

	/* End the process of creating a track. */
	if (this->creation_in_progress) {
#if 1
		/* Check of consistency between LayerTRW and the tool. */
		if (!trw->get_edited_track()) {
			qDebug() << "Layer TRW Tools: Handle Double Mouse Click: inconsistency 1";
		}
#endif
		if (!trw->get_edited_track()->empty() /* && trw->ct_x1 == trw->ct_x2 && trw->ct_y1 == trw->ct_y2 */) {
			trw->reset_edited_track();
			this->creation_in_progress = NULL;
		}
	} else {
#if 1
		/* Check of consistency between LayerTRW and the tool. */
		if (trw->get_edited_track()) {
			qDebug() << "Layer TRW Tools: Handle Double Mouse Click: inconsistency 2";
		}
#endif
	}
	trw->emit_layer_changed();
	return ToolStatus::ACK;
}




ToolStatus LayerToolTRWNewTrack::handle_mouse_release(Layer * layer, QMouseEvent * ev)
{
	return tool_new_track_release(this, (LayerTRW *) layer, ev);
}




static ToolStatus tool_new_track_release(LayerTool * tool, LayerTRW * trw, QMouseEvent * ev)
{
	if (ev->button() == Qt::MiddleButton) {
		/* Pan moving ended - enable potential point drawing again. */
		trw->draw_sync_do = true;
		trw->draw_sync_done = true;
	}

	return ToolStatus::ACK;
}




LayerToolTRWNewRoute::LayerToolTRWNewRoute(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::TRW)
{
	this->id_string = LAYER_TRW_TOOL_CREATE_ROUTE;

	this->action_icon_path   = ":/icons/layer_tool/trw_add_route_18.png";
	this->action_label       = QObject::tr("Create &Route");
	this->action_tooltip     = QObject::tr("Create Route");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_B;

	this->pan_handler = true;  /* Still need to handle clicks when in PAN mode to disable the potential trackpoint drawing. */
	this->cursor_click = new QCursor(QPixmap(":/cursors/trw_add_route.png"), 0, 0);
	this->cursor_release = new QCursor(Qt::ArrowCursor);

	this->layer_edit_info = new LayerEditInfo;
}




ToolStatus LayerToolTRWNewRoute::handle_mouse_click(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	/* If we were running the route finder, cancel it. */
	trw->route_finder_started = false;

	if (ev->button() != Qt::LeftButton) {
		/* TODO: this shouldn't even happen. */
		/* TODO: this function calls extend_track_with_mouse_click() which handles right mouse button. Why we don't allow it here? */
		return ToolStatus::IGNORED;
	}

	Track * track = NULL;
	if (this->creation_in_progress) {
#if 1
		/* Check of consistency between LayerTRW and the tool. */
		if (!trw->get_edited_track()) {
			qDebug() << "EE: Layer TRW Tools: New Route: handle mouse click: mismatch 1";
		}
#endif

		track = trw->get_edited_track();
	} else {
#if 1
		/* Check of consistency between LayerTRW and the tool. */
		if (trw->get_edited_track()) {
			qDebug() << "EE: Layer TRW Tools: New Route: handle mouse click: mismatch 1";
		}
#endif

		/* FIXME: how to handle a situation, when a track is being created right now? */
		QString new_name = trw->new_unique_element_name("sg.trw.route", QObject::tr("Route"));
		if (Preferences::get_ask_for_create_track_name()) {
			new_name = a_dialog_new_track(new_name, true, trw->get_window());
			if (new_name.isEmpty()) {
				return ToolStatus::IGNORED;
			}
		}
		track = trw->new_route_create_common(new_name);
		this->creation_in_progress = trw;
	}

	return extend_track_with_mouse_click(trw, track, ev, this->viewport);
}




ToolStatus LayerToolTRWNewRoute::handle_mouse_double_click(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;
	if (trw->type != LayerType::TRW) {
		return ToolStatus::IGNORED;
	}

	if (ev->button() != Qt::LeftButton) {
		return ToolStatus::IGNORED;
	}

	/* End the process of creating a track. */
	if (this->creation_in_progress) {
#if 1
		/* Check of consistency between LayerTRW and the tool. */
		if (!trw->get_edited_track()) {
			qDebug() << "Layer TRW Tools: New Route: Handle Double Mouse Click: inconsistency 1";
		}
#endif
		if (!trw->get_edited_track()->empty() /* && trw->ct_x1 == trw->ct_x2 && trw->ct_y1 == trw->ct_y2 */) {
			trw->reset_edited_track();
			this->creation_in_progress = NULL;
		}
	} else {
#if 1
		/* Check of consistency between LayerTRW and the tool. */
		if (trw->get_edited_track()) {
			qDebug() << "Layer TRW Tools: New Route: Handle Double Mouse Click: inconsistency 2";
		}
#endif
	}
	trw->emit_layer_changed();
	return ToolStatus::ACK;
}





ToolStatus LayerToolTRWNewRoute::handle_mouse_move(Layer * layer, QMouseEvent * ev)
{
	return tool_new_track_move(this, (LayerTRW *) layer, ev);
}




ToolStatus LayerToolTRWNewRoute::handle_mouse_release(Layer * layer, QMouseEvent * ev)
{
	return tool_new_track_release(this, (LayerTRW *) layer, ev);
}




ToolStatus LayerToolTRWNewRoute::handle_key_press(Layer * layer, QKeyEvent * ev)
{
	return tool_new_track_handle_key_press(this, (LayerTRW *) layer, ev);
}




LayerToolTRWNewWaypoint::LayerToolTRWNewWaypoint(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::TRW)
{
	this->id_string = LAYER_TRW_TOOL_CREATE_WAYPOINT;

	this->action_icon_path   = ":/icons/layer_tool/trw_add_wp_18.png";
	this->action_label       = QObject::tr("Create &Waypoint");
	this->action_tooltip     = QObject::tr("Create Waypoint");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_W;

	this->cursor_click = new QCursor(QPixmap(":/cursors/trw_add_wp.png"), 0, 0);
	this->cursor_release = new QCursor(Qt::ArrowCursor);

	this->layer_edit_info = new LayerEditInfo;
}




ToolStatus LayerToolTRWNewWaypoint::handle_mouse_click(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (trw->type != LayerType::TRW) {
		return ToolStatus::IGNORED;
	}

	const Coord coord = this->viewport->screen_pos_to_coord(ev->x(), ev->y());
	if (trw->new_waypoint(coord, trw->get_window())) {
		trw->get_waypoints_node().calculate_bounds();
		if (trw->visible) {
			qDebug() << "II: Layer TRW: created new waypoint, will emit update";
			trw->emit_layer_changed();
		}
	}
	return ToolStatus::ACK;
}




LayerToolTRWEditTrackpoint::LayerToolTRWEditTrackpoint(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::TRW)
{
	this->id_string = LAYER_TRW_TOOL_EDIT_TRACKPOINT;

	this->action_icon_path   = ":/icons/layer_tool/trw_edit_tr_18.png";
	this->action_label       = QObject::tr("Edit Trac&kpoint");
	this->action_tooltip     = QObject::tr("Edit Trackpoint");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_K;

	this->cursor_click = new QCursor(QPixmap(":/cursors/trw_edit_tr.png"), 0, 0);
	this->cursor_release = new QCursor(Qt::ArrowCursor);

	this->layer_edit_info = new LayerEditInfo;
}




/**
 * On 'initial' click: search for the nearest trackpoint or routepoint and store it as the current trackpoint.
 * Then update the viewport, statusbar and edit dialog to draw the point as being selected and it's information.
 * On subsequent clicks: (as the current trackpoint is defined) and the click is very near the same point
 * then initiate the move operation to drag the point to a new destination.
 * NB The current trackpoint will get reset elsewhere.
 */
ToolStatus LayerToolTRWEditTrackpoint::handle_mouse_click(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	TrackpointSearch tp_search(ev->x(), ev->y(), this->viewport);

	if (ev->button() != Qt::LeftButton) {
		return ToolStatus::IGNORED;
	}

	if (trw->type != LayerType::TRW) {
		return ToolStatus::IGNORED;
	}

	if (!trw->visible && !(trw->tracks->visible && trw->routes->visible)) {
		return ToolStatus::IGNORED;
	}

	Track * track = trw->get_edited_track();

	if (track && track->selected_tp.valid) {
		/* First check if it is within range of prev. tp. and if current_tp track is shown. (if it is, we are moving that trackpoint). */

		if (!track) { /* TODO: there is a mismatch between this condition and upper if() condition. */
			return ToolStatus::IGNORED;
		}

		Trackpoint * tp = *track->selected_tp.iter;
		const ScreenPos tp_pos = this->viewport->coord_to_screen_pos(tp->coord);
		const ScreenPos event_pos = ScreenPos(ev->x(), ev->y());

		if (track->visible && ScreenPos::is_close_enough(tp_pos, event_pos, TRACKPOINT_SIZE_APPROX)) {
			this->perform_selection(event_pos);
			return ToolStatus::ACK;
		}

	}

	if (trw->get_tracks_node().visible) {
		trw->get_tracks_node().track_search_closest_tp(&tp_search);

		if (tp_search.closest_tp) {
			trw->tree_view->select_and_expose(tp_search.closest_track->index);
			trw->set_edited_track(tp_search.closest_track, tp_search.closest_tp_iter);

			trw->trackpoint_properties_show();
			trw->set_statusbar_msg_info_tp(tp_search.closest_tp_iter, tp_search.closest_track);
			trw->emit_layer_changed();
			return ToolStatus::ACK;
		}
	}

	if (trw->get_routes_node().visible) {
		trw->get_routes_node().track_search_closest_tp(&tp_search);

		if (tp_search.closest_tp) {
			trw->tree_view->select_and_expose(tp_search.closest_track->index);
			trw->set_edited_track(tp_search.closest_track, tp_search.closest_tp_iter);

			trw->trackpoint_properties_show();
			trw->set_statusbar_msg_info_tp(tp_search.closest_tp_iter, tp_search.closest_track);
			trw->emit_layer_changed();
			return ToolStatus::ACK;
		}
	}

	/* The mouse click wasn't near enough any Trackpoint that belongs to any tracks/routes in this layer. */
	return ToolStatus::IGNORED;
}




ToolStatus LayerToolTRWEditTrackpoint::handle_mouse_move(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (trw->type != LayerType::TRW) {
		return ToolStatus::IGNORED;
	}

	if (!this->layer_edit_info->holding) {
		return ToolStatus::IGNORED;
	}

	Coord new_coord = this->viewport->screen_pos_to_coord(ev->x(), ev->y());

	/* Snap to Trackpoint */
	trw->get_nearby_snap_coordinates_tp(new_coord, ev, this->viewport);

	// trw->selected_tp.tp->coord = new_coord;

	/* Selected item is being moved to new position. */
	this->perform_move(this->viewport->coord_to_screen_pos(new_coord));

	return ToolStatus::ACK;
}




ToolStatus LayerToolTRWEditTrackpoint::handle_mouse_release(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (trw->type != LayerType::TRW) {
		return ToolStatus::IGNORED;
	}

	if (ev->button() != Qt::LeftButton) {
		return ToolStatus::IGNORED;
	}

	if (!this->layer_edit_info->holding) {
		return ToolStatus::IGNORED;
	}

	Track * track = trw->get_edited_track(); /* This is the track, to which belongs the edited trackpoint. */
	if (!track) {
		/* Well, there was no track that was edited, so nothing to do here. */
		return ToolStatus::IGNORED;
	}

	Coord new_coord = this->viewport->screen_pos_to_coord(ev->x(), ev->y());

	/* Snap to trackpoint */
	if (ev->modifiers() & Qt::ControlModifier) {
		Trackpoint * tp = trw->search_nearby_tp(this->viewport, ev->x(), ev->y());
		if (tp && tp != *track->selected_tp.iter) {
			new_coord = tp->coord;
		}
	}

	(*track->selected_tp.iter)->coord = new_coord;
	track->calculate_bounds();

	this->perform_release();

	/* Diff dist is diff from orig. */
	if (trw->tpwin) {
		trw->tpwin_update_dialog_data();
	}

	trw->emit_layer_changed();

	return ToolStatus::ACK;
}




LayerToolTRWExtendedRouteFinder::LayerToolTRWExtendedRouteFinder(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::TRW)
{
	this->id_string = LAYER_TRW_TOOL_ROUTE_FINDER;

	this->action_icon_path   = ":/icons/layer_tool/trw_find_route_18.png";
	this->action_label       = QObject::tr("Route &Finder");
	this->action_tooltip     = QObject::tr("Route Finder");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_F;

	this->pan_handler = true;  /* Still need to handle clicks when in PAN mode to disable the potential trackpoint drawing. */
	this->cursor_click = new QCursor(QPixmap(":/cursors/trw____.png"), 0, 0);
	this->cursor_release = new QCursor(Qt::ArrowCursor);

	this->layer_edit_info = new LayerEditInfo;
}




ToolStatus LayerToolTRWExtendedRouteFinder::handle_mouse_move(Layer * layer, QMouseEvent * ev)
{
	return tool_new_track_move(this, (LayerTRW *) layer, ev);
}




ToolStatus LayerToolTRWExtendedRouteFinder::handle_mouse_release(Layer * layer, QMouseEvent * ev)
{
	return tool_new_track_release(this, (LayerTRW *) layer, ev);
}



void LayerToolTRWExtendedRouteFinder::undo(LayerTRW * trw, Track * track)
{
	if (!track) {
		return;
	}

	Coord * new_end = track->cut_back_to_double_point();
	if (!new_end) {
		return;
	}
	delete new_end;

	trw->emit_layer_changed();

	/* Remove last ' to:...' */
	if (!track->comment.isEmpty()) {
#ifdef K_TODO
		char * last_to = strrchr(track->comment, 't');
		if (last_to && (last_to - track->comment > 1)) {
			track->set_comment(track->comment.left(last_to - track->comment - 1));
		}
#endif
	}
}




ToolStatus LayerToolTRWExtendedRouteFinder::handle_mouse_click(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	Track * track = trw->get_edited_track();

	Coord tmp = this->viewport->screen_pos_to_coord(ev->x(), ev->y());
	if (ev->button() == Qt::RightButton && track) {
		this->undo(trw, track);

	} else if (ev->button() == Qt::MiddleButton) {
		trw->draw_sync_do = false;
		return ToolStatus::IGNORED;
	}
	/* If we started the track but via undo deleted all the track points, begin again. */
	else if (track && track->type_id == "sg.trw.route" && !track->get_tp_first()) {
		return extend_track_with_mouse_click(trw, track, ev, this->viewport);

	} else if ((track && track->type_id == "sg.trw.route")
		   || ((ev->modifiers() & Qt::ControlModifier) && track)) {

		Trackpoint * tp_start = track->get_tp_last();
		const LatLon start = tp_start->coord.get_latlon();
		const LatLon end = tmp.get_latlon();

		trw->route_finder_started = true;
		trw->route_finder_append = true;  /* Merge tracks. Keep started true. */



		/* Update UI to let user know what's going on. */
		StatusBar * sb = trw->get_window()->get_statusbar();
		RoutingEngine * engine = routing_default_engine();
		if (!engine) {
			trw->get_window()->get_statusbar()->set_message(StatusBarField::INFO, "Cannot plan route without a default routing engine.");
			return ToolStatus::ACK;
		}
		const QString msg1 = QObject::tr("Querying %1 for route between (%2, %3) and (%4, %5).")
			.arg(engine->get_label())
			.arg(start.lat, 0, 'f', 3) /* ".3f" */
			.arg(start.lon, 0, 'f', 3)
			.arg(end.lat, 0, 'f', 3)
			.arg(end.lon, 0, 'f', 3);
		trw->get_window()->get_statusbar()->set_message(StatusBarField::INFO, msg1);
		trw->get_window()->set_busy_cursor();

#ifdef K_TODO
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


		trw->emit_layer_changed();
	} else {
		trw->reset_edited_track();

		/* Create a new route where we will add the planned route to. */
		ToolStatus ret = this->handle_mouse_click(trw, ev);

		trw->route_finder_started = true;

		return ret;
	}

	return ToolStatus::ACK;
}




ToolStatus LayerToolTRWExtendedRouteFinder::handle_key_press(Layer * layer, QKeyEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	Track * track = trw->get_edited_track();
	if (!track) {
		return ToolStatus::IGNORED;
	}

	switch (ev->key()) {
	case  Qt::Key_Escape:
		trw->route_finder_started = false;
		trw->reset_edited_track();
		trw->emit_layer_changed();
		return ToolStatus::ACK;

	case Qt::Key_Backspace:
		this->undo(trw, track);
		return ToolStatus::ACK;

	default:
		return ToolStatus::IGNORED;
	}
}




LayerToolTRWShowPicture::LayerToolTRWShowPicture(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::TRW)
{
	this->id_string = LAYER_TRW_TOOL_SHOW_PICTURE;

	this->action_icon_path   = ":/icons/layer_tool/trw_show_picture_18.png";
	this->action_label       = QObject::tr("Show P&icture");
	this->action_tooltip     = QObject::tr("Show Picture");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_I;

	this->cursor_click = new QCursor(QPixmap(":/cursors/trw____.png"), 0, 0);
	this->cursor_release = new QCursor(Qt::ArrowCursor);

	this->layer_edit_info = new LayerEditInfo;
}




ToolStatus LayerToolTRWShowPicture::handle_mouse_click(Layer * layer, QMouseEvent * ev)
{
	if (layer->type != LayerType::TRW) {
		return ToolStatus::IGNORED;
	}

	LayerTRW * trw = (LayerTRW *) layer;

	QString found_image = trw->get_waypoints_node().tool_show_picture_wp(ev->x(), ev->y(), this->viewport);
	if (!found_image.isEmpty()) {
		trw->show_wp_picture_cb();
		return ToolStatus::ACK; /* Found a match. */
	} else {
		return ToolStatus::IGNORED; /* Go through other layers, searching for a match. */
	}
}
