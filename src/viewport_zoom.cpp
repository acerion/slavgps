/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2008, Hein Ragas <viking@ragas.nl>
 * Copyright (C) 2010-2015, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2005-2006, Alex Foobarian <foobarian@gmail.com>
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




//#include <cstdlib>
//#include <cstring>
//#include <cctype>

//#include <QDialog>
//#include <QInputDialog>
#include <QObject>
#include <QDebug>
#include <QMouseEvent>
#include <QWheelEvent>

#include "window.h"
#include "viewport.h"
#include "viewport_zoom.h"
#include "viewport_internal.h"
#include "map_utils.h"




using namespace SlavGPS;




bool ViewportZoomDialog::custom_zoom_dialog(VikingZoomLevel & zoom, QWidget * parent)
{
	ViewportZoomDialog dialog(zoom, parent);

	if (QDialog::Accepted == dialog.exec()) {
		zoom = dialog.get_value();

		/* There is something strange about argument to qSetRealNumberPrecision().  The precision for
		   fractional part is not enough, I had to add few places for leading digits and decimal dot. */
		qDebug() << qSetRealNumberPrecision(5 + 1 + SG_VIEWPORT_ZOOM_PRECISION) << "DD: Dialog: Saving custom zoom as" << zoom;
		return true;
	} else {
		return false;
	}
}




ViewportZoomDialog::ViewportZoomDialog(VikingZoomLevel & zoom, QWidget * parent)
{
	this->setWindowTitle(QObject::tr("Zoom Factors..."));

	int row = 0;

	QLabel * main_label = new QLabel(QObject::tr("Zoom factor (in meters per pixel):"), this);
	this->grid->addWidget(main_label, row, 0, 1, 2); /* Row span = 1, Column span = 2. */
	row++;


	QLabel * xlabel = new QLabel(QObject::tr("X (easting):"), this);

	/* TODO_MAYBE: add some kind of validation and indication for values out of range. */
	this->xspin.setMinimum(SG_VIEWPORT_ZOOM_MIN);
	this->xspin.setMaximum(SG_VIEWPORT_ZOOM_MAX);
	this->xspin.setSingleStep(1);
	this->xspin.setDecimals(SG_VIEWPORT_ZOOM_PRECISION);
	this->xspin.setValue(zoom.get_x());

	this->grid->addWidget(xlabel, row, 0);
	this->grid->addWidget(&this->xspin, row, 1);
	row++;


	QLabel * ylabel = new QLabel(QObject::tr("Y (northing):"), this);

	/* TODO_MAYBE: add some kind of validation and indication for values out of range. */
	this->yspin.setMinimum(SG_VIEWPORT_ZOOM_MIN);
	this->yspin.setMaximum(SG_VIEWPORT_ZOOM_MAX);
	this->yspin.setSingleStep(1);
	this->yspin.setDecimals(SG_VIEWPORT_ZOOM_PRECISION);
	this->yspin.setValue(zoom.get_y());

	this->grid->addWidget(ylabel, row, 0);
	this->grid->addWidget(&this->yspin, row, 1);
	row++;


	this->checkbox.setText(QObject::tr("X and Y zoom factors must be equal"));
	if (zoom.x_y_is_equal()) {
		this->checkbox.setChecked(true);
	}
	this->grid->addWidget(&this->checkbox, row, 0, 1, 2); /* Row span = 1, Column span = 2. */
	row++;


	QObject::connect(&this->xspin, SIGNAL (valueChanged(double)), this, SLOT (spin_changed_cb(double)));
	QObject::connect(&this->yspin, SIGNAL (valueChanged(double)), this, SLOT (spin_changed_cb(double)));
}




VikingZoomLevel ViewportZoomDialog::get_value(void) const
{
	return VikingZoomLevel(this->xspin.value(), this->yspin.value());
}




void ViewportZoomDialog::spin_changed_cb(double new_value)
{
	if (this->checkbox.isChecked()) {
		if (new_value == this->xspin.value()) {
			this->yspin.setValue(new_value);
		} else {
			this->xspin.setValue(new_value);
		}
	}
}



ZoomOperation SlavGPS::mouse_event_to_zoom_operation(const QMouseEvent * event)
{
	ZoomOperation result;

	switch (event->button()) {
	case Qt::LeftButton:
		result = ZoomOperation::In;
		break;
	case Qt::RightButton:
		result = ZoomOperation::Out;
		break;
	default:
		result = ZoomOperation::Noop;
		break;
	};

	return result;
}




ZoomOperation SlavGPS::wheel_event_to_zoom_operation(const QWheelEvent * event)
{
	ZoomOperation result;

	const QPoint angle = event->angleDelta();
	const bool scroll_up = angle.y() > 0;

	if (angle.y() > 0) {
		result = ZoomOperation::In;
	} else if (angle.y() < 0) {
		result = ZoomOperation::Out;
	} else {
		result = ZoomOperation::Noop;
	}

	return result;
}




bool ViewportZoom::move_coordinate_to_center(ZoomOperation zoom_operation, Viewport * viewport, Window * window, const ScreenPos & event_pos)
{
	bool redraw_viewport = false;

	switch (zoom_operation) {
	case ZoomOperation::In:
		viewport->set_center_from_screen_pos(event_pos);
		viewport->zoom_in();
		window->set_dirty_flag(true);
		redraw_viewport = true;
		break;
	case ZoomOperation::Out:
		viewport->set_center_from_screen_pos(event_pos);
		viewport->zoom_out();
		window->set_dirty_flag(true);
		redraw_viewport = true;
		break;
	default:
		break; /* Ignore. */
	};

	return redraw_viewport;
}




bool ViewportZoom::keep_coordinate_in_center(ZoomOperation zoom_operation, Viewport * viewport, Window * window, const ScreenPos & center_pos)
{
	bool redraw_viewport = false;

	switch (zoom_operation) {
	case ZoomOperation::In:
		viewport->set_center_from_screen_pos(center_pos);
		viewport->zoom_in();
		window->set_dirty_flag(true);
		redraw_viewport = true;
		break;
	case ZoomOperation::Out:
		viewport->set_center_from_screen_pos(center_pos);
		viewport->zoom_out();
		window->set_dirty_flag(true);
		redraw_viewport = true;
		break;
	default:
		break; /* Ignore. */
	};

	return redraw_viewport;
}




bool ViewportZoom::keep_coordinate_under_cursor(ZoomOperation zoom_operation, Viewport * viewport, Window * window, const ScreenPos & event_pos, const ScreenPos & center_pos)
{
	bool redraw_viewport = false;

	switch (zoom_operation) {
	case ZoomOperation::In: {

		/* Here we use event position before zooming in. */
		const Coord cursor_coord = viewport->screen_pos_to_coord(event_pos);

		viewport->zoom_in();

		/* Position of event calculated in modified (zoomed in) viewport. */
		const ScreenPos orig_pos = viewport->coord_to_screen_pos(cursor_coord);

		viewport->set_center_from_screen_pos(center_pos.x + (orig_pos.x - event_pos.x), center_pos.y + (orig_pos.y - event_pos.y));
		window->set_dirty_flag(true);
		redraw_viewport = true;
		break;
	}
	case ZoomOperation::Out: {

		/* Here we use event position before zooming out. */
		const Coord cursor_coord = viewport->screen_pos_to_coord(event_pos);

		viewport->zoom_out();

		/* Position of event calculated in modified (zoomed out) viewport. */
		const ScreenPos orig_pos = viewport->coord_to_screen_pos(cursor_coord);

		viewport->set_center_from_screen_pos(center_pos.x + (orig_pos.x - event_pos.x), center_pos.y + (orig_pos.y - event_pos.y));
		window->set_dirty_flag(true);
		redraw_viewport = true;
		break;
	}
	default:
		break;
	}

	return redraw_viewport;
}




VikingZoomLevel::VikingZoomLevel(const VikingZoomLevel & other)
{
	this->x = other.x;
	this->y = other.y;
}




bool VikingZoomLevel::x_y_is_equal(void) const
{
	return this->x == this->y;
}




double VikingZoomLevel::get_x(void) const
{
	return this->x;
}




double VikingZoomLevel::get_y(void) const
{
	return this->y;
}




bool VikingZoomLevel::set(double new_x, double new_y)
{
	if (new_x >= SG_VIEWPORT_ZOOM_MIN
	    && new_x <= SG_VIEWPORT_ZOOM_MAX
	    && new_y >= SG_VIEWPORT_ZOOM_MIN
	    && new_y <= SG_VIEWPORT_ZOOM_MAX) {

		this->x = new_x;
		this->y = new_y;

		return true;
	} else {
		return false;
	}
}


bool VikingZoomLevel::zoom_in(int factor)
{
	if (this->x >= (SG_VIEWPORT_ZOOM_MIN * factor) && this->y >= (SG_VIEWPORT_ZOOM_MIN * factor)) {
		this->x /= factor;
		this->y /= factor;
		return true;
	}

	return false;
}




bool VikingZoomLevel::zoom_out(int factor)
{
	if (this->x <= (SG_VIEWPORT_ZOOM_MAX / factor) && this->y <= (SG_VIEWPORT_ZOOM_MAX / factor)) {
		this->x *= factor;
		this->y *= factor;

		return true;
	}

	return false;
}




QString VikingZoomLevel::pretty_print(CoordMode coord_mode) const
{
	QString result;

	const QString unit = coord_mode == CoordMode::UTM ? "mpp" : "pixelfact";

	if (this->x_y_is_equal()) {
		if ((int) this->x - this->x < 0.0) {
			result = QObject::tr("%1 %2").arg(this->x, 0, 'f', SG_VIEWPORT_ZOOM_PRECISION).arg(unit);
		} else {
			/* xmpp should be a whole number so don't show useless .000 bit. */
		        result = QObject::tr("%1 %2").arg((int) this->x).arg(unit);
		}
	} else {
		result = QObject::tr("%1/%2f %3")
			.arg(this->x, 0, 'f', SG_VIEWPORT_ZOOM_PRECISION)
			.arg(this->y, 0, 'f', SG_VIEWPORT_ZOOM_PRECISION)
			.arg(unit);
	}

	return result;
}





QDebug SlavGPS::operator<<(QDebug debug, const VikingZoomLevel & viking_zoom_level)
{
	debug << "VikingZoomLevel" << viking_zoom_level.get_x() << viking_zoom_level.get_y();
	return debug;
}




bool VikingZoomLevel::operator==(const VikingZoomLevel & other) const
{
	return this->x == other.x && this->y == other.y;
}




bool VikingZoomLevel::value_is_valid(double zoom)
{
	return zoom >= SG_VIEWPORT_ZOOM_MIN && zoom <= SG_VIEWPORT_ZOOM_MAX;
}




bool VikingZoomLevel::is_valid(void) const
{
	return VikingZoomLevel::value_is_valid(this->x) && VikingZoomLevel::value_is_valid(this->y);
}




TileScale VikingZoomLevel::to_tile_scale(void) const
{
	TileScale tile_scale = SlavGPS::map_utils_mpp_to_tile_scale(this->x);
	return tile_scale;
}




MapSourceZoomLevel VikingZoomLevel::to_zoom_level(void) const
{
	MapSourceZoomLevel slippy_zoom_level = SlavGPS::map_utils_mpp_to_zoom_level(this->x);
	return slippy_zoom_level;
}
