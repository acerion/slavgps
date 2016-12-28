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
#include "slav_qt.h"




#ifndef N_
#define N_(s) s
#endif




using namespace SlavGPS;




LayerToolBox::~LayerToolBox()
{
	for (unsigned int tt = 0; tt < this->n_tools; tt++) {
		delete this->tools[tt];
	}
}




QAction * LayerToolBox::add_tool(LayerTool * layer_tool)
{
#if 0
	toolbar_action_tool_entry_register(this->window->viking_vtb, &layer_tool->radioActionEntry);
#endif
	QString label(layer_tool->radioActionEntry.label);
	QAction * qa = new QAction(label, this->window);

	qa->setObjectName(layer_tool->id_string);
	qDebug() << "DD: Layer Tools: Created qaction with name" << qa->objectName() << qa;
	qa->setIcon(QIcon(QString(layer_tool->radioActionEntry.stock_id)));
	qa->setCheckable(true);

	this->tools.push_back(layer_tool);
	this->n_tools++;

	return qa;
}




LayerTool * LayerToolBox::get_tool(QString const & tool_id)
{
	for (unsigned i = 0; i < this->n_tools; i++) {
		if (tool_id == this->tools[i]->id_string) {
			return this->tools[i];
		}
	}
	return NULL;
}




void LayerToolBox::activate_tool(QAction * qa)
{
	QString tool_id = qa->objectName();
	LayerTool * tool = this->get_tool(tool_id);
	Layer * layer = this->window->layers_panel->get_selected_layer();
#if 0
	if (!layer) {
		return;
	}
#endif

	if (!tool) {
		qDebug() << "EE: Layer Tools: trying to activate a non-existent tool" << tool_id;
		return;
	}
	/* Is the tool already active? */
	if (this->active_tool == tool) {
		assert (this->active_tool_qa == qa);
		return;
	}

	if (this->active_tool) {
		if (this->active_tool->deactivate) {
			this->active_tool->deactivate(NULL, this->active_tool);
		}
	}
	qDebug() << "II: Layer Tools: activating tool" << tool_id;
	if (tool->activate) {
		tool->activate(layer, tool);
	}
	this->active_tool = tool;
	this->active_tool_qa = qa;
}





bool LayerToolBox::deactivate_tool(QAction * qa)
{
	QString tool_id = qa->objectName();
	LayerTool * tool = this->get_tool(tool_id);
	if (!tool) {
		qDebug() << "EE: Layer Tools: trying to deactivate a non-existent tool" << tool_id;
		return true;
	}

	qDebug() << "II: Layer Tools: deactivating tool" << tool_id;

	assert (this->active_tool);

	if (tool->deactivate) {
		tool->deactivate(NULL, tool);
	}
	qa->setChecked(false);

	this->active_tool = NULL;
	this->active_tool_qa = NULL;
	return false;
}




/* A new layer is selected. Update state of tool groups in tool box accordingly. */
void LayerToolBox::selected_layer(QString const & group_name)
{
	for (auto group = this->action_groups.begin(); group != this->action_groups.end(); ++group) {

		QString name((*group)->objectName());

		if (group_name == name) {
			/* This is a group for our newly selected layer. It should become enabled. */
			if ((*group)->isEnabled()) {
				/* The group is already enabled, other groups are already disabled.
				   Nothing more to do in this function. */
				break;
			} else {
				qDebug() << "II: Layer Tool Box: enabling tool group '" << name << "'";
				(*group)->setEnabled(true);
			}
		} else if ("generic" == name) {
			/* This group is always enabled, and should never be disabled. */
			continue;
		} else {
			/* Group other than "group_name". Disable. */
			if ((*group)->isEnabled()) {
				qDebug() << "II: Layer Tool Box: disabling tool group '" << name << "'";
				(*group)->setEnabled(false);
			}
		}
	}
}




/**
   Enable all buttons in given actions group

   If group is non-empty, return first action in that group.
*/
QAction * LayerToolBox::set_group_enabled(QString const & group_name)
{
	QActionGroup * group = this->get_group(group_name);
	if (!group) {
		/* This may a valid situation for layers without tools, e.g. Aggregate. */
		qDebug() << "WW: Layer Tools: can't find group" << group_name << "to enable";
		return NULL;
	}

	qDebug() << "II: Layer Tools: setting group" << group_name << "enabled";
	group->setEnabled(true);

	/* Return currently selected tool (if any is selected). */
	QAction * returned = group->checkedAction();
	if (returned) {
		qDebug() << "II: Layer Tools: returning selected action" << returned->objectName() << "from group" << group_name;
		return returned;
	}

	/* Return first tool from toolbox (even if not selected. */
	if (!group->actions().empty()) {
		qDebug() << "II: Layer Tools: returning first action" << (*group->actions().constBegin())->objectName() << "from group" << group_name;
		return *group->actions().constBegin();
	}

	qDebug() << "WW: Layer Tools: returning NULL action";
	return NULL;
}




#if 0
/**
   Disable all buttons in given actions group

   If any action is checked (active), return that action. The function doesn't un-check that action.

   If a group is "generic", its buttons are not disabled. Its active button (if present) is returned nonetheless.
*/
QAction * LayerToolBox::set_group_disabled(QString const & group_name)
{
	QActionGroup * group = this->get_group(group_name);
	if (!group) {
		qDebug() << "EE: Layer Tools: can't find group" << group_name << "to disable";
		return NULL;
	}

	if (group_name != "generic") { /* Generic tools are always enabled - they work for every type of layer. */
		qDebug() << "II: Layer Tools: setting group" << group_name << "disabled";
		group->setEnabled(false);
	}

	return group->checkedAction();
}
#endif



#if 0
/**
   Disable all buttons in groups other than given actions group

   Make an exception for actions group called "generic" - this one should be always enabled.
*/
void LayerToolBox::set_other_groups_disabled(QString const & group_name)
{
	for (auto group = this->action_groups.begin(); group != this->action_groups.end(); ++group) {

		QString other_group_name((*group)->objectName());

		if (group_name == other_group_name) {
			/* Disable other groups, not this group. */
			continue;
		}

		if ("generic" == other_group_name) {
			/* Don't disable an action group that should always be enabled. */
			continue;
		}

		this->set_group_disabled(other_group_name);
	}

	return;
}
#endif



/**
   Find group by object name
*/
QActionGroup * LayerToolBox::get_group(QString const & group_name)
{
	for (auto group = this->action_groups.begin(); group != this->action_groups.end(); ++group) {
		if ((*group)->objectName() == group_name) {
			return (*group);
		}
	}

	return NULL;
}




QAction * LayerToolBox::get_active_tool_action(void)
{
	return this->active_tool_qa;
}




LayerTool * LayerToolBox::get_active_tool(void)
{
	return this->active_tool;
}



#if 0
void LayerToolBox::activate_layer_tools(QString const & layer_type)
{
	for (auto group = this->action_groups.begin(); group != this->action_groups.end(); ++group) {
		bool is_window_tools = (*group)->objectName() == "generic";
		bool activate = (*group)->objectName() == layer_type;

		(*group)->setEnabled(activate || is_window_tools);

		if (activate) {
			QList<QAction *>::const_iterator action = (*group)->actions().constBegin();
			if (action != (*group)->actions().constEnd()) {
				(*action)->setChecked(true);
			}
		}
	}
}
#endif



void LayerToolBox::add_group(QActionGroup * group)
{
	this->action_groups.push_back(group);
}




QCursor const * LayerToolBox::get_cursor_click(QString const & tool_id)
{
	return this->get_tool(tool_id)->cursor_release;
}




QCursor const * LayerToolBox::get_cursor_release(QString const & tool_id)
{
	return this->get_tool(tool_id)->cursor_release;
}




void LayerToolBox::click(QMouseEvent * event)
{
	Layer * layer = this->window->layers_panel->get_selected_layer();
	if (!layer) {
		qDebug() << "EE: Layer Tools: click received, no layer";
		return;
	}
	qDebug() << "II: Layer Tools: click received, selected layer" << layer->debug_string;


	if (!this->active_tool) {
		qDebug() << "EE: Layer Tools: click received, no active tool";
		return;
	}


	if (!this->active_tool->click) {
		qDebug() << "EE: Layer Tools: tool" << this->active_tool->id_string << "doesn't have 'click' method";
		return;
	}


	LayerType layer_tool_type = this->active_tool->layer_type;
	if (layer_tool_type != layer->type                   /* Click received for layer other than current layer. */
	    && layer_tool_type != LayerType::NUM_TYPES) {    /* Click received for something other than generic tool. */

		qDebug() << "EE: Layer Tools: click received, invalid type";
		return;
	}

	qDebug() << "II: Layer Tools: click received, will pass it to tool" << this->active_tool->id_string << "for layer" << layer->debug_string;
	this->active_tool->viewport->setCursor(*this->active_tool->cursor_click);
	this->active_tool->click(layer, event, this->active_tool);

	return;
}




void LayerToolBox::double_click(QMouseEvent * event)
{
	Layer * layer = this->window->layers_panel->get_selected_layer();
	if (!layer) {
		qDebug() << "EE: Layer Tools: double click received, no layer";
		return;
	}
	qDebug() << "II: Layer Tools: double click received, selected layer" << layer->debug_string;


	if (!this->active_tool) {
		qDebug() << "EE: Layer Tools: click received, no active tool";
		return;
	}


	if (!this->active_tool->double_click) {
		qDebug() << "EE: Layer Tools: tool" << this->active_tool->id_string << "doesn't have 'double click' method";
		return;
	}


	LayerType layer_tool_type = this->active_tool->layer_type;
	if (layer_tool_type != layer->type                   /* Click received for layer other than current layer. */
	    && layer_tool_type != LayerType::NUM_TYPES) {    /* Click received for something other than generic tool. */

		qDebug() << "EE: Layer Tools: double click received, invalid type";
		return;
	}


	qDebug() << "II: Layer Tools: double click received, will pass it to tool" << this->active_tool->id_string << "for layer" << layer->debug_string;
	this->active_tool->viewport->setCursor(*this->active_tool->cursor_click);
	this->active_tool->double_click(layer, event, this->active_tool);

	return;
}




void LayerToolBox::move(QMouseEvent * event)
{
	Layer * layer = this->window->layers_panel->get_selected_layer();
	if (!layer) {
		qDebug() << "EE: Layer Tools: click received, no layer";
		return;
	}
	qDebug() << "II: Layer Tools: move received, selected layer" << layer->debug_string;


	if (!this->active_tool) {
		qDebug() << "EE: Layer Tools: move received, no active tool";
		return;
	}


	if (!this->active_tool->move) {
		qDebug() << "EE: Layer Tools: tool" << this->active_tool->id_string << "doesn't have 'move' method";
		return;
	}


	LayerType layer_tool_type = this->active_tool->layer_type;
	if (layer_tool_type != layer->type                   /* Click received for layer other than current layer. */
	    && layer_tool_type != LayerType::NUM_TYPES) {    /* Click received for something other than generic tool. */

		qDebug() << "EE: Layer Tools: double click received, invalid type";
		return;
	}


	qDebug() << "II: Layer Tools: move received, passing to tool" << this->active_tool->get_description();

	if (LayerToolFuncStatus::ACK_GRAB_FOCUS == this->active_tool->move(layer, event, this->active_tool)) {
#if 0
		gtk_widget_grab_focus(this->window->viewport->get_toolkit_widget());
#endif
	}

	return;
}




void LayerToolBox::release(QMouseEvent * event)
{
	Layer * layer = this->window->layers_panel->get_selected_layer();
	if (!layer) {
		qDebug() << "EE: Layer Tools: release received, no layer";
		return;
	}
	qDebug() << "II: Layer Tools: release received, selected layer" << layer->debug_string;


	if (!this->active_tool) {
		qDebug() << "EE: Layer Tools: release received, no active tool";
		return;
	}


	if (!this->active_tool->release) {
		qDebug() << "EE: Layer Tools: tool" << this->active_tool->id_string << "doesn't have 'release' method";
		return;
	}


	LayerType layer_tool_type = this->active_tool->layer_type;
	if (layer_tool_type != layer->type                   /* Click received for layer other than current layer. */
	    && layer_tool_type != LayerType::NUM_TYPES) {    /* Click received for something other than generic tool. */

		qDebug() << "EE: Layer Tools: release received, invalid type";
		return;
	}


	qDebug() << "II: Layer Tools: release received, will pass it to tool" << this->active_tool->id_string << "for layer" << layer->debug_string;
	this->active_tool->viewport->setCursor(*this->active_tool->cursor_release);
	this->active_tool->release(layer, event, this->active_tool);

	return;
}




/********************************************************************************
 ** Ruler tool code
 ********************************************************************************/




static bool draw_buf_done = true;

typedef struct {
	QWindow * window;
	GdkGC * gdk_style;
	GdkPixmap * pixmap;
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




static LayerToolFuncStatus ruler_click(Layer * layer, QMouseEvent * event, LayerTool * tool);
static LayerToolFuncStatus ruler_move(Layer * layer, QMouseEvent * event, LayerTool * tool);
static LayerToolFuncStatus ruler_release(Layer * layer, QMouseEvent * event, LayerTool * tool);
static void ruler_deactivate(Layer * layer, LayerTool * tool);
static bool ruler_key_press(Layer * layer, GdkEventKey *event, LayerTool * tool);




/**
   @param x1, y1 - coordinates of beginning of ruler (start coordinates, where cursor was pressed down)
   @param x2, y2 - coordinates of end of ruler (end coordinates, where cursor currently is)
*/
static void draw_ruler(Viewport * viewport, QPixmap * pixmap, QPen & pen, int x1, int y1, int x2, int y2, double distance)
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
	this->id_string = QString("generic.ruler");

	this->radioActionEntry.stock_id = strdup(":/icons/layer_tool/ruler_18.png");
	this->radioActionEntry.label = strdup(N_("&Ruler"));
	this->radioActionEntry.accelerator = strdup("<control><shift>U"); /* Ctrl+Shift+R is used for Refresh (deemed more important), so use 'U' instead. */
	this->radioActionEntry.tooltip = strdup(N_("Ruler Tool"));
	this->radioActionEntry.value = 2;

	this->deactivate = ruler_deactivate;
	this->click = (ToolMouseFunc) ruler_click;
	this->move = (ToolMouseMoveFunc) ruler_move;
	this->release = (ToolMouseFunc) ruler_release;
	this->key_press = ruler_key_press;

	this->cursor_click = new QCursor(Qt::ArrowCursor);
	this->cursor_release = new QCursor(Qt::ArrowCursor);
	//shape = Qt::BitmapCursor;
	//this->cursor_data = &cursor_ruler_pixbuf;


	this->ruler = (ruler_tool_state_t *) malloc(1 * sizeof (ruler_tool_state_t));
	memset(this->ruler, 0, sizeof (ruler_tool_state_t));
}





static LayerToolFuncStatus ruler_click(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
	qDebug() << "DD: Layer Tools: Ruler: ->click()";

	struct LatLon ll;
	VikCoord coord;
	char temp[128] = { 0 };

	if (event->button() == Qt::LeftButton) {
		char * lat = NULL;
		char * lon = NULL;
		tool->viewport->screen_to_coord(event->x(), event->y(), &coord);
		vik_coord_to_latlon(&coord, &ll);
		a_coords_latlon_to_string(&ll, &lat, &lon);
		if (tool->ruler->has_start_coord) {
			DistanceUnit distance_unit = a_vik_get_units_distance();
			switch (distance_unit) {
			case DistanceUnit::KILOMETRES:
				sprintf(temp, "%s %s DIFF %f meters", lat, lon, vik_coord_diff(&coord, &tool->ruler->start_coord));
				break;
			case DistanceUnit::MILES:
				sprintf(temp, "%s %s DIFF %f miles", lat, lon, VIK_METERS_TO_MILES(vik_coord_diff(&coord, &tool->ruler->start_coord)));
				break;
			case DistanceUnit::NAUTICAL_MILES:
				sprintf(temp, "%s %s DIFF %f NM", lat, lon, VIK_METERS_TO_NAUTICAL_MILES(vik_coord_diff(&coord, &tool->ruler->start_coord)));
				break;
			default:
				sprintf(temp, "Just to keep the compiler happy");
				qDebug() << "EE: Layer Tools: Ruler click: invalid distance unit:" << (int) distance_unit;
			}

			qDebug() << "II: Layer Tools: Ruler: second click, dropping start coordinates";
			tool->ruler->has_start_coord = false;
		} else {
			sprintf(temp, "%s %s", lat, lon);
			qDebug() << "II: Layer Tools: Ruler: first click, saving start coordinates";
			tool->ruler->has_start_coord = true;
		}

		QString message(temp);
		tool->window->get_statusbar()->set_message(StatusBarField::INFO, message);
		tool->ruler->start_coord = coord;
	} else {
		tool->viewport->set_center_screen((int) event->x(), (int) event->y());
		tool->window->draw_update_cb();
	}

	return LayerToolFuncStatus::ACK;
}




static LayerToolFuncStatus ruler_move(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
	qDebug() << "DD: Layer Tools: Ruler: ->move()";

	struct LatLon ll;
	VikCoord coord;
	char temp[128] = { 0 };

	if (!tool->ruler->has_start_coord) {
		qDebug() << "II: Layer Tools: Ruler: not drawing, we don't have start coordinates";
		return LayerToolFuncStatus::ACK;
	}

	static QPixmap * buf = NULL;
	char * lat = NULL;
	char * lon = NULL;
	int w1 = tool->viewport->get_width();
	int h1 = tool->viewport->get_height();
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

	tool->viewport->screen_to_coord(event->x(), event->y(), &coord);
	vik_coord_to_latlon(&coord, &ll);

	int start_x;
	int start_y;
	tool->viewport->coord_to_screen(&tool->ruler->start_coord, &start_x, &start_y);

	//gdk_draw_drawable(buf, gtk_widget_get_style(tool->viewport->get_toolkit_widget())->black_gc,
	//		  tool->viewport->get_pixmap(), 0, 0, 0, 0, -1, -1);

	QPen pen("black");
	pen.setWidth(1);
	//draw_ruler(tool->viewport, buf, gtk_widget_get_style(tool->viewport->get_toolkit_widget())->black_gc, start_x, start_y, event->x(), event->y(), vik_coord_diff(&coord, &(tool->ruler->start_coord)));
	draw_ruler(tool->viewport, buf, pen, start_x, start_y, event->x(), event->y(), vik_coord_diff(&coord, &tool->ruler->start_coord));

	if (draw_buf_done) {
#if 0
		static draw_buf_data_t pass_along;
		pass_along.window = gtk_widget_get_window(tool->viewport->get_toolkit_widget());
		pass_along.gdk_style = gtk_widget_get_style(tool->viewport->get_toolkit_widget())->black_gc;
		pass_along.pixmap = buf;
		g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, (GSourceFunc) draw_buf, (void *) &pass_along, NULL);
		draw_buf_done = false;

#else
		QPainter painter(tool->viewport->scr_buffer);
		//painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
		painter.drawPixmap(0, 0, *buf);
		tool->viewport->update();
		draw_buf_done = true;
#endif

	}


	a_coords_latlon_to_string(&ll, &lat, &lon);
	DistanceUnit distance_unit = a_vik_get_units_distance();
	switch (distance_unit) {
	case DistanceUnit::KILOMETRES:
		sprintf(temp, "%s %s DIFF %f meters", lat, lon, vik_coord_diff(&coord, &tool->ruler->start_coord));
		break;
	case DistanceUnit::MILES:
		sprintf(temp, "%s %s DIFF %f miles", lat, lon, VIK_METERS_TO_MILES (vik_coord_diff(&coord, &tool->ruler->start_coord)));
		break;
	case DistanceUnit::NAUTICAL_MILES:
		sprintf(temp, "%s %s DIFF %f NM", lat, lon, VIK_METERS_TO_NAUTICAL_MILES (vik_coord_diff(&coord, &tool->ruler->start_coord)));
		break;
	default:
		sprintf(temp, "Just to keep the compiler happy");
		qDebug() << "EE: Layer Tools: Ruler move: unknown distance unit:" << (int) distance_unit;
	}

	tool->window->get_statusbar()->set_message(StatusBarField::INFO, QString(temp));

	/* We have used the start coordinate to draw a ruler. The coordinate should be discarded on LMB release. */
	tool->ruler->invalidate_start_coord = true;

	return LayerToolFuncStatus::ACK;
}




static LayerToolFuncStatus ruler_release(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
	qDebug() << "II: Layer Tools: Ruler: ->release()";
	if (tool->ruler->invalidate_start_coord) {
		/* In ->move() we have been using ->start_coord to draw a ruler.
		   Now the ->start_coord is unnecessary and should be discarded. */
		tool->ruler->invalidate_start_coord = false;
		tool->ruler->has_start_coord = false;
	}
	return LayerToolFuncStatus::ACK;
}




static void ruler_deactivate(Layer * layer, LayerTool * tool)
{
	qDebug() << "II: Layer Tools: Ruler: ->deactivate() called";
	tool->window->draw_update_cb();
}




static bool ruler_key_press(Layer * layer, GdkEventKey *event, LayerTool * tool)
{
#if 0
	if (event->keyval == GDK_Escape) {
		tool->ruler->invalidate_start_coord = false;
		tool->ruler->has_start_coord = false;
		ruler_deactivate(layer, tool);
		return true;
	}
#endif
	/* Regardless of whether we used it, return false so other GTK things may use it. */
	return false;
}

/*** End ruler code. ********************************************************/




/********************************************************************************
 ** Zoom tool code
 ********************************************************************************/




static LayerToolFuncStatus zoomtool_click(Layer * layer, QMouseEvent * event, LayerTool * tool);
static LayerToolFuncStatus zoomtool_move(Layer * layer, QMouseEvent * event, LayerTool * tool);
static LayerToolFuncStatus zoomtool_release(Layer * layer, QMouseEvent * event, LayerTool * tool);




/*
 * In case the screen size has changed
 */
static void zoomtool_resize_pixmap(LayerTool * tool)
{
	/* Allocate a drawing area the size of the viewport. */
	int w1 = tool->window->viewport->get_width();
	int h1 = tool->window->viewport->get_height();

#if 0

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
#endif
}




LayerTool * SlavGPS::zoomtool_create(Window * window, Viewport * viewport)
{
	return new LayerToolZoom(window, viewport);
}




LayerToolZoom::LayerToolZoom(Window * window, Viewport * viewport) : LayerTool(window, viewport, LayerType::NUM_TYPES)
{
	this->id_string = QString("generic.zoom");

	this->radioActionEntry.stock_id = strdup(":/icons/layer_tool/zoom_18.png");
	this->radioActionEntry.label = strdup(N_("&Zoom"));
	this->radioActionEntry.accelerator = strdup("<control><shift>Z");
	this->radioActionEntry.tooltip = strdup(N_("Zoom Tool"));
	this->radioActionEntry.value = 1;

	this->click = (ToolMouseFunc) zoomtool_click;
	this->move = (ToolMouseMoveFunc) zoomtool_move;
	this->release = (ToolMouseFunc) zoomtool_release;

	this->cursor_click = new QCursor(Qt::ArrowCursor);
	this->cursor_release = new QCursor(Qt::ArrowCursor);

	//this->cursor_shape = Qt::BitmapCursor;
	//this->cursor_data = &cursor_zoom_pixbuf;

	this->zoom = (zoom_tool_state_t *) malloc(1 * sizeof (zoom_tool_state_t));
	memset(this->zoom, 0, sizeof (zoom_tool_state_t));
}




static LayerToolFuncStatus zoomtool_click(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
	qDebug() << "DD: Layer Tools: Zoom: ->click() called";
#if 0
	tool->window->modified = true;
	unsigned int modifiers = event->modifiers() & (GDK_SHIFT_MASK | GDK_CONTROL_MASK);

	VikCoord coord;
	int center_x = tool->window->viewport->get_width() / 2;
	int center_y = tool->window->viewport->get_height() / 2;

	bool skip_update = false;

	tool->zoom->bounds_active = false;

	if (modifiers == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) {
		/* This zoom is on the center position. */
		tool->window->viewport->set_center_screen(center_x, center_y);
		if (event->button() == Qt::LeftButton) {
			tool->window->viewport->zoom_in();
		} else if (event->button() == Qt::RigthButton) {
			tool->window->viewport->zoom_out();
		}
	} else if (modifiers == GDK_CONTROL_MASK) {
		/* This zoom is to recenter on the mouse position. */
		tool->window->viewport->set_center_screen((int) event->x, (int) event->y);
		if (event->button() == Qt::LeftButton) {
			tool->window->viewport->zoom_in();
		} else if (event->button() == Qt::RightButton) {
			tool->window->viewport->zoom_out();
		}
	} else if (modifiers == GDK_SHIFT_MASK) {
		/* Get start of new zoom bounds. */
		if (event->button() == Qt::LeftButton) {
			tool->zoom->bounds_active = true;
			tool->zoom->start_x = (int) event->x;
			tool->zoom->start_y = (int) event->y;
			skip_update = true;
		}
	} else {
		/* Make sure mouse is still over the same point on the map when we zoom. */
		tool->window->viewport->screen_to_coord(event->x, event->y, &coord);
		if (event->button() == Qt::LeftButton) {
			tool->window->viewport->zoom_in();
		} else if (event->button() == Qt::RightButton) {
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

#endif

	return LayerToolFuncStatus::ACK;
}




static LayerToolFuncStatus zoomtool_move(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
#if 0
	unsigned int modifiers = event->modifiers() & (GDK_SHIFT_MASK | GDK_CONTROL_MASK);

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
		draw_rectangle(tool->zoom->pixmap, gtk_widget_get_style(tool->window->viewport->get_toolkit_widget())->black_gc, xx, yy, width, height);

		/* Only actually draw when there's time to do so. */
		if (draw_buf_done) {
			static draw_buf_data_t pass_along;
			pass_along.window = gtk_widget_get_window(tool->window->viewport->get_toolkit_widget());
			pass_along.gdk_style = gtk_widget_get_style(tool->window->viewport->get_toolkit_widget())->black_gc;
			pass_along.pixmap = tool->zoom->pixmap;
			g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, (GSourceFunc) draw_buf, &pass_along, NULL);
			draw_buf_done = false;
		}
	} else {
		tool->zoom->bounds_active = false;
	}
#endif
	return LayerToolFuncStatus::ACK;
}




static LayerToolFuncStatus zoomtool_release(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
#if 0
	unsigned int modifiers = event->modifiers() & (GDK_SHIFT_MASK | GDK_CONTROL_MASK);

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
			if (event->button() == Qt::LeftButton) {
				tool->window->viewport->zoom_in();
				tool->window->viewport->zoom_in();
				tool->window->viewport->zoom_in();
			} else if (event->button() == Qt::RightButton) {
				tool->window->viewport->zoom_out();
				tool->window->viewport->zoom_out();
				tool->window->viewport->zoom_out();
			}
		}
	}

	tool->window->draw_update();

	/* Reset. */
	tool->zoom->bounds_active = false;
#endif
	return LayerToolFuncStatus::ACK;
}


/*** End zoom code. ********************************************************/




/********************************************************************************
 ** Pan tool code
 ********************************************************************************/




static LayerToolFuncStatus pantool_click(Layer * layer, QMouseEvent * event, LayerTool * tool);
static LayerToolFuncStatus pantool_move(Layer * layer, QMouseEvent * event, LayerTool * tool);
static LayerToolFuncStatus pantool_release(Layer * layer, QMouseEvent * event, LayerTool * tool);




LayerTool * SlavGPS::pantool_create(Window * window, Viewport * viewport)
{
	return new LayerToolPan(window, viewport);
}

LayerToolPan::LayerToolPan(Window * window, Viewport * viewport) : LayerTool(window, viewport, LayerType::NUM_TYPES)
{
	this->id_string = QString("generic.pan");

	this->radioActionEntry.stock_id = strdup(":/icons/layer_tool/pan_22.png");
	this->radioActionEntry.label = strdup(N_("&Pan"));
	this->radioActionEntry.accelerator = strdup("<control><shift>P");
	this->radioActionEntry.tooltip = strdup(N_("Pan Tool"));
	this->radioActionEntry.value = 0;

	this->click = (ToolMouseFunc) pantool_click;
	this->move = (ToolMouseMoveFunc) pantool_move;
	this->release = (ToolMouseFunc) pantool_release;

	this->cursor_click = new QCursor(Qt::ClosedHandCursor);
	this->cursor_release = new QCursor(Qt::OpenHandCursor);
}




/* NB Double clicking means this gets called THREE times!!! */
static LayerToolFuncStatus pantool_click(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
	qDebug() << "DD: Layer Tools: Pan: ->click() called";
	tool->window->modified = true;
#if 0
	if (event->type == GDK_2BUTTON_PRESS) {
		/* Zoom in / out on double click.
		   No need to change the center as that has already occurred in the first click of a double click occurrence. */
		if (event->button() == Qt::LeftButton) {
			unsigned int modifier = event->modifiers() & GDK_SHIFT_MASK;
			if (modifier) {
				tool->window->viewport->zoom_out();
			} else {
				tool->window->viewport->zoom_in();
			}
		} else if (event->button() == Qt::RightButton) {
			tool->window->viewport->zoom_out();
		}

		tool->window->draw_update();
	} else {
#endif

		qDebug() << "DD: Layer Tools: Pan: ->click() called, checking button";
		/* Standard pan click. */
		if (event->button() == Qt::LeftButton) {
			qDebug() << "DD: Layer Tools: Pan click: window->pan_click()";
			tool->window->pan_click(event);
		}
#if 0
	}
#endif
	return LayerToolFuncStatus::ACK;
}




static LayerToolFuncStatus pantool_move(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
	qDebug() << "DD: Layer Tools: Pan: calling window->pan_move()";
	tool->window->pan_move(event);

	return LayerToolFuncStatus::ACK;
}




static LayerToolFuncStatus pantool_release(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
	if (event->button() == Qt::LeftButton) {
		tool->window->pan_release(event);
	}
	return LayerToolFuncStatus::ACK;
}


/*** End pan code. ********************************************************/




/********************************************************************************
 ** Select tool code
 ********************************************************************************/




static LayerToolFuncStatus selecttool_click(Layer * layer, QMouseEvent * event, LayerTool * tool);
static LayerToolFuncStatus selecttool_move(Layer * layer, QMouseEvent * event, LayerTool * tool);
static LayerToolFuncStatus selecttool_release(Layer * layer, QMouseEvent * event, LayerTool * tool);




LayerTool * SlavGPS::selecttool_create(Window * window, Viewport * viewport)
{
	return new LayerToolSelect(window, viewport);
}




LayerToolSelect::LayerToolSelect(Window * window, Viewport * viewport) : LayerTool(window, viewport, LayerType::NUM_TYPES)
{
	this->id_string = QString("generic.select");

	this->radioActionEntry.stock_id = strdup(":/icons/layer_tool/select_18.png");
	this->radioActionEntry.label = strdup(N_("&Select"));
	this->radioActionEntry.accelerator = strdup("<control><shift>S");
	this->radioActionEntry.tooltip = strdup(N_("Select Tool"));
	this->radioActionEntry.value = 3;

	this->click = (ToolMouseFunc) selecttool_click;
	this->move = (ToolMouseMoveFunc) selecttool_move;
	this->release = (ToolMouseFunc) selecttool_release;

	this->cursor_click = new QCursor(Qt::ArrowCursor);
	this->cursor_release = new QCursor(Qt::ArrowCursor);

	this->ed  = (tool_ed_t *) malloc(1 * sizeof (tool_ed_t));
	memset(this->ed, 0, sizeof (tool_ed_t));
}




typedef struct {
	bool cont;
	Viewport * viewport;
	QMouseEvent * event;
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



#if 1
#ifdef WINDOWS
/* Hopefully Alt keys by default. */
#define VIK_MOVE_MODIFIER GDK_MOD1_MASK
#else
/* Alt+mouse on Linux desktops tend to be used by the desktop manager.
   Thus use an alternate modifier - you may need to set something into this group.
   Viking used GDK_MOD5_MASK.
*/
#define VIK_MOVE_MODIFIER Qt::ControlModifier
#endif
#endif



static LayerToolFuncStatus selecttool_click(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
	qDebug() << "DD: Layer Tools:" << tool->id_string << "->click() called";

	tool->window->select_move = false;

	/* Only allow selection on primary button. */
	if (event->button() == Qt::LeftButton) {

		if (event->modifiers() & VIK_MOVE_MODIFIER) {
			tool->window->pan_click(event);
		} else {
			/* Enable click to apply callback to potentially all track/waypoint layers. */
			/* Useful as we can find things that aren't necessarily in the currently selected layer. */
			std::list<Layer *> * layers = tool->window->layers_panel->get_all_layers_of_type(LayerType::TRW, false); /* Don't get invisible layers. */
			clicker ck;
			ck.cont = true;
			ck.viewport = tool->window->viewport;
			qDebug() << "DD: Layer Tools: Select click:" << __FUNCTION__ << __LINE__;
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

				TreeIndex const & index = tree_view->get_selected_item();
				if (index.isValid()) {
					/* Only clear if selected thing is a TrackWaypoint layer or a sublayer. */
					TreeItemType type = tree_view->get_item_type(index);
					if (type == TreeItemType::SUBLAYER
					    || tree_view->get_layer(index)->type == LayerType::TRW) {

						tree_view->unselect(index);
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
	}

	return LayerToolFuncStatus::ACK;
}




static LayerToolFuncStatus selecttool_move(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
#if 0
	if (tool->window->select_move) {
		/* Don't care about trw here. */
		if (tool->ed->trw) {
			layer->select_move(event, tool->viewport, tool); /* kamilFIXME: layer->select_move or trw->select_move? */
		}
	} else {
		/* Optional Panning. */
		if (event->modifiers() & VIK_MOVE_MODIFIER) {
			tool->window->pan_move(event);
		}
	}
#endif
	return LayerToolFuncStatus::ACK;
}




static LayerToolFuncStatus selecttool_release(Layer * layer, QMouseEvent * event, LayerTool * tool)
{
#if 0
	if (tool->window->select_move) {
		/* Don't care about trw here. */
		if (tool->ed->trw) {
			((LayerTRW *) tool->ed->trw)->select_release(event, tool->viewport, tool);
		}
	}

	if (event->button() == Qt::LeftButton && (event->modifiers() & VIK_MOVE_MODIFIER)) {
		tool->window->pan_release(event);
	}

	/* Force pan off in case it was on. */
	tool->window->pan_move_flag = false;
	tool->window->pan_x = tool->window->pan_y = -1;

	/* End of this select movement. */
	tool->window->select_move = false;
#endif

	if (event->button() == Qt::RightButton) {
		if (layer && layer->type == LayerType::TRW && layer->visible) {
			/* See if a TRW item is selected, and show menu for the item. */
			layer->select_tool_context_menu(event, tool->window->viewport);
		}
	}

	return LayerToolFuncStatus::ACK;
}

/*** End select tool code. ********************************************************/
