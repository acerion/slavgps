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




LayerToolContainer * GenericTools::create_tools(Window * window, GisViewport * gisview)
{
	/* This method should be called only once. */
	static bool created = false;
	if (created) {
		return NULL;
	}

	auto tools = new LayerToolContainer;

	LayerTool * tool = NULL;

	tool = new LayerToolSelect(window, gisview);
	tools->insert({{ tool->id_string, tool }});

	tool = new GenericToolRuler(window, gisview);
	tools->insert({{ tool->id_string, tool }});

	tool = new GenericToolZoom(window, gisview);
	tools->insert({{ tool->id_string, tool }});

	tool = new LayerToolPan(window, gisview);
	tools->insert({{ tool->id_string, tool }});

	created = true;

	return tools;
}






GenericToolRuler::GenericToolRuler(Window * window_, GisViewport * gisview_) : LayerTool(window_, gisview_, LayerType::Max)
{
	this->id_string = "sg.tool.generic.ruler";

	this->action_icon_path   = ":/icons/layer_tool/ruler_18.png";
	this->action_label       = QObject::tr("&Ruler");
	this->action_tooltip     = QObject::tr("Ruler Tool");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_U; /* Ctrl+Shift+R is used for Refresh (deemed more important), so use 'U' instead. */
}




GenericToolRuler::~GenericToolRuler()
{
	delete this->ruler;
}




ToolStatus GenericToolRuler::internal_handle_mouse_click(Layer * layer, QMouseEvent * event)
{
	qDebug() << SG_PREFIX_D << "called";


	if (event->button() != Qt::LeftButton) {
		return ToolStatus::Ignored;
	}

	QString msg;

	if (this->ruler) {
		qDebug() << SG_PREFIX_I << "second click, resetting ruler";

		msg = this->ruler->get_msg();

		this->reset_ruler();
	} else {
		qDebug() << SG_PREFIX_I << "first click, starting ruler";

		const Coord cursor_coord = this->gisview->screen_pos_to_coord(event->x(), event->y());
		msg = cursor_coord.to_string();

		/* Save clean viewport (clean == without ruler drawn on top of it). */
		this->orig_viewport_pixmap = this->gisview->get_pixmap();

		this->ruler = new Ruler(this->gisview, Preferences::get_unit_distance());
		this->ruler->set_begin(event->x(), event->y());
	}

	this->window->get_statusbar()->set_message(StatusBarField::Info, msg);

	return ToolStatus::Ack;
}




ToolStatus GenericToolRuler::internal_handle_mouse_move(Layer * layer, QMouseEvent * event)
{
	qDebug() << SG_PREFIX_D << "Called";

	if (!this->ruler) {
		/* Ruler tool may be selected, but there was no first
		   click that would establish beginning of the
		   ruler. Mouse move event does not influence the
		   ruler, because the ruler doesn't exist yet. */
		return ToolStatus::Ignored;
	}


	/* Redraw ruler that goes from place of initial click
	   (remembered by ruler) to place where mouse cursor is
	   now. */
	QPixmap marked_pixmap = this->orig_viewport_pixmap;
	QPainter painter(&marked_pixmap);
	this->ruler->set_end(event->x(), event->y());
	this->ruler->paint_ruler(painter, Preferences::get_create_track_tooltip());
	this->gisview->set_pixmap(marked_pixmap);
	/* This will call GisViewport::paintEvent(), triggering final render to screen. */
	this->gisview->update();


	const QString msg = ruler->get_msg();
	this->window->get_statusbar()->set_message(StatusBarField::Info, msg);


	return ToolStatus::Ack;
}




ToolStatus GenericToolRuler::internal_handle_mouse_release(Layer * layer, QMouseEvent * event)
{
	qDebug() << SG_PREFIX_I << "called";


	return ToolStatus::Ack;
}




bool GenericToolRuler::deactivate_tool(void)
{
	qDebug() << SG_PREFIX_I << "called";

	this->window->draw_tree_items(this->gisview);

	return true;
}




ToolStatus GenericToolRuler::internal_handle_key_press(Layer * layer, QKeyEvent * event)
{
	qDebug() << SG_PREFIX_D << "called";

	if (event->key() == Qt::Key_Escape) {
		this->reset_ruler();
		this->deactivate_tool();
		return ToolStatus::Ack;
	}

	/* Regardless of whether we used it, return false so other GTK things may use it. */
	return ToolStatus::Ignored;
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




GenericToolZoom::GenericToolZoom(Window * window_, GisViewport * gisview_) : LayerTool(window_, gisview_, LayerType::Max)
{
	this->id_string = "sg.tool.generic.zoom";

	this->action_icon_path   = ":/icons/layer_tool/zoom_18.png";
	this->action_label       = QObject::tr("&Zoom");
	this->action_tooltip     = QObject::tr("Zoom Tool");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_Z;
}




GenericToolZoom::~GenericToolZoom()
{
}




ToolStatus GenericToolZoom::internal_handle_mouse_click(Layer * layer, QMouseEvent * event)
{
	qDebug() << SG_PREFIX_D << "Called";

	const Qt::KeyboardModifiers modifiers = event->modifiers();

	const ScreenPos center_pos = this->gisview->central_get_center_screen_pos();
	const ScreenPos event_pos(event->x(), event->y());

	/* Did the zoom operation affect viewport? */
	bool redraw_viewport = false;

	this->ztr_is_active = false;

	const ZoomOperation zoom_operation = mouse_event_to_zoom_operation(event);

	switch (modifiers) {
	case (Qt::ControlModifier | Qt::ShiftModifier):
		/* Location at the center of viewport will be
		   preserved (coordinate at the center before the zoom
		   and coordinate at the center after the zoom will be
		   the same). */
		redraw_viewport = GisViewportZoom::keep_coordinate_in_center(zoom_operation, this->gisview, this->window, center_pos);
		break;

	case Qt::ControlModifier:
		/* Clicked location will be put at the center of
		   viewport (coordinate of a place under cursor before
		   zoom will be placed at the center of viewport after
		   zoom). */
		redraw_viewport = GisViewportZoom::move_coordinate_to_center(zoom_operation, this->gisview, this->window, event_pos);
		break;

	case Qt::ShiftModifier:
		/* Beginning of "zoom in to rectangle" operation.
		   Notice that there is no "zoom out to rectangle"
		   operation. Get start position of zoom bounds. */

		switch (zoom_operation) {
		case ZoomOperation::In:
			this->ztr_is_active = true;
			this->ztr_start_x = event_pos.x();
			this->ztr_start_y = event_pos.y();
			this->ztr_orig_viewport_pixmap = this->gisview->get_pixmap();
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
		redraw_viewport = GisViewportZoom::keep_coordinate_under_cursor(zoom_operation, this->gisview, this->window, event_pos, center_pos);
		break;

	default:
		/* Other modifier. Just ignore. */
		break;
	};

	if (redraw_viewport) {
		this->window->draw_tree_items(this->gisview);
	}


	return ToolStatus::Ack;
}




ToolStatus GenericToolZoom::internal_handle_mouse_move(Layer * layer, QMouseEvent * event)
{
	qDebug() << SG_PREFIX_D << "Called";

	const unsigned int modifiers = event->modifiers() & Qt::ShiftModifier;

	if (this->ztr_is_active && modifiers != Qt::ShiftModifier) {

		/* When user pressed left mouse button, he also held
		   down Shift key.  This initiated drawing a "zoom to
		   rectangle" box/bound.

		   Right now Shift key is released, so stop drawing
		   the box and abort "zoom to rectangle" procedure. */

		this->ztr_is_active = false;
		return ToolStatus::Ack;
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
	this->gisview->set_pixmap(marked_pixmap);

	/* This will call GisViewport::paintEvent(), triggering final render to screen. */
	this->gisview->update();

	return ToolStatus::Ack;
}




ToolStatus GenericToolZoom::internal_handle_mouse_release(Layer * layer, QMouseEvent * event)
{
	qDebug() << SG_PREFIX_D << "Called";

	if (event->button() != Qt::LeftButton && event->button() != Qt::RightButton) {
		return ToolStatus::Ignored;
	}

	const unsigned int modifiers = event->modifiers() & Qt::ShiftModifier;
	const ScreenPos event_pos(event->x(), event->y());

	/* Did the zoom operation affect viewport? */
	bool redraw_viewport = false;

	if (this->ztr_is_active) {
		/* Ensure that we haven't just released mouse button
		   on the exact same position i.e. probably haven't
		   moved the mouse at all. */
		if (modifiers == Qt::ShiftModifier && (std::fabs(event_pos.x() - this->ztr_start_x) >= 5) && (std::fabs(event_pos.y() - this->ztr_start_y) >= 5)) {

			const Coord start_coord = this->gisview->screen_pos_to_coord(this->ztr_start_x, this->ztr_start_y);
			const Coord cursor_coord = this->gisview->screen_pos_to_coord(event_pos);

			/* From the extend of the bounds pick the best zoom level. */
			const LatLonBBox bbox(cursor_coord.get_lat_lon(), start_coord.get_lat_lon());
			GisViewportZoom::zoom_to_show_bbox_common(this->gisview, this->gisview->get_coord_mode(), bbox, SG_GISVIEWPORT_ZOOM_MIN, false);
			redraw_viewport = true;
		}
	} else {
		/* When pressing shift and clicking for zoom, then
		   change zoom by three levels (zoom in * 3, or zoom
		   out * 3). */
		if (modifiers == Qt::ShiftModifier) {
			/* Zoom in/out by three if possible. */

			if (event->button() == Qt::LeftButton) {
				this->gisview->set_center_coord(event_pos);
				this->gisview->zoom_in_on_center_pixel();
				this->gisview->zoom_in_on_center_pixel();
				this->gisview->zoom_in_on_center_pixel();
			} else { /* Qt::RightButton */
				this->gisview->set_center_coord(event_pos);
				this->gisview->zoom_out_on_center_pixel();
				this->gisview->zoom_out_on_center_pixel();
				this->gisview->zoom_out_on_center_pixel();
			}
			redraw_viewport = true;
		}
	}

	if (redraw_viewport) {
		this->window->draw_tree_items(this->gisview);
	}

	/* Reset "zoom to rectangle" tool.
	   If there was any rectangle drawn in viewport, it has
	   already been erased when zoomed-in or zoomed-out viewport
	   has been redrawn from scratch. */
	this->ztr_is_active = false;

	return ToolStatus::Ack;
}




LayerToolPan::LayerToolPan(Window * window_, GisViewport * gisview_) : LayerTool(window_, gisview_, LayerType::Max)
{
	this->id_string = "sg.tool.generic.pan";

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




ToolStatus LayerToolPan::internal_handle_mouse_click(Layer * layer, QMouseEvent * event)
{
	qDebug() << SG_PREFIX_D << "Called";
	this->window->set_dirty_flag(true);

	/* Standard pan click. */
	if (event->button() == Qt::LeftButton) {
		qDebug() << SG_PREFIX_D << "Will call window->pan_click()";
		this->window->pan_click(event);
	}

	return ToolStatus::Ack;
}




ToolStatus LayerToolPan::internal_handle_mouse_double_click(Layer * layer, QMouseEvent * event)
{
	qDebug() << SG_PREFIX_D << "Called";
	this->window->set_dirty_flag(true);

	/* Zoom in / out on double click.
	   No need to change the center as that has already occurred in the first click of a double click occurrence. */
	if (event->button() == Qt::LeftButton) {
		if (event->modifiers() & Qt::ShiftModifier) {
			this->window->get_main_gis_view()->zoom_out_on_center_pixel();
		} else {
			this->window->get_main_gis_view()->zoom_in_on_center_pixel();
		}
	} else if (event->button() == Qt::RightButton) {
		this->window->get_main_gis_view()->zoom_out_on_center_pixel();
	} else {
		/* Ignore other mouse buttons. */
	}

	this->window->draw_tree_items(this->gisview);

	return ToolStatus::Ack;
}




ToolStatus LayerToolPan::internal_handle_mouse_move(Layer * layer, QMouseEvent * event)
{
	//qDebug() << SG_PREFIX_D << "Will call window->pan_move()";
	this->window->pan_move(event);

	return ToolStatus::Ack;
}




ToolStatus LayerToolPan::internal_handle_mouse_release(Layer * layer, QMouseEvent * event)
{
	if (event->button() == Qt::LeftButton) {
		this->window->pan_release(event);
	}
	return ToolStatus::Ack;
}




LayerToolSelect::LayerToolSelect(Window * window_, GisViewport * gisview_) : LayerTool(window_, gisview_, LayerType::Max)
{
	this->id_string = "sg.tool.generic.select";

	this->action_icon_path   = ":/icons/layer_tool/select_18.png";
	this->action_label       = QObject::tr("&Select");
	this->action_tooltip     = QObject::tr("Select Tool");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_L;
}




LayerToolSelect::LayerToolSelect(Window * window_, GisViewport * gisview_, LayerType layer_type_) : LayerTool(window_, gisview_, layer_type_)
{
}




LayerToolSelect::~LayerToolSelect()
{
}




ToolStatus LayerToolSelect::internal_handle_mouse_click(Layer * layer, QMouseEvent * event)
{
	qDebug() << SG_PREFIX_D << this->id_string;

	this->select_and_move_activated = false;

	/* Only allow selection on left button. */
	if (event->button() != Qt::LeftButton) {
		return ToolStatus::Ignored;
	}

	if (event->modifiers() & SG_MOVE_MODIFIER) {
		this->window->pan_click(event);
	} else {
		this->handle_mouse_click_common(layer, event);
	}

	return ToolStatus::Ack;
}




ToolStatus LayerToolSelect::internal_handle_mouse_double_click(Layer * layer, QMouseEvent * event)
{
	qDebug() << SG_PREFIX_D << this->id_string;

	this->select_and_move_activated = false;

	/* Only allow selection on left button. */
	if (event->button() != Qt::LeftButton) {
		return ToolStatus::Ignored;
	}

	if (event->modifiers() & SG_MOVE_MODIFIER) {
		this->window->pan_click(event);
	} else {
		this->handle_mouse_click_common(layer, event);
	}

	return ToolStatus::Ack;
}




void LayerToolSelect::handle_mouse_click_common(Layer * layer, QMouseEvent * event)
{
	/* TODO_LATER: the code in this branch visits (in one way or the
	   other) whole tree of layers, starting with top level
	   aggregate layer.  Should we really visit all layers?
	   Shouldn't we visit only selected items and its children? */

	bool handled = false;
	if (event->type() == QEvent::MouseButtonDblClick) {
		qDebug() << SG_PREFIX_D << this->id_string << "handling double click, looking for layer";
		handled = this->window->items_tree->get_top_layer()->handle_select_tool_double_click(event, this->window->get_main_gis_view(), this);
	} else {
		qDebug() << SG_PREFIX_D << this->id_string << "handle single click, looking for layer";
		handled = this->window->items_tree->get_top_layer()->handle_select_tool_click(event, this->window->get_main_gis_view(), this);
	}

	if (!handled) {
		qDebug() << SG_PREFIX_D << this->id_string << "mouse event not handled";
		/* Deselect & redraw screen if necessary to remove the highlight. */

		TreeView * tree_view = this->window->items_tree->get_tree_view();
		TreeItem * selected_item = tree_view->get_selected_tree_item();
		if (selected_item) {
			/* Only clear if selected thing is a TrackWaypoint layer or a sublayer. TODO_LATER: improve this condition. */
			if (selected_item->get_tree_item_type() == TreeItemType::Sublayer
			    || selected_item->to_layer()->type == LayerType::TRW) {

				tree_view->deselect_tree_item(selected_item);
				if (this->window->clear_highlight()) {
					this->window->draw_tree_items(this->gisview);
				}
			}
		}
	} else {
		/* Some layer has handled the click - so enable movement. */
		this->select_and_move_activated = true;
	}
}




ToolStatus LayerToolSelect::internal_handle_mouse_move(Layer * layer, QMouseEvent * event)
{
	if (this->select_and_move_activated) {
		if (layer) {
			layer->handle_select_tool_move(event, this->gisview, this);
		}
	} else {
		/* Optional Panning. */
		if (event->modifiers() & SG_MOVE_MODIFIER) {
			this->window->pan_move(event);
		}
	}
	return ToolStatus::Ack;
}




ToolStatus LayerToolSelect::internal_handle_mouse_release(Layer * layer, QMouseEvent * event)
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
		if (layer && layer->type == LayerType::TRW && layer->is_visible()) {
			/* See if a TRW item is selected, and show menu for the item. */
			layer->handle_select_tool_context_menu(event, this->window->get_main_gis_view());
		}
	}

	return ToolStatus::Ack;
}



bool LayerToolSelect::can_tool_move_object(void)
{
	switch (this->edited_object_state) {
	case ObjectState::None:
		qDebug() << SG_PREFIX_E << "Can't perform move: object in 'None' state, tool =" << this->id_string;
		return false;

	case ObjectState::Selected:
		/* We didn't actually clicked-and-held an object. */
		qDebug() << SG_PREFIX_E << "Can't perform move: object in 'Selected' state, tool =" << this->id_string;
		return false;

	case ObjectState::Held:
		return true;
	}
}
