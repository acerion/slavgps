/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (c) 2014, Rob Norris <rw_norris@hotmail.com>
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

#include "viking.h"
#include "vikutils.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>

#include "ui_util.h"
#include "preferences.h"
#include "icons/icons.h"
#include "vikmapslayer.h"
#include "vikgeoreflayer.h"
#include "vikfileentry.h"
#include "dialog.h"
#include "file.h"
#include "settings.h"
#include "globals.h"

/*
static VikLayerParamData image_default (void)
{
  VikLayerParamData data;
  data.s = g_strdup("");
  return data;
}
*/

VikLayerParam georef_layer_params[] = {
	{ VIK_LAYER_GEOREF, "image", VIK_LAYER_PARAM_STRING, VIK_LAYER_NOT_IN_PROPERTIES, NULL, (VikLayerWidgetType) 0, NULL, NULL, NULL, NULL, NULL, NULL },
	{ VIK_LAYER_GEOREF, "corner_easting", VIK_LAYER_PARAM_DOUBLE, VIK_LAYER_NOT_IN_PROPERTIES, NULL, (VikLayerWidgetType) 0, NULL, NULL, NULL, NULL, NULL, NULL },
	{ VIK_LAYER_GEOREF, "corner_northing", VIK_LAYER_PARAM_DOUBLE, VIK_LAYER_NOT_IN_PROPERTIES, NULL, (VikLayerWidgetType) 0, NULL, NULL, NULL, NULL, NULL, NULL },
	{ VIK_LAYER_GEOREF, "mpp_easting", VIK_LAYER_PARAM_DOUBLE, VIK_LAYER_NOT_IN_PROPERTIES, NULL, (VikLayerWidgetType) 0, NULL, NULL, NULL, NULL, NULL, NULL },
	{ VIK_LAYER_GEOREF, "mpp_northing", VIK_LAYER_PARAM_DOUBLE, VIK_LAYER_NOT_IN_PROPERTIES, NULL, (VikLayerWidgetType) 0, NULL, NULL, NULL, NULL, NULL, NULL },
	{ VIK_LAYER_GEOREF, "corner_zone", VIK_LAYER_PARAM_UINT, VIK_LAYER_NOT_IN_PROPERTIES, NULL, (VikLayerWidgetType) 0, NULL, NULL, NULL, NULL, NULL, NULL },
	{ VIK_LAYER_GEOREF, "corner_letter_as_int", VIK_LAYER_PARAM_UINT, VIK_LAYER_NOT_IN_PROPERTIES, NULL, (VikLayerWidgetType) 0, NULL, NULL, NULL, NULL, NULL, NULL },
	{ VIK_LAYER_GEOREF, "alpha", VIK_LAYER_PARAM_UINT, VIK_LAYER_NOT_IN_PROPERTIES, NULL, (VikLayerWidgetType) 0, NULL, NULL, NULL, NULL, NULL, NULL },
};

enum {
	PARAM_IMAGE = 0,
	PARAM_CE,
	PARAM_CN,
	PARAM_ME,
	PARAM_MN,
	PARAM_CZ,
	PARAM_CL,
	PARAM_AA,
	NUM_PARAMS };

static VikGeorefLayer *georef_layer_unmarshall(uint8_t *data, int len, Viewport * viewport);
static bool georef_layer_set_param(VikGeorefLayer *vgl, uint16_t id, VikLayerParamData data, Viewport * viewport, bool is_file_operation);
static VikLayerParamData georef_layer_get_param(VikGeorefLayer *vgl, uint16_t id, bool is_file_operation);
static VikGeorefLayer *georef_layer_new(Viewport * viewport);
//static VikGeorefLayer *georef_layer_create(Viewport * viewport);
static void georef_layer_set_image(VikGeorefLayer *vgl, const char *image);
static bool georef_layer_dialog(VikGeorefLayer *vgl, Viewport * viewport, GtkWindow *w);

/* tools */
static void * georef_layer_move_create(VikWindow *vw, Viewport * viewport);
static bool georef_layer_move_release(VikGeorefLayer *vgl, GdkEventButton *event, Viewport * viewport);
static bool georef_layer_move_press(VikGeorefLayer *vgl, GdkEventButton *event, Viewport * viewport);
static void * georef_layer_zoom_create(VikWindow *vw, Viewport * viewport);
static bool georef_layer_zoom_press(VikGeorefLayer *vgl, GdkEventButton *event, Viewport * viewport);

// See comment in viktrwlayer.c for advice on values used
static VikToolInterface georef_tools[] = {
	{ { "GeorefMoveMap", "vik-icon-Georef Move Map",  N_("_Georef Move Map"), NULL,  N_("Georef Move Map"), 0 },
	  (VikToolConstructorFunc) georef_layer_move_create,
	  NULL,
	  NULL,
	  NULL,
	  (VikToolMouseFunc) georef_layer_move_press,
	  NULL,
	  (VikToolMouseFunc) georef_layer_move_release,
	  (VikToolKeyFunc) NULL,
	  false,
	  GDK_CURSOR_IS_PIXMAP,
	  &cursor_geomove_pixbuf,
	  NULL },

	{ { "GeorefZoomTool", "vik-icon-Georef Zoom Tool",  N_("Georef Z_oom Tool"), NULL,  N_("Georef Zoom Tool"), 0 },
	  (VikToolConstructorFunc) georef_layer_zoom_create,
	  NULL,
	  NULL,
	  NULL,
	  (VikToolMouseFunc) georef_layer_zoom_press,
	  NULL, NULL,
	  (VikToolKeyFunc) NULL,
	  false,
	  GDK_CURSOR_IS_PIXMAP,
	  &cursor_geozoom_pixbuf,
	  NULL },
};

VikLayerInterface vik_georef_layer_interface = {
	"GeoRef Map",
	N_("GeoRef Map"),
	NULL,
	&vikgeoreflayer_pixbuf, /*icon */

	georef_tools,
	sizeof(georef_tools) / sizeof(VikToolInterface),

	georef_layer_params,
	NUM_PARAMS,
	NULL,
	0,

	VIK_MENU_ITEM_ALL,

	(VikLayerFuncCreate)                  NULL,

	(VikLayerFuncUnmarshall)	      georef_layer_unmarshall,

	(VikLayerFuncSetParam)                georef_layer_set_param,
	(VikLayerFuncGetParam)                georef_layer_get_param,
	(VikLayerFuncChangeParam)             NULL,
};

typedef struct {
	GtkWidget *x_spin;
	GtkWidget *y_spin;
	// UTM widgets
	GtkWidget *ce_spin; // top left
	GtkWidget *cn_spin; //    "
	GtkWidget *utm_zone_spin;
	GtkWidget *utm_letter_entry;

	GtkWidget *lat_tl_spin;
	GtkWidget *lon_tl_spin;
	GtkWidget *lat_br_spin;
	GtkWidget *lon_br_spin;
	//
	GtkWidget *tabs;
	GtkWidget *imageentry;
} changeable_widgets;

struct _VikGeorefLayer {
	VikLayer vl;
	char *image;
	GdkPixbuf *pixbuf;
	uint8_t alpha;

	struct UTM corner; // Top Left
	double mpp_easting, mpp_northing;
	struct LatLon ll_br; // Bottom Right
	unsigned int width, height;

	GdkPixbuf *scaled;
	uint32_t scaled_width, scaled_height;

	int click_x, click_y;
	changeable_widgets cw;
};

static VikLayerParam io_prefs[] = {
	{ VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "georef_auto_read_world_file", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Auto Read World Files:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL,
	  N_("Automatically attempt to read associated world file of a new image for a GeoRef layer"), NULL, NULL, NULL}
};

void vik_georef_layer_init(void)
{
	VikLayerParamData tmp;
	tmp.b = true;
	a_preferences_register(&io_prefs[0], tmp, VIKING_PREFERENCES_IO_GROUP_KEY);
}

GType vik_georef_layer_get_type()
{
	static GType vgl_type = 0;

	if (!vgl_type) {
		static const GTypeInfo vgl_info =  {
			sizeof (VikGeorefLayerClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			NULL, /* class init */
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (VikGeorefLayer),
			0,
			NULL /* instance init */
		};
		vgl_type = g_type_register_static(VIK_LAYER_TYPE, "VikGeorefLayer", &vgl_info, (GTypeFlags) 0);
	}

	return vgl_type;
}

char const * LayerGeoref::tooltip()
{
	VikGeorefLayer * vgl = (VikGeorefLayer *) this->vl;
	return vgl->image;
}

void LayerGeoref::marshall(uint8_t **data, int *len)
{
	VikGeorefLayer * vgl = (VikGeorefLayer *) this->vl;
	vik_layer_marshall_params(VIK_LAYER(vgl), data, len);
}

static VikGeorefLayer * georef_layer_unmarshall(uint8_t *data, int len, Viewport * viewport)
{
	VikGeorefLayer *rv = georef_layer_new(viewport);
	vik_layer_unmarshall_params(VIK_LAYER(rv), data, len, viewport);
	if (rv->image) {
		((LayerGeoref *) ((VikLayer *) rv)->layer)->post_read(viewport, true);
	}
	return rv;
}

static bool georef_layer_set_param(VikGeorefLayer *vgl, uint16_t id, VikLayerParamData data, Viewport * viewport, bool is_file_operation)
{
	switch (id) {
	case PARAM_IMAGE:
		georef_layer_set_image(vgl, data.s);
		break;
	case PARAM_CN:
		vgl->corner.northing = data.d;
		break;
	case PARAM_CE:
		vgl->corner.easting = data.d;
		break;
	case PARAM_MN:
		vgl->mpp_northing = data.d;
		break;
	case PARAM_ME:
		vgl->mpp_easting = data.d;
		break;
	case PARAM_CZ:
		if (data.u <= 60) {
			vgl->corner.zone = data.u;
		}
		break;
	case PARAM_CL:
		if (data.u >= 65 || data.u <= 90) {
			vgl->corner.letter = data.u;
		}
		break;
	case PARAM_AA:
		if (data.u <= 255) {
			vgl->alpha = data.u;
		}
		break;
	default:
		break;
	}
	return true;
}

static void create_image_file(VikGeorefLayer *vgl)
{
	// Create in .viking-maps
	char *filename = g_strconcat(maps_layer_default_dir(), ((Layer *) ((VikLayer *) vgl)->layer)->get_name(), ".jpg", NULL);
	GError *error = NULL;
	gdk_pixbuf_save(vgl->pixbuf, filename, "jpeg", &error, NULL);
	if (error) {
		fprintf(stderr, "WARNING: %s\n", error->message);
		g_error_free(error);
	} else {
		vgl->image = g_strdup(filename);
	}

	free(filename);
}

static VikLayerParamData georef_layer_get_param(VikGeorefLayer *vgl, uint16_t id, bool is_file_operation)
{
	VikLayerParamData rv;
	switch (id) {
	case PARAM_IMAGE: {
		bool set = false;
		if (is_file_operation) {
			if (vgl->pixbuf && !vgl->image) {
				// Force creation of image file
				create_image_file(vgl);
			}
			if (a_vik_get_file_ref_format() == VIK_FILE_REF_FORMAT_RELATIVE) {
				char *cwd = g_get_current_dir();
				if (cwd) {
					rv.s = file_GetRelativeFilename(cwd, vgl->image);
					if (!rv.s) rv.s = "";
					set = true;
				}
			}
		}
		if (!set) {
			rv.s = vgl->image ? vgl->image : "";
		}
		break;
	}
	case PARAM_CN:
		rv.d = vgl->corner.northing;
		break;
	case PARAM_CE:
		rv.d = vgl->corner.easting;
		break;
	case PARAM_MN:
		rv.d = vgl->mpp_northing;
		break;
	case PARAM_ME:
		rv.d = vgl->mpp_easting;
		break;
	case PARAM_CZ:
		rv.u = vgl->corner.zone;
		break;
	case PARAM_CL:
		rv.u = vgl->corner.letter;
		break;
	case PARAM_AA:
		rv.u = vgl->alpha;
		break;
	default:
		break;
	}
	return rv;
}

static VikGeorefLayer * georef_layer_new(Viewport * viewport)
{
	VikGeorefLayer *vgl = VIK_GEOREF_LAYER(g_object_new(VIK_GEOREF_LAYER_TYPE, NULL));

	((VikLayer *) vgl)->layer = new LayerGeoref((VikLayer *) vgl);

	// Since GeoRef layer doesn't use uibuilder
	//  initializing this way won't do anything yet..
	vik_layer_set_defaults(VIK_LAYER(vgl), viewport);

	// Make these defaults based on the current view
	vgl->mpp_northing = viewport->get_ympp();
	vgl->mpp_easting = viewport->get_xmpp();
	vik_coord_to_utm(viewport->get_center(), &(vgl->corner));

	vgl->image = NULL;
	vgl->pixbuf = NULL;
	vgl->click_x = -1;
	vgl->click_y = -1;
	vgl->scaled = NULL;
	vgl->scaled_width = 0;
	vgl->scaled_height = 0;
	vgl->ll_br.lat = 0.0;
	vgl->ll_br.lon = 0.0;
	vgl->alpha = 255;

	return vgl;
}

/**
 * Return mpp for the given coords, coord mode and image size.
 */
static void georef_layer_mpp_from_coords(VikCoordMode mode, struct LatLon ll_tl, struct LatLon ll_br, unsigned int width, unsigned int height, double *xmpp, double *ympp)
{
	struct LatLon ll_tr;
	ll_tr.lat = ll_tl.lat;
	ll_tr.lon = ll_br.lon;

	struct LatLon ll_bl;
	ll_bl.lat = ll_br.lat;
	ll_bl.lon = ll_tl.lon;

	// UTM mode should be exact MPP
	double factor = 1.0;
	if (mode == VIK_COORD_LATLON) {
		// NB the 1.193 - is at the Equator.
		// http://wiki.openstreetmap.org/wiki/Zoom_levels

		// Convert from actual image MPP to Viking 'pixelfact'
		double mid_lat = (ll_bl.lat + ll_tr.lat) / 2.0;
		// Protect against div by zero (but shouldn't have 90 degrees for mid latitude...)
		if (fabs(mid_lat) < 89.9) {
			factor = cos(DEG2RAD(mid_lat)) * 1.193;
		}
	}

	double diffx = a_coords_latlon_diff(&ll_tl, &ll_tr);
	*xmpp = (diffx / width) / factor;

	double diffy = a_coords_latlon_diff(&ll_tl, &ll_bl);
	*ympp =(diffy / height) / factor;
}

void LayerGeoref::draw(Viewport * viewport)
{
	VikGeorefLayer * vgl = (VikGeorefLayer *) this->vl;

	if (vgl->pixbuf) {
		double xmpp = viewport->get_xmpp(), ympp = viewport->get_ympp();
		GdkPixbuf *pixbuf = vgl->pixbuf;
		unsigned int layer_width = vgl->width;
		unsigned int layer_height = vgl->height;

		unsigned int width = viewport->get_width(), height = viewport->get_height();
		int32_t x, y;
		VikCoord corner_coord;
		vik_coord_load_from_utm(&corner_coord, viewport->get_coord_mode(), &(vgl->corner));
		viewport->coord_to_screen(&corner_coord, &x, &y);

		/* mark to scale the pixbuf if it doesn't match our dimensions */
		bool scale = false;
		if (xmpp != vgl->mpp_easting || ympp != vgl->mpp_northing) {
			scale = true;
			layer_width = round(vgl->width * vgl->mpp_easting / xmpp);
			layer_height = round(vgl->height * vgl->mpp_northing / ympp);
		}

		// If image not in viewport bounds - no need to draw it (or bother with any scaling)
		if ((x < 0 || x < width) && (y < 0 || y < height) && x+layer_width > 0 && y+layer_height > 0) {

			if (scale) {
				/* rescale if necessary */
				if (layer_width == vgl->scaled_width && layer_height == vgl->scaled_height && vgl->scaled != NULL) {
					pixbuf = vgl->scaled;
				} else {
					pixbuf = gdk_pixbuf_scale_simple(vgl->pixbuf,
									 layer_width,
									 layer_height,
									 GDK_INTERP_BILINEAR);

					if (vgl->scaled != NULL) {
						g_object_unref(vgl->scaled);
					}

					vgl->scaled = pixbuf;
					vgl->scaled_width = layer_width;
					vgl->scaled_height = layer_height;
				}
			}
			viewport->draw_pixbuf(pixbuf, 0, 0, x, y, layer_width, layer_height); /* todo: draw only what we need to. */
		}
	}
}

void LayerGeoref::free_()
{
	VikGeorefLayer * vgl = (VikGeorefLayer *) this->vl;
	if (vgl->image != NULL) {
		free(vgl->image);
	}

	if (vgl->scaled != NULL) {
		g_object_unref(vgl->scaled);
	}
}

#if 0
static VikGeorefLayer * georef_layer_create(Viewport * viewport)
{
	VikGeorefLayer * rv = georef_layer_new(viewport);

	return rv;
}
#endif

bool LayerGeoref::properties(void * viewport)
{
	VikGeorefLayer * vgl = (VikGeorefLayer *) this->vl;
	return georef_layer_dialog(vgl, (Viewport *) viewport, VIK_GTK_WINDOW_FROM_WIDGET(((Viewport *) viewport)->vvp));
}

/* Formerly known as georef_layer_load_image(). */
void LayerGeoref::load_image(Viewport * viewport, bool from_file)
{
	VikGeorefLayer *vgl = (VikGeorefLayer *) this->vl;
	GError *gx = NULL;
	if (vgl->image == NULL) {
		return;
	}

	if (vgl->pixbuf) {
		g_object_unref(G_OBJECT(vgl->pixbuf));
	}

	if (vgl->scaled) {
		g_object_unref(G_OBJECT(vgl->scaled));
		vgl->scaled = NULL;
	}

	vgl->pixbuf = gdk_pixbuf_new_from_file (vgl->image, &gx);

	if (gx) {
		if (!from_file) {
			a_dialog_error_msg_extra(VIK_GTK_WINDOW_FROM_WIDGET((VikViewport *) viewport->vvp), _("Couldn't open image file: %s"), gx->message);
		}
		g_error_free (gx);
	} else {
		vgl->width = gdk_pixbuf_get_width(vgl->pixbuf);
		vgl->height = gdk_pixbuf_get_height(vgl->pixbuf);

		if (vgl->pixbuf && vgl->alpha < 255) {
			vgl->pixbuf = ui_pixbuf_set_alpha(vgl->pixbuf, vgl->alpha);
		}
	}
	/* should find length and width here too */
}

static void georef_layer_set_image(VikGeorefLayer *vgl, const char *image)
{
	if (vgl->image) {
		free(vgl->image);
	}

	if (vgl->scaled) {
		g_object_unref(vgl->scaled);
		vgl->scaled = NULL;
	}
	if (image == NULL) {
		vgl->image = NULL;
	}

	if (g_strcmp0(image, "") != 0) {
		vgl->image = vu_get_canonical_filename(VIK_LAYER(vgl), image);
	} else {
		vgl->image = g_strdup(image);
	}
}

// Only positive values allowed here
static void double2spinwidget(GtkWidget *widget, double val)
{
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), val > 0 ? val : -val);
}

static void set_widget_values(changeable_widgets *cw, double values[4])
{
	double2spinwidget(cw->x_spin, values[0]);
	double2spinwidget(cw->y_spin, values[1]);
	double2spinwidget(cw->ce_spin, values[2]);
	double2spinwidget(cw->cn_spin, values[3]);
}

static bool world_file_read_line(FILE *ff, double *value, bool use_value)
{
	bool answer = true; // Success
	char buffer[1024];
	if (!fgets(buffer, 1024, ff)) {
		answer = false;
	}
	if (answer && use_value) {
		*value = g_strtod(buffer, NULL);
	}

	return answer;
}

/**
 * http://en.wikipedia.org/wiki/World_file
 *
 * Note world files do not define the units and nor are the units standardized :(
 * Currently Viking only supports:
 *  x&y scale as meters per pixel
 *  x&y coords as UTM eastings and northings respectively
 */
static int world_file_read_file(const char* filename, double values[4])
{
	fprintf(stderr, "DEBUG: %s - trying world file %s\n", __FUNCTION__, filename);

	FILE *f = g_fopen(filename, "r");
	if (!f) {
		return 1;
	} else {
		int answer = 2; // Not enough info read yet
		// **We do not handle 'skew' values ATM - normally they are a value of 0 anyway to align with the UTM grid
		if (world_file_read_line(f, &values[0], true) // x scale
		     && world_file_read_line(f, NULL, false) // Ignore value in y-skew line**
		     && world_file_read_line(f, NULL, false) // Ignore value in x-skew line**
		     && world_file_read_line(f, &values[1], true) // y scale
		     && world_file_read_line(f, &values[2], true) // x-coordinate of the upper left pixel
		     && world_file_read_line(f, &values[3], true) // y-coordinate of the upper left pixel
		    )
			{
				// Success
				fprintf(stderr, "DEBUG: %s - %s - world file read success\n", __FUNCTION__, filename);
				answer = 0;
			}
		fclose(f);
		return answer;
	}
}

static void georef_layer_dialog_load(changeable_widgets *cw)
{
	GtkWidget *file_selector = gtk_file_chooser_dialog_new(_("Choose World file"),
								NULL,
								GTK_FILE_CHOOSER_ACTION_OPEN,
								GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
								GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
								NULL);

	if (gtk_dialog_run(GTK_DIALOG (file_selector)) == GTK_RESPONSE_ACCEPT) {
		double values[4];
		int answer = world_file_read_file(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_selector)), values);
		if (answer == 1) {
			a_dialog_error_msg(VIK_GTK_WINDOW_FROM_WIDGET(cw->x_spin), _("The World file you requested could not be opened for reading."));
		} else if (answer == 2) {
			a_dialog_error_msg(VIK_GTK_WINDOW_FROM_WIDGET(cw->x_spin), _("Unexpected end of file reading World file."));
		} else {
			// NB answer should == 0 for success
			set_widget_values(cw, values);
		}
	}

	gtk_widget_destroy(file_selector);
}

static void georef_layer_export_params(void * *pass_along[2])
{
	VikGeorefLayer *vgl = VIK_GEOREF_LAYER(pass_along[0]);
	GtkWidget *file_selector = gtk_file_chooser_dialog_new(_("Choose World file"),
								NULL,
								GTK_FILE_CHOOSER_ACTION_SAVE,
								GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
								GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
								NULL);
	if (gtk_dialog_run(GTK_DIALOG (file_selector)) == GTK_RESPONSE_ACCEPT) {
		FILE *f = g_fopen(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_selector)), "w");

		gtk_widget_destroy(file_selector);
		if (!f) {
			a_dialog_error_msg(VIK_GTK_WINDOW_FROM_WIDGET(pass_along[0]), _("The file you requested could not be opened for writing."));
			return;
		} else {
			fprintf(f, "%f\n%f\n%f\n%f\n%f\n%f\n", vgl->mpp_easting, vgl->mpp_northing, 0.0, 0.0, vgl->corner.easting, vgl->corner.northing);
			fclose(f);
			f = NULL;
		}
	} else {
		gtk_widget_destroy(file_selector);
	}
}

/**
 * Auto attempt to read the world file associated with the image used for the georef
 *  Based on simple file name conventions
 * Only attempted if the preference is on.
 */
static void maybe_read_world_file(VikFileEntry *vfe, void * user_data)
{
	if (a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "georef_auto_read_world_file")->b) {
		const char* filename = vik_file_entry_get_filename(VIK_FILE_ENTRY(vfe));
		double values[4];
		if (filename && user_data) {

			changeable_widgets *cw = (changeable_widgets *) user_data;

			bool upper = g_ascii_isupper(filename[strlen(filename)-1]);
			char* filew = g_strconcat(filename, (upper ? "W" : "w") , NULL);

			if (world_file_read_file(filew, values) == 0) {
				set_widget_values(cw, values);
			} else {
				if (strlen(filename) > 3) {
					char* file0 = g_strndup(filename, strlen(filename)-2);
					char* file1 = g_strdup_printf("%s%c%c", file0, filename[strlen(filename)-1], (upper ? 'W' : 'w') );
					if (world_file_read_file(file1, values) == 0) {
						set_widget_values(cw, values);
					}
					free(file1);
					free(file0);
				}
			}
			free(filew);
		}
	}
}

static struct LatLon get_ll_tl(VikGeorefLayer *vgl)
{
	struct LatLon ll_tl;
	ll_tl.lat = gtk_spin_button_get_value(GTK_SPIN_BUTTON(vgl->cw.lat_tl_spin));
	ll_tl.lon = gtk_spin_button_get_value(GTK_SPIN_BUTTON(vgl->cw.lon_tl_spin));
	return ll_tl;
}

static struct LatLon get_ll_br(VikGeorefLayer *vgl)
{
	struct LatLon ll_br;
	ll_br.lat = gtk_spin_button_get_value(GTK_SPIN_BUTTON(vgl->cw.lat_br_spin));
	ll_br.lon = gtk_spin_button_get_value(GTK_SPIN_BUTTON(vgl->cw.lon_br_spin));
	return ll_br;
}

// Align displayed UTM values with displayed Lat/Lon values
static void align_utm2ll(VikGeorefLayer *vgl)
{
	struct LatLon ll_tl = get_ll_tl(vgl);

	struct UTM utm;
	a_coords_latlon_to_utm(&ll_tl, &utm);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(vgl->cw.ce_spin), utm.easting);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(vgl->cw.cn_spin), utm.northing);

	char tmp_letter[2];
	tmp_letter[0] = utm.letter;
	tmp_letter[1] = '\0';
	gtk_entry_set_text(GTK_ENTRY(vgl->cw.utm_letter_entry), tmp_letter);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(vgl->cw.utm_zone_spin), utm.zone);
}

// Align displayed Lat/Lon values with displayed UTM values
static void align_ll2utm(VikGeorefLayer *vgl)
{
	struct UTM corner;
	const char *letter = gtk_entry_get_text(GTK_ENTRY(vgl->cw.utm_letter_entry));
	if (*letter) {
		corner.letter = toupper(*letter);
	}
	corner.zone = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(vgl->cw.utm_zone_spin));
	corner.easting = gtk_spin_button_get_value(GTK_SPIN_BUTTON(vgl->cw.ce_spin));
	corner.northing = gtk_spin_button_get_value(GTK_SPIN_BUTTON(vgl->cw.cn_spin));

	struct LatLon ll;
	a_coords_utm_to_latlon(&corner, &ll);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(vgl->cw.lat_tl_spin), ll.lat);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(vgl->cw.lon_tl_spin), ll.lon);
}

/**
 * Align coordinates between tabs as the user may have changed the values
 *   Use this before acting on the user input
 * This is easier then trying to use the 'value-changed' signal for each individual coordinate
 *  especiallly since it tends to end up in an infinite loop continually updating each other.
 */
static void align_coords(VikGeorefLayer *vgl)
{
	if (gtk_notebook_get_current_page(GTK_NOTEBOOK(vgl->cw.tabs)) == 0) {
		align_ll2utm(vgl);
	} else {
		align_utm2ll(vgl);
	}
}

static void switch_tab(GtkNotebook *notebook, void * tab, unsigned int tab_num, void * user_data)
{
	VikGeorefLayer *vgl = (VikGeorefLayer *) user_data;
	if (tab_num == 0) {
		align_utm2ll(vgl);
	} else {
		align_ll2utm(vgl);
	}
}

/**
 *
 */
static void check_br_is_good_or_msg_user(VikGeorefLayer *vgl)
{
	// if a 'blank' ll value that's alright
	if (vgl->ll_br.lat == 0.0 && vgl->ll_br.lon == 0.0) {
		return;
	}

	struct LatLon ll_tl = get_ll_tl(vgl);
	if (ll_tl.lat < vgl->ll_br.lat || ll_tl.lon > vgl->ll_br.lon) {
		a_dialog_warning_msg(VIK_GTK_WINDOW_FROM_LAYER(vgl), _("Lower right corner values may not be consistent with upper right values"));
	}
}

/**
 *
 */
static void calculate_mpp_from_coords(GtkWidget *ww, VikGeorefLayer *vgl)
{
	const char* filename = vik_file_entry_get_filename(VIK_FILE_ENTRY(vgl->cw.imageentry));
	if (!filename) {
		return;
	}
	GError *gx = NULL;
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(filename, &gx);
	if (gx) {
		a_dialog_error_msg_extra(VIK_GTK_WINDOW_FROM_WIDGET(ww), _("Couldn't open image file: %s"), gx->message);
		g_error_free(gx);
		return;
	}

	unsigned int width = gdk_pixbuf_get_width(pixbuf);
	unsigned int height = gdk_pixbuf_get_height(pixbuf);

	if (width == 0 || height == 0) {
		a_dialog_error_msg_extra(VIK_GTK_WINDOW_FROM_WIDGET(ww), _("Invalid image size: %s"), filename);
	} else {
		align_coords(vgl);

		struct LatLon ll_tl = get_ll_tl(vgl);
		struct LatLon ll_br = get_ll_br(vgl);

		double xmpp, ympp;
		georef_layer_mpp_from_coords(VIK_COORD_LATLON, ll_tl, ll_br, width, height, &xmpp, &ympp);

		gtk_spin_button_set_value(GTK_SPIN_BUTTON(vgl->cw.x_spin), xmpp);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(vgl->cw.y_spin), ympp);

		check_br_is_good_or_msg_user(vgl);
	}

	g_object_unref(G_OBJECT(pixbuf));
}

#define VIK_SETTINGS_GEOREF_TAB "georef_coordinate_tab"

/* returns true if OK was pressed. */
static bool georef_layer_dialog(VikGeorefLayer *vgl, Viewport * viewport, GtkWindow *w)
{
	GtkWidget *dialog = gtk_dialog_new_with_buttons(_("Layer Properties"),
							 w,
							 (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
							 GTK_STOCK_CANCEL,
							 GTK_RESPONSE_REJECT,
							 GTK_STOCK_OK,
							 GTK_RESPONSE_ACCEPT,
							 NULL);
	/* Default to reject as user really needs to specify map file first */
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);
	GtkWidget *response_w = NULL;
#if GTK_CHECK_VERSION (2, 20, 0)
	response_w = gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);
#endif
	GtkWidget *table, *wfp_hbox, *wfp_label, *wfp_button, *ce_label, *cn_label, *xlabel, *ylabel, *imagelabel;
	changeable_widgets cw;

	GtkBox *dgbox = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
	table = gtk_table_new (4, 2, false);
	gtk_box_pack_start (dgbox, table, true, true, 0);

	wfp_hbox = gtk_hbox_new (false, 0);
	wfp_label = gtk_label_new (_("World File Parameters:"));
	wfp_button = gtk_button_new_with_label (_("Load From File..."));

	gtk_box_pack_start (GTK_BOX(wfp_hbox), wfp_label, true, true, 0);
	gtk_box_pack_start (GTK_BOX(wfp_hbox), wfp_button, false, false, 3);

	ce_label = gtk_label_new (_("Corner pixel easting:"));
	cw.ce_spin = gtk_spin_button_new ((GtkAdjustment *) gtk_adjustment_new (4, 0.0, 1500000.0, 1, 5, 0), 1, 4);
	gtk_widget_set_tooltip_text (GTK_WIDGET(cw.ce_spin), _("the UTM \"easting\" value of the upper-left corner pixel of the map"));

	cn_label = gtk_label_new (_("Corner pixel northing:"));
	cw.cn_spin = gtk_spin_button_new ((GtkAdjustment *) gtk_adjustment_new (4, 0.0, 9000000.0, 1, 5, 0), 1, 4);
	gtk_widget_set_tooltip_text (GTK_WIDGET(cw.cn_spin), _("the UTM \"northing\" value of the upper-left corner pixel of the map"));

	xlabel = gtk_label_new (_("X (easting) scale (mpp): "));
	ylabel = gtk_label_new (_("Y (northing) scale (mpp): "));

	cw.x_spin = gtk_spin_button_new ((GtkAdjustment *) gtk_adjustment_new (4, VIK_VIEWPORT_MIN_ZOOM, VIK_VIEWPORT_MAX_ZOOM, 1, 5, 0), 1, 8);
	gtk_widget_set_tooltip_text (GTK_WIDGET(cw.x_spin), _("the scale of the map in the X direction (meters per pixel)"));
	cw.y_spin = gtk_spin_button_new ((GtkAdjustment *) gtk_adjustment_new (4, VIK_VIEWPORT_MIN_ZOOM, VIK_VIEWPORT_MAX_ZOOM, 1, 5, 0), 1, 8);
	gtk_widget_set_tooltip_text (GTK_WIDGET(cw.y_spin), _("the scale of the map in the Y direction (meters per pixel)"));

	imagelabel = gtk_label_new (_("Map Image:"));
	cw.imageentry = vik_file_entry_new (GTK_FILE_CHOOSER_ACTION_OPEN, VF_FILTER_IMAGE, maybe_read_world_file, &cw);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON(cw.ce_spin), vgl->corner.easting);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(cw.cn_spin), vgl->corner.northing);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(cw.x_spin), vgl->mpp_easting);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(cw.y_spin), vgl->mpp_northing);
	if (vgl->image) {
		vik_file_entry_set_filename (VIK_FILE_ENTRY(cw.imageentry), vgl->image);
	}

	gtk_table_attach_defaults (GTK_TABLE(table), imagelabel, 0, 1, 0, 1);
	gtk_table_attach_defaults (GTK_TABLE(table), cw.imageentry, 1, 2, 0, 1);
	gtk_table_attach_defaults (GTK_TABLE(table), wfp_hbox, 0, 2, 1, 2);
	gtk_table_attach_defaults (GTK_TABLE(table), xlabel, 0, 1, 2, 3);
	gtk_table_attach_defaults (GTK_TABLE(table), cw.x_spin, 1, 2, 2, 3);
	gtk_table_attach_defaults (GTK_TABLE(table), ylabel, 0, 1, 3, 4);
	gtk_table_attach_defaults (GTK_TABLE(table), cw.y_spin, 1, 2, 3, 4);

	cw.tabs = gtk_notebook_new();
	GtkWidget *table_utm = gtk_table_new (3, 2, false);

	gtk_table_attach_defaults (GTK_TABLE(table_utm), ce_label, 0, 1, 0, 1);
	gtk_table_attach_defaults (GTK_TABLE(table_utm), cw.ce_spin, 1, 2, 0, 1);
	gtk_table_attach_defaults (GTK_TABLE(table_utm), cn_label, 0, 1, 1, 2);
	gtk_table_attach_defaults (GTK_TABLE(table_utm), cw.cn_spin, 1, 2, 1, 2);

	GtkWidget *utm_hbox = gtk_hbox_new (false, 0);
	cw.utm_zone_spin = gtk_spin_button_new ((GtkAdjustment*)gtk_adjustment_new(vgl->corner.zone, 1, 60, 1, 5, 0), 1, 0);
	gtk_box_pack_start (GTK_BOX(utm_hbox), gtk_label_new(_("Zone:")), true, true, 0);
	gtk_box_pack_start (GTK_BOX(utm_hbox), cw.utm_zone_spin, true, true, 0);
	gtk_box_pack_start (GTK_BOX(utm_hbox), gtk_label_new(_("Letter:")), true, true, 0);
	cw.utm_letter_entry = gtk_entry_new ();
	gtk_entry_set_max_length (GTK_ENTRY(cw.utm_letter_entry), 1);
	gtk_entry_set_width_chars (GTK_ENTRY(cw.utm_letter_entry), 2);
	char tmp_letter[2];
	tmp_letter[0] = vgl->corner.letter;
	tmp_letter[1] = '\0';
	gtk_entry_set_text (GTK_ENTRY(cw.utm_letter_entry), tmp_letter);
	gtk_box_pack_start (GTK_BOX(utm_hbox), cw.utm_letter_entry, true, true, 0);

	gtk_table_attach_defaults (GTK_TABLE(table_utm), utm_hbox, 0, 2, 2, 3);

	// Lat/Lon
	GtkWidget *table_ll = gtk_table_new (5, 2, false);

	GtkWidget *lat_tl_label = gtk_label_new (_("Upper left latitude:"));
	cw.lat_tl_spin = gtk_spin_button_new ((GtkAdjustment *) gtk_adjustment_new (0.0,-90,90.0,0.05,0.1,0), 0.1, 6);
	GtkWidget *lon_tl_label = gtk_label_new (_("Upper left longitude:"));
	cw.lon_tl_spin = gtk_spin_button_new ((GtkAdjustment *) gtk_adjustment_new (0.0,-180,180.0,0.05,0.1,0), 0.1, 6);
	GtkWidget *lat_br_label = gtk_label_new (_("Lower right latitude:"));
	cw.lat_br_spin = gtk_spin_button_new ((GtkAdjustment *) gtk_adjustment_new (0.0,-90,90.0,0.05,0.1,0), 0.1, 6);
	GtkWidget *lon_br_label = gtk_label_new (_("Lower right longitude:"));
	cw.lon_br_spin = gtk_spin_button_new ((GtkAdjustment *) gtk_adjustment_new (0.0,-180.0,180.0,0.05,0.1,0), 0.1, 6);

	gtk_table_attach_defaults (GTK_TABLE(table_ll), lat_tl_label, 0, 1, 0, 1);
	gtk_table_attach_defaults (GTK_TABLE(table_ll), cw.lat_tl_spin, 1, 2, 0, 1);
	gtk_table_attach_defaults (GTK_TABLE(table_ll), lon_tl_label, 0, 1, 1, 2);
	gtk_table_attach_defaults (GTK_TABLE(table_ll), cw.lon_tl_spin, 1, 2, 1, 2);
	gtk_table_attach_defaults (GTK_TABLE(table_ll), lat_br_label, 0, 1, 2, 3);
	gtk_table_attach_defaults (GTK_TABLE(table_ll), cw.lat_br_spin, 1, 2, 2, 3);
	gtk_table_attach_defaults (GTK_TABLE(table_ll), lon_br_label, 0, 1, 3, 4);
	gtk_table_attach_defaults (GTK_TABLE(table_ll), cw.lon_br_spin, 1, 2, 3, 4);

	GtkWidget *calc_mpp_button = gtk_button_new_with_label (_("Calculate MPP values from coordinates"));
	gtk_widget_set_tooltip_text (calc_mpp_button, _("Enter all corner coordinates before calculating the MPP values from the image size"));
	gtk_table_attach_defaults (GTK_TABLE(table_ll), calc_mpp_button, 0, 2, 4, 5);

	VikCoord vc;
	vik_coord_load_from_utm (&vc, VIK_COORD_LATLON, &(vgl->corner));
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(cw.lat_tl_spin), vc.north_south);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(cw.lon_tl_spin), vc.east_west);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(cw.lat_br_spin), vgl->ll_br.lat);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(cw.lon_br_spin), vgl->ll_br.lon);

	gtk_notebook_append_page(GTK_NOTEBOOK(cw.tabs), GTK_WIDGET(table_utm), gtk_label_new(_("UTM")));
	gtk_notebook_append_page(GTK_NOTEBOOK(cw.tabs), GTK_WIDGET(table_ll), gtk_label_new(_("Latitude/Longitude")));
	gtk_box_pack_start (dgbox, cw.tabs, true, true, 0);

	GtkWidget *alpha_hbox = gtk_hbox_new (false, 0);
	// GTK3 => GtkWidget *alpha_scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 255, 1);
	GtkWidget *alpha_scale = gtk_hscale_new_with_range (0, 255, 1);
	gtk_scale_set_digits (GTK_SCALE(alpha_scale), 0);
	gtk_range_set_value (GTK_RANGE(alpha_scale), vgl->alpha);
	gtk_box_pack_start (GTK_BOX(alpha_hbox), gtk_label_new(_("Alpha:")), true, true, 0);
	gtk_box_pack_start (GTK_BOX(alpha_hbox), alpha_scale, true, true, 0);
	gtk_box_pack_start (dgbox, alpha_hbox, true, true, 0);

	vgl->cw = cw;

	g_signal_connect (G_OBJECT(vgl->cw.tabs), "switch-page", G_CALLBACK(switch_tab), vgl);
	g_signal_connect (G_OBJECT(calc_mpp_button), "clicked", G_CALLBACK(calculate_mpp_from_coords), vgl);

	g_signal_connect_swapped (G_OBJECT(wfp_button), "clicked", G_CALLBACK(georef_layer_dialog_load), &cw);

	if (response_w) {
		gtk_widget_grab_focus (response_w);
	}

	gtk_widget_show_all (dialog);

	// Remember setting the notebook page must be done after the widget is visible.
	int page_num = 0;
	if (a_settings_get_integer (VIK_SETTINGS_GEOREF_TAB, &page_num)) {
		if (page_num < 0 || page_num > 1) {
			page_num = 0;
		}
	}
	gtk_notebook_set_current_page (GTK_NOTEBOOK(cw.tabs), page_num);

	if (gtk_dialog_run (GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		align_coords (vgl);

		vgl->corner.easting = gtk_spin_button_get_value (GTK_SPIN_BUTTON(cw.ce_spin));
		vgl->corner.northing = gtk_spin_button_get_value (GTK_SPIN_BUTTON(cw.cn_spin));
		vgl->corner.zone = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(cw.utm_zone_spin));
		const char *letter = gtk_entry_get_text (GTK_ENTRY(cw.utm_letter_entry));
		if (*letter) {
			vgl->corner.letter = toupper(*letter);
		}
		vgl->mpp_easting = gtk_spin_button_get_value (GTK_SPIN_BUTTON(cw.x_spin));
		vgl->mpp_northing = gtk_spin_button_get_value (GTK_SPIN_BUTTON(cw.y_spin));
		vgl->ll_br = get_ll_br (vgl);
		check_br_is_good_or_msg_user (vgl);
		// TODO check if image has changed otherwise no need to regenerate pixbuf
		if (!vgl->pixbuf) {
			if (g_strcmp0 (vgl->image, vik_file_entry_get_filename(VIK_FILE_ENTRY(cw.imageentry))) != 0) {
				georef_layer_set_image (vgl, vik_file_entry_get_filename(VIK_FILE_ENTRY(cw.imageentry)));
				((LayerGeoref *) ((VikLayer *) vgl)->layer)->post_read(viewport, false);
			}
		}

		vgl->alpha = (uint8_t) gtk_range_get_value (GTK_RANGE(alpha_scale));
		if (vgl->pixbuf && vgl->alpha < 255) {
			vgl->pixbuf = ui_pixbuf_set_alpha (vgl->pixbuf, vgl->alpha);
		}

		if (vgl->scaled && vgl->alpha < 255) {
			vgl->scaled = ui_pixbuf_set_alpha (vgl->scaled, vgl->alpha);
		}

		a_settings_set_integer (VIK_SETTINGS_GEOREF_TAB, gtk_notebook_get_current_page(GTK_NOTEBOOK(cw.tabs)));

		gtk_widget_destroy (GTK_WIDGET(dialog));
		return true;
	}
	gtk_widget_destroy (GTK_WIDGET(dialog));
	return false;
}

static void georef_layer_zoom_to_fit(void * vgl_vlp[2])
{
	VikLayersPanel * vlp = VIK_LAYERS_PANEL(vgl_vlp[1]);
	vlp->panel_ref->get_viewport()->set_xmpp(VIK_GEOREF_LAYER(vgl_vlp[0])->mpp_easting);
	vlp->panel_ref->get_viewport()->set_ympp(VIK_GEOREF_LAYER(vgl_vlp[0])->mpp_northing);
	vlp->panel_ref->emit_update();
}

static void georef_layer_goto_center(void * vgl_vlp[2])
{
	VikGeorefLayer *vgl = VIK_GEOREF_LAYER (vgl_vlp[0]);
	VikLayersPanel * vlp = VIK_LAYERS_PANEL(vgl_vlp[1]);
	Viewport * viewport = vlp->panel_ref->get_viewport();
	struct UTM utm;
	VikCoord coord;

	vik_coord_to_utm(viewport->get_center(), &utm);

	utm.easting = vgl->corner.easting + (vgl->width * vgl->mpp_easting / 2); /* only an approximation */
	utm.northing = vgl->corner.northing - (vgl->height * vgl->mpp_northing / 2);

	vik_coord_load_from_utm(&coord, viewport->get_coord_mode(), &utm);
	viewport->set_center_coord(&coord, true);

	vik_layers_panel_emit_update_cb(VIK_LAYERS_PANEL(vgl_vlp[1])->panel_ref);
}

void LayerGeoref::add_menu_items(GtkMenu *menu, void * vlp)
{
	VikGeorefLayer * vgl = (VikGeorefLayer *) this->vl;

	static void * pass_along[2];
	GtkWidget *item;
	pass_along[0] = vgl;
	pass_along[1] = vlp;

	item = gtk_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	/* Now with icons */
	item = gtk_image_menu_item_new_with_mnemonic(_("_Zoom to Fit Map"));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(georef_layer_zoom_to_fit), pass_along);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	item = gtk_image_menu_item_new_with_mnemonic(_("_Goto Map Center"));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(georef_layer_goto_center), pass_along);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	item = gtk_image_menu_item_new_with_mnemonic(_("_Export to World File"));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_HARDDISK, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(georef_layer_export_params), pass_along);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);
}


static void * georef_layer_move_create(VikWindow *vw, Viewport * viewport)
{
	return viewport->vvp;
}

static bool georef_layer_move_release(VikGeorefLayer *vgl, GdkEventButton *event, Viewport * viewport)
{
	if (!vgl || ((Layer *) ((VikLayer *) vgl)->layer)->type != VIK_LAYER_GEOREF) {
		return false;
	}

	if (vgl->click_x != -1) {
		vgl->corner.easting += (event->x - vgl->click_x) * viewport->get_xmpp();
		vgl->corner.northing -= (event->y - vgl->click_y) * viewport->get_ympp();
		vik_layer_emit_update(VIK_LAYER(vgl));
		return true;
	}
	return false; /* I didn't move anything on this layer! */
}

static void * georef_layer_zoom_create(VikWindow *vw, Viewport * viewport)
{
	return viewport->vvp;
}

static bool georef_layer_zoom_press(VikGeorefLayer *vgl, GdkEventButton *event, Viewport * viewport)
{
	if (!vgl || ((Layer *) ((VikLayer *) vgl)->layer)->type != VIK_LAYER_GEOREF) {
		return false;
	}

	if (event->button == 1) {
		if (vgl->mpp_easting < (VIK_VIEWPORT_MAX_ZOOM / 1.05) && vgl->mpp_northing < (VIK_VIEWPORT_MAX_ZOOM / 1.05)) {
			vgl->mpp_easting *= 1.01;
			vgl->mpp_northing *= 1.01;
		}
	} else {
		if (vgl->mpp_easting > (VIK_VIEWPORT_MIN_ZOOM * 1.05) && vgl->mpp_northing > (VIK_VIEWPORT_MIN_ZOOM * 1.05)) {
			vgl->mpp_easting /= 1.01;
			vgl->mpp_northing /= 1.01;
		}
	}
	viewport->set_xmpp(vgl->mpp_easting);
	viewport->set_ympp(vgl->mpp_northing);
	vik_layer_emit_update(VIK_LAYER(vgl));
	return true;
}

static bool georef_layer_move_press(VikGeorefLayer *vgl, GdkEventButton *event, Viewport * viewport)
{
	if (!vgl || ((Layer *) ((VikLayer *) vgl)->layer)->type != VIK_LAYER_GEOREF) {
		return false;
	}
	vgl->click_x = event->x;
	vgl->click_y = event->y;
	return true;
}

static void goto_center_ll(Viewport * viewport, struct LatLon ll_tl, struct LatLon ll_br)
{
	VikCoord vc_center;
	struct LatLon ll_center;

	ll_center.lat = (ll_tl.lat + ll_br.lat) / 2.0;
	ll_center.lon = (ll_tl.lon + ll_br.lon) / 2.0;

	vik_coord_load_from_latlon(&vc_center, viewport->get_coord_mode(), &ll_center);
	viewport->set_center_coord(&vc_center, true);
}

/**
 *
 */
VikGeorefLayer *vik_georef_layer_create(Viewport * viewport,
					VikLayersPanel *vlp,
					const char *name,
					GdkPixbuf *pixbuf,
					VikCoord *coord_tl,
					VikCoord *coord_br)
{
	VikGeorefLayer *vgl = georef_layer_new(viewport);
	((Layer *) ((VikLayer *) vgl)->layer)->rename(name);

	vgl->pixbuf = pixbuf;

	vik_coord_to_utm(coord_tl, &(vgl->corner));
	vik_coord_to_latlon(coord_br, &(vgl->ll_br));

	if (vgl->pixbuf) {
		vgl->width = gdk_pixbuf_get_width(vgl->pixbuf);
		vgl->height = gdk_pixbuf_get_height(vgl->pixbuf);

		if (vgl->width > 0 && vgl->height > 0) {

			struct LatLon ll_tl;
			vik_coord_to_latlon(coord_tl, &ll_tl);
			struct LatLon ll_br;
			vik_coord_to_latlon(coord_br, &ll_br);

			VikCoordMode mode = viewport->get_coord_mode();

			double xmpp, ympp;
			georef_layer_mpp_from_coords(mode, ll_tl, ll_br, vgl->width, vgl->height, &xmpp, &ympp);
			vgl->mpp_easting = xmpp;
			vgl->mpp_northing = ympp;

			goto_center_ll(viewport, ll_tl, ll_br);
			// Set best zoom level
			struct LatLon maxmin[2] = { ll_tl, ll_br };
			vu_zoom_to_show_latlons(viewport->get_coord_mode(), viewport, maxmin);

			return vgl;
		}
	}

	// Bad image
	LayerGeoref * layer = (LayerGeoref *) ((VikLayer *) vgl)->layer;
	layer->free_();
	return NULL;
}





LayerGeoref::LayerGeoref()
{
	this->type = VIK_LAYER_GEOREF;
	strcpy(this->type_string, "GEOREF");
}





LayerGeoref::LayerGeoref(VikLayer * vl) : Layer(vl)
{
	this->type = VIK_LAYER_GEOREF;
	strcpy(this->type_string, "GEOREF");
}





LayerGeoref::LayerGeoref(Viewport * viewport) : LayerGeoref()
{
	VikGeorefLayer *vgl = VIK_GEOREF_LAYER(g_object_new(VIK_GEOREF_LAYER_TYPE, NULL));

	((VikLayer *) vgl)->layer = this;
	this->vl = (VikLayer *) vgl;

	// Since GeoRef layer doesn't use uibuilder
	//  initializing this way won't do anything yet..
	vik_layer_set_defaults(VIK_LAYER(vgl), viewport);

	// Make these defaults based on the current view
	vgl->mpp_northing = viewport->get_ympp();
	vgl->mpp_easting = viewport->get_xmpp();
	vik_coord_to_utm(viewport->get_center(), &(vgl->corner));

	vgl->image = NULL;
	vgl->pixbuf = NULL;
	vgl->click_x = -1;
	vgl->click_y = -1;
	vgl->scaled = NULL;
	vgl->scaled_width = 0;
	vgl->scaled_height = 0;
	vgl->ll_br.lat = 0.0;
	vgl->ll_br.lon = 0.0;
	vgl->alpha = 255;
}
