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
#include <cassert>

#include <QDebug>
#include <QDir>

#include "window.h"
#include "vikutils.h"
#include "ui_util.h"
#include "util.h"
#include "preferences.h"
#include "application_state.h"
#include "map_cache.h"
#include "layer_georef.h"
#include "widget_file_entry.h"
//#include "file.h"
#include "globals.h"
#include "viewport_zoom.h"




using namespace SlavGPS;




#define PREFIX ": Layer Georef: " << __FUNCTION__ << __LINE__ << ">"




extern Tree * g_tree;




enum {
	PARAM_IMAGE_FULL_PATH = 0,
	PARAM_CORNER_UTM_EASTING,
	PARAM_CORNER_UTM_NORTHING,
	PARAM_MPP_EASTING,
	PARAM_MPP_NORTHING,
	PARAM_CORNER_UTM_ZONE,
	PARAM_CORNER_UTM_BAND_LETTER,
	PARAM_ALPHA,
	NUM_PARAMS
};




ParameterSpecification georef_layer_param_specs[] = {
	{ PARAM_IMAGE_FULL_PATH,         NULL, "image",                SGVariantType::String, PARAMETER_GROUP_HIDDEN, QString(""), WidgetType::NONE, NULL, NULL, NULL, NULL },
	{ PARAM_CORNER_UTM_EASTING,      NULL, "corner_easting",       SGVariantType::Double, PARAMETER_GROUP_HIDDEN, QString(""), WidgetType::NONE, NULL, NULL, NULL, NULL },
	{ PARAM_CORNER_UTM_NORTHING,     NULL, "corner_northing",      SGVariantType::Double, PARAMETER_GROUP_HIDDEN, QString(""), WidgetType::NONE, NULL, NULL, NULL, NULL },
	{ PARAM_MPP_EASTING,             NULL, "mpp_easting",          SGVariantType::Double, PARAMETER_GROUP_HIDDEN, QString(""), WidgetType::NONE, NULL, NULL, NULL, NULL },
	{ PARAM_MPP_NORTHING,            NULL, "mpp_northing",         SGVariantType::Double, PARAMETER_GROUP_HIDDEN, QString(""), WidgetType::NONE, NULL, NULL, NULL, NULL },
	{ PARAM_CORNER_UTM_ZONE,         NULL, "corner_zone",          SGVariantType::Uint,   PARAMETER_GROUP_HIDDEN, QString(""), WidgetType::NONE, NULL, NULL, NULL, NULL },
	{ PARAM_CORNER_UTM_BAND_LETTER,  NULL, "corner_letter_as_int", SGVariantType::Uint,   PARAMETER_GROUP_HIDDEN, QString(""), WidgetType::NONE, NULL, NULL, NULL, NULL },
	{ PARAM_ALPHA,                   NULL, "alpha",                SGVariantType::Uint,   PARAMETER_GROUP_HIDDEN, QString(""), WidgetType::NONE, NULL, NULL, NULL, NULL },

	{ NUM_PARAMS,                    NULL, NULL,                   SGVariantType::Empty,  PARAMETER_GROUP_GENERIC,QString(""), WidgetType::NONE, NULL, NULL, NULL, NULL }, /* Guard. */
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
	{ 0, PREFERENCES_NAMESPACE_IO, "georef_auto_read_world_file", SGVariantType::Boolean, PARAMETER_GROUP_GENERIC, QObject::tr("Auto Read World Files:"), WidgetType::CHECKBUTTON, NULL, NULL, NULL, N_("Automatically attempt to read associated world file of a new image for a GeoRef layer") }
};




void SlavGPS::layer_georef_init(void)
{
	Preferences::register_parameter(io_prefs[0], SGVariant(true, io_prefs[0].type_id));
}




QString LayerGeoref::get_tooltip()
{
	return this->image_full_path;
}




Layer * LayerGeorefInterface::unmarshall(uint8_t * data, size_t data_len, Viewport * viewport)
{
	LayerGeoref * layer = new LayerGeoref();
	layer->configure_from_viewport(viewport);

	layer->unmarshall_params(data, data_len);

	if (!layer->image_full_path.isEmpty()) {
		layer->post_read(viewport, true);
	}
	return layer;
}




bool LayerGeoref::set_param_value(uint16_t id, const SGVariant & param_value, bool is_file_operation)
{
	switch (id) {
	case PARAM_IMAGE_FULL_PATH:
		this->set_image_full_path(param_value.val_string);
		break;
	case PARAM_CORNER_UTM_EASTING:
		this->utm_tl.easting = param_value.val_double;
		break;
	case PARAM_CORNER_UTM_NORTHING:
		this->utm_tl.northing = param_value.val_double;
		break;
	case PARAM_MPP_EASTING:
		this->mpp_easting = param_value.val_double;
		break;
	case PARAM_MPP_NORTHING:
		this->mpp_northing = param_value.val_double;
		break;
	case PARAM_CORNER_UTM_ZONE:
		if (param_value.val_uint <= UTM_ZONES) {
			this->utm_tl.zone = param_value.val_uint; /* FIXME: data type mismatch: int vs uint. */
		} else {
			qDebug() << "EE" PREFIX << "invalid utm zone" << param_value.val_uint;
		}
		break;
	case PARAM_CORNER_UTM_BAND_LETTER:
		if (param_value.val_uint >= 65 || param_value.val_uint <= 90) {
			this->utm_tl.letter = param_value.val_uint;
		} else {
			qDebug() << "EE" PREFIX << "invalid utm band letter" << param_value.val_uint;
		}
		break;
	case PARAM_ALPHA:
		if (param_value.val_uint <= 255) {
			this->alpha = param_value.val_uint;
		} else {
			qDebug() << "EE" PREFIX << "invalid alpha value" << param_value.val_uint;
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
	const QString path = MapCache::get_default_maps_dir() + this->get_name() + ".jpg"; /* LayerMap::get_default_maps_dir() should return string with trailing separator. */
	if (!this->image.save(path, "jpeg")) {
		qDebug() << "WW: Layer Georef: failed to save pixmap to" << path;
	} else {
		this->image_full_path = path;
	}
}




SGVariant LayerGeoref::get_param_value(param_id_t id, bool is_file_operation) const
{
	SGVariant rv;
	switch (id) {
	case PARAM_IMAGE_FULL_PATH: {
		bool is_set = false;
		if (is_file_operation) {
			if (!this->image.isNull() && this->image_full_path.isEmpty()) {
				/* Force creation of image file. */
				((LayerGeoref *) this)->create_image_file();
			}
			if (Preferences::get_file_path_format() == FilePathFormat::Relative) {
				const QString cwd = QDir::currentPath();
				if (!cwd.isEmpty()) {
					rv = SGVariant(file_GetRelativeFilename(cwd, this->image_full_path));
					is_set = true;
				}
			}
		}
		if (!is_set) {
			rv = SGVariant(this->image_full_path);
		}
		break;
	}
	case PARAM_CORNER_UTM_EASTING:
		rv = SGVariant(this->utm_tl.easting);
		break;
	case PARAM_CORNER_UTM_NORTHING:
		rv = SGVariant(this->utm_tl.northing);
		break;
	case PARAM_MPP_EASTING:
		rv = SGVariant(this->mpp_easting);
		break;
	case PARAM_MPP_NORTHING:
		rv = SGVariant(this->mpp_northing);
		break;
	case PARAM_CORNER_UTM_ZONE:
		rv = SGVariant((uint32_t) this->utm_tl.zone); /* FIXME: why do we need to do cast here? FIXME: data type mismatch: int vs uint */
		break;
	case PARAM_CORNER_UTM_BAND_LETTER:
		rv = SGVariant((uint32_t) this->utm_tl.letter); /* FIXME: why do we need to do cast here? */
		break;
	case PARAM_ALPHA:
		rv = SGVariant((uint32_t) this->alpha); /* FIXME: why do we need to do cast here? */
		break;
	default:
		break;
	}
	return rv;
}




/**
   Return mpp for the given coords, coord mode and image size.
*/
static void georef_layer_mpp_from_coords(CoordMode mode, const LatLon & ll_tl, const LatLon & ll_br, int width, int height, double * xmpp, double * ympp)
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
	*ympp = (diffy / height) / factor;
}




void LayerGeoref::draw(Viewport * viewport)
{
	if (!this->image) {
		return;
	}

	const Coord corner_coord(this->utm_tl, viewport->get_coord_mode());
	const ScreenPos corner_pos = viewport->coord_to_screen_pos(corner_coord);

	QRect sub_viewport_rect;
	sub_viewport_rect.setTopLeft(QPoint(corner_pos.x, corner_pos.y));
	sub_viewport_rect.setWidth(this->image_width);
	sub_viewport_rect.setHeight(this->image_height);

	bool scale_mismatch = false; /* Flag to scale the pixmap if it doesn't match our dimensions. */
	const double xmpp = viewport->get_xmpp();
	const double ympp = viewport->get_ympp();
	if (xmpp != this->mpp_easting || ympp != this->mpp_northing) {
		scale_mismatch = true;
		sub_viewport_rect.setWidth(round(this->image_width * this->mpp_easting / xmpp));
		sub_viewport_rect.setHeight(round(this->image_height * this->mpp_northing / ympp));
	}


	/* If image not in viewport bounds - no need to draw it (or bother with any scaling).
	   TODO: rewrite this section in terms of two rectangles intersecting? */
	const QRect full_viewport_rect(0, 0, viewport->get_width(), viewport->get_height());
	if (!(corner_pos.x < 0 || corner_pos.x < full_viewport_rect.width())) {
		/* Upper-left corner of image is located beyond right border of viewport. */
		return;
	}
	if (!(corner_pos.y < 0 || corner_pos.y < full_viewport_rect.height())) {
		/* Upper-left corner of image is located below bottom border of viewport. */
		return;
	}
	if (!(corner_pos.x + sub_viewport_rect.width() > 0)) {
		/* Upper-right corner of image is located beyond left border of viewport. */
		return;
	}
	if (!(corner_pos.y + sub_viewport_rect.height() > 0)) {
		/* Upper-right corner of image is located above upper border of viewport. */
		return;
	}


	QPixmap image_to_draw = this->image;

	if (scale_mismatch) {
		/* Rescale image only if really necessary, i.e. if we don't have a valid copy of scaled image. */

		const int intended_width = sub_viewport_rect.width();
		const int intended_heigth = sub_viewport_rect.height();

		if (intended_width == this->scaled_image_width && intended_heigth == this->scaled_image_height && !this->scaled_image.isNull()) {
			/* Reuse existing scaled image. */
			image_to_draw = this->scaled_image;
		} else {
			image_to_draw = this->image.scaled(intended_width, intended_heigth, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

			this->scaled_image = image_to_draw;
			this->scaled_image_width = intended_width;
			this->scaled_image_height = intended_heigth;
		}
	}

	QRect image_rect;
	image_rect.setTopLeft(QPoint(0, 0));
	image_rect.setWidth(image_to_draw.width());
	image_rect.setHeight(image_to_draw.height());

	qDebug() << "++++++++ EXPECT 0 0:" << (image_to_draw.width() - sub_viewport_rect.width()) << (image_to_draw.height() - sub_viewport_rect.height());

	viewport->draw_pixmap(image_to_draw, sub_viewport_rect, image_rect);
}




LayerGeoref::~LayerGeoref()
{
}




bool LayerGeoref::properties_dialog(Viewport * viewport)
{
	return this->dialog(viewport, this->get_window());
}




/* Also known as LayerGeoref::load_image(). */
void LayerGeoref::post_read(Viewport * viewport, bool from_file)
{
	if (this->image_full_path.isEmpty()) {
		return;
	}

	if (!this->image.isNull()) {
		this->image = QPixmap();
		assert (this->image.isNull());
	}

	if (!this->scaled_image.isNull()) {
		this->scaled_image = QPixmap();
		assert (this->scaled_image.isNull());
	}

	if (!this->image.load(this->image_full_path)) {
		this->image = QPixmap();
		assert (this->image.isNull());

		if (!from_file) {
			Dialog::error(tr("Couldn't open image file %1").arg(this->image_full_path), this->get_window());
		}
	} else {
		this->image_width = this->image.width();
		this->image_height = this->image.height();

		if (!this->image.isNull() && this->alpha < 255) {
			ui_pixmap_set_alpha(this->image, this->alpha);
		}
	}

	/* Should find length and width here too. */
}




void LayerGeoref::set_image_full_path(const QString & image_path)
{
	if (!this->scaled_image.isNull()) {
		this->scaled_image = QPixmap();
		assert (this->scaled_image.isNull());
	}

	if (image_path != "") {
		this->image_full_path = image_path;
	} else {
		this->image_full_path = vu_get_canonical_filename(this, image_path, this->get_window()->get_current_document_full_path());
	}
}




/* Only positive values allowed here. */
static void double2spinwidget(QDoubleSpinBox * spinbox, double val)
{
	spinbox->setValue(val > 0 ? val : -val);
}




void GeorefConfigDialog::set_widget_values(double values[4])
{
	double2spinwidget(this->x_scale_spin, values[0]);
	double2spinwidget(this->y_scale_spin, values[1]);
	double2spinwidget(this->utm_entry->easting_spin, values[2]);
	double2spinwidget(this->utm_entry->northing_spin, values[3]);
}




static bool world_file_read_line(FILE * file, double * value)
{
	bool success = true;

	char buffer[1024];
	if (!fgets(buffer, 1024, file)) {
		success = false;
	}
	if (success && value) {
		*value = strtod(buffer, NULL);
	}

	return success;
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

	FILE * file = fopen(full_path.toUtf8().constData(), "r");
	if (!file) {
		return 1;
	} else {
		int answer = 2; /* Not enough info read yet. */
		/* **We do not handle 'skew' values ATM - normally they are a value of 0 anyway to align with the UTM grid. */
		if (world_file_read_line(file, &values[0]) /* x scale. */
		    && world_file_read_line(file, NULL) /* Ignore value in y-skew line**. */
		    && world_file_read_line(file, NULL) /* Ignore value in x-skew line**. */
		    && world_file_read_line(file, &values[1]) /* y scale. */
		    && world_file_read_line(file, &values[2]) /* x-coordinate of the upper left pixel. */
		    && world_file_read_line(file, &values[3]) /* y-coordinate of the upper left pixel. */
		    )
			{
				/* Success. */
				qDebug() << "II: Layer Georef: Read World File: success";
				answer = 0;
			}
		fclose(file);
		return answer;
	}
}




void GeorefConfigDialog::load_cb(void)
{
	Window * window = g_tree->tree_get_main_window();
	QFileDialog file_selector(window, tr("Choose World file"));
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
		Dialog::error(tr("The World file you requested could not be opened for reading."), window);
	} else if (answer == 2) {
		Dialog::error(tr("Unexpected end of file reading World file."), window);
	} else {
		/* NB answer should == 0 for success. */
		this->set_widget_values(values);
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

	fprintf(f, "%f\n%f\n%f\n%f\n%f\n%f\n", this->mpp_easting, this->mpp_northing, 0.0, 0.0, this->utm_tl.easting, this->utm_tl.northing);
	fclose(f);
	f = NULL;
}




/**
   Auto attempt to read the world file associated with the image used for the georef.
   Based on simple file name conventions.
   Only attempted if the preference is on.
*/
static void maybe_read_world_file(SGFileEntry * file_entry, void * user_data)
{
	if (!user_data) {
		return;
	}
	GeorefConfigDialog * dialog = (GeorefConfigDialog *) user_data;

	if (!Preferences::get_param_value(PREFERENCES_NAMESPACE_IO ".georef_auto_read_world_file").val_bool) {
		return;
	}

	const QString filename = file_entry->get_filename();
	if (filename.isEmpty()) {
		return;
	}

#ifdef K_TODO
	bool upper = g_ascii_isupper(filename[strlen(filename) - 1]);
	const QString filew = filename + (upper ? "W" : "w");

	double values[4];
	if (world_file_read_file(filew, values) == 0) {
		dialog->set_widget_values(values);
	} else {
		if (strlen(filename) > 3) {
			char* file0 = g_strndup(filename, strlen(filename)-2);
			char* file1 = g_strdup_printf("%s%c%c", file0, filename[strlen(filename)-1], (upper ? 'W' : 'w') );
			if (world_file_read_file(file1, values) == 0) {
				dialog->set_widget_values(values);
			}
			free(file1);
			free(file0);
		}
	}
#endif
}




LatLon GeorefConfigDialog::get_ll_tl(void) const
{
	return this->lat_lon_tl_entry->get_value();
}




LatLon GeorefConfigDialog::get_ll_br(void) const
{
	return this->lat_lon_br_entry->get_value();
}




/* Align displayed UTM values with displayed Lat/Lon values. */
void GeorefConfigDialog::sync_from_utm_to_lat_lon(void)
{
	const UTM utm = LatLon::to_utm(this->get_ll_tl());
	this->utm_entry->set_value(utm);
}




/* Synchronize displayed Lat/Lon values with displayed UTM values. */
void GeorefConfigDialog::sync_from_lat_lon_to_utm(void)
{
	const UTM utm_corner = this->utm_entry->get_value();
	const LatLon lat_lon = UTM::to_latlon(utm_corner);

	this->lat_lon_tl_entry->set_value(lat_lon);
}




/**
   Synchronize values of coordinates between UTM entry and LatLon
   entries as the user may have changed the values in one of them.

   Use this before acting on the user input.
   This is easier then trying to use the 'value-changed' signal for each individual coordinate
   especially since it tends to end up in an infinite loop continually updating each other.
*/
void GeorefConfigDialog::sync_coords_in_entries(void)
{
	const int current_coord_mode = this->coord_mode_combo->currentData().toInt();

	switch ((CoordMode) current_coord_mode) {
	case CoordMode::UTM:
		qDebug() << "II" PREFIX << "current coordinate mode is UTM";
		this->sync_from_lat_lon_to_utm();
		break;

	case CoordMode::LATLON:
		qDebug() << "II" PREFIX << "current coordinate mode is LatLon";
		break;

	default:
		qDebug() << "EE" PREFIX << "unexpected coordinate mode" << current_coord_mode;
		this->sync_from_utm_to_lat_lon();
		break;
	}
}




void GeorefConfigDialog::coord_mode_changed_cb(int combo_index)
{
	const int current_coord_mode = this->coord_mode_combo->currentData().toInt();

	/* TODO: figure out how to delete widgets that were
	   replaced. They are no longer owned by layout, so they have
	   to be deleted. */

	switch ((CoordMode) current_coord_mode) {
	case CoordMode::UTM:
		qDebug() << "II" PREFIX << "current coordinate mode is UTM";
		this->sync_from_utm_to_lat_lon();

		this->grid->replaceWidget(this->lat_lon_tl_entry, this->utm_entry);
		this->grid->replaceWidget(this->lat_lon_br_entry, this->dummy_entry1);
		this->grid->replaceWidget(this->calc_mpp_button, this->dummy_entry2);

		this->utm_entry->show();
		this->dummy_entry1->show();
		this->dummy_entry2->show();

		this->lat_lon_tl_entry->hide();
		this->lat_lon_br_entry->hide();
		this->calc_mpp_button->hide();

		break;

	case CoordMode::LATLON:
		qDebug() << "II" PREFIX << "current coordinate mode is LatLon";
		this->sync_from_lat_lon_to_utm();

		this->grid->replaceWidget(this->utm_entry, this->lat_lon_tl_entry);
		this->grid->replaceWidget(this->dummy_entry1, this->lat_lon_br_entry);
		this->grid->replaceWidget(this->dummy_entry2, this->calc_mpp_button);

		this->lat_lon_tl_entry->show();
		this->lat_lon_br_entry->show();
		this->calc_mpp_button->show();

		this->utm_entry->hide();
		this->dummy_entry1->hide();
		this->dummy_entry2->hide();

		break;

	default:
		qDebug() << "EE" PREFIX << "unexpected coordinate mode" << current_coord_mode;
		break;
	}
}




void GeorefConfigDialog::check_br_is_good_or_msg_user(void)
{
	/* If a 'blank' lat/lon value that's alright. */
	if (!this->layer->ll_br.is_valid()) {
		return;
	}

	const LatLon ll_tl = this->get_ll_tl();
	if (ll_tl.lat < this->layer->ll_br.lat || ll_tl.lon > this->layer->ll_br.lon) {
		Dialog::warning(tr("Lower right corner values may not be consistent with upper right values"), this->layer->get_window());
	}
}




void GeorefConfigDialog::calculate_mpp_from_coords_cb(void)
{
	const QString filename = this->map_image_file_entry->get_filename();
	if (!filename.length()) {
		return;
	}

	QPixmap * img_pixmap = new QPixmap();
	if (!img_pixmap->load(filename)) {
		delete img_pixmap;
		img_pixmap = NULL;

		Dialog::error(tr("Couldn't open image file %1").arg(filename), this->layer->get_window());
		return;
	}

	const int img_width = img_pixmap->width();
	const int img_height = img_pixmap->height();

	if (img_width == 0 || img_height == 0) {
		Dialog::error(tr("Invalid image size: %1").arg(filename), this->layer->get_window());
	} else {
		this->sync_coords_in_entries();

		double xmpp, ympp;
		georef_layer_mpp_from_coords(CoordMode::LATLON, this->get_ll_tl(), this->get_ll_br(), img_width, img_height, &xmpp, &ympp);

		this->x_scale_spin->setValue(xmpp);
		this->y_scale_spin->setValue(ympp);

		this->check_br_is_good_or_msg_user();
	}
#ifdef K_TODO
	g_object_unref(G_OBJECT(img_pixmap));
#endif
}




#define VIK_SETTINGS_GEOREF_TAB "georef_coordinate_tab"





GeorefConfigDialog::GeorefConfigDialog(LayerGeoref * the_layer, QWidget * parent)
{
	this->layer = the_layer;

	this->setWindowTitle(tr("Layer Properties"));

	this->button_box->button(QDialogButtonBox::Cancel)->setDefault(true);
	QPushButton * cancel_button = this->button_box->button(QDialogButtonBox::Cancel);

	int row = 0;

	this->map_image_file_entry = new SGFileEntry(QFileDialog::Option(0), QFileDialog::AnyFile, SGFileTypeFilter::IMAGE, tr("Select image file"), this->layer->get_window());
#ifdef K_TODO
	vik_file_entry_new (GTK_FILE_CHOOSER_ACTION_OPEN, SGFileTypeFilter::IMAGE, maybe_read_world_file, this);
#endif
	this->grid->addWidget(new QLabel(tr("Map Image:")), row, 0);
	this->grid->addWidget(this->map_image_file_entry, row, 1);
	row++;

	this->world_file_entry = new SGFileEntry(QFileDialog::Option(0), QFileDialog::AnyFile, SGFileTypeFilter::ANY, tr("Select world file"), this->layer->get_window());
	this->grid->addWidget(new QLabel(tr("World File Parameters:")), row, 0);
	this->grid->addWidget(this->world_file_entry, row, 1);
	row++;

	this->x_scale_spin = new QDoubleSpinBox();
	this->x_scale_spin->setMinimum(SG_VIEWPORT_ZOOM_MIN);
	this->x_scale_spin->setMaximum(SG_VIEWPORT_ZOOM_MAX);
	this->x_scale_spin->setSingleStep(1);
	this->x_scale_spin->setValue(4);
	this->x_scale_spin->setToolTip(tr("The scale of the map in the X direction (meters per pixel)"));
	this->grid->addWidget(new QLabel(tr("X (easting) scale (mpp): ")), row, 0);
	this->grid->addWidget(this->x_scale_spin, row, 1);
	row++;

	this->y_scale_spin = new QDoubleSpinBox();
	this->y_scale_spin->setMinimum(SG_VIEWPORT_ZOOM_MIN);
	this->y_scale_spin->setMaximum(SG_VIEWPORT_ZOOM_MAX);
	this->y_scale_spin->setSingleStep(1);
	this->y_scale_spin->setValue(4);
	this->y_scale_spin->setToolTip(tr("The scale of the map in the Y direction (meters per pixel)"));
	this->grid->addWidget(new QLabel(tr("Y (northing) scale (mpp): ")), row, 0);
	this->grid->addWidget(this->y_scale_spin, row, 1);
	row++;

	this->coord_mode_combo = new QComboBox();
	this->coord_mode_combo->addItem(tr("UTM"), (int) CoordMode::UTM);
	this->coord_mode_combo->addItem(tr("Latitude/Longitude"), (int) CoordMode::LATLON);
	this->grid->addWidget(new QLabel(tr("Coordinate Mode")), row, 0);
	this->grid->addWidget(this->coord_mode_combo, row, 1);
	row++;


	{
		this->utm_entry = new SGUTMEntry();
		this->utm_entry->set_value(this->layer->utm_tl);
		this->utm_entry->set_text(tr("Corner pixel easting:"),
					  tr("The UTM \"easting\" value of the upper-left corner pixel of the map"),
					  tr("Corner pixel northing:"),
					  tr("The UTM \"northing\" value of the upper-left corner pixel of the map"));

		this->grid->addWidget(this->utm_entry, row, 0, 1, 2);
		row++;

		this->dummy_entry1 = new QWidget();
		this->grid->addWidget(this->dummy_entry1, row, 0, 1, 2);
		row++;

		this->dummy_entry2 = new QWidget();
		this->grid->addWidget(this->dummy_entry2, row, 0, 1, 2);
		row++;
	}

	this->x_scale_spin->setValue(this->layer->mpp_easting);
	this->y_scale_spin->setValue(this->layer->mpp_northing);
	if (!this->layer->image_full_path.isEmpty()) {
		this->map_image_file_entry->set_filename(this->layer->image_full_path);
	}


	{
		/* We don't add these widget to grid (yet). Initial
		   coord mode is UTM, so the dialog/grid will contain
		   entry widgets for UTM coords, not for LatLon
		   coords. Entry widgets for LatLon will be displayed
		   after coord mode is changed to LatLon (replacing
		   entry widgets for UTM).  */

		const Coord coord(this->layer->utm_tl, CoordMode::LATLON);

		this->lat_lon_tl_entry = new SGLatLonEntry();
		this->lat_lon_tl_entry->set_text(tr("Upper left latitude:"),
						 tr("Upper left latitude"),
						 tr("Upper left longitude:"),
						 tr("Upper left longitude"));
		this->lat_lon_tl_entry->set_value(coord.ll);

		this->lat_lon_br_entry = new SGLatLonEntry();
		this->lat_lon_br_entry->set_text(tr("Lower right latitude:"),
						 tr("Lower right latitude"),
						 tr("Lower right longitude:"),
						 tr("Lower right longitude"));
		this->lat_lon_br_entry->set_value(this->layer->ll_br);

		this->calc_mpp_button = new QPushButton(tr("Calculate MPP values from coordinates"));
		this->calc_mpp_button->setToolTip(tr("Enter all corner coordinates before calculating the MPP values from the image size"));
	}


	ParameterScale alpha_scale = { 0, 255, SGVariant((int32_t) this->layer->alpha), 1, 0 };
	this->alpha_slider = new SGSlider(alpha_scale, Qt::Horizontal);
	this->grid->addWidget(new QLabel(tr("Alpha:")), row, 0);
	this->grid->addWidget(this->alpha_slider, row, 1);
	row++;


	QObject::connect(this->calc_mpp_button, SIGNAL (released(void)), this, SLOT (calculate_mpp_from_coords_cb(void)));
#ifdef K_TODO
	QObject::connect(world_file_entry_button, SIGNAL (triggered(bool)), this, SLOT (load_cb));
#endif
	QObject::connect(this->coord_mode_combo, SIGNAL (currentIndexChanged(int)), this, SLOT (coord_mode_changed_cb(int)));

	if (cancel_button) {
		cancel_button->setFocus();
	}



	/* Remember that selecting coord mode must be done after the widget is visible. */
	int coord_mode = 0;
	if (ApplicationState::get_integer(VIK_SETTINGS_GEOREF_TAB, &coord_mode)) {
		if (coord_mode != (int) CoordMode::UTM && coord_mode != (int) CoordMode::LATLON) {
			coord_mode = (int) CoordMode::UTM;
		}
	}
	const int coord_index = this->coord_mode_combo->findData(coord_mode);
	if (coord_index != -1) {
		this->coord_mode_combo->setCurrentIndex(coord_index);
	}
}




/* Returns true if OK was pressed. */
bool LayerGeoref::dialog(Viewport * viewport, Window * window_)
{
	GeorefConfigDialog dialog(this, window_);
	if (dialog.exec() != QDialog::Accepted) {
		return false;
	}

	dialog.sync_coords_in_entries();

	this->utm_tl = dialog.utm_entry->get_value();

	this->mpp_easting = dialog.x_scale_spin->value();
	this->mpp_northing = dialog.y_scale_spin->value();
	this->ll_br = dialog.get_ll_br();
	dialog.check_br_is_good_or_msg_user();

	/* Remember to get alpha value before calling post_read() that uses the alpha value. */
	this->alpha = dialog.alpha_slider->get_value();

	/* TODO check if image has changed otherwise no need to regenerate pixmap. */
	if (this->image.isNull()) {
		if (this->image_full_path != dialog.map_image_file_entry->get_filename()) {
			this->set_image_full_path(dialog.map_image_file_entry->get_filename());
			this->post_read(viewport, false);
		}
	}

	if (!this->image.isNull() && this->alpha < 255) {
		ui_pixmap_set_alpha(this->image, this->alpha);
	}

	if (!this->scaled_image.isNull() && this->alpha < 255) {
		ui_pixmap_set_alpha(this->scaled_image, this->alpha);
	}

	const int current_coord_mode = dialog.coord_mode_combo->currentData().toInt();
	ApplicationState::set_integer(VIK_SETTINGS_GEOREF_TAB, current_coord_mode);

	return true;
}




void LayerGeoref::zoom_to_fit_cb(void)
{
	Viewport * viewport = g_tree->tree_get_main_viewport();
	viewport->set_xmpp(this->mpp_easting);
	viewport->set_ympp(this->mpp_northing);

	g_tree->emit_items_tree_updated();
}




void LayerGeoref::goto_center_cb(void)
{
	Viewport * viewport = g_tree->tree_get_main_viewport();
	UTM utm = viewport->get_center()->get_utm();

	utm.easting = this->utm_tl.easting + (this->image_width * this->mpp_easting / 2); /* Only an approximation. */
	utm.northing = this->utm_tl.northing - (this->image_height * this->mpp_northing / 2);

	viewport->set_center_from_coord(Coord(utm, viewport->get_coord_mode()), true);

	g_tree->emit_items_tree_updated();
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

	this->action_icon_path   = ":/icons/layer_tool/georef_move_18.png";
	this->action_label       = QObject::tr("&Georef Move Map");
	this->action_tooltip     = QObject::tr("Georef Move Map");
	// this->action_accelerator = ...; /* Empty accelerator. */

	this->cursor_click = new QCursor(Qt::ClosedHandCursor);
	this->cursor_release = new QCursor(Qt::OpenHandCursor);
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
		this->utm_tl.easting += (ev->x() - this->click_x) * tool->viewport->get_xmpp();
		this->utm_tl.northing -= (ev->y() - this->click_y) * tool->viewport->get_ympp();
		this->emit_layer_changed();
		return true;
	}
	return false; /* I didn't move anything on this layer! */
}




LayerToolGeorefZoom::LayerToolGeorefZoom(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::GEOREF)
{
	this->id_string = "sg.tool.layer_georef.zoom";

	this->action_icon_path   = ":/icons/layer_tool/georef_zoom_18.png";
	this->action_label       = QObject::tr("Georef Z&oom Tool");
	this->action_tooltip     = QObject::tr("Georef Zoom Tool");
	// this->action_accelerator = ...; /* Empty accelerator. */

	this->cursor_click = new QCursor(Qt::ArrowCursor);
	this->cursor_release = new QCursor(Qt::ArrowCursor);
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
	if (!pixmap || pixmap->isNull()) {
		/* Bad image */
		qDebug() << "EE" PREFIX << "trying to create layer with invalid image";
		return NULL;
	}

	if (pixmap->width() <= 0 || pixmap->width() <= 0) {
		/* Bad image */
		qDebug() << "EE" PREFIX << "trying to create layer with invalid image";
		return NULL;
	}

	LayerGeoref * layer = new LayerGeoref();
	layer->configure_from_viewport(viewport);
	layer->set_name(name);
	layer->image = *pixmap;
	layer->utm_tl = coord_tl.get_utm();
	layer->ll_br = coord_br.get_latlon();
	layer->image_width = layer->image.width();
	layer->image_height = layer->image.height();

	const LatLon ll_tl = coord_tl.get_latlon();
	const LatLon ll_br = coord_br.get_latlon();
	const CoordMode coord_mode = viewport->get_coord_mode();

	double xmpp, ympp;
	georef_layer_mpp_from_coords(coord_mode, ll_tl, ll_br, layer->image_width, layer->image_height, &xmpp, &ympp);
	layer->mpp_easting = xmpp;
	layer->mpp_northing = ympp;

	goto_center_ll(viewport, ll_tl, ll_br);
	/* Set best zoom level. */
	vu_zoom_to_show_latlons(viewport->get_coord_mode(), viewport, LatLonMinMax(ll_br, ll_tl));

	return layer;
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
}




/* To be called right after constructor. */
void LayerGeoref::configure_from_viewport(Viewport const * viewport)
{
	/* Make these defaults based on the current view. */
	this->mpp_northing = viewport->get_ympp();
	this->mpp_easting = viewport->get_xmpp();
	this->utm_tl = viewport->get_center()->get_utm();
}
