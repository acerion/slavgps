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

#ifndef _VIKING_VIEWPORT_H
#define _VIKING_VIEWPORT_H
/* Requires <gtk/gtk.h> or glib, and coords.h*/

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <stdbool.h>
#include <stdint.h>


#include "vikcoord.h"


#ifdef __cplusplus
extern "C" {
#endif



#define VIK_VIEWPORT_TYPE            (vik_viewport_get_type ())
#define VIK_VIEWPORT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_VIEWPORT_TYPE, VikViewport))
#define VIK_VIEWPORT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_VIEWPORT_TYPE, VikViewportClass))
#define VIK_IS_VIEWPORT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_VIEWPORT_TYPE))
#define VIK_IS_VIEWPORT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_VIEWPORT_TYPE))

#define VIK_VIEWPORT_MAX_ZOOM 32768.0
#define VIK_VIEWPORT_MIN_ZOOM (1 / 32.0)

/* used for coord to screen etc, screen to coord */
#define VIK_VIEWPORT_UTM_WRONG_ZONE -9999999
#define VIK_VIEWPORT_OFF_SCREEN_DOUBLE -9999999.9



/* drawmode management */
typedef enum {
	VIK_VIEWPORT_DRAWMODE_UTM=0,
	VIK_VIEWPORT_DRAWMODE_EXPEDIA,
	VIK_VIEWPORT_DRAWMODE_MERCATOR,
	VIK_VIEWPORT_DRAWMODE_LATLON,
	VIK_VIEWPORT_NUM_DRAWMODES      /*< skip >*/
} VikViewportDrawMode;


namespace SlavGPS {

	class Viewport {

	public:

		Viewport();

		/* Viking initialization */
		bool configure();
		void configure_manually(int width, unsigned int height); /* for off-screen viewports */

		/* Drawing primitives */

		void draw_line(GdkGC *gc, int x1, int y1, int x2, int y2);
		void draw_rectangle(GdkGC *gc, bool filled, int x1, int y1, int x2, int y2);
		void draw_string(GdkFont *font, GdkGC *gc, int x1, int y1, const char *string);
		void draw_arc(GdkGC *gc, bool filled, int x, int y, int width, int height, int angle1, int angle2);
		void draw_polygon(GdkGC *gc, bool filled, GdkPoint *points, int npoints);
		void draw_layout(GdkGC *gc, int x, int y, PangoLayout *layout);

		void draw_pixbuf(GdkPixbuf *pixbuf, int src_x, int src_y, int dest_x, int dest_y, int region_width, int region_height);

		/* run this before drawing a line. vik_viewport_draw_line runs it for you */
		static void clip_line(int *x1, int *y1, int *x2, int *y2);

		VikCoordMode get_coord_mode(); // const
		VikCoord * get_center(); // const
		void set_coord_mode(VikCoordMode mode_);



		bool go_back();
		bool go_forward();
		bool back_available(); // const
		bool forward_available();
		void show_centers(GtkWindow *parent);


		/* viewport position */
		void set_center_coord(const VikCoord *coord, bool save_position);
		void set_center_screen(int x, int y);
		void center_for_zonen(struct UTM *center, int zone);
		char leftmost_zone();
		char rightmost_zone();
		void set_center_utm(const struct UTM *utm, bool save_position);
		void set_center_latlon(const struct LatLon *ll, bool save_position);
		void corners_for_zonen(int zone, VikCoord *ul, VikCoord *br);
		void get_min_max_lat_lon(double *min_lat, double *max_lat, double *min_lon, double *max_lon);

		int get_width();
		int get_height();

		/* coordinate transformations */
		void screen_to_coord(int x, int y, VikCoord *coord);
		void coord_to_screen(const VikCoord *coord, int *x, int *y);

		/* viewport scale */
		void set_ympp(double ympp);
		void set_xmpp(double xmpp);
		double get_ympp();
		double get_xmpp();
		void set_zoom(double mpp);
		double get_zoom();
		void zoom_in();
		void zoom_out();


		void reset_copyrights();
		void add_copyright(const char *copyright);

		void reset_logos();
		void add_logo(const GdkPixbuf *logo);


		void set_highlight_color(const char *color);
		const char *get_highlight_color();
		GdkColor *get_highlight_gdkcolor();
		void set_highlight_gdkcolor(GdkColor *);
		GdkGC* get_gc_highlight();
		void set_highlight_thickness(int thickness);



		/* Color/graphics context management */
		void set_background_color(const char *color);
		const char *get_background_color();
		GdkColor *get_background_gdkcolor();
		void set_background_gdkcolor(GdkColor *);


		double calculate_utm_zone_width(); // private
		void utm_zone_check(); // private


		/* Viewport features */
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
		const char * get_drawmode_name(VikViewportDrawMode mode);
		void set_drawmode(VikViewportDrawMode drawmode);
		VikViewportDrawMode get_drawmode();
		/* Do not forget to update vik_viewport_get_drawmode_name() if you modify VikViewportDrawMode */




		GdkGC * new_gc(char const * colorname, int thickness);
		GdkGC * new_gc_from_color(GdkColor * color, int thickness);



		/* Viewport buffer management/drawing to screen */
		GdkPixmap * get_pixmap(); /* get pointer to drawing buffer */
		void sync();             /* draw buffer to window */
		void pan_sync(int x_off, int y_off);



		/* Utilities */
		void compute_bearing(int x1, int y1, int x2, int y2, double *angle, double *baseangle);



		/* Trigger stuff. */
		void set_trigger(void * trigger);
		void * get_trigger();

		void snapshot_save();
		void snapshot_load();

		void set_half_drawn(bool half_drawn);
		bool get_half_drawn();





		/* Wether or not display OSD info */
		bool do_draw_scale;
		bool do_draw_centermark;
		bool do_draw_highlight;

		GSList *copyrights;
		GSList *logos;


		double xmpp, ympp;
		double xmfactor, ymfactor;


		VikCoordMode coord_mode;
		VikCoord center;
		GList *centers;         // The history of requested positions (of VikCoord type)

		unsigned int centers_index;    // current position within the history list
		unsigned int centers_max;      // configurable maximum size of the history list
		unsigned int centers_radius;   // Metres

		GdkPixmap *scr_buffer;
		int width;
		int height;
		// Half of the normal width and height
		int width_2;
		int height_2;

		void update_centers();
		void free_centers(unsigned int start);
		void free_center(unsigned int index);


		double utm_zone_width;
		bool one_utm_zone;

		/* subset of coord types. lat lon can be plotted in 2 ways, google or exp. */
		VikViewportDrawMode drawmode;


		GdkGC * background_gc;
		GdkColor background_color;
		GdkGC *scale_bg_gc;
		GdkGC *highlight_gc;
		GdkColor highlight_color;




		void * vvp; /* Parent VikViewport. */


		/* trigger stuff */
		void * trigger; /* Usually pointer to VikLayer. */
		GdkPixmap * snapshot_buffer;
		bool half_drawn;
	};





} /* namespace SlavGPS */


using namespace SlavGPS;


struct _VikViewport {
	GtkDrawingArea drawing_area;

	SlavGPS::Viewport port;
};

/* Glib type inheritance and initialization */
typedef struct _VikViewport VikViewport;
typedef struct _VikViewportClass VikViewportClass;

struct _VikViewportClass {
	GtkDrawingAreaClass drawing_area_class;
	void (*updated_center) (VikViewport *vw);
};
GType vik_viewport_get_type ();





/* Viking initialization */
VikViewport *vik_viewport_new ();



void vik_gc_get_fg_color(GdkGC * gc, GdkColor * dest); /* warning: could be slow, don't use obsessively */
GdkFunction vik_gc_get_function(GdkGC * gc);


void vik_viewport_add_copyright_cb(VikViewport * vvp, const char * copyright);
bool vik_viewport_configure_cb(VikViewport * vpp);

#ifdef __cplusplus
}
#endif



#endif
