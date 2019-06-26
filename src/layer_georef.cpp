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
#include "viewport_zoom.h"
#include "viewport_internal.h"
#include "layers_panel.h"




using namespace SlavGPS;




#define SG_MODULE "Layer Georef"




enum {
	PARAM_IMAGE_FILE_FULL_PATH = 0,
	PARAM_CORNER_UTM_EASTING,
	PARAM_CORNER_UTM_NORTHING,
	PARAM_MPP_EASTING,
	PARAM_MPP_NORTHING,
	PARAM_CORNER_UTM_ZONE,
	PARAM_CORNER_UTM_BAND_LETTER,
	PARAM_ALPHA,
	NUM_PARAMS
};




/* https://en.wikipedia.org/wiki/World_file */
class SlavGPS::WorldFile {
public:
	enum class ReadStatus {
		Success = 0,
		OpenError = 1,
		ReadError = 2,
	};

	ReadStatus read_file(const QString & file_full_path);

	/* We do not handle 'skew' values ATM - normally they are a value of 0 anyway to align with the UTM grid. */
	double a = 0.0; /* "A: pixel size in the x-direction in map units/pixel (x-component of the pixel width (x-scale))" */
	double d = 0.0; /* "D: rotation about y-axis (y-component of the pixel width (y-skew))" */
	double b = 0.0; /* "B: rotation about x-axis (x-component of the pixel height (x-skew))" */
	double e = 0.0; /* "E: pixel size in the y-direction in map units, almost always negative (y-component of the pixel height (y-scale))" */
	double c = 0.0; /* "C: x-coordinate of the center of the upper left pixel" */
	double f = 0.0; /* "F: y-coordinate of the center of the upper left pixel" */

private:
	bool read_line(QFile & file, double & value);
};




/* Properties dialog for Georef Layer is too complicated
   (non-standard) to be built with standard UI builder based on
   parameter specifications. Therefore all parameters are marked as
   "hidden". This means that widgets in the properties dialog (and the
   dialog itself) will not be built using parameters specification. */

static ParameterSpecification georef_layer_param_specs[] = {
	{ PARAM_IMAGE_FILE_FULL_PATH,    "image",                SGVariantType::String, PARAMETER_GROUP_HIDDEN, "", WidgetType::None, NULL, NULL, "" },
	{ PARAM_CORNER_UTM_EASTING,      "corner_easting",       SGVariantType::Double, PARAMETER_GROUP_HIDDEN, "", WidgetType::None, NULL, NULL, "" },
	{ PARAM_CORNER_UTM_NORTHING,     "corner_northing",      SGVariantType::Double, PARAMETER_GROUP_HIDDEN, "", WidgetType::None, NULL, NULL, "" },
	{ PARAM_MPP_EASTING,             "mpp_easting",          SGVariantType::Double, PARAMETER_GROUP_HIDDEN, "", WidgetType::None, NULL, NULL, "" },
	{ PARAM_MPP_NORTHING,            "mpp_northing",         SGVariantType::Double, PARAMETER_GROUP_HIDDEN, "", WidgetType::None, NULL, NULL, "" },
	{ PARAM_CORNER_UTM_ZONE,         "corner_zone",          SGVariantType::Int,    PARAMETER_GROUP_HIDDEN, "", WidgetType::None, NULL, NULL, "" },
	{ PARAM_CORNER_UTM_BAND_LETTER,  "corner_letter_as_int", SGVariantType::Int,    PARAMETER_GROUP_HIDDEN, "", WidgetType::None, NULL, NULL, "" },
	{ PARAM_ALPHA,                   "alpha",                SGVariantType::Int,    PARAMETER_GROUP_HIDDEN, "", WidgetType::None, NULL, NULL, "" },

	{ NUM_PARAMS,                    "",                     SGVariantType::Empty,  PARAMETER_GROUP_HIDDEN, "", WidgetType::None, NULL, NULL, "" }, /* Guard. */
};




LayerGeorefInterface vik_georef_layer_interface;
static ParameterScale<int> alpha_scale(0, 255, SGVariant((int32_t) 0), 1, 0);




LayerGeorefInterface::LayerGeorefInterface()
{
	this->parameters_c = georef_layer_param_specs;

	this->fixed_layer_type_string = "GeoRef Map"; /* Non-translatable. */

	// this->action_accelerator = ...; /* Empty accelerator. */
	// this->action_icon = ...; /* Set elsewhere. */

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
	{ 0, PREFERENCES_NAMESPACE_IO "georef_auto_read_world_file", SGVariantType::Boolean, PARAMETER_GROUP_GENERIC, QObject::tr("Auto Read World Files:"), WidgetType::CheckButton, NULL, NULL, QObject::tr("Automatically attempt to read associated world file of a new image for a GeoRef layer") }
};




void SlavGPS::layer_georef_init(void)
{
	Preferences::register_parameter_instance(io_prefs[0], SGVariant(true, io_prefs[0].type_id));
}




QString LayerGeoref::get_tooltip(void) const
{
	return this->image_file_full_path;
}




Layer * LayerGeorefInterface::unmarshall(Pickle & pickle, Viewport * viewport)
{
	LayerGeoref * layer = new LayerGeoref();
	layer->configure_from_viewport(viewport);

	layer->unmarshall_params(pickle);

	if (!layer->image_file_full_path.isEmpty()) {
		layer->post_read(viewport, true);
	}
	return layer;
}




bool LayerGeoref::set_param_value(param_id_t param_id, const SGVariant & param_value, bool is_file_operation)
{
	switch (param_id) {
	case PARAM_IMAGE_FILE_FULL_PATH:
		this->reset_pixmaps();
		this->set_image_file_full_path(param_value.val_string);
		break;
	case PARAM_CORNER_UTM_EASTING:
		this->utm_tl.easting = param_value.u.val_double;
		break;
	case PARAM_CORNER_UTM_NORTHING:
		this->utm_tl.northing = param_value.u.val_double;
		break;
	case PARAM_MPP_EASTING:
		this->mpp_easting = param_value.u.val_double;
		break;
	case PARAM_MPP_NORTHING:
		this->mpp_northing = param_value.u.val_double;
		break;
	case PARAM_CORNER_UTM_ZONE:
		if (sg_ret::ok != this->utm_tl.set_zone(param_value.u.val_int)) {
			qDebug() << SG_PREFIX_E << "Failed to set UTM zone from" << param_value.u.val_int;
		}
		break;
	case PARAM_CORNER_UTM_BAND_LETTER:
		/* The parameter is called "corner_letter_as_int", so we have to use .val_int here. */
		if (sg_ret::ok != this->utm_tl.set_band_letter((char) param_value.u.val_int)) {
			qDebug() << SG_PREFIX_E << "Failed to set UTM band letter from" << param_value.u.val_int;;
		}
		break;
	case PARAM_ALPHA:
		/* Alpha shall always be of type int. */
		if (alpha_scale.is_in_range(param_value.u.val_int)) {
			this->alpha = param_value.u.val_int;
		} else {
			qDebug() << SG_PREFIX_E << "Invalid alpha value" << param_value.u.val_int;
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
		qDebug() << SG_PREFIX_W << "Failed to save pixmap to" << path;
	} else {
		this->image_file_full_path = path;
	}
}




SGVariant LayerGeoref::get_param_value(param_id_t param_id, bool is_file_operation) const
{
	SGVariant rv;
	switch (param_id) {
	case PARAM_IMAGE_FILE_FULL_PATH: {
		bool is_set = false;
		if (is_file_operation) {
			if (!this->image.isNull() && this->image_file_full_path.isEmpty()) {
				/* Force creation of image file. */
				((LayerGeoref *) this)->create_image_file();
			}
			if (Preferences::get_file_path_format() == FilePathFormat::Relative) {
				const QString cwd = QDir::currentPath();
				if (!cwd.isEmpty()) {
					rv = SGVariant(file_GetRelativeFilename(cwd, this->image_file_full_path));
					is_set = true;
				}
			}
		}
		if (!is_set) {
			rv = SGVariant(this->image_file_full_path);
		}
		break;
	}
	case PARAM_CORNER_UTM_EASTING:
		rv = SGVariant(this->utm_tl.get_easting());
		break;
	case PARAM_CORNER_UTM_NORTHING:
		rv = SGVariant(this->utm_tl.get_northing());
		break;
	case PARAM_MPP_EASTING:
		rv = SGVariant(this->mpp_easting);
		break;
	case PARAM_MPP_NORTHING:
		rv = SGVariant(this->mpp_northing);
		break;
	case PARAM_CORNER_UTM_ZONE:
		rv = SGVariant((int32_t) this->utm_tl.get_zone());
		break;
	case PARAM_CORNER_UTM_BAND_LETTER:
		/* The parameter is called "corner_letter_as_int", so we have to cast to int here. */
		rv = SGVariant((int32_t) this->utm_tl.get_band_as_letter());
		break;
	case PARAM_ALPHA:
		/* Alpha shall always be int. Cast to int here, to be sure that it's stored in variant correctly. */
		rv = SGVariant((int32_t) this->alpha);
		break;
	default:
		break;
	}
	return rv;
}




/**
   Return mpp for the given coords, coord mode and image size.
*/
static void georef_layer_mpp_from_coords(CoordMode mode, const LatLon & lat_lon_tl, const LatLon & lat_lon_br, int width, int height, double * xmpp, double * ympp)
{
	const LatLon lat_lon_tr(lat_lon_tl.lat, lat_lon_br.lon);
	const LatLon lat_lon_bl(lat_lon_br.lat, lat_lon_tl.lon);

	/* UTM mode should be exact MPP. */
	double factor = 1.0;
	if (mode == CoordMode::LatLon) {
		/* NB the 1.193 - is at the Equator.
		   http://wiki.openstreetmap.org/wiki/Zoom_levels */

		/* Convert from actual image MPP to Viking 'pixelfact'. */
		double mid_lat = (lat_lon_bl.lat + lat_lon_tr.lat) / 2.0;
		/* Protect against div by zero (but shouldn't have 90 degrees for mid latitude...). */
		if (fabs(mid_lat) < 89.9) {
			factor = cos(DEG2RAD(mid_lat)) * 1.193;
		}
	}

	const double diffx = LatLon::get_distance(lat_lon_tl, lat_lon_tr);
	*xmpp = (diffx / width) / factor;

	const double diffy = LatLon::get_distance(lat_lon_tl, lat_lon_bl);
	*ympp = (diffy / height) / factor;
}




void LayerGeoref::draw_tree_item(Viewport * viewport, bool highlight_selected, bool parent_is_selected)
{
	if (this->image.isNull()) {
		qDebug() << SG_PREFIX_I << "Not drawing the layer, no image";
		return;
	}

	const LatLonBBox viewport_bbox = viewport->get_bbox();
	const LatLonBBox image_bbox(this->lat_lon_tl, this->lat_lon_br);
	qDebug() << SG_PREFIX_I << "Viewport bbox" << viewport_bbox;
	qDebug() << SG_PREFIX_I << "Image bbox   " << image_bbox;

	if (!BBOX_INTERSECT (viewport_bbox, image_bbox)) {
		qDebug() << SG_PREFIX_I << "BBoxes don't intersect";
		return;
	}

	qDebug() << SG_PREFIX_I << "BBoxes intersect";
	QPen pen;
	pen.setColor("red");
	pen.setWidth(1);
	viewport->draw_bbox(image_bbox, pen);

	QPixmap transformed_image = this->image;

#if 0
	const Coord coord_tl(this->utm_tl, viewport->get_coord_mode());
	const ScreenPos pos_tl = viewport->coord_to_screen_pos(coord_tl);

	QRect sub_viewport_rect;
	sub_viewport_rect.setTopLeft(QPoint(pos_tl.x, pos_tl.y));
	sub_viewport_rect.setWidth(this->image_width);
	sub_viewport_rect.setHeight(this->image_height);

	bool scale_mismatch = false; /* Flag to scale the pixmap if it doesn't match our dimensions. */
	const double xmpp = viewport->get_viking_scale().get_x();
	const double ympp = viewport->get_viking_scale().get_y();
	if (xmpp != this->mpp_easting || ympp != this->mpp_northing) {
		scale_mismatch = true;
		sub_viewport_rect.setWidth(round(this->image_width * this->mpp_easting / xmpp));
		sub_viewport_rect.setHeight(round(this->image_height * this->mpp_northing / ympp));
	}

	if (false && scale_mismatch) {
		/* Rescale image only if really necessary, i.e. if we don't have a valid copy of scaled image. */

		const int intended_width = sub_viewport_rect.width();
		const int intended_heigth = sub_viewport_rect.height();

		if (intended_width == this->scaled_image_width && intended_heigth == this->scaled_image_height && !this->scaled_image.isNull()) {
			/* Reuse existing scaled image. */
			transformed_image = this->scaled_image;
		} else {
			transformed_image = this->image.scaled(intended_width, intended_heigth, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

			this->scaled_image = transformed_image;
			this->scaled_image_width = intended_width;
			this->scaled_image_height = intended_heigth;
		}
	}
#else
	const Coord coord_tl(this->lat_lon_tl, viewport->get_coord_mode());
	const ScreenPos pos_tl = viewport->coord_to_screen_pos(coord_tl);

	const Coord coord_br(this->lat_lon_br, viewport->get_coord_mode());
	const ScreenPos pos_br = viewport->coord_to_screen_pos(coord_br);

	QRect sub_viewport_rect;
	sub_viewport_rect.setTopLeft(QPoint(pos_tl.x, pos_tl.y));
	sub_viewport_rect.setWidth(pos_br.x - pos_tl.x + 1);
	sub_viewport_rect.setHeight(pos_br.y - pos_tl.y + 1);
	qDebug() << SG_PREFIX_I << "Viewport rectangle =" << sub_viewport_rect;
#endif

	QRect image_rect;
	image_rect.setTopLeft(QPoint(0, 0));
	image_rect.setWidth(transformed_image.width());
	image_rect.setHeight(transformed_image.height());

	qDebug() << "++++++++ EXPECT 0 0:" << (transformed_image.width() - sub_viewport_rect.width()) << (transformed_image.height() - sub_viewport_rect.height());

	viewport->draw_pixmap(transformed_image, sub_viewport_rect, image_rect);
}




LayerGeoref::~LayerGeoref()
{
}




bool LayerGeoref::properties_dialog(Viewport * viewport)
{
	return this->run_dialog(viewport, this->get_window());
}




/* Also known as LayerGeoref::load_image(). */
void LayerGeoref::post_read(Viewport * viewport, bool from_file)
{
	if (this->image_file_full_path.isEmpty()) {
		qDebug() << SG_PREFIX_I << "Not loading image, file path is empty";
		return;
	}
	qDebug() << SG_PREFIX_I << "Will try to load image from" << this->image_file_full_path;


	this->reset_pixmaps();


	if (this->image.load(this->image_file_full_path)) {
		this->image_width = this->image.width();
		this->image_height = this->image.height();

		if (!this->image.isNull() && alpha_scale.is_in_range(this->alpha)) {
			ui_pixmap_set_alpha(this->image, this->alpha);
		}
	} else {
		qDebug() << SG_PREFIX_E << "Failed to load image from" << this->image_file_full_path;

		this->image = QPixmap();
		assert (this->image.isNull());

		if (!from_file) {
			Dialog::error(tr("Couldn't open image file %1").arg(this->image_file_full_path), this->get_window());
		}
	}

	/* Should find length and width here too. */
}




void LayerGeoref::reset_pixmaps(void)
{
	if (!this->image.isNull()) {
		this->image = QPixmap();
		assert (this->image.isNull());
	}

	if (!this->scaled_image.isNull()) {
		this->scaled_image = QPixmap();
		assert (this->scaled_image.isNull());
	}
}




void LayerGeoref::set_image_file_full_path(const QString & value)
{
	if (!value.isEmpty()) {
		this->image_file_full_path = value;
	} else {
		this->image_file_full_path = vu_get_canonical_filename(this, value, this->get_window()->get_current_document_full_path());
	}
}




/* Only positive values allowed here. */
static void double2spinwidget(QDoubleSpinBox * spinbox, double val)
{
	spinbox->setValue(val > 0 ? val : -val);
}




void GeorefConfigDialog::set_widget_values(const WorldFile & wfile)
{
	double2spinwidget(this->x_scale_spin, wfile.a);
	double2spinwidget(this->y_scale_spin, wfile.e);

	this->lat_lon_tl_entry->set_value(LatLon(wfile.f, wfile.c));
}




bool WorldFile::read_line(QFile & file, double & value)
{
	char buffer[256];
	if (-1 == file.readLine(buffer, sizeof (buffer))) {
		qDebug() << SG_PREFIX_E << "Failed to read line from file" << file.errorString();
		return false;
	}

	bool ok = false;
	value = QString(buffer).toDouble(&ok);
	if (!ok) {
		qDebug() << SG_PREFIX_E << "Failed to convert" << buffer << "to double";
		return false;
	}

	qDebug() << SG_PREFIX_I << "Read value" << value;
	return true;
}




/**
 * http://en.wikipedia.org/wiki/World_file
 *
 * Note world files do not define the units and nor are the units standardized :(
 * Currently Viking only supports:
 *  x&y scale as meters per pixel
 *  x&y coords as UTM eastings and northings respectively.
 */
WorldFile::ReadStatus WorldFile::read_file(const QString & full_path)
{
	qDebug() << SG_PREFIX_I << "Opening World file" << full_path;

	QFile file(full_path);
	if (!file.open(QIODevice::ReadOnly)) {
		qDebug() << SG_PREFIX_E << "Failed to open file" << full_path << file.errorString();
		return WorldFile::ReadStatus::OpenError;
	}

	WorldFile::ReadStatus answer;

	if (this->read_line(file, this->a)
	    && this->read_line(file, this->d)
	    && this->read_line(file, this->b)
	    && this->read_line(file, this->e)
	    && this->read_line(file, this->c)
	    && this->read_line(file, this->f)) {

		qDebug() << SG_PREFIX_I << "Successful read of all values from World File";
		answer = WorldFile::ReadStatus::Success;
	} else {
		qDebug() << SG_PREFIX_E << "Failed to read from World File";
		answer = WorldFile::ReadStatus::ReadError;
	}

	file.close();
	return answer;
}




void GeorefConfigDialog::load_world_file_cb(void)
{
	const QStringList selection = this->world_file_selector->get_selected_files_full_paths();
	if (0 == selection.size()) {
		return;
	}

	WorldFile wfile;
	const WorldFile::ReadStatus answer = wfile.read_file(selection.at(0));

	switch (answer) {
	case WorldFile::ReadStatus::OpenError:
		Dialog::error(tr("The World file you requested could not be opened for reading."), this);
		break;
	case WorldFile::ReadStatus::ReadError:
		Dialog::error(tr("Unexpected end of file reading World file."), this);
		break;
	case WorldFile::ReadStatus::Success:
		this->set_widget_values(wfile);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled read status" << (int) answer;
		break;
	}
}




void LayerGeoref::export_params_cb(void)
{
	Window * window = ThisApp::get_main_window();

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


	FILE * file = fopen(selection.at(0).toUtf8().constData(), "w");
	if (!file) {
		Dialog::error(QObject::tr("The file you requested could not be opened for writing."), window);
		return;
	}

	fprintf(file, "%f\n%f\n%f\n%f\n%f\n%f\n", this->mpp_easting, this->mpp_northing, 0.0, 0.0, this->utm_tl.easting, this->utm_tl.northing);
	fclose(file);
	file = NULL;
}




/**
   Auto attempt to read the world file associated with the image used for the georef.
   Based on simple file name conventions.
   Only attempted if the preference is on.
*/
static void maybe_read_world_file(FileSelectorWidget * file_selector, void * user_data)
{
	if (!user_data) {
		return;
	}
	GeorefConfigDialog * dialog = (GeorefConfigDialog *) user_data;

	if (!Preferences::get_param_value(PREFERENCES_NAMESPACE_IO "georef_auto_read_world_file").u.val_bool) {
		return;
	}

	const QString file_full_path = file_selector->get_selected_file_full_path();
	if (file_full_path.isEmpty()) {
		return;
	}


	const int len = file_full_path.length();
	const bool upper = file_full_path[len - 1].isUpper();
	WorldFile wfile;


	/* Naming convention 1: .jpg -> .jpgw */
	{
		const QString world_file_full_path = file_full_path + (upper ? "W" : "w");
		qDebug() << SG_PREFIX_I << "Trying to open file with naming convention 1:" << world_file_full_path;

		if (WorldFile::ReadStatus::Success == wfile.read_file(world_file_full_path)) {
			dialog->set_widget_values(wfile);
			qDebug() << SG_PREFIX_I << "Trying to open file with naming convention 1: success";
			return;
		}
	}

	/* Naming convention 2: .jpg -> .jgw */
	{
		if (len > 3) {
			QString world_file_full_path = file_full_path;
			const QChar last_char = file_full_path[len - 1];

			world_file_full_path[len - 2] = last_char;
			world_file_full_path[len - 1] = upper ? 'W' : 'w';
			qDebug() << SG_PREFIX_I << "Trying to open file with naming convention 2:" << world_file_full_path;

			if (WorldFile::ReadStatus::Success == wfile.read_file(world_file_full_path)) {
				dialog->set_widget_values(wfile);
				qDebug() << SG_PREFIX_I << "Trying to open file with naming convention 2: success";
				return;
			}
		}
	}

	/* Naming convention 3: always .wld */
	{
		if (len > 3) {
			QString world_file_full_path = file_full_path;
			world_file_full_path.chop(3);
			if (upper) {
				world_file_full_path.append("WLD");
			} else {
				world_file_full_path.append("wld");
			}
			qDebug() << SG_PREFIX_I << "Trying to open file with naming convention 3:" << world_file_full_path;

			if (WorldFile::ReadStatus::Success == wfile.read_file(world_file_full_path)) {
				dialog->set_widget_values(wfile);
				qDebug() << SG_PREFIX_I << "Trying to open file with naming convention 3: success";
				return;
			}
		}
	}
}




LatLon GeorefConfigDialog::get_lat_lon_tl(void) const
{
	return this->lat_lon_tl_entry->get_value();
}




LatLon GeorefConfigDialog::get_lat_lon_br(void) const
{
	return this->lat_lon_br_entry->get_value();
}




/* Align displayed UTM values with displayed Lat/Lon values. */
void GeorefConfigDialog::sync_from_utm_to_lat_lon(void)
{
	const UTM utm = LatLon::to_utm(this->get_lat_lon_tl());
	this->utm_entry->set_value(utm);
}




/* Synchronize displayed Lat/Lon values with displayed UTM values. */
void GeorefConfigDialog::sync_from_lat_lon_to_utm(void)
{
	const UTM utm_corner = this->utm_entry->get_value();
	const LatLon lat_lon = UTM::to_lat_lon(utm_corner);

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
		qDebug() << SG_PREFIX_I << "Current coordinate mode is UTM";
		this->sync_from_lat_lon_to_utm();
		break;

	case CoordMode::LatLon:
		qDebug() << SG_PREFIX_I << "Current coordinate mode is LatLon";
		break;

	default:
		qDebug() << SG_PREFIX_E << "Unexpected coordinate mode" << current_coord_mode;
		this->sync_from_utm_to_lat_lon();
		break;
	}
}




void GeorefConfigDialog::coord_mode_changed_cb(int combo_index)
{
	const int current_coord_mode = this->coord_mode_combo->currentData().toInt();

	/* TODO_LATER: figure out how to delete widgets that were
	   replaced. They are no longer owned by layout, so they have
	   to be deleted. */

	switch ((CoordMode) current_coord_mode) {
	case CoordMode::UTM:
		qDebug() << SG_PREFIX_I << "Current coordinate mode is UTM";
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

	case CoordMode::LatLon:
		qDebug() << SG_PREFIX_I << "Current coordinate mode is LatLon";
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
		qDebug() << SG_PREFIX_E << "Unexpected coordinate mode" << current_coord_mode;
		break;
	}
}




void GeorefConfigDialog::check_br_is_good_or_msg_user(void)
{
	/* If a 'blank' lat/lon value that's alright. */
	if (!this->layer->lat_lon_br.is_valid()) {
		return;
	}

	const LatLon lat_lon_tl = this->get_lat_lon_tl();
	if (lat_lon_tl.lat < this->layer->lat_lon_br.lat || lat_lon_tl.lon > this->layer->lat_lon_br.lon) {
		Dialog::warning(tr("Lower right corner values may not be consistent with upper right values"), this->layer->get_window());
	}
}




void GeorefConfigDialog::calculate_mpp_from_coords_cb(void)
{
	const QString filename = this->map_image_file_selector->get_selected_file_full_path();
	if (!filename.length()) {
		return;
	}

	QPixmap img_pixmap;
	if (img_pixmap.load(filename)) {
		Dialog::error(tr("Couldn't open image file %1").arg(filename), this->layer->get_window());
		return;
	}

	const int img_width = img_pixmap.width();
	const int img_height = img_pixmap.height();

	if (img_width == 0 || img_height == 0) {
		Dialog::error(tr("Invalid image size: %1").arg(filename), this->layer->get_window());
	} else {
		this->sync_coords_in_entries();

		double xmpp, ympp;
		georef_layer_mpp_from_coords(CoordMode::LatLon, this->get_lat_lon_tl(), this->get_lat_lon_br(), img_width, img_height, &xmpp, &ympp);

		this->x_scale_spin->setValue(xmpp);
		this->y_scale_spin->setValue(ympp);

		this->check_br_is_good_or_msg_user();
	}
}




#define VIK_SETTINGS_GEOREF_TAB "georef_coordinate_tab"





GeorefConfigDialog::GeorefConfigDialog(LayerGeoref * the_layer, QWidget * parent)
{
	this->layer = the_layer;

	this->setWindowTitle(tr("Layer Properties"));

	this->button_box->button(QDialogButtonBox::Cancel)->setDefault(true);
	QPushButton * cancel_button = this->button_box->button(QDialogButtonBox::Cancel);

	int row = 0;

	this->map_image_file_selector = new FileSelectorWidget(QFileDialog::Option(0), QFileDialog::AnyFile, tr("Select image file"), this->layer->get_window());
	this->map_image_file_selector->set_file_type_filter(FileSelectorWidget::FileTypeFilter::Image);
	this->map_image_file_selector->preselect_file_full_path(this->layer->image_file_full_path);
#ifdef FIXME_RESTORE /* Handle "maybe_read_world_file" argument in file selector. */
	vik_file_entry_new (GTK_FILE_CHOOSER_ACTION_OPEN, SGFileTypeFilter::IMAGE, maybe_read_world_file, this);
#endif
	this->grid->addWidget(new QLabel(tr("Map Image:")), row, 0);
	this->grid->addWidget(this->map_image_file_selector, row, 1);
	row++;


	this->world_file_selector = new FileSelectorWidget(QFileDialog::Option(0), QFileDialog::AnyFile, tr("Select world file"), this->layer->get_window());
	this->world_file_selector->preselect_file_full_path(this->layer->world_file_full_path);
	this->grid->addWidget(new QLabel(tr("World File Parameters:")), row, 0);
	this->grid->addWidget(this->world_file_selector, row, 1);
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
	this->coord_mode_combo->addItem(tr("Latitude/Longitude"), (int) CoordMode::LatLon);
	this->grid->addWidget(new QLabel(tr("Coordinate Mode")), row, 0);
	this->grid->addWidget(this->coord_mode_combo, row, 1);
	row++;


	{
		this->utm_entry = new UTMEntryWidget();
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
	if (!this->layer->image_file_full_path.isEmpty()) {
		this->map_image_file_selector->preselect_file_full_path(this->layer->image_file_full_path);
	}


	{
		/* We don't add these widget to grid (yet). Initial
		   coord mode is UTM, so the dialog/grid will contain
		   entry widgets for UTM coords, not for LatLon
		   coords. Entry widgets for LatLon will be displayed
		   after coord mode is changed to LatLon (replacing
		   entry widgets for UTM).  */

		const Coord coord(this->layer->utm_tl, CoordMode::LatLon);

		this->lat_lon_tl_entry = new LatLonEntryWidget();
		this->lat_lon_tl_entry->set_text(tr("Upper left latitude:"),
						 tr("Upper left latitude"),
						 tr("Upper left longitude:"),
						 tr("Upper left longitude"));
		this->lat_lon_tl_entry->set_value(coord.lat_lon);

		this->lat_lon_br_entry = new LatLonEntryWidget();
		this->lat_lon_br_entry->set_text(tr("Lower right latitude:"),
						 tr("Lower right latitude"),
						 tr("Lower right longitude:"),
						 tr("Lower right longitude"));
		this->lat_lon_br_entry->set_value(this->layer->lat_lon_br);

		this->calc_mpp_button = new QPushButton(tr("Calculate MPP values from coordinates"));
		this->calc_mpp_button->setToolTip(tr("Enter all corner coordinates before calculating the MPP values from the image size"));
	}


	alpha_scale.initial = SGVariant((int32_t) this->layer->alpha);
	this->alpha_slider = new SliderWidget(alpha_scale, Qt::Horizontal);
	this->grid->addWidget(new QLabel(tr("Alpha:")), row, 0);
	this->grid->addWidget(this->alpha_slider, row, 1);
	row++;


	QObject::connect(this->calc_mpp_button, SIGNAL (released(void)), this, SLOT (calculate_mpp_from_coords_cb(void)));
	QObject::connect(this->world_file_selector, SIGNAL (selection_is_made(void)), this, SLOT (load_world_file_cb(void)));
	QObject::connect(this->coord_mode_combo, SIGNAL (currentIndexChanged(int)), this, SLOT (coord_mode_changed_cb(int)));

	if (cancel_button) {
		cancel_button->setFocus();
	}



	/* Remember that selecting coord mode must be done after the widget is visible. */
	int coord_mode = 0;
	if (ApplicationState::get_integer(VIK_SETTINGS_GEOREF_TAB, &coord_mode)) {
		if (coord_mode != (int) CoordMode::UTM && coord_mode != (int) CoordMode::LatLon) {
			coord_mode = (int) CoordMode::UTM;
		}
	}
	const int coord_index = this->coord_mode_combo->findData(coord_mode);
	if (coord_index != -1) {
		this->coord_mode_combo->setCurrentIndex(coord_index);
	}
}




/* Returns true if OK was pressed. */
bool LayerGeoref::run_dialog(Viewport * viewport, QWidget * parent)
{
	GeorefConfigDialog dialog(this, parent);
	if (dialog.exec() != QDialog::Accepted) {
		return false;
	}


	dialog.sync_coords_in_entries();
	this->get_values_from_dialog(dialog);
	dialog.check_br_is_good_or_msg_user();


	/* TODO_MAYBE: we could check whether the parameters have
	   changed or if image file or world file have been changed on
	   disc and only then re-generate pixmap. */
	this->post_read(viewport, false);


	if (alpha_scale.is_in_range(this->alpha)) {
		if (!this->image.isNull()) {
			ui_pixmap_set_alpha(this->image, this->alpha);
		}
		if (!this->scaled_image.isNull()) {
			ui_pixmap_set_alpha(this->scaled_image, this->alpha);
		}
	}

	const int current_coord_mode = dialog.coord_mode_combo->currentData().toInt();
	ApplicationState::set_integer(VIK_SETTINGS_GEOREF_TAB, current_coord_mode);

	return true;
}




sg_ret LayerGeoref::get_values_from_dialog(const GeorefConfigDialog & dialog)
{
	this->set_image_file_full_path(dialog.map_image_file_selector->get_selected_file_full_path());
	this->world_file_full_path = dialog.world_file_selector->get_selected_file_full_path();
	this->alpha = dialog.alpha_slider->get_value();

	this->utm_tl = dialog.utm_entry->get_value();
	this->mpp_easting = dialog.x_scale_spin->value();
	this->mpp_northing = dialog.y_scale_spin->value();

	this->lat_lon_tl = dialog.get_lat_lon_tl();
	this->lat_lon_br = dialog.get_lat_lon_br();

	return sg_ret::ok;
}



void LayerGeoref::zoom_to_fit_cb(void)
{
	Viewport * viewport = ThisApp::get_main_viewport();
	viewport->set_viking_scale_x(this->mpp_easting);
	viewport->set_viking_scale_y(this->mpp_northing);
	viewport->request_redraw("Redrawing items after setting new zoom level in viewport");
}




void LayerGeoref::goto_center_cb(void)
{
	Viewport * viewport = ThisApp::get_main_viewport();
	UTM utm = viewport->get_center().get_utm();

	const double center_to_left_m = this->image_width * this->mpp_easting / 2;  /* Only an approximation. */
	const double center_to_bottom_m = this->image_height * this->mpp_northing / 2;

	utm.easting = this->utm_tl.get_easting() + center_to_left_m;
	utm.northing = this->utm_tl.get_northing() - center_to_bottom_m;

	viewport->set_center_from_utm(utm);
	viewport->request_redraw("Redrawing items after setting new center coord in viewport");
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




LayerToolGeorefMove::LayerToolGeorefMove(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::Georef)
{
	this->id_string = "sg.tool.layer_georef.move";

	this->action_icon_path   = ":/icons/layer_tool/georef_move_18.png";
	this->action_label       = QObject::tr("&Georef Move Map");
	this->action_tooltip     = QObject::tr("Georef Move Map");
	// this->action_accelerator = ...; /* Empty accelerator. */

	this->cursor_click = QCursor(Qt::ClosedHandCursor);
	this->cursor_release = QCursor(Qt::OpenHandCursor);
}




ToolStatus LayerToolGeorefMove::internal_handle_mouse_release(Layer * layer, QMouseEvent * ev)
{
	return ((LayerGeoref *) layer)->move_release(ev, this);
}




ToolStatus LayerGeoref::move_release(QMouseEvent * ev, LayerTool * tool)
{
	if (this->type != LayerType::Georef) {
		qDebug() << SG_PREFIX_E << "Unexpected layer type" << this->type;
		return ToolStatus::Ignored;
	}

	if (this->click_x != -1) {
		this->utm_tl.easting += (ev->x() - this->click_x) * tool->viewport->get_viking_scale().get_x();
		this->utm_tl.northing -= (ev->y() - this->click_y) * tool->viewport->get_viking_scale().get_y();
		this->emit_tree_item_changed("Georef - move released");
		return ToolStatus::Ack;
	}
	return ToolStatus::Ignored; /* I didn't move anything on this layer! */
}




LayerToolGeorefZoom::LayerToolGeorefZoom(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::Georef)
{
	this->id_string = "sg.tool.layer_georef.zoom";

	this->action_icon_path   = ":/icons/layer_tool/georef_zoom_18.png";
	this->action_label       = QObject::tr("Georef Z&oom Tool");
	this->action_tooltip     = QObject::tr("Georef Zoom Tool");
	// this->action_accelerator = ...; /* Empty accelerator. */
}




ToolStatus LayerToolGeorefZoom::internal_handle_mouse_click(Layer * layer, QMouseEvent * ev)
{
	return ((LayerGeoref *) layer)->zoom_press(ev, this);
}




ToolStatus LayerGeoref::zoom_press(QMouseEvent * ev, LayerTool * tool)
{
	if (this->type != LayerType::Georef) {
		qDebug() << SG_PREFIX_E << "Unexpected layer type" << this->type;
		return ToolStatus::Ignored;
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
	tool->viewport->set_viking_scale_x(this->mpp_easting);
	tool->viewport->set_viking_scale_y(this->mpp_northing);
	this->emit_tree_item_changed("Georef - zoom press");
	return ToolStatus::Ack;
}




ToolStatus LayerToolGeorefMove::internal_handle_mouse_click(Layer * layer, QMouseEvent * ev)
{
	return ((LayerGeoref *) layer)->move_press(ev, this);
}




ToolStatus LayerGeoref::move_press(QMouseEvent * ev, LayerTool * tool)
{
	if (this->type != LayerType::Georef) {
		qDebug() << SG_PREFIX_E << "Unexpected layer type" << this->type;
		return ToolStatus::Ignored;
	}
	this->click_x = ev->x();
	this->click_y = ev->y();
	return ToolStatus::Ack;
}




LayerGeoref * SlavGPS::georef_layer_create(Viewport * viewport, const QString & name, QPixmap * pixmap, const Coord & coord_tl, const Coord & coord_br)
{
	if (!pixmap || pixmap->isNull()) {
		/* Bad image */
		qDebug() << SG_PREFIX_E << "Trying to create layer with invalid image";
		return NULL;
	}

	if (pixmap->width() <= 0 || pixmap->width() <= 0) {
		/* Bad image */
		qDebug() << SG_PREFIX_E << "Trying to create layer with invalid image";
		return NULL;
	}

	LayerGeoref * layer = new LayerGeoref();
	layer->configure_from_viewport(viewport);
	layer->set_name(name);
	layer->image = *pixmap;
	layer->utm_tl = coord_tl.get_utm();
	layer->lat_lon_br = coord_br.get_lat_lon();
	layer->image_width = layer->image.width();
	layer->image_height = layer->image.height();

	const LatLon lat_lon_tl = coord_tl.get_lat_lon();
	const LatLon lat_lon_br = coord_br.get_lat_lon();
	const CoordMode coord_mode = viewport->get_coord_mode();

	double xmpp, ympp;
	georef_layer_mpp_from_coords(coord_mode, lat_lon_tl, lat_lon_br, layer->image_width, layer->image_height, &xmpp, &ympp);
	layer->mpp_easting = xmpp;
	layer->mpp_northing = ympp;

	const LatLonBBox bbox(lat_lon_tl, lat_lon_br);
	viewport->set_center_from_lat_lon(bbox.get_center_lat_lon()); /* TODO: is this call necessary if we call ::set_bbox() below? */

	/* Set best zoom level. */
	viewport->set_bbox(bbox);

	return layer;
}




LayerGeoref::LayerGeoref()
{
	this->type = LayerType::Georef;
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
	this->mpp_northing = viewport->get_viking_scale().get_y();
	this->mpp_easting = viewport->get_viking_scale().get_x();
	this->utm_tl = viewport->get_center().get_utm();
}
