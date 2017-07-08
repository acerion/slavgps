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

#include <QDebug>

#include "vikutils.h"
#include "ui_util.h"
#include "preferences.h"
#include "icons/icons.h"
#include "layer_map.h"
#include "layer_georef.h"
#include "widget_file_entry.h"
#include "dialog.h"
#include "file.h"
#include "settings.h"
#include "globals.h"
#include "layers_panel.h"
#include "util.h"
#include "viewport_zoom.h"




using namespace SlavGPS;




/*
static ParameterValue image_default(void)
{
	return ParameterValue(strdup(""));
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




LayerGeorefInterface vik_georef_layer_interface;




LayerGeorefInterface::LayerGeorefInterface()
{
	this->params = georef_layer_params; /* Parameters. */
	this->params_count = NUM_PARAMS;

	strncpy(this->layer_type_string, "GeoRef Map", sizeof (this->layer_type_string)); /* Non-translatable. */
	this->layer_type_string[sizeof (this->layer_type_string) - 1] = '\0';

	this->layer_name = QObject::tr("GeoRef Map");
	// this->action_accelerator = ...; /* Empty accelerator. */
	// this->action_icon = ...; /* Set elsewhere. */

	this->layer_tool_constructors.insert({{ LAYER_GEOREF_TOOL_MOVE, georef_layer_move_create }});
	this->layer_tool_constructors.insert({{ LAYER_GEOREF_TOOL_ZOOM, georef_layer_zoom_create }});

	this->menu_items_selection = LayerMenuItem::ALL;
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
	ParameterValue((bool) true);
#ifdef K
	a_preferences_register(&io_prefs[0], tmp, VIKING_PREFERENCES_IO_GROUP_KEY);
#endif
}




QString LayerGeoref::tooltip()
{
	return this->image;
}




Layer * LayerGeorefInterface::unmarshall(uint8_t * data, int len, Viewport * viewport)
{
	LayerGeoref * grl = new LayerGeoref();
	grl->configure_from_viewport(viewport);

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
	char * filename = g_strconcat(maps_layer_default_dir(), this->get_name(), ".jpg", NULL);
	if (!this->pixmap->save(filename, "jpeg")) {
		qDebug() << "WW: Layer Georef: failed to save pixmap to" << filename;
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
			if (this->pixmap && !this->image) {
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
static void georef_layer_mpp_from_coords(CoordMode mode, struct LatLon ll_tl, struct LatLon ll_br, unsigned int width, unsigned int height, double *xmpp, double *ympp)
{
	struct LatLon ll_tr;
	ll_tr.lat = ll_tl.lat;
	ll_tr.lon = ll_br.lon;

	struct LatLon ll_bl;
	ll_bl.lat = ll_br.lat;
	ll_bl.lon = ll_tl.lon;

	/* UTM mode should be exact MPP. */
	double factor = 1.0;
	if (mode == CoordMode::LATLON) {
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
	if (this->pixmap) {
		double xmpp = viewport->get_xmpp(), ympp = viewport->get_ympp();
		QPixmap * pixmap_ = this->pixmap;
		int layer_width = this->width;
		int layer_height = this->height;

		const int width_ = viewport->get_width();
		const int height_ = viewport->get_height();
		int x, y;
		Coord corner_coord(this->corner, viewport->get_coord_mode());
		viewport->coord_to_screen(&corner_coord, &x, &y);

		/* Mark to scale the pixmap if it doesn't match our dimensions. */
		bool scale = false;
		if (xmpp != this->mpp_easting || ympp != this->mpp_northing) {
			scale = true;
			layer_width = round(this->width * this->mpp_easting / xmpp);
			layer_height = round(this->height * this->mpp_northing / ympp);
		}

		/* If image not in viewport bounds - no need to draw it (or bother with any scaling). */
		if ((x < 0 || x < width_) && (y < 0 || y < height_) && x+layer_width > 0 && y+layer_height > 0) {

			if (scale) {
				/* Rescale if necessary. */
				if (layer_width == this->scaled_width && layer_height == this->scaled_height && this->scaled != NULL) {
					pixmap_ = this->scaled;
				} else {
#ifdef K
					pixmap_ = this->pixmap->scaled(layer_width, layer_height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
#endif

					if (this->scaled != NULL) {
#ifdef K
						g_object_unref(this->scaled);
#endif
					}

					this->scaled = pixmap_;
					this->scaled_width = layer_width;
					this->scaled_height = layer_height;
				}
			}
			viewport->draw_pixmap(*pixmap_, 0, 0, x, y, layer_width, layer_height); /* todo: draw only what we need to. */
		}
	}
}




LayerGeoref::~LayerGeoref()
{
	if (this->image != NULL) {
		free(this->image);
	}

	if (this->scaled != NULL) {
#ifdef K
		g_object_unref(this->scaled);
#endif
	}
}




bool LayerGeoref::properties_dialog(Viewport * viewport)
{
	return this->dialog(viewport, viewport->get_window());
}




/* Also known as LayerGeoref::load_image(). */
void LayerGeoref::post_read(Viewport * viewport, bool from_file)
{
	GError *gx = NULL;
	if (this->image == NULL) {
		return;
	}

	if (this->pixmap) {
#ifdef K
		g_object_unref(G_OBJECT(this->pixmap));
#endif
	}

	if (this->scaled) {
#ifdef K
		g_object_unref(G_OBJECT(this->scaled));
#endif
		this->scaled = NULL;
	}
#ifdef K
	this->pixmap = new QPixmap();
	if (!pixmap->load(this->image)) {
		delete pixmap;
		pixmap = NULL;
		if (!from_file) {
			dialog_error(QString("Couldn't open image file %1").arg(QString(this->image)), this->get_window());
		}
	} else {
		this->width = this->pixmap->width();
		this->height = this->pixmap->height();

		if (this->pixmap && this->alpha < 255) {
			this->pixmap = ui_pixmap_set_alpha(this->pixmap, this->alpha);
		}
	}
#endif
	/* Should find length and width here too. */
}




void LayerGeoref::set_image(char const * image_)
{
	if (this->image) {
		free(this->image);
	}

	if (this->scaled) {
#ifdef K
		g_object_unref(this->scaled);
#endif
		this->scaled = NULL;
	}
	if (image_ == NULL) {
		this->image = NULL;
	}

	if (strcmp(image_, "") != 0) {
		this->image = vu_get_canonical_filename(this, image_);
	} else {
		this->image = g_strdup(image_);
	}
}




/* Only positive values allowed here. */
static void double2spinwidget(QDoubleSpinBox * spinbox, double val)
{
	spinbox->setValue(val > 0 ? val : -val);
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
#ifdef K
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
			dialog_error("The World file you requested could not be opened for reading.", this->get_window());
		} else if (answer == 2) {
			dialog_error("Unexpected end of file reading World file.", this->get_window());
		} else {
			/* NB answer should == 0 for success. */
			set_widget_values(cw, values);
		}
	}

	gtk_widget_destroy(file_selector);
#endif
}




static void georef_layer_export_params(georef_data_t * data)
{
	LayerGeoref * layer = data->layer;
#ifdef K
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
#endif
}




/**
 * Auto attempt to read the world file associated with the image used for the georef.
 * Based on simple file name conventions.
 * Only attempted if the preference is on.
 */
static void maybe_read_world_file(SGFileEntry * file_entry, void * user_data)
{
#if 0
	if (a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "georef_auto_read_world_file")->b) {
		const QString filename = file_entry->get_filename();
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
#endif
}




struct LatLon LayerGeoref::get_ll_tl()
{
	struct LatLon ll_result;
	ll_result.lat = this->cw.lat_tl_spin.value();
	ll_result.lon = this->cw.lon_tl_spin.value();
	return ll_result;
}




struct LatLon LayerGeoref::get_ll_br()
{
	struct LatLon ll_result;
	ll_result.lat = this->cw.lat_br_spin.value();
	ll_result.lon = this->cw.lon_br_spin.value();
	return ll_result;
}




/* Align displayed UTM values with displayed Lat/Lon values. */
void LayerGeoref::align_utm2ll()
{
	struct LatLon ll_tl = this->get_ll_tl();

	struct UTM utm;
	a_coords_latlon_to_utm(&utm, &ll_tl);

	this->cw.ce_spin->setValue(utm.easting);
	this->cw.cn_spin->setValue(utm.northing);

	char tmp_letter[2];
	tmp_letter[0] = utm.letter;
	tmp_letter[1] = '\0';
	this->cw.utm_letter_entry.setText(QString(tmp_letter));

	this->cw.utm_zone_spin.setValue(utm.zone);

}




/* Align displayed Lat/Lon values with displayed UTM values. */
void LayerGeoref::align_ll2utm()
{
#ifdef K
	struct UTM corner;
	const char *letter = this->cw.utm_letter_entry.text();
	if (*letter) {
		corner.letter = toupper(*letter);
	}
#endif
	corner.zone = this->cw.utm_zone_spin.value();
	corner.easting = this->cw.ce_spin->value();
	corner.northing = this->cw.cn_spin->value();

	struct LatLon ll;
	a_coords_utm_to_latlon(&ll, &corner);
	this->cw.lat_tl_spin.setValue(ll.lat);
	this->cw.lon_tl_spin.setValue(ll.lon);
}




/**
 * Align coordinates between tabs as the user may have changed the values.
 * Use this before acting on the user input.
 * This is easier then trying to use the 'value-changed' signal for each individual coordinate
 * especially since it tends to end up in an infinite loop continually updating each other.
 */
void LayerGeoref::align_coords()
{
#ifdef K
	if (gtk_notebook_get_current_page(GTK_NOTEBOOK(this->cw.tabs)) == 0) {
		this->align_ll2utm();
	} else {
		this->align_utm2ll();
	}
#endif
}




static void switch_tab(void *notebook, void * tab, unsigned int tab_num, void * user_data)
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




void LayerGeoref::calculate_mpp_from_coords_cb(void)
{
	const QString filename = this->cw.imageentry->get_filename();
	if (!filename.length()) {
		return;
	}

	QPixmap * img_pixmap = new QPixmap();
	if (!img_pixmap->load(filename)) {
		delete img_pixmap;
		img_pixmap = NULL;

		dialog_error(QString("Couldn't open image file %1").arg(filename), this->get_window());
		return;
	}

	const int img_width = img_pixmap->width();
	const int img_height = img_pixmap->height();

	if (img_width == 0 || img_height == 0) {
		dialog_error(QString("Invalid image size: %1").arg(filename), this->get_window());
	} else {
		this->align_coords();

		double xmpp, ympp;
		georef_layer_mpp_from_coords(CoordMode::LATLON, this->get_ll_tl(), this->get_ll_br(), img_width, img_height, &xmpp, &ympp);

		this->cw.x_spin->setValue(xmpp);
		this->cw.y_spin->setValue(ympp);

		this->check_br_is_good_or_msg_user();
	}
#ifdef K
	g_object_unref(G_OBJECT(img_pixmap));
#endif
}




#define VIK_SETTINGS_GEOREF_TAB "georef_coordinate_tab"




/* Returns true if OK was pressed. */
bool LayerGeoref::dialog(Viewport * viewport, Window * window)
{
#ifdef K
	GtkWidget *dialog = gtk_dialog_new_with_buttons(_("Layer Properties"),
							window,
							(GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
							GTK_STOCK_CANCEL,
							GTK_RESPONSE_REJECT,
							GTK_STOCK_OK,
							GTK_RESPONSE_ACCEPT,
							NULL);

	/* Default to reject as user really needs to specify map file first. */
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);
	GtkWidget *response_w = NULL;
	response_w = gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);
	GtkWidget *table, *wfp_hbox, *wfp_button;
	changeable_widgets cw;

	GtkBox *dgbox = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
	table = gtk_table_new (4, 2, false);
	dgbox->addWidget(table);

	wfp_hbox = gtk_hbox_new (false, 0);
	QLabel * wfp_label = new QLabel(QObject::tr("World File Parameters:"));
	wfp_button = gtk_button_new_with_label (_("Load From File..."));

	gtk_box_pack_start (GTK_BOX(wfp_hbox), wfp_label, true, true, 0);
	gtk_box_pack_start (GTK_BOX(wfp_hbox), wfp_button, false, false, 3);

	QLabel * ce_label = new QLabel(QObject::tr("Corner pixel easting:"));
	cw.ce_spin.setValue(4);
	cw.ce_spin.setMinimum(0.0);
	cw.ce_spin.setMaximum(1500000.0);
	cw.ce_spin.setSingleStep(1);
	cw.ce_spin.setToolTip(QObject::tr("the UTM \"easting\" value of the upper-left corner pixel of the map"));

	QLabel * cn_label = new QLabel(QObject::tr("Corner pixel northing:"));
	cw.cn_spin.setValue(4);
	cw.cn_spin.setMinimum(0.0);
	cw.cn_spin.setMaximum(9000000.0);
	cw.cn_spin.setSingleStep(1);
	cw.cn_spin.setToolTip(QObject::tr("the UTM \"northing\" value of the upper-left corner pixel of the map"));

	QLabel * xlabel = new QLabel(QObject::tr("X (easting) scale (mpp): "));
	QLabel * ylabel = new QLabel(QObject::tr("Y (northing) scale (mpp): "));

	cw.x_spin.setValue(4);
	cw.x_spin.setMinimum(SG_VIEWPORT_ZOOM_MIN);
	cw.x_spin.setMaximum(SG_VIEWPORT_ZOOM_MAX);
	cw.x_spin.setSingleStep(1);
	cw.x_spin.setToolTip(QObject::tr("the scale of the map in the X direction (meters per pixel)"));

	cw.y_spin.setValue(4);
	cw.y_spin.setMinimum(SG_VIEWPORT_ZOOM_MIN);
	cw.y_spin.setMaximum(SG_VIEWPORT_ZOOM_MAX);
	cw.y_spin.setSingleStep(1);
	cw.y_spin.setToolTip(QObject::tr("the scale of the map in the Y direction (meters per pixel)"));

	QLabel * imagelabel = new QLabel(QObject::tr("Map Image:"));
	cw.imageentry = new SGFileEntry(); vik_file_entry_new (GTK_FILE_CHOOSER_ACTION_OPEN, VF_FILTER_IMAGE, maybe_read_world_file, &cw);

	cw.ce_spin.setValue(this->corner.easting);
	cw.cn_spin.setValue(this->corner.northing);
	cw.x_spin.setValue(this->mpp_easting);
	cw.y_spin.setValue(this->mpp_northing);
	if (this->image) {
		cw.imageentry->set_filename(this->image);
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
	cw.utm_zone_spin.setValue(this->corner.zone);
	cw.utm_zone_spin.setMinimum(1);
	cw.utm_zone_spin.setMaximum(60);
	cw.utm_zone_spin.setSingleStep(1);

	gtk_box_pack_start (GTK_BOX(utm_hbox), new QLabel(QObject::tr("Zone:")), true, true, 0);
	gtk_box_pack_start (GTK_BOX(utm_hbox), cw.utm_zone_spin, true, true, 0);
	gtk_box_pack_start (GTK_BOX(utm_hbox), new QLabel(QObject::tr("Letter:")), true, true, 0);
	cw.utm_letter_entry.setMaxLength(1);
	gtk_entry_set_width_chars (GTK_ENTRY(cw.utm_letter_entry), 2);
	char tmp_letter[2];
	tmp_letter[0] = this->corner.letter;
	tmp_letter[1] = '\0';
	cw.utm_letter_entry.setText(tmp_letter);
	gtk_box_pack_start (GTK_BOX(utm_hbox), cw.utm_letter_entry, true, true, 0);

	gtk_table_attach_defaults (GTK_TABLE(table_utm), utm_hbox, 0, 2, 2, 3);

	/* Lat/Lon. */
	GtkWidget *table_ll = gtk_table_new (5, 2, false);

	QLabel * lat_tl_label = new QLabel(QObject::tr("Upper left latitude:"));
	cw.lat_tl_spin.setValue(0.0);
	cw.lat_tl_spin.setMinimum(-90.0);
	cw.lat_tl_spin.setMaximum(90.0);
	cw.lat_tl_spin.setSingleStep(0.05);

	QLabel * lon_tl_label = new QLabel(QObject::tr("Upper left longitude:"));
	cw.lon_tl_spin.setValue(0.0);
	cw.lon_tl_spin.setMinimum(-180.0);
	cw.lon_tl_spin.setMaximum(180.0);
	cw.lon_tl_spin.setSingleStep(0.05);

	QLabel * lat_br_label = new QLabel(QObject::tr("Lower right latitude:"));
	cw.lat_br_spin.setValue(0.0);
	cw.lat_br_spin.setMinimum(-90.0);
	cw.lat_br_spin.setMaximum(90.0);
	cw.lat_br_spin.setSingleStep(0.05);

	QLabel * lon_br_label = new QLabel(QObject::tr("Lower right longitude:"));
	cw.lon_br_spin.setValue(0.0);
	cw.lon_br_spin.setMinimum(-180.0);
	cw.lon_br_spin.setMaximum(180.0);
	cw.lon_br_spin.setSingleStep(0.05);

	gtk_table_attach_defaults (GTK_TABLE(table_ll), lat_tl_label, 0, 1, 0, 1);
	gtk_table_attach_defaults (GTK_TABLE(table_ll), cw.lat_tl_spin, 1, 2, 0, 1);
	gtk_table_attach_defaults (GTK_TABLE(table_ll), lon_tl_label, 0, 1, 1, 2);
	gtk_table_attach_defaults (GTK_TABLE(table_ll), cw.lon_tl_spin, 1, 2, 1, 2);
	gtk_table_attach_defaults (GTK_TABLE(table_ll), lat_br_label, 0, 1, 2, 3);
	gtk_table_attach_defaults (GTK_TABLE(table_ll), cw.lat_br_spin, 1, 2, 2, 3);
	gtk_table_attach_defaults (GTK_TABLE(table_ll), lon_br_label, 0, 1, 3, 4);
	gtk_table_attach_defaults (GTK_TABLE(table_ll), cw.lon_br_spin, 1, 2, 3, 4);

	GtkWidget *calc_mpp_button = gtk_button_new_with_label (_("Calculate MPP values from coordinates"));
	calc_mpp_button->setToolTip(QObject::tr("Enter all corner coordinates before calculating the MPP values from the image size"));
	gtk_table_attach_defaults (GTK_TABLE(table_ll), calc_mpp_button, 0, 2, 4, 5);

	Coord coord(this->corner, CoordMode::LATLON);
	cw.lat_tl_spin.setValue(coord.ll.lat);
	cw.lon_tl_spin.setValue(coord.ll.lon);
	cw.lat_br_spin.setValue(this->ll_br.lat);
	cw.lon_br_spin.setValue(this->ll_br.lon);

	gtk_notebook_append_page(GTK_NOTEBOOK(cw.tabs), GTK_WIDGET(table_utm), new QLabel(QObject::tr("UTM")));
	gtk_notebook_append_page(GTK_NOTEBOOK(cw.tabs), GTK_WIDGET(table_ll), new QLabel(QObject::tr("Latitude/Longitude")));
	dgbox->addWidget(cw.tabs);

	GtkWidget *alpha_hbox = gtk_hbox_new (false, 0);
	// GTK3 => GtkWidget *alpha_scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 255, 1);
	GtkWidget *alpha_scale = gtk_hscale_new_with_range (0, 255, 1);
	gtk_scale_set_digits (GTK_SCALE(alpha_scale), 0);
	gtk_range_set_value (GTK_RANGE(alpha_scale), this->alpha);
	gtk_box_pack_start (GTK_BOX(alpha_hbox), new QLabel(QObject::tr("Alpha:")), true, true, 0);
	gtk_box_pack_start (GTK_BOX(alpha_hbox), alpha_scale, true, true, 0);
	dgbox->addWidget(alpha_hbox);

	this->cw = cw;

	QObject::connect(this->cw.tabs, SIGNAL("switch-page"), this, SLOT (switch_tab));
	QObject::connect(calc_mpp_button, SIGNAL("clicked"), this, SLOT (calculate_mpp_from_coords_cb));

	QObject::connect(wfp_button, SIGNAL("clicked"), &cw, SLOT (georef_layer_dialog_load));

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

		this->corner.easting = cw.ce_spin.value();
		this->corner.northing = cw.cn_spin.value();
		this->corner.zone = cw.utm_zone_spin.value();
		const char *letter = cw.utm_letter_entry.text();
		if (*letter) {
			this->corner.letter = toupper(*letter);
		}
		this->mpp_easting = cw.x_spin.value();
		this->mpp_northing = cw.y_spin.value();
		this->ll_br = this->get_ll_br();
		this->check_br_is_good_or_msg_user();
		/* TODO check if image has changed otherwise no need to regenerate pixmap. */
		if (!this->pixmap) {
			if (g_strcmp0 (this->image, cw.imageentry->get_filename()) != 0) {
				this->set_image(cw.imageentry->get_filename());
				this->post_read(viewport, false);
			}
		}

		this->alpha = (uint8_t) gtk_range_get_value (GTK_RANGE(alpha_scale));
		if (this->pixmap && this->alpha < 255) {
			this->pixmap = ui_pixmap_set_alpha(this->pixmap, this->alpha);
		}

		if (this->scaled && this->alpha < 255) {
			this->scaled = ui_pixmap_set_alpha(this->scaled, this->alpha);
		}

		a_settings_set_integer (VIK_SETTINGS_GEOREF_TAB, gtk_notebook_get_current_page(GTK_NOTEBOOK(cw.tabs)));

		gtk_widget_destroy (GTK_WIDGET(dialog));
		return true;
	}
	gtk_widget_destroy (GTK_WIDGET(dialog));
#endif
	return false;
}




static void georef_layer_zoom_to_fit(georef_data_t * data)
{
	LayerGeoref * layer = data->layer;
	LayersPanel * panel = data->panel;

	panel->get_viewport()->set_xmpp(layer->mpp_easting);
	panel->get_viewport()->set_ympp(layer->mpp_northing);
	panel->emit_update_cb();
}




static void georef_layer_goto_center(georef_data_t * data)
{
	LayerGeoref * layer = data->layer;
	LayersPanel * panel = data->panel;
	Viewport * viewport = panel->get_viewport();

	struct UTM utm = viewport->get_center()->get_utm();

	utm.easting = layer->corner.easting + (layer->width * layer->mpp_easting / 2); /* Only an approximation. */
	utm.northing = layer->corner.northing - (layer->height * layer->mpp_northing / 2);

	Coord coord(utm, viewport->get_coord_mode());
	viewport->set_center_coord(coord, true);

	panel->emit_update_cb();
}




void LayerGeoref::add_menu_items(QMenu & menu)
{
#ifdef K
	static georef_data_t pass_along;
	pass_along.layer = this;
	pass_along.panel = (LayersPanel *) panel;

	QAction * action = NULL;

	action = new QAction(QObject::tr("&Zoom to Fit Map"), this);
	action->setIcon(QIcon::fromTheme("GTK_STOCK_ZOOM_FIT"));
	QObject::connect(action, SIGNAL (triggered(bool)), &pass_along, SLOT (georef_layer_zoom_to_fit));
	menu->addAction(action);

	action = new QAction(QObject::tr("&Goto Map Center"), this);
	action->setIcon(QIcon::fromTheme("GTK_STOCK_JUMP_TO"));
	QObject::connect(action, SIGNAL (triggered(bool)), &pass_along, SLOT (georef_layer_goto_center));
	menu->addAction(action);

	action = new QAction(QObject::tr("&Export to World File"), this);
	action->setIcon(QIcon::fromTheme("GTK_STOCK_HARDDISK"));
	QObject::connect(action, SIGNAL (triggered(bool)), &pass_along, SLOT (georef_layer_export_params));
	menu->addAction(action);
#endif
}




static LayerTool * georef_layer_move_create(Window * window, Viewport * viewport)
{
	return new LayerToolGeorefMove(window, viewport);
}




LayerToolGeorefMove::LayerToolGeorefMove(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::GEOREF)
{
	this->id_string = QString("georef.move_map");

	this->action_icon_path   = "vik-icon-Georef Move Map";
	this->action_label       = QObject::tr("_Georef Move Map");
	this->action_tooltip     = QObject::tr("Georef Move Map");
	// this->action_accelerator = ...; /* Empty accelerator. */

#ifdef K
	this->cursor_shape = Qt::BitmapCursor;
	this->cursor_data = &cursor_geomove_pixmap;
#endif

	Layer::get_interface(LayerType::GEOREF)->layer_tools.insert({{ LAYER_GEOREF_TOOL_MOVE, this }});
}




LayerToolFuncStatus LayerToolGeorefMove::release_(Layer * vgl, QMouseEvent * ev)
{
	return (LayerToolFuncStatus) ((LayerGeoref *) vgl)->move_release(ev, this); /* kamilFIXME: resolve this cast of returned value. */
}




bool LayerGeoref::move_release(QMouseEvent * ev, LayerTool * tool)
{
	if (this->type != LayerType::GEOREF) {
		/* kamilFIXME: this shouldn't happen, right? */
		return false;
	}

	if (this->click_x != -1) {
		this->corner.easting += (ev->x() - this->click_x) * tool->viewport->get_xmpp();
		this->corner.northing -= (ev->y() - this->click_y) * tool->viewport->get_ympp();
		this->emit_changed();
		return true;
	}
	return false; /* I didn't move anything on this layer! */
}




static LayerTool * georef_layer_zoom_create(Window * window, Viewport * viewport)
{
	return new LayerToolGeorefZoom(window, viewport);
}




LayerToolGeorefZoom::LayerToolGeorefZoom(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::GEOREF)
{
	this->id_string = QString("georef.zoom");

	this->action_icon_path   = "vik-icon-Georef Zoom Tool";
	this->action_label       = QObject::tr("Georef Z&oom Tool");
	this->action_tooltip     = QObject::tr("Georef Zoom Tool");
	// this->action_accelerator = ...; /* Empty accelerator. */
#ifdef K
	this->cursor_shape = Qt::BitmapCursor;
	this->cursor_data = &cursor_geozoom_pixmap;
#endif

	Layer::get_interface(LayerType::GEOREF)->layer_tools.insert({{ LAYER_GEOREF_TOOL_ZOOM, this }});
}




LayerToolFuncStatus LayerToolGeorefZoom::click_(Layer * vgl, QMouseEvent * ev)
{
	return (LayerToolFuncStatus) ((LayerGeoref *) vgl)->zoom_press(ev, this); /* kamilFIXME: check this cast of returned value. */
}




bool LayerGeoref::zoom_press(QMouseEvent * ev, LayerTool * tool)
{
	if (this->type != LayerType::GEOREF) {
		/* kamilFIXME: this shouldn't happen, right? */
		return false;
	}

	if (ev->button() == Qt::LeftButton) {
		if (this->mpp_easting < (SG_VIEWPORT_ZOOM_MAX / 1.05) && this->mpp_northing < (SG_VIEWPORT_ZOOM_MAX / 1.05)) {
			this->mpp_easting *= 1.01;
			this->mpp_northing *= 1.01;
		}
	} else {
		if (this->mpp_easting > (SG_VIEWPORT_ZOOM_MIN * 1.05) && this->mpp_northing > (SG_VIEWPORT_ZOOM_MIN * 1.05)) {
			this->mpp_easting /= 1.01;
			this->mpp_northing /= 1.01;
		}
	}
	tool->viewport->set_xmpp(this->mpp_easting);
	tool->viewport->set_ympp(this->mpp_northing);
	this->emit_changed();
	return true;
}




LayerToolFuncStatus LayerToolGeorefMove::click_(Layer * vgl, QMouseEvent * ev)
{
	return (LayerToolFuncStatus) ((LayerGeoref *) vgl)->move_press(ev, this); /* kamilFIXME: check this cast of returned value. */
}




bool LayerGeoref::move_press(QMouseEvent * ev, LayerTool * tool)
{
	if (this->type != LayerType::GEOREF) {
		/* kamilFIXME: this shouldn't happen, right? */
		return false;
	}
	this->click_x = ev->x();
	this->click_y = ev->y();
	return true;
}




static void goto_center_ll(Viewport * viewport, struct LatLon ll_tl, struct LatLon ll_br)
{
	struct LatLon ll_center;
	ll_center.lat = (ll_tl.lat + ll_br.lat) / 2.0;
	ll_center.lon = (ll_tl.lon + ll_br.lon) / 2.0;

	Coord new_center(ll_center, viewport->get_coord_mode());
	viewport->set_center_coord(new_center, true);
}




LayerGeoref * SlavGPS::vik_georef_layer_create(Viewport * viewport,
					       const char *name,
					       QPixmap * pixmap,
					       const Coord * coord_tl,
					       const Coord * coord_br)
{

	LayerGeoref * grl = new LayerGeoref();
	grl->configure_from_viewport(viewport);
	grl->rename(name);
	grl->pixmap = pixmap;

	grl->corner = coord_tl->get_utm();
	grl->ll_br = coord_br->get_latlon();

	if (grl->pixmap) {
		grl->width = grl->pixmap->width();
		grl->height = grl->pixmap->height();

		if (grl->width > 0 && grl->height > 0) {

			struct LatLon ll_tl = coord_tl->get_latlon();
			struct LatLon ll_br = coord_br->get_latlon();

			CoordMode mode = viewport->get_coord_mode();

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
	strcpy(this->debug_string, "GEOREF");
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
	this->corner = viewport->get_center()->get_utm();
}
