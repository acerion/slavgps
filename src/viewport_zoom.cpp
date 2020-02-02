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




#define SG_MODULE "Viewport Zoom"




/* World Scale:
   VIK_GZ(MAGIC_SEVENTEEN) down to submeter scale: 1/VIK_GZ(5)

   No map provider is going to have tiles at the highest zoom in level - but we can interpolate to that. */


static const double scale_mpps[] = {
	VIK_GZ(0),    /*       1 */
	VIK_GZ(1),    /*       2 */
	VIK_GZ(2),    /*       4 */
	VIK_GZ(3),    /*       8 */
	VIK_GZ(4),    /*      16 */
	VIK_GZ(5),    /*      32 */
	VIK_GZ(6),    /*      64 */
	VIK_GZ(7),    /*     128 */
	VIK_GZ(8),    /*     256 */
	VIK_GZ(9),    /*     512 */
	VIK_GZ(10),   /*    1024 */
	VIK_GZ(11),   /*    2048 */
	VIK_GZ(12),   /*    4096 */
	VIK_GZ(13),   /*    8192 */
	VIK_GZ(14),   /*   16384 */
	VIK_GZ(15),   /*   32768 */
	VIK_GZ(16),
	VIK_GZ(17) };

static const int num_scales = (sizeof(scale_mpps) / sizeof(scale_mpps[0]));

static const double scale_neg_mpps[] = {
	1.0/VIK_GZ(0),    /*   1   */
	1.0/VIK_GZ(1),    /*   0.5 */
	1.0/VIK_GZ(2),    /*   0.25 */
	1.0/VIK_GZ(3),    /*   0.125 */
	1.0/VIK_GZ(4),    /*   0.0625 */
	1.0/VIK_GZ(5) };  /*   0.03125 */

static const int num_scales_neg = (sizeof(scale_neg_mpps) / sizeof(scale_neg_mpps[0]));




#define ERROR_MARGIN 0.01





bool GisViewportZoomDialog::custom_zoom_dialog(VikingScale & scale, QWidget * parent)
{
	GisViewportZoomDialog dialog(scale, parent);

	if (QDialog::Accepted == dialog.exec()) {
		scale = dialog.get_value();

		/* There is something strange about argument to qSetRealNumberPrecision().  The precision for
		   fractional part is not enough, I had to add few places for leading digits and decimal dot. */
		qDebug() << SG_PREFIX_I << "Saving custom Viking scale as" << scale;
		return true;
	} else {
		return false;
	}
}




GisViewportZoomDialog::GisViewportZoomDialog(VikingScale & scale, __attribute__((unused)) QWidget * parent)
{
	this->setWindowTitle(QObject::tr("Zoom Factors..."));

	int row = 0;

	QLabel * main_label = new QLabel(QObject::tr("Zoom factor (in meters per pixel):"), this);
	this->grid->addWidget(main_label, row, 0, 1, 2); /* Row span = 1, Column span = 2. */
	row++;


	QLabel * xlabel = new QLabel(QObject::tr("X (easting):"), this);

	this->xspin.setMinimum(SG_GISVIEWPORT_ZOOM_MIN);
	this->xspin.setMaximum(SG_GISVIEWPORT_ZOOM_MAX);
	this->xspin.setSingleStep(1);
	this->xspin.setDecimals(SG_GISVIEWPORT_ZOOM_PRECISION);
	this->xspin.setValue(scale.get_x());

	this->grid->addWidget(xlabel, row, 0);
	this->grid->addWidget(&this->xspin, row, 1);
	row++;


	QLabel * ylabel = new QLabel(QObject::tr("Y (northing):"), this);

	this->yspin.setMinimum(SG_GISVIEWPORT_ZOOM_MIN);
	this->yspin.setMaximum(SG_GISVIEWPORT_ZOOM_MAX);
	this->yspin.setSingleStep(1);
	this->yspin.setDecimals(SG_GISVIEWPORT_ZOOM_PRECISION);
	this->yspin.setValue(scale.get_y());

	this->grid->addWidget(ylabel, row, 0);
	this->grid->addWidget(&this->yspin, row, 1);
	row++;


	this->checkbox.setText(QObject::tr("X and Y zoom factors must be equal"));
	if (scale.x_y_is_equal()) {
		this->checkbox.setChecked(true);
	}
	this->grid->addWidget(&this->checkbox, row, 0, 1, 2); /* Row span = 1, Column span = 2. */
	row++;


	QObject::connect(&this->xspin, SIGNAL (valueChanged(double)), this, SLOT (spin_changed_cb(double)));
	QObject::connect(&this->yspin, SIGNAL (valueChanged(double)), this, SLOT (spin_changed_cb(double)));
}




VikingScale GisViewportZoomDialog::get_value(void) const
{
	return VikingScale(this->xspin.value(), this->yspin.value());
}




void GisViewportZoomDialog::spin_changed_cb(double new_value)
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

	if (angle.y() > 0) {
		result = ZoomOperation::In;
	} else if (angle.y() < 0) {
		result = ZoomOperation::Out;
	} else {
		result = ZoomOperation::Noop;
	}

	return result;
}




bool GisViewportZoom::move_coordinate_to_center(ZoomOperation zoom_operation, GisViewport * gisview, Window * window, const ScreenPos & event_pos)
{
	bool redraw_viewport = false;

	switch (zoom_operation) {
	case ZoomOperation::In:
		gisview->set_center_coord(event_pos);
		gisview->zoom_in_on_center_pixel();
		window->set_dirty_flag(true);
		redraw_viewport = true;
		break;
	case ZoomOperation::Out:
		gisview->set_center_coord(event_pos);
		gisview->zoom_out_on_center_pixel();
		window->set_dirty_flag(true);
		redraw_viewport = true;
		break;
	default:
		break; /* Ignore. */
	};

	return redraw_viewport;
}




bool GisViewportZoom::keep_coordinate_in_center(ZoomOperation zoom_operation, GisViewport * gisview, Window * window, const ScreenPos & center_pos)
{
	bool redraw_viewport = false;

	switch (zoom_operation) {
	case ZoomOperation::In:
		gisview->set_center_coord(center_pos);
		gisview->zoom_in_on_center_pixel();
		window->set_dirty_flag(true);
		redraw_viewport = true;
		break;
	case ZoomOperation::Out:
		gisview->set_center_coord(center_pos);
		gisview->zoom_out_on_center_pixel();
		window->set_dirty_flag(true);
		redraw_viewport = true;
		break;
	default:
		break; /* Ignore. */
	};

	return redraw_viewport;
}




bool GisViewportZoom::keep_coordinate_under_cursor(ZoomOperation zoom_operation, GisViewport * gisview, Window * window, const ScreenPos & event_pos, const ScreenPos & center_pos)
{
	bool redraw_viewport = false;

	switch (zoom_operation) {
	case ZoomOperation::In: {

		/* Here we use event position before zooming in. */
		const Coord cursor_coord = gisview->screen_pos_to_coord(event_pos);

		gisview->zoom_in_on_center_pixel();

		/* Position of event calculated in modified (zoomed in) viewport. */
		ScreenPos orig_pos;
		gisview->coord_to_screen_pos(cursor_coord, orig_pos);

		gisview->set_center_coord(center_pos.x() + (orig_pos.x() - event_pos.x()), center_pos.y() + (orig_pos.y() - event_pos.y()));
		window->set_dirty_flag(true);
		redraw_viewport = true;
		break;
	}
	case ZoomOperation::Out: {

		/* Here we use event position before zooming out. */
		const Coord cursor_coord = gisview->screen_pos_to_coord(event_pos);

		gisview->zoom_out_on_center_pixel();

		/* Position of event calculated in modified (zoomed out) viewport. */
		ScreenPos orig_pos;
		gisview->coord_to_screen_pos(cursor_coord, orig_pos);

		gisview->set_center_coord(center_pos.x() + (orig_pos.x() - event_pos.x()), center_pos.y() + (orig_pos.y() - event_pos.y()));
		window->set_dirty_flag(true);
		redraw_viewport = true;
		break;
	}
	default:
		break;
	}

	return redraw_viewport;
}




VikingScale::VikingScale(const VikingScale & other)
{
	this->x = other.x;
	this->y = other.y;
}




bool VikingScale::x_y_is_equal(void) const
{
	return this->x == this->y;
}




double VikingScale::get_x(void) const
{
	return this->x;
}




double VikingScale::get_y(void) const
{
	return this->y;
}




sg_ret VikingScale::set(double new_x, double new_y)
{
	if (new_x >= SG_GISVIEWPORT_ZOOM_MIN
	    && new_x <= SG_GISVIEWPORT_ZOOM_MAX
	    && new_y >= SG_GISVIEWPORT_ZOOM_MIN
	    && new_y <= SG_GISVIEWPORT_ZOOM_MAX) {

		this->x = new_x;
		this->y = new_y;

		return sg_ret::ok;
	} else {
		return sg_ret::err;
	}
}


bool VikingScale::zoom_in(int factor)
{
	if (this->x >= (SG_GISVIEWPORT_ZOOM_MIN * factor) && this->y >= (SG_GISVIEWPORT_ZOOM_MIN * factor)) {
		this->x /= factor;
		this->y /= factor;
		return true;
	}

	return false;
}




bool VikingScale::zoom_out(int factor)
{
	if (this->x <= (SG_GISVIEWPORT_ZOOM_MAX / factor) && this->y <= (SG_GISVIEWPORT_ZOOM_MAX / factor)) {
		this->x *= factor;
		this->y *= factor;

		return true;
	}

	return false;
}




QString VikingScale::pretty_print(CoordMode coord_mode) const
{
	QString result;

	const QString unit = coord_mode == CoordMode::UTM ? "mpp" : "pixelfact";

	if (this->x_y_is_equal()) {
		if ((int) this->x - this->x < 0.0) {
			result = QObject::tr("%1 %2").arg(this->x, 0, 'f', SG_GISVIEWPORT_ZOOM_PRECISION).arg(unit);
		} else {
			/* xmpp should be a whole number so don't show useless .000 bit. */
		        result = QObject::tr("%1 %2").arg((int) this->x).arg(unit);
		}
	} else {
		result = QObject::tr("%1/%2f %3")
			.arg(this->x, 0, 'f', SG_GISVIEWPORT_ZOOM_PRECISION)
			.arg(this->y, 0, 'f', SG_GISVIEWPORT_ZOOM_PRECISION)
			.arg(unit);
	}

	return result;
}




QString VikingScale::to_string(void) const
{
	QString result;

	if ((int) this->x - this->x < 0.0) {
		result = QObject::tr("%1").arg(this->x, 0, 'f', SG_GISVIEWPORT_ZOOM_PRECISION);
	} else {
		/* xmpp should be a whole number so don't show useless .000 bit. */
		result = QObject::tr("%1").arg((int) this->x);
	}

	return result;
}




QDebug SlavGPS::operator<<(QDebug debug, const VikingScale & viking_scale)
{
	debug << "VikingScale" << viking_scale.get_x() << viking_scale.get_y();
	return debug;
}




bool VikingScale::operator==(const VikingScale & other) const
{
	return this->x == other.x && this->y == other.y;
}




bool VikingScale::value_is_valid(double value)
{
	return value >= SG_GISVIEWPORT_ZOOM_MIN && value <= SG_GISVIEWPORT_ZOOM_MAX;
}




bool VikingScale::is_valid(void) const
{
	return VikingScale::value_is_valid(this->x) && VikingScale::value_is_valid(this->y);
}




TileScale VikingScale::to_tile_scale(void) const
{
	const double mpp = this->x;

	TileScale tile_scale;

	for (int i = 0; i < num_scales; i++) {
		if (std::abs(scale_mpps[i] - mpp) < ERROR_MARGIN) {
			tile_scale.set_scale_value(i);
			tile_scale.set_scale_valid(true);
			return tile_scale;
		}
	}
	for (int i = 0; i < num_scales_neg; i++) {
		if (std::abs(scale_neg_mpps[i] - mpp) < 0.000001) {
			tile_scale.set_scale_value(-i);
			tile_scale.set_scale_valid(true);
			return tile_scale;
		}
	}

	/* In original implementation of the function '255' was the
	   value returned when the loops didn't find any valid
	   value. */
	tile_scale.set_scale_value(255);
	tile_scale.set_scale_valid(false);
	return tile_scale;
}




TileZoomLevel VikingScale::to_tile_zoom_level(void) const
{
	const TileScale tile_scale = this->to_tile_scale();
	return tile_scale.osm_tile_zoom_level();
}




VikingScale::VikingScale(void)
{
}




VikingScale::VikingScale(double value)
{
	this->x = value;
	this->y = value;
}




VikingScale::VikingScale(double new_x, double new_y)
{
	this->x = new_x;
	this->y = new_y;
}




sg_ret VikingScale::get_closest_index(int & result, const std::vector<VikingScale> & viking_scales, const VikingScale & viking_scale)
{
	for (unsigned int idx = 0; idx < viking_scales.size(); idx++) {
		if (viking_scale.get_x() == viking_scales[idx].get_x()) {
			result = idx;
			return sg_ret::ok;
		}
	}

	return sg_ret::err;
}




VikingScale & VikingScale::operator*=(double rhs)
{
	if (this->is_valid()) {
		this->x *= rhs;
		this->y *= rhs;
		return *this; /* Notice that returned value may be invalid. */
	} else {
		return *this;
	}
}




VikingScale & VikingScale::operator/=(double rhs)
{
	if (0.0 == rhs) {
		qDebug() << SG_PREFIX_E << "Can't divide by zero";
		return *this;
	}

	if (this->is_valid()) {
		this->x /= rhs;
		this->y /= rhs;
		return *this; /* Notice that returned value may be invalid. */
	} else {
		return *this;
	}
}




/**
 * Work out the best zoom level for the LatLon area and set the viewport to that zoom level.
 */
sg_ret GisViewportZoom::zoom_to_show_bbox(GisViewport * gisview, CoordMode mode, const LatLonBBox & bbox)
{
	return GisViewportZoom::zoom_to_show_bbox_common(gisview, mode, bbox, 1.0, true);
}




/**
 * Work out the best zoom level for the LatLon area and set the viewport to that zoom level.
 */
sg_ret GisViewportZoom::zoom_to_show_bbox_common(GisViewport * gisview, __attribute__((unused)) CoordMode mode, const LatLonBBox & bbox, double zoom, bool save_position)
{
	/* First set the center [in case previously viewing from elsewhere]. */
	/* Then loop through zoom levels until provided positions are in view. */
	/* This method is not particularly fast - but should work well enough. */

	if (!bbox.is_valid()) {
		qDebug() << SG_PREFIX_E << "bbox is invalid:" << bbox;
		return sg_ret::err;
	}
	if (!bbox.get_center_lat_lon().is_valid()) {
		qDebug() << SG_PREFIX_E << "bbox's center is invalid:" << bbox.get_center_lat_lon();
		return sg_ret::err;
	}
	if (!VikingScale::value_is_valid(zoom)) {
		qDebug() << SG_PREFIX_E << "zoom is invalid:" << zoom;
		return sg_ret::err;
	}
	if (sg_ret::ok != gisview->set_center_coord(bbox.get_center_lat_lon(), save_position)) {
		qDebug() << SG_PREFIX_E << "Failed to set center from coordinate" << bbox.get_center_lat_lon();
		return sg_ret::err;
	}

	/* Never zoom in too far - generally not that useful, as too close! */
	/* Always recalculate the 'best' zoom level. */

	if (sg_ret::ok != gisview->set_viking_scale(zoom)) {
		qDebug() << SG_PREFIX_E << "Failed to set zoom" << zoom;
		return sg_ret::err;
	}


	/* Should only be a maximum of about 18 iterations from min to max zoom levels. */
	while (zoom <= SG_GISVIEWPORT_ZOOM_MAX) {
		const LatLonBBox current_bbox = gisview->get_bbox();
		if (current_bbox.contains_bbox(bbox)) {
			/* Found within zoom level. */
			break;
		}

		/* Try next zoom level. */
		zoom = zoom * 2;
		if (sg_ret::ok != gisview->set_viking_scale(zoom)) {
			qDebug() << SG_PREFIX_E << "Failed to set zoom" << zoom;
			return sg_ret::err;
		}
	}

	return sg_ret::ok;
}
