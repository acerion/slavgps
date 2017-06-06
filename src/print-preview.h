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

#ifndef _SG_PRINT_PREVIEW_H_
#define _SG_PRINT_PREVIEW_H_




#include <cstdint>




#define VIK_TYPE_PRINT_PREVIEW            (vik_print_preview_get_type ())
#define VIK_PRINT_PREVIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_TYPE_PRINT_PREVIEW, VikPrintPreview))
#define VIK_PRINT_PREVIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_TYPE_PRINT_PREVIEW, VikPrintPreviewClass))
#define VIK_IS_PRINT_PREVIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_TYPE_PRINT_PREVIEW))
#define VIK_IS_PRINT_PREVIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_TYPE_PRINT_PREVIEW))
#define VIK_PRINT_PREVIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), VIK_TYPE_PRINT_PREVIEW, VikPrintPreviewClass))

typedef struct _VikPrintPreview       VikPrintPreview;
typedef struct _VikPrintPreviewClass  VikPrintPreviewClass;

struct _VikPrintPreview {
	GtkAspectFrame  parent_instance;

	GtkWidget      *area;
	GtkPageSetup   *page;
	QPixmap      *pixbuf;
	bool        dragging;

	GdkDrawable    *drawable;

	double         image_offset_x;
	double         image_offset_y;
	double         image_offset_x_max;
	double         image_offset_y_max;
	double         image_xres;
	double         image_yres;

	bool        use_full_page;
};

struct _VikPrintPreviewClass {
	GtkAspectFrameClass  parent_class;

	void (* offsets_changed)  (VikPrintPreview *print_preview,
				   int              offset_x,
				   int              offset_y);
};


GType       vik_print_preview_get_type(void) G_GNUC_CONST;

GtkWidget * vik_print_preview_new(GtkPageSetup     *page,
				  GdkDrawable     *drawable);

void        vik_print_preview_set_image_dpi(VikPrintPreview *preview,
					    double           xres,
					    double           yres);

void        vik_print_preview_set_page_setup(VikPrintPreview *preview,
					     GtkPageSetup     *page);

void        vik_print_preview_set_image_offsets(VikPrintPreview *preview,
						double           offset_x,
						double           offset_y);

void        vik_print_preview_set_image_offsets_max(VikPrintPreview *preview,
						    double           offset_x_max,
						    double           offset_y_max);

void        vik_print_preview_set_use_full_page(VikPrintPreview *preview,
						bool          full_page);




#endif /* #ifndef _SG_PRINT_PREVIEW_H_ */
