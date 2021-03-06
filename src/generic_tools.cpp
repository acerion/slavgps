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




#include <QPainter>
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
#include "statusbar.h"
#include "ruler.h"




using namespace SlavGPS;




#define SG_MODULE "Generic Tools"




#ifdef WINDOWS
#define SG_MOVE_MODIFIER Qt::AltModifier
#else
/* Alt+mouse on Linux desktops tends to be used by the desktop manager.
   Thus use an alternate modifier - you may need to set something into this group.
   Viking used GDK_MOD5_MASK. */
#define SG_MOVE_MODIFIER Qt::ControlModifier
#endif




/**
   @reviewed-on 2019-12-01
*/
LayerToolContainer GenericTools::create_tools(Window * window, GisViewport * gisview)
{
	LayerToolContainer tools;

	/* This method should be called only once. */
	static bool created = false;
	if (created) {
		return tools;
	}

	LayerTool * tool = nullptr;

	/* I'm using assertions to verify that tools have unique IDs
	   (at least unique within a group of tools). */

	tool = new LayerToolSelect(window, gisview);
	auto status = tools.insert({ tool->get_tool_id(), tool });
	assert(status.second);

	tool = new GenericToolRuler(window, gisview);
	status = tools.insert({ tool->get_tool_id(), tool });
	assert(status.second);

	tool = new GenericToolZoom(window, gisview);
	status = tools.insert({ tool->get_tool_id(), tool });
	assert(status.second);

	tool = new LayerToolPan(window, gisview);
	status = tools.insert({ tool->get_tool_id(), tool });
	assert(status.second);

	created = true;

	return tools;
}




GenericToolRuler::GenericToolRuler(Window * window_, GisViewport * gisview_) : LayerTool(window_, gisview_, LayerKind::Max)
{
	this->action_icon_path   = ":/icons/layer_tool/ruler_18.png";
	this->action_label       = QObject::tr("&Ruler");
	this->action_tooltip     = QObject::tr("Ruler Tool");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_U; /* Ctrl+Shift+R is used for Refresh (deemed more important), so use 'U' instead. */
}




GenericToolRuler::~GenericToolRuler()
{
	delete this->ruler;
}




SGObjectTypeID GenericToolRuler::get_tool_id(void) const
{
	return GenericToolRuler::tool_id();
}
SGObjectTypeID GenericToolRuler::tool_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.tool.generic.ruler");
	return id;
}




LayerTool::Status GenericToolRuler::handle_mouse_click(__attribute__((unused)) Layer * layer, QMouseEvent * event)
{
	qDebug() << SG_PREFIX_D << "called";


	if (event->button() != Qt::LeftButton) {
		return LayerTool::Status::Ignored;
	}

	QString msg;

	if (this->ruler) {
		qDebug() << SG_PREFIX_I << "second click, resetting ruler";

		msg = this->ruler->get_msg();

		this->reset_ruler();
	} else {
		qDebug() << SG_PREFIX_I << "first click, starting ruler";

		const Coord cursor_coord = this->gisview->screen_pos_to_coord(event->localPos());
		if (!cursor_coord.is_valid()) {
			qDebug() << SG_PREFIX_E << "Failed to get valid coordinate";
			return LayerTool::Status::Error;
		}
		msg = cursor_coord.to_string();

		/* Save clean viewport (clean == without ruler drawn on top of it). */
		this->orig_viewport_pixmap = this->gisview->get_pixmap();

		this->ruler = new Ruler(this->gisview, Preferences::get_unit_distance());
		this->ruler->set_begin(event->localPos());
	}

	this->window->statusbar()->set_message(StatusBarField::Info, msg);

	return LayerTool::Status::Handled;
}




LayerTool::Status GenericToolRuler::handle_mouse_move(__attribute__((unused)) Layer * layer, QMouseEvent * event)
{
	qDebug() << SG_PREFIX_D << "Called";

	if (!this->ruler) {
		/* Ruler tool may be selected, but there was no first
		   click that would establish beginning of the
		   ruler. Mouse move event does not influence the
		   ruler, because the ruler doesn't exist yet. */
		return LayerTool::Status::Ignored;
	}


	/* Redraw ruler that goes from place of initial click
	   (remembered by ruler) to place where mouse cursor is
	   now. */
	QPixmap marked_pixmap = this->orig_viewport_pixmap;
	QPainter painter(&marked_pixmap);
	this->ruler->set_end(event->localPos());
	this->ruler->paint_ruler(painter, Preferences::get_create_track_tooltip());
	this->gisview->set_pixmap(marked_pixmap);
	/* This will call GisViewport::paintEvent(), triggering final render to screen. */
	this->gisview->update();


	const QString msg = ruler->get_msg();
	this->window->statusbar()->set_message(StatusBarField::Info, msg);


	return LayerTool::Status::Handled;
}




LayerTool::Status GenericToolRuler::handle_mouse_release(__attribute__((unused)) Layer * layer, __attribute__((unused)) QMouseEvent * event)
{
	qDebug() << SG_PREFIX_I << "called";

	return LayerTool::Status::Ignored;
}




bool GenericToolRuler::deactivate_tool(void)
{
	qDebug() << SG_PREFIX_I << "called";

	this->window->draw_tree_items(this->gisview);

	return true;
}




LayerTool::Status GenericToolRuler::handle_key_press(__attribute__((unused)) Layer * layer, QKeyEvent * event)
{
	qDebug() << SG_PREFIX_D << "called";

	if (event->key() == Qt::Key_Escape) {
		this->reset_ruler();
		this->deactivate_tool();
		return LayerTool::Status::Handled;
	} else {
		return LayerTool::Status::Ignored;
	}
}




void GenericToolRuler::reset_ruler(void)
{
	delete this->ruler;
	this->ruler = NULL;

	if (this->orig_viewport_pixmap.isNull()) {
		qDebug() << SG_PREFIX_W << "Detected NULL orig viewport pixmap";
	} else {
		/* Restore clean viewport (clean == without ruler drawn on top of it). */
		this->gisview->set_pixmap(this->orig_viewport_pixmap);
		this->orig_viewport_pixmap = QPixmap();

		/* This will call GisViewport::paintEvent(), triggering final render to screen. */
		this->gisview->update();
	}
}




GenericToolZoom::GenericToolZoom(Window * window_, GisViewport * gisview_) : LayerTool(window_, gisview_, LayerKind::Max)
{
	this->action_icon_path   = ":/icons/layer_tool/zoom_18.png";
	this->action_label       = QObject::tr("&Zoom");
	this->action_tooltip     = QObject::tr("Zoom Tool");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_Z;
}




GenericToolZoom::~GenericToolZoom()
{
}




SGObjectTypeID GenericToolZoom::get_tool_id(void) const
{
	return GenericToolZoom::tool_id();
}
SGObjectTypeID GenericToolZoom::tool_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.tool.generic.zoom");
	return id;
}




LayerTool::Status GenericToolZoom::handle_mouse_click(__attribute__((unused)) Layer * layer, QMouseEvent * event)
{
	qDebug() << SG_PREFIX_D << "Called";

	const Qt::KeyboardModifiers modifiers = event->modifiers();

	const ScreenPos center_pos = this->gisview->central_get_center_screen_pos();
	const ScreenPos event_pos(event->x(), event->y());

	/* Did the zoom operation affect viewport? */
	bool redraw_viewport = false;

	this->ztr.abort(this->gisview); /* Reset, just int case. */

	const ZoomDirection zoom_direction = mouse_event_to_zoom_direction(event);

	switch (modifiers) {
	case (Qt::ControlModifier | Qt::ShiftModifier):
		/* Location at the center of viewport will be
		   preserved (coordinate at the center before the zoom
		   and coordinate at the center after the zoom will be
		   the same). */
		redraw_viewport = this->gisview->zoom_with_preserving_center_coord(zoom_direction);
		if (redraw_viewport) {
			this->window->set_dirty_flag(true);
		}
		break;

	case Qt::ControlModifier:
		/* Clicked location will be put at the center of
		   viewport (coordinate of a place under cursor before
		   zoom will be placed at the center of viewport after
		   zoom). */
		redraw_viewport = this->gisview->zoom_with_setting_new_center(zoom_direction, event_pos);
		if (redraw_viewport) {
			this->window->set_dirty_flag(true);
		}
		break;

	case Qt::ShiftModifier:
		/* Beginning of "zoom in to rectangle" operation.
		   Notice that there is no "zoom out to rectangle"
		   operation. Get start position of zoom bounds. */

		switch (zoom_direction) {
		case ZoomDirection::In:
			this->ztr.begin(gisview, event_pos);
			break;
		default:
			break;
		};

		/* No zoom action (yet), so no redrawing of viewport. */
		break;

	case Qt::NoModifier:
		/* Clicked coordinate will be put after zoom at the same
		   position in viewport as before zoom.  Before zoom
		   the coordinate was under cursor, and after zoom it
		   will be still under cursor. */
		redraw_viewport = this->gisview->zoom_keep_coordinate_under_cursor(zoom_direction, event_pos, center_pos);
		if (redraw_viewport) {
			this->window->set_dirty_flag(true);
		}
		break;

	default:
		/* Other modifier. Just ignore. */
		break;
	};

	if (redraw_viewport) {
		this->window->draw_tree_items(this->gisview);
	}


	return LayerTool::Status::Handled;
}




LayerTool::Status GenericToolZoom::handle_mouse_move(__attribute__((unused)) Layer * layer, QMouseEvent * event)
{
	qDebug() << SG_PREFIX_D << "Called";

	if (!this->ztr.is_active()) {
		qDebug() << SG_PREFIX_I << "ZTR is NOT active";
		return LayerTool::Status::Ignored;
	}
	qDebug() << SG_PREFIX_I << "ZTR is active";

	const unsigned int modifiers = event->modifiers() & Qt::ShiftModifier;
	if (modifiers != Qt::ShiftModifier) {

		/* When user initially pressed left mouse button, he
		   also held down Shift key.  This initiated drawing a
		   "zoom to rectangle" box/bound (zoom-to-rectangle
		   became active).

		   Right now Shift key is released, so stop drawing
		   the box and abort "zoom to rectangle" procedure. */

		qDebug() << SG_PREFIX_I << "ZTR is active without Shift key, resetting it";
		this->ztr.abort(this->gisview); /* Stop drawing a rectangle. */
		return LayerTool::Status::Handled;
	}

	qDebug() << SG_PREFIX_I << "ZTR is active, drawing rectangle";
	this->ztr.draw_rectangle(this->gisview, event->localPos());
	return LayerTool::Status::Handled;
}




LayerTool::Status GenericToolZoom::handle_mouse_release(__attribute__((unused)) Layer * layer, QMouseEvent * event)
{
	qDebug() << SG_PREFIX_D << "Called";

	if (event->button() != Qt::LeftButton && event->button() != Qt::RightButton) {
		return LayerTool::Status::Ignored;
	}

	const unsigned int modifiers = event->modifiers() & Qt::ShiftModifier;
	const ScreenPos event_pos = event->localPos();

	/* Has the viewport changed and do we need to redraw items
	   tree into this viewport? */
	bool redraw_tree = false;

	if (this->ztr.is_active()) {
		if (modifiers == Qt::ShiftModifier) {
			if (SlavGPS::are_closer_than(event_pos, this->ztr.m_start_pos, 5)) {
				/* We have just released mouse button
				   on the exact same position
				   i.e. probably haven't moved the
				   mouse at all. Don't do anything. */
				;
			} else {
				const Coord start_coord = this->gisview->screen_pos_to_coord(this->ztr.m_start_pos);
				const Coord cursor_coord = this->gisview->screen_pos_to_coord(event_pos);
				if (!start_coord.is_valid() || !cursor_coord.is_valid()) {
					qDebug() << SG_PREFIX_E << "Failed to get valid coordinate";
					this->ztr.end();
					return LayerTool::Status::Error;
				}
				/* From the extend of the bounds pick the best zoom level. */
				const LatLonBBox bbox(cursor_coord.get_lat_lon(), start_coord.get_lat_lon());
				if (sg_ret::ok == this->gisview->zoom_to_show_bbox_common(bbox, SG_GISVIEWPORT_ZOOM_MIN, false)) {
					redraw_tree = true;
				} else {
					/* For some reason the zoom operation failed. */
				}
				this->ztr.end();
			}
		} else {
			/* User first released Shift key and (without
			   moving the mouse cursor) also left mouse
			   key. Don't zoom. */
			this->ztr.abort(this->gisview);
		}
	} else {
		if (modifiers == Qt::ShiftModifier) {
			const bool zoomed = this->gisview->zoom_with_setting_new_center(mouse_event_to_zoom_direction(event), event_pos);
			if (zoomed) {
				redraw_tree = true;
			} else {
				/* For some reason the zoom operation failed. */
			}
		}
	}

	if (redraw_tree) {
		this->window->draw_tree_items(this->gisview);
	}

	return LayerTool::Status::Handled;
}




/**
   @reviewed-on 2020-03-22
*/
ZoomToRectangle::ZoomToRectangle()
{
	this->m_pen.setColor(QColor("red"));
	this->m_pen.setWidth(1);
}




/**
   @reviewed-on 2020-03-22
*/
sg_ret ZoomToRectangle::draw_rectangle(GisViewport * gisview, const ScreenPos & cursor_pos)
{
	QRectF zoom_rect(this->m_start_pos, cursor_pos);

	/* Draw the rectangle on saved state of viewport. */
	QPixmap marked_pixmap = this->m_orig_viewport_pixmap;
	QPainter painter(&marked_pixmap);
	painter.setPen(this->m_pen);
	painter.drawRect(zoom_rect.normalized());
	painter.end();

	gisview->set_pixmap(marked_pixmap);
	/* This will call GisViewport::paintEvent(), triggering final
	   render of pixmap with rectangle to screen. */
	gisview->update();

	return sg_ret::ok;
}




/**
   @reviewed-on 2020-03-22
*/
sg_ret ZoomToRectangle::begin(GisViewport * gisview, const ScreenPos & cursor_pos)
{
	this->m_is_active = true;
	this->m_start_pos = cursor_pos;
	this->m_orig_viewport_pixmap = gisview->get_pixmap();

	return sg_ret::ok;
}




/**
   @reviewed-on 2020-03-22
*/
sg_ret ZoomToRectangle::end(void)
{
	if (!this->m_orig_viewport_pixmap.isNull()) {
		this->m_orig_viewport_pixmap = QPixmap(); /* Invalidate pixmap. */
	}
	this->m_is_active = false;
	return sg_ret::ok;
}




/**
   @reviewed-on 2020-03-22
*/
sg_ret ZoomToRectangle::abort(GisViewport * gisview)
{
	if (!this->m_orig_viewport_pixmap.isNull()) {
		/* Remove any artifacts - old zoom rectangle. */
		gisview->set_pixmap(this->m_orig_viewport_pixmap);
		gisview->update();
		this->m_orig_viewport_pixmap = QPixmap(); /* Invalidate pixmap. */
	}
	this->m_is_active = false;
	return sg_ret::ok;
}




LayerToolPan::LayerToolPan(Window * window_, GisViewport * gisview_) : LayerTool(window_, gisview_, LayerKind::Max)
{
	this->action_icon_path   = ":/icons/layer_tool/pan_22.png";
	this->action_label       = QObject::tr("&Pan");
	this->action_tooltip     = QObject::tr("Pan Tool");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_P;

	this->cursor_click = QCursor(Qt::ClosedHandCursor);
	this->cursor_release = QCursor(Qt::OpenHandCursor);
}




LayerToolPan::~LayerToolPan()
{
}




SGObjectTypeID LayerToolPan::get_tool_id(void) const
{
	return LayerToolPan::tool_id();
}
SGObjectTypeID LayerToolPan::tool_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.tool.generic.pan");
	return id;
}




LayerTool::Status LayerToolPan::handle_mouse_click(__attribute__((unused)) Layer * layer, QMouseEvent * event)
{
	qDebug() << SG_PREFIX_D << "Called";
	this->window->set_dirty_flag(true);

	/* Standard pan click. */
	if (event->button() == Qt::LeftButton) {
		qDebug() << SG_PREFIX_D << "Will call window->pan_click()";
		this->window->pan_click(event);
	}

	return LayerTool::Status::Handled;
}




LayerTool::Status LayerToolPan::handle_mouse_double_click(__attribute__((unused)) Layer * layer, QMouseEvent * event)
{
	qDebug() << SG_PREFIX_D << "Called";

	/* Zoom in / out on double click.
	   No need to change the center as that has already occurred in the first click of a double click occurrence. */
	if (event->button() == Qt::LeftButton) {
		this->window->set_dirty_flag(true);

		bool zoomed = false;
		if (event->modifiers() & Qt::ShiftModifier) {
			zoomed = this->window->main_gisview()->zoom_on_center_pixel(ZoomDirection::Out);
		} else {
			zoomed = this->window->main_gisview()->zoom_on_center_pixel(ZoomDirection::In);
		}

		if (zoomed) {
			this->window->draw_tree_items(this->gisview);
		} else {
			/* Zoom operation failed for some reason. */
		}

		return LayerTool::Status::Handled;

	} else if (event->button() == Qt::RightButton) {
		this->window->set_dirty_flag(true);
		this->window->main_gisview()->zoom_on_center_pixel(ZoomDirection::Out);
		this->window->draw_tree_items(this->gisview);

		return LayerTool::Status::Handled;

	} else {
		return LayerTool::Status::Ignored;
	}
}




LayerTool::Status LayerToolPan::handle_mouse_move(__attribute__((unused)) Layer * layer, QMouseEvent * event)
{
	//qDebug() << SG_PREFIX_D << "Will call window->pan_move()";
	this->window->pan_move(event);

	return LayerTool::Status::Handled;
}




LayerTool::Status LayerToolPan::handle_mouse_release(__attribute__((unused)) Layer * layer, QMouseEvent * event)
{
	if (event->button() == Qt::LeftButton) {
		this->window->pan_release(event);
		return LayerTool::Status::Handled;
	} else {
		return LayerTool::Status::Ignored;
	}
}




LayerToolSelect::LayerToolSelect(Window * window_, GisViewport * gisview_) : LayerTool(window_, gisview_, LayerKind::Max)
{
	this->action_icon_path   = ":/icons/layer_tool/select_18.png";
	this->action_label       = QObject::tr("&Select");
	this->action_tooltip     = QObject::tr("Select Tool");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_L;
}




LayerToolSelect::LayerToolSelect(Window * window_, GisViewport * gisview_, LayerKind layer_kind_) : LayerTool(window_, gisview_, layer_kind_)
{
}




LayerToolSelect::~LayerToolSelect()
{
}




SGObjectTypeID LayerToolSelect::get_tool_id(void) const
{
	return LayerToolSelect::tool_id();
}
SGObjectTypeID LayerToolSelect::tool_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.tool.generic.select");
	return id;
}




LayerTool::Status LayerToolSelect::handle_mouse_click(Layer * layer, QMouseEvent * event)
{
	qDebug() << SG_PREFIX_D << this->get_tool_id();

	this->select_and_move_activated = false;

	/* Only allow selection on left button. */
	if (event->button() != Qt::LeftButton) {
		return LayerTool::Status::Ignored;
	}

	if (event->modifiers() & SG_MOVE_MODIFIER) {
		this->window->pan_click(event);
	} else {
		this->handle_mouse_click_common(layer, event);
	}

	return LayerTool::Status::Handled;
}




LayerTool::Status LayerToolSelect::handle_mouse_double_click(Layer * layer, QMouseEvent * event)
{
	qDebug() << SG_PREFIX_D << this->get_tool_id();

	this->select_and_move_activated = false;

	/* Only allow selection on left button. */
	if (event->button() != Qt::LeftButton) {
		return LayerTool::Status::Ignored;
	}

	if (event->modifiers() & SG_MOVE_MODIFIER) {
		this->window->pan_click(event);
	} else {
		this->handle_mouse_click_common(layer, event);
	}

	return LayerTool::Status::Handled;
}




void LayerToolSelect::handle_mouse_click_common(__attribute__((unused)) Layer * layer, QMouseEvent * event)
{
	GisViewport * main_gisview = this->window->main_gisview();
	TreeView * tree_view = this->window->layers_panel()->tree_view();
	LayerAggregate * top_layer = this->window->layers_panel()->top_layer();


	/* TODO_LATER: the code in this function visits (in one way or
	   the other) whole tree of layers, starting with top level
	   aggregate layer.  Should we really visit all layers?
	   Shouldn't we visit only selected items and its children? */
	bool handled = false;
	if (event->type() == QEvent::MouseButtonDblClick) {
		qDebug() << SG_PREFIX_D << this->get_tool_id() << "handling double click, looking for layer";
		handled = top_layer->handle_select_tool_double_click(event, main_gisview, this);
	} else {
		qDebug() << SG_PREFIX_D << this->get_tool_id() << "handle single click, looking for layer";
		handled = top_layer->handle_select_tool_click(event, main_gisview, this);
	}


	if (!handled) {
		qDebug() << SG_PREFIX_D << this->get_tool_id() << "mouse event not handled by any layer";

		/* Deselect & redraw screen if necessary to remove the
		   highlight of selected tree item. */
		TreeItem * selected_item = tree_view->get_selected_tree_item();
		if (selected_item) {
			tree_view->deselect_tree_item(selected_item);
			if (this->window->clear_highlight()) {
				this->window->draw_tree_items(this->gisview);
			}
		}
	} else {
		/* Some layer has handled the click - so enable movement. */
		this->select_and_move_activated = true;
	}
}




LayerTool::Status LayerToolSelect::handle_mouse_move(Layer * layer, QMouseEvent * event)
{
	if (this->select_and_move_activated) {
		if (layer) {
			layer->handle_select_tool_move(event, this->gisview, this);
			return LayerTool::Status::Handled;
		} else {
			return LayerTool::Status::Ignored;
		}
	} else {
		/* Optional Panning. */
		if (event->modifiers() & SG_MOVE_MODIFIER) {
			this->window->pan_move(event);
			return LayerTool::Status::Handled;
		} else {
			return LayerTool::Status::Ignored;
		}
	}
}




LayerTool::Status LayerToolSelect::handle_mouse_release(Layer * layer, QMouseEvent * event)
{
	if (this->select_and_move_activated) {
		if (layer) {
			layer->handle_select_tool_release(event, this->gisview, this);
		}
	}

	if (event->button() == Qt::LeftButton && (event->modifiers() & SG_MOVE_MODIFIER)) {
		this->window->pan_release(event);
	}

	/* Force pan off in case it was on. */
	this->window->pan_off();

	/* End of this "select & move" operation. */
	this->select_and_move_activated = false;

	if (event->button() == Qt::RightButton) {
		if (layer && layer->m_kind == LayerKind::TRW && layer->is_visible()) {
			/* See if a TRW item is selected, and show menu for the item. */
			layer->handle_select_tool_context_menu(event, this->window->main_gisview());
		}
	}

	return LayerTool::Status::Handled;
}



bool LayerToolSelect::can_tool_move_object(void)
{
	switch (this->edited_object_state) {
	case ObjectState::NotSelected:
		qDebug() << SG_PREFIX_E << "Can't perform move: object in 'NotSelected' state, tool =" << this->get_tool_id();
		return false;

	case ObjectState::IsSelected:
		/* We didn't actually clicked-and-held an object. */
		qDebug() << SG_PREFIX_E << "Can't perform move: object in 'IsSelected' state, tool =" << this->get_tool_id();
		return false;

	case ObjectState::IsHeld:
		return true;

	default:
		qDebug() << SG_PREFIX_E << "Unexpected object state" << (int) this->edited_object_state;
		return false;
	}
}
