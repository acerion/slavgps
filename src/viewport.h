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
 *
 */

#ifndef _SG_VIEWPORT_H_
#define _SG_VIEWPORT_H_




#include <list>
#include <cstdint>

#ifdef SLAVGPS_QT
#include <QPainter>
#include <QPen>
#include <QWidget>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QPen>

#include "slav_qt.h"
#else
#include <gtk/gtk.h>
#endif

#include "coord.h"
#include "bbox.h"




#define VIK_VIEWPORT_MAX_ZOOM 32768.0
#define VIK_VIEWPORT_MIN_ZOOM (1 / 32.0)

/* Used for coord to screen etc, screen to coord. */
#define VIK_VIEWPORT_UTM_WRONG_ZONE -9999999
#define VIK_VIEWPORT_OFF_SCREEN_DOUBLE -9999999.9





/* Drawmode management. */
typedef enum {
	VIK_VIEWPORT_DRAWMODE_UTM = 0,
	VIK_VIEWPORT_DRAWMODE_EXPEDIA,
	VIK_VIEWPORT_DRAWMODE_MERCATOR,
	VIK_VIEWPORT_DRAWMODE_LATLON,
	VIK_VIEWPORT_NUM_DRAWMODES      /*< skip >*/
} VikViewportDrawMode;




namespace SlavGPS {
#ifdef SLAVGPS_QT
	typedef QPixmap Pixmap;
#else
	typedef GdkPixmap Pixmap;
#endif




	class Window;
	class Layer;




#ifdef SLAVGPS_QT
	class Viewport : public QWidget {
	Q_OBJECT
#else
	class Viewport {
#endif

	public:
#ifdef SLAVGPS_QT
		Viewport(Window * parent);
#else
		Viewport();
#endif
		~Viewport();

		/* Viking initialization. */
		bool configure();
		void configure_manually(int width, unsigned int height); /* for off-screen viewports */

#ifdef SLAVGPS_QT
		void paintEvent(QPaintEvent * event);
		void resizeEvent(QResizeEvent * event);
		void mousePressEvent(QMouseEvent * event);
		void mouseMoveEvent(QMouseEvent * event);
		void mouseReleaseEvent(QMouseEvent * event);
		void wheelEvent(QWheelEvent * event);
#endif
		/* Drawing primitives. */

#ifndef SLAVGPS_QT
		void draw_line(GdkGC * gc, int x1, int y1, int x2, int y2);
		void draw_string(GdkFont * font, GdkGC * gc, int x1, int y1, char const * string);
		void draw_arc(GdkGC * gc, bool filled, int x, int y, int width, int height, int angle1, int angle2);
		void draw_polygon(GdkGC * gc, bool filled, GdkPoint * points, int npoints);
		void draw_layout(GdkGC * gc, int x, int y, PangoLayout * layout);

		void draw_pixbuf(GdkPixbuf * pixbuf, int src_x, int src_y, int dest_x, int dest_y, int region_width, int region_height);
#else
		void draw_line(const QPen & pen, int x1, int y1, int x2, int y2);
		void draw_rectangle(const QPen & pen, bool filled, int x, int y, int width, int height);
		void fill_rectangle(const QColor & color, int x, int y, int width, int height);
		void draw_string(GdkFont * font, GdkGC * gc, int x1, int y1, char const * string);
		void draw_arc(GdkGC * gc, bool filled, int x, int y, int width, int height, int angle1, int angle2);
		void draw_polygon(GdkGC * gc, bool filled, QPointF * points, int npoints);
		void draw_layout(GdkGC * gc, int x, int y, PangoLayout * layout);
		void draw_pixbuf(GdkPixbuf * pixbuf, int src_x, int src_y, int dest_x, int dest_y, int region_width, int region_height);
#endif

		/* Run this before drawing a line. Viewport::draw_line() runs it for you. */
		static void clip_line(int * x1, int * y1, int * x2, int * y2);

		VikCoordMode get_coord_mode(); // const
		VikCoord * get_center(); // const
		void set_coord_mode(VikCoordMode mode_);



		bool go_back();
		bool go_forward();
		bool back_available(); // const
		bool forward_available();
		void show_centers(GtkWindow *parent);
		void print_centers(char * label);


		/* Viewport position. */
		void set_center_coord(const VikCoord * coord, bool save_position);
		void set_center_screen(int x, int y);
		void center_for_zonen(struct UTM *center, int zone);
		char leftmost_zone();
		char rightmost_zone();
		void set_center_utm(const struct UTM * utm, bool save_position);
		void set_center_latlon(const struct LatLon * ll, bool save_position);
		void corners_for_zonen(int zone, VikCoord * ul, VikCoord * br);
		void get_min_max_lat_lon(double * min_lat, double * max_lat, double * min_lon, double * max_lon);
		void get_bbox(LatLonBBox * bbox);
		void get_bbox_strings(LatLonBBoxStrings * bbox_strings);

		int get_width();
		int get_height();

		/* Coordinate transformations. */
		void screen_to_coord(int x, int y, VikCoord * coord);
		void coord_to_screen(const VikCoord * coord, int * x, int * y);

		/* Viewport scale. */
		void set_ympp(double ympp);
		void set_xmpp(double xmpp);
		double get_ympp();
		double get_xmpp();
		void set_zoom(double mpp);
		double get_zoom();
		void zoom_in();
		void zoom_out();


		void reset_copyrights();
		void add_copyright(char const * copyright);

		void reset_logos();
		void add_logo(const GdkPixbuf * logo);


		void set_highlight_color(char const * color);
		char const * get_highlight_color();
		QColor * get_highlight_gdkcolor();
		void set_highlight_gdkcolor(QColor * color);
		QPen * get_gc_highlight();
		void set_highlight_thickness(int thickness);



		/* Color/graphics context management. */
		void set_background_color(char const * color);
		const char * get_background_color();
		QColor * get_background_gdkcolor();
		void set_background_gdkcolor(QColor * color);


		double calculate_utm_zone_width(); // private
		void utm_zone_check(); // private


		/* Viewport features. */
		void draw_scale();
		void set_draw_scale(bool draw_scale);
		bool get_draw_scale();
		void draw_copyright();
		void draw_centermark();
		void set_draw_centermark(bool draw_centermark);
		bool get_draw_centermark();
		void draw_logo();
		void set_draw_highlight(bool draw_highlight);
		bool get_draw_highlight();


		void clear();


		bool is_one_zone();
		char const * get_drawmode_name(VikViewportDrawMode mode);
		void set_drawmode(VikViewportDrawMode drawmode);
		VikViewportDrawMode get_drawmode();
		/* Do not forget to update Viewport::get_drawmode_name() if you modify VikViewportDrawMode. */




		GdkGC * new_gc(char const * colorname, int thickness);
		QPen * new_pen(char const * colorname, int width);
		GdkGC * new_gc_from_color(GdkColor * color, int thickness);
		QPen * new_pen_from_color(const QColor & color, int width);



		/* Viewport buffer management/drawing to screen. */
		SlavGPS::Pixmap * get_pixmap(); /* Get pointer to drawing buffer. */
		void sync();              /* Draw buffer to window. */
		void pan_sync(int x_off, int y_off);



		/* Utilities. */
		void compute_bearing(int x1, int y1, int x2, int y2, double *angle, double *baseangle);



		/* Trigger stuff. */
		void set_trigger(Layer * trigger);
		Layer * get_trigger();

		void snapshot_save();
		void snapshot_load();

		void set_half_drawn(bool half_drawn);
		bool get_half_drawn();

		GtkWidget * get_toolkit_widget(void);
		GtkWindow * get_toolkit_window(void);
		void * get_toolkit_object(void);
		Window * get_window(void);




		/* Whether or not to display OSD info. */
		bool do_draw_scale = true;
		bool do_draw_centermark = true;
		bool do_draw_highlight = true;

		GSList * copyrights = NULL;
		GSList * logos = NULL;


		double xmpp, ympp;
		double xmfactor, ymfactor;


		VikCoordMode coord_mode;
		VikCoord center;

		/* centers_iter++ means moving forward in history. Thus prev(centers->end()) is the newest item.
		   centers_iter-- means moving backward in history. Thus centers->begin() is the oldest item (in the beginning of history). */
		std::list<Coord *> * centers;  /* The history of requested positions. */
		std::list<Coord *>::iterator centers_iter; /* Current position within the history list. */

		unsigned int centers_max;      /* configurable maximum size of the history list. */
		unsigned int centers_radius;   /* Metres. */

		SlavGPS::Pixmap * scr_buffer = NULL;
		int size_width = 0;
		int size_height = 0;
		/* Half of the normal width and height. */
		int size_width_2 = 0;
		int size_height_2 = 0;

		void update_centers();


		double utm_zone_width;
		bool one_utm_zone;

		/* Subset of coord types. lat lon can be plotted in 2 ways, google or exp. */
		VikViewportDrawMode drawmode;


		/* For scale and center mark. */
		QPen pen_marks_bg;
		QPen pen_marks_fg;

		QPen * background_gc = NULL;
		QColor background_color;

		QPen * highlight_gc = NULL;
		QColor highlight_color;


		/* Trigger stuff. */
		Layer * trigger = NULL;
		GdkPixmap * snapshot_buffer = NULL;
		bool half_drawn = false;

	private:
		void free_center(std::list<Coord *>::iterator iter);
		void init_drawing_area(void);

		GtkDrawingArea * drawing_area_ = NULL; /* Toolkit-specific drawing area. */
		char type_string[30] = { 0 };

		Window * window = NULL;

	signals:
		void updated_center(void);


	public slots:
		bool configure_cb(void);
	};





	void vik_gc_get_fg_color(GdkGC * gc, GdkColor * dest); /* Warning: could be slow, don't use obsessively. */
	GdkFunction vik_gc_get_function(GdkGC * gc);
	void viewport_init(void);




} /* namespace SlavGPS */




void vik_viewport_add_copyright_cb(SlavGPS::Viewport * viewport, char const * copyright);




#endif /* _SG_VIEWPORT_H_ */
