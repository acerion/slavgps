/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2008, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2009, Hein Ragas <viking@ragas.nl>
 * Copyright (c) 2012-2015, Rob Norris <rw_norris@hotmail.com>
 * Copyright (c) 2012-2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2016-2019, Kamil Ignacak <acerion@wp.pl>
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




ToolStatus helper_move_wp(LayerTRW * trw, LayerToolSelect * tool, QMouseEvent * ev, GisViewport * gisview)
{
	Waypoint * wp = trw->selected_wp_get();
	if (!wp) {
		qDebug() << SG_PREFIX_E << "Will reset waypoint properties dialog data, No waypoint";
		Waypoint::properties_dialog_reset();
		return ToolStatus::Ignored;
	}

	switch (ev->buttons()) { /* Notice that it's ::buttons(), not ::button(). */
	case Qt::LeftButton: {
		if (!tool->can_tool_move_object()) {
			qDebug() << SG_PREFIX_E << "Not moving, tool can't move object";
			return ToolStatus::Error;
		}

		Coord new_coord = gisview->screen_pos_to_coord(ev->x(), ev->y());
		qDebug() << SG_PREFIX_I << "Will now set new position of waypoint:" << new_coord;
		trw->get_nearby_snap_coordinates(new_coord, ev, gisview, wp->m_type_id);

		const bool recalculate_bbox = false; /* During moving of point we shouldn't recalculate Waypoints' bbox. */
		wp->set_coord(new_coord, recalculate_bbox);

		return ToolStatus::Ack;
	}
	default:
		return ToolStatus::Ignored;
	}
}




ToolStatus helper_release_wp(LayerTRW * trw, LayerToolSelect * tool, QMouseEvent * ev, GisViewport * gisview)
{
	if (LayerToolSelect::ObjectState::IsHeld != tool->edited_object_state) {
		/* We can't release what hasn't been held. */
		return ToolStatus::Ignored;
	}

	Waypoint * wp = trw->selected_wp_get();
	if (!wp) {
		qDebug() << SG_PREFIX_E << "Will reset waypoint properties dialog data, No waypoint";
		Waypoint::properties_dialog_reset();
		return ToolStatus::Ignored;
	}

	switch (ev->button()) {
	case Qt::LeftButton: {
		Coord new_coord = gisview->screen_pos_to_coord(ev->x(), ev->y());
		trw->get_nearby_snap_coordinates(new_coord, ev, gisview, wp->m_type_id);

		const bool recalculate_bbox = true; /* We have moved point to final position, so Waypoints' bbox can be recalculated. */
		wp->set_coord(new_coord, recalculate_bbox);

		/* Object is released by tool (so its position no
		   longer changes when cursor moves), but is still a
		   selected item. */
		qDebug() << SG_PREFIX_I << "Setting edited object state to IsSelected";
		tool->edited_object_state = LayerToolSelect::ObjectState::IsSelected;
		/* Not needed, type id stays the same when transitioning from IsHeld to IsSelected.
		tool->selected_tree_item_type_id = xxx;
		*/

		//trw->emit_tree_item_changed("TRW - edit waypoint - mouse release");
		return ToolStatus::Ack;
		}

	case Qt::RightButton:
	default:
		return ToolStatus::Ignored;
	}
}




ToolStatus helper_move_tp(LayerTRW * trw, LayerToolSelect * tool, QMouseEvent * ev, GisViewport * gisview)
{
	Track * track = trw->selected_track_get();
	if (nullptr == track) {
		qDebug() << SG_PREFIX_E << "Will reset trackpoint properties dialog data, no track";
		Track::tp_properties_dialog_reset();
		return ToolStatus::Ignored;
	}

	switch (ev->buttons()) { /* Notice that it's ::buttons(), not ::button(). */
	case Qt::LeftButton: {
		if (!tool->can_tool_move_object()) {
			qDebug() << SG_PREFIX_E << "Not moving, tool can't move object";
			return ToolStatus::Error;
		}

		qDebug() << SG_PREFIX_I << "Will now set new position of trackpoint";
		Coord new_coord = gisview->screen_pos_to_coord(ev->x(), ev->y());
		trw->get_nearby_snap_coordinates(new_coord, ev, gisview, track->m_type_id);
		track->selected_tp_set_coord(new_coord, false);
		return ToolStatus::Ack;
	}
	default:
		return ToolStatus::Ignored;
	}
}




ToolStatus helper_release_tp(LayerTRW * trw, LayerToolSelect * tool, QMouseEvent * ev, GisViewport * gisview)
{
	if (ev->button() != Qt::LeftButton) {
		return ToolStatus::Ignored;
	}
	Track * track = trw->selected_track_get(); /* This is the track, to which belongs the edited trackpoint. TODO: how can we be sure that a trackpoint is selected? */
	if (nullptr == track) {
		qDebug() << SG_PREFIX_E << "Will reset trackpoint properties dialog data, no track";
		/* Well, there was no track that was edited, so nothing to do here. */
		return ToolStatus::Ignored;
	}

	if (LayerToolSelect::ObjectState::IsHeld != tool->edited_object_state) {
		/* We can't release what hasn't been held. */
		return ToolStatus::Ignored;
	}

	Coord new_coord = gisview->screen_pos_to_coord(ev->x(), ev->y());
	trw->get_nearby_snap_coordinates(new_coord, ev, gisview, track->m_type_id);
	track->selected_tp_set_coord(new_coord, true);

	/* Object is released by tool (so its position no
	   longer changes when cursor moves), but is still a
	   selected item. */
	qDebug() << SG_PREFIX_I << "Setting edited object state to IsSelected";
	tool->edited_object_state = LayerToolSelect::ObjectState::IsSelected;
	/* Not needed, type id stays the same when transitioning from IsHeld to IsSelected.
	   this->selected_tree_item_type_id = xxx;
	*/

	return ToolStatus::Ack;
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
	if (this->m_kind != LayerKind::TRW) {
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
	const bool waypoints_inside = this->waypoints.get_bbox().intersects_with(viewport_bbox);
	qDebug() << SG_PREFIX_I << "Waypoints are" << (waypoints_visible ? "visible" : "invisible") << "and" << (waypoints_inside ? "inside" : "outside") << "of viewport";
	if (waypoints_visible && waypoints_inside) {
		WaypointSearch wp_search(ev->x(), ev->y(), gisview);
		if (true == this->try_clicking_waypoint(ev, wp_search, select_tool)) {
			if (ev->type() == QEvent::MouseButtonDblClick
			    /* flags() & Qt::MouseEventCreatedDoubleClick */) {
				qDebug() << SG_PREFIX_D << "Selected waypoint through double click";

				if (!wp_search.closest_wp->image_full_path.isEmpty()) {
					this->show_wp_picture_cb();
				}
			}
			this->emit_tree_item_changed("TRW layer changed after selecting wp with 'click'");
			return true;
		}
	}

	if (true == this->try_clicking_track_or_route_trackpoint(ev, viewport_bbox, gisview, select_tool)) {
		return true;
	}


	/* Mouse click didn't happen anywhere near a Trackpoint or Waypoint from this layer.
	   So unmark/deselect all "current"/"edited" elements of this layer. */
	qDebug() << SG_PREFIX_I << "Mouse click for Select tool didn't click any data, resetting info";
	/* At this abstraction layer we only have to call these two
	   methods. Everything else below this abstraction layer will
	   be handled by the two methods. */
	bool was_selected = false;
	was_selected = was_selected || this->selected_wp_reset();
	was_selected = was_selected || this->selected_track_reset();

	qDebug() << SG_PREFIX_I << "Will set edited object state to NotSelected";
	select_tool->edited_object_state = LayerToolSelect::ObjectState::NotSelected;
	select_tool->selected_tree_item_type_id = SGObjectTypeID();

	/* Erase info. */
	this->get_window()->get_statusbar()->set_message(StatusBarField::Info, "");

	if (was_selected) {
		/* Some item was selected, but after resetting of all
		   selections in this layer the item got
		   deselected. */
		this->emit_tree_item_changed("TRW layer changed after no object was hit with 'click'");
	}

	return false;
}




bool LayerTRW::try_clicking_track_or_route_trackpoint(QMouseEvent * ev, const LatLonBBox & viewport_bbox, GisViewport * gisview, LayerToolSelect * select_tool)
{
	/* First try for tracks. */
	const bool tracks_visible = this->get_tracks_node().is_visible();
	const bool tracks_inside = this->tracks.get_bbox().intersects_with(viewport_bbox);
	qDebug() << SG_PREFIX_I << "Tracks are" << (tracks_visible ? "visible" : "invisible") << "and" << (tracks_inside ? "inside" : "outside") << "of viewport";
	if (tracks_visible && tracks_inside) {
		TrackpointSearch tp_search(ev->x(), ev->y(), gisview);
		if (true == this->try_clicking_trackpoint(ev, tp_search, this->get_tracks_node(), select_tool)) {
			this->emit_tree_item_changed("TRW layer changed after selecting tp in track with 'click'");
			return true;
		}
	}


	/* Try again for routes. */
	const bool routes_visible = this->routes.is_visible();
	const bool routes_inside = this->routes.get_bbox().intersects_with(viewport_bbox);
	qDebug() << SG_PREFIX_I << "Routes are" << (routes_visible ? "visible" : "invisible") << "and" << (routes_inside ? "inside" : "outside") << "of viewport";
	if (routes_visible && routes_inside) {
		TrackpointSearch tp_search(ev->x(), ev->y(), gisview);
		if (true == this->try_clicking_trackpoint(ev, tp_search, this->get_routes_node(), select_tool)) {
			this->emit_tree_item_changed("TRW layer changed after selecting tp in route with 'click'");
			return true;
		}
	}


	return false;
}




/**
   This method is for handling "move" event coming from generic Select
   tool.

   The generic Select tool doesn't know how to implement
   layer-specific movement, so the layer has to implement the
   behaviour itself.
*/
bool LayerTRW::handle_select_tool_move(QMouseEvent * ev, GisViewport * gisview, LayerToolSelect * select_tool)
{
	if (select_tool->selected_tree_item_type_id == Waypoint::type_id()) {
		return ToolStatus::Ack == helper_move_wp(this, select_tool, ev, select_tool->gisview);

	} else if (select_tool->selected_tree_item_type_id == Track::type_id()
		   || select_tool->selected_tree_item_type_id == Route::type_id()) {
		return ToolStatus::Ack == helper_move_tp(this, select_tool, ev, select_tool->gisview);

	} else {
		qDebug() << SG_PREFIX_E << "Not moving due to unknown type id" << select_tool->selected_tree_item_type_id;
		return false;
	}
}




/**
   This method is for handling "release" event coming from generic
   Select tool.

   The generic Select tool doesn't know how to implement
   layer-specific release, so the layer has to implement the behaviour
   itself.
*/
bool LayerTRW::handle_select_tool_release(QMouseEvent * ev, GisViewport * gisview, LayerToolSelect * select_tool)
{
	if (select_tool->selected_tree_item_type_id == Waypoint::type_id()) {
		return ToolStatus::Ack == helper_release_wp(this, select_tool, ev, gisview);

	} else if (select_tool->selected_tree_item_type_id == Track::type_id()
		   || select_tool->selected_tree_item_type_id == Route::type_id()) {
		return ToolStatus::Ack == helper_release_tp(this, select_tool, ev, gisview);

	} else {
		qDebug() << SG_PREFIX_E << "Not releasing due to unknown type id" << select_tool->selected_tree_item_type_id;
		return false;
	}
}




bool LayerTRW::try_clicking_waypoint(QMouseEvent * ev, WaypointSearch & wp_search, LayerToolSelect * tool)
{
	this->waypoints.search_closest_wp(wp_search);
	if (NULL == wp_search.closest_wp) {
		tool->edited_object_state = LayerToolSelect::ObjectState::NotSelected;
		tool->selected_tree_item_type_id = SGObjectTypeID();
		qDebug() << SG_PREFIX_I << "No waypoint clicked";
		return false;
	}

	qDebug() << SG_PREFIX_I << wp_search.closest_wp->name << "waypoint clicked";


	/* Be sure to execute this section before setting currently
	   selected wp in current track below */
	if (this->can_start_moving_wp_on_click(ev, wp_search.closest_wp)) {
		tool->edited_object_state = LayerToolSelect::ObjectState::IsHeld;
	} else {
		/*
		  For now assign ::IsSelected. ::IsHeld state will be
		  assigned during next click event (if/because the
		  point will be already selected).
		*/
		tool->edited_object_state = LayerToolSelect::ObjectState::IsSelected;
	}
	tool->selected_tree_item_type_id = wp_search.closest_wp->m_type_id;


	LayerTRW * trw = (LayerTRW *) wp_search.closest_wp->get_owning_layer();
	trw->selected_wp_set(wp_search.closest_wp);
	wp_search.closest_wp->click_in_tree("Waypoint has been selected with 'select tool' click");

	return true;
}




bool LayerTRW::try_clicking_trackpoint(QMouseEvent * ev, TrackpointSearch & tp_search, LayerTRWTracks & tracks_or_routes, LayerToolSelect * tool)
{
	tracks_or_routes.track_search_closest_tp(tp_search);
	if (NULL == tp_search.closest_tp) {
		tool->edited_object_state = LayerToolSelect::ObjectState::NotSelected;
		tool->selected_tree_item_type_id = SGObjectTypeID();
		qDebug() << SG_PREFIX_I << "No trackpoint clicked";
		return false;
	}
	const bool is_routes = tracks_or_routes.get_type_id() == LayerTRWRoutes::type_id();
	qDebug() << SG_PREFIX_I << "Clicked trackpoint in" << (is_routes ? "route" : "track") << tp_search.closest_track->name;


	/* Be sure to execute this section before setting currently
	   selected tp in current track below */
	if (this->can_start_moving_tp_on_click(ev, tp_search.closest_track, tp_search.closest_tp_iter)) {
		qDebug() << SG_PREFIX_I << "Will set edited object state to IsHeld";
		tool->edited_object_state = LayerToolSelect::ObjectState::IsHeld;
	} else {
		/*
		  For now assign ::IsSelected. ::IsHeld state will be
		  assigned during next click event (if/because the
		  point will be already selected).
		*/
		qDebug() << SG_PREFIX_I << "Will set edited object state to IsSelected";
		tool->edited_object_state = LayerToolSelect::ObjectState::IsSelected;
	}
	tool->selected_tree_item_type_id = tp_search.closest_track->m_type_id;


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




bool LayerTRW::can_start_moving_tp_on_click(QMouseEvent * ev, Track * track, TrackPoints::iterator & tp_iter)
{
	/*
	  Can move the trackpoint immediately when
	  TRACKPOINT_MODIFIER_KEY key is held or it's the previously
	  selected tp.
	*/
	if (ev->modifiers() & TRACKPOINT_MODIFIER_KEY) {
		qDebug() << SG_PREFIX_I << "Can start moving due to event modifiers";
		return true;
	}

	const bool tp_is_already_selected = track->is_selected()
		&& 1 == track->get_selected_children().get_count()
		&& track->get_selected_children().is_member(*tp_iter);

	if (tp_is_already_selected) {
		qDebug() << SG_PREFIX_I << "Can start moving due to preselection of tp";
		return true;
	}

	qDebug() << SG_PREFIX_I << "Can't start moving";
	return false;
}




bool LayerTRW::can_start_moving_wp_on_click(QMouseEvent * ev, Waypoint * wp)
{
	/*
	  Can move the waypoint immediately when WAYPOINT_MODIFIER_KEY
	  key is held or it's the previously selected tp.
	*/
	if (ev->modifiers() & WAYPOINT_MODIFIER_KEY) {
		qDebug() << SG_PREFIX_I << "Can start moving due to event modifiers";
		return true;
	}

	const bool wp_is_already_selected = this->selected_wp_get() == wp;

	if (wp_is_already_selected) {
		qDebug() << SG_PREFIX_I << "Can start moving due to preselection of tp";
		return true;
	}

	qDebug() << SG_PREFIX_I << "Can't start moving";
	return false;
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

	if (this->m_kind != LayerKind::TRW) {
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




LayerToolTRWEditWaypoint::LayerToolTRWEditWaypoint(Window * window_, GisViewport * gisview_) : LayerToolSelect(window_, gisview_, LayerKind::TRW)
{
	this->action_icon_path   = ":/icons/layer_tool/trw_edit_wp_18.png";
	this->action_label       = QObject::tr("&Edit Waypoint");
	this->action_tooltip     = QObject::tr("Edit Waypoint");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_E;

	this->cursor_click = QCursor(QPixmap(":/cursors/trw_edit_wp.png"), 0, 0);
	this->cursor_release = QCursor(QPixmap(":/cursors/trw_edit_wp.png"), 0, 0);

	/* One Waypoint Properties Dialog for all layers. */
	this->point_properties_dialog = new WpPropertiesDialog(gisview_->get_coord_mode(), window_);
}




LayerToolTRWEditWaypoint::~LayerToolTRWEditWaypoint()
{
	delete this->point_properties_dialog;
}




SGObjectTypeID LayerToolTRWEditWaypoint::get_tool_id(void) const
{
	return LayerToolTRWEditWaypoint::tool_id();
}
SGObjectTypeID LayerToolTRWEditWaypoint::tool_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.tool.layer_trw.edit_waypoint");
	return id;
}




ToolStatus LayerToolTRWEditWaypoint::internal_handle_mouse_click(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (trw->m_kind != LayerKind::TRW) {
		qDebug() << SG_PREFIX_D << "Not TRW layer";
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
		this->gisview->coord_to_screen_pos(current_wp->get_coord(), wp_pos);

		const ScreenPos event_pos = ScreenPos(ev->x(), ev->y());

		if (ScreenPos::are_closer_than(wp_pos, event_pos, WAYPOINT_SIZE_APPROX)) {
			qDebug() << SG_PREFIX_I << "Will set edited object state to IsHeld";
			this->edited_object_state = LayerToolSelect::ObjectState::IsHeld;
			this->selected_tree_item_type_id = current_wp->m_type_id;

			/* Layer's selected waypoint now becomes
			   tool's selected waypoint. */
			newly_selected_wp = current_wp;
		}
	}

	if (!newly_selected_wp) {
		/* Either there is no globally selected waypoint, or
		   it was too far from click. Either way the tool
		   doesn't have any waypoint to operate on - yet. Try
		   to find one close to click position. */
		WaypointSearch wp_search(ev->x(), ev->y(), gisview);
		if (true == trw->try_clicking_waypoint(ev, wp_search, this)) {
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
		const bool some_object_was_released = this->edited_object_state != LayerToolSelect::ObjectState::NotSelected;

		/* We clicked on empty space, so no waypoint is selected. */
		qDebug() << SG_PREFIX_I << "Setting edited object state to NotSelected";
		this->edited_object_state = LayerToolSelect::ObjectState::NotSelected;
		this->selected_tree_item_type_id = SGObjectTypeID();

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
	if (layer->m_kind != LayerKind::TRW) {
		qDebug() << SG_PREFIX_E << "Expected TRW layer passed to TRW tool, got" << layer->m_kind;
		return ToolStatus::Ignored;
	}

	return helper_move_wp((LayerTRW *) layer, this, ev, this->gisview);
}




ToolStatus LayerToolTRWEditWaypoint::internal_handle_mouse_release(Layer * layer, QMouseEvent * ev)
{
	if (layer->m_kind != LayerKind::TRW) {
		qDebug() << SG_PREFIX_E << "Expected TRW layer passed to TRW tool, got" << layer->m_kind;
		return ToolStatus::Ignored;
	}

	return helper_release_wp((LayerTRW *) layer, this, ev, this->gisview);
}




/**
   @brief Set coordinates mode of a "waypoint properties" dialog

   The dialog contains coordinates widget, displaying coordinates of a
   waypoint. Depending on coordinates mode selected in main window,
   the widget should change its appearance (to display Lat/Lon or UTM
   coordinates).

   @reviewed on 2019-11-11
*/
sg_ret LayerToolTRWEditWaypoint::change_coord_mode(CoordMode coord_mode)
{
	if (nullptr == this->point_properties_dialog) {
		qDebug() << SG_PREFIX_W << "Waypoint properties dialog widget not constructed yet";
		return sg_ret::err_null_ptr;
	}

	return this->point_properties_dialog->set_coord_mode(coord_mode);
}




/*
  See if mouse event @param ev happened close to a Trackpoint or a
  Waypoint and if keyboard modifier specific for Trackpoint or
  Waypoint was used. If both conditions are true, put coordinates of
  such Trackpoint or Waypoint in @param point_coord.

  @reviewed-on: TBD
*/
bool LayerTRW::get_nearby_snap_coordinates(Coord & point_coord, QMouseEvent * ev, GisViewport * gisview, const SGObjectTypeID & selected_object_type_id)
{
	/* Search close trackpoint in tracks AND routes. */
	if (ev->modifiers() & TRACKPOINT_MODIFIER_KEY) {
		TrackpointSearch tp_search(ev->x(), ev->y(), gisview);

		if (selected_object_type_id == Track::type_id()
		    || selected_object_type_id == Route::type_id()) {
			/* We are searching for snap coordinates for
			   trackpoint. Tell search tool to ignore
			   coordinates of currently selected
			   trackpoint, otherwise the search tool will
			   always return coordinates of currently
			   selected trackpoint. The trackpoint will be
			   told to always snap to itself. In viewport
			   it will be visible as trackpoint not moving
			   smoothly but jumping from one "self"
			   position to another. */

			/* NOTE: if there are more selected tracks or trackpoints, this will get messier. */
			Track * track = this->selected_track_get();
			if (track && 1 == track->get_selected_children().get_count()) {
				const TrackpointReference & tp_ref = track->get_selected_children().front();
				if (tp_ref.m_iter_valid) {
					tp_search.skip_tp = *tp_ref.m_iter;
				}
			}
		}

		this->tracks.track_search_closest_tp(tp_search);
		if (NULL != tp_search.closest_tp) {
			point_coord = tp_search.closest_tp->coord;
			return true;
		}
		this->routes.track_search_closest_tp(tp_search);
		if (NULL != tp_search.closest_tp) {
			point_coord = tp_search.closest_tp->coord;
			return true;
		}
		/* Else go to searching for waypoint to snap to. */
	}

	/* Search close waypoint. */
	if (ev->modifiers() & WAYPOINT_MODIFIER_KEY) {
		WaypointSearch wp_search(ev->x(), ev->y(), gisview);
		if (selected_object_type_id == Waypoint::type_id()) {
			/* We are searching for snap coordinates for
			   waypoint. Tell search tool to ignore
			   coordinates of currently selected waypoint,
			   otherwise the search tool will always
			   return coordinates of currently selected
			   waypoint. The waypoint will be told to
			   always snap to itself. In viewport it will
			   be visible as waypoint not moving smoothly
			   but jumping from one "self" position to
			   another. */

			/* NOTE: if there are more selected waypoints, this will get messier. */
			wp_search.skip_wp = this->selected_wp_get(); /* May be NULL. */
		}
		this->waypoints.search_closest_wp(wp_search);

		if (NULL != wp_search.closest_wp) {
			point_coord = wp_search.closest_wp->get_coord();
			return true;
		}
	}

	return false;
}




LayerToolTRWNewTrack::LayerToolTRWNewTrack(Window * window_, GisViewport * gisview_) : LayerTool(window_, gisview_, LayerKind::TRW)
{
	this->is_route_tool = false;

	this->action_icon_path   = ":/icons/layer_tool/trw_add_tr_18.png";
	this->action_label       = QObject::tr("Create &Track");
	this->action_tooltip     = QObject::tr("Create Track");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_T;

	this->pan_handler = true;  /* Still need to handle clicks when in PAN mode to disable the potential trackpoint drawing. */
	this->cursor_click = QCursor(QPixmap(":/cursors/trw_add_tr.png"), 0, 0);
	this->cursor_release = QCursor(QPixmap(":/cursors/trw_add_tr.png"), 0, 0);
}




LayerToolTRWNewRoute::LayerToolTRWNewRoute(Window * window_, GisViewport * gisview_) : LayerToolTRWNewTrack(window_, gisview_)
{
	this->is_route_tool = true;

	this->action_icon_path   = ":/icons/layer_tool/trw_add_route_18.png";
	this->action_label       = QObject::tr("Create &Route");
	this->action_tooltip     = QObject::tr("Create Route");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_B;

	this->pan_handler = true;  /* Still need to handle clicks when in PAN mode to disable the potential trackpoint drawing. */
	this->cursor_click = QCursor(QPixmap(":/cursors/trw_add_route.png"), 0, 0);
	this->cursor_release = QCursor(QPixmap(":/cursors/trw_add_route.png"), 0, 0);
}





SGObjectTypeID LayerToolTRWNewTrack::get_tool_id(void) const
{
	return LayerToolTRWNewTrack::tool_id();
}
SGObjectTypeID LayerToolTRWNewTrack::tool_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.tool.layer_trw.create_track");
	return id;
}

SGObjectTypeID LayerToolTRWNewRoute::get_tool_id(void) const
{
	return LayerToolTRWNewRoute::tool_id();
}
SGObjectTypeID LayerToolTRWNewRoute::tool_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.tool.layer_trw.create_route");
	return id;
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
	if (this->get_tool_id() == LayerToolTRWNewTrack::tool_id() && !this->creation_in_progress) {
		/* Track is not being created at the moment, so a Key can't affect work of this tool. */
		return ToolStatus::Ignored;
	}
	if (this->get_tool_id() == LayerToolTRWNewRoute::tool_id() && !this->creation_in_progress) {
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

	/* Maybe snap to other point. */
	trw->get_nearby_snap_coordinates(tp->coord, ev, gisview, track->m_type_id);

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
			new_name = trw->new_unique_element_name(Route::type_id(), QObject::tr("Route"));
		} else {
			new_name = trw->new_unique_element_name(Track::type_id(), QObject::tr("Track"));
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
	if (trw->m_kind != LayerKind::TRW) {
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




LayerToolTRWNewWaypoint::LayerToolTRWNewWaypoint(Window * window_, GisViewport * gisview_) : LayerTool(window_, gisview_, LayerKind::TRW)
{
	this->action_icon_path   = ":/icons/layer_tool/trw_add_wp_18.png";
	this->action_label       = QObject::tr("Create &Waypoint");
	this->action_tooltip     = QObject::tr("Create Waypoint");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_W;

	this->cursor_click = QCursor(QPixmap(":/cursors/trw_add_wp.png"), 0, 0);
	this->cursor_release = QCursor(QPixmap(":/cursors/trw_add_wp.png"), 0, 0);
}




SGObjectTypeID LayerToolTRWNewWaypoint::get_tool_id(void) const
{
	return LayerToolTRWNewWaypoint::tool_id();
}
SGObjectTypeID LayerToolTRWNewWaypoint::tool_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.tool.layer_trw.create_waypoint");
	return id;
}




ToolStatus LayerToolTRWNewWaypoint::internal_handle_mouse_click(Layer * layer, QMouseEvent * ev)
{
	LayerTRW * trw = (LayerTRW *) layer;

	if (trw->m_kind != LayerKind::TRW) {
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




LayerToolTRWEditTrackpoint::LayerToolTRWEditTrackpoint(Window * window_, GisViewport * gisview_) : LayerToolSelect(window_, gisview_, LayerKind::TRW)
{
	this->action_icon_path   = ":/icons/layer_tool/trw_edit_tr_18.png";
	this->action_label       = QObject::tr("Edit Trac&kpoint");
	this->action_tooltip     = QObject::tr("Edit Trackpoint");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_K;

	this->cursor_click = QCursor(QPixmap(":/cursors/trw_edit_tr.png"), 0, 0);
	this->cursor_release = QCursor(QPixmap(":/cursors/trw_edit_tr.png"), 0, 0);

	/* One Trackpoint Properties Dialog for all layers. */
	this->point_properties_dialog = new TpPropertiesDialog(gisview_->get_coord_mode(), window_);
}




LayerToolTRWEditTrackpoint::~LayerToolTRWEditTrackpoint()
{
	delete this->point_properties_dialog;
}




SGObjectTypeID LayerToolTRWEditTrackpoint::get_tool_id(void) const
{
	return LayerToolTRWEditTrackpoint::tool_id();
}
SGObjectTypeID LayerToolTRWEditTrackpoint::tool_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.tool.layer_trw.edit_trackpoint");
	return id;
}




/**
   On 'initial' click: search for the nearest trackpoint or routepoint
   and store it as the current trackpoint.

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

	if (trw->m_kind != LayerKind::TRW) {
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
				qDebug() << SG_PREFIX_I << "Will set edited object state to IsHeld";
				this->edited_object_state = LayerToolSelect::ObjectState::IsHeld;
				this->selected_tree_item_type_id = track->m_type_id;
				return ToolStatus::Ack;
			}
		} else {
			qDebug() << SG_PREFIX_E << "Invalid tp reference";
		}
	}


	if (true == trw->try_clicking_track_or_route_trackpoint(ev, gisview->get_bbox(), this->gisview, this)) {
		return ToolStatus::Ack;
	}


	/* The mouse click wasn't near enough any Trackpoint that belongs to any tracks/routes in this layer. */
	return ToolStatus::Ignored;
}




ToolStatus LayerToolTRWEditTrackpoint::internal_handle_mouse_move(Layer * layer, QMouseEvent * ev)
{
	if (layer->m_kind != LayerKind::TRW) {
		qDebug() << SG_PREFIX_E << "Expected TRW layer passed to TRW tool, got" << layer->m_kind;
		return ToolStatus::Ignored;
	}

	return helper_move_tp((LayerTRW *) layer, this, ev, this->gisview);
}




ToolStatus LayerToolTRWEditTrackpoint::internal_handle_mouse_release(Layer * layer, QMouseEvent * ev)
{
	if (layer->m_kind != LayerKind::TRW) {
		qDebug() << SG_PREFIX_E << "Expected TRW layer passed to TRW tool, got" << layer->m_kind;
		return ToolStatus::Ignored;
	}

	return helper_release_tp((LayerTRW *) layer, this, ev, this->gisview);
}




/**
   @brief Set coordinates mode of a "waypoint properties" dialog

   The dialog contains coordinates widget, displaying coordinates of a
   waypoint. Depending on coordinates mode selected in main window,
   the widget should change its appearance (to display Lat/Lon or UTM
   coordinates).

   @reviewed on 2019-11-11
*/
sg_ret LayerToolTRWEditTrackpoint::change_coord_mode(CoordMode coord_mode)
{
	if (nullptr == this->point_properties_dialog) {
		qDebug() << SG_PREFIX_W << "Trackpoint properties dialog widget not constructed yet";
		return sg_ret::err_null_ptr;
	}

	return this->point_properties_dialog->set_coord_mode(coord_mode);
}




LayerToolTRWExtendedRouteFinder::LayerToolTRWExtendedRouteFinder(Window * window_, GisViewport * gisview_) : LayerTool(window_, gisview_, LayerKind::TRW)
{
	this->action_icon_path   = ":/icons/layer_tool/trw_find_route_18.png";
	this->action_label       = QObject::tr("Route &Finder");
	this->action_tooltip     = QObject::tr("Route Finder");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_F;

	this->pan_handler = true;  /* Still need to handle clicks when in PAN mode to disable the potential trackpoint drawing. */

	this->cursor_click = QCursor(QPixmap(":/cursors/trw____.png"), 0, 0);
	this->cursor_release = QCursor(Qt::ArrowCursor);
}




SGObjectTypeID LayerToolTRWExtendedRouteFinder::get_tool_id(void) const
{
	return LayerToolTRWExtendedRouteFinder::tool_id();
}
SGObjectTypeID LayerToolTRWExtendedRouteFinder::tool_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.tool.layer_trw.route_finder");
	return id;
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

		LayerTool * new_route_tool = trw->get_window()->get_toolbox()->get_tool(LayerToolTRWNewRoute::tool_id());
		if (NULL == new_route_tool) {
			qDebug() << SG_PREFIX_E << "Failed to get tool with id =" << LayerToolTRWNewRoute::tool_id();
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




LayerToolTRWShowPicture::LayerToolTRWShowPicture(Window * window_, GisViewport * gisview_) : LayerTool(window_, gisview_, LayerKind::TRW)
{
	this->action_icon_path   = ":/icons/layer_tool/trw_show_picture_18.png";
	this->action_label       = QObject::tr("Show P&icture");
	this->action_tooltip     = QObject::tr("Show Picture");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_I;

	this->cursor_click = QCursor(QPixmap(":/cursors/trw____.png"), 0, 0);
	this->cursor_release = QCursor(Qt::ArrowCursor);
}




SGObjectTypeID LayerToolTRWShowPicture::get_tool_id(void) const
{
	return LayerToolTRWShowPicture::tool_id();
}
SGObjectTypeID LayerToolTRWShowPicture::tool_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.tool.layer_trw.show_picture");
	return id;
}




ToolStatus LayerToolTRWShowPicture::internal_handle_mouse_click(Layer * layer, QMouseEvent * ev)
{
	if (layer->m_kind != LayerKind::TRW) {
		qDebug() << SG_PREFIX_E << "Expected TRW layer passed to TRW tool, got" << layer->m_kind;
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
