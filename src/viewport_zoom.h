/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_VIEWPORT_ZOOM_H_
#define _SG_VIEWPORT_ZOOM_H_




#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QCheckBox>
#include <QDebug>




#include "dialog.h"




/* Number of decimal places in presentation of zoom mpp values. */
#define SG_VIEWPORT_ZOOM_PRECISION 8


#define SG_VIEWPORT_ZOOM_MIN (1 / 32.0)
#define SG_VIEWPORT_ZOOM_MAX 32768.0




namespace SlavGPS {




	class Window;
	class Viewport;
	class ScreenPos;
	enum class CoordMode;




	class VikingZoomLevel {
		friend class Viewport;
	public:
		VikingZoomLevel(void);
		VikingZoomLevel(double zoom);
		VikingZoomLevel(double x, double y);
		VikingZoomLevel(const VikingZoomLevel & other);

		TileScale to_tile_scale(void) const;
		TileZoomLevel to_tile_zoom_level(void) const;

		bool set(double x, double y);
		double get_x(void) const;
		double get_y(void) const;
		bool x_y_is_equal(void) const;

		static bool value_is_valid(double zoom);
		bool is_valid(void) const;

		bool zoom_in(int factor);
		bool zoom_out(int factor);

		QString pretty_print(CoordMode coord_mode) const;
		QString to_string(void) const;

		bool operator==(const VikingZoomLevel & other) const;

		/**
		   Find in @viking_zooms the a zoom value that is the
		   closest to @viking_zoom_level.  On success return
		   through @result an index of the found closest zoom.

		   @return 0 on success
		   @return negative value on failure
		*/
		static int get_closest_index(int & result, const std::vector<VikingZoomLevel> & viking_zooms, const VikingZoomLevel & viking_zoom_level);

	private:
		double x = 0.0f;
		double y = 0.0f;
	};
	QDebug operator<<(QDebug debug, const VikingZoomLevel & viking_zoom_level);




	enum class ZoomOperation {
		Noop,    /* Don't change zoom. */
		In,      /* Zoom in. */
		Out      /* Zoom out. */
	};




	ZoomOperation mouse_event_to_zoom_operation(const QMouseEvent * event);
	ZoomOperation wheel_event_to_zoom_operation(const QWheelEvent * event);




	class ViewportZoom {
	public:
		/* Clicked location will be put at the center of
		   viewport (coordinate of a place under cursor before
		   zoom will be placed at the center of viewport after
		   zoom). */
		static bool move_coordinate_to_center(ZoomOperation zoom_operation, Viewport * viewport, Window * window, const ScreenPos & event_pos);

		/* Location at the center of viewport will be
		   preserved (coordinate at the center before the zoom
		   and coordinate at the center after the zoom will be
		   the same). */
		static bool keep_coordinate_in_center(ZoomOperation zoom_operation, Viewport * viewport, Window * window, const ScreenPos & center_pos);

		/* Clicked coordinate will be put after zoom at the same
		   position in viewport as before zoom.  Before zoom
		   the coordinate was under cursor, and after zoom it
		   will be still under cursor. */
		static bool keep_coordinate_under_cursor(ZoomOperation zoom_operation, Viewport * viewport, Window * window, const ScreenPos & event_pos, const ScreenPos & center_pos);
	};




	class ViewportZoomDialog : public BasicDialog {
		Q_OBJECT
	public:
		ViewportZoomDialog() {};
		ViewportZoomDialog(VikingZoomLevel & viking_zoom_level, QWidget * a_parent = NULL);
		~ViewportZoomDialog() {};

		VikingZoomLevel get_value(void) const;

		static bool custom_zoom_dialog(/* in/out */ VikingZoomLevel & viking_zoom_level, QWidget * parent);

	private slots:
		void spin_changed_cb(double new_value);

	private:
		QDoubleSpinBox xspin;
		QDoubleSpinBox yspin;

		QCheckBox checkbox;
	};





} /* namespace SlavGPS */




#endif /* #ifndef _SG_VIEWPORT_ZOOM_H_ */
