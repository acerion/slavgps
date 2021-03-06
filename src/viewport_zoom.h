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
#include "layer_map_tile.h"
#include "globals.h"




/* Number of decimal places in presentation of zoom mpp values. */
#define SG_GISVIEWPORT_ZOOM_PRECISION 8


#define SG_GISVIEWPORT_ZOOM_MIN (1 / 32.0)
#define SG_GISVIEWPORT_ZOOM_MAX 32768.0




namespace SlavGPS {




	class Window;
	class GisViewport;
	class ScreenPos;
	enum class CoordMode;



	/* "Meters per pixel" scale. How many meters on viewport
	   pixmap are represented by one pixel of viewport' pixmap. */
	class VikingScale {
		friend class GisViewport;
	public:
		VikingScale(void);
		VikingScale(double scale);
		VikingScale(double x, double y);
		VikingScale(const VikingScale & other);

		TileScale to_tile_scale(void) const;
		TileZoomLevel to_tile_zoom_level(void) const;

		sg_ret set(double x, double y);
		double get_x(void) const;
		double get_y(void) const;
		bool x_y_is_equal(void) const;

		static bool value_is_valid(double value);
		bool is_valid(void) const;

		bool zoom_in(int factor);
		bool zoom_out(int factor);

		QString pretty_print(CoordMode coord_mode) const;
		QString to_string(void) const;

		bool operator==(const VikingScale & other) const;

		VikingScale & operator*=(double rhs);
		VikingScale & operator/=(double rhs);
		VikingScale operator*(double rhs) const { VikingScale result = *this; result *= rhs; return result; }
		VikingScale operator/(double rhs) const { VikingScale result = *this; result /= rhs; return result; }

		/**
		   Find in @viking_scales the a scale value that is the
		   closest to @viking_scale.  On success return
		   through @result an index of the found closest scale.

		   @return sg_ret::ok on success
		   @return sg_ret::err on failure
		*/
		static sg_ret get_closest_index(int & result, const std::vector<VikingScale> & viking_scales, const VikingScale & viking_scale);

	private:
		/* Invalid values. */
		double x = SG_GISVIEWPORT_ZOOM_MIN - 1;
		double y = SG_GISVIEWPORT_ZOOM_MIN - 1;
	};
	QDebug operator<<(QDebug debug, const VikingScale & viking_scale);




	enum class ZoomDirection {
		None,    /* Don't change zoom. */
		In,      /* Zoom in. */
		Out      /* Zoom out. */
	};




	ZoomDirection mouse_event_to_zoom_direction(const QMouseEvent * event);
	ZoomDirection wheel_event_to_zoom_direction(const QWheelEvent * event);
	ZoomDirection key_sequence_to_zoom_direction(const QKeySequence & seq);
	QString zoom_direction_to_string(ZoomDirection zoom_direction);




	class GisViewportZoomDialog : public BasicDialog {
		Q_OBJECT
	public:
		GisViewportZoomDialog() {};
		GisViewportZoomDialog(VikingScale & viking_scale, QWidget * a_parent = NULL);
		~GisViewportZoomDialog() {};

		VikingScale get_value(void) const;

		static bool custom_zoom_dialog(/* in/out */ VikingScale & viking_scale, QWidget * parent);

	private slots:
		void spin_changed_cb(double new_value);

	private:
		QDoubleSpinBox xspin;
		QDoubleSpinBox yspin;

		QCheckBox checkbox;
	};





} /* namespace SlavGPS */




#endif /* #ifndef _SG_VIEWPORT_ZOOM_H_ */
