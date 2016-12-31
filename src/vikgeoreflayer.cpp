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

#include <cstring>
#include <cmath>
#include <cstdlib>
#include <cctype>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "vikutils.h"
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




using namespace SlavGPS;




/*
static VikParameterValue image_default(void)
{
	VikParameterValue data;
	data.s = strdup("");
	return data;
}
*/




enum {
	LAYER_GEOREF_TOOL_MOVE = 0,
	LAYER_GEOREF_TOOL_ZOOM
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
	NUM_PARAMS
};




Parameter georef_layer_params[] = {
	{ PARAM_IMAGE, "image",                ParameterType::STRING, VIK_LAYER_NOT_IN_PROPERTIES, NULL, WidgetType::NONE,        NULL, NULL, NULL, NULL, NULL, NULL },
	{ PARAM_CE,    "corner_easting",       ParameterType::DOUBLE, VIK_LAYER_NOT_IN_PROPERTIES, NULL, WidgetType::NONE,        NULL, NULL, NULL, NULL, NULL, NULL },
	{ PARAM_CN,    "corner_northing",      ParameterType::DOUBLE, VIK_LAYER_NOT_IN_PROPERTIES, NULL, WidgetType::NONE,        NULL, NULL, NULL, NULL, NULL, NULL },
	{ PARAM_ME,    "mpp_easting",          ParameterType::DOUBLE, VIK_LAYER_NOT_IN_PROPERTIES, NULL, WidgetType::NONE,        NULL, NULL, NULL, NULL, NULL, NULL },
	{ PARAM_MN,    "mpp_northing",         ParameterType::DOUBLE, VIK_LAYER_NOT_IN_PROPERTIES, NULL, WidgetType::NONE,        NULL, NULL, NULL, NULL, NULL, NULL },
	{ PARAM_CZ,    "corner_zone",          ParameterType::UINT,   VIK_LAYER_NOT_IN_PROPERTIES, NULL, WidgetType::NONE,        NULL, NULL, NULL, NULL, NULL, NULL },
	{ PARAM_CL,    "corner_letter_as_int", ParameterType::UINT,   VIK_LAYER_NOT_IN_PROPERTIES, NULL, WidgetType::NONE,        NULL, NULL, NULL, NULL, NULL, NULL },
	{ PARAM_AA,    "alpha",                ParameterType::UINT,   VIK_LAYER_NOT_IN_PROPERTIES, NULL, WidgetType::NONE,        NULL, NULL, NULL, NULL, NULL, NULL },

	{ NUM_PARAMS,  NULL,                   ParameterType::PTR,    VIK_LAYER_GROUP_NONE,        NULL, WidgetType::CHECKBUTTON, NULL, NULL, NULL, NULL, NULL, NULL }, /* Guard. */
};




/* Tools. */
static LayerTool * georef_layer_move_create(Window * window, Viewport * viewport);
static LayerTool * georef_layer_zoom_create(Window * window, Viewport * viewport);




LayerGeorefInterface vik_georef_layer_interface();




LayerGeorefInterface::LayerGeorefInterface()
{
	this->params = georef_layer_params; /* Parameters. */
	this->params_count = NUM_PARAMS;

	strndup(this->layer_type_string, "GeoRef Map", sizeof (this->layer_type_string) - 1); /* Non-translatable. */
	this->layer_type_string[sizeof (this->layer_type_string) - 1] - 1 = '\0';

	this->layer_name = tr("GeoRef Map");
	// this->action_accelerator = ...; /* Empty accelerator. */
	// this->action_icon = ...; /* Set elsewhere. */

	this->layer_tool_constructors.insert({{ LAYER_GEOREF_TOOL_MOVE, georef_layer_move_create }});
	this->layer_tool_constructors.insert({{ LAYER_GEOREF_TOOL_ZOOM, georef_layer_zoom_create }});

	this->menu_items_selection = VIK_MENU_ITEM_ALL;
}




static Parameter io_prefs[] = {
	{ 0, VIKING_PREFERENCES_IO_NAMESPACE "georef_auto_read_world_file", ParameterType::BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Auto Read World Files:"), WidgetType::CHECKBUTTON, NULL, NULL, N_("Automatically attempt to read associated world file of a new image for a GeoRef layer"), NULL, NULL, NULL}
};

typedef struct {
	LayerGeoref * layer;
	LayersPanel * panel;
} georef_data_t;




void SlavGPS::vik_georef_layer_init(void)
{
	ParameterValue tmp;
	tmp.b = true;
	a_preferences_register(&io_prefs[0], tmp, VIKING_PREFERENCES_IO_GROUP_KEY);
}




QString LayerGeoref::tooltip()
{
	return this->image;
}




Layer * LayerGeorefInterface::unmarshall(uint8_t * data, int len, Viewport * viewport)
{
	LayerGeoref * grl = new LayerGeoref();
	glr->configure_from_viewport(viewport);

	grl->unmarshall_params(data, len);

	if (grl->image) {
		grl->post_read(viewport, true);
	}
	return (Layer *) grl;
}




bool LayerGeoref::set_param_value(uint16_t id, ParameterValue data, bool is_file_operation)
{
	switch (id) {
	case PARAM_IMAGE:
		this->set_image(data.s);
		break;
	case PARAM_CN:
		this->corner.northing = data.d;
		break;
	case PARAM_CE:
		this->corner.easting = data.d;
		break;
	case PARAM_MN:
		this->mpp_northing = data.d;
		break;
	case PARAM_ME:
		this->mpp_easting = data.d;
		break;
	case PARAM_CZ:
		if (data.u <= 60) {
			this->corner.zone = data.u;
		}
		break;
	case PARAM_CL:
		if (data.u >= 65 || data.u <= 90) {
			this->corner.letter = data.u;
		}
		break;
	case PARAM_AA:
		if (data.u <= 255) {
			this->alpha = data.u;
		}
		break;
	default:
		break;
	}
	return true;
}




void LayerGeoref::create_image_file()
{
	/* Create in .viking-maps. */
	char *filename = g_strconcat(maps_layer_default_dir(), this->get_name(), ".jpg", NULL);
	GError *error = NULL;
	gdk_pixbuf_save(this->pixbuf, filename, "jpeg", &error, NULL);
	if (error) {
		fprintf(stderr, "WARNING: %s\n", error->message);
		g_error_free(error);
	} else {
		this->image = g_strdup(filename);
	}

	free(filename);
}




ParameterValue LayerGeoref::get_param_value(param_id_t id, bool is_file_operation) const
{
	ParameterValue rv;
	switch (id) {
	case PARAM_IMAGE: {
		bool set = false;
		if (is_file_operation) {
			if (this->pixbuf && !this->image) {
				/* Force creation of image file. */
				((LayerGeoref *) this)->create_image_file();
			}
			if (Preferences::get_file_ref_format() == VIK_FILE_REF_FORMAT_RELATIVE) {
				char *cwd = g_get_current_dir();
				if (cwd) {
					rv.s = file_GetRelativeFilename(cwd, this->image);
					if (!rv.s) rv.s = "";
					set = true;
				}
			}
		}
		if (!set) {
			rv.s = this->image ? this->image : "";
		}
		break;
	}
	case PARAM_CN:
		rv.d = this->corner.northing;
		break;
	case PARAM_CE:
		rv.d = this->corner.easting;
		break;
	case PARAM_MN:
		rv.d = this->mpp_northing;
		break;
	case PARAM_ME:
		rv.d = this->mpp_easting;
		break;
	case PARAM_CZ:
		rv.u = this->corner.zone;
		break;
	case PARAM_CL:
		rv.u = this->corner.letter;
		break;
	case PARAM_AA:
		rv.u = this->alpha;
		break;
	default:
		break;
	}
	return rv;
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

	/* UTM mode should be exact MPP. */
	double factor = 1.0;
	if (mode == VIK_COORD_LATLON) {
		/* NB the 1.193 - is at the Equator.
		   http://wiki.openstreetmap.org/wiki/Zoom_levels */

		/* Convert from actual image MPP to Viking 'pixelfact'. */
		double mid_lat = (ll_bl.lat + ll_tr.lat) / 2.0;
		/* Protect against div by zero (but shouldn't have 90 degrees for mid latitude...). */
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
	if (this->pixbuf) {
		double xmpp = viewport->get_xmpp(), ympp = viewport->get_ympp();
		GdkPixbuf *pixbuf = this->pixbuf;
		unsigned int layer_width = this->width;
		unsigned int layer_height = this->height;

		unsigned int width = viewport->get_width(), height = viewport->get_height();
		int32_t x, y;
		VikCoord corner_coord;
		vik_coord_load_from_utm(&corner_coord, viewport->get_coord_mode(), &(this->corner));
		viewport->coord_to_screen(&corner_coord, &x, &y);

		/* Mark to scale the pixbuf if it doesn't match our dimensions. */
		bool scale = false;
		if (xmpp != this->mpp_easting || ympp != this->mpp_northing) {
			scale = true;
			layer_width = round(this->width * this->mpp_easting / xmpp);
			layer_height = round(this->height * this->mpp_northing / ympp);
		}

		/* If image not in viewport bounds - no need to draw it (or bother with any scaling). */
		if ((x < 0 || x < width) && (y < 0 || y < height) && x+layer_width > 0 && y+layer_height > 0) {

			if (scale) {
				/* Rescale if necessary. */
				if (layer_width == this->scaled_width && layer_height == this->scaled_height && this->scaled != NULL) {
					pixbuf = this->scaled;
				} else {
					pixbuf = gdk_pixbuf_scale_simple(this->pixbuf,
									 layer_width,
									 layer_height,
									 GDK_INTERP_BILINEAR);

					if (this->scaled != NULL) {
						g_object_unref(this->scaled);
					}

					this->scaled = pixbuf;
					this->scaled_width = layer_width;
					this->scaled_height = layer_height;
				}
			}
			viewport->draw_pixmap(pixbuf, 0, 0, x, y, layer_width, layer_height); /* todo: draw only what we need to. */
		}
	}
}




LayerGeoref::~LayerGeoref()
{
	if (this->image != NULL) {
		free(this->image);
	}

	if (this->scaled != NULL) {
		g_object_unref(this->scaled);
	}
}




bool LayerGeoref::properties_dialog(Viewport * viewport)
{
	return this->dialog(viewport, viewport->get_toolkit_window());
}




/* Also known as LayerGeoref::load_image(). */
void LayerGeoref::post_read(Viewport * viewport, bool from_file)
{
	GError *gx = NULL;
	if (this->image == NULL) {
		return;
	}

	if (this->pixbuf) {
		g_object_unref(G_OBJECT(this->pixbuf));
	}

	if (this->scaled) {
		g_object_unref(G_OBJECT(this->scaled));
		this->scaled = NULL;
	}

	this->pixbuf = gdk_pixbuf_new_from_file (this->image, &gx);

	if (gx) {
		if (!from_file) {
			dialog_error(QString("Couldn't open image file: %1").arg(QString(gx->message)), this->get_window());
		}
		g_error_free (gx);
	} else {
		this->width = gdk_pixbuf_get_width(this->pixbuf);
		this->height = gdk_pixbuf_get_height(this->pixbuf);

		if (this->pixbuf && this->alpha < 255) {
			this->pixbuf = ui_pixbuf_set_alpha(this->pixbuf, this->alpha);
		}
	}
	/* Should find length and width here too. */
}




void LayerGeoref::set_image(char const * image)
{
	if (this->image) {
		free(this->image);
	}

	if (this->scaled) {
		g_object_unref(this->scaled);
		this->scaled = NULL;
	}
	if (image == NULL) {
		this->image = NULL;
	}

	if (strcmp(image, "") != 0) {
		this->image = vu_get_canonical_filename(this, image);
	} else {
		this->image = g_strdup(image);
	}
}




/* Only positive values allowed here. */
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
	bool answer = true; /* Success. */
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
 *  x&y coords as UTM eastings and northings respectively.
 */
static int world_file_read_file(const char* filename, double values[4])
{
	fprintf(stderr, "DEBUG: %s - trying world file %s\n", __FUNCTION__, filename);

	FILE *f = fopen(filename, "r");
	if (!f) {
		return 1;
	} else {
		int answer = 2; /* Not enough info read yet. */
		/* **We do not handle 'skew' values ATM - normally they are a value of 0 anyway to align with the UTM grid. */
		if (world_file_read_line(f, &values[0], true) /* x scale. */
		    && world_file_read_line(f, NULL, false) /* Ignore value in y-skew line**. */
		    && world_file_read_line(f, NULL, false) /* Ignore value in x-skew line**. */
		    && world_file_read_line(f, &values[1], true) /* y scale. */
		    && world_file_read_line(f, &values[2], true) /* x-coordinate of the upper left pixel. */
		    && world_file_read_line(f, &values[3], true) /* y-coordinate of the upper left pixel. */
		    )
			{
				/* Success. */
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
			dialog_error("The World file you requested could not be opened for reading.", VIK_GTK_WINDOW_FROM_WIDGET(cw->x_spin));
		} else if (answer == 2) {
			ialog_error("Unexpected end of file reading World file.", VIK_GTK_WINDOW_FROM_WIDGET(cw->x_spin));
		} else {
			/* NB answer should == 0 for success. */
			set_widget_values(cw, values);
		}
	}

	gtk_widget_destroy(file_selector);
}




static void georef_layer_export_params(georef_data_t * data)
{
	LayerGeoref * layer = data->layer;

	GtkWidget * file_selector = gtk_file_chooser_dialog_new(_("Choose World file"),
								NULL,
								GTK_FILE_CHOOSER_ACTION_SAVE,
								GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
								GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
								NULL);
	if (gtk_dialog_run(GTK_DIALOG (file_selector)) == GTK_RESPONSE_ACCEPT) {
		FILE * f = fopen(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_selector)), "w");

		gtk_widget_destroy(file_selector);
		if (!f) {
			dialog_error("The file you requested could not be opened for writing."), layer->get_window());
			return;
		} else {
			fprintf(f, "%f\n%f\n%f\n%f\n%f\n%f\n", layer->mpp_easting, layer->mpp_northing, 0.0, 0.0, layer->corner.easting, layer->corner.northing);
			fclose(f);
			f = NULL;
		}
	} else {
		gtk_widget_destroy(file_selector);
	}
}




/**
 * Auto attempt to read the world file associated with the image used for the georef.
 * Based on simple file name conventions.
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




struct LatLon LayerGeoref::get_ll_tl()
{
	struct LatLon ll_tl;
	ll_tl.lat = gtk_spin_button_get_value(GTK_SPIN_BUTTON(this->cw.lat_tl_spin));
	ll_tl.lon = gtk_spin_button_get_value(GTK_SPIN_BUTTON(this->cw.lon_tl_spin));
	return ll_tl;
}




struct LatLon LayerGeoref::get_ll_br()
{
	struct LatLon ll_br;
	ll_br.lat = gtk_spin_button_get_value(GTK_SPIN_BUTTON(this->cw.lat_br_spin));
	ll_br.lon = gtk_spin_button_get_value(GTK_SPIN_BUTTON(this->cw.lon_br_spin));
	return ll_br;
}




/* Align displayed UTM values with displayed Lat/Lon values. */
void LayerGeoref::align_utm2ll()
{
	struct LatLon ll_tl = this->get_ll_tl();

	struct UTM utm;
	a_coords_latlon_to_utm(&ll_tl, &utm);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(this->cw.ce_spin), utm.easting);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(this->cw.cn_spin), utm.northing);

	char tmp_letter[2];
	tmp_letter[0] = utm.letter;
	tmp_letter[1] = '\0';
	gtk_entry_set_text(GTK_ENTRY(this->cw.utm_letter_entry), tmp_letter);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(this->cw.utm_zone_spin), utm.zone);
}




/* Align displayed Lat/Lon values with displayed UTM values. */
void LayerGeoref::align_ll2utm()
{
	struct UTM corner;
	const char *letter = gtk_entry_get_text(GTK_ENTRY(this->cw.utm_letter_entry));
	if (*letter) {
		corner.letter = toupper(*letter);
	}
	corner.zone = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(this->cw.utm_zone_spin));
	corner.easting = gtk_spin_button_get_value(GTK_SPIN_BUTTON(this->cw.ce_spin));
	corner.northing = gtk_spin_button_get_value(GTK_SPIN_BUTTON(this->cw.cn_spin));

	struct LatLon ll;
	a_coords_utm_to_latlon(&corner, &ll);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(this->cw.lat_tl_spin), ll.lat);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(this->cw.lon_tl_spin), ll.lon);
}




/**
 * Align coordinates between tabs as the user may have changed the values.
 * Use this before acting on the user input.
 * This is easier then trying to use the 'value-changed' signal for each individual coordinate
 * especially since it tends to end up in an infinite loop continually updating each other.
 */
void LayerGeoref::align_coords()
{
	if (gtk_notebook_get_current_page(GTK_NOTEBOOK(this->cw.tabs)) == 0) {
		this->align_ll2utm();
	} else {
		this->align_utm2ll();
	}
}




static void switch_tab(GtkNotebook *notebook, void * tab, unsigned int tab_num, void * user_data)
{
	LayerGeoref * layer = (LayerGeoref *) user_data;

	if (tab_num == 0) {
		layer->align_utm2ll();
	} else {
		layer->align_ll2utm();
	}
}




void LayerGeoref::check_br_is_good_or_msg_user()
{
	/* If a 'blank' ll value that's alright. */
	if (this->ll_br.lat == 0.0 && this->ll_br.lon == 0.0) {
		return;
	}

	struct LatLon ll_tl = this->get_ll_tl();
	if (ll_tl.lat < this->ll_br.lat || ll_tl.lon > this->ll_br.lon) {
		dialog_warning("Lower right corner values may not be consistent with upper right values", this->get_window());
	}
}




static void calculate_mpp_from_coords_cb(GtkWidget * ww, LayerGeoref * layer)
{
	layer->calculate_mpp_from_coords(ww);
}




void LayerGeoref::calculate_mpp_from_coords(GtkWidget * ww)
{
	const char* filename = vik_file_entry_get_filename(VIK_FILE_ENTRY(this->cw.imageentry));
	if (!filename) {
		return;
	}
	GError *gx = NULL;
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(filename, &gx);
	if (gx) {
		dialog_error(QString("Couldn't open image file: %1").arg(QString(gx->message)), VIK_GTK_WINDOW_FROM_WIDGET(ww));
		g_error_free(gx);
		return;
	}

	unsigned int width = gdk_pixbuf_get_width(pixbuf);
	unsigned int height = gdk_pixbuf_get_height(pixbuf);

	if (width == 0 || height == 0) {
		dialog_error(QString("Invalid image size: %1").arg(QString(filename)), VIK_GTK_WINDOW_FROM_WIDGET(ww));
	} else {
		this->align_coords();

		struct LatLon ll_tl = this->get_ll_tl();
		struct LatLon ll_br = this->get_ll_br();

		double xmpp, ympp;
		georef_layer_mpp_from_coords(VIK_COORD_LATLON, ll_tl, ll_br, width, height, &xmpp, &ympp);

		gtk_spin_button_set_value(GTK_SPIN_BUTTON(this->cw.x_spin), xmpp);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(this->cw.y_spin), ympp);

		this->check_br_is_good_or_msg_user();
	}

	g_object_unref(G_OBJECT(pixbuf));
}




#define VIK_SETTINGS_GEOREF_TAB "georef_coordinate_tab"




/* Returns true if OK was pressed. */
bool LayerGeoref::dialog(Viewport * viewport, GtkWindow * w)
{
	GtkWidget *dialog = gtk_dialog_new_with_buttons(_("Layer Properties"),
							w,
							(GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
							GTK_STOCK_CANCEL,
							GTK_RESPONSE_REJECT,
							GTK_STOCK_OK,
							GTK_RESPONSE_ACCEPT,
							NULL);

	/* Default to reject as user really needs to specify map file first. */
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

	gtk_spin_button_set_value (GTK_SPIN_BUTTON(cw.ce_spin), this->corner.easting);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(cw.cn_spin), this->corner.northing);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(cw.x_spin), this->mpp_easting);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(cw.y_spin), this->mpp_northing);
	if (this->image) {
		vik_file_entry_set_filename (VIK_FILE_ENTRY(cw.imageentry), this->image);
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
	cw.utm_zone_spin = gtk_spin_button_new ((GtkAdjustment*)gtk_adjustment_new(this->corner.zone, 1, 60, 1, 5, 0), 1, 0);
	gtk_box_pack_start (GTK_BOX(utm_hbox), gtk_label_new(_("Zone:")), true, true, 0);
	gtk_box_pack_start (GTK_BOX(utm_hbox), cw.utm_zone_spin, true, true, 0);
	gtk_box_pack_start (GTK_BOX(utm_hbox), gtk_label_new(_("Letter:")), true, true, 0);
	cw.utm_letter_entry = gtk_entry_new ();
	gtk_entry_set_max_length (GTK_ENTRY(cw.utm_letter_entry), 1);
	gtk_entry_set_width_chars (GTK_ENTRY(cw.utm_letter_entry), 2);
	char tmp_letter[2];
	tmp_letter[0] = this->corner.letter;
	tmp_letter[1] = '\0';
	gtk_entry_set_text (GTK_ENTRY(cw.utm_letter_entry), tmp_letter);
	gtk_box_pack_start (GTK_BOX(utm_hbox), cw.utm_letter_entry, true, true, 0);

	gtk_table_attach_defaults (GTK_TABLE(table_utm), utm_hbox, 0, 2, 2, 3);

	/* Lat/Lon. */
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
	vik_coord_load_from_utm (&vc, VIK_COORD_LATLON, &(this->corner));
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(cw.lat_tl_spin), vc.north_south);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(cw.lon_tl_spin), vc.east_west);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(cw.lat_br_spin), this->ll_br.lat);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(cw.lon_br_spin), this->ll_br.lon);

	gtk_notebook_append_page(GTK_NOTEBOOK(cw.tabs), GTK_WIDGET(table_utm), gtk_label_new(_("UTM")));
	gtk_notebook_append_page(GTK_NOTEBOOK(cw.tabs), GTK_WIDGET(table_ll), gtk_label_new(_("Latitude/Longitude")));
	gtk_box_pack_start (dgbox, cw.tabs, true, true, 0);

	GtkWidget *alpha_hbox = gtk_hbox_new (false, 0);
	// GTK3 => GtkWidget *alpha_scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 255, 1);
	GtkWidget *alpha_scale = gtk_hscale_new_with_range (0, 255, 1);
	gtk_scale_set_digits (GTK_SCALE(alpha_scale), 0);
	gtk_range_set_value (GTK_RANGE(alpha_scale), this->alpha);
	gtk_box_pack_start (GTK_BOX(alpha_hbox), gtk_label_new(_("Alpha:")), true, true, 0);
	gtk_box_pack_start (GTK_BOX(alpha_hbox), alpha_scale, true, true, 0);
	gtk_box_pack_start (dgbox, alpha_hbox, true, true, 0);

	this->cw = cw;

	g_signal_connect (G_OBJECT(this->cw.tabs), "switch-page", G_CALLBACK(switch_tab), this);
	g_signal_connect (G_OBJECT(calc_mpp_button), "clicked", G_CALLBACK(calculate_mpp_from_coords_cb), this);

	g_signal_connect_swapped (G_OBJECT(wfp_button), "clicked", G_CALLBACK(georef_layer_dialog_load), &cw);

	if (response_w) {
		gtk_widget_grab_focus (response_w);
	}

	gtk_widget_show_all (dialog);

	/* Remember setting the notebook page must be done after the widget is visible. */
	int page_num = 0;
	if (a_settings_get_integer (VIK_SETTINGS_GEOREF_TAB, &page_num)) {
		if (page_num < 0 || page_num > 1) {
			page_num = 0;
		}
	}
	gtk_notebook_set_current_page (GTK_NOTEBOOK(cw.tabs), page_num);

	if (gtk_dialog_run (GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		this->align_coords();

		this->corner.easting = gtk_spin_button_get_value (GTK_SPIN_BUTTON(cw.ce_spin));
		this->corner.northing = gtk_spin_button_get_value (GTK_SPIN_BUTTON(cw.cn_spin));
		this->corner.zone = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(cw.utm_zone_spin));
		const char *letter = gtk_entry_get_text (GTK_ENTRY(cw.utm_letter_entry));
		if (*letter) {
			this->corner.letter = toupper(*letter);
		}
		this->mpp_easting = gtk_spin_button_get_value (GTK_SPIN_BUTTON(cw.x_spin));
		this->mpp_northing = gtk_spin_button_get_value (GTK_SPIN_BUTTON(cw.y_spin));
		this->ll_br = this->get_ll_br();
		this->check_br_is_good_or_msg_user();
		/* TODO check if image has changed otherwise no need to regenerate pixbuf. */
		if (!this->pixbuf) {
			if (g_strcmp0 (this->image, vik_file_entry_get_filename(VIK_FILE_ENTRY(cw.imageentry))) != 0) {
				this->set_image(vik_file_entry_get_filename(VIK_FILE_ENTRY(cw.imageentry)));
				this->post_read(viewport, false);
			}
		}

		this->alpha = (uint8_t) gtk_range_get_value (GTK_RANGE(alpha_scale));
		if (this->pixbuf && this->alpha < 255) {
			this->pixbuf = ui_pixbuf_set_alpha (this->pixbuf, this->alpha);
		}

		if (this->scaled && this->alpha < 255) {
			this->scaled = ui_pixbuf_set_alpha(this->scaled, this->alpha);
		}

		a_settings_set_integer (VIK_SETTINGS_GEOREF_TAB, gtk_notebook_get_current_page(GTK_NOTEBOOK(cw.tabs)));

		gtk_widget_destroy (GTK_WIDGET(dialog));
		return true;
	}
	gtk_widget_destroy (GTK_WIDGET(dialog));
	return false;
}




static void georef_layer_zoom_to_fit(georef_data_t * data)
{
	LayerGeoref * layer = data->layer;
	LayersPanel * panel = data->panel;

	panel->get_viewport()->set_xmpp(layer->mpp_easting);
	panel->get_viewport()->set_ympp(layer->mpp_northing);
	panel->emit_update();
}




static void georef_layer_goto_center(georef_data_t * data)
{
	LayerGeoref * layer = data->layer;
	LayersPanel * panel = data->panel;
	Viewport * viewport = panel->get_viewport();

	struct UTM utm;
	VikCoord coord;

	vik_coord_to_utm(viewport->get_center(), &utm);

	utm.easting = layer->corner.easting + (layer->width * layer->mpp_easting / 2); /* Only an approximation. */
	utm.northing = layer->corner.northing - (layer->height * layer->mpp_northing / 2);

	vik_coord_load_from_utm(&coord, viewport->get_coord_mode(), &utm);
	viewport->set_center_coord(&coord, true);

	panel->emit_update();
}




void LayerGeoref::add_menu_items(QMenu & menu)
{
	static georef_data_t pass_along;
	pass_along.layer = this;
	pass_along.panel = (LayersPanel *) panel;

	GtkWidget *item;

	item = gtk_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	/* Now with icons. */
	item = gtk_image_menu_item_new_with_mnemonic(_("_Zoom to Fit Map"));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(georef_layer_zoom_to_fit), &pass_along);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	item = gtk_image_menu_item_new_with_mnemonic(_("_Goto Map Center"));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(georef_layer_goto_center), &pass_along);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	item = gtk_image_menu_item_new_with_mnemonic(_("_Export to World File"));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_HARDDISK, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(georef_layer_export_params), &pass_along);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);
}




static LayerTool * georef_layer_move_create(Window * window, Viewport * viewport)
{
	return new LayerToolGeorefMove(window, viewport);
}




LayerToolGeorefMove::LayerToolGeorefMove(Window * window, Viewport * viewport) : LayerTool(window, viewport, LayerType::GEOREF)
{
	this->id_string = QString("GeorefMoveMap");

	this->action_icon_path   = "vik-icon-Georef Move Map";
	this->action_label       = QObject::tr("_Georef Move Map");
	this->action_tooltip     = QObject::tr("Georef Move Map");
	// this->action_accelerator = ...; /* Empty accelerator. */

	this->cursor_shape = Qt::BitmapCursor;
	this->cursor_data = &cursor_geomove_pixbuf;

	Layer::get_interface(LayerType::GEOREF)->layer_tools.insert({{ LAYER_GEOREF_TOOL_MOVE, this }});
}




LayerToolFuncStatus LayerToolGeorefMove::release_(Layer * vgl, QMouseEvent * event)
{
	return ((LayerGeoref *) vgl)->move_release(event, this);
}




bool LayerGeoref::move_release(GdkEventButton * event, LayerTool * tool)
{
	if (this->type != LayerType::GEOREF) {
		/* kamilFIXME: this shouldn't happen, right? */
		return false;
	}

	if (this->click_x != -1) {
		this->corner.easting += (event->x - this->click_x) * viewport->get_xmpp();
		this->corner.northing -= (event->y - this->click_y) * viewport->get_ympp();
		this->emit_changed();
		return true;
	}
	return false; /* I didn't move anything on this layer! */
}




static LayerTool * georef_layer_zoom_create(Window * window, Viewport * viewport)
{
	return new LayerToolGeorefZoom(window, viewport);
}




LayerToolGeorefZoom::LayerToolGeorefZoom(Window * window, Viewport * viewport) : LayerTool(window, viewport, LayerType::GEOREF)
{
	this->id_string = QString("GeorefZoomTool");

	this->action_icon_path   = "vik-icon-Georef Zoom Tool";
	this->action_label       = QObject::tr("Georef Z&oom Tool");
	this->action_tooltip     = QObject::tr("Georef Zoom Tool");
	// this->action_accelerator = ...; /* Empty accelerator. */

	this->cursor_shape = Qt::BitmapCursor;
	this->cursor_data = &cursor_geozoom_pixbuf;

	Layer::get_interface(LayerType::GEOREF)->layer_tools.insert({{ LAYER_GEOREF_TOOL_ZOOM, this }});
}




bool LayerToolGeorefZoom::click_(Layer * vgl, QMouseEvent * event)
{
	return ((LayerGeoref *) vgl)->zoom_press(event, this);
}




bool LayerGeoref::zoom_press(GdkEventButton * event, LayerTool * tool)
{
	if (this->type != LayerType::GEOREF) {
		/* kamilFIXME: this shouldn't happen, right? */
		return false;
	}

	if (event->button() == Qt::LeftButton) {
		if (this->mpp_easting < (VIK_VIEWPORT_MAX_ZOOM / 1.05) && this->mpp_northing < (VIK_VIEWPORT_MAX_ZOOM / 1.05)) {
			this->mpp_easting *= 1.01;
			this->mpp_northing *= 1.01;
		}
	} else {
		if (this->mpp_easting > (VIK_VIEWPORT_MIN_ZOOM * 1.05) && this->mpp_northing > (VIK_VIEWPORT_MIN_ZOOM * 1.05)) {
			this->mpp_easting /= 1.01;
			this->mpp_northing /= 1.01;
		}
	}
	tool->viewport->set_xmpp(this->mpp_easting);
	tool->viewport->set_ympp(this->mpp_northing);
	this->emit_changed();
	return true;
}




LayerToolFuncStatus LayerToolGeorefMove::click_(Layer * vgl, QMouseEvent *event)
{
	return ((LayerGeoref *) vgl)->move_press(event, this);
}




bool LayerGeoref::move_press(GdkEventButton * event, LayerTool * tool)
{
	if (this->type != LayerType::GEOREF) {
		/* kamilFIXME: this shouldn't happen, right? */
		return false;
	}
	this->click_x = event->x;
	this->click_y = event->y;
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




LayerGeoref * SlavGPS::vik_georef_layer_create(Viewport * viewport,
					       const char *name,
					       GdkPixbuf *pixbuf,
					       VikCoord *coord_tl,
					       VikCoord *coord_br)
{
	LayerGeoref * grl = new LayerGeoref();
	glr->configure_from_viewport(viewport);
	grl->rename(name);
	grl->pixbuf = pixbuf;

	vik_coord_to_utm(coord_tl, &(grl->corner));
	vik_coord_to_latlon(coord_br, &(grl->ll_br));

	if (grl->pixbuf) {
		grl->width = gdk_pixbuf_get_width(grl->pixbuf);
		grl->height = gdk_pixbuf_get_height(grl->pixbuf);

		if (grl->width > 0 && grl->height > 0) {

			struct LatLon ll_tl;
			vik_coord_to_latlon(coord_tl, &ll_tl);
			struct LatLon ll_br;
			vik_coord_to_latlon(coord_br, &ll_br);

			VikCoordMode mode = viewport->get_coord_mode();

			double xmpp, ympp;
			georef_layer_mpp_from_coords(mode, ll_tl, ll_br, grl->width, grl->height, &xmpp, &ympp);
			grl->mpp_easting = xmpp;
			grl->mpp_northing = ympp;

			goto_center_ll(viewport, ll_tl, ll_br);
			/* Set best zoom level. */
			struct LatLon maxmin[2] = { ll_tl, ll_br };
			vu_zoom_to_show_latlons(viewport->get_coord_mode(), viewport, maxmin);

			return grl;
		}
	}

	/* Bad image. */
	delete grl;
	return NULL;
}




LayerGeoref::LayerGeoref()
{
	this->type = LayerType::GEOREF;
	strcpy(this->type_string, "GEOREF");
	this->interface = &vik_georef_layer_interface;

	/* Since GeoRef layer doesn't use uibuilder initializing this
	   way won't do anything yet... */
	this->set_initial_parameter_values();

	this->ll_br.lat = 0.0;
	this->ll_br.lon = 0.0;
}




/* To be called right after constructor. */
void LayerGeoref::configure_from_viewport(Viewport const * viewport)
{
	/* Make these defaults based on the current view. */
	this->mpp_northing = viewport->get_ympp();
	this->mpp_easting = viewport->get_xmpp();
	vik_coord_to_utm(viewport->get_center(), &this->corner);
}
