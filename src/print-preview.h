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




#include <QPixmap>

#include <cstdint>




typedef int GtkPageSetup;
typedef int GtkAspectFrame;
typedef int GtkAllocation;
typedef int GtkPrintOperation;
typedef int GtkPrintContext;
typedef int GtkPrintOperation;
typedef int GtkRange;
typedef int GtkScrollType;




struct PrintPreview {
	PrintPreview(GtkPageSetup * page, QPixmap * drawable);
	~PrintPreview();

	void set_image_dpi(double xres, double yres);
	void queue_draw(void);
	double get_scale(void);
	void set_page_setup(GtkPageSetup * page);
	void set_image_offsets(double offset_x, double offset_y);
	void set_image_offsets_max(double offset_x_max, double offset_y_max);
	void set_use_full_page(bool full_page);

	void get_page_margins(double * left_margin, double * right_margin, double * top_margin, double * bottom_margin);

	GtkAspectFrame parent_instance;

	QWidget * area = NULL;
	GtkPageSetup * page = NULL;
	QPixmap * pixmap = NULL;
	bool dragging = false;

	QPixmap * drawable = NULL; /* Just a reference? */

	double image_offset_x = 0.0;
	double image_offset_y = 0.0;
	double image_offset_x_max = 0.0;
	double image_offset_y_max = 0.0;
	double image_xres = 230.0;
	double image_yres = 230.0;

	bool use_full_page = false;
};




#endif /* #ifndef _SG_PRINT_PREVIEW_H_ */
