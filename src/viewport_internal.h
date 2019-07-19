/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _SG_VIEWPORT_INTERNAL_H_
#define _SG_VIEWPORT_INTERNAL_H_




#include <list>
#include <cstdint>




#include <QPen>
#include <QWidget>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QPrinter>




#include "viewport.h"
#include "viewport_decorations.h"
#include "viewport_zoom.h"
#include "viewport_pixmap.h"
#include "coord.h"
#include "bbox.h"




namespace SlavGPS {




	class Window;
	class Layer;
	class ViewportPixmap;




	class CenterCoords : public std::list<Coord> {
	public:
		CenterCoords();

		void remove_item(std::list<Coord>::iterator iter);

		/* current_iter++ means moving forward in history. Thus prev(center_coords->end()) is the newest item.
		   current_iter-- means moving backward in history. Thus center_coords->begin() is the oldest item (in the beginning of history). */
		std::list<Coord>::iterator current_iter; /* Current position within the history list. */
		int max_items;      /* Configurable maximum size of the history list. */
		int radius;   /* Metres. */
	};




	/* GIS-aware viewport. Viewport that knows something about
	   coordinates etc. */
	class GisViewport : public ViewportPixmap {
		Q_OBJECT

		friend class GisViewportDecorations;
	public:

		/* ******** Methods that definitely should be in this class. ******** */

		GisViewport(int left = 0, int right = 0, int top = 0, int bottom = 0, QWidget * parent = NULL);
		~GisViewport();


		void draw_bbox(const LatLonBBox & bbox, const QPen & pen);
		sg_ret set_bbox(const LatLonBBox & bbox);
		/* Positive value of a margin mean that we want to shrink bbox from specified side. */
		LatLonBBox get_bbox(int margin_left = 0, int margin_right = 0, int margin_top = 0, int margin_bottom = 0) const;

		CoordMode get_coord_mode(void) const;
		void set_coord_mode(CoordMode mode);

		bool go_back(void);
		bool go_forward(void);
		bool back_available(void) const;
		bool forward_available(void) const;

		const Coord & get_center_coord(void) const;
		std::list<QString> get_center_coords_list(void) const;
		void show_center_coords(Window * parent) const;
		void print_center_coords(const QString & label) const;


		/* Coordinate transformations.
		   Pixel coordinates passed to the functions are in
		   Qt's coordinate system, where beginning (0,0 point)
		   is in top-left corner. */
		Coord screen_pos_to_coord(ScreenPosition screen_pos) const;
		Coord screen_pos_to_coord(fpixel x, fpixel y) const;
		Coord screen_pos_to_coord(const ScreenPos & pos) const;

		/* Coordinate transformations. */
		sg_ret coord_to_screen_pos(const Coord & coord, fpixel * x, fpixel * y) const;
		sg_ret coord_to_screen_pos(const Coord & coord, ScreenPos & pos) const;


		/* GisViewport's zoom. */
		void zoom_in(void);
		void zoom_out(void);

		sg_ret set_viking_scale(double new_value);
		sg_ret set_viking_scale_x(double new_value);
		sg_ret set_viking_scale_y(double new_value);
		sg_ret set_viking_scale(const VikingScale & new_value);
		const VikingScale & get_viking_scale(void) const;

		void set_draw_mode(GisViewportDrawMode drawmode);
		GisViewportDrawMode get_draw_mode(void) const;

		sg_ret utm_recalculate_current_center_coord_for_other_zone(UTM & center_in_other_zone, int zone);
		sg_ret get_corners_for_zone(Coord & coord_ul, Coord & coord_br, int zone);

		int get_leftmost_zone(void) const;
		int get_rightmost_zone(void) const;
		bool get_is_one_utm_zone(void) const;



		sg_ret set_center_coord(const Coord & coord, bool save_position = true);
		sg_ret set_center_coord(const UTM & utm, bool save_position = true);
		sg_ret set_center_coord(const LatLon & lat_lon, bool save_position = true);

		/* These pixel coordinates should be in Qt's
		   coordinate system, where beginning (point 0,0) is
		   in upper-left corner. */
		sg_ret set_center_coord(fpixel x, fpixel y);
		sg_ret set_center_coord(const ScreenPos & pos);


		/* These methods belong to GisViewport because only
		   this class knows something about distances in
		   meters. */
		double central_get_height_m(void) const;
		double central_get_width_m(void) const;





		/* ******** Other methods. ******** */


		/* GisViewport module initialization function. */
		static void init(void);


		void resizeEvent(QResizeEvent * event);
		void mousePressEvent(QMouseEvent * event); /* Double click is handled through event filter. */
		void mouseMoveEvent(QMouseEvent * event);
		void mouseReleaseEvent(QMouseEvent * event);
		void wheelEvent(QWheelEvent * event);

		void dragEnterEvent(QDragEnterEvent * event);
		void dropEvent(QDropEvent * event);

		void draw_mouse_motion_cb(QMouseEvent * event);



		/* Get cursor position of a mouse event.  Returned
		   position is in "beginning is in bottom-left corner"
		   coordinate system. */
		sg_ret get_cursor_pos_cbl(QMouseEvent * ev, ScreenPos & screen_pos) const;



		/*
		  Add given @attribution to list of attributions displayed in viewport.
		*/
		sg_ret add_attribution(const QString & attribution);
		sg_ret add_logo(const GisViewportLogo & logo);



		void draw_decorations(void);
		void set_center_mark_visibility(bool new_state);
		bool get_center_mark_visibility(void) const;
		void set_scale_visibility(bool new_state);
		bool get_scale_visibility(void) const;


		void clear(void);

		void request_redraw(const QString & trigger_descr);


		/* Trigger stuff. */
		void set_trigger(Layer * trigger);
		Layer * get_trigger(void) const;


		void set_half_drawn(bool half_drawn);
		bool get_half_drawn(void) const;


		/* Last arg may be invalid (::is_valid() returns
		   false) - the function will then use its standard
		   algorithm for determining expected zoom of scaled
		   viewport. */
		GisViewport * create_scaled_viewport(Window * window, int target_width, int target_height, const VikingScale & expected_viking_scale);



		Window * get_window(void) const;






		/* ******** Class variables that definitely should be in this class. ******** */



		Coord center_coord;
		CenterCoords center_coords;  /* The history of requested positions. */


		CoordMode coord_mode = CoordMode::LatLon;
		GisViewportDrawMode draw_mode = GisViewportDrawMode::Mercator;


		double utm_zone_width = 0.0;
		bool is_one_utm_zone;


		GisViewportDomain x_domain = GisViewportDomain::Max;
		GisViewportDomain y_domain = GisViewportDomain::Max;

		HeightUnit height_unit;
		DistanceUnit distance_unit;
		SpeedUnit speed_unit;



		/* ******** Other class variables. ******** */


		/* Whether or not to display some decorations. */
		bool scale_visibility = true;
		bool center_mark_visibility = true;


		/* Trigger stuff. */
		Layer * trigger = NULL;
		bool half_drawn = false;

		char debug[100] = { 0 };

	private:
		/* ******** Methods that definitely should be in this class. ******** */

		/* Store the current center coordinate into the history list
		   and emit a signal to notify clients the list has been updated. */
		void save_current_center_coord(void);


		double calculate_utm_zone_width(void) const;
		void utm_zone_check(void);



		/* ******** Other methods. ******** */



		/* ******** Class variables that definitely should be in this class. ******** */

		VikingScale viking_scale;
		double xmfactor = 0.0f;
		double ymfactor = 0.0f;

		GisViewportDecorations decorations;


		/* ******** Other class variables. ******** */

		Window * window = NULL;


	signals:
		/* ******** Signals that definitely should be in this class. ******** */

		void list_of_center_coords_changed(void);

		/**
		   To be emitted when action initiated in GisViewport
		   has changed center of viewport or zoom of viewport.
		*/
		void center_coord_or_zoom_changed(void);


		/* ******** Other signals. ******** */
		void cursor_moved(GisViewport * gisview, QMouseEvent * event);
		void button_released(GisViewport * gisview, QMouseEvent * event);




	public slots:
		bool print_cb(QPrinter *);

	protected:
		bool eventFilter(QObject * object, QEvent * event);
	};




	class ArrowSymbol {
	public:
		/**
		   @param blades_width_degrees - how widely the arrow blades will be spread?
		*/
		ArrowSymbol(double blades_width_degrees = 15.0, int size_factor = 1);

		/**
		   @param direction decides in which direction the arrow head will be pointing. -1 or +1.
		*/
		void set_arrow_tip(int x, int y, int direction = 1);

		sg_ret paint(QPainter & painter, double dx, double dy);

	private:
		double cosine_factor = 0.0;
		double sine_factor = 0.0;

		int tip_x = 0;
		int tip_y = 0;
		int direction = 1;
	};




} /* namespace SlavGPS */




#endif /* _SG_VIEWPORT_INTERNAL_H_ */
