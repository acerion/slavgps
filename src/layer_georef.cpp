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
#include <QDir>

#include "window.h"
#include "vikutils.h"
#include "ui_util.h"
#include "preferences.h"
typedef int GdkPixdata; /* TODO: remove sooner or later. */
#include "icons/icons.h"
#include "layer_map.h"
#include "layer_georef.h"
#include "widget_file_entry.h"
#include "widget_slider.h"
#include "dialog.h"
#include "file.h"
#include "application_state.h"
#include "globals.h"
#include "util.h"
#include "viewport_zoom.h"




using namespace SlavGPS;




#define PREFIX " Layer Georef: "




extern Tree * g_tree;




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




ParameterSpecification georef_layer_param_specs[] = {
	{ PARAM_IMAGE, NULL, "image",                SGVariantType::STRING, PARAMETER_GROUP_HIDDEN, QString(""), WidgetType::NONE, NULL, NULL, NULL, NULL },
	{ PARAM_CE,    NULL, "corner_easting",       SGVariantType::DOUBLE, PARAMETER_GROUP_HIDDEN, QString(""), WidgetType::NONE, NULL, NULL, NULL, NULL },
	{ PARAM_CN,    NULL, "corner_northing",      SGVariantType::DOUBLE, PARAMETER_GROUP_HIDDEN, QString(""), WidgetType::NONE, NULL, NULL, NULL, NULL },
	{ PARAM_ME,    NULL, "mpp_easting",          SGVariantType::DOUBLE, PARAMETER_GROUP_HIDDEN, QString(""), WidgetType::NONE, NULL, NULL, NULL, NULL },
	{ PARAM_MN,    NULL, "mpp_northing",         SGVariantType::DOUBLE, PARAMETER_GROUP_HIDDEN, QString(""), WidgetType::NONE, NULL, NULL, NULL, NULL },
	{ PARAM_CZ,    NULL, "corner_zone",          SGVariantType::UINT,   PARAMETER_GROUP_HIDDEN, QString(""), WidgetType::NONE, NULL, NULL, NULL, NULL },
	{ PARAM_CL,    NULL, "corner_letter_as_int", SGVariantType::UINT,   PARAMETER_GROUP_HIDDEN, QString(""), WidgetType::NONE, NULL, NULL, NULL, NULL },
	{ PARAM_AA,    NULL, "alpha",                SGVariantType::UINT,   PARAMETER_GROUP_HIDDEN, QString(""), WidgetType::NONE, NULL, NULL, NULL, NULL },

	{ NUM_PARAMS,  NULL, NULL,                   SGVariantType::EMPTY,  PARAMETER_GROUP_GENERIC,QString(""), WidgetType::NONE, NULL, NULL, NULL, NULL }, /* Guard. */
};




LayerGeorefInterface vik_georef_layer_interface;




LayerGeorefInterface::LayerGeorefInterface()
{
	this->parameters_c = georef_layer_param_specs;

	this->fixed_layer_type_string = "GeoRef Map"; /* Non-translatable. */

	// this->action_accelerator = ...; /* Empty accelerator. */
	// this->action_icon = ...; /* Set elsewhere. */

	this->menu_items_selection = LayerMenuItem::ALL;

	this->ui_labels.new_layer = QObject::tr("New GeoRef Map Layer");
	this->ui_labels.layer_type = QObject::tr("GeoRef Map");
	this->ui_labels.layer_defaults = QObject::tr("Default Settings of GeoRef Map Layer");
}




LayerToolContainer * LayerGeorefInterface::create_tools(Window * window, Viewport * viewport)
{
	/* This method should be called only once. */
	static bool created = false;
	if (created) {
		return NULL;
	}

	auto tools = new LayerToolContainer;

	LayerTool * tool = NULL;

	tool = new LayerToolGeorefMove(window, viewport);
	tools->insert({{ tool->id_string, tool }});

	tool = new LayerToolGeorefZoom(window, viewport);
	tools->insert({{ tool->id_string, tool }});

	created = true;

	return tools;
}




static ParameterSpecification io_prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_IO, "georef_auto_read_world_file", SGVariantType::BOOLEAN, PARAMETER_GROUP_GENERIC, QObject::tr("Auto Read World Files:"), WidgetType::CHECKBUTTON, NULL, NULL, NULL, N_("Automatically attempt to read associated world file of a new image for a GeoRef layer") }
};




void SlavGPS::vik_georef_layer_init(void)
{
	Preferences::register_parameter(&io_prefs[0], SGVariant(true));
}




QString LayerGeoref::get_tooltip()
{
	return this->image;
}




Layer * LayerGeorefInterface::unmarshall(uint8_t * data, size_t data_len, Viewport * viewport)
{
	LayerGeoref * layer = new LayerGeoref();
	layer->configure_from_viewport(viewport);

	layer->unmarshall_params(data, data_len);

	if (!layer->image.isEmpty()) {
		layer->post_read(viewport, true);
	}
	return layer;
}




bool LayerGeoref::set_param_value(uint16_t id, const SGVariant & param_value, bool is_file_operation)
{
	switch (id) {
	case PARAM_IMAGE:
		this->set_image(param_value.val_string);
		break;
	case PARAM_CN:
		this->corner.northing = param_value.val_double;
		break;
	case PARAM_CE:
		this->corner.easting = param_value.val_double;
		break;
	case PARAM_MN:
		this->mpp_northing = param_value.val_double;
		break;
	case PARAM_ME:
		this->mpp_easting = param_value.val_double;
		break;
	case PARAM_CZ:
		if (param_value.val_uint <= 60) {
			this->corner.zone = param_value.val_uint;
		}
		break;
	case PARAM_CL:
		if (param_value.val_uint >= 65 || param_value.val_uint <= 90) {
			this->corner.letter = param_value.val_uint;
		}
		break;
	case PARAM_AA:
		if (param_value.val_uint <= 255) {
			this->alpha = param_value.val_uint;
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
	const QString path = maps_layer_default_dir() + this->get_name() + ".jpg"; /* maps_layer_default_dir() should return string with trailing separator. */
	if (!this->pixmap->save(path, "jpeg")) {
		qDebug() << "WW: Layer Georef: failed to save pixmap to" << path;
	} else {
		this->image = path;
	}
}




SGVariant LayerGeoref::get_param_value(param_id_t id, bool is_file_operation) const
{
	SGVariant rv;
	switch (id) {
	case PARAM_IMAGE: {
		bool set = false;
		if (is_file_operation) {
			if (this->pixmap && this->image.isEmpty()) {
				/* Force creation of image file. */
				((LayerGeoref *) this)->create_image_file();
			}
			if (Preferences::get_file_ref_format() == VIK_FILE_REF_FORMAT_RELATIVE) {
				const QString cwd = QDir::currentPath();
				if (!cwd.isEmpty()) {
					rv = SGVariant(file_GetRelativeFilename(cwd, this->image));
					set = true;
				}
			}
		}
		if (!set) {
			rv = SGVariant(this->image);
		}
		break;
	}
	case PARAM_CN:
		rv = SGVariant(this->corner.northing);
		break;
	case PARAM_CE:
		rv = SGVariant(this->corner.easting);
		break;
	case PARAM_MN:
		rv = SGVariant(this->mpp_northing);
		break;
	case PARAM_ME:
		rv = SGVariant(this->mpp_easting);
		break;
	case PARAM_CZ:
		rv = SGVariant((uint32_t) this->corner.zone); /* FIXME: why do we need to do cast here? */
		break;
	case PARAM_CL:
		rv = SGVariant((uint32_t) this->corner.letter); /* FIXME: why do we need to do cast here? */
		break;
	case PARAM_AA:
		rv = SGVariant((uint32_t) this->alpha); /* FIXME: why do we need to do cast here? */
		break;
	default:
		break;
	}
	return rv;
}




/**
 * Return mpp for the given coords, coord mode and image size.
 */
static void georef_layer_mpp_from_coords(CoordMode mode, const LatLon & ll_tl, const LatLon & ll_br, unsigned int width, unsigned int height, double *xmpp, double *ympp)
{
	const LatLon ll_tr(ll_tl.lat, ll_br.lon);
	const LatLon ll_bl(ll_br.lat, ll_tl.lon);

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

	double diffx = a_coords_latlon_diff(ll_tl, ll_tr);
	*xmpp = (diffx / width) / factor;

	double diffy = a_coords_latlon_diff(ll_tl, ll_bl);
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

		const Coord corner_coord(this->corner, viewport->get_coord_mode());
		const ScreenPos corner_pos = viewport->coord_to_screen_pos(corner_coord);

		/* Mark to scale the pixmap if it doesn't match our dimensions. */
		bool scale = false;
		if (xmpp != this->mpp_easting || ympp != this->mpp_northing) {
			scale = true;
			layer_width = round(this->width * this->mpp_easting / xmpp);
			layer_height = round(this->height * this->mpp_northing / ympp);
		}

		/* If image not in viewport bounds - no need to draw it (or bother with any scaling). */
		if ((corner_pos.x < 0 || corner_pos.x < width_)
		    && (corner_pos.y < 0 || corner_pos.y < height_)
		    && corner_pos.x + layer_width > 0
		    && corner_pos.y + layer_height > 0) {

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
			viewport->draw_pixmap(*pixmap_, 0, 0, corner_pos.x, corner_pos.y, layer_width, layer_height); /* todo: draw only what we need to. */
		}
	}
}




LayerGeoref::~LayerGeoref()
{
	if (this->scaled != NULL) {
#ifdef K
		g_object_unref(this->scaled);
#endif
	}
}




bool LayerGeoref::properties_dialog(Viewport * viewport)
{
	return this->dialog(viewport, this->get_window());
}




/* Also known as LayerGeoref::load_image(). */
void LayerGeoref::post_read(Viewport * viewport, bool from_file)
{
	GError *gx = NULL;
	if (this->image.isEmpty()) {
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

	this->pixmap = new QPixmap();
	if (!pixmap->load(this->image)) {
		delete pixmap;
		pixmap = NULL;
		if (!from_file) {
			Dialog::error(tr("Couldn't open image file %1").arg(this->image), this->get_window());
		}
	} else {
		this->width = this->pixmap->width();
		this->height = this->pixmap->height();

		if (this->pixmap && this->alpha < 255) {
			this->pixmap = ui_pixmap_set_alpha(this->pixmap, this->alpha);
		}
	}

	/* Should find length and width here too. */
}




void LayerGeoref::set_image(const QString & image_)
{
	if (this->scaled) {
#ifdef K
		g_object_unref(this->scaled);
#endif
		this->scaled = NULL;
	}

	if (image_ != "") {
		this->image = image_;
	} else {
		this->image = QString(vu_get_canonical_filename(this, image_.toUtf8().constData(), this->get_window()->get_current_document_full_path().toUtf8().constData()));
	}
}




/* Only positive values allowed here. */
static void double2spinwidget(QDoubleSpinBox * spinbox, double val)
{
	spinbox->setValue(val > 0 ? val : -val);
}




static void set_widget_values(widgets_group * cw, double values[4])
{
	double2spinwidget(cw->x_scale_spin, values[0]);
	double2spinwidget(cw->y_scale_spin, values[1]);
	double2spinwidget(cw->easting_spin, values[2]);
	double2spinwidget(cw->northing_spin, values[3]);
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
static int world_file_read_file(const QString & full_path, double values[4])
{
	qDebug() << "II: Layer Georef: Read World File: file" << full_path;

	FILE *f = fopen(full_path.toUtf8().constData(), "r");
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
				qDebug() << "II: Layer Georef: Read World File: success";
				answer = 0;
			}
		fclose(f);
		return answer;
	}
}




static void georef_layer_dialog_load(widgets_group * cw)
{
	Window * window = g_tree->tree_get_main_window();
	QFileDialog file_selector(window, QObject::tr("Choose World file"));
	file_selector.setFileMode(QFileDialog::ExistingFile);
	/* AcceptMode is QFileDialog::AcceptOpen by default. */;

	if (QDialog::Accepted != file_selector.exec()) {
		return;
	}

	QStringList selection = file_selector.selectedFiles();
	if (!selection.size()) {
		return;
	}

	double values[4];
	int answer = world_file_read_file(selection.at(0), values);
	if (answer == 1) {
		Dialog::error(QObject::tr("The World file you requested could not be opened for reading."), window);
	} else if (answer == 2) {
		Dialog::error(QObject::tr("Unexpected end of file reading World file."), window);
	} else {
		/* NB answer should == 0 for success. */
		set_widget_values(cw, values);
	}
}




void LayerGeoref::export_params_cb(void)
{
	Window * window = g_tree->tree_get_main_window();

	QFileDialog file_selector(window, QObject::tr("Choose World file"));
	file_selector.setFileMode(QFileDialog::AnyFile); /* Specify new or select existing file. */
	file_selector.setAcceptMode(QFileDialog::AcceptSave);


	if (QDialog::Accepted != file_selector.exec()) {
		return;
	}


	QStringList selection = file_selector.selectedFiles();
	if (!selection.size()) {
		return;
	}


	FILE * f = fopen(selection.at(0).toUtf8().constData(), "w");
	if (!f) {
		Dialog::error(QObject::tr("The file you requested could not be opened for writing."), window);
		return;
	}

	fprintf(f, "%f\n%f\n%f\n%f\n%f\n%f\n", this->mpp_easting, this->mpp_northing, 0.0, 0.0, this->corner.easting, this->corner.northing);
	fclose(f);
	f = NULL;
}




/**
 * Auto attempt to read the world file associated with the image used for the georef.
 * Based on simple file name conventions.
 * Only attempted if the preference is on.
 */
static void maybe_read_world_file(SGFileEntry * file_entry, void * user_data)
{
	if (Preferences::get_param_value(PREFERENCES_NAMESPACE_IO ".georef_auto_read_world_file")->val_bool) {
		const QString filename = file_entry->get_filename();
#if 0
		double values[4];
		if (filename && user_data) {

			widgets_group *cw = (widgets_group *) user_data;

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
#endif
	}

}




LatLon LayerGeoref::get_ll_tl()
{
	LatLon ll_result;
	ll_result.lat = this->cw.lat_tl_spin->value();
	ll_result.lon = this->cw.lon_tl_spin->value();
	return ll_result;
}




LatLon LayerGeoref::get_ll_br()
{
	LatLon ll_result;
	ll_result.lat = this->cw.lat_br_spin->value();
	ll_result.lon = this->cw.lon_br_spin->value();
	return ll_result;
}




/* Align displayed UTM values with displayed Lat/Lon values. */
void LayerGeoref::align_utm2ll()
{
	const UTM utm = LatLon::to_utm(this->get_ll_tl());

	this->cw.easting_spin->setValue(utm.easting);
	this->cw.northing_spin->setValue(utm.northing);

	char tmp_letter[2];
	tmp_letter[0] = utm.letter;
	tmp_letter[1] = '\0';
	this->cw.utm_letter_entry->setText(QString(tmp_letter));

	this->cw.utm_zone_spin->setValue(utm.zone);

}




/* Align displayed Lat/Lon values with displayed UTM values. */
void LayerGeoref::align_ll2utm()
{
	UTM corner;

	const QString letter = this->cw.utm_letter_entry->text();
	if (1 == letter.size()) {
		corner.letter = letter.at(0).toUpper().toLatin1();
		qDebug() << "II:" PREFIX << __FUNCTION__ << __LINE__ << "UTM letter conversion" << letter << "->" << corner.letter;
	}

	corner.zone = this->cw.utm_zone_spin->value();
	corner.easting = this->cw.easting_spin->value();
	corner.northing = this->cw.northing_spin->value();

	const LatLon ll = UTM::to_latlon(corner);
	this->cw.lat_tl_spin->setValue(ll.lat);
	this->cw.lon_tl_spin->setValue(ll.lon);
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

	LatLon ll_tl = this->get_ll_tl();
	if (ll_tl.lat < this->ll_br.lat || ll_tl.lon > this->ll_br.lon) {
		Dialog::warning(tr("Lower right corner values may not be consistent with upper right values"), this->get_window());
	}
}




void LayerGeoref::calculate_mpp_from_coords_cb(void)
{
	const QString filename = this->cw.map_image_file_entry->get_filename();
	if (!filename.length()) {
		return;
	}

	QPixmap * img_pixmap = new QPixmap();
	if (!img_pixmap->load(filename)) {
		delete img_pixmap;
		img_pixmap = NULL;

		Dialog::error(tr("Couldn't open image file %1").arg(filename), this->get_window());
		return;
	}

	const int img_width = img_pixmap->width();
	const int img_height = img_pixmap->height();

	if (img_width == 0 || img_height == 0) {
		Dialog::error(tr("Invalid image size: %1").arg(filename), this->get_window());
	} else {
		this->align_coords();

		double xmpp, ympp;
		georef_layer_mpp_from_coords(CoordMode::LATLON, this->get_ll_tl(), this->get_ll_br(), img_width, img_height, &xmpp, &ympp);

		this->cw.x_scale_spin->setValue(xmpp);
		this->cw.y_scale_spin->setValue(ympp);

		this->check_br_is_good_or_msg_user();
	}
#ifdef K
	g_object_unref(G_OBJECT(img_pixmap));
#endif
}




#define VIK_SETTINGS_GEOREF_TAB "georef_coordinate_tab"




/* Returns true if OK was pressed. */
bool LayerGeoref::dialog(Viewport * viewport, Window * window_)
{

	BasicDialog dialog(window_);
	dialog.setWindowTitle(QObject::tr("Layer Properties"));


	dialog.button_box->button(QDialogButtonBox::Cancel)->setDefault(true);
	QPushButton * cancel_button = dialog.button_box->button(QDialogButtonBox::Cancel);

	int row = 0;

	cw.map_image_file_entry = new SGFileEntry(QFileDialog::Option(0), QFileDialog::AnyFile, SGFileTypeFilter::IMAGE, QObject::tr("Select image file"), window_);
	// vik_file_entry_new (GTK_FILE_CHOOSER_ACTION_OPEN, SGFileTypeFilter::IMAGE, maybe_read_world_file, &cw);
	dialog.grid->addWidget(new QLabel(QObject::tr("Map Image:")), row, 0);
	dialog.grid->addWidget(cw.map_image_file_entry, row, 1);
	row++;

	cw.world_file_entry = new SGFileEntry(QFileDialog::Option(0), QFileDialog::AnyFile, SGFileTypeFilter::ANY, QObject::tr("Select world file"), window_);
	dialog.grid->addWidget(new QLabel(QObject::tr("World File Parameters:")), row, 0);
	dialog.grid->addWidget(cw.world_file_entry, row, 1);
	row++;

	cw.x_scale_spin = new QDoubleSpinBox();
	cw.x_scale_spin->setMinimum(SG_VIEWPORT_ZOOM_MIN);
	cw.x_scale_spin->setMaximum(SG_VIEWPORT_ZOOM_MAX);
	cw.x_scale_spin->setSingleStep(1);
	cw.x_scale_spin->setValue(4);
	cw.x_scale_spin->setToolTip(QObject::tr("The scale of the map in the X direction (meters per pixel)"));
	dialog.grid->addWidget(new QLabel(QObject::tr("X (easting) scale (mpp): ")), row, 0);
	dialog.grid->addWidget(cw.x_scale_spin, row, 1);
	row++;

	cw.y_scale_spin = new QDoubleSpinBox();
	cw.y_scale_spin->setMinimum(SG_VIEWPORT_ZOOM_MIN);
	cw.y_scale_spin->setMaximum(SG_VIEWPORT_ZOOM_MAX);
	cw.y_scale_spin->setSingleStep(1);
	cw.y_scale_spin->setValue(4);
	cw.y_scale_spin->setToolTip(QObject::tr("The scale of the map in the Y direction (meters per pixel)"));
	dialog.grid->addWidget(new QLabel(QObject::tr("Y (northing) scale (mpp): ")), row, 0);
	dialog.grid->addWidget(cw.y_scale_spin, row, 1);
	row++;


	/* This should go into UTM tab of notebook. */
	{

		cw.easting_spin = new QDoubleSpinBox();
		cw.easting_spin->setMinimum(0.0);
		cw.easting_spin->setMaximum(1500000.0);
		cw.easting_spin->setSingleStep(1);
		cw.easting_spin->setValue(4);
		cw.easting_spin->setValue(this->corner.easting);
		cw.easting_spin->setToolTip(QObject::tr("The UTM \"easting\" value of the upper-left corner pixel of the map"));
		dialog.grid->addWidget(new QLabel(QObject::tr("Corner pixel easting:")), row, 0);
		dialog.grid->addWidget(cw.easting_spin, row, 1);
		row++;

		cw.northing_spin = new QDoubleSpinBox();
		cw.northing_spin->setMinimum(0.0);
		cw.northing_spin->setMaximum(9000000.0);
		cw.northing_spin->setSingleStep(1);
		cw.northing_spin->setValue(4);
		cw.northing_spin->setValue(this->corner.northing);
		cw.northing_spin->setToolTip(QObject::tr("The UTM \"northing\" value of the upper-left corner pixel of the map"));
		dialog.grid->addWidget(new QLabel(QObject::tr("Corner pixel northing:")), row, 0);
		dialog.grid->addWidget(cw.northing_spin, row, 1);
		row++;


		cw.utm_zone_spin = new QSpinBox();
		cw.utm_zone_spin->setMinimum(1);
		cw.utm_zone_spin->setMaximum(60);
		cw.utm_zone_spin->setSingleStep(1);
		cw.utm_zone_spin->setValue(this->corner.zone);
		dialog.grid->addWidget(new QLabel(QObject::tr("Zone:")), row, 0);
		dialog.grid->addWidget(cw.utm_zone_spin, row, 1);
		row++;

		cw.utm_letter_entry = new QLineEdit();
		cw.utm_letter_entry->setMaxLength(1);
		//gtk_entry_set_width_chars (GTK_ENTRY(cw.utm_letter_entry), 2);
		char tmp_letter[2];
		tmp_letter[0] = this->corner.letter;
		tmp_letter[1] = '\0';
		cw.utm_letter_entry->setText(tmp_letter);
		dialog.grid->addWidget(new QLabel(QObject::tr("Letter:")), row, 0);
		dialog.grid->addWidget(cw.utm_letter_entry, row, 1);
		row++;
	}

	cw.x_scale_spin->setValue(this->mpp_easting);
	cw.y_scale_spin->setValue(this->mpp_northing);
	if (!this->image.isEmpty()) {
		cw.map_image_file_entry->set_filename(this->image);
	}

	/* This should go into Lat/Lon tab of notebook. */
	{

		cw.lat_tl_spin = new QDoubleSpinBox();
		cw.lat_tl_spin->setMinimum(-90.0);
		cw.lat_tl_spin->setMaximum(90.0);
		cw.lat_tl_spin->setSingleStep(0.05);
		cw.lat_tl_spin->setValue(0.0);
		dialog.grid->addWidget(new QLabel(QObject::tr("Upper left latitude:")), row, 0);
		dialog.grid->addWidget(cw.lat_tl_spin, row, 1);
		row++;

		cw.lon_tl_spin = new QDoubleSpinBox();
		cw.lon_tl_spin->setMinimum(-180.0);
		cw.lon_tl_spin->setMaximum(180.0);
		cw.lon_tl_spin->setSingleStep(0.05);
		cw.lon_tl_spin->setValue(0.0);
		dialog.grid->addWidget(new QLabel(QObject::tr("Upper left longitude:")), row, 0);
		dialog.grid->addWidget(cw.lon_tl_spin, row, 1);
		row++;

		cw.lat_br_spin = new QDoubleSpinBox();
		cw.lat_br_spin->setMinimum(-90.0);
		cw.lat_br_spin->setMaximum(90.0);
		cw.lat_br_spin->setSingleStep(0.05);
		cw.lat_br_spin->setValue(0.0);
		dialog.grid->addWidget(new QLabel(QObject::tr("Lower right latitude:")), row, 0);
		dialog.grid->addWidget(cw.lat_br_spin, row, 1);
		row++;

		cw.lon_br_spin = new QDoubleSpinBox();
		cw.lon_br_spin->setMinimum(-180.0);
		cw.lon_br_spin->setMaximum(180.0);
		cw.lon_br_spin->setSingleStep(0.05);
		cw.lon_br_spin->setValue(0.0);
		dialog.grid->addWidget(new QLabel(QObject::tr("Lower right longitude:")), row, 0);
		dialog.grid->addWidget(cw.lon_br_spin, row, 1);
		row++;

		QPushButton * calc_mpp_button = new QPushButton(QObject::tr("Calculate MPP values from coordinates"));
		calc_mpp_button->setToolTip(QObject::tr("Enter all corner coordinates before calculating the MPP values from the image size"));
		dialog.grid->addWidget(calc_mpp_button, row, 0, 1, 2);
		row++;

		const Coord coord(this->corner, CoordMode::LATLON);
		cw.lat_tl_spin->setValue(coord.ll.lat);
		cw.lon_tl_spin->setValue(coord.ll.lon);
		cw.lat_br_spin->setValue(this->ll_br.lat);
		cw.lon_br_spin->setValue(this->ll_br.lon);
	}


	ParameterScale alpha_scale = { 0, 255, SGVariant((int32_t) this->alpha), 1, 0 };
	SGSlider * alpha_slider = new SGSlider(alpha_scale, Qt::Horizontal);
	dialog.grid->addWidget(new QLabel(QObject::tr("Alpha:")), row, 0);
	dialog.grid->addWidget(alpha_slider, row, 1);
	row++;


#ifdef K
	gtk_notebook_append_page(GTK_NOTEBOOK(cw.tabs), GTK_WIDGET(table_utm), new QLabel(QObject::tr("UTM")));
	gtk_notebook_append_page(GTK_NOTEBOOK(cw.tabs), GTK_WIDGET(table_ll), new QLabel(QObject::tr("Latitude/Longitude")));
	dgbox->addWidget(cw.tabs);

	QObject::connect(this->cw.tabs, SIGNAL("switch-page"), this, SLOT (switch_tab));
	QObject::connect(calc_mpp_button, SIGNAL (triggered(bool)), this, SLOT (calculate_mpp_from_coords_cb));
	QObject::connect(world_file_entry_button, SIGNAL (triggered(bool)), &cw, SLOT (georef_layer_dialog_load));
#endif
	if (cancel_button) {
		cancel_button->setFocus();
	}

	/* Remember setting the notebook page must be done after the widget is visible. */
	int page_num = 0;
	if (ApplicationState::get_integer (VIK_SETTINGS_GEOREF_TAB, &page_num)) {
		if (page_num < 0 || page_num > 1) {
			page_num = 0;
		}
	}
#ifdef K
	gtk_notebook_set_current_page (GTK_NOTEBOOK(cw.tabs), page_num);
#endif
	if (dialog.exec() == QDialog::Accepted) {

		this->align_coords();

		this->corner.easting = cw.easting_spin->value();
		this->corner.northing = cw.northing_spin->value();
		this->corner.zone = cw.utm_zone_spin->value();
#ifdef K
		const char *letter = cw.utm_letter_entry->text();
		if (*letter) {
			this->corner.letter = toupper(*letter);
		}
#endif
		this->mpp_easting = cw.x_scale_spin->value();
		this->mpp_northing = cw.y_scale_spin->value();
		this->ll_br = this->get_ll_br();
		this->check_br_is_good_or_msg_user();
		/* TODO check if image has changed otherwise no need to regenerate pixmap. */
		if (!this->pixmap) {
			if (this->image != cw.map_image_file_entry->get_filename()) {
				this->set_image(cw.map_image_file_entry->get_filename());
				this->post_read(viewport, false);
			}
		}

		this->alpha = alpha_slider->get_value();
		if (this->pixmap && this->alpha < 255) {
#ifdef K
			this->pixmap = ui_pixmap_set_alpha(this->pixmap, this->alpha);
#endif
		}

		if (this->scaled && this->alpha < 255) {
#ifdef K
			this->scaled = ui_pixmap_set_alpha(this->scaled, this->alpha);
#endif
		}

#ifdef K
		ApplicationState::set_integer(VIK_SETTINGS_GEOREF_TAB, gtk_notebook_get_current_page(GTK_NOTEBOOK(cw.tabs)));
#endif
		return true;
	}

	return false;
}




void LayerGeoref::zoom_to_fit_cb(void)
{
	Viewport * viewport = g_tree->tree_get_main_viewport();
	viewport->set_xmpp(this->mpp_easting);
	viewport->set_ympp(this->mpp_northing);

	g_tree->emit_update_window();
}




void LayerGeoref::goto_center_cb(void)
{
	Viewport * viewport = g_tree->tree_get_main_viewport();
	UTM utm = viewport->get_center()->get_utm();

	utm.easting = this->corner.easting + (this->width * this->mpp_easting / 2); /* Only an approximation. */
	utm.northing = this->corner.northing - (this->height * this->mpp_northing / 2);

	viewport->set_center_from_coord(Coord(utm, viewport->get_coord_mode()), true);

	g_tree->emit_update_window();
}




void LayerGeoref::add_menu_items(QMenu & menu)
{
	QAction * action = NULL;

	action = new QAction(QObject::tr("&Zoom to Fit Map"), this);
	action->setIcon(QIcon::fromTheme("GTK_STOCK_ZOOM_FIT"));
	QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (zoom_to_fit_cb(void)));
	menu.addAction(action);

	action = new QAction(QObject::tr("&Goto Map Center"), this);
	action->setIcon(QIcon::fromTheme("GTK_STOCK_JUMP_TO"));
	QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (goto_center_cb(void)));
	menu.addAction(action);

	action = new QAction(QObject::tr("&Export to World File"), this);
	action->setIcon(QIcon::fromTheme("GTK_STOCK_HARDDISK"));
	QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (export_params_cb(void)));
	menu.addAction(action);
}




LayerToolGeorefMove::LayerToolGeorefMove(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::GEOREF)
{
	this->id_string = "sg.tool.layer_georef.move";

	this->action_icon_path   = "vik-icon-Georef Move Map";
	this->action_label       = QObject::tr("_Georef Move Map");
	this->action_tooltip     = QObject::tr("Georef Move Map");
	// this->action_accelerator = ...; /* Empty accelerator. */

#ifdef K
	this->cursor_shape = Qt::BitmapCursor;
	this->cursor_data = &cursor_geomove_pixmap;
#endif
}




ToolStatus LayerToolGeorefMove::handle_mouse_release(Layer * layer, QMouseEvent * ev)
{
	return (ToolStatus) ((LayerGeoref *) layer)->move_release(ev, this); /* kamilFIXME: resolve this cast of returned value. */
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
		this->emit_layer_changed();
		return true;
	}
	return false; /* I didn't move anything on this layer! */
}




LayerToolGeorefZoom::LayerToolGeorefZoom(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::GEOREF)
{
	this->id_string = "sg.tool.layer_georef.zoom";

	this->action_icon_path   = "vik-icon-Georef Zoom Tool";
	this->action_label       = QObject::tr("Georef Z&oom Tool");
	this->action_tooltip     = QObject::tr("Georef Zoom Tool");
	// this->action_accelerator = ...; /* Empty accelerator. */
#ifdef K
	this->cursor_shape = Qt::BitmapCursor;
	this->cursor_data = &cursor_geozoom_pixmap;
#endif
}




ToolStatus LayerToolGeorefZoom::handle_mouse_click(Layer * layer, QMouseEvent * ev)
{
	return (ToolStatus) ((LayerGeoref *) layer)->zoom_press(ev, this); /* kamilFIXME: check this cast of returned value. */
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
	this->emit_layer_changed();
	return true;
}




ToolStatus LayerToolGeorefMove::handle_mouse_click(Layer * layer, QMouseEvent * ev)
{
	return (ToolStatus) ((LayerGeoref *) layer)->move_press(ev, this); /* kamilFIXME: check this cast of returned value. */
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




static void goto_center_ll(Viewport * viewport, const LatLon & ll_tl, const LatLon & ll_br)
{
	const LatLon ll_center = LatLon::get_average(ll_tl, ll_br);
	const Coord new_center(ll_center, viewport->get_coord_mode());
	viewport->set_center_from_coord(new_center, true);
}




LayerGeoref * SlavGPS::georef_layer_create(Viewport * viewport, const QString & name, QPixmap * pixmap, const Coord & coord_tl, const Coord & coord_br)
{
	LayerGeoref * grl = new LayerGeoref();
	grl->configure_from_viewport(viewport);
	grl->set_name(name);
	grl->pixmap = pixmap;

	grl->corner = coord_tl.get_utm();
	grl->ll_br = coord_br.get_latlon();

	if (grl->pixmap) {
		grl->width = grl->pixmap->width();
		grl->height = grl->pixmap->height();

		if (grl->width > 0 && grl->height > 0) {

			const LatLon ll_tl = coord_tl.get_latlon();
			const LatLon ll_br = coord_br.get_latlon();
			const CoordMode mode = viewport->get_coord_mode();

			double xmpp, ympp;
			georef_layer_mpp_from_coords(mode, ll_tl, ll_br, grl->width, grl->height, &xmpp, &ympp);
			grl->mpp_easting = xmpp;
			grl->mpp_northing = ympp;

			goto_center_ll(viewport, ll_tl, ll_br);
			/* Set best zoom level. */
			vu_zoom_to_show_latlons(viewport->get_coord_mode(), viewport, LatLonMinMax(ll_br, ll_tl));

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
	this->set_name(Layer::get_type_ui_label(this->type));

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
