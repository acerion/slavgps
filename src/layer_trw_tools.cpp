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




#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <cassert>




#include <QDebug>
#include <QPainter>




#include "layer_trw.h"
#include "layer_trw_painter.h"
#include "layer_trw_tools.h"
#include "layer_trw_dialogs.h"
#include "layer_trw_track_internal.h"
#include "layer_trw_trackpoint_properties.h"
#include "layer_trw_waypoint_properties.h"
#include "tree_view_internal.h"
#include "layers_panel.h"
#include "viewport_internal.h"
#include "dialog.h"
#include "dem_cache.h"
#include "util.h"
#include "preferences.h"
#include "routing.h"
#include "statusbar.h"
#include "ruler.h"
#include "globals.h"
#include "generic_tools.h"
#include "toolbox.h"




using namespace SlavGPS;




#define SG_MODULE "Layer TRW Tools"

#define WAYPOINT_MODIFIER_KEY     Qt::ShiftModifier
#define TRACKPOINT_MODIFIER_KEY   Qt::ControlModifier

/* This is how a tool knows if a click happened in vicinity of
   trackpoint or waypoint, i.e. if it happened close enough to the
   point to be treated as clicking the point itself. */
#define TRACKPOINT_SIZE_APPROX 5
#define WAYPOINT_SIZE_APPROX 5




extern LayerTool * trw_layer_tools[];


static ToolStatus create_new_trackpoint(LayerTRW * trw, Track * track, QMouseEvent * ev, GisViewport * gisview);
static ToolStatus create_new_trackpoint_route_finder(LayerTRW * trw, Track * track, QMouseEvent * ev, GisViewport * gisview) { return ToolStatus::Ignored; } /* TODO_2_LATER: implement the function for route finder tool. */




/*
  This method is for handling "move" event coming from generic Select tool.
  The generic Select tool doesn't know how to implement layer-specific movement, so the layer has to implement the behaviour itself.
*/
bool LayerTRW::handle_select_tool_move(QMouseEvent * ev, GisViewport * gisview, LayerToolSelect * select_tool) /// kamil
{
	switch (ev->buttons()) { /* Notice that it's ::buttons(), not ::button(). */
	case Qt::LeftButton: {
		/* Recognize that an object is moving. */
		if (sg_ret::ok != select_tool->remember_object_moving()) {
			qDebug() << SG_PREFIX_E << "Not moving";
			return false;
		}

		Coord new_coord = gisview->screen_pos_to_coord(ev->x(), ev->y());
		this->get_nearby_snap_coordinates(new_coord, ev, gisview);
		this->set_selected_object_position(select_tool->selected_tree_item_type_id, new_coord, false);
		return true;
	}
	default:
		return false;
	}

	return false;
}




/*
  This method is for handling "release" event coming from generic Select tool.
  The generic Select tool doesn't know how to implement layer-specific release, so the layer has to implement the behaviour itself.
*/
bool LayerTRW::handle_select_tool_release(QMouseEvent * ev, GisViewport * gisview, LayerToolSelect * select_tool)
{
	if (ev->button() != Qt::LeftButton) {
		/* If we are still holding something, but we didn't release this with left button, then we aren't interested. */
		return false;
	}

	/* Prevent accidental (small) shifts when specific movement
	   has not been requested (as the click release has occurred
	   within the click object detection area). */
	if (select_tool->selected_tree_item_type_id == "") {
		return false;
	}

	/* Recognize that an object is being moved. */
	if (sg_ret::ok != select_tool->remember_object_moving()) {
		qDebug() << SG_PREFIX_E << "Not moving";
		return false;
	}

	Coord new_coord = gisview->screen_pos_to_coord(ev->x(), ev->y());
	this->get_nearby_snap_coordinates(new_coord, ev, gisview);
	this->set_selected_object_position(select_tool->selected_tree_item_type_id, new_coord, true);

	qDebug() << SG_PREFIX_I << "Will call 'stop holding object'";
	select_tool->stop_holding_object();

	//this->emit_tree_item_changed("TRW - handle select tool release");

	return true;
}




sg_ret LayerTRW::set_selected_waypoint_position(const Coord & new_coord, bool do_recalculate_bbox)
{
	Waypoint * wp = this->selected_wp_get();
	if (!wp) {
		qDebug() << SG_PREFIX_E << "Will reset waypoint properties dialog data, No waypoint";
		this->wp_properties_dialog_reset();
		return sg_ret::err;
	}

	wp->coord = new_coord;
	if (do_recalculate_bbox) {
		this->waypoints.recalculate_bbox();
	}

	/* Update properties dialog with the most recent coordinates
	   of released waypoint. */
	/* TODO_OPTIMIZATION: optimize by changing only coordinates in
	   the dialog. This is the only parameter that will change
	   when a point is moved in x/y plane. We may consider also
	   updating an alternative altitude indicator, if the altitude
	   is retrieved from DEM info. */
	this->wp_properties_dialog_set(wp);

	this->emit_tree_item_changed("Selected waypoint's position has changed");

	return sg_ret::ok;
}



sg_ret LayerTRW::set_selected_trackpoint_position(const Coord & new_coord, bool do_recalculate_bbox)
{
	Track * track = this->selected_track_get();
	if (NULL == track) {
		qDebug() << SG_PREFIX_E << "Will reset trackpoint properties dialog data, no track";
		this->tp_properties_dialog_reset();
		return sg_ret::err;
	}
	if (1 != track->get_selected_children().get_count()) {
		qDebug() << SG_PREFIX_E << "Will reset trackpoint properties dialog data, wrong selected tp count:" << track->get_selected_children().get_count();
		this->tp_properties_dialog_reset();
		return sg_ret::err;
	}

	track->single_selected_tp_set_coord(new_coord);

	/* Update properties dialog with the most
	   recent coordinates of released
	   trackpoint. */
	/* TODO_OPTIMIZATION: optimize by changing only coordinates in
	   the dialog. This is the only parameter that will change
	   when a point is moved in x/y plane.  We may consider also
	   updating an alternative altitude indicator, if the altitude
	   is retrieved from DEM info. */
	this->tp_properties_dialog_set(track);

	if (do_recalculate_bbox) {
		if (track->is_route()) {
			this->routes.recalculate_bbox();
		} else {
			this->tracks.recalculate_bbox();
		}
	}

	this->emit_tree_item_changed("Selected trackpoint's position has changed");

	return sg_ret::ok;
}




/* Update information about new position of Waypoint/Trackpoint. */
sg_ret LayerTRW::set_selected_object_position(const QString & object_type_id, const Coord & new_coord, bool do_recalculate_bbox)
{
	if (object_type_id == "sg.trw.waypoint") {
		return this->set_selected_waypoint_position(new_coord, do_recalculate_bbox);
	} else if (object_type_id == "sg.trw.track" || object_type_id == "sg.trw.route") {
		return this->set_selected_trackpoint_position(new_coord, do_recalculate_bbox);
	} else {
		assert(0);
	}

	return sg_ret::ok;
}




/*
  This method is for handling "click" event coming from generic Select tool.
  The generic Select tool doesn't know how to implement layer-specific selection, so the layer has to implement the behaviour itself.

  Returns true if a waypoint or track is found near the requested event position for this particular layer.
  The item found is automatically selected.
  This is a tool like feature but routed via the layer interface, since it's instigated by a 'global' layer tool in window.c
*/
bool LayerTRW::handle_select_tool_click(QMouseEvent * ev, GisViewport * gisview, LayerToolSelect * select_tool)
{
	if (ev->button() != Qt::LeftButton) {
		qDebug() << SG_PREFIX_I << "Skipping non-left button";
		return false;
	}
	if (this->type != LayerType::TRW) {
		qDebug() << SG_PREFIX_E << "Skipping non-TRW layer";
		return false;
	}
	if (!this->tracks.is_visible() && !this->waypoints.is_visible() && !this->routes.is_visible()) {
		qDebug() << SG_PREFIX_D << "Skipping - all sublayers are invisible";
		return false;
	}

	const LatLonBBox viewport_bbox = gisview->get_bbox();

	/* Go for waypoints first as these often will be near a track, but it's likely the wp is wanted rather then the track. */

	const bool waypoints_visible = this->waypoints.is_visible();
	const bool waypoints_inside = BBOX_INTERSECT (this->waypoints.get_bbox(), viewport_bbox);
	qDebug() << SG_PREFIX_I << "Waypoints are" << (waypoints_visible ? "visible" : "invisible") << "and" << (waypoints_inside ? "inside" : "outside") << "of viewport";
	if (waypoints_visible && waypoints_inside) {
		WaypointSearch wp_search(ev->x(), ev->y(), gisview);
		if (true == this->try_clicking_waypoint(wp_search)) {
			select_tool->selected_tree_item_type_id = wp_search.closest_wp->type_id;
			this->layer_tool_maybe_start_holding_wp(ev, select_tool, wp_search.closest_wp);

			if (ev->type() == QEvent::MouseButtonDblClick
			    /* flags() & Qt::MouseEventCreatedDoubleClick */) {
				qDebug() << SG_PREFIX_D << "Selected waypoint through double click";

				if (!wp_search.closest_wp->image_full_path.isEmpty()) {
					this->show_wp_picture_cb();
				}
			}

			return true;
		}
	}

	const bool tracks_visible = this->tracks.is_visible();
	const bool tracks_inside = BBOX_INTERSECT (this->tracks.get_bbox(), viewport_bbox);
	qDebug() << SG_PREFIX_I << "Tracks are" << (tracks_visible ? "visible" : "invisible") << "and" << (tracks_inside ? "inside" : "outside") << "of viewport";
	if (tracks_visible && tracks_inside) {
		TrackpointSearch tp_search(ev->x(), ev->y(), gisview);
		if (true == this->try_clicking_trackpoint(tp_search, this->tracks)) {
			select_tool->selected_tree_item_type_id = tp_search.closest_track->type_id;
			this->layer_tool_maybe_start_holding_tp(ev, select_tool, tp_search.closest_track, tp_search.closest_tp_iter);
			return true;
		}
	}


	/* Try again for routes. */
	const bool routes_visible = this->routes.is_visible();
	const bool routes_inside = BBOX_INTERSECT (this->routes.get_bbox(), viewport_bbox);
	qDebug() << SG_PREFIX_I << "Routes are" << (routes_visible ? "visible" : "invisible") << "and" << (routes_inside ? "inside" : "outside") << "of viewport";
	if (routes_visible && routes_inside) {
		TrackpointSearch tp_search(ev->x(), ev->y(), gisview);
		if (true == this->try_clicking_trackpoint(tp_search, this->routes)) {
			select_tool->selected_tree_item_type_id = tp_search.closest_track->type_id;
			this->layer_tool_maybe_start_holding_tp(ev, select_tool, tp_search.closest_track, tp_search.closest_tp_iter);
			return true;
		}
	}


	/* Mouse click didn't happen anywhere near a Trackpoint or Waypoint from this layer.
	   So unmark/deselect all "current"/"edited" elements of this layer. */
	qDebug() << SG_PREFIX_I << "Mouse click for Select tool didn't click any data, resetting info";
	/* At this abstraction layer we only have to call these two
	   methods. Everything else below this abstraction layer will
	   be handled by the two methods. */
	this->selected_wp_reset();
	this->selected_track_reset();

	/* Blank info. */
	this->get_window()->get_statusbar()->set_message(StatusBarField::Info, "");

	return false;
}




bool LayerTRW::try_clicking_waypoint(WaypointSearch & wp_search)
{
	this->waypoints.search_closest_wp(wp_search);
	if (NULL == wp_search.closest_wp) {
		qDebug() << SG_PREFIX_I << "No waypoint clicked";
		return false;
	}

	qDebug() << SG_PREFIX_I << wp_search.closest_wp->name << "waypoint clicked";

	LayerTRW * trw = (LayerTRW *) wp_search.closest_wp->get_owning_layer();
	trw->selected_wp_set(wp_search.closest_wp);
	wp_search.closest_wp->click_in_tree("Waypoint has been selected with 'select tool' click");

	return true;
}




bool LayerTRW::try_clicking_trackpoint(TrackpointSearch & tp_search, LayerTRWTracks & tracks_or_routes)
{
	tracks_or_routes.track_search_closest_tp(tp_search);
	if (NULL == tp_search.closest_tp) {
		qDebug() << SG_PREFIX_I << "No trackpoint clicked";
		return false;
	}

	if ("sg.trw.tracks" == tracks_or_routes.type_id) {
		qDebug() << SG_PREFIX_I << "Trackpoint in track" << tp_search.closest_track->name << "clicked";
	} else {
		qDebug() << SG_PREFIX_I << "Trackpoint in route" << tp_search.closest_track->name << "clicked";
	}

	LayerTRW * trw = (LayerTRW *) tp_search.closest_track->get_owning_layer();
	trw->selected_track_set(tp_search.closest_track, TrackpointReference(tp_search.closest_tp_iter, true));
	tp_search.closest_track->click_in_tree("Track has been selected with 'select tool' click");

	return true;
}




/*
  This method is for handling "double click" event coming from generic Select tool.
  The generic Select tool doesn't know how to implement layer-specific response to double-click, so the layer has to implement the behaviour itself.

  Returns true if a waypoint or track is found near the requested event position for this particular layer.
  The item found is automatically selected.
  This is a tool like feature but routed via the layer interface, since it's instigated by a 'global' layer tool in window.c
*/
bool LayerTRW::handle_select_tool_double_click(QMouseEvent * ev, GisViewport * gisview, LayerToolSelect * select_tool)
{
	/* Double-click will be handled by checking event->flags() in the function below, and calling proper handling method. */
	qDebug() << SG_PREFIX_D;
	return this->handle_select_tool_click(ev, gisview, select_tool);
}




void LayerTRW::layer_tool_maybe_start_holding_tp(QMouseEvent * ev, LayerTool * tool, Track * track, TrackPoints::iterator & tp_iter)
{
	const bool tp_is_already_selected = track->is_selected()
		&& 1 == track->get_selected_children().get_count()
		&& track->get_selected_children().is_member(*tp_iter);

	/* Can move the trackpoint immediately when control held or it's the previously selected tp. */
	if ((ev->modifiers() & TRACKPOINT_MODIFIER_KEY) || tp_is_already_selected) {
		/* Remember position at which selection occurred. */
		qDebug() << SG_PREFIX_I << "Will call 'start holding object'";
		tool->start_holding_object(ScreenPos(ev->x(), ev->y()));
	} else {
		qDebug() << SG_PREFIX_I << "Not starting holding object";
	}
}




void LayerTRW::layer_tool_maybe_start_holding_wp(QMouseEvent * ev, LayerTool * tool, Waypoint * wp)
{
	/* Too easy to move it so must be holding shift to start immediately moving it
	   or otherwise be previously selected but not have an image (otherwise clicking within image bounds (again) moves it). */
	if (ev->modifiers() & WAYPOINT_MODIFIER_KEY
	    || (this->selected_wp_get() == wp && wp->image_full_path.isEmpty())) {

		/* Remember position at which selection occurred. */
		qDebug() << SG_PREFIX_I << "Will call 'start holding object'";
	        tool->start_holding_object(ScreenPos(ev->x(), ev->y()));
	} else {
		qDebug() << SG_PREFIX_I << "Not starting holding object";
	}
}




/**
   Show context menu for the currently selected item.

   This function is called when generic Select tool is selected from tool bar.
   It would be nice to somehow merge this function with code used when "edit track/route/waypoint" tool is selected.
*/
bool LayerTRW::handle_select_tool_context_menu(QMouseEvent * ev, GisViewport * gisview)
{
	if (ev->button() != Qt::RightButton) {
		return false;
	}

	if (this->type != LayerType::TRW) {
		return false;
	}

	if (!this->tracks.is_visible() && !this->waypoints.is_visible() && !this->routes.is_visible()) {
		return false;
	}

	Track * track = this->selected_track_get(); /* Track or route that is currently being selected/edited. */
	if (track && track->is_visible()) { /* Track or Route. */
		if (!track->name.isEmpty()) {

			QMenu menu(gisview);

			track->add_context_menu_items(menu, false);
			menu.exec(QCursor::pos());
			return true;
		}
	}

	Waypoint * wp = this->selected_wp_get(); /* Waypoint that is currently being selected/edited. */
	if (wp && wp->is_visible()) {
		if (!wp->name.isEmpty()) {
			QMenu menu(gisview);

			wp->add_context_menu_items(menu, false);
			menu.exec(QCursor::pos());
			return true;
		}
	}

	/* No Track/Route/Waypoint selected. */
	return false;
}




LayerToolTRWEditWaypoint::LayerToolTRWEditWaypoint(Window * window_, GisViewport * gisview_) : LayerTool(window_, gisview_, LayerType::TRW)
{
	this->id_string = LAYER_TRW_TOOL_EDIT_WAYPOINT;

	this->action_icon_path   = ":/icons/layer_tool/trw_edit_wp_18.png";
	this->action_label       = QObject::tr("&Edit Waypoint");
	this->action_tooltip     = QObject::tr("Edit Waypoint");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_E;

	this->cursor_click = QCursor(QPixmap(":/cursors/trw_edit_wp.png"), 0, 0);
	this->cursor_release = QCursor(QPixmap(":/cursors/trw_edit_wp.png"), 0, 0);

	/* One Waypoint Properties Dialog for all layers. */
	this->wp_properties_dialog = new WpPropertiesDialog(gisview_->get_coord_mode(), window_);
}




LayerToolTRWEditWaypoint::~LayerToolTRWEditWaypoint()
{
	delete this->wp_properties_dialog;
}




ToolStatus LayerToolTRWEditWaypoint::internal_handle_mouse_click(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (trw->type != LayerType::TRW) {
		qDebug() << SG_PREFIX_D << "Not TRW layer";
		return ToolStatus::Ignored;
	}
	if (this->tool_is_holding_object) {
		qDebug() << SG_PREFIX_D << "Already holding an object";
		return ToolStatus::Ignored;
	}
	if (!trw->is_visible() || !trw->waypoints.is_visible()) {
		qDebug() << SG_PREFIX_D << "Not visible";
		return ToolStatus::Ignored;
	}

	/* Does this tool have a waypoint, on which it can operate? */
	Waypoint * newly_selected_wp = NULL;
	Waypoint * current_wp = trw->selected_wp_get();

	if (current_wp && current_wp->is_visible()) {
		/* Some waypoint has already been activated before the
		   click happened, e.g. by selecting it in items tree.

		   First check if that waypoint is close enough to
		   click coordinates to keep that waypoint as selected.

		   Other waypoints (not selected) may be even closer
		   to click coordinates, but the pre-selected waypoint
		   has priority. */

		ScreenPos wp_pos;
		this->gisview->coord_to_screen_pos(current_wp->coord, wp_pos);

		const ScreenPos event_pos = ScreenPos(ev->x(), ev->y());

		if (ScreenPos::are_closer_than(wp_pos, event_pos, WAYPOINT_SIZE_APPROX)) {

			/* A waypoint has been selected in some way
			   (e.g. by selecting it in items tree), and
			   now it is also selected by this tool. */
			qDebug() << SG_PREFIX_I << "Will call 'start holding object'";
			this->start_holding_object(event_pos);

			/* Global "edited waypoint" now became tool's edited waypoint. */
			newly_selected_wp = current_wp;
			qDebug() << SG_PREFIX_D << "Setting our waypoint";
		}
	}

	if (!newly_selected_wp) {
		/* Either there is no globally selected waypoint, or
		   it was too far from click. Either way the tool
		   doesn't have any waypoint to operate on - yet. Try
		   to find one close to click position. */

		WaypointSearch wp_search(ev->x(), ev->y(), gisview);
		if (true == trw->try_clicking_waypoint(wp_search)) {
			/* TODO_LATER: do we need to verify that
			   wp_search.closest_wp != current_wp? */
#if 0
			trw->emit_tree_item_changed("TRW - edit waypoint - click");
#endif
			qDebug() << SG_PREFIX_I << "Will call 'start holding object'";
			this->start_holding_object(ScreenPos(ev->x(), ev->y()));
		        newly_selected_wp = wp_search.closest_wp;
		}
	}

	if (!newly_selected_wp) {
		/* No luck. No waypoint to operate on.

		   We have clicked on some empty space, so make sure
		   that no waypoint in this layer is globally
		   selected, no waypoint is selected by this tool, and
		   no waypoint is drawn as selected. */

		const bool wp_was_edited = trw->selected_wp_reset();
		qDebug() << SG_PREFIX_I << "Will call 'stop holding object'";
		const bool some_object_was_released = this->stop_holding_object();

		if (wp_was_edited || some_object_was_released) {
			trw->emit_tree_item_changed("Waypoint has been deselected after mouse click on area of layer without waypoints");
		}

		return ToolStatus::Ignored;
	}

	/* Finally, a waypoint that this tool can operate on. Not too much operation, though. */
	switch (ev->button()) {
	case Qt::RightButton: {
		QMenu menu;
		newly_selected_wp->add_context_menu_items(menu, false);
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

	return ToolStatus::Ack;
}




ToolStatus LayerToolTRWEditWaypoint::internal_handle_mouse_move(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;
	if (trw->type != LayerType::TRW) {
		return ToolStatus::Ignored;
	}

	switch (ev->buttons()) { /* Notice that it's ::buttons(), not ::button(). */
	case Qt::LeftButton: {
		/* Recognize that an object is being moved. */
		if (sg_ret::ok != this->remember_object_moving()) {
			qDebug() << SG_PREFIX_E << "Not moving";
			return ToolStatus::Error;
		}

		Coord new_coord = this->gisview->screen_pos_to_coord(ev->x(), ev->y());
		trw->get_nearby_snap_coordinates(new_coord, ev, gisview);
		trw->set_selected_waypoint_position(new_coord, false);
		return ToolStatus::Ack;
	}
	default:
		return ToolStatus::Ignored;
	}
}




ToolStatus LayerToolTRWEditWaypoint::internal_handle_mouse_release(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (trw->type != LayerType::TRW) {
		return ToolStatus::Ignored;
	}

	if (!this->tool_is_holding_object) {
		/* ::handle_mouse_press() probably never happened. */
		return ToolStatus::Ignored;
	}

	switch (ev->button()) {
	case Qt::LeftButton: {
		Coord new_coord = this->gisview->screen_pos_to_coord(ev->x(), ev->y());
		trw->get_nearby_snap_coordinates(new_coord, ev, this->gisview);
		trw->set_selected_waypoint_position(new_coord, true);

		qDebug() << SG_PREFIX_I << "Will call 'stop holding object'";
		this->stop_holding_object();

		//trw->emit_tree_item_changed("TRW - edit waypoint - mouse release");
		return ToolStatus::Ack;
		}

	case Qt::RightButton:
	default:
		qDebug() << SG_PREFIX_I << "Will call 'stop holding object'";
		this->stop_holding_object();
		return ToolStatus::Ignored;
	}
}




/*
  See if mouse event @param ev happened close to a Trackpoint or a
  Waypoint and if keyboard modifier specific for Trackpoint or
  Waypoint was used. If both conditions are true, put coordinates of
  such Trackpoint or Waypoint in @param point_coord.

  @reviewed-on: 2019-09-14
*/
bool LayerTRW::get_nearby_snap_coordinates(Coord & point_coord, QMouseEvent * ev, GisViewport * gisview)
{
	/* Search close trackpoint. */
	if (ev->modifiers() & TRACKPOINT_MODIFIER_KEY) {
		TrackpointSearch search(ev->x(), ev->y(), gisview);
		this->tracks.track_search_closest_tp(search); /* TODO: what about routes? Don't we want to snap to trackpoints in routes? */

		if (NULL != search.closest_tp) {
			point_coord = search.closest_tp->coord;
			return true;
		}
	}

	/* Search close waypoint. */
	if (ev->modifiers() & WAYPOINT_MODIFIER_KEY) {
		WaypointSearch search(ev->x(), ev->y(), gisview);
		this->waypoints.search_closest_wp(search);

		if (NULL != search.closest_wp) {
			point_coord = search.closest_wp->coord;
			return true;
		}
	}

	return false;
}




LayerToolTRWNewTrack::LayerToolTRWNewTrack(Window * window_, GisViewport * gisview_, bool is_route) : LayerTool(window_, gisview_, LayerType::TRW)
{
	this->is_route_tool = is_route;
	if (is_route) {
		this->id_string = LAYER_TRW_TOOL_CREATE_ROUTE;

		this->action_icon_path   = ":/icons/layer_tool/trw_add_route_18.png";
		this->action_label       = QObject::tr("Create &Route");
		this->action_tooltip     = QObject::tr("Create Route");
		this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_B;

		this->pan_handler = true;  /* Still need to handle clicks when in PAN mode to disable the potential trackpoint drawing. */
		this->cursor_click = QCursor(QPixmap(":/cursors/trw_add_route.png"), 0, 0);
		this->cursor_release = QCursor(QPixmap(":/cursors/trw_add_route.png"), 0, 0);
	} else {
		this->id_string = LAYER_TRW_TOOL_CREATE_TRACK;

		this->action_icon_path   = ":/icons/layer_tool/trw_add_tr_18.png";
		this->action_label       = QObject::tr("Create &Track");
		this->action_tooltip     = QObject::tr("Create Track");
		this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_T;

		this->pan_handler = true;  /* Still need to handle clicks when in PAN mode to disable the potential trackpoint drawing. */
		this->cursor_click = QCursor(QPixmap(":/cursors/trw_add_tr.png"), 0, 0);
		this->cursor_release = QCursor(QPixmap(":/cursors/trw_add_tr.png"), 0, 0);
	}
}




/*
 * Draw specified pixmap.
 */
static int draw_sync(LayerTRW * trw, GisViewport * gisview, const QPixmap & pixmap)
{
	if (1 /* trw->draw_sync_do*/ ) {
		gisview->draw_pixmap(pixmap, 0, 0);
		qDebug() << SG_PREFIX_SIGNAL << "Will emit 'tree_item_changed()' signal for" << trw->get_name();
		emit trw->tree_item_changed(trw->get_name());
#ifdef K_OLD_IMPLEMENTATION
		/* Sometimes don't want to draw normally because another
		   update has taken precedent such as panning the display
		   which means this pixmap is no longer valid. */
		gdk_draw_drawable(ds->drawable,
				  ds->gc,
				  ds->pixmap,
				  0, 0, 0, 0, -1, -1);
#endif
		trw->draw_sync_done = true;
	}
	return 0;
}


/*
 * Actually set the message in statusbar.
 */
static void statusbar_write(const Distance & total_distance, const Distance & last_step_distance, const Altitude & elev_gain, const Altitude & elev_loss, const Angle & angle, LayerTRW * layer)
{
	/* Only show elevation data when track has some elevation properties. */
	QString str_gain_loss;
	QString str_last_step;
	const QString total_distance_string = total_distance.convert_to_unit(Preferences::get_unit_distance()).to_nice_string();

	if (elev_gain.is_valid() || elev_loss.is_valid()) {
		const HeightUnit height_unit = Preferences::get_unit_height();
		str_gain_loss = QObject::tr(" - Gain %1 / Loss %2")
			.arg(elev_gain.convert_to_unit(height_unit).to_string())
			.arg(elev_loss.convert_to_unit(height_unit).to_string());
	}

	if (last_step_distance.is_valid()) {
		const QString last_step_distance_string = last_step_distance.convert_to_unit(Preferences::get_unit_distance()).to_nice_string();
		str_last_step = QObject::tr(" - Bearing %1 - Step %2").arg(angle.to_string()).arg(last_step_distance_string);
	}

	/* Write with full gain/loss information. */
	const QString msg = QObject::tr("Total %1%2%3").arg(total_distance_string).arg(str_last_step).arg(str_gain_loss);
	layer->get_window()->get_statusbar()->set_message(StatusBarField::Info, msg);
}




/*
 * Figure out what information should be set in the statusbar and then write it.
 */
void LayerTRW::update_statusbar()
{
	/* Get elevation data. */
	Altitude elev_gain;
	Altitude elev_loss;
	this->selected_track_get()->get_total_elevation_gain(elev_gain, elev_loss);

	/* Find out actual distance of current track. */
	const Distance total_distance = this->selected_track_get()->get_length();

	statusbar_write(total_distance, Distance(), elev_gain, elev_loss, Angle(), this);
}




ToolStatus LayerToolTRWNewTrack::internal_handle_mouse_move(Layer * layer, QMouseEvent * ev)
{
	if (!this->ruler) {
		/* Cursor is moved, but there was no mouse click that would start a new track segment. */
		return ToolStatus::Ignored;
	}

	LayerTRW * trw = (LayerTRW *) layer;
	Track * track = trw->selected_track_get();

	/* If we haven't sync'ed yet, we don't have time to do more. */
	if (/* trw->draw_sync_done && */ track && !track->empty()) {

		QPixmap marked_pixmap = this->orig_viewport_pixmap;
		QPainter painter(&marked_pixmap);



		this->ruler->set_end(ev->x(), ev->y());


		/* We didn't actually create the new track fragment
		   yet, so track->get_length() returns length without
		   this last, non-existent, work-in-progress fragment. */
		const Distance total_distance = track->get_length() + ruler->get_line_distance();

		this->ruler->set_total_distance(total_distance);
		this->ruler->paint_ruler(painter, Preferences::get_create_track_tooltip());


		this->gisview->set_pixmap(marked_pixmap);
		/* This will call GisViewport::paintEvent(), triggering final render to screen. */
		this->gisview->update();


		/* Get elevation data. */
		Altitude elev_gain;
		Altitude elev_loss;
		track->get_total_elevation_gain(elev_gain, elev_loss);


		/* Adjust elevation data (if available) for the current pointer position. */
		const Coord cursor_coord = this->gisview->screen_pos_to_coord(ev->x(), ev->y());
		const Altitude elev_new = DEMCache::get_elev_by_coord(cursor_coord, DemInterpolation::Best);
		const Trackpoint * last_tpt = track->get_tp_last();
		if (elev_new.is_valid()) {
			if (last_tpt->altitude.is_valid()) {
				/* Adjust elevation of last track point. */
				if (elev_new > last_tpt->altitude) {
					/* Going up. */
					const Altitude new_value = elev_gain + (elev_new - last_tpt->altitude);
					elev_gain = new_value;
				} else {
					/* Going down. */
					const Altitude new_value = elev_loss + (last_tpt->altitude - elev_new);
					elev_loss = new_value;
				}
			}
		}

		/* Update statusbar with full gain/loss information. */
		statusbar_write(total_distance, ruler->get_line_distance(), elev_gain, elev_loss, this->ruler->get_angle(), trw);

		return ToolStatus::AckGrabFocus;
	}
	return ToolStatus::Ack;
}




ToolStatus LayerToolTRWNewTrack::internal_handle_key_press(Layer * layer, QKeyEvent * ev)
{
	if (this->id_string == LAYER_TRW_TOOL_CREATE_TRACK && !this->creation_in_progress) {
		/* Track is not being created at the moment, so a Key can't affect work of this tool. */
		return ToolStatus::Ignored;
	}
	if (this->id_string == LAYER_TRW_TOOL_CREATE_ROUTE && !this->creation_in_progress) {
		/* Route is not being created at the moment, so a Key can't affect work of this tool. */
		return ToolStatus::Ignored;
	}

	LayerTRW * trw = (LayerTRW *) layer;
	Track * track = trw->selected_track_get();

	/* Check of consistency between LayerTRW and the tool. */
	if (true && !track) {
		qDebug() << SG_PREFIX_E << "New track handle key press: creation-in-progress=true, but no track selected in layer";
		return ToolStatus::Ignored;
	}


	switch (ev->key()) {
	case Qt::Key_Escape:
		this->creation_in_progress = NULL;
		/* Bin track if only one point as it's not very useful. */
		if (track->get_tp_count() == 1) {
			trw->detach_from_container(track);
			trw->detach_from_tree(track);
			delete track;
		}

		trw->selected_track_reset();
		trw->emit_tree_item_changed("TRW - new track - handle key escape");
		return ToolStatus::Ack;

	case Qt::Key_Backspace:
		track->remove_last_trackpoint();
		trw->update_statusbar();
		trw->emit_tree_item_changed("TRW - new track - handle key backspace");
		return ToolStatus::Ack;

	default:
		return ToolStatus::Ignored;
	}
}




/**
   \brief Add a trackpoint to track/route in response to given mouse click

   Call this function only for single clicks.

   \param trw - parent layer of track
   \param track - track to modify
   \param ev - mouse event
   \param viewport - viewport, in which a click occurred
*/
ToolStatus create_new_trackpoint(LayerTRW * trw, Track * track, QMouseEvent * ev, GisViewport * gisview)
{
	if (!track) {
		qDebug() << SG_PREFIX_E << "NULL track argument";
		return ToolStatus::Ignored;
	}


	Trackpoint * tp = new Trackpoint();
	tp->coord = gisview->screen_pos_to_coord(ev->x(), ev->y());

	/* Maybe snap to other Trackpoint. */
	trw->get_nearby_snap_coordinates(tp->coord, ev, gisview);

	tp->newsegment = false;
	tp->timestamp.invalidate();

	track->add_trackpoint(tp, true); /* Ensure bounds is updated. */
	/* Auto attempt to get elevation from DEM data (if it's available). */
	track->apply_dem_data_last_trackpoint();


	trw->emit_tree_item_changed("TRW - extend track with mouse click end");

	return ToolStatus::Ack;
}




ToolStatus LayerToolTRWNewTrack::internal_handle_mouse_click(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	/* If we were running the route finder, cancel it. */
	trw->route_finder_started = false;

	if (ev->button() == Qt::MiddleButton) {
		/* As the display is panning, the new track pixmap is now invalid so don't draw it
		   otherwise this drawing done results in flickering back to an old image. */
		/* TODO_LATER: implement panning during track creation. */
		trw->draw_sync_do = false;
		return ToolStatus::Ignored;

	} else if (ev->button() == Qt::RightButton) {
		Track * track = trw->selected_track_get();
		if (track) {
			track->remove_last_trackpoint();
			trw->update_statusbar();
			trw->emit_tree_item_changed("Track's Last trackpoint has been removed after right mouse button click");
			return ToolStatus::Ack;
		} else {
			return ToolStatus::Ignored;
		}
	} else if (ev->button() != Qt::LeftButton) {
		qDebug() << SG_PREFIX_E << "Unexpected mouse button";
		return ToolStatus::Ignored;
	} else {
		; /* Pass, go to handling Qt::LeftButton. */
	}


	/* New click = new track fragment = new ruler for indication of that track fragment. */
	if (this->ruler) {
		delete this->ruler;
		this->ruler = NULL;
	}
	this->ruler = new Ruler(this->gisview, Preferences::get_unit_distance());
	this->ruler->set_line_pen(trw->painter->m_selected_track_new_point_pen);
	this->ruler->set_begin(ev->x(), ev->y());
	this->orig_viewport_pixmap = this->gisview->get_pixmap(); /* Save clean viewport (clean == without ruler drawn on top of it). */


	Track * track = NULL;
	if (this->creation_in_progress) {
		/* Check of consistency between LayerTRW and the tool. */
		if (true && !trw->selected_track_get()) {
			qDebug() << SG_PREFIX_E << "mismatch A";
		}

		track = trw->selected_track_get();
	} else {
		/* Check of consistency between LayerTRW and the tool. */
		if (true && trw->selected_track_get()) {
			qDebug() << SG_PREFIX_E << "mismatch B";
		}

		/* FIXME: how to handle a situation, when a route is being created right now? */
		QString new_name;
		if (this->is_route_tool) {
			new_name = trw->new_unique_element_name("sg.trw.route", QObject::tr("Route"));
		} else {
			new_name = trw->new_unique_element_name("sg.trw.track", QObject::tr("Track"));
		}
		if (Preferences::get_ask_for_create_track_name()) {
			new_name = a_dialog_new_track(new_name, this->is_route_tool, trw->get_window());
			if (new_name.isEmpty()) {
				return ToolStatus::Ignored;
			}
		}
		if (this->is_route_tool) {
			track = trw->new_route_create_common(new_name);
		} else {
			track = trw->new_track_create_common(new_name);
		}
		this->creation_in_progress = trw;
	}

	return create_new_trackpoint(trw, track, ev, this->gisview);
}




ToolStatus LayerToolTRWNewTrack::internal_handle_mouse_double_click(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;
	if (trw->type != LayerType::TRW) {
		return ToolStatus::Ignored;
	}

	if (ev->button() != Qt::LeftButton) {
		return ToolStatus::Ignored;
	}

	if (this->ruler) {
		delete this->ruler;
		this->ruler = NULL;
		this->orig_viewport_pixmap = QPixmap(); /* Invalidate. */
	}


	/* End the process of creating a track. */
	if (this->creation_in_progress) {
		/* Check of consistency between LayerTRW and the tool. */
		if (true && !trw->selected_track_get()) {
			qDebug() << SG_PREFIX_E << "inconsistency A";
		}

		if (!trw->selected_track_get()->empty()) {
			trw->selected_track_reset();
			this->creation_in_progress = NULL;
		}
	} else {
		/* Check of consistency between LayerTRW and the tool. */
		if (true && trw->selected_track_get()) {
			qDebug() << SG_PREFIX_E << "inconsistency B";
		}
	}

	if (this->is_route_tool) {
		trw->emit_tree_item_changed("Completed creating new route (detected double mouse click)");
	} else {
		trw->emit_tree_item_changed("Completed creating new track (detected double mouse click)");
	}

	return ToolStatus::Ack;
}




ToolStatus LayerToolTRWNewTrack::internal_handle_mouse_release(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (ev->button() == Qt::MiddleButton) {
		/* Pan moving ended - enable potential point drawing again. */
		trw->draw_sync_do = true;
		trw->draw_sync_done = true;
	}

	return ToolStatus::Ack;
}




LayerToolTRWNewWaypoint::LayerToolTRWNewWaypoint(Window * window_, GisViewport * gisview_) : LayerTool(window_, gisview_, LayerType::TRW)
{
	this->id_string = LAYER_TRW_TOOL_CREATE_WAYPOINT;

	this->action_icon_path   = ":/icons/layer_tool/trw_add_wp_18.png";
	this->action_label       = QObject::tr("Create &Waypoint");
	this->action_tooltip     = QObject::tr("Create Waypoint");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_W;

	this->cursor_click = QCursor(QPixmap(":/cursors/trw_add_wp.png"), 0, 0);
	this->cursor_release = QCursor(QPixmap(":/cursors/trw_add_wp.png"), 0, 0);
}




ToolStatus LayerToolTRWNewWaypoint::internal_handle_mouse_click(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (trw->type != LayerType::TRW) {
		return ToolStatus::Ignored;
	}

	bool visible_with_parents = false;
	const Coord coord = this->gisview->screen_pos_to_coord(ev->x(), ev->y());
	qDebug() << SG_PREFIX_I << "Will create new waypoint with coordinates" << coord;
	if (trw->new_waypoint(coord, visible_with_parents, trw->get_window())) {
		trw->get_waypoints_node().recalculate_bbox();
		if (visible_with_parents) {
			qDebug() << SG_PREFIX_I << "Created new waypoint, will emit update";
			trw->emit_tree_item_changed("New waypoint created with 'new waypoint' tool");
		}
	}
	return ToolStatus::Ack;
}




LayerToolTRWEditTrackpoint::LayerToolTRWEditTrackpoint(Window * window_, GisViewport * gisview_) : LayerTool(window_, gisview_, LayerType::TRW)
{
	this->id_string = LAYER_TRW_TOOL_EDIT_TRACKPOINT;

	this->action_icon_path   = ":/icons/layer_tool/trw_edit_tr_18.png";
	this->action_label       = QObject::tr("Edit Trac&kpoint");
	this->action_tooltip     = QObject::tr("Edit Trackpoint");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_K;

	this->cursor_click = QCursor(QPixmap(":/cursors/trw_edit_tr.png"), 0, 0);
	this->cursor_release = QCursor(QPixmap(":/cursors/trw_edit_tr.png"), 0, 0);

	/* One Trackpoint Properties Dialog for all layers. */
	this->tp_properties_dialog = new TpPropertiesDialog(gisview_->get_coord_mode(), window_);
}




LayerToolTRWEditTrackpoint::~LayerToolTRWEditTrackpoint()
{
	delete this->tp_properties_dialog;
}




/**
 * On 'initial' click: search for the nearest trackpoint or routepoint and store it as the current trackpoint.
 * Then update the viewport, statusbar and edit dialog to draw the point as being selected and it's information.
 * On subsequent clicks: (as the current trackpoint is defined) and the click is very near the same point
 * then initiate the move operation to drag the point to a new destination.
 * NB The current trackpoint will get reset elsewhere.
 */
ToolStatus LayerToolTRWEditTrackpoint::internal_handle_mouse_click(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (ev->button() != Qt::LeftButton) {
		return ToolStatus::Ignored;
	}

	if (trw->type != LayerType::TRW) {
		return ToolStatus::Ignored;
	}

	if (!trw->is_visible() && !(trw->tracks.is_visible() && trw->routes.is_visible())) {
		return ToolStatus::Ignored;
	}

	Track * track = trw->selected_track_get();
	if (track && 1 == track->get_selected_children().get_count()) {
		/* First check if it is within range of prev. tp. and
		   if current_tp track is shown. (if it is, we are
		   moving that trackpoint). */

		const TrackpointReference & tp_ref = track->get_selected_children().front();
		if (tp_ref.m_iter_valid) {
			const Trackpoint * tp = *tp_ref.m_iter;
			ScreenPos tp_pos;
			this->gisview->coord_to_screen_pos(tp->coord, tp_pos);
			const ScreenPos event_pos = ScreenPos(ev->x(), ev->y());

			if (track->is_visible() && ScreenPos::are_closer_than(tp_pos, event_pos, TRACKPOINT_SIZE_APPROX)) {
				qDebug() << SG_PREFIX_I << "Will call 'start holding object'";
				this->start_holding_object(event_pos);
				return ToolStatus::Ack;
			}
		} else {
			qDebug() << SG_PREFIX_E << "Invalid tp reference";
		}
	}

	if (trw->get_tracks_node().is_visible()) {
		TrackpointSearch tp_search(ev->x(), ev->y(), this->gisview);
		if (trw->try_clicking_trackpoint(tp_search, trw->get_tracks_node())) {
			qDebug() << SG_PREFIX_I << "Will call 'maybe start holding object'";
			trw->layer_tool_maybe_start_holding_tp(ev, this, tp_search.closest_track, tp_search.closest_tp_iter);
			return ToolStatus::Ack;
		}
	}

	if (trw->get_routes_node().is_visible()) {
		TrackpointSearch tp_search(ev->x(), ev->y(), this->gisview);
		if (trw->try_clicking_trackpoint(tp_search, trw->get_routes_node())) {
			qDebug() << SG_PREFIX_I << "Will call 'maybe start holding object'";
			trw->layer_tool_maybe_start_holding_tp(ev, this, tp_search.closest_track, tp_search.closest_tp_iter);
			return ToolStatus::Ack;
		}
	}

	/* The mouse click wasn't near enough any Trackpoint that belongs to any tracks/routes in this layer. */
	return ToolStatus::Ignored;
}




ToolStatus LayerToolTRWEditTrackpoint::internal_handle_mouse_move(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;
	if (trw->type != LayerType::TRW) {
		qDebug() << SG_PREFIX_E << "Ignoring, not TRW layer";
		return ToolStatus::Ignored;
	}

	switch (ev->buttons()) { /* Notice that it's ::buttons(), not ::button(). */
	case Qt::LeftButton: {
		/* Recognize that an object is being moved. */
		if (sg_ret::ok != this->remember_object_moving()) {
			qDebug() << SG_PREFIX_E << "Not moving";
			return ToolStatus::Error;
		}

		qDebug() << SG_PREFIX_E << "Will now set new position";

		Coord new_coord = this->gisview->screen_pos_to_coord(ev->x(), ev->y());
		trw->get_nearby_snap_coordinates(new_coord, ev, this->gisview);
		trw->set_selected_trackpoint_position(new_coord, false);
		return ToolStatus::Ack;
	}
	default:
		return ToolStatus::Ignored;
	}
}




ToolStatus LayerToolTRWEditTrackpoint::internal_handle_mouse_release(Layer * layer, QMouseEvent * ev)
{
	if (ev->button() != Qt::LeftButton) {
		return ToolStatus::Ignored;
	}

	LayerTRW * trw = (LayerTRW *) layer;
	if (trw->type != LayerType::TRW) {
		return ToolStatus::Ignored;
	}

	Track * track = trw->selected_track_get(); /* This is the track, to which belongs the edited trackpoint. TODO: how can we be sure that a trackpoint is selected? */
	if (!track) {
		/* Well, there was no track that was edited, so nothing to do here. */
		return ToolStatus::Ignored;
	}

	/* Recognize that an object is being moved. */
	if (sg_ret::ok != this->remember_object_moving()) {
		qDebug() << SG_PREFIX_E << "Not moving";
		return ToolStatus::Error;
	}

	Coord new_coord = this->gisview->screen_pos_to_coord(ev->x(), ev->y());
	trw->get_nearby_snap_coordinates(new_coord, ev, this->gisview);
	trw->set_selected_trackpoint_position(new_coord, true);

	qDebug() << SG_PREFIX_I << "Will call 'stop holding object'";
	this->stop_holding_object();

	return ToolStatus::Ack;
}




LayerToolTRWExtendedRouteFinder::LayerToolTRWExtendedRouteFinder(Window * window_, GisViewport * gisview_) : LayerTool(window_, gisview_, LayerType::TRW)
{
	this->id_string = LAYER_TRW_TOOL_ROUTE_FINDER;

	this->action_icon_path   = ":/icons/layer_tool/trw_find_route_18.png";
	this->action_label       = QObject::tr("Route &Finder");
	this->action_tooltip     = QObject::tr("Route Finder");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_F;

	this->pan_handler = true;  /* Still need to handle clicks when in PAN mode to disable the potential trackpoint drawing. */

	this->cursor_click = QCursor(QPixmap(":/cursors/trw____.png"), 0, 0);
	this->cursor_release = QCursor(Qt::ArrowCursor);
}




ToolStatus LayerToolTRWExtendedRouteFinder::internal_handle_mouse_move(Layer * layer, QMouseEvent * ev)
{
	/* TODO_2_LATER: implement function similar to LayerToolTRWNewTrack::handle_mouse_move() */
	return ToolStatus::Ignored;
}




ToolStatus LayerToolTRWExtendedRouteFinder::internal_handle_mouse_release(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (ev->button() == Qt::MiddleButton) {
		/* Pan moving ended - enable potential point drawing again. */
		trw->draw_sync_do = true;
		trw->draw_sync_done = true;
	}

	return ToolStatus::Ack;
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

	trw->emit_tree_item_changed("TRW - extended route finder");

	/* Remove last ' to:...' */
	if (!track->comment.isEmpty()) {
#ifdef K_FIXME_RESTORE
		char * last_to = strrchr(track->comment, 't');
		if (last_to && (last_to - track->comment > 1)) {
			track->set_comment(track->comment.left(last_to - track->comment - 1));
		}
#endif
	}
}




ToolStatus LayerToolTRWExtendedRouteFinder::internal_handle_mouse_click(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	Track * track = trw->selected_track_get();

	Coord tmp = this->gisview->screen_pos_to_coord(ev->x(), ev->y());
	if (ev->button() == Qt::RightButton && track) {
		this->undo(trw, track);

	} else if (ev->button() == Qt::MiddleButton) {
		trw->draw_sync_do = false;
		return ToolStatus::Ignored;
	}
	/* If we started the track but via undo deleted all the track points, begin again. */
	else if (track && track->is_route() && !track->get_tp_first()) {
		return create_new_trackpoint_route_finder(trw, track, ev, this->gisview);

	} else if ((track && track->is_route())
		   || ((ev->modifiers() & TRACKPOINT_MODIFIER_KEY) && track)) {

		Trackpoint * tp_start = track->get_tp_last();
		const LatLon start = tp_start->coord.get_lat_lon();
		const LatLon end = tmp.get_lat_lon();

		trw->route_finder_started = true;
		trw->route_finder_append = true;  /* Merge tracks. Keep started true. */



		/* Update UI to let user know what's going on. */
		StatusBar * sb = trw->get_window()->get_statusbar();
		RoutingEngine * engine = Routing::get_default_engine();
		if (!engine) {
			trw->get_window()->get_statusbar()->set_message(StatusBarField::Info, QObject::tr("Cannot plan route without a default routing engine."));
			return ToolStatus::Ack;
		}
		const QString msg1 = QObject::tr("Querying %1 for route between (%2, %3) and (%4, %5).")
			.arg(engine->get_label())
			.arg(start.lat, 0, 'f', 3) /* ".3f" */
			.arg(start.lon, 0, 'f', 3)
			.arg(end.lat, 0, 'f', 3)
			.arg(end.lon, 0, 'f', 3);
		trw->get_window()->get_statusbar()->set_message(StatusBarField::Info, msg1);

		trw->get_window()->set_busy_cursor();
		bool find_status = Routing::find_route_with_default_engine(trw, start, end);
		trw->get_window()->clear_busy_cursor();

		/* Update UI to say we're done. */

		const QString msg2 = (find_status)
			? QObject::tr("%1 returned route between (%2, %3) and (%4, %5).").arg(engine->get_label()).arg(start.lat, 0, 'f', 3).arg(start.lon, 0, 'f', 3).arg(end.lat, 0, 'f', 3).arg(end.lon, 0, 'f', 3) /* ".3f" */
			: QObject::tr("Error getting route from %1.").arg(engine->get_label());

		trw->get_window()->get_statusbar()->set_message(StatusBarField::Info, msg2);


		trw->emit_tree_item_changed("TRW - extended route finder - handle mouse click - route");
	} else {
		trw->selected_track_reset();

		LayerTool * new_route_tool = trw->get_window()->get_toolbox()->get_tool(LAYER_TRW_TOOL_CREATE_ROUTE);
		if (NULL == new_route_tool) {
			qDebug() << SG_PREFIX_E << "Failed to get tool with id =" << LAYER_TRW_TOOL_CREATE_ROUTE;
			return ToolStatus::Ignored;
		}

		/* Create a new route where we will add the planned route to. */
		ToolStatus ret = new_route_tool->handle_mouse_click(trw, ev);

		trw->route_finder_started = true;

		return ret;
	}

	return ToolStatus::Ack;
}




ToolStatus LayerToolTRWExtendedRouteFinder::internal_handle_key_press(Layer * layer, QKeyEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	Track * track = trw->selected_track_get();
	if (!track) {
		return ToolStatus::Ignored;
	}

	switch (ev->key()) {
	case  Qt::Key_Escape:
		trw->route_finder_started = false;
		trw->selected_track_reset();
		trw->emit_tree_item_changed("TRW - extender route finder - handle key escape");
		return ToolStatus::Ack;

	case Qt::Key_Backspace:
		this->undo(trw, track);
		return ToolStatus::Ack;

	default:
		return ToolStatus::Ignored;
	}
}




LayerToolTRWShowPicture::LayerToolTRWShowPicture(Window * window_, GisViewport * gisview_) : LayerTool(window_, gisview_, LayerType::TRW)
{
	this->id_string = LAYER_TRW_TOOL_SHOW_PICTURE;

	this->action_icon_path   = ":/icons/layer_tool/trw_show_picture_18.png";
	this->action_label       = QObject::tr("Show P&icture");
	this->action_tooltip     = QObject::tr("Show Picture");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_I;

	this->cursor_click = QCursor(QPixmap(":/cursors/trw____.png"), 0, 0);
	this->cursor_release = QCursor(Qt::ArrowCursor);
}




ToolStatus LayerToolTRWShowPicture::internal_handle_mouse_click(Layer * layer, QMouseEvent * ev)
{
	if (layer->type != LayerType::TRW) {
		return ToolStatus::Ignored;
	}

	LayerTRW * trw = (LayerTRW *) layer;

	QString found_image = trw->get_waypoints_node().tool_show_picture_wp(ev->x(), ev->y(), this->gisview);
	if (!found_image.isEmpty()) {
		trw->show_wp_picture_cb();
		return ToolStatus::Ack; /* Found a match. */
	} else {
		return ToolStatus::Ignored; /* Go through other layers, searching for a match. */
	}
}
