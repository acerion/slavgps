/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 *
 * print.c
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstring>
#include <cstdlib>

#include <QPrinter>
#include <QPrintDialog>
#include <QPrintPreviewDialog>


#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "viking.h"
#include "print.h"
#include "print-preview.h"




using namespace SlavGPS;



enum class PrintCenterMode {
	NONE = 0,
	HORIZONTALLY,
	VERTICALLY,
	BOTH,
};

typedef struct {
	char *name;
	PrintCenterMode mode;
} PrintCenterName;

static const PrintCenterName center_modes[] = {
	{ (char *) N_("None"),          PrintCenterMode::NONE },
	{ (char *) N_("Horizontally"),  PrintCenterMode::HORIZONTALLY },
	{ (char *) N_("Vertically"),    PrintCenterMode::VERTICALLY },
	{ (char *) N_("Both"),          PrintCenterMode::BOTH },
	{ NULL,            (PrintCenterMode) -1}
};

typedef struct {
	int                num_pages;
	bool            show_info_header;
	//Window           * window;
	Viewport         * viewport;
	double             xmpp, ympp;  /* zoom level (meters/pixel) */
	double             xres;
	double             yres;
	int                width;
	int                height;
	double             offset_x;
	double             offset_y;
	PrintCenterMode     center;
	bool            use_full_page;
	GtkPrintOperation  *operation;
} PrintData;

static GtkWidget *create_custom_widget_cb(GtkPrintOperation *operation, PrintData * print_data);
static void begin_print(GtkPrintOperation *operation, GtkPrintContext *context, PrintData * print_data);
static void draw_page(GtkPrintOperation *print, GtkPrintContext *context, int page_nr, PrintData * print_data);

void a_print(Window * parent, Viewport * viewport)
{
#if 0
	QPrinter printer;
	QPrintDialog printDialog(&printer, parent);
	if (printDialog.exec() == QDialog::Accepted) {
		// print ...
	}
#else
	QPrinter printer;
	QPrintPreviewDialog dialog(&printer, parent);
	QObject::connect(&dialog, SIGNAL (paintRequested(QPrinter *)), viewport, SLOT (print_cb(QPrinter *)));
	if (dialog.exec() == QDialog::Accepted) {
		// print ...
	}
#endif


#ifdef K

	GtkPrintOperation *print_oper;
	GtkPrintOperationResult res;
	PrintData print_data;

	print_oper = gtk_print_operation_new();

	print_data.num_pages     = 1;
	//print_data.window        = window;
	print_data.viewport      = viewport;
	print_data.offset_x      = 0;
	print_data.offset_y      = 0;
	print_data.center        = PrintCenterMode::BOTH;
	print_data.use_full_page = false;
	print_data.operation     = print_oper;

	print_data.xmpp          = viewport->get_xmpp();
	print_data.ympp          = viewport->get_ympp();
	print_data.width         = viewport->get_width();
	print_data.height        = viewport->get_height();

	print_data.xres = print_data.yres = 1; // This forces it to default to a 100% page size

	/* TODO: make print_settings non-static when saving_settings_to_file is
	 * implemented. Keep it static for now to retain settings for each
	 * viking session
	 */
	static GtkPrintSettings * print_settings = NULL;
	if (print_settings != NULL) {
		gtk_print_operation_set_print_settings(print_oper, print_settings);
	}

	QObject::connect(print_oper, SIGNAL("begin_print"), &print_data, SLOT (begin_print));
	QObject::connect(print_oper, SIGNAL("draw_page"), &print_data, SLOT (draw_page));
	QObject::connect(print_oper, SIGNAL("create-custom-widget"), &print_data, SLOT (create_custom_widget_cb));

	gtk_print_operation_set_custom_tab_label(print_oper, _("Image Settings"));

	res = gtk_print_operation_run(print_oper,
				      GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
				      window, NULL);

	if (res == GTK_PRINT_OPERATION_RESULT_APPLY) {
		if (print_settings) {
			g_object_unref(print_settings);
		}
		print_settings = (GtkPrintSettings*) g_object_ref(gtk_print_operation_get_print_settings(print_oper));
	}

	g_object_unref(print_oper);
#endif
}




static void begin_print(GtkPrintOperation * operation, GtkPrintContext * context, PrintData * print_data)
{
#ifdef K
	// fputs("DEBUG: begin_print() called\n", stderr);
	gtk_print_operation_set_n_pages(operation, print_data->num_pages);
	gtk_print_operation_set_use_full_page(operation, print_data->use_full_page);
#endif
}




static void copy_row_from_rgb(unsigned char * surface_pixels, unsigned char * pixbuf_pixels, int width)
{
	uint32_t *cairo_data = (uint32_t *) surface_pixels;
	unsigned char  *p;

	int i = 0;
	for (i = 0, p = pixbuf_pixels; i < width; i++) {
		uint32_t r = *p++;
		uint32_t g = *p++;
		uint32_t b = *p++;
		cairo_data[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
	}
}




#define INT_MULT(a,b,t)  ((t) = (a) * (b) + 0x80, ((((t) >> 8) + (t)) >> 8))
#define INT_BLEND(a,b,alpha,tmp)  (INT_MULT((a) - (b), alpha, tmp) + (b))
static void copy_row_from_rgba(unsigned char * surface_pixels, unsigned char * pixbuf_pixels, int width)
{
	uint32_t * cairo_data = (uint32_t *) surface_pixels;
	unsigned char  *p;

	int i = 0;
	for (i = 0, p = pixbuf_pixels; i < width; i++) {
		uint32_t r = *p++;
		uint32_t g = *p++;
		uint32_t b = *p++;
		uint32_t a = *p++;

		if (a != 255) {
			uint32_t tmp;
			/* composite on a white background */
			r = INT_BLEND (r, 255, a, tmp);
			g = INT_BLEND (g, 255, a, tmp);
			b = INT_BLEND (b, 255, a, tmp);
		}
		cairo_data[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
	}
}




static void draw_page(GtkPrintOperation * print, GtkPrintContext * context, int page_nr, PrintData * print_data)
{
#ifdef K
	QPixmap       *pixmap_to_draw;

	cairo_t * cr = gtk_print_context_get_cairo_context(context);
	pixbuf_to_draw = gdk_pixbuf_get_from_drawable(NULL,
						      GDK_DRAWABLE(print_data->viewport->get_pixmap()),
						      NULL, 0, 0, 0, 0, print_data->width, print_data->height);
	cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, print_data->width, print_data->height);

	double cr_dpi_x  = gtk_print_context_get_dpi_x(context);
	double cr_dpi_y  = gtk_print_context_get_dpi_y(context);

	double scale_x = cr_dpi_x / print_data->xres;
	double scale_y = cr_dpi_y / print_data->yres;

	cairo_translate(cr,
			print_data->offset_x / cr_dpi_x * 72.0,
			print_data->offset_y / cr_dpi_y * 72.0);
	cairo_scale(cr, scale_x, scale_y);

	unsigned char * surface_pixels = cairo_image_surface_get_data(surface);
	int stride = cairo_image_surface_get_stride(surface);
	unsigned char * pixbuf_pixels = gdk_pixbuf_get_pixels(pixbuf_to_draw);
	int pixbuf_stride = gdk_pixbuf_get_rowstride(pixbuf_to_draw);
	int pixbuf_n_channels = gdk_pixbuf_get_n_channels(pixbuf_to_draw);

	// fprintf(stderr, "DEBUG: %s() surface_pixels=%p pixbuf_pixels=%p size=%d surface_width=%d surface_height=%d stride=%d data_height=%d pixmap_stride=%d pixmap_nchannels=%d pixmap_bit_per_Sample=%d\n", __PRETTY_FUNCTION__, surface_pixels, pixbuf_pixels, stride * print_data->height, cairo_image_surface_get_width(surface), cairo_image_surface_get_height(surface), stride, print_data->height, gdk_pixbuf_get_rowstride(pixbuf_to_draw), gdk_pixbuf_get_n_channels(pixbuf_to_draw), gdk_pixbuf_get_bits_per_sample(pixbuf_to_draw));

	/* Assume the pixbuf has 8 bits per channel */
	for (int y = 0; y < print_data->height; y++, surface_pixels += stride, pixbuf_pixels += pixbuf_stride) {
		switch (pixbuf_n_channels) {
		case 3:
			copy_row_from_rgb(surface_pixels, pixbuf_pixels, print_data->width);
			break;
		case 4:
			copy_row_from_rgba(surface_pixels, pixbuf_pixels, print_data->width);
			break;
		default: break;
		}
	}

	g_object_unref(G_OBJECT(pixbuf_to_draw));

	cairo_set_source_surface(cr, surface, 0, 0);
	cairo_rectangle(cr, 0, 0, print_data->width, print_data->height);
	cairo_fill(cr);
	cairo_surface_destroy(surface);
#endif
}


/*********************** page layout gui *********************/
typedef struct {
	PrintData * print_data;
	GtkWidget * center_combo;
	GtkWidget * scale;
	GtkWidget * scale_label;
	PrintPreview * preview;
} CustomWidgetInfo;

enum {
	BOTTOM,
	TOP,
	RIGHT,
	LEFT,
	WIDTH,
	HEIGHT
};




static bool scale_change_value_cb(GtkRange *range, GtkScrollType scroll, double value, CustomWidgetInfo *pinfo);
static void get_page_dimensions(CustomWidgetInfo *info, double *page_width, double *page_height, QPageLayout::Unit unit);
static void center_changed_cb(QComboBox * combo, CustomWidgetInfo *info);
static void get_max_offsets(CustomWidgetInfo *info, double *offset_x_max, double *offset_y_max);
static void update_offsets(CustomWidgetInfo *info);




static void set_scale_label(CustomWidgetInfo *pinfo, double scale_val)
{
	static const double inch_to_mm = 25.4;
	char label_text[64];

	snprintf(label_text, sizeof(label_text), "<i>%.0fx%0.f mm (%.0f%%)</i>",
		 inch_to_mm * pinfo->print_data->width / pinfo->print_data->xres,
		 inch_to_mm * pinfo->print_data->height / pinfo->print_data->yres,
		 scale_val);
#ifdef K
	gtk_label_set_markup(GTK_LABEL (pinfo->scale_label), label_text);
#endif
}

static void set_scale_value(CustomWidgetInfo *pinfo)
{
	double width;
	double height;

	get_page_dimensions(pinfo, &width, &height, QPageLayout::Inch);
	double ratio_w = 100 * pinfo->print_data->width / pinfo->print_data->xres / width;
	double ratio_h = 100 * pinfo->print_data->height / pinfo->print_data->yres / height;

	double ratio = MAX(ratio_w, ratio_h);
#ifdef K
	g_signal_handlers_block_by_func(GTK_RANGE(pinfo->scale), (void *) scale_change_value_cb, pinfo);
	gtk_range_set_value(GTK_RANGE(pinfo->scale), ratio);
	g_signal_handlers_unblock_by_func(GTK_RANGE(pinfo->scale), (void *) scale_change_value_cb, pinfo);
#endif
	set_scale_label(pinfo, ratio);
}

static void update_page_setup(CustomWidgetInfo * info)
{
	double paper_width;
	double paper_height;
	double offset_x_max, offset_y_max;

	get_page_dimensions(info, &paper_width, &paper_height, QPageLayout::Inch);
	if ((paper_width < (info->print_data->width / info->print_data->xres)) ||
	    (paper_height < (info->print_data->height / info->print_data->yres))) {
		double xres = (double) info->print_data->width / paper_width;
		double yres = (double) info->print_data->height / paper_height;
		info->print_data->xres = info->print_data->yres = MAX(xres, yres);
		info->preview->set_image_dpi(info->print_data->xres, info->print_data->yres);
	}
	get_max_offsets(info, &offset_x_max, &offset_y_max);
	info->preview->set_image_offsets_max(offset_x_max, offset_y_max);
	update_offsets(info);
	set_scale_value(info);
	if (info->preview) {
		info->preview->set_image_offsets(info->print_data->offset_x, info->print_data->offset_y);
	}
}

static void page_setup_cb(GtkWidget * widget, CustomWidgetInfo *info)
{
#ifdef K
	GtkPrintOperation * operation = info->print_data->operation;

	GtkWidget * toplevel = gtk_widget_get_toplevel(widget);
	if (! gtk_widget_is_toplevel(toplevel)) {
		toplevel = NULL;
	}

	GtkPrintSettings * settings = gtk_print_operation_get_print_settings(operation);
	if (!settings) {
		settings = gtk_print_settings_new();
	}

	GtkPageSetup * page_setup = gtk_print_operation_get_default_page_setup(operation);

	page_setup = gtk_print_run_page_setup_dialog (GTK_WINDOW (toplevel),
						      page_setup, settings);

	gtk_print_operation_set_default_page_setup(operation, page_setup);
	info->preview->set_page_setup(page_setup);
#endif
	update_page_setup(info);
}




static void full_page_toggled_cb(GtkWidget *widget, CustomWidgetInfo * info)
{
	bool active = false;
#ifdef K
	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
#endif
	info->print_data->use_full_page = active;
	update_page_setup(info);
	info->preview->set_use_full_page(active);
}

static void set_center_none(CustomWidgetInfo * info)
{
	info->print_data->center = PrintCenterMode::NONE;

	if (info->center_combo) {
#ifdef K
		g_signal_handlers_block_by_func(info->center_combo,
						(void *) center_changed_cb, info);

		info->print_data->center = PrintCenterMode::NONE;
		info->center_combo->setCurrentIndex(info->print_data->center);
		g_signal_handlers_unblock_by_func(info->center_combo,
						  (void *) center_changed_cb, info);
#endif
	}
}




static void preview_offsets_changed_cb(GtkWidget * widget, double offset_x, double offset_y, CustomWidgetInfo * info)
{
	set_center_none(info);

	info->print_data->offset_x = offset_x;
	info->print_data->offset_y = offset_y;

	update_offsets(info);
}




static void get_page_dimensions(CustomWidgetInfo * info, double * page_width, double * page_height, QPageLayout::Unit unit)
{
#ifdef K
	GtkPageSetup * setup = gtk_print_operation_get_default_page_setup(info->print_data->operation);

	*page_width = gtk_page_setup_get_paper_width(setup, unit);
	*page_height = gtk_page_setup_get_paper_height(setup, unit);

	if (!info->print_data->use_full_page) {
		double left_margin = gtk_page_setup_get_left_margin(setup, unit);
		double right_margin = gtk_page_setup_get_right_margin(setup, unit);
		double top_margin = gtk_page_setup_get_top_margin(setup, unit);
		double bottom_margin = gtk_page_setup_get_bottom_margin(setup, unit);

		*page_width -= left_margin + right_margin;
		*page_height -= top_margin + bottom_margin;
	}
#endif
}




static void get_max_offsets(CustomWidgetInfo * info, double * offset_x_max, double * offset_y_max)
{
	double width;
	double height;

	get_page_dimensions(info, &width, &height, QPageLayout::Point);

	*offset_x_max = width - 72.0 * info->print_data->width / info->print_data->xres;
	*offset_x_max = MAX (0, *offset_x_max);

	*offset_y_max = height - 72.0 * info->print_data->height / info->print_data->yres;
	*offset_y_max = MAX (0, *offset_y_max);
}




static void update_offsets(CustomWidgetInfo *info)
{
	PrintData * print_data = info->print_data;
	double    offset_x_max;
	double    offset_y_max;

	get_max_offsets(info, &offset_x_max, &offset_y_max);

	switch (print_data->center) {
	case PrintCenterMode::NONE:
		if (print_data->offset_x > offset_x_max) {
			print_data->offset_x = offset_x_max;
		}
		if (print_data->offset_y > offset_y_max) {
			print_data->offset_y = offset_y_max;
		}
		break;

	case PrintCenterMode::HORIZONTALLY:
		print_data->offset_x = offset_x_max / 2.0;
		break;

	case PrintCenterMode::VERTICALLY:
		print_data->offset_y = offset_y_max / 2.0;
		break;

	case PrintCenterMode::BOTH:
		print_data->offset_x = offset_x_max / 2.0;
		print_data->offset_y = offset_y_max / 2.0;
		break;

	default: break;
	}
}




static void center_changed_cb(QComboBox * combo, CustomWidgetInfo * info)
{
	info->print_data->center = (PrintCenterMode) combo->currentIndex();
	update_offsets(info);

	if (info->preview) {
		info->preview->set_image_offsets(info->print_data->offset_x, info->print_data->offset_y);
	}
}




static bool scale_change_value_cb(GtkRange * range, GtkScrollType scroll, double value, CustomWidgetInfo * info)
{
	double paper_width;
	double paper_height;
	double offset_x_max, offset_y_max;
	double scale = CLAMP(value, 1, 100);

	get_page_dimensions(info, &paper_width, &paper_height, QPageLayout::Inch);
	double xres = info->print_data->width * 100 / paper_width / scale;
	double yres = info->print_data->height * 100 / paper_height / scale;
	double res = MAX(xres, yres);
	info->print_data->xres = info->print_data->yres = res;
	get_max_offsets(info, &offset_x_max, &offset_y_max);
	update_offsets(info);
	if (info->preview) {
		info->preview->set_image_dpi(info->print_data->xres, info->print_data->yres);
		info->preview->set_image_offsets(info->print_data->offset_x, info->print_data->offset_y);
		info->preview->set_image_offsets_max(offset_x_max, offset_y_max);
	}

	set_scale_label(info, scale);

	return false;
}




static void custom_widgets_cleanup(CustomWidgetInfo * info)
{
	free(info);
}




static GtkWidget *create_custom_widget_cb(GtkPrintOperation *operation, PrintData * print_data)
{
	CustomWidgetInfo * info = (CustomWidgetInfo *) malloc(sizeof (CustomWidgetInfo));
	memset(info, 0, sizeof (CustomWidgetInfo));

#ifdef K
	QObject::connect(print_data->operation, _("done"), info, SLOT (custom_widgets_cleanup));

	info->print_data = print_data;

	GtkPageSetup * setup = gtk_print_operation_get_default_page_setup(print_data->operation);
	if (!setup) {
		setup = gtk_page_setup_new();
		gtk_print_operation_set_default_page_setup(print_data->operation, setup);
	}

	GtkWidget * layout = gtk_vbox_new(false, 6);
	gtk_container_set_border_width(GTK_CONTAINER (layout), 12);

	/*  main hbox  */
	GtkWidget * main_hbox = gtk_hbox_new(false, 12);
	layout->addWidget(main_hbox);
	gtk_widget_show(main_hbox);

	/*  main vbox  */
	GtkWidget * main_vbox = gtk_vbox_new(false, 12);
	main_hbox->addWidget(main_vbox);
	gtk_widget_show(main_vbox);

	GtkWidget * vbox = gtk_vbox_new(false, 6);
	main_vbox->addWidget(vbox);
	gtk_widget_show(vbox);

	/* Page Size */
	GtkWidget * button = gtk_button_new_with_mnemonic(_("_Adjust Page Size "
						"and Orientation"));
	main_vbox->addWidget(button);
	QObject::connect(button, SIGNAL("clicked"), info, SLOT (page_setup_cb));
	gtk_widget_show(button);

	/* Center */
	const PrintCenterName *center;

	GtkWidget * hbox = gtk_hbox_new(false, 6);
	main_vbox->addWidget(hbox);
	gtk_widget_show(hbox);

	QLabel * label = new QLabel(QObject::tr("C&enter:"));
	gtk_misc_set_alignment(GTK_MISC (label), 0.0, 0.5);
	hbox->addWidget(label);
	gtk_widget_show(label);

	QComboBox * combo = new QComboBox();
	for (center = center_modes; center->name; center++) {
		vik_combo_box_text_append(combo, _(center->name));
	}
	combo->setCurrentIndex(PrintCenterMode::BOTH);
	hbox->addWidget(combo);
	gtk_widget_show(combo);
	gtk_label_set_mnemonic_widget(GTK_LABEL (label), combo);
	QObject::connect(combo, SIGNAL("changed"), info, SLOT (center_changed_cb));
	info->center_combo = combo;

	/* ignore page margins */
	button = gtk_check_button_new_with_mnemonic(_("Ignore Page _Margins"));

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (button),
				     print_data->use_full_page);
	main_vbox->addWidget(button);
	QObject::connect(button, SIGNAL("toggled"), info, SLOT (full_page_toggled_cb));
	gtk_widget_show(button);

	/* scale */
	vbox = gtk_vbox_new(false, 1);
	main_vbox->addWidget(vbox);
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(false, 6);
	vbox->addWidget(hbox);
	gtk_widget_show(hbox);

	label = new QLabel(QObject::tr("Image S&ize:"));
	gtk_misc_set_alignment(GTK_MISC (label), 0.0, 0.5);
	hbox->addWidget(label);
	gtk_widget_show(label);

	label = new QLabel("");
	gtk_misc_set_alignment(GTK_MISC (label), 0.0, 0.5);
	info->scale_label = label;
	hbox->addWidget(info->scale_label);
	gtk_widget_show(info->scale_label);

	info->scale = gtk_hscale_new_with_range(1, 100, 1);
	vbox->addWidget(info->scale);
	gtk_scale_set_draw_value(GTK_SCALE(info->scale), false);
	gtk_widget_show(info->scale);
	gtk_label_set_mnemonic_widget(GTK_LABEL (label), info->scale);

	QObject::connect(info->scale, SIGNAL("change_value"), info, SLOT (scale_change_value_cb));


	info->preview = new PrintPreview(setup, print_data->viewport->get_pixmap());
	info->preview->set_use_full_page(print_data->use_full_page);
	main_hbox->addWidget(info->preview);
	gtk_widget_show(info->preview);

	QObject::connect(info->preview, SIGNAL("offsets-changed"), info, SLOT (preview_offsets_changed_cb));

	update_page_setup(info);

	double offset_x_max, offset_y_max;
	get_max_offsets(info, &offset_x_max, &offset_y_max);
	info->preview->set_image_offsets_max(offset_x_max, offset_y_max);

	set_scale_value(info);

	return layout;
#endif
}
