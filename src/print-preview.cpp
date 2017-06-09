/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/*
 * Modified for viking by Quy Tonthat <qtonthat@gmail.com>
 */



#include "print-preview.h"
#include "slav_qt.h"




#define DRAWING_AREA_SIZE 200




static void vik_print_preview_size_allocate(GtkWidget * widget, GtkAllocation * allocation, PrintPreview * preview);
static void vik_print_preview_realize(GtkWidget * widget);
static bool vik_print_preview_event(GtkWidget * widget, GdkEvent * event, PrintPreview * preview);
static bool vik_print_preview_expose_event(GtkWidget * widget, GdkEventExpose * eevent, PrintPreview * preview);




static void vik_print_preview_init(PrintPreview * preview)
{
#ifdef K
	preview->area = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER (preview), preview->area);
	gtk_widget_show(preview->area);

	gtk_widget_add_events(GTK_WIDGET (preview->area), GDK_BUTTON_PRESS_MASK);

	QObject::connect(preview->area, SIGNAL("size-allocate"), preview, SLOT (vik_print_preview_size_allocate));
	QObject::connect(preview->area, SIGNAL("realize"), NULL, SLOT (vik_print_preview_realize));
	QObject::connect(preview->area, SIGNAL("event"), preview, SLOT (vik_print_preview_event));
	QObject::connect(preview->area, SIGNAL("expose-event"), preview, SLOT (vik_print_preview_expose_event));
#endif
}




PrintPreview::~PrintPreview()
{
	if (this->drawable) {
		this->drawable = NULL; /* Just a reference. */
	}

	if (this->pixmap) {
		delete this->pixmap;
		this->pixmap = NULL;
	}

	if (this->page) {
		delete this->page;
		this->page = NULL;
	}
}




/**
 * @page: page setup
 * @drawable_id: the drawable to print
 *
 * Creates a new #PrintPreview widget.
 *
 * Return value: the new #PrintPreview widget.
 **/
PrintPreview::PrintPreview(GtkPageSetup * page_, QPixmap * drawable_)
{
	this->drawable = drawable_;

#ifdef K
	if (page) {
		this->page = gtk_page_setup_copy(page);
	} else {
		this->page = gtk_page_setup_new();
	}

	float ratio = (gtk_page_setup_get_paper_width(this->page, GTK_UNIT_POINTS) /
		 gtk_page_setup_get_paper_height(preview->page, GTK_UNIT_POINTS));

	gtk_aspect_frame_set(GTK_ASPECT_FRAME (this), 0.5, 0.5, ratio, false);

	gtk_widget_set_size_request(this->area, DRAWING_AREA_SIZE, DRAWING_AREA_SIZE);
#endif
}




/**
 * @xres: the X resolution
 * @yres: the Y resolution
 *
 * Sets the resolution of the image/drawable displayed by the
 * #PrintPreview.
 **/
void PrintPreview::set_image_dpi(double xres, double yres)
{
	if (this->image_xres != xres || this->image_yres != yres) {
		this->image_xres = xres;
		this->image_yres = yres;

		this->queue_draw();
	}
}




/**
 * @page: the page setup to use
 *
 * Sets the page setup to use by the #PrintPreview.
 **/
void PrintPreview::set_page_setup(GtkPageSetup * page_)
{
#ifdef K
	if (this->page) {
		g_object_unref(this->page);
	}

	this->page = gtk_page_setup_copy(page);

	float ratio = (gtk_page_setup_get_paper_width(page, GTK_UNIT_POINTS) /
		 gtk_page_setup_get_paper_height(page, GTK_UNIT_POINTS));

	gtk_aspect_frame_set(GTK_ASPECT_FRAME (preview), 0.5, 0.5, ratio, false);

	this->queue_draw();
#endif
}




/**
 * @offset_x: the X offset
 * @offset_y: the Y offset
 *
 * Sets the offsets of the image/drawable displayed by the #PrintPreview.
 * It does not emit the "offsets-changed" signal.
 **/
void PrintPreview::set_image_offsets(double offset_x, double offset_y)
{
	this->image_offset_x = offset_x;
	this->image_offset_y = offset_y;

	this->queue_draw();
}




/**
 * @offset_x_max: the maximum X offset allowed
 * @offset_y_max: the maximum Y offset allowed
 *
 * Sets the maximum offsets of the image/drawable displayed by the
 * #PrintPreview.  It does not emit the "offsets-changed" signal.
 **/
void PrintPreview::set_image_offsets_max(double offset_x_max, double offset_y_max)
{
	this->image_offset_x_max = offset_x_max;
	this->image_offset_y_max = offset_y_max;

	this->queue_draw();
}




/**
 * @full_page: true to ignore the page margins
 *
 * If @full_page is true, the page margins are ignored and the full page
 * can be used to setup printing.
 **/
void PrintPreview::set_use_full_page(bool full_page)
{
	this->use_full_page = full_page;

	this->queue_draw();
}




static void vik_print_preview_realize(GtkWidget * widget)
{
#ifdef K
	GdkCursor * cursor = gdk_cursor_new_for_display(gtk_widget_get_display(widget),
							GDK_FLEUR);
	gdk_window_set_cursor(gtk_widget_get_window(widget), cursor);
	gdk_cursor_unref(cursor);
#endif
}




static bool vik_print_preview_event(GtkWidget * widget, GdkEvent * event, PrintPreview * preview)
{
	static double orig_offset_x = 0.0;
	static double orig_offset_y = 0.0;
	static int    start_x       = 0;
	static int    start_y       = 0;

#ifdef K

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		gdk_pointer_grab(gtk_widget_get_window(widget), false,
				 (GdkEventMask) (GDK_BUTTON1_MOTION_MASK |
						 GDK_BUTTON_RELEASE_MASK),
				 NULL, NULL, event->button.time);

		orig_offset_x = preview->image_offset_x;
		orig_offset_y = preview->image_offset_y;

		start_x = event->button.x;
		start_y = event->button.y;

		preview->dragging = true;
		break;

	case GDK_MOTION_NOTIFY:
		double scale = preview->get_scale();

		double offset_x = (orig_offset_x + (event->motion.x - start_x) / scale);
		double offset_y = (orig_offset_y + (event->motion.y - start_y) / scale);

		offset_x = CLAMP (offset_x, 0, preview->image_offset_x_max);
		offset_y = CLAMP (offset_y, 0, preview->image_offset_y_max);

		if (preview->image_offset_x != offset_x ||
		    preview->image_offset_y != offset_y) {
			preview->set_image_offsets(offset_x, offset_y);

			g_signal_emit(preview,
				      vik_print_preview_signals[OFFSETS_CHANGED], 0,
				      preview->image_offset_x, preview->image_offset_y);
		}
		break;

	case GDK_BUTTON_RELEASE:
		gdk_display_pointer_ungrab(gtk_widget_get_display(widget),
					   event->button.time);
		start_x = start_y = 0;
		preview->dragging = false;

		preview->queue_draw();
		break;

	default:
		break;
	}
#endif
	return false;
}




static QPixmap * get_thumbnail(QPixmap * drawable, int thumb_width, int thumb_height)
{
	int width, height;
	QPixmap * thumbnail = NULL;

#ifdef K
	gdk_drawable_get_size(drawable, &width, &height);
	QPixmap * pixmap = gdk_pixbuf_get_from_drawable(NULL, drawable,
							NULL, 0, 0, 0, 0, width, height);
	thumbnail = gdk_pixbuf_scale_simple(pixmap, thumb_width, thumb_height,
					    GDK_INTERP_BILINEAR);
	g_object_unref(pixmap);
#endif
	return thumbnail;

}




static bool vik_print_preview_expose_event(GtkWidget * widget, GdkEventExpose * eevent, PrintPreview * preview)
{
	double  left_margin;
	double  right_margin;
	double  top_margin;
	double  bottom_margin;

#ifdef K
	double paper_width = gtk_page_setup_get_paper_width(preview->page,
						     GTK_UNIT_POINTS);
	double paper_height = gtk_page_setup_get_paper_height(preview->page,
						       GTK_UNIT_POINTS);
	preview->get_page_margins(&left_margin, &right_margin, &top_margin, &bottom_margin);

	cairo_t cr = gdk_cairo_create(gtk_widget_get_window(widget));

	double scale = preview->get_scale();

	/* draw background */
	cairo_scale(cr, scale, scale);
	gdk_cairo_set_source_color(cr, &gtk_widget_get_style(widget)->white);
	cairo_rectangle(cr, 0, 0, paper_width, paper_height);
	cairo_fill(cr);

	/* draw page_margins */
	gdk_cairo_set_source_color(cr, &gtk_widget_get_style(widget)->black);
	cairo_rectangle(cr,
			left_margin,
			top_margin,
			paper_width - left_margin - right_margin,
			paper_height - top_margin - bottom_margin);
	cairo_stroke(cr);

	if (preview->dragging) {
		int width, height;
		gdk_drawable_get_size(preview->drawable, &width, &height);
		cairo_rectangle(cr,
				left_margin + preview->image_offset_x,
				top_margin  + preview->image_offset_y,
				(double) width  * 72.0 / preview->image_xres,
				(double) height * 72.0 / preview->image_yres);
		cairo_stroke(cr);
	} else {
		QPixmap * drawable = preview->drawable;

		/* draw image */
		cairo_translate(cr,
				left_margin + preview->image_offset_x,
				top_margin  + preview->image_offset_y);

		if (preview->pixmap == NULL) {
			GtkAllocation allocation;
			gtk_widget_get_allocation( widget, &allocation );
			int width  = MIN (allocation.width, 1024);
			int height = MIN (allocation.height, 1024);

			preview->pixmap = get_thumbnail(drawable, width, height);
		}

		if (preview->pixmap != NULL) {
			int width, height;
			gdk_drawable_get_size(drawable, &width, &height);

			double scale_x = ((double) width /
					  gdk_pixbuf_get_width(preview->pixmap));
			double scale_y = ((double) height /
					  gdk_pixbuf_get_height(preview->pixmap));

			scale_x = scale_x * 72.0 / preview->image_xres;
			scale_y = scale_y * 72.0 / preview->image_yres;

			cairo_scale(cr, scale_x, scale_y);

			gdk_cairo_set_source_pixbuf(cr, preview->pixmap, 0, 0);

			cairo_paint(cr);
		}
	}

	cairo_destroy(cr);

#endif
	return false;
}




double PrintPreview::get_scale(void)
{
#ifdef K
	double scale_x = ((double) this->area->allocation.width /
			  gtk_page_setup_get_paper_width(this->page, GTK_UNIT_POINTS));

	double scale_y = ((double) this->area->allocation.height /
			  gtk_page_setup_get_paper_height(this->page, GTK_UNIT_POINTS));

	return MIN (scale_x, scale_y);
#endif
}




static void vik_print_preview_size_allocate(GtkWidget        *widget,
					    GtkAllocation    *allocation,
					    PrintPreview * preview)
{
	if (preview->pixmap != NULL) {
#ifdef K
		g_object_unref(preview->pixmap);
#endif
		preview->pixmap = NULL;
	}
}




void PrintPreview::get_page_margins(double * left_margin, double * right_margin, double * top_margin, double * bottom_margin)
{
	if (this->use_full_page) {
		*left_margin   = 0.0;
		*right_margin  = 0.0;
		*top_margin    = 0.0;
		*bottom_margin = 0.0;
	} else {
#ifdef K
		*left_margin   = gtk_page_setup_get_left_margin(preview->page,
								GTK_UNIT_POINTS);
		*right_margin  = gtk_page_setup_get_right_margin(preview->page,
								 GTK_UNIT_POINTS);
		*top_margin    = gtk_page_setup_get_top_margin(preview->page,
							       GTK_UNIT_POINTS);
		*bottom_margin = gtk_page_setup_get_bottom_margin(preview->page,
								  GTK_UNIT_POINTS);
#endif
	}

}




void PrintPreview::queue_draw(void)
{
#ifdef K
	gtk_widget_queue_draw(GTK_WIDGET (this->area));
#endif
}
